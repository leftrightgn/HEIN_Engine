#pragma once
#include <Components/IComponent.h>
#include "../Message/IObserver.h"
#include "../Message/Message.h"
#include <array>
#include <cstdint>
#include <wrl/client.h>
#include <d3d11.h>
#include <SimpleMath.h>

namespace HEIN
{
	class ActorManager;

	enum class TrailBlendMode : int
	{
		Additive = 0,
		AlphaBlend = 1
	};

	/**
	 * @class WeaponTrailComponent
	 * @brief High-performance Elden Ring-style weapon ribbon trail VFX component.
	 *
	 * ARCHITECTURE HIGHLIGHTS:
	 * - Zero-Allocation Ring Buffer: Fixed-capacity contiguous std::array<TrailKnot, MAX_KNOTS>
	 *    eliminates heap allocations and vector memmoves during combat.
	 * - Centripetal Catmull-Rom Spline Subdivision + Blade Length Preservation:
	 *    Subdivides coarse 60 FPS frame samples into smooth high-curvature arcs without
	 *    mid-chord ribbon pinching during fast >90-degree slashes.
	 * - LateUpdate Socket Sync & Hitstop Freeze:
	 *    Samples world-space blade positions after SocketAttachmentComponent updates, and
	 *    freezes knot aging during Hitstop frames so the slash hangs in the air on impact.
	 * - Post-Opaque Transparent Pass:
	 *    Renders in IComponent::DrawTransparent() using D3D11_MAP_WRITE_DISCARD dynamic vertex streaming.
	 */
	class WeaponTrailComponent : public IComponent, public IObserver
	{
	public:
		struct TrailKnot
		{
			DirectX::SimpleMath::Vector3 baseWorld = DirectX::SimpleMath::Vector3::Zero;
			DirectX::SimpleMath::Vector3 tipWorld  = DirectX::SimpleMath::Vector3::Zero;
			float age = 0.0f;
		};

		struct TrailVertex
		{
			DirectX::SimpleMath::Vector3 pos;
			DirectX::SimpleMath::Vector2 uv;
			DirectX::SimpleMath::Vector4 color;
		};

		__declspec(align(16)) struct CB_WeaponTrail
		{
			DirectX::SimpleMath::Matrix  ViewProj;
			DirectX::SimpleMath::Vector4 CoreColor;
			DirectX::SimpleMath::Vector4 EdgeColor;
			float EmissiveIntensity;
			float TipFalloffPower;
			float NoiseScale;
			float NoiseScrollSpeed;
			float Time;
			float ErosionStrength;
			float LeadingEdgeSharpness;
			float TrailPadding;
		};

		static constexpr size_t MAX_KNOTS = 48;
		static constexpr int    MAX_SUBDIVISIONS = 8;
		static constexpr size_t MAX_RENDER_SLICES = (MAX_KNOTS - 1) * MAX_SUBDIVISIONS + 1;
		static constexpr size_t MAX_VERTICES = MAX_RENDER_SLICES * 2;

	private:
		ActorManager* m_actorManager = nullptr;
		uint32_t m_registeredListenID = 0;

		// Zero-allocation circular buffer (index 0 via GetKnot(0) is newest, m_count - 1 is oldest)
		std::array<TrailKnot, MAX_KNOTS> m_knots{};
		size_t m_head = 0;
		size_t m_count = 0;

		// Emission state driven by CombatStateMachineComponent messages
		bool m_isEmitting = false;
		bool m_alwaysEmitPreview = false;
		bool m_showDebugBladeLine = true;
		bool m_hasCustomOffsets = false;
		bool m_enablePhase2Boost = false;

		// Local-space blade offsets (automatically calibrated from weapon collider if not customized)
		DirectX::SimpleMath::Vector3 m_baseOffset = DirectX::SimpleMath::Vector3(0.0f, 0.0f, -0.8f);
		DirectX::SimpleMath::Vector3 m_tipOffset  = DirectX::SimpleMath::Vector3(0.0f, 0.0f, -5.8f);

		// Trail timing & spline tessellation parameters
		float m_lifetime = 0.22f;
		int   m_subdivisions = 4;
		float m_minVertexDistance = 0.15f;
		float m_elapsedTime = 0.0f;

		// Visual / Shader parameters (Per-Weapon customizable)
		DirectX::SimpleMath::Vector4 m_coreColor = DirectX::SimpleMath::Vector4(1.0f, 0.96f, 0.82f, 0.95f);
		DirectX::SimpleMath::Vector4 m_edgeColor = DirectX::SimpleMath::Vector4(0.85f, 0.68f, 0.32f, 0.85f);
		float m_emissiveIntensity = 2.2f;
		float m_tipFalloffPower = 1.65f;
		float m_noiseScale = 2.0f;
		float m_noiseScrollSpeed = 2.8f;
		float m_erosionStrength = 0.75f;
		float m_leadingEdgeSharpness = 7.0f;
		TrailBlendMode m_blendMode = TrailBlendMode::Additive;

		// Per-component dynamic GPU buffers
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_dynamicVB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbTrail;

		// Ring buffer helpers
		void PushKnot(const DirectX::SimpleMath::Vector3& baseWorld, const DirectX::SimpleMath::Vector3& tipWorld);
		TrailKnot& GetKnot(size_t chronologicalIndex);
		const TrailKnot& GetKnot(size_t chronologicalIndex) const;
		void ClearKnots();

		void EnsureGPUResources(ID3D11Device* device);
		void RegisterToWielderMessages();

	public:
		WeaponTrailComponent(Actor* owner, ActorManager* manager = nullptr);
		~WeaponTrailComponent() override;

		std::string GetComponentName() const override { return "WeaponTrailComponent"; }

		void Start() override;
		void Update(float deltaTime) override;
		void LateUpdate(float deltaTime) override;
		void DrawTransparent(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& world,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		) override;

		void OnMessageAccepted(Message::MessageID messageID) override;

		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;

		// Configuration & Presets API
		void AutoFitOffsetsFromWeapon();
		void ApplyTarnishedGoldPreset();
		void ApplyColdSteelPreset();
		void ApplyBossBloodflamePreset();
		void ApplyBlackFlamePreset();

		void SetOffsets(const DirectX::SimpleMath::Vector3& baseLocal, const DirectX::SimpleMath::Vector3& tipLocal)
		{
			m_baseOffset = baseLocal;
			m_tipOffset = tipLocal;
			m_hasCustomOffsets = true;
		}
		const DirectX::SimpleMath::Vector3& GetBaseOffset() const { return m_baseOffset; }
		const DirectX::SimpleMath::Vector3& GetTipOffset() const { return m_tipOffset; }

		void SetColors(const DirectX::SimpleMath::Vector4& coreColor, const DirectX::SimpleMath::Vector4& edgeColor)
		{
			m_coreColor = coreColor;
			m_edgeColor = edgeColor;
		}
		void SetLifetime(float seconds) { m_lifetime = std::max(0.03f, seconds); }
		void SetSubdivisions(int subdivisions) { m_subdivisions = std::clamp(subdivisions, 1, MAX_SUBDIVISIONS); }
		void SetEmissiveIntensity(float intensity) { m_emissiveIntensity = std::max(0.1f, intensity); }
		void SetBlendMode(TrailBlendMode mode) { m_blendMode = mode; }
		void SetEnablePhase2Boost(bool enable) { m_enablePhase2Boost = enable; }
	};
}
