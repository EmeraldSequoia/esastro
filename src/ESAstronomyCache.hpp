//
//  ECAstronomyCache.h
//  Emerald Chronometer
//
//  Copied from Emerald Observatory 22 May 2011
//
//  Created by Steve Pucci on April 30 2009
//  Copyright Emerald Sequoia 2009. All rights reserved.
//

#ifndef _ECASTRONOMY_CACHE_
#define _ECASTRONOMY_CACHE_

#include "ESTime.hpp"

// NOTE: Enum values prior to firstLocationDependentSlotIndex should NOT depend on location,
// so please edit accordingly.  Note that by "location" we mean latitude/longitude *and* tzOffset
typedef enum _CacheSlotIndex {

    ////////////////////////////////////
    // Location independent slot indices
    ////////////////////////////////////
    priorUTMidnightSlotIndex,
    sunEclipticLongitudeSlotIndex,
    sunRASlotIndex,
    sunDeclSlotIndex,
    sunRAJ2000SlotIndex,
    sunDeclJ2000SlotIndex,
    sunTrueAnomalySlotIndex,
    sunMeanAnomalySlotIndex,
    moonRASlotIndex,
    moonDeclSlotIndex,
    moonRAJ2000SlotIndex,
    moonDeclJ2000SlotIndex,
    moonEclipticLongitudeSlotIndex,
    moonCorrectedAnomalySlotIndex,  // Apparently unused but it sounds like it's not location-dependent
    eotForDaySlotIndex,
    moonAgeSlotIndex,
    moonPhaseSlotIndex,
    nextMoonPhaseSlotIndex,
    prevMoonPhaseSlotIndex,
    closestNewMoonSlotIndex,
    closestFullMoonSlotIndex,
    closestFirstQuarterSlotIndex,
    closestThirdQuarterSlotIndex,
    closestSunEclipticLongitudeSlotIndex,
    closestSunEclipticLongitudeSlotIndex1,
    closestSunEclipticLongitudeSlotIndex2,
    closestSunEclipticLongitudeSlotIndex3,
    closestSunEclipticLongIndicatorAngleSlotIndex,
    closestSunEclipticLongIndicatorAngleSlotIndex1,
    closestSunEclipticLongIndicatorAngleSlotIndex2,
    closestSunEclipticLongIndicatorAngleSlotIndex3,
    nextNewMoonSlotIndex,
    nextFullMoonSlotIndex,
    nextFirstQuarterSlotIndex,
    nextThirdQuarterSlotIndex,
    moonPositionAngleSlotIndex,
    vernalEquinoxSlotIndex,
    moonAscendingNodeLongitudeSlotIndex,
    moonAscendingNodeRASlotIndex,
    moonAscendingNodeDeclSlotIndex,
    moonAscendingNodeRAJ2000SlotIndex,
    moonAscendingNodeDeclJ2000SlotIndex,
    precessionSlotIndex,
    calendarErrorSlotIndex,
    realMoonAgeAngleSlotIndex,
    tdtCenturiesSlotIndex,
    tdtCenturiesDeltaTSlotIndex,
    tdtHundredCenturiesSlotIndex,
    WBAscendingNodeLongitudeSlotIndex,
    WBLunarLongitudeLowSlotIndex,
    WBLunarLongitudeMidSlotIndex,
    WBLunarLongitudeFullSlotIndex,
    WBLunarLatitudeLowSlotIndex,
    WBLunarLatitudeMidSlotIndex,
    WBLunarLatitudeFullSlotIndex,
    WBLunarDistanceLowSlotIndex,
    WBLunarDistanceMidSlotIndex,
    WBLunarDistanceFullSlotIndex,
    WBMoonRALowSlotIndex,
    WBMoonRAMidSlotIndex,
    WBMoonRAFullSlotIndex,
    WBMoonDeclLowSlotIndex,
    WBMoonDeclMidSlotIndex,
    WBMoonDeclFullSlotIndex,
    WBMoonEclipticLongitudeLowSlotIndex,
    WBMoonEclipticLongitudeMidSlotIndex,
    WBMoonEclipticLongitudeFullSlotIndex,
    WBMoonEclipticLatitudeLowSlotIndex,
    WBMoonEclipticLatitudeMidSlotIndex,
    WBMoonEclipticLatitudeFullSlotIndex,
    WBMoonDistanceLowSlotIndex,
    WBMoonDistanceMidSlotIndex,
    WBMoonDistanceFullSlotIndex,
    WBSunLongitudeSlotIndex,
    WBSunLongitudeApparentSlotIndex,
    WBSunRadiusSlotIndex,
    WBNutationSlotIndex,
    WBObliquitySlotIndex,
    planetHeliocentricLongitudeSlotIndex,
    planetHeliocentricLongitudeSlotIndex1,
    planetHeliocentricLongitudeSlotIndex2,
    planetHeliocentricLongitudeSlotIndex3,
    planetHeliocentricLongitudeSlotIndex4,
    planetHeliocentricLongitudeSlotIndex5,
    planetHeliocentricLongitudeSlotIndex6,
    planetHeliocentricLongitudeSlotIndex7,
    planetHeliocentricLongitudeSlotIndex8,
    planetHeliocentricLongitudeSlotIndex9,  // up to Neptune
    planetHeliocentricLatitudeSlotIndex,
    planetHeliocentricLatitudeSlotIndex1,
    planetHeliocentricLatitudeSlotIndex2,
    planetHeliocentricLatitudeSlotIndex3,
    planetHeliocentricLatitudeSlotIndex4,
    planetHeliocentricLatitudeSlotIndex5,
    planetHeliocentricLatitudeSlotIndex6,
    planetHeliocentricLatitudeSlotIndex7,
    planetHeliocentricLatitudeSlotIndex8,
    planetHeliocentricLatitudeSlotIndex9,  // up to Neptune
    planetHeliocentricRadiusSlotIndex,
    planetHeliocentricRadiusSlotIndex1,
    planetHeliocentricRadiusSlotIndex2,
    planetHeliocentricRadiusSlotIndex3,
    planetHeliocentricRadiusSlotIndex4,
    planetHeliocentricRadiusSlotIndex5,
    planetHeliocentricRadiusSlotIndex6,
    planetHeliocentricRadiusSlotIndex7,
    planetHeliocentricRadiusSlotIndex8,
    planetHeliocentricRadiusSlotIndex9,  // up to Neptune
    planetGeocentricDistanceSlotIndex,
    planetGeocentricDistanceSlotIndex1,
    planetGeocentricDistanceSlotIndex2,
    planetGeocentricDistanceSlotIndex3,
    planetGeocentricDistanceSlotIndex4,
    planetGeocentricDistanceSlotIndex5,
    planetGeocentricDistanceSlotIndex6,
    planetGeocentricDistanceSlotIndex7,
    planetGeocentricDistanceSlotIndex8,
    planetGeocentricDistanceSlotIndex9,  // up to Neptune
    planetEclipticLongitudeSlotIndex,
    planetEclipticLongitudeSlotIndex1,
    planetEclipticLongitudeSlotIndex2,
    planetEclipticLongitudeSlotIndex3,
    planetEclipticLongitudeSlotIndex4,
    planetEclipticLongitudeSlotIndex5,
    planetEclipticLongitudeSlotIndex6,
    planetEclipticLongitudeSlotIndex7,
    planetEclipticLongitudeSlotIndex8,
    planetEclipticLongitudeSlotIndex9,  // up to Neptune
    planetEclipticLatitudeSlotIndex,
    planetEclipticLatitudeSlotIndex1,
    planetEclipticLatitudeSlotIndex2,
    planetEclipticLatitudeSlotIndex3,
    planetEclipticLatitudeSlotIndex4,
    planetEclipticLatitudeSlotIndex5,
    planetEclipticLatitudeSlotIndex6,
    planetEclipticLatitudeSlotIndex7,
    planetEclipticLatitudeSlotIndex8,
    planetEclipticLatitudeSlotIndex9,  // up to Neptune

    ////////////////////////////////////
    // Location dependent slot indices
    ////////////////////////////////////
    firstLocationDependentSlotIndex,
    nextSunriseSlotIndex,
    prevSunriseSlotIndex,
    nextMoonriseSlotIndex,
    prevMoonriseSlotIndex,
    nextSunsetSlotIndex,
    prevSunsetSlotIndex,
    nextSuntransitSlotIndex,
    nextMoonsetSlotIndex,
    prevMoonsetSlotIndex,
    nextMoontransitSlotIndex,
    sunriseForDaySlotIndex,
    sunsetForDaySlotIndex,
    moonriseForDaySlotIndex,
    moonsetForDaySlotIndex,
    suntransitForDaySlotIndex,
    moontransitForDaySlotIndex,
    moonRelativePositionAngleSlotIndex,
    moonRelativeAngleSlotIndex,
    sunAltitudeSlotIndex,
    sunAzimuthSlotIndex,
    moonAltitudeSlotIndex,
    moonAzimuthSlotIndex,
    azimuthOfHighestEclipticSlotIndex,
    longitudeOfHighestEclipticSlotIndex,
    eclipticAltitudeSlotIndex,
    longitudeOfEclipticMeridianSlotIndex,
    meridianTimeSlotIndex,
    moonMeridianTimeSlotIndex,
    lstSlotIndex,
    eclipseAngularSeparationSlotIndex,
    eclipseSeparationSlotIndex,
    eclipseShadowAngularSizeSlotIndex,
    eclipseKindSlotIndex,
    planetIsUpSlotIndex,
    planetIsUpSlotIndex1,
    planetIsUpSlotIndex2,
    planetIsUpSlotIndex3,
    planetIsUpSlotIndex4,
    planetIsUpSlotIndex5,
    planetIsUpSlotIndex6,
    planetIsUpSlotIndex7,
    planetIsUpSlotIndex8,
    planetIsUpSlotIndex9,  // up to Neptune
    nextPlanetriseSlotIndex,
    nextPlanetriseSlotIndex1,
    nextPlanetriseSlotIndex2,
    nextPlanetriseSlotIndex3,
    nextPlanetriseSlotIndex4,
    nextPlanetriseSlotIndex5,
    nextPlanetriseSlotIndex6,
    nextPlanetriseSlotIndex7,
    nextPlanetriseSlotIndex8,
    nextPlanetriseSlotIndex9,  // up to Neptune
    nextPlanetsetSlotIndex,
    nextPlanetsetSlotIndex1,
    nextPlanetsetSlotIndex2,
    nextPlanetsetSlotIndex3,
    nextPlanetsetSlotIndex4,
    nextPlanetsetSlotIndex5,
    nextPlanetsetSlotIndex6,
    nextPlanetsetSlotIndex7,
    nextPlanetsetSlotIndex8,
    nextPlanetsetSlotIndex9,  // up to Neptune
    nextPlanettransitSlotIndex,
    nextPlanettransitSlotIndex1,
    nextPlanettransitSlotIndex2,
    nextPlanettransitSlotIndex3,
    nextPlanettransitSlotIndex4,
    nextPlanettransitSlotIndex5,
    nextPlanettransitSlotIndex6,
    nextPlanettransitSlotIndex7,
    nextPlanettransitSlotIndex8,
    nextPlanettransitSlotIndex9,  // up to Neptune
    nextPlanettransitLowSlotIndex,
    nextPlanettransitLowSlotIndex1,
    nextPlanettransitLowSlotIndex2,
    nextPlanettransitLowSlotIndex3,
    nextPlanettransitLowSlotIndex4,
    nextPlanettransitLowSlotIndex5,
    nextPlanettransitLowSlotIndex6,
    nextPlanettransitLowSlotIndex7,
    nextPlanettransitLowSlotIndex8,
    nextPlanettransitLowSlotIndex9,  // up to Neptune
    prevPlanetriseSlotIndex,
    prevPlanetriseSlotIndex1,
    prevPlanetriseSlotIndex2,
    prevPlanetriseSlotIndex3,
    prevPlanetriseSlotIndex4,
    prevPlanetriseSlotIndex5,
    prevPlanetriseSlotIndex6,
    prevPlanetriseSlotIndex7,
    prevPlanetriseSlotIndex8,
    prevPlanetriseSlotIndex9,  // up to Neptune
    prevPlanetsetSlotIndex,
    prevPlanetsetSlotIndex1,
    prevPlanetsetSlotIndex2,
    prevPlanetsetSlotIndex3,
    prevPlanetsetSlotIndex4,
    prevPlanetsetSlotIndex5,
    prevPlanetsetSlotIndex6,
    prevPlanetsetSlotIndex7,
    prevPlanetsetSlotIndex8,
    prevPlanetsetSlotIndex9,  // up to Neptune
    prevPlanettransitSlotIndex,
    prevPlanettransitSlotIndex1,
    prevPlanettransitSlotIndex2,
    prevPlanettransitSlotIndex3,
    prevPlanettransitSlotIndex4,
    prevPlanettransitSlotIndex5,
    prevPlanettransitSlotIndex6,
    prevPlanettransitSlotIndex7,
    prevPlanettransitSlotIndex8,
    prevPlanettransitSlotIndex9,  // up to Neptune
    prevPlanettransitLowSlotIndex,
    prevPlanettransitLowSlotIndex1,
    prevPlanettransitLowSlotIndex2,
    prevPlanettransitLowSlotIndex3,
    prevPlanettransitLowSlotIndex4,
    prevPlanettransitLowSlotIndex5,
    prevPlanettransitLowSlotIndex6,
    prevPlanettransitLowSlotIndex7,
    prevPlanettransitLowSlotIndex8,
    prevPlanettransitLowSlotIndex9,  // up to Neptune
    dayNightMasterRiseAngleSlotIndex,
    dayNightMasterRiseAngleSlotIndex1,
    dayNightMasterRiseAngleSlotIndex2,
    dayNightMasterRiseAngleSlotIndex3,
    dayNightMasterRiseAngleSlotIndex4,
    dayNightMasterRiseAngleSlotIndex5,
    dayNightMasterRiseAngleSlotIndex6,
    dayNightMasterRiseAngleSlotIndex7,
    dayNightMasterRiseAngleSlotIndex8,
    dayNightMasterRiseAngleSlotIndex9,  // up to Neptune
    dayNightMasterSetAngleSlotIndex,
    dayNightMasterSetAngleSlotIndex1,
    dayNightMasterSetAngleSlotIndex2,
    dayNightMasterSetAngleSlotIndex3,
    dayNightMasterSetAngleSlotIndex4,
    dayNightMasterSetAngleSlotIndex5,
    dayNightMasterSetAngleSlotIndex6,
    dayNightMasterSetAngleSlotIndex7,
    dayNightMasterSetAngleSlotIndex8,
    dayNightMasterSetAngleSlotIndex9,  // up to Neptune
    dayNightMasterRTransitAngleSlotIndex,
    dayNightMasterRTransitAngleSlotIndex1,
    dayNightMasterRTransitAngleSlotIndex2,
    dayNightMasterRTransitAngleSlotIndex3,
    dayNightMasterRTransitAngleSlotIndex4,
    dayNightMasterRTransitAngleSlotIndex5,
    dayNightMasterRTransitAngleSlotIndex6,
    dayNightMasterRTransitAngleSlotIndex7,
    dayNightMasterRTransitAngleSlotIndex8,
    dayNightMasterRTransitAngleSlotIndex9,  // up to Neptune
    dayNightMasterSTransitAngleSlotIndex,
    dayNightMasterSTransitAngleSlotIndex1,
    dayNightMasterSTransitAngleSlotIndex2,
    dayNightMasterSTransitAngleSlotIndex3,
    dayNightMasterSTransitAngleSlotIndex4,
    dayNightMasterSTransitAngleSlotIndex5,
    dayNightMasterSTransitAngleSlotIndex6,
    dayNightMasterSTransitAngleSlotIndex7,
    dayNightMasterSTransitAngleSlotIndex8,
    dayNightMasterSTransitAngleSlotIndex9,  // up to Neptune
    dayNightMasterRiseAngleLSTSlotIndex,
    dayNightMasterRiseAngleLSTSlotIndex1,
    dayNightMasterRiseAngleLSTSlotIndex2,
    dayNightMasterRiseAngleLSTSlotIndex3,
    dayNightMasterRiseAngleLSTSlotIndex4,
    dayNightMasterRiseAngleLSTSlotIndex5,
    dayNightMasterRiseAngleLSTSlotIndex6,
    dayNightMasterRiseAngleLSTSlotIndex7,
    dayNightMasterRiseAngleLSTSlotIndex8,
    dayNightMasterRiseAngleLSTSlotIndex9,  // up to Neptune
    dayNightMasterSetAngleLSTSlotIndex,
    dayNightMasterSetAngleLSTSlotIndex1,
    dayNightMasterSetAngleLSTSlotIndex2,
    dayNightMasterSetAngleLSTSlotIndex3,
    dayNightMasterSetAngleLSTSlotIndex4,
    dayNightMasterSetAngleLSTSlotIndex5,
    dayNightMasterSetAngleLSTSlotIndex6,
    dayNightMasterSetAngleLSTSlotIndex7,
    dayNightMasterSetAngleLSTSlotIndex8,
    dayNightMasterSetAngleLSTSlotIndex9,  // up to Neptune
    dayNightMasterRTransitAngleLSTSlotIndex,
    dayNightMasterRTransitAngleLSTSlotIndex1,
    dayNightMasterRTransitAngleLSTSlotIndex2,
    dayNightMasterRTransitAngleLSTSlotIndex3,
    dayNightMasterRTransitAngleLSTSlotIndex4,
    dayNightMasterRTransitAngleLSTSlotIndex5,
    dayNightMasterRTransitAngleLSTSlotIndex6,
    dayNightMasterRTransitAngleLSTSlotIndex7,
    dayNightMasterRTransitAngleLSTSlotIndex8,
    dayNightMasterRTransitAngleLSTSlotIndex9,  // up to Neptune
    dayNightMasterSTransitAngleLSTSlotIndex,
    dayNightMasterSTransitAngleLSTSlotIndex1,
    dayNightMasterSTransitAngleLSTSlotIndex2,
    dayNightMasterSTransitAngleLSTSlotIndex3,
    dayNightMasterSTransitAngleLSTSlotIndex4,
    dayNightMasterSTransitAngleLSTSlotIndex5,
    dayNightMasterSTransitAngleLSTSlotIndex6,
    dayNightMasterSTransitAngleLSTSlotIndex7,
    dayNightMasterSTransitAngleLSTSlotIndex8,
    dayNightMasterSTransitAngleLSTSlotIndex9,  // up to Neptune
    sunGoldenHourMorning,
    sunRiseMorning,
    sunCivilTwilightMorning,
    sunNauticalTwilightMorning,
    sunAstroTwilightMorning,
    sunGoldenHourEvening,
    sunSetEvening,
    sunCivilTwilightEvening,
    sunNauticalTwilightEvening,
    sunAstroTwilightEvening,
    planetriseForDaySlotIndex,
    planetriseForDaySlotIndex1,
    planetriseForDaySlotIndex2,
    planetriseForDaySlotIndex3,
    planetriseForDaySlotIndex4,
    planetriseForDaySlotIndex5,
    planetriseForDaySlotIndex6,
    planetriseForDaySlotIndex7,
    planetriseForDaySlotIndex8,
    planetriseForDaySlotIndex9,  // up to Neptune
    planetsetForDaySlotIndex,
    planetsetForDaySlotIndex1,
    planetsetForDaySlotIndex2,
    planetsetForDaySlotIndex3,
    planetsetForDaySlotIndex4,
    planetsetForDaySlotIndex5,
    planetsetForDaySlotIndex6,
    planetsetForDaySlotIndex7,
    planetsetForDaySlotIndex8,
    planetsetForDaySlotIndex9,  // up to Neptune
    planettransitForDaySlotIndex,
    planettransitForDaySlotIndex1,
    planettransitForDaySlotIndex2,
    planettransitForDaySlotIndex3,
    planettransitForDaySlotIndex4,
    planettransitForDaySlotIndex5,
    planettransitForDaySlotIndex6,
    planettransitForDaySlotIndex7,
    planettransitForDaySlotIndex8,
    planettransitForDaySlotIndex9,  // up to Neptune
    planetAltitudeSlotIndex,
    planetAltitudeSlotIndex1,
    planetAltitudeSlotIndex2,
    planetAltitudeSlotIndex3,
    planetAltitudeSlotIndex4,
    planetAltitudeSlotIndex5,
    planetAltitudeSlotIndex6,
    planetAltitudeSlotIndex7,
    planetAltitudeSlotIndex8,
    planetAltitudeSlotIndex9,  // up to Neptune
    planetAzimuthSlotIndex,
    planetAzimuthSlotIndex1,
    planetAzimuthSlotIndex2,
    planetAzimuthSlotIndex3,
    planetAzimuthSlotIndex4,
    planetAzimuthSlotIndex5,
    planetAzimuthSlotIndex6,
    planetAzimuthSlotIndex7,
    planetAzimuthSlotIndex8,
    planetAzimuthSlotIndex9,  // up to Neptune
    planetRASlotIndex,
    planetRASlotIndex1,
    planetRASlotIndex2,
    planetRASlotIndex3,
    planetRASlotIndex4,
    planetRASlotIndex5,
    planetRASlotIndex6,
    planetRASlotIndex7,
    planetRASlotIndex8,
    planetRASlotIndex9,  // up to Neptune
    planetDeclSlotIndex,
    planetDeclSlotIndex1,
    planetDeclSlotIndex2,
    planetDeclSlotIndex3,
    planetDeclSlotIndex4,
    planetDeclSlotIndex5,
    planetDeclSlotIndex6,
    planetDeclSlotIndex7,
    planetDeclSlotIndex8,
    planetDeclSlotIndex9,  // up to Neptune
    planetRATopoSlotIndex,
    planetRATopoSlotIndex1,
    planetRATopoSlotIndex2,
    planetRATopoSlotIndex3,
    planetRATopoSlotIndex4,
    planetRATopoSlotIndex5,
    planetRATopoSlotIndex6,
    planetRATopoSlotIndex7,
    planetRATopoSlotIndex8,
    planetRATopoSlotIndex9,  // up to Neptune
    planetDeclTopoSlotIndex,
    planetDeclTopoSlotIndex1,
    planetDeclTopoSlotIndex2,
    planetDeclTopoSlotIndex3,
    planetDeclTopoSlotIndex4,
    planetDeclTopoSlotIndex5,
    planetDeclTopoSlotIndex6,
    planetDeclTopoSlotIndex7,
    planetDeclTopoSlotIndex8,
    planetDeclTopoSlotIndex9,  // up to Neptune
    planetMeridianTimeSlotIndex,
    planetMeridianTimeSlotIndex1,
    planetMeridianTimeSlotIndex2,
    planetMeridianTimeSlotIndex3,
    planetMeridianTimeSlotIndex4,
    planetMeridianTimeSlotIndex5,
    planetMeridianTimeSlotIndex6,
    planetMeridianTimeSlotIndex7,
    planetMeridianTimeSlotIndex8,
    planetMeridianTimeSlotIndex9,  // up to Neptune
    numCacheSlots
} CacheSlotIndex;

