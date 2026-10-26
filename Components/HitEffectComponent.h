#pragma once
#include "IComponent.h"
#include "Entities/Actor.h"
#include <array>
#include <d3d11.h>
#include <wrl/client.h>
#include <SimpleMath.h>

namespace HEIN
{
	/**
	 * @enum HitParticleType
	 * @brief Categorizes combat impact particles for dual-pass rendering (Noise-eroded Blood vs HDR Steel Sparks).
	 */
	enum class HitParticleType : uint8_t
	{
		BloodDroplet,
		BloodMist,
		SteelSpark,
		ImpactFlash
	};

	/**
	 * @enum HitPreviewMode
	 * @brief Editor-only looping particle preview modes for real-time VFX authoring on any actor.
	 */
	enum class HitPreviewMode : int
	{
		Off = 0,
		BloodBurst,
		BlockSparks,
		ParrySparks,
		AlternatingCombo
	};

	/**
	 * @struct HitParticle
	 * @brief Fixed-size particle state stored in a pre-allocated ring buffer.
	 */
	struct HitParticle
	{
		DirectX::SimpleMath::Vector3 position   = DirectX::SimpleMath::Vector3::Zero;
		DirectX::SimpleMath::Vector3 velocity   = DirectX::SimpleMath::Vector3::Zero;
		DirectX::SimpleMath::Vector4 startColor = DirectX::SimpleMath::Vector4::One;
		DirectX::SimpleMath::Vector4 endColor   = DirectX::SimpleMath::Vector4::Zero;
		float age             = 0.0f;
		float lifetime        = 0.35f;
		float startSize       = 0.15f;
		float endSize         = 0.02f;
		float stretchFactor   = 0.08f;
		float drag            = 3.0f;
		float gravityScale    = 1.5f;
		float randomSeed      = 0.0f;
		HitParticleType type  = HitParticleType::BloodDroplet;
		bool active           = false;
	};

	/**
	 * @class HitEffectComponent
	 * @brief Engine-level 3D combat impact VFX component for blood splatter and block/parry steel sparks.
	 *
	 * Design Philosophy (Golden Rule Compliance):
	 * - Actor-Agnostic: Attaches to any target actor alongside HealthComponent without hardcoded actor identifiers.
	 * - Hitstop & Editor Invariant Stepping: Advances particle kinematics in LateUpdate(deltaTime) so sparks and blood
	 *   explode dynamically during combat hitstop frames as well as when paused in Editor Mode (F1).
	 * - Dedicated HLSL Pipeline: Uses HitEffect_VS.hlsl and HitEffect_PS.hlsl with dual-octave noise erosion and HDR emissive cores.
	 */
	class HitEffectComponent : public IComponent
	{
	public:
		struct HitEffectVertex
		{
			DirectX::SimpleMath::Vector3 position;
			DirectX::SimpleMath::Vector4 color;
			DirectX::SimpleMath::Vector2 uv;
			DirectX::SimpleMath::Vector2 params;
		};

		struct alignas(16) CB_HitEffectParams
		{
			DirectX::SimpleMath::Matrix viewProj;
			float emissiveIntensity;
			float noiseScale;
			float erosionStrength;
			float passMode;
		};

	private:
		static constexpr size_t kMaxParticles = 256;
		std::array<HitParticle, kMaxParticles> m_particles{};
		size_t m_nextCursor = 0;

		// - Blood Splatter & Mist Configuration
		bool m_enableBlood = true;
		DirectX::SimpleMath::Vector4 m_bloodPrimaryColor   = DirectX::SimpleMath::Vector4(0.68f, 0.02f, 0.02f, 0.96f);
		DirectX::SimpleMath::Vector4 m_bloodSecondaryColor = DirectX::SimpleMath::Vector4(0.28f, 0.01f, 0.01f, 0.82f);
		int m_bloodParticleCount = 30;
		float m_bloodSpeed       = 28.0f;
		float m_bloodSize        = 0.75f;
		float m_bloodGravity     = 48.0f;
		float m_erosionStrength  = 0.78f;
		float m_noiseScale       = 1.85f;

