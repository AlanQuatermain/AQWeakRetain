/*
 * AQWeakRetain.h
 * AQWeakRetain
 * 
 * Created by Jim Dovey on 26/11/2010.
 * 
 * Copyright (c) 2010 Jim Dovey
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of the project's author nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
// Based on OFWeakRetain, from OmniFramework
// <https://github.com/omnigroup/OmniGroup/blob/master/Frameworks/OmniFoundation/OFWeakRetainConcreteImplementation.h>
//
// Copyright 2000-2005, 2010 Omni Development, Inc.  All rights reserved.
//
// This software may only be used and reproduced according to the
// terms in the file OmniSourceLicense.html, which should be
// distributed with this project and can also be found at
// <http://www.omnigroup.com/developer/sourcecode/sourcelicense/>.

#import <Foundation/Foundation.h>
#import <libkern/OSAtomic.h>

// #define these to 1 to enable some assertions and/or debug logs
#ifndef AQWEAKRETAIN_ASSERTIONS_ON
# define AQWEAKRETAIN_ASSERTIONS_ON 0
#endif

#ifndef AQWEAKRETAIN_DEBUG
# define AQWEAKRETAIN_DEBUG 0
#endif

///////////////////////////////////////////////////////////////////////////////////
// This is the bit you're interested in:

@protocol AQWeakRetain <NSObject>

// each conforming class must implement this itself
- (void) invalidateWeakRefcount;

// These are implemented by the AQWeakRetain_IMPLEMENTATION macro
- (void) aq_weakRetain;
- (void) aq_weakRelease;
- (id) aq_fullRetain NS_RETURNS_RETAINED;

@end

#pragma mark -
#pragma mark Weak Retain Guts

#import "AQWeakRetainGuts.h"

// a few helper macros

// this goes inside the ivar declaration block of each class adopting the protocol
#define DeclareAQWeakRetainIvars \
	AQWeakRetainIvars __AQWeakRetain_ivars;

// this goes in the init method of each class adopting the protocol
#define AQWeakRetainInit() \
	_AQWeakRetainIvarsInit(&__AQWeakRetain_ivars, self)

// this goes in the @implementation block of each class adopting the protocol
#define AQWeakRetainImplementation															\
- (void) aq_weakRetain																			\
{																							\
	_AQWeakRetainIncrement(&__AQWeakRetain_ivars, self);									\
}																							\
- (void) aq_weakRelease																		\
{																							\
	_AQWeakRetainDecrement(&__AQWeakRetain_ivars, self);									\
}																							\
- (void) aqweakretain_releaseHelper															\
{																							\
	[super release];																		\
}																							\
- (id) aq_fullRetain																			\
{																							\
	return ( _AQStrongRetainIncrement(&__AQWeakRetain_ivars, self) );						\
}																							\
- (void) release																			\
{																							\
	_AQWeakRetainRelease(&__AQWeakRetain_ivars, self);										\
}

// that's all you need -- everything below here is an implementation detail, included for inlining

#if AQWEAKRETAIN_ASSERTIONS_ON
# define AQWEAKRETAIN_ASSERT(condition) NSParameterAssert(condition)
#else
# define AQWEAKRETAIN_ASSERT(condition)
#endif

#if AQWEAKRETAIN_DEBUG
# define AQWEAKRETAIN_LOG(...)	NSLog(__VA_ARGS__);
#else
# define AQWEAKRETAIN_LOG(...)
#endif

@interface NSObject (AQWeakRetainHelper)
- (void) aqweakretain_releaseHelper;
@end

extern const unsigned int AQInvalidWeakRetainCount;

typedef struct _AQWeakRetainIvars
{
	OSSpinLock		lock;
#if AQWEAKRETAIN_ASSERTIONS_ON
	unsigned int	count:31;
	unsigned int	inited:1;
#else
	unsigned int	count;
#endif
	
} AQWeakRetainIvars;

static inline void _AQWeakRetainIvarsInit( AQWeakRetainIvars *ivars, NSObject<AQWeakRetain> * self )
{
	ivars->lock = OS_SPINLOCK_INIT;
	
	// count should already be zero at this point
	NSAssert1(ivars->count == 0, @"Weak retain ivars count should be zero at initialization, is %u", pIvars->count);
#if AQWEAKRETAIN_ASSERTIONS_ON
	ivars->inited = 1;
#endif
#if AQWEAKRETAIN_DEBUG
	NSLog( @"_AQWeakRetainIvarsInit(%@)", self );
#endif
}

static inline void _AQWeakRetainIncrement( AQWeakRetainIvars * ivars, NSObject<AQWeakRetain> * self )
{
	AQWEAKRETAIN_ASSERT(ivars->inited == 1);
	
	OSSpinLockLock( &ivars->lock );
	if ( ivars->count != AQInvalidWeakRetainCount )
		ivars->count++;

	AQWEAKRETAIN_LOG( @"-[%@ weakRetain]: count=%u", NSStringFromClass([self class]), ivars->count );
	
	OSSpinLockUnlock( &ivars->lock );
}

static inline void _AQWeakRetainDecrement( AQWeakRetainIvars * ivars, NSObject<AQWeakRetain> * self )
{
	AQWEAKRETAIN_ASSERT(ivars->inited == 1);
	
	OSSpinLockLock( &ivars->lock );
	if ( ivars->count != AQInvalidWeakRetainCount )
		ivars->count--;

	AQWEAKRETAIN_LOG( @"-[%@ weakRelease]: count=%d", NSStringFromClass([self class]), ivars->count );
	
	OSSpinLockUnlock( &ivars->lock );
}

static inline NSObject<AQWeakRetain> * _AQStrongRetainIncrement( AQWeakRetainIvars *, NSObject<AQWeakRetain> * );
static inline NSObject<AQWeakRetain> * _AQStrongRetainIncrement( AQWeakRetainIvars * ivars, NSObject<AQWeakRetain> * self )
{
	NSObject<AQWeakRetain> * result = nil;
	AQWEAKRETAIN_ASSERT(ivars->inited == 1);
	
	OSSpinLockLock( &ivars->lock );
	
	if ( ivars->count != AQInvalidWeakRetainCount )
		result = [self retain];
	
	OSSpinLockUnlock( &ivars->lock );
	
	return ( result );
}

extern void _AQWeakRetainRelease( AQWeakRetainIvars *ivars, NSObject<AQWeakRetain> * self );

// only call these on objects which conform to the AQWeakRetain protocol
@interface NSObject (AQWeakRetain)
- (id) weakRetain;
- (void) weakRelease;
- (id) weakAutorelease;
@end
