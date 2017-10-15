/*=========================================================================*\

    Copyright (c) Microsoft Corporation.  All rights reserved.

\*=========================================================================*/

#pragma once

/*=========================================================================*\
    D2D Status Codes
\*=========================================================================*/

#define FACILITY_D2D 0x899

#define MAKE_D2DHR( sev, code )\
    MAKE_HRESULT( sev, FACILITY_D2D, (code) )

#define MAKE_D2DHR_ERR( code )\
    MAKE_D2DHR( 1, code )


//+----------------------------------------------------------------------------
//
// D2D error codes
//
//------------------------------------------------------------------------------

//
//  Error codes shared with WINCODECS
//

//
// The pixel format is not supported.
//
#define D2DERR_UNSUPPORTED_PIXEL_FORMAT     WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT

//
// Error codes that were already returned in prior versions and were part of the
// MIL facility.

//
// Error codes mapped from WIN32 where there isn't already another HRESULT based
// define
//

//
// The supplied buffer was too small to accomodate the data.
//
#define D2DERR_INSUFFICIENT_BUFFER          HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)


//
// D2D specific codes
//
