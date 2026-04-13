# SogPOP

TouchDesigner 2025_32460 POP scaffold for importing `.sog` files.

Current status:
- New POP project in its own directory.
- TouchDesigner POP SDK headers are copied locally into this project.
- `SogImporter` exposes `File` and `Reload` parameters.
- The operator publishes point attributes `P`, `scale`, `quat`, `CD`, `Color`, `alpha`, `N`, and `sh1` to `sh8`.
- `N` is derived from the quaternion by rotating unit Z.
- `miniz`, `nlohmann/json`, and `libwebp` are vendored into `third_party`.
- The loader opens `.sog` as ZIP, extracts `meta.json`, decodes `.webp` textures, and handles the current SuperSplat export layout including `means_l/means_u`, `scales`, `quats`, `sh0`, and `shN`.
- `build_debug.bat` reproduces the successful direct MSVC build path via `vcvars64.bat`.
- A first-pass preview renderer is available in `../SogPreviewRenderer/` for rendering Gaussian billboard quads via a GLSL MAT.

Next integration steps:
1. Build a full anisotropic splat renderer that uses quaternion orientation and proper covariance projection.
2. Evaluate `sh1` to `sh8` in the material for view-dependent color.
3. Add asynchronous or staged loading so very large archives do not stall the TouchDesigner UI.