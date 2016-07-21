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

    dcatalog.cpp

Abstract:

    This module contains the implementation of the dcatalog class. This class
    maintains a catalog of installed WinSock2 service providers.

--*/

#include "precomp.h"
#include "install.h"


DCATALOG::DCATALOG()
/*++

Routine Description:

    Constructor for the DCATALOG object. Set member variables to known
    state. Initialization of the object is completed in Initialize().

Arguments:

    NONE.

Return Value:

    NONE.

--*/
{

    m_num_items = 0;
    m_local_item = NULL;

    // initialize the critical section object
    InitializeCriticalSection( &m_catalog_lock );
    InitializeListHead( &m_protocol_list );
}

INT
DCATALOG::Initialize(
    )
/*++

Routine Description:

    Initialization routine for the DCATALOG object. Completes the
    initialization of the DCATALOG object.  This MUST be the first member
    fuction called after a DCATALOG object is created.

Arguments:

    NONE.

Return Value:

    NO_ERROR if the fuction succeeds else a winsock2 error code.

--*/
{
    LPWSAPROTOCOL_INFOW   ProtocolInfoBuff = NULL;
    DWORD                ProtocolInfoBuffSize = 0;
    PPROTO_CATALOG_ITEM  CatalogItem;
    INT                  ReturnCode;
    INT                  ErrorCode;
    INT                  EnumResult;
    INT                  Index;

    // Call WSCEnumProtocols with a zero length buffer so we know what size to
    // send in to get all the installed PROTOCOL_INFO structs.
    WSCEnumProtocols(
        NULL,                     // lpiProtocols
        ProtocolInfoBuff,         // lpProtocolBuffer
        & ProtocolInfoBuffSize,   // lpdwBufferLength
        & ErrorCode);             // lpErrno

    ReturnCode = WSA_NOT_ENOUGH_MEMORY;
    ProtocolInfoBuff = (LPWSAPROTOCOL_INFOW)new char[ProtocolInfoBuffSize];
    if (ProtocolInfoBuff){
        EnumResult = WSCEnumProtocols(
            NULL,                     // lpiProtocols
            ProtocolInfoBuff,         // lpProtocolBuffer
            & ProtocolInfoBuffSize,   // lpdwBufferLength
            & ErrorCode);

        ReturnCode = WSASYSNOTREADY;
        if (EnumResult != SOCKET_ERROR){
            for (Index=0; Index < EnumResult ; Index++){

                //Create a new catalog item for the PROTOCOL_INFO struct.
                CatalogItem = new PROTO_CATALOG_ITEM;
                if (CatalogItem){

                    ReturnCode = CatalogItem->Initialize(
                        &ProtocolInfoBuff[Index]);
                    if (NO_ERROR == ReturnCode){

                        //Add the new catalog item to the catalog
                        AcquireCatalogLock();
                        AppendCatalogItem(
                            CatalogItem);
                        if (memcmp (&CatalogItem->GetProtocolInfo()->ProviderId,
                                &LayeredProviderGuid,
                                sizeof (GUID))==0)
                            m_local_item = CatalogItem;
                        ReleaseCatalogLock();
                    } //if
                    else{
                        break;
                    } //else
                } //if
            } //for

            if ((NO_ERROR==ReturnCode)
                    && (m_local_item==NULL))
                ReturnCode = WSASYSNOTREADY;
        } //if
        delete(ProtocolInfoBuff);
    } //if
    return(ReturnCode);
}




