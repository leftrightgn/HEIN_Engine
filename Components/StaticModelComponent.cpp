#include "pch.h"
#include "Framework/GameContext.h"
#include "StaticModelComponent.h"
#include "Entities/Actor.h"
#include <Effects.h>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>
#include <Windows.h>
#include "DebugingTools/EditorUtils.h"
#include <string>
#include <vector>
#include <filesystem>
#include <d3dcompiler.h>
#include "Effect/ReadData.h"
#include "TransformComponent.h"
#include "Common/ShadowSystem.h"
#include "Common/ShaderStructures.h"
#include "LightComponent.h"
#include "FogComponent.h"
#include "Entities/ActorManager.h"
#include "Camera/CameraController.h"

namespace HEIN
{
    bool TryLoadTexture(
        ID3D11Device* device,
        const std::wstring& dir,
        const wchar_t* name,
        ID3D11ShaderResourceView** textureView
    )
    {
        if (!device || !name || !textureView) return false;
        *textureView = nullptr;

        std::wstring rawName = name;
        std::vector<std::wstring> candidates;

        // Direct path in dir and raw path
        if (!dir.empty())
        {
            candidates.push_back(dir + rawName);
        }
        candidates.push_back(rawName);

        // Extract potential subnames by splitting on '_'
    
        size_t pos = 0;
        while ((pos = rawName.find(L'_', pos)) != std::wstring::npos)
        {
            pos++;
            if (pos < rawName.length())
            {
                std::wstring sub = rawName.substr(pos);
                if (!sub.empty() && sub.find(L'.') != std::wstring::npos)
                {
                    if (!dir.empty()) candidates.push_back(dir + sub);
                    candidates.push_back(sub);
                }
            }
        }

        // xtract just the filename if it was a path with '/' or '\\'
        std::filesystem::path rawP(rawName);
        std::wstring filenameOnly = rawP.filename().wstring();
        if (!filenameOnly.empty())
        {
            if (!dir.empty()) candidates.push_back(dir + filenameOnly);
            candidates.push_back(filenameOnly);
        }

        // Try loading candidates
        for (const auto& cand : candidates)
        {
            if (std::filesystem::exists(cand))
            {
                HRESULT hr = DirectX::CreateDDSTextureFromFile(device, cand.c_str(), nullptr, textureView);
                if (SUCCEEDED(hr) && *textureView) return true;

                hr = DirectX::CreateWICTextureFromFile(device, cand.c_str(), nullptr, textureView);
                if (SUCCEEDED(hr) && *textureView) return true;
            }
        }

        // Fuzzy search in directory
        if (!dir.empty() && std::filesystem::exists(dir) && std::filesystem::is_directory(dir))
        {
            std::wstring stem = rawP.stem().wstring();
            size_t lastUnder = stem.rfind(L'_');
            if (lastUnder != std::wstring::npos)
            {
                stem = stem.substr(lastUnder + 1);
            }
            std::wstring lowerStem = stem;
            for (auto& c : lowerStem) c = towlower(c);

            try
            {
                for (const auto& entry : std::filesystem::directory_iterator(dir))
                {
                    if (entry.is_regular_file())
                    {
                        std::wstring entryExt = entry.path().extension().wstring();
                        for (auto& c : entryExt) c = towlower(c);

                        if (entryExt == L".dds" || entryExt == L".png" || entryExt == L".jpg" || entryExt == L".tga")
                        {
                            std::wstring entryStem = entry.path().stem().wstring();
                            for (auto& c : entryStem) c = towlower(c);

                            if (!lowerStem.empty() && (entryStem.find(lowerStem) != std::wstring::npos || lowerStem.find(entryStem) != std::wstring::npos))
                            {
                                std::wstring matchPath = entry.path().wstring();
                                HRESULT hr = DirectX::CreateDDSTextureFromFile(device, matchPath.c_str(), nullptr, textureView);
                                if (SUCCEEDED(hr) && *textureView) return true;

                                hr = DirectX::CreateWICTextureFromFile(device, matchPath.c_str(), nullptr, textureView);
                                if (SUCCEEDED(hr) && *textureView) return true;
                            }
                        }
                    }
                }
            }
            catch (...) {}
        }

        // Fallback 1x1 white texture so rendering never crashes
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        uint32_t whitePixel = 0xFFFFFFFF;
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = &whitePixel;
        initData.SysMemPitch = sizeof(uint32_t);

        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(device->CreateTexture2D(&desc, &initData, tex.GetAddressOf())))
        {
            device->CreateShaderResourceView(tex.Get(), nullptr, textureView);
            return (*textureView != nullptr);
        }

