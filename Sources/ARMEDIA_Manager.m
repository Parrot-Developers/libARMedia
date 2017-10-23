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
/**
 * @file   ARMEDIA_Manager.m
 * @author malick.sylla.ext@parrot.fr
 * @brief
 *
 */

#import <UIKit/UIKit.h>
#import <libARMedia/ARMedia.h>
#import <libARMedia/ARMEDIA_Manager.h>
#import <libARMedia/ARMEDIA_Object.h>
#import <libARDataTransfer/ARDataTransfer.h>

#import "ALAssetRepresentation+VideoAtoms.h"

#define ARMEDIA_MANAGER_DATABASE_FILENAME                       @"ARMediaDB.ar"

// PVAT Keys
NSString *const kARMediaManagerPVATRunDateKey                   = @"run_date";
NSString *const kARMediaManagerPVATMediaDateKey                 = @"media_date";
NSString *const kARMediaManagerPVATProductIdKey                 = @"product_id";
NSString *const kARMediaManagerPVATUUID                         = @"uuid";
NSString *const kARMediaManagerPVATFileName                     = @"filename";

// Archive keys
NSString *const kARMediaManagerArchiverKey                      = @"kARMediaManagerArchiverKey";
NSString *const kARMediaManagerKey                              = @"kARMediaManagerKey";

// Notification keys
NSString *const kARMediaManagerNotificationInitialized          = @"kARMediaManagerNotificationInitialized";
NSString *const kARMediaManagerNotificationUpdating             = @"kARMediaManagerNotificationUpdating";
NSString *const kARMediaManagerNotificationUpdated              = @"kARMediaManagerNotificationUpdated";
NSString *const kARMediaManagerNotificationMediaAdded           = @"kARMediaManagerNotificationMediaAdded";
NSString *const kARMediaManagerNotificationEndOfMediaAdding     = @"kARMediaManagerNotificationEndOfMediaAdding";
NSString *const kARMediaManagerNotificationAccesDenied          = @"kARMediaManagerNotificationAccesDenied";

NSString *const kARMediaManagerDefaultSkyControllerDateKey      = @"2014-01-01";

// This block is always executed. If failure, an NSError is passed.
typedef void (^ARMediaManagerTranferingBlock)(NSString *assetURLString);

@interface ARMediaManager ()
@property (nonatomic, assign) BOOL cancelRefresh;
@property (nonatomic, assign) BOOL isUpdate;
@property (nonatomic, assign) BOOL isUpdating;
@property (nonatomic, assign) BOOL isAdding;
@property (nonatomic, assign) BOOL isInit;
@property (nonatomic, assign) NSUInteger mediaAssetsCount;
@property (nonatomic, strong) NSMutableDictionary *privateProjectsDictionary;
@property (nonatomic, strong) NSMutableDictionary *groupMediaDictionary;
@property (nonatomic, strong) NSDictionary *projectsDictionary;
@property (nonatomic, strong) NSMutableArray *mediaToAdd;

- (BOOL)saveMedia:(NSString *)mediaPath transferingBlock:(ARMediaManagerTranferingBlock)_transferingBlock;
- (void)addAssetToLibrary:(ALAsset *)asset albumName:(NSString *)albumName;
@end

@implementation ARMediaManager

/*************************************/
/*      ARMediaManager (public)     */
/*************************************/

+ (ARMediaManager *)sharedInstance
{
    static ARMediaManager *_sharedARMediaManager = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        _sharedARMediaManager = [[ARMediaManager alloc] init];
        
        /**
         *  custom init
         */
        _sharedARMediaManager.isInit = NO;
        _sharedARMediaManager.isUpdate = NO;
        _sharedARMediaManager.isUpdating = NO;
        _sharedARMediaManager.isAdding = NO;
        _sharedARMediaManager.groupMediaDictionary = [NSMutableDictionary dictionary];
        _sharedARMediaManager.mediaToAdd = [NSMutableArray array];
    });
    
    return _sharedARMediaManager;
}

