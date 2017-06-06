//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2016 Ripple Labs Inc.

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

#ifndef RIPPLE_CONDITIONS_PREIMAGE_SHA256_H
#define RIPPLE_CONDITIONS_PREIMAGE_SHA256_H

#include <ripple/basics/Buffer.h>
#include <ripple/basics/Slice.h>
#include <ripple/conditions/Condition.h>
#include <ripple/conditions/Fulfillment.h>
#include <ripple/conditions/impl/Der.h>

namespace ripple {
namespace cryptoconditions {

/** Fullfillment for a preimage cryptocondition

    A preimage has a condition that is a sha256 hash and a fulfillment with a
    payload that will hash to the specified hash in the condition.

    @note a preimage does not depend on the cryptocondition message.
 */
class PreimageSha256 final : public Fulfillment
{
public:
    /** The maximum allowed length of a preimage.

        The specification does not specify a minimum supported
        length, nor does it require all conditions to support
        the same minimum length.

        While future versions of this code will never lower
        this limit, they may opt to raise it.
    */
    static constexpr std::size_t maxPreimageLength = 128;

private:
    Buffer payload_;

    void
    encodeFingerprint(der::Encoder& encoder) const override;

    bool
    checkEqual(Fulfillment const& rhs) const override;

    bool
    validationDependsOnMessage() const override;
public:
    PreimageSha256(der::Constructor const&) noexcept {};

    PreimageSha256(Buffer&& b) noexcept : payload_(std::move(b))
    {
    }

    PreimageSha256(Slice s) noexcept : payload_(s)
    {
    }

    template<class F>
    void withTuple(F&& f)
    {
        f(std::tie(payload_));
    }

    template<class F>
    void withTuple(F&& f) const
    {
        const_cast<PreimageSha256*>(this)->withTuple(std::forward<F>(f));
    }

    Type
    type() const override
    {
        return Type::preimageSha256;
    }

    Buffer
    fingerprint(std::error_code& ec) const override;

    std::uint32_t
    cost() const override;

    std::bitset<5>
    subtypes() const override;

    bool validate(Slice) const override;

    void
    encode(der::Encoder& encoder) const override;

    void
    decode(der::Decoder& decoder) override;

    std::uint64_t
    derEncodedLength() const override;
};

}
}

#endif
