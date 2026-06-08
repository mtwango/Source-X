/**
* @file CSWindow.cpp
* @brief Base window class for controls.
*/

#ifdef _WIN32

#include "CSWindow.h"


/* CSWindow */

CSWindow::operator HWND () const       // cast as a HWND
{
    return m_hWnd;
}
CSWindow::CSWindow() : m_pnid{}
{
    m_hWnd = nullptr;
}
CSWindow::~CSWindow()
{
    DestroyWindow();
}

// Standard message handlers.
BOOL CSWindow::OnCreate(const HWND hwnd, const LPCREATESTRUCT lpCreateStruct)
{
    UnreferencedParameter(lpCreateStruct);
    m_hWnd = hwnd;
    return true;
}

void CSWindow::OnDestroy()
{
    m_hWnd = nullptr;
}

void CSWindow::OnDestroy(const HWND hwnd )
{
    UnreferencedParameter(hwnd);
    m_hWnd = nullptr;
}

// Basic window functions.
BOOL CSWindow::IsWindow() const
{
    if (m_hWnd == nullptr)
        return false;
    return ::IsWindow(m_hWnd);
}

HWND CSWindow::GetParent() const
{
    ASSERT(m_hWnd);
    return ::GetParent(m_hWnd);
}

LRESULT CSWindow::SendMessage(const UINT uMsg, const WPARAM wParam, const LPARAM lParam) const
{
    ASSERT(m_hWnd);
    return ::SendMessage(m_hWnd, uMsg, wParam, lParam);
}

BOOL CSWindow::PostMessage(const UINT uMsg, const WPARAM wParam, const LPARAM lParam) const
{
    ASSERT(m_hWnd);
    return ::PostMessage(m_hWnd, uMsg, wParam, lParam);
}

HWND CSWindow::GetDlgItem(const int id) const
{
    ASSERT(m_hWnd);
    return ::GetDlgItem(m_hWnd, id);
}

BOOL CSWindow::SetDlgItemText(const int nIDDlgItem, const LPCTSTR lpString)
{
    ASSERT(m_hWnd);
    return ::SetDlgItemText(m_hWnd, nIDDlgItem, lpString);
}

// Create/Destroy
void CSWindow::DestroyWindow()
{
    if (m_hWnd == nullptr)
        return;
    ::DestroyWindow(m_hWnd);
    ASSERT(m_hWnd == nullptr);
}

// Area and location
BOOL CSWindow::MoveWindow(const int X, const int Y, const int nWidth, const int nHeight, const BOOL bRepaint)
{
    ASSERT(m_hWnd);
    return ::MoveWindow(m_hWnd, X, Y, nWidth, nHeight, bRepaint);
}

BOOL CSWindow::SetForegroundWindow()
{
    ASSERT(m_hWnd);
    return ::SetForegroundWindow(m_hWnd);
}

HWND CSWindow::SetFocus()
{
    ASSERT(m_hWnd);
    return ::SetFocus(m_hWnd);
}

BOOL CSWindow::ShowWindow(const int nCmdShow)
{
    // SW_SHOW
    ASSERT(m_hWnd);
    return ::ShowWindow(m_hWnd, nCmdShow);
}

// Standard windows props.
int CSWindow::GetWindowText(const LPTSTR lpszText, const int iLen)
{
    ASSERT(m_hWnd);
    return ::GetWindowText(m_hWnd, lpszText, iLen);
}

BOOL CSWindow::SetWindowText(const LPCTSTR lpszText)
{
    ASSERT(m_hWnd);
    return ::SetWindowText(m_hWnd, lpszText);
}

void CSWindow::SetFont(HFONT hFont, const BOOL fRedraw)
{
    SendMessage(WM_SETFONT, reinterpret_cast<WPARAM>(hFont), MAKELPARAM(fRedraw, 0));
}

HICON CSWindow::SetIcon(HICON hIcon, const BOOL fType)
{
    // ICON_BIG vs ICON_SMALL
    return reinterpret_cast<HICON>(SendMessage(WM_SETICON, static_cast<WPARAM>(fType), reinterpret_cast<LPARAM>(hIcon)));
}

UINT_PTR CSWindow::SetTimer(const UINT_PTR uTimerID, const UINT uWaitmSec)
{
    ASSERT(m_hWnd);
    return ::SetTimer(m_hWnd, uTimerID, uWaitmSec, nullptr);
}

BOOL CSWindow::KillTimer(const UINT_PTR uTimerID)
{
    ASSERT(m_hWnd);
    return ::KillTimer(m_hWnd, uTimerID);
}

int CSWindow::MessageBox(const lpctstr lpszText, const lpctstr lpszTitle, const UINT fuStyle) const
{
    // ASSERT( m_hWnd ); ok for this to be nullptr !
    return ::MessageBox(m_hWnd, lpszText, lpszTitle, fuStyle);
}

