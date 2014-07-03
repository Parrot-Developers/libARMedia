/*
 * ARMedia.h
 *
 * Created by f.dhaeyer on 29/03/14
 * Copyright 2014 Parrot SA. All rights reserved.
 *
 */
#ifndef _ARMEDIA_H_
#define _ARMEDIA_H_
#include <libARSAL/ARSAL.h>
#include <libARMedia/ARMEDIA_Error.h>
#include <libARMedia/ARMEDIA_VideoAtoms.h>
#define ARMEDIA_MP4_EXTENSION   "mp4"
#define ARMEDIA_JPG_EXTENSION   "jpg"

#define ARMEDIA_JSON_DESCRIPTION_DATE_FMT "%FT%H%M%S%z"
#define ARMEDIA_JSON_DESCRIPTION_MAXLENGTH (512)

typedef enum
{
    MEDIA_TYPE_VIDEO = 0,
    MEDIA_TYPE_PHOTO,
    MEDIA_TYPE_MAX
} eMEDIA_TYPE;

#endif // _ARMEDIA_H_
