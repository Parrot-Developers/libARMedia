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
#include <jni.h>
#include <libARMedia/ARMEDIA_Error.h>
#include <libARMedia/ARMEDIA_VideoAtoms.h>
#include <libARMedia/ARMEDIA_VideoEncapsuler.h>
#include <stdlib.h>
#include <sys/file.h>

JNIEXPORT jbyteArray JNICALL
Java_com_parrot_arsdk_armedia_ARMediaVideoAtoms_nativeGetAtom(JNIEnv *env, jclass clazz, jstring fileName, jstring atom)
{
    const char *fname = (*env)->GetStringUTFChars(env, fileName, NULL);
    const char *atomName = (*env)->GetStringUTFChars(env, atom, NULL);

    jbyteArray retArray = NULL;

    FILE *file = fopen (fname, "rb");
    if (file != NULL)
    {
        int ret = flock(fileno(file), LOCK_EX);

        uint32_t size;
        uint8_t *data = createDataFromFile (file, atomName, &size);

        ret = flock(fileno(file), LOCK_UN);
        fclose(file);

        if (data != NULL)
        {
            retArray = (*env)->NewByteArray(env, size);
            (*env)->SetByteArrayRegion(env, retArray, 0, size, (jbyte*)data);

            free(data);
        }
    }

    (*env)->ReleaseStringUTFChars(env, fileName, fname);
    (*env)->ReleaseStringUTFChars(env, atom, atomName);

    return retArray;
}

JNIEXPORT void JNICALL
Java_com_parrot_arsdk_armedia_ARMediaVideoAtoms_nativeChangePvatDate(JNIEnv *env, jclass clazz, jstring fileName, jstring date)
{
    const char *fname = (*env)->GetStringUTFChars(env, fileName, NULL);
    const char *videoDate = (*env)->GetStringUTFChars(env, date, NULL);

    FILE *videoFile = fopen(fname, "r+");

    if (videoFile != NULL)
    {
      ARMEDIA_VideoEncapsuler_changePVATAtomDate(videoFile, videoDate);
      fclose(videoFile);
    }

    if (fileName != NULL && fname != NULL)
    {
        (*env)->ReleaseStringUTFChars(env, fileName, fname);
    }

    if (date != NULL && videoDate != NULL)
    {
        (*env)->ReleaseStringUTFChars(env, date, videoDate);
    }
}

JNIEXPORT void JNICALL
Java_com_parrot_arsdk_armedia_ARMediaVideoAtoms_nativeWritePvat(JNIEnv *env, jclass clazz, jstring fileName, int product, jstring date)
{
    const char *fname = (*env)->GetStringUTFChars(env, fileName, NULL);
    const char *videoDate = (*env)->GetStringUTFChars(env, date, NULL);

    FILE *videoFile = fopen(fname, "ab");

    if (videoFile != NULL)
    {
        ARMEDIA_VideoEncapsuler_addPVATAtom(videoFile, product, videoDate);
	fclose(videoFile);
    }

    if (fileName != NULL && fname != NULL)
    {
        (*env)->ReleaseStringUTFChars(env, fileName, fname);
    }
    
    if (date != NULL && videoDate != NULL)
    {
        (*env)->ReleaseStringUTFChars(env, date, videoDate);
    }
}
