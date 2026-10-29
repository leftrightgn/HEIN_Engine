#include "pch.h"
#if HEIN_WITH_EDITOR
#include "pch.h"
#include "GenericNodeGraphView.h"
#include "ImGui/imgui.h"
#include "ImGui/imnodes.h"
#include <algorithm>
#include <cmath>

void HEIN::GenericNodeGraphView::Render(const GenericGraphDesc& graph, bool* isOpen, int* outSelectedNodeId)
{
	ImGui::SetNextWindowSize(ImVec2(850, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(graph.graphTitle.c_str(), isOpen))
	{
		ImGui::End();
		return;
	}

	// Automatic LOD evaluation based on window size
	ImVec2 winSize = ImGui::GetWindowSize();
	bool isDetailed = m_magnified || (m_autoLOD && (winSize.x >= 1050.0f && winSize.y >= 700.0f));

	// Toolbar
	ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Active: %s", graph.activeNodeName.empty() ? "[None]" : graph.activeNodeName.c_str());
	ImGui::SameLine();
	ImGui::Checkbox("Auto LOD", &m_autoLOD);
	ImGui::SameLine();
	ImGui::Checkbox("Magnified View", &m_magnified);

	if (graph.onRenderToolbarExtra)
	{
		ImGui::SameLine();
		graph.onRenderToolbarExtra();
	}

	ImGui::Separator();

	ImNodes::BeginNodeEditor();

	// Render Generic Nodes
	for (const auto& node : graph.nodes)
	{
		if (node.isActive)
		{
			ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(35, 160, 65, 255));
			ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, IM_COL32(45, 190, 80, 255));
			ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, IM_COL32(55, 220, 95, 255));
			ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(70, 255, 110, 255));
			ImNodes::PushColorStyle(ImNodesCol_NodeBackground, IM_COL32(20, 45, 25, 240));
		}

		ImNodes::BeginNode(node.id);

		// Node Title Bar
		ImNodes::BeginNodeTitleBar();
		if (node.isActive)
		{
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.3f, 1.0f), "[ACTIVE] %s", node.title.c_str());
		}
		else
		{
			ImGui::TextUnformatted(node.title.c_str());
		}
		ImNodes::EndNodeTitleBar();

		// Input Pins
		for (const auto& pin : node.inputPins)
		{
			ImNodes::BeginInputAttribute(pin.id);
			ImGui::TextUnformatted(pin.label.c_str());
			ImNodes::EndInputAttribute();
		}

		// Active State Live Execution Progress Bar
		if (node.isActive && node.progress > 0.0f)
		{
			ImGui::ProgressBar(std::clamp(node.progress, 0.0f, 1.0f), ImVec2(isDetailed ? 140.0f : 90.0f, 12.0f), "");
		}

		// Magnified View reveals subtitles & custom body; Compact view keeps node small and tidy
		if (isDetailed)
		{
			if (!node.subtitle.empty())
			{
				ImGui::TextDisabled("%s", node.subtitle.c_str());
			}
			if (node.onRenderDetailedBody)
			{
				node.onRenderDetailedBody();
			}
		}

		// Output Pins
		for (const auto& pin : node.outputPins)
		{
			ImNodes::BeginOutputAttribute(pin.id);
			ImGui::Indent(isDetailed ? 25.0f : 15.0f);
			ImGui::TextUnformatted(pin.label.c_str());
			ImNodes::EndOutputAttribute();
		}

		ImNodes::EndNode();

		if (node.isActive)
		{
			ImNodes::PopColorStyle();
			ImNodes::PopColorStyle();
			ImNodes::PopColorStyle();
			ImNodes::PopColorStyle();
			ImNodes::PopColorStyle();
		}
	}

	// Render Generic Links with Chromatic Pulse
	for (const auto& link : graph.links)
	{
		if (link.isPulsing)
		{
			float pulse = 0.75f + 0.25f * std::sin(link.pulseTimer * 24.0f);
			unsigned int redCol = IM_COL32(255, static_cast<int>(40.0f * pulse), static_cast<int>(40.0f * pulse), 255);

			ImNodes::PushColorStyle(ImNodesCol_Link, redCol);
			ImNodes::PushColorStyle(ImNodesCol_LinkHovered, redCol);
			ImNodes::PushColorStyle(ImNodesCol_LinkSelected, redCol);
			ImNodes::PushStyleVar(ImNodesStyleVar_LinkThickness, 6.0f);
		}

		ImNodes::Link(link.id, link.startPinId, link.endPinId);

		if (link.isPulsing)
		{
			ImNodes::PopStyleVar();
			ImNodes::PopColorStyle();
			ImNodes::PopColorStyle();
			ImNodes::PopColorStyle();
		}
	}

	ImNodes::EndNodeEditor();

	// Selection synchronization
	if (outSelectedNodeId != nullptr)
	{
		int numSelected = ImNodes::NumSelectedNodes();
		if (numSelected > 0)
		{
			std::vector<int> selectedNodes(numSelected);
			ImNodes::GetSelectedNodes(selectedNodes.data());
			*outSelectedNodeId = selectedNodes[0];
		}
	}

	ImGui::End();
}

#endif
