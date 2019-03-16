//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2018 Ripple Labs Inc.

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

#include <ripple/beast/unit_test.h>
#include <ripple/beast/xor_shift_engine.h>
#include <ripple/protocol/digest.h>
#include <ripple/protocol/tokens.h>
#include <boost/algorithm/clamp.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ripple {

namespace Base58TestDetail {
// old implementation of decoding functions used as reference to confirm the
// new implementation matches the old implementation

static char rippleAlphabet[] =
    "rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz";

static char bitcoinAlphabet[] =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

//------------------------------------------------------------------------------

template <class Hasher>
static typename Hasher::result_type
digest(void const* data, std::size_t size) noexcept
{
    Hasher h;
    h(data, size);
    return static_cast<typename Hasher::result_type>(h);
}

template <
    class Hasher,
    class T,
    std::size_t N,
    class = std::enable_if_t<sizeof(T) == 1>>
static typename Hasher::result_type
digest(std::array<T, N> const& v)
{
    return digest<Hasher>(v.data(), v.size());
}

// Computes a double digest (e.g. digest of the digest)
template <class Hasher, class... Args>
static typename Hasher::result_type
digest2(Args const&... args)
{
    return digest<Hasher>(digest<Hasher>(args...));
}

/*  Calculate a 4-byte checksum of the data

    The checksum is calculated as the first 4 bytes
    of the SHA256 digest of the message. This is added
    to the base58 encoding of identifiers to detect
    user error in data entry.

    @note This checksum algorithm is part of the client API
*/
void
checksum(void* out, void const* message, std::size_t size)
{
    auto const h = digest2<sha256_hasher>(message, size);
    std::memcpy(out, h.data(), 4);
}

//------------------------------------------------------------------------------

// Code from Bitcoin: https://github.com/bitcoin/bitcoin
// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Modified from the original
//
// WARNING Do not call this directly, use
//         encodeBase58Token instead since it
//         calculates the size of buffer needed.
static std::string
encodeBase58(
    void const* message,
    std::size_t size,
    void* temp,
    std::size_t temp_size,
    char const* const alphabet)
{
    auto pbegin = reinterpret_cast<unsigned char const*>(message);
    auto const pend = pbegin + size;

    // Skip & count leading zeroes.
    int zeroes = 0;
    while (pbegin != pend && *pbegin == 0)
    {
        pbegin++;
        zeroes++;
    }

    auto const b58begin = reinterpret_cast<unsigned char*>(temp);
    auto const b58end = b58begin + temp_size;

    std::fill(b58begin, b58end, 0);

    while (pbegin != pend)
    {
        int carry = *pbegin;
        // Apply "b58 = b58 * 256 + ch".
        for (auto iter = b58end; iter != b58begin; --iter)
        {
            carry += 256 * (iter[-1]);
            iter[-1] = carry % 58;
            carry /= 58;
        }
        assert(carry == 0);
        pbegin++;
    }

    // Skip leading zeroes in base58 result.
    auto iter = b58begin;
    while (iter != b58end && *iter == 0)
        ++iter;

    // Translate the result into a string.
    std::string str;
    str.reserve(zeroes + (b58end - iter));
    str.assign(zeroes, alphabet[0]);
    while (iter != b58end)
        str += alphabet[*(iter++)];
    return str;
}

static std::string
encodeToken(
    TokenType type,
    void const* token,
    std::size_t size,
    char const* const alphabet)
{
    // expanded token includes type + 4 byte checksum
    auto const expanded = 1 + size + 4;

    // We need expanded + expanded * (log(256) / log(58)) which is
    // bounded by expanded + expanded * (138 / 100 + 1) which works
    // out to expanded * 3:
    auto const bufsize = expanded * 3;

    boost::container::small_vector<std::uint8_t, 1024> buf(bufsize);

    // Lay the data out as
    //      <type><token><checksum>
    buf[0] = static_cast<std::underlying_type_t<TokenType>>(type);
    if (size)
        std::memcpy(buf.data() + 1, token, size);
    checksum(buf.data() + 1 + size, buf.data(), 1 + size);

    return encodeBase58(
        buf.data(),
        expanded,
        buf.data() + expanded,
        bufsize - expanded,
        alphabet);
}

std::string
base58EncodeToken(TokenType type, void const* token, std::size_t size)
{
    return encodeToken(type, token, size, rippleAlphabet);
}

std::string
base58EncodeTokenBitcoin(TokenType type, void const* token, std::size_t size)
{
    return encodeToken(type, token, size, bitcoinAlphabet);
}

//------------------------------------------------------------------------------

// Code from Bitcoin: https://github.com/bitcoin/bitcoin
// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Modified from the original
template <class InverseArray>
static std::string
decodeBase58(std::string const& s, InverseArray const& inv)
{
    auto psz = s.c_str();
    auto remain = s.size();
    // Skip and count leading zeroes
    int zeroes = 0;
    while (remain > 0 && inv[*psz] == 0)
    {
        ++zeroes;
        ++psz;
        --remain;
    }
    // Allocate enough space in big-endian base256 representation.
    // log(58) / log(256), rounded up.
    std::vector<unsigned char> b256(remain * 733 / 1000 + 1);
    while (remain > 0)
    {
        auto carry = inv[*psz];
        if (carry == -1)
            return {};
        // Apply "b256 = b256 * 58 + carry".
        for (auto iter = b256.rbegin(); iter != b256.rend(); ++iter)
        {
            carry += 58 * *iter;
            *iter = carry % 256;
            carry /= 256;
        }
        assert(carry == 0);
        ++psz;
        --remain;
    }
    // Skip leading zeroes in b256.
    auto iter = std::find_if(
        b256.begin(), b256.end(), [](unsigned char c) { return c != 0; });
    std::string result;
    result.reserve(zeroes + (b256.end() - iter));
    result.assign(zeroes, 0x00);
    while (iter != b256.end())
        result.push_back(*(iter++));
    return result;
}

/*  Base58 decode a Ripple token

    The type and checksum are are checked
    and removed from the returned result.
*/
template <class InverseArray>
static std::string
decodeBase58Token(std::string const& s, TokenType type, InverseArray const& inv)
{
    auto ret = decodeBase58(s, inv);

    // Reject zero length tokens
    if (ret.size() < 6)
        return {};

    // The type must match.
    if (type != static_cast<TokenType>(ret[0]))
        return {};

    // And the checksum must as well.
    std::array<char, 4> guard;
    checksum(guard.data(), ret.data(), ret.size() - guard.size());
    if (!std::equal(guard.rbegin(), guard.rend(), ret.rbegin()))
        return {};

    // Skip the leading type byte and the trailing checksum.
    return ret.substr(1, ret.size() - 1 - guard.size());
}

//------------------------------------------------------------------------------

// Maps characters to their base58 digit
class InverseAlphabet
{
private:
    std::array<int, 256> map_;

public:
    explicit InverseAlphabet(std::string const& digits)
    {
        map_.fill(-1);
        int i = 0;
        for (auto const c : digits)
            map_[static_cast<unsigned char>(c)] = i++;
    }

