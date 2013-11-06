/**
 * @file   ARMedia_MediaDescription.m
 * @author malick.sylla.ext@parrot.fr
 * @brief
 *
 */

#import "ARMedia_MediaDescription.h"

@implementation ARMedia_MediaDescription
@synthesize mediaDate = _mediaDate;
@synthesize runDate = _runDate;
@synthesize device = _device;

- (id)init
{
    self = [super init];
    if (self)
    {
    }
    return self;
}

- (id)initWithCoder: (NSCoder *)coder
{
    if (self = [super init])
    {
        [self setMediaDate: [coder decodeObjectForKey:@"mediaDate"]];
        [self setRunDate: [coder decodeObjectForKey:@"runDate"]];
        [self setDevice: [coder decodeObjectForKey:@"device"]];
        
    }
    return self;
}

- (void)encodeWithCoder: (NSCoder *)coder
{
    [coder encodeObject:self.mediaDate forKey:@"mediaDate"];
    [coder encodeObject:self.runDate forKey:@"runDate"];
    [coder encodeObject:self.device forKey:@"device"];
}

- (id)copyWithZone:(NSZone *)zone
{
    id copy = [[ARMedia_MediaDescription alloc] init];

    if (copy)
    {
        [copy setMediaDate:[self.mediaDate copyWithZone:zone]];
        [copy setRunDate:[self.runDate copyWithZone:zone]];
        [copy setDevice:[self.device copyWithZone:zone]];
    }
    return copy;
}
         
@end