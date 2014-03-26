/**
 * @file   ALAssetRepresentation+VideoAtoms.h
 * @author malick.sylla.ext@parrot.fr
 * @brief
 *
 */

#import <Foundation/Foundation.h>
#import <AssetsLibrary/AssetsLibrary.h>
#import "ARMedia_Video_Atoms.h"

@interface ALAssetRepresentation (VideosAtoms)

/**
 check from a video if atomName it exist.
 @param NSString for the name of the atom
 @return NSDictionary with the pvat informations.
 */
- (NSDictionary *)atomExist:(NSString *)atomName;

@end
