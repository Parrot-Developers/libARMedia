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
 * ARMEDIA_VideoAtoms.c
 * ARDroneLib
 *
 * Created by n.brulez on 19/08/11
 * Copyright 2011 Parrot SA. All rights reserved.
 *
 */

#include <libARMedia/ARMedia.h>
#include <libARMedia/ARMEDIA_VideoEncapsuler.h>
#include <libARSAL/ARSAL_Print.h>
#include <json-c/json.h>

#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

#define ATOM_MALLOC(SIZE) malloc(SIZE)
#define ATOM_CALLOC(NMEMB, SIZE) calloc(NMEMB, SIZE)
#define ATOM_REALLOC(PTR, SIZE) realloc(PTR, SIZE)
#define ATOM_FREE(PTR) free(PTR)
#define ATOM_MEMCOPY(DST, SRC, SIZE) memcpy(DST, SRC, SIZE)

#define TIMESTAMP_FROM_1970_TO_1904 (0x7c25b080UL)
#define ATOM_SIZE 4

#define ARMEDIA_TAG "ARMEDIA_VideoAtoms"

/**
 * To use these macros, function must ensure :
 *  - currentIndex is declared and set to zero BEFORE first call
 *  - data is a valid pointer to the output array of datas we want to write
 * These macros don't include ANY size check, so function have to ensure that the data buffer have enough left space
 */

// Write a 4CC into atom data
//  Usage : ATOM_WRITE_4CC ('c', 'o', 'd', 'e');
#define ATOM_WRITE_4CC(_A, _B, _C, _D)          \
    do                                          \
    {                                           \
        data[currentIndex++] = _A;              \
        data[currentIndex++] = _B;              \
        data[currentIndex++] = _C;              \
        data[currentIndex++] = _D;              \
    } while (0)
// Write a 8 bit word into atom data
#define ATOM_WRITE_U8(VAL)                      \
    do                                          \
    {                                           \
        data[currentIndex] = (uint8_t)VAL;      \
        currentIndex++;                         \
    } while (0)
// Write a 16 bit word into atom data
#define ATOM_WRITE_U16(VAL)                                     \
    do                                                          \
    {                                                           \
        uint16_t *DPOINTER = (uint16_t *)&(data[currentIndex]); \
        currentIndex += 2;                                      \
        *DPOINTER = htons((uint16_t)VAL);                       \
    } while (0)
// Write a 32 bit word into atom data
#define ATOM_WRITE_U32(VAL)                                 \
    do                                                      \
    {                                                       \
        data[currentIndex++] = (uint8_t)((VAL>>24)&0xff);   \
        data[currentIndex++] = (uint8_t)((VAL>>16)&0xff);   \
        data[currentIndex++] = (uint8_t)((VAL>>8)&0xff);    \
        data[currentIndex++] = (uint8_t)((VAL)&0xff);       \
    } while (0)
// Write an defined number of bytes into atom data
//  Usage : ATOM_WRITE_BYTES (pointerToData, sizeToCopy)
#define ATOM_WRITE_BYTES(POINTER, SIZE)                     \
    do                                                      \
    {                                                       \
        ATOM_MEMCOPY (&data[currentIndex], POINTER, SIZE);  \
        currentIndex += SIZE;                               \
    } while (0)

/**
 * Reader functions
 * Get informations about a video from the video file, or directly from atom buffers
 */

/**
 * Read from a buffer macros :
 * Note : buffer MUST be called "pBuffer"
 */

#define ATOM_READ_U32(VAL)                      \
    do                                          \
    {                                           \
        VAL = ntohl (*(uint32_t *)pBuffer);     \
        pBuffer += 4;                           \
    } while (0)

#define ATOM_READ_U16(VAL)                      \
    do                                          \
    {                                           \
        VAL = ntohs (*(uint16_t *)pBuffer);     \
        pBuffer += 2;                           \
    } while (0)

#define ATOM_READ_U8(VAL)                       \
    do                                          \
    {                                           \
        VAL = *(uint8_t *)pBuffer;              \
        pBuffer++;                              \
    } while (0)

#define ATOM_READ_BYTES(POINTER, SIZE)                      \
    do                                                      \
    {                                                       \
        ATOM_MEMCOPY (POINTER, (uint8_t *)pBuffer, SIZE);   \
        pBuffer += SIZE;                                    \
    } while (0)

// Actual read functions
static void read_uint32 (FILE *fptr, uint32_t *dest)
{
    uint32_t locValue = 0;
    if (1 != fread (&locValue, sizeof (locValue), 1, fptr))
    {
        fprintf (stderr, "Error reading a uint32_t\n");
    }
    *dest = ntohl (locValue);
}

static void read_uint64 (FILE *fptr, uint64_t *dest)
{
    uint64_t locValue = 0;
    if (1 != fread (&locValue, sizeof (locValue), 1, fptr))
    {
        fprintf (stderr, "Error reading low value of a uint64_t\n");
        return;
    }
    *dest = swap_uint64(locValue);
}

static void read_4cc (FILE *fptr, char dest[5])
{
    if (1 != fread (dest, 4, 1, fptr))
    {
        fprintf (stderr, "Error reading a 4cc\n");
    }
}

movie_atom_t *atomFromData (uint32_t data_size, const char *tag, const uint8_t *data)
{
    movie_atom_t *retAtom = (movie_atom_t*) ATOM_MALLOC (sizeof (movie_atom_t));
    if (NULL == retAtom)
    {
        return retAtom;
    }
    retAtom->size = data_size + 8;
    memcpy (retAtom->tag, tag, 4);

    retAtom->data = NULL;
    if (NULL != data && data_size > 0)
    {
        retAtom->data = ATOM_MALLOC (data_size);
        if (NULL == retAtom->data)
        {
            ATOM_FREE (retAtom);
            retAtom = NULL;
            return NULL;
        }
        ATOM_MEMCOPY (retAtom->data, data, data_size);
    }
    retAtom->wide = 0;
    return retAtom;
}

void insertAtomIntoAtom (movie_atom_t *container, movie_atom_t **leaf)
{
    uint32_t new_container_size = container->size - 8 + (*leaf)->size;
    uint32_t leafSizeNetworkEndian;

    container->data = ATOM_REALLOC (container->data, new_container_size);
    if (NULL == container->data)
    {
        fprintf (stderr, "Alloc error for atom insertion\n");
        return;
    }
    leafSizeNetworkEndian = htonl ((*leaf)->size);
    ATOM_MEMCOPY (&container->data[container->size - 8], &leafSizeNetworkEndian, sizeof (uint32_t));
    ATOM_MEMCOPY (&container->data[container->size - 4], (*leaf)->tag, 4);
    if (NULL != (*leaf)->data)
    {
        ATOM_MEMCOPY (&container->data[container->size], (*leaf)->data, ((*leaf)->size - 8));
        container->size = new_container_size + 8;
    }
    ATOM_FREE ((*leaf)->data);
    (*leaf)->data = NULL;
    ATOM_FREE (*leaf);
    *leaf = NULL;
}

