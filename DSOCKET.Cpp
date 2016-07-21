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

dsocket.cpp

Abstract:

This module contains the implemetation of the dsocket object used
by lsp.dll

--*/

#include "precomp.h"

LIST_ENTRY  DSOCKET::m_socket_list;
CRITICAL_SECTION DSOCKET::m_socket_list_lock;

INT
DSOCKET::DSocketClassInitialize (
    )
{
    InitializeCriticalSection (&m_socket_list_lock);
    InitializeListHead (&m_socket_list);
    return 0;
}


INT
DSOCKET::DSocketClassCleanup(
    )
{
    PLIST_ENTRY  ListMember;
    PDSOCKET     Socket;
    PDPROVIDER   Provider;
    SOCKET       ProviderSocket;
    INT          Errno;

    //Kill all the open sockets
    ListMember = m_socket_list.Flink;

    while (ListMember != &m_socket_list){
        Socket = CONTAINING_RECORD(
            ListMember,
            DSOCKET,
            m_list_linkage);
        ListMember = ListMember->Flink;

        Socket->Remove ();
        Provider = Socket->GetDProvider();
        ProviderSocket = Socket->GetProviderSocket();


        Provider->WSPCloseSocket(
            ProviderSocket,
            &Errno);

        gUpCallTable.lpWPUCloseSocketHandle(
            Socket->GetSocketHandle(),
            &Errno);
        
        delete(Socket);
    } //while

    DeleteCriticalSection (&m_socket_list_lock);
    return 0;
}


PDSOCKET
DSOCKET::FindDSocketFromProviderSocket (
    SOCKET  s
    ) {
    PDSOCKET    Socket = NULL;
    PLIST_ENTRY ListMember;

    EnterCriticalSection (&m_socket_list_lock);
    ListMember = m_socket_list.Flink;

    while (ListMember != &m_socket_list){
        PDSOCKET    Socket2 = CONTAINING_RECORD(
            ListMember,
            DSOCKET,
            m_list_linkage);
        ListMember = ListMember->Flink;
        if (Socket2->GetProviderSocket()==s) {
            Socket = Socket2;   
            break;
        }
    }
    LeaveCriticalSection (&m_socket_list_lock);

    return Socket;

}


DSOCKET::DSOCKET(
    )
/*++

Routine Description:

    DSOCKET  object  constructor.   Creates and returns a DSOCKET object.  Note
    that  the  DSOCKET object has not been fully initialized.  The "Initialize"
    member function must be the first member function called on the new DSOCKET
    object.

Arguments:

    None

Return Value:

    None
--*/
{
    // Set our data members to known values
    m_provider          = NULL;
    m_socket_handle     = INVALID_SOCKET;
    m_catalog_entry_id  = NULL;
    m_provider_socket   = INVALID_SOCKET;
    m_completion_context = INVALID_SOCKET;
    m_async_events      = NULL;
    m_async_window      = NULL;
    m_async_message     = NULL;
    m_closing           = FALSE;
}




INT
DSOCKET::Initialize(
        IN PDPROVIDER Provider,
        IN SOCKET     ProviderSocket,
        IN DWORD      CatalogEntryId,
        IN SOCKET     SocketHandle
        )
/*++

Routine Description:

    Completes  the  initialization  of  the  DSOCKET object.  This must be the
    first  member  function  called  for  the  DSOCKET object.  This procedure
    should be called only once for the object.

Arguments:

    Provider - Supplies  a  reference  to  the DPROVIDER object associated with
               this DSOCKET object.

    ProviderSocket - The socket handle returned from the lower level provider.

    CatalogEntryId - The CatalogEntryId for the provider referenced by
                     m_provider.

    SocketHandle        - The socket handle returned from WPUCreateSocketHandle().

Return Value:

    The  function returns ERROR_SUCCESS if successful.  Otherwise it
    returns an appropriate WinSock error code if the initialization
    cannot be completed.
--*/
{
    // Store the provider and process object.
    m_provider = Provider;
    m_provider_socket = ProviderSocket;
    m_catalog_entry_id = CatalogEntryId;
    m_socket_handle = SocketHandle;

    // Add this socket to the list of sockets.
    EnterCriticalSection(&m_socket_list_lock);
    InsertHeadList(
        &m_socket_list,
        &m_list_linkage);
    LeaveCriticalSection(&m_socket_list_lock);
    DEBUGF( DBG_TRACE,
            ("Initializing socket %X\n",this));
    return(ERROR_SUCCESS);
}

VOID
DSOCKET::Remove (
    )
{
    EnterCriticalSection(&m_socket_list_lock);
    RemoveEntryList(&m_list_linkage);
    LeaveCriticalSection(&m_socket_list_lock);
}


DSOCKET::~DSOCKET()
/*++

Routine Description:

    DSOCKET  object  destructor.   This  procedure  has  the  responsibility to
    perform  any required shutdown operations for the DSOCKET object before the
    object  memory  is  deallocated.

Arguments:

    None

Return Value:

    None
--*/
{

    DEBUGF( DBG_TRACE,
            ("Destroying socket %X\n",this));
}


VOID
DSOCKET::RegisterAsyncOperation(
    HWND     Window,
    UINT     Message,
    LONG     Events
    )
/*++

Routine Description:

    Registers interest in net work events.

Arguments:

    Window  - The handle to the window that will receive notification of
              network events.

    Message - The message to send for net event notification.

    Events  - The events to be registered.

Return Value:

    NO_ERROR on success else a valid winsock errorcode.

--*/
{

    m_async_window  = Window;
    m_async_message = Message;
    m_async_events  = Events;
}

VOID
DSOCKET::SignalAsyncEvents(
    LPARAM    lParam
    )
/*++

Routine Description:

     The notification function called by the worker thread to signal network
     events.

Arguments:

    None

Return Value:

    None
--*/
{

    gUpCallTable.lpWPUPostMessage (m_async_window,
                                    m_async_message,
                                    (WPARAM)m_socket_handle,
                                    lParam);

}

