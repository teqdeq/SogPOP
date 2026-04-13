@echo off
setlocal

set VSROOT=C:\Program Files\Microsoft Visual Studio\18\Community
set VCVARS=%VSROOT%\VC\Auxiliary\Build\vcvars64.bat

if not exist "%VCVARS%" (
	echo vcvars64.bat not found at "%VCVARS%"
	exit /b 1
)

call "%VCVARS%"
if errorlevel 1 exit /b %errorlevel%

if not exist Debug mkdir Debug

set OUTPUT_NAME=%~1
if "%OUTPUT_NAME%"=="" set OUTPUT_NAME=SogPOP_test.dll

cl /nologo /std:c++17 /EHsc /LD ^
	/I. ^
	/Ithird_party\miniz_repo ^
	/Ithird_party\nlohmann ^
	/Ithird_party\libwebp ^
	/Ithird_party\libwebp\src ^
	/DWIN32 /D_WINDOWS /D_USRDLL /DSOGPOP_EXPORTS /DHAVE_CONFIG_H ^
	SogImporter.cpp ^
	third_party\miniz_repo\miniz.c ^
	third_party\miniz_repo\miniz_tdef.c ^
	third_party\miniz_repo\miniz_tinfl.c ^
	third_party\miniz_repo\miniz_zip.c ^
	third_party\libwebp\src\dec\alpha_dec.c ^
	third_party\libwebp\src\dec\buffer_dec.c ^
	third_party\libwebp\src\dec\frame_dec.c ^
	third_party\libwebp\src\dec\idec_dec.c ^
	third_party\libwebp\src\dec\io_dec.c ^
	third_party\libwebp\src\dec\quant_dec.c ^
	third_party\libwebp\src\dec\tree_dec.c ^
	third_party\libwebp\src\dec\vp8_dec.c ^
	third_party\libwebp\src\dec\vp8l_dec.c ^
	third_party\libwebp\src\dec\webp_dec.c ^
	third_party\libwebp\src\dsp\alpha_processing.c ^
	third_party\libwebp\src\dsp\alpha_processing_sse2.c ^
	third_party\libwebp\src\dsp\cpu.c ^
	third_party\libwebp\src\dsp\dec.c ^
	third_party\libwebp\src\dsp\dec_clip_tables.c ^
	third_party\libwebp\src\dsp\dec_sse2.c ^
	third_party\libwebp\src\dsp\filters.c ^
	third_party\libwebp\src\dsp\filters_sse2.c ^
	third_party\libwebp\src\dsp\lossless.c ^
	third_party\libwebp\src\dsp\lossless_sse2.c ^
	third_party\libwebp\src\dsp\rescaler.c ^
	third_party\libwebp\src\dsp\rescaler_sse2.c ^
	third_party\libwebp\src\dsp\upsampling.c ^
	third_party\libwebp\src\dsp\upsampling_sse2.c ^
	third_party\libwebp\src\dsp\yuv.c ^
	third_party\libwebp\src\dsp\yuv_sse2.c ^
	third_party\libwebp\src\utils\bit_reader_utils.c ^
	third_party\libwebp\src\utils\color_cache_utils.c ^
	third_party\libwebp\src\utils\filters_utils.c ^
	third_party\libwebp\src\utils\huffman_utils.c ^
	third_party\libwebp\src\utils\palette.c ^
	third_party\libwebp\src\utils\quant_levels_dec_utils.c ^
	third_party\libwebp\src\utils\random_utils.c ^
	third_party\libwebp\src\utils\rescaler_utils.c ^
	third_party\libwebp\src\utils\thread_utils.c ^
	third_party\libwebp\src\utils\utils.c ^
	/link /OUT:Debug\%OUTPUT_NAME%

endlocal