int writeAtomToFile (movie_atom_t **atom, FILE *file)
{
    uint32_t networkEndianSize;

    if (NULL == *atom)
    {
        return -1;
    }
    networkEndianSize = htonl ((*atom)->size);

    if (4 != fwrite (&networkEndianSize, 1, 4, file))
    {
        return -1;
    }
    if (4 != fwrite ((*atom)->tag, 1, 4, file))
    {
        return -1;
    }
    if (NULL != (*atom)->data)
    {
        uint32_t atom_data_size = 0;
        if (1 == (*atom)->wide)
        {
            atom_data_size = 8;
        }
        else
        {
            atom_data_size = (*atom)->size - 8;
        }

        if (atom_data_size != fwrite ((*atom)->data, 1, atom_data_size, file))
        {
            return -1;
        }
        else
        {
            fflush(file);
        }
    }

    ATOM_FREE ((*atom)->data);
    (*atom)->data = NULL;
    ATOM_FREE (*atom);
    *atom = NULL;

    return 0;
}

void freeAtom (movie_atom_t **atom)
{
    if ((NULL != atom) &&
        (NULL != *atom))
    {
        if (NULL != (*atom)->data)
        {
            ATOM_FREE ((*atom)->data);
        }
        ATOM_FREE (*atom);
        *atom = NULL;
    }
}

movie_atom_t *ftypAtomForFormatAndCodecWithOffset (eARMEDIA_ENCAPSULER_VIDEO_CODEC codec, off_t *offset)
{
    uint32_t dataSize;
    if (codec == CODEC_MPEG4_AVC) dataSize = 24;
    else if (codec == CODEC_MOTION_JPEG) dataSize = 12;
    else return NULL;

    if (NULL == offset)
        return NULL;

    uint8_t *data = (uint8_t*) ATOM_MALLOC (dataSize);
    uint32_t currentIndex = 0;
    movie_atom_t *retAtom = NULL;

    if (NULL == data)
        return NULL;

    if (codec == CODEC_MPEG4_AVC) {
        ATOM_WRITE_4CC('i', 's', 'o', 'm'); // major brand
        ATOM_WRITE_U32(0x00000200); // minor version
        ATOM_WRITE_4CC('i', 's', 'o', 'm'); // compatible brands
        ATOM_WRITE_4CC('i', 's', 'o', '2');
        ATOM_WRITE_4CC('m', 'p', '4', '1');
        ATOM_WRITE_4CC('a', 'v', 'c', '1');
        *offset = 48;
    } else if (codec == CODEC_MOTION_JPEG) {
        ATOM_WRITE_4CC('q', 't', ' ', ' '); // major brand
        ATOM_WRITE_U32(0x00000200); // minor version
        ATOM_WRITE_4CC('q', 't', ' ', ' ');
        *offset = 36;
    }
    retAtom = atomFromData (dataSize, "ftyp", data);

    ATOM_FREE(data);
    return retAtom;
}

movie_atom_t *mdatAtomForFormatWithVideoSize (uint64_t videoSize)
{
    uint32_t dataSize = 8;
    uint8_t *data = (uint8_t*) ATOM_MALLOC (dataSize);

    if (NULL == data)
        return NULL;

    uint32_t currentIndex = 0;
    movie_atom_t *retAtom = NULL;
    if (videoSize <= UINT32_MAX)
    {
        // Free/wide atom + mdat
        ATOM_WRITE_U32(0x00000000);
        ATOM_WRITE_4CC('m', 'd', 'a', 't');
        uint32_t sizeNe = htonl ((uint32_t) videoSize);
        ATOM_MEMCOPY (data, &sizeNe, sizeof (uint32_t));
        retAtom = atomFromData (dataSize, "free", data);
        retAtom->size = dataSize;
        retAtom->wide = 1;
    }
    else
    {
        // 64bit wide mdat atom
        uint32_t highSize = ((videoSize+8) >> 32);
        uint32_t lowSize = ((videoSize+8) & 0xffffffff);
        uint32_t highSizeNe = htonl (highSize);
        uint32_t lowSizeNe = htonl (lowSize);
        ATOM_MEMCOPY (data, &highSizeNe, sizeof (uint32_t));
        ATOM_MEMCOPY (&data[4], &lowSizeNe, sizeof (uint32_t));
        retAtom = atomFromData (dataSize, "mdat", data);
        retAtom->size = 1;
        retAtom->wide = 1;
    }
    ATOM_FREE(data);
    return retAtom;
}

movie_atom_t *mvhdAtomFromFpsNumFramesAndDate (uint32_t timescale, uint32_t duration, time_t date)
{
    uint32_t date_1904 = (uint32_t)date + TIMESTAMP_FROM_1970_TO_1904;
    uint32_t dataSize = 100;
    uint8_t *data = (uint8_t*) ATOM_MALLOC (dataSize);
    uint32_t currentIndex = 0;
    movie_atom_t *retAtom;

    if (NULL == data)
    {
        return NULL;
    }


    ATOM_WRITE_U32 (0); /* Version (8) + Flags (24) */
    ATOM_WRITE_U32 (date_1904); /* Creation time */
    ATOM_WRITE_U32 (date_1904); /* Modification time */
    ATOM_WRITE_U32 (timescale); /* Timescale */
    ATOM_WRITE_U32 (duration); /* Duration (in timescale units) */

    ATOM_WRITE_U32 (0x00010000); /* Rate */
    ATOM_WRITE_U16 (0x0100); /* Volume */
    ATOM_WRITE_U16 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */

    ATOM_WRITE_U32 (0x00010000); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0x00010000); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0x40000000); /* Reserved */

    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (4); /* Next track id */

    retAtom = atomFromData (dataSize, "mvhd", data);
    ATOM_FREE (data);
    return retAtom;
}

movie_atom_t *tkhdAtomWithResolutionNumFramesFpsAndDate (uint32_t w, uint32_t h, uint32_t timescale, uint32_t duration, time_t date, eARMEDIA_VIDEOATOM_MEDIATYPE type)
{
    uint32_t date_1904 = (uint32_t)date + TIMESTAMP_FROM_1970_TO_1904;
    uint32_t dataSize = 84;
    uint8_t *data = (uint8_t*) ATOM_MALLOC (dataSize);
    uint32_t currentIndex = 0;
    movie_atom_t *retAtom;

    if (NULL == data)
    {
        return NULL;
    }

    ATOM_WRITE_U32 (0x0000000f); /* Version (8) + Flags (24) */
    ATOM_WRITE_U32 (date_1904); /* Creation time */
    ATOM_WRITE_U32 (date_1904); /* Modification time */
    ATOM_WRITE_U32 ((type+1)); /* Track ID */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (duration); /* Duration (in timescale units) */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    if (type == ARMEDIA_VIDEOATOM_MEDIATYPE_SOUND)
    {
        ATOM_WRITE_U16 (0); /* layer */
        ATOM_WRITE_U16 (1); /* alternative group */
        ATOM_WRITE_U8 (1); /* volume */
        ATOM_WRITE_U8 (0); /* volume */
        ATOM_WRITE_U16 (0); /* reserved */
    } else {
        ATOM_WRITE_U32 (0); /* layer (16) + alternative group (16) */
        ATOM_WRITE_U32 (0); /* volume (16) + reserved (16) */
    }

    ATOM_WRITE_U32 (0x00010000); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0x00010000); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0x40000000); /* Reserved */

    if (type == ARMEDIA_VIDEOATOM_MEDIATYPE_VIDEO)
    {
        ATOM_WRITE_U16 (w); /* Width */
        ATOM_WRITE_U16 (0); /* Reserved */
        ATOM_WRITE_U16 (h); /* Height*/
        ATOM_WRITE_U16 (0); /* Reserved */
    } else {
        ATOM_WRITE_U32 (0);
        ATOM_WRITE_U32 (0);
    }

    retAtom = atomFromData (dataSize, "tkhd", data);
    ATOM_FREE (data);
    data = NULL;
    return retAtom;
}

