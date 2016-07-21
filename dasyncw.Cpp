/*++

     Copyright (c) 1996 Intel Corporation
     Copyright 1996 - 1998 Microsoft Corporation
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


Abstract:


--*/

#include "precomp.h"

#define INVALID_ASYNC_WINDOW ((HWND)-1)
DWORD
AsyncThreadProc(
	LPVOID  Context
    )
/*++
Routine Description:

    Thread procedure passed to CreatThread().

Arguments:

    Context - Context value passed to CreateThread().  The context value is the
              async thread object.

Return Value:

    The Return value of the worker thread

--*/

{
    PDASYNCWINDOW             Thread;
    HINSTANCE                 HModule;
    MSG                       msg;

    HModule = LoadLibraryA (gLibraryName);
    Thread = (PDASYNCWINDOW)Context;
    if (Thread->CreateAsyncWindow ()==NO_ERROR) {
        while(GetMessage( &msg, NULL, 0, 0 )== TRUE) {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }

    DEBUGF( DBG_TRACE,
            ("Exiting async thread.\n"));
    FreeLibraryAndExitThread (HModule, 0);
    return 0;   // Keep compiler happy
}

LRESULT
CALLBACK
AsyncWndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
    )
{
    PDASYNCWINDOW    Thread = (PDASYNCWINDOW)GetWindowLongPtr(hwnd, 0);
    if (Thread) {
        BOOL             ReturnCode;

        ReturnCode = Thread->ProcessAsyncMessage (msg, (SOCKET)wParam, lParam);
        if (ReturnCode==TRUE)
            return 0;
        else if (ReturnCode == -1)
            delete Thread;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}




DASYNCWINDOW::DASYNCWINDOW()
/*++
Routine Description:

    Creates any internal state.

Arguments:

    None

Return Value:

    None

--*/

{
    m_async_thread = NULL;
    m_async_window = NULL;
    m_exit_thread = FALSE;
}



DASYNCWINDOW::~DASYNCWINDOW()
/*++
Routine Description:

    destroys any internal state.

Arguments:

    None

Return Value:

    None

--*/
{

    DEBUGF( DBG_TRACE,
            ("Destroyed async thread object\n"));
}



INT
DASYNCWINDOW::Initialize(
    )
/*++
Routine Description:

    Initializes the DASYNCWINDOW object.

Arguments:

    NONE

Return Value:

    If no error occurs, Initialize() returns NO_ERROR.  Otherwise the value
    SOCKET_ERROR  is  returned,  and  a  specific  error  code  is available in
    lpErrno.

--*/
{
    INT ReturnCode;
    DWORD ThreadId;

    DEBUGF( DBG_TRACE,
            ("Initializing async thread \n"));


    //
    // Create the worker thread and wait for the new thred to finish its
    // initialization.
    //
    m_async_thread = CreateThread(
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)::AsyncThreadProc,
        this,
        0,
        &ThreadId);
    if (m_async_thread){
        while (m_async_window==NULL)
            Sleep (0);
        if (m_async_window!=INVALID_ASYNC_WINDOW)
            ReturnCode = NO_ERROR;
        else
            ReturnCode = WSAENOBUFS;
    }
    else {
        ReturnCode = CreateAsyncWindow ();
    }

    return ReturnCode;

} //Initailize

VOID
DASYNCWINDOW::Destroy(
    )
/*++
Routine Description:

    Initiates destruction of the DASYNCWINDOW object.
    This can't be done syncronously in destructor because this
    will require waiting for thread object to exit which we wont
    to avoid.

Arguments:

    NONE

Return Value:

    NONE
--*/
{
        // Just destroy the window and it will quit the thread
        // if one exists
    if ((m_async_window!=NULL) && (m_async_window!=INVALID_ASYNC_WINDOW)) {
        m_exit_thread = TRUE;
        DestroyWindow (m_async_window);
    }
}


DWORD
DASYNCWINDOW::CreateAsyncWindow (
    )
{
    WNDCLASSA wndclass;

    //
    // Register the window class if necessary.
    //

    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = (WNDPROC)AsyncWndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = sizeof(PDASYNCWINDOW);
    wndclass.hInstance = HDllInstance;
    wndclass.hIcon = LoadIcon( NULL, IDI_APPLICATION );
    wndclass.hCursor = LoadCursor( NULL, IDC_ARROW );
    wndclass.hbrBackground = (HBRUSH)GetStockObject( WHITE_BRUSH );
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = ASYNC_WINDOW_CLASS_NAME;

    if( RegisterClass( &wndclass ) == 0 ) {

        return WSAENOBUFS;

    }


    //
    // Create the window.
    //

    m_async_window = CreateWindowA(
                                ASYNC_WINDOW_CLASS_NAME,    // lpszClassName
                                "",                         // lpszWindowName
                                WS_OVERLAPPEDWINDOW,        // dwStyle
                                CW_USEDEFAULT,              // x
                                CW_USEDEFAULT,              // y
                                CW_USEDEFAULT,              // nWidth
                                CW_USEDEFAULT,              // nHeight
                                NULL,                       // hwndParent
                                NULL,                       // hmenu
                                HDllInstance,               // hinst
                                NULL                        // lpvParam
                                );

    if( m_async_window != NULL ) {

        //
        // Associate the async thread object with window.
        //
		SetWindowLongPtr(m_async_window, 0, (LONG_PTR)this);
        return NO_ERROR;

    }
    else {
        m_async_window = INVALID_ASYNC_WINDOW;
        return WSAENOBUFS;
    }

}


BOOL
DASYNCWINDOW::ProcessAsyncMessage (
    UINT    msg,
    SOCKET  s,
    LPARAM  param
    ) {
    PDSOCKET    Socket;

    if (!m_exit_thread) {
        switch( msg ) {
        case WM_SELECT_MESSAGE:
            Socket = DSOCKET::FindDSocketFromProviderSocket (s);
            if (Socket)
                Socket->SignalAsyncEvents (param);
            return TRUE;

        case WM_DESTROY :

            if (m_async_thread) {
                PostThreadMessage (NULL, WM_QUIT, 0, 0);
                CloseHandle (m_async_thread);
                m_async_thread = NULL;
                DEBUGF (DBG_TRACE, ("Async thread received quit message.\n"));
            }
            return -1;
        default:
            return FALSE;
        }
    }
    else {
        if (m_async_thread) {
            PostThreadMessage (NULL, WM_QUIT, 0, 0);
            CloseHandle (m_async_thread);
            m_async_thread = NULL;
            DEBUGF (DBG_TRACE, ("Async thread exit flag set.\n"));
        }
        return -1;
    }
}

INT
DASYNCWINDOW::RegisterSocket(
    PDSOCKET Socket
    )
/*++
Routine Description:

    Register the socket with provider to receive messages for async window
    maintained by this async thread object
    
Arguments:

    Socket - A pointer to a DSOCKET object for the socket that is registering.

Return Value:

    On success NO_ERROR else a valid winsock error code.

--*/
{
    INT        ReturnCode;
    LONG       SocketEventMask;
    SOCKET     ProviderSocket;
    PDPROVIDER Provider;

    // Get the info we need from the socket
    ProviderSocket    = Socket->GetProviderSocket();
    SocketEventMask = Socket->GetAsyncEventMask();
    Provider        = Socket->GetDProvider();

    //Add the socket to the wait array
    if (Provider->WSPAsyncSelect(
            ProviderSocket,
            m_async_window,
            WM_SELECT_MESSAGE,
            SocketEventMask,
            &ReturnCode)==0)
        return NO_ERROR;
    else
        return ReturnCode;
}



