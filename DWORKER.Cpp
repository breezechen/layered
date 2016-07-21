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


PDOVERLAPPEDSTRUCTMGR gOverlappedManager;




VOID
CALLBACK
APCProc(
	DWORD_PTR Context
    )
/*++
Routine Description:

    This routine unpacks the context value passed to WPUQueueApc() and calls
    the users completion function. This function is called in the clients
    thread context.

Arguments:

    Context - The context value passed to WPUQueueApc().

Return Value:

    None

--*/
{
    LPOVERLAPPED                        lpOverlapped;
    LPWSAOVERLAPPED_COMPLETION_ROUTINE  UserCompletionRoutine;

    lpOverlapped = (LPOVERLAPPED) Context;
    UserCompletionRoutine =
        (LPWSAOVERLAPPED_COMPLETION_ROUTINE)lpOverlapped->Internal;
    lpOverlapped->Internal = lpOverlapped->Offset; // To make sure it
                                                   // in no longer is 
                                                   // WSS_OPERATION_IN_PROGRESS
    UserCompletionRoutine (
        lpOverlapped->Offset,
        lpOverlapped->InternalHigh,
        lpOverlapped,
        lpOverlapped->OffsetHigh
        );


}

VOID
CALLBACK
OverlappedCompletionProc(
    DWORD  dwError,
    DWORD  cbTransferred,
    LPWSAOVERLAPPED lpOverlapped,
    DWORD dwFlags
    )
/*++
Routine Description:

    This routine is the completion routine for all overlapped operations
    initiated with the lower level provider.

Arguments:

    dwError - The error code for the overlapped operation.

    cbTransferred - The number of bytes transfered by the overlapped
                    operation.

    lpOverlapped - a pointer to the WSAOVERLAPPED struct associated with the
                   overlapped operation.

    dwFlags -  Not Used must be zero.

Return Value:

    None

--*/
{
    INT                                Errno = NO_ERROR;
    PINTERNALOVERLAPPEDSTRUCT          OverlappedStruct;
    DWORD                              OperationType;
    PDPROVIDER                         Provider;
    SOCKET                             ProviderSocket, Socket;
    WSATHREADID                        UserThreadId;
    LPWSAOVERLAPPED_COMPLETION_ROUTINE UserCompletionRoutine;
    LPWSAOVERLAPPED                    UserOverlappedStruct;



    // Get the stored overlapped operation parameters
    OverlappedStruct = gOverlappedManager->GetInternalOverlappedStructure(
                        lpOverlapped);
    // If the completed operation was a recieve operation copy the internal
    // buffers into the users buffers.
    if (OverlappedStruct != NULL){
        OperationType = OverlappedStruct->iolOperationType;
        Socket = OverlappedStruct->iolSocket;
        Provider = OverlappedStruct->iolProvider;
        ProviderSocket = OverlappedStruct->iolProviderSocket;
        UserCompletionRoutine = OverlappedStruct->iolUserCompletionRoutine;
        UserOverlappedStruct = OverlappedStruct->iolUserOverlappedStruct;
        UserThreadId = OverlappedStruct->iolUserThreadId;
        
        if (dwError==WSA_IO_PENDING)
            Provider->WSPGetOverlappedResult (
                    ProviderSocket,
                    lpOverlapped,
                    &cbTransferred,
                    FALSE,
                    &dwFlags,
                    (int *)&dwError);
                                

        switch (OperationType) {
        case WSP_RECV:
        case WSP_RECVFROM:
            gBufferManager->CopyBuffer(
                (OverlappedStruct->iolInternalBufferCount <= MAX_FAST_BUFS)
                    ? &OverlappedStruct->iolInternalBuffers[0]
                    : OverlappedStruct->iolpInternalBuffers,
                OverlappedStruct->iolInternalBufferCount,
                0,
                cbTransferred,
                (OverlappedStruct->iolUserBufferCount <= MAX_FAST_BUFS)
                    ? &OverlappedStruct->iolUserBuffers[0]
                    : OverlappedStruct->iolpUserBuffers,
                OverlappedStruct->iolUserBufferCount,
                0);
            gBufferManager->FreeBuffer(
                (OverlappedStruct->iolInternalBufferCount <= MAX_FAST_BUFS)
                    ? &OverlappedStruct->iolInternalBuffers[0]
                    : OverlappedStruct->iolpInternalBuffers,
                OverlappedStruct->iolInternalBufferCount);
            if (OverlappedStruct->iolUserBufferCount > MAX_FAST_BUFS)
                delete OverlappedStruct->iolpUserBuffers;
            if (OverlappedStruct->iolInternalBufferCount > MAX_FAST_BUFS)
                delete OverlappedStruct->iolpInternalBuffers;
            break;

        case WSP_SEND:
        case WSP_SENDTO:
            if (OverlappedStruct->iolUserBufferCount > MAX_FAST_BUFS)
                delete OverlappedStruct->iolpUserBuffers;
            break;

        case WSP_IOCTL:
            if ((OverlappedStruct->iolIoControlCode==SIO_GET_EXTENSION_FUNCTION_POINTER)
                    && (NO_ERROR==Errno)) {
                Provider->InterceptExtensions (
                                    OverlappedStruct->iolInputBuffer,
                                    OverlappedStruct->iolOutputBuffer,
                                    &Errno);
            }
            break;
        }

        gOverlappedManager->FreeOverlappedStruct (lpOverlapped);


        // If the user requested completion routine notification of the I/O
        // completion queue an APC to the user thread else signal the users
        // event.
        if (UserCompletionRoutine){
            UserOverlappedStruct->InternalHigh = cbTransferred;
            UserOverlappedStruct->Offset = dwError;
            UserOverlappedStruct->OffsetHigh = dwFlags;

            // Changing Internal field to anything other than
            // WSS_OPERATON_IN_PROGRESS indicates that operation
            // is completed and results can be read by WSPGetOverlappedResult,
            // That's why we set this field last (after updating all others)
            // Note that event if address of user completion routine is 
            // equal to WSS_OPERATON_IN_PROGRESS (numericall), we are still
            // safe, because we'll update it again in our completion routine
            // Also, generally if application specifies completion routine,
            // it should not use WSAGetOverlappedResult, so essentially by
            // doing all of this we are just making our code bullet proof
            UserOverlappedStruct->Internal = (DWORD)UserCompletionRoutine;

            gUpCallTable.lpWPUQueueApc(
                &UserThreadId,
                APCProc,
                (DWORD)UserOverlappedStruct,
                &Errno);

        } //if
        else{
            UserOverlappedStruct->Offset = dwError;
            UserOverlappedStruct->OffsetHigh = dwFlags;
                // Internal and InternalHigh fields will be updated
                // by the WPUCompleteOverlappedRequest
            lpWPUCompleteOverlappedRequest (
                Socket,
                UserOverlappedStruct,
                dwError,
                cbTransferred,
                &Errno);
        } //else
    } //if
}



