Texture2D shaderTexture : register(t0);
Texture2D normalMapTexture : register(t1);
SamplerState SampleType : register(s0);

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 Normal : NORMAL;
    float2 Tex : TEXCOORD0;
};



cbuffer WaterSettings : register(b1)
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
cbuffer LightBuffer : register(b2)
{
    float3 lightDir;
    float time;
    float3 camPos;
    float padding2;
};
float4 main(PS_INPUT input) : SV_TARGET
{
    // Base Color of the Ocean
    //float3 waterColor = float3(0.0f, 0.3f, 0.7f);
    
    float4 textureColor = shaderTexture.Sample(SampleType, input.Tex);
    float3 baseColor = waterColor * (textureColor.rgb + 0.5f);
    
    
    // Normal Mapping Logic
    float3 vertexNormal = normalize(input.Normal);
    
    // Sample the Normal Map Twice
    float2 uv1 = input.Tex * 2.0f + float2(time * 0.03f, time * 0.01f);
    float2 uv2 = input.Tex * 2.5f - float2(time * 0.01f, time * 0.03f);
    
    // Sampl NormalTexture
    float3 normalSample1 = normalMapTexture.Sample(SampleType, uv1).rgb * 2.0f - 1.0f;
    float3 normalSample2 = normalMapTexture.Sample(SampleType, uv2).rgb * 2.0f - 1.0f;
    
    // Combine the NormalMap with vertex normal
    float3 combineNormal = normalize(vertexNormal + (normalSample1 + normalSample2) * normalBlend);
    
    float3 light = normalize(-lightDir);
    float3 viewDir = normalize(camPos - input.WorldPos);
    
    // Ambient light
    float3 ambient = baseColor * 0.2f;
    
    // Diffuse the sunlight
    float diffIntensity = saturate(dot(combineNormal, light));
    float3 diffuse = baseColor * diffIntensity;
    
    // Specular
    float3 reflectDir = reflect(-light, combineNormal);
    float specIntensity = pow(saturate(dot(viewDir, reflectDir)), specularPower);
    float3 specular = float3(1.0f, 1.0f, 1.0f) * specIntensity * 0.3f;
    
    // Fresnel Effect
    float fresnel = pow(1.0f - saturate(dot(combineNormal, viewDir)), fresnelPower);
    
    float3 skyReflection = specular + float3(0.4f, 0.5f, 0.7f) ;
    // Combine all the light
    float3 finalColor = lerp(diffuse + ambient, skyReflection, fresnel);
    
    return float4(finalColor, 0.8f);

}