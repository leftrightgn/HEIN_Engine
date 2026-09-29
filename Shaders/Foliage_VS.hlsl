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

    // ==========================================================
    // GLOBAL WIND WITH PERLIN NOISE SAMPLES
    // ==========================================================
    
    // Establish the base global wind direction from the inspector
    float2 baseWindDir = normalize(windDirection);
    
    // Calculate the scrolling UV coordinates for the noise texture based on world position and time
    float2 windUV = (input.instPos.xz * noiseScale) + (baseWindDir * (time * windSpeed * 0.05f));
    
    // Sample the Perlin noise texture (Returns 0.0 to 1.0)
    float noiseVal = g_noiseTexture.SampleLevel(g_samplerWrap, windUV, 0).r;
    
    // Calculate Gust Strength
    // Mapped from 0.0 to 1.0. The wind NEVER blows backward, it only varies from calm (0.0) to strong (1.0).
    float gust = noiseVal * windStrength;
    
    // Calculate Local Wind Swirl (Direction Perturbation)
    // Extract the base angle of the global wind
    float baseAngle = atan2(baseWindDir.y, baseWindDir.x);
    // Use the noise to slightly twist the wind direction left or right (Max ~45 degrees / 0.8 radians)
    // This makes the field look like fluid waves rather than a rigid sheet.
    float localAngle = baseAngle + (noiseVal * 2.0f - 1.0f) * 0.8f;
    float2 activeWindDir = float2(cos(localAngle), sin(localAngle));

    // Calculate High-Frequency Flutter
    // Small sine wave applied to the tips to simulate rapid fluttering
    float microFlutter = sin(time * windSpeed * 2.5f + input.instPos.x * 2.0f + input.instPos.z * 2.0f) * (windStrength * 0.15f);
    
    // Combine Sway
    // Ensure the total sway cannot dip below zero, preventing backward swinging
    float totalSway = max(0.0f, gust + microFlutter) * input.instScale;

    // ==========================================================
    // PROCEDURAL GRASS GENERATION
    // ==========================================================
    
    if (input.instType == 0) // Grass
    {
        float v = input.windWeight;
        float isLeft = input.position.x; // Encoded -1.0 or 1.0
        
        float3 basePos = input.instPos;
        
        // Calculate the natural leaning direction of the blade based on its randomized yaw
        float2 facingDir = float2(cos(input.instRot), sin(input.instRot));
        float3 facing = float3(facingDir.x, 0.0f, facingDir.y);
        
        float height = input.instScale * maxGrassHeight;
 
        // A. Setup the Base Bezier Control Points for the natural shape
        float3 p0 = basePos; // Root
        float3 p1 = basePos + float3(0.0f, height * bend, 0.0f); // Lower spine
        float3 p3 = basePos + float3(facing.x * tilt, height, facing.z * tilt); // Tip
        
        // Push P2 backward relative to the facing direction to create a natural, drooping arch
        float3 p2 = p3 - float3(facing.x * tilt * bend, height * bend, facing.z * tilt * bend);

        // B. Apply the active, noise-driven wind direction to the control points
        // The root (P0) does not move. The spine bends progressively more towards the tip.
        p1.x += activeWindDir.x * totalSway * 0.2f;
        p1.z += activeWindDir.y * totalSway * 0.2f;

        p2.x += activeWindDir.x * totalSway * 0.6f;
        p2.z += activeWindDir.y * totalSway * 0.6f;

        p3.x += activeWindDir.x * totalSway;
        p3.z += activeWindDir.y * totalSway;
        p3.y -= abs(totalSway) * 0.25f; // Push the tip down slightly to simulate air pressure

        // C. Evaluate the final curve position and tangent
        float3 curvePos = EvaluateCubicBezier(p0, p1, p2, p3, v);
        float3 tangent = EvaluateCubicBezierDerivative(p0, p1, p2, p3, v);
        
        // D. Widen the blade outward from the spine
        float3 ortho = float3(-facing.z, 0.0f, facing.x);
        float currentWidth = lerp(maxGrassWidth, 0.0f, v) * input.instScale;
        
        worldPos = curvePos + (ortho * isLeft * currentWidth * 0.5f);
        
        // E. Calculate Normals (Rounded cylinder illusion)
        float3 faceNormal = normalize(cross(tangent, ortho));
        float curveAmount = 0.9f;
        float3 normalLeft = normalize(faceNormal - ortho * curveAmount);
        float3 normalRight = normalize(faceNormal + ortho * curveAmount);
        
        float blendFactor = input.position.x * 0.5f * 0.5f;
        finalNormal = normalize(lerp(normalLeft, normalRight, blendFactor));
        
        // F. View Space Thickening (Anti-Aliasing technique)
        float3 viewDir = normalize(cameraPosition.xyz - worldPos);
        float viewDotNormal = abs(dot(viewDir, faceNormal));
        float thickenFactor = smoothstep(0.8f, 1.0f, 1.0f - viewDotNormal);
        worldPos += ortho * isLeft * thickenFactor * 0.05f;
    }
    else // Flowers & Trees 
    {
        float cosR = cos(input.instRot);
        float sinR = sin(input.instRot);
        
        // Rotate the static mesh around the Y axis
        float3 rotatedPos;
        rotatedPos.x = input.position.x * cosR - input.position.z * sinR;
        rotatedPos.y = input.position.y;
        rotatedPos.z = input.position.x * sinR + input.position.z * cosR;

        float3 scaledPos = rotatedPos * input.instScale;
        worldPos = scaledPos + input.instPos;

        // Apply local wind direction to flat mesh vertices based on vertex height/weight
        float meshSway = totalSway * input.windWeight;
        worldPos.x += activeWindDir.x * meshSway;
        worldPos.z += activeWindDir.y * meshSway;
        worldPos.y -= abs(meshSway) * 0.25f;

        float3 rotatedNormal;
        rotatedNormal.x = input.normal.x * cosR - input.normal.z * sinR;
        rotatedNormal.y = input.normal.y;
        rotatedNormal.z = input.normal.x * sinR + input.normal.z * cosR;
        finalNormal = normalize(rotatedNormal);
    }

    // ==========================================================
    // STANDARD MATRICES & OUTPUT
    // ==========================================================
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