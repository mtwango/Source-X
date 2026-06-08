/**
* @file CSString.cpp
*/

#include "CSString.h"
#include "sstringobjs.h"

#ifndef _WIN32
#include <cstdio>   // for vsnprintf
#endif

#define	CSTRING_DEFAULT_SIZE	42

//#define DEBUG_STRINGS
#ifdef DEBUG_STRINGS
	uint	gAmount		= 0;		// Current amount of CSString.
	size_t	gMemAmount	= 0;		// Total mem allocated by CGStrings.
	uint	gReallocs	= 0;		// Total reallocs caused by CSString resizing.
#endif


// CSString:: Constructors

CSString::CSString(const bool fDefaultInit) :
    m_pchData(nullptr), m_iLength(0), m_iMaxLength(0)
{
#ifdef DEBUG_STRINGS
	++gAmount;
#endif
	if (fDefaultInit)
        InitDefault();
	else
        InitEmpty(false);
}

CSString::CSString(const lpctstr pStr) :
	m_pchData(nullptr), m_iLength(0), m_iMaxLength(0)
{
	Copy(pStr);
}

CSString::CSString(const lpctstr pStr, const int iLen) :
	m_pchData(nullptr), m_iLength(0), m_iMaxLength(0)
{
	CopyLen(pStr, iLen);
}

CSString::CSString(const CSString& s) :
	m_pchData(nullptr), m_iLength(0), m_iMaxLength(0)
{
	Copy(s.GetBuffer());
}


CSString::~CSString() noexcept
{
#ifdef DEBUG_STRINGS
    --gAmount;
#endif
    if (IsValid())
        delete[] m_pchData;
}

// private
void CSString::InitEmpty(const bool fManageBuffer)
{
    if (fManageBuffer && IsValid())
    {
#ifdef DEBUG_STRINGS
        gMemAmount -= m_iMaxLength;
#endif
        delete[] m_pchData;
    }

    m_iMaxLength = 0;
    m_iLength    = 0;

    // Suppress GCC warning by using the const_cast.
    // I know that i shouldn't set the non-cast buffer to a string constant, but i make sure in every method that i can't modify it.
    // Dirty, but this will not pass IsValid, so it will be reallocated.
    m_pchData = const_cast<char*>("");
}

void CSString::InitDefault()
{
    ASSERT(m_pchData == nullptr);

    m_iMaxLength = CSTRING_DEFAULT_SIZE;
	m_iLength = 0;
    m_pchData = new tchar[static_cast<size_t>(m_iMaxLength + 1)];
	m_pchData[m_iLength] = '\0';
#ifdef DEBUG_STRINGS
        gMemAmount += m_iMaxLength;
#endif
}

// CSString:: Capacity

void CSString::Clear(const bool fResetBuffer) noexcept
{
    if (fResetBuffer)
    {
        if (IsValid())
            delete[] m_pchData;
        InitDefault();
        return;
    }

    if (IsValid())
    {
        m_pchData[0] = '\0';
        m_iLength = 0;
    }
    else
    {
        InitEmpty(false);
    }
}

bool CSString::IsValid() const noexcept
{
    return ((m_iMaxLength != 0) && (m_pchData != nullptr));
}

int CSString::Resize(const int iNewLength, const bool fPreciseSize)
{
    // Invalid new length.
    if (iNewLength < 0)
    {
        return -1;
    }

    const bool fValid = IsValid();

    // Special‐case zero length: revert to empty literal.
    if (iNewLength == 0)
    {
        InitEmpty(true);
        return 0;
    }

    // Only reallocate if growing beyond capacity or starting from invalid (empty literal).
    if ((iNewLength >= m_iMaxLength) || !fValid)
	{
#ifdef DEBUG_STRINGS
		gMemAmount -= m_iMaxLength;
#endif

        // allow grow, and expand only
        int iNewMax;
        if (fPreciseSize)
        {
            iNewMax = iNewLength;
        }
        else
        {
            //iNewMax = iNewLength + (CSTRING_DEFAULT_SIZE >> 1);   // Probably too much...
            iNewMax = (iNewLength * 3) >> 1;   // >> 2 is equal to doing / 2
        }
        iNewMax = std::max(iNewMax, 1);

        tchar *pNewData = nullptr;
        try
        {
            pNewData = new tchar[static_cast<size_t>(iNewMax + 1)];
        }
        catch (const std::bad_alloc &)
        {
            // Allocation failed: leave object unchanged
            return -1;
        }

        if (fValid)
		{
			const int iMinLength = 1 + minimum(iNewLength, m_iLength);
			Str_CopyLimitNull(pNewData, m_pchData, iMinLength);
		}

#ifdef DEBUG_STRINGS
        if (fValid)
            gMemAmount -= m_iMaxLength;
        gMemAmount += m_iMaxLength;
        ++gReallocs;
#endif

        if (fValid)
        {
            delete[] m_pchData;
            m_pchData = nullptr;
            m_iMaxLength = 0;
        }

		m_pchData = pNewData;
        m_iMaxLength = iNewMax;
	}
	ASSERT(m_pchData);
	m_iLength = iNewLength;
	m_pchData[m_iLength] = '\0';
	return m_iLength;
}

