#pragma once
#include "IScene.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <vector>

namespace HEIN
{
    // Mirrors Unity's LoadSceneMode
    enum class LoadSceneMode
    {
        Single,
        Additive
    };

    class SceneManager
    {
    private:
        using SceneFactory = std::function<std::unique_ptr<IScene>()>;

        // Keeps track of an instantiated scene and its name
        struct ActiveScene
        {
            std::string name;
            std::unique_ptr<IScene> instance;
        };

        // Command queue for loading/unloading safely
        struct PendingLoad
        {
            std::string sceneName;
            LoadSceneMode mode;
        };

        std::unordered_map<std::string, SceneFactory> m_sceneRegistry;
        std::vector<ActiveScene> m_activeScenes;

        std::vector<PendingLoad> m_pendingLoads;
        std::vector<std::string> m_pendingUnloads;

    public:
        SceneManager() = default;
        ~SceneManager() = default;

        template <class TScene>
        void RegisterScene(const std::string& sceneName)
        {
            m_sceneRegistry[sceneName] = []() { return std::make_unique<TScene>(); };
        }

        void LoadScene(const std::string& sceneName, LoadSceneMode mode = LoadSceneMode::Single);
        void UnloadScene(const std::string& sceneName);

        void Update(GameContext& context);
        void Render(GameContext& context);
    };
}