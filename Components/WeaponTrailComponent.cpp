#include "pch.h"
#include "WeaponTrailComponent.h"
#include "Framework/GameContext.h"
#include "Entities/Actor.h"
#include "Entities/ActorManager.h"
#include "Components/TransformComponent.h"
#include "Components/SocketAttachmentComponent.h"
#include "Components/StaticModelComponent.h"
#include "Components/DamageDealerComponent.h"
#include "Components/HealthComponent.h"
#include "Components/ColliderComponent/OBBColliderComponent.h"
#include "Components/ColliderComponent/CapsuleColliderComponent.h"
#include "DebugingTools/DebugCollisionRenderer.h"
#include "Message/Messenger.h"
#include "Effect/ReadData.h"
#include <ImGui/imgui.h>
#include <d3dcompiler.h>
#include <cmath>
#include <algorithm>
#include <vector>

namespace HEIN
{
	namespace
	{
		ID3D11Device* s_trailCachedDevice = nullptr;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> s_sharedTrailVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> s_sharedTrailPS;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> s_sharedTrailInputLayout;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> s_sharedTrailSampler;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> s_sharedNoiseSRV;

		void EnsureSharedTrailResources(ID3D11Device* device)
		{
			if (!device) return;

			if (s_trailCachedDevice != device)
			{
				s_sharedTrailVS.Reset();
				s_sharedTrailPS.Reset();
				s_sharedTrailInputLayout.Reset();
				s_sharedTrailSampler.Reset();
				s_sharedNoiseSRV.Reset();
				s_trailCachedDevice = device;
			}

			if (!s_sharedTrailVS || !s_sharedTrailPS || !s_sharedTrailInputLayout)
			{
				bool loadedCso = false;
				std::vector<uint8_t> vsData, psData;
				try
				{
					vsData = DX::ReadData(L"Resources/Shaders/WeaponTrail_VS.cso");
					psData = DX::ReadData(L"Resources/Shaders/WeaponTrail_PS.cso");
					loadedCso = true;
				}
				catch (...)
				{
					try
					{
						vsData = DX::ReadData(L"WeaponTrail_VS.cso");
						psData = DX::ReadData(L"WeaponTrail_PS.cso");
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
					HRESULT hr = D3DCompileFromFile(
						L"../External/Engine/Shaders/WeaponTrail_VS.hlsl",
						nullptr,
						D3D_COMPILE_STANDARD_FILE_INCLUDE,
						"main",
						"vs_5_0",
						0,
						0,
						&vsBlob,
						&errorBlob
					);
					if (FAILED(hr))
					{
						hr = D3DCompileFromFile(
							L"External/Engine/Shaders/WeaponTrail_VS.hlsl",
							nullptr,
							D3D_COMPILE_STANDARD_FILE_INCLUDE,
							"main",
							"vs_5_0",
							0,
							0,
							&vsBlob,
							&errorBlob
						);
					}
					if (FAILED(hr))
					{
						if (errorBlob)
						{
							OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
						}
						return;
					}

					hr = D3DCompileFromFile(
						L"../External/Engine/Shaders/WeaponTrail_PS.hlsl",
						nullptr,
						D3D_COMPILE_STANDARD_FILE_INCLUDE,
						"main",
						"ps_5_0",
						0,
						0,
						&psBlob,
						&errorBlob
					);
					if (FAILED(hr))
					{
						hr = D3DCompileFromFile(
							L"External/Engine/Shaders/WeaponTrail_PS.hlsl",
							nullptr,
							D3D_COMPILE_STANDARD_FILE_INCLUDE,
							"main",
							"ps_5_0",
							0,
							0,
							&psBlob,
							&errorBlob
						);
					}
					if (FAILED(hr))
					{
						if (errorBlob)
						{
							OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
						}
						return;
					}

					vsBytecode = vsBlob->GetBufferPointer();
					vsBytecodeSize = vsBlob->GetBufferSize();
					psBytecode = psBlob->GetBufferPointer();
					psBytecodeSize = psBlob->GetBufferSize();
				}

				device->CreateVertexShader(vsBytecode, vsBytecodeSize, nullptr, s_sharedTrailVS.ReleaseAndGetAddressOf());
				device->CreatePixelShader(psBytecode, psBytecodeSize, nullptr, s_sharedTrailPS.ReleaseAndGetAddressOf());

				static const D3D11_INPUT_ELEMENT_DESC trailInputLayoutDesc[] = {
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }
				};
				device->CreateInputLayout(
					trailInputLayoutDesc,
					static_cast<UINT>(_countof(trailInputLayoutDesc)),
					vsBytecode,
					vsBytecodeSize,
					s_sharedTrailInputLayout.ReleaseAndGetAddressOf()
				);
			}

			if (!s_sharedTrailSampler)
			{
				D3D11_SAMPLER_DESC sampDesc = {};
				sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
				sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
				sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
				sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
				device->CreateSamplerState(&sampDesc, s_sharedTrailSampler.ReleaseAndGetAddressOf());
			}

			if (!s_sharedNoiseSRV)
			{
				if (!TryLoadCachedTextureFile(device, L"Resources/Textures/waternoise.dds", s_sharedNoiseSRV.ReleaseAndGetAddressOf()))
				{
					// Procedural 64x64 anisotropic streaky noise texture fallback
					constexpr int TEX_SIZE = 64;
					std::array<uint32_t, TEX_SIZE * TEX_SIZE> pixels{};
					for (int y = 0; y < TEX_SIZE; ++y)
					{
						uint32_t rowSeed = static_cast<uint32_t>(y * 1973u + 9277u);
						for (int x = 0; x < TEX_SIZE; ++x)
						{
							uint32_t h = (rowSeed ^ static_cast<uint32_t>(x * 269u)) * 2654435761u;
							float streak = 0.55f + 0.45f * std::sin(static_cast<float>(y) * 0.65f + static_cast<float>(x) * 0.18f);
							float fine = static_cast<float>((h >> 16) & 0xFF) / 255.0f;
							uint8_t val = static_cast<uint8_t>(std::clamp((streak * 0.7f + fine * 0.3f) * 255.0f, 32.0f, 255.0f));
							pixels[y * TEX_SIZE + x] = (0xFFu << 24) | (val << 16) | (val << 8) | val;
						}
					}

					D3D11_TEXTURE2D_DESC desc = {};
					desc.Width = TEX_SIZE;
					desc.Height = TEX_SIZE;
					desc.MipLevels = 1;
					desc.ArraySize = 1;
					desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
					desc.SampleDesc.Count = 1;
					desc.Usage = D3D11_USAGE_IMMUTABLE;
					desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

					D3D11_SUBRESOURCE_DATA initData = {};
					initData.pSysMem = pixels.data();
					initData.SysMemPitch = TEX_SIZE * sizeof(uint32_t);

					Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
					if (SUCCEEDED(device->CreateTexture2D(&desc, &initData, tex.GetAddressOf())))
					{
						device->CreateShaderResourceView(tex.Get(), nullptr, s_sharedNoiseSRV.ReleaseAndGetAddressOf());
					}
				}
			}
		}
	}