- (eARMEDIA_MANAGER_ERROR)initWithProjectIDs:(NSArray *)projectIDs
{
    NSUInteger returnVal = ARMEDIA_MANAGER_ALREADY_INITIALIZED;
    if(!_isInit)
    {
        // Get productlist from the saved file with NSCoding
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
        NSString *documentsDirectory = [paths objectAtIndex:0];
        NSString *path = [documentsDirectory stringByAppendingPathComponent:ARMEDIA_MANAGER_DATABASE_FILENAME];
        NSData *data = [[NSData alloc] initWithContentsOfFile:path];
        NSUInteger valueKARMediaManagerKey = [[[NSUserDefaults standardUserDefaults] valueForKey:kARMediaManagerKey] integerValue];
        
        if(valueKARMediaManagerKey > 0)
        {
            // If data is not empty, unarchiving
            if (data != nil)
            {
                NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingWithData:data];
                _privateProjectsDictionary  = [unarchiver decodeObjectForKey:kARMediaManagerArchiverKey];
                [unarchiver finishDecoding];
            }
        }
        else
        {
            _privateProjectsDictionary = [NSMutableDictionary dictionary];
            [[NSUserDefaults standardUserDefaults] setValue:[NSString stringWithFormat:@"%d",0]  forKey:kARMediaManagerKey];
        }
        
        for (NSString *projectID in projectIDs)
        {
            if([_privateProjectsDictionary valueForKey:projectID] == nil)
            {
                [_privateProjectsDictionary setObject:[NSMutableDictionary dictionary] forKey:projectID];
            }
        }
        _isInit = YES;
        [[NSNotificationCenter defaultCenter] postNotificationName:kARMediaManagerNotificationInitialized object:nil];
        
        returnVal = ARMEDIA_MANAGER_OK;
    }
    return returnVal;
}

- (NSDictionary *)getProjectDictionary:(NSString *)project
{
    NSMutableDictionary *retDictionary = nil;
    if (project == nil)
    {
        NSMutableDictionary *mutableReturnedDictionary = [NSMutableDictionary dictionary];
        for (NSDictionary *dictionary in _projectsDictionary)
            [mutableReturnedDictionary setObject:[_projectsDictionary objectForKey:dictionary] forKey:dictionary];
        retDictionary = mutableReturnedDictionary;
    }
    else
    {
        retDictionary = [_projectsDictionary objectForKey:project];
    }
    
    return retDictionary;
}


- (eARMEDIA_MANAGER_ERROR)update
{
    __block eARMEDIA_MANAGER_ERROR retVal = ARMEDIA_MANAGER_OK;

    // Get All assets from camera roll
    NSLog(@"Get All assets from camera roll");
    void (^assetGroupEnumerator)(ALAssetsGroup *, BOOL *) =  ^(ALAssetsGroup *group, BOOL *stop)
    {
        if (group != nil && [group valueForProperty:ALAssetsGroupPropertyName])
        {
            [_groupMediaDictionary setValue:[group valueForProperty:ALAssetsGroupPropertyURL] forKey:[group valueForProperty:ALAssetsGroupPropertyName]];
            if([(NSNumber *)[group valueForProperty:ALAssetsGroupPropertyType] intValue] == ALAssetsGroupSavedPhotos)
            {
                // Get count of assets
                _mediaAssetsCount = [group numberOfAssets];

                NSLog(@"mediaAssetCount : %d",(int)[group numberOfAssets]);
                if(_mediaAssetsCount > 0)
                {
                    [self retrieveAssetsWithGroup:group];
                }

            }
        }
        if (group == nil) // end enumeration
        {
            _projectsDictionary = [_privateProjectsDictionary copy];
            [[NSNotificationCenter defaultCenter] postNotificationName:kARMediaManagerNotificationUpdated object:nil];
            _isUpdate = YES;
            _isUpdating = NO;
        }
    };

    void (^failureBlock)(NSError *) = ^(NSError *error)
    {
        NSLog(@"Failure : %@", error);
        if (error.code == ALAssetsLibraryAccessUserDeniedError)
        {
            retVal = ARMEDIA_MANAGER_NOT_INITIALIZED;
            [[NSNotificationCenter defaultCenter] postNotificationName:kARMediaManagerNotificationAccesDenied object:nil];
        }
        _projectsDictionary = [_privateProjectsDictionary copy];
        _isUpdate = NO;
    };


    if(!_isInit)
    {
        retVal = ARMEDIA_MANAGER_NOT_INITIALIZED;
    }
    else if (!_isUpdating)
    {
        _isUpdating = YES;
        ALAssetsLibrary *library = [[ALAssetsLibrary alloc] init];
        _isUpdate = NO;
        [library enumerateGroupsWithTypes:ALAssetsGroupAll
                               usingBlock:assetGroupEnumerator
                             failureBlock:failureBlock];
    }
    else
    {
        retVal = ARMEDIA_ERROR;
    }
    return retVal;
}

