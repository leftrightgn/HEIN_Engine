#include "pch.h"
#include "InputManager.h"
#include "Camera/CameraController.h"
#include "Framework/GameContext.h"
#include <Message/Messenger.h>
#include <Message/Message.h>

namespace HEIN
{
	void InputManager::InitializeDefaultBindings()
	{
		if (s_bindingsInitialized) return;
		s_bindingsInitialized = true;
		s_actionBindings.clear();

		s_actionBindings[InputAction::MoveForward] = { InputBinding::FromKey(DirectX::Keyboard::Keys::W) };
		s_actionBindings[InputAction::MoveBackward] = { InputBinding::FromKey(DirectX::Keyboard::Keys::S) };
		s_actionBindings[InputAction::MoveLeft] = { InputBinding::FromKey(DirectX::Keyboard::Keys::A) };
		s_actionBindings[InputAction::MoveRight] = { InputBinding::FromKey(DirectX::Keyboard::Keys::D) };

		s_actionBindings[InputAction::Attack] = { InputBinding::FromMouse(InputMouseButton::Left) };
		s_actionBindings[InputAction::Block] = { InputBinding::FromMouse(InputMouseButton::Right) };
		s_actionBindings[InputAction::Dodge] = { InputBinding::FromKey(DirectX::Keyboard::Keys::Space) };
		s_actionBindings[InputAction::Jump] = { InputBinding::FromKey(DirectX::Keyboard::Keys::F) };
		s_actionBindings[InputAction::ToggleModel] = { InputBinding::FromKey(DirectX::Keyboard::Keys::M) };
		s_actionBindings[InputAction::Possess] = { InputBinding::FromKey(DirectX::Keyboard::Keys::Q) };

		s_actionBindings[InputAction::CameraLockOn] = { InputBinding::FromMouse(InputMouseButton::Middle) };
		s_actionBindings[InputAction::CameraToggleDebug] = { InputBinding::FromKey(DirectX::Keyboard::Keys::Q) };
		s_actionBindings[InputAction::CameraFirstPerson] = { InputBinding::FromKey(DirectX::Keyboard::Keys::P) };
		s_actionBindings[InputAction::CameraThirdPerson] = { InputBinding::FromKey(DirectX::Keyboard::Keys::T) };
		s_actionBindings[InputAction::CameraSpring] = { InputBinding::FromKey(DirectX::Keyboard::Keys::E) };

		s_actionBindings[InputAction::EditorToggle] = { InputBinding::FromKey(DirectX::Keyboard::Keys::F3) };
		s_actionBindings[InputAction::HitboxDebugToggle] = { InputBinding::FromKey(DirectX::Keyboard::Keys::F1) };
		s_actionBindings[InputAction::Escape] = { InputBinding::FromKey(DirectX::Keyboard::Keys::Escape) };
	}

	void InputManager::BindAction(InputAction action, DirectX::Keyboard::Keys key)
	{
		InitializeDefaultBindings();
		s_actionBindings[action].push_back(InputBinding::FromKey(key));
	}

	void InputManager::BindAction(InputAction action, InputMouseButton button)
	{
		InitializeDefaultBindings();
		s_actionBindings[action].push_back(InputBinding::FromMouse(button));
	}

	void InputManager::ClearBindings(InputAction action)
	{
		InitializeDefaultBindings();
		s_actionBindings[action].clear();
	}

	bool InputManager::IsActionDown(const GameContext& gameContext, InputAction action)
	{
		InitializeDefaultBindings();
		auto it = s_actionBindings.find(action);
		if (it == s_actionBindings.end()) return false;

		for (const auto& binding : it->second)
		{
			if (binding.key != DirectX::Keyboard::Keys::None)
			{
				if (gameContext.keyboardState.IsKeyDown(binding.key)) return true;
			}
			if (binding.mouseButton != InputMouseButton::None)
			{
				switch (binding.mouseButton)
				{
				case InputMouseButton::Left:   if (gameContext.mouseState.leftButton) return true; break;
				case InputMouseButton::Right:  if (gameContext.mouseState.rightButton) return true; break;
				case InputMouseButton::Middle: if (gameContext.mouseState.middleButton) return true; break;
				default: break;
				}
			}
		}
		return false;
	}

	bool InputManager::WasActionPressed(const GameContext& gameContext, InputAction action)
	{
		InitializeDefaultBindings();
		auto it = s_actionBindings.find(action);
		if (it == s_actionBindings.end()) return false;

		for (const auto& binding : it->second)
		{
			if (binding.key != DirectX::Keyboard::Keys::None)
			{
				if (gameContext.keyboardTracker.IsKeyPressed(binding.key)) return true;
			}
			if (binding.mouseButton != InputMouseButton::None)
			{
				switch (binding.mouseButton)
				{
				case InputMouseButton::Left:
					if (gameContext.mouseButtonTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED) return true;
					break;
				case InputMouseButton::Right:
					if (gameContext.mouseButtonTracker.rightButton == DirectX::Mouse::ButtonStateTracker::PRESSED) return true;
					break;
				case InputMouseButton::Middle:
					if (gameContext.mouseButtonTracker.middleButton == DirectX::Mouse::ButtonStateTracker::PRESSED) return true;
					break;
				default: break;
				}
			}
		}
		return false;
	}

