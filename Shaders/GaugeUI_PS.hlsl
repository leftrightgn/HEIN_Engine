Texture2D SpriteTexture     : register(t0);
Texture2D BackgroundTexture : register(t1);
SamplerState SpriteSampler  : register(s0);

cbuffer CB_GaugeUI : register(b0)
{
    float4 fillColor;
    float4 trailColor;
    float4 bgColor;
    float4 borderColor;
    float  fillRatio;
    float  trailRatio;
    float  fillStart;
    float  fillEnd;
    float2 barSizePixels;
    float  fillRightToLeft;
    float  hasFillTexture;
    float  hasBgTexture;
    float  clipTexture;
    float  fillTexMatchesFrame;
    float  useProceduralFrame;
    float  bevelIntensity;
    float  meniscusGlow;
    float  padding0;
    float  padding1;
};

struct PSInput
{
    float4 color    : COLOR0;
    float2 texCoord : TEXCOORD0;
    float4 position : SV_POSITION;
};

float4 main(PSInput input) : SV_Target
{
    float2 uv = input.texCoord;

    float span = max(0.0001f, fillEnd - fillStart);
    float u = saturate((uv.x - fillStart) / span);
    if (fillRightToLeft > 0.5f)
    {
        u = 1.0f - u;
    }

    // Cylindrical vertical bevel and specular ridge across bar height
    float vCentered = abs(uv.y * 2.0f - 1.0f);
    float verticalBevel = lerp(1.0f, 1.0f - 0.48f * pow(vCentered, 1.75f), bevelIntensity);
    float specBand = exp(-pow((uv.y - 0.28f) / 0.14f, 2.0f)) * 0.26f * bevelIntensity;

    // Base recessed trough (textured or procedural inner channel)
    float4 resultColor;
    if (hasBgTexture > 0.5f)
    {
        float4 bgSample = BackgroundTexture.Sample(SpriteSampler, uv);
        resultColor = bgSample * bgColor;
    }
    else
    {
        resultColor = bgColor;
        resultColor.rgb *= (0.55f + 0.45f * (1.0f - vCentered));
    }

    bool insideActiveSpan = (uv.x >= fillStart && uv.x <= fillEnd);
    if (insideActiveSpan)
    {
        if (u <= fillRatio && fillRatio > 0.0001f)
        {
            float4 texSample = float4(1.0f, 1.0f, 1.0f, 1.0f);
            if (hasFillTexture > 0.5f)
            {
                float2 fillUV = uv;
                if (clipTexture > 0.5f)
                {
                    float fillU = (fillTexMatchesFrame > 0.5f) ? uv.x : u;
                    fillUV = float2(fillU, uv.y);
                }
                else
                {
                    float stretchedU = u / max(0.0001f, fillRatio);
                    float fillU = (fillTexMatchesFrame > 0.5f) ? lerp(fillStart, fillEnd, stretchedU) : stretchedU;
                    fillUV = float2(fillU, uv.y);
                }
                texSample = SpriteTexture.Sample(SpriteSampler, fillUV);
            }

            float distToEdgePixels = abs(fillRatio - u) * (barSizePixels.x * span);
            float edgeGlow = exp(-distToEdgePixels * 0.45f) * meniscusGlow;

            float3 litFill = texSample.rgb * fillColor.rgb * verticalBevel + specBand + edgeGlow * float3(1.0f, 0.92f, 0.78f);
            float fillAlpha = saturate(texSample.a * fillColor.a);

            resultColor.rgb = lerp(resultColor.rgb, saturate(litFill), fillAlpha);
            resultColor.a = max(resultColor.a, fillAlpha);
        }
        else if (u <= trailRatio && trailRatio > fillRatio)
        {
            // Delayed Elden Ring gold/amber damage trail
            float shapeMask = 1.0f;
            if (hasFillTexture > 0.5f)
            {
                float maskU = (fillTexMatchesFrame > 0.5f) ? uv.x : u;
                shapeMask = SpriteTexture.Sample(SpriteSampler, float2(maskU, uv.y)).a;
            }

            float trailProgress = saturate((u - fillRatio) / max(0.0001f, trailRatio - fillRatio));
            float trailEdge = exp(-pow((1.0f - trailProgress) * 3.0f, 2.0f)) * 0.25f;
            float3 litTrail = trailColor.rgb * verticalBevel + specBand * 0.65f + trailEdge;
            float trailAlpha = saturate(shapeMask * trailColor.a);

            resultColor.rgb = lerp(resultColor.rgb, saturate(litTrail), trailAlpha);
            resultColor.a = max(resultColor.a, trailAlpha);
        }
    }

    // Procedural Elden Ring gilded metallic frame and end-cap ticks
    if (useProceduralFrame > 0.5f)
    {
        float2 px = uv * barSizePixels;
        float distLeft   = px.x;
        float distRight  = barSizePixels.x - px.x;
        float distTop    = px.y;
        float distBottom = barSizePixels.y - px.y;

        float minBorderY = min(distTop, distBottom);
        float minBorderX = min(distLeft, distRight);

        if (minBorderY < 1.6f || minBorderX < 2.2f)
        {
            float3 goldHighlight = borderColor.rgb * (0.85f + 0.35f * (1.0f - uv.y));
            resultColor = float4(saturate(goldHighlight), borderColor.a);
        }
        else if (minBorderY < 2.8f || minBorderX < 3.4f)
        {
            resultColor.rgb *= 0.25f;
        }
    }

    return saturate(resultColor);
}
