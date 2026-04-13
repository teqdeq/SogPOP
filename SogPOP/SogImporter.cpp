#include "SogImporter.h"

#include <algorithm>
#include <array>
#include <assert.h>
#include <cmath>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string.h>
#include <unordered_map>

#include "third_party/miniz_repo/miniz.h"
#include "third_party/nlohmann/json.hpp"
#include "third_party/libwebp/src/webp/decode.h"

using json = nlohmann::json;

namespace
{
constexpr const char* kFileParName = "File";
constexpr const char* kReloadParName = "Reload";
constexpr const char* kPageName = "SOG";
constexpr float kSh0Constant = 0.28209479177387814f;
constexpr uint32_t kHigherOrderShTripletCount = 8;
const std::array<const char*, 8> kHigherOrderShNames = { "sh1", "sh2", "sh3", "sh4", "sh5", "sh6", "sh7", "sh8" };

const std::array<std::string, 6> kMeanAliases = { "means", "mean", "positions", "position", "pos", "p" };
const std::array<std::string, 4> kScaleAliases = { "scales", "scale", "sizes", "size" };
const std::array<std::string, 6> kRotationAliases = { "rotations", "rotation", "quaternions", "quaternion", "quats", "quat" };
const std::array<std::string, 5> kColorAliases = { "colors", "color", "rgb", "diffuse", "albedo" };
const std::array<std::string, 4> kOpacityAliases = { "opacities", "opacity", "alphas", "alpha" };

struct DecodedImage
{
	std::string entryName;
	int width = 0;
	int height = 0;
	std::vector<uint8_t> rgba;

	[[nodiscard]] bool empty() const
	{
		return width <= 0 || height <= 0 || rgba.empty();
	}
};

struct ArchiveData
{
	json meta;
	std::unordered_map<std::string, DecodedImage> imagesByStem;
	std::vector<std::string> warnings;
};

struct QuantRange
{
	std::vector<float> mins;
	std::vector<float> maxs;

	[[nodiscard]] float decode(uint8_t sample, size_t component) const
	{
		const size_t index = std::min(component, mins.size() - 1);
		const float t = static_cast<float>(sample) / 255.0f;
		return mins[index] + t * (maxs[index] - mins[index]);
	}
};

template <typename T>
OP_SmartRef<POP_Buffer>
copyBuffer(POP_Context* context, const T* data, uint32_t count, POP_BufferUsage usage = POP_BufferUsage::Attribute)
{
	POP_BufferInfo createInfo;
	createInfo.size = static_cast<uint64_t>(sizeof(T)) * count;
	createInfo.usage = usage;
	createInfo.location = POP_BufferLocation::CPU;
	createInfo.mode = POP_BufferMode::SequentialWrite;

	OP_SmartRef<POP_Buffer> buffer = context->createBuffer(createInfo, nullptr);
	if (!buffer)
		return buffer;

	if (count > 0)
		memcpy(buffer->getData(nullptr), data, sizeof(T) * count);

	return buffer;
}

std::string
normalizeKey(std::string_view text)
{
	std::string result;
	result.reserve(text.size());
	for (unsigned char ch : text)
	{
		if (std::isalnum(ch) != 0)
			result.push_back(static_cast<char>(std::tolower(ch)));
	}
	return result;
}

std::string
toLower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
	{
		return static_cast<char>(std::tolower(ch));
	});
	return value;
}

std::string
normalizedStem(const std::string& archiveName)
{
	return normalizeKey(std::filesystem::path(archiveName).stem().string());
}

void
setString(OP_String* value, const std::string& text)
{
	value->setString(text.c_str());
}

std::string
joinMessages(const std::vector<std::string>& messages)
{
	if (messages.empty())
		return std::string();

	std::ostringstream stream;
	for (size_t index = 0; index < messages.size(); ++index)
	{
		if (index > 0)
			stream << "; ";
		stream << messages[index];
	}
	return stream.str();
}

const json*
findObjectValueNormalized(const json& object, const std::string& target)
{
	if (!object.is_object())
		return nullptr;

	for (auto it = object.begin(); it != object.end(); ++it)
	{
		if (normalizeKey(it.key()) == target)
			return &it.value();
	}
	return nullptr;
}

bool
parseFloatArray(const json& node, size_t minComponents, std::vector<float>& values)
{
	if (!node.is_array() || node.size() < minComponents)
		return false;

	values.clear();
	values.reserve(node.size());
	for (const json& item : node)
	{
		if (!item.is_number())
			return false;
		values.push_back(item.get<float>());
	}
	return true;
}

std::optional<QuantRange>
tryParseRangeObject(const json& node, size_t minComponents)
{
	if (!node.is_object())
		return std::nullopt;

	static const std::array<std::string, 4> minKeys = { "mins", "min", "offset", "offsets" };
	static const std::array<std::string, 4> maxKeys = { "maxs", "max", "scale", "scales" };

	const json* minsNode = nullptr;
	const json* maxsNode = nullptr;
	for (const std::string& key : minKeys)
	{
		minsNode = findObjectValueNormalized(node, normalizeKey(key));
		if (minsNode)
			break;
	}
	for (const std::string& key : maxKeys)
	{
		maxsNode = findObjectValueNormalized(node, normalizeKey(key));
		if (maxsNode)
			break;
	}

	if (!minsNode || !maxsNode)
		return std::nullopt;

	QuantRange range;
	if (!parseFloatArray(*minsNode, minComponents, range.mins) || !parseFloatArray(*maxsNode, minComponents, range.maxs))
		return std::nullopt;
	return range;
}

