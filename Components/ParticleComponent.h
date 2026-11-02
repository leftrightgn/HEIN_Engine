#pragma once
#include "../Message/IObserver.h"
#include "../Message/Message.h"
#include "Components/IComponent.h"
#include "SimpleMath.h"
#include <array>
#include <cstdint>
#include <d3d11.h>
#include <string>
#include <vector>
#include <wrl/client.h>
#include "System/DamageSystem.h"
#if HEIN_WITH_EDITOR
#include <ImGui/imgui.h>
#endif

namespace HEIN
{
	class Actor;
	class ActorManager;

	/**
	 * @enum ParticleBlendMode
	 * @brief Blending state applied per emitter slot or trail module.
	 */
	enum class ParticleBlendMode : int
	{
		AlphaBlend = 0, ///< Soft alpha blending for blood droplets, mist, smoke, black flame, stone debris
		Additive   = 1  ///< HDR additive blending for steel sparks, fire embers, soul wisps, glowing slash arcs
	};

	/**
	 * @enum ParticleProceduralStyle
	 * @brief Explicit GPU evaluation style for universal procedural particles.
	 */
	enum class ParticleProceduralStyle : int
	{
		ErodedDroplet = 0, ///< Organic Noise-Eroded Droplet & Mist (Blood, Mud, Debris, Poison)
		StreakSpark   = 1, ///< Anisotropic Incandescent Streak / Spark (Sword Clashes, Sparks, Lightning)
		SoftWisp      = 2, ///< Ethereal Soft Wisp & Plasma (Fire, Souls, Smoke, Floating Embers)
		ShockRing     = 3  ///< Expanding Shock & Pulse Ring (Parry Rings, Blast Waves, Guard Break)
	};

	/**
	 * @enum ParticleSpawnShape
	 * @brief Procedural emission volume shape for directional or omnidirectional bursts.
	 */
	enum class ParticleSpawnShape : int
	{
		Cone       = 0, ///< Directional cone around the impact / emission direction
		Sphere     = 1, ///< Omnidirectional 3D sphere burst
		Hemisphere = 2, ///< Upward-biased hemisphere burst
		Point      = 3  ///< Exact point spawn (e.g., core flash or shock ring)
	};

	/**
	 * @enum ParticleStudioSoloMode
	 * @brief Controls what the isolated Editor "Particle View" viewport renders for the selected actor.
	 */
	enum class ParticleStudioSoloMode : int
	{
		Auto        = 0, ///< Automatically shows active Emitters and/or Ribbon Trail (whichever are enabled)
		SelectedSlot = 1, ///< Solos only the currently selected Emitter Slot in the Inspector
		CueFilter   = 2, ///< Solos all Emitter Slots matching the selected Trigger Cue
		RibbonTrail = 3  ///< Solos the simulated 3D Ribbon Trail slash arc at the studio origin
	};

	/**
	 * @struct GPUParticleSeed
	 * @brief 96-byte (6 x float4) stateless particle seed uploaded once on spawn to GPU StructuredBuffer (t1).
	 *
	 * The Vertex Shader (Particle_VS.hlsl) evaluates closed-form drag + gravity kinematics and expands
	 * 6-vertex camera-aligned or velocity-stretched billboard quads using SV_VertexID with zero CPU loop.
	 */
	struct alignas(16) GPUParticleSeed
	{
		DirectX::SimpleMath::Vector3 spawnPos    = DirectX::SimpleMath::Vector3::Zero;
		float                        spawnTime   = -1000.0f;

		DirectX::SimpleMath::Vector3 initialVel  = DirectX::SimpleMath::Vector3::Zero;
		float                        lifetime    = 0.0f;

		DirectX::SimpleMath::Vector4 startColor  = DirectX::SimpleMath::Vector4::One;
		DirectX::SimpleMath::Vector4 endColor    = DirectX::SimpleMath::Vector4::Zero;

		float                        startSize   = 0.5f;
		float                        endSize     = 0.1f;
		float                        drag        = 2.5f;
		float                        gravityScale = 1.0f;

		float                        stretchFactor = 0.0f;
		float                        randomSeed    = 0.0f;
		uint32_t                     slotIndex     = 0;
		float                        seedPadding   = 0.0f;
	};

