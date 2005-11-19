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
//  Describe me here please...
//
// -----------------------------------------------------------------------------

#ifndef __ICQ_RATES_H
#define __ICQ_RATES_H


#define RIT_AWAYMSG_RESPONSE 0x01   // response to status msg request

#define RIT_XSTATUS_REQUEST  0x10   // schedule xstatus details requests
#define RIT_XSTATUS_RESPONSE 0x11   // response to xstatus details request

typedef struct rate_record_s
{
  BYTE bType;         // type of request
  int rate_group;
  int nMinDelay;
  HANDLE hContact;
  DWORD dwUin;
  DWORD dwMid1;
  DWORD dwMid2;
  WORD wCookie;
  BOOL bThruDC;
  char *szData;
  BYTE msgType;
} rate_record;

// Level 2 of rate management
int handleRateItem(rate_record *item, BOOL bAllowDelay);

void InitRates();
void UninitRates();

#endif /* __ICQ_RATES_H */