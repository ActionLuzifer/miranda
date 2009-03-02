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

#include "../../core/commonheaders.h"
#include "../srfile/file.h"

static BOOL bModuleInitialized = FALSE;
static HANDLE hIniChangeNotification;
extern TCHAR mirandabootini[MAX_PATH];

static BOOL CALLBACK InstallIniDlgProc(HWND hwndDlg,UINT message,WPARAM wParam,LPARAM lParam)
{
	switch(message) {
		case WM_INITDIALOG:
			TranslateDialogDefault(hwndDlg);
			SetDlgItemText(hwndDlg, IDC_ININAME, (TCHAR*)lParam);
			{	
                TCHAR szSecurity[11];
                const TCHAR *pszSecurityInfo;
			  	
                GetPrivateProfileString(_T("AutoExec"), _T("Warn"), _T("notsafe"), szSecurity, SIZEOF(szSecurity), mirandabootini);
				if(!lstrcmpi(szSecurity, _T("all")))
					pszSecurityInfo = _T("Security systems to prevent malicious changes are in place and you will be warned before every change that is made.");
				else if (!lstrcmpi(szSecurity, _T("onlyunsafe")))
					pszSecurityInfo = _T("Security systems to prevent malicious changes are in place and you will be warned before changes that are known to be unsafe.");
				else if (!lstrcmpi(szSecurity, _T("none")))
					pszSecurityInfo = _T("Security systems to prevent malicious changes have been disabled. You will receive no further warnings.");
				else pszSecurityInfo = NULL;
				if (pszSecurityInfo) SetDlgItemText(hwndDlg, IDC_SECURITYINFO, TranslateTS(pszSecurityInfo));
			}
			return TRUE;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDC_VIEWINI:
				{	TCHAR szPath[MAX_PATH];
					GetDlgItemText(hwndDlg, IDC_ININAME, szPath, SIZEOF(szPath));
					ShellExecute(hwndDlg, _T("open"), szPath, NULL, NULL, SW_SHOW);
					break;
				}
				case IDOK:
				case IDCANCEL:
				case IDC_NOTOALL:
					EndDialog(hwndDlg,LOWORD(wParam));
					break;
			}
			break;
	}
	return FALSE;
}

static int IsInSpaceSeparatedList(const char *szWord,const char *szList)
{
	const char *szItem,*szEnd;
	int wordLen = lstrlenA(szWord);

	for(szItem = szList;;) {
		szEnd = strchr(szItem,' ');
		if (szEnd == NULL)
			return !lstrcmpA( szItem, szWord );
		if ( szEnd - szItem == wordLen ) {
			if ( !strncmp( szItem, szWord, wordLen ))
				return 1;
		}
		szItem = szEnd+1;
}	}

struct warnSettingChangeInfo_t {
	TCHAR *szIniPath;
	char *szSection;
	char *szSafeSections;
	char *szUnsafeSections;
	char *szName;
	char *szValue;
	int warnNoMore,cancel;
};

static BOOL CALLBACK WarnIniChangeDlgProc(HWND hwndDlg,UINT message,WPARAM wParam,LPARAM lParam)
{
	static struct warnSettingChangeInfo_t *warnInfo;

	switch(message) {
		case WM_INITDIALOG:
		{	char szSettingName[256];
			const TCHAR *pszSecurityInfo;
			warnInfo=(struct warnSettingChangeInfo_t*)lParam;
			TranslateDialogDefault(hwndDlg);
			SetDlgItemText(hwndDlg,IDC_ININAME,warnInfo->szIniPath);
			lstrcpyA(szSettingName,warnInfo->szSection);
			lstrcatA(szSettingName," / ");
			lstrcatA(szSettingName,warnInfo->szName);
			SetDlgItemTextA(hwndDlg,IDC_SETTINGNAME,szSettingName);
			SetDlgItemTextA(hwndDlg,IDC_NEWVALUE,warnInfo->szValue);
			if(IsInSpaceSeparatedList(warnInfo->szSection,warnInfo->szSafeSections))
				pszSecurityInfo=LPGENT("This change is known to be safe.");
			else if(IsInSpaceSeparatedList(warnInfo->szSection,warnInfo->szUnsafeSections))
				pszSecurityInfo=LPGENT("This change is known to be potentially hazardous.");
			else
				pszSecurityInfo=LPGENT("This change is not known to be safe.");
			SetDlgItemText(hwndDlg,IDC_SECURITYINFO,TranslateTS(pszSecurityInfo));
			return TRUE;
		}
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
				case IDCANCEL:
					warnInfo->cancel=1;
				case IDYES:
				case IDNO:
					warnInfo->warnNoMore=IsDlgButtonChecked(hwndDlg,IDC_WARNNOMORE);
					EndDialog(hwndDlg,LOWORD(wParam));
					break;
			}
			break;
	}
	return FALSE;
}

