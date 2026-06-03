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
    flat int cameraIndex;
} fIn;

layout(location = 0) out vec4 fragColor;

vec3 applySaturation(vec3 color, float saturation)
{
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    return mix(vec3(luma), color, saturation);
}

vec3 evalSh2(vec3 viewDir, vec3 sh1, vec3 sh2, vec3 sh3, vec3 sh4, vec3 sh5, vec3 sh6, vec3 sh7, vec3 sh8)
{
    float x = viewDir.x;
    float y = viewDir.y;
    float z = viewDir.z;

    float b1 = 0.4886025119 * y;
    float b2 = 0.4886025119 * z;
    float b3 = 0.4886025119 * x;
    float b4 = 1.0925484306 * x * y;
    float b5 = 1.0925484306 * y * z;
    float b6 = 0.3153915653 * (3.0 * z * z - 1.0);
    float b7 = 1.0925484306 * x * z;
    float b8 = 0.5462742153 * (x * x - y * y);

    return
        sh1 * b1 +
        sh2 * b2 +
        sh3 * b3 +
        sh4 * b4 +
        sh5 * b5 +
        sh6 * b6 +
        sh7 * b7 +
        sh8 * b8;
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
    vec3 shDelta = evalSh2(viewDir, fIn.sh1, fIn.sh2, fIn.sh3, fIn.sh4, fIn.sh5, fIn.sh6, fIn.sh7, fIn.sh8);
    vec3 shColor = baseColor + shDelta * uShIntensity;
    vec3 rgb = mix(baseColor, shColor, clamp(uShMix, 0.0, 1.0));
    rgb = applySaturation(rgb, max(0.0, uShSaturation));
    rgb = clamp(rgb, uShClampMin, uShClampMax);

    vec4 color = vec4(rgb * alpha, alpha);
    color = TDFog(color, fIn.worldPos, fIn.cameraIndex);
    color = TDDither(color);
    fragColor = TDOutputSwizzle(TDPixelColor(color));
}
