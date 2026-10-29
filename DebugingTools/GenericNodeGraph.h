#pragma once
#include <string>
#include <vector>
#include <functional>

namespace HEIN
{
	/**
	 * @brief Generic pin representation for node editor input/output sockets
	 */
	struct GenericGraphPin
	{
		int id = 0;
		std::string label = "";
		bool isInput = false;
	};

	/**
	 * @brief Generic node model decoupling gameplay state machines from imnodes canvas
	 */
	struct GenericGraphNode
	{
		int id = 0;
		std::string title = "";
		std::string subtitle = "";
		bool isActive = false;
		float progress = 0.0f;

		std::vector<GenericGraphPin> inputPins;
		std::vector<GenericGraphPin> outputPins;

		// Optional callback executed when node is rendered in detailed/magnified LOD mode
		std::function<void()> onRenderDetailedBody = nullptr;
	};

	/**
	 * @brief Generic directed link connecting two attributes with chromatic pulse telemetry
	 */
	struct GenericGraphLink
	{
		int id = 0;
		int startPinId = 0;
		int endPinId = 0;
		bool isPulsing = false;
		float pulseTimer = 0.0f;
	};

	/**
	 * @brief Data container passed to GenericNodeGraphView to render any node graph
	 */
	struct GenericGraphDesc
	{
		std::string graphTitle = "Node View";
		std::string activeNodeName = "";
		std::vector<GenericGraphNode> nodes;
		std::vector<GenericGraphLink> links;

		// Optional toolbar extension callback for domain-specific controls
		std::function<void()> onRenderToolbarExtra = nullptr;
	};
}
