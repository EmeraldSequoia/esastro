//
//  ECAstronomy.cpp
//  Emerald Chronometer
//
//  Created by Steve Pucci on May 11 2008
//  Converted to C++ in May-June 2011 by Steve Pucci
//  Copyright Emerald Sequoia 2008-2011. All rights reserved.
//

#include <math.h>

#include "ESAstroConstants.hpp"
#include "ESAstronomy.hpp"
#include "ESWatchTime.hpp"
#include "ESTimeEnvironment.hpp"
#include "ESLocation.hpp"
#include "../Willmann-Bell/ESWillmannBell.hpp"
#include "ESErrorReporter.hpp"
#include "ESUtil.hpp"
#include "ESUserString.hpp"

#define kECDaysInEpochCentury (36525.0)
#define kEC1990Epoch (-347241600.0)  // 12/31/1989 GMT - 1/1/2001 GMT, calculated as 24 * 3600 * (365 * 8 + 366 * 3 + 1) /*1992, 1996, 2000*/ and verified with NS-Calendar
#define kECSunAngularDiameterAtR0 (0.533128*M_PI/180)
#define kECJulianDateOf1990Epoch (2447891.5)
#define kECJulianDateOf2000Epoch (2451545.0)
#define kECJulianDaysPerCentury (36525.0)
#define kECSecondsInTropicalYear (3600.0 * 24 * 365.2422) // NOTE: Average at J2000 (approx), will be less in the future, more in the past
#define kECMoonOrbitSemimajorAxis (384401) // km
#define kECMoonAngularSizeAtA (.5181*M_PI/180)
#define kECMoonParallaxAtA (0.9507*M_PI/180)
#define kECT0k1 (100.46061837 * M_PI/180) // Source: MeeusR2
#define kECT0k2 (36000.770053608 * M_PI/180)
#define kECT0k3 (1/38710000.0 * M_PI/180)
#define kECUTUnitsPerGSTUnit (1/1.00273790935)
#define kECRefractionAtHorizonX (34.0 / 60 * (M_PI / 180))  // 34 arcminutes
#define kECLunarCycleInSeconds (29.530589 * 3600 * 24)
#define kECcosMoonEquatorEclipticAngle 0.999637670406006
#define kECsinMoonEquatorEclipticAngle 0.026917056028711
#define kECSunDistanceR0 (1.495985E8 / kECAUInKilometers)  // semi-major axis
#define kECLimitingAzimuthLatitude (89.9999 * M_PI / 180)  // when the latitude exceeds this (in absolute value), limit it to provide more information about azimuth at the poles

#include "ESAstronomyCache.hpp"  // Needs to follow #defines above

static bool printingEnabled = false;

#define kECAlwaysBelowHorizon nan("1")
#define kECAlwaysAboveHorizon nan("2")

static void
printAngle(double      angle,
           const char *description) {
    if (!printingEnabled) {
        return;
    }
    if (isnan(angle)) {
        if (ESUtil::nansEqual(angle, kECAlwaysAboveHorizon)) {
            ESErrorReporter::logInfo(description, "            NAN (kECAlwaysAboveHorizon)");
        } else if (ESUtil::nansEqual(angle, kECAlwaysBelowHorizon)) {
            ESErrorReporter::logInfo(description, "            NAN (kECAlwaysBelowHorizon)");
        } else {
            ESErrorReporter::logInfo(description, "            NAN (\"\")");
        }
        return;
    }
    int sign = angle < 0 ? -1 : 1;
    double absAngle = fabs(angle);
    int degrees = sign * (int)(((long long int)floor(absAngle * 180/M_PI)));
    int arcMinutes = (int)(((long long int)floor(absAngle * 180/M_PI * 60)) % 60);
    int arcSeconds = (int)(((long long int)floor(absAngle * 180/M_PI * 3600)) % 60);
    int arcSecondHundredths = (int)(((long long int)floor(absAngle * 180/M_PI * 360000)) % 100);
    int hours = sign * (int)(((long long int)floor(absAngle * 12/M_PI)));
    int          minutes  = (int)(((long long int)floor(absAngle * 12/M_PI * 60)) % 60);
    int minuteThousandths = (int)(((long long int)floor(absAngle * 12/M_PI * 60000)) % 1000);
    int          seconds = (int)(((long long int)floor(absAngle * 12/M_PI * 3600)) % 60);
    int secondHundredths = (int)(((long long int)floor(absAngle * 12/M_PI * 360000)) % 100);
    ESErrorReporter::logInfo(description, "%32.24fr %16.8fd %5do%02d'%02d.%02d\" %16.8fh %5dh%02dm%02d.%02ds %5dh%02d.%03dm",
           angle,
           angle * 180 / M_PI,
           degrees,
           arcMinutes,
           arcSeconds,
           arcSecondHundredths,
           angle * 12 / M_PI,
           hours,
           minutes,
           seconds,
           secondHundredths,
           hours,
           minutes,
           minuteThousandths);
}

void EC_printAngle(double     angle,
                   const char *description) {
    bool savePrintingEnabled = printingEnabled;
    printingEnabled = true;
    printAngle(angle, description);
    printingEnabled = savePrintingEnabled;
}

static bool
timesAreOnSameDay(ESTimeInterval dt1,
                  ESTimeInterval dt2,
                  ESTimeZone     *estz) {
    ESDateComponents cs1;
    ESCalendar_localDateComponentsFromTimeInterval(dt1, estz, &cs1);
    ESDateComponents cs2;
    ESCalendar_localDateComponentsFromTimeInterval(dt2, estz, &cs2);
    return cs1.era == cs2.era && cs1.year == cs2.year && cs1.month == cs2.month && cs1.day == cs2.day;
}

static void
printDouble(double     value,
            const char *description) {
    if (!printingEnabled) {
        return;
    }
    printf("%16.8f        %s\n", value, description);
}

#undef ASTRO_DEBUG_PRINT
#ifdef ASTRO_DEBUG_PRINT

static void
printDateD(ESTimeInterval dt,
           const char     *description) {
    if (!printingEnabled) {
        return;
    }
    ESDateComponents cs;
    ESCalendar_UTCDateComponentsFromTimeInterval(dt, &cs);
    int second = floor(cs.seconds);
    double fractionalSeconds = cs.seconds - second;
    int microseconds = round(fractionalSeconds * 1000000);
    printf("%s %04d/%02d/%02d %02d:%02d:%02d.%06d UT %s\n",
           cs.era ? " CE" : "BCE", cs.year, cs.month, cs.day, cs.hour, cs.minute, second, microseconds,
           description);
}
#endif

static void
printDateDWithTimeZone(ESTimeInterval dt,
                       ESTimeZone     *estz,
                       const char     *description) {
    if (!printingEnabled) {
        return;
    }
    ESDateComponents cs;
    ESCalendar_localDateComponentsFromTimeInterval(dt, estz, &cs);
    int second = floor(cs.seconds);
    double fractionalSeconds = cs.seconds - second;
    int microseconds = round(fractionalSeconds * 1000000);
    printf("%s %04d/%02d/%02d %02d:%02d:%02d.%06d LT %s\n",
           cs.era ? " CE" : "BCE", cs.year, cs.month, cs.day, cs.hour, cs.minute, second, microseconds,
           description);
}

#ifdef ASTRO_DEBUG_PRINT
#define PRINT_DOUBLE(D) printDouble(D, #D)
#define PRINT_ANGLE(A) printAngle(A, #A)
#define PRINT_DATE(D) printDateD(D, #D)
#define PRINT_DATE_ACT_LT(D) ([self printDate:D withDescription:#D])
#define PRINT_DATE_VIRT_LT(D) printDateDWithTimeZone(D, estz, #D)
#define PRINT_STRING(S) { if (printingEnabled) printf(S); } 
#define PRINT_STRING1(S, A1) { if (printingEnabled) printf(S, A1); }
#else
#define PRINT_DOUBLE(D)
#define PRINT_ANGLE(A)
#define PRINT_DATE(D)
#define PRINT_DATE_ACT_LT(D)
#define PRINT_DATE_VIRT_LT(D)
#define PRINT_STRING(S)
#define PRINT_STRING1(S, A1)
#endif

static double deltaTTable[] = {  // From 1620 thru 2004 on alternate years (1620, 1622, 1624, etc)  From Meeus 2nd ed, p 79
    121/*1620*/, 112/*1622*/, 103/*1624*/, 95/*1626*/, 88/*1628*/,  82/*1630*/, 77/*1632*/, 72/*1634*/, 68/*1636*/, 63/*1638*/,  60/*1640*/, 56/*1642*/, 53/*1644*/, 51/*1646*/, 48/*1648*/,  46/*1650*/, 44/*1652*/, 42/*1654*/, 40/*1656*/, 38/*1658*/,
    35/*1660*/, 33/*1662*/, 31/*1664*/, 29/*1666*/, 26/*1668*/,  24/*1670*/, 22/*1672*/, 20/*1674*/, 18/*1676*/, 16/*1678*/,  14/*1680*/, 12/*1682*/, 11/*1684*/, 10/*1686*/, 9/*1688*/,  8/*1690*/, 7/*1692*/, 7/*1694*/, 7/*1696*/, 7/*1698*/,
    7/*1700*/, 7/*1702*/, 8/*1704*/, 8/*1706*/, 9/*1708*/,  9/*1710*/, 9/*1712*/, 9/*1714*/, 9/*1716*/, 10/*1718*/,  10/*1720*/, 10/*1722*/, 10/*1724*/, 10/*1726*/, 10/*1728*/,  10/*1730*/, 10/*1732*/, 11/*1734*/, 11/*1736*/, 11/*1738*/,
    11/*1740*/, 11/*1742*/, 12/*1744*/, 12/*1746*/, 12/*1748*/,  12/*1750*/, 13/*1752*/, 13/*1754*/, 13/*1756*/, 14/*1758*/,  14/*1760*/, 14/*1762*/, 14/*1764*/, 15/*1766*/, 15/*1768*/,  15/*1770*/, 15/*1772*/, 15/*1774*/, 16/*1776*/, 16/*1778*/,
    16/*1780*/, 16/*1782*/, 16/*1784*/, 16/*1786*/, 16/*1788*/,  16/*1790*/, 15/*1792*/, 15/*1794*/, 14/*1796*/, 13/*1798*/,  13.1/*1800*/, 12.5/*1802*/, 12.2/*1804*/, 12/*1806*/, 12/*1808*/,  12/*1810*/, 12/*1812*/, 12/*1814*/, 12/*1816*/, 11.9/*1818*/,
    11.6/*1820*/, 11/*1822*/, 10.2/*1824*/, 9.2/*1826*/, 8.2/*1828*/,  7.1/*1830*/, 6.2/*1832*/, 5.6/*1834*/, 5.4/*1836*/, 5.3/*1838*/,  5.4/*1840*/, 5.6/*1842*/, 5.9/*1844*/, 6.2/*1846*/, 6.5/*1848*/,  6.8/*1850*/, 7.1/*1852*/, 7.3/*1854*/, 7.5/*1856*/, 7.6/*1858*/,
    7.7/*1860*/, 7.3/*1862*/, 6.2/*1864*/, 5.2/*1866*/, 2.7/*1868*/,  1.4/*1870*/, -1.2/*1872*/, -2.8/*1874*/, -3.8/*1876*/, -4.8/*1878*/,  -5.5/*1880*/, -5.3/*1882*/, -5.6/*1884*/, -5.7/*1886*/, -5.9/*1888*/,  -6.0/*1890*/, -6.3/*1892*/, -6.5/*1894*/, -6.2/*1896*/, -4.7/*1898*/,
    -2.8/*1900*/, -0.1/*1902*/, 2.6/*1904*/, 5.3/*1906*/, 7.7/*1908*/,  10.4/*1910*/, 13.3/*1912*/, 16.0/*1914*/, 18.2/*1916*/, 20.2/*1918*/,  21.1/*1920*/, 22.4/*1922*/, 23.5/*1924*/, 23.8/*1926*/, 24.3/*1928*/,  24/*1930*/, 23.9/*1932*/, 23.9/*1934*/, 23.7/*1936*/, 24/*1938*/,
    24.3/*1940*/, 25.3/*1942*/, 26.2/*1944*/, 27.3/*1946*/, 28.2/*1948*/,  29.1/*1950*/, 30/*1952*/, 30.7/*1954*/, 31.4/*1956*/, 32.2/*1958*/,  33.1/*1960*/, 34/*1962*/, 35/*1964*/, 36.5/*1966*/, 38.3/*1968*/,  40.2/*1970*/, 42.2/*1972*/, 44.5/*1974*/, 46.5/*1976*/, 48.5/*1978*/,
    50.5/*1980*/, 52.2/*1982*/, 53.8/*1984*/, 54.9/*1986*/, 55.8/*1988*/,  56.9/*1990*/, 58.3/*1992*/, 60/*1994*/, 61.6/*1996*/, 63/*1998*/,  63.8/*2000*/, 64.3/*2002*/, 64.6/*2004*/
};

// From Meeus, p78
double ECMeeusDeltaT(double yearValue) {  // year value as in 2008.5 for July 1 (approx)
    double deltaT;
    if (yearValue < 948) {
        double t = (yearValue - 2000)/100;
        deltaT = 2177 + 497*t + 44.1*t*t;
    } else if (yearValue < 1620) {
        double t = (yearValue - 2000)/100;
        deltaT = 102 + 102*t + 25.3*t*t;
    } else if (yearValue >= 2100) {
        double t = (yearValue - 2000)/100;
        deltaT = 102 + 102*t + 25.3*t*t;
    } else if (yearValue > 2004) {
        double t = (yearValue - 2000)/100;
        deltaT = 102 + 102*t + 25.3*t*t + 0.37*(yearValue - 2100);
    } else if (yearValue == 2004) {
        deltaT = deltaTTable[(2004-1620)/2];
    } else {
        double realIndex = (yearValue-1620)/2;
        int priorIndex = floor(realIndex);
        int nextIndex = priorIndex + 1;
        double interpolation = (realIndex - priorIndex);
        deltaT = deltaTTable[priorIndex] + (deltaTTable[nextIndex] - deltaTTable[priorIndex])*interpolation;
    }
    return deltaT;
}

static double espenakDeltaT(double yearValue) {  // year value as in 2008.5 for July 1
    if (yearValue >= 2005 && yearValue <= 2050) {  // common case first
        double t = (yearValue - 2000);
        double t2 = t * t;
        return 62.92 + 0.32217*t + 0.005589*t2;
    } else if (yearValue < -500 || yearValue >= 2150) {  // really only claimed to be valid back to -1999, so our use of it prior to then is questionable
        double u = (yearValue - 1820) / 100;
        return -20 + 32 * u*u;
    } else if (yearValue < 500) {
        double u = yearValue / 100;
        double u2 = u * u;
        double u3 = u2 * u;
        double u4 = u2 * u2;
        double u5 = u3 * u2;
        double u6 = u3 * u3;
        return 10583.6 - 1014.41*u + 33.78311*u2 - 5.952053*u3
            - 0.1798452*u4 + 0.022174192*u5 + 0.0090316521*u6;
    } else if (yearValue < 1600) {
        double u = (yearValue-1000) / 100;
        double u2 = u * u;
        double u3 = u2 * u;
        double u4 = u2 * u2;
        double u5 = u3 * u2;
        double u6 = u3 * u3;
        return 1574.2 - 556.01*u + 71.23472*u2 + 0.319781*u3
            - 0.8503463*u4 - 0.005050998*u5 + 0.0083572073*u6;
    } else if (yearValue < 1700) {
        double t = (yearValue - 1600);
        double t2 = t * t;
        double t3 = t2 * t;
        return 120 - 0.9808*t - 0.01532*t2 + t3/7129;
    } else if (yearValue < 1800) {
        double t = (yearValue - 1700);
        double t2 = t * t;
        double t3 = t2 * t;
        double t4 = t2 * t2;
        return 8.83 + 0.1603*t - 0.0059285*t2 + 0.00013336*t3 - t4/1174000;
    } else if (yearValue < 1860) {
        double t = (yearValue - 1800);
        double t2 = t * t;
        double t3 = t2 * t;
        double t4 = t2 * t2;
        double t5 = t3 * t2;
        double t6 = t3 * t3;
        double t7 = t4 * t3;
        return 13.72 - 0.332447*t + 0.0068612*t2 + 0.0041116*t3 - 0.00037436*t4 
            + 0.0000121272*t5 - 0.0000001699*t6 + 0.000000000875*t7;
    } else if (yearValue < 1900) {
        double t = (yearValue - 1860);
        double t2 = t * t;
        double t3 = t2 * t;
        double t4 = t2 * t2;
        double t5 = t3 * t2;
        return 7.62 + 0.5737*t - 0.251754*t2 + 0.01680668*t3
            -0.0004473624*t4 + t5/233174;
    } else if (yearValue < 1920) {
        double t = (yearValue - 1900);
        double t2 = t * t;
        double t3 = t2 * t;
        double t4 = t2 * t2;
        return -2.79 + 1.494119*t - 0.0598939*t2 + 0.0061966*t3 - 0.000197*t4;
    } else if (yearValue < 1941) {
        double t = (yearValue - 1920);
        double t2 = t * t;
        double t3 = t2 * t;
        return 21.20 + 0.84493*t - 0.076100*t2 + 0.0020936*t3;
    } else if (yearValue < 1961) {
        double t = (yearValue - 1950);
        double t2 = t * t;
        double t3 = t2 * t;
        return 29.07 + 0.407*t - t2/233 + t3/2547;
    } else if (yearValue < 1986) {
        double t = (yearValue - 1975);
        double t2 = t * t;
        double t3 = t2 * t;
        return 45.45 + 1.067*t - t2/260 - t3/718;
    } else if (yearValue < 2005) {
        double t = (yearValue - 2000);
        double t2 = t * t;
        double t3 = t2 * t;
        double t4 = t2 * t2;
        double t5 = t3 * t2;
        return 63.86 + 0.3345*t - 0.060374*t2 + 0.0017275*t3 + 0.000651814*t4 
            + 0.00002373599*t5;
    } else if (yearValue < 2150) {
        ESAssert(yearValue > 2050);  // should have caught it in first case
        double t1 = (yearValue-1820)/100;
        return -20 + 32 * t1*t1 - 0.5628 * (2150 - yearValue);
#ifndef NDEBUG
    } else {
        ESAssert(false);  // should have caught it in second case
#endif
    }
    return 0;
}

static bool useMeeusDeltaT = false;

static double convertUTtoET(double ut,
                            double yearValue) {
    if (useMeeusDeltaT) {
        return ut + ECMeeusDeltaT(yearValue);
    } else {
        return ut + espenakDeltaT(yearValue);
    }
}

#ifndef NDEBUG
#if 0  // only call commented out
static void testConversion() {
    //for (int year = 900; year < 2110; year += 2) {
    for (int year = -500; year < 2110; year += 50) {
        useMeeusDeltaT = true;
        double newValue = convertUTtoET(0, year);
        printf("\n%04d %10.3f Meeus\n", year, newValue);
        useMeeusDeltaT = false;
        newValue = convertUTtoET(0, year);
        printf("%04d %10.3f Espenak\n", year, newValue);
    }
}
#endif  // if 0
#endif  // NDEBUG

double
julianDateForDate(ESTimeInterval dateInterval) {
    double secondsSince1990Epoch = dateInterval - kEC1990Epoch;
    return kECJulianDateOf1990Epoch + (secondsSince1990Epoch / (24 * 3600));
}

static ESTimeInterval
priorUTMidnightForDateRaw(ESTimeInterval dateInterval) {
    ESDateComponents cs;
    ESCalendar_UTCDateComponentsFromTimeInterval(dateInterval, &cs);
    cs.hour = 0;
    cs.minute = 0;
    cs.seconds = 0;
    return ESCalendar_timeIntervalFromUTCDateComponents(&cs);
}

static ESTimeInterval
priorUTMidnightForDateInterval(ESTimeInterval calculationDateInterval,
                               ECAstroCache   *_currentCache) {
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - calculationDateInterval) <= ASTRO_SLOP);
    double val;
    if (_currentCache && _currentCache->cacheSlotValidFlag[priorUTMidnightSlotIndex] == _currentCache->currentFlag) {
        val = _currentCache->cacheSlots[priorUTMidnightSlotIndex];
    } else {
        static double lastCalculatedMidnight = 0;
        if (calculationDateInterval > lastCalculatedMidnight && calculationDateInterval < lastCalculatedMidnight + 24 * 3600) {
            val = lastCalculatedMidnight;
        } else {
            val = priorUTMidnightForDateRaw(calculationDateInterval);
            lastCalculatedMidnight = val;
        }
        if (_currentCache) {
            _currentCache->cacheSlots[priorUTMidnightSlotIndex] = val;
            _currentCache->cacheSlotValidFlag[priorUTMidnightSlotIndex] = _currentCache->currentFlag;
        }
    }
    return val;
}

static ESTimeInterval
noonUTForDateInterval(ESTimeInterval dateInterval) {
    ESDateComponents cs;
    ESCalendar_UTCDateComponentsFromTimeInterval(dateInterval, &cs);
    cs.hour = 12;
    cs.minute = 0;
    cs.seconds = 0;
    return ESCalendar_timeIntervalFromUTCDateComponents(&cs);
}

static double positionAngle(double sunRightAscension,
                            double sunDeclination,
                            double objRightAscension,
                            double objDeclination) {
    return atan2(cos(sunDeclination) * sin(sunRightAscension - objRightAscension),
                 cos(objDeclination) * sin(sunDeclination) - sin(objDeclination) * cos(sunDeclination) * cos(sunRightAscension - objRightAscension));
}

static double greatCircleCourse(double latitude1,
                                double longitude1,
                                double latitude2,
                                double longitude2) {
    return atan2(sin(longitude1 - longitude2) * cos(latitude2),
                 cos(latitude1)*sin(latitude2)-sin(latitude1)*cos(latitude2)*cos(longitude1-longitude2));
}

static double northAngleForObject(double altitude,
                                  double azimuth,
                                  double observerLatitude) {
    // this is the great circle course from the object to the celestial north pole
    // expressed in lat/long coordinates for a sphere whose north is at the zenith
    // and where the celestial north pole is at latitude = observerLatitude and longitude = 0
    // and the object is at latitude=altitude and longitude=azimuth
    return greatCircleCourse(altitude, azimuth, observerLatitude, 0);
}

// Returns TDT/ET Julian Centuries since J2000.0 given a UT date
static double
julianCenturiesSince2000EpochForDateInterval(ESTimeInterval dateInterval,
                                             double         *deltaT,
                                             ECAstroCache   *_currentCache) {
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - dateInterval) <= ASTRO_SLOP);
    double julianCenturiesSince2000Epoch;
    if (_currentCache && _currentCache->cacheSlotValidFlag[tdtCenturiesSlotIndex] == _currentCache->currentFlag) {  // we use one slot index valid value to cover all values
        julianCenturiesSince2000Epoch = _currentCache->cacheSlots[tdtCenturiesSlotIndex];
        if (deltaT) {
            *deltaT = _currentCache->cacheSlots[tdtCenturiesDeltaTSlotIndex];
        }
    } else {
        double utSeconds = dateInterval;
        ESTimeInterval firstOfThisYearInterval;
        static ESTimeInterval lastCalculatedFirstInterval = 0;
        static int lastYearValue = 0;
        if (utSeconds > lastCalculatedFirstInterval && utSeconds < lastCalculatedFirstInterval + (24 * 3600 * 330)) {
            firstOfThisYearInterval = lastCalculatedFirstInterval;
        } else {
            ESDateComponents cs;
            ESCalendar_UTCDateComponentsFromTimeInterval(dateInterval, &cs);
            cs.month = 1;
            cs.day = 1;
            cs.hour = 0;
            cs.minute = 0;
            cs.seconds = 0;
            firstOfThisYearInterval = ESCalendar_timeIntervalFromUTCDateComponents(&cs);
            lastCalculatedFirstInterval = firstOfThisYearInterval;
            lastYearValue = cs.era ? cs.year : 1 - cs.year;
        }
        double yearValue = lastYearValue + (utSeconds - firstOfThisYearInterval)/(365.25 * 24 * 3600);
        PRINT_DOUBLE(yearValue);
        double etSeconds = convertUTtoET(utSeconds, yearValue);
        if (deltaT) {
            *deltaT = etSeconds - utSeconds;
        }
        double julianDaysSince2000Epoch = julianDateForDate(etSeconds) - kECJulianDateOf2000Epoch;
        julianCenturiesSince2000Epoch = julianDaysSince2000Epoch / kECJulianDaysPerCentury;
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[tdtCenturiesSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[tdtHundredCenturiesSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[tdtCenturiesSlotIndex] = julianCenturiesSince2000Epoch;
            _currentCache->cacheSlots[tdtCenturiesDeltaTSlotIndex] = etSeconds - utSeconds;
            _currentCache->cacheSlots[tdtHundredCenturiesSlotIndex] = julianCenturiesSince2000Epoch / 100;
        }
    }
    return julianCenturiesSince2000Epoch;
}

static double sunEclipticLongitudeForDate(ESTimeInterval dateInterval,
                                          ECAstroCache   *_currentCache) {
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - dateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[sunEclipticLongitudeSlotIndex] == _currentCache->currentFlag) {
        return _currentCache->cacheSlots[sunEclipticLongitudeSlotIndex];
    }
    double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(dateInterval, NULL, _currentCache);
    double eclipticLongitude = WB_sunLongitudeApparent(julianCenturiesSince2000Epoch/100, _currentCache);
    //printAngle(eclipticLongitude, "EL Willmann-Bell");
    if (_currentCache) {
        _currentCache->cacheSlotValidFlag[sunEclipticLongitudeSlotIndex] = _currentCache->currentFlag;
        _currentCache->cacheSlots[sunEclipticLongitudeSlotIndex] = eclipticLongitude;
    }
    return eclipticLongitude;
}

#if 0  // Unused
static double meanObliquityOfEclipticForDate(ESTimeInterval dateInterval) {
    double julianDaysSince2000Epoch = julianDateForDate(dateInterval) - kECJulianDateOf2000Epoch;
    double julianCenturiesSince2000Epoch = julianDaysSince2000Epoch / kECJulianDaysPerCentury;
    double obliquity =
        (23.439292 - (46.815 * julianCenturiesSince2000Epoch +
                      0.0006 * (julianCenturiesSince2000Epoch * julianCenturiesSince2000Epoch) +
                      0.00181 * (julianCenturiesSince2000Epoch * julianCenturiesSince2000Epoch * julianCenturiesSince2000Epoch)) / 3600.0) * M_PI / 180.0;
    PRINT_ANGLE(obliquity);
    return obliquity;
}
#endif

// Method taking obliquity directly (for testing purposes, we break this out)
static void raAndDeclO(double eclipticLatitude,
                       double eclipticLongitude,
                       double obliquity,
                       double *rightAscensionReturn,
                       double *declinationReturn) {
    double sinDelta = sin(eclipticLatitude)*cos(obliquity) + cos(eclipticLatitude)*sin(obliquity)*sin(eclipticLongitude);
    PRINT_DOUBLE(sinDelta);
    *declinationReturn = asin(sinDelta);
    PRINT_ANGLE(*declinationReturn);
    double y = sin(eclipticLongitude)*cos(obliquity)-tan(eclipticLatitude)*sin(obliquity);
    PRINT_DOUBLE(y);
    double x = cos(eclipticLongitude);
    PRINT_DOUBLE(x);
    *rightAscensionReturn = atan2(y, x);
    PRINT_ANGLE(*rightAscensionReturn);
}

// raAndDecl with eclipticLatitude == 0
static void sunRAandDecl(ESTimeInterval dateInterval,
                         double         *rightAscensionReturn,
                         double         *declinationReturn,
                         ECAstroCache   *_currentCache) {
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - dateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[sunRASlotIndex] == _currentCache->currentFlag) {  // both slotValid flags are always set at the same time
        *rightAscensionReturn = _currentCache->cacheSlots[sunRASlotIndex];
        *declinationReturn = _currentCache->cacheSlots[sunDeclSlotIndex];
        return;
    }
    double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(dateInterval, NULL, _currentCache);
    double sunLongitude;
    WB_sunRAAndDecl(julianCenturiesSince2000Epoch/100, rightAscensionReturn, declinationReturn, &sunLongitude, _currentCache);
    if (_currentCache) {
        _currentCache->cacheSlotValidFlag[sunRASlotIndex] = _currentCache->currentFlag;
        _currentCache->cacheSlotValidFlag[sunDeclSlotIndex] = _currentCache->currentFlag;
        _currentCache->cacheSlots[sunRASlotIndex] = *rightAscensionReturn;
        _currentCache->cacheSlots[sunDeclSlotIndex] = *declinationReturn;
    }
}

// From Meeus, chs 11 & 40
static void topocentricParallax(double ra,  // radians
                                double decl,// radians
                                double H,   // hour angle, radians
                                double distInAU, // AU
                                double observerLatitude,  // radians
                                double observerAltitude,  // m
                                double *Hprime,
                                double *declPrime) {
    static const double bOverA = 0.99664719;
    double u = atan(bOverA * tan(observerLatitude));
    double delta = observerAltitude/6378140;
    double rhoSinPhiPrime = bOverA * sin(u) + delta * sin(observerLatitude);
    double rhoCosPhiPrime = cos(u) + delta * cos(observerLatitude);
    double sinPi = sin(8.794/3600*M_PI/180)/distInAU;  // equatorial horizontal parallax
    double A = cos(decl) * sin(H);
    double B = cos(decl) * cos(H) - rhoCosPhiPrime * sinPi;
    double C = sin(decl) - rhoSinPhiPrime * sinPi;
    double q = sqrt(A*A + B*B + C*C);
    *Hprime = atan2(A,B);
    if (*Hprime < 0) {
        *Hprime += (M_PI * 2);
    }
    *declPrime = asin(C/q);
}

static void moonRAAndDecl(ESTimeInterval dateInterval,
                          double         *rightAscensionReturn,
                          double         *declinationReturn,
                          double         *moonEclipticLongitudeReturn,
                          ECAstroCache   *_currentCache) {
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - dateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonRASlotIndex] == _currentCache->currentFlag) {  // we use one slot index valid value to cover all values
        *rightAscensionReturn = _currentCache->cacheSlots[moonRASlotIndex];
        *declinationReturn = _currentCache->cacheSlots[moonDeclSlotIndex];
        *moonEclipticLongitudeReturn = _currentCache->cacheSlots[moonEclipticLongitudeSlotIndex];
        return;
    }

    double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(dateInterval, NULL, _currentCache);
    //printf("Date %s\n", [[[NSDate dateWithTimeIntervalSinceReferenceDate:dateInterval] description] UTF8String]);
    //printf("Julian date %.10f\n", julianDateForDate(dateInterval));

    double moonEclipticLatitude;
    WB_MoonRAAndDecl(julianCenturiesSince2000Epoch, rightAscensionReturn, declinationReturn, moonEclipticLongitudeReturn, &moonEclipticLatitude, _currentCache, ECWBFullPrecision);
    //printAngle(*moonEclipticLongitudeReturn, "eclip long WB");
    //printAngle(moonEclipticLatitude, "eclip lat WB");
    //printAngle(*rightAscensionReturn, "wb moon RA");
    //printAngle(*declinationReturn, "wb moon decl");
    if (_currentCache) {
        _currentCache->cacheSlotValidFlag[moonRASlotIndex] = _currentCache->currentFlag;
        _currentCache->cacheSlots[moonRASlotIndex] = *rightAscensionReturn;
        _currentCache->cacheSlots[moonDeclSlotIndex] = *declinationReturn;
        _currentCache->cacheSlots[moonEclipticLongitudeSlotIndex] = *moonEclipticLongitudeReturn;
    }
}