VOID
InitiateOverlappedOperation (
    PINTERNALOVERLAPPEDSTRUCT           OverlappedStruct,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE  CompletionProc,
    LPWSATHREADID                       thread_id
    )
/*++
Routine Description:

    Initiates overlapped operation with provider.

Arguments:

    OverlappedStruct    - packet operation parameters
    CompletionProc      - routine to be invoked upon completion
    thread_id           - id of the thread to which to post APC if any
Return Value:

    NONE
--*/
{
    DWORD                     BytesTransfered;
    INT                       Errno;
    INT                       ReturnCode;

    switch (OverlappedStruct->iolOperationType) {
    case WSP_RECV:
        ReturnCode = OverlappedStruct->iolProvider->WSPRecv(
            OverlappedStruct->iolProviderSocket,
            (OverlappedStruct->iolInternalBufferCount <= MAX_FAST_BUFS)
                ? &OverlappedStruct->iolInternalBuffers[0]
                : OverlappedStruct->iolpInternalBuffers,
            OverlappedStruct->iolInternalBufferCount,
            &BytesTransfered,
            &OverlappedStruct->iolFlags,
            &OverlappedStruct->iolInternalOverlappedStruct,
            CompletionProc,
            thread_id,
            &Errno);
        break;

    case WSP_RECVFROM:
        ReturnCode = OverlappedStruct->iolProvider->WSPRecvFrom(
            OverlappedStruct->iolProviderSocket,
            (OverlappedStruct->iolInternalBufferCount <= MAX_FAST_BUFS)
                ? &OverlappedStruct->iolInternalBuffers[0]
                : OverlappedStruct->iolpInternalBuffers,
            OverlappedStruct->iolInternalBufferCount,
            &BytesTransfered,
            &OverlappedStruct->iolFlags,
            OverlappedStruct->iolSockAddr,
            OverlappedStruct->iolSockAddrLenPtr,
            &OverlappedStruct->iolInternalOverlappedStruct,
            CompletionProc,
            thread_id,
            &Errno);
        break;

    case WSP_SEND:
         ReturnCode = OverlappedStruct->iolProvider->WSPSend(
            OverlappedStruct->iolProviderSocket,
            (OverlappedStruct->iolUserBufferCount <= MAX_FAST_BUFS)
                ? &OverlappedStruct->iolUserBuffers[0]
                : OverlappedStruct->iolpUserBuffers,
            OverlappedStruct->iolUserBufferCount,
            &BytesTransfered,
            OverlappedStruct->iolFlags,
            &OverlappedStruct->iolInternalOverlappedStruct,
            CompletionProc,
            thread_id,
            &Errno);
        break;
    case WSP_SENDTO:
        ReturnCode = OverlappedStruct->iolProvider->WSPSendTo(
            OverlappedStruct->iolProviderSocket,
            (OverlappedStruct->iolUserBufferCount <= MAX_FAST_BUFS)
                ? &OverlappedStruct->iolUserBuffers[0]
                : OverlappedStruct->iolpUserBuffers,
            OverlappedStruct->iolUserBufferCount,
            &BytesTransfered,
            OverlappedStruct->iolFlags,
            OverlappedStruct->iolSockAddr,
            OverlappedStruct->iolSockAddrLen,
            &OverlappedStruct->iolInternalOverlappedStruct,
            CompletionProc,
            thread_id,
            &Errno);
        break;

    case WSP_IOCTL:
        ReturnCode = OverlappedStruct->iolProvider->WSPIoctl(
            OverlappedStruct->iolProviderSocket,
            OverlappedStruct->iolIoControlCode,
            OverlappedStruct->iolInputBuffer,
            OverlappedStruct->iolInputBufferLength,
            OverlappedStruct->iolOutputBuffer,
            OverlappedStruct->iolOutputBufferLength,
            &BytesTransfered,
            &OverlappedStruct->iolInternalOverlappedStruct,
            CompletionProc,
            thread_id,
            &Errno);
        break;
    case ACCEPT_EX:
        ReturnCode = OverlappedStruct->iolProvider->AcceptEx (
            OverlappedStruct->iolListenSocket,
            OverlappedStruct->iolAcceptSocket,
            OverlappedStruct->iolOutputBuffer,
            OverlappedStruct->iolOutputBufferLength,
            OverlappedStruct->iolLocalAddressLength,
            OverlappedStruct->iolRemoteAddressLength,
            &BytesTransfered,
            &OverlappedStruct->iolInternalOverlappedStruct,
            &Errno);
        break;
    case TRANSMIT_FILE:
        ReturnCode = OverlappedStruct->iolProvider->TransmitFile (
            OverlappedStruct->iolProviderSocket,
            OverlappedStruct->iolFileHandle,
            OverlappedStruct->iolBytesToWrite,
            OverlappedStruct->iolBytesPerSend,
            &OverlappedStruct->iolInternalOverlappedStruct,
            ((OverlappedStruct->iolTransmitBuffers.HeadLength!=0)
                    || (OverlappedStruct->iolTransmitBuffers.TailLength!=0))
                ? &OverlappedStruct->iolTransmitBuffers
                : NULL,
            OverlappedStruct->iolReserved,
            &Errno);
        break;
    default:
        assert (FALSE);
        break;
    } //switch

    // Complete ourselves if provider fails right away
    if ((ReturnCode!=NO_ERROR) && (Errno!=WSA_IO_PENDING))
        OverlappedCompletionProc (
            Errno,
            0,
            &OverlappedStruct->iolInternalOverlappedStruct,
            0);

}

