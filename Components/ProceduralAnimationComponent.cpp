#include "pch.h"
#include "ProceduralAnimationComponent.h"
#include "Components/SkinnedModelComponent.h"
#include "Components/ColliderComponent/TerrainColliderComponent.h"
#include "Components/ColliderComponent/MeshColliderComponent.h"
#include "Common/CollisionMath.h"
#include "Components/TransformComponent.h"
#include "Entities/ActorManager.h"
#include "Framework/GameContext.h"
#include <ImGui/imgui.h>
#include <algorithm>
#include <cmath>

using namespace DirectX::SimpleMath;

namespace HEIN
{
    ProceduralAnimationComponent::ProceduralAnimationComponent(Actor* owner, ActorManager* manager) 
        : IComponent(owner), m_actorManager(manager)
    {
        // Default chains like Unity/Unreal Avatar
        m_ikChains.push_back({ L"mixamorig:LeftFoot", L"mixamorig:LeftLeg", L"mixamorig:LeftUpLeg", L"mixamorig:LeftToe_End", 1.0f, 0.0f, true });
        m_ikChains.push_back({ L"mixamorig:RightFoot", L"mixamorig:RightLeg", L"mixamorig:RightUpLeg", L"mixamorig:RightToe_End", 1.0f, 0.0f, true });
    }

    nlohmann::json ProceduralAnimationComponent::Serialize()
    {
        nlohmann::json data = IComponent::Serialize();
        data["Enabled"] = m_enabled;
        data["GlobalIKWeight"] = m_globalIKWeight;

        nlohmann::json chainsJson = nlohmann::json::array();
        for (const auto& chain : m_ikChains)
        {
            nlohmann::json c;
            c["Effector"] = std::string(chain.effectorBoneName.begin(), chain.effectorBoneName.end());
            c["Mid"] = std::string(chain.midBoneName.begin(), chain.midBoneName.end());
            c["Root"] = std::string(chain.rootBoneName.begin(), chain.rootBoneName.end());
            c["Toe"] = std::string(chain.toeBoneName.begin(), chain.toeBoneName.end());
            c["Weight"] = chain.weight;
            c["HeightOffset"] = chain.heightOffset;
            c["IsFoot"] = chain.isFoot;
            chainsJson.push_back(c);
        }
        data["IKChains"] = chainsJson;

        return data;
    }

    void ProceduralAnimationComponent::Deserialize(const nlohmann::json& data)
    {
        IComponent::Deserialize(data);
        if (data.contains("Enabled")) m_enabled = data["Enabled"];
        if (data.contains("GlobalIKWeight")) m_globalIKWeight = data["GlobalIKWeight"];

        if (data.contains("IKChains") && data["IKChains"].is_array())
        {
            m_ikChains.clear();
            for (const auto& c : data["IKChains"])
            {
                IKChain chain;
                if (c.contains("Effector")) { std::string s = c["Effector"]; chain.effectorBoneName = std::wstring(s.begin(), s.end()); }
                if (c.contains("Mid")) { std::string s = c["Mid"]; chain.midBoneName = std::wstring(s.begin(), s.end()); }
                if (c.contains("Root")) { std::string s = c["Root"]; chain.rootBoneName = std::wstring(s.begin(), s.end()); }
                if (c.contains("Toe")) { std::string s = c["Toe"]; chain.toeBoneName = std::wstring(s.begin(), s.end()); }
                if (c.contains("Weight")) chain.weight = c["Weight"];
                if (c.contains("HeightOffset")) chain.heightOffset = c["HeightOffset"];
                if (c.contains("IsFoot")) chain.isFoot = c["IsFoot"];
                m_ikChains.push_back(chain);
            }
        }
    }

