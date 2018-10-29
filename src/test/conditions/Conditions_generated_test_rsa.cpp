
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

// THIS FILE WAS AUTOMATICALLY GENERATED -- DO NOT EDIT

//==============================================================================

#include <test/conditions/ConditionsTestBase.h>

namespace ripple {
namespace cryptoconditions {

class Conditions_rsa_test : public ConditionsTestBase
{
    void
    testRsa0()
    {
        testcase("Rsa0");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * rsa0

        auto const rsa0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 256> const rsa0PublicKey{
            {0xa7, 0x6c, 0x6f, 0x92, 0x15, 0x7c, 0x56, 0xe8, 0xad, 0x03, 0x9a,
             0x2b, 0x04, 0x6c, 0xb6, 0x41, 0x02, 0x4a, 0x73, 0x12, 0x87, 0x86,
             0xc2, 0x58, 0x3c, 0xd0, 0x04, 0xa4, 0x8c, 0x98, 0xa7, 0x7b, 0x79,
             0x21, 0xba, 0xde, 0x38, 0xca, 0x67, 0xef, 0xdd, 0xb4, 0x9e, 0x37,
             0x8b, 0x65, 0xf5, 0x5a, 0xce, 0xfe, 0x16, 0xfd, 0xad, 0x62, 0x7b,
             0xa7, 0xba, 0x5e, 0x2c, 0x10, 0x99, 0x40, 0x30, 0xf6, 0xee, 0x26,
             0x49, 0x27, 0x3b, 0xc5, 0xb8, 0x21, 0x30, 0x5c, 0x3e, 0x46, 0xb6,
             0x02, 0x15, 0xc4, 0x0f, 0x18, 0x24, 0xf2, 0xcb, 0x38, 0x8b, 0x47,
             0x77, 0x31, 0x63, 0x71, 0x48, 0xee, 0x76, 0xb3, 0xfd, 0xb6, 0xa2,
             0xa0, 0x8e, 0x5a, 0x01, 0x73, 0x32, 0x35, 0xbf, 0x98, 0x39, 0x3e,
             0x1a, 0xab, 0x87, 0xda, 0xbf, 0x82, 0x0c, 0x71, 0x2e, 0xb5, 0x8a,
             0x54, 0xcc, 0x7f, 0xc2, 0xbf, 0xb4, 0x7f, 0xe2, 0xe5, 0xb9, 0x0f,
             0x6e, 0xee, 0x4a, 0x2f, 0x8b, 0x20, 0xb1, 0xea, 0xaa, 0xdc, 0x89,
             0x59, 0x34, 0x7c, 0xba, 0x9b, 0x3a, 0xa5, 0xe1, 0x57, 0x70, 0x64,
             0x68, 0xf1, 0xbb, 0xae, 0x40, 0xf6, 0xf8, 0x90, 0x63, 0x55, 0x75,
             0x3a, 0x3a, 0x9e, 0x71, 0x6a, 0x93, 0x50, 0x8d, 0x62, 0xfd, 0xaf,
             0x89, 0x8a, 0x1f, 0x27, 0x57, 0xdd, 0x30, 0xd3, 0xca, 0x7d, 0x8f,
             0xa5, 0x59, 0xca, 0x4e, 0xe4, 0xec, 0x9d, 0x25, 0x20, 0xa7, 0x81,
             0x6a, 0x06, 0xa6, 0xbb, 0xb2, 0xf1, 0x75, 0xf9, 0x6b, 0x29, 0xc7,
             0x75, 0xc0, 0x9f, 0xcc, 0x87, 0x42, 0x19, 0x9f, 0x8b, 0xb5, 0x2b,
             0xa6, 0x9a, 0xc3, 0x61, 0x88, 0x74, 0x82, 0x91, 0x76, 0xf5, 0x46,
             0x8c, 0xe7, 0x81, 0xf5, 0x2c, 0x25, 0x6d, 0x5d, 0x50, 0x18, 0x03,
             0xd6, 0x1d, 0xed, 0x8d, 0x74, 0x7f, 0x05, 0xae, 0xbb, 0x35, 0xb9,
             0xe7, 0x6f, 0xe1}};
        std::array<std::uint8_t, 256> const rsa0Sig{
            {0x71, 0xb8, 0x65, 0x03, 0x03, 0x44, 0x48, 0x41, 0xf2, 0xcf, 0x59,
             0xcc, 0xd2, 0x51, 0x7b, 0xf2, 0xa4, 0x55, 0x34, 0xf6, 0xeb, 0x50,
             0xbf, 0xf7, 0xe8, 0xb8, 0x87, 0xf2, 0xbe, 0xef, 0x63, 0xf2, 0xf4,
             0xdc, 0xb0, 0x67, 0x19, 0x7c, 0x4c, 0x53, 0x78, 0xfd, 0x17, 0xae,
             0x8c, 0x3b, 0x64, 0x3d, 0x4d, 0x61, 0xb1, 0x04, 0x2b, 0xad, 0x52,
             0x58, 0x67, 0x11, 0xda, 0x82, 0x23, 0x23, 0xbd, 0xab, 0x70, 0x09,
             0x0f, 0xc6, 0x41, 0xce, 0x80, 0x88, 0x25, 0x52, 0x64, 0x77, 0x5b,
             0xb4, 0xa6, 0x90, 0x37, 0x69, 0x75, 0xc9, 0x79, 0xeb, 0xe7, 0xbf,
             0x52, 0x5f, 0x26, 0xf5, 0xf4, 0xc3, 0xde, 0xbd, 0xfe, 0x29, 0xa2,
             0xdf, 0x3c, 0xdf, 0x8d, 0x20, 0xf2, 0xb7, 0xa7, 0x32, 0x96, 0x95,
             0xdc, 0xc2, 0xb6, 0x01, 0x31, 0x3f, 0x5e, 0x5d, 0x1f, 0x72, 0x83,
             0x2e, 0x80, 0x02, 0xc1, 0x1f, 0xa6, 0x4b, 0xb4, 0x78, 0xfa, 0xcc,
             0x13, 0x40, 0xac, 0x82, 0xed, 0x10, 0xe6, 0x38, 0xcd, 0x5f, 0xb2,
             0x83, 0xe6, 0x6c, 0xf9, 0xea, 0x09, 0x1e, 0x0d, 0xa0, 0x03, 0xf0,
             0x73, 0xd0, 0x39, 0xf8, 0x4c, 0x02, 0xbb, 0x64, 0x0b, 0x20, 0x55,
             0x18, 0x04, 0x3a, 0x91, 0x12, 0x6e, 0x16, 0xd6, 0xb7, 0x39, 0x4d,
             0xd4, 0x5e, 0x43, 0x52, 0xe6, 0x4a, 0xf6, 0x1b, 0x4c, 0xb8, 0x9e,
             0x75, 0x12, 0xdc, 0x0f, 0x90, 0x9e, 0x17, 0x20, 0x06, 0xc1, 0x95,
             0x26, 0xe7, 0xd2, 0x6d, 0x4c, 0x49, 0x29, 0xaf, 0xdd, 0xb5, 0xa2,
             0x72, 0x89, 0x01, 0xf9, 0x8b, 0xd2, 0xd5, 0xbc, 0x2a, 0x1a, 0x7c,
             0xf4, 0xe4, 0xeb, 0x30, 0x25, 0xc1, 0xd9, 0x5e, 0xc7, 0x4f, 0x18,
             0xe9, 0x7a, 0x31, 0xe2, 0x5e, 0xb4, 0x6c, 0xbb, 0x9a, 0x8f, 0x04,
             0x95, 0xd2, 0x5d, 0xc1, 0x6c, 0x9f, 0x03, 0x92, 0xc6, 0x7e, 0xa2,
             0x2d, 0x5b, 0x32}};

        auto rsa0 = std::make_unique<RsaSha256>(
            makeSlice(rsa0PublicKey), makeSlice(rsa0Sig));
        {
            auto rsa0EncodedFulfillment =
                "\xa3\x82\x02\x08\x80\x82\x01\x00\xa7\x6c\x6f\x92\x15\x7c\x56"
                "\xe8\xad\x03\x9a\x2b\x04\x6c\xb6\x41\x02\x4a\x73\x12\x87\x86"
                "\xc2\x58\x3c\xd0\x04\xa4\x8c\x98\xa7\x7b\x79\x21\xba\xde\x38"
                "\xca\x67\xef\xdd\xb4\x9e\x37\x8b\x65\xf5\x5a\xce\xfe\x16\xfd"
                "\xad\x62\x7b\xa7\xba\x5e\x2c\x10\x99\x40\x30\xf6\xee\x26\x49"
                "\x27\x3b\xc5\xb8\x21\x30\x5c\x3e\x46\xb6\x02\x15\xc4\x0f\x18"
                "\x24\xf2\xcb\x38\x8b\x47\x77\x31\x63\x71\x48\xee\x76\xb3\xfd"
                "\xb6\xa2\xa0\x8e\x5a\x01\x73\x32\x35\xbf\x98\x39\x3e\x1a\xab"
                "\x87\xda\xbf\x82\x0c\x71\x2e\xb5\x8a\x54\xcc\x7f\xc2\xbf\xb4"
                "\x7f\xe2\xe5\xb9\x0f\x6e\xee\x4a\x2f\x8b\x20\xb1\xea\xaa\xdc"
                "\x89\x59\x34\x7c\xba\x9b\x3a\xa5\xe1\x57\x70\x64\x68\xf1\xbb"
                "\xae\x40\xf6\xf8\x90\x63\x55\x75\x3a\x3a\x9e\x71\x6a\x93\x50"
                "\x8d\x62\xfd\xaf\x89\x8a\x1f\x27\x57\xdd\x30\xd3\xca\x7d\x8f"
                "\xa5\x59\xca\x4e\xe4\xec\x9d\x25\x20\xa7\x81\x6a\x06\xa6\xbb"
                "\xb2\xf1\x75\xf9\x6b\x29\xc7\x75\xc0\x9f\xcc\x87\x42\x19\x9f"
                "\x8b\xb5\x2b\xa6\x9a\xc3\x61\x88\x74\x82\x91\x76\xf5\x46\x8c"
                "\xe7\x81\xf5\x2c\x25\x6d\x5d\x50\x18\x03\xd6\x1d\xed\x8d\x74"
                "\x7f\x05\xae\xbb\x35\xb9\xe7\x6f\xe1\x81\x82\x01\x00\x71\xb8"
                "\x65\x03\x03\x44\x48\x41\xf2\xcf\x59\xcc\xd2\x51\x7b\xf2\xa4"
                "\x55\x34\xf6\xeb\x50\xbf\xf7\xe8\xb8\x87\xf2\xbe\xef\x63\xf2"
                "\xf4\xdc\xb0\x67\x19\x7c\x4c\x53\x78\xfd\x17\xae\x8c\x3b\x64"
                "\x3d\x4d\x61\xb1\x04\x2b\xad\x52\x58\x67\x11\xda\x82\x23\x23"
                "\xbd\xab\x70\x09\x0f\xc6\x41\xce\x80\x88\x25\x52\x64\x77\x5b"
                "\xb4\xa6\x90\x37\x69\x75\xc9\x79\xeb\xe7\xbf\x52\x5f\x26\xf5"
                "\xf4\xc3\xde\xbd\xfe\x29\xa2\xdf\x3c\xdf\x8d\x20\xf2\xb7\xa7"
                "\x32\x96\x95\xdc\xc2\xb6\x01\x31\x3f\x5e\x5d\x1f\x72\x83\x2e"
                "\x80\x02\xc1\x1f\xa6\x4b\xb4\x78\xfa\xcc\x13\x40\xac\x82\xed"
                "\x10\xe6\x38\xcd\x5f\xb2\x83\xe6\x6c\xf9\xea\x09\x1e\x0d\xa0"
                "\x03\xf0\x73\xd0\x39\xf8\x4c\x02\xbb\x64\x0b\x20\x55\x18\x04"
                "\x3a\x91\x12\x6e\x16\xd6\xb7\x39\x4d\xd4\x5e\x43\x52\xe6\x4a"
                "\xf6\x1b\x4c\xb8\x9e\x75\x12\xdc\x0f\x90\x9e\x17\x20\x06\xc1"
                "\x95\x26\xe7\xd2\x6d\x4c\x49\x29\xaf\xdd\xb5\xa2\x72\x89\x01"
                "\xf9\x8b\xd2\xd5\xbc\x2a\x1a\x7c\xf4\xe4\xeb\x30\x25\xc1\xd9"
                "\x5e\xc7\x4f\x18\xe9\x7a\x31\xe2\x5e\xb4\x6c\xbb\x9a\x8f\x04"
                "\x95\xd2\x5d\xc1\x6c\x9f\x03\x92\xc6\x7e\xa2\x2d\x5b\x32"s;
            auto const rsa0EncodedCondition =
                "\xa3\x27\x80\x20\x07\x62\xb2\xf0\x74\x9d\x30\xf9\xaa\xf6\x56"
                "\x29\x05\x9d\xb1\x00\x4b\xd6\x8f\x1e\x1d\xa7\x38\xb0\x34\x9c"
                "\x27\xaa\x3a\xfa\x12\x09\x81\x03\x01\x00\x00"s;
            auto const rsa0EncodedFingerprint =
                "\x30\x82\x01\x04\x80\x82\x01\x00\xa7\x6c\x6f\x92\x15\x7c\x56"
                "\xe8\xad\x03\x9a\x2b\x04\x6c\xb6\x41\x02\x4a\x73\x12\x87\x86"
                "\xc2\x58\x3c\xd0\x04\xa4\x8c\x98\xa7\x7b\x79\x21\xba\xde\x38"
                "\xca\x67\xef\xdd\xb4\x9e\x37\x8b\x65\xf5\x5a\xce\xfe\x16\xfd"
                "\xad\x62\x7b\xa7\xba\x5e\x2c\x10\x99\x40\x30\xf6\xee\x26\x49"
                "\x27\x3b\xc5\xb8\x21\x30\x5c\x3e\x46\xb6\x02\x15\xc4\x0f\x18"
                "\x24\xf2\xcb\x38\x8b\x47\x77\x31\x63\x71\x48\xee\x76\xb3\xfd"
                "\xb6\xa2\xa0\x8e\x5a\x01\x73\x32\x35\xbf\x98\x39\x3e\x1a\xab"
                "\x87\xda\xbf\x82\x0c\x71\x2e\xb5\x8a\x54\xcc\x7f\xc2\xbf\xb4"
                "\x7f\xe2\xe5\xb9\x0f\x6e\xee\x4a\x2f\x8b\x20\xb1\xea\xaa\xdc"
                "\x89\x59\x34\x7c\xba\x9b\x3a\xa5\xe1\x57\x70\x64\x68\xf1\xbb"
                "\xae\x40\xf6\xf8\x90\x63\x55\x75\x3a\x3a\x9e\x71\x6a\x93\x50"
                "\x8d\x62\xfd\xaf\x89\x8a\x1f\x27\x57\xdd\x30\xd3\xca\x7d\x8f"
                "\xa5\x59\xca\x4e\xe4\xec\x9d\x25\x20\xa7\x81\x6a\x06\xa6\xbb"
                "\xb2\xf1\x75\xf9\x6b\x29\xc7\x75\xc0\x9f\xcc\x87\x42\x19\x9f"
                "\x8b\xb5\x2b\xa6\x9a\xc3\x61\x88\x74\x82\x91\x76\xf5\x46\x8c"
                "\xe7\x81\xf5\x2c\x25\x6d\x5d\x50\x18\x03\xd6\x1d\xed\x8d\x74"
                "\x7f\x05\xae\xbb\x35\xb9\xe7\x6f\xe1"s;
            check(
                std::move(rsa0),
                rsa0Msg,
                std::move(rsa0EncodedFulfillment),
                rsa0EncodedCondition,
                rsa0EncodedFingerprint);
        }
    }

    void
    run() override
    {
        testRsa0();
    }
};

BEAST_DEFINE_TESTSUITE(Conditions_rsa, conditions, ripple);
}  // namespace cryptoconditions
}  // namespace ripple