static BOOL CALLBACK IniImportDoneDlgProc(HWND hwndDlg,UINT message,WPARAM wParam,LPARAM lParam)
{
	switch(message) {
		case WM_INITDIALOG:
			TranslateDialogDefault(hwndDlg);
			SetDlgItemText(hwndDlg,IDC_ININAME,(TCHAR*)lParam);
			SetDlgItemText(hwndDlg,IDC_NEWNAME,(TCHAR*)lParam);
			return TRUE;
		case WM_COMMAND:
		{	TCHAR szIniPath[MAX_PATH];
			GetDlgItemText(hwndDlg,IDC_ININAME,szIniPath,SIZEOF(szIniPath));
			switch(LOWORD(wParam)) {
				case IDC_DELETE:
					DeleteFile(szIniPath);
				case IDC_LEAVE:
					EndDialog(hwndDlg,LOWORD(wParam));
					break;
				case IDC_RECYCLE:
					{	SHFILEOPSTRUCT shfo={0};
						shfo.wFunc=FO_DELETE;
						shfo.pFrom=szIniPath;
						szIniPath[lstrlen(szIniPath)+1]='\0';
						shfo.fFlags=FOF_NOCONFIRMATION|FOF_NOERRORUI|FOF_SILENT;
						SHFileOperation(&shfo);
					}
					EndDialog(hwndDlg,LOWORD(wParam));
					break;
				case IDC_MOVE:
					{	TCHAR szNewPath[MAX_PATH];
						GetDlgItemText(hwndDlg,IDC_NEWNAME,szNewPath,SIZEOF(szNewPath));
						MoveFile(szIniPath,szNewPath);
					}
					EndDialog(hwndDlg,LOWORD(wParam));
					break;
			}
			break;
		}
	}
	return FALSE;
}

