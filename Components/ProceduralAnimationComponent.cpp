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

        const DirectX::SimpleMath::Matrix* currentLocalBones = m_skinnedModel->GetCurrentLocalBones();
        if (!currentLocalBones)
            return;

        const size_t boneCount = m_skinnedModel->GetBoneCount();
        if (boneCount == 0)
            return;

        TransformComponent* transform = m_owner->GetComponent<TransformComponent>();
        if (!transform)
            return;

        std::vector<DirectX::SimpleMath::Matrix> modifiedBones(currentLocalBones, currentLocalBones + boneCount);
        const DirectX::SimpleMath::Matrix actorWorld = transform->GetWorldMatrix();

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
        DirectX::SimpleMath::Matrix* localBones,
        int rootIdx,
        int midIdx,
        int effectorIdx,
        int toeIdx,
        const DirectX::SimpleMath::Matrix& worldMatrix,
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

        // ==================================================================================
        // ANALYTICAL TWO-BONE INVERSE KINEMATICS & TERRAIN ADAPTATION PIPELINE
        // ==================================================================================
        // Solves a 3-joint kinematic chain (Root/Thigh -> Mid/Knee -> Effector/Ankle)
        // using the Law of Cosines combined with swing-twist rotational decomposition,
        // dual-point ground raycasting, temporal smoothing, and terrain surface normal alignment.
        // ==================================================================================

        // Stage 1: Coordinate Space Transformations & Model Hierarchy Traversal
        // Computes model-space transformation matrix for any bone index by walking up parent links:
        // M_model = M_local[idx] * M_local[parent] * ... * M_local[root]
        auto modelMatrix = [&](int idx) -> DirectX::SimpleMath::Matrix
            {
                DirectX::SimpleMath::Matrix m = localBones[idx];
                for (int p = m_skinnedModel->GetParentBoneIndex(idx);
                    p >= 0;
                    p = m_skinnedModel->GetParentBoneIndex(p))
                {
                    m = m * localBones[p];
                }
                return m;
            };

        // Rigid body rotation around pivot: preserves translation while rotating orientation:
        // R_pivot(M, R) = T(pos) * R * T(-pos) * M_untranslated
        auto rotateInPlace = [](const DirectX::SimpleMath::Matrix& m, const DirectX::SimpleMath::Matrix& rot) -> DirectX::SimpleMath::Matrix
            {
                DirectX::SimpleMath::Matrix r = m;
                const DirectX::SimpleMath::Vector3 t = m.Translation();
                r.Translation(DirectX::SimpleMath::Vector3::Zero);
                r = r * rot;
                r.Translation(t);
                return r;
            };

        // Stage 2: Unmodified Animation World Pose Evaluation
        // Compute world-space matrices and translations for root, mid, and effector before IK adjustment
        const int rootParent = m_skinnedModel->GetParentBoneIndex(rootIdx);
        const DirectX::SimpleMath::Matrix rootParentModel = (rootParent >= 0) ? modelMatrix(rootParent) : DirectX::SimpleMath::Matrix::Identity;
        const DirectX::SimpleMath::Matrix rootParentWorld = rootParentModel * worldMatrix;

        const DirectX::SimpleMath::Matrix rootWorld = localBones[rootIdx] * rootParentWorld;
        const DirectX::SimpleMath::Matrix midWorld = localBones[midIdx] * rootWorld;
        const DirectX::SimpleMath::Matrix effWorld = localBones[effectorIdx] * midWorld;

        const DirectX::SimpleMath::Vector3 rootPos = rootWorld.Translation();
        const DirectX::SimpleMath::Vector3 midPos = midWorld.Translation();
        const DirectX::SimpleMath::Vector3 effPos = effWorld.Translation();

        // Bone segment lengths (invariant under rigid skeletal kinematics)
        const float upperLen = DirectX::SimpleMath::Vector3::Distance(rootPos, midPos);
        const float lowerLen = DirectX::SimpleMath::Vector3::Distance(midPos, effPos);
        if (upperLen < 0.0001f || lowerLen < 0.0001f)
            return;

        // Stage 3: Dynamic Target Coordinate & Footprint Ground Sampling
        DirectX::SimpleMath::Vector3 targetPos = effPos;
        DirectX::SimpleMath::Vector3 m_footNormal = DirectX::SimpleMath::Vector3::Up;
        float targetOffset = 0.0f;

        // Swing Phase Masking:
        // Evaluates vertical distance between foot and actor root.
        // During gait swing phase (foot lifted off the floor), fade IK influence to 0
        // to preserve natural walk/run animation curves and prevent legs sticking to ground.
        float footHeightRelative = effPos.y - worldMatrix.Translation().y;
        float swingMask = 1.0f;

        if (footHeightRelative > 1.5f)
        {
            swingMask = std::clamp(1.0f - ((footHeightRelative - 1.20f) * 4.0f), 0.0f, 1.0f);
        }
        float activeWeight = weight * swingMask;

        if (alignToTerrain && (m_terrain || m_meshCollider))
        {
            const DirectX::SimpleMath::Vector3 actorPos = worldMatrix.Translation();
            float groundAtFoot = 0.0f, groundAtActor = 0.0f;

            // Multi-surface raycast: queries heightmap and static mesh triangle BVH for highest contact
            auto getGroundHeight = [&](const DirectX::SimpleMath::Vector3& pos, float& outHeight, DirectX::SimpleMath::Vector3* outNormal = nullptr) -> bool {
                bool hitAny = false;
                float bestHeight = -FLT_MAX;
                DirectX::SimpleMath::Vector3 bestNorm = DirectX::SimpleMath::Vector3::Up;

                if (m_terrain)
                {
                    DirectX::SimpleMath::Vector3 n;
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
                    DirectX::SimpleMath::Vector3 rayOrigin(pos.x, pos.y + 10.0f, pos.z);
                    DirectX::SimpleMath::Vector3 rayDir = DirectX::SimpleMath::Vector3::Down;
                    float closestDist = FLT_MAX;
                    bool hitMesh = false;
                    DirectX::SimpleMath::Vector3 meshNormal = DirectX::SimpleMath::Vector3::Up;
                    for (const auto& tri : m_meshCollider->GetWorldTriangles())
                    {
                        float dist;
                        DirectX::SimpleMath::Vector3 n;
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

            // Dual-Point Sampling: Toe and Heel offsets along horizontal foot direction
            DirectX::SimpleMath::Vector3 footDir = worldMatrix.Forward();
            footDir.y = 0.0f;
            footDir.Normalize();

            DirectX::SimpleMath::Vector3 toePos = effPos + footDir * 0.15f;
            DirectX::SimpleMath::Vector3 heelPos = effPos - footDir * 0.05f;

            if (toeIdx >= 0 && toeIdx < boneCount)
            {
                DirectX::SimpleMath::Matrix toeWorldMat = modelMatrix(toeIdx) * worldMatrix;
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
            DirectX::SimpleMath::Vector3 terrainNormal = DirectX::SimpleMath::Vector3::Up;

            bool hitActor = getGroundHeight(actorPos, groundAtActor);
            bool hitToe = getGroundHeight(toePos, toeHeight, &terrainNormal);
            bool hitHeel = getGroundHeight(heelPos, heelHeight);

            if (hitActor || hitToe || hitHeel)
            {
                if (!hitActor) groundAtActor = hitToe ? toeHeight : heelHeight;
                if (!hitToe) toeHeight = groundAtActor;
                if (!hitHeel) heelHeight = groundAtActor;
              
                if (hitToe) {
                    m_footNormal = terrainNormal;
                }
                m_footNormal.Normalize();

                // Compute optimal ankle elevation: weighted average prevents toe/heel ground clipping
                float optimalAnkleHeight = heelHeight + (toeHeight - heelHeight) * 0.25f;
                const float maxStep = 5.0f;
                targetOffset = std::clamp(optimalAnkleHeight - groundAtActor, -maxStep, maxStep);
            }
        }

        // Stage 4: Temporal Low-Pass Filtering (Anti-Popping Filter)
        // Uses frame-rate independent exponential smoothing to eliminate high-frequency jitter:
        // offset(t) = offset(t-1) + (target - offset(t-1)) * (1 - e^(-k * dt))
        float lerpSpeed = 15.0f * deltaTime;
        lerpSpeed = std::clamp(lerpSpeed, 0.0f, 1.0f);

        currentOffset = currentOffset + (targetOffset - currentOffset) * lerpSpeed;
        currentNormal = DirectX::SimpleMath::Vector3::Lerp(currentNormal, m_footNormal, lerpSpeed);
        currentNormal.Normalize();

        targetPos.y += currentOffset * activeWeight;
        targetPos.y += heightOffset;

        // Stage 5: Reach Boundary Clamping (Triangle Inequality Enforcement)
        // Maximum reachable distance: d_max = upperLen + lowerLen - epsilon (avoids singular stretch)
        // Minimum foldable distance: d_min = |upperLen - lowerLen| + epsilon (avoids full self-collapse)
        DirectX::SimpleMath::Vector3 toTarget = targetPos - rootPos;
        float dist = toTarget.Length();
        if (dist < 0.0001f)
            return;

        const float maxDist = upperLen + lowerLen - 0.001f;
        const float minDist = std::min(maxDist, std::abs(upperLen - lowerLen) + 0.001f);
        dist = std::clamp(dist, minDist, maxDist);
        toTarget.Normalize();
        targetPos = rootPos + toTarget * dist;

        // Stage 6: Analytical Knee Hinge Solve via the Law of Cosines
        // For triangle with sides a = upperLen, b = lowerLen, c = dist:
        // cos(theta_interior) = (a^2 + b^2 - c^2) / (2 * a * b)
        // Knee bend angle: wantedBend = XM_PI - theta_interior
        DirectX::SimpleMath::Vector3 U = midPos - rootPos;  U.Normalize();
        DirectX::SimpleMath::Vector3 L = effPos - midPos;   L.Normalize();
       
        // Knee hinge rotation axis: perpendicular to plane formed by thigh and calf vectors
        DirectX::SimpleMath::Vector3 axis = U.Cross(L);
        if (axis.LengthSquared() < 1e-8f)
        {
            // Collinear fallback: use root bone lateral right axis when leg is fully straight
            axis = rootWorld.Right();
        }
        axis.Normalize();
        
        const float curBend = std::acos(std::clamp(U.Dot(L), -1.0f, 1.0f));
        const float cosInterior = std::clamp(
            (upperLen * upperLen + lowerLen * lowerLen - dist * dist) / (2.0f * upperLen * lowerLen),
            -1.0f, 1.0f);
        const float wantedBend = DirectX::XM_PI - std::acos(cosInterior);
       
        // Safe matrix inversion lambda via Cramer's Rule for 3D affine transformations.
        // Computes analytic 3x3 cofactor determinant and inverts translation block.
        // Aborts inversion if determinant approaches zero (scale singularity).
        auto safeInvert = [](const DirectX::SimpleMath::Matrix& m, bool& outSuccess) -> DirectX::SimpleMath::Matrix
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
                    return DirectX::SimpleMath::Matrix::Identity;
                }

                float invDet = 1.0f / det;
                outSuccess = true;

                DirectX::SimpleMath::Matrix invM = DirectX::SimpleMath::Matrix::Identity;
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

        // Stage 7: Knee Hinge Rotation & Mid Joint Local Matrix Update
        // Rotate mid joint around hinge axis by angular difference delta = wantedBend - curBend
        const DirectX::SimpleMath::Matrix bendRot = DirectX::SimpleMath::Matrix::CreateFromAxisAngle(axis, wantedBend - curBend);
        const DirectX::SimpleMath::Matrix newMidWorld = rotateInPlace(midWorld, bendRot);

        bool invertSuccess = false;
        DirectX::SimpleMath::Matrix rootWorldInv = safeInvert(rootWorld, invertSuccess);
        if (!invertSuccess) return;

        // Reconstruct local mid matrix: M_local[mid] = M_world[mid, new] * (M_world[root])^-1
        localBones[midIdx] = newMidWorld * rootWorldInv;
    
        // Stage 8: Whole-Leg Swing Rotation to Aim Effector at Target Coordinate
        // Evaluates angular difference between solved leg direction (root -> effector) and target vector (root -> target):
        // cos(theta) = curDir . toTarget, sin(theta) = ||curDir x toTarget||
        const DirectX::SimpleMath::Matrix newEffWorld = localBones[effectorIdx] * newMidWorld;
        DirectX::SimpleMath::Vector3 curDir = newEffWorld.Translation() - rootPos;
        curDir.Normalize();
       
        const DirectX::SimpleMath::Vector3 swingAxis = curDir.Cross(toTarget);
        const float swingSin = swingAxis.Length();
        const float swingCos = curDir.Dot(toTarget);

        if (swingSin > 1e-6f)
        {
            const float angle = std::atan2(swingSin, swingCos);
            const DirectX::SimpleMath::Matrix swingRot = DirectX::SimpleMath::Matrix::CreateFromAxisAngle(swingAxis / swingSin, angle);
            
            // Apply swing rotation to root bone and recalculate local root bone matrix:
            // M_local[root] = (M_pivot(M_world[root], swingRot)) * (M_world[rootParent])^-1
            const DirectX::SimpleMath::Matrix newRootWorld = rotateInPlace(rootWorld, swingRot);
            DirectX::SimpleMath::Matrix rootParentInv = safeInvert(rootParentWorld, invertSuccess);
            if (invertSuccess)
            {
                localBones[rootIdx] = newRootWorld * rootParentInv;
            }
        }

        // Stage 9: Foot Orientation & Terrain Normal Alignment
        // Recalculate intermediate mid world matrix post-swing:
        DirectX::SimpleMath::Matrix finalMidWorld = localBones[midIdx] * (localBones[rootIdx] * rootParentWorld);

        // Preserve unconstrained ankle rotation while translating to solved IK position:
        // Decouples foot pitch from knee bend to prevent unnatural toe dipping.
        DirectX::SimpleMath::Matrix finalEffWorld = effWorld;
        finalEffWorld.Translation((localBones[effectorIdx] * finalMidWorld).Translation());

        // Align foot bottom with terrain contact normal vector via shortest-arc quaternion/axis-angle:
        // axis = globalUp x terrainNormal, angle = atan2(||axis||, globalUp . terrainNormal)
        if (alignToTerrain && currentNormal.y > 0.001f)
        {
            DirectX::SimpleMath::Vector3 alignAxis = DirectX::SimpleMath::Vector3::Up.Cross(currentNormal);
            float alignSin = alignAxis.Length();
            float alignCos = DirectX::SimpleMath::Vector3::Up.Dot(currentNormal);

            if (alignSin > 1e-5f)
            {
                alignAxis.Normalize();
                float alignAngle = std::atan2(alignSin, alignCos);

                // Blend rotation angle by active weight (faded during swing phase)
                DirectX::SimpleMath::Matrix alignRot = DirectX::SimpleMath::Matrix::CreateFromAxisAngle(alignAxis, alignAngle * activeWeight);
                finalEffWorld = rotateInPlace(finalEffWorld, alignRot);
            }
        }

        // Stage 10: Local Effector Matrix Reconstruction
        // M_local[effector] = M_world[effector, final] * (M_world[mid, final])^-1
        DirectX::SimpleMath::Matrix midInv = safeInvert(finalMidWorld, invertSuccess);
        if (invertSuccess)
        {
            localBones[effectorIdx] = finalEffWorld * midInv;
        }
    }
}