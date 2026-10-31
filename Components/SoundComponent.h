#pragma once
#include "Components/IComponent.h"
#include "System/AudioSystem.h"
#include <string>
#include <vector>

namespace HEIN
{
	/**
	 * @enum CombatImpactSoundType
	 * @brief Reactive combat collision outcomes dispatched to any actor's SoundComponent.
	 */
	enum class CombatImpactSoundType : int
	{
		ParryClash = 0,
		BlockGuard = 1,
		GuardBreak = 2,
		FleshHit = 3,
		HeavyHit = 4
	};

	/**
	 * @struct SoundCueEntry
	 * @brief Serializable configuration for a single named sound cue on an Actor's SoundComponent.
	 */
	struct SoundCueEntry
	{
		std::string cueName = "NewCue";
		std::string filePath = "";
		AudioBus bus = AudioBus::CombatSFX;
		float volume = 1.0f;
		float pitch = 0.0f;
		float pitchVariance = 0.04f;
		bool isLooping = false;
		bool playOnStart = false;
		float crossfadeDuration = 1.2f;

		// - 3D Spatialization settings
		bool is3D = false;
		float minDistance = 15.0f;
		float maxDistance = 160.0f;

		// - Dynamic Sidechain Ducking settings (e.g. Sword Clash lowers BGM and blends back)
		SidechainDuckConfig ducking;
	};

	/**
	 * @class SoundComponent
	 * @brief Actor-agnostic component for authoring, serializing, and triggering BGM, Combat SFX, 3D audio, and UI sounds.
	 *
	 * Design Philosophy (Golden Rule Compliance):
	 * - Attaches to ANY Actor (GameManager, Player, Boss, Vessel, UIButton, Environmental Prop).
	 * - Sibling components (CombatStateMachineComponent, UIButtonComponent, GameFlowStateMachineComponent)
	 *   query their owning Actor's SoundComponent by logical cue name without hardcoding file paths or actor types.
	 */
	class SoundComponent : public IComponent
	{
	private:
		std::vector<SoundCueEntry> m_cues;
		bool m_hasTriggeredPlayOnStart = false;

		// - Configurable cue mappings for reactive combat collisions
		std::string m_parryClashCue = "Clash_Parry";
		std::string m_blockGuardCue = "Guard_Block";
		std::string m_guardBreakCue = "Guard_Break";
		std::string m_fleshHitCue = "Hit_Flesh";
		std::string m_heavyHitCue = "Slash_Heavy";

		static float RandomPitchOffset(float variance);
		DirectX::SimpleMath::Vector3 GetOwnerWorldPosition() const;

	public:
		SoundComponent(Actor* owner);
		~SoundComponent() override;

		void Start() override;
		void Update(float deltaTime) override;
		void OnInspectorGUI(GameContext& gameContext) override;

		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		std::string GetComponentName() const override { return "SoundComponent"; }

		// - Primary Cue Playback API
		bool PlayCue(const std::string& cueName, float volumeMultiplier = 1.0f);
		bool PlayCueOrFallbackPath(const std::string& cueNameOrFilePath, AudioBus defaultBus = AudioBus::CombatSFX, float volumeMultiplier = 1.0f);
		void StopCue(const std::string& cueName, float fadeOutDuration = 0.5f);
		void PlayCombatImpact(CombatImpactSoundType impactType, float intensityMultiplier = 1.0f);

		// - Cue Table Management & Presets
		void AddOrUpdateCue(const SoundCueEntry& entry);
		bool HasCue(const std::string& cueName) const;
		const SoundCueEntry* FindCue(const std::string& cueName) const;
		const std::vector<SoundCueEntry>& GetCues() const { return m_cues; }
		void SetCues(const std::vector<SoundCueEntry>& cues) { m_cues = cues; }

		void ApplyBGMPreset();
		void ApplyCombatCharacterPreset();
		void ApplyUIButtonPreset();

		void CopyConfigurationFrom(const SoundComponent& other);

		// - Reactive Combat Mapping Accessors
		const std::string& GetParryClashCue() const { return m_parryClashCue; }
		void SetParryClashCue(const std::string& cue) { m_parryClashCue = cue; }

		const std::string& GetBlockGuardCue() const { return m_blockGuardCue; }
		void SetBlockGuardCue(const std::string& cue) { m_blockGuardCue = cue; }

		const std::string& GetGuardBreakCue() const { return m_guardBreakCue; }
		void SetGuardBreakCue(const std::string& cue) { m_guardBreakCue = cue; }

		const std::string& GetFleshHitCue() const { return m_fleshHitCue; }
		void SetFleshHitCue(const std::string& cue) { m_fleshHitCue = cue; }

		const std::string& GetHeavyHitCue() const { return m_heavyHitCue; }
		void SetHeavyHitCue(const std::string& cue) { m_heavyHitCue = cue; }
	};
}
