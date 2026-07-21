#pragma once

struct GameContext;

namespace HEIN
{
	class Actor;

	class IComponent
	{
	protected:

		Actor* m_owner;

	public:

		IComponent(Actor* owner)
			: m_owner(owner)
		{
		}

		virtual ~IComponent() = default;

		virtual void Update(float deltaTime) = 0;

		virtual void LateUpdate(float /*deltaTime*/) {}

		virtual void Start() {}

		virtual void Draw(
			GameContext& /*gameContext*/, 
			const DirectX::SimpleMath::Matrix& /*world*/, 
			const DirectX::SimpleMath::Matrix& /*view*/, 
			const DirectX::SimpleMath::Matrix& /*proj*/
		) {}

		virtual void OnInspectorGUI() {}

		Actor* GetOwner() const { return m_owner; }
	};
}
