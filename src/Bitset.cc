// -*- lsst-c++ -*-

/**
 * @file
 * @brief   Implementation of the Bitset template and helper functions.
 *
 * @ingroup associate
 */

#include <climits>

#include <lsst/ap/Bitset.h>


namespace lsst {
namespace ap {
namespace detail {

#if LSST_AP_HAVE_BUILTIN_POPCOUNT

static inline int populationCount(uint64_t const val) {
#   if ULLONG_MAX == 0xffffffffffffffff
    return __builtin_popcountll(val);
#   elif ULONG_MAX == 0xffffffffffffffff
    return __builtin_popcountl(val);
#   else
#       error Unable to map between uint64_t and built-in integral types
#   endif
}

static inline int populationCount(uint32_t const val) {
#   if ULONG_MAX == 0xffffffff
    return __builtin_popcountl(val);
#   elif UINT_MAX == 0xffffffff
    return __builtin_popcount(val);
#   else
#       error Unable to map between uint32_t and built-in integral types
#   endif
}

static inline int populationCount(uint16_t const val) { return __builtin_popcount(val); }
static inline int populationCount(uint8_t  const val) { return __builtin_popcount(val); }

#else

static uint64_t const sFreedMagic64[6] = {
    UINT64_C(0x5555555555555555),
    UINT64_C(0x3333333333333333),
    UINT64_C(0x0F0F0F0F0F0F0F0F),
    UINT64_C(0x00FF00FF00FF00FF),
    UINT64_C(0x0000FFFF0000FFFF),
    UINT64_C(0x00000000FFFFFFFF)
};

static uint32_t const sFreedMagic32[5] = {
    0x55555555,
    0x33333333,
    0x0F0F0F0F,
    0x00FF00FF,
    0x0000FFFF
};

// count bits with sideways addition using Freed's binary magic numbers

static inline int populationCount(uint64_t const val)
{
    uint64_t v = val;
    v = ((v >>  1) & sFreedMagic64[0]) + (v & sFreedMagic64[0]);
    v = ((v >>  2) & sFreedMagic64[1]) + (v & sFreedMagic64[1]);
    v = ((v >>  4) & sFreedMagic64[2]) + (v & sFreedMagic64[2]);
    v = ((v >>  8) & sFreedMagic64[3]) + (v & sFreedMagic64[3]);
    v = ((v >> 16) & sFreedMagic64[4]) + (v & sFreedMagic64[4]);
    v = ((v >> 32) & sFreedMagic64[5]) + (v & sFreedMagic64[5]);
    return v;
}

static inline int populationCount(uint32_t const val)
{
    uint32_t v = val;
    v = ((v >>  1) & sFreedMagic32[0]) + (v & sFreedMagic32[0]);
    v = ((v >>  2) & sFreedMagic32[1]) + (v & sFreedMagic32[1]);
    v = ((v >>  4) & sFreedMagic32[2]) + (v & sFreedMagic32[2]);
    v = ((v >>  8) & sFreedMagic32[3]) + (v & sFreedMagic32[3]);
    v = ((v >> 16) & sFreedMagic32[4]) + (v & sFreedMagic32[4]);
    return v;
}

static inline int populationCount(uint16_t const val)
{
    uint32_t v = val;
    v = ((v >>  1) & sFreedMagic32[0]) + (v & sFreedMagic32[0]);
    v = ((v >>  2) & sFreedMagic32[1]) + (v & sFreedMagic32[1]);
    v = ((v >>  4) & sFreedMagic32[2]) + (v & sFreedMagic32[2]);
    v = ((v >>  8) & sFreedMagic32[3]) + (v & sFreedMagic32[3]);
    return v;
}

static inline int populationCount(uint8_t const val)
{
    uint32_t v = val;
    v = ((v >>  1) & sFreedMagic32[0]) + (v & sFreedMagic32[0]);
    v = ((v >>  2) & sFreedMagic32[1]) + (v & sFreedMagic32[1]);
    v = ((v >>  4) & sFreedMagic32[2]) + (v & sFreedMagic32[2]);
    return v;
}

#endif


/**
 * Attempts to find at least @a numBitsToSet zero bits in the array @a words (containing at least
 * @a numBits bits). If @a numBitsToSet zero bits cannot be found, @c false is returned. Otherwise,
 * the first @a numBitsToSet zero bits in @a words are set to one and @c true is returned.
 * The indexes of the bits which were set to one are stored in the @a indexes array.
 *
 * @param[out]    indexes       An array of at least @a numBitsToSet integers, which is filled
 *                              with the indexes of zero bits that were set (to one).
 * @param[in,out] words         A memory buffer containing at least @a numBits bits
 * @param[in]     numBitsToSet  The number of bits to reset (to zero)
 * @param[in]     numBits       The number of bits in @a words
 *
 * @return     @c true if @a numBitsToSet zero bits were found and set to one in @a words,
 *             @c false otherwise.
 */
template <typename WordT>
bool set(
    int        * const indexes,
    WordT      * const words,
    int  const         numBitsToSet,
    int  const         numBits
) {
    assert(words   != 0 && numBits > 0 && "null or empty bitset");
    assert(indexes != 0 && numBitsToSet > 0 && "null or empty index array");

    bool  const special = (numBits & (BitTraits<WordT>::BITS_PER_WORD - 1)) > 0;
    WordT const last    = ~(maskForBit<WordT>(numBits) - 1);
    int   const nwords  = (numBits + (BitTraits<WordT>::BITS_PER_WORD - 1)) >>
                         BitTraits<WordT>::BITS_PER_WORD_LOG2;

    int zeroes = numBitsToSet;
    int i;
    for (i = 0; i < nwords - special && zeroes > 0; ++i) {
        zeroes -= BitTraits<WordT>::BITS_PER_WORD -
                  populationCount(static_cast<WordT>(words[i] & BitTraits<WordT>::WORD_MASK));
    }
    if (zeroes > 0 && special) {
        zeroes -= BitTraits<WordT>::BITS_PER_WORD -
                  populationCount(static_cast<WordT>((words[i] & BitTraits<WordT>::WORD_MASK) | last));
    }
    if (zeroes > 0) {
        // didn't find enough zeroes
        return false;
    }

    zeroes = 0;
    for (int i = 0; zeroes < numBitsToSet; ++i) {
        WordT w = words[i] & BitTraits<WordT>::WORD_MASK;
        if (w == BitTraits<WordT>::WORD_MASK) {
            continue;
        }
        WordT mask = 1;
        for (int j = 0; j < BitTraits<WordT>::BITS_PER_WORD; ++j, mask <<= 1) {
            if ((w & mask) == 0) {
                indexes[zeroes++] = (i << BitTraits<WordT>::BITS_PER_WORD_LOG2) + j;
                w |= mask;
                if (zeroes == numBitsToSet) {
                    break;
                }
            }
        }
        words[i] = w;
    }

    return true;
}


/**
 * Given the array @a words containing @a numBits bits and a list of @a numBitsToReset @a indexes,
 * resets (to zero) the bit at each index.
 *
 * @param[in,out] words             A memory buffer containing at least @a numBits bits
 * @param[in]     indexes           An array of at least @a numBitsToReset integers, each corresponding
 *                                  to the index of a single bit to reset (to zero).
 * @param[in]     numBitsToReset    The number of bits to reset (to zero)
 * @param[in]     numBits           The number of bits in @a words
 */
template <typename WordT>
void reset(
    WordT     * const words,
    int const * const indexes,
    int const         numBitsToReset,
    int const         numBits
) {
    assert(words != 0 && numBits > 0 && "null or empty bitset");
    assert(indexes != 0 && numBitsToReset >= 0 && "null or empty index array");

    for (int i = 0; i < numBitsToReset; ++i) {
        int const j = indexes[i];
        assert(j >= 0 && j < numBits);
        words[wordForBit<WordT>(j)] &= ~ maskForBit<WordT>(j);
    }
}


// Explicit instantiations
/// @cond
template bool set(int * const, uint8_t  * const, int const, int const);
template bool set(int * const, uint16_t * const, int const, int const);
template bool set(int * const, uint32_t * const, int const, int const);
template bool set(int * const, uint64_t * const, int const, int const);
template void reset(uint8_t  * const, int const * const, int const, int const);
template void reset(uint16_t * const, int const * const, int const, int const);
template void reset(uint32_t * const, int const * const, int const, int const);
template void reset(uint64_t * const, int const * const, int const, int const);
/// @endcond

}}} // end of namespace lsst::ap::detail

