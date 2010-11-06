// -*- lsst-c++ -*-

/* 
 * LSST Data Management System
 * Copyright 2008, 2009, 2010 LSST Corporation.
 * 
 * This product includes software developed by the
 * LSST Project (http://www.lsst.org/).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the LSST License Statement and 
 * the GNU General Public License along with this program.  If not, 
 * see <http://www.lsstcorp.org/LegalNotices/>.
 */

/** @file
  * @brief  Class for simulated reference catalog positions.
  */
#ifndef LSST_AP_UTIL_REFERENCEPOSITION_H
#define LSST_AP_UTIL_REFERENCEPOSITION_H

#include "Eigen/Core"

#include "../Common.h"
#include "../util/SpatialUtils.h"
#include "../util/EarthPosition.h"
#include "BBox.h"


namespace lsst { namespace ap { namespace match {

/** Position related parameters of a simulated reference catalog source.
  * There are no errors - these are inputs to the image simulator.
  */
class ReferencePosition : public BBox {
public:
    enum Flags {
        MOVING     = 0x1, ///< Set if the reference position has proper motion
        PARALLAX   = 0x2, ///< Set if the reference position has
                          ///  parallax \> MIN_PARALLAX
        SSB_TO_GEO = 0x4  ///< Set if SSB to geocentric corrections are applied
                          ///  by getPosition(double).
    };

    static double const MIN_PARALLAX = 1e-7;

    inline ReferencePosition(int64_t id,
                             double ra,
                             double dec,
                             double epoch=J2000_MJD);

    virtual ~ReferencePosition() { }

    void clearMotion();    
    void setMotion(double muRa,
                   double muDecl,
                   double parallax,
                   double vRadial,
                   bool trueAngle,
                   bool ssbToGeo);

    void setTimeRange(double epoch1, double epoch2);

    inline int64_t getId() const; 
    inline double getEpoch() const;
    inline int getFlags() const;
    inline Eigen::Vector2d const & getSphericalCoords() const;
    inline Eigen::Vector3d const & getPosition() const;
    inline Eigen::Vector3d const & getVelocity() const;

    inline Eigen::Vector3d const getPosition(double epoch) const;

    // BBox API
    virtual double getMinCoord0() const;
    virtual double getMaxCoord0() const;
    virtual double getMinCoord1() const;
    virtual double getMaxCoord1() const; 

private:
    Eigen::Vector2d _sc;     ///< (ra, decl) at _epoch, ICRS rad
    int64_t _id;
    double _epoch;           ///< epoch of reference position, MJD
    Eigen::Vector3d _p;      ///< (x, y, z) at _epoch
    Eigen::Vector3d _v;      ///< (dx/dt, dy/dt, dz/dt)
    double _parallax;        ///< parallax, rad
    double _minDecl;
    double _maxDecl;
    double _minRa;
    double _maxRa; 
    int _flags;
};

}}} // namespace lsst::ap::match

#include "ReferencePosition.cc"

#endif // LSST_AP_UTIL_REFERENCEPOSITION_H

