#include "pch.h"
#include "FogComponent.h"
#include <ImGui/imgui.h>

namespace HEIN
{
	FogComponent::FogComponent(Actor* owner)
		: IComponent(owner)
	{
	}

	nlohmann::json FogComponent::Serialize()
	{
		nlohmann::json data = IComponent::Serialize();

		data["FogStart"] = m_fogStart;
		data["FogEnd"] = m_fogEnd;
		data["FogColor"] = nlohmann::json::array({ m_fogColor.x, m_fogColor.y, m_fogColor.z, m_fogColor.w });

		return data;
	}

	void FogComponent::Deserialize(const nlohmann::json& data)
	{
		IComponent::Deserialize(data);

		if (data.contains("FogStart")) m_fogStart = data["FogStart"];
		if (data.contains("FogEnd")) m_fogEnd = data["FogEnd"];
		if (data.contains("FogColor"))
		{
			m_fogColor = DirectX::SimpleMath::Vector4(
				data["FogColor"][0], 
				data["FogColor"][1], 
				data["FogColor"][2], 
				data["FogColor"][3]
			);
		}
	}

	void FogComponent::OnInspectorGUI(GameContext& gameContext)
	{
		if (ImGui::CollapsingHeader("Fog Component", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat("Fog Start", &m_fogStart, 5.0f, 0.0f, 5000.0f);
			ImGui::DragFloat("Fog End", &m_fogEnd, 5.0f, 0.0f, 5000.0f);
			ImGui::ColorEdit4("Fog Color", &m_fogColor.x);
		}
	}
}
