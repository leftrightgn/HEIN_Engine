//--------------------------------------------------------------------------------------
// File: GameContext.h
//
// シーンへ渡すゲームコンテキストクラス
//
// Date: 2026.3.3
// Author: Hideyasu Imase
//--------------------------------------------------------------------------------------
#pragma once

#include "Common/StepTimer.h"
#include "Common/DeviceResources.h"
#include "DebugingTools/DebugRenderer.h"
#include "DebugingTools/DebugCollisionRenderer.h"
#include "Common/EventManager.h"

namespace HEIN { 
	class CameraController; 
	class InputManager;
}

// 各シーンに渡す共通リソースを記述してください
struct GameContext
{
	// ステップタイマー
	DX::StepTimer& timer;

	// デバイスリソース
	DX::DeviceResources& deviceResources;

	// キーボードステートトラッカー
	DirectX::Keyboard::KeyboardStateTracker& keyboardTracker;

	// マウスステートトラッカー
	DirectX::Mouse::ButtonStateTracker& mouseButtonTracker;

	// コモンステート
	DirectX::CommonStates& commonStates;


	DirectX::Mouse::State mouseState;

	DirectX::Keyboard::State keyboardState;
	
	HEIN::DebugRenderer* debugRenderer = nullptr;

	HEIN::InputManager* inputManager = nullptr;

	HEIN::DebugCollisionRenderer* debugCollisionRenderer = nullptr;

	HEIN::EventManager* eventManager = nullptr;

	HEIN::CameraController* mainCamera = nullptr;
};

