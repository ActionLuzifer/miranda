// ---------------------------------------------------------------------------80
//                ICQ plugin for Miranda Instant Messenger
//                ________________________________________
// 
// Copyright � 2000,2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
// Copyright � 2001,2002 Jon Keating, Richard Hughes
// Copyright � 2002,2003,2004 Martin �berg, Sam Kothari, Robert Rainwater
// Copyright � 2004,2005 Joe Kucera
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// -----------------------------------------------------------------------------
//
// File name      : $Source$
// Revision       : $Revision$
// Last change on : $Date$
// Last change by : $Author$
//
// DESCRIPTION:
//
//  Provides capability & signature based client detection
//
// -----------------------------------------------------------------------------

#include "icqoscar.h"


capstr* MatchCap(char* buf, int bufsize, const capstr* cap, int capsize)
{
  while (bufsize>0) // search the buffer for a capability
  {
    if (!memcmp(buf, cap, capsize))
    {
      return (capstr*)buf; // give found capability for version info
    }
    else
    {
      buf += 0x10;
      bufsize -= 0x10;
    }
  }
  return 0;
}



static void makeClientVersion(char *szBuf, const char* szClient, unsigned v1, unsigned v2, unsigned v3, unsigned v4)
{
  if (v4) 
    null_snprintf(szBuf, 64, "%s%u.%u.%u.%u", szClient, v1, v2, v3, v4);
  else if (v3) 
    null_snprintf(szBuf, 64, "%s%u.%u.%u", szClient, v1, v2, v3);
  else
    null_snprintf(szBuf, 64, "%s%u.%u", szClient, v1, v2);
}



static void verToStr(char* szStr, int v)
{
  char szVer[64];

  makeClientVersion(szVer, "", (v>>24)&0x7F, (v>>16)&0xFF, (v>>8)&0xFF, v&0xFF); 
  strcat(szStr, szVer);
  if (v&0x80000000) strcat(szStr, " alpha");
}


// Dont free the returned string.
// Function is not multithread safe.
static char* MirandaVersionToString(char* szStr, int v, int m)
{
  if (!v) // this is not Miranda
    return NULL;
  else
  {
    strcpy(szStr, "Miranda IM ");

    if (v == 1)
      verToStr(szStr, 0x80010200);
    else if ((v&0x7FFFFFFF) <= 0x030301)
      verToStr(szStr, v);
    else 
    {
      if (m)
      {
        verToStr(szStr, m);
        strcat(szStr, " ");
      }
      strcat(szStr, "(ICQ v");
      verToStr(szStr, v);
      strcat(szStr, ")");
    }
  }

  return szStr;
}


