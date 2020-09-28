//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2020 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef RIPPLE_BASICS_INSTRUMENTEDALLOCATOR_H_INCLUDED
#define RIPPLE_BASICS_INSTRUMENTEDALLOCATOR_H_INCLUDED

#include <ripple/basics/CountedObject.h>
#include <ripple/beast/cxx17/pmr.h>

namespace ripple {

class InstrumentedAllocator : public pmr_memory_resource
{
private:
    pmr_memory_resource* upstream_;
    CountedObjects::CounterBase* counter_;

public:
    explicit InstrumentedAllocator(
        CountedObjects::CounterBase* counter,
        pmr_memory_resource* us = pmr_get_default_resource())
        : upstream_{us}, counter_(counter)
    {
    }

    // can't be a constructor because of the template
    template <class T>
    static InstrumentedAllocator
    make_InstrumentedAllocator(
        pmr_memory_resource* us = pmr_get_default_resource())
    {
        return InstrumentedAllocator(&CountedObject<T>::getCounter(), us);
    }

private:
    void*
    do_allocate(size_t bytes, size_t alignment) override
    {
        counter_->updateSizeDeltaBytes(bytes);
        void* ret = upstream_->allocate(bytes, alignment);
        return ret;
    }
    void
    do_deallocate(void* ptr, size_t bytes, size_t alignment) override
    {
        counter_->updateSizeDeltaBytes(-bytes);
        upstream_->deallocate(ptr, bytes, alignment);
    }
    bool
    do_is_equal(pmr_memory_resource const& other) const noexcept override
    {
        if (this == &other)
            return true;
        auto op = dynamic_cast<const InstrumentedAllocator*>(&other);
        return op != nullptr && op->counter_ == counter_ &&
            upstream_->is_equal(other);
    }
};
}  // namespace ripple

#endif
