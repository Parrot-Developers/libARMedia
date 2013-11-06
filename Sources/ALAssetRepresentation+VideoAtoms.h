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
 get the next Atom in a video from an offset.
 @param movie_atom_t for the Atom
 @param long long for thr offset
 @return BOOL return YES if sucsess else NO.
 */
- (BOOL)getNextAtom:(movie_atom_t *)atom fromOffset:(long long *)offset;
/**
 get the ardtAtom from a video if it exist.
 @return NSString with the ardt.
 */
- (NSString *)ardtAtomExist;

/**
 get the pvatAtom from a video if it exist.
 @return NSDictionary with the pvat informations.
 */
- (NSDictionary *)pvatAtomExist;

@end