// Note spucci 2017-10-29:  GAAAAAAH!!
// moonAge is just a bad concept, but it's encoded into the terminator, so we're stuck with it until/unless the terminator gets rewritten.
// When I was writing that code back in 2008, I apparently was under the impression that what was important was how the Moon went around the Earth
// with respect to the Sun (the Moon-Earth-Sun angle, if you will).  But the phase is solely dependent on the Earth-Moon-Sun angle (in fact,
// that's how astronomical calculations are defined), since that's how we see the shadow on the Moon.  I got "lucky" in that the phase and the
// "age angle" are essentially complements (they, along with the Earth-Sun-Moon angle, are the three angles of a triangle, but the Earth-Sun-Moon
// angle is very very small).  So by assuming 180-phase=age, the calculations (mostly) worked out.  This weird convention is unfortunate, since
// we're trying to do planet phases now for Android in Terra II, and there age and phase are *not* complements, so the assumptions don't work out.

// THE "phase" RETURNED HERE IS WRONG.  I have no idea where I got "phase = (1 - cos(age))/2", but that's just malarkey.  I don't think we actually
// use the phase anywhere, so it's probably ok.  It should just be 180-age.  Not changing now.
static double
moonAge(ESTimeInterval dateInterval,
        double         *phase,   // NOT REALLY PHASE, JUST BOGUS NUMBER
        ECAstroCache   *_currentCache) {
    double age;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - dateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonAgeSlotIndex] == _currentCache->currentFlag) {
        age = _currentCache->cacheSlots[moonAgeSlotIndex];
        *phase = _currentCache->cacheSlots[moonPhaseSlotIndex];
    } else {
        double rightAscension;
        double declination;
        double moonEclipticLongitude;
        moonRAAndDecl(dateInterval, &rightAscension, &declination, &moonEclipticLongitude, _currentCache);
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(dateInterval, NULL, _currentCache);
        double sunEclipticLongitude = WB_sunLongitudeApparent(julianCenturiesSince2000Epoch/100, _currentCache);
        age = moonEclipticLongitude - sunEclipticLongitude;
        if (age < 0) {
            age += (M_PI * 2);
        }
        PRINT_ANGLE(age);
        *phase = (1 - cos(age))/2;  // HUH?
        PRINT_DOUBLE(*phase);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[moonAgeSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[moonAgeSlotIndex] = age;
            _currentCache->cacheSlots[moonPhaseSlotIndex] = *phase;
        }
    }
    return age;
}

/*static*/ double
ESAstronomyManager::moonDeltaEclipticLongitudeAtDateInterval(double dateInterval)
{
    double unused_phase;
    return moonAge(dateInterval, &unused_phase, NULL/*currentCache*/);
}


double
ESAstronomyManager::planetMoonAgeAngle(int planetNumber) {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double phase;
    double moonAge;
    planetAge(planetNumber, &moonAge/*planetMoonAgeReturn*/, &phase/*phaseReturn*/);  // Ignore return 'age'
    return moonAge;
}

static ESTimeInterval
stepRefineMoonAgeTargetForDate(ESTimeInterval dateInterval,
                               double         targetAge,
                               ECAstroCache   *_currentCache) {
    double phase;
    double age = moonAge(dateInterval, &phase, _currentCache);
    double deltaAge = targetAge - age;  // amount by which we must increase the calculation date to reach the target age
    if (deltaAge > M_PI) {
        deltaAge -= (M_PI * 2);
    } else if (deltaAge < -M_PI) {
        deltaAge += (M_PI * 2);
    }
    return (dateInterval + deltaAge/(M_PI * 2)*kECLunarCycleInSeconds);
}

static ESTimeInterval
refineMoonAgeTargetForDate(ESTimeInterval dateInterval,
                           double         targetAge,
                           ECAstroCachePool *cachePool) {
    ESTimeInterval tryDate = dateInterval;
    for (int i = 0; i < 5; i++) {
        ECAstroCache *priorCache = pushECAstroCacheWithSlopInPool(cachePool, &cachePool->refinementCache, tryDate, 0);
        ESTimeInterval newDate = stepRefineMoonAgeTargetForDate(tryDate, targetAge, cachePool->currentCache);
        popECAstroCacheToInPool(cachePool, priorCache);
        if (fabs(newDate - tryDate) < 0.1) {
            return newDate;
        }
        tryDate = newDate;
    }
    return tryDate;
}

static double convertLSTtoGST(double lst,
                              double observerLongitude,
                              int    *dayOffset) {
    double gst = lst - observerLongitude;
    if (gst < 0) {
        gst += (M_PI * 2);
        if (dayOffset) {
            *dayOffset = -1;
        }
    } else if (gst > (M_PI * 2)) {
        gst -= (M_PI * 2);
        if (dayOffset) {
            *dayOffset = 1;
        }
    } else {
        if (dayOffset) {
            *dayOffset = 0;
        }
    }
    return gst;
}

static double convertGSTtoLST(double gst,
                              double observerLongitude) {
    double lst = gst + observerLongitude;
    if (lst < 0) {
        lst += (M_PI * 2);
    } else if (lst > (M_PI * 2)) {
        lst -= (M_PI * 2);
    }
    return lst;
}

// P03; returns seconds
static double
convertUTToGSTP03x(double         centuriesSinceEpochTDT,
                   double         deltaTSeconds,
                   double         utSinceMidnightRadians,
                   ESTimeInterval priorUTMidnight) {
    double t = centuriesSinceEpochTDT;
    double tu = t - deltaTSeconds/(24*3600*kECJulianDaysPerCentury);
    double t2 = t*t;
    double t3 = t2*t;
    double t4 = t2*t2;
    double t5 = t3*t2;
    double gmst = 24110.5493771
        + 8640184.79447825*tu
        + 307.4771013*(t - tu)
        + 0.092772110*t2
        - 0.0000002926*t3
        - 0.00000199708*t4
        - 0.000000002454*t5;
    // convert from seconds to radians
    gmst *= M_PI / (12.0 * 3600);
    gmst += utSinceMidnightRadians;
    gmst = ESUtil::fmod(gmst, M_PI * 2);
    if (gmst < 0) {
        gmst += M_PI * 2;
    }
    return gmst;
}

static double
convertUTToGSTP03(double       calculationDate,
                  ECAstroCache *_currentCache) {
    double deltaTSeconds;
    double centuriesSinceEpochTDT = julianCenturiesSince2000EpochForDateInterval(calculationDate, &deltaTSeconds, _currentCache);
    double priorUTMidnightD = priorUTMidnightForDateInterval(calculationDate, _currentCache);
    double utRadiansSinceMidnight = (calculationDate - priorUTMidnightD) * M_PI/(12 * 3600);
    return convertUTToGSTP03x(centuriesSinceEpochTDT, deltaTSeconds, utRadiansSinceMidnight, priorUTMidnightD);
}

static double
convertGSTtoUT(double           gst,
               ESTimeInterval   priorUTMidnight,
               double           *ut2,
               ECAstroCachePool *cachePool) {
    PRINT_ANGLE(gst);
    PRINT_DATE(priorUTMidnight);

    ECAstroCache *priorCache = NULL;
    if (cachePool) {
        priorCache = pushECAstroCacheInPool(cachePool, &cachePool->midnightCache, priorUTMidnight);
    }
    double deltaTSeconds;
    double centuriesSinceEpochTDT = julianCenturiesSince2000EpochForDateInterval(priorUTMidnight, &deltaTSeconds, cachePool ? cachePool->currentCache : NULL);
    double T0 = convertUTToGSTP03x(centuriesSinceEpochTDT, deltaTSeconds, 0, priorUTMidnight);
    if (cachePool) {
        popECAstroCacheToInPool(cachePool, priorCache);
    }

    double ut = gst - T0;
    if (ut < 0) {
        ut += (M_PI * 2);
    } else if (ut > (M_PI * 2)) {
        ut -= (M_PI * 2);
    }
    ut *= kECUTUnitsPerGSTUnit;
    PRINT_ANGLE(ut);
    *ut2 = ut + (kECUTUnitsPerGSTUnit * (M_PI * 2));  // there might be two uts for this gst
    if (*ut2 > (M_PI * 2)) {
        *ut2 = -1;  // only one ut for this gst
    } else {
        PRINT_ANGLE(*ut2);
    }
    return ut;
}

static double
STDifferenceForDate(ESTimeInterval dateInterval,
                    ECAstroCache   *_currentCache) {
    double deltaTSeconds;
    double centuriesSinceEpochTDT = julianCenturiesSince2000EpochForDateInterval(dateInterval, &deltaTSeconds, _currentCache);
    double priorUTMidnightD = priorUTMidnightForDateInterval(dateInterval, _currentCache);
    double utRadiansSinceMidnight = (dateInterval - priorUTMidnightD) * M_PI/(12 * 3600);
    double gst = convertUTToGSTP03x(centuriesSinceEpochTDT, deltaTSeconds, utRadiansSinceMidnight, priorUTMidnightD);
    return gst - utRadiansSinceMidnight;
}

static ESTimeInterval
convertGSTtoUTclosest(double           gst,
                      ESTimeInterval   closestToThisDate,
                      ECAstroCachePool *cachePool) {
    PRINT_DATE(closestToThisDate);
    double priorUTMidnightD = priorUTMidnightForDateInterval(closestToThisDate, cachePool ? cachePool->currentCache : NULL);

    // Calculate answer for this UT date
    double ut0_2;
    double ut0 = convertGSTtoUT(gst, priorUTMidnightD, &ut0_2, cachePool);
    double utSecondsSinceMidnight = ut0 * (12 * 3600)/M_PI;

    // seconds since reference date for answer
    double utD = priorUTMidnightD + utSecondsSinceMidnight;

    // If answer is less than target date - 12h, then we want the next UT date
    if (utD < closestToThisDate - 12 * 3600.0 * kECUTUnitsPerGSTUnit) {
        // First see if there is a second, later UT date for the given GST:
        if (ut0_2 > 0) {
            PRINT_STRING("...using second UT for this GST\n");
            ut0 = ut0_2;
            utSecondsSinceMidnight = ut0 * (12 * 3600)/M_PI;
            utD = priorUTMidnightD + utSecondsSinceMidnight;
        } else {
            PRINT_STRING("...moving forward a day\n");
            priorUTMidnightD += 24 * 3600.0;
            ut0 = convertGSTtoUT(gst, priorUTMidnightD, &ut0_2, cachePool);
            utSecondsSinceMidnight = ut0 * (12 * 3600)/M_PI;
            utD = priorUTMidnightD + utSecondsSinceMidnight;
        }
    } else if (utD > closestToThisDate + 12 * 3600.0 * kECUTUnitsPerGSTUnit) {
        PRINT_STRING("...backing up a day\n");
        priorUTMidnightD -= 24 * 3600.0;
        ut0 = convertGSTtoUT(gst, priorUTMidnightD, &ut0_2, cachePool);
        if (ut0_2 > 0) { // we want the later of the two if there is one
            PRINT_STRING("...using later of two UTs for this GST\n");
            ut0 = ut0_2;
        }
        utSecondsSinceMidnight = ut0 * (12 * 3600)/M_PI;
        utD = priorUTMidnightD + utSecondsSinceMidnight;
    }
    return utD;
}

// From P03; includes both motion of the equator in the GCRS and the motion of the ecliptic
// in the ICRS.
static double
generalPrecessionSinceJ2000(double julianCenturiesSince2000Epoch) {
    double t = julianCenturiesSince2000Epoch;
    double t2 = t*t;
    double t3 = t*t2;
    double t4 = t2*t2;
    double t5 = t2*t3;

    double arcSeconds = 5028.796195*t + 1.1054348*t2 + 0.00007964*t3 - 0.000023857*t4 - 0.0000000383*t5;
    double radians = arcSeconds * M_PI/(3600 * 180);
//    char buf[128];
//    sprintf(buf, "%20.10f", julianCenturiesSince2000Epoch);
//    printAngle(radians, buf);
    return radians;
}

// From P03; includes both motion of the equator in the GCRS and the motion of the ecliptic
// in the ICRS.
static double
generalObliquity(double julianCenturiesSince2000Epoch) {
    double t = julianCenturiesSince2000Epoch;
    double t2 = t*t;
    double t3 = t*t2;
    double t4 = t2*t2;
    double t5 = t2*t3;
    double e0 = 84381.406;
    double eA = e0 - 46.836769*t - 0.0001831*t2 + 0.00200340*t3 - 0.000000576*t4 - 0.0000000434*t5;
    double radians = eA * M_PI/(3600 * 180);
    return radians;
}

// From P03; includes both motion of the equator in the GCRS and the motion of the ecliptic
// in the ICRS.
static void
generalPrecessionQuantities(double julianCenturiesSince2000Epoch,
                            double *pA,
                            double *eA,
                            double *chiA,
                            double *zetaA,
                            double *zA,
                            double *thetaA) {
    double t = julianCenturiesSince2000Epoch;
    double t2 = t*t;
    double t3 = t*t2;
    double t4 = t2*t2;
    double t5 = t2*t3;
    double arcSeconds = 5028.796195*t + 1.1054348*t2 + 0.00007964*t3 - 0.000023857*t4 - 0.0000000383*t5;
    *pA = arcSeconds * M_PI/(3600 * 180);
    double e0 = 84381.406;
    arcSeconds = e0 - 46.836769*t - 0.0001831*t2 + 0.00200340*t3 - 0.000000576*t4 - 0.0000000434*t5;
    *eA = arcSeconds * M_PI/(3600 * 180);
    arcSeconds = 10.556403*t - 2.3814292*t2 - 0.00121197*t3 + 0.000170663*t4 - 0.0000000560*t5;
    *chiA = arcSeconds * M_PI/(3600 * 180);
    arcSeconds = 2.650545 + 2306.083227*t + 0.2988499*t2 + 0.01801828*t3 - 0.000005971*t4 - 0.0000003173*t5;
    *zetaA = arcSeconds * M_PI/(3600 * 180);
    arcSeconds = -2.650545 + 2306.077181*t + 1.0927348*t2 + 0.01826837*t3 - 0.000028596*t4 - 0.0000002904*t5;
    *zA = arcSeconds * M_PI/(3600 * 180);
    arcSeconds = 2004.19103*t - 0.4294934*t2 - 0.04182264*t3 - 0.000007089*t4 - 0.0000001274*t5;
    *thetaA = arcSeconds * M_PI/(3600 * 180);
}

// P03; uses general precession quantities
static void
convertJ2000ToOfDate(double julianCenturiesSince2000Epoch,
                     double raJ2000,
                     double declJ2000,
                     double *raOfDate,
                     double *declOfDate) {
    double pA, eA, chiA, zetaA, zA, thetaA;
    generalPrecessionQuantities(julianCenturiesSince2000Epoch, &pA, &eA, &chiA, &zetaA, &zA, &thetaA);
    double cosDecl = cos(declJ2000);
    double sinDecl = sin(declJ2000);
    double cosTheta = cos(thetaA);
    double sinTheta = sin(thetaA);
    double term = cosDecl*cos(raJ2000 + zetaA);
    double A = cosDecl*sin(raJ2000 + zetaA);
    double B = cosTheta*term - sinTheta*sinDecl;
    double C = sinTheta*term + cosTheta*sinDecl;
    double raMinusZ = atan2(A, B);
    double ra = ESUtil::fmod(raMinusZ + zA, M_PI * 2);
    if (ra < 0) {
        ra += M_PI * 2;
    }
    *raOfDate = ra;
    *declOfDate = asin(C);  // Meeus says: if star is close to celestial pole, use decl = acos(sqrt(A*A + B*B)) instead; but for now we're just dealing with things in the ecliptic
}

// Meeus; P03 does not have formulae for angles to convert back to J2000; see also refineConvertToJ2000FromOfDate below
static void
convertToJ2000FromOfDate(double julianCenturiesSince2000Epoch,
                         double raOfDate,
                         double declOfDate,
                         double *raJ2000,
                         double *declJ2000) {
    double T = julianCenturiesSince2000Epoch;
    double T2 = T*T;
    double t = -T;
    double t2 = t*t;
    double t3 = t2*t;
    double arcSeconds = (2306.2181 + 1.39656*T - 0.000139*T2)*t
        + (0.30188 - 0.000344*T)*t2 + 0.017998*t3;
    double zetaA = arcSeconds * M_PI/(3600 * 180);
    arcSeconds = (2306.2181 + 1.39656*T - 0.000139*T2)*t
        + (1.09468 + 0.000066*T)*t2 + 0.018203*t3;
    double zA = arcSeconds * M_PI/(3600 * 180);
    arcSeconds = (2004.3109 - 0.85330*T - 0.000217*T2)*t
        - (0.42665 + 0.000217*T)*t2 - 0.041833*t3;
    double thetaA = arcSeconds * M_PI/(3600 * 180);
    double cosDecl = cos(declOfDate);
    double sinDecl = sin(declOfDate);
    double cosTheta = cos(thetaA);
    double sinTheta = sin(thetaA);
    double term = cosDecl*cos(raOfDate + zetaA);
    double A = cosDecl*sin(raOfDate + zetaA);
    double B = cosTheta*term - sinTheta*sinDecl;
    double C = sinTheta*term + cosTheta*sinDecl;
    double raMinusZ = atan2(A, B);
    double ra = ESUtil::fmod(raMinusZ + zA, M_PI * 2);
    if (ra < 0) {
        ra += M_PI * 2;
    }
    *raJ2000 = ra;
    *declJ2000 = asin(C);  // Meeus says: if star is close to celestial pole, use decl = acos(sqrt(A*A + B*B)) instead
}

// Meeus gets very close (10 arcseconds?), but this will get us as exact as we need.  Initial plus 2 refines gets us to within .01 arcsecond
static void
refineConvertToJ2000FromOfDate(double julianCenturiesSince2000Epoch,
                               double raOfDate,
                               double declOfDate,
                               double *raJ2000,
                               double *declJ2000) {
    double raTry2000, declTry2000;
    convertToJ2000FromOfDate(julianCenturiesSince2000Epoch, raOfDate, declOfDate, &raTry2000, &declTry2000);
    double raRoundTrip, declRoundTrip;
    convertJ2000ToOfDate(julianCenturiesSince2000Epoch, raTry2000, declTry2000, &raRoundTrip, &declRoundTrip);
    double raOfDateTweak = raOfDate + (raOfDate - raRoundTrip);
    double declOfDateTweak = declOfDate + (declOfDate - declRoundTrip);
    convertToJ2000FromOfDate(julianCenturiesSince2000Epoch, raOfDateTweak, declOfDateTweak, &raTry2000, &declTry2000);
    convertJ2000ToOfDate(julianCenturiesSince2000Epoch, raTry2000, declTry2000, &raRoundTrip, &declRoundTrip);
    raOfDateTweak = raOfDateTweak + (raOfDate - raRoundTrip); 
    declOfDateTweak = declOfDateTweak + (declOfDate - declRoundTrip);
    convertToJ2000FromOfDate(julianCenturiesSince2000Epoch, raOfDateTweak, declOfDateTweak, &raTry2000, &declTry2000);
    *raJ2000 = raTry2000;
    *declJ2000 = declTry2000;
}

static void sunRAandDeclJ2000(ESTimeInterval dateInterval,
                              double         *rightAscensionReturn,
                              double         *declinationReturn,
                              ECAstroCache   *_currentCache) {
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - dateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[sunRAJ2000SlotIndex] == _currentCache->currentFlag) {
        *rightAscensionReturn = _currentCache->cacheSlots[sunRAJ2000SlotIndex];
        *declinationReturn = _currentCache->cacheSlots[sunDeclJ2000SlotIndex];
        return;
    }
    double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(dateInterval, NULL, _currentCache);
    double raOfDate;
    double declOfDate;
    double sunLongitude;
    WB_sunRAAndDecl(julianCenturiesSince2000Epoch/100, &raOfDate, &declOfDate, &sunLongitude, _currentCache);
    refineConvertToJ2000FromOfDate(julianCenturiesSince2000Epoch, raOfDate, declOfDate, rightAscensionReturn, declinationReturn);
    if (_currentCache) {
        _currentCache->cacheSlotValidFlag[sunRAJ2000SlotIndex] = _currentCache->currentFlag;
        _currentCache->cacheSlotValidFlag[sunDeclJ2000SlotIndex] = _currentCache->currentFlag;
        _currentCache->cacheSlots[sunRAJ2000SlotIndex] = *rightAscensionReturn;
        _currentCache->cacheSlots[sunDeclJ2000SlotIndex] = *declinationReturn;
    }
}

static void moonRAandDeclJ2000(ESTimeInterval dateInterval,
                               double         *rightAscensionReturn,
                               double         *declinationReturn,
                               ECAstroCache   *_currentCache) {
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - dateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonRAJ2000SlotIndex] == _currentCache->currentFlag) {
        *rightAscensionReturn = _currentCache->cacheSlots[moonRAJ2000SlotIndex];
        *declinationReturn = _currentCache->cacheSlots[moonDeclJ2000SlotIndex];
        return;
    }
    double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(dateInterval, NULL, _currentCache);
    double raOfDate;
    double declOfDate;
    double moonEclipticLongitude;
    double moonEclipticLatitude;
    WB_MoonRAAndDecl(julianCenturiesSince2000Epoch, &raOfDate, &declOfDate, &moonEclipticLongitude, &moonEclipticLatitude, _currentCache, ECWBFullPrecision);
    refineConvertToJ2000FromOfDate(julianCenturiesSince2000Epoch, raOfDate, declOfDate, rightAscensionReturn, declinationReturn);
    if (_currentCache) {
        _currentCache->cacheSlotValidFlag[moonRAJ2000SlotIndex] = _currentCache->currentFlag;
        _currentCache->cacheSlotValidFlag[moonDeclJ2000SlotIndex] = _currentCache->currentFlag;
        _currentCache->cacheSlots[moonRAJ2000SlotIndex] = *rightAscensionReturn;
        _currentCache->cacheSlots[moonDeclJ2000SlotIndex] = *declinationReturn;
    }
}

#if 0  // unused
static void
testConvertJ2000() {
    double julianCenturiesSince2000Epoch = -60;
    double raJ2000 = 41.054063 * M_PI / 180;
    double declJ2000 = 49.227750 * M_PI / 180;
    double raOfDate;
    double declOfDate;
    convertJ2000ToOfDate(julianCenturiesSince2000Epoch, raJ2000, declJ2000, &raOfDate, &declOfDate);
    //printAngle(raOfDate, "test convert RA");
    //printAngle(declOfDate, "test convert decl");
    double raOrig = raJ2000;
    double declOrig = declJ2000;
    refineConvertToJ2000FromOfDate(julianCenturiesSince2000Epoch, raOfDate, declOfDate, &raJ2000, &declJ2000);
    printf("And the results are:\n");
    printAngle(raOrig, "RA J2000 orig");
    printAngle(raJ2000, "RA J2000 round trip");
    printAngle(declOrig, "Decl J2000 orig");
    printAngle(declJ2000, "Decl J2000 round trip");
}
#endif

static double planetRadiiInAU[ECNumPlanets] = {
    695500  / kECAUInKilometers,  // ECPlanetSun       = 0
    1737.10 / kECAUInKilometers,  // ECPlanetMoon      = 1
    2439.7  / kECAUInKilometers,  // ECPlanetMercury   = 2
    6051.8  / kECAUInKilometers,  // ECPlanetVenus     = 3
    6371.0  / kECAUInKilometers,  // ECPlanetEarth     = 4,
    3389.5  / kECAUInKilometers,  // ECPlanetMars      = 5,
    69911   / kECAUInKilometers,  // ECPlanetJupiter   = 6,
    58232   / kECAUInKilometers,  // ECPlanetSaturn    = 7,
    25362   / kECAUInKilometers,  // ECPlanetUranus    = 8,
    24622   / kECAUInKilometers,  // ECPlanetNeptune   = 9,
    1195    / kECAUInKilometers   // ECPlanetPluto     = 10,
};

static double planetMassInKG[ECNumPlanets] = {
    11.9891e30, // Sun
    7.3477e22,  // Moon
    0.330104* 1e24,     // Mercury
    4.86732 * 1e24,     // Venus
    5.97219 * 1e24,     // Earth
    0.641693* 1e24,     // Mars
    1898.13 * 1e24,     // Jupiter
    568.319 * 1e24,     // Saturn
    86.8103 * 1e24,     // Uranus
    102.410 * 1e24,     // Neptune
    0.01309 * 1e24      // Pluto
};

static double planetOrbitalPeriodInYears[ECNumPlanets] = {
    0,  // Sun
    27.321582 / 365.256366,     // Moon
    0.2408467,  // Mercury
    0.61519726, // Venus
    1.0000174,  // Earth
    1.8808476,  // Mars
    11.862615,  // Jupiter
    29.447498,  // Saturn
    84.016846,  // Uranus
    164.79132,  // Neptune
    247.92065   // Pluto
};

static void
planetSizeAndParallax(int    planetNumber,
                      double distanceInAU,
                      double *angularSizeReturn,
                      double *parallaxReturn) {
    ESAssert(planetNumber >= 0 && planetNumber < ECNumPlanets);
    double radiusInAU = planetRadiiInAU[planetNumber];
    *angularSizeReturn = 2 * atan(radiusInAU / distanceInAU);
    *parallaxReturn = asin(sin(8.794/3600*M_PI/180) / distanceInAU);
}

static double
planetAltAz(int            planetNumber,
            ESTimeInterval calculationDateInterval,
            double         observerLatitude,
            double         observerLongitude,
            bool           correctForParallax,
            bool           altNotAz,
            ECAstroCache   *_currentCache) {
    double angle;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - calculationDateInterval) <= ASTRO_SLOP);
    int slotBase = altNotAz ? planetAltitudeSlotIndex : planetAzimuthSlotIndex;
    if (_currentCache && _currentCache->cacheSlotValidFlag[slotBase+planetNumber] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[slotBase+planetNumber];
    } else {
        // At the north pole, the azimuth of *everything* is south.  But that's not useful, so use the limiting value of azimuth as the latitude approaches zero
        if (observerLatitude > kECLimitingAzimuthLatitude) {
            observerLatitude = kECLimitingAzimuthLatitude;
        } else if (observerLatitude < - kECLimitingAzimuthLatitude) {
            observerLatitude = - kECLimitingAzimuthLatitude;
        }
        double planetRightAscension;
        double planetDeclination;
        double planetGeocentricDistance;
        if (_currentCache && _currentCache->cacheSlotValidFlag[planetRASlotIndex+planetNumber] == _currentCache->currentFlag) {
            ESAssert(_currentCache->cacheSlotValidFlag[planetDeclSlotIndex+planetNumber] == _currentCache->currentFlag);
            planetRightAscension = _currentCache->cacheSlots[planetRASlotIndex+planetNumber];
            planetDeclination = _currentCache->cacheSlots[planetDeclSlotIndex+planetNumber];
            planetGeocentricDistance = _currentCache->cacheSlots[planetGeocentricDistanceSlotIndex+planetNumber];
        } else {
            double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(calculationDateInterval, NULL, _currentCache);
            double planetEclipticLongitude;
            double latitude;
            WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &planetEclipticLongitude, &latitude, &planetGeocentricDistance, &planetRightAscension, &planetDeclination, _currentCache, ECWBFullPrecision);
        }
        double gst = convertUTToGSTP03(calculationDateInterval, _currentCache);
        double lst = convertGSTtoLST(gst, observerLongitude);
        double planetHourAngle = lst - planetRightAscension;
        if (correctForParallax) {
            double planetTopoHourAngle;
            double planetTopoDecl;
            topocentricParallax(planetRightAscension, planetDeclination, planetHourAngle, planetGeocentricDistance, observerLatitude, 0, &planetTopoHourAngle, &planetTopoDecl);
            //printAngle(planetDeclination, ESUtil::stringWithFormat("%s Decl", nameOfPlanetWithNumber(planetNumber).c_str()).c_str());
            //printAngle(planetTopoDecl, ESUtil::stringWithFormat("%s Topo Decl", nameOfPlanetWithNumber(planetNumber).c_str()).c_str());
            planetDeclination = planetTopoDecl;
            planetHourAngle = planetTopoHourAngle;
        }
        double sinAlt = sin(planetDeclination)*sin(observerLatitude) + cos(planetDeclination)*cos(observerLatitude)*cos(planetHourAngle);
        //printAngle(observerLatitude, ESUtil::stringWithFormat("%s observerLatitude", nameOfPlanetWithNumber(planetNumber).c_str()).c_str());
        //printAngle(cos(observerLatitude), ESUtil::stringWithFormat("%s cos(observerLatitude)", nameOfPlanetWithNumber(planetNumber).c_str()).c_str());
        //double numerator = -cos(planetDeclination)*cos(observerLatitude)*sin(planetHourAngle);
        //printAngle(numerator, ESUtil::stringWithFormat("%s numerator", nameOfPlanetWithNumber(planetNumber).c_str()).c_str());
        //double denominator = sin(planetDeclination) - sin(observerLatitude)*sinAlt;
        //printAngle(denominator, ESUtil::stringWithFormat("%s denominator", nameOfPlanetWithNumber(planetNumber).c_str()).c_str());
        double planetAzimuth = atan2(-cos(planetDeclination)*cos(observerLatitude)*sin(planetHourAngle), sin(planetDeclination) - sin(observerLatitude)*sinAlt);
        //printAngle(planetAzimuth, ESUtil::stringWithFormat("%s Azimuth", nameOfPlanetWithNumber(planetNumber).c_str()).c_str());
        double planetAltitude = asin(sinAlt);
        //printAngle(planetAltitude, ESUtil::stringWithFormat("%s Altitude", nameOfPlanetWithNumber(planetNumber).c_str()).c_str());
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[planetAltitudeSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[planetAzimuthSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlots[planetAltitudeSlotIndex+planetNumber] = planetAltitude;
            _currentCache->cacheSlots[planetAzimuthSlotIndex+planetNumber] = planetAzimuth;
        }
        angle = altNotAz ? planetAltitude : planetAzimuth;
    }
    return angle;
}

//static double maxdiff =0;
double
cachelessPlanetAlt(int            planetNumber,
                   ESTimeInterval calculationDateInterval,
                   double         observerLatitude,
                   double         observerLongitude) {
    //printf("cachelessPlanetAlt checking time %s\n", [[[NSDate dateWithTimeIntervalSinceReferenceDate:calculationDateInterval] description] UTF8String]);
    double angle;
    double planetRightAscension;
    double planetDeclination;
    double planetGeocentricDistance;
    double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(calculationDateInterval, NULL, NULL);
    double planetEclipticLongitude;
    double latitude;
    WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &planetEclipticLongitude, &latitude, &planetGeocentricDistance, &planetRightAscension, &planetDeclination, NULL, ECWBFullPrecision);
    double gst = convertUTToGSTP03(calculationDateInterval, NULL);
    double lst = convertGSTtoLST(gst, observerLongitude);
    double planetHourAngle = lst - planetRightAscension;
/*
    double planetTopoHourAngle;
    double planetTopoDecl;
    topocentricParallax(planetRightAscension, planetDeclination, planetHourAngle, planetGeocentricDistance, observerLatitude, 0, &planetTopoHourAngle, &planetTopoDecl);
    if (fabs(planetDeclination - planetTopoDecl) > maxdiff) {
        maxdiff = fabs(planetDeclination - planetTopoDecl);
        printf("%g\n", maxdiff);            // result (for Sun) is 4.32235e-05
    }
    planetDeclination = planetTopoDecl;
    planetHourAngle = planetTopoHourAngle;
 */
    double sinAlt = sin(planetDeclination)*sin(observerLatitude) + cos(planetDeclination)*cos(observerLatitude)*cos(planetHourAngle);
    //double planetAzimuth = atan2(-cos(planetDeclination)*cos(observerLatitude)*sin(planetHourAngle), sin(planetDeclination) - sin(observerLatitude)*sinAlt);
    double planetAltitude = asin(sinAlt);
    angle = planetAltitude;
    return angle;
}

double
cachelessSunDecl (double dateInterval) {
    double sunRightAscension;
    double sunDeclination;
    sunRAandDecl(dateInterval, &sunRightAscension, &sunDeclination, NULL);
    return sunDeclination;
}

