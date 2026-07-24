#include "pch.h"
#include "Framework/GameContext.h"
#include "SkinnedModelComponent.h"
#include "Entities/Actor.h"

std::shared_ptr<DirectX::EffectFactory> HEIN::SkinnedModelComponent::s_fxFactory = nullptr;
std::unordered_map<std::wstring, std::weak_ptr<DirectX::Model>> HEIN::SkinnedModelComponent::s_modelCache;

namespace HEIN
{
	SkinnedModelComponent::SkinnedModelComponent(Actor* owner)
		: IComponent(owner)
	{
	}

	void SkinnedModelComponent::Initialize(
		GameContext& gameContext,
		const wchar_t* modelPath,
		const wchar_t* textureDir
	)
	{
		m_modelPath = modelPath;
		m_textureDir = textureDir;
		
		ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();

		if (s_fxFactory == nullptr)
		{
			s_fxFactory = std::make_shared<DirectX::EffectFactory>(device);
		}
		static_cast<DirectX::EffectFactory*>(s_fxFactory.get())->SetDirectory(textureDir);
	
		std::wstring key = modelPath;

		std::shared_ptr<DirectX::Model> cachedModel = s_modelCache[key].lock();

		if (cachedModel != nullptr)
		{
			m_model = cachedModel;
		}
		else
		{
			m_model = DirectX::Model::CreateFromSDKMESH(
				device,
				modelPath,
				*s_fxFactory,
				static_cast<DirectX::ModelLoaderFlags>
				(
					DirectX::ModelLoader_Clockwise |
					DirectX::ModelLoader_IncludeBones
					)
			);
			s_modelCache[key] = m_model;
		}

		m_drawBones = DirectX::ModelBone::MakeArray(m_model->bones.size());
		m_skinBones = DirectX::ModelBone::MakeArray(m_model->bones.size());
		m_targetBones = DirectX::ModelBone::MakeArray(m_model->bones.size());
		m_shapShotBones = DirectX::ModelBone::MakeArray(m_model->bones.size());
		m_blendedLocalBones = DirectX::ModelBone::MakeArray(m_model->bones.size());


		// bone name checker
		/*OutputDebugStringW(L"--- BONE LIST START ---\n");
		for (const auto& bone : m_model->bones)
		{
			OutputDebugStringW(bone.name.c_str());
			OutputDebugStringW(L"\n");
		}
		OutputDebugStringW(L"--- BONE LIST END ---\n");*/



	}

	void SkinnedModelComponent::Update(float deltaTime)
	{
		if (!m_model) return;

		if (m_isBlending && m_currentAnimation != nullptr &&
			m_targetAnimation != nullptr)
		{
			m_blendTimer += deltaTime;
			float blendFactor = m_blendTimer / m_blendDuration;

			if (blendFactor >= 1.0f)
			{
				m_currentAnimation = m_targetAnimation;
				m_targetAnimation = nullptr;
				m_isBlending = false;

				m_currentAnimation->Update(deltaTime);
				m_currentAnimation->Apply(*m_model, m_model->bones.size(), m_drawBones.get());
			}
			else
			{
				//Live Dynamic Blending
				// Update Both Animation
				m_currentAnimation->Update(deltaTime);
				m_targetAnimation->Update(deltaTime);

				float stopTime = m_currentAnimation->GetEndTime() - 0.05f;

				if (m_currentAnimation->GetAnimTime() >= stopTime)
				{
					m_currentAnimation->SetAnimTime(stopTime);
				}

				m_currentAnimation->Apply(*m_model, m_model->bones.size(), m_shapShotBones.get());
				m_targetAnimation->Apply(*m_model, m_model->bones.size(), m_targetBones.get());
				
				// Get the Live raw Bones
				const DirectX::XMMATRIX* sourceLocalBones = m_currentAnimation->GetLocalBones();
				const DirectX::XMMATRIX* targetLocalBones = m_targetAnimation->GetLocalBones();

				
				for (size_t i = 0; i < m_model->bones.size(); ++i)
				{

					DirectX::XMVECTOR scaleA, rotA, transA;
					DirectX::XMVECTOR scaleB, rotB, transB;

					DirectX::XMMatrixDecompose(&scaleA, &rotA, &transA, sourceLocalBones[i]);
					DirectX::XMMatrixDecompose(&scaleB, &rotB, &transB, targetLocalBones[i]);

					DirectX::XMVECTOR blendScale = DirectX::XMVectorLerp(scaleA, scaleB, blendFactor);
					DirectX::XMVECTOR blendRot = DirectX::XMQuaternionSlerp(rotA, rotB, blendFactor);
					DirectX::XMVECTOR blendTrans = DirectX::XMVectorLerp(transA, transB, blendFactor);

					m_blendedLocalBones[i] = DirectX::XMMatrixScalingFromVector(blendScale) *
						                     DirectX::XMMatrixRotationQuaternion(blendRot) *
						                     DirectX::XMMatrixTranslationFromVector(blendTrans);
				}
				m_model->CopyAbsoluteBoneTransforms(m_model->bones.size(), m_blendedLocalBones.get(), m_drawBones.get());
			}
		}
		else if (m_currentAnimation != nullptr)
		{
			m_currentAnimation->Update(deltaTime);
			m_currentAnimation->Apply(*m_model, m_model->bones.size(), m_drawBones.get());
		}
		if (m_currentAnimation != nullptr)
		{
			for (size_t i = 0; i < m_model->bones.size(); i++)
			{
				m_skinBones[i] = m_drawBones[i];
			}
			m_currentAnimation->ApplySkinMatrix(*m_model, m_model->bones.size(), m_skinBones.get());
		}
	}

