//--------------------------------------------------------------------------------------
// File: Animation.cpp
// Simple animation playback system for CMO and SDKMESH for DirectX Tool Kit
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "Animation.h"
#include <cassert>
#include <fstream>
#include <stdexcept>

using namespace DX;
using namespace DirectX;

//--------------------------------------------------------------------------------------
// DirectX SDK SDKMESH animation
//--------------------------------------------------------------------------------------
namespace
{
#pragma pack(push,8)

    static constexpr uint32_t SDKMESH_FILE_VERSION = 101;
    static constexpr uint32_t MAX_FRAME_NAME = 100;

    struct SDKANIMATION_FILE_HEADER
    {
        uint32_t Version;
        uint8_t  IsBigEndian;
        uint32_t FrameTransformType;
        uint32_t NumFrames;
        uint32_t NumAnimationKeys;
        uint32_t AnimationFPS;
        uint64_t AnimationDataSize;
        uint64_t AnimationDataOffset;
    };

    static_assert(sizeof(SDKANIMATION_FILE_HEADER) == 40, "SDK Mesh structure size incorrect");

    struct SDKANIMATION_DATA
    {
        XMFLOAT3 Translation;
        XMFLOAT4 Orientation;
        XMFLOAT3 Scaling;
    };

    static_assert(sizeof(SDKANIMATION_DATA) == 40, "SDK Mesh structure size incorrect");

    struct SDKANIMATION_FRAME_DATA
    {
        char FrameName[MAX_FRAME_NAME];
        union
        {
            uint64_t DataOffset;
            SDKANIMATION_DATA* pAnimationData;
        };
    };

    static_assert(sizeof(SDKANIMATION_FRAME_DATA) == 112, "SDK Mesh structure size incorrect");

#pragma pack(pop)
}

// Constructor
AnimationSDKMESH::AnimationSDKMESH() noexcept
    :
    m_startTime(0.0),               // Start time
    m_endTime(0.0),                 // End time
    m_animTime(0.0),                // Animation time
    m_animData{},                   // Animation data
    m_animSize(0),                  // Animation size
    m_boneToTrack{},                // Bone to track conversion
    m_animBones{},                  // Animation bones
    m_boneNumber{},                 // Bone number
    m_boneTransforms{},             // Bone transform matrices
    m_blendFactor{}

{
}

// Load animation data
HRESULT AnimationSDKMESH::Load(_In_z_ const wchar_t* fileName)
{
    // Release resources
    Release();

    if (!fileName)
        return E_INVALIDARG;

    // Declare file stream
    std::ifstream inFile(fileName, std::ios::in | std::ios::binary | std::ios::ate);
    if (!inFile)
        return E_FAIL;

    // Get file size
    std::streampos length = inFile.tellg();
    if (!inFile)
        return E_FAIL;

    if (length < sizeof(SDKANIMATION_FILE_HEADER))
        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

    if (length > UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);

    // Allocate memory area to read file
    std::unique_ptr<uint8_t[]> blob(new (std::nothrow) uint8_t[size_t(length)]);
    if (!blob)
        return E_OUTOFMEMORY;

    // Move seek pointer to the beginning
    inFile.seekg(0, std::ios::beg);
    if (!inFile)
        return E_FAIL;

    // Read file
    inFile.read(reinterpret_cast<char*>(blob.get()), length);
    if (!inFile)
        return E_FAIL;

    // Close file stream
    inFile.close();

    // Get header
    const SDKANIMATION_FILE_HEADER* header = reinterpret_cast<const SDKANIMATION_FILE_HEADER*>(blob.get());

    if (header->Version != SDKMESH_FILE_VERSION
        || header->IsBigEndian != 0
        || header->FrameTransformType != 0 /*FTT_RELATIVE*/
        || header->NumAnimationKeys == 0
        || header->NumFrames == 0
        || header->AnimationFPS == 0)
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

    // Calculate data size
    uint64_t dataSize = header->AnimationDataOffset + header->AnimationDataSize;
    if (dataSize > uint64_t(length))
        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

    m_animData.swap(blob);

    // Calculate animation size
    m_animSize = static_cast<size_t>(length);
    return S_OK;
}

