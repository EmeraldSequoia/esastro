//
//  ESTimeLocAstroEnvironment.cpp
//
//  Created by Steven Pucci 27 Aug 2012
//  Copyright Emerald Sequoia LLC 2012. All rights reserved.
//

#include "ESTimeLocAstroEnvironment.hpp"
#include "ESAstronomy.hpp"

ESTimeLocAstroEnvironment::ESTimeLocAstroEnvironment(const char *timeZoneName,
                                                     bool       observingIPhoneTime)
:   ESTimeLocEnvironment(timeZoneName,
                         observingIPhoneTime)
{
    _astronomyManager = new ESAstronomyManager(this, this->location());
}

ESTimeLocAstroEnvironment::ESTimeLocAstroEnvironment(const char *timeZoneName,
                                                     bool       observingIPhoneTime,
                                                     const char *locationPrefsPrefix)
:   ESTimeLocEnvironment(timeZoneName,
                         observingIPhoneTime,
                         locationPrefsPrefix)
{
    _astronomyManager = new ESAstronomyManager(this, this->location());
}

ESTimeLocAstroEnvironment::ESTimeLocAstroEnvironment(const char *timeZoneName,
                                                     const char *cityName,
                                                     double     latitudeInDegrees,
                                                     double     longitudeInDegrees)
:   ESTimeLocEnvironment(timeZoneName, cityName, latitudeInDegrees, longitudeInDegrees)
{
    _astronomyManager = new ESAstronomyManager(this, this->location());
}

ESTimeLocAstroEnvironment::~ESTimeLocAstroEnvironment() {
    delete _astronomyManager;
}


/*virtual*/ void 
ESTimeLocAstroEnvironment::setupLocalEnvironmentForThreadFromActionButton(bool        fromActionButton,
                                                                          ESWatchTime *watchTime) {
    _astronomyManager->setupLocalEnvironmentForThreadFromActionButton(fromActionButton, watchTime);
}

/*virtual*/ void 
ESTimeLocAstroEnvironment::cleanupLocalEnvironmentForThreadFromActionButton(bool fromActionButton) {
    _astronomyManager->cleanupLocalEnvironmentForThreadFromActionButton(fromActionButton);
}

