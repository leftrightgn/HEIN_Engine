#include "pch.h"
#include "TerrainColliderComponent.h"
#include "Entities/Actor.h"
#include "Components/TransformComponent.h"
#include <ImGui/imgui.h>
#include <cmath>

namespace HEIN
{
	TerrainColliderComponent::TerrainColliderComponent(Actor* owner)
		: ColliderComponent(owner, ColliderShape::Terrain)
	{
	}

	nlohmann::json TerrainColliderComponent::Serialize()
	{
		return ColliderComponent::Serialize();
	}

	void TerrainColliderComponent::Deserialize(const nlohmann::json& data)
	{
		ColliderComponent::Deserialize(data);
	}

	void TerrainColliderComponent::OnInspectorGUI(GameContext& gameContext)
	{
		if (ImGui::CollapsingHeader("Terrain Collider Component", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ColliderComponent::OnInspectorGUI(gameContext);
			
			if (!m_terrain && m_owner) m_terrain = m_owner->GetComponent<TerrainComponent>();

			if (m_terrain)
			{
				ImGui::Text("Linked to TerrainComponent: Yes");
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Warning: No TerrainComponent found on this actor!");
			}
		}
	}

	void TerrainColliderComponent::Start()
	{
		ColliderComponent::Start();
		
		// Attempt to find the TerrainComponent on the same actor
		if (m_owner)
		{
			m_terrain = m_owner->GetComponent<TerrainComponent>();
		}
	}

	void TerrainColliderComponent::SyncColliderState()
	{
		// Terrain collider is generally static, but if the terrain transform changes,
		// the world transformation inside GetHeightAtPosition will automatically handle it.
	}

	void TerrainColliderComponent::Draw(
		GameContext& gameContext,
		const DirectX::SimpleMath::Matrix& world,
		const DirectX::SimpleMath::Matrix& view,
		const DirectX::SimpleMath::Matrix& proj
	)
	{
		// Optional: Implement debug drawing for the terrain collider if needed
	}

	bool TerrainColliderComponent::GetHeightAtPosition(float worldX, float worldZ, float& outHeight, DirectX::SimpleMath::Vector3& outNormal)
	{
		if (!m_terrain && m_owner) m_terrain = m_owner->GetComponent<TerrainComponent>();
		if (!m_terrain) return false;

		// 1. Get the terrain's world matrix to convert World Space -> Local Space
		HEIN::TransformComponent* terrainTrans = m_terrain->GetOwner()->GetComponent<HEIN::TransformComponent>();
		DirectX::SimpleMath::Matrix invWorld = DirectX::SimpleMath::Matrix::Identity;
		if (terrainTrans)
		{
			invWorld = terrainTrans->GetWorldMatrix().Invert();
		}

		// Convert the player's world position into the terrain's local grid space
		DirectX::SimpleMath::Vector3 localPos = DirectX::SimpleMath::Vector3::Transform(
			DirectX::SimpleMath::Vector3(worldX, 0.0f, worldZ), invWorld);

		int width = m_terrain->GetTerrainWidth();
		int height = m_terrain->GetTerrainHeight();
		float scale = m_terrain->GetHeightScale();
		const auto& heightMap = m_terrain->GetHeightMap();

		if (width <= 0 || height <= 0 || heightMap.empty()) return false;

		// 2. Reverse the centering offset used during buffer generation
		float halfWidth = (float)width / 2.0f;
		float halfDepth = (float)height / 2.0f;

		float gridX = localPos.x + halfWidth;
		float gridZ = localPos.z + halfDepth;

		// 3. Boundary Check with Clamping (so player doesn't fall off the exact edge)
		if (gridX < 0.0f) gridX = 0.0f;
		if (gridZ < 0.0f) gridZ = 0.0f;
		if (gridX >= (width - 1.001f)) gridX = (float)width - 1.001f;
		if (gridZ >= (height - 1.001f)) gridZ = (float)height - 1.001f;

		// 4. Find the integer grid cell (the Quad) the player is standing on
		int col = static_cast<int>(std::floor(gridX));
		int row = static_cast<int>(std::floor(gridZ));

		// 5. Find the fractional decimals (How far across the quad are they?)
		float dx = gridX - (float)col;
		float dz = gridZ - (float)row;

		// 6. Get the 4 corners of this specific quad instantly via array indexing
		int indexBottomLeft = (row * width) + col;
		int indexBottomRight = (row * width) + (col + 1);
		int indexTopLeft = ((row + 1) * width) + col;
		int indexTopRight = ((row + 1) * width) + (col + 1);

		float h00 = heightMap[indexBottomLeft].y * scale;
		float h10 = heightMap[indexBottomRight].y * scale;
		float h01 = heightMap[indexTopLeft].y * scale;
		float h11 = heightMap[indexTopRight].y * scale;

		DirectX::SimpleMath::Vector3 n00(heightMap[indexBottomLeft].nx, heightMap[indexBottomLeft].ny, heightMap[indexBottomLeft].nz);
		DirectX::SimpleMath::Vector3 n10(heightMap[indexBottomRight].nx, heightMap[indexBottomRight].ny, heightMap[indexBottomRight].nz);
		DirectX::SimpleMath::Vector3 n01(heightMap[indexTopLeft].nx, heightMap[indexTopLeft].ny, heightMap[indexTopLeft].nz);
		DirectX::SimpleMath::Vector3 n11(heightMap[indexTopRight].nx, heightMap[indexTopRight].ny, heightMap[indexTopRight].nz);

		// 7. Barycentric Interpolation (Which of the 2 triangles are they standing on?)
		if (dx + dz <= 1.0f)
		{
			// Bottom-Left Triangle
			outHeight = h00 + (h10 - h00) * dx + (h01 - h00) * dz;
			outNormal = n00 + (n10 - n00) * dx + (n01 - n00) * dz;
		}
		else
		{
			// Top-Right Triangle
			outHeight = h11 - (h11 - h10) * (1.0f - dz) - (h11 - h01) * (1.0f - dx);
			outNormal = n11 - (n11 - n10) * (1.0f - dz) - (n11 - n01) * (1.0f - dx);
		}

		outNormal.Normalize();

		// 8. Convert the local height and normal back to World Space
		if (terrainTrans)
		{
			DirectX::SimpleMath::Vector3 worldPoint = DirectX::SimpleMath::Vector3::Transform(
				DirectX::SimpleMath::Vector3(localPos.x, outHeight, localPos.z),
				terrainTrans->GetWorldMatrix());
			outHeight = worldPoint.y;

			outNormal = DirectX::SimpleMath::Vector3::TransformNormal(outNormal, terrainTrans->GetWorldMatrix());
			outNormal.Normalize();
		}

		return true;
	}

	bool TerrainColliderComponent::GetCellAtPosition(float worldX, float worldZ, int& outCol, int& outRow)
	{
		if (!m_terrain && m_owner) m_terrain = m_owner->GetComponent<TerrainComponent>();
		if (!m_terrain) return false;

		HEIN::TransformComponent* terrainTrans = m_terrain->GetOwner()->GetComponent<HEIN::TransformComponent>();
		DirectX::SimpleMath::Matrix invWorld = DirectX::SimpleMath::Matrix::Identity;
		if (terrainTrans)
		{
			invWorld = terrainTrans->GetWorldMatrix().Invert();
		}

		DirectX::SimpleMath::Vector3 localPos = DirectX::SimpleMath::Vector3::Transform(
			DirectX::SimpleMath::Vector3(worldX, 0.0f, worldZ), invWorld);

		int width = m_terrain->GetTerrainWidth();
		int height = m_terrain->GetTerrainHeight();

		if (width <= 0 || height <= 0) return false;

		float halfWidth = (float)width / 2.0f;
		float halfDepth = (float)height / 2.0f;

		float gridX = localPos.x + halfWidth;
		float gridZ = localPos.z + halfDepth;

		if (gridX < 0.0f) gridX = 0.0f;
		if (gridZ < 0.0f) gridZ = 0.0f;
		if (gridX >= (width - 1.001f)) gridX = (float)width - 1.001f;
		if (gridZ >= (height - 1.001f)) gridZ = (float)height - 1.001f;

		outCol = static_cast<int>(std::floor(gridX));
		outRow = static_cast<int>(std::floor(gridZ));

		return true;
	}
}
