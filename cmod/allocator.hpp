/*
 *Copyright (c) 2017-2018, yinqiwen <yinqiwen@gmail.com>
 *All rights reserved.
 *
 *Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Redis nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PTMALLOC_ALLOCATOR_HPP_
#define PTMALLOC_ALLOCATOR_HPP_

#include <memory>
#include <new>
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <stdio.h>
#include <boost/interprocess/offset_ptr.hpp>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <pthread.h>
#include <mutex>

//4KB Meta size
#define ALLOCATOR_META_SIZE 4096
#define META_USER_PADDING_SIZE 2048
#define MAX_ROOT_STRUCT_SIZE 1024
#define MAX_NAMING_TABLE_SIZE 256

extern "C"
{
    extern void *mspace_malloc(void *msp, size_t bytes);
    extern void *mspace_realloc(void *msp, void *oldmem, size_t bytes);
    extern size_t mspace_usable_size(void *mem);
    extern void mspace_free(void *msp, void *mem);
    extern size_t mspace_used(void *msp);
    extern void *mspace_top_address(void *msp);
}

namespace cmod
{
struct Meta
{
    char user_padding[META_USER_PADDING_SIZE];
    char root_object[MAX_ROOT_STRUCT_SIZE];
    char naming_table[MAX_NAMING_TABLE_SIZE];
    size_t size;
    size_t mspace_offset;
    Meta()
        : size(0), mspace_offset(1)
    {
    }
};
struct MemorySpaceInfo
{
    boost::interprocess::offset_ptr<void> space;
    MemorySpaceInfo()
    {
    }
};

struct UserPadding
{
    // In the LinuxThreads implementation, no resources are associated with mutex objects,
    // thus pthread_mutex_destroy actually does nothing except checking that the mutex is unlocked.
    pthread_mutex_t l;
    bool use_locks;

    UserPadding() : use_locks(false)
    {
    }

    void lock()
    {
        if (!use_locks)
        {
            return;
        }

        pthread_mutex_lock(&l);
    }

    void unlock()
    {
        if (!use_locks)
        {
            return;
        }

        pthread_mutex_unlock(&l);
    }
};

template <typename T>
class Allocator
{
private:
    //Self type
    typedef Allocator<T> self_t;

    //typedef boost::interprocess::offset_ptr<void> VoidPtr;
    //Not assignable from related allocator
    template <class T2>
    Allocator &operator=(const Allocator<T2> &);

    //Not assignable from other allocator

    //Pointer to the allocator
    MemorySpaceInfo m_space;

public:
    typedef T value_type;
    //typedef VoidPtr pointer;
    //typedef const VoidPtr const_pointer;
    //typedef T* pointer;
    //typedef const T* const_pointer;
    typedef boost::interprocess::offset_ptr<T> pointer;
    typedef const pointer const_pointer;
    typedef T &reference;
    typedef const T &const_reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

    //!Obtains an allocator that allocates
    //!objects of type T2
    template <class T2>
    struct rebind
    {
        typedef Allocator<T2> other;
    };
    Allocator()
    {
    }
    //!Constructor from the segment manager.
    //!Never throws
    Allocator(const MemorySpaceInfo &space)
        : m_space(space)
    {
    }

    //!Constructor from other allocator.
    //!Never throws
    Allocator(const Allocator &other)
    {
        if (other.m_space.space.get() != NULL)
        {
            m_space = other.m_space;
        }
    }

    //!Constructor from related allocator.
    //!Never throws
    template <class T2>
    Allocator(const Allocator<T2> &other)
    {
        if (other.get_space().space.get() != NULL)
        {
            m_space = other.get_space();
        }
    }
    Allocator &operator=(const Allocator &other)
    {
        if (other.m_space.space.get() != NULL)
        {
            m_space = other.m_space;
        }
        return *this;
    }

    //!Allocates memory for an array of count elements.
    //!Throws boost::interprocess::bad_alloc if there is no enough memory
    T *allocate(size_type count, const_pointer hint = 0)
    {
        (void)hint;
        void *p = NULL;
        Meta *meta = (Meta *)(m_space.space.get());
        if (NULL == meta)
        {
            return (T *)malloc(count * sizeof(T));
        }

        UserPadding *user_padding = (UserPadding *)meta->user_padding;
        std::lock_guard<UserPadding> guard(*user_padding);
        p = mspace_malloc((char *)(meta) + meta->mspace_offset, count * sizeof(T));
        //allocate from other space
        if (NULL == p)
        {
            throw std::bad_alloc();
        }
        //printf("###malloc at %d\n", (char*)(p +  count * sizeof(T)) - (char*)meta);
        return (T *)p;
    }

    void *realloc(void *oldmem, size_t bytes)
    {
        void *p = NULL;
        Meta *meta = (Meta *)(m_space.space.get());
        if (NULL == meta)
        {
            return realloc(oldmem, bytes);
        }

        UserPadding *user_padding = (UserPadding *)meta->user_padding;
        std::lock_guard<UserPadding> guard(*user_padding);
        p = mspace_realloc((char *)(meta) + meta->mspace_offset, oldmem, bytes);
        if (NULL == p)
        {
            throw std::bad_alloc();
        }
        return p;
    }

    //!Deallocates memory previously allocated.
    //!Never throws

    inline void deallocate_ptr(const T *ptr, size_type n = 1)
    {
        if (NULL == ptr)
            return;
        T *p = (T *)ptr;
        Meta *meta = (Meta *)(m_space.space.get());
        if (NULL == meta)
        {
            free(p);
            return;
        }

        UserPadding *user_padding = (UserPadding *)meta->user_padding;
        std::lock_guard<UserPadding> guard(*user_padding);
        mspace_free((char *)(meta) + meta->mspace_offset, p);
    }
    inline void deallocate(const pointer &ptr, size_type n = 1)
    {
        deallocate_ptr(ptr.get(), n);
    }

    //!Returns the number of elements that could be allocated.
    //!Never throws
    size_type max_size() const
    {
        Meta *meta = (Meta *)(m_space.space.get());
        if (NULL == meta)
        {
            return UINT32_MAX;
        }
        return meta->size / sizeof(T);
    }

    //!Swap segment manager. Does not throw. If each allocator is placed in
    //!different memory segments, the result is undefined.
    friend void swap(self_t &alloc1, self_t &alloc2)
    {
        //ipcdetail::do_swap(alloc1.mp_mngr, alloc2.mp_mngr);
    }

    //!Returns maximum the number of objects the previously allocated memory
    //!pointed by p can hold. This size only works for memory allocated with
    //!allocate, allocation_command and allocate_many.
    size_type size(const pointer &p) const
    {
        Meta *meta = (Meta *)(m_space.space.get());
        if (NULL == meta)
        {
            return UINT32_MAX;
        }
        return (size_type)mspace_usable_size(p.get()) / sizeof(T);
    }

    //!Returns address of mutable object.
    //!Never throws
    pointer address(reference value) const
    {
        return &value;
    }

    //!Returns address of non mutable object.
    //!Never throws
    const_pointer address(const_reference value) const
    {
        return &value;
    }

    //!Constructs an object
    //!Throws if T's constructor throws
    //!For backwards compatibility with libraries using C++03 allocators
    //            template<class P>
    //            void construct(const pointer &ptr, BOOST_FWD_REF(P) p)
    //            {
    //                ::new ((void*) (ptr.get())) T(::boost::forward<P>(p));
    //            }

    void construct(const pointer &ptr, const_reference val)
    {
        ::new ((void *)(ptr.get())) T(val);
    }

    //!Destroys object. Throws if object's
    //!destructor throws
    void destroy(const pointer &ptr)
    {
        //(*ptr).~value_type();
        T *p = ptr.get();
        (*p).~value_type();
    }

    MemorySpaceInfo get_space() const
    {
        return m_space;
    }

    void *get_mspace() const
    {
        Meta *meta = (Meta *)(m_space.space.get());
        return (char *)(meta) + meta->mspace_offset;
    }

    size_t used_space() const
    {
        Meta *meta = (Meta *)(m_space.space.get());
        void *top = mspace_top_address(get_mspace());
        return (char *)top - (char *)meta;
    }
};

//!Equality test for same type
//!of allocator
template <class T>
inline bool operator==(const Allocator<T> &alloc1, const Allocator<T> &alloc2)
{
    return alloc1.get_space().space == alloc2.get_space().space;
}

//!Inequality test for same type
//!of allocator
template <class T>
inline bool operator!=(const Allocator<T> &alloc1, const Allocator<T> &alloc2)
{
    return alloc1.get_space().space != alloc2.get_space().space;
}
template <class T1, class T2>
inline bool operator!=(const Allocator<T1> &alloc1, const Allocator<T2> &alloc2)
{
    return alloc1.get_space().space != alloc2.get_space().space;
}
} // namespace cmod

#endif /* PTMALLOC_ALLOCATOR_HPP_ */
