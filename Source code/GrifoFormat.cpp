// ============================================================================
//  GrifoFormat  -  Formateador de memorias USB
//  Creador: xyvenqorix
//  Compilar con Dev-C++ (MinGW). NO necesita librerias ni flags extra.
// ============================================================================
#define UNICODE
#define _UNICODE
#define _WIN32_IE 0x0500
#define _WIN32_WINNT 0x0501
#include <windows.h>
#include <shlobj.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

// ---------------------------- DATOS DEL CREADOR -----------------------------
#define CREATOR_NAME   L"xyvenqorix"
#define CREATOR_PHONE  L"+53 56639178"
#define CREATOR_WEB    L"https://xyvenqorix.vercel.app"
#define CREATOR_WEBTXT L"xyvenqorix.vercel.app"
// >>> PEGA AQUI TU DIRECCION USDT (POLYGON) <<<
#define USDT_ADDR      L"0x57b2268e875B4555f2bF2dE6EE09196a9E17C55f"
#define APP_VERSION    L"1.0"
// ----------------------------------------------------------------------------


// ---------- gdi32 se carga en tiempo de ejecucion (no hace falta enlazar nada) ----------
typedef HBRUSH (WINAPI *F_CSB)(COLORREF);
typedef HPEN (WINAPI *F_CP)(int, int, COLORREF);
typedef HGDIOBJ (WINAPI *F_SO)(HDC, HGDIOBJ);
typedef BOOL (WINAPI *F_DO)(HGDIOBJ);
typedef BOOL (WINAPI *F_RR)(HDC, int, int, int, int, int, int);
typedef BOOL (WINAPI *F_PG)(HDC, const POINT*, int);
typedef int (WINAPI *F_SBM)(HDC, int);
typedef COLORREF (WINAPI *F_SC)(HDC, COLORREF);
typedef BOOL (WINAPI *F_TO)(HDC, int, int, LPCWSTR, int);
typedef BOOL (WINAPI *F_MT)(HDC, int, int, LPPOINT);
typedef BOOL (WINAPI *F_LT)(HDC, int, int);
typedef HFONT (WINAPI *F_CF)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCWSTR);
static F_CSB pCreateSolidBrush; static F_CP pCreatePen; static F_SO pSelectObject; static F_DO pDeleteObject;
static F_RR pRoundRect; static F_PG pPolygon; static F_SBM pSetBkMode; static F_SC pSetTextColor, pSetBkColor;
static F_TO pTextOutW; static F_MT pMoveToEx; static F_LT pLineTo; static F_CF pCreateFontW;
#define CreateSolidBrush pCreateSolidBrush
#define CreatePen pCreatePen
#define SelectObject pSelectObject
#define DeleteObject pDeleteObject
#define RoundRect pRoundRect
#define Polygon pPolygon
#define SetBkMode pSetBkMode
#define SetTextColor pSetTextColor
#define SetBkColor pSetBkColor
#define TextOutW pTextOutW
#define MoveToEx pMoveToEx
#define LineTo pLineTo
#define CreateFontW pCreateFontW
static bool LoadGdi()
{
    HMODULE g = LoadLibraryW(L"gdi32.dll");
    if (!g) return false;
    pCreateSolidBrush = (F_CSB)GetProcAddress(g, "CreateSolidBrush");
    pCreatePen = (F_CP)GetProcAddress(g, "CreatePen");
    pSelectObject = (F_SO)GetProcAddress(g, "SelectObject");
    pDeleteObject = (F_DO)GetProcAddress(g, "DeleteObject");
    pRoundRect = (F_RR)GetProcAddress(g, "RoundRect");
    pPolygon = (F_PG)GetProcAddress(g, "Polygon");
    pSetBkMode = (F_SBM)GetProcAddress(g, "SetBkMode");
    pSetTextColor = (F_SC)GetProcAddress(g, "SetTextColor");
    pSetBkColor = (F_SC)GetProcAddress(g, "SetBkColor");
    pTextOutW = (F_TO)GetProcAddress(g, "TextOutW");
    pMoveToEx = (F_MT)GetProcAddress(g, "MoveToEx");
    pLineTo = (F_LT)GetProcAddress(g, "LineTo");
    pCreateFontW = (F_CF)GetProcAddress(g, "CreateFontW");
    return pCreateSolidBrush && pCreatePen && pSelectObject && pDeleteObject && pRoundRect && pPolygon &&
           pSetBkMode && pSetTextColor && pSetBkColor && pTextOutW && pMoveToEx && pLineTo && pCreateFontW;
}

#define IDI_MAIN 100
enum { IDC_DRIVE = 101, IDC_FS, IDC_LABEL, IDC_QUICK, IDC_BKFIRST, IDC_REFRESH,
       IDC_BACKUP, IDC_FORMAT, IDC_ABOUT, IDC_PROG, IDC_STATUS, IDC_NOTE };
enum { IDA_WEB = 201, IDA_PHONE, IDA_USDT, IDA_CLOSE };

#define WM_PROG (WM_APP + 1)
#define WM_STAT (WM_APP + 2)
#define WM_DONE (WM_APP + 3)

#define IOCTL_LEN   0x7405C   // IOCTL_DISK_GET_LENGTH_INFO
#define IOCTL_PARTX 0x70048   // IOCTL_DISK_GET_PARTITION_INFO_EX
#define IOCTL_STORQ 0x2D1400  // IOCTL_STORAGE_QUERY_PROPERTY

static HINSTANCE g_inst;
static HWND g_hwnd;
static HFONT fUI, fLabel, fTitle, fSub, fMono, fBig;
static HBRUSH brBg, brPanel;
static HICON g_icoBig, g_icoSmall;
static const COLORREF cBg = RGB(13, 15, 26), cPanel = RGB(24, 27, 46),
                      cGold = RGB(212, 175, 55), cText = RGB(232, 226, 208),
                      cMuted = RGB(150, 146, 130);

static volatile bool g_busy = false;
static int g_pos = 0;
static void SetProg(int p)
{
    g_pos = p < 0 ? 0 : (p > 100 ? 100 : p);
    HWND c = GetDlgItem(g_hwnd, IDC_PROG);
    if (c) InvalidateRect(c, NULL, FALSE);
}
static void DrawProg(DRAWITEMSTRUCT* d)
{
    RECT r = d->rcItem;
    HBRUSH b1 = CreateSolidBrush(cPanel);
    FillRect(d->hDC, &r, b1);
    DeleteObject(b1);
    r.right = r.left + (r.right - r.left) * g_pos / 100;
    HBRUSH b2 = CreateSolidBrush(cGold);
    FillRect(d->hDC, &r, b2);
    DeleteObject(b2);
}
static wchar_t g_letter;
static int g_fsIdx, g_op;           // g_op: 1 = solo respaldo, 2 = formatear
static wchar_t g_label[40];
static bool g_quick, g_bkFirst;
static wchar_t g_backupDir[MAX_PATH * 2];
static wchar_t g_backupBase[MAX_PATH * 2];   // carpeta elegida al momento de respaldar