movie_atom_t *cdscAtomGen (uint32_t *target_track_id, uint32_t target_track_id_count)
{
    uint32_t dataSize = target_track_id_count * sizeof(uint32_t);
    uint8_t *data = (uint8_t*) ATOM_MALLOC (dataSize);
    uint32_t currentIndex = 0, i;
    movie_atom_t *retAtom;

    if (NULL == data)
    {
        return NULL;
    }

    for (i = 0; i < target_track_id_count; i++)
    {
        ATOM_WRITE_U32 (target_track_id[i]); /* track_IDs[] */
    }

    retAtom = atomFromData (dataSize, "cdsc", data);
    ATOM_FREE (data);
    return retAtom;
}

movie_atom_t *mdhdAtomFromFpsNumFramesAndDate (uint32_t timescale, uint32_t duration, time_t date)
{
    uint32_t date_1904 = (uint32_t)date + TIMESTAMP_FROM_1970_TO_1904;
    uint32_t dataSize = 24;
    uint32_t currentIndex = 0;
    uint8_t *data = (uint8_t*) ATOM_MALLOC (dataSize);
    movie_atom_t *retAtom;

    if (NULL == data)
    {
        return NULL;
    }

    ATOM_WRITE_U32 (0); /* Version (8) + Flags (24) */
    ATOM_WRITE_U32 (date_1904); /* Creation time */
    ATOM_WRITE_U32 (date_1904); /* Modification time */
    ATOM_WRITE_U32 (timescale); /* Timescale */
    ATOM_WRITE_U32 (duration); /* Duration (in timescale units) */
    ATOM_WRITE_U32 (0x55c40000); /* Language code (16) + Quality (16) */

    retAtom = atomFromData (dataSize, "mdhd", data);
    ATOM_FREE (data);
    data = NULL;
    return retAtom;
}

movie_atom_t *hdlrAtomForMdia (eARMEDIA_VIDEOATOM_MEDIATYPE type)
{
    uint8_t data [25] =  {0x00, 0x00, 0x00, 0x00,
                          'm', 'h', 'l', 'r',
                          'v', 'i', 'd', 'e',
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00};
    int dataSize = 25;

    if (type == ARMEDIA_VIDEOATOM_MEDIATYPE_SOUND) {
        data[8] = 's';
        data[9] = 'o';
        data[10] = 'u';
        data[11] = 'n';
    } else if (type == ARMEDIA_VIDEOATOM_MEDIATYPE_METADATA) {
        data[8] = 'm';
        data[9] = 'e';
        data[10] = 't';
        data[11] = 'a';
    }
    return atomFromData (dataSize, "hdlr", data);
}

movie_atom_t *hdlrAtomForMetadata ()
{
    uint8_t data [25] =  {0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          'm', 'd', 't', 'a',
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00};

    return atomFromData (25, "hdlr", data);
}

movie_atom_t *hdlrAtomForUdtaMetadata ()
{
    uint8_t data [25] =  {0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          'm', 'd', 'i', 'r',
                          'a', 'p', 'p', 'l',
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00};

    return atomFromData (25, "hdlr", data);
}

movie_atom_t *smhdAtomGen (void)
{
    uint8_t data [8] = {0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00};
    return atomFromData (8, "smhd", data);
}

movie_atom_t *vmhdAtomGen (void)
{
    uint8_t data [12] = {0x00, 0x00, 0x00, 0x01,
                         0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00};
    return atomFromData (12, "vmhd", data);
}

movie_atom_t *nmhdAtomGen (void)
{
    uint8_t data [4] = {0x00, 0x00, 0x00, 0x00};
    return atomFromData (4, "nmhd", data);
}

movie_atom_t *hdlrAtomForMinf (void)
{
    uint8_t data [25] =  {0x00, 0x00, 0x00, 0x00,
                          'd', 'h', 'l', 'r',
                          'u', 'r', 'l', ' ',
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00};
    return atomFromData (25, "hdlr", data);
}

movie_atom_t *drefAtomGen (void)
{
    uint8_t data [20] = {0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x01,
                         0x00, 0x00, 0x00, 0x0C,
                         0x75, 0x72, 0x6c, 0x20,
                         0x00, 0x00, 0x00, 0x01};
    return atomFromData (20, "dref", data);
}

