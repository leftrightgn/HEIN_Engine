#include "pch.h"
#include "Components/SoundComponent.h"
#include "Components/TransformComponent.h"
#include "DebugingTools/EditorUtils.h"
#include "Entities/Actor.h"
#include "Framework/GameContext.h"
#if HEIN_WITH_EDITOR
#include "ImGui/imgui.h"
#include "ImGui/imgui_stdlib.h"
#endif
#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace HEIN
{
	static std::string WideToUtf8Str(const std::wstring& wstr)
	{
		if (wstr.empty()) return "";
		int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (len <= 0) return std::string(wstr.begin(), wstr.end());
		std::string str(static_cast<size_t>(len), '\0');
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], len, nullptr, nullptr);
		if (!str.empty() && str.back() == '\0') str.pop_back();
		return str;
	}

	SoundComponent::SoundComponent(Actor* owner)
		: IComponent(owner)
	{
	}

	SoundComponent::~SoundComponent()
	{
		if (m_owner != nullptr)
		{
			try
			{
				if (AudioSystem* audio = AudioSystem::GetInstance())
				{
					audio->StopLoopingCue(m_owner->GetID());
				}
			}
			catch (...)
			{
			}
		}
	}

	float SoundComponent::RandomPitchOffset(float variance)
	{
		if (variance <= 0.0001f) return 0.0f;
		float u = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
		return u * variance;
	}

	DirectX::SimpleMath::Vector3 SoundComponent::GetOwnerWorldPosition() const
	{
		if (m_owner)
		{
			if (auto* tr = m_owner->GetComponent<TransformComponent>())
			{
				return tr->GetPosition();
			}
		}
		return DirectX::SimpleMath::Vector3::Zero;
	}

	void SoundComponent::Start()
	{
		m_hasTriggeredPlayOnStart = false;
	}

	void SoundComponent::Update(float deltaTime)
	{
		if (!m_owner) return;

		// - Keep 3D voice positions synced with owning Actor's TransformComponent
		if (auto* tr = m_owner->GetComponent<TransformComponent>())
		{
			if (auto* audio = AudioSystem::GetInstance())
			{
				audio->UpdateActor3DPosition(m_owner->GetID(), tr->GetPosition());
			}
		}

		// - Defer playOnStart cues until active simulation ticks (deltaTime > 0.0f) so Editor Scene View stays quiet while authoring
		if (!m_hasTriggeredPlayOnStart && deltaTime > 0.0f)
		{
			m_hasTriggeredPlayOnStart = true;
			for (const auto& cue : m_cues)
			{
				if (cue.playOnStart && !cue.filePath.empty())
				{
					PlayCue(cue.cueName);
				}
			}
		}
	}

	bool SoundComponent::PlayCue(const std::string& cueName, float volumeMultiplier)
	{
		if (cueName.empty()) return false;

		const SoundCueEntry* entry = FindCue(cueName);
		if (!entry || entry->filePath.empty())
		{
			return false;
		}

		AudioSystem* audio = AudioSystem::GetInstance();
		if (!audio) return false;

		float finalVol = std::clamp(entry->volume * volumeMultiplier, 0.0f, 2.0f);
		float finalPitch = std::clamp(entry->pitch + RandomPitchOffset(entry->pitchVariance), -1.0f, 1.0f);

		if (entry->bus == AudioBus::BGM)
		{
			if (entry->ducking.enabled)
			{
				audio->TriggerSidechainDuck(entry->ducking);
			}
			return audio->PlayBGM(entry->filePath, finalVol, finalPitch, entry->crossfadeDuration, entry->isLooping, false);
		}

		uint64_t ownerID = (m_owner != nullptr) ? m_owner->GetID() : 0;
		return audio->PlayCue(
			entry->filePath,
			entry->bus,
			finalVol,
			finalPitch,
			entry->isLooping,
			entry->is3D,
			GetOwnerWorldPosition(),
			entry->minDistance,
			entry->maxDistance,
			entry->ducking,
			ownerID,
			entry->cueName
		);
	}

	bool SoundComponent::PlayCueOrFallbackPath(const std::string& cueNameOrFilePath, AudioBus defaultBus, float volumeMultiplier)
	{
		if (cueNameOrFilePath.empty()) return false;

		if (PlayCue(cueNameOrFilePath, volumeMultiplier))
		{
			return true;
		}

		// - Direct file path or bare asset stem fallback if cue is not in m_cues or user typed a .wav name directly
		AudioSystem* audio = AudioSystem::GetInstance();
		if (!audio) return false;

		SidechainDuckConfig noDuck;
		uint64_t ownerID = (m_owner != nullptr) ? m_owner->GetID() : 0;

		if (audio->PlayCue(
			cueNameOrFilePath,
			defaultBus,
			volumeMultiplier,
			RandomPitchOffset(0.03f),
			false,
			false,
			GetOwnerWorldPosition(),
			15.0f,
			160.0f,
			noDuck,
			ownerID,
			cueNameOrFilePath
		))
		{
			return true;
		}

		if (cueNameOrFilePath.find(".wav") == std::string::npos)
		{
			std::string wavCandidate = cueNameOrFilePath + ".wav";
			if (audio->PlayCue(
				wavCandidate,
				defaultBus,
				volumeMultiplier,
				RandomPitchOffset(0.03f),
				false,
				false,
				GetOwnerWorldPosition(),
				15.0f,
				160.0f,
				noDuck,
				ownerID,
				cueNameOrFilePath
			))
			{
				return true;
			}
		}

		return false;
	}

	void SoundComponent::StopCue(const std::string& cueName, float fadeOutDuration)
	{
		const SoundCueEntry* entry = FindCue(cueName);
		AudioSystem* audio = AudioSystem::GetInstance();
		if (!audio) return;

		if (entry && entry->bus == AudioBus::BGM)
		{
			audio->StopBGM(fadeOutDuration);
			return;
		}

		uint64_t ownerID = (m_owner != nullptr) ? m_owner->GetID() : 0;
		audio->StopLoopingCue(ownerID, cueName);
	}

	void SoundComponent::PlayCombatImpact(CombatImpactSoundType impactType, float intensityMultiplier)
	{
		AudioSystem* audio = AudioSystem::GetInstance();
		switch (impactType)
		{
		case CombatImpactSoundType::ParryClash:
			if (!PlayCue(m_parryClashCue, intensityMultiplier))
			{
				// - Default fallback if user hasn't customized the slot name yet
				SidechainDuckConfig clashDuck;
				clashDuck.enabled = true;
				clashDuck.targetBus = AudioBus::BGM;
				clashDuck.targetVolume = 0.25f;
				clashDuck.attackSpeed = 28.0f;
				clashDuck.holdDuration = 0.32f;
				clashDuck.releaseSpeed = 3.0f;
				if (audio) audio->PlayCue(
					"Resources/Audio/Clash_Parry.wav",
					AudioBus::CombatSFX,
					std::clamp(1.0f * intensityMultiplier, 0.0f, 1.5f),
					RandomPitchOffset(0.04f),
					false,
					false,
					GetOwnerWorldPosition(),
					15.0f,
					160.0f,
					clashDuck
				);
			}
			break;

		case CombatImpactSoundType::BlockGuard:
			if (!PlayCue(m_blockGuardCue, intensityMultiplier))
			{
				SidechainDuckConfig blockDuck;
				blockDuck.enabled = true;
				blockDuck.targetBus = AudioBus::BGM;
				blockDuck.targetVolume = 0.55f;
				blockDuck.attackSpeed = 24.0f;
				blockDuck.holdDuration = 0.14f;
				blockDuck.releaseSpeed = 4.0f;
				if (audio) audio->PlayCue(
					"Resources/Audio/Guard_Block.wav",
					AudioBus::CombatSFX,
					std::clamp(0.95f * intensityMultiplier, 0.0f, 1.5f),
					RandomPitchOffset(0.05f),
					false,
					false,
					GetOwnerWorldPosition(),
					15.0f,
					160.0f,
					blockDuck
				);
			}
			break;

		case CombatImpactSoundType::GuardBreak:
			if (!PlayCue(m_guardBreakCue, intensityMultiplier))
			{
				SidechainDuckConfig breakDuck;
				breakDuck.enabled = true;
				breakDuck.targetBus = AudioBus::BGM;
				breakDuck.targetVolume = 0.22f;
				breakDuck.attackSpeed = 30.0f;
				breakDuck.holdDuration = 0.45f;
				breakDuck.releaseSpeed = 2.5f;
				if (audio) audio->PlayCue(
					"Resources/Audio/Guard_Break.wav",
					AudioBus::CombatSFX,
					std::clamp(1.1f * intensityMultiplier, 0.0f, 1.5f),
					RandomPitchOffset(0.03f),
					false,
					false,
					GetOwnerWorldPosition(),
					15.0f,
					160.0f,
					breakDuck
				);
			}
			break;

		case CombatImpactSoundType::FleshHit:
			if (!PlayCue(m_fleshHitCue, intensityMultiplier))
			{
				SidechainDuckConfig noDuck;
				if (audio) audio->PlayCue(
					"Resources/Audio/Hit_Flesh.wav",
					AudioBus::CombatSFX,
					std::clamp(0.95f * intensityMultiplier, 0.0f, 1.5f),
					RandomPitchOffset(0.05f),
					false,
					false,
					GetOwnerWorldPosition(),
					15.0f,
					160.0f,
					noDuck
				);
			}
			break;

		case CombatImpactSoundType::HeavyHit:
			if (!PlayCue(m_heavyHitCue, intensityMultiplier))
			{
				PlayCombatImpact(CombatImpactSoundType::FleshHit, intensityMultiplier * 1.15f);
			}
			break;
		}
	}

	void SoundComponent::AddOrUpdateCue(const SoundCueEntry& entry)
	{
		for (auto& existing : m_cues)
		{
			if (existing.cueName == entry.cueName)
			{
				existing = entry;
				return;
			}
		}
		m_cues.push_back(entry);
	}

	bool SoundComponent::HasCue(const std::string& cueName) const
	{
		return FindCue(cueName) != nullptr;
	}

	const SoundCueEntry* SoundComponent::FindCue(const std::string& cueName) const
	{
		if (cueName.empty()) return nullptr;

		// - Exact cueName match first
		for (const auto& cue : m_cues)
		{
			if (cue.cueName == cueName)
			{
				return &cue;
			}
		}

		// - Synonym / alias mapping so state effects using WAV stems ("Slash_Swing1", "Slash_Heavy", "Dodge_Roll")
		//   automatically resolve to the user's configured SoundComponent cue ("Slash1", "Slash2", "Slash3", "Dodge")
		std::string aliasCue;
		if (cueName == "Slash_Swing1" || cueName == "Slash_Swing" || cueName == "Combo1" || cueName == "OneHand")
		{
			aliasCue = "Slash1";
		}
		else if (cueName == "Slash_Swing2" || cueName == "Combo2")
		{
			aliasCue = "Slash2";
		}
		else if (cueName == "Slash_Heavy" || cueName == "Combo3")
		{
			aliasCue = "Slash3";
		}
		else if (cueName == "HeavyAttack" || cueName == "HighSpinAttack")
		{
			aliasCue = "HeavySlam";
		}
		else if (cueName == "Dodge_Roll" || cueName == "Roll")
		{
			aliasCue = "Dodge";
		}

		if (!aliasCue.empty())
		{
			for (const auto& cue : m_cues)
			{
				if (cue.cueName == aliasCue)
				{
					return &cue;
				}
			}
		}

		// - Stem / filename match against cue.filePath (e.g. "Slash_Swing" matches "Resources/Audio/Slash_Swing.wav")
		std::filesystem::path targetPath(cueName);
		std::string targetStem = targetPath.stem().string();
		std::string targetFilename = targetPath.filename().string();
		for (const auto& cue : m_cues)
		{
			if (cue.filePath.empty()) continue;
			std::filesystem::path cueFile(cue.filePath);
			if (cue.filePath == cueName ||
				cueFile.filename().string() == targetFilename ||
				(!targetStem.empty() && cueFile.stem().string() == targetStem))
			{
				return &cue;
			}
		}

		return nullptr;
	}

	void SoundComponent::ApplyBGMPreset()
	{
		auto addBgm = [this](const char* name, const char* path, bool loop, bool onStart, float fade)
		{
			SoundCueEntry e;
			e.cueName = name;
			e.filePath = path;
			e.bus = AudioBus::BGM;
			e.volume = 0.85f;
			e.pitch = 0.0f;
			e.pitchVariance = 0.0f;
			e.isLooping = loop;
			e.playOnStart = onStart;
			e.crossfadeDuration = fade;
			AddOrUpdateCue(e);
		};

		addBgm("BGM_Arena", "Resources/Audio/BGM_Arena.wav", true, true, 1.5f);
		addBgm("BGM_Intro", "Resources/Audio/BGM_Intro.wav", true, false, 1.2f);
		addBgm("BGM_Victory", "Resources/Audio/BGM_Victory.wav", false, false, 0.8f);
		addBgm("BGM_GameOver", "Resources/Audio/BGM_GameOver.wav", false, false, 0.8f);
	}

	void SoundComponent::ApplyCombatCharacterPreset()
	{
		auto addSfx = [this](const char* name, const char* path, float vol, float pitchVar, bool duck, float duckVol, float holdSec, float releaseSpd)
		{
			SoundCueEntry e;
			e.cueName = name;
			e.filePath = path;
			e.bus = AudioBus::CombatSFX;
			e.volume = vol;
			e.pitch = 0.0f;
			e.pitchVariance = pitchVar;
			e.isLooping = false;
			e.playOnStart = false;
			e.ducking.enabled = duck;
			e.ducking.targetBus = AudioBus::BGM;
			e.ducking.targetVolume = duckVol;
			e.ducking.attackSpeed = 28.0f;
			e.ducking.holdDuration = holdSec;
			e.ducking.releaseSpeed = releaseSpd;
			AddOrUpdateCue(e);
		};

		addSfx("Slash1", "Resources/Audio/Slash_Swing.wav", 0.90f, 0.05f, false, 1.0f, 0.0f, 3.0f);
		addSfx("Slash2", "Resources/Audio/Slash_Swing.wav", 0.92f, 0.05f, false, 1.0f, 0.0f, 3.0f);
		addSfx("Slash3", "Resources/Audio/Slash_Heavy.wav", 1.00f, 0.04f, true, 0.55f, 0.15f, 3.5f);
		addSfx("HeavySlam", "Resources/Audio/Slash_Heavy.wav", 1.05f, 0.03f, true, 0.45f, 0.22f, 3.0f);
		addSfx("Dodge", "Resources/Audio/Dodge_Roll.wav", 0.80f, 0.04f, false, 1.0f, 0.0f, 3.0f);
		addSfx("Footstep", "Resources/Audio/Footstep.wav", 0.65f, 0.06f, false, 1.0f, 0.0f, 3.0f);

		// - Sword Clash / Perfect Parry: lowers BGM to 25% and smoothly blends back
		addSfx("Clash_Parry", "Resources/Audio/Clash_Parry.wav", 1.10f, 0.04f, true, 0.25f, 0.35f, 2.8f);
		addSfx("Guard_Block", "Resources/Audio/Guard_Block.wav", 0.95f, 0.05f, true, 0.55f, 0.14f, 4.0f);
		addSfx("Guard_Break", "Resources/Audio/Guard_Break.wav", 1.10f, 0.03f, true, 0.22f, 0.45f, 2.4f);
		addSfx("Hit_Flesh", "Resources/Audio/Hit_Flesh.wav", 0.95f, 0.05f, false, 1.0f, 0.0f, 3.0f);
	}

	void SoundComponent::ApplyUIButtonPreset()
	{
		SoundCueEntry hover;
		hover.cueName = "UI_Hover";
		hover.filePath = "Resources/Audio/UI_Hover.wav";
		hover.bus = AudioBus::UISFX;
		hover.volume = 0.75f;
		hover.pitchVariance = 0.02f;
		AddOrUpdateCue(hover);

		SoundCueEntry click;
		click.cueName = "UI_Click";
		click.filePath = "Resources/Audio/UI_Click.wav";
		click.bus = AudioBus::UISFX;
		click.volume = 0.95f;
		click.pitchVariance = 0.01f;
		AddOrUpdateCue(click);
	}

	void SoundComponent::CopyConfigurationFrom(const SoundComponent& other)
	{
		m_cues = other.m_cues;
		m_parryClashCue = other.m_parryClashCue;
		m_blockGuardCue = other.m_blockGuardCue;
		m_guardBreakCue = other.m_guardBreakCue;
		m_fleshHitCue = other.m_fleshHitCue;
		m_heavyHitCue = other.m_heavyHitCue;
	}

	nlohmann::json SoundComponent::Serialize()
	{
		nlohmann::json data = IComponent::Serialize();
		data["ParryClashCue"] = m_parryClashCue;
		data["BlockGuardCue"] = m_blockGuardCue;
		data["GuardBreakCue"] = m_guardBreakCue;
		data["FleshHitCue"] = m_fleshHitCue;
		data["HeavyHitCue"] = m_heavyHitCue;

		data["Cues"] = nlohmann::json::array();
		for (const auto& cue : m_cues)
		{
			nlohmann::json cJson;
			cJson["CueName"] = cue.cueName;
			cJson["FilePath"] = cue.filePath;
			cJson["Bus"] = AudioSystem::AudioBusToString(cue.bus);
			cJson["Volume"] = cue.volume;
			cJson["Pitch"] = cue.pitch;
			cJson["PitchVariance"] = cue.pitchVariance;
			cJson["IsLooping"] = cue.isLooping;
			cJson["PlayOnStart"] = cue.playOnStart;
			cJson["CrossfadeDuration"] = cue.crossfadeDuration;
			cJson["Is3D"] = cue.is3D;
			cJson["MinDistance"] = cue.minDistance;
			cJson["MaxDistance"] = cue.maxDistance;
			cJson["DuckEnabled"] = cue.ducking.enabled;
			cJson["DuckTargetBus"] = AudioSystem::AudioBusToString(cue.ducking.targetBus);
			cJson["DuckTargetVolume"] = cue.ducking.targetVolume;
			cJson["DuckAttackSpeed"] = cue.ducking.attackSpeed;
			cJson["DuckHoldDuration"] = cue.ducking.holdDuration;
			cJson["DuckReleaseSpeed"] = cue.ducking.releaseSpeed;
			data["Cues"].push_back(cJson);
		}
		return data;
	}

	void SoundComponent::Deserialize(const nlohmann::json& data)
	{
		IComponent::Deserialize(data);

		if (data.contains("ParryClashCue")) m_parryClashCue = data["ParryClashCue"].get<std::string>();
		if (data.contains("BlockGuardCue")) m_blockGuardCue = data["BlockGuardCue"].get<std::string>();
		if (data.contains("GuardBreakCue")) m_guardBreakCue = data["GuardBreakCue"].get<std::string>();
		if (data.contains("FleshHitCue")) m_fleshHitCue = data["FleshHitCue"].get<std::string>();
		if (data.contains("HeavyHitCue")) m_heavyHitCue = data["HeavyHitCue"].get<std::string>();

		m_cues.clear();
		if (data.contains("Cues") && data["Cues"].is_array())
		{
			for (const auto& cJson : data["Cues"])
			{
				SoundCueEntry cue;
				cue.cueName = cJson.value("CueName", "NewCue");
				cue.filePath = cJson.value("FilePath", "");
				cue.bus = AudioSystem::StringToAudioBus(cJson.value("Bus", "CombatSFX"));
				cue.volume = cJson.value("Volume", 1.0f);
				cue.pitch = cJson.value("Pitch", 0.0f);
				cue.pitchVariance = cJson.value("PitchVariance", 0.04f);
				cue.isLooping = cJson.value("IsLooping", false);
				cue.playOnStart = cJson.value("PlayOnStart", false);
				cue.crossfadeDuration = cJson.value("CrossfadeDuration", 1.2f);
				cue.is3D = cJson.value("Is3D", false);
				cue.minDistance = cJson.value("MinDistance", 15.0f);
				cue.maxDistance = cJson.value("MaxDistance", 160.0f);
				cue.ducking.enabled = cJson.value("DuckEnabled", false);
				cue.ducking.targetBus = AudioSystem::StringToAudioBus(cJson.value("DuckTargetBus", "BGM"));
				cue.ducking.targetVolume = cJson.value("DuckTargetVolume", 0.25f);
				cue.ducking.attackSpeed = cJson.value("DuckAttackSpeed", 28.0f);
				cue.ducking.holdDuration = cJson.value("DuckHoldDuration", 0.30f);
				cue.ducking.releaseSpeed = cJson.value("DuckReleaseSpeed", 3.2f);
				m_cues.push_back(cue);
			}
		}
	}