static void DoAutoExec(void)
{
	TCHAR szUse[7], szIniPath[MAX_PATH], szFindPath[MAX_PATH];
	char szLine[2048];
	TCHAR *str2;
	FILE *fp;
	char szSection[128];
	int lineLength;
	TCHAR buf[2048], szSecurity[11], szOverrideSecurityFilename[MAX_PATH];
    char *szSafeSections, *szUnsafeSections;
	int warnThisSection=0;

	GetPrivateProfileString(_T("AutoExec"),_T("Use"),_T("prompt"),szUse,SIZEOF(szUse),mirandabootini);
	if(!lstrcmpi(szUse,_T("no"))) return;
	GetPrivateProfileString(_T("AutoExec"),_T("Safe"),_T("CLC Icons CLUI CList SkinSounds"),buf,SIZEOF(buf),mirandabootini);
    szSafeSections = mir_t2a(buf);
	GetPrivateProfileString(_T("AutoExec"),_T("Unsafe"),_T("ICQ MSN"),buf,SIZEOF(buf),mirandabootini);
    szUnsafeSections = mir_t2a(buf);
	GetPrivateProfileString(_T("AutoExec"),_T("Warn"),_T("notsafe"),szSecurity,SIZEOF(szSecurity),mirandabootini);
	GetPrivateProfileString(_T("AutoExec"),_T("OverrideSecurityFilename"),_T(""),szOverrideSecurityFilename,SIZEOF(szOverrideSecurityFilename),mirandabootini);
	
	GetPrivateProfileString(_T("AutoExec"),_T("Glob"),_T("autoexec_*.ini"),szFindPath,SIZEOF(szFindPath),mirandabootini);
    
	REPLACEVARSDATA dat = {0};
	dat.cbSize = sizeof(dat);
    dat.dwFlags = RVF_TCHAR;

    TCHAR* szExpandedFindPath = (TCHAR*)CallService(MS_UTILS_REPLACEVARS, (WPARAM)szFindPath, (LPARAM)&dat);
    pathToAbsoluteT(szExpandedFindPath, szFindPath, NULL);
    mir_free(szExpandedFindPath);

	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(szFindPath, &fd);
    if (hFind == INVALID_HANDLE_VALUE) 
    {
        mir_free(szSafeSections);
        mir_free(szUnsafeSections);
        return;
    }

    str2 = _tcsrchr(szFindPath, '\\');
	if (str2 == NULL) szFindPath[0] = 0;
	else str2[1] = 0;

    szSection[0]='\0';
	do {
		lstrcpy(szIniPath, szFindPath);
		lstrcat(szIniPath, fd.cFileName);
		if(!lstrcmpi(szUse,_T("prompt")) && lstrcmpi(fd.cFileName,szOverrideSecurityFilename)) {
			int result=DialogBoxParam(hMirandaInst,MAKEINTRESOURCE(IDD_INSTALLINI),NULL,InstallIniDlgProc,(LPARAM)szIniPath);
			if(result==IDC_NOTOALL) break;
			if(result==IDCANCEL) continue;
		}
		fp=_tfopen(szIniPath,_T("rt"));
		while(!feof(fp)) {
			if(fgets(szLine,sizeof(szLine),fp)==NULL) break;
			lineLength=lstrlenA(szLine);
			while(lineLength && (BYTE)(szLine[lineLength-1])<=' ') szLine[--lineLength]='\0';
			if(szLine[0]==';' || szLine[0]<=' ') continue;
			if(szLine[0]=='[') {
				char *szEnd=strchr(szLine+1,']');
				if(szEnd==NULL) continue;
				if(szLine[1]=='!')
					szSection[0]='\0';
				else {
					lstrcpynA(szSection,szLine+1,min(sizeof(szSection),szEnd-szLine));
					if(!lstrcmpi(szSecurity,_T("none"))) warnThisSection=0;
					else if(!lstrcmpi(szSecurity,_T("notsafe")))
						warnThisSection=!IsInSpaceSeparatedList(szSection,szSafeSections);
					else if(!lstrcmpi(szSecurity,_T("onlyunsafe")))
						warnThisSection=IsInSpaceSeparatedList(szSection,szUnsafeSections);
					else warnThisSection=1;
					if(!lstrcmpi(fd.cFileName,szOverrideSecurityFilename)) warnThisSection=0;
				}
			}
			else {
				char *szValue;
				char szName[128];
				struct warnSettingChangeInfo_t warnInfo;

				if(szSection[0]=='\0') continue;
				szValue=strchr(szLine,'=');
				if(szValue==NULL) continue;
				lstrcpynA(szName,szLine,min(sizeof(szName),szValue-szLine+1));
				szValue++;
				warnInfo.szIniPath=szIniPath;
				warnInfo.szName=szName;
				warnInfo.szSafeSections=szSafeSections;
				warnInfo.szSection=szSection;
				warnInfo.szUnsafeSections=szUnsafeSections;
				warnInfo.szValue=szValue;
				warnInfo.warnNoMore=0;
				warnInfo.cancel=0;
				if(!warnThisSection || IDNO!=DialogBoxParam(hMirandaInst,MAKEINTRESOURCE(IDD_WARNINICHANGE),NULL,WarnIniChangeDlgProc,(LPARAM)&warnInfo)) {
					if(warnInfo.cancel) break;
					if(warnInfo.warnNoMore) warnThisSection=0;
					switch(szValue[0]) {
						case 'b':
						case 'B':
							DBWriteContactSettingByte(NULL,szSection,szName,(BYTE)strtol(szValue+1,NULL,0));
							break;
						case 'w':
						case 'W':
							DBWriteContactSettingWord(NULL,szSection,szName,(WORD)strtol(szValue+1,NULL,0));
							break;
						case 'd':
						case 'D':
							DBWriteContactSettingDword(NULL,szSection,szName,(DWORD)strtoul(szValue+1,NULL,0));
							break;
						case 'l':
						case 'L':
							DBDeleteContactSetting(NULL,szSection,szName);
							break;
						case 's':
						case 'S':
							DBWriteContactSettingString(NULL,szSection,szName,szValue+1);
							break;
						case 'u':
						case 'U':
							DBWriteContactSettingStringUtf(NULL,szSection,szName,szValue+1);
							break;
						case 'n':
						case 'N':
							{	PBYTE buf;
								int len;
								char *pszValue,*pszEnd;
								DBCONTACTWRITESETTING cws;

								buf=(PBYTE)mir_alloc(lstrlenA(szValue+1));
								for(len=0,pszValue=szValue+1;;len++) {
									buf[len]=(BYTE)strtol(pszValue,&pszEnd,0x10);
									if(pszValue==pszEnd) break;
									pszValue=pszEnd;
								}
								cws.szModule=szSection;
								cws.szSetting=szName;
								cws.value.type=DBVT_BLOB;
								cws.value.pbVal=buf;
								cws.value.cpbVal=len;
								CallService(MS_DB_CONTACT_WRITESETTING,(WPARAM)(HANDLE)NULL,(LPARAM)&cws);
								mir_free(buf);
							}
							break;
						default:
							MessageBox(NULL,TranslateT("Invalid setting type. The first character of every value must be b, w, d, l, s or n."),TranslateT("Install Database Settings"),MB_OK);
							break;
					}
				}
			}
		}
		fclose(fp);
		if(!lstrcmpi(fd.cFileName,szOverrideSecurityFilename))
			DeleteFile(szIniPath);
		else {
			TCHAR szOnCompletion[8];
			GetPrivateProfileString(_T("AutoExec"),_T("OnCompletion"),_T("recycle"),szOnCompletion,SIZEOF(szOnCompletion),mirandabootini);
			if(!lstrcmpi(szOnCompletion,_T("delete")))
				DeleteFile(szIniPath);
			else if(!lstrcmpi(szOnCompletion,_T("recycle"))) {
				SHFILEOPSTRUCT shfo={0};
				shfo.wFunc=FO_DELETE;
				shfo.pFrom=szIniPath;
				szIniPath[lstrlen(szIniPath)+1]='\0';
				shfo.fFlags=FOF_NOCONFIRMATION|FOF_NOERRORUI|FOF_SILENT;
				SHFileOperation(&shfo);
			}
			else if(!lstrcmpi(szOnCompletion,_T("rename"))) {
				TCHAR szRenamePrefix[MAX_PATH];
				TCHAR szNewPath[MAX_PATH];
				GetPrivateProfileString(_T("AutoExec"),_T("RenamePrefix"),_T("done_"),szRenamePrefix,SIZEOF(szRenamePrefix),mirandabootini);
				lstrcpy(szNewPath,szFindPath);
				lstrcat(szNewPath,szRenamePrefix);
				lstrcat(szNewPath,fd.cFileName);
				MoveFile(szIniPath,szNewPath);
			}
			else if(!lstrcmpi(szOnCompletion,_T("ask")))
				DialogBoxParam(hMirandaInst,MAKEINTRESOURCE(IDD_INIIMPORTDONE),NULL,IniImportDoneDlgProc,(LPARAM)szIniPath);
		}
	} while (FindNextFile(hFind, &fd));
	FindClose(hFind);
    mir_free(szSafeSections);
    mir_free(szUnsafeSections);
}

