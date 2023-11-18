//
//  ECAstronomy.h
//  Emerald Chronometer
//
//  Created by Steve Pucci 5/2008.
//  Copyright Emerald Sequoia LLC 2008. All rights reserved.
//
//  Copied from Emerald Observatory 22 May 2011
//

// ConverterHints
// environment
// watchTime
// calculationDateInterval
// estz
// locationValid
// currentCache
// astroCachePool
// scratchWatchTime
// inActionButton
// EndConverterHints

#ifndef _ESASTRONOMY_HPP_
#define _ESASTRONOMY_HPP_

#include "ESAstroConstants.hpp"

class ESTimeEnvironment;
class ESWatchTime;
class ESLocation;

#include "ESAstronomyCache.hpp"
#include "ESCalendar.hpp"  // For opaque ESTimeZone
#include "ESUserString.hpp"

#include <string>

// Internal type only
typedef ESTimeInterval (*CalculationMethod)(ESTimeInterval   calculationDate,
                                            double           observerLatitude,
                                            double           observerLongitude,
                                            bool             riseNotSet,
                                            int              planetNumber,
                                            double           overrideAltitudeDesired,
                                            double           *riseSetOrTransit,
                                            ECAstroCachePool *cachePool);

class ESAstronomyManager {
  public:
                            ESAstronomyManager(ESTimeEnvironment *environment,
                                               ESLocation        *location);
    virtual                 ~ESAstronomyManager();

    static void             initializeStatics();
    static double           widthOfZodiacConstellation(int n);
    static double           centerOfZodiacConstellation(int n);
    static ESUserString     zodiacConstellationOf(double elong);

    /* Ecliptic longitudes of constellation centers (per Bill) */
    static const double     *zodiacCentersDegrees() { return _zodiacCenters; }

    /* Ecliptic longitudes of constellation western edges (per Bill) */
    static const double     *zodiacEdgesDegrees() { return _zodiacEdges; }

    static ESUserString     nameOfPlanetWithNumber(int planetNumber);

// Methods called by ECGLWatch prior to updating a series of parts, and after the update is complete
    void                    setupLocalEnvironmentForThreadFromActionButton(bool        fromActionButton,
                                                                           ESWatchTime *watchTime);
    void                    cleanupLocalEnvironmentForThreadFromActionButton(bool fromActionButton);

// The following calculation functions operate on the ESWatchTime virtual time
// indicated by the environment's watch time

// The sunrise for the day given by the environment's time, whether before or after that time
    ESTimeInterval          sunriseForDay();
// The sunset for the day given by the environment's time, whether before or after that time
    ESTimeInterval          sunsetForDay();

// The first sunrise following the time in the environment, whether on the same day or the next
// When the environment's clock is running backward, it returns the previous sunrise instead.
    ESTimeInterval          nextSunrise();
    ESTimeInterval          nextSunriseOrMidnight();
// The first sunset following the time in the environment, whether on the same day or the next
// When the environment's clock is running backward, it returns the previous sunset instead.
    ESTimeInterval          nextSunset();
    ESTimeInterval          nextSunsetOrMidnight();
    ESTimeInterval          prevSunrise();
    ESTimeInterval          prevSunset();

// The moonrise for the day given by the environment's time, whether before or after that time
    ESTimeInterval          moonriseForDay();
// The moonset for the day given by the environment's time, whether before or after that time
    ESTimeInterval          moonsetForDay();

// The first moonrise following the time in the environment, whether on the same day or the next
// When the environment's clock is running backward, it returns the previous moonrise instead.
    ESTimeInterval          nextMoonrise();
    ESTimeInterval          nextMoonriseOrMidnight();
// The first moonset following the time in the environment, whether on the same day or the next
// When the environment's clock is running backward, it returns the previous moonset instead.
    ESTimeInterval          nextMoonset();
    ESTimeInterval          nextMoonsetOrMidnight();
    ESTimeInterval          prevMoonrise();
    ESTimeInterval          prevMoonset();

    ESTimeInterval          planetriseForDay(int planetNumber);
    ESTimeInterval          planetsetForDay(int planetNumber);
    ESTimeInterval          nextPlanetriseForPlanetNumber(int planetNumber);
    ESTimeInterval          nextPlanetsetForPlanetNumber(int planetNumber);
    ESTimeInterval          prevPlanetriseForPlanetNumber(int planetNumber);
    ESTimeInterval          prevPlanetsetForPlanetNumber(int planetNumber);