- (void)addMediaToQueue:(NSString *)mediaPath
{
    [_mediaToAdd addObject:mediaPath];

    if (_isUpdate)
        [self addMedia];
}

- (BOOL)isUpdated
{
    return _isUpdate;
}

- (BOOL)isUpdating
{
    return _isUpdating;
}

- (BOOL)isAdding
{
    return _isAdding;
}

/*************************************/
/*      ARMediaManager (private)    */
/*************************************/

-(void)saveMediaOnArchive
{
    // Save on file with NSCoding
    NSString *documentsDirectory = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
    NSString *archivePath = [documentsDirectory stringByAppendingPathComponent:ARMEDIA_MANAGER_DATABASE_FILENAME];
    NSMutableData *data = [NSMutableData data];
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initForWritingWithMutableData:data];
    [archiver encodeObject:_privateProjectsDictionary forKey:kARMediaManagerArchiverKey];
    [archiver finishEncoding];
    [data writeToFile:archivePath atomically:YES];
}

- (void)addMedia
{
    if ([_mediaToAdd count] == 0 || !_isUpdate)
        return;
    
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    
    void (^transferingBlock)(NSString *) = ^(NSString *assetURLString)
    {
        if(assetURLString != nil)
        {
            [self saveMediaOnArchive];
            _projectsDictionary = [_privateProjectsDictionary copy];
            [[NSNotificationCenter defaultCenter] postNotificationName:kARMediaManagerNotificationMediaAdded object:[_mediaToAdd objectAtIndex:0]];
        }
        [[NSFileManager defaultManager] removeItemAtPath:[_mediaToAdd objectAtIndex:0] error:nil];
        [_mediaToAdd removeObjectAtIndex:0];
        
        if ([_mediaToAdd count] == 0)
        {
            _isUpdate = YES;
            _isAdding = NO;
            [[NSNotificationCenter defaultCenter] postNotificationName:kARMediaManagerNotificationEndOfMediaAdding object:nil];
        }
        dispatch_semaphore_signal(sema);
    };
    
    if (_isUpdate)
    {
        _isUpdate = NO;
        while (([_mediaToAdd count] != 0))
        {
            _isAdding = YES;
            [self saveMedia:[_mediaToAdd objectAtIndex:0] transferingBlock:transferingBlock];
            dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
        }
    }
}

- (void)addAssetToLibrary:(ALAsset *)asset albumName:(NSString *)albumName
{
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    ALAssetsLibrary *library = [[ALAssetsLibrary alloc] init];
    if([_groupMediaDictionary valueForKey:albumName] == nil)
    {
        [library addAssetsGroupAlbumWithName:albumName resultBlock:^(ALAssetsGroup *group)
         {
             [_groupMediaDictionary setValue:[group valueForProperty:ALAssetsGroupPropertyURL] forKey:albumName];
             [group addAsset:asset];
             dispatch_semaphore_signal(sema);
             
         }
        failureBlock:^(NSError *error)
         {
             NSLog(@"Failure : %@", error);
             dispatch_semaphore_signal(sema);
         }];
    }
    else
    {
        [library groupForURL:[_groupMediaDictionary valueForKey:albumName] resultBlock:^(ALAssetsGroup *group)
         {
             if(group != nil)
             {
                 [group addAsset:asset];
             }
             dispatch_semaphore_signal(sema);
         }
                failureBlock:^(NSError *error)
         {
             NSLog(@"Failure : %@", error);
             dispatch_semaphore_signal(sema);
         }];
    }
    
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
}

