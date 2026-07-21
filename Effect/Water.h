#pragma once
#include "pch.h" 
#include "Framework/GameContext.h"
class Water
{
private:
    struct WaterConstantBuffer
    {
        float time;
        float padding[3];
        DirectX::SimpleMath::Matrix world;
        DirectX::SimpleMath::Matrix view;
        DirectX::SimpleMath::Matrix projection;
    };

    struct LightBuffer
    {
        DirectX::SimpleMath::Vector3 lightDir;
        float time;
        DirectX::SimpleMath::Vector3 camPos;
        float padding2;
    };

    struct WaterSettingsBuffer
    {
        DirectX::SimpleMath::Vector3 waterColor{};
        float fresnelPower{};
        float specularPower{};
        float waveSpeed{};
        float normalBlend{};
        float padding3{};
        float uvScale{};
        float offset{};
        float multiplier{};
        float padding4{};
    };

    // Standard Vertex Structure
    struct VertexType
    {
        DirectX::SimpleMath::Vector3 position;
        DirectX::SimpleMath::Vector3 normal;
        DirectX::SimpleMath::Vector2 texture;
    };

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_lightBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_watersettingsBuffer;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_layout;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_normalMapTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_noiseMapTexture;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;

    int m_indexCount;
    float m_totalTime;

    DirectX::SimpleMath::Vector3 m_waterColor;
    float m_fresnelPower;
    float m_specularPower;
    float m_waveSpeed;
    float m_normalBlend;

    float m_spacing;
    float m_uvScale;
    float m_offset;
    float m_multiplier;

public:
    Water();
    ~Water();

    void Initialize(GameContext& gameContext, const wchar_t* textureFilename, const wchar_t* normalMapFilename, const wchar_t* noiseMapFilename);
    void Update(float deltaTime);
    void Draw(GameContext& gameContext,
        const DirectX::SimpleMath::Matrix& view,
        const DirectX::SimpleMath::Matrix& projection,
        const DirectX::SimpleMath::Vector3& cameraPos);

    //void DrawDebugUI();
};