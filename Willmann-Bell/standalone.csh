#!/bin/csh -f

g++ -c -g -DSTANDALONE -DES_IOS -I ../deps/esutil/src -I ../deps/estime/src ../src/ESAstronomyCache.cpp
g++ -c -g -DSTANDALONE -DES_IOS -I ../deps/esutil/src -I ../deps/estime/src ESWillmannBell.cpp
g++ -o test ESWillmannBell.o ESAstronomyCache.o && ./test