DWORD
WorkerThreadProc(
    DWORD  Context
    )
/*++
Routine Description:

    Thread procedure passed to CreatThread().

Arguments:

    Context - Context value passed to CreateThread().  The context value is the
              worker thread object.

Return Value:

    The Return value of the worker thread

--*/

{
    PDWORKERTHREAD            Thread;
    HINSTANCE                 HModule;
    DWORD                     ReturnCode;
    HModule = LoadLibraryA (gLibraryName);


    Thread = (PDWORKERTHREAD) Context;
    ReturnCode = Thread->WorkerThreadProc();
    delete Thread;
    
    DEBUGF( DBG_TRACE,
            ("Exiting worker thread.\n"));
    FreeLibraryAndExitThread (HModule, ReturnCode);
    return 0;   // Keep compiler happy
}





DWORKERTHREAD::DWORKERTHREAD()
/*++
Routine Description:

    Creates any internal state.

Arguments:

    None

Return Value:

    None

--*/

{
    m_exit_thread = FALSE;
    m_thread_count = NULL;
    m_wakeup_semaphore = NULL;
    m_completion_port = NULL;

    InitializeCriticalSection(
        &m_overlapped_operation_queue_lock);

    InitializeListHead(
        &m_overlapped_operation_queue);
}

VOID
DWORKERTHREAD::Destroy (
    )
/*++
Routine Description:

    Initiates destruction of the DWORKERTHREAD object.
    This can't be done syncronously in destructor because this
    will require waiting for thread object to exit which we want
    to avoid.

Arguments:

    NONE

Return Value:

    NONE
--*/
{
    // If we made it through Initialize. Wake up our threads tell them to exit.
    if (m_thread_count>0){
        m_exit_thread = TRUE;
        if (m_completion_port) {
            static OVERLAPPED overlapped;
            PostQueuedCompletionStatus (m_completion_port,
                                    0,
                                    INVALID_SOCKET,
                                    &overlapped
                                    );
        }
        else {
            ReleaseSemaphore (m_wakeup_semaphore, m_thread_count, NULL);
        }
        m_thread_count = 0;
    }
}


DWORKERTHREAD::~DWORKERTHREAD()
/*++
Routine Description:

    destroys any internal state.

Arguments:

    None

Return Value:

    None

--*/
{
    PINTERNALOVERLAPPEDSTRUCT pOLTemp;
    PLIST_ENTRY               ListEntry;
    OSVERSIONINFO             osVersion;
    //only Win98 and NT40 supports HasOverlappedIoCompleted
    BOOL                      bHasOverlappedIoCompleted = FALSE;

    if (m_completion_port) {
        CloseHandle (m_completion_port);
        m_completion_port = NULL;
    }
    else if (m_wakeup_semaphore){
        CloseHandle(m_wakeup_semaphore);
        m_wakeup_semaphore = NULL;
    } //if

    EnterCriticalSection(&m_overlapped_operation_queue_lock);

//We need to make sure an I/O completes before deleting its overlapped structure. 
//Winsock 2 on NT supports HasOverlappedCompleted macro. For NT3.51 and Win9x, here we simply
//call Sleep (100) which is typically safe as all outstanding 
//I/O shall complete shortly after socket handles are closed.
//
//                   Win95     Win98     WinNT 3.51     WinNT 4.0
//                  ------------------------------------------------
//dwPlatFormID         1         1            2              2
//dwMajorVersion       4         4            3              4
//dwMinorVersion       0        10           51              0  

    if (GetVersionEx(&osVersion)) {
        //NT 40 or greater
        if (osVersion.dwMajorVersion >= 4 && osVersion.dwPlatformId == 2 ) 
            bHasOverlappedIoCompleted = TRUE;
    }

    if (bHasOverlappedIoCompleted) {

        while (!IsListEmpty(&m_overlapped_operation_queue)){
            ListEntry = RemoveHeadList(
                &m_overlapped_operation_queue);
            pOLTemp = CONTAINING_RECORD(
                ListEntry,
                INTERNALOVERLAPPEDSTRUCT,
                iolListLinkage);
            while (!HasOverlappedIoCompleted((LPOVERLAPPED)pOLTemp)) Sleep(0);
        } 
    }
    else
        Sleep(100);

    LeaveCriticalSection(&m_overlapped_operation_queue_lock);

    if (gOverlappedManager) {
        delete(gOverlappedManager);
        gOverlappedManager = NULL;
    }

    DeleteCriticalSection(
        &m_overlapped_operation_queue_lock);


    DEBUGF( DBG_TRACE,
            ("Destroyed worker thread object\n"));
}



INT
DWORKERTHREAD::Initialize(

    )
