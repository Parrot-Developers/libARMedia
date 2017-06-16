/*
    Copyright (C) 2014 Parrot SA

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the 
      distribution.
    * Neither the name of Parrot nor the names
      of its contributors may be used to endorse or promote products
      derived from this software without specific prior written
      permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
    INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
    BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
    OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
    AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
    OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
*/
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
    
    movie_atom_t atom = {
        .size = 0,
        .tag = "",
        .data = NULL,
        .wide = 0,
    };
   
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
            result = seekMediaBufferToAtom(buff, &offset, [self size], [atomName UTF8String]);
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
