/*

Jabber Protocol Plugin for Miranda IM
Copyright ( C ) 2002-04  Santithorn Bunchua
Copyright ( C ) 2005-07  George Hazan

Idea & portions of code by Artem Shpynov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or ( at your option ) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

File name      : $Source: /cvsroot/miranda/miranda/protocols/JabberG/jabber_misc.cpp,v $
Revision       : $Revision: 3322 $
Last change on : $Date: 2006-07-13 16:11:29 +0400
Last change by : $Author: rainwater $

*/

#include "jabber.h"
#include "jabber_list.h"

#include <commctrl.h>
#include "m_icolib.h"

#include "resource.h"

#define IDI_ONLINE                      104
#define IDI_OFFLINE                     105
#define IDI_AWAY                        128
#define IDI_FREE4CHAT                   129
#define IDI_INVISIBLE                   130
#define IDI_NA                          131
#define IDI_DND                         158
#define IDI_OCCUPIED                    159
#define IDI_ONTHEPHONE                  1002
#define IDI_OUTTOLUNCH                  1003

HIMAGELIST hAdvancedStatusIcon = NULL;
struct
{
	TCHAR* mask;
	char*  proto;
	int    startIndex;
}
static TransportProtoTable[] =
{
	{ _T("|icq*|jit*"), "ICQ",  -1},
	{ _T("msn*"),       "MSN",  -1},
	{ _T("yahoo*"),     "YAHOO",-1},
	{ _T("mrim*"),      "MRA",  -1},
	{ _T("aim*"),       "AIM",  -1},
	//request #3094
	{ _T("gg*"),        "GaduGadu",   -1},
	{ _T("tv*"),        "TV",         -1},
	{ _T("dict*"),      "Dictionary", -1},
	{ _T("weather*"),   "Weather",    -1},
	{ _T("sms*"),       "SMS",        -1},
	{ _T("smtp*"),      "SMTP",       -1},
	//j2j 
	{ _T("gtalk*"),     "GTalk", -1},
	{ _T("xmpp*"),   "Jabber2Jabber", -1},
	//jabbim.cz - services
	{ _T("disk*"),      "Jabber Disk", -1},
	{ _T("irc*"),       "IRC", -1},
	{ _T("rss*"),       "RSS", -1},
	{ _T("tlen*"),      "Tlen", -1}
};

static int skinIconStatusToResourceId[] = {IDI_OFFLINE,IDI_ONLINE,IDI_AWAY,IDI_DND,IDI_NA,IDI_NA,/*IDI_OCCUPIED,*/IDI_FREE4CHAT,IDI_INVISIBLE,IDI_ONTHEPHONE,IDI_OUTTOLUNCH};
static int skinStatusToJabberStatus[] = {0,1,2,3,4,4,6,7,2,2};

/////////////////////////////////////////////////////////////////////////////////////////
// Icons init