/*++
Routine Description:

    Initializes the DWORKERTHREAD object.

Arguments:

    NONE

Return Value:

    If no error occurs, Initialize() returns NO_ERROR.  Otherwise the value
    SOCKET_ERROR  is  returned,  and  a  specific  error  code  is available in
    lpErrno.

--*/
{
    INT ReturnCode =WSAENOBUFS;
    DWORD           ThreadId;
    HANDLE          hThread;
    SYSTEM_INFO     info;
    DWORD           max_threads;

    DEBUGF( DBG_TRACE,
            ("Initializing worker thread \n"));

    m_completion_port = CreateIoCompletionPort (
                                INVALID_HANDLE_VALUE,
                                NULL,
                                0,
                                0);
    if (m_completion_port!=NULL) {
        ReturnCode = NO_ERROR;
        // Don't need extra worker threads if we use completion
        // port -> IO will be initiated/executed in the context
        // of the original client thread and the worker thread
        // will only be used to receive and redirect completion 
        // notification.
        max_threads = 1;
    }
    else {
        // Create the semaphore we will use to communicat with the worker thread.
        m_wakeup_semaphore = CreateSemaphore(
            NULL,
            0,
            MAXLONG,
            NULL);
        if ( m_wakeup_semaphore){
            GetSystemInfo (&info);
            max_threads = info.dwNumberOfProcessors;
            ReturnCode = NO_ERROR;
        } //if
    } // if


    if (NO_ERROR == ReturnCode){
        gOverlappedManager = new DOVERLAPPEDSTRUCTMGR;
        if (gOverlappedManager!=NULL) {
            ReturnCode = gOverlappedManager->Initialize();
        }
        else
            ReturnCode = WSAENOBUFS;
    }

    //
    // Create the worker threads.
    //
    if (NO_ERROR == ReturnCode){
        for (m_thread_count=0; m_thread_count<max_threads; m_thread_count++) {
            hThread = CreateThread(
                NULL,
                0,
                (LPTHREAD_START_ROUTINE)::WorkerThreadProc,
                this,
                0,
                &ThreadId);
            if (hThread!=NULL)
                CloseHandle (hThread);
            else
                break;
        } // for
        if (m_thread_count>0)
            ReturnCode = NO_ERROR;
        else
            ReturnCode = WSAENOBUFS;

    } //if

    if (NO_ERROR != ReturnCode){
        //Cleanup any resources we may have allocated.
        if (m_thread_count>0){
            m_exit_thread = TRUE;
            ReleaseSemaphore(m_wakeup_semaphore, m_thread_count, NULL);
        } //if

        if (gOverlappedManager) {
            delete gOverlappedManager;
            gOverlappedManager = NULL;
        }

        if (m_wakeup_semaphore) {
            CloseHandle (m_wakeup_semaphore);
            m_wakeup_semaphore = NULL;
        } // if

    } //if

    return(ReturnCode);
} //Initailize




DWORD
DWORKERTHREAD::WorkerThreadProc()
/*++
Routine Description:

    The thread procedure for this object.

Arguments:

    NONE

Return Value:

    NO_ERROR

--*/
{
    INT   Errno;

    gUpCallTable.lpWPUOpenCurrentThread(
        &m_thread_id,
        &Errno);

    while (!m_exit_thread){
        if (m_completion_port) {
            BOOL            result;
            LPOVERLAPPED    lpOverlapped;
			ULONG_PTR           key;
			DWORD cbTransferred;
            result = GetQueuedCompletionStatus (
                        m_completion_port,
                        &cbTransferred,
                        &key,
                        &lpOverlapped,
                        INFINITE
                        );
            if (m_exit_thread) {
                DEBUGF (DBG_TRACE, ("Worker thread received signal to exit.\n"));
                break;
            } // if
            else if ((key!=-1) && (lpOverlapped!=NULL)) {
                OverlappedCompletionProc (
                    WSA_IO_PENDING,
                    cbTransferred,
                    lpOverlapped,
                    0);
            } //else if

        } // if
        else {
            DWORD                       ReturnCode;
            PINTERNALOVERLAPPEDSTRUCT   OverlappedStruct;
            ReturnCode = WaitForSingleObjectEx(
                m_wakeup_semaphore,
                INFINITE,
                TRUE);
            if (m_exit_thread) {
                DEBUGF (DBG_TRACE, ("Worker thread received signal to exit.\n"));
                break;
            } // if
            else if (ReturnCode == WAIT_OBJECT_0){

                // Is there a queued overlapped operation that needs to be
                // initiated.
                OverlappedStruct = NextOverlappedOperation();

                if (OverlappedStruct){
                    InitiateOverlappedOperation (
                        OverlappedStruct,
                        OverlappedCompletionProc,
                        &m_thread_id);
                } //if
            } // else if
        } //else
    } //while

    gUpCallTable.lpWPUCloseThread(
        &m_thread_id,
        &Errno);

    return(NO_ERROR);
}

INT
DWORKERTHREAD::QueueOverlappedRecv(
    PDSOCKET                           Socket,
    LPWSABUF                           UserBuffers,
    DWORD                              UserBufferCount,
    LPDWORD                            UserBytesRecvd,
    LPDWORD                            UserFlags,
    LPWSAOVERLAPPED                    UserOverlappedStruct,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE UserCompletionRoutine,
    LPWSATHREADID                      UserThreadId,
    LPWSABUF                           InternalBuffers,
    DWORD                              InternalBufferCount,
    LPINT                              Errno
    )
