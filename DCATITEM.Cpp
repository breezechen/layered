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

    dcatitem.cpp

Abstract:

    This  file  contains  the  class  implementation for the PROTO_CATALOG_ITEM
    class.   This  class  defines  the  interface  to  the  entries that can be
    retrieved from the protocol catalog.

--*/


#include "precomp.h"


PROTO_CATALOG_ITEM::PROTO_CATALOG_ITEM()
/*++

Routine Description:

    This  procedure  constructs  an empty PROTO_CATALOG_ITEM object.  The first
    method  invoked  after  this  constructor must be Initialize().

Arguments:

    None

Return Value:

    Implicitly  returns  a pointer to a newly created PROTO_CATALOG_ITEM object
    or NULL if there is a memory allocation failure.
--*/
{
    DEBUGF(
        DBG_TRACE,
        ("PROTO_CATALOG_ITEM constructor\n"));

    // Simply initialize embedded pointer values for safety.
    m_LibraryPath[0] = '\0';
    m_Provider = NULL;
    m_ProviderCatalogEntry = NULL;
}  // PROTO_CATALOG_ITEM





INT
PROTO_CATALOG_ITEM::Initialize(
    IN  LPWSAPROTOCOL_INFOW  ProtoInfo
    )
/*++

Routine Description:

    This  procedure initializes the protocol info part of the catalog item from
    the  values  supplied.  Values are copied from the structures passed by the
    caller,  so  the  caller  is  free  to  deallocate the passed structures on
    return.

Arguments:

    ProtoInfo    - Supplies  a reference to the fully initialized protocol info
                   structure describing this protocol.

return Value:

    The  function  returns ERROR_SUCCESS if successful, otherwise it returns an
    appropriate WinSock error code.

--*/
{
    INT ReturnValue = ERROR_SUCCESS;
    INT BufferSize  = sizeof(m_LibraryPath);

    // Copy ProtoInfo
    m_ProtoInfo = *ProtoInfo;

    // Get the LibraryPath
    gUpCallTable.lpWPUGetProviderPath(
        &ProtoInfo->ProviderId,
        (WCHAR*)&m_LibraryPath,
        &BufferSize,
        &ReturnValue);

    if (ERROR_SUCCESS != ReturnValue){
        m_LibraryPath[0] = '\0';
    } //if

    return (ReturnValue);

}  // Initialize




PROTO_CATALOG_ITEM::~PROTO_CATALOG_ITEM()
/*++

Routine Description:

    This  procedure  destroys  a  protocol catalog item, deallocating memory it
    owns.   It  is the caller's responsibility to remove the item from the list
    it  occupies  before  calling  this  procedure.   It  is  also the caller's
    responsibility  to unload and/or destroy any dprovider associated with this
    catalog item if appropriate.

Arguments:

    None

Return Value:

    None
--*/
{
    m_LibraryPath[0] = '\0';
}  // ~PROTO_CATALOG_ITEM




LPWSAPROTOCOL_INFOW
PROTO_CATALOG_ITEM::GetProtocolInfo()
/*++

Routine Description:

    This  procedure  retrieves a reference to the protocol info associated with
    the  catalog  item.   Note  that  the  reference is to storage owned by the
    catalog item.  It is the caller's responsibility to make sure the reference
    is no longer used after the catalog item is destroyed.

Arguments:

    None

Return Value:

    Returns a pointer to the associated protocol info.
--*/
{
    assert(m_LibraryPath[0] != '\0');
    return(& m_ProtoInfo);
}  // GetProtocolInfo




PWCHAR
PROTO_CATALOG_ITEM::GetLibraryPath()
/*++

Routine Description:

    This    procedure   retrieves   a   reference   to   the   zero-terminated,
    fully-qualified  path  of  the library that is the service provider for the
    protocol  associated  with the catalog item.  Note that the reference is to
    storage  owned  by  the catalog item.  It is the caller's responsibility to
    make  sure  the  reference  is  no  longer  used  after the catalog item is
    destroyed.

Arguments:

    None

Return Value:

    Returns a pointer to the library path string.
--*/
{
    assert(m_LibraryPath[0] != '\0');
    return(m_LibraryPath);
}  // GetLibraryPath




VOID
PROTO_CATALOG_ITEM::SetProviderCatalogEntry(
    IN  PPROTO_CATALOG_ITEM  CatalogEntry
    )
/*++

Routine Description:

    This procedure sets the PPROTO_CATALOG_ENTRY

Arguments:

    Provider - Supplies the new catalog entry reference.

Return Value:

    None
--*/
{
    if (m_ProviderCatalogEntry != NULL) {
        DEBUGF(
            DBG_WARN,
            ("Suspicious - overwriting a catalog entry reference\n"));
    }
    m_ProviderCatalogEntry = CatalogEntry;
}  // SetProviderCatalogEntry


VOID
PROTO_CATALOG_ITEM::SetProvider(
    IN  PDPROVIDER  Provider
    )
/*++

Routine Description:

    This procedure sets the DPROVIDER associated with a catalog entry.

Arguments:

    Provider - Supplies the new DPROVIDER reference.

Return Value:

    None
--*/
{
    if (m_Provider != NULL) {
        DEBUGF(
            DBG_WARN,
            ("Suspicious - overwriting a dprovider reference\n"));
    }
    m_Provider = Provider;
}  // SetProvider





PDPROVIDER
PROTO_CATALOG_ITEM::GetProvider()
/*++

Routine Description:

    This  procedure  retrieves  a  reference to the DPROVIDER associated with a
    catalog  entry.  Note that the reference may be NULL if no provider has yet
    been loaded for this protocol.

Arguments:

    None

Return Value:

    Returns  the  current provider reference, or NULL if there is no associated
    provider.
--*/
{
    return(m_Provider);
}  // GetProvider


PPROTO_CATALOG_ITEM
PROTO_CATALOG_ITEM::GetProviderCatalogEntry()
/*++

Routine Description:

    This  procedure  retrieves  a  reference to the protocol catalog entry associated with a
    catalog  entry.  Note that the reference may be NULL if no provider has yet
    been loaded for this protocol.

Arguments:

    None

Return Value:

    Returns  the  current provider catalog entry reference, or NULL if there is no associated
    provider.
--*/
{
    return(m_ProviderCatalogEntry);
}
