Texture2D noiseMap : register(t0);
SamplerState SampleType : register(s0);
cbuffer WaterBuffer : register(b0)
{
    float time;
    float3 padding;
    matrix world;
    matrix view;
    matrix projection;
};

cbuffer WaterSetting : register(b1)
{
    float3 waterColor;
    float fresnelPower;
    float specularPower;
    float waveSpeed;
    float normalBlend;
    float padding3;
    float uvScale;
    float offset;
    float multiplier;
}

struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 Tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 Normal : NORMAL;
    float2 Tex : TEXCOORD0;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    
    float4 posWorld = mul(input.Pos, world);
    float t = time * waveSpeed;
    
    
    // scrolling
    float2 uvScroll = float2(t * 0.03f, t * 0.02f);
    
    float2 centerUV = (posWorld.xz * uvScale) + uvScroll;
    
    // Sample the texture for the Height
    // Sample slightly to the right and slightly up to calculate the normal slope
  
    float heightCenter = noiseMap.SampleLevel(SampleType, centerUV, 0).r;
    float heightRight = noiseMap.SampleLevel(SampleType, centerUV + float2(offset, 0.0f), 0).r;
    float heightUp = noiseMap.SampleLevel(SampleType, centerUV + float2(0.0f, offset), 0).r;
    
    // Apply the height to the vertex
    float waveHeightMutiplier = multiplier;
    posWorld.y += heightCenter * waveHeightMutiplier;
    
    
    float physicalDistance = offset / uvScale;
    // Calculate the true Normal
    // Calculate the slope (derivative) by comparing the center pixel to its neighbors
    float dYdX = ((heightRight - heightCenter) * waveHeightMutiplier) / physicalDistance;
    float dYdZ = ((heightUp - heightCenter) * waveHeightMutiplier) / physicalDistance;

      
    // Create the new normal vector facing perpendicular to the wave slope
    float3 trueNormal = normalize(float3(-dYdX, 1.0f, -dYdZ));

    // 3. Set Outputs
    output.WorldPos = posWorld.xyz;
    output.Pos = mul(posWorld, mul(view, projection));
    
    // Apply the new wave normal instead of the flat grid normal
    output.Normal = mul(trueNormal, (float3x3) world);
    
    output.Tex = input.Tex + float2(t * 0.05f, t * 0.02f);
    
    return output;
}