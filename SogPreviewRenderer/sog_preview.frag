uniform float uFalloff = 2.5;
uniform float uAlphaMul = 1.0;

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

    float gaussian = exp(-radiusSq * uFalloff * uFalloff);
    float alpha = clamp(fIn.color.a * gaussian * uAlphaMul, 0.0, 1.0);
    TDAlphaTest(alpha);

    vec4 color = vec4(fIn.color.rgb * alpha, alpha);
    color = TDFog(color, fIn.worldPos, fIn.cameraIndex);
    color = TDDither(color);
    fragColor = TDOutputSwizzle(TDPixelColor(color));
}
