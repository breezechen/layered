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

    classfwd.h

Abstract:

    This module contains forward definitions the for classes used in the
    WinSock2 layered service provider sample.

--*/

#ifndef _CLASSFWD_
#define _CLASSFWD_


#include <winsock2.h>

class DSOCKET;
typedef DSOCKET FAR * PDSOCKET;

class DPROVIDER;
typedef DPROVIDER FAR * PDPROVIDER;

class PROTO_CATALOG_ITEM;
typedef PROTO_CATALOG_ITEM  FAR * PPROTO_CATALOG_ITEM;

class DCATALOG;
typedef DCATALOG FAR * PDCATALOG;

class DWORKERTHREAD;
typedef DWORKERTHREAD FAR * PDWORKERTHREAD;

class DASYNCWINDOW;
typedef DASYNCWINDOW FAR * PDASYNCWINDOW;

class DOVERLAPPEDSTRUCTMGR;
typedef DOVERLAPPEDSTRUCTMGR FAR * PDOVERLAPPEDSTRUCTMGR;

class DBUFFERMANAGER;
typedef DBUFFERMANAGER FAR * PDBUFFERMANAGER;

#endif  // _CLASSFWD_
