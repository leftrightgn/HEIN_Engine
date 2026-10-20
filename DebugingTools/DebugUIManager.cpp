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
#include <Components/GaugeComponent.h>
#include <Components/AnimationEditorComponent.h>
#include <Windows.h>
#include <commdlg.h>
#include <filesystem>
#include <vector>
#include <Components/TransformComponent.h>
#include <Camera/CameraController.h>

namespace HEIN
{
	IGizmoEditable* g_ActiveGizmoTarget = nullptr;

	namespace
	{
		float s_activeVpX = 0.0f;
		float s_activeVpY = 0.0f;
		float s_activeVpW = 0.0f;
		float s_activeVpH = 0.0f;
	}

	DebugUIManager::~DebugUIManager()
	{
		g_ActiveGizmoTarget = nullptr;
		s_activeVpX = 0.0f;
		s_activeVpY = 0.0f;
		s_activeVpW = 0.0f;
		s_activeVpH = 0.0f;
	}

	void DebugUIManager::GetActiveViewportRect(float& outX, float& outY, float& outW, float& outH)
	{
		if (s_activeVpW > 10.0f && s_activeVpH > 10.0f)
		{
			outX = s_activeVpX;
			outY = s_activeVpY;
			outW = s_activeVpW;
			outH = s_activeVpH;
		}
		else
		{
			ImGuiIO& io = ImGui::GetIO();
			outX = 0.0f;
			outY = 0.0f;
			outW = (io.DisplaySize.x > 0.0f) ? io.DisplaySize.x : 1280.0f;
			outH = (io.DisplaySize.y > 0.0f) ? io.DisplaySize.y : 720.0f;
		}
	}

	void DebugUIManager::SetActiveViewportRect(float x, float y, float w, float h)
	{
		s_activeVpX = x;
		s_activeVpY = y;
		s_activeVpW = w;
		s_activeVpH = h;
	}

	namespace
	{
		D3D11_VIEWPORT FitViewportToAspect(const D3D11_VIEWPORT& bounds, float targetAspect = 1280.0f / 720.0f)
		{
			if (bounds.Width <= 10.0f || bounds.Height <= 10.0f || targetAspect <= 0.0f)
			{
				return bounds;
			}

			float currentAspect = bounds.Width / bounds.Height;
			D3D11_VIEWPORT fitted = bounds;

			if (currentAspect > targetAspect)
			{
				// Bounds are wider than 16:9 -> pillarbox horizontally
				fitted.Width = std::floor(bounds.Height * targetAspect);
				fitted.TopLeftX = std::floor(bounds.TopLeftX + (bounds.Width - fitted.Width) * 0.5f);
			}
			else if (currentAspect < targetAspect)
			{
				// Bounds are taller than 16:9 -> letterbox vertically
				fitted.Height = std::floor(bounds.Width / targetAspect);
				fitted.TopLeftY = std::floor(bounds.TopLeftY + (bounds.Height - fitted.Height) * 0.5f);
			}

			return fitted;
		}
	}

	D3D11_VIEWPORT DebugUIManager::GetCenterViewport(const GameContext& gameContext) const
	{
		D3D11_VIEWPORT fullVp = gameContext.deviceResources.GetScreenViewport();
		if (!m_isVisible)
		{
			return fullVp;
		}

		float screenW = fullVp.Width;
		float screenH = fullVp.Height;
		float hierarchyWidth = std::floor(screenW * 0.14f);
		float toolbarHeight = (m_toolbarHeight > 60.0f) ? m_toolbarHeight : 88.0f;
		float inspectorWidth = (m_inspectorWidth > 100.0f) ? m_inspectorWidth : std::floor(screenW * 0.24f);
		if (inspectorWidth > screenW - hierarchyWidth - 100.0f)
		{
			inspectorWidth = std::max(100.0f, screenW - hierarchyWidth - 100.0f);
		}

		D3D11_VIEWPORT vp = {};
		vp.TopLeftX = hierarchyWidth;
		vp.TopLeftY = toolbarHeight;
		vp.Width = std::max(100.0f, std::floor(screenW - hierarchyWidth - inspectorWidth));
		vp.Height = std::max(100.0f, std::floor(screenH - toolbarHeight));
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		return vp;
	}

	D3D11_VIEWPORT DebugUIManager::GetSceneViewport(const GameContext& gameContext) const
	{
		D3D11_VIEWPORT center = GetCenterViewport(gameContext);
		if (!m_isVisible)
		{
			return center;
		}
		if (m_viewportLayout == EditorViewportLayout::SplitSideBySide)
		{
			D3D11_VIEWPORT vp = center;
			vp.Width = std::floor(center.Width * 0.5f);
			return FitViewportToAspect(vp);
		}
		return FitViewportToAspect(center);
	}

	D3D11_VIEWPORT DebugUIManager::GetGameViewport(const GameContext& gameContext) const
	{
		D3D11_VIEWPORT center = GetCenterViewport(gameContext);
		if (!m_isVisible)
		{
			return center;
		}
		if (m_viewportLayout == EditorViewportLayout::SplitSideBySide)
		{
			float leftW = std::floor(center.Width * 0.5f);
			D3D11_VIEWPORT vp = center;
			vp.TopLeftX = center.TopLeftX + leftW;
			vp.Width = center.Width - leftW;
			return FitViewportToAspect(vp);
		}
		return FitViewportToAspect(center);
	}

