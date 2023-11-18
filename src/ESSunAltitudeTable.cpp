//
//  ESSunAltitudeTable.cpp
//
//  Created by Steven Pucci 08 Apr 2013
//  Copyright Emerald Sequoia LLC 2013. All rights reserved.
//

#include "ESSunAltitudeTable.hpp"

#include "ESUtil.hpp"
#define ESTRACE
#include "ESTrace.hpp"
#include "ESFile.hpp"

#define NUM_PARAMETRIC_POINTS 1000001  // Best if this is a multiple of 2 plus 1 -- that makes zero come out exact

#if 0  // This would work except that the _SLOTS macros are arithmetic expressions
#define ES_TABLE_FILE_FMT(SS,L,ALT) "SunAltitudeData-ss" #SS "-lat" #L "-alt" #ALT ".dat"
#define ES_TABLE_FILE_X(SS,L,ALT) ES_TABLE_FILE_FMT(SS,L,ALT)
#define ES_TABLE_FILE   ES_TABLE_FILE_X(ES_SUBSOLAR_SLOTS,ES_LATITUDE_SLOTS,ES_ALTITUDE_SLOTS)
#else
#define SETUP_TABLE_FILE_NAME \
    char ES_TABLE_FILE[66];   \
    snprintf(ES_TABLE_FILE, sizeof(ES_TABLE_FILE), "SunAltitudeData-ss%d-lat%d-alt%d-%d.dat", ES_SUBSOLAR_SLOTS, ES_LATITUDE_SLOTS, ES_ALTITUDE_SLOTS, -ES_ALT_MIN_DEGREES); \
    ESAssert(strlen(ES_TABLE_FILE) < sizeof(ES_TABLE_FILE) - 2);
#endif

// Returns true iff sslat < 0 and latitude should be flipped
static bool getInterpolatedSSLatIndex(double sslat,
                                      int    *beforeIndex,
                                      int    *afterIndex) {
    bool flipLatitude;
    if (sslat < 0) {
        flipLatitude = true;
        sslat = - sslat;
    } else {
        flipLatitude = false;
    }
    double ssLatIndexD = (sslat - ES_SUBSOLAR_MIN) * ES_SUBSOLAR_STEPS / ES_SUBSOLAR_RANGE;
    *beforeIndex = floor(ssLatIndexD);
    *afterIndex = ceil(ssLatIndexD);
    ESAssert(*beforeIndex >= 0);
    ESAssert(*afterIndex < ES_SUBSOLAR_SLOTS);
    return flipLatitude;
}

#if 0  // Unused
static void getInterpolatedLatIndex(double lat,
                                    int    *beforeIndex,
                                    int    *afterIndex) {
    double latIndexD = (lat - ES_LATITUDE_MIN) * ES_LATITUDE_STEPS / ES_LATITUDE_RANGE;
    *beforeIndex = floor(latIndexD);
    *afterIndex = ceil(latIndexD);
    ESAssert(*beforeIndex >= 0);
    ESAssert(*afterIndex < ES_LATITUDE_SLOTS);
}
#endif

/*static*/ ESSunAltitudeTable *
ESSunAltitudeTable::createFromFile() {
    SETUP_TABLE_FILE_NAME
    size_t tableSize;
    char *ptr = ESFile::getFileContentsInMallocdArray(ES_TABLE_FILE,
                                                      ESFilePathTypeRelativeToResourceDir,
                                                      false/*missingOK*/,
                                                      &tableSize);
    return (ESSunAltitudeTable *)ptr;
}

/*static*/ ESSunAltitudeTable *
ESSunAltitudeTable::createFromScratch() {
#ifdef ESTRACE
    SETUP_TABLE_FILE_NAME
    tracePrintf1("Table file is %s\n", ES_TABLE_FILE)
#endif
    ESSunAltitudeTable *table = new ESSunAltitudeTable;
    table->fillInFromScratch();
    return table;
}

