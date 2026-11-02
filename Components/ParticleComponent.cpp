#include "pch.h"
#include "ParticleComponent.h"
#include "Entities/Actor.h"
#include "Entities/ActorManager.h"
#include "Components/TransformComponent.h"
#include "Components/SocketAttachmentComponent.h"
#include "Components/DamageDealerComponent.h"
#include "Components/ColliderComponent/ColliderComponent.h"
#include "Components/ColliderComponent/AABBColliderComponent.h"
#include "Components/ColliderComponent/OBBColliderComponent.h"
#include "Components/ColliderComponent/CapsuleColliderComponent.h"
#include "Message/Messenger.h"
#include "Framework/GameContext.h"
#include "Common/DeviceResources.h"
#include "Effect/ReadData.h"

#include <d3dcompiler.h>
#include <DirectXColors.h>
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <cmath>
#include <random>
#include <algorithm>
#include <fstream>

#if HEIN_WITH_EDITOR
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>
#endif

namespace HEIN
{
	// - Shared Engine GPU Shader & Procedural Texture Registry
	static Microsoft::WRL::ComPtr<ID3D11VertexShader>       s_universalParticleVS;
	static Microsoft::WRL::ComPtr<ID3D11PixelShader>        s_multiStyleParticlePS;
	static Microsoft::WRL::ComPtr<ID3D11SamplerState>       s_linearWrapSampler;
	static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> s_sharedNoiseSRV;
	static Microsoft::WRL::ComPtr<ID3D11VertexShader>       s_universalTrailVS;
	static Microsoft::WRL::ComPtr<ID3D11PixelShader>        s_universalTrailPS;
	static Microsoft::WRL::ComPtr<ID3D11InputLayout>        s_trailInputLayout;

	static std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11PixelShader>>        s_customPixelShaders;
	static std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> s_customTextureSRVs;
	static ID3D11Device*                                                                    s_cachedDevice = nullptr;