    void ProceduralAnimationComponent::OnInspectorGUI(GameContext& gameContext)
    {
        if (ImGui::CollapsingHeader("Procedural Animation (Whole Body IK)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Enable Procedural IK", &m_enabled);
            ImGui::SliderFloat("Global IK Weight", &m_globalIKWeight, 0.0f, 1.0f);

            ImGui::Separator();
            ImGui::Text("IK Chains");
            
            for (size_t i = 0; i < m_ikChains.size(); ++i)
            {
                ImGui::PushID((int)i);
                auto& chain = m_ikChains[i];
                
                auto drawStr = [](const char* label, std::wstring& ws) {
                    std::string s(ws.begin(), ws.end());
                    char buf[256];
                    strcpy_s(buf, s.c_str());
                    if (ImGui::InputText(label, buf, 256)) {
                        std::string ns(buf);
                        ws = std::wstring(ns.begin(), ns.end());
                    }
                };

                drawStr("Effector", chain.effectorBoneName);
                drawStr("Mid", chain.midBoneName);
                drawStr("Root", chain.rootBoneName);
                drawStr("Toe", chain.toeBoneName);
                ImGui::SliderFloat("Weight", &chain.weight, 0.0f, 1.0f);
                ImGui::DragFloat("Height Offset", &chain.heightOffset, 0.1f);
                ImGui::Checkbox("Is Foot", &chain.isFoot);
                
                if (ImGui::Button("Remove Chain"))
                {
                    m_ikChains.erase(m_ikChains.begin() + i);
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
                ImGui::Separator();
            }

            if (ImGui::Button("Add IK Chain"))
            {
                m_ikChains.push_back({L"NewEffector", L"NewMid", L"NewRoot", L"NewToe", 1.0f, 0.0f, false});
            }

            if (m_terrain || m_meshCollider)
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Terrain/Mesh Linked");
            else
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Terrain/Mesh Not Linked");
        }
    }

    void ProceduralAnimationComponent::Start()
    {
        m_skinnedModel = m_owner->GetComponent<SkinnedModelComponent>();
    }

    void ProceduralAnimationComponent::Update(float deltaTime) {}

    void ProceduralAnimationComponent::LateUpdate(float deltaTime)
    {
        m_debugLines.clear();
        if (!m_enabled || m_globalIKWeight <= 0.01f) return;
        SolveWholeBodyIK();
    }

    void ProceduralAnimationComponent::Draw(GameContext& gameContext, const DirectX::SimpleMath::Matrix& world, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj)
    {
        if (gameContext.debugCollisionRenderer && !m_debugLines.empty())
        {
            // debugCollisionRenderer internally honors the Show Colliders toggle before actually rendering.
            for (const auto& line : m_debugLines)
            {
                gameContext.debugCollisionRenderer->QueueLine(line.start, line.end, line.color);
            }
        }
    }

    void ProceduralAnimationComponent::SolveWholeBodyIK()
    {
        // Start() can run before SkinnedModelComponent exists, so look it up lazily too.
        if (!m_skinnedModel)
            m_skinnedModel = m_owner->GetComponent<SkinnedModelComponent>();

        if (!m_skinnedModel)
        {
            OutputDebugStringA("[IK] no SkinnedModelComponent on owner\n");
            return;
        }

        if (!m_terrain && !m_meshCollider)
        {
            if (m_actorManager)
            {
                for (auto& pair : m_actorManager->GetAllActors())
                {
                    if (!m_terrain)
                        m_terrain = pair.second->GetComponent<TerrainColliderComponent>();
                    if (!m_meshCollider)
                        m_meshCollider = pair.second->GetComponent<MeshColliderComponent>();
                    
                    if (m_terrain && m_meshCollider)
                    {
                        // Found both ground sources
                        break;
                    }
                }
            }
            else
            {
                // Most likely reason IK "does nothing": nobody passed an ActorManager in.
                OutputDebugStringA("[IK] m_actorManager is null -> terrain/mesh can never be found\n");
            }
        }

        const Matrix* currentLocalBones = m_skinnedModel->GetCurrentLocalBones();
        if (!currentLocalBones)
        {
            OutputDebugStringA("[IK] no local bones (no animation playing yet?)\n");
            return;
        }

        const size_t boneCount = m_skinnedModel->GetBoneCount();
        if (boneCount == 0)
            return;

        TransformComponent* transform = m_owner->GetComponent<TransformComponent>();
        if (!transform)
            return;

        std::vector<Matrix> modifiedBones(currentLocalBones, currentLocalBones + boneCount);
        const Matrix actorWorld = transform->GetWorldMatrix();


        for (const auto& chain : m_ikChains)
        {
            const float weight = std::clamp(chain.weight * m_globalIKWeight, 0.0f, 1.0f);
            if (weight <= 0.001f)
                continue;

            const int effector = m_skinnedModel->GetBoneIndex(chain.effectorBoneName);
            const int mid = m_skinnedModel->GetBoneIndex(chain.midBoneName);
            const int root = m_skinnedModel->GetBoneIndex(chain.rootBoneName);
            const int toe = chain.toeBoneName.empty() ? -1 : m_skinnedModel->GetBoneIndex(chain.toeBoneName);

            if (root < 0 || mid < 0 || effector < 0)
            {
                // Bone names must match the model exactly (substring match), e.g. "mixamorig:LeftFoot".
                OutputDebugStringW((L"[IK] bone not found in chain: " + chain.effectorBoneName + L"\n").c_str());
                continue;
            }

            SolveTwoBoneIK(modifiedBones.data(), root, mid, effector, toe, actorWorld, weight, chain.heightOffset, chain.isFoot);
        }

        m_skinnedModel->OverrideBones(modifiedBones.data());
    }

    void ProceduralAnimationComponent::SolveTwoBoneIK(
        Matrix* localBones,
        int rootIdx,
        int midIdx,
        int effectorIdx,
        int toeIdx,
        const Matrix& worldMatrix,
        float weight,
        float heightOffset,
        bool alignToTerrain)
    {
        if (!localBones || !m_skinnedModel || weight <= 0.001f)
            return;

        static bool s_hasPrintedNaN = false;

        auto checkNaN = [](const Matrix& m, const char* name) {
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 4; ++j) {
                    if (std::isnan(m.m[i][j]) || std::isinf(m.m[i][j])) {
                        if (!s_hasPrintedNaN) {
                            char buf[256];
                            sprintf_s(buf, "[IK-FIRST-ERROR] %s has NaN/Inf!\n", name);
                            OutputDebugStringA(buf);
                            s_hasPrintedNaN = true;
                        }
                        return true;
                    }
                }
            }
            return false;
        };
        auto checkVecNaN = [](const Vector3& v, const char* name) {
            if (std::isnan(v.x) || std::isinf(v.x) || std::isnan(v.y) || std::isinf(v.y) || std::isnan(v.z) || std::isinf(v.z)) {
                if (!s_hasPrintedNaN) {
                    char buf[256];
                    sprintf_s(buf, "[IK-FIRST-ERROR] %s has NaN/Inf!\n", name);
                    OutputDebugStringA(buf);
                    s_hasPrintedNaN = true;
                }
                return true;
            }
            return false;
        };
        auto checkF = [](float v, const char* name) {
            if (std::isnan(v) || std::isinf(v)) {
                if (!s_hasPrintedNaN) {
                    char buf[256];
                    sprintf_s(buf, "[IK-FIRST-ERROR] %s has NaN/Inf!\n", name);
                    OutputDebugStringA(buf);
                    s_hasPrintedNaN = true;
                }
                return true;
            }
            return false;
        };

