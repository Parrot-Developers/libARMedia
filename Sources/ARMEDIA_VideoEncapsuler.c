/*
 * ARMEDIA_Videoencapsuler.c
 * ARDroneLib
 *
 * Created by n.brulez on 18/08/11
 * Copyright 2011 Parrot SA. All rights reserved.
 *
 */
#include <libARMedia/ARMedia.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <libARMedia/ARMEDIA_VideoEncapsuler.h>
#include <json/json.h>
#include <libARDiscovery/ARDiscovery.h>

#define ENCAPSULER_SMALL_STRING_SIZE (30)
#define ENCAPSULER_INFODATA_MAX_SIZE (256)

#define ENCAPSULER_DDT_TOLERANCE_PCT (15)

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

struct ARMEDIA_VideoEncapsuler_t
{
    uint32_t fps;
    uint32_t timescale;
    ARMEDIA_Video_t* video;
};

struct ARMEDIA_Video_t
{
    uint8_t version;
    // Provided to constructor
    eARDISCOVERY_PRODUCT product;
    char uuid[UUID_MAXLENGTH];
    char runDate[DATETIME_MAXLENGTH];
    uint32_t defaultFrameDuration;
    // Read from frames frameHeader
    eARMEDIA_ENCAPSULER_CODEC videoCodec;
    uint16_t width;
    uint16_t height;
    uint64_t totalsize;
    // Private datas
    char infoFilePath [ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE]; // Keep this so we can delete the file
    char outFilePath  [ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE]; // Keep this so we can rename the output file
    char tempFilePath [ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE]; // Keep this so we can rename the output file
    FILE *infoFile;
    FILE *outFile;
    uint32_t framesCount; // Number of frames
    uint32_t mdatAtomOffset;
    uint32_t framesDataOffset;

    /* H.264 only values */
    uint8_t *sps;
    uint8_t *pps;
    uint16_t spsSize; // full size with headers 0x00000001
    uint16_t ppsSize; // full size with headers 0x00000001

    /* Slices recording values */
    uint32_t lastFrameNumber;
    eARMEDIA_ENCAPSULER_FRAME_TYPE lastFrameType;
    uint32_t currentFrameSize;

    uint64_t lastFrameTimestamp;

    time_t creationTime;
    uint16_t droneVersion;
    ARMEDIA_videoGpsInfos_t videoGpsInfos;
};

#define ENCAPSULER_DEBUG_ENABLE (0)

// File extension for temporary files
#define TEMPFILE_EXT ".tmpvid"

#define ARMEDIA_ENCAPSULER_TAG      "ARMedia Video Encapsuler"

#define ENCAPSULER_ERROR(...)                                           \
    do                                                                  \
    {                                                                   \
        fprintf (stderr, "ARMedia video encapsuler error (%s @ %d) : ", __FUNCTION__, __LINE__); \
        fprintf (stderr, __VA_ARGS__);                                  \
        fprintf (stderr, "\n");                                         \
    } while (0)

#if defined (DEBUG) || ENCAPSULER_DEBUG_ENABLE
#define ENCAPSULER_DEBUG(...)                                           \
    do                                                                  \
    {                                                                   \
        fprintf (stdout, "(%s @ %d) : ", __FUNCTION__, __LINE__); \
        fprintf (stdout, __VA_ARGS__);                                  \
        fprintf (stdout, "\n");                                         \
    } while (0)
#else
#define ENCAPSULER_DEBUG(...)
#endif

#define DEF_ATOM(NAME) movie_atom_t* NAME##Atom
#define EMPTY_ATOM(NAME) NAME##Atom = atomFromData (0, #NAME, NULL)

#define ENCAPSULER_CLEANUP(FUNC, PTR)           \
    do                                          \
    {                                           \
        if (NULL != PTR)                        \
        {                                       \
            FUNC(PTR);                          \
        }                                       \
    } while (0)

