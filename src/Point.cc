// -*- lsst-c++ -*-

/**
 * @file
 * @brief   Implementation of Point class.
 *
 * @ingroup associate
 */

#include <cassert>
#include <cmath>

#include <iostream>

#include "lsst/ap/Common.h"
#include "lsst/ap/Point.h"
#include "lsst/ap/SpatialUtil.h"
#include "lsst/ap/Time.h"

using lsst::afw::math::Random;


namespace lsst { namespace ap { namespace {

double randomDec(Random & rng, double const decMin, double const decMax) {

    assert(decMin < decMax && decMin < 90.0 && decMax > -90.0);

    double min  = (decMin < -90.0) ? -90.0 : decMin;
    double max  = (decMax >  90.0) ?  90.0 : decMax;
    double z    = rng.flat(std::sin(radians(min)), std::sin(radians(max)));
    double res  = degrees(std::asin(z));
    if (res < decMin) {
        return decMin;
    } else if (res > decMax) {
        return decMax;
    }
    return res;
}

}}} // end of namespace lsst::ap::<anonymous>


/**
 * Randomly perturbs the point such that the results are distributed according to a normal
 * distribution centered on the original point and having a standard deviation of @a sigma
 * degrees.
 */
lsst::ap::Point & lsst::ap::Point::perturb(Random & rng, double const sigma) {
    return perturb(rng, sigma, rng.uniform()*360.0);
}


/**
 * Randomly perturbs the point in the direction given by the specified position angle so that the
 * distance to the original point is normally distributed with a standard deviation of @a sigma degrees.
 */
lsst::ap::Point & lsst::ap::Point::perturb(Random & rng, double const sigma, double const pa) {

    double sra  = std::sin(radians(_ra));
    double cra  = std::cos(radians(_ra));
    double sdec = std::sin(radians(_dec));
    double cdec = std::cos(radians(_dec));

    // original position p
    double x = cra*cdec;
    double y = sra*cdec;
    double z = sdec;

    double spa = std::sin(radians(pa));
    double cpa = std::cos(radians(pa));

    // north vector tangential to p
    double nx = - cra*sdec;
    double ny = - sra*sdec;
    double nz =   cdec;

    // east vector tangential to p
    double ex = - sra;
    double ey =   cra;
    double ez =   0.0;

    // rotate north vector at V by minus position angle
    double tx = spa*ex + cpa*nx;
    double ty = spa*ey + cpa*ny;
    double tz = spa*ez + cpa*nz;

    // perturb in this direction by a random angle that is normally
    // distributed with a standard deviation of sigma degrees
    double mag  = radians(rng.gaussian()*sigma);
    double smag = std::sin(mag);
    double cmag = std::cos(mag);

    // obtain the perturbed position
    x = x*cmag + tx*smag;
    y = y*cmag + ty*smag;
    z = z*cmag + tz*smag;
    // finally, convert back to spherical coordinates (in degrees)
    _ra = degrees(std::atan2(y, x));
    if (_ra < 0.0) {
        _ra += 360.0;
    }
    _dec = degrees(std::asin(z));
    if (_dec <= -90.0) {
        _dec = -90.0;
    } else if (_dec >= 90.0) {
        _dec = 90.0;
    }
    return *this;
}


/** Returns the angular distance to the given point (in degrees). */
double lsst::ap::Point::distance(Point const & p) const {

    double sra  = std::sin(radians(_ra));
    double cra  = std::cos(radians(_ra));
    double sdec = std::sin(radians(_dec));
    double cdec = std::cos(radians(_dec));

    double x = cra*cdec;
    double y = sra*cdec;
    double z = sdec;

    sra  = std::sin(radians(p._ra));
    cra  = std::cos(radians(p._ra));
    sdec = std::sin(radians(p._dec));
    cdec = std::cos(radians(p._dec));

    x *= cra*cdec;
    y *= sra*cdec;
    z *= sdec;

    return degrees(std::acos(x + y + z));
}


/** Picks a point uniformly at random on the unit sphere. */
lsst::ap::Point const lsst::ap::Point::random(Random & rng) {
    double z = rng.flat(-1.0, 1.0);
    return Point(rng.flat(0.0, 360.0), degrees(std::asin(z)));
}


/** Picks a point uniformly at random in the specified dec band. */
lsst::ap::Point const lsst::ap::Point::random(
    Random & rng,
    double const decMin,
    double const decMax
) {
    return Point(rng.flat(0.0, 360.0), randomDec(rng, decMin, decMax));
}


/** Picks a point uniformly at random in the specified box. */
lsst::ap::Point const lsst::ap::Point::random(
    Random & rng, 
    double const raMin,
    double const raMax,
    double const decMin,
    double const decMax
) {
    assert(raMin >= 0.0 && raMin <= 360.0);
    assert(raMax >= 0.0 && raMax <= 360.0);

    double ra;
    if (raMin < raMax) {
        ra = rng.flat(raMin, raMin);
        if (ra > raMax) {
            ra = raMax;
        }
    } else {
        // wrap-around
        double m = raMin - 360.0;
        ra = rng.flat(m, raMax);
        if (ra < 0) {
            ra += 360.0;
        }
    }
    return Point(ra, randomDec(rng, decMin, decMax));
}