        auto checkValidAffine = [](const Matrix& m, const char* name) {
            // Uninitialized MSVC debug memory (0xcdcdcdcd) is -4.32e8.
            // It bypasses NaN/Inf checks, but completely destroys the math.
            // A valid affine bone matrix MUST have its 4th column close to (0, 0, 0, 1).
            if (std::abs(m._14) > 1e-4f || std::abs(m._24) > 1e-4f || 
                std::abs(m._34) > 1e-4f || std::abs(m._44 - 1.0f) > 1e-4f) {
                if (!s_hasPrintedNaN) {
                    char buf[256];
                    sprintf_s(buf, "[IK-FIRST-ERROR] %s is uninitialized or not affine! (0xCDCDCDCD caught)\n", name);
                    OutputDebugStringA(buf);
                    s_hasPrintedNaN = true;
                }
                return true;
            }
            return false;
        };

        if (checkNaN(worldMatrix, "worldMatrix")) return;

        const int boneCount = static_cast<int>(m_skinnedModel->GetBoneCount());
        if (rootIdx < 0 || midIdx < 0 || effectorIdx < 0 ||
            rootIdx >= boneCount || midIdx >= boneCount || effectorIdx >= boneCount)
        {
            OutputDebugStringA("[IK] Bone index out of bounds!\n");
            return;
        }