movie_atom_t *stsdAtomWithResolutionAndCodec (uint32_t w, uint32_t h, eARMEDIA_ENCAPSULER_VIDEO_CODEC codec)
{
    uint32_t dataSize;
    uint32_t currentIndex = 0;
    movie_atom_t *retAtom;

    if (CODEC_MPEG4_AVC == codec) {
        dataSize = 128;
    } else if (CODEC_MOTION_JPEG == codec) {
        dataSize = 94; //138;
    } else if (CODEC_MPEG4_VISUAL == codec) {
        dataSize = 187;
    } else {
        return NULL;
    }

    uint8_t* data = (uint8_t*) ATOM_MALLOC(dataSize);
    if (NULL == data)
    {
        return NULL;
    }

    if (CODEC_MPEG4_AVC == codec)
    {
        ATOM_WRITE_U32(0x00000000);         // 0 - version & flags
        ATOM_WRITE_U32(0x00000001);         // 4 - number of entries

        ATOM_WRITE_U32(0x00000078);         // 8 - size of sample description
        ATOM_WRITE_4CC('a', 'v', 'c', '1'); // 12 - tag for sample description
        ATOM_WRITE_U32(0x00000000);         // 16 -
        ATOM_WRITE_U32(0x00000001);         // 20 -
        ATOM_WRITE_U32(0x00000000);         // 24 - version & revision
        ATOM_WRITE_4CC('P', 'A', 'O', 'T'); // 28 - vendor
        ATOM_WRITE_U32(0x00000200);         // 32 - temporal quality
        ATOM_WRITE_U32(0x00000200);         // 36 - spatial quality
        ATOM_WRITE_U16(w);                  // 40 - width
        ATOM_WRITE_U16(h);                  // 42 - height
        ATOM_WRITE_U32(0x00480000);         // 44 - horizontal resolution
        ATOM_WRITE_U32(0x00480000);         // 48 - vertical resolution
        ATOM_WRITE_U32(0x00000000);         // 52 - data size
        ATOM_WRITE_U16(0x0001);             // 56 - frame count
        ATOM_WRITE_U16(0x0000);             // 58 - compressor name
        ATOM_WRITE_U32(0x00000000);         // 60 - ...
        ATOM_WRITE_U32(0x00000000);         // 64 - ...
        ATOM_WRITE_U32(0x00000000);         // 68 - ...
        ATOM_WRITE_U32(0x00000000);         // 72 - ...
        ATOM_WRITE_U32(0x00000000);         // 76 - ...
        ATOM_WRITE_U32(0x00000000);         // 80 - ...
        ATOM_WRITE_U32(0x00000000);         // 84 - compressor name
        ATOM_WRITE_U16(0x0000);             // 88 - compressor name
        ATOM_WRITE_U16(24);                 // 90 - color depth
        ATOM_WRITE_U16(0xffff);             // 92 - color table (-1 => none)

        ATOM_WRITE_U16(0x0000);             // 94 - size of description extension
        ATOM_WRITE_U16(0x0022);             // 96 - size of description extension
        ATOM_WRITE_4CC('a', 'v', 'c', 'C'); // 98 - tag of description extension
        ATOM_WRITE_U16(0x0142);             // 102 - AVC decoder config record
        ATOM_WRITE_U32(0x801effe1);         // 104 - AVC decoder config record
        ATOM_WRITE_U32(0x00096742);         // 108 - AVC decoder config record
        ATOM_WRITE_U32(0x801e8b68);         // 112 - AVC decoder config record
        ATOM_WRITE_U32(0x0a02f101);         // 116 - AVC decoder config record
        ATOM_WRITE_U32(0x000668ce);         // 120 - AVC decoder config record
        ATOM_WRITE_U32(0x01a87720);         // 124 - AVC decoder config record
                                            // 128 -- END
    }
    else if (CODEC_MOTION_JPEG == codec)
    {
        ATOM_WRITE_U32(0x00000000);         // 0 - version & flags
        ATOM_WRITE_U32(0x00000001);         // 4 - number of entries

        ATOM_WRITE_U32(0x00000056);         // 8 - size of sample description
        ATOM_WRITE_4CC('j', 'p', 'e', 'g'); // 12 - tag for sample description
        ATOM_WRITE_U32(0x00000000);         // 16 -
        ATOM_WRITE_U32(0x00000001);         // 20 -
        ATOM_WRITE_U32(0x00000000);         // 24 - version & revision
        ATOM_WRITE_4CC('P', 'A', 'O', 'T'); // 28 - vendor
        ATOM_WRITE_U32(512);                // 32 - temporal quality
        ATOM_WRITE_U32(512);                // 36 - spatial quality
        ATOM_WRITE_U16(w);                  // 40 - width
        ATOM_WRITE_U16(h);                  // 42 - height
        ATOM_WRITE_U32(0x00480000);         // 44 - horizontal resolution
        ATOM_WRITE_U32(0x00480000);         // 48 - vertical resolution
        ATOM_WRITE_U32(0x00000000);         // 52 - data size
        ATOM_WRITE_U16(0x0001);             // 56 - frame count
        ATOM_WRITE_U16(0x0000);             // 58 - compressor name
        ATOM_WRITE_U32(0x00000000);         // 60 - compressor name
        ATOM_WRITE_U32(0x00000000);         // 64 - ...
        ATOM_WRITE_U32(0x00000000);         // 68 - ...
        ATOM_WRITE_U32(0x00000000);         // 72 - ...
        ATOM_WRITE_U32(0x00000000);         // 76 - ...
        ATOM_WRITE_U32(0x00000000);         // 80 - ...
        ATOM_WRITE_U32(0x00000000);         // 84 - ...
        ATOM_WRITE_U16(0x0000);             // 88 - compressor name
        ATOM_WRITE_U16(24);                 // 90 - color depth
        ATOM_WRITE_U16(0xffff);             // 92 - color table (-1 => none)
                                            // 96 -- END
    }
    else if (CODEC_MPEG4_VISUAL == codec)
    {
        ATOM_WRITE_U32(0x00000000);         // 0 - version & flags
        ATOM_WRITE_U32(0x00000001);         // 4 - number of entries

        ATOM_WRITE_U32(0x000000b3);         // 8 - size of sample description
        ATOM_WRITE_4CC('m', 'p', '4', 'v'); // 12 - tag for sample description
        ATOM_WRITE_U32(0x00000000);         // 16 -
        ATOM_WRITE_U32(0x00000001);         // 20 -
        ATOM_WRITE_U32(0x00000000);         // 24 - version & revision
        ATOM_WRITE_4CC('P', 'A', 'O', 'T'); // 28 - vendor
        ATOM_WRITE_U32(0x00000200);         // 32 - temporal quality
        ATOM_WRITE_U32(0x00000200);         // 36 - spatial quality
        ATOM_WRITE_U16(w);                  // 40 - width
        ATOM_WRITE_U16(h);                  // 42 - height
        ATOM_WRITE_U32(0x00480000);         // 44 - horizontal resolution
        ATOM_WRITE_U32(0x00480000);         // 48 - vertical resolution
        ATOM_WRITE_U32(0x00000000);         // 52 - data size
        ATOM_WRITE_U16(0x0001);             // 56 - frame count
        ATOM_WRITE_U8 (0x05);               // 58 - compressor name size
        ATOM_WRITE_4CC('m', 'p', 'e', 'g'); // 59 - compressor name
        ATOM_WRITE_U8 ('4');                // 63 - compressor name
        ATOM_WRITE_U32(0x00000000);         // 64 - ...
        ATOM_WRITE_U32(0x00000000);         // 68 - ...
        ATOM_WRITE_U32(0x00000000);         // 72 - ...
        ATOM_WRITE_U32(0x00000000);         // 76 - ...
        ATOM_WRITE_U32(0x00000000);         // 80 - ...
        ATOM_WRITE_U32(0x00000000);         // 84 - ...
        ATOM_WRITE_U16(0x0000);             // 88 - compressor name
        ATOM_WRITE_U16(24);                 // 90 - color depth
        ATOM_WRITE_U16(0xffff);             // 92 - color table (-1 => none)

        ATOM_WRITE_U32(0x0000005d);         // 94 - size of description extension
        ATOM_WRITE_4CC('e', 's', 'd', 's'); // 98 - tag of description extension
        ATOM_WRITE_U32(0x00000000);         // 102 - version & flags
        ATOM_WRITE_U32(0x03808080);         // 106 - elementary stream descriptor
        ATOM_WRITE_U32(0x4c000100);         // 110 - elementary stream descriptor
        ATOM_WRITE_U32(0x04808080);         // 114 - elementary stream descriptor
        ATOM_WRITE_U32(0x3e201100);         // 118 - elementary stream descriptor
        ATOM_WRITE_U32(0x00000007);         // 122 - elementary stream descriptor
        ATOM_WRITE_U32(0xacda0007);         // 126 - elementary stream descriptor
        ATOM_WRITE_U32(0xacda0580);         // 130 - elementary stream descriptor
        ATOM_WRITE_U32(0x80802c00);         // 134 - elementary stream descriptor
        ATOM_WRITE_U32(0x0001b001);         // 138 - elementary stream descriptor
        ATOM_WRITE_U32(0x000001b5);         // 142 - elementary stream descriptor
        ATOM_WRITE_U32(0x89130000);         // 146 - elementary stream descriptor
        ATOM_WRITE_U32(0x01000000);         // 150 - elementary stream descriptor
        ATOM_WRITE_U32(0x012000c4);         // 154 - elementary stream descriptor
        ATOM_WRITE_U32(0x8d8800f5);         // 158 - elementary stream descriptor
        ATOM_WRITE_U32(0x14042e14);         // 162 - elementary stream descriptor
        ATOM_WRITE_U32(0x63000001);         // 166 - elementary stream descriptor
        ATOM_WRITE_U8 (0xb2);               // 170 - elementary stream descriptor
        ATOM_WRITE_4CC('B', 'e', 'b', 'o'); // 171 - elementary stream descriptor
        ATOM_WRITE_4CC('p', 'D', 'r', 'o'); // 175 - elementary stream descriptor
        ATOM_WRITE_4CC('n', 'e', 6, 0x80);  // 179 - elementary stream descriptor
        ATOM_WRITE_U32(0x80800102);         // 183 - elementary stream descriptor
                                            // 187 -- END
    }
    retAtom = atomFromData (dataSize, "stsd", data);
    ATOM_FREE(data);
    return retAtom;
}

