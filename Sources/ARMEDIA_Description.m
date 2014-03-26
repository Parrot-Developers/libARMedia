/**
 * @file   ARMEDIA_Description.m
 * @author malick.sylla.ext@parrot.fr
 * @brief
 *
 */

#import "ARMEDIA_Description.h"

@implementation ARMediaDescription
@synthesize mediaDate = _mediaDate;
@synthesize runDate = _runDate;
@synthesize device = _device;

- (id)init
{
    self = [super init];
    if (self)
    {
        _mediaDate = nil;
        _runDate = nil;
        _device = nil;
    }
    return self;
}

- (id)initWithCoder: (NSCoder *)coder
{
    self = [super init];
    if (self)
    {
        _mediaDate = [coder decodeObjectForKey:@"mediaDate"];
        _runDate = [coder decodeObjectForKey:@"runDate"];
        _device = [coder decodeObjectForKey:@"device"];
    }
    return self;
}

- (void)encodeWithCoder: (NSCoder *)coder
{
    [coder encodeObject:_mediaDate forKey:@"mediaDate"];
    [coder encodeObject:_runDate forKey:@"runDate"];
    [coder encodeObject:_device forKey:@"device"];
}

- (id)copyWithZone:(NSZone *)zone
{
    ARMediaDescription *copy = [[ARMediaDescription alloc] init];

    if (copy)
    {
        [copy setMediaDate:[_mediaDate copyWithZone:zone]];
        [copy setRunDate:[_runDate copyWithZone:zone]];
        [copy setDevice:[_device copyWithZone:zone]];
    }
    
    return copy;
}
         
@end