static double
distanceOfPlanetInAU(int           planetNumber,
                     double        julianCenturiesSince2000Epoch,
                     ECAstroCache  *_currentCache,
                     ECWBPrecision moonPrecision) {
    ESAssert(planetNumber >= 0 && planetNumber < ECNumLegalPlanets);
    switch(planetNumber) {
      case ECPlanetSun:
        return WB_sunRadius(julianCenturiesSince2000Epoch/100, _currentCache);
      case ECPlanetMoon:
        return WB_MoonDistance(julianCenturiesSince2000Epoch, _currentCache, moonPrecision) / kECAUInKilometers;
      case ECPlanetMercury:
      case ECPlanetVenus:
      case ECPlanetMars:
      case ECPlanetJupiter:
      case ECPlanetSaturn:
      case ECPlanetUranus:
      case ECPlanetNeptune:
        {
            double geocentricApparentLongitude;
            double geocentricApparentLatitude;
            double geocentricDistance;
            double apparentRightAscension;
            double apparentDeclination;
            WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100,
                                      &geocentricApparentLongitude, &geocentricApparentLatitude,
                                      &geocentricDistance, &apparentRightAscension, &apparentDeclination, _currentCache, ECWBFullPrecision);
            return geocentricDistance;
        }
      case ECPlanetEarth:
      default:
        ESAssert(false);
        return 0;
    }
}

static void angularSizeAndParallaxForPlanet(double         julianCenturiesSince2000Epoch,
                                            int            planetNumber,
                                            double         *angularSize,
                                            double         *parallax,
                                            ECAstroCache   *_currentCache,
                                            ECWBPrecision  moonPrecision) {
    double planetDistance = distanceOfPlanetInAU(planetNumber, julianCenturiesSince2000Epoch, _currentCache, moonPrecision);
    planetSizeAndParallax(planetNumber, planetDistance, angularSize, parallax);
}

// Meeus calls this h0
static double
altitudeAtRiseSet(double        julianCenturiesSince2000Epoch,
                  int           planetNumber,
                  bool          wantGeocentricAltitude,
                  ECAstroCache  *_currentCache,
                  ECWBPrecision moonPrecision) {
    double angularDiameter;
    double parallax;
    angularSizeAndParallaxForPlanet(julianCenturiesSince2000Epoch, planetNumber, &angularDiameter, &parallax, _currentCache, moonPrecision);
    return (wantGeocentricAltitude ? parallax : 0) - kECRefractionAtHorizonX - angularDiameter/2.0;
//    if (wantGeocentricAltitude) {  // I think this is right, but it makes almost no difference...
//      double alt = parallax - kECRefractionAtHorizonX - angularDiameter/2.0;
//      return asin(sin(parallax)*cos(alt)) - kECRefractionAtHorizonX - angularDiameter/2.0;
//    } else {
//      return -kECRefractionAtHorizonX - angularDiameter/2.0;
//    }
}

// Note: does not incorporate delta-m correction from Meeus here, but otherwise follows pp 102-103
static ESTimeInterval
riseSetTime(bool             riseNotSet,
            double           rightAscension,
            double           declination,
            double           observerLatitude,
            double           observerLongitude,
            double           altAtRiseSet,
            ESTimeInterval   calculationDateInterval,
            ECAstroCachePool *cachePool) {
    double cosH = (sin(altAtRiseSet) - sin(observerLatitude)*sin(declination)) / (cos(observerLatitude)*cos(declination));
    PRINT_DOUBLE(cosH);
    if (cosH < -1.0) {
        PRINT_STRING1("No rise/set: cosh negative (%g)\n", cosH);
        return kECAlwaysAboveHorizon;    // always above the horizon (obsLat > 0 == decl > 0)
    } else if (cosH > 1.0) {
        PRINT_STRING1("No rise/set: cosh positive (%g)\n", cosH);
        return kECAlwaysBelowHorizon;    // always below the horizon (obsLat > 0 != decl > 0)
    }
    double H = acos(cosH);
    PRINT_ANGLE(H);
    double LST_rs = rightAscension + (riseNotSet ? (M_PI * 2) - H : H);
    PRINT_ANGLE(LST_rs);
    if (LST_rs > (M_PI * 2)) {
        LST_rs -= (M_PI * 2);
    }
    PRINT_ANGLE(LST_rs);
    int riseSetDayOffset;
    double GST_rs = convertLSTtoGST(LST_rs, observerLongitude, &riseSetDayOffset);
    PRINT_ANGLE(GST_rs);
    PRINT_STRING1("     ...day offset: %d\n", riseSetDayOffset);
    ESTimeInterval riseSetDate = convertGSTtoUTclosest(GST_rs, calculationDateInterval, cachePool);
    return riseSetDate;
}
                    
static ESTimeInterval
transitTime(ESTimeInterval dateInterval,
            bool           wantHighTransit,
            double         observerLongitude,
            double         rightAscension,
            ECAstroCache   *currentCache) {
    double gst = convertUTToGSTP03(dateInterval, currentCache);
    if (!wantHighTransit) {
        rightAscension += M_PI;
    }
    double hourAngle = ESUtil::fmod(gst + observerLongitude - rightAscension, (M_PI * 2));
    if (hourAngle > M_PI) {
        hourAngle -= (M_PI * 2);
    } else if (hourAngle < -M_PI) {
        hourAngle += (M_PI * 2);
    }
    double transit = dateInterval - hourAngle*(12*3600)/M_PI;
    return transit;
}

static double linearFit(double X1,
                        double Y1,
                        double X2,
                        double Y2) {
    // Offset to reduce roundoff error:
    double offset = X1;
    X1 = 0;
    Y1 -= offset;
    X2 -= offset;
    Y2 -= offset;
    double denom = X2 - X1 - Y2 + Y1;
    if (denom == 0) {
        return Y2 + offset;  // Best we can do
    }
    //printDateD(X1+offset, "X1 lin");
    //printDateD(Y1+offset, "Y1 lin");
    //printDateD(X2+offset, "X2 lin");
    //printDateD(Y2+offset, "Y2 lin");
    double root = (Y1*(X2 - X1) - X1*(Y2 - Y1)) / denom;
    //printDateD(offset + root, "Y root");
    if (fabs(root - Y2) > 12 * 3600) {  // bogus
        return Y2 + offset;
    }
    return offset + root;
}

// This function presumes that we are trying to find x such that f(x) = x,
// for the function whose prior values are y1 = f(x1), y2 = f(x2), etc, and
// such that the latest values in the array are presumed to be most accurate.
// If there is only one point, then the only reasonable value is to choose y1.
// For two points, we draw a line through P1 and P2 and see where it intersects
// y == x.  For three or more points, we take the most recent three points,
// draw a parabola through it (a quadratic equation), and see where (if anywhere)
// that parabola intersects y == x.  If there are no roots, we revert to linear;
// if there are two roots, we take the closest root to yN.
static double extrapolateToYEqualX(const double x[],
                                   const double y[],
                                   int    numValues) {
    ESAssert(numValues > 0);
    if (numValues == 1) {
        return y[0];
    }
    
    if (numValues > 2) {

        // To greatly increase the resolution of the numbers we're working from, offset every number from X1
        double offset = x[numValues - 3];
        double X1 = 0;
        double Y1 = y[numValues - 3] - offset;
        double X2 = x[numValues - 2] - offset;
        double Y2 = y[numValues - 2] - offset;
        double X3 = x[numValues - 1] - offset;
        double Y3 = y[numValues - 1] - offset;

        // Expanding Lagrange's formula for a parabola through 3 points:

        if (X1 != X2 && X1 != X3 && X2 != X3) {

            double k1 = Y1/((X1 - X2)*(X1 - X3));
            double k2 = Y2/((X2 - X1)*(X2 - X3));
            double k3 = Y3/((X3 - X1)*(X3 - X2));

            // Following, then, are coefficients of quadratic equation through p1,p2,p3, for y = C2*x*x - C1*x + C0
            double C2 = k1 + k2 + k3;
            double C1 = k1*(X2 + X3) + k2*(X1 + X3) + k3*(X1 + X2);
            double C0 = k1*X2*X3 + k2*X1*X3 + k3*X1*X2;

            // If y == x, then it becomes C2*x*x + (-C1 - 1)*x + C0 = 0, or in std quadratic form A = C2, B = -C1-1, C = C0, then dividing by A to get p and q we get
            if (C2 != 0) {
                double p = (-C1 - 1)/C2;
                double q = C0 / C2;
                double D = p*p/4 - q;
                if (D >= 0) {
                    double sqrtTerm = sqrt(D);
                    double root1 = -p/2 + sqrtTerm;
                    double root2 = -p/2 - sqrtTerm;
                    if (fabs(root1 - Y3) < fabs(root2 - Y3)) {
                        if (fabs(root1 - Y3) < 24 * 3600) { // reject totally bogus values and revert to linear
                            //printDateD(X1+offset, "X1");
                            //printDateD(Y1+offset, "Y1");
                            //printDateD(X2+offset, "X2");
                            //printDateD(Y2+offset, "Y2");
                            //printDateD(X3+offset, "X3");
                            //printDateD(Y3+offset, "Y3");
                            //printDateD(root1+offset, "root1");
                            return root1+offset;
                        }
                        if (printingEnabled) printf("Totally bogus\n");
                    } else {
                        if (fabs(root2 - Y3) < 24 * 3600) { // reject totally bogus values and revert to linear
                            //printDateD(X1+offset, "X1");
                            //printDateD(Y1+offset, "Y1");
                            //printDateD(X2+offset, "X2");
                            //printDateD(Y2+offset, "Y2");
                            //printDateD(X3+offset, "X3");
                            //printDateD(Y3+offset, "Y3");
                            //printDateD(root2+offset, "root2");
                            return root2+offset;
                        }
                        if (printingEnabled) printf("Totally bogus\n");
                    }
                }
            }
        }
    }
    return linearFit(x[numValues - 2], y[numValues -2], x[numValues - 1], y[numValues - 1]);
}

static ESTimeInterval
planettransitTimeRefined(ESTimeInterval              calculationDateInterval,
                         double                      observerLatitude,
                         double                      observerLongitude,
                         bool                        wantHighTransit,
                         int                         planetNumber,
                         double                      overrideAltitudeDesired,   // useless parameter here
                         double                      *riseSetOrTransit,  // useless parameter here
                         ECAstroCachePool            *cachePool)
{
    ESAssert(planetNumber >= 0 && planetNumber <= ECLastLegalPlanet);
    ESTimeInterval tryDate = calculationDateInterval;
    ECWBPrecision precision = planetNumber == ECPlanetMoon ? ECWBLowPrecision : ECWBFullPrecision;  // Start out moon at low precision
    const int numIterations = 7;
    double tryDates[numIterations];
    double results[numIterations];
    int fitTries = 0;
    for(int i = 0; i < numIterations; i++) {
        if (planetNumber == ECPlanetMoon && i == numIterations - 1 && precision != ECWBFullPrecision) {
            precision = ECWBFullPrecision;
            i --;  // Give us two more shots at it with full precision
            fitTries = 0;  // And ignore any low-precision prior values
        }
        double rightAscension;
        double declination;
        ECAstroCache *priorCache = pushECAstroCacheWithSlopInPool(cachePool, &cachePool->refinementCache, tryDate, 0);
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(tryDate, NULL, cachePool->currentCache);
        double longitude;
        double latitude;
        double distance;
        WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &longitude, &latitude, &distance, &rightAscension, &declination, cachePool->currentCache, precision);
//      printAngle(rightAscension, "rightAscension");
//      printAngle(declination, "declination");
        double newDate = transitTime(tryDate, wantHighTransit, observerLongitude, rightAscension, cachePool->currentCache);
        ESAssert(!isnan(newDate));  // there's always a transit time
        popECAstroCacheToInPool(cachePool, priorCache);
#ifndef NDEBUG
        //printf("Planet %s transit %d (%s) Iteration %d: %s\n",
        //       wantHighTransit ? "high" : "low",
        //       planetNumber,
        //       ESAstronomyManager::nameOfPlanetWithNumber(planetNumber).localizedValue().c_str(),
        //       i,
        //       ESTime::timeAsString(newDate).c_str());
#else
        //printf("Planet %s %d Iteration %d: %s\n", wantHighTransit ? "high" : "low", planetNumber, i, [[[NSDate dateWithTimeIntervalSinceReferenceDate:tryDate] description] UTF8String]);
#endif
        if (fabs(newDate - tryDate) < 0.1) {  // values within 0.1 second are deemed close enough
            if (planetNumber == ECPlanetMoon && precision != ECWBFullPrecision) {
                precision = ECWBFullPrecision;
            } else {
                *riseSetOrTransit = newDate;
                return newDate;
            }
        }
        tryDates[fitTries] = tryDate;
        results [fitTries++] = newDate;
        tryDate = extrapolateToYEqualX(tryDates, results, fitTries);
    }
#ifndef NDEBUG
    // printf("Planet transit %d (%s): %s\n", planetNumber, [nameOfPlanetWithNumber(planetNumber) UTF8String], [[[NSDate dateWithTimeIntervalSinceReferenceDate:tryDate] description] UTF8String]);
#else
    // printf("Planet %d: %s\n", planetNumber, [[[NSDate dateWithTimeIntervalSinceReferenceDate:tryDate] description] UTF8String]);
#endif
    *riseSetOrTransit = tryDate;
    return tryDate;
}

// Return the rise time closest to the given calculation date, by iterative refinement
static ESTimeInterval
planetaryRiseSetTimeRefined(ESTimeInterval           calculationDateInterval,
                            double                   observerLatitude,
                            double                   observerLongitude,
                            bool                     riseNotSet,
                            int                      planetNumber,
                            double                   overrideAltitudeDesired,
                            double                   *riseSetOrTransit,
                            ECAstroCachePool         *cachePool) {
    ESAssert(planetNumber >= 0 && planetNumber <= ECLastLegalPlanet);
    ESTimeInterval tryDate = calculationDateInterval;
    ESAssert(!isnan(tryDate));
    ESTimeInterval lastValidResultDate = nan("");
    ESTimeInterval lastValidTryDate = nan("");
    bool convergedToInvalid = false;
    bool polarSpecial = fabs(observerLatitude) > M_PI / 180 * 89;
    //if (printingEnabled) printf("polarSpecial %s\n", polarSpecial ? "true" : "false");
    ECWBPrecision precision = planetNumber == ECPlanetMoon ? ECWBLowPrecision : ECWBFullPrecision;  // Start out moon at low precision
    if (polarSpecial) {
        precision = ECWBFullPrecision;  // We need all the help we can get at polar latitudes
    }
    const int numIterations = 20;
    const int numPolarTries = 10;  // Number of binary-search tries to find a place that has a valid rise/set -- should get us down to less than a minute
    double tryDates[numIterations + numPolarTries + 1];  // +1 because I'm too lazy to see if I really need it
    double results[numIterations + numPolarTries + 1];
    int fitTries = 0;
    double lastDelta = 0;
    double firstNan = nan("");
    double firstTransit = tryDate;
    for(int i = 0; i < numIterations; i++) {
        if (planetNumber == ECPlanetMoon && i == numIterations - 1 && precision != ECWBFullPrecision) {
            precision = ECWBFullPrecision;
            i --;  // Give us two more shots at it with full precision
            fitTries = 0;  // And ignore any low-precision prior values
        }
        double rightAscension;
        double declination;
        ECAstroCache *priorCache = pushECAstroCacheWithSlopInPool(cachePool, &cachePool->refinementCache, tryDate, 0);
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(tryDate, NULL, cachePool->currentCache);
        double longitude;
        double latitude;
        double distance;
        WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &longitude, &latitude, &distance, &rightAscension, &declination, cachePool->currentCache, precision);
        double altitude = isnan(overrideAltitudeDesired) ? altitudeAtRiseSet(julianCenturiesSince2000Epoch, planetNumber, true/*wantGeocentricAltitude*/, cachePool->currentCache, precision) : overrideAltitudeDesired;
        double newDate = riseSetTime(riseNotSet, rightAscension, declination,
                                     observerLatitude, observerLongitude,
                                     altitude,
                                     tryDate, cachePool);
        popECAstroCacheToInPool(cachePool, priorCache);
        if (isnan(newDate)) {
            // Mostly this means there is no rise/set this day.  But near the first rise/set of the season, the decl may reach a "legal"
            // spot closer to the actual rise time during the same day.  To detect this case, we first calculate the transit time which is most likely to cross
            // the horizon, and see if we're legal there.
            if (!convergedToInvalid) { // If we haven't already done this
                convergedToInvalid = true;
                bool wantHighTransit = ESUtil::nansEqual(newDate, kECAlwaysBelowHorizon);  // if the object is below, we want high transit, to see if the highest point is any better
                priorCache = pushECAstroCacheWithSlopInPool(cachePool, &cachePool->refinementCache, tryDate, 0);
                double tt;
                double transitT = planettransitTimeRefined(tryDate, observerLatitude, observerLongitude, wantHighTransit, planetNumber, nan(""), &tt, cachePool);
                popECAstroCacheToInPool(cachePool, priorCache);
                firstTransit = transitT;
                firstNan = newDate;
                priorCache = pushECAstroCacheWithSlopInPool(cachePool, &cachePool->refinementCache, transitT, 0);
                julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(transitT, NULL, cachePool->currentCache);
                WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &longitude, &latitude, &distance, &rightAscension, &declination, cachePool->currentCache, precision);
                double altit = isnan(overrideAltitudeDesired) ? altitudeAtRiseSet(julianCenturiesSince2000Epoch, planetNumber, true/*wantGeocentricAltitude*/, cachePool->currentCache, precision) : overrideAltitudeDesired;
                newDate = riseSetTime(riseNotSet, rightAscension, declination,
                                      observerLatitude, observerLongitude,
                                      altit,
                                      transitT, cachePool);
                popECAstroCacheToInPool(cachePool, priorCache);
                if (isnan(newDate)) {
                    if (polarSpecial) {
                        // In this case the effect due to the Earth's rotation is small compared to the change due to the Sun's motion in Decl
                        // Go back and forth 13 hours and see if the sun transitioned between up and down; if so binary search to see when it happened
                    
                        // Check -13 hrs
                        // If nan same as ours, skip and check other side (+13 hrs)
                        // If nan different than ours or isn't nan, setup lastPolarUp and lastPolarDown, average them, and iterate
                    
                        ESTimeInterval priorPolar = transitT - 13 * 3600;
                        priorCache = pushECAstroCacheWithSlopInPool(cachePool, &cachePool->refinementCache, priorPolar, 0);
                        julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(priorPolar, NULL, cachePool->currentCache);
                        WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &longitude, &latitude, &distance, &rightAscension, &declination, cachePool->currentCache, precision);
                        ESTimeInterval priorPolarEvent = riseSetTime(riseNotSet, rightAscension, declination,
                                                                     observerLatitude, observerLongitude,
                                                                     altitudeAtRiseSet(julianCenturiesSince2000Epoch, planetNumber, true/*wantGeocentricAltitude*/, cachePool->currentCache, precision),
                                                                     priorPolar, cachePool);
                        popECAstroCacheToInPool(cachePool, priorCache);
                        ESTimeInterval binaryLow = nan("");
                        ESTimeInterval binaryHigh = nan("");
                        ESTimeInterval binaryLowEvent = nan("");
                        ESTimeInterval binaryHighEvent = nan("");
                        if (isnan(priorPolarEvent)) {
                            if (!ESUtil::nansEqual(priorPolarEvent, newDate)) {
                                binaryLow = priorPolar;
                                binaryLowEvent = priorPolarEvent;
                                binaryHigh = transitT;
                                binaryHighEvent = newDate;
                            }
                            ESTimeInterval nextPolar = tryDate + 13 * 3600;
                            priorCache = pushECAstroCacheWithSlopInPool(cachePool, &cachePool->refinementCache, nextPolar, 0);
                            julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(nextPolar, NULL, cachePool->currentCache);
                            WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &longitude, &latitude, &distance, &rightAscension, &declination, cachePool->currentCache, precision);
                            ESTimeInterval nextPolarEvent = riseSetTime(riseNotSet, rightAscension, declination,
                                                                        observerLatitude, observerLongitude,
                                                                        altitudeAtRiseSet(julianCenturiesSince2000Epoch, planetNumber, true/*wantGeocentricAltitude*/, cachePool->currentCache, precision),
                                                                        nextPolar, cachePool);
                            popECAstroCacheToInPool(cachePool, priorCache);
                            if (isnan(nextPolarEvent)) {
                                if (!ESUtil::nansEqual(nextPolarEvent, newDate)) {
                                    binaryLow = transitT;
                                    binaryLowEvent = newDate;
                                    binaryHigh = nextPolar;
                                    binaryHighEvent = nextPolarEvent;
                                } else if (isnan(binaryLow)) {
                                    *riseSetOrTransit = transitT;
                                    ESAssert(!isnan(*riseSetOrTransit));
                                    return newDate;
                                }
                            } else {
                                if (nextPolarEvent > tryDate + 24 * 3600) {
                                    *riseSetOrTransit = transitT;
                                    ESAssert(!isnan(*riseSetOrTransit));
                                    return newDate;
                                }
                                tryDate = nextPolar;
                                ESAssert(!isnan(tryDate));
                                newDate = nextPolarEvent;
                            }
                        } else {
                            if (priorPolarEvent < tryDate - 24 * 3600) {  // Too long ago, doesn't count
                                *riseSetOrTransit = transitT;
                                ESAssert(!isnan(*riseSetOrTransit));
                                return newDate;
                            }
                            tryDate = priorPolar;
                            ESAssert(!isnan(tryDate));
                            newDate = priorPolarEvent;
                        }
                        if (!isnan(binaryLow)) {
                            //printDateD(binaryLow, "binary search between here");
                            //printDateD(binaryHigh, ".. and here");
                            int polarTries = numPolarTries;
                            while (polarTries--) {
                                ESTimeInterval split = (binaryLow + binaryHigh) / 2;
                                priorCache = pushECAstroCacheWithSlopInPool(cachePool, &cachePool->refinementCache, split, 0);
                                julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(split, NULL, cachePool->currentCache);
                                WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &longitude, &latitude, &distance, &rightAscension, &declination, cachePool->currentCache, precision);
                                ESTimeInterval splitEvent = riseSetTime(riseNotSet, rightAscension, declination,
                                                                        observerLatitude, observerLongitude,
                                                                        altitudeAtRiseSet(julianCenturiesSince2000Epoch, planetNumber, true/*wantGeocentricAltitude*/, cachePool->currentCache, precision),
                                                                        split, cachePool);
                                popECAstroCacheToInPool(cachePool, priorCache);
                                if (!isnan(splitEvent)) {
                                    transitT = split;  // pseudo "transit" for polarSpecial
                                    newDate = splitEvent;
                                    break;
                                }
                                if (ESUtil::nansEqual(splitEvent, binaryLowEvent)) {
                                    binaryLow = split;
                                    binaryLowEvent = splitEvent;
                                } else {
                                    ESAssert(ESUtil::nansEqual(splitEvent, binaryHighEvent));
                                    binaryHigh = split;
                                    binaryHighEvent = splitEvent;
                                }
                            }
                            if (isnan(newDate)) {
                                *riseSetOrTransit = transitT;
                                ESAssert(!isnan(*riseSetOrTransit));
                                return newDate;
                            }
                            //printDateD(transitT, "...binary search found that asking here");
                            //printDateD(newDate, "...got this result");
                        }
                    } else {  // else not polar special
                        //printDateD(tryDate, "no riseSet here, first time through, failed");
                        //printDateD(transitT, "... (transit time was here)");
                        //printAngle(newDate, "...returning newDate");
                        *riseSetOrTransit = transitT;
                        ESAssert(!isnan(*riseSetOrTransit));
                        return newDate;
                    }
                } // end if isnan(newDate) for transit
                //printDateD(transitT, "no riseSet at requestTime, but transitTime here works");
                //if (printingEnabled) printf("nan at calculationDate but transit is better\n");
                ESAssert(!isnan(newDate));
                lastValidTryDate = transitT;
                ESAssert(!isnan(transitT));
                lastValidResultDate = newDate;
                tryDates[fitTries] = transitT;
                results [fitTries++] = newDate;
                tryDate = extrapolateToYEqualX(tryDates, results, fitTries);  // The point (transitT, newDate) is perfectly acceptable as a fit point
                ESAssert(!isnan(tryDate));
                //tryDate = newDate;
            } else { // already did !convergedToInvalid case
                //printDateD(tryDate, "no riseSet here, been here before");
                // If we've been here before, we know that lastValidTryDate resulted in a legal rise/set.  Let's halve
                // the distance between that and our tryDate here.
                ESAssert(!isnan(lastValidTryDate));
                ESAssert(!isnan(tryDate));
                tryDate = (tryDate + lastValidTryDate) / 2;
                ESAssert(!isnan(tryDate));
                // We have no info about the curve, since it isn't valid here.  So we ignore this in the fitTries arrays
            }
        } else {
            //if (printingEnabled) printDateD(tryDate, "riseSet valid here");
            //if (printingEnabled) printDateD(newDate, "...with result here");
            lastValidTryDate = tryDate;
            lastValidResultDate = newDate;
            tryDates[fitTries] = tryDate;
            results [fitTries++] = newDate;
            tryDate = extrapolateToYEqualX(tryDates, results, fitTries);
            ESAssert(!isnan(tryDate));
            //printDateD(tryDate, "...extrapolating to here");
        }
        //if (printingEnabled) printf("Iterate %d Planet %d (%s %s): %s\n", i, planetNumber, [nameOfPlanetWithNumber(planetNumber) UTF8String],
        //                          riseNotSet ? "rise" : "set",
        //                          [[[NSDate dateWithTimeIntervalSinceReferenceDate:lastValidResultDate] description] UTF8String]);
        lastDelta = lastValidResultDate - lastValidTryDate;
        if (fabs(lastDelta) < 0.1) {
            if (planetNumber == ECPlanetMoon && precision != ECWBFullPrecision) {
                precision = ECWBFullPrecision;
                continue;
            }
#ifndef NDEBUG
            //if (printingEnabled) printf("Converged Planet %d (%s %s): %s\n", planetNumber, [nameOfPlanetWithNumber(planetNumber) UTF8String],
            //riseNotSet ? "rise" : "set",
            //[[[NSDate dateWithTimeIntervalSinceReferenceDate:lastValidResultDate] description] UTF8String]);
#else
            //printf("Planet %d converged on iteration %d: %s\n", planetNumber, i, [[[NSDate dateWithTimeIntervalSinceReferenceDate:lastValidResultDate] description] UTF8String]);
#endif
            *riseSetOrTransit = lastValidResultDate;
            ESAssert(!isnan(*riseSetOrTransit));
            return lastValidResultDate;
        //} else if (printingEnabled && i == numIterations - 1) {
        //    if (printingEnabled) printf("Last delta %.2f seconds off\n", lastDelta);
        }
    }
#ifndef NDEBUG
    //if (printingEnabled) printf("Didn't converge Planet %d (%s): %s\n", planetNumber, [nameOfPlanetWithNumber(planetNumber) UTF8String], [[[NSDate dateWithTimeIntervalSinceReferenceDate:lastValidResultDate] description] UTF8String]);
#else
    //if (printingEnabled) printf("Planet %d didn't converge: %s\n", planetNumber, [[[NSDate dateWithTimeIntervalSinceReferenceDate:lastValidResultDate] description] UTF8String]);
#endif
    if (isnan(lastValidResultDate)) {
        *riseSetOrTransit = tryDate;
        ESAssert(!isnan(*riseSetOrTransit));
    } else if (fabs(lastDelta) > 60) { // Still futzing around
        *riseSetOrTransit = firstTransit;
        lastValidResultDate = firstNan;
        ESAssert(!isnan(*riseSetOrTransit));
    } else {
        *riseSetOrTransit = lastValidResultDate;
        ESAssert(!isnan(*riseSetOrTransit));
    }
    ESAssert(!isnan(*riseSetOrTransit)); 
    return lastValidResultDate;
}

// NOTE: THIS function is off, since it calculates the EOT not at the
// given UT but at the UT whose value is UT+EOT .  Thus it will be off
// by the amount that the EOT has changed during those minutes.  This
// error is no more than the order of 1% of the amount that the EOT
// changes during a day, which tends to be larger when the percentage
// is lower.  The actual error is estimated to be on the order of one
// second or less.
static double EOTSeconds(ESTimeInterval   dateInterval,
                         ECAstroCachePool *cachePool) {
    // Find the longitude at which the mean Sun crosses the meridian at this time.
    // That's the longitude whose offset from Greenwich is exactly the fraction of
    // a day from UT noon.
    ESTimeInterval noonD = noonUTForDateInterval(dateInterval);
    ESTimeInterval secondsFromNoon = dateInterval - noonD;
    double longitudeOfMeanSun = - secondsFromNoon * M_PI / (12 * 3600);  // Sign change:  if it's one hour after UT noon, the longitude of the Sun is one hour west
    double rightAscension;
    double declination;
    // Get the Sun's RA.   This is the local actual sidereal time for the given longitude.
    sunRAandDecl(dateInterval, &rightAscension, &declination, cachePool ? cachePool->currentCache : NULL);
    // The actual sidereal time at Greenwich can be obtained by subtracting the longitude
    double gast = rightAscension - longitudeOfMeanSun;
    // Now convert from gst to UT to get actual solar noon at magic longitude
    ESTimeInterval utDate = convertGSTtoUTclosest(gast, dateInterval, cachePool);
    return dateInterval - utDate;
}

ESTimeInterval
EOTSecondsForDateInterval(double dateInterval) {
    return EOTSeconds(dateInterval, NULL);
}

double EOT(ESTimeInterval   dateInterval,
           ECAstroCachePool *cachePool) {
    return EOTSeconds(dateInterval, cachePool) * M_PI / (12 * 3600);
}

void
ESAstronomyManager::printDateD(ESTimeInterval dt,
                               const char     *description) {
    if (!printingEnabled) {
        return;
    }
    double fractionalSeconds = dt - floor(dt);
    int microseconds = round(fractionalSeconds * 1000000);

    ESDateComponents ltcs;
    ESCalendar_localDateComponentsFromTimeInterval(dt, _estz, &ltcs);
    int ltSecond = floor(ltcs.seconds);

    ESDateComponents utcs;
    ESCalendar_localDateComponentsFromTimeInterval(dt, _estz, &utcs);
    int utSecond = floor(utcs.seconds);

    printf("%s %04d/%02d/%02d %02d:%02d:%02d.%06d LT, %s %04d/%02d/%02d %02d:%02d:%02d.%06d UT %s\n",
           ltcs.era ? " CE" : "BCE", ltcs.year, ltcs.month, ltcs.day, ltcs.hour, ltcs.minute, ltSecond, microseconds,
           utcs.era ? " CE" : "BCE", utcs.year, utcs.month, utcs.day, utcs.hour, utcs.minute, utSecond, microseconds,
           description);
}

ESAstronomyManager::ESAstronomyManager() {
    ESAssert(false);
}

/*virtual*/ 
ESAstronomyManager::~ESAstronomyManager() {
}

/* static */ void
ESAstronomyManager::initializeStatics () {
    initializeAstroCache();
#ifndef NDEBUG
    //WB_printMemoryUsage();
#endif
}

/*static*/ double
ESAstronomyManager::_zodiacCenters[12] = {          // ecliptic longitudes of constellation centers
     11,    // Psc
     42,    // Ari
     72,    // Tau
    104,    // Gem
    128,    // Can
    156,    // Leo
    196,    // Vir
    230,    // Lib
    254,    // Sco
    283,    // Sgr
    314,    // Cap
    340     // Aqr
};

/*static*/ double
ESAstronomyManager::_zodiacEdges[13] = {        // ecliptic longitudes of constellation western edges
     -8,   //  0 Psc
     29,   //  1 Ari
     54,   //  2 Tau
     90,   //  3 Gem
    118,   //  4 Can
    138,   //  5 Leo
    174,   //  6 Vir
    218,   //  7 Lib
    242,   //  8 Sco, incl Oph
    266,   //  9 Sgr
    300,   // 10 Cap
    327,   // 11 Aqr
    352    // 12 Psc
};