	WeaponTrailComponent::WeaponTrailComponent(Actor* owner, ActorManager* manager)
		: IComponent(owner)
		, m_actorManager(manager)
	{
	}

	WeaponTrailComponent::~WeaponTrailComponent()
	{
		if (Messenger* msg = Messenger::GetInstance())
		{
			msg->UnRegisterObserver(this);
		}
	}

	void WeaponTrailComponent::Start()
	{
		if (!m_hasCustomOffsets)
		{
			AutoFitOffsetsFromWeapon();
		}
		RegisterToWielderMessages();
	}

	void WeaponTrailComponent::RegisterToWielderMessages()
	{
		if (!m_owner) return;

		ActorID listenID = m_owner->GetOwnerID();
		if (m_actorManager && listenID != INVALID_ACTOR_ID && !m_actorManager->GetActor(listenID))
		{
			listenID = INVALID_ACTOR_ID;
		}

		if (listenID == INVALID_ACTOR_ID && m_owner->GetParentID() != INVALID_ACTOR_ID)
		{
			listenID = m_owner->GetParentID();
			if (m_actorManager && !m_actorManager->GetActor(listenID))
			{
				listenID = INVALID_ACTOR_ID;
			}
		}

		if (listenID == INVALID_ACTOR_ID)
		{
			if (auto* socketAttach = m_owner->GetComponent<SocketAttachmentComponent>())
			{
				listenID = socketAttach->GetSocketOwnerActorID();
				if (m_actorManager && listenID != INVALID_ACTOR_ID && !m_actorManager->GetActor(listenID))
				{
					listenID = INVALID_ACTOR_ID;
				}
			}
		}

		if (listenID == INVALID_ACTOR_ID && m_actorManager != nullptr && m_owner->GetActorType() != ActorType::Enemy)
		{
			if (Actor* player = m_actorManager->GetFirstActorByType(ActorType::Player))
			{
				listenID = player->GetID();
			}
		}

		if (listenID != INVALID_ACTOR_ID && listenID != m_registeredListenID)
		{
			if (m_registeredListenID != INVALID_ACTOR_ID && Messenger::GetInstance())
			{
				Messenger::GetInstance()->UnRegisterObserver(this);
			}
			Messenger::GetInstance()->Register(listenID, this);
			m_registeredListenID = listenID;
		}
	}

	void WeaponTrailComponent::OnMessageAccepted(Message::MessageID messageID)
	{
		if (messageID == Message::SET_WEAPON_ACTIVE)
		{
			if (!m_isEmitting)
			{
				ClearKnots();
			}
			m_isEmitting = true;
		}
		else if (messageID == Message::SET_WEAPON_INACTIVE)
		{
			m_isEmitting = false;
		}
		else if (messageID == Message::RESET_WEAPON_SWING)
		{
			// Clear previous swing's knots so chained combos start a fresh slash arc
			ClearKnots();
		}
	}

