#pragma once

#if defined(_WIN32) && defined(MINIZ_DLL)
#define MINIZ_EXPORT __declspec(dllexport)
#elif defined(_WIN32) && defined(MINIZ_DLL_IMPORT)
#define MINIZ_EXPORT __declspec(dllimport)
#else
#define MINIZ_EXPORT
#endif