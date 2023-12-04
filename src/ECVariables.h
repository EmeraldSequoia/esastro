//
//  ECVariables.h
//
//  Created by Steve Pucci 19 Mar 2016
//  Copyright Emerald Sequoia LLC 2016. All rights reserved.
//
//  Ported from the prior version of ECImportVariables in ECGlobals.m

#ifndef _ECVARIABLES_H_
#define _ECVARIABLES_H_

#include "ECConstants.h"

struct ECPredefinedVariable {
    const char              *const name;
    const double            value;
};

struct ECPredefinedVariable ECPredefinedVariables[] = {
    { "pi", M_PI },
    { "M_PI", M_PI },
    
    { "none", 0 },
    { "front", frontMask },
    { "night", nightMask },
    { "back", backMask },
    { "stop", stopMask },
    { "spare", spareMask },
    { "world", worldMask },
    { "special", specialMask },
    { "all", allModes },
    
    { "upright", ECDialOrientationUpright },
    { "radial", ECDialOrientationRadial },
    { "demi", ECDialOrientationDemiRadial },
    { "rotated", ECDialOrientationRotatedRadial },
    { "tachy", ECDialOrientationTachy },
    { "year", ECDialOrientationYear },

    { "tickNone", ECDialTickNone },
    { "tick4", ECDialTick4 },
    { "tick8", ECDialTick8 },
    { "tick10", ECDialTick10 },
    { "tick12", ECDialTick12 },
    { "tick16", ECDialTick16 },
    { "tick36", ECDialTick36 },
    { "tick60", ECDialTick60 },
    { "tick72", ECDialTick72 },
    { "tick96", ECDialTick96 },
    { "tick180", ECDialTick180 },
    { "tick240", ECDialTick240 },
    { "tick241", ECDialTick241 },
    { "tick288", ECDialTick288 },
    { "tick300", ECDialTick300 },
    { "tick360", ECDialTick360 },
    
    { "twelve", ECWheelOrientationTwelve },
    { "three", ECWheelOrientationThree },
    { "six", ECWheelOrientationSix },
    { "nine", ECWheelOrientationNine },
    { "straight", ECWheelOrientationStraight },
    
    { "center", ECDiskMarksMaskCenter },
    { "inner", ECDiskMarksMaskInner },
    { "outer", ECDiskMarksMaskOuter },
    { "dot", ECDiskMarksMaskDotRing },
    { "tachym", ECDiskMarksMaskTachy },
    { "no5s", ECDiskMarksMaskNo5s },
    { "rose", ECDiskMarksMaskRose },
    { "tickIn", ECDiskMarksMaskTickIn },
    { "tickOut", ECDiskMarksMaskTickOut },
    { "arc", ECDiskMarksMaskArc },
    { "line", ECDiskMarksMaskLine },
    { "odd", ECDiskMarksMaskOdd },
    
    { "rect", ECQHandRect },
    { "tri", ECQHandTri },
    { "quad", ECQHandQuad },
    { "cube", ECQHandCube },
    { "rise", ECQHandRise },
    { "set", ECQHandSet },
    { "sun", ECQHandSun },
    { "sun2", ECQHandSun2 },
    { "spoke", ECQHandSpoke },
    { "wire", ECQHandWire },
    { "gear", ECQHandGear },
    { "breguet", ECQHandBreguet },
    
    { "black", ECblack },
    { "blue", ECblue },
    { "green", ECgreen },
    { "cyan", ECcyan },
    { "red", ECred },
    { "yellow", ECyellow },
    { "magenta", ECmagenta },
    { "white", ECwhite },
    { "brown", ECbrown },
    { "darkGray", ECdarkGray },
    { "lightGray", EClightGray },
    { "clear", ECclear },
    { "nfgclr", ECnfgclr },
    
    { "window", ECHoleWind },
    { "porthole", ECHolePort },
    