void CSString::SetValFalse()
{
    Copy("0");
}

void CSString::SetValTrue()
{
    Copy("1");
}


// CSString:: Element access

void CSString::SetAt(const int nIndex, const tchar ch)
{
	if (!IsValid())
	{
        InitDefault();
	}
	ASSERT(nIndex < m_iLength);

	m_pchData[nIndex] = ch;
	if (!ch)
	{
		m_iLength = static_cast<int>(strlen(m_pchData));	// \0 inserted. line truncated
	}
}


// CSString:: Modifiers

void CSString::Add(const tchar ch)
{
	const int iLen = m_iLength;
	Resize(iLen + 1);
	SetAt(iLen, ch);
}

void CSString::Add(const lpctstr pszStr)
{
    ASSERT(pszStr);
    if (const int iLenCat = static_cast<int>(strlen(pszStr)))
	{
		Resize(iLenCat + m_iLength);
        m_iLength = static_cast<int>(Str_ConcatLimitNull(m_pchData, pszStr, m_iLength + 1));
	}
}

void CSString::Copy(lpctstr pszStr)
{
    if (!pszStr)
    {
        InitEmpty(false);
#ifdef _DEBUG
        g_Log.EventDebug("Trying to Copy a nullptr c-string in a CSString. You may want to double-check this, as it might be unintentional.\n");
#endif
        return;
    }

    if (/* redundant: m_pchData &&*/ pszStr == m_pchData)
        return;

    if (*pszStr == '\0')
    {
        Clear(false);
        return;
    }

    const size_t uiLen = strlen(pszStr);
    if (uiLen == 0)
    {
        Clear(false);
        return;
    }

    Resize(static_cast<int>(uiLen), true); // it adds a +1
    Str_CopyLimitNull(m_pchData, pszStr, uiLen + 1);
}

void CSString::CopyLen(lpctstr pszStr, const int iLen)
{
    if (!pszStr)
    {
        InitEmpty(false);
#ifdef _DEBUG
        g_Log.EventDebug("Trying to CopyLen a nullptr c-string in a CSString. You may want to double-check this, as it might be unintentional.\n");
#endif
        return;
    }

    if (/* redundant: m_pchData &&*/ pszStr == m_pchData)
        return;


    if ((*pszStr == '\0') || (iLen <= 0))
    {
        Clear(false);
        return;
    }

    Resize(iLen, true); // it adds a +1
    Str_CopyLimitNull(m_pchData, pszStr, static_cast<size_t>(iLen + 1));
}


// CSString:: Operators

const CSString& CSString::operator=(const CSString& s)
{
	Copy(s.GetBuffer());
	return *this;
}

const CSString& CSString::operator=(const lpctstr pStr)
{
	Copy(pStr);
	return *this;
}

const CSString& CSString::operator+=(const lpctstr string)
{
	Add(string);
	return(*this);
}

const CSString& CSString::operator+=(const tchar ch)
{
	Add(ch);
	return(*this);
}

CSString CSString::operator+(const lpctstr string)
{
	CSString temp(*this);
	temp += string;
	return temp;
}

CSString& CSString::operator=(CSString&& s) noexcept
{
	if (this != &s)
	{
		if (m_pchData != nullptr)
		{
			delete[] m_pchData;
		}
		m_iLength = s.m_iLength;
		m_iMaxLength = s.m_iMaxLength;
		m_pchData = s.m_pchData;
		s.m_pchData = nullptr;
	}
	return *this;
}

// CSString:: Formatting

void CSString::Format(lpctstr pStr, ...)
{
	va_list vargs;
	va_start(vargs, pStr);
	FormatV(pStr, vargs);
	va_end(vargs);
}

void CSString::FormatV(const lpctstr pszFormat, const va_list args)
{
	TemporaryString tsTemp;
	vsnprintf(tsTemp.buffer(), tsTemp.capacity(), pszFormat, args);
	Copy(tsTemp.buffer());
}

#define FORMATNUM_WRAPPER(function, arg, base) \
    tchar ptcBuf[24]; \
    Copy(function(arg, ptcBuf, sizeof(ptcBuf), base))

