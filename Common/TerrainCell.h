#pragma once

namespace HEIN
{
	struct TerrainVertexType
	{
		DirectX::SimpleMath::Vector3 position;
		DirectX::SimpleMath::Vector2 texture;
		DirectX::SimpleMath::Vector3 normal;
		DirectX::SimpleMath::Vector3 tangent;
		DirectX::SimpleMath::Vector3 binormal;
		DirectX::SimpleMath::Vector4 color;
	};

	class TerrainCell
	{
	private:

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;

		int m_vertexCount;
		int m_indexCount;

		DirectX::BoundingBox m_boundingBox;

	public:

		TerrainCell();
		~TerrainCell() = default;

		bool Initialize(ID3D11Device* device, void* vertexData, int vertexCount, void* indexData, int indexCount);
		void Draw(ID3D11DeviceContext* context);

		DirectX::BoundingBox GetBoundingBox() const { return m_boundingBox; }
		int GetVertexCount() const { return m_vertexCount; }
		int GetIndexCount() const { return m_indexCount; }

	};
}