const capstr capMirandaIm = {'M', 'i', 'r', 'a', 'n', 'd', 'a', 'M', 0, 0, 0, 0, 0, 0, 0, 0};
const capstr capTrillian  = {0x97, 0xb1, 0x27, 0x51, 0x24, 0x3c, 0x43, 0x34, 0xad, 0x22, 0xd6, 0xab, 0xf7, 0x3f, 0x14, 0x09};
const capstr capTrilCrypt = {0xf2, 0xe7, 0xc7, 0xf4, 0xfe, 0xad, 0x4d, 0xfb, 0xb2, 0x35, 0x36, 0x79, 0x8b, 0xdf, 0x00, 0x00};
const capstr capSim       = {'S', 'I', 'M', ' ', 'c', 'l', 'i', 'e', 'n', 't', ' ', ' ', 0, 0, 0, 0};
const capstr capSimOld    = {0x97, 0xb1, 0x27, 0x51, 0x24, 0x3c, 0x43, 0x34, 0xad, 0x22, 0xd6, 0xab, 0xf7, 0x3f, 0x14, 0x00};
const capstr capLicq      = {'L', 'i', 'c', 'q', ' ', 'c', 'l', 'i', 'e', 'n', 't', ' ', 0, 0, 0, 0};
const capstr capKopete    = {'K', 'o', 'p', 'e', 't', 'e', ' ', 'I', 'C', 'Q', ' ', ' ', 0, 0, 0, 0};
const capstr capmIcq      = {'m', 'I', 'C', 'Q', ' ', 0xA9, ' ', 'R', '.', 'K', '.', ' ', 0, 0, 0, 0};
const capstr capAndRQ     = {'&', 'R', 'Q', 'i', 'n', 's', 'i', 'd', 'e', 0, 0, 0, 0, 0, 0, 0};
const capstr capRAndQ     = {'R', '&', 'Q', 'i', 'n', 's', 'i', 'd', 'e', 0, 0, 0, 0, 0, 0, 0};
const capstr capQip       = {0x56, 0x3F, 0xC8, 0x09, 0x0B, 0x6F, 0x41, 'Q', 'I', 'P', ' ', '2', '0', '0', '5', 'a'};
const capstr capIm2       = {0x74, 0xED, 0xC3, 0x36, 0x44, 0xDF, 0x48, 0x5B, 0x8B, 0x1C, 0x67, 0x1A, 0x1F, 0x86, 0x09, 0x9F}; // IM2 Ext Msg
const capstr capMacIcq    = {0xdd, 0x16, 0xf2, 0x02, 0x84, 0xe6, 0x11, 0xd4, 0x90, 0xdb, 0x00, 0x10, 0x4b, 0x9b, 0x4b, 0x7d};
const capstr capRichText  = {0x97, 0xb1, 0x27, 0x51, 0x24, 0x3c, 0x43, 0x34, 0xad, 0x22, 0xd6, 0xab, 0xf7, 0x3f, 0x14, 0x92};
const capstr capIs2001    = {0x2e, 0x7a, 0x64, 0x75, 0xfa, 0xdf, 0x4d, 0xc8, 0x88, 0x6f, 0xea, 0x35, 0x95, 0xfd, 0xb6, 0xdf};
const capstr capIs2002    = {0x10, 0xcf, 0x40, 0xd1, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00};
const capstr capStr20012  = {0xa0, 0xe9, 0x3f, 0x37, 0x4f, 0xe9, 0xd3, 0x11, 0xbc, 0xd2, 0x00, 0x04, 0xac, 0x96, 0xdd, 0x96};
const capstr capAimIcon   = {0x09, 0x46, 0x13, 0x46, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}; // CAP_AIM_BUDDYICON
const capstr capAimChat   = {0x74, 0x8F, 0x24, 0x20, 0x62, 0x87, 0x11, 0xD1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00};
const capstr capUim       = {0xA7, 0xE4, 0x0A, 0x96, 0xB3, 0xA0, 0x47, 0x9A, 0xB8, 0x45, 0xC9, 0xE4, 0x67, 0xC5, 0x6B, 0x1F};
const capstr capRambler   = {0x7E, 0x11, 0xB7, 0x78, 0xA3, 0x53, 0x49, 0x26, 0xA8, 0x02, 0x44, 0x73, 0x52, 0x08, 0xC4, 0x2A};
const capstr capAbv       = {0x00, 0xE7, 0xE0, 0xDF, 0xA9, 0xD0, 0x4F, 0xe1, 0x91, 0x62, 0xC8, 0x90, 0x9A, 0x13, 0x2A, 0x1B};
const capstr capNetvigator= {0x4C, 0x6B, 0x90, 0xA3, 0x3D, 0x2D, 0x48, 0x0E, 0x89, 0xD6, 0x2E, 0x4B, 0x2C, 0x10, 0xD9, 0x9F};


static BOOL hasRichText, hasRichChecked;

static BOOL hasCapRichText(BYTE* caps, WORD wLen)
{
  if (!hasRichChecked) 
  {
    hasRichText = MatchCap(caps, wLen, &capRichText, 0x10)?TRUE:FALSE;
    hasRichChecked = TRUE;
  }
  return hasRichText;
}


char* cliLibicq2k  = "libicq2000";
char* cliLicqVer   = "Licq ";
char* cliCentericq = "Centericq";
char* cliLibicqUTF = "libicq2000 (Unicode)";
char* cliTrillian  = "Trillian";
char* cliQip       = "QIP 200%c%c";
char* cliIM2       = "IM2";
char* cliSpamBot   = "Spam Bot";


