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

dworker.h

Abstract:

   This module contins the interface to the DWORKERTHREAD class for
   LSP.DLL.

--*/

#ifndef _DWORKERTHREAD_
#define _DWORKERTHREAD_


#include <winsock2.h>
#include "llist.h"
#include "doverlap.h"
#include "classfwd.h"

#define WSP_RECV       0x00000001
#define WSP_RECVFROM   0x00000002
#define WSP_SEND       0x00000004
#define WSP_SENDTO     0x00000008
#define WSP_IOCTL      0x00000010
#define TRANSMIT_FILE  0x00000020
#define ACCEPT_EX      0x00000040



class DWORKERTHREAD
{
  public:

    DWORKERTHREAD();

    INT
    Initialize();

    VOID
    Destroy ();

    ~DWORKERTHREAD();

    BOOL
    RegisterSocketHandle (
        SOCKET  socket
        );

    DWORD
    WorkerThreadProc();

    LPWSATHREADID
    GetThreadId();

    INT
    QueueOverlappedRecv(
        PDSOCKET                           Socket,
        LPWSABUF                           UserBuffers,
        DWORD                              UserBufferCount,
        LPDWORD                            UserBytesRecvd,
        LPDWORD                            UserFlags,
        LPWSAOVERLAPPED                    UserOverlappedStruct,
        LPWSAOVERLAPPED_COMPLETION_ROUTINE UserlpCompletionRoutine,
        LPWSATHREADID                      UserThreadId,
        LPWSABUF                           InternalBuffers,
        DWORD                              InternalBufferCount,
        LPINT                              Errno
        );

    INT
    QueueOverlappedRecvFrom(
        PDSOCKET                           Socket,
        LPWSABUF                           UserBuffers,
        DWORD                              UserBufferCount,
        LPDWORD                            UserBytesRecvd,
        LPDWORD                            UserFlags,
        struct sockaddr FAR *              UserFrom,
        LPINT                              UserFromlen,
        LPWSAOVERLAPPED                    UserOverlappedStruct,
        LPWSAOVERLAPPED_COMPLETION_ROUTINE UserlpCompletionRoutine,
        LPWSATHREADID                      UserThreadId,
        LPWSABUF                           InternalBuffers,
        DWORD                              InternalBufferCount,
        LPINT                              Errno
        );
    INT
    QueueOverlappedSend(
        PDSOCKET                           Socket,
        LPWSABUF                           UserBuffers,
        DWORD                              UserBufferCount,
        LPDWORD                            UserBytesSent,
        DWORD                            UserFlags,
        LPWSAOVERLAPPED                    UserOverlappedStruct,
        LPWSAOVERLAPPED_COMPLETION_ROUTINE UserlpCompletionRoutine,
        LPWSATHREADID                      UserThreadId,
        LPINT           Errno
        );

    INT
    QueueOverlappedSendTo(
        PDSOCKET                           Socket,
        LPWSABUF                           UserBuffers,
        DWORD                              UserBufferCount,
        LPDWORD                            UserBytesSent,
        DWORD                              UserFlags,
        const struct sockaddr FAR *        UserTo,
        INT                                UserTolen,
        LPWSAOVERLAPPED                    UserOverlappedStruct,
        LPWSAOVERLAPPED_COMPLETION_ROUTINE UserlpCompletionRoutine,
        LPWSATHREADID                      UserThreadId,
        LPINT                              Errno
        );

    INT
    QueueOverlappedIoctl(
        PDSOCKET                           Socket,
        DWORD                              dwIoControlCode,
        LPVOID                             lpvInBuffer,
        DWORD                              cbInBuffer,
        LPVOID                             lpvOutBuffer,
        DWORD                              cbOutBuffer,
        LPDWORD                            lpcbBytesReturned,
        LPWSAOVERLAPPED                    UserOverlappedStruct,
        LPWSAOVERLAPPED_COMPLETION_ROUTINE UserlpCompletionRoutine,
        LPWSATHREADID                      UserThreadId,
        LPINT                              Errno
        );

    INT
    QueueOverlappedAcceptEx(
        PDSOCKET                           ListenSocket,
        PDSOCKET                           AcceptSocket,
        LPVOID                             lpOutputBuffer,
        DWORD                              dwReceiveDataLength,
        DWORD                              dwLocalAddressLength,
        DWORD                              dwRemoteAddressLength,
        LPWSAOVERLAPPED                    UserOverlappedStruct,
        LPINT                              Errno
        );

    INT
    QueueOverlappedTransmitFile(
        PDSOCKET                           Socket,
        HANDLE                             hFile,
        DWORD                              nNumberOfBytesToWrite,
        DWORD                              nNumberOfBytesPerSend,
        LPWSAOVERLAPPED                    UserOverlappedStruct,
        LPTRANSMIT_FILE_BUFFERS            lpTransmitBuffers,
        DWORD                              dwReserved,
        LPINT                              Errno
        );
  private:
    VOID
    AddOverlappedOperation(
        IN PDSOCKET                  Socket,
        IN PINTERNALOVERLAPPEDSTRUCT OverlappedOperation
        );

    PINTERNALOVERLAPPEDSTRUCT
    NextOverlappedOperation();

    DWORD m_thread_count;
    // Number of worker threads.

    HANDLE m_completion_port;
    // The handle to NT completion port used for accepting of completion
    // indication from providers in worker thread(s)

    HANDLE m_wakeup_semaphore;
    // The handle to the WIN32 semaphore used to communicate with our worker
    // thread(s) if completion port is not available (Win9x).

    BOOL   m_exit_thread;
    // A boolean to tell our threads to exit at object destruction time.

    LIST_ENTRY       m_overlapped_operation_queue;

    CRITICAL_SECTION m_overlapped_operation_queue_lock;

    WSATHREADID  m_thread_id;
    // The thread ID for the worker thread.

};   // class DWORKERTHREAD

inline
LPWSATHREADID
DWORKERTHREAD::GetThreadId()
{
    return(&m_thread_id);
}

#endif // _DWORKERTHREAD_