movie_atom_t *stsdAtomWithAudioCodec(eARMEDIA_ENCAPSULER_AUDIO_CODEC codec, eARMEDIA_ENCAPSULER_AUDIO_FORMAT format, uint16_t nchannel, uint16_t freq)
{
    uint32_t dataSize = 68;
    uint32_t currentIndex = 0;
    movie_atom_t *retAtom;

    uint8_t* data = (uint8_t*) ATOM_MALLOC(dataSize);
    if (NULL == data)
    {
        return NULL;
    }

    ATOM_WRITE_U32(0);                  // 0 - version & flags
    ATOM_WRITE_U32(1);                  // 4 - number of entries

    ATOM_WRITE_U32(60);                 // 8 - size of sample description
    ATOM_WRITE_4CC('s', 'o', 'w', 't'); // 12 - tag for sample description
    ATOM_WRITE_U32(0x00000000);         // 16 - reserved
    ATOM_WRITE_U16(0x0000);             // 20 - reserved
    ATOM_WRITE_U16(0x0001);             // 22 - data reference index
    ATOM_WRITE_U32(0x00000000);         // 24 - version flag
    ATOM_WRITE_U32(0x00000000);         // 28 - vendor
    ATOM_WRITE_U16(nchannel);           // 32 - number of channels
    ATOM_WRITE_U16(format);             // 34 - sample size
    ATOM_WRITE_U16(0);                  // 36 - compression ID
    ATOM_WRITE_U16(0);                  // 38 - packet size
    ATOM_WRITE_U16(freq);               // 40 - sample rate
    ATOM_WRITE_U16(0);                  // 42 - sample rate
    ATOM_WRITE_U32(24);                 // 44 - channel descriptor size
    ATOM_WRITE_4CC('c', 'h', 'a', 'n'); // 48 - chan descriptor atom
    ATOM_WRITE_U32(0x00000000);         // 52 - version & flags
    ATOM_WRITE_U32(0x00640001);         // 56 - audio channel layout
    ATOM_WRITE_U32(0x00000000);         // 60 - audio channel layout
    ATOM_WRITE_U32(0x00000000);         // 64 - audio channel layout
                                        // 68 -- END

    retAtom = atomFromData (dataSize, "stsd", data);
    ATOM_FREE(data);
    return retAtom;
}

movie_atom_t *stsdAtomForMetadata (const char *content_encoding, const char *mime_format)
{
    uint32_t dataSize = 24;
    uint32_t currentIndex = 0;
    int content_encoding_len, mime_format_len;
    movie_atom_t *retAtom;

    if (content_encoding != NULL)
        content_encoding_len = strnlen (content_encoding, ARMEDIA_ENCAPSULER_METADATA_STSD_INFO_SIZE);
    else
        content_encoding_len = 0;

    if (mime_format != NULL)
        mime_format_len = strnlen (mime_format, ARMEDIA_ENCAPSULER_METADATA_STSD_INFO_SIZE);
    else
        mime_format_len = 0;

    dataSize += content_encoding_len+1 + mime_format_len+1;

    uint8_t* data = (uint8_t*) ATOM_MALLOC(dataSize);
    if (NULL == data)
    {
        return NULL;
    }

    ATOM_WRITE_U32(0);                      // 0 - version & flags
    ATOM_WRITE_U32(1);                      // 4 - number of entries

    ATOM_WRITE_U32((dataSize-8));           // 8 - size of sample description
    ATOM_WRITE_4CC('m', 'e', 't', 't');     // 12 - tag for sample description
    ATOM_WRITE_U32(0x00000000);             // 16 - reserved
    ATOM_WRITE_U16(0x0000);                 // 20 - reserved
    ATOM_WRITE_U16(0x0001);                 // 22 - data reference index

    // 24 - content_encoding beginning
    memcpy (data + currentIndex, content_encoding, content_encoding_len);
    currentIndex += content_encoding_len;
    data[currentIndex] = '\0';
    currentIndex++;

    // 24 + content_encoding_len+1 -> mime_format beginning
    memcpy (data + currentIndex, mime_format, mime_format_len);
    currentIndex += mime_format_len;
    data[currentIndex] = '\0';
    currentIndex++;

    retAtom = atomFromData (dataSize, "stsd", data);
    ATOM_FREE(data);
    return retAtom;
}

movie_atom_t *stsdAtomWithResolutionCodecSpsAndPps (uint32_t w, uint32_t h, eARMEDIA_ENCAPSULER_VIDEO_CODEC codec, uint8_t *sps, uint32_t spsSize, uint8_t *pps, uint32_t ppsSize)
{
    uint32_t avcCSize = 19 + spsSize + ppsSize;
    uint32_t vse_size = avcCSize + 86;
    uint32_t dataSize = vse_size + 8;
    uint8_t *data = NULL;
    uint32_t currentIndex = 0;
    movie_atom_t *retAtom;

    if (NULL == sps || NULL == pps || CODEC_MPEG4_AVC != codec)
    {
        return stsdAtomWithResolutionAndCodec (w, h, codec);
    }

    data = (uint8_t*) ATOM_MALLOC (dataSize);
    if (NULL == data)
    {
        return NULL;
    }

    ATOM_WRITE_U32 (0); /* version / flags */
    ATOM_WRITE_U32 (1); /* entry count */
    ATOM_WRITE_U32 (vse_size); /* video sample entry size */
    ATOM_WRITE_4CC ('a', 'v', 'c', '1'); /* VSE type */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U16 (0); /* Reserved */
    ATOM_WRITE_U16 (1); /* Data reference index */
    ATOM_WRITE_U16 (0); /* Codec stream version */
    ATOM_WRITE_U16 (0); /* Codec stream revision */
    ATOM_WRITE_4CC ('P', 'A', 'O', 'T'); /* Muxer name */
    ATOM_WRITE_U32 (0x200); /* Temporal quality */
    ATOM_WRITE_U32 (0x200); /* Spatial quality */
    ATOM_WRITE_U16 (w); /* Width */
    ATOM_WRITE_U16 (h); /* Height */
    ATOM_WRITE_U32 (0x00480000); /* Horiz DPI : 72dpi */
    ATOM_WRITE_U32 (0x00480000); /* Vert DPI : 72dpi */
    ATOM_WRITE_U32 (0); /* Data size */
    ATOM_WRITE_U16 (1); /* Frame count */
    ATOM_WRITE_U32 (0); /* Compressor name ... 32 octets of zeros */
    ATOM_WRITE_U32 (0); /* Compressor name ... 32 octets of zeros */
    ATOM_WRITE_U32 (0); /* Compressor name ... 32 octets of zeros */
    ATOM_WRITE_U32 (0); /* Compressor name ... 32 octets of zeros */
    ATOM_WRITE_U32 (0); /* Compressor name ... 32 octets of zeros */
    ATOM_WRITE_U32 (0); /* Compressor name ... 32 octets of zeros */
    ATOM_WRITE_U32 (0); /* Compressor name ... 32 octets of zeros */
    ATOM_WRITE_U32 (0); /* Compressor name ... 32 octets of zeros */
    ATOM_WRITE_U16 (0x18); /* Reserved */
    ATOM_WRITE_U16 (0xffff); /* Reserved */
    /* avcC tag */
    ATOM_WRITE_U32 (avcCSize); /* Size */
    ATOM_WRITE_4CC ('a', 'v', 'c', 'C'); /* avcC header */
    ATOM_WRITE_U8 (1); /* version */
    ATOM_WRITE_U8 (sps[1]); /* profile */
    ATOM_WRITE_U8 (sps[2]); /* profile compat */
    ATOM_WRITE_U8 (sps[3]); /* level */
    ATOM_WRITE_U8 (0xff); /* Reserved (6bits) -- NAL size length - 1 (2bits) */
    ATOM_WRITE_U8 (0xe1); /* Reserved (3 bits) -- Number of SPS (5 bits) */
    ATOM_WRITE_U16 (spsSize); /* Size of SPS */
    ATOM_WRITE_BYTES (sps, spsSize); /* SPS header */
    ATOM_WRITE_U8 (1); /* Number of PPS */
    ATOM_WRITE_U16 (ppsSize); /* Size of PPS */
    ATOM_WRITE_BYTES (pps, ppsSize); /* PPS Header */

    retAtom = atomFromData (dataSize, "stsd", data);
    ATOM_FREE (data);
    data = NULL;
    return retAtom;
}

