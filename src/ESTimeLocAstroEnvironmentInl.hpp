//
//  ESTimeLocAstroEnvironmentInl.hpp
//
//  Created by Steven Pucci 27 Aug 2012
//  Copyright Emerald Sequoia LLC 2012. All rights reserved.
//

#ifndef _ESTIMELOCASTROENVIRONMENT_HPP_
#error "Include this file only from ESTimeLocEnvironment.hpp"
#endif

#ifdef _ESTIMELOCASTROENVIRONMENT_INL_HPP_
#error "Include this file only once from ESTimeLocEnvironment.hpp"
#endif

#define _ESTIMELOCASTROENVIRONMENT_INL_HPP_

inline ESAstronomyManager *
ESTimeLocAstroEnvironment::astronomyManager() {
    return _astronomyManager;
}

// NOTE: The following is a method on ESTimeEnvironment *which is only defined here*
inline ESTimeLocAstroEnvironment *
ESTimeEnvironment::asAstroEnv() {
    ESAssert(isAstroEnv()); return (ESTimeLocAstroEnvironment *)this;
}
