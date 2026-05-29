/**
* @file CItemMemory.h
*
*/

#ifndef _INC_CITEMMEMORY_H
#define _INC_CITEMMEMORY_H

#include "CItem.h"

class CItemStone;


class CItemMemory : public CItem
{
	// IT_EQ_MEMORY
	// Allow extra tags for the memory
public:
	static const char *m_sClassName;
    CItemMemory(ITEMID_TYPE id, CItemBase * pItemDef);
	~CItemMemory() override = default;

	CItemMemory(const CItemMemory& copy) = delete;
	CItemMemory& operator=(const CItemMemory& other) = delete;

	word SetMemoryTypes( word wType );

	word GetMemoryTypes() const;

	bool Guild_IsAbbrevOn() const;
	void Guild_SetAbbrev( bool fAbbrevShow );
	word Guild_GetVotes() const;
	void Guild_SetVotes( word wVotes );
	bool Guild_SetLoyalTo(const CUID &uid );
	CUID Guild_GetLoyalTo() const;
	bool Guild_SetTitle( lpctstr pszTitle );
	lpctstr Guild_GetTitle() const;
	CItemStone * Guild_GetLink();

	int FixWeirdness() override;
};


#endif // CITEMMEMORY_H
