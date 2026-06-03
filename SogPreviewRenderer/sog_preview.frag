uniform float uFalloff = 1.0;
uniform float uAlphaMul = 1.0;
uniform float uAlphaGamma = 1.0;
uniform float uColorMul = 1.0;
uniform float uSoftClip = 0.01;

in GeoData
{
    vec4 color;
    vec2 localCoord;
    vec3 worldPos;
    flat int cameraIndex;
} fIn;

layout(location = 0) out vec4 fragColor;

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

    vec3 rgb = fIn.color.rgb * uColorMul;
    vec4 color = vec4(rgb * alpha, alpha);
    color = TDFog(color, fIn.worldPos, fIn.cameraIndex);
    color = TDDither(color);
    fragColor = TDOutputSwizzle(TDPixelColor(color));
}
