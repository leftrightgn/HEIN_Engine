#pragma once
#include <Common/json.hpp>

struct GameContext;

namespace HEIN
{
	class Actor;

	/**
	 * @brief Interface for all components attached to an Actor.
	 * 
	 * Provides lifecycle methods such as Update, Start, Draw, and serialization.
	 */
	class IComponent
	{
	protected:
		/** @brief Pointer to the Actor that owns this component. */
		Actor* m_owner;

	public:
		/**
		 * @brief Constructs the component with an owner.
		 * @param owner The Actor this component is attached to.
		 */
		IComponent(Actor* owner)
			: m_owner(owner)
		{
		}

		/** @brief Virtual destructor. */
		virtual ~IComponent() = default;

		/**
		 * @brief Updates the component.
		 * @param deltaTime The time elapsed since the last frame.
		 */
		virtual void Update(float deltaTime) = 0;

		/**
		 * @brief Late update called after all normal updates.
		 * @param deltaTime The time elapsed since the last frame.
		 */
		virtual void LateUpdate(float /*deltaTime*/) {}

		/** @brief Called when the component starts. */
		virtual void Start() {}

		/**
		 * @brief Draws the component (3D).
		 * @param gameContext The current game context.
		 * @param world The world matrix.
		 * @param view The view matrix.
		 * @param proj The projection matrix.
		 */
		virtual void Draw(
			GameContext& /*gameContext*/, 
			const DirectX::SimpleMath::Matrix& /*world*/, 
			const DirectX::SimpleMath::Matrix& /*view*/, 
			const DirectX::SimpleMath::Matrix& /*proj*/
		) {}

		virtual void DrawShadow(
			GameContext& gameContext, 
			const DirectX::SimpleMath::Matrix& lightViewProj
		) {}
		/** @brief Checks if this component requires 2D drawing. */
		virtual bool Is2D() const { return false; }
		
		/**
		 * @brief Draws the component (2D).
		 * @param gameContext The current game context.
		 */
		virtual void Draw2D(GameContext& /*gameContext*/) {}

		/**
		 * @brief Draws properties in the inspector GUI.
		 * @param gameContext The current game context.
		 */
		virtual void OnInspectorGUI(GameContext& gamecontext) {}

		/** @brief Gets the name of the component. */
		virtual std::string GetComponentName() const { return "Unknown"; }

		/** @brief Serializes the component data to JSON. */
		virtual nlohmann::json Serialize() { return nlohmann::json(); }

		/** @brief Deserializes the component data from JSON. */
		virtual void Deserialize(const nlohmann::json& data) {}

		/** @brief Initializes the component after deserialization. */
		virtual void InitializeAfterDeserialize(GameContext& gameContext) {}

		/** @brief Gets the owner Actor. */
		Actor* GetOwner() const { return m_owner; }
	};
}