/* static */ double
ESAstronomyManager::centerOfZodiacConstellation(int n) {
    return _zodiacCenters[(int)n]/360*2*M_PI;
}

/* static */ double
ESAstronomyManager::widthOfZodiacConstellation(int n) {
    return fabs(_zodiacEdges[(int)n]-_zodiacEdges[(int)n+1])*2.0*M_PI/360.0;
}

/* static */ ESUserString
ESAstronomyManager::zodiacConstellationOf(double elong) {
    for (int i=1; i<13; i++) {
        if ((_zodiacEdges[i] * M_PI/180) > elong) {
            switch (i-1) {
                case  0: return ESLocalizedString("Pisces", "the constellation of the zodiac");
                case  1: return ESLocalizedString("Aries", "the constellation of the zodiac");
                case  2: return ESLocalizedString("Taurus", "the constellation of the zodiac");
                case  3: return ESLocalizedString("Gemini", "the constellation of the zodiac");
                case  4: return ESLocalizedString("Cancer", "the constellation of the zodiac");
                case  5: return ESLocalizedString("Leo", "the constellation of the zodiac");
                case  6: return ESLocalizedString("Virgo", "the constellation of the zodiac");
                case  7: return ESLocalizedString("Libra", "the constellation of the zodiac");
                case  8: return ESLocalizedString("Scorpius", "the constellation of the zodiac");
                case  9: return ESLocalizedString("Sagittarius", "the constellation of the zodiac");
                case 10: return ESLocalizedString("Capricornus", "the constellation of the zodiac");
                case 11: return ESLocalizedString("Aquarius", "the constellation of the zodiac");
                default: ESAssert(false); return NULL;
            }
        }
    }
    return ESLocalizedString("Pisces", "the constellation of the zodiac");
}

#ifndef NDEBUG
void
ESAstronomyManager::testPolarEdge () {
    ESDateComponents cs;
    cs.year = 2009;
    cs.month = 3;
    cs.day = 27;
    cs.hour = 12;
    cs.minute = 0;
    cs.seconds = 0;
    ESTimeZone *estzTest = ESCalendar_initTimeZoneFromOlsonID("US/Pacific");
    ESTimeInterval calculationDate = ESCalendar_timeIntervalFromLocalDateComponents(estzTest, &cs);
    double riseSetOrTransit;
    ESTimeInterval riseTime = planetaryRiseSetTimeRefined(calculationDate,
                                                            70 * M_PI / 180, // latitude
                                                          -122 * M_PI / 180, // longitude
                                                          true, // riseNotSet
                                                          ECPlanetVenus,
                                                          nan(""),
                                                          &riseSetOrTransit,
                                                          _astroCachePool);
    printingEnabled = true;
    printDateDWithTimeZone(riseTime, estzTest, "polarEdge Venusrise");
    ESCalendar_releaseTimeZone(estzTest);
    printingEnabled = false;
}
#endif

void
ESAstronomyManager::runTests () {
#ifndef NDEBUG
    double ra;
    double decl;
    double moonEclipticLongitude;
    double age;
    double moonPhase;
    double angularSize;
    double parallax;
    ESDateComponents cs;
    cs.era = 1;
    static bool testsRun = false;
    if (testsRun) {
        return;
    }
    testsRun = true;

    printf("\nSection 51\n");
    cs.year = 1980;
    cs.month = 7;
    cs.day = 27;
    cs.hour = 12;
    cs.minute = 0;
    cs.seconds = 0;
    ::EOT(ESCalendar_timeIntervalFromUTCDateComponents(&cs), _astroCachePool);

    printf("\nSection 65\n");
    cs.year = 1979;
    cs.month = 2;
    cs.day = 26;
    cs.hour = 16;
    cs.minute = 0;
    cs.seconds = 0;
    moonRAAndDecl(ESCalendar_timeIntervalFromUTCDateComponents(&cs), &ra, &decl, &moonEclipticLongitude, _currentCache);

    printf("\nSection 66\n");
    cs.year = 1979;
    cs.month = 2;
    cs.day = 26;
    cs.hour = 17;
    cs.minute = 0;
    cs.seconds = 0;
    moonRAAndDecl(ESCalendar_timeIntervalFromUTCDateComponents(&cs), &ra, &decl, &moonEclipticLongitude, _currentCache);

    printf("\nSection 67\n");
    cs.year = 1979;
    cs.month = 2;
    cs.day = 26;
    cs.hour = 16;
    cs.minute = 0;
    cs.seconds = 0;
    age = moonAge(ESCalendar_timeIntervalFromUTCDateComponents(&cs), &moonPhase, _currentCache);
    printf("age=%g\n", age);

    printf("\nSection 69\n");
    cs.year = 1979;
    cs.month = 9;
    cs.day = 6;
    cs.hour = 0;
    cs.minute = 0;
    cs.seconds = 0;
    double t = ESCalendar_timeIntervalFromUTCDateComponents(&cs);
    double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(t, NULL, _currentCache);
    double distance = WB_MoonDistance(julianCenturiesSince2000Epoch, _currentCache, ECWBFullPrecision);
    moonRAAndDecl(t, &ra, &decl, &moonEclipticLongitude, _currentCache);
    planetSizeAndParallax(ECPlanetMoon, distance/kECAUInKilometers, &angularSize, &parallax);

    printf("\nSection 27\n");
    double elong = (139 + 41/60.0 + 10/3600.0) * M_PI / 180.0;
    PRINT_ANGLE(elong);
    double elat = (4 + 52/60.0 + 31/3600.0) * M_PI / 180.0;
    PRINT_ANGLE(elat);
    double obli = 23.441884 * M_PI / 180.0;
    PRINT_ANGLE(obli);

    raAndDeclO(elat, elong, obli, &ra, &decl);
    printAngle(ra, "ra");
    printAngle(decl, "decl");
    
    printf("\nSection 47\n");
    cs.year = 1988;
    cs.month = 7;
    cs.day = 27;
    cs.hour = 0;
    cs.minute = 0;
    cs.seconds = 0;
    sunRAandDecl(ESCalendar_timeIntervalFromUTCDateComponents(&cs), &ra, &decl, _currentCache);
    printAngle(ra, "ra");
    printAngle(decl, "decl");

    printf("\nSection 15\n");
    double lst = (0 + 24/60.0 + 5.23/3600.0) * M_PI / 12.0;
    PRINT_ANGLE(lst);
    double olong = - 64 * M_PI / 180.0;
    PRINT_ANGLE(olong);
    int dayO;
    double gst = convertLSTtoGST(lst, olong, &dayO);
    printAngle(gst, "gst");
    PRINT_STRING1("     ...day offset: %d\n", dayO);

    printf("\nSection 13\n");
    gst = (4 + 40/60.0 + 5.23/3600.0) * M_PI / 12.0;
    PRINT_ANGLE(gst);
    cs.year = 1980;
    cs.month = 4;
    cs.day = 22;
    cs.hour = 0;
    cs.minute = 0;
    cs.seconds = 0;
    double ut0_2;
    double ut0 = convertGSTtoUT(gst, ESCalendar_timeIntervalFromUTCDateComponents(&cs), &ut0_2, _astroCachePool);
    printAngle(ut0, "ut");

    printf("\nMeeus Example 12.a (in reverse)\n");
    gst = (13 + 10/60.0 + 46.3668/3600.0) * M_PI / 12.0;
    PRINT_ANGLE(gst);
    cs.year = 1987;
    cs.month = 4;
    cs.day = 10;
    cs.hour = 0;
    cs.minute = 0;
    cs.seconds = 0;
    ut0 = convertGSTtoUT(gst, ESCalendar_timeIntervalFromUTCDateComponents(&cs), &ut0_2, _astroCachePool);
    printAngle(ut0, "ut");

    printf("\nSection 4\n");
    cs.year = 1985;
    cs.month = 2;
    cs.day = 17;
    cs.hour = 6;
    cs.minute = 0;
    cs.seconds = 0;
    double jd = julianDateForDate(ESCalendar_timeIntervalFromUTCDateComponents(&cs));
    printDouble(jd, "jd");

    printf("\nSection 49\n");
    cs.era = 1;
    cs.year = 2009;
    cs.month = 3;
    cs.day = 27;
    cs.hour = 12;
    cs.minute = 0;
    cs.seconds = 0;
    ESTimeZone *estzTest = ESCalendar_initTimeZoneFromOlsonID("America/New_York");
    /*ESTimeInterval calculationDate = */ ESCalendar_timeIntervalFromLocalDateComponents(estzTest, &cs);
    estzTest = NULL;
    cs.year = 1986;
    cs.month = 3;
    cs.day = 10;
    cs.hour = 6;
    cs.minute = 0;
    cs.seconds = 0;
    ESTimeInterval tryDateD = ESCalendar_timeIntervalFromLocalDateComponents(estzTest, &cs);
    double olat = 42.37 * M_PI/180;
    olong = -71.05 * M_PI/180;
    double riseSetOrTransit;
    ESTimeInterval riseD = planetaryRiseSetTimeRefined(tryDateD, olat, olong, true, ECPlanetSun, nan(""), &riseSetOrTransit, _astroCachePool);
    printDateD(riseD, "sunrise"/*withDescription*/);
    cs.hour = 18;
    tryDateD = ESCalendar_timeIntervalFromLocalDateComponents(estzTest, &cs);
    ESCalendar_releaseTimeZone(estzTest);
    ESTimeInterval setD = planetaryRiseSetTimeRefined(tryDateD, olat, olong, false, ECPlanetSun, nan(""), &riseSetOrTransit, _astroCachePool);
    printDateD(setD, "sunset"/*withDescription*/);

    printf("\nSection 70\n");
    cs.year = 1986;
    cs.month = 3;
    cs.day = 6;
    cs.hour = 17;  // noon Boston
    cs.minute = 0;
    cs.seconds = 0;
    olat = (42.0 + 22/60.0) * M_PI/180;
    olong = -(71 + 3/60.0) * M_PI/180;
    riseD = planetaryRiseSetTimeRefined(ESCalendar_timeIntervalFromUTCDateComponents(&cs), olat, olong, true, ECPlanetMoon, nan(""), &riseSetOrTransit, _astroCachePool);
    setD = planetaryRiseSetTimeRefined(ESCalendar_timeIntervalFromUTCDateComponents(&cs), olat, olong, false, ECPlanetMoon, nan(""), &riseSetOrTransit, _astroCachePool);
    printDateD(riseD, "moonrise"/*withDescription*/);
    printDateD(setD, "moonset"/*withDescription*/);

    printf("\nBug 1\n");
    cs.year = 2008;
    cs.month = 6;
    cs.day = 27;
    cs.hour = 23;  // 16:35 PDT
    cs.minute = 35;
    cs.seconds = 0;
    riseD = planetaryRiseSetTimeRefined(ESCalendar_timeIntervalFromUTCDateComponents(&cs), 37.32 * M_PI/180, -122.03 * M_PI/180, true, ECPlanetSun, nan(""), &riseSetOrTransit, _astroCachePool);
    printDateD(riseD, "sunrise"/*withDescription*/);

    printf("\nBug 2\n");
    cs.year = 2008;
    cs.month = 8;
    cs.day = 27;
    cs.hour = 3;  // 20:00 PDT
    cs.minute = 0;
    cs.seconds = 0;
    riseD = planetaryRiseSetTimeRefined(ESCalendar_timeIntervalFromUTCDateComponents(&cs), 70 * M_PI/180, -122.03 * M_PI/180, true, ECPlanetSun, nan(""), &riseSetOrTransit, _astroCachePool);
    printDateD(riseD, "sunrise"/*withDescription*/);

    printf("\nSection 68\n");
    cs.year = 1979;
    cs.month = 5;
    cs.day = 19;
    cs.hour = 0;
    cs.minute = 0;
    cs.seconds = 0;
    sunRAandDecl(ESCalendar_timeIntervalFromUTCDateComponents(&cs), &ra, &decl, _currentCache);
    //printAngle(ra, "sun ra");
    //printAngle(decl, "sun decl");
    double moonRA;
    double moonDecl;
    moonRAAndDecl(ESCalendar_timeIntervalFromUTCDateComponents(&cs), &moonRA, &moonDecl, &moonEclipticLongitude, _currentCache);
    printAngle(moonRA, "moon ra");
    printAngle(moonDecl, "moon decl");
    double pa = positionAngle(ra, decl, moonRA, moonDecl);
    printAngle(pa, "position angle");

    printf("\n\n");
    printingEnabled = false;
#endif
}

void
ESAstronomyManager::setupLocalEnvironmentForThreadFromActionButton(bool         fromActionButton,
                                                                   ESWatchTime *watchTime) {
    ECAstroCachePool *poolForThisThread = getCachePoolForThisThread();
    ESAssert(poolForThisThread);
    //if (_astroCachePool) {
    //printf("  ");
    //}
    //printf("astro setupLocalEnvironment, fromActionButton=%s, poolForThisThread=0x%08x, _astroCachePool=0x%08x, _inActionButton=%s, _currentCache=0x%08x\n",
    //fromActionButton ? "true" : "false",
    //(unsigned int)poolForThisThread,
    //(unsigned int)_astroCachePool,
    //_inActionButton ? "true" : "false",
    //(unsigned int)_currentCache);
    if (_astroCachePool) {
        ESAssert(!fromActionButton);
        ESAssert(_inActionButton);
        ESAssert(_astroCachePool->inActionButton);
        ESAssert(_astroCachePool == poolForThisThread);
        ESAssert(_estz);
        ESAssert(!_watchTime || _watchTime->currentTime() == _calculationDateInterval);
        ESAssert(_currentCache);
        if (_currentCache && fabs(_currentCache->dateInterval - _calculationDateInterval) > ASTRO_SLOP) {
            pushECAstroCacheInPool(_astroCachePool, &_astroCachePool->finalCache, _calculationDateInterval);
        }
        ESAssert(fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
        //printf("...exiting early\n");
        return;
    }
    _astroCachePool = poolForThisThread;
    _watchTime = watchTime;
#ifndef NDEBUG
    if (_inActionButton) {
        ESAssert(poolForThisThread->inActionButton);
    }
    ESAssert(!_estz);
    ESAssert(!_currentCache);
    ESAssert(_observerLatitude == 0);
    ESAssert(_observerLongitude == 0);
#endif

    _calculationDateInterval = _watchTime->currentTime();

    _estz = ESCalendar_retainTimeZone(_environment->estz());

    _observerLatitude = _location->latitudeRadians();
    //PRINT_ANGLE(observerLatitude);

    _observerLongitude = _location->longitudeRadians();
    //PRINT_ANGLE(observerLongitude);

    _locationValid = true;

    initializeCachePool(poolForThisThread,
                        _calculationDateInterval,
                        _observerLatitude,
                        _observerLongitude,
                        _watchTime->runningBackward(),
                        _watchTime->tzOffsetUsingEnv(_environment));

    _currentCache = _astroCachePool->currentCache;
    ESAssert(_currentCache);
    ESAssert(fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);

    _scratchWatchTime = new ESWatchTime;

    if (fromActionButton) {
        ESAssert(!_inActionButton);
        ESAssert(!poolForThisThread->inActionButton);
        _inActionButton = true;
        poolForThisThread->inActionButton = true;
    }
}

void
ESAstronomyManager::cleanupLocalEnvironmentForThreadFromActionButton(bool fromActionButton) {
    ESAssert(_astroCachePool);
    ESAssert(_astroCachePool == getCachePoolForThisThread());
    ESAssert(_currentCache);
    //if (_inActionButton && !fromActionButton) {
    //printf("  ");
    //}
    //printf("astro cleanupLocalEnvironment, fromActionButton=%s, poolForThisThread=0x%08x, _astroCachePool=0x%08x, _inActionButton=%s, _currentCache=0x%08x\n",
    //fromActionButton ? "true" : "false",
    //(unsigned int)(getCachePoolForThisThread()),
    //(unsigned int)_astroCachePool,
    //_inActionButton ? "true" : "false",
    //(unsigned int)_currentCache);
    if (fromActionButton) {
        ESAssert(_inActionButton);
        ESAssert(_astroCachePool->inActionButton);
        _inActionButton = false;
        _astroCachePool->inActionButton = false;
        releaseCachePoolForThisThread(_astroCachePool);
    } else {
        if (_inActionButton) {
            ESAssert(_astroCachePool->inActionButton);
            return;
        }
        if (!_astroCachePool->inActionButton) {
            releaseCachePoolForThisThread(_astroCachePool);
        }
    }
    _astroCachePool = NULL;
    _watchTime = NULL;
    _currentCache = NULL;
    _locationValid = false;
    _observerLatitude = 0;
    _observerLongitude = 0;
    _locationValid = false;
    _calculationDateInterval = 0;
    ESAssert(_estz);
    ESCalendar_releaseTimeZone(_estz);
    _estz = NULL;
    delete _scratchWatchTime;
    _scratchWatchTime = NULL;
}

/* In seconds */
static double
localSiderealTime(double       calculationDateInterval,
                  double       observerLongitude,
                  ECAstroCache *currentCache) {
    double ret;
    if (currentCache && currentCache->cacheSlotValidFlag[lstSlotIndex] == currentCache->currentFlag) {
        ret = calculationDateInterval - currentCache->cacheSlots[lstSlotIndex];
    } else {
        double deltaTSeconds;
        double centuriesSinceEpochTDT = julianCenturiesSince2000EpochForDateInterval(calculationDateInterval, &deltaTSeconds, currentCache);
        double priorUTMidnightD = priorUTMidnightForDateInterval(calculationDateInterval, currentCache);
        double utRadiansSinceMidnight = (calculationDateInterval - priorUTMidnightD) * M_PI/(12 * 3600);
        double gst = convertUTToGSTP03x(centuriesSinceEpochTDT, deltaTSeconds, utRadiansSinceMidnight, priorUTMidnightD);
        ret = convertGSTtoLST(gst, observerLongitude) * (12 * 3600)/M_PI + priorUTMidnightD;
        if (currentCache) {
            currentCache->cacheSlotValidFlag[lstSlotIndex] = currentCache->currentFlag;
            currentCache->cacheSlots[lstSlotIndex] = calculationDateInterval - ret;
        }
    }
    return ret;
}

double
ESAstronomyManager::localSiderealTime () {
    double ret;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[lstSlotIndex] == _currentCache->currentFlag) {
        ret = _calculationDateInterval - _currentCache->cacheSlots[lstSlotIndex];
    } else {
        double deltaTSeconds;
        double centuriesSinceEpochTDT = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, &deltaTSeconds, _currentCache);
        double priorUTMidnightD = priorUTMidnightForDateInterval(_calculationDateInterval, _currentCache);
        double utRadiansSinceMidnight = (_calculationDateInterval - priorUTMidnightD) * M_PI/(12 * 3600);
        double gst = convertUTToGSTP03x(centuriesSinceEpochTDT, deltaTSeconds, utRadiansSinceMidnight, priorUTMidnightD);
        ret = convertGSTtoLST(gst, _observerLongitude) * (12 * 3600)/M_PI + priorUTMidnightD;
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[lstSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[lstSlotIndex] = _calculationDateInterval - ret;
        }
    }
    return ret;
}

static bool isSummer(ESTimeInterval _calculationDateInterval,
                     double         observerLatitude,
                     ECAstroCache   *_currentCache) {
    double rightAscension;
    double declination;
    sunRAandDecl(_calculationDateInterval, &rightAscension, &declination, _currentCache);       
    return ((declination >= 0 && observerLatitude >= 0) ||
            (declination <  0 && observerLatitude < 0));
}

static bool moonIsSummer(ESTimeInterval _calculationDateInterval,
                         double         observerLatitude,
                         ECAstroCache   *_currentCache) {
    double rightAscension;
    double declination;
    double moonEclipticLongitude;
    moonRAAndDecl(_calculationDateInterval, &rightAscension, &declination, &moonEclipticLongitude, _currentCache);
    return ((declination >= 0 && observerLatitude >= 0) ||
            (declination <  0 && observerLatitude < 0));
}

static bool planetIsSummer(ESTimeInterval _calculationDateInterval,
                           double         observerLatitude,
                           int            planetNumber,
                           ECAstroCache   *_currentCache) {
    double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
    double planetRightAscension;
    double planetDeclination;
    double planetEclipticLongitude;
    double planetEclipticLatitude;
    double planetGeocentricDistance;
    WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &planetEclipticLongitude, &planetEclipticLatitude, &planetGeocentricDistance, &planetRightAscension, &planetDeclination, _currentCache, ECWBFullPrecision);
    return ((planetDeclination >= 0 && observerLatitude >= 0) ||
            (planetDeclination <  0 && observerLatitude < 0));
}

// returns 1 in summer half of the year, 0 otherwise; (the equator is considered northern)
bool
ESAstronomyManager::summer () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    bool ret = isSummer(_calculationDateInterval, _observerLatitude, _currentCache);
    return ret;
}

// returns 1 if planet is above the equator and the observer is also, or both below
bool
ESAstronomyManager::planetIsSummer(int planetNumber) {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    bool ret = ::planetIsSummer(_calculationDateInterval, _observerLatitude, planetNumber, _currentCache);
    return ret;
}

double
ESAstronomyManager::EOTSeconds() {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double eot;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[eotForDaySlotIndex] == _currentCache->currentFlag) {
        eot = _currentCache->cacheSlots[eotForDaySlotIndex];
    } else {
        eot = ::EOTSeconds(_calculationDateInterval, _astroCachePool);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[eotForDaySlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[eotForDaySlotIndex] = eot;
        }
    }
    return eot;
}

double
ESAstronomyManager::EOT() {
    return this->EOTSeconds() * M_PI / (12 * 3600);
}

static double fudgeFactorSeconds = 5;  // enough so refined closest is behind us

// compiler work around:
static ESTimeInterval (*ECsaveCalculationMethod)(ESTimeInterval calculationDate, double observerLatitude, double observerLongitude, bool riseNotSet, int planetNumber, double overrideAltitudeDesired, double *riseSetOrTransit, ECAstroCachePool *cachePool) = NULL;


ESTimeInterval
ESAstronomyManager::nextPrevRiseSetInternalWithFudgeInterval(double            fudgeSeconds,
                                                             CalculationMethod calculationMethod,
                                                             double            overrideAltitudeDesired,
                                                             int               planetNumber,
                                                             bool              riseNotSet,
                                                             bool              isNext,
                                                             ESTimeInterval    lookahead,
                                                             ESTimeInterval    *riseSetOrTransit) {
    // work around apparent compiler bug
    ECsaveCalculationMethod = calculationMethod;

    // strategy: Pick closest time.  If it's ahead of us, we're done.
    //    otherwise look ahead and pick closest.
    if (!isNext) {
        fudgeSeconds = -fudgeSeconds;
        lookahead = -lookahead;
    }
    //printDateD(_calculationDateInterval, (isNext ? "NEXT starting with date here" : "PREV starting with date here")/*withDescription*/);
    ESTimeInterval fudgeDate = _calculationDateInterval + fudgeSeconds;
    //printDateD(fudgeDate, "fudging to here"/*withDescription*/);
    ESTimeInterval returnDate = (*calculationMethod)(fudgeDate, _observerLatitude, _observerLongitude, riseNotSet, planetNumber, overrideAltitudeDesired, riseSetOrTransit, _astroCachePool);
    ESAssert(!isnan(*riseSetOrTransit));
    if (isNext
        ? *riseSetOrTransit >= fudgeDate
        : *riseSetOrTransit < fudgeDate) {
        //if (isnan(returnDate)) {
        //    printAngle(returnDate, "nextPrev initial success same day");
        //} else {
        //    printDateD(returnDate, "nextPrev initial success same day"/*withDescription*/);
        //}
        return returnDate;
    }
    //printDateD(returnDate, "nextPrev initial failure different day"/*withDescription*/);

    ESTimeInterval tryDate = fudgeDate + lookahead;
    //printDateD(tryDate, "...so looking ahead from here"/*withDescription*/);
    calculationMethod = ECsaveCalculationMethod;  // work around apparent compiler bug
    returnDate = (*calculationMethod)(tryDate, _observerLatitude, _observerLongitude, riseNotSet, planetNumber, overrideAltitudeDesired, riseSetOrTransit, _astroCachePool);
    //if (isnan(returnDate)) {
    //  printAngle(returnDate, "...... to get here");
    //} else {
    //  printDateD(returnDate, "...... to get here"/*withDescription*/);
    //}
    return returnDate;
}

