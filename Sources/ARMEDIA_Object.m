/**
 * @file   ARMediaObject.m
 * @author malick.sylla.ext@parrot.fr
 * @brief
 *
 **/

#import <libARMedia/ARMEDIA_Object.h>

@implementation ARMediaObject
@synthesize mediaType = _mediaType;
@synthesize productId = _productId;
@synthesize runDate = _runDate;
@synthesize name = _name;
@synthesize date = _date;
@synthesize filePath = _filePath;
@synthesize size = _size;
@synthesize thumbnail = _thumbnail;

- (id)init
{
    self = [super init];
    if (self)
    {
        _name = nil;
        _filePath = nil;
        _date = nil;
        _size = NULL;
        _productId = nil;
        _thumbnail = nil;
        _runDate = nil;
        _mediaType = [NSNumber numberWithInt:MEDIA_TYPE_MAX];
    }
    return self;
}

- (void)updateDataTransferMedia:(ARDATATRANSFER_Media_t *)media
{
    if(media != NULL)
    {
        _name = [NSString stringWithUTF8String:media->name];
        _filePath = [NSString stringWithUTF8String:media->filePath];
        _date = [NSString stringWithUTF8String:media->date];
        _size = [NSNumber numberWithDouble:media->size];
        _productId = [NSString stringWithFormat:@"%04x",ARDISCOVERY_getProductID(media->product)];
        _thumbnail = [UIImage imageWithData:[NSData dataWithBytes:media->thumbnail length:media->thumbnailSize]];
    }
    
    if([[[NSString stringWithUTF8String:media->name] pathExtension] isEqual:[NSString stringWithUTF8String:ARMEDIA_JPG_EXTENSION]])
    {
        _mediaType = [NSNumber numberWithInt:MEDIA_TYPE_PHOTO];
    }
    else if([[[NSString stringWithUTF8String:media->name] pathExtension] isEqual:[NSString stringWithUTF8String:ARMEDIA_MP4_EXTENSION]])
    {
        _mediaType = [NSNumber numberWithInt:MEDIA_TYPE_VIDEO];
    }
    
}


- (void)updateThumbnailWithARDATATRANSFER_Media_t:(ARDATATRANSFER_Media_t *)media
{
    _thumbnail = [UIImage imageWithData:[NSData dataWithBytes:media->thumbnail length:media->thumbnailSize]];
}

- (void)updateThumbnailWithNSUrl:(NSURL *)assetUrl
{
    ALAssetsLibrary *assetslibrary = [[ALAssetsLibrary alloc] init];
    ALAssetsLibraryAssetForURLResultBlock resultblock = ^(ALAsset *myasset)
    {
        if ([[myasset valueForProperty:ALAssetPropertyType]isEqualToString:ALAssetTypeVideo])
        {
            _mediaType = [NSNumber numberWithInt:MEDIA_TYPE_VIDEO];
        }
        else if ([[myasset valueForProperty:ALAssetPropertyType]isEqualToString:ALAssetTypePhoto])
        {
            _mediaType = [NSNumber numberWithInt:MEDIA_TYPE_PHOTO];
        }
        // NO ELSE -
        
        ALAssetRepresentation *rep = [myasset defaultRepresentation];
        CGImageRef iref = [rep fullResolutionImage];
        if (iref != nil)
        {
            _thumbnail = [[UIImage alloc]initWithCGImage:iref];
        }
        // NO ELSE
        
    };
    
    ALAssetsLibraryAccessFailureBlock failureblock  = ^(NSError *myerror)
    {
        NSLog(@"cant get image - %@",[myerror localizedDescription]);
    };
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [assetslibrary assetForURL:assetUrl resultBlock:resultblock failureBlock:failureblock];
    });
}

- (id)initWithCoder: (NSCoder *)coder
{
    self = [super init];
    if (self)
    {
        _productId = [coder decodeObjectForKey:@"productId"];
        _name = [coder decodeObjectForKey:@"name"];
        _date = [coder decodeObjectForKey:@"date"];
        _filePath = [coder decodeObjectForKey:@"filePath"];
        _size = [coder decodeObjectForKey:@"size"];
        _thumbnail = [coder decodeObjectForKey:@"thumbnail"];
        _mediaType = [coder decodeObjectForKey:@"mediaType"];
    }
    return self;
}

- (void)encodeWithCoder: (NSCoder *)coder
{
    [coder encodeObject:_productId forKey:@"productId"];
    [coder encodeObject:_name forKey:@"name"];
    [coder encodeObject:_date forKey:@"date"];
    [coder encodeObject:_filePath forKey:@"filePath"];
    [coder encodeObject:_runDate forKey:@"runDate"];
    [coder encodeObject:_size forKey:@"size"];
    [coder encodeObject:_thumbnail forKey:@"thumbnail"];
    [coder encodeObject:_mediaType forKey:@"mediaType"];
}

- (id)copyWithZone:(NSZone *)zone
{
    ARMediaObject *copy = [[ARMediaObject alloc] init];
    if (copy)
    {
        [copy setProductId:[_productId copyWithZone:zone]];
        [copy setName:[_name copyWithZone:zone]];
        [copy setDate:[_date copyWithZone:zone]];
        [copy setFilePath:[_filePath copyWithZone:zone]];
        [copy setRunDate:[_runDate copyWithZone:zone]];
        [copy setSize:[_size copyWithZone:zone]];
        [copy setThumbnail:[[UIImage allocWithZone:zone] initWithCGImage:_thumbnail.CGImage]];
        [copy setMediaType:[_mediaType copyWithZone:zone]];
    }
    return copy;
}

@end