        return false;
    }

    class SmartEffectFactory : public DirectX::EffectFactory
    {
    private:
        ID3D11Device* m_pDevice;
        std::wstring m_dir;

    public:
        SmartEffectFactory(ID3D11Device* device)
            : DirectX::EffectFactory(device), m_pDevice(device)
        {
        }

        void SetDirectory(const wchar_t* path)
        {
            m_dir = path ? path : L"";
            DirectX::EffectFactory::SetDirectory(path);
        }

        void CreateTexture(
            const wchar_t* name,
            ID3D11DeviceContext* deviceContext,
            ID3D11ShaderResourceView** textureView
        ) override
        {
            std::wstring wname = name ? name : L"";
            std::wstring cand1 = m_dir + wname;
            std::wstring cand2 = wname;
            
            bool exists = false;
            if (!wname.empty())
            {
                if (std::filesystem::exists(cand1) || std::filesystem::exists(cand2) ||
                    std::filesystem::exists(cand1 + L".dds") || std::filesystem::exists(cand2 + L".dds"))
                {
                    exists = true;
                }
            }

            if (exists)
            {
                try
                {
                    DirectX::EffectFactory::CreateTexture(name, deviceContext, textureView);
                    if (textureView && *textureView) return;
                }
                catch (...) {}
            }

            TryLoadTexture(m_pDevice, m_dir, name, textureView);
        }
    };
}

std::shared_ptr<DirectX::EffectFactory> HEIN::StaticModelComponent::s_fxFactory = nullptr;
std::unordered_map<std::wstring, std::weak_ptr<DirectX::Model>> HEIN::StaticModelComponent::s_modelCache;

HEIN::StaticModelComponent::StaticModelComponent(Actor* owner)
	: IComponent(owner) 
{
}

