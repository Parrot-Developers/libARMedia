/*
 * ARMEDIA_Video_Atoms.c
 * ARDroneLib
 *
 * Created by n.brulez on 19/08/11
 * Copyright 2011 Parrot SA. All rights reserved.
 *
 */

#include "ARMEDIA_Video_Atoms.h"
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
#define ATOM_WRITE_U32(VAL)                                     \
    do                                                          \
    {                                                           \
        uint32_t *DPOINTER = (uint32_t *)&(data[currentIndex]); \
        currentIndex += 4;                                      \
        *DPOINTER = htonl((uint32_t)VAL);                       \
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


static int seekMediaFileToAtom (FILE *videoFile, char *atomName, uint64_t *retAtomSize)
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

prrt_data_t *createPrrtDataFromFile (FILE *videoFile, int *videoNbFrames)
{
    uint64_t atomSize = 0;
    uint8_t *atomBuffer = NULL;
    prrt_data_t *retBuffer = NULL;
    int valid = 1;
    // Rewind videoFile
    if (NULL != videoFile)
    {
        rewind (videoFile);
    }

    // Seek to prrt atom
    int seekRes = seekMediaFileToAtom (videoFile, "prrt", &atomSize);
    if (0 == seekRes)
    {
        if (NULL != videoNbFrames) { *videoNbFrames = 0; }
        return NULL;
    }

    atomSize -= 8; // Remove the [size - tag] part, as it was read during seek

    // Alloc local buffer
    atomBuffer = ATOM_MALLOC (atomSize);
    if (NULL == atomBuffer)
    {
        valid = 0;
    }

    // Read prrt atom from file
    if (1 == valid)
    {
        size_t nbRead = fread (atomBuffer, sizeof (uint8_t), atomSize, videoFile);
        if (atomSize != nbRead)
        {
            valid = 0;
        }
    }

    // Create prrt buffer from prrt atom
    if (1 == valid)
    {
        retBuffer = createPrrtDataFromAtom (atomBuffer, atomSize, videoNbFrames);
    }

    // Free any allocated resource
    if (NULL != atomBuffer) { ATOM_FREE (atomBuffer); }

    return retBuffer;
}

prrt_data_t *createPrrtDataFromAtom (uint8_t *prrtAtomBuffer, const int prrtAtomSize, int *videoNbFrames)
{
    uint16_t prrtVersion;
    uint16_t prrtSize;
    uint32_t nbFrames;
    uint32_t totalPrrtSize;
    uint32_t maxSize = prrtAtomSize;
    uint8_t *pBuffer = prrtAtomBuffer;
    prrt_data_t *retVal = NULL;
    int sizeFailed = 0;
    // Sanity check : ensure that the buffer we got is not null and long enough
    if (NULL == prrtAtomBuffer ||
        12 > prrtAtomSize)
    {
        return NULL;
    }

    // Read prrtData infos
    ATOM_READ_U16 (prrtVersion);
    ATOM_READ_U16 (prrtSize);
    ATOM_READ_U32 (nbFrames);
    if (NULL != videoNbFrames)
    {
        *videoNbFrames = nbFrames;
    }
    ATOM_READ_U32 (totalPrrtSize);

    // Sanity check : remaining buffer size must be ok with the read totalSize
    if (maxSize - 12 /* We already have read 12 bytes */ < totalPrrtSize)
    {
        return NULL;
    }

    // Sanity check : ensure the size of the video prrt_data_vX_t match ours
    switch (prrtVersion)
    {
    case 1:
        if (sizeof (prrt_data_v1_t) != prrtSize)
            sizeFailed = 1;
        break;
    case 2:
        if (sizeof (prrt_data_v2_t) != prrtSize)
            sizeFailed = 1;
        break;
        // Add all known cases after this one

    case 3:
        if (sizeof (prrt_data_v3_t) != prrtSize)
            sizeFailed = 1;
        break;
        // Add all known cases after this one
            
    default: // Unknown version
        sizeFailed = 1;
        break;
    }
    if (1 == sizeFailed)
    {
        return NULL;
    }

    // Buffer and prrt size are ok, alloc return pointer
    retVal = ATOM_CALLOC (nbFrames, sizeof (prrt_data_t));
    if (NULL == retVal)
    {
        return NULL;
    }

    // Start reading prrt infos
    int currFrame;
    switch (prrtVersion) {
    case PRRT_DATA_VERSION:
        // Ideal case : just memcpy
        ATOM_MEMCOPY (retVal, pBuffer, totalPrrtSize);
        break;
    case 1:
        for (currFrame = 0; currFrame < nbFrames; currFrame++)
        {
            // Read obsolete data
            prrt_data_v1_t data;
            ATOM_READ_PRRT_V(1, data);
            // get pointer to output data
            prrt_data_t *out = &retVal[currFrame];
            // Fill output fata with as many data as possible
            out->frameControlState = data.frameControlState;
            out->frameTimestamp = data.frameTimestamp;
            out->frameTheta = data.frameTheta;
            out->framePhi = data.framePhi;
            out->framePsi = data.framePsi;
            // out->framePsiDiscontinuity can't be retreived, let to zero
            // out->frameAltitude can't be retreived, let to INT32_MAX
            out->frameAltitude = INT32_MAX;
        }
        break;

    case 2:
        for (currFrame = 0; currFrame < nbFrames; currFrame++)
        {
            // Read obsolete data
            prrt_data_v2_t data;
            ATOM_READ_PRRT_V(2, data);
            // get pointer to output data
            prrt_data_t *out = &retVal[currFrame];
            // Fill output fata with as many data as possible
            out->frameControlState = data.frameControlState;
            out->frameTimestamp = data.frameTimestamp;
            out->frameTheta = data.frameTheta;
            out->framePhi = data.framePhi;
            out->framePsi = data.framePsi;
            out->framePsiDiscontinuity = data.framePsiDiscontinuity;
            // out->frameAltitude can't be retreived, let to INT32_MAX
            out->frameAltitude = INT32_MAX;
        }
        break;
        // Add older known cases after this one
            
    default:
        // Should never come here
        ATOM_FREE (retVal);
        retVal = NULL;
        break;
    }

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