/*++

Routine Description:

    this routine allocates an internal overlapped structure stores its
    arguments in the allocated structure and enqueues the structure for the
    worker thread to complet the I/O operation.

Arguments:

    Socket      - Socket object

    UserBuffers - The pointer to the user buffer(s).

    UserBufferCount - The number of user buffers.

    UserBytesRecvd - The pointer to the user BytesRecvd parameter.

    UserFlags - A pointer to the user flags argument.

    UserOverlappedStruct - The user overlapped struct pointer.

    UserCompletionRoutine - The user overlapped completion routine.

    UserThreadId - The user thread ID.

    InternalBuffers - A pointer to our internal buffer(s).

    InternalBufferCount - The number of internal buffers.

    Errno - A pointer to the user errno parameter.

Return Value:

    NO_ERROR on success else a valid winsock2 error code.

--*/
{
    INT ReturnCode;

    PINTERNALOVERLAPPEDSTRUCT OverlappedStruct;

    ReturnCode = SOCKET_ERROR;
    *Errno = WSAENOBUFS;

    OverlappedStruct =
        gOverlappedManager->AllocateOverlappedStruct();
    if (OverlappedStruct){
        OverlappedStruct->iolOperationType = WSP_RECV;
        OverlappedStruct->iolSocket = Socket->GetSocketHandle ();
        OverlappedStruct->iolProvider = Socket->GetDProvider();
        OverlappedStruct->iolProviderSocket = Socket->GetProviderSocket ();
        OverlappedStruct->iolUserOverlappedStruct = UserOverlappedStruct;
        OverlappedStruct->iolUserCompletionRoutine = UserCompletionRoutine;
        OverlappedStruct->iolUserThreadId = *UserThreadId;
        if (UserBufferCount<=MAX_FAST_BUFS)
            memcpy (OverlappedStruct->iolUserBuffers, UserBuffers,
                        sizeof (WSABUF)*UserBufferCount);
        else {
            OverlappedStruct->iolpUserBuffers = new WSABUF[UserBufferCount];
            if (OverlappedStruct->iolpUserBuffers==NULL) {
                gOverlappedManager->FreeOverlappedStruct (
                        &OverlappedStruct->iolInternalOverlappedStruct);
                return ReturnCode;
            }
            memcpy (OverlappedStruct->iolpUserBuffers, UserBuffers,
                        sizeof (WSABUF)*UserBufferCount);
        }
        OverlappedStruct->iolUserBufferCount = UserBufferCount;
        if (InternalBufferCount<=MAX_FAST_BUFS)
            memcpy (OverlappedStruct->iolInternalBuffers, InternalBuffers,
                        sizeof (WSABUF)*InternalBufferCount);
        else {
            OverlappedStruct->iolpInternalBuffers = new WSABUF[InternalBufferCount];
            if (OverlappedStruct->iolpInternalBuffers==NULL) {
                if (UserBufferCount>MAX_FAST_BUFS)
                    delete OverlappedStruct->iolpUserBuffers;
                gOverlappedManager->FreeOverlappedStruct (
                        &OverlappedStruct->iolInternalOverlappedStruct);
                return ReturnCode;
            }
            memcpy (OverlappedStruct->iolpInternalBuffers, InternalBuffers,
                        sizeof (WSABUF)*InternalBufferCount);
        }
        OverlappedStruct->iolInternalBufferCount = InternalBufferCount;
        OverlappedStruct->iolFlags = *UserFlags;
        AddOverlappedOperation(
            Socket,
            OverlappedStruct);
        *Errno = WSA_IO_PENDING;

    } //if
    return(ReturnCode);
}


INT
DWORKERTHREAD::QueueOverlappedRecvFrom(
    PDSOCKET                           Socket,
    LPWSABUF                           UserBuffers,
    DWORD                              UserBufferCount,
    LPDWORD                            UserBytesRecvd,
    LPDWORD                            UserFlags,
    struct sockaddr FAR *              UserFrom,
    LPINT                              UserFromLen,
    LPWSAOVERLAPPED                    UserOverlappedStruct,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE UserCompletionRoutine,
    LPWSATHREADID                      UserThreadId,
    LPWSABUF                           InternalBuffers,
    DWORD                              InternalBufferCount,
    LPINT                              Errno
    )
/*++

Routine Description:

    this routine allocates an internal overlapped structure stores its
    arguments in the allocated structure and enqueues the structure for the
    worker thread to complet the I/O operation.

Arguments:

    Socket      - Socket object

    UserBuffers - The pointer to the user buffer(s).

    UserBufferCount - The number of user buffers.

    UserBytesRecvd - The pointer to the user BytesRecvd parameter.

    UserFlags - A pointer to the user flags argument.

    UserFrom  - A pinter to the user sockaddr structure,

    UserFromLen - A pointer to the length of UserFrom.

    UserOverlappedStruct - The user overlapped struct pointer.

    UserCompletionRoutine - The user overlapped completion routine.

    UserThreadId - The user thread ID.

    InternalBuffers - A pointer to our internal buffer(s).

    InternalBufferCount - The number of internal buffers.

    Errno - A pointer to the user errno parameter.

Return Value:

    NO_ERROR on success else a valid winsock2 error code.

--*/
{
    INT                       ReturnCode;
    PINTERNALOVERLAPPEDSTRUCT OverlappedStruct;

    ReturnCode = SOCKET_ERROR;
    *Errno = WSAENOBUFS;

    OverlappedStruct =
        gOverlappedManager->AllocateOverlappedStruct();
    if (OverlappedStruct){
        OverlappedStruct->iolOperationType = WSP_RECVFROM;
        OverlappedStruct->iolSocket = Socket->GetSocketHandle ();
        OverlappedStruct->iolProvider = Socket->GetDProvider();
        OverlappedStruct->iolProviderSocket = Socket->GetProviderSocket ();
        OverlappedStruct->iolUserOverlappedStruct = UserOverlappedStruct;
        OverlappedStruct->iolUserCompletionRoutine = UserCompletionRoutine;
        OverlappedStruct->iolUserThreadId = *UserThreadId;
        if (UserBufferCount<=MAX_FAST_BUFS)
            memcpy (OverlappedStruct->iolUserBuffers, UserBuffers,
                        sizeof (WSABUF)*UserBufferCount);
        else {
            OverlappedStruct->iolpUserBuffers = new WSABUF[UserBufferCount];
            if (OverlappedStruct->iolpUserBuffers==NULL) {
                gOverlappedManager->FreeOverlappedStruct (
                        &OverlappedStruct->iolInternalOverlappedStruct);
                return ReturnCode;
            }
            memcpy (OverlappedStruct->iolpUserBuffers, UserBuffers,
                        sizeof (WSABUF)*UserBufferCount);
        }
        OverlappedStruct->iolUserBufferCount = UserBufferCount;
        if (InternalBufferCount<=MAX_FAST_BUFS)
            memcpy (OverlappedStruct->iolInternalBuffers, InternalBuffers,
                        sizeof (WSABUF)*InternalBufferCount);
        else {
            OverlappedStruct->iolpInternalBuffers = new WSABUF[InternalBufferCount];
            if (OverlappedStruct->iolpInternalBuffers==NULL) {
                if (UserBufferCount>MAX_FAST_BUFS)
                    delete OverlappedStruct->iolpUserBuffers;
                gOverlappedManager->FreeOverlappedStruct (
                        &OverlappedStruct->iolInternalOverlappedStruct);
                return ReturnCode;
            }
            memcpy (OverlappedStruct->iolpInternalBuffers, InternalBuffers,
                        sizeof (WSABUF)*InternalBufferCount);
        }
        OverlappedStruct->iolInternalBufferCount = InternalBufferCount;
        OverlappedStruct->iolFlags = *UserFlags;
        OverlappedStruct->iolSockAddr = UserFrom;
        OverlappedStruct->iolSockAddrLenPtr = UserFromLen;

        AddOverlappedOperation(
            Socket,
            OverlappedStruct);
        *Errno = WSA_IO_PENDING;

    } //if
    return(ReturnCode);
}

