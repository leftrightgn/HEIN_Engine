#include "pch.h"
#include "DebugCameraMode.h"

HEIN::DebugCameraMode::DebugCameraMode(
    float startDistance, 
	DirectX::SimpleMath::Vector3 target
)
    : m_target(target)
    , m_yaw(YAW)
    , m_pitch(PITCH)
    , m_roll(ROLL)
    , m_distance(startDistance)
    , m_mouseSensitivity(MOUSE_SENSITIVITY)
    , m_lastMouseX(LAST_MOUSEPOS)
    , m_lastMouseY(LAST_MOUSEPOS)
    , m_isDragging(false)
    , m_lastScrollValue(SCROLLVALUE)
    , m_needsScrollSync(true)
{
}

void HEIN::DebugCameraMode::OnEnter(CameraData& /*data*/)
{
    m_needsScrollSync = true;
}

void HEIN::DebugCameraMode::OnResume(CameraData& /*data*/)
{
    m_needsScrollSync = true;
}

void HEIN::DebugCameraMode::ProcessInput(const CameraInputState& input)
{
    if (m_needsScrollSync)
    {
        m_lastScrollValue = input.scrollWheelDelta;
        m_needsScrollSync = false;

    }

    float currentScroll = input.scrollWheelDelta;
    float actualScrollDelta = currentScroll - m_lastScrollValue;

    m_lastScrollValue = currentScroll;

    if (actualScrollDelta != 0.0f)
    {
        m_distance -= (actualScrollDelta / SCROLL_WHEEL_DELTA);
        m_distance = std::max(MIN_DISTANCE, m_distance); // Prevent zooming past the center point
    }

    // Only look around if the Left Mouse Button is held down
    if (input.isLeftMouseDown)
    {
        if (!m_isDragging)
        {
            m_lastMouseX = input.mouseX;
            m_lastMouseY = input.mouseY;
            m_isDragging = true;
        }
        else
        {
            float deltaX = input.mouseX - m_lastMouseX;
            float deltaY = input.mouseY - m_lastMouseY;

            m_yaw += -deltaX * m_mouseSensitivity;
            m_pitch += -deltaY * m_mouseSensitivity;

            // Update the last position for the next frame
            m_lastMouseX = input.mouseX;
            m_lastMouseY = input.mouseY;
        }
    }
    else
    {
        // Button was released, stop dragging
        m_isDragging = false;
    }

    // Clamp the pitch so it doesn't flip upside down
    const float pitchLimit = DirectX::XM_PIDIV2 - PITCH_LIMIT_OFFSET;
    m_pitch = std::clamp(m_pitch, -pitchLimit, pitchLimit);

    if (input.movementIntent.LengthSquared() > 0.0f)
    {
        DirectX::SimpleMath::Quaternion rotation =
            DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(m_yaw, PITCH, YAW);

        DirectX::SimpleMath::Vector3 worldMovement =
            DirectX::SimpleMath::Vector3::Transform(input.movementIntent, rotation);

        float panSpeed = 0.5f;

        m_target += worldMovement * panSpeed;
    }
}

void HEIN::DebugCameraMode::Update(CameraData& outData, float /*deltaTime*/, ICameraController& /*controller*/)
{
    DirectX::SimpleMath::Quaternion rotation = 
        DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(m_yaw, m_pitch, m_roll);

    DirectX::SimpleMath::Vector3 rotBackward = 
        DirectX::SimpleMath::Vector3::Transform(DirectX::SimpleMath::Vector3::Backward, rotation);

    DirectX::SimpleMath::Vector3 offset = rotBackward * m_distance;

    outData.position = m_target + offset;
    outData.rotation = rotation;
    outData.viewMatrix = 
        DirectX::SimpleMath::Matrix::CreateLookAt(outData.position, m_target, DirectX::SimpleMath::Vector3::Up);

    outData.fov = DirectX::XMConvertToRadians(DEBUG_CAM_FOV);

}