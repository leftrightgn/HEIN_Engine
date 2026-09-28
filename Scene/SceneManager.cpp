#include "pch.h"
#include "SceneManager.h"

namespace HEIN
{
    void SceneManager::LoadScene(const std::string& sceneName, LoadSceneMode mode)
    {
        m_pendingLoads.push_back({ sceneName, mode });
    }

    void SceneManager::UnloadScene(const std::string& sceneName)
    {
        m_pendingUnloads.push_back(sceneName);
    }

    void SceneManager::Update(GameContext& context)
    {
        // Process pending scene unloads
        for (const std::string& nameToUnload : m_pendingUnloads)
        {
            for (auto it = m_activeScenes.begin(); it != m_activeScenes.end(); )
            {
                if (it->name == nameToUnload)
                {
                    it->instance->OnExit(context);
                    it = m_activeScenes.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }
        m_pendingUnloads.clear();

        // Process pending scene loads
        for (const PendingLoad& load : m_pendingLoads)
        {
            // Destroy currently active scenes if loading in Single mode
            if (load.mode == LoadSceneMode::Single)
            {
                for (auto& active : m_activeScenes)
                {
                    active.instance->OnExit(context);
                }
                m_activeScenes.clear();
            }

            // Instantiate registered scene instance
            auto it = m_sceneRegistry.find(load.sceneName);
            if (it != m_sceneRegistry.end())
            {
                ActiveScene newScene;
                newScene.name = load.sceneName;
                newScene.instance = it->second();

                newScene.instance->OnEnter(context);
                m_activeScenes.push_back(std::move(newScene));
            }
        }
        m_pendingLoads.clear();

        // Update all currently active scene instances
        for (auto& active : m_activeScenes)
        {
            active.instance->Update(context);
        }
    }

    void SceneManager::Render(GameContext& context)
    {
        // Render from index 0 to the end. 
        // Additive scenes (loaded later) will be drawn on top of the Single (base) scene!
        for (auto& active : m_activeScenes)
        {
            active.instance->Render(context);
        }
    }
}