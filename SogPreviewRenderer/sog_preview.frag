uniform float uFalloff = 1.0;
uniform float uAlphaMul = 1.0;
uniform float uAlphaGamma = 1.0;
uniform float uColorMul = 1.0;
uniform float uSoftClip = 0.01;
uniform float uShMix = 1.0;
uniform float uShIntensity = 1.0;
uniform float uShSaturation = 1.0;
uniform float uShClampMin = 0.0;
uniform float uShClampMax = 4.0;
uniform float uShDegree = 2.0;

in GeoData
{
    vec4 color;
    vec2 localCoord;
    vec3 worldPos;
    flat vec3 sh1;
    flat vec3 sh2;
    flat vec3 sh3;
    flat vec3 sh4;
    flat vec3 sh5;
    flat vec3 sh6;
    flat vec3 sh7;
    flat vec3 sh8;
    flat vec3 sh9;
    flat vec3 sh10;
    flat vec3 sh11;
    flat vec3 sh12;
    flat vec3 sh13;
    flat vec3 sh14;
    flat vec3 sh15;
    flat int cameraIndex;
} fIn;

layout(location = 0) out vec4 fragColor;

vec3 applySaturation(vec3 color, float saturation)
{
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(luma), color, saturation);
}

vec3 evalSh(
    vec3 viewDir,
    float degree,
    vec3 sh1,
    vec3 sh2,
    vec3 sh3,
    vec3 sh4,
    vec3 sh5,
    vec3 sh6,
    vec3 sh7,
    vec3 sh8,
    vec3 sh9,
    vec3 sh10,
    vec3 sh11,
    vec3 sh12,
    vec3 sh13,
    vec3 sh14,
    vec3 sh15)
{
    float x = viewDir.x;
    float y = viewDir.y;
    float z = viewDir.z;

    vec3 result = vec3(0.0);

    if (degree >= 1.0)
    {
        float b1 = 0.4886025119 * y;
        float b2 = 0.4886025119 * z;
        float b3 = 0.4886025119 * x;
        result += sh1 * b1 + sh2 * b2 + sh3 * b3;
    }

    if (degree >= 2.0)
    {
        float b4 = 1.0925484306 * x * y;
        float b5 = 1.0925484306 * y * z;
        float b6 = 0.3153915653 * (3.0 * z * z - 1.0);
        float b7 = 1.0925484306 * x * z;
        float b8 = 0.5462742153 * (x * x - y * y);
        result += sh4 * b4 + sh5 * b5 + sh6 * b6 + sh7 * b7 + sh8 * b8;
    }

    if (degree >= 3.0)
    {
        float b9 = 0.5900435899 * y * (3.0 * x * x - y * y);
        float b10 = 2.8906114426 * x * y * z;
        float b11 = 0.4570457995 * y * (5.0 * z * z - 1.0);
        float b12 = 0.3731763325 * z * (5.0 * z * z - 3.0);
        float b13 = 0.4570457995 * x * (5.0 * z * z - 1.0);
        float b14 = 1.4453057213 * z * (x * x - y * y);
        float b15 = 0.5900435899 * x * (x * x - 3.0 * y * y);
        result += sh9 * b9 + sh10 * b10 + sh11 * b11 + sh12 * b12 + sh13 * b13 + sh14 * b14 + sh15 * b15;
    }

    return result;
}

void main()
{
    TDCheckDiscard();

    float radiusSq = dot(fIn.localCoord, fIn.localCoord);
    if (radiusSq > 1.0)
        discard;

    float sigma = max(1.0e-5, uFalloff);
    float gaussian = exp(-0.5 * radiusSq / (sigma * sigma));

    float edge = smoothstep(1.0, max(1.0 - uSoftClip, 1.0e-3), radiusSq);
    gaussian *= edge;

    float alpha = clamp(fIn.color.a * gaussian * uAlphaMul, 0.0, 1.0);
    alpha = pow(alpha, max(0.01, uAlphaGamma));
    TDAlphaTest(alpha);

    vec3 camPos = uTDMats[fIn.cameraIndex].camInverse[3].xyz;
    vec3 viewDir = camPos - fIn.worldPos;
    float vlen = length(viewDir);
    if (vlen <= 1.0e-8)
        viewDir = vec3(0.0, 0.0, 1.0);
    else
        viewDir /= vlen;

    vec3 baseColor = fIn.color.rgb * uColorMul;
    float shDegree = clamp(floor(uShDegree + 0.5), 0.0, 3.0);
    vec3 shDelta = evalSh(
        viewDir,
        shDegree,
        fIn.sh1,
        fIn.sh2,
        fIn.sh3,
        fIn.sh4,
        fIn.sh5,
        fIn.sh6,
        fIn.sh7,
        fIn.sh8,
        fIn.sh9,
        fIn.sh10,
        fIn.sh11,
        fIn.sh12,
        fIn.sh13,
        fIn.sh14,
        fIn.sh15);
    vec3 shColor = baseColor + shDelta * uShIntensity;
    vec3 rgb = mix(baseColor, shColor, clamp(uShMix, 0.0, 1.0));
    rgb = applySaturation(rgb, max(0.0, uShSaturation));
    rgb = clamp(rgb, uShClampMin, uShClampMax);

    vec4 color = vec4(rgb * alpha, alpha);
    color = TDFog(color, fIn.worldPos, fIn.cameraIndex);
    color = TDDither(color);
    fragColor = TDOutputSwizzle(TDPixelColor(color));
}