        // The solver assumes root -> mid -> effector are direct parent/child links.
        if (m_skinnedModel->GetParentBoneIndex(midIdx) != rootIdx ||
            m_skinnedModel->GetParentBoneIndex(effectorIdx) != midIdx)
        {
            char buf[512];
            sprintf_s(buf, "[IK] Hierarchy mismatch! root=%d, mid=%d (parent=%d), eff=%d (parent=%d)\n", 
                rootIdx, midIdx, m_skinnedModel->GetParentBoneIndex(midIdx), 
                effectorIdx, m_skinnedModel->GetParentBoneIndex(effectorIdx));
            OutputDebugStringA(buf);
            return;
        }

        // Model-space matrix of any bone, built from the LOCAL pose (child * parent * grandparent ...).
        auto modelMatrix = [&](int idx) -> Matrix
            {
                Matrix m = localBones[idx];
                for (int p = m_skinnedModel->GetParentBoneIndex(idx);
                    p >= 0;
                    p = m_skinnedModel->GetParentBoneIndex(p))
                {
                    m = m * localBones[p];
                }
                return m;
            };

        // Rotate a matrix around its own translation by a world-space rotation.
        auto rotateInPlace = [](const Matrix& m, const Matrix& rot) -> Matrix
            {
                Matrix r = m;
                const Vector3 t = m.Translation();
                r.Translation(Vector3::Zero);
                r = r * rot;
                r.Translation(t);
                return r;
            };

        // ---- 1. Current pose in world space ---------------------------------------------------
        const int rootParent = m_skinnedModel->GetParentBoneIndex(rootIdx);
        const Matrix rootParentModel = (rootParent >= 0) ? modelMatrix(rootParent) : Matrix::Identity;
        const Matrix rootParentWorld = rootParentModel * worldMatrix;   // also correct when root has no parent

        const Matrix rootWorld = localBones[rootIdx] * rootParentWorld;
        const Matrix midWorld = localBones[midIdx] * rootWorld;
        const Matrix effWorld = localBones[effectorIdx] * midWorld;

        if (checkNaN(rootParentWorld, "rootParentWorld")) return;
        if (checkValidAffine(localBones[rootIdx], "localBones[rootIdx] (INPUT)")) return;
        if (checkNaN(rootWorld, "rootWorld")) return;
        if (checkValidAffine(localBones[midIdx], "localBones[midIdx] (INPUT)")) return;

        if (checkNaN(midWorld, "midWorld"))
        {
            char buf[1024];
            sprintf_s(buf, "[IK-DEBUG] localBones[midIdx] = [%.2e, %.2e, %.2e, %.2e | %.2e, %.2e, %.2e, %.2e | %.2e, %.2e, %.2e, %.2e | %.2e, %.2e, %.2e, %.2e]\n",
                localBones[midIdx]._11, localBones[midIdx]._12, localBones[midIdx]._13, localBones[midIdx]._14,
                localBones[midIdx]._21, localBones[midIdx]._22, localBones[midIdx]._23, localBones[midIdx]._24,
                localBones[midIdx]._31, localBones[midIdx]._32, localBones[midIdx]._33, localBones[midIdx]._34,
                localBones[midIdx]._41, localBones[midIdx]._42, localBones[midIdx]._43, localBones[midIdx]._44);
            OutputDebugStringA(buf);

            sprintf_s(buf, "[IK-DEBUG] rootWorld = [%.2e, %.2e, %.2e, %.2e | %.2e, %.2e, %.2e, %.2e | %.2e, %.2e, %.2e, %.2e | %.2e, %.2e, %.2e, %.2e]\n",
                rootWorld._11, rootWorld._12, rootWorld._13, rootWorld._14,
                rootWorld._21, rootWorld._22, rootWorld._23, rootWorld._24,
                rootWorld._31, rootWorld._32, rootWorld._33, rootWorld._34,
                rootWorld._41, rootWorld._42, rootWorld._43, rootWorld._44);
            OutputDebugStringA(buf);
            return;
        }

        if (checkValidAffine(localBones[effectorIdx], "localBones[effectorIdx] (INPUT)")) return;
        if (checkNaN(effWorld, "effWorld")) return;

        const Vector3 rootPos = rootWorld.Translation();
        const Vector3 midPos = midWorld.Translation();
        const Vector3 effPos = effWorld.Translation();

        const float upperLen = Vector3::Distance(rootPos, midPos);
        const float lowerLen = Vector3::Distance(midPos, effPos);
        if (upperLen < 0.0001f || lowerLen < 0.0001f)
            return;

