// -*- lsst-c++ -*-
//
//##====----------------                                ----------------====##/
//
//! \file   Bitset.h
//! \brief  A fixed capacity FIFO buffer for integers.
//
//##====----------------                                ----------------====##/

#ifndef LSST_AP_BITSET_H
#define LSST_AP_BITSET_H

#include <cassert>
#include <cstring>

#include <boost/static_assert.hpp>

#include "Common.h"


namespace lsst {
namespace ap {

namespace detail {

template <typename Word> struct BitTraits {
    static bool const IS_SPECIALIZED = false;
};

template <> struct LSST_AP_LOCAL BitTraits<uint8_t>  {
    static bool    const IS_SPECIALIZED     = true;
    static int     const BITS_PER_WORD_LOG2 = 3;
    static int     const BITS_PER_WORD      = 8;
    static uint8_t const WORD_MASK          = 0xff;
};

template <> struct LSST_AP_LOCAL BitTraits<uint16_t> {
    static bool     const IS_SPECIALIZED     = true;
    static int      const BITS_PER_WORD_LOG2 = 4;
    static int      const BITS_PER_WORD      = 16;
    static uint16_t const WORD_MASK          = 0xffff;
};

template <> struct LSST_AP_LOCAL BitTraits<uint32_t> {
    static bool     const IS_SPECIALIZED     = true;
    static int      const BITS_PER_WORD_LOG2 = 5;
    static int      const BITS_PER_WORD      = 32;
    static uint32_t const WORD_MASK          = 0xffffffff;
};

template <> struct LSST_AP_LOCAL BitTraits<uint64_t> {
    static bool     const IS_SPECIALIZED     = true;
    static int      const BITS_PER_WORD_LOG2 = 6;
    static int      const BITS_PER_WORD      = 64;
    static uint64_t const WORD_MASK          = UINT64_C(0xffffffffffffffff);
};

template <typename Word>
inline int wordForBit(int const i) {
    return (i >> BitTraits<Word>::BITS_PER_WORD_LOG2);
}

template <typename Word>
inline Word maskForBit(int const i) {
    return static_cast<Word>(1) << (i & (BitTraits<Word>::BITS_PER_WORD - 1));
}

template <typename Word>
bool set(
    int        * const indexes,
    Word       * const words,
    int  const         numBitsToSet,
    int  const         numBits
);

template <typename Word>
void reset(
    Word      * const words,
    int const * const indexes,
    int const         numBitsToReset,
    int const         numBits
);

} // end of namespace detail


/*! \brief  A fixed size set of bits. */
template <typename Word, int NumBits>
class Bitset {

private :

    BOOST_STATIC_ASSERT(NumBits > 0);
    BOOST_STATIC_ASSERT(detail::BitTraits<Word>::IS_SPECIALIZED);

public :

    static int const NUM_BITS  = NumBits;
    static int const NUM_WORDS = (NumBits + (detail::BitTraits<Word>::BITS_PER_WORD - 1)) >>
                                 detail::BitTraits<Word>::BITS_PER_WORD_LOG2;

    /*! Clears all bits. */
    void reset() {
        std::memset(_bits, 0, sizeof(_bits));
    }

    /*! Sets all bits to 1. */
    void set() {
        std::memset(_bits, -1, sizeof(_bits));
    }

    /*! Sets the i-th bit in the set to zero. */
    void reset(int const i) {
        assert(i >= 0 && i < NumBits);
        _bits[detail::wordForBit<Word>(i)] &= ~ detail::maskForBit<Word>(i);
    }

    /*! Sets the i-th bit in the set to one. */
    void set(int const i) {
        assert(i >= 0 && i < NumBits);
        _bits[detail::wordForBit<Word>(i)] |= detail::maskForBit<Word>(i);
    }

    /*! Sets the i-th bit in the set to one if \a on is \c true and to zero otherwise. */
    void set(int const i, bool const on) {
        if (on) {
            set(i);
        } else {
            reset(i);
        }
    }

    /*! Returns \c true if the i-th bit in the set is one and \c false otherwise. */
    bool test(int const i) const {
        assert(i >= 0 && i < NumBits);
        return _bits[detail::wordForBit<Word>(i)] & detail::maskForBit<Word>(i);
    }

    /*! If at least \a numBits zero bits are available in this Bitset, this function sets the first
        \a numBits of them to one and returns \c true. Otherwise, \c false is returned. */
    bool set(int * const indexes, int const numBits) {
        return detail::set<Word>(indexes, _bits, numBits, NumBits);
    }

    /*! Sets \a numBits bits identified by the integers in \a indexes to zero. */
    void reset(int const * const indexes, int const numBits) {
        detail::reset<Word>(_bits, indexes, numBits, NumBits);
    }

private :

    Word _bits[NUM_WORDS];
};


}} // end of namespace lsst::ap

#endif // LSST_AP_BITSET_H
