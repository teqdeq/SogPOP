# SogPOP

TouchDesigner `.sog` importer and preview renderer for SuperSplat-style exports.

Quick start:
- Recommended test DLL: [SogPOP/Debug/SogPOP_test_clean.dll](SogPOP/Debug/SogPOP_test_clean.dll)
- Sample file: [cluster fly XL.sog](cluster%20fly%20XL.sog)
- Importer setup: [SogPOP/README.md](SogPOP/README.md)
- Preview renderer setup: [SogPreviewRenderer/README.md](SogPreviewRenderer/README.md)

Included in this repository:
- `SogPOP/`: the self-contained CPlusPlus POP project, including vendored dependencies needed to build on another computer
- `SogPreviewRenderer/`: GLSL preview renderer files for a first-pass Gaussian billboard preview inside TouchDesigner

Excluded from this repository:
- local build outputs (`Debug/`, `.obj`, `.dll`, `.exe`, `.lib`, `.exp`)
- local Visual Studio cache/state files
- local diagnostic helper files used during reverse-engineering
- sample `.sog` data and TouchDesigner sample/reference projects

Start with [SogPOP/README.md](SogPOP/README.md) for the importer and [SogPreviewRenderer/README.md](SogPreviewRenderer/README.md) for the preview renderer setup.