// Bind model and bones
bool AnimationSDKMESH::Bind(const Model& model)
{
    assert(m_animData && m_animSize > 0);

    if (model.bones.empty())
        return false;

    const SDKANIMATION_FILE_HEADER* header = reinterpret_cast<const SDKANIMATION_FILE_HEADER*>(m_animData.get());
    assert(header->Version == SDKMESH_FILE_VERSION);
    SDKANIMATION_FRAME_DATA* frameData = reinterpret_cast<SDKANIMATION_FRAME_DATA*>(m_animData.get() + header->AnimationDataOffset);

    if (header->NumAnimationKeys > 0 && header->AnimationFPS > 0)
    {
        // Calculate animation end time
        m_endTime = static_cast<float>(header->NumAnimationKeys) / static_cast<float>(header->AnimationFPS);
    }
    else
    {
        // Set to 0 if data is invalid
        m_endTime = 0.0f;
    }

    m_boneToTrack.resize(model.bones.size());
    for (uint32_t& it : m_boneToTrack)
    {
        it = ModelBone::c_Invalid;
    }

    bool result = false;

    for (size_t j = 0; j < header->NumFrames; ++j)
    {
        uint64_t offset = sizeof(SDKANIMATION_FILE_HEADER) + frameData[j].DataOffset;
        uint64_t end = offset + sizeof(SDKANIMATION_DATA) * uint64_t(header->NumAnimationKeys);
        if (end > UINT32_MAX
            || end > m_animSize)
            throw std::runtime_error("Animation file invalid");

        frameData[j].pAnimationData = reinterpret_cast<SDKANIMATION_DATA*>(m_animData.get() + offset);

        wchar_t frameName[MAX_FRAME_NAME] = {};
        MultiByteToWideChar(CP_UTF8, 0, frameData[j].FrameName, -1, frameName, MAX_FRAME_NAME);

        size_t count = 0;
        for (const DirectX::ModelBone& it : model.bones)
        {
            if (_wcsicmp(frameName, it.name.c_str()) == 0)
            {
#if(_DEBUG)
                wchar_t buffer[128];
                swprintf_s(buffer, _countof(buffer), L"%3zu: %-40ws \n", count, it.name.c_str());
                OutputDebugString(buffer);
#endif
                m_boneToTrack[count] = static_cast<uint32_t>(j);
                result = true;
                break;
            }
            count++;
        }
    }

    // Allocate bone array
    m_animBones = ModelBone::MakeArray(model.bones.size());
    return result;
}

// Update animation time
void AnimationSDKMESH::Update(float delta)
{
    m_animTime += delta;
}

