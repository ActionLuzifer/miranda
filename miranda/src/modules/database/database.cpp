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
#include "profilemanager.h"
#include "../srfile/file.h"

// from the plugin loader, hate extern but the db frontend is pretty much tied
extern PLUGINLINK pluginCoreLink;
// contains the location of mirandaboot.ini
extern TCHAR mirandabootini[MAX_PATH];
TCHAR szDefaultMirandaProfile[MAX_PATH];

// returns 1 if the profile path was returned, without trailing slash
int getProfilePath(TCHAR * buf, size_t cch)
{
	TCHAR profiledir[MAX_PATH];
	GetPrivateProfileString(_T("Database"), _T("ProfileDir"), _T(""), profiledir, SIZEOF(profiledir), mirandabootini);

	REPLACEVARSDATA dat = {0};
	dat.cbSize = sizeof( dat );
    dat.dwFlags = RVF_TCHAR;

    if (profiledir[0] == 0)
        _tcscpy(profiledir, _T("%miranda_path%"));

    TCHAR* exprofiledir = (TCHAR*)CallService(MS_UTILS_REPLACEVARS, (WPARAM)profiledir, (LPARAM)&dat);
    size_t len = pathToAbsoluteT(exprofiledir, buf, NULL);
    mir_free(exprofiledir);

    
    if (len < (cch-1)) _tcscat(buf, _T("\\"));
	return 0;
}

// returns 1 if *.dat spec is matched
int isValidProfileName(const TCHAR *name)
{
    int len = _tcslen(name) - 4;
    return len > 0 && _tcsicmp(&name[len], _T(".dat")) == 0;
}

// returns 1 if a single profile (full path) is found within the profile dir
static int getProfile1(TCHAR * szProfile, size_t cch, TCHAR * profiledir, BOOL * noProfiles)
{
	unsigned int found = 0;

	TCHAR searchspec[MAX_PATH];
    mir_sntprintf(searchspec, SIZEOF(searchspec), _T("%s*.dat"), profiledir);
	
	WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(searchspec, &ffd);
	if (hFind != INVALID_HANDLE_VALUE) 
	{
        do
        {
		    // make sure the first hit is actually a *.dat file
		    if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) 
		    {
			    // copy the profile name early cos it might be the only one
			    if (++found == 1 && szProfile[0] == 0) 
                    mir_sntprintf(szProfile, cch, _T("%s%s"), profiledir, ffd.cFileName);
		    }
        }
	    while (FindNextFile(hFind, &ffd));
		FindClose(hFind);
	}

	if (noProfiles) 
		*noProfiles = found == 0;

    return found == 1;
}

// returns 1 if something that looks like a profile is there
static int getProfileCmdLineArgs(TCHAR * szProfile, size_t cch)
{
	TCHAR *szCmdLine=GetCommandLine();
	TCHAR *szEndOfParam;
	TCHAR szThisParam[1024];
	int firstParam=1;

    REPLACEVARSDATA dat = {0};
    dat.cbSize = sizeof(dat);
    dat.dwFlags = RVF_TCHAR;

    while(szCmdLine[0]) {
		if(szCmdLine[0]=='"') {
			szEndOfParam=_tcsrchr(szCmdLine+1,'"');
			if(szEndOfParam==NULL) break;
			lstrcpyn(szThisParam,szCmdLine+1,min( SIZEOF(szThisParam),szEndOfParam-szCmdLine));
			szCmdLine=szEndOfParam+1;
		}
		else {
			szEndOfParam=szCmdLine+_tcscspn(szCmdLine,_T(" \t"));
			lstrcpyn(szThisParam,szCmdLine,min( SIZEOF(szThisParam),szEndOfParam-szCmdLine+1));
			szCmdLine=szEndOfParam;
		}
		while(*szCmdLine && *szCmdLine<=' ') szCmdLine++;
		if(firstParam) {firstParam=0; continue;}   //first param is executable name
		if(szThisParam[0]=='/' || szThisParam[0]=='-') continue;  //no switches supported

        TCHAR* res = (TCHAR*)CallService(MS_UTILS_REPLACEVARS, (WPARAM)szThisParam, (LPARAM)&dat);
        if (res == NULL) return 0;
        _tcsncpy(szProfile, res, cch); szProfile[cch-1] = 0;
        mir_free(res);
		return 1;
	}
	return 0;
}

