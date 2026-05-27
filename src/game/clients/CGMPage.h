/**
* @file CGMPage.h
*
*/

#ifndef _INC_CGMPAGE_H
#define _INC_CGMPAGE_H

#include "../../common/sphere_library/CSString.h"
#include "../../common/CScriptObj.h"
#include "../CServerTime.h"
#include "CAccount.h"

class CClient;
class CGMPage : public CScriptObj
{
	// RES_GMPAGE
public:
	static const char* m_sClassName;
	static lpctstr const sm_szLoadKeys[];
	CGMPage(lpctstr pszAccount);
	~CGMPage() override;
    CGMPage(const CGMPage& copy) = delete;
    CGMPage& operator=(const CGMPage& other) = delete;

	CClient* m_pClientHandling;
	CSString m_sAccount;
	CUID m_uidChar;
	CPointMap m_pt;
	CSString m_sReason;
	CServerTime m_time;

	void SetHandler(CClient* pClient);
	void ClearHandler();

	lpctstr GetName() const override {
        return m_sAccount;
    }

	void r_Write(CScript& s) const;
	bool r_WriteVal(lpctstr pszKey, CSString& sVal, CTextConsole* pSrc, bool fNoCallParent = false, bool fNoCallChildren = false) override;
	bool r_LoadVal(CScript& s) override;
};

#endif	// _INC_CGMPAGE_H
