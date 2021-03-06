
//  Copyright (C) 2007, 2008 Tim Blechmann & Thomas Grill
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  Disclaimer: Not a Boost library.

/* Copyright (c) 2008-2011, Stefan Eilemann <eile@equalizergraphics.com> 
   Modifications to use within co::base namespace and naming conventions.
   Original at http://tim.klingt.org/git?p=boost_lockfree.git;a=tree
*/

#ifndef COBASE_ATOMIC_H
#define COBASE_ATOMIC_H

#include <co/base/nonCopyable.h>    // base class
#include <co/base/compareAndSwap.h> // used in inline methods
#include <co/base/compiler.h>       // GCC version

#include <string> // _Atomic_word

namespace co
{
namespace base
{
#ifdef EQ_GCC_4_1_OR_LATER

/**
 * An variable with atomic operations.
 *
 * Atomic variables can be modified safely from multiple threads
 * concurrently. They are useful to implement lock-free algorithms.
 *
 * For implementation reasons, only signed atomic variables are supported, of
 * which only int32_t is implemented right now.
 */
template <typename T> class Atomic
{
public:
    /** Construct a new atomic variable with an initial value. @version 1.0 */
    explicit Atomic( const T v = 0 )
            : _value(v)
    {}

    /** Construct a copy of an atomic variable. Not thread-safe! @version 1.0 */
    explicit Atomic( const Atomic< T >& v )
            : _value( v._value )
    {}

    /** @return the current value @version 1.0 */
    operator T(void) const
    {
        return __sync_fetch_and_add(&_value, 0);
    }

    /** Assign a new value @version 1.0 */
    void operator = ( const T v )
    {
        _value = v;
        __sync_synchronize();
    }

    /** Assign a new value. Not thread-safe! @version 1.0 */
    void operator = ( const Atomic< T >& v)
    {
        _value = v._value;
        __sync_synchronize();
    }

    /** Atomically add a value and return the new value. @version 1.0 */
    T operator +=(T v)
    {
        return __sync_add_and_fetch(&_value, v);
    }

    /** Atomically substract a value and return the new value. @version 1.0 */
    T operator -=(T v)
    {
        return __sync_sub_and_fetch(&_value, v);
    }

    /** Atomically increment by one and return the new value. @version 1.0 */
    T operator ++(void)
    {
        return __sync_add_and_fetch(&_value, 1);
    }

    /** Atomically decrement by one and return the new value. @version 1.0 */
    T operator --(void)
    {
        return __sync_sub_and_fetch(&_value, 1);
    }

    /** Atomically increment by one and return the old value. @version 1.0 */
    T operator ++(int)
    {
        return __sync_fetch_and_add(&_value, 1);
    }

    /** Atomically decrement by one and return the old value. @version 1.0 */
    T operator --(int)
    {
        return __sync_fetch_and_sub(&_value, 1);
    }

private:
    mutable T _value;
};

#elif defined(__GLIBCPP__) || defined(__GLIBCXX__)

template <typename T> class Atomic
{
public:
    explicit Atomic( T v = 0 ) : _value(v) {}
    explicit Atomic( const Atomic< T >& v ) : _value( v._value ) {}

    operator T(void) const
    {
        return __gnu_cxx::__exchange_and_add(&_value, 0);
    }

    void operator = ( const T v ) { _value = v; }
    void operator = ( const Atomic< T >& v ) { _value = v._value; }

    T operator +=(T v)
    {
        return __gnu_cxx::__exchange_and_add(&_value, v) + v;
    }

    T operator -=(T v)
    {
        return __gnu_cxx::__exchange_and_add(&_value, -v) - v;
    }

    /* prefix operator */
    T operator ++(void)
    {
        return operator+=(1);
    }

    /* prefix operator */
    T operator --(void)
    {
        return operator-=(1);
    }

    /* postfix operator */
    T operator ++(int)
    {
        return __gnu_cxx::__exchange_and_add(&_value, 1);
    }

    /* postfix operator */
    T operator --(int)
    {
        return __gnu_cxx::__exchange_and_add(&_value, -1);
    }

private:
    mutable _Atomic_word _value;
};

#elif defined (EQ_HAS_COMPARE_AND_SWAP) /* emulate via compareAndSwap */

template <typename T> class Atomic
{
public:
    explicit Atomic( const T v ) { *this = v; }
    explicit Atomic( const Atomic< T >& v ) { *this = v; }
    explicit Atomic( ) { *this = 0; }
    
    operator T(void) const
    {
        memoryBarrier();
        return _value;
    }

    void operator = ( const T v )
    {
        _value = v;
        memoryBarrier();
    }

    void operator = ( const Atomic< T >& v )
    {
        _value = v._value;
        memoryBarrier();
    }

    /* prefix operator */
    T operator ++()
    {
        return *this += 1;
    }

    /* prefix operator */
    T operator --()
    {
        return *this -= 1;
    }

    T operator +=(T v)
    {
        for(;;)
        {
            T oldv = _value;
            T newv = oldv+v;
            if(compareAndSwap(&_value,oldv,newv))
                return newv;
        }
    }

    T operator -=(T v)
    {
        for(;;)
        {
            T oldv = _value;
            T newv = oldv-v;
            if(compareAndSwap(&_value,oldv,newv))
                return newv;
        }
    }

    /* postfix operator */
    T operator ++(int)
    {
        for(;;)
        {
            T oldv = _value;
            if(compareAndSwap(&_value,oldv,oldv+1))
                return oldv;
        }
    }

    /* postfix operator */
    T operator --(int)
    {
        for(;;)
        {
            T oldv = _value;
            if(compareAndSwap(&_value,oldv,oldv-1))
                return oldv;
        }
    }

private:
    T _value;
};

#else
#  warning ("Atomic emulation")
#  include <co/base/lock.h>       // used in inline methods

template <typename T>
class Atomic: public NonCopyable
{
public:
    explicit Atomic(T v = 0)
    {
        *this = v;
    }

    operator T(void) const
    {
        return _value;
    }

    void operator =(T v)
    {
        _lock.set();
        _value = v;
        _lock.unset();
    }

    /* prefix operator */
    T operator ++()
    {
        return *this += 1;
    }

    /* prefix operator */
    T operator --()
    {
        return *this -= 1;
    }

    T operator +=(T v)
    {
        _lock.set();
        _value += v;
        T newv = _value;
        _lock.unset();

        return newv;
    }

    T operator -=(T v)
    {
        _lock.set();
        _value -= v;
        T newv = _value;
        _lock.unset();

        return newv;
    }

    /* postfix operator */
    T operator ++(int)
    {
        _lock.set();
        T oldv = _value;
        ++_value;
        _lock.unset();
        
        return oldv;
    }

    /* postfix operator */
    T operator --(int)
    {
        _lock.set();
        T oldv = _value;
        --_value;
        _lock.unset();

        return oldv;
    }

private:
    T    _value;
    Lock _lock;
};
#endif
typedef Atomic< long > a_int32_t; //!< An atomic 32 bit integer variable

}
}
#endif  // COBASE_ATOMIC_H