typedef struct _ECAstroCache {
    ESTimeInterval dateInterval;
    ESTimeInterval astroSlop;
    unsigned int currentFlag;  // if validFlag[i] == currentFlag it means the cache slot is valid
    unsigned int globalValidFlag;   // to test against currentGlobalCacheFlag in a similar way;
    int inUseCount;
    unsigned int cacheSlotValidFlag[numCacheSlots];
    double cacheSlots[numCacheSlots];
} ECAstroCache;

typedef struct _ECAstroCachePool {
    double       observerLatitude;
    double       observerLongitude;
    bool         runningBackward;
    int          tzOffsetSeconds;
    bool         inActionButton;
    unsigned int currentGlobalCacheFlag;
    ECAstroCache finalCache;
    ECAstroCache tempCache;
    ECAstroCache refinementCache;
    ECAstroCache midnightCache;
    ECAstroCache year2000Cache;
    ECAstroCache *currentCache;
} ECAstroCachePool;  // about 43k bytes used here in static storage

#define ASTRO_SLOP_RAW (2.0)  // number of seconds of slop in astro functions -- if the date has not changed by this much we do not recalculate
#define ASTRO_SLOP (_currentCache ? _currentCache->astroSlop : ASTRO_SLOP_RAW)

// Return cache pool for this thread
extern ECAstroCachePool *getCachePoolForThisThread();

