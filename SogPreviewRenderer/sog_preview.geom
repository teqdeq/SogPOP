layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform float uScaleMul = 1.0;
uniform float uSigmaExtent = 3.0;
uniform float uMinWorldSize = 0.0005;
uniform float uMaxWorldSize = 0.25;
uniform float uAnisoAmount = 1.0;

in VertexData
{
    vec3 worldPos;
    vec4 color;
    vec3 scale;
    vec4 rot;
    flat int cameraIndex;
} gIn[];

out GeoData
{
    vec4 color;
    vec2 localCoord;
    vec3 worldPos;
    flat int cameraIndex;
} gOut;

mat3 quatToMat3(vec4 q)
{
    vec4 nq = q;
    float len = length(nq);
    if (len <= 1.0e-8)
        nq = vec4(0.0, 0.0, 0.0, 1.0);
    else
        nq /= len;

    float x = nq.x;
    float y = nq.y;
    float z = nq.z;
    float w = nq.w;

    float xx = x * x;
    float yy = y * y;
    float zz = z * z;
    float xy = x * y;
    float xz = x * z;
    float yz = y * z;
    float wx = w * x;
    float wy = w * y;
    float wz = w * z;

    return mat3(
        1.0 - 2.0 * (yy + zz), 2.0 * (xy + wz), 2.0 * (xz - wy),
        2.0 * (xy - wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz + wx),
        2.0 * (xz + wy), 2.0 * (yz - wx), 1.0 - 2.0 * (xx + yy));
}

vec2 projectToCameraPlane(vec3 axis, vec3 camRight, vec3 camUp)
{
    return vec2(dot(axis, camRight), dot(axis, camUp));
}

void solveSymmetricEigen(vec2 c0, vec2 c1, out vec2 dirMajor, out vec2 dirMinor, out float valMajor, out float valMinor)
{
    float a = c0.x;
    float b = c0.y;
    float c = c1.y;
    float trace = a + c;
    float diff = a - c;
    float disc = sqrt(max(0.0, 0.25 * diff * diff + b * b));

    valMajor = max(1.0e-10, 0.5 * trace + disc);
    valMinor = max(1.0e-10, 0.5 * trace - disc);

    vec2 v = vec2(b, valMajor - a);
    if (dot(v, v) <= 1.0e-14)
        v = vec2(1.0, 0.0);
    dirMajor = normalize(v);
    dirMinor = vec2(-dirMajor.y, dirMajor.x);
}

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
    vec3 scale3 = max(gIn[0].scale * uScaleMul, vec3(1.0e-7));

    vec3 camRight = normalize(uTDMats[cameraIndex].camInverse[0].xyz);
    vec3 camUp = normalize(uTDMats[cameraIndex].camInverse[1].xyz);

    mat3 rotMat = quatToMat3(gIn[0].rot);
    vec3 axisX = rotMat[0] * scale3.x;
    vec3 axisY = rotMat[1] * scale3.y;
    vec3 axisZ = rotMat[2] * scale3.z;

    vec2 px = projectToCameraPlane(axisX, camRight, camUp);
    vec2 py = projectToCameraPlane(axisY, camRight, camUp);
    vec2 pz = projectToCameraPlane(axisZ, camRight, camUp);

    vec2 covCol0Aniso = vec2(
        px.x * px.x + py.x * py.x + pz.x * pz.x,
        px.x * px.y + py.x * py.y + pz.x * pz.y);
    vec2 covCol1Aniso = vec2(
        covCol0Aniso.y,
        px.y * px.y + py.y * py.y + pz.y * pz.y);

    float isoRadius = max(max(scale3.x, scale3.y), scale3.z);
    vec2 covCol0Iso = vec2(isoRadius * isoRadius, 0.0);
    vec2 covCol1Iso = vec2(0.0, isoRadius * isoRadius);

    float aniso = clamp(uAnisoAmount, 0.0, 1.0);
    vec2 covCol0 = mix(covCol0Iso, covCol0Aniso, aniso);
    vec2 covCol1 = mix(covCol1Iso, covCol1Aniso, aniso);

    vec2 dirMajor2;
    vec2 dirMinor2;
    float valMajor;
    float valMinor;
    solveSymmetricEigen(covCol0, covCol1, dirMajor2, dirMinor2, valMajor, valMinor);

    vec3 majorWorld = normalize(camRight * dirMajor2.x + camUp * dirMajor2.y);
    vec3 minorWorld = normalize(camRight * dirMinor2.x + camUp * dirMinor2.y);

    float majorHalf = clamp(uSigmaExtent * sqrt(valMajor), uMinWorldSize, uMaxWorldSize);
    float minorHalf = clamp(uSigmaExtent * sqrt(valMinor), uMinWorldSize, uMaxWorldSize);
    vec2 halfSize = vec2(majorHalf, minorHalf);

    emitCorner(vec2(-1.0, -1.0), center, majorWorld, minorWorld, halfSize, color, cameraIndex);
    emitCorner(vec2( 1.0, -1.0), center, majorWorld, minorWorld, halfSize, color, cameraIndex);
    emitCorner(vec2(-1.0,  1.0), center, majorWorld, minorWorld, halfSize, color, cameraIndex);
    emitCorner(vec2( 1.0,  1.0), center, majorWorld, minorWorld, halfSize, color, cameraIndex);
    EndPrimitive();
}