DCATALOG::~DCATALOG()
/*++

Routine Description:

    This  function  destroys the catalog object.  It takes care of removing and
    destroying  all  of  the  catalog  entries  in  the catalog.  This includes
    destroying  all  of the DPROVIDER objects referenced by the catalog.  It is
    the  caller's responsibility to make sure that the DPROVIDER objects are no
    longer referenced.

Arguments:

    None

Return Value:

    None

Implementation Notes:

    for each catalog entry
        remove the entry
        get its DPROVIDER reference
        if reference is non-null
            Set providers for all entries with matching IDs null
            destroy the DPROVIDER
        endif
        destroy the entry
    end for
    deallocate the list head
    close the catalog registry mutex
--*/
{
    PLIST_ENTRY  this_linkage;
    PPROTO_CATALOG_ITEM  this_item;
    PDPROVIDER  this_provider;

    DEBUGF(
        DBG_TRACE,
        ("Catalog destructor\n"));

    AcquireCatalogLock();

    while ((this_linkage = m_protocol_list.Flink) != & m_protocol_list) {
        this_item = CONTAINING_RECORD(
            this_linkage,        // address
            PROTO_CATALOG_ITEM,  // type
            m_CatalogLinkage     // field
            );
        RemoveCatalogItem(
            this_item  // CatalogItem
            );
        this_provider = this_item->GetProvider();
        if (this_provider)
            delete this_provider;
        delete this_item;
    }  // while (get entry linkage)

    assert( m_num_items == 0 );

    ReleaseCatalogLock();
    DeleteCriticalSection( &m_catalog_lock );

}  // ~DCATALOG




VOID
DCATALOG::EnumerateCatalogItems(
    IN CATALOGITERATION  Iteration,
    IN DWORD             PassBack
    )
/*++

Routine Description:

    This  procedure enumerates all of the DPROTO_CATALOG_ITEM structures in the
    catalog  by  calling  the indicated iteration procedure once for each item.
    The called procedure can stop the iteration early by returning FALSE.

    Note  that  the DPROVIDER associated with an enumerated DPROTO_CATALOG_ITEM
    may  be  NULL.   To retrieve DPROTO_CATALOG_ITEM structure that has had its
    DPROVIDER      loaded      and      initialized,      you      can      use
    GetCatalogItemFromCatalogEntryId.

Arguments:

    Iteration - Supplies   a  reference  to  the  catalog  iteration  procedure
                supplied by the client.

    PassBack  - Supplies  a  value uninterpreted by this procedure.  This value
                is  passed  unmodified to the catalog iteration procedure.  The
                client can use this value to carry context between the original
                call site and the iteration procedure.

Return Value:

    None
--*/
{
    PLIST_ENTRY         ListMember;
    PPROTO_CATALOG_ITEM CatalogEntry;
    BOOL                enumerate_more;

    assert(Iteration != NULL);

    enumerate_more = TRUE;

    AcquireCatalogLock();

    ListMember = m_protocol_list.Flink;

    while (enumerate_more && (ListMember != & m_protocol_list)) {
        CatalogEntry = CONTAINING_RECORD(
            ListMember,
            PROTO_CATALOG_ITEM,
            m_CatalogLinkage);
        ListMember = ListMember->Flink;
        enumerate_more = (* Iteration) (
            PassBack,     // PassBack
            CatalogEntry  // CatalogEntry
            );
    } //while

    ReleaseCatalogLock();

}  // EnumerateCatalogItems



INT
DCATALOG::FindNextProviderInChain(
        IN  LPWSAPROTOCOL_INFOW lpProtocolInfo,
        OUT PDPROVIDER          *NextProvider,
        OUT PPROTO_CATALOG_ITEM *BaseProviderCatalogEntry
    )
