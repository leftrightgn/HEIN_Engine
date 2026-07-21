#pragma once
#include <SimpleMath.h>
#include <Entities/Actor.h>

struct GameContext;
namespace HEIN
{
	enum class CameraType;

	class InputManager
	{
	private:

		inline static int m_lastMouseX = 0;
		inline static int m_lastMouseY = 0;

		inline static int m_deltaX = 0;
		inline static int m_deltaY = 0;
	public:

		InputManager() = default;
		~InputManager() = default;

		static void Update(const GameContext& gameContext);

		static std::pair<int, int> GetMouseDelta();

		static bool IsDebugDrugHeld(const GameContext& gameContext);


		static DirectX::SimpleMath::Vector3 GetDebugMoveIntent(const GameContext& gameContext);
 
		static bool WasCameraSwitchPressed(const GameContext& gameContext, HEIN::CameraType& outType);
		static bool WasDebugMagnifyPressed(const GameContext& gameContext);
		static bool WasDebugTogglePressed(const GameContext& gameContext);
		
		static void BroadCastPlayerInput(const GameContext& gameContext, HEIN::ActorID playerID);
	};
}
