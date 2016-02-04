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

package com.parrot.arsdk.armedia;

import java.util.HashMap;

/**
 * Java copy of the eARMEDIA_ERROR enum
 */
public enum ARMEDIA_ERROR_ENUM {
   /** Dummy value for all unknown cases */
    eARMEDIA_ERROR_UNKNOWN_ENUM_VALUE (Integer.MIN_VALUE, "Dummy value for all unknown cases"),
   /** No error */
    ARMEDIA_OK (0, "No error"),
   /** Unknown generic error */
    ARMEDIA_ERROR (-1000, "Unknown generic error"),
   /** Bad parameters */
    ARMEDIA_ERROR_BAD_PARAMETER (-999, "Bad parameters"),
   /** Function not implemented */
    ARMEDIA_ERROR_NOT_IMPLEMENTED (-998, "Function not implemented"),
   /** Unknown manager error */
    ARMEDIA_ERROR_MANAGER (-2000, "Unknown manager error"),
   /** Error manager already initialized */
    ARMEDIA_ERROR_MANAGER_ALREADY_INITIALIZED (-1999, "Error manager already initialized"),
   /** Error manager not initialized */
    ARMEDIA_ERROR_MANAGER_NOT_INITIALIZED (-1998, "Error manager not initialized"),
   /** Error encapsuler generic error */
    ARMEDIA_ERROR_ENCAPSULER (-3000, "Error encapsuler generic error"),
   /** Error encapsuler waiting for i-frame */
    ARMEDIA_ERROR_ENCAPSULER_WAITING_FOR_IFRAME (-2999, "Error encapsuler waiting for i-frame"),
   /** Codec non-supported */
    ARMEDIA_ERROR_ENCAPSULER_BAD_CODEC (-2998, "Codec non-supported"),
   /** Error in video frame header */
    ARMEDIA_ERROR_ENCAPSULER_BAD_VIDEO_FRAME (-2997, "Error in video frame header"),
   /** Error in audio sample header */
    ARMEDIA_ERROR_ENCAPSULER_BAD_VIDEO_SAMPLE (-2996, "Error in audio sample header"),
   /** File error while encapsulating */
    ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR (-2995, "File error while encapsulating"),
   /** Timestamp is before previous sample */
    ARMEDIA_ERROR_ENCAPSULER_BAD_TIMESTAMP (-2994, "Timestamp is before previous sample");

    private final int value;
    private final String comment;
    static HashMap<Integer, ARMEDIA_ERROR_ENUM> valuesList;

    ARMEDIA_ERROR_ENUM (int value) {
        this.value = value;
        this.comment = null;
    }

    ARMEDIA_ERROR_ENUM (int value, String comment) {
        this.value = value;
        this.comment = comment;
    }

    /**
     * Gets the int value of the enum
     * @return int value of the enum
     */
    public int getValue () {
        return value;
    }

    /**
     * Gets the ARMEDIA_ERROR_ENUM instance from a C enum value
     * @param value C value of the enum
     * @return The ARMEDIA_ERROR_ENUM instance, or null if the C enum value was not valid
     */
    public static ARMEDIA_ERROR_ENUM getFromValue (int value) {
        if (null == valuesList) {
            ARMEDIA_ERROR_ENUM [] valuesArray = ARMEDIA_ERROR_ENUM.values ();
            valuesList = new HashMap<Integer, ARMEDIA_ERROR_ENUM> (valuesArray.length);
            for (ARMEDIA_ERROR_ENUM entry : valuesArray) {
                valuesList.put (entry.getValue (), entry);
            }
        }
        ARMEDIA_ERROR_ENUM retVal = valuesList.get (value);
        if (retVal == null) {
            retVal = eARMEDIA_ERROR_UNKNOWN_ENUM_VALUE;
        }
        return retVal;    }

    /**
     * Returns the enum comment as a description string
     * @return The enum description
     */
    public String toString () {
        if (this.comment != null) {
            return this.comment;
        }
        return super.toString ();
    }
}