void CSString::FormatLLHex(const llong iVal)
{
    //Format("0%" PRIx64, iVal);
    FORMATNUM_WRAPPER(Str_FromLL_Fast, iVal, 16);
}
void CSString::FormatULLHex(const ullong uiVal)
{
    //Format("0%" PRIx64, uiVal);
    FORMATNUM_WRAPPER(Str_FromULL_Fast, uiVal, 16);
}
void CSString::FormatHex(const dword dwVal)
{
    // As a general rule, all values in sphere logic are signed...
    // dwVal may contain a (signed) number "big" as the numeric representation of an unsigned ( +(INT_MAX*2) ),
    // but in this case its bit representation would be considered as negative, yet we know it's a positive number.
    // So if it's negative we MUST hexformat it as 64 bit int or reinterpreting it in a
    // script WILL completely mess up
    if (dwVal > static_cast<dword>INT32_MIN)			// if negative (remember two's complement)
        return FormatULLHex(dwVal);
    //Format("0%" PRIx32, dwVal);
    FORMATNUM_WRAPPER(Str_FromUI_Fast, dwVal, 16);
}
void CSString::FormatCVal(const char iVal)
{
    //Format("%hhd", iVal);
    FORMATNUM_WRAPPER(Str_FromI_Fast, iVal, 10);
}
void CSString::FormatUCVal(const uchar uiVal)
{
    //Format("%hhu", uiVal);
    FORMATNUM_WRAPPER(Str_FromUI_Fast, uiVal, 10);
}
void CSString::FormatSVal(const short iVal)
{
    //Format("%hd", iVal);
    FORMATNUM_WRAPPER(Str_FromI_Fast, iVal, 10);
}
void CSString::FormatUSVal(const ushort uiVal)
{
    //Format("%hu", uiVal);
    FORMATNUM_WRAPPER(Str_FromUI_Fast, uiVal, 10);
}
void CSString::FormatVal(const int iVal)
{
    //Format("%d", iVal);
    FORMATNUM_WRAPPER(Str_FromI_Fast, iVal, 10);
}
void CSString::FormatUVal(const uint uiVal)
{
    //Format("%u", uiVal);
    FORMATNUM_WRAPPER(Str_FromUI_Fast, uiVal, 10);
}
void CSString::FormatLLVal(const llong iVal)
{
    //Format("%lld", iVal);
    FORMATNUM_WRAPPER(Str_FromLL_Fast, iVal, 10);
}
void CSString::FormatULLVal(const ullong uiVal)
{
    //Format("%llu", uiVal);
    FORMATNUM_WRAPPER(Str_FromULL_Fast, uiVal, 10);
}
void CSString::FormatSTVal(const size_t uiVal)
{
    static_assert(sizeof(size_t) <= sizeof(ullong),
        "You can't use FormatSTVal on this architecture (it uses internally Str_FromULL_Fast). Use the old call to Format instead.");
    //Format("%" PRIuSIZE_T, iVal);
    FORMATNUM_WRAPPER(Str_FromULL_Fast, uiVal, 10);
}
void CSString::FormatBVal(const byte uiVal)
{
    //Format("0%" PRIx8, uiVal);
    FORMATNUM_WRAPPER(Str_FromUI_Fast, uiVal, 16);
}
void CSString::FormatWVal(const word uiVal)
{
    //Format("0%" PRIx16, uiVal);
    FORMATNUM_WRAPPER(Str_FromUI_Fast, uiVal, 16);
}
void CSString::FormatDWVal(const dword uiVal)
{
    //Format("0%" PRIx32, uiVal);
    FORMATNUM_WRAPPER(Str_FromUI_Fast, uiVal, 16);
}
void CSString::Format8Val(const int8 iVal)
{
    //Format("%" PRId8, iVal);
    FORMATNUM_WRAPPER(Str_FromI_Fast, iVal, 10);
}
void CSString::FormatU8Val(const uint8 uiVal)
{
    //Format("%" PRIu8, uiVal);
    FORMATNUM_WRAPPER(Str_FromUI_Fast, uiVal, 10);
}
void CSString::Format16Val(const int16 iVal)
{
    //Format("%" PRId16, iVal);
    FORMATNUM_WRAPPER(Str_FromI_Fast, iVal, 10);
}
void CSString::FormatU16Val(const uint16 uiVal)
{
    //Format("%" PRIu16, uiVal);
    FORMATNUM_WRAPPER(Str_FromUI_Fast, uiVal, 10);
}
void CSString::Format32Val(const int32 iVal)
{
    //Format("%" PRId32, iVal);
    FORMATNUM_WRAPPER(Str_FromI_Fast, iVal, 10);
}
void CSString::FormatU32Val(const uint32 uiVal)
{
    //Format("%" PRIu32, uiVal);
    FORMATNUM_WRAPPER(Str_FromUI_Fast, uiVal, 10);
}
void CSString::Format64Val(const int64 iVal)
{
    //Format("%" PRId64, iVal);
    FORMATNUM_WRAPPER(Str_FromLL_Fast, iVal, 10);
}
void CSString::FormatU64Val(const uint64 uiVal)
{
    //Format("%" PRIu64, uiVal);
    FORMATNUM_WRAPPER(Str_FromULL_Fast, uiVal, 10);
}

