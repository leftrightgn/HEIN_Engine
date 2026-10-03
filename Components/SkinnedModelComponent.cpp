#include "pch.h"
#include "Framework/GameContext.h"
#include "SkinnedModelComponent.h"
#include "Entities/Actor.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>
#include <Windows.h>
#include "DebugingTools/EditorUtils.h"
#include <string>
#include <filesystem>
#include <d3dcompiler.h>
#include "Effect/ReadData.h"
#include "TransformComponent.h"
#include "Common/ShadowSystem.h"
#include "Entities/ActorManager.h"
#include "Common/ShaderStructures.h"
#include "LightComponent.h"
#include "Camera/CameraController.h"
#include "FogComponent.h"
#include "../../../Dual/BlackBoard/CombatBlackBoard.h"


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
			try
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
			catch (const std::exception&)
			{
				// Model file not found or invalid
				m_model = nullptr;
			}
		}

		if (m_model == nullptr)
		{
			return; // Do not initialize bones if the model failed to load
		}

		m_drawBones = DirectX::ModelBone::MakeArray(m_model->bones.size());
		m_skinBones = DirectX::ModelBone::MakeArray(m_model->bones.size());
		m_targetBones = DirectX::ModelBone::MakeArray(m_model->bones.size());
		m_shapShotBones = DirectX::ModelBone::MakeArray(m_model->bones.size());
		m_blendedLocalBones = DirectX::ModelBone::MakeArray(m_model->bones.size());

		// Ensure they never hold uninitialized memory from the start
		for (size_t i = 0; i < m_model->bones.size(); ++i)
		{
			m_drawBones[i] = DirectX::SimpleMath::Matrix::Identity;
			m_skinBones[i] = DirectX::SimpleMath::Matrix::Identity;
			m_targetBones[i] = DirectX::SimpleMath::Matrix::Identity;
			m_shapShotBones[i] = DirectX::SimpleMath::Matrix::Identity;
			m_blendedLocalBones[i] = DirectX::SimpleMath::Matrix::Identity;
		}


		// Load Shaders (Precompiled CSO first, then runtime fallback)
		std::vector<uint8_t> vsData;
		std::vector<uint8_t> psData;
		bool loadedCso = false;
		try
		{
			vsData = DX::ReadData(L"Resources/Shaders/CustomSkinned_VS.cso");
			psData = DX::ReadData(L"Resources/Shaders/CustomSkinned_PS.cso");
			loadedCso = true;
		}
		catch (...)
		{
			try
			{
				vsData = DX::ReadData(L"CustomSkinned_VS.cso");
				psData = DX::ReadData(L"CustomSkinned_PS.cso");
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
			HRESULT hr = D3DCompileFromFile(L"../External/Engine/Shaders/CustomSkinned.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
			if (FAILED(hr))
			{
				hr = D3DCompileFromFile(L"External/Engine/Shaders/CustomSkinned.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
			}
			if (FAILED(hr))
			{
				if (errorBlob) {
					FILE* f;
					if (fopen_s(&f, "ShaderError.txt", "w") == 0) {
						fprintf(f, "%s", (char*)errorBlob->GetBufferPointer());
						fclose(f);
					}
				}
				return; // Gracefully fail
			}

			hr = D3DCompileFromFile(L"../External/Engine/Shaders/CustomSkinned.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &psBlob, &errorBlob);
			if (FAILED(hr))
			{
				hr = D3DCompileFromFile(L"External/Engine/Shaders/CustomSkinned.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &psBlob, &errorBlob);
			}
			if (FAILED(hr))
			{
				if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
				return; // Gracefully fail
			}

			vsBytecode = vsBlob->GetBufferPointer();
			vsBytecodeSize = vsBlob->GetBufferSize();
			psBytecode = psBlob->GetBufferPointer();
			psBytecodeSize = psBlob->GetBufferSize();
		}

		device->CreateVertexShader(vsBytecode, vsBytecodeSize, nullptr, m_vertexShader.ReleaseAndGetAddressOf());
		device->CreatePixelShader(psBytecode, psBytecodeSize, nullptr, m_pixelShader.ReleaseAndGetAddressOf());

		// Create Constant Buffers 
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		cbDesc.ByteWidth = sizeof(CB_Matrices);
		device->CreateBuffer(&cbDesc, nullptr, m_cbMatrices.ReleaseAndGetAddressOf());

		cbDesc.ByteWidth = sizeof(CB_Lighting);
		device->CreateBuffer(&cbDesc, nullptr, m_cbLighting.ReleaseAndGetAddressOf());

		// Create Input Layout explicitly to match CustomSkinned.hlsl shader using the actual vertex declaration from the model!
		bool layoutCreated = false;
		if (!m_model->meshes.empty())
		{
			for (const auto& mesh : m_model->meshes)
			{
				for (const auto& part : mesh->meshParts)
				{
					if (dynamic_cast<DirectX::IEffectSkinning*>(part->effect.get()))
					{
						auto& decl = part->vbDecl;
						if (decl)
						{
							std::vector<D3D11_INPUT_ELEMENT_DESC> modifiedDecl = *decl;
							for (auto& element : modifiedDecl)
							{
								if (strcmp(element.SemanticName, "SV_Position") == 0 || strcmp(element.SemanticName, "SV_POSITION") == 0)
								{
									element.SemanticName = "POSITION";
								}
							}

							HRESULT hrLayout = device->CreateInputLayout(
								modifiedDecl.data(),
								(UINT)modifiedDecl.size(),
								vsBytecode,
								vsBytecodeSize,
								m_inputLayout.ReleaseAndGetAddressOf()
							);
							if (SUCCEEDED(hrLayout))
							{
								layoutCreated = true;
								break;
							}
						}
					}
				}
				if (layoutCreated) break;
			}
		}

		
		// Create Sampler State
		D3D11_SAMPLER_DESC sampDesc = {};
		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		device->CreateSamplerState(&sampDesc, m_samplerState.ReleaseAndGetAddressOf());

	}

	
	int SkinnedModelComponent::GetRootBoneIndex() const
	{
		if (!m_model || m_model->bones.empty()) return -1;

		// 1. Explicitly configured root bone name
		if (!m_rootBoneName.empty())
		{
			for (size_t i = 0; i < m_model->bones.size(); ++i)
			{
				if (m_model->bones[i].name.find(m_rootBoneName) != std::wstring::npos)
				{
					return static_cast<int>(i);
				}
			}
		}

		// 2. Search common root bone naming conventions
		const wchar_t* commonNames[] = {
			L"mixamorig:Hips",
			L"Hips",
			L"hips",
			L"Root",
			L"root",
			L"Pelvis",
			L"pelvis",
			L"Bip01"
		};
		for (const wchar_t* name : commonNames)
		{
			for (size_t i = 0; i < m_model->bones.size(); ++i)
			{
				if (m_model->bones[i].name.find(name) != std::wstring::npos)
				{
					return static_cast<int>(i);
				}
			}
		}

		// 3. Fallback: bone without parent (parentIndex == ModelBone::c_Invalid)
		for (size_t i = 0; i < m_model->bones.size(); ++i)
		{
			if (m_model->bones[i].parentIndex == DirectX::ModelBone::c_Invalid)
			{
				return static_cast<int>(i);
			}
		}

		// 4. Default fallback: bone 0
		return 0;
	}

	DirectX::SimpleMath::Vector3 SkinnedModelComponent::ExtractClipRootMotionDelta(
		DX::AnimationSDKMESH* anim,
		int rootIdx,
		double& inOutPrevTime,
		bool& inOutHasPrev
	)
	{
		if (!anim || rootIdx < 0) return DirectX::SimpleMath::Vector3::Zero;

		double currentAnimTime = anim->GetAnimTime();
		double clipDuration = anim->GetEndTime();

		if (clipDuration <= 0.0001)
		{
			return DirectX::SimpleMath::Vector3::Zero;
		}

		uint32_t numKeys = anim->GetNumAnimationKeys();
		if (numKeys == 0) return DirectX::SimpleMath::Vector3::Zero;

		if (!inOutHasPrev)
		{
			inOutHasPrev = true;
			inOutPrevTime = currentAnimTime;
			return DirectX::SimpleMath::Vector3::Zero;
		}

		double prevAnimTime = inOutPrevTime;
		inOutPrevTime = currentAnimTime;

		DirectX::SimpleMath::Vector3 startPos = anim->GetRootTranslationAtKey(rootIdx, 0);
		DirectX::SimpleMath::Vector3 endPos = anim->GetRootTranslationAtKey(rootIdx, numKeys - 1);
		DirectX::SimpleMath::Vector3 cycleDisplacement = endPos - startPos;

		int prevCycle = static_cast<int>(std::floor(prevAnimTime / clipDuration));
		int currentCycle = static_cast<int>(std::floor(currentAnimTime / clipDuration));

		DirectX::SimpleMath::Vector3 rawDelta = DirectX::SimpleMath::Vector3::Zero;

		if (currentCycle == prevCycle)
		{
			double tPrev = prevAnimTime - prevCycle * clipDuration;
			double tCurr = currentAnimTime - currentCycle * clipDuration;
			DirectX::SimpleMath::Vector3 pPrev = anim->GetRootTranslationAtTime(rootIdx, tPrev);
			DirectX::SimpleMath::Vector3 pCurr = anim->GetRootTranslationAtTime(rootIdx, tCurr);
			rawDelta = pCurr - pPrev;
		}
		else if (currentCycle > prevCycle)
		{
			double tPrev = prevAnimTime - prevCycle * clipDuration;
			double tCurr = currentAnimTime - currentCycle * clipDuration;

			DirectX::SimpleMath::Vector3 pPrev = anim->GetRootTranslationAtTime(rootIdx, tPrev);
			DirectX::SimpleMath::Vector3 pCurr = anim->GetRootTranslationAtTime(rootIdx, tCurr);

			DirectX::SimpleMath::Vector3 deltaFromPrevToEnd = endPos - pPrev;
			int fullCycles = (currentCycle - prevCycle) - 1;
			DirectX::SimpleMath::Vector3 fullCyclesDelta = cycleDisplacement * static_cast<float>(fullCycles > 0 ? fullCycles : 0);
			DirectX::SimpleMath::Vector3 deltaFromStartToCurr = pCurr - startPos;

			rawDelta = deltaFromPrevToEnd + fullCyclesDelta + deltaFromStartToCurr;
		}
		else
		{
			// animTime jumped backwards (e.g. animation restarted)
			double tCurr = currentAnimTime - currentCycle * clipDuration;
			DirectX::SimpleMath::Vector3 pCurr = anim->GetRootTranslationAtTime(rootIdx, tCurr);
			rawDelta = pCurr - startPos;
		}

		if (!m_rootMotionExtractY)
		{
			rawDelta.y = 0.0f;
		}

		return rawDelta;
	}

	void SkinnedModelComponent::LockRootBone(
		DirectX::XMMATRIX* localBones,
		int rootIdx,
		const DirectX::SimpleMath::Vector3& initialPos
	)
	{
		if (!localBones || rootIdx < 0) return;

		DirectX::XMVECTOR scale, rot, trans;
		if (!DirectX::XMMatrixDecompose(&scale, &rot, &trans, localBones[rootIdx])) return;

		DirectX::SimpleMath::Vector3 currentPos = trans;
		DirectX::SimpleMath::Vector3 lockedTrans;
		if (m_rootMotionExtractY)
		{
			lockedTrans = initialPos;
		}
		else
		{
			// Preserve vertical Y translation so hip bobbing / dips stay on the mesh
			lockedTrans = DirectX::SimpleMath::Vector3(initialPos.x, currentPos.y, initialPos.z);
		}

		localBones[rootIdx] = DirectX::XMMatrixScalingFromVector(scale) *
		                      DirectX::XMMatrixRotationQuaternion(rot) *
		                      DirectX::XMMatrixTranslationFromVector(DirectX::SimpleMath::Vector3(lockedTrans));
	}

	void SkinnedModelComponent::ApplyRootMotionDeltaToWorld(const DirectX::SimpleMath::Vector3& localDelta, float deltaTime)
	{
		DirectX::SimpleMath::Vector3 worldDelta = DirectX::SimpleMath::Vector3::Zero;
		if (m_owner != nullptr)
		{
			auto* transform = m_owner->GetComponent<TransformComponent>();
			if (transform != nullptr)
			{
				DirectX::SimpleMath::Vector3 scaledDelta = localDelta * transform->GetScale();
				worldDelta = DirectX::SimpleMath::Vector3::Transform(scaledDelta, transform->GetRotation());
			}
		}
		m_rootMotionDelta = worldDelta;

		if (m_isVisible && m_owner != nullptr)
		{
			auto* bb = m_owner->GetComponent<CombatBlackBoard>();
			auto* transform = m_owner->GetComponent<TransformComponent>();

			if (bb != nullptr)
			{
				// Deadzone (0.01 units) prevents microscopic in-place hip bobbing from triggering movement
				if (worldDelta.LengthSquared() > 0.0001f)
				{
					bb->hasRootMotion = true;
					float safeDt = (deltaTime > 0.0001f) ? deltaTime : 0.016f;
					DirectX::SimpleMath::Vector3 computedVel = worldDelta / safeDt;
					float speedSq = computedVel.LengthSquared();
					constexpr float MAX_ROOT_SPEED = 60.0f; // Sane limit prevents velocity explosions
					if (speedSq > MAX_ROOT_SPEED * MAX_ROOT_SPEED)
					{
						computedVel = (computedVel / std::sqrt(speedSq)) * MAX_ROOT_SPEED;
					}
					bb->rootMotionVelocity = computedVel;
					bb->rootMotionDelta = worldDelta;
				}
				else
				{
					bb->hasRootMotion = false;
					bb->rootMotionVelocity = DirectX::SimpleMath::Vector3::Zero;
					bb->rootMotionDelta = DirectX::SimpleMath::Vector3::Zero;
				}
			}
			else if (transform != nullptr && worldDelta.LengthSquared() > 0.0001f)
			{
				// If no CombatBlackBoard on actor, directly move transform
				transform->SetPosition(transform->GetPosition() + worldDelta);
			}
		}
	}

	void SkinnedModelComponent::Update(float deltaTime)
	{
		if (!m_model) return;

		int rootIdx = m_enableRootMotion ? GetRootBoneIndex() : -1;

		if (m_isBlending)
		{
			if (m_currentAnimation == m_targetAnimation || m_targetAnimation == nullptr)
			{
				m_isBlending = false;
				m_targetAnimation = nullptr;
			}
		}

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

				// Transfer target tracking state to active tracking state
				m_prevAnimTime = m_targetPrevAnimTime;
				m_initialRootTranslation = m_targetInitialRootTranslation;
				m_hasPrevRootTranslation = m_targetHasPrevRootTranslation;
				m_targetHasPrevRootTranslation = false;

				m_currentAnimation->Update(deltaTime);
				m_currentAnimation->Apply(*m_model, m_model->bones.size(), m_drawBones.get());

				if (m_enableRootMotion && rootIdx >= 0)
				{
					DirectX::SimpleMath::Vector3 localDelta = ExtractClipRootMotionDelta(
						m_currentAnimation, rootIdx,
						m_prevAnimTime, m_hasPrevRootTranslation
					);
					LockRootBone(m_currentAnimation->GetLocalBones(), rootIdx, m_initialRootTranslation);
					m_model->CopyAbsoluteBoneTransforms(m_model->bones.size(), m_currentAnimation->GetLocalBones(), m_drawBones.get());
					ApplyRootMotionDeltaToWorld(localDelta, deltaTime);
				}
			}
			else
			{
				// Live Dynamic Blending
				m_currentAnimation->Update(deltaTime);
				m_targetAnimation->Update(deltaTime);

				m_currentAnimation->Apply(*m_model, m_model->bones.size(), m_shapShotBones.get());
				m_targetAnimation->Apply(*m_model, m_model->bones.size(), m_targetBones.get());

				if (m_enableRootMotion && rootIdx >= 0)
				{
					DirectX::SimpleMath::Vector3 deltaA = ExtractClipRootMotionDelta(
						m_currentAnimation, rootIdx,
						m_prevAnimTime, m_hasPrevRootTranslation
					);
					DirectX::SimpleMath::Vector3 deltaB = ExtractClipRootMotionDelta(
						m_targetAnimation, rootIdx,
						m_targetPrevAnimTime, m_targetHasPrevRootTranslation
					);

					// Blend the displacement: (1 - blendFactor) * A + blendFactor * B
					DirectX::SimpleMath::Vector3 blendedDelta = DirectX::SimpleMath::Vector3::Lerp(deltaA, deltaB, blendFactor);

					// Lock root bones in both clips so blended bones remain anchored
					LockRootBone(m_currentAnimation->GetLocalBones(), rootIdx, m_initialRootTranslation);
					LockRootBone(m_targetAnimation->GetLocalBones(), rootIdx, m_targetInitialRootTranslation);

					ApplyRootMotionDeltaToWorld(blendedDelta, deltaTime);
				}

				// Get the Live raw Bones
				const DirectX::XMMATRIX* sourceLocalBones = m_currentAnimation->GetLocalBones();
				const DirectX::XMMATRIX* targetLocalBones = m_targetAnimation->GetLocalBones();

				for (size_t i = 0; i < m_model->bones.size(); ++i)
				{
					DirectX::XMVECTOR scaleA, rotA, transA;
					DirectX::XMVECTOR scaleB, rotB, transB;

					bool decompA = DirectX::XMMatrixDecompose(&scaleA, &rotA, &transA, sourceLocalBones[i]);
					bool decompB = DirectX::XMMatrixDecompose(&scaleB, &rotB, &transB, targetLocalBones[i]);

					if (decompA && decompB)
					{
						DirectX::XMVECTOR blendScale = DirectX::XMVectorLerp(scaleA, scaleB, blendFactor);
						DirectX::XMVECTOR blendRot = DirectX::XMQuaternionSlerp(rotA, rotB, blendFactor);
						DirectX::XMVECTOR blendTrans = DirectX::XMVectorLerp(transA, transB, blendFactor);

						m_blendedLocalBones[i] = DirectX::XMMatrixScalingFromVector(blendScale) *
												 DirectX::XMMatrixRotationQuaternion(blendRot) *
												 DirectX::XMMatrixTranslationFromVector(blendTrans);
					}
					else
					{
						m_blendedLocalBones[i] = sourceLocalBones[i];
					}
				}

				m_model->CopyAbsoluteBoneTransforms(m_model->bones.size(), m_blendedLocalBones.get(), m_drawBones.get());
			}
		}
		else if (m_currentAnimation != nullptr)
		{
			m_currentAnimation->Update(deltaTime);
			m_currentAnimation->Apply(*m_model, m_model->bones.size(), m_drawBones.get());

			if (m_enableRootMotion && rootIdx >= 0)
			{
				if (!m_hasPrevRootTranslation)
				{
					m_initialRootTranslation = m_currentAnimation->GetRootTranslationAtKey(rootIdx, 0);
				}
				DirectX::SimpleMath::Vector3 localDelta = ExtractClipRootMotionDelta(
					m_currentAnimation, rootIdx,
					m_prevAnimTime, m_hasPrevRootTranslation
				);
				LockRootBone(m_currentAnimation->GetLocalBones(), rootIdx, m_initialRootTranslation);
				m_model->CopyAbsoluteBoneTransforms(m_model->bones.size(), m_currentAnimation->GetLocalBones(), m_drawBones.get());
				ApplyRootMotionDeltaToWorld(localDelta, deltaTime);
			}
		}
		else
		{
			m_rootMotionDelta = DirectX::SimpleMath::Vector3::Zero;
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
		if (!m_model || !m_isVisible) return;

		bool isPlayer = (m_owner->GetActorType() == HEIN::ActorType::Player);

		if (!isPlayer)
		{
			DirectX::SimpleMath::Matrix cullingView = view;
			DirectX::SimpleMath::Matrix cullingProj = proj;

			// Force culling to use the main camera
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
			DirectX::BoundingFrustum worldFrustum(cullingProj, true);
			DirectX::SimpleMath::Matrix camWorld;
			if (std::abs(cullingView.Determinant()) < 1e-6f)
				camWorld = DirectX::SimpleMath::Matrix::Identity;
			else
				camWorld = cullingView.Invert();

			worldFrustum.Transform(worldFrustum, camWorld);

			DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&worldFrustum.Orientation);
			q = DirectX::XMQuaternionNormalize(q);
			DirectX::XMStoreFloat4(&worldFrustum.Orientation, q);

			// Test Expanded Bounding Boxes against the Frustum
			bool isVisible = false;
			for (const auto& mesh : m_model->meshes)
			{
				// Retrieve the base static bounding box
				DirectX::BoundingBox expandedBox = mesh->boundingBox;

				// Expand the extents by 50% to account for animation stretching/reaching
				expandedBox.Extents.x *= 1.5f;
				expandedBox.Extents.y *= 1.5f;
				expandedBox.Extents.z *= 1.5f;

				DirectX::BoundingOrientedBox worldOBB;
				DirectX::BoundingOrientedBox::CreateFromBoundingBox(worldOBB, expandedBox);
				worldOBB.Transform(worldOBB, world);

				if (worldFrustum.Intersects(worldOBB))
				{
					isVisible = true;
					break;
				}
			}

			// Abort drawing if completely off-screen
			if (!isVisible) return;
		}

		ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

		// The Matrix Buffer update has been moved inside the mesh loop

		// Update Lighting Buffer
		HEIN::LightComponent* activeLight = nullptr;
		HEIN::FogComponent* fogComp = nullptr;
		for (auto& pair : gameContext.actorManager->GetAllActors())
		{
			if (!activeLight) activeLight = pair.second->GetComponent<HEIN::LightComponent>();
			if (!fogComp) fogComp = pair.second->GetComponent<HEIN::FogComponent>();
			if (activeLight && fogComp) break;
		}

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
					// Ensure LightDir is never exactly zero to prevent normalize(0) -> NaN in shader
					if (cbLight->LightDir.LengthSquared() < 0.0001f) cbLight->LightDir = DirectX::SimpleMath::Vector3(0, -1, 0);
					
					cbLight->LightType = static_cast<int>(activeLight->GetLightType());
					cbLight->LightColor = activeLight->GetColor();
					cbLight->LightIntensity = activeLight->GetIntensity();
					cbLight->LightRange = activeLight->GetRange();
					cbLight->LightSpotAngle = DirectX::XMConvertToRadians(activeLight->GetSpotAngle());
				}
				else
				{
					// Fallback if no LightComponent is in the scene so the model doesn't turn invisible (NaN)
					cbLight->LightPos = DirectX::SimpleMath::Vector3(0, 10, 0);
					cbLight->LightDir = DirectX::SimpleMath::Vector3(0, -1, 1); // Not zero!
					cbLight->LightType = 0; // Directional
					cbLight->LightColor = DirectX::SimpleMath::Color(1, 1, 1, 1);
					cbLight->LightIntensity = 1.0f;
					cbLight->LightRange = 100.0f;
					cbLight->LightSpotAngle = 0.785f;
				}

				// Fill fog data from global FogComponent
				if (fogComp)
				{
					cbLight->FogStart = fogComp->m_fogStart;
					cbLight->FogEnd = fogComp->m_fogEnd;
					cbLight->FogColor = fogComp->m_fogColor;
				}
				else
				{
					// Default: push fog far away to effectively disable it
					cbLight->FogStart = 100000.0f;
					cbLight->FogEnd = 200000.0f;
					cbLight->FogColor = DirectX::SimpleMath::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
				}

				// Camera position for distance-based fog in the pixel shader
				DirectX::SimpleMath::Matrix viewInv = view.Invert();
				cbLight->CameraPos = viewInv.Translation();
				cbLight->FogPadding = 0.0f;

				context->Unmap(m_cbLighting.Get(), 0);
			}
		}

		// Bind Pipeline
		context->IASetInputLayout(m_inputLayout.Get());
		context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
		context->PSSetShader(nullptr, nullptr, 0);

		ID3D11Buffer* cbs[] = { m_cbMatrices.Get() };
		context->VSSetConstantBuffers(0, 1, cbs);

		ID3D11Buffer* lightCbs[] = { m_cbLighting.Get() };
		context->PSSetConstantBuffers(1, 1, lightCbs);

		ID3D11SamplerState* samplers[] = { m_samplerState.Get() };
		context->PSSetSamplers(0, 1, samplers);

		// Draw Mesh Parts
		for (const auto& mesh : m_model->meshes)
		{
			// UPDATE MATRIX BUFFER FOR THIS SPECIFIC MESH
			if (m_cbMatrices)
			{
				D3D11_MAPPED_SUBRESOURCE mapped;
				if (SUCCEEDED(context->Map(m_cbMatrices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
				{
					CB_Matrices* cb = (CB_Matrices*)mapped.pData;
					cb->World = XMMatrixTranspose(world);
					cb->WorldViewProj = XMMatrixTranspose(world * view * proj);

					// This requires access to the shadow system from GameContext
					cb->LightViewProj = XMMatrixTranspose(gameContext.shadowSystem->GetLightViewProj());

					// Populate the bone matrices using the mesh's specific bone palette (boneInfluences)
					for (size_t i = 0; i < 256; ++i)
					{
						cb->BoneTransforms[i] = DirectX::XMMatrixIdentity();
					}

					if (!mesh->boneInfluences.empty())
					{
						for (size_t i = 0; i < mesh->boneInfluences.size() && i < 256; ++i)
						{
							uint32_t globalBoneIndex = mesh->boneInfluences[i];
							if (globalBoneIndex < m_model->bones.size())
							{
								cb->BoneTransforms[i] = XMMatrixTranspose(m_skinBones[globalBoneIndex]);
							}
						}
					}
					else
					{
						// Fallback if no palette is used
						for (size_t i = 0; i < m_model->bones.size() && i < 256; ++i)
						{
							cb->BoneTransforms[i] = XMMatrixTranspose(m_skinBones[i]);
						}
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

				auto skinnedEffect = dynamic_cast<DirectX::IEffectSkinning*>(part->effect.get());
				if (skinnedEffect)
				{
					part->effect->Apply(context);
					
					if (!m_staticInputLayouts[part.get()])
					{
						part->CreateInputLayout(gameContext.deviceResources.GetD3DDevice(), part->effect.get(), m_staticInputLayouts[part.get()].GetAddressOf());
					}
					if (m_staticInputLayouts[part.get()])
					{
						context->IASetInputLayout(m_staticInputLayouts[part.get()].Get());
					}

					// Bind specialized skinned mesh shaders and constant buffers
					context->IASetInputLayout(m_inputLayout.Get());
					context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
					context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
					context->VSSetConstantBuffers(0, 1, m_cbMatrices.GetAddressOf());
					context->PSSetConstantBuffers(1, 1, m_cbLighting.GetAddressOf());
					ID3D11SamplerState* mySamplers[] = { m_samplerState.Get() };
					context->PSSetSamplers(0, 1, mySamplers);

					// Explicitly re-bind shadow map resources to guarantee they aren't clobbered by other actors
					if (gameContext.shadowSystem)
					{
						ID3D11ShaderResourceView* shadowSRV = gameContext.shadowSystem->GetShadowMapSRV();
						context->PSSetShaderResources(4, 1, &shadowSRV);

						ID3D11SamplerState* shadowSampler = gameContext.shadowSystem->GetShadowSampler();
						context->PSSetSamplers(1, 1, &shadowSampler);
					}
				}
				else if (part->effect)
				{
					// For non-skinned parts (like weapons), just use the default effect
					part->effect->Apply(context);
					
					if (!m_staticInputLayouts[part.get()])
					{
						part->CreateInputLayout(gameContext.deviceResources.GetD3DDevice(), part->effect.get(), m_staticInputLayouts[part.get()].GetAddressOf());
					}
					if (m_staticInputLayouts[part.get()])
					{
						context->IASetInputLayout(m_staticInputLayouts[part.get()].Get());
					}
				}

				// Rasterizer state: CullNone ensures two-sided geometry remains visible
				context->RSSetState(gameContext.commonStates.CullNone());

				
				context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
				context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);

				context->DrawIndexed(part->indexCount, part->startIndex, part->vertexOffset);
			}
		}

		if (gameContext.shadowSystem)
		{
			ID3D11ShaderResourceView* nullSRV = nullptr;
			context->PSSetShaderResources(4, 1, &nullSRV);
			ID3D11SamplerState* nullSampler = nullptr;
			context->PSSetSamplers(1, 1, &nullSampler);
		}
	}


	void SkinnedModelComponent::DrawShadow(GameContext& gameContext, const DirectX::SimpleMath::Matrix& lightViewProj)
	{
		if (!m_model || !m_isVisible) return;

		ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();

		for (const auto& mesh : m_model->meshes)
		{
			// UPDATE MATRIX BUFFER FOR THIS SPECIFIC MESH
			if (m_cbMatrices)
			{
				D3D11_MAPPED_SUBRESOURCE mapped;
				if (SUCCEEDED(context->Map(m_cbMatrices.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
				{
					CB_Matrices* cb = (CB_Matrices*)mapped.pData;
					cb->WorldViewProj = XMMatrixTranspose(m_owner->GetComponent<TransformComponent>()->GetWorldMatrix() * lightViewProj);

					cb->World = DirectX::XMMatrixIdentity();
					cb->LightViewProj = DirectX::XMMatrixIdentity();

					// Populate the bone matrices using the mesh's specific bone palette (boneInfluences)
					for (size_t i = 0; i < 256; ++i)
					{
						cb->BoneTransforms[i] = DirectX::XMMatrixIdentity();
					}

					if (!mesh->boneInfluences.empty())
					{
						for (size_t i = 0; i < mesh->boneInfluences.size() && i < 256; ++i)
						{
							uint32_t globalBoneIndex = mesh->boneInfluences[i];
							if (globalBoneIndex < m_model->bones.size())
							{
								cb->BoneTransforms[i] = XMMatrixTranspose(m_skinBones[globalBoneIndex]);
							}
						}
					}
					else
					{
						for (size_t i = 0; i < m_model->bones.size() && i < 256; ++i)
						{
							cb->BoneTransforms[i] = XMMatrixTranspose(m_skinBones[i]);
						}
					}
					context->Unmap(m_cbMatrices.Get(), 0);
				}
			}

			for (const auto& part : mesh->meshParts)
			{
				if (part->isAlpha)
				{
					continue; // Skip transparent quads (like blob shadows/auras) in shadow pass
				}

				UINT stride = part->vertexStride;
				UINT offset = 0;
				context->IASetVertexBuffers(0, 1, part->vertexBuffer.GetAddressOf(), &stride, &offset);
				context->IASetIndexBuffer(part->indexBuffer.Get(), part->indexFormat, 0);
				context->IASetPrimitiveTopology(part->primitiveType);

				auto skinnedEffect = dynamic_cast<DirectX::IEffectSkinning*>(part->effect.get());
				if (skinnedEffect)
				{
					// Apply effect to bind textures, then immediately overwrite shaders/buffers with ours
					part->effect->Apply(context);

					context->IASetInputLayout(m_inputLayout.Get());
					context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
					context->PSSetShader(nullptr, nullptr, 0); // DEPTH ONLY PASS
					context->VSSetConstantBuffers(0, 1, m_cbMatrices.GetAddressOf());
					
					// Depth-only shadow generation pass: omit pixel shader samplers and lighting constant buffers
					// Force CullNone
					context->RSSetState(gameContext.commonStates.CullNone());

					// Shadows also need depth testing enabled to calculate proper occlusions!
					context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
					context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);

					context->DrawIndexed(part->indexCount, part->startIndex, part->vertexOffset);

				}
			}
		}
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

		try 
		{
			// The Animation loader strictly requires backslashes in the file path or it will fail!
			std::wstring safePath = animPath;
			for (wchar_t& c : safePath) 
			{
				if (c == L'/') c = L'\\';
			}

			std::unique_ptr<DX::AnimationSDKMESH> newAnim = std::make_unique<DX::AnimationSDKMESH>();
			DX::ThrowIfFailed(newAnim->Load(safePath.c_str()));
			newAnim->Bind(*m_model);

			bool wasCurrent = (m_currentAnimation != nullptr && m_animations.find(name) != m_animations.end() && m_currentAnimation == m_animations[name].get());
			bool wasTarget = (m_targetAnimation != nullptr && m_animations.find(name) != m_animations.end() && m_targetAnimation == m_animations[name].get());

			m_animations[name] = std::move(newAnim);
			m_animationPaths[name] = animPath;

			if (m_currentAnimation == nullptr || wasCurrent)
			{
				m_currentAnimation = m_animations[name].get();
			}
			if (wasTarget)
			{
				m_targetAnimation = m_animations[name].get();
			}
		}
		catch (const std::exception& e)
		{
			char buffer[512];
			sprintf_s(buffer, "Failed to load animation: %s\n", e.what()); m_lastError = buffer;
			OutputDebugStringA(buffer);
		}
	}

	void SkinnedModelComponent::RemoveAnimation(const std::string& name)
	{
		auto it = m_animations.find(name);
		if (it != m_animations.end())
		{
			if (m_currentAnimation == it->second.get())
			{
				m_currentAnimation = nullptr;
			}
			if (m_targetAnimation == it->second.get())
			{
				m_targetAnimation = nullptr;
				m_isBlending = false;
			}
			m_animations.erase(it);
		}

		m_animationPaths.erase(name);

		if (m_currentAnimation == nullptr && !m_animations.empty())
		{
			m_currentAnimation = m_animations.begin()->second.get();
		}
	}

	void SkinnedModelComponent::ChangeAnimation(const std::string& name)
	{
		auto it = m_animations.find(name);
		if (it != m_animations.end())
		{
			m_currentAnimation = it->second.get();
			m_targetAnimation = nullptr;
			m_isBlending = false;
			ResetRootMotionTracking();
			if (m_enableRootMotion && m_currentAnimation)
			{
				int rootIdx = GetRootBoneIndex();
				if (rootIdx >= 0)
				{
					m_initialRootTranslation = m_currentAnimation->GetRootTranslationAtKey(rootIdx, 0);
				}
			}
		}
	}

	void SkinnedModelComponent::CrossfadeAnimation(const std::string& name, float duration, bool forceRestart)
	{
		std::unordered_map<std::string, std::unique_ptr<DX::AnimationSDKMESH>>::iterator it =
			m_animations.find(name);

		if (it == m_animations.end()) return;

		DX::AnimationSDKMESH* requestedAnim = it->second.get();

		if (m_currentAnimation == nullptr)
		{
			m_currentAnimation = requestedAnim;
			ResetRootMotionTracking();
			if (m_enableRootMotion && m_currentAnimation)
			{
				int rootIdx = GetRootBoneIndex();
				if (rootIdx >= 0)
				{
					m_initialRootTranslation = m_currentAnimation->GetRootTranslationAtKey(rootIdx, 0);
				}
			}
			return;
		}

		// Prevent crossfading into the exact same animation clip (which would cause double-speed updates)
		if (m_currentAnimation == requestedAnim)
		{
			if (!m_isBlending)
			{
				if (forceRestart)
				{
					m_currentAnimation->SetAnimTime(0.0f);
					m_prevAnimTime = 0.0;
					m_hasPrevRootTranslation = false;
				}
				return;
			}
			else if (m_targetAnimation == requestedAnim)
			{
				return;
			}
			else
			{
				// Currently blending away, but requested to stay on current: cancel the blend
				m_targetAnimation = nullptr;
				m_isBlending = false;
				return;
			}
		}

		if (m_isBlending && m_targetAnimation == requestedAnim) return;

		if (m_model)
		{
			for (size_t i = 0; i < m_model->bones.size(); ++i)
			{
				m_shapShotBones[i] = m_drawBones[i];
			}

			// PREVENT RACE CONDITION: If another component reads GetCurrentLocalBones() 
			// before SkinnedModelComponent::Update() runs, m_blendedLocalBones would be uninitialized garbage.
			const DirectX::SimpleMath::Matrix* currentLocal = GetCurrentLocalBones();
			if (currentLocal)
			{
				for (size_t i = 0; i < m_model->bones.size(); ++i)
				{
					m_blendedLocalBones[i] = currentLocal[i];
				}
			}
		}

		// Begin Crossfade
		m_isBlending = true;
		m_blendTimer = 0.0f;
		m_blendDuration = duration;

		m_targetAnimation = it->second.get();
		if (forceRestart)
		{
			m_targetAnimation->SetAnimTime(0.0f);
		}

		m_targetHasPrevRootTranslation = false;
		m_targetPrevAnimTime = m_targetAnimation->GetAnimTime();
		if (m_enableRootMotion && m_targetAnimation)
		{
			int rootIdx = GetRootBoneIndex();
			if (rootIdx >= 0)
			{
				m_targetInitialRootTranslation = m_targetAnimation->GetRootTranslationAtKey(rootIdx, 0);
			}
		}
	}

	float SkinnedModelComponent::GetAnimationDuration(const std::string& name) const
	{
		auto it = m_animations.find(name);
		if (it != m_animations.end() && it->second)
		{
			return static_cast<float>(it->second->GetEndTime());
		}
		return 0.0f;
	}

	bool SkinnedModelComponent::HasAnimation(const std::string& name) const
	{
		return m_animations.find(name) != m_animations.end();
	}
		
	void SkinnedModelComponent::OnInspectorGUI(GameContext& gameContext)
	{
			
		if (ImGui::CollapsingHeader("Skinned Model Component", ImGuiTreeNodeFlags_DefaultOpen))
		{
			HWND windowHandle = gameContext.deviceResources.GetWindow();
			if (!m_lastError.empty()) ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", m_lastError.c_str());
			ImGui::Checkbox("Visible", &m_isVisible);

			ImGui::Separator();
			ImGui::Text("Root Motion Settings:");
			ImGui::Checkbox("Enable Root Motion", &m_enableRootMotion);
			ImGui::Checkbox("Extract Vertical (Y) Motion", &m_rootMotionExtractY);

			char rootBoneBuf[128] = { 0 };
			std::string narrowRootBone(m_rootBoneName.begin(), m_rootBoneName.end());
			strncpy_s(rootBoneBuf, narrowRootBone.c_str(), sizeof(rootBoneBuf) - 1);
			if (ImGui::InputText("Root Bone Name", rootBoneBuf, sizeof(rootBoneBuf)))
			{
				std::string s(rootBoneBuf);
				m_rootBoneName = std::wstring(s.begin(), s.end());
			}

			int rootIndex = GetRootBoneIndex();
			std::string detectedName = GetBoneName(rootIndex);
			ImGui::Text("Detected Root Bone: [%d] %s", rootIndex, detectedName.c_str());
			ImGui::Text("Root Motion Delta: (%.3f, %.3f, %.3f)", m_rootMotionDelta.x, m_rootMotionDelta.y, m_rootMotionDelta.z);

			if (m_owner != nullptr)
			{
				std::vector<SkinnedModelComponent*> allModels = m_owner->GetComponents<SkinnedModelComponent>();
				if (allModels.size() > 1)
				{
					ImGui::Text("Attached Skinned Models: %zu (Active: %d)", allModels.size(), m_owner->GetActiveSkinnedModelIndex());
					if (ImGui::Button("Toggle Active Model (Key: M)"))
					{
						m_owner->ToggleSkinnedModel();
					}
				}
				else
				{
					if (ImGui::Button("Toggle Root Motion (Key: M)"))
					{
						m_owner->ToggleSkinnedModel();
					}
				}
			}

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

			// Texture Dir Editor
			std::string texDirStr = std::string(m_textureDir.begin(), m_textureDir.end());
			if (ImGui::InputText("Texture Dir", &texDirStr))
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

			ImGui::Separator();
			ImGui::Text("Loaded Animations:");

			std::string animToRemove = "";
			for (auto& pair : m_animations)
			{
				ImGui::PushID(pair.first.c_str());
				if (ImGui::Button("Play"))
				{
					CrossfadeAnimation(pair.first, 0.2f, true);
				}
				ImGui::SameLine();
				ImGui::Text("%s", pair.first.c_str());
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.8f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
				if (ImGui::Button("Remove"))
				{
					animToRemove = pair.first;
				}
				ImGui::PopStyleColor(2);
				ImGui::PopID();
			}

			if (!animToRemove.empty())
			{
				RemoveAnimation(animToRemove);
			}

			ImGui::Separator();
			ImGui::Text("Load New Animation:");
			
			static std::string newAnimName = "";
			ImGui::InputText("Anim Name", &newAnimName);
			
			if (ImGui::Button("Browse Animation..."))
			{
				std::wstring file = HEIN::EditorUtils::OpenFileDialog(L"Animation Files\0*.sdkmesh_anim\0All Files\0*.*\0", windowHandle);
				if (!file.empty())
				{
					std::string name = newAnimName;
					if (name.empty()) name = "Anim" + std::to_string(m_animations.size());
					LoadAnimation(name, HEIN::EditorUtils::MakeRelativePath(file).c_str());
				}
			}
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
    data["EnableRootMotion"] = m_enableRootMotion;
    data["RootMotionExtractY"] = m_rootMotionExtractY;
    std::string narrowRootBone(m_rootBoneName.begin(), m_rootBoneName.end());
    data["RootBoneName"] = narrowRootBone;
    data["IsVisible"] = m_isVisible;

    nlohmann::json animsArray = nlohmann::json::array();
    for (const auto& pair : m_animationPaths)
    {
        nlohmann::json animData;
        animData["Name"] = pair.first;
        std::string pathStr(pair.second.begin(), pair.second.end());
        animData["Path"] = pathStr;
        animsArray.push_back(animData);
    }
    data["Animations"] = animsArray;

    return data;
}

void HEIN::SkinnedModelComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("EnableRootMotion")) m_enableRootMotion = data["EnableRootMotion"];
    if (data.contains("RootMotionExtractY")) m_rootMotionExtractY = data["RootMotionExtractY"];
    if (data.contains("RootBoneName"))
    {
        std::string narrowRootBone = data["RootBoneName"];
        m_rootBoneName = std::wstring(narrowRootBone.begin(), narrowRootBone.end());
    }
    if (data.contains("IsVisible")) m_isVisible = data["IsVisible"];
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
    if (data.contains("Animations"))
    {
        for (const auto& animData : data["Animations"])
        {
            std::string name = animData["Name"];
            std::string pathStr = animData["Path"];
            m_animationPaths[name] = std::wstring(pathStr.begin(), pathStr.end());
        }
    }
}

void HEIN::SkinnedModelComponent::InitializeAfterDeserialize(GameContext& gameContext)
{
    if (!m_modelPath.empty())
    {
        if (m_textureDir.empty())
        {
            std::filesystem::path p(m_modelPath);
            m_textureDir = p.parent_path().wstring() + L"/";
        }
        Initialize(gameContext, m_modelPath.c_str(), m_textureDir.c_str());

        for (const auto& pair : m_animationPaths)
        {
            LoadAnimation(pair.first, pair.second.c_str());
        }
    }
}


void HEIN::SkinnedModelComponent::OverrideBones(const DirectX::SimpleMath::Matrix* localBones)
{
	if (!m_model) return;

	// Safely cast the SimpleMath matrix array to the raw XMMATRIX array DirectXTK expects
	const DirectX::XMMATRIX* rawLocalBones = reinterpret_cast<const DirectX::XMMATRIX*>(localBones);

	// Converts local matrices down the hierarchy into absolute world-space bone matrices
	m_model->CopyAbsoluteBoneTransforms(m_model->bones.size(), rawLocalBones, m_drawBones.get());

	// Copy the draw bones to the skin bones array
	for (size_t i = 0; i < m_model->bones.size(); i++)
	{
		m_skinBones[i] = m_drawBones[i];
	}

	// Evaluate inverse bind pose transformation using active animation frame matrices
	if (m_currentAnimation != nullptr)
	{
		m_currentAnimation->ApplySkinMatrix(*m_model, m_model->bones.size(), m_skinBones.get());
	}
}
