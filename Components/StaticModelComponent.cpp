#include "pch.h"
#include "StaticModelComponent.h"
#include "Entities/Actor.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>
#include <Windows.h>
#include "DebugingTools/EditorUtils.h"
#include <string>
#include <filesystem>

std::shared_ptr<DirectX::EffectFactory> HEIN::StaticModelComponent::s_fxFactory = nullptr;
std::unordered_map<std::wstring, std::weak_ptr<DirectX::Model>> HEIN::StaticModelComponent::s_modelCache;

HEIN::StaticModelComponent::StaticModelComponent(Actor* owner)
	: IComponent(owner) 
{
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

    if (s_fxFactory == nullptr)
    {
        s_fxFactory = std::make_shared<DirectX::EffectFactory>(device);
    }
    if (!m_textureDir.empty())
    {
        static_cast<DirectX::EffectFactory*>(s_fxFactory.get())->SetDirectory(m_textureDir.c_str());
    }
    else
    {
        static_cast<DirectX::EffectFactory*>(s_fxFactory.get())->SetDirectory(nullptr);
    }

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
                m_model = DirectX::Model::CreateFromCMO(
                    device,
                    m_modelPath.c_str(),
                    *s_fxFactory,
                    static_cast<DirectX::ModelLoaderFlags>(
                        DirectX::ModelLoader_CounterClockwise |
                        DirectX::ModelLoader_IncludeBones
                    )
                );
            }
            else
            {
                m_model = DirectX::Model::CreateFromSDKMESH(
                    device,
                    m_modelPath.c_str(),
                    *s_fxFactory,
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
}

void HEIN::StaticModelComponent::Update(float)
{
}

void HEIN::StaticModelComponent::Draw(
    GameContext& gameContext, 
    const DirectX::SimpleMath::Matrix& world, 
    const DirectX::SimpleMath::Matrix& view, 
    const DirectX::SimpleMath::Matrix& proj
)
{
    if (m_needsReload)
    {
        if (m_textureDir.empty() && !m_modelPath.empty())
        {
            std::filesystem::path p(m_modelPath);
            std::wstring parent = p.parent_path().wstring();
            if (!parent.empty())
            {
                m_textureDir = parent + L"/";
            }
        }

        Initialize(gameContext, m_modelPath.c_str(), m_textureDir.empty() ? nullptr : m_textureDir.c_str());
        m_needsReload = false;
    }

    if (!m_isVisible || !m_model) return;

    ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
    DirectX::DX11::CommonStates& states = gameContext.commonStates;

    if (!m_model->bones.empty() && m_drawBones)
    {
        m_model->Draw(context, states, m_model->bones.size(), m_drawBones.get(), world, view, proj);
    }
    else
    {
        m_model->Draw(context, states, world, view, proj);
    }
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
}

nlohmann::json HEIN::StaticModelComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    std::string narrowModelPath(m_modelPath.begin(), m_modelPath.end());
    std::string narrowTextureDir(m_textureDir.begin(), m_textureDir.end());
    data["ModelPath"] = narrowModelPath;
    data["TextureDir"] = narrowTextureDir;
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
