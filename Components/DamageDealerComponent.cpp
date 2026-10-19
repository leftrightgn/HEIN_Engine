#include "pch.h"
#include "DamageDealerComponent.h"
#include <ImGui/imgui.h>
#include <Message/Messenger.h>
#include <Entities/Actor.h>
#include <Components/SocketAttachmentComponent.h>

HEIN::DamageDealerComponent::DamageDealerComponent(Actor* owner)
	: IComponent(owner)
	, m_damageAmount(0.0f)
	, m_damageType(DamageType::Physical)
	, m_isActive(true)
	, m_causesStun(false)
	, m_knockbackForce(20.0f)
	, m_registeredListenID(INVALID_ACTOR_ID)
{
}

HEIN::DamageDealerComponent::~DamageDealerComponent()
{
	if (Messenger* msg = Messenger::GetInstance())
	{
		msg->UnRegisterObserver(this);
	}
	m_registeredListenID = INVALID_ACTOR_ID;
}

void HEIN::DamageDealerComponent::RegisterToWielderMessages()
{
	if (!m_owner) return;

	ActorID listenID = m_owner->GetOwnerID();
	if (listenID == INVALID_ACTOR_ID && m_owner->GetParentID() != INVALID_ACTOR_ID)
	{
		listenID = m_owner->GetParentID();
	}
	if (listenID == INVALID_ACTOR_ID)
	{
		if (auto* socketAttach = m_owner->GetComponent<SocketAttachmentComponent>())
		{
			listenID = socketAttach->GetSocketOwnerActorID();
		}
	}

	if (listenID != INVALID_ACTOR_ID && listenID != m_registeredListenID)
	{
		if (m_registeredListenID != INVALID_ACTOR_ID && Messenger::GetInstance())
		{
			Messenger::GetInstance()->UnRegisterObserver(this);
		}
		Messenger::GetInstance()->Register(listenID, this);
		m_registeredListenID = listenID;
	}
}

void HEIN::DamageDealerComponent::Start()
{
	RegisterToWielderMessages();
}

void HEIN::DamageDealerComponent::Update(float /*deltaTime*/)
{
	RegisterToWielderMessages();
}

void HEIN::DamageDealerComponent::OnMessageAccepted(Message::MessageID messageID)
{
	if (messageID == Message::SET_WEAPON_ACTIVE)
	{
		if (!m_isActive)
		{
			m_hitActorsThisSwing.clear();
		}
		m_isActive = true;
	}
	else if (messageID == Message::SET_WEAPON_INACTIVE)
	{
		m_isActive = false;
		m_hitActorsThisSwing.clear();
	}
	else if (messageID == Message::RESET_WEAPON_SWING)
	{
		m_hitActorsThisSwing.clear();
	}
}

void HEIN::DamageDealerComponent::Initialize(float damageAmount, DamageType damageType, bool causesStun, float knockbackForce)
{
	m_damageAmount = damageAmount;
	m_damageType = damageType;
	m_causesStun = causesStun;
	m_knockbackForce = knockbackForce;
	m_hitstopFrames = causesStun ? 8 : 5;
}

nlohmann::json HEIN::DamageDealerComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    data["DamageAmount"] = m_damageAmount;
    data["DamageType"] = static_cast<int>(m_damageType);
    data["IsActive"] = m_isActive;
    data["CausesStun"] = m_causesStun;
    data["KnockbackForce"] = m_knockbackForce;
    data["HitstopFrames"] = m_hitstopFrames;
    return data;
}

void HEIN::DamageDealerComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("DamageAmount")) m_damageAmount = data["DamageAmount"];
    if (data.contains("DamageType")) m_damageType = static_cast<DamageType>(data["DamageType"]);
    if (data.contains("IsActive")) m_isActive = data["IsActive"];
    if (data.contains("CausesStun")) m_causesStun = data["CausesStun"];
    if (data.contains("KnockbackForce"))
        m_knockbackForce = data["KnockbackForce"];
    else
        m_knockbackForce = 20.0f;

    if (data.contains("HitstopFrames"))
        m_hitstopFrames = data["HitstopFrames"];
    else
        m_hitstopFrames = m_causesStun ? 8 : 5;
}

void HEIN::DamageDealerComponent::OnInspectorGUI(GameContext& gameContext)
{
    if (ImGui::CollapsingHeader("DamageDealerComponent", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Damage Amount", &m_damageAmount, 0.5f);
        ImGui::DragFloat("Knockback Force", &m_knockbackForce, 0.5f, 0.0f, 200.0f);
        ImGui::SliderInt("Hitstop Frames (60fps)", &m_hitstopFrames, 0, 20);
        ImGui::Checkbox("Is Active", &m_isActive);
        ImGui::Checkbox("Causes Stun", &m_causesStun);

        const char* damageTypeNames[] = { "Physical", "Magical", "Fire", "Poison", "Stun" };
        int currentType = static_cast<int>(m_damageType);
        if (currentType >= 0 && currentType < static_cast<int>(IM_ARRAYSIZE(damageTypeNames)))
        {
            if (ImGui::Combo("Damage Type", &currentType, damageTypeNames, IM_ARRAYSIZE(damageTypeNames)))
            {
                m_damageType = static_cast<DamageType>(currentType);
            }
        }
    }
}
