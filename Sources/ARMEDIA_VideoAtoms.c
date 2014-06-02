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

#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

#define ATOM_MALLOC(SIZE) malloc(SIZE)
#define ATOM_CALLOC(NMEMB, SIZE) calloc(NMEMB, SIZE)
#define ATOM_REALLOC(PTR, SIZE) realloc(PTR, SIZE)
#define ATOM_FREE(PTR) free(PTR)
#define ATOM_MEMCOPY(DST, SRC, SIZE) memcpy(DST, SRC, SIZE)

#define TIMESTAMP_FROM_1970_TO_1904 (0x7c25b080U)

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
    uint64_t retValue = 0;
    uint32_t locValue = 0;
    if (1 != fread (&locValue, sizeof (locValue), 1, fptr))
    {
        fprintf (stderr, "Error reading low value of a uint64_t\n");
        return;
    }
    retValue = (uint64_t)(ntohl (locValue));
    if (1 != fread (&locValue, sizeof (locValue), 1, fptr))
    {
        fprintf (stderr, "Error reading a high value of a uint64_t\n");
    }
    retValue += ((uint64_t)(ntohl (locValue))) << 32;
    *dest = retValue;
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
    strncpy (retAtom->tag, tag, 4);

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

movie_atom_t *ftypAtomForFormatAndCodecWithOffset (eARMEDIA_ENCAPSULER_CODEC codec, uint32_t *offset)
{
    uint32_t dataSize;
    if (codec == CODEC_MPEG4_AVC) dataSize = 24;
    else if (codec == CODEC_MOTION_JPEG) dataSize = 20;
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
        ATOM_WRITE_4CC('m', 'p', '4', '2'); // major brand
        ATOM_WRITE_U32(0x00000001); // minor version
        ATOM_WRITE_4CC('m', 'p', '4', '1');
        ATOM_WRITE_4CC('m', 'p', '4', '2');
        ATOM_WRITE_4CC('i', 's', 'o', 'm'); // compatible brands
        *offset = 44;
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
    if (videoSize <= INT32_MAX)
    {
        // Free/wide atom + mdat
        ATOM_WRITE_U32(0x00000000);
        ATOM_WRITE_4CC('m', 'd', 'a', 't');
        uint32_t sizeNe = htonl ((uint32_t) videoSize);
        ATOM_MEMCOPY (data, &sizeNe, sizeof (uint32_t));
        retAtom = atomFromData (dataSize, "wide", data);
        retAtom->size = dataSize;
        retAtom->wide = 1;
    }
    else
    {
        // 64bit wide mdat atom
        uint32_t highSize = (videoSize >> 32);
        uint32_t lowSize = (videoSize & 0xffffffff);
        uint32_t highSizeNe = htonl (highSize);
        uint32_t lowSizeNe = htonl (lowSize);
        ATOM_MEMCOPY (data, &highSizeNe, sizeof (uint32_t));
        ATOM_MEMCOPY (&data[4], &lowSizeNe, sizeof (uint32_t));
        retAtom = atomFromData (dataSize, "mdat", data);
        retAtom->size = 0;
        retAtom->wide = 1;
    }
    return retAtom;
}
movie_atom_t *mvhdAtomFromFpsNumFramesAndDate (uint32_t timescale, uint32_t fps, uint32_t nbFrames, time_t date)
{
    uint32_t dataSize = 100;
    uint8_t *data = (uint8_t*) ATOM_MALLOC (dataSize);
    uint32_t currentIndex = 0;
    movie_atom_t *retAtom;

    if (NULL == data)
    {
        return NULL;
    }

    ATOM_WRITE_U32 (0); /* Version (8) + Flags (24) */
    ATOM_WRITE_U32 (0); /* Creation time */
    ATOM_WRITE_U32 (0); /* Modification time */
    ATOM_WRITE_U32 (timescale); /* Timescale */
    ATOM_WRITE_U32 (timescale * nbFrames / fps); /* Duration (in timescale units) */

    ATOM_WRITE_U32 (0x00010000); /* Reserved */
    ATOM_WRITE_U16 (0x0100); /* Reserved */
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
    ATOM_WRITE_U32 (2); /* Next track id */

    retAtom = atomFromData (100, "mvhd", data);
    ATOM_FREE (data);
    return retAtom;
}


