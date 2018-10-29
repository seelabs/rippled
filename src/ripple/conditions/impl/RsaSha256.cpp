//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2017 Ripple Labs Inc.

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
// requirements imposed by section 8.4.1 of the RFC (Draft Ver. 4)
bool
checkModulusLength(int len)
{
    if (len <= 0)
        return false;

    return len == boost::algorithm::clamp(len, 128, 512);
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

RsaSha256::RsaSha256(Slice m, Slice s)
{
    modulus_.resize(m.size());
    std::copy(m.data(), m.data() + m.size(), modulus_.data());

    signature_.resize(s.size());
    std::copy(s.data(), s.data() + s.size(), signature_.data());
}

std::array<std::uint8_t, 32>
RsaSha256::fingerprint(std::error_code& ec) const
{
    return Fulfillment::fingerprint(ec);
}

void
RsaSha256::encodeFingerprint(der::Encoder& encoder) const
{
    // modulus must be greater than 128 bytes and less than or equal to 512 bytes
    if (modulus_.size() <= 128 || modulus_.size() > 512)
    {
        encoder.ec_ =
                der::make_error_code(der::Error::rsaModulusSizeRangeError);
        return;
    }
    encoder << std::tie(modulus_);
}

bool
RsaSha256::validate(Slice data) const
{
    if (modulus_.empty() || signature_.empty())
        return false;

    {
        // Verify the signature is numerically less than the modulus
        // As required by 8.4.5 of the RFC (Ver. 4)

        // compare slices
        // return -1 if lhs is numerically less than rhs
        // return 1 if lhs is numerically greater than rhs
        // return 0 if lhs is numerically equal to rhs
        // The size in bytes of lhs must be <= than the size in
        // bytes of rhs. The code will handle leading zero bytes
        // correctly. I.e. if lhs==0x02 and rhs==0x0001, lhs is still
        // greater than rhs even though it represented by fewer bytes
        auto cmpSlices = [](Slice lhs, Slice rhs) -> int {
            assert(lhs.size() <= rhs.size());
            auto const numLeadingRHSBytes = rhs.size() - lhs.size();
            for (size_t i = 0; i < numLeadingRHSBytes; ++i)
            {
                if (rhs[i])
                    return -1;
            }
            for (size_t i = 0, e = lhs.size(); i != e; ++i)
            {
                auto const a = lhs[i];
                auto const b = rhs[i+numLeadingRHSBytes];
                // -1 if a<b; 0 if a==b; 1 if a>b;
                int const cmp = (a>b) - (a<b);
                if (cmp)
                    return cmp;
            }
            return 0; // equal
        };

        if (modulus_.size() > signature_.size())
        {
            if (cmpSlices(makeSlice(signature_), makeSlice(modulus_)) != -1)
                return false;
        }
        else
        {
            if (cmpSlices(makeSlice(modulus_), makeSlice(signature_)) != 1)
                return false;
        }
    }

    detail::RsaKey rsa(RSA_new());

    auto n = BN_new();
    BN_bin2bn(modulus_.data(), modulus_.size(), n);
    auto e = BN_new();
    BN_set_word(e, 65537);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    rsa->n = n;
    rsa->e = e;
#else
    if (!RSA_set0_key(rsa.get(), n, e, nullptr))
        return false;
#endif

    return detail::validateHelper(rsa.get(), data, makeSlice(signature_));
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
    der::TagMode encoderTagMode,
    der::TraitsCache& traitsCache) const
{
    return cryptoconditions::der::withTupleEncodedLengthHelper(
        *this, parentGroupType, encoderTagMode, traitsCache);
}

void
RsaSha256::encode(cryptoconditions::der::Encoder& encoder) const
{
    // modulus must be greater than 128 bytes and less than or equal to 512 bytes
    if (modulus_.size() <= 128 || modulus_.size() > 512)
    {
        encoder.ec_ =
            der::make_error_code(der::Error::rsaModulusSizeRangeError);
        return;
    }
    cryptoconditions::der::withTupleEncodeHelper(*this, encoder);
}

void
RsaSha256::decode(cryptoconditions::der::Decoder& decoder)
{
    cryptoconditions::der::withTupleDecodeHelper(*this, decoder);
    // modulus must be greater than 128 bytes and less than or equal to 512 bytes
    if (modulus_.size() <= 128 || modulus_.size() > 512)
    {
        decoder.ec_ =
            der::make_error_code(der::Error::rsaModulusSizeRangeError);
        return;
    }
}

bool
RsaSha256::checkEqualForTesting(Fulfillment const& rhs) const
{
    if (auto c = dynamic_cast<RsaSha256 const*>(&rhs))
        return c->modulus_ == modulus_ && c->signature_ == signature_;
    return false;

}

int
RsaSha256::compare(Fulfillment const& rhs, der::TraitsCache& traitsCache) const
{
    return cryptoconditions::der::withTupleCompareHelper(*this, rhs, traitsCache);
}


bool
RsaSha256::validationDependsOnMessage() const
{
    return true;
}

}
}