	bool InputManager::WasActionReleased(const GameContext& gameContext, InputAction action)
	{
		InitializeDefaultBindings();
		auto it = s_actionBindings.find(action);
		if (it == s_actionBindings.end()) return false;

		for (const auto& binding : it->second)
		{
			if (binding.key != DirectX::Keyboard::Keys::None)
			{
				if (gameContext.keyboardTracker.IsKeyReleased(binding.key)) return true;
			}
			if (binding.mouseButton != InputMouseButton::None)
			{
				switch (binding.mouseButton)
				{
				case InputMouseButton::Left:
					if (gameContext.mouseButtonTracker.leftButton == DirectX::Mouse::ButtonStateTracker::RELEASED) return true;
					break;
				case InputMouseButton::Right:
					if (gameContext.mouseButtonTracker.rightButton == DirectX::Mouse::ButtonStateTracker::RELEASED) return true;
					break;
				case InputMouseButton::Middle:
					if (gameContext.mouseButtonTracker.middleButton == DirectX::Mouse::ButtonStateTracker::RELEASED) return true;
					break;
				default: break;
				}
			}
		}
		return false;
	}

	void InputManager::Update(const GameContext& gameContext)
	{
		if (gameContext.mouseState.positionMode == DirectX::Mouse::MODE_RELATIVE)
		{
			m_deltaX = gameContext.mouseState.x;
			m_deltaY = gameContext.mouseState.y;
		}
		else
		{
			m_deltaX = gameContext.mouseState.x - m_lastMouseX;
			m_deltaY = gameContext.mouseState.y - m_lastMouseY;
		}
		
		m_lastMouseX = gameContext.mouseState.x;
		m_lastMouseY = gameContext.mouseState.y;
	}

	std::pair<int, int> InputManager::GetMouseDelta()
	{
		return std::make_pair(m_deltaX, m_deltaY);
	}
	
	bool InputManager::IsDebugDrugHeld(const GameContext& gameContext)
	{
		return gameContext.mouseState.leftButton;
	}

	DirectX::SimpleMath::Vector3 InputManager::GetDebugMoveIntent(const GameContext& gameContext)
	{
		DirectX::SimpleMath::Vector3 intent = DirectX::SimpleMath::Vector3::Zero;

		if (IsActionDown(gameContext, InputAction::MoveForward)) intent.z -= 1.0f;
		if (IsActionDown(gameContext, InputAction::MoveBackward)) intent.z += 1.0f;
		if (IsActionDown(gameContext, InputAction::MoveLeft)) intent.x -= 1.0f;
		if (IsActionDown(gameContext, InputAction::MoveRight)) intent.x += 1.0f;
		if (gameContext.keyboardState.Up) intent.y += 1.0f;
		if (gameContext.keyboardState.Down) intent.y -= 1.0f;

		return intent;
	}
	
	bool InputManager::WasCameraSwitchPressed(const GameContext& gameContext, HEIN::CameraType& outType)
	{
		if (WasActionPressed(gameContext, InputAction::CameraThirdPerson))
		{
			outType = HEIN::CameraType::ThirdPerson;
			return true;
		}
		if (WasActionPressed(gameContext, InputAction::CameraFirstPerson))
		{
			outType = HEIN::CameraType::FirstPerson;
			return true;
		}
		if (WasActionPressed(gameContext, InputAction::CameraSpring))
		{
			outType = HEIN::CameraType::Spring;
			return true;
		}
		if (WasActionPressed(gameContext, InputAction::CameraToggleDebug))
		{
			outType = HEIN::CameraType::Debug;
			return true;
		}
		if (WasActionPressed(gameContext, InputAction::CameraLockOn))
		{
			outType = HEIN::CameraType::LockOn;
			return true;
		}
		return false;
	}

	bool InputManager::WasDebugMagnifyPressed(const GameContext& gameContext)
	{
		return gameContext.keyboardTracker.pressed.F2;
	}

	bool InputManager::WasDebugTogglePressed(const GameContext& gameContext)
	{
		return WasActionPressed(gameContext, InputAction::EditorToggle);
	}

	void InputManager::BroadCastPlayerInput(const GameContext& gameContext, HEIN::ActorID playerID)
	{
		Messenger* messenger = Messenger::GetInstance();
		if (!messenger) return;

		bool isMoving = false;

		if (IsActionDown(gameContext, InputAction::MoveForward))
		{
			messenger->Notify(playerID, Message::PLAYER_MOVE_FORWARD);
			isMoving = true;
		}
		if (IsActionDown(gameContext, InputAction::MoveBackward))
		{
			messenger->Notify(playerID, Message::PLAYER_MOVE_BACKWARD);
			isMoving = true;
		}
		if (IsActionDown(gameContext, InputAction::MoveLeft))
		{
			messenger->Notify(playerID, Message::PLAYER_MOVE_LEFT);
			isMoving = true;
		}
		if (IsActionDown(gameContext, InputAction::MoveRight))
		{
			messenger->Notify(playerID, Message::PLAYER_MOVE_RIGHT);
			isMoving = true;
		}

		if (!isMoving)
		{
			messenger->Notify(playerID, Message::PLAYER_STOP_MOVEMENT);
		}

		if (WasActionPressed(gameContext, InputAction::Attack))
		{
			messenger->Notify(playerID, Message::PLAYER_ACTION_ATTACK);
		}

		if (WasActionPressed(gameContext, InputAction::Dodge))
		{
			messenger->Notify(playerID, Message::PLAYER_ACTION_DODGE);
		}

		if (WasActionPressed(gameContext, InputAction::Block))
		{
			messenger->Notify(playerID, Message::PLAYER_ACTION_BLOCK);
		}
		else if (WasActionReleased(gameContext, InputAction::Block))
		{
			messenger->Notify(playerID, Message::PLAYER_STOP_BLOCK);
		}

		if (WasActionPressed(gameContext, InputAction::ToggleModel))
		{
			messenger->Notify(playerID, Message::PLAYER_TOGGLE_MODEL);
		}

		if (WasActionPressed(gameContext, InputAction::Jump))
		{
			messenger->Notify(playerID, Message::PLAYER_ACTION_JUMP);
		}
	}
}
