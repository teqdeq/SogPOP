# SOG Preview Renderer

This is a TouchDesigner preview renderer for the SogPOP importer. It is not a full anisotropic Gaussian splat renderer, but it replaces 1-pixel points with camera-facing Gaussian quads so the preview is much closer to SuperSplat.

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
4. On the GLSL MAT `Attribute` tab add one custom attribute row:
   - `Name` = `scale`
   - `Type` = `float3`
   - do not use the `Matrix Attribute` section for this
5. Press `Load Uniform Names`, then set these uniforms on the GLSL MAT:
   - `uScaleMul = 3.0`
   - `uMinWorldSize = 0.0005`
   - `uFalloff = 2.5`
   - `uAlphaMul = 1.0`
6. On the GLSL MAT `Common` page:
   - enable `Blending`
   - set source blend to `Source Alpha`
   - set destination blend to `One Minus Source Alpha`
   - keep `Depth Test` on
   - turn `Write Depth Values` off
   - set `Cull Face` to `Off`
7. Assign the GLSL MAT to the Geometry COMP and render it through a `Render TOP`.

Quick mapping to your screenshots:
- the second screenshot is the correct `Load` tab; the missing item there is that `Geometry Shader` must point to `sog_preview.geom`
- the first screenshot is the correct `Attribute` tab; for `scale`, choose `float3` exactly

Notes:
- This preview uses the importer’s `Color` and `alpha` attributes directly.
- It uses the `scale` attribute only as a billboard radius estimate. It does not yet use the quaternion for anisotropic ellipse orientation.
- It does not yet evaluate the `sh1` to `sh8` coefficients for view-dependent shading.
- If the result looks too puffy, reduce `uScaleMul`. If it looks too sparse, increase `uScaleMul`.
- If overlapping transparency looks noisy, try enabling order independent transparency in the `Render TOP`.

What this solves:
- point-cloud look in the POP viewer
- tiny 1-pixel points that hide shape continuity

What it does not solve yet:
- exact SuperSplat visual parity
- anisotropic covariance projection
- SH-based view-dependent lighting
