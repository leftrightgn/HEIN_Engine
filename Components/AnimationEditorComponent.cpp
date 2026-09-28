#include "pch.h"
#include "AnimationEditorComponent.h"
#include "Components/SkinnedModelComponent.h"
#include "Components/TransformComponent.h"
#include "Entities/Actor.h"
#include <ImGui/imgui.h>
#include <ImGui/ImGuizmo.h>
#include <ImGui/imgui_stdlib.h>
#include <DebugingTools/DebugUIManager.h>
#include <fstream>
#include <algorithm>

using namespace HEIN;

// ------------------------------------------------------------------------------
// Smooth Easing Interpolation (Catmull-like ease in/out)
// ------------------------------------------------------------------------------
DirectX::SimpleMath::Matrix EditorTrack::Evaluate(float time) const
{
    if (keyframes.empty()) return DirectX::SimpleMath::Matrix::Identity;

    // Clamp to first frame if before start
    if (keyframes.size() == 1 || time <= keyframes.front().time)
    {
        return DirectX::SimpleMath::Matrix::CreateScale(keyframes.front().scale) *
            DirectX::SimpleMath::Matrix::CreateFromQuaternion(keyframes.front().rotation) *
            DirectX::SimpleMath::Matrix::CreateTranslation(keyframes.front().translation);
    }
    // Clamp to last frame if after end
    if (time >= keyframes.back().time)
    {
        return DirectX::SimpleMath::Matrix::CreateScale(keyframes.back().scale) *
            DirectX::SimpleMath::Matrix::CreateFromQuaternion(keyframes.back().rotation) *
            DirectX::SimpleMath::Matrix::CreateTranslation(keyframes.back().translation);
    }

    // Find the two keyframes surrounding the current time
    for (size_t i = 0; i < keyframes.size() - 1; ++i)
    {
        if (time >= keyframes[i].time && time <= keyframes[i + 1].time)
        {
            const auto& k1 = keyframes[i];
            const auto& k2 = keyframes[i + 1];

            // Calculate linear time ratio (0.0 to 1.0)
            float t = (time - k1.time) / (k2.time - k1.time);

            // Apply Smoothstep Easing formula: f(t) = 3t^2 - 2t^3
            // This prevents robotic, perfectly linear movement and simulates muscle acceleration.
            float smoothT = t * t * (3.0f - 2.0f * t);

            DirectX::SimpleMath::Vector3 lerpScale = DirectX::SimpleMath::Vector3::Lerp(k1.scale, k2.scale, smoothT);
            DirectX::SimpleMath::Quaternion lerpRot = DirectX::SimpleMath::Quaternion::Slerp(k1.rotation, k2.rotation, smoothT);
            DirectX::SimpleMath::Vector3 lerpTrans = DirectX::SimpleMath::Vector3::Lerp(k1.translation, k2.translation, smoothT);

            return DirectX::SimpleMath::Matrix::CreateScale(lerpScale) *
                DirectX::SimpleMath::Matrix::CreateFromQuaternion(lerpRot) *
                DirectX::SimpleMath::Matrix::CreateTranslation(lerpTrans);
        }
    }
    return DirectX::SimpleMath::Matrix::Identity;
}

// ------------------------------------------------------------------------------
// Component Core
// ------------------------------------------------------------------------------
AnimationEditorComponent::AnimationEditorComponent(Actor* owner) : IComponent(owner) {}

void AnimationEditorComponent::Start()
{
    m_targetModel = m_owner->GetComponent<SkinnedModelComponent>();
}

void AnimationEditorComponent::Update(float deltaTime)
{
    // If the editor is open, hijack the model's rendering every frame
    if (m_isEditorActive && m_targetModel != nullptr)
    {
        ApplyEditorPoseToModel();
    }
}

