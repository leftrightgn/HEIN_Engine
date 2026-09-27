// CustomSkinned.hlsl

// ==============================================================================
// CONSTANT BUFFERS (Data from C++)
// ==============================================================================
cbuffer cbMatrices : register(b0)
{
    matrix WorldViewProj;
    matrix World;
    matrix LightViewProj; // Matrix from the Light's perspective for Shadows
    matrix BoneTransforms[256]; // Animation bone matrices
};

cbuffer cbLighting : register(b1)
{
    float3 LightPos;
    int LightType; // 0 = Directional, 1 = Point, 2 = Spot
    
    float3 LightDir;
    float LightIntensity;
    
    float4 LightColor;
    
    float LightRange;
    float LightSpotAngle; // Passed in radians
    float2 Padding;
};

// ==============================================================================
// TEXTURES & SAMPLERS
// ==============================================================================
Texture2D DiffuseMap : register(t0); // The model's texture
Texture2D ShadowMap : register(t4); // The generated shadow map
SamplerState LinearSampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1); // Sampler for shadow filtering

// ==============================================================================
// STRUCTURES
// ==============================================================================
struct VSInput
{
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    uint4 BoneIndices : BLENDINDICES0;
    float4 BoneWeights : BLENDWEIGHT0;
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
    float3 WorldPos : TEXCOORD2;
    float3 Normal : TEXCOORD3;
    float2 TexCoord : TEXCOORD0;
    float4 LightSpacePos : TEXCOORD1; // Position from the Light's point of view
};

// ==============================================================================
// VERTEX SHADER
// ==============================================================================
VSOutput VSMain(VSInput input)
{
    VSOutput output;

    // Normalize weights just in case the model exporter left them unnormalized
    float totalWeight = input.BoneWeights.x + input.BoneWeights.y + input.BoneWeights.z + input.BoneWeights.w;
    if (totalWeight > 0.0f)
    {
        input.BoneWeights /= totalWeight;
    }

    // Skinning Calculation
    matrix skinTransform =
        BoneTransforms[input.BoneIndices.x] * input.BoneWeights.x +
        BoneTransforms[input.BoneIndices.y] * input.BoneWeights.y +
        BoneTransforms[input.BoneIndices.z] * input.BoneWeights.z +
        BoneTransforms[input.BoneIndices.w] * input.BoneWeights.w;

    // Apply Skinning to Local Position and Normal
    float4 localPos = mul(float4(input.Pos, 1.0f), skinTransform);
    float3 localNormal = mul(input.Normal, (float3x3) skinTransform);

    // Transform to Screen Space for Camera
    output.WorldPos = mul(localPos, World).xyz;
    output.Pos = mul(localPos, WorldViewProj);
    
    // Pass data to Pixel Shader
    output.Normal = normalize(mul(localNormal, (float3x3) World));
    output.TexCoord = input.TexCoord;
    
    // Transform to Light Space for Shadow Calculation
    output.LightSpacePos = mul(localPos, LightViewProj);

    return output;
}

// ==============================================================================
// SHADOW CALCULATION
// ==============================================================================
float CalculateShadow(float4 lightSpacePos)
{
    // Perspective Divide (Clip Space to NDC)
    float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    
    // Convert NDC [-1, 1] to Texture UV [0, 1]
    projCoords.x = projCoords.x * 0.5f + 0.5f;
    projCoords.y = -projCoords.y * 0.5f + 0.5f;

    // Ignore shadow if outside the map bounds
    if (projCoords.x < 0 || projCoords.x > 1 || projCoords.y < 0 || projCoords.y > 1)
        return 1.0f;

    float currentDepth = projCoords.z;
    float shadow = 0.0f;
    float bias = 0.0005f; // Reduced bias for better precision with small objects
    
    // Hardware PCF using SampleCmpLevelZero
    float2 texelSize = 1.0f / 2048.0f;
    
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float2 uv = projCoords.xy + float2(x, y) * texelSize;
            float pcfResult = ShadowMap.SampleCmpLevelZero(ShadowSampler, uv, currentDepth - bias).r;
            // SampleCmpLevelZero returns 1.0f if the depth in texture >= currentDepth - bias. (lit)
            // It returns 0.0f if the depth in texture < currentDepth - bias. (shadowed)
            // The 'shadow' variable is accumulating '1.0' for light and '0.0' for shadow.
            shadow += pcfResult;
        }
    }
    shadow /= 9.0f; // Average the samples
    
    return shadow; // Returns 0.0 for shadow, 1.0 for light
}

// ==============================================================================
// PIXEL SHADER
// ==============================================================================
float4 PSMain(VSOutput input) : SV_TARGET
{
    float4 baseColor = DiffuseMap.Sample(LinearSampler, input.TexCoord);
    
    // Optional Alpha Clipping in main pass if needed
    clip(baseColor.a - 0.05f);

    float3 norm = normalize(input.Normal);
    
    float3 lightVector = LightPos - input.WorldPos;
    float distanceToLight = length(lightVector);
    
    // Determine Light Direction (L)
    float3 L = (LightType == 0) ? normalize(-LightDir) : normalize(lightVector);

    // Base Diffuse Calculation
    float diff = max(dot(norm, L), 0.0f);
    
    // Attenuation (Falloff over distance for Point/Spot lights)
    float attenuation = 1.0f;
    if (LightType == 1 || LightType == 2)
    {
        attenuation = 1.0f - saturate(distanceToLight / LightRange);
        attenuation *= attenuation; // Quadratic falloff
    }

    // Spot Light Cone Calculation
    if (LightType == 2)
    {
        float theta = dot(L, normalize(-LightDir));
        float cosHalfAngle = cos(LightSpotAngle * 0.5f);
        
        if (theta < cosHalfAngle)
            attenuation = 0.0f;
        else
            attenuation *= smoothstep(cosHalfAngle, cosHalfAngle + 0.05f, theta);
    }

    // Shadow Mapping (Only calculate shadows if attenuation > 0)
    float shadowFactor = 1.0f;
    if (attenuation > 0.0f && LightType == 0) // Currently shadows only for Directional
    {
        shadowFactor = CalculateShadow(input.LightSpacePos);
    }

    // Final Color Combination
    float3 finalColor = baseColor.rgb * LightColor.rgb * diff * LightIntensity * attenuation * shadowFactor;
    
    // Add ambient light so shadows aren't pitch black
    float3 ambient = baseColor.rgb * 0.4f;
    finalColor += ambient;

    return float4(finalColor, baseColor.a);
}