struct
{
	char*  szDescr;
	char*  szName;
	int    defIconID;
	HANDLE hIconLibItem;
}
static iconList[] =
{
	{   LPGEN("Protocol icon"),         "main",             IDI_JABBER            },
	{   LPGEN("Agents list"),           "Agents",           IDI_AGENTS            },
	{   LPGEN("Change password"),       "key",              IDI_KEYS              },
	{   LPGEN("Multi-User Conference"), "group",            IDI_GROUP             },
	{   LPGEN("Personal vCard"),        "vcard",            IDI_VCARD             },
	{   LPGEN("Request authorization"), "Request",          IDI_REQUEST           },
	{   LPGEN("Grant authorization"),   "Grant",            IDI_GRANT             },
	{   LPGEN("Revoke authorization"),  "Revoke",           IDI_AUTHREVOKE        },
	{   LPGEN("Convert to room"),       "convert",          IDI_USER2ROOM         },
	{   LPGEN("Add to roster"),         "addroster",        IDI_ADDROSTER         },
	{   LPGEN("Login/logout"),          "trlogonoff",       IDI_LOGIN             },
	{   LPGEN("Resolve nicks"),         "trresolve",        IDI_REFRESH           },
	{   LPGEN("Bookmarks"),             "bookmarks",        IDI_BOOKMARKS         }, 
	{   LPGEN("Privacy Lists"),         "privacylists",     IDI_PRIVACY_LISTS     },
	{   LPGEN("Service Discovery"),     "servicediscovery", IDI_SERVICE_DISCOVERY },
	{   LPGEN("View as tree"),          "sd_view_tree",     IDI_VIEW_TREE         },
	{   LPGEN("View as list"),          "sd_view_list",     IDI_VIEW_LIST         },
	{   LPGEN("Navigate home"),         "sd_nav_home",      IDI_NAV_HOME          },
	{   LPGEN("Refresh node"),          "sd_nav_refresh",   IDI_NAV_REFRESH       },
	{   LPGEN("Browse node"),           "sd_browse",        IDI_BROWSE            },
	{   LPGEN("Apply filter"),          "sd_filter_apply",  IDI_FILTER_APPLY      },
	{   LPGEN("Reset filter"),          "sd_filter_reset",  IDI_FILTER_RESET      },
	{   LPGEN("Discovery succeeded"),   "disco_ok",         IDI_DISCO_OK          },
	{   LPGEN("Discovery failed"),      "disco_fail",       IDI_DISCO_FAIL        },
	{   LPGEN("Discovery in progress"), "disco_progress",   IDI_DISCO_PROGRESS    },
	{   LPGEN("RSS service"),           "node_rss",         IDI_NODE_RSS          },
	{   LPGEN("Server"),                "node_server",      IDI_NODE_SERVER       },
	{   LPGEN("Storage service"),       "node_store",       IDI_NODE_STORE        },
	{   LPGEN("Weather service"),       "node_weather",     IDI_NODE_WEATHER      },
	{   LPGEN("AdHoc Command"),         "adhoc",            IDI_COMMAND           },
};

void JabberIconsInit( void )
{
	SKINICONDESC sid = {0};
	char szFile[MAX_PATH];
	GetModuleFileNameA(hInst, szFile, MAX_PATH);

	sid.cbSize = sizeof(SKINICONDESC);
	sid.pszDefaultFile = szFile;
	sid.cx = sid.cy = 16;
	sid.pszSection = Translate( jabberProtoName );

	for ( int i = 0; i < SIZEOF(iconList); i++ ) {
		char szSettingName[100];
		mir_snprintf( szSettingName, sizeof( szSettingName ), "%s_%s", jabberProtoName, iconList[i].szName );
		sid.pszName = szSettingName;
		sid.pszDescription = Translate( iconList[i].szDescr );
		sid.iDefaultIndex = -iconList[i].defIconID;
		iconList[i].hIconLibItem = ( HANDLE )CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
}	}

HANDLE __stdcall GetIconHandle( int iconId )
{
	for ( int i=0; i < SIZEOF(iconList); i++ )
		if ( iconList[i].defIconID == iconId )
			return iconList[i].hIconLibItem;

	return NULL;
}

HICON LoadIconEx( const char* name )
{
	char szSettingName[100];
	mir_snprintf( szSettingName, sizeof( szSettingName ), "%s_%s", jabberProtoName, name );
	return ( HICON )JCallService( MS_SKIN2_GETICON, 0, (LPARAM)szSettingName );
}

/////////////////////////////////////////////////////////////////////////////////////////
// internal functions

static inline TCHAR qtoupper( TCHAR c )
{
	return ( c >= 'a' && c <= 'z' ) ? c - 'a' + 'A' : c;
}