std::optional<QuantRange>
findQuantRange(const json& meta, const std::vector<std::string>& aliases, size_t minComponents)
{
	std::vector<const json*> containers = { &meta };
	if (const json* quant = findObjectValueNormalized(meta, normalizeKey("quantization")))
		containers.push_back(quant);
	if (const json* quant = findObjectValueNormalized(meta, normalizeKey("dequantization")))
		containers.push_back(quant);

	for (const json* container : containers)
	{
		if (!container || !container->is_object())
			continue;

		for (const std::string& alias : aliases)
		{
			const std::string normalizedAlias = normalizeKey(alias);
			if (const json* node = findObjectValueNormalized(*container, normalizedAlias))
			{
				if (std::optional<QuantRange> range = tryParseRangeObject(*node, minComponents))
					return range;
			}

			const json* minsNode = findObjectValueNormalized(*container, normalizedAlias + "mins");
			const json* maxsNode = findObjectValueNormalized(*container, normalizedAlias + "maxs");
			if (!minsNode)
				minsNode = findObjectValueNormalized(*container, normalizedAlias + "offsets");
			if (!maxsNode)
				maxsNode = findObjectValueNormalized(*container, normalizedAlias + "scales");

			if (minsNode && maxsNode)
			{
				QuantRange range;
				if (parseFloatArray(*minsNode, minComponents, range.mins) && parseFloatArray(*maxsNode, minComponents, range.maxs))
					return range;
			}
		}
	}

	return std::nullopt;
}

const DecodedImage*
findImage(const ArchiveData& archive, const std::vector<std::string>& aliases)
{
	for (const auto& [stem, image] : archive.imagesByStem)
	{
		for (const std::string& alias : aliases)
		{
			const std::string needle = normalizeKey(alias);
			if (stem == needle || stem.find(needle) != std::string::npos)
				return &image;
		}
	}
	return nullptr;
}

bool
decodeWebP(const std::vector<uint8_t>& encoded, const std::string& entryName, DecodedImage& decoded, std::string& error)
{
	int width = 0;
	int height = 0;
	if (!WebPGetInfo(encoded.data(), encoded.size(), &width, &height) || width <= 0 || height <= 0)
	{
		error = "Failed to read WebP header: " + entryName;
		return false;
	}

	decoded.entryName = entryName;
	decoded.width = width;
	decoded.height = height;
	decoded.rgba.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4U);
	if (WebPDecodeRGBAInto(encoded.data(), encoded.size(), decoded.rgba.data(), decoded.rgba.size(), width * 4) == nullptr)
	{
		error = "Failed to decode WebP payload: " + entryName;
		return false;
	}

	return true;
}

bool
extractZipEntry(mz_zip_archive& zip, mz_uint fileIndex, std::vector<uint8_t>& bytes, std::string& error)
{
	size_t extractedSize = 0;
	void* extracted = mz_zip_reader_extract_to_heap(&zip, fileIndex, &extractedSize, 0);
	if (!extracted)
	{
		error = "Failed to extract ZIP entry from .sog archive.";
		return false;
	}

	bytes.assign(static_cast<uint8_t*>(extracted), static_cast<uint8_t*>(extracted) + extractedSize);
	mz_free(extracted);
	return true;
}

bool
loadArchive(const std::string& filePath, ArchiveData& archive, std::string& error)
{
	const std::filesystem::path archivePath = std::filesystem::u8path(filePath);
	std::ifstream input(archivePath, std::ios::binary | std::ios::ate);
	if (!input)
	{
		error = "Failed to open .sog file for reading.";
		return false;
	}

	const std::streamsize byteCount = input.tellg();
	if (byteCount <= 0)
	{
		error = "The .sog file is empty or unreadable.";
		return false;
	}

	input.seekg(0, std::ios::beg);
	std::vector<uint8_t> zipBytes(static_cast<size_t>(byteCount));
	if (!input.read(reinterpret_cast<char*>(zipBytes.data()), byteCount))
	{
		error = "Failed to read the .sog file into memory.";
		return false;
	}

	mz_zip_archive zip = {};
	if (!mz_zip_reader_init_mem(&zip, zipBytes.data(), zipBytes.size(), 0))
	{
		error = std::string("Failed to open .sog archive as ZIP: ") + mz_zip_get_error_string(zip.m_last_error);
		return false;
	}

	bool ok = true;
	bool metaFound = false;
	const mz_uint fileCount = mz_zip_reader_get_num_files(&zip);
	for (mz_uint fileIndex = 0; fileIndex < fileCount && ok; ++fileIndex)
	{
		mz_zip_archive_file_stat stat = {};
		if (!mz_zip_reader_file_stat(&zip, fileIndex, &stat))
			continue;
		if (stat.m_is_directory)
			continue;

		const std::string archiveName = stat.m_filename;
		const std::string lowerName = toLower(archiveName);

		if (std::filesystem::path(lowerName).filename() == "meta.json")
		{
			std::vector<uint8_t> bytes;
			ok = extractZipEntry(zip, fileIndex, bytes, error);
			if (!ok)
				break;

			try
			{
				archive.meta = json::parse(bytes.begin(), bytes.end());
				metaFound = true;
			}
			catch (const std::exception& exception)
			{
				error = std::string("Failed to parse meta.json: ") + exception.what();
				ok = false;
			}
		}
		else if (std::filesystem::path(lowerName).extension() == ".webp")
		{
			std::vector<uint8_t> bytes;
			ok = extractZipEntry(zip, fileIndex, bytes, error);
			if (!ok)
				break;

			DecodedImage image;
			ok = decodeWebP(bytes, archiveName, image, error);
			if (!ok)
				break;

			archive.imagesByStem[normalizedStem(archiveName)] = std::move(image);
		}
	}

	mz_zip_reader_end(&zip);

	if (!ok)
		return false;
	if (!metaFound)
	{
		error = "The .sog archive does not contain meta.json.";
		return false;
	}
	if (archive.imagesByStem.empty())
	{
		error = "The .sog archive does not contain any .webp attribute textures.";
		return false;
	}

	return true;
}

