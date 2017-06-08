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

#include <ripple/basics/contract.h>
#include <ripple/conditions/Condition.h>
#include <ripple/conditions/Fulfillment.h>
#include <ripple/conditions/impl/Der.h>
#include <ripple/conditions/impl/PreimageSha256.h>
#include <ripple/conditions/impl/utils.h>
#include <boost/regex.hpp>
#include <boost/optional.hpp>
#include <bitset>
#include <vector>
#include <iostream>

namespace ripple {
namespace cryptoconditions {

bool
Condition::
isCompoundCondition(Type t)
{
    static_assert(Type::last == Type::ed25519Sha256, "Add new case");
    switch (t)
    {
        case Type::preimageSha256:
        case Type::rsaSha256:
        case Type::ed25519Sha256:
            return false;
        case Type::prefixSha256:
        case Type::thresholdSha256:
            return true;
    }
    assert(0);
    return false;  // silence compiler warning
}

namespace der {
void
DerCoderTraits<Condition>::
encode(
    Encoder& encoder,
    Condition const& c)
{
    cryptoconditions::der::withTupleEncodeHelper(c, encoder);
}

void
DerCoderTraits<Condition>::
decode(
    Decoder& decoder,
    Condition& v)
{
    if (decoder.parentSlice().size() > Condition::maxSerializedCondition)
    {
        decoder.ec_ = error::large_size;
        return;
    }

    auto const parentTag = decoder.parentTag();
    if (!parentTag)
    {
        decoder.ec_ = make_error_code(Error::logicError);
        return;
    }

    if (parentTag->classId != classId())
    {
        decoder.ec_ = make_error_code(Error::preambleMismatch);
        return;
    }

    if (parentTag->tagNum > static_cast<std::uint64_t>(Type::last))
    {
        decoder.ec_ = make_error_code(Error::preambleMismatch);
        return;
    }

    v.type = static_cast<Type>(parentTag->tagNum);
    cryptoconditions::der::withTupleDecodeHelper(v, decoder);

    if (decoder.ec_)
        return;

    if (v.type == Type::preimageSha256 &&
        v.cost > PreimageSha256::maxPreimageLength)
    {
        decoder.ec_ = error::preimage_too_long;
    }
}

std::uint64_t
DerCoderTraits<Condition>::
length(
    Condition const& v,
    boost::optional<GroupType> const& parentGroupType,
    TagMode encoderTagMode)
{
    auto const l = cryptoconditions::der::withTupleEncodedLengthHelper(
        v, parentGroupType, encoderTagMode);
    if (encoderTagMode == TagMode::automatic)
        return l;
    return 1 + l + contentLengthLength(l);
}

}  // der

Condition::Condition(Type t, std::uint32_t c, Slice fp, std::bitset<5> const& s)
    : type(t)
    , fingerprint(fp)
    , cost(c)
    , subtypes(s)
{
}

Condition::Condition(Type t, std::uint32_t c, Buffer&& fp, std::bitset<5> const& s)
    : type(t)
    , fingerprint(std::move(fp))
    , cost(c)
    , subtypes(s)
{
}

Condition::Condition(der::Constructor const&){}

std::bitset<5>
Condition::selfAndSubtypes() const
{
    std::bitset<5> result{subtypes};
    result.set(static_cast<std::size_t>(type));
    return result;
}

Condition
Condition::deserialize(Slice s, std::error_code& ec)
{
    // The binary encoding of conditions differs based on their
    // type.  All types define at least a fingerprint and cost
    // sub-field.  Some types, such as the compound condition
    // types, define additional sub-fields that are required to
    // convey essential properties of the cryptocondition (such
    // as the sub-types used by sub-conditions in the case of
    // the compound types).
    //
    // Conditions are encoded as follows:
    //
    //    Condition ::= CHOICE {
    //      preimageSha256   [0] SimpleSha256Condition,
    //      prefixSha256     [1] CompoundSha256Condition,
    //      thresholdSha256  [2] CompoundSha256Condition,
    //      rsaSha256        [3] SimpleSha256Condition,
    //      ed25519Sha256    [4] SimpleSha256Condition
    //    }
    //
    //    SimpleSha256Condition ::= SEQUENCE {
    //      fingerprint          OCTET STRING (SIZE(32)),
    //      cost                 INTEGER (0..4294967295)
    //    }
    //
    //    CompoundSha256Condition ::= SEQUENCE {
    //      fingerprint          OCTET STRING (SIZE(32)),
    //      cost                 INTEGER (0..4294967295),
    //      subtypes             ConditionTypes
    //    }
    //
    //    ConditionTypes ::= BIT STRING {
    //      preImageSha256   (0),
    //      prefixSha256     (1),
    //      thresholdSha256  (2),
    //      rsaSha256        (3),
    //      ed25519Sha256    (4)
    //    }

    using namespace der;

    Condition v{der::constructor};

    der::Decoder decoder(s, der::TagMode::automatic);
    decoder >> v >> der::eos;

    ec = decoder.ec_;
    return v;
}
}
}
