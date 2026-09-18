
// Constant Buffers(Data sent form C++ to GPU every frame)

cbuffer MatrixBuffer : register(b0)
{
    matrix worldMatrix; // where is the terrain in the world
    matrix viewMatrix; // where is the camera looking
    matrix projectionMatrix; // camera field of view (fov) aspect ration
}

cbuffer lightBuffer : register(b1)
{
    float4 diffuseColor;   // the color of the sunlight (RGBA) 16bytes
    float3 lightDirection;  // the angle sun is shining for the (xyz) 12bytes
    float  hasTexture;      // flag if diffuse texture is bound 4 bytes
    
    
    float  textureTiling;  // 4bytes
    float  hasNormalMap;    // flag if normal map is bound 4bytes
    float hasAlphaMap;   // 4bytes
    float hasTexture2;   // 4bytes
    float fogStart;
    float fogEnd;
    float padding1;
    float padding2;
    float4 fogColor;
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
    float  viewZ    : TEXCOORD1;
    float3 normal   : NORMAL;
    float3 tangent  : TANGENT;
    float3 binormal : BINORMAL;
    float4 color    : COLOR;
};