ESTimeInterval
ESAstronomyManager::nextPrevPlanetRiseSetForPlanet(int  planetNumber,
                                                   bool riseNotSet,
                                                   bool nextNotPrev) {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval returnDate;
    if (!_locationValid) {
        returnDate = nan("");
    } else {
        ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
        int slotIndexBase;
        if (riseNotSet) {
            if (nextNotPrev) {
                slotIndexBase = nextPlanetriseSlotIndex;
            } else {
                slotIndexBase = prevPlanetriseSlotIndex;
            }
        } else {
            if (nextNotPrev) {
                slotIndexBase = nextPlanetsetSlotIndex;
            } else {
                slotIndexBase = prevPlanetsetSlotIndex;
            }
        }
        if (_currentCache && _currentCache->cacheSlotValidFlag[slotIndexBase+planetNumber] == _currentCache->currentFlag) {
            returnDate = _currentCache->cacheSlots[slotIndexBase+planetNumber];
        } else {
            double riseSetOrTransit;
            returnDate = nextPrevRiseSetInternalWithFudgeInterval(fudgeFactorSeconds, planetaryRiseSetTimeRefined/*calculationMethod*/, nan("")/*overrideAltitudeDesired*/, planetNumber, riseNotSet, (_watchTime->runningBackward() ^ nextNotPrev)/*isNext*/, (3600 * 13.2)/*lookahead*/, &riseSetOrTransit);
            PRINT_DATE_VIRT_LT(returnDate);
            if (_currentCache) {
                _currentCache->cacheSlotValidFlag[slotIndexBase+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlots[slotIndexBase+planetNumber] = returnDate;
            }
        }
    }
    return returnDate;
}

ESTimeInterval
ESAstronomyManager::nextSunrise () {
    return nextPrevPlanetRiseSetForPlanet(ECPlanetSun, true/*riseNotSet*/, true/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::nextSunset () {
    return nextPrevPlanetRiseSetForPlanet(ECPlanetSun, false/*riseNotSet*/, true/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::prevSunrise () {
    return nextPrevPlanetRiseSetForPlanet(ECPlanetSun, true/*riseNotSet*/, false/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::prevSunset () {
    return nextPrevPlanetRiseSetForPlanet(ECPlanetSun, false/*riseNotSet*/, false/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::nextMoonrise () {
    return nextPrevPlanetRiseSetForPlanet(ECPlanetMoon, true/*riseNotSet*/, true/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::nextMoonset () {
    return nextPrevPlanetRiseSetForPlanet(ECPlanetMoon, false/*riseNotSet*/, true/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::prevMoonrise () {
    return nextPrevPlanetRiseSetForPlanet(ECPlanetMoon, true/*riseNotSet*/, false/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::prevMoonset () {
    return nextPrevPlanetRiseSetForPlanet(ECPlanetMoon, false/*riseNotSet*/, false/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::nextPlanetriseForPlanetNumber(int planetNumber) {
    return nextPrevPlanetRiseSetForPlanet(planetNumber, true/*riseNotSet*/, true/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::nextPlanetsetForPlanetNumber(int planetNumber) {
    return nextPrevPlanetRiseSetForPlanet(planetNumber, false/*riseNotSet*/, true/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::prevPlanetriseForPlanetNumber(int planetNumber) {
    return nextPrevPlanetRiseSetForPlanet(planetNumber, true/*riseNotSet*/, false/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::prevPlanetsetForPlanetNumber(int planetNumber) {
    return nextPrevPlanetRiseSetForPlanet(planetNumber, false/*riseNotSet*/, false/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::nextOrMidnightForDateInterval(ESTimeInterval opDate) {
    ESTimeZone *estzHere = _environment->estz();
    ESDateComponents cs;
    ESCalendar_localDateComponentsFromTimeInterval(_watchTime->currentTime(), estzHere, &cs);
    cs.hour = 0;
    cs.minute = 0;
    cs.seconds = 0;
    ESTimeInterval nextMidnightD = ESCalendar_timeIntervalFromLocalDateComponents(estzHere, &cs);
    if (_watchTime->runningBackward()) {
        if (opDate < nextMidnightD) {
            return nextMidnightD;
        }
    } else {
        nextMidnightD = ESCalendar_addDaysToTimeInterval(nextMidnightD, estzHere, 1);
        if (opDate > nextMidnightD) {
            return nextMidnightD;
        }
    }
    return opDate;
}

// Note:  Returns internal storage
ESWatchTime  *
ESAstronomyManager::watchTimeForInterval(ESTimeInterval dateInterval) {
    ESAssert(_scratchWatchTime);
    _scratchWatchTime->setToFrozenDateInterval(dateInterval);
    return _scratchWatchTime;
}

ESTimeInterval
ESAstronomyManager::planetRiseSetForDay(int  planetNumber,
                                        bool riseNotSet) {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval returnDate;
    if (!_locationValid) {
        return nan("");
    }
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    int slotIndexBase = riseNotSet ? planetriseForDaySlotIndex : planetsetForDaySlotIndex;
    if (_currentCache && _currentCache->cacheSlotValidFlag[slotIndexBase+planetNumber] == _currentCache->currentFlag) {
        returnDate = _currentCache->cacheSlots[slotIndexBase+planetNumber];
    } else {
        ESTimeInterval riseSetOrTransit;
        returnDate = nextPrevRiseSetInternalWithFudgeInterval(-fudgeFactorSeconds, planetaryRiseSetTimeRefined/*calculationMethod*/, nan("")/*overrideAltitudeDesired*/,
                                                              planetNumber, riseNotSet, true/*isNext*/, (3600*13.2)/*lookahead*/, &riseSetOrTransit/*riseSetOrTransit*/);
        if (!timesAreOnSameDay(riseSetOrTransit, _calculationDateInterval, _estz)) {
            returnDate = nextPrevRiseSetInternalWithFudgeInterval(-fudgeFactorSeconds, planetaryRiseSetTimeRefined/*calculationMethod*/, nan("")/*overrideAltitudeDesired*/,
                                                                  planetNumber, riseNotSet, false/*isNext*/, (3600*13.2)/*lookahead*/, &riseSetOrTransit/*riseSetOrTransit*/);
            if (!isnan(returnDate) && !timesAreOnSameDay(returnDate, _calculationDateInterval, _estz)) {
                returnDate = nan("");
            }
        }
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[slotIndexBase+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlots[slotIndexBase+planetNumber] = returnDate;
        }
    }
    PRINT_DATE_VIRT_LT(returnDate);
#if 0    
    if (false && planetNumber == ECPlanetMoon && !riseNotSet && !isnan(returnDate)) {
        printf("\nMoonset at %s\n", [[[NSDate dateWithTimeIntervalSinceReferenceDate:returnDate] description] UTF8String]);
        ESWatchTime *saveTime = [[ESWatchTime alloc] initWithFrozenDateInterval:[_watchTime currentTime] andCalendar:ltCalendar];
        [saveTime makeTimeIdenticalToOtherTimer:_watchTime];
        bool savePrintingEnabled = printingEnabled;
        printingEnabled = true;
        for (int i = -20; i <= 20; i += 1) {
            double t = returnDate + i;
            [self cleanupLocalEnvironmentForThreadFromActionButton:false];
            [_watchTime unlatchTime];
            [_watchTime setToFrozenDateInterval:t];
            [_watchTime latchTime];
            [self setupLocalEnvironmentForThreadFromActionButton:false];
            bool isUp = [self planetIsUp:planetNumber];
            printf("  %4d: %s %s\n", i, isUp ? "up" : "not up", [[[NSDate dateWithTimeIntervalSinceReferenceDate:_calculationDateInterval] description] UTF8String]);
        }
        printingEnabled = savePrintingEnabled;
        [self cleanupLocalEnvironmentForThreadFromActionButton:false];
        [_watchTime unlatchTime];
        [_watchTime makeTimeIdenticalToOtherTimer:saveTime];
        [_watchTime latchTime];
        [self setupLocalEnvironmentForThreadFromActionButton:false];
        [saveTime release];
    }
#endif
    return returnDate;
}

static void getParamsForAltitudeKind(CacheSlotIndex altitudeKind,
                                     double         *altitude,
                                     bool           *riseNotSet) {
    ESAssert(altitudeKind >= sunGoldenHourMorning && altitudeKind <= sunAstroTwilightEvening);
    switch(altitudeKind) {
      case sunRiseMorning:
        *altitude = nan("");  // No override, do true rise/set
        *riseNotSet = true;
        break;
      case sunSetEvening:
        *altitude = nan("");  // No override, do true rise/set
        *riseNotSet = false;
        break;
      case sunGoldenHourMorning:
        *altitude = 15 * M_PI / 180;
        *riseNotSet = true;
        break;
      case sunGoldenHourEvening:
        *altitude = 15 * M_PI / 180;
        *riseNotSet = false;
        break;
      case sunCivilTwilightMorning:
        *altitude = -6 * M_PI / 180;
        *riseNotSet = true;
        break;
      case sunCivilTwilightEvening:
        *altitude = -6 * M_PI / 180;
        *riseNotSet = false;
        break;
      case sunNauticalTwilightMorning:
        *altitude = -12 * M_PI / 180;
        *riseNotSet = true;
        break;
      case sunNauticalTwilightEvening:
        *altitude = -12 * M_PI / 180;
        *riseNotSet = false;
        break;
      case sunAstroTwilightMorning:
        *altitude = -18 * M_PI / 180;
        *riseNotSet = true;
        break;
      case sunAstroTwilightEvening:
        *altitude = -18 * M_PI / 180;
        *riseNotSet = false;
        break;
      default:
        ESAssert(false);
        *altitude = nan("");
        *riseNotSet = false;
        break;
    }
}

ESTimeInterval
ESAstronomyManager::sunTimeForDayForAltitudeKind(CacheSlotIndex altitudeKind) {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    if (!_locationValid) {
        return nan("");
    }
    bool riseNotSet;
    double altitude;
    getParamsForAltitudeKind(altitudeKind, &altitude, &riseNotSet);
    if (isnan(altitude)) {  // will be nan if altitudeKind isn't in range
        return altitude;
    }
    ESTimeInterval returnDate;
    if (_currentCache && _currentCache->cacheSlotValidFlag[altitudeKind] == _currentCache->currentFlag) {
        returnDate = _currentCache->cacheSlots[altitudeKind];
    } else {
        ESTimeInterval riseSetOrTransit;
        returnDate = nextPrevRiseSetInternalWithFudgeInterval(-fudgeFactorSeconds, planetaryRiseSetTimeRefined/*calculationMethod*/, altitude/*overrideAltitudeDesired*/, 
                                                              ECPlanetSun/*planetNumber*/, riseNotSet, true/*isNext*/, (3600*13.2)/*lookahead*/, &riseSetOrTransit/*riseSetOrTransit*/);
        if (!timesAreOnSameDay(riseSetOrTransit, _calculationDateInterval, _estz)) {
            returnDate = nextPrevRiseSetInternalWithFudgeInterval(-fudgeFactorSeconds, planetaryRiseSetTimeRefined/*calculationMethod*/, altitude/*overrideAltitudeDesired*/,
                                                                  ECPlanetSun/*planetNumber*/, riseNotSet, false/*isNext*/, (3600*13.2)/*lookahead*/, &riseSetOrTransit/*riseSetOrTransit*/);
            if (!isnan(returnDate) && !timesAreOnSameDay(returnDate, _calculationDateInterval, _estz)) {
                returnDate = nan("");
            }
        }
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[altitudeKind] = _currentCache->currentFlag;
            _currentCache->cacheSlots[altitudeKind] = returnDate;
        }
    }
    PRINT_DATE_VIRT_LT(returnDate);
    return returnDate;
}

ESTimeInterval
ESAstronomyManager::sunriseForDay () {
    double t = planetRiseSetForDay(ECPlanetSun, true/*riseNotSet*/);
    return t;
}

ESTimeInterval
ESAstronomyManager::sunsetForDay () {
    return planetRiseSetForDay(ECPlanetSun, false/*riseNotSet*/);
}

ESTimeInterval
ESAstronomyManager::moonriseForDay () {
    return planetRiseSetForDay(ECPlanetMoon, true/*riseNotSet*/);
}

ESTimeInterval
ESAstronomyManager::moonsetForDay () {
    return planetRiseSetForDay(ECPlanetMoon, false/*riseNotSet*/);
}

ESTimeInterval
ESAstronomyManager::planetriseForDay(int planetNumber) {
    return planetRiseSetForDay(planetNumber, true/*riseNotSet*/);
}
 
ESTimeInterval
ESAstronomyManager::planetsetForDay(int planetNumber) {
    return planetRiseSetForDay(planetNumber, false/*riseNotSet*/);
}
 
ESTimeInterval
ESAstronomyManager::planettransitForDay(int planetNumber) {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval returnDate;
    if (!_locationValid) {
        PRINT_STRING("planettransitForDay returns NULL\n");
        //printf("planettransitForDay returns NULL\n");
        returnDate = nan("");
    } else {
        ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
        if (_currentCache && _currentCache->cacheSlotValidFlag[planettransitForDaySlotIndex+planetNumber] == _currentCache->currentFlag) {
            returnDate = _currentCache->cacheSlots[planettransitForDaySlotIndex+planetNumber];
        } else {
            ESTimeInterval riseSetOrTransit;
            returnDate = nextPrevRiseSetInternalWithFudgeInterval(-fudgeFactorSeconds, planettransitTimeRefined/*calculationMethod*/, nan("")/*overrideAltitudeDesired*/,
                                                                  planetNumber, true/*riseNotSet; means return high transit*/,
                                                                  true/*isNext*/, (3600*13.2)/*lookahead*/, &riseSetOrTransit);
            ESAssert(!isnan(returnDate));
            ESAssert(riseSetOrTransit == returnDate);
            if (!timesAreOnSameDay(returnDate, _calculationDateInterval, _estz)) {
                returnDate = nextPrevRiseSetInternalWithFudgeInterval(-fudgeFactorSeconds, planettransitTimeRefined/*calculationMethod*/, nan("")/*overrideAltitudeDesired*/,
                                                                      planetNumber, true/*riseNotSet; means return high transit*/, false/*isNext*/, (3600*13.2)/*lookahead*/, &riseSetOrTransit);
                ESAssert(!isnan(returnDate));
                ESAssert(riseSetOrTransit == returnDate);
                if (!timesAreOnSameDay(returnDate, _calculationDateInterval, _estz)) {
                    returnDate = nan("");
                }
            }
            if (_currentCache) {
                _currentCache->cacheSlotValidFlag[planettransitForDaySlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlots[planettransitForDaySlotIndex+planetNumber] = returnDate;
            }
        }
    }
    PRINT_DATE_VIRT_LT(returnDate);
    //printf("planettransit for day: %s\n", [[returnDate description] UTF8String]);
    return returnDate;
}

ESTimeInterval
ESAstronomyManager::suntransitForDay () {
    return planettransitForDay(ECPlanetSun);
}

ESTimeInterval
ESAstronomyManager::moontransitForDay () {
    return planettransitForDay(ECPlanetMoon);
}

ESTimeInterval
ESAstronomyManager::nextPrevPlanettransit(int  planetNumber,
                                          bool nextNotPrev,
                                          bool wantHighTransit) {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval returnDate;
    if (!_locationValid) {
        PRINT_STRING("nextPlanettransit returns NULL\n");
        returnDate = nan("");
    } else {
        ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
        int slotIndexBase;
        if (nextNotPrev) {
            if (wantHighTransit) {
                slotIndexBase = nextPlanettransitSlotIndex;
            } else {
                slotIndexBase = nextPlanettransitLowSlotIndex;
            }
        } else {
            if (wantHighTransit) {
                slotIndexBase = prevPlanettransitSlotIndex;
            } else {
                slotIndexBase = prevPlanettransitLowSlotIndex;
            }
        }
        int slotIndex = slotIndexBase + planetNumber;
        if (_currentCache && _currentCache->cacheSlotValidFlag[slotIndex] == _currentCache->currentFlag) {
            returnDate = _currentCache->cacheSlots[slotIndex];
        } else {
            ESTimeInterval riseSetOrTransit;
            if (_watchTime->runningBackward()) {
                returnDate = nextPrevRiseSetInternalWithFudgeInterval(fudgeFactorSeconds, planettransitTimeRefined/*calculationMethod*/, nan("")/*overrideAltitudeDesired*/,
                                                                      planetNumber, wantHighTransit/*riseNotSet; true means want high transit*/, !nextNotPrev/*isNext*/, (3600 * 13.2)/*lookahead*/, &riseSetOrTransit/*riseSetOrTransit*/);
            } else {
                returnDate = nextPrevRiseSetInternalWithFudgeInterval(fudgeFactorSeconds, planettransitTimeRefined/*calculationMethod*/, nan("")/*overrideAltitudeDesired*/,
                                                                      planetNumber, wantHighTransit/*riseNotSet; true means want high transit*/,  nextNotPrev/*isNext*/, (3600 * 13.2)/*lookahead*/, &riseSetOrTransit/*riseSetOrTransit*/);
            }   
            ESAssert(returnDate == riseSetOrTransit);
            PRINT_DATE_VIRT_LT(returnDate);
            if (_currentCache) {
                _currentCache->cacheSlotValidFlag[slotIndex] = _currentCache->currentFlag;
                _currentCache->cacheSlots[slotIndex] = returnDate;
            }
        }
    }
    return returnDate;
}

ESTimeInterval 
ESAstronomyManager::prevSuntransit() {
    return nextPrevPlanettransit(ECPlanetSun, false/*!nextNotPrev*/, true/*wantHighTransit*/);
}

ESTimeInterval 
ESAstronomyManager::nextSuntransitLow() {
    return nextPrevPlanettransit(ECPlanetSun, true/*nextNotPrev*/, false/*!wantHighTransit*/);
}

ESTimeInterval 
ESAstronomyManager::prevSuntransitLow() {
    return nextPrevPlanettransit(ECPlanetSun, false/*!nextNotPrev*/, false/*!wantHighTransit*/);
}


ESTimeInterval
ESAstronomyManager::nextSuntransit () {
    return nextPrevPlanettransit(ECPlanetSun, true/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::nextMoontransit () {
    return nextPrevPlanettransit(ECPlanetMoon, true/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::nextPlanettransit(int planetNumber) {
    return nextPrevPlanettransit(planetNumber, true/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::prevPlanettransit(int planetNumber) {
    return nextPrevPlanettransit(planetNumber, true/*nextNotPrev*/);
}

ESTimeInterval
ESAstronomyManager::nextSunriseOrMidnight () {
    return nextOrMidnightForDateInterval(nextSunrise());
}

ESTimeInterval
ESAstronomyManager::nextSunsetOrMidnight () {
    return nextOrMidnightForDateInterval(nextSunset());
}

ESTimeInterval
ESAstronomyManager::nextMoonriseOrMidnight () {
    return nextOrMidnightForDateInterval(nextMoonrise());
}

ESTimeInterval
ESAstronomyManager::nextMoonsetOrMidnight () {
    return nextOrMidnightForDateInterval(nextMoonset());
}

double
ESAstronomyManager::planetHeliocentricLongitude(int planetNumber) {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    if (planetNumber < ECFirstActualPlanet || planetNumber > ECLastLegalPlanet) {
        return nan("");
    } else if (!_locationValid) {
        return nan("");
    }
    double longitude;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[planetHeliocentricLongitudeSlotIndex+planetNumber] == _currentCache->currentFlag) {
        longitude = _currentCache->cacheSlots[planetHeliocentricLongitudeSlotIndex+planetNumber];
    } else {
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
        longitude = WB_planetHeliocentricLongitude(planetNumber, julianCenturiesSince2000Epoch/100, _currentCache);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[planetHeliocentricLongitudeSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlots[planetHeliocentricLongitudeSlotIndex+planetNumber] = longitude;
        }
    }
    return longitude;
}

double
ESAstronomyManager::planetHeliocentricLatitude(int planetNumber) {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    if (planetNumber < ECFirstActualPlanet || planetNumber > ECLastLegalPlanet) {
        return nan("");
    } else if (!_locationValid) {
        return nan("");
    }
    double latitude;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[planetHeliocentricLatitudeSlotIndex+planetNumber] == _currentCache->currentFlag) {
        latitude = _currentCache->cacheSlots[planetHeliocentricLatitudeSlotIndex+planetNumber];
    } else {
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
        latitude = WB_planetHeliocentricLatitude(planetNumber, julianCenturiesSince2000Epoch/100, _currentCache);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[planetHeliocentricLatitudeSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlots[planetHeliocentricLatitudeSlotIndex+planetNumber] = latitude;
        }
    }
    return latitude;
}

double
ESAstronomyManager::planetHeliocentricRadius(int planetNumber) {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    if (planetNumber < ECFirstActualPlanet || planetNumber > ECLastLegalPlanet) {
        return nan("");
    } else if (!_locationValid) {
        return nan("");
    }
    double radius;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[planetHeliocentricRadiusSlotIndex+planetNumber] == _currentCache->currentFlag) {
        radius = _currentCache->cacheSlots[planetHeliocentricRadiusSlotIndex+planetNumber];
    } else {
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
        radius = WB_planetHeliocentricRadius(planetNumber, julianCenturiesSince2000Epoch/100, _currentCache);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[planetHeliocentricRadiusSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlots[planetHeliocentricRadiusSlotIndex+planetNumber] = radius;
        }
    }
    return radius;
}

std::string
ESAstronomyManager::moonPhaseString () {
    double age;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double phase;
    age = moonAge(_calculationDateInterval, &phase, _currentCache) * 180/M_PI;
    if (age >= 359 || age <= 1) {
        return "New";
    } else if (age < 89) {
        return "Waxing Crescent";
    } else if (age <= 91) {
        return "1st Quarter";
    } else if (age < 179) {
        return "Waxing Gibbous";
    } else if (age <= 181) {
        return "Full";
    } else if (age < 269) {
        return "Waning Gibbous";
    } else if (age <= 271) {
        return "3rd Quarter";
    } else {
        return "Waning Crescent";
    }
}

double
ESAstronomyManager::moonAgeAngle () {
    double age;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double phase;
    age = moonAge(_calculationDateInterval, &phase, _currentCache);
    return age;
}

ESTimeInterval
ESAstronomyManager::nextMoonPhase () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval nextOne;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[nextMoonPhaseSlotIndex] == _currentCache->currentFlag) {
        nextOne = _currentCache->cacheSlots[nextMoonPhaseSlotIndex];
    } else {
        double phase;
        double age = moonAge(_calculationDateInterval, &phase, _currentCache);
        bool runningBackward = _watchTime->runningBackward();
        double fudgeFactor = runningBackward ? -0.01 : 0.01;
        double ageSinceQuarter = ESUtil::fmod(age + fudgeFactor, M_PI/2);  // now age is age angle since nearest exact phase (new, 1st quarter, full, 3rd quarter)
        double ageAtLastQuarter = age + fudgeFactor - ageSinceQuarter;
        double targetAge = runningBackward ? ageAtLastQuarter : ageAtLastQuarter + M_PI/2;
        if (targetAge > 15.0/8 * M_PI) {
            targetAge -= (M_PI * 2);
        }
        nextOne = refineMoonAgeTargetForDate(_calculationDateInterval, targetAge, _astroCachePool);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[nextMoonPhaseSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[nextMoonPhaseSlotIndex] = nextOne;
        }
    }
//    printf("next moon phase: %s (%.2f)\n\n", [[nextOne description] UTF8String], targetAge / (M_PI * 2));
    return nextOne;
}

ESTimeInterval
ESAstronomyManager::prevMoonPhase () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval nextOne;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[prevMoonPhaseSlotIndex] == _currentCache->currentFlag) {
        nextOne = _currentCache->cacheSlots[prevMoonPhaseSlotIndex];
    } else {
        double phase;
        double age = moonAge(_calculationDateInterval, &phase, _currentCache);
        bool runningBackward = !_watchTime->runningBackward();
        double fudgeFactor = runningBackward ? -0.01 : 0.01;
        double ageSinceQuarter = ESUtil::fmod(age + fudgeFactor, M_PI/2);  // now age is age angle since nearest exact phase (new, 1st quarter, full, 3rd quarter)
        double ageAtLastQuarter = age + fudgeFactor - ageSinceQuarter;
        double targetAge = runningBackward ? ageAtLastQuarter : ageAtLastQuarter + M_PI/2;
        if (targetAge > 15.0/8 * M_PI) {
            targetAge -= (M_PI * 2);
        }
        nextOne = refineMoonAgeTargetForDate(_calculationDateInterval, targetAge, _astroCachePool);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[prevMoonPhaseSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[prevMoonPhaseSlotIndex] = nextOne;
        }
    }
//    printf("next moon phase: %s (%.2f)\n\n", [[nextOne description] UTF8String], targetAge / (M_PI * 2));
    return nextOne;
}

double
ESAstronomyManager::realMoonAgeAngle () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double ageAngle;
    ESTimeInterval newMoonDate;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[realMoonAgeAngleSlotIndex] == _currentCache->currentFlag) {
        ageAngle = _currentCache->cacheSlots[realMoonAgeAngleSlotIndex];
    } else {
        double phase;
        ageAngle = moonAge(_calculationDateInterval, &phase, _currentCache);
        if (ageAngle > (M_PI * 2)-0.0001) {
            ageAngle = 0;
        }
        ESTimeInterval guessDate = _calculationDateInterval - kECLunarCycleInSeconds * ageAngle/(M_PI * 2);
        newMoonDate = refineMoonAgeTargetForDate(guessDate, 0, _astroCachePool);
        ageAngle = (_calculationDateInterval - newMoonDate)/86400;
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[realMoonAgeAngleSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[realMoonAgeAngleSlotIndex] = ageAngle;
        }
    }
    // printf("%8.2f since new at %s\n", ageAngle, [[[NSDate dateWithTimeIntervalSinceReferenceDate:newMoonDate] description] UTF8String]);
    return ageAngle;
}

ESTimeInterval
ESAstronomyManager::closestQuarterAngle(double quarterAngle) {
    double phase;
    double age = moonAge(_calculationDateInterval, &phase, _currentCache);
    double ageSinceQuarter = ESUtil::fmod(age - quarterAngle, (M_PI * 2));
    bool closestIsBack =
    _watchTime->runningBackward()
    ? ageSinceQuarter < M_PI + 0.01
    : ageSinceQuarter < M_PI - 0.01;
    ESTimeInterval guessDate =
    closestIsBack
    ? _calculationDateInterval - kECLunarCycleInSeconds * ageSinceQuarter/(M_PI * 2)
    : _calculationDateInterval + kECLunarCycleInSeconds * ((M_PI * 2) - ageSinceQuarter)/(M_PI * 2);
    return refineMoonAgeTargetForDate(guessDate, quarterAngle, _astroCachePool);
}

ESTimeInterval
ESAstronomyManager::closestNewMoon () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval nextOne;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[closestNewMoonSlotIndex] == _currentCache->currentFlag) {
        nextOne = _currentCache->cacheSlots[closestNewMoonSlotIndex];
    } else {
        nextOne = closestQuarterAngle(0);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[closestNewMoonSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[closestNewMoonSlotIndex] = nextOne;
        }
    }
    return nextOne;
}

ESTimeInterval
ESAstronomyManager::closestFullMoon () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval nextOne;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[closestFullMoonSlotIndex] == _currentCache->currentFlag) {
        nextOne = _currentCache->cacheSlots[closestFullMoonSlotIndex];
    } else {
        nextOne = closestQuarterAngle(M_PI);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[closestFullMoonSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[closestFullMoonSlotIndex] = nextOne;
        }
    }
    return nextOne;
}

ESTimeInterval
ESAstronomyManager::closestFirstQuarter () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval nextOne;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[closestFirstQuarterSlotIndex] == _currentCache->currentFlag) {
        nextOne = _currentCache->cacheSlots[closestFirstQuarterSlotIndex];
    } else {
        nextOne = closestQuarterAngle((M_PI/2));
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[closestFirstQuarterSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[closestFirstQuarterSlotIndex] = nextOne;
        }
    }
    return nextOne;
}

ESTimeInterval
ESAstronomyManager::closestThirdQuarter () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval nextOne;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[closestThirdQuarterSlotIndex] == _currentCache->currentFlag) {
        nextOne = _currentCache->cacheSlots[closestThirdQuarterSlotIndex];
    } else {
        nextOne = closestQuarterAngle((3*M_PI/2));
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[closestThirdQuarterSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[closestThirdQuarterSlotIndex] = nextOne;
        }
    }
    return nextOne;
}

ESTimeInterval
ESAstronomyManager::nextQuarterAngle(double quarterAngle) {
    double phase;
    double age = moonAge(_calculationDateInterval, &phase, _currentCache);
    if (_watchTime->runningBackward()) {
        age -= 0.01;  // in case we're right on the same quarter
    } else {
        age += 0.01;
    }
    double ageSinceQuarter = ESUtil::fmod(age - quarterAngle, (M_PI * 2));
    ESTimeInterval guessDate;
    if (_watchTime->runningBackward()) {
        guessDate = _calculationDateInterval - kECLunarCycleInSeconds * ageSinceQuarter/(M_PI * 2);
    } else {
        guessDate = _calculationDateInterval + kECLunarCycleInSeconds * ((M_PI * 2) - ageSinceQuarter)/(M_PI * 2);
    }
    return refineMoonAgeTargetForDate(guessDate, quarterAngle, _astroCachePool);
}

ESTimeInterval
ESAstronomyManager::nextQuarterAngle(double         quarterAngle,
                                     ESTimeInterval fromTime,
                                     bool           nextNotPrev) {
    double phase;
    ECAstroCache *priorCache = pushECAstroCacheWithSlopInPool(_astroCachePool, &_astroCachePool->refinementCache, fromTime, 0);
    double age = moonAge(fromTime, &phase, _astroCachePool->currentCache);
    popECAstroCacheToInPool(_astroCachePool, priorCache);
    if (nextNotPrev) {
        age += 0.01;  // in case we're right on the same quarter
    } else {
        age -= 0.01;
    }
    double ageSinceQuarter = ESUtil::fmod(age - quarterAngle, (M_PI * 2));
    ESTimeInterval guessDate;
    if (_watchTime->runningBackward() == nextNotPrev) {
        guessDate = fromTime - kECLunarCycleInSeconds * ageSinceQuarter/(M_PI * 2);
    } else {
        guessDate = fromTime + kECLunarCycleInSeconds * ((M_PI * 2) - ageSinceQuarter)/(M_PI * 2);
    }
    return refineMoonAgeTargetForDate(guessDate, quarterAngle, _astroCachePool);
}

ESTimeInterval
ESAstronomyManager::nextNewMoon () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval nextOne;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[nextNewMoonSlotIndex] == _currentCache->currentFlag) {
        nextOne = _currentCache->cacheSlots[nextNewMoonSlotIndex];
    } else {
        nextOne = nextQuarterAngle(0);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[nextNewMoonSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[nextNewMoonSlotIndex] = nextOne;
        }
    }
    return nextOne;
}

ESTimeInterval
ESAstronomyManager::nextFullMoon () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval nextOne;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[nextFullMoonSlotIndex] == _currentCache->currentFlag) {
        nextOne = _currentCache->cacheSlots[nextFullMoonSlotIndex];
    } else {
        nextOne = nextQuarterAngle(M_PI);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[nextFullMoonSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[nextFullMoonSlotIndex] = nextOne;
        }
    }
    return nextOne;
}

ESTimeInterval
ESAstronomyManager::nextFirstQuarter () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval nextOne;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[nextFirstQuarterSlotIndex] == _currentCache->currentFlag) {
        nextOne = _currentCache->cacheSlots[nextFirstQuarterSlotIndex];
    } else {
        nextOne = nextQuarterAngle((M_PI/2));
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[nextFirstQuarterSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[nextFirstQuarterSlotIndex] = nextOne;
        }
    }
    return nextOne;
}

ESTimeInterval
ESAstronomyManager::nextThirdQuarter () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval nextOne;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[nextThirdQuarterSlotIndex] == _currentCache->currentFlag) {
        nextOne = _currentCache->cacheSlots[nextThirdQuarterSlotIndex];
    } else {
        nextOne = nextQuarterAngle((3*M_PI/2));
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[nextThirdQuarterSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[nextThirdQuarterSlotIndex] = nextOne;
        }
    }
    return nextOne;
}

double
ESAstronomyManager::planetAge(int    planetNumber,
                              double *moonAge,
                              double *phase) {
    // The phase of a planet is the angle  Sun -> object -> Earth.
    double planet_r = planetHeliocentricRadius(planetNumber);  // Distance from Sun to planet
    double planet_delta = planetGeocentricDistance(planetNumber);  // Distance from Earth to planet
    double planet_R = planetHeliocentricRadius(ECPlanetEarth);  // Distance from Earth to Sun
    // Solving for an angle in a triangle where we know the lengths of the three sides:
    double cos_i = ((planet_r*planet_r) + (planet_delta*planet_delta) - (planet_R*planet_R)) / (2 * planet_r * planet_delta);
    *phase = acos(cos_i);

    // Here be hacks galore.

    // First, we shouldn't be using 'age' at all in our terminator, but we are.  The terminator is strictly based on
    // the phase, not the age.  (The "phase" is the angle Sun-Moon-Earth, and the "age" is delta ecliptic longitude of
    // the Sun and the Moon, which is roughly the angle Sun-Earth-Moon.  The phase controls the shadow, and the only
    // reason the age could be a proxy for the phase is that the age is essentially the complement of the phase in this
    // triangle, since the Moon-Sun-Earth angle is nearly zero).  So even though we've correctly calculated the phase
    // above, we can't use it in the terminator, because the terminator (improperly) wants the age, assuming the age works
    // as with the Moon.

    // So we figure out what Moon age would generate the phase we calcualte above, and then return that.  That's simply the
    // complement, as I said above, subject to sign variations (since we only have the absolute phase value).

    *moonAge = M_PI - *phase;  // The complement of the phase.
    // EC_printAngle(*moonAge, "moonAge");

    // NOTE: Sometimes we actually want the "age" of the object itself, via the delta ecliptic longitudes, or just
    // figure out the appropriate angle in the same triangle (Sun-Earth-Moon):
    cos_i = ((planet_R*planet_R) + (planet_delta*planet_delta) - (planet_r*planet_r)) / (2 * planet_delta * planet_R);
    double age = acos(cos_i);

    // EC_printAngle(age, "age");

    // But age can be negative rather than positive, and the way we calculate it, we only have the absolute value
    // (since it's based on only the sizes of the sides of a triangle).  To distinguish the two cases (+ and -) we need
    // to analyze the relative heliocentric longitudes:
    double deltaHeliocentricLongitude = planetHeliocentricLongitude(planetNumber) - planetHeliocentricLongitude(ECPlanetEarth);
    if (deltaHeliocentricLongitude < 0) {
        deltaHeliocentricLongitude += 2 * M_PI;
    }
    // EC_printAngle(deltaHeliocentricLongitude, "deltaHeliocentricLongitude");
    if (deltaHeliocentricLongitude > M_PI) {
        age = 2 * M_PI - age;
        *moonAge = 2 * M_PI - *moonAge;
    }
    // EC_printAngle(age, "age with sign");
    // EC_printAngle(*moonAge, "moonAge with sign");
    return age;
}

double
ESAstronomyManager::planetPositionAngle(int planetNumber) {  // rotation of terminator relative to North (std defn)
    double sunRightAscension;
    double sunDeclination;
    sunRAandDecl(_calculationDateInterval, &sunRightAscension, &sunDeclination, _currentCache);
    double planetRightAscension = planetRA(planetNumber, false/*correctForParallax*/);
    double planetDeclination = planetDecl(planetNumber, false/*correctForParallax*/);
    return positionAngle(sunRightAscension, sunDeclination, planetRightAscension, planetDeclination);
}

double
ESAstronomyManager::planetRelativePositionAngle(int planetNumber) {  // rotation of terminator as it appears in the sky
    double angle;
    double sunRightAscension;
    double sunDeclination;
    sunRAandDecl(_calculationDateInterval, &sunRightAscension, &sunDeclination, _currentCache);
    double planetRightAscension = planetRA(planetNumber, false/*correctForParallax*/);
    double planetDeclination = planetDecl(planetNumber, false/*correctForParallax*/);
    double posAngle = positionAngle(sunRightAscension, sunDeclination, planetRightAscension, planetDeclination);
    double phase;
    double moonAge;
    planetAge(planetNumber, &moonAge/*planetMoonAgeReturn*/, &phase/*phaseReturn*/);
    if (moonAge > M_PI) { // bright limb on the left, sense of posAngle is reversed by 180
        if (posAngle > M_PI) {
            posAngle -= M_PI;
        } else {
            posAngle += M_PI;
        }
    }
    double gst = convertUTToGSTP03(_calculationDateInterval, _currentCache);
    double lst = convertGSTtoLST(gst, _observerLongitude);
    double planetHourAngle = lst - planetRightAscension;
    double sinAlt = sin(planetDeclination)*sin(_observerLatitude) + cos(planetDeclination)*cos(_observerLatitude)*cos(planetHourAngle);
    double planetAzimuth = atan2(-cos(planetDeclination)*cos(_observerLatitude)*sin(planetHourAngle), sin(planetDeclination) - sin(_observerLatitude)*sinAlt);
    double planetAltitude = asin(sinAlt);
    double northAngle = northAngleForObject(planetAltitude, planetAzimuth, _observerLatitude);
    angle = -northAngle - posAngle - M_PI/2;
    if (angle < 0) {
        angle += (M_PI * 2);
    } else if (angle > (M_PI * 2)) {
        angle -= (M_PI * 2);
    }
    return angle;
}

double
ESAstronomyManager::moonPositionAngle () {  // rotation of terminator relative to North (std defn)
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonPositionAngleSlotIndex] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[moonPositionAngleSlotIndex];
    } else {
        double sunRightAscension;
        double sunDeclination;
        sunRAandDecl(_calculationDateInterval, &sunRightAscension, &sunDeclination, _currentCache);
        double moonRightAscension;
        double moonDeclination;
        double moonEclipticLongitude;
        moonRAAndDecl(_calculationDateInterval, &moonRightAscension, &moonDeclination, &moonEclipticLongitude, _currentCache);
        angle = positionAngle(sunRightAscension, sunDeclination, moonRightAscension, moonDeclination);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[moonPositionAngleSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[moonPositionAngleSlotIndex] = angle;
        }
    }
    return angle;
}

double
ESAstronomyManager::moonRelativePositionAngle () {  // rotation of terminator as it appears in the sky
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonRelativePositionAngleSlotIndex] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[moonRelativePositionAngleSlotIndex];
    } else {
        double sunRightAscension;
        double sunDeclination;
        sunRAandDecl(_calculationDateInterval, &sunRightAscension, &sunDeclination, _currentCache);
        double moonRightAscension;
        double moonDeclination;
        double moonEclipticLongitude;
        moonRAAndDecl(_calculationDateInterval, &moonRightAscension, &moonDeclination, &moonEclipticLongitude, _currentCache);
        double posAngle = positionAngle(sunRightAscension, sunDeclination, moonRightAscension, moonDeclination);
        double phase;
        double moonAgeAngle = moonAge(_calculationDateInterval, &phase, _currentCache);
        if (moonAgeAngle > M_PI) { // bright limb on the left, sense of posAngle is reversed by 180
            if (posAngle > M_PI) {
                posAngle -= M_PI;
            } else {
                posAngle += M_PI;
            }
        }
        double gst = convertUTToGSTP03(_calculationDateInterval, _currentCache);
        double lst = convertGSTtoLST(gst, _observerLongitude);
        double moonHourAngle = lst - moonRightAscension;
        double sinAlt = sin(moonDeclination)*sin(_observerLatitude) + cos(moonDeclination)*cos(_observerLatitude)*cos(moonHourAngle);
        double moonAzimuth = atan2(-cos(moonDeclination)*cos(_observerLatitude)*sin(moonHourAngle), sin(moonDeclination) - sin(_observerLatitude)*sinAlt);
        double moonAltitude = asin(sinAlt);
        double northAngle = northAngleForObject(moonAltitude, moonAzimuth, _observerLatitude);
        angle = -northAngle - posAngle - M_PI/2;
        if (angle < 0) {
            angle += (M_PI * 2);
        } else if (angle > (M_PI * 2)) {
            angle -= (M_PI * 2);
        }
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[moonRelativePositionAngleSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[moonRelativePositionAngleSlotIndex] = angle;
        }
    }
    return angle;
}

double
ESAstronomyManager::moonRelativeAngle () { // rotation of moon image as it appears in the sky
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonRelativeAngleSlotIndex] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[moonRelativeAngleSlotIndex];
    } else {
        double moonRightAscension;
        double moonDeclination;
        double moonEclipticLongitude;
        moonRAAndDecl(_calculationDateInterval, &moonRightAscension, &moonDeclination, &moonEclipticLongitude, _currentCache);
        double gst = convertUTToGSTP03(_calculationDateInterval, _currentCache);
        double lst = convertGSTtoLST(gst, _observerLongitude);
        double moonHourAngle = lst - moonRightAscension;
        double sinAlt = sin(moonDeclination)*sin(_observerLatitude) + cos(moonDeclination)*cos(_observerLatitude)*cos(moonHourAngle);
        double moonAzimuth = atan2(-cos(moonDeclination)*cos(_observerLatitude)*sin(moonHourAngle), sin(moonDeclination) - sin(_observerLatitude)*sinAlt);
        double moonAltitude = asin(sinAlt);
        double northAngle = northAngleForObject(moonAltitude, moonAzimuth, _observerLatitude);

        // Approximate:
        double apparentGeocentricLongitude = moonRightAscension - gst;
        double apparentGeocentricLatitude = moonDeclination;

        // Meeus p373, "Position Angle of Axis"
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
        double eclipticTrueObliquity = generalObliquity(julianCenturiesSince2000Epoch);
        double longitudeOfAscendingNode = WB_MoonAscendingNodeLongitude(julianCenturiesSince2000Epoch, _currentCache);  // FIX: Add cache, although WB already caches it
        double W = apparentGeocentricLongitude - longitudeOfAscendingNode;
        double b = asin(-sin(W)*cos(apparentGeocentricLatitude)*kECsinMoonEquatorEclipticAngle - sin(apparentGeocentricLatitude) * kECcosMoonEquatorEclipticAngle);
        // Ignore physical librarions, for now (Meeus p 373, rho and sigma)
        double V = longitudeOfAscendingNode;
        double X = kECsinMoonEquatorEclipticAngle * sin(V);
        double Y = kECsinMoonEquatorEclipticAngle * cos(V) * cos(eclipticTrueObliquity) - kECcosMoonEquatorEclipticAngle * sin(eclipticTrueObliquity);
        double omega = atan2(X, Y);
        double sinP = sqrt(X * X + Y * Y) * cos(moonRightAscension - omega) / cos(b);
        double posAngle = asin(sinP);
#if 0
        printingEnabled = true;
        printAngle(posAngle, "posAngle");
        printingEnabled = false;
#endif
        angle = -northAngle - posAngle/* - M_PI/2*/;
        if (angle < 0) {
            angle += (M_PI * 2);
        } else if (angle > (M_PI * 2)) {
            angle -= (M_PI * 2);
        }
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[moonRelativeAngleSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[moonRelativeAngleSlotIndex] = angle;
        }
    }
    return angle;
}

