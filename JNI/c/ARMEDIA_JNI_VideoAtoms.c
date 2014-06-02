#include <jni.h>
#include <libARMedia/ARMEDIA_Error.h>
#include <libARMedia/ARMEDIA_VideoAtoms.h>
#include <libARMedia/ARMEDIA_VideoEncapsuler.h>
#include <stdlib.h>

JNIEXPORT jbyteArray JNICALL
Java_com_parrot_arsdk_armedia_ARMediaVideoAtoms_nativeGetAtom(JNIEnv *env, jclass clazz, jstring fileName, jstring atom)
{
    const char *fname = (*env)->GetStringUTFChars(env, fileName, NULL);
    const char *atomName = (*env)->GetStringUTFChars(env, atom, NULL);


    FILE *file = fopen (fname, "rb");

    uint32_t size;
    uint8_t *data = createDataFromFile (file, atomName, &size);

    fclose(file);

    jbyteArray retArray = NULL;
    if (data != NULL)
    {
        retArray = (*env)->NewByteArray(env, size);
        (*env)->SetByteArrayRegion(env, retArray, 0, size, (jbyte*)data);

        free(data);
    }

    (*env)->ReleaseStringUTFChars(env, fileName, fname);
    (*env)->ReleaseStringUTFChars(env, atom, atomName);

    return retArray;
}