_Use_decl_annotations_
void AnimationSDKMESH::Apply(const DirectX::Model& model, size_t nbones, XMMATRIX* boneTransforms) const
{
    assert(m_animData && m_animSize > 0);

    const SDKANIMATION_FILE_HEADER* header = reinterpret_cast<const SDKANIMATION_FILE_HEADER*>(m_animData.get());
    SDKANIMATION_FRAME_DATA* frameData = reinterpret_cast<SDKANIMATION_FRAME_DATA*>(m_animData.get() + header->AnimationDataOffset);

    // Calculate keyframe interpolation
    float timeInTicks = static_cast<float>(header->AnimationFPS) * static_cast<float>(m_animTime);
    uint32_t tick1 = static_cast<uint32_t>(timeInTicks) % header->NumAnimationKeys;

    // Smooth interpolation when looping (if last frame, set next frame to 0)
    uint32_t tick2 = (tick1 + 1 < header->NumAnimationKeys) ? (tick1 + 1) : 0;

    // Set interpolation factor
    float lerpFactor = timeInTicks - std::floor(timeInTicks);

    for (size_t j = 0; j < nbones; ++j)
    {
        if (m_boneToTrack[j] == ModelBone::c_Invalid)
        {
            m_animBones[j] = model.boneMatrices[j];
        }
        else
        {
            SDKANIMATION_FRAME_DATA* frame = &frameData[m_boneToTrack[j]];
            const SDKANIMATION_DATA* data1 = &frame->pAnimationData[tick1];
            const SDKANIMATION_DATA* data2 = &frame->pAnimationData[tick2];

            // Linear interpolation of position (Lerp)
            XMVECTOR position1 = XMLoadFloat3(&data1->Translation);
            XMVECTOR position2 = XMLoadFloat3(&data2->Translation);
            XMVECTOR interpolatedPosition = XMVectorLerp(position1, position2, lerpFactor);

            // Spherical linear interpolation of rotation (Slerp)
            XMVECTOR q1 = XMLoadFloat4(&data1->Orientation);
            XMVECTOR q2 = XMLoadFloat4(&data2->Orientation);
            XMVECTOR interpolatedRotation = XMQuaternionSlerp(q1, q2, lerpFactor);

            // Fix: Quaternion normalization (prevent deformation)
            interpolatedRotation = XMQuaternionNormalize(interpolatedRotation);

            // Linear interpolation of scale (Lerp)
            XMVECTOR scale1 = XMLoadFloat3(&data1->Scaling);
            XMVECTOR scale2 = XMLoadFloat3(&data2->Scaling);
            XMVECTOR interpolatedScale = XMVectorLerp(scale1, scale2, lerpFactor);

            // Fix matrix multiplication order (Scale x Rotation x Translation)
            XMMATRIX scale = XMMatrixScalingFromVector(interpolatedScale);
            XMMATRIX rotation = XMMatrixRotationQuaternion(interpolatedRotation);
            XMMATRIX translation = XMMatrixTranslationFromVector(interpolatedPosition);
            m_animBones[j] = scale * rotation * translation;
        }
    }

    // Apply absolute bone transformations
    model.CopyAbsoluteBoneTransforms(nbones, m_animBones.get(), boneTransforms);

    // Apply inverse bind pose matrix (prevent deformation) is currently commented out in original file
    //for (size_t index = 0; index < nbones; ++index)
    //{
        //boneTransforms[index] = XMMatrixMultiply(model.invBindPoseMatrices[index], boneTransforms[index]);
    //}
}

// Apply skin matrix (apply inverse bind pose matrix to prevent deformation)
void AnimationSDKMESH::ApplySkinMatrix(const DirectX::Model& model, size_t nbones, DirectX::XMMATRIX* outSkinningBones)
{
    // Apply inverse bind pose matrix (prevent deformation)
    for (size_t index = 0; index < nbones; ++index)
    {
        outSkinningBones[index] = XMMatrixMultiply(model.invBindPoseMatrices[index], outSkinningBones[index]);
    }
}

void AnimationSDKMESH::Release()
{
    m_animTime = 0.0;
    m_animSize = 0;
    m_animData.reset();
    m_boneToTrack.clear();
    m_animBones.reset();
}



//--------------------------------------------------------------------------------------
// Visual Studio Starter Kit CMO animation
//--------------------------------------------------------------------------------------
namespace
{
#pragma pack(push,1)

    struct Clip
    {
        float StartTime;
        float EndTime;
        uint32_t keys;
    };

    static_assert(sizeof(Clip) == 12, "CMO Mesh structure size incorrect");

    struct Keyframe
    {
        uint32_t BoneIndex;
        float Time;
        DirectX::XMFLOAT4X4 Transform;
    };
    static_assert(sizeof(Keyframe) == 72, "CMO Mesh structure size incorrect");

#pragma pack(pop)
}

AnimationCMO::AnimationCMO() noexcept :
    m_animTime(0.0f),
    m_startTime(0.0f),
    m_endTime(0.0f)
{
}

