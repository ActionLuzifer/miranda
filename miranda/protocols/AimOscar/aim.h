#ifndef AIM_H
#define AIM_H

#define MIRANDA_VER 0x800

#if defined __GNUC__
#pragma GCC system_header
#endif 

#define  _CRT_SECURE_NO_DEPRECATE
#pragma warning (disable : 4996)

#include <m_stdhdr.h>

//System includes
#include <windows.h>
#include <ctype.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <prsht.h>
#include <richedit.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <winuser.h>

//Miranda IM includes
#pragma warning( disable: 4100 )
#pragma warning( disable: 4244 )
#pragma warning( disable: 4201 )
#include <newpluginapi.h>
#include <statusmodes.h>
#include <m_button.h>
#include <m_clist.h>
#include <m_clui.h>
#include "m_cluiframes.h"
#include <m_database.h>
#include <m_idle.h>
#include <m_langpack.h>
#include <m_message.h>
#include <m_netlib.h>
#include <m_options.h>
#include <m_popup.h>
#include <m_protocols.h>
#include <m_protomod.h>
#include <m_protosvc.h>
#include <m_skin.h>
#include <statusmodes.h>
#include <m_system.h>
#include <m_userinfo.h>
#include <m_addcontact.h>
#include <m_icolib.h>
#include <m_utils.h>
#include <m_system_cpp.h>
#include <win2k.h>

#pragma warning( default: 4100 )
#pragma warning( default: 4244 )
#pragma warning( default: 4201 )
//independent includes
#include "strl.h"
#include "flap.h"
#include "snac.h"
#include "tlv.h"
//rest o includes
#include "avatars.h"
#include "utility.h"
#include "client.h"
#include "direct_connect.h"
#include "conv.h"
#include "error.h"
#include "file.h"
#include "links.h"
#include "packets.h"
#include "popup.h"
#include "proxy.h"
#include "resource.h"
#include "services.h"
#include "server.h"
#include "theme.h"
#include "windows.h"
#include "proto.h"

//Packet Stuff
#define MSG_LEN							4089

//Extended Status Icon Numbers
#define ACCOUNT_TYPE_UNCONFIRMED		1
#define ACCOUNT_TYPE_CONFIRMED			2
#define ACCOUNT_TYPE_ICQ				3
#define ACCOUNT_TYPE_AOL				4
#define ACCOUNT_TYPE_ADMIN				5
#define EXTENDED_STATUS_BOT				1
#define EXTENDED_STATUS_HIPTOP			2

//Popup flags
#define	MAIL_POPUP						4

//Main Option Window Keys
#define AIM_KEY_SN						"SN"
#define AIM_KEY_NK						"Nick"
#define AIM_KEY_PW						"Password"
#define AIM_KEY_HN						"loginhost"
#define AIM_KEY_PN						"loginport"
#define AIM_KEY_DC						"DelConf"//delivery confirmation
#define AIM_KEY_FP						"ForceProxyTransfer"
#define AIM_KEY_GP						"FileTransferGracePeriod"//in seconds default 60
#define AIM_KEY_KA						"KeepAlive"//in seconds default 60
#define AIM_KEY_HF						"HiptopFake"
#define AIM_KEY_AT						"DisableATIcons"
#define AIM_KEY_ES						"DisableESIcons"
#define AIM_KEY_DM						"DisableModeMsg"
#define AIM_KEY_FI						"FormatIncoming"//html->bbcodes
#define AIM_KEY_FO						"FormatOutgoing"//bbcodes->html
#define AIM_KEY_FR						"FirstRun"
#define AIM_KEY_II						"InstantIdle"
#define AIM_KEY_IIT						"InstantIdleTS"
#define AIM_KEY_CM						"CheckMail"
#define AIM_KEY_DA						"DisableAvatars"
#define AIM_KEY_DSSL					"DisableSSL"