// returns 1 if a valid filename (incl. dat) is found, includes fully qualified path
static int getProfileCmdLine(TCHAR * szProfile, size_t cch, TCHAR * profiledir)
{
	TCHAR buf[MAX_PATH];
	int rc = 0;
	if ( getProfileCmdLineArgs(buf, SIZEOF(buf)) ) {
		// have something that looks like a .dat, with or without .dat in the filename
        pathToAbsoluteT(buf, szProfile, profiledir); 
        if (!isValidProfileName(szProfile))
            _tcscat(szProfile, _T(".dat"));

        TCHAR *p = _tcsrchr(buf, '\\');
        if (p)
        {
            *p = 0;
            size_t len = _tcslen(profiledir);
            mir_sntprintf(profiledir+len, cch-len, _T("%s\\"), buf); 
        }
	}
	return rc;
}

// returns 1 if the profile manager should be shown
static int showProfileManager(void)
{
	TCHAR Mgr[32];
	// is control pressed?
	if (GetAsyncKeyState(VK_CONTROL)&0x8000) return 1;
	// wanna show it?
	GetPrivateProfileString(_T("Database"), _T("ShowProfileMgr"), _T("never"), Mgr, SIZEOF(Mgr), mirandabootini);
	if ( _tcsicmp(Mgr, _T("yes")) == 0 ) return 1;
	return 0;
}

// returns 1 if a default profile should be selected instead of showing the manager.
static int getProfileAutoRun(TCHAR * szProfile, size_t cch, TCHAR * profiledir)
{
	TCHAR Mgr[32];
	GetPrivateProfileString(_T("Database"), _T("ShowProfileMgr"), _T(""), Mgr, SIZEOF(Mgr), mirandabootini);
	if (_tcsicmp(Mgr, _T("never"))) return 0;		

	if (szDefaultMirandaProfile[0] == 0) return 0;

	mir_sntprintf(szProfile, cch, _T("%s%s.dat"), profiledir, szDefaultMirandaProfile);

    return 1;
}

bool shouldAutoCreate(void)
{
	TCHAR ac[32];
	GetPrivateProfileString(_T("Database"), _T("AutoCreate"), _T("no"), ac, SIZEOF(ac), mirandabootini);
	return _tcsicmp(ac, _T("yes")) == 0;
}

 static void getDefaultProfile(void)
{
	TCHAR defaultProfile[MAX_PATH];
	GetPrivateProfileString(_T("Database"), _T("DefaultProfile"), _T(""), defaultProfile, SIZEOF(defaultProfile), mirandabootini);

    REPLACEVARSDATA dat = {0};
    dat.cbSize = sizeof(dat);
    dat.dwFlags = RVF_TCHAR;

    TCHAR* res = (TCHAR*)CallService(MS_UTILS_REPLACEVARS, (WPARAM)defaultProfile, (LPARAM)&dat);
    if (res)
    {
        _tcsncpy(szDefaultMirandaProfile, res, SIZEOF(szDefaultMirandaProfile)); 
        szDefaultMirandaProfile[SIZEOF(szDefaultMirandaProfile)-1] = 0;
        mir_free(res);
    }
    else
        szDefaultMirandaProfile[0] = 0;
}