	void DebugUIManager::Update(
		const GameContext& gameContext,
		HEIN::ActorManager& actorManager,
		const DirectX::SimpleMath::Matrix& view,
		const DirectX::SimpleMath::Matrix& proj,
		HEIN::CameraController* debugCameraController
	)
	{
		if (gameContext.keyboardTracker.pressed.F1)
		{
			m_isVisible = !m_isVisible;
		}

		if (gameContext.keyboardTracker.pressed.F2)
		{
			EditorViewportLayout nextLayout;
			switch (m_viewportLayout)
			{
			case EditorViewportLayout::SceneViewOnly:
				nextLayout = EditorViewportLayout::GameViewOnly;
				break;
			case EditorViewportLayout::GameViewOnly:
				nextLayout = EditorViewportLayout::SplitSideBySide;
				break;
			case EditorViewportLayout::SplitSideBySide:
			default:
				nextLayout = EditorViewportLayout::SceneViewOnly;
				break;
			}
			SetViewportLayout(nextLayout);
		}

		D3D11_VIEWPORT sceneVp = GetSceneViewport(gameContext);
		SetActiveViewportRect(sceneVp.TopLeftX, sceneVp.TopLeftY, sceneVp.Width, sceneVp.Height);

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

		if (gameContext.keyboardTracker.pressed.F && m_selectedActor != nullptr && !ImGui::GetIO().WantCaptureKeyboard)
		{
			HEIN::TransformComponent* transform = m_selectedActor->GetComponent<HEIN::TransformComponent>();
			if (transform && debugCameraController)
			{
				DirectX::SimpleMath::Matrix worldMat = transform->GetWorldMatrix();
				DirectX::SimpleMath::Vector3 worldPos = worldMat.Translation();

				ICameraMode* currentMode = debugCameraController->GetCurrentCameraMode();
				if (currentMode && currentMode->GetType() == CameraType::Debug)
				{
					currentMode->SetTarget(worldPos);
				}
			}
		}

		if (isShiftHeld)
		{
			if (gameContext.keyboardTracker.pressed.W) m_currentGinzmoOperation = ImGuizmo::TRANSLATE;
			if (gameContext.keyboardTracker.pressed.E) m_currentGinzmoOperation = ImGuizmo::ROTATE;
			if (gameContext.keyboardTracker.pressed.R) m_currentGinzmoOperation = ImGuizmo::SCALE;
		}

		if (m_rightPanelMode == RightPanelMode::SceneTransition && !ImGui::GetIO().WantCaptureKeyboard)
		{
			if (gameContext.keyboardTracker.pressed.F5)
			{
				if (m_viewportLayout == EditorViewportLayout::SceneViewOnly)
				{
					SetViewportLayout(EditorViewportLayout::GameViewOnly);
				}
				if (gameContext.sceneManager)
				{
					gameContext.sceneManager->PreviewTransition(m_editorTransitionDesc);
				}
			}
			if (gameContext.keyboardTracker.pressed.Space)
			{
				if (m_viewportLayout == EditorViewportLayout::SceneViewOnly)
				{
					SetViewportLayout(EditorViewportLayout::GameViewOnly);
				}
				if (gameContext.sceneManager)
				{
					if (!gameContext.sceneManager->IsTransitioning())
					{
						gameContext.sceneManager->StartPausedPreview(m_editorTransitionDesc);
					}
					else
					{
						gameContext.sceneManager->ToggleTransitionPause();
					}
				}
			}
		}

		if (m_viewportLayout != EditorViewportLayout::GameViewOnly &&
			gameContext.mouseButtonTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED &&
			!ImGui::GetIO().WantCaptureMouse)
		{
			double currentTime = gameContext.timer.GetTotalSeconds();
			if ((currentTime - m_lastleftClickTime) < DOUBLE_CLICK_THRESHOLD)
			{
				float mouseX = static_cast<float>(gameContext.mouseState.x);
				float mouseY = static_cast<float>(gameContext.mouseState.y);

				if (mouseX < sceneVp.TopLeftX || mouseX > sceneVp.TopLeftX + sceneVp.Width ||
					mouseY < sceneVp.TopLeftY || mouseY > sceneVp.TopLeftY + sceneVp.Height)
				{
					m_lastleftClickTime = -1.0;
					return;
				}

				DirectX::SimpleMath::Viewport viewport(sceneVp.TopLeftX, sceneVp.TopLeftY, sceneVp.Width, sceneVp.Height);
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
					if (!actor || !actor->IsVisible()) continue;
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

					// 2D Screen Picking for UI Button Component (mapped from center viewport to 1280x720 reference canvas)
					auto* uiBtn = actor->GetComponent<HEIN::UIButtonComponent>();
					if (uiBtn != nullptr)
					{
						float refMouseX = (mouseX - sceneVp.TopLeftX) / (sceneVp.Width / 1280.0f);
						float refMouseY = (mouseY - sceneVp.TopLeftY) / (sceneVp.Height / 720.0f);
						DirectX::SimpleMath::Vector2 bPos = uiBtn->GetPosition();
						DirectX::SimpleMath::Vector2 bSize = uiBtn->GetSize();
						if (refMouseX >= bPos.x && refMouseX <= bPos.x + bSize.x &&
							refMouseY >= bPos.y && refMouseY <= bPos.y + bSize.y)
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

		m_rightPanelMode = RightPanelMode::Inspector;

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

		bool isVisible = actor->IsVisible();
		ImGui::PushStyleColor(ImGuiCol_Button, isVisible ? ImVec4(0.2f, 0.45f, 0.2f, 0.6f) : ImVec4(0.35f, 0.35f, 0.35f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_Text, isVisible ? ImVec4(0.9f, 0.9f, 0.9f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 0.7f));
		if (ImGui::SmallButton(isVisible ? "V" : "-"))
		{
			actor->SetVisible(!isVisible);
		}
		ImGui::PopStyleColor(2);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(isVisible ? "Visible (Click to hide)" : "Hidden (Click to show)");
		}
		ImGui::SameLine();

		if (!actor->IsVisible())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 0.7f));
		}

		bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)actor->GetID(), nodeFlags, "%s", narrowTag.c_str());

		if (!actor->IsVisible())
		{
			ImGui::PopStyleColor();
		}

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

			if (ImGui::MenuItem(actor->IsVisible() ? "Hide Actor" : "Show Actor"))
			{
				actor->SetVisible(!actor->IsVisible());
			}

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
		float hierarchyWidth = std::floor(screenW * 0.14f);
		m_toolbarHeight = 88.0f;

		// -------------------------------------------------------------------------
		// DRAW THE TOP TOOLBAR (Spans full width from Hierarchy to Right Edge)
		//    Inspector is placed strictly below m_toolbarHeight so it NEVER covers ToolBar!
		// -------------------------------------------------------------------------
		ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
		float toolbarWidth = std::max(100.0f, screenW - hierarchyWidth);

		ImGui::SetNextWindowPos(ImVec2(hierarchyWidth, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(toolbarWidth, m_toolbarHeight), ImGuiCond_Always);

		auto SameLineWrap = [](float nextItemEstimatedWidth)
		{
			ImGui::SameLine();
			if (ImGui::GetContentRegionAvail().x < nextItemEstimatedWidth)
			{
				ImGui::NewLine();
			}
		};

		ImGui::Begin("ToolBar", nullptr, toolbarFlags);
		if (ImGui::Button("PLAY (Exit Editor)")) currentAction = HEIN::EditorAction::PlayPressed;
		SameLineWrap(105.0f);
		if (ImGui::Button("STOP (Pause)")) currentAction = HEIN::EditorAction::StopPressed;
		SameLineWrap(105.0f);

		// Unity-style Scene View vs Game View layout selector tabs (F2)
		bool isSceneOnly = (m_viewportLayout == EditorViewportLayout::SceneViewOnly);
		bool isGameOnly  = (m_viewportLayout == EditorViewportLayout::GameViewOnly);
		bool isSplit     = (m_viewportLayout == EditorViewportLayout::SplitSideBySide);

		if (isSceneOnly)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.85f, 1.0f));
			ImGui::Button("[ Scene ] (F2)");
			ImGui::PopStyleColor();
		}
		else
		{
			if (ImGui::Button("[ Scene ] (F2)"))
			{
				SetViewportLayout(EditorViewportLayout::SceneViewOnly);
			}
		}

		SameLineWrap(75.0f);

		if (isGameOnly)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.65f, 0.35f, 1.0f));
			ImGui::Button("[ Game ]");
			ImGui::PopStyleColor();
		}
		else
		{
			if (ImGui::Button("[ Game ]"))
			{
				SetViewportLayout(EditorViewportLayout::GameViewOnly);
			}
		}

		SameLineWrap(150.0f);

		if (isSplit)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.35f, 0.85f, 1.0f));
			ImGui::Button("[ Split: Scene | Game ]");
			ImGui::PopStyleColor();
		}
		else
		{
			if (ImGui::Button("[ Split: Scene | Game ]"))
			{
				SetViewportLayout(EditorViewportLayout::SplitSideBySide);
			}
		}

		SameLineWrap(120.0f);
		if (m_rightPanelMode == RightPanelMode::SceneTransition)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.45f, 0.15f, 1.0f));
			if (ImGui::Button("INSPECTOR")) m_rightPanelMode = RightPanelMode::Inspector;
			ImGui::PopStyleColor();
		}
		else
		{
			if (ImGui::Button("SCENE CHANGER")) m_rightPanelMode = RightPanelMode::SceneTransition;
		}

		// Pause / Pin Scene Transition directly from the ToolBar
		if (gameContext.sceneManager != nullptr)
		{
			bool isTransRunning = gameContext.sceneManager->IsTransitioning();
			bool isTransPaused  = gameContext.sceneManager->IsTransitionPaused();

			SameLineWrap(145.0f);
			if (isTransRunning && isTransPaused)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.90f, 0.55f, 0.10f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 0.65f, 0.20f, 1.0f));
				if (ImGui::Button("RESUME TRANSITION"))
				{
					gameContext.sceneManager->SetTransitionPaused(false);
				}
				ImGui::PopStyleColor(2);

				SameLineWrap(95.0f);
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.20f, 0.20f, 1.0f));
				if (ImGui::Button("HIDE TRANS"))
				{
					gameContext.sceneManager->StopTransitionPreview();
				}
				ImGui::PopStyleColor();
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.50f, 0.65f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.62f, 0.80f, 1.0f));
				if (ImGui::Button("PAUSE TRANSITION"))
				{
					if (!m_hasLoadedEditorDesc)
					{
						m_editorTransitionDesc = gameContext.sceneManager->GetDefaultTransition();
						m_hasLoadedEditorDesc = true;
					}
					if (m_viewportLayout == EditorViewportLayout::SceneViewOnly)
					{
						SetViewportLayout(EditorViewportLayout::GameViewOnly);
					}
					if (!isTransRunning)
					{
						gameContext.sceneManager->StartPausedPreview(m_editorTransitionDesc);
					}
					else
					{
						gameContext.sceneManager->SetTransitionPaused(true);
					}
				}
				ImGui::PopStyleColor(2);
			}
		}

		SameLineWrap(95.0f);
		if (ImGui::Button("SAVE SCENE")) currentAction = HEIN::EditorAction::SavePressed;
		SameLineWrap(90.0f);
		if (ImGui::Button("AUTO SAVE")) currentAction = HEIN::EditorAction::AutoSavePressed;
		SameLineWrap(95.0f);
		if (ImGui::Button("LOAD SCENE")) currentAction = HEIN::EditorAction::LoadPressed;
		SameLineWrap(95.0f);
		if (ImGui::Button("NEW SCENE")) currentAction = HEIN::EditorAction::NewScenePressed;
		SameLineWrap(85.0f);
		if (ImGui::Button("ANIMATOR")) m_showAnimatorWindow = !m_showAnimatorWindow;

		ImGui::Separator();
		if (ImGui::Button("Create Empty Actor"))
		{
			HEIN::Actor* newActor = manager.CreateActor(L"GameObject");
			newActor->AddComponent<HEIN::TransformComponent>();
			newActor->Start();
			SelectActor(newActor);
		}
		SameLineWrap(90.0f);
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
		SameLineWrap(90.0f);
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
		SameLineWrap(90.0f);
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
		SameLineWrap(80.0f);
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
		SameLineWrap(75.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 0.9f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
		if (ImGui::Button("+ Stage"))
		{
			currentAction = HEIN::EditorAction::CreateStagePressed;
		}
		ImGui::PopStyleColor(2);

		SameLineWrap(105.0f);
		ImGui::Checkbox("PiP Preview", &m_showViewportPreview);
		SameLineWrap(115.0f);
		ImGui::Checkbox("Show Colliders", &m_showColliders);
		SameLineWrap(85.0f);
		ImGui::Checkbox("Show UI", &m_showUI);
		SameLineWrap(125.0f);
		ImGui::Checkbox("Frustum Culling", &gameContext.enableFrustumCulling);

		if (m_selectedActor != nullptr)
		{
			SameLineWrap(115.0f);
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

			SameLineWrap(115.0f);
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
			if (ImGui::Button("Delete Selected"))
			{
				ActorID idToDelete = m_selectedActor->GetID();
				SelectActor(nullptr);
				manager.DestroyID(idToDelete);
			}
			ImGui::PopStyleColor();
		}
		ImGui::End();

		// -------------------------------------------------------------------------
		// DRAW THE INSPECTOR WINDOW (Strictly below ToolBar so it NEVER covers it!)
		// -------------------------------------------------------------------------
		float inspectorTopY = m_toolbarHeight;
		float inspectorHeight = std::max(100.0f, screenH - inspectorTopY);
		float maxInspectorWidth = std::max(200.0f, screenW - hierarchyWidth - 150.0f);

		ImGui::SetNextWindowSizeConstraints(ImVec2(180.0f, inspectorHeight), ImVec2(maxInspectorWidth, inspectorHeight));
		ImGui::SetNextWindowSize(ImVec2(std::floor(screenW * 0.24f), inspectorHeight), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(screenW, inspectorTopY), ImGuiCond_Always, ImVec2(1.0f, 0.0f));

		ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
		
		ImVec2 inspectorSize = ImGui::GetWindowSize();
		m_inspectorWidth = inspectorSize.x;

		D3D11_VIEWPORT sceneVp = GetSceneViewport(gameContext);
		D3D11_VIEWPORT gameVp  = GetGameViewport(gameContext);
		SetActiveViewportRect(sceneVp.TopLeftX, sceneVp.TopLeftY, sceneVp.Width, sceneVp.Height);

		// Draw subtle Unity-style border around the 16:9 fitted Scene / Game Viewport(s)
		if (ImDrawList* bgList = ImGui::GetBackgroundDrawList())
		{
			if (m_viewportLayout != EditorViewportLayout::GameViewOnly)
			{
				bgList->AddRect(
					ImVec2(sceneVp.TopLeftX, sceneVp.TopLeftY),
					ImVec2(sceneVp.TopLeftX + sceneVp.Width, sceneVp.TopLeftY + sceneVp.Height),
					IM_COL32(55, 130, 210, 180), 0.0f, 0, 1.5f
				);
			}
			if (m_viewportLayout != EditorViewportLayout::SceneViewOnly)
			{
				bgList->AddRect(
					ImVec2(gameVp.TopLeftX, gameVp.TopLeftY),
					ImVec2(gameVp.TopLeftX + gameVp.Width, gameVp.TopLeftY + gameVp.Height),
					IM_COL32(60, 175, 95, 180), 0.0f, 0, 1.5f
				);
			}
		}

		// Mode Tabs: [ Inspector ]  [ Scene Transition ]
		if (m_rightPanelMode == RightPanelMode::Inspector)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.85f, 1.0f));
			ImGui::Button("Inspector", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 2.0f, 26));
			ImGui::PopStyleColor();
		}
		else
		{
			if (ImGui::Button("Inspector", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 2.0f, 26)))
			{
				m_rightPanelMode = RightPanelMode::Inspector;
			}
		}

		ImGui::SameLine();

		if (m_rightPanelMode == RightPanelMode::SceneTransition)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.45f, 0.15f, 1.0f));
			ImGui::Button("Scene Transition", ImVec2(-1, 26));
			ImGui::PopStyleColor();
		}
		else
		{
			if (ImGui::Button("Scene Transition", ImVec2(-1, 26)))
			{
				m_rightPanelMode = RightPanelMode::SceneTransition;
			}
		}

		ImGui::Separator();

		if (m_rightPanelMode == RightPanelMode::SceneTransition)
		{
			RenderSceneTransitionPanel(gameContext);
		}
		else
		{
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

				if (m_selectedActor != nullptr)
				{
					ImGui::Separator();

					bool isVis = m_selectedActor->IsVisible();
					if (ImGui::Checkbox("Visible", &isVis))
					{
						m_selectedActor->SetVisible(isVis);
					}
					ImGui::SameLine();
					bool isAct = m_selectedActor->IsActive();
					if (ImGui::Checkbox("Active", &isAct))
					{
						m_selectedActor->SetActive(isAct);
					}

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

					if (m_selectedActor)
					{
						const char* actorTypeNames[] = {
							"Default",
							"Player",
							"Enemy",
							"Items",
							"Enviroment",
							"Ghost"
						};
						int currentTypeIdx = static_cast<int>(m_selectedActor->GetActorType());
						if (currentTypeIdx < 0 || currentTypeIdx >= IM_ARRAYSIZE(actorTypeNames))
						{
							currentTypeIdx = 0;
						}

						if (ImGui::Combo("Actor Type", &currentTypeIdx, actorTypeNames, IM_ARRAYSIZE(actorTypeNames)))
						{
							m_selectedActor->SetActorType(static_cast<HEIN::ActorType>(currentTypeIdx));
						}
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
				}
			}
		} // end if (m_rightPanelMode == RightPanelMode::SceneTransition) ... else

		// ImGuizmo 2D & 3D Viewport Controls Integration (active regardless of which right panel tab is open)
		if (m_selectedActor != nullptr && g_ActiveGizmoTarget != nullptr)
		{
			bool is2DUITarget = (m_selectedActor->GetComponent<HEIN::UIButtonComponent>() != nullptr ||
			                     m_selectedActor->GetComponent<HEIN::GaugeComponent>() != nullptr);
			if (is2DUITarget)
			{
				// 2D UI targets draw in Game View when GameViewOnly or SplitSideBySide is active
				D3D11_VIEWPORT uiVp = (m_viewportLayout == EditorViewportLayout::SceneViewOnly) ? sceneVp : gameVp;
				SetActiveViewportRect(uiVp.TopLeftX, uiVp.TopLeftY, uiVp.Width, uiVp.Height);
				g_ActiveGizmoTarget->DrawGizmo(view, proj, m_currentGinzmoOperation, m_currentGinzmo);
				SetActiveViewportRect(sceneVp.TopLeftX, sceneVp.TopLeftY, sceneVp.Width, sceneVp.Height);
			}
			else if (m_viewportLayout != EditorViewportLayout::GameViewOnly)
			{
				SetActiveViewportRect(sceneVp.TopLeftX, sceneVp.TopLeftY, sceneVp.Width, sceneVp.Height);
				g_ActiveGizmoTarget->DrawGizmo(view, proj, m_currentGinzmoOperation, m_currentGinzmo);
			}
		}

		ImGui::End();

		// -------------------------------------------------------------------------
		// INTERACTIVE DRAG HANDLES FOR PAUSED TRANSITION UI TEXT & LOADING BAR
		// -------------------------------------------------------------------------
		if (gameContext.sceneManager &&
		    gameContext.sceneManager->IsTransitioning() &&
		    gameContext.sceneManager->IsTransitionPaused() &&
		    m_viewportLayout != EditorViewportLayout::SceneViewOnly)
		{
			if (!m_hasLoadedEditorDesc)
			{
				m_editorTransitionDesc = gameContext.sceneManager->GetDefaultTransition();
				m_hasLoadedEditorDesc = true;
			}

			ImDrawList* fg = ImGui::GetForegroundDrawList();
			if (fg && gameVp.Width > 50.0f && gameVp.Height > 50.0f)
			{
				float uiScale = std::min(gameVp.Width / 1280.0f, gameVp.Height / 720.0f);

				// Compute Tip UI Text Box in screen space inside gameVp
				float tipScreenX = gameVp.TopLeftX + m_editorTransitionDesc.tipNormalizedPos.x * gameVp.Width;
				float tipScreenY = gameVp.TopLeftY + m_editorTransitionDesc.tipNormalizedPos.y * gameVp.Height;
				ImVec2 tipMin(tipScreenX - 6.0f, tipScreenY - 6.0f);
				ImVec2 tipMax(tipScreenX + 380.0f * uiScale, tipScreenY + 62.0f * uiScale);

				// Compute Loading Bar Box in screen space inside gameVp
				float barScreenX = gameVp.TopLeftX + m_editorTransitionDesc.barNormalizedPos.x * gameVp.Width;
				float barScreenY = gameVp.TopLeftY + m_editorTransitionDesc.barNormalizedPos.y * gameVp.Height;
				float barLen = m_editorTransitionDesc.barLength * uiScale;
				ImVec2 barMin(barScreenX - 10.0f, barScreenY - 18.0f * uiScale);
				ImVec2 barMax(barScreenX + barLen + 10.0f, barScreenY + 18.0f * uiScale);
				if (m_editorTransitionDesc.barOrientation == LoadingBarOrientation::Horizontal_RightToLeft)
				{
					barMin.x = barScreenX - barLen - 10.0f;
					barMax.x = barScreenX + 10.0f;
				}
				else if (m_editorTransitionDesc.barOrientation == LoadingBarOrientation::Center_Horizontal)
				{
					barMin.x = barScreenX - barLen * 0.5f - 10.0f;
					barMax.x = barScreenX + barLen * 0.5f + 10.0f;
				}

				ImVec2 mousePos = io.MousePos;
				bool overTip = m_editorTransitionDesc.showTips &&
				               (mousePos.x >= tipMin.x && mousePos.x <= tipMax.x && mousePos.y >= tipMin.y && mousePos.y <= tipMax.y);
				bool overBar = m_editorTransitionDesc.showLoadingBar &&
				               (mousePos.x >= barMin.x && mousePos.x <= barMax.x && mousePos.y >= barMin.y && mousePos.y <= barMax.y);

				if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					m_draggingTransitionElement = 0;
				}
				else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive() && !ImGuizmo::IsUsing())
				{
					if (overTip)
					{
						m_draggingTransitionElement = 1;
					}
					else if (overBar)
					{
						m_draggingTransitionElement = 2;
					}
				}

				if (m_draggingTransitionElement == 1 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
				{
					m_editorTransitionDesc.tipNormalizedPos.x = std::clamp(m_editorTransitionDesc.tipNormalizedPos.x + io.MouseDelta.x / gameVp.Width, 0.0f, 0.95f);
					m_editorTransitionDesc.tipNormalizedPos.y = std::clamp(m_editorTransitionDesc.tipNormalizedPos.y + io.MouseDelta.y / gameVp.Height, 0.0f, 0.95f);
					gameContext.sceneManager->UpdateActiveTransitionDesc(m_editorTransitionDesc);
				}
				else if (m_draggingTransitionElement == 2 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
				{
					m_editorTransitionDesc.barNormalizedPos.x = std::clamp(m_editorTransitionDesc.barNormalizedPos.x + io.MouseDelta.x / gameVp.Width, 0.0f, 0.98f);
					m_editorTransitionDesc.barNormalizedPos.y = std::clamp(m_editorTransitionDesc.barNormalizedPos.y + io.MouseDelta.y / gameVp.Height, 0.0f, 0.98f);
					gameContext.sceneManager->UpdateActiveTransitionDesc(m_editorTransitionDesc);
				}

				if (m_editorTransitionDesc.showTips)
				{
					ImU32 col = (m_draggingTransitionElement == 1 || overTip) ? IM_COL32(80, 220, 255, 230) : IM_COL32(80, 200, 255, 130);
					fg->AddRect(tipMin, tipMax, col, 4.0f, 0, 1.5f);
					fg->AddText(ImVec2(tipMin.x + 4.0f, tipMin.y - 16.0f), col, "[Drag UI Tip Text]");
				}

				if (m_editorTransitionDesc.showLoadingBar)
				{
					ImU32 col = (m_draggingTransitionElement == 2 || overBar) ? IM_COL32(255, 210, 80, 230) : IM_COL32(255, 195, 70, 130);
					fg->AddRect(barMin, barMax, col, 4.0f, 0, 1.5f);
					fg->AddText(ImVec2(barMin.x + 4.0f, barMin.y - 16.0f), col, "[Drag Loading Bar]");
				}
			}
		}
		else
		{
			m_draggingTransitionElement = 0;
		}

		// DRAW THE HIERARCHY WINDOW
		// The Scene Hierarchy provides a visual tree-view of the Actor parent-child graph.
		// It supports drag-and-drop parenting, context menu spawning, and selection tracking.
		// Reorganising the hierarchy here directly alters the execution order of CascadeTransforms.
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

		// Draw Animator Window
		if (m_showAnimatorWindow)
		{
			ImGui::SetNextWindowSize(ImVec2(800.0f, 500.0f), ImGuiCond_FirstUseEver);
			ImGui::Begin("Animator", &m_showAnimatorWindow);

			if (m_selectedActor != nullptr)
			{
				auto* animEditor = m_selectedActor->GetComponent<HEIN::AnimationEditorComponent>();
				if (animEditor)
				{
					animEditor->DrawAnimatorWindow(gameContext);
				}
				else
				{
					ImGui::Text("Selected actor does not have an AnimationEditorComponent.");
					if (ImGui::Button("Add Animation Editor"))
					{
						auto* newComp = m_selectedActor->AddComponent<HEIN::AnimationEditorComponent>();
						newComp->Start();
					}
				}
			}
			else
			{
				ImGui::Text("Please select an actor to edit its animations.");
			}
			ImGui::End();
		}

		if (m_viewportLayout == EditorViewportLayout::SplitSideBySide)
		{
			ImDrawList* fg = ImGui::GetForegroundDrawList();
			if (fg)
			{
				// Subtle vertical divider line between Scene View and Game View
				fg->AddLine(
					ImVec2(gameVp.TopLeftX, sceneVp.TopLeftY),
					ImVec2(gameVp.TopLeftX, sceneVp.TopLeftY + sceneVp.Height),
					IM_COL32(35, 35, 35, 255), 2.0f
				);

				// Left: [ Scene View ] badge
				fg->AddRectFilled(
					ImVec2(sceneVp.TopLeftX + 8.0f, sceneVp.TopLeftY + 8.0f),
					ImVec2(sceneVp.TopLeftX + 106.0f, sceneVp.TopLeftY + 28.0f),
					IM_COL32(20, 20, 20, 200), 4.0f
				);
				fg->AddText(
					ImVec2(sceneVp.TopLeftX + 16.0f, sceneVp.TopLeftY + 11.0f),
					IM_COL32(90, 180, 255, 255), "Scene View"
				);

				// Right: [ Game View ] badge
				fg->AddRectFilled(
					ImVec2(gameVp.TopLeftX + 8.0f, gameVp.TopLeftY + 8.0f),
					ImVec2(gameVp.TopLeftX + 106.0f, gameVp.TopLeftY + 28.0f),
					IM_COL32(20, 20, 20, 200), 4.0f
				);
				fg->AddText(
					ImVec2(gameVp.TopLeftX + 16.0f, gameVp.TopLeftY + 11.0f),
					IM_COL32(100, 220, 130, 255), "Game View"
				);
			}
		}

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

	void DebugUIManager::RenderSceneTransitionPanel(GameContext& gameContext)
	{
		if (!gameContext.sceneManager)
		{
			ImGui::TextDisabled("SceneManager not available in GameContext.");
			return;
		}

		if (!m_hasLoadedEditorDesc)
		{
			m_editorTransitionDesc = gameContext.sceneManager->GetDefaultTransition();
			m_hasLoadedEditorDesc = true;
		}

		float dt = static_cast<float>(gameContext.timer.GetElapsedSeconds());
		if (m_transitionStatusTimer > 0.0f)
		{
			m_transitionStatusTimer -= dt;
			if (m_transitionStatusTimer <= 0.0f)
			{
				m_transitionStatusMessage.clear();
			}
		}

		ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.2f, 1.0f), "Scene Transition Asset Settings");
		ImGui::TextDisabled("Configure full-screen fade, tint, hold, and UI text placement.");
		ImGui::Separator();

		// -------------------------------------------------------------------------
		// PAUSE & UI TEXT PLACEMENT CONTROLS (Top-level quick access)
		// -------------------------------------------------------------------------
		bool isTransRunning = gameContext.sceneManager->IsTransitioning();
		bool isTransPaused  = gameContext.sceneManager->IsTransitionPaused();

		if (isTransRunning && isTransPaused)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.90f, 0.55f, 0.10f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 0.65f, 0.20f, 1.0f));
			if (ImGui::Button("PAUSED - Click to Resume (Space)", ImVec2(-1, 32)))
			{
				gameContext.sceneManager->SetTransitionPaused(false);
			}
			ImGui::PopStyleColor(2);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.78f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.65f, 0.90f, 1.0f));
			if (ImGui::Button("Pause / Pin Transition to Place UI Text (Space)", ImVec2(-1, 32)))
			{
				if (m_viewportLayout == EditorViewportLayout::SceneViewOnly)
				{
					SetViewportLayout(EditorViewportLayout::GameViewOnly);
				}
				if (!isTransRunning)
				{
					gameContext.sceneManager->StartPausedPreview(m_editorTransitionDesc);
				}
				else
				{
					gameContext.sceneManager->SetTransitionPaused(true);
				}
			}
			ImGui::PopStyleColor(2);
		}

		if (ImGui::Button("Play Full Preview (F5)", ImVec2(ImGui::GetContentRegionAvail().x * 0.62f - 2.0f, 26)))
		{
			if (m_viewportLayout == EditorViewportLayout::SceneViewOnly)
			{
				SetViewportLayout(EditorViewportLayout::GameViewOnly);
			}
			gameContext.sceneManager->PreviewTransition(m_editorTransitionDesc);
		}
		ImGui::SameLine();
		if (ImGui::Button("Hide / Stop", ImVec2(-1, 26)))
		{
			gameContext.sceneManager->StopTransitionPreview();
		}

		// Live Scrubbers & Quick UI Text Placement Sliders while Transition is Active/Paused
		if (gameContext.sceneManager->IsTransitioning())
		{
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "Live UI Text & Bar Placement (Drag in Game View or Slide):");

			float alpha = gameContext.sceneManager->GetTransitionAlpha();
			if (ImGui::SliderFloat("Overlay Alpha", &alpha, 0.0f, 1.0f, "%.2f"))
			{
				gameContext.sceneManager->SetTransitionPaused(true);
				gameContext.sceneManager->SetTransitionAlpha(alpha);
			}

			float progress = gameContext.sceneManager->GetLoadingProgress();
			if (ImGui::SliderFloat("Bar Progress", &progress, 0.0f, 1.0f, "%.2f"))
			{
				gameContext.sceneManager->SetTransitionPaused(true);
				gameContext.sceneManager->SetLoadingProgress(progress);
			}

			ImGui::SliderFloat2("UI Tip Text Pos (X,Y)", &m_editorTransitionDesc.tipNormalizedPos.x, 0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat2("Loading Bar Pos (X,Y)", &m_editorTransitionDesc.barNormalizedPos.x, 0.0f, 1.0f, "%.3f");

			if (!m_editorTransitionDesc.tips.empty())
			{
				size_t tipIdx = gameContext.sceneManager->GetActiveTipIndex() % m_editorTransitionDesc.tips.size();
				if (ImGui::Button("< Prev Tip"))
				{
					size_t nextIdx = (tipIdx == 0) ? (m_editorTransitionDesc.tips.size() - 1) : (tipIdx - 1);
					gameContext.sceneManager->SetActiveTipIndex(nextIdx);
				}
				ImGui::SameLine();
				if (ImGui::Button("Next Tip >"))
				{
					size_t nextIdx = (tipIdx + 1) % m_editorTransitionDesc.tips.size();
					gameContext.sceneManager->SetActiveTipIndex(nextIdx);
				}
				ImGui::SameLine();
				ImGui::TextDisabled("Tip #%d: %s", static_cast<int>(tipIdx + 1), m_editorTransitionDesc.tips[tipIdx].title.c_str());
			}
		}

		ImGui::Separator();

		// Transition Style Selector
		if (m_editorTransitionDesc.texturePath.empty())
		{
			m_transitionStyleIndex = 0; // Solid Color Fade
		}
		else if (m_editorTransitionDesc.columns <= 1 && m_editorTransitionDesc.rows <= 1 && m_editorTransitionDesc.totalFrames <= 1)
		{
			m_transitionStyleIndex = 1; // Static Image (.dds)
		}
		else
		{
			m_transitionStyleIndex = 2; // Animated Video Flipbook (.dds Sprite Sheet)
		}

		ImGui::Text("Transition Style:");
		if (ImGui::RadioButton("Solid Color Fade", &m_transitionStyleIndex, 0))
		{
			m_editorTransitionDesc.texturePath = L"";
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Static Image", &m_transitionStyleIndex, 1))
		{
			if (m_editorTransitionDesc.texturePath.empty())
			{
				m_editorTransitionDesc.texturePath = L"Resources/Textures/fade.dds";
			}
			m_editorTransitionDesc.columns = 1;
			m_editorTransitionDesc.rows = 1;
			m_editorTransitionDesc.totalFrames = 1;
		}
		ImGui::SameLine();
		if (ImGui::RadioButton("Flipbook Sheet", &m_transitionStyleIndex, 2))
		{
			if (m_editorTransitionDesc.texturePath.empty())
			{
				m_editorTransitionDesc.texturePath = L"Resources/Textures/transition_sheet.dds";
			}
			if (m_editorTransitionDesc.columns <= 1) m_editorTransitionDesc.columns = 4;
			if (m_editorTransitionDesc.rows <= 1) m_editorTransitionDesc.rows = 4;
			if (m_editorTransitionDesc.totalFrames <= 1) m_editorTransitionDesc.totalFrames = 16;
		}

		ImGui::Separator();

		// Texture File Picker (shown for styles 1 and 2)
		if (m_transitionStyleIndex > 0)
		{
			ImGui::Text("Texture Path (.dds):");
			std::string texturePathStr(m_editorTransitionDesc.texturePath.begin(), m_editorTransitionDesc.texturePath.end());
			char pathBuf[512] = {};
			strcpy_s(pathBuf, sizeof(pathBuf), texturePathStr.c_str());

			if (ImGui::InputText("##TexturePathInput", pathBuf, sizeof(pathBuf)))
			{
				m_editorTransitionDesc.texturePath = std::wstring(pathBuf, pathBuf + strlen(pathBuf));
			}

			if (ImGui::Button("Browse .dds..."))
			{
				OPENFILENAMEW ofn = {};
				wchar_t szFile[MAX_PATH] = {};
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = nullptr;
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
				ofn.lpstrFilter = L"DDS Textures (*.dds)\0*.dds\0All Files (*.*)\0*.*\0";
				ofn.nFilterIndex = 1;
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

				if (GetOpenFileNameW(&ofn))
				{
					std::filesystem::path p(szFile);
					std::wstring ws = p.lexically_normal().wstring();
					// Find "Resources" or "Dual/Resources"
					size_t pos = ws.find(L"Resources");
					if (pos != std::wstring::npos)
					{
						ws = ws.substr(pos);
					}
					for (wchar_t& c : ws)
					{
						if (c == L'\\') c = L'/';
					}
					m_editorTransitionDesc.texturePath = ws;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear to Solid Fade"))
			{
				m_editorTransitionDesc.texturePath = L"";
				m_transitionStyleIndex = 0;
			}

			ImGui::Separator();
		}

		// Timing Controls
		ImGui::Text("Transition Timings:");
		ImGui::DragFloat("Fade Out Duration", &m_editorTransitionDesc.fadeOutDuration, 0.05f, 0.05f, 5.0f, "%.2fs");
		if (ImGui::DragFloat("Min Loading Hold Duration", &m_editorTransitionDesc.minLoadingHoldDuration, 0.05f, 0.1f, 10.0f, "%.2fs"))
		{
			m_editorTransitionDesc.holdDuration = m_editorTransitionDesc.minLoadingHoldDuration;
		}
		ImGui::DragFloat("Fade In Duration", &m_editorTransitionDesc.fadeInDuration, 0.05f, 0.05f, 5.0f, "%.2fs");

		ImGui::Separator();

		// Color Picker
		ImGui::Text("Color & Tint:");
		ImGui::ColorEdit4("Tint / Fade Color", &m_editorTransitionDesc.tintColor.x);

		ImGui::Separator();

		// Flipbook / Video Settings (shown when Flipbook mode is active)
		if (m_transitionStyleIndex == 2 || ImGui::CollapsingHeader("Flipbook / Video Settings"))
		{
			ImGui::DragInt("Columns", &m_editorTransitionDesc.columns, 1, 1, 32);
			ImGui::DragInt("Rows", &m_editorTransitionDesc.rows, 1, 1, 32);
			ImGui::DragInt("Total Frames", &m_editorTransitionDesc.totalFrames, 1, 1, 1024);
			ImGui::DragFloat("Playback FPS", &m_editorTransitionDesc.fps, 0.5f, 1.0f, 120.0f, "%.1f FPS");
			ImGui::Checkbox("Loop Animation", &m_editorTransitionDesc.loopAnimation);
			ImGui::Separator();
		}

		// Versatile Elden Ring Loading Bar Controls
		if (ImGui::CollapsingHeader("Loading Bar Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Show Loading Bar", &m_editorTransitionDesc.showLoadingBar);

			if (m_editorTransitionDesc.showLoadingBar)
			{
				// Orientation combo
				const char* orientationNames[] = {
					"Horizontal (Left -> Right)",
					"Horizontal (Right -> Left)",
					"Vertical (Bottom -> Top)",
					"Vertical (Top -> Bottom)",
					"Center Horizontal",
					"Center Vertical"
				};
				int currentOri = static_cast<int>(m_editorTransitionDesc.barOrientation);
				if (ImGui::Combo("Orientation", &currentOri, orientationNames, IM_ARRAYSIZE(orientationNames)))
				{
					m_editorTransitionDesc.barOrientation = static_cast<LoadingBarOrientation>(currentOri);
				}

				// Style combo
				const char* styleNames[] = {
					"Elden Ring Needle",
					"Solid Fill",
					"Segmented",
					"Custom Textured"
				};
				int currentStyle = static_cast<int>(m_editorTransitionDesc.barStyle);
				if (ImGui::Combo("Bar Style", &currentStyle, styleNames, IM_ARRAYSIZE(styleNames)))
				{
					m_editorTransitionDesc.barStyle = static_cast<LoadingBarStyle>(currentStyle);
				}

				ImGui::SliderFloat2("Screen Anchor (0..1)", &m_editorTransitionDesc.barNormalizedPos.x, 0.0f, 1.0f, "%.3f");
				ImGui::DragFloat("Bar Length", &m_editorTransitionDesc.barLength, 1.0f, 50.0f, 1600.0f, "%.0f px");
				ImGui::DragFloat("Bar Thickness", &m_editorTransitionDesc.barThickness, 0.5f, 1.0f, 60.0f, "%.1f px");

				ImGui::Checkbox("Show End-Caps", &m_editorTransitionDesc.showEndCaps);
				if (m_editorTransitionDesc.showEndCaps)
				{
					ImGui::SameLine();
					ImGui::SetNextItemWidth(100);
					ImGui::DragFloat("Length##CapLen", &m_editorTransitionDesc.endCapLength, 0.5f, 2.0f, 60.0f, "%.0f px");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(80);
					ImGui::DragFloat("Thick##CapThick", &m_editorTransitionDesc.endCapThickness, 0.5f, 1.0f, 20.0f, "%.1f px");
				}

				ImGui::Checkbox("Show Needle", &m_editorTransitionDesc.showNeedle);
				if (m_editorTransitionDesc.showNeedle)
				{
					ImGui::SameLine();
					ImGui::SetNextItemWidth(100);
					ImGui::DragFloat("Length##NeedleLen", &m_editorTransitionDesc.needleLength, 0.5f, 2.0f, 80.0f, "%.0f px");
					ImGui::SameLine();
					ImGui::SetNextItemWidth(80);
					ImGui::DragFloat("Thick##NeedleThick", &m_editorTransitionDesc.needleThickness, 0.5f, 1.0f, 20.0f, "%.1f px");
					ImGui::DragFloat("Needle Pulse Speed", &m_editorTransitionDesc.needlePulseSpeed, 0.1f, 0.0f, 20.0f, "%.1f");
				}

				if (m_editorTransitionDesc.barStyle == LoadingBarStyle::Segmented)
				{
					ImGui::DragInt("Segment Count", &m_editorTransitionDesc.segmentCount, 1, 2, 60);
				}

				ImGui::Checkbox("Show Percentage Readout", &m_editorTransitionDesc.showPercentageText);

				ImGui::ColorEdit4("Track Color", &m_editorTransitionDesc.barTrackColor.x);
				ImGui::ColorEdit4("Fill Color", &m_editorTransitionDesc.barFillColor.x);
				ImGui::ColorEdit4("Needle Color", &m_editorTransitionDesc.barNeedleColor.x);

				if (m_editorTransitionDesc.barStyle == LoadingBarStyle::CustomTextured ||
					ImGui::TreeNode("Custom Bar Textures (.dds)"))
				{
					auto DrawTexPicker = [&](const char* label, std::wstring& targetPath)
					{
						std::string s(targetPath.begin(), targetPath.end());
						char buf[512] = {};
						strcpy_s(buf, sizeof(buf), s.c_str());
						ImGui::Text("%s:", label);
						if (ImGui::InputText((std::string("##Input") + label).c_str(), buf, sizeof(buf)))
						{
							targetPath = std::wstring(buf, buf + strlen(buf));
						}
						ImGui::SameLine();
						if (ImGui::Button((std::string("Browse##") + label).c_str()))
						{
							OPENFILENAMEW ofn = {};
							wchar_t szFile[MAX_PATH] = {};
							ofn.lStructSize = sizeof(ofn);
							ofn.hwndOwner = nullptr;
							ofn.lpstrFile = szFile;
							ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
							ofn.lpstrFilter = L"DDS Textures (*.dds)\0*.dds\0All Files (*.*)\0*.*\0";
							ofn.nFilterIndex = 1;
							ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

							if (GetOpenFileNameW(&ofn))
							{
								std::filesystem::path p(szFile);
								std::wstring ws = p.lexically_normal().wstring();
								size_t pos = ws.find(L"Resources");
								if (pos != std::wstring::npos) ws = ws.substr(pos);
								for (wchar_t& c : ws) if (c == L'\\') c = L'/';
								targetPath = ws;
							}
						}
						ImGui::SameLine();
						if (ImGui::Button((std::string("Clear##") + label).c_str()))
						{
							targetPath = L"";
						}
					};

					DrawTexPicker("Bar Track Texture", m_editorTransitionDesc.barTrackTexturePath);
					DrawTexPicker("Bar Fill Texture", m_editorTransitionDesc.barFillTexturePath);
					DrawTexPicker("Bar Needle Texture", m_editorTransitionDesc.barNeedleTexturePath);

					if (m_editorTransitionDesc.barStyle != LoadingBarStyle::CustomTextured)
					{
						ImGui::TreePop();
					}
				}
			}
			ImGui::Separator();
		}

		// Lore / Gameplay Tips Controls
		if (ImGui::CollapsingHeader("Lore & Gameplay Tips Cycler", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Show Gameplay Tips", &m_editorTransitionDesc.showTips);
			if (m_editorTransitionDesc.showTips)
			{
				ImGui::SliderFloat2("Tips Anchor (0..1)", &m_editorTransitionDesc.tipNormalizedPos.x, 0.0f, 1.0f, "%.3f");

				ImGui::Text("Active Tips List (%d total):", static_cast<int>(m_editorTransitionDesc.tips.size()));
				int tipToRemove = -1;
				for (size_t i = 0; i < m_editorTransitionDesc.tips.size(); ++i)
				{
					ImGui::PushID(static_cast<int>(i));
					char titleBuf[128] = {};
					char descBuf[512] = {};
					strcpy_s(titleBuf, sizeof(titleBuf), m_editorTransitionDesc.tips[i].title.c_str());
					strcpy_s(descBuf, sizeof(descBuf), m_editorTransitionDesc.tips[i].description.c_str());

					ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
					if (ImGui::InputText("Title", titleBuf, sizeof(titleBuf)))
					{
						m_editorTransitionDesc.tips[i].title = titleBuf;
					}
					ImGui::SameLine();
					if (ImGui::Button("Delete"))
					{
						tipToRemove = static_cast<int>(i);
					}

					if (ImGui::InputTextMultiline("Description", descBuf, sizeof(descBuf), ImVec2(-1, 44)))
					{
						m_editorTransitionDesc.tips[i].description = descBuf;
					}
					ImGui::Separator();
					ImGui::PopID();
				}

				if (tipToRemove >= 0 && tipToRemove < static_cast<int>(m_editorTransitionDesc.tips.size()))
				{
					m_editorTransitionDesc.tips.erase(m_editorTransitionDesc.tips.begin() + tipToRemove);
				}

				if (ImGui::Button("+ Add New Tip"))
				{
					m_editorTransitionDesc.tips.push_back({ "New Tip Title", "Enter helpful gameplay tip or world lore here." });
				}
			}
			ImGui::Separator();
		}

		// Action Buttons
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.65f, 0.35f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.75f, 0.45f, 1.0f));
		if (ImGui::Button("Preview Transition (F5 / Space)", ImVec2(-1, 32)))
		{
			if (m_viewportLayout == EditorViewportLayout::SceneViewOnly)
			{
				SetViewportLayout(EditorViewportLayout::GameViewOnly);
			}
			gameContext.sceneManager->PreviewTransition(m_editorTransitionDesc);
		}
		ImGui::PopStyleColor(2);

		ImGui::Spacing();

		// Test Switch Scene
		auto sceneNames = gameContext.sceneManager->GetRegisteredSceneNames();
		if (sceneNames.empty())
		{
			sceneNames = { "TitleScene", "GameScene" };
		}

		if (m_selectedTargetSceneIndex < 0 || m_selectedTargetSceneIndex >= static_cast<int>(sceneNames.size()))
		{
			m_selectedTargetSceneIndex = 0;
		}

		if (ImGui::BeginCombo("Target Scene", sceneNames[m_selectedTargetSceneIndex].c_str()))
		{
			for (int i = 0; i < static_cast<int>(sceneNames.size()); ++i)
			{
				bool isSelected = (m_selectedTargetSceneIndex == i);
				if (ImGui::Selectable(sceneNames[i].c_str(), isSelected))
				{
					m_selectedTargetSceneIndex = i;
				}
				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.4f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.5f, 0.2f, 1.0f));
		if (ImGui::Button("Test Switch Scene", ImVec2(-1, 28)))
		{
			if (m_viewportLayout == EditorViewportLayout::SceneViewOnly)
			{
				SetViewportLayout(EditorViewportLayout::GameViewOnly);
			}
			if (m_selectedTargetSceneIndex >= 0 && m_selectedTargetSceneIndex < static_cast<int>(sceneNames.size()))
			{
				gameContext.sceneManager->LoadScene(sceneNames[m_selectedTargetSceneIndex], m_editorTransitionDesc);
			}
		}
		ImGui::PopStyleColor(2);

		ImGui::Spacing();

		// Save & Reset Buttons
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.85f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.95f, 1.0f));
		if (ImGui::Button("Save Transition Asset", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 2.0f, 28)))
		{
			gameContext.sceneManager->SetDefaultTransition(m_editorTransitionDesc);
			bool success = gameContext.sceneManager->SaveTransitionConfig();
			if (success)
			{
				m_transitionStatusMessage = "Saved to Resources/Data/SceneTransition.json!";
			}
			else
			{
				m_transitionStatusMessage = "Failed to save SceneTransition.json!";
			}
			m_transitionStatusTimer = 3.5f;
		}
		ImGui::PopStyleColor(2);

		ImGui::SameLine();

		if (ImGui::Button("Reset to Default", ImVec2(-1, 28)))
		{
			m_editorTransitionDesc = gameContext.sceneManager->GetDefaultTransition();
			m_transitionStatusMessage = "Reset to default transition settings.";
			m_transitionStatusTimer = 2.0f;
		}

		if (!m_transitionStatusMessage.empty())
		{
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", m_transitionStatusMessage.c_str());
		}

		// Live-sync editor settings to active/paused transition overlay in real time
		if (gameContext.sceneManager->IsTransitioning())
		{
			gameContext.sceneManager->UpdateActiveTransitionDesc(m_editorTransitionDesc);
		}
	}
}