static double
infinityForSSLatAlt(double subSolarLatitude,
                    double latitude,
                    double altitude) {
    // Representation of longitude if the curve doesn't cross the given latitude:
    // We want pi if the Sun is always above the given altitude, and zero if it's always below, because
    //   the larger the longitude, the further we go (from longitude zero) before starting the night region (and pi is as far as we can go)
    // But it's a bit tricky figuring that out:  It only happens near the poles (for negative altitudes relatively near zero,
    //   which is what we're dealing with).  The Sun can be always up if either
    //   - the pole is in summer or
    //   - the pole is somewhat in winter but the altitude threshold is sufficiently negative that the Sun never gets down that far
    //   The Sun can be always down if the pole is in winter and the altitude threshold is sufficiently close to zero that
    //   the Sun never gets *up* that far.

    //   At Jun solstice (sslat +23.4):
    //   lat   alt         avg
    //    90   23 ->  23    23
    //    45  -22 ->  68    23
    //    23  -43 ->  90    23
    //    10  -57 ->  77    10
    //     0  -67 ->  67     0
    //   -10  -77 ->  57   -10
    //   -23  -90 ->  43   -23
    //   -30  -83 ->  37   -23
    //   -45  -68 ->  22   -23
    //   -80  -33 -> -13   -23
    //   -90  -23 -> -23   -23

    //  At sslat 10:
    //    90   10 ->  10    10
    //    45  
    //    23  -57 ->  77    10
    //    10  -70 ->  90    10
    //     0  -80 ->  80     0
    //   -10  -90 ->  70   -10
    //   -23  -77 ->  57   -10
    //   -30  -70 ->  50   -10  
    //   -45  
    //   -80  
    //   -90  

    //  At Equinox (sslat 0):
    //    90    0 ->  0      0
    //    45  -45 -> 45      0
    //    23  -68 -> 68      0
    //    10  -80 -> 80      0
    //     0  -90 -> 90      0
    //   -10  -80 -> 80      0
    //   -23  -68 -> 68      0
    //   -30  -60 -> 60      0
    //   -45  -45 -> 45      0
    //   -80  -10 -> 10      0
    //   -90    0 ->  0      0

    //  altMax =  90 - abs(lat - sslat);
    //  altMin = -90 + abs(lat + sslat);

    //  altAvg = (altMax+altMin)/2
    //  altAvg = abs(lat + sslat) - abs(lat - sslat)

    // double altMax =  M_PI/2 - fabs(latitude - subSolarLatitude);
    // double altMin = -M_PI/2 + fabs(latitude + subSolarLatitude);
    // double altAvg = (altMax + altMin) / 2.0;

    // bool polarWinter = altitude > altMax;  // if the altitude is above the max for this latitude, the Sun never gets up this high and we're in Winter
    // bool polarSummer = altitude < altMin;  // if the altitude is below the min for this latitude, the Sun never gets down this low and we're in Summer

    double altMax =  M_PI/2 - fabs(latitude - subSolarLatitude);
    double altMin = -M_PI/2 + fabs(latitude + subSolarLatitude);
    // double altAvg = (fabs(latitude - subSolarLatitude) - fabs(latitude + subSolarLatitude)) / 2;
    if (altitude > altMax - .0001) {
        // The altitude is above the max for this latitude, the Sun never gets up this high and we're in winter
        return 0;
    } else if (altitude < altMin + 0.0001) {
        // if the altitude is below the min for this latitude, the Sun never gets down this low and we're in Summer
        return M_PI;
    } else {
        printf("altMax %.2f (now %.2f), fabs %.2f, lat %.2f, ssl %.2f\n",
               altMax*180/M_PI, 
               (M_PI - fabs(latitude - subSolarLatitude))*180/M_PI,
               fabs(latitude - subSolarLatitude)*180/M_PI, latitude*180/M_PI, subSolarLatitude*180/M_PI);
        ESAssert(false);
        return 0;
    }
}