movie_atom_t *stscAtomGen (uint32_t uniqueCount, uint32_t* stscTable, uint32_t nEntries)
{
    movie_atom_t* retAtom = NULL;
    uint8_t* data;
    uint32_t currentIndex = 0;
    uint32_t dataSize = 2 * sizeof(uint32_t);
    uint32_t localStscTable[3];

    if (uniqueCount != 0) {
        nEntries = 1;
        localStscTable[0] = htonl(1);
        localStscTable[1] = htonl(uniqueCount);
        localStscTable[2] = htonl(1);
        stscTable = localStscTable;
    } else if (stscTable == NULL) {
        return NULL;
    }

    dataSize += nEntries * 3 * sizeof(uint32_t);
    data = (uint8_t*) ATOM_MALLOC(dataSize);
    if (data == NULL) {
        return NULL;
    }

    ATOM_WRITE_U32(0); // version & flags
    ATOM_WRITE_U32(nEntries); // entry count
    memcpy (&data[currentIndex], stscTable, nEntries * 3 * sizeof(uint32_t));

    retAtom = atomFromData(dataSize, "stsc", data);
    ATOM_FREE(data);
    return retAtom;
}

movie_atom_t *stszAtomGen(uint32_t uniqueSize, uint32_t* sizeTable, uint32_t nSamples)
{
    movie_atom_t* retAtom = NULL;
    uint32_t currentIndex = 0;
    uint8_t* data;
    uint32_t dataSize = 3 * sizeof(uint32_t);

    if (uniqueSize == 0 && sizeTable != NULL) {
        dataSize += nSamples * sizeof(uint32_t);
    }

    data = (uint8_t*) ATOM_MALLOC(dataSize);
    if (data == NULL) {
        return NULL;
    }

    ATOM_WRITE_U32(0); // versions & flags
    ATOM_WRITE_U32(uniqueSize); // null if table
    ATOM_WRITE_U32(nSamples); // or table nentries

    if (uniqueSize == 0 && sizeTable != NULL) {
        memcpy (&data[currentIndex], sizeTable, nSamples * sizeof (uint32_t));
    }

    retAtom = atomFromData(dataSize, "stsz", data);
    ATOM_FREE(data);
    return retAtom;
}

movie_atom_t *metadataKeysAtom(const char *key[], uint32_t keyCount)
{
    movie_atom_t* retAtom = NULL;
    uint32_t currentIndex = 0, i;
    uint8_t* data;
    uint32_t dataSize = 2 * sizeof(uint32_t);
    for (i = 0; i < keyCount; i++)
    {
        dataSize += (2 * sizeof(uint32_t) + strlen(key[i]));
    }

    data = (uint8_t*) ATOM_MALLOC(dataSize);
    if (data == NULL) {
        return NULL;
    }

    ATOM_WRITE_U32(0); // versions & flags
    ATOM_WRITE_U32(keyCount); // entry_count
    for (i = 0; i < keyCount; i++)
    {
        ATOM_WRITE_U32((8 + strlen(key[i]))); // key_size
        ATOM_WRITE_4CC('m', 'd', 't', 'a'); // key_namespace
        ATOM_WRITE_BYTES(key[i], strlen(key[i])); // key_value
    }

    retAtom = atomFromData(dataSize, "keys", data);
    ATOM_FREE(data);
    return retAtom;
}

movie_atom_t *metadataAtomFromTagAndValue (uint32_t tagId, const char *tag, const char *value, uint8_t classId)
{
    movie_atom_t *retAtom = NULL;
    movie_atom_t *dataAtom = NULL;
    char locTag [4] = {0};
    if (tagId != 0)
    {
        locTag[0] = (tagId >> 24) & 0xFF;
        locTag[1] = (tagId >> 16) & 0xFF;
        locTag[2] = (tagId >> 8) & 0xFF;
        locTag[3] = tagId & 0xFF;
    }
    else
    {
        if (!tag)
        {
            return NULL;
        }
        /* If tag have a length of 3 chars, the (c) sign is added by this function */
        if (3 == strlen (tag))
        {
            locTag[0] = '\251'; // (c) sign
            strncpy (&locTag[1], tag, 3);
        }
        /* Custom tag */
        else if (4 == strlen (tag))
        {
            strncpy (locTag, tag, 4);
        }
    }

    uint16_t valLen = (uint16_t) strlen (value);
    uint32_t currentIndex = 0;
    uint32_t dataSize = valLen + 8;
    uint8_t *data = ATOM_MALLOC (dataSize);
    if (NULL != data)
    {
        ATOM_WRITE_U8 (0);
        ATOM_WRITE_U8 (0);
        ATOM_WRITE_U8 (0);
        ATOM_WRITE_U8 (classId);
        ATOM_WRITE_U8 (0);
        ATOM_WRITE_U8 (0);
        ATOM_WRITE_U8 (0);
        ATOM_WRITE_U8 (0);
        ATOM_WRITE_BYTES (value, valLen); /* Actual value */
        dataAtom = atomFromData (dataSize, "data", data);
        ATOM_FREE (data);
        data = NULL;
        retAtom = atomFromData(0, locTag, NULL);
        insertAtomIntoAtom(retAtom, &dataAtom);
    }
    return retAtom;
}

