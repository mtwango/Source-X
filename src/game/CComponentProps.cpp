#include "../sphere/threads.h"
#include "../common/CScript.h"
#include "CComponentProps.h"


CComponentProps::~CComponentProps() noexcept = default;

bool CComponentProps::BaseCont_GetPropertyNum(const BaseContNum_t* container, const PropertyIndex_t iPropIndex, PropertyValNum_t* piOutVal) const
{
    ADDTOCALLSTACK("CComponentProps::GetPropertyNum");
    if (const auto it = container->find(iPropIndex); it != container->end())
    {
        *piOutVal = it->second;
        return true;
    }
    *piOutVal = 0;
    return false;
}

bool CComponentProps::BaseCont_GetPropertyStr(const BaseContStr_t* container, const PropertyIndex_t iPropIndex, CSString *psOutVal, const bool fZero) const
{
    ADDTOCALLSTACK("CComponentProps::GetPropertyStr");
    if (const auto it = container->find(iPropIndex); it != container->end())
    {
        const lpctstr val = it->second.GetBuffer();
        ASSERT(val);
        if (val[0] == '\0')
        {
            *psOutVal = fZero ? "0" : "";
            return true;
        }
        *psOutVal = it->second;
        return true;
    }
    return false;
}

bool CComponentProps::BaseProp_LoadPropVal(const PropertyIndex_t iPropIndex, const bool fPropStr, CScript & s, CObjBase* pLinkedObj, const RESDISPLAY_VERSION iLimitToExpansion)
{
    ADDTOCALLSTACK("CComponentProps::BaseProp_LoadPropVal");
    if (fPropStr)
        return SetPropertyStr(iPropIndex, s.GetArgStr(), pLinkedObj, iLimitToExpansion, true);

    return SetPropertyNum(iPropIndex, s.GetArgVal(), pLinkedObj, iLimitToExpansion, true);
}

bool CComponentProps::BaseProp_WritePropVal(const PropertyIndex_t iPropIndex, const bool fPropStr, CSString & sVal) const
{
    ADDTOCALLSTACK("CComponentProps::BaseProp_WritePropVal");
    if (fPropStr)
    {
        return GetPropertyStrPtr(iPropIndex, &sVal);
    }

    PropertyValNum_t iVal = 0;
    const bool fRet = GetPropertyNumPtr(iPropIndex, &iVal);
    sVal.FormatLLVal(iVal);
    return fRet;
}

void CComponentProps::BaseCont_Write_ContNum(const BaseContNum_t* container, const lpctstr *ptcPropsTable, CScript &s) // static
{
    for (const auto &[fst, snd] : *container)
    {
        if (snd == 0)
            continue;
        s.WriteKeyVal(ptcPropsTable[fst], snd);
    }
}

void CComponentProps::BaseCont_Write_ContStr(const BaseContStr_t* container, const lpctstr *ptcPropsTable, CScript &s) // static
{
    for (const auto &[fst, snd] : *container)
    {
        const lpctstr ptcVal = snd.GetBuffer();
        ASSERT(ptcVal);
        if (ptcVal[0] == '\0')
            continue;
        s.WriteKeyStr(ptcPropsTable[fst], ptcVal);
    }
}

CComponentProps::PropertyValNum_t CComponentProps::GetPropertyNum(const PropertyIndex_t iPropIndex) const
{
    ADDTOCALLSTACK("CComponentProps::GetPropertyNum");
    // Basically a wrapper for GetPropertyNumPtr, when you don't care if the property is present or not
    PropertyValNum_t iRet = 0;
    GetPropertyNumPtr(iPropIndex, &iRet);
    return iRet;
}

CSString CComponentProps::GetPropertyStr(const PropertyIndex_t iPropIndex, const bool fZero) const
{
    ADDTOCALLSTACK("CComponentProps::GetPropertyStr");
    // Basically a wrapper for GetPropertyStrPtr, when you don't care if the property is present or not
    CSString sRet;
    GetPropertyStrPtr(iPropIndex, &sRet, fZero);
    return sRet;
}