    int operator[](char c) const
    {
        return map_[static_cast<unsigned char>(c)];
    }
};

static InverseAlphabet rippleInverse(rippleAlphabet);

static InverseAlphabet bitcoinInverse(bitcoinAlphabet);

std::string
decodeBase58Token(std::string const& s, TokenType type)
{
    return decodeBase58Token(s, type, rippleInverse);
}

std::string
decodeBase58TokenBitcoin(std::string const& s, TokenType type)
{
    return decodeBase58Token(s, type, bitcoinInverse);
}
}  // namespace Base58TestDetail

class Base58_test : public beast::unit_test::suite
{
    beast::xor_shift_engine engine_;  // use the default seed for repeatability

    bool
    checkMatch(Slice expected, Slice got, DecodeMetadata const& metadata)
    {
        auto printIt = [&] {
            log << std::hex;
            log << "Exp, Got:\n";
            for (auto c : expected)
                log << std::uint32_t(c);
            log << "\n";
            log << std::uint32_t(metadata.tokenType);
            if (metadata.isRippleLibEncoded())
            {
                for (auto c : metadata.encodingType)
                    log << std::uint32_t(c);
            }
            for (auto c : got)
                log << std::uint32_t(c);
            for (auto c : metadata.checksum)
                log << std::uint32_t(c);
            log << std::dec;
            log << "\n";
        };

        if (expected[0] != metadata.tokenType)
        {
            log << "Token type mismatch\n";
            printIt();
            return false;
        }
        expected += 1;
        if (metadata.isRippleLibEncoded())
        {
            if (expected[0] == std::uint8_t(0xE1) &&
                expected[1] == std::uint8_t(0x4B))
                expected += 2;
            else
            {
                log << "Ripple lib encoded mismatch\n";
                printIt();
                return false;
            }
        }
        if (!std::equal(
                expected.data() + expected.size() - 4,
                expected.data() + expected.size(),
                metadata.checksum.begin(),
                metadata.checksum.end()))
        {
            log << "Checksum mismatch\n";
            printIt();
            return false;
        }
        if (!std::equal(
                expected.data(),
                expected.data() + expected.size() - 4,
                got.data(),
                got.data() + got.size()))
        {
            log << "Data mismatch\n";
            printIt();
            return false;
        }
        return true;
    }

    /** Generate a random string of base58 characters. Do not generate a
     * checksum */
    void
    randomEncodedBase58(MutableSlice result)
    {
        std::uniform_int_distribution<std::uint8_t> d(0, 57);
        std::generate(result.data(), result.data() + result.size(), [&] {
            return Base58TestDetail::rippleAlphabet[d(engine_)];
        });
    }

    /** Generate test data to decode

        @Note The memory of the generated result is managed by this class
     */
    template <std::size_t MaxResultSize>
    class GenerateDecodeData
    {
        beast::xor_shift_engine
            engine_;  // use the default seed for repeatability

        // Buffer to allocate results from
        std::array<std::uint8_t, MaxResultSize> buf_;
        // Size of the result
        std::size_t resultSize_{0};
        // Force a ripple lib prefix (0x010E14B) to the result
        bool forceRippleLibPrefix_{false};
        // Fill the start of the result with fillPrefixSize_ bytes of
        // fillPrefixValue_
        std::size_t fillPrefixSize_{0};
        std::uint8_t fillPrefixValue_{0};
        // Fill the start of the result with the values in this slice (may be
        // empty)
        Slice varPrefix_;