- (void)retrieveAssetsWithGroup:(ALAssetsGroup *)group
{
    // RETRIEVING ALL ASSETS IN CAMERA ROLL
    __block NSMutableDictionary *tempProjectDictionaries = [NSMutableDictionary dictionary];
    __block unsigned int productId;
    for(NSMutableDictionary *tmpProject in _privateProjectsDictionary)
    {
        [tempProjectDictionaries setObject:[NSMutableDictionary dictionary] forKey:tmpProject];
    }
    
    void (^assetEnumerator)(ALAsset *, NSUInteger, BOOL *) = ^(ALAsset *asset, NSUInteger index, BOOL *stop)
    {
        if(asset != nil)
        {
            NSString *stringAsset = nil;
            ALAssetRepresentation *representation = [asset defaultRepresentation];
            ARMediaObject *mediaObject = [[ARMediaObject alloc]init];
            if ((NSUInteger)[[[NSUserDefaults standardUserDefaults] valueForKey:kARMediaManagerKey] intValue] > index)
            {
                stringAsset = [[representation url] absoluteString];
                for (NSString *projectID in _privateProjectsDictionary)
                {
                    NSMutableDictionary *projectDictionary = [_privateProjectsDictionary valueForKey:projectID];
                    mediaObject = [projectDictionary objectForKey:stringAsset];
                    if (mediaObject != nil)
                    {
                        NSScanner* scanner = [NSScanner scannerWithString:mediaObject.productId];
                        if([scanner scanHexInt:&productId])
                        {
                            [[tempProjectDictionaries objectForKey:projectID] setValue:[projectDictionary objectForKey:stringAsset] forKey:stringAsset];
                            [self addAssetToLibrary:asset albumName:[NSString stringWithUTF8String:ARDISCOVERY_getProductName(ARDISCOVERY_getProductFromProductID(productId))]];
                        }
                        else
                        {
                            NSLog(@"Failed to scan hexadecimal product ID");
                        }
                    }
                    // NO ELSE - We add only existing media from Camera roll
                }
            }
            else
            {
                if([asset valueForProperty:ALAssetPropertyType] == ALAssetTypeVideo)
                {
                    NSDictionary *atomValue = [representation atomExist:[NSString stringWithUTF8String:ARMEDIA_VIDEOATOMS_PVAT]];
                    if(atomValue != nil)
                    {
                        stringAsset = [[representation url] absoluteString];
                        
                        NSScanner* scanner = [NSScanner scannerWithString:[atomValue valueForKey:kARMediaManagerPVATProductIdKey]];
                        if([scanner scanHexInt:&productId])
                        {
                            mediaObject.productId = (NSString *)[atomValue valueForKey:kARMediaManagerPVATProductIdKey];
                            mediaObject.date = (NSString *)[atomValue valueForKey:kARMediaManagerPVATMediaDateKey];
                            mediaObject.runDate = (NSString *)[atomValue valueForKey:kARMediaManagerPVATRunDateKey];
                            mediaObject.uuid = (NSString *) [atomValue valueForKey:kARMediaManagerPVATUUID];
                            mediaObject.mediaType = [NSNumber numberWithInt:MEDIA_TYPE_VIDEO];
                            mediaObject.name = (NSString *) [atomValue valueForKey:kARMediaManagerPVATFileName];
                            [[tempProjectDictionaries valueForKey:[NSString stringWithUTF8String:ARDISCOVERY_getProductName(ARDISCOVERY_getProductFromProductID(productId))]] setValue:mediaObject forKey:stringAsset];
                            [self addAssetToLibrary:asset albumName:[NSString stringWithUTF8String:ARDISCOVERY_getProductName(ARDISCOVERY_getProductFromProductID(productId))]];
                        }
                        else
                        {
                            NSLog(@"Failed to scan hexadecimal product ID");
                        }
                    }
                    // NO ELSE - Ignoring this video - ardt value format not recognized
                }
                else if([asset valueForProperty:ALAssetPropertyType] == ALAssetTypePhoto)
                {
                    NSDictionary *metadata = [representation metadata];
                    if(metadata != nil)
                    {
                        NSDictionary *tiffDictionary = [metadata valueForKey:(NSString *)kCGImagePropertyTIFFDictionary];
                        
                        if(tiffDictionary != nil)
                        {
                            NSString *tiffDescription = [tiffDictionary valueForKey:(NSString *)kCGImagePropertyTIFFImageDescription];
                            stringAsset = [[representation url] absoluteString];
                            NSError *jSONerror = nil;
                            
                            NSData * data = [tiffDescription dataUsingEncoding:NSASCIIStringEncoding];
                            if (data != nil)
                            {
                                id jSONDataDic =[NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:&jSONerror];
                                NSScanner* scanner = [NSScanner scannerWithString:[jSONDataDic valueForKey:kARMediaManagerPVATProductIdKey]];
                                if([scanner scanHexInt:&productId])
                                {
                                    if((jSONDataDic != nil) && [[_privateProjectsDictionary allKeys] containsObject:[NSString stringWithUTF8String:ARDISCOVERY_getProductName(ARDISCOVERY_getProductFromProductID(productId))]] && (jSONerror == nil))
                                    {
                                        mediaObject.productId = (NSString *)[jSONDataDic valueForKey:kARMediaManagerPVATProductIdKey];
                                        mediaObject.date = (NSString *)[jSONDataDic valueForKey:kARMediaManagerPVATMediaDateKey];
                                        mediaObject.runDate = (NSString *)[jSONDataDic valueForKey:kARMediaManagerPVATRunDateKey];
                                        mediaObject.uuid = (NSString *) [jSONDataDic valueForKey:kARMediaManagerPVATUUID];
                                        mediaObject.mediaType = [NSNumber numberWithInt:MEDIA_TYPE_PHOTO];
                                        mediaObject.name = (NSString *) [jSONDataDic valueForKey:kARMediaManagerPVATFileName];
                                        [[tempProjectDictionaries valueForKey:[NSString stringWithUTF8String:ARDISCOVERY_getProductName(ARDISCOVERY_getProductFromProductID(productId))]] setValue:mediaObject forKey:stringAsset];
                                        [self addAssetToLibrary:asset albumName:[NSString stringWithUTF8String:ARDISCOVERY_getProductName(ARDISCOVERY_getProductFromProductID(productId))]];
                                    }
                                    else
                                    {
                                        NSLog(@"Failed to scan hexadecimal product ID");
                                    }
                                }
                            }
                        }
                    }
                }
                // NO ELSE, ALAssetPropertyType == ALAssetTypeUnknown => We don't process this type
            }
            
            [[NSNotificationCenter defaultCenter] postNotificationName:kARMediaManagerNotificationUpdating object:[NSString stringWithFormat:@"%0.f",(((double)index+1)/(double)_mediaAssetsCount)*100]];
        }
        else
        {
            [[NSUserDefaults standardUserDefaults] setValue:[NSString stringWithFormat:@"%lu",(unsigned long)index]  forKey:kARMediaManagerKey];
            [_privateProjectsDictionary setDictionary:tempProjectDictionaries];
            [self saveMediaOnArchive];
            _projectsDictionary = [_privateProjectsDictionary copy];
            [[NSNotificationCenter defaultCenter] postNotificationName:kARMediaManagerNotificationUpdated object:nil];
            _isUpdate = YES;
        }
        *stop = _cancelRefresh;
    };
    [group enumerateAssetsUsingBlock:assetEnumerator];
}

