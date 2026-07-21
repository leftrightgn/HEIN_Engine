#pragma once

struct GameContext;

namespace HEIN
{
    class IScene
    {
    public:
        virtual ~IScene() = default;

        virtual void OnEnter(GameContext& gameContext) {}
        virtual void OnExit(GameContext& gameContext) {}
        virtual void Update(GameContext& gameContext) = 0;
        virtual void Render(GameContext& gameContext) = 0;
    };
}