	void WeaponTrailComponent::PushKnot(const DirectX::SimpleMath::Vector3& baseWorld, const DirectX::SimpleMath::Vector3& tipWorld)
	{
		m_head = (m_head + MAX_KNOTS - 1) % MAX_KNOTS;
		m_knots[m_head].baseWorld = baseWorld;
		m_knots[m_head].tipWorld  = tipWorld;
		m_knots[m_head].age       = 0.0f;

		if (m_count < MAX_KNOTS)
		{
			++m_count;
		}
	}

	WeaponTrailComponent::TrailKnot& WeaponTrailComponent::GetKnot(size_t chronologicalIndex)
	{
		return m_knots[(m_head + chronologicalIndex) % MAX_KNOTS];
	}

	const WeaponTrailComponent::TrailKnot& WeaponTrailComponent::GetKnot(size_t chronologicalIndex) const
	{
		return m_knots[(m_head + chronologicalIndex) % MAX_KNOTS];
	}

	void WeaponTrailComponent::ClearKnots()
	{
		m_head = 0;
		m_count = 0;
	}

	void WeaponTrailComponent::Update(float /*deltaTime*/)
	{
		// Intentionally empty: all sampling occurs in LateUpdate after SocketAttachmentComponent
	}

	void WeaponTrailComponent::LateUpdate(float deltaTime)
	{
		if (!m_owner || !m_owner->IsActive()) return;

		RegisterToWielderMessages();

		// Resolve wielder actor to check Hitstop (Hit Freeze) state
		Actor* wielder = nullptr;
		if (m_actorManager && m_registeredListenID != INVALID_ACTOR_ID)
		{
			wielder = m_actorManager->GetActor(m_registeredListenID);
		}

		// During Hitstop, freeze the slash arc in mid-air without aging or pushing duplicate knots
		if (wielder && wielder->IsHitstopped())
		{
			return;
		}

		// Early-out if completely idle and no active knots exist in the ring buffer (0 CPU cost)
		bool shouldEmit = m_isEmitting || m_alwaysEmitPreview;
		if (!shouldEmit && m_count == 0)
		{
			return;
		}

		// Advance shader time and age existing knots
		float effectiveDt = (deltaTime > 0.0f) ? deltaTime : (m_alwaysEmitPreview ? 0.016f : 0.0f);
		m_elapsedTime += effectiveDt;

		if (effectiveDt > 0.0f && m_count > 0)
		{
			for (size_t i = 0; i < m_count; ++i)
			{
				GetKnot(i).age += effectiveDt;
			}

			// O(1) tail expiration from ring buffer
			while (m_count > 0 && GetKnot(m_count - 1).age >= m_lifetime)
			{
				--m_count;
			}
		}

		if (!shouldEmit)
		{
			return;
		}

		// Guarantee socket parent transform is up-to-date for this frame regardless of component order
		if (auto* socketAttach = m_owner->GetComponent<SocketAttachmentComponent>())
		{
			socketAttach->LateUpdate(0.0f);
		}

		auto* transform = m_owner->GetComponent<TransformComponent>();
		if (!transform) return;

		const DirectX::SimpleMath::Matrix worldMat = transform->GetWorldMatrix();
		const DirectX::SimpleMath::Vector3 baseWorld = DirectX::SimpleMath::Vector3::Transform(m_baseOffset, worldMat);
		const DirectX::SimpleMath::Vector3 tipWorld  = DirectX::SimpleMath::Vector3::Transform(m_tipOffset, worldMat);

		if (m_count == 0)
		{
			PushKnot(baseWorld, tipWorld);
			return;
		}

		const float minDistSq = m_minVertexDistance * m_minVertexDistance;
		const float tipDistSq  = DirectX::SimpleMath::Vector3::DistanceSquared(tipWorld, GetKnot(0).tipWorld);
		const float baseDistSq = DirectX::SimpleMath::Vector3::DistanceSquared(baseWorld, GetKnot(0).baseWorld);

		if (tipDistSq >= minDistSq || baseDistSq >= minDistSq)
		{
			PushKnot(baseWorld, tipWorld);
		}
		else
		{
			// Keep the leading knot welded flush to the physical blade edge
			GetKnot(0).baseWorld = baseWorld;
			GetKnot(0).tipWorld  = tipWorld;
			GetKnot(0).age       = 0.0f;
		}
	}