INT
DWORKERTHREAD::QueueOverlappedSend(
    PDSOCKET                           Socket,
    LPWSABUF                           UserBuffers,
    DWORD                              UserBufferCount,
    LPDWORD                            UserBytesSent,
    DWORD                              UserFlags,
    LPWSAOVERLAPPED                    UserOverlappedStruct,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE UserCompletionRoutine,
    LPWSATHREADID                      UserThreadId,
    LPINT           Errno
    )
/*++

Routine Description:

    this routine allocates an internal overlapped structure stores its
    arguments in the allocated structure and enqueues the structure for the
    worker thread to complet the I/O operation.

Arguments:

    Socket      - Socket object

    UserBuffers - The pointer to the user buffer(s).

    UserBufferCount - The number of user buffers.

    UserBytesSent - The pointer to the user BytesSent parameter.

    UserFlags - The user flags .

    UserOverlappedStruct - The user overlapped struct pointer.

    UserCompletionRoutine - The user overlapped completion routine.

    UserThreadId - The user thread ID.

    InternalBuffers - A pointer to our internal buffer(s).

    InternalBufferCount - The number of internal buffers.

    Errno - A pointer to the user errno parameter.

Return Value:

    NO_ERROR on success else a valid winsock2 error code.

--*/
{
    INT                       ReturnCode;
    PINTERNALOVERLAPPEDSTRUCT OverlappedStruct;

    ReturnCode = SOCKET_ERROR;
    *Errno = WSAENOBUFS;

    OverlappedStruct =
        gOverlappedManager->AllocateOverlappedStruct();
    if (OverlappedStruct){
        OverlappedStruct->iolOperationType = WSP_SEND;
        OverlappedStruct->iolSocket = Socket->GetSocketHandle ();
        OverlappedStruct->iolProvider = Socket->GetDProvider();
        OverlappedStruct->iolProviderSocket = Socket->GetProviderSocket ();
        OverlappedStruct->iolUserOverlappedStruct = UserOverlappedStruct;
        OverlappedStruct->iolUserCompletionRoutine = UserCompletionRoutine;
        OverlappedStruct->iolUserThreadId = *UserThreadId;
        if (UserBufferCount<=MAX_FAST_BUFS)
            memcpy (OverlappedStruct->iolUserBuffers, UserBuffers,
                        sizeof (WSABUF)*UserBufferCount);
        else {
            OverlappedStruct->iolpUserBuffers = new WSABUF[UserBufferCount];
            if (OverlappedStruct->iolpUserBuffers==NULL) {
                gOverlappedManager->FreeOverlappedStruct (
                        &OverlappedStruct->iolInternalOverlappedStruct);
                return ReturnCode;
            }
            memcpy (OverlappedStruct->iolpUserBuffers, UserBuffers,
                        sizeof (WSABUF)*UserBufferCount);
        }
        OverlappedStruct->iolUserBufferCount = UserBufferCount;
        OverlappedStruct->iolFlags = UserFlags;

        AddOverlappedOperation(
            Socket,
            OverlappedStruct);

        *Errno = WSA_IO_PENDING;

    } //if
    return(ReturnCode);
}


INT
DWORKERTHREAD::QueueOverlappedSendTo(
    PDSOCKET                           Socket,
    LPWSABUF                           UserBuffers,
    DWORD                              UserBufferCount,
    LPDWORD                            UserBytesSent,
    DWORD                              UserFlags,
    const struct sockaddr FAR *        UserTo,
    INT                                UserToLen,
    LPWSAOVERLAPPED                    UserOverlappedStruct,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE UserCompletionRoutine,
    LPWSATHREADID                      UserThreadId,
    LPINT                              Errno
    )
