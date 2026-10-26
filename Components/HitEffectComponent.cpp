#include "pch.h"
#include "HitEffectComponent.h"
#include "Components/TransformComponent.h"
#include "Effect/ReadData.h"
#include "Framework/GameContext.h"
#include "ImGui/imgui.h"
#include <algorithm>
#include <cmath>
#include <d3dcompiler.h>
#include <random>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")

namespace HEIN
{
	void HitEffectComponent::EnsureSharedHitResources(ID3D11Device* device)
	{
		if (!device) return;
		if (s_cachedHitDevice != device)
		{
			s_sharedHitVS.Reset();
			s_sharedHitPS.Reset();
			s_sharedHitInputLayout.Reset();
			s_sharedHitSampler.Reset();
			s_sharedNoiseSRV.Reset();
			s_cachedHitDevice = device;
		}

		if (!s_sharedHitVS || !s_sharedHitPS || !s_sharedHitInputLayout)
		{
			std::vector<uint8_t> vsData;
			std::vector<uint8_t> psData;
			bool loadedCso = false;

			try
			{
				vsData = DX::ReadData(L"Resources/Shaders/HitEffect_VS.cso");
				psData = DX::ReadData(L"Resources/Shaders/HitEffect_PS.cso");
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
					vsData = DX::ReadData(L"Dual/Resources/Shaders/HitEffect_VS.cso");
					psData = DX::ReadData(L"Dual/Resources/Shaders/HitEffect_PS.cso");
					loadedCso = !vsData.empty() && !psData.empty();
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
					L"../External/Engine/Shaders/HitEffect_VS.hlsl",
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
						L"External/Engine/Shaders/HitEffect_VS.hlsl",
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
					L"../External/Engine/Shaders/HitEffect_PS.hlsl",
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
						L"External/Engine/Shaders/HitEffect_PS.hlsl",
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

			device->CreateVertexShader(vsBytecode, vsBytecodeSize, nullptr, s_sharedHitVS.ReleaseAndGetAddressOf());
			device->CreatePixelShader(psBytecode, psBytecodeSize, nullptr, s_sharedHitPS.ReleaseAndGetAddressOf());

			static const D3D11_INPUT_ELEMENT_DESC hitInputLayoutDesc[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};
			device->CreateInputLayout(
				hitInputLayoutDesc,
				static_cast<UINT>(_countof(hitInputLayoutDesc)),
				vsBytecode,
				vsBytecodeSize,
				s_sharedHitInputLayout.ReleaseAndGetAddressOf()
			);
		}

		if (!s_sharedHitSampler)
		{
			D3D11_SAMPLER_DESC sampDesc = {};
			sampDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			sampDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
			sampDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
			sampDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
			sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			sampDesc.MinLOD         = 0.0f;
			sampDesc.MaxLOD         = D3D11_FLOAT32_MAX;
			device->CreateSamplerState(&sampDesc, s_sharedHitSampler.ReleaseAndGetAddressOf());
		}

		if (!s_sharedNoiseSRV)
		{
			if (FAILED(DirectX::CreateDDSTextureFromFile(device, L"Resources/Textures/waternoise.dds", nullptr, s_sharedNoiseSRV.ReleaseAndGetAddressOf())))
			{
				DirectX::CreateDDSTextureFromFile(device, L"Dual/Resources/Textures/waternoise.dds", nullptr, s_sharedNoiseSRV.ReleaseAndGetAddressOf());
			}

			// Fallback: Procedural 64x64 cellular & turbulent organic splatter noise texture
			if (!s_sharedNoiseSRV)
			{
				constexpr int kTexDim = 64;
				std::vector<uint32_t> pixels(kTexDim * kTexDim);
				auto hash2D = [](int x, int y) -> float
				{
					int n = ((x & 63) * 73) ^ ((y & 63) * 199);
					n = (n << 13) ^ n;
					return 1.0f - static_cast<float>((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
				};

				for (int y = 0; y < kTexDim; ++y)
				{
					for (int x = 0; x < kTexDim; ++x)
					{
						float n0 = hash2D(x / 2, y / 2) * 0.5f + 0.5f;
						float n1 = hash2D(x, y) * 0.5f + 0.5f;
						float v = std::clamp(n0 * 0.65f + n1 * 0.35f, 0.0f, 1.0f);
						uint8_t c = static_cast<uint8_t>(v * 255.0f);
						pixels[y * kTexDim + x] = (255u << 24) | (c << 16) | (c << 8) | c;
					}
				}

				D3D11_TEXTURE2D_DESC texDesc = {};
				texDesc.Width            = kTexDim;
				texDesc.Height           = kTexDim;
				texDesc.MipLevels        = 1;
				texDesc.ArraySize        = 1;
				texDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
				texDesc.SampleDesc.Count = 1;
				texDesc.Usage            = D3D11_USAGE_IMMUTABLE;
				texDesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

				D3D11_SUBRESOURCE_DATA initData = {};
				initData.pSysMem     = pixels.data();
				initData.SysMemPitch = kTexDim * sizeof(uint32_t);

				Microsoft::WRL::ComPtr<ID3D11Texture2D> noiseTex;
				if (SUCCEEDED(device->CreateTexture2D(&texDesc, &initData, noiseTex.GetAddressOf())))
				{
					device->CreateShaderResourceView(noiseTex.Get(), nullptr, s_sharedNoiseSRV.ReleaseAndGetAddressOf());
				}
			}
		}
	}

	float HitEffectComponent::RandomRange(float minVal, float maxVal)
	{
		static thread_local std::mt19937 rng{ std::random_device{}() };
		std::uniform_real_distribution<float> dist(minVal, maxVal);
		return dist(rng);
	}

	HitEffectComponent::HitEffectComponent(Actor* owner)
		: IComponent(owner)
	{
	}

	void HitEffectComponent::Start()
	{
		Clear();
	}

	void HitEffectComponent::Clear()
	{
		for (auto& p : m_particles)
		{
			p.active = false;
		}
		m_nextCursor = 0;
		m_activeParticleCount = 0;
	}

	void HitEffectComponent::SpawnParticle(const HitParticle& p)
	{
		m_particles[m_nextCursor] = p;
		m_particles[m_nextCursor].active = true;
		m_nextCursor = (m_nextCursor + 1) % kMaxParticles;
	}

	void HitEffectComponent::EnsureGPUResources(ID3D11Device* device)
	{
		if (!device) return;
		EnsureSharedHitResources(device);

		if (!m_dynamicVB)
		{
			D3D11_BUFFER_DESC vbDesc = {};
			vbDesc.ByteWidth      = static_cast<UINT>(sizeof(HitEffectVertex) * kMaxParticles * 6);
			vbDesc.Usage          = D3D11_USAGE_DYNAMIC;
			vbDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
			vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			device->CreateBuffer(&vbDesc, nullptr, m_dynamicVB.ReleaseAndGetAddressOf());
		}

		if (!m_cbHitEffect)
		{
			D3D11_BUFFER_DESC cbDesc = {};
			cbDesc.ByteWidth      = sizeof(CB_HitEffectParams);
			cbDesc.Usage          = D3D11_USAGE_DYNAMIC;
			cbDesc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
			cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			device->CreateBuffer(&cbDesc, nullptr, m_cbHitEffect.ReleaseAndGetAddressOf());
		}
	}

	void HitEffectComponent::TriggerPreviewBurst(HitPreviewMode mode)
	{
		if (!m_owner) return;

		auto* trans = m_owner->GetComponent<TransformComponent>();
		DirectX::SimpleMath::Vector3 testPos = trans
			? (trans->GetPosition() + DirectX::SimpleMath::Vector3(0.0f, m_impactHeightOffset, 0.0f))
			: DirectX::SimpleMath::Vector3(0.0f, m_impactHeightOffset, 0.0f);

		const float yawRad = DirectX::XMConvertToRadians(m_previewYawDeg);
		DirectX::SimpleMath::Vector3 previewDir(std::sin(yawRad), 0.15f, std::cos(yawRad));
		if (previewDir.LengthSquared() > 0.0001f)
		{
			previewDir.Normalize();
		}

		switch (mode)
		{
		case HitPreviewMode::BloodBurst:
			EmitBlood(testPos, previewDir, m_previewIntensity);
			break;
		case HitPreviewMode::BlockSparks:
			EmitBlockSparks(testPos, previewDir, false, m_previewIntensity);
			break;
		case HitPreviewMode::ParrySparks:
			EmitBlockSparks(testPos, previewDir, true, m_previewIntensity);
			break;
		case HitPreviewMode::AlternatingCombo:
			if (m_previewComboToggle)
			{
				EmitBlockSparks(testPos, previewDir, true, m_previewIntensity);
			}
			else
			{
				EmitBlood(testPos, previewDir, m_previewIntensity);
			}
			m_previewComboToggle = !m_previewComboToggle;
			break;
		default:
			break;
		}
	}

	void HitEffectComponent::EmitBlood(
		const DirectX::SimpleMath::Vector3& worldPos,
		const DirectX::SimpleMath::Vector3& hitDirection,
		float intensityMultiplier)
	{
		if (!m_enableBlood) return;

		float clampedIntensity = std::clamp(intensityMultiplier, 0.4f, 3.0f);
		DirectX::SimpleMath::Vector3 baseDir = hitDirection;
		if (baseDir.LengthSquared() < 0.0001f)
		{
			baseDir = DirectX::SimpleMath::Vector3(0.0f, 0.4f, 1.0f);
		}
		baseDir.Normalize();

		int dropletCount = static_cast<int>(std::round(m_bloodParticleCount * clampedIntensity));
		for (int i = 0; i < dropletCount; ++i)
		{
			DirectX::SimpleMath::Vector3 jitter(
				RandomRange(-0.75f, 0.75f),
				RandomRange(0.10f, 0.95f),
				RandomRange(-0.75f, 0.75f)
			);
			DirectX::SimpleMath::Vector3 launchDir = baseDir + jitter;
			if (launchDir.LengthSquared() > 0.0001f)
			{
				launchDir.Normalize();
			}

			float speed = m_bloodSpeed * RandomRange(0.45f, 1.35f) * std::sqrt(clampedIntensity);

			HitParticle p;
			p.type          = HitParticleType::BloodDroplet;
			p.position      = worldPos + DirectX::SimpleMath::Vector3(RandomRange(-0.45f, 0.45f), RandomRange(-0.35f, 0.35f), RandomRange(-0.45f, 0.45f));
			p.velocity      = launchDir * speed;
			p.startColor    = m_bloodPrimaryColor;
			p.endColor      = DirectX::SimpleMath::Vector4(m_bloodSecondaryColor.x, m_bloodSecondaryColor.y, m_bloodSecondaryColor.z, 0.0f);
			p.age           = 0.0f;
			p.lifetime      = RandomRange(0.24f, 0.52f);
			p.startSize     = m_bloodSize * RandomRange(0.65f, 1.35f) * std::sqrt(clampedIntensity);
			p.endSize       = p.startSize * 0.30f;
			p.stretchFactor = 0.035f;
			p.drag          = 3.5f;
			p.gravityScale  = m_bloodGravity;
			p.randomSeed    = RandomRange(0.0f, 1.0f);
			SpawnParticle(p);
		}

		// Spawn volumetric blood mist puffs at the wound site
		int mistCount = (std::max)(5, dropletCount / 4);
		for (int i = 0; i < mistCount; ++i)
		{
			DirectX::SimpleMath::Vector3 mistVel(
				baseDir.x * RandomRange(2.5f, 8.5f) + RandomRange(-3.5f, 3.5f),
				RandomRange(1.0f, 5.5f),
				baseDir.z * RandomRange(2.5f, 8.5f) + RandomRange(-3.5f, 3.5f)
			);

			HitParticle mist;
			mist.type          = HitParticleType::BloodMist;
			mist.position      = worldPos + DirectX::SimpleMath::Vector3(RandomRange(-0.55f, 0.55f), RandomRange(-0.45f, 0.45f), RandomRange(-0.55f, 0.55f));
			mist.velocity      = mistVel;
			mist.startColor    = m_bloodSecondaryColor;
			mist.endColor      = DirectX::SimpleMath::Vector4(m_bloodSecondaryColor.x * 0.5f, 0.0f, 0.0f, 0.0f);
			mist.age           = 0.0f;
			mist.lifetime      = RandomRange(0.32f, 0.62f);
			mist.startSize     = m_bloodSize * RandomRange(1.8f, 2.8f) * std::sqrt(clampedIntensity);
			mist.endSize       = mist.startSize * 2.4f;
			mist.stretchFactor = 0.0f;
			mist.drag          = 6.5f;
			mist.gravityScale  = 3.5f;
			mist.randomSeed    = RandomRange(0.0f, 1.0f);
			SpawnParticle(mist);
		}
	}

	void HitEffectComponent::EmitBlockSparks(
		const DirectX::SimpleMath::Vector3& worldPos,
		const DirectX::SimpleMath::Vector3& blockNormal,
		bool isPerfectParry,
		float intensityMultiplier)
	{
		if (!m_enableSparks) return;

		float clampedIntensity = std::clamp(intensityMultiplier, 0.5f, 3.0f);
		DirectX::SimpleMath::Vector3 norm = blockNormal;
		if (norm.LengthSquared() < 0.0001f)
		{
			norm = DirectX::SimpleMath::Vector3(0.0f, 0.3f, 1.0f);
		}
		norm.Normalize();

		int baseCount = isPerfectParry ? m_parryParticleCount : m_sparkParticleCount;
		int count = static_cast<int>(std::round(baseCount * clampedIntensity));
		float speedBoost = isPerfectParry ? 1.35f : 1.0f;

		for (int i = 0; i < count; ++i)
		{
			// Wide metallic deflection cone with radial scatter
			DirectX::SimpleMath::Vector3 scatter(
				RandomRange(-1.15f, 1.15f),
				RandomRange(-0.35f, 1.25f),
				RandomRange(-1.15f, 1.15f)
			);
			DirectX::SimpleMath::Vector3 sparkDir = norm * 0.85f + scatter;
			if (sparkDir.LengthSquared() > 0.0001f)
			{
				sparkDir.Normalize();
			}

			float speed = m_sparkSpeed * speedBoost * RandomRange(0.45f, 1.45f);

			HitParticle spark;
			spark.type          = HitParticleType::SteelSpark;
			spark.position      = worldPos + DirectX::SimpleMath::Vector3(RandomRange(-0.25f, 0.25f), RandomRange(-0.25f, 0.25f), RandomRange(-0.25f, 0.25f));
			spark.velocity      = sparkDir * speed;
			spark.startColor    = isPerfectParry ? m_parryFlashColor : m_sparkCoreColor;
			spark.endColor      = DirectX::SimpleMath::Vector4(m_sparkEdgeColor.x, m_sparkEdgeColor.y, m_sparkEdgeColor.z, 0.0f);
			spark.age           = 0.0f;
			spark.lifetime      = RandomRange(0.15f, isPerfectParry ? 0.45f : 0.34f);
			spark.startSize     = m_sparkSize * (isPerfectParry ? 1.25f : 1.0f) * RandomRange(0.65f, 1.35f);
			spark.endSize       = spark.startSize * 0.18f;
			spark.stretchFactor = m_sparkStretch;
			spark.drag          = 2.2f;
			spark.gravityScale  = m_sparkGravity;
			spark.randomSeed    = RandomRange(0.0f, 1.0f);
			SpawnParticle(spark);
		}

		// Spawn instantaneous incandescent clash flash at the weapon contact point
		int flashCount = isPerfectParry ? 4 : 2;
		for (int i = 0; i < flashCount; ++i)
		{
			HitParticle flash;
			flash.type          = HitParticleType::ImpactFlash;
			flash.position      = worldPos;
			flash.velocity      = DirectX::SimpleMath::Vector3::Zero;
			flash.startColor    = isPerfectParry ? m_parryFlashColor : m_sparkCoreColor;
			flash.endColor      = DirectX::SimpleMath::Vector4(m_sparkEdgeColor.x, m_sparkEdgeColor.y, m_sparkEdgeColor.z, 0.0f);
			flash.age           = 0.0f;
			flash.lifetime      = isPerfectParry ? 0.15f : 0.10f;
			flash.startSize     = m_sparkSize * (isPerfectParry ? 4.8f : 3.0f) * (1.0f + i * 0.35f);
			flash.endSize       = flash.startSize * (isPerfectParry ? 2.4f : 1.6f);
			flash.stretchFactor = 0.0f;
			flash.drag          = 0.0f;
			flash.gravityScale  = 0.0f;
			flash.randomSeed    = RandomRange(0.0f, 1.0f);
			SpawnParticle(flash);
		}
	}

	void HitEffectComponent::Update(float /*deltaTime*/)
	{
		// Particle kinematics are stepped in LateUpdate(deltaTime) so blood and steel sparks
		// continue bursting at real-time speed during combat hitstop frames and Editor Mode preview.
	}

	void HitEffectComponent::LateUpdate(float deltaTime)
	{
		if (!m_owner || !m_owner->IsActive()) return;

		m_activeParticleCount = 0;
		for (const auto& p : m_particles)
		{
			if (p.active) ++m_activeParticleCount;
		}

		const bool isPreviewActive = (m_previewMode != HitPreviewMode::Off);
		if (deltaTime <= 0.0f && !isPreviewActive && m_activeParticleCount == 0)
		{
			return;
		}

		// Step in Editor Mode (deltaTime == 0.0f) whenever previewing or draining one-shot test bursts
		const float baseDt = (deltaTime > 0.0f) ? deltaTime : 0.016f;
		const float effectiveDt = baseDt * ((isPreviewActive || deltaTime <= 0.0f) ? m_previewTimeScale : 1.0f);

		if (isPreviewActive)
		{
			if (m_previewAutoRotate)
			{
				m_previewYawDeg += 90.0f * effectiveDt;
				if (m_previewYawDeg > 180.0f) m_previewYawDeg -= 360.0f;
			}

			m_previewTimer -= effectiveDt;
			if (m_previewTimer <= 0.0f)
			{
				TriggerPreviewBurst(m_previewMode);
				m_previewTimer = (std::max)(0.08f, m_previewInterval);
			}
		}

		m_activeParticleCount = 0;
		for (auto& p : m_particles)
		{
			if (!p.active) continue;

			p.age += effectiveDt;
			if (p.age >= p.lifetime)
			{
				p.active = false;
				continue;
			}

			// Exponential air drag and gravitational acceleration
			float dragMultiplier = std::exp(-p.drag * effectiveDt);
			p.velocity *= dragMultiplier;
			p.velocity.y -= p.gravityScale * effectiveDt;
			p.position += p.velocity * effectiveDt;
			++m_activeParticleCount;
		}
	}

	void HitEffectComponent::DrawTransparent(
		GameContext& gameContext,
		const DirectX::SimpleMath::Matrix& /*world*/,
		const DirectX::SimpleMath::Matrix& view,
		const DirectX::SimpleMath::Matrix& proj)
	{
		bool hasActive = false;
		for (const auto& p : m_particles)
		{
			if (p.active)
			{
				hasActive = true;
				break;
			}
		}
		if (!hasActive) return;

		auto* device  = gameContext.deviceResources.GetD3DDevice();
		auto* context = gameContext.deviceResources.GetD3DDeviceContext();
		if (!device || !context) return;

		EnsureGPUResources(device);
		if (!s_sharedHitVS || !s_sharedHitPS || !s_sharedHitInputLayout || !m_cbHitEffect || !m_dynamicVB)
		{
			return;
		}

		// Extract camera basis vectors from inverse view matrix
		DirectX::SimpleMath::Matrix invView = view.Invert();
		DirectX::SimpleMath::Vector3 camRight(invView._11, invView._12, invView._13);
		DirectX::SimpleMath::Vector3 camUp(invView._21, invView._22, invView._23);
		DirectX::SimpleMath::Vector3 camForward(invView._31, invView._32, invView._33);
		camRight.Normalize();
		camUp.Normalize();
		camForward.Normalize();

		auto UpdateConstantBuffer = [&](float passModeValue)
		{
			D3D11_MAPPED_SUBRESOURCE mappedCB = {};
			if (SUCCEEDED(context->Map(m_cbHitEffect.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedCB)))
			{
				auto* cb = static_cast<CB_HitEffectParams*>(mappedCB.pData);
				cb->viewProj          = view * proj;
				cb->emissiveIntensity = m_emissiveIntensity;
				cb->noiseScale        = m_noiseScale;
				cb->erosionStrength   = m_erosionStrength;
				cb->passMode          = passModeValue;
				context->Unmap(m_cbHitEffect.Get(), 0);
			}
		};

		auto BuildQuadVertices = [&](const HitParticle& p, HitEffectVertex* outVerts)
		{
			float t = std::clamp(p.age / (std::max)(0.001f, p.lifetime), 0.0f, 1.0f);
			float radius = p.startSize + (p.endSize - p.startSize) * t;
			DirectX::SimpleMath::Vector4 color = DirectX::SimpleMath::Vector4::Lerp(p.startColor, p.endColor, t);
			DirectX::SimpleMath::Vector2 params(t, p.randomSeed);

			DirectX::SimpleMath::Vector3 axisLong = camUp * radius;
			DirectX::SimpleMath::Vector3 axisLat  = camRight * radius;

			float speedSq = p.velocity.LengthSquared();
			if (p.stretchFactor > 0.0001f && speedSq > 0.01f)
			{
				float speed = std::sqrt(speedSq);
				DirectX::SimpleMath::Vector3 velDir = p.velocity / speed;
				DirectX::SimpleMath::Vector3 side = camForward.Cross(velDir);
				if (side.LengthSquared() < 0.0001f)
				{
					side = camRight;
				}
				else
				{
					side.Normalize();
				}

				float stretchedLength = radius * (1.0f + p.stretchFactor * speed);
				axisLong = velDir * stretchedLength;
				axisLat  = side * radius;
			}

			DirectX::SimpleMath::Vector3 v0 = p.position - axisLat - axisLong;
			DirectX::SimpleMath::Vector3 v1 = p.position - axisLat + axisLong;
			DirectX::SimpleMath::Vector3 v2 = p.position + axisLat - axisLong;
			DirectX::SimpleMath::Vector3 v3 = p.position + axisLat + axisLong;

			outVerts[0] = { v0, color, DirectX::SimpleMath::Vector2(-1.0f, -1.0f), params };
			outVerts[1] = { v1, color, DirectX::SimpleMath::Vector2(-1.0f,  1.0f), params };
			outVerts[2] = { v2, color, DirectX::SimpleMath::Vector2( 1.0f, -1.0f), params };
			outVerts[3] = { v2, color, DirectX::SimpleMath::Vector2( 1.0f, -1.0f), params };
			outVerts[4] = { v1, color, DirectX::SimpleMath::Vector2(-1.0f,  1.0f), params };
			outVerts[5] = { v3, color, DirectX::SimpleMath::Vector2( 1.0f,  1.0f), params };
		};

		// Configure common D3D11 pipeline state for billboard particles
		UINT stride = sizeof(HitEffectVertex);
		UINT offset = 0;
		context->IASetInputLayout(s_sharedHitInputLayout.Get());
		context->IASetVertexBuffers(0, 1, m_dynamicVB.GetAddressOf(), &stride, &offset);
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->VSSetShader(s_sharedHitVS.Get(), nullptr, 0);
		context->VSSetConstantBuffers(0, 1, m_cbHitEffect.GetAddressOf());
		context->PSSetShader(s_sharedHitPS.Get(), nullptr, 0);
		context->PSSetConstantBuffers(0, 1, m_cbHitEffect.GetAddressOf());

		ID3D11SamplerState* samplers[] = { s_sharedHitSampler.Get() };
		context->PSSetSamplers(0, 1, samplers);

		ID3D11ShaderResourceView* srvs[] = { s_sharedNoiseSRV.Get() };
		context->PSSetShaderResources(0, 1, srvs);

		context->RSSetState(gameContext.commonStates.CullNone());
		context->OMSetDepthStencilState(gameContext.commonStates.DepthRead(), 0);

		// - Pass AlphaBlend (PassMode = 0.0f): Organic Noise-Eroded Blood Mist & Droplets
		{
			UINT bloodVertexCount = 0;
			D3D11_MAPPED_SUBRESOURCE mappedVB = {};
			if (SUCCEEDED(context->Map(m_dynamicVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVB)))
			{
				auto* verts = static_cast<HitEffectVertex*>(mappedVB.pData);
				for (const auto& p : m_particles)
				{
					if (!p.active) continue;
					if (p.type == HitParticleType::BloodDroplet || p.type == HitParticleType::BloodMist)
					{
						BuildQuadVertices(p, &verts[bloodVertexCount]);
						bloodVertexCount += 6;
					}
				}
				context->Unmap(m_dynamicVB.Get(), 0);
			}

			if (bloodVertexCount > 0)
			{
				UpdateConstantBuffer(0.0f);
				context->OMSetBlendState(gameContext.commonStates.AlphaBlend(), nullptr, 0xFFFFFFFF);
				context->Draw(bloodVertexCount, 0);
			}
		}

		// - Pass Additive (PassMode = 1.0f): Incandescent Steel Sparks & Deflection Flash
		{
			UINT sparkVertexCount = 0;
			D3D11_MAPPED_SUBRESOURCE mappedVB = {};
			if (SUCCEEDED(context->Map(m_dynamicVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedVB)))
			{
				auto* verts = static_cast<HitEffectVertex*>(mappedVB.pData);
				for (const auto& p : m_particles)
				{
					if (!p.active) continue;
					if (p.type == HitParticleType::SteelSpark || p.type == HitParticleType::ImpactFlash)
					{
						BuildQuadVertices(p, &verts[sparkVertexCount]);
						sparkVertexCount += 6;
					}
				}
				context->Unmap(m_dynamicVB.Get(), 0);
			}

			if (sparkVertexCount > 0)
			{
				UpdateConstantBuffer(1.0f);
				context->OMSetBlendState(gameContext.commonStates.Additive(), nullptr, 0xFFFFFFFF);
				context->Draw(sparkVertexCount, 0);
			}
		}

		// Restore default pipeline states so subsequent passes are cleanly isolated
		context->RSSetState(gameContext.commonStates.CullCounterClockwise());
		context->OMSetDepthStencilState(gameContext.commonStates.DepthDefault(), 0);
		context->OMSetBlendState(gameContext.commonStates.Opaque(), nullptr, 0xFFFFFFFF);
	}

	void HitEffectComponent::OnInspectorGUI(GameContext& /*gameContext*/)
	{
		if (ImGui::CollapsingHeader("HitEffectComponent (Blood & Steel Sparks VFX)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// - Real-Time Editor Particle Preview & Slow-Motion Sequencer
			ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Real-Time Particle Preview (Works in Editor Mode)");
			float poolUsage = static_cast<float>(m_activeParticleCount) / static_cast<float>(kMaxParticles);
			char overlayBuf[64];
			snprintf(overlayBuf, sizeof(overlayBuf), "Active Particles: %zu / %zu", m_activeParticleCount, kMaxParticles);
			ImGui::ProgressBar(poolUsage, ImVec2(-1.0f, 18.0f), overlayBuf);

			const char* previewModes[] = {
				"Off (Gameplay Only)",
				"Loop: Blood Splatter Burst",
				"Loop: Block Steel Sparks",
				"Loop: Perfect Parry Flash",
				"Loop: Alternating Blood & Parry"
			};
			int currentMode = static_cast<int>(m_previewMode);
			if (ImGui::Combo("Loop Preview Mode", &currentMode, previewModes, IM_ARRAYSIZE(previewModes)))
			{
				m_previewMode = static_cast<HitPreviewMode>(currentMode);
				m_previewTimer = 0.0f;
			}

			ImGui::SliderFloat("Burst Interval (s)", &m_previewInterval, 0.12f, 2.50f, "%.2f s");
			ImGui::SliderFloat("Preview Time Scale", &m_previewTimeScale, 0.05f, 2.00f, "%.2fx (Slow-Mo)");
			ImGui::SliderFloat("Preview Intensity", &m_previewIntensity, 0.40f, 2.50f, "%.2fx");
			ImGui::SliderFloat("Impact Direction Yaw", &m_previewYawDeg, -180.0f, 180.0f, "%.0f deg");
			ImGui::SameLine();
			ImGui::Checkbox("Auto-Orbit", &m_previewAutoRotate);

			if (ImGui::Button("One-Shot Blood"))
			{
				TriggerPreviewBurst(HitPreviewMode::BloodBurst);
			}
			ImGui::SameLine();
			if (ImGui::Button("One-Shot Block"))
			{
				TriggerPreviewBurst(HitPreviewMode::BlockSparks);
			}
			ImGui::SameLine();
			if (ImGui::Button("One-Shot Parry"))
			{
				TriggerPreviewBurst(HitPreviewMode::ParrySparks);
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear Particles"))
			{
				Clear();
			}

			ImGui::Separator();
			ImGui::DragFloat("Impact Height Offset", &m_impactHeightOffset, 0.1f, 0.0f, 25.0f, "%.2f");

			ImGui::Separator();
			ImGui::Checkbox("Enable Blood Splatter", &m_enableBlood);
			if (m_enableBlood)
			{
				ImGui::ColorEdit4("Blood Primary Color", &m_bloodPrimaryColor.x);
				ImGui::ColorEdit4("Blood Mist Color", &m_bloodSecondaryColor.x);
				ImGui::SliderInt("Blood Particle Count", &m_bloodParticleCount, 4, 96);
				ImGui::DragFloat("Blood Burst Speed", &m_bloodSpeed, 0.5f, 1.0f, 100.0f, "%.1f");
				ImGui::DragFloat("Blood Droplet Size", &m_bloodSize, 0.02f, 0.05f, 4.0f, "%.2f");
				ImGui::DragFloat("Blood Gravity", &m_bloodGravity, 0.5f, 0.0f, 120.0f, "%.1f");
				ImGui::SliderFloat("Splatter Erosion Strength", &m_erosionStrength, 0.0f, 1.0f, "%.2f");
				ImGui::DragFloat("Noise Tiling Scale", &m_noiseScale, 0.05f, 0.2f, 8.0f, "%.2f");
			}

			ImGui::Separator();
			ImGui::Checkbox("Enable Block / Parry Sparks", &m_enableSparks);
			if (m_enableSparks)
			{
				ImGui::ColorEdit4("Spark Core Color", &m_sparkCoreColor.x);
				ImGui::ColorEdit4("Spark Edge Color", &m_sparkEdgeColor.x);
				ImGui::ColorEdit4("Parry Flash Color", &m_parryFlashColor.x);
				ImGui::SliderInt("Block Spark Count", &m_sparkParticleCount, 4, 96);
				ImGui::SliderInt("Parry Spark Count", &m_parryParticleCount, 8, 128);
				ImGui::DragFloat("Spark Speed", &m_sparkSpeed, 0.5f, 2.0f, 120.0f, "%.1f");
				ImGui::DragFloat("Spark Size", &m_sparkSize, 0.01f, 0.02f, 3.0f, "%.3f");
				ImGui::DragFloat("Spark Velocity Stretch", &m_sparkStretch, 0.002f, 0.0f, 0.35f, "%.3f");
				ImGui::DragFloat("Spark Gravity", &m_sparkGravity, 0.5f, 0.0f, 120.0f, "%.1f");
				ImGui::DragFloat("Spark HDR Emissive", &m_emissiveIntensity, 0.05f, 0.5f, 8.0f, "%.2f");
			}
		}
	}

	nlohmann::json HitEffectComponent::Serialize()
	{
		nlohmann::json j;
		j["EnableBlood"]          = m_enableBlood;
		j["BloodPrimaryColor"]    = { m_bloodPrimaryColor.x, m_bloodPrimaryColor.y, m_bloodPrimaryColor.z, m_bloodPrimaryColor.w };
		j["BloodSecondaryColor"]  = { m_bloodSecondaryColor.x, m_bloodSecondaryColor.y, m_bloodSecondaryColor.z, m_bloodSecondaryColor.w };
		j["BloodParticleCount"]   = m_bloodParticleCount;
		j["BloodSpeed"]           = m_bloodSpeed;
		j["BloodSize"]            = m_bloodSize;
		j["BloodGravity"]         = m_bloodGravity;
		j["ErosionStrength"]      = m_erosionStrength;
		j["NoiseScale"]           = m_noiseScale;
		j["EnableSparks"]         = m_enableSparks;
		j["SparkCoreColor"]       = { m_sparkCoreColor.x, m_sparkCoreColor.y, m_sparkCoreColor.z, m_sparkCoreColor.w };
		j["SparkEdgeColor"]       = { m_sparkEdgeColor.x, m_sparkEdgeColor.y, m_sparkEdgeColor.z, m_sparkEdgeColor.w };
		j["ParryFlashColor"]      = { m_parryFlashColor.x, m_parryFlashColor.y, m_parryFlashColor.z, m_parryFlashColor.w };
		j["SparkParticleCount"]   = m_sparkParticleCount;
		j["ParryParticleCount"]   = m_parryParticleCount;
		j["SparkSpeed"]           = m_sparkSpeed;
		j["SparkSize"]            = m_sparkSize;
		j["SparkStretch"]         = m_sparkStretch;
		j["SparkGravity"]         = m_sparkGravity;
		j["EmissiveIntensity"]    = m_emissiveIntensity;
		j["ImpactHeightOffset"]   = m_impactHeightOffset;
		return j;
	}

	void HitEffectComponent::Deserialize(const nlohmann::json& data)
	{
		if (data.contains("EnableBlood")) m_enableBlood = data["EnableBlood"].get<bool>();
		if (data.contains("BloodPrimaryColor") && data["BloodPrimaryColor"].is_array() && data["BloodPrimaryColor"].size() >= 4)
		{
			m_bloodPrimaryColor = DirectX::SimpleMath::Vector4(
				data["BloodPrimaryColor"][0].get<float>(),
				data["BloodPrimaryColor"][1].get<float>(),
				data["BloodPrimaryColor"][2].get<float>(),
				data["BloodPrimaryColor"][3].get<float>()
			);
		}
		if (data.contains("BloodSecondaryColor") && data["BloodSecondaryColor"].is_array() && data["BloodSecondaryColor"].size() >= 4)
		{
			m_bloodSecondaryColor = DirectX::SimpleMath::Vector4(
				data["BloodSecondaryColor"][0].get<float>(),
				data["BloodSecondaryColor"][1].get<float>(),
				data["BloodSecondaryColor"][2].get<float>(),
				data["BloodSecondaryColor"][3].get<float>()
			);
		}
		if (data.contains("BloodParticleCount")) m_bloodParticleCount = data["BloodParticleCount"].get<int>();
		if (data.contains("BloodSpeed"))         m_bloodSpeed = data["BloodSpeed"].get<float>();
		if (data.contains("BloodSize"))          m_bloodSize = data["BloodSize"].get<float>();
		if (data.contains("BloodGravity"))       m_bloodGravity = data["BloodGravity"].get<float>();
		if (data.contains("ErosionStrength"))    m_erosionStrength = data["ErosionStrength"].get<float>();
		if (data.contains("NoiseScale"))         m_noiseScale = data["NoiseScale"].get<float>();

		if (data.contains("EnableSparks")) m_enableSparks = data["EnableSparks"].get<bool>();
		if (data.contains("SparkCoreColor") && data["SparkCoreColor"].is_array() && data["SparkCoreColor"].size() >= 4)
		{
			m_sparkCoreColor = DirectX::SimpleMath::Vector4(
				data["SparkCoreColor"][0].get<float>(),
				data["SparkCoreColor"][1].get<float>(),
				data["SparkCoreColor"][2].get<float>(),
				data["SparkCoreColor"][3].get<float>()
			);
		}
		if (data.contains("SparkEdgeColor") && data["SparkEdgeColor"].is_array() && data["SparkEdgeColor"].size() >= 4)
		{
			m_sparkEdgeColor = DirectX::SimpleMath::Vector4(
				data["SparkEdgeColor"][0].get<float>(),
				data["SparkEdgeColor"][1].get<float>(),
				data["SparkEdgeColor"][2].get<float>(),
				data["SparkEdgeColor"][3].get<float>()
			);
		}
		if (data.contains("ParryFlashColor") && data["ParryFlashColor"].is_array() && data["ParryFlashColor"].size() >= 4)
		{
			m_parryFlashColor = DirectX::SimpleMath::Vector4(
				data["ParryFlashColor"][0].get<float>(),
				data["ParryFlashColor"][1].get<float>(),
				data["ParryFlashColor"][2].get<float>(),
				data["ParryFlashColor"][3].get<float>()
			);
		}
		if (data.contains("SparkParticleCount")) m_sparkParticleCount = data["SparkParticleCount"].get<int>();
		if (data.contains("ParryParticleCount")) m_parryParticleCount = data["ParryParticleCount"].get<int>();
		if (data.contains("SparkSpeed"))         m_sparkSpeed = data["SparkSpeed"].get<float>();
		if (data.contains("SparkSize"))          m_sparkSize = data["SparkSize"].get<float>();
		if (data.contains("SparkStretch"))       m_sparkStretch = data["SparkStretch"].get<float>();
		if (data.contains("SparkGravity"))       m_sparkGravity = data["SparkGravity"].get<float>();
		if (data.contains("EmissiveIntensity"))  m_emissiveIntensity = data["EmissiveIntensity"].get<float>();
		if (data.contains("ImpactHeightOffset")) m_impactHeightOffset = data["ImpactHeightOffset"].get<float>();
	}
}