// ============================== UTILIDADES ==================================
static void PostText(UINT m, WPARAM w, const wchar_t* s)
{
    wchar_t* c = s ? _wcsdup(s) : NULL;
    PostMessageW(g_hwnd, m, w, (LPARAM)c);
}
static void Status(const wchar_t* s) { PostText(WM_STAT, 0, s); }
static void Progress(int p) { PostMessageW(g_hwnd, WM_PROG, (WPARAM)p, 0); }

static HICON LoadAppIcon(int size)
{
    HICON h = (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(IDI_MAIN), IMAGE_ICON, size, size, LR_DEFAULTCOLOR);
    if (!h) h = (HICON)LoadImageW(NULL, L"grifo.ico", IMAGE_ICON, size, size, LR_LOADFROMFILE);
    return h;
}

static void CopyToClipboard(HWND owner, const wchar_t* t)
{
    if (!OpenClipboard(owner)) return;
    EmptyClipboard();
    size_t n = (wcslen(t) + 1) * sizeof(wchar_t);
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, n);
    if (g) {
        memcpy(GlobalLock(g), t, n);
        GlobalUnlock(g);
        SetClipboardData(CF_UNICODETEXT, g);
    }
    CloseClipboard();
}

// Informacion de la unidad: tamano, tipo de bus (7 = USB) y modelo
static bool DriveData(wchar_t L, ULONGLONG* size, int* bus, wchar_t* model, int mlen)
{
    wchar_t p[16];
    _snwprintf(p, 16, L"\\\\.\\%lc:", L);
    HANDLE h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD br = 0;
    LONGLONG len = 0;
    if (!DeviceIoControl(h, IOCTL_LEN, NULL, 0, &len, 8, &br, NULL)) len = 0;
    *size = (ULONGLONG)len;
    *bus = 0;
    model[0] = 0;
    struct { DWORD id, qt; BYTE ap[4]; } q = { 0, 0, { 0, 0, 0, 0 } };
    BYTE buf[1024];
    memset(buf, 0, sizeof(buf));
    if (DeviceIoControl(h, IOCTL_STORQ, &q, sizeof(q), buf, sizeof(buf), &br, NULL)) {
        *bus = (int)*(DWORD*)(buf + 28);
        DWORD po = *(DWORD*)(buf + 16);
        if (po && po < sizeof(buf) - 2) {
            MultiByteToWideChar(CP_ACP, 0, (char*)buf + po, -1, model, mlen);
            int n = (int)wcslen(model);
            while (n > 0 && model[n - 1] == L' ') model[--n] = 0;
        }
    }
    CloseHandle(h);
    return true;
}

static void Dismount(wchar_t L)
{
    wchar_t p[16];
    _snwprintf(p, 16, L"\\\\.\\%lc:", L);
    HANDLE h = CreateFileW(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD br;
    DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &br, NULL);
    CloseHandle(h);
}

static int CALLBACK BrowseCb(HWND h, UINT m, LPARAM, LPARAM data)
{
    if (m == BFFM_INITIALIZED && data) SendMessageW(h, BFFM_SETSELECTIONW, TRUE, data);
    return 0;
}

// Pregunta donde guardar la copia de seguridad. Devuelve false si se cancela.
static bool AskBackupDir()
{
    static bool oleInit = false;
    if (!oleInit) {   // necesario para el dialogo moderno (con boton "Crear carpeta")
        oleInit = true;
        HMODULE o = LoadLibraryW(L"ole32.dll");
        typedef HRESULT (WINAPI *F_OI)(LPVOID);
        F_OI oi = o ? (F_OI)GetProcAddress(o, "OleInitialize") : NULL;
        if (oi) oi(NULL);
    }
    static wchar_t start[MAX_PATH];
    start[0] = 0;
    SHGetSpecialFolderPathW(NULL, start, CSIDL_PERSONAL, FALSE);
    wchar_t disp[MAX_PATH];
    BROWSEINFOW bi;
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = g_hwnd;
    bi.pszDisplayName = disp;
    bi.lpszTitle = L"Elige la carpeta donde guardar la copia de seguridad:";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | 0x0040;   // 0x40 = BIF_NEWDIALOGSTYLE
    bi.lpfn = BrowseCb;
    bi.lParam = (LPARAM)start;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return false;
    bool ok = SHGetPathFromIDListW(pidl, g_backupBase) != 0;
    IMalloc* im = NULL;
    if (SUCCEEDED(SHGetMalloc(&im)) && im) { im->Free(pidl); im->Release(); }
    return ok;
}

static void RefreshDrives()
{
    HWND cb = GetDlgItem(g_hwnd, IDC_DRIVE);
    int sel = (int)SendMessageW(cb, CB_GETCURSEL, 0, 0);
    wchar_t keep = 0;
    if (sel >= 0) keep = (wchar_t)SendMessageW(cb, CB_GETITEMDATA, sel, 0);
    SendMessageW(cb, CB_RESETCONTENT, 0, 0);

    wchar_t win[MAX_PATH];
    GetWindowsDirectoryW(win, MAX_PATH);
    DWORD mask = GetLogicalDrives();
    int idx = -1, count = 0;
    for (int i = 0; i < 26; i++) {
        if (!(mask & (1u << i))) continue;
        wchar_t L = (wchar_t)(L'A' + i);
        if (L == (wchar_t)towupper(win[0])) continue;           // nunca el disco de Windows
        wchar_t root[4] = { L, L':', L'\\', 0 };
        UINT t = GetDriveTypeW(root);
        if (t == DRIVE_REMOTE || t == DRIVE_CDROM || t == DRIVE_RAMDISK) continue;
        ULONGLONG sz;
        int bus;
        wchar_t model[64];
        if (!DriveData(L, &sz, &bus, model, 64)) continue;
        if (t != DRIVE_REMOVABLE && bus != 7) continue;          // solo USB / removibles
        if (sz == 0) continue;
        wchar_t vol[64] = L"";
        GetVolumeInformationW(root, vol, 64, NULL, NULL, NULL, NULL, 0);
        wchar_t text[256];
        _snwprintf(text, 256, L"%lc:   %ls   %ls   (%.1f GB)", L,
                   vol[0] ? vol : L"Sin nombre", model[0] ? model : L"USB", (double)sz / 1073741824.0);
        int ix = (int)SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)text);
        SendMessageW(cb, CB_SETITEMDATA, ix, (LPARAM)L);
        if (L == keep) idx = ix;
        count++;
    }
    if (count) SendMessageW(cb, CB_SETCURSEL, idx >= 0 ? idx : 0, 0);
    SetDlgItemTextW(g_hwnd, IDC_STATUS, count ? L"Listo. Elige una unidad y un formato."
                                              : L"No se detectaron memorias USB. Conecta una y pulsa Actualizar.");
}

