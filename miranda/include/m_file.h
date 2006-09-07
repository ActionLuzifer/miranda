/*

Miranda IM: the free IM client for Microsoft* Windows*

Copyright 2000-2003 Miranda ICQ/IM project, 
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

#ifndef M_FILE_H__
#define M_FILE_H__ 1

//brings up the send file dialog for a contact
//wParam=(WPARAM)(HANDLE)hContact
//lParam=0
//returns 0 on success or nonzero on failure
//returns immediately, without waiting for the send
#define MS_FILE_SENDFILE   "SRFile/SendCommand"

//brings up the send file dialog with the specified files already chosen
//v0.1.2.1+
//wParam=(WPARAM)(HANDLE)hContact
//lParam=(LPARAM)(const char**)ppFiles
//returns 0 on success or nonzero on failure
//returns immediately, without waiting for the send
//the user is not prevented from changing the filename with the 'choose again'
//button
//ppFiles is a NULL-terminated array of fully qualified filenames.
//To send subdirectories, include their name in the list without a trailing
//backslash. In order to keep contained files in their correct place on
//receiving, the subdirectory they're in must preceed the file. This applies
//to subdirectories themselves too: they must be preceeded by their container
//if you want to send the container and keep the original directory inside it.
#define MS_FILE_SENDSPECIFICFILES  "SRFile/SendSpecificFiles"

//get the received files folder    v0.1.2.2+
//wParam=(WPARAM)(HANDLE)hContact
//lParam=(LPARAM)(char *)pszOutput
//returns 0 on success or nonzero on failure
//pszOutput must be at least MAX_PATH characters long
//If hContact is NULL this function will retrieve the received files folder
//name without any appended user names.
//Note that the directory name returned by this function does not necessarily
//exist.
#define MS_FILE_GETRECEIVEDFILESFOLDER  "SRFile/GetReceivedFilesFolder"

#endif // M_FILE_H__