movie_atom_t *metadataAtomFromTagAndFile (uint32_t tagId, const char *tag, const char *file, uint8_t classId)
{
    movie_atom_t *retAtom = NULL;
    movie_atom_t *dataAtom = NULL;
    char locTag [4] = {0};
    if (tagId != 0)
    {
        locTag[0] = (tagId >> 24) & 0xFF;
        locTag[1] = (tagId >> 16) & 0xFF;
        locTag[2] = (tagId >> 8) & 0xFF;
        locTag[3] = tagId & 0xFF;
    }
    else
    {
        if (!tag)
        {
            return NULL;
        }
        /* If tag have a length of 3 chars, the (c) sign is added by this function */
        if (3 == strlen (tag))
        {
            locTag[0] = '\251'; // (c) sign
            strncpy (&locTag[1], tag, 3);
        }
        /* Custom tag */
        else if (4 == strlen (tag))
        {
            strncpy (locTag, tag, 4);
        }
    }

    FILE *f = fopen(file, "rb");
    if (!f)
    {
        ARSAL_PRINT(ARSAL_PRINT_ERROR, ARMEDIA_TAG, "failed to open cover file '%s'", file);
        return NULL;
    }
    fseeko(f, 0, SEEK_END);
    off_t fileLen = ftello(f);
    fseeko(f, 0, SEEK_SET);
    if (fileLen <= 0)
    {
        ARSAL_PRINT(ARSAL_PRINT_ERROR, ARMEDIA_TAG, "null length for cover file '%s'", file);
        return NULL;
    }
    if (fileLen < (INT32_MAX - 8))
    {
        uint32_t currentIndex = 0;
        uint32_t dataSize = fileLen + 8;
        uint8_t *data = ATOM_MALLOC (dataSize);
        if (NULL != data)
        {
            ATOM_WRITE_U8 (0);
            ATOM_WRITE_U8 (0);
            ATOM_WRITE_U8 (0);
            ATOM_WRITE_U8 (classId);
            ATOM_WRITE_U8 (0);
            ATOM_WRITE_U8 (0);
            ATOM_WRITE_U8 (0);
            ATOM_WRITE_U8 (0);
            fread(data + 8, dataSize, 1, f);
            ATOM_WRITE_BYTES (data + 8, fileLen); /* Actual data */
            dataAtom = atomFromData (dataSize, "data", data);
            ATOM_FREE (data);
            data = NULL;
            retAtom = atomFromData(0, locTag, NULL);
            insertAtomIntoAtom(retAtom, &dataAtom);
        }
    }
    else
    {
        ARSAL_PRINT(ARSAL_PRINT_ERROR, ARMEDIA_TAG, "cover file '%s' is too large to fit into atom (size: %zu)", file, (size_t)fileLen);
        return NULL;
    }
    return retAtom;
}

movie_atom_t *pvatAtomGen(const char *jsonString)
{
    uint32_t dataSize = (uint32_t) strlen(jsonString);

    /* Do not include '\0' end character */
    return atomFromData(dataSize, ARMEDIA_VIDEOATOMS_PVAT, (const uint8_t *)jsonString);
}

/**
 * Reader functions
 * Get informations about a video from the video file, or directly from atom buffers
 */

/**
 * Read from a buffer macros :
 * Note : buffer MUST be called "pBuffer"
 */

// DO NOT USE DIRECTLY
#define _ATOM_READ_PRRT_V(VAL, TYPE)            \
    do                                          \
    {                                           \
        VAL = *(TYPE *)pBuffer;                 \
        pBuffer += sizeof (TYPE);               \
    } while (0)
#define ATOM_READ_PRRT_V(VER, VAL) _ATOM_READ_PRRT_V(VAL, prrt_data_v##VER##_t)

/**
 * Read from a file functions
 * Note : fptr MUST be a valid file pointer
 */
/* Commented until actual use to avoid warnings
   static void read_uint8 (FILE *fptr, uint8_t *dest)
   {
   uint8_t locValue = 0;
   if (1 != fread (&locValue, sizeof (locValue), 1, fptr))
   {
   fprintf (stderr, "Error reading a uint8_t\n");
   }
   *dest = locValue;
   }

   static void read_uint16 (FILE *fptr, uint16_t *dest)
   {
   uint16_t locValue = 0;
   if (1 != fread (&locValue, sizeof (locValue), 1, fptr))
   {
   fprintf (stderr, "Error reading a uint16_t\n");
   }
   *dest = ntohs (locValue);
   }
*/

int seekMediaFileToAtom (FILE *videoFile, const char *atomName, uint64_t *retAtomSize, uint32_t atomIndex)
{
    uint32_t atomSize = 0;
    char fourCCTag [5] = {0};
    off_t wideAtomSize = 8;
    uint64_t size64 = 0;
    int found = 0;
    int fseekRet = 0;
    if (NULL == videoFile)
    {
        return 0;
    }
    while (0 == found && !feof (videoFile))
    {
        fseekRet = fseeko (videoFile, wideAtomSize - 8, SEEK_CUR);
        if (fseekRet < 0)
        {
            // Error while fseek
            break;
        }

        read_uint32 (videoFile, &atomSize);
        read_4cc (videoFile, fourCCTag);
        if (1 == atomSize)
        {
            read_uint64 (videoFile, &size64);
            wideAtomSize = (off_t)size64;
            // remove wideAtom size (64 bits)
            wideAtomSize = wideAtomSize - 8;
        }
        else if (0 == atomSize)
        {
            break;
        }
        else
        {
            wideAtomSize = atomSize;
        }
        if (0 == strncmp (fourCCTag, atomName, 4))
        {
            atomIndex--;
            ARSAL_PRINT(ARSAL_PRINT_VERBOSE, ARMEDIA_TAG, "found atom:%s, iterations left:%d", atomName, atomIndex);
            if (atomIndex == 0)
            {
                found = 1;
            }
        }
    }
    if (1 == found && NULL != retAtomSize)
    {
        *retAtomSize = wideAtomSize;
    }
    return found;
}

uint8_t *createDataFromFile (FILE *videoFile, const char* atom, uint32_t *dataSize)
{
    uint64_t atomSize = 0;
    uint8_t *atomBuffer = NULL;
    uint8_t *retBuffer = NULL;
    int valid = 1;
    char *tokSave;
    char *token;
    char *localAtom;
    char fcc[ATOM_SIZE + 1] = {0};

    if (videoFile == NULL)
    {
        goto err_nofree;
    }
    // Rewind videoFile
    rewind (videoFile);

    localAtom = strdup(atom);
    if(localAtom == NULL)
    {
        goto err_nofree;
    }
    token = strtok_r(localAtom, "/", &tokSave);
    while (token != NULL)
    {
        int tokenLen = strlen(token);
        ARSAL_PRINT(ARSAL_PRINT_VERBOSE, ARMEDIA_TAG, "token=%s len=%d",token, tokenLen);
        int idx = 1; // by default we search for the first atom that matches
        if (tokenLen > ATOM_SIZE)
        {
            // We got a special atom of type "idx:atom" to decode ex:"moov/2:trak/tkhd" in this we want the second trak atom only
            sscanf(token, "%d:%s", &idx, fcc);
            ARSAL_PRINT(ARSAL_PRINT_VERBOSE, ARMEDIA_TAG, "split %s into %s - %d\n", token, fcc, idx);
            token = fcc;
        }
        // Seek to atom
        ARSAL_PRINT(ARSAL_PRINT_VERBOSE, ARMEDIA_TAG, "looking for token:%s",token);
        int seekRes = seekMediaFileToAtom (videoFile, token, &atomSize, idx);
        ARSAL_PRINT(ARSAL_PRINT_VERBOSE, ARMEDIA_TAG, "token:%s, seekRes=%d",token, seekRes);
        if (0 == seekRes)
        {
            goto err_free;
        }
        token = strtok_r(NULL, "/", &tokSave);
    }

    if (atomSize > 8) {
        atomSize -= 8; // Remove the [size - tag] part, as it was read during seek
    } else {
        // atomSize is minimum 8; if not -> problem
        goto err_free;
    }

    // Alloc local buffer
    atomBuffer = ATOM_MALLOC (atomSize);
    if (NULL == atomBuffer)
    {
        valid = 0;
    }

    // Read atom from file
    if (1 == valid)
    {
        size_t nbRead = fread (atomBuffer, sizeof (uint8_t), atomSize, videoFile);
        if (atomSize != nbRead)
        {
            valid = 0;
        }
    }

    // Create buffer from atom
    if (1 == valid)
    {
        retBuffer = createDataFromAtom (atomBuffer, atomSize);
    }

    if (dataSize != NULL)
    {
        *dataSize = atomSize;
    }

    // Free any allocated resource
    if (NULL != atomBuffer)
    {
        ATOM_FREE (atomBuffer);
    }

err_free:
    free (localAtom);
err_nofree:
    return retBuffer;
}

