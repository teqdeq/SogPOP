layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform float uScaleMul = 3.0;
uniform float uMinWorldSize = 0.0005;

in VertexData
{
    vec3 worldPos;
    vec4 color;
    vec2 previewScale;
    flat int cameraIndex;
} gIn[];

out GeoData
{
    vec4 color;
    vec2 localCoord;
    vec3 worldPos;
    flat int cameraIndex;
} gOut;

void emitCorner(vec2 corner, vec3 center, vec3 rightAxis, vec3 upAxis, vec2 halfSize, vec4 color, int cameraIndex)
{
    vec3 worldPos = center + rightAxis * (corner.x * halfSize.x) + upAxis * (corner.y * halfSize.y);
    gOut.color = color;
    gOut.localCoord = corner;
    gOut.worldPos = worldPos;
    gOut.cameraIndex = cameraIndex;
    gl_Position = TDWorldToProj(worldPos, cameraIndex);
    EmitVertex();
}

void main()
{
    vec3 center = gIn[0].worldPos;
    vec4 color = gIn[0].color;
    int cameraIndex = gIn[0].cameraIndex;

    vec3 camRight = normalize(uTDMats[cameraIndex].camInverse[0].xyz);
    vec3 camUp = normalize(uTDMats[cameraIndex].camInverse[1].xyz);
    vec2 halfSize = max(gIn[0].previewScale * uScaleMul, vec2(uMinWorldSize));

    emitCorner(vec2(-1.0, -1.0), center, camRight, camUp, halfSize, color, cameraIndex);
    emitCorner(vec2( 1.0, -1.0), center, camRight, camUp, halfSize, color, cameraIndex);
    emitCorner(vec2(-1.0,  1.0), center, camRight, camUp, halfSize, color, cameraIndex);
    emitCorner(vec2( 1.0,  1.0), center, camRight, camUp, halfSize, color, cameraIndex);
    EndPrimitive();
}