void HEIN::StaticModelComponent::InitializeShaders(ID3D11Device* device)
{
    if (m_vertexShader && m_pixelShader && m_cbMatrices && m_cbLighting) return;

    bool loadedCso = false;
    std::vector<uint8_t> vsData, psData;
    try
    {
        vsData = DX::ReadData(L"Resources/Shaders/StaticModel_VS.cso");
        psData = DX::ReadData(L"Resources/Shaders/StaticModel_PS.cso");
        loadedCso = true;
    }
    catch (...)
    {
        try
        {
            vsData = DX::ReadData(L"StaticModel_VS.cso");
            psData = DX::ReadData(L"StaticModel_PS.cso");
            loadedCso = true;
        }
        catch (...)
        {
            loadedCso = false;
        }
    }

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    const void* vsBytecode = nullptr;
    size_t vsBytecodeSize = 0;
    const void* psBytecode = nullptr;
    size_t psBytecodeSize = 0;

    if (loadedCso)
    {
        vsBytecode = vsData.data();
        vsBytecodeSize = vsData.size();
        psBytecode = psData.data();
        psBytecodeSize = psData.size();
    }
    else
    {
        HRESULT hr = D3DCompileFromFile(L"../External/Engine/Shaders/StaticModel_VS.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
        if (FAILED(hr))
        {
            hr = D3DCompileFromFile(L"External/Engine/Shaders/StaticModel_VS.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
        }
        if (FAILED(hr))
        {
            if (errorBlob)
            {
                OutputDebugStringA((char*)errorBlob->GetBufferPointer());
                FILE* f;
                if (fopen_s(&f, "ShaderError_StaticVS.txt", "w") == 0)
                {
                    fprintf(f, "%s", (char*)errorBlob->GetBufferPointer());
                    fclose(f);
                }
            }
            return;
        }

        hr = D3DCompileFromFile(L"../External/Engine/Shaders/StaticModel_PS.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
        if (FAILED(hr))
        {
            hr = D3DCompileFromFile(L"External/Engine/Shaders/StaticModel_PS.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
        }
        if (FAILED(hr))
        {
            if (errorBlob)
            {
                OutputDebugStringA((char*)errorBlob->GetBufferPointer());
                FILE* f;
                if (fopen_s(&f, "ShaderError_StaticPS.txt", "w") == 0)
                {
                    fprintf(f, "%s", (char*)errorBlob->GetBufferPointer());
                    fclose(f);
                }
            }
            return;
        }

        vsBytecode = vsBlob->GetBufferPointer();
        vsBytecodeSize = vsBlob->GetBufferSize();
        psBytecode = psBlob->GetBufferPointer();
        psBytecodeSize = psBlob->GetBufferSize();
    }

    m_vsBytecode.assign((const uint8_t*)vsBytecode, (const uint8_t*)vsBytecode + vsBytecodeSize);

    device->CreateVertexShader(vsBytecode, vsBytecodeSize, nullptr, m_vertexShader.ReleaseAndGetAddressOf());
    device->CreatePixelShader(psBytecode, psBytecodeSize, nullptr, m_pixelShader.ReleaseAndGetAddressOf());

    // Create Constant Buffers
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    cbDesc.ByteWidth = sizeof(CB_StaticMatrices);
    device->CreateBuffer(&cbDesc, nullptr, m_cbMatrices.ReleaseAndGetAddressOf());

    cbDesc.ByteWidth = sizeof(CB_Lighting);
    device->CreateBuffer(&cbDesc, nullptr, m_cbLighting.ReleaseAndGetAddressOf());

    // Create Sampler State (Linear Wrap)
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    device->CreateSamplerState(&sampDesc, m_samplerState.ReleaseAndGetAddressOf());
}

void HEIN::StaticModelComponent::Initialize(
    GameContext& gameContext, 
    const wchar_t* modelPath,
    const wchar_t* textureDir
)
{
    m_modelPath = modelPath ? modelPath : L"";
    m_textureDir = textureDir ? textureDir : L"";
    
    // Auto-detect texture directory from model path if not specified
    if (m_textureDir.empty() && !m_modelPath.empty())
    {
        std::filesystem::path p(m_modelPath);
        std::wstring parent = p.parent_path().wstring();
        if (!parent.empty())
        {
            m_textureDir = parent + L"/";
        }
    }

    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();

    std::wstring key = m_modelPath;
    std::shared_ptr<DirectX::Model> cachedModel = s_modelCache[key].lock();

    if (cachedModel != nullptr)
    {
        m_model = cachedModel;
    }
    else
    {
        try
        {
            std::filesystem::path p(m_modelPath);
            std::wstring ext = p.extension().wstring();
            for (auto& c : ext) c = towlower(c);

            if (ext == L".cmo")
            {
                // Use EffectFactory (not DGSLEffectFactory) so the model gets BasicEffect
                // which supports IEffectFog for fog blending. DGSLEffect does not support IEffectFog.
                SmartEffectFactory cmoFactory(device);
                if (!m_textureDir.empty())
                {
                    cmoFactory.SetDirectory(m_textureDir.c_str());
                }
                else
                {
                    cmoFactory.SetDirectory(nullptr);
                }

                m_model = DirectX::Model::CreateFromCMO(
                    device,
                    m_modelPath.c_str(),
                    cmoFactory,
                    static_cast<DirectX::ModelLoaderFlags>(
                        DirectX::ModelLoader_CounterClockwise |
                        DirectX::ModelLoader_IncludeBones
                    )
                );
            }
            else
            {
                SmartEffectFactory sdkMeshFactory(device);
                if (!m_textureDir.empty())
                {
                    sdkMeshFactory.SetDirectory(m_textureDir.c_str());
                }
                else
                {
                    sdkMeshFactory.SetDirectory(nullptr);
                }

                m_model = DirectX::Model::CreateFromSDKMESH(
                    device,
                    m_modelPath.c_str(),
                    sdkMeshFactory,
                    static_cast<DirectX::ModelLoaderFlags>(
                        DirectX::ModelLoader_Clockwise |
                        DirectX::ModelLoader_IncludeBones
                    )
                );
            }
            s_modelCache[key] = m_model;
            m_lastError = "";
        }
        catch (const std::exception& e)
        {
            m_model = nullptr;
            m_lastError = e.what();
        }
    }

    if (m_model == nullptr)
    {
        return;
    }

    if (!m_model->bones.empty())
    {
        m_drawBones = DirectX::ModelBone::MakeArray(m_model->bones.size());
        m_model->CopyAbsoluteBoneTransformsTo(m_model->bones.size(), m_drawBones.get());
    }
    else
    {
        m_drawBones.reset();
    }

    InitializeShaders(device);
}

void HEIN::StaticModelComponent::Update(float deltaTime)
{
    m_ghostTime += deltaTime;
}

void HEIN::StaticModelComponent::Draw(
    GameContext& gameContext,
    const DirectX::SimpleMath::Matrix& world,
    const DirectX::SimpleMath::Matrix& view,
    const DirectX::SimpleMath::Matrix& proj
)
{
    if (!m_isVisible || !m_model) return;

    bool isPlayer = (m_owner->GetActorType() == HEIN::ActorType::Player);

    if (!isPlayer)
    {
        DirectX::SimpleMath::Matrix cullingView = view;
        DirectX::SimpleMath::Matrix cullingProj = proj;

        // Force culling to use the main camera to match Terrain and Foliage systems
        if (gameContext.mainCamera != nullptr)
        {
            cullingView = gameContext.mainCamera->GetView();

            D3D11_VIEWPORT vp = gameContext.deviceResources.GetScreenViewport();
            float aspect = (vp.Height > 0.0f) ? (vp.Width / vp.Height) : (1280.0f / 720.0f);
            float fov = gameContext.mainCamera->GetFov();
            if (fov <= 0.0f) fov = DirectX::XM_PI / 4.0f;

            cullingProj = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(
                fov, aspect, 0.1f, 5000.0f
            );
        }

        // Build the World-Space Camera Frustum
        DirectX::BoundingFrustum worldFrustum(cullingProj, true); // true for Right-Handed
        DirectX::SimpleMath::Matrix camWorld;
        if (std::abs(cullingView.Determinant()) < 1e-6f)
            camWorld = DirectX::SimpleMath::Matrix::Identity;
        else
            camWorld = cullingView.Invert();

        worldFrustum.Transform(worldFrustum, camWorld);

        // Normalize orientation quaternion to guarantee numerical stability
        DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&worldFrustum.Orientation);
        q = DirectX::XMQuaternionNormalize(q);
        DirectX::XMStoreFloat4(&worldFrustum.Orientation, q);

        // Combine all sub-meshes into one master bounding box
        DirectX::BoundingBox masterBox;
        if (!m_model->meshes.empty())
        {
            masterBox = m_model->meshes[0]->boundingBox;
            for (size_t i = 1; i < m_model->meshes.size(); i++)
            {
                DirectX::BoundingBox::CreateMerged(masterBox, masterBox, m_model->meshes[i]->boundingBox);
            }
        }

        // SDKMESH files often default to 1x1x1 bounding boxes (Extents = 0.5f).
        // If the master box is suspiciously small (<= 1.0f), force the massive fallback.
        if (masterBox.Extents.x <= 1.0f && masterBox.Extents.y <= 1.0f && masterBox.Extents.z <= 1.0f)
        {
            masterBox.Extents = DirectX::SimpleMath::Vector3(100.0f, 100.0f, 100.0f);
        }
        else
        {
            // Pad legitimate bounding boxes by 50% just in case of slight exporter inaccuracies
            masterBox.Extents.x *= 1.5f;
            masterBox.Extents.y *= 1.5f;
            masterBox.Extents.z *= 1.5f;
        }

        DirectX::BoundingOrientedBox worldOBB;
        DirectX::BoundingOrientedBox::CreateFromBoundingBox(worldOBB, masterBox);
        worldOBB.Transform(worldOBB, world);

        DirectX::XMVECTOR qBox = DirectX::XMLoadFloat4(&worldOBB.Orientation);
        qBox = DirectX::XMQuaternionNormalize(qBox);
        DirectX::XMStoreFloat4(&worldOBB.Orientation, qBox);

        // Abort drawing if the entire merged model is completely off-screen
        if (!worldFrustum.Intersects(worldOBB))
        {
            return;
        }
    }

    ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
    DirectX::DX11::CommonStates& states = gameContext.commonStates;

    if (!m_vertexShader || !m_pixelShader || !m_cbMatrices || !m_cbLighting)
    {
        InitializeShaders(device);
    }

    if (!m_vertexShader || !m_pixelShader)
    {
        // Fallback to DirectXTK standard drawing if custom shaders failed to compile
        if (!m_model->bones.empty() && m_drawBones)
        {
            m_model->Draw(context, states, m_model->bones.size(), m_drawBones.get(), world, view, proj);
        }
        else
        {
            m_model->Draw(context, states, world, view, proj);
        }
        return;
    }

    // Find active LightComponent and FogComponent
    HEIN::LightComponent* activeLight = nullptr;
    HEIN::FogComponent* fogComp = nullptr;
    for (auto& pair : gameContext.actorManager->GetAllActors())
    {
        if (!activeLight) activeLight = pair.second->GetComponent<HEIN::LightComponent>();
        if (!fogComp) fogComp = pair.second->GetComponent<HEIN::FogComponent>();
        if (activeLight && fogComp) break;
    }

    // Update Lighting & Fog Constant Buffer (b1)
    if (m_cbLighting)
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(context->Map(m_cbLighting.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            CB_Lighting* cbLight = (CB_Lighting*)mapped.pData;
            if (activeLight)
            {
                HEIN::TransformComponent* lightTrans = activeLight->GetOwner()->GetComponent<HEIN::TransformComponent>();
                cbLight->LightPos = lightTrans->GetPosition();
                cbLight->LightDir = lightTrans->GetWorldMatrix().Forward();
                if (cbLight->LightDir.LengthSquared() < 0.0001f) cbLight->LightDir = DirectX::SimpleMath::Vector3(0, -1, 0);

                cbLight->LightType = static_cast<int>(activeLight->GetLightType());
                cbLight->LightColor = activeLight->GetColor();
                cbLight->LightIntensity = activeLight->GetIntensity();
                cbLight->LightRange = activeLight->GetRange();
                cbLight->LightSpotAngle = DirectX::XMConvertToRadians(activeLight->GetSpotAngle());
            }
            else
            {
                cbLight->LightPos = DirectX::SimpleMath::Vector3(0, 10, 0);
                cbLight->LightDir = DirectX::SimpleMath::Vector3(0, -1, 1);
                cbLight->LightType = 0; // Directional
                cbLight->LightColor = DirectX::SimpleMath::Color(1, 1, 1, 1);
                cbLight->LightIntensity = 1.0f;
                cbLight->LightRange = 100.0f;
                cbLight->LightSpotAngle = 0.785f;
            }

            if (fogComp)
            {
                cbLight->FogStart = fogComp->m_fogStart;
                cbLight->FogEnd = fogComp->m_fogEnd;
                cbLight->FogColor = fogComp->m_fogColor;
            }
            else
            {
                cbLight->FogStart = 100000.0f;
                cbLight->FogEnd = 200000.0f;
                cbLight->FogColor = DirectX::SimpleMath::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            }

            DirectX::SimpleMath::Matrix viewInv = view.Invert();
            cbLight->CameraPos = viewInv.Translation();
            cbLight->FogPadding = 0.0f;

            // Ghost effect parameters
            bool isGhost = m_isGhostMode || (m_owner != nullptr && m_owner->GetActorType() == HEIN::ActorType::Ghost);
            cbLight->IsGhost = isGhost ? 1 : 0;
            cbLight->GhostColor = m_ghostColor;
            cbLight->GhostRimPower = m_ghostRimPower;
            cbLight->GhostRimIntensity = m_ghostRimIntensity;
            cbLight->GhostTime = m_ghostTime;
            cbLight->HitFlashColor = DirectX::SimpleMath::Vector4(0.0f, 0.0f, 0.0f, 0.0f);

            context->Unmap(m_cbLighting.Get(), 0);
        }
    }

    // Render each mesh using custom shaders
    for (const auto& mesh : m_model->meshes)
    {
        DirectX::SimpleMath::Matrix meshWorld = world;
        if (!m_model->bones.empty() && m_drawBones && mesh->boneIndex != DirectX::ModelBone::c_Invalid && mesh->boneIndex < m_model->bones.size())
        {
            DirectX::SimpleMath::Matrix boneMat(m_drawBones[mesh->boneIndex]);
            meshWorld = boneMat * world;
        }

        // Update Matrix Buffer (b0) for this mesh
        if (m_cbMatrices)
        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (SUCCEEDED(context->Map(m_cbMatrices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                CB_StaticMatrices* cb = (CB_StaticMatrices*)mapped.pData;
                cb->World = DirectX::XMMatrixTranspose(meshWorld);
                cb->WorldViewProj = DirectX::XMMatrixTranspose(meshWorld * view * proj);
                if (gameContext.shadowSystem)
                {
                    cb->LightViewProj = DirectX::XMMatrixTranspose(gameContext.shadowSystem->GetLightViewProj());
                }
                else
                {
                    cb->LightViewProj = DirectX::XMMatrixIdentity();
                }
                context->Unmap(m_cbMatrices.Get(), 0);
            }
        }

        for (const auto& part : mesh->meshParts)
        {
            UINT stride = part->vertexStride;
            UINT offset = 0;
            context->IASetVertexBuffers(0, 1, part->vertexBuffer.GetAddressOf(), &stride, &offset);
            context->IASetIndexBuffer(part->indexBuffer.Get(), part->indexFormat, 0);
            context->IASetPrimitiveTopology(part->primitiveType);

            // Apply the part's effect first so textures (DiffuseMap at t0) are bound
            if (part->effect)
            {
                part->effect->Apply(context);
            }

            // Create input layout for this mesh part if not cached
            if (!m_inputLayouts[part.get()])
            {
                auto& decl = part->vbDecl;
                if (decl && !m_vsBytecode.empty())
                {
                    std::vector<D3D11_INPUT_ELEMENT_DESC> modifiedDecl = *decl;
                    for (auto& element : modifiedDecl)
                    {
                        if (_stricmp(element.SemanticName, "SV_Position") == 0)
                        {
                            element.SemanticName = "POSITION";
                        }
                    }

                    device->CreateInputLayout(
                        modifiedDecl.data(),
                        (UINT)modifiedDecl.size(),
                        m_vsBytecode.data(),
                        m_vsBytecode.size(),
                        m_inputLayouts[part.get()].ReleaseAndGetAddressOf()
                    );
                }

                if (!m_inputLayouts[part.get()] && !m_vsBytecode.empty())
                {
                    static const D3D11_INPUT_ELEMENT_DESC defaultLayout[] = {
                        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                    };
                    device->CreateInputLayout(
                        defaultLayout,
                        (UINT)_countof(defaultLayout),
                        m_vsBytecode.data(),
                        m_vsBytecode.size(),
                        m_inputLayouts[part.get()].ReleaseAndGetAddressOf()
                    );
                }
            }

            if (m_inputLayouts[part.get()])
            {
                context->IASetInputLayout(m_inputLayouts[part.get()].Get());
                context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
                context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
                context->VSSetConstantBuffers(0, 1, m_cbMatrices.GetAddressOf());
                context->PSSetConstantBuffers(1, 1, m_cbLighting.GetAddressOf());

                ID3D11SamplerState* samplers[] = { m_samplerState.Get() };
                context->PSSetSamplers(0, 1, samplers);

                if (gameContext.shadowSystem)
                {
                    ID3D11ShaderResourceView* shadowSRV = gameContext.shadowSystem->GetShadowMapSRV();
                    context->PSSetShaderResources(4, 1, &shadowSRV);

                    ID3D11SamplerState* shadowSampler = gameContext.shadowSystem->GetShadowSampler();
                    context->PSSetSamplers(1, 1, &shadowSampler);
                }

                bool isGhost = m_isGhostMode || (m_owner != nullptr && m_owner->GetActorType() == HEIN::ActorType::Ghost);
                context->RSSetState(gameContext.commonStates.CullNone());
                if (isGhost)
                {
                    context->OMSetDepthStencilState(gameContext.commonStates.DepthRead(), 0);
                    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
                }
                else
                {
                    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
                    context->OMSetBlendState(part->isAlpha ? gameContext.commonStates.AlphaBlend() : gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
                }

                context->DrawIndexed(part->indexCount, part->startIndex, part->vertexOffset);
            }
            else
            {
                // Fallback to basic part draw if layout creation failed
                bool isGhost = m_isGhostMode || (m_owner != nullptr && m_owner->GetActorType() == HEIN::ActorType::Ghost);
                context->RSSetState(gameContext.commonStates.CullNone());
                if (isGhost)
                {
                    context->OMSetDepthStencilState(gameContext.commonStates.DepthRead(), 0);
                    context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
                }
                else
                {
                    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
                    context->OMSetBlendState(part->isAlpha ? gameContext.commonStates.AlphaBlend() : gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
                }
                context->DrawIndexed(part->indexCount, part->startIndex, part->vertexOffset);
            }
        }
    }

    // Restore default pipeline states
    context->RSSetState(gameContext.commonStates.CullCounterClockwise());
    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
    context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);

    // Unbind shadow SRV to avoid D3D11 resource hazards when shadow depth is written next frame
    if (gameContext.shadowSystem)
    {
        ID3D11ShaderResourceView* nullSRV = nullptr;
        context->PSSetShaderResources(4, 1, &nullSRV);
    }
}

void HEIN::StaticModelComponent::DrawShadow(
    GameContext& gameContext,
    const DirectX::SimpleMath::Matrix& lightViewProj
)
{
    bool isGhost = m_isGhostMode || (m_owner != nullptr && m_owner->GetActorType() == HEIN::ActorType::Ghost);
    if (!m_isVisible || !m_model || !m_castShadows || isGhost) return;

    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
    DirectX::DX11::CommonStates& states = gameContext.commonStates;

    DirectX::SimpleMath::Matrix world = m_owner->GetComponent<TransformComponent>()->GetWorldMatrix();
    DirectX::SimpleMath::Matrix view = DirectX::SimpleMath::Matrix::Identity;

    // Shadow depth rendering: substitute projection matrix with lightViewProj.
    // The DirectXTK effects render against the bound shadow depth-stencil view.
    // Pipeline color outputs are discarded, preserving depth information in the shadow buffer.
    
    // DirectXTK model draw calls configure internal rasterizer and blend states.
    
    if (!m_model->bones.empty() && m_drawBones)
    {
        m_model->Draw(context, states, m_model->bones.size(), m_drawBones.get(), world, view, lightViewProj);
    }
    else
    {
        m_model->Draw(context, states, world, view, lightViewProj);
    }
    
    // Restore states for shadow mapping if DirectXTK modified them
    context->RSSetState(gameContext.commonStates.CullNone());
    context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
    context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
}

DirectX::SimpleMath::Vector3 HEIN::StaticModelComponent::GetBoneWorldPosition(
    const wchar_t* boneName, 
    const DirectX::SimpleMath::Matrix& actorWorldMatrix
)
{
    if (!m_model || m_model->bones.empty() || !m_drawBones) return DirectX::SimpleMath::Vector3::Zero;

    for (size_t i = 0; i < m_model->bones.size(); i++)
    {
        if (m_model->bones[i].name.find(boneName) != std::wstring::npos)
        {
            DirectX::SimpleMath::Matrix boneMatrix = m_drawBones[i];
            DirectX::SimpleMath::Matrix finalWorldMatrix = boneMatrix * actorWorldMatrix;

            return finalWorldMatrix.Translation();
        }
    }

    return DirectX::SimpleMath::Vector3::Zero;
}

DirectX::SimpleMath::Vector3 HEIN::StaticModelComponent::GetBoneWorldPosition(
    const int boneNum, 
    const DirectX::SimpleMath::Matrix& actorWorldMatrix
)
{
    if (!m_model || m_model->bones.empty() || !m_drawBones) return DirectX::SimpleMath::Vector3::Zero;

    if (boneNum < 0 || static_cast<size_t>(boneNum) >= m_model->bones.size())
    {
        return DirectX::SimpleMath::Vector3::Zero;
    }

    DirectX::SimpleMath::Matrix boneMatrix = m_drawBones[boneNum];
    DirectX::SimpleMath::Matrix finalWorldMatrix = boneMatrix * actorWorldMatrix;

    return finalWorldMatrix.Translation();
}

DirectX::SimpleMath::Matrix HEIN::StaticModelComponent::GetBoneWorldMatrix(
    const wchar_t* boneName, 
    const DirectX::SimpleMath::Matrix& actorWorldMatrix
)
{
    if (!m_model || m_model->bones.empty() || !m_drawBones) return DirectX::SimpleMath::Matrix::Identity;

    for (size_t i = 0; i < m_model->bones.size(); i++)
    {
        if (m_model->bones[i].name.find(boneName) != std::wstring::npos)
        {
            DirectX::SimpleMath::Matrix boneMatrix = m_drawBones[i];
            DirectX::SimpleMath::Matrix finalWorldMatrix = boneMatrix * actorWorldMatrix;

            return finalWorldMatrix;
        }
    }

    return DirectX::SimpleMath::Matrix::Identity;
}

DirectX::SimpleMath::Matrix HEIN::StaticModelComponent::GetBoneWorldMatrix(
    const int boneNum, 
    const DirectX::SimpleMath::Matrix& actorWorldMatrix
)
{
    if (!m_model || m_model->bones.empty() || !m_drawBones) return DirectX::SimpleMath::Matrix::Identity;

    if (boneNum < 0 || static_cast<size_t>(boneNum) >= m_model->bones.size())
    {
        return DirectX::SimpleMath::Matrix::Identity;
    }
    DirectX::SimpleMath::Matrix boneMatrix = m_drawBones[boneNum];
    DirectX::SimpleMath::Matrix finalWorldMatrix = boneMatrix * actorWorldMatrix;

    return finalWorldMatrix;
}

int HEIN::StaticModelComponent::GetBoneIndex(const std::wstring boneName)
{
    if (!m_model) return -1;
    for (size_t i = 0; i < m_model->bones.size(); i++)
    {
        if (m_model->bones[i].name.find(boneName) != std::wstring::npos)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

DirectX::BoundingBox HEIN::StaticModelComponent::GetBoundingBox() const
{
    DirectX::BoundingBox totalBox;

    if (m_model != nullptr && !m_model->meshes.empty())
    {
        totalBox = m_model->meshes[0]->boundingBox;

        for (size_t i = 1; i < m_model->meshes.size(); i++) 
        {
            DirectX::BoundingBox::CreateMerged(totalBox, totalBox, m_model->meshes[i]->boundingBox);
        }
    }

    return totalBox;
}

DirectX::BoundingSphere HEIN::StaticModelComponent::GetBoundingSphere() const
{
    DirectX::BoundingSphere totalSphere;
    if (m_model != nullptr && !m_model->meshes.empty())
    {
        totalSphere = m_model->meshes[0]->boundingSphere;

        for (size_t i = 1; i < m_model->meshes.size(); i++)
        {
            DirectX::BoundingSphere::CreateMerged(totalSphere, totalSphere, m_model->meshes[i]->boundingSphere);
        }
    }
    return totalSphere;
}

void HEIN::StaticModelComponent::OnInspectorGUI(GameContext& gameContext)
{
    if (ImGui::CollapsingHeader("Static Model Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        HWND windowHandle = gameContext.deviceResources.GetWindow();
        if (!m_lastError.empty()) ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", m_lastError.c_str());
        ImGui::Checkbox("Visible", &m_isVisible);
        ImGui::SameLine();
        ImGui::Checkbox("Cast Shadow", &m_castShadows);

        // Model Path Editor
        std::string modelPathStr = std::string(m_modelPath.begin(), m_modelPath.end());
        if (ImGui::InputText("Model Path", &modelPathStr))
        {
            m_modelPath = std::wstring(modelPathStr.begin(), modelPathStr.end());
            m_needsReload = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Browse##Model"))
        {
            std::wstring file = HEIN::EditorUtils::OpenFileDialog(L"Model Files\0*.cmo;*.sdkmesh\0All Files\0*.*\0", windowHandle);
            if (!file.empty())
            {
                m_modelPath = HEIN::EditorUtils::MakeRelativePath(file);
                m_needsReload = true;
            }
        }

        // Texture Dir Editor (Optional)
        std::string texDirStr = std::string(m_textureDir.begin(), m_textureDir.end());
        if (ImGui::InputText("Texture Dir (Optional)", &texDirStr))
        {
            m_textureDir = std::wstring(texDirStr.begin(), texDirStr.end());
            m_needsReload = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse##Tex"))
        {
            std::wstring selectedFolder = HEIN::EditorUtils::SelectFolder(windowHandle);
            if (!selectedFolder.empty())
            {
                m_textureDir = HEIN::EditorUtils::MakeRelativePath(selectedFolder);
                
                // Make sure it ends with a slash if it doesn't already, so the EffectFactory handles it correctly
                if (!m_textureDir.empty() && m_textureDir.back() != L'\\' && m_textureDir.back() != L'/')
                {
                    m_textureDir += L'/';
                }
                
                m_needsReload = true;
            }
        }
    }

    if (m_needsReload)
    {
        m_needsReload = false;
        m_inputLayouts.clear();
        Initialize(gameContext, m_modelPath.c_str(), m_textureDir.empty() ? nullptr : m_textureDir.c_str());
    }
}

nlohmann::json HEIN::StaticModelComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    std::string narrowModelPath(m_modelPath.begin(), m_modelPath.end());
    std::string narrowTextureDir(m_textureDir.begin(), m_textureDir.end());
    data["ModelPath"] = narrowModelPath;
    data["TextureDir"] = narrowTextureDir;
    data["CastShadows"] = m_castShadows;
    return data;
}

void HEIN::StaticModelComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("ModelPath"))
    {
        std::string narrowModelPath = data["ModelPath"];
        m_modelPath = std::wstring(narrowModelPath.begin(), narrowModelPath.end());
    }
    if (data.contains("TextureDir"))
    {
        std::string narrowTextureDir = data["TextureDir"];
        m_textureDir = std::wstring(narrowTextureDir.begin(), narrowTextureDir.end());
    }
    if (data.contains("CastShadows"))
    {
        m_castShadows = data["CastShadows"];
    }
}

void HEIN::StaticModelComponent::InitializeAfterDeserialize(GameContext& gameContext)
{
    if (!m_modelPath.empty())
    {
        if (m_textureDir.empty())
        {
            std::filesystem::path p(m_modelPath);
            std::wstring parent = p.parent_path().wstring();
            if (!parent.empty())
            {
                m_textureDir = parent + L"/";
            }
        }
        
        Initialize(gameContext, m_modelPath.c_str(), m_textureDir.empty() ? nullptr : m_textureDir.c_str());
    }
}
