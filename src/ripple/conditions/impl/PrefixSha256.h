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

#ifndef RIPPLE_CONDITIONS_PREFIX_SHA256_H
#define RIPPLE_CONDITIONS_PREFIX_SHA256_H

#include <ripple/basics/Buffer.h>
#include <ripple/conditions/Condition.h>
#include <ripple/conditions/Fulfillment.h>
#include <ripple/conditions/impl/Der.h>

#include <memory>

namespace ripple {
namespace cryptoconditions {

class PrefixSha256 final : public Fulfillment
{
    Buffer prefix_;
    std::uint64_t maxMessageLength_ = 0;
    std::unique_ptr<Fulfillment> subfulfillment_;

    template <class Coder>
    void
    serialize(Coder& c)
    {
        c & std::tie(prefix_, maxMessageLength_, subfulfillment_);
    }

    void
    encodeFingerprint(der::Encoder& encoder) const override;

    bool
    checkEqual(Fulfillment const& rhs) const override;

    bool
    validationDependsOnMessage() const override;

public:
    PrefixSha256(der::Constructor const&) noexcept {};

    PrefixSha256() = delete;

    PrefixSha256(
        Slice prefix,
        std::uint64_t maxLength,
        std::unique_ptr<Fulfillment> subfulfillment);

    Type
    type() const override
    {
        return Type::prefixSha256;
    }

    Buffer
    fingerprint(std::error_code& ec) const override;

    bool
    validate(Slice data) const override;

    std::uint32_t
    cost() const override;

    std::bitset<5>
    subtypes() const override;

    void
    encode(der::Encoder& encoder) const override;

    void
    decode(der::Decoder& decoder) override;
};
}
}

#endif