- (BOOL)saveMedia:(NSString *)mediaPath transferingBlock:(ARMediaManagerTranferingBlock)_transferingBlock
{
    __block BOOL added = NO;
    __block  NSString *stringAsset = nil;
    __block ALAssetsLibrary *library = [[ALAssetsLibrary alloc] init];
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block ARMediaObject *mediaObject = [[ARMediaObject alloc]init];
    __block unsigned int productId;
    if ([mediaPath.pathExtension isEqualToString:[NSString stringWithUTF8String:ARMEDIA_MP4_EXTENSION]] ||
        [mediaPath.pathExtension isEqualToString:[NSString stringWithUTF8String:ARMEDIA_MOV_EXTENSION]])
    {
        [library writeVideoAtPathToSavedPhotosAlbum:[NSURL URLWithString:mediaPath]  completionBlock:^(NSURL *assetURL, NSError *error)
         {
             if(error != nil || assetURL == nil)
             {
                 NSLog(@"Failure : %@", error);
                 _transferingBlock(nil);
                 added = NO;
             }
             else
             {
                 [library assetForURL:assetURL
                          resultBlock:^(ALAsset *asset) {

                              ALAssetRepresentation *representation = [asset defaultRepresentation];
                              NSDictionary *atomValue = [representation atomExist:[NSString stringWithUTF8String:ARMEDIA_VIDEOATOMS_PVAT]];

                              if(atomValue != nil)
                              {
                                  stringAsset = [[representation url] absoluteString];
                                  
                                  NSScanner* scanner = [NSScanner scannerWithString:[atomValue valueForKey:kARMediaManagerPVATProductIdKey]];
                                  if([scanner scanHexInt:&productId])
                                  {
                                      mediaObject.productId = (NSString *)[atomValue valueForKey:kARMediaManagerPVATProductIdKey];
                                      mediaObject.date = (NSString *)[atomValue valueForKey:kARMediaManagerPVATMediaDateKey];
                                      mediaObject.runDate = (NSString *)[atomValue valueForKey:kARMediaManagerPVATRunDateKey];
                                      mediaObject.uuid = (NSString *) [atomValue valueForKey:kARMediaManagerPVATUUID];
                                      mediaObject.mediaType = [NSNumber numberWithInt:MEDIA_TYPE_VIDEO];
                                      mediaObject.name = (NSString *) [atomValue valueForKey:kARMediaManagerPVATFileName];
                                      [[_privateProjectsDictionary valueForKey:[NSString stringWithUTF8String:ARDISCOVERY_getProductName(ARDISCOVERY_getProductFromProductID(productId))]] setValue:mediaObject forKey:stringAsset];
                                      
                                      [self addAssetToLibrary:asset albumName:[NSString stringWithUTF8String:ARDISCOVERY_getProductName(ARDISCOVERY_getProductFromProductID(productId))]];
                                  }
                                  else
                                  {
                                      NSLog(@"Failed to scan hexadecimal product ID");
                                  }
                              }
                              // NO ELSE - Ignoring this video - ardt value format not recognized
                              dispatch_semaphore_signal(sema);
                          }
                  
                         failureBlock:^(NSError* error) {
                             NSLog(@"failed to retrieve image asset:\nError: %@ ", [error localizedDescription]);
                             dispatch_semaphore_signal(sema);
                         }];
                 
                 dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
                 _transferingBlock([assetURL absoluteString]);
                 added = YES;
             }
         }];
        
    }
    else if ([mediaPath.pathExtension isEqualToString:[NSString stringWithUTF8String:ARMEDIA_JPG_EXTENSION]])
    {
        NSData *data = [NSData dataWithContentsOfFile:mediaPath];
        [library writeImageDataToSavedPhotosAlbum:data metadata:nil completionBlock:^(NSURL *assetURL, NSError *error)
         {
             if(error != nil || assetURL == nil)
             {
                 NSLog(@"Failure : %@", error);
                 _transferingBlock(nil);
                 added = NO;
             }
             else
             {
                 [library assetForURL:assetURL
                          resultBlock:^(ALAsset *asset) {
                              ALAssetRepresentation *representation = [asset defaultRepresentation];
                              
                              NSDictionary *metadata = [representation metadata];

                              if(metadata != nil)
                              {
                                  NSDictionary *tiffDictionary = [metadata valueForKey:(NSString *)kCGImagePropertyTIFFDictionary];
                                  if(tiffDictionary != nil)
                                  {
                                      NSString *tiffDescription = [tiffDictionary valueForKey:(NSString *)kCGImagePropertyTIFFImageDescription];
                                      stringAsset = [[representation url] absoluteString];
                                      
                                      NSError *jSONerror = nil;
                                      NSData *data = [tiffDescription dataUsingEncoding:NSUTF8StringEncoding];
                                      if (data != nil)
                                      {
                                          id jSONDataDic =[NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:&jSONerror];
                                          NSScanner* scanner = [NSScanner scannerWithString:[jSONDataDic valueForKey:kARMediaManagerPVATProductIdKey]];
                                          if([scanner scanHexInt:&productId])
                                          {
                                              if((jSONDataDic != nil) && [[_privateProjectsDictionary allKeys] containsObject:[NSString stringWithUTF8String:ARDISCOVERY_getProductName(ARDISCOVERY_getProductFromProductID(productId))]] && (jSONerror == nil))
                                              {
                                                  mediaObject.productId = (NSString *)[jSONDataDic valueForKey:kARMediaManagerPVATProductIdKey];
                                                  mediaObject.date = (NSString *)[jSONDataDic valueForKey:kARMediaManagerPVATMediaDateKey];
                                                  mediaObject.runDate = (NSString *)[jSONDataDic valueForKey:kARMediaManagerPVATRunDateKey];
                                                  mediaObject.uuid = (NSString *) [jSONDataDic valueForKey:kARMediaManagerPVATUUID];
                                                  mediaObject.mediaType = [NSNumber numberWithInt:MEDIA_TYPE_PHOTO];
                                                  mediaObject.name = (NSString *) [jSONDataDic valueForKey:kARMediaManagerPVATFileName];
                                                  
                                                  if (asset.editable && [[[mediaObject.date componentsSeparatedByString:@"T"] firstObject] isEqualToString:kARMediaManagerDefaultSkyControllerDateKey])
                                                  {
                                                      uint8_t *buffer = (uint8_t *)malloc(representation.size);
                                                      NSError *error = nil;
                                                      NSUInteger buffered = [representation getBytes:buffer fromOffset:0.0 length:representation.size error:&error];
                                                      if (error == nil)
                                                      {
                                                          NSData *data = [NSData dataWithBytesNoCopy:buffer length:buffered freeWhenDone:YES];
                                                          [asset setImageData:data metadata:[self setDateInTiffDictionary:metadata] completionBlock:nil];
                                                      }
                                                      else
                                                      {
                                                          free(buffer);
                                                      }
                                                  }
                                                  
                                                  [[_privateProjectsDictionary valueForKey:[NSString stringWithUTF8String:ARDISCOVERY_getProductName(ARDISCOVERY_getProductFromProductID(productId))]] setValue:mediaObject forKey:stringAsset];
                                                  [self addAssetToLibrary:asset albumName:[NSString stringWithUTF8String:ARDISCOVERY_getProductName(ARDISCOVERY_getProductFromProductID(productId))]];
                                                  
                                              }
                                              else
                                              {
                                                  NSLog(@"Failed to scan hexadecimal product ID");
                                              }
                                          }
                                      }
                                  }
                              }
                              dispatch_semaphore_signal(sema);
                          }
                  
                         failureBlock:^(NSError* error) {
                             NSLog(@"failed to retrieve image asset:\nError: %@ ", [error localizedDescription]);
                             dispatch_semaphore_signal(sema);
                         }];
                 
                 dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
                 _transferingBlock([assetURL absoluteString]);
                 added = YES;
             }
         }];
    }
    else //Bad media extension
    {
        _transferingBlock(nil);
        added = NO;
    }
    
    return added;
}


