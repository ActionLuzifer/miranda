/*
Plugin of Miranda IM for communicating with users of the MSN Messenger protocol. 
Copyright(C) 2002-2004 George Hazan (modification) and Richard Hughes (original)

Miranda IM: the free icq client for MS Windows 
Copyright (C) 2000-2002 Richard Hughes, Roland Rabien & Tristan Van de Vreede

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

#include "msn_global.h"

#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>

int MSN_HandleCommands(ThreadData *info,char *cmdString);
int MSN_HandleErrors(ThreadData *info,char *cmdString);
extern LONG (WINAPI *MyInterlockedIncrement)(PLONG pVal);

//=======================================================================================
//	Keep-alive thread for the main connection
//=======================================================================================

void __cdecl msn_keepAliveThread(ThreadData *info)
{
	while( TRUE )
	{
		if ( SleepEx( 60000, TRUE ) == WAIT_IO_COMPLETION ) 
			break;

		/*
		 * if proxy is not used, every connection uses select() to send PNG
		 */

		if ( msnLoggedIn && !MyOptions.UseGateway )
			if ( MSN_GetByte( "KeepAlive", 0 )) 
				MSN_WS_Send( msnNSSocket, "PNG\r\n", 5 );
}	}

//=======================================================================================
//	MSN File Transfer Protocol commands processing
//=======================================================================================