/*++

Routine Description:
    
    This procedure finds and loads the provider below this
    provider in the protocol chain.

Arguments:
    lpProtocolInfo - A pointer to the WSAPROTOCOL_INFO struct for the
                          current protocol chain.

    NextProvider        - A pointer to DPROVIDER object pointer for the
                          next provider in chain
    BaseProviderCatalogEntry - If next provider is a base provider, this
                          pointer will contain the next provider catalog
                          entry
Return Value:

    NO_ERROR if the next provider is located else a valid winsock2 error
    code.
--*/
{

    PLIST_ENTRY         ListMember;
    PPROTO_CATALOG_ITEM CatalogEntry;
    PPROTO_CATALOG_ITEM NextProviderCatalogEntry;
    DWORD               LocalCatalogEntryId;
    DWORD               NextProviderCatalogEntryId;
    INT                 Index;
    INT                 ReturnCode =WSASYSNOTREADY;

    assert (NextProvider!=NULL);
    assert (BaseProviderCatalogEntry!=NULL);

    *NextProvider = NULL;
    *BaseProviderCatalogEntry = NULL;
    
    if (m_local_item==NULL)
        return WSASYSNOTREADY;

    LocalCatalogEntryId = m_local_item->GetProtocolInfo()->dwCatalogEntryId;

    AcquireCatalogLock();

    // First try to see if we have processed this chain already and
    // thus loaded the provider that follows us in it
    ListMember = m_protocol_list.Flink;

    while (ListMember != & m_protocol_list) {

        CatalogEntry = CONTAINING_RECORD(
            ListMember,
            PROTO_CATALOG_ITEM,
            m_CatalogLinkage);
        ListMember = ListMember->Flink;
        if (CatalogEntry->GetProtocolInfo()->dwCatalogEntryId ==
                    lpProtocolInfo->dwCatalogEntryId) {
            *NextProvider = CatalogEntry->GetProvider ();
            if (*NextProvider!=NULL) {
                 ReturnCode = NO_ERROR;
            }
            else {
                //
                // Get the next providers CatalogEntryId from the protocol chain
                //
                for (Index=0;
                     Index < lpProtocolInfo->ProtocolChain.ChainLen;
                     Index++){
                    if ((LocalCatalogEntryId==
                            lpProtocolInfo->ProtocolChain.ChainEntries[Index])
                            && (lpProtocolInfo->ProtocolChain.ChainLen>Index+1)) {
                        NextProviderCatalogEntryId =
                            lpProtocolInfo->ProtocolChain.ChainEntries[Index+1];
                        break;
                    } //if
                } // for
                //
                // If we found ourselves before reaching the end of the chain,
                // go load the next guy in the chain
                //
                if (Index<lpProtocolInfo->ProtocolChain.ChainLen) {
                    ReturnCode = GetCatalogItemFromCatalogEntryId (
                            NextProviderCatalogEntryId,
                            &NextProviderCatalogEntry);
                    if (NO_ERROR==ReturnCode) {
                        // Pass the chain protocol info if
                        // the provider we are loading is not
                        // the last in the chain (base provider)
                        if (Index + 1 ==lpProtocolInfo->ProtocolChain.ChainLen-1)
                            ReturnCode = LoadProvider (
                                NextProviderCatalogEntry,
                                    NextProviderCatalogEntry->GetProtocolInfo(),
                                    NextProvider);
                        else
                            ReturnCode = LoadProvider (
                                NextProviderCatalogEntry,
                                    lpProtocolInfo,
                                    NextProvider);

                        // Cache provider if we succeded
                        if (NO_ERROR==ReturnCode) {
                            CatalogEntry->SetProvider (*NextProvider);
                            CatalogEntry->SetProviderCatalogEntry (NextProviderCatalogEntry);
                        }
                    } //if
                }
            }
            if ((NO_ERROR==ReturnCode)
                    && (CatalogEntry->GetProviderCatalogEntry()->GetProtocolInfo()->ProtocolChain.ChainLen==BASE_PROTOCOL))
                *BaseProviderCatalogEntry = CatalogEntry->GetProviderCatalogEntry();
                
            break;
        }
    }

    ReleaseCatalogLock();
    return(ReturnCode);
}



INT
DCATALOG::GetCatalogItemFromCatalogEntryId(
    IN  DWORD                     CatalogEntryId,
    OUT PPROTO_CATALOG_ITEM FAR * CatalogItem
    )