        // ---- 2. Target ------------------------------------------------------------------------
        Vector3 targetPos = effPos;
        Vector3 m_footNormal = Vector3::Zero;

        if (alignToTerrain && (m_terrain || m_meshCollider))
        {
            const Vector3 actorPos = worldMatrix.Translation();
            float groundAtFoot = 0.0f, groundAtActor = 0.0f;
            
            auto getGroundHeight = [&](const Vector3& pos, float& outHeight, Vector3* outNormal = nullptr) -> bool {
                bool hitAny = false;
                float bestHeight = -FLT_MAX;
                Vector3 bestNorm = Vector3::Up;

                if (m_terrain)
                {
                    Vector3 n;
                    float h;
                    if (m_terrain->GetHeightAtPosition(pos.x, pos.z, h, n))
                    {
                        bestHeight = h;
                        bestNorm = n;
                        hitAny = true;
                    }
                }
                
                if (m_meshCollider)
                {
                    Vector3 rayOrigin(pos.x, pos.y + 10000.0f, pos.z);
                    Vector3 rayDir = Vector3::Down;
                    float closestDist = FLT_MAX;
                    bool hitMesh = false;
                    Vector3 meshNormal = Vector3::Up;
                    for (const auto& tri : m_meshCollider->GetWorldTriangles())
                    {
                        float dist;
                        Vector3 n;
                        if (CollisionMath::IntersectRayTriangle(rayOrigin, rayDir, tri, dist, n))
                        {
                            if (dist < closestDist)
                            {
                                closestDist = dist;
                                meshNormal = n;
                                hitMesh = true;
                            }
                        }
                    }
                    if (hitMesh)
                    {
                        float h = rayOrigin.y - closestDist;
                        if (!hitAny || h > bestHeight)
                        {
                            bestHeight = h;
                            bestNorm = meshNormal;
                            hitAny = true;
                        }
                    }
                }

                if (hitAny)
                {
                    outHeight = bestHeight;
                    if (outNormal) *outNormal = bestNorm;
                    return true;
                }
                return false;
            };

            Vector3 footDir = worldMatrix.Forward();
            footDir.y = 0.0f;
            footDir.Normalize();

            // Estimate toe and heel positions in world space
            Vector3 toePos = effPos + footDir * 0.15f;
            Vector3 heelPos = effPos - footDir * 0.05f;

            if (toeIdx >= 0 && toeIdx < boneCount)
            {
                Matrix toeWorldMat = modelMatrix(toeIdx) * worldMatrix;
                toePos = toeWorldMat.Translation();
                footDir = toePos - effPos;
                footDir.y = 0.0f;
                if (footDir.LengthSquared() > 1e-6f)
                {
                    footDir.Normalize();
                    heelPos = effPos - footDir * 0.05f;
                }
            }

            float toeHeight = 0.0f, heelHeight = 0.0f;
            Vector3 terrainNormal = Vector3::Up; // Declare vector to catch the normal

            bool hitActor = getGroundHeight(actorPos, groundAtActor);
            bool hitToe = getGroundHeight(toePos, toeHeight, &terrainNormal); // Pass it to the toe raycast
            bool hitHeel = getGroundHeight(heelPos, heelHeight);

            if (hitActor || hitToe || hitHeel)
            {
                if (!hitActor) groundAtActor = hitToe ? toeHeight : heelHeight;
                if (!hitToe) toeHeight = groundAtActor;
                if (!hitHeel) heelHeight = groundAtActor;
                if (checkF(toeHeight, "toeHeight") || checkF(heelHeight, "heelHeight") || checkF(groundAtActor, "groundAtActor")) return;

                // 1. Set the terrain normal directly from the raycast
                if (hitToe) {
                    m_footNormal = terrainNormal;
                }
                else {
                    m_footNormal = Vector3::Up;
                }
                m_footNormal.Normalize();

                Vector3 toeWorld(toePos.x, toeHeight, toePos.z);
                Vector3 heelWorld(heelPos.x, heelHeight, heelPos.z);

                // 2. To prevent the toe or heel from clipping into the terrain, the ankle must be placed 
                // exactly on the line connecting the heel and toe.
                float optimalAnkleHeight = heelHeight + (toeHeight - heelHeight) * 0.25f;

                const float maxStep = 5.0f; 
                const float offset = std::clamp(optimalAnkleHeight - groundAtActor, -maxStep, maxStep);
                targetPos.y += offset * weight;
                targetPos.y += heightOffset; // Apply custom offset from Inspector

                // Debug Visualizations (Visible when Show Colliders is on)
                m_debugLines.push_back(DebugLine{ toePos + Vector3(0, 5.0f, 0), toeWorld, DirectX::SimpleMath::Color(DirectX::Colors::Yellow) });
                m_debugLines.push_back(DebugLine{ heelPos + Vector3(0, 5.0f, 0), heelWorld, DirectX::SimpleMath::Color(DirectX::Colors::Yellow) });
                m_debugLines.push_back(DebugLine{ toeWorld, heelWorld, DirectX::SimpleMath::Color(DirectX::Colors::Cyan) }); // The calculated slope
                Vector3 midPoint = (toeWorld + heelWorld) * 0.5f;
                m_debugLines.push_back(DebugLine{ midPoint, midPoint + m_footNormal * 1.0f, DirectX::SimpleMath::Color(DirectX::Colors::Magenta) }); // Upward normal
            }
            else
            {
                OutputDebugStringA("[IK] Ground raycast completely missed!\n");
            }
        }

