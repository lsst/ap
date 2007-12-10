// -*- lsst-c++ -*-
//
//##====----------------                                ----------------====##/
//
//! \file   Condition.h
//! \brief  Simple wrapper for POSIX condition variables.
//
//##====----------------                                ----------------====##/

#ifndef LSST_AP_CONDITION_H
#define LSST_AP_CONDITION_H

#include <cassert>
#include <errno.h>
#include <pthread.h>

#include <boost/noncopyable.hpp>

#include <lsst/ap/Mutex.h>
#include <lsst/ap/Time.h>


namespace lsst {
namespace ap {

/*! \brief  Encapsulates a POSIX condition variable. */
template <typename MutexType>
class Condition : private boost::noncopyable {

public :

    typedef ScopedLock<MutexType> LockType;

    Condition();

    ~Condition() {
        int result = ::pthread_cond_destroy(&_condition);
        assert(result == 0);
    }

    /*!
        Waits on the condition variable until the calling thread is woken as a result
        of another thread calling notify() or notifyAll(). Spurious wakeup can occur.

        \pre    \a lock has been successfully acquired
     */
    void wait(LockType & lock) {
        assert(lock.isAcquired());
        int result = ::pthread_cond_wait(&_condition, lock.getPosixMutex());
        assert(result == 0);
    }

    /*!
        Waits on the condition variable until the given predicate evaluates to \c true.

        \pre    \a lock has been successfully acquired
     */
    template <typename P>
    void wait(LockType & lock, P predicate)
    {
        assert(lock.isAcquired());
        while (!predicate()) {
            int result = ::pthread_cond_wait(&_condition, lock.getPosixMutex());
            assert(result == 0);
        }
    }

    /*!
        Waits on this condition variable until either the given deadline expires or the calling
        thread is woken as a result of another thread calling notify() or notifyAll(). Spurious
        wakeup can occur.

        \pre    \a lock has been successfully acquired
        \return \c false if the deadline was missed, and \c true otherwise.
     */
    bool wait(LockType & lock, TimeSpec const & ts) {
        assert(lock.isAcquired());
        int result = ::pthread_cond_timedwait(&_condition, lock.getPosixMutex(), &ts);
        if (result == ETIMEDOUT) {
            return false;
        }
        assert(result == 0);
        return true;
    }

    /*!
        Waits on this condition variable until the given predicate evaluates to \c true
        or the given deadline is missed.

        \pre    \a lock has been successfully acquired
        \return \c true if the predicate became \c true before the deadline expired,
                and \c false if the deadline was missed.
     */
    template <typename P>
    bool wait(LockType & lock, P predicate, TimeSpec const & deadline) {
        assert(lock.isAcquired());
        while (!predicate()) {
            int result = ::pthread_cond_timedwait(&_condition, lock.getPosixMutex(), &deadline);
            if (result == ETIMEDOUT) {
                return false;
            }
            assert(result == 0);
        }
        return true;
    }

    /*! Wakes up at least one thread waiting on the condition. For predictable scheduling, the
        mutex associated with the condition should be acquired prior to calling this method. */
    void notify() {
        int result = ::pthread_cond_signal(&_condition);
        assert(result == 0);
    }

    /*! Wakes up all threads waiting on the condition. For predictable scheduling, the
        mutex associated with the condition should be acquired prior to calling this method. */
    void notifyAll() {
        int result = ::pthread_cond_broadcast(&_condition);
        assert(result == 0);
    }

private :

    ::pthread_cond_t _condition;
};


}} // end of namespace lsst::ap

#endif // LSST_AP_CONDITION_H