int MSN_HandleMSNFTP( ThreadData *info, char *cmdString )
{
	char* params = "";
	filetransfer* ft = info->ft;

	if ( cmdString[ 3 ] )
		params = cmdString+4;

	switch((*(PDWORD)cmdString&0x00FFFFFF)|0x20000000) 
	{
		case ' EYB':    //********* BYE
		{
			MSN_SendBroadcast( ft->std.hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0 );
			return 1;
		}
		case ' LIF':    //********* FIL
		{	
			char filesize[ 30 ];
			if ( sscanf( params, "%s", filesize ) < 1 )
				goto LBL_InvalidCommand;
			
			info->mCaller = 1;
			MSN_WS_Send( info->s, "TFR\r\n", 5 );
			_chdir( ft->std.workingDir );

			char filefull[ 1024 ];
			_snprintf( filefull, sizeof( filefull ), "%s\\%s", ft->std.workingDir, ft->std.currentFile );
			ft->std.currentFile = strdup( filefull );

			if ( msnRunningUnderNT )
				ft->fileId = _wopen( ft->wszFileName, _O_BINARY | _O_CREAT | _O_TRUNC | _O_WRONLY, _S_IREAD | _S_IWRITE);
			else
				ft->fileId = _open( ft->std.currentFile, _O_BINARY | _O_CREAT | _O_TRUNC | _O_WRONLY, _S_IREAD | _S_IWRITE);
			if ( ft->fileId == -1 )
				break;

			MSN_SendBroadcast( ft->std.hContact, ACKTYPE_FILE, ACKRESULT_INITIALISING, ft, 0);
			break;
		}
		case ' RFT':    //********* TFR
		{
			char* sendpacket = ( char* )alloca( 2048 );
			int wPlace;
			WORD packetLen;
			filetransfer* ft = info->ft;

			info->mCaller = 3;
			ft->std.currentFileSize = ft->std.totalBytes;
			ft->std.currentFileProgress = 0;
			ft->fileId = _open( ft->std.currentFile, _O_BINARY | _O_RDONLY );
			if ( ft->fileId == -1 )
				break;

			MSN_SendBroadcast( ft->std.hContact, ACKTYPE_FILE, ACKRESULT_INITIALISING, ft, 0 );
			while( info->mTotalSend < ft->std.currentFileSize ) 
			{
				if ( ft->bCanceled )
					break;

				wPlace=0;
				sendpacket[wPlace++]=0x00;
				packetLen=(((ft->std.currentFileSize)-(info->mTotalSend)) > 2045) ? 2045: (WORD)((ft->std.currentFileSize)-(info->mTotalSend));
				sendpacket[wPlace++]=(packetLen & 0x00ff);
				sendpacket[wPlace++]=((packetLen & 0xff00) >> 8);
				_read(ft->fileId, &sendpacket[wPlace], packetLen);

				info->mTotalSend+=packetLen;
				MSN_WS_Send( info->s, &sendpacket[0], packetLen+3 );

				ft->std.totalProgress += packetLen;
				ft->std.currentFileProgress += packetLen;

				MSN_SendBroadcast( ft->std.hContact, ACKTYPE_FILE, ACKRESULT_DATA, ft, ( LPARAM )&ft->std );
			}
			
			ft->bCompleted = true;
			ft->close();
			break;
		}
		case ' RSU':    //********* USR
		{
			char email[130],authcookie[14];
			if ( sscanf(params,"%129s %13s",email,authcookie) < 2 ) 
			{
				MSN_DebugLog( "Invalid USR OK command, ignoring" );
				break;
			}

			char tCommand[ 30 ];
			_snprintf( tCommand, sizeof( tCommand ), "FIL %i\r\n", info->ft->std.totalBytes );
			MSN_WS_Send( info->s, tCommand, strlen( tCommand ));
			break;
		}
		case ' REV':    //********* VER
		{	
			char protocol1[ 7 ];
			if ( sscanf( params, "%6s", protocol1 ) < 1 ) 
			{	
LBL_InvalidCommand:
				MSN_DebugLog( "Invalid %.3s command, ignoring", cmdString );
				break;
			}

			if ( strcmp( protocol1, "MSNFTP" ) != 0 )
			{	
				int tempInt;
				int tFieldCount = sscanf( params, "%d %6s", &tempInt, protocol1 );
				if ( tFieldCount != 2 || strcmp( protocol1, "MSNFTP" ) != 0 )
				{
					MSN_DebugLog( "Another side requested the unknown protocol (%s), closing thread", params );
					return 1;
			}	}

			{	DBVARIANT dbv;
				if ( !DBGetContactSetting( NULL, msnProtocolName, "e-mail", &dbv ))
				{
					if ( info->mCaller == 0 )  //receive
					{
						char tCommand[ MSN_MAX_EMAIL_LEN + 50 ];
						_snprintf( tCommand, sizeof( tCommand ), "USR %s %s\r\n", dbv.pszVal, info->mCookie );
						MSN_WS_Send( info->s, tCommand, strlen( tCommand ));
					}
					else if ( info->mCaller == 2 )  //send
					{	
						static char sttCommand[] = "VER MSNFTP\r\n";
						MSN_WS_Send( info->s, sttCommand, strlen( sttCommand ));
					}

					MSN_FreeVariant( &dbv );
			}	}
			break;
		}
		default:		// receiving file
		{
			HReadBuffer tBuf( info, int( cmdString - info->mData ));

			while ( true ) 
			{
				if ( ft->bCanceled )
				{	MSN_WS_Send( info->s, "CCL\r\n", 5 );
					close( ft->fileId );
					return 0;
				}

				BYTE* p = tBuf.surelyRead( info->s, 3 );
				if ( p == NULL ) {
LBL_Error:		ft->close();
					MSN_ShowError( "file transfer is canceled by remote host" );
					return 1;
				}

				BYTE tIsTransitionFinished = *p++;
				WORD dataLen = *p++;
				dataLen |= (*p++ << 8);

				if ( tIsTransitionFinished ) {
LBL_Success:	ft->bCompleted = true;
					MSN_SendBroadcast( ft->std.hContact, ACKTYPE_FILE, ACKRESULT_SUCCESS, ft, 0);
					ft->close();

					static char sttCommand[] = "BYE 16777989\r\n";
					MSN_WS_Send( info->s, sttCommand, strlen( sttCommand ));
					return 0;
				}

				p = tBuf.surelyRead( info->s, dataLen );
				if ( p == NULL )
					goto LBL_Error;

				_write( ft->fileId, p, dataLen );
				info->mTotalSend += dataLen;
					
				ft->std.totalProgress += dataLen;
				ft->std.currentFileProgress += dataLen;

				MSN_SendBroadcast( ft->std.hContact, ACKTYPE_FILE, ACKRESULT_DATA, ft, ( LPARAM )&ft->std );

				if ( info->mTotalSend == ft->std.totalBytes )
					goto LBL_Success;
	}	}	}

	return 0;
}

