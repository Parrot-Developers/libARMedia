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

@interface ARMediaObject : NSObject <NSCoding, NSCopying>

@property (nonatomic, strong) NSString *runDate;
@property (nonatomic, strong) NSString *productId;
@property (nonatomic, strong) NSString *name;
@property (nonatomic, strong) NSString *date;
@property (nonatomic, strong) NSString *filePath;
@property (nonatomic, strong) NSNumber *size;
@property (nonatomic, strong) UIImage *thumbnail;
@property (nonatomic, strong) NSNumber *mediaType;

- (void)updateDataTransferMedia:(ARDATATRANSFER_Media_t *)media;
- (void)updateThumbnailWithARDATATRANSFER_Media_t:(ARDATATRANSFER_Media_t *)media;
- (void)updateThumbnailWithNSUrl:(NSURL *)assetUrl;
@end
