This is a library intended for a portable version of various Emerald Sequoia apps.

Note that as of this writing (2023) it is only used by the following apps:

*   The WearOS (aka Android) version of Emerald Chronometer
*   Emerald Observatory

Fundamental astronomy calculations, such as the positions of the Moon or the planets
as seen from the center of the Earth, are primarily done using code derived from the
Willmann-Bell/Sky&Telescope code described in the NOTICE file in this directory. That
code is in the Willmann-Bell directory here.

The code in `src/ESAstronomy.cpp` builds on those fundamental calculations in two ways:

1.  A large number of other values are derived from the fundamental calculations, such
    as
    *   planetrise, planetset
    *   altitude/azimuth as seen from an observer's location
    *   eclipse-specific calculations near solar or lunar eclipses
2.  A cache of intermediate values that are calculated for a particular time value. This
    was very important in the early days of the iPhone when calculating all of the
    information for a watch like Miami, showing all of the planets' rise, set, and
    transits, but with today's devices, even WearOS devices, the cache is unneeded (but
    still present because that's easier than tearing it out).
