/**
* @file CCPropsItemWeaponRanged.h
*
*/

#ifndef _INC_CCPROPSITEMWEAPONRANGED_H
#define _INC_CCPROPSITEMWEAPONRANGED_H

#include "../CComponentProps.h"

class CObjBase;


enum PROPIWEAPRNG_TYPE : CComponentProps::PropertyIndex_t
{
    #define ADDPROP(a,b,c) PROPIWEAPRNG_##a,
    #include "../../tables/CCPropsItemWeaponRanged_props.tbl"
    #undef ADDPROP
    PROPIWEAPRNG_QTY,
};

class CCPropsItemWeaponRanged : public CComponentProps
{
    static lpctstr const        _ptcPropertyKeys[];
    static RESDISPLAY_VERSION   _iPropertyExpansion[];

public:
    static constexpr COMPPROPS_TYPE _kiType = COMP_PROPS_ITEMWEAPONRANGED;

    CCPropsItemWeaponRanged();
    ~CCPropsItemWeaponRanged() override = default;

    static bool CanSubscribe(const CItemBase* pItemBase) noexcept;
    static bool CanSubscribe(const CItem* pItem) noexcept;

    lpctstr GetName() const override {
        return "ItemWeaponRanged";
    }
    PropertyIndex_t GetPropsQty() const override {
        return PROPIWEAPRNG_QTY;
    }
    KeyTableDesc_s GetPropertyKeysData() const override;
    lpctstr GetPropertyName(PropertyIndex_t iPropIndex) const override;
    bool IsPropertyStr(PropertyIndex_t iPropIndex) const override;
    bool GetPropertyNumPtr(PropertyIndex_t iPropIndex, PropertyValNum_t* piOutVal) const override;
    bool GetPropertyStrPtr(PropertyIndex_t iPropIndex, CSString *psOutVal, bool fZero = false) const override;
    bool SetPropertyNum(PropertyIndex_t iPropIndex, PropertyValNum_t iVal, CObjBase* pLinkedObj, RESDISPLAY_VERSION iLimitToExpansion = RDS_QTY, bool fDeleteZero = true) override;
    bool SetPropertyStr(PropertyIndex_t iPropIndex, lpctstr ptcVal, CObjBase* pLinkedObj, RESDISPLAY_VERSION iLimitToExpansion = RDS_QTY, bool fDeleteZero = true) override;
    void DeletePropertyNum(PropertyIndex_t iPropIndex) override;
    void DeletePropertyStr(PropertyIndex_t iPropIndex) override;

    bool FindLoadPropVal(CScript & s, CObjBase* pLinkedObj, RESDISPLAY_VERSION iLimitToExpansion, PropertyIndex_t iPropIndex, bool fPropStr) override; // Use pLinkedObj = nullptr if calling this from CItemBase or CCharBase
    bool FindWritePropVal(CSString & sVal, PropertyIndex_t iPropIndex, bool fPropStr) const override;
    void r_Write(CScript & s) override;
    void Copy(const CComponentProps *target) override;

    void AddPropsTooltipData(CObjBase* pLinkedObj) override;

private:
    BaseContNum_t _mPropsNum;
    BaseContStr_t _mPropsStr;
};


#endif //_INC_CCPROPSITEMWEAPONRANGED_H