INT_PTR CSWindow::SetWindowLongPtr(const int nIndex, const INT_PTR dwNewLong)
{
    ASSERT(m_hWnd);
    return ::SetWindowLongPtr(m_hWnd, nIndex, dwNewLong);
}

INT_PTR CSWindow::GetWindowLongPtr(const int nIndex) const
{
    ASSERT(m_hWnd);
    return ::GetWindowLongPtr(m_hWnd, nIndex);
}

int CSWindow::SetDlgItemText(const int ID, const lpctstr lpszText) const
{
    return ::SetDlgItemText(m_hWnd, ID, lpszText);
}


/* CDialogBase */

INT_PTR CALLBACK CDialogBase::DialogProc(const HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam ) // static
{
    CDialogBase * pDlg;
    if ( message == WM_INITDIALOG )
    {
        pDlg = reinterpret_cast<CDialogBase *>(lParam);
        ASSERT( pDlg );
        pDlg->m_hWnd = hWnd;	// OnCreate()
        pDlg->SetWindowLongPtr( GWLP_USERDATA, reinterpret_cast<INT_PTR>(pDlg) );
    }
    else
    {
        pDlg = static_cast<CDialogBase *>(reinterpret_cast<LPVOID>(::GetWindowLongPtr(hWnd, GWLP_USERDATA)));
    }
    if ( pDlg )
    {
        return pDlg->DefDialogProc( message, wParam, lParam );
    }

    return FALSE;
}

BOOL CDialogBase::DefDialogProc(const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    UnreferencedParameter(message);
    UnreferencedParameter(wParam);
    UnreferencedParameter(lParam);
    return FALSE;
}

/* CSWindowBase */

ATOM CSWindowBase::RegisterClass( WNDCLASS & wc )	// static
{
    wc.lpfnWndProc = WndProc;
    return ::RegisterClass( &wc );
}

LRESULT WINAPI CSWindowBase::WndProc(const HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam ) // static
{
    // NOTE: It is important to call OnDestroy() for asserts to work.
    CSWindowBase * pWnd;
    if ( message == WM_NCCREATE || message == WM_CREATE )
    {
        const auto *const lpCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        ASSERT(lpCreateStruct);
        pWnd = static_cast<CSWindowBase *>(lpCreateStruct->lpCreateParams);
        ASSERT( pWnd );
        pWnd->m_hWnd = hWnd;	// OnCreate()
        pWnd->SetWindowLongPtr(GWLP_USERDATA, reinterpret_cast<INT_PTR>(pWnd));
    }
    pWnd = static_cast<CSWindowBase *>(reinterpret_cast<LPVOID>(::GetWindowLongPtr(hWnd, GWLP_USERDATA)));
    return ( pWnd ? pWnd->DefWindowProc(message, wParam, lParam) : ::DefWindowProc(hWnd, message, wParam, lParam) );
}


/* CWinApp */

CWinApp::CWinApp()
{
    m_pszAppName = "";
    m_hInstance = nullptr;
    m_lpCmdLine = nullptr;
    m_pMainWnd = nullptr;
}

void CWinApp::InitInstance(const LPCTSTR pszAppName, const HINSTANCE hInstance, const LPTSTR lpszCmdLine)
{
    m_pszAppName = pszAppName;	// assume this is a static data pointer valid forever.
    m_hInstance	= hInstance;
    m_lpCmdLine	= lpszCmdLine;

    char szFileName[SPHERE_MAX_PATH];
    if (! GetModuleFileName(m_hInstance, szFileName, sizeof(szFileName)))
        return;
    m_pszExeName = szFileName;

    auto *pszTmp = const_cast<LPTSTR>(strrchr(m_pszExeName, '\\'));	// Get title
    lstrcpy(szFileName, (pszTmp == nullptr) ? m_pszExeName : (pszTmp + 1));
    pszTmp = strrchr(szFileName, '.');	// Get extension.
    if (pszTmp != nullptr)
        pszTmp[0] = '\0';
    lstrcat(szFileName, ".INI");

    OFSTRUCT ofs = { };
    if (OpenFile(szFileName, &ofs, OF_EXIST) != HFILE_ERROR)
    {
        m_pszProfileName = ofs.szPathName;
    }
    else
    {
        m_pszProfileName = szFileName;
    }
}

HICON CWinApp::LoadIcon(const int id) const
{
    return ::LoadIcon(m_hInstance, MAKEINTRESOURCE(id));
}
HMENU CWinApp::LoadMenu(const int id) const
{
    return ::LoadMenu(m_hInstance, MAKEINTRESOURCE(id));
}


/* CScrollBar */

