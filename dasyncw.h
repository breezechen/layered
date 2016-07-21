/*++


     Copyright c 1996 Intel Corporation
     All Rights Reserved

     Permission is granted to use, copy and distribute this software and
     its documentation for any purpose and without fee, provided, that
     the above copyright notice and this statement appear in all copies.
     Intel makes no representations about the suitability of this
     software for any purpose.  This software is provided "AS IS."

     Intel specifically disclaims all warranties, express or implied,
     and all liability, including consequential and other indirect
     damages, for the use of this software, including liability for
     infringement of any proprietary rights, and including the
     warranties of merchantability and fitness for a particular purpose.
     Intel does not assume any responsibility for any errors which may
     appear in this software nor any responsibility to update it.


Module Name:

dasyncw.h

Abstract:

   This module contins the interface to the DASYNCWINDOW class for
   LSP.DLL.

--*/

#ifndef _DASYNCWINDOW_
#define _DASYNCWINDOW_

#include <winsock2.h>
#include "classfwd.h"


#define WM_SELECT_MESSAGE   (WM_USER+100)
#define ASYNC_WINDOW_CLASS_NAME "Layered Provider Async Window"

class DASYNCWINDOW
{
  public:

    DASYNCWINDOW();

    INT
    Initialize();

    VOID
    Destroy ();

    ~DASYNCWINDOW();

    DWORD
    CreateAsyncWindow ();

    BOOL
    ProcessAsyncMessage (
        UINT    msg,
        SOCKET  s,
        LPARAM  param
        );

    INT
    RegisterSocket(
        PDSOCKET Socket
        );


private:
    
    HWND    m_async_window;
    HANDLE  m_async_thread;
    BOOL    m_exit_thread;

};



#endif