//
//  ECAstronomyCache.h
//  Emerald Chronometer
//
//  Copied from Emerald Observatory 22 May 2011
//
//  Created by Steve Pucci on April 30 2009
//  Copyright Emerald Sequoia 2009. All rights reserved.
//

#include "ESPlatform.h"
//#include "ESAstronomy.hpp"
#include "ESAstronomyCache.hpp"
#include "ESThread.hpp"
#include "ESErrorReporter.hpp"

#ifdef STANDALONE
#include <stdio.h>
#include <assert.h>
#endif

#include <math.h>

static ECAstroCachePool astroCachePools[2];  // By entry thread# into ECAstronomy (if location, time zone, or calculation dates are different; see reserveCachePool)

static void bumpValidFlagsForLocationIndependentSlotsWithFlagValue(ECAstroCache *cache,
                                                                   int          oldGlobalFlag) {
    unsigned int *p = cache->cacheSlotValidFlag;
    unsigned int *end = p + firstLocationDependentSlotIndex;
    while (p < end) {
        if (*p == oldGlobalFlag) {
            (*p)++;
        }
        p++;
    }
}

// Once we've reserved a cache pool, set up all of the caches in that cache pool with the right parameters, bumping up the global flag
// if we don't match the cache in use
void setupGlobalCacheFlag(ECAstroCachePool *cachePool,
			  double 	   observerLatitude,
			  double 	   observerLongitude,
			  bool   	   runningBackward,
			  int              tzOffsetSeconds) {
    if (runningBackward != cachePool->runningBackward) {
        // If the time parameters have changed then we gotta redo the cache no matter what
	cachePool->runningBackward = runningBackward;
	cachePool->currentGlobalCacheFlag++;
    } else if (tzOffsetSeconds != cachePool->tzOffsetSeconds ||
               observerLatitude != cachePool->observerLatitude ||
               observerLongitude != cachePool->observerLongitude) {
        // But if only the location parameters have changed, we only need to invalidate the location-dependent slots.
        // We do this by bumping the global flag, then comparing each location-independent slot with the global flag,
        // and if the slot was valid before, we bump it now since all that's changed is the location.
	cachePool->observerLatitude = observerLatitude;
	cachePool->observerLongitude = observerLongitude;
	cachePool->tzOffsetSeconds = tzOffsetSeconds;
        unsigned int oldGlobalFlag = cachePool->currentGlobalCacheFlag++;
        bumpValidFlagsForLocationIndependentSlotsWithFlagValue(&cachePool->finalCache, oldGlobalFlag);
//        bumpValidFlagsForLocationIndependentSlotsWithFlagValue(&cachePool->tempCache, oldGlobalFlag);        // temp cache never matches global time anyway
//        bumpValidFlagsForLocationIndependentSlotsWithFlagValue(&cachePool->refinementCache, oldGlobalFlag);  // refinement cache never matches global time anyway
        bumpValidFlagsForLocationIndependentSlotsWithFlagValue(&cachePool->midnightCache, oldGlobalFlag);
        bumpValidFlagsForLocationIndependentSlotsWithFlagValue(&cachePool->year2000Cache, oldGlobalFlag);
    }
}

void reinitializeECAstroCache(ECAstroCache *valueCache) {
    valueCache->currentFlag = 1;
    unsigned int *p = valueCache->cacheSlotValidFlag;
    unsigned int *end = p + numCacheSlots;
    while (p < end) {
	*p++ = 0;
    }
}

// Set the given value cache active, and return the previously active cache
// so it can be popped to later.  If dateInterval isn't sufficiently
// close to cached value, invalidate the cache.
ECAstroCache *pushECAstroCacheWithSlopInPool(ECAstroCachePool *cachePool,
					     ECAstroCache     *valueCache,
					     ESTimeInterval   dateInterval,
					     ESTimeInterval   slop) {
    ESAssert(!isnan(dateInterval));
    ECAstroCache *oldCache = cachePool->currentCache;
    cachePool->currentCache = valueCache;
    if (valueCache) {
	valueCache->astroSlop = slop;
    } else {
	return oldCache;
    }
    if (valueCache->currentFlag == 0) {
	// The only time this state occurs is before ever calling this function, so initialize here
	valueCache->currentFlag = 1;
    }
    if (valueCache->globalValidFlag != cachePool->currentGlobalCacheFlag) {
	valueCache->globalValidFlag = cachePool->currentGlobalCacheFlag;
	goto invalid;
    } else if (isnan(dateInterval)) {
	if (!isnan(valueCache->dateInterval)) {
	    goto invalid;
	}
    } else if (isnan(valueCache->dateInterval)) {
	goto invalid;
    } else if (fabs(dateInterval - valueCache->dateInterval) > slop) {
	goto invalid;
    }
    return oldCache;
 invalid:
    if (valueCache->currentFlag == 0xffffffff) {  // this won't happen very often :-)
	reinitializeECAstroCache(valueCache);
    } else {
	valueCache->currentFlag++;
    }
    valueCache->dateInterval = dateInterval;
    return oldCache;
}

