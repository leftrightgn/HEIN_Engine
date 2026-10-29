#pragma once
#include "GenericNodeGraph.h"

namespace HEIN
{
	/**
	 * @brief Component-agnostic node graph presentation engine
	 * Renders visual graphs with adaptive LOD and dynamic chromatic link pulses.
	 */
	class GenericNodeGraphView
	{
	public:
		GenericNodeGraphView() = default;
		~GenericNodeGraphView() = default;

		/**
		 * @brief Renders the visual node graph canvas window
		 * @param graph Generic graph description container
		 * @param isOpen Pointer to boolean controlling window visibility
		 * @param outSelectedNodeId Receives ID of user-selected node
		 */
		void Render(const GenericGraphDesc& graph, bool* isOpen, int* outSelectedNodeId);

		bool IsMagnified() const { return m_magnified; }
		void SetMagnified(bool magnified) { m_magnified = magnified; }

		bool IsAutoLOD() const { return m_autoLOD; }
		void SetAutoLOD(bool autoLOD) { m_autoLOD = autoLOD; }

	private:
		bool m_magnified = false;
		bool m_autoLOD = true;
	};
}