    public:
        std::size_t
        maxResultSize() const
        {
            return buf_.size();
        }

        GenerateDecodeData&
        resultSize(std::size_t v)
        {
            resultSize_ = v;
            return *this;
        }

        GenerateDecodeData&
        forceRippleLibPrefix(bool v)
        {
            forceRippleLibPrefix_ = v;
            return *this;
        }

        GenerateDecodeData&
        fillPrefix(std::size_t fillSize, std::uint8_t fillValue)
        {
            fillPrefixSize_ = fillSize;
            fillPrefixValue_ = fillValue;
            return *this;
        }

        GenerateDecodeData&
        varPrefix(Slice v)
        {
            varPrefix_ = v;
            return *this;
        }

        /** Generate data to decode. Generate a valid checksum for the data */
        MutableSlice
        generate()
        {
            assert(resultSize_ <= buf_.size());
            MutableSlice result(buf_.data(), resultSize_);
            {
                // random fill
                std::uniform_int_distribution<std::uint8_t> d(0, 255);
                std::generate(
                    result.data(), result.data() + result.size(), [&] {
                        return d(engine_);
                    });
            }
            if (fillPrefixSize_ > 0)
                memset(
                    result.data(),
                    fillPrefixValue_,
                    std::min(fillPrefixSize_, result.size()));

            assert(!(forceRippleLibPrefix_ && !varPrefix_.empty()));
            if (forceRippleLibPrefix_ && result.size() > 3)
            {
                result.data()[0] = 0x01;
                result.data()[1] = 0xE1;
                result.data()[2] = 0x4B;
            }
            else if (!varPrefix_.empty())
            {
                memcpy(result.data(), varPrefix_.data(), varPrefix_.size());
            }

            Base58TestDetail::checksum(
                result.data() + result.size() - 4,
                result.data(),
                result.size() - 4);

            return result;
        }
    };

