
// Constant Buffers(Data sent form C++ to GPU every frame)

cbuffer MatrixBuffer : register(b0)
{
    matrix worldMatrix; // where is the terrain in the world
    matrix viewMatrix; // where is the camera looking
    matrix projectionMatrix; // camera field of view (fov) aspect ration
}

cbuffer lightBuffer : register(b1)
{
    float4 diffuseColor; // the color of the sunlight (RGBA)
    float3 lightDirection; // the angle sun is shining for the (xyz)
    
    // WARNING: The 16-Byte Rule!
    // GPU memory chunks MUST be packed in multiples of 16 bytes (4 floats).
    // float4 is 16 bytes (Perfect).
    // float3 is 12 bytes. We use the remaining 4 bytes as a flag
    // to tell the shader if a texture is bound!
    float hasTexture;
}

//  Struct (Data format for moving vertices through pipeline)
struct VertexInputType
{
    float4 position : SV_Position;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 tex      : TEXCOORD0;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex      : TEXCOORD0;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
};