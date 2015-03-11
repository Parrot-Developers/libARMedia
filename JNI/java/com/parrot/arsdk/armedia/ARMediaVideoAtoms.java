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
package com.parrot.arsdk.armedia;

import com.parrot.arsdk.arsal.ARSALPrint;
import com.parrot.arsdk.ardiscovery.ARDISCOVERY_PRODUCT_ENUM;

import java.io.UnsupportedEncodingException;

public class ARMediaVideoAtoms {

    private static native byte [] nativeGetAtom(String path, String atom);

    private static native void nativeWritePvat(String path, int discoveryProduct, String videoDate);

    private static native void nativeChangePvatDate(String path, String videoDate);

    private static final String TAG = "ARMediaVideoAtoms";

    public static String getPvat(String path)
    {
        byte [] data = nativeGetAtom(path, "pvat");

        String s = null;
        if (data != null)
        {
            try
            {
                s = new String (data, "UTF-8");
            }
            catch (UnsupportedEncodingException uee)
            {
                ARSALPrint.e(TAG, "Error while creating pvat string");
                uee.printStackTrace();
            }
        }

        return s;
    }

    public static byte[] getAtom(String path, String atom)
    {
        return nativeGetAtom(path, atom);
    }

    public static void writePvat(String path, ARDISCOVERY_PRODUCT_ENUM discoveryProduct, String videoDate)
    {
        nativeWritePvat(path, discoveryProduct.getValue(), videoDate);
    }

    public static void changePvatDate(String path, String videoDate)
    {
        nativeChangePvatDate(path, videoDate);
    }
}