bool
ensureMatchingDimensions(const DecodedImage& reference, const DecodedImage* candidate, const char* label, std::string& error)
{
	if (!candidate)
		return true;
	if (candidate->width == reference.width && candidate->height == reference.height)
		return true;

	error = std::string("Texture dimension mismatch for ") + label + ".";
	return false;
}

float
normalizedByte(uint8_t value)
{
	return static_cast<float>(value) / 255.0f;
}

void
normalizeQuaternion(float* quat)
{
	const float lengthSq = quat[0] * quat[0] + quat[1] * quat[1] + quat[2] * quat[2] + quat[3] * quat[3];
	if (lengthSq <= 1.0e-12f)
	{
		quat[0] = 0.0f;
		quat[1] = 0.0f;
		quat[2] = 0.0f;
		quat[3] = 1.0f;
		return;
	}

	const float invLength = 1.0f / std::sqrt(lengthSq);
	quat[0] *= invLength;
	quat[1] *= invLength;
	quat[2] *= invLength;
	quat[3] *= invLength;
}

bool
tryGetFileList(const json& node, std::vector<std::string>& files)
{
	const json* filesNode = findObjectValueNormalized(node, normalizeKey("files"));
	if (!filesNode || !filesNode->is_array())
		return false;

	files.clear();
	files.reserve(filesNode->size());
	for (const json& item : *filesNode)
	{
		if (!item.is_string())
			return false;
		files.push_back(item.get<std::string>());
	}
	return !files.empty();
}

const DecodedImage*
findImageByFileName(const ArchiveData& archive, const std::string& fileName)
{
	const auto found = archive.imagesByStem.find(normalizedStem(fileName));
	if (found == archive.imagesByStem.end())
		return nullptr;
	return &found->second;
}

bool
tryGetImagesFromFiles(const ArchiveData& archive, const json& node, std::vector<const DecodedImage*>& images)
{
	std::vector<std::string> files;
	if (!tryGetFileList(node, files))
		return false;

	images.clear();
	images.reserve(files.size());
	for (const std::string& fileName : files)
	{
		const DecodedImage* image = findImageByFileName(archive, fileName);
		if (!image)
			return false;
		images.push_back(image);
	}
	return !images.empty();
}

bool
tryGetMetaCount(const json& meta, uint32_t& count)
{
	const json* countNode = findObjectValueNormalized(meta, normalizeKey("count"));
	if (!countNode || !countNode->is_number_unsigned())
		return false;

	count = countNode->get<uint32_t>();
	return count > 0;
}

bool
tryGetCodebook(const json& node, std::vector<float>& codebook)
{
	const json* codebookNode = findObjectValueNormalized(node, normalizeKey("codebook"));
	if (!codebookNode)
		return false;
	return parseFloatArray(*codebookNode, 1, codebook);
}

float
decodeCodebookValue(uint8_t sample, const std::vector<float>& codebook)
{
	if (codebook.empty())
		return normalizedByte(sample);

	const size_t index = std::min<size_t>(sample, codebook.size() - 1);
	return codebook[index];
}

float
decodeUint16Quantized(uint8_t lowByte, uint8_t highByte, const QuantRange& range, size_t component)
{
	const uint16_t packed = static_cast<uint16_t>(lowByte) | (static_cast<uint16_t>(highByte) << 8);
	const size_t index = std::min(component, range.mins.size() - 1);
	const float t = static_cast<float>(packed) / 65535.0f;
	return range.mins[index] + t * (range.maxs[index] - range.mins[index]);
}

float
decodeSignedUnit(uint8_t sample)
{
	return static_cast<float>(sample) / 127.5f - 1.0f;
}

float
sigmoid(float value)
{
	if (value >= 0.0f)
	{
		const float expValue = std::exp(-value);
		return 1.0f / (1.0f + expValue);
	}

	const float expValue = std::exp(value);
	return expValue / (1.0f + expValue);
}

float
decodeSh0Color(float coeff)
{
	return std::clamp(0.5f + kSh0Constant * coeff, 0.0f, 1.0f);
}

uint16_t
decodeUint16LittleEndian(uint8_t lowByte, uint8_t highByte)
{
	return static_cast<uint16_t>(lowByte) | (static_cast<uint16_t>(highByte) << 8);
}
}  // namespace

extern "C"
{
DLLEXPORT
void
FillPOPPluginInfo(POP_PluginInfo* info)
{
	if (!info->setAPIVersion(POPCPlusPlusAPIVersion))
		return;

	info->customOPInfo.opType->setString("Sogimporter");
	info->customOPInfo.opLabel->setString("SOG Importer");
	info->customOPInfo.opIcon->setString("SOG");
	info->customOPInfo.authorName->setString("GitHub Copilot");
	info->customOPInfo.authorEmail->setString("n/a");
	info->customOPInfo.minInputs = 0;
	info->customOPInfo.maxInputs = 0;
	info->customOPInfo.opHelpURL->setString("https://docs.derivative.ca/");
}

DLLEXPORT
POP_CPlusPlusBase*
CreatePOPInstance(const OP_NodeInfo* info, POP_Context* context)
{
	return new SogImporter(info, context);
}

DLLEXPORT
void
DestroyPOPInstance(POP_CPlusPlusBase* instance, POP_Context* context)
{
	(void)context;
	delete static_cast<SogImporter*>(instance);
}
}

void
SogImporter::PointCache::clear()
{
	positions.clear();
	scales.clear();
	quaternions.clear();
	colors.clear();
	alphas.clear();
	normals.clear();
	shCoefficients.clear();
}

bool
SogImporter::PointCache::empty() const
{
	return positions.empty();
}

