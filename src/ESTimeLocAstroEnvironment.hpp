//
//  ESTimeLocAstroEnvironment.hpp
//
//  Created by Steven Pucci 26 Aug 2012
//  Copyright Emerald Sequoia LLC 2012. All rights reserved.
//

#ifndef _ESTIMELOCASTROENVIRONMENT_HPP_
#define _ESTIMELOCASTROENVIRONMENT_HPP_

#include "ESTimeLocEnvironment.hpp"

class ESAstronomyManager;

class ESTimeLocAstroEnvironment : public ESTimeLocEnvironment {
  public:
                            ESTimeLocAstroEnvironment(const char *timeZoneName,
                                                      bool       observingIPhoneTime);
                            ESTimeLocAstroEnvironment(const char *timeZoneName,
                                                      bool       observingIPhoneTime,
                                                      const char *locationPrefsPrefix);
                            ESTimeLocAstroEnvironment(const char *timeZoneName,
                                                      const char *cityName,
                                                      double     latitudeInDegrees,
                                                      double     longitudeInDegrees);
                            ~ESTimeLocAstroEnvironment();
    ESAstronomyManager      *astronomyManager();

    virtual bool            isAstroEnv() const { return true; }

    virtual void            setupLocalEnvironmentForThreadFromActionButton(bool        fromActionButton,
                                                                           ESWatchTime *watchTime);
    virtual void            cleanupLocalEnvironmentForThreadFromActionButton(bool fromActionButton);

  private:
    ESAstronomyManager      *_astronomyManager;
};

// Include this only once, here
#include "ESTimeLocAstroEnvironmentInl.hpp"

#endif  // _ESTIMELOCASTROENVIRONMENT_HPP_