static BOOL WildComparei( const TCHAR* name, const TCHAR* mask )
{
	const TCHAR* last='\0';
	for ( ;; mask++, name++) {
		if ( *mask != '?' && qtoupper( *mask ) != qtoupper( *name ))
			break;
		if ( *name == '\0' )
			return ((BOOL)!*mask);
	}

	if ( *mask != '*' )
		return FALSE;

	for (;; mask++, name++ ) {
		while( *mask == '*' ) {
			last = mask++;
			if ( *mask == '\0' )
				return ((BOOL)!*mask);   /* true */
		}

		if ( *name == '\0' )
			return ((BOOL)!*mask);      /* *mask == EOS */
		if ( *mask != '?' && qtoupper( *mask ) != qtoupper( *name ))
			name -= (size_t)(mask - last) - 1, mask = last;
}	}

static BOOL MatchMask( const TCHAR* name, const TCHAR* mask)
{
	if ( !mask || !name )
		return mask == name;

	if ( *mask != '|' )
		return WildComparei( name, mask );

	TCHAR* temp = NEWTSTR_ALLOCA(mask);
	for ( int e=1; mask[e] != '\0'; e++ ) {
		int s = e;
		while ( mask[e] != '\0' && mask[e] != '|')
			e++;

		memcpy( temp, mask+s, e-s );
		temp[e-s] = '\0';
		if ( WildComparei( name, temp ))
			return TRUE;

		if ( mask[e] == 0 )
			return FALSE;
	}

	return FALSE;
}

static HICON ExtractIconFromPath(const char *path, BOOL * needFree)
{
	char *comma;
	char file[MAX_PATH],fileFull[MAX_PATH];
	int n;
	HICON hIcon;
	lstrcpynA(file,path,sizeof(file));
	comma=strrchr(file,',');
	if(comma==NULL) n=0;
	else {n=atoi(comma+1); *comma=0;}
	CallService(MS_UTILS_PATHTOABSOLUTE, (WPARAM)file, (LPARAM)fileFull);
	hIcon=NULL;
	ExtractIconExA(fileFull,n,NULL,&hIcon,1);
	if (needFree)
		*needFree=(hIcon!=NULL);

	return hIcon;
}

static HICON LoadTransportIcon(char *filename,int i,char *IconName,char *SectName,char *Description,int internalidx, BOOL * needFree)
{
	char szPath[MAX_PATH],szMyPath[MAX_PATH], szFullPath[MAX_PATH],*str;
	HICON hIcon=NULL;
	BOOL has_proto_icon=FALSE;
	SKINICONDESC sid={0};
	if (needFree) *needFree=FALSE;
	GetModuleFileNameA(GetModuleHandle(NULL), szPath, MAX_PATH);
	str=strrchr(szPath,'\\');
	if(str!=NULL) *str=0;
	_snprintf(szMyPath, sizeof(szMyPath), "%s\\Icons\\%s", szPath, filename);
	_snprintf(szFullPath, sizeof(szFullPath), "%s\\Icons\\%s,%d", szPath, filename, i);
	BOOL nf;
	HICON hi=ExtractIconFromPath(szFullPath,&nf);
	if (hi) has_proto_icon=TRUE;
	if (hi && nf) DestroyIcon(hi);
	if (!ServiceExists(MS_SKIN2_ADDICON)) {
		hIcon=ExtractIconFromPath(szFullPath,needFree);
		if (hIcon) return hIcon;
		_snprintf(szFullPath, sizeof(szFullPath), "%s,%d", szMyPath, internalidx);
		hIcon=ExtractIconFromPath(szFullPath,needFree);
		if (hIcon) return hIcon;
	}
	else {
		if ( IconName != NULL && SectName != NULL)  {
			sid.cbSize = sizeof(sid);
			sid.cx=16;
			sid.cy=16;
			sid.hDefaultIcon = (has_proto_icon)?NULL:(HICON)CallService(MS_SKIN_LOADPROTOICON,(WPARAM)NULL,(LPARAM)(-internalidx));
			sid.pszSection = Translate(SectName);
			sid.pszName=IconName;
			sid.pszDescription=Description;
			sid.pszDefaultFile=szMyPath;
			sid.iDefaultIndex=i;
			CallService(MS_SKIN2_ADDICON, 0, (LPARAM)&sid);
		}
		return ((HICON)CallService(MS_SKIN2_GETICON, 0, (LPARAM)IconName));
	}
	return NULL;
}