double
ESAstronomyManager::sunRA () {
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[sunRASlotIndex] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[sunRASlotIndex];
    } else {
        double sunRightAscension;
        double sunDeclination;
        sunRAandDecl(_calculationDateInterval, &sunRightAscension, &sunDeclination, _currentCache);
        angle = sunRightAscension;
    }
    return angle;
}
double
ESAstronomyManager::sunRAJ2000 () {
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[sunRAJ2000SlotIndex] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[sunRAJ2000SlotIndex];
    } else {
        double sunRightAscension;
        double sunDeclination;
        sunRAandDeclJ2000(_calculationDateInterval, &sunRightAscension, &sunDeclination, _currentCache);
        angle = sunRightAscension;
    }
    return angle;
}
double
ESAstronomyManager::sunDecl () {
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[sunDeclSlotIndex] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[sunDeclSlotIndex];
    } else {
        double sunRightAscension;
        double sunDeclination;
        sunRAandDecl(_calculationDateInterval, &sunRightAscension, &sunDeclination, _currentCache);
        angle = sunDeclination;
    }
    return angle;
}
double
ESAstronomyManager::sunDeclJ2000 () {
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[sunDeclSlotIndex] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[sunDeclSlotIndex];
    } else {
        double sunRightAscension;
        double sunDeclination;
        sunRAandDeclJ2000(_calculationDateInterval, &sunRightAscension, &sunDeclination, _currentCache);
        angle = sunDeclination;
    }
    return angle;
}
double
ESAstronomyManager::moonRA () {
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonRASlotIndex] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[moonRASlotIndex];
    } else {
        double moonRightAscension;
        double moonDeclination;
        double moonEclipticLongitude;
        moonRAAndDecl(_calculationDateInterval, &moonRightAscension, &moonDeclination, &moonEclipticLongitude, _currentCache);
        angle = moonRightAscension;
    }
    return angle;
}
double
ESAstronomyManager::moonRAJ2000 () {
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonRASlotIndex] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[moonRASlotIndex];
    } else {
        double moonRightAscension;
        double moonDeclination;
        moonRAandDeclJ2000(_calculationDateInterval, &moonRightAscension, &moonDeclination, _currentCache);
        angle = moonRightAscension;
    }
    return angle;
}
double
ESAstronomyManager::moonDecl () {
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonDeclSlotIndex] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[moonDeclSlotIndex];
    } else {
        double moonRightAscension;
        double moonDeclination;
        double moonEclipticLongitude;
        moonRAAndDecl(_calculationDateInterval, &moonRightAscension, &moonDeclination, &moonEclipticLongitude, _currentCache);
        angle = moonDeclination;
    }
    return angle;
}
double
ESAstronomyManager::moonDeclJ2000 () {
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonDeclSlotIndex] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[moonDeclSlotIndex];
    } else {
        double moonRightAscension;
        double moonDeclination;
        moonRAandDeclJ2000(_calculationDateInterval, &moonRightAscension, &moonDeclination, _currentCache);
        angle = moonDeclination;
    }
    return angle;
}

// Note: planetAzimuth and planetAltitude correct for topocentric parallax.  For inner planets it improves the error in azimuth by a factor of 3 or so, by removing the topocentric error of approx half an arcsecond
double
ESAstronomyManager::planetAltitude(int planetNumber) {
    if (planetNumber < 0 || planetNumber > ECLastLegalPlanet || planetNumber == ECPlanetEarth) {
        return nan("");
    }
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double angle = planetAltAz(planetNumber, _calculationDateInterval, _observerLatitude, _observerLongitude, true/*correctForParallax*/, true/*altNotAz*/, _currentCache);
    return angle;
}
double
ESAstronomyManager::planetAzimuth(int planetNumber) {
    if (planetNumber < 0 || planetNumber > ECLastLegalPlanet || planetNumber == ECPlanetEarth) {
        return nan("");
    }
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double angle = planetAltAz(planetNumber, _calculationDateInterval, _observerLatitude, _observerLongitude, true/*correctForParallax*/, false/*!altNotAz*/, _currentCache);
    return angle;
}

double
ESAstronomyManager::planetAltitude(int            planetNumber,
                                   ESTimeInterval atDateInterval) {
    if (planetNumber < 0 || planetNumber > ECLastLegalPlanet || planetNumber == ECPlanetEarth) {
        return nan("");
    }
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double angle = planetAltAz(planetNumber, atDateInterval, _observerLatitude, _observerLongitude, true/*correctForParallax*/, true/*altNotAz*/, NULL);
    return angle;
}
double
ESAstronomyManager::planetAzimuth(int            planetNumber,
                                  ESTimeInterval atDateInterval) {
    if (planetNumber < 0 || planetNumber > ECLastLegalPlanet || planetNumber == ECPlanetEarth) {
        return nan("");
    }
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double angle = planetAltAz(planetNumber, atDateInterval, _observerLatitude, _observerLongitude, true/*correctForParallax*/, false/*!altNotAz*/, _currentCache);
    return angle;
}

// By "up" here, we mean past the calculated rise and before the calculated set
bool
ESAstronomyManager::planetIsUp(int planetNumber) {
    if (planetNumber < 0 || planetNumber > ECLastLegalPlanet || planetNumber == ECPlanetEarth) {
        return nan("");
    }
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    bool isUp;
    if (!_locationValid) {
        PRINT_STRING("planetIsUp returns NULL\n");
        //printf("planetIsUp returns NULL\n");
        isUp = false;
    } else {
        ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
        if (_currentCache && _currentCache->cacheSlotValidFlag[planetIsUpSlotIndex+planetNumber] == _currentCache->currentFlag) {
            isUp = (int) _currentCache->cacheSlots[planetIsUpSlotIndex+planetNumber];
        } else {
            double altitude = planetAltAz(planetNumber, _calculationDateInterval, _observerLatitude, _observerLongitude,
                                          true/*correctForParallax*/, true/*altNotAz*/, _currentCache);  // already incorporates topocentric parallax
            //printAngle(altitude, "planetIsUp altitude");
            double altAtRiseSet = altitudeAtRiseSet(julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache),
                                                    planetNumber, false/*!wantGeocentricAltitude*/, _currentCache, ECWBFullPrecision);
            //printAngle(altAtRiseSet, "...altAtRiseSet");
            isUp = altitude > altAtRiseSet;
            if (_currentCache) {
                _currentCache->cacheSlotValidFlag[planetIsUpSlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlots[planetIsUpSlotIndex+planetNumber] = (double) (isUp);
            }
        }
    }
    return isUp;
}

double
ESAstronomyManager::moonAzimuth () {
    return planetAzimuth(ECPlanetMoon);
}

double
ESAstronomyManager::moonAltitude () {
    return planetAltitude(ECPlanetMoon);
}

double
ESAstronomyManager::sunAzimuth () {
    return planetAzimuth(ECPlanetSun);
}

double
ESAstronomyManager::sunAltitude () {
    return planetAltitude(ECPlanetSun);
}

double
ESAstronomyManager::planetRA(int  planetNumber,
                             bool correctForParallax) {
    if (planetNumber < 0 || planetNumber > ECLastLegalPlanet || planetNumber == ECPlanetEarth) {
        return nan("");
    }
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    int slotIndexBase = correctForParallax ? planetRATopoSlotIndex : planetRASlotIndex;
    if (_currentCache && _currentCache->cacheSlotValidFlag[slotIndexBase+planetNumber] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[slotIndexBase+planetNumber];
    } else {
        double planetRightAscension;
        double planetDeclination;
        double planetGeocentricDistance;
        if (correctForParallax && _currentCache &&
            _currentCache->cacheSlotValidFlag[planetDeclSlotIndex+planetNumber] == _currentCache->currentFlag &&
            _currentCache->cacheSlotValidFlag[planetRASlotIndex+planetNumber] == _currentCache->currentFlag &&
            _currentCache->cacheSlotValidFlag[planetGeocentricDistanceSlotIndex+planetNumber] == _currentCache->currentFlag) {
            planetDeclination = _currentCache->cacheSlots[planetDeclSlotIndex+planetNumber];
            planetRightAscension = _currentCache->cacheSlots[planetRASlotIndex+planetNumber];
            planetGeocentricDistance = _currentCache->cacheSlots[planetGeocentricDistanceSlotIndex+planetNumber];
        } else {
            double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
            double planetEclipticLongitude;
            double planetEclipticLatitude;
            WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &planetEclipticLongitude, &planetEclipticLatitude, &planetGeocentricDistance,
                                      &planetRightAscension, &planetDeclination, _currentCache, ECWBFullPrecision);
            if (_currentCache) {
                _currentCache->cacheSlotValidFlag[planetEclipticLongitudeSlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlotValidFlag[planetEclipticLatitudeSlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlotValidFlag[planetDeclSlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlotValidFlag[planetRASlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlotValidFlag[planetGeocentricDistanceSlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlots[planetEclipticLongitudeSlotIndex+planetNumber] = planetEclipticLongitude;
                _currentCache->cacheSlots[planetEclipticLatitudeSlotIndex+planetNumber] = planetEclipticLatitude;
                _currentCache->cacheSlots[planetDeclSlotIndex+planetNumber] = planetDeclination;
                _currentCache->cacheSlots[planetRASlotIndex+planetNumber] = planetRightAscension;
                _currentCache->cacheSlots[planetGeocentricDistanceSlotIndex+planetNumber] = planetGeocentricDistance;;
            }
        }
        if (correctForParallax) {
            ESAssert(!_currentCache || _currentCache->cacheSlotValidFlag[planetRATopoSlotIndex+planetNumber] != _currentCache->currentFlag);  // Otherwise very first cache check should succeed
            double gst = convertUTToGSTP03(_calculationDateInterval, _currentCache);
            double lst = convertGSTtoLST(gst, _observerLongitude);
            double planetHourAngle = lst - planetRightAscension;
            double planetTopoRightAscension;
            double planetTopoDeclination;
            double planetTopoHourAngle;
            topocentricParallax(planetRightAscension, planetDeclination, planetHourAngle, planetGeocentricDistance, _observerLatitude, 0/*_observerAltitude*/,
                                &planetTopoHourAngle, &planetTopoDeclination);
            planetTopoRightAscension = lst - planetTopoHourAngle;
            if (planetTopoRightAscension < 0) {
                planetTopoRightAscension += M_PI * 2;
            }
            //EC_printAngle(planetRightAscension, "planetRightAscension");
            //EC_printAngle(planetTopoRightAscension, "planetTopoRightAscension");
            if (_currentCache) {
                _currentCache->cacheSlotValidFlag[planetDeclTopoSlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlotValidFlag[planetRATopoSlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlots[planetDeclTopoSlotIndex+planetNumber] = planetTopoDeclination;
                _currentCache->cacheSlots[planetRATopoSlotIndex+planetNumber] = planetTopoRightAscension;
            }
            angle = planetTopoRightAscension;
        } else {
            angle = planetRightAscension;
        }
    }
    return angle;
}

double
ESAstronomyManager::planetRA(int            planetNumber,
                             ESTimeInterval atTime,
                             bool           correctForParallax) {
    if (planetNumber < 0 || planetNumber > ECLastLegalPlanet || planetNumber == ECPlanetEarth) {
        return nan("");
    }
    double planetRightAscension;
    double planetDeclination;
    double planetGeocentricDistance;
    ECAstroCache *priorCache = pushECAstroCacheWithSlopInPool(_astroCachePool, &_astroCachePool->refinementCache, atTime, 0);
    double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(atTime, NULL, _astroCachePool->currentCache);
    double planetEclipticLongitude;
    double planetEclipticLatitude;
    WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &planetEclipticLongitude, &planetEclipticLatitude, &planetGeocentricDistance,
                              &planetRightAscension, &planetDeclination, _astroCachePool->currentCache, ECWBFullPrecision);

    popECAstroCacheToInPool(_astroCachePool, priorCache);
    if (correctForParallax) {
        double gst = convertUTToGSTP03(atTime, _currentCache);
        double lst = convertGSTtoLST(gst, _observerLongitude);
        double planetHourAngle = lst - planetRightAscension;
        double planetTopoRightAscension;
        double planetTopoDeclination;
        double planetTopoHourAngle;
        topocentricParallax(planetRightAscension, planetDeclination, planetHourAngle, planetGeocentricDistance, _observerLatitude, 0/*observerAltitude*/,
                        &planetTopoHourAngle, &planetTopoDeclination);
        planetTopoRightAscension = lst - planetTopoHourAngle;
        if (planetTopoRightAscension < 0) {
            planetTopoRightAscension += M_PI * 2;
        }
        //EC_printAngle(planetRightAscension, "planetRightAscension");
        //EC_printAngle(planetTopoRightAscension, "planetTopoRightAscension");
        return planetTopoRightAscension;
    } else {
        return planetRightAscension;
    }
}

double
ESAstronomyManager::planetDecl(int  planetNumber,
                               bool correctForParallax) {
    if (planetNumber < 0 || planetNumber > ECLastLegalPlanet || planetNumber == ECPlanetEarth) {
        return nan("");
    }
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    int slotIndexBase = correctForParallax ? planetDeclTopoSlotIndex : planetDeclSlotIndex;
    if (_currentCache && _currentCache->cacheSlotValidFlag[slotIndexBase+planetNumber] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[slotIndexBase+planetNumber];
    } else {
        double planetRightAscension;
        double planetDeclination;
        double planetGeocentricDistance;
        if (correctForParallax && _currentCache &&
            _currentCache->cacheSlotValidFlag[planetDeclSlotIndex+planetNumber] == _currentCache->currentFlag &&
            _currentCache->cacheSlotValidFlag[planetRASlotIndex+planetNumber] == _currentCache->currentFlag &&
            _currentCache->cacheSlotValidFlag[planetGeocentricDistanceSlotIndex+planetNumber] == _currentCache->currentFlag) {
            planetDeclination = _currentCache->cacheSlots[planetDeclSlotIndex+planetNumber];
            planetRightAscension = _currentCache->cacheSlots[planetRASlotIndex+planetNumber];
            planetGeocentricDistance = _currentCache->cacheSlots[planetGeocentricDistanceSlotIndex+planetNumber];
        } else {
            double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
            double planetEclipticLongitude;
            double planetEclipticLatitude;
            WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &planetEclipticLongitude, &planetEclipticLatitude, &planetGeocentricDistance, &planetRightAscension, &planetDeclination, _currentCache, ECWBFullPrecision);
            if (_currentCache) {
                _currentCache->cacheSlotValidFlag[planetEclipticLongitudeSlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlotValidFlag[planetEclipticLatitudeSlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlotValidFlag[planetDeclSlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlotValidFlag[planetRASlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlotValidFlag[planetGeocentricDistanceSlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlots[planetEclipticLongitudeSlotIndex+planetNumber] = planetEclipticLongitude;
                _currentCache->cacheSlots[planetEclipticLatitudeSlotIndex+planetNumber] = planetEclipticLatitude;
                _currentCache->cacheSlots[planetDeclSlotIndex+planetNumber] = planetDeclination;
                _currentCache->cacheSlots[planetRASlotIndex+planetNumber] = planetRightAscension;
                _currentCache->cacheSlots[planetGeocentricDistanceSlotIndex+planetNumber] = planetGeocentricDistance;;
            }
        }
        if (correctForParallax) {
            ESAssert(!_currentCache || _currentCache->cacheSlotValidFlag[planetDeclTopoSlotIndex+planetNumber] != _currentCache->currentFlag);  // Otherwise very first cache check should succeed
            double gst = convertUTToGSTP03(_calculationDateInterval, _currentCache);
            double lst = convertGSTtoLST(gst, _observerLongitude);
            double planetHourAngle = lst - planetRightAscension;
            double planetTopoRightAscension;
            double planetTopoDeclination;
            topocentricParallax(planetRightAscension, planetDeclination, planetHourAngle, planetGeocentricDistance, _observerLatitude, 0/*observerAltitude*/,
                                &planetTopoRightAscension, &planetTopoDeclination);
            if (_currentCache) {
                _currentCache->cacheSlotValidFlag[planetDeclTopoSlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlotValidFlag[planetRATopoSlotIndex+planetNumber] = _currentCache->currentFlag;
                _currentCache->cacheSlots[planetDeclTopoSlotIndex+planetNumber] = planetTopoDeclination;
                _currentCache->cacheSlots[planetRATopoSlotIndex+planetNumber] = planetTopoRightAscension;
            }
            angle = planetTopoDeclination;
        } else {
            angle = planetDeclination;
        }
    }
    return angle;
}

double
ESAstronomyManager::planetEclipticLongitude(int planetNumber) {
    if (planetNumber < 0 || planetNumber > ECLastLegalPlanet || planetNumber == ECPlanetEarth) {
        return nan("");
    }
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[planetEclipticLongitudeSlotIndex+planetNumber] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[planetEclipticLongitudeSlotIndex+planetNumber];
    } else {
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
        double planetRightAscension;
        double planetDeclination;
        double planetEclipticLongitude;
        double planetEclipticLatitude;
        double planetGeocentricDistance;
        WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &planetEclipticLongitude, &planetEclipticLatitude, &planetGeocentricDistance, &planetRightAscension, &planetDeclination, _currentCache, ECWBFullPrecision);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[planetEclipticLongitudeSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[planetEclipticLatitudeSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[planetDeclSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[planetRASlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[planetGeocentricDistanceSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlots[planetEclipticLongitudeSlotIndex+planetNumber] = planetEclipticLongitude;
            _currentCache->cacheSlots[planetEclipticLatitudeSlotIndex+planetNumber] = planetEclipticLatitude;
            _currentCache->cacheSlots[planetDeclSlotIndex+planetNumber] = planetDeclination;
            _currentCache->cacheSlots[planetRASlotIndex+planetNumber] = planetRightAscension;
            _currentCache->cacheSlots[planetGeocentricDistanceSlotIndex+planetNumber] = planetGeocentricDistance;;
        }
        angle = planetEclipticLongitude;
    }
    return angle;
}

double
ESAstronomyManager::planetEclipticLatitude(int planetNumber) {
    if (planetNumber < 0 || planetNumber > ECLastLegalPlanet || planetNumber == ECPlanetEarth) {
        return nan("");
    }
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[planetEclipticLatitudeSlotIndex+planetNumber] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[planetEclipticLatitudeSlotIndex+planetNumber];
    } else {
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
        double planetRightAscension;
        double planetDeclination;
        double planetEclipticLongitude;
        double planetEclipticLatitude;
        double planetGeocentricDistance;
        WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &planetEclipticLongitude, &planetEclipticLatitude, &planetGeocentricDistance, &planetRightAscension, &planetDeclination, _currentCache, ECWBFullPrecision);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[planetEclipticLongitudeSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[planetEclipticLatitudeSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[planetDeclSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[planetRASlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[planetGeocentricDistanceSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlots[planetEclipticLongitudeSlotIndex+planetNumber] = planetEclipticLongitude;
            _currentCache->cacheSlots[planetEclipticLatitudeSlotIndex+planetNumber] = planetEclipticLatitude;
            _currentCache->cacheSlots[planetDeclSlotIndex+planetNumber] = planetDeclination;
            _currentCache->cacheSlots[planetRASlotIndex+planetNumber] = planetRightAscension;
            _currentCache->cacheSlots[planetGeocentricDistanceSlotIndex+planetNumber] = planetGeocentricDistance;;
        }
        angle = planetEclipticLatitude;
    }
    return angle;
}

double
ESAstronomyManager::planetGeocentricDistance(int planetNumber) {  // in AU
    if (planetNumber < 0 || planetNumber > ECLastLegalPlanet || planetNumber == ECPlanetEarth) {
        return nan("");
    }
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double distance;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[planetGeocentricDistanceSlotIndex+planetNumber] == _currentCache->currentFlag) {
        distance = _currentCache->cacheSlots[planetGeocentricDistanceSlotIndex+planetNumber];
    } else {
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
        double planetRightAscension;
        double planetDeclination;
        double planetEclipticLongitude;
        double planetEclipticLatitude;
        double planetGeocentricDistance;
        WB_planetApparentPosition(planetNumber, julianCenturiesSince2000Epoch/100, &planetEclipticLongitude, &planetEclipticLatitude, &planetGeocentricDistance, &planetRightAscension, &planetDeclination, _currentCache, ECWBFullPrecision);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[planetEclipticLongitudeSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[planetEclipticLatitudeSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[planetDeclSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[planetRASlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[planetGeocentricDistanceSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlots[planetEclipticLongitudeSlotIndex+planetNumber] = planetEclipticLongitude;
            _currentCache->cacheSlots[planetEclipticLatitudeSlotIndex+planetNumber] = planetEclipticLatitude;
            _currentCache->cacheSlots[planetDeclSlotIndex+planetNumber] = planetDeclination;
            _currentCache->cacheSlots[planetRASlotIndex+planetNumber] = planetRightAscension;
            _currentCache->cacheSlots[planetGeocentricDistanceSlotIndex+planetNumber] = planetGeocentricDistance;;
        }
        distance = planetGeocentricDistance;
    }
    return distance;
}

double
ESAstronomyManager::planetMass(int n) {
    return planetMassInKG[n];                   // kilograms
}

double
ESAstronomyManager::planetOribitalPeriod(int n) {
    return planetOrbitalPeriodInYears[n];       // years
}

double
ESAstronomyManager::planetRadius(int n) {
    return planetRadiiInAU[n] * kECAUInKilometers;                              // kilometers
}

double
ESAstronomyManager::planetApparentDiameter(int n) {
    return atan((planetRadiiInAU[n])/planetGeocentricDistance(n))*2;    // radians
}

void
ESAstronomyManager::calculateHighestEcliptic () {
    double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
    double nutation;
    double obliquity;
    WB_nutationObliquity(julianCenturiesSince2000Epoch/100, &nutation, &obliquity, _currentCache);
    double gst = convertUTToGSTP03(_calculationDateInterval, _currentCache);
    double lst = convertGSTtoLST(gst, _observerLongitude);
    double sinObliquity = sin(obliquity);
    double cosObliquity = cos(obliquity);
    double sinLst = sin(lst);
    double cosObsLat = cos(_observerLatitude);
    double sinObsLat = sin(_observerLatitude);
    double eclipticLongitude = atan2(-cos(lst), sinObliquity*tan(_observerLatitude) + cosObliquity*sinLst);  // longitude at horizon
    eclipticLongitude += M_PI/2;  // guess + rather than -
    double sinEclipLong = sin(eclipticLongitude);
    double declination = asin(sinObliquity * sinEclipLong);
    double rightAscension = atan2(cosObliquity * sinEclipLong, cos(eclipticLongitude));
    double hourAngle = lst - rightAscension;
    double sinAlt = sin(declination)*sinObsLat + cos(declination)*cosObsLat*cos(hourAngle);

    double azimuth = atan2(-cos(declination)*cosObsLat*sin(hourAngle), sin(declination) - sinObsLat*sinAlt);

    // Check if we guessed right by checking altitude: If +, we got it right
    if (sinAlt < 0) { // guessed wrong
        azimuth = ESUtil::fmod(azimuth + M_PI, (M_PI * 2));
        eclipticLongitude = ESUtil::fmod(eclipticLongitude + M_PI, (M_PI * 2));
    } else { // guessed right
        azimuth = ESUtil::fmod(azimuth, (M_PI * 2));
        eclipticLongitude = ESUtil::fmod(eclipticLongitude, (M_PI * 2));
    }
    if (azimuth < 0) {
        azimuth += (M_PI * 2);
    }
    if (eclipticLongitude < 0) {
        eclipticLongitude += (M_PI * 2);
    }

    // Now calculate ecliptic longitude of north meridian, which is the location for which the azimuth is 0 and the ecliptic latitude is 0
    // Note cos(azimuth) = 1, sin(azimuth) = 0
    // The hourAngle is 0 or 180, depending on .... sign of sinAlt - sinObsLat*sinDecl?  but we only care about tan(HA) which ignores the +180
    // Call it zero, so RA = lst - HA = lst
    double meridianRA = lst;
    double longitudeOfEclipticMeridian = atan(tan(meridianRA) / cosObliquity);
    // This is the longitude of the meridian that intersects the half of the ecliptic with positive altitude.  But we want the north one, which might be the other one
    // Also, we must follow the quadrant of the meridianRA
    bool flipBecauseOfRA = (cos(meridianRA) > 0);
    bool flipBecauseOfAzimuth = _observerLatitude > 0
        ? (cos(azimuth) > 0) && _observerLatitude < M_PI / 4
        : (cos(azimuth) > 0) || _observerLatitude < -M_PI / 4;
        
    if (flipBecauseOfRA != flipBecauseOfAzimuth) { // either is on but not both where they cancel each other out
        longitudeOfEclipticMeridian -= M_PI;
    }
    if (longitudeOfEclipticMeridian < 0) {
        longitudeOfEclipticMeridian += (M_PI * 2);
    }
    double eclipticAltitude = acos(cosObliquity*sinObsLat - sinObliquity*cosObsLat*sinLst);
    //printAngle(eclipticLongitude, "longitude of highest ecliptic");
    //printAngle(azimuth, "azimuth of highest ecliptic");
    //printAngle(eclipticAltitude, "altitude of ecliptic");
    //printAngle(longitudeOfEclipticMeridian, "longitudeOfEclipticMeridian");
    if (_currentCache) {
        _currentCache->cacheSlotValidFlag[azimuthOfHighestEclipticSlotIndex] = _currentCache->currentFlag;
        _currentCache->cacheSlotValidFlag[longitudeOfHighestEclipticSlotIndex] = _currentCache->currentFlag;
        _currentCache->cacheSlotValidFlag[eclipticAltitudeSlotIndex] = _currentCache->currentFlag;
        _currentCache->cacheSlotValidFlag[longitudeOfEclipticMeridianSlotIndex] = _currentCache->currentFlag;
        _currentCache->cacheSlots[azimuthOfHighestEclipticSlotIndex] = azimuth;
        _currentCache->cacheSlots[longitudeOfHighestEclipticSlotIndex] = eclipticLongitude;
        _currentCache->cacheSlots[eclipticAltitudeSlotIndex] = eclipticAltitude;
        _currentCache->cacheSlots[longitudeOfEclipticMeridianSlotIndex] = longitudeOfEclipticMeridian;
    }
}

double
ESAstronomyManager::azimuthOfHighestEclipticAltitude () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(_currentCache);
    double angle;
    ESAssert(fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache->cacheSlotValidFlag[azimuthOfHighestEclipticSlotIndex] != _currentCache->currentFlag) {
        calculateHighestEcliptic();
    }
    angle = _currentCache->cacheSlots[azimuthOfHighestEclipticSlotIndex];
    return angle;
}

double
ESAstronomyManager::longitudeOfHighestEclipticAltitude () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(_currentCache);
    double angle;
    ESAssert(fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache->cacheSlotValidFlag[longitudeOfHighestEclipticSlotIndex] != _currentCache->currentFlag) {
        calculateHighestEcliptic();
    }
    angle = _currentCache->cacheSlots[longitudeOfHighestEclipticSlotIndex];
    return angle;
}

double
ESAstronomyManager::longitudeAtNorthMeridian () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(_currentCache);
    double angle;
    ESAssert(fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache->cacheSlotValidFlag[longitudeOfEclipticMeridianSlotIndex] != _currentCache->currentFlag) {
        calculateHighestEcliptic();
    }
    angle = _currentCache->cacheSlots[longitudeOfEclipticMeridianSlotIndex];
    return angle;
}

double
ESAstronomyManager::eclipticAltitude () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(_currentCache);
    double angle;
    ESAssert(fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache->cacheSlotValidFlag[eclipticAltitudeSlotIndex] != _currentCache->currentFlag) {
        calculateHighestEcliptic();
    }
    angle = _currentCache->cacheSlots[eclipticAltitudeSlotIndex];
    return angle;
}

// Amount the sidereal time coordinate system has rotated around since the autumnal equinox
double
ESAstronomyManager::vernalEquinoxAngle () {
    double angle;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[vernalEquinoxSlotIndex] == _currentCache->currentFlag) {
        angle = _currentCache->cacheSlots[vernalEquinoxSlotIndex];
    } else {
        angle = STDifferenceForDate(_calculationDateInterval, _currentCache);
        //printDateD(_calculationDateInterval, "vernalEquinoxAngle date");
        //printAngle(angle, "vernalEquinoxAngle");
        //double eclipLong = sunEclipticLongitudeForDate(_calculationDateInterval, _currentCache);
        //printAngle(eclipLong, "sunEclipticLong");
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[vernalEquinoxSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[vernalEquinoxSlotIndex] = angle;
        }
    }
    return angle;
}

static double
timeOfClosestSunEclipticLongitude(double       targetSunLong,
                                  double       tryDate,
                                  ECAstroCache *_currentCache) {

    double sunLongitudeForTryDate = sunEclipticLongitudeForDate(tryDate, _currentCache);
    double howFarAway = targetSunLong - sunLongitudeForTryDate;
    double deltaAngleToTarget;
    if (howFarAway >= 0) {
        if (howFarAway >= M_PI) {
            deltaAngleToTarget = howFarAway - (M_PI * 2);
        } else {
            deltaAngleToTarget = howFarAway;
        }
    } else if (howFarAway >= - M_PI) {
        deltaAngleToTarget = howFarAway;
    } else {
        deltaAngleToTarget = howFarAway + (M_PI * 2);
    }
    double returnDate = tryDate + deltaAngleToTarget * kECSecondsInTropicalYear / (M_PI * 2);
    return returnDate;
}

static ESTimeInterval refineClosestEclipticLongitude(int              longitudeQuarter,
                                                     ESTimeInterval   dateInterval,
                                                     ECAstroCachePool *cachePool) {
    double targetSunLongitude = longitudeQuarter * M_PI / 2;
    double tryDate = timeOfClosestSunEclipticLongitude(targetSunLongitude, dateInterval, cachePool->currentCache);
    ECAstroCache *priorCache = pushECAstroCacheWithSlopInPool(cachePool, &cachePool->refinementCache, tryDate, 0);
    tryDate = timeOfClosestSunEclipticLongitude(targetSunLongitude, tryDate, cachePool->currentCache);
    popECAstroCacheToInPool(cachePool, priorCache);
    priorCache = pushECAstroCacheWithSlopInPool(cachePool, &cachePool->refinementCache, tryDate, 0);
    tryDate = timeOfClosestSunEclipticLongitude(targetSunLongitude, tryDate, cachePool->currentCache);
    popECAstroCacheToInPool(cachePool, priorCache);
    priorCache = pushECAstroCacheWithSlopInPool(cachePool, &cachePool->refinementCache, tryDate, 0);
    ESTimeInterval closestTime = timeOfClosestSunEclipticLongitude(targetSunLongitude, tryDate, cachePool->currentCache);
    popECAstroCacheToInPool(cachePool, priorCache);
    return closestTime;
}

ESTimeInterval
ESAstronomyManager::refineTimeOfClosestSunEclipticLongitude(int longitudeQuarter) {  // 0 => long==0, 1 => long==PI/2, etc
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval closestTime;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    ESAssert(longitudeQuarter >= 0 && longitudeQuarter <= 3);
    int slotIndex = closestSunEclipticLongitudeSlotIndex + longitudeQuarter;
    if (_currentCache && _currentCache->cacheSlotValidFlag[slotIndex] == _currentCache->currentFlag) {
        closestTime = _currentCache->cacheSlots[slotIndex];
    } else {
        closestTime = refineClosestEclipticLongitude(longitudeQuarter, _calculationDateInterval, _astroCachePool);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[slotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[slotIndex] = closestTime;
        }
    }
    return closestTime;
}

double
ESAstronomyManager::closestSunEclipticLongitudeQuarter366IndicatorAngle(int longitudeQuarter) {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double indicatorAngle;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    ESAssert(longitudeQuarter >= 0 && longitudeQuarter <= 3);
    int slotIndex = closestSunEclipticLongIndicatorAngleSlotIndex + longitudeQuarter;
    if (_currentCache && _currentCache->cacheSlotValidFlag[slotIndex] == _currentCache->currentFlag) {
        indicatorAngle = _currentCache->cacheSlots[slotIndex];
    } else {
        double targetTime = refineClosestEclipticLongitude(longitudeQuarter, _calculationDateInterval, _astroCachePool);
        ESWatchTime *targetTimer = watchTimeForInterval(targetTime);
        indicatorAngle = targetTimer->year366IndicatorFractionUsingEnv(_environment) * (M_PI * 2);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[slotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[slotIndex] = indicatorAngle;
        }
    }
    return indicatorAngle;
}

ESTimeInterval
ESAstronomyManager::meridianTimeForSeason () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval meridianTime;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[meridianTimeSlotIndex] == _currentCache->currentFlag) {
        meridianTime = _currentCache->cacheSlots[meridianTimeSlotIndex];
    } else {
        // Get date for midnight on this day
        ESDateComponents cs;
        ESCalendar_localDateComponentsFromTimeInterval(_calculationDateInterval, _estz, &cs);
        cs.hour = 0;
        cs.minute = 0;
        cs.seconds = 0;
        ESTimeInterval midnightD = ESCalendar_timeIntervalFromLocalDateComponents(_estz, &cs);
        // calculate meridian time in seconds from local noon
        double eot = ::EOT(_calculationDateInterval, _astroCachePool) * 3600 * 12 / M_PI;
        double tzOffset = _watchTime->tzOffsetUsingEnv(_environment);
        double longitudeOffset = _observerLongitude * 3600 * 12 / M_PI;
        double meridianOffset = tzOffset - longitudeOffset - eot;
        // If summer, interesting time is midnight; if winter, it's noon
        if (isSummer(_calculationDateInterval, _observerLatitude, _currentCache)) {
            if (meridianOffset < 0) {
                meridianOffset += 24 * 3600;
            }
        } else {
            meridianOffset += 12 * 3600;
        }
        // Apply meridianOffset to midnight
        meridianTime = midnightD + meridianOffset;
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[meridianTimeSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[meridianTimeSlotIndex] = meridianTime;
        }
    }
    return meridianTime;
}

ESTimeInterval
ESAstronomyManager::moonMeridianTimeForSeason () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval meridianTime;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonMeridianTimeSlotIndex] == _currentCache->currentFlag) {
        meridianTime = _currentCache->cacheSlots[moonMeridianTimeSlotIndex];
    } else {
        // Get date for midnight on this day
        ESDateComponents cs;
        ESCalendar_UTCDateComponentsFromTimeInterval(_calculationDateInterval, &cs);
        cs.hour = 0;
        cs.minute = 0;
        cs.seconds = 0;
        ESTimeInterval midnightD = ESCalendar_timeIntervalFromLocalDateComponents(_estz, &cs);
        double meridianOffset = 0;
        if (moonIsSummer(_calculationDateInterval, _observerLatitude, _currentCache)) {
            meridianOffset = 12 * 3600;
        }
        meridianTime = midnightD + meridianOffset;
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[moonMeridianTimeSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[moonMeridianTimeSlotIndex] = meridianTime;
        }
    }
    return meridianTime;
}

double
ESAstronomyManager::planetMeridianTimeForSeason(int planetNumber) {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESTimeInterval meridianTime;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[planetMeridianTimeSlotIndex+planetNumber] == _currentCache->currentFlag) {
        meridianTime = _currentCache->cacheSlots[planetMeridianTimeSlotIndex+planetNumber];
    } else {
        // Get date for midnight on this day
        ESDateComponents cs;
        ESCalendar_UTCDateComponentsFromTimeInterval(_calculationDateInterval, &cs);
        cs.hour = 0;
        cs.minute = 0;
        cs.seconds = 0;
        ESTimeInterval midnightD = ESCalendar_timeIntervalFromLocalDateComponents(_estz, &cs);
        double meridianOffset = 0;
        if (::planetIsSummer(_calculationDateInterval, _observerLatitude, planetNumber, _currentCache)) {
            meridianOffset = 12 * 3600;
        }
        meridianTime = midnightD + meridianOffset;
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[planetMeridianTimeSlotIndex+planetNumber] = _currentCache->currentFlag;
            _currentCache->cacheSlots[planetMeridianTimeSlotIndex+planetNumber] = meridianTime;
        }
    }
    return meridianTime;
}

double
ESAstronomyManager::moonAscendingNodeLongitude () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double longitude;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonAscendingNodeLongitudeSlotIndex] == _currentCache->currentFlag) {
        longitude = _currentCache->cacheSlots[moonAscendingNodeLongitudeSlotIndex];
    } else {
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
        longitude = WB_MoonAscendingNodeLongitude(julianCenturiesSince2000Epoch, _currentCache);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[moonAscendingNodeLongitudeSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[moonAscendingNodeLongitudeSlotIndex] = longitude;
        }
    }
    return longitude;
}

double
ESAstronomyManager::moonAscendingNodeRA () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double RA;
    double longitude;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonAscendingNodeRASlotIndex] == _currentCache->currentFlag) {
        RA = _currentCache->cacheSlots[moonAscendingNodeRASlotIndex];
    } else {
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
        if (_currentCache && _currentCache->cacheSlotValidFlag[moonAscendingNodeLongitudeSlotIndex] == _currentCache->currentFlag) {
            longitude = _currentCache->cacheSlots[moonAscendingNodeLongitudeSlotIndex];
        } else {
            longitude = WB_MoonAscendingNodeLongitude(julianCenturiesSince2000Epoch, _currentCache);
            if (_currentCache) {
                _currentCache->cacheSlotValidFlag[moonAscendingNodeLongitudeSlotIndex] = _currentCache->currentFlag;
                _currentCache->cacheSlots[moonAscendingNodeLongitudeSlotIndex] = longitude;
            }
        }
        double obliquity;
        double nutation;
        WB_nutationObliquity(julianCenturiesSince2000Epoch/100,
                             &nutation,
                             &obliquity, _currentCache);
        double decl;
        raAndDeclO(0 /*eclipticLatitude*/, longitude, obliquity, &RA, &decl);
        if (RA < 0) {
            RA += 2 * M_PI;
        }
        //printAngle(longitude, "ascending node longitude");
        //printAngle(RA, "ascending node RA");
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[moonAscendingNodeRASlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[moonAscendingNodeRASlotIndex] = RA;
            _currentCache->cacheSlotValidFlag[moonAscendingNodeDeclSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[moonAscendingNodeDeclSlotIndex] = decl;
        }
    }
    return RA;
}