    void
    testRandomEncodeDecode(std::size_t numTestIterations)
    {
        testcase("base58 random encode/decode");
        std::uniform_int_distribution<std::uint8_t> decodeSizeDist(10, 34);
        std::uniform_int_distribution<std::uint8_t> leadingZeroesDist(1, 6);
        std::uniform_real_distribution<float> zeroOneDist(0.0f, 1.0f);
        GenerateDecodeData<MaxDecodedTokenBytes> genDecodeData;
        for (int i = 0; i < numTestIterations; i++)
        {
            // force test case to start with 0x01e14b - the prefix used to
            // distinguish a ripple lib encoded seed
            bool const forceRippleLibPrefix = zeroOneDist(engine_) > 0.90f;
            std::size_t const decodeSize = [&]() -> size_t {
                if (forceRippleLibPrefix)
                {
                    // The correct size for a ripple lib encoded seed is 23
                    // Since the probably of forcing a ripple lib prefix is
                    // small, we usually want to give the correct size (or we
                    // will rarely test the case that succeeds). So give a small
                    // chance of too small, and a small chance of too large.
                    // Usually return the correct size.
                    auto const p = zeroOneDist(engine_);
                    if (p < 0.05)
                        return 22;
                    else if (p > 0.95)
                        return 24;
                    return 23;
                }
                return decodeSizeDist(engine_);
            }();
            // number of forced leading zeros
            std::size_t const leadingZeroes =
                (zeroOneDist(engine_) > 0.75) ? leadingZeroesDist(engine_) : 0;
            assert(decodeSize <= genDecodeData.maxResultSize() - 4);

            MutableSlice decodeSlice =
                genDecodeData.resultSize(decodeSize)
                    .forceRippleLibPrefix(forceRippleLibPrefix)
                    .fillPrefix(leadingZeroes, 0)
                    .generate();

            DecodeMetadata const metadataRef = [&] {
                DecodeMetadata m;
                m.tokenType = decodeSlice[0];
                if (decodeSlice.size() == 23 &&
                    m.tokenType == static_cast<std::uint8_t>(TokenType::None) &&
                    decodeSlice[1] == std::uint8_t(0xE1) &&
                    decodeSlice[2] == std::uint8_t(0x4B))
                {
                    m.encodingType[0] = 0xE1;
                    m.encodingType[1] = 0x4B;
                }
                else
                {
                    m.encodingType[0] = 0;
                    m.encodingType[1] = 0;
                }
                memcpy(
                    m.checksum.data(),
                    decodeSlice.data() + decodeSlice.size() - 4,
                    4);
                return m;
            }();

            auto const decodeAsToken = metadataRef.isRippleLibEncoded()
                ? TokenType::FamilySeed
                : static_cast<TokenType>(decodeSlice[0]);

            // encode with old impl
            std::string encoded = Base58TestDetail::base58EncodeToken(
                static_cast<TokenType>(decodeSlice[0]),
                decodeSlice.data() + 1,
                decodeSlice.size() - 5);  // 1 for token, 4 for checksum
            std::string encodedBitcoin =
                Base58TestDetail::base58EncodeTokenBitcoin(
                    static_cast<TokenType>(decodeSlice[0]),
                    decodeSlice.data() + 1,
                    decodeSlice.size() - 5);  // 1 for token, 4 for checksum
            // decode with new impl
            std::array<std::uint8_t, 2 * MaxDecodedTokenBytes>
                decodeResultBuf;  //*2 to allow oversized tests

            for (auto resultBufSizeDelta : {-5, -1, 0, 1, 5})
            {
                auto resultBuf = [&] {
                    std::size_t s = decodeSize - 5 +
                        resultBufSizeDelta;  // -5 for token and checksum
                    if (metadataRef.isRippleLibEncoded() && s > 2)
                        s -= 2;  // the two ripple lib prefix bytes
                    // aren't decoded into the result
                    return MutableSlice(decodeResultBuf.data(), s);
                }();

                {
                    // Allow resize
                    auto const decodedRaw =
                        decodeBase58Resizable(makeSlice(encoded), resultBuf);
                    if (!metadataRef.isRippleLibEncoded())
                    {
                        bool const expectDecoded = resultBufSizeDelta >= 0;
                        BEAST_EXPECT(expectDecoded == bool(decodedRaw));
                        if (decodedRaw)
                            BEAST_EXPECT(checkMatch(
                                decodeSlice,
                                decodedRaw->first,
                                decodedRaw->second));
                    }
                }
                {
                    // Don't allow resize
                    {
                        // Decode ripple token
                        auto const wasDecoded = decodeBase58Token(
                            makeSlice(encoded), decodeAsToken, resultBuf);
                        auto const decodedTokenRef = [&] {
                            auto tok = decodeAsToken;
                            if (metadataRef.isRippleLibEncoded())
                                tok = TokenType::None;
                            return Base58TestDetail::decodeBase58Token(
                                encoded, tok);
                        }();
                        if (resultBufSizeDelta == 0)
                        {
                            if (!metadataRef.isRippleLibEncoded())
                            {
                                BEAST_EXPECT(
                                    decodedTokenRef.empty() != wasDecoded);
                                if (wasDecoded)
                                    BEAST_EXPECT(std::equal(
                                        makeSlice(decodedTokenRef).begin(),
                                        makeSlice(decodedTokenRef).end(),
                                        resultBuf.begin(),
                                        resultBuf.end()));
                            }
                            else
                            {
                                BEAST_EXPECT(
                                    decodedTokenRef.empty() != wasDecoded);
                                if (wasDecoded)
                                {
                                    BEAST_EXPECT(
                                        decodedTokenRef.size() - 2 ==
                                        resultBuf.size());
                                    // the first type bytes in decodedToken
                                    // will be the ripple lib prefix. Don't
                                    // check them.
                                    BEAST_EXPECT(std::equal(
                                        makeSlice(decodedTokenRef).begin() + 2,
                                        makeSlice(decodedTokenRef).end(),
                                        resultBuf.begin(),
                                        resultBuf.end()));
                                }
                            }
                        }
                        else
                        {
                            BEAST_EXPECT(!wasDecoded);
                        }
                        memset(resultBuf.data(), 0, resultBuf.size());
                    }

                    {
                        // Deocde bitcoin token
                        auto const wasDecoded = decodeBase58TokenBitcoin(
                            makeSlice(encodedBitcoin),
                            decodeAsToken,
                            resultBuf);
                        auto const decodedTokenRef =
                            Base58TestDetail::decodeBase58Token(
                                encoded, decodeAsToken);
                        if (resultBufSizeDelta == 0)
                        {
                            // ripple lib encoding shouldn't matter for
                            // bitcoin encoding
                            BEAST_EXPECT(decodedTokenRef.empty() != wasDecoded);
                            if (wasDecoded)
                                BEAST_EXPECT(std::equal(
                                    makeSlice(decodedTokenRef).begin(),
                                    makeSlice(decodedTokenRef).end(),
                                    resultBuf.begin(),
                                    resultBuf.end()));
                        }
                        else
                        {
                            BEAST_EXPECT(!wasDecoded);
                        }
                        memset(resultBuf.data(), 0, resultBuf.size());
                    }

                    {
                        // Decode family seed token
                        auto const decodedToken =
                            [&]() -> boost::optional<
                                      std::pair<Slice, ExtraB58Encoding>> {
                            MutableSlice rb = resultBuf;
                            if (metadataRef.isRippleLibEncoded() &&
                                resultBuf.size() == 18)
                            {
                                rb = MutableSlice(
                                    resultBuf.data(), resultBuf.size() - 2);
                            }
                            if (auto encoding = decodeBase58FamilySeed(
                                    makeSlice(encoded), rb))
                            {
                                return std::make_pair(Slice(rb), *encoding);
                            }
                            return {};
                        }();
                        auto const decodedTokenRef =
                            Base58TestDetail::decodeBase58Token(
                                encoded,
                                metadataRef.isRippleLibEncoded()
                                    ? TokenType::None
                                    : TokenType::FamilySeed);
                        size_t const validTokenRefSize =
                            metadataRef.isRippleLibEncoded() ? 18 : 16;
                        if (resultBufSizeDelta == 0 &&
                            decodedTokenRef.size() == validTokenRefSize)
                        {
                            BEAST_EXPECT(
                                decodeAsToken == TokenType::FamilySeed ||
                                !decodedToken);
                            bool const decodedAsRippleLib = decodedToken &&
                                decodedToken->second ==
                                    ExtraB58Encoding::RippleLib;
                            BEAST_EXPECT(
                                !decodedToken ||
                                decodedAsRippleLib ==
                                    metadataRef.isRippleLibEncoded());
                            BEAST_EXPECT(
                                decodedTokenRef.empty() != bool(decodedToken));
                            if (!metadataRef.isRippleLibEncoded())
                            {
                                if (decodedToken)
                                    BEAST_EXPECT(std::equal(
                                        makeSlice(decodedTokenRef).begin(),
                                        makeSlice(decodedTokenRef).end(),
                                        decodedToken->first.begin(),
                                        decodedToken->first.end()));
                            }
                            else
                            {
                                if (decodedToken)
                                    BEAST_EXPECT(std::equal(
                                        makeSlice(decodedTokenRef).begin() + 2,
                                        makeSlice(decodedTokenRef).end(),
                                        decodedToken->first.begin(),
                                        decodedToken->first.end()));
                            }
                        }
                        else
                        {
                            BEAST_EXPECT(!decodedToken);
                        }
                        memset(resultBuf.data(), 0, resultBuf.size());
                    }
                }
            }
        }
    }

