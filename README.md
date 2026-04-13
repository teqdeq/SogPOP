# SogPOP

TouchDesigner `.sog` importer and preview renderer for SuperSplat-style exports.

Quick start:
- Recommended DLL: [SogPOP/Debug/SogPOP.dll](SogPOP/Debug/SogPOP.dll)
- Sample file: [cluster fly XL.sog](cluster%20fly%20XL.sog)
- Importer setup: [SogPOP/README.md](SogPOP/README.md)
- Preview renderer setup: [SogPreviewRenderer/README.md](SogPreviewRenderer/README.md)

Compatibility note:
- The included DLL is built for 64-bit TouchDesigner 2025.32460. If another machine reports `failed to load plugin`, first verify it is running a compatible 64-bit TouchDesigner 2025 build.
- If TouchDesigner only shows a generic load failure, run this on that machine to get the real Windows loader error:
- `powershell -ExecutionPolicy Bypass -File .\tools\check_plugin_load.ps1 .\SogPOP\Debug\SogPOP.dll`
- The current build imports `CreateFile2` and `GetSystemTimePreciseAsFileTime`, so Windows 8+ is required.

Included in this repository:
- `SogPOP/`: the self-contained CPlusPlus POP project, including vendored dependencies needed to build on another computer
- `SogPreviewRenderer/`: GLSL preview renderer files for a first-pass Gaussian billboard preview inside TouchDesigner

Excluded from this repository:
- local build outputs (`Debug/`, `.obj`, `.dll`, `.exe`, `.lib`, `.exp`)
- local Visual Studio cache/state files
- local diagnostic helper files used during reverse-engineering
- sample `.sog` data and TouchDesigner sample/reference projects

Start with [SogPOP/README.md](SogPOP/README.md) for the importer and [SogPreviewRenderer/README.md](SogPreviewRenderer/README.md) for the preview renderer setup.