uint32_t
SogImporter::PointCache::size() const
{
	return static_cast<uint32_t>(positions.size());
}

SogImporter::SogImporter(const OP_NodeInfo* info, POP_Context* context) :
	myNodeInfo(info),
	myContext(context),
	myReloadRequested(false),
	myExecuteCount(0)
{
}

SogImporter::~SogImporter() = default;

void
SogImporter::getGeneralInfo(POP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved)
{
	(void)inputs;
	(void)reserved;
	ginfo->cookEveryFrameIfAsked = false;
	ginfo->cookEveryFrame = false;
}

void
SogImporter::execute(POP_Output* output, const OP_Inputs* inputs, void* reserved)
{
	(void)reserved;
	std::lock_guard<std::mutex> lock(myMutex);
	myExecuteCount++;

	const char* rawFilePath = inputs->getParFilePath(kFileParName);
	const std::string filePath = rawFilePath ? rawFilePath : std::string();

	if (filePath.empty())
	{
		myLoadedFile.clear();
		myCache.clear();
		myWarning.clear();
		myError.clear();
		return;
	}

	if (myReloadRequested || filePath != myLoadedFile || myCache.empty())
	{
		refreshCache(filePath);
		myReloadRequested = false;
	}

	if (myCache.empty())
		return;

	publishCache(output, myCache);
}

int32_t
SogImporter::getNumInfoCHOPChans(void* reserved)
{
	(void)reserved;
	return 2;
}

void
SogImporter::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void* reserved)
{
	(void)reserved;
	if (index == 0)
	{
		chan->name->setString("executeCount");
		chan->value = static_cast<float>(myExecuteCount);
	}
	else if (index == 1)
	{
		chan->name->setString("pointCount");
		chan->value = static_cast<float>(myCache.size());
	}
}

bool
SogImporter::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved)
{
	(void)reserved;
	infoSize->rows = 4;
	infoSize->cols = 2;
	infoSize->byColumn = false;
	return true;
}

void
SogImporter::getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries* entries, void* reserved)
{
	(void)nEntries;
	(void)reserved;

	switch (index)
	{
	case 0:
		setString(entries->values[0], "file");
		setString(entries->values[1], myLoadedFile);
		break;
	case 1:
		setString(entries->values[0], "points");
		setString(entries->values[1], std::to_string(myCache.size()));
		break;
	case 2:
		setString(entries->values[0], "warning");
		setString(entries->values[1], myWarning);
		break;
	case 3:
		setString(entries->values[0], "error");
		setString(entries->values[1], myError);
		break;
	default:
		break;
	}
}

void
SogImporter::getWarningString(OP_String* warning, void* reserved)
{
	(void)reserved;
	warning->setString(myWarning.c_str());
}

void
SogImporter::getErrorString(OP_String* error, void* reserved)
{
	(void)reserved;
	error->setString(myError.c_str());
}

void
SogImporter::setupParameters(OP_ParameterManager* manager, void* reserved)
{
	(void)reserved;

	{
		OP_StringParameter sp;
		sp.name = kFileParName;
		sp.label = "File";
		sp.page = kPageName;
		sp.defaultValue = "";

		OP_ParAppendResult result = manager->appendFile(sp);
		assert(result == OP_ParAppendResult::Success);
	}

	{
		OP_NumericParameter np;
		np.name = kReloadParName;
		np.label = "Reload";
		np.page = kPageName;

		OP_ParAppendResult result = manager->appendPulse(np);
		assert(result == OP_ParAppendResult::Success);
	}
}

void
SogImporter::pulsePressed(const char* name, void* reserved)
{
	(void)reserved;
	if (name && std::string(name) == kReloadParName)
		myReloadRequested = true;
}

bool
SogImporter::refreshCache(const std::string& filePath)
{
	PointCache nextCache;
	std::string nextWarning;
	std::string nextError;

	if (!buildPointCache(filePath, nextCache, nextWarning, nextError))
	{
		myLoadedFile = filePath;
		myCache.clear();
		myWarning = nextWarning;
		myError = nextError;
		return false;
	}

	myLoadedFile = filePath;
	myCache = std::move(nextCache);
	myWarning = nextWarning;
	myError.clear();
	return true;
}

