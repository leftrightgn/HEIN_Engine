
// Constant Buffers(Data sent form C++ to GPU every frame)

cbuffer MatrixBuffer : register(b0)
{
    matrix worldMatrix; // where is the terrain in the world
    matrix viewMatrix; // where is the camera looking
    matrix projectionMatrix; // camera field of view (fov) aspect ration
}

cbuffer lightBuffer : register(b1)
{
    float4 diffuseColor;   // the color of the sunlight (RGBA)
    float3 lightDirection;  // the angle sun is shining for the (xyz)
    float  hasTexture;      // flag if diffuse texture is bound
    float  textureTiling;
    float  hasNormalMap;    // flag if normal map is bound
    float2 padding;
}

//  Struct (Data format for moving vertices through pipeline)
struct VertexInputType
{
    float4 position : POSITION;
    float2 tex      : TEXCOORD0;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
    float3 binormal : BINORMAL;
    float4 color    : COLOR;
   
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex      : TEXCOORD0;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
    float3 binormal : BINORMAL;
    float4 color    : COLOR;
};