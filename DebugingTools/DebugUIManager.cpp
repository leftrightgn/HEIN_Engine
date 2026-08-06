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
#include <Components/UIButtonComponent.h>
#include <Windows.h>
#include <vector>
#include <Components/TransformComponent.h>
#include <Camera/CameraController.h>

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

		// Clean up dangling selection if actor was destroyed elsewhere
		if (m_selectedActor != nullptr && !actorManager.HasActor(m_selectedActor->GetID()))
		{
			m_selectedActor = nullptr;
			g_ActiveGizmoTarget = nullptr;
		}

		// Delete shortcut key
		if (m_selectedActor != nullptr && !ImGui::GetIO().WantCaptureKeyboard)
		{
			if (gameContext.keyboardTracker.pressed.Delete)
			{
				ActorID idToDelete = m_selectedActor->GetID();
				m_selectedActor = nullptr;
				g_ActiveGizmoTarget = nullptr;
				actorManager.DestroyID(idToDelete);
			}
		}

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

					// 3D Picking for Camera Component
					auto* camComp = actor->GetComponent<HEIN::CameraController>();
					if (camComp != nullptr)
					{
						DirectX::BoundingSphere camSphere(camComp->GetPosition(), 1.5f);
						float d = 0.0f;
						if (camSphere.Intersects(rayOrigin, rayDir, d))
						{
							hit = true;
							if (d < actorClosestHit) actorClosestHit = d;
						}
					}

					// 2D Screen Picking for UI Button Component
					auto* uiBtn = actor->GetComponent<HEIN::UIButtonComponent>();
					if (uiBtn != nullptr)
					{
						DirectX::SimpleMath::Vector2 bPos = uiBtn->GetPosition();
						DirectX::SimpleMath::Vector2 bSize = uiBtn->GetSize();
						if (mouseX >= bPos.x && mouseX <= bPos.x + bSize.x &&
							mouseY >= bPos.y && mouseY <= bPos.y + bSize.y)
						{
							hit = true;
							actorClosestHit = -1.0f; // UI always takes top priority over 3D geometry
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
					
					if (m_selectedActor != nullptr)
					{
						if (auto* cam = m_selectedActor->GetComponent<HEIN::CameraController>())
						{
							g_ActiveGizmoTarget = cam;
						}
						else if (auto* btn = m_selectedActor->GetComponent<HEIN::UIButtonComponent>())
						{
							g_ActiveGizmoTarget = btn;
						}
						else if (auto* trans = m_selectedActor->GetComponent<HEIN::TransformComponent>())
						{
							g_ActiveGizmoTarget = trans;
						}
						else
						{
							g_ActiveGizmoTarget = nullptr;
						}
					}
					else
					{
						g_ActiveGizmoTarget = nullptr;
					}
				}

				m_lastleftClickTime = -1.0;
			}
			else
			{
				m_lastleftClickTime = currentTime;
			}
		}
	}
	void DebugUIManager::SelectActor(HEIN::Actor* actor)
	{
		m_selectedActor = actor;
		if (actor == nullptr)
		{
			g_ActiveGizmoTarget = nullptr;
			return;
		}
		if (auto* cam = actor->GetComponent<HEIN::CameraController>())
		{
			g_ActiveGizmoTarget = cam;
		}
		else if (auto* btn = actor->GetComponent<HEIN::UIButtonComponent>())
		{
			g_ActiveGizmoTarget = btn;
		}
		else if (auto* trans = actor->GetComponent<HEIN::TransformComponent>())
		{
			g_ActiveGizmoTarget = trans;
		}
		else
		{
			g_ActiveGizmoTarget = nullptr;
		}
	}

	void DebugUIManager::DrawActorTreeNode(
		HEIN::Actor* actor,
		HEIN::ActorManager& manager,
		GameContext& gameContext,
		HEIN::ActorID& actorToDelete
	)
	{
		if (actor == nullptr) return;

		const std::vector<ActorID>& children = actor->GetChildren();
		bool hasChildren = !children.empty();

		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (m_selectedActor == actor)
		{
			nodeFlags |= ImGuiTreeNodeFlags_Selected;
		}

		if (!hasChildren)
		{
			nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		std::wstring tag = actor->GetTag();
		std::string narrowTag(tag.begin(), tag.end());

		ImGui::PushID(static_cast<int>(actor->GetID()));

		bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)actor->GetID(), nodeFlags, "%s", narrowTag.c_str());

		// Left-click to select
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
		{
			SelectActor(actor);
		}

		// Drag and Drop Source (drag this actor)
		if (ImGui::BeginDragDropSource())
		{
			ActorID draggedID = actor->GetID();
			ImGui::SetDragDropPayload("HIERARCHY_ACTOR_NODE", &draggedID, sizeof(ActorID));
			ImGui::Text("Move %s", narrowTag.c_str());
			ImGui::EndDragDropSource();
		}

		// Drag and Drop Target (drop another actor onto this actor to parent it)
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ACTOR_NODE"))
			{
				ActorID draggedID = *(const ActorID*)payload->Data;
				if (draggedID != actor->GetID())
				{
					manager.SetParent(draggedID, actor->GetID(), true);
				}
			}
			ImGui::EndDragDropTarget();
		}

		// Right-click context menu on the actor item
		if (ImGui::BeginPopupContextItem())
		{
			SelectActor(actor);

			if (ImGui::MenuItem("Create Child Empty"))
			{
				HEIN::Actor* childActor = manager.CreateActor(L"GameObject");
				childActor->AddComponent<HEIN::TransformComponent>();
				manager.SetParent(childActor->GetID(), actor->GetID(), false);
				childActor->Start();
				SelectActor(childActor);
			}

			if (actor->GetParentID() != INVALID_ACTOR_ID)
			{
				if (ImGui::MenuItem("Unparent (Set as Root)"))
				{
					manager.SetParent(actor->GetID(), INVALID_ACTOR_ID, true);
				}
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Duplicate Actor", "Ctrl+D"))
			{
				HEIN::Actor* copy = manager.DuplicateActor(actor, gameContext);
				if (copy != nullptr)
				{
					SelectActor(copy);
				}
			}

			if (ImGui::MenuItem("Delete Actor"))
			{
				actorToDelete = actor->GetID();
			}

			ImGui::EndPopup();
		}

		// Recursively render children if open
		if (hasChildren && nodeOpen)
		{
			for (ActorID childID : children)
			{
				Actor* child = manager.GetActor(childID);
				if (child != nullptr)
				{
					DrawActorTreeNode(child, manager, gameContext, actorToDelete);
				}
			}
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	EditorAction DebugUIManager::Draw(
		GameContext& gameContext,
		HEIN::ActorManager& manager,
		const DirectX::SimpleMath::Matrix& view,
		const DirectX::SimpleMath::Matrix& proj
	)
	{
		HEIN::EditorAction currentAction = HEIN::EditorAction::None;

		if (!m_isVisible) return currentAction;

		ImGuiIO& io = ImGui::GetIO();
		float screenW = io.DisplaySize.x;
		float screenH = io.DisplaySize.y;

		// Lock the Height to always be screenH, but allow Width to be resized (min 100, max screenW)
		ImGui::SetNextWindowSizeConstraints(ImVec2(100.0f, screenH), ImVec2(screenW, screenH));

		// Set the starting size (only applies the very first time it opens)
		ImGui::SetNextWindowSize(ImVec2(screenW * 0.25f, screenH), ImGuiCond_FirstUseEver);

		// The last parameter ImVec2(1.0f, 0.0f) is the Pivot (1.0 = Right edge, 0.0 = Top edge)
		ImGui::SetNextWindowPos(ImVec2(screenW, 0.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));

		ImGui::Begin("Inspector");
		
		ImVec2 inspectorPos = ImGui::GetWindowPos();
		ImVec2 inspectorSize = ImGui::GetWindowSize();

		// Ctrl+D shortcut to duplicate selected actor
		if (!ImGui::GetIO().WantTextInput && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
		{
			if (m_selectedActor != nullptr)
			{
				HEIN::Actor* copy = manager.DuplicateActor(m_selectedActor, gameContext);
				if (copy != nullptr)
				{
					SelectActor(copy);
				}
			}
		}

		if (m_selectedActor == nullptr || !manager.HasActor(m_selectedActor->GetID()))
		{
			m_selectedActor = nullptr;
			g_ActiveGizmoTarget = nullptr;
			ImGui::TextDisabled("No Actor Selected");
		}
		else
		{
			// Inspector Header: Duplicate & Delete buttons
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.85f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.65f, 0.95f, 1.0f));
			if (ImGui::Button("Duplicate Actor", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 4.0f, 26)))
			{
				HEIN::Actor* copy = manager.DuplicateActor(m_selectedActor, gameContext);
				if (copy != nullptr)
				{
					SelectActor(copy);
				}
			}
			ImGui::PopStyleColor(2);

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.25f, 0.25f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.05f, 0.05f, 1.0f));
			if (ImGui::Button("Delete", ImVec2(-1, 26)))
			{
				ActorID idToDelete = m_selectedActor->GetID();
				SelectActor(nullptr);
				manager.DestroyID(idToDelete);
			}
			ImGui::PopStyleColor(3);
			ImGui::Separator();

			std::wstring wtag = m_selectedActor ? m_selectedActor->GetTag() : L"";
			std::string narrowTag(wtag.begin(), wtag.end());

			char nameBuffer[256];
			strncpy_s(nameBuffer, narrowTag.c_str(), sizeof(nameBuffer));
			if (m_selectedActor && ImGui::InputText("Actor Name", nameBuffer, sizeof(nameBuffer)))
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

			if (m_selectedActor)
			{
				m_selectedActor->DrawInspector(gameContext);
			}

			ImGui::Separator();

			// Add Component UI
			std::vector<std::string> componentNames = HEIN::ComponentFactory::GetRegisteredComponentNames();
			if (componentNames.empty())
			{
				HEIN::ComponentFactory::Initialize();
				componentNames = HEIN::ComponentFactory::GetRegisteredComponentNames();
			}

			if (m_selectedActor && ImGui::BeginCombo("Add Component", "Select Component..."))
			{
				for (const std::string& compName : componentNames)
				{
					if (ImGui::Selectable(compName.c_str()))
					{
						HEIN::IComponent* newComp = HEIN::ComponentFactory::CreateComponent(compName, m_selectedActor, &manager);
						if (newComp)
						{
							newComp->Start();
						}
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

		ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

		
		float hierarchyWidth = screenW * 0.12f;
		float toolbarWidth = inspectorPos.x - hierarchyWidth;
		if (toolbarWidth < 10.0f) toolbarWidth = 10.0f; // Ensure a minimum width

		ImGui::SetNextWindowPos(ImVec2(hierarchyWidth, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(toolbarWidth, 80.0f), ImGuiCond_Always);

		ImGui::Begin("ToolBar", nullptr, toolbarFlags);
		if (ImGui::Button("PLAY")) currentAction = HEIN::EditorAction::PlayPressed;
		ImGui::SameLine();
		if (ImGui::Button("STOP")) 	currentAction = HEIN::EditorAction::StopPressed;
		ImGui::SameLine();
		if (ImGui::Button("SAVE SCENE")) currentAction = HEIN::EditorAction::SavePressed;
		ImGui::SameLine();
		if (ImGui::Button("AUTO SAVE")) currentAction = HEIN::EditorAction::AutoSavePressed;
		ImGui::SameLine();
		if (ImGui::Button("LOAD SCENE")) currentAction = HEIN::EditorAction::LoadPressed;
		ImGui::SameLine();
		if (ImGui::Button("NEW SCENE")) currentAction = HEIN::EditorAction::NewScenePressed;
		ImGui::SameLine();
		ImGui::Checkbox("Show Viewport", &m_showViewportPreview);

		if (m_selectedActor != nullptr)
		{
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.85f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.65f, 0.95f, 1.0f));
			if (ImGui::Button("Duplicate Actor"))
			{
				HEIN::Actor* copy = manager.DuplicateActor(m_selectedActor, gameContext);
				if (copy != nullptr)
				{
					SelectActor(copy);
				}
			}
			ImGui::PopStyleColor(2);

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
			if (ImGui::Button("Delete Selected"))
			{
				ActorID idToDelete = m_selectedActor->GetID();
				SelectActor(nullptr);
				manager.DestroyID(idToDelete);
			}
			ImGui::PopStyleColor();
		}

		ImGui::Separator();
		if (ImGui::Button("Create Empty Actor"))
		{
			HEIN::Actor* newActor = manager.CreateActor(L"GameObject");
			newActor->AddComponent<HEIN::TransformComponent>();
			newActor->Start();
			SelectActor(newActor);
		}
		ImGui::SameLine();
		if (ImGui::Button("+ UI Button"))
		{
			HEIN::Actor* newActor = manager.CreateActor(L"UIButton");
			newActor->AddComponent<HEIN::TransformComponent>();
			auto* btn = newActor->AddComponent<HEIN::UIButtonComponent>();
			btn->SetElementType(HEIN::UIElementType::Button);
			btn->SetText("Button");
			btn->Initialize(gameContext, nullptr, nullptr, nullptr);
			newActor->Start();
			SelectActor(newActor);
			g_ActiveGizmoTarget = btn;
		}
		ImGui::SameLine();
		if (ImGui::Button("+ UI Image"))
		{
			HEIN::Actor* newActor = manager.CreateActor(L"UIImage");
			newActor->AddComponent<HEIN::TransformComponent>();
			auto* img = newActor->AddComponent<HEIN::UIButtonComponent>();
			img->SetElementType(HEIN::UIElementType::Image);
			img->Initialize(gameContext, nullptr, nullptr, nullptr);
			newActor->Start();
			SelectActor(newActor);
			g_ActiveGizmoTarget = img;
		}
		ImGui::SameLine();
		if (ImGui::Button("+ UI Text"))
		{
			HEIN::Actor* newActor = manager.CreateActor(L"UIText");
			newActor->AddComponent<HEIN::TransformComponent>();
			auto* txt = newActor->AddComponent<HEIN::UIButtonComponent>();
			txt->SetElementType(HEIN::UIElementType::Text);
			txt->SetText("New Text Label");
			txt->SetFontSize(1.5f);
			txt->Initialize(gameContext, nullptr, nullptr, nullptr);
			newActor->Start();
			SelectActor(newActor);
			g_ActiveGizmoTarget = txt;
		}
		ImGui::SameLine();
		if (ImGui::Button("+ Camera"))
		{
			HEIN::Actor* newActor = manager.CreateActor(L"MainCamera");
			newActor->AddComponent<HEIN::TransformComponent>();
			auto* cam = newActor->AddComponent<HEIN::CameraController>();
			cam->SetPosition(DirectX::SimpleMath::Vector3(0.0f, 15.0f, -40.0f));
			newActor->Start();
			SelectActor(newActor);
			g_ActiveGizmoTarget = cam;
		}
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
		if (ImGui::Button("+ Stage"))
		{
			currentAction = HEIN::EditorAction::CreateStagePressed;
		}
		ImGui::PopStyleColor(2);
		ImGui::End();

		// DRAW THE HIERARCHY WINDOW
		ImGuiWindowFlags staticFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(hierarchyWidth, screenH), ImGuiCond_Always);
		ImGui::Begin("Hierarchy", nullptr, staticFlags);

		// Context menu on empty window area
		if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::MenuItem("Create Empty Actor"))
			{
				HEIN::Actor* newActor = manager.CreateActor(L"GameObject");
				newActor->AddComponent<HEIN::TransformComponent>();
				newActor->Start();
				SelectActor(newActor);
			}
			ImGui::EndPopup();
		}

		ActorID actorToDelete = INVALID_ACTOR_ID;

		// Collect and draw only root actors (parent == INVALID_ACTOR_ID)
		std::vector<HEIN::Actor*> rootActors;
		for (const auto& pair : manager.GetAllActors())
		{
			if (pair.second->GetParentID() == HEIN::INVALID_ACTOR_ID)
			{
				rootActors.push_back(pair.second.get());
			}
		}

		for (HEIN::Actor* rootActor : rootActors)
		{
			DrawActorTreeNode(rootActor, manager, gameContext, actorToDelete);
		}

		// Drag and drop target onto empty space in hierarchy window to unparent to root level
		ImVec2 avail = ImGui::GetContentRegionAvail();
		if (avail.y > 10.0f)
		{
			ImGui::Dummy(avail);
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ACTOR_NODE"))
				{
					ActorID draggedID = *(const ActorID*)payload->Data;
					manager.SetParent(draggedID, HEIN::INVALID_ACTOR_ID, true);
				}
				ImGui::EndDragDropTarget();
			}
		}

		if (actorToDelete != INVALID_ACTOR_ID)
		{
			if (m_selectedActor && m_selectedActor->GetID() == actorToDelete)
			{
				SelectActor(nullptr);
			}
			manager.DestroyID(actorToDelete);
		}

		ImGui::End();

		// Draw the floating, movable and resizable Camera Viewport window
		DrawViewportWindow(gameContext, true);

		return currentAction;
	}

	void DebugUIManager::DrawViewportWindow(GameContext& gameContext, bool isMagnified)
	{
		D3D11_VIEWPORT screenVp = gameContext.deviceResources.GetScreenViewport();

		if (isMagnified)
		{
			if (!m_showViewportPreview)
			{
				m_isViewportVisibleInUI = false;
				return;
			}

			ImGui::SetNextWindowSize(ImVec2(380.0f, 220.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowPos(ImVec2(screenVp.Width * 0.10f + 20.0f, screenVp.Height - 240.0f), ImGuiCond_FirstUseEver);

			// Make the window body 100% transparent so the D3D11 3D scene renders crystal clear with NO black overlay!
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.7f, 1.0f, 0.8f));

			ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
			if (ImGui::Begin("Camera Viewport (Live Preview)###CameraViewportWin", &m_showViewportPreview, winFlags))
			{
				ImVec2 contentMin = ImGui::GetCursorScreenPos();
				ImVec2 contentSize = ImGui::GetContentRegionAvail();

				if (contentSize.x >= 50.0f && contentSize.y >= 50.0f)
				{
					m_viewportPos = DirectX::SimpleMath::Vector2(contentMin.x, contentMin.y);
					m_viewportSize = DirectX::SimpleMath::Vector2(contentSize.x, contentSize.y);
					m_isViewportVisibleInUI = true;
				}
				else
				{
					m_isViewportVisibleInUI = false;
				}
			}
			else
			{
				m_isViewportVisibleInUI = false;
			}
			ImGui::End();
			ImGui::PopStyleColor(3);
		}
		else
		{
			ImGui::SetNextWindowSize(ImVec2(400.0f, 240.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowPos(ImVec2(screenVp.Width - 420.0f, 20.0f), ImGuiCond_FirstUseEver);

			// Make the window body 100% transparent so the D3D11 3D scene renders crystal clear with NO black overlay!
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.7f, 1.0f, 0.8f));

			ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
			if (ImGui::Begin("Debug Viewport (F2 to Full Editor)###DebugViewportWin", nullptr, winFlags))
			{
				ImVec2 contentMin = ImGui::GetCursorScreenPos();
				ImVec2 contentSize = ImGui::GetContentRegionAvail();

				if (contentSize.x >= 50.0f && contentSize.y >= 50.0f)
				{
					m_viewportPos = DirectX::SimpleMath::Vector2(contentMin.x, contentMin.y);
					m_viewportSize = DirectX::SimpleMath::Vector2(contentSize.x, contentSize.y);
					m_isViewportVisibleInUI = true;
				}
				else
				{
					m_isViewportVisibleInUI = false;
				}
			}
			else
			{
				m_isViewportVisibleInUI = false;
			}
			ImGui::End();
			ImGui::PopStyleColor(3);
		}
	}
}