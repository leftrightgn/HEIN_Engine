#pragma once
#include "IComponent.h"
#include <DebugingTools/IGizmoEditable.h>

namespace HEIN
{
    class SkinnedModelComponent;
    class TransformComponent;
    // A struct to define a single socket attachment point
    struct Socket
    {
        std::wstring name;
        std::wstring boneName;
        DirectX::SimpleMath::Vector3 localPosition;
        DirectX::SimpleMath::Vector3 localRotation; // X=Pitch, Y=Yaw, Z=Roll

        Socket()
            : name(L""), boneName(L"")
            , localPosition(DirectX::SimpleMath::Vector3::Zero)
            , localRotation(DirectX::SimpleMath::Vector3::Zero)
        {
        }

        Socket(const std::wstring& n, const std::wstring& bName,
            const DirectX::SimpleMath::Vector3& pos = DirectX::SimpleMath::Vector3::Zero,
            const DirectX::SimpleMath::Vector3& rot = DirectX::SimpleMath::Vector3::Zero)
            : name(n), boneName(bName), localPosition(pos), localRotation(rot)
        {
        }
    };

    class SocketComponent : public IComponent, public IGizmoEditable
    {
    private:
        std::unordered_map<std::wstring, Socket> m_sockets;

        SkinnedModelComponent* m_model;
        TransformComponent* m_transform;

    public:
		std::string GetComponentName() const override { return "SocketComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;

        SocketComponent(Actor* owner);

        void Start() override;

        void Update(float /*deltaTime*/) override {}

        void UpdateSocketOffset(
            const std::wstring& socketName,
            const DirectX::SimpleMath::Vector3& newPos,
            const DirectX::SimpleMath::Vector3& newRot
        );

        void OnInspectorGUI(GameContext& gameContext) override;

        void DrawGizmo(
            const DirectX::SimpleMath::Matrix& view,
            const DirectX::SimpleMath::Matrix& proj,
            int operation,
            int mode
        ) override;

        void AddSocket(const Socket& socket);

        bool HasSocket(const std::wstring& socketName) const;

        Socket* GetSocket(const std::wstring& socketName);
      
        DirectX::SimpleMath::Matrix GetSocketWorldMatrix(const std::wstring& socketName);
        
    };
}