        Vector3 toTarget = targetPos - rootPos;
        float dist = toTarget.Length();
        if (checkF(dist, "initial dist")) return;
        if (dist < 0.0001f)
            return;

        const float maxDist = upperLen + lowerLen - 0.001f;
        const float minDist = std::min(maxDist, std::abs(upperLen - lowerLen) + 0.001f);
        dist = std::clamp(dist, minDist, maxDist);
        toTarget.Normalize();
        if (checkVecNaN(toTarget, "toTarget.Normalize()")) return;
        targetPos = rootPos + toTarget * dist;

        // ---- 3. Bend the knee so |root -> effector| == dist -----------------------------------
        Vector3 U = midPos - rootPos;  U.Normalize();
        Vector3 L = effPos - midPos;   L.Normalize();
        if (checkVecNaN(U, "U") || checkVecNaN(L, "L")) return;

        Vector3 axis = U.Cross(L);
        if (axis.LengthSquared() < 1e-8f)
        {
            axis = worldMatrix.Right();
        }
        else
        {
            axis.Normalize();
            // ANTI-CLAP POLE VECTOR BIAS:
            // Idle animations often have slight knock-knees. When compressed by IK, this amplifies into clapping knees.
            // We force the bend axis to stay primarily aligned with the character's left/right axis so knees bend forward.
            Vector3 idealAxis = worldMatrix.Right();
            if (axis.Dot(idealAxis) < 0)
                idealAxis = -idealAxis;
            
            // Blend 90% towards the ideal forward-bending axis
            axis = Vector3::Lerp(axis, idealAxis, 0.9f);
        }
        axis.Normalize();
        if (checkVecNaN(axis, "axis")) return;

        const float curBend = std::acos(std::clamp(U.Dot(L), -1.0f, 1.0f));   // 0 = straight
        const float cosInterior = std::clamp(
            (upperLen * upperLen + lowerLen * lowerLen - dist * dist) / (2.0f * upperLen * lowerLen),
            -1.0f, 1.0f);
        const float wantedBend = DirectX::XM_PI - std::acos(cosInterior);
        if (checkF(curBend, "curBend") || checkF(wantedBend, "wantedBend")) return;

