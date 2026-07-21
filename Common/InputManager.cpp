#include "pch.h"
#include "InputManager.h"
#include "Camera/CameraController.h"
#include "Framework/GameContext.h"
#include <Message/Messenger.h>
#include <Message/Message.h>

namespace HEIN
{
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

		if (gameContext.keyboardState.W) intent.z -= 1.0f;
		if (gameContext.keyboardState.S) intent.z += 1.0f;
		if (gameContext.keyboardState.A) intent.x -= 1.0f;
		if (gameContext.keyboardState.D) intent.x += 1.0f;
		if (gameContext.keyboardState.Up) intent.y += 1.0f;
		if (gameContext.keyboardState.Down) intent.y -= 1.0f;

		return intent;
	}
	
	bool InputManager::WasCameraSwitchPressed(const GameContext& gameContext, HEIN::CameraType& outType)
	{
		if (gameContext.keyboardTracker.pressed.T)
		{
			outType = HEIN::CameraType::ThirdPerson;
			return true;
		}
		if (gameContext.keyboardTracker.pressed.P)
		{
			outType = HEIN::CameraType::FirstPerson;
			return true;
		}
		if (gameContext.keyboardTracker.pressed.E)
		{
			outType = HEIN::CameraType::Spring;
			return true;
		}
		if (gameContext.keyboardTracker.pressed.Q)
		{
			outType = HEIN::CameraType::Debug;
			return true;
		}
		if (gameContext.mouseButtonTracker.middleButton)
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
		return gameContext.keyboardTracker.pressed.F3;
	}

	void InputManager::BroadCastPlayerInput(const GameContext& gameContext, HEIN::ActorID playerID)
	{
		Messenger* messenger = Messenger::GetInstance();

		bool isMoving = false;

		if (gameContext.keyboardState.W)
		{
			messenger->Notify(playerID, Message::PLAYER_MOVE_FORWARD);
			isMoving = true;
		}
		if (gameContext.keyboardState.S)
		{
			messenger->Notify(playerID, Message::PLAYER_MOVE_BACKWARD);
			isMoving = true;
		}
		if (gameContext.keyboardState.A)
		{
			messenger->Notify(playerID, Message::PLAYER_MOVE_LEFT);
			isMoving = true;
		}
		if (gameContext.keyboardState.D)
		{
			messenger->Notify(playerID, Message::PLAYER_MOVE_RIGHT);
			isMoving = true;
		}

		if (!isMoving)
		{
			messenger->Notify(playerID, Message::PLAYER_STOP_MOVEMENT);
		}

		if (gameContext.mouseButtonTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
		{
			messenger->Notify(playerID, Message::PLAYER_ACTION_ATTACK);
		}

		if (gameContext.keyboardTracker.pressed.Space)
		{
			messenger->Notify(playerID, Message::PLAYER_ACTION_DODGE);
		}

		if (gameContext.mouseButtonTracker.rightButton == DirectX::Mouse::ButtonStateTracker::PRESSED)
		{
			messenger->Notify(playerID, Message::PLAYER_ACTION_BLOCK);
		}
		else if (gameContext.mouseButtonTracker.rightButton == DirectX::Mouse::ButtonStateTracker::RELEASED)
		{
			messenger->Notify(playerID, Message::PLAYER_STOP_BLOCK);
		}
	}
	
}

