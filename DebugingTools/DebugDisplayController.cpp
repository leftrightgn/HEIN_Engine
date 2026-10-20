#include "pch.h"
#include "Framework/GameContext.h"
#include "Common/InputManager.h"
#include "Common/ShadowSystem.h"
#include "DebugDisplayController.h"
#include <Camera/DebugCameraMode.h>
#include <Camera/CameraController.h>
#include "Effect/Skybox.h"
#include <Entities/Actor.h>
#include <Components/UIButtonComponent.h>
#include <Components/GaugeComponent.h>


namespace HEIN
{
	DebugDisplayController::DebugDisplayController()
		: m_isVisible(true)
		, m_isMagnified(true)
	{
	}

	void DebugDisplayController::Initialize()
	{
        m_isVisible = true;
        m_isMagnified = true;
        m_justInitialized = true;
        m_debugUI.SetSceneViewActive(true);

        HEIN::Actor* cameraActor = m_actorManager.CreateActor(L"MainCamera");
        m_debugCameraID = cameraActor->GetID();
        HEIN::CameraController* cameraComp = cameraActor->AddComponent<HEIN::CameraController>();
        cameraActor->Start();

        cameraComp->RegisterCamera(
            HEIN::CameraType::Debug,
            []()
            { return std::make_unique<HEIN::DebugCameraMode>(); }
        );

        cameraComp->SetFirstCamera(CameraType::Debug);
	}

    void DebugDisplayController::Update(const GameContext& gameContext, HEIN::ActorManager& mainActorManager)
    {
        if (m_justInitialized)
        {
            m_justInitialized = false;
        }
        else if (gameContext.keyboardTracker.pressed.F3)
        {
            m_isVisible = !m_isVisible;
        }

        if (!m_isVisible)
        {
            m_isMagnified = false;
            const_cast<GameContext&>(gameContext).showColliders = false;
            if (gameContext.mainCamera != nullptr)
            {
                gameContext.mainCamera->UpdateMouseMode();
            }
            return;
        }

        m_isMagnified = (m_debugUI.GetViewportLayout() != EditorViewportLayout::GameViewOnly);

        HEIN::Actor* cameraActor = m_actorManager.GetActor(m_debugCameraID);
        HEIN::CameraController* cameraComp = nullptr;
        if (cameraActor != nullptr)
        {
            cameraComp = cameraActor->GetComponent<HEIN::CameraController>();
        }

        D3D11_VIEWPORT sceneVp = m_debugUI.GetSceneViewport(gameContext);
        DebugUIManager::SetActiveViewportRect(sceneVp.TopLeftX, sceneVp.TopLeftY, sceneVp.Width, sceneVp.Height);

        if (cameraComp != nullptr)
        {
            float aspectRatio = (sceneVp.Height > 0.0f) ? (sceneVp.Width / sceneVp.Height) : 1.777f;
            DirectX::Mouse::Get().SetMode(DirectX::Mouse::MODE_ABSOLUTE);

            m_projMatrix = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(
                cameraComp->GetFov(),
                aspectRatio,
                0.1f,
                5000.0f
            );
        }

        m_debugUI.Update(gameContext, mainActorManager, GetViewMatrix(), m_projMatrix, cameraComp);
        m_isMagnified = (m_debugUI.GetViewportLayout() != EditorViewportLayout::GameViewOnly);
        const_cast<GameContext&>(gameContext).viewportLayout = m_debugUI.GetViewportLayout();
        const_cast<GameContext&>(gameContext).showUI = m_debugUI.IsShowUIEnabled();
        const_cast<GameContext&>(gameContext).showColliders = m_isMagnified && m_debugUI.IsShowCollidersEnabled();

        const float deltaTime = static_cast<float>(gameContext.timer.GetElapsedSeconds());

        if (m_isMagnified && cameraComp != nullptr)
        {
            CameraInputState debugInput;

            std::pair<int, int> mouseDelta = gameContext.inputManager->GetMouseDelta();
            bool isHeld = gameContext.inputManager->IsDebugDrugHeld(gameContext) || 
                          gameContext.mouseState.rightButton || 
                          gameContext.mouseState.middleButton;

            bool isUICapturingMouse = ImGui::GetIO().WantCaptureMouse || ImGuizmo::IsOver() || ImGuizmo::IsUsing();
            if (isUICapturingMouse)
            {
                isHeld = false;
                mouseDelta = { 0, 0 };
            }
            // -------------------------------

            if (isHeld)
            {
                m_virtualMouseX += static_cast<float>(mouseDelta.first);
                m_virtualMouseY += static_cast<float>(mouseDelta.second);
            }

            debugInput.mouseX = m_virtualMouseX;
            debugInput.mouseY = m_virtualMouseY;
            bool isShiftHeld = gameContext.keyboardState.LeftShift || gameContext.keyboardState.RightShift;

            if (ImGui::GetIO().WantCaptureKeyboard)
            {
                debugInput.movementIntent = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
            }
            else
            {
                debugInput.movementIntent = gameContext.inputManager->GetDebugMoveIntent(gameContext);
                if (isShiftHeld)
                {
                    debugInput.movementIntent *= 2.5f; // Fast move with shift held
                }
            }
            debugInput.isLeftMouseDown = isHeld;
            debugInput.scrollWheelDelta = static_cast<float>(gameContext.mouseState.scrollWheelValue);
            debugInput.ignoreScroll = isUICapturingMouse;
            debugInput.ignoreMovement = ImGui::GetIO().WantCaptureKeyboard;

            cameraComp->ProcessInput(debugInput);
        }

        m_actorManager.UpdateAll(deltaTime);
    }

