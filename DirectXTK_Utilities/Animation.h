#pragma once
//--------------------------------------------------------------------------------------
// File: Animation.h
//
// Simple animation playback system for CMO and SDKMESH for DirectX Tool Kit
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------
#pragma once

#include <DirectXMath.h>
#include <Model.h>

#include <memory>
#include <utility>
#include <vector>

namespace DX
{
    class AnimationSDKMESH
    {
    public:
        // Additional accessors
        // Get animation time
        double GetAnimTime() const { return m_animTime; }
        // Set animation time
        void SetAnimTime(const double& animTime) { m_animTime = animTime; }
        // Set animation start time
        void SetStartTime(const double& startTime) { m_animTime = startTime; }
        // Get animation end time
        double GetEndTime() const { return m_endTime; }
        // Set animation end time
        void SetEndTime(const double& endTime) { m_endTime = endTime; }

        // Get AnimData
        uint8_t* GetAnimData() { return m_animData.get(); }
        // Get specified BoneToTrack
        uint32_t GetBoneToTrack(const int& index) { return m_boneToTrack[index]; }
        // Get bone transforms
        DirectX::XMMATRIX* GetBoneTransforms() { return m_boneTransforms; }

        const DirectX::XMMATRIX* GetLocalBones() const { return m_animBones.get(); }

    public:
        // Constructor
        AnimationSDKMESH() noexcept;
        // Destructor
        ~AnimationSDKMESH() = default;

        AnimationSDKMESH(AnimationSDKMESH&&) = default;
        AnimationSDKMESH& operator= (AnimationSDKMESH&&) = default;

        AnimationSDKMESH(AnimationSDKMESH const&) = delete;
        AnimationSDKMESH& operator= (AnimationSDKMESH const&) = delete;

        // Load animation
        HRESULT Load(_In_z_ const wchar_t* fileName);
        // Release resources
        void Release();
        // Bind model
        bool Bind(const DirectX::Model& model);
        // Update animation
        void Update(float delta);
        // Apply bone transformations
        void Apply(const DirectX::Model& model, size_t nbones, _Out_writes_(nbones) DirectX::XMMATRIX* boneTransforms) const;
        // Apply skin matrix
        void ApplySkinMatrix(const DirectX::Model& model, size_t nbones, DirectX::XMMATRIX* outSkinningBones);

    private:
        // Start time
        double m_startTime;
        // End time
        double m_endTime;
        // Animation time
        double m_animTime;
        // Animation data buffer
        std::unique_ptr<uint8_t[]> m_animData;
        // Animation size
        size_t  m_animSize;
        // Bone to track conversion array
        std::vector<uint32_t> m_boneToTrack;
        // Animation bones
        DirectX::ModelBone::TransformArray  m_animBones;
        // Bone number
        int m_boneNumber;
        // Bone transform matrix
        DirectX::XMMATRIX* m_boneTransforms;
        // Blend factor
        float m_blendFactor;
    };

    class AnimationCMO
    {
    public:
        AnimationCMO() noexcept;
        ~AnimationCMO() = default;

        AnimationCMO(AnimationCMO&&) = default;
        AnimationCMO& operator= (AnimationCMO&&) = default;

        AnimationCMO(AnimationCMO const&) = delete;
        AnimationCMO& operator= (AnimationCMO const&) = delete;

        HRESULT Load(_In_z_ const wchar_t* fileName, size_t offset, _In_opt_z_ const wchar_t* clipName = nullptr);

        void Release()
        {
            m_animTime = m_startTime = m_endTime = 0.f;
            m_keys.clear();
            m_transforms.reset();
            m_animBones.reset();
        }

        void Bind(const DirectX::Model& model);

        void Update(float delta);

        void Apply(
            const DirectX::Model& model,
            size_t nbones,
            _Out_writes_(nbones) DirectX::XMMATRIX* boneTransforms) const;

    private:
        using Key = std::pair<uint32_t, float>;

        float  m_animTime;
        float  m_startTime;
        float  m_endTime;
        std::vector<Key> m_keys;
        DirectX::ModelBone::TransformArray  m_transforms;
        DirectX::ModelBone::TransformArray  m_animBones;
    };
}