//=======================================================================================
//	MSN server thread - read and process commands from a server
//=======================================================================================

void __cdecl MSNServerThread( ThreadData* info )
{
	MSN_DebugLog( "Thread started: server='%s', type=%d", info->mServer, info->mType );

	NETLIBOPENCONNECTION tConn;
	tConn.cbSize = sizeof( tConn );
	tConn.flags = 0;

 	char* tPortDelim = strrchr( info->mServer, ':' );
	if ( tPortDelim != NULL )
		*tPortDelim = '\0';

	if ( MyOptions.UseGateway && !MyOptions.UseProxy ) {
		tConn.szHost = MSN_DEFAULT_GATEWAY;
		tConn.wPort = 80;
	}
	else {
		tConn.szHost = info->mServer;
		tConn.wPort = MSN_DEFAULT_PORT;

		if ( tPortDelim != NULL ) {
			int tPortNumber;
			if ( sscanf( tPortDelim+1, "%d", &tPortNumber ) == 1 )
				tConn.wPort = ( WORD )tPortNumber;
	}	}

	info->s = ( HANDLE )MSN_CallService( MS_NETLIB_OPENCONNECTION, ( WPARAM )hNetlibUser, ( LPARAM )&tConn );
	if ( info->s == NULL ) {
		MSN_DebugLog( "Connection Failed (%d)", WSAGetLastError() );

		switch ( info->mType ) {
		case SERVER_NOTIFICATION:
		case SERVER_DISPATCH:
			MSN_SendBroadcast( NULL, ACKTYPE_LOGIN, ACKRESULT_FAILED, NULL, LOGINERR_NOSERVER );
			MSN_GoOffline();
			break;
		}

		return;
	}

	MSN_DebugLog( "Connected with handle=%08X", info->s );

	if ( MyOptions.UseGateway ) {
		info->mGatewayTimeout = 2;
		MSN_CallService( MS_NETLIB_SETPOLLINGTIMEOUT, WPARAM( info->s ), 2 );
	}

	if ( info->mType == SERVER_DISPATCH || info->mType == SERVER_NOTIFICATION ) {
		if ( MSN_GetByte( "UseMsnp10", 0 ))
			MSN_SendPacket( info->s, "VER", "MSNP10 MSNP9 CVR0" );
		else
			MSN_SendPacket( info->s, "VER", "MSNP9 MSNP8 CVR0" );
	}
	else if ( info->mType == SERVER_SWITCHBOARD ) {
		char tEmail[ MSN_MAX_EMAIL_LEN ];
		MSN_GetStaticString( "e-mail", NULL, tEmail, sizeof( tEmail ));
		MSN_SendPacket( info->s, info->mCaller ? "USR" : "ANS", "%s %s", tEmail, info->mCookie );
	} 
	else if ( info->mType == SERVER_FILETRANS && info->mCaller == 0 ) {
		MSN_WS_Send( info->s, "VER MSNFTP\r\n", 12 );
	}

	bool tIsMainThread = false;
	if ( info->mType == SERVER_NOTIFICATION )
		tIsMainThread = true;
	else if ( info->mType == SERVER_DISPATCH && MyOptions.UseGateway )
		tIsMainThread = true;

	if ( tIsMainThread ) {
		MSN_EnableMenuItems( TRUE );

		msnNSSocket = info->s;
		msnLoggedIn = true;

		ThreadData* newThread = new ThreadData;
		memcpy( newThread, info, sizeof( ThreadData ));
		newThread->startThread(( pThreadFunc )msn_keepAliveThread );
	}

	MSN_DebugLog( "Entering main recv loop" );
	info->mBytesInData = 0;
	while ( TRUE ) {
		int handlerResult;

		int recvResult = MSN_WS_Recv( info->s, info->mData + info->mBytesInData, sizeof( info->mData ) - info->mBytesInData );
		if ( recvResult == SOCKET_ERROR ) {
			MSN_DebugLog( "Connection %08p [%d] was abortively closed", info->s, GetCurrentThreadId());
			break;
		}

		if ( !recvResult ) {
			MSN_DebugLog( "Connection %08p [%d] was gracefully closed", info->s, GetCurrentThreadId());
			break;
		}

		info->mBytesInData += recvResult;

		if ( info->mCaller == 1 && info->mType == SERVER_FILETRANS ) {
			handlerResult = MSN_HandleMSNFTP( info, info->mData );
			if ( handlerResult ) 
				break;
		} 
		else {
			while( TRUE ) {
				char* peol = strchr(info->mData,'\r');
				if ( peol == NULL )
					break;

				if ( info->mBytesInData < peol-info->mData+2 )
					break;  //wait for full line end

				char msg[ sizeof(info->mData) ];
				memcpy( msg, info->mData, peol-info->mData ); msg[ peol-info->mData ] = 0;

				if ( *++peol != '\n' )
					MSN_DebugLog( "Dodgy line ending to command: ignoring" );
				else
					peol++;

				info->mBytesInData -= peol - info->mData;
				memmove( info->mData, peol, info->mBytesInData );

				MSN_DebugLog( "RECV:%s", msg );

				if ( !isalnum( msg[0] ) || !isalnum(msg[1]) || !isalnum(msg[2]) || (msg[3] && msg[3]!=' ')) {
					MSN_DebugLog( "Invalid command name" );
					continue;
				}

				if ( info->mType != SERVER_FILETRANS ) {
					if ( isdigit(msg[0]) && isdigit(msg[1]) && isdigit(msg[2]))   //all error messages
						handlerResult = MSN_HandleErrors( info, msg );
					else
						handlerResult = MSN_HandleCommands( info, msg );
				}
				else handlerResult = MSN_HandleMSNFTP( info, msg );

				if ( handlerResult )
					break;
		}	}

		if ( info->mBytesInData == sizeof( info->mData )) {
			MSN_DebugLog( "sizeof(data) is too small: the longest line won't fit" );
			break;
	}	}

	if ( tIsMainThread ) {
		MSN_GoOffline();
	}
	else if ( info->mType == SERVER_SWITCHBOARD ) {
		if ( info->mJoinedContacts != NULL )
			free( info->mJoinedContacts );
	}

	MSN_DebugLog( "Closing connection handle %08X", info->s );
	Netlib_CloseHandle( info->s );

	if ( tIsMainThread )
		msnNSSocket = NULL;

	MSN_DebugLog( "Thread ending now" );
}

