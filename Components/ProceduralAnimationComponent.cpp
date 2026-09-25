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
                m_ikChains.push_back({ L"NewEffector", L"NewMid", L"NewRoot", L"NewToe", 1.0f, 0.0f, false });
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
        SolveWholeBodyIK(deltaTime);
    }

    void ProceduralAnimationComponent::Draw(GameContext& gameContext, const DirectX::SimpleMath::Matrix& world, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj)
    {
        if (gameContext.debugCollisionRenderer && !m_debugLines.empty())
        {
            for (const auto& line : m_debugLines)
            {
                gameContext.debugCollisionRenderer->QueueLine(line.start, line.end, line.color);
            }
        }
    }

    void ProceduralAnimationComponent::SolveWholeBodyIK(float deltaTime)
    {
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
                        break;
                }
            }
            else
            {
                OutputDebugStringA("[IK] m_actorManager is null -> terrain/mesh can never be found\n");
            }
        }

        const Matrix* currentLocalBones = m_skinnedModel->GetCurrentLocalBones();
        if (!currentLocalBones)
            return;

        const size_t boneCount = m_skinnedModel->GetBoneCount();
        if (boneCount == 0)
            return;

        TransformComponent* transform = m_owner->GetComponent<TransformComponent>();
        if (!transform)
            return;

        std::vector<Matrix> modifiedBones(currentLocalBones, currentLocalBones + boneCount);
        const Matrix actorWorld = transform->GetWorldMatrix();

        for (auto& chain : m_ikChains)
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
                OutputDebugStringW((L"[IK] bone not found in chain: " + chain.effectorBoneName + L"\n").c_str());
                continue;
            }

            SolveTwoBoneIK(modifiedBones.data(), root, mid, effector, toe, actorWorld, weight, chain.heightOffset, chain.isFoot, deltaTime, chain.currentOffset, chain.currentNormal);
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
        bool alignToTerrain,
        float deltaTime,
        float& currentOffset,
        DirectX::SimpleMath::Vector3& currentNormal)
    {
        // Returns early if data is missing or IK weight is negligible.
        if (!localBones || !m_skinnedModel || weight <= 0.001f)
            return;

        static bool s_hasPrintedNaN = false;

        // Utility lambda functions for math validation. 
        // Prevents corrupted matrices from crashing the physics or rendering pipelines.
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

        // Validates bone indices against the model skeleton.
        const int boneCount = static_cast<int>(m_skinnedModel->GetBoneCount());
        if (rootIdx < 0 || midIdx < 0 || effectorIdx < 0 ||
            rootIdx >= boneCount || midIdx >= boneCount || effectorIdx >= boneCount)
        {
            OutputDebugStringA("[IK] Bone index out of bounds!\n");
            return;
        }

        // Verifies the logical parent-child relationship required for a two-bone solver.
        if (m_skinnedModel->GetParentBoneIndex(midIdx) != rootIdx ||
            m_skinnedModel->GetParentBoneIndex(effectorIdx) != midIdx)
        {
            return;
        }

        // Calculates the model-space matrix of any bone by walking up the hierarchy.
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

        // Rotates a matrix around its own translation vector by a world-space rotation matrix.
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
        // Computes initial world positions and matrices based on the un-modified animation pose.
        const int rootParent = m_skinnedModel->GetParentBoneIndex(rootIdx);
        const Matrix rootParentModel = (rootParent >= 0) ? modelMatrix(rootParent) : Matrix::Identity;
        const Matrix rootParentWorld = rootParentModel * worldMatrix;

        const Matrix rootWorld = localBones[rootIdx] * rootParentWorld;
        const Matrix midWorld = localBones[midIdx] * rootWorld;
        const Matrix effWorld = localBones[effectorIdx] * midWorld;

        if (checkNaN(rootParentWorld, "rootParentWorld")) return;
        if (checkValidAffine(localBones[rootIdx], "localBones[rootIdx] (INPUT)")) return;
        if (checkNaN(rootWorld, "rootWorld")) return;
        if (checkValidAffine(localBones[midIdx], "localBones[midIdx] (INPUT)")) return;
        if (checkNaN(midWorld, "midWorld")) return;
        if (checkValidAffine(localBones[effectorIdx], "localBones[effectorIdx] (INPUT)")) return;
        if (checkNaN(effWorld, "effWorld")) return;

        const Vector3 rootPos = rootWorld.Translation();
        const Vector3 midPos = midWorld.Translation();
        const Vector3 effPos = effWorld.Translation();

        // Calculates the immutable lengths of the upper and lower leg segments.
        const float upperLen = Vector3::Distance(rootPos, midPos);
        const float lowerLen = Vector3::Distance(midPos, effPos);
        if (upperLen < 0.0001f || lowerLen < 0.0001f)
            return;

        // ---- 2. Target ------------------------------------------------------------------------
        Vector3 targetPos = effPos;
        Vector3 m_footNormal = Vector3::Up;
        float targetOffset = 0.0f;

        Matrix effModel = modelMatrix(effectorIdx);
        // SWING PHASE MASKING
        float footHeightRelative = effPos.y - worldMatrix.Translation().y;
        float swingMask = 1.0f;

        if (footHeightRelative > 1.5f)
        {
            swingMask = std::clamp(1.0f - ((footHeightRelative - 1.20f) * 4.0f), 0.0f, 1.0f);
        }
        float activeWeight = weight * swingMask;

        if (alignToTerrain && (m_terrain || m_meshCollider))
        {
            const Vector3 actorPos = worldMatrix.Translation();
            float groundAtFoot = 0.0f, groundAtActor = 0.0f;

            // Helper lambda to query both terrain and mesh colliders for the highest floor point.
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
                    Vector3 rayOrigin(pos.x, pos.y + 10.0f, pos.z);
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

            // Estimates horizontal footprint orientation based on the actor's forward vector.
            Vector3 footDir = worldMatrix.Forward();
            footDir.y = 0.0f;
            footDir.Normalize();

            Vector3 toePos = effPos + footDir * 0.15f;
            Vector3 heelPos = effPos - footDir * 0.05f;

            // Refines footprint estimation using actual toe bone position if available.
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
            Vector3 terrainNormal = Vector3::Up;

            bool hitActor = getGroundHeight(actorPos, groundAtActor);
            bool hitToe = getGroundHeight(toePos, toeHeight, &terrainNormal);
            bool hitHeel = getGroundHeight(heelPos, heelHeight);

            // Interpolates missing raycast data by falling back to actor ground height.
            if (hitActor || hitToe || hitHeel)
            {
                if (!hitActor) groundAtActor = hitToe ? toeHeight : heelHeight;
                if (!hitToe) toeHeight = groundAtActor;
                if (!hitHeel) heelHeight = groundAtActor;
                if (checkF(toeHeight, "toeHeight") || checkF(heelHeight, "heelHeight") || checkF(groundAtActor, "groundAtActor")) return;

                if (hitToe) {
                    m_footNormal = terrainNormal;
                }
                m_footNormal.Normalize();

                // Calculates ankle height requirement to prevent toe or heel clipping.
                float optimalAnkleHeight = heelHeight + (toeHeight - heelHeight) * 0.25f;
                const float maxStep = 5.0f;
                targetOffset = std::clamp(optimalAnkleHeight - groundAtActor, -maxStep, maxStep);
            }
        }

        // TEMPORAL SMOOTHING: Interpolates target offset and normal over time using deltaTime.
        // Prevents violent, single-frame popping when stepping over sharp geometry edges.
        float lerpSpeed = 15.0f * deltaTime;
        lerpSpeed = std::clamp(lerpSpeed, 0.0f, 1.0f);

        currentOffset = currentOffset + (targetOffset - currentOffset) * lerpSpeed;
        currentNormal = Vector3::Lerp(currentNormal, m_footNormal, lerpSpeed);
        currentNormal.Normalize();

        // Modifies target elevation based on smoothed terrain calculation and user offset.
        targetPos.y += currentOffset * activeWeight;
        targetPos.y += heightOffset;

        // Restricts the target position to the mathematical limits of the leg reach.
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
        // Determines the current orientation vectors of the upper and lower leg.
        Vector3 U = midPos - rootPos;  U.Normalize();
        Vector3 L = effPos - midPos;   L.Normalize();
        if (checkVecNaN(U, "U") || checkVecNaN(L, "L")) return;

        Vector3 axis = U.Cross(L);
        if (axis.LengthSquared() < 1e-8f)
        {
            // Fallback to the thigh's local right vector only if the leg is perfectly straight
            axis = rootWorld.Right();
        }
        axis.Normalize();
        if (checkVecNaN(axis, "axis")) return;

        // Applies the Law of Cosines to calculate the interior angle required to reach the target distance.
        const float curBend = std::acos(std::clamp(U.Dot(L), -1.0f, 1.0f));
        const float cosInterior = std::clamp(
            (upperLen * upperLen + lowerLen * lowerLen - dist * dist) / (2.0f * upperLen * lowerLen),
            -1.0f, 1.0f);
        const float wantedBend = DirectX::XM_PI - std::acos(cosInterior);
        if (checkF(curBend, "curBend") || checkF(wantedBend, "wantedBend")) return;

        // Safe matrix inversion lambda. Aborts inversion if determinant approaches zero (scale corruption).
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
                invM._11 = (a22 * a33 - a23 * a32) * invDet;
                invM._12 = -(a12 * a33 - a13 * a32) * invDet;
                invM._13 = (a12 * a23 - a13 * a22) * invDet;

                invM._21 = -(a21 * a33 - a23 * a31) * invDet;
                invM._22 = (a11 * a33 - a13 * a31) * invDet;
                invM._23 = -(a11 * a23 - a13 * a21) * invDet;

                invM._31 = (a21 * a32 - a22 * a31) * invDet;
                invM._32 = -(a11 * a32 - a12 * a31) * invDet;
                invM._33 = (a11 * a22 - a12 * a21) * invDet;

                float tx = m._41, ty = m._42, tz = m._43;
                invM._41 = -(tx * invM._11 + ty * invM._21 + tz * invM._31);
                invM._42 = -(tx * invM._12 + ty * invM._22 + tz * invM._32);
                invM._43 = -(tx * invM._13 + ty * invM._23 + tz * invM._33);

                return invM;
            };

        // Rotates the mid-joint relative to the calculated axis and bend delta.
        const Matrix bendRot = Matrix::CreateFromAxisAngle(axis, wantedBend - curBend);
        if (checkNaN(bendRot, "bendRot")) return;
        const Matrix newMidWorld = rotateInPlace(midWorld, bendRot);

        bool invertSuccess = false;
        Matrix rootWorldInv = safeInvert(rootWorld, invertSuccess);
        if (!invertSuccess) return;

        // Overwrites local mid matrix based on the new world configuration.
        localBones[midIdx] = newMidWorld * rootWorldInv;
        if (checkNaN(localBones[midIdx], "localBones[midIdx]")) return;

        // ---- 4. Swing the whole leg so the effector direction points at the target ------------
        // Computes the angular difference between the current effector direction and target direction.
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

            // Applies rotation to root bone to pivot the entire leg towards the target coordinate.
            const Matrix newRootWorld = rotateInPlace(rootWorld, swingRot);
            Matrix rootParentInv = safeInvert(rootParentWorld, invertSuccess);
            if (invertSuccess)
            {
                localBones[rootIdx] = newRootWorld * rootParentInv;
                if (checkNaN(localBones[rootIdx], "localBones[rootIdx]")) return;
            }
        }

        // ---- 5. Align the foot to the terrain normal ------------------------------------------
        // Recalculates mid bone world matrix reflecting the completed bend and swing operations.
        Matrix finalMidWorld = localBones[midIdx] * (localBones[rootIdx] * rootParentWorld);

        // Isolates original effector rotation while substituting the solved IK translation position.
        // Prevents the foot geometry from pitching downwards automatically when the knee bends.
        Matrix finalEffWorld = effWorld;
        finalEffWorld.Translation((localBones[effectorIdx] * finalMidWorld).Translation());

        // Aligns foot orientation by rotating the global upward vector towards the terrain normal.
        if (alignToTerrain && currentNormal.y > 0.001f)
        {
            Vector3 alignAxis = Vector3::Up.Cross(currentNormal);
            float alignSin = alignAxis.Length();
            float alignCos = Vector3::Up.Dot(currentNormal);

            if (alignSin > 1e-5f)
            {
                alignAxis.Normalize();
                float alignAngle = std::atan2(alignSin, alignCos);

                // Rotates foot based on angle scaled by active weight to retain swing phase transitions.
                Matrix alignRot = Matrix::CreateFromAxisAngle(alignAxis, alignAngle * activeWeight);
                finalEffWorld = rotateInPlace(finalEffWorld, alignRot);
            }
        }

        // Overwrites local effector matrix with final configuration.
        Matrix midInv = safeInvert(finalMidWorld, invertSuccess);
        if (invertSuccess)
        {
            localBones[effectorIdx] = finalEffWorld * midInv;
        }
    }
}