/*
    Copyright (C) 2014 Parrot SA

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the 
      distribution.
    * Neither the name of Parrot nor the names
      of its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
    OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
    AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
    OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
*/
/*
 * GENERATED FILE
 *  Do not modify this file, it will be erased during the next configure run 
 */

/**
 * @file ARMEDIA_Error.c
 * @brief ToString function for eARMEDIA_ERROR enum
 */

#include <libARMedia/ARMEDIA_Error.h>

const char* ARMEDIA_Error_ToString (eARMEDIA_ERROR error)
{
    switch (error)
    {
    case ARMEDIA_OK:
        return "No error";
        break;
    case ARMEDIA_ERROR:
        return "Unknown generic error";
        break;
    case ARMEDIA_ERROR_BAD_PARAMETER:
        return "Bad parameters";
        break;
    case ARMEDIA_ERROR_NOT_IMPLEMENTED:
        return "Function not implemented";
        break;
    case ARMEDIA_ERROR_MANAGER:
        return "Unknown manager error";
        break;
    case ARMEDIA_ERROR_MANAGER_ALREADY_INITIALIZED:
        return "Error manager already initialized";
        break;
    case ARMEDIA_ERROR_MANAGER_NOT_INITIALIZED:
        return "Error manager not initialized";
        break;
    case ARMEDIA_ERROR_ENCAPSULER:
        return "Error encapsuler generic error";
        break;
    case ARMEDIA_ERROR_ENCAPSULER_WAITING_FOR_IFRAME:
        return "Error encapsuler waiting for i-frame";
        break;
    case ARMEDIA_ERROR_ENCAPSULER_BAD_CODEC:
        return "Codec non-supported";
        break;
    case ARMEDIA_ERROR_ENCAPSULER_BAD_VIDEO_FRAME:
        return "Error in video frame header";
        break;
    case ARMEDIA_ERROR_ENCAPSULER_BAD_VIDEO_SAMPLE:
        return "Error in audio sample header";
        break;
    case ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR:
        return "File error while encapsulating";
        break;
    case ARMEDIA_ERROR_ENCAPSULER_BAD_TIMESTAMP:
        return "Timestamp is before previous sample";
        break;
    default:
        break;
    }
    return "Unknown value";
}
