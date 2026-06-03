out VertexData
{
    vec3 worldPos;
    vec4 color;
    vec3 scale;
    vec4 rot;
    vec3 sh1;
    vec3 sh2;
    vec3 sh3;
    vec3 sh4;
    vec3 sh5;
    vec3 sh6;
    vec3 sh7;
    vec3 sh8;
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
    vOut.sh1 = TDAttrib_sh1();
    vOut.sh2 = TDAttrib_sh2();
    vOut.sh3 = TDAttrib_sh3();
    vOut.sh4 = TDAttrib_sh4();
    vOut.sh5 = TDAttrib_sh5();
    vOut.sh6 = TDAttrib_sh6();
    vOut.sh7 = TDAttrib_sh7();
    vOut.sh8 = TDAttrib_sh8();
    vOut.cameraIndex = TDCameraIndex();

    gl_Position = TDWorldToProj(worldPos);
}
