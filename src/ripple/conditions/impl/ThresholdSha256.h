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

#ifndef RIPPLE_CONDITIONS_THRESHOLD_SHA256_H
#define RIPPLE_CONDITIONS_THRESHOLD_SHA256_H

#include <ripple/basics/Buffer.h>
#include <ripple/conditions/Condition.h>
#include <ripple/conditions/Fulfillment.h>
#include <ripple/conditions/impl/Der.h>

#include <cstdint>

namespace ripple {
namespace cryptoconditions {

/** Fulfillment for an m-of-n collection of fulfillments

    The fulfillment contains a collection of subfulfillments. This is the
    threshold (the m in the m-of-n). It also contains a collection of
    subconditions. These are the additional conditions that will not be
    verified (but of course, are part of the condition).

    @note The number of sub-fulfillments is the m in the m-of-n. The number of
    sub-fulfillments plus the number of sub-conditions is the n in the m-of-n.
 */
class ThresholdSha256 final : public Fulfillment
{
    /** Subfulfillments to be verified. The number of subfulfillments is the
        threshold (the m in the m-of-n).
     */
    std::vector<std::unique_ptr<Fulfillment>> subfulfillments_;
    /** Subconditions that will not be verified (but are part of this object's
        condition).
     */
    std::vector<Condition> subconditions_;

    void
    encodeFingerprint(der::Encoder& encoder) const override;

    bool
    checkEqual(Fulfillment const& rhs) const override;

    bool
    validationDependsOnMessage() const override;
public:
    ThresholdSha256(der::Constructor const&) noexcept {};

    ThresholdSha256() = delete;
    ThresholdSha256(
        std::vector<std::unique_ptr<Fulfillment>> subfulfillments,
        std::vector<Condition> subconditions);

    template<class F>
    void withTuple(F&& f)
    {
        auto fulfillmentsSet = der::make_set(subfulfillments_);
        auto conditionsSet = der::make_set(subconditions_);
        f(std::tie(fulfillmentsSet, conditionsSet));
    }

    template<class F>
    void withTuple(F&& f) const
    {
        const_cast<ThresholdSha256*>(this)->withTuple(std::forward<F>(f));
    }

    Type
    type() const override
    {
        return Type::thresholdSha256;
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

    int
    compare(Fulfillment const& rhs) const override;
};

}
}

#endif
