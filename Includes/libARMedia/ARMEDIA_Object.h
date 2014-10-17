/**
 * @file   ARMediaObject.h
 * @author malick.sylla.ext@parrot.fr
 * @brief
 *
 **/
#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>
#import <AssetsLibrary/AssetsLibrary.h>
#import <ImageIO/CGImageProperties.h>
#import <CoreLocation/CoreLocation.h>

#import <libARDataTransfer/ARDataTransfer.h>
#import <libARMedia/ARMedia.h>

@class ARMediaObject;

@protocol ARMediaObjectDelegate <NSObject>

- (void)mediaObjectDidUpdateThumbnail:(ARMediaObject*)mediaObject;

@end

@interface ARMediaObject : NSObject <NSCoding, NSCopying>

@property (nonatomic, weak)   id<ARMediaObjectDelegate> delegate;
@property (nonatomic, assign) int index;
@property (nonatomic, strong) NSString *runDate;
@property (nonatomic, strong) NSString *productId;
@property (nonatomic, strong) NSString *name;
@property (nonatomic, strong) NSString *date;
@property (nonatomic, strong) NSString *filePath;
@property (nonatomic, strong) NSNumber *size;
@property (nonatomic, strong) UIImage *thumbnail;
@property (nonatomic, strong) NSNumber *mediaType;
@property (nonatomic, strong) NSURL *assetUrl;
@property (nonatomic, strong) NSString *uuid;

- (void)updateDataTransferMedia:(ARDATATRANSFER_Media_t *)media withIndex:(int)index;
- (void)updateThumbnailWithARDATATRANSFER_Media_t:(ARDATATRANSFER_Media_t *)media;
- (void)updateThumbnailWithNSUrl:(NSURL *)assetUrl;
@end
