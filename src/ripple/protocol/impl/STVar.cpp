//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

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

#include <ripple/basics/contract.h>
#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/STAmount.h>
#include <ripple/protocol/STArray.h>
#include <ripple/protocol/STBase.h>
#include <ripple/protocol/STBitString.h>
#include <ripple/protocol/STBlob.h>
#include <ripple/protocol/STInteger.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/STPathSet.h>
#include <ripple/protocol/STVector256.h>
#include <ripple/protocol/impl/STVar.h>

namespace ripple {
namespace detail {

defaultObject_t defaultObject;
nonPresentObject_t nonPresentObject;

//------------------------------------------------------------------------------

STVar::STVar(allocator_type allocator) : mem_resource_{allocator.resource()}
{
}

STVar::~STVar()
{
    destroy();
}

STVar::STVar(STVar const& other, allocator_type allocator)
    : mem_resource_{allocator.resource()}
{
    if (other.p_ != nullptr)
        p_ = other.p_->copy(max_size, &d_, allocator);
}

STVar::STVar(STVar&& other) : mem_resource_{other.mem_resource_}
{
    if (other.on_heap())
    {
        p_ = other.p_;
        other.p_ = nullptr;
    }
    else
    {
        p_ = other.p_->move(max_size, &d_, mem_resource_);
    }
}

STVar::STVar(STVar&& other, allocator_type allocator)
    : mem_resource_{allocator.resource()}
{
    if (other.on_heap())
    {
        p_ = other.p_;
        other.p_ = nullptr;
    }
    else
    {
        p_ = other.p_->move(max_size, &d_, mem_resource_);
    }
}

STVar&
STVar::operator=(STVar const& rhs)
{
    if (&rhs != this)
    {
        destroy();
        if (rhs.p_)
            p_ = rhs.p_->copy(max_size, &d_, mem_resource_);
        else
            p_ = nullptr;
    }

    return *this;
}

STVar&
STVar::operator=(STVar&& rhs)
{
    if (&rhs != this)
    {
        destroy();
        if (rhs.on_heap())
        {
            p_ = rhs.p_;
            rhs.p_ = nullptr;
        }
        else
        {
            // do _not_ use rhs's mem_resource_
            p_ = rhs.p_->move(max_size, &d_, mem_resource_);
        }
    }

    return *this;
}

STVar::STVar(defaultObject_t, SField const& name, allocator_type allocator)
    : STVar(name.fieldType, name, allocator)
{
}

STVar::STVar(nonPresentObject_t, SField const& name, allocator_type allocator)
    : STVar(STI_NOTPRESENT, name, allocator)
{
}

STVar::STVar(
    SerialIter& sit,
    SField const& name,
    int depth,
    allocator_type allocator)
    : mem_resource_{allocator.resource()}
{
    if (depth > 10)
        Throw<std::runtime_error>("Maximum nesting depth of STVar exceeded");
    switch (name.fieldType)
    {
        case STI_NOTPRESENT:
            construct<STBase>(allocator, name);
            return;
        case STI_UINT8:
            construct<STUInt8>(allocator, sit, name);
            return;
        case STI_UINT16:
            construct<STUInt16>(allocator, sit, name);
            return;
        case STI_UINT32:
            construct<STUInt32>(allocator, sit, name);
            return;
        case STI_UINT64:
            construct<STUInt64>(allocator, sit, name);
            return;
        case STI_AMOUNT:
            construct<STAmount>(allocator, sit, name);
            return;
        case STI_HASH128:
            construct<STHash128>(allocator, sit, name);
            return;
        case STI_HASH160:
            construct<STHash160>(allocator, sit, name);
            return;
        case STI_HASH256:
            construct<STHash256>(allocator, sit, name);
            return;
        case STI_VECTOR256:
            construct<STVector256>(allocator, sit, name);
            return;
        case STI_VL:
            construct<STBlob>(allocator, sit, name);
            return;
        case STI_ACCOUNT:
            construct<STAccount>(allocator, sit, name);
            return;
        case STI_PATHSET:
            construct<STPathSet>(allocator, sit, name);
            return;
        case STI_OBJECT:
            construct<STObject>(allocator, sit, name, depth);
            return;
        case STI_ARRAY:
            construct<STArray>(allocator, sit, name, depth);
            return;
        default:
            Throw<std::runtime_error>("Unknown object type");
    }
}

STVar::STVar(SerializedTypeID id, SField const& name, allocator_type allocator)
    : mem_resource_{allocator.resource()}
{
    assert((id == STI_NOTPRESENT) || (id == name.fieldType));
    switch (id)
    {
        case STI_NOTPRESENT:
            construct<STBase>(allocator, name);
            return;
        case STI_UINT8:
            construct<STUInt8>(allocator, name);
            return;
        case STI_UINT16:
            construct<STUInt16>(allocator, name);
            return;
        case STI_UINT32:
            construct<STUInt32>(allocator, name);
            return;
        case STI_UINT64:
            construct<STUInt64>(allocator, name);
            return;
        case STI_AMOUNT:
            construct<STAmount>(allocator, name);
            return;
        case STI_HASH128:
            construct<STHash128>(allocator, name);
            return;
        case STI_HASH160:
            construct<STHash160>(allocator, name);
            return;
        case STI_HASH256:
            construct<STHash256>(allocator, name);
            return;
        case STI_VECTOR256:
            construct<STVector256>(allocator, name);
            return;
        case STI_VL:
            construct<STBlob>(allocator, name);
            return;
        case STI_ACCOUNT:
            construct<STAccount>(allocator, name);
            return;
        case STI_PATHSET:
            construct<STPathSet>(allocator, name);
            return;
        case STI_OBJECT:
            construct<STObject>(allocator, name);
            return;
        case STI_ARRAY:
            construct<STArray>(allocator, name);
            return;
        default:
            Throw<std::runtime_error>("Unknown object type");
    }
}

void
STVar::destroy()
{
    if (on_heap())
        p_->destroy();
    else
        p_->~STBase();

    p_ = nullptr;
}

}  // namespace detail
}  // namespace ripple