#ifdef AC_VIDEOENC
ARMEDIA_VideoEncapsuler_t *ARMEDIA_VideoEncapsuler_New (const char *videoPath, int fps, char* uuid, char* runDate, eARDISCOVERY_PRODUCT product, eARMEDIA_ERROR *error)
{
    ARMEDIA_VideoEncapsuler_t *retVideo = NULL;

    if (NULL == error)
    {
        ENCAPSULER_ERROR ("error pointer must not be null");
        return NULL;
    }
    if (NULL == videoPath)
    {
        ENCAPSULER_ERROR ("videoPath pointer must not be null");
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
    retVideo->video = (ARMEDIA_Video_t*) malloc (sizeof(ARMEDIA_Video_t));
    memset(retVideo->video, 0, sizeof(ARMEDIA_Video_t));

    retVideo->fps = (uint32_t)fps;
    retVideo->timescale = (uint32_t)(fps * 2000);
    retVideo->video->version = ARMEDIA_ENCAPSULER_VERSION_NUMBER;
    retVideo->video->product = product;
    retVideo->video->defaultFrameDuration = 1000000 / fps;
    retVideo->video->lastFrameNumber = UINT32_MAX;
    retVideo->video->currentFrameSize = 0;
    retVideo->video->height = 0;
    retVideo->video->width = 0;
    retVideo->video->totalsize = 0;
    retVideo->video->videoCodec = 0;
    retVideo->video->sps = NULL;
    retVideo->video->spsSize = 0;
    retVideo->video->pps = NULL;
    retVideo->video->ppsSize = 0;
    retVideo->video->videoGpsInfos.latitude = 500.0;
    retVideo->video->videoGpsInfos.longitude = 500.0;
    retVideo->video->videoGpsInfos.altitude = 500.0;
    retVideo->video->lastFrameType = ARMEDIA_ENCAPSULER_FRAME_TYPE_UNKNNOWN;
    strncpy  (retVideo->video->uuid, uuid, UUID_MAXLENGTH);
    strncpy  (retVideo->video->runDate, runDate, DATETIME_MAXLENGTH);
    snprintf (retVideo->video->infoFilePath, ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE, "%s%s", videoPath, INFOFILE_EXT);
    snprintf (retVideo->video->tempFilePath, ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE, "%s%s", videoPath, TEMPFILE_EXT);
    snprintf (retVideo->video->outFilePath,  ARMEDIA_ENCAPSULER_VIDEO_PATH_SIZE, "%s", videoPath);
    retVideo->video->infoFile = fopen(retVideo->video->infoFilePath, "w+b");
    if (NULL == retVideo->video->infoFile)
    {
        ENCAPSULER_ERROR ("Unable to open file %s for writing", retVideo->video->infoFilePath);
        *error = ARMEDIA_ERROR_ENCAPSULER;
        free (retVideo);
        retVideo = NULL;
        return NULL;
    }

    retVideo->video->outFile = (FILE*)fopen64(retVideo->video->tempFilePath, "w+b");

    if (NULL == retVideo->video->outFile)
    {
        ENCAPSULER_ERROR ("Unable to open file %s for writing", videoPath);
        *error = ARMEDIA_ERROR_ENCAPSULER;
        fclose (retVideo->video->infoFile);
        free (retVideo);
        retVideo = NULL;
        return NULL;
    }
    retVideo->video->framesCount = 0;
    retVideo->video->mdatAtomOffset = 0;
    retVideo->video->framesDataOffset = 0;

    retVideo->video->creationTime = time (NULL);

    *error = ARMEDIA_OK;

    return retVideo;
}
#endif

eARMEDIA_ERROR ARMEDIA_VideoEncapsuler_AddSlice (ARMEDIA_VideoEncapsuler_t *encapsuler, ARMEDIA_Frame_Header_t *frameHeader)
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
        ENCAPSULER_ERROR ("slice pointer must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    }

    if (!frameHeader->frame_size)
    {
        // Do nothing
        ENCAPSULER_DEBUG ("Empty slice\n");
        return ARMEDIA_OK;
    }

    // get frame data
    if (NULL == frameHeader->frame)
    {
        ENCAPSULER_ERROR ("Unable to allocate local data (%d bytes) ", frameHeader->frame_size);
        return ARMEDIA_ERROR_ENCAPSULER;
    }

    data = frameHeader->frame;

    // First slice
    if (!video->width)
    {
        // Ensure that the codec is h.264 or mjpeg (mp4 not supported)
        if ((CODEC_MPEG4_AVC != frameHeader->video_codec) && (CODEC_MOTION_JPEG != frameHeader->video_codec))
        {
            ENCAPSULER_ERROR ("Only h.264 or mjpeg codec are supported");
            return ARMEDIA_ERROR_ENCAPSULER_BAD_CODEC;
        }

        // Init video structure
        video->width = frameHeader->video_width;
        video->height = frameHeader->video_height;
        video->videoCodec = frameHeader->video_codec;

        // If codec is H.264, save SPS/PPS
        if (CODEC_MPEG4_AVC == video->videoCodec)
        {
            if (ARMEDIA_ENCAPSULER_FRAME_TYPE_I_FRAME != frameHeader->frame_type)
            {
                // Wait until we get an IFrame-slice 1 to start the actual recording
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
        rewind(video->outFile);
        ftypAtom = ftypAtomForFormatAndCodecWithOffset (video->videoCodec, &(video->framesDataOffset));
        if (NULL == ftypAtom)
        {
            ENCAPSULER_ERROR ("Unable to create ftyp atom");
            return ARMEDIA_ERROR_ENCAPSULER;
        }

        if (-1 == writeAtomToFile (&ftypAtom, video->outFile))
        {
            ENCAPSULER_ERROR ("Unable to write ftyp atom");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }

        // Add an offset for PVAT at beginning
        video->framesDataOffset += ARMEDIA_JSON_DESCRIPTION_MAXLENGTH+8;

        if (-1 == fseek (video->outFile, video->framesDataOffset, SEEK_SET))
        {
            ENCAPSULER_ERROR ("Unable to set file write pointer to %d", video->framesDataOffset);
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
        video->mdatAtomOffset = video->framesDataOffset - 16;

        // Write video infos to info file header
        descriptorSize = sizeof (ARMEDIA_Video_t);
        if (video->videoCodec == CODEC_MPEG4_AVC) descriptorSize += video->spsSize + video->ppsSize;

        // Write total length
        if (1 != fwrite (&descriptorSize, sizeof (uint32_t), 1, video->infoFile))
        {
            ENCAPSULER_ERROR ("Unable to write size of video descriptor");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
        // Write video_t info
        if (1 != fwrite (video, sizeof (ARMEDIA_Video_t), 1, video->infoFile))
        {
            ENCAPSULER_ERROR ("Unable to write video descriptor");
            return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }

        if (video->videoCodec == CODEC_MPEG4_AVC)
        {
            // Write SPS
            if (video->spsSize != fwrite (video->sps, sizeof (uint8_t), video->spsSize, video->infoFile))
            {
                ENCAPSULER_ERROR ("Unable to write sps header");
                return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
            // Write PPS
            if (video->ppsSize != fwrite (video->pps, sizeof (uint8_t), video->ppsSize, video->infoFile))
            {
                ENCAPSULER_ERROR ("Unable to write pps header");
                return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
        }
    }

    // Normal operation : file pointer is at end of file
    if (video->videoCodec != frameHeader->video_codec ||
        video->width != frameHeader->video_width ||
        video->height != frameHeader->video_height)
    {
        ENCAPSULER_ERROR ("New slice don't match the video size/codec");
        return ARMEDIA_ERROR_ENCAPSULER_BAD_CODEC;
    }

    // New frame, write all infos about last one
    if (video->lastFrameNumber != frameHeader->frame_number)
    {
        // if frame TS is null, set it as last TS + default duration (from fps)
        if (frameHeader->timestamp == 0) {
            frameHeader->timestamp = video->lastFrameTimestamp + video->defaultFrameDuration;
        }

        // Use lastFrameType to check that we don't write infos about a null frame
        if (ARMEDIA_ENCAPSULER_FRAME_TYPE_UNKNNOWN != video->lastFrameType)
        {
            uint32_t infoLen;

            char infoData [ENCAPSULER_INFODATA_MAX_SIZE] = {0};
            char fTypeChar = 'p';
            if (ARMEDIA_ENCAPSULER_FRAME_TYPE_I_FRAME == video->lastFrameType ||
                ARMEDIA_ENCAPSULER_FRAME_TYPE_JPEG == video->lastFrameType)
            {
                fTypeChar = 'i';
            }
            snprintf (infoData, ENCAPSULER_INFODATA_MAX_SIZE, ARMEDIA_ENCAPSULER_INFO_PATTERN,
                    video->currentFrameSize,
                    fTypeChar,
                    (uint32_t)(frameHeader->timestamp - video->lastFrameTimestamp)); // frame duration
            infoLen = strlen (infoData);
            if (infoLen != fwrite (infoData, 1, infoLen, video->infoFile))
            {
                ENCAPSULER_ERROR ("Unable to write frameInfo into info file");
                return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
        }
        video->currentFrameSize = 0;
        video->lastFrameType = frameHeader->frame_type;
        video->lastFrameNumber = frameHeader->frame_number;
        video->lastFrameTimestamp = frameHeader->timestamp;
        video->framesCount++;
        if (ARMEDIA_ENCAPSULER_FRAMES_COUNT_LIMIT <= video->framesCount)
        {
            ENCAPSULER_ERROR ("Video contains already %d frames, which is the maximum", ARMEDIA_ENCAPSULER_FRAMES_COUNT_LIMIT);
            return ARMEDIA_ERROR_ENCAPSULER;
        }
    }

    if (video->videoCodec == CODEC_MPEG4_AVC) {
        // Modify slices before writing
        if (ARMEDIA_ENCAPSULER_FRAME_TYPE_I_FRAME == frameHeader->frame_type) {
            // set correct sizes for SPS & PPS headers + frame
            uint32_t sps_size_NE = htonl (video->spsSize - 4);
            uint32_t pps_size_NE = htonl (video->ppsSize - 4);
            uint32_t f_size_NE   = htonl (frameHeader->frame_size - (video->spsSize + video->ppsSize + 4));
            memcpy ( data                                 , &sps_size_NE, sizeof (uint32_t));
            memcpy (&data[video->spsSize]                 , &pps_size_NE, sizeof (uint32_t));
            memcpy (&data[video->spsSize + video->ppsSize], &f_size_NE  , sizeof (uint32_t));
        } else if (ARMEDIA_ENCAPSULER_FRAME_TYPE_P_FRAME == frameHeader->frame_type) {
            // set correct size for frame
            uint32_t f_size_NE = htonl (frameHeader->frame_size - 4);
            memcpy (data, &f_size_NE, sizeof (uint32_t));
        }
    }

    if (frameHeader->frame_size != fwrite (data, 1, frameHeader->frame_size, video->outFile))
    {
        ENCAPSULER_ERROR ("Unable to write slice into data file");
        return ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
    }

    video->currentFrameSize += frameHeader->frame_size;
    video->totalsize += frameHeader->frame_size;

    // synchronisation
    if ((video->framesCount % 10) == 0) {
        fflush (video->outFile);
        fsync(fileno(video->outFile));
        fflush (video->infoFile);
        fsync(fileno(video->infoFile));
    }

    return ARMEDIA_OK;
}

eARMEDIA_ERROR ARMEDIA_VideoEncapsuler_Finish (ARMEDIA_VideoEncapsuler_t **encapsuler)
{
    eARMEDIA_ERROR localError = ARMEDIA_OK;
    ARMEDIA_Video_t *myVideo = NULL;

    // Create internal buffers
    uint32_t *frameSizeBufferNE   = NULL;
    uint32_t *frameOffsetBuffer   = NULL;
    uint32_t *frameTimeSyncBuffer = NULL;
    uint32_t *iFrameIndexBuffer   = NULL;

    struct tm *nowTm;
    uint64_t dataTotalSize = 0;
    uint32_t nbFrames = 0;
    char dateInfoString[ENCAPSULER_SMALL_STRING_SIZE] = {0};

    if (NULL == *encapsuler)
    {
        ENCAPSULER_ERROR ("encapsuler pointer must not be null");
        localError = ARMEDIA_ERROR_BAD_PARAMETER;
    }

    if (NULL == (*encapsuler)->video)
    {
        ENCAPSULER_ERROR ("video pointer must not be null");
        localError = ARMEDIA_ERROR_BAD_PARAMETER;
    }


    if (ARMEDIA_OK == localError)
    {
        myVideo = (*encapsuler)->video; // ease of reading

        if (!myVideo->width)
        {
            // Video was not initialized
            ENCAPSULER_ERROR ("video was not initialized");
            localError =  ARMEDIA_ERROR_BAD_PARAMETER;
        } // No else
    }

    if (ARMEDIA_OK == localError)
    {
        // For slice, we'll write infos about the last frame here
        if (0 != myVideo->currentFrameSize)
        {
            if (ARMEDIA_ENCAPSULER_FRAME_TYPE_UNKNNOWN != myVideo->lastFrameType)
            {
                char infoData [ENCAPSULER_INFODATA_MAX_SIZE] = {0};
                char fTypeChar = 'p';
                uint32_t infoLen;

                if (ARMEDIA_ENCAPSULER_FRAME_TYPE_I_FRAME == myVideo->lastFrameType ||
                    ARMEDIA_ENCAPSULER_FRAME_TYPE_JPEG == myVideo->lastFrameType)
                {
                    fTypeChar = 'i';
                }
                snprintf (infoData, ENCAPSULER_INFODATA_MAX_SIZE, ARMEDIA_ENCAPSULER_INFO_PATTERN,
                        myVideo->currentFrameSize,
                        fTypeChar,
                        myVideo->defaultFrameDuration);

                infoLen = strlen (infoData);
                if (infoLen != fwrite (infoData, 1, infoLen, myVideo->infoFile))
                {
                    ENCAPSULER_ERROR ("Unable to write frameInfo into info file");
                    localError = ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
                }
            }
        }
    }


    if (ARMEDIA_OK == localError)
    {
        // Init internal buffers
        frameSizeBufferNE   = calloc (myVideo->framesCount, sizeof (uint32_t));
        frameOffsetBuffer   = calloc (myVideo->framesCount, sizeof (uint32_t));
        frameTimeSyncBuffer = calloc (2*myVideo->framesCount, sizeof (uint32_t));
        iFrameIndexBuffer   = calloc (myVideo->framesCount, sizeof (uint32_t));

        if (NULL == frameSizeBufferNE   ||
            NULL == frameOffsetBuffer   ||
            NULL == frameTimeSyncBuffer ||
            NULL == iFrameIndexBuffer )
        {
            ENCAPSULER_ERROR ("Unable to allocate buffers for video finish");
            free (frameSizeBufferNE);
            frameSizeBufferNE = NULL;
            free (frameOffsetBuffer);
            frameOffsetBuffer = NULL;
            free (frameTimeSyncBuffer);
            frameTimeSyncBuffer = NULL;
            free (iFrameIndexBuffer);
            iFrameIndexBuffer = NULL;
            localError = ARMEDIA_ERROR_ENCAPSULER;
        }
    }


    if (ARMEDIA_OK == localError)
    {
        // Init internal counters
        uint32_t nbIFrames = 0;
        uint32_t frameOffset = myVideo->framesDataOffset;
        uint32_t descriptorSize = 0;
        // Time management
        uint32_t sttsNentries = 0;
        uint32_t groupSumDT = 0;
        uint32_t groupMeanDT = 0;
        uint32_t groupNframes = 0;
        uint32_t totalDuration = 0;
        uint32_t vtimescale = (*encapsuler)->timescale;

        movie_atom_t *mvhdAtom;
        movie_atom_t *tkhdAtom;
        movie_atom_t *mdhdAtom;
        movie_atom_t *hdlrAtom;
        movie_atom_t *vmhdAtom;
        movie_atom_t *hdlr2Atom;
        movie_atom_t *drefAtom;
        movie_atom_t *stsdAtom;
        DEF_ATOM(moov);
        DEF_ATOM(trak);
        DEF_ATOM(mdia);
        DEF_ATOM(minf);
        DEF_ATOM(dinf);
        DEF_ATOM(stbl);
        DEF_ATOM(udta);

        uint32_t stssDataLen;
        uint8_t *stssBuffer;
        uint32_t nenbIFrames;
        movie_atom_t *stscAtom;
        movie_atom_t *stssAtom;

        // Generate stts atom from frameTimeSyncBuffer
        uint32_t sttsDataLen;
        uint8_t *sttsBuffer;
        uint32_t sttsNentriesNE;
        movie_atom_t *sttsAtom;

        // Generate stsz atom from frameSizeBufferNE and nbFrames
        uint32_t stszDataLen;
        uint8_t *stszBuffer;
        uint32_t nenbFrames;
        movie_atom_t *stszAtom;

        // Generate stco atom from frameOffsetBuffer and nbFrames
        uint32_t stcoDataLen;
        uint8_t *stcoBuffer;
        movie_atom_t *stcoAtom;

        movie_atom_t *swrAtom;
        movie_atom_t *dayAtom;

        // Read info file
        rewind (myVideo->infoFile);
        // Skip video descriptor

        if (1 != fread(&descriptorSize, sizeof (uint32_t), 1, myVideo->infoFile))
        {
            ENCAPSULER_ERROR ("Unable to read descriptor size (probably an old .infovid file)");
            rewind (myVideo->infoFile);
        }
        else
        {
            fseek (myVideo->infoFile, descriptorSize, SEEK_CUR);
        }

        while (! feof (myVideo->infoFile) && nbFrames < myVideo->framesCount)
        {
            uint32_t fSize = 0;
            char fType = 'a';
            uint32_t interframeDT = 0;
            uint64_t tmpinterframeDT = 0;
            if (ARMEDIA_ENCAPSULER_NUM_MATCH_PATTERN ==
                fscanf (myVideo->infoFile, ARMEDIA_ENCAPSULER_INFO_PATTERN, &fSize, &fType, &interframeDT))
            {
                frameSizeBufferNE [nbFrames] = htonl (fSize);
                frameOffsetBuffer [nbFrames] = htonl (frameOffset);

                // from microseconds to time units
                tmpinterframeDT = ((uint64_t)vtimescale) * ((uint64_t) interframeDT);
                interframeDT = (uint32_t)(tmpinterframeDT / 1000000);
                // Time sync mgmt
                if ((interframeDT < (groupMeanDT * (100 - ENCAPSULER_DDT_TOLERANCE_PCT) / 100))
                ||  (interframeDT > (groupMeanDT * (100 + ENCAPSULER_DDT_TOLERANCE_PCT) / 100))) {
                    // out tolerance
                    if (groupMeanDT != 0) { // not first frame
                        frameTimeSyncBuffer[2*sttsNentries] = htonl(groupNframes);
                        frameTimeSyncBuffer[2*sttsNentries+1] = htonl(groupMeanDT);
                        totalDuration += groupNframes * groupMeanDT;
                        ENCAPSULER_DEBUG("STTS Entry | %04u | %08u |", groupNframes, groupMeanDT);
                        sttsNentries++;
                    }
                    groupNframes = 1;
                    groupSumDT = interframeDT;
                    groupMeanDT = interframeDT;
                } else {
                    // in tolerance
                    groupSumDT += interframeDT;
                    groupNframes++;
                    groupMeanDT = groupSumDT / groupNframes;
                }

                frameOffset += fSize;
                if ('i' == fType)
                {
                    iFrameIndexBuffer [nbIFrames++] = htonl (nbFrames+1);
                }
                nbFrames ++;
                dataTotalSize += (uint64_t) fSize;
            }
        }
        frameTimeSyncBuffer[2*sttsNentries] = htonl(groupNframes);
        frameTimeSyncBuffer[2*sttsNentries+1] = htonl(groupMeanDT);
        totalDuration += groupNframes * groupMeanDT;
        ENCAPSULER_DEBUG("STTS Entry | %04u | %08u |", groupNframes, groupMeanDT);
        sttsNentries++;

        // get time values
        tzset();
        nowTm = localtime (&(myVideo->creationTime));
        time_t cdate = myVideo->creationTime - timezone;
        if (nowTm->tm_isdst >= 0) {
            cdate += (3600 * nowTm->tm_isdst);
        }

        // create atoms
        // Generating Atoms
        EMPTY_ATOM(moov);
        mvhdAtom = mvhdAtomFromFpsNumFramesAndDate ((*encapsuler)->timescale, totalDuration, cdate);
        EMPTY_ATOM(trak);
        tkhdAtom = tkhdAtomWithResolutionNumFramesFpsAndDate (myVideo->width, myVideo->height, (*encapsuler)->timescale, totalDuration, cdate);
        EMPTY_ATOM(mdia);
        mdhdAtom = mdhdAtomFromFpsNumFramesAndDate ((*encapsuler)->timescale, totalDuration, cdate);
        hdlrAtom = hdlrAtomForMdia ();
        EMPTY_ATOM(minf);
        vmhdAtom = vmhdAtomGen ();
        if (CODEC_MPEG4_AVC == myVideo->videoCodec)
            hdlr2Atom = hdlrAtomForMinf ();
        EMPTY_ATOM(dinf);
        drefAtom = drefAtomGen ();
        EMPTY_ATOM(stbl);
        stsdAtom = stsdAtomWithResolutionCodecSpsAndPps (myVideo->width, myVideo->height, myVideo->videoCodec, &myVideo->sps[4], myVideo->spsSize -4, &myVideo->pps[4], myVideo->ppsSize -4);

        // Generate stts atom from frameTimeSyncBuffer
        sttsDataLen = (8 + 2 * sttsNentries * sizeof(uint32_t));
        sttsBuffer = (uint8_t*) calloc (sttsDataLen, 1);
        sttsNentriesNE = htonl(sttsNentries);
        memcpy (&sttsBuffer[4], &sttsNentriesNE, sizeof(uint32_t));
        memcpy (&sttsBuffer[8], frameTimeSyncBuffer, 2 * sttsNentries * sizeof(uint32_t));
        sttsAtom = atomFromData (sttsDataLen, "stts", sttsBuffer);
        //sttsAtom = sttsAtomWithNumFrames (nbFrames, (*encapsuler)->timescale, (*encapsuler)->fps);
        free(sttsBuffer);

        if (CODEC_MPEG4_AVC == myVideo->videoCodec) {
            // Generate stss atom from iFramesIndexBuffer and nbIFrames
            stssDataLen = (8 + (nbIFrames * sizeof (uint32_t)));
            stssBuffer = (uint8_t*) calloc (stssDataLen, 1);
            nenbIFrames = htonl (nbIFrames);
            memcpy (&stssBuffer[4], &nenbIFrames, sizeof (uint32_t));
            memcpy (&stssBuffer[8], iFrameIndexBuffer, nbIFrames * sizeof (uint32_t));
            stssAtom = atomFromData (stssDataLen, "stss", stssBuffer);
            free (stssBuffer);
        }

        stscAtom = stscAtomGen ();

        // Generate stsz atom from frameSizeBufferNE and nbFrames
        stszDataLen = (12 + (nbFrames * sizeof (uint32_t)));
        stszBuffer = (uint8_t*) calloc (stszDataLen, 1);
        nenbFrames = htonl (nbFrames);
        memcpy (&stszBuffer[8], &nenbFrames, sizeof (uint32_t));
        memcpy (&stszBuffer[12], frameSizeBufferNE, nbFrames * sizeof (uint32_t));
        stszAtom = atomFromData (stszDataLen, "stsz", stszBuffer);
        free (stszBuffer);

        // Generate stco atom from frameOffsetBuffer and nbFrames
        stcoDataLen = (8 + (nbFrames * sizeof (uint32_t)));
        stcoBuffer = (uint8_t*) calloc (stcoDataLen, 1);
        memcpy (&stcoBuffer[4], &nenbFrames, sizeof (uint32_t));
        memcpy (&stcoBuffer[8], frameOffsetBuffer, nbFrames * sizeof (uint32_t));
        stcoAtom = atomFromData (stcoDataLen, "stco", stcoBuffer);
        free (stcoBuffer);

        EMPTY_ATOM (udta);
        swrAtom = metadataAtomFromTagAndValue ("swr ", "Parrot Drone");

        strftime(dateInfoString, ENCAPSULER_SMALL_STRING_SIZE, ARMEDIA_JSON_DESCRIPTION_DATE_FMT, nowTm);
        dayAtom = metadataAtomFromTagAndValue ("day ", dateInfoString);

       // Create atom tree
        insertAtomIntoAtom (udtaAtom, &swrAtom);
        insertAtomIntoAtom (udtaAtom, &dayAtom);

        insertAtomIntoAtom (stblAtom, &stsdAtom);
        insertAtomIntoAtom (stblAtom, &sttsAtom);
        if (CODEC_MPEG4_AVC == myVideo->videoCodec)
            insertAtomIntoAtom (stblAtom, &stssAtom);

        insertAtomIntoAtom (stblAtom, &stscAtom);
        insertAtomIntoAtom (stblAtom, &stszAtom);
        insertAtomIntoAtom (stblAtom, &stcoAtom);

        insertAtomIntoAtom (dinfAtom, &drefAtom);

        insertAtomIntoAtom (minfAtom, &vmhdAtom);
        if (CODEC_MPEG4_AVC == myVideo->videoCodec)
            insertAtomIntoAtom (minfAtom, &hdlr2Atom);
        insertAtomIntoAtom (minfAtom, &dinfAtom);
        insertAtomIntoAtom (minfAtom, &stblAtom);

        insertAtomIntoAtom (mdiaAtom, &mdhdAtom);
        insertAtomIntoAtom (mdiaAtom, &hdlrAtom);
        insertAtomIntoAtom (mdiaAtom, &minfAtom);

        insertAtomIntoAtom (trakAtom, &tkhdAtom);
        insertAtomIntoAtom (trakAtom, &mdiaAtom);

        insertAtomIntoAtom (moovAtom, &mvhdAtom);
        insertAtomIntoAtom (moovAtom, &trakAtom);
        insertAtomIntoAtom (moovAtom, &udtaAtom);

        if (-1 == writeAtomToFile (&moovAtom, myVideo->outFile))
        {
            ENCAPSULER_ERROR ("Error while writing moovAtom\n");
            localError = ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
    }

    if (ARMEDIA_OK == localError)
    {
        movie_atom_t *mdatAtom = mdatAtomForFormatWithVideoSize (myVideo->totalsize + 8);
        // write mdat atom
        fseek (myVideo->outFile, myVideo->mdatAtomOffset, SEEK_SET);
        if (-1 == writeAtomToFile (&mdatAtom, myVideo->outFile))
        {
            ENCAPSULER_ERROR ("Error while writing mdatAtom\n");
            localError = ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
        }
    }

    /* pvat insertion at the benning of the file */
    if (ARMEDIA_OK == localError)
    {
        char* pvatstr = ARMEDIA_VideoAtom_GetPVATString((*encapsuler)->video->product, (*encapsuler)->video->uuid, (*encapsuler)->video->runDate, nowTm);
        if (pvatstr != NULL) {
            size_t len = strlen(pvatstr);
            movie_atom_t *pvatAtom = pvatAtomGen(pvatstr);
            fseek (myVideo->outFile, myVideo->mdatAtomOffset - (ARMEDIA_JSON_DESCRIPTION_MAXLENGTH+8), SEEK_SET);
            if (-1 == writeAtomToFile (&pvatAtom, myVideo->outFile))
            {
                ENCAPSULER_ERROR ("Error while writing pvatAtom\n");
                localError = ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            }
            // fill leaving space with a free atom until the mdatom
            uint32_t emptydatasize = ARMEDIA_JSON_DESCRIPTION_MAXLENGTH-(len+8);
            uint8_t *emptydata = calloc(emptydatasize, sizeof(uint8_t));
            if (emptydata == NULL) {
                ENCAPSULER_ERROR ("Error allocating freedata\n");
                localError = ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
            } else {
                movie_atom_t *fillatom = atomFromData(emptydatasize, "free", emptydata);
                if (-1 == writeAtomToFile (&fillatom, myVideo->outFile))
                {
                    ENCAPSULER_ERROR ("Error while writing fillatom\n");
                    localError = ARMEDIA_ERROR_ENCAPSULER_FILE_ERROR;
                }
                free(emptydata);
            }

            free(pvatstr);
        } else {
            ENCAPSULER_ERROR ("Error Json Pvat string empty\n");
        }
    }

    if (ARMEDIA_OK == localError)
    {
        if (NULL!=myVideo->outFile)  fclose (myVideo->outFile);
        if (NULL!=myVideo->infoFile) fclose (myVideo->infoFile);

        remove (myVideo->infoFilePath);
        rename (myVideo->tempFilePath, myVideo->outFilePath);


        if (myVideo->sps)
        {
            free (myVideo->sps);
            myVideo->sps = NULL;
        }
        if (myVideo->pps)
        {
            free (myVideo->pps);
            myVideo->pps = NULL;
        }

        free (myVideo);
        myVideo = NULL;
        free ((*encapsuler));
        *encapsuler = NULL;

        free (frameSizeBufferNE);
        frameSizeBufferNE = NULL;
        free (frameOffsetBuffer);
        frameOffsetBuffer = NULL;
        free (frameTimeSyncBuffer);
        frameTimeSyncBuffer = NULL;
        free (iFrameIndexBuffer);
        iFrameIndexBuffer = NULL;
    }
    else
    {
        free (frameSizeBufferNE);
        frameSizeBufferNE = NULL;
        free (frameOffsetBuffer);
        frameOffsetBuffer = NULL;
        free (frameTimeSyncBuffer);
        frameTimeSyncBuffer = NULL;
        free (iFrameIndexBuffer);
        iFrameIndexBuffer = NULL;
        ARMEDIA_VideoEncapsuler_Cleanup (encapsuler);
    }
    return localError;
}

eARMEDIA_ERROR ARMEDIA_VideoEncapsuler_Cleanup (ARMEDIA_VideoEncapsuler_t **encapsuler)
{
    ARMEDIA_Video_t *myVideo;

    if (NULL == (*encapsuler))
    {
        ENCAPSULER_ERROR ("encapsuler pointer must not be null");
        return ARMEDIA_ERROR_BAD_PARAMETER;
    }
    myVideo = (*encapsuler)->video; // ease of reading

    if(NULL!=myVideo->outFile)  fclose (myVideo->outFile);
    if(NULL!=myVideo->infoFile) fclose (myVideo->infoFile);
    remove (myVideo->infoFilePath);
    remove (myVideo->tempFilePath);
    remove (myVideo->outFilePath);

    if (myVideo->sps)
    {
        free (myVideo->sps);
        myVideo->sps = NULL;
    }
    if (myVideo->pps)
    {
        free (myVideo->pps);
        myVideo->pps = NULL;
    }

    free (myVideo);
    *encapsuler = NULL;

    return ARMEDIA_OK;
}

void ARMEDIA_VideoEncapsuler_SetGPSInfos (ARMEDIA_VideoEncapsuler_t* encapsuler, double latitude, double longitude, double altitude)
{
    if (encapsuler != NULL) {
        if (encapsuler->video != NULL) {
            encapsuler->video->videoGpsInfos.latitude = latitude;
            encapsuler->video->videoGpsInfos.longitude = longitude;
            encapsuler->video->videoGpsInfos.altitude = altitude;
        }
    }
}

int ARMEDIA_VideoEncapsuler_TryFixInfoFile (const char *infoFilePath)
{
    // Local values
    ARMEDIA_VideoEncapsuler_t *encapsuler = NULL;
    ARMEDIA_Video_t *video = NULL;
    FILE *infoFile = NULL;
    int noError = 1;
    int finishNoError = 1;
    uint32_t frameNumber = 0;
    uint32_t dataSize = 0;
    size_t tmpvidSize = 0;

    // Alloc local video pointer
    if (noError)
    {
        encapsuler = (ARMEDIA_VideoEncapsuler_t*)calloc (1, sizeof (ARMEDIA_VideoEncapsuler_t));
        video = (ARMEDIA_Video_t*)calloc (1, sizeof (ARMEDIA_Video_t));
        if (NULL == video)
        {
            noError = 0;
            ENCAPSULER_DEBUG ("Unable to alloc video pointer");
        } // No else
        encapsuler->video = video;
        encapsuler->fps = 30;
        encapsuler->timescale = 30 * 2000;
    } // No else

    // Open file for reading
    if (noError)
    {
        infoFile = fopen (infoFilePath, "r+b");
        if (NULL == infoFile)
        {
            noError = 0;
            ENCAPSULER_DEBUG ("Unable to open infoFile");
        } // No else
    } // No else

    // Read size from info descriptor
    if (noError)
    {
        uint32_t descriptorSize = 0;
        if (1 != fread (&descriptorSize, sizeof (uint32_t), 1, infoFile))
        {
            noError = 0;
            ENCAPSULER_DEBUG ("Unable to read descriptorSize");
        }
        else if (descriptorSize < sizeof (ARMEDIA_Video_t))
        {
            noError = 0;
            ENCAPSULER_DEBUG ("Descriptor size (%d) is smaller than ARMEDIA_Video_t size (%"PRIuPTR")", descriptorSize, sizeof (ARMEDIA_Video_t));
        } // No else
    } // No else

    // Read video
    if (noError)
    {
        if (1 != fread (video, sizeof (ARMEDIA_Video_t), 1, infoFile))
        {
            noError = 0;
            ENCAPSULER_DEBUG ("Unable to read ARMEDIA_Video_t from infoFile");
        }
        else
        {
            video->infoFile = infoFile;
            video->sps = NULL;
            video->pps = NULL;
            video->outFile = NULL;
            infoFile = NULL;
        }
    } // No else

    if (noError)
    {
        if (ARMEDIA_ENCAPSULER_VERSION_NUMBER != video->version)
        {
            noError = 0;
            ENCAPSULER_DEBUG ("Bad ARMEDIA_Video_t version number");
        } // No else, version number is OK
    }

    // Allocate / copy SPS/PPS pointers
    if (noError) {
        if(video->videoCodec == CODEC_MPEG4_AVC) {
            if (0 != video->spsSize && 0 != video->ppsSize)
            {
                video->sps = (uint8_t*) malloc (video->spsSize);
                video->pps = (uint8_t*) malloc (video->ppsSize);
                if (NULL == video->sps ||
                    NULL == video->pps)
                {
                    noError = 0;
                    ENCAPSULER_DEBUG ("Unable to allocate video sps/pps");
                    free (video->sps);
                    free (video->pps);
                    video->sps = NULL;
                    video->pps = NULL;
                }
                else
                {
                    if (1 != fread (video->sps, video->spsSize, 1, video->infoFile))
                    {
                        ENCAPSULER_DEBUG ("Unable to read video SPS");
                        noError = 0;
                    }
                    else if (1 != fread (video->pps, video->ppsSize, 1, video->infoFile))
                    {
                        ENCAPSULER_DEBUG ("Unable to read video PPS");
                        noError = 0;
                    } // No else
                }
            }
            else
            {
                ENCAPSULER_DEBUG ("Video SPS/PPS sizes are bad");
                noError = 0;
            }
        }
    }

    // Open temp file
    if (noError)
    {
        video->outFile = fopen (video->tempFilePath, "r+b");
        if (NULL == video->outFile)
        {
            noError = 0;
            ENCAPSULER_DEBUG ("Unable to open outFile : %s", video->tempFilePath);
        } // No else
    } // No else

    // Count frames
    if (noError)
    {
        int endOfSearch = 0;
        uint32_t frameSize = 0;
        uint32_t frameTstp = 0;
        char fType = 'a';
        size_t prevInfoIndex = 0;

        fseek (video->outFile, 0, SEEK_END);
        tmpvidSize = ftell (video->outFile);

        while (!endOfSearch)
        {
            prevInfoIndex = ftell (video->infoFile);
            if (! feof (video->infoFile) &&
                ARMEDIA_ENCAPSULER_NUM_MATCH_PATTERN ==
                fscanf (video->infoFile, ARMEDIA_ENCAPSULER_INFO_PATTERN, &frameSize, &fType, &frameTstp))
            {
                if ((dataSize + video->framesDataOffset + frameSize) > tmpvidSize)
                {
                    // We have too many infos : truncate at prevInfoIndex
                    fseek (video->infoFile, 0, SEEK_SET);
                    if (0 != ftruncate (fileno (video->infoFile), prevInfoIndex))
                    {
                        ENCAPSULER_DEBUG ("Unable to truncate infoFile");
                        noError = 0;
                    } // No else
                    endOfSearch = 1;
                }
                else
                {
                    dataSize += frameSize;
                    frameNumber ++;
                }
            }
            else
            {
                endOfSearch = 1;
            }
        }
    } // No else

    video->totalsize = dataSize;

    if (noError)
    {
        // If needed remove unused frames from .tmpvid file
        dataSize += video->framesDataOffset;
        if (tmpvidSize > dataSize)
        {
            if (0 != ftruncate (fileno (video->outFile), dataSize))
            {
                ENCAPSULER_DEBUG ("Unable to truncate outFile");
                noError = 0;
            } // No else
        } // No else

        fseek (video->outFile, 0, SEEK_END);
    }

    if (noError)
    {
        video->framesCount = frameNumber;
        rewind (video->infoFile);
        if (ARMEDIA_OK != ARMEDIA_VideoEncapsuler_Finish (&encapsuler))
        {
            ENCAPSULER_DEBUG ("Unable to finish video");
            /* We don't use the "noError" variable here as the
             * ARMEDIA_Videofinish function will do the cleanup
             * even if it fails */
            finishNoError = 0;
        }
    }

    if (!noError)
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
            ENCAPSULER_CLEANUP (fclose, video->infoFile);
            ENCAPSULER_CLEANUP (fclose, video->outFile);
            ENCAPSULER_CLEANUP (free ,video);
            video = NULL;
        }

        if (NULL != infoFile)
        {
            ENCAPSULER_DEBUG ("Closing local file");
            ENCAPSULER_CLEANUP (fclose, infoFile);
        }

        ENCAPSULER_DEBUG ("Cleaning up files");
        ENCAPSULER_DEBUG ("removing %s", infoFilePath);
        remove (infoFilePath);
        pathLen = strlen (infoFilePath);
        tmpVidFilePath = (char*) malloc (pathLen + 1);

        if (NULL != tmpVidFilePath)
        {
            strncpy (tmpVidFilePath, infoFilePath, pathLen);
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

    return noError && finishNoError;
}

int ARMEDIA_VideoEncapsuler_addPVATAtom (FILE *videoFile, eARDISCOVERY_PRODUCT product, const char *videoDate)
{
    int retVal = 0;
    struct json_object* pvato;
    pvato = json_object_new_object();
    if (pvato != NULL) {
        char prodid[5];
        snprintf(prodid, 5, "%04X", ARDISCOVERY_getProductID(product));
        json_object_object_add(pvato, "product_id", json_object_new_string(prodid));
        json_object_object_add(pvato, "run_date", json_object_new_string(videoDate));
        json_object_object_add(pvato, "media_date", json_object_new_string(videoDate));

        movie_atom_t *pvatAtom = pvatAtomGen(json_object_to_json_string(pvato));
        if (-1 == writeAtomToFile (&pvatAtom, videoFile))
        {
            ENCAPSULER_ERROR ("Error while writing pvatAtom\n");
        }
        else
        {
            json_object_put(pvato);
            retVal = 1;
        }
    }
    return retVal;
}