ECAstroCache *pushECAstroCacheInPool(ECAstroCachePool *cachePool,
				     ECAstroCache     *valueCache,
				     ESTimeInterval   dateInterval) {
    return pushECAstroCacheWithSlopInPool(cachePool, valueCache, dateInterval, ASTRO_SLOP_RAW);
}

// The given cache is presumed to still represent the correct date interval.
void popECAstroCacheToInPool(ECAstroCachePool *cachePool,
			     ECAstroCache     *valueCache) {
    cachePool->currentCache = valueCache;
}

void printCache(ECAstroCache     *valueCache,
		ECAstroCachePool *cachePool) {
    printf("\nCache at 0x%08lx: currentFlag %u, globalFlag %u (with global %u)\n",
	   (unsigned long)valueCache, valueCache->currentFlag, valueCache->globalValidFlag, cachePool->currentGlobalCacheFlag);
    for (int i = 0; i < numCacheSlots; i++) {
	printf("..%3d: %s %g\n", i, (valueCache->cacheSlotValidFlag[i] ? "OK" : "XX"), valueCache->cacheSlots[i]);
    }
}

void initializeAstroCache() {
    astroCachePools[0].currentGlobalCacheFlag = 1;
    astroCachePools[1].currentGlobalCacheFlag = 1;
}

void assertCacheValidForTDTCenturies(ECAstroCache *cache,
				     double       t) {
    // If cache is active, MUST store tdt in (or pull tdt from) cache before calling this routine:
    ESAssert(!cache || (cache->cacheSlotValidFlag[tdtCenturiesSlotIndex] == cache->currentFlag && fabs(cache->cacheSlots[tdtCenturiesSlotIndex] - t) < 0.0000000000001));
}

void assertCacheValidForTDTHundredCenturies(ECAstroCache *cache,
					    double       hundredCenturiesSinceEpochTDT) {
    // If cache is active, MUST store tdt in (or pull tdt from) cache before calling this routine:
    ESAssert(!cache || (cache->cacheSlotValidFlag[tdtHundredCenturiesSlotIndex] == cache->currentFlag && fabs(cache->cacheSlots[tdtHundredCenturiesSlotIndex] - hundredCenturiesSinceEpochTDT) < 0.00000000001));
}

ECAstroCachePool *getCachePoolForThisThread() {
    int cacheIndex = ESThread::inMainThread() ? 0 : 1;
    return &astroCachePools[cacheIndex];
}

void initializeCachePool(ECAstroCachePool *pool,
			 ESTimeInterval   dateInterval,
			 double           observerLatitude,
			 double           observerLongitude,
			 bool             runningBackward,
			 int              tzOffsetSeconds) {
    setupGlobalCacheFlag(pool, observerLatitude, observerLongitude, runningBackward, tzOffsetSeconds);
    if (pool->inActionButton) {
	ESAssert(pool->currentCache);
	pushECAstroCacheInPool(pool, &pool->finalCache, dateInterval);
    } else {
	ESAssert(!pool->currentCache);
	pushECAstroCacheInPool(pool, &pool->finalCache, dateInterval);
    }
}

void releaseCachePoolForThisThread(ECAstroCachePool *cachePool) {
    ESAssert(cachePool == &astroCachePools[ESThread::inMainThread() ? 0 : 1]);
    ESAssert(cachePool->currentCache);
    popECAstroCacheToInPool(cachePool, NULL);
}

void clearAllCaches() {
    astroCachePools[0].currentGlobalCacheFlag++;
    astroCachePools[1].currentGlobalCacheFlag++;
}

