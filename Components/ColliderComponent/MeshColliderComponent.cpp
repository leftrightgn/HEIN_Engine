#include "pch.h"
#include "MeshColliderComponent.h"
#include <fstream>
#include <sstream>
#include <Framework/GameContext.h>
#include <DebugingTools/EditorUtils.h>
#include <ImGui/imgui.h>

HEIN::MeshColliderComponent::MeshColliderComponent(Actor* owner)
	: ColliderComponent(owner, ColliderShape::Mesh)
{
}

void HEIN::MeshColliderComponent::LoadFromObj(const wchar_t* filePath, ID3D11Device* device)
{
	m_objPath = (filePath != nullptr) ? filePath : L"";
	m_debugVertices.clear();
	m_debugIndices.clear();
	m_localTriangles.clear();
	m_worldTriangles.clear();
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();
	m_indexCount = 0;

	if (m_objPath.empty()) return;

	std::ifstream file(m_objPath);
	if (!file.is_open())
	{
		file.open(L"Dual/" + m_objPath);
	}
	if (!file.is_open()) return;

	std::string line;

	try
	{
		while (std::getline(file, line))
		{
			if (line.empty()) continue;

			std::istringstream iss(line);
			std::string type;
			if (!(iss >> type)) continue;

			if (type == "v")
			{
				float x, y, z;
				if (iss >> x >> y >> z)
				{
					DirectX::VertexPosition vertex;
					vertex.position = DirectX::SimpleMath::Vector3(x, y, z);
					m_debugVertices.push_back(vertex);
				}
			}
			else if (type == "f")
			{
				std::vector<std::string> faceTokens;
				std::string token;
				while (iss >> token)
				{
					faceTokens.push_back(token);
				}

				if (faceTokens.size() >= 3)
				{
					auto ParseIndex = [](const std::string& tok, size_t vertCount) -> int
					{
						std::string idxStr = tok.substr(0, tok.find('/'));
						if (idxStr.empty()) return -1;
						int idx = std::stoi(idxStr);
						if (idx > 0) return idx - 1; // 1-based index
						if (idx < 0) return static_cast<int>(vertCount) + idx; // Relative index
						return -1;
					};

					int i0 = ParseIndex(faceTokens[0], m_debugVertices.size());
					int i1 = ParseIndex(faceTokens[1], m_debugVertices.size());
					int i2 = ParseIndex(faceTokens[2], m_debugVertices.size());

					if (i0 >= 0 && i1 >= 0 && i2 >= 0 &&
						static_cast<size_t>(i0) < m_debugVertices.size() &&
						static_cast<size_t>(i1) < m_debugVertices.size() &&
						static_cast<size_t>(i2) < m_debugVertices.size())
					{
						m_debugIndices.push_back(static_cast<uint32_t>(i0));
						m_debugIndices.push_back(static_cast<uint32_t>(i1));
						m_debugIndices.push_back(static_cast<uint32_t>(i2));

						Triangle tri;
						tri.v0 = m_debugVertices[i0].position;
						tri.v1 = m_debugVertices[i1].position;
						tri.v2 = m_debugVertices[i2].position;
						m_localTriangles.push_back(tri);
					}

					// Support quads (triangulate 4th vertex as i0, i2, i3)
					if (faceTokens.size() >= 4)
					{
						int i3 = ParseIndex(faceTokens[3], m_debugVertices.size());
						if (i0 >= 0 && i2 >= 0 && i3 >= 0 &&
							static_cast<size_t>(i0) < m_debugVertices.size() &&
							static_cast<size_t>(i2) < m_debugVertices.size() &&
							static_cast<size_t>(i3) < m_debugVertices.size())
						{
							m_debugIndices.push_back(static_cast<uint32_t>(i0));
							m_debugIndices.push_back(static_cast<uint32_t>(i2));
							m_debugIndices.push_back(static_cast<uint32_t>(i3));

							Triangle tri;
							tri.v0 = m_debugVertices[i0].position;
							tri.v1 = m_debugVertices[i2].position;
							tri.v2 = m_debugVertices[i3].position;
							m_localTriangles.push_back(tri);
						}
					}
				}
			}
		}
	}
	catch (...)
	{
		// Safely swallow parsing errors from invalid OBJ lines
	}

	m_worldTriangles.resize(m_localTriangles.size());
	SyncColliderState();

	// Debug wireframe buffers are built lazily in Draw() only when the editor shows colliders.
	// Runtime collision only needs m_localTriangles / m_worldTriangles.
	(void)device;
	m_debugVertices.clear();
	m_debugVertices.shrink_to_fit();
	m_debugIndices.clear();
	m_debugIndices.shrink_to_fit();
}

