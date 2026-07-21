#pragma once
#include <SimpleMath.h>
#include "IComponent.h"
#include <DebugingTools/IGizmoEditable.h>

namespace HEIN
{
    class TransformComponent : public IComponent, public IGizmoEditable
    {
    private:
        DirectX::SimpleMath::Vector3 m_position;
        DirectX::SimpleMath::Quaternion m_rotation;
        DirectX::SimpleMath::Vector3 m_scale;
        DirectX::SimpleMath::Matrix m_parentMatrix;
    public:

        TransformComponent(Actor* owner);

        void Update(float deltaTime) override;

        void OnInspectorGUI() override;

        void DrawGizmo(
            const DirectX::SimpleMath::Matrix& view,
            const DirectX::SimpleMath::Matrix& proj,
            int operation,
            int mode
        ) override;
        // --- Getters & Setters ---

        void SetPosition(const DirectX::SimpleMath::Vector3& pos) { m_position = pos; }
        const DirectX::SimpleMath::Vector3& GetPosition() const { return m_position; }

        void SetRotation(const DirectX::SimpleMath::Quaternion& rot) { m_rotation = rot; }
        const DirectX::SimpleMath::Quaternion& GetRotation() const { return m_rotation; }

        void SetScale(const DirectX::SimpleMath::Vector3& scale) { m_scale = scale; }
        const DirectX::SimpleMath::Vector3& GetScale() const { return m_scale; }

        DirectX::SimpleMath::Vector3 GetForward() const
        {
            return DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Forward, m_rotation);
        }
   
        void SetRotationEuler(const DirectX::SimpleMath::Vector3& eulerAngles);
        DirectX::SimpleMath::Vector3 GetRotationEuler() const;

        void SetParentMatrix(const DirectX::SimpleMath::Matrix& parent) { m_parentMatrix = parent; }

        // --- Core 3D Math ---

        // Generates the World Matrix for DirectX 11 rendering
        DirectX::SimpleMath::Matrix GetWorldMatrix() const;
     

    };
}