// returns 1 if a profile was selected
static int getProfile(TCHAR * szProfile, size_t cch)
{
	TCHAR profiledir[MAX_PATH];
    PROFILEMANAGERDATA pd = {0};

    getProfilePath(profiledir, SIZEOF(profiledir));
    getDefaultProfile();
	if (getProfileCmdLine(szProfile, cch, profiledir)) return 1;
	if (getProfileAutoRun(szProfile, cch, profiledir)) return 1;

    if ( !showProfileManager() && getProfile1(szProfile, cch, profiledir, &pd.noProfiles) ) return 1;
	else {		
		pd.szProfile=szProfile;
		pd.szProfileDir=profiledir;
		return getProfileManager(&pd);
	}
}

// called by the UI, return 1 on success, use link to create profile, set error if any
int makeDatabase(TCHAR * profile, DATABASELINK * link, HWND hwndDlg)
{
	TCHAR buf[256];
	int err=0;	
	// check if the file already exists
	TCHAR * file = _tcsrchr(profile,'\\');
	if (file) file++;
	if (_taccess(profile, 6) == 0) {		
		mir_sntprintf(buf, SIZEOF(buf), TranslateTS( _T("The profile '%s' already exists. Do you want to move it to the ")
			_T("Recycle Bin? \n\nWARNING: The profile will be deleted if Recycle Bin is disabled.\nWARNING: A profile may contain confidential information and should be properly deleted.")),file);
		// file already exists!
		if ( MessageBox(hwndDlg, buf, TranslateT("The profile already exists"), MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2) != IDYES ) return 0;
		// move the file
		{		
			SHFILEOPSTRUCT sf;
			ZeroMemory(&sf,sizeof(sf));
			sf.wFunc=FO_DELETE;
			sf.pFrom=buf;
			sf.fFlags=FOF_NOCONFIRMATION|FOF_NOERRORUI|FOF_SILENT;
			mir_sntprintf(buf, SIZEOF(buf), _T("%s\0"), profile);
			if ( SHFileOperation(&sf) != 0 ) {
				mir_sntprintf(buf, SIZEOF(buf),TranslateT("Couldn't move '%s' to the Recycle Bin, Please select another profile name."),file);
				MessageBox(0,buf,TranslateT("Problem moving profile"),MB_ICONINFORMATION|MB_OK);
				return 0;
			}
		}
		// now the file should be gone!
	}
	// ask the database to create the profile
    char *prf = mir_t2a(profile);
	if ( link->makeDatabase(prf, &err) ) { 
		mir_sntprintf(buf, SIZEOF(buf),TranslateT("Unable to create the profile '%s', the error was %x"),file, err);
		MessageBox(hwndDlg,buf,TranslateT("Problem creating profile"),MB_ICONERROR|MB_OK);
        mir_free(prf);
		return 0;
	}
	// the profile has been created! woot
    mir_free(prf);
	return 1;
}

// enumerate all plugins that had valid DatabasePluginInfo()
static int FindDbPluginForProfile(char*, DATABASELINK * dblink, LPARAM lParam)
{
    int res = DBPE_CONT;
	if ( dblink && dblink->cbSize == sizeof(DATABASELINK) ) 
    {
	    char * szProfile = mir_t2a((TCHAR*) lParam);
		// liked the profile?
		int err = 0;
		if (dblink->grokHeader(szProfile, &err) == 0) { 			
			// added APIs?
            res = dblink->Load(szProfile, &pluginCoreLink) ? DBPE_HALT : DBPE_DONE;
		} else {
			res = DBPE_HALT;
			switch ( err ) {				 
				case EGROKPRF_CANTREAD:
				case EGROKPRF_UNKHEADER:
				{
					// just not supported.
					res = DBPE_CONT;
				}
				case EGROKPRF_VERNEWER:
				case EGROKPRF_DAMAGED:
					break;
			}
		} //if
        mir_free(szProfile);
	}
	return res;
}