    void
    testRandomDecode(std::size_t numTestIterations)
    {
        testcase("Random Decode");
        constexpr size_t maxEncodeSize = 52;  // ceil(log(2^(8*38), 58))
        std::uniform_int_distribution<std::uint8_t> encodeSizeDist(
            5, maxEncodeSize);
        std::uniform_int_distribution<std::uint8_t> leadingZeroesDist(0, 6);
        std::string encoded;
        encoded.reserve(maxEncodeSize);
        std::array<std::uint8_t, 2 * MaxDecodedTokenBytes>
            decodeResultBuf;  //*2 to allow oversized tests
        for (int i = 0; i < numTestIterations; i++)
        {
            std::size_t const encodeSize = encodeSizeDist(engine_);
            std::size_t const leadingZeroes = leadingZeroesDist(engine_);
            encoded.resize(encodeSize);
            randomEncodedBase58(makeMutableSlice(encoded));
            for (int si = 0, se = std::min(encoded.size(), leadingZeroes);
                 si != se;
                 ++si)
            {
                encoded[si] = 'r';
            }

            auto const decodedRef = Base58TestDetail::decodeBase58(
                encoded, Base58TestDetail::rippleInverse);
            auto const decodeSize = decodedRef.size();
            for (auto resultBufSizeDelta : {-5, -1, 0, 1, 5})
            {
                auto resultBuf = MutableSlice(
                    decodeResultBuf.data(),
                    decodeSize - 5 +
                        resultBufSizeDelta);  // -5 for token and checksum
                auto const decoded = decodeBase58ResizableNoChecksumTest(
                    makeSlice(encoded), resultBuf);
                bool const expectDecoded = !decodedRef.empty() &&
                    (decodeSize > 4) &&
                    (decodeSize - 5 + resultBufSizeDelta <=
                     MaxDecodedTokenBytes) &&
                    decodeSize <= MaxDecodedTokenBytes &&
                    resultBufSizeDelta >= 0;
                BEAST_EXPECT(expectDecoded == bool(decoded));
                if (decoded)
                    BEAST_EXPECT(checkMatch(
                        makeSlice(decodedRef),
                        decoded->first,
                        decoded->second));
            }
        }
    }

    void
    testMinMaxEncodeDecode()
    {
        testcase("base58 min/max encode/decode");
        // encode all zeros and all 0xff of different sizes
        constexpr std::size_t maxTestDecodeBytes = 40;
        GenerateDecodeData<maxTestDecodeBytes + 4> genDecodeData;
        for (std::size_t decodeSize = 5; decodeSize <= maxTestDecodeBytes;
             ++decodeSize)
        {
            assert(decodeSize <= genDecodeData.maxResultSize() - 4);

            for (bool allZeros : {true, false})
            {
                MutableSlice decodeSlice =
                    genDecodeData.resultSize(decodeSize)
                        .fillPrefix(decodeSize, allZeros ? 0 : 0xff)
                        .generate();

                // encode with old impl
                std::string encoded = Base58TestDetail::base58EncodeToken(
                    static_cast<TokenType>(decodeSlice[0]),
                    decodeSlice.data() + 1,
                    decodeSlice.size() - 5);  // 1 for token, 4 for checksum
                // decode with new impl
                std::array<std::uint8_t, 2 * MaxDecodedTokenBytes>
                    decodeResultBuf;  //*2 to allow oversized tests

                for (auto resultBufSizeDelta : {-5, -1, 0, 1, 5})
                {
                    auto resultBuf = MutableSlice(
                        decodeResultBuf.data(),
                        decodeSize - 5 + resultBufSizeDelta);  // -5 for token
                                                               // and checksum
                    auto const decoded =
                        decodeBase58Resizable(makeSlice(encoded), resultBuf);
                    bool const expectDecoded =
                        decodeSize <= MaxDecodedTokenBytes &&
                        resultBufSizeDelta >= 0;
                    BEAST_EXPECT(expectDecoded == bool(decoded));
                    if (decoded)
                        BEAST_EXPECT(checkMatch(
                            decodeSlice, decoded->first, decoded->second));
                }
            }
        }
    }

