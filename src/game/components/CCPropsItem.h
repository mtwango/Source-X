/**
* @file CCPropsItem.h
*
*/

#ifndef _INC_CCPROPSITEM_H
#define _INC_CCPROPSITEM_H

#include "../CComponentProps.h"


enum PROPIT_TYPE : CComponentProps::PropertyIndex_t
{
    #define ADDPROP(a,b,c) PROPIT_##a,
    #include "../../tables/CCPropsItem_props.tbl"
    #undef ADDPROP
    PROPIT_QTY,
};

class CCPropsItem : public CComponentProps
{
    static lpctstr const        _ptcPropertyKeys[];
    static RESDISPLAY_VERSION   _iPropertyExpansion[];

public:
    static constexpr COMPPROPS_TYPE _kiType = COMP_PROPS_ITEM;

    CCPropsItem();
    ~CCPropsItem() override = default;

    //static bool CanSubscribe(const CObjBase* pObj) noexcept;

    lpctstr GetName() const override {
        return "Item";
    }
    PropertyIndex_t GetPropsQty() const override {
        return PROPIT_QTY;
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


#endif //_INC_CCPROPSITEM_H