#undef FORMATNUM_WRAPPER

// CSString:: String operations

tchar CSString::GetAt(const int nIndex) const
{
    ASSERT(nIndex >= 0);
    ASSERT(nIndex <= m_iLength);  // Allow to get the null char.
    return m_pchData[nIndex];
}

tchar& CSString::ReferenceAt(const int nIndex)
{
    ASSERT(nIndex >= 0);
    ASSERT(nIndex < m_iLength);
    return m_pchData[nIndex];
}

void CSString::MakeUpper() noexcept
{
    _strupr(m_pchData);
}

void CSString::MakeLower() noexcept
{
    _strlwr(m_pchData);
}

void CSString::Reverse() noexcept
{
    Str_Reverse(m_pchData);
}

int CSString::Compare(const lpctstr pStr) const noexcept
{
    return strcmp(m_pchData, pStr);
}

int CSString::CompareNoCase(const lpctstr pStr) const noexcept
{
    return strcmpi(m_pchData, pStr);
}

int CSString::indexOf(const tchar c) noexcept
{
    return indexOf(c, 0);
}

int CSString::indexOf(const CSString& str) noexcept
{
    return indexOf(str, 0);
}

int CSString::lastIndexOf(const tchar c) noexcept
{
    return lastIndexOf(c, 0);
}

int CSString::lastIndexOf(const CSString& str) noexcept
{
    return lastIndexOf(str, 0);
}

int CSString::indexOf(const tchar c, const int offset) noexcept
{
	if ((offset < 0) || !IsValid())
		return -1;

    const int len = static_cast<int>(strlen(m_pchData));
	if (offset >= len)
		return -1;

	for (int i = offset; i<len; ++i)
	{
		if (m_pchData[i] == c)
			return i;
	}
	return -1;
}

int CSString::indexOf(const CSString& str, const int offset) noexcept
{
	if ((offset < 0) || !IsValid())
		return -1;

    const int len = static_cast<int>(strlen(m_pchData));
	if (offset >= len)
		return -1;

	int const slen = str.GetLength();
	if (slen > len)
		return -1;

    const auto str_value = new tchar[static_cast<size_t>(slen + 1)];
	Str_CopyLimitNull(str_value, str.GetBuffer(), static_cast<size_t>(slen+1));
    const tchar firstChar = str_value[0];

	for (int i = offset; i < len; ++i)
	{
        if (tchar const c = m_pchData[i]; c == firstChar)
		{
            if (int const rem = len - i; rem >= slen)
			{
				int j = i;
				int k = 0;
				bool found = true;
				while (k < slen)
				{
					if (m_pchData[j] != str_value[k])
					{
						found = false;
						break;
					}
					++j; ++k;
				}
				if (found)
				{
					delete[] str_value;
					return i;
				}
			}
		}
	}

	delete[] str_value;
	return -1;
}

int CSString::lastIndexOf(const tchar c, const int from) noexcept
{
	if ((from < 0) || !IsValid())
		return -1;

    const int len = static_cast<int>(strlen(m_pchData));
	if (from > len)
		return -1;

	for (int i = (len - 1); i >= from; --i)
	{
		if (m_pchData[i] == c)
			return i;
	}
	return -1;
}

int CSString::lastIndexOf(const CSString& str, const int from) noexcept
{
	if ((from < 0) || !IsValid())
		return -1;

    const int len = static_cast<int>(strlen(m_pchData));
	if (from >= len)
		return -1;

	int slen = str.GetLength();
	if (slen > len)
		return -1;

	lpctstr str_value = str.GetBuffer();
	const tchar firstChar = str_value[0];
	for (int i = (len - 1); i >= from; --i)
	{
        if (const tchar c = m_pchData[i]; c == firstChar)
		{
			if (i >= slen)
			{
				int j = i;
				int k = 0;
				bool found = true;
				while (k < slen)
				{
					if (m_pchData[j] != str_value[k])
					{
						found = false;
						break;
					}
					++j; ++k;
				}
				if (found)
				{
					return i;
				}
			}
		}
	}

	return -1;
}
