#pragma once
#include <SimpleMath.h>
#include <Entities/Actor.h>
#include <vector>
#include <unordered_map>
#include <Keyboard.h>
#include <Mouse.h>

struct GameContext;
namespace HEIN
{
	enum class CameraType;

	enum class InputAction
	{
		MoveForward,
		MoveBackward,
		MoveLeft,
		MoveRight,
		Attack,
		Block,
		Dodge,
		Jump,
		ToggleModel,
		Possess,
		CameraLockOn,
		CameraToggleDebug,
		CameraFirstPerson,
		CameraThirdPerson,
		CameraSpring,
		EditorToggle,
		HitboxDebugToggle,
		Escape
	};

	enum class InputMouseButton
	{
		None,
		Left,
		Right,
		Middle
	};

	struct InputBinding
	{
		DirectX::Keyboard::Keys key = DirectX::Keyboard::Keys::None;
		InputMouseButton mouseButton = InputMouseButton::None;

		static InputBinding FromKey(DirectX::Keyboard::Keys k)
		{
			InputBinding b;
			b.key = k;
			b.mouseButton = InputMouseButton::None;
			return b;
		}

		static InputBinding FromMouse(InputMouseButton m)
		{
			InputBinding b;
			b.key = DirectX::Keyboard::Keys::None;
			b.mouseButton = m;
			return b;
		}
	};

	class InputManager
	{
	private:
		inline static int m_lastMouseX = 0;
		inline static int m_lastMouseY = 0;

		inline static int m_deltaX = 0;
		inline static int m_deltaY = 0;

		inline static std::unordered_map<InputAction, std::vector<InputBinding>> s_actionBindings;
		inline static bool s_bindingsInitialized = false;

	public:
		InputManager() = default;
		~InputManager() = default;

		static void InitializeDefaultBindings();
		static void BindAction(InputAction action, DirectX::Keyboard::Keys key);
		static void BindAction(InputAction action, InputMouseButton button);
		static void ClearBindings(InputAction action);

		static bool IsActionDown(const GameContext& gameContext, InputAction action);
		static bool WasActionPressed(const GameContext& gameContext, InputAction action);
		static bool WasActionReleased(const GameContext& gameContext, InputAction action);

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
