// -*- lsst-c++ -*-

/**
 * @file
 * @brief   Miscellaneous helper methods for operating on DataProperty and Policy instances.
 *
 * @ingroup associate
 */

#ifndef LSST_AP_UTILS_H
#define LSST_AP_UTILS_H

#include <boost/any.hpp>
#include <boost/format.hpp>
#include <boost/numeric/conversion/converter.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_integral.hpp>

#include <lsst/daf/base/DataProperty.h>
#include <lsst/pex/policy/Policy.h>

#include "Common.h"


namespace lsst {
namespace ap {


LSST_AP_API lsst::daf::base::DataProperty::PtrType extractRequired(
    lsst::daf::base::DataProperty::PtrType const & properties,
    std::string                            const & key
);

LSST_AP_API std::string const getTableName(
    lsst::pex::policy::Policy::Ptr         const & policy,
    lsst::daf::base::DataProperty::PtrType const & properties
);

LSST_AP_API std::string const getTableTemplateName(
    lsst::pex::policy::Policy::Ptr         const & policy,
    lsst::daf::base::DataProperty::PtrType const & properties
);

LSST_AP_API void verifyPathName(std::string const & name);


/**
 * Extracts an integer of the specified type from the given boost::any. The extraction
 * will succeed if and only if @a v contains an integer value (of built-in C++ type)
 * that can be converted to an integer of the desired type without overflow.
 */
template <typename TargetT>
TargetT anyToInteger(boost::any const & v) {

    using namespace boost::numeric;

    BOOST_STATIC_ASSERT(boost::is_integral<TargetT>::value);

    std::type_info const & type = v.type();
    if (type == typeid(bool)) {
        typedef converter<TargetT, bool> Converter;
        return Converter::convert(boost::any_cast<bool>(v));
    } else if (type == typeid(char)) {
        typedef converter<TargetT, char> Converter;
        return Converter::convert(boost::any_cast<char>(v));
    } else if (type == typeid(signed char)) {
        typedef converter<TargetT, signed char> Converter;
        return Converter::convert(boost::any_cast<signed char>(v));
    } else if (type == typeid(unsigned char)) {
        typedef converter<TargetT, unsigned char> Converter;
        return Converter::convert(boost::any_cast<unsigned char>(v));
    } else if (type == typeid(short)) {
        typedef converter<TargetT, short> Converter;
        return Converter::convert(boost::any_cast<short>(v));
    } else if (type == typeid(unsigned short)) {
        typedef converter<TargetT, unsigned short> Converter;
        return Converter::convert(boost::any_cast<unsigned short>(v));
    } else if (type == typeid(int)) {
        typedef converter<TargetT, int> Converter;
        return Converter::convert(boost::any_cast<int>(v));
    } else if (type == typeid(unsigned int)) {
        typedef converter<TargetT, unsigned int> Converter;
        return Converter::convert(boost::any_cast<unsigned int>(v));
    } else if (type == typeid(long)) {
        typedef converter<TargetT, long> Converter;
        return Converter::convert(boost::any_cast<long>(v));
    } else if (type == typeid(unsigned long)) {
        typedef converter<TargetT, unsigned long> Converter;
        return Converter::convert(boost::any_cast<unsigned long>(v));
    } else if (type == typeid(long long)) {
        typedef converter<TargetT, long long> Converter;
        return Converter::convert(boost::any_cast<long long>(v));
    } else if (type == typeid(unsigned long long)) {
        typedef converter<TargetT, unsigned long long> Converter;
        return Converter::convert(boost::any_cast<unsigned long long>(v));
    }
    throw boost::bad_any_cast();
}


}} // end of namespace lsst::ap

#endif // LSST_AP_UTILS_H

