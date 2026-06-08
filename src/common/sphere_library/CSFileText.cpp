#include "../../sphere/threads.h"
#include "CSFileText.h"

#ifdef _WIN32
    #include <io.h> // for _get_osfhandle (used by STDFUNC_FILENO)
#endif

// CSFileText:: Constructors, Destructor, Assign operator.

const char*
CSFileText::m_sClassName = "CSFileText";

CSFileText::CSFileText()
{
    _pStream = nullptr;
#ifdef _WIN32
    _fNoBuffer = false;
#endif
    _fBinaryMode = false;
}

CSFileText::~CSFileText()
{
    Close();
}

// CSFileText:: File management.

bool CSFileText::_IsFileOpen() const
{
    return (_pStream != nullptr);
}
bool CSFileText::IsFileOpen() const
{
    MT_SHARED_LOCK_RETURN(this, _pStream != nullptr);
}

bool CSFileText::_Open(lpctstr ptcFilename, const uint uiModeFlags)
{
    ADDTOCALLSTACK("CSFileText::_Open");

    // Open a text file.

	if ( !ptcFilename )
        ptcFilename = _strFileName.GetBuffer();
    else
        _strFileName = ptcFilename;

    if ( _strFileName.IsEmpty() )
        return false;

    _uiMode = uiModeFlags;
    const lpctstr ptcModeStr = _GetModeStr();

    _pStream = fopen( ptcFilename, ptcModeStr );
    if ( _pStream == nullptr )
        return false;

    // Get the file descriptor for it.
    _fileDescriptor = reinterpret_cast<file_descriptor_t>(STDFUNC_FILENO(_pStream));

    return true;
}
bool CSFileText::Open(const lpctstr ptcFilename, const uint uiModeFlags)
{
    ADDTOCALLSTACK("CSFileText::Open");
    MT_UNIQUE_LOCK_RETURN(this, CSFileText::_Open(ptcFilename, uiModeFlags));
}

void CSFileText::_Close()
{
    ADDTOCALLSTACK("CSFileText::_Close");

    // CCacheableScriptFile opens the file, reads and closes it. It should never be opened, so pStream should be always nullptr.
    if ((_pStream != nullptr) /*&& (_pStream != _kInvalidFD)*/)
    {
        if (_IsWriteMode())
        {
            fflush(_pStream);
        }

        fclose(_pStream);
        _pStream = nullptr;
        _fileDescriptor = _kInvalidFD;
    }
}
void CSFileText::Close()
{
    ADDTOCALLSTACK("CSFileText::Close");
    MT_UNIQUE_LOCK_SET(this);
    CSFileText::_Close();
}

// CSFileText:: Content management.
int CSFileText::_Seek(const int iOffset, const int iOrigin )
{
    // RETURN:
    //  true = success
    ADDTOCALLSTACK("CSFileText::_Seek");

    if ( !CSFileText::_IsFileOpen() )
        return 0;
    if ( iOffset < 0 )
        return 0;

    if ( fseek(_pStream, iOffset, iOrigin) != 0 )
        return 0;

    const long iPos = ftell(_pStream);
    if ( iPos < 0 )
    {
        return 0;
    }
    if (iPos > INT_MAX) // be consistent between windows and linux: support on both platforms at maximum an int (long has 4 or 8 bytes width, depending on the os)
    {
        _NotifyIOError("CSFileText::Seek (length)");
        return INT_MAX;
    }

    return static_cast<int>(iPos);
}
int CSFileText::Seek(const int iOffset, const int iOrigin )
{
    // RETURN:
    //  true = success
    ADDTOCALLSTACK("CSFileText::Seek");
    MT_UNIQUE_LOCK_RETURN(this, CSFileText::_Seek(iOffset, iOrigin));
}

void CSFileText::_Flush() const
{
    ADDTOCALLSTACK("CSFileText::_Flush");

    if ( !_IsFileOpen() )
        return;

    ASSERT(_pStream);
    fflush(_pStream);
}
void CSFileText::Flush() const
{
    ADDTOCALLSTACK("CSFileText::Flush");
    MT_UNIQUE_LOCK_SET(this);
    _Flush();
}

bool CSFileText::_IsEOF() const
{
    //ADDTOCALLSTACK("CSFileText::_IsEOF");

    if ( !_IsFileOpen() )
        return true;

    return (feof(_pStream) ? true : false);
}
bool CSFileText::IsEOF() const
{
    //ADDTOCALLSTACK("CSFileText::IsEOF");
    MT_SHARED_LOCK_RETURN(this, CSFileText::_IsEOF());
}


