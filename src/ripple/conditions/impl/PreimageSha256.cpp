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

#include <ripple/basics/Buffer.h>
#include <ripple/basics/Slice.h>
#include <ripple/conditions/Condition.h>
#include <ripple/conditions/Fulfillment.h>
#include <ripple/conditions/impl/Der.h>
#include <ripple/conditions/impl/PreimageSha256.h>
#include <ripple/conditions/impl/error.h>
#include <ripple/protocol/digest.h>

namespace ripple {
namespace cryptoconditions {

Buffer
PreimageSha256::fingerprint(std::error_code& ec) const
{
    sha256_hasher h;
    h(payload_.data(), payload_.size());
    auto const d = static_cast<sha256_hasher::result_type>(h);
    return {d.data(), d.size()};
}

void
PreimageSha256::encodeFingerprint(der::Encoder& encoder) const
{
    // PrefixSha256's fingerprint is not der encoded
    assert(0);
}

std::uint32_t
PreimageSha256::cost() const
{
    return static_cast<std::uint32_t>(payload_.size());
}

std::bitset<5>
PreimageSha256::subtypes() const
{
    return std::bitset<5>{};
}

bool PreimageSha256::validate(Slice) const
{
    // the message is not relevant.
    return true;
}

std::uint64_t
PreimageSha256::derEncodedLength(
    boost::optional<der::GroupType> const& parentGroupType,
    der::TagMode encoderTagMode) const
{
    return cryptoconditions::der::withTupleEncodedLengthHelper(
        *this, parentGroupType, encoderTagMode);
}

void
PreimageSha256::encode(cryptoconditions::der::Encoder& encoder) const
{
    cryptoconditions::der::withTupleEncodeHelper(*this, encoder);
}

void
PreimageSha256::decode(der::Decoder& decoder)
{
    if (decoder.parentSlice().size() > maxPreimageLength)
    {
        decoder.ec_ = error::preimage_too_long;
        return;
    }

    cryptoconditions::der::withTupleDecodeHelper(*this, decoder);
}


bool
PreimageSha256::checkEqual(Fulfillment const& rhs) const
{
    if (auto c = dynamic_cast<PreimageSha256 const*>(&rhs))
        return c->payload_ == payload_;
    return false;

}

bool
PreimageSha256::validationDependsOnMessage() const
{
    return false;
}

}
}
