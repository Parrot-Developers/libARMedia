package com.parrot.arsdk.armedia;

import com.parrot.arsdk.arsal.ARSALPrint;

import java.io.UnsupportedEncodingException;

public class ARMediaVideoAtoms {

    private static native byte [] nativeGetAtom(String path, String atom);

    private static final String TAG = "toto";
    //private static final String TAG = ARMediaVideoAtoms.class.getSimpleName();

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
}
