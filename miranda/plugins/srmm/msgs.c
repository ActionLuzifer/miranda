/*
SRMM

Copyright 2000-2003 Miranda ICQ/IM project, 
all portions of this codebase are copyrighted to the people 
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#include "commonheaders.h"
#pragma hdrstop

static void InitREOleCallback(void);

HCURSOR hCurSplitNS, hCurSplitWE, hCurHyperlinkHand;
HANDLE hMessageWindowList;
static HANDLE hEventDbEventAdded, hEventDbSettingChange, hEventContactDeleted;
HANDLE *hMsgMenuItem = NULL, hHookWinEvt=NULL;
int hMsgMenuItemCount = 0;

extern HINSTANCE g_hInst;

static int ReadMessageCommand(WPARAM wParam, LPARAM lParam)
{
    struct NewMessageWindowLParam newData = { 0 };
    HWND hwndExisting;

    hwndExisting = WindowList_Find(hMessageWindowList, ((CLISTEVENT *) lParam)->hContact);
    newData.hContact = ((CLISTEVENT *) lParam)->hContact;
    CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_MSGSPLIT), NULL, DlgProcMessage, (LPARAM) & newData);
    return 0;
}

static int MessageEventAdded(WPARAM wParam, LPARAM lParam)
{
    CLISTEVENT cle;
    DBEVENTINFO dbei;
    char *contactName;
    char toolTip[256];
    HWND hwnd;

    ZeroMemory(&dbei, sizeof(dbei));
    dbei.cbSize = sizeof(dbei);
    dbei.cbBlob = 0;
    CallService(MS_DB_EVENT_GET, lParam, (LPARAM) & dbei);

    if (dbei.flags & (DBEF_SENT | DBEF_READ) || dbei.eventType != EVENTTYPE_MESSAGE)
        return 0;

    CallServiceSync(MS_CLIST_REMOVEEVENT, wParam, (LPARAM) 1);
    /* does a window for the contact exist? */
    hwnd = WindowList_Find(hMessageWindowList, (HANDLE) wParam);
    if (hwnd) {
        if (GetForegroundWindow()==hwnd)
            SkinPlaySound("RecvMsgActive");
        else SkinPlaySound("RecvMsgInactive");
        return 0;
    }
    /* new message */
    SkinPlaySound("AlertMsg");

    if (DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_AUTOPOPUP, SRMSGDEFSET_AUTOPOPUP)) {
        struct NewMessageWindowLParam newData = { 0 };
        newData.hContact = (HANDLE) wParam;
        CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_MSGSPLIT), NULL, DlgProcMessage, (LPARAM) & newData);
        return 0;
    }

    ZeroMemory(&cle, sizeof(cle));
    cle.cbSize = sizeof(cle);
    cle.hContact = (HANDLE) wParam;
    cle.hDbEvent = (HANDLE) lParam;
    cle.hIcon = LoadSkinnedIcon(SKINICON_EVENT_MESSAGE);
    cle.pszService = "SRMsg/ReadMessage";
    contactName = (char *) CallService(MS_CLIST_GETCONTACTDISPLAYNAME, wParam, 0);
    _snprintf(toolTip, sizeof(toolTip), Translate("Message from %s"), contactName);
    cle.pszTooltip = toolTip;
    CallService(MS_CLIST_ADDEVENT, 0, (LPARAM) & cle);
    return 0;
}

