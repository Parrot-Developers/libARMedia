/**
 * @file   ARMEDIA_Manager.h
 * @author malick.sylla.ext@parrot.fr
 * @brief
 */
#import <Foundation/Foundation.h>
#import <AssetsLibrary/AssetsLibrary.h>
#import <ImageIO/CGImageProperties.h>
#import <CoreLocation/CoreLocation.h>

 /**
  * @brief status return by the callback.
 */
typedef enum 
{
    ARMEDIA_MANAGER_OK = 0,
    ARMEDIA_MANAGER_ALREADY_INITIALIZED,
    ARMEDIA_MANAGER_NOT_INITIALIZED
} eARMEDIA_MANAGER_ERROR;

/**
 * @brief notification key post.
 */
UIKIT_EXTERN NSString *const kARMediaManagerNotificationInitialized;
UIKIT_EXTERN NSString *const kARMediaManagerNotificationUpdating;
UIKIT_EXTERN NSString *const kARMediaManagerNotificationUpdated;
UIKIT_EXTERN NSString *const kARMediaManagerNotificationMediaAdded;
UIKIT_EXTERN NSString *const kARMediaManagerNotificationEndOfMediaAdding;
UIKIT_EXTERN NSString *const kARMediaManagerNotificationAccesDenied;

@interface ARMediaManager : NSObject
/**
 init a ARMedia_Manager Singleton.
 @return ARMedia_Manager Singleton.
 */
+ (ARMediaManager *)sharedInstance;
/**
 init a ARMedia_Manager with NSArray of NSString projectIDs.
 @param  NSArray for array of project.
 @return eARMEDIA_MANAGER_ERROR for enum of status.
 */
- (eARMEDIA_MANAGER_ERROR)initWithProjectIDs:(NSArray *)projectIDs;
/**
 update media of ARMedia_Manager library.
 @return eARMEDIA_MANAGER_ERROR for enum of status.
 */
- (eARMEDIA_MANAGER_ERROR)update;
/**
 add a media in a queue ARMedia_Manager.
 @param NSString for mediaPath.
 */
- (void)addMediaToQueue:(NSString *)mediaPath;
/**
 retreive the refresh project media dictionary.
 @param NSString for project to get, if nil all project are get.
 @return NSDictionary who containt media projectDictionary.
 */
- (NSDictionary *)getProjectDictionary:(NSString *)project;
/**
 @return Boolean if the ARMedia_Manager is update or not.
 */
- (BOOL)isUpdated;
/**
 @return Boolean if the ARMedia_Manager is updating or not.
 */
- (BOOL)isUpdating;

- (BOOL)isAdding;

@end