void AnimationEditorComponent::ApplyEditorPoseToModel()
{
    size_t boneCount = m_targetModel->GetBoneCount();
    if (boneCount == 0) return;

    bool needsInit = false;
    if (m_editorLocalBones.size() != boneCount)
    {
        m_editorLocalBones.resize(boneCount, DirectX::SimpleMath::Matrix::Identity);
        needsInit = true;
    }

    bool timeChanged = (std::abs(m_currentTime - m_previousTime) > 1e-6f);

    if (needsInit || timeChanged)
    {
        m_previousTime = m_currentTime;

        // Build the local matrix array for this exact frame
        for (size_t i = 0; i < boneCount; ++i)
        {
            if (m_tracks.find(i) != m_tracks.end())
            {
                m_editorLocalBones[i] = m_tracks[i].Evaluate(m_currentTime);
            }
            else
            {
                // If the bone has no keyframes, use its original default T-Pose
                m_editorLocalBones[i] = m_targetModel->GetBindPoseLocalMatrix(i);
            }
        }
    }

    // Push the matrices to the SkinnedModelComponent
    m_targetModel->OverrideBones(m_editorLocalBones.data());
}

// ------------------------------------------------------------------------------
// ImGui Interface
// ------------------------------------------------------------------------------
void AnimationEditorComponent::OnInspectorGUI(GameContext& gameContext)
{
    ImGui::TextDisabled("Use the ANIMATOR window in the toolbar to edit animations.");
}

void AnimationEditorComponent::DrawAnimatorWindow(GameContext& gameContext)
{
    ImGui::Checkbox("Enable Animation Editing Mode", &m_isEditorActive);

    if (m_isEditorActive)
    {
        // Force ImGuizmo to target this component
        HEIN::g_ActiveGizmoTarget = this;

        ImGui::Separator();
        ImGui::Text("Timeline & Playback");

        ImGui::SliderFloat("Timeline (s)", &m_currentTime, 0.0f, m_maxTime);
        ImGui::DragFloat("Max Duration (s)", &m_maxTime, 0.1f, 0.1f, 60.0f);

        ImGui::Separator();
        ImGui::Text("Rigging Tools");

        // Bone selection dropdown for skeletal posing
        std::string currentBoneName = (m_selectedBoneIndex == -1) ? "None" : (m_targetModel ? m_targetModel->GetBoneName(m_selectedBoneIndex) : "Unknown");
        if (currentBoneName.empty()) currentBoneName = "Bone " + std::to_string(m_selectedBoneIndex);
        if (m_targetModel && ImGui::BeginCombo("Select Bone", currentBoneName.c_str()))
        {
            for (int i = 0; i < m_targetModel->GetBoneCount(); ++i)
            {
                bool isSelected = (m_selectedBoneIndex == i);
                std::string boneName = m_targetModel->GetBoneName(i);
                if (boneName.empty()) boneName = "Bone " + std::to_string(i);
                std::string selectableLabel = boneName + "##" + std::to_string(i);
                if (ImGui::Selectable(selectableLabel.c_str(), isSelected))
                {
                    m_selectedBoneIndex = i;
                }
            }
            ImGui::EndCombo();
        }

        if (m_selectedBoneIndex != -1)
        {
            if (ImGui::Button("Record Keyframe (Current Pose)", ImVec2(-1, 30)))
            {
                RecordKeyframe();
            }
        }

        ImGui::Separator();
        ImGui::Text("Exporter");

        ImGui::InputText("Export Path", &m_exportPath);

        // Status Message Color formatting
        ImVec4 statusColor = (m_editorStatusMessage.find("Error") != std::string::npos) ? ImVec4(1, 0, 0, 1) : ImVec4(0, 1, 0, 1);
        ImGui::TextColored(statusColor, "Status: %s", m_editorStatusMessage.c_str());

        if (ImGui::Button("Bake & Export to .sdkmesh_anim", ImVec2(-1, 40)))
        {
            ExportToSDKMeshAnim(m_exportPath);
        }
    }
}