    void
    testMinMaxDecode()
    {
        testcase("base58 min/max decode");
        // encode all 'r'(0) and all 'z' (58) of different sizes
        constexpr size_t maxValidEncodeChars = 52;  // ceil(log(2^(8*38), 58))
        constexpr size_t maxEncodeChars =
            3 + maxValidEncodeChars;  // encode some that could overflow
        std::string encoded;
        encoded.reserve(maxEncodeChars);
        std::array<std::uint8_t, 2 * MaxDecodedTokenBytes>
            decodeResultBuf;  //*2 to allow oversized tests
        for (std::size_t decodeSize = 1; decodeSize <= maxEncodeChars;
             ++decodeSize)
        {
            encoded.resize(decodeSize);
            for (bool allZeros : {true, false})
            {
                encoded.assign(decodeSize, allZeros ? 'r' : 'z');
                auto const decodedRef = Base58TestDetail::decodeBase58(
                    encoded, Base58TestDetail::rippleInverse);
                auto const decodeSize = decodedRef.size();
                for (auto resultBufSizeDelta : {-5, -1, 0, 1, 5})
                {
                    auto resultBuf = MutableSlice(
                        decodeResultBuf.data(),
                        decodeSize - 5 + resultBufSizeDelta);  // -5 for token
                                                               // and checksum
                    auto const decoded = decodeBase58ResizableNoChecksumTest(
                        makeSlice(encoded), resultBuf);
                    bool const expectDecoded = !decodedRef.empty() &&
                        (decodeSize > 4) &&
                        (decodeSize - 5 + resultBufSizeDelta <=
                         MaxDecodedTokenBytes) &&
                        decodeSize <= MaxDecodedTokenBytes &&
                        resultBufSizeDelta >= 0;
                    BEAST_EXPECT(expectDecoded == bool(decoded));
                    if (decoded)
                        BEAST_EXPECT(checkMatch(
                            makeSlice(decodedRef),
                            decoded->first,
                            decoded->second));
                }
            }
        }
    }

    void
    testRippleLibEncoded()
    {
        testcase("ripplelib encoded");

        constexpr std::size_t maxTestDecodeBytes = 30;
        enum class FillValue { min, max, random };
        // decodeSize does _not_ include the 3 byte prefix (token type and
        // ripple lib bytes) or 4 byte suffix (checksum)
        auto testIt = [this](
                          std::size_t decodeSize,
                          FillValue fillValue = FillValue::random,
                          std::array<std::uint8_t, 3> const& prefix =
                              std::array<std::uint8_t, 3>{0x01, 0xE1, 0x4B}) {
            GenerateDecodeData<maxTestDecodeBytes + 7> genDecodeData;
            bool const hasRipplelibPrefix =
                prefix[0] == 0x01 && prefix[1] == 0xE1 && prefix[2] == 0x4B;
            assert(decodeSize <= genDecodeData.maxResultSize() - 7);

            genDecodeData.resultSize(decodeSize + 7)
                .varPrefix(makeSlice(prefix));

            if (fillValue != FillValue::random)
            {
                assert(
                    fillValue == FillValue::min || fillValue == FillValue::max);
                genDecodeData.fillPrefix(
                    decodeSize + 7, fillValue == FillValue::min ? 0 : 0xff);
            }

            MutableSlice decodeSlice = genDecodeData.generate();

            // encode with old impl
            std::string encoded = Base58TestDetail::base58EncodeToken(
                static_cast<TokenType>(decodeSlice[0]),
                decodeSlice.data() + 1,
                decodeSlice.size() - 5);  // 1 for token, 4 for checksum
            // decode with new impl
            for (auto resultBufSizeDelta : {-5, -1, 0, 1, 5})
            {
                // use a vector, not an array of max size declared outside the
                // loop to aid sanitizers
                std::vector<std::uint8_t> decodeResultBuf(
                    decodeSize + resultBufSizeDelta);
                auto resultBuf = MutableSlice(
                    decodeResultBuf.data(), decodeResultBuf.size());
                auto const extraB58Encoding =
                    decodeBase58FamilySeed(makeSlice(encoded), resultBuf);

                if (resultBufSizeDelta == 0 && decodeSize == 16 &&
                    hasRipplelibPrefix)
                {
                    BEAST_EXPECT(
                        extraB58Encoding == ExtraB58Encoding::RippleLib);
                    // Don't include the prefix or checksum in the decodeSlice
                    BEAST_EXPECT(std::equal(
                        resultBuf.begin(),
                        resultBuf.end(),
                        decodeSlice.begin() + 3,
                        decodeSlice.end() - 4));
                }
                else
                {
                    BEAST_EXPECT(!extraB58Encoding);
                }
            }
        };

        // encode all zeros and all 0xff of different sizes
        for (std::size_t decodeSize = 5; decodeSize <= maxTestDecodeBytes;
             ++decodeSize)
        {
            for (auto fillValue : {FillValue::min, FillValue::max})
            {
                testIt(decodeSize, fillValue);
            }
        }

        for (std::size_t decodeSize = 14; decodeSize <= 18; ++decodeSize)
        {
            // Test random values
            for (int i = 0; i < 10000; ++i)
                testIt(decodeSize, FillValue::random);
        }

        {
            // Test bad prefix. Starting at a random index, change one value of
            // the prefix, then the adjacent value (mod 3), then the adjacent
            // value to that.
            std::array<std::uint8_t, 3> prefix{0x01, 0xE1, 0x4B};
            std::uniform_int_distribution<std::uint8_t> randIndex(
                0, prefix.size() - 1);
            std::uniform_int_distribution<std::uint8_t> randValue(0, 255);
            for (int i = 0; i < 10000; ++i)
            {
                prefix = {0x01, 0xE1, 0x4B};
                auto const indexToChange = randIndex(engine_);
                for (std::size_t j = 0; j < prefix.size(); ++j)
                {
                    prefix[(indexToChange + j) % prefix.size()] =
                        randValue(engine_);
                    testIt(16, FillValue::random, prefix);
                }
            }
        }
    }