bool
SogImporter::buildPointCache(const std::string& filePath, PointCache& nextCache, std::string& nextWarning, std::string& nextError) const
{
	if (!isSogPath(filePath))
	{
		nextError = "The selected file must use the .sog extension.";
		return false;
	}

	std::error_code ec;
	if (!std::filesystem::exists(filePath, ec) || ec)
	{
		nextError = "The selected .sog file could not be found.";
		return false;
	}

	const auto fileSize = std::filesystem::file_size(std::filesystem::u8path(filePath), ec);
	if (!ec && fileSize == 0)
	{
		nextError = "The selected .sog file is empty (0 bytes).";
		return false;
	}

	ArchiveData archive;
	if (!loadArchive(filePath, archive, nextError))
		return false;

	const json* meansMeta = findObjectValueNormalized(archive.meta, normalizeKey("means"));
	const json* scalesMeta = findObjectValueNormalized(archive.meta, normalizeKey("scales"));
	const json* quatsMeta = findObjectValueNormalized(archive.meta, normalizeKey("quats"));
	const json* sh0Meta = findObjectValueNormalized(archive.meta, normalizeKey("sh0"));
	const json* shNMeta = findObjectValueNormalized(archive.meta, normalizeKey("shn"));

	std::vector<const DecodedImage*> meansImages;
	std::vector<const DecodedImage*> scaleImages;
	std::vector<const DecodedImage*> quatImages;
	std::vector<const DecodedImage*> sh0Images;
	std::vector<const DecodedImage*> shNImages;
	std::vector<float> scaleCodebook;
	std::vector<float> sh0Codebook;
	std::vector<float> shNCodebook;
	uint32_t metaCount = 0;

	const bool hasSupersplatMeans = meansMeta && tryGetImagesFromFiles(archive, *meansMeta, meansImages) && meansImages.size() == 2;
	const bool hasSupersplatScales = scalesMeta && tryGetImagesFromFiles(archive, *scalesMeta, scaleImages) && scaleImages.size() == 1 && tryGetCodebook(*scalesMeta, scaleCodebook);
	const bool hasSupersplatQuats = quatsMeta && tryGetImagesFromFiles(archive, *quatsMeta, quatImages) && quatImages.size() == 1;
	const bool hasSupersplatSh0 = sh0Meta && tryGetImagesFromFiles(archive, *sh0Meta, sh0Images) && sh0Images.size() == 1 && tryGetCodebook(*sh0Meta, sh0Codebook);
	const bool hasSupersplatShN = shNMeta && tryGetImagesFromFiles(archive, *shNMeta, shNImages) && shNImages.size() == 2 && tryGetCodebook(*shNMeta, shNCodebook);
	const bool hasSupersplatSchema = hasSupersplatMeans && hasSupersplatScales && hasSupersplatQuats && hasSupersplatSh0;

	if (hasSupersplatSchema)
	{
		std::optional<QuantRange> meanRange = tryParseRangeObject(*meansMeta, 3);
		if (!meanRange.has_value())
		{
			nextError = "The means entry in meta.json does not contain mins/maxs.";
			return false;
		}

		const DecodedImage* meansLow = meansImages[0];
		const DecodedImage* meansHigh = meansImages[1];
		const DecodedImage* scales = scaleImages[0];
		const DecodedImage* rotations = quatImages[0];
		const DecodedImage* sh0 = sh0Images[0];
		const DecodedImage* shNCentroids = hasSupersplatShN ? shNImages[0] : nullptr;
		const DecodedImage* shNLabels = hasSupersplatShN ? shNImages[1] : nullptr;

		if (!ensureMatchingDimensions(*meansLow, meansHigh, "means high", nextError) ||
			!ensureMatchingDimensions(*meansLow, scales, "scale", nextError) ||
			!ensureMatchingDimensions(*meansLow, rotations, "rotation", nextError) ||
			!ensureMatchingDimensions(*meansLow, sh0, "sh0", nextError) ||
			!ensureMatchingDimensions(*meansLow, shNLabels, "shN labels", nextError))
		{
			return false;
		}

		const uint32_t availablePoints = static_cast<uint32_t>(meansLow->width * meansLow->height);
		const bool hasCount = tryGetMetaCount(archive.meta, metaCount);
		const uint32_t pointCount = hasCount ? std::min(metaCount, availablePoints) : availablePoints;
		if (hasCount && metaCount > availablePoints)
		{
			archive.warnings.push_back("meta.count exceeds available decoded texels.");
		}

		nextCache.positions.resize(pointCount);
		nextCache.scales.resize(pointCount, Vector(0.01f, 0.01f, 0.01f));
		nextCache.quaternions.resize(static_cast<size_t>(pointCount) * 4U, 0.0f);
		nextCache.colors.resize(static_cast<size_t>(pointCount) * 3U, 1.0f);
		nextCache.alphas.resize(pointCount, 1.0f);
		nextCache.normals.resize(pointCount, Vector(0.0f, 0.0f, 1.0f));
		nextCache.shCoefficients.clear();

		std::vector<float> shNCentroidValues;
		if (hasSupersplatShN)
		{
			const size_t centroidPixelCount = static_cast<size_t>(shNCentroids->width) * static_cast<size_t>(shNCentroids->height);
			if ((centroidPixelCount % kHigherOrderShTripletCount) != 0)
			{
				archive.warnings.push_back("shN centroid texture has an unexpected layout; skipping higher-order SH decode.");
				shNCentroids = nullptr;
				shNLabels = nullptr;
			}
			else
			{
				const size_t centroidCount = centroidPixelCount / kHigherOrderShTripletCount;
				shNCentroidValues.resize(centroidCount * kHigherOrderShTripletCount * 3U, 0.0f);
				for (size_t centroidIndex = 0; centroidIndex < centroidCount; ++centroidIndex)
				{
					for (uint32_t tripletIndex = 0; tripletIndex < kHigherOrderShTripletCount; ++tripletIndex)
					{
						const size_t pixelIndex = (centroidIndex * kHigherOrderShTripletCount + tripletIndex) * 4U;
						const size_t coeffIndex = (centroidIndex * kHigherOrderShTripletCount + tripletIndex) * 3U;
						shNCentroidValues[coeffIndex + 0] = decodeCodebookValue(shNCentroids->rgba[pixelIndex + 0], shNCodebook);
						shNCentroidValues[coeffIndex + 1] = decodeCodebookValue(shNCentroids->rgba[pixelIndex + 1], shNCodebook);
						shNCentroidValues[coeffIndex + 2] = decodeCodebookValue(shNCentroids->rgba[pixelIndex + 2], shNCodebook);
					}
				}
				nextCache.shCoefficients.resize(static_cast<size_t>(pointCount) * kHigherOrderShTripletCount * 3U, 0.0f);
			}
		}

		for (uint32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex)
		{
			const size_t rgbaIndex = static_cast<size_t>(pointIndex) * 4U;

			nextCache.positions[pointIndex] = Position(
				decodeUint16Quantized(meansLow->rgba[rgbaIndex + 0], meansHigh->rgba[rgbaIndex + 0], *meanRange, 0),
				decodeUint16Quantized(meansLow->rgba[rgbaIndex + 1], meansHigh->rgba[rgbaIndex + 1], *meanRange, 1),
				decodeUint16Quantized(meansLow->rgba[rgbaIndex + 2], meansHigh->rgba[rgbaIndex + 2], *meanRange, 2));

			nextCache.scales[pointIndex] = Vector(
				std::exp(decodeCodebookValue(scales->rgba[rgbaIndex + 0], scaleCodebook)),
				std::exp(decodeCodebookValue(scales->rgba[rgbaIndex + 1], scaleCodebook)),
				std::exp(decodeCodebookValue(scales->rgba[rgbaIndex + 2], scaleCodebook)));

			float* quaternion = &nextCache.quaternions[rgbaIndex];
			quaternion[0] = decodeSignedUnit(rotations->rgba[rgbaIndex + 0]);
			quaternion[1] = decodeSignedUnit(rotations->rgba[rgbaIndex + 1]);
			quaternion[2] = decodeSignedUnit(rotations->rgba[rgbaIndex + 2]);
			quaternion[3] = decodeSignedUnit(rotations->rgba[rgbaIndex + 3]);
			normalizeQuaternion(quaternion);

			float* color = &nextCache.colors[static_cast<size_t>(pointIndex) * 3U];
			color[0] = decodeSh0Color(decodeCodebookValue(sh0->rgba[rgbaIndex + 0], sh0Codebook));
			color[1] = decodeSh0Color(decodeCodebookValue(sh0->rgba[rgbaIndex + 1], sh0Codebook));
			color[2] = decodeSh0Color(decodeCodebookValue(sh0->rgba[rgbaIndex + 2], sh0Codebook));
			nextCache.alphas[pointIndex] = sigmoid(decodeCodebookValue(sh0->rgba[rgbaIndex + 3], sh0Codebook));

			if (!nextCache.shCoefficients.empty() && shNLabels)
			{
				const uint16_t centroidIndex = decodeUint16LittleEndian(shNLabels->rgba[rgbaIndex + 0], shNLabels->rgba[rgbaIndex + 1]);
				const size_t centroidBase = static_cast<size_t>(centroidIndex) * kHigherOrderShTripletCount * 3U;
				const size_t pointBase = static_cast<size_t>(pointIndex) * kHigherOrderShTripletCount * 3U;
				if (centroidBase + kHigherOrderShTripletCount * 3U <= shNCentroidValues.size())
				{
					std::copy_n(shNCentroidValues.data() + centroidBase, kHigherOrderShTripletCount * 3U, nextCache.shCoefficients.data() + pointBase);
				}
			}

			nextCache.normals[pointIndex] = rotateUnitZByQuaternion(quaternion);
		}

		nextWarning = joinMessages(archive.warnings);
		return true;
	}

	const DecodedImage* means = findImage(archive, std::vector<std::string>(kMeanAliases.begin(), kMeanAliases.end()));
	if (!means)
	{
		nextError = "No means texture was found in the .sog archive.";
		return false;
	}

	const DecodedImage* scales = findImage(archive, std::vector<std::string>(kScaleAliases.begin(), kScaleAliases.end()));
	const DecodedImage* rotations = findImage(archive, std::vector<std::string>(kRotationAliases.begin(), kRotationAliases.end()));
	const DecodedImage* colors = findImage(archive, std::vector<std::string>(kColorAliases.begin(), kColorAliases.end()));
	const DecodedImage* opacities = findImage(archive, std::vector<std::string>(kOpacityAliases.begin(), kOpacityAliases.end()));

	if (!ensureMatchingDimensions(*means, scales, "scale", nextError) ||
		!ensureMatchingDimensions(*means, rotations, "rotation", nextError) ||
		!ensureMatchingDimensions(*means, colors, "color", nextError) ||
		!ensureMatchingDimensions(*means, opacities, "opacity", nextError))
	{
		return false;
	}

	const std::optional<QuantRange> meanRange = findQuantRange(archive.meta, std::vector<std::string>(kMeanAliases.begin(), kMeanAliases.end()), 3);
	if (!meanRange.has_value())
	{
		nextError = "No quantization range for means was found in meta.json.";
		return false;
	}

	const std::optional<QuantRange> scaleRange = findQuantRange(archive.meta, std::vector<std::string>(kScaleAliases.begin(), kScaleAliases.end()), 3);
	const std::optional<QuantRange> rotationRange = findQuantRange(archive.meta, std::vector<std::string>(kRotationAliases.begin(), kRotationAliases.end()), 4);
	const std::optional<QuantRange> colorRange = findQuantRange(archive.meta, std::vector<std::string>(kColorAliases.begin(), kColorAliases.end()), 3);
	const std::optional<QuantRange> opacityRange = findQuantRange(archive.meta, std::vector<std::string>(kOpacityAliases.begin(), kOpacityAliases.end()), 1);

	if (scales && !scaleRange.has_value())
		archive.warnings.push_back("Scale texture found without quantization range. Falling back to default scale.");
	if (rotations && !rotationRange.has_value())
		archive.warnings.push_back("Rotation texture found without quantization range. Falling back to identity quaternion.");
	if (colors && !colorRange.has_value())
		archive.warnings.push_back("Color texture found without quantization range. Using normalized byte colors.");
	if (opacities && !opacityRange.has_value())
		archive.warnings.push_back("Opacity texture found without quantization range. Using normalized byte alpha.");

	const uint32_t pointCount = static_cast<uint32_t>(means->width * means->height);
	nextCache.positions.resize(pointCount);
	nextCache.scales.resize(pointCount, Vector(0.01f, 0.01f, 0.01f));
	nextCache.quaternions.resize(static_cast<size_t>(pointCount) * 4U, 0.0f);
	nextCache.colors.resize(static_cast<size_t>(pointCount) * 3U, 1.0f);
	nextCache.alphas.resize(pointCount, 1.0f);
	nextCache.normals.resize(pointCount, Vector(0.0f, 0.0f, 1.0f));

	for (uint32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex)
	{
		const size_t rgbaIndex = static_cast<size_t>(pointIndex) * 4U;

		nextCache.positions[pointIndex] = Position(
			meanRange->decode(means->rgba[rgbaIndex + 0], 0),
			meanRange->decode(means->rgba[rgbaIndex + 1], 1),
			meanRange->decode(means->rgba[rgbaIndex + 2], 2));

		if (scales && scaleRange.has_value())
		{
			nextCache.scales[pointIndex] = Vector(
				scaleRange->decode(scales->rgba[rgbaIndex + 0], 0),
				scaleRange->decode(scales->rgba[rgbaIndex + 1], 1),
				scaleRange->decode(scales->rgba[rgbaIndex + 2], 2));
		}

		float* quaternion = &nextCache.quaternions[rgbaIndex];
		quaternion[0] = 0.0f;
		quaternion[1] = 0.0f;
		quaternion[2] = 0.0f;
		quaternion[3] = 1.0f;
		if (rotations && rotationRange.has_value())
		{
			quaternion[0] = rotationRange->decode(rotations->rgba[rgbaIndex + 0], 0);
			quaternion[1] = rotationRange->decode(rotations->rgba[rgbaIndex + 1], 1);
			quaternion[2] = rotationRange->decode(rotations->rgba[rgbaIndex + 2], 2);
			quaternion[3] = rotationRange->decode(rotations->rgba[rgbaIndex + 3], 3);
			normalizeQuaternion(quaternion);
		}

		float* color = &nextCache.colors[static_cast<size_t>(pointIndex) * 3U];
		if (colors)
		{
			if (colorRange.has_value())
			{
				color[0] = colorRange->decode(colors->rgba[rgbaIndex + 0], 0);
				color[1] = colorRange->decode(colors->rgba[rgbaIndex + 1], 1);
				color[2] = colorRange->decode(colors->rgba[rgbaIndex + 2], 2);
			}
			else
			{
				color[0] = normalizedByte(colors->rgba[rgbaIndex + 0]);
				color[1] = normalizedByte(colors->rgba[rgbaIndex + 1]);
				color[2] = normalizedByte(colors->rgba[rgbaIndex + 2]);
			}
		}

		if (opacities)
		{
			nextCache.alphas[pointIndex] = opacityRange.has_value() ?
				opacityRange->decode(opacities->rgba[rgbaIndex + 0], 0) :
				normalizedByte(opacities->rgba[rgbaIndex + 0]);
		}
		else if (colors)
		{
			nextCache.alphas[pointIndex] = normalizedByte(colors->rgba[rgbaIndex + 3]);
		}

		nextCache.normals[pointIndex] = rotateUnitZByQuaternion(quaternion);
	}

	nextWarning = joinMessages(archive.warnings);
	return true;
}