	/**
	 * @struct CB_ParticleFrame
	 * @brief Per-draw constant buffer matching cbParticleFrame : register(b0) in Particle_Common.hlsli.
	 */
	struct alignas(16) CB_ParticleFrame
	{
		DirectX::SimpleMath::Matrix  ViewProj;
		DirectX::SimpleMath::Vector3 CameraRight;
		float                        CurrentTime       = 0.0f;
		DirectX::SimpleMath::Vector3 CameraUp;
		float                        FramePadding      = 0.0f;
		DirectX::SimpleMath::Vector3 CameraForward;
		float                        GravityAccel      = 9.8f;
		DirectX::SimpleMath::Vector4 ShaderParams      = DirectX::SimpleMath::Vector4(1.0f, 1.85f, 0.78f, 0.0f);
	};

	/**
	 * @struct ParticleEmitterSlot
	 * @brief Unity-style modular GPU billboard emitter configuration bound to a generic trigger cue or continuous loop.
	 */
	struct ParticleEmitterSlot
	{
		// - Identity & Cue Routing Module
		std::string slotName   = "NewEmitter";
		std::string triggerCue = "OnDamage";          ///< Logical event cue ("OnDamage", "OnBlock", "OnParry", "OnWeaponActive", or custom)
		bool        enabled    = true;

		// - Pluggable Shader & Procedural Style Module
		ParticleProceduralStyle proceduralStyle   = ParticleProceduralStyle::ErodedDroplet; ///< Explicit procedural math evaluation style
		std::string             shaderName        = "Particle_PS";                          ///< Optional custom pixel shader override (.cso)
		std::string             customTexturePath = "";                                     ///< Optional texture override (empty = procedural noise)
		ParticleBlendMode       blendMode         = ParticleBlendMode::AlphaBlend;

		// - Emission Module (One-Shot Burst & Continuous Rate Over Time)
		int   burstCount   = 28;
		bool  looping      = false;
		float rateOverTime = 15.0f;

		// - Shape Module
		ParticleSpawnShape           shape       = ParticleSpawnShape::Cone;
		float                        spreadCone  = 0.75f;
		float                        spawnRadius = 0.35f;
		DirectX::SimpleMath::Vector3 localOffset = DirectX::SimpleMath::Vector3::Zero;

		// - Kinematic Module (Evaluated 100% on GPU in Particle_VS.hlsl)
		float speedMin      = 14.0f;
		float speedMax      = 34.0f;
		float lifetimeMin   = 0.22f;
		float lifetimeMax   = 0.50f;
		float drag          = 3.4f;
		float gravity       = 42.0f;
		float stretchFactor = 0.035f;

		// - Over-Lifetime Module
		float                        sizeStart    = 0.58f;
		float                        sizeEnd      = 0.18f;
		DirectX::SimpleMath::Vector4 startColor   = DirectX::SimpleMath::Vector4(0.68f, 0.02f, 0.02f, 0.96f);
		DirectX::SimpleMath::Vector4 endColor     = DirectX::SimpleMath::Vector4(0.28f, 0.01f, 0.01f, 0.00f);
		DirectX::SimpleMath::Vector4 shaderParams = DirectX::SimpleMath::Vector4(1.15f, 1.85f, 0.78f, 0.0f);

		// - Runtime accumulator for continuous rate-over-time emission
		float emissionAccumulator = 0.0f;
	};

	/**
	 * @struct ParticleTrailModule
	 * @brief Modular Ribbon / Weapon Trail configuration embedded inside ParticleComponent.
	 *
	 * Replaces the standalone WeaponTrailComponent so any actor with a ParticleComponent can enable
	 * a Catmull-Rom spline ribbon trail with pluggable trail pixel shaders and presets.
	 */
	struct ParticleTrailModule
	{
		bool              enabled              = false;
		bool              hasCustomOffsets     = false;
		bool              showDebugBladeLine   = true;
		bool              enablePhase2Boost    = false;
		std::string       shaderName           = "WeaponTrail_PS";
		ParticleBlendMode blendMode            = ParticleBlendMode::Additive;

