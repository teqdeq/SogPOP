# SOG Preview Renderer

This is a TouchDesigner preview renderer for the SogPOP importer.
It supports anisotropic Gaussian splat orientation using per-point `InitScale` and `rot` attributes, and adaptive SH view-dependent color evaluation using `sh1` to `sh15` attributes.

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
4. On the GLSL MAT `Attribute` tab:
   - always add:
   - `Name` = `InitScale`
   - `Type` = `float3`
   - `Name` = `rot`
   - `Type` = `float4`
   - then add SH rows for your export level:
   - `sh0`: add none of `sh1..sh15`
   - `sh1`: add `sh1..sh3`
   - `sh2`: add `sh1..sh8`
   - `sh3`: add `sh1..sh15`
   - full-capability setup (works for all exports) is to add `sh1..sh15` once and keep it.
   - do not use the `Matrix Attribute` section for these

   Full-capability `sh1..sh15` rows:
   - `Name` = `InitScale`
   - `Type` = `float3`
   - `Name` = `rot`
   - `Type` = `float4`
   - `Name` = `sh1`
   - `Type` = `float3`
   - `Name` = `sh2`
   - `Type` = `float3`
   - `Name` = `sh3`
   - `Type` = `float3`
   - `Name` = `sh4`
   - `Type` = `float3`
   - `Name` = `sh5`
   - `Type` = `float3`
   - `Name` = `sh6`
   - `Type` = `float3`
   - `Name` = `sh7`
   - `Type` = `float3`
   - `Name` = `sh8`
   - `Type` = `float3`
   - `Name` = `sh9`
   - `Type` = `float3`
   - `Name` = `sh10`
   - `Type` = `float3`
   - `Name` = `sh11`
   - `Type` = `float3`
   - `Name` = `sh12`
   - `Type` = `float3`
   - `Name` = `sh13`
   - `Type` = `float3`
   - `Name` = `sh14`
   - `Type` = `float3`
   - `Name` = `sh15`
   - `Type` = `float3`
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
   - `uShMix = 1.0`
   - `uShIntensity = 1.0`
   - `uShSaturation = 1.0`
   - `uShClampMin = 0.0`
   - `uShClampMax = 4.0`
   - `uShDegree = 2.0`

Vectors tab value mapping (important):
- In TouchDesigner, these uniforms appear in the `Vectors` page with 4 value fields.
- All uniforms in this shader are `float`, so only `Value 1` (left-most/X) is used.
- `Value 2`, `Value 3`, and `Value 4` are ignored for these uniforms.

Uniform reference (what each value does):
- `uScaleMul`: global splat size multiplier before projection. Lower = smaller splats.
- `uSigmaExtent`: how far each quad extends in sigma units. Lower = tighter splats.
- `uMinWorldSize`: minimum world-space half-size clamp. Prevents tiny splats disappearing.
- `uMaxWorldSize`: maximum world-space half-size clamp. Main control to stop giant billboards.
- `uAnisoAmount`: anisotropy blend (`0` isotropic, `1` fully anisotropic from `InitScale` + `rot`).
- `uFalloff`: Gaussian falloff width in local splat space. Lower = denser center, faster edge fade.
- `uAlphaMul`: alpha multiplier after Gaussian. Higher = more opaque/stronger accumulation.
- `uAlphaGamma`: gamma on alpha (`1` linear). Higher generally thins low-alpha tails.
- `uColorMul`: multiplies base color intensity before SH mixing.
- `uSoftClip`: soft edge clipping near splat boundary. Higher = softer cut near radius edge.
- `uShMix`: SH blend amount (`0` base color only, `1` full SH contribution).
- `uShIntensity`: SH contribution strength.
- `uShSaturation`: saturation after SH mix (`0` grayscale, `1` original saturation).
- `uShClampMin`: minimum RGB clamp after SH/color processing.
- `uShClampMax`: maximum RGB clamp after SH/color processing.
- `uShDegree`: SH degree used in shader (`0`, `1`, `2`, `3`). Rounded to nearest integer in shader.
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
- `uShMix`: `0.0` (disable SH) to `1.0` (full SH)
- `uShIntensity`: `0.5` to `2.0`
- `uShSaturation`: `0.0` to `1.5`
- `uShClampMin`: `-1.0` to `0.2`
- `uShClampMax`: `1.0` to `8.0`
- `uShDegree`: `0.0` (sh0), `1.0` (sh1), `2.0` (sh2), `3.0` (sh3)

Automation:
- The importer now exposes these Info values:
- `shTripletCount`
- `shDegree`
- You can auto-drive the GLSL MAT `uShDegree` from those values.
- A ready script is included at `tools/touchdesigner_auto_sh_degree.py`.
- Typical setup:
- Create a DAT Execute DAT and monitor the SogPOP Info DAT table.
- Paste the script into that DAT Execute DAT.
- Update `INFO_DAT_PATH` and `GLSL_MAT_PATH` constants at the top of the script.
- Keep GLSL MAT attributes on full-capability (`sh1..sh15`) to avoid dynamic remapping.

Notes:
- This preview uses the importer’s `Color` and `alpha` attributes directly.
- It uses `InitScale` + `rot` to estimate anisotropic projected covariance and orient each splat ellipse.
- It evaluates SH view-dependent color from `sh1` to `sh15` and mixes it with the base `Color`.
- Set `uShDegree` to match your export level from SuperSplat: sh0/sh1/sh2/sh3.
- Or use the automation script to set `uShDegree` automatically from the importer Info DAT.
- If you want to compare against the old static look quickly, set `uShMix = 0.0`.
- If the result looks too puffy, reduce `uScaleMul` or `uSigmaExtent`.
- If the result looks too sparse, increase `uScaleMul`, `uAlphaMul`, or `uMaxWorldSize`.
- If overlapping transparency looks noisy, try enabling order independent transparency in the `Render TOP`.

What this solves:
- point-cloud look in the POP viewer
- tiny 1-pixel points that hide shape continuity
- isotropic billboard look that ignores splat orientation
- no view-dependent color from SH2 data

What it does not solve yet:
- exact SuperSplat visual parity
- full higher-order SH workflows beyond SH3
