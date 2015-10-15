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
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <utime.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <arpa/inet.h>
#include <json/json.h>
#include <libARDiscovery/ARDiscovery.h>
#include <libARMedia/ARMedia.h>
#include <libARSAL/ARSAL_Print.h>
#include <libARMedia/ARMEDIA_VideoEncapsuler.h>

#define ENCAPSULER_SMALL_STRING_SIZE    (30)
#define ENCAPSULER_INFODATA_MAX_SIZE    (256)
#define ARMEDIA_ENCAPSULER_TAG          "ARMEDIA Encapsuler"

#define ENCAPSULER_DEBUG_ENABLE (1)
#define ENCAPSULER_LOG_TIMESTAMPS (0)

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

struct ARMEDIA_VideoEncapsuler_t
{
    // Encapsuler local data
    uint8_t version;
    uint32_t timescale;
    uint8_t got_iframe;
    uint8_t got_audio;
    ARMEDIA_Video_t* video;
    ARMEDIA_Audio_t* audio;
    time_t creationTime;

    // Atoms datas
    uint32_t mdatAtomOffset;
    uint32_t dataOffset;

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

struct ARMEDIA_Audio_t
{
    eARMEDIA_ENCAPSULER_AUDIO_CODEC codec;
    eARMEDIA_ENCAPSULER_AUDIO_FORMAT format;
    uint16_t nchannel;
    uint16_t freq;
    uint32_t defaultSampleDuration; // in usec
    uint32_t sampleCount; // number of samples
    uint32_t totalsize;

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
    uint64_t totalsize;
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
#ifdef AC_VIDEOENC
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
    retVideo->video = (ARMEDIA_Video_t*) malloc (sizeof(ARMEDIA_Video_t));
    retVideo->audio = NULL;
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

