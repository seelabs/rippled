
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
            {0x0b, 0x73, 0x8b, 0x03, 0xfd, 0x24, 0xf7, 0x37, 0x29, 0x44, 0x5c,
             0x79, 0x28, 0x89, 0x59, 0xf7, 0x9c, 0x30, 0x05, 0x74, 0x3c, 0xe9,
             0x1b, 0x5b, 0x78, 0x9b, 0xb5, 0xea, 0xeb, 0x87, 0x42, 0x51, 0xa5,
             0x26, 0x9b, 0xad, 0x86, 0xb7, 0xeb, 0x95, 0x15, 0x9f, 0x46, 0x98,
             0x51, 0x40, 0x8b, 0xcd, 0x2f, 0xf6, 0xa2, 0x0a, 0x43, 0x72, 0x86,
             0xf8, 0xa7, 0xd2, 0x05, 0xa9, 0xad, 0x33, 0xa3, 0x0e, 0x56, 0xd1,
             0xde, 0xce, 0x64, 0x29, 0x89, 0x41, 0x96, 0x2e, 0x1c, 0x27, 0x89,
             0xf8, 0x48, 0x33, 0xf9, 0x21, 0xdf, 0x23, 0x55, 0xf2, 0xa7, 0x3b,
             0x14, 0x32, 0xbe, 0x5a, 0x3a, 0x15, 0x4d, 0x44, 0x6f, 0x6a, 0x99,
             0x1e, 0x67, 0xb3, 0x55, 0x85, 0x1b, 0xbc, 0x48, 0x98, 0xcd, 0x1e,
             0xce, 0x60, 0x12, 0x95, 0x59, 0xd5, 0x01, 0x17, 0x51, 0xf7, 0xfa,
             0x64, 0x3f, 0x23, 0xea, 0x60, 0xd8, 0x95, 0xde, 0xa4, 0x5c, 0x97,
             0x0d, 0x4a, 0xe0, 0x6f, 0x74, 0x6b, 0xcf, 0x33, 0xb6, 0x43, 0x32,
             0x0a, 0xcf, 0x39, 0xd9, 0x20, 0xc4, 0x8b, 0xdc, 0x9d, 0xfd, 0x58,
             0x49, 0xb3, 0x0a, 0x4d, 0x4f, 0x37, 0x0e, 0xa9, 0xda, 0xbb, 0x82,
             0x1e, 0x77, 0x93, 0x87, 0x61, 0x29, 0x23, 0x13, 0xd6, 0xb2, 0x12,
             0xd8, 0x82, 0xd9, 0xff, 0xbb, 0x35, 0x43, 0x15, 0x54, 0x4e, 0xd5,
             0x55, 0xc6, 0x6e, 0xae, 0x13, 0x3e, 0xa8, 0x97, 0x05, 0x20, 0xb8,
             0x38, 0x93, 0x7d, 0x20, 0xfa, 0xca, 0x3f, 0xd5, 0x7d, 0x45, 0x52,
             0x45, 0xcd, 0x1a, 0xb2, 0x79, 0x79, 0xd1, 0xd9, 0x46, 0x25, 0x1d,
             0x3a, 0xa7, 0x46, 0x6c, 0x63, 0x06, 0xb2, 0xfc, 0xfd, 0xaa, 0x40,
             0x80, 0xbd, 0x64, 0x4f, 0x68, 0x66, 0x24, 0x35, 0x4a, 0x07, 0x84,
             0x2b, 0xc8, 0xbf, 0x94, 0x88, 0x3e, 0xc3, 0x91, 0x25, 0x39, 0x2a,
             0xac, 0x73, 0x25}};

        RsaSha256 const rsa0(makeSlice(rsa0PublicKey), makeSlice(rsa0Sig));
        {
            auto const rsa0EncodedFulfillment =
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
                "\x7f\x05\xae\xbb\x35\xb9\xe7\x6f\xe1\x81\x82\x01\x00\x0b\x73"
                "\x8b\x03\xfd\x24\xf7\x37\x29\x44\x5c\x79\x28\x89\x59\xf7\x9c"
                "\x30\x05\x74\x3c\xe9\x1b\x5b\x78\x9b\xb5\xea\xeb\x87\x42\x51"
                "\xa5\x26\x9b\xad\x86\xb7\xeb\x95\x15\x9f\x46\x98\x51\x40\x8b"
                "\xcd\x2f\xf6\xa2\x0a\x43\x72\x86\xf8\xa7\xd2\x05\xa9\xad\x33"
                "\xa3\x0e\x56\xd1\xde\xce\x64\x29\x89\x41\x96\x2e\x1c\x27\x89"
                "\xf8\x48\x33\xf9\x21\xdf\x23\x55\xf2\xa7\x3b\x14\x32\xbe\x5a"
                "\x3a\x15\x4d\x44\x6f\x6a\x99\x1e\x67\xb3\x55\x85\x1b\xbc\x48"
                "\x98\xcd\x1e\xce\x60\x12\x95\x59\xd5\x01\x17\x51\xf7\xfa\x64"
                "\x3f\x23\xea\x60\xd8\x95\xde\xa4\x5c\x97\x0d\x4a\xe0\x6f\x74"
                "\x6b\xcf\x33\xb6\x43\x32\x0a\xcf\x39\xd9\x20\xc4\x8b\xdc\x9d"
                "\xfd\x58\x49\xb3\x0a\x4d\x4f\x37\x0e\xa9\xda\xbb\x82\x1e\x77"
                "\x93\x87\x61\x29\x23\x13\xd6\xb2\x12\xd8\x82\xd9\xff\xbb\x35"
                "\x43\x15\x54\x4e\xd5\x55\xc6\x6e\xae\x13\x3e\xa8\x97\x05\x20"
                "\xb8\x38\x93\x7d\x20\xfa\xca\x3f\xd5\x7d\x45\x52\x45\xcd\x1a"
                "\xb2\x79\x79\xd1\xd9\x46\x25\x1d\x3a\xa7\x46\x6c\x63\x06\xb2"
                "\xfc\xfd\xaa\x40\x80\xbd\x64\x4f\x68\x66\x24\x35\x4a\x07\x84"
                "\x2b\xc8\xbf\x94\x88\x3e\xc3\x91\x25\x39\x2a\xac\x73\x25"s;
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
                &rsa0,
                rsa0Msg,
                rsa0EncodedFulfillment,
                rsa0EncodedCondition,
                rsa0EncodedFingerprint);
        }
    }

    void
    run()
    {
        testRsa0();
    }
};

BEAST_DEFINE_TESTSUITE(Conditions_rsa, conditions, ripple);
}  // cryptoconditions
}  // ripple