// ============================== RESPALDO ====================================
static ULONGLONG g_n, g_total, g_bytes;
static int g_fail;
static bool g_countOnly;

static void Walk(const wchar_t* src, const wchar_t* dst)
{
    wchar_t pat[1100];
    _snwprintf(pat, 1100, L"%ls\\*", src);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        wchar_t s[1100], d[1100];
        _snwprintf(s, 1100, L"%ls\\%ls", src, fd.cFileName);
        _snwprintf(d, 1100, L"%ls\\%ls", dst, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
            if (!_wcsicmp(fd.cFileName, L"System Volume Information") || !_wcsicmp(fd.cFileName, L"$RECYCLE.BIN")) continue;
            if (!g_countOnly) CreateDirectoryW(d, NULL);
            Walk(s, d);
        } else if (g_countOnly) {
            g_total++;
            g_bytes += ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
        } else {
            if (!CopyFileW(s, d, FALSE)) g_fail++;
            g_n++;
            if (g_total) Progress((int)(g_n * 100 / g_total));
            if ((g_n % 8) == 0 || g_n == 1) {
                wchar_t m[300];
                _snwprintf(m, 300, L"Respaldando %I64u de %I64u:  %ls", g_n, g_total, fd.cFileName);
                Status(m);
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static bool DoBackup(wchar_t* msg)
{
    wchar_t base[MAX_PATH * 2];
    wcsncpy(base, g_backupBase, MAX_PATH * 2 - 1);
    base[MAX_PATH * 2 - 1] = 0;
    int bl = (int)wcslen(base);
    while (bl > 0 && (base[bl - 1] == L'\\' || base[bl - 1] == L'/')) base[--bl] = 0;
    if (!base[0]) {
        wcscpy(msg, L"No hay una carpeta de respaldo valida.");
        return false;
    }
    if (base[1] == L':' && towupper(base[0]) == towupper(g_letter)) {
        wcscpy(msg, L"La carpeta de respaldo esta dentro de la propia memoria USB. Elige una carpeta en tu PC u otro disco.");
        return false;
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snwprintf(g_backupDir, MAX_PATH * 2, L"%ls\\GrifoFormat_USB_%lc_%04d%02d%02d_%02d%02d%02d", base, g_letter,
               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    if (!CreateDirectoryW(g_backupDir, NULL)) {
        wcscpy(msg, L"No pude crear la carpeta de respaldo (revisa permisos).");
        return false;
    }
    wchar_t src[4] = { g_letter, L':', 0, 0 };

    Status(L"Analizando archivos de la memoria...");
    g_countOnly = true; g_total = 0; g_bytes = 0; g_n = 0; g_fail = 0;
    Walk(src, g_backupDir);

    ULARGE_INTEGER avail, tot, fr;
    if (GetDiskFreeSpaceExW(base, &avail, &tot, &fr) && avail.QuadPart < g_bytes + 52428800ULL) {
        _snwprintf(msg, 300, L"No hay espacio suficiente en el destino para el respaldo (se necesitan %.1f GB).",
                   (double)g_bytes / 1073741824.0);
        return false;
    }
    g_countOnly = false;
    Walk(src, g_backupDir);
    Progress(100);
    if (g_fail) {
        _snwprintf(msg, 300, L"El respaldo termino con %d archivo(s) que no se pudieron copiar.\nCarpeta: %ls", g_fail, g_backupDir);
        return false;
    }
    _snwprintf(msg, 300, L"Respaldo completo: %I64u archivos guardados en:\n%ls", g_n, g_backupDir);
    return true;
}

// ============================== FORMATEO ====================================
static bool WriteAt(HANDLE h, ULONGLONG off, const void* buf, DWORD len)
{
    LARGE_INTEGER li;
    li.QuadPart = (LONGLONG)off;
    if (!SetFilePointerEx(h, li, NULL, FILE_BEGIN)) return false;
    DWORD w = 0;
    return WriteFile(h, buf, len, &w, NULL) && w == len;
}
static void P16(BYTE* b, int o, DWORD v) { b[o] = (BYTE)v; b[o + 1] = (BYTE)(v >> 8); }
static void P32(BYTE* b, int o, DWORD v) { P16(b, o, v & 0xFFFF); P16(b, o + 2, v >> 16); }

static ULONGLONG zwDone, zwTotal;
static bool ZeroRange(HANDLE h, DWORD bps, ULONGLONG startSec, ULONGLONG count)
{
    DWORD chunk = (1 << 20) / bps;
    if (!chunk) chunk = 1;
    BYTE* z = (BYTE*)VirtualAlloc(NULL, (SIZE_T)chunk * bps, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!z) return false;
    bool ok = true;
    int last = -1;
    while (count && ok) {
        DWORD n = count > chunk ? chunk : (DWORD)count;
        ok = WriteAt(h, startSec * bps, z, n * bps);
        startSec += n; count -= n; zwDone += n;
        int p = zwTotal ? (int)(zwDone * 100 / zwTotal) : 0;
        if (p > 99) p = 99;
        if (p != last) { last = p; Progress(p); }
    }
    VirtualFree(z, 0, MEM_RELEASE);
    return ok;
}

// ---- Formateador FAT32 propio (Windows se niega a hacer FAT32 en >32 GB) ----
static bool Fat32Format(wchar_t L, const wchar_t* label, bool quick, wchar_t* err)
{
    wchar_t p[16];
    _snwprintf(p, 16, L"\\\\.\\%lc:", L);
    HANDLE h = CreateFileW(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL);
    if (h == INVALID_HANDLE_VALUE) { wcscpy(err, L"No pude abrir la unidad. Cierra ventanas que la usen."); return false; }
    DWORD br;
    bool locked = false;
    for (int i = 0; i < 10 && !locked; i++) {
        locked = DeviceIoControl(h, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &br, NULL) != 0;
        if (!locked) Sleep(500);
    }
    if (!locked) { wcscpy(err, L"La unidad esta en uso por otro programa. Cierralo e intenta de nuevo."); CloseHandle(h); return false; }
    DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &br, NULL);

    DISK_GEOMETRY g;
    DWORD bps = 512;
    if (DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &g, sizeof(g), &br, NULL) && g.BytesPerSector) bps = g.BytesPerSector;
    BYTE pi[256];
    memset(pi, 0, sizeof(pi));
    LONGLONG start = 0, len = 0;
    if (DeviceIoControl(h, IOCTL_PARTX, NULL, 0, pi, sizeof(pi), &br, NULL)) {
        start = *(LONGLONG*)(pi + 8);
        len = *(LONGLONG*)(pi + 16);
    }
    if (len <= 0 && !DeviceIoControl(h, IOCTL_LEN, NULL, 0, &len, 8, &br, NULL)) len = 0;

    DWORD eps = bps / 4;
    ULONGLONG total = (ULONGLONG)len / bps;
    bool ok = false;
    BYTE* s = NULL;
    BYTE* f = NULL;
    if (total < 70000) { wcscpy(err, L"La unidad es muy pequena para FAT32 (minimo ~35 MB)."); goto fin; }
    if (total > 0xFFFFFFFFULL) { wcscpy(err, L"FAT32 no soporta unidades de mas de 2 TB. Usa exFAT o NTFS."); goto fin; }
    {
        ULONGLONG mb = (ULONGLONG)len >> 20;
        DWORD cb;
        if (mb <= 260) cb = 512; else if (mb <= 8192) cb = 4096; else if (mb <= 16384) cb = 8192;
        else if (mb <= 32768) cb = 16384; else cb = 32768;
        DWORD spc = cb / bps;
        if (!spc) spc = 1;
        const DWORD rsvd = 32;
        DWORD fatSz = 0;
        ULONGLONG clusters = 0;
        for (;;) {
            fatSz = (DWORD)(((total - rsvd) / spc + 2 + eps - 1) / eps);
            clusters = (total - rsvd - 2ULL * fatSz) / spc;
            if (clusters >= 65525 || spc == 1) break;
            spc /= 2;
        }
        if (clusters < 65525 || clusters > 0x0FFFFFF5ULL) { wcscpy(err, L"No se pudo calcular una estructura FAT32 valida para esta unidad."); goto fin; }

        s = (BYTE*)VirtualAlloc(NULL, bps, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        f = (BYTE*)VirtualAlloc(NULL, bps, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!s || !f) { wcscpy(err, L"Sin memoria."); goto fin; }

        char lab[12];
        memset(lab, ' ', 11); lab[11] = 0;
        bool hasLabel = false;
        for (int i = 0; i < 11 && label[i]; i++) {
            wchar_t c = label[i];
            lab[i] = (c < 128 && c >= 32) ? (char)towupper(c) : '_';
            hasLabel = true;
        }

        // sector de arranque
        memset(s, 0, bps);
        s[0] = 0xEB; s[1] = 0x58; s[2] = 0x90;
        memcpy(s + 3, "MSWIN4.1", 8);
        P16(s, 11, bps); s[13] = (BYTE)spc; P16(s, 14, rsvd); s[16] = 2; s[21] = 0xF8;
        P16(s, 24, 63); P16(s, 26, 255);
        P32(s, 28, (DWORD)((ULONGLONG)start / bps)); P32(s, 32, (DWORD)total); P32(s, 36, fatSz);
        P32(s, 44, 2); P16(s, 48, 1); P16(s, 50, 6);
        s[64] = 0x80; s[66] = 0x29; P32(s, 67, GetTickCount());
        memcpy(s + 71, hasLabel ? lab : "NO NAME    ", 11);
        memcpy(s + 82, "FAT32   ", 8);
        s[510] = 0x55; s[511] = 0xAA;

        // limpiar
        ULONGLONG head = (ULONGLONG)rsvd + 2ULL * fatSz + spc;
        zwDone = 0;
        zwTotal = quick ? head : total;
        if (!ZeroRange(h, bps, 0, zwTotal)) { wcscpy(err, L"Error de escritura al limpiar la unidad."); goto fin; }

        bool w = true;
        w &= WriteAt(h, 0, s, bps);
        w &= WriteAt(h, 6ULL * bps, s, bps);
        // FSInfo
        memset(f, 0, bps);
        P32(f, 0, 0x41615252); P32(f, 484, 0x61417272); P32(f, 488, (DWORD)(clusters - 1)); P32(f, 492, 3);
        f[510] = 0x55; f[511] = 0xAA;
        w &= WriteAt(h, 1ULL * bps, f, bps);
        w &= WriteAt(h, 7ULL * bps, f, bps);
        // tablas FAT
        memset(f, 0, bps);
        P32(f, 0, 0x0FFFFFF8); P32(f, 4, 0xFFFFFFFF); P32(f, 8, 0x0FFFFFFF);
        w &= WriteAt(h, (ULONGLONG)rsvd * bps, f, bps);
        w &= WriteAt(h, ((ULONGLONG)rsvd + fatSz) * bps, f, bps);
        // etiqueta en el directorio raiz
        if (hasLabel) {
            memset(f, 0, bps);
            memcpy(f, lab, 11); f[11] = 0x08;
            w &= WriteAt(h, ((ULONGLONG)rsvd + 2ULL * fatSz) * bps, f, bps);
        }
        FlushFileBuffers(h);
        if (!w) { wcscpy(err, L"Error de escritura al crear el sistema FAT32."); goto fin; }
        ok = true;
    }
fin:
    DeviceIoControl(h, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &br, NULL);
    CloseHandle(h);
    if (s) VirtualFree(s, 0, MEM_RELEASE);
    if (f) VirtualFree(f, 0, MEM_RELEASE);
    if (ok) {
        Progress(100);
        wchar_t root[4] = { L, L':', L'\\', 0 };
        SHChangeNotify(SHCNE_MEDIAINSERTED, SHCNF_PATH, root, NULL);
    }
    return ok;
}

// ---- exFAT / NTFS con el motor de Windows (fmifs.dll) ----
typedef BOOLEAN (WINAPI *PFMIFSCALLBACK)(int, DWORD, PVOID);
typedef VOID (WINAPI *PFORMATEX)(PWCHAR, int, PWCHAR, PWCHAR, BOOLEAN, ULONG, PFMIFSCALLBACK);
static volatile bool g_fmtOk, g_fmtDone, g_fmtInUse;

static BOOLEAN WINAPI FmtCb(int cmd, DWORD sub, PVOID info)
{
    (void)sub;
    if (cmd == 0 && info) Progress((int)*(DWORD*)info);        // PROGRESS
    else if (cmd == 8) g_fmtInUse = true;                       // VOLUMEINUSE
    else if (cmd == 0xB && info) { g_fmtOk = *(BOOLEAN*)info != 0; g_fmtDone = true; }  // DONE
    return TRUE;
}

// Plan B: usa format.com de Windows (/X fuerza el desmontaje aunque este "en uso")
static bool FormatComRun(wchar_t L, int fs, const wchar_t* label, bool quick)
{
    wchar_t sys[MAX_PATH], cmd[700], lab[40];
    GetSystemDirectoryW(sys, MAX_PATH);
    // etiqueta segura para la linea de comandos (sin espacios ni simbolos raros)
    int n = 0;
    for (int i = 0; label[i] && n < (fs == 1 ? 11 : 32); i++) {
        wchar_t c = label[i];
        if (iswalnum(c) || c == L'_' || c == L'-') lab[n++] = c;
        else if (c == L' ') lab[n++] = L'_';
    }
    lab[n] = 0;
    _snwprintf(cmd, 700, L"\"%ls\\format.com\" %lc: /FS:%ls %ls%ls%ls /X /Y%ls",
               sys, L, fs == 1 ? L"exFAT" : L"NTFS",
               n ? L"/V:" : L"", n ? lab : L"", L"", quick ? L" /Q" : L"");

    SECURITY_ATTRIBUTES sa;
    memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return false;
    SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = rd;
    si.hStdOutput = nul;
    si.hStdError = nul;
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    bool ok = false;
    if (CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        // por si pide "Presione ENTRAR" o la etiqueta actual, mandamos varios Enter
        DWORD w;
        const char* enters = "\r\n\r\n\r\n\r\n";
        WriteFile(wr, enters, 8, &w, NULL);
        CloseHandle(wr); wr = NULL;
        int tick = 0;
        while (WaitForSingleObject(pi.hProcess, 500) == WAIT_TIMEOUT) {
            tick++;
            Progress(tick < 90 ? tick : 90);      // progreso aproximado
        }
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        ok = (code == 0);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    if (wr) CloseHandle(wr);
    CloseHandle(rd);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    return ok;
}

static bool FormatExRun(wchar_t L, int fs, const wchar_t* label, bool quick, wchar_t* err)
{
    HMODULE m = LoadLibraryW(L"fmifs.dll");
    PFORMATEX fx = m ? (PFORMATEX)GetProcAddress(m, "FormatEx") : NULL;
    if (!fx) { wcscpy(err, L"No pude cargar el motor de formato de Windows (fmifs.dll)."); return false; }
    wchar_t root[4] = { L, L':', L'\\', 0 };
    wchar_t fsn[8], lab[40];
    wcscpy(fsn, fs == 1 ? L"exFAT" : L"NTFS");
    wcsncpy(lab, label, 32); lab[32] = 0;
    if (fs == 1 && wcslen(lab) > 11) lab[11] = 0;

    // Intento 1: motor de Windows, con reintentos (Explorer / antivirus / indexador
    // vuelven a montar la unidad justo despues de desmontarla y la dejan "en uso").
    g_fmtOk = false;
    for (int t = 0; t < 4 && !g_fmtOk; t++) {
        Dismount(L);
        Sleep(600);
        g_fmtDone = false; g_fmtOk = false; g_fmtInUse = false;
        fx(root, 12, fsn, lab, quick ? TRUE : FALSE, 0, FmtCb);
        if (g_fmtOk) break;
        Sleep(1000);
    }
    if (g_fmtOk) { Progress(100); return true; }

    // Intento 2: format.com con desmontaje forzado
    Status(L"Reintentando con el formateador de Windows (modo forzado)...");
    Progress(0);
    Dismount(L);
    Sleep(600);
    if (FormatComRun(L, fs, label, quick)) { Progress(100); return true; }

    wcscpy(err, g_fmtInUse
        ? L"La unidad sigue en uso. Cierra el Explorador de archivos y cualquier programa que use la memoria, "
          L"desconectala, vuelve a conectarla e intenta otra vez."
        : L"Windows no pudo formatear la unidad (puede estar protegida contra escritura o danada).");
    return false;
}

// ============================== HILO TRABAJADOR =============================
static DWORD WINAPI Worker(LPVOID)
{
    wchar_t msg[400] = L"";
    bool ok = true;
    if (g_op == 1 || g_bkFirst) {
        ok = DoBackup(msg);
        if (!ok && g_op == 2) {
            wchar_t tmp[400];
            _snwprintf(tmp, 400, L"%ls\n\nPor seguridad NO se formateo nada.", msg);
            wcscpy(msg, tmp);
        }
    }
    if (ok && g_op == 2) {
        Status(g_quick ? L"Formateando (rapido)..." : L"Formateando (completo, puede tardar mucho)...");
        Progress(0);
        wchar_t err[300] = L"";
        const wchar_t* names[3] = { L"FAT32", L"exFAT", L"NTFS" };
        ok = (g_fsIdx == 0) ? Fat32Format(g_letter, g_label, g_quick, err)
                            : FormatExRun(g_letter, g_fsIdx, g_label, g_quick, err);
        if (ok) _snwprintf(msg, 400, L"\u00a1Listo! La unidad %lc: fue formateada en %ls.", g_letter, names[g_fsIdx]);
        else wcscpy(msg, err);
    }
    PostText(WM_DONE, ok ? 1 : 0, msg);
    return 0;
}

// ============================== INTERFAZ ====================================
static void SetBusy(bool b)
{
    g_busy = b;
    int ids[] = { IDC_DRIVE, IDC_FS, IDC_LABEL, IDC_QUICK, IDC_BKFIRST, IDC_REFRESH, IDC_BACKUP, IDC_FORMAT, IDC_ABOUT };
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) EnableWindow(GetDlgItem(g_hwnd, ids[i]), !b);
}

static void StartOp(int op)
{
    if (g_busy) return;
    HWND cb = GetDlgItem(g_hwnd, IDC_DRIVE);
    int sel = (int)SendMessageW(cb, CB_GETCURSEL, 0, 0);
    if (sel < 0) { MessageBoxW(g_hwnd, L"Primero conecta y elige una memoria USB.", L"GrifoFormat", MB_OK | MB_ICONINFORMATION); return; }
    g_letter = (wchar_t)SendMessageW(cb, CB_GETITEMDATA, sel, 0);
    g_fsIdx = (int)SendMessageW(GetDlgItem(g_hwnd, IDC_FS), CB_GETCURSEL, 0, 0);
    if (g_fsIdx < 0) g_fsIdx = 0;
    GetDlgItemTextW(g_hwnd, IDC_LABEL, g_label, 33);
    g_quick = SendDlgItemMessageW(g_hwnd, IDC_QUICK, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_bkFirst = SendDlgItemMessageW(g_hwnd, IDC_BKFIRST, BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_op = op;

    if (op == 2) {
        wchar_t item[256], q[600];
        SendMessageW(cb, CB_GETLBTEXT, sel, (LPARAM)item);
        _snwprintf(q, 600, L"\u00a1ATENCI\u00d3N!\n\nSe borrar\u00e1n TODOS los datos de:\n\n%ls\n\n\u00bfSeguro que quieres continuar?", item);
        if (MessageBoxW(g_hwnd, q, L"GrifoFormat - Confirmar", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
        if (!g_bkFirst) {
            if (MessageBoxW(g_hwnd, L"Esta acci\u00f3n NO se puede deshacer y no hiciste respaldo.\n\n\u00bfFormatear de todas formas?",
                            L"GrifoFormat - \u00daltima advertencia", MB_YESNO | MB_ICONSTOP | MB_DEFBUTTON2) != IDYES) return;
        }
    }
    if (op == 1 || g_bkFirst) {
        if (!AskBackupDir()) {
            SetDlgItemTextW(g_hwnd, IDC_STATUS, L"Cancelado: no elegiste carpeta para el respaldo. No se hizo nada.");
            return;
        }
    }
    SetBusy(true);
    SetProg(0);
    SetDlgItemTextW(g_hwnd, IDC_STATUS, L"Trabajando... no desconectes la memoria.");
    HANDLE t = CreateThread(NULL, 0, Worker, NULL, 0, NULL);
    if (t) CloseHandle(t); else SetBusy(false);
}

static void DrawBtn(DRAWITEMSTRUCT* d, bool primary)
{
    HDC dc = d->hDC;
    RECT r = d->rcItem;
    bool dis = (d->itemState & ODS_DISABLED) != 0, sel = (d->itemState & ODS_SELECTED) != 0;
    COLORREF fill = primary ? (sel ? RGB(170, 138, 38) : cGold) : (sel ? RGB(44, 48, 78) : cPanel);
    COLORREF border = cGold, tx = primary ? cBg : cGold;
    if (dis) { fill = RGB(34, 36, 48); border = RGB(80, 80, 92); tx = RGB(110, 110, 124); }
    FillRect(dc, &r, brBg);
    HBRUSH b = CreateSolidBrush(fill);
    HPEN pn = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, pn);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, 12, 12);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); DeleteObject(pn);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, tx);
    HGDIOBJ of = SelectObject(dc, fLabel);
    wchar_t t[80];
    GetWindowTextW(d->hwndItem, t, 80);
    DrawTextW(dc, t, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, of);
}

static HWND Mk(const wchar_t* cls, const wchar_t* txt, DWORD style, int x, int y, int w, int h, int id, HFONT f, DWORD ex = 0)
{
    HWND c = CreateWindowExW(ex, cls, txt, WS_CHILD | WS_VISIBLE | style, x, y, w, h, g_hwnd, (HMENU)(INT_PTR)id, g_inst, NULL);
    if (f) SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE);
    return c;
}

static void Build()
{
    Mk(L"COMBOBOX", NULL, WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 24, 150, 376, 260, IDC_DRIVE, fUI);
    Mk(L"BUTTON", L"Actualizar", WS_TABSTOP | BS_OWNERDRAW, 410, 149, 66, 26, IDC_REFRESH, fLabel);

    HWND fs = Mk(L"COMBOBOX", NULL, WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 24, 212, 226, 200, IDC_FS, fUI);
    SendMessageW(fs, CB_ADDSTRING, 0, (LPARAM)L"FAT32  (compatible con todo)");
    SendMessageW(fs, CB_ADDSTRING, 0, (LPARAM)L"exFAT  (archivos enormes)");
    SendMessageW(fs, CB_ADDSTRING, 0, (LPARAM)L"NTFS  (Windows)");
    SendMessageW(fs, CB_SETCURSEL, 0, 0);

    HWND ed = Mk(L"EDIT", L"GRIFO", WS_TABSTOP | ES_AUTOHSCROLL, 262, 212, 214, 24, IDC_LABEL, fUI, WS_EX_CLIENTEDGE);
    SendMessageW(ed, EM_SETLIMITTEXT, 32, 0);

    HWND q = Mk(L"BUTTON", L"Formato r\u00e1pido (recomendado)", WS_TABSTOP | BS_AUTOCHECKBOX, 24, 252, 452, 22, IDC_QUICK, fUI);
    SendMessageW(q, BM_SETCHECK, BST_CHECKED, 0);
    Mk(L"BUTTON", L"Respaldar todo a mi PC antes de formatear", WS_TABSTOP | BS_AUTOCHECKBOX, 24, 278, 452, 22, IDC_BKFIRST, fUI);

    Mk(L"STATIC", L"FAT32 funciona en memorias de cualquier tama\u00f1o (64 GB, 128 GB o m\u00e1s) gracias al formateador propio de GrifoFormat. "
                  L"Al respaldar, GrifoFormat te preguntar\u00e1 en qu\u00e9 carpeta guardar la copia.", SS_LEFT, 24, 312, 452, 36, IDC_NOTE, fLabel);

    Mk(L"BUTTON", L"RESPALDAR AHORA", WS_TABSTOP | BS_OWNERDRAW, 24, 362, 218, 42, IDC_BACKUP, fLabel);
    Mk(L"BUTTON", L"FORMATEAR", WS_TABSTOP | BS_OWNERDRAW, 258, 362, 218, 42, IDC_FORMAT, fLabel);

    Mk(L"STATIC", NULL, SS_OWNERDRAW, 24, 424, 452, 14, IDC_PROG, NULL);

    Mk(L"STATIC", L"", SS_LEFT, 24, 446, 452, 40, IDC_STATUS, fUI);
    Mk(L"BUTTON", L"Acerca de", WS_TABSTOP | BS_OWNERDRAW, 24, 500, 120, 30, IDC_ABOUT, fLabel);
}

// ----------------------------- Ventana "Acerca de" ---------------------------
static LRESULT CALLBACK AboutProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE: {
        HWND b;
        b = CreateWindowExW(0, L"BUTTON", L"Abrir web", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 24, 352, 124, 32, h, (HMENU)IDA_WEB, g_inst, NULL);
        SendMessageW(b, WM_SETFONT, (WPARAM)fLabel, TRUE);
        b = CreateWindowExW(0, L"BUTTON", L"Copiar n\u00famero", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 158, 352, 124, 32, h, (HMENU)IDA_PHONE, g_inst, NULL);
        SendMessageW(b, WM_SETFONT, (WPARAM)fLabel, TRUE);
        b = CreateWindowExW(0, L"BUTTON", L"Copiar USDT", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 292, 352, 124, 32, h, (HMENU)IDA_USDT, g_inst, NULL);
        SendMessageW(b, WM_SETFONT, (WPARAM)fLabel, TRUE);
        b = CreateWindowExW(0, L"BUTTON", L"Cerrar", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 150, 398, 140, 34, h, (HMENU)IDA_CLOSE, g_inst, NULL);
        SendMessageW(b, WM_SETFONT, (WPARAM)fLabel, TRUE);
        SendMessageW(h, WM_SETICON, ICON_BIG, (LPARAM)g_icoBig);
        SendMessageW(h, WM_SETICON, ICON_SMALL, (LPARAM)g_icoSmall);
        return 0;
    }
    case WM_ERASEBKGND: {
        RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, brBg); return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        SetBkMode(dc, TRANSPARENT);
        if (g_icoBig) DrawIconEx(dc, 24, 18, g_icoBig, 64, 64, 0, NULL, DI_NORMAL);
        SelectObject(dc, fBig); SetTextColor(dc, cGold);
        TextOutW(dc, 104, 18, L"GrifoFormat", 11);
        SelectObject(dc, fSub); SetTextColor(dc, cText);
        wchar_t v[64]; _snwprintf(v, 64, L"Versi\u00f3n %ls  \u00b7  Guardi\u00e1n de tus memorias", APP_VERSION);
        TextOutW(dc, 106, 56, v, (int)wcslen(v));

        RECT r = { 24, 100, 416, 168 };
        SelectObject(dc, fUI); SetTextColor(dc, cText);
        DrawTextW(dc, L"GrifoFormat formatea tus memorias USB con estilo. Soporta FAT32 aunque tu memoria sea de 64 GB, 128 GB o m\u00e1s "
                      L"(algo que Windows normalmente no deja), adem\u00e1s de exFAT y NTFS. Incluye copia de seguridad autom\u00e1tica a tu PC.",
                  -1, &r, DT_LEFT | DT_WORDBREAK);

        SelectObject(dc, fLabel); SetTextColor(dc, cGold);
        TextOutW(dc, 24, 178, L"CREADOR", 7);
        SelectObject(dc, fBig); SetTextColor(dc, cText);
        TextOutW(dc, 24, 194, CREATOR_NAME, (int)wcslen(CREATOR_NAME));
        SelectObject(dc, fUI);
        wchar_t t[128];
        _snwprintf(t, 128, L"Contacto:  %ls", CREATOR_PHONE);
        TextOutW(dc, 24, 232, t, (int)wcslen(t));
        _snwprintf(t, 128, L"Web:  %ls", CREATOR_WEBTXT);
        TextOutW(dc, 24, 254, t, (int)wcslen(t));

        SelectObject(dc, fLabel); SetTextColor(dc, cGold);
        { const wchar_t* sb = L"DIRECCI\u00d3N USDT (POLYGON)"; TextOutW(dc, 24, 288, sb, (int)wcslen(sb)); }
        RECT ra = { 24, 308, 416, 346 };
        SelectObject(dc, fMono); SetTextColor(dc, cText);
        DrawTextW(dc, USDT_ADDR, -1, &ra, DT_LEFT | DT_WORDBREAK);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_DRAWITEM:
        DrawBtn((DRAWITEMSTRUCT*)l, ((DRAWITEMSTRUCT*)l)->CtlID == IDA_CLOSE);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDA_WEB: ShellExecuteW(h, L"open", CREATOR_WEB, NULL, NULL, SW_SHOWNORMAL); break;
        case IDA_PHONE: CopyToClipboard(h, CREATOR_PHONE); SetDlgItemTextW(h, IDA_PHONE, L"\u00a1Copiado!"); break;
        case IDA_USDT: CopyToClipboard(h, USDT_ADDR); SetDlgItemTextW(h, IDA_USDT, L"\u00a1Copiado!"); break;
        case IDA_CLOSE: DestroyWindow(h); break;
        }
        return 0;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY:
        EnableWindow(g_hwnd, TRUE);
        SetForegroundWindow(g_hwnd);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void ShowAbout()
{
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = AboutProc; wc.hInstance = g_inst; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = brBg; wc.lpszClassName = L"GrifoAbout";
        RegisterClassW(&wc);
        reg = true;
    }
    RECT rc = { 0, 0, 440, 450 };
    DWORD st = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&rc, st, FALSE);
    RECT pr; GetWindowRect(g_hwnd, &pr);
    int w = rc.right - rc.left, hh = rc.bottom - rc.top;
    int x = pr.left + ((pr.right - pr.left) - w) / 2, y = pr.top + ((pr.bottom - pr.top) - hh) / 2;
    EnableWindow(g_hwnd, FALSE);
    HWND a = CreateWindowExW(0, L"GrifoAbout", L"Acerca de GrifoFormat", st, x, y, w, hh, g_hwnd, NULL, g_inst, NULL);
    if (!a) { EnableWindow(g_hwnd, TRUE); return; }
    ShowWindow(a, SW_SHOW);
}

