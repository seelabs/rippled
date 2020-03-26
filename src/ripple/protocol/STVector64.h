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

#ifndef RIPPLE_PROTOCOL_STVECTOR64_H_INCLUDED
#define RIPPLE_PROTOCOL_STVECTOR64_H_INCLUDED

#include <ripple/protocol/STBase.h>
#include <ripple/protocol/STBitString.h>
#include <ripple/protocol/STInteger.h>

namespace ripple {

class STVector64 : public STBase
{
public:
    using value_type = std::vector<std::uint64_t> const&;

    STVector64() = default;

    explicit STVector64(SField const& n) : STBase(n)
    {
    }

    explicit STVector64(std::vector<std::uint64_t> const& vector)
        : mValue(vector)
    {
    }

    STVector64(SField const& n, std::vector<std::uint64_t> const& vector)
        : STBase(n), mValue(vector)
    {
    }

    STVector64(SerialIter& sit, SField const& name);

    STBase*
    copy(std::size_t n, void* buf) const override
    {
        return emplace(n, buf, *this);
    }

    STBase*
    move(std::size_t n, void* buf) override
    {
        return emplace(n, buf, std::move(*this));
    }

    SerializedTypeID
    getSType() const override
    {
        return STI_VECTOR64;
    }

    void
    add(Serializer& s) const override;

    Json::Value getJson(JsonOptions) const override;

    bool
    isEquivalent(const STBase& t) const override;

    bool
    isDefault() const override
    {
        return mValue.empty();
    }

    STVector64&
    operator=(std::vector<std::uint64_t> const& v)
    {
        mValue = v;
        return *this;
    }

    STVector64&
    operator=(std::vector<std::uint64_t>&& v)
    {
        mValue = std::move(v);
        return *this;
    }

    void
    setValue(const STVector64& v)
    {
        mValue = v.mValue;
    }

    /** Retrieve a copy of the vector we contain */
    explicit operator std::vector<std::uint64_t>() const
    {
        return mValue;
    }

    std::size_t
    size() const
    {
        return mValue.size();
    }

    void
    resize(std::size_t n)
    {
        return mValue.resize(n);
    }

    bool
    empty() const
    {
        return mValue.empty();
    }

    std::vector<std::uint64_t>::reference
    operator[](std::vector<std::uint64_t>::size_type n)
    {
        return mValue[n];
    }

    std::vector<std::uint64_t>::const_reference
    operator[](std::vector<std::uint64_t>::size_type n) const
    {
        return mValue[n];
    }

    std::vector<std::uint64_t> const&
    value() const
    {
        return mValue;
    }

    std::vector<std::uint64_t>::iterator
    insert(
        std::vector<std::uint64_t>::const_iterator pos,
        std::uint64_t const& value)
    {
        return mValue.insert(pos, value);
    }

    std::vector<std::uint64_t>::iterator
    insert(
        std::vector<std::uint64_t>::const_iterator pos,
        std::uint64_t&& value)
    {
        return mValue.insert(pos, std::move(value));
    }

    void
    push_back(std::uint64_t const& v)
    {
        mValue.push_back(v);
    }

    std::vector<std::uint64_t>::iterator
    begin()
    {
        return mValue.begin();
    }

    std::vector<std::uint64_t>::const_iterator
    begin() const
    {
        return mValue.begin();
    }

    std::vector<std::uint64_t>::iterator
    end()
    {
        return mValue.end();
    }

    std::vector<std::uint64_t>::const_iterator
    end() const
    {
        return mValue.end();
    }

    std::vector<std::uint64_t>::iterator
    erase(std::vector<std::uint64_t>::iterator position)
    {
        return mValue.erase(position);
    }

    void
    clear() noexcept
    {
        return mValue.clear();
    }

private:
    std::vector<std::uint64_t> mValue;
};

}  // namespace ripple

#endif
