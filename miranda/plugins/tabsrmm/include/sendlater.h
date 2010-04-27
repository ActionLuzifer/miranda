/*
 * astyle --force-indent=tab=4 --brackets=linux --indent-switches
 *		  --pad=oper --one-line=keep-blocks  --unpad=paren
 *
 * Miranda IM: the free IM client for Microsoft* Windows*
 *
 * Copyright 2000-2009 Miranda ICQ/IM project,
 * all portions of this codebase are copyrighted to the people
 * listed in contributors.txt.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * you should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * part of tabSRMM messaging plugin for Miranda.
 *
 * (C) 2005-2010 by silvercircle _at_ gmail _dot_ com and contributors
 *
 * $Id$
 *
 * the sendlater class
 */

#ifndef __SENDLATER_H
#define __SENDLATER_H

#define TIMERID_SENDLATER 12000
#define TIMERID_SENDLATER_TICK 13000

#define TIMEOUT_SENDLATER 10000
#define TIMEOUT_SENDLATER_TICK 200

class CSendLaterJob {

public:
	CSendLaterJob();
	~CSendLaterJob();
	char	szId[20];									// database key name (time stamp of original send)
	HANDLE	hContact;									// original contact where the message has been assigned
	HANDLE  hTargetContact;								// *real* contact (can be different for metacontacts, e.g).
	HANDLE	hProcess;									// returned from the protocols sending service. needed to find it in the ACK handler
	time_t	created;									// job was created at this time (important to kill jobs, that are too old)
	time_t	lastSent;									// time at which the delivery was initiated. used to handle timeouts
	char	*sendBuffer;								// utf-8 send buffer
	PBYTE	pBuf;										// conventional send buffer (for non-utf8 protocols)
	DWORD	dwFlags;
	int		iSendCount;									// # of times we tried to send it...
	bool	fSuccess, fFailed;
};

typedef std::vector<CSendLaterJob *>::iterator SendLaterJobIterator;

class CSendLater {
	
public:
	enum {
		SENDLATER_AGE_THRESHOLD = (86400 * 3),				// 3 days, older messages will be removed from the db.
		SENDLATER_RESEND_THRESHOLD = 180,					// timeouted messages should be resent after that many seconds
		SENDLATER_PROCESS_INTERVAL = 50					// process the list of waiting job every this many seconds
	};

	CSendLater();
	~CSendLater();
	bool											isAvail() const { return(m_fAvail); }
	bool											isJobListEmpty() const { return(m_sendLaterJobList.empty() ? true : false); }
	void											startJobListProcess() { m_jobIterator = m_sendLaterJobList.begin(); }
	time_t											lastProcessed() const { return(m_last_sendlater_processed); }
	void											setLastProcessed(const time_t _t) { m_last_sendlater_processed = _t; }
	bool											haveJobs() const
		{
			if(m_sendLaterJobList.empty() || m_jobIterator == m_sendLaterJobList.end())
				return(false);
			else
				return(true);
		}
	bool											processCurrentJob();
	void											processContacts();
	int												addJob(const char *szSetting, LPARAM lParam);
	void											addContact(const HANDLE hContact);
	static int _cdecl								addStub(const char *szSetting, LPARAM lParam);
	HANDLE											processAck(const ACKDATA *ack);
private:
	void											processSingleContact(const HANDLE hContact);
	int												sendIt(CSendLaterJob *job);

	std::vector<HANDLE>								m_sendLaterContactList;
	std::vector<CSendLaterJob *>					m_sendLaterJobList;
	bool											m_fAvail;
	time_t											m_last_sendlater_processed;
	SendLaterJobIterator							m_jobIterator;
};

extern CSendLater* sendLater;

#endif /* __SENDLATER_H */
