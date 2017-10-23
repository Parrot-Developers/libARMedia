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
 * ARMEDIA_Videoencapsuler.c
 * ARDroneLib
 *
 * Created by n.brulez on 18/08/11
 * Copyright 2011 Parrot SA. All rights reserved.
 *
 */

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <utime.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <json-c/json.h>
#include <locale.h>
#include <libARDiscovery/ARDiscovery.h>
#include <libARMedia/ARMedia.h>
#include <libARSAL/ARSAL_Print.h>
#include <libARMedia/ARMEDIA_VideoEncapsuler.h>

#define ENCAPSULER_SMALL_STRING_SIZE    (30)
#define ENCAPSULER_INFODATA_MAX_SIZE    (256)
#define ARMEDIA_ENCAPSULER_TAG          "ARMEDIA Encapsuler"

#define ENCAPSULER_DEBUG_ENABLE (1)
#define ENCAPSULER_LOG_TIMESTAMPS (0)


typedef enum {
    ARMEDIA_UNTIMED_METADATA_KEY_LOCATION = 0,
    ARMEDIA_UNTIMED_METADATA_KEY_ARTIST,
    ARMEDIA_UNTIMED_METADATA_KEY_TITLE,
    ARMEDIA_UNTIMED_METADATA_KEY_DATE,
    ARMEDIA_UNTIMED_METADATA_KEY_COMMENT,
    ARMEDIA_UNTIMED_METADATA_KEY_COPYRIGHT,
    ARMEDIA_UNTIMED_METADATA_KEY_MAKER,
    ARMEDIA_UNTIMED_METADATA_KEY_MODEL,
    ARMEDIA_UNTIMED_METADATA_KEY_VERSION,
    ARMEDIA_UNTIMED_METADATA_KEY_COVER,
    ARMEDIA_UNTIMED_METADATA_KEY_SERIAL,
    ARMEDIA_UNTIMED_METADATA_KEY_MODEL_ID,
    ARMEDIA_UNTIMED_METADATA_KEY_BUILD_ID,
    ARMEDIA_UNTIMED_METADATA_KEY_RUN_ID,
    ARMEDIA_UNTIMED_METADATA_KEY_RUN_DATE,
    ARMEDIA_UNTIMED_METADATA_KEY_PICTURE_HFOV,
    ARMEDIA_UNTIMED_METADATA_KEY_PICTURE_VFOV,
    ARMEDIA_UNTIMED_METADATA_KEY_MAX,
} eARMEDIA_UNTIMED_METADATA_KEY;

static const char *ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_MAX] =
{
    "com.apple.quicktime.location.ISO6709",
    "com.apple.quicktime.artist",
    "com.apple.quicktime.title",
    "com.apple.quicktime.creationdate",
    "com.apple.quicktime.comment",
    "com.apple.quicktime.copyright",
    "com.apple.quicktime.make",
    "com.apple.quicktime.model",
    "com.apple.quicktime.software",
    "com.apple.quicktime.artwork",
    "com.parrot.serial",
    "com.parrot.model.id",
    "com.parrot.build.id",
    "com.parrot.run.id",
    "com.parrot.run.date",
    "com.parrot.picture.hfov",
    "com.parrot.picture.vfov",
};


#if ENCAPSULER_LOG_TIMESTAMPS
FILE* tslogger = NULL;
#include <inttypes.h>
#endif

/* The structure is initialised to an invalid value
   so we won't set any position in videos unless we got a
   valid ARMEDIA_Videoset_gps_infos() call */
typedef struct
{
    double latitude;
    double longitude;
    double altitude;
} ARMEDIA_videoGpsInfos_t;

typedef struct ARMEDIA_Video_t ARMEDIA_Video_t;
typedef struct ARMEDIA_Audio_t ARMEDIA_Audio_t;
typedef struct ARMEDIA_Metadata_t ARMEDIA_Metadata_t;

struct ARMEDIA_VideoEncapsuler_t
{
    // Encapsuler local data
    uint8_t version;
    uint32_t timescale;
    uint8_t got_iframe;
    uint8_t got_audio;
    uint8_t got_metadata;
    ARMEDIA_Video_t* video;
    ARMEDIA_Audio_t* audio;
    ARMEDIA_Metadata_t* metadata;
    time_t creationTime;
    ARMEDIA_Untimed_Metadata_t untimed_metadata;
    uint8_t got_untimed_metadata;
    char thumbnailFilePath[ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE];

    // Atoms datas
    off_t mdatAtomOffset;
    off_t dataOffset;

    // Provided to constructor
    char uuid[UUID_MAXLENGTH];
    char runDate[DATETIME_MAXLENGTH];
    eARDISCOVERY_PRODUCT product;

    // Path gestion
    char metaFilePath [ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE]; // metadata file path
    char dataFilePath [ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE]; // Keep this so we can rename the output file
    char tempFilePath [ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE]; // Keep this so we can rename the output file
    FILE *metaFile;
    FILE *dataFile;

    // additionnal data
    ARMEDIA_videoGpsInfos_t videoGpsInfos;
};

struct ARMEDIA_Metadata_t
{
    uint32_t block_size;
    uint32_t framesCount;
    off_t totalsize;
    uint64_t lastFrameTimestamp;
    char content_encoding[ARMEDIA_ENCAPSULER_METADATA_STSD_INFO_SIZE];
    char mime_format[ARMEDIA_ENCAPSULER_METADATA_STSD_INFO_SIZE];
};

struct ARMEDIA_Audio_t
{
    eARMEDIA_ENCAPSULER_AUDIO_CODEC codec;
    eARMEDIA_ENCAPSULER_AUDIO_FORMAT format;
    uint16_t nchannel;
    uint16_t freq;
    uint32_t defaultSampleDuration; // in usec
    uint32_t sampleCount; // number of samples
    off_t totalsize;

    /* Samples recording values */
    uint64_t lastSampleTimestamp;
    uint64_t theoreticalts;
    uint32_t stscEntries;
    uint32_t lastChunkSize;
};

struct ARMEDIA_Video_t
{
    uint32_t fps;
    uint32_t defaultFrameDuration;
    // Read from frames frameHeader
    eARMEDIA_ENCAPSULER_VIDEO_CODEC codec;
    uint16_t width;
    uint16_t height;
    off_t totalsize;
    // Private datas
    uint32_t framesCount; // Number of frames

    /* H.264 only values */
    uint8_t *sps;
    uint8_t *pps;
    uint16_t spsSize; // full size with headers 0x00000001
    uint16_t ppsSize; // full size with headers 0x00000001

    /* Frames recording values */
    uint64_t lastFrameTimestamp;
    uint64_t firstFrameTimestamp;
};

// File extension for temporary files
#define TEMPFILE_EXT "-encaps.tmp"

// Limit for audio drift. If more, then add encapsuler adds blank.
#define ADRIFT_LIMIT 10000 // usec

#define ENCAPSULER_ERROR(...)                                           \
    do {                                                                \
        ARSAL_PRINT (ARSAL_PRINT_ERROR, ARMEDIA_ENCAPSULER_TAG, "error: " __VA_ARGS__); \
    } while (0)

#ifdef ENCAPSULER_DEBUG_ENABLE
#define ENCAPSULER_DEBUG(...)                                                                               \
    do {                                                                                                    \
        ARSAL_PRINT (ARSAL_PRINT_DEBUG, ARMEDIA_ENCAPSULER_TAG, "debug > " __VA_ARGS__);                    \
    } while (0)
#else
#define ENCAPSULER_DEBUG(...)
#endif

#define ENCAPSULER_CLEANUP(FUNC, PTR)           \
    do                                          \
{                                           \
    if (NULL != PTR)                        \
    {                                       \
        FUNC(PTR);                          \
    }                                       \
    PTR = NULL;                             \
} while (0)

static eARMEDIA_ERROR ARMEDIA_VideoEncapsuler_Cleanup (ARMEDIA_VideoEncapsuler_t **encapsuler, bool rename_tempFile);
static eARMEDIA_ERROR ARMEDIA_FillZeros(FILE* file, uint32_t nBytes);

ARMEDIA_VideoEncapsuler_t *ARMEDIA_VideoEncapsuler_New (const char *mediaPath, int fps, char* uuid, char* runDate, eARDISCOVERY_PRODUCT product, eARMEDIA_ERROR *error)
{
    ARMEDIA_VideoEncapsuler_t *retVideo = NULL;

    if (NULL == error)
    {
        ENCAPSULER_ERROR ("error pointer must not be null");
        return NULL;
    }
    if (NULL == mediaPath)
    {
        ENCAPSULER_ERROR ("mediaPath pointer must not be null");
        *error = ARMEDIA_ERROR_BAD_PARAMETER;
        return NULL;
    }
    if ('\0' == *mediaPath)
    {
        ENCAPSULER_ERROR ("mediaPath must not be empty");
        *error = ARMEDIA_ERROR_BAD_PARAMETER;
        return NULL;
    }

    retVideo = (ARMEDIA_VideoEncapsuler_t*) malloc (sizeof(ARMEDIA_VideoEncapsuler_t));
    if (NULL == retVideo)
    {
        ENCAPSULER_ERROR ("Unable to allocate retVideo");
        *error = ARMEDIA_ERROR_ENCAPSULER;
        return NULL;
    }

    // encapsuler data initialization
    retVideo->version = ARMEDIA_ENCAPSULER_VERSION_NUMBER;
    retVideo->timescale = (uint32_t)(fps * 2000);
    retVideo->got_iframe = 0;
    retVideo->got_audio = 0;
    retVideo->got_metadata = 0;
    retVideo->got_untimed_metadata = 0;
    memset(&retVideo->untimed_metadata, 0, sizeof(ARMEDIA_Untimed_Metadata_t));
    memset(retVideo->thumbnailFilePath, 0, sizeof retVideo->thumbnailFilePath);
    retVideo->video = (ARMEDIA_Video_t*) malloc (sizeof(ARMEDIA_Video_t));
    retVideo->audio = NULL;
    retVideo->metadata = NULL;
    memset(retVideo->video, 0, sizeof(ARMEDIA_Video_t));
    retVideo->creationTime = time (NULL);

    retVideo->mdatAtomOffset = 0;
    retVideo->dataOffset = 0;

    snprintf (retVideo->uuid, UUID_MAXLENGTH, "%s", uuid);
    snprintf (retVideo->runDate, DATETIME_MAXLENGTH, "%s", runDate);
    retVideo->product = product;

    // path and files initialization
#if ENCAPSULER_LOG_TIMESTAMPS
    char loggerpath [ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE];
    snprintf (loggerpath, ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE, "%s-ts.log", mediaPath);
    tslogger = fopen(loggerpath, "w");
#endif
    snprintf (retVideo->metaFilePath, ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE, "%s%s", mediaPath, METAFILE_EXT);
    snprintf (retVideo->tempFilePath, ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE, "%s%s", mediaPath, TEMPFILE_EXT);
    snprintf (retVideo->dataFilePath, ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE, "%s", mediaPath);
    retVideo->metaFile = fopen(retVideo->metaFilePath, "w+b");
    if (NULL == retVideo->metaFile)
    {
        ENCAPSULER_ERROR ("Unable to open file %s for writing", retVideo->metaFilePath);
        *error = ARMEDIA_ERROR_ENCAPSULER;
        free (retVideo);
        retVideo = NULL;
        return NULL;
    }

    retVideo->dataFile = fopen(retVideo->tempFilePath, "w+b");
    if (NULL == retVideo->dataFile)
    {
        ENCAPSULER_ERROR ("Unable to open file %s for writing", mediaPath);
        *error = ARMEDIA_ERROR_ENCAPSULER;
        fclose (retVideo->metaFile);
        free (retVideo);
        retVideo = NULL;
        return NULL;
    }

    // gps data initialization
    retVideo->videoGpsInfos.latitude = 500.0;
    retVideo->videoGpsInfos.longitude = 500.0;
    retVideo->videoGpsInfos.altitude = 500.0;

    // video data initialization
    retVideo->video->fps = (uint32_t)fps;
    retVideo->video->defaultFrameDuration = 1000000 / fps;
    retVideo->video->codec = 0;
    retVideo->video->width = 0;
    retVideo->video->height = 0;
    retVideo->video->totalsize = 0;
    retVideo->video->framesCount = 0;
    retVideo->video->sps = NULL;
    retVideo->video->pps = NULL;
    retVideo->video->spsSize = 0;
    retVideo->video->ppsSize = 0;
    retVideo->video->lastFrameTimestamp = 0;
    retVideo->video->firstFrameTimestamp = 0;

    *error = ARMEDIA_OK;

    return retVideo;
}

#define BYTE_STREAM_NALU_START_CODE 0x00000001

static int ARMEDIA_H264StartcodeMatch(const uint8_t* pBuf, unsigned int bufSize)
{
    int ret, pos, end;
    uint32_t shiftVal = 0;
    const uint8_t* ptr = pBuf;

    if (bufSize < 4) return -2;

    pos = 0;
    end = bufSize;

    do
    {
        shiftVal <<= 8;
        shiftVal |= (*ptr++) & 0xFF;
        pos++;
    }
    while (((shiftVal != BYTE_STREAM_NALU_START_CODE) && (pos < end)) || (pos < 4));

    if (shiftVal == BYTE_STREAM_NALU_START_CODE)
    {
        ret = pos - 4;
    }
    else
    {
        ret = -2;
    }

    return ret;
}

