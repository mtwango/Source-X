/**
* @file CItemContainer.h
*
*/

#ifndef _INC_CITEMCONTAINER_H
#define _INC_CITEMCONTAINER_H

#include "../CContainer.h"
#include "CItemVendable.h"


class CItemContainer : public CItemVendable, public CContainer
{
	// This item has other items inside it.
	static lpctstr const sm_szVerbKeys[];
    CUID _uidMultiSecured;
    CUID _uidMultiCrate;

public:
	static const char *m_sClassName;

	bool NotifyDelete() override;	// overrides CItem:: method
	void DeletePrepare() override;

    void SetSecuredOfMulti(const CUID& uidMulti);
    void SetCrateOfMulti(const CUID& uidMulti);

	CItemContainer( ITEMID_TYPE id, CItemBase * pItemDef );
    ~CItemContainer() override;

    CItemContainer(const CItemContainer& copy) = delete;
    CItemContainer& operator=(const CItemContainer& other) = delete;

	bool IsWeighed() const;
	bool IsSearchable() const;

	bool IsItemInside(const CItem * pItem) const;
	bool CanContainerHold(const CItem * pItem, const CChar * pCharMsg );

	bool r_Verb( CScript & s, CTextConsole * pSrc ) override;
	void r_Write( CScript & s ) override;
	bool r_WriteVal( lpctstr ptcKey, CSString & sVal, CTextConsole * pSrc = nullptr, bool fNoCallParent = false, bool fNoCallChildren = false ) override;
	bool r_GetRef( lpctstr & ptcKey, CScriptObj * & pRef ) override;

	int GetWeight(word amount = 0) const override;
	void OnWeightChange( int iChange ) override;

	// Contents/Carry stuff. ---------------------------------
	void ContentAdd( CItem * pItem, bool bForceNoStack = false ) override;
	void ContentAdd( CItem * pItem, CPointMap pt, bool fForceNoStack = false, uchar gridIndex = 0 );
protected:
	void OnRemoveObj( CSObjContRec* pObRec ) override;	// Override this = called when removed from list.

	void _GoAwake() final;
	void _GoSleep() final;

public:
	bool IsItemInTrade() const;
	void Trade_Status( bool bCheck );
	void Trade_UpdateGold( dword platinum, dword gold );
	bool Trade_Delete();

	void MakeKey();
	void SetKeyRing();
	void Game_Create();
	void Restock();
	bool _OnTick() override;

	void DupeCopy( const CObjBase * pItem ) override;  // overriding CItem::DupeCopy

    /**
     * Gets the random location in container based on sizes defined in tdata3/4.
     */
    CPointMap GetRandContainerLoc() const;

	void OnOpenEvent( CChar * pCharOpener, const CObjBaseTemplate * pObjTop );
};


#endif // _INC_CITEMCONTAINER_H
