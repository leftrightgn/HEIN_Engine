#include "pch.h"
#include "System/AudioSystem.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace HEIN
{
	static constexpr float kHalfPi = 1.57079632679f;
	static constexpr float kTwoPi = 6.28318530718f;
	static constexpr float kMinRetriggerInterval = 0.035f;

	static std::string NormalizeSlashes(const std::string& path)
	{
		std::string result = path;
		for (char& ch : result)
		{
			if (ch == '\\') ch = '/';
		}
		return result;
	}

	static std::wstring Utf8ToWide(const std::string& str)
	{
		if (str.empty()) return L"";
		int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
		if (len <= 0) return std::wstring(str.begin(), str.end());
		std::wstring wstr(static_cast<size_t>(len), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
		if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
		return wstr;
	}

	static void WriteSynthesizedWavFile(const std::filesystem::path& filePath, const std::vector<int16_t>& samples, uint32_t sampleRate = 44100)
	{
		std::error_code ec;
		if (std::filesystem::exists(filePath, ec))
		{
			return;
		}
		std::filesystem::create_directories(filePath.parent_path(), ec);

		std::ofstream out(filePath, std::ios::binary);
		if (!out.is_open()) return;

		uint16_t numChannels = 1;
		uint16_t bitsPerSample = 16;
		uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
		uint16_t blockAlign = numChannels * (bitsPerSample / 8);
		uint32_t dataChunkSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
		uint32_t riffChunkSize = 36 + dataChunkSize;

		out.write("RIFF", 4);
		out.write(reinterpret_cast<const char*>(&riffChunkSize), 4);
		out.write("WAVE", 4);
		out.write("fmt ", 4);
		uint32_t fmtChunkSize = 16;
		uint16_t audioFormat = 1;
		out.write(reinterpret_cast<const char*>(&fmtChunkSize), 4);
		out.write(reinterpret_cast<const char*>(&audioFormat), 2);
		out.write(reinterpret_cast<const char*>(&numChannels), 2);
		out.write(reinterpret_cast<const char*>(&sampleRate), 4);
		out.write(reinterpret_cast<const char*>(&byteRate), 4);
		out.write(reinterpret_cast<const char*>(&blockAlign), 2);
		out.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
		out.write("data", 4);
		out.write(reinterpret_cast<const char*>(&dataChunkSize), 4);
		if (!samples.empty())
		{
			out.write(reinterpret_cast<const char*>(samples.data()), dataChunkSize);
		}
	}

	static float PseudoNoise(uint32_t& seed)
	{
		seed = seed * 1664525u + 1013904223u;
		return (static_cast<float>((seed >> 8) & 0xFFFF) / 32767.5f) - 1.0f;
	}

	static int16_t FloatToPcm16(float v)
	{
		float clamped = std::clamp(v, -1.0f, 1.0f);
		return static_cast<int16_t>(clamped * 32000.0f);
	}

	AudioSystem::~AudioSystem()
	{
		Shutdown();
	}

	AudioSystem* AudioSystem::GetInstance()
	{
		if (s_isTerminated)
		{
			return nullptr;
		}

		if (!s_instance)
		{
			s_instance = std::unique_ptr<AudioSystem>(new AudioSystem());
		}
		return s_instance.get();
	}

	void AudioSystem::DestroyInstance()
	{
		s_isTerminated = true;
		if (s_instance)
		{
			s_instance->Shutdown();
			s_instance.reset();
		}
	}

	void AudioSystem::Initialize()
	{
		if (m_isInitialized) return;
		m_isInitialized = true;
		s_isTerminated = false;

		for (size_t i = 0; i < m_buses.size(); ++i)
		{
			m_buses[i].baseVolume = 1.0f;
			m_buses[i].muted = false;
			m_buses[i].currentDuckGain = 1.0f;
			m_buses[i].targetDuckGain = 1.0f;
			m_buses[i].duckAttackSpeed = 28.0f;
			m_buses[i].duckHoldTimer = 0.0f;
			m_buses[i].duckReleaseSpeed = 3.2f;
		}
		m_buses[static_cast<size_t>(AudioBus::BGM)].baseVolume = 0.75f;
		m_buses[static_cast<size_t>(AudioBus::Ambient)].baseVolume = 0.80f;
		m_buses[static_cast<size_t>(AudioBus::CombatSFX)].baseVolume = 1.0f;
		m_buses[static_cast<size_t>(AudioBus::CharacterSFX)].baseVolume = 0.90f;
		m_buses[static_cast<size_t>(AudioBus::UISFX)].baseVolume = 0.90f;

		// - Ensure the audio directory exists without regenerating or overwriting user-deleted .wav files
		std::error_code ec;
		std::filesystem::path audioDir = "Resources/Audio";
		if (!std::filesystem::exists("Resources", ec) && std::filesystem::exists("Dual/Resources", ec))
		{
			audioDir = "Dual/Resources/Audio";
		}
		std::filesystem::create_directories(audioDir, ec);

		try
		{
			DirectX::AUDIO_ENGINE_FLAGS eflags = DirectX::AudioEngine_Default;
#ifdef _DEBUG
			eflags = eflags | DirectX::AudioEngine_Debug;
#endif
			m_audioEngine = std::make_unique<DirectX::AudioEngine>(eflags);
			m_isAudioDeviceAvailable = (m_audioEngine != nullptr && m_audioEngine->IsAudioDevicePresent());
		}
		catch (...)
		{
			m_audioEngine.reset();
			m_isAudioDeviceAvailable = false;
		}

		RefreshDiscoveredAudioFiles();
	}

	void AudioSystem::Shutdown()
	{
		if (!m_isInitialized && !m_audioEngine)
		{
			return;
		}

		// - Stop all active SoundEffectInstance playback immediately
		if (m_currentBgm.instance)
		{
			try { m_currentBgm.instance->Stop(true); } catch (...) {}
		}
		if (m_fadingOutBgm.instance)
		{
			try { m_fadingOutBgm.instance->Stop(true); } catch (...) {}
		}
		for (auto& voice : m_activeVoices)
		{
			if (voice.instance)
			{
				try { voice.instance->Stop(true); } catch (...) {}
			}
		}

		// - Flush pending stop requests through AudioEngine and suspend rendering threads
		if (m_audioEngine)
		{
			try
			{
				m_audioEngine->Update();
				m_audioEngine->Suspend();
			}
			catch (...)
			{
			}
		}

		// - Release all SoundEffectInstance pointers prior to freeing wave data
		m_currentBgm.instance.reset();
		m_currentBgm.normalizedPath.clear();

		m_fadingOutBgm.instance.reset();
		m_fadingOutBgm.normalizedPath.clear();
		m_isCrossfading = false;

		for (auto& voice : m_activeVoices)
		{
			voice.instance.reset();
		}
		m_activeVoices.clear();

		// - Release wave audio buffers in sound cache
		m_soundCache.clear();
		m_failedPaths.clear();
		m_lastPlayTimestamp.clear();

		// - Safely destroy AudioEngine instance
		if (m_audioEngine)
		{
			try
			{
				m_audioEngine.reset();
			}
			catch (...)
			{
			}
		}

		m_isAudioDeviceAvailable = false;
		m_isInitialized = false;
	}

	void AudioSystem::EnsureDefaultAudioAssets()
	{
		std::error_code ec;

		std::filesystem::path audioDir = "Resources/Audio";
		if (!std::filesystem::exists("Resources", ec) && std::filesystem::exists("Dual/Resources", ec))
		{
			audioDir = "Dual/Resources/Audio";
		}
		std::filesystem::create_directories(audioDir, ec);

		constexpr uint32_t sr = 44100;

		// - Clash_Parry.wav: High-impact steel blade clash + harmonic ring out
		if (!std::filesystem::exists(audioDir / "Clash_Parry.wav", ec))
		{
			size_t count = static_cast<size_t>(sr * 0.75f);
			std::vector<int16_t> pcm(count);
			uint32_t seed = 1337u;
			for (size_t i = 0; i < count; ++i)
			{
				float t = static_cast<float>(i) / static_cast<float>(sr);
				float transientEnv = std::exp(-t * 42.0f);
				float ringEnv = std::exp(-t * 4.8f);
				float shimmerEnv = std::exp(-t * 8.5f);

				float metalRing =
					0.36f * std::sin(kTwoPi * 1480.0f * t + 0.4f * std::sin(kTwoPi * 2960.0f * t)) +
					0.28f * std::sin(kTwoPi * 2215.0f * t) +
					0.20f * std::sin(kTwoPi * 3340.0f * t) +
					0.14f * std::sin(kTwoPi * 4720.0f * t) * shimmerEnv;

				float lowImpact = 0.45f * std::sin(kTwoPi * (190.0f * std::exp(-t * 14.0f) + 65.0f) * t) * transientEnv;
				float sparkNoise = 0.55f * PseudoNoise(seed) * transientEnv;

				pcm[i] = FloatToPcm16((metalRing * ringEnv + lowImpact + sparkNoise) * 0.88f);
			}
			WriteSynthesizedWavFile(audioDir / "Clash_Parry.wav", pcm, sr);
		}

		// - Guard_Block.wav: Heavy shield metallic thud + short steel resonance
		if (!std::filesystem::exists(audioDir / "Guard_Block.wav", ec))
		{
			size_t count = static_cast<size_t>(sr * 0.42f);
			std::vector<int16_t> pcm(count);
			uint32_t seed = 2026u;
			for (size_t i = 0; i < count; ++i)
			{
				float t = static_cast<float>(i) / static_cast<float>(sr);
				float thudEnv = std::exp(-t * 24.0f);
				float metalEnv = std::exp(-t * 11.5f);

				float thud = 0.58f * std::sin(kTwoPi * (145.0f * std::exp(-t * 10.0f) + 55.0f) * t) * thudEnv;
				float steel = (0.30f * std::sin(kTwoPi * 860.0f * t) + 0.22f * std::sin(kTwoPi * 1290.0f * t)) * metalEnv;
				float grit = 0.35f * PseudoNoise(seed) * std::exp(-t * 38.0f);

				pcm[i] = FloatToPcm16((thud + steel + grit) * 0.85f);
			}
			WriteSynthesizedWavFile(audioDir / "Guard_Block.wav", pcm, sr);
		}

		// - Guard_Break.wav: Heavy shattering stance-break impact
		if (!std::filesystem::exists(audioDir / "Guard_Break.wav", ec))
		{
			size_t count = static_cast<size_t>(sr * 0.85f);
			std::vector<int16_t> pcm(count);
			uint32_t seed = 4096u;
			for (size_t i = 0; i < count; ++i)
			{
				float t = static_cast<float>(i) / static_cast<float>(sr);
				float subEnv = std::exp(-t * 6.5f);
				float shatterEnv = std::exp(-t * 9.0f);

				float subDrop = 0.55f * std::sin(kTwoPi * (160.0f * std::exp(-t * 4.5f) + 42.0f) * t) * subEnv;
				float dissonantMetal = (0.28f * std::sin(kTwoPi * 620.0f * t) +
					0.24f * std::sin(kTwoPi * 915.0f * t) +
					0.20f * std::sin(kTwoPi * 1540.0f * t)) * shatterEnv;
				float burst = 0.42f * PseudoNoise(seed) * std::exp(-t * 18.0f);

				pcm[i] = FloatToPcm16((subDrop + dissonantMetal + burst) * 0.90f);
			}
			WriteSynthesizedWavFile(audioDir / "Guard_Break.wav", pcm, sr);
		}

		// - Slash_Swing1.wav, Slash_Swing2.wav, Slash_Heavy.wav: Sword air-cut whooshes
		auto makeWhoosh = [&](const char* filename, float durationSec, float pitchScale, float heaviness)
		{
			if (std::filesystem::exists(audioDir / filename, ec)) return;
			size_t count = static_cast<size_t>(sr * durationSec);
			std::vector<int16_t> pcm(count);
			uint32_t seed = 777u;
			float lpState = 0.0f;
			float bpState = 0.0f;
			for (size_t i = 0; i < count; ++i)
			{
				float t = static_cast<float>(i) / static_cast<float>(sr);
				float u = t / durationSec;
				float env = std::sin(u * 3.14159265f) * std::exp(-u * 1.6f);

				float rawNoise = PseudoNoise(seed);
				float cutoff = std::clamp((0.06f + 0.22f * env) * pitchScale, 0.02f, 0.65f);
				lpState += cutoff * (rawNoise - lpState);
				bpState += 0.04f * (lpState - bpState);
				float bandWhoosh = lpState - bpState;

				float bladeWhistle = 0.16f * std::sin(kTwoPi * (380.0f + 260.0f * env) * pitchScale * t) * env;
				float lowBody = heaviness * 0.30f * std::sin(kTwoPi * (95.0f + 45.0f * (1.0f - u)) * t) * env;

				pcm[i] = FloatToPcm16((bandWhoosh * 1.45f * env + bladeWhistle + lowBody) * 0.82f);
			}
			WriteSynthesizedWavFile(audioDir / filename, pcm, sr);
		};
		makeWhoosh("Slash_Swing1.wav", 0.30f, 1.05f, 0.4f);
		makeWhoosh("Slash_Swing2.wav", 0.32f, 0.92f, 0.5f);
		makeWhoosh("Slash_Heavy.wav", 0.48f, 0.72f, 1.0f);
		makeWhoosh("Dodge_Roll.wav", 0.34f, 0.60f, 0.35f);

		// - Hit_Flesh.wav: Visceral melee hit impact
		if (!std::filesystem::exists(audioDir / "Hit_Flesh.wav", ec))
		{
			size_t count = static_cast<size_t>(sr * 0.32f);
			std::vector<int16_t> pcm(count);
			uint32_t seed = 9001u;
			float filt = 0.0f;
			for (size_t i = 0; i < count; ++i)
			{
				float t = static_cast<float>(i) / static_cast<float>(sr);
				float punchEnv = std::exp(-t * 26.0f);
				float punch = 0.68f * std::sin(kTwoPi * (135.0f * std::exp(-t * 16.0f) + 48.0f) * t) * punchEnv;
				float raw = PseudoNoise(seed);
				filt += 0.25f * (raw - filt);
				float slashSnap = 0.48f * filt * std::exp(-t * 32.0f);
				pcm[i] = FloatToPcm16((punch + slashSnap) * 0.88f);
			}
			WriteSynthesizedWavFile(audioDir / "Hit_Flesh.wav", pcm, sr);
		}

		// - Footstep.wav: Armored step tap
		if (!std::filesystem::exists(audioDir / "Footstep.wav", ec))
		{
			size_t count = static_cast<size_t>(sr * 0.16f);
			std::vector<int16_t> pcm(count);
			uint32_t seed = 5150u;
			float filt = 0.0f;
			for (size_t i = 0; i < count; ++i)
			{
				float t = static_cast<float>(i) / static_cast<float>(sr);
				float env = std::exp(-t * 36.0f);
				filt += 0.18f * (PseudoNoise(seed) - filt);
				float step = (0.45f * std::sin(kTwoPi * 95.0f * t) + 0.40f * filt) * env;
				pcm[i] = FloatToPcm16(step * 0.65f);
			}
			WriteSynthesizedWavFile(audioDir / "Footstep.wav", pcm, sr);
		}

		// - UI_Hover.wav: Subtle Elden-Ring style menu focus chime
		if (!std::filesystem::exists(audioDir / "UI_Hover.wav", ec))
		{
			size_t count = static_cast<size_t>(sr * 0.14f);
			std::vector<int16_t> pcm(count);
			for (size_t i = 0; i < count; ++i)
			{
				float t = static_cast<float>(i) / static_cast<float>(sr);
				float attack = std::min(1.0f, t / 0.006f);
				float decay = std::exp(-t * 28.0f);
				float tone = (0.55f * std::sin(kTwoPi * 1046.5f * t) + 0.30f * std::sin(kTwoPi * 1567.98f * t)) * attack * decay;
				pcm[i] = FloatToPcm16(tone * 0.45f);
			}
			WriteSynthesizedWavFile(audioDir / "UI_Hover.wav", pcm, sr);
		}

		// - UI_Click.wav: Warm golden confirmation chord resonance
		if (!std::filesystem::exists(audioDir / "UI_Click.wav", ec))
		{
			size_t count = static_cast<size_t>(sr * 0.45f);
			std::vector<int16_t> pcm(count);
			for (size_t i = 0; i < count; ++i)
			{
				float t = static_cast<float>(i) / static_cast<float>(sr);
				float attack = std::min(1.0f, t / 0.008f);
				float decay = std::exp(-t * 7.5f);
				float chord =
					0.40f * std::sin(kTwoPi * 587.33f * t) +
					0.32f * std::sin(kTwoPi * 880.00f * t) +
					0.24f * std::sin(kTwoPi * 1174.66f * t);
				pcm[i] = FloatToPcm16(chord * attack * decay * 0.65f);
			}
			WriteSynthesizedWavFile(audioDir / "UI_Click.wav", pcm, sr);
		}

		// - BGM_Arena.wav: Seamless 8-second dark orchestral D-minor boss arena drone + rhythmic war pulse
		if (!std::filesystem::exists(audioDir / "BGM_Arena.wav", ec))
		{
			float duration = 8.0f;
			size_t count = static_cast<size_t>(sr * duration);
			std::vector<int16_t> pcm(count);
			for (size_t i = 0; i < count; ++i)
			{
				float t = static_cast<float>(i) / static_cast<float>(sr);
				float lfo = 0.82f + 0.18f * std::sin(kTwoPi * 0.25f * t);

				// D minor dark choir/cellos (D2 = 73.42Hz, A2 = 110Hz, D3 = 146.83Hz, F3 = 174.61Hz, A3 = 220Hz)
				float pad =
					0.32f * std::sin(kTwoPi * 73.416f * t) +
					0.24f * std::sin(kTwoPi * 110.000f * t) +
					0.20f * std::sin(kTwoPi * 146.832f * t + 0.3f * std::sin(kTwoPi * 0.5f * t)) +
					0.15f * std::sin(kTwoPi * 174.614f * t) +
					0.11f * std::sin(kTwoPi * 220.000f * t);

				// War drum heartbeat pulse every 1.0s
				float beatPhase = std::fmod(t, 1.0f);
				float drumEnv = std::exp(-beatPhase * 12.0f);
				float drum = 0.28f * std::sin(kTwoPi * 55.0f * beatPhase) * drumEnv;

				// Smooth loop cross-edge envelope so loop point is click-free
				float edgeFade = 1.0f;
				if (t < 0.05f) edgeFade = t / 0.05f;
				else if (t > duration - 0.05f) edgeFade = (duration - t) / 0.05f;

				pcm[i] = FloatToPcm16((pad * lfo + drum) * edgeFade * 0.55f);
			}
			WriteSynthesizedWavFile(audioDir / "BGM_Arena.wav", pcm, sr);
		}

		// - BGM_Intro.wav: Tense low atmospheric intro drone
		if (!std::filesystem::exists(audioDir / "BGM_Intro.wav", ec))
		{
			float duration = 6.0f;
			size_t count = static_cast<size_t>(sr * duration);
			std::vector<int16_t> pcm(count);
			for (size_t i = 0; i < count; ++i)
			{
				float t = static_cast<float>(i) / static_cast<float>(sr);
				float swell = 0.65f + 0.35f * std::sin(kTwoPi * (t / duration));
				float drone =
					0.38f * std::sin(kTwoPi * 55.0f * t) +
					0.26f * std::sin(kTwoPi * 82.41f * t) +
					0.18f * std::sin(kTwoPi * 110.0f * t) +
					0.12f * std::sin(kTwoPi * 164.81f * t);
				pcm[i] = FloatToPcm16(drone * swell * 0.50f);
			}
			WriteSynthesizedWavFile(audioDir / "BGM_Intro.wav", pcm, sr);
		}

		// - BGM_Victory.wav: Triumphant D-major resolution chord
		if (!std::filesystem::exists(audioDir / "BGM_Victory.wav", ec))
		{
			float duration = 4.0f;
			size_t count = static_cast<size_t>(sr * duration);
			std::vector<int16_t> pcm(count);
			for (size_t i = 0; i < count; ++i)
			{
				float t = static_cast<float>(i) / static_cast<float>(sr);
				float attack = std::min(1.0f, t / 0.15f);
				float decay = std::exp(-t * 0.55f);
				float chord =
					0.30f * std::sin(kTwoPi * 146.83f * t) +
					0.25f * std::sin(kTwoPi * 220.00f * t) +
					0.22f * std::sin(kTwoPi * 293.66f * t) +
					0.18f * std::sin(kTwoPi * 369.99f * t) +
					0.14f * std::sin(kTwoPi * 440.00f * t);
				pcm[i] = FloatToPcm16(chord * attack * decay * 0.65f);
			}
			WriteSynthesizedWavFile(audioDir / "BGM_Victory.wav", pcm, sr);
		}

		// - BGM_GameOver.wav: Somber low tolling bell decay
		if (!std::filesystem::exists(audioDir / "BGM_GameOver.wav", ec))
		{
			float duration = 4.0f;
			size_t count = static_cast<size_t>(sr * duration);
			std::vector<int16_t> pcm(count);
			for (size_t i = 0; i < count; ++i)
			{
				float t = static_cast<float>(i) / static_cast<float>(sr);
				float attack = std::min(1.0f, t / 0.04f);
				float decay = std::exp(-t * 0.75f);
				float toll =
					0.36f * std::sin(kTwoPi * 73.42f * t) +
					0.28f * std::sin(kTwoPi * 110.0f * t) +
					0.20f * std::sin(kTwoPi * 174.61f * t) +
					0.15f * std::sin(kTwoPi * 207.65f * t);
				pcm[i] = FloatToPcm16(toll * attack * decay * 0.65f);
			}
			WriteSynthesizedWavFile(audioDir / "BGM_GameOver.wav", pcm, sr);
		}
	}

	std::string AudioSystem::ResolveAudioFilePath(const std::string& relativeOrRawPath) const
	{
		if (relativeOrRawPath.empty()) return "";
		std::error_code ec;

		std::string clean = NormalizeSlashes(relativeOrRawPath);
		std::filesystem::path p(clean);
		std::string filename = p.filename().string();

		std::vector<std::string> candidates = {
			clean,
			"Dual/" + clean,
			"../" + clean,
			"../Dual/" + clean,
			"Resources/Audio/" + filename,
			"Dual/Resources/Audio/" + filename,
			"../Dual/Resources/Audio/" + filename,
			"Resources/Sound/" + filename,
			"Dual/Resources/Sound/" + filename,
			"../Dual/Resources/Sound/" + filename
		};

		for (const auto& candidate : candidates)
		{
			if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec))
			{
				return NormalizeSlashes(candidate);
			}
		}

		// - If Slash_Swing1.wav or Slash_Swing2.wav was deleted in favor of Slash_Swing.wav, fall back to Slash_Swing.wav
		if (filename == "Slash_Swing1.wav" || filename == "Slash_Swing2.wav")
		{
			std::vector<std::string> swingFallbacks = {
				"Resources/Audio/Slash_Swing.wav",
				"Dual/Resources/Audio/Slash_Swing.wav",
				"../Dual/Resources/Audio/Slash_Swing.wav"
			};
			for (const auto& fb : swingFallbacks)
			{
				if (std::filesystem::exists(fb, ec) && std::filesystem::is_regular_file(fb, ec))
				{
					return NormalizeSlashes(fb);
				}
			}
		}

		return "";
	}

	DirectX::SoundEffect* AudioSystem::GetOrLoadSound(const std::string& path)
	{
		if (!m_isInitialized) Initialize();
		if (!m_isAudioDeviceAvailable || !m_audioEngine || path.empty()) return nullptr;

		std::string normKey = NormalizeSlashes(path);
		if (m_failedPaths.contains(normKey))
		{
			return nullptr;
		}

		auto it = m_soundCache.find(normKey);
		if (it != m_soundCache.end() && it->second)
		{
			return it->second.get();
		}

		std::string resolved = ResolveAudioFilePath(normKey);
		if (resolved.empty())
		{
			m_failedPaths.insert(normKey);
			return nullptr;
		}

		auto itResolved = m_soundCache.find(resolved);
		if (itResolved != m_soundCache.end() && itResolved->second)
		{
			return itResolved->second.get();
		}

		try
		{
			std::wstring wResolved = Utf8ToWide(resolved);
			auto sfx = std::make_unique<DirectX::SoundEffect>(m_audioEngine.get(), wResolved.c_str());
			DirectX::SoundEffect* rawPtr = sfx.get();
			m_soundCache[normKey] = std::move(sfx);
			return rawPtr;
		}
		catch (...)
		{
			m_failedPaths.insert(normKey);
			return nullptr;
		}
	}

	void AudioSystem::ReloadAllAudioAssets()
	{
		if (m_currentBgm.instance)
		{
			m_currentBgm.instance->Stop(true);
			m_currentBgm.instance.reset();
		}
		if (m_fadingOutBgm.instance)
		{
			m_fadingOutBgm.instance->Stop(true);
			m_fadingOutBgm.instance.reset();
		}
		for (auto& voice : m_activeVoices)
		{
			if (voice.instance)
			{
				voice.instance->Stop(true);
				voice.instance.reset();
			}
		}
		m_activeVoices.clear();
		m_soundCache.clear();
		m_failedPaths.clear();
		RefreshDiscoveredAudioFiles();

		if (!m_currentBgm.normalizedPath.empty())
		{
			std::string reloadPath = m_currentBgm.normalizedPath;
			float vol = m_currentBgm.baseVolume;
			float pitch = m_currentBgm.pitch;
			bool loop = m_currentBgm.isLooping;
			m_currentBgm.normalizedPath.clear();
			PlayBGM(reloadPath, vol, pitch, 0.2f, loop, true);
		}
	}

	const std::vector<std::string>& AudioSystem::GetDiscoveredAudioFiles()
	{
		if (s_discoveredAudioFiles.empty())
		{
			RefreshDiscoveredAudioFiles();
		}
		return s_discoveredAudioFiles;
	}

	void AudioSystem::RefreshDiscoveredAudioFiles()
	{
		s_discoveredAudioFiles.clear();
		std::error_code ec;

		std::vector<std::string> searchDirs = {
			"Resources/Audio",
			"Dual/Resources/Audio",
			"../Dual/Resources/Audio",
			"Resources/Sound",
			"Dual/Resources/Sound",
			"../Dual/Resources/Sound"
		};

		for (const auto& dir : searchDirs)
		{
			if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) continue;
			try
			{
				for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec))
				{
					if (!entry.is_regular_file(ec)) continue;
					std::string ext = entry.path().extension().string();
					for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
					if (ext != ".wav") continue;

					std::string relPath = NormalizeSlashes(entry.path().string());
					auto pos = relPath.find("Resources/");
					if (pos != std::string::npos)
					{
						relPath = relPath.substr(pos);
					}

					if (std::find(s_discoveredAudioFiles.begin(), s_discoveredAudioFiles.end(), relPath) == s_discoveredAudioFiles.end())
					{
						s_discoveredAudioFiles.push_back(relPath);
					}
				}
			}
			catch (...)
			{
			}
		}
		std::sort(s_discoveredAudioFiles.begin(), s_discoveredAudioFiles.end());
	}

	const char* AudioSystem::AudioBusToString(AudioBus bus)
	{
		switch (bus)
		{
		case AudioBus::Master: return "Master";
		case AudioBus::BGM: return "BGM";
		case AudioBus::Ambient: return "Ambient";
		case AudioBus::CombatSFX: return "CombatSFX";
		case AudioBus::CharacterSFX: return "CharacterSFX";
		case AudioBus::UISFX: return "UISFX";
		default: return "CombatSFX";
		}
	}

	AudioBus AudioSystem::StringToAudioBus(const std::string& str)
	{
		if (str == "Master") return AudioBus::Master;
		if (str == "BGM") return AudioBus::BGM;
		if (str == "Ambient") return AudioBus::Ambient;
		if (str == "CharacterSFX") return AudioBus::CharacterSFX;
		if (str == "UISFX") return AudioBus::UISFX;
		return AudioBus::CombatSFX;
	}

	float AudioSystem::Compute3DAttenuation(const DirectX::SimpleMath::Vector3& sourcePos, float minDist, float maxDist) const
	{
		float safeMin = std::max(0.1f, minDist);
		float safeMax = std::max(safeMin + 0.1f, maxDist);
		float dist = DirectX::SimpleMath::Vector3::Distance(sourcePos, m_listenerPosition);
		if (dist <= safeMin) return 1.0f;
		if (dist >= safeMax) return 0.0f;
		float linear = 1.0f - ((dist - safeMin) / (safeMax - safeMin));
		return std::clamp(linear * linear, 0.0f, 1.0f);
	}

	bool AudioSystem::PlayCue(
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
		uint64_t ownerActorID,
		const std::string& cueName
	)
	{
		if (!m_isInitialized) Initialize();
		if (filePath.empty()) return false;

		// - Trigger sidechain ducking envelope even if routed to BGM or SFX
		if (duckConfig.enabled)
		{
			TriggerSidechainDuck(duckConfig);
		}

		// - If this cue is routed to the BGM bus, use the equal-power BGM crossfader
		if (bus == AudioBus::BGM)
		{
			return PlayBGM(filePath, volume, pitch, 1.2f, isLooping, false);
		}

		if (!m_isAudioDeviceAvailable || !m_audioEngine) return false;

		std::string normKey = NormalizeSlashes(filePath);

		// - Same-cue anti-stacking cooldown prevents constructive phase clipping on multi-collider same-frame hits
		if (!isLooping)
		{
			auto itTime = m_lastPlayTimestamp.find(normKey);
			if (itTime != m_lastPlayTimestamp.end() && (m_totalUnscaledTime - itTime->second) < kMinRetriggerInterval)
			{
				return false;
			}
			m_lastPlayTimestamp[normKey] = m_totalUnscaledTime;
		}
		else if (ownerActorID != 0)
		{
			// - Prevent duplicate looping instances of the same cue on the same actor
			for (const auto& active : m_activeVoices)
			{
				if (active.isLooping && active.ownerActorID == ownerActorID &&
					((!cueName.empty() && active.cueName == cueName) || active.filePath == normKey))
				{
					if (active.instance && active.instance->GetState() == DirectX::PLAYING)
					{
						return true;
					}
				}
			}
		}

		DirectX::SoundEffect* effect = GetOrLoadSound(normKey);
		if (!effect) return false;

		// - Reclaim finished non-looping voices or evict oldest non-looping voice if at capacity
		for (auto it = m_activeVoices.begin(); it != m_activeVoices.end();)
		{
			if (!it->instance || it->instance->GetState() == DirectX::STOPPED)
			{
				it = m_activeVoices.erase(it);
			}
			else
			{
				++it;
			}
		}

		if (m_activeVoices.size() >= kMaxActiveVoices)
		{
			auto oldestOneShot = std::find_if(m_activeVoices.begin(), m_activeVoices.end(), [](const ActiveVoice& v) {
				return !v.isLooping;
			});
			if (oldestOneShot != m_activeVoices.end())
			{
				if (oldestOneShot->instance) oldestOneShot->instance->Stop(true);
				m_activeVoices.erase(oldestOneShot);
			}
			else
			{
				return false;
			}
		}

		try
		{
			auto inst = effect->CreateInstance();
			if (!inst) return false;

			float busGain = GetEffectiveBusGain(bus);
			float spatialGain = is3D ? Compute3DAttenuation(worldPos, minDist, maxDist) : 1.0f;
			float finalVol = std::clamp(volume * busGain * spatialGain, 0.0f, 2.0f);
			float clampedPitch = std::clamp(pitch, -1.0f, 1.0f);

			inst->SetVolume(finalVol);
			inst->SetPitch(clampedPitch);
			inst->Play(isLooping);

			ActiveVoice voice;
			voice.instance = std::move(inst);
			voice.filePath = normKey;
			voice.bus = bus;
			voice.baseVolume = volume;
			voice.pitch = clampedPitch;
			voice.isLooping = isLooping;
			voice.is3D = is3D;
			voice.worldPos = worldPos;
			voice.minDistance = minDist;
			voice.maxDistance = maxDist;
			voice.ownerActorID = ownerActorID;
			voice.cueName = cueName;

			m_activeVoices.push_back(std::move(voice));
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	void AudioSystem::StopLoopingCue(uint64_t ownerActorID, const std::string& cueName)
	{
		for (auto it = m_activeVoices.begin(); it != m_activeVoices.end();)
		{
			bool matchOwner = (ownerActorID == 0 || it->ownerActorID == ownerActorID);
			bool matchCue = (cueName.empty() || it->cueName == cueName);
			if (matchOwner && matchCue)
			{
				if (it->instance)
				{
					try
					{
						it->instance->Stop(true);
					}
					catch (...)
					{
					}
					it->instance.reset();
				}
				it = m_activeVoices.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void AudioSystem::StopAllLoopingSFX()
	{
		for (auto it = m_activeVoices.begin(); it != m_activeVoices.end();)
		{
			if (it->isLooping)
			{
				if (it->instance)
				{
					it->instance->Stop(true);
					it->instance.reset();
				}
				it = m_activeVoices.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	bool AudioSystem::PlayBGM(
		const std::string& filePath,
		float volume,
		float pitch,
		float crossfadeDuration,
		bool loop,
		bool forceRestart
	)
	{
		if (!m_isInitialized) Initialize();
		if (!m_isAudioDeviceAvailable || !m_audioEngine || filePath.empty()) return false;

		std::string normKey = NormalizeSlashes(filePath);

		// - Idempotent guard: if the same BGM track is already active and playing, update base volume without restarting
		if (!forceRestart && m_currentBgm.normalizedPath == normKey && m_currentBgm.instance != nullptr)
		{
			if (m_currentBgm.instance->GetState() == DirectX::PLAYING)
			{
				m_currentBgm.baseVolume = volume;
				m_currentBgm.pitch = std::clamp(pitch, -1.0f, 1.0f);
				m_currentBgm.instance->SetPitch(m_currentBgm.pitch);
				return true;
			}
		}

		DirectX::SoundEffect* effect = GetOrLoadSound(normKey);
		if (!effect) return false;

		try
		{
			auto newInstance = effect->CreateInstance();
			if (!newInstance) return false;

			float clampedPitch = std::clamp(pitch, -1.0f, 1.0f);
			newInstance->SetPitch(clampedPitch);

			float bgmBusGain = GetEffectiveBusGain(AudioBus::BGM);

			if (crossfadeDuration > 0.01f && m_currentBgm.instance && m_currentBgm.instance->GetState() == DirectX::PLAYING)
			{
				if (m_fadingOutBgm.instance)
				{
					m_fadingOutBgm.instance->Stop(true);
					m_fadingOutBgm.instance.reset();
				}
				m_fadingOutBgm = std::move(m_currentBgm);
				m_crossfadeTimer = 0.0f;
				m_crossfadeDuration = crossfadeDuration;
				m_isCrossfading = true;

				newInstance->SetVolume(0.0f);
				newInstance->Play(loop);
			}
			else
			{
				if (m_currentBgm.instance)
				{
					m_currentBgm.instance->Stop(true);
					m_currentBgm.instance.reset();
				}
				if (m_fadingOutBgm.instance)
				{
					m_fadingOutBgm.instance->Stop(true);
					m_fadingOutBgm.instance.reset();
				}
				m_isCrossfading = false;

				newInstance->SetVolume(std::clamp(volume * bgmBusGain, 0.0f, 2.0f));
				newInstance->Play(loop);
			}

			m_currentBgm.instance = std::move(newInstance);
			m_currentBgm.normalizedPath = normKey;
			m_currentBgm.baseVolume = volume;
			m_currentBgm.pitch = clampedPitch;
			m_currentBgm.isLooping = loop;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	void AudioSystem::StopBGM(float fadeOutDuration)
	{
		if (!m_currentBgm.instance) return;

		if (fadeOutDuration > 0.01f && m_currentBgm.instance->GetState() == DirectX::PLAYING)
		{
			if (m_fadingOutBgm.instance)
			{
				m_fadingOutBgm.instance->Stop(true);
				m_fadingOutBgm.instance.reset();
			}
			m_fadingOutBgm = std::move(m_currentBgm);
			m_currentBgm.normalizedPath.clear();
			m_crossfadeTimer = 0.0f;
			m_crossfadeDuration = fadeOutDuration;
			m_isCrossfading = true;
		}
		else
		{
			m_currentBgm.instance->Stop(true);
			m_currentBgm.instance.reset();
			m_currentBgm.normalizedPath.clear();
			if (m_fadingOutBgm.instance)
			{
				m_fadingOutBgm.instance->Stop(true);
				m_fadingOutBgm.instance.reset();
			}
			m_fadingOutBgm.normalizedPath.clear();
			m_isCrossfading = false;
		}
	}

	bool AudioSystem::IsBGMPlaying() const
	{
		return m_currentBgm.instance != nullptr && m_currentBgm.instance->GetState() == DirectX::PLAYING;
	}

	void AudioSystem::TriggerSidechainDuck(const SidechainDuckConfig& config)
	{
		if (!config.enabled) return;
		size_t idx = static_cast<size_t>(config.targetBus);
		if (idx >= m_buses.size()) return;

		AudioBusState& busState = m_buses[idx];
		float clampedTarget = std::clamp(config.targetVolume, 0.0f, 1.0f);

		// - Minimum-floor envelope protection: rapid consecutive clashes hold at the target floor rather than multiplying to zero
		busState.targetDuckGain = std::min(busState.targetDuckGain, clampedTarget);
		busState.duckAttackSpeed = std::max(1.0f, config.attackSpeed);
		busState.duckHoldTimer = std::max(busState.duckHoldTimer, std::max(0.0f, config.holdDuration));
		busState.duckReleaseSpeed = std::max(0.2f, config.releaseSpeed);
	}

	void AudioSystem::ResetAllDucking()
	{
		for (auto& bus : m_buses)
		{
			bus.currentDuckGain = 1.0f;
			bus.targetDuckGain = 1.0f;
			bus.duckHoldTimer = 0.0f;
		}
	}

	void AudioSystem::Update(float unscaledDeltaTime, const DirectX::SimpleMath::Vector3& listenerPos)
	{
		if (!m_isInitialized) Initialize();

		float dt = std::clamp(unscaledDeltaTime, 0.0f, 0.1f);
		m_totalUnscaledTime += dt;
		m_listenerPosition = listenerPos;

		if (m_audioEngine)
		{
			if (!m_audioEngine->Update())
			{
				if (m_audioEngine->IsCriticalError())
				{
					m_isAudioDeviceAvailable = false;
				}
			}
		}

		// - Advance Sidechain Ducking Envelopes (Attack -> Hold -> Release Blend) across all buses
		for (auto& bus : m_buses)
		{
			if (bus.targetDuckGain < 0.999f)
			{
				if (bus.currentDuckGain > bus.targetDuckGain + 0.005f)
				{
					// - Attack phase: rapidly drop volume toward targetDuckGain
					float step = std::min(1.0f, bus.duckAttackSpeed * dt);
					bus.currentDuckGain += (bus.targetDuckGain - bus.currentDuckGain) * step;
				}
				else
				{
					// - Hold phase: sustain ducked gain while clash resonates
					bus.currentDuckGain = bus.targetDuckGain;
					if (bus.duckHoldTimer > 0.0f)
					{
						bus.duckHoldTimer -= dt;
					}
					if (bus.duckHoldTimer <= 0.0f)
					{
						bus.duckHoldTimer = 0.0f;
						bus.targetDuckGain = 1.0f;
					}
				}
			}
			else if (bus.currentDuckGain < 0.999f)
			{
				// - Release phase: smoothly blend volume back to 1.0f
				float step = std::min(1.0f, bus.duckReleaseSpeed * dt);
				bus.currentDuckGain += (1.0f - bus.currentDuckGain) * step;
				if (bus.currentDuckGain >= 0.998f)
				{
					bus.currentDuckGain = 1.0f;
				}
			}
		}

		// - Update Equal-Power BGM Crossfading & Real-Time Bus Ducking Modulation
		float bgmBusGain = GetEffectiveBusGain(AudioBus::BGM);
		if (m_isCrossfading)
		{
			m_crossfadeTimer += dt;
			float u = (m_crossfadeDuration > 0.001f) ? std::clamp(m_crossfadeTimer / m_crossfadeDuration, 0.0f, 1.0f) : 1.0f;

			float outFade = std::cos(u * kHalfPi);
			float inFade = std::sin(u * kHalfPi);

			if (m_fadingOutBgm.instance)
			{
				m_fadingOutBgm.instance->SetVolume(std::clamp(m_fadingOutBgm.baseVolume * bgmBusGain * outFade, 0.0f, 2.0f));
			}
			if (m_currentBgm.instance)
			{
				m_currentBgm.instance->SetVolume(std::clamp(m_currentBgm.baseVolume * bgmBusGain * inFade, 0.0f, 2.0f));
			}

			if (u >= 1.0f)
			{
				if (m_fadingOutBgm.instance)
				{
					m_fadingOutBgm.instance->Stop(true);
					m_fadingOutBgm.instance.reset();
				}
				m_fadingOutBgm.normalizedPath.clear();
				m_isCrossfading = false;
			}
		}
		else if (m_currentBgm.instance)
		{
			if (m_currentBgm.instance->GetState() == DirectX::PLAYING)
			{
				m_currentBgm.instance->SetVolume(std::clamp(m_currentBgm.baseVolume * bgmBusGain, 0.0f, 2.0f));
			}
		}

		// - Update Active SFX & Ambient Voices (Real-time bus gain + 3D spatial attenuation + finished voice cleanup)
		for (auto it = m_activeVoices.begin(); it != m_activeVoices.end();)
		{
			if (!it->instance || it->instance->GetState() == DirectX::STOPPED)
			{
				it = m_activeVoices.erase(it);
				continue;
			}

			float busGain = GetEffectiveBusGain(it->bus);
			float spatial = it->is3D ? Compute3DAttenuation(it->worldPos, it->minDistance, it->maxDistance) : 1.0f;
			it->instance->SetVolume(std::clamp(it->baseVolume * busGain * spatial, 0.0f, 2.0f));
			++it;
		}
	}

	void AudioSystem::UpdateActor3DPosition(uint64_t ownerActorID, const DirectX::SimpleMath::Vector3& worldPos)
	{
		if (ownerActorID == 0) return;
		for (auto& voice : m_activeVoices)
		{
			if (voice.ownerActorID == ownerActorID && voice.is3D)
			{
				voice.worldPos = worldPos;
			}
		}
	}

	void AudioSystem::Suspend()
	{
		if (m_audioEngine)
		{
			m_audioEngine->Suspend();
		}
	}

	void AudioSystem::Resume()
	{
		if (m_audioEngine)
		{
			m_audioEngine->Resume();
		}
	}

	void AudioSystem::SetBusVolume(AudioBus bus, float volume)
	{
		size_t idx = static_cast<size_t>(bus);
		if (idx < m_buses.size())
		{
			m_buses[idx].baseVolume = std::clamp(volume, 0.0f, 2.0f);
		}
	}

	float AudioSystem::GetBusVolume(AudioBus bus) const
	{
		size_t idx = static_cast<size_t>(bus);
		return (idx < m_buses.size()) ? m_buses[idx].baseVolume : 1.0f;
	}

	void AudioSystem::SetBusMuted(AudioBus bus, bool muted)
	{
		size_t idx = static_cast<size_t>(bus);
		if (idx < m_buses.size())
		{
			m_buses[idx].muted = muted;
		}
	}

	bool AudioSystem::IsBusMuted(AudioBus bus) const
	{
		size_t idx = static_cast<size_t>(bus);
		return (idx < m_buses.size()) ? m_buses[idx].muted : false;
	}

	float AudioSystem::GetBusDuckGain(AudioBus bus) const
	{
		size_t idx = static_cast<size_t>(bus);
		return (idx < m_buses.size()) ? m_buses[idx].currentDuckGain : 1.0f;
	}

	float AudioSystem::GetEffectiveBusGain(AudioBus bus) const
	{
		const AudioBusState& master = m_buses[static_cast<size_t>(AudioBus::Master)];
		if (master.muted) return 0.0f;
		float masterGain = master.baseVolume * master.currentDuckGain;

		size_t idx = static_cast<size_t>(bus);
		if (idx == static_cast<size_t>(AudioBus::Master) || idx >= m_buses.size())
		{
			return masterGain;
		}

		const AudioBusState& target = m_buses[idx];
		if (target.muted) return 0.0f;
		return masterGain * target.baseVolume * target.currentDuckGain;
	}
}
