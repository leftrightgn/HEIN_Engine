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


/**
 * @brief A basic game implementation that creates a D3D11 device and provides a game loop.
 */
class Game final : public DX::IDeviceNotify
{
public:

    Game() noexcept(false);
    ~Game();

    Game(Game&&) = default;
    Game& operator= (Game&&) = default;

    Game(Game const&) = delete;
    Game& operator= (Game const&) = delete;

    /**
     * @brief Initialization and management
     * @param window The window handle
     * @param width The width of the window
     * @param height The height of the window
     */
    void Initialize(HWND window, int width, int height);

    /**
     * @brief Basic game loop
     */
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

    /**
     * @brief Registers a scene with the scene manager.
     * @tparam TScene The scene type to register
     * @param name The name of the scene
     */
    template <class TScene>
    void RegisterScene(const std::string& name)
    {
        m_sceneManager.RegisterScene<TScene>(name);
    }

    /**
     * @brief Loads a scene. Defaults to Single mode if the user doesn't specify.
     * @param name The name of the scene to load
     * @param mode The load scene mode (defaults to Single)
     */
    void LoadScene(const std::string& name, HEIN::LoadSceneMode mode = HEIN::LoadSceneMode::Single)
    {
        m_sceneManager.LoadScene(name, mode);
    }

    /**
     * @brief Unloads a scene.
     * @param name The name of the scene to unload
     */
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

    /// @brief Device resources.
    std::unique_ptr<DX::DeviceResources>    m_deviceResources;

    /// @brief Rendering loop timer.
    DX::StepTimer                           m_timer;

    // --------------------------------------------------------------------- //

private:

    /// @brief Keyboard state tracker
    DirectX::Keyboard::KeyboardStateTracker m_keyboardTracker;

    /// @brief Mouse button state tracker
    DirectX::Mouse::ButtonStateTracker m_mouseButtonTracker;

    /// @brief Common states
    std::unique_ptr<DirectX::CommonStates> m_states;

    /// @brief Game context
    std::optional<GameContext> m_gameContext;

    /// @brief Scene manager
    HEIN::SceneManager m_sceneManager;

    /// @brief Input manager
    HEIN::InputManager m_inputManager;
    
    /// @brief Debug renderer
    HEIN::DebugRenderer m_debugRenderer;
    
    /// @brief Debug collision renderer
    HEIN::DebugCollisionRenderer m_debugCollisionRenderer;
    
    /// @brief Event manager
    HEIN::EventManager m_eventManager;

};
