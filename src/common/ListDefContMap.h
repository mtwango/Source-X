/**
* @file ListDefContMap.h
*
*/

#ifndef _INC_LISTDEFCONTMAP_H
#define _INC_LISTDEFCONTMAP_H

#include <deque>
#include <set>
#include "sphere_library/CSString.h"


class CTextConsole;
class CScript;

class CListDefContElem
{
	CSString m_Key;	// reference to map key

public:
	static const char *m_sClassName;

    explicit CListDefContElem(lpctstr ptcKey);
	virtual ~CListDefContElem();

	CListDefContElem(const CListDefContElem& copy) = delete;
	CListDefContElem& operator=(const CListDefContElem& other) = delete;

    lpctstr GetKey() const {
        return m_Key.GetBuffer();
    }
    void SetKey(const lpctstr ptcKey) {
        m_Key = ptcKey;
    }

	virtual lpctstr GetValStr() const = 0;
	virtual int64 GetValNum() const = 0;
	virtual CListDefContElem * CopySelf() const = 0;
};


class CListDefContNum: public CListDefContElem
{
	int64 m_iVal;

public:
	static const char *m_sClassName;

	explicit CListDefContNum(lpctstr ptcKey);
	CListDefContNum(lpctstr ptcKey, int64 iVal);
	~CListDefContNum() override = default;

private:
	CListDefContNum(const CListDefContNum& copy);
	CListDefContNum& operator=(const CListDefContNum& other);

public:
    int64 GetValNum() const override {
        return m_iVal;
    }
    void SetValNum(const int64 iVal) {
        m_iVal = iVal;
    }
	lpctstr GetValStr() const override;

	bool r_LoadVal( CScript & s );
	bool r_WriteVal( lpctstr pKey, CSString & sVal, CTextConsole * pSrc = nullptr);

	CListDefContElem * CopySelf() const override;
};


class CListDefContStr: public CListDefContElem
{
	CSString m_sVal;	// the assigned value. (What if numeric?)

public:
	static const char *m_sClassName;

	CListDefContStr(lpctstr ptcKey, lpctstr pszVal);
	explicit CListDefContStr(lpctstr ptcKey);
	~CListDefContStr() override = default;

private:
	CListDefContStr(const CListDefContStr& copy);
	CListDefContStr& operator=(const CListDefContStr& other);

public:
    lpctstr GetValStr() const override {
        return m_sVal.GetBuffer();
    }
	void SetValStr( lpctstr pszVal );
	int64 GetValNum() const override;

	bool r_LoadVal( CScript & s );
	bool r_WriteVal( lpctstr pKey, CSString & sVal, CTextConsole * pSrc = nullptr );

	CListDefContElem * CopySelf() const override;
};


class CListDefCont
{
	CSString m_Key;	// reference to map key

	typedef std::deque<CListDefContElem *> DefList;

protected:
	DefList	m_listElements;

	void DeleteAtIterator(const DefList::iterator &it, bool fEraseFromDefList = true);

public:
	static const char *m_sClassName;

	explicit CListDefCont(lpctstr ptcKey);
	~CListDefCont() = default;

private:
	CListDefCont(const CListDefCont& copy);
	CListDefCont& operator=(const CListDefCont& other);

public:
    lpctstr GetKey() const {
        return m_Key.GetBuffer();
    }
	void SetKey( lpctstr ptcKey );

	CListDefContElem* GetAt(size_t nIndex) const;
	bool SetNumAt(size_t nIndex, int64 iVal);
	bool SetStrAt(size_t nIndex, lpctstr pszVal);
    size_t GetCount() const {
        return m_listElements.size();
    }

	lpctstr GetValStr(size_t nIndex) const;
	int64 GetValNum(size_t nIndex) const;

	int FindValNum( int64 iVal, size_t nStartIndex = 0 ) const;
	int FindValStr( lpctstr pVal, size_t nStartIndex = 0 ) const;

	bool AddElementNum(int64 iVal);
	bool AddElementStr(lpctstr ptcKey);

	bool RemoveElement(size_t nIndex);
	void RemoveAll();
	void Sort(bool bDesc = false, bool bCase = false);

	bool InsertElementNum(size_t nIndex, int64 iVal);
	bool InsertElementStr(size_t nIndex, lpctstr ptcKey);

	CListDefCont * CopySelf();
	void PrintElements(CSString& strElements) const;
	void DumpElements( CTextConsole * pSrc, lpctstr pszPrefix = nullptr ) const;
	void r_WriteSave( CScript& s ) const;
	bool r_LoadVal( CScript& s );
	bool r_LoadVal( lpctstr ptcArg );
};


class CListDefMap
{
	struct ltstr
	{
        bool operator()(const CListDefCont * s1, const CListDefCont * s2) const
        {
            return( strcmpi(s1->GetKey(), s2->GetKey()) < 0 );
        }
	};

	typedef std::set<CListDefCont *, ltstr> DefSet;
	typedef std::pair<DefSet::iterator, bool> DefPairResult;

	DefSet m_Container;

public:
	static const char *m_sClassName;
	CListDefMap & operator = ( const CListDefMap & array );
	CListDefMap() { }
	~CListDefMap();

private:
	CListDefMap(const CListDefMap& copy);

	CListDefCont * GetAtKey( lpctstr at );
	void DeleteAt( size_t at );
	void DeleteAtKey( lpctstr at );
	void DeleteAtIterator(const DefSet::iterator &it );

public:
	void Copy( const CListDefMap * pArray );
	void Empty();
    size_t GetCount() const {
        return m_Container.size();
    }

	lpctstr FindValNum( int64 iVal ) const;
	lpctstr FindValStr( lpctstr pVal ) const;

	CListDefCont * GetAt( size_t at );
	CListDefCont * GetKey( lpctstr ptcKey ) const;

	CListDefCont* AddList(lpctstr ptcKey);

    void DumpKeys( CTextConsole * pSrc, lpctstr pszPrefix = nullptr ) const;
	void ClearKeys(lpctstr mask = nullptr);
	void DeleteKey( lpctstr key );

    bool r_LoadVal( lpctstr ptcKey, const CScript & s ) NONVIRTUAL;
    bool r_Write( CTextConsole *pSrc, lpctstr pszString, CSString& strVal ) const NONVIRTUAL;
    void r_WriteSave( CScript& s ) const NONVIRTUAL;
};

#endif // _INC_LISTDEFCONTMAP_H