// Attributes
void CScrollBar::GetScrollRange(const LPINT lpMinPos, const LPINT lpMaxPos) const
{
    ASSERT(IsWindow());
    ::GetScrollRange(m_hWnd, SB_CTL, lpMinPos, lpMaxPos);
}
BOOL CScrollBar::GetScrollInfo(const LPSCROLLINFO lpScrollInfo, const UINT nMask)
{
    lpScrollInfo->cbSize = sizeof(*lpScrollInfo);
    lpScrollInfo->fMask = nMask;
    return ::GetScrollInfo(m_hWnd, SB_CTL, lpScrollInfo);
}


/* CEdit */

// Operations

void CEdit::SetSel(const DWORD dwSelection, const BOOL bNoScroll)
{
    UnreferencedParameter(bNoScroll);
    ASSERT(IsWindow());
    SendMessage(EM_SETSEL, dwSelection, dwSelection);
}
void CEdit::SetSel(const size_t nStartChar, const size_t nEndChar, const BOOL bNoScroll)
{
    UnreferencedParameter(bNoScroll);
    ASSERT(IsWindow());
    SendMessage(EM_SETSEL, nStartChar, static_cast<LPARAM>(nEndChar));
}
size_t CEdit::GetSel() const
{
    ASSERT(IsWindow());
    return static_cast<size_t>(SendMessage(EM_GETSEL));
}
void CEdit::GetSel(size_t& nStartChar, size_t& nEndChar) const
{
    ASSERT(IsWindow());
    size_t nSelection = GetSel();
    nStartChar = LOWORD(nSelection);
    nEndChar = HIWORD(nSelection);
}

void CEdit::ReplaceSel(lpctstr lpszNewText, const BOOL bCanUndo)
{
    ASSERT(IsWindow());
    SendMessage(EM_REPLACESEL, static_cast<WPARAM>(bCanUndo), reinterpret_cast<LPARAM>(lpszNewText));
}


/* CRichEditCtrl */

COLORREF CRichEditCtrl::SetBackgroundColor(const BOOL bSysColor, const COLORREF cr)
{
    return static_cast<DWORD>(SendMessage(EM_SETBKGNDCOLOR, static_cast<WPARAM>(bSysColor), cr));
}

void CRichEditCtrl::SetSel(const int nStartChar, const int nEndChar)
{
    ASSERT(IsWindow());
    CHARRANGE range;
    range.cpMin = nStartChar;
    range.cpMax = nEndChar;
    SendMessage(EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
}
void CRichEditCtrl::GetSel(int& nStartChar, int& nEndChar) const
{
    ASSERT(IsWindow());
    CHARRANGE range;
    SendMessage(EM_EXGETSEL, 0, reinterpret_cast<LPARAM>(&range));
    nStartChar = range.cpMin;
    nEndChar = range.cpMax;
}

void CRichEditCtrl::SetRedraw(const BOOL val)
{
    ASSERT(IsWindow());
    SendMessage(WM_SETREDRAW, static_cast<WPARAM>(val), 0);
}

void CRichEditCtrl::SetCaretHide(const BOOL val)
{
    ASSERT(IsWindow());
    if (val)
        HideCaret(m_hWnd);
    else
        ShowCaret(m_hWnd);
}

DWORD CRichEditCtrl::ScrollLine()
{
    return static_cast<DWORD>(PostMessage(EM_SCROLL, SB_LINEDOWN));
}

DWORD CRichEditCtrl::ScrollPageDown()
{
    return static_cast<DWORD>(PostMessage(EM_SCROLL, SB_PAGEDOWN));
}

DWORD CRichEditCtrl::ScrollBottomRight()
{
    return static_cast<DWORD>(PostMessage(WM_VSCROLL, SB_BOTTOM));
}

// Formatting.
BOOL CRichEditCtrl::SetDefaultCharFormat(CHARFORMAT& cf)
{
    return static_cast<BOOL>(static_cast<DWORD>(SendMessage(EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&cf))));
}

BOOL CRichEditCtrl::SetSelectionCharFormat(CHARFORMAT& cf)
{
    return static_cast<BOOL>(static_cast<DWORD>(SendMessage(EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&cf))));
}

// Events.
int CRichEditCtrl::GetEventMask() const
{
    return static_cast<DWORD>(SendMessage(EM_GETEVENTMASK));
}

DWORD CRichEditCtrl::SetEventMask(const DWORD dwEventMask)
{
    // ENM_NONE = default.
    return static_cast<DWORD>(SendMessage(EM_SETEVENTMASK, 0, dwEventMask));
}


/* CListbox */

void CListbox::ResetContent()
{
    ASSERT(IsWindow());
    SendMessage(LB_RESETCONTENT);
}
int CListbox::GetCount() const
{
    return static_cast<int>(static_cast<DWORD>(SendMessage(LB_GETCOUNT)));
}
int CListbox::AddString(LPCTSTR lpsz) const
{
    return static_cast<int>(static_cast<DWORD>(SendMessage(LB_ADDSTRING, 0L, reinterpret_cast<LPARAM>(lpsz))));
}


#endif	// _WIN32