void HEIN::MeshColliderComponent::CreateGpuBuffers(ID3D11Device* device)
{
	if (!device) return;

	if (m_debugVertices.empty() && !m_localTriangles.empty())
	{
		m_debugVertices.reserve(m_localTriangles.size() * 3);
		m_debugIndices.reserve(m_localTriangles.size() * 3);
		for (const auto& tri : m_localTriangles)
		{
			uint32_t baseIdx = static_cast<uint32_t>(m_debugVertices.size());
			m_debugVertices.emplace_back(tri.v0);
			m_debugVertices.emplace_back(tri.v1);
			m_debugVertices.emplace_back(tri.v2);
			m_debugIndices.push_back(baseIdx);
			m_debugIndices.push_back(baseIdx + 1);
			m_debugIndices.push_back(baseIdx + 2);
		}
	}

	if (m_debugVertices.empty() || m_debugIndices.empty()) return;

	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();

	D3D11_BUFFER_DESC vbd = {};
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = static_cast<UINT>(sizeof(DirectX::VertexPosition) * m_debugVertices.size());
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vData = {};
	vData.pSysMem = m_debugVertices.data();

	HRESULT hr = device->CreateBuffer(&vbd, &vData, m_vertexBuffer.ReleaseAndGetAddressOf());
	if (FAILED(hr)) return;

	D3D11_BUFFER_DESC ibd = {};
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * m_debugIndices.size());
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA iData = {};
	iData.pSysMem = m_debugIndices.data();

	hr = device->CreateBuffer(&ibd, &iData, m_indexBuffer.ReleaseAndGetAddressOf());
	if (FAILED(hr))
	{
		m_vertexBuffer.Reset();
		return;
	}

	m_indexCount = static_cast<UINT>(m_debugIndices.size());
}

void HEIN::MeshColliderComponent::ReleaseDebugResources()
{
	m_vertexBuffer.Reset();
	m_indexBuffer.Reset();
	m_indexCount = 0;
	m_debugVertices.clear();
	m_debugVertices.shrink_to_fit();
	m_debugIndices.clear();
	m_debugIndices.shrink_to_fit();
}

void HEIN::MeshColliderComponent::Start()
{
	ColliderComponent::Start();
	SyncColliderState();
}