/***************************************************************************************/

void __cdecl MSNSendfileThread( ThreadData* info )
{
	MSN_DebugLog( "Waiting for an incoming connection to '%s'...", info->mServer );

	filetransfer* ft = info->ft;

	switch( WaitForSingleObject( ft->hWaitEvent, 60000 )) {
	case WAIT_TIMEOUT:
	case WAIT_FAILED:
		MSN_DebugLog( "Incoming connection timed out, closing file transfer" );
		return;
	}	

	info->mBytesInData = 0;

	while ( TRUE )
	{
		int handlerResult;

		int recvResult = MSN_WS_Recv( info->s, info->mData+info->mBytesInData, 1000 - info->mBytesInData );
		if ( recvResult == SOCKET_ERROR || !recvResult )
			break;

		info->mBytesInData += recvResult;

		//pull off each line for parsing
		if (( info->mCaller == 3 ) && ( info->mType == SERVER_FILETRANS )) 
		{
			handlerResult = MSN_HandleMSNFTP( info, info->mData );
			if ( handlerResult ) 
				break;
		} 
		else  // info->mType!=SERVER_FILETRANS
		{ 
			while ( TRUE )
			{
				char* peol = strchr(info->mData,'\r');
				if ( peol == NULL )
					break;

				if ( info->mBytesInData < peol - info->mData + 2 )
					break;  //wait for full line end

				char msg[ sizeof(info->mData) ];
				memcpy( msg, info->mData, peol - info->mData ); msg[ peol - info->mData ] = 0;
				if ( *++peol != '\n' )
					MSN_DebugLog( "Dodgy line ending to command: ignoring" );
				else
					peol++;

				info->mBytesInData -= peol - info->mData;
				memmove( info->mData, peol, info->mBytesInData );

				MSN_DebugLog( "RECV:%s", msg );

				if ( !isalnum(msg[0]) || !isalnum(msg[1]) || !isalnum(msg[2]) || (msg[3] && msg[3]!=' ')) 
				{
					MSN_DebugLog( "Invalid command name");
					continue;
				}

				if ( info->mType != SERVER_FILETRANS )
				{
					if ( isdigit( msg[0]) && isdigit(msg[1]) && isdigit(msg[2]))   //all error messages
						handlerResult = MSN_HandleErrors(info,msg);
					else
						handlerResult = MSN_HandleCommands( info, msg );
				} 
				else handlerResult = MSN_HandleMSNFTP( info, msg );

				if ( handlerResult )
					break;
		}	}

		if ( info->mBytesInData == sizeof( info->mData )) 
		{	MSN_DebugLog( "sizeof(data) is too small: the longest line won't fit" );
			break;
	}	}

	MSN_DebugLog( "Closing file transfer thread" );
}

