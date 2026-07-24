#include "pch.h"
#include "DebugUIManager.h"
#include "Components/SocketComponent.h"
#include <Components/ColliderComponent/CapsuleColliderComponent.h>
#include "Components/ColliderComponent/OBBColliderComponent.h"
#include "ImGui/imgui.h" 
#include <ImGui/ImGuizmo.h>
#include <Components/ColliderComponent/AABBColliderComponent.h>
#include <Components/ColliderComponent/SphereColliderComponent.h>
#include <Entities/ActorManager.h>
#include <Components/ColliderComponent/MeshColliderComponent.h>
#include <Factory/ComponentFactory.h>
//#include <Components/TransformComponent.h>

namespace HEIN
{
	IGizmoEditable* g_ActiveGizmoTarget = nullptr;

	void DebugUIManager::Update(
		const GameContext& gameContext,
		HEIN::ActorManager& actorManager,
		const DirectX::SimpleMath::Matrix& view,
		const DirectX::SimpleMath::Matrix& proj
	)
	{
		if (gameContext.keyboardTracker.pressed.F1)
		{
			m_isVisible = !m_isVisible;
		}

		if (!m_isVisible) return;

		bool isShiftHeld = gameContext.keyboardState.LeftShift || gameContext.keyboardState.RightShift;

		if (isShiftHeld)
		{
			if (gameContext.keyboardTracker.pressed.W) m_currentGinzmoOperation = ImGuizmo::TRANSLATE;
			if (gameContext.keyboardTracker.pressed.E) m_currentGinzmoOperation = ImGuizmo::ROTATE;
			if (gameContext.keyboardTracker.pressed.R) m_currentGinzmoOperation = ImGuizmo::SCALE;
		}
		if (gameContext.mouseButtonTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED &&
			!ImGui::GetIO().WantCaptureMouse)
		{
			double currentTime = gameContext.timer.GetTotalSeconds();
			if ((currentTime - m_lastleftClickTime) < DOUBLE_CLICK_THRESHOLD)
			{
				float mouseX = static_cast<float>(gameContext.mouseState.x);
				float mouseY = static_cast<float>(gameContext.mouseState.y);

				RECT size = gameContext.deviceResources.GetOutputSize();
				float screenWidth = static_cast<float>(size.right - size.left);
				float screenHeight = static_cast<float>(size.bottom - size.top);

				DirectX::SimpleMath::Viewport viewport(0.0f, 0.0f, screenWidth, screenHeight);
				DirectX::SimpleMath::Matrix world = DirectX::SimpleMath::Matrix::Identity;

				// Unproject screen coordinates to 3D space
				DirectX::SimpleMath::Vector3 rayOrigin = viewport.Unproject(DirectX::SimpleMath::Vector3(mouseX, mouseY, 0.0f), proj, view, world);
				DirectX::SimpleMath::Vector3 rayTarget = viewport.Unproject(DirectX::SimpleMath::Vector3(mouseX, mouseY, 1.0f), proj, view, world);

				DirectX::SimpleMath::Vector3 rayDir = rayTarget - rayOrigin;
				rayDir.Normalize();

				float closestHit = FLT_MAX;
				HEIN::Actor* hitActor = nullptr;

				for (const std::pair<const HEIN::ActorID, std::unique_ptr<HEIN::Actor>>& pair : actorManager.GetAllActors())
				{
					HEIN::Actor* actor = pair.second.get();
					float actorClosestHit = FLT_MAX;
					bool hit = false;

					std::vector<HEIN::OBBColliderComponent*> obbs = actor->GetComponents<HEIN::OBBColliderComponent>();
					for (auto* obb : obbs)
					{
						float d = 0.0f;
						if (obb->GetWorldOBB().Intersects(rayOrigin, rayDir, d)) { hit = true; if (d < actorClosestHit) actorClosestHit = d; }
					}

					std::vector<HEIN::AABBColliderComponent*> aabbs = actor->GetComponents<HEIN::AABBColliderComponent>();
					for (auto* aabb : aabbs)
					{
						float d = 0.0f;
						if (aabb->GetWorldAABB().Intersects(rayOrigin, rayDir, d)) { hit = true; if (d < actorClosestHit) actorClosestHit = d; }
					}

					std::vector<HEIN::SphereColliderComponent*> spheres = actor->GetComponents<HEIN::SphereColliderComponent>();
					for (auto* sphere : spheres)
					{
						float d = 0.0f;
						if (sphere->GetWorldSphere().Intersects(rayOrigin, rayDir, d)) { hit = true; if (d < actorClosestHit) actorClosestHit = d; }
					}

					std::vector<HEIN::CapsuleColliderComponent*> capsules = actor->GetComponents<HEIN::CapsuleColliderComponent>();
					for (auto* capsule : capsules)
					{
						DirectX::SimpleMath::Vector3 top = capsule->GetWorldTopCenter();
						DirectX::SimpleMath::Vector3 bottom = capsule->GetWorldBottomCenter();
						DirectX::SimpleMath::Vector3 center = (top + bottom) * 0.5f;
						float halfHeight = (top - bottom).Length() * 0.5f;
						DirectX::BoundingSphere bound(center, halfHeight + capsule->GetRadius());

						float d = 0.0f;
						if (bound.Intersects(rayOrigin, rayDir, d)) { hit = true; if (d < actorClosestHit) actorClosestHit = d; }
					}

					std::vector<HEIN::MeshColliderComponent*> meshes = actor->GetComponents<HEIN::MeshColliderComponent>();
					for (auto* mesh : meshes)
					{
						const std::vector<HEIN::Triangle>& triangles = mesh->GetWorldTriangles();

						for (const HEIN::Triangle& tri : triangles)
						{
							float d = 0.0f;
							DirectX::SimpleMath::Vector3 normal;

							if (HEIN::CollisionMath::IntersectRayTriangle(rayOrigin, rayDir, tri, d, normal))
							{
								hit = true;
								if (d < actorClosestHit) actorClosestHit = d;
							}
						}
					}

					if (hit && actorClosestHit < closestHit)
					{
						closestHit = actorClosestHit;
						hitActor = actor;
					}


				}
				if (hitActor != m_selectedActor)
				{
					m_selectedActor = hitActor;
					
					g_ActiveGizmoTarget = nullptr;
				}

				m_lastleftClickTime = -1.0;
			}
			else
			{
				m_lastleftClickTime = currentTime;
			}
		}
	}
	EditorAction DebugUIManager::Draw(
		HEIN::ActorManager& manager,
		const DirectX::SimpleMath::Matrix& view,
		const DirectX::SimpleMath::Matrix& proj
	)
	{
		HEIN::EditorAction currentAction = HEIN::EditorAction::None;

		if (!m_isVisible) return currentAction;

		ImGui::Begin("ToolBar");
		if (ImGui::Button("PLAY")) currentAction = HEIN::EditorAction::PlayPressed;
		ImGui::SameLine();
		if (ImGui::Button("STOP")) 	currentAction = HEIN::EditorAction::StopPressed;
		ImGui::SameLine();
		if (ImGui::Button("SAVE SCENE")) currentAction = HEIN::EditorAction::SavePressed;
		ImGui::SameLine();
		if (ImGui::Button("LOAD SCENE")) currentAction = HEIN::EditorAction::LoadPressed;
		ImGui::Separator();
		if (ImGui::Button("Create Empty Actor"))
		{
			HEIN::Actor* newActor = manager.CreateActor(L"NewActor");
			m_selectedActor = newActor;
		}
		ImGui::End();

		// DRAW THE INSPECTOR WINDOW
		ImGui::Begin("Inspector");

		if (m_selectedActor == nullptr || manager.GetActor(m_selectedActor->GetID()) == nullptr)
		{
			m_selectedActor = nullptr;
			ImGui::TextDisabled("No Actor Selected");
		}
		else
		{
			std::wstring wtag = m_selectedActor->GetTag();
			std::string narrowTag(wtag.begin(), wtag.end());
			
			char nameBuffer[256];
			strncpy_s(nameBuffer, narrowTag.c_str(), sizeof(nameBuffer));
			if (ImGui::InputText("Actor Name", nameBuffer, sizeof(nameBuffer)))
			{
				std::string newTag(nameBuffer);
				std::wstring newWTag(newTag.begin(), newTag.end());
				m_selectedActor->SetTag(newWTag);
			}
			
			ImGui::Separator();

			// Draw gizmo mode toggles
			if (ImGui::RadioButton("Local", m_currentGinzmo == ImGuizmo::LOCAL)) m_currentGinzmo = ImGuizmo::LOCAL;
			ImGui::SameLine();
			if (ImGui::RadioButton("World", m_currentGinzmo == ImGuizmo::WORLD)) m_currentGinzmo = ImGuizmo::WORLD;

			ImGui::Separator();

			m_selectedActor->DrawInspector();

			ImGui::Separator();
			
			// Add Component UI
			std::vector<std::string> componentNames = HEIN::ComponentFactory::GetRegisteredComponentNames();
			if (ImGui::BeginCombo("Add Component", "Select Component..."))
			{
				for (const std::string& compName : componentNames)
				{
					if (ImGui::Selectable(compName.c_str()))
					{
						HEIN::ComponentFactory::CreateComponent(compName, m_selectedActor, &manager);
					}
				}
				ImGui::EndCombo();
			}

			if (g_ActiveGizmoTarget != nullptr)
			{
				g_ActiveGizmoTarget->DrawGizmo(view, proj, m_currentGinzmoOperation, m_currentGinzmo);
			}

		}
		ImGui::End();

		return currentAction;
	}
}