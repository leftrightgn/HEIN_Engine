#include "pch.h"
#include "MeshColliderComponent.h"
#include <fstream>
#include <sstream>
#include <Framework/GameContext.h>
#include <ImGui/imgui.h>

HEIN::MeshColliderComponent::MeshColliderComponent(Actor* owner)
	: ColliderComponent(owner, ColliderShape::Mesh)
{
}

void HEIN::MeshColliderComponent::LoadFromObj(const wchar_t* filePath)
{
	m_objPath = filePath;
	std::ifstream file(filePath);
	if (!file.is_open()) return;

	std::string line;

	while (std::getline(file, line))
	{
		std::istringstream iss(line);
		std::string type;
		iss >> type;

		if (type == "v")
		{
			float x, y, z;
			iss >> x >> y >> z;

			DirectX::VertexPosition vertex;
			vertex.position = DirectX::SimpleMath::Vector3(x, y, z);

			m_debugVertices.push_back(vertex);
		}
		else if (type == "f")
		{
			std::string v1, v2, v3;
			iss >> v1 >> v2 >> v3;

			uint16_t i1 = std::stoi(v1.substr(0, v1.find('/'))) - 1;
			uint16_t i2 = std::stoi(v2.substr(0, v2.find('/'))) - 1;
			uint16_t i3 = std::stoi(v3.substr(0, v3.find('/'))) - 1;

			m_debugIndices.push_back(i1);
			m_debugIndices.push_back(i2);
			m_debugIndices.push_back(i3);

			Triangle tri;
			tri.v0 = m_debugVertices[i1].position;
			tri.v1 = m_debugVertices[i2].position;
			tri.v2 = m_debugVertices[i3].position;

			m_localTriangles.push_back(tri);
		}
	}
	m_worldTriangles.resize(m_localTriangles.size());
}

void HEIN::MeshColliderComponent::SyncColliderState()
{
	DirectX::SimpleMath::Matrix worldMatrix = GetCalculateWorldMatrix();

	for (size_t i = 0; i < m_localTriangles.size(); ++i)
	{
		m_worldTriangles[i].v0 = DirectX::SimpleMath::Vector3::Transform(m_localTriangles[i].v0, worldMatrix);
		m_worldTriangles[i].v1 = DirectX::SimpleMath::Vector3::Transform(m_localTriangles[i].v1, worldMatrix);
		m_worldTriangles[i].v2 = DirectX::SimpleMath::Vector3::Transform(m_localTriangles[i].v2, worldMatrix);
	}
}

void HEIN::MeshColliderComponent::Draw(
	GameContext& gameContext, 
	const DirectX::SimpleMath::Matrix& world, 
	const DirectX::SimpleMath::Matrix& view,
	const DirectX::SimpleMath::Matrix& proj
)
{
	if (gameContext.debugCollisionRenderer == nullptr) return;

	DirectX::SimpleMath::Matrix worldMatrix = GetCalculateWorldMatrix();

	DirectX::SimpleMath::Vector3 scale, pos;
	DirectX::SimpleMath::Quaternion rot;
	worldMatrix.Decompose(scale, rot, pos);
    rot.Normalize();

	DirectX::SimpleMath::Color debugColor = DirectX::SimpleMath::Color(DirectX::Colors::Red);
	if (m_isCollidingThisFrame)
	{
		debugColor = DirectX::Colors::Yellow;
	}

	gameContext.debugCollisionRenderer->QueueMesh(
		m_debugVertices,
		m_debugIndices,
		scale,
		pos,
		rot,
		debugColor
	);

}

nlohmann::json HEIN::MeshColliderComponent::Serialize()
{
    nlohmann::json data = ColliderComponent::Serialize();
    std::string pathStr(m_objPath.begin(), m_objPath.end());
    data["ObjPath"] = pathStr;
    return data;
}

void HEIN::MeshColliderComponent::Deserialize(const nlohmann::json& data)
{
    ColliderComponent::Deserialize(data);
    if (data.contains("ObjPath"))
    {
        std::string pathStr = data["ObjPath"];
        m_objPath = std::wstring(pathStr.begin(), pathStr.end());
        if (!m_objPath.empty())
        {
            LoadFromObj(m_objPath.c_str());
        }
    }
}

void HEIN::MeshColliderComponent::OnInspectorGUI(GameContext& gameContext)
{
    ColliderComponent::OnInspectorGUI(gameContext);
    if (ImGui::CollapsingHeader("MeshCollider Properties", ImGuiTreeNodeFlags_DefaultOpen))
    {
        std::string narrowPath(m_objPath.begin(), m_objPath.end());
        char bufferPath[256];
        strcpy_s(bufferPath, sizeof(bufferPath), narrowPath.c_str());
        if (ImGui::InputText("Obj Path", bufferPath, sizeof(bufferPath)))
        {
            std::string newPath(bufferPath);
            m_objPath = std::wstring(newPath.begin(), newPath.end());
            if (!m_objPath.empty())
            {
                LoadFromObj(m_objPath.c_str());
            }
        }
    }
}
