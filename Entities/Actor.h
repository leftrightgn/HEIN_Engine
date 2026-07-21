#pragma once
#include <vector>
#include <memory>
#include <string>
#include <utility>
#include <algorithm>
#include <cstdint>

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

	class Actor
	{
	private:

		ActorID m_id;
		ActorType m_type;
		ActorID m_ownerID = INVALID_ACTOR_ID;
		ActorID m_parentID = INVALID_ACTOR_ID;
		std::vector<ActorID> m_childrensID;

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

		void Start();

		void DrawInspector();

		ActorID GetID() const { return m_id; }
		std::wstring GetTag() const { return m_tag; }

		void SetParent(ActorID id) { m_parentID = id; }
		ActorID GetParentID() const { return m_parentID; }
		
		void SetOwnerID(ActorID id) { m_ownerID = id; }
		ActorID GetOwnerID() const { return m_ownerID; }

		void SetActorType(ActorType type) { m_type = type; }
		ActorType GetActorType() const { return m_type; }

		void AddChild(ActorID id) { m_childrensID.push_back(id); }
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

	};

}