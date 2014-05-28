#include <jni.h>
#include <libARMedia/ARMEDIA_Error.h>
#include <libARMedia/ARMEDIA_VideoAtoms.h>
#include <libARMedia/ARMEDIA_VideoEncapsuler.h>
#include <stdlib.h>

JNIEXPORT jstring JNICALL
Java_com_parrot_arsdk_armedia_ARMediaVideoAtoms_nativeGetPvat(JNIEnv *env, jclass clazz, jstring fileName)
{
    const char *fname = (*env)->GetStringUTFChars(env, fileName, NULL);

    FILE *file = fopen (fname, "rb");

    uint8_t *data = createDataFromFile (file, "pvat");

    fclose(file);

    jstring retStr = (*env)->NewStringUTF(env, data);

    free(data);

    (*env)->ReleaseStringUTFChars(env, fileName, fname);

    return retStr;
}