double
ESAstronomyManager::moonAscendingNodeRAJ2000 () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double RA;
    double longitude;
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache && _currentCache->cacheSlotValidFlag[moonAscendingNodeRAJ2000SlotIndex] == _currentCache->currentFlag) {
        RA = _currentCache->cacheSlots[moonAscendingNodeRAJ2000SlotIndex];
    } else {
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
        if (_currentCache && _currentCache->cacheSlotValidFlag[moonAscendingNodeLongitudeSlotIndex] == _currentCache->currentFlag) {
            longitude = _currentCache->cacheSlots[moonAscendingNodeLongitudeSlotIndex];
        } else {
            longitude = WB_MoonAscendingNodeLongitude(julianCenturiesSince2000Epoch, _currentCache);
            if (_currentCache) {
                _currentCache->cacheSlotValidFlag[moonAscendingNodeLongitudeSlotIndex] = _currentCache->currentFlag;
                _currentCache->cacheSlots[moonAscendingNodeLongitudeSlotIndex] = longitude;
            }
        }
        double obliquity;
        double nutation;
        WB_nutationObliquity(julianCenturiesSince2000Epoch/100,
                             &nutation,
                             &obliquity, _currentCache);
        double declOfDate;
        double raOfDate;
        raAndDeclO(0 /*eclipticLatitude*/, longitude, obliquity, &raOfDate, &declOfDate);
        if (raOfDate < 0) {
            raOfDate += 2 * M_PI;
        }
        double decl;
        refineConvertToJ2000FromOfDate(julianCenturiesSince2000Epoch, raOfDate, declOfDate, &RA, &decl);
        //printAngle(longitude, "ascending node longitude");
        //printAngle(RA, "ascending node RA");
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[moonAscendingNodeRAJ2000SlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[moonAscendingNodeRAJ2000SlotIndex] = RA;
            _currentCache->cacheSlotValidFlag[moonAscendingNodeDeclJ2000SlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[moonAscendingNodeDeclJ2000SlotIndex] = decl;
        }
    }
    return RA;
}

#if 0 // THIS FUNCTION IS VERY BROKEN, DON'T HAVE TIME TO FIX IT
// Note: shadow diameter goes negative past the tip of the shadow cone
static double
shadowDiameter(double lightSourceDiameter,
               double castingObjectDiameter,
               double distanceFromLightSourceToCastingObject,
               double distanceFromCastingObjectToShadow) {
    double lengthOfShadowCone = castingObjectDiameter * distanceFromLightSourceToCastingObject/(lightSourceDiameter - castingObjectDiameter);
    double halfConeAngle = asin(castingObjectDiameter/distanceFromLightSourceToCastingObject);
    double shadowDiam = castingObjectDiameter * (lengthOfShadowCone - distanceFromCastingObjectToShadow) / (lengthOfShadowCone * cos(halfConeAngle));
    return shadowDiam;
}
#endif

static double
umbralAngularRadius(double moonParallax,
                    double sunAngularRadius,
                    double sunParallax) {
    return 1.01 * moonParallax - sunAngularRadius + sunParallax;
}

// This formula works well for small separation values, unlike ones that end with acos
static double
angularSeparation(double rightAscension1,
                  double declination1,
                  double rightAscension2,
                  double declination2) {
    double sinDecl1 = sin(declination1);
    double cosDecl1 = cos(declination1);
    double sinDecl2 = sin(declination2);
    double cosDecl2 = cos(declination2);
    double sinRADelta = sin(rightAscension2 - rightAscension1);
    double cosRADelta = cos(rightAscension2 - rightAscension1);
    double x = cosDecl1 * sinDecl2 - sinDecl1 * cosDecl2 * cosRADelta;
    double y = cosDecl2 * sinRADelta;
    double z = sinDecl1 * sinDecl2 + cosDecl1 * cosDecl2 * cosRADelta;
    return atan2(sqrt(x*x + y*y), z);
}

#if 0  // unused
#ifndef NDEBUG
static const char *nameOfEclipseKind(ECEclipseKind kind) {
    switch (kind) {
      case ECEclipseNoneSolar:
        return "ECEclipseNoneSolar";
      case ECEclipseNoneLunar:
        return "ECEclipseNoneLunar";
      case ECEclipseSolarNotUp:
        return "ECEclipseSolarNotUp";
      case ECEclipsePartialSolar:
        return "ECEclipsePartialSolar";
      case ECEclipseAnnularSolar:
        return "ECEclipseAnnularSolar";
      case ECEclipseTotalSolar:
        return "ECEclipseTotalSolar";
      case ECEclipseLunarNotUp:
        return "ECEclipseLunarNotUp";
      case ECEclipsePartialLunar:
        return "ECEclipsePartialLunar";
      case ECEclipseTotalLunar:
        return "ECEclipseTotalLunar";
      default:
        return "Bogus EclipseKind";
    }
}
#endif  // NDEBUG
#endif  // if 0

static void
calculateEclipse(ESTimeInterval _calculationDateInterval,
                 double         observerLatitude,
                 double         observerLongitude,
                 double         *abstractSeparation,
                 double         *angularSep,
                 double         *shadowAngularSize,
                 ECEclipseKind  *eclipseKind,
                 ECAstroCache   *_currentCache) {
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    if (_currentCache
        && _currentCache->cacheSlotValidFlag[eclipseSeparationSlotIndex] == _currentCache->currentFlag
        && _currentCache->cacheSlotValidFlag[eclipseKindSlotIndex] == _currentCache->currentFlag) {
        *abstractSeparation = _currentCache->cacheSlots[eclipseSeparationSlotIndex];
        *angularSep = _currentCache->cacheSlots[eclipseAngularSeparationSlotIndex];
        *eclipseKind = (ECEclipseKind)rint(_currentCache->cacheSlots[eclipseKindSlotIndex]);
        *shadowAngularSize = _currentCache->cacheSlots[eclipseShadowAngularSizeSlotIndex];
    } else {
        double gst = convertUTToGSTP03(_calculationDateInterval, _currentCache);
        double lst = convertGSTtoLST(gst, observerLongitude);
        double julianCenturiesSince2000Epoch = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
        double sunRightAscension;
        double sunDeclination;
        double sunEclipticLongitude;
        double sunEclipticLatitude;
        double sunGeocentricDistance;
        WB_planetApparentPosition(ECPlanetSun, julianCenturiesSince2000Epoch/100, &sunEclipticLongitude, &sunEclipticLatitude, &sunGeocentricDistance, &sunRightAscension, &sunDeclination, _currentCache, ECWBFullPrecision);
        double sunAngularSize;
        double sunParallax;
        planetSizeAndParallax(ECPlanetSun, sunGeocentricDistance, &sunAngularSize, &sunParallax);
        double moonRightAscension;
        double moonDeclination;
        double moonEclipticLongitude;
        double moonEclipticLatitude;
        double moonGeocentricDistance;
        WB_planetApparentPosition(ECPlanetMoon, julianCenturiesSince2000Epoch/100, &moonEclipticLongitude, &moonEclipticLatitude, &moonGeocentricDistance, &moonRightAscension, &moonDeclination, _currentCache, ECWBFullPrecision);
        double moonAngularSize;
        double moonParallax;
        planetSizeAndParallax(ECPlanetMoon, moonGeocentricDistance, &moonAngularSize, &moonParallax);
        // Quick check:
        double raDelta = ESUtil::fmod(fabs(moonRightAscension - sunRightAscension), M_PI * 2);
        double physicalSeparation;
        double separationAtPartialEclipse;
        double separationAtTotalEclipse;
        bool solarNotLunar;
        if (raDelta < M_PI / 2) { // might be solar
            double sunHourAngle = lst - sunRightAscension;
            double sunTopoHourAngle;
            double sunTopoDecl;
            topocentricParallax(sunRightAscension, sunDeclination, sunHourAngle, sunGeocentricDistance, observerLatitude, 0/*observerAltitude*/, &sunTopoHourAngle, &sunTopoDecl);
            double sunTopoRA = lst - sunTopoHourAngle;
            
            double moonHourAngle = lst - moonRightAscension;
            double moonTopoHourAngle;
            double moonTopoDecl;
            topocentricParallax(moonRightAscension, moonDeclination, moonHourAngle, moonGeocentricDistance, observerLatitude, 0/*observerAltitude*/, &moonTopoHourAngle, &moonTopoDecl);
            double moonTopoRA = lst - moonTopoHourAngle;
            
            physicalSeparation = angularSeparation(sunTopoRA, sunTopoDecl, moonTopoRA, moonTopoDecl);
            separationAtPartialEclipse        =  sunAngularSize / 2 + moonAngularSize / 2;
            separationAtTotalEclipse          = moonAngularSize / 2 - sunAngularSize / 2;  // might be negative (no total)
            double separationAtAnnularEclipse =  sunAngularSize / 2 - moonAngularSize / 2;  // might be negative (no annular)
            
            double altitude = planetAltAz(ECPlanetSun, _calculationDateInterval, observerLatitude, observerLongitude,
                                          true/*correctForParallax*/, true/*altNotAz*/, _currentCache);  // already incorporates topocentric parallax
            //printAngle(altitude, "planetIsUp altitude");
            double altAtRiseSet = altitudeAtRiseSet(julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache),
                                                    ECPlanetSun, false/*!wantGeocentricAltitude*/, _currentCache, ECWBFullPrecision);
            //printAngle(altAtRiseSet, "...altAtRiseSet");
            if (altitude < altAtRiseSet) {
                *eclipseKind = ECEclipseSolarNotUp;
            } else if (physicalSeparation > separationAtPartialEclipse) {
                *eclipseKind = ECEclipseNoneSolar;
            } else if (physicalSeparation < separationAtAnnularEclipse) {
                *eclipseKind = ECEclipseAnnularSolar;
            } else if (physicalSeparation > separationAtTotalEclipse) {
                *eclipseKind = ECEclipsePartialSolar;
            } else {
                *eclipseKind = ECEclipseTotalSolar;
            }
            solarNotLunar = true;
            *shadowAngularSize = 0;  // N/A for solar
        } else {  // might be lunar
            *shadowAngularSize = 2 * umbralAngularRadius(moonParallax, sunAngularSize/2, sunParallax);
            //printAngle(*shadowAngularSize/2, "corrected shadow angular radius");
            double shadowRA = sunRightAscension + M_PI;
            if (shadowRA > 2 * M_PI) {
                shadowRA -= 2 * M_PI;
            }
            double shadowDecl = -sunDeclination;
            
            physicalSeparation = angularSeparation(shadowRA, shadowDecl, moonRightAscension, moonDeclination);
            separationAtPartialEclipse = moonAngularSize / 2 + *shadowAngularSize / 2;
            separationAtTotalEclipse = *shadowAngularSize / 2 - moonAngularSize / 2;
            
            double altitude = planetAltAz(ECPlanetMoon, _calculationDateInterval, observerLatitude, observerLongitude,
                                          true/*correctForParallax*/, true/*altNotAz*/, _currentCache);  // already incorporates topocentric parallax
            //printAngle(altitude, "planetIsUp altitude");
            double altAtRiseSet = altitudeAtRiseSet(julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache),
                                                    ECPlanetMoon, false/*!wantGeocentricAltitude*/, _currentCache, ECWBFullPrecision);
            //printAngle(altAtRiseSet, "...altAtRiseSet");
            if (altitude < altAtRiseSet) {
                *eclipseKind = ECEclipseLunarNotUp;
            } else if (physicalSeparation > separationAtPartialEclipse) {
                *eclipseKind = ECEclipseNoneLunar;
            } else if (physicalSeparation > separationAtTotalEclipse) {
                *eclipseKind = ECEclipsePartialLunar;
            } else {
                *eclipseKind = ECEclipseTotalLunar;
            }
            solarNotLunar = false;
        }
        //printingEnabled = true;
        //printAngle(physicalSeparation, "physicalSeparation");
        //printAngle(separationAtPartialEclipse, "separationAtPartialEclipse");
        //printAngle(separationAtTotalEclipse, "separationAtTotalEclipse");
        //printingEnabled = false;
        
        // Fit y=mx+b to (separationAtTotalEclipse, 1), (separationAtPartialEclipse, 2)
        // y = y1 + (x - x1)*(y2 - y1)/(x2 - x1), and note y2 - y1 == 1
        *angularSep = physicalSeparation;
        *abstractSeparation = 1 + (physicalSeparation - separationAtTotalEclipse) / (separationAtPartialEclipse - separationAtTotalEclipse);
        //printf("raw separation %.2f\n", separation);
        if (*abstractSeparation < 0) {
            *abstractSeparation = 0;
        } else if (*abstractSeparation > 3) {
            *abstractSeparation = 3;
            *eclipseKind = solarNotLunar ? ECEclipseNoneSolar : ECEclipseNoneLunar;  // override possible not-up if needle is pegged
        }
        //printf("separation %.2f\n", separation);
        //printf("%s\n", nameOfEclipseKind(*eclipseKind));
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[eclipseSeparationSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[eclipseKindSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[eclipseSeparationSlotIndex] = *abstractSeparation;
            _currentCache->cacheSlots[eclipseAngularSeparationSlotIndex] = physicalSeparation;
            _currentCache->cacheSlots[eclipseKindSlotIndex] = *eclipseKind;
            _currentCache->cacheSlots[eclipseShadowAngularSizeSlotIndex] = *shadowAngularSize;
        }
    }
}

// Separation of Sun from Moon, or Earth's shadow from Moon, scaled such that
//   1) partial eclipse starts when separation == 2
//   2) total eclipse starts when separation == 1
//   3) Limited to range 0 < sep < 3
// Note that zero doesn't therefore represent zero separation, and that zero separation may lie above or below the total eclipse point depending on the relative diameters
double
ESAstronomyManager::eclipseAbstractSeparation () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double abstractSeparation;
    double angularSeparation;
    double shadowAngularSize;
    ECEclipseKind eclipseKind;
    calculateEclipse(_calculationDateInterval, _observerLatitude, _observerLongitude, &abstractSeparation, &angularSeparation, &shadowAngularSize, &eclipseKind, _currentCache);
    return abstractSeparation;
}

double
ESAstronomyManager::eclipseAngularSeparation () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double abstractSeparation;
    double angularSeparation;
    double shadowAngularSize;
    ECEclipseKind eclipseKind;
    calculateEclipse(_calculationDateInterval, _observerLatitude, _observerLongitude, &abstractSeparation, &angularSeparation, &shadowAngularSize, &eclipseKind, _currentCache);
    return angularSeparation;
}

double
ESAstronomyManager::eclipseShadowAngularSize () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double abstractSeparation;
    double angularSeparation;
    double shadowAngularSize;
    ECEclipseKind eclipseKind;
    calculateEclipse(_calculationDateInterval, _observerLatitude, _observerLongitude, &abstractSeparation, &angularSeparation, &shadowAngularSize, &eclipseKind, _currentCache);
    return shadowAngularSize;
}

ECEclipseKind
ESAstronomyManager::eclipseKind () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    double abstractSeparation;
    double angularSeparation;
    double shadowAngularSize;
    ECEclipseKind eclipseKind;
    calculateEclipse(_calculationDateInterval, _observerLatitude, _observerLongitude, &abstractSeparation, &angularSeparation, &shadowAngularSize, &eclipseKind, _currentCache);
    return eclipseKind;
}

/* static */ bool
ESAstronomyManager::eclipseKindIsMoreSolarThanLunar(ECEclipseKind eclipseKind) {
    switch (eclipseKind) {
      case ECEclipseNoneSolar:
        return true;
      case ECEclipseNoneLunar:
        return false;
      case ECEclipseSolarNotUp:
        return true;
      case ECEclipsePartialSolar:
        return true;
      case ECEclipseAnnularSolar:
        return true;
      case ECEclipseTotalSolar:
        return true;
      case ECEclipseLunarNotUp:
        return false;
      case ECEclipsePartialLunar:
        return false;
      case ECEclipseTotalLunar:
        return false;
      default:
        ESAssert(false);
        return false;
    }
}

// How much the vernal equinox has moved with respect to the ideal tropical year, defined as the
// exact ecliptic longitude of the Sun in the year 2000 CE.
double
ESAstronomyManager::calendarErrorVsTropicalYear () {
   ESAssert(_astroCachePool);
   ESAssert(_currentCache == _astroCachePool->currentCache);
   ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
   double errorAngle;
   if (_currentCache && _currentCache->cacheSlotValidFlag[calendarErrorSlotIndex] == _currentCache->currentFlag) {
       errorAngle = _currentCache->cacheSlots[calendarErrorSlotIndex];
   } else {
       double todaysLongitude = sunEclipticLongitudeForDate(_calculationDateInterval, _currentCache);

       ESDateComponents cs;
       ESCalendar_UTCDateComponentsFromTimeInterval(_calculationDateInterval, &cs);
       cs.era = 1;    // CE
       cs.year = 2001;
       ESTimeInterval thisDay2000 = ESCalendar_timeIntervalFromUTCDateComponents(&cs);

       ECAstroCache *priorCache = pushECAstroCacheInPool(_astroCachePool, &_astroCachePool->year2000Cache, thisDay2000);
       double year2000Longitude = sunEclipticLongitudeForDate(thisDay2000, _astroCachePool->currentCache);
       popECAstroCacheToInPool(_astroCachePool, priorCache);

       errorAngle = year2000Longitude - todaysLongitude;
       if (_currentCache) {
           _currentCache->cacheSlotValidFlag[calendarErrorSlotIndex] = _currentCache->currentFlag;
           _currentCache->cacheSlots[calendarErrorSlotIndex] = errorAngle;
       }
   }
   return errorAngle;
}

