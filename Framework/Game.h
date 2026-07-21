//
// Game.h
//

#pragma once

#include "Common/DeviceResources.h"
#include "Common/StepTimer.h"

#include <memory>
#include <optional>
#include <Scene/SceneManager.h>
#include "DebugingTools/DebugRenderer.h"
#include "Common/InputManager.h"
#include "DebugingTools/DebugCollisionRenderer.h"
#include "Common/EventManager.h"
#include "GameContext.h"


// A basic game implementation that creates a D3D11 device and
// provides a game loop.
class Game final : public DX::IDeviceNotify
{
public:

    Game() noexcept(false);
    ~Game();

    Game(Game&&) = default;
    Game& operator= (Game&&) = default;

    Game(Game const&) = delete;
    Game& operator= (Game const&) = delete;

    // Initialization and management
    void Initialize(HWND window, int width, int height);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    void OnDeviceLost() override;
    void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowMoved();
    void OnDisplayChange();
    void OnWindowSizeChanged(int width, int height);

    // Properties
    void GetDefaultSize( int& width, int& height ) const noexcept;

    template <class TScene>
    void RegisterScene(const std::string& name)
    {
        m_sceneManager.RegisterScene<TScene>(name);
    }

    // Default to Single mode if the user doesn't specify
    void LoadScene(const std::string& name, HEIN::LoadSceneMode mode = HEIN::LoadSceneMode::Single)
    {
        m_sceneManager.LoadScene(name, mode);
    }

    void UnloadScene(const std::string& name)
    {
        m_sceneManager.UnloadScene(name);
    }

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    // Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer                           m_timer;

    // --------------------------------------------------------------------- //

private:

    // キーボードトラッカー
    DirectX::Keyboard::KeyboardStateTracker m_keyboardTracker;

    // マウスボタントラッカー
    DirectX::Mouse::ButtonStateTracker m_mouseButtonTracker;

    // コモンステート
    std::unique_ptr<DirectX::CommonStates> m_states;

    // ゲームコンテキスト
    std::optional<GameContext> m_gameContext;

    // シーンマネージャー
    HEIN::SceneManager m_sceneManager;

    HEIN::InputManager m_inputManager;
    HEIN::DebugRenderer m_debugRenderer;
    HEIN::DebugCollisionRenderer m_debugCollisionRenderer;
    HEIN::EventManager m_eventManager;

};
