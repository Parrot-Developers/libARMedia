/**
 *  @file  ARMedia_Singleton.h
 *  @brief headers to define singletons.
 *  @date 07/10/2013
 *  @author frederic.dhaeyer@parrot.com
 */

#ifndef _ARMEDIA_SINGLETON_H_
#define _ARMEDIA_SINGLETON_H_

#define SYNTHESIZE_SINGLETON_FOR_CLASS(classname, initFunc) \
+ (classname *)shared##classname                            \
{                                                           \
    static classname *shared##classname = nil;              \
    static dispatch_once_t onceToken;                       \
    dispatch_once(&onceToken, ^{                            \
        shared##classname = [[classname alloc] init];       \
        [shared##classname initFunc];                       \
    });                                                     \
                                                            \
    return shared##classname;                               \
}

#define DECLARE_SINGLETON_FOR_CLASS(classname)              \
+ (classname *)shared##classname;

#define SINGLETON_FOR_CLASS(classname)                      \
[classname shared##classname]

#endif /** _ARMEDIA_SINGLETON_H_ */