movie_atom_t *tkhdAtomWithResolutionNumFramesFpsAndDate (uint32_t w, uint32_t h, uint32_t nbFrames, uint32_t timescale, uint32_t fps, time_t date)
{
    uint32_t dataSize = 84;
    uint8_t *data = (uint8_t*) ATOM_MALLOC (dataSize);
    uint32_t currentIndex = 0;
    movie_atom_t *retAtom;

    if (NULL == data)
    {
        return NULL;
    }

    ATOM_WRITE_U32 (0x0000000f); /* Version (8) + Flags (24) */
    ATOM_WRITE_U32 (0); /* Creation time */
    ATOM_WRITE_U32 (0); /* Modification time */
    ATOM_WRITE_U32 (1); /* Track ID */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (timescale * nbFrames / fps); /* Duration, in timescale unit */
    ATOM_WRITE_U32 (0); /* Reserved */
    ATOM_WRITE_U32 (0); /* Reserved */
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

    ATOM_WRITE_U16 (w); /* Width */
    ATOM_WRITE_U16 (0); /* Reserved */
    ATOM_WRITE_U16 (h); /* Height*/
    ATOM_WRITE_U16 (0); /* Reserved */

    retAtom = atomFromData (84, "tkhd", data);
    ATOM_FREE (data);
    data = NULL;
    return retAtom;
}

movie_atom_t *mdhdAtomFromFpsNumFramesAndDate (uint32_t timescale, uint32_t fps, uint32_t nbFrames, time_t date)
{
    uint32_t dataSize = 24;
    uint32_t currentIndex = 0;
    uint8_t *data = (uint8_t*) ATOM_MALLOC (dataSize);
    movie_atom_t *retAtom;

    if (NULL == data)
    {
        return NULL;
    }

    ATOM_WRITE_U32 (0); /* Version (8) + Flags (24) */
    ATOM_WRITE_U32 (0); /* Creation time */
    ATOM_WRITE_U32 (0); /* Modification time */
    ATOM_WRITE_U32 (timescale); /* Timescale */
    ATOM_WRITE_U32 (timescale * nbFrames / fps); /* Duration */
    ATOM_WRITE_U32 (0x55c40000); /* Language code (16) + Quality (16) */

    retAtom = atomFromData (24, "mdhd", data);
    ATOM_FREE (data);
    data = NULL;
    return retAtom;
}


movie_atom_t *hdlrAtomForMdia ()
{
    uint8_t data [37] =  {0x00, 0x00, 0x00, 0x00,
                          'm', 'h', 'l', 'r',
                          'v', 'i', 'd', 'e',
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x0c, 'V', 'i', 'd',
                          'e', 'o', 'H', 'a',
                          'n', 'd', 'l', 'e',
                          'r'};
    return atomFromData (37, "hdlr", data);
}

movie_atom_t *vmhdAtomGen ()
{
    uint8_t data [12] = {0x00, 0x00, 0x00, 0x01,
                         0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00};
    return atomFromData (12, "vmhd", data);
}

movie_atom_t *hdlrAtomForMinf ()
{
    uint8_t data [36] =  {0x00, 0x00, 0x00, 0x00,
                          'd', 'h', 'l', 'r',
                          'u', 'r', 'l', ' ',
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x0b, 'D', 'a', 't',
                          'a', 'H', 'a', 'n',
                          'd', 'l', 'e', 'r'};
    return atomFromData (36, "hdlr", data);
}

movie_atom_t *drefAtomGen ()
{
    uint8_t data [20] = {0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x01,
                         0x00, 0x00, 0x00, 0x0C,
                         0x75, 0x72, 0x6c, 0x20,
                         0x00, 0x00, 0x00, 0x01};
    return atomFromData (20, "dref", data);
}