//Other plugin Option Keys
#define OTH_KEY_AI						"AwayIgnore"
#define OTH_KEY_AD						"AwayDefault"
#define OTH_KEY_AM						"AwayMsg"
#define OTH_KEY_OI						"OccupiedIgnore"
#define OTH_KEY_OD						"OccupiedDefault"
#define OTH_KEY_OM						"OccupiedMsg"
#define OTH_KEY_NI						"NaIgnore"
#define OTH_KEY_ND						"NaDefault"
#define OTH_KEY_NM						"NaMsg"
#define OTH_KEY_DI						"DndIgnore"
#define OTH_KEY_DD						"DndDefault"
#define OTH_KEY_DM						"DndMsg"
#define OTH_KEY_PI						"OtpIgnore"
#define OTH_KEY_PD						"OtpDefault"
#define OTH_KEY_PM						"OtpMsg"
#define OTH_KEY_LI						"OtlIgnore"
#define OTH_KEY_LD						"OtlDefault"
#define OTH_KEY_LM						"OtlMsg"

#define OTH_KEY_SM						"StatusMsg"
#define OTH_KEY_GP						"Group"
//Module Name Key
#define MOD_KEY_SA						"SRAway"
#define MOD_KEY_CL						"CList"
//Settings Keys
#define AIM_KEY_PR						"Profile"
#define AIM_KEY_LA						"LastAwayChange"
#define AIM_MOD_IG						"ID2Group"
#define AIM_MOD_GI						"Group2ID"
//Contact Keys
#define AIM_KEY_BI						"BuddyId"
#define AIM_KEY_GI						"GroupId"
#define AIM_KEY_ST						"Status"
#define AIM_KEY_IT						"IdleTS"
#define AIM_KEY_OT						"LogonTS"
#define AIM_KEY_AC						"AccType"//account type		
#define AIM_KEY_ET						"ESType"//Extended Status type
#define AIM_KEY_MV						"MirVer"
#define AIM_KEY_US						"Utf8Support"
#define AIM_KEY_NL						"NotOnList"
#define AIM_KEY_LM						"LastMessage"
#define AIM_KEY_NC						"NewContact"
#define AIM_KEY_AH						"AvatarHash"
//File Transfer Keys
#define AIM_KEY_FT						"FileTransfer"//1= sending 0=receiving
#define AIM_KEY_CK						"Cookie"
#define AIM_KEY_CK2						"Cookie2"
#define AIM_KEY_FN						"FileName"
#define AIM_KEY_FS						"FileSize"
#define AIM_KEY_FD						"FileDesc"
#define AIM_KEY_IP						"IP"
#define AIM_KEY_PS						"ProxyStage"
#define AIM_KEY_PC						"PortCheck"
#define AIM_KEY_DH						"DCHandle"
//Old Keys
#define OLD_KEY_PW						"password"
#define OLD_KEY_DM						"AutoResponse"

#define AIM_DEFAULT_SERVER				"slogin.oscar.aol.com"
#define AIM_DEFAULT_SERVER_NS			"login.oscar.aol.com"
#define AIM_PROXY_SERVER	            "ars.oscar.aol.com"
#define AIM_DEFAULT_PORT				5190