double
ESAstronomyManager::precession () {
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    double precession;
    if (_currentCache && _currentCache->cacheSlotValidFlag[precessionSlotIndex] == _currentCache->currentFlag) {
        precession = _currentCache->cacheSlots[precessionSlotIndex];
    } else {
        double centuriesSinceEpochTDT = julianCenturiesSince2000EpochForDateInterval(_calculationDateInterval, NULL, _currentCache);
        precession = generalPrecessionSinceJ2000(centuriesSinceEpochTDT);
        if (_currentCache) {
            _currentCache->cacheSlotValidFlag[precessionSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[precessionSlotIndex] = precession;
        }
    }
    return precession;
}

bool
ESAstronomyManager::nextSunriseValid () {
    return !isnan(nextSunrise());
}

bool
ESAstronomyManager::nextSunsetValid () {
    return !isnan(nextSunset());
}

bool
ESAstronomyManager::nextMoonriseValid () {
    return !isnan(nextMoonrise());
}

bool
ESAstronomyManager::nextMoonsetValid () {
    return !isnan(nextMoonset());
}

bool
ESAstronomyManager::prevSunriseValid () {
    return !isnan(prevSunrise());
}

bool
ESAstronomyManager::prevSunsetValid () {
    return !isnan(prevSunset());
}

bool
ESAstronomyManager::prevMoonriseValid () {
    return !isnan(prevMoonrise());
}

bool
ESAstronomyManager::prevMoonsetValid () {
    return !isnan(prevMoonset());
}

bool
ESAstronomyManager::nextPlanetriseValid(int planetNumber) {
    return !isnan(nextPlanetriseForPlanetNumber(planetNumber));
}

bool
ESAstronomyManager::nextPlanetsetValid(int planetNumber) {
    return !isnan(nextPlanetsetForPlanetNumber(planetNumber));
}

bool
ESAstronomyManager::sunriseForDayValid () {
    return !isnan(sunriseForDay());
}

bool
ESAstronomyManager::sunsetForDayValid () {
    return !isnan(sunsetForDay());
}

bool
ESAstronomyManager::suntransitForDayValid () {
    return !isnan(suntransitForDay());
}

bool
ESAstronomyManager::moonriseForDayValid () {
    return !isnan(moonriseForDay());
}

bool
ESAstronomyManager::moonsetForDayValid () {
    return !isnan(moonsetForDay());
}

bool
ESAstronomyManager::moontransitForDayValid () {
    return !isnan(moontransitForDay());
}

bool
ESAstronomyManager::planetriseForDayValid(int planetNumber) {
    return !isnan(planetriseForDay(planetNumber));
}

bool
ESAstronomyManager::planetsetForDayValid(int planetNumber) {
    return !isnan(planetsetForDay(planetNumber));
}

bool
ESAstronomyManager::planettransitForDayValid(int planetNumber) {
    return !isnan(planettransitForDay(planetNumber));
}

double
ESAstronomyManager::angle24HourForDateInterval(ESTimeInterval dateInterval,
                                               ESTimeBaseKind timeBaseKind) {
    if (isnan(dateInterval)) {
        return dateInterval;
    }
    _scratchWatchTime->setToFrozenDateInterval(dateInterval);
    switch(timeBaseKind) {
      case ESTimeBaseKindLT:
        return _scratchWatchTime->hour24ValueUsingEnv(_environment) * M_PI / 12;
      case ESTimeBaseKindUT:
      {
        ESDateComponents cs;
        ESCalendar_UTCDateComponentsFromTimeInterval(dateInterval, &cs);
        return (cs.hour + cs.minute / 60.0 + cs.seconds / 3600.0) * M_PI / 12;
      }
      case ESTimeBaseKindLST:
      {
        ECAstroCache *priorCache = pushECAstroCacheWithSlopInPool(_astroCachePool, &_astroCachePool->refinementCache, dateInterval, 0);
        double lst = ::localSiderealTime(dateInterval, _observerLongitude, _astroCachePool->currentCache);
        popECAstroCacheToInPool(_astroCachePool, priorCache);
        return lst * M_PI / (12 * 3600);
      }
      default:
        ESAssert(false);
        return nan("");
    }
}

// Special op for day/night indicator leaves.  Returns 24-hour angle
// numLeaves == 0 means special cases by leafNumber:
//    0: rise24HourIndicatorAngle
//    1:  set24HourIndicatorAngle
//    2: polar summer mask angle
//    3: polar winter mask angle
//    4: transit24HourIndicatorAngle
// numLeaves < 0 special case for Dawn/dusk indicators
// planetNumber == 9 means return angles for nighttime leaves 
double
ESAstronomyManager::dayNightLeafAngleForPlanetNumber(int            planetNumber,
                                                     double         leafNumber,
                                                     int            numLeaves,
                                                     double         overrideAltitudeDesired,
                                                     bool           *isRiseSet, // Valid only if numLeaves == 0; will store false here if there is no rise or set and we're returning the transit
                                                     bool           *aboveHorizon, // Valid only if numLeaves == 0 and *isRiseSet returns false
                                                     ESTimeBaseKind timeBaseKind) {
    ESAssert(!(isRiseSet && numLeaves != 0));
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    ESAssert(!_currentCache || fabs(_currentCache->dateInterval - _calculationDateInterval) <= ASTRO_SLOP);
    bool nightTime = planetNumber == ECPlanetMidnightSun;
    if (nightTime) {
        planetNumber = ECPlanetSun;
    }
    ESAssert(timeBaseKind == ESTimeBaseKindLT || timeBaseKind == ESTimeBaseKindLST);  // Else we need another set of slots...
    int possibleLSTOffset = timeBaseKind == ESTimeBaseKindLT ? 0 : (dayNightMasterRiseAngleLSTSlotIndex - dayNightMasterRiseAngleSlotIndex);
    int masterRiseSlotIndex = dayNightMasterRiseAngleSlotIndex + planetNumber + possibleLSTOffset;
    int masterSetSlotIndex  = dayNightMasterSetAngleSlotIndex + planetNumber + possibleLSTOffset;
    int masterRTransitSlotIndex = dayNightMasterRTransitAngleSlotIndex + planetNumber + possibleLSTOffset;
    int masterSTransitSlotIndex  = dayNightMasterSTransitAngleSlotIndex + planetNumber + possibleLSTOffset;
    double riseTimeAngle;
    double setTimeAngle;
    double rTransitAngle;
    double sTransitAngle;
    if (_currentCache && isnan(overrideAltitudeDesired) && (_currentCache->cacheSlotValidFlag[masterRiseSlotIndex] == _currentCache->currentFlag)) {
        ESAssert(_currentCache->cacheSlotValidFlag[masterSetSlotIndex] == _currentCache->currentFlag);
        ESAssert(_currentCache->cacheSlotValidFlag[masterRTransitSlotIndex] == _currentCache->currentFlag);
        ESAssert(_currentCache->cacheSlotValidFlag[masterSTransitSlotIndex] == _currentCache->currentFlag);
        riseTimeAngle = _currentCache->cacheSlots[masterRiseSlotIndex];
        setTimeAngle = _currentCache->cacheSlots[masterSetSlotIndex];
        rTransitAngle = _currentCache->cacheSlots[masterRTransitSlotIndex];
        sTransitAngle = _currentCache->cacheSlots[masterSTransitSlotIndex];
    } else {
        // Get rise, set, transit
        bool planetIsUp;
        if (!isnan(overrideAltitudeDesired)) {
            ESAssert(planetNumber == ECPlanetSun);
            double planetAlt = planetAltAz(ECPlanetSun, _calculationDateInterval, _observerLatitude, _observerLongitude,
                                           true/*correctForParallax*/, true/*altNotAz*/, _currentCache);  // already incorporates topocentric parallax  (but not refraction, but this isn't the true rise/set so that's not important)
            planetIsUp = planetAlt > overrideAltitudeDesired;
        } else {
            planetIsUp = this->planetIsUp(planetNumber);
        }
        double rTransit;
        double riseTime = nextPrevRiseSetInternalWithFudgeInterval(-fudgeFactorSeconds, planetaryRiseSetTimeRefined/*calculationMethod*/, overrideAltitudeDesired,
                                                                   planetNumber, true/*riseNotSet*/, !planetIsUp/*isNext*/, (3600 * 13.2)/*lookahead*/, &rTransit/*riseSetOrTransit*/);
        double sTransit;
        double setTime  = nextPrevRiseSetInternalWithFudgeInterval(-fudgeFactorSeconds, planetaryRiseSetTimeRefined/*calculationMethod*/, overrideAltitudeDesired,
                                                                   planetNumber, false/*riseNotSet*/, planetIsUp/*isNext*/, (3600 * 13.2)/*lookahead*/, &sTransit/*riseSetOrTransit*/);
        //printingEnabled = false;
        ESAssert(!isnan(rTransit));
        ESAssert(!isnan(sTransit));
        riseTimeAngle = angle24HourForDateInterval(riseTime, timeBaseKind);
        setTimeAngle = angle24HourForDateInterval(setTime, timeBaseKind);
        rTransitAngle = angle24HourForDateInterval(rTransit, timeBaseKind);
        if (isnan(riseTimeAngle)) {
            if (ESUtil::nansEqual(riseTimeAngle, kECAlwaysAboveHorizon)) {
                // In this case, the transit time will be for the low transit.  We want the high transit always, so add 180
                rTransitAngle = ESUtil::fmod(rTransitAngle + M_PI, 2 * M_PI);
            }
        }
        sTransitAngle = angle24HourForDateInterval(sTransit, timeBaseKind);
        if (isnan(setTimeAngle)) {
            if (ESUtil::nansEqual(setTimeAngle, kECAlwaysAboveHorizon)) {
                // In this case, the transit time will be for the low transit.  We want the high transit always, so add 180
                sTransitAngle = ESUtil::fmod(sTransitAngle + M_PI, 2 * M_PI);
            }
        }
        //if (planetNumber == ECPlanetSun && leafNumber == 0) {
        //    printingEnabled = true;
        //    printf("\nTimezone of current calendar is %s\n", [[[ltCalendar timeZone] name] UTF8String]);
        //    printAngle(_observerLongitude, "_observerLongitude");
        //    printAngle(_observerLatitude, "_observerLatitude");
        //    printAngle(riseTimeAngle, "riseTimeAngle");
        //    printAngle(setTimeAngle, "setTimeAngle");
        //    printAngle(rTransitAngle, "rTransitAngle");
        //    printAngle(sTransitAngle, "sTransitAngle");
        //    printingEnabled = false;
        //}
        if (_currentCache && isnan(overrideAltitudeDesired)) {
            _currentCache->cacheSlotValidFlag[masterRiseSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[masterSetSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[masterRTransitSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlotValidFlag[masterSTransitSlotIndex] = _currentCache->currentFlag;
            _currentCache->cacheSlots[masterRiseSlotIndex] = riseTimeAngle;
            _currentCache->cacheSlots[masterSetSlotIndex] = setTimeAngle;
            _currentCache->cacheSlots[masterRTransitSlotIndex] = rTransitAngle;
            _currentCache->cacheSlots[masterSTransitSlotIndex] = sTransitAngle;
        }
    }   
    bool isSpecial = false;
    if (numLeaves == 0) { // Special case 24-hour indicator angle
        if (leafNumber == 0) {  // rise
            if (isnan(riseTimeAngle)) {
                ESAssert(!isnan(rTransitAngle));
                if (isRiseSet) {
                    *isRiseSet = false;
                }
                if (aboveHorizon) {
                    *aboveHorizon = ESUtil::nansEqual(riseTimeAngle, kECAlwaysAboveHorizon);
                }
                return rTransitAngle;
            } else {
                if (isRiseSet) *isRiseSet = true;
                return riseTimeAngle;
            }
        } else if (leafNumber == 1) { // set
            if (isnan(setTimeAngle)) {
                ESAssert(!isnan(sTransitAngle));
                if (isRiseSet) {
                    *isRiseSet = false;
                }
                if (aboveHorizon) {
                    *aboveHorizon = ESUtil::nansEqual(setTimeAngle, kECAlwaysAboveHorizon);
                }
                return sTransitAngle;
            } else {
                if (isRiseSet) *isRiseSet = true;
                return setTimeAngle;
            }
        } else {
            isSpecial = true;    //handled below
        }
    } else if (numLeaves < 0) {  // Dawn/dusk indicators; abs(numLeaves) is amount to move backward when 
        numLeaves = -numLeaves;
    }
    ESAssert(!isRiseSet);  // Not meaningful unless numLeaves == 0
    ESAssert(!aboveHorizon); // ditto
    double leafWidth = M_PI * 2 / numLeaves;
    bool polarSummer = false;
    bool polarWinter = false;
    if (isnan(riseTimeAngle)) {
        if (isnan(setTimeAngle)) {
            // Can't tell: Use average transit of rise & set
            if (sTransitAngle > rTransitAngle + M_PI) {
                sTransitAngle -= (2 * M_PI);
            } else if (sTransitAngle < rTransitAngle - M_PI) {
                sTransitAngle -= (2 * M_PI);
            }
            double avgTransitAngle = (rTransitAngle + sTransitAngle) / 2;
            if (ESUtil::nansEqual(riseTimeAngle, kECAlwaysAboveHorizon)) {
                riseTimeAngle = avgTransitAngle - M_PI;
                setTimeAngle = avgTransitAngle + M_PI;
                polarSummer = true;
            } else {
                riseTimeAngle = avgTransitAngle - leafWidth / 2 - .00001;  // Make them a tad bigger so we don't lose the info later  // [stevep 11/14/09]: ??? what info? should this have been on the summer case?
                setTimeAngle = avgTransitAngle + leafWidth / 2 + .00001;
                polarWinter = true;
            }
        } else {  // rise invalid, set valid
            if (ESUtil::nansEqual(riseTimeAngle, kECAlwaysAboveHorizon)) {
                riseTimeAngle = setTimeAngle - (2 * M_PI);
                polarSummer = true;
            } else {
                riseTimeAngle = setTimeAngle - leafWidth;
                polarWinter = true;
            }
        }
    } else {
        if (isnan(setTimeAngle)) {
            if (ESUtil::nansEqual(setTimeAngle, kECAlwaysAboveHorizon)) {
                setTimeAngle = riseTimeAngle + (2 * M_PI);
                polarSummer = true;
            } else {
                setTimeAngle = riseTimeAngle + leafWidth;
                polarWinter = true;
            }
        }
    }
    if (isSpecial) {
        if (leafNumber == 2) {
            return polarSummer;
        } else if (leafNumber == 3) {
            return polarWinter;
        } else if (leafNumber == 4) {
            double tt;  // ignored
            // NOTE: NOT cached, but relatively fast
            double transitT = planettransitTimeRefined(_calculationDateInterval, _observerLatitude, _observerLongitude, true/*wantHighTransit*/, planetNumber, nan(""), &tt, _astroCachePool);
            double transitA = angle24HourForDateInterval(transitT, timeBaseKind);
            // printingEnabled = true;
            // printAngle(transitA, "Transit Angle");
            // printingEnabled = false;
            return transitA;
        } else {
            ESAssert(false);
        }
    }
    ESAssert(!isnan(riseTimeAngle));
    ESAssert(!isnan(setTimeAngle));
    riseTimeAngle = ESUtil::fmod(riseTimeAngle, 2 * M_PI);
    setTimeAngle = ESUtil::fmod(setTimeAngle, 2 * M_PI);
    if (setTimeAngle <= riseTimeAngle + 0.0001) {
        setTimeAngle += 2 * M_PI;
    }
    if (nightTime) {
        setTimeAngle += leafWidth/2;
        riseTimeAngle -= leafWidth/2;
    } else {
        setTimeAngle -= leafWidth/2;
        riseTimeAngle += leafWidth/2;
    }

    if (setTimeAngle < riseTimeAngle) {
        riseTimeAngle = setTimeAngle = (riseTimeAngle + setTimeAngle) / 2;
    }
    double leafCenterAngle;
    if (nightTime) {
        leafCenterAngle = setTimeAngle + (2*M_PI - setTimeAngle + riseTimeAngle) / (numLeaves - 1) * leafNumber;
    } else {
        leafCenterAngle = riseTimeAngle + (setTimeAngle - riseTimeAngle) / (numLeaves - 1) * leafNumber;
    }

    if (leafCenterAngle > 2 * M_PI) {
        leafCenterAngle -= 2 * M_PI;
    }
    ESAssert(!isnan(leafCenterAngle));
    return leafCenterAngle;
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithSunriseForDay() {
    ESTimeInterval date = sunriseForDay();
    if (isnan(date)) {
        date = meridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithSunsetForDay() {
    ESTimeInterval date = sunsetForDay();
    if (isnan(date)) {
        date = meridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithSuntransitForDay() {
    ESTimeInterval date = suntransitForDay();
    if (isnan(date)) {
        date = meridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithNextSunrise() {
    ESTimeInterval date = nextSunrise();
    if (isnan(date)) {
        date = meridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithPrevSunrise() {
    ESTimeInterval date = prevSunrise();
    if (isnan(date)) {
        date = meridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithNextSunset() {
    ESTimeInterval date = nextSunset();
    if (isnan(date)) {
        date = meridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithPrevSunset() {
    ESTimeInterval date = prevSunset();
    if (isnan(date)) {
        date = meridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithMoonriseForDay() {
    ESTimeInterval date = moonriseForDay();
    if (isnan(date)) {
        date = moonMeridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithMoonsetForDay() {
    ESTimeInterval date = moonsetForDay();
    if (isnan(date)) {
        date = moonMeridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithMoontransitForDay() {
    ESTimeInterval date = moontransitForDay();
    if (isnan(date)) {
        date = moonMeridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithNextMoonrise() {
    ESTimeInterval date = nextMoonrise();
    if (isnan(date)) {
        date = moonMeridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithPrevMoonrise() {
    ESTimeInterval date = prevMoonrise();
    if (isnan(date)) {
        date = moonMeridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithNextMoonset() {
    ESTimeInterval date = nextMoonset();
    if (isnan(date)) {
        date = moonMeridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithPrevMoonset() {
    ESTimeInterval date = prevMoonset();
    if (isnan(date)) {
        date = moonMeridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithNextPlanetrise(int planetNumber) {
    ESTimeInterval date = nextPlanetriseForPlanetNumber(planetNumber);
    if (isnan(date)) {
        date = nextPlanettransit(planetNumber);
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithPrevPlanetrise(int planetNumber) {
    ESTimeInterval date = prevPlanetriseForPlanetNumber(planetNumber);
    if (isnan(date)) {
        date = prevPlanettransit(planetNumber);
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithNextPlanetset(int planetNumber) {
    ESTimeInterval date = nextPlanetsetForPlanetNumber(planetNumber);
    if (isnan(date)) {
        date = nextPlanettransit(planetNumber);
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithPrevPlanetset(int planetNumber) {
    ESTimeInterval date = prevPlanetsetForPlanetNumber(planetNumber);
    if (isnan(date)) {
        date = prevPlanettransit(planetNumber);
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithPlanetriseForDay(int planetNumber) {
    ESTimeInterval date = planetriseForDay(planetNumber);
    if (isnan(date)) {
        date = planettransitForDay(planetNumber);
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithPlanetsetForDay(int planetNumber) {
    ESTimeInterval date = planetsetForDay(planetNumber);
    if (isnan(date)) {
        date = planettransitForDay(planetNumber);
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithPlanettransitForDay(int planetNumber) {
    ESTimeInterval date = planettransitForDay(planetNumber);
    if (isnan(date)) {
        date = meridianTimeForSeason();
    }
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithClosestNewMoon() {
    ESTimeInterval date = closestNewMoon();
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithClosestFullMoon() {
    ESTimeInterval date = closestFullMoon();
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithClosestFirstQuarter() {
    ESTimeInterval date = closestFirstQuarter();
    return watchTimeForInterval(date);
}

ESWatchTime  *
ESAstronomyManager::watchTimeWithClosestThirdQuarter() {
    ESTimeInterval date = closestThirdQuarter();
    return watchTimeForInterval(date);
}

// special ops for Mauna Kea
bool
ESAstronomyManager::sunriseIndicatorValid() {
    if (_watchTime->runningBackward()) {
        return (planetIsUp(ECPlanetSun) ? nextSunriseValid() : prevSunriseValid());
    } else {
        return (planetIsUp(ECPlanetSun) ? prevSunriseValid() : nextSunriseValid());
    }
}
bool
ESAstronomyManager::sunsetIndicatorValid() {
    if (_watchTime->runningBackward()) {
        return (planetIsUp(ECPlanetSun) ? prevSunsetValid() : nextSunsetValid());
    } else {
        return (planetIsUp(ECPlanetSun) ? nextSunsetValid() : prevSunsetValid());
    }
}

double
ESAstronomyManager::sunrise24HourIndicatorAngle() {
    return dayNightLeafAngleForPlanetNumber(ECPlanetSun, 0/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, NULL/*isRiseSet*/, NULL/*aboveHorizon*/);
}

double
ESAstronomyManager::sunset24HourIndicatorAngle() {
    return dayNightLeafAngleForPlanetNumber(ECPlanetSun, 1/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, NULL/*isRiseSet*/, NULL/*aboveHorizon*/);
}

bool
ESAstronomyManager::polarSummer() {
    return dayNightLeafAngleForPlanetNumber(ECPlanetSun, 2/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, NULL/*isRiseSet*/, NULL/*aboveHorizon*/);
}

bool
ESAstronomyManager::polarWinter() {
    return dayNightLeafAngleForPlanetNumber(ECPlanetSun, 3/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, NULL/*isRiseSet*/, NULL/*aboveHorizon*/);
}

bool
ESAstronomyManager::polarPlanetSummer(int planetNumber) {
    return dayNightLeafAngleForPlanetNumber(planetNumber, 2/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, NULL/*isRiseSet*/, NULL/*aboveHorizon*/);
}

bool
ESAstronomyManager::polarPlanetWinter(int planetNumber) {
    return dayNightLeafAngleForPlanetNumber(planetNumber, 3/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, NULL/*isRiseSet*/, NULL/*aboveHorizon*/);
}

double
ESAstronomyManager::moonrise24HourIndicatorAngle() {
    return dayNightLeafAngleForPlanetNumber(ECPlanetMoon, 0/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, NULL/*isRiseSet*/, NULL/*aboveHorizon*/);
}

double
ESAstronomyManager::moonset24HourIndicatorAngle() {
    return dayNightLeafAngleForPlanetNumber(ECPlanetMoon, 1/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, NULL/*isRiseSet*/, NULL/*aboveHorizon*/);
}

double
ESAstronomyManager::planetrise24HourIndicatorAngle(int planetNumber) {
    return dayNightLeafAngleForPlanetNumber(planetNumber, 0/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, NULL/*isRiseSet*/, NULL/*aboveHorizon*/);
}

double
ESAstronomyManager::planetset24HourIndicatorAngle(int planetNumber) {
    return dayNightLeafAngleForPlanetNumber(planetNumber, 1/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, NULL/*isRiseSet*/, NULL/*aboveHorizon*/);
}

double
ESAstronomyManager::planetrise24HourIndicatorAngle(int  planetNumber,
                                                   bool *isRiseSet,
                                                   bool *aboveHorizon) {
    return dayNightLeafAngleForPlanetNumber(planetNumber, 0/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, isRiseSet, aboveHorizon);
}

double
ESAstronomyManager::planetset24HourIndicatorAngle(int  planetNumber,
                                                  bool *isRiseSet,
                                                  bool *aboveHorizon) {
    return dayNightLeafAngleForPlanetNumber(planetNumber, 1/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, isRiseSet, aboveHorizon);
}

double
ESAstronomyManager::planettransit24HourIndicatorAngle(int planetNumber) {
    return dayNightLeafAngleForPlanetNumber(planetNumber, 4/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, NULL/*isRiseSet*/, NULL/*aboveHorizon*/);
}

double
ESAstronomyManager::planetrise24HourIndicatorAngleLST(int planetNumber) {
    return dayNightLeafAngleForPlanetNumber(planetNumber, 0/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, NULL/*isRiseSet*/, NULL/*aboveHorizon*/, ESTimeBaseKindLST/*timeBaseKind*/);
}

double
ESAstronomyManager::planetset24HourIndicatorAngleLST(int planetNumber) {
    return dayNightLeafAngleForPlanetNumber(planetNumber, 1/*leafNumber*/, 0/*numLeaves*/, nan("")/*overrideAltitudeDesired*/, NULL/*isRiseSet*/, NULL/*aboveHorizon*/, ESTimeBaseKindLST/*timeBaseKind*/);
}

double
ESAstronomyManager::sunSpecial24HourIndicatorAngleForAltitudeKind(CacheSlotIndex altitudeKind,
                                                                  bool           *validReturn) {
    double altitude;
    bool riseNotSet;
    getParamsForAltitudeKind(altitudeKind, &altitude, &riseNotSet);
    double angle;
    if (altitudeKind == sunRiseMorning || altitudeKind == sunSetEvening) {
        angle = dayNightLeafAngleForPlanetNumber(ECPlanetSun, (riseNotSet ? 0 : 1)/*leafNumber*/, 0/*numLeaves*/, altitude/*overrideAltitudeDesired*/, validReturn/*isRiseSet*/, NULL/*aboveHorizon*/);
    } else {
        ECAstroCache *priorCache;
        double riseSetOrTransit;
        ESTimeInterval saveCalculationDate = _calculationDateInterval;
        if (riseNotSet) {
            // go forward to next sunset (or transit), then back to previous rising twilight
            ESTimeInterval nextSunsetOrTransit;
            nextPrevRiseSetInternalWithFudgeInterval(fudgeFactorSeconds, planetaryRiseSetTimeRefined/*calculationMethod*/, nan("")/*overrideAltitudeDesired*/, ECPlanetSun/*planetNumber*/,
                                                     false/*riseNotSet*/, !_watchTime->runningBackward()/*isNext*/, (3600 * 13.2)/*lookahead*/, &nextSunsetOrTransit/*riseSetOrTransit*/);
            // Set current time to sunset and push a temporary cache here
            _calculationDateInterval = nextSunsetOrTransit;  // Danger Will Robinson.
            priorCache = pushECAstroCacheInPool(_astroCachePool, &_astroCachePool->tempCache, _calculationDateInterval);
            // Go back to previous rising twilight
            double ignoreMe = nextPrevRiseSetInternalWithFudgeInterval(fudgeFactorSeconds, planetaryRiseSetTimeRefined/*calculationMethod*/, altitude/*overrideAltitudeDesired*/, ECPlanetSun/*planetNumber*/,
                                                                       true/*riseNotSet*/, _watchTime->runningBackward()/*isNext*/, (3600 * 13.2)/*lookahead*/, &riseSetOrTransit/*riseSetOrTransit*/);
            *validReturn = !isnan(ignoreMe);
        } else {
            // go backward to prev sunrise (or transit), then forward to next setting twilight
            ESTimeInterval prevSunriseOrTransit;
            nextPrevRiseSetInternalWithFudgeInterval(fudgeFactorSeconds, planetaryRiseSetTimeRefined/*calculationMethod*/, nan("")/*overrideAltitudeDesired*/,
                                                     ECPlanetSun/*planetNumber*/, true/*riseNotSet*/, _watchTime->runningBackward()/*isNext*/, (3600 * 13.2)/*lookahead*/, &prevSunriseOrTransit/*riseSetOrTransit*/);
            // Set current time to sunrise and push a temporary cache here
            _calculationDateInterval = prevSunriseOrTransit;  // Danger Will Robinson
            priorCache = pushECAstroCacheInPool(_astroCachePool, &_astroCachePool->tempCache, _calculationDateInterval);
            // Go forward to next setting twilight
            double ignoreMe = nextPrevRiseSetInternalWithFudgeInterval(fudgeFactorSeconds, planetaryRiseSetTimeRefined/*calculationMethod*/, altitude/*overrideAltitudeDesired*/, ECPlanetSun/*planetNumber*/,
                                                                       false/*riseNotSet*/, !_watchTime->runningBackward()/*isNext*/, (3600 * 13.2)/*lookahead*/, &riseSetOrTransit/*riseSetOrTransit*/);
            *validReturn = !isnan(ignoreMe);
        }
        popECAstroCacheToInPool(_astroCachePool, priorCache);
        _calculationDateInterval = saveCalculationDate;
        angle = angle24HourForDateInterval(riseSetOrTransit, ESTimeBaseKindLT);
    }
    //EC_printAngle(angle, [[NSString stringWithFormat:@"sunSpecial24 altitude=%.1f %s", altitude * 180 / M_PI, riseNotSet ? "rise" : "set"] UTF8String]);
    return angle;
}

#if 0 // save to record WB precession formula
static void testRAPrecessionForDate(double centuriesSinceEpochTDT) {
    double T = centuriesSinceEpochTDT / 10;
    double T2 = T*T;
    double T3 = T2*T;
    double T4 = T2*T2;
    double T5 = T3*T2;
    double T6 = T3*T3;
    double T7 = T4*T3;
    double pa03 = generalPrecessionSinceJ2000(centuriesSinceEpochTDT);
    double pawb = 50290.966*T + 111.1971*T2 + 0.07732*T3 - 0.235316*T4 - 0.0018055*T5 + 0.00017451*T6 + 0.000013095*T7;
    pawb = pawb * M_PI/(3600 * 180);
    printf("\nprecession for %15.10f centuries since J2000\n", centuriesSinceEpochTDT);
    printAngle(pa03, "pa03");
    printAngle(pawb, "pawb");
    
}
#endif

ESAstronomyManager::ESAstronomyManager(ESTimeEnvironment *environment,
                                       ESLocation        *location) {
    _environment = environment; // no retain; we are the ownee, not the owner
    _location = location;   // ditto
    _watchTime = NULL;
    _estz = NULL;
    _calculationDateInterval = 0;
    _observerLatitude = 0;
    _observerLongitude = 0;
    _inActionButton = 0;
    _astroCachePool = NULL;
    _currentCache = NULL;
    _observerLongitude = 0;
    _observerLatitude = 0;

#ifndef NDEBUG
    //testPolarEdge();
    //exit(0);
    //testConversion();
    //runTests();  // DEBUG: REMOVE
    //testReligion();   // DEBUG
//    static bool tested = false;
//    if (!tested) {
//      testConvertJ2000();
//      testRAPrecessionForDate(-1);
//      for(int i = -3; i < 27; i++) {
//          testRAPrecessionForDate(-i * 10);
//      }
//      for(int i = 210; i < 220; i++) {
//          testRAPrecessionForDate(-i);
//      }
//      for(int i = 0; i < 27; i++) {
//          generalPrecessionSinceJ2000(-i * 10);
//      }
//      tested = true;
//    }
#endif
}

/*static*/ ESUserString 
ESAstronomyManager::nameOfPlanetWithNumber(int planetNumber) {
    switch(planetNumber) {
      case ECPlanetSun:
        return ESUserString("Sun");
      case ECPlanetMoon:
        return ESUserString("Moon");
      case ECPlanetMercury:
        return ESUserString("Mercury");
      case ECPlanetVenus:
        return ESUserString("Venus");
      case ECPlanetEarth:
        return ESUserString("Earth");
      case ECPlanetMars:
        return ESUserString("Mars");
      case ECPlanetJupiter:
        return ESUserString("Jupiter");
      case ECPlanetSaturn:
        return ESUserString("Saturn");
      case ECPlanetUranus:
        return ESUserString("Uranus");
      case ECPlanetNeptune:
        return ESUserString("Neptune");
      case ECPlanetPluto:
        return ESUserString("Pluto");
      default:
        return ESUserString("Unknown planet number");
    }
}

#if 0  // Needs to be converted to ESCalendar/ESTimeZone
NSDate  *
ESAstronomyManager::dateOfNewYearInCalendar(NS-Calendar *cal,
                                            NSDate      *forGregorianDate) {
    ESAssert(false);  // Steve 5/22/09: Need to fix for BCE
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    NS-DateComponents *inputComps = [ltCalendar components:(NSEraCalendarUnit|NSYearCalendarUnit) fromDate:dat];
    NS-DateComponents *hebrewComps = [cal components:NSEraCalendarUnit|NSYearCalendarUnit|NSMonthCalendarUnit|NSDayCalendarUnit fromDate:dat];
    hebrewComps.month = 1;
    hebrewComps.day = 1;
    NS-Date *ret = [cal dateFromComponents:hebrewComps];
    NS-DateComponents *newComps = [ltCalendar components:(NSEraCalendarUnit|NSYearCalendarUnit) fromDate:ret];
    if (newComps.year < inputComps.year) {  // Hebrew New Year was last year
        // try the next Hebrew year
        hebrewComps.year += 1;
        NS-Date *ret2 = [cal dateFromComponents:hebrewComps];
        NS-DateComponents *newComps2 =[ltCalendar components:(NSEraCalendarUnit|NSYearCalendarUnit) fromDate:ret2];
        if (newComps2.era == inputComps.era && newComps2.year == inputComps.year) {
            ret = ret2;
        } else {
            ESAssert(false);
        }
    } else {
        ESAssert(newComps.year == inputComps.year && newComps.era == inputComps.era);
    }
    return ret;
}

NSDate  *
ESAstronomyManager::dateOfEasterForYearOfDate(NSDate *dat) {
    int y;
    ESAssert(_astroCachePool);
    ESAssert(_currentCache == _astroCachePool->currentCache);
    NS-DateComponents *comps = [ltCalendar components:(NSEraCalendarUnit|NSYearCalendarUnit) fromDate:dat];
    y = [comps year];
    /*
     This algorithm is due to J.-M. Oudin (1940) and is reprinted in the Explanatory Supplement to the Astronomical Almanac, ed. P. K. Seidelmann (1992).
     See Chapter 12, "Calendars", by L. E. Doggett.
     */
    int c,n,k,i,j,l,m,d;        // must use integer arithmetic
    c = y / 100;
    n = y - 19 * ( y / 19 );
    k = ( c - 17 ) / 25;
    i = c - c / 4 - ( c - k ) / 3 + 19 * n + 15;
    i = i - 30 * ( i / 30 );
    i = i - ( i / 28 ) * ( 1 - ( i / 28 ) * ( 29 / ( i + 1 ) ) * ( ( 21 - n ) / 11 ) );
    j = y + y / 4 + i + 2 - c + c / 4;
    j = j - 7 * ( j / 7 );
    l = i - j;
    m = 3 + ( l + 40 ) / 44;
    d = l + 28 - 31 * ( m / 4 );
    /* end of Oudin algorithm */
    comps.month = m;
    comps.day = d;
    NSDate *ret = [ltCalendar dateFromComponents:comps];
    return ret;
}

void
ESAstronomyManager::testReligion() {
#ifndef NDEBUG
    static bool rtestsRun = false;
    if (rtestsRun) {
        return;
    }
    rtestsRun = true;
    for (int i=1980; i<2025; i++) {
        NS-DateComponents *c = [[[NS-DateComponents alloc] init] autorelease];
        c.year = i;
        c.month = 6;
        NSDate *d = [gmtCalendar dateFromComponents:c];
        NSDate *e = [self dateOfEasterForYearOfDate:d];
        printf("Easter %d: %s\n", i, [[e description] UTF8String]);
    }
    NS-Calendar *hebrewCal = [[[NS-Calendar alloc]initWithCalendarIdentifier:NSHebrewCalendar] autorelease];
    for (int i=1900; i<2100; i++) {
        NS-DateComponents *c = [[[NS-DateComponents alloc] init] autorelease];
        c.year = i;
        c.month = 12;
        printf("Rosh Hashanah: %s\n", [[[self dateOfNewYearInCalendar:hebrewCal forGregorianDate:[gmtCalendar dateFromComponents:c]] description] UTF8String]);
    }
    NS-Calendar *islamicCalendar = [[[NS-Calendar alloc]initWithCalendarIdentifier:NSIslamicCivilCalendar] autorelease];
    for (int i=1900; i<2100; i++) {
        NS-DateComponents *c = [[[NS-DateComponents alloc] init] autorelease];
        c.year = i;
        c.month = 12;
        printf("Islamic New Year: %s\n", [[[self dateOfNewYearInCalendar:islamicCalendar forGregorianDate:[gmtCalendar dateFromComponents:c]] description] UTF8String]);
    }
#endif
}
#endif
