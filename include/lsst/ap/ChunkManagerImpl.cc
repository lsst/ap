// -*- lsst-c++ -*-
//
//##====----------------                                ----------------====##/
//
//! \file   ChunkManagerImpl.cc
//
//##====----------------                                ----------------====##/

#ifndef LSST_AP_CHUNK_MANAGER_IMPL_CC
#define LSST_AP_CHUNK_MANAGER_IMPL_CC

#include <iostream>

#include <boost/format.hpp>

#include "Chunk.cc"
#include "ChunkManagerImpl.h"
#include "SpatialUtil.h"


namespace lsst {
namespace ap {
namespace detail {

// -- HashedSet ----------------

/*!
    Returns the 32 bit hash of a 32 bit value using Thomas Wang's
    <a href="http://www.concentric.net/~Ttwang/tech/inthash.htm">mixing function</a>.
 */
inline uint32_t hash(uint32_t key) {
    key = (~key) + (key << 15); // key = (key << 15) - key - 1;
    key = key ^ (key >> 12);
    key = key + (key << 2);
    key = key ^ (key >> 4);
    key = key * 2057;           // key = (key + (key << 3)) + (key << 11);
    key = key ^ (key >> 16);
    return key;
}

/*!
    Returns the 32 bit hash of a 64 bit value using Thomas Wang's
    <a href="http://www.concentric.net/~Ttwang/tech/inthash.htm">mixing function</a>.
 */
inline uint32_t hash(uint64_t key) {
    key = (~key) + (key << 18); // key = (key << 18) - key - 1;
    key = key ^ (key >> 31);
    key = key * 21; // key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 11);
    key = key + (key << 6);
    key = key ^ (key >> 22);
    return static_cast<uint32_t>(key);
}

inline uint32_t hash(int32_t key) { return hash(static_cast<uint32_t>(key)); }
inline uint32_t hash(int64_t key) { return hash(static_cast<uint64_t>(key)); }


template <typename EntryType, uint32_t NumEntries>
HashedSet<EntryType, NumEntries>::HashedSet() :
    _free(0), _size(0)
{
    for (uint32_t i = 0; i < 2*NumEntries; ++i) {
        _hashTable[i] = -1;
    }
    // initialized linked list of free entries (embedded in the entries themselves)
    for (uint32_t i = 0; i < NumEntries - 1; ++i) {
        _entries[i].setId(-1);
        _entries[i].setNextInChain(static_cast<int>(i + 1));
    }
    _entries[NumEntries - 1].setNextInChain(-1);
}


/*!
    Returns a pointer to the entry with the given identifier, or null if there is no such entry.

    \param[in] id   The identifier of the entry to find.
    \return         A pointer to the entry with the given identifier, or null if no such entry was found.
 */
template <typename EntryType, uint32_t NumEntries>
EntryType const * HashedSet<EntryType, NumEntries>::doFind(int64_t const id) const {
    int i = _hashTable[hash(id) & (2*NumEntries - 1)];
    while (i >= 0) {
        if (id == _entries[i].getId()) {
            return &_entries[i];
        }
        i = _entries[i].getNextInChain();
    }
    return 0;
}


/*!
    Returns a pointer to a freshly initialized entry with the given identifier,
    or null if an entry with the given identifier already exists.

    \param[in] id   The identifier (unique within this set) of the entry to insert.
    \return         A pointer to a freshly default-constructed entry with identifier set to \a id,
                    or null if either a preexisting entry with the given identifier was found or
                    no space for new entries remains.
 */
template <typename EntryType, uint32_t NumEntries>
EntryType * HashedSet<EntryType, NumEntries>::insert(int64_t const id) {

    // check that there is space for another entry
    if (_free < 0) {
        return 0;
    }

    uint32_t const bucket = hash(id) & (2*NumEntries - 1);

    int i    = _hashTable[bucket];
    int last = -1;

    while (i >= 0) {
        if (id == _entries[i].getId()) {
            return 0; // already have an entry with the given id
        }
        last = i;
        i    = _entries[i].getNextInChain();
    }

    // take an entry off the free list
    int const c = _free;
    _free = _entries[c].getNextInChain();

    if (last < 0) {
        _hashTable[bucket] = c;
    } else {
        // hash collision - chain to the end of the bucket
        _entries[last].setNextInChain(c);
    }

    // basic entry initialization, then return
    new (&_entries[c]) EntryType();
    _entries[c].setId(id);
    _entries[c].setNextInChain(-1);
    ++_size;
    return &_entries[c];
}


/*!
    Returns a pointer to a preexisting or freshly default-constructed entry
    with the given identifier, along with a boolean indicating whether the entry was
    inserted (\c true) or found (\c false). The pointer returned is null if and
    only if a fresh entry was required but there were no free entries available.

    \param[in] id   The identifier (unique within this set) of the entry to find or insert.
    \return         A pointer to the entry with the given id along with a boolean indicating
                    whether the entry was inserted (\c true) or found (\c false).
 */
template <typename EntryType, uint32_t NumEntries>
std::pair<EntryType *, bool> HashedSet<EntryType, NumEntries>::findOrInsert(int64_t const id) {

    uint32_t const bucket = hash(id) & (2*NumEntries - 1);

    int i    = _hashTable[bucket];
    int last = -1;

    while (i >= 0) {
        if (id == _entries[i].getId()) {
            return std::pair<EntryType *, bool>(&_entries[i], false); // found an entry with the given id
        }
        last = i;
        i    = _entries[i].getNextInChain();
    }

    // check that there is a free chunk, if so use it
    int const c = _free;
    if (c < 0) {
        return std::pair<EntryType *, bool>(0, true);
    }
    _free = _entries[c].getNextInChain();

    if (last < 0) {
        _hashTable[bucket] = c;
    } else {
        // hash collision - chain to the end of the bucket
        _entries[last].setNextInChain(c);
    }

    // basic chunk initialization, then return
    new (&_entries[c]) EntryType();
    _entries[c].setId(id);
    _entries[c].setNextInChain(-1);
    ++_size;
    return std::pair<EntryType *, bool>(&_entries[c], true);
}


/*!
    Erases the entry with the given id, returning \c true if an entry with the given id was found.

    \param[in] id   The id of the entry to erase.
    \return         \c true if an entry with the given id was found (and erased).
 */
template <typename EntryType, uint32_t NumEntries>
bool HashedSet<EntryType, NumEntries>::erase(int64_t const id) {

    uint32_t const bucket = hash(id) & (2*NumEntries - 1);

    int i    = _hashTable[bucket];
    int last = -1;

    while (i >= 0) {
        if (id == _entries[i].getId()) {
            // found entry to erase, unlink it from the chain for its bucket
            if (last < 0) {
                _hashTable[bucket] = _entries[i].getNextInChain();
            } else {
                _entries[last].setNextInChain(_entries[i].getNextInChain());
            }
            // append the entry to the free list
            _entries[i].setId(-1);
            _entries[i].setNextInChain(_free);
            _free = i;
            --_size;
            return true;
        }
        last = i;
        i    = _entries[i].getNextInChain();
    }
    return false;
}


// -- BlockAllocator ----------------

/*!
    Creates a new BlockAllocator instance. The memory blocks to be tracked by the allocator are located
    in contiguous memory, starting \a offset bytes after the given \a reference address.

    \param[in] reference    The address relative to which \a offset is specified.
    \param[in] offset       The location of the first memory block in the pool of contiguous blocks
                            to be managed by this allocator instance, specified as an offset in bytes
                            relative to the \a reference address.
 */
template <typename MutexType, typename DataType, typename TraitsType>
BlockAllocator<MutexType, DataType, TraitsType>::BlockAllocator(
    uint8_t const * const reference,
    size_t  const         offset
) :
    _mutex(),
    _allocator(),
    _offset(static_cast<size_t>((reference + offset) - reinterpret_cast<uint8_t * >(this)))
{
    _allocator.reset();
}


/*!
    Allocates a single memory block.

    \return     The offset (in bytes relative to the address of this allocator instance)
                of the newly allocated block.

    \throw std::bad_alloc   Thrown if no free block was available.
 */
template <typename MutexType, typename DataType, typename TraitsType>
size_t BlockAllocator<MutexType, DataType, TraitsType>::allocate() {
    int i[1];
    ScopedLock<MutexType> lock(_mutex);
    if (!_allocator.set(i, 1)) {
        throw std::bad_alloc();
    }
    return _offset + i[0]*BLOCK_SIZE;
}


/*!
    Allocates \a n memory blocks, storing their offsets in the given array.

    \param[out] blockOffsets    The array in which the offsets (relative to this allocator instance) of
                                allocated memory blocks are stored. Assumed to be of length at least \a n.
    \param[in]  n               The number of memory blocks to allocate.

    \throw std::bad_alloc   Thrown if there were less than \a n free blocks available.
 */
template <typename MutexType, typename DataType, typename TraitsType>
void BlockAllocator<MutexType, DataType, TraitsType>::allocate(
    size_t * const blockOffsets,
    uint32_t const n
) {
    if (n < 0 || n > TraitsType::MAX_BLOCKS_PER_CHUNK) {
        LSST_AP_THROW(OutOfRange, "invalid number of memory blocks in allocation request");
    }
    int i[TraitsType::MAX_BLOCKS_PER_CHUNK];

    ScopedLock<MutexType> lock(_mutex);
    if (!_allocator.set(i, n)) {
        throw std::bad_alloc();
    }
    for (uint32_t j = 0; j < n; ++j) {
        blockOffsets[j] = _offset + i[j]*BLOCK_SIZE;
    }
}


/*!
    Frees \a n memory blocks, identified by offsets stored in the given array. Never throws.

    \param[in] blockOffsets The array in which the offsets (relative to this allocator instance) of
                            the memory blocks to free are stored. Assumed to be of length at least \a n.
    \param[in] n            The number of memory blocks to free.
 */
template <typename MutexType, typename DataType, typename TraitsType>
void BlockAllocator<MutexType, DataType, TraitsType>::free(
    size_t   const * const blockOffsets,
    uint32_t const         n
) {
    assert(n >= 0 && n <= TraitsType::MAX_BLOCKS_PER_CHUNK &&
        "invalid number of memory blocks in free request");

    // translate block offsets to block indexes
    int i[TraitsType::MAX_BLOCKS_PER_CHUNK];
    for (uint32_t j = 0; j < n; ++j) {
        size_t off = blockOffsets[j] - _offset;
        assert(off < TraitsType::NUM_BLOCKS*BLOCK_SIZE && "block was not allocated by this allocator");
        assert(off % BLOCK_SIZE == 0 && "invalid block address");
        i[j] = static_cast<int>(off/BLOCK_SIZE);
    }

    // clear bit corresponding to each block to free
    ScopedLock<MutexType> lock(_mutex);
    _allocator.reset(i, n);
}


// -- VisitTracker ----------------

/*!
    Returns \c true if the given visit is being tracked by this
    VisitTracker and has not been marked as failed.
 */
bool VisitTracker::isValid(int64_t const visitId) const {
    Visit const * v = this->find(visitId);
    if (v == 0) {
        return false;
    }
    return !v->failed();
}

void VisitTracker::print(std::ostream & os) const {
    std::vector<int64_t> v;
    v.reserve(size());
    Visit const * const e = end();
    for (Visit const * beg = begin(); beg != e; ++beg) {
        int64_t const id = beg->getId();
        if (id >= 0) {
            v.push_back(id);
        }
    }
    if (v.empty()) {
        os << "    No visits being tracked";
    } else {
        std::sort(v.begin(), v.end());
        boost::format fmt("    visit %1% %|32t|: %2%\n");
        for (std::vector<int64_t>::const_iterator i = v.begin(); i != v.end(); ++i) {
            Visit const * v = find(*i);
            os << fmt % v->getId() % (v->failed() ? "failed" : "in-flight");
        }
    }
    os << std::endl;
}

void VisitTracker::print(int64_t const visitId, std::ostream & os) const {
    Visit const * v = find(visitId);
    boost::format fmt("    visit %1% %|32t|: %2%\n");
    fmt % visitId;
    if (v == 0) {
        os << fmt % "not being tracked";
    } else {
        os << fmt % (v->failed() ? "failed" : "in-flight");
    }
    os << std::endl;
}


// -- SubManager ----------------

/*!
    Registers the given visit as an interested party of each of the given chunks. If any of the
    given identifiers doesn't correspond to a chunk, an empty chunk is created. Newly created
    chunks are stored in the \a toRead list (indicating data for them must be read from disk),
    previously existing chunks are returned in the \a toWaitFor list (indicating that the
    visit must wait until it owns those instances before processing can begin).

    \param[out] toRead      Set to the list of chunks that were not found in memory
                            and must be read in from disk.
    \param[out] toWaitFor   Set to the list of chunks instances that are already in memory
                            and must be waited on.
    \param[in]  visitId     The visit to register interest or create chunks for.
    \param[in]  chunkIds    A list of identifiers for chunks required by the visit.
                            Assumed to be duplicate free.
 */
template <typename MutexType, typename DataType, typename TraitsType>
void SubManager<MutexType, DataType, TraitsType>::createOrRegisterInterest(
    std::vector<ChunkType>       & toRead,
    std::vector<ChunkType>       & toWaitFor,
    int64_t                const   visitId,
    std::vector<int64_t>   const & chunkIds
) {
    std::vector<int64_t>::const_iterator const end = chunkIds.end();
    for (std::vector<int64_t>::const_iterator i = chunkIds.begin(); i != end; ++i) {
        std::pair<ChunkDescriptorType *, bool> p(_chunks.findOrInsert(*i));
        if (p.second) {
            // new chunk descriptor was allocated
            if (p.first == 0) {
                throw std::bad_alloc(); // ran out of space
            }
            p.first->_visitId = visitId;
            p.first->_usable  = false;
            toRead.push_back(ChunkType(p.first, &_allocator));
        } else {
            // existing chunk descriptor was found
            assert(p.first != 0);
            p.first->_interestedParties.enqueue(visitId);
            toWaitFor.push_back(ChunkType(p.first, &_allocator));
        }
    }
}


/*!
    Checks to see whether the chunks in the \a toWaitFor list are owned by the given visit.
    Any chunks that have had their ownership transferred to the given visit are removed from
    \a toWaitFor. Of these chunks, the subset that were not completely read into memory (that
    is, whose previous owners failed while reading them in) are appended to \a toRead.

    \param[out]    toRead       Set to the list of chunks now owned by the given visit,
                                but which must be re-read from disk.
    \param[in,out] toWaitFor    The list of chunks for which ownership must be checked. Any chunks
                                now owned by the given visit are removed from the list by the call.
    \param[in]     visitId      An identifier for a visit to a FOV.

    \return     \c true if and only if all the chunks initially in the \a toWaitFor list now
                belong to the given visit.
 */
template <typename MutexType, typename DataType, typename TraitsType>
bool SubManager<MutexType, DataType, TraitsType>::checkForOwnership(
    std::vector<ChunkType> & toRead,
    std::vector<ChunkType> & toWaitFor,
    int64_t const            visitId
) {
    size_t size   = toWaitFor.size();
    size_t i      = 0;

    while (i < size) {
        ChunkType & c = toWaitFor[i];
        if (c.getVisitId() != visitId) {
            ++i;
        } else {
            if (!c.isUsable()) {
                c.clear();
                toRead.push_back(c);
            }
            // deletes element i in O(1) time (but changes element ordering)
            --size;
            if (i < size) {
                toWaitFor[i] = toWaitFor[size];
            }
            toWaitFor.pop_back();
        }
    }
    return toWaitFor.empty();
}


/*!
    Returns a chunk for each of the given identifiers that corresponds to a chunk
    managed by this SubManager.

    \param[out] chunks      Set to the list of chunk instances managed by this SubManager and with an
                            identifier in \a chunkIds.
    \param[in]  chunkIds    The list of chunk identifiers to return chunk instances for.
                            Assumed to be duplicate free.
 */
template <typename MutexType, typename DataType, typename TraitsType>
void SubManager<MutexType, DataType, TraitsType>::getChunks(
    std::vector<ChunkType>     & chunks,
    std::vector<int64_t> const & chunkIds
) {
    std::vector<int64_t>::const_iterator const end = chunkIds.end();
    for (std::vector<int64_t>::const_iterator i = chunkIds.begin(); i != end; ++i) {
        ChunkDescriptorType * d = _chunks.find(*i);
        if (d != 0) {
            chunks.push_back(ChunkType(d, &_allocator));
        }
    }
}


/*!
    Relinquishes ownership of any chunks owned by the given visit (each chunk is passed on to
    its first interested party that is still in flight).

    \param[in] visitId      The visit owning the chunks to relinquish ownership of.
    \param[in] rollback     Flag indicating whether or not in-memory changes to a chunk should
                            be rolled back (\c true) or committed (\c false) prior to relinquishing
                            ownership.
    \param[in] tracker      Tracks the status of visits (whether or not a visit is in flight,
                            and if so, whether or not it has failed).

    \return     \c true if any chunks changed hands.
 */
template <typename MutexType, typename DataType, typename TraitsType>
bool SubManager<MutexType, DataType, TraitsType>::relinquishOwnership(
    int64_t      const   visitId,
    bool         const   rollback,
    VisitTracker const & tracker
) {
    bool change = false;
    ChunkDescriptorType * const end = _chunks.end();
    for (ChunkDescriptorType * i = _chunks.begin(); i != end; ++i) {
        if (i->getId() != -1 && i->_visitId == visitId) {
            bool foundSuccessor = false;
            while (!i->_interestedParties.empty()) {
                int64_t const nextVisitId = i->_interestedParties.dequeue();
                if (tracker.isValid(nextVisitId)) {
                    i->_visitId    = nextVisitId;
                    change         = true;
                    foundSuccessor = true;
                    break;
                }
            }
            if (foundSuccessor) {
                ChunkType c(i, &_allocator);
                if (rollback) {
                    c.rollback();
                } else {
                    c.commit();
                }
            } else {
                // deallocate chunk
                _allocator.free(i->_blocks, i->_numBlocks);
                _chunks.erase(i->getId());
            }
        }
    }
    return change;
}


namespace {