#if HEIN_WITH_EDITOR
	void SoundComponent::OnInspectorGUI(GameContext& gameContext)
	{
		HWND windowHandle = gameContext.deviceResources.GetWindow();
		AudioSystem* audio = AudioSystem::GetInstance();

		if (ImGui::CollapsingHeader("Sound Component (BGM, Combat SFX & Sidechain Ducking)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			// - Quick Role Presets for GameManager, Combat Character, or UI Button
			ImGui::TextColored(ImVec4(0.35f, 0.90f, 1.0f, 1.0f), "Quick Actor Presets:");
			if (ImGui::Button("+ BGM / GameManager Preset"))
			{
				ApplyBGMPreset();
			}
			ImGui::SameLine();
			if (ImGui::Button("+ Combat Character Preset"))
			{
				ApplyCombatCharacterPreset();
			}
			ImGui::SameLine();
			if (ImGui::Button("+ UI Button Preset"))
			{
				ApplyUIButtonPreset();
			}

			// - Live Global Bus Mixer & Ducking Envelope Monitor
			if (audio && ImGui::TreeNodeEx("Global Audio Bus Mixer & Live Ducking Monitor", ImGuiTreeNodeFlags_DefaultOpen))
			{
				float bgmDuck = audio->GetBusDuckGain(AudioBus::BGM);
				ImVec4 duckColor = (bgmDuck < 0.95f)
					? ImVec4(1.0f, 0.65f, 0.15f, 1.0f)
					: ImVec4(0.30f, 0.95f, 0.45f, 1.0f);
				ImGui::TextColored(duckColor, "BGM Sidechain Ducking Gain: %.0f%%", bgmDuck * 100.0f);
				ImGui::ProgressBar(bgmDuck, ImVec2(-1.0f, 14.0f));

				for (int b = 0; b < static_cast<int>(AudioBus::Count); ++b)
				{
					AudioBus bus = static_cast<AudioBus>(b);
					ImGui::PushID(b);
					float vol = audio->GetBusVolume(bus);
					bool muted = audio->IsBusMuted(bus);
					if (ImGui::Checkbox("##Mute", &muted))
					{
						audio->SetBusMuted(bus, muted);
					}
					ImGui::SameLine();
					if (ImGui::SliderFloat(AudioSystem::AudioBusToString(bus), &vol, 0.0f, 1.5f, "%.2f"))
					{
						audio->SetBusVolume(bus, vol);
					}
					ImGui::PopID();
				}

				if (ImGui::Button("Test Clash + BGM Ducking Effect"))
				{
					if (!audio->IsBGMPlaying())
					{
						audio->PlayBGM("Resources/Audio/BGM_Arena.wav", 0.85f, 0.0f, 0.3f, true);
					}
					PlayCombatImpact(CombatImpactSoundType::ParryClash, 1.0f);
				}
				ImGui::SameLine();
				if (ImGui::Button("Stop BGM"))
				{
					audio->StopBGM(0.5f);
				}
				ImGui::SameLine();
				if (ImGui::Button("Refresh .wav Files"))
				{
					audio->ReloadAllAudioAssets();
				}
				ImGui::TreePop();
			}

			// - Reactive Combat Collision Sound Mappings
			if (ImGui::TreeNode("Reactive Combat Impact Cue Mappings (Used on Parry / Block / Hit)"))
			{
				ImGui::InputText("Perfect Parry / Sword Clash Cue", &m_parryClashCue);
				ImGui::InputText("Standard Shield Block Cue", &m_blockGuardCue);
				ImGui::InputText("Guard Break Stagger Cue", &m_guardBreakCue);
				ImGui::InputText("Flesh Hit Impact Cue", &m_fleshHitCue);
				ImGui::InputText("Heavy Strike Impact Cue", &m_heavyHitCue);
				ImGui::TreePop();
			}

			ImGui::Separator();
			ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.30f, 1.0f), "Configured Sound Cues (%zu slots):", m_cues.size());

			if (ImGui::Button("+ Add New Sound Cue Slot"))
			{
				SoundCueEntry newEntry;
				newEntry.cueName = "Cue_" + std::to_string(m_cues.size());
				const auto& discovered = AudioSystem::GetDiscoveredAudioFiles();
				if (!discovered.empty())
				{
					newEntry.filePath = discovered.front();
				}
				m_cues.push_back(newEntry);
			}

			const auto& discoveredFiles = AudioSystem::GetDiscoveredAudioFiles();
			const char* busNames[] = { "Master", "BGM", "Ambient", "CombatSFX", "CharacterSFX", "UISFX" };

			int cueToDelete = -1;
			for (int i = 0; i < static_cast<int>(m_cues.size()); ++i)
			{
				SoundCueEntry& cue = m_cues[i];
				ImGui::PushID(i);
				ImGui::Separator();

				std::string headerLabel = cue.cueName + " [" + AudioSystem::AudioBusToString(cue.bus) + "]###CueHeader_" + std::to_string(i);
				bool open = ImGui::TreeNodeEx(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

				ImGui::SameLine();
				if (ImGui::SmallButton("Play"))
				{
					PlayCue(cue.cueName);
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Stop"))
				{
					StopCue(cue.cueName, 0.3f);
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Delete"))
				{
					cueToDelete = i;
				}

				if (open)
				{
					ImGui::InputText("Cue Name (ID)", &cue.cueName);

					// - Discovered .wav file dropdown + File Browser
					if (!discoveredFiles.empty())
					{
						if (ImGui::BeginCombo("Audio Asset (.wav)", cue.filePath.empty() ? "<Select .wav>" : cue.filePath.c_str()))
						{
							for (const auto& f : discoveredFiles)
							{
								bool isSel = (cue.filePath == f);
								if (ImGui::Selectable(f.c_str(), isSel))
								{
									cue.filePath = f;
								}
								if (isSel) ImGui::SetItemDefaultFocus();
							}
							ImGui::EndCombo();
						}
					}
					ImGui::InputText("File Path", &cue.filePath);
					ImGui::SameLine();
					if (ImGui::Button("Browse..."))
					{
						std::wstring selected = EditorUtils::OpenFileDialog(L"WAV Audio Files\0*.wav\0All Files\0*.*\0", windowHandle);
						if (!selected.empty())
						{
							std::wstring rel = EditorUtils::MakeRelativePath(selected);
							cue.filePath = WideToUtf8Str(rel);
							for (char& ch : cue.filePath) { if (ch == '\\') ch = '/'; }
							AudioSystem::RefreshDiscoveredAudioFiles();
						}
					}

					int busIdx = static_cast<int>(cue.bus);
					if (ImGui::Combo("Audio Bus", &busIdx, busNames, IM_ARRAYSIZE(busNames)))
					{
						cue.bus = static_cast<AudioBus>(busIdx);
					}

					ImGui::SliderFloat("Volume", &cue.volume, 0.0f, 1.5f, "%.2f");
					ImGui::SliderFloat("Pitch Offset", &cue.pitch, -0.75f, 0.75f, "%.2f");
					ImGui::SliderFloat("Pitch Variance", &cue.pitchVariance, 0.0f, 0.25f, "%.3f");

					ImGui::Checkbox("Is Looping", &cue.isLooping);
					ImGui::SameLine();
					ImGui::Checkbox("Play On Start", &cue.playOnStart);
					ImGui::SameLine();
					ImGui::Checkbox("3D Spatial", &cue.is3D);

					if (cue.bus == AudioBus::BGM)
					{
						ImGui::SliderFloat("BGM Crossfade (s)", &cue.crossfadeDuration, 0.0f, 5.0f, "%.2f s");
					}

					if (cue.is3D)
					{
						ImGui::DragFloatRange2("3D Falloff Distance", &cue.minDistance, &cue.maxDistance, 1.0f, 1.0f, 1000.0f, "Min: %.1f m", "Max: %.1f m");
					}

					// - Sidechain Ducking Controls (e.g., Clashing Sound Lowers BGM with Blending Effect)
					ImGui::Checkbox("Duck Target Bus on Play (Sidechain Blending)", &cue.ducking.enabled);
					if (cue.ducking.enabled)
					{
						int duckBusIdx = static_cast<int>(cue.ducking.targetBus);
						if (ImGui::Combo("Target Bus to Duck", &duckBusIdx, busNames, IM_ARRAYSIZE(busNames)))
						{
							cue.ducking.targetBus = static_cast<AudioBus>(duckBusIdx);
						}
						ImGui::SliderFloat("Ducked Target Volume", &cue.ducking.targetVolume, 0.0f, 1.0f, "%.2f (Lowered Level)");
						ImGui::SliderFloat("Attack Speed (Drop)", &cue.ducking.attackSpeed, 1.0f, 60.0f, "%.1f /s");
						ImGui::SliderFloat("Hold Duration (s)", &cue.ducking.holdDuration, 0.0f, 2.5f, "%.2f s");
						ImGui::SliderFloat("Release Blend Speed", &cue.ducking.releaseSpeed, 0.5f, 15.0f, "%.2f /s");
					}

					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			if (cueToDelete >= 0 && cueToDelete < static_cast<int>(m_cues.size()))
			{
				m_cues.erase(m_cues.begin() + cueToDelete);
			}
		}
	}
#else
	void SoundComponent::OnInspectorGUI(GameContext& /*gameContext*/) {}
#endif
}
