#pragma once
#include "Components/IComponent.h"
#include "Framework/GameContext.h"
#include "pch.h"

namespace HEIN
{
	class StaticModelComponent : public IComponent
	{
	private:

		std::unique_ptr<DirectX::Model> m_model;
		std::unique_ptr<DirectX::EffectFactory> m_fxFactory;
		bool m_isVisible = true;
    public:
		std::wstring m_modelPath;
		std::wstring m_textureDir;

		std::string GetComponentName() const override { return "StaticModelComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void InitializeAfterDeserialize(GameContext& gameContext) override;

        StaticModelComponent(Actor* owner);

        void Initialize(
            GameContext& gameContext,
            const wchar_t* modelPath,
            const wchar_t* textureDir
        );
      
        void Update(float /*deltaTime*/) override;

        void Draw(
            GameContext& gameContext, 
            const DirectX::SimpleMath::Matrix& world, 
            const DirectX::SimpleMath::Matrix& view, 
            const DirectX::SimpleMath::Matrix& proj
        );


        DirectX::BoundingBox GetBoundingBox() const;
        DirectX::BoundingSphere GetBoundingSphere() const;

        void SetVisible(bool visible) { m_isVisible = visible; }
        bool IsVisible() const { return m_isVisible; }
	};
}