    void DebugDisplayController::Render(
        GameContext& gameContext,
        HEIN::ActorManager& actorManager, 
        Skybox* skybox,
        DirectX::SimpleMath::Matrix mainView,
        DirectX::SimpleMath::Matrix mainProj
    )
    {
        if (!m_isVisible)
        {
            if (gameContext.debugCollisionRenderer != nullptr) gameContext.debugCollisionRenderer->Clear();
            return;
        }

        EditorViewportLayout layout = m_debugUI.GetViewportLayout();
        gameContext.viewportLayout = layout;
        gameContext.showUI = m_debugUI.IsShowUIEnabled();
        gameContext.showColliders = (layout != EditorViewportLayout::GameViewOnly) && m_debugUI.IsShowCollidersEnabled();

        ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
        ID3D11RenderTargetView* rtv = gameContext.deviceResources.GetRenderTargetView();
        ID3D11DepthStencilView* dsv = gameContext.deviceResources.GetDepthStencilView();
        D3D11_VIEWPORT fullscreen = gameContext.deviceResources.GetScreenViewport();

        // Clear entire backbuffer with editor dark clear color
        const float editorClearColor[4] = { 0.14f, 0.14f, 0.14f, 1.0f };
        context->OMSetRenderTargets(1, &rtv, dsv);
        context->ClearRenderTargetView(rtv, editorClearColor);

        // Debug Camera matrices
        DirectX::SimpleMath::Matrix debugView = DirectX::SimpleMath::Matrix::Identity;
        HEIN::Actor* cameraActor = m_actorManager.GetActor(m_debugCameraID);
        HEIN::CameraController* cameraComp = nullptr;
        if (cameraActor != nullptr)
        {
            cameraComp = cameraActor->GetComponent<HEIN::CameraController>();
            if (cameraComp != nullptr)
            {
                debugView = cameraComp->GetView();
            }
        }

        // Helper: Render Scene View pass (Debug Camera + Colliders + Frustums, NO Game HUD)
        auto RenderSceneViewPass = [&](const D3D11_VIEWPORT& vp)
        {
            DebugUIManager::SetActiveViewportRect(vp.TopLeftX, vp.TopLeftY, vp.Width, vp.Height);
            context->RSSetViewports(1, &vp);
            context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

            float aspectRatio = (vp.Height > 0.0f) ? (vp.Width / vp.Height) : 1.777f;
            float fov = (cameraComp != nullptr) ? cameraComp->GetFov() : (DirectX::XM_PI / 4.0f);
            m_projMatrix = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(fov, aspectRatio, 0.1f, 5000.0f);

            gameContext.isSceneViewPass = true;
            gameContext.showUI = false; // NO Game HUD in Scene View!
            gameContext.showColliders = m_debugUI.IsShowCollidersEnabled();

            if (skybox) skybox->Draw(gameContext, debugView, m_projMatrix);

            if (gameContext.shadowSystem != nullptr)
            {
                ID3D11ShaderResourceView* shadowSRV = gameContext.shadowSystem->GetShadowMapSRV();
                context->PSSetShaderResources(4, 1, &shadowSRV);
                ID3D11SamplerState* shadowSampler = gameContext.shadowSystem->GetShadowSampler();
                context->PSSetSamplers(1, 1, &shadowSampler);
            }

            actorManager.DrawAll(gameContext, debugView, m_projMatrix);

            // Draw frustum and wireframes in Scene View
            DirectX::BoundingFrustum mainCamFrustum(mainProj, false);
            DirectX::SimpleMath::Matrix mainCamWorld;
            if (std::abs(mainView.Determinant()) < 1e-6f)
            {
                mainCamWorld = DirectX::SimpleMath::Matrix::Identity;
            }
            else
            {
                mainCamWorld = mainView.Invert();
            }
            mainCamFrustum.Transform(mainCamFrustum, mainCamWorld);

            DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&mainCamFrustum.Orientation);
            q = DirectX::XMQuaternionNormalize(q);
            DirectX::XMStoreFloat4(&mainCamFrustum.Orientation, q);

            gameContext.debugRenderer->Begin(debugView, m_projMatrix);
            gameContext.debugRenderer->DrawFrustum(mainCamFrustum, DirectX::XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f));
            DirectX::BoundingSphere camEye(mainCamWorld.Translation(), 0.3f);
            gameContext.debugRenderer->DrawSphere(camEye, DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f));
            gameContext.debugRenderer->End();

            if (gameContext.debugCollisionRenderer != nullptr)
            {
                if (m_debugUI.IsShowCollidersEnabled())
                    gameContext.debugCollisionRenderer->RenderAndFlush(context, gameContext.commonStates, debugView, m_projMatrix);
                else
                    gameContext.debugCollisionRenderer->Clear();
            }
        };

        // Helper: Render Game View pass (Main Camera + Game HUD + Scene Transitions, NO Colliders/Gizmos)
        auto RenderGameViewPass = [&](const D3D11_VIEWPORT& vp)
        {
            context->RSSetViewports(1, &vp);
            context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

            float aspectRatio = (vp.Height > 0.0f) ? (vp.Width / vp.Height) : 1.777f;
            float fov = (gameContext.mainCamera != nullptr) ? gameContext.mainCamera->GetFov() : (DirectX::XM_PI / 4.0f);
            DirectX::SimpleMath::Matrix gameProj = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(fov, aspectRatio, 0.1f, 5000.0f);

            bool isTransitioning = (gameContext.sceneManager && gameContext.sceneManager->IsTransitioning() && gameContext.sceneManager->GetTransitionAlpha() > 0.05f);
            bool isTransitionPaused = (gameContext.sceneManager && gameContext.sceneManager->IsTransitionPaused());

            gameContext.isSceneViewPass = false;
            gameContext.showUI = m_debugUI.IsShowUIEnabled() && !isTransitioning; // Suppress HUD underneath active transition
            gameContext.showColliders = false; // NO Colliders in Game View!

            if (skybox) skybox->Draw(gameContext, mainView, gameProj);

            if (gameContext.shadowSystem != nullptr)
            {
                ID3D11ShaderResourceView* shadowSRV = gameContext.shadowSystem->GetShadowMapSRV();
                context->PSSetShaderResources(4, 1, &shadowSRV);
                ID3D11SamplerState* shadowSampler = gameContext.shadowSystem->GetShadowSampler();
                context->PSSetSamplers(1, 1, &shadowSampler);
            }

            actorManager.DrawAll(gameContext, mainView, gameProj);

            if (gameContext.debugCollisionRenderer != nullptr)
            {
                gameContext.debugCollisionRenderer->RenderAndFlush(context, gameContext.commonStates, mainView, gameProj);
            }

            // Render transition overlay ONLY inside the Game View panel
            if (gameContext.sceneManager && gameContext.sceneManager->IsTransitioning())
            {
                gameContext.sceneManager->RenderTransitionOverlay(gameContext, vp);

                // When the transition is PAUSED in the editor and Show UI is enabled,
                // render scene UI elements (UI Text / Buttons / Gauges) ON TOP of the paused transition
                // so the user can visually place UI text over the transition screen!
                if (isTransitionPaused && m_debugUI.IsShowUIEnabled())
                {
                    gameContext.showUI = true;
                    context->RSSetViewports(1, &vp);
                    for (const auto& pair : actorManager.GetAllActors())
                    {
                        HEIN::Actor* actor = pair.second.get();
                        if (!actor || !actor->IsVisible() || !actor->IsActive()) continue;
                        auto uiComps = actor->GetComponents<HEIN::UIButtonComponent>();
                        for (auto* ui : uiComps)
                        {
                            if (ui) ui->Draw(gameContext, DirectX::SimpleMath::Matrix::Identity, mainView, gameProj);
                        }
                        auto gaugeComps = actor->GetComponents<HEIN::GaugeComponent>();
                        for (auto* gauge : gaugeComps)
                        {
                            if (gauge) gauge->Draw(gameContext, DirectX::SimpleMath::Matrix::Identity, mainView, gameProj);
                        }
                    }
                }
            }
        };

        // Execute Viewport Layout Passes
        if (layout == EditorViewportLayout::SceneViewOnly)
        {
            D3D11_VIEWPORT sceneVp = m_debugUI.GetSceneViewport(gameContext);
            RenderSceneViewPass(sceneVp);

            // Optional PiP Preview (disabled by default)
            if (m_debugUI.IsViewportPreviewEnabled() && m_debugUI.IsViewportVisibleInUI())
            {
                D3D11_VIEWPORT camPreviewViewport;
                camPreviewViewport.TopLeftX = m_debugUI.GetViewportPos().x;
                camPreviewViewport.TopLeftY = m_debugUI.GetViewportPos().y;
                camPreviewViewport.Width = m_debugUI.GetViewportSize().x;
                camPreviewViewport.Height = m_debugUI.GetViewportSize().y;
                camPreviewViewport.MinDepth = 0.0f;
                camPreviewViewport.MaxDepth = 1.0f;
                RenderGameViewPass(camPreviewViewport);
            }
        }
        else if (layout == EditorViewportLayout::GameViewOnly)
        {
            D3D11_VIEWPORT gameVp = m_debugUI.GetGameViewport(gameContext);
            RenderGameViewPass(gameVp);
        }
        else if (layout == EditorViewportLayout::SplitSideBySide)
        {
            D3D11_VIEWPORT sceneVp = m_debugUI.GetSceneViewport(gameContext);
            D3D11_VIEWPORT gameVp = m_debugUI.GetGameViewport(gameContext);

            // Left half: Scene View
            RenderSceneViewPass(sceneVp);

            // Right half: Game View
            RenderGameViewPass(gameVp);
        }

        // Reset viewport to fullscreen for ImGui UI draw pass
        context->RSSetViewports(1, &fullscreen);

        // Unbind shadow SRV and sampler to eliminate pipeline hazards across frame boundaries
        ID3D11ShaderResourceView* nullSRV = nullptr;
        context->PSSetShaderResources(4, 1, &nullSRV);
        ID3D11SamplerState* nullSampler = nullptr;
        context->PSSetSamplers(1, 1, &nullSampler);

        // Draw Editor UI
        ImGuizmo::BeginFrame();
        if (layout == EditorViewportLayout::GameViewOnly)
        {
            m_currentAction = m_debugUI.Draw(gameContext, actorManager, mainView, mainProj);
        }
        else
        {
            m_currentAction = m_debugUI.Draw(gameContext, actorManager, debugView, m_projMatrix);
        }
        m_isMagnified = (m_debugUI.GetViewportLayout() != EditorViewportLayout::GameViewOnly);
    }

	const DirectX::SimpleMath::Matrix DebugDisplayController::GetViewMatrix() const
	{
        HEIN::Actor* cameraActor = const_cast<HEIN::ActorManager&>(m_actorManager).GetActor(m_debugCameraID);
        if (cameraActor != nullptr)
        {
            HEIN::CameraController* cameraComp = cameraActor->GetComponent<HEIN::CameraController>();
            if (cameraComp != nullptr)
            {
                return cameraComp->GetView();
            }
        }
        return DirectX::SimpleMath::Matrix::Identity;
	}

	const DirectX::SimpleMath::Matrix DebugDisplayController::GetProjMatrix() const
	{
		return m_projMatrix;
	}

    void DebugDisplayController::SetDebugTargets(HEIN::ActorID playerID, HEIN::ActorID swordID, HEIN::ActorID axeID, HEIN::ActorID stageID, HEIN::ActorID enemyID)
    {
        m_debugPlayerID = playerID;
        m_debugSwordID = swordID;
        m_debugStageID = stageID;
        m_debugEnemyID = enemyID;
        m_debugAxeID = axeID;
    }


}
