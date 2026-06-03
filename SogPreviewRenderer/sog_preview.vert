out VertexData
{
    vec3 worldPos;
    vec4 color;
    vec3 scale;
    vec4 rot;
    flat int cameraIndex;
} vOut;

void main()
{
    vec3 worldPos = TDDeform(TDPos()).xyz;
    vec3 scale3 = max(abs(TDAttrib_scale()), vec3(1.0e-6));
    vec4 rot4 = TDAttrib_rot();

    vOut.worldPos = worldPos;
    vOut.color = TDPointColor();
    vOut.scale = scale3;
    vOut.rot = rot4;
    vOut.cameraIndex = TDCameraIndex();

    gl_Position = TDWorldToProj(worldPos);
}