static HICON LoadSmallIcon(HINSTANCE hInstance, LPCTSTR lpIconName)
{
	HICON hIcon=NULL;                 // icon handle
	int index=-(int)lpIconName;
	TCHAR filename[MAX_PATH]={0};
	GetModuleFileName(hInstance,filename,MAX_PATH);
	ExtractIconEx(filename,index,NULL,&hIcon,1);
	return hIcon;
}

static int LoadAdvancedIcons(int iID)
{
	int i;
	char * proto=TransportProtoTable[iID].proto;
	char * defFile[MAX_PATH]={0};
	char * Group[255];
	char * Uname[255];
	int first=-1;
	HICON empty=LoadSmallIcon(NULL,MAKEINTRESOURCE(102));

	_snprintf((char *)Group, sizeof(Group),"%s/%s/%s %s",Translate("Status Icons"), jabberModuleName, proto, Translate("transport"));
	_snprintf((char *)defFile, sizeof(defFile),"proto_%s.dll",proto);
	if (!hAdvancedStatusIcon)
		hAdvancedStatusIcon=(HIMAGELIST)CallService(MS_CLIST_GETICONSIMAGELIST,0,0);

	EnterCriticalSection( &modeMsgMutex );
	for (i=0; i<ID_STATUS_ONTHEPHONE-ID_STATUS_OFFLINE; i++) {
		HICON hicon;
		BOOL needFree;
		int n=skinStatusToJabberStatus[i];
		char * descr=(char*)CallService(MS_CLIST_GETSTATUSMODEDESCRIPTION,n+ID_STATUS_OFFLINE,0);
		_snprintf((char *)Uname, sizeof(Uname),"%s_Transport_%s_%d",jabberProtoName,proto,n);
		hicon=(HICON)LoadTransportIcon((char*)defFile,-skinIconStatusToResourceId[i],(char*)Uname,(char*)Group,(char*)descr,-(n+ID_STATUS_OFFLINE),&needFree);
		int index=(TransportProtoTable[iID].startIndex == -1)?-1:TransportProtoTable[iID].startIndex+n;
		int added=ImageList_ReplaceIcon(hAdvancedStatusIcon,index,hicon?hicon:empty);
		if (first == -1) first=added;
		if (hicon && needFree) DestroyIcon(hicon);
	}

	if ( TransportProtoTable[ iID ].startIndex == -1 )
		TransportProtoTable[ iID ].startIndex = first;
	LeaveCriticalSection( &modeMsgMutex );
	return 0;
}

static int GetTransportProtoID( TCHAR* TransportDomain )
{
	for ( int i=0; i<SIZEOF(TransportProtoTable); i++ )
		if ( MatchMask( TransportDomain, TransportProtoTable[i].mask ))
			return i;

	return -1;
}

static int GetTransportStatusIconIndex(int iID, int Status)
{
	if ( iID < 0 || iID >= SIZEOF( TransportProtoTable ))
		return -1;

	//icons not loaded - loading icons
	if ( TransportProtoTable[iID].startIndex == -1 )
		LoadAdvancedIcons( iID );

	//some fault on loading icons
	if ( TransportProtoTable[ iID ].startIndex == -1 )
		return -1;

	if ( Status < ID_STATUS_OFFLINE )
		Status = ID_STATUS_OFFLINE;

	return TransportProtoTable[iID].startIndex + skinStatusToJabberStatus[ Status - ID_STATUS_OFFLINE ];
}

/////////////////////////////////////////////////////////////////////////////////////////
// a hook for the IcoLib plugin

