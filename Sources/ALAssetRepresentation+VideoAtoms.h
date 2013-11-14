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
 @param long long for the offset
 @return BOOL return YES if sucsess else NO.
 */
//- (BOOL)getNextAtom:(movie_atom_t *)atom fromOffset:(long long *)offset;

/**
 check from a video if atomName it exist.
 @param NSString for the name of the atom
 @return NSDictionary with the pvat informations.
 */
- (NSDictionary *)atomExist:(NSString *)atomName;

@end
