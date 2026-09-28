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

		// Coordinate Space Transform: World Space -> Terrain Model Space
		HEIN::TransformComponent* terrainTrans = m_terrain->GetOwner()->GetComponent<HEIN::TransformComponent>();
		DirectX::SimpleMath::Matrix invWorld = DirectX::SimpleMath::Matrix::Identity;
		if (terrainTrans)
		{
			invWorld = terrainTrans->GetWorldMatrix().Invert();
		}

		// Transform query world position into the terrain's local coordinate frame
		DirectX::SimpleMath::Vector3 localPos = DirectX::SimpleMath::Vector3::Transform(
			DirectX::SimpleMath::Vector3(worldX, 0.0f, worldZ), invWorld);

		int width = m_terrain->GetTerrainWidth();
		int height = m_terrain->GetTerrainHeight();
		float scale = m_terrain->GetHeightScale();
		const auto& heightMap = m_terrain->GetHeightMap();

		if (width <= 0 || height <= 0 || heightMap.empty()) return false;

		// Map local origin (centered at [0, 0]) to heightmap array grid coordinates [0, width) x [0, height)
		float halfWidth = (float)width / 2.0f;
		float halfDepth = (float)height / 2.0f;

		float gridX = localPos.x + halfWidth;
		float gridZ = localPos.z + halfDepth;

		// Boundary Clamping: prevent index overflow beyond the valid heightmap grid extent
		if (gridX < 0.0f) gridX = 0.0f;
		if (gridZ < 0.0f) gridZ = 0.0f;
		if (gridX >= (width - 1.001f)) gridX = (float)width - 1.001f;
		if (gridZ >= (height - 1.001f)) gridZ = (float)height - 1.001f;

		// Integer grid cell index (bottom-left corner of the active quad)
		int col = static_cast<int>(std::floor(gridX));
		int row = static_cast<int>(std::floor(gridZ));

		// Normalized sub-cell fractional coordinates within the unit quad [0, 1] x [0, 1]
		float dx = gridX - (float)col;
		float dz = gridZ - (float)row;

		// Retrieve heightfield samples at all four vertices of the active cell quad
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

		// Triangle Discretization & Barycentric Surface Interpolation:
		// The quad is bisected diagonally from (0, 0) to (1, 1).
		// If (dx + dz <= 1.0), the sample lies on the lower triangle (v00, v10, v01).
		// Otherwise, it lies on the upper triangle (v11, v01, v10).
		if (dx + dz <= 1.0f)
		{
			// Lower Triangle: interpolate along basis vectors (v10 - v00) and (v01 - v00)
			outHeight = h00 + (h10 - h00) * dx + (h01 - h00) * dz;
			outNormal = n00 + (n10 - n00) * dx + (n01 - n00) * dz;
		}
		else
		{
			// Upper Triangle: interpolate relative to opposite corner (1, 1)
			outHeight = h11 - (h11 - h10) * (1.0f - dz) - (h11 - h01) * (1.0f - dx);
			outNormal = n11 - (n11 - n10) * (1.0f - dz) - (n11 - n01) * (1.0f - dx);
		}

		outNormal.Normalize();

		// Transform interpolated local height and surface normal back into World Coordinates
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