	void WeaponTrailComponent::EnsureGPUResources(ID3D11Device* device)
	{
		if (!device) return;

		EnsureSharedTrailResources(device);

		if (!m_dynamicVB)
		{
			D3D11_BUFFER_DESC vbDesc = {};
			vbDesc.Usage = D3D11_USAGE_DYNAMIC;
			vbDesc.ByteWidth = static_cast<UINT>(sizeof(TrailVertex) * MAX_VERTICES);
			vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			device->CreateBuffer(&vbDesc, nullptr, m_dynamicVB.ReleaseAndGetAddressOf());
		}

		if (!m_cbTrail)
		{
			D3D11_BUFFER_DESC cbDesc = {};
			cbDesc.Usage = D3D11_USAGE_DYNAMIC;
			cbDesc.ByteWidth = sizeof(CB_WeaponTrail);
			cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			device->CreateBuffer(&cbDesc, nullptr, m_cbTrail.ReleaseAndGetAddressOf());
		}
	}

	void WeaponTrailComponent::DrawTransparent(
		GameContext& gameContext,
		const DirectX::SimpleMath::Matrix& world,
		const DirectX::SimpleMath::Matrix& view,
		const DirectX::SimpleMath::Matrix& proj
	)
	{
		if (!m_owner || !m_owner->IsVisible() || !m_owner->IsActive()) return;

		if (!m_actorManager && gameContext.actorManager)
		{
			m_actorManager = gameContext.actorManager;
		}

		// Draw visual blade-span debug line in F3 Editor Scene View when colliders are shown
		if (gameContext.isEditorMode && gameContext.showColliders && m_showDebugBladeLine && gameContext.debugCollisionRenderer != nullptr)
		{
			const DirectX::SimpleMath::Vector3 dbgBase = DirectX::SimpleMath::Vector3::Transform(m_baseOffset, world);
			const DirectX::SimpleMath::Vector3 dbgTip  = DirectX::SimpleMath::Vector3::Transform(m_tipOffset, world);
			gameContext.debugCollisionRenderer->QueueLine(
				dbgBase,
				dbgTip,
				DirectX::SimpleMath::Color(0.0f, 1.0f, 1.0f, 1.0f)
			);
		}

		if (m_count < 2) return;

		ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
		ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
		if (!device || !context) return;

		EnsureGPUResources(device);
		if (!m_dynamicVB || !m_cbTrail || !s_sharedTrailVS || !s_sharedTrailPS || !s_sharedTrailInputLayout)
		{
			return;
		}

		// Stream Catmull-Rom subdivided triangle strip into Dynamic Vertex Buffer
		D3D11_MAPPED_SUBRESOURCE mappedVB = {};
		if (FAILED(context->Map(m_dynamicVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVB)))
		{
			return;
		}

		auto* outVerts = static_cast<TrailVertex*>(mappedVB.pData);
		size_t vertexCount = 0;

		const int subdivs = std::clamp(m_subdivisions, 1, MAX_SUBDIVISIONS);
		const float invSubdivs = 1.0f / static_cast<float>(subdivs);
		const float invLifetime = 1.0f / std::max(0.01f, m_lifetime);

		for (size_t i = 0; i < m_count - 1; ++i)
		{
			const TrailKnot& k0 = GetKnot((i == 0) ? 0 : (i - 1));
			const TrailKnot& k1 = GetKnot(i);
			const TrailKnot& k2 = GetKnot(i + 1);
			const TrailKnot& k3 = GetKnot((i + 2 < m_count) ? (i + 2) : (m_count - 1));

			const float len1 = (k1.tipWorld - k1.baseWorld).Length();
			const float len2 = (k2.tipWorld - k2.baseWorld).Length();

			const int steps = (i == m_count - 2) ? subdivs : (subdivs - 1);
			for (int s = 0; s <= steps; ++s)
			{
				if (vertexCount + 2 > MAX_VERTICES) break;

				const float t = static_cast<float>(s) * invSubdivs;

				DirectX::SimpleMath::Vector3 interpBase = DirectX::SimpleMath::Vector3::CatmullRom(
					k0.baseWorld, k1.baseWorld, k2.baseWorld, k3.baseWorld, t
				);
				DirectX::SimpleMath::Vector3 interpTip = DirectX::SimpleMath::Vector3::CatmullRom(
					k0.tipWorld, k1.tipWorld, k2.tipWorld, k3.tipWorld, t
				);

				// Preserve exact blade span length across wide-angle slashes
				DirectX::SimpleMath::Vector3 bladeVec = interpTip - interpBase;
				const float currentLenSq = bladeVec.LengthSquared();
				const float targetLen = len1 + (len2 - len1) * t;
				if (currentLenSq > 0.0001f && targetLen > 0.001f)
				{
					bladeVec *= (targetLen / std::sqrt(currentLenSq));
					interpTip = interpBase + bladeVec;
				}

				const float interpAge = k1.age + (k2.age - k1.age) * t;
				const float u = std::clamp(interpAge * invLifetime, 0.0f, 1.0f);

				// Vertex 0: Blade Base (v = 0.0)
				outVerts[vertexCount].pos   = interpBase;
				outVerts[vertexCount].uv    = DirectX::SimpleMath::Vector2(u, 0.0f);
				outVerts[vertexCount].color = DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
				++vertexCount;

				// Vertex 1: Blade Tip (v = 1.0)
				outVerts[vertexCount].pos   = interpTip;
				outVerts[vertexCount].uv    = DirectX::SimpleMath::Vector2(u, 1.0f);
				outVerts[vertexCount].color = DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
				++vertexCount;
			}
		}

		context->Unmap(m_dynamicVB.Get(), 0);

		if (vertexCount < 4) return;

		// Check if wielder is in Phase 2 (Enraged Berserker <= 50% HP) for dynamic boss ember boost
		float phaseMultiplier = 1.0f;
		DirectX::SimpleMath::Vector4 activeCore = m_coreColor;
		DirectX::SimpleMath::Vector4 activeEdge = m_edgeColor;
		if (m_enablePhase2Boost && m_actorManager && m_registeredListenID != INVALID_ACTOR_ID)
		{
			if (Actor* wielder = m_actorManager->GetActor(m_registeredListenID))
			{
				if (auto* health = wielder->GetComponent<HealthComponent>())
				{
					if (health->GetMaxHealth() > 0 &&
						static_cast<float>(health->GetCurrentHealth()) / static_cast<float>(health->GetMaxHealth()) <= 0.5f)
					{
						phaseMultiplier = 1.45f;
						activeCore = DirectX::SimpleMath::Vector4(1.0f, 0.85f, 0.55f, std::min(1.0f, m_coreColor.w * 1.15f));
						activeEdge = DirectX::SimpleMath::Vector4(1.0f, 0.18f, 0.05f, std::min(1.0f, m_edgeColor.w * 1.15f));
					}
				}
			}
		}

		// Update Per-Weapon Constant Buffer (b0)
		D3D11_MAPPED_SUBRESOURCE mappedCB = {};
		if (SUCCEEDED(context->Map(m_cbTrail.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
		{
			auto* cb = static_cast<CB_WeaponTrail*>(mappedCB.pData);
			cb->ViewProj             = DirectX::XMMatrixTranspose(view * proj);
			cb->CoreColor            = activeCore;
			cb->EdgeColor            = activeEdge;
			cb->EmissiveIntensity    = m_emissiveIntensity * phaseMultiplier;
			cb->TipFalloffPower      = m_tipFalloffPower;
			cb->NoiseScale           = m_noiseScale;
			cb->NoiseScrollSpeed     = m_noiseScrollSpeed;
			cb->Time                 = m_elapsedTime;
			cb->ErosionStrength      = m_erosionStrength;
			cb->LeadingEdgeSharpness = m_leadingEdgeSharpness;
			cb->TrailPadding         = 0.0f;
			context->Unmap(m_cbTrail.Get(), 0);
		}

		// Configure D3D11 Pipeline State for Double-Sided Depth-Tested Ribbon VFX
		UINT stride = sizeof(TrailVertex);
		UINT offset = 0;
		context->IASetInputLayout(s_sharedTrailInputLayout.Get());
		context->IASetVertexBuffers(0, 1, m_dynamicVB.GetAddressOf(), &stride, &offset);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		context->VSSetShader(s_sharedTrailVS.Get(), nullptr, 0);
		context->VSSetConstantBuffers(0, 1, m_cbTrail.GetAddressOf());

		context->PSSetShader(s_sharedTrailPS.Get(), nullptr, 0);
		context->PSSetConstantBuffers(0, 1, m_cbTrail.GetAddressOf());

		ID3D11SamplerState* samplers[] = { s_sharedTrailSampler.Get() };
		context->PSSetSamplers(0, 1, samplers);

		ID3D11ShaderResourceView* srvs[] = { s_sharedNoiseSRV.Get() };
		context->PSSetShaderResources(0, 1, srvs);

		context->RSSetState(gameContext.commonStates.CullNone());
		context->OMSetDepthStencilState(gameContext.commonStates.DepthRead(), 0);
		context->OMSetBlendState(
			(m_blendMode == TrailBlendMode::Additive)
				? gameContext.commonStates.Additive()
				: gameContext.commonStates.AlphaBlend(),
			nullptr,
			0xFFFFFFFF
		);

		context->Draw(static_cast<UINT>(vertexCount), 0);

		// Restore default pipeline states so subsequent passes (UI / Debug) are cleanly isolated
		context->RSSetState(gameContext.commonStates.CullCounterClockwise());
		context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
		context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
	}

	void WeaponTrailComponent::AutoFitOffsetsFromWeapon()
	{
		if (!m_owner) return;

		// Prefer OBBColliderComponent (used by Knight & Vessel Swords)
		if (auto* obb = m_owner->GetComponent<OBBColliderComponent>())
		{
			const DirectX::SimpleMath::Vector3 center  = obb->GetOffset();
			const DirectX::SimpleMath::Vector3 extents = obb->GetExtents();

			DirectX::SimpleMath::Vector3 axis = DirectX::SimpleMath::Vector3::Zero;
			if (extents.y >= extents.x && extents.y >= extents.z)
			{
				axis = DirectX::SimpleMath::Vector3(0.0f, extents.y, 0.0f);
			}
			else if (extents.z >= extents.x && extents.z >= extents.y)
			{
				axis = DirectX::SimpleMath::Vector3(0.0f, 0.0f, extents.z);
			}
			else
			{
				axis = DirectX::SimpleMath::Vector3(extents.x, 0.0f, 0.0f);
			}

			const DirectX::SimpleMath::Vector3 pA = center - axis;
			const DirectX::SimpleMath::Vector3 pB = center + axis;

			// Point closer to the hand origin (0,0,0) is the hilt/base; further point is the blade tip
			if (pA.LengthSquared() <= pB.LengthSquared())
			{
				m_baseOffset = pA;
				m_tipOffset  = pB;
			}
			else
			{
				m_baseOffset = pB;
				m_tipOffset  = pA;
			}
			return;
		}

		// Fallback to CapsuleColliderComponent (used by Boss Axe head in GameScene.json)
		if (auto* cap = m_owner->GetComponent<CapsuleColliderComponent>())
		{
			const DirectX::SimpleMath::Vector3 center = cap->GetOffset();
			const float reach = center.Length();
			if (reach > 0.001f)
			{
				const DirectX::SimpleMath::Vector3 dir = center / reach;
				const float outerSpan = cap->GetRadius() + cap->GetHeight() * 0.5f;
				m_baseOffset = center - dir * (outerSpan * 1.6f);
				m_tipOffset  = center + dir * (outerSpan * 0.95f);
				return;
			}
		}

		// Fallback: default weapon blade length if no collider exists or offsets are collapsed
		if (!m_hasCustomOffsets || (m_tipOffset - m_baseOffset).LengthSquared() < 1.0f)
		{
			m_baseOffset = DirectX::SimpleMath::Vector3(0.0f, 140.0f, 0.0f);
			m_tipOffset  = DirectX::SimpleMath::Vector3(0.0f, 580.0f, 0.0f);
			m_hasCustomOffsets = true;
		}
	}

	void WeaponTrailComponent::ApplyTarnishedGoldPreset()
	{
		m_coreColor            = DirectX::SimpleMath::Vector4(1.0f, 0.97f, 0.86f, 0.95f);
		m_edgeColor            = DirectX::SimpleMath::Vector4(0.88f, 0.68f, 0.30f, 0.85f);
		m_emissiveIntensity    = 2.3f;
		m_tipFalloffPower      = 1.6f;
		m_noiseScale           = 2.0f;
		m_noiseScrollSpeed     = 2.8f;
		m_erosionStrength      = 0.78f;
		m_leadingEdgeSharpness = 7.5f;
		m_lifetime             = 0.20f;
		m_blendMode            = TrailBlendMode::Additive;
	}

	void WeaponTrailComponent::ApplyColdSteelPreset()
	{
		m_coreColor            = DirectX::SimpleMath::Vector4(0.92f, 0.97f, 1.0f, 0.92f);
		m_edgeColor            = DirectX::SimpleMath::Vector4(0.42f, 0.68f, 0.95f, 0.78f);
		m_emissiveIntensity    = 2.0f;
		m_tipFalloffPower      = 1.75f;
		m_noiseScale           = 2.2f;
		m_noiseScrollSpeed     = 3.2f;
		m_erosionStrength      = 0.82f;
		m_leadingEdgeSharpness = 8.0f;
		m_lifetime             = 0.18f;
		m_blendMode            = TrailBlendMode::Additive;
	}

	void WeaponTrailComponent::ApplyBossBloodflamePreset()
	{
		m_coreColor            = DirectX::SimpleMath::Vector4(1.0f, 0.78f, 0.42f, 0.96f);
		m_edgeColor            = DirectX::SimpleMath::Vector4(0.88f, 0.12f, 0.06f, 0.90f);
		m_emissiveIntensity    = 2.5f;
		m_tipFalloffPower      = 1.45f;
		m_noiseScale           = 1.8f;
		m_noiseScrollSpeed     = 2.5f;
		m_erosionStrength      = 0.80f;
		m_leadingEdgeSharpness = 6.0f;
		m_lifetime             = 0.26f;
		m_enablePhase2Boost    = true;
		m_blendMode            = TrailBlendMode::Additive;
	}

	void WeaponTrailComponent::ApplyBlackFlamePreset()
	{
		m_coreColor            = DirectX::SimpleMath::Vector4(0.95f, 0.95f, 0.98f, 0.95f);
		m_edgeColor            = DirectX::SimpleMath::Vector4(0.12f, 0.10f, 0.16f, 0.92f);
		m_emissiveIntensity    = 1.8f;
		m_tipFalloffPower      = 1.5f;
		m_noiseScale           = 2.4f;
		m_noiseScrollSpeed     = 3.0f;
		m_erosionStrength      = 0.88f;
		m_leadingEdgeSharpness = 6.5f;
		m_lifetime             = 0.24f;
		m_blendMode            = TrailBlendMode::AlphaBlend;
	}

	nlohmann::json WeaponTrailComponent::Serialize()
	{
		nlohmann::json data = IComponent::Serialize();
		data["HasCustomOffsets"]     = m_hasCustomOffsets;
		data["BaseOffset"]           = { m_baseOffset.x, m_baseOffset.y, m_baseOffset.z };
		data["TipOffset"]            = { m_tipOffset.x, m_tipOffset.y, m_tipOffset.z };
		data["Lifetime"]             = m_lifetime;
		data["Subdivisions"]         = m_subdivisions;
		data["MinVertexDistance"]    = m_minVertexDistance;
		data["CoreColor"]            = { m_coreColor.x, m_coreColor.y, m_coreColor.z, m_coreColor.w };
		data["EdgeColor"]            = { m_edgeColor.x, m_edgeColor.y, m_edgeColor.z, m_edgeColor.w };
		data["EmissiveIntensity"]    = m_emissiveIntensity;
		data["TipFalloffPower"]      = m_tipFalloffPower;
		data["NoiseScale"]           = m_noiseScale;
		data["NoiseScrollSpeed"]     = m_noiseScrollSpeed;
		data["ErosionStrength"]      = m_erosionStrength;
		data["LeadingEdgeSharpness"] = m_leadingEdgeSharpness;
		data["BlendMode"]            = static_cast<int>(m_blendMode);
		data["EnablePhase2Boost"]    = m_enablePhase2Boost;
		data["ShowDebugBladeLine"]   = m_showDebugBladeLine;
		return data;
	}

	void WeaponTrailComponent::Deserialize(const nlohmann::json& data)
	{
		IComponent::Deserialize(data);
		if (data.contains("HasCustomOffsets")) m_hasCustomOffsets = data["HasCustomOffsets"];
		if (data.contains("BaseOffset") && data["BaseOffset"].is_array() && data["BaseOffset"].size() == 3)
		{
			m_baseOffset = DirectX::SimpleMath::Vector3(data["BaseOffset"][0], data["BaseOffset"][1], data["BaseOffset"][2]);
		}
		if (data.contains("TipOffset") && data["TipOffset"].is_array() && data["TipOffset"].size() == 3)
		{
			m_tipOffset = DirectX::SimpleMath::Vector3(data["TipOffset"][0], data["TipOffset"][1], data["TipOffset"][2]);
		}
		if (data.contains("Lifetime"))             m_lifetime = std::max(0.03f, data["Lifetime"].get<float>());
		if (data.contains("Subdivisions"))         m_subdivisions = std::clamp(data["Subdivisions"].get<int>(), 1, MAX_SUBDIVISIONS);
		if (data.contains("MinVertexDistance"))    m_minVertexDistance = std::max(0.01f, data["MinVertexDistance"].get<float>());
		if (data.contains("CoreColor") && data["CoreColor"].is_array() && data["CoreColor"].size() == 4)
		{
			m_coreColor = DirectX::SimpleMath::Vector4(data["CoreColor"][0], data["CoreColor"][1], data["CoreColor"][2], data["CoreColor"][3]);
		}
		if (data.contains("EdgeColor") && data["EdgeColor"].is_array() && data["EdgeColor"].size() == 4)
		{
			m_edgeColor = DirectX::SimpleMath::Vector4(data["EdgeColor"][0], data["EdgeColor"][1], data["EdgeColor"][2], data["EdgeColor"][3]);
		}
		if (data.contains("EmissiveIntensity"))    m_emissiveIntensity = data["EmissiveIntensity"];
		if (data.contains("TipFalloffPower"))      m_tipFalloffPower = data["TipFalloffPower"];
		if (data.contains("NoiseScale"))           m_noiseScale = data["NoiseScale"];
		if (data.contains("NoiseScrollSpeed"))     m_noiseScrollSpeed = data["NoiseScrollSpeed"];
		if (data.contains("ErosionStrength"))      m_erosionStrength = data["ErosionStrength"];
		if (data.contains("LeadingEdgeSharpness")) m_leadingEdgeSharpness = data["LeadingEdgeSharpness"];
		if (data.contains("BlendMode"))            m_blendMode = static_cast<TrailBlendMode>(data["BlendMode"].get<int>());
		if (data.contains("EnablePhase2Boost"))    m_enablePhase2Boost = data["EnablePhase2Boost"];
		if (data.contains("ShowDebugBladeLine"))   m_showDebugBladeLine = data["ShowDebugBladeLine"];
	}

	void WeaponTrailComponent::OnInspectorGUI(GameContext& /*gameContext*/)
	{
		if (ImGui::CollapsingHeader("WeaponTrailComponent (Elden Ring VFX)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TextDisabled("Active Ring-Buffer Knots: %zu / %zu", m_count, MAX_KNOTS);
			ImGui::Checkbox("Always Emit (Editor Preview)", &m_alwaysEmitPreview);
			ImGui::SameLine();
			ImGui::Checkbox("Show Blade Line", &m_showDebugBladeLine);

			ImGui::Separator();
			ImGui::Text("Elden Ring Style Presets:");
			if (ImGui::Button("Tarnished Gold")) ApplyTarnishedGoldPreset();
			ImGui::SameLine();
			if (ImGui::Button("Cold Steel")) ApplyColdSteelPreset();
			if (ImGui::Button("Boss Bloodflame")) ApplyBossBloodflamePreset();
			ImGui::SameLine();
			if (ImGui::Button("Black Flame")) ApplyBlackFlamePreset();

			ImGui::Separator();
			ImGui::Text("Blade Geometry & Length (Local Weapon Space):");

			DirectX::SimpleMath::Vector3 bladeVec = m_tipOffset - m_baseOffset;
			float currentLength = bladeVec.Length();
			DirectX::SimpleMath::Vector3 bladeDir = (currentLength > 0.001f) ? (bladeVec / currentLength) : DirectX::SimpleMath::Vector3(0, 1, 0);

			ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Current Blade Span: %.1f units", currentLength);

			// Direct Blade Length control: stretches or shrinks the tip along the blade axis
			float editLength = currentLength;
			if (ImGui::DragFloat("Blade Length", &editLength, 1.0f, 1.0f, 3000.0f, "%.1f units"))
			{
				if (editLength > 0.1f)
				{
					m_tipOffset = m_baseOffset + bladeDir * editLength;
					m_hasCustomOffsets = true;
				}
			}

			// Tip Trim: pull tip back into the blade (negative) or extend outward (positive)
			float tipTrim = 0.0f;
			if (ImGui::SliderFloat("Tip Trim (Shorten / Extend)", &tipTrim, -150.0f, 150.0f, "%+.1f units"))
			{
				m_tipOffset = m_baseOffset + bladeDir * std::max(1.0f, currentLength + tipTrim);
				m_hasCustomOffsets = true;
			}

			// Base Shift: move where the trail starts on the blade surface
			float baseShift = 0.0f;
			if (ImGui::SliderFloat("Base Shift (Hilt / Tip)", &baseShift, -100.0f, 100.0f, "%+.1f units"))
			{
				m_baseOffset = m_baseOffset + bladeDir * baseShift;
				m_hasCustomOffsets = true;
			}

			// Direct 3D offset vectors
			if (ImGui::DragFloat3("Base Offset (Hilt)", &m_baseOffset.x, 1.0f))
			{
				m_hasCustomOffsets = true;
			}
			if (ImGui::DragFloat3("Tip Offset (Blade Tip)", &m_tipOffset.x, 1.0f))
			{
				m_hasCustomOffsets = true;
			}
			if (ImGui::Button("Auto-Fit Offsets from Weapon Collider", ImVec2(-1, 24)))
			{
				AutoFitOffsetsFromWeapon();
				m_hasCustomOffsets = true;
			}

			ImGui::Separator();
			ImGui::Text("Spline & Timing:");
			ImGui::SliderFloat("Trail Lifetime (s)", &m_lifetime, 0.05f, 0.60f, "%.2f");
			ImGui::SliderInt("Catmull-Rom Subdivisions", &m_subdivisions, 1, MAX_SUBDIVISIONS);
			ImGui::SliderFloat("Min Sample Distance", &m_minVertexDistance, 0.02f, 2.0f, "%.2f");

			ImGui::Separator();
			ImGui::Text("Shader & Art Direction:");
			ImGui::ColorEdit4("Core Color (Leading Edge)", &m_coreColor.x);
			ImGui::ColorEdit4("Edge Color (Arc Body)", &m_edgeColor.x);
			ImGui::SliderFloat("Emissive Intensity", &m_emissiveIntensity, 0.2f, 6.0f, "%.2f");
			ImGui::SliderFloat("Tip Falloff Power", &m_tipFalloffPower, 0.3f, 4.0f, "%.2f");
			ImGui::SliderFloat("Leading Edge Sharpness", &m_leadingEdgeSharpness, 1.0f, 16.0f, "%.1f");
			ImGui::SliderFloat("Erosion Strength", &m_erosionStrength, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Noise Tiling Scale", &m_noiseScale, 0.2f, 6.0f, "%.2f");
			ImGui::SliderFloat("Noise Scroll Speed", &m_noiseScrollSpeed, 0.0f, 8.0f, "%.2f");

			const char* blendModes[] = { "Additive (Glowing Arc)", "AlphaBlend (Smoke / Black Flame)" };
			int currentBlend = static_cast<int>(m_blendMode);
			if (ImGui::Combo("Blend Mode", &currentBlend, blendModes, IM_ARRAYSIZE(blendModes)))
			{
				m_blendMode = static_cast<TrailBlendMode>(currentBlend);
			}

			ImGui::Checkbox("Boss Phase 2 Ember Boost (<= 50% HP)", &m_enablePhase2Boost);
		}
	}
}