/*++

Routine Description:

    this routine allocates an internal overlapped structure stores its
    arguments in the allocated structure and enqueues the structure for the
    worker thread to complet the I/O operation.

Arguments:

    Socket      - Socket object

    UserBuffers - The pointer to the user buffer(s).

    UserBufferCount - The number of user buffers.

    UserBytesRecvd - The pointer to the user BytesRecvd parameter.

    UserFlags - A pointer to the user flags argument.

    UserTo - A pointer to the user sockaddr structure.

    UserToLen - The length of the user sockaddr structure.

    UserOverlappedStruct - The user overlapped struct pointer.

    UserCompletionRoutine - The user overlapped completion routine.

    UserThreadId - The user thread ID.

    InternalBuffers - A pointer to our internal buffer(s).

    InternalBufferCount - The number of internal buffers.

    Errno - A pointer to the user errno parameter.

Return Value:

    NO_ERROR on success else a valid winsock2 error code.

--*/
{
    INT                       ReturnCode;
    PINTERNALOVERLAPPEDSTRUCT OverlappedStruct;

    ReturnCode = SOCKET_ERROR;
    *Errno = WSAENOBUFS;

    OverlappedStruct =
        gOverlappedManager->AllocateOverlappedStruct();
    if (OverlappedStruct){
        OverlappedStruct->iolOperationType = WSP_SENDTO;
        OverlappedStruct->iolSocket = Socket->GetSocketHandle ();
        OverlappedStruct->iolProvider = Socket->GetDProvider();
        OverlappedStruct->iolProviderSocket = Socket->GetProviderSocket ();
        OverlappedStruct->iolUserOverlappedStruct = UserOverlappedStruct;
        OverlappedStruct->iolUserCompletionRoutine = UserCompletionRoutine;
        OverlappedStruct->iolUserThreadId = *UserThreadId;
        if (UserBufferCount<=MAX_FAST_BUFS)
            memcpy (OverlappedStruct->iolUserBuffers, UserBuffers,
                        sizeof (WSABUF)*UserBufferCount);
        else {
            OverlappedStruct->iolpUserBuffers = new WSABUF[UserBufferCount];
            if (OverlappedStruct->iolpUserBuffers==NULL) {
                gOverlappedManager->FreeOverlappedStruct (
                        &OverlappedStruct->iolInternalOverlappedStruct);
                return ReturnCode;
            }
            memcpy (OverlappedStruct->iolpUserBuffers, UserBuffers,
                        sizeof (WSABUF)*UserBufferCount);
        }
        OverlappedStruct->iolUserBufferCount = UserBufferCount;
        OverlappedStruct->iolFlags = UserFlags;
        OverlappedStruct->iolSockAddr = (struct sockaddr FAR *)UserTo;
        OverlappedStruct->iolSockAddrLen = UserToLen;

        AddOverlappedOperation(
            Socket,
            OverlappedStruct);

        *Errno = WSA_IO_PENDING;
    } //if
    return(ReturnCode);
}

INT
DWORKERTHREAD::QueueOverlappedIoctl(
    PDSOCKET                           Socket,
    DWORD                              dwIoControlCode,
    LPVOID                             lpvInBuffer,
    DWORD                              cbInBuffer,
    LPVOID                             lpvOutBuffer,
    DWORD                              cbOutBuffer,
    LPDWORD                            lpcbBytesReturned,
    LPWSAOVERLAPPED                    UserOverlappedStruct,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE UserCompletionRoutine,
    LPWSATHREADID                      UserThreadId,
    LPINT                              Errno
    )
/*++

Routine Description:

    this routine allocates an internal overlapped structure stores its
    arguments in the allocated structure and enqueues the structure for the
    worker thread to complet the I/O operation.

Arguments:

    Socket      - Socket object

    UserBuffers - The pointer to the user buffer(s).

    UserBufferCount - The number of user buffers.

    UserBytesRecvd - The pointer to the user BytesRecvd parameter.

    UserFlags - A pointer to the user flags argument.

    UserTo - A pointer to the user sockaddr structure.

    UserToLen - The length of the user sockaddr structure.

    UserOverlappedStruct - The user overlapped struct pointer.

    UserCompletionRoutine - The user overlapped completion routine.

    UserThreadId - The user thread ID.

    InternalBuffers - A pointer to our internal buffer(s).

    InternalBufferCount - The number of internal buffers.

    Errno - A pointer to the user errno parameter.

Return Value:

    NO_ERROR on success else a valid winsock2 error code.

--*/
{
    INT                       ReturnCode;
    PINTERNALOVERLAPPEDSTRUCT OverlappedStruct;

    ReturnCode = SOCKET_ERROR;
    *Errno = WSAENOBUFS;

    OverlappedStruct =
        gOverlappedManager->AllocateOverlappedStruct();
    if (OverlappedStruct){
        OverlappedStruct->iolOperationType = WSP_IOCTL;
        OverlappedStruct->iolSocket = Socket->GetSocketHandle ();
        OverlappedStruct->iolProvider = Socket->GetDProvider();
        OverlappedStruct->iolProviderSocket = Socket->GetProviderSocket ();
        OverlappedStruct->iolUserOverlappedStruct = UserOverlappedStruct;
        OverlappedStruct->iolUserCompletionRoutine = UserCompletionRoutine;
        OverlappedStruct->iolUserThreadId = *UserThreadId;
        OverlappedStruct->iolInputBuffer = lpvInBuffer;
        OverlappedStruct->iolInputBufferLength = cbInBuffer;
        OverlappedStruct->iolOutputBuffer = lpvOutBuffer;
        OverlappedStruct->iolOutputBufferLength = cbOutBuffer;
        OverlappedStruct->iolIoControlCode = dwIoControlCode;

        AddOverlappedOperation(
            Socket,
            OverlappedStruct);

        *Errno = WSA_IO_PENDING;
    } //if
    return(ReturnCode);
}



