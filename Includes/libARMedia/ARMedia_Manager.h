/**
 * @file   ARMedia_Manager.h
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
}eARMEDIA_MANAGER_ERROR;

/**
 * @brief notification key post.
 */
UIKIT_EXTERN NSString *const kARMediaManagerNotificationInitialized;
UIKIT_EXTERN NSString *const kARMediaManagerNotificationUpdating;
UIKIT_EXTERN NSString *const kARMediaManagerNotificationUpdated;
UIKIT_EXTERN NSString *const kARMediaManagerNotificationMediaAdded;

@interface ARMedia_Manager : NSObject
/**
 init a ARMedia_Manager Singleton.
 @return ARMedia_Manager Singleton.
 */
+ (ARMedia_Manager *)sharedARMedia_Manager;
/**
 init a ARMedia_Manager with NSArray of NSString projectIDs.
 @param  NSArray for array of project.
 @return eARMEDIA_MANAGER_ERROR for enum of status.
 */
- (eARMEDIA_MANAGER_ERROR)ARMedia_Manager_InitWithProjectIDs:(NSArray *)projectIDs;
/**
 update media of ARMedia_Manager library.
 @return eARMEDIA_MANAGER_ERROR for enum of status.
 */
- (eARMEDIA_MANAGER_ERROR)ARMedia_Manager_Update;
/**
 add a media in ARMedia_Manager.
 @param NSString for mediaPath.
 @return BOOL for result of the add.
 */
- (BOOL)ARMedia_Manager_AddMedia:(NSString *)mediaPath;
/**
 retreive the refresh project media dictionary.
 @param NSString for project to get, if nil all project are get.
 @return NSDictionary who containt media projectDictionary.
 */
- (NSDictionary *)ARMedia_Manager_GetProjectDictionary:(NSString *)project;
@end