    ESTimeInterval          moontransitForDay();
    ESTimeInterval          suntransitForDay();
    ESTimeInterval          nextMoontransit();
    ESTimeInterval          nextSuntransit();
    ESTimeInterval          prevSuntransit();
    ESTimeInterval          nextSuntransitLow();
    ESTimeInterval          prevSuntransitLow();
    
// return true in summer half of the year
    bool                    summer();

// returns 1 if planet is above the equator and the observer is also, or both below
    bool                    planetIsSummer(int planetNumber);

// return the local sidereal time
    double                  localSiderealTime();

// Separation of Sun from Moon, or Earth's shadow from Moon, scaled such that
//   1) partial eclipse starts when separation == 2
//   2) total eclipse starts when separation == 1
// Note that zero doesn't therefore represent zero separation, and that zero separation may lie above or below the total eclipse point depending on the relative diameters
    double                  eclipseAbstractSeparation();
    double                  eclipseAngularSeparation();
    double                  eclipseShadowAngularSize();
    ECEclipseKind           eclipseKind();
    static bool             eclipseKindIsMoreSolarThanLunar(ECEclipseKind eclipseKind);

// Whether the given op has a valid date (difficult to tell otherwise now that we supply the meridian
// time on the clock)
    bool                    nextSunriseValid();
    bool                    nextSunsetValid();
    bool                    nextMoonriseValid();
    bool                    nextMoonsetValid();
    bool                    prevSunriseValid();
    bool                    prevSunsetValid();
    bool                    prevMoonriseValid();
    bool                    prevMoonsetValid();
    bool                    sunriseForDayValid();
    bool                    sunsetForDayValid();
    bool                    moonriseForDayValid();
    bool                    moonsetForDayValid();
    bool                    suntransitForDayValid();
    bool                    moontransitForDayValid();
    bool                    planetriseForDayValid(int planetNumber);
    bool                    planetsetForDayValid(int planetNumber);
    bool                    planettransitForDayValid(int planetNumber);
    bool                    nextPlanetriseValid(int planetNumber);
    bool                    nextPlanetsetValid(int planetNumber);
    
// Special op for day/night indicator leaves
    double                  dayNightLeafAngleForPlanetNumber(int            planetNumber,
                                                             double         leafNumber,
                                                             int            numLeaves,
                                                             double         overrideAltitudeDesired,
                                                             bool           *isRiseSet, // Valid only if numLeaves == 0; will store false here if there is no rise or set and we're returning the transit
                                                             bool           *aboveHorizon, // Valid only if numLeaves == 0 and *isRiseSet returns false.  You must supply non-NULL here if 
                                                             ESTimeBaseKind timeBaseKind = ESTimeBaseKindLT);

// Special ops for Mauna Kea & the planet equivalent
    bool                    sunriseIndicatorValid();
    bool                    sunsetIndicatorValid();
    double                  sunrise24HourIndicatorAngle();
    bool                    polarSummer();
    bool                    polarWinter();
    bool                    polarPlanetSummer(int planetNumber);
    bool                    polarPlanetWinter(int planetNumber);
    double                  sunset24HourIndicatorAngle();
    double                  moonrise24HourIndicatorAngle();
    double                  moonset24HourIndicatorAngle();
    double                  planetrise24HourIndicatorAngle(int planetNumber);
    double                  planetset24HourIndicatorAngle(int planetNumber);
    double                  planetrise24HourIndicatorAngle(int  planetNumber,
                                                           bool *isRiseSet,
                                                           bool *aboveHorizon);
    double                  planetset24HourIndicatorAngle(int  planetNumber,
                                                          bool *isRiseSet,
                                                          bool *aboveHorizon);
    double                  planettransit24HourIndicatorAngle(int planetNumber);
    double                  planetrise24HourIndicatorAngleLST(int planetNumber);
    double                  planetset24HourIndicatorAngleLST(int planetNumber);

// Age in moon.  This routine makes one revolution of EC_2PI every 28+ days
    double                  moonAgeAngle();
    double                  realMoonAgeAngle();
    ESTimeInterval          nextMoonPhase(); // new, 1st, full, third
    ESTimeInterval          prevMoonPhase(); // new, 1st, full, third
    ESTimeInterval          nextQuarterAngle(double         quarterAngle,
                                             ESTimeInterval fromTime,
                                             bool           nextNotPrev);
    double                  moonPositionAngle();  // rotation of terminator relative to earth's North (std defn)
    double                  moonRelativePositionAngle();  // rotation of terminator as it appears in the sky
    double                  moonRelativeAngle(); // rotation of moon image as it appears in the sky
    ESTimeInterval          closestNewMoon();
    ESTimeInterval          closestFullMoon();
    ESTimeInterval          closestFirstQuarter();
    ESTimeInterval          closestThirdQuarter();
    ESTimeInterval          nextNewMoon();
    ESTimeInterval          nextFullMoon();
    ESTimeInterval          nextFirstQuarter();
    ESTimeInterval          nextThirdQuarter();
    std::string             moonPhaseString();

