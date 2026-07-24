#pragma once
#include <Components/IComponent.h>
#include <string>
#include <Entities/Actor.h>

namespace HEIN
{
	class SocketComponent;
	class ActorManager;

	class SocketAttachmentComponent : public IComponent
	{
	private:

		HEIN::ActorManager* m_manager;
		HEIN::ActorID m_targetActorID;
		std::wstring m_socketName;

	public:
		std::string GetComponentName() const override { return "SocketAttachmentComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;


		SocketAttachmentComponent(Actor* owner, ActorManager* manager);

		void Initialize(HEIN::ActorID targetActorID, const std::wstring& socketName);

		void Start() override {}
		void Update(float /*deltaTime*/) override {}
		void LateUpdate(float deltaTime) override;
		void Draw(
			GameContext& /*gameContext*/,
			const DirectX::SimpleMath::Matrix& /*world*/,
			const DirectX::SimpleMath::Matrix& /*view*/,
			const DirectX::SimpleMath::Matrix& /*proj*/
			) override {}

	};

}