void HEIN::MeshColliderComponent::SyncColliderState()
{
	DirectX::SimpleMath::Matrix worldMatrix = CalculateWorldMatrix();

	if (m_worldTriangles.size() != m_localTriangles.size())
	{
		m_worldTriangles.resize(m_localTriangles.size());
	}

	DirectX::SimpleMath::Vector3 minPt(FLT_MAX, FLT_MAX, FLT_MAX);
	DirectX::SimpleMath::Vector3 maxPt(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (size_t i = 0; i < m_localTriangles.size(); ++i)
	{
		m_worldTriangles[i].v0 = DirectX::SimpleMath::Vector3::Transform(m_localTriangles[i].v0, worldMatrix);
		m_worldTriangles[i].v1 = DirectX::SimpleMath::Vector3::Transform(m_localTriangles[i].v1, worldMatrix);
		m_worldTriangles[i].v2 = DirectX::SimpleMath::Vector3::Transform(m_localTriangles[i].v2, worldMatrix);

		minPt = DirectX::SimpleMath::Vector3::Min(minPt, m_worldTriangles[i].v0);
		minPt = DirectX::SimpleMath::Vector3::Min(minPt, m_worldTriangles[i].v1);
		minPt = DirectX::SimpleMath::Vector3::Min(minPt, m_worldTriangles[i].v2);

		maxPt = DirectX::SimpleMath::Vector3::Max(maxPt, m_worldTriangles[i].v0);
		maxPt = DirectX::SimpleMath::Vector3::Max(maxPt, m_worldTriangles[i].v1);
		maxPt = DirectX::SimpleMath::Vector3::Max(maxPt, m_worldTriangles[i].v2);
	}

	if (!m_worldTriangles.empty())
	{
		m_worldAABB.Center = (minPt + maxPt) * 0.5f;
		m_worldAABB.Extents = (maxPt - minPt) * 0.5f;
	}
}

void HEIN::MeshColliderComponent::Draw(
	GameContext& gameContext, 
	const DirectX::SimpleMath::Matrix& world, 
	const DirectX::SimpleMath::Matrix& view, 
	const DirectX::SimpleMath::Matrix& proj
)
{
	if (!gameContext.showColliders || gameContext.debugCollisionRenderer == nullptr) return;
	if (m_localTriangles.empty()) return;

	if (m_vertexBuffer == nullptr)
	{
		CreateGpuBuffers(gameContext.deviceResources.GetD3DDevice());
	}
	if (m_vertexBuffer == nullptr || m_indexBuffer == nullptr || m_indexCount == 0) return;

	DirectX::SimpleMath::Matrix worldMatrix = GetCalculateWorldMatrix();

	DirectX::SimpleMath::Color debugColor = DirectX::SimpleMath::Color(DirectX::Colors::Red);
	if (m_isCollidingThisFrame)
	{
		debugColor = DirectX::Colors::Yellow;
	}

	gameContext.debugCollisionRenderer->QueueMesh(
		m_vertexBuffer.Get(),
		m_indexBuffer.Get(),
		m_indexCount,
		worldMatrix,
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
        HWND windowHandle = gameContext.deviceResources.GetWindow();

        std::string narrowPath(m_objPath.begin(), m_objPath.end());
        char bufferPath[512];
        strncpy_s(bufferPath, narrowPath.c_str(), sizeof(bufferPath));
        if (ImGui::InputText("Obj Path", bufferPath, sizeof(bufferPath), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            std::string newPath(bufferPath);
            std::wstring newWPath(newPath.begin(), newPath.end());
            if (newWPath != m_objPath)
            {
                m_objPath = newWPath;
                if (!m_objPath.empty())
                {
                    LoadFromObj(m_objPath.c_str(), gameContext.deviceResources.GetD3DDevice());
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse##MeshObj"))
        {
            std::wstring file = HEIN::EditorUtils::OpenFileDialog(L"Wavefront OBJ (*.obj)\0*.obj\0All Files\0*.*\0", windowHandle);
            if (!file.empty())
            {
                m_objPath = HEIN::EditorUtils::MakeRelativePath(file);
                LoadFromObj(m_objPath.c_str(), gameContext.deviceResources.GetD3DDevice());
            }
        }

        if (m_localTriangles.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "No mesh loaded. Specify or browse a .obj file.");
        }
        else
        {
            ImGui::Text("Vertices: %zu | Triangles: %zu", m_debugVertices.size(), m_localTriangles.size());
        }
    }
}

bool HEIN::MeshColliderComponent::IntersectsFrustum(const DirectX::BoundingFrustum& frustum) const
{
    if (m_worldTriangles.empty()) return true;
    DirectX::BoundingBox box = m_worldAABB;
    box.Extents.x *= 1.2f;
    box.Extents.y *= 1.2f;
    box.Extents.z *= 1.2f;
    return frustum.Intersects(box);
}