//Some Defaults for various things
#define DEFAULT_KEEPALIVE_TIMER			60// 1000 milliseconds * 60 = 60 secs
#define DEFAULT_GRACE_PERIOD			60
#define AIM_DEFAULT_GROUP				"miranda merged"
#define SYSTEM_BUDDY					"aolsystemmsg"
#define DEFAULT_AWAY_MSG				"I am away from my computer right now."
//Md5 Roasting stuff
#define AIM_MD5_STRING					"AOL Instant Messenger (SM)"
#define MD5_HASH_LENGTH					16
//Aim Version Stuff
#define AIM_CLIENT_ID_NUMBER			"\x01\x09"
#define AIM_CLIENT_MAJOR_VERSION		"\0\x05"
#define AIM_CLIENT_MINOR_VERSION		"\0\x09"
#define AIM_CLIENT_LESSER_VERSION		"\0\0"
#define AIM_CLIENT_BUILD_NUMBER			"\x0b\xdc"
#define AIM_CLIENT_DISTRIBUTION_NUMBER	"\0\0\0\xd2"
#define AIM_LANGUAGE					"en"
#define AIM_COUNTRY						"us"
#define AIM_MSG_TYPE					"text/x-aolrtf; charset=\"us-ascii\""
#define AIM_TOOL_VERSION				"\x01\x10\x08\xf1"
extern char AIM_CLIENT_ID_STRING[];		//Client id EXTERN
//Supported Clients
#define CLIENT_UNKNOWN					"?"
#define CLIENT_AIM5						"AIM 5.x"
#define CLIENT_AIM4						"AIM 4.x"
#define CLIENT_AIMEXPRESS5				"AIM Express 5"
#define CLIENT_AIMEXPRESS6				"AIM Express 6"
#define CLIENT_AIMEXPRESS7				"AIM Express 7"
#define CLIENT_AIM_TRITON				"AIM Triton"
#define CLIENT_AIM6_1					"AIM 6.1"
#define CLIENT_AIM6_5					"AIM 6.5"
#define CLIENT_AIM6_8					"AIM 6.8"
#define CLIENT_AIMTOC					"AIM TOC"
#define CLIENT_BOT						"AIM Bot"
#define CLIENT_GAIM						"Gaim"
#define CLIENT_PURPLE					"Purple"
#define CLIENT_ADIUM					"Adium X"
#define CLIENT_GPRS						"GPRS"
#define CLIENT_ICHAT					"iChat"
#define CLIENT_IM2						"IM2"
#define CLIENT_KOPETE					"Kopete"
#define CLIENT_MEEBO					"Meebo"
#define CLIENT_MICQ						"mICQ"
#define CLIENT_AIMOSCAR					"Miranda IM %d.%d.%d.%d(AimOSCAR v%d.%d.%d.%d)"
#define CLIENT_OSCARJ					"Miranda IM %d.%d.%d.%d(ICQ v0.%d.%d.%d)"
#define CLIENT_NAIM						"naim"
#define CLIENT_QIP						"qip"
#define CLIENT_SIM						"SIM"
#define CLIENT_SMS						"SMS"
#define CLIENT_TERRAIM					"TerraIM"
#define CLIENT_TRILLIAN_PRO				"Trillian Pro"
#define CLIENT_TRILLIAN					"Trillian"
//Aim Caps
#define AIM_CAPS_LENGTH					16

// Official
#define AIM_CAP_SHORT_CAPS				"\x09\x46\x00\x00\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_SECURE_IM				"\x09\x46\x00\x01\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_XHTML_IM				"\x09\x46\x00\x02\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_RTCVIDEO    			"\x09\x46\x01\x01\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_HAS_MICROPHONE			"\x09\x46\x01\x02\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_HAS_CAMERA  			"\x09\x46\x01\x03\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_RTCAUDIO    			"\x09\x46\x01\x04\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_HOST_STATUS_TEXT_AWARE  "\x09\x46\x01\x0a\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_RTIM                    "\x09\x46\x01\x0b\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_SMART_CAPS				"\x09\x46\x01\xff\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_FILE_TRANSFER			"\x09\x46\x13\x43\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_DIRECT_ICBM				"\x09\x46\x13\x45\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_FILE_SHARING			"\x09\x46\x13\x48\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_SUPPORT_ICQ				"\x09\x46\x13\x4D\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"

