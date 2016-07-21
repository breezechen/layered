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

dsocket.h

Abstract:

    This  header  defines the "DSOCKET" class.  The DSOCKET class defines state
    variables  and  operations for DSOCKET objects within the LSP.DLL.  A
    DSOCKET  object  represents  all  of the information that the LSP.DLL knows
    about a socket.
--*/

#ifndef _DSOCKET_
#define _DSOCKET_


#include <winsock2.h>
#include "llist.h"
#include "classfwd.h"

class DSOCKET
{
  public:

    static
    INT
    DSocketClassInitialize();

    static
    INT
    DSocketClassCleanup();

    static 
    PDSOCKET
    FindDSocketFromProviderSocket (
        SOCKET s
        );


    DSOCKET();

    INT
    Initialize(
        IN PDPROVIDER Provider,
        IN SOCKET     ProviderSocket,
        IN DWORD      CatalogEntryId,
        IN SOCKET     Socket
        );
    VOID
    Remove ();

    ~DSOCKET();

    SOCKET
    GetProviderSocket();

    PDPROVIDER
    GetDProvider();

    DWORD
    GetCatalogEntryId();

    SOCKET
    GetSocketHandle();

    VOID
    RegisterAsyncOperation(
        HWND     Window,
        UINT     Message,
        LONG     Events
        );

    VOID
    SignalAsyncEvents(
        LPARAM    lParam
        );

    LONG
    GetAsyncEventMask();

    DWORD
    GetCompletionContext ();

    VOID
    SetCompletionContext (
        DWORD   context
        );

  private:

    PDPROVIDER  m_provider;
    // Reference  to  the  DPROVIDER object representing the service provider
    // that controls this socket.

    SOCKET  m_provider_socket;
    // Socket handle exposed by the provider

    DWORD   m_catalog_entry_id;
    // The catalog entry id of the provider that this socket is attached to.

    SOCKET  m_socket_handle;
    // The socket handle returned from WPUCreateSocketHandle.

    DWORD   m_completion_context;
    // Completion key if provider socket is associated with
    // completion port

    LONG    m_async_events;
    // The event mask for the events the client has registered interest in.

    HWND    m_async_window;
    // The handle of the window to receive net event messages.

    UINT    m_async_message;
    // The message to send to the client to signal net envents.

    BOOL    m_closing;
    // Indicates that socket is being closed
    
    LIST_ENTRY  m_list_linkage;
    // Provides the linkage space for a list of DSOCKET objects 

    // Note that no LIST_ENTRY is required to correspond to the DPROVIDER
    // object associated  with  this  DSOCKET object since the DPROVIDER object
    // does not  maintain a list of sockets it controls.
    
    static LIST_ENTRY m_socket_list;
    // Global list of all sockets

    static CRITICAL_SECTION  m_socket_list_lock;
    // Critical section that protects socket list

};   // class DSOCKET



inline SOCKET
DSOCKET::GetSocketHandle()
/*++

Routine Description:

    Retrieves  the  external socket-handle value corresponding to this internal
    DSOCKET object.

Arguments:

    None

Return Value:

    The corresponding external socket-handle value.
--*/
{
    return(m_socket_handle);
}




inline PDPROVIDER
DSOCKET::GetDProvider()
/*++

Routine Description:

    Retrieves  a reference to the DPROVIDER object associated with this DSOCKET
    object.

Arguments:

    None

Return Value:

    The reference to the DPROVIDER object associated with this DSOCKET object.
--*/
{
    return(m_provider);
}


inline DWORD
DSOCKET::GetCatalogEntryId()
/*++

Routine Description:

    Retrieves  a reference to the DPROVIDER object associated with this DSOCKET
    object.

Arguments:

    None

Return Value:

    The reference to the DPROVIDER object associated with this DSOCKET object.
--*/
{
    return(m_catalog_entry_id);
}


inline SOCKET
DSOCKET::GetProviderSocket()
/*++

Routine Description:

    Retrieves the handle for the provider socket

Arguments:

    None

Return Value:

    The provider socket associated with this socket
--*/
{
    return(m_provider_socket);
}

inline
LONG
DSOCKET::GetAsyncEventMask()
/*++

Routine Description:

    Returns the event mask for this socket

Arguments:

    None

Return Value:

    The event mask for this socket.
--*/
{
    return(m_async_events);
}


inline DWORD
DSOCKET::GetCompletionContext()
/*++

Routine Description:

    Retrieves provider's socket completion context,

Arguments:

    None

Return Value:

    Provider's socket completion context

--*/
{
    return(m_completion_context);
}


inline VOID
DSOCKET::SetCompletionContext(
    DWORD   Context
    )
/*++

Routine Description:

    Sets provider's socket completion context,

Arguments:

    Context     completion context

Return Value:
    
    None

--*/
{
    m_completion_context = Context;
}




#endif // _DSOCKET_