		// - Local-space ribbon span offsets (automatically calibrated from weapon collider if not customized)
		DirectX::SimpleMath::Vector3 baseOffset = DirectX::SimpleMath::Vector3(0.0f, 0.0f, -0.8f);
		DirectX::SimpleMath::Vector3 tipOffset  = DirectX::SimpleMath::Vector3(0.0f, 0.0f, -5.8f);

		// - Spline & Timing parameters
		float lifetime          = 0.22f;
		int   subdivisions      = 4;
		float minVertexDistance = 0.15f;

		// - Pluggable Trail Shader parameters
		DirectX::SimpleMath::Vector4 coreColor            = DirectX::SimpleMath::Vector4(1.0f, 0.96f, 0.82f, 0.95f);
		DirectX::SimpleMath::Vector4 edgeColor            = DirectX::SimpleMath::Vector4(0.85f, 0.68f, 0.32f, 0.85f);
		float                        emissiveIntensity    = 2.2f;
		float                        tipFalloffPower      = 1.65f;
		float                        noiseScale           = 2.0f;
		float                        noiseScrollSpeed     = 2.8f;
		float                        erosionStrength      = 0.75f;
		float                        leadingEdgeSharpness = 7.0f;
	};

	/**
	 * @class ParticleComponent
	 * @brief Unified, modular GPU Particle & Ribbon Trail Component for any Actor in the engine.
	 *
	 * Design Highlights (Golden Rule Compliance):
	 * - Replaces both legacy HitEffectComponent and WeaponTrailComponent with a single modular component.
	 * - Module A (GPU Billboard Emitters): Stateless GPU evaluation via StructuredBuffer<GPUParticleSeed> + SV_VertexID.
	 * - Module B (Ribbon / Trail Module): Zero-allocation Catmull-Rom spline ribbon trail with blade length preservation.
	 * - Pluggable Pixel Shaders (.cso): Any particle or trail pixel shader plugs into any Emitter Slot or Trail Module.
	 * - Isolated Editor "Particle View" Viewport (#if HEIN_WITH_EDITOR): Renders soloed emitters and simulated 3D
	 *   slash trails in a dedicated dockable Editor Viewport window without polluting Scene View or Game View.
	 */
	class ParticleComponent : public IComponent, public IObserver
	{
	public:
		static constexpr size_t kMaxParticles       = 2048;
		static constexpr size_t kMaxTrailKnots      = 48;
		static constexpr int    kMaxTrailSubdivs    = 8;
		static constexpr size_t kMaxTrailSlices     = (kMaxTrailKnots - 1) * kMaxTrailSubdivs + 1;
		static constexpr size_t kMaxTrailVertices   = kMaxTrailSlices * 2;

		struct TrailKnot
		{
			DirectX::SimpleMath::Vector3 baseWorld = DirectX::SimpleMath::Vector3::Zero;
			DirectX::SimpleMath::Vector3 tipWorld  = DirectX::SimpleMath::Vector3::Zero;
			float                        age       = 0.0f;
		};

		struct TrailVertex
		{
			DirectX::SimpleMath::Vector3 pos;
			DirectX::SimpleMath::Vector2 uv;
			DirectX::SimpleMath::Vector4 color;
		};

		struct alignas(16) CB_TrailParams
		{
			DirectX::SimpleMath::Matrix  ViewProj;
			DirectX::SimpleMath::Vector4 CoreColor;
			DirectX::SimpleMath::Vector4 EdgeColor;
			float                        EmissiveIntensity;
			float                        TipFalloffPower;
			float                        NoiseScale;
			float                        NoiseScrollSpeed;
			float                        Time;
			float                        ErosionStrength;
			float                        LeadingEdgeSharpness;
			float                        TrailPadding;
		};

	private:
		struct GPUParticlePool
		{
			std::array<GPUParticleSeed, kMaxParticles> cpuSeeds{};
			size_t                                     nextCursor   = 0;
			float                                      simTime      = 0.0f;
			bool                                       gpuDirty     = true;
			size_t                                     activeCount  = 0;

			Microsoft::WRL::ComPtr<ID3D11Buffer>             structuredBuffer;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> structuredSRV;
		};

		struct TrailRingBuffer
		{
			std::array<TrailKnot, kMaxTrailKnots> knots{};
			size_t                                head        = 0;
			size_t                                count       = 0;
			float                                 elapsedTime = 0.0f;