    template <typename T> struct PtrLessThan {
        bool operator()(T const * t1, T const * t2) { return *t1 < *t2; }
    };

    template <typename ChunkDescriptorType>
    bool mergePrint(ChunkDescriptorType const * d1, ChunkDescriptorType const * d2) {
        if (d1->_visitId != d1->_visitId || d1->_usable != d2->_usable) {
            return false;
        }
        if (d2->_interestedParties.empty() != d2->_interestedParties.empty()) {
            return false;
        }
        return ZoneStripeChunkDecomposition::chunkToStripe(d1->_chunkId) ==
               ZoneStripeChunkDecomposition::chunkToStripe(d2->_chunkId);
    }

    template <typename ChunkDescriptorType>
    void printChunks(std::ostream & os, std::vector<ChunkDescriptorType const *> const & v) {

        boost::format oneFmt ("        chunk  %1%     in stripe %2% %|32t|: %3%%4%\n");
        boost::format manyFmt("        chunks %1%-%2% in stripe %3% %|32t|: %4%%5%\n");

        uint32_t start = 0;
        ChunkDescriptorType const * c = v[0];

        for (uint32_t i = 1; i <= v.size(); ++i) {
            if (i < v.size() && mergePrint(c, v[i])) {
                continue;
            }
            if (i - start > 1) {
                manyFmt % ZoneStripeChunkDecomposition::chunkToSequence(c->_chunkId);
                manyFmt % ZoneStripeChunkDecomposition::chunkToSequence(v[i - 1]->_chunkId);
                manyFmt % ZoneStripeChunkDecomposition::chunkToStripe(c->_chunkId);
                manyFmt % (c->_usable ? "  usable" : "unusable");
                manyFmt % (c->_interestedParties.empty() ? "" : ", interesting");
                os << manyFmt;
            } else {
                oneFmt % ZoneStripeChunkDecomposition::chunkToSequence(c->_chunkId);
                oneFmt % ZoneStripeChunkDecomposition::chunkToStripe(c->_chunkId);
                oneFmt % (c->_usable ? "  usable" : "unusable");
                oneFmt % (c->_interestedParties.empty() ? "" : ", interesting");
                os << oneFmt;
            }
            if (i < v.size()) {
                if (c->_visitId != v[i]->_visitId) {
                    os << "    Owned by visit " << v[i]->_visitId << ":\n";
                }
                start = i;
                c = v[i];
            }
        }
    }

} // end of anonymous namespace


template <typename MutexType, typename DataType, typename TraitsType>
void SubManager<MutexType, DataType, TraitsType>::print(std::ostream & os) const {
    ChunkDescriptorType const * const end = _chunks.end();
    std::vector<ChunkDescriptorType const *> v;
    v.reserve(_chunks.size());
    for (ChunkDescriptorType const * beg = _chunks.begin(); beg != end; ++beg) {
        if (beg->_chunkId != -1) {
            v.push_back(beg);
        }
    }
    std::sort(v.begin(), v.end(), PtrLessThan<ChunkDescriptorType>());
    os << "    Chunks with an owner";
    if (v.empty()) {
        os << ": None";
    } else {
        os << ":\n";
        os << "    Owned by visit " << v[0]->_visitId << ":\n";
        printChunks(os, v);
    }
    os << std::endl;
}


template <typename MutexType, typename DataType, typename TraitsType>
void SubManager<MutexType, DataType, TraitsType>::print(
    int64_t const   chunkId,
    std::ostream  & os
) const {
    ChunkDescriptorType const * c = _chunks.find(chunkId);
    os << "    [" << c->_chunkId << "] chunk " << 
          ZoneStripeChunkDecomposition::chunkToSequence(chunkId) << " in stripe " <<
          ZoneStripeChunkDecomposition::chunkToStripe(chunkId);
    if (c == 0) {
        os << ": not being tracked\n";
    } else {
        os << ":\n        " << (c->_usable ? "usable" : "unusable") << "\n        ";
        if (c->_interestedParties.empty()) {
            os << "un";
        }
        os << "interesting\n        ";
        uint32_t sz = c->_size;
        os << sz << " entries in " << c->_nextBlock << " blocks (" <<
              c->_numBlocks << " allocated)\n        ";
        if (sz <= c->_delta) {
            sz = 0;
        } else {
            sz -= c->_delta;
        }
        os << sz << " entries in delta\n";
    }
    os << std::endl;
}


template <typename MutexType, typename DataType, typename TraitsType>
void SubManager<MutexType, DataType, TraitsType>::printVisit(
    int64_t const   visitId,
    std::ostream  & os
) const {
    ChunkDescriptorType const * const end = _chunks.end();
    std::vector<ChunkDescriptorType const *> v;
    v.reserve(_chunks.size());
    for (ChunkDescriptorType const * beg = _chunks.begin(); beg != end; ++beg) {
        if (beg->_chunkId != -1 && beg->_visitId == visitId) {
            v.push_back(beg);
        }
    }
    std::sort(v.begin(), v.end(), PtrLessThan<ChunkDescriptorType>());
    os << "    Chunks belonging to visit " << visitId;
    if (v.empty()) {
        os << ": None";
    } else {
        os << ":\n";
        printChunks(os, v);
    }
    os << std::endl;
}


// -- ChunkManagerSingleImpl ----------------

template <typename MutexType, typename DataType, typename TraitsType>
ChunkManagerSingleImpl<MutexType, DataType, TraitsType>::ChunkManagerSingleImpl() :
    _data(reinterpret_cast<uint8_t *>(this), blocks())
{}


/*! Returns \c true if the given visit is in-flight and has not been marked as failed. */
template <typename MutexType, typename DataType, typename TraitsType>
bool ChunkManagerSingleImpl<MutexType, DataType, TraitsType>::isVisitInFlight(int64_t const visitId) {
    ScopedLock<MutexType> lock(_mutex);
    return _visits.isValid(visitId);
}


/*!
    Marks the given visit a failure. If the given visit has not been previously
    registered, or has already been marked as failed, then the call has no effect.
 */
template <typename MutexType, typename DataType, typename TraitsType>
void ChunkManagerSingleImpl<MutexType, DataType, TraitsType>::failVisit(int64_t const visitId) {
    ScopedLock<MutexType> lock(_mutex);
    Visit * v = _visits.find(visitId);
    if (v != 0) {
        v->setFailed();
    }
}


/*! Registers the given visit as in-flight without performing any further action. */
template <typename MutexType, typename DataType, typename TraitsType>
void ChunkManagerSingleImpl<MutexType, DataType, TraitsType>::registerVisit(int64_t const visitId) {
    ScopedLock<MutexType> lock(_mutex);
    if (_visits.find(visitId) != 0) {
        LSST_AP_THROW(
            InvalidParameter,
            boost::format("Cannot start processing visit %1%: visit is already in flight") % visitId
        );
    }
    if (_visits.space() == 0) {
        LSST_AP_THROW(
            LengthError,
            boost::format("Cannot register visit %1%: too many visits in-flight") % visitId
        );
    }
    Visit * v = _visits.insert(visitId);
    assert(v != 0);
}


/*!
    Begins visit processing by registering the given visit as an interested party of each chunk with
    identifier in the given list. If any identifier in the list does not have a corresponding chunk,
    a new chunk (owned by the specified visit) is created.

    Note that the \a toWaitFor and \a toRead output vectors are cleared immediately on entry to the
    function. Under the assumption that these vectors are empty to begin with, strong exception safety
    is guaranteed.

    \param[out] toRead      Set to the list of newly created chunks that must be read from disk.
    \param[out] toWaitFor   Set to the list of chunks that are already in memory and must be waited on.
    \param[in]  visitId     The visit to begin.
    \param[in]  chunkIds    Identifiers for chunks to register an interest in or create.
 */
template <typename MutexType, typename DataType, typename TraitsType>
void ChunkManagerSingleImpl<MutexType, DataType, TraitsType>::startVisit(
    std::vector<ChunkType>       & toRead,
    std::vector<ChunkType>       & toWaitFor,
    int64_t                const   visitId,
    std::vector<int64_t>   const & chunkIds
) {
    toRead.clear();
    toWaitFor.clear();

    // ensure external resources necessary for success are available
    toRead.reserve(chunkIds.size());
    toWaitFor.reserve(chunkIds.size());

    ScopedLock<MutexType> lock(_mutex);
    // ensure internal resources necessary for success are available
    if (_data.space() < chunkIds.size()) {
        LSST_AP_THROW(LengthError, "requested additional chunks exceed chunk manager capacity");
    }
    if (!_visits.isValid(visitId)) {
        LSST_AP_THROW(
            InvalidParameter,
            boost::format("Cannot start processing for visit %1%: visit is not in-flight") % visitId
        );
    }
    // having pre-allocated/checked that there is space for everything,
    // manager state can be modified without throwing
    _data.createOrRegisterInterest(toRead, toWaitFor, visitId, chunkIds);
}


/*!
    Blocks the calling thread until the given visit owns every one of the given chunks.

    Note that the vector \a toRead passed into the method is assumed to be empty -
    it is immediately cleared upon entry to the function.

    \param[out]    toRead       Set to the list of chunks acquired by the given visit,
                                but which must be re-read from disk.
    \param[in,out] toWaitFor    The list of chunks that must be owned by the given visit before
                                returning - acquired chunks are removed from the list, so that
                                the list is empty on successfull return.
    \param[in]     visitId      The visit that must wait for chunk ownership.
    \param[in]     deadline     The point in time after which chunk acquisition should be abandoned.

    \throw  Timeout     Thrown if the given visit deadline expired while
                        waiting to acquire the specified chunks.
 */
template <typename MutexType, typename DataType, typename TraitsType>
void ChunkManagerSingleImpl<MutexType, DataType, TraitsType>::waitForOwnership(
    std::vector<ChunkType> & toRead,
    std::vector<ChunkType> & toWaitFor,
    int64_t          const   visitId,
    TimeSpec         const & deadline
) {
    toRead.clear();
    toRead.reserve(toWaitFor.size());

    ScopedLock<MutexType> lock(_mutex);
    while (true) {
        if (_data.checkForOwnership(toRead, toWaitFor, visitId)) {
            break; // all chunks belong to the visit - ok to proceed
        }
        // wait for ownership
        if (!_ownerCondition.wait(lock, deadline)) {
            LSST_AP_THROW(Timeout, boost::format("Deadline for visit %1% expired") % visitId);
        }
    }
}


/*!
    Returns a chunk for each identifier in the given list that corresponds to a managed chunk.

    \param[out] chunks      The list to store chunks in.
    \param[in]  chunkIds    The list of identifiers for which corresponding chunks are desired.
 */
template <typename MutexType, typename DataType, typename TraitsType>
void ChunkManagerSingleImpl<MutexType, DataType, TraitsType>::getChunks(
    std::vector<ChunkType>     & chunks,
    std::vector<int64_t> const & chunkIds
) {
    ScopedLock<MutexType> lock(_mutex);
    _data.getChunks(chunks, chunkIds);
}


/*!
    Relinquishes ownership of any chunks owned by the given visit (each chunk is passed on to
    its first interested party that is still in flight) and removes the given visit from the
    list of in-flight visits.

    \param[in] visitId      The visit to remove from the set of in-flight visits being tracked
    \param[in] rollback     Flag indicating whether chunks should be rolled back (to the state
                            they were in before being acquired by the given visit), or whether
                            changes should be marked as committed.

    \return     \c true if the visit existed, was not marked as a failure and was committed,
                \c false otherwise.
 */
template <typename MutexType, typename DataType, typename TraitsType>
bool ChunkManagerSingleImpl<MutexType, DataType, TraitsType>::endVisit(
    int64_t const visitId,
    bool    const rollback
) {
    ScopedLock<MutexType> lock(_mutex);
    bool roll = rollback || !_visits.isValid(visitId);
    if (!_visits.erase(visitId)) {
        return false;
    }
    // relinquish chunk ownership: if any chunks change hands, notify all threads
    // waiting on chunk ownership to check whether they can proceed
    if (_data.relinquishOwnership(visitId, roll, _visits)) {
        _ownerCondition.notifyAll();
    }
    return !roll;
}


template <typename MutexType, typename DataType, typename TraitsType>
void ChunkManagerSingleImpl<MutexType, DataType, TraitsType>::printVisits(std::ostream & os) const {
    ScopedLock<MutexType> lock(_mutex);
    _visits.print(os);
}


template <typename MutexType, typename DataType, typename TraitsType>
void ChunkManagerSingleImpl<MutexType, DataType, TraitsType>::printChunks(std::ostream & os) const {
    ScopedLock<MutexType> lock(_mutex);
    _data.print(os);
}


template <typename MutexType, typename DataType, typename TraitsType>
void ChunkManagerSingleImpl<MutexType, DataType, TraitsType>::printVisit(
    int64_t const   visitId,
    std::ostream  & os
) const {
    ScopedLock<MutexType> lock(_mutex);
    _visits.print(visitId, os);
    _data.printVisit(visitId, os);
}


template <typename MutexType, typename DataType, typename TraitsType>
void ChunkManagerSingleImpl<MutexType, DataType, TraitsType>::printChunk(
    int64_t const   chunkId,
    std::ostream  & os
) const {
    ScopedLock<MutexType> lock(_mutex);
    _data.print(chunkId, os);
}


}}} // end of namespace lsst::ap::detail

#endif // LSST_AP_CHUNK_MANAGER_IMPL_CC

