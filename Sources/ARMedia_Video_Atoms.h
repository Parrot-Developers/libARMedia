/*
 * ardrone_video_atoms.h
 * ARDroneLib
 *
 * Created by n.brulez on 19/08/11
 * Copyright 2011 Parrot SA. All rights reserved.
 *
 */
#ifndef _ARDRONE_VIDEO_ATOMS_H_
#define _ARDRONE_VIDEO_ATOMS_H_

#include <inttypes.h>
#include <stdio.h>
#include <time.h>

typedef struct _movie_atom_t {
  uint32_t size;
  char tag [4];
  uint8_t *data;
  uint8_t wide;
} movie_atom_t;

#define PRRT_DATA_VERSION 3 // No parenthesis !

typedef struct _prrt_data_v1_t {
    uint32_t frameControlState;
    uint32_t frameTimestamp;
    float frameTheta;
    float framePhi;
    float framePsi;
} prrt_data_v1_t;

typedef struct _prrt_data_v2_t {
    uint32_t frameControlState;
    uint32_t frameTimestamp;
    float frameTheta;
    float framePhi;
    float framePsi;
    float framePsiDiscontinuity;
} prrt_data_v2_t;

typedef struct _prrt_data_v3_t {
    uint32_t frameControlState;
    uint32_t frameTimestamp;
    float frameTheta;
    float framePhi;
    float framePsi;
    float framePsiDiscontinuity;
    int32_t frameAltitude;
} prrt_data_v3_t;

// Auto select the correct prrt_data_t type for the current version
#define DECLARE_PRRT_DATA_T(VER) _DECLARE_PRRT_DATA_T(VER)
#define _DECLARE_PRRT_DATA_T(VER)               \
    typedef prrt_data_v##VER##_t prrt_data_t
DECLARE_PRRT_DATA_T (PRRT_DATA_VERSION);


/* Atoms :
LEGEND :
-> specific = specific function to generate this atom
-> empty = use atomFromData (0, "name", NULL);
-> from data = use atomFromData (dataSize, "name", dataPointer);

ftyp -> specific
mdat -> specific (include freeAtom if needed)
moov -> empty
 |- mvhd -> specific
 |- trak -> empty
 |   |- tkhd -> specific
 |   \- mdia -> empty
 |       |- mdhd -> specific
 |       |- hdlr -> specific (for mdia)
 |       \- minf -> empty
 |           |- vmhd -> specific
 |           |- hdlr -> specific (for minf)
 |           |- dinf -> empty
 |           |   \- dref -> specific
 |           \- stbl -> empty
 |               |- stsd -> specific
 |               |- stts -> specific
 |               |- stss -> from data (i frame positions as uint32_t network endian)
 |               |- stsc -> specific
 |               |- stsz -> from data (frames sizes as uint32_t network endian)
 |               \- stco -> from data (frames offset as uint32_t network endian)
 \- udta -> empty
     |- meta1 -> all meta specific (metadataAtomFromTagAndValue)
   [...]
     \- metaN -> all meta specific (metadataAtomFromTagAndValue)
ardt -> specific
prrt -> specific
*/

/* REMINDER : NEVER INCLUDE A MDAT ATOM INTO ANY OTHER ATOM */


/**
 * @brief Read PRRT data from a video file into a self alloced array
 * Thid function get the PRRT data from a video file and convert it to the latest version
 * All fields present in video prrt info and not in prrt_data_t will be discarded
 * All fields present in prrt_data_t but not in video prrt info will be filled with zeroes
 * This function alloc (using vp_os_calloc) the return pointer. Application MUST handle the free of the pointer.
 * @param videoFile Pointer to the video file.
 * @param videoNbFrames [out] Pointer to an int that will contain the number of frames in the video
 * @return A new, malloc'd, array of prrt_data_t filled with as many informations as possible about the video. In case of failure, returns NULL
 * @note The video FILE* pointer position will be modified by this call
 */
prrt_data_t *createPrrtDataFromFile (FILE *videoFile, int *videoNbFrames);

/**
 * @brief Read PRRT data from prrt atom into a self alloced array
 * This function get the PRRT data from a video and convert it to the latest version
 * All fields present in video prrt info and not in prrt_data_t will be discarded
 * All fields present in prrt_data_t but not in video prrt info will be filled with zeroes
 * This function alloc (using vp_os_calloc) the return pointer. Application MUST handle the free of the pointer.
 * @param prrtAtomBuffer Pointer to the prrt atom data (not including the leading size and atom tag)
 * @param prrtAtomSize Size of the prrtAtomBuffer. This is used to avoid overflow, or if the prrtAtom size was set to zero
 * @param videoNbFrames [out] Pointer to an int that will contain the number of frames in the video
 * @return A new, malloc'd, array of prrt_data_t filled with as many informations as possible about the video. In case of failure, returns NULL
 */
prrt_data_t *createPrrtDataFromAtom (uint8_t *prrtAtomBuffer, const int prrtAtomSize, int *videoNbFrames);

/**
 * @brief Read FPS from a given video file
 * This function get the FPS of a given video file
 * @param videoFile Pointer to the video file
 * @return The number of frames per second of the video. Returns zero if we were unable to read the actual value.
 * @note The video FILE* pointer position will be modified by this call
 * @note This call may fail on non-AR.Drone generated videos
 */
uint32_t getVideoFpsFromFile (FILE *videoFile);

/**
 * @brief Read FPS from a given video mdhd atom
 * This function get the FPS of a given video mdhd atom
 * @param mdhdAtom Pointer to the video atom
 * @param atomSize Size of the video atom pointer
 * @return The number of frames per second of the video. Returns zero if we were unable to read the actual value.
 * @note This call may fail on non-AR.Drone generated videos
 */
uint32_t getVideoFpsFromAtom (uint8_t *mdhdAtom, const int atomSize);

#endif // _ARDRONE_VIDEO_ATOMS_H_