void
ESSunAltitudeTable::fillInFromScratch() {
    traceEnter3("ESSunAltitudeTable::fillInFromScratch ss%d-lat%d-alt%d", ES_SUBSOLAR_SLOTS, ES_LATITUDE_SLOTS, ES_ALTITUDE_SLOTS);
    tracePrintf1("table size is %ld", sizeof(*this));

    for (int subsolarIndex = 0; subsolarIndex < ES_SUBSOLAR_SLOTS; subsolarIndex++) {
        double subSolarLatitude = ES_INDEX_TO_SUBSOLAR(subsolarIndex);
        traceEnter2("subSolar index %d (%.2f)", subsolarIndex, subSolarLatitude*180/M_PI);

        ESSunAltitudeMapTable *altitudeMapTable = &_altitudeMapForSubSolarLatitude[subsolarIndex];

        for (int altitudeIndex = 0; altitudeIndex < ES_ALTITUDE_SLOTS; altitudeIndex++) {
            double sunAltitude = ES_INDEX_TO_ALT(altitudeIndex);

            ESAssert(subSolarLatitude >= 0);  // If ssLat is < 0, flip the sign of the input latitude when looking up values
            const double cossslat = cos(subSolarLatitude);
            const double sinsslat = sin(subSolarLatitude);

            const double sinAlt = sin(sunAltitude);
            const double cosAlt = cos(sunAltitude);

            // Precalculate unvarying quantities:
            double sinBPart = sinsslat*sinAlt;
            double yPart = cosAlt*cossslat;
            
            double lastLatitude;
            double lastLongitude;
            int latitudeIndex;
            double latitudeForLatitudeIndex;
            
            bool firstTime = true;
            bool lastTime = false;

            for (int i = 0; i < NUM_PARAMETRIC_POINTS; i++) {
                lastTime = false;
                double psi = i * (M_PI / (NUM_PARAMETRIC_POINTS - 1)) - M_PI/2;
                
                double sinB = sinBPart + yPart*sin(psi);
                double x = sinAlt - sinsslat*sinB;
                double B = asin(sinB);      // B is the latitude the parameteric curve gives at this iteration
                double y = yPart*cos(psi);
                double L = atan2(y, x);     // and L is the longitude
                if (L > M_PI) {
                    L -= 2 * M_PI;
                } else if (L < -M_PI) {
                    L += 2 * M_PI;
                }
                ESAssert(L >= 0);  // Otherwise we should flip the sign, but worry that the entire table has to flip at the same time
                //tracePrintf("");
                //traceAngle(psi, "psi");
                //traceAngle(B, "B (latitude)");
                //traceAngle(L, "L (longitude)");
                
                // A latitude index maps to a given latitude.  We want to put the best possible value in that slot, which will be the interpolation between two
                // latitude values before and after the value.  So we remember the previous value as we move forward, and once we cross over a new value, we insert
                // into that slot the appropriate interpolated longitude.
                if (firstTime) {
                    firstTime = false;
                    int newLatitudeIndex = ES_LAT_TO_INDEX(B);
                    ESAssert(newLatitudeIndex >= 0 && newLatitudeIndex < ES_LATITUDE_SLOTS);
                    for (latitudeIndex = 0; latitudeIndex < newLatitudeIndex; latitudeIndex++) {
                        altitudeMapTable->rowDataForLatitude[latitudeIndex].longitudeForAltitude[altitudeIndex] =
                            infinityForSSLatAlt(subSolarLatitude, ES_INDEX_TO_LAT(latitudeIndex), sunAltitude);
                    }
                    ESAssert(latitudeIndex == newLatitudeIndex);
                    altitudeMapTable->rowDataForLatitude[latitudeIndex].longitudeForAltitude[altitudeIndex] = L;
                    latitudeForLatitudeIndex = ES_INDEX_TO_LAT(latitudeIndex);
                    if (B > latitudeForLatitudeIndex) {  // We're passed it, so we've done the best we can do there; move on
                        latitudeIndex++;
                        latitudeForLatitudeIndex = ES_INDEX_TO_LAT(latitudeIndex);
                    } else {
                        // Otherwise we can still do better for this slot, since we should later get spanning latitudes for this slot between which we can
                        // interpolate, so leave latitudeIndex as is
                    }
                } else {
                    ESAssert(B > lastLatitude);
//                    ESAssert(fabs(L-lastLongitude) < .001);
                    while (B > latitudeForLatitudeIndex) {
                        // Interpolate between the two latitudes, set the value, advance
                        double interpolatedLongitude = lastLongitude + (latitudeForLatitudeIndex - lastLatitude)/(B - lastLatitude) * (L - lastLongitude);
                        ESAssert(latitudeIndex < ES_LATITUDE_SLOTS);
                        altitudeMapTable->rowDataForLatitude[latitudeIndex].longitudeForAltitude[altitudeIndex] = interpolatedLongitude;
                        latitudeIndex++;
                        ESAssert(latitudeIndex < ES_LATITUDE_SLOTS);  // B > M_PI/2 ? 
                        latitudeForLatitudeIndex = ES_INDEX_TO_LAT(latitudeIndex);
                        if (B > latitudeForLatitudeIndex) {
                            printf("Seem to not have enough parameteric points; try increasing NUM_PARAMETRIC_POINTS\n");
                        }
                    }
                    if (i == NUM_PARAMETRIC_POINTS-1) {  // Last time
                        // Last chance -- fill in last one
                        int newLatitudeIndex = ES_LAT_TO_INDEX(B);
                        if (newLatitudeIndex == latitudeIndex) {
                            // By the while loop exit above, B is <= latitudeForLatitudeIndex, but still we round to the same value, so we can fill in the long here
                            // directly cause this is the closest we're ever going to get
                            altitudeMapTable->rowDataForLatitude[latitudeIndex].longitudeForAltitude[altitudeIndex] = L;
                        } else {
                            ESAssert(newLatitudeIndex < latitudeIndex);
                            // If we're here, it means that we rounded down below the latitudeIndex we're trying to get, so ignore it; we already set the previous
                            // index to what presumably was the best value for that index
                        }
                        // Now fill in with positive infinity with everything left
                        while (latitudeIndex < ES_LATITUDE_SLOTS) {
                            altitudeMapTable->rowDataForLatitude[latitudeIndex].longitudeForAltitude[altitudeIndex] =
                                infinityForSSLatAlt(subSolarLatitude, ES_INDEX_TO_LAT(latitudeIndex), sunAltitude);
                            latitudeIndex++;
                        }
                        lastTime = true;
                    }
                }
                lastLatitude = B;
                lastLongitude = L;
            }
            ESAssert(lastTime);
        }
        traceExit1("subSolar index %d", subsolarIndex);
    }
    traceExit("ESSunAltitudeTable::fillInFromScratch");
}