// ------------------------------------------------------------------------------
// ImGuizmo 3D Viewport Handling
// ------------------------------------------------------------------------------
void AnimationEditorComponent::DrawGizmo(
    const DirectX::SimpleMath::Matrix& view,
    const DirectX::SimpleMath::Matrix& proj,
    int operation,
    int mode)
{
    if (!m_isEditorActive || m_selectedBoneIndex == -1 || m_targetModel == nullptr) return;

    TransformComponent* transform = m_owner->GetComponent<TransformComponent>();
    DirectX::SimpleMath::Matrix actorWorld = transform->GetWorldMatrix();

    // Query bone world-space transformation matrix
    DirectX::SimpleMath::Matrix boneWorld = m_targetModel->GetBoneWorldMatrix(m_selectedBoneIndex, actorWorld);

    ImGuizmo::SetID(999);
    ImGuizmo::Manipulate((float*)&view.m[0][0], (float*)&proj.m[0][0],
        (ImGuizmo::OPERATION)operation, (ImGuizmo::MODE)mode,
        (float*)&boneWorld.m[0][0]);

    // Handle interactive Gizmo manipulation
    if (ImGuizmo::IsUsing())
    {
        int parentIndex = m_targetModel->GetParentBoneIndex(m_selectedBoneIndex);

        DirectX::SimpleMath::Matrix parentWorld = actorWorld;
        if (parentIndex != -1)
        {
            parentWorld = m_targetModel->GetBoneWorldMatrix(parentIndex, actorWorld);
        }

        // Convert world-space gizmo transform to parent-relative local coordinates:
        // Local Matrix = New World Matrix * Inverse(Parent World Matrix)
        DirectX::SimpleMath::Matrix newLocal = boneWorld * parentWorld.Invert();

        // Enforce pure SRT (Scale-Rotate-Translate) immediately. 
        // This prevents the bone from accumulating shear or non-uniform scale artifacts that 
        // Decompose() would later strip out, which causes the pose to 'snap' when recording.
        DirectX::SimpleMath::Vector3 s, t;
        DirectX::SimpleMath::Quaternion r;
        newLocal.Decompose(s, r, t);
        newLocal = DirectX::SimpleMath::Matrix::CreateScale(s) *
                   DirectX::SimpleMath::Matrix::CreateFromQuaternion(r) *
                   DirectX::SimpleMath::Matrix::CreateTranslation(t);

        size_t boneCount = m_targetModel->GetBoneCount();
        if (m_editorLocalBones.size() != boneCount)
        {
            m_editorLocalBones.resize(boneCount, DirectX::SimpleMath::Matrix::Identity);
        }

        m_editorLocalBones[m_selectedBoneIndex] = newLocal;
    }
}

// ------------------------------------------------------------------------------
// Keyframe Recording Logic
// ------------------------------------------------------------------------------
void AnimationEditorComponent::RecordKeyframe()
{
    if (m_selectedBoneIndex == -1 || m_targetModel == nullptr) return;

    size_t boneCount = m_targetModel->GetBoneCount();
    if (m_editorLocalBones.size() != boneCount)
    {
        m_editorLocalBones.resize(boneCount, DirectX::SimpleMath::Matrix::Identity);
    }

    DirectX::SimpleMath::Vector3 scale, trans;
    DirectX::SimpleMath::Quaternion rot;
    m_editorLocalBones[m_selectedBoneIndex].Decompose(scale, rot, trans);
    rot.Normalize();

    EditorKeyframe newKey{ m_currentTime, trans, rot, scale };
    auto& track = m_tracks[m_selectedBoneIndex];

    // Safe Error Handling: Check if a frame already exists at this exact time limit (+/- 0.01s)
    auto it = std::find_if(track.keyframes.begin(), track.keyframes.end(),
        [time = m_currentTime](const EditorKeyframe& k) {
            return std::abs(k.time - time) < 0.01f;
        });

    if (it != track.keyframes.end())
    {
        // Overwrite the existing frame safely
        *it = newKey;
        m_editorStatusMessage = "Overwrote Keyframe at " + std::to_string(m_currentTime) + "s";
    }
    else
    {
        // Insert new frame and sort the track by time
        track.keyframes.push_back(newKey);
        std::sort(track.keyframes.begin(), track.keyframes.end(),
            [](const EditorKeyframe& a, const EditorKeyframe& b) { return a.time < b.time; });

        m_editorStatusMessage = "Created Keyframe at " + std::to_string(m_currentTime) + "s";
    }

    // Force the engine to immediately re-evaluate the animation tracks next frame
    // so the visual pose perfectly snaps to the recorded keyframe state.
    m_previousTime = -1.0f;
}