int CSFileText::_Printf(lpctstr pFormat, ...)
{
    ADDTOCALLSTACK("CSFileText::_Printf");
    ASSERT(pFormat);

    va_list vargs;
    va_start(vargs, pFormat);
    const int iRet = _VPrintf(pFormat, vargs);
    va_end(vargs);

    return iRet;
}
int CSFileText::Printf( lpctstr pFormat, ... )
{
    ADDTOCALLSTACK("CSFileText::Printf");
    ASSERT(pFormat);

    MT_UNIQUE_LOCK_SET(this);

    va_list vargs;
    va_start( vargs, pFormat );
    const int iRet = _VPrintf( pFormat, vargs );
    va_end( vargs );

    return iRet;
}

int CSFileText::Read( void * pBuffer, const int sizemax ) const
{
    // This can return: EOF(-1) constant.
    // returns the number of full items actually read
    ADDTOCALLSTACK("CSFileText::Read");
    ASSERT(pBuffer);

    if ( IsEOF() )
        return 0;	// LINUX will ASSERT if we read past end.

    MT_UNIQUE_LOCK_SET(this);
    const size_t ret = fread( pBuffer, 1, sizemax, _pStream );
    if (ret > INT_MAX)
    {
        _NotifyIOError("CSFileText::Read (length)");
        return 0;
    }
    return static_cast<int>(ret);
}

tchar * CSFileText::_ReadString( tchar * pBuffer, const int sizemax )
{
    // Read a line of text. nullptr/nullptr = EOF
    ADDTOCALLSTACK("CSFileText::_ReadString");
    ASSERT(pBuffer);

    if ( _IsEOF() )
        return nullptr;	// LINUX will ASSERT if we read past end.

    return fgets( pBuffer, sizemax, _pStream );
}

tchar * CSFileText::ReadString( tchar * pBuffer, const int sizemax )
{
    ADDTOCALLSTACK("CSFileText::ReadString");
    MT_UNIQUE_LOCK_RETURN(this, CSFileText::_ReadString(pBuffer, sizemax));
}

int CSFileText::_VPrintf(const lpctstr pFormat, const va_list args )
{
    ADDTOCALLSTACK("CSFileText::_VPrintf");
    ASSERT(pFormat);

    if ( !_IsFileOpen() )
        return -1;

    return vfprintf( _pStream, pFormat, args );
}

int CSFileText::VPrintf(const lpctstr pFormat, const va_list args)
{
    ADDTOCALLSTACK("CSFileText::VPrintf");
    ASSERT(pFormat);

    MT_UNIQUE_LOCK_RETURN(this, CSFileText::_VPrintf(pFormat, args));
}

bool CSFileText::_Write( const void * pData, const int iLen )
{
    // RETURN: 1 = success else fail.
    ADDTOCALLSTACK("CSFileText::_Write");
    ASSERT(pData);

    if ( !_IsFileOpen() )
        return false;

#ifdef _WIN32 // Windows flushing, the only safe mode to cancel it ;)
    if ( !_fNoBuffer )
    {
        setvbuf(_pStream, nullptr, _IONBF, 0);
        _fNoBuffer = true;
    }
#endif
    const size_t uiStatus = fwrite( pData, iLen, 1, _pStream );
#ifndef _WIN32	// However, in unix, it works
    fflush( _pStream );
#endif
    return ( uiStatus == 1 );
}

bool CSFileText::Write(const void* pData, const int iLen)
{
    // RETURN: 1 = success else fail.
    ADDTOCALLSTACK("CSFileText::Write");
    MT_UNIQUE_LOCK_RETURN(this, CSFileText::_Write(pData, iLen));
}

bool CSFileText::_WriteString(const lpctstr pStr )
{
    // RETURN: < 0 = failed.
    ADDTOCALLSTACK("CSFileText::_WriteString");
    ASSERT(pStr);

    return _Write( pStr, static_cast<int>(strlen(pStr)) );
}

bool CSFileText::WriteString(const lpctstr pStr)
{
    ADDTOCALLSTACK("CSFileText::WriteString");
    MT_UNIQUE_LOCK_RETURN(this, CSFileText::_WriteString(pStr));
}

// CSFileText:: Mode operations.

lpctstr CSFileText::_GetModeStr() const
{
    ADDTOCALLSTACK("CSFileText::_GetModeStr");
    // end of line translation is crap. ftell and fseek don't work correctly when you use it.
    // fopen() args
    if ( _IsBinaryMode())
        return ( _IsWriteMode() ? "wb" : "rb" );
    if ( _GetMode() & OF_READWRITE )
        return "a+b";
    if ( _GetMode() & OF_CREATE )
        return "w";
    if ( _IsWriteMode() )
        return "w";

    return "rb";
    // don't parse out the \n\r
}
