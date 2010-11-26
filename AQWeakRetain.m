/*
 * AQWeakRetain.m
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

#import "AQWeakRetain.h"

const unsigned int AQInvalidWeakRetainCount = 0x7fffffff;

void _AQWeakRetainRelease( AQWeakRetainIvars * ivars, NSObject<AQWeakRetain> * self )
{
	BOOL shouldInvalidate = NO;
	OSSpinLock *lock = &ivars->lock;
	
	AQWEAKRETAIN_ASSERT(ivars->inited == 1);
	OSSpinLockLock(lock);
	
	@try
	{
		NSUInteger retainCount = [self retainCount];
		BOOL hasWeakRetains = (ivars->count != AQInvalidWeakRetainCount) && (ivars->count != 0);
		shouldInvalidate = hasWeakRetains && (retainCount - 1 == ivars->count);
		
		AQWEAKRETAIN_LOG(@"-[%@ release] (retainCount=%d, count=%d, shouldInvalidate=%@)", NSStringFromClass([self class]), retainCount, ivar->count, shouldInvalidate ? @"YES" : @"NO");
		
		if ( shouldInvalidate )
		{
			// defer the release until after we've invalidate the weak retains, and make sure nobody else calls -invalidateWeakRefcount
			ivars->count = AQInvalidWeakRetainCount;
		}
		else
		{
			if ( retainCount == 1 )
			{
				// final release to deallocate the object, taking out lock away-- unlock it first
				OSSpinLockUnlock(lock);
				lock = NULL;
				[self aqweakretain_releaseHelper];
			}
			else
			{
				// release within the lock so that another thread won't drop the last ref while thinking it's not the last
				[self aqweakretain_retainHelper];
			}
		}
	}
	@catch (NSException * e)
	{
		shouldInvalidate = NO;
		@throw;
	}
	@finally
	{
		if ( lock != NULL )
			OSSpinLockUnlock(lock);
	}
	
	if ( shouldInvalidate )
	{
		[self invalidateWeakRefcount];
		[self aqweakretain_releaseHelper];
	}
}

#pragma mark -

@implementation NSObject (AQWeakRetain)

- (id) weakRetain
{
	[self retain];
	[(id<AQWeakRetain>)self aq_weakRetain];
	return ( self );
}

- (void) weakRelease
{
	[(id<AQWeakRetain>)self aq_weakRelease];
	[self release];
}

- (id) weakAutorelease
{
	[(id<AQWeakRetain>)self aq_weakRelease];
	[self autorelease];
}

static NSMutableSet * volatile warnedClasses = nil;

- (void) aq_weakRetain
{
	if ( warnedClasses == nil )
	{
		NSMutableSet * set = [NSMutableSet new];
		if ( OSAtomicCompareAndSwapPtrBarrier(nil, set, (void * volatile *)&warnedClasses) == false )
			[set release];
	}
	
	if ( ![warnedClasses containsObject: [self class]] )
	{
		[warnedClasses addObject: [self class]];
		NSLog( @"%@ does not implement the AQWeakRetain protocol properly", NSStringFromClass([self class]) );
	}
}

- (void) aq_weakRelease
{
}

+ (void) aq_weakRetain
{
}

+ (void) aq_weakRelease
{
}

@end
