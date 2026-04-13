#pragma once

#include "POP_CPlusPlusBase.h"

#include <mutex>
#include <string>
#include <vector>

using namespace TD;

class SogImporter : public POP_CPlusPlusBase
{
public:
	SogImporter(const OP_NodeInfo* info, POP_Context* context);
	~SogImporter() override;

	void		getGeneralInfo(POP_GeneralInfo*, const OP_Inputs*, void* reserved1) override;
	void		execute(POP_Output*, const OP_Inputs*, void* reserved) override;

	int32_t		getNumInfoCHOPChans(void* reserved) override;
	void		getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void* reserved) override;
	bool		getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved) override;
	void		getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries* entries, void* reserved) override;

	void		getWarningString(OP_String* warning, void* reserved) override;
	void		getErrorString(OP_String* error, void* reserved) override;

	void		setupParameters(OP_ParameterManager* manager, void* reserved) override;
	void		pulsePressed(const char* name, void* reserved) override;

private:
	struct PointCache
	{
		std::vector<Position> positions;
		std::vector<Vector> scales;
		std::vector<float> quaternions;
		std::vector<float> colors;
		std::vector<float> alphas;
		std::vector<Vector> normals;
		std::vector<float> shCoefficients;

		void clear();
		[[nodiscard]] bool empty() const;
		[[nodiscard]] uint32_t size() const;
	};

	bool		refreshCache(const std::string& filePath);
	bool		buildPointCache(const std::string& filePath, PointCache& nextCache, std::string& nextWarning, std::string& nextError) const;
	void		publishCache(POP_Output* output, const PointCache& cache);

	OP_SmartRef<POP_Buffer> createRawBuffer(uint64_t size, POP_BufferUsage usage = POP_BufferUsage::Attribute) const;
	OP_SmartRef<POP_Buffer> createPointInfoBuffer(uint32_t numPoints) const;
	OP_SmartRef<POP_Buffer> createTopologyInfoBuffer(uint32_t numPoints) const;
	OP_SmartRef<POP_Buffer> createPointIndexBuffer(uint32_t numPoints) const;

	static Vector rotateUnitZByQuaternion(const float* quat);
	static bool   isSogPath(const std::string& filePath);

	const OP_NodeInfo* const	myNodeInfo;
	POP_Context* const			myContext;
	PointCache					myCache;
	std::mutex					myMutex;
	std::string					myLoadedFile;
	std::string					myWarning;
	std::string					myError;
	bool						myReloadRequested;
	int32_t						myExecuteCount;
};