/*=======================================================================================
 * Added by George B. Hazan (ghazan@postman.ru)
 * The following code is required to abortively stop all started threads upon exit
 *=======================================================================================*/

static ThreadData*		sttThreads[ MAX_THREAD_COUNT ];	// up to MAX_THREAD_COUNT threads
static CRITICAL_SECTION	sttLock;

void __stdcall MSN_InitThreads()
{
	memset( sttThreads, 0, sizeof( sttThreads ));
	InitializeCriticalSection( &sttLock );
}

void __stdcall MSN_CloseConnections()
{
	int i;

	EnterCriticalSection( &sttLock );

	for ( i=0; i < MAX_THREAD_COUNT; i++ ) {
		ThreadData* T = sttThreads[ i ];
		if ( T == NULL )
			continue;
		
		if ( T->mType == SERVER_SWITCHBOARD && T->s != NULL )
			MSN_SendPacket( T->s, "OUT", NULL );
	}

	LeaveCriticalSection( &sttLock );
}

void __stdcall MSN_CloseThreads()
{
	EnterCriticalSection( &sttLock );

	for ( int i=0; i < MAX_THREAD_COUNT; i++ ) {
		ThreadData* T = sttThreads[ i ];
		if ( T == NULL || T->s == NULL )
			continue;

		SOCKET s = MSN_CallService( MS_NETLIB_GETSOCKET, LPARAM( T->s ), 0 );
		if ( s != INVALID_SOCKET )
			shutdown( s, 2 );
	}

	LeaveCriticalSection( &sttLock );
}

void Threads_Uninit( void )
{
	DeleteCriticalSection( &sttLock );
}

ThreadData* __stdcall MSN_GetThreadByContact( HANDLE hContact )
{
	EnterCriticalSection( &sttLock );

	ThreadData* T = NULL;

	for ( int i=0; i < MAX_THREAD_COUNT; i++ )
	{
		T = sttThreads[ i ];
		if ( T == NULL )
			continue;

		if ( T->mJoinedCount == 0 || T->mJoinedContacts == NULL || T->s == NULL )
			continue;

		if ( T->mJoinedContacts[0] == hContact )
			break;
	}

	LeaveCriticalSection( &sttLock );
	return T;
}

ThreadData* __stdcall MSN_GetUnconnectedThread( HANDLE hContact )
{
	EnterCriticalSection( &sttLock );

	ThreadData* T = NULL;

	for ( int i=0; i < MAX_THREAD_COUNT; i++ )
	{
		T = sttThreads[ i ];
		if ( T == NULL )
			continue;

		if ( T->mInitialContact == hContact )
			break;
	}

	LeaveCriticalSection( &sttLock );
	return T;
}

