#pragma once
#include <string>

namespace HEIN
{
	enum class HumanoidBone
	{
		Hips,
		Spine,
		Spine1,
		Spine2,
		Neck,
		Head,
		HeadTop,
		LeftShoulder,
		LeftArm,
		LeftForeArm,
		LeftHand,
		RightShoulder,
		RightArm,
		RightForeArm,
		RightHand,
		LeftUpLeg,
		LeftLeg,
		LeftFoot,
		LeftToe,
		RightUpLeg,
		RightLeg,
		RightFoot,
		RightToe
	};

	/**
	 * @brief Modular bone profile mapping logical humanoid bones to model-specific rig bone names.
	 * Decouples game logic, weapon sockets, and collider attachments from rig-specific prefixes ("mixamorig:").
	 */
	struct SkeletonProfile
	{
		std::wstring hips = L"mixamorig:Hips";
		std::wstring spine = L"mixamorig:Spine";
		std::wstring spine1 = L"mixamorig:Spine1";
		std::wstring spine2 = L"mixamorig:Spine2";
		std::wstring neck = L"mixamorig:Neck";
		std::wstring head = L"mixamorig:Head";
		std::wstring headTop = L"mixamorig:HeadTop_End";

		std::wstring leftShoulder = L"mixamorig:LeftShoulder";
		std::wstring leftArm = L"mixamorig:LeftArm";
		std::wstring leftForeArm = L"mixamorig:LeftForeArm";
		std::wstring leftHand = L"mixamorig:LeftHand";

		std::wstring rightShoulder = L"mixamorig:RightShoulder";
		std::wstring rightArm = L"mixamorig:RightArm";
		std::wstring rightForeArm = L"mixamorig:RightForeArm";
		std::wstring rightHand = L"mixamorig:RightHand";

		std::wstring leftUpLeg = L"mixamorig:LeftUpLeg";
		std::wstring leftLeg = L"mixamorig:LeftLeg";
		std::wstring leftFoot = L"mixamorig:LeftFoot";
		std::wstring leftToe = L"mixamorig:LeftToe_End";

		std::wstring rightUpLeg = L"mixamorig:RightUpLeg";
		std::wstring rightLeg = L"mixamorig:RightLeg";
		std::wstring rightFoot = L"mixamorig:RightFoot";
		std::wstring rightToe = L"mixamorig:RightToe_End";

		const std::wstring& GetBone(HumanoidBone bone) const
		{
			switch (bone)
			{
			case HumanoidBone::Hips:          return hips;
			case HumanoidBone::Spine:         return spine;
			case HumanoidBone::Spine1:        return spine1;
			case HumanoidBone::Spine2:        return spine2;
			case HumanoidBone::Neck:          return neck;
			case HumanoidBone::Head:          return head;
			case HumanoidBone::HeadTop:       return headTop;
			case HumanoidBone::LeftShoulder:  return leftShoulder;
			case HumanoidBone::LeftArm:       return leftArm;
			case HumanoidBone::LeftForeArm:   return leftForeArm;
			case HumanoidBone::LeftHand:      return leftHand;
			case HumanoidBone::RightShoulder: return rightShoulder;
			case HumanoidBone::RightArm:      return rightArm;
			case HumanoidBone::RightForeArm:  return rightForeArm;
			case HumanoidBone::RightHand:     return rightHand;
			case HumanoidBone::LeftUpLeg:     return leftUpLeg;
			case HumanoidBone::LeftLeg:       return leftLeg;
			case HumanoidBone::LeftFoot:      return leftFoot;
			case HumanoidBone::LeftToe:       return leftToe;
			case HumanoidBone::RightUpLeg:    return rightUpLeg;
			case HumanoidBone::RightLeg:      return rightLeg;
			case HumanoidBone::RightFoot:     return rightFoot;
			case HumanoidBone::RightToe:      return rightToe;
			default:                          return hips;
			}
		}

		static const SkeletonProfile& DefaultMixamo()
		{
			static SkeletonProfile s_defaultMixamo;
			return s_defaultMixamo;
		}

		static const SkeletonProfile& UnrealMannequin()
		{
			static SkeletonProfile s_unreal;
			s_unreal.hips = L"pelvis";
			s_unreal.spine = L"spine_01";
			s_unreal.spine1 = L"spine_02";
			s_unreal.spine2 = L"spine_03";
			s_unreal.neck = L"neck_01";
			s_unreal.head = L"head";
			s_unreal.headTop = L"head";
			s_unreal.leftShoulder = L"clavicle_l";
			s_unreal.leftArm = L"upperarm_l";
			s_unreal.leftForeArm = L"lowerarm_l";
			s_unreal.leftHand = L"hand_l";
			s_unreal.rightShoulder = L"clavicle_r";
			s_unreal.rightArm = L"upperarm_r";
			s_unreal.rightForeArm = L"lowerarm_r";
			s_unreal.rightHand = L"hand_r";
			s_unreal.leftUpLeg = L"thigh_l";
			s_unreal.leftLeg = L"calf_l";
			s_unreal.leftFoot = L"foot_l";
			s_unreal.leftToe = L"ball_l";
			s_unreal.rightUpLeg = L"thigh_r";
			s_unreal.rightLeg = L"calf_r";
			s_unreal.rightFoot = L"foot_r";
			s_unreal.rightToe = L"ball_r";
			return s_unreal;
		}
	};
}