movie_atom_t *stsdAtomWithResolutionAndCodec (uint32_t w, uint32_t h, eARMEDIA_ENCAPSULER_CODEC codec)
{
    uint32_t dataSize;
    uint32_t currentIndex = 0;

    if (CODEC_MPEG4_AVC == codec) {
        dataSize = 128;
    } else if (CODEC_MOTION_JPEG) {
        dataSize = 94; //138;
    } else if (CODEC_MPEG4_VISUAL) {
        dataSize = 187;
    } else {
        return NULL;
    }

    uint8_t* data = (uint8_t*) ATOM_MALLOC(dataSize);


    if (CODEC_MPEG4_AVC == codec)
    {
        ATOM_WRITE_U32(0x00000000);         // 0 - version & flags
        ATOM_WRITE_U32(0x00000001);         // 4 - number of entries

        ATOM_WRITE_U32(0x00000078);         // 8 - size of sample description
        ATOM_WRITE_4CC('a', 'v', 'c', '1'); // 12 - tag for sample description
        ATOM_WRITE_U32(0x00000000);         // 16 -
        ATOM_WRITE_U32(0x00000001);         // 20 -
        ATOM_WRITE_U32(0x00000000);         // 24 - version & revision
        ATOM_WRITE_4CC('A', 'R', '.', 'D'); // 28 - vendor
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
        ATOM_WRITE_4CC('A', 'R', '.', 'D'); // 28 - vendor
        ATOM_WRITE_U32(0x00000000);         // 32 - temporal quality
        ATOM_WRITE_U32(0x00000000);         // 36 - spatial quality
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
        ATOM_WRITE_4CC('A', 'R', '.', 'D'); // 28 - vendor
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
        ATOM_WRITE_4CC('A', 'R', '.', 'D'); // 171 - elementary stream descriptor
        ATOM_WRITE_4CC('r', 'o', 'n', 'e'); // 175 - elementary stream descriptor
        ATOM_WRITE_4CC('_', '3', 6, 0x80);  // 179 - elementary stream descriptor
        ATOM_WRITE_U32(0x80800102);         // 183 - elementary stream descriptor
                                            // 187 -- END
    }
    return atomFromData (dataSize, "stsd", data);
}

