#pragma once
#include <Components/IComponent.h>
#include <SimpleMath.h>
#include <string>
#include <vector>

namespace HEIN
{
    class ActorManager;
    class SkinnedModelComponent;
    class TerrainColliderComponent;
    class MeshColliderComponent;

    struct IKChain
    {
        std::wstring effectorBoneName;
        std::wstring midBoneName;
        std::wstring rootBoneName;
        std::wstring toeBoneName;
        float weight = 1.0f;
        float heightOffset = 0.0f;
        bool isFoot = false;
    };

    class ProceduralAnimationComponent : public IComponent
    {
    private:
        ActorManager* m_actorManager = nullptr;
        SkinnedModelComponent* m_skinnedModel = nullptr;
        TerrainColliderComponent* m_terrain = nullptr;
        MeshColliderComponent* m_meshCollider = nullptr;

        bool m_enabled = true;
        float m_globalIKWeight = 1.0f;

        std::vector<IKChain> m_ikChains;
        
    public:
        ProceduralAnimationComponent(Actor* owner, ActorManager* manager = nullptr);
        ~ProceduralAnimationComponent() = default;
        
        std::string GetComponentName() const override { return "ProceduralAnimationComponent"; }

        nlohmann::json Serialize() override;
        void Deserialize(const nlohmann::json& data) override;
        void OnInspectorGUI(GameContext& gameContext) override;

        void Start() override;
        void Update(float deltaTime) override;
        void LateUpdate(float deltaTime) override;
        void Draw(GameContext& gameContext, const DirectX::SimpleMath::Matrix& world, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj) override;

    private:
        struct DebugLine
        {
            DirectX::SimpleMath::Vector3 start;
            DirectX::SimpleMath::Vector3 end;
            DirectX::SimpleMath::Color color;
        };
        std::vector<DebugLine> m_debugLines;

        void SolveWholeBodyIK();
        void SolveTwoBoneIK(
            DirectX::SimpleMath::Matrix* localBones,
            int rootIdx, int midIdx, int effectorIdx, int toeIdx,
            const DirectX::SimpleMath::Matrix& worldMatrix,
            float weight, float heightOffset, bool alignToTerrain);
    };
}
