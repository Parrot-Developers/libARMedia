/**
 * @file   ARMedia_MediaDescription.h
 * @author malick.sylla.ext@parrot.fr
 * @brief
 *
*/

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>


@interface ARMedia_MediaDescription : NSObject <NSCoding, NSCopying>
@property (nonatomic, strong) NSString *mediaDate;
@property (nonatomic, strong) NSString *runDate;
@property (nonatomic, strong) NSString *device;
@end