	void SkinnedModelComponent::Draw(
		GameContext& gameContext,
		const DirectX::SimpleMath::Matrix& world,
		const DirectX::SimpleMath::Matrix& view,
		const DirectX::SimpleMath::Matrix& proj
	)
	{
		if (!m_isVisible) return;

		if (!m_model) return;

		ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
		DirectX::DX11::CommonStates& states = gameContext.commonStates;

		m_model->DrawSkinned(context, states, m_model->bones.size(), m_skinBones.get(), world, view, proj);
	}

	DirectX::SimpleMath::Vector3 SkinnedModelComponent::GetBoneWorldPosition(
		const wchar_t* boneName, 
		const DirectX::SimpleMath::Matrix& actorWorldMatrix)
	{
		if (!m_model) return DirectX::SimpleMath::Vector3::Zero;

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

	DirectX::SimpleMath::Vector3 SkinnedModelComponent::GetBoneWorldPosition(
		const int boneNum, 
		const DirectX::SimpleMath::Matrix& actorWorldMatrix
	)
	{
		if (!m_model) return DirectX::SimpleMath::Vector3::Zero;

		DirectX::SimpleMath::Matrix boneMatrix = m_drawBones[boneNum];
		DirectX::SimpleMath::Matrix finalWorldMatrix = boneMatrix * actorWorldMatrix;

		return finalWorldMatrix.Translation();
	}

	DirectX::SimpleMath::Matrix SkinnedModelComponent::GetBoneWorldMatrix(
		const wchar_t* boneName,
		const DirectX::SimpleMath::Matrix& actorWorldMatrix
	)
	{
		if (!m_model) return DirectX::SimpleMath::Matrix::Identity;

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

	DirectX::SimpleMath::Matrix SkinnedModelComponent::GetBoneWorldMatrix(const int boneNum, const DirectX::SimpleMath::Matrix& actorWorldMatrix)
	{
		if (!m_model) return DirectX::SimpleMath::Matrix::Identity;

		if (boneNum < 0 || static_cast<size_t>(boneNum) >= m_model->bones.size())
		{
			return DirectX::SimpleMath::Matrix::Identity;
		}
		DirectX::SimpleMath::Matrix boneMatrix = m_drawBones[boneNum];
		DirectX::SimpleMath::Matrix finalWorldMatrix = boneMatrix * actorWorldMatrix;

		return finalWorldMatrix;
	}

	int SkinnedModelComponent::GetBoneIndex(const std::wstring boneName)
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

	void SkinnedModelComponent::LoadAnimation(const std::string& name, const wchar_t* animPath)
	{
		if (!m_model) return;

		std::unique_ptr<DX::AnimationSDKMESH> newAnim = std::make_unique<DX::AnimationSDKMESH>();
		DX::ThrowIfFailed(newAnim->Load(animPath));
		newAnim->Bind(*m_model);

		m_animations[name] = std::move(newAnim);

		if (m_currentAnimation == nullptr)
		{
			m_currentAnimation = m_animations[name].get();
		}
	}

	void SkinnedModelComponent::ChangeAnimation(const std::string& name)
	{
		auto it = m_animations.find(name);
		if (it != m_animations.end())
		{
			m_currentAnimation = it->second.get();
		}
	}

	void SkinnedModelComponent::CrossfadeAnimation(const std::string& name, float duration, bool forceRestart)
	{
		std::unordered_map<std::string, std::unique_ptr<DX::AnimationSDKMESH>>::iterator it =
			m_animations.find(name);

		if (it != m_animations.end())
		{
			if (m_currentAnimation == nullptr)
			{
				m_currentAnimation = it->second.get();
				return;
			}

			if (m_isBlending && m_targetAnimation == it->second.get()) return;

			if (!m_isBlending && m_currentAnimation == it->second.get()) return;
			
		
			for (size_t i = 0; i < m_model->bones.size(); ++i)
			{
				m_shapShotBones[i] = m_drawBones[i];
			}

			m_targetAnimation = it->second.get();
			m_blendDuration = duration;
			m_blendTimer = 0.0f;
			m_isBlending = true;

			m_targetAnimation->SetAnimTime(0.0f);
		}
	}

}

nlohmann::json HEIN::SkinnedModelComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    std::string narrowModelPath(m_modelPath.begin(), m_modelPath.end());
    std::string narrowTextureDir(m_textureDir.begin(), m_textureDir.end());
    data["ModelPath"] = narrowModelPath;
    data["TextureDir"] = narrowTextureDir;
    return data;
}

void HEIN::SkinnedModelComponent::Deserialize(const nlohmann::json& data)
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

void HEIN::SkinnedModelComponent::InitializeAfterDeserialize(GameContext& gameContext)
{
    if (!m_modelPath.empty() && !m_textureDir.empty())
    {
        Initialize(gameContext, m_modelPath.c_str(), m_textureDir.c_str());
    }
}
