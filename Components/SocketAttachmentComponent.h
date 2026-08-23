#pragma once
#include <Components/IComponent.h>
#include <string>
#include <Entities/Actor.h>

namespace HEIN
{
	class SocketComponent;
	class ActorManager;

	/// <summary>
	/// Locks an actor's transform (like a weapon) to a specific socket attachment point on a skeletal mesh.
	/// Ensures weapons perfectly track hand sockets by cascading matrices:
	/// M_weapon_world = M_socket_offset * M_hand_bone * M_character_world
	/// Updated in Phase C (LateUpdate) to guarantee frame-accurate sync with animation updates.
	/// </summary>
	class SocketAttachmentComponent : public IComponent
	{
	private:

		HEIN::ActorManager* m_actorManager;
		ActorID m_socketOwnerActorID;
		std::wstring m_socketOwnerTag;
		std::wstring m_socketName;

	public:
		std::string GetComponentName() const override { return "SocketAttachmentComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;


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
