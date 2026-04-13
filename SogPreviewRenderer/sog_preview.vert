out VertexData
{
    vec3 worldPos;
    vec4 color;
    vec2 previewScale;
    flat int cameraIndex;
} vOut;

void main()
{
    vec3 worldPos = TDDeform(TDPos()).xyz;
    vec3 scale3 = max(abs(TDAttrib_scale()), vec3(1.0e-6));

    vOut.worldPos = worldPos;
    vOut.color = TDPointColor();
    vOut.previewScale = vec2(max(scale3.x, scale3.z), max(scale3.y, scale3.z));
    vOut.cameraIndex = TDCameraIndex();

    gl_Position = TDWorldToProj(worldPos);
}