			void PushKnot(const DirectX::SimpleMath::Vector3& baseWorld, const DirectX::SimpleMath::Vector3& tipWorld)
			{
				head = (head + kMaxTrailKnots - 1) % kMaxTrailKnots;
				knots[head].baseWorld = baseWorld;
				knots[head].tipWorld  = tipWorld;
				knots[head].age       = 0.0f;
				if (count < kMaxTrailKnots)
				{
					++count;
				}
			}

			TrailKnot& GetKnot(size_t chronologicalIndex)
			{
				return knots[(head + chronologicalIndex) % kMaxTrailKnots];
			}

			const TrailKnot& GetKnot(size_t chronologicalIndex) const
			{
				return knots[(head + chronologicalIndex) % kMaxTrailKnots];
			}
			void Clear()
			{
				head  = 0;
				count = 0;
			}
		};

ActorManager* m_actorManager       = nullptr;
uint32_t      m_registeredListenID = 0;
std::string   m_assetPath;

// - Module A: GPU Billboard Emitter Slots
std::vector<ParticleEmitterSlot>     m_emitters;
float                                m_impactHeightOffset = 8.5f;
GPUParticlePool                      m_gameplayPool;
Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbParticleFrame;

// - Module B: Ribbon / Trail Module
ParticleTrailModule                  m_trail;
TrailRingBuffer                      m_gameplayTrail;
bool                                 m_isTrailEmitting = false;
bool                                 m_alwaysEmitPreview = false;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_trailDynamicVB;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbTrail;

#if HEIN_WITH_EDITOR
		// - Isolated Editor "Particle View" Studio State (Strictly decoupled from Scene View & Game View)
		GPUParticlePool        m_previewPool;
		TrailRingBuffer        m_previewTrail;
		ParticleStudioSoloMode m_studioSoloMode      = ParticleStudioSoloMode::Auto;
		int                    m_selectedSlotIndex   = 0;
		std::string            m_previewCueFilter    = "OnDamage";
		bool                   m_previewAutoLoop     = true;
		float                  m_previewLoopInterval = 0.65f;
		float                  m_previewLoopTimer    = 0.0f;
		float                  m_previewTimeScale    = 1.0f;
		float                  m_previewIntensity    = 1.0f;
		float                  m_previewSlashPhase   = 0.0f;
		float                  m_previewCamYaw       = 0.45f;
		float                  m_previewCamPitch     = 0.30f;
		float                  m_previewCamDist      = 24.0f;
		DirectX::SimpleMath::Vector3 m_previewCamTarget = DirectX::SimpleMath::Vector3(0.0f, 2.0f, 0.0f);
#endif

		static void EnsureSharedResources(ID3D11Device* device);
		static ID3D11PixelShader* ResolvePixelShader(ID3D11Device* device, const std::string& shaderName);
		static ID3D11ShaderResourceView* ResolveTextureSRV(ID3D11Device* device, const std::string& customTexturePath);
		static float RandomRange(float minVal, float maxVal);

		void EnsurePoolGPUResources(ID3D11Device* device, GPUParticlePool& pool);
		void EnsureTrailGPUResources(ID3D11Device* device);
		void RegisterToWielderMessages();

		void SpawnSlotIntoPool(
			GPUParticlePool& pool,
			size_t slotIndex,
			const DirectX::SimpleMath::Vector3& worldPos,
			const DirectX::SimpleMath::Vector3& dir,
			float intensityMultiplier
		);

		void DrawParticlePool(
			ID3D11Device* device,
			ID3D11DeviceContext* context,
			DirectX::CommonStates& states,
			GPUParticlePool& pool,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj,
			int soloSlotIndex = -1
		);

		void DrawTrailBuffer(
			ID3D11Device* device,
			ID3D11DeviceContext* context,
			DirectX::CommonStates& states,
			const TrailRingBuffer& trailBuf,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj,
			bool allowPhase2Check
		);