char* detectUserClient(HANDLE hContact, DWORD dwUin, WORD wVersion, DWORD dwFT1, DWORD dwFT2, DWORD dwFT3, DWORD dwOnlineSince, DWORD dwDirectCookie, DWORD dwWebPort, BYTE* caps, WORD wLen, DWORD* dwClientId)
{
  LPSTR szClient = NULL;
  static char szClientBuf[64];

  *dwClientId = 1; // Most clients does not tick as MsgIDs
  hasRichChecked = FALSE; // init fast rich text detection

  // Is this a Miranda IM client?
  if (dwFT1 == 0xffffffff)
  {
    if (dwFT2 == 0xffffffff)
    { // This is Gaim not Miranda
      szClient = "Gaim";
    }
    else if (!dwFT2 && wVersion == 7)
    { // This is WebICQ not Miranda
      szClient = "WebICQ";
    }
    else if (!dwFT2 && dwFT3 == 0x3B7248ED)
    { // And this is most probably Spam Bot
      szClient = cliSpamBot;
    }
    else 
    { // Yes this is most probably Miranda, get the version info
      szClient = MirandaVersionToString(szClientBuf, dwFT2, 0);
    }
  }
  else if ((dwFT1 & 0xFF7F0000) == 0x7D000000)
  { // This is probably an Licq client
    DWORD ver = dwFT1 & 0xFFFF;

    makeClientVersion(szClientBuf, cliLicqVer, ver / 1000, (ver / 10) % 100, ver % 10, 0);
    if (dwFT1 & 0x00800000)
      strcat(szClientBuf, "/SSL");

    szClient = szClientBuf;
  }
  else if (dwFT1 == 0xffffff8f)
  {
    szClient = "StrICQ";
  }
  else if (dwFT1 == 0xffffff42)
  {
    szClient = "mICQ";
  }
  else if (dwFT1 == 0xffffffbe)
  {
    unsigned ver1 = (dwFT2>>24)&0xFF;
    unsigned ver2 = (dwFT2>>16)&0xFF;
    unsigned ver3 = (dwFT2>>8)&0xFF;
  
    makeClientVersion(szClientBuf, "Alicq ", ver1, ver2, ver3, 0);

    szClient = szClientBuf;
  }
  else if (dwFT1 == 0xFFFFFF7F)
  {
    szClient = "&RQ";
  }
  else if (dwFT1 == 0xFFFFFFAB)
  {
    szClient = "YSM";
  }
  else if (dwFT1 == 0x04031980)
  {
    szClient = "vICQ";
  }
  else if ((dwFT1 == 0x3AA773EE) && (dwFT2 == 0x3AA66380))
  {
    szClient = cliLibicq2k;
  }
  else if (dwFT1 == 0x3B75AC09)
  {
    szClient = cliTrillian;
  }
  else if (dwFT1 == 0xFFFFFFFE && dwFT3 == 0xFFFFFFFE)
  {
    szClient = "Jimm";
  }
  else if (dwFT1 == 0x3FF19BEB && dwFT3 == 0x3FF19BEB)
  {
    szClient = cliIM2;
  }
  else if (dwFT1 == 0xDDDDEEFF && !dwFT2 && !dwFT3)
  {
    szClient = "SmartICQ";
  }
  else if ((dwFT1 & 0xFFFFFFF0) == 0x494D2B00 && !dwFT2 && !dwFT3)
  { // last byte of FT1: (5 = Win32, 3 = SmartPhone, Pocket PC)
    szClient = "IM+";
  }
  else if (dwFT1 == 0x3B4C4C0C && !dwFT2 && dwFT3 == 0x3B7248ed)
  {
    szClient = "KXicq2";
  }
  else if (dwFT1 == dwFT2 && dwFT2 == dwFT3 && wVersion == 8)
  {
    if ((dwFT1 < dwOnlineSince + 3600) && (dwFT1 > (dwOnlineSince - 3600)))
    {
      szClient = cliSpamBot;
    }
  }

  { // capabilities based detection
    capstr* capId;

    if (dwUin && caps)
    {
      // check capabilities for client identification
      if (capId = MatchCap(caps, wLen, &capMirandaIm, 8))
      { // new Miranda Signature
        DWORD iver = (*capId)[0xC] << 0x18 | (*capId)[0xD] << 0x10 | (*capId)[0xE] << 8 | (*capId)[0xF];
        DWORD mver = (*capId)[0x8] << 0x18 | (*capId)[0x9] << 0x10 | (*capId)[0xA] << 8 | (*capId)[0xB];

        szClient = MirandaVersionToString(szClientBuf, iver, mver);
      }
      else if (MatchCap(caps, wLen, &capTrillian, 0x10) || MatchCap(caps, wLen, &capTrilCrypt, 0x10))
      { // this is Trillian, check for new version
        if (hasCapRichText(caps, wLen))
          szClient = "Trillian v3";
        else
          szClient = cliTrillian;
      }
      else if ((capId = MatchCap(caps, wLen, &capSimOld, 0xF)) && ((*capId)[0xF] != 0x92 && (*capId)[0xF] >= 0x20 || (*capId)[0xF] == 0))
      {
        int hiVer = (((*capId)[0xF]) >> 6) - 1;
        unsigned loVer = (*capId)[0xF] & 0x1F;

        if ((hiVer < 0) || ((hiVer == 0) && (loVer == 0))) 
          szClient = "Kopete";
        else
        {
          null_snprintf(szClientBuf, sizeof(szClientBuf), "SIM %u.%u", (unsigned)hiVer, loVer);
          szClient = szClientBuf;
        }
      }
      else if (capId = MatchCap(caps, wLen, &capSim, 0xC))
      {
        unsigned ver1 = (*capId)[0xC];
        unsigned ver2 = (*capId)[0xD];
        unsigned ver3 = (*capId)[0xE];

        makeClientVersion(szClientBuf, "SIM ", ver1, ver2, ver3, 0);
        if ((*capId)[0xF] & 0x80) 
          strcat(szClientBuf,"/Win32");
        else if ((*capId)[0xF] & 0x40) 
          strcat(szClientBuf,"/MacOS X");

        szClient = szClientBuf;
      }
      else if (capId = MatchCap(caps, wLen, &capLicq, 0xC))
      {
        unsigned ver1 = (*capId)[0xC];
        unsigned ver2 = (*capId)[0xD] % 100;
        unsigned ver3 = (*capId)[0xE];

        makeClientVersion(szClientBuf, cliLicqVer, ver1, ver2, ver3, 0);
        if ((*capId)[0xF]) 
          strcat(szClientBuf,"/SSL");

        szClient = szClientBuf;
      }
      else if (capId = MatchCap(caps, wLen, &capKopete, 0xC))
      {
        unsigned ver1 = (*capId)[0xC];
        unsigned ver2 = (*capId)[0xD];
        unsigned ver3 = (*capId)[0xE];
        unsigned ver4 = (*capId)[0xF];

        makeClientVersion(szClientBuf, "Kopete ", ver1, ver2, ver3, ver4);

        szClient = szClientBuf;
      }
      else if (capId = MatchCap(caps, wLen, &capmIcq, 0xC))
      {
        unsigned ver1 = (*capId)[0xC];
        unsigned ver2 = (*capId)[0xD];
        unsigned ver3 = (*capId)[0xE];
        unsigned ver4 = (*capId)[0xF];

        makeClientVersion(szClientBuf, "mICQ ", ver1, ver2, ver3, ver4);

        szClient = szClientBuf;
      }
      else if (MatchCap(caps, wLen, &capIm2, 0x10))
      { // IM2 v2 provides also Aim Icon cap
        szClient = cliIM2;
      }
      else if (capId = MatchCap(caps, wLen, &capAndRQ, 9))
      {
        unsigned ver1 = (*capId)[0xC];
        unsigned ver2 = (*capId)[0xB];
        unsigned ver3 = (*capId)[0xA];
        unsigned ver4 = (*capId)[9];

        makeClientVersion(szClientBuf, "&RQ ", ver1, ver2, ver3, ver4);

        szClient = szClientBuf;
      }
      else if (capId = MatchCap(caps, wLen, &capRAndQ, 9))
      {
        unsigned ver1 = (*capId)[0xC];
        unsigned ver2 = (*capId)[0xB];
        unsigned ver3 = (*capId)[0xA];
        unsigned ver4 = (*capId)[9];

        makeClientVersion(szClientBuf, "R&Q ", ver1, ver2, ver3, ver4);

        szClient = szClientBuf;
      }
      else if (capId = MatchCap(caps, wLen, &capQip, 0xE))
      {
        char v1 = (*capId)[0xE];
        char v2 = (*capId)[0xF];

        null_snprintf(szClientBuf, sizeof(szClientBuf), cliQip, v1, v2);
        szClient = szClientBuf;
      }
      else if (MatchCap(caps, wLen, &capMacIcq, 0x10))
      {
        szClient = "ICQ for Mac";
      }
      else if (MatchCap(caps, wLen, &capUim, 0x10))
      {
        szClient = "uIM";
      }
      else if (MatchCap(caps, wLen, &capAimIcon, 0x10))
      { // this is what I really hate, but as it seems to me, there is no other way to detect libgaim
        szClient = "libgaim";
      }
      else if (szClient == cliLibicq2k)
      { // try to determine which client is behind libicq2000
        if (hasCapRichText(caps, wLen))
          szClient = cliCentericq; // centericq added rtf capability to libicq2000
        else if (CheckContactCapabilities(hContact, CAPF_UTF))
          szClient = cliLibicqUTF; // IcyJuice added unicode capability to libicq2000
        // others - like jabber transport uses unmodified library, thus cannot be detected
      }
      else if (szClient == NULL) // HERE ENDS THE SIGNATURE DETECTION, after this only feature default will be detected
      {
        if (wVersion == 8 && (MatchCap(caps, wLen, &capStr20012, 0x10) || CheckContactCapabilities(hContact, CAPF_SRV_RELAY)))
        { // try to determine 2001-2003 versions
          if (MatchCap(caps, wLen, &capIs2001, 0x10))
          {
            if (!dwFT1 && !dwFT2 && !dwFT3)
              if (hasCapRichText(caps, wLen))
                szClient = "TICQClient"; // possibly also older GnomeICU
              else
                szClient = "ICQ for Pocket PC";
            else
            {
              *dwClientId = 0;
              szClient = "ICQ 2001";
            }
          }
          else if (MatchCap(caps, wLen, &capIs2002, 0x10))
          {
            *dwClientId = 0;
            szClient = "ICQ 2002";
          }
          else if (CheckContactCapabilities(hContact, CAPF_SRV_RELAY || CAPF_UTF) && hasCapRichText(caps, wLen))
          {
            if (!dwFT1 && !dwFT2 && !dwFT3)
            {
              if (!dwWebPort)
                szClient = "GnomeICU 0.99.5+"; // no other way
              else
                szClient = "IC@";
              }
            else
            {
              *dwClientId = 0;
              szClient = "ICQ 2002/2003a";
            }
          }
        }
        else if (wVersion == 9)
        { // try to determine lite versions
          if (CheckContactCapabilities(hContact, CAPF_XTRAZ))
          {
            *dwClientId = 0;
            if (CheckContactCapabilities(hContact, CAPF_AIM_FILE))
            {
              strcpy(szClientBuf, "icq5");
              if (MatchCap(caps, wLen, &capRambler, 0x10))
              {
                strcat(szClientBuf, " (Rambler)");
              }
              else if (MatchCap(caps, wLen, &capAbv, 0x10))
              {
                strcat(szClientBuf, " (Abv)");
              }
              else if (MatchCap(caps, wLen, &capNetvigator, 0x10))
              {
                strcat(szClientBuf, " (Netvigator)");
              }
              szClient = szClientBuf;
            }
            else
              szClient = "ICQ Lite v4";
          }
        }
        else if (wVersion == 7)
        {
          if (hasCapRichText(caps, wLen))
            szClient = "GnomeICU"; // this is an exception
          else if (CheckContactCapabilities(hContact, CAPF_SRV_RELAY))
          {
            if (!dwFT1 && !dwFT2 && !dwFT3)
              szClient = "&RQ";
            else
            {
              *dwClientId = 0;
              szClient = "ICQ 2000";
            }
          }
          else if (CheckContactCapabilities(hContact, CAPF_TYPING))
            szClient = "Icq2Go! (Java)";
          else 
            szClient = "Icq2Go!";
        }
        else if (wVersion == 0xA)
        {
          if (!hasCapRichText(caps, wLen) && !CheckContactCapabilities(hContact, CAPF_UTF))
          { // this is bad, but we must do it - try to detect QNext
            ClearContactCapabilities(hContact, CAPF_SRV_RELAY);
            NetLog_Server("Forcing simple messages (QNext client).");
            szClient = "QNext";
          }
        }
        else if (wVersion == 0)
        {
          if (CheckContactCapabilities(hContact, CAPF_TYPING) && MatchCap(caps, wLen, &capIs2001, 0x10) &&
            MatchCap(caps, wLen, &capIs2002, 0x10) && MatchCap(caps, wLen, &capStr20012, 0x10) && wLen==0x40 && !dwFT1 && !dwFT2 && !dwFT3)
            szClient = cliSpamBot;
          if (MatchCap(caps, wLen, &capAimChat, 0x10) && !dwFT1 && !dwFT2 && !dwFT3)
            szClient = "Easy Message";
        }

        if (szClient == NULL)
        { // still unknown client, try Agile
          if (CheckContactCapabilities(hContact, CAPF_UTF) && !hasCapRichText(caps, wLen) && !dwFT1 && !dwFT2 && !dwFT3)
            szClient = "Agile Messenger";
        }
      }
    }
    else if (!dwUin)
    {
      szClient = "AIM";
    }
  }

  if (szClient == 0)
  {
    NetLog_Server("No client identification, put default ICQ client for protocol.");

    *dwClientId = 0;

    switch (wVersion)
    {  // client detection failed, provide default clients
      case 1: 
        szClient = "ICQ 1.x";
        break;
      case 2: 
        szClient = "ICQ 2.x";
        break;
      case 4:
        szClient = "ICQ98";
        break;
      case 6:
        szClient = "ICQ99";
        break;
      case 7:
        szClient = "ICQ 2000/Icq2Go";
        break;
      case 8: 
        szClient = "ICQ 2001-2003a";
        break;
      case 9: 
        szClient = "ICQ Lite";
        break;
      case 0xA:
        szClient = "ICQ 2003b";
    }
  }
  else
  {
    NetLog_Server("Client identified as %s", szClient);
  }

  return szClient;
}