- (NSDictionary *)setDateInTiffDictionary:(NSDictionary *)metadata
{
    NSMutableDictionary *retVal = [metadata mutableCopy];

    NSString *tiffDescription = [[retVal objectForKey:(NSString *)kCGImagePropertyTIFFDictionary] objectForKey:(NSString *)kCGImagePropertyTIFFImageDescription];
    NSData *data = [tiffDescription dataUsingEncoding:NSUTF8StringEncoding];
    
    if (data != nil)
    {
        NSError *jSONerror = nil;
        NSString *updatedTiffDescription = nil;
        id jSONDataDic =[NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:&jSONerror];
        if (jSONerror == nil)
        {
            NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
            [dateFormatter setTimeZone:[NSTimeZone systemTimeZone]];
            [dateFormatter setTimeZone:[NSTimeZone timeZoneWithAbbreviation:@"UTC"]];
            [dateFormatter setDateFormat:@"yyyy-MM-dd'T'HHmmss+0000"];
            
            NSDate *currentDate = [[NSDate alloc] init];
            NSString *dateString = [dateFormatter stringFromDate:currentDate];
            
            [jSONDataDic setValue:dateString forKey:(NSString *)kARMediaManagerPVATMediaDateKey];
            [jSONDataDic setValue:dateString forKey:(NSString *)kARMediaManagerPVATRunDateKey];
            data = [NSJSONSerialization dataWithJSONObject:jSONDataDic options:0 error:&jSONerror];
        }
        if (jSONerror == nil)
        {
            updatedTiffDescription = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
            [[retVal objectForKey:(NSString *)kCGImagePropertyTIFFDictionary] setObject:updatedTiffDescription forKey:(NSString *)kCGImagePropertyTIFFImageDescription];
        }
    }
    return (NSDictionary *)retVal;
}
@end