uint8_t *createDataFromAtom(uint8_t *atomBuffer, const int atomSize)
{
    uint8_t *pBuffer = atomBuffer;
    uint8_t *retVal = NULL;
    // Sanity check : ensure that the buffer we got is not null and long enough
    if ((NULL == atomBuffer) || (atomSize < 0))
    {
        return NULL;
    }

    // Buffer and size are ok, alloc return pointer
    retVal = ATOM_CALLOC (1, atomSize);
    if (NULL == retVal)
    {
        return NULL;
    }

    // Start reading atom data
    ATOM_READ_BYTES(retVal, atomSize);

    return retVal;
}

uint32_t getVideoFpsFromFile (FILE *videoFile)
{
    uint64_t atomSize = 0;
    uint8_t *atomBuffer = NULL;
    uint32_t retFps = 0;
    int valid = 1;
    int seekRes;

    if (videoFile == NULL)
    {
        return 0;
    }
    // Rewind videoFile
    rewind (videoFile);

    // Seek to mdhd atom, following its parents :
    // ROOT:
    // moov
    //  \ trak
    //     \ mdia
    //        \ mdhd <-
    // First seek : moov
    seekRes = seekMediaFileToAtom (videoFile, "moov", NULL, 1);
    if (0 == seekRes)
    {
        valid = 0;
    }
    // Second seek : trak
    if (1 == valid)
    {
        seekRes = seekMediaFileToAtom (videoFile, "trak", NULL, 1);
        if (0 == seekRes)
        {
            valid = 0;
        }
    }
    // Third seek : mdia
    if (1 == valid)
    {
        seekRes = seekMediaFileToAtom (videoFile, "mdia", NULL, 1);
        if (0 == seekRes)
        {
            valid = 0;
        }
    }
    // Final seek : mdhd
    if (1 == valid)
    {
        seekRes = seekMediaFileToAtom (videoFile, "mdhd", &atomSize, 1);
        if (0 == seekRes)
        {
            valid = 0;
        }
    }

    // If seek were sucessfull, alloc local buffer
    if (1 == valid)
    {
        atomSize -= 8; // Remove [size - tag] as it was read by seek
        atomBuffer = ATOM_MALLOC (atomSize);
        if (NULL == atomBuffer)
        {
            valid = 0;
        }
    }

    // Read data from file
    if (1 == valid)
    {
        size_t nbRead = fread (atomBuffer, sizeof (uint8_t), atomSize, videoFile);
        if (atomSize != nbRead)
        {
            valid = 0;
        }
    }

    // Get fps info from atom buffer
    if (1 == valid)
    {
        retFps = getVideoFpsFromAtom (atomBuffer, atomSize);
    }

    // Free any allocated resource
    if (NULL != atomBuffer) { ATOM_FREE (atomBuffer); }

    return retFps;
}

uint32_t getVideoFpsFromAtom (uint8_t *mdhdAtom, const int atomSize)
{
    uint32_t vflags;
    uint32_t cdate;
    uint32_t mdate;
    uint32_t fps = 0;
    int valid = 1;
    uint8_t *pBuffer = mdhdAtom;
    // Sanity check : atom must not be null
    if (NULL == mdhdAtom)
    {
        valid = 0;
    }

    // Sanity check : size must be greater than 16 (should in most cases be 24)
    if (16 > atomSize)
    {
        valid = 0;
    }

    // Read infos
    if (1 == valid)
    {
        ATOM_READ_U32 (vflags);
        ATOM_READ_U32 (cdate);
        ATOM_READ_U32 (mdate);
        ATOM_READ_U32 (fps);
    }

    // Remove "unused" warnings
    (void)vflags;
    (void)cdate;
    (void)mdate;

    return fps;
}

uint64_t swap_uint64(uint64_t value)
{
    uint32_t atomSizeLow;
    uint32_t atomSizeHigh;

    atomSizeLow = value & 0xffffffff;
    atomSizeHigh = value >> 32;
    atomSizeHigh = ntohl(atomSizeHigh);
    atomSizeLow = ntohl(atomSizeLow);

    return ((uint64_t)atomSizeLow) << 32 | atomSizeHigh;
}

int seekMediaBufferToAtom (uint8_t *buff, long long *offset,long long size, const char *tag)
{
    int retVal = 0;

    uint32_t atomSize32 = 0;
    uint64_t atomSize = 0;
    uint32_t atomTag = 0;
    memcpy(&atomSize32, buff, sizeof(uint32_t));
    atomSize = (uint64_t)ntohl(atomSize32);
    memcpy(&atomTag, buff + sizeof(uint32_t), sizeof(uint32_t));

    if (atomSize == 0)
    {
        *offset += size - *offset;
    }

    if(atomSize == 1)
    {
        memcpy(&atomSize, buff + (2 * sizeof(uint32_t)), sizeof(uint64_t));
        atomSize = atom_ntohll(atomSize);
    }

    if(((uint8_t *)&atomTag)[0] == tag[0] &&
       ((uint8_t *)&atomTag)[1] == tag[1] &&
       ((uint8_t *)&atomTag)[2] == tag[2] &&
       ((uint8_t *)&atomTag)[3] == tag[3])
    {
        retVal = 1;
    }
    else
    {
        *offset += atomSize;
    }

    return retVal;
}


char* ARMEDIA_VideoAtom_GetPVATString(const eARDISCOVERY_PRODUCT id, const char* uuid, const char* runDate, const char* filepath, const struct tm* mediaDate)
{
    struct json_object* pvato = (struct json_object*) json_object_new_object();

    if (pvato != NULL) {
        char dateBuff[DATETIME_MAXLENGTH];
        char prodid[5];

        snprintf(prodid, 5, "%04X", ARDISCOVERY_getProductID(id));
        json_object_object_add(pvato, "product_id", json_object_new_string(prodid));

        if (uuid != NULL) {
            json_object_object_add(pvato, "uuid", json_object_new_string(uuid));
        }

        if (runDate != NULL) {
            json_object_object_add(pvato, "run_date", json_object_new_string(runDate));
        }

        if (filepath != NULL) {
            // get filename from full path
            char* filename = strrchr(filepath, '/');
            if (filename == NULL) // '/' not found in path
                json_object_object_add(pvato, "filename", json_object_new_string(filepath));
            else
                json_object_object_add(pvato, "filename", json_object_new_string(filename + 1));
        }

        if (mediaDate != NULL) {
            strftime(dateBuff, DATETIME_MAXLENGTH, ARMEDIA_JSON_DESCRIPTION_DATE_FMT, mediaDate);
            json_object_object_add(pvato, "media_date", json_object_new_string(dateBuff));
        }

        char *buff = malloc(ARMEDIA_JSON_DESCRIPTION_MAXLENGTH);

        if (buff != NULL) {
            strncpy(buff, (const char*) json_object_to_json_string(pvato), ARMEDIA_JSON_DESCRIPTION_MAXLENGTH);
            buff[ARMEDIA_JSON_DESCRIPTION_MAXLENGTH-1] = '\0'; // safety
        }

        json_object_put(pvato);

        return buff;
    }
    return NULL;
}