    static double           moonDeltaEclipticLongitudeAtDateInterval(double dateInterval);

// Functions for doing planetary terminator displays
    double                  planetMoonAgeAngle(int planetNumber);
    double                  planetPositionAngle(int planetNumber);  // rotation of terminator relative to North (std defn)
    double                  planetRelativePositionAngle(int planetNumber); // rotation of terminator as it appears in the sky

// Sun and Moon equatorial positions
    double                  sunRA();
    double                  sunDecl();
    double                  moonRA();
    double                  moonDecl();

// Sun and Moon alt/az positions
    double                  sunAltitude();
    double                  sunAzimuth();
    double                  moonAltitude();
    double                  moonAzimuth();
    double                  planetAltitude(int planetNumber);
    double                  planetAzimuth(int planetNumber);
    double                  planetAltitude(int            planetNumber,
                                           ESTimeInterval atDateInterval);
    double                  planetAzimuth(int            planetNumber,
                                          ESTimeInterval atDateInterval);
    bool                    planetIsUp(int planetNumber);
    double                  planetRA(int  planetNumber,
                                     bool correctForParallax);
    double                  planetRA(int            planetNumber,
                                     ESTimeInterval atTime,
                                     bool           correctForParallax);
    double                  planetDecl(int  planetNumber,
                                       bool correctForParallax);
    double                  planetEclipticLongitude(int planetNumber);
    double                  planetEclipticLatitude(int planetNumber);
    double                  planetGeocentricDistance(int planetNumber);
    double                  planetRadius(int n);
    double                  planetApparentDiameter(int n);
    double                  planetMass(int n);
    double                  planetOribitalPeriod(int n);
    
// Moon ascending node
    double                  moonAscendingNodeLongitude();
    double                  moonAscendingNodeRA();
    double                  moonAscendingNodeRAJ2000();

// Precession of the equinoxes
    double                  precession();

// Calendar error for Julian calendar
    double                  calendarErrorVsTropicalYear();

// 0 => long==0, 1 => long==PI/2, etc
    ESTimeInterval          refineTimeOfClosestSunEclipticLongitude(int longitudeQuarter);
    double                  closestSunEclipticLongitudeQuarter366IndicatorAngle(int longitudeQuarter);

    ESTimeInterval          planettransitForDay(int planetNumber);
    ESTimeInterval          nextPlanettransit(int planetNumber);
    ESTimeInterval          prevPlanettransit(int planetNumber);

    double                  planetHeliocentricLongitude(int planetNumber);
    double                  planetHeliocentricLatitude(int planetNumber);
    double                  planetHeliocentricRadius(int planetNumber);

// Azimuth and ecliptic longitude of where the ecliptic has its highest altitude at the present time
    double                  azimuthOfHighestEclipticAltitude();
    double                  longitudeOfHighestEclipticAltitude();

// Angle ecliptic makes with the horizon
    double                  eclipticAltitude();

// Ecliptic longitude at azimuth==0
    double                  longitudeAtNorthMeridian();

// Amount the sidereal time coordinate system has rotated around since the autumnal equinox
    double                  vernalEquinoxAngle();

// Equation of Time for today expressed as an angle
    double                  EOT();
// Equation of Time for today expressed as a time interval
    ESTimeInterval          EOTSeconds();

 // Specific times for this day where twilight occurs
    ESTimeInterval          sunTimeForDayForAltitudeKind(CacheSlotIndex altitudeKind);
    ESTimeInterval          sunSpecial24HourIndicatorAngleForAltitudeKind(CacheSlotIndex altitudeKind,
                                                                          bool           *validReturn);

// These convenience methods return a temporary watch, from which standard
// ESWatchTime methods can be used to extract the proper hour angle, etc.
// They are based on the calculation methods above and represent the same times as those methods.
    ESWatchTime             *watchTimeForInterval(ESTimeInterval dateInterval);