int __stdcall MSN_GetActiveThreads( ThreadData** parResult )
{
	int tCount = 0;
	EnterCriticalSection( &sttLock );

	for ( int i=0; i < MAX_THREAD_COUNT; i++ )
	{
		ThreadData* T = sttThreads[ i ];
		if ( T == NULL )
			continue;

		if ( T->mType == SERVER_SWITCHBOARD && T->mJoinedCount != 0 && T->mJoinedContacts != NULL )
			parResult[ tCount++ ] = T;
	}
	
	LeaveCriticalSection( &sttLock );
	return tCount;
}

ThreadData* __stdcall MSN_GetThreadByConnection( HANDLE s )
{
	ThreadData* tResult = NULL;

	EnterCriticalSection( &sttLock );

	for ( int i=0; i < MAX_THREAD_COUNT; i++ )
	{
		ThreadData* T = sttThreads[ i ];
		if ( T == NULL )
			continue;

		if ( T->s == s )
		{	tResult = T;
			break;
	}	}
	
	LeaveCriticalSection( &sttLock );
	return tResult;
}

ThreadData* __stdcall MSN_GetThreadByPort( WORD wPort )
{
	ThreadData* tResult = NULL;

	EnterCriticalSection( &sttLock );

	for ( int i=0; i < MAX_THREAD_COUNT; i++ )
	{
		ThreadData* T = sttThreads[ i ];
		if ( T == NULL || T->ft == NULL )
			continue;

		if ( T->ft->mIncomingPort == wPort )
		{	tResult = T;
			break;
	}	}
	
	LeaveCriticalSection( &sttLock );
	return tResult;
}

/////////////////////////////////////////////////////////////////////////////////////////
// class ThreadData members

ThreadData::ThreadData() 
{
	memset( this, 0, sizeof ThreadData );
}

ThreadData::~ThreadData() 
{
	if ( s != NULL )
		Netlib_CloseHandle( s );

	if ( ft != NULL )
		delete ft;
}

void ThreadData::applyGatewayData( HANDLE hConn, bool isPoll )
{
	char szHttpPostUrl[300];
	getGatewayUrl( szHttpPostUrl, sizeof( szHttpPostUrl ), isPoll );

	MSN_DebugLog( "applying '%s' to %08X [%d]", szHttpPostUrl, this, GetCurrentThreadId() );

	NETLIBHTTPPROXYINFO nlhpi = {0};
	nlhpi.cbSize = sizeof(nlhpi);
	nlhpi.flags = 0;
	nlhpi.szHttpGetUrl = NULL;
	nlhpi.szHttpPostUrl = szHttpPostUrl;
	nlhpi.firstPostSequence = 1;
	CallService( MS_NETLIB_SETHTTPPROXYINFO, (WPARAM)hConn, (LPARAM)&nlhpi);
}

static char sttFormatString[] = "http://gateway.messenger.hotmail.com/gateway/gateway.dll?Action=open&Server=%s&IP=%s";

void ThreadData::getGatewayUrl( char* dest, int destlen, bool isPoll )
{
	if ( mSessionID[0] == 0 ) {
		if ( mType == SERVER_NOTIFICATION || mType == SERVER_DISPATCH )
			_snprintf( dest, destlen, sttFormatString, "NS", "messenger.hotmail.com" );
		else
			_snprintf( dest, destlen, sttFormatString, "SB", mServer );
		strcpy( mGatewayIP, MSN_DEFAULT_GATEWAY );
	}
	else _snprintf( dest, destlen, "http://%s/gateway/gateway.dll?%sSessionID=%s", 
		mGatewayIP, ( isPoll ) ? "Action=poll&" : "", mSessionID );
}

