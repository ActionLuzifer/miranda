/*

Miranda IM: the free IM client for Microsoft* Windows*

Copyright 2000-2008 Miranda ICQ/IM project,
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

#define OPENOPTIONSDIALOG_OLD_SIZE 12

static HANDLE hOptionsInitEvent;
static HWND hwndOptions=NULL;

typedef HRESULT (STDAPICALLTYPE *pfnEnableThemeDialogTexture)(HWND,DWORD);
static pfnEnableThemeDialogTexture MyEnableThemeDialogTexture = NULL;

static void FillFilterCombo(HWND hDlg, struct OptionsPageData * opd, int PageCount);

struct OptionsPageInit
{
	int pageCount;
	OPTIONSDIALOGPAGE *odp;
};

struct DlgTemplateExBegin
{
    WORD   dlgVer;
    WORD   signature;
    DWORD  helpID;
    DWORD  exStyle;
    DWORD  style;
    WORD   cDlgItems;
    short  x;
    short  y;
    short  cx;
    short  cy;
};

struct OptionsPageData
{
	DLGTEMPLATE *pTemplate;
	DLGPROC dlgProc;
	HINSTANCE hInst;
	HTREEITEM hTreeItem;
	HWND hwnd;
	int changed;
	int simpleHeight,expertHeight;
	int simpleWidth,expertWidth;
	int simpleBottomControlId,simpleRightControlId;
	int nExpertOnlyControls;
	UINT *expertOnlyControls;
	DWORD flags;
	TCHAR *pszTitle, *pszGroup, *pszTab;
	BOOL insideTab;
	LPARAM dwInitParam;
};

struct OptionsDlgData
{
	int pageCount;
	int currentPage;
	HTREEITEM hCurrentPage;
	struct OptionsPageData *opd;
	RECT rcDisplay;
	RECT rcTab;
	HFONT hBoldFont;
};

static HTREEITEM FindNamedTreeItemAtRoot(HWND hwndTree, const TCHAR* name)
{
	TVITEM tvi;
	TCHAR str[128];

	tvi.mask = TVIF_TEXT;
	tvi.pszText = str;
	tvi.cchTextMax = SIZEOF( str );
	tvi.hItem = TreeView_GetRoot( hwndTree );
	while( tvi.hItem != NULL ) {
		SendMessage( hwndTree, TVM_GETITEM, 0, (LPARAM)&tvi );
		if( !_tcsicmp( str,name ))
			return tvi.hItem;

		tvi.hItem = TreeView_GetNextSibling( hwndTree, tvi.hItem );
	}
	return NULL;
}

static HTREEITEM FindNamedTreeItemAtChildren(HWND hwndTree, HTREEITEM hItem, const TCHAR* name)
{
	TVITEM tvi;
	TCHAR str[128];

	tvi.mask = TVIF_TEXT;
	tvi.pszText = str;
	tvi.cchTextMax = SIZEOF( str );
	tvi.hItem = TreeView_GetChild( hwndTree, hItem );
	while( tvi.hItem != NULL ) {
		SendMessage( hwndTree, TVM_GETITEM, 0, (LPARAM)&tvi );
		if( !_tcsicmp( str,name ))
			return tvi.hItem;

		tvi.hItem = TreeView_GetNextSibling( hwndTree, tvi.hItem );
	}
	return NULL;
}

static BOOL CALLBACK BoldGroupTitlesEnumChildren(HWND hwnd,LPARAM lParam)
{
	TCHAR szClass[64];

	GetClassName(hwnd,szClass,SIZEOF(szClass));
	if(!lstrcmp(szClass,_T("Button")) && (GetWindowLong(hwnd,GWL_STYLE)&0x0F)==BS_GROUPBOX)
		SendMessage(hwnd,WM_SETFONT,lParam,0);
	return TRUE;
}

#define OPTSTATE_PREFIX "s_"

static void SaveOptionsTreeState(HWND hdlg)
{
	TVITEMA tvi;
	char buf[130],str[128];
	tvi.mask = TVIF_TEXT | TVIF_STATE;
	tvi.pszText = str;
	tvi.cchTextMax = SIZEOF(str);
	tvi.hItem = TreeView_GetRoot( GetDlgItem( hdlg, IDC_PAGETREE ));
	while ( tvi.hItem != NULL ) {
		if ( SendMessageA( GetDlgItem(hdlg,IDC_PAGETREE), TVM_GETITEMA, 0, (LPARAM)&tvi )) {
			wsprintfA(buf,"%s%s",OPTSTATE_PREFIX,str);
			DBWriteContactSettingByte(NULL,"Options",buf,(BYTE)((tvi.state&TVIS_EXPANDED)?1:0));
		}
		tvi.hItem = TreeView_GetNextSibling( GetDlgItem( hdlg, IDC_PAGETREE ), tvi.hItem );
}	}

#define DM_FOCUSPAGE   (WM_USER+10)
#define DM_REBUILDPAGETREE (WM_USER+11)

static void ThemeDialogBackground(HWND hwnd, BOOL tabbed)
{
	if (MyEnableThemeDialogTexture)
		MyEnableThemeDialogTexture(hwnd,(tabbed?0x00000002:0x00000001)|0x00000004); //0x00000002|0x00000004=ETDT_ENABLETAB
}

static int lstrcmpnull(TCHAR *str1, TCHAR *str2)
{
	if ( str1 == NULL && str2 == NULL )
		return 0;
	if ( str1 != NULL && str2 == NULL )
		return 1;
	if ( str1 == NULL && str2 != NULL )
		return -1;

   return lstrcmp(str1, str2);
}

static BOOL CALLBACK OptionsDlgProc(HWND hdlg,UINT message,WPARAM wParam,LPARAM lParam)
{
	struct OptionsDlgData* dat = (struct OptionsDlgData* )GetWindowLong( hdlg, GWL_USERDATA );

	switch ( message ) {
	case WM_INITDIALOG:
	{	
		PROPSHEETHEADER *psh=(PROPSHEETHEADER*)lParam;
		OPENOPTIONSDIALOG *ood=(OPENOPTIONSDIALOG*)psh->pStartPage;
		OPTIONSDIALOGPAGE *odp;
		int i;
		POINT pt;
		RECT rc,rcDlg;
		struct DlgTemplateExBegin *dte;
		TCHAR *lastPage = NULL, *lastGroup = NULL, *lastTab = NULL;
		DBVARIANT dbv;
		TCITEM tie;

		Utils_RestoreWindowPositionNoSize(hdlg, NULL, "Options", "");
		TranslateDialogDefault(hdlg);
		Window_SetIcon_IcoLib(hdlg, SKINICON_OTHER_OPTIONS);
		CheckDlgButton(hdlg,IDC_EXPERT,DBGetContactSettingByte(NULL,"Options","Expert",SETTING_SHOWEXPERT_DEFAULT)?BST_CHECKED:BST_UNCHECKED);
		EnableWindow(GetDlgItem(hdlg,IDC_APPLY),FALSE);
		dat=(struct OptionsDlgData*)mir_alloc(sizeof(struct OptionsDlgData));
		SetWindowLong(hdlg,GWL_USERDATA,(LONG)dat);
		SetWindowText(hdlg,psh->pszCaption);
		{	LOGFONT lf;
			dat->hBoldFont=(HFONT)SendDlgItemMessage(hdlg,IDC_EXPERT,WM_GETFONT,0,0);
			GetObject(dat->hBoldFont,sizeof(lf),&lf);
			lf.lfWeight=FW_BOLD;
			dat->hBoldFont=CreateFontIndirect(&lf);
		}
		dat->pageCount = psh->nPages;
		dat->opd = ( struct OptionsPageData* )mir_alloc( sizeof(struct OptionsPageData) * dat->pageCount );
		odp = ( OPTIONSDIALOGPAGE* )psh->ppsp;

		dat->currentPage = -1;
		if ( ood->pszPage == NULL ) {
			if ( !DBGetContactSettingTString( NULL, "Options", "LastPage", &dbv )) {
				lastPage = mir_tstrdup( dbv.ptszVal );
				DBFreeVariant( &dbv );
			}
		}
		else lastPage = LangPackPcharToTchar( ood->pszPage );

		if ( ood->pszGroup == NULL ) {
			if ( !DBGetContactSettingTString( NULL, "Options", "LastGroup", &dbv )) {
				lastGroup = mir_tstrdup( dbv.ptszVal );
				DBFreeVariant( &dbv );
			}
		}
		else lastGroup = LangPackPcharToTchar( ood->pszGroup );

		if ( ood->pszTab == NULL ) {
			if ( !DBGetContactSettingTString( NULL, "Options", "LastTab", &dbv )) {
				lastTab = mir_tstrdup( dbv.ptszVal );
				DBFreeVariant( &dbv );
			}
		}
		else lastTab = LangPackPcharToTchar( ood->pszTab );

		for ( i=0; i < dat->pageCount; i++, odp++ ) {
			struct OptionsPageData* opd = &dat->opd[i];
			HRSRC hrsrc=FindResourceA(odp->hInstance,odp->pszTemplate,MAKEINTRESOURCEA(5));
			HGLOBAL hglb=LoadResource(odp->hInstance,hrsrc);
			DWORD resSize=SizeofResource(odp->hInstance,hrsrc);
			opd->pTemplate = ( DLGTEMPLATE* )mir_alloc(resSize);
			memcpy(opd->pTemplate,LockResource(hglb),resSize);
			dte=(struct DlgTemplateExBegin*)opd->pTemplate;
			if ( dte->signature == 0xFFFF ) {
				//this feels like an access violation, and is according to boundschecker
				//...but it works - for now
				//may well have to remove and sort out the original dialogs
				dte->style&=~(WS_VISIBLE|WS_CHILD|WS_POPUP|WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER);
				dte->style|=WS_CHILD;
			}
			else {
				opd->pTemplate->style&=~(WS_VISIBLE|WS_CHILD|WS_POPUP|WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME|DS_CENTER);
				opd->pTemplate->style|=WS_CHILD;
			}
			opd->dlgProc=odp->pfnDlgProc;
			opd->hInst=odp->hInstance;
			opd->hwnd=NULL;
			opd->changed=0;
			opd->simpleHeight=opd->expertHeight=0;
			opd->simpleBottomControlId=odp->nIDBottomSimpleControl;
			opd->simpleWidth=opd->expertWidth=0;
			opd->simpleRightControlId=odp->nIDRightSimpleControl;
			opd->nExpertOnlyControls=odp->nExpertOnlyControls;
			opd->expertOnlyControls=odp->expertOnlyControls;
			opd->flags=odp->flags;
			opd->dwInitParam=odp->dwInitParam;
			if ( odp->pszTitle == NULL )
				opd->pszTitle = NULL;
			else if ( odp->flags & ODPF_UNICODE ) {
				#if defined ( _UNICODE )
					opd->pszTitle = ( TCHAR* )mir_wstrdup( odp->ptszTitle );
				#else
					opd->pszTitle = u2a(( WCHAR* )odp->ptszTitle );
				#endif
			}
			else opd->pszTitle = ( TCHAR* )mir_strdup( odp->pszTitle );

			if ( odp->pszGroup == NULL )
				opd->pszGroup = NULL;
			else if ( odp->flags & ODPF_UNICODE ) {
				#if defined ( _UNICODE )
					opd->pszGroup = ( TCHAR* )mir_wstrdup( odp->ptszGroup );
				#else
					opd->pszGroup = u2a(( WCHAR* )odp->ptszGroup );
				#endif
			}
			else opd->pszGroup = ( TCHAR* )mir_strdup( odp->pszGroup );

			if ( odp->pszTab == NULL )
				opd->pszTab = NULL;
			else if ( odp->flags & ODPF_UNICODE ) {
				#if defined ( _UNICODE )
					opd->pszTab = ( TCHAR* )mir_wstrdup( odp->ptszTab );
				#else
					opd->pszTab = u2a(( WCHAR* )odp->ptszTab );
				#endif
			}
			else opd->pszTab = ( TCHAR* )mir_strdup( odp->pszTab );

			if ( !lstrcmp( lastPage, odp->ptszTitle ) &&
				  !lstrcmpnull( lastGroup, odp->ptszGroup ) &&
				  (( ood->pszTab == NULL && dat->currentPage == -1 ) || !lstrcmpnull( lastTab, odp->ptszTab )))
				dat->currentPage = i;
		}
		mir_free( lastGroup );
		mir_free( lastPage );
		mir_free( lastTab );

		GetWindowRect(hdlg,&rcDlg);
		pt.x=pt.y=0;
		ClientToScreen(hdlg,&pt);
		GetWindowRect(GetDlgItem(hdlg,IDC_STATIC_FILTERCAPTION),&rc);
		dat->rcDisplay.left=rc.right-pt.x+(rc.left-rcDlg.left);
		dat->rcDisplay.top=rc.top-pt.y;
		dat->rcDisplay.right=rcDlg.right-(rc.left-rcDlg.left)-pt.x;
		GetWindowRect(GetDlgItem(hdlg,IDOK),&rc);
		dat->rcDisplay.bottom=rc.top-(rcDlg.bottom-rc.bottom)-pt.y;

		// Add an item to count in height
		tie.mask = TCIF_TEXT | TCIF_IMAGE;
		tie.iImage = -1;
		tie.pszText = _T("X");
		TabCtrl_InsertItem(GetDlgItem(hdlg,IDC_TAB), 0, &tie);

		GetWindowRect(GetDlgItem(hdlg,IDC_TAB), &dat->rcTab);
		dat->rcTab.top -= pt.y;
		dat->rcTab.bottom -= pt.y;
		dat->rcTab.left -= pt.x;
		dat->rcTab.right -= pt.x;
		TabCtrl_AdjustRect(GetDlgItem(hdlg,IDC_TAB), FALSE, &dat->rcTab);

		FillFilterCombo(hdlg, dat->opd, dat->pageCount); 
		SendMessage(hdlg,DM_REBUILDPAGETREE,0,0);

		return TRUE;
	}
	case DM_REBUILDPAGETREE:
	{	int i;
		TVINSERTSTRUCT tvis;
		TVITEMA tvi;
		char str[128],buf[130];

		HINSTANCE FilterInst=NULL;
		int FilterSelIndex=SendDlgItemMessage(hdlg,IDC_MODULES,CB_GETCURSEL,0,0);
		if (FilterSelIndex>0) 
		   FilterInst=(HINSTANCE)SendDlgItemMessage(hdlg,IDC_MODULES,CB_GETITEMDATA,FilterSelIndex,0);

		TreeView_SelectItem(GetDlgItem(hdlg,IDC_PAGETREE),NULL);
		if ( dat->currentPage != (-1))
			ShowWindow(dat->opd[dat->currentPage].hwnd,SW_HIDE);
		ShowWindow(GetDlgItem(hdlg,IDC_PAGETREE),SW_HIDE);	 //deleteall is annoyingly visible
		TreeView_DeleteAllItems(GetDlgItem(hdlg,IDC_PAGETREE));
		dat->hCurrentPage = NULL;
		tvis.hParent = NULL;
		tvis.hInsertAfter = TVI_SORT;
		tvis.item.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
		tvis.item.state = tvis.item.stateMask = TVIS_EXPANDED;
		for ( i=0; i < dat->pageCount; i++ ) {
			static TCHAR *fullTitle=NULL;
			TCHAR * useTitle;
			if (fullTitle) mir_free(fullTitle);
			fullTitle=NULL;
			if ( FilterInst!=NULL && dat->opd[i].hInst!=FilterInst ) continue;
			if (( dat->opd[i].flags & ODPF_SIMPLEONLY ) && IsDlgButtonChecked( hdlg, IDC_EXPERT )) continue;
			if (( dat->opd[i].flags & ODPF_EXPERTONLY ) && !IsDlgButtonChecked( hdlg, IDC_EXPERT )) continue;
			tvis.hParent = NULL;
			if ( FilterInst!=NULL ) {
				size_t sz=dat->opd[i].pszGroup?_tcslen(dat->opd[i].pszGroup)+1:0;
				if (sz) sz+=3;
				sz+=dat->opd[i].pszTitle?_tcslen(dat->opd[i].pszTitle)+1:0;
				fullTitle = ( TCHAR* )mir_alloc(sz*sizeof(TCHAR));
				mir_sntprintf(fullTitle,sz,(dat->opd[i].pszGroup && dat->opd[i].pszTitle)?_T("%s - %s"):_T("%s%s"),dat->opd[i].pszGroup?dat->opd[i].pszGroup:_T(""),dat->opd[i].pszTitle?dat->opd[i].pszTitle:_T("") );
			}
			useTitle=fullTitle?fullTitle:dat->opd[i].pszTitle;
			if(dat->opd[i].pszGroup != NULL && FilterInst==NULL) {
				tvis.hParent = FindNamedTreeItemAtRoot(GetDlgItem(hdlg,IDC_PAGETREE),dat->opd[i].pszGroup);
				if(tvis.hParent == NULL) {
					tvis.item.lParam = -1;
					tvis.item.pszText = dat->opd[i].pszGroup;
					tvis.hParent = TreeView_InsertItem( GetDlgItem(hdlg,IDC_PAGETREE), &tvis );
				}
			}
			else {
				TVITEM tvi;
				tvi.hItem = FindNamedTreeItemAtRoot(GetDlgItem(hdlg,IDC_PAGETREE),useTitle);
				if( tvi.hItem != NULL ) {
					if ( i == dat->currentPage ) dat->hCurrentPage=tvi.hItem;
					tvi.mask = TVIF_PARAM;
					TreeView_GetItem(GetDlgItem(hdlg,IDC_PAGETREE),&tvi);
					if ( tvi.lParam == -1 ) {
						tvi.lParam = i;
						TreeView_SetItem(GetDlgItem(hdlg,IDC_PAGETREE),&tvi);
						continue;
			}	}	}

			if ( dat->opd[i].pszTab != NULL ) {
				HTREEITEM hItem;
				if (tvis.hParent == NULL)
					hItem = FindNamedTreeItemAtRoot(GetDlgItem(hdlg,IDC_PAGETREE),useTitle);
				else
					hItem = FindNamedTreeItemAtChildren(GetDlgItem(hdlg,IDC_PAGETREE),tvis.hParent,useTitle);
				if( hItem != NULL ) {
					if ( i == dat->currentPage ) {
						TVITEM tvi;
						tvi.hItem=hItem;
						tvi.mask=TVIF_PARAM;
						tvi.lParam=dat->currentPage;
						TreeView_SetItem(GetDlgItem(hdlg,IDC_PAGETREE),&tvi);
						dat->hCurrentPage=hItem;
					}
					continue;
				}
			}

			tvis.item.pszText = useTitle;
			tvis.item.lParam = i;
			dat->opd[i].hTreeItem = TreeView_InsertItem( GetDlgItem(hdlg,IDC_PAGETREE), &tvis);
			if ( i == dat->currentPage )
				dat->hCurrentPage = dat->opd[i].hTreeItem;	

			if (fullTitle) mir_free(fullTitle);
			fullTitle=NULL;
		}
		tvi.mask = TVIF_TEXT | TVIF_STATE;
		tvi.pszText = str;
		tvi.cchTextMax = SIZEOF(str);
		tvi.hItem = TreeView_GetRoot(GetDlgItem(hdlg,IDC_PAGETREE));
		while ( tvi.hItem != NULL ) {
			if ( SendMessageA( GetDlgItem(hdlg,IDC_PAGETREE), TVM_GETITEMA, 0, (LPARAM)&tvi )) {
				wsprintfA(buf,"%s%s",OPTSTATE_PREFIX,str);
				if ( !DBGetContactSettingByte( NULL, "Options", buf, 1 ))
					TreeView_Expand( GetDlgItem(hdlg,IDC_PAGETREE), tvi.hItem, TVE_COLLAPSE );
			}
			tvi.hItem = TreeView_GetNextSibling( GetDlgItem( hdlg, IDC_PAGETREE ), tvi.hItem );
		}
		if(dat->hCurrentPage==NULL) 
		{
			dat->hCurrentPage=TreeView_GetRoot(GetDlgItem(hdlg,IDC_PAGETREE));
			dat->currentPage=-1;
		}
		TreeView_SelectItem(GetDlgItem(hdlg,IDC_PAGETREE),dat->hCurrentPage);
		ShowWindow(GetDlgItem(hdlg,IDC_PAGETREE),SW_SHOW);
		break;
	}
	case PSM_CHANGED:
		EnableWindow(GetDlgItem(hdlg,IDC_APPLY),TRUE);
		if(dat->currentPage != (-1)) dat->opd[dat->currentPage].changed=1;
		return TRUE;
	case PSM_ISEXPERT:
		SetWindowLong(hdlg,DWL_MSGRESULT,IsDlgButtonChecked(hdlg,IDC_EXPERT));
		return TRUE;
	case PSM_GETBOLDFONT:
		SetWindowLong(hdlg,DWL_MSGRESULT,(LONG)dat->hBoldFont);
		return TRUE;
	case WM_NOTIFY:
		switch(wParam) {
		case IDC_TAB:
		case IDC_PAGETREE:
			switch(((LPNMHDR)lParam)->code) {
				case TVN_ITEMEXPANDING:
					SetWindowLong(hdlg,DWL_MSGRESULT,FALSE);
					return TRUE;
				case TCN_SELCHANGING:
				case TVN_SELCHANGING:
				{	PSHNOTIFY pshn;
					if(dat->currentPage==-1 || dat->opd[dat->currentPage].hwnd==NULL) break;
					pshn.hdr.code=PSN_KILLACTIVE;
					pshn.hdr.hwndFrom=dat->opd[dat->currentPage].hwnd;
					pshn.hdr.idFrom=0;
					pshn.lParam=0;
					if(SendMessage(dat->opd[dat->currentPage].hwnd,WM_NOTIFY,0,(LPARAM)&pshn)) {
						SetWindowLong(hdlg,DWL_MSGRESULT,TRUE);
						return TRUE;
					}
					break;
				}
				case TCN_SELCHANGE:
				case TVN_SELCHANGED:
				{	BOOL tabChange = (wParam == IDC_TAB);
					ShowWindow(GetDlgItem(hdlg,IDC_STNOPAGE),SW_HIDE);
					if(dat->currentPage!=-1 && dat->opd[dat->currentPage].hwnd!=NULL) ShowWindow(dat->opd[dat->currentPage].hwnd,SW_HIDE);
					if (!tabChange) {
						TVITEM tvi;
						tvi.hItem=dat->hCurrentPage=TreeView_GetSelection(GetDlgItem(hdlg,IDC_PAGETREE));
						if(tvi.hItem==NULL) break;
						tvi.mask=TVIF_HANDLE|TVIF_PARAM;
						TreeView_GetItem(GetDlgItem(hdlg,IDC_PAGETREE),&tvi);
						dat->currentPage=tvi.lParam;
						ShowWindow(GetDlgItem(hdlg,IDC_TAB),SW_HIDE);
					} else {
						TCITEM tie;
						TVITEM tvi;

						tie.mask = TCIF_PARAM;
						TabCtrl_GetItem(GetDlgItem(hdlg,IDC_TAB),TabCtrl_GetCurSel(GetDlgItem(hdlg,IDC_TAB)),&tie);
						dat->currentPage=tie.lParam;

						tvi.hItem=dat->hCurrentPage;
						tvi.mask=TVIF_PARAM;
						tvi.lParam=dat->currentPage;
						TreeView_SetItem(GetDlgItem(hdlg,IDC_PAGETREE),&tvi);
					}
					if ( dat->currentPage != -1 ) {
						if ( dat->opd[dat->currentPage].hwnd == NULL ) {
							RECT rcPage;
							RECT rcControl,rc;
							int w,h,pages=0;

							dat->opd[dat->currentPage].hwnd=CreateDialogIndirectParamA(dat->opd[dat->currentPage].hInst,dat->opd[dat->currentPage].pTemplate,hdlg,dat->opd[dat->currentPage].dlgProc,dat->opd[dat->currentPage].dwInitParam);
							if(dat->opd[dat->currentPage].flags&ODPF_BOLDGROUPS)
								EnumChildWindows(dat->opd[dat->currentPage].hwnd,BoldGroupTitlesEnumChildren,(LPARAM)dat->hBoldFont);
							GetClientRect(dat->opd[dat->currentPage].hwnd,&rcPage);
							dat->opd[dat->currentPage].expertWidth=rcPage.right;
							dat->opd[dat->currentPage].expertHeight=rcPage.bottom;
							GetWindowRect(dat->opd[dat->currentPage].hwnd,&rc);

							if(dat->opd[dat->currentPage].simpleBottomControlId) {
								GetWindowRect(GetDlgItem(dat->opd[dat->currentPage].hwnd,dat->opd[dat->currentPage].simpleBottomControlId),&rcControl);
								dat->opd[dat->currentPage].simpleHeight=rcControl.bottom-rc.top;
							}
							else dat->opd[dat->currentPage].simpleHeight=dat->opd[dat->currentPage].expertHeight;

							if(dat->opd[dat->currentPage].simpleRightControlId) {
								GetWindowRect(GetDlgItem(dat->opd[dat->currentPage].hwnd,dat->opd[dat->currentPage].simpleRightControlId),&rcControl);
								dat->opd[dat->currentPage].simpleWidth=rcControl.right-rc.left;
							}
							else dat->opd[dat->currentPage].simpleWidth=dat->opd[dat->currentPage].expertWidth;

							if(IsDlgButtonChecked(hdlg,IDC_EXPERT)) {
								w=dat->opd[dat->currentPage].expertWidth;
								h=dat->opd[dat->currentPage].expertHeight;
							}
							else {
								int i;
								for(i=0;i<dat->opd[dat->currentPage].nExpertOnlyControls;i++)
									ShowWindow(GetDlgItem(dat->opd[dat->currentPage].hwnd,dat->opd[dat->currentPage].expertOnlyControls[i]),SW_HIDE);
								w=dat->opd[dat->currentPage].simpleWidth;
								h=dat->opd[dat->currentPage].simpleHeight;
							}
							if (dat->opd[dat->currentPage].pszTab != NULL) {
								// Count tabs to calc position
								int i;
								for ( i=0; i < dat->pageCount && pages < 2; i++ ) {
									if (( dat->opd[i].flags & ODPF_SIMPLEONLY ) && IsDlgButtonChecked( hdlg, IDC_EXPERT )) continue;
									if (( dat->opd[i].flags & ODPF_EXPERTONLY ) && !IsDlgButtonChecked( hdlg, IDC_EXPERT )) continue;
									if ( lstrcmp(dat->opd[i].pszTitle, dat->opd[dat->currentPage].pszTitle) || lstrcmpnull(dat->opd[i].pszGroup, dat->opd[dat->currentPage].pszGroup) ) continue;
									pages++;
							}	}
							dat->opd[dat->currentPage].insideTab = (pages > 1);
							if (dat->opd[dat->currentPage].insideTab) {
								SetWindowPos(dat->opd[dat->currentPage].hwnd,HWND_TOP,(dat->rcTab.left+dat->rcTab.right-w)>>1,dat->rcTab.top,w,h,0);
								ThemeDialogBackground(dat->opd[dat->currentPage].hwnd,TRUE);
							} else {
								SetWindowPos(dat->opd[dat->currentPage].hwnd,HWND_TOP,(dat->rcDisplay.left+dat->rcDisplay.right-w)>>1,(dat->rcDisplay.top+dat->rcDisplay.bottom-h)>>1,w,h,0);
								ThemeDialogBackground(dat->opd[dat->currentPage].hwnd,FALSE);
							}
						}
						if (!tabChange && dat->opd[dat->currentPage].insideTab) {
							// Make tabbed pane
							int i,pages=0,sel=0;
							TCITEM tie;
							HWND hwndTab = GetDlgItem(hdlg,IDC_TAB);

							TabCtrl_DeleteAllItems(hwndTab);
							tie.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_PARAM;
							tie.iImage = -1;
							for ( i=0; i < dat->pageCount; i++ ) {
								if (( dat->opd[i].flags & ODPF_SIMPLEONLY ) && IsDlgButtonChecked( hdlg, IDC_EXPERT )) continue;
								if (( dat->opd[i].flags & ODPF_EXPERTONLY ) && !IsDlgButtonChecked( hdlg, IDC_EXPERT )) continue;
								if ( lstrcmp(dat->opd[i].pszTitle, dat->opd[dat->currentPage].pszTitle) || lstrcmpnull(dat->opd[i].pszGroup, dat->opd[dat->currentPage].pszGroup) ) continue;

								tie.pszText = dat->opd[i].pszTab;
								tie.lParam = i;
								TabCtrl_InsertItem(hwndTab, pages, &tie);
								if ( !lstrcmp(dat->opd[i].pszTab,dat->opd[dat->currentPage].pszTab) )
									sel = pages;
								pages++;
							}
							TabCtrl_SetCurSel(hwndTab,sel);
							ShowWindow(hwndTab,SW_SHOW);
						}
						ShowWindow(dat->opd[dat->currentPage].hwnd,SW_SHOW);
						if(((LPNMTREEVIEW)lParam)->action==TVC_BYMOUSE) PostMessage(hdlg,DM_FOCUSPAGE,0,0);
						else SetFocus(GetDlgItem(hdlg,IDC_PAGETREE));
					}
					else ShowWindow(GetDlgItem(hdlg,IDC_STNOPAGE),SW_SHOW);
					break;
		}	}	}
		break;
	case DM_FOCUSPAGE:
		if(dat->currentPage==-1) break;
		SetFocus(dat->opd[dat->currentPage].hwnd);
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
			case IDC_MODULES:
				if ( HIWORD(wParam)==CBN_SELCHANGE ) {
					SaveOptionsTreeState(hdlg);
					SendMessage(hdlg,DM_REBUILDPAGETREE,0,0);
				}
				break;

			case IDC_EXPERT:
			{
				int expert=IsDlgButtonChecked(hdlg,IDC_EXPERT);
				int i,j;
				PSHNOTIFY pshn;
				RECT rcPage;
				int neww,newh;

				DBWriteContactSettingByte(NULL,"Options","Expert",(BYTE)expert);
				pshn.hdr.idFrom=0;
				pshn.lParam=expert;
				pshn.hdr.code=PSN_EXPERTCHANGED;
				for(i=0;i<dat->pageCount;i++) {
					int pages=0;

					if(dat->opd[i].hwnd==NULL) continue;
					if (( dat->opd[i].flags & ODPF_SIMPLEONLY ) && expert) continue;
					if (( dat->opd[i].flags & ODPF_EXPERTONLY ) && !expert) continue;
					pshn.hdr.hwndFrom=dat->opd[i].hwnd;
					SendMessage(dat->opd[i].hwnd,WM_NOTIFY,0,(LPARAM)&pshn);

					for(j=0;j<dat->opd[i].nExpertOnlyControls;j++)
						ShowWindow(GetDlgItem(dat->opd[i].hwnd,dat->opd[i].expertOnlyControls[j]),expert?SW_SHOW:SW_HIDE);

					if (dat->opd[i].pszTab != NULL) {
						// Count tabs to calc position
						int j;
						for ( j=0; j < dat->pageCount && pages < 2; j++ ) {
							if (( dat->opd[j].flags & ODPF_SIMPLEONLY ) && IsDlgButtonChecked( hdlg, IDC_EXPERT )) continue;
							if (( dat->opd[j].flags & ODPF_EXPERTONLY ) && !IsDlgButtonChecked( hdlg, IDC_EXPERT )) continue;
							if ( lstrcmp(dat->opd[j].pszTitle, dat->opd[i].pszTitle) || lstrcmpnull(dat->opd[j].pszGroup, dat->opd[i].pszGroup) ) continue;
							pages++;
					}	}
					dat->opd[i].insideTab = (pages > 1);

					GetWindowRect(dat->opd[i].hwnd,&rcPage);
					if(dat->opd[i].simpleBottomControlId) newh=expert?dat->opd[i].expertHeight:dat->opd[i].simpleHeight;
					else newh=rcPage.bottom-rcPage.top;
					if(dat->opd[i].simpleRightControlId) neww=expert?dat->opd[i].expertWidth:dat->opd[i].simpleWidth;
					else neww=rcPage.right-rcPage.left;
					if(i==dat->currentPage) {
						POINT ptStart,ptEnd,ptNow;
						DWORD thisTick,startTick;
						RECT rc;

						ptNow.x=ptNow.y=0;
						ClientToScreen(hdlg,&ptNow);
						GetWindowRect(dat->opd[i].hwnd,&rc);
						ptStart.x=rc.left-ptNow.x;
						ptStart.y=rc.top-ptNow.y;
						if (dat->opd[i].insideTab) {
							ptEnd.x=(dat->rcTab.left+dat->rcTab.right-neww)>>1;
							ptEnd.y=dat->rcTab.top;
						} else {
							ptEnd.x=(dat->rcDisplay.left+dat->rcDisplay.right-neww)>>1;
							ptEnd.y=(dat->rcDisplay.top+dat->rcDisplay.bottom-newh)>>1;
						}
						if(abs(ptEnd.x-ptStart.x)>5 || abs(ptEnd.y-ptStart.y)>5) {
							startTick=GetTickCount();
							SetWindowPos(dat->opd[i].hwnd,HWND_TOP,0,0,min(neww,rcPage.right),min(newh,rcPage.bottom),SWP_NOMOVE);
							UpdateWindow(dat->opd[i].hwnd);
							for(;;) {
								thisTick=GetTickCount();
								if(thisTick>startTick+100) break;
								ptNow.x=ptStart.x+(ptEnd.x-ptStart.x)*(int)(thisTick-startTick)/100;
								ptNow.y=ptStart.y+(ptEnd.y-ptStart.y)*(int)(thisTick-startTick)/100;
								SetWindowPos(dat->opd[i].hwnd,0,ptNow.x,ptNow.y,0,0,SWP_NOZORDER|SWP_NOSIZE);
							}
						}
						if (dat->opd[i].insideTab)
							ShowWindow(GetDlgItem(hdlg,IDC_TAB),SW_SHOW);
						else
							ShowWindow(GetDlgItem(hdlg,IDC_TAB),SW_HIDE);
					}

					if (dat->opd[i].insideTab) {
						SetWindowPos(dat->opd[i].hwnd,HWND_TOP,(dat->rcTab.left+dat->rcTab.right-neww)>>1,dat->rcTab.top,neww,newh,0);
						ThemeDialogBackground(dat->opd[i].hwnd,TRUE);
					} else {
						SetWindowPos(dat->opd[i].hwnd,HWND_TOP,(dat->rcDisplay.left+dat->rcDisplay.right-neww)>>1,(dat->rcDisplay.top+dat->rcDisplay.bottom-newh)>>1,neww,newh,0);
						ThemeDialogBackground(dat->opd[i].hwnd,FALSE);
					}
				}
				SaveOptionsTreeState(hdlg);
				SendMessage(hdlg,DM_REBUILDPAGETREE,0,0);
				break;
			}
			case IDCANCEL:
			{	int i;
				PSHNOTIFY pshn;
				pshn.hdr.idFrom=0;
				pshn.lParam=0;
				pshn.hdr.code=PSN_RESET;
				for(i=0;i<dat->pageCount;i++) {
					if(dat->opd[i].hwnd==NULL || !dat->opd[i].changed) continue;
					pshn.hdr.hwndFrom=dat->opd[i].hwnd;
					SendMessage(dat->opd[i].hwnd,WM_NOTIFY,0,(LPARAM)&pshn);
				}
				DestroyWindow(hdlg);
				break;
			}
			case IDOK:
			case IDC_APPLY:
			{	int i;
				PSHNOTIFY pshn;
				EnableWindow(GetDlgItem(hdlg,IDC_APPLY),FALSE);
				SetFocus(GetDlgItem(hdlg,IDC_PAGETREE));
				if(dat->currentPage!=(-1)) {
					pshn.hdr.idFrom=0;
					pshn.lParam=0;
					pshn.hdr.code=PSN_KILLACTIVE;
					pshn.hdr.hwndFrom=dat->opd[dat->currentPage].hwnd;
					if(SendMessage(dat->opd[dat->currentPage].hwnd,WM_NOTIFY,0,(LPARAM)&pshn))
						break;
				}

				pshn.hdr.code=PSN_APPLY;
				for(i=0;i<dat->pageCount;i++) {
					if(dat->opd[i].hwnd==NULL || !dat->opd[i].changed) continue;
					dat->opd[i].changed=0;
					pshn.hdr.hwndFrom=dat->opd[i].hwnd;
					if(SendMessage(dat->opd[i].hwnd,WM_NOTIFY,0,(LPARAM)&pshn)==PSNRET_INVALID_NOCHANGEPAGE) {
						dat->hCurrentPage=dat->opd[i].hTreeItem;
						TreeView_SelectItem(GetDlgItem(hdlg,IDC_PAGETREE),dat->hCurrentPage);
						if(dat->currentPage!=(-1)) ShowWindow(dat->opd[dat->currentPage].hwnd,SW_HIDE);
						dat->currentPage=i;
						if (dat->currentPage != (-1)) ShowWindow(dat->opd[dat->currentPage].hwnd,SW_SHOW);
						return 0;
				}	}

				if ( LOWORD( wParam ) == IDOK )
					DestroyWindow(hdlg);
				break;
		}	}
		break;
	case WM_DESTROY:
		SaveOptionsTreeState( hdlg );
		Window_FreeIcon_IcoLib( hdlg );
		if ( dat->currentPage != -1 ) {
			if ( dat->opd[dat->currentPage].pszTab )
				DBWriteContactSettingTString( NULL, "Options", "LastTab", dat->opd[dat->currentPage].pszTab );
			else DBDeleteContactSetting( NULL, "Options", "LastTab" );
			if ( dat->opd[dat->currentPage].pszGroup )
				DBWriteContactSettingTString( NULL, "Options", "LastGroup", dat->opd[dat->currentPage].pszGroup );
			else DBDeleteContactSetting( NULL, "Options", "LastGroup" );
			DBWriteContactSettingTString( NULL, "Options", "LastPage", dat->opd[dat->currentPage].pszTitle );
		}
		else {
			DBDeleteContactSetting(NULL,"Options","LastTab");
			DBDeleteContactSetting(NULL,"Options","LastGroup");
			DBDeleteContactSetting(NULL,"Options","LastPage");
		}
		Utils_SaveWindowPosition(hdlg, NULL, "Options", "");
		{
			int i;
			for ( i=0; i < dat->pageCount; i++ ) {
				if ( dat->opd[i].hwnd != NULL )
					DestroyWindow(dat->opd[i].hwnd);
				mir_free(dat->opd[i].pszGroup);
				mir_free(dat->opd[i].pszTab);
				mir_free(dat->opd[i].pszTitle);
				mir_free(dat->opd[i].pTemplate);
		}	}
		mir_free( dat->opd );
		DeleteObject( dat->hBoldFont );
		mir_free( dat );
		hwndOptions = NULL;
		break;
	}
	return FALSE;
}

static void FreeOptionsData( struct OptionsPageInit* popi )
{
	int i;
	for ( i=0; i < popi->pageCount; i++ ) {
		mir_free(( char* )popi->odp[i].pszTitle );
		mir_free( popi->odp[i].pszGroup );
		mir_free( popi->odp[i].pszTab );
		if (( DWORD )popi->odp[i].pszTemplate & 0xFFFF0000 )
			mir_free((char*)popi->odp[i].pszTemplate);
	}
	mir_free(popi->odp);
}

void OpenAccountOptions( PROTOACCOUNT* pa )
{
	struct OptionsPageInit opi = { 0 };
	if ( pa->ppro == NULL )
		return;

	pa->ppro->OnEvent( EV_PROTO_ONOPTIONS, ( WPARAM )&opi, 0 );
	if ( opi.pageCount > 0 ) {
		TCHAR tszTitle[ 100 ];
		OPENOPTIONSDIALOG ood = { 0 };
		PROPSHEETHEADER psh = { 0 };

		mir_sntprintf( tszTitle, SIZEOF(tszTitle), TranslateT("%s options"), pa->tszAccountName );
		
		ood.cbSize = sizeof(ood);
		ood.pszGroup = LPGEN("Network");
		ood.pszPage = mir_t2a( pa->tszAccountName );

		psh.dwSize = sizeof(psh);
		psh.dwFlags = PSH_PROPSHEETPAGE|PSH_NOAPPLYNOW;
		psh.hwndParent = NULL;
		psh.nPages = opi.pageCount;
		psh.pStartPage = (LPCTSTR)&ood;
		psh.pszCaption = tszTitle;
		psh.ppsp = (PROPSHEETPAGE*)opi.odp;
		hwndOptions = CreateDialogParam(hMirandaInst,MAKEINTRESOURCE(IDD_OPTIONS),NULL,OptionsDlgProc,(LPARAM)&psh);
		mir_free(( void* )ood.pszPage );
		FreeOptionsData( &opi );
}	}

static void OpenOptionsNow(const char *pszGroup,const char *pszPage,const char *pszTab)
{
	if ( IsWindow( hwndOptions )) {
		ShowWindow( hwndOptions, SW_RESTORE );
		SetForegroundWindow( hwndOptions );
	}
	else {
		struct OptionsPageInit opi = { 0 };
		NotifyEventHooks( hOptionsInitEvent, ( WPARAM )&opi, 0 );
		if ( opi.pageCount > 0 ) {
			OPENOPTIONSDIALOG ood = { 0 };
			PROPSHEETHEADER psh = { 0 };
			psh.dwSize = sizeof(psh);
			psh.dwFlags = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW;
			psh.nPages = opi.pageCount;
			ood.pszGroup = pszGroup;
			ood.pszPage = pszPage;
			ood.pszTab = pszTab;
			psh.pStartPage = (LPCTSTR)&ood;	  //more structure misuse
			psh.pszCaption = TranslateT("Miranda IM Options");
			psh.ppsp = (PROPSHEETPAGE*)opi.odp;		  //blatent misuse of the structure, but what the hell
			hwndOptions = CreateDialogParam(hMirandaInst,MAKEINTRESOURCE(IDD_OPTIONS),NULL,OptionsDlgProc,(LPARAM)&psh);
			FreeOptionsData( &opi );
}	}	}

static int OpenOptions(WPARAM wParam,LPARAM lParam)
{
	OPENOPTIONSDIALOG *ood=(OPENOPTIONSDIALOG*)lParam;
	if ( ood == NULL )
		return 1;

	if ( ood->cbSize == OPENOPTIONSDIALOG_OLD_SIZE )
		OpenOptionsNow( ood->pszGroup, ood->pszPage, NULL );
	else if (ood->cbSize == sizeof(OPENOPTIONSDIALOG))
		OpenOptionsNow( ood->pszGroup, ood->pszPage, ood->pszTab );
	else
		return 1;

	return 0;
}

static int OpenOptionsDialog(WPARAM wParam,LPARAM lParam)
{
	OpenOptionsNow(NULL,NULL,NULL);
	return 0;
}

static int AddOptionsPage(WPARAM wParam,LPARAM lParam)
{	OPTIONSDIALOGPAGE *odp=(OPTIONSDIALOGPAGE*)lParam, *dst;
	struct OptionsPageInit *opi=(struct OptionsPageInit*)wParam;

	if(odp==NULL||opi==NULL) return 1;
	if(odp->cbSize!=sizeof(OPTIONSDIALOGPAGE)
			&& odp->cbSize != OPTIONPAGE_OLD_SIZE
			&& odp->cbSize != OPTIONPAGE_OLD_SIZE2
			&& odp->cbSize != OPTIONPAGE_OLD_SIZE3)
		return 1;

	opi->odp=(OPTIONSDIALOGPAGE*)mir_realloc(opi->odp,sizeof(OPTIONSDIALOGPAGE)*(opi->pageCount+1));
	dst = opi->odp + opi->pageCount;
	memset( dst, 0, sizeof( OPTIONSDIALOGPAGE ));
	memcpy( dst, odp, odp->cbSize );

	if ( odp->ptszTitle != NULL ) {
		#if defined( _UNICODE )
			if ( odp->flags & ODPF_UNICODE )
				dst->ptszTitle = mir_wstrdup( TranslateW( odp->ptszTitle ));
			else {
				dst->ptszTitle = LangPackPcharToTchar( odp->pszTitle );
				dst->flags |= ODPF_UNICODE;
			}
		#else
			dst->pszTitle = mir_strdup( Translate( odp->pszTitle ));
		#endif
	}

	if ( odp->ptszGroup != NULL ) {
		#if defined( _UNICODE )
			if ( odp->flags & ODPF_UNICODE )
				dst->ptszGroup = mir_wstrdup( TranslateW( odp->ptszGroup ));
			else {
				dst->ptszGroup = LangPackPcharToTchar( odp->pszGroup );
				dst->flags |= ODPF_UNICODE;
			}
		#else
			dst->pszGroup = mir_strdup( Translate( odp->pszGroup ));
		#endif
	}

	if ( odp->cbSize > OPTIONPAGE_OLD_SIZE2 && odp->ptszTab != NULL ) {
		#if defined( _UNICODE )
			if ( odp->flags & ODPF_UNICODE )
				dst->ptszTab = mir_wstrdup( TranslateW( odp->ptszTab ));
			else {
				dst->ptszTab = LangPackPcharToTchar( odp->pszTab );
				dst->flags |= ODPF_UNICODE;
			}
		#else
			dst->pszTab = mir_strdup( Translate( odp->pszTab ));
		#endif
	}

	if (( DWORD )odp->pszTemplate & 0xFFFF0000 )
		dst->pszTemplate = mir_strdup( odp->pszTemplate );

	opi->pageCount++;
	return 0;
}

static int OptModulesLoaded(WPARAM wParam,LPARAM lParam)
{
	CLISTMENUITEM mi = { 0 };
	mi.cbSize = sizeof(mi);
	mi.flags = CMIF_ICONFROMICOLIB;
	mi.icolibItem = GetSkinIconHandle( SKINICON_OTHER_OPTIONS );
	mi.position = 1900000000;
	mi.pszName = LPGEN("&Options...");
	mi.pszService = "Options/OptionsCommand";
	CallService( MS_CLIST_ADDMAINMENUITEM, 0, ( LPARAM )&mi );
	return 0;
}

int ShutdownOptionsModule(WPARAM wParam,LPARAM lParam)
{
	if (IsWindow(hwndOptions)) DestroyWindow(hwndOptions);
	hwndOptions=NULL;
	return 0;
}

int LoadOptionsModule(void)
{
	hwndOptions=NULL;
	hOptionsInitEvent=CreateHookableEvent(ME_OPT_INITIALISE);
	CreateServiceFunction(MS_OPT_ADDPAGE,AddOptionsPage);
	CreateServiceFunction(MS_OPT_OPENOPTIONS,OpenOptions);
	CreateServiceFunction("Options/OptionsCommand",OpenOptionsDialog);
	HookEvent(ME_SYSTEM_MODULESLOADED,OptModulesLoaded);
	HookEvent(ME_SYSTEM_PRESHUTDOWN,ShutdownOptionsModule);

	if (IsWinVerXPPlus()) {
		HMODULE hThemeAPI = GetModuleHandleA("uxtheme");
		if (hThemeAPI)
			MyEnableThemeDialogTexture = (pfnEnableThemeDialogTexture)GetProcAddress(hThemeAPI,"EnableThemeDialogTexture");
	}
	return 0;
}

static void FillFilterCombo(HWND hDlg, struct OptionsPageData * opd, int PageCount)
{
	int i;
	int index;
	HINSTANCE* KnownInstances = ( HINSTANCE* )alloca(sizeof(HINSTANCE)*PageCount);
	int countKnownInst = 0;
	TCHAR* tszModuleName = ( TCHAR* )alloca(MAX_PATH*sizeof(TCHAR));
	SendDlgItemMessage(hDlg, IDC_MODULES,(UINT) CB_RESETCONTENT, 0,0);
	index=SendDlgItemMessage(hDlg, IDC_MODULES,(UINT) CB_ADDSTRING,(WPARAM)0, (LPARAM)TranslateT("<all modules>"));
	SendDlgItemMessage(hDlg, IDC_MODULES,(UINT) CB_SETITEMDATA,(WPARAM)index, (LPARAM)NULL);
	index=SendDlgItemMessage(hDlg, IDC_MODULES,(UINT) CB_ADDSTRING,(WPARAM)0, (LPARAM)TranslateT("<core settings>"));
	SendDlgItemMessage(hDlg, IDC_MODULES,(UINT) CB_SETITEMDATA,(WPARAM)index, (LPARAM)hMirandaInst);
	for (i=0; i<PageCount; i++)
	{		
		TCHAR * dllName;
		int j;
		HINSTANCE inst=opd[i].hInst;
		if (inst==hMirandaInst) continue;
		for (j=0; j<countKnownInst; j++)
			if (KnownInstances[j]==inst) break;
		if (j!=countKnownInst) continue;
		KnownInstances[countKnownInst]=inst;
		countKnownInst++;
		GetModuleFileName(inst, tszModuleName, MAX_PATH*sizeof(TCHAR));
	    dllName=_tcsrchr(tszModuleName,_T('\\'));
		if (!dllName) dllName=tszModuleName;
		else dllName++;		
		{
			index=SendDlgItemMessage(hDlg, IDC_MODULES,(UINT) CB_ADDSTRING,(WPARAM)0, (LPARAM)dllName);
			SendDlgItemMessage(hDlg, IDC_MODULES,(UINT) CB_SETITEMDATA,(WPARAM)index, (LPARAM)inst);
		}	
	}
	SendDlgItemMessage(hDlg, IDC_MODULES,(UINT) CB_SETCURSEL,(WPARAM)0, (LPARAM)0);
}