static int SendMessageCommand(WPARAM wParam, LPARAM lParam)
{
    HWND hwnd;
    struct NewMessageWindowLParam newData = { 0 };

    {
        /* does the HCONTACT's protocol support IM messages? */
        char *szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, wParam, 0);
        if (szProto) {
            if (!CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_IMSEND)
                return 1;
        }
        else {
            /* unknown contact */
            return 1;
        }                       //if
    }

    if (hwnd = WindowList_Find(hMessageWindowList, (HANDLE) wParam)) {
        if (lParam) {
            HWND hEdit;
            hEdit = GetDlgItem(hwnd, IDC_MESSAGE);
            SendMessage(hEdit, EM_SETSEL, -1, SendMessage(hEdit, WM_GETTEXTLENGTH, 0, 0));
            SendMessage(hEdit, EM_REPLACESEL, FALSE, (LPARAM) (char *) lParam);
        }
        ShowWindow(hwnd, SW_SHOWNORMAL);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
    }
    else {
        newData.hContact = (HANDLE) wParam;
        newData.szInitialText = (const char *) lParam;
        CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_MSGSPLIT), NULL, DlgProcMessage, (LPARAM) & newData);
    }
    return 0;
}

static int TypingMessageCommand(WPARAM wParam, LPARAM lParam)
{
    CLISTEVENT *cle = (CLISTEVENT *) lParam;

    if (!cle)
        return 0;
    SendMessageCommand((WPARAM) cle->hContact, 0);
    return 0;
}

static int TypingMessage(WPARAM wParam, LPARAM lParam)
{
    HWND hwnd;
    int foundWin = 0;

    if (!DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_SHOWTYPING, SRMSGDEFSET_SHOWTYPING))
        return 0;
    if (hwnd = WindowList_Find(hMessageWindowList, (HANDLE) wParam)) {
        SendMessage(hwnd, DM_TYPING, 0, lParam);
        foundWin = 1;
    }
    if ((int) lParam && !foundWin && DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_SHOWTYPINGNOWIN, SRMSGDEFSET_SHOWTYPINGNOWIN)) {
        char szTip[256];

        _snprintf(szTip, sizeof(szTip), Translate("%s is typing a message"), (char *) CallService(MS_CLIST_GETCONTACTDISPLAYNAME, wParam, 0));
        if (ServiceExists(MS_CLIST_SYSTRAY_NOTIFY) && !DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_SHOWTYPINGCLIST, SRMSGDEFSET_SHOWTYPINGCLIST)) {
            MIRANDASYSTRAYNOTIFY tn;
            tn.szProto = NULL;
            tn.cbSize = sizeof(tn);
            tn.szInfoTitle = Translate("Typing Notification");
            tn.szInfo = szTip;
            tn.dwInfoFlags = NIIF_INFO;
            tn.uTimeout = 1000 * 4;
            CallService(MS_CLIST_SYSTRAY_NOTIFY, 0, (LPARAM) & tn);
        }
        else {
            CLISTEVENT cle;

            ZeroMemory(&cle, sizeof(cle));
            cle.cbSize = sizeof(cle);
            cle.hContact = (HANDLE) wParam;
            cle.hDbEvent = (HANDLE) 1;
            cle.flags = CLEF_ONLYAFEW;
            cle.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IsWinVerXPPlus()? IDI_TYPING32 : IDI_TYPING));
            cle.pszService = "SRMsg/TypingMessage";
            cle.pszTooltip = szTip;
            CallServiceSync(MS_CLIST_REMOVEEVENT, wParam, (LPARAM) 1);
            CallServiceSync(MS_CLIST_ADDEVENT, wParam, (LPARAM) & cle);
        }
    }
    return 0;
}

static int MessageSettingChanged(WPARAM wParam, LPARAM lParam)
{
    DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING *) lParam;
    char *szProto;

    szProto = (char *) CallService(MS_PROTO_GETCONTACTBASEPROTO, wParam, 0);
    if (lstrcmpA(cws->szModule, "CList") && (szProto == NULL || lstrcmpA(cws->szModule, szProto)))
        return 0;
    WindowList_Broadcast(hMessageWindowList, DM_UPDATETITLE, (WPARAM) cws, 0);
    return 0;
}

static int ContactDeleted(WPARAM wParam, LPARAM lParam)
{
    HWND hwnd;

    if (hwnd = WindowList_Find(hMessageWindowList, (HANDLE) wParam)) {
        SendMessage(hwnd, WM_CLOSE, 0, 0);
    }
    return 0;
}