    retVideo->dataFile = (FILE*)fopen64(retVideo->tempFilePath, "w+b");
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
#else
    if (NULL != error)
    {
        *error = ARMEDIA_ERROR_NOT_IMPLEMENTED;
    }
    return NULL;
#endif
}

#define BYTE_STREAM_NALU_START_CODE 0x00000001

static int ARMEDIA_H264StartcodeMatch(uint8_t* pBuf, unsigned int bufSize)
{
    int ret, pos, end;
    uint32_t shiftVal = 0;
    uint8_t* ptr = pBuf;

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

eARMEDIA_ERROR ARMEDIA_VideoEncapsuler_AddFrame (ARMEDIA_VideoEncapsuler_t *encapsuler, ARMEDIA_Frame_Header_t *frameHeader)
{
    uint8_t *data;
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

    if (!frameHeader->frame_size)
    {
        // Do nothing
        ENCAPSULER_DEBUG ("Empty frame\n");
        return ARMEDIA_OK;
    }

    // get frame data
    if (NULL == frameHeader->frame)
    {
        ENCAPSULER_ERROR ("Unable to get frame data (%d bytes) ", frameHeader->frame_size);
        return ARMEDIA_ERROR_ENCAPSULER;
    }

    if (ARMEDIA_ENCAPSULER_FRAMES_COUNT_LIMIT <= video->framesCount)
    {
        ENCAPSULER_ERROR ("Video contains already %d frames, which is the maximum", ARMEDIA_ENCAPSULER_FRAMES_COUNT_LIMIT);
        return ARMEDIA_ERROR_ENCAPSULER;
    }

    data = frameHeader->frame;

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
                return ARMEDIA_ERROR_ENCAPSULER_WAITING_FOR_IFRAME;
            }

            // we'll need to search the "00 00 00 01" pattern to find each header size
            // Search start at index 4 to avoid finding the SPS "00 00 00 01" tag
            for (searchIndex = 4; searchIndex <= frameHeader->frame_size - 4; searchIndex ++)
            {
                if (0 == data[searchIndex  ] &&
                        0 == data[searchIndex+1] &&
                        0 == data[searchIndex+2] &&
                        1 == data[searchIndex+3])
                {
                    break;  // PPS header found
                }
            }
            video->spsSize = searchIndex;

            // Search start at index 4 to avoid finding the SPS "00 00 00 01" tag
            for (searchIndex = video->spsSize+4; searchIndex <= frameHeader->frame_size - 4; searchIndex ++)
            {
                if (0 == data[searchIndex  ] &&
                        0 == data[searchIndex+1] &&
                        0 == data[searchIndex+2] &&
                        1 == data[searchIndex+3])
                {
                    break;  // frame header found
                }
            }

            video->ppsSize = searchIndex - video->spsSize;

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
            memcpy (video->sps, data, video->spsSize);
            memcpy (video->pps, &data[video->spsSize], video->ppsSize);
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

        if (-1 == fseek (encapsuler->dataFile, encapsuler->dataOffset, SEEK_SET))
        {
            ENCAPSULER_ERROR ("Unable to set file write pointer to %d", encapsuler->dataOffset);
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
        encapsuler->mdatAtomOffset = encapsuler->dataOffset - 16;

        // Write video infos to info file header
        descriptorSize = sizeof(ARMEDIA_VideoEncapsuler_t) + sizeof(ARMEDIA_Video_t) + sizeof(ARMEDIA_Audio_t);
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
        fseek(encapsuler->metaFile, sizeof(ARMEDIA_Audio_t), SEEK_CUR);
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

        char infoData [ENCAPSULER_INFODATA_MAX_SIZE] = {0};
        char fTypeChar = 'p';
        if (ARMEDIA_ENCAPSULER_FRAME_TYPE_I_FRAME == frameHeader->frame_type ||
                ARMEDIA_ENCAPSULER_FRAME_TYPE_JPEG == frameHeader->frame_type)
        {
            fTypeChar = 'i';
        }
        snprintf (infoData, ENCAPSULER_INFODATA_MAX_SIZE, ARMEDIA_ENCAPSULER_INFO_PATTERN,
                ARMEDIA_ENCAPSULER_VIDEO_INFO_TAG,
                frameHeader->frame_size,
                fTypeChar,
                (uint32_t)(frameHeader->timestamp - video->lastFrameTimestamp)); // frame duration
        infoLen = strlen (infoData);
        if (infoLen != fwrite (infoData, 1, infoLen, encapsuler->metaFile))
        {
            ENCAPSULER_ERROR ("Unable to write frameInfo into info file");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
    }

    video->lastFrameTimestamp = frameHeader->timestamp;
    video->framesCount++;

    if (video->codec == CODEC_MPEG4_AVC) {
        // Replace the NAL units start code by the NALU size
        int startCodePos = 0, naluStart = 0, naluEnd = 0, sizeLeft = frameHeader->frame_size;
        uint32_t naluSize = 0;
        startCodePos = ARMEDIA_H264StartcodeMatch(data, sizeLeft);
        naluStart = startCodePos;
        while (startCodePos >= 0)
        {
            sizeLeft = frameHeader->frame_size - naluStart - 4;
            startCodePos = ARMEDIA_H264StartcodeMatch(data + naluStart + 4, sizeLeft);
            if (startCodePos >= 0)
            {
                naluEnd = naluStart + 4 + startCodePos;
            }
            else
            {
                naluEnd = frameHeader->frame_size;
            }
            naluSize = htonl(naluEnd - naluStart - 4);
            memcpy(data + naluStart, &naluSize, sizeof(uint32_t));
            naluStart = naluEnd;
        }
    }

    if (frameHeader->frame_size != fwrite (data, 1, frameHeader->frame_size, encapsuler->dataFile))
    {
        ENCAPSULER_ERROR ("Unable to write frame into data file");
        return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
    }

    video->totalsize += frameHeader->frame_size;
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
        fseek(encapsuler->metaFile, sizeof(uint32_t), SEEK_SET);
        if (1 != fwrite (encapsuler, sizeof(ARMEDIA_VideoEncapsuler_t), 1, encapsuler->metaFile))
        {
            ENCAPSULER_ERROR ("Unable to rewrite encapsuler descriptor");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
        // Write Audio info
        uint32_t offset = sizeof(uint32_t) /*descriptor*/ + sizeof(ARMEDIA_VideoEncapsuler_t) + sizeof(ARMEDIA_Video_t);
        offset += encapsuler->video->spsSize + encapsuler->video->ppsSize;
        fseek(encapsuler->metaFile, offset, SEEK_SET);
        if (1 != fwrite (encapsuler->audio, sizeof (ARMEDIA_Audio_t), 1, encapsuler->metaFile))
        {
            ENCAPSULER_ERROR ("Unable to write audio descriptor");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
        fseek(encapsuler->metaFile, 0, SEEK_END); // return to the end of file
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
            if (-zlen >= sampleHeader->sample_size) {
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
                newChunkSize,
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

    // Create internal buffers
    // video
    uint32_t *frameSizeBufferNE   = NULL;
    uint32_t *videoOffsetBuffer   = NULL;
    uint32_t *frameTimeSyncBuffer = NULL;
    uint32_t *iFrameIndexBuffer   = NULL;
    // audio
    uint32_t *audioOffsetBuffer = NULL;
    uint32_t *audioStscBuffer   = NULL;

    struct tm *nowTm;
    uint32_t nbFrames = 0;
    uint32_t nbaChunks = 0;

#if ENCAPSULER_LOG_TIMESTAMPS
    fclose(tslogger);
#endif

    if (NULL == encapsuler)
    {
        ENCAPSULER_ERROR ("encapsuler double pointer must not be null");
        localError = ARMEDIA_ERROR_BAD_PARAMETER;
    }

    if (NULL == *encapsuler)
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
        videoOffsetBuffer   = calloc (video->framesCount, sizeof (uint32_t));
        frameTimeSyncBuffer = calloc (2*video->framesCount, sizeof (uint32_t));
        if (video->codec == CODEC_MPEG4_AVC) {
            iFrameIndexBuffer = calloc (video->framesCount, sizeof (uint32_t));
        }
        if (encaps->got_audio) {
            audioOffsetBuffer = calloc (audio->sampleCount, sizeof (uint32_t));
            audioStscBuffer = calloc(audio->stscEntries*3, sizeof (uint32_t));
        }

        if (NULL == frameSizeBufferNE   ||
                NULL == videoOffsetBuffer   ||
                NULL == frameTimeSyncBuffer ||
                (NULL == iFrameIndexBuffer && video->codec == CODEC_MPEG4_AVC) ||
                (encaps->got_audio && (NULL == audioOffsetBuffer ||
                                       NULL == audioStscBuffer)))
        {
            ENCAPSULER_ERROR ("Unable to allocate buffers for video finish");

            ENCAPSULER_CLEANUP(free, frameSizeBufferNE);
            ENCAPSULER_CLEANUP(free, videoOffsetBuffer);
            ENCAPSULER_CLEANUP(free, frameTimeSyncBuffer);
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
        uint32_t chunkOffset = encaps->dataOffset;
        uint32_t descriptorSize = 0;

        uint32_t vtimescale = encaps->timescale;
        // Video time management
        uint32_t videosttsNentries = 0;
        uint32_t groupInterFrameDT = 0;
        uint32_t groupNframes = 0;
        uint32_t videoDuration = 0;
        uint32_t videoUniqueSize = 0;

        movie_atom_t* moovAtom;         // root
        movie_atom_t* mvhdAtom;         // > mvhd
        movie_atom_t* trakAtom;         // > trak
        movie_atom_t* tkhdAtom;         // | > tkhd
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
            fseek (encaps->metaFile, descriptorSize, SEEK_CUR);
        }

        uint32_t interframeDT;
        uint64_t tmpinterframeDT;
        uint32_t lastChunkSize = 0;
        uint32_t cptAudioStsc = 0;
        while (!feof(encaps->metaFile))
        {
            uint32_t fSize = 0;
            char fType = '\0';
            char dataType = '\0';
            // video
            interframeDT = 0;
            tmpinterframeDT = 0;
            if (ARMEDIA_ENCAPSULER_NUM_MATCH_PATTERN ==
                    fscanf (encaps->metaFile, ARMEDIA_ENCAPSULER_INFO_PATTERN, &dataType, &fSize, &fType, &interframeDT))
            {
                if (dataType == ARMEDIA_ENCAPSULER_VIDEO_INFO_TAG) // video
                {
                    frameSizeBufferNE[nbFrames] = htonl (fSize);
                    videoOffsetBuffer[nbFrames] = htonl (chunkOffset);

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
                    audioOffsetBuffer[nbaChunks] = htonl(chunkOffset);
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

        // get time values
        tzset();
        nowTm = localtime (&(encaps->creationTime));
        time_t cdate = encaps->creationTime - timezone;
        if (nowTm->tm_isdst >= 0) {
            cdate += (3600 * nowTm->tm_isdst);
        }

        // create atoms
        // Generating Atoms
        moovAtom = atomFromData(0, "moov", NULL);
        mvhdAtom = mvhdAtomFromFpsNumFramesAndDate (encaps->timescale, videoDuration, cdate);
        trakAtom = atomFromData(0, "trak", NULL);
        tkhdAtom = tkhdAtomWithResolutionNumFramesFpsAndDate (video->width, video->height, encaps->timescale, videoDuration, cdate, ARMEDIA_VIDEOATOM_MEDIATYPE_VIDEO);
        mdiaAtom = atomFromData(0, "mdia", NULL);
        mdhdAtom = mdhdAtomFromFpsNumFramesAndDate (encaps->timescale, videoDuration, cdate);
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
        stcoDataLen = (8 + (nbFrames * sizeof (uint32_t)));
        stcoBuffer = (uint8_t*) calloc (stcoDataLen, 1);
        memcpy (&stcoBuffer[4], &nbFramesNE, sizeof (uint32_t));
        memcpy (&stcoBuffer[8], videoOffsetBuffer, nbFrames * sizeof (uint32_t));
        stcoAtom = atomFromData (stcoDataLen, "stco", stcoBuffer);
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

            // Generate stco atom from videoOffsetBuffer and nbFrames
            stcoDataLen = (8 + (nbaChunks * sizeof (uint32_t)));
            stcoBuffer = (uint8_t*) calloc (stcoDataLen, 1);
            memcpy (&stcoBuffer[4], &nbaChunksNE, sizeof (uint32_t));
            memcpy (&stcoBuffer[8], audioOffsetBuffer, nbaChunks * sizeof (uint32_t));
            stcoAtom = atomFromData (stcoDataLen, "stco", stcoBuffer);
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
            mdhdAtom = mdhdAtomFromFpsNumFramesAndDate (audio->freq, nbSamples, cdate);
            hdlrmdiaAtom = hdlrAtomForMdia (ARMEDIA_VIDEOATOM_MEDIATYPE_SOUND);

            insertAtomIntoAtom(mdiaAtom, &mdhdAtom);
            insertAtomIntoAtom(mdiaAtom, &hdlrmdiaAtom);
            insertAtomIntoAtom(mdiaAtom, &minfAtom);


            trakAtom = atomFromData(0, "trak", NULL);
            tkhdAtom = tkhdAtomWithResolutionNumFramesFpsAndDate (0, 0, encaps->timescale, videoDuration, cdate, ARMEDIA_VIDEOATOM_MEDIATYPE_SOUND);
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
        uint32_t mdatsize = video->totalsize + 8;
        if (encaps->got_audio) mdatsize += audio->totalsize;
        movie_atom_t *mdatAtom = mdatAtomForFormatWithVideoSize(mdatsize);
        // write mdat atom
        fseek (encaps->dataFile, encaps->mdatAtomOffset, SEEK_SET);
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
            fseek (encaps->dataFile, encaps->mdatAtomOffset - (ARMEDIA_JSON_DESCRIPTION_MAXLENGTH+8), SEEK_SET);
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
    ENCAPSULER_CLEANUP(free, iFrameIndexBuffer);
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
    ARMEDIA_Audio_t *audio = NULL;
    ARMEDIA_Video_t *video = NULL;
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
    audio = encaps->audio;
    video = encaps->video;

    if(NULL!=encaps->dataFile) fclose (encaps->dataFile);
    if(NULL!=encaps->metaFile) fclose (encaps->metaFile);
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

    if (video->sps)
    {
        free (video->sps);
        video->sps = NULL;
    }
    if (video->pps)
    {
        free (video->pps);
        video->pps = NULL;
    }

    ENCAPSULER_CLEANUP(free, audio);
    ENCAPSULER_CLEANUP(free, video);
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
    FILE *metaFile = NULL;
    int ret = 1;
    uint32_t frameNumber = 0;
    uint32_t sampleNumber = 0;
    uint32_t dataSize = 0;
    size_t tmpvidSize = 0;
    uint32_t descriptorSize = 0;
    uint64_t vsize = 0;
    uint64_t asize = 0;
    uint32_t stscEntries = 0;
    int endOfSearch = 0;
    uint32_t fSize = 0;
    uint32_t interDT = 0;
    char fType = '\0';
    char dataType = '\0';
    size_t prevInfoIndex = 0;

    // Open file for reading
    metaFile = fopen (metaFilePath, "r+b");
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
    else if (descriptorSize < sizeof(ARMEDIA_VideoEncapsuler_t) + sizeof(ARMEDIA_Video_t) + sizeof(ARMEDIA_Audio_t))
    {
        ret = 0;
        ENCAPSULER_DEBUG ("Descriptor size (%d) is not the right size supported by this software\n", descriptorSize);
        goto cleanup;
    }

    // Alloc local video pointer
    encapsuler = (ARMEDIA_VideoEncapsuler_t*) calloc(1, sizeof(ARMEDIA_VideoEncapsuler_t));
    video = (ARMEDIA_Video_t*) calloc(1, sizeof(ARMEDIA_Video_t));
    audio = (ARMEDIA_Audio_t*) calloc(1, sizeof(ARMEDIA_Audio_t));
    if (NULL == video || NULL == audio || NULL == encapsuler)
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

    // Read audio
    if (1 != fread (audio, sizeof (ARMEDIA_Audio_t), 1, metaFile))
    {
        ret = 0;
        ENCAPSULER_DEBUG ("Unable to read ARMEDIA_Audio_t from metaFile\n");
        goto cleanup;
    }

    // Open temp file
    encapsuler->dataFile = fopen (encapsuler->tempFilePath, "r+b");
    if (NULL == encapsuler->dataFile)
    {
        ret = 0;
        ENCAPSULER_DEBUG ("Unable to open dataFile : %s\n", encapsuler->tempFilePath);
        goto cleanup;
    }

    // Count frames and samples
    fseek (encapsuler->dataFile, 0, SEEK_END);
    tmpvidSize = ftell (encapsuler->dataFile);

    uint32_t prevSize = 0;
    while (!endOfSearch)
    {
        prevInfoIndex = ftell (encapsuler->metaFile);
        if ((!feof (encapsuler->metaFile)) &&
                ARMEDIA_ENCAPSULER_NUM_MATCH_PATTERN ==
                fscanf (encapsuler->metaFile, ARMEDIA_ENCAPSULER_INFO_PATTERN, &dataType, &fSize, &fType, &interDT))
        {
            if ((asize + vsize + encapsuler->dataOffset + fSize) > tmpvidSize)
            {
                ENCAPSULER_DEBUG ("Too many infos : truncate at %u\n", prevInfoIndex);
                fseek (encapsuler->metaFile, 0, SEEK_SET);
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
    dataSize = asize + vsize;

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

    fseek (encapsuler->dataFile, 0, SEEK_END);

    audio->sampleCount = sampleNumber;
    video->framesCount = frameNumber;
    audio->stscEntries = stscEntries;
    if (ARMEDIA_OK != ARMEDIA_VideoEncapsuler_Finish (&encapsuler))
    {
        ENCAPSULER_DEBUG ("Unable to finish video");
        /* ARMEDIA_VideoEncapsuler_Finish function will do the cleanup
         * even if it fails */
        ret = 0;
        goto no_cleanup;
    }

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
            metaFile = NULL;
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
    ENCAPSULER_CLEANUP (free, encapsuler);

no_cleanup:
    return ret;
}

int ARMEDIA_VideoEncapsuler_changePVATAtomDate (FILE *videoFile, const char *videoDate)
{
    int result = 0;
    int retVal = 0;

    long offset = 0;
    uint8_t *atomBuffer = NULL;

    uint64_t atomSize = 0;
    result = seekMediaFileToAtom (videoFile, "pvat", &atomSize, 1);

    if(result)
    {
        offset = ftell(videoFile);
        atomSize -= (2 * sizeof(uint32_t)); // Remove [size - tag] as it was read by seek
        atomBuffer = (uint8_t *)calloc(atomSize, sizeof(uint8_t));

        if (atomSize == fread(atomBuffer, sizeof (uint8_t), atomSize, videoFile))
        {
            json_object *jobj = json_tokener_parse((char *)atomBuffer);
            json_object *jstringVideoDate = json_object_new_string(videoDate);
            json_object_object_add(jobj, "media_date", jstringVideoDate);
            json_object_object_add(jobj, "run_date", jstringVideoDate);

            movie_atom_t *pvatAtom = pvatAtomGen(json_object_to_json_string(jobj));
            fseek(videoFile, offset - 8, SEEK_SET);

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


