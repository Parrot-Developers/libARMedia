/**
 * @file   ALAssetRepresentation+VideoAtoms.m
 * @author malick.sylla.ext@parrot.fr
 * @brief
 *
 */

#import "ALAssetRepresentation+VideoAtoms.h"


/*************************************/
/*ALAssetRepresentation (VideosAtoms)*/
/*************************************/

@implementation ALAssetRepresentation (VideosAtoms)
- (BOOL) getNextAtom:(movie_atom_t *)atom fromOffset:(long long *)offset
{
    BOOL result = NO;
    long long current_offset = *offset;
    if([self getBytes:(uint8_t *)&atom->size fromOffset:current_offset length:sizeof(uint32_t) error:nil] != 0)
    {
        atom->size = ntohl(atom->size);
        current_offset += sizeof(uint32_t);
        if([self getBytes:(uint8_t *)&atom->tag[0] fromOffset:current_offset length:sizeof(uint32_t) error:nil] != 0)
        {
            current_offset += sizeof(uint32_t);
            *offset = current_offset;
            result = YES;
        }
        // NO ELSE because if error, we continue the parsing
    }
    // NO ELSE because if error, we continue the parsing
    
    return result;
}

- (NSString *)ardtAtomExist
{
    BOOL result = NO;
    NSString *retval = nil;
    movie_atom_t atom = { 0 };
    long long atomOffset = 0;
    
    do
    {
        result = [self getNextAtom:&atom fromOffset:&atomOffset];
        atomOffset += (atom.size - 8);
    }
    while (result &&
           ('a' != atom.tag [0] ||
            'r' != atom.tag [1] ||
            'd' != atom.tag [2] ||
            't' != atom.tag [3] ) );
    
    // If result == YES, we found atom "ardt"
    if(result)
    {
        atomOffset -= (atom.size - 8);
        atom.data = (uint8_t *)malloc(sizeof(uint8_t) * ((atom.size - 8) + 1));
        memset(atom.data, 0, sizeof(uint8_t) * ((atom.size - 8) + 1)); // +1 for '\0'
        if((atom.size - 8) > 0)
        {
            [self getBytes:atom.data fromOffset:atomOffset length:(atom.size - 8) error:nil];
            retval = [NSString stringWithUTF8String:(const char *)atom.data + 4];
        }
        free(atom.data);
    }
    // NO ELSE - we didn't find atom 'ardt' atom, nothing to do.
    
    return retval;
}

- (NSDictionary *)pvatAtomExist
{
    BOOL result = NO;
    NSDictionary *retval = nil;
    movie_atom_t atom = { 0 };
    long long atomOffset = 0;
    
    do
    {
        result = [self getNextAtom:&atom fromOffset:&atomOffset];
        atomOffset += (atom.size - 8);
    }
    while (result &&
           ('p' != atom.tag [0] ||
            'v' != atom.tag [1] ||
            'a' != atom.tag [2] ||
            't' != atom.tag [3] ) );
    
    // If result == YES, we found atom "ardt"
    if(result)
    {
        atomOffset -= (atom.size - 8);
        atom.data = (uint8_t *)malloc(sizeof(uint8_t) * (atom.size - 8));
        memset(atom.data, 0, sizeof(uint8_t) * (atom.size - 8));
        if((atom.size - 8) > 0)
        {
            NSError *error = nil;

            [self getBytes:atom.data fromOffset:atomOffset length:atom.size error:nil];
            NSDictionary *jSONDataDic = [NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:atom.data length:atom.size - 8] options:NSJSONReadingMutableContainers error:&error];
            if(error == nil)
            {
                retval = jSONDataDic;
            }
        }
        
        free(atom.data);
    }
    // NO ELSE - we didn't find atom 'ardt' atom, nothing to do.
    
    return retval;
}
@end

