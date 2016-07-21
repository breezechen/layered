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

doverlap.h

Abstract:

    This module defines the DOVERLAPPEDSTRUCTMGR class.  This class
    maintains a pool of internal overlapped structures.  These structures
    are used to store the information the layered service provider will
    need to satisfy users overlapped I/O requests.

--*/

#ifndef _DOVERLAPPEDSTRUCTMGR_
#define _DOVERLAPPEDSTRUCTMGR_


#include <winsock2.h>
#include "llist.h"
#include "classfwd.h"
#include "stddef.h"

#define  OUTSTANDINGOVERLAPPEDSTRUCTS 1000
#define  STRUCTSIGNATURE 0xbeadface
// Maximum number of buffers inside the structure (if we have to handle
// more than that, we'll allocate from heap)
#define  MAX_FAST_BUFS 8

// The internal overlapped struct
typedef struct _INTERNALOVERLAPPEDSTRUCT
{
    DWORD                              iolSignature;
    DWORD                              iolOperationType;
    SOCKET                             iolSocket;
    PDPROVIDER                         iolProvider;
    union {
        SOCKET                             iolProviderSocket;
        SOCKET                             iolListenSocket;
    };
    WSATHREADID                        iolUserThreadId;
    LPWSAOVERLAPPED                    iolUserOverlappedStruct;
    LPWSAOVERLAPPED_COMPLETION_ROUTINE iolUserCompletionRoutine;
    union {
        TRANSMIT_FILE_BUFFERS              iolTransmitBuffers;
        struct {
            union {
                WSABUF                             UserBuffers[MAX_FAST_BUFS];
#define iolUserBuffers iolRest.UserBuffers
                LPWSABUF                           pUserBuffers;
#define iolpUserBuffers iolRest.pUserBuffers
                LPVOID                             InputBuffer;
#define iolInputBuffer iolRest.InputBuffer
            };
            union {
                DWORD                              UserBufferCount;
#define iolUserBufferCount iolRest.UserBufferCount
                DWORD                              InputBufferLength;
#define iolInputBufferLength iolRest.InputBufferLength
            };
            union {
                WSABUF                             InternalBuffers[MAX_FAST_BUFS];
#define iolInternalBuffers iolRest.InternalBuffers
                LPWSABUF                           pInternalBuffers;
#define iolpInternalBuffers iolRest.pInternalBuffers
                LPVOID                             OutputBuffer;
#define iolOutputBuffer iolRest.OutputBuffer
            };
            union {
                DWORD                              InternalBufferCount;
#define iolInternalBufferCount iolRest.InternalBufferCount
                DWORD                              OutputBufferLength;
#define iolOutputBufferLength iolRest.OutputBufferLength
            };
        }                                   iolRest;
    };
    union {
        struct sockaddr FAR *              iolSockAddr;
        DWORD                              iolLocalAddressLength;
        DWORD                              iolBytesToWrite;
    };
    union {
        LPINT                              iolSockAddrLenPtr;
        INT                                iolSockAddrLen;
        DWORD                              iolRemoteAddressLength;
        DWORD                              iolBytesPerSend;
    };
    union {
        DWORD                              iolFlags;
        DWORD                              iolIoControlCode;
        SOCKET                             iolAcceptSocket;
        HANDLE                             iolFileHandle;
    };
    DWORD                              iolReserved;
    LIST_ENTRY                         iolListLinkage;
    WSAOVERLAPPED                      iolInternalOverlappedStruct;
} INTERNALOVERLAPPEDSTRUCT, *PINTERNALOVERLAPPEDSTRUCT;

class DOVERLAPPEDSTRUCTMGR
{
  public:

    DOVERLAPPEDSTRUCTMGR();

    INT
    Initialize(
        );

    ~DOVERLAPPEDSTRUCTMGR();

    PINTERNALOVERLAPPEDSTRUCT
    AllocateOverlappedStruct(
        );

    VOID
    FreeOverlappedStruct(
        LPWSAOVERLAPPED   pOverlappedStruct
        );

    PINTERNALOVERLAPPEDSTRUCT
    GetInternalOverlappedStructure (
        LPWSAOVERLAPPED   pOverlappedStruct
        );

  private:

    PINTERNALOVERLAPPEDSTRUCT
    PopOverlappedStruct();

    VOID
    PushOverlappedStruct(
        LPWSAOVERLAPPED OverlappedStruct
        );

    SINGLE_LIST_ENTRY m_overlapped_free_list;
    // A list of available internal overlapped structs.

    CRITICAL_SECTION  m_overlapped_free_list_lock;
    // Syncronization object for updateing m_overlapped_free_list.

    PBYTE             m_overlapped_struct_block;
    // Pointer to the memory block containing the internal overlapped
    // structures.


};   // class DOVERLAPPEDSTRUCTMGR

#endif // _DOVERLAPPEDSTRUCTMGR_
