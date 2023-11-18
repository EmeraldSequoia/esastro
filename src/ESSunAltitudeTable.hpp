//
//  ESSunAltitudeTable.hpp
//
//  Created by Steven Pucci 08 Apr 2013
//  Copyright Emerald Sequoia LLC 2013. All rights reserved.
//

#ifndef _ESSUNALTITUDETABLE_HPP_
#define _ESSUNALTITUDETABLE_HPP_

#undef ES_HUGE_TABLE
#define ES_LARGE_TABLE
#undef ES_MEDIUM_TABLE
#undef ES_TINY_TABLE

#if defined(ES_HUGE_TABLE)
# define ES_SUBSOLAR_STEPS 100
# define ES_LATITUDE_STEPS 299
# define ES_ALTITUDE_STEPS 22
#elif defined(ES_LARGE_TABLE)
# define ES_SUBSOLAR_STEPS 100
# define ES_LATITUDE_STEPS 149
# define ES_ALTITUDE_STEPS 22
#elif defined(ES_MEDIUM_TABLE)
# define ES_SUBSOLAR_STEPS 2
# define ES_LATITUDE_STEPS 149
# define ES_ALTITUDE_STEPS 1
#elif defined(ES_TINY_TABLE)
# define ES_SUBSOLAR_STEPS 2
# define ES_LATITUDE_STEPS 149
# define ES_ALTITUDE_STEPS 1
#endif

#define ES_LATITUDE_SLOTS (ES_LATITUDE_STEPS + 1)

#define ES_ALTITUDE_SLOTS (ES_ALTITUDE_STEPS + 1)

#define ES_SUBSOLAR_SLOTS (ES_SUBSOLAR_STEPS + 1)


// The following is *map* latitude, not sslat
#define ES_LAT_TO_INDEX(B) (round((B + M_PI/2)/M_PI * ES_LATITUDE_STEPS))  // Note: STEPS not SLOTS -- we have a slot for M_PI/2 at the end
#define ES_INDEX_TO_LAT(I) (-M_PI/2 + (I * M_PI / ES_LATITUDE_STEPS))

#define ES_LATITUDE_MIN (- M_PI - .0001)
#define ES_LATITUDE_MAX   (M_PI + .0001)
#define ES_LATITUDE_RANGE (ES_LATITUDE_MAX - ES_LATITUDE_MIN)

#define ES_SUBSOLAR_MAX (24 * M_PI / 180)  // Contains max decl (and thus sslat) of 23.4
#define ES_SUBSOLAR_MIN (0)                // Negative sslat is handled by inverting map latitude
#define ES_SUBSOLAR_RANGE (ES_SUBSOLAR_MAX - ES_SUBSOLAR_MIN)

#define ES_INDEX_TO_SUBSOLAR(I) (ES_SUBSOLAR_MIN + (I * ES_SUBSOLAR_RANGE / ES_SUBSOLAR_STEPS))

#define ES_ALT_MAX 0
#define ES_ALT_MIN_DEGREES -9		// start/end of civil twilight
//#define ES_ALT_MIN_DEGREES -18		// start/end of nautical twilight
#define ES_ALT_MIN (ES_ALT_MIN_DEGREES * M_PI / 180)
#define ES_ALT_RANGE (ES_ALT_MAX - ES_ALT_MIN)

#define ES_INDEX_TO_ALT(I) (ES_ALT_MAX - (I * ES_ALT_RANGE / ES_ALTITUDE_STEPS))


// This struct represents a single row of data for a single location latitude within a table for a given subsolar latitude
struct ESSunAltitudeLatitudeRowData {
    float                   longitudeForAltitude[ES_ALTITUDE_SLOTS];
};

// This struct contains all of the required data for a given subsolar latitude
struct ESSunAltitudeMapTable {
    ESSunAltitudeLatitudeRowData rowDataForLatitude[ES_LATITUDE_SLOTS];
};

/** This class gives a means of determining the longitudes at which the Sun is at a particular altitude, given a latitude and a subsolar location.
 *  It is intended for Emerald Observatory's Earth Map display showing the light and dark regions of the Earth.
 *  The idea is that given a subsolar latitude, one can then calculate, by successive approximation of a parametric formula,
 *  the longitude at which the given altitude appears.  We record that longitude in a table by altitude, for a given latitude and subsolar latitude.
 *  Once we have the table (which we can serialize to disk and then deserialize to read it back in during a later session) we can then do an
 *  interpolation in the table to determine the proper longitude for a given set of data.
*/
class ESSunAltitudeTable {
  public:
    static ESSunAltitudeTable *createFromFile();
    static ESSunAltitudeTable *createFromScratch();

    void                    interpolateRowData(float                        subsolarLatitude,
                                               int                          mapLatitudeIndex,
                                               ESSunAltitudeLatitudeRowData *rowData) const;

    void                    serializeToFile();

    void                    printTable();  // Only available if ESTRACE turned on in .cpp file

  private:
                            ESSunAltitudeTable() {}
    void                    fillInFromScratch();

    ESSunAltitudeMapTable   _altitudeMapForSubSolarLatitude[ES_SUBSOLAR_SLOTS];
};

#endif  // _ESSUNALTITUDETABLE_HPP_
