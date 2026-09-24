#pragma once
#include <Components/IComponent.h>
#include <SimpleMath.h>
#include <string>

namespace HEIN
{
    class SkinnedModelComponent;
    class TerrainColliderComponent;
    class ActorManager;

    class FootIKComponent : public IComponent
    {
    private:
        SkinnedModelComponent* m_skinnedModel = nullptr;
        TerrainColliderComponent* m_terrain = nullptr;

        std::wstring m_leftThighName;
        std::wstring m_leftCalfName;
        std::wstring m_leftFootName;

        std::wstring m_rightThighName;
        std::wstring m_rightCalfName;
        std::wstring m_rightFootName;

        float m_footOffset = 0.1f; // Height from ankle to bottom of foot
        float m_ikBlendWeight = 1.0f; // 0.0 to 1.0 for fading IK in/out
        ActorManager* m_actorManager = nullptr;

    public:
        FootIKComponent(Actor* owner, ActorManager* manager = nullptr);
        ~FootIKComponent() = default;

        void Initialize(
            const std::wstring& lThigh = L"mixamorig:LeftUpLeg",
            const std::wstring& lCalf = L"mixamorig:LeftLeg",
            const std::wstring& lFoot = L"mixamorig:LeftFoot",
            const std::wstring& rThigh = L"mixamorig:RightUpLeg",
            const std::wstring& rCalf = L"mixamorig:RightLeg",
            const std::wstring& rFoot = L"mixamorig:RightFoot"
        );

        void SetTerrain(TerrainColliderComponent* terrain) { m_terrain = terrain; }

        std::string GetComponentName() const override { return "FootIKComponent"; }
        nlohmann::json Serialize() override;
        void Deserialize(const nlohmann::json& data) override;
        void OnInspectorGUI(GameContext& gameContext) override;

        void Start() override;
        void Update(float deltaTime) override {}
        void LateUpdate(float deltaTime) override;

    private:
        void SolveTwoBoneIK(
            DirectX::SimpleMath::Matrix* localBones,
            int thighIdx, int calfIdx, int footIdx,
            const DirectX::SimpleMath::Matrix& worldMatrix);
    };
}