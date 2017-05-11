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

#include <ripple/conditions/impl/Der.h>
#include <ripple/conditions/impl/ThresholdSha256.h>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace ripple {
namespace cryptoconditions {

ThresholdSha256::ThresholdSha256(
    std::vector<std::unique_ptr<Fulfillment>> subfulfillments,
    std::vector<Condition> subconditions)
    : subfulfillments_(std::move(subfulfillments))
    , subconditions_(std::move(subconditions))
{
}

Buffer
ThresholdSha256::fingerprint() const
{
    return Fulfillment::fingerprint();
}

void
ThresholdSha256::encodeFingerprint(der::Encoder& encoder) const
{
    std::uint16_t const threshold = static_cast<std::uint16_t>(subfulfillments_.size());
    // swd TBD - this is very inefficient
    std::vector<Condition> allConditions(subconditions_);
    for(auto const& f : subfulfillments_)
        allConditions.push_back(f->condition());
    auto conditionsSet = der::make_set(allConditions);
    encoder << std::tie(threshold, conditionsSet);
}

bool
ThresholdSha256::validate(Slice data) const
{
    for (auto const& f : subfulfillments_)
        if (!f || !f->validate(data))
            return false;
    return true;
}

std::uint32_t
ThresholdSha256::cost() const
{
    std::vector<std::uint32_t> subcosts;
    subcosts.reserve(subconditions_.size() + subfulfillments_.size());
    for (auto const& c : subconditions_)
        subcosts.push_back(c.cost);
    for (auto const& f : subfulfillments_)
        subcosts.push_back(f->cost());
    size_t const threshold = subfulfillments_.size();
    std::nth_element(
        subcosts.begin(), subcosts.end() - threshold, subcosts.end());
    std::uint64_t const result =
        std::accumulate(subcosts.end() - threshold, subcosts.end(), 0ull) +
        1024 * (subfulfillments_.size() + subconditions_.size());
    if (result > std::numeric_limits<std::uint32_t>::max())
        return std::numeric_limits<std::uint32_t>::max();
    return static_cast<uint32_t>(result);
}

std::bitset<5>
ThresholdSha256::subtypes() const
{
    std::bitset<5> result;
    for (auto const& s : subconditions_)
        result |= s.selfAndSubtypes();
    for (auto const& s : subfulfillments_)
        result |= s->selfAndSubtypes();
    result[static_cast<int>(type())] = 0;
    return result;
}

void
ThresholdSha256::encode(der::Encoder& encoder) const
{
    const_cast<ThresholdSha256*>(this)->serialize(encoder);
}

void
ThresholdSha256::decode(der::Decoder& decoder)
{
    serialize(decoder);
}

bool
ThresholdSha256::checkEqual(Fulfillment const& rhs) const
{
    auto c = dynamic_cast<ThresholdSha256 const*>(&rhs);
    if (!c)
        return false;
    if (c->subfulfillments_.size() != subfulfillments_.size() ||
        c->subconditions_.size() != subconditions_.size())
        return false;

    if (subfulfillments_.size() > 64 || subconditions_.size() > 64)
    {
        // swd TBD
        assert(0);
    }

    {
        std::uint64_t foundEqual = 0;
        for (size_t i = 0; i < subfulfillments_.size(); ++i)
        {
            for (size_t j = 0; j < subfulfillments_.size(); ++j)
            {
                if (c->subfulfillments_[i]->checkEqual(*subfulfillments_[j]))
                {
                    std::uint64_t mask = 1 << j;
                    if (foundEqual & mask)
                        continue;

                    foundEqual |= 1 << j;
                    break;
                }
            }
        }
        if (foundEqual != ((1 << subfulfillments_.size()) - 1))
            return false;
    }

    {
        std::uint64_t foundEqual = 0;
        for (size_t i = 0; i < subconditions_.size(); ++i)
        {
            for (size_t j = 0; j < subconditions_.size(); ++j)
            {
                if (c->subconditions_[i] == subconditions_[j])
                {
                    std::uint64_t mask = 1 << j;
                    if (foundEqual & mask)
                        continue;

                    foundEqual |= 1 << j;
                    break;
                }
            }
        }
        if (foundEqual != ((1 << subconditions_.size()) - 1))
            return false;
    }

    return true;
}


bool
ThresholdSha256::validationDependsOnMessage() const
{
    for(auto const& f : subfulfillments_)
        if (f->validationDependsOnMessage())
            return true;

    return false;
}

}
}