    { "updateAtNextSunrise", ECDynamicUpdateNextSunrise },
    { "updateAtNextSunset", ECDynamicUpdateNextSunset },
    { "updateAtNextMoonrise", ECDynamicUpdateNextMoonrise },
    { "updateAtNextMoonset", ECDynamicUpdateNextMoonset },
    { "updateAtNextMoonriseOrMoonset", ECDynamicUpdateNextMoonriseOrMoonset },
    { "updateAtNextSunriseOrSunset", ECDynamicUpdateNextSunriseOrSunset },
    { "updateForSunriseCover", ECDynamicUpdateForSunriseCover },
    { "updateForSunsetCover", ECDynamicUpdateForSunsetCover },
    { "updateForMoonriseCover", ECDynamicUpdateForMoonriseCover },
    { "updateForMoonsetCover", ECDynamicUpdateForMoonsetCover },
    { "updateAtNextSunriseOrMidnight", ECDynamicUpdateNextSunriseOrMidnight },
    { "updateAtNextSunsetOrMidnight", ECDynamicUpdateNextSunsetOrMidnight },
    { "updateAtNextMoonriseOrMidnight", ECDynamicUpdateNextMoonriseOrMidnight },
    { "updateAtNextMoonsetOrMidnight", ECDynamicUpdateNextMoonsetOrMidnight },
    { "updateAtNextDSTChange", ECDynamicUpdateNextDSTChange },
    { "updateAtEnvChangeOnly", ECDynamicUpdateNextEnvChange },
    { "updateForLocSyncIndicator", ECDynamicUpdateLocSyncIndicator },
    { "updateForTimeSyncIndicator", ECDynamicUpdateTimeSyncIndicator },
    
    { "secondKind", ECSecondHandKind },
    { "minuteKind", ECMinuteHandKind },
    { "hour12Kind", ECHour12HandKind },
    { "hour24Kind", ECHour24HandKind },
    { "targetMinuteKind", ECTargetMinuteHandKind },
    { "targetHour12Kind", ECTargetHour12HandKind },
    { "targetHour12KindB", ECTargetHour12HandKindB },
    { "targetHour24Kind", ECTargetHour24HandKind },
    { "intervalSecondKind", ECIntervalSecondHandKind },
    { "intervalMinuteKind", ECIntervalMinuteHandKind },
    { "intervalHour12Kind", ECIntervalHour12HandKind },
    { "intervalHour24Kind", ECIntervalHour24HandKind },
    { "reverseHour24Kind", ECReverseHour24Kind },
    { "dayKind", ECDayHandKind },
    { "weekDayKind", ECWkDayHandKind },
    { "moonDayKind", ECMoonDayHandKind },
    { "monthKind", ECMonthHandKind },
    { "year1Kind", ECYear1HandKind },
    { "mercuryYearKind", ECMercuryYearHandKind },
    { "venusYearKind", ECVenusYearHandKind },
    { "earthYearKind", ECEarthYearHandKind },
    { "marsYearKind", ECMarsYearHandKind },
    { "jupiterYearKind", ECJupiterYearHandKind },
    { "saturnYearKind", ECSaturnYearHandKind },
    { "year10Kind", ECYear10HandKind },
    { "year100Kind", ECYear100HandKind },
    { "year1000Kind", ECYear1000HandKind },
    { "greatYearKind", ECGreatYearHandKind },
    { "hour24MoonKind", ECHour24MoonHandKind },
    { "moonRAKind", ECMoonRAHandKind },
    { "reverseMoonRAKind", ECReverseMoonRAHandKind },
    { "reverseSunRAKind", ECReverseSunRAHandKind },
    { "sunRAKind", ECSunRAHandKind },
    { "nodalKind", ECNodalHandKind },
    { "worldtimeRingKind", ECWorldtimeRingHandKind },
    { "latitudeMinuteOnesKind", ECLatitudeMinuteOnesHandKind },
    { "latitudeMinuteTensKind", ECLatitudeMinuteTensHandKind },
    { "latitudeOnesKind", ECLatitudeOnesHandKind },
    { "latitudeTensKind", ECLatitudeTensHandKind },
    { "latitudeSignKind", ECLatitudeSignHandKind },
    { "longitudeMinuteOnesKind", ECLongitudeMinuteOnesHandKind },
    { "longitudeMinuteTensKind", ECLongitudeMinuteTensHandKind },
    { "longitudeOnesKind", ECLongitudeOnesHandKind },
    { "longitudeTensKind", ECLongitudeTensHandKind },
    { "longitudeHundredsKind", ECLongitudeHundredsHandKind },
    { "longitudeSignKind", ECLongitudeSignHandKind },
    