// enumerate all plugins that had valid DatabasePluginInfo()
static int FindDbPluginAutoCreate(char*, DATABASELINK * dblink, LPARAM lParam)
{
    int res = DBPE_CONT;
	if (dblink && dblink->cbSize == sizeof(DATABASELINK)) 
    {
		int err;
	    char *szProfile = mir_t2a((TCHAR*)lParam);
		if (dblink->makeDatabase(szProfile, &err) == 0) 
            res = dblink->Load(szProfile, &pluginCoreLink) ? DBPE_HALT : DBPE_DONE;
        mir_free(szProfile);
	}
	return res;
}

typedef struct {
	TCHAR * profile;
	UINT msg;
	ATOM aPath;
	int found;
} ENUMMIRANDAWINDOW;

static BOOL CALLBACK EnumMirandaWindows(HWND hwnd, LPARAM lParam)
{
	TCHAR classname[256];
	ENUMMIRANDAWINDOW * x = (ENUMMIRANDAWINDOW *)lParam;
	DWORD res=0;
	if ( GetClassName(hwnd,classname,SIZEOF(classname)) && lstrcmp( _T("Miranda"),classname)==0 ) {		
		if ( SendMessageTimeout(hwnd, x->msg, (WPARAM)x->aPath, 0, SMTO_ABORTIFHUNG, 100, &res) && res ) {
			x->found++;
			return FALSE;
		}
	}
	return TRUE;
}

static int FindMirandaForProfile(TCHAR * szProfile)
{
	ENUMMIRANDAWINDOW x={0};
	x.profile=szProfile;
	x.msg=RegisterWindowMessage( _T( "Miranda::ProcessProfile" ));
	x.aPath=GlobalAddAtom(szProfile);
	EnumWindows(EnumMirandaWindows, (LPARAM)&x);
	GlobalDeleteAtom(x.aPath);
	return x.found;
}

int LoadDatabaseModule(void)
{
	int iReturn = 0;
	TCHAR szProfile[MAX_PATH];
	szProfile[0]=0;

	// load the older basic services of the db
	InitTime();
	InitUtils();

	// find out which profile to load
	if ( getProfile(szProfile, SIZEOF(szProfile)) )
	{
		int rc;
		PLUGIN_DB_ENUM dbe;

		dbe.cbSize=sizeof(PLUGIN_DB_ENUM);
		dbe.lParam=(LPARAM)szProfile;

        if (_taccess(szProfile, 0) && shouldAutoCreate())
		    dbe.pfnEnumCallback=( int(*) (char*,void*,LPARAM) )FindDbPluginAutoCreate;
        else
		    dbe.pfnEnumCallback=( int(*) (char*,void*,LPARAM) )FindDbPluginForProfile;

		// find a driver to support the given profile
		rc=CallService(MS_PLUGINS_ENUMDBPLUGINS, 0, (LPARAM)&dbe);
		switch ( rc ) {
			case -1: {
				// no plugins at all
				TCHAR buf[256];
				TCHAR * p = _tcsrchr(szProfile,'\\');
				mir_sntprintf(buf,SIZEOF(buf),TranslateT("Miranda is unable to open '%s' because you do not have any profile plugins installed.\nYou need to install dbx_3x.dll or equivalent."), p ? ++p : szProfile );
				MessageBox(0,buf,TranslateT("No profile support installed!"),MB_OK | MB_ICONERROR);
				break;
			}
			case 1:
				// if there were drivers but they all failed cos the file is locked, try and find the miranda which locked it
				if ( !FindMirandaForProfile(szProfile) ) {
					// file isn't locked, just no driver could open it.
					TCHAR buf[256];
					TCHAR * p = _tcsrchr(szProfile,'\\');
					mir_sntprintf(buf,SIZEOF(buf),TranslateT("Miranda was unable to open '%s', its in an unknown format.\nThis profile might also be damaged, please run DB-tool which should be installed."), p ? ++p : szProfile);
					MessageBox(0,buf,TranslateT("Miranda can't understand that profile"),MB_OK | MB_ICONERROR);
				}
				break;
		}
		iReturn = (rc != 0);
	}
	else
	{
		iReturn = 1;
	}

	return iReturn;
}