eARMEDIA_ERROR ARMEDIA_VideoEncapsuler_SetAvcParameterSets (ARMEDIA_VideoEncapsuler_t *encapsuler, const uint8_t *sps, uint32_t spsSize, const uint8_t *pps, uint32_t ppsSize)
{
    if (NULL == encapsuler)
    {
        ENCAPSULER_ERROR ("encapsuler pointer must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    }
    if (NULL == sps)
    {
        ENCAPSULER_ERROR ("SPS pointer must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    }
    if (0 == spsSize)
    {
        ENCAPSULER_ERROR ("SPS size must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    }
    if (NULL == pps)
    {
        ENCAPSULER_ERROR ("PPS pointer must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    }
    if (0 == ppsSize)
    {
        ENCAPSULER_ERROR ("PPS pointer must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    }

    ARMEDIA_Video_t* video = encapsuler->video;
    video->spsSize = spsSize;
    video->ppsSize = ppsSize;
    video->sps = malloc (video->spsSize);
    video->pps = malloc (video->ppsSize);

    if (NULL == video->sps || NULL == video->pps)
    {
        ENCAPSULER_ERROR ("Unable to allocate SPS/PPS buffers");
        if (NULL != video->sps)
        {
            free (video->sps);
            video->sps = NULL;
        }

        if (NULL != video->pps)
        {
            free (video->pps);
            video->pps = NULL;
        }
        return ARMEDIA_ERROR_ENCAPSULER;
    }
    memcpy (video->sps, sps, video->spsSize);
    memcpy (video->pps, pps, video->ppsSize);

    return ARMEDIA_OK;
}

eARMEDIA_ERROR ARMEDIA_VideoEncapsuler_SetMetadataInfo (ARMEDIA_VideoEncapsuler_t *encapsuler, const char *content_encoding, const char *mime_format, uint32_t metadata_block_size)
{
    // Init ot metadata structure
    encapsuler->metadata = (ARMEDIA_Metadata_t*) malloc (sizeof(ARMEDIA_Metadata_t));
    if (encapsuler->metadata != NULL)
    {
        encapsuler->got_metadata = 1;
    }
    else
    {
        ENCAPSULER_ERROR ("Unable to allocate metadata structure");
        return ARMEDIA_ERROR_ENCAPSULER;
    }

    encapsuler->metadata->framesCount = 0;
    encapsuler->metadata->totalsize = 0;
    encapsuler->metadata->lastFrameTimestamp = 0;

    snprintf (encapsuler->metadata->content_encoding,
            sizeof(encapsuler->metadata->content_encoding), "%s", content_encoding);

    snprintf (encapsuler->metadata->mime_format,
            sizeof(encapsuler->metadata->mime_format), "%s", mime_format);

    encapsuler->metadata->block_size = metadata_block_size;

    return ARMEDIA_OK;
}

eARMEDIA_ERROR ARMEDIA_VideoEncapsuler_SetUntimedMetadata (ARMEDIA_VideoEncapsuler_t *encapsuler, const ARMEDIA_Untimed_Metadata_t *metadata)
{
    int i, k;
    if (!encapsuler)
    {
        return ARMEDIA_ERROR_ENCAPSULER;
    }
    if (!metadata)
    {
        return ARMEDIA_ERROR_ENCAPSULER;
    }

    if (strlen(metadata->maker))
    {
        snprintf(encapsuler->untimed_metadata.maker,
                 ARMEDIA_ENCAPSULER_UNTIMED_METADATA_MAKER_SIZE, "%s",
                 metadata->maker);
    }
    if (strlen(metadata->model))
    {
        snprintf(encapsuler->untimed_metadata.model,
                 ARMEDIA_ENCAPSULER_UNTIMED_METADATA_MODEL_SIZE, "%s",
                 metadata->model);
    }
    if (strlen(metadata->modelId))
    {
        snprintf(encapsuler->untimed_metadata.modelId,
                 ARMEDIA_ENCAPSULER_UNTIMED_METADATA_MODEL_ID_SIZE, "%s",
                 metadata->modelId);
    }
    if (strlen(metadata->serialNumber))
    {
        snprintf(encapsuler->untimed_metadata.serialNumber,
                 ARMEDIA_ENCAPSULER_UNTIMED_METADATA_SERIAL_NUM_SIZE, "%s",
                 metadata->serialNumber);
    }
    if (strlen(metadata->softwareVersion))
    {
        snprintf(encapsuler->untimed_metadata.softwareVersion,
                 ARMEDIA_ENCAPSULER_UNTIMED_METADATA_SOFT_VER_SIZE, "%s",
                 metadata->softwareVersion);
    }
    if (strlen(metadata->buildId))
    {
        snprintf(encapsuler->untimed_metadata.buildId,
                 ARMEDIA_ENCAPSULER_UNTIMED_METADATA_BUILD_ID_SIZE, "%s",
                 metadata->buildId);
    }
    if (strlen(metadata->artist))
    {
        snprintf(encapsuler->untimed_metadata.artist,
                 ARMEDIA_ENCAPSULER_UNTIMED_METADATA_ARTIST_SIZE, "%s",
                 metadata->artist);
    }
    if (strlen(metadata->title))
    {
        snprintf(encapsuler->untimed_metadata.title,
                 ARMEDIA_ENCAPSULER_UNTIMED_METADATA_TITLE_SIZE, "%s",
                 metadata->title);
    }
    if (strlen(metadata->comment))
    {
        snprintf(encapsuler->untimed_metadata.comment,
                 ARMEDIA_ENCAPSULER_UNTIMED_METADATA_COMMENT_SIZE, "%s",
                 metadata->comment);
    }
    if (strlen(metadata->copyright))
    {
        snprintf(encapsuler->untimed_metadata.copyright,
                 ARMEDIA_ENCAPSULER_UNTIMED_METADATA_COPYRIGHT_SIZE, "%s",
                 metadata->copyright);
    }
    if (strlen(metadata->mediaDate))
    {
        snprintf(encapsuler->untimed_metadata.mediaDate,
                 ARMEDIA_ENCAPSULER_UNTIMED_METADATA_MEDIA_DATE_SIZE, "%s",
                 metadata->mediaDate);
    }
    if (strlen(metadata->runDate))
    {
        snprintf(encapsuler->untimed_metadata.runDate,
                 ARMEDIA_ENCAPSULER_UNTIMED_METADATA_RUN_DATE_SIZE, "%s",
                 metadata->runDate);
    }
    if (strlen(metadata->runUuid))
    {
        snprintf(encapsuler->untimed_metadata.runUuid,
                 ARMEDIA_ENCAPSULER_UNTIMED_METADATA_RUN_UUID_SIZE, "%s",
                 metadata->runUuid);
    }
    encapsuler->untimed_metadata.takeoffLatitude = metadata->takeoffLatitude;
    encapsuler->untimed_metadata.takeoffLongitude = metadata->takeoffLongitude;
    encapsuler->untimed_metadata.takeoffAltitude = metadata->takeoffAltitude;
    encapsuler->untimed_metadata.pictureHFov = metadata->pictureHFov;
    encapsuler->untimed_metadata.pictureVFov = metadata->pictureVFov;
    for (i = 0, k = 0; i < ARMEDIA_ENCAPSULER_UNTIMED_METADATA_CUSTOM_MAX_COUNT; i++)
    {
        if (strlen(metadata->custom[i].key) && strlen(metadata->custom[i].value))
        {
            snprintf(encapsuler->untimed_metadata.custom[k].key,
                     ARMEDIA_ENCAPSULER_UNTIMED_METADATA_CUSTOM_KEY_SIZE, "%s",
                     metadata->custom[i].key);
            snprintf(encapsuler->untimed_metadata.custom[k].value,
                     ARMEDIA_ENCAPSULER_UNTIMED_METADATA_CUSTOM_VALUE_SIZE, "%s",
                     metadata->custom[i].value);
            k++;
        }
    }
    encapsuler->got_untimed_metadata = 1;

    return ARMEDIA_OK;
}

eARMEDIA_ERROR ARMEDIA_VideoEncapsuler_SetVideoThumbnail (ARMEDIA_VideoEncapsuler_t *encapsuler, const char *file)
{
    if (!encapsuler)
    {
        return ARMEDIA_ERROR_ENCAPSULER;
    }
    if (!file)
    {
        return ARMEDIA_ERROR_ENCAPSULER;
    }

    snprintf(encapsuler->thumbnailFilePath,
             ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE, "%s",
             file);

    return ARMEDIA_OK;
}

eARMEDIA_ERROR ARMEDIA_VideoEncapsuler_AddFrame (ARMEDIA_VideoEncapsuler_t *encapsuler, ARMEDIA_Frame_Header_t *frameHeader, const void *metadataBuffer)
{
    uint8_t searchIndex;
    uint32_t descriptorSize;
    movie_atom_t *ftypAtom;

    if (NULL == encapsuler)
    {
        ENCAPSULER_ERROR ("encapsuler pointer must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    }

    ARMEDIA_Video_t* video = encapsuler->video;
    if (NULL == video)
    {
        ENCAPSULER_ERROR ("video pointer must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    }
    if (NULL == frameHeader)
    {
        ENCAPSULER_ERROR ("frame pointer must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    }

    // get frame data
    if ((NULL == frameHeader->frame) && (0 == frameHeader->avc_nalu_count))
    {
        ENCAPSULER_ERROR ("Unable to get frame data (%d bytes) ", frameHeader->frame_size);
        return ARMEDIA_ERROR_ENCAPSULER;
    }

    if ((NULL != frameHeader->frame) && (!frameHeader->frame_size))
    {
        // Do nothing
        ENCAPSULER_DEBUG ("Empty frame\n");
        return ARMEDIA_OK;
    }

    if (ARMEDIA_ENCAPSULER_FRAMES_COUNT_LIMIT <= video->framesCount)
    {
        ENCAPSULER_ERROR ("Video contains already %d frames, which is the maximum", ARMEDIA_ENCAPSULER_FRAMES_COUNT_LIMIT);
        return ARMEDIA_ERROR_ENCAPSULER;
    }

    // First frame
    if (!video->width)
    {
        // Ensure that the codec is h.264 or mjpeg (mp4 not supported)
        if ((CODEC_MPEG4_AVC != frameHeader->codec) && (CODEC_MOTION_JPEG != frameHeader->codec))
        {
            ENCAPSULER_ERROR ("Only h.264 or mjpeg codec are supported");
            return ARMEDIA_ERROR_ENCAPSULER_BAD_CODEC;
        }

        // Init video structure
        video->width = frameHeader->width;
        video->height = frameHeader->height;
        video->codec = frameHeader->codec;

        // If codec is H.264, save SPS/PPS
        if (CODEC_MPEG4_AVC == video->codec)
        {
            if (ARMEDIA_ENCAPSULER_FRAME_TYPE_I_FRAME != frameHeader->frame_type)
            {
                // Wait until we get an IFrame 1 to start the actual recording
                video->width = video->height = frameHeader->codec = 0;
                return ARMEDIA_ERROR_ENCAPSULER_WAITING_FOR_IFRAME;
            }

            if ((NULL == video->sps) && (NULL == video->pps))
            {
                const uint8_t *pSps = NULL, *pPps = NULL;
                if (frameHeader->avc_nalu_count)
                {
                    // We consider that on a I-Frame the first NALU is an SPS and the second NALU is a PPS
                    video->spsSize = frameHeader->avc_nalu_size[0];
                    video->ppsSize = frameHeader->avc_nalu_size[1];
                    if (frameHeader->frame)
                    {
                        pSps = frameHeader->frame;
                        pPps = frameHeader->frame + video->spsSize;
                    }
                    else
                    {
                        pSps = frameHeader->avc_nalu_data[0];
                        pPps = frameHeader->avc_nalu_data[1];
                    }
                }
                else
                {
                    // we'll need to search the "00 00 00 01" pattern to find each header size
                    // Search start at index 4 to avoid finding the SPS "00 00 00 01" tag
                    for (searchIndex = 4; searchIndex <= frameHeader->frame_size - 4; searchIndex ++)
                    {
                        if (0 == frameHeader->frame[searchIndex  ] &&
                                0 == frameHeader->frame[searchIndex+1] &&
                                0 == frameHeader->frame[searchIndex+2] &&
                                1 == frameHeader->frame[searchIndex+3])
                        {
                            break;  // PPS header found
                        }
                    }
                    video->spsSize = searchIndex;
                    pSps = frameHeader->frame;

                    // Search start at index 4 to avoid finding the SPS "00 00 00 01" tag
                    for (searchIndex = video->spsSize+4; searchIndex <= frameHeader->frame_size - 4; searchIndex ++)
                    {
                        if (0 == frameHeader->frame[searchIndex  ] &&
                                0 == frameHeader->frame[searchIndex+1] &&
                                0 == frameHeader->frame[searchIndex+2] &&
                                1 == frameHeader->frame[searchIndex+3])
                        {
                            break;  // frame header found
                        }
                    }
                    video->ppsSize = searchIndex - video->spsSize;
                    pPps = frameHeader->frame + video->spsSize;
                }

                video->sps = malloc (video->spsSize);
                video->pps = malloc (video->ppsSize);

                if (NULL == video->sps || NULL == video->pps)
                {
                    ENCAPSULER_ERROR ("Unable to allocate SPS/PPS buffers");
                    if (NULL != video->sps)
                    {
                        free (video->sps);
                        video->sps = NULL;
                    }

                    if (NULL != video->pps)
                    {
                        free (video->pps);
                        video->pps = NULL;
                    }
                    return ARMEDIA_ERROR_ENCAPSULER;
                }
                memcpy (video->sps, pSps, video->spsSize);
                memcpy (video->pps, pPps, video->ppsSize);
            }
        }

        // Start to write file
        rewind(encapsuler->dataFile);
        ftypAtom = ftypAtomForFormatAndCodecWithOffset (video->codec, &(encapsuler->dataOffset));
        if (NULL == ftypAtom)
        {
            ENCAPSULER_ERROR ("Unable to create ftyp atom");
            return ARMEDIA_ERROR_ENCAPSULER;
        }

        if (-1 == writeAtomToFile (&ftypAtom, encapsuler->dataFile))
        {
            ENCAPSULER_ERROR ("Unable to write ftyp atom");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }

        // Add an offset for PVAT at beginning
        encapsuler->dataOffset += ARMEDIA_JSON_DESCRIPTION_MAXLENGTH+8;

        if (-1 == fseeko(encapsuler->dataFile, encapsuler->dataOffset, SEEK_SET))
        {
            ENCAPSULER_ERROR ("Unable to set file write pointer to %zu", (size_t)encapsuler->dataOffset);
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
        encapsuler->mdatAtomOffset = encapsuler->dataOffset - 16;

        // Write video infos to info file header
        descriptorSize = sizeof(ARMEDIA_VideoEncapsuler_t) +
            sizeof(ARMEDIA_Video_t) + sizeof(ARMEDIA_Audio_t) + sizeof(ARMEDIA_Metadata_t);
        if (video->codec == CODEC_MPEG4_AVC) descriptorSize += video->spsSize + video->ppsSize;

        // Write total length
        if (1 != fwrite (&descriptorSize, sizeof (uint32_t), 1, encapsuler->metaFile))
        {
            ENCAPSULER_ERROR ("Unable to write size of video descriptor");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
        // Write VideoEncapsuler info
        if (1 != fwrite (encapsuler, sizeof (ARMEDIA_VideoEncapsuler_t), 1, encapsuler->metaFile))
        {
            ENCAPSULER_ERROR ("Unable to write encapsuler descriptor");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
        // Write Video info
        if (1 != fwrite (video, sizeof (ARMEDIA_Video_t), 1, encapsuler->metaFile))
        {
            ENCAPSULER_ERROR ("Unable to write video descriptor");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }

        if (video->codec == CODEC_MPEG4_AVC)
        {
            // Write SPS
            if (video->spsSize != fwrite (video->sps, sizeof (uint8_t), video->spsSize, encapsuler->metaFile))
            {
                ENCAPSULER_ERROR ("Unable to write sps header");
                return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
            // Write PPS
            if (video->ppsSize != fwrite (video->pps, sizeof (uint8_t), video->ppsSize, encapsuler->metaFile))
            {
                ENCAPSULER_ERROR ("Unable to write pps header");
                return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
        }

        if (metadataBuffer != NULL && encapsuler->metadata != NULL && encapsuler->metadata->block_size > 0)
        {
            // Write Metadata info
            if (1 != fwrite (encapsuler->metadata, sizeof (ARMEDIA_Metadata_t), 1, encapsuler->metaFile))
            {
                ENCAPSULER_ERROR ("Unable to write video descriptor");
                return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
        }
        else
        {
            fseeko(encapsuler->metaFile, (off_t)sizeof(ARMEDIA_Metadata_t), SEEK_CUR);
        }

        fseeko(encapsuler->metaFile, (off_t)sizeof(ARMEDIA_Audio_t), SEEK_CUR);
        encapsuler->got_iframe = 1;
        video->firstFrameTimestamp = frameHeader->timestamp;
    } // end first frame

    // Normal operation : file pointer is at end of file
    if (video->codec != frameHeader->codec ||
        video->width != frameHeader->width ||
        video->height != frameHeader->height)
    {
        ENCAPSULER_ERROR ("New frame don't match the video size/codec");
        return ARMEDIA_ERROR_ENCAPSULER_BAD_VIDEO_FRAME;
    }

    // New frame, write all infos about last one
    // if frame TS is null, set it as last TS + default duration (from fps)
    if (frameHeader->timestamp == 0) {
        frameHeader->timestamp = video->lastFrameTimestamp + video->defaultFrameDuration;
    }
    // For first frame, no previous timestamp => dt = 0
    if (video->lastFrameTimestamp == 0) {
        video->lastFrameTimestamp = frameHeader->timestamp;
    }

    ARMEDIA_Metadata_t *metadata = NULL;
    if (encapsuler->metadata != NULL && encapsuler->metadata->block_size > 0)
    {
        metadata = encapsuler->metadata;

        // Same as for video, no previous timestamp => dt = 0
        if (metadataBuffer != NULL && metadata->lastFrameTimestamp == 0) {
            metadata->lastFrameTimestamp = frameHeader->timestamp;
        }
    }

    // Use frameHeader->frame_type to check that we don't write infos about a null frame

    // synchronisation every 10 frames in MJPEG or else before new I-Frames
    if ((video->codec == CODEC_MOTION_JPEG && (video->framesCount % 10) == 0) ||
	frameHeader->frame_type == ARMEDIA_ENCAPSULER_FRAME_TYPE_I_FRAME)
    {
        fflush (encapsuler->dataFile);
        fsync(fileno(encapsuler->dataFile));
        fflush (encapsuler->metaFile);
        fsync(fileno(encapsuler->metaFile));
    }

    if (ARMEDIA_ENCAPSULER_FRAME_TYPE_UNKNNOWN != frameHeader->frame_type)
    {
        uint32_t infoLen;
        off_t totalFrameSize = 0;
        if (frameHeader->frame)
        {
            totalFrameSize += frameHeader->frame_size;
        }
        else
        {
            uint32_t i;
            for (i = 0; i < frameHeader->avc_nalu_count; i++)
            {
                totalFrameSize += frameHeader->avc_nalu_size[i];
            }
        }
        if (frameHeader->avc_insert_ps)
        {
            if (video->spsSize > 4)
                totalFrameSize += video->spsSize;
            if (video->ppsSize > 4)
                totalFrameSize += video->ppsSize;
        }

        char infoData [ENCAPSULER_INFODATA_MAX_SIZE] = {0};
        char fTypeChar = 'p';
        if (ARMEDIA_ENCAPSULER_FRAME_TYPE_I_FRAME == frameHeader->frame_type ||
                ARMEDIA_ENCAPSULER_FRAME_TYPE_JPEG == frameHeader->frame_type)
        {
            fTypeChar = 'i';
        }
        snprintf (infoData, ENCAPSULER_INFODATA_MAX_SIZE,
                ARMEDIA_ENCAPSULER_INFO_PATTERN,
                ARMEDIA_ENCAPSULER_VIDEO_INFO_TAG,
                (long long)totalFrameSize,
                fTypeChar,
                (uint32_t)(frameHeader->timestamp - video->lastFrameTimestamp)); // frame duration
        infoLen = strlen(infoData);

        if (infoLen != fwrite (infoData, 1, infoLen, encapsuler->metaFile))
        {
            ENCAPSULER_ERROR ("Unable to write frameInfo into info file");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }

        if (metadataBuffer != NULL && metadata != NULL && metadata->block_size > 0)
        {
            snprintf (infoData, ENCAPSULER_INFODATA_MAX_SIZE,
                    ARMEDIA_ENCAPSULER_INFO_PATTERN,
                    ARMEDIA_ENCAPSULER_METADATA_INFO_TAG,
                    (long long)metadata->block_size,
                    fTypeChar, // or always 'p'?
                    (uint32_t)(frameHeader->timestamp - metadata->lastFrameTimestamp));
            infoLen = strlen(infoData);

            if (infoLen != fwrite (infoData, 1, infoLen, encapsuler->metaFile))
            {
                ENCAPSULER_ERROR ("Unable to write metadataInfo into info file");
                return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }

            metadata->lastFrameTimestamp = frameHeader->timestamp;
            metadata->framesCount++;
        }
    }

    video->lastFrameTimestamp = frameHeader->timestamp;
    video->framesCount++;

    if (frameHeader->avc_insert_ps)
    {
        // Force insertion of SPS and PPS before this frame
        uint32_t naluSize, naluSizeNe;
        if (video->spsSize > 4)
        {
            naluSize = video->spsSize - 4;
            naluSizeNe = htonl(naluSize);
            if (4 != fwrite (&naluSizeNe, 1, 4, encapsuler->dataFile))
            {
                ENCAPSULER_ERROR ("Unable to write SPS into data file");
                return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
            if (naluSize != fwrite (video->sps + 4, 1, naluSize, encapsuler->dataFile))
            {
                ENCAPSULER_ERROR ("Unable to write SPS into data file");
                return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
            video->totalsize += video->spsSize;
        }
        if (video->ppsSize > 4)
        {
            naluSize = video->ppsSize - 4;
            naluSizeNe = htonl(naluSize);
            if (4 != fwrite (&naluSizeNe, 1, 4, encapsuler->dataFile))
            {
                ENCAPSULER_ERROR ("Unable to write PPS into data file");
                return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
            if (naluSize != fwrite (video->pps + 4, 1, naluSize, encapsuler->dataFile))
            {
                ENCAPSULER_ERROR ("Unable to write PPS into data file");
                return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
            video->totalsize += video->ppsSize;
        }
    }

    // Write the frame to the data file (and replace the NAL units start code by the NALU size in case of H.264)
    if ((video->codec == CODEC_MPEG4_AVC) && (frameHeader->avc_nalu_count > 0))
    {
        uint32_t i, offset, naluSize, naluSizeNE;
        for (i = 0, offset = 0; i < frameHeader->avc_nalu_count; i++)
        {
            naluSize = (frameHeader->avc_nalu_size[i] > 4) ? frameHeader->avc_nalu_size[i] - 4 : 0;
            naluSizeNE = htonl(naluSize);
            if (fwrite(&naluSizeNE, 4, 1, encapsuler->dataFile) != 1)
            {
                ENCAPSULER_ERROR ("Unable to write frame into data file");
                return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
            if (frameHeader->frame)
            {
                if (fwrite(frameHeader->frame + offset + 4, naluSize, 1, encapsuler->dataFile) != 1)
                {
                    ENCAPSULER_ERROR ("Unable to write frame into data file");
                    return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
                }
            }
            else if (frameHeader->avc_nalu_data[i])
            {
                if (fwrite(frameHeader->avc_nalu_data[i] + 4, naluSize, 1, encapsuler->dataFile) != 1)
                {
                    ENCAPSULER_ERROR ("Unable to write frame into data file");
                    return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
                }
            }
            else
            {
                ENCAPSULER_ERROR ("No valid pointer for NALU");
            }
            offset += frameHeader->avc_nalu_size[i];
            video->totalsize += frameHeader->avc_nalu_size[i];
        }
    }
    else
    {
        if (video->codec == CODEC_MPEG4_AVC)
        {
            int startCodePos = 0, naluStart = 0, naluEnd = 0, sizeLeft = frameHeader->frame_size;
            uint32_t naluSize, naluSizeNE;
            startCodePos = ARMEDIA_H264StartcodeMatch(frameHeader->frame, sizeLeft);
            naluStart = startCodePos;
            while (startCodePos >= 0)
            {
                sizeLeft = frameHeader->frame_size - naluStart - 4;
                startCodePos = ARMEDIA_H264StartcodeMatch(frameHeader->frame + naluStart + 4, sizeLeft);
                if (startCodePos >= 0)
                {
                    naluEnd = naluStart + 4 + startCodePos;
                }
                else
                {
                    naluEnd = frameHeader->frame_size;
                }
                naluSize = naluEnd - naluStart - 4;
                naluSizeNE = htonl(naluSize);
                if (fwrite(&naluSizeNE, 4, 1, encapsuler->dataFile) != 1)
                {
                    ENCAPSULER_ERROR ("Unable to write frame into data file");
                    return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
                }
                if (fwrite(frameHeader->frame + naluStart + 4, naluSize, 1, encapsuler->dataFile) != 1)
                {
                    ENCAPSULER_ERROR ("Unable to write frame into data file");
                    return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
                }
                naluStart = naluEnd;
            }
        }
        else
        {
            if (frameHeader->frame_size != fwrite(frameHeader->frame, 1, frameHeader->frame_size, encapsuler->dataFile))
            {
                ENCAPSULER_ERROR ("Unable to write frame into data file");
                return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
        }
        video->totalsize += frameHeader->frame_size;
    }

    if (metadataBuffer != NULL && metadata != NULL && metadata->block_size > 0)
    {
        if (metadata->block_size != fwrite (metadataBuffer, 1, metadata->block_size, encapsuler->dataFile))
        {
            ENCAPSULER_ERROR ("Unable to write metadata into file");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
        metadata->totalsize += metadata->block_size;
    }

#if ENCAPSULER_LOG_TIMESTAMPS
    fprintf(tslogger, "V;%"PRIu64"\n", frameHeader->timestamp);
#endif

    return ARMEDIA_OK;
}

#define ZBUFF_SIZE 1024 // <=> 32 ms of sound
static eARMEDIA_ERROR ARMEDIA_FillZeros(FILE* file, uint32_t nBytes)
{
    uint8_t zbuff[ZBUFF_SIZE] = {0};

    uint32_t rest = nBytes % ZBUFF_SIZE;
    uint32_t quotient = (nBytes - rest) / ZBUFF_SIZE;
    uint32_t i;

    for (i=0 ; i<quotient ; i++) {
        if (ZBUFF_SIZE != fwrite(zbuff, sizeof(uint8_t), ZBUFF_SIZE, file)) {
            goto fillzeros_error;
        }
    }

    // write rest
    if (rest) {
        if (rest != fwrite(zbuff, sizeof(uint8_t), rest, file)) {
            goto fillzeros_error;
        }
    }

    return ARMEDIA_OK;
fillzeros_error:
    return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
}

eARMEDIA_ERROR ARMEDIA_VideoEncapsuler_AddSample (ARMEDIA_VideoEncapsuler_t *encapsuler, ARMEDIA_Sample_Header_t *sampleHeader)
{
    uint8_t *data;
    if (NULL == encapsuler)
    {
        ENCAPSULER_ERROR ("encapsuler pointer must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    }

    if (encapsuler->got_iframe == 0)
    {
        ENCAPSULER_DEBUG("Waiting for Iframe");
        return ARMEDIA_ERROR_ENCAPSULER_WAITING_FOR_IFRAME;
    }

    ARMEDIA_Audio_t* audio;
    if (NULL == sampleHeader)
    {
        ENCAPSULER_ERROR ("sample pointer must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    }

    if (!sampleHeader->sample_size)
    {
        // Do nothing
        ENCAPSULER_DEBUG ("Empty sample\n");
        return ARMEDIA_OK;
    }

    // check sample data
    if (NULL == sampleHeader->sample)
    {
        ENCAPSULER_ERROR ("Unable to get sample data (%d bytes)", sampleHeader->sample_size);
        return ARMEDIA_ERROR_ENCAPSULER;
    }

    data = sampleHeader->sample;

    // First sample
    if ((encapsuler->got_audio == 0) && (encapsuler->audio == NULL))
    {
        // Init audio data
        encapsuler->audio = (ARMEDIA_Audio_t*) malloc (sizeof(ARMEDIA_Audio_t));
        if (encapsuler->audio != NULL) {
            encapsuler->got_audio = 1;
        } else {
            ENCAPSULER_ERROR ("Unable to allocate audio structure");
            return ARMEDIA_ERROR_ENCAPSULER;
        }
        encapsuler->audio->format = sampleHeader->format;
        encapsuler->audio->codec = sampleHeader->codec;
        encapsuler->audio->nchannel = sampleHeader->nchannel;
        encapsuler->audio->freq = sampleHeader->frequency;
        encapsuler->audio->defaultSampleDuration = ((uint64_t)sampleHeader->sample_size*8000000 / (sampleHeader->nchannel*sampleHeader->format)) / sampleHeader->frequency;
        encapsuler->audio->sampleCount = 0;
        encapsuler->audio->totalsize = 0;
        encapsuler->audio->lastSampleTimestamp = 0;
        encapsuler->audio->theoreticalts = encapsuler->video->firstFrameTimestamp;
        encapsuler->audio->stscEntries = 0;
        encapsuler->audio->lastChunkSize = 0;

        // Rewrite encapsuler info
        fseeko(encapsuler->metaFile, (off_t)sizeof(uint32_t), SEEK_SET);
        if (1 != fwrite (encapsuler, sizeof(ARMEDIA_VideoEncapsuler_t), 1, encapsuler->metaFile))
        {
            ENCAPSULER_ERROR ("Unable to rewrite encapsuler descriptor");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
        // Write Audio info
        off_t offset = sizeof(uint32_t) /*descriptor*/ + sizeof(ARMEDIA_VideoEncapsuler_t) + sizeof(ARMEDIA_Video_t) + sizeof(ARMEDIA_Metadata_t);
        offset += encapsuler->video->spsSize + encapsuler->video->ppsSize;
        fseeko(encapsuler->metaFile, offset, SEEK_SET);
        if (1 != fwrite (encapsuler->audio, sizeof (ARMEDIA_Audio_t), 1, encapsuler->metaFile))
        {
            ENCAPSULER_ERROR ("Unable to write audio descriptor");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
        fseeko(encapsuler->metaFile, 0, SEEK_END); // return to the end of file
    }

    audio = encapsuler->audio;
    if ((encapsuler->got_audio != 0) && (audio != NULL))
    {
        // Write audio data

        // if frame TS is null, set it as last TS + default duration (from fps)
        if (sampleHeader->timestamp == 0) {
            sampleHeader->timestamp = audio->lastSampleTimestamp + audio->defaultSampleDuration;
        }
        // For first frame, no previous timestamp => dt = 0
        if (audio->lastSampleTimestamp == 0) {
            audio->lastSampleTimestamp = sampleHeader->timestamp;
        }

#if ENCAPSULER_LOG_TIMESTAMPS
        fprintf(tslogger, "A;%"PRIu64"\n", sampleHeader->timestamp);
#endif
        uint32_t newSize = sampleHeader->sample_size;
        uint8_t* newData = data;

        // synchronisation
        int64_t tsdiff = sampleHeader->timestamp - audio->theoreticalts;
        int32_t zlen = 0;
        if (tsdiff > ADRIFT_LIMIT) {
            ENCAPSULER_DEBUG("Audio drift too high (%"PRId64"µs) on %uth sample\n", tsdiff, audio->sampleCount);
            audio->theoreticalts += tsdiff;
            zlen = (tsdiff * audio->freq / 1000000) * (audio->nchannel * audio->format / 8);
            eARMEDIA_ERROR error = ARMEDIA_FillZeros(encapsuler->dataFile, zlen);
            if (error != ARMEDIA_OK) {
                ENCAPSULER_ERROR ("Unable to write zeros into data file");
                return error;
            }
        }
        else if (tsdiff < -ADRIFT_LIMIT)
        {
            ENCAPSULER_DEBUG("Audio drift too low (%"PRId64"µs) on %uth sample\n", tsdiff, audio->sampleCount);
            audio->theoreticalts += tsdiff;
            zlen = (tsdiff * audio->freq / 1000000) * (audio->nchannel * audio->format / 8);
            if (((uint32_t)-zlen) >= sampleHeader->sample_size) {
                ENCAPSULER_ERROR ("Timestamp anterior to previous sample");
                return ARMEDIA_ERROR_ENCAPSULER_BAD_TIMESTAMP;
            }
            newSize += zlen;
            newData -= zlen;
        }
        uint32_t newChunkSize = sampleHeader->sample_size + zlen;

        if (audio->lastChunkSize != newChunkSize) {
            audio->lastChunkSize = newChunkSize;
            audio->stscEntries++;
        }

        uint32_t infoLen;
        char infoData [ENCAPSULER_INFODATA_MAX_SIZE] = {0};
        snprintf (infoData, ENCAPSULER_INFODATA_MAX_SIZE, ARMEDIA_ENCAPSULER_INFO_PATTERN,
                ARMEDIA_ENCAPSULER_AUDIO_INFO_TAG,
                (long long)newChunkSize,
                'm',
                (uint32_t)(sampleHeader->timestamp - audio->lastSampleTimestamp)); // Chunk duration (in usec)
        infoLen = strlen (infoData);
        if (infoLen != fwrite (infoData, 1, infoLen, encapsuler->metaFile))
        {
            ENCAPSULER_ERROR ("Unable to write sampleInfo into info file");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }

        audio->theoreticalts += audio->defaultSampleDuration;
        audio->lastSampleTimestamp = sampleHeader->timestamp;
        audio->sampleCount++;

        if (newSize != fwrite (newData, 1, newSize, encapsuler->dataFile))
        {
            ENCAPSULER_ERROR ("Unable to write sample into data file");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }

        audio->totalsize += sampleHeader->sample_size + zlen;
    }
    else
    {
        // issue
        ENCAPSULER_ERROR ("Issue while getting audio sample (%u - %p)\n", encapsuler->got_audio, encapsuler->audio);
        return ARMEDIA_ERROR_ENCAPSULER;
    }

    return ARMEDIA_OK;
}

eARMEDIA_ERROR ARMEDIA_VideoEncapsuler_Finish (ARMEDIA_VideoEncapsuler_t **encapsuler)
{
    eARMEDIA_ERROR localError = ARMEDIA_OK;
    ARMEDIA_VideoEncapsuler_t* encaps = NULL;
    ARMEDIA_Video_t *video = NULL;
    ARMEDIA_Audio_t *audio = NULL;
    ARMEDIA_Metadata_t *metadata = NULL;

    // Create internal buffers
    // video
    uint32_t *frameSizeBufferNE   = NULL;
    uint64_t *videoOffsetBuffer   = NULL;
    uint32_t *frameTimeSyncBuffer = NULL;
    uint32_t *metadataTimeSyncBuffer = NULL;
    uint32_t *iFrameIndexBuffer   = NULL;
    // metadata
    uint64_t *metadataOffsetBuffer = NULL;
    // audio
    uint64_t *audioOffsetBuffer = NULL;
    uint32_t *audioStscBuffer   = NULL;

    struct tm *nowTm;
    uint32_t nbFrames = 0;
    uint32_t nbaChunks = 0;
    uint32_t nbtFrames = 0;

#if ENCAPSULER_LOG_TIMESTAMPS
    fclose(tslogger);
#endif

    if (NULL == encapsuler)
    {
        ENCAPSULER_ERROR ("encapsuler double pointer must not be null");
        localError = ARMEDIA_ERROR_BAD_PARAMETER;
    }

    if (encapsuler && (NULL == *encapsuler))
    {
        ENCAPSULER_ERROR ("encapsuler pointer must not be null");
        localError = ARMEDIA_ERROR_BAD_PARAMETER;
    }
    else
    {
        encaps = *encapsuler;
        if (NULL == encaps->video)
        {
            ENCAPSULER_ERROR ("video pointer must not be null");
            localError = ARMEDIA_ERROR_BAD_PARAMETER;
        }
    }

    if (ARMEDIA_OK == localError)
    {
        video = encaps->video; // ease of reading
        audio = encaps->audio; // ease of reading
        metadata = encaps->metadata; // ease of reading
        if (!video->width)
        {
            // Video was not initialized
            ENCAPSULER_ERROR ("video was not initialized");
            localError =  ARMEDIA_ERROR_BAD_PARAMETER;
        } // No else
    }

    // alloc all buffers for metadata writings
    if (ARMEDIA_OK == localError)
    {
        // Init internal buffers
        frameSizeBufferNE   = calloc (video->framesCount, sizeof (uint32_t));
        videoOffsetBuffer   = calloc (video->framesCount, sizeof (uint64_t));
        frameTimeSyncBuffer = calloc (2*video->framesCount, sizeof (uint32_t));
        metadataTimeSyncBuffer = calloc (2*video->framesCount, sizeof (uint32_t));
        if (encaps->got_metadata && metadata != NULL && metadata->block_size > 0)
        {
            metadataOffsetBuffer = calloc (metadata->framesCount, sizeof (uint64_t));
        }
        if (video->codec == CODEC_MPEG4_AVC) {
            iFrameIndexBuffer = calloc (video->framesCount, sizeof (uint32_t));
        }
        if (encaps->got_audio) {
            audioOffsetBuffer = calloc (audio->sampleCount, sizeof (uint64_t));
            audioStscBuffer = calloc(audio->stscEntries*3, sizeof (uint32_t));
        }

        if (NULL == frameSizeBufferNE   ||
                NULL == videoOffsetBuffer   ||
                NULL == frameTimeSyncBuffer ||
                NULL == metadataTimeSyncBuffer ||
                (encaps->got_metadata && metadata != NULL && metadata->block_size > 0
                 && NULL == metadataOffsetBuffer) ||
                (NULL == iFrameIndexBuffer && video->codec == CODEC_MPEG4_AVC) ||
                (encaps->got_audio && (NULL == audioOffsetBuffer ||
                                       NULL == audioStscBuffer)))
        {
            ENCAPSULER_ERROR ("Unable to allocate buffers for video finish");

            ENCAPSULER_CLEANUP(free, frameSizeBufferNE);
            ENCAPSULER_CLEANUP(free, videoOffsetBuffer);
            ENCAPSULER_CLEANUP(free, frameTimeSyncBuffer);
            ENCAPSULER_CLEANUP(free, metadataTimeSyncBuffer);
            ENCAPSULER_CLEANUP(free, metadataOffsetBuffer);
            ENCAPSULER_CLEANUP(free, iFrameIndexBuffer);
            ENCAPSULER_CLEANUP(free, audioOffsetBuffer);
            ENCAPSULER_CLEANUP(free, audioStscBuffer);

            localError = ARMEDIA_ERROR_ENCAPSULER;
        }
    }


    if (ARMEDIA_OK == localError)
    {
        // Init internal counters
        uint32_t nbIFrames = 0;
        uint64_t chunkOffset = encaps->dataOffset;
        off_t descriptorSize = 0;

        uint32_t vtimescale = encaps->timescale;
        // Video time management
        uint32_t videosttsNentries = 0;
        uint32_t metadatasttsNentries = 0;
        uint32_t groupInterFrameDT = 0;
        uint32_t groupNframes = 0;
        uint32_t metadataGroupInterFrameDT = 0;
        uint32_t metadataGroupNframes = 0;
        uint32_t videoDuration = 0;
        off_t videoUniqueSize = 0;

        movie_atom_t* moovAtom;         // root
        movie_atom_t* mvhdAtom;         // > mvhd
        movie_atom_t* trakAtom;         // > trak
        movie_atom_t* tkhdAtom;         // | > tkhd
        movie_atom_t* trefAtom;         // | > tref
        movie_atom_t* cdscAtom;         // |   > cdsc
        movie_atom_t* mdiaAtom;         // | > mdia
        movie_atom_t* mdhdAtom;         // |   > mdhd
        movie_atom_t* hdlrmdiaAtom;     // |   > hdlr
        movie_atom_t* minfAtom;         // |   > minf
        movie_atom_t* hdlrminfAtom;     // |     > hdlr (used only with H264)
        movie_atom_t* vmhdAtom;         // |     > vmhd
        movie_atom_t* dinfAtom;         // |     > dinf
        movie_atom_t* drefAtom;         // |     | > dref
        movie_atom_t* stblAtom;         // |     > stbl
        movie_atom_t* stsdAtom;         // |       > stsd
        movie_atom_t* sttsAtom;         // |       > stts
        movie_atom_t* stssAtom;         // |       > stss (used only with H264)
        movie_atom_t* stscAtom;         // |       > stsc
        movie_atom_t* stszAtom;         // |       > stsz
        movie_atom_t* stcoAtom;         // |       > stco
        movie_atom_t* udtaAtom;         // > udta
        movie_atom_t* xyzUdtaAtom;      // | > xyz
        movie_atom_t* metaUdtaAtom;     // | > meta
        movie_atom_t* hdlrMetaUdtaAtom; // |   > hdlr
        movie_atom_t* ilstMetaUdtaAtom; // |   > ilst
        movie_atom_t* artistMetaUdtaAtom;   // |     > ART
        movie_atom_t* titleMetaUdtaAtom;    // |     > nam
        movie_atom_t* dateMetaUdtaAtom;     // |     > day
        movie_atom_t* commentMetaUdtaAtom;  // |     > cmt
        movie_atom_t* copyrightMetaUdtaAtom;// |     > cpy
        movie_atom_t* makerMetaUdtaAtom;    // |     > mak
        movie_atom_t* modelMetaUdtaAtom;    // |     > mod
        movie_atom_t* versionMetaUdtaAtom;  // |     > swr
        movie_atom_t* encoderMetaUdtaAtom;  // |     > too
        movie_atom_t* coverMetaUdtaAtom;    // |     > covr
        movie_atom_t* freeUdtaAtom;         // | > free
        movie_atom_t* metaAtom;         // > meta
        movie_atom_t* hdlrMetaAtom;     // | > hdlr
        movie_atom_t* keysMetaAtom;     // | > keys
        movie_atom_t* ilstMetaAtom;     // | > ilst
        movie_atom_t* locationMetaAtom; // |   > com.apple.quicktime.location.ISO6709
        movie_atom_t* artistMetaAtom;   // |   > com.apple.quicktime.artist
        movie_atom_t* titleMetaAtom;    // |   > com.apple.quicktime.title
        movie_atom_t* dateMetaAtom;     // |   > com.apple.quicktime.creationdate
        movie_atom_t* commentMetaAtom;  // |   > com.apple.quicktime.comment
        movie_atom_t* copyrightMetaAtom;// |   > com.apple.quicktime.copyright
        movie_atom_t* makerMetaAtom;    // |   > com.apple.quicktime.make
        movie_atom_t* modelMetaAtom;    // |   > com.apple.quicktime.model
        movie_atom_t* versionMetaAtom;  // |   > com.apple.quicktime.software
        movie_atom_t* coverMetaAtom;    // |   > com.apple.quicktime.artwork
        movie_atom_t* serialMetaAtom;   // |   > com.parrot.serial
        movie_atom_t* modelidMetaAtom;  // |   > com.parrot.model.id
        movie_atom_t* buildidMetaAtom;  // |   > com.parrot.build.id
        movie_atom_t* runidMetaAtom;    // |   > com.parrot.run.id
        movie_atom_t* rundateMetaAtom;  // |   > com.parrot.run.date
        movie_atom_t* customMetaAtom;   // |   > user-defined
        movie_atom_t* picturehfovMetaAtom;  // |   > com.parrot.picture.hfov
        movie_atom_t* picturevfovMetaAtom;  // |   > com.parrot.picture.vfov
        movie_atom_t* freeMetaAtom;         // | > free

        uint32_t stssDataLen;
        uint8_t *stssBuffer;
        uint32_t nenbIFrames;

        // Generate stts atom from frameTimeSyncBuffer
        uint32_t sttsDataLen;
        uint8_t *sttsBuffer;
        uint32_t sttsNentriesNE;

        // Generate stsz atom from frameSizeBufferNE and nbFrames
        uint32_t nbFramesNE;

        // Generate stco atom from videoOffsetBuffer and nbFrames
        uint32_t stcoDataLen;
        uint8_t *stcoBuffer;

        // Read info file
        rewind (encaps->metaFile);

        // Skip video descriptor
        if (1 != fread(&descriptorSize, sizeof (uint32_t), 1, encaps->metaFile))
        {
            ENCAPSULER_ERROR ("Unable to read descriptor size (probably an old .infovid file)");
            rewind (encaps->metaFile);
        }
        else
        {
            fseeko(encaps->metaFile, descriptorSize, SEEK_CUR);
        }

        uint32_t interframeDT;
        uint64_t tmpinterframeDT;
        off_t lastChunkSize = 0;
        uint32_t cptAudioStsc = 0;
        while (!feof(encaps->metaFile))
        {
            long long fSize_tmp;
            off_t fSize = 0;
            char fType = '\0';
            char dataType = '\0';
            int readPatterns;
            // video
            interframeDT = 0;
            tmpinterframeDT = 0;
            readPatterns = fscanf (encaps->metaFile, ARMEDIA_ENCAPSULER_INFO_PATTERN,
                    &dataType, &fSize_tmp,
                    &fType, &interframeDT);
            if (ARMEDIA_ENCAPSULER_NUM_MATCH_PATTERN == readPatterns)
            {
                fSize = fSize_tmp;
                if (dataType == ARMEDIA_ENCAPSULER_VIDEO_INFO_TAG) // video
                {
                    frameSizeBufferNE[nbFrames] = htonl (fSize);
                    uint64_t n_co_l = htonl(chunkOffset & 0xffffffff);
                    uint64_t n_co_h = htonl(chunkOffset >> 32);
                    videoOffsetBuffer[nbFrames] = (n_co_l << 32) + n_co_h;

                    // from microseconds to time units
                    tmpinterframeDT = ((uint64_t)vtimescale) * ((uint64_t) interframeDT);
                    interframeDT = (uint32_t)(tmpinterframeDT / 1000000);
                    // Time sync mgmt
                    if (interframeDT != 0) {
                        // not first frame
                        if (interframeDT != groupInterFrameDT) {
                            // new entry => save previous entry and create a new one
                            if (groupInterFrameDT != 0) { // not first group
                                frameTimeSyncBuffer[2*videosttsNentries] = htonl(groupNframes);
                                frameTimeSyncBuffer[2*videosttsNentries+1] = htonl(groupInterFrameDT);
                                videoDuration += groupNframes * groupInterFrameDT;
                                videosttsNentries++;
                            }
                            groupNframes = 1;
                            groupInterFrameDT = interframeDT;
                        } else {
                            // use previous entry
                            groupNframes++;
                        }

                        // size mgmt
                        if (videoUniqueSize != 0 && videoUniqueSize != fSize) {
                            videoUniqueSize = 0;
                        }
                    } else {
                        // first frame => no DT
                        videoUniqueSize = fSize;
                    }

                    nbFrames++;

                    if (('i' == fType) && (video->codec == CODEC_MPEG4_AVC))
                    {
                        iFrameIndexBuffer [nbIFrames++] = htonl (nbFrames);
                    }
                }
                else if (dataType == ARMEDIA_ENCAPSULER_AUDIO_INFO_TAG) // audio
                {
                    uint64_t n_co_l = htonl(chunkOffset & 0xffffffff);
                    uint64_t n_co_h = htonl(chunkOffset >> 32);
                    audioOffsetBuffer[nbaChunks] = (n_co_l << 32) + n_co_h;
                    nbaChunks++;

                    if (lastChunkSize != fSize) {
                        // add stsc entry
                        uint32_t nSampleInChunk = fSize * 8*sizeof(uint8_t) / (audio->nchannel*audio->format);
                        audioStscBuffer[3*cptAudioStsc] = htonl(nbaChunks);
                        audioStscBuffer[3*cptAudioStsc+1] = htonl(nSampleInChunk);
                        audioStscBuffer[3*cptAudioStsc+2] = htonl(1);
                        cptAudioStsc++;
                        lastChunkSize = fSize;
                    }
                }
                else if (dataType == ARMEDIA_ENCAPSULER_METADATA_INFO_TAG)
                {
                    uint64_t n_co_l = htonl(chunkOffset & 0xffffffff);
                    uint64_t n_co_h = htonl(chunkOffset >> 32);
                    metadataOffsetBuffer[nbtFrames] = (n_co_l << 32) + n_co_h;

                    // from microseconds to time units
                    tmpinterframeDT = ((uint64_t)vtimescale) * ((uint64_t) interframeDT);
                    interframeDT = (uint32_t)(tmpinterframeDT / 1000000);
                    // Time sync mgmt
                    if (interframeDT != 0) {
                        // not first frame
                        if (interframeDT != metadataGroupInterFrameDT) {
                            // new entry => save previous entry and create a new one
                            if (metadataGroupInterFrameDT != 0) { // not first group
                                metadataTimeSyncBuffer[2*metadatasttsNentries] = htonl(metadataGroupNframes);
                                metadataTimeSyncBuffer[2*metadatasttsNentries+1] = htonl(metadataGroupInterFrameDT);
                                metadatasttsNentries++;
                            }
                            metadataGroupNframes = 1;
                            metadataGroupInterFrameDT = interframeDT;
                        } else {
                            // use previous entry
                            metadataGroupNframes++;
                        }
                    }

                    nbtFrames++;
                }
                chunkOffset += fSize; // common computation of chunk offset
            }
        }

        // last frame to default DT + last entry
        // from microseconds to time units
        tmpinterframeDT = ((uint64_t)vtimescale) * ((uint64_t) video->defaultFrameDuration);
        interframeDT = (uint32_t)(tmpinterframeDT / 1000000);
        if (groupInterFrameDT == interframeDT) { // add last frame to previous entry
            groupNframes++;
        } else { // write previous entry then add last frame to new entry
            if (groupInterFrameDT != 0) { // not first group
                frameTimeSyncBuffer[2*videosttsNentries] = htonl(groupNframes);
                frameTimeSyncBuffer[2*videosttsNentries+1] = htonl(groupInterFrameDT);
                videoDuration += groupNframes * groupInterFrameDT;
                videosttsNentries++;
            }
            groupNframes = 1;
            groupInterFrameDT = interframeDT;
        }
        frameTimeSyncBuffer[2*videosttsNentries] = htonl(groupNframes);
        frameTimeSyncBuffer[2*videosttsNentries+1] = htonl(groupInterFrameDT);
        videoDuration += groupNframes * groupInterFrameDT;
        videosttsNentries++;

        // last frame to default DT + last entry
        // from microseconds to time units
        tmpinterframeDT = ((uint64_t)vtimescale) * ((uint64_t) video->defaultFrameDuration);
        interframeDT = (uint32_t)(tmpinterframeDT / 1000000);
        if (metadataGroupInterFrameDT == interframeDT) { // add last frame to previous entry
            metadataGroupNframes++;
        } else { // write previous entry then add last frame to new entry
            if (metadataGroupInterFrameDT != 0) { // not first group
                metadataTimeSyncBuffer[2*metadatasttsNentries] = htonl(metadataGroupNframes);
                metadataTimeSyncBuffer[2*metadatasttsNentries+1] = htonl(metadataGroupInterFrameDT);
                metadatasttsNentries++;
            }
            metadataGroupNframes = 1;
            metadataGroupInterFrameDT = interframeDT;
        }
        metadataTimeSyncBuffer[2*metadatasttsNentries] = htonl(metadataGroupNframes);
        metadataTimeSyncBuffer[2*metadatasttsNentries+1] = htonl(metadataGroupInterFrameDT);
        metadatasttsNentries++;

        // get the local time value
        nowTm = localtime (&(encaps->creationTime));

        // create atoms
        // Generating Atoms
        moovAtom = atomFromData(0, "moov", NULL);

        // Untimed metadata
        if (encaps->got_untimed_metadata || strlen(encaps->thumbnailFilePath))
        {
            const char *key[ARMEDIA_UNTIMED_METADATA_KEY_MAX + ARMEDIA_ENCAPSULER_UNTIMED_METADATA_CUSTOM_MAX_COUNT];
            uint32_t keyCount = 0;
            udtaAtom = atomFromData(0, "udta", NULL);
            uint32_t zero = 0;
            metaUdtaAtom = atomFromData(4, "meta", (uint8_t*)&zero);
            hdlrMetaUdtaAtom = hdlrAtomForUdtaMetadata();
            ilstMetaUdtaAtom = atomFromData(0, "ilst", NULL);
            metaAtom = atomFromData(0, "meta", NULL);
            hdlrMetaAtom = hdlrAtomForMetadata();
            ilstMetaAtom = atomFromData(0, "ilst", NULL);
            if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.artist))
            {
                artistMetaUdtaAtom = metadataAtomFromTagAndValue(0, "ART", encaps->untimed_metadata.artist, 1);
                if (artistMetaUdtaAtom)
                {
                    insertAtomIntoAtom(ilstMetaUdtaAtom, &artistMetaUdtaAtom);
                }
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_ARTIST];
                if (key[keyCount]) keyCount++;
                artistMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.artist, 1);
                if (artistMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &artistMetaAtom);
                }
            }
            else if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.maker) && strlen(encaps->untimed_metadata.model))
            {
                // Build the artist string as maker + model
                char artist[100];
                snprintf(artist, sizeof(artist), "%s %s",
                         encaps->untimed_metadata.maker,
                         encaps->untimed_metadata.model);
                artistMetaUdtaAtom = metadataAtomFromTagAndValue(0, "ART", artist, 1);
                if (artistMetaUdtaAtom)
                {
                    insertAtomIntoAtom(ilstMetaUdtaAtom, &artistMetaUdtaAtom);
                }
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_ARTIST];
                if (key[keyCount]) keyCount++;
                artistMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, artist, 1);
                if (artistMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &artistMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.title))
            {
                titleMetaUdtaAtom = metadataAtomFromTagAndValue(0, "nam", encaps->untimed_metadata.title, 1);
                if (titleMetaUdtaAtom)
                {
                    insertAtomIntoAtom(ilstMetaUdtaAtom, &titleMetaUdtaAtom);
                }
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_TITLE];
                if (key[keyCount]) keyCount++;
                titleMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.title, 1);
                if (titleMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &titleMetaAtom);
                }
            }
            else if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.runDate))
            {
                // If no title is defined, used the run date
                titleMetaUdtaAtom = metadataAtomFromTagAndValue(0, "nam", encaps->untimed_metadata.runDate, 1);
                if (titleMetaUdtaAtom)
                {
                    insertAtomIntoAtom(ilstMetaUdtaAtom, &titleMetaUdtaAtom);
                }
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_TITLE];
                if (key[keyCount]) keyCount++;
                titleMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.runDate, 1);
                if (titleMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &titleMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.mediaDate))
            {
                dateMetaUdtaAtom = metadataAtomFromTagAndValue(0, "day", encaps->untimed_metadata.mediaDate, 1);
                if (dateMetaUdtaAtom)
                {
                    insertAtomIntoAtom(ilstMetaUdtaAtom, &dateMetaUdtaAtom);
                }
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_DATE];
                if (key[keyCount]) keyCount++;
                dateMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.mediaDate, 1);
                if (dateMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &dateMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && (encaps->untimed_metadata.takeoffLatitude != 500.) && (encaps->untimed_metadata.takeoffLongitude != 500.))
            {
                char xyz[100];
                char *location = xyz + 4;
                /* ISO 6709 Annex H string expression */
                snprintf(location, sizeof(xyz) - 4, "%+08.4f%+09.4f/",
                         encaps->untimed_metadata.takeoffLatitude,
                         encaps->untimed_metadata.takeoffLongitude);
                xyz[0] = (strlen(location) >> 8) & 0xFF;
                xyz[1] = strlen(location) & 0xFF;
                xyz[2] = 0x15; // language code = 0x15c7
                xyz[3] = 0xc7; // language code = 0x15c7
                xyzUdtaAtom = atomFromData (4 + strlen(location), "\xA9xyz", (uint8_t*)xyz);
                if (xyzUdtaAtom)
                {
                    insertAtomIntoAtom(udtaAtom, &xyzUdtaAtom);
                }
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_LOCATION];
                if (key[keyCount]) keyCount++;
                snprintf(xyz, sizeof(xyz), "%+012.8f%+013.8f%+.2f/",
                         encaps->untimed_metadata.takeoffLatitude,
                         encaps->untimed_metadata.takeoffLongitude,
                         encaps->untimed_metadata.takeoffAltitude);
                locationMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, xyz, 1);
                if (locationMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &locationMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.maker))
            {
                makerMetaUdtaAtom = metadataAtomFromTagAndValue(0, "mak", encaps->untimed_metadata.maker, 1);
                if (makerMetaUdtaAtom)
                {
                    insertAtomIntoAtom(ilstMetaUdtaAtom, &makerMetaUdtaAtom);
                }
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_MAKER];
                if (key[keyCount]) keyCount++;
                makerMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.maker, 1);
                if (makerMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &makerMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.model))
            {
                modelMetaUdtaAtom = metadataAtomFromTagAndValue(0, "mod", encaps->untimed_metadata.model, 1);
                if (modelMetaUdtaAtom)
                {
                    insertAtomIntoAtom(ilstMetaUdtaAtom, &modelMetaUdtaAtom);
                }
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_MODEL];
                if (key[keyCount]) keyCount++;
                modelMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.model, 1);
                if (modelMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &modelMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.modelId))
            {
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_MODEL_ID];
                if (key[keyCount]) keyCount++;
                modelidMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.modelId, 1);
                if (modelidMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &modelidMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.buildId))
            {
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_BUILD_ID];
                if (key[keyCount]) keyCount++;
                buildidMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.buildId, 1);
                if (buildidMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &buildidMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.softwareVersion))
            {
                versionMetaUdtaAtom = metadataAtomFromTagAndValue(0, "swr", encaps->untimed_metadata.softwareVersion, 1);
                if (versionMetaUdtaAtom)
                {
                    insertAtomIntoAtom(ilstMetaUdtaAtom, &versionMetaUdtaAtom);
                }
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_VERSION];
                if (key[keyCount]) keyCount++;
                versionMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.softwareVersion, 1);
                if (versionMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &versionMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.serialNumber))
            {
                encoderMetaUdtaAtom = metadataAtomFromTagAndValue(0, "too", encaps->untimed_metadata.serialNumber, 1);
                if (encoderMetaUdtaAtom)
                {
                    insertAtomIntoAtom(ilstMetaUdtaAtom, &encoderMetaUdtaAtom);
                }
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_SERIAL];
                if (key[keyCount]) keyCount++;
                serialMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.serialNumber, 1);
                if (serialMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &serialMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.comment))
            {
                commentMetaUdtaAtom = metadataAtomFromTagAndValue(0, "cmt", encaps->untimed_metadata.comment, 1);
                if (commentMetaUdtaAtom)
                {
                    insertAtomIntoAtom(ilstMetaUdtaAtom, &commentMetaUdtaAtom);
                }
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_COMMENT];
                if (key[keyCount]) keyCount++;
                commentMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.comment, 1);
                if (commentMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &commentMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.copyright))
            {
                copyrightMetaUdtaAtom = metadataAtomFromTagAndValue(0, "cpy", encaps->untimed_metadata.copyright, 1);
                if (copyrightMetaUdtaAtom)
                {
                    insertAtomIntoAtom(ilstMetaUdtaAtom, &copyrightMetaUdtaAtom);
                }
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_COPYRIGHT];
                if (key[keyCount]) keyCount++;
                copyrightMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.copyright, 1);
                if (copyrightMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &copyrightMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.runUuid))
            {
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_RUN_ID];
                if (key[keyCount]) keyCount++;
                runidMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.runUuid, 1);
                if (runidMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &runidMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && strlen(encaps->untimed_metadata.runDate))
            {
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_RUN_DATE];
                if (key[keyCount]) keyCount++;
                rundateMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.runDate, 1);
                if (rundateMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &rundateMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && (encaps->untimed_metadata.pictureHFov != 0.))
            {
                char hfov[20];
                snprintf(hfov, 20, "%.2f", encaps->untimed_metadata.pictureHFov);
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_PICTURE_HFOV];
                if (key[keyCount]) keyCount++;
                picturehfovMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, hfov, 1);
                if (picturehfovMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &picturehfovMetaAtom);
                }
            }
            if ((encaps->got_untimed_metadata) && (encaps->untimed_metadata.pictureVFov != 0.))
            {
                char vfov[20];
                snprintf(vfov, 20, "%.2f", encaps->untimed_metadata.pictureVFov);
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_PICTURE_VFOV];
                if (key[keyCount]) keyCount++;
                picturevfovMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, vfov, 1);
                if (picturevfovMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &picturevfovMetaAtom);
                }
            }
            if (encaps->got_untimed_metadata)
            {
                int i;
                for (i = 0; i < ARMEDIA_ENCAPSULER_UNTIMED_METADATA_CUSTOM_MAX_COUNT; i++)
                {
                    if ((strlen(encaps->untimed_metadata.custom[i].key)) && (strlen(encaps->untimed_metadata.custom[i].value)))
                    {
                        key[keyCount] = encaps->untimed_metadata.custom[i].key;
                        keyCount++;
                        customMetaAtom = metadataAtomFromTagAndValue(keyCount, NULL, encaps->untimed_metadata.custom[i].value, 1);
                        if (customMetaAtom)
                        {
                            insertAtomIntoAtom(ilstMetaAtom, &customMetaAtom);
                        }
                    }
                }
            }
            if (strlen(encaps->thumbnailFilePath))
            {
                coverMetaUdtaAtom = metadataAtomFromTagAndFile(0, "covr", encaps->thumbnailFilePath, 13);
                if (coverMetaUdtaAtom)
                {
                    insertAtomIntoAtom(ilstMetaUdtaAtom, &coverMetaUdtaAtom);
                }
                key[keyCount] = ARMEDIA_UntimedMetadataKey[ARMEDIA_UNTIMED_METADATA_KEY_COVER];
                if (key[keyCount]) keyCount++;
                coverMetaAtom = metadataAtomFromTagAndFile(keyCount, NULL, encaps->thumbnailFilePath, 13);
                if (coverMetaAtom)
                {
                    insertAtomIntoAtom(ilstMetaAtom, &coverMetaAtom);
                }
            }

            keysMetaAtom = metadataKeysAtom(key, keyCount);

            // Add 1kB free space at the end of the udta and meta boxes to allow post-editing of metadata
            uint32_t emptydatasize = 1024 - 8; // remove 8 bytes for box size and type
            uint8_t *emptydata = calloc(emptydatasize, sizeof(uint8_t));
            if (emptydata == NULL) {
                ENCAPSULER_ERROR("Error allocating free data");
                freeUdtaAtom = NULL;
                freeMetaAtom = NULL;
            } else {
                freeUdtaAtom = atomFromData(emptydatasize, "free", emptydata);
                freeMetaAtom = atomFromData(emptydatasize, "free", emptydata);
                free(emptydata);
            }

            insertAtomIntoAtom(metaUdtaAtom, &hdlrMetaUdtaAtom);
            insertAtomIntoAtom(metaUdtaAtom, &ilstMetaUdtaAtom);
            insertAtomIntoAtom(udtaAtom, &metaUdtaAtom);
            if (freeUdtaAtom) insertAtomIntoAtom(udtaAtom, &freeUdtaAtom);
            insertAtomIntoAtom(moovAtom, &udtaAtom);
            insertAtomIntoAtom(metaAtom, &hdlrMetaAtom);
            insertAtomIntoAtom(metaAtom, &keysMetaAtom);
            insertAtomIntoAtom(metaAtom, &ilstMetaAtom);
            if (freeMetaAtom) insertAtomIntoAtom(metaAtom, &freeMetaAtom);
            insertAtomIntoAtom(moovAtom, &metaAtom);
        }

        mvhdAtom = mvhdAtomFromFpsNumFramesAndDate (encaps->timescale, videoDuration, encaps->creationTime);
        trakAtom = atomFromData(0, "trak", NULL);
        tkhdAtom = tkhdAtomWithResolutionNumFramesFpsAndDate (video->width, video->height, encaps->timescale, videoDuration, encaps->creationTime, ARMEDIA_VIDEOATOM_MEDIATYPE_VIDEO);
        mdiaAtom = atomFromData(0, "mdia", NULL);
        mdhdAtom = mdhdAtomFromFpsNumFramesAndDate (encaps->timescale, videoDuration, encaps->creationTime);
        hdlrmdiaAtom = hdlrAtomForMdia (ARMEDIA_VIDEOATOM_MEDIATYPE_VIDEO);
        minfAtom = atomFromData(0, "minf", NULL);
        vmhdAtom = vmhdAtomGen ();
        if (CODEC_MPEG4_AVC == video->codec)
            hdlrminfAtom = hdlrAtomForMinf();
        dinfAtom = atomFromData(0, "dinf", NULL);
        drefAtom = drefAtomGen ();
        stblAtom = atomFromData(0, "stbl", NULL);
        stsdAtom = stsdAtomWithResolutionCodecSpsAndPps (video->width, video->height, video->codec, &video->sps[4], video->spsSize -4, &video->pps[4], video->ppsSize -4);

        // Generate stts atom from frameTimeSyncBuffer
        sttsDataLen = (8 + 2 * videosttsNentries * sizeof(uint32_t));
        sttsBuffer = (uint8_t*) calloc (sttsDataLen, 1);
        sttsNentriesNE = htonl(videosttsNentries);
        memcpy (&sttsBuffer[4], &sttsNentriesNE, sizeof(uint32_t));
        memcpy (&sttsBuffer[8], frameTimeSyncBuffer, 2 * videosttsNentries * sizeof(uint32_t));
        sttsAtom = atomFromData (sttsDataLen, "stts", sttsBuffer);
        free(sttsBuffer);

        if (CODEC_MPEG4_AVC == video->codec) {
            // Generate stss atom from iFramesIndexBuffer and nbIFrames
            stssDataLen = (8 + (nbIFrames * sizeof (uint32_t)));
            stssBuffer = (uint8_t*) calloc (stssDataLen, 1);
            nenbIFrames = htonl (nbIFrames);
            memcpy (&stssBuffer[4], &nenbIFrames, sizeof (uint32_t));
            memcpy (&stssBuffer[8], iFrameIndexBuffer, nbIFrames * sizeof (uint32_t));
            stssAtom = atomFromData (stssDataLen, "stss", stssBuffer);
            free (stssBuffer);
        }

        stscAtom = stscAtomGen(1, NULL, 1); // 1 video frame = 1 chunk

        // Generate stsz atom from frameSizeBufferNE and nbFrames
        stszAtom = stszAtomGen(videoUniqueSize, frameSizeBufferNE, nbFrames);

        // Generate stco atom from videoOffsetBuffer and nbFrames
        nbFramesNE = htonl (nbFrames);
        stcoDataLen = (8 + (nbFrames * sizeof (uint64_t)));
        stcoBuffer = (uint8_t*) calloc (stcoDataLen, 1);
        memcpy (&stcoBuffer[4], &nbFramesNE, sizeof (uint32_t));
        memcpy (&stcoBuffer[8], videoOffsetBuffer, nbFrames * sizeof (uint64_t));
        stcoAtom = atomFromData (stcoDataLen, "co64", stcoBuffer);
        free (stcoBuffer);

        // Create atom tree
        insertAtomIntoAtom(stblAtom, &stsdAtom);
        insertAtomIntoAtom(stblAtom, &sttsAtom);
        if (CODEC_MPEG4_AVC == video->codec)
            insertAtomIntoAtom(stblAtom, &stssAtom);

        insertAtomIntoAtom(stblAtom, &stscAtom);
        insertAtomIntoAtom(stblAtom, &stszAtom);
        insertAtomIntoAtom(stblAtom, &stcoAtom);

        insertAtomIntoAtom(dinfAtom, &drefAtom);

        insertAtomIntoAtom(minfAtom, &vmhdAtom);
        if (CODEC_MPEG4_AVC == video->codec)
            insertAtomIntoAtom(minfAtom, &hdlrminfAtom);
        insertAtomIntoAtom(minfAtom, &dinfAtom);
        insertAtomIntoAtom(minfAtom, &stblAtom);

        insertAtomIntoAtom(mdiaAtom, &mdhdAtom);
        insertAtomIntoAtom(mdiaAtom, &hdlrmdiaAtom);
        insertAtomIntoAtom(mdiaAtom, &minfAtom);

        insertAtomIntoAtom(trakAtom, &tkhdAtom);
        insertAtomIntoAtom(trakAtom, &mdiaAtom);

        insertAtomIntoAtom(moovAtom, &mvhdAtom);
        insertAtomIntoAtom(moovAtom, &trakAtom);

        if (encaps->got_metadata && metadata != NULL && metadata->block_size > 0)
        {
            uint32_t nbtFramesNE = htonl(nbtFrames);
            movie_atom_t* nmhdAtom;

            stblAtom = atomFromData(0, "stbl", NULL);

            stsdAtom = stsdAtomForMetadata (
                    metadata->content_encoding, metadata->mime_format);

            // Generate stts atom from metadataTimeSyncBuffer
            sttsDataLen = (8 + 2 * metadatasttsNentries * sizeof(uint32_t));
            sttsBuffer = (uint8_t*) calloc (sttsDataLen, 1);
            sttsNentriesNE = htonl(metadatasttsNentries);
            memcpy (&sttsBuffer[4], &sttsNentriesNE, sizeof(uint32_t));
            memcpy (&sttsBuffer[8], metadataTimeSyncBuffer, 2 * metadatasttsNentries * sizeof(uint32_t));
            sttsAtom = atomFromData (sttsDataLen, "stts", sttsBuffer);
            free(sttsBuffer);

            stscAtom = stscAtomGen(1, NULL, 1); // 1 metadata frame = 1 chunk

            // Generate stsz atom from of metadata blocks_size and nbtFrames
            stszAtom = stszAtomGen(metadata->block_size, NULL, nbtFrames);

            // Generate stco atom from metadataOffsetBuffer and nbFrames
            stcoDataLen = (8 + (nbtFrames * sizeof (uint64_t)));
            stcoBuffer = (uint8_t*) calloc (stcoDataLen, 1);
            memcpy (&stcoBuffer[4], &nbtFramesNE, sizeof (uint32_t));
            memcpy (&stcoBuffer[8], metadataOffsetBuffer, nbtFrames * sizeof (uint64_t));
            stcoAtom = atomFromData (stcoDataLen, "co64", stcoBuffer);
            free (stcoBuffer);

            insertAtomIntoAtom(stblAtom, &stsdAtom);
            insertAtomIntoAtom(stblAtom, &sttsAtom); // Same as video
            insertAtomIntoAtom(stblAtom, &stscAtom);
            insertAtomIntoAtom(stblAtom, &stszAtom);
            insertAtomIntoAtom(stblAtom, &stcoAtom);


            dinfAtom = atomFromData(0, "dinf", NULL);
            drefAtom = drefAtomGen ();

            insertAtomIntoAtom(dinfAtom, &drefAtom);

            minfAtom = atomFromData(0, "minf", NULL);
            nmhdAtom = nmhdAtomGen();

            insertAtomIntoAtom(minfAtom, &nmhdAtom);
            insertAtomIntoAtom(minfAtom, &dinfAtom);
            insertAtomIntoAtom(minfAtom, &stblAtom);


            mdiaAtom = atomFromData(0, "mdia", NULL);
            mdhdAtom = mdhdAtomFromFpsNumFramesAndDate (encaps->timescale, videoDuration, encaps->creationTime);
            hdlrmdiaAtom = hdlrAtomForMdia (ARMEDIA_VIDEOATOM_MEDIATYPE_METADATA);

            insertAtomIntoAtom(mdiaAtom, &mdhdAtom);
            insertAtomIntoAtom(mdiaAtom, &hdlrmdiaAtom);
            insertAtomIntoAtom(mdiaAtom, &minfAtom);


            trakAtom = atomFromData(0, "trak", NULL);
            tkhdAtom = tkhdAtomWithResolutionNumFramesFpsAndDate (0, 0, encaps->timescale, videoDuration, encaps->creationTime, ARMEDIA_VIDEOATOM_MEDIATYPE_METADATA);
            trefAtom = atomFromData(0, "tref", NULL);
            uint32_t cdsc_track_id = ARMEDIA_VIDEOATOM_MEDIATYPE_VIDEO + 1;
            cdscAtom = cdscAtomGen (&cdsc_track_id, 1);
            insertAtomIntoAtom(trakAtom, &tkhdAtom);
            insertAtomIntoAtom(trefAtom, &cdscAtom);
            insertAtomIntoAtom(trakAtom, &trefAtom);
            insertAtomIntoAtom(trakAtom, &mdiaAtom);
            insertAtomIntoAtom(moovAtom, &trakAtom);
        }

        if(encaps->got_audio)
        {
            uint32_t nbSamples = audio->totalsize * 8*sizeof(uint8_t) / (audio->format * audio->nchannel);
            uint32_t nbSamplesNE = htonl(nbSamples);
            uint32_t nbaChunksNE = htonl(nbaChunks);
            movie_atom_t* smhdAtom;

            stblAtom = atomFromData(0, "stbl", NULL);

            stsdAtom = stsdAtomWithAudioCodec(audio->codec, audio->format, audio->nchannel, audio->freq);

            sttsDataLen = (8 + 2 * sizeof(uint32_t));
            sttsBuffer = (uint8_t*) calloc (sttsDataLen, 1);
            sttsNentriesNE = htonl(1);
            memcpy (&sttsBuffer[4], &sttsNentriesNE, sizeof(uint32_t));
            memcpy (&sttsBuffer[8], &nbSamplesNE, sizeof(uint32_t));
            memcpy (&sttsBuffer[12], &sttsNentriesNE, sizeof(uint32_t));
            sttsAtom = atomFromData (sttsDataLen, "stts", sttsBuffer);
            free(sttsBuffer);

            stscAtom = stscAtomGen(0, audioStscBuffer, cptAudioStsc);

            // Generate stsz atom
            stszAtom = stszAtomGen(audio->nchannel * audio->format/(8*sizeof(uint8_t)), NULL, nbSamples);

            // Generate stco atom from audioOffsetBuffer and nbFrames
            stcoDataLen = (8 + (nbaChunks * sizeof (uint64_t)));
            stcoBuffer = (uint8_t*) calloc (stcoDataLen, 1);
            memcpy (&stcoBuffer[4], &nbaChunksNE, sizeof (uint32_t));
            memcpy (&stcoBuffer[8], audioOffsetBuffer, nbaChunks * sizeof (uint64_t));
            stcoAtom = atomFromData (stcoDataLen, "co64", stcoBuffer);
            free (stcoBuffer);

            insertAtomIntoAtom(stblAtom, &stsdAtom);
            insertAtomIntoAtom(stblAtom, &sttsAtom);
            insertAtomIntoAtom(stblAtom, &stscAtom);
            insertAtomIntoAtom(stblAtom, &stszAtom);
            insertAtomIntoAtom(stblAtom, &stcoAtom);


            dinfAtom = atomFromData(0, "dinf", NULL);
            drefAtom = drefAtomGen ();

            insertAtomIntoAtom(dinfAtom, &drefAtom);


            minfAtom = atomFromData(0, "minf", NULL);
            smhdAtom = smhdAtomGen();

            insertAtomIntoAtom(minfAtom, &smhdAtom);
            insertAtomIntoAtom(minfAtom, &dinfAtom);
            insertAtomIntoAtom(minfAtom, &stblAtom);


            mdiaAtom = atomFromData(0, "mdia", NULL);
            mdhdAtom = mdhdAtomFromFpsNumFramesAndDate (audio->freq, nbSamples, encaps->creationTime);
            hdlrmdiaAtom = hdlrAtomForMdia (ARMEDIA_VIDEOATOM_MEDIATYPE_SOUND);

            insertAtomIntoAtom(mdiaAtom, &mdhdAtom);
            insertAtomIntoAtom(mdiaAtom, &hdlrmdiaAtom);
            insertAtomIntoAtom(mdiaAtom, &minfAtom);


            trakAtom = atomFromData(0, "trak", NULL);
            tkhdAtom = tkhdAtomWithResolutionNumFramesFpsAndDate (0, 0, encaps->timescale, videoDuration, encaps->creationTime, ARMEDIA_VIDEOATOM_MEDIATYPE_SOUND);
            insertAtomIntoAtom(trakAtom, &tkhdAtom);
            insertAtomIntoAtom(trakAtom, &mdiaAtom);
            insertAtomIntoAtom(moovAtom, &trakAtom);
        }

        if (-1 == writeAtomToFile (&moovAtom, encaps->dataFile))
        {
            ENCAPSULER_ERROR ("Error while writing moovAtom");
            localError = ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
        fflush(encaps->dataFile);
        fsync(fileno(encaps->dataFile));
    }

    if (ARMEDIA_OK == localError)
    {
        off_t mdatsize = video->totalsize + 8;
        if (encaps->got_audio) mdatsize += audio->totalsize;
        if (encaps->got_metadata && metadata != NULL && metadata->block_size > 0)
            mdatsize += metadata->totalsize;
        movie_atom_t *mdatAtom = mdatAtomForFormatWithVideoSize(mdatsize);
        // write mdat atom
        fseeko(encaps->dataFile, encaps->mdatAtomOffset, SEEK_SET);
        if (-1 == writeAtomToFile (&mdatAtom, encaps->dataFile))
        {
            ENCAPSULER_ERROR ("Error while writing mdatAtom");
            localError = ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
        fflush(encaps->dataFile);
        fsync(fileno(encaps->dataFile));
    }

    /* pvat insertion at the benning of the file */
    if (ARMEDIA_OK == localError)
    {
        char* pvatstr = ARMEDIA_VideoAtom_GetPVATString(encaps->product, encaps->uuid, encaps->runDate, encaps->dataFilePath, nowTm);
        if (pvatstr != NULL) {
            size_t len = strlen(pvatstr);
            movie_atom_t *pvatAtom = pvatAtomGen(pvatstr);
            fseeko(encaps->dataFile, encaps->mdatAtomOffset - (ARMEDIA_JSON_DESCRIPTION_MAXLENGTH+8), SEEK_SET);
            if (-1 == writeAtomToFile (&pvatAtom, encaps->dataFile))
            {
                ENCAPSULER_ERROR ("Error while writing pvatAtom");
                localError = ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
            // fill leaving space with a free atom until the mdatom
            uint32_t emptydatasize = ARMEDIA_JSON_DESCRIPTION_MAXLENGTH-(len+8);
            uint8_t *emptydata = calloc(emptydatasize, sizeof(uint8_t));
            if (emptydata == NULL) {
                ENCAPSULER_ERROR ("Error allocating freedata");
                localError = ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            } else {
                movie_atom_t *fillatom = atomFromData(emptydatasize, "free", emptydata);
                if (-1 == writeAtomToFile (&fillatom, encaps->dataFile))
                {
                    ENCAPSULER_ERROR ("Error while writing fillatom");
                    localError = ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
                }
                free(emptydata);
            }

            free(pvatstr);
        } else {
            ENCAPSULER_ERROR ("Error Json Pvat string empty");
        }
        fflush(encaps->dataFile);
        fsync(fileno(encaps->dataFile));
    }

    bool rename_tempFile = (ARMEDIA_OK == localError);
    ENCAPSULER_CLEANUP(free, frameSizeBufferNE);
    ENCAPSULER_CLEANUP(free, videoOffsetBuffer);
    ENCAPSULER_CLEANUP(free, frameTimeSyncBuffer);
    ENCAPSULER_CLEANUP(free, metadataTimeSyncBuffer);
    ENCAPSULER_CLEANUP(free, iFrameIndexBuffer);
    ENCAPSULER_CLEANUP(free, metadataOffsetBuffer);
    ENCAPSULER_CLEANUP(free, audioOffsetBuffer);
    ENCAPSULER_CLEANUP(free, audioStscBuffer);
    ARMEDIA_VideoEncapsuler_Cleanup (encapsuler, rename_tempFile);

    return localError;
}

/**
 * Abort a video recording
 * This will delete all created files
 * @param video pointer to your video_encapsuler pointer (will be set to NULL by call)
 */
eARMEDIA_ERROR ARMEDIA_VideoEncapsuler_Cleanup (ARMEDIA_VideoEncapsuler_t **encapsuler, bool rename_tempFile)
{
    ARMEDIA_VideoEncapsuler_t* encaps = NULL;

    if (NULL == encapsuler)
    {
        ENCAPSULER_ERROR ("encapsuler double pointer must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    }
    if (NULL == (*encapsuler))
    {
        ENCAPSULER_ERROR ("encapsuler pointer must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    } else {
        encaps = *encapsuler;
    }

    ENCAPSULER_CLEANUP(fclose, encaps->dataFile);
    ENCAPSULER_CLEANUP(fclose, encaps->metaFile);
    remove (encaps->metaFilePath);

    if (rename_tempFile)
    {
        rename (encaps->tempFilePath, encaps->dataFilePath);

        // change date creation in file system
        struct utimbuf fsys_time;
        fsys_time.actime = encaps->creationTime;
        fsys_time.modtime = encaps->creationTime;
        utime(encaps->dataFilePath, &fsys_time);
    }
    else
    {
        remove (encaps->tempFilePath);
        remove (encaps->dataFilePath);
    }

    ENCAPSULER_CLEANUP(free, encaps->video->sps);
    ENCAPSULER_CLEANUP(free, encaps->video->pps);

    ENCAPSULER_CLEANUP(free, encaps->audio);
    ENCAPSULER_CLEANUP(free, encaps->video);
    ENCAPSULER_CLEANUP(free, encaps->metadata);
    ENCAPSULER_CLEANUP(free, encaps);
    *encapsuler = NULL;

    return ARMEDIA_OK;
}

/* DEPRECATED: useless function */
void ARMEDIA_VideoEncapsuler_SetGPSInfos (ARMEDIA_VideoEncapsuler_t* encapsuler, double latitude, double longitude, double altitude)
{
    if (encapsuler != NULL) {
        if (encapsuler->video != NULL) {
            encapsuler->videoGpsInfos.latitude = latitude;
            encapsuler->videoGpsInfos.longitude = longitude;
            encapsuler->videoGpsInfos.altitude = altitude;
        }
    }
}

int ARMEDIA_VideoEncapsuler_TryFixMediaFile (const char *metaFilePath)
{
    // Local values
    ARMEDIA_VideoEncapsuler_t *encapsuler = NULL;
    ARMEDIA_Video_t *video = NULL;
    ARMEDIA_Audio_t *audio = NULL;
    ARMEDIA_Metadata_t *metadata = NULL;
    FILE *metaFile = NULL;
    int ret = 1;
    uint32_t frameNumber = 0;
    uint32_t sampleNumber = 0;
    uint32_t frameTNumber = 0;
    off_t dataSize = 0;
    off_t tmpvidSize = 0;
    uint32_t descriptorSize = 0;
    off_t vsize = 0;
    off_t asize = 0;
    off_t tsize = 0;
    uint32_t stscEntries = 0;
    int endOfSearch = 0;
    off_t fSize = 0;
    long long fSize_tmp = 0;
    uint32_t interDT = 0;
    char fType = '\0';
    char dataType = '\0';
    off_t prevInfoIndex = 0;
    off_t prevSize = 0;

    // Open file for reading
    metaFile = fopen(metaFilePath, "r+b");
    if (NULL == metaFile)
    {
        ret = 0;
        ENCAPSULER_DEBUG ("Unable to open metaFile");
        goto cleanup;
    }

    // Read size from info descriptor
    if (1 != fread (&descriptorSize, sizeof (uint32_t), 1, metaFile))
    {
        ret = 0;
        ENCAPSULER_DEBUG ("Unable to read descriptorSize");
        goto cleanup;
    }
    else if (descriptorSize < sizeof(ARMEDIA_VideoEncapsuler_t) + sizeof(ARMEDIA_Video_t) + sizeof(ARMEDIA_Audio_t) + sizeof(ARMEDIA_Metadata_t))
    {
        ret = 0;
        ENCAPSULER_DEBUG ("Descriptor size (%d) is not the right size supported by this software\n", descriptorSize);
        goto cleanup;
    }

    // Alloc local video pointer
    encapsuler = (ARMEDIA_VideoEncapsuler_t*) calloc(1, sizeof(ARMEDIA_VideoEncapsuler_t));
    video = (ARMEDIA_Video_t*) calloc(1, sizeof(ARMEDIA_Video_t));
    audio = (ARMEDIA_Audio_t*) calloc(1, sizeof(ARMEDIA_Audio_t));
    metadata = (ARMEDIA_Metadata_t*) calloc(1, sizeof(ARMEDIA_Metadata_t));
    if (NULL == video || NULL == audio || NULL == metadata || NULL == encapsuler)
    {
        ret = 0;
        ENCAPSULER_DEBUG ("Unable to alloc structures pointers\n");
        goto cleanup;
    }

    // Read encapsuler
    if (1 != fread (encapsuler, sizeof (ARMEDIA_VideoEncapsuler_t), 1, metaFile))
    {
        ret = 0;
        ENCAPSULER_DEBUG ("Unable to read ARMEDIA_VideoEncapsuler_t from metaFile\n");
        goto cleanup;
    }
    else
    {
        encapsuler->metaFile = metaFile;
        encapsuler->dataFile = NULL;
        encapsuler->video = video;
        encapsuler->audio = audio;
        encapsuler->metadata = metadata;
    }

    if (ARMEDIA_ENCAPSULER_VERSION_NUMBER != encapsuler->version)
    {
        ret = 0;
        ENCAPSULER_DEBUG ("Encapsuler version number differ\n");
        goto cleanup;
    }

    // Read video
    if (1 != fread (video, sizeof (ARMEDIA_Video_t), 1, metaFile))
    {
        ret = 0;
        ENCAPSULER_DEBUG ("Unable to read ARMEDIA_Video_t from metaFile\n");
        goto cleanup;
    }
    else
    {
        video->sps = NULL;
        video->pps = NULL;
    }

    // Allocate / copy SPS/PPS pointers
    if(video->codec == CODEC_MPEG4_AVC) {
        if (0 != video->spsSize && 0 != video->ppsSize)
        {
            video->sps = (uint8_t*) malloc (video->spsSize);
            video->pps = (uint8_t*) malloc (video->ppsSize);
            if (NULL == video->sps ||
                    NULL == video->pps)
            {
                ret = 0;
                ENCAPSULER_DEBUG ("Unable to allocate video sps/pps");
                free (video->sps);
                free (video->pps);
                video->sps = NULL;
                video->pps = NULL;
                goto cleanup;
            }
            else
            {
                if (1 != fread (video->sps, video->spsSize, 1, encapsuler->metaFile))
                {
                    ENCAPSULER_DEBUG ("Unable to read video SPS");
                    ret = 0;
                    goto cleanup;
                }
                else if (1 != fread (video->pps, video->ppsSize, 1, encapsuler->metaFile))
                {
                    ENCAPSULER_DEBUG ("Unable to read video PPS");
                    ret = 0;
                    goto cleanup;
                } // No else
            }
        }
        else
        {
            ENCAPSULER_DEBUG ("Video SPS/PPS sizes are bad");
            ret = 0;
            goto cleanup;
        }
    }

    // Read metadata
    if (1 != fread (metadata, sizeof (ARMEDIA_Metadata_t), 1, metaFile))
    {
        ret = 0;
        ENCAPSULER_DEBUG ("Unable to read ARMEDIA_Metadata_t from metaFile\n");
        goto cleanup;
    }

    // Read audio
    if (1 != fread (audio, sizeof (ARMEDIA_Audio_t), 1, metaFile))
    {
        ret = 0;
        ENCAPSULER_DEBUG ("Unable to read ARMEDIA_Audio_t from metaFile\n");
        goto cleanup;
    }

    // Open temp file
    encapsuler->dataFile = fopen(encapsuler->tempFilePath, "r+b");
    if (NULL == encapsuler->dataFile)
    {
        ret = 0;
        ENCAPSULER_DEBUG ("Unable to open dataFile : %s\n", encapsuler->tempFilePath);
        goto cleanup;
    }

    // Count frames and samples
    fseeko(encapsuler->dataFile, 0, SEEK_END);
    tmpvidSize = ftello(encapsuler->dataFile);

    prevSize = 0;
    while (!endOfSearch)
    {
        prevInfoIndex = ftello(encapsuler->metaFile);
        if ((!feof (encapsuler->metaFile)) &&
                ARMEDIA_ENCAPSULER_NUM_MATCH_PATTERN ==
                fscanf (encapsuler->metaFile, ARMEDIA_ENCAPSULER_INFO_PATTERN, &dataType, &fSize_tmp, &fType, &interDT))
        {
            fSize = fSize_tmp;
            if ((asize + vsize + tsize + encapsuler->dataOffset + fSize) > tmpvidSize)
            {
                ENCAPSULER_DEBUG ("Too many infos : truncate at %zu\n", (size_t)prevInfoIndex);
                fseeko(encapsuler->metaFile, 0, SEEK_SET);
                if (0 != ftruncate (fileno (encapsuler->metaFile), prevInfoIndex))
                {
                    ENCAPSULER_DEBUG ("Unable to truncate metaFile");
                    ret = 0;
                    goto cleanup;
                }
                endOfSearch = 1;
            }
            else
            {
                if (dataType == ARMEDIA_ENCAPSULER_AUDIO_INFO_TAG) {
                    asize += fSize;
                    if (prevSize != fSize) {
                        prevSize = fSize;
                        stscEntries++;
                    }
                    sampleNumber++;
                } else if (dataType == ARMEDIA_ENCAPSULER_VIDEO_INFO_TAG) {
                    vsize += fSize;
                    frameNumber++;
                } else if (dataType == ARMEDIA_ENCAPSULER_METADATA_INFO_TAG) {
                    tsize += fSize;
                    frameTNumber++;
                }
            }
        }
        else
        {
            endOfSearch = 1;
        }
    }

    video->totalsize = vsize;
    audio->totalsize = asize;
    metadata->totalsize = tsize;
    dataSize = asize + vsize + tsize;

    // If needed remove unused frames from .dat file
    dataSize += encapsuler->dataOffset;
    if (tmpvidSize > dataSize)
    {
        if (0 != ftruncate (fileno (encapsuler->dataFile), dataSize))
        {
            ENCAPSULER_DEBUG ("Unable to truncate dataFile");
            ret = 0;
            goto cleanup;
        }
    }

    fseeko(encapsuler->dataFile, 0, SEEK_END);

    audio->sampleCount = sampleNumber;
    video->framesCount = frameNumber;
    metadata->framesCount = frameTNumber;
    audio->stscEntries = stscEntries;
    if (ARMEDIA_OK != ARMEDIA_VideoEncapsuler_Finish (&encapsuler))
    {
        ENCAPSULER_DEBUG ("Unable to finish video");
        /* ARMEDIA_VideoEncapsuler_Finish function will do the cleanup
         * even if it fails */
        ret = 0;
    }

    goto no_cleanup;

cleanup:
    if (!ret)
    {
        uint32_t pathLen;
        char *tmpVidFilePath;

        if (NULL != video)
        {
            ENCAPSULER_DEBUG ("Freeing local copies");
            ENCAPSULER_CLEANUP (free, video->sps);
            video->sps = NULL;
            ENCAPSULER_CLEANUP (free, video->pps);
            video->pps = NULL;
            ENCAPSULER_CLEANUP (fclose, encapsuler->metaFile);
            ENCAPSULER_CLEANUP (fclose, encapsuler->dataFile);
            ENCAPSULER_CLEANUP (free ,video);
            video = NULL;
        }

        if (NULL != metaFile)
        {
            ENCAPSULER_DEBUG ("Closing local file");
            ENCAPSULER_CLEANUP (fclose, metaFile);
        }

        ENCAPSULER_DEBUG ("Cleaning up files");
        ENCAPSULER_DEBUG ("removing %s", metaFilePath);
        remove (metaFilePath);
        pathLen = strlen (metaFilePath);
        tmpVidFilePath = (char*) malloc (pathLen + 1);

        if (NULL != tmpVidFilePath)
        {
            strncpy (tmpVidFilePath, metaFilePath, pathLen);
            tmpVidFilePath[pathLen] = 0;
            strncpy (&tmpVidFilePath[pathLen-7], "tmpvid", 7);
            ENCAPSULER_DEBUG ("removing %s", tmpVidFilePath);
            remove (tmpVidFilePath);
            ENCAPSULER_CLEANUP (free, tmpVidFilePath);
        }
        else
        {
            ENCAPSULER_DEBUG ("Unable to allocate filename buffer, cleanup was not complete");
        }
    }
    ENCAPSULER_CLEANUP (free, audio);
    ENCAPSULER_CLEANUP (free, video);
    ENCAPSULER_CLEANUP (free, metadata);
    ENCAPSULER_CLEANUP (free, encapsuler);

no_cleanup:
    return ret;
}

int ARMEDIA_VideoEncapsuler_changePVATAtomDate (FILE *videoFile, const char *videoDate)
{
    int result = 0;
    int retVal = 0;

    off_t offset = 0;
    uint8_t *atomBuffer = NULL;

    uint64_t atomSize = 0;
    result = seekMediaFileToAtom (videoFile, "pvat", &atomSize, 1);

    if(result)
    {
        offset = ftello(videoFile);
        atomSize -= (2 * sizeof(uint32_t)); // Remove [size - tag] as it was read by seek
        atomBuffer = (uint8_t *)calloc(atomSize, sizeof(uint8_t));

        if (atomSize == fread(atomBuffer, sizeof (uint8_t), atomSize, videoFile))
        {
            json_object *jobj = json_tokener_parse((char *)atomBuffer);
            json_object *jstringVideoDate = json_object_new_string(videoDate);
            json_object_object_add(jobj, "media_date", jstringVideoDate);
            json_object_object_add(jobj, "run_date", jstringVideoDate);

            movie_atom_t *pvatAtom = pvatAtomGen(json_object_to_json_string(jobj));
            fseeko(videoFile, offset - 8, SEEK_SET);

            if (-1 == writeAtomToFile(&pvatAtom, videoFile))
            {
                ENCAPSULER_ERROR ("Error while writing pvatAtom");
            }
            else
            {
                retVal = 1;
            }
        }
        free(atomBuffer);
    }

    return retVal;
}

int ARMEDIA_VideoEncapsuler_addPVATAtom (FILE *videoFile, eARDISCOVERY_PRODUCT product, const char *videoDate)
{
    int retVal = 0;
    struct json_object* pvato;
    pvato = json_object_new_object();
    if (pvato != NULL)
    {
        char prodid[5];
        snprintf(prodid, 5, "%04X", ARDISCOVERY_getProductID(product));
        json_object_object_add(pvato, "product_id", json_object_new_string(prodid));
        json_object_object_add(pvato, "run_date", json_object_new_string(videoDate));
        json_object_object_add(pvato, "media_date", json_object_new_string(videoDate));
        movie_atom_t *pvatAtom = pvatAtomGen(json_object_to_json_string(pvato));

        if (-1 == writeAtomToFile (&pvatAtom, videoFile))
        {
            ENCAPSULER_ERROR ("Error while writing pvatAtom");
        }
        else
        {
            retVal = 1;
        }
        json_object_put(pvato);
    }
    return retVal;
}


