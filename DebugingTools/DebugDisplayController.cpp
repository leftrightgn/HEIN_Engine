#include "pch.h"
#include "Framework/GameContext.h"
#include "Common/InputManager.h"
#include "DebugDisplayController.h"
#include <Camera/DebugCameraMode.h>
#include <Camera/CameraController.h>
#include "Effect/Skybox.h"
#include <Entities/Actor.h>


namespace HEIN
{
	DebugDisplayController::DebugDisplayController()
		: m_isVisible(true)
		, m_isMagnified(false)
	{
	}

	void DebugDisplayController::Initialize()
	{
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
        if (gameContext.keyboardTracker.pressed.F2) m_isMagnified = !m_isMagnified;
        if (gameContext.keyboardTracker.pressed.F3) m_isVisible = !m_isVisible;

        m_debugUI.Update(gameContext, mainActorManager, GetViewMatrix(), m_projMatrix);

        const float deltaTime = static_cast<float>(gameContext.timer.GetElapsedSeconds());

        HEIN::Actor* cameraActor = m_actorManager.GetActor(m_debugCameraID);
        HEIN::CameraController* cameraComp = nullptr;
        if (cameraActor != nullptr)
        {
            cameraComp = cameraActor->GetComponent<HEIN::CameraController>();
        }

        if (m_isMagnified && cameraComp != nullptr)
        {
            CameraInputState debugInput;

            std::pair<int, int> mouseDelta = gameContext.inputManager->GetMouseDelta();
            bool isHeld = gameContext.inputManager->IsDebugDrugHeld(gameContext);

            if (ImGui::GetIO().WantCaptureMouse || ImGuizmo::IsOver() || ImGuizmo::IsUsing())
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

            if (isShiftHeld)
            {
                debugInput.movementIntent = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
            }
            else
            {
                debugInput.movementIntent = gameContext.inputManager->GetDebugMoveIntent(gameContext);
            }
            debugInput.isLeftMouseDown = isHeld;
            debugInput.scrollWheelDelta = static_cast<float>(gameContext.mouseState.scrollWheelValue);

            cameraComp->ProcessInput(debugInput);
        }

        m_actorManager.UpdateAll(deltaTime);

        if (cameraComp != nullptr)
        {
            float aspectRatio;

            if (m_isMagnified)
            {
                D3D11_VIEWPORT viewport = gameContext.deviceResources.GetScreenViewport();
                aspectRatio = static_cast<float>(viewport.Width) / static_cast<float>(viewport.Height);
                DirectX::Mouse::Get().SetMode(DirectX::Mouse::MODE_ABSOLUTE);
            }
            else
            {
                aspectRatio = 400.0f / 225.0f;
                if (gameContext.mainCamera != nullptr)
                {
                    gameContext.mainCamera->UpdateMouseMode();
                }
            }

            m_projMatrix = DirectX::SimpleMath::Matrix::CreatePerspectiveFieldOfView(
                cameraComp->GetFov(),
                aspectRatio,
                0.1f,
                1000.0f
            );
        }
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

        ID3D11DeviceContext* context = gameContext.deviceResources.GetD3DDeviceContext();
        ID3D11DepthStencilView* dsv = gameContext.deviceResources.GetDepthStencilView();
        D3D11_VIEWPORT fullscreen = gameContext.deviceResources.GetScreenViewport();
        D3D11_VIEWPORT debugViewport;

        if (m_isMagnified)
        {
            debugViewport = fullscreen;
            ID3D11RenderTargetView* rtv = gameContext.deviceResources.GetRenderTargetView();
            const float clearColor[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
            context->ClearRenderTargetView(rtv, clearColor);
        }
        else
        {
            debugViewport.Width = 400.0f;
            debugViewport.Height = 225.0f;
            debugViewport.TopLeftX = fullscreen.Width - debugViewport.Width - 20.0f;
            debugViewport.TopLeftY = 20.0f;
            debugViewport.MinDepth = 0.0f;
            debugViewport.MaxDepth = 1.0f;
        }

        context->RSSetViewports(1, &debugViewport);
        context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

        DirectX::SimpleMath::Matrix view = DirectX::SimpleMath::Matrix::Identity;
        HEIN::Actor* cameraActor = m_actorManager.GetActor(m_debugCameraID);
        if (cameraActor != nullptr)
        {
            HEIN::CameraController* cameraComp = cameraActor->GetComponent<HEIN::CameraController>();
            view = cameraComp->GetView();
        }

        if (skybox && m_isMagnified) skybox->Draw(gameContext, view, m_projMatrix);

        actorManager.DrawAll(gameContext, view, m_projMatrix);

        DirectX::BoundingFrustum mainCamFrustum(mainProj, false);
        DirectX::SimpleMath::Matrix mainCamWorld = mainView.Invert();
        mainCamFrustum.Transform(mainCamFrustum, mainCamWorld);
        gameContext.debugRenderer->Begin(view, m_projMatrix);
        gameContext.debugRenderer->DrawFrustum(mainCamFrustum, DirectX::XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f));
        DirectX::BoundingSphere camEye(mainCamWorld.Translation(), 0.3f);
        gameContext.debugRenderer->DrawSphere(camEye, DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f));
        gameContext.debugRenderer->End();

        if (gameContext.debugCollisionRenderer != nullptr)
        {
            gameContext.debugCollisionRenderer->RenderAndFlush(context, gameContext.commonStates, view, m_projMatrix);
        }
        context->RSSetViewports(1, &fullscreen);

        if (m_isMagnified)
        {
            ImGuizmo::BeginFrame();

           m_currentAction = m_debugUI.Draw(actorManager, view, m_projMatrix);
        }
        else
        {
            m_currentAction = HEIN::EditorAction::None;
        }
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
