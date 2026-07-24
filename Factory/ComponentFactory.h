#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <Entities/Actor.h>
#include <Entities/ActorManager.h>
#include <Components/TransformComponent.h>
#include <Components/DamageDealerComponent.h>
#include <Components/HealthComponent.h>
#include <Camera/CameraController.h>
#include <Components/RigidBodyComponent.h>
#include <Components/SkinnedModelComponent.h>
#include <Components/TargetTrackingComponent.h>
#include <Components/StaticModelComponent.h>
#include <Components/BoneLinkComponent.h>
#include <Components/SocketComponent.h>
#include <Components/SocketAttachmentComponent.h>
#include <Components/TwoBoneLinkComponent.h>
#include <Components/ColliderComponent/AABBColliderComponent.h>
#include <Components/ColliderComponent/CapsuleColliderComponent.h>
#include <Components/ColliderComponent/MeshColliderComponent.h>
#include <Components/ColliderComponent/OBBColliderComponent.h>
#include <Components/ColliderComponent/SphereColliderComponent.h>

namespace HEIN
{
	class ComponentFactory
	{
	private:

		// for the map to link the sting
		using CreatorFunc = std::function<HEIN::IComponent*(HEIN::Actor*, HEIN::ActorManager*)>;
		
		inline static std::unordered_map<std::string, CreatorFunc> m_creators;

	public:

		template <typename T>
		static void RegisterComponent(const std::string& name)
		{
			m_creators[name] = [](HEIN::Actor* owner, HEIN::ActorManager* manager) -> HEIN::IComponent* {
				if constexpr (std::is_constructible_v<T, HEIN::Actor*, HEIN::ActorManager*>)
				{
					return owner->AddComponent<T>(manager);
				}
				else
				{
					return owner->AddComponent<T>();
				}
			};
		}

		static HEIN::IComponent* CreateComponent(const std::string& name, HEIN::Actor* owner, HEIN::ActorManager* manager = nullptr)
		{
			if (m_creators.empty()) Initialize();
			auto it = m_creators.find(name);
			if (it != m_creators.end())
			{
				return it->second(owner, manager);
			}
			return nullptr;
		}

		static std::vector<std::string> GetRegisteredComponentNames()
		{
			if (m_creators.empty()) Initialize();
			std::vector<std::string> names;
			for (const auto& pair : m_creators)
			{
				names.push_back(pair.first);
			}
			return names;
		}

		static void Initialize()
		{
			RegisterComponent<TransformComponent>("TransformComponent");
			RegisterComponent<DamageDealerComponent>("DamageDealerComponent");
			RegisterComponent<HealthComponent>("HealthComponent");
			RegisterComponent<RigidBodyComponent>("RigidBodyComponent");
			RegisterComponent<SkinnedModelComponent>("SkinnedModelComponent");
			RegisterComponent<TargetTrackingComponent>("TargetTrackingComponent");
			RegisterComponent<StaticModelComponent>("StaticModelComponent");
			RegisterComponent<BoneLinkComponent>("BoneLinkComponent");
			RegisterComponent<SocketComponent>("SocketComponent");
			RegisterComponent<SocketAttachmentComponent>("SocketAttachmentComponent");
			RegisterComponent<TwoBoneLinkComponent>("TwoBoneLinkComponent");
			RegisterComponent<CameraController>("CameraController");
			RegisterComponent<AABBColliderComponent>("AABBColliderComponent");
			RegisterComponent<CapsuleColliderComponent>("CapsuleColliderComponent");
			RegisterComponent<MeshColliderComponent>("MeshColliderComponent");
			RegisterComponent<OBBColliderComponent>("OBBColliderComponent");
			RegisterComponent<SphereColliderComponent>("SphereColliderComponent");
		}
	};
}