    { "always", ECButtonEnabledAlways },
    { "stemOutOnly", ECButtonEnabledStemOutOnly },
    { "alarmStemOutOnly", ECButtonEnabledAlarmStemOutOnly },
    { "wrongTimeOnly", ECButtonEnabledWrongTimeOnly },
    
    { "animAlwaysCW", ECAnimationDirAlwaysCW },
    { "animAlwaysCCW", ECAnimationDirAlwaysCCW },
    { "animLogicalForward", ECAnimationDirLogicalForward },
    { "animLogicalBackward", ECAnimationDirLogicalBackward },
    { "animClosest", ECAnimationDirClosest },
    { "animFurthest", ECAnimationDirFurthest },

    { "dragAnimationNever", ECDragAnimationNever },
    { "dragAnimationAlways", ECDragAnimationAlways },
    { "dragAnimationHack1", ECDragAnimationHack1 },
    { "dragAnimationHack2", ECDragAnimationHack2 },

    { "dragNormal", ECDragNormal },
    { "dragHack1", ECDragHack1 },

    { "mainTimer", ECMainTimer },
    { "stopwatchTimer", ECStopwatchTimer },
    { "stopwatchLapTimer", ECStopwatchLapTimer },

    { "notSpecial", ECPartNotSpecial },
    { "specialWorldtime", ECPartSpecialWorldtimeRing },
    { "specialSubdial", ECPartSpecialSubdial },
    { "specialDotsMap", ECPartSpecialDotsMap },

    { "ECPartDoesNotRepeat", ECPartDoesNotRepeat },
    { "ECPartRepeatsSlowlyOnly", ECPartRepeatsSlowlyOnly },
    { "ECPartRepeatsAndAcceleratesOnce", ECPartRepeatsAndAcceleratesOnce },
    { "ECPartRepeatsAndAcceleratesTwice", ECPartRepeatsAndAcceleratesTwice },

    { "notCalendarWheel", ECNotCalendarWheel },
    { "calendarWheel012B", ECCalendarWheel012B },
    { "calendarWheel3456", ECCalendarWheel3456 },
    { "calendarWheelOct1582", ECCalendarWheelOct1582 },

    { "row1Left", ECCalendarCoverRow1Left },
    { "row1Right", ECCalendarCoverRow1Right },
    { "row56Right", ECCalendarCoverRow56Right },
    { "row6Left", ECCalendarCoverRow6Left },

    { "planetSun", ECPlanetSun },
    { "planetMoon", ECPlanetMoon },
    { "planetMercury", ECPlanetMercury },
    { "planetVenus", ECPlanetVenus },
    { "planetEarth", ECPlanetEarth },
    { "planetMars", ECPlanetMars },
    { "planetJupiter", ECPlanetJupiter },
    { "planetSaturn", ECPlanetSaturn },
    { "planetUranus", ECPlanetUranus },
    { "planetNeptune", ECPlanetNeptune },
    // NO PLUTO -- NOTHING WORKS, CATCH IT IN THE XML
    { "planetMidnightSun", ECPlanetMidnightSun },
    
};

int ECNumPredefinedVariables = sizeof(ECPredefinedVariables) / sizeof(struct ECPredefinedVariable);

#endif  // _ECVARIABLES_H_
