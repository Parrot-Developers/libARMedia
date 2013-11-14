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

- (NSDictionary *)atomExist:(NSString *)atomName
{
    int result = 0;
    NSDictionary *retVal = nil;
    
    movie_atom_t atom = { 0 };
   
    NSError *error = nil;
    long long offset = 0;
    
    uint32_t size = (2 * sizeof(uint32_t)) + sizeof(uint64_t);
    uint8_t *buff = (uint8_t *)malloc(size);
    NSUInteger bytesRead;
    
    if(buff != NULL)
    {
        do
        {
            bytesRead = [self getBytes:buff fromOffset:offset length:size error:&error];
            result = seekMediaBufferToAtom(buff, &offset, [atomName UTF8String]);
        }
        while ((error == nil) && !result && bytesRead !=0);
        
        // If result = 1, we found the atomName
        if(result)
        {
            
            [self getBytes:(uint8_t *)&atom.size fromOffset:offset length:sizeof(uint32_t) error:&error];
            if(error != nil)
                result = 0;
        }
        
        if(result)
        {
            atom.size = htonl(atom.size);
            offset += (2 * sizeof(uint32_t));
            
            if(atom.size == 1)
            {
                [self getBytes:(uint8_t *)&atom.size fromOffset:offset length:sizeof(uint64_t) error:&error];
                if(error == nil)
                {
                    atom.size = atom_ntohll(atom.size);
                }
                else
                {
                    result = 0;
                }
            }
            // NO ELSE - Don't need to get 64 bits wide
        }

        if(result)
        {
            atom.size -= (2 * sizeof(uint32_t));
            if(atom.size > 0)
            {
                NSError *error = nil;

                atom.data = (uint8_t *)malloc(atom.size * sizeof(uint8_t));
                memset(atom.data, 0, sizeof(uint8_t) * atom.size);

                [self getBytes:atom.data fromOffset:offset length:atom.size error:&error];
                if(error != nil)
                    result = 0;
            }
            // TODO case
        }
        
        if(result)
        {
            NSDictionary *jSONDataDic = [NSJSONSerialization JSONObjectWithData:[NSData dataWithBytes:atom.data length:atom.size] options:NSJSONReadingMutableContainers error:&error];
            free(atom.data);
            if (error == nil)
            {
                retVal = jSONDataDic;
            }
        }
        free(buff);
    }
    return retVal;
}
@end
