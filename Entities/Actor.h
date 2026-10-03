#pragma once
#include <vector>
#include <memory>
#include <string>
#include <utility>
#include <algorithm>
#include <cstdint>
#include "../Components/IComponent.h"
#include <Common/json.hpp>

struct GameContext;

namespace HEIN
{
	enum class ActorType
	{
		Default = 0,
		Player = 1,
		Enemy = 2,
		Items = 3,
		Enviroment = 4
	};

	using ActorID = uint32_t;
	constexpr ActorID INVALID_ACTOR_ID = 0;

	class IComponent;
	class ActorManager;
	class SkinnedModelComponent;

	/// <summary>
	/// Represents an entity in the game world using a handle-based architecture.
	/// Actors are organized in a parent-child scene graph and do not hold direct pointers to each other.
	/// Relying on ActorIDs prevents dangling references and provides memory stability.
	/// </summary>
	class Actor
	{
	private:

		ActorID m_id;
		ActorType m_type;
		ActorID m_ownerID = INVALID_ACTOR_ID;
		ActorID m_parentID = INVALID_ACTOR_ID;
		std::vector<ActorID> m_childrensID;

		// Active SkinnedModel tracking for toggling between models (e.g. root motion vs non-root motion)
		int m_activeSkinnedModelIndex = 0;

		// Unity-style Visibility and Active state
		bool m_isVisible = true;
		bool m_isActive = true;

		// Memory safe Array of Components
		std::vector<std::unique_ptr<HEIN::IComponent>> m_components;
		std::wstring m_tag;

	public:

		Actor(ActorID id, const std::wstring& tag = L"Actor");

		~Actor() = default;

		void Update(float deltaTime);

		void LateUpdate(float deltaTime);

		void Draw(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		);

		void Draw2D(GameContext& gameContext);

		void Start();

		void DrawInspector(GameContext& gameContext);

		nlohmann::json Serialize(ActorManager* manager = nullptr);
		void Deserialize(const nlohmann::json& actorData, ActorManager* manager = nullptr);
		void InitializeAfterDeserialize(GameContext& gameContext);

		ActorID GetID() const { return m_id; }
		std::wstring GetTag() const { return m_tag; }
		void SetTag(const std::wstring& tag) { m_tag = tag; }

		void SetParent(ActorID id) { m_parentID = id; }
		ActorID GetParentID() const { return m_parentID; }
		
		void SetOwnerID(ActorID id) { m_ownerID = id; }
		ActorID GetOwnerID() const { return m_ownerID; }

		void SetActorType(ActorType type) { m_type = type; }
		ActorType GetActorType() const { return m_type; }

		// Unity-style Visibility & Active controls
		bool IsVisible() const { return m_isVisible; }
		void SetVisible(bool visible) { m_isVisible = visible; }
		bool IsActive() const { return m_isActive; }
		void SetActive(bool active) { m_isActive = active; }

		/// <summary>
		/// Adds a child to this actor's scene graph hierarchy.
		/// Used during CascadeTransforms to recursively calculate the world matrix:
		/// M_world = M_local * M_parent_world.
		/// </summary>
		void AddChild(ActorID id) { m_childrensID.push_back(id); }
		void RemoveChild(ActorID id)
		{
			m_childrensID.erase(
				std::remove(m_childrensID.begin(), m_childrensID.end(), id),
				m_childrensID.end()
			);
		}
		const std::vector<ActorID>& GetChildren() const { return m_childrensID; }
		
		// Template  Components
		// Creates a component, adds it to the Actor, and returns a pointer to it
		template <typename T, typename... TArgs>
		T* AddComponent(TArgs&&... mArgs)
		{
			// Create the new component, passing 'this' as the owner, plus any other arguments
			std::unique_ptr<T> newComponent = std::make_unique<T>(this, std::forward<TArgs>(mArgs)...);
			T* result = newComponent.get();

			m_components.push_back(std::move(newComponent));
			return result;
		}

		// Searches the Actor for a specific component type
		template <typename T>
		T* GetComponent()
		{
			for (std::unique_ptr<HEIN::IComponent>& comp : m_components)
			{
				T* target = dynamic_cast<T*>(comp.get());
				if (target != nullptr)
				{
					return target;
				}
			}
			return nullptr;
		}

		void RemoveComponent(HEIN::IComponent* componentToRemove)
		{
			m_components.erase(
				std::remove_if(m_components.begin(), m_components.end(),
					[componentToRemove](const std::unique_ptr<HEIN::IComponent>& comp) {
						return comp.get() == componentToRemove;
					}),
				m_components.end()
			);
		}

		template <typename T>
		std::vector<T*> GetComponents()
		{
			std::vector<T*> result;
			for (std::unique_ptr<HEIN::IComponent>& comp : m_components)
			{
				T* target = dynamic_cast<T*>(comp.get());
				if (target != nullptr)
				{
					result.push_back(target);
				}
			}
			return result;
		}

		// Skinned Model Management (Support switching between multiple models, e.g. with and without root motion)
		HEIN::SkinnedModelComponent* GetActiveSkinnedModel();
		int GetActiveSkinnedModelIndex() const { return m_activeSkinnedModelIndex; }
		void SetActiveSkinnedModelIndex(int index);
		void ToggleSkinnedModel();

	};

	// Explicit specialization so GetComponent<SkinnedModelComponent>() returns the active / visible model
	template <>
	HEIN::SkinnedModelComponent* Actor::GetComponent<HEIN::SkinnedModelComponent>();

}