package com.parrot.arsdk.armedia;

public class ARMediaVideoAtoms {

    private static native String nativeGetPvat(String path);

    public static String getPvat(String path)
    {
	return nativeGetPvat(path);
    }
}