	public:
		ParticleComponent(Actor* owner, ActorManager* manager = nullptr);
		~ParticleComponent() override;

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
std::string GetComponentName() const override { return "ParticleComponent"; }

// - External JSON Asset Loading & Saving
const std::string& GetAssetPath() const { return m_assetPath; }
void SetAssetPath(const std::string& path) { m_assetPath = path; }
bool LoadAsset(const std::string& assetPath);
bool SaveAsset(const std::string& assetPath);
void SerializeData(nlohmann::json& out) const;
void DeserializeData(const nlohmann::json& in);

// - Generic Combat Impact Event Handler (Zero Hardcoded Logic)
void OnCombatImpact(const CombatImpactDesc& impact);

// - Generic Cue-Driven Particle Trigger API
void EmitCue(
	const std::string& cueName,
	const DirectX::SimpleMath::Vector3& worldPos,
	const DirectX::SimpleMath::Vector3& impactDir = DirectX::SimpleMath::Vector3::UnitY,
	float intensityMultiplier = 1.0f
);

void EmitSlot(
	size_t slotIndex,
	const DirectX::SimpleMath::Vector3& worldPos,
	const DirectX::SimpleMath::Vector3& impactDir = DirectX::SimpleMath::Vector3::UnitY,
	float intensityMultiplier = 1.0f
);

void Clear();

// - Emitter Slot Presets (Data-Driven Asset Loaders)
void ApplyDefaultCombatPreset();
void ApplySoulGhostPreset();
void ApplyStoneDebrisPreset();
void ApplyFireEmberPreset();

// - Ribbon / Trail Module API & Presets
ParticleTrailModule& GetTrailModule() { return m_trail; }
const ParticleTrailModule& GetTrailModule() const { return m_trail; }
void SetTrailEnabled(bool enabled) { m_trail.enabled = enabled; }
bool IsTrailEnabled() const { return m_trail.enabled; }
void SetTrailEmitting(bool emitting) { m_isTrailEmitting = emitting; }
bool IsTrailEmitting() const { return m_isTrailEmitting; }
void SetAlwaysEmitPreview(bool enable) { m_alwaysEmitPreview = enable; }
bool IsAlwaysEmitPreview() const { return m_alwaysEmitPreview; }

void AutoFitOffsetsFromWeapon();
void ApplyTarnishedGoldTrailPreset();
void ApplyColdSteelTrailPreset();
void ApplyBossBloodflameTrailPreset();
void ApplyBlackFlameTrailPreset();

void SetOffsets(const DirectX::SimpleMath::Vector3& baseLocal, const DirectX::SimpleMath::Vector3& tipLocal)
{
	m_trail.baseOffset       = baseLocal;
	m_trail.tipOffset        = tipLocal;
	m_trail.hasCustomOffsets = true;
}
		const DirectX::SimpleMath::Vector3& GetBaseOffset() const { return m_trail.baseOffset; }
		const DirectX::SimpleMath::Vector3& GetTipOffset() const { return m_trail.tipOffset; }

		// - Accessors
		std::vector<ParticleEmitterSlot>& GetEmitters() { return m_emitters; }
		const std::vector<ParticleEmitterSlot>& GetEmitters() const { return m_emitters; }
		void SetEmitters(const std::vector<ParticleEmitterSlot>& emitters) { m_emitters = emitters; }
		size_t GetActiveParticleCount() const { return m_gameplayPool.activeCount; }

		float GetImpactHeightOffset() const { return m_impactHeightOffset; }
		void SetImpactHeightOffset(float offset) { m_impactHeightOffset = offset; }

#if HEIN_WITH_EDITOR
		// - Editor-Only Separate "Particle View" Viewport Integration
		void TriggerEditorPreviewBurst();
		void UpdateEditorPreview(float unscaledDeltaTime);
		void RenderEditorPreviewPass(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& previewView,
			const DirectX::SimpleMath::Matrix& previewProj
		);
		void DrawParticleViewportOverlayUI(GameContext& gameContext, const ImVec2& contentPos, const ImVec2& contentSize);

		float& GetPreviewCamYaw() { return m_previewCamYaw; }
		float& GetPreviewCamPitch() { return m_previewCamPitch; }
		float& GetPreviewCamDist() { return m_previewCamDist; }
		DirectX::SimpleMath::Vector3& GetPreviewCamTarget() { return m_previewCamTarget; }
		void ResetPreviewCamera();
#endif
	};
}