// ----------------------------- Ventana principal -----------------------------
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        g_hwnd = h;
        SendMessageW(h, WM_SETICON, ICON_BIG, (LPARAM)g_icoBig);
        SendMessageW(h, WM_SETICON, ICON_SMALL, (LPARAM)g_icoSmall);
        Build();
        RefreshDrives();
        return 0;
    case WM_ERASEBKGND: {
        RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, brBg); return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        SetBkMode(dc, TRANSPARENT);
        if (g_icoBig) DrawIconEx(dc, 24, 16, g_icoBig, 80, 80, 0, NULL, DI_NORMAL);
        SelectObject(dc, fTitle); SetTextColor(dc, cGold);
        TextOutW(dc, 118, 16, L"GRIFOFORMAT", 11);
        SelectObject(dc, fSub); SetTextColor(dc, cText);
        { const wchar_t* sb = L"Guardi\u00e1n de tus memorias"; TextOutW(dc, 121, 66, sb, (int)wcslen(sb)); }

        HPEN pn = CreatePen(PS_SOLID, 2, cGold);
        HGDIOBJ op = SelectObject(dc, pn);
        MoveToEx(dc, 24, 112, NULL); LineTo(dc, 236, 112);
        MoveToEx(dc, 264, 112, NULL); LineTo(dc, 476, 112);
        HBRUSH gb = CreateSolidBrush(cGold);
        HGDIOBJ ob = SelectObject(dc, gb);
        POINT dm[4] = { { 250, 104 }, { 258, 112 }, { 250, 120 }, { 242, 112 } };
        Polygon(dc, dm, 4);
        SelectObject(dc, ob); SelectObject(dc, op);
        DeleteObject(gb); DeleteObject(pn);

        SelectObject(dc, fLabel); SetTextColor(dc, cGold);
        TextOutW(dc, 24, 128, L"UNIDAD USB", 10);
        TextOutW(dc, 24, 192, L"SISTEMA DE ARCHIVOS", 19);
        TextOutW(dc, 262, 192, L"NOMBRE DEL VOLUMEN", 18);
        SetTextColor(dc, cMuted);
        TextOutW(dc, 190, 507, L"Hecho por xyvenqorix", 20);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        SetBkColor(dc, cBg);
        SetTextColor(dc, GetDlgCtrlID((HWND)l) == IDC_NOTE ? cMuted : cText);
        return (LRESULT)brBg;
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC dc = (HDC)w;
        SetBkColor(dc, cPanel);
        SetTextColor(dc, cText);
        return (LRESULT)brPanel;
    }
    case WM_DRAWITEM:
        if (((DRAWITEMSTRUCT*)l)->CtlID == IDC_PROG) DrawProg((DRAWITEMSTRUCT*)l);
        else DrawBtn((DRAWITEMSTRUCT*)l, ((DRAWITEMSTRUCT*)l)->CtlID == IDC_FORMAT);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDC_REFRESH: if (!g_busy) RefreshDrives(); break;
        case IDC_BACKUP: StartOp(1); break;
        case IDC_FORMAT: StartOp(2); break;
        case IDC_ABOUT: ShowAbout(); break;
        }
        return 0;
    case WM_DEVICECHANGE:
        if (!g_busy && (w == 0x8000 || w == 0x8004)) RefreshDrives();   // conectar / quitar USB
        return TRUE;
    case WM_PROG:
        SetProg((int)w);
        return 0;
    case WM_STAT:
        if (l) { SetDlgItemTextW(h, IDC_STATUS, (wchar_t*)l); free((void*)l); }
        return 0;
    case WM_DONE: {
        wchar_t* msg = (wchar_t*)l;
        bool ok = w != 0;
        SetBusy(false);
        SetProg(ok ? 100 : 0);
        SetDlgItemTextW(h, IDC_STATUS, msg ? msg : L"");
        MessageBoxW(h, msg ? msg : L"", L"GrifoFormat", MB_OK | (ok ? MB_ICONINFORMATION : MB_ICONERROR));
        if (ok && g_op == 1) ShellExecuteW(h, L"open", g_backupDir, NULL, NULL, SW_SHOWNORMAL);
        if (msg) free(msg);
        RefreshDrives();
        SetDlgItemTextW(h, IDC_STATUS, ok ? L"Listo." : L"Hubo un problema. Revisa el mensaje anterior.");
        return 0;
    }
    case WM_CLOSE:
        if (g_busy) {
            MessageBoxW(h, L"Hay una operaci\u00f3n en curso. Espera a que termine para no da\u00f1ar la memoria.", L"GrifoFormat", MB_OK | MB_ICONWARNING);
            return 0;
        }
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static bool IsElevated()
{
    HANDLE t;
    DWORD e = 0, r = 0;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) {
        GetTokenInformation(t, (TOKEN_INFORMATION_CLASS)20, &e, sizeof(e), &r);   // TokenElevation
        CloseHandle(t);
    }
    return e != 0;
}

