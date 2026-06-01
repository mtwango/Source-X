/**
* @file receive.h
* @brief Inwards packets.
*/

#ifndef _INC_RECEIVE_H
#define _INC_RECEIVE_H

#include "../common/CUID.h"
#include "../common/sphereproto.h"
#include "../game/uo_files/uofiles_enums.h"
#include "../game/uo_files/uofiles_enums_itemid.h"
#include "../game/uo_files/uofiles_types.h"
#include "packet.h"


/***************************************************************************
 *
 *
 *	Packet ???? : PacketUnknown						unknown or unhandled packet
 *
 *
 ***************************************************************************/
class PacketUnknown : public Packet
{
public:
	PacketUnknown(uint size = 0);
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x00 : PacketCreate						create new character request
 *
 *
 ***************************************************************************/
class PacketCreate : public Packet
{
public:
	PacketCreate(uint size = 104);
    bool onReceive(CNetState* net) override;

protected:
    // TODO: pack this abomination of arguments list in a struct
	bool doCreate(const CNetState * net, lpctstr charname, bool bFemale, RACE_TYPE rtRace, ushort wStr, ushort wDex, ushort wInt, PROFESSION_TYPE prProf,
		SKILL_TYPE skSkill1, ushort uiSkillVal1, SKILL_TYPE skSkill2, ushort uiSkillVal2, SKILL_TYPE skSkill3, ushort uiSkillVal3, SKILL_TYPE skSkill4, ushort uiSkillVal4,
		HUE_TYPE wSkinHue, ITEMID_TYPE idHair, HUE_TYPE wHairHue, ITEMID_TYPE idBeard, HUE_TYPE wBeardHue, HUE_TYPE wShirtHue, HUE_TYPE wPantsHue, ITEMID_TYPE idFace,
		int iStartLoc, uint uiFlags);
};

/***************************************************************************
 *
 *
 *	Packet 0x02 : PacketMovementReq					movement request
 *
 *
 ***************************************************************************/
class PacketMovementReq : public Packet
{
public:
	PacketMovementReq(uint size = 7);
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x03 : PacketSpeakReq					character talking
 *
 *
 ***************************************************************************/
class PacketSpeakReq : public Packet
{
public:
	PacketSpeakReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x05 : PacketAttackReq					attack request
 *
 *
 ***************************************************************************/
class PacketAttackReq : public Packet
{
public:
	PacketAttackReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x06 : PacketDoubleClick					double click object
 *
 *
 ***************************************************************************/
class PacketDoubleClick : public Packet
{
public:
	PacketDoubleClick();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x07 : PacketItemPickupReq				pick up item request
 *
 *
 ***************************************************************************/
class PacketItemPickupReq : public Packet
{
public:
	PacketItemPickupReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x08 : PacketItemDropReq					drop item request
 *
 *
 ***************************************************************************/
class PacketItemDropReq : public Packet
{
public:
	PacketItemDropReq();
    uint getExpectedLength(CNetState* client, Packet* packet) override;
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x09 : PacketSingleClick					single click object
 *
 *
 ***************************************************************************/
class PacketSingleClick : public Packet
{
public:
	PacketSingleClick();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x12 : PacketTextCommand					text command
 *
 *
 ***************************************************************************/
class PacketTextCommand : public Packet
{
public:
	PacketTextCommand();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x13 : PacketItemEquipReq				item equip request
 *
 *
 ***************************************************************************/
class PacketItemEquipReq : public Packet
{
public:
	PacketItemEquipReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x22 : PacketResynchronize				resend all request
 *
 *
 ***************************************************************************/
class PacketResynchronize : public Packet
{
public:
	PacketResynchronize();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x2c : PacketDeathStatus					death status
 *
 *
 ***************************************************************************/
class PacketDeathStatus : public Packet
{
public:
    enum Mode
	{
		Dead = 0x00,        // Old "server sent"
        Resurrect = 0x1,    // Sent by the client
		Alive = 0x02        // Sent by the client
	};