// Initialize currentCache in that pool with the given data
extern void initializeCachePool(ECAstroCachePool *cachePool,
				ESTimeInterval   dateInterval,
				double           observerLatitude,
				double           observerLongitude,
				bool             runningBackward,
				int              tzOffsetSeconds);

// Release cache pool
extern void releaseCachePoolForThisThread(ECAstroCachePool *cachePool);

// Set the given value cache active in the given pool, and return the previously active cache
// so it can be popped to later.  If dateInterval isn't sufficiently
// close to cached value in the new cache, invalidate the cache.
extern ECAstroCache *pushECAstroCacheInPool(ECAstroCachePool *cachePool,
					    ECAstroCache     *valueCache,
					    ESTimeInterval   dateInterval);
extern ECAstroCache *pushECAstroCacheWithSlopInPool(ECAstroCachePool *cachePool,
						    ECAstroCache     *valueCache,
						    ESTimeInterval   dateInterval,
						    ESTimeInterval   slop);

// The given cache is presumed to still represent the correct date interval, so no checking is done.
extern void popECAstroCacheToInPool(ECAstroCachePool *cachePool,
				    ECAstroCache     *valueCache);

extern void printCache(ECAstroCache     *valueCache,
		       ECAstroCachePool *cachePool);

extern void initializeAstroCache();

extern void assertCacheValidForTDTCenturies(ECAstroCache *cache,
					    double       centuriesSinceEpochTDT);
extern void assertCacheValidForTDTHundredCenturies(ECAstroCache *cache,
						   double       hundredCenturiesSinceEpochTDT);

extern void clearAllCaches();

#endif // _ECASTRONOMY_CACHE_