		// - Steel Spark & Deflection Flash Configuration
		bool m_enableSparks = true;
		DirectX::SimpleMath::Vector4 m_sparkCoreColor  = DirectX::SimpleMath::Vector4(1.00f, 0.95f, 0.76f, 1.0f);
		DirectX::SimpleMath::Vector4 m_sparkEdgeColor  = DirectX::SimpleMath::Vector4(1.00f, 0.42f, 0.06f, 1.0f);
		DirectX::SimpleMath::Vector4 m_parryFlashColor = DirectX::SimpleMath::Vector4(1.00f, 0.88f, 0.38f, 1.0f);
		int m_sparkParticleCount  = 36;
		int m_parryParticleCount  = 64;
		float m_sparkSpeed        = 46.0f;
		float m_sparkSize         = 0.42f;
		float m_sparkStretch      = 0.065f;
		float m_sparkGravity      = 55.0f;
		float m_emissiveIntensity = 2.40f;
		float m_impactHeightOffset = 9.5f;

		// - Editor Particle Preview State (Transient / Non-Serialized)
		HitPreviewMode m_previewMode = HitPreviewMode::Off;
		float m_previewInterval      = 0.65f;
		float m_previewTimer         = 0.0f;
		float m_previewTimeScale     = 1.0f;
		float m_previewIntensity     = 1.0f;
		float m_previewYawDeg        = 0.0f;
		bool  m_previewAutoRotate    = false;
		bool  m_previewComboToggle   = false;
		size_t m_activeParticleCount = 0;

		// - Per-Component Dynamic GPU Buffers
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_dynamicVB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbHitEffect;

		// - Shared Engine Shader & Noise Texture State
		inline static Microsoft::WRL::ComPtr<ID3D11VertexShader>       s_sharedHitVS;
		inline static Microsoft::WRL::ComPtr<ID3D11PixelShader>        s_sharedHitPS;
		inline static Microsoft::WRL::ComPtr<ID3D11InputLayout>        s_sharedHitInputLayout;
		inline static Microsoft::WRL::ComPtr<ID3D11SamplerState>       s_sharedHitSampler;
		inline static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> s_sharedNoiseSRV;
		inline static ID3D11Device*                                    s_cachedHitDevice = nullptr;

		static void EnsureSharedHitResources(ID3D11Device* device);
		static float RandomRange(float minVal, float maxVal);

		void SpawnParticle(const HitParticle& p);
		void EnsureGPUResources(ID3D11Device* device);
		void TriggerPreviewBurst(HitPreviewMode mode);

	public:
		HitEffectComponent(Actor* owner);
		~HitEffectComponent() override = default;

		void Start() override;
		void Update(float deltaTime) override;
		void LateUpdate(float deltaTime) override;
		void DrawTransparent(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& world,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		) override;
		void OnInspectorGUI(GameContext& gameContext) override;

		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		std::string GetComponentName() const override { return "HitEffectComponent"; }

		// - Combat Impact Emitters
		void EmitBlood(
			const DirectX::SimpleMath::Vector3& worldPos,
			const DirectX::SimpleMath::Vector3& hitDirection,
			float intensityMultiplier = 1.0f
		);
		void EmitBlockSparks(
			const DirectX::SimpleMath::Vector3& worldPos,
			const DirectX::SimpleMath::Vector3& blockNormal,
			bool isPerfectParry = false,
			float intensityMultiplier = 1.0f
		);
		void Clear();

		// - Accessors & Mutators
		bool IsBloodEnabled() const { return m_enableBlood; }
		void SetBloodEnabled(bool enabled) { m_enableBlood = enabled; }

		bool IsSparksEnabled() const { return m_enableSparks; }
		void SetSparksEnabled(bool enabled) { m_enableSparks = enabled; }

		float GetImpactHeightOffset() const { return m_impactHeightOffset; }
		void SetImpactHeightOffset(float offset) { m_impactHeightOffset = offset; }
	};
}