void
SogImporter::publishCache(POP_Output* output, const PointCache& cache)
{
	POP_SetBufferInfo sinfo;
	std::vector<Color> colorRgba(cache.size());
	for (uint32_t pointIndex = 0; pointIndex < cache.size(); ++pointIndex)
	{
		const size_t colorIndex = static_cast<size_t>(pointIndex) * 3U;
		colorRgba[pointIndex] = Color(
			cache.colors[colorIndex + 0],
			cache.colors[colorIndex + 1],
			cache.colors[colorIndex + 2],
			cache.alphas[pointIndex]);
	}

	OP_SmartRef<POP_Buffer> positionBuffer = copyBuffer(myContext, cache.positions.data(), cache.size());
	OP_SmartRef<POP_Buffer> scaleBuffer = copyBuffer(myContext, cache.scales.data(), cache.size());
	OP_SmartRef<POP_Buffer> quatBuffer = copyBuffer(myContext, cache.quaternions.data(), cache.size() * 4U);
	OP_SmartRef<POP_Buffer> displayColorBuffer = copyBuffer(myContext, colorRgba.data(), cache.size());
	OP_SmartRef<POP_Buffer> colorBuffer = copyBuffer(myContext, cache.colors.data(), cache.size() * 3U);
	OP_SmartRef<POP_Buffer> alphaBuffer = copyBuffer(myContext, cache.alphas.data(), cache.size());
	OP_SmartRef<POP_Buffer> normalBuffer = copyBuffer(myContext, cache.normals.data(), cache.size());
	OP_SmartRef<POP_Buffer> indexBuffer = createPointIndexBuffer(cache.size());

	POP_AttributeInfo posInfo;
	posInfo.name = "P";
	posInfo.numComponents = 3;
	posInfo.type = POP_AttributeType::Float;
	posInfo.attribClass = POP_AttributeClass::Point;
	output->setAttribute(&positionBuffer, posInfo, sinfo, nullptr);

	POP_AttributeInfo scaleInfo;
	scaleInfo.name = "scale";
	scaleInfo.numComponents = 3;
	scaleInfo.type = POP_AttributeType::Float;
	scaleInfo.attribClass = POP_AttributeClass::Point;
	output->setAttribute(&scaleBuffer, scaleInfo, sinfo, nullptr);

	POP_AttributeInfo quatInfo;
	quatInfo.name = "quat";
	quatInfo.numComponents = 4;
	quatInfo.type = POP_AttributeType::Float;
	quatInfo.attribClass = POP_AttributeClass::Point;
	output->setAttribute(&quatBuffer, quatInfo, sinfo, nullptr);

	POP_AttributeInfo colorInfo;
	colorInfo.name = "CD";
	colorInfo.numComponents = 3;
	colorInfo.type = POP_AttributeType::Float;
	colorInfo.attribClass = POP_AttributeClass::Point;
	output->setAttribute(&colorBuffer, colorInfo, sinfo, nullptr);

	POP_AttributeInfo displayColorInfo;
	displayColorInfo.name = "Color";
	displayColorInfo.numComponents = 4;
	displayColorInfo.type = POP_AttributeType::Float;
	displayColorInfo.attribClass = POP_AttributeClass::Point;
	output->setAttribute(&displayColorBuffer, displayColorInfo, sinfo, nullptr);

	POP_AttributeInfo alphaInfo;
	alphaInfo.name = "alpha";
	alphaInfo.numComponents = 1;
	alphaInfo.type = POP_AttributeType::Float;
	alphaInfo.attribClass = POP_AttributeClass::Point;
	output->setAttribute(&alphaBuffer, alphaInfo, sinfo, nullptr);

	POP_AttributeInfo normalInfo;
	normalInfo.name = "N";
	normalInfo.numComponents = 3;
	normalInfo.type = POP_AttributeType::Float;
	normalInfo.qualifier = POP_AttributeQualifier::Direction;
	normalInfo.attribClass = POP_AttributeClass::Point;
	output->setAttribute(&normalBuffer, normalInfo, sinfo, nullptr);

	if (!cache.shCoefficients.empty())
	{
		for (uint32_t tripletIndex = 0; tripletIndex < kHigherOrderShTripletCount; ++tripletIndex)
		{
			std::vector<float> shAttribute(cache.size() * 3U, 0.0f);
			for (uint32_t pointIndex = 0; pointIndex < cache.size(); ++pointIndex)
			{
				const size_t sourceIndex = (static_cast<size_t>(pointIndex) * kHigherOrderShTripletCount + tripletIndex) * 3U;
				const size_t destIndex = static_cast<size_t>(pointIndex) * 3U;
				shAttribute[destIndex + 0] = cache.shCoefficients[sourceIndex + 0];
				shAttribute[destIndex + 1] = cache.shCoefficients[sourceIndex + 1];
				shAttribute[destIndex + 2] = cache.shCoefficients[sourceIndex + 2];
			}

			OP_SmartRef<POP_Buffer> shBuffer = copyBuffer(myContext, shAttribute.data(), cache.size() * 3U);
			POP_AttributeInfo shInfo;
			shInfo.name = kHigherOrderShNames[tripletIndex];
			shInfo.numComponents = 3;
			shInfo.type = POP_AttributeType::Float;
			shInfo.attribClass = POP_AttributeClass::Point;
			output->setAttribute(&shBuffer, shInfo, sinfo, nullptr);
		}
	}

	POP_IndexBufferInfo indexInfo;
	indexInfo.type = POP_IndexType::UInt32;
	output->setIndexBuffer(&indexBuffer, indexInfo, sinfo, nullptr);

	POP_InfoBuffers infoBuffers;
	infoBuffers.pointInfo = createPointInfoBuffer(cache.size());
	infoBuffers.topoInfo = createTopologyInfoBuffer(cache.size());
	output->setInfoBuffers(&infoBuffers, sinfo, nullptr);
}