/*++

Routine Description:

    This  procedure  retrieves  a  reference  to a catalog item given a catalog
    entry ID to search for.


Arguments:

    CatalogEntryId  - Supplies The ID of a catalog entry to be searched for.

    CatalogItem     - Returns a reference to the catalog item with the matching
                      catalog entry ID if it is found, otherwise returns NULL.

Return Value:

  The  function  returns  NO_ERROR  if  successful, otherwise it returns an
  appropriate WinSock error code.
--*/
{
    PLIST_ENTRY         ListMember;
    INT                 ReturnCode=WSASYSNOTREADY;
    PPROTO_CATALOG_ITEM CatalogEntry;

    assert(CatalogItem != NULL);

    // Prepare for early error return
    * CatalogItem = NULL;

    AcquireCatalogLock();

    ListMember = m_protocol_list.Flink;

    while (ListMember != & m_protocol_list) {

        CatalogEntry = CONTAINING_RECORD(
            ListMember,
            PROTO_CATALOG_ITEM,
            m_CatalogLinkage);
        ListMember = ListMember->Flink;
        if (CatalogEntry->GetProtocolInfo()->dwCatalogEntryId ==
                CatalogEntryId) {
            * CatalogItem = CatalogEntry;
            ReturnCode = NO_ERROR;
            break;
        } //if
    } //while

    ReleaseCatalogLock();
    return(ReturnCode);
}  // GetCatalogItemFromCatalogEntryId



INT
DCATALOG::LoadProvider(
    IN PPROTO_CATALOG_ITEM CatalogEntry,
    IN LPWSAPROTOCOL_INFOW lpProtocolInfo,
    OUT PDPROVIDER         *Provider
    )
/*++

Routine Description:

    Load   the   provider  described  by  CatalogEntry.

Arguments:

    CatalogEntry - Supplies  a reference to a protocol catalog entry describing
                   the provider to load.

    lpProtocolInfo - Protocol info structure to pass to provider when
                    loading it

    Provider     - Returns a reference to the newly loaded provider object.

Return Value:

    The  function  returns NO_ERROR if successful, otherwise it returns an
    appropriate WinSock error code.
--*/
{
    INT ReturnCode = WSA_NOT_ENOUGH_MEMORY;
    PDPROVIDER LocalProvider;

    assert(CatalogEntry != NULL);
    assert(Provider != NULL);

    *Provider = NULL;

    LocalProvider = new(DPROVIDER);
    if (LocalProvider) {

        ReturnCode = LocalProvider->Initialize(
            CatalogEntry->GetLibraryPath(),
            lpProtocolInfo
            );
        if (NO_ERROR == ReturnCode) {
            *Provider = LocalProvider;
        } //if
        else {
            delete(LocalProvider);
        } //else
    } //if
    return(ReturnCode);
}  // LoadProvider



VOID
DCATALOG::AppendCatalogItem(
    IN  PPROTO_CATALOG_ITEM  CatalogItem
    )
/*++

Routine Description:

    This procedure appends a catalog item to the end of the (in-memory) catalog
    object.   It becomes the last item in the catalog.

Arguments:

    CatalogItem - Supplies a reference to the catalog item to be added.

Return Value:

    None
--*/
{
    assert(CatalogItem != NULL);

    InsertTailList(
        & m_protocol_list,               // ListHead
        & CatalogItem->m_CatalogLinkage  // Entry
       );
    m_num_items++;
}  // AppendCatalogItem



VOID
DCATALOG::RemoveCatalogItem(
    IN  PPROTO_CATALOG_ITEM  CatalogItem
    )
/*++

Routine Description:

    This  procedure removes a catalog item from the (in-memory) catalog object.
    The catalog information in the registry is NOT updated.

Arguments:

    CatalogItem - Supplies a reference to the catalog item to be removed.

Return Value:

    None
--*/
{
    assert(CatalogItem != NULL);

    RemoveEntryList(
        & CatalogItem->m_CatalogLinkage  // Entry
        );
    assert(m_num_items > 0);
    m_num_items--;
}  // RemoveCatalogItem
