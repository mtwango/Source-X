/**
* @file CCItemDamageable.h
*
*/

#ifndef _INC_CCITEMDAMAGEABLE_H
#define _INC_CCITEMDAMAGEABLE_H

#include "../CComponent.h"

class CItem;


class CCItemDamageable : public CComponent
{
    CItem *_pLink;
    static lpctstr const sm_szLoadKeys[];

    word _iCurHits;
    word _iMaxHits;
    int64 _iTimeLastUpdate;
    bool _fNeedUpdate;

public:
    CCItemDamageable(CItem *pLink);
    ~CCItemDamageable() override = default;
    static bool CanSubscribe(const CItem* pItem) noexcept;

    CItem *GetLink() const noexcept;

    void SetCurHits(word iCurHits);
    void SetMaxHits(word iMaxHits);
    word GetCurHits() const;
    word GetMaxHits() const;
    void OnTickStatsUpdate();

    void Delete(bool fForced = false) override;
    bool r_LoadVal(CScript & s) override;
    bool r_WriteVal(lpctstr ptcKey, CSString & s, CTextConsole * pSrc = nullptr) override;
    void r_Write(CScript & s) override;
    bool r_GetRef(lpctstr & ptcKey, CScriptObj * & pRef) override;
    bool r_Verb(CScript & s, CTextConsole * pSrc) override;
    void Copy(const CComponent *target) override;
    CCRET_TYPE OnTickComponent() override;
};
#endif //_INC_CCITEMDAMAGEABLE_H
