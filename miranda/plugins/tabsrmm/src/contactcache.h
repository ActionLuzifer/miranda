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
 * (C) 2005-2009 by silvercircle _at_ gmail _dot_ com and contributors
 *
 * $Id$
 *
 * the contact cache
 *
 */

#ifndef __CONTACTCACHE_H
#define __CONTACTCACHE_H

#define C_INVALID_PROTO "<proto error>"
#define C_INVALID_ACCOUNT _T("<account error>")
#define C_INVALID_PROTO_T _T("<proto error>")
#define HISTORY_INITIAL_ALLOCSIZE 300


#endif /* __CONTACTCACHE_H */

struct TInputHistory {
	TCHAR*	szText;
	size_t	lLen;
};

struct TSessionStats {
	enum {
		BYTES_RECEIVED 		= 1,
		BYTES_SENT 			= 2,
		FAILURE				= 3,
		UPDATE_WITH_LAST_RCV= 4,
		SET_LAST_RCV		= 5,
		INIT_TIMER			= 6,
	};

	time_t 			started;
	unsigned int 	iSent, iReceived, iSentBytes, iReceivedBytes;
	unsigned int	messageCount;
	unsigned int 	iFailures;
	unsigned int 	lastReceivedChars;
	BOOL 			bWritten;
};

class CContactCache {
public:

	CContactCache									() {}
	CContactCache									(const HANDLE hContact);
	~CContactCache									()
	{
		int i;

		if(m_stats)
			delete m_stats;

		if (m_history) {
			for (i = 0; i <= m_iHistorySize; i++) {
				if (m_history[i].szText != 0) {
					free(m_history[i].szText);
				}
			}
			free(m_history);
		}
	}
	void					inc						() { m_accessCount++; }
	const	WORD			getStatus				() const { return(m_wStatus); }
	const	WORD			getMetaStatus			() const { return(m_wMetaStatus); }
	const	WORD			getActiveStatus			() const { return(m_isMeta ? m_wMetaStatus : m_wStatus); }
	const	WORD			getOldStatus			() const { return(m_wOldStatus); }
	const	TCHAR*			getNick					() const { return(m_szNick); }
	const	HANDLE			getContact				() const { return(m_hContact); }
	const	HANDLE			getActiveContact		() const { return(m_isMeta ? (m_hSubContact ? m_hSubContact : m_hContact) : m_hContact); }
	const	DWORD			getIdleTS				() const { return(m_idleTS); }
	const	char*			getProto				() const { return(m_szProto); }
	const	TCHAR*			getProtoT				() const { return(m_tszProto); }
	const	char*			getMetaProto			() const { return(m_szMetaProto ? m_szMetaProto : C_INVALID_PROTO); }
	const	TCHAR*			getMetaProtoT			() const { return(m_szMetaProto ? m_tszMetaProto : C_INVALID_PROTO_T); }
	const	char*			getActiveProto			() const { return(m_isMeta ? (m_szMetaProto ? m_szMetaProto : m_szProto) : m_szProto); }
	const	TCHAR*			getActiveProtoT			() const { return(m_isMeta ? (m_szMetaProto ? m_tszMetaProto : m_tszProto) : m_tszProto); }
	bool					isMeta					() const { return(m_isMeta); }
	const TCHAR*			getRealAccount			() const { return(m_szAccount ? m_szAccount : C_INVALID_ACCOUNT); }
	const TCHAR*			getUIN					() const { return(m_szUIN); }
	const HWND				getWindowData			(_MessageWindowData*& dat) const { dat = m_dat; return(m_hwnd); }

	void					updateStats				(int iType, size_t value = 0);
	const DWORD				getSessionStart			() const { return(m_stats->started); }
	const int				getSessionMsgCount		() const { return((int)m_stats->messageCount) ; }
	void					updateState				();
	bool					updateStatus			();
	void					updateNick				();
	void					updateMeta				();
	void					updateUIN				();
	void					setWindowData			(const HWND hwnd = 0, _MessageWindowData *dat = 0);

	/*
	 * input history
	 */
	void 					saveHistory				(WPARAM wParam, LPARAM lParam);
	void					inputHistoryEvent		(WPARAM wParam);

private:
	void					allocStats				();
	void					initPhaseTwo			();
	void					allocHistory			();

private:
	size_t				m_accessCount;
	HANDLE				m_hContact;
	HANDLE				m_hSubContact;
	WORD				m_wStatus, m_wMetaStatus;
	WORD				m_wOldStatus;
	char*				m_szProto, *m_szMetaProto;
	TCHAR*				m_tszProto, m_tszMetaProto[40];
	TCHAR*				m_szAccount;
	TCHAR				m_szNick[80], m_szUIN[80];
	DWORD				m_idleTS;
	bool				m_isMeta;
	bool				m_Valid;
	HWND				m_hwnd;
	int     			m_iHistoryCurrent, m_iHistoryTop, m_iHistorySize;
	_MessageWindowData*	m_dat;
	TSessionStats* 		m_stats;
	TInputHistory*		m_history;
};

struct TCCache {
	HANDLE			hContact;
	CContactCache 	*c;
};