// ------------------------------------------------------------------------------
// Binary File Exporter
// ------------------------------------------------------------------------------
bool AnimationEditorComponent::ExportToSDKMeshAnim(const std::string& filepath)
{
    if (m_tracks.empty())
    {
        m_editorStatusMessage = "Error: No animation tracks to save!";
        return false;
    }

    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        m_editorStatusMessage = "Error: Failed to open file for writing.";
        return false;
    }

    try
    {
        uint32_t fps = 60; // Bake standard at 60 FPS
        uint32_t totalFrames = static_cast<uint32_t>(m_maxTime * fps);
        uint32_t numAnimatedBones = static_cast<uint32_t>(m_tracks.size());

        // Build File Header
        SDKANIMATION_FILE_HEADER header = {};
        header.Version = SDKMESH_FILE_VERSION;
        header.IsBigEndian = 0;
        header.FrameTransformType = 0; // FTT_RELATIVE
        
        // In DirectXTK, 'NumFrames' means number of bones (Frame Datas), 
        // and 'NumAnimationKeys' means number of keyframes per track.
        header.NumFrames = numAnimatedBones;
        header.NumAnimationKeys = totalFrames;
        
        header.AnimationFPS = fps;
        header.AnimationDataSize = (numAnimatedBones * sizeof(SDKANIMATION_FRAME_DATA)) + (numAnimatedBones * totalFrames * sizeof(SDKANIMATION_DATA));
        header.AnimationDataOffset = sizeof(SDKANIMATION_FILE_HEADER);

        // Write Header
        file.write(reinterpret_cast<const char*>(&header), sizeof(SDKANIMATION_FILE_HEADER));

        // Build and Write Frame Lookup Array
        // DataOffset in frameData is relative to the start of the FRAME_DATA array (which is right after the header)
        uint64_t currentDataOffset = numAnimatedBones * sizeof(SDKANIMATION_FRAME_DATA);
        for (auto const& [boneIndex, track] : m_tracks)
        {
            SDKANIMATION_FRAME_DATA frameData = {};

            std::string boneName = m_targetModel->GetBoneName(boneIndex);
            strncpy_s(frameData.FrameName, sizeof(frameData.FrameName), boneName.c_str(), _TRUNCATE);

            frameData.DataOffset = currentDataOffset;
            currentDataOffset += (totalFrames * sizeof(SDKANIMATION_DATA));

            file.write(reinterpret_cast<const char*>(&frameData), sizeof(SDKANIMATION_FRAME_DATA));
        }

        // Keyframe Baking: Sample animated bone tracks uniformly at designated sampling frequency
        for (auto const& [boneIndex, track] : m_tracks)
        {
            for (uint32_t frame = 0; frame < totalFrames; ++frame)
            {
                float time = static_cast<float>(frame) / fps;

                DirectX::SimpleMath::Matrix sampledLocal = track.Evaluate(time);

                DirectX::SimpleMath::Vector3 scale, trans;
                DirectX::SimpleMath::Quaternion rot;
                sampledLocal.Decompose(scale, rot, trans);
                rot.Normalize();

                // Convert SimpleMath to base XMFLOAT formats for raw memory writing
                SDKANIMATION_DATA data;
                data.Translation = { trans.x, trans.y, trans.z };
                data.Orientation = { rot.x, rot.y, rot.z, rot.w };
                data.Scale = { scale.x, scale.y, scale.z };

                file.write(reinterpret_cast<const char*>(&data), sizeof(SDKANIMATION_DATA));
            }
        }

        file.close();

        if (file.fail())
        {
            m_editorStatusMessage = "Error: Stream failed during write!";
            return false;
        }

        m_editorStatusMessage = "Success: Baked " + std::to_string(totalFrames) + " frames to file!";
        return true;
    }
    catch (const std::exception& e)
    {
        m_editorStatusMessage = std::string("Fatal Error: ") + e.what();
        file.close();
        return false;
    }
}