    void
    testMalformed(std::size_t numTestIterations)
    {
        constexpr std::size_t maxTestDecodeBytes = 30;
        auto randTokenType = [this]() -> TokenType {
            constexpr std::array<TokenType, 7> tokenTypes{
                TokenType::NodePublic,
                TokenType::NodePrivate,
                TokenType::AccountID,
                TokenType::AccountPublic,
                TokenType::AccountSecret,
                TokenType::FamilyGenerator,
                TokenType::FamilySeed};
            std::uniform_int_distribution<std::uint8_t> tokenIndexDist(
                0, tokenTypes.size() - 1);
            return tokenTypes[tokenIndexDist(engine_)];
        };

        auto randBase58Value = [this](
                                   boost::optional<std::uint8_t> const& oldChar,
                                   bool allowInvalidChars) -> std::uint8_t {
            if (oldChar)
            {
                if (allowInvalidChars)
                {
                    // adding a random value from 1 to 254 to an uint8 value
                    // (mod 256) will return a random value not equal to the
                    // origional
                    std::uniform_int_distribution<std::uint8_t> d(1, 254);
                    return *oldChar + d(engine_);
                }
                else
                {
                    std::uniform_int_distribution<std::uint8_t> d(0, 57);
                    while (1)
                    {
                        auto const r = d(engine_);
                        if (oldChar != r)
                            return r;
                    }
                }
            }
            else
            {
                if (allowInvalidChars)
                {
                    std::uniform_int_distribution<std::uint8_t> d(0, 255);
                    return d(engine_);
                }
                else
                {
                    std::uniform_int_distribution<std::uint8_t> d(0, 57);
                    return Base58TestDetail::rippleAlphabet[d(engine_)];
                }
            }
        };

        auto reencode = [&](Slice const& decoded, DecodeMetadata const& meta) {
            std::array<std::uint8_t, 1024> tmpBuf;
            std::vector<std::uint8_t> toDecode(decoded.size() + 5);
            toDecode[0] = meta.tokenType;
            memcpy(&toDecode[1], decoded.data(), decoded.size());
            memcpy(
                &toDecode[decoded.size() + 1],
                meta.checksum.data(),
                meta.checksum.size());
            return Base58TestDetail::encodeBase58(
                toDecode.data(),
                toDecode.size(),
                tmpBuf.data(),
                tmpBuf.size(),
                Base58TestDetail::rippleAlphabet);
        };

        constexpr uint8_t mutateChange = 1 << 0;
        constexpr uint8_t mutateRemove = 1 << 1;
        constexpr uint8_t mutateInsert = 1 << 2;
        constexpr uint8_t numMutationTypes = 3;
        static_assert(
            mutateInsert == 1 << (numMutationTypes - 1),
            "numMutationTypes must be adjusted when new mutations are added");

        GenerateDecodeData<maxTestDecodeBytes + 5> genDecodeData;

        auto testIt = [this,
                       &genDecodeData,
                       &randBase58Value,
                       &randTokenType,
                       &reencode](
                          std::size_t decodeSize,
                          std::uint8_t mutations,
                          bool allowInvalidChars) {
            // exactly one mutation type should be set. The `!(mutations &
            // (mutations - 1)` bit twiddling is true when exactly one bit
            // is set (or zero bits are set, but that is checked earlier)
            assert(
                mutations > 0 && mutations < (1 << numMutationTypes) &&
                !(mutations & (mutations - 1)));
            assert(decodeSize <= genDecodeData.maxResultSize() - 5);

            auto const tokenType = randTokenType();

            // decodeSize does _not_ include the one byte token prefix or 4 byte
            // suffix (checksum)
            MutableSlice decodeSlice = genDecodeData.resultSize(decodeSize + 5)
                                           .varPrefix(Slice(&tokenType, 1))
                                           .generate();

            std::string encoded = Base58TestDetail::base58EncodeToken(
                tokenType,
                decodeSlice.data() + 1,
                decodeSlice.size() - 5);  // 1 for token, 4 for checksum
            std::string unmutatedEncoded = encoded;
            if (mutations & mutateChange)
            {
                std::uniform_int_distribution<std::uint8_t> indexDist(
                    0, encoded.size() - 1);
                auto& toChange = encoded[indexDist(engine_)];
                toChange = randBase58Value(toChange, allowInvalidChars);
            }
            if (mutations & mutateRemove)
            {
                std::uniform_int_distribution<std::uint8_t> indexDist(
                    0, encoded.size() - 1);
                encoded.erase(indexDist(engine_), 1);
            }
            if (mutations & mutateInsert)
            {
                std::uniform_int_distribution<std::uint8_t> indexDist(
                    0, encoded.size() - 1);
                auto const toInsert =
                    randBase58Value(boost::none, allowInvalidChars);
                encoded.insert(indexDist(engine_), 1, toInsert);
            }
            {
                // use a vector, not an array of max size declared outside
                // the loop to aid sanitizers.
                for (auto const sizeDelta : {-1, 0, 2})
                {
                    // decode token (exact size)
                    std::vector<std::uint8_t> decodeResultBuf(
                        decodeSize + sizeDelta);
                    auto resultBuf = MutableSlice(
                        decodeResultBuf.data(), decodeResultBuf.size());
                    auto const r = decodeBase58Token(
                        makeSlice(encoded), tokenType, resultBuf);
                    BEAST_EXPECT(!r);
                }
                {
                    // decode resizable. Note the size delta of two.
                    // Appending a new base58 digit may add two bytes to a
                    // decoding
                    std::vector<std::uint8_t> decodeResultBuf(decodeSize + 2);
                    auto resultBuf = MutableSlice(
                        decodeResultBuf.data(), decodeResultBuf.size());
                    {
                        auto const r = decodeBase58Resizable(
                            makeSlice(encoded), resultBuf);
                        // will always fail, as checksum is bad
                        BEAST_EXPECT(!r);
                    }
                    {
                        auto const r = decodeBase58ResizableNoChecksumTest(
                            makeSlice(encoded), resultBuf);
                        if (!allowInvalidChars)
                        {
                            // Checksum isn't checked, new char is valid,
                            // buffer is large enough. So should always
                            // succeed.
                            expect(r);
                            if (r)
                            {
                                auto const reencoded =
                                    reencode(r->first, r->second);
                                BEAST_EXPECT(std::equal(
                                    reencoded.begin(),
                                    reencoded.end(),
                                    encoded.begin(),
                                    encoded.end()));
                            }
                        }
                    }
                }
            }
        };

        std::uniform_int_distribution<std::uint8_t> mutateDist(
            1, numMutationTypes - 1);
        for (std::size_t decodeSize = 14; decodeSize <= 18; ++decodeSize)
        {
            for (int i = 0; i < numTestIterations / 2; ++i)
            {
                auto const mutation = 1 << mutateDist(engine_);
                testIt(decodeSize, mutation, /*allowInvalidChars*/ false);
                testIt(decodeSize, mutation, /*allowInvalidChars*/ true);
            }
        }
    }

