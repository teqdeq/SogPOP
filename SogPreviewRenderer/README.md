# SOG Preview Renderer

This is a TouchDesigner preview renderer for the SogPOP importer.
It now supports anisotropic Gaussian splat orientation using per-point `scale` and `rot` attributes, so it looks much closer to a true Gaussian splat render than simple point sprites.

Files:
- `sog_preview.vert`
- `sog_preview.geom`
- `sog_preview.frag`

Recommended TouchDesigner setup:
1. Use a `Geometry COMP` and render your `CPlusPlus POP` from inside it, not just the POP viewer.
2. Create a `GLSL MAT` and point its shaders at these three files.
3. On the GLSL MAT `Load` tab:
   - set `Vertex Shader` to `sog_preview.vert`
   - set `Pixel Shader` to `sog_preview.frag`
   - set `Geometry Shader` to `sog_preview.geom`
   - after assigning the geometry shader, make sure these geometry-shader settings are visible on the GLSL MAT parameters:
   - `Input Primitive Type` = `Points`
   - `Output Primitive Type` = `Triangle Strip`
   - `Num Output Vertices` = `4`
   - if you do not see those three fields immediately, widen the parameter dialog or scroll farther down/right on the `Load` tab; they belong to the GLSL MAT itself, not to the shader code
4. On the GLSL MAT `Attribute` tab add these custom attribute rows:
   - `Name` = `scale`
   - `Type` = `float3`
   - `Name` = `rot`
   - `Type` = `float4`
   - do not use the `Matrix Attribute` section for these
5. Press `Load Uniform Names`, then set these uniforms on the GLSL MAT:
   - `uScaleMul = 1.0`
   - `uSigmaExtent = 3.0`
   - `uMinWorldSize = 0.0005`
   - `uMaxWorldSize = 0.25`
   - `uAnisoAmount = 1.0`
   - `uFalloff = 1.0`
   - `uAlphaMul = 1.0`
   - `uAlphaGamma = 1.0`
   - `uColorMul = 1.0`
   - `uSoftClip = 0.01`
6. On the GLSL MAT `Common` page:
   - enable `Blending`
   - set source blend to `Source Alpha`
   - set destination blend to `One Minus Source Alpha`
   - keep `Depth Test` on
   - turn `Write Depth Values` off
   - set `Cull Face` to `Off`
7. Assign the GLSL MAT to the Geometry COMP and render it through a `Render TOP`.

Recommended control ranges:
- `uScaleMul`: `0.2` to `4.0`
- `uSigmaExtent`: `1.5` to `4.0`
- `uMinWorldSize`: `0.00005` to `0.005`
- `uMaxWorldSize`: `0.02` to `1.0`
- `uAnisoAmount`: `0.0` (isotropic) to `1.0` (full anisotropic)
- `uFalloff`: `0.4` to `1.5`
- `uAlphaMul`: `0.2` to `4.0`
- `uAlphaGamma`: `0.6` to `1.8`
- `uColorMul`: `0.5` to `2.0`
- `uSoftClip`: `0.0` to `0.08`

Notes:
- This preview uses the importer’s `Color` and `alpha` attributes directly.
- It uses `scale` + `rot` to estimate anisotropic projected covariance and orient each splat ellipse.
- It does not yet evaluate the `sh1` to `sh8` coefficients for view-dependent shading.
- If the result looks too puffy, reduce `uScaleMul` or `uSigmaExtent`.
- If the result looks too sparse, increase `uScaleMul`, `uAlphaMul`, or `uMaxWorldSize`.
- If overlapping transparency looks noisy, try enabling order independent transparency in the `Render TOP`.

What this solves:
- point-cloud look in the POP viewer
- tiny 1-pixel points that hide shape continuity
- isotropic billboard look that ignores splat orientation

What it does not solve yet:
- exact SuperSplat visual parity
- SH-based view-dependent lighting