_Use_decl_annotations_
HRESULT AnimationCMO::Load(const wchar_t* fileName, size_t offset, const wchar_t* clipName)
{
    if (!fileName || !offset)
        return E_INVALIDARG;

    std::ifstream inFile(fileName, std::ios::in | std::ios::binary | std::ios::ate);
    if (!inFile)
        return E_FAIL;

    std::streampos length = inFile.tellg();
    if (!inFile)
        return E_FAIL;

    if (length > UINT32_MAX)
        return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);

    inFile.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!inFile)
        return E_FAIL;

    std::streamoff remaining = length - static_cast<std::streamoff>(offset);

    if (remaining < sizeof(uint32_t))
        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

    size_t dataSize = static_cast<size_t>(remaining);
    std::unique_ptr<uint8_t[]> blob(new (std::nothrow) uint8_t[dataSize]);
    if (!blob)
        return E_OUTOFMEMORY;

    inFile.read(reinterpret_cast<char*>(blob.get()), remaining);
    if (!inFile)
        return E_FAIL;

    inFile.close();

    const uint32_t* nClips = reinterpret_cast<const uint32_t*>(blob.get());
    size_t usedSize = sizeof(uint32_t);
    if (dataSize < usedSize)
        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

    if (!nClips)
        return E_FAIL;

    for (size_t j = 0; j < *nClips; ++j)
    {
        // Clip name
        const uint32_t* nName = reinterpret_cast<const uint32_t*>(blob.get() + usedSize);
        usedSize += sizeof(uint32_t);
        if (dataSize < usedSize)
            return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

        const wchar_t* name = reinterpret_cast<const wchar_t*>(blob.get() + usedSize);

        usedSize += sizeof(wchar_t) * (*nName);
        if (dataSize < usedSize)
            return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

        const Clip* clip = reinterpret_cast<const Clip*>(blob.get() + usedSize);
        usedSize += sizeof(Clip);
        if (dataSize < usedSize)
            return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

        if (!clip->keys)
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

        const Keyframe* keys = reinterpret_cast<const Keyframe*>(blob.get() + usedSize);
        usedSize += sizeof(Keyframe) * clip->keys;
        if (dataSize < usedSize)
            return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);

        if (!clipName || _wcsicmp(clipName, name) == 0)
        {
            m_startTime = clip->StartTime;
            m_endTime = clip->EndTime;

            m_keys.resize(clip->keys);
            m_transforms = ModelBone::MakeArray(clip->keys);

            for (size_t k = 0; k < clip->keys; ++k)
            {
                m_keys[k].first = keys[k].BoneIndex;
                m_keys[k].second = keys[k].Time;
                m_transforms[k] = XMLoadFloat4x4(&keys[k].Transform);
            }
            return S_OK;
        }
    }
    return E_FAIL;
}

void AnimationCMO::Bind(const Model& model)
{
    assert(!m_keys.empty());

    m_animBones = ModelBone::MakeArray(model.bones.size());
}

void AnimationCMO::Update(float delta)
{
    m_animTime += delta;
    if (m_animTime > m_endTime)
    {
        m_animTime -= m_endTime;
    }
}

_Use_decl_annotations_
void AnimationCMO::Apply(const Model& model, size_t nbones, XMMATRIX* boneTransforms) const
{
    assert(!m_keys.empty());

    if (!nbones || !boneTransforms)
    {
        throw std::invalid_argument("Bone transforms array required");
    }

    if (nbones < model.bones.size())
    {
        throw std::invalid_argument("Bone transforms array is too small");
    }

    if (model.bones.empty())
    {
        throw std::runtime_error("Model is missing bones");
    }

    // Compute local bone transforms
    model.CopyBoneTransformsTo(nbones, m_animBones.get());

    // Apply keyframes
    if (m_animTime >= m_startTime)
    {
        size_t k = 0;
        for (const Key& kit : m_keys)
        {
            if (kit.second > m_animTime)
            {
                break;
            }

            m_animBones[kit.first] = m_transforms[k];
            ++k;
        }
    }

    // Compute absolute locations
    model.CopyAbsoluteBoneTransforms(nbones, m_animBones.get(), boneTransforms);

    // Adjust for model's bind pose.
    for (size_t j = 0; j < nbones; ++j)
    {
        boneTransforms[j] = XMMatrixMultiply(model.invBindPoseMatrices[j], boneTransforms[j]);
    }
}