// Extensions
#define AIM_CAP_UNKNOWNA				"\x09\x46\x01\x05\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_UNKNOWNB				"\x09\x46\x01\x07\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_UNKNOWND				"\x09\x46\x01\x0c\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_HIPTOP					"\x09\x46\x13\x23\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_VOICE_CHAT				"\x09\x46\x13\x41\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_DIRECT_PLAY				"\x09\x46\x13\x42\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_ROUTER_FIND				"\x09\x46\x13\x44\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"//icq?
#define AIM_CAP_AVATARS					"\x09\x46\x13\x46\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_ADDINS					"\x09\x46\x13\x47\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_CHANNEL_TWO				"\x09\x46\x13\x49\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"//icq? channel 2 extended, TLV(0x2711) based messages
#define AIM_CAP_GAMES					"\x09\x46\x13\x4A\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_LIST_TRANSFER			"\x09\x46\x13\x4B\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_UTF8					"\x09\x46\x13\x4E\x4C\x7F\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_UNKNOWN4				"\x09\x46\xf0\x03\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_UNKNOWN1				"\x09\x46\xf0\x04\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_UNKNOWNC				"\x09\x46\xf0\x05\x4c\x7f\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_CHAT					"\x74\x8F\x24\x20\x62\x87\x11\xD1\x82\x22\x44\x45\x53\x54\0\0"
#define AIM_CAP_IM2						"\x74\xed\xc3\x36\x44\xdf\x48\x5b\x8b\x1c\x67\x1a\x1f\x86\x09\x9f"
#define AIM_CAP_TRILLIAN				"\xF2\xE7\xC7\xF4\xFE\xAD\x4D\xFB\xB2\x35\x36\x79\x8B\xDF\0\0"
extern char	AIM_CAP_MIRANDA[];			//Miranda cap EXTERN
//Aim Services
#define AIM_SERVICE_GENERIC				"\0\x01\0\x04"//version 4
#define AIM_SERVICE_SSI					"\0\x13\0\x03"//version 3
#define AIM_SERVICE_LOCATION			"\0\x02\0\x01"//version 1
#define AIM_SERVICE_BUDDYLIST			"\0\x03\0\x01"//version 1
#define AIM_SERVICE_MESSAGING			"\0\x04\0\x01"//version 1
#define AIM_SERVICE_INVITATION			"\0\x06\0\x01"//version 1
#define AIM_SERVICE_POPUP				"\0\x08\0\x01"//version 1
#define AIM_SERVICE_BOS					"\0\x09\0\x01"//version 1
#define AIM_SERVICE_AVATAR				"\0\x10\0\x01"//version 1
#define AIM_SERVICE_USERLOOKUP			"\0\x0A\0\x01"//version 1
#define AIM_SERVICE_STATS				"\0\x0B\0\x01"//version 1
#define AIM_SERVICE_MAIL				"\0\x18\0\x01"//version 1
#define AIM_SERVICE_RATES				"\0\x01\0\x02\0\x03\0\x04\0\x05"
//Aim Statuses
#define AIM_STATUS_WEBAWARE				"\0\x01"	
#define AIM_STATUS_SHOWIP				"\0\x02"
#define AIM_STATUS_BIRTHDAY				"\0\x08"
#define AIM_STATUS_WEBFRONT				"\0\x20"
#define AIM_STATUS_DCAUTH				"\x10\0"
#define AIM_STATUS_DCCONT				"\x20\0"
#define AIM_STATUS_NULL					"\0\0"
#define	AIM_STATUS_ONLINE				"\0\0"
#define	AIM_STATUS_AWAY					"\0\x01"
#define	AIM_STATUS_DND					"\0\x02"
#define	AIM_STATUS_NA					"\0\x04"
#define	AIM_STATUS_OCCUPIED				"\0\x10"
#define	AIM_STATUS_FREE4CHAT			"\0\x20"
#define AIM_STATUS_INVISIBLE			"\x01\0"

#define HOOKEVENT_SIZE 10
#define SERVICES_SIZE  30

extern HINSTANCE hInstance; //plugin dll instance

#define NEWSTR_ALLOCA(A) (A==NULL)?NULL:strcpy((char*)alloca(strlen(A)+1),A)
#define NEWTSTR_ALLOCA(A) (A==NULL)?NULL:_tcscpy((TCHAR*)alloca(sizeof(TCHAR)*(_tcslen(A)+1)),A)

#endif
