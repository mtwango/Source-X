
#include "../common/resource/CResourceLock.h"
//#include "../common/CExpression.h" // included in the precompiled header
#include "../common/CLog.h"
#include "../common/CUOInstall.h"
#include "../game/chars/CChar.h"
#include "../game/components/CCItemDamageable.h"
#include "../game/components/CCPropsChar.h"
#include "../game/clients/CClient.h"
#include "../game/clients/CClientTooltip.h"
#include "../game/clients/CParty.h"
#include "../game/items/CItemCorpse.h"
#include "../game/items/CItemMap.h"
#include "../game/items/CItemMessage.h"
#include "../game/items/CItemMultiCustom.h"
#include "../game/items/CItemStone.h"
#include "../game/items/CItemVendable.h"
#include "../game/uo_files/uofiles_enums_creid.h"
#include "../game/CServer.h"
#include "../game/CServerConfig.h"
#include "../game/CWorldGameTime.h"
#include "CNetState.h"
#include "CNetworkManager.h"
#include "send.h"

namespace zlib {
#include <zlib/zlib.h>
}
#include <algorithm>


/***************************************************************************
 *
 *
 *	Packet **** : PacketGeneric				Temporary packet till all will be redone! (NORMAL)
 *
 *
 ***************************************************************************/
/*
PacketGeneric::PacketGeneric(const CClient* target, byte *data, uint length) : PacketSend(0, length, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketGeneric::PacketGeneric");

	seek();
	writeData(data, length);
	push(target);
}
*/


/***************************************************************************
 *
 *
 *	Packet **** : PacketTelnet				send message to telnet client (NORMAL)
 *
 *
 ***************************************************************************/
PacketTelnet::PacketTelnet(const CClient* target, const lpctstr message, const bool fNullTerminated) : PacketSend(0, 0, PRI_HIGHEST)
{
	ADDTOCALLSTACK("PacketTelnet::PacketTelnet");

	seek();

	for (uint i = 0; message[i] != '\0'; ++i)
	{
		if (message[i] == '\n')
			writeCharASCII('\r');

		writeCharASCII(message[i]);
	}
	if (fNullTerminated)
		writeCharASCII('\0');

	trim();
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet **** : PacketWeb					send message to web client (NORMAL)
 *
 *
 ***************************************************************************/
PacketWeb::PacketWeb(const CClient * target, const byte * data, const uint length) : PacketSend(0, 0, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketWeb::PacketWeb");

	if (data != nullptr && length > 0)
		setData(data, length);

	if (target != nullptr)
		push(target);
}

void PacketWeb::setData(const byte * data, const uint length)
{
	seek();
	writeData(data, length);
	trim();
}


/***************************************************************************
 *
 *
 *	Packet 0x0B : PacketCombatDamage		sends notification of got damage (NORMAL)
 *
 *
 ***************************************************************************/
PacketCombatDamage::PacketCombatDamage(const CClient* target, word damage, const CUID &defender) : PacketSend(XCMD_DamagePacket, 7, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketCombatDamage::PacketCombatDamage");

	if ( damage >= UINT16_MAX )
		damage = UINT16_MAX;

	writeInt32(defender);
	writeInt16(damage);
	push(target);
}

bool PacketCombatDamage::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_NEWDAMAGE);
}


/***************************************************************************
 *
 *
 *	Packet 0x11 : PacketObjectStatus		sends status window data (LOW)
 *
 *
 ***************************************************************************/
PacketObjectStatus::PacketObjectStatus(const CClient* target, CObjBase* object) : PacketSend(XCMD_Status, 7, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketObjectStatus::PacketObjectStatus");
    ASSERT(object);

	const CNetState * state = target->GetNetState();
	const CChar *character = target->GetChar();
    CChar *objectChar = object->IsChar() ? static_cast<CChar *>(object) : nullptr;
	bool fCanRename = false;

	byte version = 0;

	initLength();

	writeInt32(object->GetUID());
	writeStringFixedASCII(object->GetName(), 30);

	if (state->isClientVersionNumber(MINCLIVER_STATUS_V6))
		version = 6;
	else if (state->isClientVersionNumber(MINCLIVER_STATUS_V5))
		version = 5;
	else if (state->isClientVersionNumber(MINCLIVER_STATUS_V4))
		version = 4;
	else if (state->isClientVersionNumber(MINCLIVER_STATUS_V3))
		version = 3;
	else if (state->isClientVersionNumber(MINCLIVER_STATUS_V2))
		version = 2;
	else
		version = 1;

	if (objectChar && (objectChar == character))
	{
		// Send real (not percentual) hitpoints if these infos regard myself
		writeInt16(objectChar->Stat_GetVal(STAT_STR));
		writeInt16(objectChar->Stat_GetMaxAdjusted(STAT_STR));
		writeBool(fCanRename);
		writeByte(version);
		WriteVersionSpecific(target, objectChar, version);
	}
	else
	{
        word iHitsCurrent = 0;
        const word iHitsMax = 100;
		if ( objectChar )
		{
            fCanRename = objectChar->IsOwnedBy(character);
		    const ushort tmpMaxHits = objectChar->Stat_GetMaxAdjusted(STAT_STR);
		    iHitsCurrent = static_cast<word>((objectChar->Stat_GetVal(STAT_STR) * 100) / maximum(tmpMaxHits, 1));
		}
		else
		{
            if (object->IsItem() ? dynamic_cast<const CItem *>(object) : nullptr)
            {
                if (const auto *const pItem = dynamic_cast<CCItemDamageable *>(object->GetComponent(COMP_ITEMDAMAGEABLE)))
                {
                    const ushort tmpMaxHits = pItem->GetMaxHits();
                    iHitsCurrent = static_cast<word>((pItem->GetCurHits() * 100) / maximum(tmpMaxHits, 1));
                }
                else
                {
                    iHitsCurrent = iHitsMax;  // Can't get hitpoints, asume 100%
                }
            }
		}

		// Send percentual hitpoints
        writeInt16(iHitsCurrent);		// Max hit points
		writeInt16(iHitsMax);		// Max hit points
		writeBool(fCanRename);
		writeByte(version);
        if (state->isClientEnhanced() && objectChar && objectChar->IsClientType() /*objectChar->IsPlayableCharacter()*/)
		{
			// The Enhanced Client wants the char race and other things when showing paperdolls (otherwise the interface throws an "unnoticeable" internal error)
			WriteVersionSpecific(target, objectChar, version);
		}
	}

	push(target);
}

void PacketObjectStatus::WriteVersionSpecific(const CClient* target, CChar* other, const byte version)
{
    const bool fElemental = IsSetCombatFlags(COMBAT_ELEMENTAL_ENGINE);
	const CCharBase * otherDefinition = other->Char_GetDef();
	const CCPropsChar* pCCPChar = other->GetComponentProps<CCPropsChar>();
	const CCPropsChar* pBaseCCPChar = otherDefinition->GetComponentProps<CCPropsChar>();

	writeBool(otherDefinition->IsFemale());
	writeInt16(other->Stat_GetAdjusted(STAT_STR));
	writeInt16(other->Stat_GetAdjusted(STAT_DEX));
	writeInt16(other->Stat_GetAdjusted(STAT_INT));
	writeInt16(other->Stat_GetVal(STAT_DEX));
	writeInt16(other->Stat_GetMaxAdjusted(STAT_DEX));
	writeInt16(other->Stat_GetVal(STAT_INT));
	writeInt16(other->Stat_GetMaxAdjusted(STAT_INT));

    if (g_Cfg.m_iFeatureTOL & FEATURE_TOL_VIRTUALGOLD)
    {
        writeInt32(static_cast<dword>(other->m_virtualGold));
    }
    else if (g_Cfg.m_fPayFromPackOnly)
    {
        writeInt32(other->GetPackSafe()->ContentCount(CResourceID(RES_TYPEDEF, IT_GOLD)));
    }
    else
    {
        writeInt32(other->ContentCount(CResourceID(RES_TYPEDEF, IT_GOLD)));
    }

	if (fElemental)
		writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_RESPHYSICAL, pBaseCCPChar)));
	else
		writeInt16(other->m_defense + otherDefinition->m_defense);

	writeInt16(static_cast<word>(other->GetTotalWeight() / WEIGHT_UNITS));

	if (version >= 5) // ML attributes
	{
		writeInt16(static_cast<word>(g_Cfg.Calc_MaxCarryWeight(other) / WEIGHT_UNITS));

		switch (other->GetDispID())
		{
		case CREID_MAN:
		case CREID_WOMAN:
		case CREID_GHOSTMAN:
		case CREID_GHOSTWOMAN:
			writeByte(RACETYPE_HUMAN);
			break;
		case CREID_ELFMAN:
		case CREID_ELFWOMAN:
		case CREID_ELFGHOSTMAN:
		case CREID_ELFGHOSTWOMAN:
			writeByte(RACETYPE_ELF);
			break;
		case CREID_GARGMAN:
		case CREID_GARGWOMAN:
		case CREID_GARGGHOSTMAN:
		case CREID_GARGGHOSTWOMAN:
			writeByte(RACETYPE_GARGOYLE);
			break;
		default:
			writeByte(RACETYPE_UNDEFINED);
			break;
		}
	}

	if (version >= 2) // T2A attributes
	{
		writeInt16(static_cast<word>(other->Stat_GetSumLimit()));
	}

	if (version >= 3) // Renaissance attributes
	{
		if (other->m_pPlayer != nullptr)
		{
            writeByte(static_cast<byte>(other->GetCurFollowers()));
			writeByte(static_cast<byte>(other->GetDefNum("MAXFOLLOWER", true)));
		}
		else
		{
			writeByte(0);
			writeByte(0);
		}
	}

	if (version >= 4) // AOS attributes
	{
        if (fElemental || g_Cfg.m_fDisplayElementalResistance)
        {
            writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_RESFIRE, pBaseCCPChar)));
            writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_RESCOLD, pBaseCCPChar)));
            writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_RESPOISON, pBaseCCPChar)));
            writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_RESENERGY, pBaseCCPChar)));
        }
        else
        {
            writeInt64(0);
        }
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_LUCK, pBaseCCPChar)));

		const CItem* weapon = other->m_uidWeapon.ItemFind();
		writeInt16(static_cast<word>(other->Fight_CalcDamage(weapon, true, false)));
		writeInt16(static_cast<word>(other->Fight_CalcDamage(weapon, true, true)));

		writeInt32(static_cast<dword>(other->GetDefNum("TITHING", true)));
	}

	if (version >= 6)	// SA attributes
	{
        if (fElemental || g_Cfg.m_fDisplayElementalResistance)
        {
            writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_RESPHYSICALMAX, pBaseCCPChar)));
            writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_RESFIREMAX, pBaseCCPChar)));
            writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_RESCOLDMAX, pBaseCCPChar)));
            writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_RESPOISONMAX, pBaseCCPChar)));
            writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_RESENERGYMAX, pBaseCCPChar)));
        }
        else
        {
            writeInt16(0);
            writeInt64(0);
        }
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_INCREASEDEFCHANCE, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_INCREASEDEFCHANCEMAX, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_INCREASEHITCHANCE, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_INCREASESWINGSPEED, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_INCREASEDAM, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_LOWERREAGENTCOST, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_INCREASESPELLDAM, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_FASTERCASTRECOVERY, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_FASTERCASTING, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_LOWERMANACOST, pBaseCCPChar)));
	}
	if (target->GetNetState()->isClientKR())
	{
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_INCREASEHITCHANCE, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_INCREASESWINGSPEED, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_INCREASEDAM, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_LOWERREAGENTCOST, pBaseCCPChar)));
		writeInt16(static_cast<word>(other->Stats_GetRegenRate(STAT_STR) / MSECS_PER_SEC));
		writeInt16(static_cast<word>(other->Stats_GetRegenRate(STAT_DEX) / MSECS_PER_SEC));
		writeInt16(static_cast<word>(other->Stats_GetRegenRate(STAT_INT) / MSECS_PER_SEC));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_REFLECTPHYSICALDAM, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_ENHANCEPOTIONS, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_INCREASEDEFCHANCE, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_INCREASESPELLDAM, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_FASTERCASTRECOVERY, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_FASTERCASTING, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->GetPropNum(pCCPChar, PROPCH_LOWERMANACOST, pBaseCCPChar)));
        writeInt16(static_cast<word>(other->Stat_GetMod(STAT_STR)));     // Gained from items with the prop BONUSSTR?
        writeInt16(static_cast<word>(other->Stat_GetMod(STAT_DEX)));     // Gained from items with the prop BONUSDEX?
        writeInt16(static_cast<word>(other->Stat_GetMod(STAT_INT)));     // Gained from items with the prop BONUSINT?
        writeInt16(static_cast<word>(other->GetDefNum("BONUSHITS", true)));    // What's that for? Should it be instead the Regen Val maybe?
        writeInt16(static_cast<word>(other->GetDefNum("BONUSSTAM", true)));    // What's that for? Should it be instead the Regen Val maybe?
        writeInt16(static_cast<word>(other->GetDefNum("BONUSMANA", true)));    // What's that for? Should it be instead the Regen Val maybe?
        writeInt16(static_cast<word>(other->Stat_GetMaxMod(STAT_STR)));  // Gained from items with the prop BONUSHITSMAX?
        writeInt16(static_cast<word>(other->Stat_GetMaxMod(STAT_DEX)));  // Gained from items with the prop BONUSSTAMMAX?
        writeInt16(static_cast<word>(other->Stat_GetMaxMod(STAT_INT)));  // Gained from items with the prop BONUSMANAMAX?
	}
}

/***************************************************************************
*
*
*	Packet 0x1 : PacketHealthBarUpdateNew		update health bar colour (LOW)
*
*
***************************************************************************/
PacketHealthBarUpdateNew::PacketHealthBarUpdateNew(const CClient* target, const CChar* character) : PacketSend(XCMD_HealthBarColorNew, 12, g_Cfg.m_fUsePacketPriorities ? PRI_LOW : PRI_NORMAL), m_character(character->GetUID())
{
    ADDTOCALLSTACK("PacketHealthBarUpdateNew::PacketHealthBarUpdateNew");

    word wColor = 0;
    if ( character->IsStatFlag(STATF_POISONED) )
        wColor = GreenBar;
    else if ( character->IsStatFlag(STATF_FREEZE|STATF_STONE) )
        wColor = YellowBar;

    initLength();

    writeInt32(character->GetUID());
    writeInt16(1);
    writeInt16(wColor);
    writeBool(wColor != 0);

    push(target);
}

bool PacketHealthBarUpdateNew::onSend(const CClient* client)
{
    ADDTOCALLSTACK("PacketHealthBarUpdateNew::onSend");
    if (g_NetworkManager.isOutputThreaded())
        return true;

    return client->CanSee(m_character.CharFind());
}

bool PacketHealthBarUpdateNew::CanSendTo(const CNetState* state) // static
{
    return state->isClientEnhanced();
}

/***************************************************************************
 *
 *
 *	Packet 0x17 : PacketHealthBarUpdate		update health bar colour (LOW)
 *
 *
 ***************************************************************************/
PacketHealthBarUpdate::PacketHealthBarUpdate(const CClient* target, const CChar* character) : PacketSend(XCMD_HealthBarColor, 15, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL), m_character(character->GetUID())
{
	ADDTOCALLSTACK("PacketHealthBarUpdate::PacketHealthBarUpdate");

	initLength();

	writeInt32(character->GetUID());

	writeInt16(2);
	writeInt16(GreenBar);
	writeByte(character->IsStatFlag(STATF_POISONED));
	writeInt16(YellowBar);
	writeByte(character->IsStatFlag(STATF_FREEZE|STATF_STONE));

	push(target);
}

bool PacketHealthBarUpdate::onSend(const CClient* client)
{
	ADDTOCALLSTACK("PacketHealthBarUpdate::onSend");
	if (g_NetworkManager.isOutputThreaded())
		return true;

	return client->CanSee(m_character.CharFind());
}

bool PacketHealthBarUpdate::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_SA) || state->isClientKR();
}


/***************************************************************************
 *
 *
 *	Packet 0x1A : PacketItemWorld			sends item on ground (NORMAL)
 *
 *
 ***************************************************************************/
PacketItemWorld::PacketItemWorld(const byte id, const uint size, const CUID& uid) : PacketSend(id, size, PRI_NORMAL), m_item(uid)
{
}

PacketItemWorld::PacketItemWorld(const CClient* target, const CItem *item) : PacketSend(XCMD_Put, 20, PRI_NORMAL), m_item(item->GetUID())
{
	ADDTOCALLSTACK("PacketItemWorld::PacketItemWorld");

	dword uid = item->GetUID();
	word amount = 0;
    if (item->GetType() == IT_CORPSE)
    {
        amount = item->GetAmount();  // char id of the corpse
    }
    else if (item->CanSendAmount())
    {
        if (const word itemAmount = item->GetAmount(); itemAmount > 1)
            amount = itemAmount;
    }
	ITEMID_TYPE id = item->GetDispID();
	CPointMap p = item->GetTopPoint();
	DIR_TYPE dir = DIR_N;
	HUE_TYPE hue = item->GetHue();
	byte flags = 0;
	byte light = 0;

	adjustItemData(target, item, id, hue, amount, dir, flags, light);

	// this packet only supports item ids up to 0x3fff, and multis start from 0x4000 (ITEMID_MULTI_LEGACY)
	// multis need to be adjusted to the lower range, and items between 03fff and 08000 need to be adjusted
	// to something safer
	if (id >= ITEMID_MULTI)
		id = static_cast<ITEMID_TYPE>(id - (ITEMID_MULTI - ITEMID_MULTI_LEGACY));
	else if (id >= ITEMID_MULTI_LEGACY)
		id = ITEMID_WorldGem;

	if (amount > 0)
		uid |= 0x80000000;
	else
		uid &= 0x7fffffff;

	p.m_x &= 0x7fff;
	if ( (dir > 0) || (light > 0) )	// with this packet the item can be flippable OR a light source, not both
		p.m_x |= 0x8000;
	p.m_y &= 0x3fff;
	if (hue > 0)
		p.m_y |= 0x8000;
	if (flags > 0)
		p.m_y |= 0x4000;

	initLength();
	writeInt32(uid);
	writeInt16(static_cast<word>(id));
	if (amount > 0)
		writeInt16(amount);
	writeInt16(p.m_x);
	writeInt16(p.m_y);
	if (light > 0)
		writeByte(light);
	else if (dir > 0)
		writeByte(static_cast<byte>(dir));
	writeByte(p.m_z);
	if (hue > 0)
		writeInt16(hue);
	if (flags > 0)
		writeByte(flags);

	push(target);
}

void PacketItemWorld::adjustItemData(const CClient* target, const CItem* item, ITEMID_TYPE &id, HUE_TYPE &hue, word &amount, DIR_TYPE &dir, byte &flags, byte& light)  // static
{
	ADDTOCALLSTACK("PacketItemWorld::adjustItemData");
	const CChar* character = target->GetChar();
	ASSERT(character);

	// modify the values for the specific client/item.
	if (id != ITEMID_CORPSE)
	{
        if (const CItemBase *itemDefintion = item->Item_GetDef(); itemDefintion && (target->GetResDisp() < itemDefintion->GetResLevel()))
		{
			id = static_cast<ITEMID_TYPE>(itemDefintion->GetResDispDnId());
			if (itemDefintion->GetResDispDnHue() != HUE_DEFAULT)
				hue = itemDefintion->GetResDispDnHue();
		}

		// on monster this just colors the underwear. thats it.
		if (hue & HUE_UNDERWEAR)
			hue = 0;
		else if ((hue & HUE_MASK_HI) > HUE_QTY)
			hue &= HUE_MASK_LO | HUE_TRANSLUCENT;
		else
			hue &= HUE_MASK_HI | HUE_TRANSLUCENT;
	}
	else
	{
		// adjust amount and hue of corpse if necessary
        if (const CCharBase *charDefinition = CCharBase::FindCharBase(item->m_itCorpse.m_BaseID);
            charDefinition && (target->GetResDisp() < charDefinition->GetResLevel()))
		{
			amount = charDefinition->GetResDispDnId();
			if (charDefinition->GetResDispDnHue() != HUE_DEFAULT)
				hue = charDefinition->GetResDispDnHue();
		}

		// allow HUE_UNDERWEAR colours only on corpses
		if ((hue & HUE_MASK_HI) > HUE_QTY)
			hue &= HUE_MASK_LO | HUE_UNDERWEAR | HUE_TRANSLUCENT;
		else
			hue &= HUE_MASK_HI | HUE_UNDERWEAR | HUE_TRANSLUCENT;

		dir = item->m_itCorpse.m_facing_dir;
	}

	if (character->CanMoveItem(item, false))
		flags |= ITEMF_MOVABLE;

	if (target->IsPriv(PRIV_DEBUG))
	{
		id = ITEMID_WorldGem;
		amount = 0;
		flags |= ITEMF_MOVABLE;
	}
	else
	{
		if (character->CanSee(item) == false)
			flags |= ITEMF_INVIS;

		if (item->Can(CAN_I_LIGHT))
		{
			if (item->IsTypeLit())
				light = item->m_itLight.m_pattern;
			else
				light = LIGHT_LARGE;
		}
	}
}