movie_atom_t *stsdAtomWithResolutionCodecSpsAndPps (uint32_t w, uint32_t h, eARMEDIA_ENCAPSULER_CODEC codec, uint8_t *sps, uint32_t spsSize, uint8_t *pps, uint32_t ppsSize)
{
    uint32_t avcCSize = 19 + spsSize + ppsSize;
    uint32_t vse_size = avcCSize + 86;
    uint32_t dataSize = vse_size + 8;
    uint8_t *data = (uint8_t*) ATOM_MALLOC (dataSize);
    uint32_t currentIndex = 0;
    movie_atom_t *retAtom;

    if (NULL == sps || NULL == pps || CODEC_MPEG4_AVC != codec)
    {
        return stsdAtomWithResolutionAndCodec (w, h, codec);
    }

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
    ATOM_WRITE_4CC ('A', 'R', '.', 'D'); /* Muxer name */
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

movie_atom_t *sttsAtomWithNumFrames (uint32_t nbFrames, uint32_t timescale, uint32_t fps)
{
    uint8_t data [16];
    uint32_t currentIndex = 0;

    ATOM_WRITE_U32(0);
    ATOM_WRITE_U32(1);
    ATOM_WRITE_U32(nbFrames);
    ATOM_WRITE_U32(timescale/fps);

    return atomFromData (16, "stts", data);
}

movie_atom_t *stscAtomGen ()
{
    uint8_t data [20] = {0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x01,
                         0x00, 0x00, 0x00, 0x01,
                         0x00, 0x00, 0x00, 0x01,
                         0x00, 0x00, 0x00, 0x01};
    return atomFromData (20, "stsc", data);
}

movie_atom_t *metadataAtomFromTagAndValue (const char *tag, const char *value)
{
    movie_atom_t *retAtom = NULL;
    char locTag [4] = {0};
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

    /* Continue only if we got a valid tag */
    if (0 != locTag [0])
    {
        uint16_t valLen = (uint16_t) strlen (value);
        uint16_t langCode = 0x55c4; /* 5-bit ascii for "und", undefined */
        uint32_t currentIndex = 0;
        uint32_t dataSize = valLen + 4;
        uint8_t *data = ATOM_MALLOC (dataSize);
        if (NULL != data)
        {
            ATOM_WRITE_U16 (valLen); /* Length of the value field */
            ATOM_WRITE_U16 (langCode); /* Language code, set to "und" (undefined) */
            ATOM_WRITE_BYTES (value, valLen); /* Actual value */
            retAtom = atomFromData (dataSize, locTag, data);
            ATOM_FREE (data);
            data = NULL;
        }
    }
    return retAtom;
}

movie_atom_t *pvatAtomGen(const char *jsonString) {
    uint32_t dataSize = (uint32_t) strlen(jsonString);

    return atomFromData(dataSize+1, ARMEDIA_VIDEOATOMS_PVAT, jsonString);
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

static int seekMediaFileToAtom (FILE *videoFile, const char *atomName, uint64_t *retAtomSize)
{
    uint32_t atomSize = 0;
    char fourCCTag [5] = {0};
    uint64_t wideAtomSize = 0;
    int found = 0;
    if (NULL == videoFile)
    {
        return 0;
    }

    read_uint32 (videoFile, &atomSize);
    read_4cc (videoFile, fourCCTag);
    if (0 == atomSize)
    {
        read_uint64 (videoFile, &wideAtomSize);
    }
    else
    {
        wideAtomSize = atomSize;
    }
    if (0 == strncmp (fourCCTag, atomName, 4))
    {
        found = 1;
    }

    while (0 == found &&
           !feof (videoFile))
    {
        fseek (videoFile, wideAtomSize - 8, SEEK_CUR);

        read_uint32 (videoFile, &atomSize);
        read_4cc (videoFile, fourCCTag);
        if (0 == atomSize)
        {
            read_uint64 (videoFile, &wideAtomSize);
        }
        else
        {
            wideAtomSize = atomSize;
        }
        if (0 == strncmp (fourCCTag, atomName, 4))
        {
            found = 1;
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

    // Rewind videoFile
    if (NULL != videoFile)
    {
        rewind (videoFile);
    }

    localAtom = strdup(atom);

    token = strtok_r(localAtom, "/", &tokSave);

    while (token != NULL)
    {
        // Seek to atom
        int seekRes = seekMediaFileToAtom (videoFile, token, &atomSize);
        if (0 == seekRes)
        {
            return NULL;
        }
        token = strtok_r(NULL, "/", &tokSave);
    }
    free (localAtom);

    atomSize -= 8; // Remove the [size - tag] part, as it was read during seek

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

    return retBuffer;
}

uint8_t *createDataFromAtom(uint8_t *atomBuffer, const int atomSize)
{
    uint32_t dataSize;
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
    // Rewind videoFile
    if (NULL != videoFile)
    {
        rewind (videoFile);
    }

    // Seek to mdhd atom, following its parents :
    // ROOT:
    // moov
    //  \ trak
    //     \ mdia
    //        \ mdhd <-
    // First seek : moov
    seekRes = seekMediaFileToAtom (videoFile, "moov", NULL);
    if (0 == seekRes)
    {
        valid = 0;
    }
    // Second seek : trak
    if (1 == valid)
    {
        seekRes = seekMediaFileToAtom (videoFile, "trak", NULL);
        if (0 == seekRes)
        {
            valid = 0;
        }
    }
    // Third seek : mdia
    if (1 == valid)
    {
        seekRes = seekMediaFileToAtom (videoFile, "mdia", NULL);
        if (0 == seekRes)
        {
            valid = 0;
        }
    }
    // Final seek : mdhd
    if (1 == valid)
    {
        seekRes = seekMediaFileToAtom (videoFile, "mdhd", &atomSize);
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

int seekMediaBufferToAtom (uint8_t *buff, long long *offset, const char *tag)
{
    int retVal = 0;

    uint32_t atomSize32 = 0;
    uint64_t atomSize = 0;
    uint32_t atomTag = 0;
    memcpy(&atomSize32, buff, sizeof(uint32_t));
    atomSize = (uint64_t)ntohl(atomSize32);
    memcpy(&atomTag, buff + sizeof(uint32_t), sizeof(uint32_t));

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
