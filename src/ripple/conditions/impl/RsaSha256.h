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

#ifndef RIPPLE_CONDITIONS_RSA_SHA256_H
#define RIPPLE_CONDITIONS_RSA_SHA256_H

#include <ripple/basics/Buffer.h>
#include <ripple/conditions/Condition.h>
#include <ripple/conditions/Fulfillment.h>
#include <ripple/conditions/impl/Der.h>

namespace ripple {
namespace cryptoconditions {

/** Fulfillment for a RsaSha256 cryptocondition.

    A RsaSha256 condition specifies an RsaSha256 public key (the modulus). The
    fulfillment contains a signature of cryptocondition message.
*/
class RsaSha256 final : public Fulfillment
{
    Buffer modulus_;
    Buffer signature_;

    void
    encodeFingerprint(der::Encoder& encoder) const override;

    bool
    checkEqual(Fulfillment const& rhs) const override;

    bool
    validationDependsOnMessage() const override;

public:
    RsaSha256(der::Constructor const&) noexcept {};

    RsaSha256() = delete;

    RsaSha256(Buffer m, Buffer s);
    RsaSha256(Slice m, Slice s);

    template<class F>
    void withTuple(F&& f)
    {
        f(std::tie(modulus_, signature_));
    }

    template<class F>
    void withTuple(F&& f) const
    {
        const_cast<RsaSha256*>(this)->withTuple(std::forward<F>(f));
    }

    Type
    type() const override
    {
        return Type::rsaSha256;
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

    std::uint64_t
    derEncodedLength(
        boost::optional<der::GroupType> const& parentGroupType,
        der::TagMode encoderTagMode) const override;
};

}
}

#endif