int ReloadIconsEventHook(WPARAM wParam, LPARAM lParam)
{
	for ( int i=0; i < SIZEOF(TransportProtoTable); i++ )
		if ( TransportProtoTable[i].startIndex != -1 )
			LoadAdvancedIcons(i);

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Prototype for Jabber and other protocols to return index of Advanced status
// wParam - HCONTACT of called protocol
// lParam - should be 0 (reserverd for futher usage)
// return value: -1 - no Advanced status
// : other - index of icons in clcimagelist.
// if imagelist require advanced painting status overlay(like xStatus)
// index should be shifted to HIWORD, LOWORD should be 0

int JGetAdvancedStatusIcon(WPARAM wParam, LPARAM lParam)
{
	HANDLE hContact=(HANDLE) wParam;
	if ( !hContact )
		return -1;

	if ( !JGetByte( hContact, "IsTransported", 0 ))
		return -1;

	DBVARIANT dbv;
	if ( JGetStringT( hContact, "Transport", &dbv ))
		return -1;

	int iID = GetTransportProtoID( dbv.ptszVal );
	DBFreeVariant(&dbv);
	if ( iID >= 0 ) {
		WORD Status = ID_STATUS_OFFLINE;
		Status = JGetWord( hContact, "Status", ID_STATUS_OFFLINE );
		if ( Status < ID_STATUS_OFFLINE )
			Status = ID_STATUS_OFFLINE;
		else if (Status > ID_STATUS_INVISIBLE )
			Status = ID_STATUS_ONLINE;
		return GetTransportStatusIconIndex( iID, Status );
	}
	return -1;
}

/////////////////////////////////////////////////////////////////////////////////////////
//   Transport check functions

static int PushIconLibRegistration( TCHAR* TransportDomain ) //need to push Imagelist addition to
{
	if ( ServiceExists( MS_SKIN2_ADDICON ))
		return GetTransportStatusIconIndex( GetTransportProtoID( TransportDomain ), ID_STATUS_OFFLINE );

	return 0;
}

BOOL JabberDBCheckIsTransportedContact(const TCHAR* jid, HANDLE hContact)
{
	// check if transport is already set
	if ( !jid || !hContact )
		return FALSE;

	// strip domain part from jid
	TCHAR* domain  = _tcschr(( TCHAR* )jid, '@' );
	BOOL   isAgent = (domain == NULL) ? TRUE : FALSE;
	BOOL   isTransported = FALSE;
	if ( domain!=NULL )
		domain = NEWTSTR_ALLOCA(domain+1);
	else
		domain = NEWTSTR_ALLOCA(jid);

	TCHAR* resourcepos = _tcschr( domain, '/' );
	if ( resourcepos != NULL )
		*resourcepos = '\0';

	for ( int i=0; i < SIZEOF(TransportProtoTable); i++ ) {
		if ( MatchMask( domain, TransportProtoTable[i].mask )) {
			PushIconLibRegistration( domain );
			isTransported = TRUE;
			break;
	}	}

	if ( jabberTransports.getIndex( domain ) == -1 ) {
		if ( isAgent ) {
			jabberTransports.insert( _tcsdup(domain) ); 
			JSetByte( hContact, "IsTransport", 1 );
	}	}

	if ( isTransported ) {
		JSetStringT( hContact, "Transport", domain );
		JSetByte( hContact, "IsTransported", 1 );
	}
	return isTransported;
}

void JabberCheckAllContactsAreTransported()
{
	HANDLE hContact = ( HANDLE ) JCallService( MS_DB_CONTACT_FINDFIRST, 0, 0 );
	while ( hContact != NULL ) {
		char* szProto = ( char* )JCallService( MS_PROTO_GETCONTACTBASEPROTO, ( WPARAM ) hContact, 0 );
		if ( !lstrcmpA( jabberProtoName, szProto )) {
			DBVARIANT dbv;
			if ( !JGetStringT( hContact, "jid", &dbv )) {
				JabberDBCheckIsTransportedContact( dbv.ptszVal, hContact );
				JFreeVariant( &dbv );
		}	}

		hContact = ( HANDLE )JCallService( MS_DB_CONTACT_FINDNEXT, ( WPARAM )hContact, 0 );
}	}