static void RestoreUnreadMessageAlerts(void)
{
    CLISTEVENT cle = { 0 };
    DBEVENTINFO dbei = { 0 };
    char toolTip[256];
    int windowAlreadyExists;
    int autoPopup = DBGetContactSettingByte(NULL, SRMMMOD, SRMSGSET_AUTOPOPUP, SRMSGDEFSET_AUTOPOPUP);
    HANDLE hDbEvent, hContact;

    dbei.cbSize = sizeof(dbei);
    cle.cbSize = sizeof(cle);
    cle.hIcon = LoadSkinnedIcon(SKINICON_EVENT_MESSAGE);
    cle.pszService = "SRMsg/ReadMessage";

    hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDFIRST, 0, 0);
    while (hContact) {
        hDbEvent = (HANDLE) CallService(MS_DB_EVENT_FINDFIRSTUNREAD, (WPARAM) hContact, 0);
        while (hDbEvent) {
            dbei.cbBlob = 0;
            CallService(MS_DB_EVENT_GET, (WPARAM) hDbEvent, (LPARAM) & dbei);
            if (!(dbei.flags & (DBEF_SENT | DBEF_READ)) && dbei.eventType == EVENTTYPE_MESSAGE) {
                windowAlreadyExists = WindowList_Find(hMessageWindowList, hContact) != NULL;
                if (windowAlreadyExists)
                    continue;

                if (autoPopup && !windowAlreadyExists) {
                    struct NewMessageWindowLParam newData = { 0 };
                    newData.hContact = hContact;
                    CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_MSGSPLIT), NULL, DlgProcMessage, (LPARAM) & newData);
                }
                else {
                    cle.hContact = hContact;
                    cle.hDbEvent = hDbEvent;
                    _snprintf(toolTip, sizeof(toolTip), Translate("Message from %s"), (char *) CallService(MS_CLIST_GETCONTACTDISPLAYNAME, (WPARAM) hContact, 0));
                    cle.pszTooltip = toolTip;
                    CallService(MS_CLIST_ADDEVENT, 0, (LPARAM) & cle);
                }
            }
            hDbEvent = (HANDLE) CallService(MS_DB_EVENT_FINDNEXT, (WPARAM) hDbEvent, 0);
        }
        hContact = (HANDLE) CallService(MS_DB_CONTACT_FINDNEXT, (WPARAM) hContact, 0);
    }
}