    void
    testExportBits()
    {
        testcase("Multiprecision export bits");
        using namespace boost::multiprecision;
        // Export bits must remove leading zeros, except when then value is
        // zero, there there must be exactly one zero.
        {
            // test zero
            std::array<std::uint8_t, 4> dst;
            checked_uint128_t const v{0};
            auto const e = export_bits(v, dst.data(), 8);
            BEAST_EXPECT(std::distance(dst.data(), e) == 1 && dst[0] == 0);
        }
        {
            // test import with leading zeros
            std::array<std::uint8_t, 4> dst{};
            // use hex or will interpret leading zeros as octal
            checked_uint128_t const v{"0x00000000000000000000000042"};
            auto const e = export_bits(v, dst.data(), 8);
            BEAST_EXPECT(std::distance(dst.data(), e) == 1 && dst[0] == 0x42);
        }
        {
            // test calculation that leaves leading zeros
            std::array<std::uint8_t, 4> dst;
            checked_uint128_t const v1{"900000000000000000000000042"};
            checked_uint128_t const v0{"900000000000000000000000000"};
            checked_uint128_t const v = v1 - v0;
            auto const e = export_bits(v, dst.data(), 8);
            BEAST_EXPECT(std::distance(dst.data(), e) == 1 && dst[0] == 42);
        }
    }

public:
    void
    run() override
    {
        {
            std::size_t numTestIterations = 10'000;
            constexpr std::size_t maxIterations = 100'000'000;
            constexpr std::size_t minIterations = 100;
            if (!arg().empty())
            {
                // Use `--unittest-arg` to change the number of test iterations
                // to try
                try
                {
                    std::size_t sz;
                    auto const& a = arg();
                    auto const ai = std::stoi(a, &sz);
                    if (a.size() == sz)
                    {
                        numTestIterations = boost::algorithm::clamp(
                            ai, minIterations, maxIterations);
                    }
                }
                catch (...)
                {
                }
            }
            testRandomEncodeDecode(numTestIterations);
            testRandomDecode(numTestIterations);
            testMalformed(numTestIterations);
        }
        testRippleLibEncoded();
        testMinMaxEncodeDecode();
        testMinMaxDecode();
        testExportBits();
    }
};  // namespace ripple

BEAST_DEFINE_TESTSUITE(Base58, protocol, ripple);

}  // namespace ripple