        auto safeInvert = [](const Matrix& m, bool& outSuccess) -> Matrix
        {
            float a11 = m._11, a12 = m._12, a13 = m._13;
            float a21 = m._21, a22 = m._22, a23 = m._23;
            float a31 = m._31, a32 = m._32, a33 = m._33;

            float det = a11 * (a22 * a33 - a23 * a32) - 
                        a12 * (a21 * a33 - a23 * a31) + 
                        a13 * (a21 * a32 - a22 * a31);

            if (std::abs(det) < 1e-10f)
            {
                outSuccess = false;
                return Matrix::Identity;
            }

            float invDet = 1.0f / det;
            outSuccess = true;

            Matrix invM = Matrix::Identity;
            invM._11 =  (a22 * a33 - a23 * a32) * invDet;
            invM._12 = -(a12 * a33 - a13 * a32) * invDet;
            invM._13 =  (a12 * a23 - a13 * a22) * invDet;

            invM._21 = -(a21 * a33 - a23 * a31) * invDet;
            invM._22 =  (a11 * a33 - a13 * a31) * invDet;
            invM._23 = -(a11 * a23 - a13 * a21) * invDet;

            invM._31 =  (a21 * a32 - a22 * a31) * invDet;
            invM._32 = -(a11 * a32 - a12 * a31) * invDet;
            invM._33 =  (a11 * a22 - a12 * a21) * invDet;

            float tx = m._41, ty = m._42, tz = m._43;
            invM._41 = -(tx * invM._11 + ty * invM._21 + tz * invM._31);
            invM._42 = -(tx * invM._12 + ty * invM._22 + tz * invM._32);
            invM._43 = -(tx * invM._13 + ty * invM._23 + tz * invM._33);

            return invM;
        };

        // Rotating about (U x L) by a positive angle increases the bend.
        const Matrix bendRot = Matrix::CreateFromAxisAngle(axis, wantedBend - curBend);
        if (checkNaN(bendRot, "bendRot")) return;
        const Matrix newMidWorld = rotateInPlace(midWorld, bendRot);

        bool invertSuccess = false;
        Matrix rootWorldInv = safeInvert(rootWorld, invertSuccess);
        if (!invertSuccess) return; // Abort IK if root bone is completely scaled to 0

        localBones[midIdx] = newMidWorld * rootWorldInv;
        if (checkNaN(localBones[midIdx], "localBones[midIdx]")) return;

        // ---- 4. Swing the whole leg so the effector direction points at the target ------------
        const Matrix newEffWorld = localBones[effectorIdx] * newMidWorld;
        Vector3 curDir = newEffWorld.Translation() - rootPos;
        curDir.Normalize();
        if (checkVecNaN(curDir, "curDir")) return;

        const Vector3 swingAxis = curDir.Cross(toTarget);
        const float swingSin = swingAxis.Length();
        const float swingCos = curDir.Dot(toTarget);

        if (swingSin > 1e-6f)
        {
            const float angle = std::atan2(swingSin, swingCos);
            if (checkF(angle, "angle")) return;
            const Matrix swingRot = Matrix::CreateFromAxisAngle(swingAxis / swingSin, angle);
            if (checkNaN(swingRot, "swingRot")) return;
            const Matrix newRootWorld = rotateInPlace(rootWorld, swingRot);
            Matrix rootParentInv = safeInvert(rootParentWorld, invertSuccess);
            if (invertSuccess)
            {
                localBones[rootIdx] = newRootWorld * rootParentInv;
                if (checkNaN(localBones[rootIdx], "localBones[rootIdx]")) return;
            }
        }

        // ---- 5. Align the foot to the terrain normal ------------------------------------------
        if (alignToTerrain && m_footNormal.y > 0.001f) // If we found a valid upward normal
        {
            // We want to rotate the foot so that the world UP vector matches the terrain normal.
            // DirectXMath's CreateFromAxisAngle is Left-Handed, so A x B rotates A *away* from B.
            // To rotate Up to m_footNormal, we must use m_footNormal x Up as the axis!
            Vector3 alignAxis = Vector3::Up.Cross(m_footNormal);
            float alignSin = alignAxis.Length();
            float alignCos = Vector3::Up.Dot(m_footNormal);
            
            if (alignSin > 1e-5f)
            {
                alignAxis.Normalize();
                float alignAngle = std::atan2(alignSin, alignCos);
                Matrix alignRot = Matrix::CreateFromAxisAngle(alignAxis, alignAngle);
                
                // Calculate the final world matrix of the effector after the leg swung
                // We must recalculate it because root and mid were modified!
                Matrix finalMidWorld = localBones[midIdx] * (localBones[rootIdx] * rootParentWorld);
                Matrix finalEffWorld = localBones[effectorIdx] * finalMidWorld;
                
                // Rotate the effector in world space around its own position
                Matrix alignedEffWorld = rotateInPlace(finalEffWorld, alignRot);
                
                Matrix midInv = safeInvert(finalMidWorld, invertSuccess);
                if (invertSuccess)
                {
                    localBones[effectorIdx] = alignedEffWorld * midInv;
                }
            }
        }
    }
}
