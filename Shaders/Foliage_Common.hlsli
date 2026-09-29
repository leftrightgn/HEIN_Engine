//--------------------------------------------------------------------------------------
// Foliage_Common.hlsli
// Stylized Foliage Shader (Ghibli / Ghost of Tsushima style)
//--------------------------------------------------------------------------------------

cbuffer PerFrameBuffer : register(b0)
{
    matrix worldMatrix;
    matrix viewMatrix;
    matrix projectionMatrix;
    matrix lightViewProj;
    float4 cameraPosition;
    float  time;
    float  windSpeed;
    float  windStrength;
    float  noiseScale;
    float2 windDirection;
    float  alphaCutoff;
    float  terrainWidth;
    float  terrainHeight;
    float3 paddingPerFrame;
};

cbuffer FoliageSettingsBuffer : register(b1)
{
    float4 grassRootColor;
    float4 grassTipColor;
    float4 flowerColor;
    float4 leafColor;
    float4 barkColor;
    float4 lightDirection;
    float4 lightColor;
    float4 ambientColor;
    float4 flags; // x: useColorMap, y: useTexture, z: hasShadows, w: unused
    
    float maxGrassHeight;
    float maxGrassWidth;
    float tilt;
    float bend;
};

struct FoliageVertexInput
{
    // Stream 0: Shared Mesh Data
    float3 position   : POSITION;
    float3 normal     : NORMAL;
    float2 texCoord   : TEXCOORD0;
    float  windWeight : BLENDWEIGHT;
    float4 color      : COLOR;

    // Stream 1: Per-Instance Data
    float3 instPos    : INST_POS;
    float  instRot    : INST_ROT;
    float  instScale  : INST_SCALE;
    uint   instType   : INST_TYPE;
};

struct FoliagePixelInput
{
    float4 position    : SV_POSITION;
    float2 texCoord    : TEXCOORD0;
    float3 normal      : NORMAL;
    float3 worldPos    : TEXCOORD1;
    float  windWeight  : BLENDWEIGHT;
    uint   foliageType : BLENDINDICES;
    float4 shadowPos   : TEXCOORD2;
    float4 vertexColor : COLOR;
};
