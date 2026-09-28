#include "Foliage_Common.hlsli"

Texture2D g_noiseTexture : register(t1);
SamplerState g_samplerWrap : register(s0);

// Evaluates a Cubic Bezier curve at parameter t (0 to 1)
float3 EvaluateCubicBezier(float3 p0, float3 p1, float3 p2, float3 p3, float t)
{
    float u = 1.0 - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;
    
    float3 p = uuu * p0; // (1-t)^3 * P0
    p += 3.0 * uu * t * p1; // 3(1-t)^2 * t * P1
    p += 3.0 * u * tt * p2; // 3(1-t) * t^2 * P2
    p += ttt * p3; // t^3 * P3
    return p;
}

// Evaluates the derivative (tangent) of a Cubic Bezier curve at parameter t
float3 EvaluateCubicBezierDerivative(float3 p0, float3 p1, float3 p2, float3 p3, float t)
{
    float u = 1.0 - t;
    float3 d = 3.0 * u * u * (p1 - p0);
    d += 6.0 * u * t * (p2 - p1);
    d += 3.0 * t * t * (p3 - p2);
    return normalize(d);
}

FoliagePixelInput main(FoliageVertexInput input)
{
    FoliagePixelInput output;
    float3 worldPos;
    float3 finalNormal;

    // Wind computation shared across all types
    float2 windDir = normalize(windDirection);
    float2 windUV = (input.instPos.xz * noiseScale) + (windDir * (time * windSpeed * 0.05f));
    float noiseVal = g_noiseTexture.SampleLevel(g_samplerWrap, windUV, 0).r;
    float gust = (noiseVal * 2.0f - 1.0f) * windStrength;
    float microFlutter = sin(time * windSpeed * 2.5f + input.instPos.x * 2.0f + input.instPos.z * 2.0f) * (windStrength * 0.2f);
    float totalSway = (gust + microFlutter) * input.instScale;

    // Grass Procedural Generation (Bezier Curve)
    if (input.instType == 0)
    {
        float v = input.windWeight;
        float isLeft = input.position.x; // Encoded -1.0 or 1.0
        
        float3 basePos = input.instPos;
        float2 facingDir = float2(cos(input.instRot), sin(input.instRot));
        float3 facing = float3(facingDir.x, 0.0f, facingDir.y);
        
        float MAX_GRASS_HEIGHT = 3.0f;
        float MAX_GRASS_WIDTH = 0.9f;
        float height = input.instScale * MAX_GRASS_HEIGHT;
        float tilt = 8.0f;
        float bend = 0.45f;
        
        // Determine Control Points
        float3 p0 = basePos;
        float3 p3 = basePos + float3(facing.x * tilt, height, facing.z * tilt);
        float3 p1 = basePos + float3(0, height * bend, 0);
        float3 p2 = p3 - float3(0, height * bend, 0);

        // Displace the control points (mostly the tip) based on wind
        p3.x += windDir.x * totalSway;
        p3.z += windDir.y * totalSway;
        p3.y -= abs(totalSway) * 0.25f;
        
        p2.x += windDir.x * totalSway * 0.6f;
        p2.z += windDir.y * totalSway * 0.6f;

        // Evaluate Curve
        float3 curvePos = EvaluateCubicBezier(p0, p1, p2, p3, v);
        float3 tangent = EvaluateCubicBezierDerivative(p0, p1, p2, p3, v);
        
        // Widen Blade Orthogonally
        float3 ortho = float3(-facing.z, 0.0f, facing.x);
        float currentWidth = lerp(MAX_GRASS_WIDTH, 0.0f, v) * input.instScale;
        
        worldPos = curvePos + (ortho * isLeft * currentWidth * 0.5f);
        
        // Calculate Normals
        float3 faceNormal = normalize(cross(tangent, ortho));
        
        float curveAmount = 0.9f;
        float3 normalLeft = normalize(faceNormal - ortho * curveAmount);
        float3 normalRight = normalize(faceNormal + ortho * curveAmount);
        
        float blendFactor = input.position.x * 0.5f * 0.5f;
        
        finalNormal = normalize(lerp(normalLeft, normalRight, blendFactor)); // Fake rounded cylinder
        
        // View Space Thickening
        float3 viewDir = normalize(cameraPosition.xyz - worldPos);
        float viewDotNormal = abs(dot(viewDir, faceNormal));
        float thickenFactor = smoothstep(0.8f, 1.0f, 1.0f - viewDotNormal);
        worldPos += ortho * isLeft * thickenFactor * 0.05f;
    }
    else // Flowers & Trees 
    {
        float cosR = cos(input.instRot);
        float sinR = sin(input.instRot);
        
        float3 rotatedPos;
        rotatedPos.x = input.position.x * cosR - input.position.z * sinR;
        rotatedPos.y = input.position.y;
        rotatedPos.z = input.position.x * sinR + input.position.z * cosR;

        float3 scaledPos = rotatedPos * input.instScale;
        worldPos = scaledPos + input.instPos;

        totalSway *= input.windWeight;
        worldPos.x += windDir.x * totalSway;
        worldPos.z += windDir.y * totalSway;
        worldPos.y -= abs(totalSway) * 0.25f;

        float3 rotatedNormal;
        rotatedNormal.x = input.normal.x * cosR - input.normal.z * sinR;
        rotatedNormal.y = input.normal.y;
        rotatedNormal.z = input.normal.x * sinR + input.normal.z * cosR;
        finalNormal = normalize(rotatedNormal);
    }

    float4 viewPos = mul(float4(worldPos, 1.0f), viewMatrix);
    output.position = mul(viewPos, projectionMatrix);
    
    output.normal = finalNormal;
    output.texCoord = input.texCoord;
    output.worldPos = worldPos;
    output.windWeight = input.windWeight;
    output.foliageType = input.instType;
    output.vertexColor = input.color;
    output.shadowPos = mul(float4(worldPos, 1.0f), lightViewProj);

    return output;
}