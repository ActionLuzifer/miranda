/*

Miranda IM: the free IM client for Microsoft* Windows*

Copyright 2000-2009 Miranda ICQ/IM project, 
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
#include "clc.h"

#define WM_DOCKCALLBACK   (WM_USER+121)
#define EDGESENSITIVITY   3

#define DOCKED_NONE    0
#define DOCKED_LEFT    1
#define DOCKED_RIGHT   2

static char docked;

static void Docking_GetMonitorRectFromPoint(LPPOINT pt, LPRECT rc)
{
	if (MyMonitorFromPoint) 
	{
		MONITORINFO monitorInfo;
		HMONITOR hMonitor = MyMonitorFromPoint(*pt, MONITOR_DEFAULTTONEAREST); // always returns a valid value
		monitorInfo.cbSize = sizeof(MONITORINFO);

		if (MyGetMonitorInfo(hMonitor, &monitorInfo)) 
		{
			*rc = monitorInfo.rcMonitor;
			return;
		}
	}

	// "generic" win95/NT support, also serves as failsafe
	rc->left = 0;
	rc->top = 0;
	rc->bottom = GetSystemMetrics(SM_CYSCREEN);
	rc->right = GetSystemMetrics(SM_CXSCREEN);
}

static void Docking_GetMonitorRectFromWindow(HWND hWnd, RECT * rc)
{
	GetWindowRect(hWnd, rc);
	Docking_GetMonitorRectFromPoint((LPPOINT)rc, rc);
}

static void Docking_AdjustPosition(HWND hwnd, RECT * rcDisplay, RECT * rc, bool query)
{
	APPBARDATA abd = {0};

	abd.cbSize = sizeof(abd);
	abd.hWnd = hwnd;
	abd.uEdge = docked == DOCKED_LEFT ? ABE_LEFT : ABE_RIGHT;
	abd.rc.top = rcDisplay->top;
	abd.rc.bottom = rcDisplay->bottom;
	if (docked == DOCKED_LEFT) 
	{
		abd.rc.right = rcDisplay->left + (rc->right - rc->left);
		abd.rc.left = rcDisplay->left;
	}
	else
	{
		abd.rc.left = rcDisplay->right - (rc->right - rc->left);
		abd.rc.right = rcDisplay->right;
	}
	SHAppBarMessage(query ? ABM_QUERYPOS : ABM_SETPOS, &abd);
	*rc = abd.rc;
}

static UINT_PTR Docking_Command(HWND hwnd, int cmd)
{
	APPBARDATA abd = {0};

	abd.cbSize = sizeof(abd);
	abd.hWnd = hwnd;
	abd.uCallbackMessage = WM_DOCKCALLBACK;
	return SHAppBarMessage(cmd, &abd);
}

static void Docking_SetSize(HWND hwnd, LPRECT rc, bool query)
{
	RECT rcMonitor;
	Docking_GetMonitorRectFromPoint((LPPOINT)rc, &rcMonitor);
	Docking_AdjustPosition(hwnd, &rcMonitor, rc, query);
}

static bool Docking_IsWindowVisible(HWND hwnd)
{
	LONG style = GetWindowLong(hwnd, GWL_STYLE);
	return style & WS_VISIBLE && !(style & WS_MINIMIZE);
}

INT_PTR Docking_IsDocked(WPARAM, LPARAM)
{
	return docked;
}

int fnDocking_ProcessWindowMessage(WPARAM wParam, LPARAM lParam)
{
	static int draggingTitle;
	MSG *msg = (MSG *) wParam;

	if (msg->message == WM_DESTROY)
		DBWriteContactSettingByte(NULL, "CList", "Docked", (BYTE) docked);

	if (!docked && msg->message != WM_CREATE && msg->message != WM_MOVING)
		return 0;

	switch (msg->message) 
	{
	case WM_CREATE:
		draggingTitle = 0;
		docked = DBGetContactSettingByte(NULL, "CLUI", "DockToSides", 1) ? 
			(char) DBGetContactSettingByte(NULL, "CList", "Docked", 0) : 0;
		break;

	case WM_ACTIVATE:
		Docking_Command(msg->hwnd, ABM_ACTIVATE);
		break;

	case WM_WINDOWPOSCHANGING:
	{
		LPWINDOWPOS wp = (LPWINDOWPOS)msg->lParam;

		bool vis = Docking_IsWindowVisible(msg->hwnd);
		if (wp->flags & SWP_SHOWWINDOW) 
			vis = !IsIconic(msg->hwnd);
		if (wp->flags & SWP_HIDEWINDOW)
			vis = false;

		if (vis)
		{
			if (!(wp->flags & (SWP_NOMOVE | SWP_NOSIZE)))
			{
				bool addbar = Docking_Command(msg->hwnd, ABM_NEW) != 0;
				
				RECT rc = {0};
				GetWindowRect(msg->hwnd, &rc);
				int cx = rc.right - rc.left + 1;
				if (!(wp->flags & SWP_NOMOVE)) { rc.left = wp->x; rc.top = wp->y; }
				if (!(wp->flags & SWP_NOSIZE)) 
				{ 
					rc.right = rc.left + wp->cx - 1; 
					rc.bottom = rc.top + wp->cy - 1; 
					addbar |= (cx != wp->cx); 
				}
				
				Docking_SetSize(msg->hwnd, &rc, !addbar);

				if (!(wp->flags & SWP_NOMOVE)) { wp->x = rc.left; wp->y = rc.top; }
				if (!(wp->flags & SWP_NOSIZE)) wp->cy = rc.bottom - rc.top + 1;

				*((LRESULT *) lParam) = TRUE; 
				return TRUE;
			}
			else
			{
				if ((wp->flags & SWP_SHOWWINDOW) && Docking_Command(msg->hwnd, ABM_NEW)) 
				{
					RECT rc = {0};
					GetWindowRect(msg->hwnd, &rc);
					Docking_SetSize(msg->hwnd, &rc, false);
					wp->x = rc.left; 
					wp->y = rc.top; 
					wp->cy = rc.bottom - rc.top + 1;
					wp->cx = rc.right - rc.left + 1;
					wp->flags &=  ~(SWP_NOSIZE | SWP_NOMOVE);
				}
			}
		}
		break;
	}

	case WM_WINDOWPOSCHANGED:
	{
		LPWINDOWPOS wp = (LPWINDOWPOS)msg->lParam;
		bool vis = Docking_IsWindowVisible(msg->hwnd);
		if (wp->flags & SWP_SHOWWINDOW) 
			vis = !IsIconic(msg->hwnd);
		if (wp->flags & SWP_HIDEWINDOW)
			vis = false;

		if (!vis)
			Docking_Command(msg->hwnd, ABM_REMOVE);

		Docking_Command(msg->hwnd, ABM_WINDOWPOSCHANGED);
		break;
	}

	case WM_MOVING:
		if (!docked)
		{
			RECT rcMonitor;
			POINT ptCursor;

			// stop early
			if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
				return 0;

			// GetMessagePos() is no good, position is always unsigned
			GetCursorPos(&ptCursor);
			Docking_GetMonitorRectFromPoint(&ptCursor, &rcMonitor);

			if (((ptCursor.x < rcMonitor.left + EDGESENSITIVITY) || 
				(ptCursor.x >= rcMonitor.right - EDGESENSITIVITY)) &&
				DBGetContactSettingByte(NULL, "CLUI", "DockToSides", 1)) 
			{
				docked = (ptCursor.x < rcMonitor.left + EDGESENSITIVITY) ? DOCKED_LEFT : DOCKED_RIGHT;
				PostMessage(msg->hwnd, WM_LBUTTONUP, 0, MAKELPARAM(ptCursor.x, ptCursor.y));

				Docking_Command(msg->hwnd, ABM_NEW);
				Docking_AdjustPosition(msg->hwnd, &rcMonitor, (LPRECT)msg->lParam, false);

				*((LRESULT *) lParam) = TRUE; 
				return TRUE;
			}
		}
		break;

	case WM_NCHITTEST:
		switch (DefWindowProc(msg->hwnd, WM_NCHITTEST, msg->wParam, msg->lParam))
		{
		case HTSIZE: case HTTOP: case HTTOPLEFT: case HTTOPRIGHT:
		case HTBOTTOM: case HTBOTTOMRIGHT: case HTBOTTOMLEFT: 
			*((LRESULT *) lParam) = HTCLIENT;
			return TRUE;

		case HTLEFT:
			if (docked == DOCKED_LEFT)
			{
				*((LRESULT *) lParam) = HTCLIENT;
				return TRUE;
			}
			break;

		case HTRIGHT:
			if (docked == DOCKED_RIGHT)
			{
				*((LRESULT *) lParam) = HTCLIENT;
				return TRUE;
			}
			break;
		}
		break;

	case WM_SYSCOMMAND:
		if ((msg->wParam & 0xFFF0) != SC_MOVE)
			return 0;

		SetActiveWindow(msg->hwnd);
		SetCapture(msg->hwnd);
		draggingTitle = 1;
		*((LRESULT *) lParam) = 0;
		return 1;

	case WM_MOUSEMOVE:
		if (draggingTitle)
		{
			RECT rc;
			POINT pt;
			GetClientRect(msg->hwnd, &rc);
			if ((docked == DOCKED_LEFT && (short) LOWORD(msg->lParam) > rc.right) ||
				(docked == DOCKED_RIGHT && (short) LOWORD(msg->lParam) < 0)) 
			{
				ReleaseCapture();
				draggingTitle = 0;
				docked = 0;
				GetCursorPos(&pt);
				PostMessage(msg->hwnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(pt.x, pt.y));
				SetWindowPos(msg->hwnd, 0, pt.x - rc.right / 2, 
					pt.y - GetSystemMetrics(SM_CYFRAME) - GetSystemMetrics(SM_CYSMCAPTION) / 2,
					DBGetContactSettingDword(NULL, "CList", "Width", 0), 
					DBGetContactSettingDword(NULL, "CList", "Height", 0),
					SWP_NOZORDER);
				Docking_Command(msg->hwnd, ABM_REMOVE);
			}
			return 1;
		}
		break;

	case WM_LBUTTONUP:
		if (draggingTitle) 
		{
			ReleaseCapture();
			draggingTitle = 0;
		}
		break;

	case WM_DOCKCALLBACK:
		switch (msg->wParam)
		{
		case ABN_WINDOWARRANGE:
			ShowWindow(msg->hwnd, msg->lParam ? SW_HIDE : SW_SHOW);
			break;

		case ABN_POSCHANGED:
			break;
		}
		return 1;

	case WM_DESTROY:
		Docking_Command(msg->hwnd, ABM_REMOVE);
		break;
	}
	return 0;
}