static int SplitmsgModulesLoaded(WPARAM wParam, LPARAM lParam)
{
    CLISTMENUITEM mi;
    PROTOCOLDESCRIPTOR **protocol;
    int protoCount, i;

    LoadMsgLogIcons();
    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(mi);
    mi.position = -2000090000;
    mi.flags = 0;
    mi.hIcon = LoadSkinnedIcon(SKINICON_EVENT_MESSAGE);
    mi.pszName = Translate("&Message");
    mi.pszService = MS_MSG_SENDMESSAGE;
    CallService(MS_PROTO_ENUMPROTOCOLS, (WPARAM) & protoCount, (LPARAM) & protocol);
    for (i = 0; i < protoCount; i++) {
        if (protocol[i]->type != PROTOTYPE_PROTOCOL)
            continue;
        if (CallProtoService(protocol[i]->szName, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_IMSEND) {
            mi.pszContactOwner = protocol[i]->szName;
            hMsgMenuItem = realloc(hMsgMenuItem, (hMsgMenuItemCount + 1) * sizeof(HANDLE));
            hMsgMenuItem[hMsgMenuItemCount++] = (HANDLE) CallService(MS_CLIST_ADDCONTACTMENUITEM, 0, (LPARAM) & mi);
        }
    }
    HookEvent(ME_CLIST_DOUBLECLICKED, SendMessageCommand);
    RestoreUnreadMessageAlerts();
    return 0;
}

int PreshutdownSendRecv(WPARAM wParam, LPARAM lParam)
{
    WindowList_BroadcastAsync(hMessageWindowList, WM_CLOSE, 0, 0);
    return 0;
}

int SplitmsgShutdown(void)
{
    DestroyCursor(hCurSplitNS);
    DestroyCursor(hCurHyperlinkHand);
    DestroyCursor(hCurSplitWE);
    UnhookEvent(hEventDbEventAdded);
    UnhookEvent(hEventDbSettingChange);
    UnhookEvent(hEventContactDeleted);
    FreeMsgLogIcons();
    FreeLibrary(GetModuleHandleA("riched20"));
    OleUninitialize();
    if (hMsgMenuItem) {
        free(hMsgMenuItem);
        hMsgMenuItem = NULL;
        hMsgMenuItemCount = 0;
    }
    return 0;
}

static int IconsChanged(WPARAM wParam, LPARAM lParam)
{
    if (hMsgMenuItem) {
        int j;
        CLISTMENUITEM mi;

        mi.cbSize = sizeof(mi);
        mi.flags = CMIM_ICON;
        mi.hIcon = LoadSkinnedIcon(SKINICON_EVENT_MESSAGE);

        for (j = 0; j < hMsgMenuItemCount; j++) {
            CallService(MS_CLIST_MODIFYMENUITEM, (WPARAM) hMsgMenuItem[j], (LPARAM) & mi);
        }
    }
    FreeMsgLogIcons();
    LoadMsgLogIcons();
    WindowList_Broadcast(hMessageWindowList, DM_REMAKELOG, 0, 0);
    // change all the icons
    WindowList_Broadcast(hMessageWindowList, DM_UPDATEWINICON, 0, 0);
    return 0;
}

static int GetWindowAPI(WPARAM wParam, LPARAM lParam)
{
    return PLUGIN_MAKE_VERSION(0,0,0,1);
}

int LoadSendRecvMessageModule(void)
{
    if (LoadLibraryA("riched20.dll") == NULL) {
        if (IDYES !=
            MessageBoxA(0,
                        Translate
                        ("Miranda could not load the built-in message module, riched20.dll is missing. If you are using Windows 95 or WINE please make sure you have riched20.dll installed. Press 'Yes' to continue loading Miranda."),
                        Translate("Information"), MB_YESNO | MB_ICONINFORMATION))
            return 1;
        return 0;
    }
    OleInitialize(NULL);
    InitREOleCallback();
    hMessageWindowList = (HANDLE) CallService(MS_UTILS_ALLOCWINDOWLIST, 0, 0);
    InitOptions();
    hEventDbEventAdded = HookEvent(ME_DB_EVENT_ADDED, MessageEventAdded);
    hEventDbSettingChange = HookEvent(ME_DB_CONTACT_SETTINGCHANGED, MessageSettingChanged);
    hEventContactDeleted = HookEvent(ME_DB_CONTACT_DELETED, ContactDeleted);
    HookEvent(ME_SYSTEM_MODULESLOADED, SplitmsgModulesLoaded);
    HookEvent(ME_SKIN_ICONSCHANGED, IconsChanged);
    HookEvent(ME_PROTO_CONTACTISTYPING, TypingMessage);
    HookEvent(ME_SYSTEM_PRESHUTDOWN, PreshutdownSendRecv);
    CreateServiceFunction(MS_MSG_SENDMESSAGE, SendMessageCommand);
    CreateServiceFunction(MS_MSG_GETWINDOWAPI, GetWindowAPI);
    CreateServiceFunction("SRMsg/ReadMessage", ReadMessageCommand);
    CreateServiceFunction("SRMsg/TypingMessage", TypingMessageCommand);
    hHookWinEvt=CreateHookableEvent(ME_MSG_WINDOWEVENT);
    SkinAddNewSoundEx("RecvMsgActive", Translate("Messages"), Translate("Incoming (Focused Window)"));
    SkinAddNewSoundEx("RecvMsgInactive", Translate("Messages"), Translate("Incoming (Unfocused Window)"));
    SkinAddNewSoundEx("AlertMsg", Translate("Messages"), Translate("Incoming (New Session)"));
    SkinAddNewSoundEx("SendMsg", Translate("Messages"), Translate("Outgoing"));
    hCurSplitNS = LoadCursor(NULL, IDC_SIZENS);
    hCurSplitWE = LoadCursor(NULL, IDC_SIZEWE);
    hCurHyperlinkHand = LoadCursor(NULL, IDC_HAND);
    if (hCurHyperlinkHand == NULL)
        hCurHyperlinkHand = LoadCursor(g_hInst, MAKEINTRESOURCE(IDC_HYPERLINKHAND));
    return 0;
}

static IRichEditOleCallbackVtbl reOleCallbackVtbl;
struct CREOleCallback reOleCallback;

static STDMETHODIMP_(ULONG) CREOleCallback_QueryInterface(struct CREOleCallback *lpThis, REFIID riid, LPVOID * ppvObj)
{
    if (IsEqualIID(riid, &IID_IRichEditOleCallback)) {
        *ppvObj = lpThis;
        lpThis->lpVtbl->AddRef((IRichEditOleCallback *) lpThis);
        return S_OK;
    }
    *ppvObj = NULL;
    return E_NOINTERFACE;
}

static STDMETHODIMP_(ULONG) CREOleCallback_AddRef(struct CREOleCallback *lpThis)
{
    if (lpThis->refCount == 0) {
        if (S_OK != StgCreateDocfile(NULL, STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_CREATE | STGM_DELETEONRELEASE, 0, &lpThis->pictStg))
            lpThis->pictStg = NULL;
        lpThis->nextStgId = 0;
    }
    return ++lpThis->refCount;
}

static STDMETHODIMP_(ULONG) CREOleCallback_Release(struct CREOleCallback *lpThis)
{
    if (--lpThis->refCount == 0) {
        if (lpThis->pictStg)
            lpThis->pictStg->lpVtbl->Release(lpThis->pictStg);
    }
    return lpThis->refCount;
}

static STDMETHODIMP_(HRESULT) CREOleCallback_ContextSensitiveHelp(struct CREOleCallback *lpThis, BOOL fEnterMode)
{
    return S_OK;
}

static STDMETHODIMP_(HRESULT) CREOleCallback_DeleteObject(struct CREOleCallback *lpThis, LPOLEOBJECT lpoleobj)
{
    return S_OK;
}

static STDMETHODIMP_(HRESULT) CREOleCallback_GetClipboardData(struct CREOleCallback *lpThis, CHARRANGE * lpchrg, DWORD reco, LPDATAOBJECT * lplpdataobj)
{
    return E_NOTIMPL;
}

static STDMETHODIMP_(HRESULT) CREOleCallback_GetContextMenu(struct CREOleCallback *lpThis, WORD seltype, LPOLEOBJECT lpoleobj, CHARRANGE * lpchrg, HMENU * lphmenu)
{
    return E_INVALIDARG;
}

static STDMETHODIMP_(HRESULT) CREOleCallback_GetDragDropEffect(struct CREOleCallback *lpThis, BOOL fDrag, DWORD grfKeyState, LPDWORD pdwEffect)
{
    return S_OK;
}

static STDMETHODIMP_(HRESULT) CREOleCallback_GetInPlaceContext(struct CREOleCallback *lpThis, LPOLEINPLACEFRAME * lplpFrame, LPOLEINPLACEUIWINDOW * lplpDoc, LPOLEINPLACEFRAMEINFO lpFrameInfo)
{
    return E_INVALIDARG;
}

static STDMETHODIMP_(HRESULT) CREOleCallback_GetNewStorage(struct CREOleCallback *lpThis, LPSTORAGE * lplpstg)
{
    WCHAR szwName[64];
    char szName[64];
    wsprintfA(szName, "s%u", lpThis->nextStgId);
    MultiByteToWideChar(CP_ACP, 0, szName, -1, szwName, sizeof(szwName) / sizeof(szwName[0]));
    if (lpThis->pictStg == NULL)
        return STG_E_MEDIUMFULL;
    return lpThis->pictStg->lpVtbl->CreateStorage(lpThis->pictStg, szwName, STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_CREATE, 0, 0, lplpstg);
}

static STDMETHODIMP_(HRESULT) CREOleCallback_QueryAcceptData(struct CREOleCallback *lpThis, LPDATAOBJECT lpdataobj, CLIPFORMAT * lpcfFormat, DWORD reco, BOOL fReally, HGLOBAL hMetaPict)
{
    return S_OK;
}

static STDMETHODIMP_(HRESULT) CREOleCallback_QueryInsertObject(struct CREOleCallback *lpThis, LPCLSID lpclsid, LPSTORAGE lpstg, LONG cp)
{
    return S_OK;
}

static STDMETHODIMP_(HRESULT) CREOleCallback_ShowContainerUI(struct CREOleCallback *lpThis, BOOL fShow)
{
    return S_OK;
}

static void InitREOleCallback(void)
{
    reOleCallback.lpVtbl = &reOleCallbackVtbl;
    reOleCallback.lpVtbl->AddRef = (ULONG(__stdcall *) (IRichEditOleCallback *)) CREOleCallback_AddRef;
    reOleCallback.lpVtbl->Release = (ULONG(__stdcall *) (IRichEditOleCallback *)) CREOleCallback_Release;
    reOleCallback.lpVtbl->QueryInterface = (ULONG(__stdcall *) (IRichEditOleCallback *, REFIID, PVOID *)) CREOleCallback_QueryInterface;
    reOleCallback.lpVtbl->ContextSensitiveHelp = (HRESULT(__stdcall *) (IRichEditOleCallback *, BOOL)) CREOleCallback_ContextSensitiveHelp;
    reOleCallback.lpVtbl->DeleteObject = (HRESULT(__stdcall *) (IRichEditOleCallback *, LPOLEOBJECT)) CREOleCallback_DeleteObject;
    reOleCallback.lpVtbl->GetClipboardData = (HRESULT(__stdcall *) (IRichEditOleCallback *, CHARRANGE *, DWORD, LPDATAOBJECT *)) CREOleCallback_GetClipboardData;
    reOleCallback.lpVtbl->GetContextMenu = (HRESULT(__stdcall *) (IRichEditOleCallback *, WORD, LPOLEOBJECT, CHARRANGE *, HMENU *)) CREOleCallback_GetContextMenu;
    reOleCallback.lpVtbl->GetDragDropEffect = (HRESULT(__stdcall *) (IRichEditOleCallback *, BOOL, DWORD, LPDWORD)) CREOleCallback_GetDragDropEffect;
    reOleCallback.lpVtbl->GetInPlaceContext = (HRESULT(__stdcall *) (IRichEditOleCallback *, LPOLEINPLACEFRAME *, LPOLEINPLACEUIWINDOW *, LPOLEINPLACEFRAMEINFO))
        CREOleCallback_GetInPlaceContext;
    reOleCallback.lpVtbl->GetNewStorage = (HRESULT(__stdcall *) (IRichEditOleCallback *, LPSTORAGE *)) CREOleCallback_GetNewStorage;
    reOleCallback.lpVtbl->QueryAcceptData = (HRESULT(__stdcall *) (IRichEditOleCallback *, LPDATAOBJECT, CLIPFORMAT *, DWORD, BOOL, HGLOBAL)) CREOleCallback_QueryAcceptData;
    reOleCallback.lpVtbl->QueryInsertObject = (HRESULT(__stdcall *) (IRichEditOleCallback *, LPCLSID, LPSTORAGE, LONG)) CREOleCallback_QueryInsertObject;
    reOleCallback.lpVtbl->ShowContainerUI = (HRESULT(__stdcall *) (IRichEditOleCallback *, BOOL)) CREOleCallback_ShowContainerUI;
    reOleCallback.refCount = 0;
}