bool PacketItemWorld::onSend(const CClient* client)
{
	ADDTOCALLSTACK("PacketItemWorld::onSend");
	if (g_NetworkManager.isOutputThreaded())
		return true;

	return client->CanSee(m_item.ItemFind());
}


/***************************************************************************
 *
 *
 *	Packet 0x1B : PacketPlayerStart			allow client to start playing (HIGH)
 *
 *
 ***************************************************************************/
PacketPlayerStart::PacketPlayerStart(const CClient* target) : PacketSend(XCMD_Start, 37, g_Cfg.m_fUsePacketPriorities? PRI_HIGH : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketPlayerStart::PacketPlayerStart");

    const CChar* character = target->GetChar();
    if (character == nullptr)
    {
        return;
    }

	const CPointMap& pt = character->GetTopPoint();

	writeInt32(character->GetUID());
	writeInt32(0);
	writeInt16(static_cast<word>(character->GetDispID()));
	writeInt16(pt.m_x);
	writeInt16(pt.m_y);
    writeByte(0);
	writeByte(pt.m_z);
	writeByte(character->GetDirFlag());
	writeByte(0);
	writeInt32(0xffffffff);
	writeInt16(0);
	writeInt16(0);
	writeInt16(pt.m_map > 0 ? g_MapList.GetMapSizeX(pt.m_map) : 0x1800);
	writeInt16(pt.m_map > 0 ? g_MapList.GetMapSizeY(pt.m_map) : 0x1000);
	writeInt16(0);
	writeInt32(0);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x1C: PacketMessageASCII			show message to client (NORMAL)
 *
 *
 ***************************************************************************/
PacketMessageASCII::PacketMessageASCII(const CClient* target, const lpctstr pszText, const CObjBaseTemplate * source, const HUE_TYPE hue, const TALKMODE_TYPE mode, const FONT_TYPE font) : PacketSend(XCMD_Speak, 42, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketMessageASCII::PacketMessageASCII");

	initLength();

	if (source == nullptr)
		writeInt32(0xFFFFFFFF);
	else
		writeInt32(source->GetUID());

	if (source == nullptr || source->IsChar() == false)
	{
		writeInt16(0xFFFF);
	}
	else
	{
        const auto sourceCharacter = dynamic_cast<const CChar*>(source);
		ASSERT(sourceCharacter);
		writeInt16(static_cast<word>(sourceCharacter->GetDispID()));
	}

	writeByte(static_cast<byte>(mode));
	writeInt16(hue);
	writeInt16(font);

	// we need to ensure that the name is null terminated here when using TALKMODE_ITEM, otherwise
	// the journal can freeze and crash older client versions
	if (source == nullptr)
		writeStringFixedASCII("System", 30);
	else
		writeStringFixedASCII(source->GetName(), 30, true);

	writeStringASCII(pszText);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x1D : PacketRemoveObject		removes object from view (NORMAL)
 *
 *
 ***************************************************************************/
PacketRemoveObject::PacketRemoveObject(const CClient* target, const CUID &uid) : PacketSend(XCMD_Remove, 5, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketRemoveObject::PacketRemoveObject");

	writeInt32(uid);
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x20 : PacketPlayerUpdate		update player character on screen (NORMAL)
 *
 *
 ***************************************************************************/
PacketPlayerUpdate::PacketPlayerUpdate(const CClient* target) : PacketSend(XCMD_PlayerUpdate, 19, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketPlayerUpdate::PacketPlayerUpdate");
	//PS: This packet remove weather effects on client screen.

	const CChar* character = target->GetChar();
	ASSERT(character);

	CREID_TYPE id;
	HUE_TYPE hue;
	target->GetAdjustedCharID(character, id, hue);

	const CPointMap& pt = character->GetTopPoint();

	writeInt32(character->GetUID());
	writeInt16(static_cast<word>(id));
	writeByte(0);
	writeInt16(hue);
	writeByte(character->GetModeFlag(target));
	writeInt16(pt.m_x);
	writeInt16(pt.m_y);
	writeInt16(0);
	writeByte(character->GetDirFlag());
	writeByte(pt.m_z);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x21 : PacketMovementRej			rejects movement (HIGHEST)
 *
 *
 ***************************************************************************/
PacketMovementRej::PacketMovementRej(const CClient* target, const byte sequence) : PacketSend(XCMD_WalkReject, 8, g_Cfg.m_fUsePacketPriorities? PRI_HIGHEST : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketMovementRej::PacketMovementRej");

	const CChar* character = target->GetChar();
	ASSERT(character);

	const CPointMap& pt = character->GetTopPoint();
	writeByte(sequence);
	writeInt16(pt.m_x);
	writeInt16(pt.m_y);
	writeByte(character->GetDirFlag());
	writeByte(pt.m_z);
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x22 : PacketMovementAck			accepts movement (HIGHEST)
 *
 *
 ***************************************************************************/
PacketMovementAck::PacketMovementAck(const CClient* target, const byte sequence) : PacketSend(XCMD_WalkAck, 3, g_Cfg.m_fUsePacketPriorities? PRI_HIGHEST : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketMovementAck::PacketMovementAck");

	writeByte(sequence);
	writeByte(target->GetChar()->Noto_GetFlag(target->GetChar(), true, target->GetNetState()->isClientVersionNumber(MINCLIVER_NOTOINVUL), true));
	push(target);
}

/***************************************************************************
 *
 *
 *	Packet 0x23 : PacketDragAnimation		drag animation (LOW)
 *
 *
 ***************************************************************************/
PacketDragAnimation::PacketDragAnimation(const CChar* source, const CItem* item, const CObjBase* container, const CPointMap* pt) : PacketSend(XCMD_DragAnim, 26, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketDragAnimation::PacketDragAnimation");

	writeInt16(static_cast<word>(item->GetDispID()));
	writeByte(0);
	writeInt16(item->GetHue());
	writeInt16(item->CanSendAmount() ? item->GetAmount() : 1);

	const CPointMap& sourcepos = source->GetTopPoint();

	if (container != nullptr)
	{
		// item is being dragged into a container
		const CObjBaseTemplate* target = container->GetTopLevelObj();

	    if (target == nullptr)
	    {
	        return;
	    }

		const CPointMap& targetpos = target->GetTopPoint();

		writeInt32(source->GetUID());
		writeInt16(sourcepos.m_x);
		writeInt16(sourcepos.m_y);
		writeByte(sourcepos.m_z);
		writeInt32(target->GetUID());
		writeInt16(targetpos.m_x);
		writeInt16(targetpos.m_y);
		writeByte(targetpos.m_z);
	}
	else if (pt != nullptr)
	{
		// item is being dropped onto the floor
		writeInt32(source->GetUID());
		writeInt16(sourcepos.m_x);
		writeInt16(sourcepos.m_y);
		writeByte(sourcepos.m_z);
		writeInt32(0);
		writeInt16(pt->m_x);
		writeInt16(pt->m_y);
		writeByte(pt->m_z);
	}
	else
	{
		// item is being picked up from the ground
		const CObjBaseTemplate* target = item->GetTopLevelObj();
		const CPointMap& targetpos = target->GetTopPoint();

		writeInt32((target == item)? 0 : static_cast<dword>(target->GetUID()));
		writeInt16(targetpos.m_x);
		writeInt16(targetpos.m_y);
		writeByte(targetpos.m_z);
		writeInt32(0);
		writeInt16(sourcepos.m_x);
		writeInt16(sourcepos.m_y);
		writeByte(sourcepos.m_z);
	}
}

bool PacketDragAnimation::canSendTo(const CNetState* state) const
{
	// don't send to SA clients
	if (state->isClientEnhanced() || state->isClientVersionNumber(MINCLIVER_SA))
		return false;

	return PacketSend::canSendTo(state);
}

/***************************************************************************
 *
 *
 *	Packet 0x24 : PacketContainerOpen		open container gump (LOW)
 *
 *
 ***************************************************************************/
PacketContainerOpen::PacketContainerOpen(const CClient* target, const CObjBase* container, const GUMP_TYPE gump) : PacketSend(XCMD_ContOpen, 9, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL),
	m_container(container->GetUID())
{
	ADDTOCALLSTACK("PacketContainerOpen::PacketContainerOpen");

	writeInt32(m_container);
	writeInt16(static_cast<word>(gump));

	// HS clients needs an extra 'container type' byte (0x00 for vendors, 0x7D for spellbooks/containers)
	if (target->GetNetState()->isClientVersionNumber(MINCLIVER_HS) || target->GetNetState()->isClientKR() || target->GetNetState()->isClientEnhanced())
	{
        const word ContType = (gump == GUMP_VENDOR_RECT) ? 0x00 : 0x7D;
		writeInt16(ContType);
	}

	trim();
	push(target);
}

bool PacketContainerOpen::onSend(const CClient* client)
{
	ADDTOCALLSTACK("PacketContainerOpen::onSend");
	if (g_NetworkManager.isOutputThreaded())
		return true;

	return client->CanSee(m_container.ObjFind());
}


/***************************************************************************
 *
 *
 *	Packet 0x25 : PacketItemContainer		sends item in a container (NORMAL)
 *
 *
 ***************************************************************************/
PacketItemContainer::PacketItemContainer(const CClient* target, const CItem* item) : PacketSend(XCMD_ContAdd, 21, PRI_NORMAL), m_item(item->GetUID())
{
	ADDTOCALLSTACK("PacketItemContainer::PacketItemContainer");

    const CItemContainer* container = dynamic_cast<CItemContainer*>( item->GetParent() );
    if (container == nullptr)
    {
        return;
    }

	const CPointBase& pt = item->GetContainedPoint();

	const CItemBase* itemDefinition = item->Item_GetDef();
	ITEMID_TYPE id = item->GetDispID();
	HUE_TYPE hue = item->GetHueVisible() & HUE_MASK_HI;

	if (itemDefinition && target->GetResDisp() < itemDefinition->GetResLevel())
	{
		id = static_cast<ITEMID_TYPE>(itemDefinition->GetResDispDnId());
		if (itemDefinition->GetResDispDnHue() != HUE_DEFAULT)
			hue = itemDefinition->GetResDispDnHue() & HUE_MASK_HI;
	}

	if (hue > HUE_QTY)
		hue &= HUE_MASK_LO;

	writeInt32(item->GetUID());
	writeInt16(static_cast<word>(id));
	writeByte(0);
	writeInt16(item->GetAmount());
	writeInt16(pt.m_x);
	writeInt16(pt.m_y);

	if (target->GetNetState()->isClientVersionNumber(MINCLIVER_ITEMGRID) || target->GetNetState()->isClientKR() || target->GetNetState()->isClientEnhanced())
		writeByte(item->GetContainedGridIndex());

	writeInt32(container->GetUID());
	writeInt16(hue);

	trim();
	push(target);
}

PacketItemContainer::PacketItemContainer(const CItem* spellbook, const CSpellDef* spell) : PacketSend(XCMD_ContAdd, 21, PRI_NORMAL), m_item(spellbook->GetUID())
{
	ADDTOCALLSTACK("PacketItemContainer::PacketItemContainer(2)");

	writeInt32(UID_F_ITEM|UID_O_INDEX_FREE|spell->m_idSpell);
	writeInt16(static_cast<word>(spell->m_idScroll));
	writeByte(0);
	writeInt16(static_cast<word>(spell->m_idSpell));
	writeInt16(0x48);
	writeInt16(0x7D);
}

void PacketItemContainer::completeForTarget(const CClient* target, const CItem* spellbook)
{
	ADDTOCALLSTACK("PacketItemContainer::completeForTarget");

    const bool shouldIncludeGrid = (target->GetNetState()->isClientVersionNumber(MINCLIVER_ITEMGRID) || target->GetNetState()->isClientKR() || target->GetNetState()->isClientEnhanced());

	if (getLength() >= 20)
	{
		// only append the additional information if it needs to be changed
        if (const bool containsGrid = getLength() == 21; shouldIncludeGrid == containsGrid)
			return;
	}

	seek(14);

	if (shouldIncludeGrid)
		writeByte(0);

	writeInt32(spellbook->GetUID());
	writeInt16(HUE_DEFAULT);

	trim();
}

bool PacketItemContainer::onSend(const CClient* client)
{
	ADDTOCALLSTACK("PacketItemContainer::onSend");
	if (g_NetworkManager.isOutputThreaded())
		return true;

	return client->CanSee(m_item.ItemFind());
}


/***************************************************************************
 *
 *
 *	Packet 0x26 : PacketKick				notifies client they have been kicked (HIGHEST)
 *
 *
 ***************************************************************************/
PacketKick::PacketKick(const CClient* target) : PacketSend(XCMD_Kick, 5, PRI_HIGHEST)
{
	ADDTOCALLSTACK("PacketKick::PacketKick");

	writeInt32(0);
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x27 : PacketDragCancel			cancel item drag (HIGH)
 *
 *
 ***************************************************************************/
PacketDragCancel::PacketDragCancel(const CClient* target, const Reason code) : PacketSend(XCMD_DragCancel, 2, g_Cfg.m_fUsePacketPriorities? PRI_HIGH : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketDragCancel::PacketDragCancel");

	writeByte(static_cast<byte>(code));
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x29 : PacketDropAccepted		notify drop accepted (kr) (NORMAL)
 *
 *
 ***************************************************************************/
PacketDropAccepted::PacketDropAccepted(const CClient* target) : PacketSend(XCMD_DropAccepted, 1, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketDropAccepted::PacketDropAccepted");

	push(target);
}

bool PacketDropAccepted::CanSendTo(const CNetState* state) // static
{
    return state->isClientKR();
}


/***************************************************************************
 *
 *
 *	Packet 0x2C : PacketDeathMenu			display death menu/effect (NORMAL)
 *
 *
 ***************************************************************************/
PacketDeathMenu::PacketDeathMenu(const CClient* target, const Mode mode) : PacketSend(XCMD_DeathMenu, 2, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketDeathMenu::PacketDeathMenu");

	writeByte(static_cast<byte>(mode));
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x2E : PacketItemEquipped		sends equipped item  (NORMAL)
 *
 *
 ***************************************************************************/
PacketItemEquipped::PacketItemEquipped(const CClient* target, const CItem* item) : PacketSend(XCMD_ItemEquip, 15, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketItemEquipped::PacketItemEquipped");

	const CChar* parent = dynamic_cast<CChar*>(item->GetParent());
	ASSERT(parent);

    const LAYER_TYPE layer = item->GetEquipLayer();
	ITEMID_TYPE id;
	HUE_TYPE hue;
	target->GetAdjustedItemID(parent, item, id, hue);

	if (layer == LAYER_BANKBOX)
		id = ITEMID_CHEST_SILVER;
/*
	else if ((layer > 25) && (layer < 29))
	{
		id = ITEMID_BACKPACK;
		hue = 0;
	}
*/

	writeInt32(item->GetUID());
	writeInt16(static_cast<word>(id));
	writeByte(0);
	writeByte(layer);
	writeInt32(parent->GetUID());
	writeInt16(hue);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x2F : PacketSwing				fight swing (LOW)
 *
 *
 ***************************************************************************/
PacketSwing::PacketSwing(const CClient* target, const CChar* defender) : PacketSend(XCMD_Fight, 10, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketSwing::PacketSwing");

	writeByte(0);
	writeInt32(target->GetChar()->GetUID());
	writeInt32(defender->GetUID());
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x3A : PacketSkills				character skills (LOW)
 *
 *
 ***************************************************************************/
PacketSkills::PacketSkills(const CClient* target, const CChar* character, const SKILL_TYPE skill) : PacketSend(XCMD_Skill, 15, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketSkills::PacketSkills");

	initLength();

	if (character == nullptr)
		character = target->GetChar();

    const bool includeCaps = target->GetNetState()->isClientVersionNumber(MINCLIVER_SKILLCAPS);
	if (skill >= SKILL_QTY)
	{
		// all skills
		if (includeCaps)
			writeByte(0x02);
		else
			writeByte(0x00);

		for (uint i = 0; i < g_Cfg.m_iMaxSkill; ++i)
		{
			if (g_Cfg.m_SkillIndexDefs.valid_index(i) == false)
				continue;

			writeInt16(static_cast<word>(i + 1));
			writeInt16(character->Skill_GetAdjusted(static_cast<SKILL_TYPE>(i)));
			writeInt16(character->Skill_GetBase(static_cast<SKILL_TYPE>(i)));
			writeByte(static_cast<byte>(character->Skill_GetLock(static_cast<SKILL_TYPE>(i))));
			if (includeCaps)
				writeInt16(character->Skill_GetMax(static_cast<SKILL_TYPE>(i)));
		}

		writeInt16(0);
	}
	else
	{
		// one skill
		if (includeCaps)
			writeByte(0xDF);
		else
			writeByte(0xFF);

		writeInt16(static_cast<word>(skill));
		writeInt16(character->Skill_GetAdjusted(skill));
		writeInt16(character->Skill_GetBase(skill));
		writeByte(static_cast<byte>(character->Skill_GetLock(skill)));
		if (includeCaps)
			writeInt16(character->Skill_GetMax(skill));
	}

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x3B : PacketCloseVendor			close vendor menu (NORMAL)
 *
 *
 ***************************************************************************/
PacketCloseVendor::PacketCloseVendor(const CClient* target, const CChar* vendor) : PacketSend(XCMD_VendorBuy, 8, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketCloseVendor::PacketCloseVendor");

	initLength();
	writeInt32(vendor->GetUID());
	writeByte(0); // no items

	push(target);
}


/***************************************************************************
*
*
*	Packet 0x3C : PacketItemContents		contents of an item (NORMAL)
*
*
***************************************************************************/
PacketItemContents::PacketItemContents(CClient* target, const CItemContainer* container, const bool fIsShop, const bool fFilterLayers) : PacketSend(XCMD_Content, 5, PRI_NORMAL),
	m_container(container->GetUID()), m_count(0)
{
	ADDTOCALLSTACK("PacketItemContents::PacketItemContents");

	initLength();
	skip(2);

    const CNetState* ns = target->GetNetState();
    ASSERT(ns);
    const bool fClientEnhanced = ns->isClientEnhanced();
	const bool fIncludeGrid = (ns->isClientVersionNumber(MINCLIVER_ITEMGRID) || ns->isClientKR() || fClientEnhanced);
    bool fIsLayerSent[LAYER_HORSE] = {};

    const CChar* viewer = target->GetChar();
    if (viewer == nullptr)
    {
        return;
    }
	std::vector<CItem*> items;
	items.reserve(container->GetContentCount());

	// Classic Client wants the container items sent with order a->z, Enhanced Client with order z->a;
	// Classic client wants the prices sent (in PacketVendorBuyList::fillBuyData) with order a->z, Enhanced Client with order a->z.
	auto it = container->begin();
    const auto itEnd = container->end();
    auto itRev = container->rbegin();
    const auto itRevEnd = container->rend();
    for ( ; m_count < g_Cfg.m_iContainerMaxItems ; ++it, ++itRev )
	{
		CItem* item;
		if (fClientEnhanced)
		{
			if (itRev == itRevEnd)
				break;
			item = dynamic_cast<CItem *>(*itRev);
		}
		else
		{
			if (it == itEnd)
				break;
			item = dynamic_cast<CItem*>(*it);
		}
		ASSERT (item != nullptr);

		word wAmount = item->GetAmount();
		CPointMap pos = item->GetContainedPoint();

		if ( fIsShop )
		{
            if (const auto vendorItem = dynamic_cast<const CItemVendable *>(item);
                vendorItem == nullptr || vendorItem->GetAmount() == 0 || vendorItem->IsType(IT_GOLD) )
				continue;

			wAmount = minimum((word)g_Cfg.m_iVendorMaxSell, wAmount);
			pos.m_x = static_cast<short>(m_count + 1);
			pos.m_y = 1;
		}
		else
		{
			if ( item->IsAttr(ATTR_INVIS) && !viewer->CanSee(item) )
				continue;
		}

		const CItemBase* itemDefinition = item->Item_GetDef();
		ITEMID_TYPE id = item->GetDispID();
		HUE_TYPE hue = item->GetHueVisible() & HUE_MASK_HI;

		if ( fFilterLayers )
		{
            if (const auto layer = static_cast<LAYER_TYPE>(item->GetContainedLayer()); (layer > LAYER_NONE) && (layer < LAYER_HORSE))
			{
				switch (layer)	// don't put these on a corpse.
				{
				case LAYER_PACK: // these display strange.
					continue;

				default:
					// Make sure that no more than one of each layer goes out to client....crashes otherwise!!
					if (fIsLayerSent[layer])
						continue;

					fIsLayerSent[layer] = true;
					break;
				}
			}
		}

		if ( (itemDefinition != nullptr) && (target->GetResDisp() < itemDefinition->GetResLevel()) )
		{
			id = static_cast<ITEMID_TYPE>(itemDefinition->GetResDispDnId());

			if ( itemDefinition->GetResDispDnHue() != HUE_DEFAULT )
				hue = itemDefinition->GetResDispDnHue() & HUE_MASK_HI;
		}

		if ( hue > HUE_QTY )
			hue &= HUE_MASK_LO;		// restrict colors

		// write item data
		writeInt32(item->GetUID());
		writeInt16(static_cast<word>(id));
		writeByte(0);
		writeInt16(wAmount);
		writeInt16(pos.m_x);
		writeInt16(pos.m_y);
		if ( fIncludeGrid )
			writeByte(item->GetContainedGridIndex());
		writeInt32(container->GetUID());
		writeInt16(hue);

		items.push_back(item);

		if ( ++m_count >= g_Cfg.m_iContainerMaxItems )
			break;
	}

	// write item count
    const uint l = getPosition();
	seek(3);
	writeInt16(m_count);
	seek(l);

	if (m_count > 0)
	{
		// send tooltips
        for (CItem *pItem : items)
			target->addAOSTooltip(pItem, false, fIsShop);
	}

	push(target);
}

PacketItemContents::PacketItemContents(const CClient* target, const CItem* spellbook) : PacketSend(XCMD_Content, 5, PRI_NORMAL),
	m_container(spellbook->GetUID()), m_count(0)
{
	ADDTOCALLSTACK("PacketItemContents::PacketItemContents(2)");

    const CNetState* ns = target->GetNetState();
    ASSERT(ns);
    const bool fIncludeGrid = (ns->isClientVersionNumber(MINCLIVER_ITEMGRID) || ns->isClientKR() || ns->isClientEnhanced());

	initLength();
	skip(2);

	for (int i = SPELL_Clumsy; i <= SPELL_MAGERY_QTY; ++i)
	{
		if (spellbook->IsSpellInBook(static_cast<SPELL_TYPE>(i)) == false)
			continue;

		writeInt32(UID_F_ITEM + UID_O_INDEX_FREE + i);
		writeInt16(0x1F2E);
		writeByte(0);
		writeInt16(static_cast<word>(i));
		writeInt16(0);
		writeInt16(0);
		if (fIncludeGrid)
			writeByte(static_cast<byte>(m_count));
		writeInt32(spellbook->GetUID());
		writeInt16(HUE_DEFAULT);

		++m_count;
	}

	// write item count
    const uint l = getPosition();
	seek(3);
	writeInt16(m_count);
	seek(l);

	push(target);
}

PacketItemContents::PacketItemContents(const CClient* target, const CItemContainer* spellbook) : PacketSend(XCMD_Content, 5, PRI_NORMAL),
	m_container(spellbook->GetUID()), m_count(0)
{
	ADDTOCALLSTACK("PacketItemContents::PacketItemContents(3)");

    const CNetState* ns = target->GetNetState();
    ASSERT(ns);
    const bool fIncludeGrid = (ns->isClientVersionNumber(MINCLIVER_ITEMGRID) || ns->isClientKR() || ns->isClientEnhanced());

    initLength();
	skip(2);

	for (const CSObjContRec* pObjRec : *spellbook)
	{
        const auto item = static_cast<const CItem*>(pObjRec);
		if (item->IsType(IT_SCROLL) == false)
			continue;

		const CSpellDef *spellDefinition = g_Cfg.GetSpellDef(static_cast<SPELL_TYPE>(item->m_itSpell.m_spell));
		if (spellDefinition == nullptr)
			continue;

		writeInt32(item->GetUID());
		writeInt16(static_cast<word>(spellDefinition->m_idScroll));
		writeByte(0);
		writeInt16(item->m_itSpell.m_spell);
		writeInt16(0);
		writeInt16(0);
		if (fIncludeGrid)
			writeByte(static_cast<byte>(m_count));
		writeInt32(spellbook->GetUID());
		writeInt16(HUE_DEFAULT);

		++m_count;
	}

	// write item count
    const uint l = getPosition();
	seek(3);
	writeInt16(m_count);
	seek(l);

	push(target);
}

bool PacketItemContents::onSend(const CClient* client)
{
	ADDTOCALLSTACK("PacketItemContents::onSend");

	if (g_NetworkManager.isOutputThreaded())
		return true;

	return client->CanSee(m_container.ItemFind());
}

/***************************************************************************
 *
 *
 *	Packet 0x3F : PacketQueryClient			Query Client for block info (NORMAL)
 *
 *
 ***************************************************************************/
PacketQueryClient::PacketQueryClient(CClient* target, const byte bCmd) : PacketSend(XCMD_StaticUpdate, 15, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketQueryClient::PacketQueryClient");
	initLength();
	switch (bCmd)
	{
		case 0x01:
		{
			// Update Map Definitions Command
            const int length = 2 * 9; //map count * 9
            int count = length / 7;
            int padding = 0;
            if (length - (count * 7) > 0)
            {
                ++count;
                padding = (count * 7) - length;
            }

			writeInt32(0);
			writeInt32(4);
			writeInt16(0);
			writeByte(0x01);
			writeByte(0);

			for (uchar i = 0; i < 2; ++i)
			{
				writeByte(i);
				writeInt16(g_MapList.GetMapSizeX(i));
				writeInt16(g_MapList.GetMapSizeY(i));
				writeInt16(g_MapList.GetMapSizeX(i));
				writeInt16(g_MapList.GetMapSizeY(i));
            }

            for (int i = 0; i < padding; ++i)
                writeByte(0);

			break;
		}
		case 0x02:
		{
			//Login Complete Command
			writeInt32(1);
			writeInt32(4);
			writeInt16(0);
			writeByte(0x02);
			writeByte(0);
			writeStringFixedASCII(g_Serv.GetName(),28);
			break;
		}
		case 0x03:
		{
			//Refresh Client View Command
			writeInt32(0);
			writeInt32(0);
			writeInt16(0);
			writeByte(0x03);
			writeByte(0);
			break;
		}
		case 0xFF:
		{
			//Query Client Command
		    const CChar* pChar = target->GetChar();
		    if (pChar == nullptr)
		    {
		        return;
		    }
            const byte bMap = pChar->GetTopMap();
            const CPointMap pt = pChar->GetTopPoint();
            const dword dwBlockId = (pt.m_x * (g_MapList.GetMapSizeY( bMap ) / UO_BLOCK_SIZE)) + pt.m_y;
			writeInt32(dwBlockId);
			writeInt32(0);
			writeInt16(0);
			writeByte(0xFF);
			writeByte(bMap);
			break;
		}
	}

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x4F : PacketGlobalLight			sets global light level (NORMAL)
 *
 *
 ***************************************************************************/
PacketGlobalLight::PacketGlobalLight(const CClient* target, const byte light) : PacketSend(XCMD_Light, 2, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketGlobalLight::PacketGlobalLight");

	writeByte(light);
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x53 : PacketWarningMessage		show popup warning message (NORMAL)
 *
 *
 ***************************************************************************/
PacketWarningMessage::PacketWarningMessage(const CClient* target, const Message code) : PacketSend(XCMD_IdleWarning, 2, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketWarningMessage::PacketWarningMessage");

	writeByte(static_cast<byte>(code));
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x54 : PacketPlaySound			play a sound (NORMAL)
 *
 *
 ***************************************************************************/
PacketPlaySound::PacketPlaySound(const CClient* target, const SOUND_TYPE sound, const int flags, const int volume, const CPointMap& pos) : PacketSend(XCMD_Sound, 12, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketPlaySound::PacketPlaySound");

	writeByte(static_cast<byte>(flags));
	writeInt16(sound);
	writeInt16(static_cast<word>(volume));
	writeInt16(pos.m_x);
	writeInt16(pos.m_y);
	writeInt16(pos.m_z);
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x55 : PacketLoginComplete		redraw all (NORMAL)
 *
 *
 ***************************************************************************/
PacketLoginComplete::PacketLoginComplete(const CClient* target) : PacketSend(XCMD_LoginComplete, 1, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketLoginComplete::PacketLoginComplete");

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x56 : PacketMapPlot				show/edit map plots (LOW)
 *
 *
 ***************************************************************************/
PacketMapPlot::PacketMapPlot(const CClient* target, const CItem* map, const MAPCMD_TYPE mode, const bool edit) : PacketSend(XCMD_MapEdit, 11, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketMapPlot::PacketMapPlot");

	writeInt32(map->GetUID());
	writeByte(static_cast<byte>(mode));
	writeBool(edit);
	writeInt16(0);
	writeInt16(0);

	push(target);
}

PacketMapPlot::PacketMapPlot(const CItem* map, const MAPCMD_TYPE mode, const bool edit) : PacketSend(XCMD_MapEdit, 11, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketMapPlot::PacketMapPlot");

	writeInt32(map->GetUID());
	writeByte(static_cast<byte>(mode));
	writeBool(edit);
}

void PacketMapPlot::setPin(const short x, const short y)
{
	ADDTOCALLSTACK("PacketMapPlot::setPin");

	seek(7);
	writeInt16(static_cast<word>(x));
	writeInt16(static_cast<word>(y));
}


/***************************************************************************
 *
 *
 *	Packet 0x5B : PacketGameTime			current game time (IDLE)
 *
 *
 ***************************************************************************/
PacketGameTime::PacketGameTime(const CClient* target, const int hours, const int minutes, const int seconds) : PacketSend(XCMD_Time, 4, g_Cfg.m_fUsePacketPriorities? PRI_IDLE : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketGameTime::PacketGameTime");

	writeByte(static_cast<byte>(hours));
	writeByte(static_cast<byte>(minutes));
	writeByte(static_cast<byte>(seconds));
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x65 : PacketWeather				set current weather (IDLE)
 *
 *
 ***************************************************************************/
PacketWeather::PacketWeather(const CClient* target, const WEATHER_TYPE weather, const int severity, const int temperature) : PacketSend(XCMD_Weather, 4, g_Cfg.m_fUsePacketPriorities? PRI_IDLE : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketWeather::PacketWeather");

	writeByte(weather);
	writeByte(static_cast<byte>(severity));
	writeByte(static_cast<byte>(temperature));
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x66 : PacketBookPageContent		send book page content (LOW)
 *
 *
 ***************************************************************************/
PacketBookPageContent::PacketBookPageContent(const CClient* target, const CItem* book, const word startpage, const word pagecount) : PacketSend(XCMD_BookPage, 8, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketBookPageContent::PacketBookPageContent");

	m_pages = 0;

	initLength();
	writeInt32(book->GetUID());
	writeInt16(0);

	for (word i = 0; i < pagecount; ++i)
		addPage(book, startpage + i);

	push(target);
}

void PacketBookPageContent::addPage(const CItem* book, const word page)
{
	ADDTOCALLSTACK("PacketBookPageContent::addPage");

	writeInt16(page);

	// skip line count for now
    const uint linesPos = getPosition();
	uint lines = 0;
	writeInt16(0);

	if (book->IsBookSystem())
	{
        if (CResourceLock s; g_Cfg.ResourceLock(s, CResourceID(RES_BOOK, book->m_itBook.m_ResID.GetResIndex(), page)) == true)
		{
			while (s.ReadKey(false))
			{
				lines++;
				writeStringASCII(s.GetKey());
			}
		}
	}
	else
	{
		// user written book pages
        if (const auto message = dynamic_cast<const CItemMessage *>(book); message != nullptr)
		{
			if (page > 0 && page <= message->GetPageCount())
			{
				// copy the pages from the book
                if (lpctstr text = message->GetPageText(page - 1); text != nullptr)
				{
					for (tchar ch = *text; ch != '\0'; ch = *(++text))
					{
						if (ch == '\t')
						{
							ch = '\0';
							++lines;
						}

						writeCharASCII(ch);
					}

					writeCharASCII('\0');
					++lines;
				}
			}
		}
	}

    const uint endPos = getPosition();

	// seek back to write line count
	seek(linesPos);
	writeInt16(static_cast<word>(lines));

	// seek further back to increment page count
	seek(7);
	writeInt16(static_cast<word>(++m_pages));

	// return to end
	seek(endPos);
}


/***************************************************************************
 *
 *
 *	Packet 0x6C : PacketAddTarget				adds target cursor to client (LOW)
 *	Packet 0x99 : PacketAddTarget				adds target cursor to client with multi (LOW)
 *
 *
 ***************************************************************************/
PacketAddTarget::PacketAddTarget(const CClient* target, const TargetType type, const dword context, const Flags flags) : PacketSend(XCMD_Target, 19, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketAddTarget::PacketAddTarget");

	writeByte(static_cast<byte>(type));
	writeInt32(context);
	writeByte(static_cast<byte>(flags));

	// unused data
	writeInt32(0);
	writeInt16(0);
	writeInt16(0);
	writeByte(0);
	writeByte(0);
	writeInt16(0);

	push(target);
}

PacketAddTarget::PacketAddTarget(const CClient* target, const TargetType type, const dword context, const Flags flags, const ITEMID_TYPE id, const HUE_TYPE color) : PacketSend(XCMD_TargetMulti, 30, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketAddTarget::PacketAddTarget(2)");

	//CItemBase *pItemDef = CItemBase::FindItemBase((ITEMID_TYPE)(ResGetIndex(id)));
	CItemBase *pItemDef = CItemBase::FindItemBase(id);
	if ( !pItemDef )
		return;

	word x = 0, y = 0, z = 0;
    if (const auto pMultiDef = static_cast<CItemBaseMulti *>(pItemDef); pMultiDef && CItemBase::IsID_Multi(id))
	{
		x = static_cast<word>(pMultiDef->m_Offset.m_dx != 0 ? (pMultiDef->m_rect.m_left + pMultiDef->m_Offset.m_dx) : 0);
		y = static_cast<word>(pMultiDef->m_rect.m_bottom + pMultiDef->m_Offset.m_dy);
		z = static_cast<word>(pMultiDef->m_Offset.m_dz);
	}

	writeByte(static_cast<byte>(type));
	writeInt32(context);
	writeByte(static_cast<byte>(flags));

	writeInt32(0);
	writeInt32(0);
	writeInt16(0);
	writeByte(0);

	writeInt16(static_cast<word>(id - ITEMID_MULTI));

	writeInt16(x);	// x
	writeInt16(y);	// y
	writeInt16(z);	// z

	if ( target->GetNetState()->isClientVersionNumber(MINCLIVER_HS) )
		writeInt32(color);	// hue

	trim();
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x6D : PacketPlayMusic			adds music to the client (IDLE)
 *
 *
 ***************************************************************************/
PacketPlayMusic::PacketPlayMusic(const CClient* target, const word musicID) : PacketSend(XCMD_PlayMusic, 3, g_Cfg.m_fUsePacketPriorities? PRI_IDLE : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketPlayMusic::PacketPlayMusic");

	writeInt16(musicID);
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x6E : PacketAction				plays an animation (LOW)
 *  Packet 0xE2 : PacketActionBasic			plays an animation (client > 7.0.0.0) (LOW)
 *
 ***************************************************************************/
PacketAction::PacketAction(const CChar* character, const ANIM_TYPE action, const word repeat, const bool backward, const byte delay, const byte len) : PacketSend(XCMD_CharAction, 14, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketAction::PacketAction");

	writeInt32(character->GetUID());
	writeInt16(static_cast<word>(action));
	writeInt16(len);
	writeInt16(repeat);
	writeBool(backward);
	writeBool(repeat != 1);
	writeByte(delay);
}

PacketActionBasic::PacketActionBasic(const CChar* character, const ANIM_TYPE_NEW action, const ANIM_TYPE_NEW subaction, const byte variation) : PacketSend(XCMD_NewAnimUpdate, 10, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketActionBasic::PacketActionBasic");

	writeInt32(character->GetUID());
	writeInt16(static_cast<word>(action));
	writeInt16(static_cast<word>(subaction));
	writeByte(variation);
}

PacketAction::~PacketAction() = default;
PacketActionBasic::~PacketActionBasic() = default;

/***************************************************************************
 *
 *
 *	Packet 0x6F : PacketTradeAction			perform a trade action (NORMAL)
 *
 *
 ***************************************************************************/
PacketTradeAction::PacketTradeAction(const SECURE_TRADE_TYPE action) : PacketSend(XCMD_SecureTrade, 17, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketTradeAction::PacketTradeAction");

	initLength();
	writeByte(static_cast<byte>(action));
}

PacketTradeAction::~PacketTradeAction() = default;

void PacketTradeAction::prepareContainerOpen(const CChar *character, const CItem *container1, const CItem *container2)
{
	ADDTOCALLSTACK("PacketTradeAction::prepareContainerOpen");

	seek(4);
	writeInt32(character->GetUID());
	writeInt32(container1->GetUID());
	writeInt32(container2->GetUID());
	writeBool(true);
	writeStringFixedASCII(character->GetName(), 30);
}

void PacketTradeAction::prepareReadyChange(const CItemContainer *container1, const CItemContainer *container2)
{
	ADDTOCALLSTACK("PacketTradeAction::prepareReadyChange");

	seek(4);
	writeInt32(container1->GetUID());
	writeInt32(container1->m_itEqTradeWindow.m_bCheck);
	writeInt32(container2->m_itEqTradeWindow.m_bCheck);
	writeBool(false);
}

void PacketTradeAction::prepareClose(const CItemContainer *container)
{
	ADDTOCALLSTACK("PacketTradeAction::prepareClose");

	seek(4);
	writeInt32(container->GetUID());
	writeInt32(0);
	writeInt32(0);
	writeBool(false);
}

void PacketTradeAction::prepareUpdateGold(const CItemContainer *container, const dword gold, const dword platinum)
{
	ADDTOCALLSTACK("PacketTradeAction::prepareUpdateGold");

	seek(4);
	writeInt32(container->GetUID());
	writeInt32(gold);
	writeInt32(platinum);
	writeBool(false);
}

void PacketTradeAction::prepareUpdateLedger(const CItemContainer *container, const dword gold, const dword platinum)
{
	ADDTOCALLSTACK("PacketTradeAction::prepareUpdateLedger");

	seek(4);
	writeInt32(container->GetUID());
	writeInt32(gold);
	writeInt32(platinum);
	writeBool(false);
}


/***************************************************************************
 *
 *
 *	Packet 0x70 : PacketEffect				displays a visual effect (NORMAL)
 *	Packet 0xC0 : PacketEffect				displays a hued visual effect (NORMAL)
 *  Packet 0xc7 : PacketEffect              displays a particle effect in Enhanced Client (NORMAL)
 *
 *
 ***************************************************************************/
// Non hued effect
PacketEffect::PacketEffect(const CClient* target, const EFFECT_TYPE motion, const ITEMID_TYPE id, const CObjBaseTemplate* dst, const CObjBaseTemplate* src, const byte speed, const byte loop, const bool explode)
    : PacketSend(XCMD_Effect, 20, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketEffect::PacketEffect");

	writeBasicEffect(motion, id, dst, src, speed, loop, explode);

	push(target);
}

// Hued effect
PacketEffect::PacketEffect(const CClient* target, const EFFECT_TYPE motion, const ITEMID_TYPE id, const CObjBaseTemplate* dst, const CObjBaseTemplate* src, const byte speed, const byte loop, const bool explode, const dword hue, const dword render)
    : PacketSend(XCMD_EffectEx, 28, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketEffect::PacketEffect(hued)");

	writeBasicEffect(motion, id, dst, src, speed, loop, explode);
	writeHuedEffect(hue, render);

	push(target);
}

// Particle effect
PacketEffect::PacketEffect(const CClient* target, const EFFECT_TYPE motion, const ITEMID_TYPE id, const CObjBaseTemplate* dst, const CObjBaseTemplate* src, const byte speed, const byte loop, const bool explode, const dword hue, const dword render, const word effectid, const dword explodeid, const word explodesound, const dword effectuid, const byte type)
    : PacketSend(XCMD_EffectParticle, 49, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketEffect::PacketEffect(particle)");

	writeBasicEffect(motion, id, dst, src, speed, loop, explode);
	writeHuedEffect(hue, render);

	writeInt16(effectid);
	writeInt16(static_cast<word>(explodeid));
	writeInt16(explodesound);
	writeInt32(effectuid);
	writeByte(type == 0 ? 0xFF : 0x03 );	// (0xFF or 0x03)
	writeInt16(0x0);
	push(target);
}

void PacketEffect::writeBasicEffect(const EFFECT_TYPE motion, const ITEMID_TYPE id, const CObjBaseTemplate* dst, const CObjBaseTemplate* src, const byte speed, byte loop, const bool explode)
{
	ADDTOCALLSTACK("PacketEffect::writeBasicEffect");

	bool oneDirection = true;
    dst = dst->GetTopLevelObj();
    if (dst == nullptr)
    {
        return;
    }
    const CPointMap dstpos = dst->GetTopPoint();

	CPointMap srcpos;
	if (src != nullptr && motion == EFFECT_BOLT)
	{
		src = src->GetTopLevelObj();
		srcpos = src->GetTopPoint();
	}
	else
		srcpos = dstpos;


	writeByte(static_cast<byte>(motion));

	switch (motion)
	{
		case EFFECT_BOLT: // a targeted bolt
			if (src == nullptr)
				src = dst;

			oneDirection = false;
			loop = 0; // does not apply.

			writeInt32(src->GetUID()); // source
			writeInt32(dst->GetUID());
			break;

		case EFFECT_LIGHTNING: // lightning bolt.
		case EFFECT_XYZ: // stay at current xyz
		case EFFECT_OBJ: // effect at single object.
			writeInt32(dst->GetUID());
			writeInt32(0);
			break;

		default: // unknown (should never happen)
			writeInt32(0);
			writeInt32(0);
			break;
	}

	writeInt16(static_cast<word>(id));
	writeInt16(srcpos.m_x);
	writeInt16(srcpos.m_y);
	writeByte(srcpos.m_z);
	writeInt16(dstpos.m_x);
	writeInt16(dstpos.m_y);
	writeByte(dstpos.m_z);
	writeByte(speed); // 0=very fast, 7=slow
	writeByte(loop); // 0=really long, 1=shortest, 6=longer
	writeInt16(0);
	writeByte(oneDirection);
	writeByte(explode);

}

void PacketEffect::writeHuedEffect(const dword hue, const dword render)
{
    ADDTOCALLSTACK("PacketEffect::writeHuedEffect");

    writeInt32((hue != 0) ? hue - 1 : hue);    // Idx 0: default hue. If idx > 1, then the value picked up in hues.mul has idx - 1.
    writeInt32(render);
}

// Non hued effect to coordinates (not to object)
PacketEffect::PacketEffect(const CClient* target, const EFFECT_TYPE motion, const ITEMID_TYPE id, const CPointMap *ptDest, const CPointMap *ptSrc, const byte speed, const byte loop, const bool explode) : PacketSend(XCMD_Effect, 20, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketEffect::PacketEffect(XYZ)");
    writeBasicEffectLocation(motion, id, ptDest, ptSrc, speed, loop, explode);
	push(target);
}

// Hued effect to coordinates (not to object)
PacketEffect::PacketEffect(const CClient* target, const EFFECT_TYPE motion, const ITEMID_TYPE id, const CPointMap *ptDest, const CPointMap *ptSrc, const byte speed, const byte loop, const bool explode, const dword hue, const dword render) : PacketSend(XCMD_EffectEx, 28, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketEffect::PacketEffect(XYZ)(hued)");

    writeBasicEffectLocation(motion, id, ptDest, ptSrc, speed, loop, explode);
	writeHuedEffect(hue, render);

	push(target);
}

// Particle effect to coordinates (not to object)
PacketEffect::PacketEffect(const CClient* target, const EFFECT_TYPE motion, const ITEMID_TYPE id, const CPointMap *ptDest, const CPointMap *ptSrc, const byte speed, const byte loop, const bool explode, const dword hue, const dword render, const word effectid, const dword explodeid, const word explodesound, const dword effectuid, const byte type) : PacketSend(XCMD_EffectParticle, 49, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketEffect::PacketEffect(XYZ)(particle)");

    writeBasicEffectLocation(motion, id, ptDest, ptSrc, speed, loop, explode);
	writeHuedEffect(hue, render);

	writeInt16(effectid);
	writeInt16(static_cast<word>(explodeid));
	writeInt16(explodesound);
	writeInt32(effectuid);
	writeByte(type == 0 ? 0xFF : 0x03);	// (0xFF or 0x03)
	writeInt16(0x0);
	push(target);
}

void PacketEffect::writeBasicEffectLocation(
    const EFFECT_TYPE motion, const ITEMID_TYPE id, const CPointMap *ptSrc, const CPointMap *ptDest, const byte speed, byte loop, const bool explode)
{
	ADDTOCALLSTACK("PacketEffect::writeBasicEffectLocation");

	bool oneDirection = true;
    const CPointMap* ptSrcToUse = (motion != EFFECT_BOLT) ? ptDest : ptSrc;
    if (motion == EFFECT_BOLT)
    {
        oneDirection = false;
        loop = 0; // does not apply.
    }

	writeByte(static_cast<byte>(motion));

    writeInt32(0);  // src uid
    writeInt32(0);  // dest uid

	writeInt16(static_cast<word>(id));
    if (ptSrcToUse)
    {
        writeInt16(ptSrcToUse->m_x);
        writeInt16(ptSrcToUse->m_y);
        writeByte(ptSrcToUse->m_z);
    }
    else
    {
        writeInt32(0);
        writeByte(0);
    }
    if (ptDest)
    {
        writeInt16(ptDest->m_x);
        writeInt16(ptDest->m_y);
        writeByte(ptDest->m_z);
    }
    else
    {
        writeInt32(0);
        writeByte(0);
    }
	writeByte(speed); // 0=very fast, 7=slow
	writeByte(loop); // 0=really long, 1=shortest, 6=longer
	writeInt16(0);
	writeByte(oneDirection);
	writeByte(explode);

}


/***************************************************************************
 *
 *
 *	Packet 0x71 : PacketBulletinBoard		display a bulletin board or message (NORMAL/LOW)
 *
 *
 ***************************************************************************/
PacketBulletinBoard::PacketBulletinBoard(const CClient* target, const CItemContainer* board) : PacketSend(XCMD_BBoard, 20, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketBulletinBoard::PacketBulletinBoard");

	initLength();
	writeByte(BBOARDF_NAME);
	writeInt32(board->GetUID());
	writeStringASCII(board->GetName());

	push(target);
}

PacketBulletinBoard::PacketBulletinBoard(const CClient* target, const BBOARDF_TYPE action, const CItemContainer* board, const CItemMessage* message) : PacketSend(XCMD_BBoard, 20, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketBulletinBoard::PacketBulletinBoard(2)");

	initLength();
	writeByte(static_cast<byte>(action == BBOARDF_REQ_FULL ? BBOARDF_MSG_BODY : BBOARDF_MSG_HEAD));
	writeInt32(board->GetUID());

	writeInt32(message->GetUID());
	if (action == BBOARDF_REQ_HEAD)
		writeInt32(0);

	size_t lenstr = 0;
	tchar* tempstr = Str_GetTemp();

	// author name. if it has one.
	if (message->m_sAuthor.IsEmpty())
	{
		writeByte(0x01);
		writeCharASCII('\0');
	}
	else
	{
        const lpctstr author = message->m_sAuthor;

		lenstr = strlen(author) + 1;
		if (lenstr > 255)
			lenstr = 255;

		writeByte(static_cast<byte>(lenstr));
		writeStringFixedASCII(author, static_cast<uint>(lenstr));
	}

	// message title
	lenstr = strlen(message->GetName()) + 1;
	if (lenstr > 255)
		lenstr = 255;

	writeByte(static_cast<byte>(lenstr));
	writeStringFixedASCII(message->GetName(), static_cast<uint>(lenstr));

	// message time
    const CSTime datetime(message->GetTimeStampS());
	snprintf(tempstr, Str_TempLength(), "%s", datetime.Format("%b %d, %Y"));
	lenstr = strlen(tempstr) + 1;

	writeByte(static_cast<byte>(lenstr));
	writeStringFixedASCII(tempstr, static_cast<uint>(lenstr));

	if (action == BBOARDF_REQ_FULL)
	{
		// requesst for full message body
		writeInt32(0);

        const ushort lines = message->GetPageCount();
		writeInt16(lines);

		for (ushort i = 0; i < lines; ++i)
		{
            const lpctstr text = message->GetPageText(i);
			if (text == nullptr)
				continue;

			lenstr = strlen(text) + 2;
			if (lenstr > 255)
				lenstr = 255;

			writeByte(static_cast<byte>(lenstr));
			writeStringFixedASCII(text, static_cast<uint>(lenstr));
		}
	}

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x72 : PacketWarMode				update war mode status (LOW)
 *
 *
 ***************************************************************************/
PacketWarMode::PacketWarMode(const CClient* target, const CChar* character) : PacketSend(XCMD_War, 5, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketWarMode::PacketWarMode");

	writeBool(character->IsStatFlag(STATF_WAR));
	writeByte(0x00);
	writeByte(0x32);
	writeByte(0x00);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x73 : PacketPingAck				ping reply (IDLE)
 *
 *
 ***************************************************************************/
PacketPingAck::PacketPingAck(const CClient* target, const byte value) : PacketSend(XCMD_Ping, 2, g_Cfg.m_fUsePacketPriorities? PRI_IDLE : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketPingAck::PacketPingAck");

	writeByte(value);
	push(target);
}


/***************************************************************************
*
*
*	Packet 0x74 : PacketVendorBuyList		show list of vendor items (LOW)
*
*
***************************************************************************/
PacketVendorBuyList::PacketVendorBuyList() : PacketSend(XCMD_VendOpenBuy, 8, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
}

uint PacketVendorBuyList::fillBuyData(const CItemContainer* container, const int iConvertFactor)
{
	ADDTOCALLSTACK("PacketVendorBuyList::fillBuyData");

	seek(1); // just to be sure
	initLength();

	writeInt32(container->GetUID());

	uint count = 0;
    const uint countpos = getPosition();
	skip(1);

	// Classic Client wants the container items sent (in PacketItemContents) with order a->z, Enhanced Client with order z->a;
	// Classic Client wants the prices sent with order a->z, Enhanced Client with order a->z.
	for (CSObjContRec* pObjRec : *container)
	{
        const auto pItem = static_cast<CItem*>(pObjRec);
        const auto vendorItem = static_cast<CItemVendable *>(pItem);
		if (vendorItem == nullptr || vendorItem->GetAmount() == 0)
			continue;

		dword price = vendorItem->GetVendorPrice(iConvertFactor, false);
		if (price == 0)
		{
			vendorItem->Item_GetDef()->ResetMakeValue();
			price = vendorItem->GetVendorPrice(iConvertFactor, false);

			if (price == 0 && vendorItem->IsValidNPCSaleItem())
				price = vendorItem->GetBasePrice();

			if (price == 0)
				price = 100000;
		}

		tchar* name;
/*
		CVarDefCont	* pVar = item->GetDefKey("NAMELOC", true);
		if (pVar)
		{
			name = Str_GetTemp();
			sprintf(name, "%" PRId64, pVar->GetValNum());
		}
		else
*/
			name = const_cast<tchar*>(vendorItem->GetName());
		uint len = static_cast<uint>(strlen(name)) + 1;
		if (len > UCHAR_MAX)
			len = UCHAR_MAX;

		writeInt32(price);
		writeByte(static_cast<byte>(len));
		writeStringFixedASCII(name, len);

		if ( ++count > g_Cfg.m_iContainerMaxItems )
			break;
	}

	// seek back to write count
    const uint endpos = getPosition();
	seek(countpos);
	writeByte(static_cast<byte>(count));
	seek(endpos);

	return count;
}


/***************************************************************************
 *
 *
 *	Packet 0x76 : PacketZoneChange			change server zone (LOW)
 *
 *
 ***************************************************************************/
PacketZoneChange::PacketZoneChange(const CClient* target, const CPointMap& pos) : PacketSend(XCMD_ZoneChange, 16, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketZoneChange::PacketZoneChange");

	writeInt16(pos.m_x);
	writeInt16(pos.m_y);
	writeInt16(pos.m_z);
	writeByte(0);
	writeInt16(0);
	writeInt16(0);
	writeInt16(g_MapList.GetMapSizeX(pos.m_map));
	writeInt16(g_MapList.GetMapSizeY(pos.m_map));

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x77 : PacketCharacterMove		move a character (NORMAL)
 *
 *
 ***************************************************************************/
PacketCharacterMove::PacketCharacterMove(const CClient* target, const CChar* character, const byte direction) : PacketSend(XCMD_CharMove, 17, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketCharacterMove::PacketCharacterMove");
	// NOTE: This packet move characters on screen, but can't move the
	// client char receiving the packet (use packet 0x20 instead).

	CREID_TYPE id;
	HUE_TYPE hue;
	target->GetAdjustedCharID(character, id, hue);
	const CPointMap& pos = character->GetTopPoint();

	writeInt32(character->GetUID());
	writeInt16(static_cast<word>(id));
	writeInt16(pos.m_x);
	writeInt16(pos.m_y);
	writeByte(pos.m_z);
	writeByte(direction);
	writeInt16(hue);
	writeByte(character->GetModeFlag(target));
	writeByte(character->Noto_GetFlag(target->GetChar(), true, target->GetNetState()->isClientVersionNumber(MINCLIVER_NOTOINVUL), true));

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x78 : PacketCharacter			create a character (NORMAL)
 *
 *
 ***************************************************************************/
PacketCharacter::PacketCharacter(CClient* target, const CChar* character) : PacketSend(XCMD_Char, 23, PRI_NORMAL), m_character(character->GetUID())
{
	ADDTOCALLSTACK("PacketCharacter::PacketCharacter");

	const CChar* viewer = target->GetChar();
	ASSERT(viewer);

	CREID_TYPE id;
	HUE_TYPE hue;
	target->GetAdjustedCharID(character, id, hue);
	const CPointMap &pos = character->GetTopPoint();
    const CNetState *ns = target->GetNetState();

	initLength();
	writeInt32(character->GetUID());
	writeInt16(static_cast<word>(id));
	writeInt16(pos.m_x);
	writeInt16(pos.m_y);
	writeByte(pos.m_z);
	writeByte(character->GetDirFlag());
	writeInt16(hue);
	writeByte(character->GetModeFlag(target));
	writeByte(character->Noto_GetFlag(target->GetChar(), true, ns->isClientVersionNumber(MINCLIVER_NOTOINVUL), true));

    const bool isNewMobilePacket = ns->isClientVersionNumber(MINCLIVER_NEWMOBINCOMING);

	if (character->IsStatFlag(STATF_SLEEPING) == false)
	{
        bool isLayerSent[LAYER_HORSE + 1] = {false};

		for (CSObjContRec* pObjRec : *character)
		{
            const auto item = static_cast<CItem*>(pObjRec);
            const LAYER_TYPE layer = item->GetEquipLayer();
			if (CItemBase::IsVisibleLayer(layer) == false)	// sanity check for layer value, ensure we don't get out of bounds in isLayerSent array
				continue;
			if (viewer->CanSeeItem(item) == false && viewer != character)
				continue;

			// prevent same layer being sent twice
			if (isLayerSent[layer])
				continue;

			isLayerSent[layer] = true;

			target->addAOSTooltip(item);

			ITEMID_TYPE itemid;
			target->GetAdjustedItemID(character, item, itemid, hue);

			writeInt32(item->GetUID());

			if (isNewMobilePacket)
			{
				writeInt16(static_cast<word>(itemid));
				writeByte(layer);
				writeInt16(hue);
			}
			else if (hue != 0)
			{
				writeInt16(static_cast<word>(itemid | 0x8000));
				writeByte(layer);
				writeInt16(hue);
			}
			else
			{
				writeInt16(static_cast<word>(itemid));
				writeByte(layer);
			}
		}
	}

	writeInt32(0);

	push(target);
}

bool PacketCharacter::onSend(const CClient* client)
{
	ADDTOCALLSTACK("PacketCharacter::onSend");
	if (g_NetworkManager.isOutputThreaded())
		return true;

	return client->CanSee(m_character.CharFind());
}


/***************************************************************************
 *
 *
 *	Packet 0x7C : PacketDisplayMenu			show a menu selection (LOW)
 *
 *
 ***************************************************************************/
PacketDisplayMenu::PacketDisplayMenu(const CClient* target, const CLIMODE_TYPE mode, const CMenuItem* items, const uint count, const CObjBase* object) : PacketSend(XCMD_MenuItems, 11, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketDisplayMenu::PacketDisplayMenu");

	initLength();
	writeInt32(object->GetUID());
	writeInt16(static_cast<word>(mode));

	uint len = items[0].m_sText.GetLength();
	if (len > 255)
		len = 255;
	writeByte(static_cast<byte>(len));
	writeStringFixedASCII(items[0].m_sText, len);

	writeByte(static_cast<byte>(count));
	for (uint i = 1; i <= count; ++i)
	{
		writeInt16(items[i].m_id);
		writeInt16(items[i].m_color);

		len = items[i].m_sText.GetLength();
		if (len > 255)
			len = 255;
		writeByte(static_cast<byte>(len));
		writeStringFixedASCII(items[i].m_sText, len);
	}

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x81 : PacketChangeCharacter		allow client to change character (LOW)
 *
 *
 ***************************************************************************/
PacketChangeCharacter::PacketChangeCharacter(CClient* target) : PacketSend(XCMD_CharList3, 5, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketChangeCharacter::PacketChangeCharacter");

	initLength();

    const uint countPos = getPosition();
	skip(1);

	writeByte(0);
    const uint count = target->Setup_FillCharList(this, target->GetChar());

	seek(countPos);
	writeByte(static_cast<byte>(count));
	skip((count * 60) + 1);

	push(target);
}

bool PacketChangeCharacter::CanSendTo(const CNetState* state) // static
{
    return !(state->isClientKR() || state->isClientEnhanced());
}


/***************************************************************************
 *
 *
 *	Packet 0x82 : PacketLoginError			login error response (HIGHEST)
 *
 *
 ***************************************************************************/
PacketLoginError::PacketLoginError(const CClient* target, const Reason reason) : PacketSend(XCMD_LogBad, 2, PRI_HIGHEST)
{
	ADDTOCALLSTACK("PacketLoginError::PacketLoginError");

	writeByte(static_cast<byte>(reason));
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x85 : PacketDeleteError			delete character error response (LOW)
 *
 *
 ***************************************************************************/
PacketDeleteError::PacketDeleteError(const CClient* target, const Reason reason) : PacketSend(XCMD_DeleteBad, 2, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketDeleteError::PacketDeleteError");

	writeByte(static_cast<byte>(reason));
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x86 : PacketCharacterListUpdate	update character list (LOW)
 *
 *
 ***************************************************************************/
PacketCharacterListUpdate::PacketCharacterListUpdate(CClient* target, const CChar* lastCharacter) : PacketSend(XCMD_CharList2, 4, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketCharacterListUpdate::PacketCharacterListUpdate");

	initLength();

    const uint countPos = getPosition();
	skip(1);

    const uint count = target->Setup_FillCharList(this, lastCharacter);

	seek(countPos);
	writeByte(static_cast<byte>(count));
	skip(count * 60);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x88 : PacketPaperdoll			show paperdoll (LOW)
 *
 *
 ***************************************************************************/
PacketPaperdoll::PacketPaperdoll(const CClient* target, const CChar* character) : PacketSend(XCMD_PaperDoll, 66, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketPaperdoll::PacketPaperdoll");

	uint mode = 0;
	if (character->IsStatFlag(STATF_WAR))
		mode |= (target->GetNetState()->isClientVersionNumber(MINCLIVER_ML)) ? 0x1 : 0x40;
	if (target->GetNetState()->isClientVersionNumber(MINCLIVER_ML))
	{
        if (character == target->GetChar() || target->GetChar()->CanDress(character))
            mode |= 0x2;
	}

	writeInt32(character->GetUID());

	if (character->IsStatFlag(STATF_INCOGNITO))
	{
		writeStringFixedASCII(character->GetName(), 60);
	}
	else
	{
		tchar* text = Str_GetTemp();
		int len = 0;

        if (const CStoneMember *guildMember = character->Guild_FindMember(MEMORY_GUILD);
            guildMember != nullptr && guildMember->IsAbbrevOn() && guildMember->GetParentStone()->GetAbbrev()[0])
		{
			len = snprintf(text, Str_TempLength(), "%s [%s], %s",
				character->Noto_GetTitle(), guildMember->GetParentStone()->GetAbbrev(),
							( guildMember->GetTitle()[0] ? guildMember->GetTitle() : (IsSetOF(OF_NoPaperdollTradeTitle) ? "" : character->GetTradeTitle()) ) );
		}

		if (len <= 0)
		{
			const char *title = nullptr;
			if (!IsSetOF(OF_NoPaperdollTradeTitle))
				title = character->GetTradeTitle();
			if ( title && title[0] )
				snprintf(text, Str_TempLength(), "%s, %s", character->Noto_GetTitle(), title);
			else
				snprintf(text, Str_TempLength(), "%s", character->Noto_GetTitle());
		}

		writeStringFixedASCII(text, 60);
	}

	writeByte(static_cast<byte>(mode));
	push(target);
}


/***************************************************************************
*
*
*	Packet 0x89 : PacketCorpseEquipment		send corpse equipment (NORMAL)
*
*
***************************************************************************/
PacketCorpseEquipment::PacketCorpseEquipment(CClient* target, const CItemContainer* corpse) : PacketSend(XCMD_CorpEquip, 7, PRI_NORMAL), m_corpse(corpse->GetUID())
{
	ADDTOCALLSTACK("PacketCorpseEquipment::PacketCorpseEquipment");

    const CChar* viewer = target->GetChar();
    if (viewer == nullptr)
    {
        return;
    }

	bool isLayerSent[LAYER_HORSE] = {};

    initLength();
	writeInt32(corpse->GetUID());

    uint count = 0;

	for (CSObjContRec* pObjRec : *corpse)
	{
        const auto item = dynamic_cast<CItem*>(pObjRec);
		if (item == nullptr || (item->IsAttr(ATTR_INVIS) && !viewer->CanSee(item)))
			continue;

        const auto layer = static_cast<LAYER_TYPE>(item->GetContainedLayer());
		ASSERT(layer < LAYER_HORSE);
		switch (layer) // don't put these on a corpse.
		{
			case LAYER_NONE:
			case LAYER_FACE:
			case LAYER_PACK: // these display strange.
				continue;

			default:
				// make certain that no more than one of each layer goes out to client....crashes otherwise!!
				if (isLayerSent[layer])
					continue;
				isLayerSent[layer] = true;
				break;
		}


		writeByte(layer);
		writeInt32(item->GetUID());

		// include tooltip
		target->addAOSTooltip(item);

		if (++count > g_Cfg.m_iContainerMaxItems)
			break;
	}

	writeByte(0); // terminator
	push(target);
}

bool PacketCorpseEquipment::onSend(const CClient* client)
{
	ADDTOCALLSTACK("PacketCorpseEquipment::onSend");

	if (g_NetworkManager.isOutputThreaded())
		return true;

	return client->CanSee(m_corpse.ItemFind());
}


/***************************************************************************
 *
 *
 *	Packet 0x8B : PacketSignGump			show a sign (LOW)
 *
 *
 ***************************************************************************/
PacketSignGump::PacketSignGump(const CClient* target, const CObjBase* object, const GUMP_TYPE gump, const lpctstr unknown, const lpctstr text) : PacketSend(XCMD_GumpTextDisp, 13, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketSignGump::PacketSignGump");

	initLength();
	writeInt32(object->GetUID());
	writeInt16(static_cast<word>(gump));

	if (unknown != nullptr)
	{
        const uint len = static_cast<uint>(strlen(unknown)) + 1;
		writeInt16(static_cast<word>(len));
		writeStringFixedASCII(unknown, len);
	}
	else
	{
		writeInt16(0);
	}

	if (text != nullptr)
	{
        const uint len = static_cast<uint>(strlen(text)) + 1;
		writeInt16(static_cast<word>(len));
		writeStringFixedASCII(text, len);
	}
	else
	{
		writeInt16(0);
	}

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x8C : PacketServerRelay			relay client to server (IDLE)
 *
 *
 ***************************************************************************/
PacketServerRelay::PacketServerRelay(const CClient* target, const dword ip, const word port, const dword customerId) : PacketSend(XCMD_Relay, 11, g_Cfg.m_fUsePacketPriorities? PRI_IDLE : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketServerRelay::PacketServerRelay");
	m_customerId = customerId;

	writeByte((ip      ) & 0xFF);
	writeByte((ip >> 8 ) & 0xFF);
	writeByte((ip >> 16) & 0xFF);
	writeByte((ip >> 24) & 0xFF);
	writeInt16(port);
	writeInt32(customerId);

	push(target);
}

void PacketServerRelay::onSent(CClient* client)
{
	ADDTOCALLSTACK("PacketServerRelay::onSent");

	// in case the client decides not to establish a new connection, change over to the game encryption
	client->m_Crypt.InitFast(m_customerId, CONNECT_GAME); // init decryption table
	client->SetConnectType(client->m_Crypt.GetConnectType());
}


/***************************************************************************
 *
 *
 *	Packet 0x90 : PacketDisplayMap			display map (LOW)
 *
 *
 ***************************************************************************/
PacketDisplayMap::PacketDisplayMap(const CClient* target, const CItemMap* map, const CRectMap& rect) : PacketSend(XCMD_MapDisplay, 19, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketDisplayMap::PacketDisplayMap");

	const CItemBase* itemDef = map->Item_GetDef();
	ASSERT(itemDef != nullptr);

    const word width = static_cast<word>(itemDef->m_ttMap.m_iGumpWidth > 0 ? itemDef->m_ttMap.m_iGumpWidth : static_cast<word>(CItemMap::DEFAULT_SIZE));
    const word height = static_cast<word>(itemDef->m_ttMap.m_iGumpHeight > 0 ? itemDef->m_ttMap.m_iGumpHeight : static_cast<word>(CItemMap::DEFAULT_SIZE));

	writeInt32(map->GetUID());
	writeInt16(GUMP_MAP_2_NORTH);
	writeInt16(static_cast<word>(rect.m_left));
	writeInt16(static_cast<word>(rect.m_top));
	writeInt16(static_cast<word>(rect.m_right));
	writeInt16(static_cast<word>(rect.m_bottom));
	writeInt16(width);
	writeInt16(height);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x93 : PacketDisplayBook			display book (LOW)
 *
 *
 ***************************************************************************/
PacketDisplayBook::PacketDisplayBook(const CClient* target, CItem* book) : PacketSend(XCMD_BookOpen, 99, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketDisplayBook::PacketDisplayBook");

	if ( !book )
		return;

	bool isWritable = false;
	int pages = 0;
	CSString title;
	CSString author;

	if (book->IsBookSystem())
	{
		isWritable = false;

        if (CResourceLock s; g_Cfg.ResourceLock(s, book->m_itBook.m_ResID))
		{
			while (s.ReadKeyParse())
			{
				switch (FindTableSorted(s.GetKey(), CItemMessage::sm_szLoadKeys, std::size(CItemMessage::sm_szLoadKeys) - 1))
				{
					case CIC_AUTHOR:
						author = s.GetArgStr();
						break;
					case CIC_PAGES:
						pages = s.GetArgVal();
						break;
					case CIC_TITLE:
						title = s.GetArgStr();
						break;
				}
			}
		}

		// make sure book is named
		if (title.IsEmpty() == false)
			book->SetName(title);
	}
	else
	{
		// user written book
        if (const auto message = dynamic_cast<const CItemMessage *>(book); message != nullptr)
		{
			isWritable = message->IsBookWritable();
			pages = isWritable ? MAX_BOOK_PAGES : static_cast<int>(message->GetPageCount());
			title = message->GetName();
			author = message->m_sAuthor.IsEmpty()? g_Cfg.GetDefaultMsg(DEFMSG_BOOK_AUTHOR_UNKNOWN) : static_cast<lpctstr>(message->m_sAuthor);
		}
	}


	writeInt32(book->GetUID());
	writeBool(isWritable);
	writeBool(isWritable);
	writeInt16(static_cast<word>(pages));
	writeStringFixedASCII(title, 60);
	writeStringFixedASCII(author, 30);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x95 : PacketShowDyeWindow		show dye window (LOW)
 *
 *
 ***************************************************************************/
PacketShowDyeWindow::PacketShowDyeWindow(const CClient* target, const CObjBase* object) : PacketSend(XCMD_DyeVat, 9, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketShowDyeWindow::PacketShowDyeWindow");

	ITEMID_TYPE id;
	if (object->IsItem())
	{
        const auto item = dynamic_cast<const CItem*>(object);
		ASSERT(item);
		id = item->GetDispID();
	}
	else
	{
		// get the item equiv for the creature
        const auto character = dynamic_cast<const CChar*>(object);
		ASSERT(character);
		id = character->Char_GetDef()->m_trackID;
	}

	writeInt32(object->GetUID());
	writeInt16(object->GetHue());
	writeInt16(static_cast<word>(id));

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x98 : PacketAllNamesResponse	all names macro response (PRI_IDLE)
 *
 *
 ***************************************************************************/
PacketAllNamesResponse::PacketAllNamesResponse(const CClient* target, const CObjBase* object) : PacketSend(XCMD_AllNames3D, 37, g_Cfg.m_fUsePacketPriorities? PRI_IDLE : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketAllNamesResponse::PacketAllNamesResponse");

	initLength();
	writeInt32(object->GetUID());
	writeStringFixedASCII(object->GetName(), 30);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0x9A : PacketAddPrompt			prompt for ascii text response (LOW)
 *	Packet 0xC2 : PacketAddPrompt			prompt for unicode text response (LOW)
 *
 *
 ***************************************************************************/
PacketAddPrompt::PacketAddPrompt(const CClient* target, const CUID &context1, const CUID &context2, const bool useUnicode) : PacketSend(static_cast<byte>(useUnicode ? XCMD_PromptUNICODE : XCMD_Prompt), 16, g_Cfg.m_fUsePacketPriorities ? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketAddPrompt::PacketAddPrompt");

	initLength();

	writeInt32(context1);
	writeInt32(context2);
	writeInt32(0);

	if (useUnicode)
	{
		writeStringFixedASCII("", 4);
		writeCharUTF16('\0');
	}
	else
	{
		writeCharASCII('\0');
	}

	push(target);
}


/***************************************************************************
*
*
*	Packet 0x9E : PacketVendorSellList		show list of items to sell (LOW)
*
*
***************************************************************************/
PacketVendorSellList::PacketVendorSellList(const CChar* vendor) : PacketSend(XCMD_VendOpenSell, 9, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketVendorSellList::PacketVendorSellList");

	initLength();
	writeInt32(vendor->GetUID());
}

uint PacketVendorSellList::fillSellList(CClient* target, const CItemContainer* container, const CItemContainer * stock1, CItemContainer* stock2, const int iConvertFactor)
{
	ADDTOCALLSTACK("PacketVendorSellList::fillSellList");
	UnreferencedParameter(target);
	seek(7); // just to be sure

    const uint countpos = getPosition();
	skip(2);

    const bool bLimitStock = IsSetOF(OF_VendorStockLimit);
	uint count = 0;

	std::deque<const CItemContainer*> otherBoxes;
	while (true)
	{
		if (container == nullptr)
			break;

		for (CSObjContRec* pObjRec : *container)
		{
            const auto item = static_cast<CItem*>(pObjRec);

			container = dynamic_cast<CItemContainer*>(item);
			if (container != nullptr && !container->IsContainerEmpty())
			{
				if (container->IsSearchable())
					otherBoxes.push_back(container);
			}
			else
			{
                if (const auto vendItem = dynamic_cast<CItemVendable *>(item); vendItem != nullptr)
				{
                    if (CItemVendable *vendSell = CChar::NPC_FindVendableItem(vendItem, stock1, stock2); vendSell != nullptr)
					{
						HUE_TYPE hue = vendItem->GetHue() & HUE_MASK_HI;
						if (hue > HUE_QTY)
							hue &= HUE_MASK_LO;

                        const lpctstr name = vendItem->GetName();
						uint len = static_cast<uint>(strlen(name)) + 1;
						if (len > UCHAR_MAX)
							len = UCHAR_MAX;

						writeInt32(vendItem->GetUID());
						writeInt16(static_cast<word>(vendItem->GetDispID()));
						writeInt16(hue);

						if (bLimitStock) {
							if (vendSell->GetAmount() <= vendItem->GetAmount())
								writeInt16(vendSell->GetAmount());
							else
								writeInt16(vendItem->GetAmount());
						}
						else {
							writeInt16(vendItem->GetAmount());
						}

						uint price = 0;

						// If OVERRIDE.VALUE is define on the script and this NPC buy this item at a specific price, we use this price in priority
						// Else, we calculate the value of the item in the player's backpack
						if (vendSell->GetKey("OVERRIDE.VALUE", true))
						{
							//Get the price on NPC template
							price = vendSell->GetVendorPrice(iConvertFactor, true);
						}
						else
						{
							//Get the price/Value of the real item in the backpack
							price = vendItem->GetVendorPrice(iConvertFactor, true);
						}

						writeInt16(static_cast<word>((price > UINT16_MAX) ? UINT16_MAX : price));
						writeInt16(static_cast<word>(len));
						writeStringFixedASCII(name, len);

						if (++count >= g_Cfg.m_iContainerMaxItems)
							break;
					}
				}
			}
		}

        if (otherBoxes.empty())
            break;

        container = otherBoxes.front();
        otherBoxes.pop_front();
	}

	// seek back to write count
    const uint endpos = getPosition();
	seek(countpos);
	writeInt16(static_cast<word>(count));
	seek(endpos);

	return count;
}


/***************************************************************************
 *
 *
 *	Packet 0xA1 : PacketHealthUpdate		update character health (LOW)
 *
 *
 ***************************************************************************/
PacketHealthUpdate::PacketHealthUpdate(const CChar* character, const bool full) : PacketSend(XCMD_StatChngStr, 9, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketHealthUpdate::PacketHealthUpdate");

	writeInt32(character->GetUID());

	if ( full )
	{
		writeInt16(character->Stat_GetMaxAdjusted(STAT_STR));
		writeInt16(character->Stat_GetVal(STAT_STR));
	}
	else
	{
		writeInt16(100);
        const ushort iStatMax = character->Stat_GetMaxAdjusted(STAT_STR);
		writeInt16(static_cast<word>((character->Stat_GetVal(STAT_STR) * 100) / maximum(iStatMax, 1)));
	}
}


/***************************************************************************
 *
 *
 *	Packet 0xA2 : PacketManaUpdate			update character mana (LOW)
 *
 *
 ***************************************************************************/
PacketManaUpdate::PacketManaUpdate(const CChar* character, const bool full) : PacketSend(XCMD_StatChngInt, 9, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketManaUpdate::PacketManaUpdate");

	writeInt32(character->GetUID());

	if ( full )
	{
		writeInt16(character->Stat_GetMaxAdjusted(STAT_INT));
		writeInt16(character->Stat_GetVal(STAT_INT));
	}
	else
	{
		writeInt16(100);
        const ushort iStatMax = character->Stat_GetMaxAdjusted(STAT_INT);
		writeInt16(static_cast<word>((character->Stat_GetVal(STAT_INT) * 100) / maximum(iStatMax, 1)));
	}
}


/***************************************************************************
 *
 *
 *	Packet 0xA3 : PacketStaminaUpdate		update character stamina (LOW)
 *
 *
 ***************************************************************************/
PacketStaminaUpdate::PacketStaminaUpdate(const CChar* character, const bool full) : PacketSend(XCMD_StatChngDex, 9, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketStaminaUpdate::PacketStaminaUpdate");

	writeInt32(character->GetUID());

	if ( full )
	{
		writeInt16(character->Stat_GetMaxAdjusted(STAT_DEX));
		writeInt16(character->Stat_GetVal(STAT_DEX));
	}
	else
	{
		writeInt16(100);
        const ushort iStatMax = character->Stat_GetMaxAdjusted(STAT_DEX);
		writeInt16(static_cast<word>((character->Stat_GetVal(STAT_DEX) * 100) / maximum(iStatMax, 1)));
	}
}


/***************************************************************************
 *
 *
 *	Packet 0xA5 : PacketWebPage				send client to a webpage (LOW)
 *
 *
 ***************************************************************************/
PacketWebPage::PacketWebPage(const CClient* target, const lpctstr url) : PacketSend(XCMD_Web, 3, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketWebPage::PacketWebPage");

	initLength();
	writeStringASCII(url);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xA6 : PacketOpenScroll			open scroll message (LOW)
 *
 *
 ***************************************************************************/
PacketOpenScroll::PacketOpenScroll(const CClient* target, CResourceLock &s, const SCROLL_TYPE type, const dword context, const lpctstr header) : PacketSend(XCMD_Scroll, 10, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketOpenScroll::PacketOpenScroll");

	initLength();

	writeByte(static_cast<byte>(type));
	writeInt32(context);

    const uint lengthPosition(getPosition());
	skip(2);

	if (header)
	{
		writeStringASCII(header, false);
		writeCharASCII(0x0D);
		writeStringASCII("  ", false);
		writeCharASCII(0x0D);
	}

	while (s.ReadKey(false))
	{
		writeStringASCII(s.GetKey(), false);
		writeCharASCII(0x0D);
	}

    const uint endPosition(getPosition());
    const uint length = getPosition() - lengthPosition;
	seek(lengthPosition);
	writeInt16(static_cast<word>(length));
	seek(endPosition);

	writeCharASCII('\0');

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xA8 : PacketServerList			send server list (LOW)
 *
 *
 ***************************************************************************/
PacketServerList::PacketServerList(const CClient* target) : PacketSend(XCMD_ServerList, 46, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketServerList::PacketServerList");

	// clients before 4.0.0 require serverlist ips to be in reverse
    const bool reverseIp = target->GetNetState()->isClientLessVersionNumber(MAXCLIVER_REVERSEIP);

	initLength();
	writeByte(0xFF);

	word count = 0;
    const uint countPosition = getPosition();
	skip(2);

	writeServerEntry(&g_Serv, ++count, reverseIp);

	//	too many servers in list can crash the client
#define	MAX_SERVERS_LIST	32
	for (uint i = 0; count < MAX_SERVERS_LIST; ++i)
	{
		CServerRef server = g_Cfg.Server_GetDef(i);
		if (server == nullptr)
			break;

		writeServerEntry(server, ++count, reverseIp);
	}
#undef MAX_SERVERS_LIST

    const uint endPosition(getPosition());
	seek(countPosition);
	writeInt16(count);
	seek(endPosition);

	push(target);
}

void PacketServerList::writeServerEntry(const CServerRef& server, const int index, const bool reverseIp)
{
	ADDTOCALLSTACK("PacketServerList::writeServerEntry");

	uint percentFull;
    const size_t servClients = server->StatGet(SERV_STAT_CLIENTS);
	if (server == &g_Serv)
		percentFull = static_cast<uint>((minimum((servClients * 100) / maximum(1, g_Cfg.m_iClientsMax), 100)));
		//percentFull = (int)maximum(0, minimum((servClients * 100) / maximum(1, g_Cfg.m_iClientsMax), 100));
	else
		percentFull = static_cast<uint>((minimum(servClients, 100)));

    const dword ip = server->m_ip.GetAddrIP();


	writeInt16(static_cast<word>(index));
	writeStringFixedASCII(server->GetName(), 32);
	writeByte(static_cast<byte>(percentFull));
	writeByte(server->m_TimeZone);

	if (reverseIp)
	{
		// Clients less than 4.0.0 require IP to be sent in reverse
		writeByte((ip      ) & 0xFF);
		writeByte((ip >> 8 ) & 0xFF);
		writeByte((ip >> 16) & 0xFF);
		writeByte((ip >> 24) & 0xFF);
	}
	else
	{
		// Clients since 4.0.0 require IP to be sent in order
		writeByte((ip >> 24) & 0xFF);
		writeByte((ip >> 16) & 0xFF);
		writeByte((ip >> 8 ) & 0xFF);
		writeByte((ip      ) & 0xFF);
	}
}


/***************************************************************************
 *
 *
 *	Packet 0xA9 : PacketCharacterList		send character list (LOW)
 *
 *
 ***************************************************************************/
PacketCharacterList::PacketCharacterList(CClient* target) : PacketSend(XCMD_CharList, 9, g_Cfg.m_fUsePacketPriorities ? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketCharacterList::PacketCharacterList");

	ASSERT(target != nullptr);
	const CAccount * account = target->GetAccount();
	ASSERT(account != nullptr);

	initLength();

    const uint countPos = getPosition();
	skip(1);

    const uchar count =  n32_narrow_n8(
        target->Setup_FillCharList(this, account->m_uidLastChar.CharFind()));
	seek(countPos);

	writeByte(count);
	skip(count * 60);

    const size_t startCount = g_Cfg.m_StartDefs.size();
	writeByte( static_cast<byte>((startCount > UINT8_MAX) ? UINT8_MAX : startCount) );

	// since 7.0.13.0, start locations have extra information
    const dword tmVer = static_cast<dword>(account->m_TagDefs.GetKeyNum("clientversion"));
    const dword tmVerReported = static_cast<dword>(account->m_TagDefs.GetKeyNum("reportedcliver"));
	if ( tmVer >= MINCLIVER_EXTRASTARTINFO || tmVerReported >= MINCLIVER_EXTRASTARTINFO )
	{
		// newer clients receive additional start info
		for (size_t i = 0; i < startCount; ++i )
		{
			const CStartLoc *start = g_Cfg.m_StartDefs[i];
			ASSERT(start);
			writeByte(static_cast<byte>(i));
			writeStringFixedASCII(start->m_sArea, MAX_NAME_SIZE + 2);
			writeStringFixedASCII(start->m_sName, MAX_NAME_SIZE + 2);
			writeInt32(start->m_pt.m_x);
			writeInt32(start->m_pt.m_y);
			writeInt32(start->m_pt.m_z);
			writeInt32(start->m_pt.m_map);
			writeInt32(start->iClilocDescription);
			writeInt32(0);
		}
	}
	else
	{
		for (size_t i = 0; i < startCount; ++i )
		{
			const CStartLoc *start = g_Cfg.m_StartDefs[i];
			ASSERT(start);
			writeByte(static_cast<byte>(i));
			writeStringFixedASCII(start->m_sArea, MAX_NAME_SIZE + 1);
			writeStringFixedASCII(start->m_sName, MAX_NAME_SIZE + 1);
		}
	}

    if (tmVerReported > 1'26'00'00)
    {
		const CNetState* ns = target->GetNetState();
        dword flags = g_Cfg.GetPacketFlag(true, account->GetResDisp(),
            std::max(account->GetMaxChars(), static_cast<byte>(account->m_Chars.GetCharCount())));
        if (ns->getClientType() == CLIENTTYPE_2D)
            flags |= 0x400;
        writeInt32(flags);

        if (ns->isClientEnhanced() )
        {
            word wLastCharSlot = 0;
            for ( ushort i = 0; i < count; ++i )
            {
                if ( !account->m_Chars.IsValidIndex(i) )
                    continue;
                if ( account->m_Chars.GetChar(i) != account->m_uidLastChar )
                    continue;

                wLastCharSlot = i;
                break;
            }
            writeInt16(wLastCharSlot);
        }
    }

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xAA : PacketAttack				set attack target (NORMAL)
 *
 *
 ***************************************************************************/
PacketAttack::PacketAttack(const CClient* target, const CUID &serial) : PacketSend(XCMD_AttackOK, 5, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketAttack::PacketAttack");

    // UID = 00 00 00 00 means attack request refused
	writeInt32(serial);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xAB : PacketGumpValueInput		show input dialog (LOW)
 *
 *
 ***************************************************************************/
PacketGumpValueInput::PacketGumpValueInput(const CClient* target, const bool cancel, const INPVAL_STYLE style, const dword maxLength, const lpctstr text, const lpctstr caption, const CObjBase * object) : PacketSend(XCMD_GumpInpVal, 21, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketGumpValueInput::PacketGumpValueInput");

	initLength();
	writeInt32(object->GetUID());
	writeInt16(CLIMODE_INPVAL);

	int len = static_cast<int>(strlen(text)) + 1;
	if (len > 255)
		len = 255;

	writeInt16(static_cast<word>(len));
	writeStringFixedASCII(text, len);

	writeBool(cancel);
	writeByte(static_cast<byte>(style));
	writeInt32(maxLength);

	tchar* z = nullptr;
	switch (style)
	{
		case INPVAL_STYLE_NOEDIT: // None
		default:
			len = 1;
			break;

		case INPVAL_STYLE_TEXTEDIT: // Text
			z = Str_GetTemp();
			len = snprintf(z, Str_TempLength(), "%s (%u chars max)", caption, maxLength) + 1;
			break;

		case INPVAL_STYLE_NUMEDIT: // Numeric
			z = Str_GetTemp();
			len = snprintf(z, Str_TempLength(), "%s (0 - %u)", caption, maxLength) + 1;
			break;
	}

	if (len > 255)
		len = 255;
	writeInt16(static_cast<word>(len));
	writeStringFixedASCII(z, len);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xAE: PacketMessageUNICODE			show message to client (NORMAL)
 *
 *
 ***************************************************************************/
PacketMessageUNICODE::PacketMessageUNICODE(const CClient* target, const nachar * pszText, const CObjBaseTemplate * source, const HUE_TYPE hue, const TALKMODE_TYPE mode, const FONT_TYPE font, const CLanguageID language) : PacketSend(XCMD_SpeakUNICODE, 48, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketMessageUNICODE::PacketMessageUNICODE");

	initLength();

	if (source == nullptr)
		writeInt32(0xFFFFFFFF);
	else
		writeInt32(source->GetUID());

	if (source == nullptr || source->IsChar() == false)
	{
		writeInt16(0xFFFF);
	}
	else
	{
        const auto sourceCharacter = dynamic_cast<const CChar*>(source);
		ASSERT(sourceCharacter);
		writeInt16(static_cast<word>(sourceCharacter->GetDispID()));
	}

	writeByte(static_cast<byte>(mode));
	writeInt16(hue);
	writeInt16(font);
	writeStringFixedASCII(language.GetStr(), 4);

	if (source == nullptr)
		writeStringFixedASCII("System", 30);
	else
		writeStringFixedASCII(source->GetName(), 30);

	writeStringUTF16(reinterpret_cast<const wchar*>(pszText));

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xAF : PacketDeath				notifies about character death (NORMAL)
 *
 *
 ***************************************************************************/
PacketDeath::PacketDeath(const CChar * dead, const CItemCorpse * corpse, const bool fFrontFall) : PacketSend(XCMD_CharDeath, 13, PRI_NORMAL)
{
	//UnreferencedParameter(fFrontFall);
	ADDTOCALLSTACK("PacketDeath::PacketDeath");

	writeInt32(dead->GetUID());
	writeInt32(corpse == nullptr ? 0 : static_cast<dword>(corpse->GetUID()));
	writeInt32(fFrontFall);
}


/***************************************************************************
 *
 *
 *	Packet 0xB0 : PacketGumpDialog			displays a dialog gump (LOW)
 *	Packet 0xDD : PacketGumpDialog			displays a dialog gump using compression (LOW)
 *
 *
 ***************************************************************************/
PacketGumpDialog::PacketGumpDialog(const int x, const int y, const CObjBase * object, const dword context) : PacketSend(XCMD_GumpDialog, 24, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketGumpDialog::PacketGumpDialog");

	initLength();

	writeInt32(object->GetUID());
	writeInt32(context);
	writeInt32(x);
	writeInt32(y);
}

void PacketGumpDialog::writeControls(const CClient* target, std::vector<CSString> const* controls, std::vector<CSString> const* texts)
{
	ADDTOCALLSTACK("PacketGumpDialog::writeControls");

    if (const CNetState *net = target->GetNetState(); net->isClientVersionNumber(MINCLIVER_COMPRESSDIALOG) || net->isClientKR() || net->isClientEnhanced())
		writeCompressedControls(controls, texts);
	else
		writeStandardControls(controls, texts);
}

void PacketGumpDialog::writeCompressedControls(std::vector<CSString> const* controls, std::vector<CSString> const* texts)
{
	ADDTOCALLSTACK("PacketGumpDialog::writeCompressedControls");

	seek(0);
	writeByte(XCMD_CompressedGumpDialog);

	seek(19);

    if (controls)
	{
		// compress and write controls
		uint controlLength = 1;
		for (CSString const& ctrl : *controls)
        {
            controlLength += static_cast<uint>(ctrl.GetLength()) + 2; // String terminator not needed.
        }

        const auto toCompress = new char[controlLength];
		uint controlLengthCurrent = 0;
		for (CSString const& ctrl : *controls)
        {
            const uint uiAvailableLength = std::max(0u, controlLength - controlLengthCurrent);
            const int iJustWrittenLength = snprintf(&toCompress[controlLengthCurrent], uiAvailableLength, "{%s}", ctrl.GetBuffer());
            controlLengthCurrent += iJustWrittenLength;
        }
		++ controlLengthCurrent;

		ASSERT(controlLengthCurrent == controlLength);

		zlib::uLong compressLength = zlib::compressBound(controlLengthCurrent);
        const auto compressBuffer = new byte[compressLength];

        const int error = zlib::compress2(compressBuffer, &compressLength, reinterpret_cast<byte *>(toCompress), controlLengthCurrent, Z_DEFAULT_COMPRESSION);
		delete[] toCompress;

		if (error != Z_OK || compressLength <= 0)
		{
			delete[] compressBuffer;
			g_Log.EventError("Compress failed with error %d when generating gump. Using old packet.\n", error);
			writeStandardControls(controls, texts);
			return;
		}

		writeInt32(static_cast<dword>(compressLength) + 4u);
		writeInt32(controlLengthCurrent);
		writeData(compressBuffer, static_cast<uint>(compressLength));

		delete[] compressBuffer;
	}
    else
    {
        writeInt32(0);
    }

    if (texts)
	{
		// compress and write texts
        const uint textsPosition(getPosition());

		for (CSString const& txt : *texts)
		{
			writeInt16(static_cast<word>(txt.GetLength()));
			writeStringFixedNETUTF16(txt.GetBuffer(), txt.GetLength());
		}

        const uint textsLength = getPosition() - textsPosition;

		zlib::uLong compressLength = zlib::compressBound(textsLength);
        const auto compressBuffer = new byte[compressLength];

        if (const int error = zlib::compress2(compressBuffer, &compressLength, &m_buffer[textsPosition], textsLength, Z_DEFAULT_COMPRESSION);
            error != Z_OK || compressLength <= 0)
		{
			delete[] compressBuffer;
			g_Log.EventError("Compress failed with error %d when generating gump. Using old packet.\n", error);
			writeStandardControls(controls, texts);
			return;
		}

		seek(textsPosition);
		writeInt32(static_cast<dword>(texts->size()));
		writeInt32(compressLength + 4);
		writeInt32(textsLength);
		writeData(compressBuffer, compressLength);

		delete[] compressBuffer;
	}
}

void PacketGumpDialog::writeStandardControls(std::vector<CSString> const* controls, std::vector<CSString> const* texts)
{
	ADDTOCALLSTACK("PacketGumpDialog::writeStandardControls");

	seek(0);
	writeByte(XCMD_GumpDialog);

	seek(19);

	// skip controls length until they're written
    const uint controlLengthPosition(getPosition());
	skip(2);

    if (controls)
    {
        // write controls
        for (CSString const& ctrl : *controls)
        {
            writeCharASCII('{');
            writeStringASCII(ctrl.GetBuffer(), false);
            writeCharASCII('}');
        }

        // write controls length
        const uint endPosition(getPosition());
        seek(controlLengthPosition);
        writeInt16(static_cast<word>(endPosition - controlLengthPosition - 2));
        seek(endPosition);
    }
    else
    {
        writeInt16(0);
    }

    if (texts)
    {
        // write texts
        writeInt16(static_cast<word>(texts->size()));
        for (CSString const& txt : *texts)
        {
            writeInt16(static_cast<word>(txt.GetLength()));
            writeStringFixedNETUTF16(txt.GetBuffer(), txt.GetLength());
        }
    }
    else
    {
        writeInt16(0);
    }
}


/***************************************************************************
 *
 *
 *	Packet 0xB2 : PacketChatMessage			send a chat system message (LOW)
 *
 *
 ***************************************************************************/
PacketChatMessage::PacketChatMessage(const CClient* target, const CHATMSG_TYPE type, const lpctstr param1, const lpctstr param2, const CLanguageID language) : PacketSend(XCMD_ChatReq, 11, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketChatMessage::PacketChatMessage");

	initLength();
	writeInt16(static_cast<word>(type));
	writeStringFixedASCII(language.GetStr(), 4);

	if (param1 != nullptr)
		writeStringNETUTF16(param1);
	else
		writeCharNETUTF16('\0');

	if (param2 != nullptr)
		writeStringNETUTF16(param2);
	else
		writeCharNETUTF16('\0');

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xB7 : PacketTooltip				send a tooltip (IDLE)
 *
 *
 ***************************************************************************/
PacketTooltip::PacketTooltip(const CClient* target, const CObjBase* object, const lpctstr text) : PacketSend(XCMD_ToolTip, 8, g_Cfg.m_fUsePacketPriorities? PRI_IDLE : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketTooltip::PacketTooltip");

	initLength();
	writeInt32(object->GetUID());
	writeStringNETUTF16(text);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xB8 : PacketProfile				send a character profile (LOW)
 *
 *
 ***************************************************************************/
PacketProfile::PacketProfile(const CClient* target, const CChar* character) : PacketSend(XCMD_CharProfile, 12, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketProfile::PacketProfile");

	// alter profile when viewing an incognitoed player, unless being viewed by a GM or the profile is our own
    const bool isIncognito = character->IsStatFlag(STATF_INCOGNITO) && !target->IsPriv(PRIV_GM) && character != target->GetChar();

	initLength();

	writeInt32(character->GetUID());
	writeStringASCII(character->GetName());

	if (isIncognito == false)
	{
		CSString sConstText;
		sConstText.Format("%s, %s", character->Noto_GetTitle(), character->GetTradeTitle());

		writeStringNETUTF16(sConstText);

		if (character->m_pPlayer != nullptr)
			writeStringNETUTF16(character->m_pPlayer->m_sProfile);
		else
			writeCharNETUTF16('\0');
	}
	else
	{
		writeStringNETUTF16(character->Noto_GetTitle());
		writeCharNETUTF16('\0');
	}

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xB9 : PacketEnableFeatures		enable client features (NORMAL)
 *
 *
 ***************************************************************************/
PacketEnableFeatures::PacketEnableFeatures(const CClient* target, const dword flags) : PacketSend(XCMD_Features, 5, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketEnableFeatures::PacketEnableFeatures");

	const CAccount * account = target->GetAccount();
	ASSERT(account != nullptr);
    const dword tmVer = static_cast<dword>(account->m_TagDefs.GetKeyNum("clientversion"));

    // since 6.0.14.2, feature flags are 4 bytes instead of 2.
	if (const dword tmVerReported = static_cast<dword>(account->m_TagDefs.GetKeyNum("reportedcliver"));
        tmVer >= MINCLIVER_EXTRAFEATURES || tmVerReported >= MINCLIVER_EXTRAFEATURES)
		writeInt32(flags);
	else
		writeInt16(static_cast<word>(flags));

	trim();
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xBA : PacketArrowQuest			display onscreen arrow for client to follow (NORMAL)
 *
 *
 ***************************************************************************/
PacketArrowQuest::PacketArrowQuest(const CClient* target, const int x, const int y, const int id) : PacketSend(XCMD_Arrow, 10, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketArrowQuest::PacketArrowQuest");

	writeBool(x && y);
	writeInt16(static_cast<word>(x));
	writeInt16(static_cast<word>(y));

	if (target->GetNetState()->isClientVersionNumber(MINCLIVER_HS) || target->GetNetState()->isClientEnhanced())
		writeInt32(id);

	trim();
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xBC : PacketSeason				change season (NORMAL)
 *
 *
 ***************************************************************************/
PacketSeason::PacketSeason(const CClient* target, const SEASON_TYPE season, const bool playMusic) : PacketSend(XCMD_Season, 3, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketSeason::PacketSeason");

	writeByte(season);
	writeBool(playMusic);

	push(target);
}


/***************************************************************************
*
*
*	Packet 0xBD : PacketClientVersionReq	request client version (HIGH)
*
*
***************************************************************************/
PacketClientVersionReq::PacketClientVersionReq(const CClient* target) : PacketSend(XCMD_ClientVersion, 3, PRI_HIGH)
{
	ADDTOCALLSTACK("PacketClientVersionReq::PacketClientVersionReq");

	initLength();
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xBF : PacketExtended			extended command
 *
 *
 ***************************************************************************/
PacketExtended::PacketExtended(const EXTDATA_TYPE type, const uint len, const Priority priority) : PacketSend(XCMD_ExtData, len, priority)
{
	ADDTOCALLSTACK("PacketExtended::PacketExtended");

	initLength();

	writeInt16(static_cast<word>(type));
}


/***************************************************************************
 *
 *
 *	Packet 0xBF.0x04 : PacketGumpChange		change gump (LOW)
 *
 *
 ***************************************************************************/
PacketGumpChange::PacketGumpChange(const CClient* target, const dword context, const int buttonId) : PacketExtended(EXTDATA_GumpChange, 13, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketGumpChange::PacketGumpChange");

	writeInt32(context);
	writeInt32(buttonId);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xBF.0x06 : PacketParty			party packet
 *
 *
 ***************************************************************************/
PacketParty::PacketParty(const PARTYMSG_TYPE type, const uint len, const Priority priority) : PacketExtended(EXTDATA_Party_Msg, len, priority)
{
	ADDTOCALLSTACK("PacketParty::PacketParty");

	writeByte(static_cast<byte>(type));
}


/***************************************************************************
 *
 *
 *	Packet 0xBF.0x06.0x01 : PacketPartyList		send list of party members (NORMAL)
 *
 *
 ***************************************************************************/
PacketPartyList::PacketPartyList(const CCharRefArray* members) : PacketParty(PARTYMSG_Add, 11, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketPartyList::PacketPartyList");

    const size_t iQty = members->GetCharCount();

	writeByte(static_cast<byte>(iQty));

	for (uint i = 0; i < iQty; ++i)
		writeInt32(members->GetChar(i));
}


/***************************************************************************
 *
 *
 *	Packet 0xBF.0x06.0x02 : PacketPartyRemoveMember		remove member from party (NORMAL)
 *
 *
 ***************************************************************************/
PacketPartyRemoveMember::PacketPartyRemoveMember(const CChar* member, const CCharRefArray* members) : PacketParty(PARTYMSG_Remove, 11, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketPartyRemoveMember::PacketPartyRemoveMember");

	ASSERT(member != nullptr);

    const size_t iQty = members == nullptr? 0 : members->GetCharCount();

	writeByte(static_cast<byte>(iQty));
	writeInt32(member->GetUID());

	for (uint i = 0; i < iQty; i++)
		writeInt32(members->GetChar(i));
}


/***************************************************************************
 *
 *
 *	Packet 0xBF.0x06.0x04 : PacketPartyChat		send party chat message (NORMAL)
 *
 *
 ***************************************************************************/
PacketPartyChat::PacketPartyChat(const CChar* source, const nachar* text) : PacketParty(PARTYMSG_Msg, 11, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketPartyChat::PacketPartyChat");

	writeInt32(source->GetUID());
	writeStringUTF16(reinterpret_cast<const wchar*>(text));
}


/***************************************************************************
 *
 *
 *	Packet 0xBF.0x06.0x07 : PacketPartyInvite	send party invitation (NORMAL)
 *
 *
 ***************************************************************************/
PacketPartyInvite::PacketPartyInvite(const CClient* target, const CChar* inviter) : PacketParty(PARTYMSG_NotoInvited, 10, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketPartyInvite::PacketPartyInvite");

	writeInt32(inviter->GetUID());

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xBF.0x08 : PacketMapChange			change map (NORMAL)
 *
 *
 ***************************************************************************/
PacketMapChange::PacketMapChange(const CClient* target, const int map) : PacketExtended(EXTDATA_Map_Change, 6, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketMapChange::PacketMapChange");

	writeByte(static_cast<byte>(map));

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xBF.0x10 : PacketPropertyListVersionOld		property (tool tip) version (LOW)
 *
 *
 ***************************************************************************/
PacketPropertyListVersionOld::PacketPropertyListVersionOld(const CClient* target, const CObjBase* object, const dword version) : PacketExtended(EXTDATA_OldAOSTooltipInfo, 13, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketPropertyListVersionOld::PacketPropertyListVersionOld");

	m_object = object->GetUID();

	writeInt32(m_object);
	writeInt32(version);

	if (target != nullptr)
		push(target, false);
}

bool PacketPropertyListVersionOld::onSend(const CClient* client)
{
	ADDTOCALLSTACK("PacketPropertyListVersionOld::onSend");
	if (g_NetworkManager.isOutputThreaded())
		return true;

	const CChar* character = client->GetChar();
	if (character == nullptr)
		return false;

	const CObjBase* object = m_object.ObjFind();
    if (const int iCharVisualRange = character->GetVisualRange();
        object == nullptr || character->GetTopDistSight(object->GetTopLevelObj()) > maximum(iCharVisualRange, g_Cfg.m_iMapViewSize))
		return false;

	return true;
}

bool PacketPropertyListVersionOld::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_TOOLTIP);
}


/***************************************************************************
 *
 *
 *	Packet 0xBF.0x14 : PacketDisplayPopup		display popup menu (LOW)
 *
 *
 ***************************************************************************/
PacketDisplayPopup::PacketDisplayPopup(const CClient* target, const CUID &uid) : PacketExtended(EXTDATA_Popup_Display, 12, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketDisplayPopup::PacketDisplayPopup");

	m_popupCount = 0;
	m_newPacketFormat = target->GetNetState()->isClientKR() || target->GetNetState()->isClientEnhanced() || target->GetNetState()->isClientVersionNumber(MINCLIVER_NEWCONTEXTMENU);

	if (m_newPacketFormat)
		writeInt16(2);
	else
		writeInt16(1);

	writeInt32(uid);

	writeByte(0); // popup count
}

void PacketDisplayPopup::addOption(const word entryTag, dword textId, word flags, const word color)
{
	ADDTOCALLSTACK("PacketDisplayPopup::addOption");

	if (m_popupCount >= g_Cfg.m_iContextMenuLimit)
	{
		DEBUG_ERR(("Bad AddContextEntry usage: Too many entries, max = %d\n", static_cast<int>(MAX_POPUPS)));
		return;
	}

	if (m_newPacketFormat)
	{
		if ( textId <= 32767 )
			textId += 3000000;
		if (flags & POPUPFLAG_COLOR)
			flags &= ~POPUPFLAG_COLOR;

		writeInt32(textId);
		writeInt16(entryTag);
		writeInt16(flags);
	}
	else
	{
		writeInt16(entryTag);
		writeInt16(static_cast<word>(textId));
		writeInt16(flags);

		if (flags & POPUPFLAG_COLOR)
			writeInt16(color);
	}

	m_popupCount++;
}

void PacketDisplayPopup::finalise()
{
	ADDTOCALLSTACK("PacketDisplayPopup::finalise");

    const uint endPosition(getPosition());

	seek(11);
	writeByte(static_cast<byte>(m_popupCount));

	seek(endPosition);
}

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x16 : PacketCloseUIWindow		Close User Interface Windows (NORMAL)
 *
 *
 ***************************************************************************/
PacketCloseUIWindow::PacketCloseUIWindow(const CClient* target, const CObjBase* obj, const PacketCloseUIWindowType command) : PacketExtended(EXTDATA_CloseUI_Window, 13, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketCloseUIWindow::PacketCloseUIWindow");

    // obj: Character or Container
	writeInt32(command);
	writeInt32(obj->GetUID());

	push(target);
}

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x16.0x0C : PacketCloseContainer		Close Container (NORMAL)
 *
 *
 ***************************************************************************/
PacketCloseContainer::PacketCloseContainer(const CClient* target, const CObjBase* object) : PacketExtended(EXTDATA_CloseUI_Window, 13, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketCloseContainer::PacketCloseContainer");

	writeInt32(0x0C);
	writeInt32(object->GetUID());

	push(target);
}

/***************************************************************************
*
*
*	Packet 0xBF.0x17 : PacketCodexOfWisdom		open Codex of Wisdom (LOW)
*
*
***************************************************************************/
PacketCodexOfWisdom::PacketCodexOfWisdom(const CClient *target, const dword dwTopicID, const bool fForceOpen) : PacketExtended(EXTDATA_Codex_Wisdom, 11, g_Cfg.m_fUsePacketPriorities ? PRI_LOW : PRI_NORMAL)
{
    ADDTOCALLSTACK("PacketCodexOfWisdom::PacketCodexOfWisdom");

    writeByte(1);
    writeInt32(dwTopicID);
    writeBool(fForceOpen);

    push(target);
}

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x18 : PacketEnableMapDiffs		enable use of map diff files (NORMAL)
 *
 *
 ***************************************************************************/
PacketEnableMapDiffs::PacketEnableMapDiffs(const CClient* target) : PacketExtended(EXTDATA_Map_Diff, 13, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketEnableMapDiffs::PacketEnableMapDiffs");

	int mapCount = 1;
	int map;

	// find map count
	for (map = MAP_SUPPORTED_QTY - 1; map >= 0; --map)
	{
		if (g_MapList.IsMapSupported(map) == false)
			continue;

		mapCount = map;
		break;
	}

	writeInt32(mapCount);

	for (map = 0; map < mapCount; map++)
	{
		if (g_Cfg.m_fUseMapDiffs && g_MapList.IsMapSupported(map))
		{
			if (g_Install.m_Mapdifl[map].IsFileOpen())
				writeInt32(static_cast<dword>(g_Install.m_Mapdifl[map].GetLength()) / 4);
			else
				writeInt32(0);

			if (g_Install.m_Stadifl[map].IsFileOpen())
				writeInt32(static_cast<dword>(g_Install.m_Stadifl[map].GetLength()) / 4);
			else
				writeInt32(0);
		}
		else
		{
			// mapdiffs are disabled or map does not exist
			writeInt32(0);
			writeInt32(0);
		}
	}

	push(target);
}


/***************************************************************************
*
*
*	Packet 0xBF.0x19.0x00 : BondedStatus			    set bonded status (NORMAL)
*
*
***************************************************************************/

PacketBondedStatus::PacketBondedStatus(const CClient * target, const CChar * pChar, const bool IsGhost) : PacketExtended(EXTDATA_BondedStatus, 11, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketBondedStatus::PacketBondedStatus");

	writeByte(0x0);
	writeInt32(pChar->GetUID());
	writeByte(IsGhost);

	push(target);
}


/***************************************************************************
*
*
*	Packet 0xBF.0x19.0x02 : PacketStatLocks		update lock status of stats (NORMAL)
*
*
***************************************************************************/
PacketStatLocks::PacketStatLocks(const CClient* target, const CChar* character) : PacketExtended(EXTDATA_Stats_Enable, 12, PRI_NORMAL)
{
    ADDTOCALLSTACK("PacketStatLocks::PacketStatLocks");

    byte status = 0;
    if (character->m_pPlayer != nullptr)
    {
        status |= static_cast<byte>(character->m_pPlayer->Stat_GetLock(STAT_INT));
        status |= static_cast<byte>(character->m_pPlayer->Stat_GetLock(STAT_DEX)) << 2;
        status |= static_cast<byte>(character->m_pPlayer->Stat_GetLock(STAT_STR)) << 4;
    }

    /*
    // Packet guides report this difference, but it would be better to test this before uncommenting
    const CNetState* ns = target->GetNetState();
    if (ns->isClient3D() || ns->isClientKR())
        writeByte(0x05);
    else
    */
        writeByte(0x02);
    writeInt32(character->GetUID());
    writeByte(0);
    writeByte(status);

    push(target);
}

bool PacketStatLocks::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_STATLOCKS);
}

/***************************************************************************
*
*
*	Packet 0xBF.0x19.0x05 : PacketStatueAnimation	update character animation frame (NORMAL)
*
*
***************************************************************************/
PacketStatueAnimation::PacketStatueAnimation(const CClient * target, const CChar * pChar, const int iAnimation, const int iFrame) : PacketExtended(EXTDATA_StatueAnimation, 17, PRI_NORMAL)
{
    ADDTOCALLSTACK("PacketStatue::PacketStatue");

    writeByte(0x05);
    writeInt32(pChar->GetUID());
    writeByte(0x00);
    writeByte(0xFF);
    writeByte(0x01);
    writeInt16(static_cast<word>(iAnimation));
    writeInt16(static_cast<word>(iFrame));

    push(target);
}

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x1B : PacketSpellbookContent	spellbook content (NORMAL)
 *
 *
 ***************************************************************************/
PacketSpellbookContent::PacketSpellbookContent(const CClient* target, const CItem* spellbook, const word offset) : PacketExtended(EXTDATA_NewSpellbook, 23, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketSpellbookContent::PacketSpellbookContent");

	writeInt16(1);
	writeInt32(spellbook->GetUID());
	writeInt16(static_cast<word>(spellbook->GetDispID()));
	writeInt16(offset);
	writeInt64(spellbook->m_itSpellbook.m_spells1, spellbook->m_itSpellbook.m_spells2);

	push(target);
}

bool PacketSpellbookContent::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_SPELLBOOK);
}


/***************************************************************************
 *
 *
 *	Packet 0xBF.0x1D : PacketHouseDesignVersion	house design version (LOW)
 *
 *
 ***************************************************************************/
PacketHouseDesignVersion::PacketHouseDesignVersion(const CClient* target, const CItemMultiCustom* house) : PacketExtended(EXTDATA_HouseDesignVer, 13, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketHouseDesignVersion::PacketHouseDesignVersion");

	writeInt32(house->GetUID());
	writeInt32(house->GetRevision(target));

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xBF.0x20.0x04 : PacketHouseBeginCustomise	begin house customisation (NORMAL)
 *
 *
 ***************************************************************************/
PacketHouseBeginCustomise::PacketHouseBeginCustomise(const CClient* target, const CItemMultiCustom* house) : PacketExtended(EXTDATA_HouseCustom, 17, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketHouseBeginCustomise::PacketHouseBeginCustomise");

	writeInt32(house->GetUID());
	writeByte(0x04);
	writeInt16(0x0000);
	writeInt16(0xFFFF);
	writeInt16(0xFFFF);
	writeByte(0xFF);

	push(target);
}

bool PacketHouseBeginCustomise::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_CUSTOMMULTI) || state->isClientKR() || state->isClientEnhanced();
}


/***************************************************************************
 *
 *
 *	Packet 0xBF.0x20.0x05 : PacketHouseEndCustomise	end house customisation (NORMAL)
 *
 *
 ***************************************************************************/
PacketHouseEndCustomise::PacketHouseEndCustomise(const CClient* target, const CItemMultiCustom* house) : PacketExtended(EXTDATA_HouseCustom, 17, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketHouseEndCustomise::PacketHouseEndCustomise");

	writeInt32(house->GetUID());
	writeByte(0x05);
	writeInt16(0x0000);
	writeInt16(0xFFFF);
	writeInt16(0xFFFF);
	writeByte(0xFF);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xBF.0x22 : PacketCombatDamageOld		[old] sends notification of got damage (NORMAL)
 *
 *
 ***************************************************************************/
PacketCombatDamageOld::PacketCombatDamageOld(const CClient* target, byte damage, const CUID &defender) : PacketExtended(EXTDATA_DamagePacketOld, 11, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketCombatDamageOld::PacketCombatDamageOld");

	if ( damage >= UINT8_MAX )
		damage = UINT8_MAX;

	writeByte(0x01);
	writeInt32(defender);
	writeByte(damage);

	push(target);
}

bool PacketCombatDamageOld::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_DAMAGE);
}

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x26 : PacketSpeedMode		set movement speed (HIGH)
 *
 *
 ***************************************************************************/
PacketSpeedMode::PacketSpeedMode(const CClient* target, const byte mode) : PacketExtended(EXTDATA_SpeedMode, 6, g_Cfg.m_fUsePacketPriorities? PRI_HIGH : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketSpeedMode::PacketSpeedMode");

	writeByte(mode);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xC1: PacketMessageLocalised		show localised message to client (NORMAL)
 *
 *
 ***************************************************************************/
PacketMessageLocalised::PacketMessageLocalised(const CClient* target, const int cliloc, const CObjBaseTemplate* source, const HUE_TYPE hue, const TALKMODE_TYPE mode, const FONT_TYPE font, const lpctstr args) : PacketSend(XCMD_SpeakLocalized, 50, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketMessageLocalised::PacketMessageLocalised");

	initLength();

	if (source == nullptr)
		writeInt32(0xFFFFFFFF);
	else
		writeInt32(source->GetUID());

	if (source == nullptr || source->IsChar() == false)
	{
		writeInt16(0xFFFF);
	}
	else
	{
        const auto sourceCharacter = dynamic_cast<const CChar*>(source);
		ASSERT(sourceCharacter);
		writeInt16(static_cast<word>(sourceCharacter->GetDispID()));
	}

	writeByte(static_cast<byte>(mode));
	writeInt16(hue);
	writeInt16(font);
	writeInt32(cliloc);

	if (source == nullptr)
		writeStringFixedASCII("System", 30);
	else
		writeStringFixedASCII(source->GetName(), 30);

	writeStringUTF16(args);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xC8: PacketVisualRange			set the visual range of the client (NORMAL)
 *
 *
 ***************************************************************************/
PacketVisualRange::PacketVisualRange(const CClient* target, const byte range) : PacketSend(XCMD_ViewRange, 2, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketVisualRange::PacketVisualRange");

	writeByte(range);
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xCC: PacketMessageLocalisedEx	show extended localised message to client (NORMAL)
 *
 *
 ***************************************************************************/
PacketMessageLocalisedEx::PacketMessageLocalisedEx(const CClient* target, const int cliloc, const CObjBaseTemplate* source, const HUE_TYPE hue, const TALKMODE_TYPE mode, const FONT_TYPE font, const AFFIX_TYPE affixType, const lpctstr affix, const lpctstr args) : PacketSend(XCMD_SpeakLocalizedEx, 52, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketMessageLocalisedEx::PacketMessageLocalisedEx");

	initLength();

	if (source == nullptr)
		writeInt32(0xFFFFFFFF);
	else
		writeInt32(source->GetUID());

	if (source == nullptr || source->IsChar() == false)
	{
		writeInt16(0xFFFF);
	}
	else
	{
        const auto sourceCharacter = dynamic_cast<const CChar*>(source);
		ASSERT(sourceCharacter);
		writeInt16(static_cast<word>(sourceCharacter->GetDispID()));
	}

	writeByte(static_cast<byte>(mode));
	writeInt16(hue);
	writeInt16(font);
	writeInt32(cliloc);
	writeByte(static_cast<byte>(affixType));

	if (source == nullptr)
		writeStringFixedASCII("System", 30);
	else
		writeStringFixedASCII(source->GetName(), 30);

	writeStringASCII(affix);
	writeStringUTF16(args);

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xD1 : PacketLogoutAck			accept logout char (LOW)
 *
 *
 ***************************************************************************/
PacketLogoutAck::PacketLogoutAck(const CClient* target) : PacketSend(XCMD_LogoutStatus, 2, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketLogoutAck::PacketLogoutAck");

	writeByte(1);
	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xD4 : PacketDisplayBookNew		display book  (LOW)
 *
 *
 ***************************************************************************/
PacketDisplayBookNew::PacketDisplayBookNew(const CClient* target, CItem* book) : PacketSend(XCMD_AOSBookPage, 17, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketDisplayBookNew::PacketDisplayBookNew");

	if ( !book )
		return;

	bool isWritable = false;
	int pages = 0;
	CSString title;
	CSString author;

	if (book->IsBookSystem())
	{
		isWritable = false;

        if (CResourceLock s; g_Cfg.ResourceLock(s, book->m_itBook.m_ResID))
		{
			while (s.ReadKeyParse())
			{
				switch (FindTableSorted(s.GetKey(), CItemMessage::sm_szLoadKeys, std::size(CItemMessage::sm_szLoadKeys) - 1))
				{
					case CIC_AUTHOR:
						author = s.GetArgStr();
						break;
					case CIC_PAGES:
						pages = s.GetArgVal();
						break;
					case CIC_TITLE:
						title = s.GetArgStr();
						break;
				}
			}
		}

		// make sure book is named
		if (title.IsEmpty() == false)
			book->SetName(title);
	}
	else
	{
		// user written book
        if (const auto message = dynamic_cast<const CItemMessage *>(book); message != nullptr)
		{
			isWritable = message->IsBookWritable();
			pages = isWritable ? MAX_BOOK_PAGES : static_cast<int>(message->GetPageCount());
			title = message->GetName();
			author = message->m_sAuthor.IsEmpty()? g_Cfg.GetDefaultMsg(DEFMSG_BOOK_AUTHOR_UNKNOWN) : static_cast<lpctstr>(message->m_sAuthor);
		}
	}


	initLength();
	writeInt32(book->GetUID());
	writeBool(isWritable);
	writeBool(isWritable);
	writeInt16(static_cast<word>(pages));
	writeInt16(static_cast<word>(title.GetLength() + 1));
	writeStringFixedASCII(title.GetBuffer(), title.GetLength() + 1);
	writeInt16(static_cast<word>(author.GetLength() + 1));
	writeStringFixedASCII(author.GetBuffer(), author.GetLength() + 1);

	push(target);
}

bool PacketDisplayBookNew::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_NEWBOOK) || state->isClientKR() || state->isClientEnhanced();
}

/***************************************************************************
 *
 *
 *	Packet 0xD6 : PacketPropertyList		property (tool tip) for objects (IDLE)
 *
 *
 ***************************************************************************/
PacketPropertyList::PacketPropertyList(const CObjBase* object, const dword version, const std::vector<std::unique_ptr<CClientTooltip>>& data) : PacketSend(XCMD_AOSTooltip, 48, PRI_IDLE)
{
	ADDTOCALLSTACK("PacketPropertyList::PacketPropertyList");

	m_time = CWorldGameTime::GetCurrentTime().GetTimeRaw();
	m_object = object->GetUID();
	m_version = version;
	m_entryCount = static_cast<int>(data.size());

	initLength();
	writeInt16(1);
	writeInt32(object->GetUID());
	writeInt16(0);
	writeInt32(version);

	for (int x = 0; x < m_entryCount; ++x)
	{
		const CClientTooltip* tipEntry = data[x].get();
        const size_t tipLength = strlen(tipEntry->m_args);

		writeInt32(tipEntry->m_clilocid);
		writeInt16(static_cast<word>(tipLength * sizeof(wchar)));
		writeStringFixedUTF16(tipEntry->m_args, static_cast<uint>(tipLength));
	}

	writeInt32(0);
}

PacketPropertyList::PacketPropertyList(const CClient* target, const PacketPropertyList* other) : PacketSend(other)
{
	ADDTOCALLSTACK("PacketPropertyList::PacketPropertyList2");

	m_time = CWorldGameTime::GetCurrentTime().GetTimeRaw();
	m_object = other->getObject();
	m_version = other->getVersion();
	m_entryCount = other->getEntryCount();

	push(target, false);
}

bool PacketPropertyList::onSend(const CClient* client)
{
	ADDTOCALLSTACK("PacketPropertyList::onSend");
	if (g_NetworkManager.isOutputThreaded())
		return true;

	const CChar* character = client->GetChar();
	if (!character)
		return false;

	const CObjBase* object = m_object.ObjFind();
    if (const int iCharVisualRange = character->GetVisualRange();
        !object || character->GetTopDistSight(object->GetTopLevelObj()) > maximum(iCharVisualRange, g_Cfg.m_iMapViewSize) && !character->IsPriv(PRIV_ALLSHOW))
		return false;

	if (hasExpired(30 * MSECS_PER_SEC))
		return false;

	return true;
}

bool PacketPropertyList::hasExpired(const int64 iTimeout) const
{
	ADDTOCALLSTACK("PacketPropertyList::hasExpired");
	return (m_time + iTimeout) < CWorldGameTime::GetCurrentTime().GetTimeRaw();
}

bool PacketPropertyList::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_TOOLTIP);
}


/***************************************************************************
 *
 *
 *	Packet 0xD8 : PacketHouseDesign			house design (IDLE)
 *
 *
 ***************************************************************************/
PacketHouseDesign::PacketHouseDesign(const CItemMultiCustom* house, const int revision) : PacketSend(XCMD_AOSCustomHouse, 64, g_Cfg.m_fUsePacketPriorities? PRI_IDLE : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketHouseDesign::PacketHouseDesign");

	m_house = house;

	initLength();

	writeByte(0x03);
	writeByte(0x00);
	writeInt32(house->GetUID());
	writeInt32(revision);
	writeInt16(0); // item count
	writeInt16(0); // data size
	writeByte(0); // plane count

	m_itemCount = 0;
	m_dataSize = 1;
	m_planeCount = 0;
	m_stairPlaneCount = 0;

	m_stairBuffer = new StairData[STAIRSPERBLOCK]{};
	m_stairCount = 0;
}

PacketHouseDesign::PacketHouseDesign(const PacketHouseDesign* other) : PacketSend(other)
{
	ADDTOCALLSTACK("PacketHouseDesign::PacketHouseDesign(2)");

	m_house = other->m_house;
	m_itemCount = other->m_itemCount;
	m_dataSize = other->m_dataSize;
	m_planeCount = other->m_planeCount;
	m_stairPlaneCount = other->m_stairPlaneCount;

	m_stairBuffer = new StairData[STAIRSPERBLOCK];
	memcpy(m_stairBuffer, other->m_stairBuffer, STAIRDATA_BUFFER);
	m_stairCount = other->m_stairCount;
}

PacketHouseDesign::~PacketHouseDesign()
{
	if (m_stairBuffer != nullptr)
	{
		delete[] m_stairBuffer;
		m_stairBuffer = nullptr;
	}
}

bool PacketHouseDesign::writePlaneData(const int plane, const int itemCount, const byte * data, const int dataSize)
{
	ADDTOCALLSTACK("PacketHouseDesign::writePlaneData");

	// compress data
	zlib::uLong compressLength = zlib::compressBound(dataSize);
    const auto compressBuffer = new byte[compressLength];

    if (const int error = zlib::compress2(compressBuffer, &compressLength, data, dataSize, Z_DEFAULT_COMPRESSION); error != Z_OK )
	{
		// an error occured with this floor, but we should be able to continue to the next without problems
		delete[] compressBuffer;
		g_Log.EventError("Compress failed with error %d when generating house design for floor %d on building 0%x.\n", error, plane, static_cast<dword>(m_house->GetUID()));
		return false;
	}
    if (compressLength <= 0 || compressLength >= PLANEDATA_BUFFER)
    {
        // too much data, but we should be able to continue to the next floor without problems
        delete[] compressBuffer;
        g_Log.EventWarn("Floor %d on building 0%x too large with compressed length of %lu.\n", plane, static_cast<dword>(m_house->GetUID()), compressLength);
        return false;
    }

    writeByte(static_cast<byte>(plane | 0x20));
	writeByte(static_cast<byte>(dataSize));
	writeByte(static_cast<byte>(compressLength));
	writeByte(((dataSize >> 4) & 0xF0) | ((compressLength >> 8) & 0x0F));
	writeData(compressBuffer, compressLength);
	delete[] compressBuffer;

	m_planeCount += 1;
	m_itemCount += itemCount;
	m_dataSize += (4 + compressLength);
	return true;
}

bool PacketHouseDesign::writeStairData(const ITEMID_TYPE id, const int x, const int y, const int z)
{
	ADDTOCALLSTACK("PacketHouseDesign::writeStairData");

	m_stairBuffer[m_stairCount].m_id = static_cast<word>(id);
	m_stairBuffer[m_stairCount].m_x = static_cast<byte>(x);
	m_stairBuffer[m_stairCount].m_y = static_cast<byte>(y);
	m_stairBuffer[m_stairCount].m_z = static_cast<byte>(z);
	m_stairCount += 1;

	if (m_stairCount >= STAIRSPERBLOCK)
		flushStairData();

	return true;
}

void PacketHouseDesign::flushStairData()
{
	ADDTOCALLSTACK("PacketHouseDesign::flushStairData");

	if (m_stairCount <= 0)
		return;

    const int stairCount = maximum(0, m_stairCount);
    const uint stairSize = static_cast<uint>(stairCount) * static_cast<uint>(sizeof(StairData));

	m_stairCount = 0;

	// compress data
	zlib::uLong compressLength = zlib::compressBound(stairSize);
    const auto compressBuffer = new byte[compressLength];

    if (const int error = zlib::compress2(compressBuffer, &compressLength, reinterpret_cast<byte *>(m_stairBuffer), stairSize, Z_DEFAULT_COMPRESSION); error != Z_OK )
	{
		// an error occured with this block, but we should be able to continue to the next without problems
		delete[] compressBuffer;
		g_Log.EventError("Compress failed with error %d when generating house design on building 0%x.\n", error, static_cast<dword>(m_house->GetUID()));
		return;
	}
    if (compressLength <= 0 || compressLength >= STAIRDATA_BUFFER)
    {
        // too much data, but we should be able to continue to the next block without problems
        delete[] compressBuffer;
        g_Log.EventWarn("Building 0%x too large with compressed length of %lu.\n", static_cast<dword>(m_house->GetUID()), compressLength);
        return;
    }

    writeByte(static_cast<byte>(9 + m_stairPlaneCount));
	writeByte(static_cast<byte>(stairSize));
	writeByte(static_cast<byte>(compressLength));
	writeByte(((stairSize >> 4) & 0xF0) | ((compressLength >> 8) & 0x0F));
	writeData(compressBuffer, compressLength);
	delete[] compressBuffer;

	m_stairPlaneCount++;
	m_itemCount += stairCount;
	m_dataSize += (4 + compressLength);
}

void PacketHouseDesign::finalise()
{
	ADDTOCALLSTACK("PacketHouseDesign::finalise");

	flushStairData();

    const uint endPosition(getPosition());

	seek(13);
	writeInt16(static_cast<word>(m_itemCount));
	writeInt16(static_cast<word>(m_dataSize));
	writeByte(static_cast<byte>(m_planeCount + m_stairPlaneCount));

	seek(endPosition);
}

bool PacketHouseDesign::CanSendToClient(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_CUSTOMMULTI) || state->isClientKR() || state->isClientEnhanced();
}


/***************************************************************************
 *
 *
 *	Packet 0xDC : PacketPropertyListVersion		property (tool tip) version (LOW)
 *
 *
 ***************************************************************************/
PacketPropertyListVersion::PacketPropertyListVersion(const CClient* target, const CObjBase* object, const dword version) : PacketSend(XCMD_AOSTooltipInfo, 9, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketPropertyListVersion::PacketPropertyListVersion");

	m_object = object->GetUID();

	writeInt32(m_object);
	writeInt32(version);

	if (target != nullptr)
		push(target);
}

bool PacketPropertyListVersion::onSend(const CClient* client)
{
	ADDTOCALLSTACK("PacketPropertyList::onSend");
	if (g_NetworkManager.isOutputThreaded())
		return true;

	const CChar* character = client->GetChar();
	if (character == nullptr)
		return false;

	const CObjBase* object = m_object.ObjFind();
    if (const int iCharVisualRange = character->GetVisualRange();
        object == nullptr || character->GetTopDistSight(object->GetTopLevelObj()) > maximum(iCharVisualRange, g_Cfg.m_iMapViewSize))
		return false;

	return true;
}

bool PacketPropertyListVersion::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_TOOLTIPHASH);
}


/***************************************************************************
 *
 *
 *	Packet 0xDF : PacketBuff				add/remove buff icon (LOW)
 *
 *
 ***************************************************************************/
PacketBuff::PacketBuff(const CClient* target, const BUFF_ICONS iconId, const dword clilocOne, const dword clilocTwo, const word durationSeconds, const lpctstr * args, const uint argCount) : PacketSend(XCMD_BuffPacket, 72, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketBuff::PacketBuff");
	// At date of 04/2015 RUOSI seems to have a different structure than the one we have with one more argument and different order... however this one seems to keep working: http://ruosi.org/packetguide/index.xml#serverDF

	const CChar* character = target->GetChar();
	ASSERT(character != nullptr);

	initLength();

	writeInt32(character->GetUID());
	writeInt16(static_cast<word>(iconId));
	writeInt16(0x1);	// show

	writeInt32(0);
	writeInt16(static_cast<word>(iconId));
	writeInt16(0x1);	// show

	writeInt32(0);
	writeInt16(durationSeconds);	//simple countdown without automatic remove
	writeInt16(0);
	writeByte(0);

	writeInt32(clilocOne);
	writeInt32(clilocTwo);

	if ( argCount )
	{
		writeInt32(0);
		writeInt16(0x1);		// show icon
		writeInt16(0);

		for (uint i = 0; i < argCount; ++i)
		{
			writeCharUTF16('\t');
			writeStringUTF16(args[i], false);
		}
		writeCharUTF16('\t');
		writeCharUTF16('\0');

		writeInt16(0x1);
		writeInt16(0);
	}
	else
	{
		// Original code - it leaves empty clilocTwo exactly as it is
		//writeInt32(0);
		//writeInt32(0);
		//writeInt16(0);

		// Workaround - it fills empty clilocTwo with an whitespace just to make it show ' ' instead '~1_SOMETHING~'
		// This is a Sphere custom behavior, since it uses ~1_NOTHING~ clilocs which are not really used on OSI
		writeInt32(0);
		writeInt16(0x1);
		writeInt16(0);

		writeStringUTF16("\t ", true);
	}
	push(target);
}

PacketBuff::PacketBuff(const CClient* target, const BUFF_ICONS iconId) : PacketSend(XCMD_BuffPacket, 15, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketBuff::PacketBuff(2)");

	const CChar* character = target->GetChar();
	ASSERT(character != nullptr);

	initLength();

	writeInt32(character->GetUID());
	writeInt16(static_cast<word>(iconId));
	writeInt16(0);		// hide icon

	push(target);
}

bool PacketBuff::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_BUFFS);
}

/***************************************************************************
 *
 *
 *	Packet 0xE3 : PacketKREncryption		Sends encryption data to KR client
 *
 *
 ***************************************************************************/
PacketKREncryption::PacketKREncryption(const CClient* target) : PacketSend(XCMD_EncryptionReq, 77, g_Cfg.m_fUsePacketPriorities? PRI_HIGH : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketKREncryption::PacketKREncryption");

	static byte pDataKR_E3[76] = {
						0x00, 0x4d,
						0x00, 0x00, 0x00, 0x03, 0x02, 0x00, 0x03,
						0x00, 0x00, 0x00, 0x13, 0x02, 0x11, 0x00, 0x00, 0x2f, 0xe3, 0x81, 0x93, 0xcb, 0xaf, 0x98, 0xdd, 0x83, 0x13, 0xd2, 0x9e, 0xea, 0xe4, 0x13,
						0x00, 0x00, 0x00, 0x10, 0x00, 0x13, 0xb7, 0x00, 0xce, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00, 0x00, 0x20,
						0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	writeData(pDataKR_E3, sizeof(pDataKR_E3));
	push(target);
}

/***************************************************************************
*
*
*	Packet 0xE5 : PacketWaypointAdd			Add waypoint on KR/SA radar map
*
*
***************************************************************************/
PacketWaypointAdd::PacketWaypointAdd(const CClient *target, const CObjBase *object, const MAPWAYPOINT_TYPE type) : PacketSend(XCMD_WaypointShow, 25, g_Cfg.m_fUsePacketPriorities ? PRI_LOW : PRI_NORMAL)
{
    ADDTOCALLSTACK("PacketWaypointAdd::PacketWaypointAdd");

    if (!object)
        return;

    const CPointMap pt = object->GetTopPoint();
    const dword cliloc = (type == MAPWAYPOINT_Corpse) ? 1028198 : 1062613;	// corpse : "~1_NAME~"

    initLength();
    writeInt32(object->GetUID());

    writeInt16(pt.m_x);
    writeInt16(pt.m_y);
    writeByte(pt.m_z);
    writeByte(pt.m_map);

    writeInt16(static_cast<word>(type));
    writeInt16(0);

    writeInt32(cliloc);
    writeStringUTF16(object->GetName());

    push(target);
}

bool PacketWaypointAdd::CanSendTo(const CNetState *state) // static
{
    return state->isClientVersionNumber(MINCLIVER_MAPWAYPOINT) || state->isClientKR() || state->isClientEnhanced();
}

/***************************************************************************
*
*
*	Packet 0xE6 : PacketWaypointRemove		Remove waypoint on KR/SA radar map
*
*
***************************************************************************/
PacketWaypointRemove::PacketWaypointRemove(const CClient *target, const CObjBase *object) : PacketSend(XCMD_WaypointHide, 5, g_Cfg.m_fUsePacketPriorities ? PRI_LOW : PRI_NORMAL)
{
    ADDTOCALLSTACK("PacketWaypointRemove::PacketWaypointRemove");

    if (!object)
        return;

    writeInt32(object->GetUID());

    push(target);
}

bool PacketWaypointRemove::CanSendTo(const CNetState *state) // static
{
    return state->isClientVersionNumber(MINCLIVER_MAPWAYPOINT) || state->isClientKR() || state->isClientEnhanced();
}

/***************************************************************************
 *
 *
 *	Packet 0xEA : PacketToggleHotbar		toggle kr hotbar (NORMAL)
 *
 *
 ***************************************************************************/
PacketToggleHotbar::PacketToggleHotbar(const CClient* target, const bool enable) : PacketSend(XCMD_ToggleHotbar, 3, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketToggleHotbar::PacketToggleHotbar");

	writeInt16(enable? 0x01 : 0x00);

	push(target);
}

bool PacketToggleHotbar::CanSendTo(const CNetState* state) // static
{
    return state->isClientKR();
}


/***************************************************************************
 *
 *
 *	Packet 0xF2 : PacketTimeSyncResponse	time sync response (HIGH)
 *
 *
 ***************************************************************************/
PacketTimeSyncResponse::PacketTimeSyncResponse(const CClient* target) : PacketSend(XCMD_TimeSyncResponse, 25, PRI_HIGH)
{
	ADDTOCALLSTACK("PacketTimeSyncResponse::PacketTimeSyncResponse");
	// A single tick here represents one hundred nanoseconds (the "standard" Windows tick time) or one ten-millionth of a second. There are 10,000 ticks in a millisecond.
	// From Wyatt packet guide: DateTime1, DateTime2 and DateTime3 - ticks from Unix time divided by 10,000 --> so, milliseconds

	//const int64 llTime = CWorldGameTime::GetCurrentTime().GetTimeRaw();
	const int64 llTime = CSTime::GetCurrentTime().GetTime()	// Seconds from the UNIX epoch.
						 * MSECS_PER_SEC;

	writeInt64(llTime);
	writeInt64(llTime);
	writeInt64(llTime);

	push(target);
}

bool PacketTimeSyncResponse::CanSendTo(const CNetState* state) //static
{
    return state->isClientVersionNumber(MINCLIVER_SA) || state->isClientEnhanced() || state->isClientKR();
}


/***************************************************************************
 *
 *
 *	Packet 0xF3 : PacketItemWorldNew		sends item on ground (NORMAL)
 *
 *
 ***************************************************************************/
PacketItemWorldNew::PacketItemWorldNew(const byte id, const uint size, const CUID& uid) : PacketItemWorld(id, size, uid)
{
}

PacketItemWorldNew::PacketItemWorldNew(const CClient* target, const CItem *item) : PacketItemWorld(XCMD_PutNew, 26, item->GetUID())
{
	ADDTOCALLSTACK("PacketItemWorldNew::PacketItemWorldNew");

    const CNetState *ns = target->GetNetState();
	DataSource source;		// 0=Tiledata, 1=Character, 2=Multi
    const dword uid = item->GetUID();
	ITEMID_TYPE id = item->GetDispID();
	DIR_TYPE dir = DIR_N;
    word amount = 0;
    if (item->GetType() == IT_CORPSE)
    {
        amount = item->GetAmount();  // char id of the corpse
    }
    else if (item->CanSendAmount())
    {
        if (const word itemAmount = item->GetAmount(); itemAmount > 1)
            amount = itemAmount;
    }
    const CPointMap pt = item->GetTopPoint();
	HUE_TYPE hue = item->GetHueVisible();
	byte light = 0;
	byte flags = 0;

	adjustItemData(target, item, id, hue, amount, dir, flags, light);

	if ( id >= ITEMID_MULTI )
	{
		source = Multi;
		id = static_cast<ITEMID_TYPE>(id & 0x3FFF);
	}
	else
	{
		source = (item->Can(CAN_I_DAMAGEABLE) && ns->isClientVersionNumber(MINCLIVER_STATUS_V6)) ? Damageable : TileData;
		id = static_cast<ITEMID_TYPE>(id & 0xFFFF);
	}

	writeInt16(1);
	writeByte(static_cast<byte>(source));
	writeInt32(uid);
	writeInt16(static_cast<word>(id));
	writeByte(static_cast<byte>(dir));
	writeInt16(amount);
	writeInt16(amount);
	writeInt16(pt.m_x & 0x7FFF);
	writeInt16(pt.m_y & 0x3FFF);
	writeByte(pt.m_z);
	writeByte(light);	// if the item hasn't the LightSource flag in the tiledata, sending this will have no effect
	writeInt16(hue);
	writeByte(flags);

	if ( ns->isClientVersionNumber(MINCLIVER_HS) )
		writeInt16(0);		// 0 = World Item, 1 = Player Item (why should a item on the ground be defined as player item? and what is the difference?)

	trim();
	push(target);
}

PacketItemWorldNew::PacketItemWorldNew(const CClient* target, const CChar* mobile) : PacketItemWorld(XCMD_PutNew, 26, mobile->GetUID())
{
    constexpr DataSource source = Character;
    const dword uid = mobile->GetUID();
    const CREID_TYPE id = mobile->GetDispID();
    const CPointMap p = mobile->GetTopPoint();
    const byte dir = static_cast<byte>(mobile->m_dirFace);
    const HUE_TYPE hue = mobile->GetHue();

	writeInt16(1);
	writeByte(source);
	writeInt32(uid);
	writeInt16(static_cast<word>(id));
	writeByte(dir);
	writeInt16(1);
	writeInt16(1);
	writeInt16(p.m_x);
	writeInt16(p.m_y);
	writeByte(p.m_z);
	writeByte(0);
	writeInt16(hue);
	writeByte(0);

	if (target->GetNetState()->isClientVersionNumber(MINCLIVER_HS))
		writeInt16(0);

	trim();
	push(target);
}

bool PacketItemWorldNew::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_SA) || state->isClientEnhanced();
}

/***************************************************************************
 *
 *
 *	Packet 0xF5 : PacketDisplayMapNew		display map (LOW)
 *
 *
 ***************************************************************************/
PacketDisplayMapNew::PacketDisplayMapNew(const CClient* target, const CItemMap* map, const CRectMap& rect) : PacketSend(XCMD_MapDisplayNew, 21, g_Cfg.m_fUsePacketPriorities? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketDisplayMapNew::PacketDisplayMapNew");

	const CItemBase* itemDef = map->Item_GetDef();
	ASSERT(itemDef != nullptr);

	word width	= static_cast<word>(itemDef->m_ttMap.m_iGumpWidth > 0 ? itemDef->m_ttMap.m_iGumpWidth : static_cast<word>(CItemMap::DEFAULT_SIZE));
	word height = static_cast<word>(itemDef->m_ttMap.m_iGumpHeight > 0 ? itemDef->m_ttMap.m_iGumpHeight : static_cast<word>(CItemMap::DEFAULT_SIZE));

    if (const word overrideWidth = static_cast<word>(map->GetKeyNum("OVERRIDE.MAPWIDTH", true)); overrideWidth > 0)
		width = overrideWidth;

    if (const word overrideHeight = static_cast<word>(map->GetKeyNum("OVERRIDE.MAPHEIGHT", true)); overrideHeight > 0)
		height = overrideHeight;

	writeInt32(map->GetUID());
	writeInt16(GUMP_MAP_2_NORTH);
	writeInt16(static_cast<word>(rect.m_left));
	writeInt16(static_cast<word>(rect.m_top));
	writeInt16(static_cast<word>(rect.m_right));
	writeInt16(static_cast<word>(rect.m_bottom));
	writeInt16(height); //the packet guide lists the width as the value sent before heigth, but in game the values are actually inverted.
	writeInt16(width);
	writeInt16(static_cast<word>(rect.m_map));

	push(target);
}

bool PacketDisplayMapNew::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_NEWMAPDISPLAY) || state->isClientEnhanced();
}


/***************************************************************************
 *
 *
 *	Packet 0xF6 : PacketMoveShip			move ship (NORMAL)
 *
 *
 ***************************************************************************/
PacketMoveShip::PacketMoveShip(const CClient* target, const CObjBase* movingObj, CObjBase** objects, const uint objectCount, const byte movedirection, const byte boatdirection, const byte speed) : PacketSend(XCMD_MoveShip, 18, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketMoveShip::PacketMoveShip");
	ASSERT(objectCount > 0);
	const CPointMap& shipLocation = movingObj->GetTopPoint();

	initLength();
	writeInt32(movingObj->GetUID());
	writeByte(speed);
	writeByte(movedirection);
	writeByte(boatdirection);
	writeInt16(shipLocation.m_x);
	writeInt16(shipLocation.m_y);
	writeInt16(shipLocation.m_z);

	// assume that first object is the ship itself
	writeInt16(static_cast<word>(objectCount - 1));

	for (uint i = 1; i < objectCount; ++i)
	{
		const CObjBase* object = objects[i];
		const CPointMap& objectLocation = object->GetTopPoint();

		writeInt32(object->GetUID());
		writeInt16(objectLocation.m_x);
		writeInt16(objectLocation.m_y);
		writeInt16(objectLocation.m_z);
	}

	push(target);
}


/***************************************************************************
 *
 *
 *	Packet 0xF7 : PacketContainer			multiple packets (NORMAL)
 *
 *
 ***************************************************************************/
PacketContainer::PacketContainer(const CClient* target, CObjBase** objects, const uint objectCount) : PacketSend(XCMD_PacketCont, 5, PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketContainer::PacketContainer");
	ASSERT(objectCount > 0);

	using ds = PacketItemWorldNew::DataSource;

	initLength();
	writeInt16(static_cast<word>(objectCount));

	for (uint i = 0; i < objectCount; ++i)
	{
        if (CObjBase *object = objects[i]; object->IsItem())
		{
            const auto item = static_cast<CItem*>(object);
			ds source = ds::TileData;
            const dword uid = item->GetUID();
			word amount = item->GetAmount();
			ITEMID_TYPE id = item->GetDispID();
			const CPointMap& p = item->GetTopPoint();
			DIR_TYPE dir = DIR_N;
			HUE_TYPE hue = item->GetHue();
			byte flags = 0;
			byte light = 0;

			PacketItemWorld::adjustItemData(target, item, id, hue, amount, dir, flags, light);

			if (id >= ITEMID_MULTI)
				id = static_cast<ITEMID_TYPE>(id - ITEMID_MULTI);

			if (item->IsTypeMulti())
				source = ds::Multi;

			writeByte(0xF3);
			writeInt16(1);
			writeByte(static_cast<byte>(source));
			writeInt32(uid);
			writeInt16(static_cast<word>(id));
			writeByte(static_cast<byte>(dir));
			writeInt16(amount);
			writeInt16(amount);
			writeInt16(p.m_x);
			writeInt16(p.m_y);
			writeByte(p.m_z);
			writeByte(light);
			writeInt16(hue);
			writeByte(flags);
			writeInt16(0);
		}
		else
		{
            const CChar * mobile = dynamic_cast<CChar*>(object);
            constexpr ds source = ds::Character;
            const dword uid = mobile->GetUID();
            const CREID_TYPE id = mobile->GetDispID();
            const CPointMap p = mobile->GetTopPoint();
            const byte dir = static_cast<byte>(mobile->m_dirFace);
            const HUE_TYPE hue = mobile->GetHue();

			writeByte(0xF3);
			writeInt16(1);
			writeByte(source);
			writeInt32(uid);
			writeInt16(static_cast<word>(id));
			writeByte(dir);
			writeInt16(1);
			writeInt16(1);
			writeInt16(p.m_x);
			writeInt16(p.m_y);
			writeByte(p.m_z);
			writeByte(0);
			writeInt16(hue);
			writeByte(0);
			writeInt16(0);
		}
	}

	push(target);
}

bool PacketContainer::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_HS);
}


/***************************************************************************
 *
 *
 *	Packet 0xF9 : PacketGlobalChat			global chat (LOW) (INCOMPLETE)
 *
 *
 ***************************************************************************/
PacketGlobalChat::PacketGlobalChat(const CClient* target, const byte unknown, const byte action, const byte stanza, const lpctstr xml) : PacketSend(XCMD_GlobalChat, 4, g_Cfg.m_fUsePacketPriorities ? PRI_LOW : PRI_NORMAL)
{
	ADDTOCALLSTACK("PacketGlobalChat::PacketGlobalChat");

	tchar* xmlFull = Str_GetTemp();
	sprintf(xmlFull, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\" ?><ultima_stanza>%s</ultima_stanza>", xml);
	//DEBUG_ERR(("GlobalChat XML sent: %s\n", xmlFull));

	writeByte(unknown);
	writeByte(action);
	writeByte(stanza);
	writeStringASCII(xmlFull);

	trim();
	push(target);
}

bool PacketGlobalChat::CanSendTo(const CNetState* state) // static
{
    return state->isClientVersionNumber(MINCLIVER_GLOBALCHAT);
}