OP_SmartRef<POP_Buffer>
SogImporter::createRawBuffer(uint64_t size, POP_BufferUsage usage) const
{
	POP_BufferInfo createInfo;
	createInfo.size = size;
	createInfo.usage = usage;
	createInfo.location = POP_BufferLocation::CPU;
	createInfo.mode = POP_BufferMode::SequentialWrite;
	return myContext->createBuffer(createInfo, nullptr);
}

OP_SmartRef<POP_Buffer>
SogImporter::createPointInfoBuffer(uint32_t numPoints) const
{
	OP_SmartRef<POP_Buffer> buffer = createRawBuffer(sizeof(POP_PointInfo), POP_BufferUsage::PointInfoBuffer);
	if (!buffer)
		return buffer;

	POP_PointInfo* pointInfo = static_cast<POP_PointInfo*>(buffer->getData(nullptr));
	memset(pointInfo, 0, sizeof(POP_PointInfo));
	pointInfo->numPoints = numPoints;
	return buffer;
}

OP_SmartRef<POP_Buffer>
SogImporter::createTopologyInfoBuffer(uint32_t numPoints) const
{
	OP_SmartRef<POP_Buffer> buffer = createRawBuffer(sizeof(POP_TopologyInfo), POP_BufferUsage::TopologyInfoBuffer);
	if (!buffer)
		return buffer;

	POP_TopologyInfo* topologyInfo = static_cast<POP_TopologyInfo*>(buffer->getData(nullptr));
	memset(topologyInfo, 0, sizeof(POP_TopologyInfo));
	topologyInfo->pointPrimitivesStartIndex = 0;
	topologyInfo->pointPrimitivesCount = numPoints;
	return buffer;
}

OP_SmartRef<POP_Buffer>
SogImporter::createPointIndexBuffer(uint32_t numPoints) const
{
	std::vector<uint32_t> indices(numPoints);
	for (uint32_t index = 0; index < numPoints; ++index)
		indices[index] = index;

	return copyBuffer(myContext, indices.data(), numPoints, POP_BufferUsage::IndexBuffer);
}

Vector
SogImporter::rotateUnitZByQuaternion(const float* quat)
{
	if (!quat)
		return Vector(0.0f, 0.0f, 1.0f);

	const float x = quat[0];
	const float y = quat[1];
	const float z = quat[2];
	const float w = quat[3];

	Vector normal(
		2.0f * (x * z + w * y),
		2.0f * (y * z - w * x),
		1.0f - 2.0f * (x * x + y * y));

	if (normal.length() <= 0.0f)
		return Vector(0.0f, 0.0f, 1.0f);

	normal.normalize();
	return normal;
}

bool
SogImporter::isSogPath(const std::string& filePath)
{
	const std::string lower = toLower(filePath);
	return lower.size() >= 4 && lower.substr(lower.size() - 4) == ".sog";
}
