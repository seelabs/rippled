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

#include <ripple/conditions/impl/RsaSha256.h>
#include <ripple/protocol/digest.h>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

#include <boost/algorithm/clamp.hpp>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>

namespace ripple {
namespace cryptoconditions {

namespace detail {

struct rsa_deleter
{
    void
    operator()(RSA* rsa) const
    {
        RSA_free(rsa);
    }
};

using RsaKey = std::unique_ptr<RSA, rsa_deleter>;

struct bn_deleter
{
    void
    operator()(BIGNUM* bn) const
    {
        BN_free(bn);
    }
};

using BigNum = std::unique_ptr<BIGNUM, bn_deleter>;

// Check whether the public modulus meets the length
// requirements imposed by section 4.4.1 of the RFC.
bool
checkModulusLength(int len)
{
    if (len <= 0)
        return false;

    return len == boost::algorithm::clamp(len, 128, 512);
}

bool
signHelper(RSA* key, Slice message, Buffer& modulus, Buffer& signature)
{
    int const keySize = RSA_size(key);
    if (!checkModulusLength(keySize))
        return false;

    sha256_hasher h;
    h(message.data(), message.size());
    auto digest = static_cast<sha256_hasher::result_type>(h);

    Buffer buf;

    // Pad the result (-1 -> use hash length as salt length)
    if (!RSA_padding_add_PKCS1_PSS(
            key, buf.alloc(keySize), digest.data(), EVP_sha256(), -1))
        return false;

    // Sign - we've manually padded the input already.
    auto ret = RSA_private_encrypt(
        keySize, buf.data(), signature.alloc(buf.size()), key, RSA_NO_PADDING);

    if (ret == -1)
        return false;

    BN_bn2bin(key->n, modulus.alloc(BN_num_bytes(key->n)));
    return true;
}

bool
validateHelper(RSA* key, Slice message, Slice signature)
{
    int const keySize = RSA_size(key);
    if (!checkModulusLength(keySize))
        return false;

    Buffer buf;

    auto ret = RSA_public_decrypt(
        keySize, signature.data(), buf.alloc(keySize), key, RSA_NO_PADDING);

    if (ret == -1)
        return false;

    sha256_hasher h;
    h(message.data(), message.size());
    auto digest = static_cast<sha256_hasher::result_type>(h);

    return RSA_verify_PKCS1_PSS(
               key, digest.data(), EVP_sha256(), buf.data(), -1) == 1;
}
}

RsaSha256::RsaSha256(Buffer m, Buffer s)
    : modulus_(std::move(m)), signature_(std::move(s))
{
}

RsaSha256::RsaSha256(Slice m, Slice s) : modulus_(m), signature_(s)
{
}

Buffer
RsaSha256::fingerprint(std::error_code& ec) const
{
    return Fulfillment::fingerprint(ec);
}

void
RsaSha256::encodeFingerprint(der::Encoder& encoder) const
{
    encoder << std::tie(modulus_);
}

bool
RsaSha256::validate(Slice data) const
{
    if (modulus_.empty() || signature_.empty())
        return false;

    detail::RsaKey rsa(RSA_new());

    rsa->n = BN_new();
    BN_bin2bn(modulus_.data(), modulus_.size(), rsa->n);

    rsa->e = BN_new();
    BN_set_word(rsa->e, 65537);

    return detail::validateHelper(rsa.get(), data, signature_);
}

std::uint32_t
RsaSha256::cost() const
{
    auto const mSize = modulus_.size();
    if (mSize >= 65535)
        // would overflow
        return std::numeric_limits<std::uint32_t>::max();
    return mSize * mSize;
}

std::bitset<5>
RsaSha256::subtypes() const
{
    return std::bitset<5>{};
}

std::uint64_t
RsaSha256::derEncodedLength(
    boost::optional<der::GroupType> const& parentGroupType,
    der::TagMode encoderTagMode) const
{
    return cryptoconditions::der::withTupleEncodedLengthHelper(
        *this, parentGroupType, encoderTagMode);
}

void
RsaSha256::encode(cryptoconditions::der::Encoder& encoder) const
{
    cryptoconditions::der::withTupleEncodeHelper(*this, encoder);
}

void
RsaSha256::decode(cryptoconditions::der::Decoder& decoder)
{
    cryptoconditions::der::withTupleDecodeHelper(*this, decoder);
}

bool
RsaSha256::checkEqual(Fulfillment const& rhs) const
{
    if (auto c = dynamic_cast<RsaSha256 const*>(&rhs))
        return c->modulus_ == modulus_ && c->signature_ == signature_;
    return false;

}

bool
RsaSha256::validationDependsOnMessage() const
{
    return true;
}

}
}
