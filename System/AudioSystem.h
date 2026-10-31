#pragma once
#include "Audio.h"
#include "SimpleMath.h"
#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace HEIN
{
	/**
	 * @enum AudioBus
	 * @brief Hierarchical mixing buses for routing music, combat SFX, character audio, and UI cues.
	 */
	enum class AudioBus : int
	{
		Master = 0,
		BGM = 1,
		Ambient = 2,
		CombatSFX = 3,
		CharacterSFX = 4,
		UISFX = 5,
		Count = 6
	};

	/**
	 * @struct SidechainDuckConfig
	 * @brief Envelope parameters for dynamic sidechain ducking (e.g. lowering BGM volume during sword clashes and blending back).
	 */
	struct SidechainDuckConfig
	{
		bool enabled = false;
		AudioBus targetBus = AudioBus::BGM;
		float targetVolume = 0.25f;
		float attackSpeed = 28.0f;
		float holdDuration = 0.30f;
		float releaseSpeed = 3.2f;
	};

	/**
	 * @struct AudioBusState
	 * @brief Real-time state for a single mixing bus including base gain, mute state, and sidechain ducking envelope.
	 */
	struct AudioBusState
	{
		float baseVolume = 1.0f;
		bool muted = false;

		// - Sidechain envelope state
		float currentDuckGain = 1.0f;
		float targetDuckGain = 1.0f;
		float duckAttackSpeed = 28.0f;
		float duckHoldTimer = 0.0f;
		float duckReleaseSpeed = 3.2f;
	};

	/**
	 * @class AudioSystem
	 * @brief Engine-wide audio mixer, wave asset cache, equal-power BGM crossfader, and sidechain ducking processor.
	 *
	 * Design Philosophy (Golden Rule Compliance):
	 * - Actor-Agnostic Service: Contains zero references to specific actor names, tags, or gameplay classes.
	 * - Safe Resource Lifecycle: Owns all DirectX::SoundEffectInstance and DirectX::SoundEffect objects so
	 *   voices are always stopped and destroyed prior to the wave cache and DirectX::AudioEngine.
	 * - Hitstop & Editor Invariant Stepping: Ticks on unscaled real time so sidechain ducking envelopes,
	 *   BGM crossfades, and editor sound previews operate smoothly even during combat hitstop or editor pause.
	 */
	class AudioSystem
	{
	private:
		struct ActiveVoice
		{
			std::unique_ptr<DirectX::SoundEffectInstance> instance;
			std::string filePath;
			AudioBus bus = AudioBus::CombatSFX;
			float baseVolume = 1.0f;
			float pitch = 0.0f;
			bool isLooping = false;
			bool is3D = false;
			DirectX::SimpleMath::Vector3 worldPos = DirectX::SimpleMath::Vector3::Zero;
			float minDistance = 10.0f;
			float maxDistance = 120.0f;
			uint64_t ownerActorID = 0;
			std::string cueName;
		};

		struct BgmTrackState
		{
			std::unique_ptr<DirectX::SoundEffectInstance> instance;
			std::string normalizedPath;
			float baseVolume = 1.0f;
			float pitch = 0.0f;
			bool isLooping = true;
		};
		inline static std::unique_ptr<AudioSystem> s_instance;
		inline static bool s_isTerminated = false;

		std::unique_ptr<DirectX::AudioEngine> m_audioEngine;
		bool m_isInitialized = false;
		bool m_isAudioDeviceAvailable = false;

		std::array<AudioBusState, static_cast<size_t>(AudioBus::Count)> m_buses{};
		std::unordered_map<std::string, std::unique_ptr<DirectX::SoundEffect>> m_soundCache;
		std::unordered_set<std::string> m_failedPaths;
		std::unordered_map<std::string, float> m_lastPlayTimestamp;

		std::vector<ActiveVoice> m_activeVoices;
		static constexpr size_t kMaxActiveVoices = 48;

		BgmTrackState m_currentBgm;
		BgmTrackState m_fadingOutBgm;
		float m_crossfadeTimer = 0.0f;
		float m_crossfadeDuration = 0.0f;
		bool m_isCrossfading = false;

		DirectX::SimpleMath::Vector3 m_listenerPosition = DirectX::SimpleMath::Vector3::Zero;
		float m_totalUnscaledTime = 0.0f;

		inline static std::vector<std::string> s_discoveredAudioFiles;

		AudioSystem() = default;

		std::string ResolveAudioFilePath(const std::string& relativeOrRawPath) const;
		float Compute3DAttenuation(const DirectX::SimpleMath::Vector3& sourcePos, float minDist, float maxDist) const;
		void EnsureDefaultAudioAssets();

	public:
		~AudioSystem();

		AudioSystem(const AudioSystem&) = delete;
		AudioSystem& operator=(const AudioSystem&) = delete;

		static AudioSystem* GetInstance();
		static void DestroyInstance();

		void Initialize();
		void Shutdown();
		void Update(float unscaledDeltaTime, const DirectX::SimpleMath::Vector3& listenerPos);

		void Suspend();
		void Resume();

		// - Resource management & file discovery
		DirectX::SoundEffect* GetOrLoadSound(const std::string& path);
		void ReloadAllAudioAssets();
		static const std::vector<std::string>& GetDiscoveredAudioFiles();
		static void RefreshDiscoveredAudioFiles();
		static const char* AudioBusToString(AudioBus bus);
		static AudioBus StringToAudioBus(const std::string& str);

		// - One-shot and looping cue playback
		bool PlayCue(
			const std::string& filePath,
			AudioBus bus,
			float volume,
			float pitch,
			bool isLooping,
			bool is3D,
			const DirectX::SimpleMath::Vector3& worldPos,
			float minDist,
			float maxDist,
			const SidechainDuckConfig& duckConfig,
			uint64_t ownerActorID = 0,
			const std::string& cueName = ""
		);

		void StopLoopingCue(uint64_t ownerActorID, const std::string& cueName = "");
		void StopAllLoopingSFX();

		// - Background Music (Equal-Power Crossfade & Loop Management)
		bool PlayBGM(
			const std::string& filePath,
			float volume = 0.85f,
			float pitch = 0.0f,
			float crossfadeDuration = 1.5f,
			bool loop = true,
			bool forceRestart = false
		);
		void StopBGM(float fadeOutDuration = 1.0f);
		bool IsBGMPlaying() const;
		const std::string& GetActiveBGMPath() const { return m_currentBgm.normalizedPath; }

		// - Dynamic Sidechain Ducking
		void TriggerSidechainDuck(const SidechainDuckConfig& config);
		void ResetAllDucking();

		// - Bus Mixer Controls
		void SetBusVolume(AudioBus bus, float volume);
		float GetBusVolume(AudioBus bus) const;
		void SetBusMuted(AudioBus bus, bool muted);
		bool IsBusMuted(AudioBus bus) const;
		float GetBusDuckGain(AudioBus bus) const;
		float GetEffectiveBusGain(AudioBus bus) const;

		bool IsAudioDeviceAvailable() const { return m_isAudioDeviceAvailable; }
		const DirectX::SimpleMath::Vector3& GetListenerPosition() const { return m_listenerPosition; }
		void UpdateActor3DPosition(uint64_t ownerActorID, const DirectX::SimpleMath::Vector3& worldPos);
	};
}