	static std::wstring ToWideString(const std::string& str)
	{
		if (str.empty()) return std::wstring();
		int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), nullptr, 0);
		std::wstring result(len, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), &result[0], len);
		return result;
	}

	void ParticleComponent::EnsureSharedResources(ID3D11Device* device)
	{
		if (!device) return;

		if (s_cachedDevice != device)
		{
			s_universalParticleVS.Reset();
			s_multiStyleParticlePS.Reset();
			s_linearWrapSampler.Reset();
			s_sharedNoiseSRV.Reset();
			s_universalTrailVS.Reset();
			s_universalTrailPS.Reset();
			s_trailInputLayout.Reset();
			s_customPixelShaders.clear();
			s_customTextureSRVs.clear();
			s_cachedDevice = device;
		}

		// - Initialize Universal Particle Billboard Shaders
		if (!s_universalParticleVS || !s_multiStyleParticlePS)
		{
			std::vector<uint8_t> vsData;
			std::vector<uint8_t> psData;
			bool loadedCso = false;

			try
			{
				vsData = DX::ReadData(L"Resources/Shaders/Particle_VS.cso");
				psData = DX::ReadData(L"Resources/Shaders/Particle_PS.cso");
				loadedCso = !vsData.empty() && !psData.empty();
			}
			catch (...)
			{
				loadedCso = false;
			}

			if (!loadedCso)
			{
				try
				{
					vsData = DX::ReadData(L"Dual/Resources/Shaders/Particle_VS.cso");
					psData = DX::ReadData(L"Dual/Resources/Shaders/Particle_PS.cso");
					loadedCso = !vsData.empty() && !psData.empty();
				}
				catch (...)
				{
					loadedCso = false;
				}
			}



			const void* vsBytecode = nullptr;
			size_t vsBytecodeSize  = 0;
			const void* psBytecode = nullptr;
			size_t psBytecodeSize  = 0;
			Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

			if (loadedCso)
			{
				vsBytecode     = vsData.data();
				vsBytecodeSize = vsData.size();
				psBytecode     = psData.data();
				psBytecodeSize = psData.size();
			}
			else
			{
				std::vector<std::wstring> vsCandidates = {
					L"External/Engine/Shaders/Particle_VS.hlsl",
					L"../External/Engine/Shaders/Particle_VS.hlsl"
				};
				for (const auto& path : vsCandidates)
				{
					if (SUCCEEDED(D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0", 0, 0, &vsBlob, &errBlob)))
					{
						vsBytecode     = vsBlob->GetBufferPointer();
						vsBytecodeSize = vsBlob->GetBufferSize();
						break;
					}
				}

				std::vector<std::wstring> psCandidates = {
					L"External/Engine/Shaders/Particle_PS.hlsl",
					L"../External/Engine/Shaders/Particle_PS.hlsl"
				};
				for (const auto& path : psCandidates)
				{
					if (SUCCEEDED(D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", 0, 0, &psBlob, &errBlob)))
					{
						psBytecode     = psBlob->GetBufferPointer();
						psBytecodeSize = psBlob->GetBufferSize();
						break;
					}
				}
			}

			if (vsBytecode && vsBytecodeSize > 0)
			{
				device->CreateVertexShader(vsBytecode, vsBytecodeSize, nullptr, s_universalParticleVS.ReleaseAndGetAddressOf());
			}
			if (psBytecode && psBytecodeSize > 0)
			{
				device->CreatePixelShader(psBytecode, psBytecodeSize, nullptr, s_multiStyleParticlePS.ReleaseAndGetAddressOf());
			}
		}

		// - Initialize Universal Ribbon Trail Shaders
		if (!s_universalTrailVS || !s_universalTrailPS || !s_trailInputLayout)
		{
			std::vector<uint8_t> vsData;
			std::vector<uint8_t> psData;
			bool loadedTrailCso = false;

			try
			{
				vsData = DX::ReadData(L"Resources/Shaders/WeaponTrail_VS.cso");
				psData = DX::ReadData(L"Resources/Shaders/WeaponTrail_PS.cso");
				loadedTrailCso = !vsData.empty() && !psData.empty();
			}
			catch (...)
			{
				loadedTrailCso = false;
			}

			if (!loadedTrailCso)
			{
				try
				{
					vsData = DX::ReadData(L"Dual/Resources/Shaders/WeaponTrail_VS.cso");
					psData = DX::ReadData(L"Dual/Resources/Shaders/WeaponTrail_PS.cso");
					loadedTrailCso = !vsData.empty() && !psData.empty();
				}
				catch (...)
				{
					loadedTrailCso = false;
				}
			}

			const void* vsBytecode = nullptr;
			size_t vsBytecodeSize  = 0;
			const void* psBytecode = nullptr;
			size_t psBytecodeSize  = 0;
			Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

			if (loadedTrailCso)
			{
				vsBytecode     = vsData.data();
				vsBytecodeSize = vsData.size();
				psBytecode     = psData.data();
				psBytecodeSize = psData.size();
			}
			else
			{
				std::vector<std::wstring> vsCandidates = {
					L"External/Engine/Shaders/WeaponTrail_VS.hlsl",
					L"../External/Engine/Shaders/WeaponTrail_VS.hlsl"
				};
				for (const auto& path : vsCandidates)
				{
					if (SUCCEEDED(D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0", 0, 0, &vsBlob, &errBlob)))
					{
						vsBytecode     = vsBlob->GetBufferPointer();
						vsBytecodeSize = vsBlob->GetBufferSize();
						break;
					}
				}

				std::vector<std::wstring> psCandidates = {
					L"External/Engine/Shaders/WeaponTrail_PS.hlsl",
					L"../External/Engine/Shaders/WeaponTrail_PS.hlsl"
				};
				for (const auto& path : psCandidates)
				{
					if (SUCCEEDED(D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", 0, 0, &psBlob, &errBlob)))
					{
						psBytecode     = psBlob->GetBufferPointer();
						psBytecodeSize = psBlob->GetBufferSize();
						break;
					}
				}
			}

			if (vsBytecode && vsBytecodeSize > 0)
			{
				device->CreateVertexShader(vsBytecode, vsBytecodeSize, nullptr, s_universalTrailVS.ReleaseAndGetAddressOf());

				static const D3D11_INPUT_ELEMENT_DESC inputLayoutDesc[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
					{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }
				};

				device->CreateInputLayout(
					inputLayoutDesc,
					static_cast<UINT>(std::size(inputLayoutDesc)),
					vsBytecode,
					vsBytecodeSize,
					s_trailInputLayout.ReleaseAndGetAddressOf()
				);
			}

			if (psBytecode && psBytecodeSize > 0)
			{
				device->CreatePixelShader(psBytecode, psBytecodeSize, nullptr, s_universalTrailPS.ReleaseAndGetAddressOf());
			}
		}

		// - Linear Wrap Sampler State
		if (!s_linearWrapSampler)
		{
			D3D11_SAMPLER_DESC sampDesc = {};
			sampDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			sampDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
			sampDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
			sampDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
			sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			sampDesc.MinLOD         = 0.0f;
			sampDesc.MaxLOD         = D3D11_FLOAT32_MAX;
			device->CreateSamplerState(&sampDesc, s_linearWrapSampler.ReleaseAndGetAddressOf());
		}

		// - Shared Procedural Noise Texture
		if (!s_sharedNoiseSRV)
		{
			if (FAILED(DirectX::CreateDDSTextureFromFile(device, L"Resources/Textures/waternoise.dds", nullptr, s_sharedNoiseSRV.ReleaseAndGetAddressOf())))
			{
				DirectX::CreateDDSTextureFromFile(device, L"Dual/Resources/Textures/waternoise.dds", nullptr, s_sharedNoiseSRV.ReleaseAndGetAddressOf());
			}

			if (!s_sharedNoiseSRV)
			{
				constexpr int kDim = 64;
				std::vector<uint32_t> pixels(kDim * kDim);
				auto hash2D = [](int x, int y) -> float
				{
					int n = ((x & 63) * 73) ^ ((y & 63) * 199);
					n = (n << 13) ^ n;
					return 1.0f - static_cast<float>((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
				};

				for (int y = 0; y < kDim; ++y)
				{
					for (int x = 0; x < kDim; ++x)
					{
						float n0 = hash2D(x / 2, y / 2) * 0.5f + 0.5f;
						float n1 = hash2D(x, y) * 0.5f + 0.5f;
						float v  = std::clamp(n0 * 0.65f + n1 * 0.35f, 0.0f, 1.0f);
						uint8_t c = static_cast<uint8_t>(v * 255.0f);
						pixels[y * kDim + x] = (255u << 24) | (c << 16) | (c << 8) | c;
					}
				}

				D3D11_TEXTURE2D_DESC texDesc = {};
				texDesc.Width            = kDim;
				texDesc.Height           = kDim;
				texDesc.MipLevels        = 1;
				texDesc.ArraySize        = 1;
				texDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
				texDesc.SampleDesc.Count = 1;
				texDesc.Usage            = D3D11_USAGE_IMMUTABLE;
				texDesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem     = pixels.data();
				initData.SysMemPitch = kDim * sizeof(uint32_t);

				Microsoft::WRL::ComPtr<ID3D11Texture2D> noiseTex;
				if (SUCCEEDED(device->CreateTexture2D(&texDesc, &initData, noiseTex.GetAddressOf())))
				{
					device->CreateShaderResourceView(noiseTex.Get(), nullptr, s_sharedNoiseSRV.ReleaseAndGetAddressOf());
				}
			}
		}
	}

	ID3D11PixelShader* ParticleComponent::ResolvePixelShader(ID3D11Device* device, const std::string& shaderName)
	{
		EnsureSharedResources(device);

		// - Procedural styles and all standard particle shaders route directly to the multi-style procedural shader
		if (shaderName.empty() ||
			shaderName == "Particle_PS" ||
			shaderName == "Default" ||
			shaderName == "HitEffect_PS" ||
			shaderName == "Particle_Eroded_PS" ||
			shaderName == "Particle_Streak_PS" ||
			shaderName == "Particle_Wisp_PS" ||
			shaderName == "Particle_Ring_PS" ||
			shaderName == "ParticleBlood_PS" ||
			shaderName == "ParticleSparks_PS")
		{
			return s_multiStyleParticlePS.Get();
		}

		auto it = s_customPixelShaders.find(shaderName);
		if (it != s_customPixelShaders.end())
		{
			return it->second ? it->second.Get() : s_multiStyleParticlePS.Get();
		}

		std::string stem = shaderName;
		size_t lastSlash = stem.find_last_of("/\\");
		if (lastSlash != std::string::npos) stem = stem.substr(lastSlash + 1);
		size_t dotPos = stem.find_last_of('.');
		if (dotPos != std::string::npos) stem = stem.substr(0, dotPos);

		std::vector<std::wstring> csoCandidates = {
			L"Resources/Shaders/" + ToWideString(stem) + L".cso",
			L"Dual/Resources/Shaders/" + ToWideString(stem) + L".cso",
			ToWideString(shaderName)
		};

		for (const auto& path : csoCandidates)
		{
			try
			{
				auto cso = DX::ReadData(path.c_str());
				if (!cso.empty())
				{
					Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
					if (SUCCEEDED(device->CreatePixelShader(cso.data(), cso.size(), nullptr, ps.GetAddressOf())))
					{
						s_customPixelShaders[shaderName] = ps;
						return ps.Get();
					}
				}
			}
			catch (...) {}
		}

		std::vector<std::wstring> hlslCandidates = {
			L"External/Engine/Shaders/" + ToWideString(stem) + L".hlsl",
			L"../External/Engine/Shaders/" + ToWideString(stem) + L".hlsl",
			ToWideString(shaderName)
		};

		for (const auto& path : hlslCandidates)
		{
			Microsoft::WRL::ComPtr<ID3DBlob> psBlob, errBlob;
			if (SUCCEEDED(D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", 0, 0, &psBlob, &errBlob)))
			{
				Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;
				if (SUCCEEDED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, ps.GetAddressOf())))
				{
					s_customPixelShaders[shaderName] = ps;
					return ps.Get();
				}
			}
		}

		// - Cache nullptr so failed lookups never hit disk again
		s_customPixelShaders[shaderName] = nullptr;
		return s_multiStyleParticlePS.Get();
	}

	ID3D11ShaderResourceView* ParticleComponent::ResolveTextureSRV(ID3D11Device* device, const std::string& customTexturePath)
	{
		EnsureSharedResources(device);
		if (customTexturePath.empty())
		{
			return s_sharedNoiseSRV.Get();
		}

		auto it = s_customTextureSRVs.find(customTexturePath);
		if (it != s_customTextureSRVs.end() && it->second)
		{
			return it->second.Get();
		}

		std::wstring wpath = ToWideString(customTexturePath);
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
		if (FAILED(DirectX::CreateDDSTextureFromFile(device, wpath.c_str(), nullptr, srv.GetAddressOf())))
		{
			DirectX::CreateWICTextureFromFile(device, wpath.c_str(), nullptr, srv.GetAddressOf());
		}

		if (srv)
		{
			s_customTextureSRVs[customTexturePath] = srv;
			return srv.Get();
		}

		return s_sharedNoiseSRV.Get();
	}

	float ParticleComponent::RandomRange(float minVal, float maxVal)
	{
		static thread_local std::mt19937 rng{ std::random_device{}() };
		if (minVal > maxVal) std::swap(minVal, maxVal);
		std::uniform_real_distribution<float> dist(minVal, maxVal);
		return dist(rng);
	}

	void ParticleComponent::EnsurePoolGPUResources(ID3D11Device* device, GPUParticlePool& pool)
	{
		if (!device) return;
		EnsureSharedResources(device);

		if (!pool.structuredBuffer || !pool.structuredSRV)
		{
			D3D11_BUFFER_DESC sbDesc = {};
			sbDesc.ByteWidth           = static_cast<UINT>(sizeof(GPUParticleSeed) * kMaxParticles);
			sbDesc.Usage               = D3D11_USAGE_DYNAMIC;
			sbDesc.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
			sbDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
			sbDesc.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			sbDesc.StructureByteStride = sizeof(GPUParticleSeed);

			D3D11_SUBRESOURCE_DATA initData = {};
			initData.pSysMem = pool.cpuSeeds.data();

			if (SUCCEEDED(device->CreateBuffer(&sbDesc, &initData, pool.structuredBuffer.ReleaseAndGetAddressOf())))
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Format              = DXGI_FORMAT_UNKNOWN;
				srvDesc.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Buffer.NumElements  = static_cast<UINT>(kMaxParticles);
				device->CreateShaderResourceView(pool.structuredBuffer.Get(), &srvDesc, pool.structuredSRV.ReleaseAndGetAddressOf());
			}
		}

		if (!m_cbParticleFrame)
		{
			D3D11_BUFFER_DESC cbDesc = {};
			cbDesc.ByteWidth      = sizeof(CB_ParticleFrame);
			cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
			cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
			cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			device->CreateBuffer(&cbDesc, nullptr, m_cbParticleFrame.ReleaseAndGetAddressOf());
		}
	}

	void ParticleComponent::EnsureTrailGPUResources(ID3D11Device* device)
	{
		if (!device) return;
		EnsureSharedResources(device);

		if (!m_trailDynamicVB)
		{
			D3D11_BUFFER_DESC vbDesc = {};
			vbDesc.Usage          = D3D11_USAGE_DYNAMIC;
			vbDesc.ByteWidth      = static_cast<UINT>(sizeof(TrailVertex) * kMaxTrailVertices);
			vbDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
			vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			device->CreateBuffer(&vbDesc, nullptr, m_trailDynamicVB.ReleaseAndGetAddressOf());
		}

		if (!m_cbTrail)
		{
			D3D11_BUFFER_DESC cbDesc = {};
			cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
			cbDesc.ByteWidth      = sizeof(CB_TrailParams);
			cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
			cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			device->CreateBuffer(&cbDesc, nullptr, m_cbTrail.ReleaseAndGetAddressOf());
		}
	}

	void ParticleComponent::RegisterToWielderMessages()
	{
		if (!m_owner) return;

		ActorID listenID = INVALID_ACTOR_ID;

		// - 1. Socket attachment owner (wielder actor holding the weapon)
		if (auto* socketAttach = m_owner->GetComponent<SocketAttachmentComponent>())
		{
			listenID = socketAttach->GetSocketOwnerActorID();
			if (m_actorManager && listenID != INVALID_ACTOR_ID && !m_actorManager->GetActor(listenID))
			{
				listenID = INVALID_ACTOR_ID;
			}
		}

		// - 2. Hierarchy parent actor
		if (listenID == INVALID_ACTOR_ID && m_owner->GetParentID() != INVALID_ACTOR_ID)
		{
			listenID = m_owner->GetParentID();
			if (m_actorManager && !m_actorManager->GetActor(listenID))
			{
				listenID = INVALID_ACTOR_ID;
			}
		}

		// - 3. Explicit owner ID (must not be the weapon itself)
		if (listenID == INVALID_ACTOR_ID && m_owner->GetOwnerID() != INVALID_ACTOR_ID && m_owner->GetOwnerID() != m_owner->GetID())
		{
			listenID = m_owner->GetOwnerID();
			if (m_actorManager && !m_actorManager->GetActor(listenID))
			{
				listenID = INVALID_ACTOR_ID;
			}
		}

		// - 4. Fallback: Player actor if not an enemy weapon
		if (listenID == INVALID_ACTOR_ID && m_actorManager != nullptr && m_owner->GetActorType() != ActorType::Enemy)
		{
			if (Actor* player = m_actorManager->GetFirstActorByType(ActorType::Player))
			{
				listenID = player->GetID();
			}
		}

		if (listenID != INVALID_ACTOR_ID && listenID != m_registeredListenID)
		{
			Messenger* msg = Messenger::GetInstance();
			if (!msg)
			{
				return;
			}
			if (m_registeredListenID != INVALID_ACTOR_ID)
			{
				msg->UnRegisterObserver(this);
			}
			msg->Register(listenID, this);
			m_registeredListenID = listenID;
		}
	}

	ParticleComponent::ParticleComponent(Actor* owner, ActorManager* manager)
		: IComponent(owner)
		, m_actorManager(manager)
	{
		ApplyDefaultCombatPreset();
	}

	ParticleComponent::~ParticleComponent()
	{
		if (Messenger* msg = Messenger::GetInstance())
		{
			msg->UnRegisterObserver(this);
		}
	}

	void ParticleComponent::Start()
	{
		Clear();
		if (m_trail.enabled && !m_trail.hasCustomOffsets)
		{
			AutoFitOffsetsFromWeapon();
		}
		RegisterToWielderMessages();
	}

	void ParticleComponent::Clear()
	{
		for (auto& s : m_gameplayPool.cpuSeeds)
		{
			s.lifetime  = 0.0f;
			s.spawnTime = -1000.0f;
		}
		m_gameplayPool.nextCursor  = 0;
		m_gameplayPool.activeCount = 0;
		m_gameplayPool.gpuDirty    = true;
		m_gameplayTrail.Clear();

#if HEIN_WITH_EDITOR
		for (auto& s : m_previewPool.cpuSeeds)
		{
			s.lifetime  = 0.0f;
			s.spawnTime = -1000.0f;
		}
		m_previewPool.nextCursor  = 0;
		m_previewPool.activeCount = 0;
		m_previewPool.gpuDirty    = true;
		m_previewTrail.Clear();
#endif
	}

	void ParticleComponent::OnMessageAccepted(Message::MessageID messageID)
	{
		if (messageID == Message::SET_WEAPON_ACTIVE)
		{
			if (!m_isTrailEmitting)
			{
				m_gameplayTrail.Clear();
			}
			m_isTrailEmitting = true;
		}
		else if (messageID == Message::SET_WEAPON_INACTIVE)
		{
			m_isTrailEmitting = false;
		}
		else if (messageID == Message::RESET_WEAPON_SWING)
		{
			m_gameplayTrail.Clear();
		}
	}

	void ParticleComponent::Update(float deltaTime)
	{
		if (!m_owner || !m_owner->IsActive()) return;

		// - Update Continuous Rate-Over-Time Emitter Slots
		for (size_t i = 0; i < m_emitters.size(); ++i)
		{
			auto& slot = m_emitters[i];
			if (!slot.enabled || !slot.looping || slot.rateOverTime <= 0.0f) continue;

			slot.emissionAccumulator += slot.rateOverTime * deltaTime;
			if (slot.emissionAccumulator >= 1.0f)
			{
				int toSpawn = static_cast<int>(std::floor(slot.emissionAccumulator));
				slot.emissionAccumulator -= static_cast<float>(toSpawn);

				DirectX::SimpleMath::Vector3 worldPos = DirectX::SimpleMath::Vector3::Zero;
				if (auto* transform = m_owner->GetComponent<TransformComponent>())
				{
					worldPos = transform->GetWorldMatrix().Translation();
				}
				worldPos.y += m_impactHeightOffset;

				for (int s = 0; s < toSpawn; ++s)
				{
					SpawnSlotIntoPool(m_gameplayPool, i, worldPos, DirectX::SimpleMath::Vector3::UnitY, 1.0f);
				}
			}
		}
	}

	void ParticleComponent::LateUpdate(float deltaTime)
	{
		if (!m_owner || !m_owner->IsActive()) return;

		m_gameplayPool.simTime += deltaTime;

		// - Count Active Billboard Particles
		size_t activeCount = 0;
		for (size_t i = 0; i < kMaxParticles; ++i)
		{
			const auto& seed = m_gameplayPool.cpuSeeds[i];
			float age = m_gameplayPool.simTime - seed.spawnTime;
			if (seed.lifetime > 0.001f && age >= 0.0f && age < seed.lifetime)
			{
				++activeCount;
			}
		}
		m_gameplayPool.activeCount = activeCount;

		// - Ribbon Trail Module LateUpdate (Only active if trail module enabled)
		if (!m_trail.enabled) return;

		RegisterToWielderMessages();

		Actor* wielder = nullptr;
		if (m_actorManager && m_registeredListenID != INVALID_ACTOR_ID)
		{
			wielder = m_actorManager->GetActor(m_registeredListenID);
		}

		if (wielder && wielder->IsHitstopped())
		{
			return;
		}

		bool shouldEmit = m_isTrailEmitting || m_alwaysEmitPreview;
		if (auto* dealer = m_owner->GetComponent<DamageDealerComponent>())
		{
			if (dealer->IsActive())
			{
				shouldEmit = true;
			}
		}

		if (!shouldEmit && m_gameplayTrail.count == 0)
		{
			return;
		}

		float effectiveDt = (deltaTime > 0.0f) ? deltaTime : (m_alwaysEmitPreview ? 0.016f : 0.0f);
		m_gameplayTrail.elapsedTime += effectiveDt;

		if (effectiveDt > 0.0f && m_gameplayTrail.count > 0)
		{
			for (size_t i = 0; i < m_gameplayTrail.count; ++i)
			{
				m_gameplayTrail.GetKnot(i).age += effectiveDt;
			}

			while (m_gameplayTrail.count > 0 && m_gameplayTrail.GetKnot(m_gameplayTrail.count - 1).age >= m_trail.lifetime)
			{
				--m_gameplayTrail.count;
			}
		}

		if (!shouldEmit) return;

		if (auto* socketAttach = m_owner->GetComponent<SocketAttachmentComponent>())
		{
			socketAttach->LateUpdate(0.0f);
		}

		auto* transform = m_owner->GetComponent<TransformComponent>();
		if (!transform) return;

		const DirectX::SimpleMath::Matrix worldMat = transform->GetWorldMatrix();
		const DirectX::SimpleMath::Vector3 baseWorld = DirectX::SimpleMath::Vector3::Transform(m_trail.baseOffset, worldMat);
		const DirectX::SimpleMath::Vector3 tipWorld  = DirectX::SimpleMath::Vector3::Transform(m_trail.tipOffset, worldMat);

		if (m_gameplayTrail.count == 0)
		{
			m_gameplayTrail.PushKnot(baseWorld, tipWorld);
			return;
		}

		const float minDistSq = m_trail.minVertexDistance * m_trail.minVertexDistance;
		const float tipDistSq  = DirectX::SimpleMath::Vector3::DistanceSquared(tipWorld, m_gameplayTrail.GetKnot(0).tipWorld);
		const float baseDistSq = DirectX::SimpleMath::Vector3::DistanceSquared(baseWorld, m_gameplayTrail.GetKnot(0).baseWorld);

		if (tipDistSq >= minDistSq || baseDistSq >= minDistSq)
		{
			m_gameplayTrail.PushKnot(baseWorld, tipWorld);
		}
		else
		{
			m_gameplayTrail.GetKnot(0).baseWorld = baseWorld;
			m_gameplayTrail.GetKnot(0).tipWorld  = tipWorld;
			m_gameplayTrail.GetKnot(0).age       = 0.0f;
		}
	}

	void ParticleComponent::SpawnSlotIntoPool(
		GPUParticlePool& pool,
		size_t slotIndex,
		const DirectX::SimpleMath::Vector3& worldPos,
		const DirectX::SimpleMath::Vector3& dir,
		float intensityMultiplier)
	{
		if (slotIndex >= m_emitters.size()) return;
		const auto& slot = m_emitters[slotIndex];
		if (!slot.enabled) return;

		float clampedIntensity = std::clamp(intensityMultiplier, 0.35f, 3.0f);
		DirectX::SimpleMath::Vector3 baseDir = dir;
		if (baseDir.LengthSquared() < 0.0001f)
		{
			baseDir = DirectX::SimpleMath::Vector3::UnitY;
		}
		baseDir.Normalize();

		int spawnCount = static_cast<int>(std::round(slot.burstCount * clampedIntensity));
		spawnCount = std::clamp(spawnCount, 1, static_cast<int>(kMaxParticles / 4));

		float speedScale = std::sqrt(clampedIntensity);
		float sizeScale  = std::sqrt(clampedIntensity);

		for (int i = 0; i < spawnCount; ++i)
		{
			DirectX::SimpleMath::Vector3 offset(
				RandomRange(-slot.spawnRadius, slot.spawnRadius),
				RandomRange(-slot.spawnRadius, slot.spawnRadius),
				RandomRange(-slot.spawnRadius, slot.spawnRadius)
			);
			if (slot.shape == ParticleSpawnShape::Point)
			{
				offset = DirectX::SimpleMath::Vector3::Zero;
			}

			DirectX::SimpleMath::Vector3 launchDir = baseDir;
			if (slot.shape == ParticleSpawnShape::Sphere)
			{
				launchDir = DirectX::SimpleMath::Vector3(
					RandomRange(-1.0f, 1.0f),
					RandomRange(-1.0f, 1.0f),
					RandomRange(-1.0f, 1.0f)
				);
			}
			else if (slot.shape == ParticleSpawnShape::Hemisphere)
			{
				launchDir = DirectX::SimpleMath::Vector3(
					RandomRange(-1.0f, 1.0f),
					RandomRange(0.05f, 1.0f),
					RandomRange(-1.0f, 1.0f)
				);
			}
			else if (slot.shape == ParticleSpawnShape::Cone)
			{
				DirectX::SimpleMath::Vector3 jitter(
					RandomRange(-slot.spreadCone, slot.spreadCone),
					RandomRange(-slot.spreadCone * 0.35f, slot.spreadCone),
					RandomRange(-slot.spreadCone, slot.spreadCone)
				);
				launchDir = baseDir + jitter;
			}

			if (launchDir.LengthSquared() > 0.0001f)
			{
				launchDir.Normalize();
			}

			float speed    = RandomRange(slot.speedMin, slot.speedMax) * speedScale;
			float lifetime = (std::max)(0.04f, RandomRange(slot.lifetimeMin, slot.lifetimeMax));
			float sFactor  = RandomRange(0.75f, 1.25f) * sizeScale;

			GPUParticleSeed seed;
			seed.spawnPos      = worldPos + slot.localOffset + offset;
			seed.spawnTime     = pool.simTime;
			seed.initialVel    = (slot.shape == ParticleSpawnShape::Point && slot.speedMax <= 0.25f)
				? DirectX::SimpleMath::Vector3::Zero
				: (launchDir * speed);
			seed.lifetime      = lifetime;
			seed.startColor    = slot.startColor;
			seed.endColor      = slot.endColor;
			seed.startSize     = slot.sizeStart * sFactor;
			seed.endSize       = slot.sizeEnd * sFactor;
			seed.drag          = slot.drag;
			seed.gravityScale  = slot.gravity / 9.8f;
			seed.stretchFactor = slot.stretchFactor;
			seed.randomSeed    = RandomRange(0.0f, 1.0f);
			seed.slotIndex     = static_cast<uint32_t>(slotIndex);
			seed.seedPadding   = 0.0f;

			pool.cpuSeeds[pool.nextCursor] = seed;
			pool.nextCursor = (pool.nextCursor + 1) % kMaxParticles;
		}

		pool.gpuDirty = true;
	}

	void ParticleComponent::EmitCue(
		const std::string& cueName,
		const DirectX::SimpleMath::Vector3& worldPos,
		const DirectX::SimpleMath::Vector3& impactDir,
		float intensityMultiplier)
	{
		if (cueName.empty()) return;

		for (size_t i = 0; i < m_emitters.size(); ++i)
		{
			if (m_emitters[i].enabled && m_emitters[i].triggerCue == cueName)
			{
				SpawnSlotIntoPool(m_gameplayPool, i, worldPos, impactDir, intensityMultiplier);
			}
		}
	}

	void ParticleComponent::EmitSlot(
		size_t slotIndex,
		const DirectX::SimpleMath::Vector3& worldPos,
		const DirectX::SimpleMath::Vector3& impactDir,
		float intensityMultiplier)
	{
		if (slotIndex < m_emitters.size())
		{
			SpawnSlotIntoPool(m_gameplayPool, slotIndex, worldPos, impactDir, intensityMultiplier);
		}
	}

	void ParticleComponent::DrawParticlePool(
		ID3D11Device* device,
		ID3D11DeviceContext* context,
		DirectX::CommonStates& states,
		GPUParticlePool& pool,
		const DirectX::SimpleMath::Matrix& view,
		const DirectX::SimpleMath::Matrix& proj,
		int soloSlotIndex)
	{
		if (m_emitters.empty()) return;

		EnsurePoolGPUResources(device, pool);
		if (!s_universalParticleVS || !pool.structuredBuffer || !pool.structuredSRV || !m_cbParticleFrame)
		{
			return;
		}

		if (pool.gpuDirty)
		{
			D3D11_MAPPED_SUBRESOURCE mappedSB = {};
			if (SUCCEEDED(context->Map(pool.structuredBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSB)))
			{
				std::memcpy(mappedSB.pData, pool.cpuSeeds.data(), sizeof(GPUParticleSeed) * kMaxParticles);
				context->Unmap(pool.structuredBuffer.Get(), 0);
			}
			pool.gpuDirty = false;
		}

		// - Extract camera basis vectors for billboarding
		DirectX::SimpleMath::Matrix invView = view.Invert();
		DirectX::SimpleMath::Vector3 camRight(invView._11, invView._12, invView._13);
		DirectX::SimpleMath::Vector3 camUp(invView._21, invView._22, invView._23);
		DirectX::SimpleMath::Vector3 camForward(invView._31, invView._32, invView._33);
		camRight.Normalize();
		camUp.Normalize();
		camForward.Normalize();

		DirectX::SimpleMath::Matrix viewProj = view * proj;

		// - Bind Pipeline for SV_VertexID GPU Expansion
		context->IASetInputLayout(nullptr);
		ID3D11Buffer* nullVB = nullptr;
		UINT zeroStride = 0;
		UINT zeroOffset = 0;
		context->IASetVertexBuffers(0, 1, &nullVB, &zeroStride, &zeroOffset);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		context->VSSetShader(s_universalParticleVS.Get(), nullptr, 0);
		context->VSSetConstantBuffers(0, 1, m_cbParticleFrame.GetAddressOf());
		ID3D11ShaderResourceView* vsSRVs[] = { pool.structuredSRV.Get() };
		context->VSSetShaderResources(1, 1, vsSRVs);

		context->PSSetConstantBuffers(0, 1, m_cbParticleFrame.GetAddressOf());
		ID3D11SamplerState* samplers[] = { s_linearWrapSampler.Get() };
		context->PSSetSamplers(0, 1, samplers);

		context->RSSetState(states.CullNone());
		context->OMSetDepthStencilState(states.DepthRead(), 0);

		for (size_t slotIdx = 0; slotIdx < m_emitters.size(); ++slotIdx)
		{
			if (soloSlotIndex >= 0 && static_cast<int>(slotIdx) != soloSlotIndex)
			{
				continue;
			}

			const auto& slot = m_emitters[slotIdx];
			if (!slot.enabled) continue;

			ID3D11PixelShader* ps = ResolvePixelShader(device, slot.shaderName);
			if (!ps) continue;

			ID3D11ShaderResourceView* texSRV = ResolveTextureSRV(device, slot.customTexturePath);
			context->PSSetShaderResources(0, 1, &texSRV);
			context->PSSetShader(ps, nullptr, 0);

			if (slot.blendMode == ParticleBlendMode::Additive)
			{
				context->OMSetBlendState(states.Additive(), nullptr, 0xFFFFFFFF);
			}
			else
			{
				context->OMSetBlendState(states.NonPremultiplied(), nullptr, 0xFFFFFFFF);
			}

			D3D11_MAPPED_SUBRESOURCE mappedCB = {};
			if (SUCCEEDED(context->Map(m_cbParticleFrame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
			{
				auto* cb = static_cast<CB_ParticleFrame*>(mappedCB.pData);
				cb->ViewProj      = DirectX::XMMatrixTranspose(viewProj);
				cb->CameraRight   = camRight;
				cb->CurrentTime   = pool.simTime;
				cb->CameraUp      = camUp;
				cb->FramePadding  = 0.0f;
				cb->CameraForward = camForward;
				cb->GravityAccel  = slot.gravity;
				cb->ShaderParams  = DirectX::SimpleMath::Vector4(
					slot.shaderParams.x,
					slot.shaderParams.y,
					slot.shaderParams.z,
					static_cast<float>(slot.proceduralStyle)
				);
				context->Unmap(m_cbParticleFrame.Get(), 0);
			}
			context->Draw(static_cast<UINT>(kMaxParticles * 6), 0);
		}

		ID3D11ShaderResourceView* nullSRV = nullptr;
		context->VSSetShaderResources(1, 1, &nullSRV);
		context->PSSetShaderResources(0, 1, &nullSRV);
	}

	void ParticleComponent::DrawTrailBuffer(
		ID3D11Device* device,
		ID3D11DeviceContext* context,
		DirectX::CommonStates& states,
		const TrailRingBuffer& trailBuf,
		const DirectX::SimpleMath::Matrix& view,
		const DirectX::SimpleMath::Matrix& proj,
		bool /*allowPhase2Check*/)
	{
		if (trailBuf.count < 2) return;

		EnsureTrailGPUResources(device);
		if (!m_trailDynamicVB || !m_cbTrail || !s_universalTrailVS || !s_universalTrailPS || !s_trailInputLayout)
		{
			return;
		}

		D3D11_MAPPED_SUBRESOURCE mappedVB = {};
		if (FAILED(context->Map(m_trailDynamicVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVB)))
		{
			return;
		}

		auto* outVerts = static_cast<TrailVertex*>(mappedVB.pData);
		size_t vertexCount = 0;

		const int subdivs = std::clamp(m_trail.subdivisions, 1, kMaxTrailSubdivs);
		const float invSubdivs = 1.0f / static_cast<float>(subdivs);
		const float invLifetime = 1.0f / (std::max)(0.01f, m_trail.lifetime);

		for (size_t i = 0; i < trailBuf.count - 1; ++i)
		{
			const auto& k0 = trailBuf.GetKnot((i == 0) ? 0 : (i - 1));
			const auto& k1 = trailBuf.GetKnot(i);
			const auto& k2 = trailBuf.GetKnot(i + 1);
			const auto& k3 = trailBuf.GetKnot((i + 2 < trailBuf.count) ? (i + 2) : (trailBuf.count - 1));

			const float len1 = (k1.tipWorld - k1.baseWorld).Length();
			const float len2 = (k2.tipWorld - k2.baseWorld).Length();

			const int steps = (i == trailBuf.count - 2) ? subdivs : (subdivs - 1);
			for (int s = 0; s <= steps; ++s)
			{
				if (vertexCount + 2 > kMaxTrailVertices) break;

				const float t = static_cast<float>(s) * invSubdivs;

				DirectX::SimpleMath::Vector3 interpBase = DirectX::SimpleMath::Vector3::CatmullRom(
					k0.baseWorld, k1.baseWorld, k2.baseWorld, k3.baseWorld, t
				);
				DirectX::SimpleMath::Vector3 interpTip = DirectX::SimpleMath::Vector3::CatmullRom(
					k0.tipWorld, k1.tipWorld, k2.tipWorld, k3.tipWorld, t
				);

				DirectX::SimpleMath::Vector3 bladeVec = interpTip - interpBase;
				const float currentLenSq = bladeVec.LengthSquared();
				const float targetLen = len1 + (len2 - len1) * t;
				if (currentLenSq > 0.0001f && targetLen > 0.001f)
				{
					const float invCurLen = 1.0f / std::sqrt(currentLenSq);
					const DirectX::SimpleMath::Vector3 bladeDir = bladeVec * invCurLen;
					interpTip = interpBase + bladeDir * targetLen;
				}

				const float knotAge = k1.age + (k2.age - k1.age) * t;
				const float normAge = std::clamp(knotAge * invLifetime, 0.0f, 1.0f);
				const float trailAlpha = std::pow(1.0f - normAge, m_trail.tipFalloffPower);

				const float uCoord = normAge;

				outVerts[vertexCount].pos   = interpBase;
				outVerts[vertexCount].uv    = DirectX::SimpleMath::Vector2(uCoord, 0.0f);
				outVerts[vertexCount].color = DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, trailAlpha);
				++vertexCount;

				outVerts[vertexCount].pos   = interpTip;
				outVerts[vertexCount].uv    = DirectX::SimpleMath::Vector2(uCoord, 1.0f);
				outVerts[vertexCount].color = DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, trailAlpha);
				++vertexCount;
			}
		}
		context->Unmap(m_trailDynamicVB.Get(), 0);

		if (vertexCount < 4) return;

		D3D11_MAPPED_SUBRESOURCE mappedCB = {};
		if (SUCCEEDED(context->Map(m_cbTrail.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
		{
			auto* cb = static_cast<CB_TrailParams*>(mappedCB.pData);
			cb->ViewProj             = DirectX::XMMatrixTranspose(view * proj);
			cb->CoreColor            = m_trail.coreColor;
			cb->EdgeColor            = m_trail.edgeColor;
			cb->EmissiveIntensity    = m_trail.emissiveIntensity;
			cb->TipFalloffPower      = m_trail.tipFalloffPower;
			cb->NoiseScale           = m_trail.noiseScale;
			cb->NoiseScrollSpeed     = m_trail.noiseScrollSpeed;
			cb->Time                 = trailBuf.elapsedTime;
			cb->ErosionStrength      = m_trail.erosionStrength;
			cb->LeadingEdgeSharpness = m_trail.leadingEdgeSharpness;
			cb->TrailPadding         = 0.0f;
			context->Unmap(m_cbTrail.Get(), 0);
		}

		UINT stride = sizeof(TrailVertex);
		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, m_trailDynamicVB.GetAddressOf(), &stride, &offset);
		context->IASetInputLayout(s_trailInputLayout.Get());
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		context->VSSetShader(s_universalTrailVS.Get(), nullptr, 0);
		context->VSSetConstantBuffers(0, 1, m_cbTrail.GetAddressOf());

		ID3D11PixelShader* trailPS = ResolvePixelShader(device, m_trail.shaderName);
		if (!trailPS) trailPS = s_universalTrailPS.Get();

		context->PSSetShader(trailPS, nullptr, 0);
		context->PSSetConstantBuffers(0, 1, m_cbTrail.GetAddressOf());

		ID3D11ShaderResourceView* noiseSRV = s_sharedNoiseSRV.Get();
		context->PSSetShaderResources(0, 1, &noiseSRV);
		ID3D11SamplerState* samplers[] = { s_linearWrapSampler.Get() };
		context->PSSetSamplers(0, 1, samplers);

		context->RSSetState(states.CullNone());
		context->OMSetDepthStencilState(states.DepthRead(), 0);

		if (m_trail.blendMode == ParticleBlendMode::Additive)
		{
			context->OMSetBlendState(states.Additive(), nullptr, 0xFFFFFFFF);
		}
		else
		{
			context->OMSetBlendState(states.NonPremultiplied(), nullptr, 0xFFFFFFFF);
		}

		context->Draw(static_cast<UINT>(vertexCount), 0);

		ID3D11ShaderResourceView* nullSRV = nullptr;
		context->PSSetShaderResources(0, 1, &nullSRV);
	}

	void ParticleComponent::DrawTransparent(
		GameContext& gameContext,
		const DirectX::SimpleMath::Matrix& world,
		const DirectX::SimpleMath::Matrix& view,
		const DirectX::SimpleMath::Matrix& proj)
	{
		if (!m_owner || !m_owner->IsVisible() || !m_owner->IsActive()) return;

		ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
		ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
		if (!device || !context) return;

		// - Draw Blade Debug Line in Editor Scene View when colliders are shown
		if (gameContext.isEditorMode && gameContext.showColliders && m_trail.showDebugBladeLine && m_trail.enabled && gameContext.debugCollisionRenderer)
		{
			const DirectX::SimpleMath::Vector3 dbgBase = DirectX::SimpleMath::Vector3::Transform(m_trail.baseOffset, world);
			const DirectX::SimpleMath::Vector3 dbgTip  = DirectX::SimpleMath::Vector3::Transform(m_trail.tipOffset, world);
			gameContext.debugCollisionRenderer->QueueLine(dbgBase, dbgTip, DirectX::SimpleMath::Color(0.0f, 1.0f, 1.0f, 1.0f));
		}

		// - Render GPU Billboard Emitters
		DrawParticlePool(device, context, gameContext.commonStates, m_gameplayPool, view, proj, -1);

		// - Render Ribbon Trail
		if (m_trail.enabled)
		{
			DrawTrailBuffer(device, context, gameContext.commonStates, m_gameplayTrail, view, proj, m_trail.enablePhase2Boost);
		}
	}

	void ParticleComponent::AutoFitOffsetsFromWeapon()
	{
		if (!m_owner) return;

		// - Blade segment = collider's longest axis; endpoint nearest the grip (local origin) is the base
		auto fitFromBox = [this](const DirectX::SimpleMath::Vector3& center, const DirectX::SimpleMath::Vector3& extents)
		{
			DirectX::SimpleMath::Vector3 halfAxis;
			if (extents.y >= extents.x && extents.y >= extents.z)      halfAxis = DirectX::SimpleMath::Vector3(0.0f, extents.y, 0.0f);
			else if (extents.z >= extents.x && extents.z >= extents.y) halfAxis = DirectX::SimpleMath::Vector3(0.0f, 0.0f, extents.z);
			else                                                       halfAxis = DirectX::SimpleMath::Vector3(extents.x, 0.0f, 0.0f);

			const DirectX::SimpleMath::Vector3 pA = center - halfAxis;
			const DirectX::SimpleMath::Vector3 pB = center + halfAxis;
			const bool aIsBase = pA.LengthSquared() <= pB.LengthSquared();
			m_trail.baseOffset = aIsBase ? pA : pB;
			m_trail.tipOffset  = aIsBase ? pB : pA;
			m_trail.hasCustomOffsets = true;
		};

		if (auto* obb = m_owner->GetComponent<OBBColliderComponent>())
		{
			fitFromBox(obb->GetOffset(), obb->GetExtents());
			return;
		}

		if (auto* aabb = m_owner->GetComponent<AABBColliderComponent>())
		{
			fitFromBox(aabb->GetOffset(), aabb->GetExtents());
			return;
		}

		if (auto* cap = m_owner->GetComponent<CapsuleColliderComponent>())
		{
			const float halfH = cap->GetHeight() * 0.5f;
			fitFromBox(cap->GetOffset(), DirectX::SimpleMath::Vector3(0.0f, halfH, 0.0f));
			return;
		}

		m_trail.baseOffset = DirectX::SimpleMath::Vector3(0.0f, 0.0f, -0.8f);
		m_trail.tipOffset  = DirectX::SimpleMath::Vector3(0.0f, 0.0f, -5.8f);
		m_trail.hasCustomOffsets = true;
	}

void ParticleComponent::ApplyDefaultCombatPreset()
	{
		LoadAsset("Resources/Data/VFX/CombatImpact.json");
	}

	void ParticleComponent::ApplySoulGhostPreset()
	{
		LoadAsset("Resources/Data/VFX/SoulGhost.json");
	}

	void ParticleComponent::ApplyStoneDebrisPreset()
	{
		LoadAsset("Resources/Data/VFX/StoneDebris.json");
	}

	void ParticleComponent::ApplyFireEmberPreset()
	{
		LoadAsset("Resources/Data/VFX/FireEmbers.json");
	}

	void ParticleComponent::ApplyTarnishedGoldTrailPreset()
	{
		LoadAsset("Resources/Data/VFX/GoldenSlashTrail.json");
	}

	void ParticleComponent::ApplyColdSteelTrailPreset()
	{
		LoadAsset("Resources/Data/VFX/ColdSteelTrail.json");
	}

	void ParticleComponent::ApplyBossBloodflameTrailPreset()
	{
		LoadAsset("Resources/Data/VFX/BloodflameSlashTrail.json");
	}

	void ParticleComponent::ApplyBlackFlameTrailPreset()
	{
		LoadAsset("Resources/Data/VFX/BloodflameSlashTrail.json");
	}

	void ParticleComponent::OnCombatImpact(const CombatImpactDesc& impact)
	{
		std::string targetCue = "OnDamage";
		switch (impact.impactType)
		{
		case CombatImpactType::Parry:
			targetCue = "OnParry";
			break;
		case CombatImpactType::Block:
			targetCue = "OnBlock";
			break;
		case CombatImpactType::GuardBreak:
			targetCue = "OnGuardBreak";
			break;
		case CombatImpactType::Damage:
		default:
			targetCue = "OnDamage";
			break;
		}

		EmitCue(targetCue, impact.contactPoint, impact.contactNormal, impact.intensity);
	}

	bool ParticleComponent::LoadAsset(const std::string& assetPath)
	{
		std::vector<std::string> candidates = {
			assetPath,
			"Dual/" + assetPath,
			"../Dual/" + assetPath
		};

		std::ifstream file;
		for (const auto& candidate : candidates)
		{
			file.open(candidate);
			if (file.is_open()) break;
		}

		if (!file.is_open())
		{
			return false;
		}

		try
		{
			nlohmann::json j;
			file >> j;
			m_assetPath = assetPath;
			DeserializeData(j);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool ParticleComponent::SaveAsset(const std::string& assetPath)
	{
		nlohmann::json j;
		j["AssetType"] = "VFXAsset";
		j["Version"]   = 1;
		SerializeData(j);

		std::vector<std::string> candidates = {
			assetPath,
			"Dual/" + assetPath,
			"../Dual/" + assetPath
		};

		for (const auto& candidate : candidates)
		{
			std::ofstream file(candidate);
			if (file.is_open())
			{
				file << j.dump(4);
				m_assetPath = assetPath;
				return true;
			}
		}
		return false;
	}

	nlohmann::json ParticleComponent::Serialize()
	{
		nlohmann::json j;
		j["Type"] = "ParticleComponent";
		if (!m_assetPath.empty())
		{
			j["AssetPath"] = m_assetPath;
			j["HasCustomOffsets"] = m_trail.hasCustomOffsets;
			if (m_trail.hasCustomOffsets)
			{
				j["BaseOffset"] = { m_trail.baseOffset.x, m_trail.baseOffset.y, m_trail.baseOffset.z };
				j["TipOffset"]  = { m_trail.tipOffset.x, m_trail.tipOffset.y, m_trail.tipOffset.z };
			}
			return j;
		}

		SerializeData(j);
		return j;
	}

	void ParticleComponent::SerializeData(nlohmann::json& j) const
	{
		j["ImpactHeightOffset"] = m_impactHeightOffset;

		// - Serialize Emitter Slots
		nlohmann::json emittersArray = nlohmann::json::array();
		for (const auto& slot : m_emitters)
		{
			nlohmann::json s;
			s["SlotName"]          = slot.slotName;
			s["TriggerCue"]        = slot.triggerCue;
			s["ShaderName"]        = slot.shaderName;
			s["CustomTexturePath"] = slot.customTexturePath;
			s["BlendMode"]         = static_cast<int>(slot.blendMode);
			s["Shape"]             = static_cast<int>(slot.shape);
			s["Enabled"]           = slot.enabled;
			s["Looping"]           = slot.looping;
			s["RateOverTime"]      = slot.rateOverTime;
			s["BurstCount"]        = slot.burstCount;
			s["SpreadCone"]        = slot.spreadCone;
			s["SpawnRadius"]       = slot.spawnRadius;
			s["LocalOffset"]       = { slot.localOffset.x, slot.localOffset.y, slot.localOffset.z };
			s["SpeedMin"]          = slot.speedMin;
			s["SpeedMax"]          = slot.speedMax;
			s["LifetimeMin"]       = slot.lifetimeMin;
			s["LifetimeMax"]       = slot.lifetimeMax;
			s["Drag"]              = slot.drag;
			s["Gravity"]           = slot.gravity;
			s["StretchFactor"]     = slot.stretchFactor;
			s["SizeStart"]         = slot.sizeStart;
			s["SizeEnd"]           = slot.sizeEnd;
			s["StartColor"]        = { slot.startColor.x, slot.startColor.y, slot.startColor.z, slot.startColor.w };
			s["EndColor"]          = { slot.endColor.x, slot.endColor.y, slot.endColor.z, slot.endColor.w };
			s["ShaderParams"]      = { slot.shaderParams.x, slot.shaderParams.y, slot.shaderParams.z, slot.shaderParams.w };
			s["ProceduralStyle"]   = static_cast<int>(slot.proceduralStyle);
			emittersArray.push_back(s);
		}
		j["Emitters"] = emittersArray;

		// - Serialize Ribbon Trail Module
		nlohmann::json trailJson;
		trailJson["Enabled"]              = m_trail.enabled;
		trailJson["HasCustomOffsets"]     = m_trail.hasCustomOffsets;
		trailJson["ShowDebugBladeLine"]   = m_trail.showDebugBladeLine;
		trailJson["EnablePhase2Boost"]    = m_trail.enablePhase2Boost;
		trailJson["ShaderName"]           = m_trail.shaderName;
		trailJson["BlendMode"]            = static_cast<int>(m_trail.blendMode);
		trailJson["BaseOffset"]           = { m_trail.baseOffset.x, m_trail.baseOffset.y, m_trail.baseOffset.z };
		trailJson["TipOffset"]            = { m_trail.tipOffset.x, m_trail.tipOffset.y, m_trail.tipOffset.z };
		trailJson["Lifetime"]             = m_trail.lifetime;
		trailJson["Subdivisions"]         = m_trail.subdivisions;
		trailJson["MinVertexDistance"]    = m_trail.minVertexDistance;
		trailJson["CoreColor"]            = { m_trail.coreColor.x, m_trail.coreColor.y, m_trail.coreColor.z, m_trail.coreColor.w };
		trailJson["EdgeColor"]            = { m_trail.edgeColor.x, m_trail.edgeColor.y, m_trail.edgeColor.z, m_trail.edgeColor.w };
		trailJson["EmissiveIntensity"]    = m_trail.emissiveIntensity;
		trailJson["TipFalloffPower"]      = m_trail.tipFalloffPower;
		trailJson["NoiseScale"]           = m_trail.noiseScale;
		trailJson["NoiseScrollSpeed"]     = m_trail.noiseScrollSpeed;
		trailJson["ErosionStrength"]      = m_trail.erosionStrength;
		trailJson["LeadingEdgeSharpness"] = m_trail.leadingEdgeSharpness;
		j["TrailModule"]                  = trailJson;
	}

	void ParticleComponent::Deserialize(const nlohmann::json& data)
	{
		if (data.contains("AssetPath") && data["AssetPath"].is_string() && !data["AssetPath"].get<std::string>().empty())
		{
			m_assetPath = data["AssetPath"].get<std::string>();
			LoadAsset(m_assetPath);

			// - Apply per-actor geometric overrides if defined in actor data
			if (data.contains("HasCustomOffsets") && data["HasCustomOffsets"].is_boolean())
			{
				m_trail.hasCustomOffsets = data["HasCustomOffsets"].get<bool>();
			}
			if (data.contains("BaseOffset") && data["BaseOffset"].is_array() && data["BaseOffset"].size() >= 3)
			{
				m_trail.baseOffset = DirectX::SimpleMath::Vector3(
					data["BaseOffset"][0].is_number() ? data["BaseOffset"][0].get<float>() : 0.0f,
					data["BaseOffset"][1].is_number() ? data["BaseOffset"][1].get<float>() : 0.0f,
					data["BaseOffset"][2].is_number() ? data["BaseOffset"][2].get<float>() : 0.0f
				);
			}
			if (data.contains("TipOffset") && data["TipOffset"].is_array() && data["TipOffset"].size() >= 3)
			{
				m_trail.tipOffset = DirectX::SimpleMath::Vector3(
					data["TipOffset"][0].is_number() ? data["TipOffset"][0].get<float>() : 0.0f,
					data["TipOffset"][1].is_number() ? data["TipOffset"][1].get<float>() : 0.0f,
					data["TipOffset"][2].is_number() ? data["TipOffset"][2].get<float>() : 0.0f
				);
			}
			return;
		}

		DeserializeData(data);
	}

	void ParticleComponent::DeserializeData(const nlohmann::json& data)
	{
		if (data.contains("ImpactHeightOffset") && data["ImpactHeightOffset"].is_number())
		{
			m_impactHeightOffset = data["ImpactHeightOffset"].get<float>();
		}

		// - Deserialize Emitters
		if (data.contains("Emitters") && data["Emitters"].is_array())
		{
			m_emitters.clear();
			for (const auto& s : data["Emitters"])
			{
				if (!s.is_object()) continue;
				ParticleEmitterSlot slot;
				if (s.contains("SlotName") && s["SlotName"].is_string()) slot.slotName = s["SlotName"].get<std::string>();
				if (s.contains("TriggerCue") && s["TriggerCue"].is_string()) slot.triggerCue = s["TriggerCue"].get<std::string>();
				if (s.contains("ShaderName") && s["ShaderName"].is_string()) slot.shaderName = s["ShaderName"].get<std::string>();
				else if (s.contains("PixelShaderName") && s["PixelShaderName"].is_string()) slot.shaderName = s["PixelShaderName"].get<std::string>();
				if (s.contains("CustomTexturePath") && s["CustomTexturePath"].is_string()) slot.customTexturePath = s["CustomTexturePath"].get<std::string>();
				if (s.contains("BlendMode") && s["BlendMode"].is_number()) slot.blendMode = static_cast<ParticleBlendMode>(s["BlendMode"].get<int>());
				if (s.contains("Shape") && s["Shape"].is_number()) slot.shape = static_cast<ParticleSpawnShape>(s["Shape"].get<int>());
				if (s.contains("Enabled") && s["Enabled"].is_boolean()) slot.enabled = s["Enabled"].get<bool>();
				if (s.contains("Looping") && s["Looping"].is_boolean()) slot.looping = s["Looping"].get<bool>();
				if (s.contains("RateOverTime") && s["RateOverTime"].is_number()) slot.rateOverTime = s["RateOverTime"].get<float>();
				if (s.contains("BurstCount") && s["BurstCount"].is_number()) slot.burstCount = s["BurstCount"].get<int>();
				if (s.contains("SpreadCone") && s["SpreadCone"].is_number()) slot.spreadCone = s["SpreadCone"].get<float>();
				if (s.contains("SpawnRadius") && s["SpawnRadius"].is_number()) slot.spawnRadius = s["SpawnRadius"].get<float>();
				if (s.contains("LocalOffset") && s["LocalOffset"].is_array() && s["LocalOffset"].size() >= 3)
				{
					slot.localOffset = DirectX::SimpleMath::Vector3(
						s["LocalOffset"][0].is_number() ? s["LocalOffset"][0].get<float>() : 0.0f,
						s["LocalOffset"][1].is_number() ? s["LocalOffset"][1].get<float>() : 0.0f,
						s["LocalOffset"][2].is_number() ? s["LocalOffset"][2].get<float>() : 0.0f
					);
				}
				if (s.contains("SpeedMin") && s["SpeedMin"].is_number()) slot.speedMin = s["SpeedMin"].get<float>();
				if (s.contains("SpeedMax") && s["SpeedMax"].is_number()) slot.speedMax = s["SpeedMax"].get<float>();
				if (s.contains("LifetimeMin") && s["LifetimeMin"].is_number()) slot.lifetimeMin = s["LifetimeMin"].get<float>();
				if (s.contains("LifetimeMax") && s["LifetimeMax"].is_number()) slot.lifetimeMax = s["LifetimeMax"].get<float>();
				if (s.contains("Drag") && s["Drag"].is_number()) slot.drag = s["Drag"].get<float>();
				if (s.contains("Gravity"))
				{
					if (s["Gravity"].is_number())
					{
						slot.gravity = s["Gravity"].get<float>();
					}
					else if (s["Gravity"].is_array() && s["Gravity"].size() >= 2)
					{
						float gy = s["Gravity"][1].is_number() ? s["Gravity"][1].get<float>() : 0.0f;
						slot.gravity = (gy < 0.0f) ? -gy : gy;
					}
				}
				if (s.contains("StretchFactor") && s["StretchFactor"].is_number()) slot.stretchFactor = s["StretchFactor"].get<float>();
				if (s.contains("SizeStart") && s["SizeStart"].is_number()) slot.sizeStart = s["SizeStart"].get<float>();
				if (s.contains("SizeEnd") && s["SizeEnd"].is_number()) slot.sizeEnd = s["SizeEnd"].get<float>();

				// - Support both StartColor and legacy ColorStart
				if (s.contains("StartColor") && s["StartColor"].is_array() && s["StartColor"].size() >= 4)
				{
					slot.startColor = DirectX::SimpleMath::Vector4(
						s["StartColor"][0].is_number() ? s["StartColor"][0].get<float>() : 1.0f,
						s["StartColor"][1].is_number() ? s["StartColor"][1].get<float>() : 1.0f,
						s["StartColor"][2].is_number() ? s["StartColor"][2].get<float>() : 1.0f,
						s["StartColor"][3].is_number() ? s["StartColor"][3].get<float>() : 1.0f
					);
				}
				else if (s.contains("ColorStart") && s["ColorStart"].is_array() && s["ColorStart"].size() >= 4)
				{
					slot.startColor = DirectX::SimpleMath::Vector4(
						s["ColorStart"][0].is_number() ? s["ColorStart"][0].get<float>() : 1.0f,
						s["ColorStart"][1].is_number() ? s["ColorStart"][1].get<float>() : 1.0f,
						s["ColorStart"][2].is_number() ? s["ColorStart"][2].get<float>() : 1.0f,
						s["ColorStart"][3].is_number() ? s["ColorStart"][3].get<float>() : 1.0f
					);
				}

				// - Support both EndColor and legacy ColorEnd
				if (s.contains("EndColor") && s["EndColor"].is_array() && s["EndColor"].size() >= 4)
				{
					slot.endColor = DirectX::SimpleMath::Vector4(
						s["EndColor"][0].is_number() ? s["EndColor"][0].get<float>() : 1.0f,
						s["EndColor"][1].is_number() ? s["EndColor"][1].get<float>() : 1.0f,
						s["EndColor"][2].is_number() ? s["EndColor"][2].get<float>() : 1.0f,
						s["EndColor"][3].is_number() ? s["EndColor"][3].get<float>() : 0.0f
					);
				}
				else if (s.contains("ColorEnd") && s["ColorEnd"].is_array() && s["ColorEnd"].size() >= 4)
				{
					slot.endColor = DirectX::SimpleMath::Vector4(
						s["ColorEnd"][0].is_number() ? s["ColorEnd"][0].get<float>() : 1.0f,
						s["ColorEnd"][1].is_number() ? s["ColorEnd"][1].get<float>() : 1.0f,
						s["ColorEnd"][2].is_number() ? s["ColorEnd"][2].get<float>() : 1.0f,
						s["ColorEnd"][3].is_number() ? s["ColorEnd"][3].get<float>() : 0.0f
					);
				}

				if (s.contains("ShaderParams") && s["ShaderParams"].is_array() && s["ShaderParams"].size() >= 4)
				{
					slot.shaderParams = DirectX::SimpleMath::Vector4(
						s["ShaderParams"][0].is_number() ? s["ShaderParams"][0].get<float>() : 1.0f,
						s["ShaderParams"][1].is_number() ? s["ShaderParams"][1].get<float>() : 1.0f,
						s["ShaderParams"][2].is_number() ? s["ShaderParams"][2].get<float>() : 1.0f,
						s["ShaderParams"][3].is_number() ? s["ShaderParams"][3].get<float>() : 0.0f
					);
				}

				if (s.contains("ProceduralStyle") && s["ProceduralStyle"].is_number())
				{
					slot.proceduralStyle = static_cast<ParticleProceduralStyle>(s["ProceduralStyle"].get<int>());
				}
				m_emitters.push_back(slot);
			}
		}

		// - Deserialize Trail Module
		const nlohmann::json* trailSrc = nullptr;
		if (data.contains("TrailModule") && data["TrailModule"].is_object())
		{
			trailSrc = &data["TrailModule"];
		}
		else if (data.contains("Type") && data["Type"] == "WeaponTrailComponent")
		{
			trailSrc = &data;
			m_trail.enabled = true;
		}

		if (trailSrc)
		{
			const auto& t = *trailSrc;
			if (t.contains("Enabled") && t["Enabled"].is_boolean()) m_trail.enabled = t["Enabled"].get<bool>();
			if (t.contains("HasCustomOffsets") && t["HasCustomOffsets"].is_boolean()) m_trail.hasCustomOffsets = t["HasCustomOffsets"].get<bool>();
			if (t.contains("ShowDebugBladeLine") && t["ShowDebugBladeLine"].is_boolean()) m_trail.showDebugBladeLine = t["ShowDebugBladeLine"].get<bool>();
			if (t.contains("EnablePhase2Boost") && t["EnablePhase2Boost"].is_boolean()) m_trail.enablePhase2Boost = t["EnablePhase2Boost"].get<bool>();
			if (t.contains("ShaderName") && t["ShaderName"].is_string()) m_trail.shaderName = t["ShaderName"].get<std::string>();
			if (t.contains("BlendMode") && t["BlendMode"].is_number()) m_trail.blendMode = static_cast<ParticleBlendMode>(t["BlendMode"].get<int>());
			if (t.contains("Lifetime") && t["Lifetime"].is_number()) m_trail.lifetime = t["Lifetime"].get<float>();
			if (t.contains("Subdivisions") && t["Subdivisions"].is_number()) m_trail.subdivisions = t["Subdivisions"].get<int>();
			if (t.contains("MinVertexDistance") && t["MinVertexDistance"].is_number()) m_trail.minVertexDistance = t["MinVertexDistance"].get<float>();
			if (t.contains("EmissiveIntensity") && t["EmissiveIntensity"].is_number()) m_trail.emissiveIntensity = t["EmissiveIntensity"].get<float>();
			if (t.contains("TipFalloffPower") && t["TipFalloffPower"].is_number()) m_trail.tipFalloffPower = t["TipFalloffPower"].get<float>();
			if (t.contains("NoiseScale") && t["NoiseScale"].is_number()) m_trail.noiseScale = t["NoiseScale"].get<float>();
			if (t.contains("NoiseScrollSpeed") && t["NoiseScrollSpeed"].is_number()) m_trail.noiseScrollSpeed = t["NoiseScrollSpeed"].get<float>();
			if (t.contains("ErosionStrength") && t["ErosionStrength"].is_number()) m_trail.erosionStrength = t["ErosionStrength"].get<float>();
			if (t.contains("LeadingEdgeSharpness") && t["LeadingEdgeSharpness"].is_number()) m_trail.leadingEdgeSharpness = t["LeadingEdgeSharpness"].get<float>();

			if (t.contains("BaseOffset") && t["BaseOffset"].is_array() && t["BaseOffset"].size() >= 3)
			{
				m_trail.baseOffset = DirectX::SimpleMath::Vector3(
					t["BaseOffset"][0].is_number() ? t["BaseOffset"][0].get<float>() : 0.0f,
					t["BaseOffset"][1].is_number() ? t["BaseOffset"][1].get<float>() : 0.0f,
					t["BaseOffset"][2].is_number() ? t["BaseOffset"][2].get<float>() : 0.0f
				);
			}
			if (t.contains("TipOffset") && t["TipOffset"].is_array() && t["TipOffset"].size() >= 3)
			{
				m_trail.tipOffset = DirectX::SimpleMath::Vector3(
					t["TipOffset"][0].is_number() ? t["TipOffset"][0].get<float>() : 0.0f,
					t["TipOffset"][1].is_number() ? t["TipOffset"][1].get<float>() : 0.0f,
					t["TipOffset"][2].is_number() ? t["TipOffset"][2].get<float>() : 0.0f
				);
			}
			if (t.contains("CoreColor") && t["CoreColor"].is_array() && t["CoreColor"].size() >= 4)
			{
				m_trail.coreColor = DirectX::SimpleMath::Vector4(
					t["CoreColor"][0].is_number() ? t["CoreColor"][0].get<float>() : 1.0f,
					t["CoreColor"][1].is_number() ? t["CoreColor"][1].get<float>() : 1.0f,
					t["CoreColor"][2].is_number() ? t["CoreColor"][2].get<float>() : 1.0f,
					t["CoreColor"][3].is_number() ? t["CoreColor"][3].get<float>() : 1.0f
				);
			}
			if (t.contains("EdgeColor") && t["EdgeColor"].is_array() && t["EdgeColor"].size() >= 4)
			{
				m_trail.edgeColor = DirectX::SimpleMath::Vector4(
					t["EdgeColor"][0].is_number() ? t["EdgeColor"][0].get<float>() : 1.0f,
					t["EdgeColor"][1].is_number() ? t["EdgeColor"][1].get<float>() : 1.0f,
					t["EdgeColor"][2].is_number() ? t["EdgeColor"][2].get<float>() : 1.0f,
					t["EdgeColor"][3].is_number() ? t["EdgeColor"][3].get<float>() : 1.0f
				);
			}
		}
	}

	void ParticleComponent::OnInspectorGUI(GameContext& /*gameContext*/)
	{
#if HEIN_WITH_EDITOR
		ImGui::PushID(this);

		ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "Unified Modular Particle & Trail System");
		ImGui::Separator();

		char assetPathBuf[256];
		strncpy_s(assetPathBuf, m_assetPath.c_str(), sizeof(assetPathBuf) - 1);
		if (ImGui::InputText("VFX Asset Path", assetPathBuf, sizeof(assetPathBuf)))
		{
			m_assetPath = assetPathBuf;
		}
		if (ImGui::Button("Load VFX Asset"))
		{
			LoadAsset(m_assetPath);
		}
		ImGui::SameLine();
		if (ImGui::Button("Save VFX Asset"))
		{
			SaveAsset(m_assetPath);
		}

		const char* kPresetPaths[] = {
			"Resources/Data/VFX/CombatImpact.json",
			"Resources/Data/VFX/GoldenSlashTrail.json",
			"Resources/Data/VFX/BloodflameSlashTrail.json",
			"Resources/Data/VFX/ColdSteelTrail.json",
			"Resources/Data/VFX/SoulGhost.json",
			"Resources/Data/VFX/StoneDebris.json",
			"Resources/Data/VFX/FireEmbers.json"
		};
		const char* kPresetNames[] = {
			"Combat Impact (Blood/Sparks/Parry)",
			"Golden Slash Trail",
			"Bloodflame Slash Trail",
			"Cold Steel Trail",
			"Soul Ghost Wisps",
			"Stone Debris",
			"Fire Embers"
		};
		static int s_presetIdx = 0;
		if (ImGui::Combo("Preset VFX Asset", &s_presetIdx, kPresetNames, IM_ARRAYSIZE(kPresetNames)))
		{
			m_assetPath = kPresetPaths[s_presetIdx];
			LoadAsset(m_assetPath);
		}
		ImGui::Separator();

		ImGui::DragFloat("Impact Height Offset", &m_impactHeightOffset, 0.1f, -20.0f, 50.0f, "%.2f m");
		ImGui::Text("Active Billboard Particles: %zu / %zu", m_gameplayPool.activeCount, kMaxParticles);

		// - Module A: GPU Billboard Emitters Section
		if (ImGui::CollapsingHeader("GPU Billboard Emitters", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button("Preset: Default Combat")) ApplyDefaultCombatPreset();
			ImGui::SameLine();
			if (ImGui::Button("Preset: Soul Ghost")) ApplySoulGhostPreset();
			ImGui::SameLine();
			if (ImGui::Button("Preset: Stone")) ApplyStoneDebrisPreset();
			ImGui::SameLine();
			if (ImGui::Button("Preset: Fire")) ApplyFireEmberPreset();

			if (ImGui::Button("+ Add Emitter Slot"))
			{
				ParticleEmitterSlot newSlot;
				newSlot.slotName = "Emitter_" + std::to_string(m_emitters.size() + 1);
				m_emitters.push_back(newSlot);
			}

			for (size_t i = 0; i < m_emitters.size(); ++i)
			{
				auto& slot = m_emitters[i];
				ImGui::PushID(static_cast<int>(i));

				std::string headerName = slot.slotName + " [" + slot.triggerCue + "]###Slot_" + std::to_string(i);
				if (ImGui::TreeNode(headerName.c_str()))
				{
					char nameBuf[64];
					strncpy_s(nameBuf, slot.slotName.c_str(), sizeof(nameBuf) - 1);
					if (ImGui::InputText("Slot Name", nameBuf, sizeof(nameBuf)))
					{
						slot.slotName = nameBuf;
					}

					char cueBuf[64];
					strncpy_s(cueBuf, slot.triggerCue.c_str(), sizeof(cueBuf) - 1);
					if (ImGui::InputText("Trigger Cue", cueBuf, sizeof(cueBuf)))
					{
						slot.triggerCue = cueBuf;
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("OnDamage")) slot.triggerCue = "OnDamage";
					ImGui::SameLine();
					if (ImGui::SmallButton("OnBlock")) slot.triggerCue = "OnBlock";
					ImGui::SameLine();
					if (ImGui::SmallButton("OnParry")) slot.triggerCue = "OnParry";

					ImGui::Checkbox("Enabled", &slot.enabled);
					ImGui::SameLine();
					ImGui::Checkbox("Looping", &slot.looping);
					if (slot.looping)
					{
						ImGui::DragFloat("Rate Over Time", &slot.rateOverTime, 0.5f, 0.0f, 200.0f, "%.1f /s");
					}

					char shaderBuf[64];
					strncpy_s(shaderBuf, slot.shaderName.c_str(), sizeof(shaderBuf) - 1);
					if (ImGui::InputText("Pixel Shader (.cso / .hlsl)", shaderBuf, sizeof(shaderBuf)))
					{
						slot.shaderName = shaderBuf;
					}
					ImGui::SameLine();
					if (ImGui::BeginCombo("##ShaderQuickPick", "Select..."))
					{
						if (ImGui::Selectable("Particle_Eroded_PS")) slot.shaderName = "Particle_Eroded_PS";
						if (ImGui::Selectable("Particle_Streak_PS")) slot.shaderName = "Particle_Streak_PS";
						if (ImGui::Selectable("Particle_Wisp_PS"))   slot.shaderName = "Particle_Wisp_PS";
						if (ImGui::Selectable("Particle_Ring_PS"))   slot.shaderName = "Particle_Ring_PS";
						ImGui::EndCombo();
					}

					int styleIdx = static_cast<int>(slot.proceduralStyle);
					const char* styleNames[] = {
						"Eroded Droplet (Blood / Mud / Debris)",
						"Streak Spark (Sparks / Clash / Lightning)",
						"Soft Wisp (Fire / Souls / Embers)",
						"Shock Ring (Parry / Blast Wave)"
					};
					if (ImGui::Combo("Procedural Style", &styleIdx, styleNames, IM_ARRAYSIZE(styleNames)))
					{
						slot.proceduralStyle = static_cast<ParticleProceduralStyle>(styleIdx);
					}

					int blendIdx = static_cast<int>(slot.blendMode);
					const char* blendNames[] = { "AlphaBlend (Eroded / Debris)", "Additive (Sparks / Fire / Wisps)" };
					if (ImGui::Combo("Blend Mode", &blendIdx, blendNames, 2))
					{
						slot.blendMode = static_cast<ParticleBlendMode>(blendIdx);
					}

					int shapeIdx = static_cast<int>(slot.shape);
					const char* shapeNames[] = { "Cone (Directional)", "Sphere (3D Burst)", "Hemisphere (Upward)", "Point (Flash / Ring)" };
					if (ImGui::Combo("Emission Shape", &shapeIdx, shapeNames, 4))
					{
						slot.shape = static_cast<ParticleSpawnShape>(shapeIdx);
					}

					ImGui::DragInt("Burst Count", &slot.burstCount, 1, 1, 128);
					ImGui::DragFloat("Spread Cone", &slot.spreadCone, 0.05f, 0.0f, 3.14f, "%.2f rad");
					ImGui::DragFloat("Spawn Radius", &slot.spawnRadius, 0.05f, 0.0f, 5.0f, "%.2f m");
					ImGui::DragFloat3("Local Offset", &slot.localOffset.x, 0.05f);

					ImGui::Separator();
					ImGui::Text("Kinematics (Evaluated on GPU)");
					ImGui::DragFloatRange2("Speed Min / Max", &slot.speedMin, &slot.speedMax, 0.5f, 0.0f, 150.0f, "Min: %.1f", "Max: %.1f");
					ImGui::DragFloatRange2("Lifetime Min / Max", &slot.lifetimeMin, &slot.lifetimeMax, 0.02f, 0.02f, 5.0f, "Min: %.2fs", "Max: %.2fs");
					ImGui::DragFloat("Air Drag (k)", &slot.drag, 0.1f, 0.0f, 20.0f, "%.1f");
					ImGui::DragFloat("Gravity Accel", &slot.gravity, 0.5f, -50.0f, 150.0f, "%.1f m/s²");
					ImGui::DragFloat("Velocity Stretch Factor", &slot.stretchFactor, 0.005f, 0.0f, 0.5f, "%.3f");

					ImGui::Separator();
					ImGui::Text("Over Lifetime & Shader Parameters");
					ImGui::DragFloat("Size Start", &slot.sizeStart, 0.02f, 0.01f, 10.0f, "%.2f");
					ImGui::DragFloat("Size End", &slot.sizeEnd, 0.02f, 0.0f, 10.0f, "%.2f");
					ImGui::ColorEdit4("Start Color", &slot.startColor.x, ImGuiColorEditFlags_Float);
					ImGui::ColorEdit4("End Color", &slot.endColor.x, ImGuiColorEditFlags_Float);
					ImGui::DragFloat("Emissive Intensity", &slot.shaderParams.x, 0.1f, 0.1f, 10.0f, "%.1f");
					ImGui::DragFloat("Noise Scale", &slot.shaderParams.y, 0.05f, 0.1f, 10.0f, "%.2f");
					ImGui::DragFloat("Erosion / Ring Sharpness", &slot.shaderParams.z, 0.05f, 0.0f, 5.0f, "%.2f");

					if (ImGui::Button("Test Emit This Slot"))
					{
						DirectX::SimpleMath::Vector3 origin = DirectX::SimpleMath::Vector3::Zero;
						if (auto* transform = m_owner->GetComponent<TransformComponent>())
						{
							origin = transform->GetWorldMatrix().Translation();
						}
						origin.y += m_impactHeightOffset;
						EmitSlot(i, origin, DirectX::SimpleMath::Vector3::UnitY, 1.0f);
					}
					ImGui::SameLine();
					if (ImGui::Button("Delete Slot"))
					{
						m_emitters.erase(m_emitters.begin() + i);
						ImGui::TreePop();
						ImGui::PopID();
						break;
					}

					ImGui::TreePop();
				}
				ImGui::PopID();
			}
		}

		// - Module B: Ribbon / Weapon Trail Section
		if (ImGui::CollapsingHeader("Ribbon / Weapon Trail Module", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Enable Ribbon Trail", &m_trail.enabled);
			if (m_trail.enabled)
			{
				ImGui::Checkbox("Always Emit (Editor Preview)", &m_alwaysEmitPreview);
				if (ImGui::Button("Preset: Tarnished Gold")) ApplyTarnishedGoldTrailPreset();
				ImGui::SameLine();
				if (ImGui::Button("Preset: Cold Steel")) ApplyColdSteelTrailPreset();
				ImGui::SameLine();
				if (ImGui::Button("Preset: Boss Bloodflame")) ApplyBossBloodflameTrailPreset();
				ImGui::SameLine();
				if (ImGui::Button("Preset: Black Flame")) ApplyBlackFlameTrailPreset();

				if (ImGui::Checkbox("Use Custom Offsets Override", &m_trail.hasCustomOffsets))
				{
					if (!m_trail.hasCustomOffsets)
					{
						AutoFitOffsetsFromWeapon();
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Auto-Calibrate Offsets from Weapon Collider"))
				{
					m_trail.hasCustomOffsets = false;
					AutoFitOffsetsFromWeapon();
				}

				if (ImGui::DragFloat3("Base Span Offset", &m_trail.baseOffset.x, 0.05f) ||
					ImGui::DragFloat3("Tip Span Offset", &m_trail.tipOffset.x, 0.05f))
				{
					m_trail.hasCustomOffsets = true;
				}
				ImGui::Checkbox("Show Blade Line in Scene View", &m_trail.showDebugBladeLine);
				ImGui::Checkbox("Enable Phase 2 Intensity Boost", &m_trail.enablePhase2Boost);

				int trailBlendIdx = static_cast<int>(m_trail.blendMode);
				const char* trailBlendNames[] = { "AlphaBlend (Black Flame)", "Additive (Holy / Cold / Bloodflame)" };
				if (ImGui::Combo("Trail Blend Mode", &trailBlendIdx, trailBlendNames, 2))
				{
					m_trail.blendMode = static_cast<ParticleBlendMode>(trailBlendIdx);
				}

				ImGui::DragFloat("Trail Lifetime", &m_trail.lifetime, 0.01f, 0.05f, 1.5f, "%.2f s");
				ImGui::SliderInt("Spline Subdivisions", &m_trail.subdivisions, 1, kMaxTrailSubdivs);
				ImGui::DragFloat("Min Vertex Distance", &m_trail.minVertexDistance, 0.02f, 0.02f, 1.0f, "%.2f m");

				ImGui::ColorEdit4("Core Glow Color", &m_trail.coreColor.x, ImGuiColorEditFlags_Float);
				ImGui::ColorEdit4("Edge Dissolve Color", &m_trail.edgeColor.x, ImGuiColorEditFlags_Float);
				ImGui::DragFloat("Emissive Multiplier", &m_trail.emissiveIntensity, 0.1f, 0.5f, 10.0f, "%.1f");
				ImGui::DragFloat("Tip Falloff Power", &m_trail.tipFalloffPower, 0.05f, 0.5f, 5.0f, "%.2f");
				ImGui::DragFloat("Noise Scale", &m_trail.noiseScale, 0.1f, 0.2f, 10.0f, "%.1f");
				ImGui::DragFloat("Noise Scroll Speed", &m_trail.noiseScrollSpeed, 0.1f, 0.0f, 15.0f, "%.1f");
				ImGui::DragFloat("Erosion Strength", &m_trail.erosionStrength, 0.02f, 0.0f, 1.0f, "%.2f");
				ImGui::DragFloat("Leading Edge Sharpness", &m_trail.leadingEdgeSharpness, 0.2f, 1.0f, 20.0f, "%.1f");
			}
		}

		ImGui::Separator();
		if (ImGui::Button("Trigger Cue: OnDamage"))
		{
			DirectX::SimpleMath::Vector3 pos = (m_owner && m_owner->GetComponent<TransformComponent>()) ? m_owner->GetComponent<TransformComponent>()->GetWorldMatrix().Translation() : DirectX::SimpleMath::Vector3::Zero;
			pos.y += m_impactHeightOffset;
			EmitCue("OnDamage", pos, DirectX::SimpleMath::Vector3::UnitY, 1.0f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Trigger Cue: OnBlock"))
		{
			DirectX::SimpleMath::Vector3 pos = (m_owner && m_owner->GetComponent<TransformComponent>()) ? m_owner->GetComponent<TransformComponent>()->GetWorldMatrix().Translation() : DirectX::SimpleMath::Vector3::Zero;
			pos.y += m_impactHeightOffset;
			EmitCue("OnBlock", pos, DirectX::SimpleMath::Vector3::UnitY, 1.0f);
		}
		ImGui::SameLine();
		if (ImGui::Button("Trigger Cue: OnParry"))
		{
			DirectX::SimpleMath::Vector3 pos = (m_owner && m_owner->GetComponent<TransformComponent>()) ? m_owner->GetComponent<TransformComponent>()->GetWorldMatrix().Translation() : DirectX::SimpleMath::Vector3::Zero;
			pos.y += m_impactHeightOffset;
			EmitCue("OnParry", pos, DirectX::SimpleMath::Vector3::UnitY, 1.0f);
		}

		ImGui::PopID();
#endif
	}

#if HEIN_WITH_EDITOR
	void ParticleComponent::TriggerEditorPreviewBurst()
	{
		DirectX::SimpleMath::Vector3 studioOrigin = DirectX::SimpleMath::Vector3(0.0f, 2.0f, 0.0f);
		DirectX::SimpleMath::Vector3 studioDir    = DirectX::SimpleMath::Vector3(0.0f, 0.45f, 1.0f);
		studioDir.Normalize();

		if (m_studioSoloMode == ParticleStudioSoloMode::SelectedSlot)
		{
			if (m_selectedSlotIndex >= 0 && m_selectedSlotIndex < static_cast<int>(m_emitters.size()))
			{
				SpawnSlotIntoPool(m_previewPool, static_cast<size_t>(m_selectedSlotIndex), studioOrigin, studioDir, m_previewIntensity);
			}
		}
		else if (m_studioSoloMode == ParticleStudioSoloMode::CueFilter)
		{
			for (size_t i = 0; i < m_emitters.size(); ++i)
			{
				if (m_emitters[i].enabled && m_emitters[i].triggerCue == m_previewCueFilter)
				{
					SpawnSlotIntoPool(m_previewPool, i, studioOrigin, studioDir, m_previewIntensity);
				}
			}
		}
		else if (m_studioSoloMode == ParticleStudioSoloMode::RibbonTrail)
		{
			// Reset slash phase for studio simulated slash arc
			m_previewSlashPhase = 0.0f;
		}
		else // Auto
		{
			// Emit all active slots matching default cue
			for (size_t i = 0; i < m_emitters.size(); ++i)
			{
				if (m_emitters[i].enabled)
				{
					SpawnSlotIntoPool(m_previewPool, i, studioOrigin, studioDir, m_previewIntensity);
				}
			}
			m_previewSlashPhase = 0.0f;
		}
	}

	void ParticleComponent::UpdateEditorPreview(float unscaledDeltaTime)
	{
		float dt = unscaledDeltaTime * m_previewTimeScale;
		m_previewPool.simTime += dt;

		// - Auto-looping burst timer
		if (m_previewAutoLoop)
		{
			m_previewLoopTimer += dt;
			if (m_previewLoopTimer >= m_previewLoopInterval)
			{
				m_previewLoopTimer = 0.0f;
				TriggerEditorPreviewBurst();
			}
		}

		// - Update Preview Ribbon Trail (Simulate dynamic 3D slash arc at studio origin)
		if (m_trail.enabled)
		{
			m_previewTrail.elapsedTime += dt;

			if (dt > 0.0f && m_previewTrail.count > 0)
			{
				for (size_t i = 0; i < m_previewTrail.count; ++i)
				{
					m_previewTrail.GetKnot(i).age += dt;
				}

				while (m_previewTrail.count > 0 && m_previewTrail.GetKnot(m_previewTrail.count - 1).age >= m_trail.lifetime)
				{
					--m_previewTrail.count;
				}
			}

			// Advance slash arc angle
			m_previewSlashPhase += dt * 4.5f;
			float angle = m_previewSlashPhase;

			float bladeRadius = 2.4f;
			DirectX::SimpleMath::Vector3 center(0.0f, 2.0f, 0.0f);
			DirectX::SimpleMath::Vector3 basePos = center + DirectX::SimpleMath::Vector3(std::cos(angle) * 0.4f, std::sin(angle * 1.5f) * 0.2f, std::sin(angle) * 0.4f);
			DirectX::SimpleMath::Vector3 tipPos  = center + DirectX::SimpleMath::Vector3(std::cos(angle) * bladeRadius, std::sin(angle * 1.5f) * 0.8f, std::sin(angle) * bladeRadius);

			m_previewTrail.PushKnot(basePos, tipPos);
		}
	}

	void ParticleComponent::RenderEditorPreviewPass(
		GameContext& gameContext,
		const DirectX::SimpleMath::Matrix& previewView,
		const DirectX::SimpleMath::Matrix& previewProj)
	{
		ID3D11Device* device = gameContext.deviceResources.GetD3DDevice();
		ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
		if (!device || !context) return;

		// - Update preview simulation state
		UpdateEditorPreview(gameContext.timer.GetElapsedSeconds() > 0.0 ? static_cast<float>(gameContext.timer.GetElapsedSeconds()) : 0.016f);

		// - Draw Billboard Emitters
		if (m_studioSoloMode != ParticleStudioSoloMode::RibbonTrail)
		{
			int soloSlot = (m_studioSoloMode == ParticleStudioSoloMode::SelectedSlot) ? m_selectedSlotIndex : -1;
			DrawParticlePool(device, context, gameContext.commonStates, m_previewPool, previewView, previewProj, soloSlot);
		}

		// - Draw Ribbon Trail
		if (m_trail.enabled && (m_studioSoloMode == ParticleStudioSoloMode::RibbonTrail || m_studioSoloMode == ParticleStudioSoloMode::Auto))
		{
			DrawTrailBuffer(device, context, gameContext.commonStates, m_previewTrail, previewView, previewProj, m_trail.enablePhase2Boost);
		}
	}

	void ParticleComponent::ResetPreviewCamera()
	{
		m_previewCamYaw    = 0.45f;
		m_previewCamPitch  = 0.30f;
		m_previewCamDist   = 24.0f;
		m_previewCamTarget = DirectX::SimpleMath::Vector3(0.0f, 2.0f, 0.0f);
	}

	void ParticleComponent::DrawParticleViewportOverlayUI(GameContext& /*gameContext*/, const ImVec2& contentPos, const ImVec2& contentSize)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();

		// - Top Banner Strip
		float bannerH = 26.0f;
		dl->AddRectFilled(contentPos, ImVec2(contentPos.x + contentSize.x, contentPos.y + bannerH), IM_COL32(16, 20, 26, 220));
		dl->AddText(ImVec2(contentPos.x + 10.0f, contentPos.y + 5.0f), IM_COL32(100, 215, 255, 255), "[ Particle Studio Viewport ]");

		// - Interactive Camera Orbit & Pan on Viewport Drag
		ImGui::SetCursorScreenPos(contentPos);
		ImGui::InvisibleButton("ParticleViewportCanvas", contentSize);
		bool isHovered = ImGui::IsItemHovered();
		bool isActive  = ImGui::IsItemActive();

		if (isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
		{
			ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
			ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);
			m_previewCamYaw   += delta.x * 0.008f;
			m_previewCamPitch  = std::clamp(m_previewCamPitch + delta.y * 0.008f, -1.45f, 1.45f);
		}
		if (isActive && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
		{
			ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
			ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
			m_previewCamTarget.y += delta.y * 0.02f;
		}
		if (isHovered)
		{
			float wheel = ImGui::GetIO().MouseWheel;
			if (std::abs(wheel) > 0.01f)
			{
				m_previewCamDist = std::clamp(m_previewCamDist - wheel * 1.5f, 2.0f, 80.0f);
			}
		}

		// - Control Strip Overlay (Top Left)
		ImVec2 ctrlPos(contentPos.x + 10.0f, contentPos.y + bannerH + 8.0f);
		ImGui::SetCursorScreenPos(ctrlPos);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.10f, 0.14f, 0.78f));
		if (ImGui::BeginChild("ParticleOverlayControls", ImVec2(340.0f, 130.0f), true, ImGuiWindowFlags_NoScrollbar))
		{
			if (ImGui::Button("Burst Now"))
			{
				TriggerEditorPreviewBurst();
			}
			ImGui::SameLine();
			ImGui::Checkbox("Auto-Loop", &m_previewAutoLoop);
			ImGui::SameLine();
			if (ImGui::SmallButton("Reset Cam"))
			{
				ResetPreviewCamera();
			}

			int soloModeIdx = static_cast<int>(m_studioSoloMode);
			const char* soloModeNames[] = { "Auto (All Active)", "Solo Selected Slot", "Solo Cue Filter", "Solo Ribbon Trail" };
			if (ImGui::Combo("Studio Mode", &soloModeIdx, soloModeNames, 4))
			{
				m_studioSoloMode = static_cast<ParticleStudioSoloMode>(soloModeIdx);
			}

			if (m_studioSoloMode == ParticleStudioSoloMode::SelectedSlot && !m_emitters.empty())
			{
				ImGui::SliderInt("Slot", &m_selectedSlotIndex, 0, static_cast<int>(m_emitters.size() - 1));
			}

			ImGui::SliderFloat("Speed", &m_previewTimeScale, 0.05f, 2.0f, "%.2fx");
			ImGui::SliderFloat("Interval", &m_previewLoopInterval, 0.1f, 3.0f, "%.2fs");
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();

		// - Bottom Navigation Hint
		const char* hint = "RMB: Orbit | MMB: Pan Y | Wheel: Zoom | Select Actor to inspect VFX";
		ImVec2 hintSize = ImGui::CalcTextSize(hint);
		dl->AddText(ImVec2(contentPos.x + contentSize.x - hintSize.x - 12.0f, contentPos.y + contentSize.y - hintSize.y - 6.0f), IM_COL32(175, 175, 175, 210), hint);
	}
#endif
}