INT
DWORKERTHREAD::QueueOverlappedAcceptEx(
    PDSOCKET                           ListenSocket,
    PDSOCKET                           AcceptSocket,
    LPVOID                             lpOutputBuffer,
    DWORD                              dwReceiveDataLength,
    DWORD                              dwLocalAddressLength,
    DWORD                              dwRemoteAddressLength,
    LPWSAOVERLAPPED                    UserOverlappedStruct,
    LPINT                              Errno
    ) {
    INT                       ReturnCode;
    PINTERNALOVERLAPPEDSTRUCT OverlappedStruct;

    ReturnCode = SOCKET_ERROR;
    *Errno = WSAENOBUFS;

    OverlappedStruct =
        gOverlappedManager->AllocateOverlappedStruct();
    if (OverlappedStruct){
        OverlappedStruct->iolOperationType = ACCEPT_EX;
        OverlappedStruct->iolSocket = ListenSocket->GetSocketHandle ();
        OverlappedStruct->iolProvider = ListenSocket->GetDProvider();
        OverlappedStruct->iolListenSocket = ListenSocket->GetProviderSocket ();
        OverlappedStruct->iolAcceptSocket = AcceptSocket->GetProviderSocket ();
        OverlappedStruct->iolUserOverlappedStruct = UserOverlappedStruct;
        OverlappedStruct->iolUserCompletionRoutine = NULL;
        OverlappedStruct->iolOutputBuffer = lpOutputBuffer;
        OverlappedStruct->iolOutputBufferLength = dwReceiveDataLength;
        OverlappedStruct->iolLocalAddressLength = dwLocalAddressLength;
        OverlappedStruct->iolRemoteAddressLength = dwRemoteAddressLength;

        AddOverlappedOperation(
            ListenSocket,
            OverlappedStruct);

        *Errno = WSA_IO_PENDING;
    } //if
    return(ReturnCode);
}


INT
DWORKERTHREAD::QueueOverlappedTransmitFile(
    PDSOCKET                            Socket,
    HANDLE                              hFile,
    DWORD                               nNumberOfBytesToWrite,
    DWORD                               nNumberOfBytesPerSend,
    LPWSAOVERLAPPED                     UserOverlappedStruct,
    LPTRANSMIT_FILE_BUFFERS             lpTransmitBuffers,
    DWORD                               dwReserved,
    LPINT                               Errno
    )
{
    INT                       ReturnCode;
    PINTERNALOVERLAPPEDSTRUCT OverlappedStruct;

    ReturnCode = SOCKET_ERROR;
    *Errno = WSAENOBUFS;

    OverlappedStruct =
        gOverlappedManager->AllocateOverlappedStruct();
    if (OverlappedStruct){
        OverlappedStruct->iolOperationType = TRANSMIT_FILE;
        OverlappedStruct->iolSocket = Socket->GetSocketHandle ();
        OverlappedStruct->iolProvider = Socket->GetDProvider();
        OverlappedStruct->iolProviderSocket = Socket->GetProviderSocket ();
        OverlappedStruct->iolUserOverlappedStruct = UserOverlappedStruct;
        OverlappedStruct->iolUserCompletionRoutine = NULL;
        OverlappedStruct->iolFileHandle = hFile;
        OverlappedStruct->iolBytesToWrite = nNumberOfBytesToWrite;
        OverlappedStruct->iolBytesPerSend = nNumberOfBytesPerSend;
        if (lpTransmitBuffers)
            OverlappedStruct->iolTransmitBuffers = *lpTransmitBuffers;
        else
            OverlappedStruct->iolTransmitBuffers.HeadLength =
                OverlappedStruct->iolTransmitBuffers.TailLength = 0;

        OverlappedStruct->iolReserved = dwReserved;
        AddOverlappedOperation(
            Socket,
            OverlappedStruct);

        *Errno = WSA_IO_PENDING;
    } //if
    return(ReturnCode);
}

VOID
DWORKERTHREAD::AddOverlappedOperation(
    IN PDSOCKET                  Socket,
    IN PINTERNALOVERLAPPEDSTRUCT OverlappedOperation
    )
/*++
Routine Description:

    This routine adds an internal overlapped structure to the queue of requests
    to be completed by the worker thread.

Arguments:

    Socket              - A pointer to socket object
    OverlappedOperation - A pointer to an internal overlapped structure that
                          describes the operation to be performed by the worker
                          thread.

Return Value:

    NONE
--*/
{

    // If we are using completion ports or user requested
    // completion via APC, initiate operation in the
    // context of the current thread
    if (m_completion_port!=NULL) {
        if (Socket->GetCompletionContext ()==INVALID_SOCKET) {
            CreateIoCompletionPort (
                (HANDLE)OverlappedOperation->iolProviderSocket,
                m_completion_port,
                OverlappedOperation->iolSocket,
                0);
            Socket->SetCompletionContext (OverlappedOperation->iolSocket);
        }
        InitiateOverlappedOperation (
            OverlappedOperation,
            NULL,
            &OverlappedOperation->iolUserThreadId);
    }
    else {
        EnterCriticalSection(&m_overlapped_operation_queue_lock);
        InsertTailList(
            &m_overlapped_operation_queue,
            &OverlappedOperation->iolListLinkage);
        LeaveCriticalSection(&m_overlapped_operation_queue_lock);
        ReleaseSemaphore (m_wakeup_semaphore, 1, NULL);
    }
}


PINTERNALOVERLAPPEDSTRUCT
DWORKERTHREAD::NextOverlappedOperation()
/*++
Routine Description:

    This routine returns the first internal overlapped structure from the queue
    of requests to be completed by the worker thread.

Arguments:

    NONE

Return Value:

    A pointer to an internal overlapped structure or NULL

--*/
{
    PINTERNALOVERLAPPEDSTRUCT ReturnValue;
    PLIST_ENTRY               ListEntry;

    ReturnValue = NULL;

    EnterCriticalSection(&m_overlapped_operation_queue_lock);

    if (!IsListEmpty(&m_overlapped_operation_queue)){
        ListEntry = RemoveHeadList(
            &m_overlapped_operation_queue);
        ReturnValue = CONTAINING_RECORD(
            ListEntry,
            INTERNALOVERLAPPEDSTRUCT,
            iolListLinkage);
    } //if

    LeaveCriticalSection(&m_overlapped_operation_queue_lock);
    return(ReturnValue);
}