void 
ESSunAltitudeTable::interpolateRowData(float                        subsolarLatitude,
                                       int                          mapLatitudeIndex,
                                       ESSunAltitudeLatitudeRowData *rowData) const {
    //traceEnter3("interpolateRowData for ssLat %.2f, mapLat %d(%.2f)", subsolarLatitude*180/M_PI, mapLatitudeIndex, ES_INDEX_TO_LAT(mapLatitudeIndex)*180/M_PI);
    int beforeSSLatIndex;
    int afterSSLatIndex;
    bool flipLatitude = getInterpolatedSSLatIndex(subsolarLatitude, &beforeSSLatIndex, &afterSSLatIndex);
    const ESSunAltitudeMapTable *beforeMapTable = &_altitudeMapForSubSolarLatitude[beforeSSLatIndex];
    const ESSunAltitudeMapTable *afterMapTable = &_altitudeMapForSubSolarLatitude[afterSSLatIndex];

    if (flipLatitude) {
        mapLatitudeIndex = ES_LATITUDE_STEPS - mapLatitudeIndex;  // aka (ES_LATITUDE_SLOTS-1) - mapLatitudeIndex
    }

    float *outputPtr = rowData->longitudeForAltitude;
    const float *stopPtr = outputPtr + ES_ALTITUDE_SLOTS;

    const float *ptrB = beforeMapTable->rowDataForLatitude[mapLatitudeIndex].longitudeForAltitude;
    const float *ptrA =  afterMapTable->rowDataForLatitude[mapLatitudeIndex].longitudeForAltitude;

    while (outputPtr < stopPtr) {
        *outputPtr++ = (*ptrA++ + *ptrB++) / 2;
        //tracePrintf3("interpolateRowData combined %.2f %.2f and got %.2f", ptrB[-1]*180/M_PI, ptrA[-1]*180/M_PI, outputPtr[-1]*180/M_PI);
    }
    //traceExit("interpolateRowData");
}

void
ESSunAltitudeTable::serializeToFile() {
    SETUP_TABLE_FILE_NAME
    bool st = ESFile::writeArrayToFile(this, sizeof(*this), ES_TABLE_FILE, ESFilePathTypeRelativeToAppSupportDir);
    if (st) {
        ESErrorReporter::logInfo("ESSunAltitudeTable", "Successfully wrote to table file %s\n", ES_TABLE_FILE);
    } else {
        ESErrorReporter::logError("ESSunAltitudeTable", "Failed to write to table file %s\n", ES_TABLE_FILE);
    }
}

#ifdef ESTRACE
void 
ESSunAltitudeTable::printTable() {
    traceEnter1("Sun altitude table (%d subsolar slot pages)", ES_SUBSOLAR_SLOTS);
    for (int subsolarIndex = 0; subsolarIndex < ES_SUBSOLAR_SLOTS; subsolarIndex++) {
        traceEnter3("Subsolar page %3d (subsolar latitude %6.2f degrees), %d latitude slots", subsolarIndex, ES_INDEX_TO_SUBSOLAR(subsolarIndex) * 180 / M_PI, ES_LATITUDE_SLOTS);
        ESSunAltitudeMapTable *mapTable = &_altitudeMapForSubSolarLatitude[subsolarIndex];
        for (int latitudeIndex = 0; latitudeIndex < ES_LATITUDE_SLOTS; latitudeIndex++) {
            traceEnter3("Latitude index %3d (latitude %6.2f degrees), %d altitude slots", latitudeIndex, ES_INDEX_TO_LAT(latitudeIndex) * 180 / M_PI, ES_ALTITUDE_SLOTS);
            ESSunAltitudeLatitudeRowData *rowData = &mapTable->rowDataForLatitude[latitudeIndex];
            for (int altitudeIndex = 0; altitudeIndex < ES_ALTITUDE_SLOTS; altitudeIndex++) {
                tracePrintf3("Altitude index %3d (altitude %6.2f degrees) => longitude %7.2f degrees",
                             altitudeIndex, ES_INDEX_TO_ALT(altitudeIndex) * 180 / M_PI, rowData->longitudeForAltitude[altitudeIndex] * 180 / M_PI);
            }
            traceExit2("Latitude index %d (latitude %6.2f degrees)", latitudeIndex, ES_INDEX_TO_LAT(latitudeIndex) * M_PI / 180);
        }
        traceExit2("Subsolar page %d (subsolar latitude %6.2f degrees)", subsolarIndex, ES_INDEX_TO_SUBSOLAR(subsolarIndex) * M_PI / 180);
    }
    traceExit("\nSun altitude table\n");
}
#endif