static int CheckIniImportNow(WPARAM, LPARAM)
{
	DoAutoExec();
	FindNextChangeNotification(hIniChangeNotification);
	return 0;
}

int InitIni(void)
{
	TCHAR szMirandaDir[MAX_PATH], *str2;

	bModuleInitialized = TRUE;

	DoAutoExec();
	GetModuleFileName(hMirandaInst, szMirandaDir, SIZEOF(szMirandaDir));
	str2 = _tcsrchr(szMirandaDir,'\\');
	if (str2!=NULL) *str2=0;
	hIniChangeNotification=FindFirstChangeNotification(szMirandaDir, 0, FILE_NOTIFY_CHANGE_FILE_NAME);
	if (hIniChangeNotification != INVALID_HANDLE_VALUE) {
		CreateServiceFunction("DB/Ini/CheckImportNow", CheckIniImportNow);
		CallService(MS_SYSTEM_WAITONHANDLE, (WPARAM)hIniChangeNotification, (LPARAM)"DB/Ini/CheckImportNow");
	}
	return 0;
}

void UninitIni(void)
{
	if ( !bModuleInitialized ) return;
	CallService(MS_SYSTEM_REMOVEWAIT,(WPARAM)hIniChangeNotification,0);
	FindCloseChangeNotification(hIniChangeNotification);
}