	PacketDeathStatus();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x34 : PacketObjStatusReq				request information on the object
 *
 *
 ***************************************************************************/
class PacketObjStatusReq : public Packet
{
public:
	PacketObjStatusReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x3A : PacketSkillLockChange				change skill locks
 *
 *
 ***************************************************************************/
class PacketSkillLockChange : public Packet
{
public:
	PacketSkillLockChange();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x3B : PacketVendorBuyReq				buy item from vendor
 *
 *
 ***************************************************************************/
struct VendorItem
{
	CUID m_serial;
	word m_vcAmount;
	dword m_price;
};

class PacketVendorBuyReq : public Packet
{
public:
	PacketVendorBuyReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x3F : PacketStaticUpdate			Ultima live and (God Client?)
 *
 *
 ***************************************************************************/

class PacketStaticUpdate : public Packet
{
public:
	PacketStaticUpdate();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x56 : PacketMapEdit						edit map pins
 *
 *
 ***************************************************************************/
class PacketMapEdit : public Packet
{
public:
	PacketMapEdit();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x5D : PacketCharPlay					character select
 *
 *
 ***************************************************************************/
class PacketCharPlay : public Packet
{
public:
	PacketCharPlay();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x66 : PacketBookPageEdit				edit book page
 *
 *
 ***************************************************************************/
class PacketBookPageEdit : public Packet
{
public:
	PacketBookPageEdit();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x6C : PacketTarget						target object
 *
 *
 ***************************************************************************/
class PacketTarget : public Packet
{
public:
	PacketTarget();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x6F : PacketSecureTradeReq				trade with another character
 *
 *
 ***************************************************************************/
class PacketSecureTradeReq : public Packet
{
public:
	PacketSecureTradeReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x71 : PacketBulletinBoardReq			request bulletin board
 *
 *
 ***************************************************************************/
class PacketBulletinBoardReq : public Packet
{
public:
	PacketBulletinBoardReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x72 : PacketWarModeReq					toggle war mode
 *
 *
 ***************************************************************************/
class PacketWarModeReq : public Packet
{
public:
	PacketWarModeReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x73 : PacketPingReq						ping requests
 *
 *
 ***************************************************************************/
class PacketPingReq : public Packet
{
public:
	PacketPingReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x75 : PacketCharRename					rename character/pet
 *
 *
 ***************************************************************************/
class PacketCharRename : public Packet
{
public:
	PacketCharRename();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x7D : PacketMenuChoice					select menu option
 *
 *
 ***************************************************************************/
class PacketMenuChoice : public Packet
{
public:
	PacketMenuChoice();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x80 : PacketServersReq					request server list
 *
 *
 ***************************************************************************/
class PacketServersReq : public Packet
{
public:
	PacketServersReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x83 : PacketCharDelete					delete character
 *
 *
 ***************************************************************************/
class PacketCharDelete : public Packet
{
public:
	PacketCharDelete();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x8D : PacketCreateNew					create new character request (KR/SA Enhanced Client)
 *
 *
 ***************************************************************************/
class PacketCreateNew : public PacketCreate
{
public:
	PacketCreateNew();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x91 : PacketCharListReq					request character list
 *
 *
 ***************************************************************************/
class PacketCharListReq : public Packet
{
public:
	PacketCharListReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x93 : PacketBookHeaderEdit				edit book header (title/author)
 *
 *
 ***************************************************************************/
class PacketBookHeaderEdit : public Packet
{
public:
	PacketBookHeaderEdit();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x95 : PacketDyeObject					colour selection dialog
 *
 *
 ***************************************************************************/
class PacketDyeObject : public Packet
{
public:
	PacketDyeObject();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x98 : PacketAllNamesReq					all names command (ctrl+shift)
 *
 *
 ***************************************************************************/
class PacketAllNamesReq : public Packet
{
public:
	PacketAllNamesReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x9A : PacketPromptResponse				prompt response (ascii)
 *
 *
 ***************************************************************************/
class PacketPromptResponse : public Packet
{
public:
	PacketPromptResponse();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x9B : PacketHelpPageReq					GM help page request
 *
 *
 ***************************************************************************/
class PacketHelpPageReq : public Packet
{
public:
	PacketHelpPageReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0x9F : PacketVendorSellReq				sell item to vendor
 *
 *
 ***************************************************************************/
class PacketVendorSellReq : public Packet
{
public:
	PacketVendorSellReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xA0 : PacketServerSelect				select server
 *
 *
 ***************************************************************************/
class PacketServerSelect : public Packet
{
public:
	PacketServerSelect();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xA4 : PacketSystemInfo					system info from client
 *
 *
 ***************************************************************************/
class PacketSystemInfo : public Packet
{
public:
	PacketSystemInfo();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xA7 : PacketTipReq						tip request
 *
 *
 ***************************************************************************/
class PacketTipReq : public Packet
{
public:
	PacketTipReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xAC : PacketGumpValueInputResponse		gump text input
 *
 *
 ***************************************************************************/
class PacketGumpValueInputResponse : public Packet
{
public:
	PacketGumpValueInputResponse();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xAD : PacketSpeakReqUNICODE				character talking (unicode)
 *
 *
 ***************************************************************************/
class PacketSpeakReqUNICODE : public Packet
{
public:
	PacketSpeakReqUNICODE();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xB1 : PacketGumpDialogRet				dialog button pressed
 *
 *
 ***************************************************************************/
class PacketGumpDialogRet : public Packet
{
public:
	PacketGumpDialogRet();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xB3 : PacketChatCommand					chat command
 *
 *
 ***************************************************************************/
class PacketChatCommand : public Packet
{
public:
	PacketChatCommand();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xB5 : PacketChatButton					chat button pressed
 *
 *
 ***************************************************************************/
class PacketChatButton : public Packet
{
public:
	PacketChatButton();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xB6 : PacketToolTipReq					tooltip requested
 *
 *
 ***************************************************************************/
class PacketToolTipReq : public Packet
{
public:
	PacketToolTipReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xB8 : PacketProfileReq					character profile requested
 *
 *
 ***************************************************************************/
class PacketProfileReq : public Packet
{
public:
	PacketProfileReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBB : PacketMailMessage					send mail message
 *
 *
 ***************************************************************************/
class PacketMailMessage : public Packet
{
public:
	PacketMailMessage();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBD : PacketClientVersion				client version
 *
 *
 ***************************************************************************/
class PacketClientVersion : public Packet
{
public:
	PacketClientVersion();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBE : PacketAssistVersion				assist version
 *
 *
 ***************************************************************************/
class PacketAssistVersion : public Packet
{
public:
    PacketAssistVersion();
    ~PacketAssistVersion() override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF : PacketExtendedCommand				extended command
 *
 *
 ***************************************************************************/
class PacketExtendedCommand : public Packet
{
public:
	PacketExtendedCommand();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x05 : PacketScreenSize				screen size report
 *
 *
 ***************************************************************************/
class PacketScreenSize : public Packet
{
public:
	PacketScreenSize();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x06 : PacketPartyMessage			party message
 *
 *
 ***************************************************************************/
class PacketPartyMessage : public Packet
{
public:
	PacketPartyMessage();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x07 : PacketArrowClick				click quest arrow
 *
 *
 ***************************************************************************/
class PacketArrowClick : public Packet
{
public:
	PacketArrowClick();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x09 : PacketWrestleDisarm			wrestle disarm macro
 *
 *
 ***************************************************************************/
class PacketWrestleDisarm : public Packet
{
public:
	PacketWrestleDisarm();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x0A : PacketWrestleStun			wrestle stun macro
 *
 *
 ***************************************************************************/
class PacketWrestleStun : public Packet
{
public:
	PacketWrestleStun();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x0B : PacketLanguage				language report
 *
 *
 ***************************************************************************/
class PacketLanguage : public Packet
{
public:
	PacketLanguage();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x0C : PacketStatusClose			status window closed
 *
 *
 ***************************************************************************/
class PacketStatusClose : public Packet
{
public:
	PacketStatusClose();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x0E : PacketAnimationReq			play an animation
 *
 *
 ***************************************************************************/
class PacketAnimationReq : public Packet
{
public:
	PacketAnimationReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x0F : PacketClientInfo				client information
 *
 *
 ***************************************************************************/
class PacketClientInfo : public Packet
{
public:
	PacketClientInfo();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x10 : PacketAosTooltipInfo			tooltip request (old)
 *
 *
 ***************************************************************************/
class PacketAosTooltipInfo : public Packet
{
public:
	PacketAosTooltipInfo();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x13 : PacketPopupReq				request popup menu
 *
 *
 ***************************************************************************/
class PacketPopupReq : public Packet
{
public:
	PacketPopupReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x15 : PacketPopupSelect			popup menu option selected
 *
 *
 ***************************************************************************/
class PacketPopupSelect : public Packet
{
public:
	PacketPopupSelect();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x1A : PacketChangeStatLock			set stat locks
 *
 *
 ***************************************************************************/
class PacketChangeStatLock : public Packet
{
public:
	PacketChangeStatLock();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x1C : PacketSpellSelect			select/cast spell
 *
 *
 ***************************************************************************/
class PacketSpellSelect : public Packet
{
public:
	PacketSpellSelect();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x1E : PacketHouseDesignReq			house design request
 *
 *
 ***************************************************************************/
class PacketHouseDesignReq : public Packet
{
public:
	PacketHouseDesignReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x24 : PacketAntiCheat				anti-cheat (unknown)
 *
 *
 ***************************************************************************/
class PacketAntiCheat : public Packet
{
public:
	PacketAntiCheat();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x2C : PacketBandageMacro			bandage macro
 *
 *
 ***************************************************************************/
class PacketBandageMacro : public Packet
{
public:
	PacketBandageMacro();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
*
*
*	Packet 0xBF.0x2E : PacketTargetedSkill			bandage macro
*
*
***************************************************************************/
class PacketTargetedSkill : public Packet
{
public:
    PacketTargetedSkill();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x32 : PacketGargoyleFly			gargoyle toggle flying
 *
 *
 ***************************************************************************/
class PacketGargoyleFly : public Packet
{
public:
	PacketGargoyleFly();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xBF.0x33 : PacketWheelBoatMove			wheel boat movement
 *
 *
 ***************************************************************************/
class PacketWheelBoatMove : public Packet
{
public:
	PacketWheelBoatMove();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xC2 : PacketPromptResponseUnicode		prompt response (unicode)
 *
 *
 ***************************************************************************/
class PacketPromptResponseUnicode : public Packet
{
public:
	PacketPromptResponseUnicode();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xC8 : PacketViewRange					change view range
 *
 *
 ***************************************************************************/
class PacketViewRange : public Packet
{
public:
	PacketViewRange();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD1 : PacketLogout						client logout notification
 *
 *
 ***************************************************************************/
class PacketLogout : public Packet
{
public:
	PacketLogout();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD4 : PacketBookHeaderEditNew		edit book header (title/author)
 *
 *
 ***************************************************************************/
class PacketBookHeaderEditNew : public Packet
{
public:
	PacketBookHeaderEditNew();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD6 : PacketAOSTooltipReq				aos tooltip request
 *
 *
 ***************************************************************************/
class PacketAOSTooltipReq : public Packet
{
public:
	PacketAOSTooltipReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7 : PacketEncodedCommand				encoded command
 *
 *
 ***************************************************************************/
class PacketEncodedCommand : public Packet
{
public:
	PacketEncodedCommand();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x02 : PacketHouseDesignBackup		backup house design
 *
 *
 ***************************************************************************/
class PacketHouseDesignBackup : public Packet
{
public:
	PacketHouseDesignBackup();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x03 : PacketHouseDesignRestore		restore house design
 *
 *
 ***************************************************************************/
class PacketHouseDesignRestore : public Packet
{
public:
	PacketHouseDesignRestore();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x04 : PacketHouseDesignCommit		commit house design
 *
 *
 ***************************************************************************/
class PacketHouseDesignCommit : public Packet
{
public:
	PacketHouseDesignCommit();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x05 : PacketHouseDesignDestroyItem	destroy house design item
 *
 *
 ***************************************************************************/
class PacketHouseDesignDestroyItem : public Packet
{
public:
	PacketHouseDesignDestroyItem();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x06 : PacketHouseDesignPlaceItem	place house design item
 *
 *
 ***************************************************************************/
class PacketHouseDesignPlaceItem : public Packet
{
public:
	PacketHouseDesignPlaceItem();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x0C : PacketHouseDesignExit		exit house designer
 *
 *
 ***************************************************************************/
class PacketHouseDesignExit : public Packet
{
public:
	PacketHouseDesignExit();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x0D : PacketHouseDesignPlaceStair	place house design stairs
 *
 *
 ***************************************************************************/
class PacketHouseDesignPlaceStair : public Packet
{
public:
	PacketHouseDesignPlaceStair();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x0E : PacketHouseDesignSync		synchronise house design
 *
 *
 ***************************************************************************/
class PacketHouseDesignSync : public Packet
{
public:
	PacketHouseDesignSync();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x10 : PacketHouseDesignClear		clear house design
 *
 *
 ***************************************************************************/
class PacketHouseDesignClear : public Packet
{
public:
	PacketHouseDesignClear();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x12 : PacketHouseDesignSwitch		switch house design floor
 *
 *
 ***************************************************************************/
class PacketHouseDesignSwitch : public Packet
{
public:
	PacketHouseDesignSwitch();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x13 : PacketHouseDesignPlaceRoof	place house design roof
 *
 *
 ***************************************************************************/
class PacketHouseDesignPlaceRoof : public Packet
{
public:
	PacketHouseDesignPlaceRoof();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x14 : PacketHouseDesignDestroyRoof	destroy house design roof
 *
 *
 ***************************************************************************/
class PacketHouseDesignDestroyRoof : public Packet
{
public:
	PacketHouseDesignDestroyRoof();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x19 : PacketSpecialMove			perform special move
 *
 *
 ***************************************************************************/
class PacketSpecialMove : public Packet
{
public:
	PacketSpecialMove();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x1A : PacketHouseDesignRevert		revert house design
 *
 *
 ***************************************************************************/
class PacketHouseDesignRevert : public Packet
{
public:
	PacketHouseDesignRevert();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x1E : PacketEquipLastWeapon		equip last weapon macro
 *
 *
 ***************************************************************************/
class PacketEquipLastWeapon : public Packet
{
public:
	PacketEquipLastWeapon();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x28 : PacketGuildButton			guild button pressed
 *
 *
 ***************************************************************************/
class PacketGuildButton : public Packet
{
public:
	PacketGuildButton();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD7.0x32 : PacketQuestButton			quest button pressed
 *
 *
 ***************************************************************************/
class PacketQuestButton : public Packet
{
public:
	PacketQuestButton();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xD9 : PacketHardwareInfo				hardware info from client
 *
 *
 ***************************************************************************/
class PacketHardwareInfo : public Packet
{
public:
	PacketHardwareInfo();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xE0 : PacketBugReport					bug report
 *
 *
 ***************************************************************************/
class PacketBugReport : public Packet
{
public:
	PacketBugReport();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xE1 : PacketClientType					client type (KR/SA)
 *
 *
 ***************************************************************************/
class PacketClientType : public Packet
{
public:
	PacketClientType();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xE8 : PacketRemoveUIHighlight			remove ui highlight
 *
 *
 ***************************************************************************/
class PacketRemoveUIHighlight : public Packet
{
public:
	PacketRemoveUIHighlight();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xEB : PacketUseHotbar					use hotbar
 *
 *
 ***************************************************************************/
class PacketUseHotbar : public Packet
{
public:
	PacketUseHotbar();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xEC : PacketEquipItemMacro				equip item(s) macro (KR)
 *
 *
 ***************************************************************************/
class PacketEquipItemMacro : public Packet
{
public:
	PacketEquipItemMacro();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xED : PacketUnEquipItemMacro			unequip item(s) macro (KR)
 *
 *
 ***************************************************************************/
class PacketUnEquipItemMacro : public Packet
{
public:
	PacketUnEquipItemMacro();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xF0 : PacketMovementReqNew				movement request (KR/SA)
 *
 *
 ***************************************************************************/
class PacketMovementReqNew : public Packet
{
public:
	PacketMovementReqNew();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xF1 : PacketTimeSyncRequest				time sync request (KR/SA)
 *
 *
 ***************************************************************************/
class PacketTimeSyncRequest : public Packet
{
public:
	PacketTimeSyncRequest();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xF4 : PacketCrashReport					crash report
 *
 *
 ***************************************************************************/
class PacketCrashReport : public Packet
{
public:
	PacketCrashReport();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xF8 : PacketCreateHS					create new character request (only by CC 7.0.16+)
 *
 *
 ***************************************************************************/
class PacketCreateHS : public PacketCreate
{
public:
	PacketCreateHS();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
 *
 *
 *	Packet 0xF9 : PacketGlobalChatReq				global chat (INCOMPLETE)
 *
 *
 ***************************************************************************/
class PacketGlobalChatReq : public Packet
{
public:
	PacketGlobalChatReq();
    bool onReceive(CNetState* net) override;
};

/***************************************************************************
*
*
*	Packet 0xFA : PacketUltimaStoreButton			ultima store button pressed
*
*
***************************************************************************/
class PacketUltimaStoreButton : public Packet
{
public:
    PacketUltimaStoreButton();
    bool onReceive(CNetState* net) override;
};


/***************************************************************************
*
*
*	Packet 0xFB : PacketPublicHouseContent			show/hide public house content
*
*
***************************************************************************/
class PacketPublicHouseContent : public Packet
{
public:
    PacketPublicHouseContent();
    bool onReceive(CNetState* net) override;
};

#endif // _INC_RECEIVE_H