static HFONT MkFont(const wchar_t* face, int h, int weight, bool italic)
{
    return CreateFontW(h, 0, 0, 0, weight, italic, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE, LPSTR, int)
{
    g_inst = hi;
    FreeConsole();   // oculta la ventana negra (simbolo del sistema) si el proyecto es de consola
    // Para formatear se necesitan permisos de administrador: se relanza solo.
    if (!IsElevated() && !wcsstr(GetCommandLineW(), L"/e")) {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(NULL, exe, MAX_PATH);
        if ((INT_PTR)ShellExecuteW(NULL, L"runas", exe, L"/e", NULL, SW_SHOWNORMAL) > 32) return 0;
    }
    if (!LoadGdi()) { MessageBoxW(NULL, L"No se pudo cargar gdi32.dll", L"GrifoFormat", MB_OK | MB_ICONERROR); return 1; }

    brBg = CreateSolidBrush(cBg);
    brPanel = CreateSolidBrush(cPanel);
    fUI = MkFont(L"Segoe UI", -15, FW_NORMAL, false);
    fLabel = MkFont(L"Segoe UI", -13, FW_BOLD, false);
    fTitle = MkFont(L"Georgia", -40, FW_BOLD, false);
    fSub = MkFont(L"Georgia", -17, FW_NORMAL, true);
    fBig = MkFont(L"Georgia", -28, FW_BOLD, false);
    fMono = MkFont(L"Consolas", -14, FW_NORMAL, false);
    g_icoBig = LoadAppIcon(64);
    g_icoSmall = LoadAppIcon(16);

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc; wc.hInstance = hi;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = brBg;
    wc.lpszClassName = L"GrifoFormatWnd";
    wc.hIcon = g_icoBig; wc.hIconSm = g_icoSmall;
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, 500, 548 };
    DWORD st = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rc, st, FALSE);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    HWND wnd = CreateWindowExW(0, L"GrifoFormatWnd", L"GrifoFormat - Formateador USB", st, x, y, w, h, NULL, NULL, hi, NULL);
    if (!wnd) return 1;
    ShowWindow(wnd, SW_SHOW);
    UpdateWindow(wnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