void ThreadData::processSessionData( const char* str )
{
	char tSessionID[40], tGateIP[ 40 ];

	char* tDelim = strchr( str, ';' );
	if ( tDelim == NULL )
		return;

	*tDelim = 0; tDelim += 2;

	if ( !sscanf( str, "SessionID=%s", tSessionID ))
		return;

	char* tDelim2 = strchr( tDelim, ';' );
	if ( tDelim2 != NULL )
		*tDelim2 = '\0';

	if ( !sscanf( tDelim, "GW-IP=%s", tGateIP ))
		return;

//	MSN_DebugLog( "msn_httpGatewayUnwrapRecv printed '%s','%s' to %08X (%08X)", tSessionID, tGateIP, s, this );
	strcpy( mGatewayIP, tGateIP );
	strcpy( mSessionID, tSessionID );
}

/////////////////////////////////////////////////////////////////////////////////////////
// thread start code

struct FORK_ARG {
	HANDLE hEvent;
	pThreadFunc threadcode;
	ThreadData* arg;
};

static void sttRegisterThread( ThreadData* s )
{
	EnterCriticalSection( &sttLock );

	for ( int i=0; i < MAX_THREAD_COUNT; i++ )
		if ( sttThreads[ i ] == NULL )
		{	sttThreads[ i ] = s;
			break;
		}

	LeaveCriticalSection( &sttLock );
}

static void sttUnregisterThread( ThreadData* s )
{
	EnterCriticalSection( &sttLock );

	for ( int i=0; i < MAX_THREAD_COUNT; i++ )
	{	if ( sttThreads[ i ] == s )
		{	sttThreads[ i ] = NULL;
			break;
	}	}

	LeaveCriticalSection( &sttLock );
}

static void __cdecl forkthread_r(struct FORK_ARG *fa)
{	
	pThreadFunc callercode = fa->threadcode;
	ThreadData* arg = fa->arg;
	MSN_CallService(MS_SYSTEM_THREAD_PUSH, 0, 0);
	sttRegisterThread( arg );
	SetEvent(fa->hEvent);
	__try {
		callercode(arg);
	} __finally {
		sttUnregisterThread( arg );
		delete arg;

		MSN_CallService(MS_SYSTEM_THREAD_POP, 0, 0);
	} 
	return;
}

void ThreadData::startThread( pThreadFunc parFunc )
{
	FORK_ARG fa = { CreateEvent(NULL, FALSE, FALSE, NULL), parFunc, this };
	unsigned long rc = _beginthread(( pThreadFunc )forkthread_r, 0, &fa );
	if ((unsigned long) -1L != rc)
		WaitForSingleObject(fa.hEvent, INFINITE);
	CloseHandle(fa.hEvent);
}

/////////////////////////////////////////////////////////////////////////////////////////
// HReadBuffer members

HReadBuffer::HReadBuffer( ThreadData* T, int iStart )
{
	owner = T;
	buffer = ( BYTE* )T->mData;
	totalDataSize = T->mBytesInData;
	startOffset = iStart;
}

HReadBuffer::~HReadBuffer()
{
	owner->mBytesInData = totalDataSize - startOffset;
	if ( owner->mBytesInData != 0 )
		memmove( owner->mData, owner->mData + startOffset, owner->mBytesInData );
}

BYTE* HReadBuffer::surelyRead( HANDLE s, int parBytes )
{
	if ( startOffset + parBytes > totalDataSize )
	{
		int tNewLen = totalDataSize - startOffset;
		if ( tNewLen > 0 )
			memmove( buffer, buffer + startOffset, tNewLen );
		else
			tNewLen = 0;

		startOffset = 0;
		totalDataSize = tNewLen;
	}

	int bufferSize = sizeof owner->mData;
	if ( parBytes > bufferSize - startOffset ) {
		MSN_DebugLog( "HReadBuffer::surelyRead: not enough memory, %d %d %d", parBytes, bufferSize, startOffset );
		return NULL;
	}

	while( totalDataSize - startOffset < parBytes )
	{
		int recvResult = MSN_WS_Recv( s, ( char* )buffer + totalDataSize, bufferSize - totalDataSize );
		if ( recvResult <= 0 )
			return NULL;

		totalDataSize += recvResult;
	}

	BYTE* result = buffer + startOffset; startOffset += parBytes;
	return result;
}