    ESWatchTime             *watchTimeWithSunriseForDay();
    ESWatchTime             *watchTimeWithSunsetForDay();
    ESWatchTime             *watchTimeWithSuntransitForDay();
    ESWatchTime             *watchTimeWithNextSunrise();
    ESWatchTime             *watchTimeWithNextSunset();
    ESWatchTime             *watchTimeWithPrevSunrise();
    ESWatchTime             *watchTimeWithPrevSunset();
    ESWatchTime             *watchTimeWithMoonriseForDay();
    ESWatchTime             *watchTimeWithMoonsetForDay();
    ESWatchTime             *watchTimeWithMoontransitForDay();
    ESWatchTime             *watchTimeWithNextMoonrise();
    ESWatchTime             *watchTimeWithNextMoonset();
    ESWatchTime             *watchTimeWithPrevMoonrise();
    ESWatchTime             *watchTimeWithPrevMoonset();
    ESWatchTime             *watchTimeWithPrevPlanetrise(int planetNumber);
    ESWatchTime             *watchTimeWithNextPlanetrise(int planetNumber);
    ESWatchTime             *watchTimeWithPrevPlanetset(int planetNumber);
    ESWatchTime             *watchTimeWithNextPlanetset(int planetNumber);
    ESWatchTime             *watchTimeWithClosestNewMoon();
    ESWatchTime             *watchTimeWithClosestFullMoon();
    ESWatchTime             *watchTimeWithClosestFirstQuarter();
    ESWatchTime             *watchTimeWithClosestThirdQuarter();
    ESWatchTime             *watchTimeWithPlanetriseForDay(int planetNumber);
    ESWatchTime             *watchTimeWithPlanetsetForDay(int planetNumber);
    ESWatchTime             *watchTimeWithPlanettransitForDay(int planetNumber);
#if 0
    ESWatchTime             *watchTimeWithGoldenHourBegin();
    ESWatchTime             *watchTimeWithCivilTwilightEnd();
    ESWatchTime             *watchTimeWithNauticalTwilightEnd();
    ESWatchTime             *watchTimeWithAstronomicalTwilightEnd();
    ESWatchTime             *watchTimeWithGoldenHourEnd();
    ESWatchTime             *watchTimeWithCivilTwilightBegin();
    ESWatchTime             *watchTimeWithNauticalTwilightBegin();
    ESWatchTime             *watchTimeWithAstronomicalTwilightBegin();
#endif

    double                  observerLatitude() const { return _observerLatitude; }
    double                  observerLongitude() const { return _observerLongitude; }

  private:
                            ESAstronomyManager();  // Use other ctor

    void                    printDateD(ESTimeInterval dt,
                                       const char     *withDescription);

    ESTimeInterval          nextPrevRiseSetInternalWithFudgeInterval(double            fudgeSeconds,
                                                                     CalculationMethod calculationMethod,
                                                                     double            overrideAltitudeDesired,
                                                                     int               planetNumber,
                                                                     bool              riseNotSet,
                                                                     bool              isNext,
                                                                     ESTimeInterval    lookahead,
                                                                     ESTimeInterval    *riseSetOrTransit);
    ESTimeInterval          nextPrevPlanetRiseSetForPlanet(int  planetNumber,
                                                           bool riseNotSet,
                                                           bool nextNotPrev);
    ESTimeInterval          nextOrMidnightForDateInterval(ESTimeInterval opDate);
    ESTimeInterval          planetRiseSetForDay(int  planetNumber,
                                                bool riseNotSet);
    ESTimeInterval          nextPrevPlanettransit(int  planetNumber,
                                                  bool nextNotPrev,
                                                  bool wantHighTransit=true);
    ESTimeInterval          closestQuarterAngle(double quarterAngle);
    ESTimeInterval          nextQuarterAngle(double quarterAngle);
    double                  sunRAJ2000 ();
    double                  sunDeclJ2000 ();
    double                  moonRAJ2000 ();
    double                  moonDeclJ2000 ();
    void                    calculateHighestEcliptic ();
    ESTimeInterval          meridianTimeForSeason ();
    ESTimeInterval          moonMeridianTimeForSeason ();
    double                  planetMeridianTimeForSeason(int planetNumber);
    double                  angle24HourForDateInterval(ESTimeInterval dateInterval,
                                                       ESTimeBaseKind timeBaseKind);
    double                  planetAge(int    planetNumber,
                                      double *moonAge,
                                      double *phase);
    void                    runTests();

#ifndef NDEBUG
    void                    testPolarEdge();
#endif

    // Input parameters
    ESTimeEnvironment       *_environment;
    ESWatchTime             *_watchTime;
    ESLocation              *_location;

    // Internal data -- temporary only while calculating
    ESTimeInterval          _calculationDateInterval;
    ESTimeZone              *_estz;
    double                  _observerLatitude;
    double                  _observerLongitude;
    bool                    _locationValid;
    ECAstroCache            *_currentCache;
    ECAstroCachePool        *_astroCachePool;
    ESWatchTime             *_scratchWatchTime;
    bool                    _inActionButton;  // in the action button for *this* astro mgr

    static double           _zodiacCenters[12];
    static double           _zodiacEdges[13];
};

extern double
cachelessPlanetAlt(int            planetNumber,
		   ESTimeInterval calculationDateInterval,
		   double         observerLatitude,
		   double         observerLongitude);
extern double
cachelessSunDecl(double dateInterval);
extern double
EOTSecondsForDateInterval(double dateInterval);
extern void
EC_printAngle(double     angle,
	      const char *description);

#endif // _ESASTRONOMY_HPP_
