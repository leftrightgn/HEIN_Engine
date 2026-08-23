#pragma once
#include "Components/IComponent.h"
#include "DebugingTools/IGizmoEditable.h"
#include <map>
#include <vector>
#include <string>

namespace HEIN
{
    class SkinnedModelComponent;

    // --------------------------------------------------------------------------
    // DirectXTK Binary Formats
    // Must exactly match Microsoft's memory layout to save properly.
    // The pragma pack(1) ensures no padding is added by the C++ compiler.
    // --------------------------------------------------------------------------
#pragma pack(push, 8)
    static constexpr uint32_t SDKMESH_FILE_VERSION = 101;
    static constexpr uint32_t MAX_FRAME_NAME = 100;

    struct SDKANIMATION_FILE_HEADER
    {
        uint32_t Version;             // Version 101
        uint8_t  IsBigEndian;         // 0 for false
        uint32_t FrameTransformType;  // 0 for FTT_RELATIVE
        uint32_t NumFrames;           // Total frames in the animation
        uint32_t NumAnimationKeys;    // Number of bones being animated
        uint32_t AnimationFPS;        // Baking frame rate (e.g., 60)
        uint64_t AnimationDataSize;   // Total size of all keyframe data in bytes
        uint64_t AnimationDataOffset; // Byte offset to the actual keyframe data array
    };

    struct SDKANIMATION_DATA
    {
        DirectX::XMFLOAT3 Translation;
        DirectX::XMFLOAT4 Orientation; // Quaternion
        DirectX::XMFLOAT3 Scale;
    };

    struct SDKANIMATION_FRAME_DATA
    {
        char FrameName[MAX_FRAME_NAME];          
        uint64_t DataOffset;          // Byte offset to this bone's specific array of SDKANIMATION_DATA
    };
#pragma pack(pop)

    // --------------------------------------------------------------------------
    // Editor Data Structures
    // --------------------------------------------------------------------------
    struct EditorKeyframe
    {
        float time;
        DirectX::SimpleMath::Vector3 translation;
        DirectX::SimpleMath::Quaternion rotation;
        DirectX::SimpleMath::Vector3 scale;
    };

    struct EditorTrack
    {
        std::vector<EditorKeyframe> keyframes;

        // Helper to get smoothly interpolated transform for any given time
        DirectX::SimpleMath::Matrix Evaluate(float time) const;
    };

    // --------------------------------------------------------------------------
    // The Component Class
    // --------------------------------------------------------------------------
    class AnimationEditorComponent : public IComponent, public IGizmoEditable
    {
    private:
        SkinnedModelComponent* m_targetModel = nullptr;

        // Editor State
        bool m_isEditorActive = false;
        float m_currentTime = 0.0f;
        float m_previousTime = -1.0f;
        float m_maxTime = 2.0f; // Default animation length
        int m_selectedBoneIndex = -1;
        std::string m_editorStatusMessage = "Ready";
        std::string m_exportPath = "Resources/CustomAnim.sdkmesh_anim";

        // Track data mapped by Bone Index
        std::map<int, EditorTrack> m_tracks;

        // Current posed local transforms for all bones
        std::vector<DirectX::SimpleMath::Matrix> m_editorLocalBones;

    public:
        AnimationEditorComponent(Actor* owner);

        void Start() override;
        void Update(float deltaTime) override;

        void OnInspectorGUI(GameContext& gameContext) override;
        void DrawAnimatorWindow(GameContext& gameContext);
        void DrawGizmo(
            const DirectX::SimpleMath::Matrix& view,
            const DirectX::SimpleMath::Matrix& proj,
            int operation,
            int mode) override;

        std::string GetComponentName() const override { return "AnimationEditorComponent"; }

        // Stub out serialization so it doesn't break JSON loader
        nlohmann::json Serialize() override { return IComponent::Serialize(); }
        void Deserialize(const nlohmann::json& data) override { IComponent::Deserialize(data); }

    private:
        void RecordKeyframe();
        bool ExportToSDKMeshAnim(const std::string& filepath);
        void ApplyEditorPoseToModel();
    };
}