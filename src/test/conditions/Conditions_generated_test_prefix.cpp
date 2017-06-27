
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

class Conditions_prefix_test : public ConditionsTestBase
{
    void
    testPrefix0()
    {
        testcase("Prefix0");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** preim1

        auto const preim1Preimage = "I am root"s;
        auto const preim1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto preim1 =
            std::make_unique<PreimageSha256>(makeSlice(preim1Preimage));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(preim1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x16\x80\x02\x50\x30\x81\x01\x0e\xa2\x0d\xa0\x0b\x80\x09"
                "\x49\x20\x61\x6d\x20\x72\x6f\x6f\x74"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2a\x80\x20\x6e\x0d\x78\x32\x51\x7e\x2d\x0d\xec\xbc\x76"
                "\xf8\x96\x6b\xef\x3c\xd1\x3c\x58\xa9\x9e\x5e\x68\x0d\xf2\xf4"
                "\x83\x00\xd4\x9c\x18\x9c\x81\x02\x04\x19\x82\x02\x07\x80"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x30\x80\x02\x50\x30\x81\x01\x0e\xa2\x27\xa0\x25\x80\x20"
                "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
                "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
                "\xeb\x4e\x81\x01\x09"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix1()
    {
        testcase("Prefix1");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** preim2

        auto const preim2Preimage = "I am root"s;
        auto const preim2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto preim2 =
            std::make_unique<PreimageSha256>(makeSlice(preim2Preimage));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(preim2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x21\x80\x02\x50\x30\x81\x01\x0e\xa2\x18\xa1\x16\x80\x02"
                "\x50\x31\x81\x01\x0e\xa2\x0d\xa0\x0b\x80\x09\x49\x20\x61\x6d"
                "\x20\x72\x6f\x6f\x74"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2a\x80\x20\x50\x02\x05\xf3\xc9\x64\x56\xe6\x93\x7b\xa1"
                "\x71\x6a\xe5\xd9\xd9\x48\x9f\x6c\x07\xa2\x84\x61\x35\x28\x2c"
                "\xa4\xc1\x51\x42\x89\x31\x81\x02\x08\x29\x82\x02\x07\x80"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x35\x80\x02\x50\x30\x81\x01\x0e\xa2\x2c\xa1\x2a\x80\x20"
                "\xc5\xfb\xcf\xa9\x11\x1d\x41\xc3\x1d\xca\x7a\x24\x9c\x22\x55"
                "\xd3\xe0\xd2\xee\x24\x73\xf7\xcf\x78\xb0\x11\x02\x5b\x7a\xb4"
                "\x11\x7c\x81\x02\x04\x19\x82\x02\x07\x80"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix2()
    {
        testcase("Prefix2");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** prefix2
        // **** preim3

        auto const preim3Preimage = "I am root"s;
        auto const preim3Msg = "P2P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix2Prefix = "P2"s;
        auto const prefix2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix2MaxMsgLength = 14;
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto preim3 =
            std::make_unique<PreimageSha256>(makeSlice(preim3Preimage));
        auto prefix2 = std::make_unique<PrefixSha256>(
            makeSlice(prefix2Prefix), prefix2MaxMsgLength, std::move(preim3));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(prefix2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x2c\x80\x02\x50\x30\x81\x01\x0e\xa2\x23\xa1\x21\x80\x02"
                "\x50\x31\x81\x01\x0e\xa2\x18\xa1\x16\x80\x02\x50\x32\x81\x01"
                "\x0e\xa2\x0d\xa0\x0b\x80\x09\x49\x20\x61\x6d\x20\x72\x6f\x6f"
                "\x74"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2a\x80\x20\x69\xaa\xfe\x99\x3c\xb0\x8d\x77\xba\xe3\x75"
                "\x3d\x6b\xce\xab\xab\x11\x28\x63\xc2\x4a\x2b\xff\x84\xc9\xa0"
                "\xee\xa8\xa7\x57\xec\x43\x81\x02\x0c\x39\x82\x02\x07\x80"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x35\x80\x02\x50\x30\x81\x01\x0e\xa2\x2c\xa1\x2a\x80\x20"
                "\xad\xff\xf5\xae\x14\x94\xaa\x40\xc1\x0a\x94\xef\x53\x65\xf7"
                "\xb9\x36\xd9\xcf\x32\x0c\xbd\xdd\x22\x2e\x20\x9b\x06\xd9\xe7"
                "\x6b\x00\x81\x02\x08\x29\x82\x02\x07\x80"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix3()
    {
        testcase("Prefix3");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** thresh2
        // **** preim3

        auto const preim3Preimage = "I am root"s;
        auto const preim3Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const thresh2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto preim3 =
            std::make_unique<PreimageSha256>(makeSlice(preim3Preimage));
        std::vector<std::unique_ptr<Fulfillment>> thresh2Subfulfillments;
        thresh2Subfulfillments.emplace_back(std::move(preim3));
        std::vector<Condition> thresh2Subconditions{};
        auto thresh2 = std::make_unique<ThresholdSha256>(
            std::move(thresh2Subfulfillments), std::move(thresh2Subconditions));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(thresh2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x27\x80\x02\x50\x30\x81\x01\x0e\xa2\x1e\xa1\x1c\x80\x02"
                "\x50\x31\x81\x01\x0e\xa2\x13\xa2\x11\xa0\x0d\xa0\x0b\x80\x09"
                "\x49\x20\x61\x6d\x20\x72\x6f\x6f\x74\xa1\x00"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2a\x80\x20\x01\xd0\xbe\xb8\x89\xbc\x77\x91\x3d\xc1\x3c"
                "\x1d\x8d\x18\xe1\xcc\xf1\x44\x40\x62\x9a\xc2\x74\xc6\x2c\x93"
                "\xec\x68\xac\x55\xf6\x2e\x81\x02\x0c\x29\x82\x02\x05\xa0"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x35\x80\x02\x50\x30\x81\x01\x0e\xa2\x2c\xa1\x2a\x80\x20"
                "\xff\x6a\x9f\x56\xfb\xa3\xd4\xd7\x72\x2a\x85\x3b\xcf\x8e\x6f"
                "\x54\x51\xcb\x5c\xb4\x34\x51\xf5\x44\xc5\xea\xb6\x90\x85\x0d"
                "\x99\x0c\x81\x02\x08\x19\x82\x02\x05\xa0"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix4()
    {
        testcase("Prefix4");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** thresh2
        // **** Preim4Cond
        // **** Rsa5Cond
        // **** Ed6Cond
        // **** preim3

        auto const preim3Preimage = "I am root"s;
        auto const preim3Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const thresh2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim4CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim4Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim4CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa5CondConditionFingerprint =
            "\x99\xfb\x0b\x38\x94\x4d\x20\x85\xc8\xda\x3a\x64\x31\x44\x6f"
            "\x6c\x3b\x46\x25\x50\xd7\x7f\xdf\xee\x75\x72\x71\xf9\x61\x40"
            "\x63\xfa"s;
        Condition const Rsa5Cond{Type::rsaSha256,
                                 65536,
                                 makeSlice(Rsa5CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const Ed6CondConditionFingerprint =
            "\x00\xd3\xc9\x24\x3f\x2d\x2e\x64\x93\xa8\x49\x29\x82\x75\xea"
            "\xbf\xe3\x53\x7f\x8e\x45\x16\xdb\x5e\xc6\xdf\x39\xd2\xcb\xea"
            "\x62\xfb"s;
        Condition const Ed6Cond{Type::ed25519Sha256,
                                131072,
                                makeSlice(Ed6CondConditionFingerprint),
                                std::bitset<5>{0}};
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto preim3 =
            std::make_unique<PreimageSha256>(makeSlice(preim3Preimage));
        std::vector<std::unique_ptr<Fulfillment>> thresh2Subfulfillments;
        thresh2Subfulfillments.emplace_back(std::move(preim3));
        std::vector<Condition> thresh2Subconditions{
            {Preim4Cond, Rsa5Cond, Ed6Cond}};
        auto thresh2 = std::make_unique<ThresholdSha256>(
            std::move(thresh2Subfulfillments), std::move(thresh2Subconditions));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(thresh2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x81\xa4\x80\x02\x50\x30\x81\x01\x0e\xa2\x81\x9a\xa1\x81"
                "\x97\x80\x02\x50\x31\x81\x01\x0e\xa2\x81\x8d\xa2\x81\x8a\xa0"
                "\x0d\xa0\x0b\x80\x09\x49\x20\x61\x6d\x20\x72\x6f\x6f\x74\xa1"
                "\x79\xa0\x25\x80\x20\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8"
                "\x5f\x5e\x54\x2d\x6a\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc"
                "\xfd\x53\xee\x93\x58\xeb\x4e\x81\x01\x09\xa3\x27\x80\x20\x99"
                "\xfb\x0b\x38\x94\x4d\x20\x85\xc8\xda\x3a\x64\x31\x44\x6f\x6c"
                "\x3b\x46\x25\x50\xd7\x7f\xdf\xee\x75\x72\x71\xf9\x61\x40\x63"
                "\xfa\x81\x03\x01\x00\x00\xa4\x27\x80\x20\x00\xd3\xc9\x24\x3f"
                "\x2d\x2e\x64\x93\xa8\x49\x29\x82\x75\xea\xbf\xe3\x53\x7f\x8e"
                "\x45\x16\xdb\x5e\xc6\xdf\x39\xd2\xcb\xea\x62\xfb\x81\x03\x02"
                "\x00\x00"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\xa0\xde\xd9\x5d\x95\xc3\x47\xb6\xe3\x9d\xd2"
                "\xa9\xef\x29\xa9\xeb\xb1\x88\xd3\x7a\x23\x7d\xff\xec\x0b\x3c"
                "\xe5\x0f\x70\x88\xaa\xd1\x81\x03\x02\x18\x20\x82\x02\x03\xb8"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\x50\xd9\x24\x4f\x7a\x00\x8c\x96\xfb\x26\xaf\x1e\x96\xb5\xc1"
                "\x36\x41\x7d\x28\xb2\xec\x83\x26\xe1\x61\x0b\xb0\x8a\xe1\x7e"
                "\xed\x84\x81\x03\x02\x14\x10\x82\x02\x03\xb8"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix5()
    {
        testcase("Prefix5");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** thresh2
        // **** Preim9Cond
        // **** Rsa10Cond
        // **** Ed11Cond
        // **** preim3
        // **** thresh4
        // ***** Preim6Cond
        // ***** Rsa7Cond
        // ***** Ed8Cond
        // ***** preim5

        auto const preim3Preimage = "I am root"s;
        auto const preim3Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const preim5Preimage = "I am root"s;
        auto const preim5Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const thresh4Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim6CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim6Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim6CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa7CondConditionFingerprint =
            "\x6c\x7b\xea\x83\xa1\xf4\x82\x3d\x36\xe7\x6e\xae\x1a\xbc\xa0"
            "\xba\x90\x3d\x96\xc1\xe6\xad\x3a\x47\xa5\xcb\x88\xab\x3c\x5f"
            "\xcc\xd5"s;
        Condition const Rsa7Cond{Type::rsaSha256,
                                 65536,
                                 makeSlice(Rsa7CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const Ed8CondConditionFingerprint =
            "\xf1\x68\x96\xa6\x2e\xef\x7f\x47\x06\x51\x4c\xc6\x7e\x24\xf7"
            "\x29\x84\x9c\xd6\xb0\xd9\x4b\xd9\x0f\xc9\x34\x01\x9d\x92\xeb"
            "\xbc\x0a"s;
        Condition const Ed8Cond{Type::ed25519Sha256,
                                131072,
                                makeSlice(Ed8CondConditionFingerprint),
                                std::bitset<5>{0}};
        auto const thresh2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim9CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim9Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim9CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa10CondConditionFingerprint =
            "\x3c\x73\x38\xcf\x23\xc6\x31\x53\x28\xc4\x27\xf8\x95\x87\x99"
            "\x83\x2d\x35\x3c\x03\x9b\xd1\xff\xff\x2e\x53\x20\xe9\x5e\x62"
            "\xb9\xb7"s;
        Condition const Rsa10Cond{Type::rsaSha256,
                                  65536,
                                  makeSlice(Rsa10CondConditionFingerprint),
                                  std::bitset<5>{0}};
        auto const Ed11CondConditionFingerprint =
            "\x41\x80\x08\xb2\x60\x74\x57\x6d\xac\xed\x74\x7f\x54\xdb\x96"
            "\x18\x91\x06\x0a\x95\xa1\x49\x17\xc7\x65\xe3\x94\xc8\x5e\x2c"
            "\x92\x20"s;
        Condition const Ed11Cond{Type::ed25519Sha256,
                                 131072,
                                 makeSlice(Ed11CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto preim3 =
            std::make_unique<PreimageSha256>(makeSlice(preim3Preimage));
        auto preim5 =
            std::make_unique<PreimageSha256>(makeSlice(preim5Preimage));
        std::vector<std::unique_ptr<Fulfillment>> thresh4Subfulfillments;
        thresh4Subfulfillments.emplace_back(std::move(preim5));
        std::vector<Condition> thresh4Subconditions{
            {Preim6Cond, Rsa7Cond, Ed8Cond}};
        auto thresh4 = std::make_unique<ThresholdSha256>(
            std::move(thresh4Subfulfillments), std::move(thresh4Subconditions));
        std::vector<std::unique_ptr<Fulfillment>> thresh2Subfulfillments;
        thresh2Subfulfillments.emplace_back(std::move(preim3));
        thresh2Subfulfillments.emplace_back(std::move(thresh4));
        std::vector<Condition> thresh2Subconditions{
            {Preim9Cond, Rsa10Cond, Ed11Cond}};
        auto thresh2 = std::make_unique<ThresholdSha256>(
            std::move(thresh2Subfulfillments), std::move(thresh2Subconditions));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(thresh2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x82\x01\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x82\x01\x2b"
                "\xa1\x82\x01\x27\x80\x02\x50\x31\x81\x01\x0e\xa2\x82\x01\x1c"
                "\xa2\x82\x01\x18\xa0\x81\x9a\xa0\x0b\x80\x09\x49\x20\x61\x6d"
                "\x20\x72\x6f\x6f\x74\xa2\x81\x8a\xa0\x0d\xa0\x0b\x80\x09\x49"
                "\x20\x61\x6d\x20\x72\x6f\x6f\x74\xa1\x79\xa0\x25\x80\x20\x5d"
                "\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a\x5b"
                "\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58\xeb"
                "\x4e\x81\x01\x09\xa3\x27\x80\x20\x6c\x7b\xea\x83\xa1\xf4\x82"
                "\x3d\x36\xe7\x6e\xae\x1a\xbc\xa0\xba\x90\x3d\x96\xc1\xe6\xad"
                "\x3a\x47\xa5\xcb\x88\xab\x3c\x5f\xcc\xd5\x81\x03\x01\x00\x00"
                "\xa4\x27\x80\x20\xf1\x68\x96\xa6\x2e\xef\x7f\x47\x06\x51\x4c"
                "\xc6\x7e\x24\xf7\x29\x84\x9c\xd6\xb0\xd9\x4b\xd9\x0f\xc9\x34"
                "\x01\x9d\x92\xeb\xbc\x0a\x81\x03\x02\x00\x00\xa1\x79\xa0\x25"
                "\x80\x20\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54"
                "\x2d\x6a\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee"
                "\x93\x58\xeb\x4e\x81\x01\x09\xa3\x27\x80\x20\x3c\x73\x38\xcf"
                "\x23\xc6\x31\x53\x28\xc4\x27\xf8\x95\x87\x99\x83\x2d\x35\x3c"
                "\x03\x9b\xd1\xff\xff\x2e\x53\x20\xe9\x5e\x62\xb9\xb7\x81\x03"
                "\x01\x00\x00\xa4\x27\x80\x20\x41\x80\x08\xb2\x60\x74\x57\x6d"
                "\xac\xed\x74\x7f\x54\xdb\x96\x18\x91\x06\x0a\x95\xa1\x49\x17"
                "\xc7\x65\xe3\x94\xc8\x5e\x2c\x92\x20\x81\x03\x02\x00\x00"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\x40\xde\x6f\x75\xde\x2b\xb4\xe8\xf8\x3b\x34"
                "\x27\xb1\x15\xd7\xb3\x8e\x01\x64\x60\x97\xe9\x0d\x34\x20\xc2"
                "\x43\xb4\x53\xf8\xf1\x63\x81\x03\x04\x2c\x20\x82\x02\x03\xb8"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\xf9\x45\xd5\x9d\x4a\x2a\xe1\x52\xe7\xf0\xc2\xc4\xd1\x26\x9a"
                "\x8b\x08\x37\x18\x22\xbe\x6d\xd1\x93\xf9\x04\xb6\x44\x49\x91"
                "\xc5\xd4\x81\x03\x04\x28\x10\x82\x02\x03\xb8"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix6()
    {
        testcase("Prefix6");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** thresh2
        // **** Preim9Cond
        // **** Rsa10Cond
        // **** Ed11Cond
        // **** Thresh12Cond
        // **** preim3
        // **** thresh4
        // ***** Preim6Cond
        // ***** Rsa7Cond
        // ***** Ed8Cond
        // ***** preim5

        auto const preim3Preimage = "I am root"s;
        auto const preim3Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const preim5Preimage = "I am root"s;
        auto const preim5Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const thresh4Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim6CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim6Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim6CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa7CondConditionFingerprint =
            "\x6c\x7b\xea\x83\xa1\xf4\x82\x3d\x36\xe7\x6e\xae\x1a\xbc\xa0"
            "\xba\x90\x3d\x96\xc1\xe6\xad\x3a\x47\xa5\xcb\x88\xab\x3c\x5f"
            "\xcc\xd5"s;
        Condition const Rsa7Cond{Type::rsaSha256,
                                 65536,
                                 makeSlice(Rsa7CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const Ed8CondConditionFingerprint =
            "\xf1\x68\x96\xa6\x2e\xef\x7f\x47\x06\x51\x4c\xc6\x7e\x24\xf7"
            "\x29\x84\x9c\xd6\xb0\xd9\x4b\xd9\x0f\xc9\x34\x01\x9d\x92\xeb"
            "\xbc\x0a"s;
        Condition const Ed8Cond{Type::ed25519Sha256,
                                131072,
                                makeSlice(Ed8CondConditionFingerprint),
                                std::bitset<5>{0}};
        auto const thresh2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim9CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim9Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim9CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa10CondConditionFingerprint =
            "\x3c\x73\x38\xcf\x23\xc6\x31\x53\x28\xc4\x27\xf8\x95\x87\x99"
            "\x83\x2d\x35\x3c\x03\x9b\xd1\xff\xff\x2e\x53\x20\xe9\x5e\x62"
            "\xb9\xb7"s;
        Condition const Rsa10Cond{Type::rsaSha256,
                                  65536,
                                  makeSlice(Rsa10CondConditionFingerprint),
                                  std::bitset<5>{0}};
        auto const Ed11CondConditionFingerprint =
            "\x41\x80\x08\xb2\x60\x74\x57\x6d\xac\xed\x74\x7f\x54\xdb\x96"
            "\x18\x91\x06\x0a\x95\xa1\x49\x17\xc7\x65\xe3\x94\xc8\x5e\x2c"
            "\x92\x20"s;
        Condition const Ed11Cond{Type::ed25519Sha256,
                                 131072,
                                 makeSlice(Ed11CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const Thresh12CondConditionFingerprint =
            "\x2b\x40\xdc\x99\x90\xf5\xc1\xc1\x79\x66\x76\xa2\xc6\x2e\xb7"
            "\x46\xeb\x34\xa9\x67\x07\xb2\xe3\xd4\x31\x8e\x61\xbf\x80\x1a"
            "\x20\x4a"s;
        Condition const Thresh12Cond{
            Type::thresholdSha256,
            135168,
            makeSlice(Thresh12CondConditionFingerprint),
            std::bitset<5>{25}};
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto preim3 =
            std::make_unique<PreimageSha256>(makeSlice(preim3Preimage));
        auto preim5 =
            std::make_unique<PreimageSha256>(makeSlice(preim5Preimage));
        std::vector<std::unique_ptr<Fulfillment>> thresh4Subfulfillments;
        thresh4Subfulfillments.emplace_back(std::move(preim5));
        std::vector<Condition> thresh4Subconditions{
            {Preim6Cond, Rsa7Cond, Ed8Cond}};
        auto thresh4 = std::make_unique<ThresholdSha256>(
            std::move(thresh4Subfulfillments), std::move(thresh4Subconditions));
        std::vector<std::unique_ptr<Fulfillment>> thresh2Subfulfillments;
        thresh2Subfulfillments.emplace_back(std::move(preim3));
        thresh2Subfulfillments.emplace_back(std::move(thresh4));
        std::vector<Condition> thresh2Subconditions{
            {Preim9Cond, Rsa10Cond, Ed11Cond, Thresh12Cond}};
        auto thresh2 = std::make_unique<ThresholdSha256>(
            std::move(thresh2Subfulfillments), std::move(thresh2Subconditions));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(thresh2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x82\x01\x64\x80\x02\x50\x30\x81\x01\x0e\xa2\x82\x01\x59"
                "\xa1\x82\x01\x55\x80\x02\x50\x31\x81\x01\x0e\xa2\x82\x01\x4a"
                "\xa2\x82\x01\x46\xa0\x81\x9a\xa0\x0b\x80\x09\x49\x20\x61\x6d"
                "\x20\x72\x6f\x6f\x74\xa2\x81\x8a\xa0\x0d\xa0\x0b\x80\x09\x49"
                "\x20\x61\x6d\x20\x72\x6f\x6f\x74\xa1\x79\xa0\x25\x80\x20\x5d"
                "\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a\x5b"
                "\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58\xeb"
                "\x4e\x81\x01\x09\xa3\x27\x80\x20\x6c\x7b\xea\x83\xa1\xf4\x82"
                "\x3d\x36\xe7\x6e\xae\x1a\xbc\xa0\xba\x90\x3d\x96\xc1\xe6\xad"
                "\x3a\x47\xa5\xcb\x88\xab\x3c\x5f\xcc\xd5\x81\x03\x01\x00\x00"
                "\xa4\x27\x80\x20\xf1\x68\x96\xa6\x2e\xef\x7f\x47\x06\x51\x4c"
                "\xc6\x7e\x24\xf7\x29\x84\x9c\xd6\xb0\xd9\x4b\xd9\x0f\xc9\x34"
                "\x01\x9d\x92\xeb\xbc\x0a\x81\x03\x02\x00\x00\xa1\x81\xa6\xa0"
                "\x25\x80\x20\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e"
                "\x54\x2d\x6a\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53"
                "\xee\x93\x58\xeb\x4e\x81\x01\x09\xa2\x2b\x80\x20\x2b\x40\xdc"
                "\x99\x90\xf5\xc1\xc1\x79\x66\x76\xa2\xc6\x2e\xb7\x46\xeb\x34"
                "\xa9\x67\x07\xb2\xe3\xd4\x31\x8e\x61\xbf\x80\x1a\x20\x4a\x81"
                "\x03\x02\x10\x00\x82\x02\x03\x98\xa3\x27\x80\x20\x3c\x73\x38"
                "\xcf\x23\xc6\x31\x53\x28\xc4\x27\xf8\x95\x87\x99\x83\x2d\x35"
                "\x3c\x03\x9b\xd1\xff\xff\x2e\x53\x20\xe9\x5e\x62\xb9\xb7\x81"
                "\x03\x01\x00\x00\xa4\x27\x80\x20\x41\x80\x08\xb2\x60\x74\x57"
                "\x6d\xac\xed\x74\x7f\x54\xdb\x96\x18\x91\x06\x0a\x95\xa1\x49"
                "\x17\xc7\x65\xe3\x94\xc8\x5e\x2c\x92\x20\x81\x03\x02\x00\x00"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\xeb\x69\xcc\x9e\xe8\x3c\xa2\x2d\x67\x01\x1c"
                "\xbc\x37\xaf\x40\x9c\x33\x9d\x90\xb0\x72\x59\xa5\xbf\x7f\x12"
                "\x85\x81\x9f\x9f\x18\xb7\x81\x03\x04\x40\x20\x82\x02\x03\xb8"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\x30\xa5\x84\x21\xd7\x4f\x97\x61\x29\x91\x8e\xd2\x55\x65\x7b"
                "\xd5\xeb\x17\x43\x4f\x0a\x32\xd0\x76\xab\x85\xa7\x01\xd4\xd1"
                "\xdf\xc1\x81\x03\x04\x3c\x10\x82\x02\x03\xb8"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix7()
    {
        testcase("Prefix7");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** rsa1

        auto const rsa1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 256> const rsa1PublicKey{
            {0xc4, 0x1b, 0xc9, 0x7a, 0x96, 0x81, 0xce, 0x2e, 0xf2, 0x53, 0xc0,
             0xd4, 0xa8, 0xb9, 0xa8, 0x12, 0x92, 0x45, 0x06, 0xf1, 0xf4, 0xcd,
             0x27, 0x7d, 0xff, 0xc1, 0x65, 0x75, 0xae, 0xb7, 0xc4, 0x98, 0x53,
             0xd8, 0xfa, 0x6d, 0x86, 0x63, 0xd8, 0x4e, 0xf5, 0x20, 0xe2, 0x9e,
             0x96, 0x04, 0x36, 0x0c, 0x3f, 0xac, 0x7d, 0x09, 0x42, 0x11, 0x13,
             0x30, 0x2d, 0x7f, 0x60, 0x2c, 0xec, 0x2a, 0x34, 0xc3, 0xd8, 0xba,
             0x6d, 0x14, 0x75, 0x28, 0x56, 0xdc, 0x73, 0x6c, 0xb7, 0xd6, 0xba,
             0x8a, 0xa3, 0x9e, 0x09, 0x0e, 0xa4, 0xf3, 0x6b, 0x5d, 0x12, 0xc6,
             0xe4, 0xdd, 0x8c, 0xb1, 0x98, 0xcd, 0xde, 0xca, 0xad, 0xff, 0x86,
             0xb6, 0x06, 0x25, 0x9b, 0x71, 0x84, 0xa0, 0x9b, 0x19, 0x14, 0x88,
             0xd0, 0xc7, 0x55, 0x99, 0xe0, 0x1e, 0x0e, 0x39, 0x67, 0x74, 0xdf,
             0xf6, 0x29, 0xfa, 0x92, 0xb6, 0xbb, 0xe6, 0xe1, 0x1c, 0xd9, 0xee,
             0x65, 0xda, 0x13, 0xec, 0x50, 0x6a, 0x11, 0x7e, 0xae, 0xb4, 0xac,
             0x85, 0xa5, 0xc7, 0xcb, 0x43, 0x42, 0x36, 0x73, 0x34, 0x31, 0xb6,
             0x0b, 0x5e, 0xf0, 0x9e, 0x80, 0x49, 0xab, 0xc7, 0x79, 0xc5, 0xa7,
             0xe3, 0x16, 0x35, 0x3a, 0x48, 0xe6, 0xc0, 0x69, 0xe2, 0x70, 0x30,
             0x20, 0x74, 0x47, 0x3e, 0x3a, 0x51, 0x01, 0x21, 0x60, 0x15, 0x53,
             0x40, 0x68, 0x6e, 0xe7, 0x9f, 0x73, 0xb6, 0x98, 0x3b, 0x6e, 0x50,
             0xb8, 0xb2, 0xe7, 0x42, 0x90, 0x77, 0x61, 0xd4, 0x22, 0x6c, 0x0b,
             0x3d, 0x66, 0xe0, 0x1b, 0x7d, 0xb0, 0xa1, 0xa9, 0xa9, 0xea, 0x0e,
             0xf2, 0xab, 0xe0, 0x32, 0xf4, 0x49, 0x44, 0xcb, 0xae, 0x60, 0x1d,
             0xe1, 0xac, 0xd3, 0x34, 0x2b, 0x03, 0x97, 0x98, 0x2d, 0xda, 0xf8,
             0xe8, 0x0b, 0x81, 0x94, 0x98, 0x3a, 0xbd, 0x6d, 0x17, 0x18, 0x42,
             0x58, 0x0c, 0xa3}};
        std::array<std::uint8_t, 256> const rsa1Sig{
            {0x1d, 0x21, 0xbe, 0xf2, 0x61, 0x8e, 0x45, 0x86, 0x47, 0xd3, 0xc5,
             0x91, 0x1a, 0x8d, 0x47, 0xa6, 0xbf, 0xc8, 0x6e, 0x5e, 0x95, 0xc8,
             0x50, 0x6e, 0xb7, 0x44, 0xb0, 0x13, 0x9f, 0x0c, 0xde, 0x7d, 0x77,
             0xb4, 0x7b, 0x2e, 0x91, 0xb8, 0xe6, 0x2c, 0x85, 0xd8, 0xa5, 0xde,
             0xff, 0x28, 0x9d, 0x92, 0x33, 0x7a, 0x36, 0x46, 0xbb, 0x2b, 0x5f,
             0x20, 0xe0, 0x06, 0xd9, 0x57, 0x6f, 0xa2, 0xb4, 0x1c, 0xc3, 0xa8,
             0x02, 0x62, 0xfe, 0x93, 0x6c, 0x64, 0x57, 0x2b, 0x29, 0xb3, 0xff,
             0x95, 0xbf, 0x42, 0xb3, 0x5f, 0x1a, 0x32, 0xd6, 0x54, 0xec, 0xfe,
             0xf3, 0x68, 0xdc, 0xfe, 0x49, 0x2d, 0xf7, 0x0e, 0xc9, 0xcc, 0xa1,
             0x03, 0x19, 0xc7, 0x2e, 0xf1, 0x07, 0x2a, 0xb1, 0x2f, 0x3e, 0xf6,
             0x9c, 0x1b, 0x9c, 0x2c, 0x7a, 0xb0, 0x12, 0x1a, 0xfb, 0x80, 0x94,
             0x3b, 0x48, 0x44, 0x55, 0xc7, 0x8f, 0x0f, 0x34, 0x28, 0xcc, 0xd0,
             0x78, 0xa1, 0xa2, 0x57, 0x8d, 0xfe, 0x47, 0x8f, 0x3a, 0xb2, 0xa2,
             0x1f, 0x2d, 0xe4, 0x1d, 0x78, 0xf8, 0xb1, 0xb7, 0x1f, 0xaf, 0xc7,
             0xe0, 0xaa, 0xf8, 0xc2, 0xf8, 0x10, 0xaa, 0xc1, 0xad, 0x1d, 0x7b,
             0x74, 0x57, 0xbb, 0xe6, 0xd8, 0x40, 0xaf, 0x22, 0x17, 0x8b, 0x9d,
             0xa4, 0xee, 0xcf, 0x3c, 0x3e, 0x91, 0x6f, 0x99, 0xe7, 0xae, 0x96,
             0x43, 0xbf, 0x71, 0xfd, 0x36, 0xeb, 0x71, 0xcd, 0xc8, 0xf3, 0xae,
             0xa4, 0x29, 0xe7, 0x1e, 0xbd, 0xdd, 0x2a, 0x99, 0x10, 0x7f, 0x09,
             0xaf, 0x22, 0x50, 0x34, 0xff, 0x6c, 0xae, 0xda, 0xf0, 0xe8, 0xf5,
             0xe5, 0x8f, 0x9c, 0xa7, 0xe1, 0xec, 0x40, 0xc9, 0xfe, 0xe4, 0x39,
             0xa7, 0x27, 0xde, 0x4e, 0x01, 0xe6, 0xd5, 0xbb, 0xb7, 0x2b, 0x9f,
             0xf6, 0x3c, 0x4e, 0x29, 0x15, 0xc4, 0xf8, 0xf9, 0x6a, 0xf7, 0xe0,
             0x4c, 0xd2, 0x92}};
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto rsa1 = std::make_unique<RsaSha256>(
            makeSlice(rsa1PublicKey), makeSlice(rsa1Sig));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(rsa1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x82\x02\x17\x80\x02\x50\x30\x81\x01\x0e\xa2\x82\x02\x0c"
                "\xa3\x82\x02\x08\x80\x82\x01\x00\xc4\x1b\xc9\x7a\x96\x81\xce"
                "\x2e\xf2\x53\xc0\xd4\xa8\xb9\xa8\x12\x92\x45\x06\xf1\xf4\xcd"
                "\x27\x7d\xff\xc1\x65\x75\xae\xb7\xc4\x98\x53\xd8\xfa\x6d\x86"
                "\x63\xd8\x4e\xf5\x20\xe2\x9e\x96\x04\x36\x0c\x3f\xac\x7d\x09"
                "\x42\x11\x13\x30\x2d\x7f\x60\x2c\xec\x2a\x34\xc3\xd8\xba\x6d"
                "\x14\x75\x28\x56\xdc\x73\x6c\xb7\xd6\xba\x8a\xa3\x9e\x09\x0e"
                "\xa4\xf3\x6b\x5d\x12\xc6\xe4\xdd\x8c\xb1\x98\xcd\xde\xca\xad"
                "\xff\x86\xb6\x06\x25\x9b\x71\x84\xa0\x9b\x19\x14\x88\xd0\xc7"
                "\x55\x99\xe0\x1e\x0e\x39\x67\x74\xdf\xf6\x29\xfa\x92\xb6\xbb"
                "\xe6\xe1\x1c\xd9\xee\x65\xda\x13\xec\x50\x6a\x11\x7e\xae\xb4"
                "\xac\x85\xa5\xc7\xcb\x43\x42\x36\x73\x34\x31\xb6\x0b\x5e\xf0"
                "\x9e\x80\x49\xab\xc7\x79\xc5\xa7\xe3\x16\x35\x3a\x48\xe6\xc0"
                "\x69\xe2\x70\x30\x20\x74\x47\x3e\x3a\x51\x01\x21\x60\x15\x53"
                "\x40\x68\x6e\xe7\x9f\x73\xb6\x98\x3b\x6e\x50\xb8\xb2\xe7\x42"
                "\x90\x77\x61\xd4\x22\x6c\x0b\x3d\x66\xe0\x1b\x7d\xb0\xa1\xa9"
                "\xa9\xea\x0e\xf2\xab\xe0\x32\xf4\x49\x44\xcb\xae\x60\x1d\xe1"
                "\xac\xd3\x34\x2b\x03\x97\x98\x2d\xda\xf8\xe8\x0b\x81\x94\x98"
                "\x3a\xbd\x6d\x17\x18\x42\x58\x0c\xa3\x81\x82\x01\x00\x1d\x21"
                "\xbe\xf2\x61\x8e\x45\x86\x47\xd3\xc5\x91\x1a\x8d\x47\xa6\xbf"
                "\xc8\x6e\x5e\x95\xc8\x50\x6e\xb7\x44\xb0\x13\x9f\x0c\xde\x7d"
                "\x77\xb4\x7b\x2e\x91\xb8\xe6\x2c\x85\xd8\xa5\xde\xff\x28\x9d"
                "\x92\x33\x7a\x36\x46\xbb\x2b\x5f\x20\xe0\x06\xd9\x57\x6f\xa2"
                "\xb4\x1c\xc3\xa8\x02\x62\xfe\x93\x6c\x64\x57\x2b\x29\xb3\xff"
                "\x95\xbf\x42\xb3\x5f\x1a\x32\xd6\x54\xec\xfe\xf3\x68\xdc\xfe"
                "\x49\x2d\xf7\x0e\xc9\xcc\xa1\x03\x19\xc7\x2e\xf1\x07\x2a\xb1"
                "\x2f\x3e\xf6\x9c\x1b\x9c\x2c\x7a\xb0\x12\x1a\xfb\x80\x94\x3b"
                "\x48\x44\x55\xc7\x8f\x0f\x34\x28\xcc\xd0\x78\xa1\xa2\x57\x8d"
                "\xfe\x47\x8f\x3a\xb2\xa2\x1f\x2d\xe4\x1d\x78\xf8\xb1\xb7\x1f"
                "\xaf\xc7\xe0\xaa\xf8\xc2\xf8\x10\xaa\xc1\xad\x1d\x7b\x74\x57"
                "\xbb\xe6\xd8\x40\xaf\x22\x17\x8b\x9d\xa4\xee\xcf\x3c\x3e\x91"
                "\x6f\x99\xe7\xae\x96\x43\xbf\x71\xfd\x36\xeb\x71\xcd\xc8\xf3"
                "\xae\xa4\x29\xe7\x1e\xbd\xdd\x2a\x99\x10\x7f\x09\xaf\x22\x50"
                "\x34\xff\x6c\xae\xda\xf0\xe8\xf5\xe5\x8f\x9c\xa7\xe1\xec\x40"
                "\xc9\xfe\xe4\x39\xa7\x27\xde\x4e\x01\xe6\xd5\xbb\xb7\x2b\x9f"
                "\xf6\x3c\x4e\x29\x15\xc4\xf8\xf9\x6a\xf7\xe0\x4c\xd2\x92"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\x4d\x51\x18\x04\xa1\x5d\x3a\xf4\x26\xc3\x0a"
                "\x98\x43\x35\xe4\xf5\xa0\x9d\x26\x5a\xec\xe7\x5e\xfe\x13\x74"
                "\x61\x99\xb6\x92\xb9\x83\x81\x03\x01\x04\x10\x82\x02\x04\x10"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x32\x80\x02\x50\x30\x81\x01\x0e\xa2\x29\xa3\x27\x80\x20"
                "\x23\xc9\xfd\x99\xdc\xf3\x74\x06\xe3\x11\x57\x5e\x7a\x90\x4b"
                "\x0d\x0c\x36\xa7\x8d\x25\xbc\xd5\x5f\x0d\x1f\x25\x08\x2e\x4f"
                "\xca\x1a\x81\x03\x01\x00\x00"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix8()
    {
        testcase("Prefix8");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** rsa2

        auto const rsa2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 256> const rsa2PublicKey{
            {0xba, 0x2c, 0x3b, 0x50, 0xb6, 0xbf, 0xf9, 0x0f, 0x1d, 0xd7, 0x32,
             0x4c, 0x01, 0x5f, 0xff, 0x2f, 0x2a, 0xf6, 0x33, 0xd0, 0xfb, 0xea,
             0x1f, 0xa4, 0xf2, 0x2d, 0x22, 0x8a, 0x19, 0x95, 0xa9, 0x17, 0xb7,
             0x4f, 0x17, 0xcf, 0x55, 0xcd, 0x1a, 0x3a, 0x5f, 0x07, 0x73, 0xcc,
             0xaa, 0x21, 0x70, 0x64, 0xb3, 0xa0, 0xf4, 0xb7, 0x30, 0xa3, 0x82,
             0x37, 0x93, 0xc6, 0x59, 0xde, 0x1b, 0xa1, 0x16, 0x90, 0x5a, 0x1a,
             0xf6, 0x73, 0xab, 0x92, 0xc8, 0x2f, 0xf4, 0x6f, 0x5c, 0xf2, 0x22,
             0x1d, 0x30, 0xf8, 0x03, 0xd8, 0x9b, 0x5f, 0x73, 0x72, 0x8e, 0x5f,
             0xd5, 0x37, 0x4b, 0x43, 0xda, 0xfe, 0x84, 0x21, 0x67, 0xe8, 0xe3,
             0xd7, 0x91, 0x3f, 0x24, 0x1d, 0xfb, 0x1f, 0x12, 0x6e, 0xcb, 0xfc,
             0xb7, 0x5b, 0x0a, 0x35, 0x73, 0x3b, 0xce, 0x44, 0x34, 0x8e, 0xcd,
             0x53, 0xa4, 0xcf, 0xa7, 0x63, 0x73, 0xcd, 0x31, 0x0f, 0xe0, 0x75,
             0x8d, 0xe4, 0xa9, 0xdc, 0xfe, 0xf0, 0xc9, 0x3d, 0x26, 0xaf, 0xbf,
             0x7b, 0x0f, 0x0e, 0x17, 0xb9, 0xd0, 0x4a, 0x32, 0x80, 0x64, 0x6b,
             0x54, 0x73, 0x5a, 0x50, 0xc7, 0x31, 0x59, 0xf9, 0x73, 0x72, 0xa5,
             0x79, 0xba, 0xdb, 0xa1, 0x14, 0x8d, 0x77, 0x67, 0x3e, 0xc0, 0x5b,
             0xec, 0x6f, 0x0b, 0xf7, 0xc5, 0xee, 0x5a, 0xa6, 0x8d, 0x49, 0x63,
             0x81, 0xbb, 0xd1, 0xf9, 0x9e, 0xbb, 0xed, 0xb2, 0xa9, 0x18, 0x60,
             0xa7, 0xee, 0xeb, 0x30, 0xa1, 0x92, 0x93, 0xe8, 0xd8, 0x34, 0x9e,
             0xac, 0xd6, 0x23, 0xfc, 0x7f, 0xcb, 0xe7, 0xfe, 0xa7, 0xe6, 0x42,
             0xac, 0x77, 0x11, 0xc0, 0x67, 0x77, 0xd1, 0xaa, 0x5e, 0xed, 0x3b,
             0xd5, 0xa5, 0x8d, 0x34, 0x7c, 0xd9, 0x57, 0x44, 0xa7, 0xc5, 0x44,
             0x2e, 0x1e, 0xe7, 0x63, 0xd8, 0x53, 0x1b, 0x9a, 0xd9, 0x67, 0x02,
             0x13, 0x32, 0x61}};
        std::array<std::uint8_t, 256> const rsa2Sig{
            {0x5d, 0xe1, 0x49, 0x82, 0xdc, 0x56, 0x4d, 0x33, 0x81, 0x4c, 0x72,
             0xd0, 0x01, 0x54, 0xdd, 0xc9, 0x7e, 0x80, 0x1f, 0x4d, 0xb0, 0x6f,
             0xbd, 0x83, 0x74, 0xf4, 0x4b, 0x88, 0xf7, 0xc9, 0x44, 0xaa, 0x93,
             0x3b, 0x43, 0x9e, 0x09, 0x03, 0x64, 0x22, 0xaa, 0xb6, 0x2e, 0x47,
             0x05, 0x9c, 0x33, 0x2f, 0x65, 0x95, 0x35, 0xa1, 0xed, 0x91, 0x94,
             0xfc, 0x02, 0xff, 0xaa, 0x93, 0x76, 0x3b, 0xad, 0x4f, 0xee, 0xf3,
             0xd7, 0x98, 0x0e, 0x8c, 0x2d, 0xb5, 0x0f, 0xf3, 0x81, 0x54, 0xe8,
             0x05, 0x11, 0x13, 0xbe, 0xb8, 0xc1, 0x82, 0x01, 0x13, 0x6a, 0x7f,
             0x78, 0x63, 0x0d, 0x4c, 0x3d, 0x77, 0x59, 0x1b, 0x38, 0x83, 0x27,
             0x58, 0x56, 0xa7, 0x39, 0xaa, 0x48, 0x46, 0xc2, 0xe8, 0x41, 0xfb,
             0x10, 0xab, 0x48, 0x6d, 0x2c, 0xd0, 0xd1, 0x63, 0x47, 0xa7, 0x22,
             0x60, 0xaf, 0x69, 0xfc, 0xc7, 0x8d, 0x7d, 0xe6, 0x6d, 0xeb, 0x0b,
             0x51, 0xc0, 0xfc, 0x41, 0xe8, 0xc0, 0x42, 0x1f, 0x58, 0x09, 0x53,
             0xa9, 0xe2, 0xe7, 0x13, 0x05, 0x89, 0xb9, 0xee, 0x4e, 0x38, 0x3c,
             0xba, 0x0b, 0xdc, 0x0d, 0xbc, 0x76, 0x57, 0xa9, 0xa1, 0x83, 0x7b,
             0xcf, 0x4e, 0x3e, 0x4a, 0xd3, 0x77, 0x8d, 0xd6, 0x56, 0x94, 0xab,
             0x2f, 0xd1, 0xc8, 0x6e, 0x34, 0x88, 0x79, 0x1e, 0x02, 0x41, 0x3e,
             0x84, 0x15, 0xeb, 0x11, 0x41, 0xd4, 0x95, 0xa5, 0x0c, 0x0d, 0x3e,
             0x4a, 0x57, 0x0a, 0x3a, 0xf3, 0x9a, 0x01, 0x8c, 0x19, 0xbb, 0x6d,
             0xbf, 0x26, 0x80, 0x65, 0x1f, 0x3e, 0x87, 0xc4, 0x93, 0x3d, 0x8c,
             0x17, 0xce, 0xaf, 0x54, 0x42, 0x2f, 0x66, 0x10, 0x8c, 0x71, 0x29,
             0x40, 0x53, 0xf7, 0x74, 0x39, 0xee, 0x32, 0xa9, 0x61, 0x98, 0xc6,
             0x5d, 0x2a, 0x4e, 0x7d, 0xbf, 0x9c, 0x3a, 0x06, 0xba, 0x46, 0xdc,
             0xd4, 0x9d, 0xac}};
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto rsa2 = std::make_unique<RsaSha256>(
            makeSlice(rsa2PublicKey), makeSlice(rsa2Sig));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(rsa2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x82\x02\x26\x80\x02\x50\x30\x81\x01\x0e\xa2\x82\x02\x1b"
                "\xa1\x82\x02\x17\x80\x02\x50\x31\x81\x01\x0e\xa2\x82\x02\x0c"
                "\xa3\x82\x02\x08\x80\x82\x01\x00\xba\x2c\x3b\x50\xb6\xbf\xf9"
                "\x0f\x1d\xd7\x32\x4c\x01\x5f\xff\x2f\x2a\xf6\x33\xd0\xfb\xea"
                "\x1f\xa4\xf2\x2d\x22\x8a\x19\x95\xa9\x17\xb7\x4f\x17\xcf\x55"
                "\xcd\x1a\x3a\x5f\x07\x73\xcc\xaa\x21\x70\x64\xb3\xa0\xf4\xb7"
                "\x30\xa3\x82\x37\x93\xc6\x59\xde\x1b\xa1\x16\x90\x5a\x1a\xf6"
                "\x73\xab\x92\xc8\x2f\xf4\x6f\x5c\xf2\x22\x1d\x30\xf8\x03\xd8"
                "\x9b\x5f\x73\x72\x8e\x5f\xd5\x37\x4b\x43\xda\xfe\x84\x21\x67"
                "\xe8\xe3\xd7\x91\x3f\x24\x1d\xfb\x1f\x12\x6e\xcb\xfc\xb7\x5b"
                "\x0a\x35\x73\x3b\xce\x44\x34\x8e\xcd\x53\xa4\xcf\xa7\x63\x73"
                "\xcd\x31\x0f\xe0\x75\x8d\xe4\xa9\xdc\xfe\xf0\xc9\x3d\x26\xaf"
                "\xbf\x7b\x0f\x0e\x17\xb9\xd0\x4a\x32\x80\x64\x6b\x54\x73\x5a"
                "\x50\xc7\x31\x59\xf9\x73\x72\xa5\x79\xba\xdb\xa1\x14\x8d\x77"
                "\x67\x3e\xc0\x5b\xec\x6f\x0b\xf7\xc5\xee\x5a\xa6\x8d\x49\x63"
                "\x81\xbb\xd1\xf9\x9e\xbb\xed\xb2\xa9\x18\x60\xa7\xee\xeb\x30"
                "\xa1\x92\x93\xe8\xd8\x34\x9e\xac\xd6\x23\xfc\x7f\xcb\xe7\xfe"
                "\xa7\xe6\x42\xac\x77\x11\xc0\x67\x77\xd1\xaa\x5e\xed\x3b\xd5"
                "\xa5\x8d\x34\x7c\xd9\x57\x44\xa7\xc5\x44\x2e\x1e\xe7\x63\xd8"
                "\x53\x1b\x9a\xd9\x67\x02\x13\x32\x61\x81\x82\x01\x00\x5d\xe1"
                "\x49\x82\xdc\x56\x4d\x33\x81\x4c\x72\xd0\x01\x54\xdd\xc9\x7e"
                "\x80\x1f\x4d\xb0\x6f\xbd\x83\x74\xf4\x4b\x88\xf7\xc9\x44\xaa"
                "\x93\x3b\x43\x9e\x09\x03\x64\x22\xaa\xb6\x2e\x47\x05\x9c\x33"
                "\x2f\x65\x95\x35\xa1\xed\x91\x94\xfc\x02\xff\xaa\x93\x76\x3b"
                "\xad\x4f\xee\xf3\xd7\x98\x0e\x8c\x2d\xb5\x0f\xf3\x81\x54\xe8"
                "\x05\x11\x13\xbe\xb8\xc1\x82\x01\x13\x6a\x7f\x78\x63\x0d\x4c"
                "\x3d\x77\x59\x1b\x38\x83\x27\x58\x56\xa7\x39\xaa\x48\x46\xc2"
                "\xe8\x41\xfb\x10\xab\x48\x6d\x2c\xd0\xd1\x63\x47\xa7\x22\x60"
                "\xaf\x69\xfc\xc7\x8d\x7d\xe6\x6d\xeb\x0b\x51\xc0\xfc\x41\xe8"
                "\xc0\x42\x1f\x58\x09\x53\xa9\xe2\xe7\x13\x05\x89\xb9\xee\x4e"
                "\x38\x3c\xba\x0b\xdc\x0d\xbc\x76\x57\xa9\xa1\x83\x7b\xcf\x4e"
                "\x3e\x4a\xd3\x77\x8d\xd6\x56\x94\xab\x2f\xd1\xc8\x6e\x34\x88"
                "\x79\x1e\x02\x41\x3e\x84\x15\xeb\x11\x41\xd4\x95\xa5\x0c\x0d"
                "\x3e\x4a\x57\x0a\x3a\xf3\x9a\x01\x8c\x19\xbb\x6d\xbf\x26\x80"
                "\x65\x1f\x3e\x87\xc4\x93\x3d\x8c\x17\xce\xaf\x54\x42\x2f\x66"
                "\x10\x8c\x71\x29\x40\x53\xf7\x74\x39\xee\x32\xa9\x61\x98\xc6"
                "\x5d\x2a\x4e\x7d\xbf\x9c\x3a\x06\xba\x46\xdc\xd4\x9d\xac"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\x11\x64\x40\xd9\xd3\x5e\x9b\xe8\x11\x62\x00"
                "\x00\x46\x04\xf3\x3d\xbc\x44\x83\x15\x64\xae\xb8\x83\xa4\x84"
                "\x9d\xed\x01\xfe\x6b\x04\x81\x03\x01\x08\x20\x82\x02\x04\x10"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\x1d\x7f\xae\x40\x60\x6d\x72\x35\xc5\xea\xe7\x2f\x5b\xeb\xaa"
                "\xb2\x87\x12\xe2\x89\x91\x28\x8e\xc9\xc2\xb1\x12\x1a\xe9\xa8"
                "\x76\xc8\x81\x03\x01\x04\x10\x82\x02\x04\x10"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix9()
    {
        testcase("Prefix9");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** prefix2
        // **** rsa3

        auto const rsa3Msg = "P2P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 256> const rsa3PublicKey{
            {0xbd, 0xd1, 0xc7, 0xf0, 0xb0, 0x3a, 0xa5, 0x5b, 0x3e, 0x49, 0x8d,
             0x4e, 0x00, 0x54, 0x89, 0xb9, 0x89, 0xcd, 0x4b, 0x43, 0xde, 0x59,
             0xf6, 0x7a, 0x67, 0x5c, 0x3a, 0xc6, 0xcf, 0x82, 0x3f, 0x35, 0x9c,
             0xcc, 0xda, 0xcd, 0xd3, 0x97, 0x86, 0x5b, 0xe9, 0xf6, 0x05, 0x55,
             0x0b, 0x26, 0xef, 0x1e, 0x88, 0xd5, 0xb6, 0xba, 0x14, 0x0a, 0xb2,
             0x76, 0xb9, 0xb3, 0x46, 0x0c, 0xc0, 0x80, 0x17, 0x13, 0x68, 0x23,
             0xdc, 0xec, 0x10, 0x18, 0xfc, 0xaa, 0xbe, 0xb3, 0xc4, 0xc7, 0xa9,
             0x84, 0xa6, 0x4e, 0x5c, 0x08, 0x6b, 0x7b, 0x4c, 0x81, 0x91, 0x79,
             0x5d, 0x90, 0x06, 0x15, 0xbb, 0x76, 0x2f, 0x5c, 0x53, 0x60, 0x0f,
             0xac, 0xf3, 0x7c, 0x49, 0xc5, 0x47, 0xec, 0xb3, 0xda, 0x93, 0x87,
             0xc1, 0xb9, 0xcf, 0x2c, 0xb5, 0xf0, 0x85, 0xad, 0xb4, 0x38, 0x67,
             0x88, 0xda, 0x3d, 0xfa, 0x01, 0xb7, 0x54, 0xd9, 0x41, 0x0b, 0x7b,
             0x8a, 0x09, 0xe0, 0x84, 0x7d, 0xbb, 0x89, 0xb2, 0xfc, 0x0b, 0x70,
             0x36, 0x93, 0x56, 0x62, 0xcc, 0xb4, 0xfc, 0xf9, 0x1f, 0x37, 0x92,
             0x9b, 0x3a, 0x4e, 0x7c, 0xad, 0x4b, 0xa6, 0x76, 0x6f, 0xda, 0xc4,
             0x2f, 0x83, 0x53, 0xbd, 0x93, 0xa9, 0x76, 0x89, 0x53, 0xe1, 0x4d,
             0xee, 0x27, 0x11, 0x6f, 0xbc, 0x21, 0xad, 0x42, 0x9f, 0x29, 0xf6,
             0x03, 0xdd, 0xec, 0xfa, 0xa1, 0x78, 0xd2, 0xde, 0x29, 0x2e, 0xd8,
             0x3a, 0x7f, 0xe9, 0x9b, 0x5d, 0xeb, 0x37, 0xb8, 0xb0, 0xa0, 0x66,
             0x3f, 0x02, 0xcd, 0x2a, 0x6e, 0xd3, 0x1c, 0xa5, 0x65, 0xdc, 0x73,
             0xbe, 0x93, 0x54, 0x9a, 0x2b, 0xf8, 0x32, 0x8b, 0xe8, 0xce, 0x9a,
             0x59, 0xd0, 0x05, 0xeb, 0xbb, 0xac, 0xfc, 0x4c, 0x4b, 0x2e, 0xac,
             0x2a, 0xc3, 0x0f, 0x0a, 0xd7, 0x46, 0xaf, 0xfd, 0x22, 0x0d, 0x0d,
             0x54, 0xcc, 0x2f}};
        std::array<std::uint8_t, 256> const rsa3Sig{
            {0x1a, 0x83, 0xd1, 0x1b, 0x34, 0x73, 0xd7, 0x36, 0xe3, 0x93, 0x2a,
             0x97, 0x38, 0x87, 0xfa, 0x5d, 0x59, 0x3d, 0x3d, 0xde, 0x16, 0xae,
             0x77, 0x78, 0xea, 0x12, 0xd7, 0x83, 0xc9, 0xa3, 0x25, 0x4f, 0xfb,
             0xc3, 0xa6, 0x03, 0x9d, 0x97, 0xa0, 0x94, 0x49, 0xf0, 0x26, 0x0f,
             0x31, 0x73, 0xcd, 0xf8, 0xb6, 0x90, 0x21, 0xa9, 0xce, 0xfb, 0x9a,
             0x3b, 0xdb, 0xba, 0x2c, 0x6d, 0xef, 0xdf, 0xcb, 0x81, 0x80, 0xc1,
             0xd5, 0xff, 0x62, 0x39, 0xb1, 0x7f, 0x9d, 0xb3, 0x5a, 0x47, 0x1b,
             0xd6, 0x8f, 0x5a, 0xa9, 0x9a, 0x9b, 0xea, 0x59, 0xd8, 0xe8, 0x8a,
             0x51, 0xb3, 0xc5, 0x65, 0x5a, 0x70, 0x08, 0x92, 0xbd, 0xa1, 0xb7,
             0x9c, 0x77, 0x55, 0xbd, 0x38, 0xfc, 0x09, 0x58, 0xe0, 0x05, 0x65,
             0x55, 0x39, 0x92, 0x7d, 0xcf, 0xd4, 0x9b, 0xeb, 0x14, 0xa5, 0xaf,
             0xf7, 0x99, 0x14, 0x62, 0x6e, 0x7c, 0x6b, 0xd6, 0xd1, 0xc6, 0xae,
             0x41, 0x88, 0x3c, 0x22, 0xb5, 0x87, 0x0d, 0xbe, 0xc8, 0x93, 0x62,
             0x37, 0xb9, 0x23, 0x57, 0x5a, 0xd1, 0x9b, 0x4f, 0x51, 0xcf, 0x8e,
             0xa7, 0xaf, 0x9f, 0x68, 0x90, 0xdb, 0xc6, 0x18, 0xdc, 0x7c, 0xa2,
             0xd9, 0xd2, 0xca, 0x19, 0x23, 0xe7, 0xcd, 0x62, 0x2a, 0x12, 0x46,
             0x13, 0x71, 0x40, 0x0a, 0x4f, 0x4f, 0xef, 0xb8, 0x32, 0x7b, 0xd1,
             0x27, 0x6e, 0xcb, 0x66, 0xb6, 0x5b, 0xeb, 0x58, 0x4b, 0x3f, 0x21,
             0xf8, 0x6e, 0x1d, 0xb6, 0x73, 0xdf, 0x22, 0x00, 0xfb, 0x44, 0x32,
             0xdf, 0x39, 0x80, 0x85, 0x3c, 0x0e, 0x1c, 0x02, 0x1e, 0x8c, 0x24,
             0xb6, 0xe2, 0xba, 0x08, 0xb1, 0xfa, 0x53, 0x65, 0x84, 0x70, 0xae,
             0x23, 0xda, 0xfd, 0xe0, 0xbb, 0x08, 0x29, 0xaf, 0x19, 0xe4, 0xfa,
             0x87, 0x36, 0xbb, 0x89, 0x74, 0xe8, 0xe3, 0xbb, 0x3b, 0x97, 0x1c,
             0x52, 0x56, 0x00}};
        auto const prefix2Prefix = "P2"s;
        auto const prefix2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix2MaxMsgLength = 14;
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto rsa3 = std::make_unique<RsaSha256>(
            makeSlice(rsa3PublicKey), makeSlice(rsa3Sig));
        auto prefix2 = std::make_unique<PrefixSha256>(
            makeSlice(prefix2Prefix), prefix2MaxMsgLength, std::move(rsa3));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(prefix2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x82\x02\x35\x80\x02\x50\x30\x81\x01\x0e\xa2\x82\x02\x2a"
                "\xa1\x82\x02\x26\x80\x02\x50\x31\x81\x01\x0e\xa2\x82\x02\x1b"
                "\xa1\x82\x02\x17\x80\x02\x50\x32\x81\x01\x0e\xa2\x82\x02\x0c"
                "\xa3\x82\x02\x08\x80\x82\x01\x00\xbd\xd1\xc7\xf0\xb0\x3a\xa5"
                "\x5b\x3e\x49\x8d\x4e\x00\x54\x89\xb9\x89\xcd\x4b\x43\xde\x59"
                "\xf6\x7a\x67\x5c\x3a\xc6\xcf\x82\x3f\x35\x9c\xcc\xda\xcd\xd3"
                "\x97\x86\x5b\xe9\xf6\x05\x55\x0b\x26\xef\x1e\x88\xd5\xb6\xba"
                "\x14\x0a\xb2\x76\xb9\xb3\x46\x0c\xc0\x80\x17\x13\x68\x23\xdc"
                "\xec\x10\x18\xfc\xaa\xbe\xb3\xc4\xc7\xa9\x84\xa6\x4e\x5c\x08"
                "\x6b\x7b\x4c\x81\x91\x79\x5d\x90\x06\x15\xbb\x76\x2f\x5c\x53"
                "\x60\x0f\xac\xf3\x7c\x49\xc5\x47\xec\xb3\xda\x93\x87\xc1\xb9"
                "\xcf\x2c\xb5\xf0\x85\xad\xb4\x38\x67\x88\xda\x3d\xfa\x01\xb7"
                "\x54\xd9\x41\x0b\x7b\x8a\x09\xe0\x84\x7d\xbb\x89\xb2\xfc\x0b"
                "\x70\x36\x93\x56\x62\xcc\xb4\xfc\xf9\x1f\x37\x92\x9b\x3a\x4e"
                "\x7c\xad\x4b\xa6\x76\x6f\xda\xc4\x2f\x83\x53\xbd\x93\xa9\x76"
                "\x89\x53\xe1\x4d\xee\x27\x11\x6f\xbc\x21\xad\x42\x9f\x29\xf6"
                "\x03\xdd\xec\xfa\xa1\x78\xd2\xde\x29\x2e\xd8\x3a\x7f\xe9\x9b"
                "\x5d\xeb\x37\xb8\xb0\xa0\x66\x3f\x02\xcd\x2a\x6e\xd3\x1c\xa5"
                "\x65\xdc\x73\xbe\x93\x54\x9a\x2b\xf8\x32\x8b\xe8\xce\x9a\x59"
                "\xd0\x05\xeb\xbb\xac\xfc\x4c\x4b\x2e\xac\x2a\xc3\x0f\x0a\xd7"
                "\x46\xaf\xfd\x22\x0d\x0d\x54\xcc\x2f\x81\x82\x01\x00\x1a\x83"
                "\xd1\x1b\x34\x73\xd7\x36\xe3\x93\x2a\x97\x38\x87\xfa\x5d\x59"
                "\x3d\x3d\xde\x16\xae\x77\x78\xea\x12\xd7\x83\xc9\xa3\x25\x4f"
                "\xfb\xc3\xa6\x03\x9d\x97\xa0\x94\x49\xf0\x26\x0f\x31\x73\xcd"
                "\xf8\xb6\x90\x21\xa9\xce\xfb\x9a\x3b\xdb\xba\x2c\x6d\xef\xdf"
                "\xcb\x81\x80\xc1\xd5\xff\x62\x39\xb1\x7f\x9d\xb3\x5a\x47\x1b"
                "\xd6\x8f\x5a\xa9\x9a\x9b\xea\x59\xd8\xe8\x8a\x51\xb3\xc5\x65"
                "\x5a\x70\x08\x92\xbd\xa1\xb7\x9c\x77\x55\xbd\x38\xfc\x09\x58"
                "\xe0\x05\x65\x55\x39\x92\x7d\xcf\xd4\x9b\xeb\x14\xa5\xaf\xf7"
                "\x99\x14\x62\x6e\x7c\x6b\xd6\xd1\xc6\xae\x41\x88\x3c\x22\xb5"
                "\x87\x0d\xbe\xc8\x93\x62\x37\xb9\x23\x57\x5a\xd1\x9b\x4f\x51"
                "\xcf\x8e\xa7\xaf\x9f\x68\x90\xdb\xc6\x18\xdc\x7c\xa2\xd9\xd2"
                "\xca\x19\x23\xe7\xcd\x62\x2a\x12\x46\x13\x71\x40\x0a\x4f\x4f"
                "\xef\xb8\x32\x7b\xd1\x27\x6e\xcb\x66\xb6\x5b\xeb\x58\x4b\x3f"
                "\x21\xf8\x6e\x1d\xb6\x73\xdf\x22\x00\xfb\x44\x32\xdf\x39\x80"
                "\x85\x3c\x0e\x1c\x02\x1e\x8c\x24\xb6\xe2\xba\x08\xb1\xfa\x53"
                "\x65\x84\x70\xae\x23\xda\xfd\xe0\xbb\x08\x29\xaf\x19\xe4\xfa"
                "\x87\x36\xbb\x89\x74\xe8\xe3\xbb\x3b\x97\x1c\x52\x56\x00"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\x36\x2d\x6a\xce\xb6\x9b\x67\xb0\x32\x85\x53"
                "\x05\x7b\x69\xb9\x21\x5e\x96\xd1\x42\x22\x8e\xfa\x00\x3f\x32"
                "\x55\x21\x7b\xd6\xe1\xf9\x81\x03\x01\x0c\x30\x82\x02\x04\x10"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\xf7\x5d\xa2\x8c\x5a\x93\x5f\x41\xca\x1c\xb4\xb3\x8c\xae\x0f"
                "\x3d\x22\x58\x1e\xb3\x37\x76\x7d\xf6\xce\x6b\xdc\x5d\x16\x57"
                "\xf4\xc6\x81\x03\x01\x08\x20\x82\x02\x04\x10"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix10()
    {
        testcase("Prefix10");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** thresh2
        // **** rsa3

        auto const rsa3Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 256> const rsa3PublicKey{
            {0xbd, 0xd1, 0xc7, 0xf0, 0xb0, 0x3a, 0xa5, 0x5b, 0x3e, 0x49, 0x8d,
             0x4e, 0x00, 0x54, 0x89, 0xb9, 0x89, 0xcd, 0x4b, 0x43, 0xde, 0x59,
             0xf6, 0x7a, 0x67, 0x5c, 0x3a, 0xc6, 0xcf, 0x82, 0x3f, 0x35, 0x9c,
             0xcc, 0xda, 0xcd, 0xd3, 0x97, 0x86, 0x5b, 0xe9, 0xf6, 0x05, 0x55,
             0x0b, 0x26, 0xef, 0x1e, 0x88, 0xd5, 0xb6, 0xba, 0x14, 0x0a, 0xb2,
             0x76, 0xb9, 0xb3, 0x46, 0x0c, 0xc0, 0x80, 0x17, 0x13, 0x68, 0x23,
             0xdc, 0xec, 0x10, 0x18, 0xfc, 0xaa, 0xbe, 0xb3, 0xc4, 0xc7, 0xa9,
             0x84, 0xa6, 0x4e, 0x5c, 0x08, 0x6b, 0x7b, 0x4c, 0x81, 0x91, 0x79,
             0x5d, 0x90, 0x06, 0x15, 0xbb, 0x76, 0x2f, 0x5c, 0x53, 0x60, 0x0f,
             0xac, 0xf3, 0x7c, 0x49, 0xc5, 0x47, 0xec, 0xb3, 0xda, 0x93, 0x87,
             0xc1, 0xb9, 0xcf, 0x2c, 0xb5, 0xf0, 0x85, 0xad, 0xb4, 0x38, 0x67,
             0x88, 0xda, 0x3d, 0xfa, 0x01, 0xb7, 0x54, 0xd9, 0x41, 0x0b, 0x7b,
             0x8a, 0x09, 0xe0, 0x84, 0x7d, 0xbb, 0x89, 0xb2, 0xfc, 0x0b, 0x70,
             0x36, 0x93, 0x56, 0x62, 0xcc, 0xb4, 0xfc, 0xf9, 0x1f, 0x37, 0x92,
             0x9b, 0x3a, 0x4e, 0x7c, 0xad, 0x4b, 0xa6, 0x76, 0x6f, 0xda, 0xc4,
             0x2f, 0x83, 0x53, 0xbd, 0x93, 0xa9, 0x76, 0x89, 0x53, 0xe1, 0x4d,
             0xee, 0x27, 0x11, 0x6f, 0xbc, 0x21, 0xad, 0x42, 0x9f, 0x29, 0xf6,
             0x03, 0xdd, 0xec, 0xfa, 0xa1, 0x78, 0xd2, 0xde, 0x29, 0x2e, 0xd8,
             0x3a, 0x7f, 0xe9, 0x9b, 0x5d, 0xeb, 0x37, 0xb8, 0xb0, 0xa0, 0x66,
             0x3f, 0x02, 0xcd, 0x2a, 0x6e, 0xd3, 0x1c, 0xa5, 0x65, 0xdc, 0x73,
             0xbe, 0x93, 0x54, 0x9a, 0x2b, 0xf8, 0x32, 0x8b, 0xe8, 0xce, 0x9a,
             0x59, 0xd0, 0x05, 0xeb, 0xbb, 0xac, 0xfc, 0x4c, 0x4b, 0x2e, 0xac,
             0x2a, 0xc3, 0x0f, 0x0a, 0xd7, 0x46, 0xaf, 0xfd, 0x22, 0x0d, 0x0d,
             0x54, 0xcc, 0x2f}};
        std::array<std::uint8_t, 256> const rsa3Sig{
            {0x1a, 0xf2, 0xdd, 0xd7, 0xcd, 0x23, 0x5d, 0xbf, 0x39, 0x59, 0xa0,
             0xbf, 0x44, 0x03, 0x0b, 0xbd, 0xbf, 0xed, 0xfc, 0xb3, 0x7f, 0x7f,
             0x05, 0x16, 0x74, 0xbc, 0x9a, 0x55, 0xaa, 0xa7, 0x76, 0x5f, 0xd2,
             0x7a, 0x67, 0x7c, 0x1a, 0xa9, 0x7a, 0x4d, 0x24, 0xd1, 0xfa, 0xf0,
             0x8e, 0x4b, 0x18, 0xd5, 0xdc, 0x4c, 0x2c, 0x38, 0x87, 0x10, 0x39,
             0x8f, 0x07, 0xab, 0x3e, 0xcc, 0x6d, 0x47, 0x71, 0xfb, 0x8c, 0x25,
             0x16, 0x1a, 0xf9, 0x86, 0x5e, 0xb3, 0xe9, 0xc2, 0x24, 0xe4, 0xfc,
             0x64, 0x61, 0xd2, 0xe7, 0x80, 0x77, 0x11, 0xb9, 0x2f, 0xa5, 0x29,
             0x5c, 0xa2, 0x29, 0x81, 0x41, 0x70, 0x50, 0x35, 0x96, 0xa8, 0x0a,
             0xd0, 0xa7, 0x85, 0xb2, 0xcb, 0x60, 0xf3, 0x3f, 0xb2, 0x3d, 0xab,
             0xce, 0x1a, 0x42, 0xdc, 0x1b, 0x70, 0xd0, 0x8c, 0xd4, 0xc6, 0x37,
             0xc4, 0x63, 0xdf, 0x88, 0x46, 0xbe, 0xf6, 0x67, 0xb6, 0x10, 0x2a,
             0x56, 0xc4, 0x08, 0x77, 0x7f, 0x3c, 0x3a, 0x71, 0x59, 0x72, 0x4e,
             0xae, 0xe2, 0x25, 0x0f, 0xaa, 0x76, 0xc8, 0x08, 0x02, 0xea, 0x30,
             0x8a, 0x29, 0x37, 0x76, 0x0c, 0xd0, 0xff, 0x1c, 0x8d, 0xd2, 0xa7,
             0x3c, 0xf1, 0xec, 0x9c, 0x9f, 0x5e, 0xac, 0xae, 0x98, 0xa0, 0xca,
             0x6d, 0xb1, 0xa1, 0xa5, 0xad, 0x80, 0xd8, 0x05, 0x07, 0x76, 0x65,
             0x51, 0x6d, 0xd9, 0x97, 0x25, 0xa7, 0x18, 0xca, 0xfa, 0x1a, 0x0e,
             0x8a, 0xfc, 0xf0, 0xd7, 0x59, 0xc5, 0x99, 0xf6, 0xa0, 0x80, 0x7c,
             0x8a, 0x4f, 0x16, 0xbc, 0x65, 0x33, 0xd5, 0xc7, 0xb7, 0xb5, 0xf0,
             0x91, 0x68, 0xdc, 0x71, 0x2e, 0xce, 0xae, 0x10, 0x82, 0xba, 0x43,
             0x42, 0x70, 0x7a, 0x3f, 0xce, 0x84, 0xb8, 0x86, 0xab, 0xec, 0xbc,
             0xfa, 0x5b, 0x56, 0xab, 0xb2, 0x80, 0x4d, 0xc1, 0x2f, 0x59, 0xf8,
             0xe2, 0xaf, 0xfd}};
        auto const thresh2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto rsa3 = std::make_unique<RsaSha256>(
            makeSlice(rsa3PublicKey), makeSlice(rsa3Sig));
        std::vector<std::unique_ptr<Fulfillment>> thresh2Subfulfillments;
        thresh2Subfulfillments.emplace_back(std::move(rsa3));
        std::vector<Condition> thresh2Subconditions{};
        auto thresh2 = std::make_unique<ThresholdSha256>(
            std::move(thresh2Subfulfillments), std::move(thresh2Subconditions));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(thresh2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x82\x02\x30\x80\x02\x50\x30\x81\x01\x0e\xa2\x82\x02\x25"
                "\xa1\x82\x02\x21\x80\x02\x50\x31\x81\x01\x0e\xa2\x82\x02\x16"
                "\xa2\x82\x02\x12\xa0\x82\x02\x0c\xa3\x82\x02\x08\x80\x82\x01"
                "\x00\xbd\xd1\xc7\xf0\xb0\x3a\xa5\x5b\x3e\x49\x8d\x4e\x00\x54"
                "\x89\xb9\x89\xcd\x4b\x43\xde\x59\xf6\x7a\x67\x5c\x3a\xc6\xcf"
                "\x82\x3f\x35\x9c\xcc\xda\xcd\xd3\x97\x86\x5b\xe9\xf6\x05\x55"
                "\x0b\x26\xef\x1e\x88\xd5\xb6\xba\x14\x0a\xb2\x76\xb9\xb3\x46"
                "\x0c\xc0\x80\x17\x13\x68\x23\xdc\xec\x10\x18\xfc\xaa\xbe\xb3"
                "\xc4\xc7\xa9\x84\xa6\x4e\x5c\x08\x6b\x7b\x4c\x81\x91\x79\x5d"
                "\x90\x06\x15\xbb\x76\x2f\x5c\x53\x60\x0f\xac\xf3\x7c\x49\xc5"
                "\x47\xec\xb3\xda\x93\x87\xc1\xb9\xcf\x2c\xb5\xf0\x85\xad\xb4"
                "\x38\x67\x88\xda\x3d\xfa\x01\xb7\x54\xd9\x41\x0b\x7b\x8a\x09"
                "\xe0\x84\x7d\xbb\x89\xb2\xfc\x0b\x70\x36\x93\x56\x62\xcc\xb4"
                "\xfc\xf9\x1f\x37\x92\x9b\x3a\x4e\x7c\xad\x4b\xa6\x76\x6f\xda"
                "\xc4\x2f\x83\x53\xbd\x93\xa9\x76\x89\x53\xe1\x4d\xee\x27\x11"
                "\x6f\xbc\x21\xad\x42\x9f\x29\xf6\x03\xdd\xec\xfa\xa1\x78\xd2"
                "\xde\x29\x2e\xd8\x3a\x7f\xe9\x9b\x5d\xeb\x37\xb8\xb0\xa0\x66"
                "\x3f\x02\xcd\x2a\x6e\xd3\x1c\xa5\x65\xdc\x73\xbe\x93\x54\x9a"
                "\x2b\xf8\x32\x8b\xe8\xce\x9a\x59\xd0\x05\xeb\xbb\xac\xfc\x4c"
                "\x4b\x2e\xac\x2a\xc3\x0f\x0a\xd7\x46\xaf\xfd\x22\x0d\x0d\x54"
                "\xcc\x2f\x81\x82\x01\x00\x1a\xf2\xdd\xd7\xcd\x23\x5d\xbf\x39"
                "\x59\xa0\xbf\x44\x03\x0b\xbd\xbf\xed\xfc\xb3\x7f\x7f\x05\x16"
                "\x74\xbc\x9a\x55\xaa\xa7\x76\x5f\xd2\x7a\x67\x7c\x1a\xa9\x7a"
                "\x4d\x24\xd1\xfa\xf0\x8e\x4b\x18\xd5\xdc\x4c\x2c\x38\x87\x10"
                "\x39\x8f\x07\xab\x3e\xcc\x6d\x47\x71\xfb\x8c\x25\x16\x1a\xf9"
                "\x86\x5e\xb3\xe9\xc2\x24\xe4\xfc\x64\x61\xd2\xe7\x80\x77\x11"
                "\xb9\x2f\xa5\x29\x5c\xa2\x29\x81\x41\x70\x50\x35\x96\xa8\x0a"
                "\xd0\xa7\x85\xb2\xcb\x60\xf3\x3f\xb2\x3d\xab\xce\x1a\x42\xdc"
                "\x1b\x70\xd0\x8c\xd4\xc6\x37\xc4\x63\xdf\x88\x46\xbe\xf6\x67"
                "\xb6\x10\x2a\x56\xc4\x08\x77\x7f\x3c\x3a\x71\x59\x72\x4e\xae"
                "\xe2\x25\x0f\xaa\x76\xc8\x08\x02\xea\x30\x8a\x29\x37\x76\x0c"
                "\xd0\xff\x1c\x8d\xd2\xa7\x3c\xf1\xec\x9c\x9f\x5e\xac\xae\x98"
                "\xa0\xca\x6d\xb1\xa1\xa5\xad\x80\xd8\x05\x07\x76\x65\x51\x6d"
                "\xd9\x97\x25\xa7\x18\xca\xfa\x1a\x0e\x8a\xfc\xf0\xd7\x59\xc5"
                "\x99\xf6\xa0\x80\x7c\x8a\x4f\x16\xbc\x65\x33\xd5\xc7\xb7\xb5"
                "\xf0\x91\x68\xdc\x71\x2e\xce\xae\x10\x82\xba\x43\x42\x70\x7a"
                "\x3f\xce\x84\xb8\x86\xab\xec\xbc\xfa\x5b\x56\xab\xb2\x80\x4d"
                "\xc1\x2f\x59\xf8\xe2\xaf\xfd\xa1\x00"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\x9c\x64\x9a\x7e\x08\xe1\x71\xc7\x71\x0d\xb6"
                "\x81\x70\x30\x2b\x57\x14\xef\xde\x06\x37\x89\xe5\xeb\xe9\x25"
                "\xd3\xaf\xa4\x57\xfc\x61\x81\x03\x01\x0c\x20\x82\x02\x04\x30"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\xa3\x6e\x68\x3b\xd7\x6f\xed\x39\xe6\x37\xc8\xf0\xa8\xfe\x25"
                "\x1f\x71\xff\x98\x39\x3c\xdb\xd3\xae\x39\x58\x2a\x99\x2a\x57"
                "\xf3\xcd\x81\x03\x01\x08\x10\x82\x02\x04\x30"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix11()
    {
        testcase("Prefix11");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** thresh2
        // **** Preim4Cond
        // **** Rsa5Cond
        // **** Ed6Cond
        // **** rsa3

        auto const rsa3Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 256> const rsa3PublicKey{
            {0xbd, 0xd1, 0xc7, 0xf0, 0xb0, 0x3a, 0xa5, 0x5b, 0x3e, 0x49, 0x8d,
             0x4e, 0x00, 0x54, 0x89, 0xb9, 0x89, 0xcd, 0x4b, 0x43, 0xde, 0x59,
             0xf6, 0x7a, 0x67, 0x5c, 0x3a, 0xc6, 0xcf, 0x82, 0x3f, 0x35, 0x9c,
             0xcc, 0xda, 0xcd, 0xd3, 0x97, 0x86, 0x5b, 0xe9, 0xf6, 0x05, 0x55,
             0x0b, 0x26, 0xef, 0x1e, 0x88, 0xd5, 0xb6, 0xba, 0x14, 0x0a, 0xb2,
             0x76, 0xb9, 0xb3, 0x46, 0x0c, 0xc0, 0x80, 0x17, 0x13, 0x68, 0x23,
             0xdc, 0xec, 0x10, 0x18, 0xfc, 0xaa, 0xbe, 0xb3, 0xc4, 0xc7, 0xa9,
             0x84, 0xa6, 0x4e, 0x5c, 0x08, 0x6b, 0x7b, 0x4c, 0x81, 0x91, 0x79,
             0x5d, 0x90, 0x06, 0x15, 0xbb, 0x76, 0x2f, 0x5c, 0x53, 0x60, 0x0f,
             0xac, 0xf3, 0x7c, 0x49, 0xc5, 0x47, 0xec, 0xb3, 0xda, 0x93, 0x87,
             0xc1, 0xb9, 0xcf, 0x2c, 0xb5, 0xf0, 0x85, 0xad, 0xb4, 0x38, 0x67,
             0x88, 0xda, 0x3d, 0xfa, 0x01, 0xb7, 0x54, 0xd9, 0x41, 0x0b, 0x7b,
             0x8a, 0x09, 0xe0, 0x84, 0x7d, 0xbb, 0x89, 0xb2, 0xfc, 0x0b, 0x70,
             0x36, 0x93, 0x56, 0x62, 0xcc, 0xb4, 0xfc, 0xf9, 0x1f, 0x37, 0x92,
             0x9b, 0x3a, 0x4e, 0x7c, 0xad, 0x4b, 0xa6, 0x76, 0x6f, 0xda, 0xc4,
             0x2f, 0x83, 0x53, 0xbd, 0x93, 0xa9, 0x76, 0x89, 0x53, 0xe1, 0x4d,
             0xee, 0x27, 0x11, 0x6f, 0xbc, 0x21, 0xad, 0x42, 0x9f, 0x29, 0xf6,
             0x03, 0xdd, 0xec, 0xfa, 0xa1, 0x78, 0xd2, 0xde, 0x29, 0x2e, 0xd8,
             0x3a, 0x7f, 0xe9, 0x9b, 0x5d, 0xeb, 0x37, 0xb8, 0xb0, 0xa0, 0x66,
             0x3f, 0x02, 0xcd, 0x2a, 0x6e, 0xd3, 0x1c, 0xa5, 0x65, 0xdc, 0x73,
             0xbe, 0x93, 0x54, 0x9a, 0x2b, 0xf8, 0x32, 0x8b, 0xe8, 0xce, 0x9a,
             0x59, 0xd0, 0x05, 0xeb, 0xbb, 0xac, 0xfc, 0x4c, 0x4b, 0x2e, 0xac,
             0x2a, 0xc3, 0x0f, 0x0a, 0xd7, 0x46, 0xaf, 0xfd, 0x22, 0x0d, 0x0d,
             0x54, 0xcc, 0x2f}};
        std::array<std::uint8_t, 256> const rsa3Sig{
            {0x4b, 0x80, 0x07, 0xb5, 0x59, 0xf1, 0x5b, 0x69, 0x40, 0x50, 0x0d,
             0xcc, 0x42, 0xa2, 0x3d, 0x66, 0xcc, 0xf6, 0xfd, 0xca, 0x38, 0x4a,
             0x9c, 0x35, 0x10, 0x5c, 0xee, 0xf2, 0x2c, 0x90, 0xd7, 0x03, 0x3e,
             0x04, 0x1c, 0x32, 0xd5, 0x59, 0x81, 0x6d, 0x91, 0xe7, 0xa1, 0x07,
             0x85, 0xd4, 0x03, 0x4c, 0x83, 0x39, 0x55, 0x24, 0x51, 0xf5, 0x3b,
             0x45, 0x46, 0x4a, 0x4e, 0xf0, 0xea, 0x03, 0x7d, 0x64, 0xdf, 0xf7,
             0xea, 0x53, 0x74, 0xb5, 0x47, 0x3a, 0xf9, 0x2f, 0xaa, 0x8c, 0x9c,
             0x8e, 0x26, 0xd7, 0xe1, 0xa2, 0x4d, 0xb6, 0x97, 0x4d, 0xf0, 0x3e,
             0xb2, 0xdb, 0xf9, 0xc4, 0xeb, 0x37, 0x5d, 0xca, 0xc6, 0xa1, 0xf1,
             0xf7, 0x87, 0x48, 0xf4, 0x18, 0xd0, 0x12, 0x21, 0x61, 0x09, 0xd3,
             0x52, 0xe2, 0x2b, 0xf1, 0xce, 0xd6, 0xa1, 0x44, 0x6d, 0x2d, 0x7e,
             0xd5, 0x83, 0xa2, 0xaa, 0xcd, 0x54, 0xe0, 0x20, 0x59, 0x95, 0xe8,
             0x73, 0x2e, 0x0d, 0xfe, 0x6f, 0x31, 0x9b, 0xb2, 0x94, 0x35, 0x40,
             0x07, 0x6d, 0x1a, 0x38, 0xaa, 0xf0, 0x07, 0x7e, 0xc8, 0x5a, 0x1a,
             0xf4, 0x0b, 0x8f, 0x80, 0x4b, 0xea, 0xd3, 0x1a, 0x60, 0xd6, 0xf2,
             0x9d, 0xa5, 0x6e, 0x23, 0xad, 0x6d, 0xed, 0x96, 0xf5, 0xf5, 0x82,
             0xf9, 0x2f, 0xce, 0x90, 0x76, 0x20, 0x98, 0xcc, 0xa7, 0x85, 0xd3,
             0x77, 0xbb, 0x74, 0x7b, 0xee, 0xb1, 0xf8, 0x47, 0x6f, 0x63, 0x6c,
             0x06, 0x89, 0x8a, 0xa5, 0x17, 0x66, 0xed, 0x31, 0xdd, 0xea, 0x4f,
             0x88, 0x47, 0x91, 0x5d, 0x06, 0xde, 0x0e, 0x70, 0x62, 0x23, 0x9e,
             0xff, 0x0e, 0x2a, 0x21, 0x43, 0x91, 0x79, 0xc8, 0x2e, 0xcd, 0xe1,
             0xb1, 0x89, 0xca, 0x1a, 0x34, 0x9d, 0x76, 0x9e, 0x74, 0x3d, 0x85,
             0x50, 0x99, 0xec, 0xc1, 0x7b, 0x01, 0x8b, 0xe4, 0x34, 0x99, 0xbc,
             0x71, 0xab, 0x07}};
        auto const thresh2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim4CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim4Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim4CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa5CondConditionFingerprint =
            "\x99\xfb\x0b\x38\x94\x4d\x20\x85\xc8\xda\x3a\x64\x31\x44\x6f"
            "\x6c\x3b\x46\x25\x50\xd7\x7f\xdf\xee\x75\x72\x71\xf9\x61\x40"
            "\x63\xfa"s;
        Condition const Rsa5Cond{Type::rsaSha256,
                                 65536,
                                 makeSlice(Rsa5CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const Ed6CondConditionFingerprint =
            "\x00\xd3\xc9\x24\x3f\x2d\x2e\x64\x93\xa8\x49\x29\x82\x75\xea"
            "\xbf\xe3\x53\x7f\x8e\x45\x16\xdb\x5e\xc6\xdf\x39\xd2\xcb\xea"
            "\x62\xfb"s;
        Condition const Ed6Cond{Type::ed25519Sha256,
                                131072,
                                makeSlice(Ed6CondConditionFingerprint),
                                std::bitset<5>{0}};
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto rsa3 = std::make_unique<RsaSha256>(
            makeSlice(rsa3PublicKey), makeSlice(rsa3Sig));
        std::vector<std::unique_ptr<Fulfillment>> thresh2Subfulfillments;
        thresh2Subfulfillments.emplace_back(std::move(rsa3));
        std::vector<Condition> thresh2Subconditions{
            {Preim4Cond, Rsa5Cond, Ed6Cond}};
        auto thresh2 = std::make_unique<ThresholdSha256>(
            std::move(thresh2Subfulfillments), std::move(thresh2Subconditions));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(thresh2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x82\x02\xa9\x80\x02\x50\x30\x81\x01\x0e\xa2\x82\x02\x9e"
                "\xa1\x82\x02\x9a\x80\x02\x50\x31\x81\x01\x0e\xa2\x82\x02\x8f"
                "\xa2\x82\x02\x8b\xa0\x82\x02\x0c\xa3\x82\x02\x08\x80\x82\x01"
                "\x00\xbd\xd1\xc7\xf0\xb0\x3a\xa5\x5b\x3e\x49\x8d\x4e\x00\x54"
                "\x89\xb9\x89\xcd\x4b\x43\xde\x59\xf6\x7a\x67\x5c\x3a\xc6\xcf"
                "\x82\x3f\x35\x9c\xcc\xda\xcd\xd3\x97\x86\x5b\xe9\xf6\x05\x55"
                "\x0b\x26\xef\x1e\x88\xd5\xb6\xba\x14\x0a\xb2\x76\xb9\xb3\x46"
                "\x0c\xc0\x80\x17\x13\x68\x23\xdc\xec\x10\x18\xfc\xaa\xbe\xb3"
                "\xc4\xc7\xa9\x84\xa6\x4e\x5c\x08\x6b\x7b\x4c\x81\x91\x79\x5d"
                "\x90\x06\x15\xbb\x76\x2f\x5c\x53\x60\x0f\xac\xf3\x7c\x49\xc5"
                "\x47\xec\xb3\xda\x93\x87\xc1\xb9\xcf\x2c\xb5\xf0\x85\xad\xb4"
                "\x38\x67\x88\xda\x3d\xfa\x01\xb7\x54\xd9\x41\x0b\x7b\x8a\x09"
                "\xe0\x84\x7d\xbb\x89\xb2\xfc\x0b\x70\x36\x93\x56\x62\xcc\xb4"
                "\xfc\xf9\x1f\x37\x92\x9b\x3a\x4e\x7c\xad\x4b\xa6\x76\x6f\xda"
                "\xc4\x2f\x83\x53\xbd\x93\xa9\x76\x89\x53\xe1\x4d\xee\x27\x11"
                "\x6f\xbc\x21\xad\x42\x9f\x29\xf6\x03\xdd\xec\xfa\xa1\x78\xd2"
                "\xde\x29\x2e\xd8\x3a\x7f\xe9\x9b\x5d\xeb\x37\xb8\xb0\xa0\x66"
                "\x3f\x02\xcd\x2a\x6e\xd3\x1c\xa5\x65\xdc\x73\xbe\x93\x54\x9a"
                "\x2b\xf8\x32\x8b\xe8\xce\x9a\x59\xd0\x05\xeb\xbb\xac\xfc\x4c"
                "\x4b\x2e\xac\x2a\xc3\x0f\x0a\xd7\x46\xaf\xfd\x22\x0d\x0d\x54"
                "\xcc\x2f\x81\x82\x01\x00\x4b\x80\x07\xb5\x59\xf1\x5b\x69\x40"
                "\x50\x0d\xcc\x42\xa2\x3d\x66\xcc\xf6\xfd\xca\x38\x4a\x9c\x35"
                "\x10\x5c\xee\xf2\x2c\x90\xd7\x03\x3e\x04\x1c\x32\xd5\x59\x81"
                "\x6d\x91\xe7\xa1\x07\x85\xd4\x03\x4c\x83\x39\x55\x24\x51\xf5"
                "\x3b\x45\x46\x4a\x4e\xf0\xea\x03\x7d\x64\xdf\xf7\xea\x53\x74"
                "\xb5\x47\x3a\xf9\x2f\xaa\x8c\x9c\x8e\x26\xd7\xe1\xa2\x4d\xb6"
                "\x97\x4d\xf0\x3e\xb2\xdb\xf9\xc4\xeb\x37\x5d\xca\xc6\xa1\xf1"
                "\xf7\x87\x48\xf4\x18\xd0\x12\x21\x61\x09\xd3\x52\xe2\x2b\xf1"
                "\xce\xd6\xa1\x44\x6d\x2d\x7e\xd5\x83\xa2\xaa\xcd\x54\xe0\x20"
                "\x59\x95\xe8\x73\x2e\x0d\xfe\x6f\x31\x9b\xb2\x94\x35\x40\x07"
                "\x6d\x1a\x38\xaa\xf0\x07\x7e\xc8\x5a\x1a\xf4\x0b\x8f\x80\x4b"
                "\xea\xd3\x1a\x60\xd6\xf2\x9d\xa5\x6e\x23\xad\x6d\xed\x96\xf5"
                "\xf5\x82\xf9\x2f\xce\x90\x76\x20\x98\xcc\xa7\x85\xd3\x77\xbb"
                "\x74\x7b\xee\xb1\xf8\x47\x6f\x63\x6c\x06\x89\x8a\xa5\x17\x66"
                "\xed\x31\xdd\xea\x4f\x88\x47\x91\x5d\x06\xde\x0e\x70\x62\x23"
                "\x9e\xff\x0e\x2a\x21\x43\x91\x79\xc8\x2e\xcd\xe1\xb1\x89\xca"
                "\x1a\x34\x9d\x76\x9e\x74\x3d\x85\x50\x99\xec\xc1\x7b\x01\x8b"
                "\xe4\x34\x99\xbc\x71\xab\x07\xa1\x79\xa0\x25\x80\x20\x5d\xa0"
                "\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a\x5b\xd1"
                "\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58\xeb\x4e"
                "\x81\x01\x09\xa3\x27\x80\x20\x99\xfb\x0b\x38\x94\x4d\x20\x85"
                "\xc8\xda\x3a\x64\x31\x44\x6f\x6c\x3b\x46\x25\x50\xd7\x7f\xdf"
                "\xee\x75\x72\x71\xf9\x61\x40\x63\xfa\x81\x03\x01\x00\x00\xa4"
                "\x27\x80\x20\x00\xd3\xc9\x24\x3f\x2d\x2e\x64\x93\xa8\x49\x29"
                "\x82\x75\xea\xbf\xe3\x53\x7f\x8e\x45\x16\xdb\x5e\xc6\xdf\x39"
                "\xd2\xcb\xea\x62\xfb\x81\x03\x02\x00\x00"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\xe9\x2c\x6f\xeb\xd7\xfe\x00\x7f\x20\x26\x5f"
                "\x19\x4d\x16\xd6\x16\x42\x34\xbb\xba\x09\x82\x2f\x2d\xff\x6f"
                "\x7a\xe7\x82\x9f\x5b\x06\x81\x03\x02\x18\x20\x82\x02\x03\xb8"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\xf1\xf8\xd6\xe7\x27\x30\xca\x4d\x49\xe2\x26\xef\xdf\xaf\x77"
                "\xbb\x01\xd8\x93\x03\xb5\x2e\xf4\xa1\x54\x6e\xa9\x6b\x53\x4e"
                "\xf4\x68\x81\x03\x02\x14\x10\x82\x02\x03\xb8"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix12()
    {
        testcase("Prefix12");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** thresh2
        // **** Preim9Cond
        // **** Rsa10Cond
        // **** Ed11Cond
        // **** rsa3
        // **** thresh4
        // ***** Preim6Cond
        // ***** Rsa7Cond
        // ***** Ed8Cond
        // ***** rsa5

        auto const rsa3Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 256> const rsa3PublicKey{
            {0xbd, 0xd1, 0xc7, 0xf0, 0xb0, 0x3a, 0xa5, 0x5b, 0x3e, 0x49, 0x8d,
             0x4e, 0x00, 0x54, 0x89, 0xb9, 0x89, 0xcd, 0x4b, 0x43, 0xde, 0x59,
             0xf6, 0x7a, 0x67, 0x5c, 0x3a, 0xc6, 0xcf, 0x82, 0x3f, 0x35, 0x9c,
             0xcc, 0xda, 0xcd, 0xd3, 0x97, 0x86, 0x5b, 0xe9, 0xf6, 0x05, 0x55,
             0x0b, 0x26, 0xef, 0x1e, 0x88, 0xd5, 0xb6, 0xba, 0x14, 0x0a, 0xb2,
             0x76, 0xb9, 0xb3, 0x46, 0x0c, 0xc0, 0x80, 0x17, 0x13, 0x68, 0x23,
             0xdc, 0xec, 0x10, 0x18, 0xfc, 0xaa, 0xbe, 0xb3, 0xc4, 0xc7, 0xa9,
             0x84, 0xa6, 0x4e, 0x5c, 0x08, 0x6b, 0x7b, 0x4c, 0x81, 0x91, 0x79,
             0x5d, 0x90, 0x06, 0x15, 0xbb, 0x76, 0x2f, 0x5c, 0x53, 0x60, 0x0f,
             0xac, 0xf3, 0x7c, 0x49, 0xc5, 0x47, 0xec, 0xb3, 0xda, 0x93, 0x87,
             0xc1, 0xb9, 0xcf, 0x2c, 0xb5, 0xf0, 0x85, 0xad, 0xb4, 0x38, 0x67,
             0x88, 0xda, 0x3d, 0xfa, 0x01, 0xb7, 0x54, 0xd9, 0x41, 0x0b, 0x7b,
             0x8a, 0x09, 0xe0, 0x84, 0x7d, 0xbb, 0x89, 0xb2, 0xfc, 0x0b, 0x70,
             0x36, 0x93, 0x56, 0x62, 0xcc, 0xb4, 0xfc, 0xf9, 0x1f, 0x37, 0x92,
             0x9b, 0x3a, 0x4e, 0x7c, 0xad, 0x4b, 0xa6, 0x76, 0x6f, 0xda, 0xc4,
             0x2f, 0x83, 0x53, 0xbd, 0x93, 0xa9, 0x76, 0x89, 0x53, 0xe1, 0x4d,
             0xee, 0x27, 0x11, 0x6f, 0xbc, 0x21, 0xad, 0x42, 0x9f, 0x29, 0xf6,
             0x03, 0xdd, 0xec, 0xfa, 0xa1, 0x78, 0xd2, 0xde, 0x29, 0x2e, 0xd8,
             0x3a, 0x7f, 0xe9, 0x9b, 0x5d, 0xeb, 0x37, 0xb8, 0xb0, 0xa0, 0x66,
             0x3f, 0x02, 0xcd, 0x2a, 0x6e, 0xd3, 0x1c, 0xa5, 0x65, 0xdc, 0x73,
             0xbe, 0x93, 0x54, 0x9a, 0x2b, 0xf8, 0x32, 0x8b, 0xe8, 0xce, 0x9a,
             0x59, 0xd0, 0x05, 0xeb, 0xbb, 0xac, 0xfc, 0x4c, 0x4b, 0x2e, 0xac,
             0x2a, 0xc3, 0x0f, 0x0a, 0xd7, 0x46, 0xaf, 0xfd, 0x22, 0x0d, 0x0d,
             0x54, 0xcc, 0x2f}};
        std::array<std::uint8_t, 256> const rsa3Sig{
            {0x0b, 0xe7, 0xe5, 0xaa, 0xdc, 0xb7, 0xbb, 0xca, 0x1b, 0xa1, 0xac,
             0xed, 0xd8, 0x31, 0x6b, 0xf8, 0x30, 0xb9, 0x58, 0x13, 0xec, 0x08,
             0x52, 0xe0, 0x83, 0x02, 0xfa, 0x1a, 0x5a, 0xe9, 0x86, 0x28, 0xe7,
             0x77, 0xc5, 0xed, 0xc9, 0xba, 0xb4, 0x9f, 0x3f, 0xdf, 0x1d, 0xd4,
             0xbf, 0xbc, 0x7d, 0x6f, 0x17, 0x3a, 0x94, 0xf0, 0xa4, 0x3d, 0xae,
             0x3d, 0x04, 0xf2, 0x1e, 0x5b, 0xcd, 0x64, 0xfe, 0xca, 0x99, 0xd3,
             0x94, 0x3d, 0x27, 0x4f, 0x7c, 0xcc, 0xe1, 0xde, 0x2d, 0x2d, 0x26,
             0x43, 0xe4, 0xd5, 0x50, 0xd9, 0x7e, 0x5e, 0x07, 0x35, 0xd7, 0x7b,
             0x8b, 0x1b, 0xc5, 0xd7, 0xf6, 0xb6, 0x83, 0xd8, 0x9f, 0x0a, 0x1b,
             0x6d, 0x9d, 0xaf, 0xc1, 0x93, 0x51, 0x8a, 0x1c, 0x5c, 0xac, 0x96,
             0x66, 0x0d, 0xda, 0x76, 0x7e, 0x7b, 0x87, 0x68, 0x0c, 0xff, 0xff,
             0xa1, 0xf0, 0xe3, 0x12, 0xe2, 0x43, 0x2a, 0x72, 0x45, 0xee, 0x13,
             0x59, 0x0f, 0x6a, 0xa9, 0x55, 0xc8, 0x7c, 0xa7, 0x17, 0x9a, 0xdd,
             0xba, 0x47, 0x41, 0x2f, 0x79, 0xe6, 0x7c, 0x90, 0xbb, 0xc5, 0x9e,
             0x55, 0x28, 0xc8, 0x29, 0x64, 0x0b, 0xe1, 0x8a, 0x4b, 0x24, 0x0d,
             0x40, 0x2c, 0xb3, 0x53, 0xa0, 0x89, 0x9c, 0xc6, 0x6a, 0xd1, 0x6d,
             0x98, 0xf9, 0xe1, 0x01, 0xa8, 0x1f, 0x02, 0xed, 0x61, 0xc5, 0xd2,
             0xc0, 0x15, 0xd7, 0x01, 0x4a, 0x3f, 0x1c, 0xc5, 0x72, 0xcf, 0x24,
             0x68, 0xab, 0xcf, 0x31, 0x4c, 0x1a, 0x11, 0x3f, 0x03, 0x40, 0x83,
             0x94, 0xc5, 0x47, 0x02, 0xcd, 0x01, 0xe0, 0x19, 0xf4, 0x0a, 0xd4,
             0x2b, 0xcc, 0x13, 0xfc, 0x76, 0xca, 0x91, 0x90, 0x67, 0x69, 0xdd,
             0x39, 0x8d, 0x38, 0xb5, 0xf7, 0x51, 0xc6, 0x73, 0xce, 0xb7, 0x60,
             0x54, 0x13, 0xbc, 0x7f, 0x8b, 0xa4, 0x48, 0x27, 0xf5, 0x5a, 0x1b,
             0x59, 0x8a, 0x14}};
        auto const rsa5Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 256> const rsa5PublicKey{
            {0xc0, 0x00, 0xef, 0x8f, 0x4b, 0x81, 0x10, 0x1e, 0x52, 0xe0, 0x07,
             0x9f, 0x68, 0xe7, 0x2f, 0x92, 0xd4, 0x77, 0x3c, 0x1f, 0xa3, 0xff,
             0x72, 0x64, 0x5b, 0x37, 0xf1, 0xf3, 0xa3, 0xc5, 0xfb, 0xcd, 0xfb,
             0xda, 0xcc, 0x8b, 0x52, 0xe1, 0xde, 0xbc, 0x28, 0x8d, 0xe5, 0xad,
             0xab, 0x86, 0x61, 0x45, 0x97, 0x65, 0x37, 0x68, 0x26, 0x21, 0x92,
             0x17, 0xa3, 0xb0, 0x74, 0x5c, 0x8a, 0x45, 0x8d, 0x87, 0x5b, 0x9b,
             0xd1, 0x7b, 0x07, 0xc4, 0x8c, 0x67, 0xa0, 0xe9, 0x82, 0x0c, 0xe0,
             0x6b, 0xea, 0x91, 0x5c, 0xba, 0xe3, 0xd9, 0x9d, 0x39, 0xfd, 0x77,
             0xac, 0xcb, 0x33, 0x9b, 0x28, 0x51, 0x8d, 0xbf, 0x3e, 0xe4, 0x94,
             0x1c, 0x9a, 0x60, 0x71, 0x4b, 0x34, 0x07, 0x30, 0xda, 0x42, 0x46,
             0x0e, 0xb8, 0xb7, 0x2c, 0xf5, 0x2f, 0x4b, 0x9e, 0xe7, 0x64, 0x81,
             0xa1, 0xa2, 0x05, 0x66, 0x92, 0xe6, 0x75, 0x9f, 0x37, 0xae, 0x40,
             0xa9, 0x16, 0x08, 0x19, 0xe8, 0xdc, 0x47, 0xd6, 0x03, 0x29, 0xab,
             0xcc, 0x58, 0xa2, 0x37, 0x2a, 0x32, 0xb8, 0x15, 0xc7, 0x51, 0x91,
             0x73, 0xb9, 0x1d, 0xc6, 0xd0, 0x4f, 0x85, 0x86, 0xd5, 0xb3, 0x21,
             0x1a, 0x2a, 0x6c, 0xeb, 0x7f, 0xfe, 0x84, 0x17, 0x10, 0x2d, 0x0e,
             0xb4, 0xe1, 0xc2, 0x48, 0x4c, 0x3f, 0x61, 0xc7, 0x59, 0x75, 0xa7,
             0xc1, 0x75, 0xce, 0x67, 0x17, 0x42, 0x2a, 0x2f, 0x96, 0xef, 0x8a,
             0x2d, 0x74, 0xd2, 0x13, 0x68, 0xe1, 0xe9, 0xea, 0xfb, 0x73, 0x68,
             0xed, 0x8d, 0xd3, 0xac, 0x49, 0x09, 0xf9, 0xec, 0x62, 0xdf, 0x53,
             0xab, 0xfe, 0x90, 0x64, 0x4b, 0x92, 0x60, 0x0d, 0xdd, 0x00, 0xfe,
             0x02, 0xe6, 0xf3, 0x9b, 0x2b, 0xac, 0x4f, 0x70, 0xe8, 0x5b, 0x69,
             0x9c, 0x40, 0xd3, 0xeb, 0x37, 0xad, 0x6f, 0x37, 0xab, 0xf3, 0x79,
             0x8e, 0xcb, 0x1d}};
        std::array<std::uint8_t, 256> const rsa5Sig{
            {0x0c, 0xad, 0x6c, 0xcb, 0x38, 0x30, 0x2e, 0xf4, 0x40, 0x44, 0xd8,
             0x2c, 0x42, 0x0a, 0x5b, 0x9f, 0xa7, 0x5b, 0xb7, 0x4e, 0xe5, 0xa9,
             0x6c, 0xc6, 0x97, 0x6b, 0xe3, 0x91, 0x18, 0x1f, 0xb9, 0x5c, 0x0c,
             0xff, 0x08, 0xcc, 0x22, 0x46, 0x04, 0x06, 0xd0, 0x3b, 0x67, 0x77,
             0x25, 0x3a, 0x19, 0x5c, 0x81, 0x35, 0x78, 0x6a, 0x52, 0x39, 0xfa,
             0x61, 0xc4, 0x68, 0xed, 0x94, 0x82, 0x68, 0x48, 0x4f, 0x63, 0x89,
             0xde, 0x3e, 0xa8, 0x6a, 0x16, 0x16, 0xbb, 0xc0, 0xe2, 0x6c, 0xa0,
             0x90, 0xaf, 0x45, 0x71, 0x44, 0xec, 0x25, 0xca, 0x04, 0xfa, 0x64,
             0xc6, 0x73, 0xc9, 0xe3, 0x79, 0x44, 0x12, 0xb3, 0xe3, 0x18, 0x13,
             0x28, 0x73, 0xaf, 0xf4, 0x4b, 0x13, 0xa3, 0xbf, 0x41, 0x25, 0xdf,
             0xf2, 0x10, 0xff, 0x62, 0x0b, 0xa8, 0x9b, 0x70, 0x7c, 0x95, 0x62,
             0x6c, 0xb7, 0xbd, 0x69, 0xcd, 0x22, 0x9f, 0x6f, 0x5b, 0xcb, 0x28,
             0x62, 0x71, 0xde, 0xed, 0x7b, 0x4c, 0x8c, 0xd8, 0x23, 0x10, 0xe0,
             0x13, 0x20, 0xec, 0xe4, 0xee, 0x09, 0xc3, 0xe6, 0x53, 0x17, 0x45,
             0x2b, 0x4c, 0x36, 0x14, 0xa6, 0x68, 0xf2, 0xdf, 0x92, 0x55, 0x31,
             0x18, 0xd3, 0x53, 0x06, 0x4f, 0x0f, 0xf5, 0x27, 0xdb, 0x2e, 0xfe,
             0x34, 0x96, 0xaf, 0x79, 0x63, 0x0e, 0x15, 0x71, 0x02, 0x1a, 0x7c,
             0x29, 0xa1, 0x4a, 0x97, 0x1d, 0x1e, 0x76, 0xe3, 0x2f, 0xc8, 0x03,
             0x26, 0x78, 0x16, 0x70, 0xf6, 0x2e, 0xa0, 0x6e, 0xab, 0xbf, 0xef,
             0x7c, 0x6a, 0x09, 0x25, 0xde, 0xef, 0x55, 0x15, 0xc6, 0x38, 0x0d,
             0xd7, 0xc6, 0x0c, 0x3d, 0x87, 0x24, 0x73, 0x4d, 0x5b, 0x2c, 0x5a,
             0xfb, 0xb6, 0xc5, 0xb1, 0x6f, 0xa2, 0x67, 0xe4, 0x3c, 0x43, 0xfa,
             0x53, 0x93, 0xa7, 0xdf, 0x76, 0x29, 0xa7, 0x44, 0x4b, 0x6a, 0x35,
             0x67, 0x56, 0x53}};
        auto const thresh4Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim6CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim6Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim6CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa7CondConditionFingerprint =
            "\x6c\x7b\xea\x83\xa1\xf4\x82\x3d\x36\xe7\x6e\xae\x1a\xbc\xa0"
            "\xba\x90\x3d\x96\xc1\xe6\xad\x3a\x47\xa5\xcb\x88\xab\x3c\x5f"
            "\xcc\xd5"s;
        Condition const Rsa7Cond{Type::rsaSha256,
                                 65536,
                                 makeSlice(Rsa7CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const Ed8CondConditionFingerprint =
            "\xf1\x68\x96\xa6\x2e\xef\x7f\x47\x06\x51\x4c\xc6\x7e\x24\xf7"
            "\x29\x84\x9c\xd6\xb0\xd9\x4b\xd9\x0f\xc9\x34\x01\x9d\x92\xeb"
            "\xbc\x0a"s;
        Condition const Ed8Cond{Type::ed25519Sha256,
                                131072,
                                makeSlice(Ed8CondConditionFingerprint),
                                std::bitset<5>{0}};
        auto const thresh2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim9CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim9Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim9CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa10CondConditionFingerprint =
            "\x3c\x73\x38\xcf\x23\xc6\x31\x53\x28\xc4\x27\xf8\x95\x87\x99"
            "\x83\x2d\x35\x3c\x03\x9b\xd1\xff\xff\x2e\x53\x20\xe9\x5e\x62"
            "\xb9\xb7"s;
        Condition const Rsa10Cond{Type::rsaSha256,
                                  65536,
                                  makeSlice(Rsa10CondConditionFingerprint),
                                  std::bitset<5>{0}};
        auto const Ed11CondConditionFingerprint =
            "\x41\x80\x08\xb2\x60\x74\x57\x6d\xac\xed\x74\x7f\x54\xdb\x96"
            "\x18\x91\x06\x0a\x95\xa1\x49\x17\xc7\x65\xe3\x94\xc8\x5e\x2c"
            "\x92\x20"s;
        Condition const Ed11Cond{Type::ed25519Sha256,
                                 131072,
                                 makeSlice(Ed11CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto rsa3 = std::make_unique<RsaSha256>(
            makeSlice(rsa3PublicKey), makeSlice(rsa3Sig));
        auto rsa5 = std::make_unique<RsaSha256>(
            makeSlice(rsa5PublicKey), makeSlice(rsa5Sig));
        std::vector<std::unique_ptr<Fulfillment>> thresh4Subfulfillments;
        thresh4Subfulfillments.emplace_back(std::move(rsa5));
        std::vector<Condition> thresh4Subconditions{
            {Preim6Cond, Rsa7Cond, Ed8Cond}};
        auto thresh4 = std::make_unique<ThresholdSha256>(
            std::move(thresh4Subfulfillments), std::move(thresh4Subconditions));
        std::vector<std::unique_ptr<Fulfillment>> thresh2Subfulfillments;
        thresh2Subfulfillments.emplace_back(std::move(rsa3));
        thresh2Subfulfillments.emplace_back(std::move(thresh4));
        std::vector<Condition> thresh2Subconditions{
            {Preim9Cond, Rsa10Cond, Ed11Cond}};
        auto thresh2 = std::make_unique<ThresholdSha256>(
            std::move(thresh2Subfulfillments), std::move(thresh2Subconditions));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(thresh2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x82\x05\x38\x80\x02\x50\x30\x81\x01\x0e\xa2\x82\x05\x2d"
                "\xa1\x82\x05\x29\x80\x02\x50\x31\x81\x01\x0e\xa2\x82\x05\x1e"
                "\xa2\x82\x05\x1a\xa0\x82\x04\x9b\xa2\x82\x02\x8b\xa0\x82\x02"
                "\x0c\xa3\x82\x02\x08\x80\x82\x01\x00\xc0\x00\xef\x8f\x4b\x81"
                "\x10\x1e\x52\xe0\x07\x9f\x68\xe7\x2f\x92\xd4\x77\x3c\x1f\xa3"
                "\xff\x72\x64\x5b\x37\xf1\xf3\xa3\xc5\xfb\xcd\xfb\xda\xcc\x8b"
                "\x52\xe1\xde\xbc\x28\x8d\xe5\xad\xab\x86\x61\x45\x97\x65\x37"
                "\x68\x26\x21\x92\x17\xa3\xb0\x74\x5c\x8a\x45\x8d\x87\x5b\x9b"
                "\xd1\x7b\x07\xc4\x8c\x67\xa0\xe9\x82\x0c\xe0\x6b\xea\x91\x5c"
                "\xba\xe3\xd9\x9d\x39\xfd\x77\xac\xcb\x33\x9b\x28\x51\x8d\xbf"
                "\x3e\xe4\x94\x1c\x9a\x60\x71\x4b\x34\x07\x30\xda\x42\x46\x0e"
                "\xb8\xb7\x2c\xf5\x2f\x4b\x9e\xe7\x64\x81\xa1\xa2\x05\x66\x92"
                "\xe6\x75\x9f\x37\xae\x40\xa9\x16\x08\x19\xe8\xdc\x47\xd6\x03"
                "\x29\xab\xcc\x58\xa2\x37\x2a\x32\xb8\x15\xc7\x51\x91\x73\xb9"
                "\x1d\xc6\xd0\x4f\x85\x86\xd5\xb3\x21\x1a\x2a\x6c\xeb\x7f\xfe"
                "\x84\x17\x10\x2d\x0e\xb4\xe1\xc2\x48\x4c\x3f\x61\xc7\x59\x75"
                "\xa7\xc1\x75\xce\x67\x17\x42\x2a\x2f\x96\xef\x8a\x2d\x74\xd2"
                "\x13\x68\xe1\xe9\xea\xfb\x73\x68\xed\x8d\xd3\xac\x49\x09\xf9"
                "\xec\x62\xdf\x53\xab\xfe\x90\x64\x4b\x92\x60\x0d\xdd\x00\xfe"
                "\x02\xe6\xf3\x9b\x2b\xac\x4f\x70\xe8\x5b\x69\x9c\x40\xd3\xeb"
                "\x37\xad\x6f\x37\xab\xf3\x79\x8e\xcb\x1d\x81\x82\x01\x00\x0c"
                "\xad\x6c\xcb\x38\x30\x2e\xf4\x40\x44\xd8\x2c\x42\x0a\x5b\x9f"
                "\xa7\x5b\xb7\x4e\xe5\xa9\x6c\xc6\x97\x6b\xe3\x91\x18\x1f\xb9"
                "\x5c\x0c\xff\x08\xcc\x22\x46\x04\x06\xd0\x3b\x67\x77\x25\x3a"
                "\x19\x5c\x81\x35\x78\x6a\x52\x39\xfa\x61\xc4\x68\xed\x94\x82"
                "\x68\x48\x4f\x63\x89\xde\x3e\xa8\x6a\x16\x16\xbb\xc0\xe2\x6c"
                "\xa0\x90\xaf\x45\x71\x44\xec\x25\xca\x04\xfa\x64\xc6\x73\xc9"
                "\xe3\x79\x44\x12\xb3\xe3\x18\x13\x28\x73\xaf\xf4\x4b\x13\xa3"
                "\xbf\x41\x25\xdf\xf2\x10\xff\x62\x0b\xa8\x9b\x70\x7c\x95\x62"
                "\x6c\xb7\xbd\x69\xcd\x22\x9f\x6f\x5b\xcb\x28\x62\x71\xde\xed"
                "\x7b\x4c\x8c\xd8\x23\x10\xe0\x13\x20\xec\xe4\xee\x09\xc3\xe6"
                "\x53\x17\x45\x2b\x4c\x36\x14\xa6\x68\xf2\xdf\x92\x55\x31\x18"
                "\xd3\x53\x06\x4f\x0f\xf5\x27\xdb\x2e\xfe\x34\x96\xaf\x79\x63"
                "\x0e\x15\x71\x02\x1a\x7c\x29\xa1\x4a\x97\x1d\x1e\x76\xe3\x2f"
                "\xc8\x03\x26\x78\x16\x70\xf6\x2e\xa0\x6e\xab\xbf\xef\x7c\x6a"
                "\x09\x25\xde\xef\x55\x15\xc6\x38\x0d\xd7\xc6\x0c\x3d\x87\x24"
                "\x73\x4d\x5b\x2c\x5a\xfb\xb6\xc5\xb1\x6f\xa2\x67\xe4\x3c\x43"
                "\xfa\x53\x93\xa7\xdf\x76\x29\xa7\x44\x4b\x6a\x35\x67\x56\x53"
                "\xa1\x79\xa0\x25\x80\x20\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51"
                "\xe8\x5f\x5e\x54\x2d\x6a\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68"
                "\xfc\xfd\x53\xee\x93\x58\xeb\x4e\x81\x01\x09\xa3\x27\x80\x20"
                "\x6c\x7b\xea\x83\xa1\xf4\x82\x3d\x36\xe7\x6e\xae\x1a\xbc\xa0"
                "\xba\x90\x3d\x96\xc1\xe6\xad\x3a\x47\xa5\xcb\x88\xab\x3c\x5f"
                "\xcc\xd5\x81\x03\x01\x00\x00\xa4\x27\x80\x20\xf1\x68\x96\xa6"
                "\x2e\xef\x7f\x47\x06\x51\x4c\xc6\x7e\x24\xf7\x29\x84\x9c\xd6"
                "\xb0\xd9\x4b\xd9\x0f\xc9\x34\x01\x9d\x92\xeb\xbc\x0a\x81\x03"
                "\x02\x00\x00\xa3\x82\x02\x08\x80\x82\x01\x00\xbd\xd1\xc7\xf0"
                "\xb0\x3a\xa5\x5b\x3e\x49\x8d\x4e\x00\x54\x89\xb9\x89\xcd\x4b"
                "\x43\xde\x59\xf6\x7a\x67\x5c\x3a\xc6\xcf\x82\x3f\x35\x9c\xcc"
                "\xda\xcd\xd3\x97\x86\x5b\xe9\xf6\x05\x55\x0b\x26\xef\x1e\x88"
                "\xd5\xb6\xba\x14\x0a\xb2\x76\xb9\xb3\x46\x0c\xc0\x80\x17\x13"
                "\x68\x23\xdc\xec\x10\x18\xfc\xaa\xbe\xb3\xc4\xc7\xa9\x84\xa6"
                "\x4e\x5c\x08\x6b\x7b\x4c\x81\x91\x79\x5d\x90\x06\x15\xbb\x76"
                "\x2f\x5c\x53\x60\x0f\xac\xf3\x7c\x49\xc5\x47\xec\xb3\xda\x93"
                "\x87\xc1\xb9\xcf\x2c\xb5\xf0\x85\xad\xb4\x38\x67\x88\xda\x3d"
                "\xfa\x01\xb7\x54\xd9\x41\x0b\x7b\x8a\x09\xe0\x84\x7d\xbb\x89"
                "\xb2\xfc\x0b\x70\x36\x93\x56\x62\xcc\xb4\xfc\xf9\x1f\x37\x92"
                "\x9b\x3a\x4e\x7c\xad\x4b\xa6\x76\x6f\xda\xc4\x2f\x83\x53\xbd"
                "\x93\xa9\x76\x89\x53\xe1\x4d\xee\x27\x11\x6f\xbc\x21\xad\x42"
                "\x9f\x29\xf6\x03\xdd\xec\xfa\xa1\x78\xd2\xde\x29\x2e\xd8\x3a"
                "\x7f\xe9\x9b\x5d\xeb\x37\xb8\xb0\xa0\x66\x3f\x02\xcd\x2a\x6e"
                "\xd3\x1c\xa5\x65\xdc\x73\xbe\x93\x54\x9a\x2b\xf8\x32\x8b\xe8"
                "\xce\x9a\x59\xd0\x05\xeb\xbb\xac\xfc\x4c\x4b\x2e\xac\x2a\xc3"
                "\x0f\x0a\xd7\x46\xaf\xfd\x22\x0d\x0d\x54\xcc\x2f\x81\x82\x01"
                "\x00\x0b\xe7\xe5\xaa\xdc\xb7\xbb\xca\x1b\xa1\xac\xed\xd8\x31"
                "\x6b\xf8\x30\xb9\x58\x13\xec\x08\x52\xe0\x83\x02\xfa\x1a\x5a"
                "\xe9\x86\x28\xe7\x77\xc5\xed\xc9\xba\xb4\x9f\x3f\xdf\x1d\xd4"
                "\xbf\xbc\x7d\x6f\x17\x3a\x94\xf0\xa4\x3d\xae\x3d\x04\xf2\x1e"
                "\x5b\xcd\x64\xfe\xca\x99\xd3\x94\x3d\x27\x4f\x7c\xcc\xe1\xde"
                "\x2d\x2d\x26\x43\xe4\xd5\x50\xd9\x7e\x5e\x07\x35\xd7\x7b\x8b"
                "\x1b\xc5\xd7\xf6\xb6\x83\xd8\x9f\x0a\x1b\x6d\x9d\xaf\xc1\x93"
                "\x51\x8a\x1c\x5c\xac\x96\x66\x0d\xda\x76\x7e\x7b\x87\x68\x0c"
                "\xff\xff\xa1\xf0\xe3\x12\xe2\x43\x2a\x72\x45\xee\x13\x59\x0f"
                "\x6a\xa9\x55\xc8\x7c\xa7\x17\x9a\xdd\xba\x47\x41\x2f\x79\xe6"
                "\x7c\x90\xbb\xc5\x9e\x55\x28\xc8\x29\x64\x0b\xe1\x8a\x4b\x24"
                "\x0d\x40\x2c\xb3\x53\xa0\x89\x9c\xc6\x6a\xd1\x6d\x98\xf9\xe1"
                "\x01\xa8\x1f\x02\xed\x61\xc5\xd2\xc0\x15\xd7\x01\x4a\x3f\x1c"
                "\xc5\x72\xcf\x24\x68\xab\xcf\x31\x4c\x1a\x11\x3f\x03\x40\x83"
                "\x94\xc5\x47\x02\xcd\x01\xe0\x19\xf4\x0a\xd4\x2b\xcc\x13\xfc"
                "\x76\xca\x91\x90\x67\x69\xdd\x39\x8d\x38\xb5\xf7\x51\xc6\x73"
                "\xce\xb7\x60\x54\x13\xbc\x7f\x8b\xa4\x48\x27\xf5\x5a\x1b\x59"
                "\x8a\x14\xa1\x79\xa0\x25\x80\x20\x5d\xa0\x30\xef\xfd\xe1\x75"
                "\x11\x51\xe8\x5f\x5e\x54\x2d\x6a\x5b\xd1\x5c\xc9\x33\x21\x2c"
                "\xe2\x68\xfc\xfd\x53\xee\x93\x58\xeb\x4e\x81\x01\x09\xa3\x27"
                "\x80\x20\x3c\x73\x38\xcf\x23\xc6\x31\x53\x28\xc4\x27\xf8\x95"
                "\x87\x99\x83\x2d\x35\x3c\x03\x9b\xd1\xff\xff\x2e\x53\x20\xe9"
                "\x5e\x62\xb9\xb7\x81\x03\x01\x00\x00\xa4\x27\x80\x20\x41\x80"
                "\x08\xb2\x60\x74\x57\x6d\xac\xed\x74\x7f\x54\xdb\x96\x18\x91"
                "\x06\x0a\x95\xa1\x49\x17\xc7\x65\xe3\x94\xc8\x5e\x2c\x92\x20"
                "\x81\x03\x02\x00\x00"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\x3d\x2d\x3f\x23\xc6\xf1\x4c\x82\xa4\x27\x52"
                "\xfa\xd8\x0a\x14\xe4\x38\x8d\x13\x31\x42\x35\xda\x1f\x48\x09"
                "\x85\x07\x5e\x2b\x19\x49\x81\x03\x04\x2c\x20\x82\x02\x03\xb8"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\xfc\x57\x44\x31\xba\xd7\x2d\x95\xfa\xeb\xfc\x0c\x3a\x5c\x94"
                "\xfd\x56\xe0\x8d\x57\x03\xce\xd5\xac\x29\xf6\x8c\x2e\xbf\x5c"
                "\xe3\x38\x81\x03\x04\x28\x10\x82\x02\x03\xb8"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix13()
    {
        testcase("Prefix13");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** thresh2
        // **** Preim9Cond
        // **** Rsa10Cond
        // **** Ed11Cond
        // **** Thresh12Cond
        // **** rsa3
        // **** thresh4
        // ***** Preim6Cond
        // ***** Rsa7Cond
        // ***** Ed8Cond
        // ***** rsa5

        auto const rsa3Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 256> const rsa3PublicKey{
            {0xbd, 0xd1, 0xc7, 0xf0, 0xb0, 0x3a, 0xa5, 0x5b, 0x3e, 0x49, 0x8d,
             0x4e, 0x00, 0x54, 0x89, 0xb9, 0x89, 0xcd, 0x4b, 0x43, 0xde, 0x59,
             0xf6, 0x7a, 0x67, 0x5c, 0x3a, 0xc6, 0xcf, 0x82, 0x3f, 0x35, 0x9c,
             0xcc, 0xda, 0xcd, 0xd3, 0x97, 0x86, 0x5b, 0xe9, 0xf6, 0x05, 0x55,
             0x0b, 0x26, 0xef, 0x1e, 0x88, 0xd5, 0xb6, 0xba, 0x14, 0x0a, 0xb2,
             0x76, 0xb9, 0xb3, 0x46, 0x0c, 0xc0, 0x80, 0x17, 0x13, 0x68, 0x23,
             0xdc, 0xec, 0x10, 0x18, 0xfc, 0xaa, 0xbe, 0xb3, 0xc4, 0xc7, 0xa9,
             0x84, 0xa6, 0x4e, 0x5c, 0x08, 0x6b, 0x7b, 0x4c, 0x81, 0x91, 0x79,
             0x5d, 0x90, 0x06, 0x15, 0xbb, 0x76, 0x2f, 0x5c, 0x53, 0x60, 0x0f,
             0xac, 0xf3, 0x7c, 0x49, 0xc5, 0x47, 0xec, 0xb3, 0xda, 0x93, 0x87,
             0xc1, 0xb9, 0xcf, 0x2c, 0xb5, 0xf0, 0x85, 0xad, 0xb4, 0x38, 0x67,
             0x88, 0xda, 0x3d, 0xfa, 0x01, 0xb7, 0x54, 0xd9, 0x41, 0x0b, 0x7b,
             0x8a, 0x09, 0xe0, 0x84, 0x7d, 0xbb, 0x89, 0xb2, 0xfc, 0x0b, 0x70,
             0x36, 0x93, 0x56, 0x62, 0xcc, 0xb4, 0xfc, 0xf9, 0x1f, 0x37, 0x92,
             0x9b, 0x3a, 0x4e, 0x7c, 0xad, 0x4b, 0xa6, 0x76, 0x6f, 0xda, 0xc4,
             0x2f, 0x83, 0x53, 0xbd, 0x93, 0xa9, 0x76, 0x89, 0x53, 0xe1, 0x4d,
             0xee, 0x27, 0x11, 0x6f, 0xbc, 0x21, 0xad, 0x42, 0x9f, 0x29, 0xf6,
             0x03, 0xdd, 0xec, 0xfa, 0xa1, 0x78, 0xd2, 0xde, 0x29, 0x2e, 0xd8,
             0x3a, 0x7f, 0xe9, 0x9b, 0x5d, 0xeb, 0x37, 0xb8, 0xb0, 0xa0, 0x66,
             0x3f, 0x02, 0xcd, 0x2a, 0x6e, 0xd3, 0x1c, 0xa5, 0x65, 0xdc, 0x73,
             0xbe, 0x93, 0x54, 0x9a, 0x2b, 0xf8, 0x32, 0x8b, 0xe8, 0xce, 0x9a,
             0x59, 0xd0, 0x05, 0xeb, 0xbb, 0xac, 0xfc, 0x4c, 0x4b, 0x2e, 0xac,
             0x2a, 0xc3, 0x0f, 0x0a, 0xd7, 0x46, 0xaf, 0xfd, 0x22, 0x0d, 0x0d,
             0x54, 0xcc, 0x2f}};
        std::array<std::uint8_t, 256> const rsa3Sig{
            {0x70, 0xc6, 0x53, 0xde, 0x72, 0xce, 0x3d, 0xba, 0x89, 0xe0, 0x1c,
             0xc6, 0xaf, 0x4a, 0xa3, 0xf0, 0x52, 0x41, 0x81, 0x9c, 0xad, 0x38,
             0xc5, 0xc2, 0x2a, 0x20, 0x72, 0xd6, 0xfa, 0x9c, 0x80, 0xbf, 0xfc,
             0x30, 0xd1, 0xa7, 0x6e, 0xa8, 0xa7, 0x2d, 0xb8, 0x63, 0xe0, 0xee,
             0x0e, 0xc9, 0x5d, 0x11, 0x4d, 0x2d, 0x7d, 0x0c, 0xab, 0x70, 0x5a,
             0x87, 0xcc, 0x05, 0x35, 0x4a, 0x97, 0xa1, 0x71, 0x17, 0x36, 0x5e,
             0x32, 0x52, 0x77, 0xf7, 0xc0, 0xc3, 0xe9, 0x0b, 0x75, 0xa5, 0xb4,
             0xe3, 0xa9, 0x6c, 0x4b, 0x26, 0x42, 0x13, 0xde, 0xb4, 0x24, 0x36,
             0xc9, 0x63, 0x04, 0xcc, 0x38, 0x1e, 0x6d, 0x52, 0x31, 0x50, 0x31,
             0x6b, 0x76, 0x55, 0x35, 0x16, 0x11, 0x4c, 0xd0, 0xb0, 0xb9, 0x93,
             0x2c, 0x8c, 0x5e, 0x50, 0xd5, 0x4d, 0xc9, 0xbd, 0x3d, 0x85, 0xbe,
             0xc5, 0x53, 0xa0, 0x71, 0x67, 0x93, 0xdb, 0x72, 0x28, 0x1f, 0x3c,
             0x4e, 0x81, 0x59, 0x21, 0x87, 0x5e, 0x42, 0x4f, 0xbb, 0x7d, 0xde,
             0x40, 0x2c, 0x75, 0xe4, 0xb1, 0x8d, 0xb8, 0x1c, 0xfd, 0x70, 0x87,
             0x49, 0xe9, 0x4a, 0x75, 0x3d, 0x98, 0xf5, 0xb1, 0x6c, 0x55, 0x0a,
             0x7a, 0x51, 0xce, 0xe0, 0x4a, 0x80, 0x0e, 0x42, 0xee, 0x72, 0x70,
             0xee, 0xe2, 0xd6, 0x97, 0xe8, 0x77, 0xa6, 0xc0, 0xa4, 0xe4, 0x92,
             0xa7, 0x00, 0x85, 0x04, 0x13, 0x67, 0xcc, 0xff, 0x56, 0x71, 0xb6,
             0xa1, 0x8d, 0x17, 0x34, 0x01, 0x1d, 0x46, 0xff, 0xbb, 0x01, 0x57,
             0x88, 0xb5, 0xd4, 0xd5, 0x87, 0x7c, 0x9b, 0xf8, 0xd0, 0xfe, 0x01,
             0x15, 0xa8, 0x41, 0x80, 0x31, 0xd1, 0xf0, 0xfe, 0x6d, 0xcc, 0xb8,
             0x7b, 0x99, 0xed, 0x8f, 0xeb, 0x66, 0x08, 0x78, 0x71, 0x98, 0xea,
             0x6b, 0xb1, 0x27, 0xda, 0xba, 0x3d, 0x53, 0x91, 0x69, 0x08, 0xe9,
             0xee, 0x54, 0xe6}};
        auto const rsa5Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 256> const rsa5PublicKey{
            {0xc0, 0x00, 0xef, 0x8f, 0x4b, 0x81, 0x10, 0x1e, 0x52, 0xe0, 0x07,
             0x9f, 0x68, 0xe7, 0x2f, 0x92, 0xd4, 0x77, 0x3c, 0x1f, 0xa3, 0xff,
             0x72, 0x64, 0x5b, 0x37, 0xf1, 0xf3, 0xa3, 0xc5, 0xfb, 0xcd, 0xfb,
             0xda, 0xcc, 0x8b, 0x52, 0xe1, 0xde, 0xbc, 0x28, 0x8d, 0xe5, 0xad,
             0xab, 0x86, 0x61, 0x45, 0x97, 0x65, 0x37, 0x68, 0x26, 0x21, 0x92,
             0x17, 0xa3, 0xb0, 0x74, 0x5c, 0x8a, 0x45, 0x8d, 0x87, 0x5b, 0x9b,
             0xd1, 0x7b, 0x07, 0xc4, 0x8c, 0x67, 0xa0, 0xe9, 0x82, 0x0c, 0xe0,
             0x6b, 0xea, 0x91, 0x5c, 0xba, 0xe3, 0xd9, 0x9d, 0x39, 0xfd, 0x77,
             0xac, 0xcb, 0x33, 0x9b, 0x28, 0x51, 0x8d, 0xbf, 0x3e, 0xe4, 0x94,
             0x1c, 0x9a, 0x60, 0x71, 0x4b, 0x34, 0x07, 0x30, 0xda, 0x42, 0x46,
             0x0e, 0xb8, 0xb7, 0x2c, 0xf5, 0x2f, 0x4b, 0x9e, 0xe7, 0x64, 0x81,
             0xa1, 0xa2, 0x05, 0x66, 0x92, 0xe6, 0x75, 0x9f, 0x37, 0xae, 0x40,
             0xa9, 0x16, 0x08, 0x19, 0xe8, 0xdc, 0x47, 0xd6, 0x03, 0x29, 0xab,
             0xcc, 0x58, 0xa2, 0x37, 0x2a, 0x32, 0xb8, 0x15, 0xc7, 0x51, 0x91,
             0x73, 0xb9, 0x1d, 0xc6, 0xd0, 0x4f, 0x85, 0x86, 0xd5, 0xb3, 0x21,
             0x1a, 0x2a, 0x6c, 0xeb, 0x7f, 0xfe, 0x84, 0x17, 0x10, 0x2d, 0x0e,
             0xb4, 0xe1, 0xc2, 0x48, 0x4c, 0x3f, 0x61, 0xc7, 0x59, 0x75, 0xa7,
             0xc1, 0x75, 0xce, 0x67, 0x17, 0x42, 0x2a, 0x2f, 0x96, 0xef, 0x8a,
             0x2d, 0x74, 0xd2, 0x13, 0x68, 0xe1, 0xe9, 0xea, 0xfb, 0x73, 0x68,
             0xed, 0x8d, 0xd3, 0xac, 0x49, 0x09, 0xf9, 0xec, 0x62, 0xdf, 0x53,
             0xab, 0xfe, 0x90, 0x64, 0x4b, 0x92, 0x60, 0x0d, 0xdd, 0x00, 0xfe,
             0x02, 0xe6, 0xf3, 0x9b, 0x2b, 0xac, 0x4f, 0x70, 0xe8, 0x5b, 0x69,
             0x9c, 0x40, 0xd3, 0xeb, 0x37, 0xad, 0x6f, 0x37, 0xab, 0xf3, 0x79,
             0x8e, 0xcb, 0x1d}};
        std::array<std::uint8_t, 256> const rsa5Sig{
            {0x86, 0x08, 0x9d, 0x96, 0x92, 0x63, 0x3b, 0x79, 0x7e, 0x06, 0xf6,
             0x35, 0xa3, 0xdd, 0x5b, 0xbc, 0x01, 0x1d, 0x11, 0x6d, 0x9f, 0x92,
             0x57, 0x8a, 0x95, 0x9f, 0x55, 0x44, 0x91, 0x18, 0x02, 0xc0, 0xe3,
             0x3f, 0xdc, 0x2a, 0x2b, 0xfd, 0xda, 0x04, 0xdb, 0xd0, 0x22, 0xe3,
             0x14, 0x9d, 0x1d, 0xd5, 0x17, 0x4d, 0x09, 0x51, 0x1b, 0xd0, 0x37,
             0xa8, 0xaf, 0xc4, 0x30, 0xe6, 0x11, 0x2b, 0xe5, 0x8b, 0x37, 0xda,
             0xfb, 0x1a, 0x6b, 0xe4, 0x96, 0x54, 0xc5, 0xca, 0xf8, 0x5e, 0x78,
             0xb9, 0x61, 0x88, 0xbc, 0xe3, 0x24, 0x86, 0x0c, 0xf7, 0xe6, 0xc9,
             0x75, 0x90, 0x13, 0x8d, 0x6e, 0x2c, 0x2b, 0x39, 0xe0, 0x48, 0x99,
             0x00, 0x76, 0x6a, 0xb6, 0x73, 0x0f, 0x05, 0xd6, 0x95, 0x21, 0x49,
             0xe0, 0x7f, 0x4a, 0xba, 0x6d, 0xc9, 0x97, 0xe5, 0x89, 0x2a, 0x15,
             0xd9, 0xf5, 0x7f, 0x0d, 0x87, 0x81, 0x3d, 0xbe, 0xc1, 0x1e, 0x03,
             0xea, 0x31, 0x1f, 0x9d, 0xa6, 0x7e, 0x40, 0x37, 0xcf, 0x4a, 0x11,
             0x3b, 0x0a, 0x3b, 0x64, 0xb2, 0x65, 0x1d, 0x1e, 0xf2, 0xd5, 0x78,
             0xe1, 0xb8, 0x92, 0xa3, 0xc7, 0xc3, 0xe9, 0x8e, 0x51, 0xa2, 0x47,
             0xb9, 0x5e, 0x9a, 0x61, 0x81, 0x2e, 0x19, 0xf4, 0x06, 0xd3, 0xf0,
             0xc2, 0x89, 0xf7, 0x73, 0xbb, 0x18, 0x4d, 0x23, 0x2e, 0xce, 0x22,
             0x1d, 0x0f, 0x9c, 0xff, 0x00, 0x5f, 0xef, 0x38, 0x36, 0xc4, 0xac,
             0xfc, 0x1f, 0x81, 0x6c, 0x87, 0x8d, 0xaa, 0xca, 0x02, 0x70, 0x42,
             0x7a, 0xbd, 0x72, 0xeb, 0xf9, 0xba, 0x76, 0x7f, 0xdf, 0x93, 0x66,
             0xea, 0x49, 0x77, 0xd6, 0x75, 0xa6, 0x5b, 0x5f, 0x61, 0x04, 0xeb,
             0x28, 0x87, 0xaf, 0x39, 0x78, 0xbe, 0x03, 0xd6, 0x5d, 0x2b, 0x61,
             0x52, 0x23, 0x50, 0xaa, 0xd2, 0xf1, 0x85, 0xc7, 0x6b, 0x3e, 0x47,
             0x60, 0x75, 0x5e}};
        auto const thresh4Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim6CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim6Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim6CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa7CondConditionFingerprint =
            "\x6c\x7b\xea\x83\xa1\xf4\x82\x3d\x36\xe7\x6e\xae\x1a\xbc\xa0"
            "\xba\x90\x3d\x96\xc1\xe6\xad\x3a\x47\xa5\xcb\x88\xab\x3c\x5f"
            "\xcc\xd5"s;
        Condition const Rsa7Cond{Type::rsaSha256,
                                 65536,
                                 makeSlice(Rsa7CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const Ed8CondConditionFingerprint =
            "\xf1\x68\x96\xa6\x2e\xef\x7f\x47\x06\x51\x4c\xc6\x7e\x24\xf7"
            "\x29\x84\x9c\xd6\xb0\xd9\x4b\xd9\x0f\xc9\x34\x01\x9d\x92\xeb"
            "\xbc\x0a"s;
        Condition const Ed8Cond{Type::ed25519Sha256,
                                131072,
                                makeSlice(Ed8CondConditionFingerprint),
                                std::bitset<5>{0}};
        auto const thresh2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim9CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim9Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim9CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa10CondConditionFingerprint =
            "\x3c\x73\x38\xcf\x23\xc6\x31\x53\x28\xc4\x27\xf8\x95\x87\x99"
            "\x83\x2d\x35\x3c\x03\x9b\xd1\xff\xff\x2e\x53\x20\xe9\x5e\x62"
            "\xb9\xb7"s;
        Condition const Rsa10Cond{Type::rsaSha256,
                                  65536,
                                  makeSlice(Rsa10CondConditionFingerprint),
                                  std::bitset<5>{0}};
        auto const Ed11CondConditionFingerprint =
            "\x41\x80\x08\xb2\x60\x74\x57\x6d\xac\xed\x74\x7f\x54\xdb\x96"
            "\x18\x91\x06\x0a\x95\xa1\x49\x17\xc7\x65\xe3\x94\xc8\x5e\x2c"
            "\x92\x20"s;
        Condition const Ed11Cond{Type::ed25519Sha256,
                                 131072,
                                 makeSlice(Ed11CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const Thresh12CondConditionFingerprint =
            "\x03\xb7\x57\xb9\x56\x68\xc6\x36\x20\x05\x2b\xd6\x6f\x92\x23"
            "\x03\x30\xa8\x6f\x6e\xd9\x64\x9c\xd0\xc2\x02\x89\xc3\xcf\xe9"
            "\xce\x85"s;
        Condition const Thresh12Cond{
            Type::thresholdSha256,
            135168,
            makeSlice(Thresh12CondConditionFingerprint),
            std::bitset<5>{25}};
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto rsa3 = std::make_unique<RsaSha256>(
            makeSlice(rsa3PublicKey), makeSlice(rsa3Sig));
        auto rsa5 = std::make_unique<RsaSha256>(
            makeSlice(rsa5PublicKey), makeSlice(rsa5Sig));
        std::vector<std::unique_ptr<Fulfillment>> thresh4Subfulfillments;
        thresh4Subfulfillments.emplace_back(std::move(rsa5));
        std::vector<Condition> thresh4Subconditions{
            {Preim6Cond, Rsa7Cond, Ed8Cond}};
        auto thresh4 = std::make_unique<ThresholdSha256>(
            std::move(thresh4Subfulfillments), std::move(thresh4Subconditions));
        std::vector<std::unique_ptr<Fulfillment>> thresh2Subfulfillments;
        thresh2Subfulfillments.emplace_back(std::move(rsa3));
        thresh2Subfulfillments.emplace_back(std::move(thresh4));
        std::vector<Condition> thresh2Subconditions{
            {Preim9Cond, Rsa10Cond, Ed11Cond, Thresh12Cond}};
        auto thresh2 = std::make_unique<ThresholdSha256>(
            std::move(thresh2Subfulfillments), std::move(thresh2Subconditions));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(thresh2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x82\x05\x66\x80\x02\x50\x30\x81\x01\x0e\xa2\x82\x05\x5b"
                "\xa1\x82\x05\x57\x80\x02\x50\x31\x81\x01\x0e\xa2\x82\x05\x4c"
                "\xa2\x82\x05\x48\xa0\x82\x04\x9b\xa2\x82\x02\x8b\xa0\x82\x02"
                "\x0c\xa3\x82\x02\x08\x80\x82\x01\x00\xc0\x00\xef\x8f\x4b\x81"
                "\x10\x1e\x52\xe0\x07\x9f\x68\xe7\x2f\x92\xd4\x77\x3c\x1f\xa3"
                "\xff\x72\x64\x5b\x37\xf1\xf3\xa3\xc5\xfb\xcd\xfb\xda\xcc\x8b"
                "\x52\xe1\xde\xbc\x28\x8d\xe5\xad\xab\x86\x61\x45\x97\x65\x37"
                "\x68\x26\x21\x92\x17\xa3\xb0\x74\x5c\x8a\x45\x8d\x87\x5b\x9b"
                "\xd1\x7b\x07\xc4\x8c\x67\xa0\xe9\x82\x0c\xe0\x6b\xea\x91\x5c"
                "\xba\xe3\xd9\x9d\x39\xfd\x77\xac\xcb\x33\x9b\x28\x51\x8d\xbf"
                "\x3e\xe4\x94\x1c\x9a\x60\x71\x4b\x34\x07\x30\xda\x42\x46\x0e"
                "\xb8\xb7\x2c\xf5\x2f\x4b\x9e\xe7\x64\x81\xa1\xa2\x05\x66\x92"
                "\xe6\x75\x9f\x37\xae\x40\xa9\x16\x08\x19\xe8\xdc\x47\xd6\x03"
                "\x29\xab\xcc\x58\xa2\x37\x2a\x32\xb8\x15\xc7\x51\x91\x73\xb9"
                "\x1d\xc6\xd0\x4f\x85\x86\xd5\xb3\x21\x1a\x2a\x6c\xeb\x7f\xfe"
                "\x84\x17\x10\x2d\x0e\xb4\xe1\xc2\x48\x4c\x3f\x61\xc7\x59\x75"
                "\xa7\xc1\x75\xce\x67\x17\x42\x2a\x2f\x96\xef\x8a\x2d\x74\xd2"
                "\x13\x68\xe1\xe9\xea\xfb\x73\x68\xed\x8d\xd3\xac\x49\x09\xf9"
                "\xec\x62\xdf\x53\xab\xfe\x90\x64\x4b\x92\x60\x0d\xdd\x00\xfe"
                "\x02\xe6\xf3\x9b\x2b\xac\x4f\x70\xe8\x5b\x69\x9c\x40\xd3\xeb"
                "\x37\xad\x6f\x37\xab\xf3\x79\x8e\xcb\x1d\x81\x82\x01\x00\x86"
                "\x08\x9d\x96\x92\x63\x3b\x79\x7e\x06\xf6\x35\xa3\xdd\x5b\xbc"
                "\x01\x1d\x11\x6d\x9f\x92\x57\x8a\x95\x9f\x55\x44\x91\x18\x02"
                "\xc0\xe3\x3f\xdc\x2a\x2b\xfd\xda\x04\xdb\xd0\x22\xe3\x14\x9d"
                "\x1d\xd5\x17\x4d\x09\x51\x1b\xd0\x37\xa8\xaf\xc4\x30\xe6\x11"
                "\x2b\xe5\x8b\x37\xda\xfb\x1a\x6b\xe4\x96\x54\xc5\xca\xf8\x5e"
                "\x78\xb9\x61\x88\xbc\xe3\x24\x86\x0c\xf7\xe6\xc9\x75\x90\x13"
                "\x8d\x6e\x2c\x2b\x39\xe0\x48\x99\x00\x76\x6a\xb6\x73\x0f\x05"
                "\xd6\x95\x21\x49\xe0\x7f\x4a\xba\x6d\xc9\x97\xe5\x89\x2a\x15"
                "\xd9\xf5\x7f\x0d\x87\x81\x3d\xbe\xc1\x1e\x03\xea\x31\x1f\x9d"
                "\xa6\x7e\x40\x37\xcf\x4a\x11\x3b\x0a\x3b\x64\xb2\x65\x1d\x1e"
                "\xf2\xd5\x78\xe1\xb8\x92\xa3\xc7\xc3\xe9\x8e\x51\xa2\x47\xb9"
                "\x5e\x9a\x61\x81\x2e\x19\xf4\x06\xd3\xf0\xc2\x89\xf7\x73\xbb"
                "\x18\x4d\x23\x2e\xce\x22\x1d\x0f\x9c\xff\x00\x5f\xef\x38\x36"
                "\xc4\xac\xfc\x1f\x81\x6c\x87\x8d\xaa\xca\x02\x70\x42\x7a\xbd"
                "\x72\xeb\xf9\xba\x76\x7f\xdf\x93\x66\xea\x49\x77\xd6\x75\xa6"
                "\x5b\x5f\x61\x04\xeb\x28\x87\xaf\x39\x78\xbe\x03\xd6\x5d\x2b"
                "\x61\x52\x23\x50\xaa\xd2\xf1\x85\xc7\x6b\x3e\x47\x60\x75\x5e"
                "\xa1\x79\xa0\x25\x80\x20\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51"
                "\xe8\x5f\x5e\x54\x2d\x6a\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68"
                "\xfc\xfd\x53\xee\x93\x58\xeb\x4e\x81\x01\x09\xa3\x27\x80\x20"
                "\x6c\x7b\xea\x83\xa1\xf4\x82\x3d\x36\xe7\x6e\xae\x1a\xbc\xa0"
                "\xba\x90\x3d\x96\xc1\xe6\xad\x3a\x47\xa5\xcb\x88\xab\x3c\x5f"
                "\xcc\xd5\x81\x03\x01\x00\x00\xa4\x27\x80\x20\xf1\x68\x96\xa6"
                "\x2e\xef\x7f\x47\x06\x51\x4c\xc6\x7e\x24\xf7\x29\x84\x9c\xd6"
                "\xb0\xd9\x4b\xd9\x0f\xc9\x34\x01\x9d\x92\xeb\xbc\x0a\x81\x03"
                "\x02\x00\x00\xa3\x82\x02\x08\x80\x82\x01\x00\xbd\xd1\xc7\xf0"
                "\xb0\x3a\xa5\x5b\x3e\x49\x8d\x4e\x00\x54\x89\xb9\x89\xcd\x4b"
                "\x43\xde\x59\xf6\x7a\x67\x5c\x3a\xc6\xcf\x82\x3f\x35\x9c\xcc"
                "\xda\xcd\xd3\x97\x86\x5b\xe9\xf6\x05\x55\x0b\x26\xef\x1e\x88"
                "\xd5\xb6\xba\x14\x0a\xb2\x76\xb9\xb3\x46\x0c\xc0\x80\x17\x13"
                "\x68\x23\xdc\xec\x10\x18\xfc\xaa\xbe\xb3\xc4\xc7\xa9\x84\xa6"
                "\x4e\x5c\x08\x6b\x7b\x4c\x81\x91\x79\x5d\x90\x06\x15\xbb\x76"
                "\x2f\x5c\x53\x60\x0f\xac\xf3\x7c\x49\xc5\x47\xec\xb3\xda\x93"
                "\x87\xc1\xb9\xcf\x2c\xb5\xf0\x85\xad\xb4\x38\x67\x88\xda\x3d"
                "\xfa\x01\xb7\x54\xd9\x41\x0b\x7b\x8a\x09\xe0\x84\x7d\xbb\x89"
                "\xb2\xfc\x0b\x70\x36\x93\x56\x62\xcc\xb4\xfc\xf9\x1f\x37\x92"
                "\x9b\x3a\x4e\x7c\xad\x4b\xa6\x76\x6f\xda\xc4\x2f\x83\x53\xbd"
                "\x93\xa9\x76\x89\x53\xe1\x4d\xee\x27\x11\x6f\xbc\x21\xad\x42"
                "\x9f\x29\xf6\x03\xdd\xec\xfa\xa1\x78\xd2\xde\x29\x2e\xd8\x3a"
                "\x7f\xe9\x9b\x5d\xeb\x37\xb8\xb0\xa0\x66\x3f\x02\xcd\x2a\x6e"
                "\xd3\x1c\xa5\x65\xdc\x73\xbe\x93\x54\x9a\x2b\xf8\x32\x8b\xe8"
                "\xce\x9a\x59\xd0\x05\xeb\xbb\xac\xfc\x4c\x4b\x2e\xac\x2a\xc3"
                "\x0f\x0a\xd7\x46\xaf\xfd\x22\x0d\x0d\x54\xcc\x2f\x81\x82\x01"
                "\x00\x70\xc6\x53\xde\x72\xce\x3d\xba\x89\xe0\x1c\xc6\xaf\x4a"
                "\xa3\xf0\x52\x41\x81\x9c\xad\x38\xc5\xc2\x2a\x20\x72\xd6\xfa"
                "\x9c\x80\xbf\xfc\x30\xd1\xa7\x6e\xa8\xa7\x2d\xb8\x63\xe0\xee"
                "\x0e\xc9\x5d\x11\x4d\x2d\x7d\x0c\xab\x70\x5a\x87\xcc\x05\x35"
                "\x4a\x97\xa1\x71\x17\x36\x5e\x32\x52\x77\xf7\xc0\xc3\xe9\x0b"
                "\x75\xa5\xb4\xe3\xa9\x6c\x4b\x26\x42\x13\xde\xb4\x24\x36\xc9"
                "\x63\x04\xcc\x38\x1e\x6d\x52\x31\x50\x31\x6b\x76\x55\x35\x16"
                "\x11\x4c\xd0\xb0\xb9\x93\x2c\x8c\x5e\x50\xd5\x4d\xc9\xbd\x3d"
                "\x85\xbe\xc5\x53\xa0\x71\x67\x93\xdb\x72\x28\x1f\x3c\x4e\x81"
                "\x59\x21\x87\x5e\x42\x4f\xbb\x7d\xde\x40\x2c\x75\xe4\xb1\x8d"
                "\xb8\x1c\xfd\x70\x87\x49\xe9\x4a\x75\x3d\x98\xf5\xb1\x6c\x55"
                "\x0a\x7a\x51\xce\xe0\x4a\x80\x0e\x42\xee\x72\x70\xee\xe2\xd6"
                "\x97\xe8\x77\xa6\xc0\xa4\xe4\x92\xa7\x00\x85\x04\x13\x67\xcc"
                "\xff\x56\x71\xb6\xa1\x8d\x17\x34\x01\x1d\x46\xff\xbb\x01\x57"
                "\x88\xb5\xd4\xd5\x87\x7c\x9b\xf8\xd0\xfe\x01\x15\xa8\x41\x80"
                "\x31\xd1\xf0\xfe\x6d\xcc\xb8\x7b\x99\xed\x8f\xeb\x66\x08\x78"
                "\x71\x98\xea\x6b\xb1\x27\xda\xba\x3d\x53\x91\x69\x08\xe9\xee"
                "\x54\xe6\xa1\x81\xa6\xa0\x25\x80\x20\x5d\xa0\x30\xef\xfd\xe1"
                "\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a\x5b\xd1\x5c\xc9\x33\x21"
                "\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58\xeb\x4e\x81\x01\x09\xa2"
                "\x2b\x80\x20\x03\xb7\x57\xb9\x56\x68\xc6\x36\x20\x05\x2b\xd6"
                "\x6f\x92\x23\x03\x30\xa8\x6f\x6e\xd9\x64\x9c\xd0\xc2\x02\x89"
                "\xc3\xcf\xe9\xce\x85\x81\x03\x02\x10\x00\x82\x02\x03\x98\xa3"
                "\x27\x80\x20\x3c\x73\x38\xcf\x23\xc6\x31\x53\x28\xc4\x27\xf8"
                "\x95\x87\x99\x83\x2d\x35\x3c\x03\x9b\xd1\xff\xff\x2e\x53\x20"
                "\xe9\x5e\x62\xb9\xb7\x81\x03\x01\x00\x00\xa4\x27\x80\x20\x41"
                "\x80\x08\xb2\x60\x74\x57\x6d\xac\xed\x74\x7f\x54\xdb\x96\x18"
                "\x91\x06\x0a\x95\xa1\x49\x17\xc7\x65\xe3\x94\xc8\x5e\x2c\x92"
                "\x20\x81\x03\x02\x00\x00"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\x95\x13\xee\x55\x02\x8a\x6c\x0d\xc9\x1c\x2c"
                "\xaf\xa3\x5f\x06\xf2\x9d\x46\xe6\xfe\x3f\x9f\x63\x66\xff\x0a"
                "\x5a\x28\x91\x1f\xe5\xca\x81\x03\x04\x40\x20\x82\x02\x03\xb8"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\x01\xc0\xb5\xd8\xa7\x80\x84\x80\x29\xa9\xbe\x52\x62\xaf\xb6"
                "\x50\xa6\x3e\x77\xc1\xb2\xc4\x17\x4f\x50\xda\x3e\x8b\x12\xd1"
                "\xc7\xaa\x81\x03\x04\x3c\x10\x82\x02\x03\xb8"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix14()
    {
        testcase("Prefix14");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** ed1

        auto const ed1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 32> const ed1PublicKey{
            {0x63, 0x99, 0x70, 0x0c, 0xa9, 0x50, 0x9c, 0xd4, 0xf5, 0x06, 0xdd,
             0x19, 0xc8, 0xa1, 0xd2, 0x9e, 0x03, 0x10, 0xb9, 0x89, 0x2e, 0x02,
             0x34, 0x62, 0x2d, 0x23, 0xca, 0xa1, 0x1d, 0xde, 0x23, 0x6a}};
        std::array<std::uint8_t, 64> const ed1Sig{
            {0x6c, 0x25, 0xd0, 0x01, 0x10, 0x29, 0x70, 0x01, 0x05, 0xc0, 0xfb,
             0x51, 0xd8, 0x59, 0x5b, 0x7b, 0x83, 0x32, 0xd6, 0x18, 0x63, 0x35,
             0x07, 0xcf, 0xdf, 0x63, 0x90, 0x39, 0xd1, 0x24, 0x42, 0x8a, 0xc0,
             0xd1, 0xac, 0xd3, 0x55, 0xf6, 0x5c, 0x96, 0xda, 0x7b, 0x2e, 0xaa,
             0xfc, 0x72, 0x1b, 0x1e, 0x2e, 0xb2, 0x97, 0xad, 0x8d, 0x04, 0x32,
             0xd9, 0x9c, 0xb7, 0xf4, 0x32, 0x38, 0x4e, 0xd7, 0x0c}};
        std::array<std::uint8_t, 32> const ed1SigningKey{
            {0xe9, 0x20, 0xaa, 0x41, 0x73, 0x35, 0x2f, 0xae, 0xa2, 0x4b, 0x4b,
             0x19, 0x64, 0xda, 0xc0, 0xd5, 0x7b, 0xfd, 0x99, 0x06, 0x90, 0x80,
             0x48, 0xe4, 0x6a, 0xc4, 0x86, 0x30, 0x7d, 0x53, 0x37, 0xb9}};
        (void)ed1SigningKey;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto ed1 = std::make_unique<Ed25519>(ed1PublicKey, ed1Sig);
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(ed1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x6f\x80\x02\x50\x30\x81\x01\x0e\xa2\x66\xa4\x64\x80\x20"
                "\x63\x99\x70\x0c\xa9\x50\x9c\xd4\xf5\x06\xdd\x19\xc8\xa1\xd2"
                "\x9e\x03\x10\xb9\x89\x2e\x02\x34\x62\x2d\x23\xca\xa1\x1d\xde"
                "\x23\x6a\x81\x40\x6c\x25\xd0\x01\x10\x29\x70\x01\x05\xc0\xfb"
                "\x51\xd8\x59\x5b\x7b\x83\x32\xd6\x18\x63\x35\x07\xcf\xdf\x63"
                "\x90\x39\xd1\x24\x42\x8a\xc0\xd1\xac\xd3\x55\xf6\x5c\x96\xda"
                "\x7b\x2e\xaa\xfc\x72\x1b\x1e\x2e\xb2\x97\xad\x8d\x04\x32\xd9"
                "\x9c\xb7\xf4\x32\x38\x4e\xd7\x0c"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\x82\x35\x0b\x82\x9e\x34\xb7\x9e\xef\xce\x06"
                "\xba\x22\x10\x3a\x1f\xf3\x2f\x2b\xdf\x8e\xac\x18\x06\xc6\x5b"
                "\x49\xfb\xdc\x38\x93\xd6\x81\x03\x02\x04\x10\x82\x02\x03\x08"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x32\x80\x02\x50\x30\x81\x01\x0e\xa2\x29\xa4\x27\x80\x20"
                "\x42\x58\xea\xf4\xb3\xac\xde\x0b\xd9\x1e\x0c\x53\xe5\xae\x63"
                "\x84\xee\xc0\xf5\xcf\x88\xa7\x43\x7b\x05\x47\x87\xee\x73\x3e"
                "\xa3\x83\x81\x03\x02\x00\x00"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix15()
    {
        testcase("Prefix15");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** ed2

        auto const ed2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 32> const ed2PublicKey{
            {0xb1, 0x2f, 0x54, 0xbe, 0xb6, 0xf8, 0x76, 0x71, 0x72, 0xed, 0x44,
             0x03, 0x71, 0x74, 0x2d, 0x7f, 0x98, 0x10, 0x4b, 0x57, 0xf2, 0x45,
             0xfb, 0x3e, 0xea, 0xfd, 0xdd, 0x39, 0x42, 0xbf, 0x24, 0x4d}};
        std::array<std::uint8_t, 64> const ed2Sig{
            {0xbd, 0xa7, 0x4d, 0x84, 0x5f, 0xfd, 0x7c, 0x9b, 0xd0, 0xef, 0x41,
             0x2c, 0x54, 0x50, 0xd8, 0x26, 0xf0, 0x06, 0x61, 0x22, 0x5c, 0x90,
             0xc2, 0x72, 0x5e, 0x7a, 0x3b, 0x6f, 0xba, 0xa7, 0x22, 0x04, 0xa6,
             0x6c, 0x67, 0xbb, 0x58, 0x28, 0xf7, 0xd8, 0x0c, 0x8e, 0xd3, 0x62,
             0x66, 0x66, 0x2c, 0xe9, 0x71, 0xd0, 0x38, 0xab, 0x33, 0xfa, 0x10,
             0x10, 0x53, 0x4f, 0xf2, 0xf1, 0x97, 0x0d, 0xbd, 0x06}};
        std::array<std::uint8_t, 32> const ed2SigningKey{
            {0xa7, 0xeb, 0x15, 0xc5, 0x2a, 0x41, 0x59, 0xf9, 0xf7, 0xb4, 0x78,
             0x5f, 0xdb, 0x79, 0xe5, 0x5b, 0x16, 0x44, 0xf7, 0xc7, 0xcf, 0xe2,
             0x46, 0xc5, 0xb3, 0x54, 0x64, 0xb5, 0x2f, 0x6c, 0x8e, 0x8e}};
        (void)ed2SigningKey;
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto ed2 = std::make_unique<Ed25519>(ed2PublicKey, ed2Sig);
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(ed2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x7a\x80\x02\x50\x30\x81\x01\x0e\xa2\x71\xa1\x6f\x80\x02"
                "\x50\x31\x81\x01\x0e\xa2\x66\xa4\x64\x80\x20\xb1\x2f\x54\xbe"
                "\xb6\xf8\x76\x71\x72\xed\x44\x03\x71\x74\x2d\x7f\x98\x10\x4b"
                "\x57\xf2\x45\xfb\x3e\xea\xfd\xdd\x39\x42\xbf\x24\x4d\x81\x40"
                "\xbd\xa7\x4d\x84\x5f\xfd\x7c\x9b\xd0\xef\x41\x2c\x54\x50\xd8"
                "\x26\xf0\x06\x61\x22\x5c\x90\xc2\x72\x5e\x7a\x3b\x6f\xba\xa7"
                "\x22\x04\xa6\x6c\x67\xbb\x58\x28\xf7\xd8\x0c\x8e\xd3\x62\x66"
                "\x66\x2c\xe9\x71\xd0\x38\xab\x33\xfa\x10\x10\x53\x4f\xf2\xf1"
                "\x97\x0d\xbd\x06"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\x86\x8a\xe2\xb3\xfb\x2c\x75\x50\x77\xb1\x96"
                "\x01\x3b\x89\x92\x75\x69\x99\x38\x30\xa6\x15\x16\xa8\x10\x21"
                "\x85\x76\x1e\xca\x31\x72\x81\x03\x02\x08\x20\x82\x02\x03\x08"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\x2d\x35\x2a\x9a\xff\x16\x5a\x95\x64\x87\xf3\xa8\x86\x17\xe4"
                "\x53\xd4\xfc\x87\x72\x80\x9a\x74\x05\xa0\x2c\x5c\xbf\x8b\xdb"
                "\xaf\xae\x81\x03\x02\x04\x10\x82\x02\x03\x08"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix16()
    {
        testcase("Prefix16");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** prefix2
        // **** ed3

        auto const ed3Msg = "P2P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 32> const ed3PublicKey{
            {0x42, 0x0a, 0x60, 0x63, 0x9e, 0xca, 0xaa, 0x48, 0xac, 0xcb, 0x2c,
             0xd9, 0x46, 0x79, 0x69, 0x34, 0x35, 0xf3, 0x8e, 0x29, 0xd5, 0x4b,
             0x24, 0x57, 0x9d, 0xd7, 0x3b, 0x04, 0x5d, 0x9c, 0x2f, 0x32}};
        std::array<std::uint8_t, 64> const ed3Sig{
            {0x33, 0x01, 0x0b, 0xe8, 0x91, 0xa9, 0xa0, 0x9b, 0x54, 0x9a, 0xfc,
             0xde, 0xcd, 0xac, 0x70, 0x42, 0xeb, 0x1e, 0x41, 0x7f, 0xb8, 0xe0,
             0x62, 0x39, 0x8d, 0x55, 0xa8, 0xe8, 0xc5, 0xba, 0x2b, 0xfd, 0xf6,
             0xfb, 0xc9, 0x96, 0x83, 0x17, 0x54, 0x06, 0xa9, 0xb3, 0xa9, 0x64,
             0xe8, 0xd3, 0xa7, 0x92, 0xde, 0xb2, 0x4d, 0x85, 0x49, 0x2c, 0x2a,
             0xeb, 0x48, 0x1f, 0x7a, 0x33, 0x60, 0x83, 0x56, 0x0f}};
        std::array<std::uint8_t, 32> const ed3SigningKey{
            {0x1b, 0x90, 0xf2, 0x2c, 0xd5, 0x5d, 0xcd, 0xe5, 0x2d, 0x71, 0x84,
             0x66, 0x10, 0xf0, 0xf1, 0x91, 0xfe, 0xb5, 0xbe, 0x94, 0x85, 0xc1,
             0xc8, 0xf3, 0x61, 0x91, 0xc3, 0xa4, 0x41, 0x58, 0xcd, 0xfc}};
        (void)ed3SigningKey;
        auto const prefix2Prefix = "P2"s;
        auto const prefix2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix2MaxMsgLength = 14;
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto ed3 = std::make_unique<Ed25519>(ed3PublicKey, ed3Sig);
        auto prefix2 = std::make_unique<PrefixSha256>(
            makeSlice(prefix2Prefix), prefix2MaxMsgLength, std::move(ed3));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(prefix2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x81\x85\x80\x02\x50\x30\x81\x01\x0e\xa2\x7c\xa1\x7a\x80"
                "\x02\x50\x31\x81\x01\x0e\xa2\x71\xa1\x6f\x80\x02\x50\x32\x81"
                "\x01\x0e\xa2\x66\xa4\x64\x80\x20\x42\x0a\x60\x63\x9e\xca\xaa"
                "\x48\xac\xcb\x2c\xd9\x46\x79\x69\x34\x35\xf3\x8e\x29\xd5\x4b"
                "\x24\x57\x9d\xd7\x3b\x04\x5d\x9c\x2f\x32\x81\x40\x33\x01\x0b"
                "\xe8\x91\xa9\xa0\x9b\x54\x9a\xfc\xde\xcd\xac\x70\x42\xeb\x1e"
                "\x41\x7f\xb8\xe0\x62\x39\x8d\x55\xa8\xe8\xc5\xba\x2b\xfd\xf6"
                "\xfb\xc9\x96\x83\x17\x54\x06\xa9\xb3\xa9\x64\xe8\xd3\xa7\x92"
                "\xde\xb2\x4d\x85\x49\x2c\x2a\xeb\x48\x1f\x7a\x33\x60\x83\x56"
                "\x0f"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\x2f\x00\x20\xee\x05\x8f\x40\xe2\x56\x45\x32"
                "\xd9\xa9\xa3\x27\x18\x1b\x2e\x33\x89\x9f\xaa\x2a\x18\x65\x93"
                "\xe6\x36\x9a\x5e\xb7\xa7\x81\x03\x02\x0c\x30\x82\x02\x03\x08"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\xe9\xa9\x26\x1a\xaf\xf8\x2a\x49\x3a\x0e\x97\xca\x70\x81\x21"
                "\xd9\xad\xce\x28\xc7\x47\x76\x0d\x67\x89\x41\x06\x50\x63\xb8"
                "\x0c\x50\x81\x03\x02\x08\x20\x82\x02\x03\x08"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix17()
    {
        testcase("Prefix17");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** thresh2
        // **** ed3

        auto const ed3Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 32> const ed3PublicKey{
            {0x42, 0x0a, 0x60, 0x63, 0x9e, 0xca, 0xaa, 0x48, 0xac, 0xcb, 0x2c,
             0xd9, 0x46, 0x79, 0x69, 0x34, 0x35, 0xf3, 0x8e, 0x29, 0xd5, 0x4b,
             0x24, 0x57, 0x9d, 0xd7, 0x3b, 0x04, 0x5d, 0x9c, 0x2f, 0x32}};
        std::array<std::uint8_t, 64> const ed3Sig{
            {0x8e, 0xa4, 0xd9, 0x6a, 0x9b, 0x3e, 0x19, 0xe8, 0x89, 0xa4, 0x4e,
             0xf2, 0xa8, 0x1c, 0xc2, 0xbd, 0xd3, 0xe0, 0x6f, 0xe0, 0xd1, 0x6b,
             0x85, 0x47, 0x9a, 0x58, 0x2e, 0x9f, 0x38, 0x09, 0x1f, 0x6d, 0x02,
             0x9a, 0xf6, 0x9c, 0x60, 0x3f, 0x16, 0x6c, 0xa5, 0x0e, 0xfb, 0xa3,
             0x08, 0xd6, 0xb6, 0x97, 0x5f, 0x2e, 0x94, 0xfb, 0x49, 0x56, 0xbe,
             0x2c, 0x58, 0x48, 0x15, 0x49, 0x73, 0xa2, 0xae, 0x09}};
        std::array<std::uint8_t, 32> const ed3SigningKey{
            {0x1b, 0x90, 0xf2, 0x2c, 0xd5, 0x5d, 0xcd, 0xe5, 0x2d, 0x71, 0x84,
             0x66, 0x10, 0xf0, 0xf1, 0x91, 0xfe, 0xb5, 0xbe, 0x94, 0x85, 0xc1,
             0xc8, 0xf3, 0x61, 0x91, 0xc3, 0xa4, 0x41, 0x58, 0xcd, 0xfc}};
        (void)ed3SigningKey;
        auto const thresh2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto ed3 = std::make_unique<Ed25519>(ed3PublicKey, ed3Sig);
        std::vector<std::unique_ptr<Fulfillment>> thresh2Subfulfillments;
        thresh2Subfulfillments.emplace_back(std::move(ed3));
        std::vector<Condition> thresh2Subconditions{};
        auto thresh2 = std::make_unique<ThresholdSha256>(
            std::move(thresh2Subfulfillments), std::move(thresh2Subconditions));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(thresh2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x81\x80\x80\x02\x50\x30\x81\x01\x0e\xa2\x77\xa1\x75\x80"
                "\x02\x50\x31\x81\x01\x0e\xa2\x6c\xa2\x6a\xa0\x66\xa4\x64\x80"
                "\x20\x42\x0a\x60\x63\x9e\xca\xaa\x48\xac\xcb\x2c\xd9\x46\x79"
                "\x69\x34\x35\xf3\x8e\x29\xd5\x4b\x24\x57\x9d\xd7\x3b\x04\x5d"
                "\x9c\x2f\x32\x81\x40\x8e\xa4\xd9\x6a\x9b\x3e\x19\xe8\x89\xa4"
                "\x4e\xf2\xa8\x1c\xc2\xbd\xd3\xe0\x6f\xe0\xd1\x6b\x85\x47\x9a"
                "\x58\x2e\x9f\x38\x09\x1f\x6d\x02\x9a\xf6\x9c\x60\x3f\x16\x6c"
                "\xa5\x0e\xfb\xa3\x08\xd6\xb6\x97\x5f\x2e\x94\xfb\x49\x56\xbe"
                "\x2c\x58\x48\x15\x49\x73\xa2\xae\x09\xa1\x00"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\x57\xdf\xea\x2a\xb7\x65\xa0\x0b\x6d\x77\xa4"
                "\x5f\x1d\x73\xfa\xdc\x88\xa3\x5e\x35\x20\x91\xf2\x86\x8a\xb9"
                "\x9f\x55\x23\x6e\x7c\xab\x81\x03\x02\x0c\x20\x82\x02\x03\x28"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\x9e\x95\x7a\xc0\xd8\x94\x1f\x1a\xed\xe3\xe9\xe4\xb5\x2f\x36"
                "\x40\xe4\x6b\xd5\xe8\xc0\x46\x2f\x3b\x5f\xce\x4d\x40\xe1\xac"
                "\xea\x25\x81\x03\x02\x08\x10\x82\x02\x03\x28"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix18()
    {
        testcase("Prefix18");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** thresh2
        // **** Preim4Cond
        // **** Rsa5Cond
        // **** Ed6Cond
        // **** ed3

        auto const ed3Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 32> const ed3PublicKey{
            {0x42, 0x0a, 0x60, 0x63, 0x9e, 0xca, 0xaa, 0x48, 0xac, 0xcb, 0x2c,
             0xd9, 0x46, 0x79, 0x69, 0x34, 0x35, 0xf3, 0x8e, 0x29, 0xd5, 0x4b,
             0x24, 0x57, 0x9d, 0xd7, 0x3b, 0x04, 0x5d, 0x9c, 0x2f, 0x32}};
        std::array<std::uint8_t, 64> const ed3Sig{
            {0x8e, 0xa4, 0xd9, 0x6a, 0x9b, 0x3e, 0x19, 0xe8, 0x89, 0xa4, 0x4e,
             0xf2, 0xa8, 0x1c, 0xc2, 0xbd, 0xd3, 0xe0, 0x6f, 0xe0, 0xd1, 0x6b,
             0x85, 0x47, 0x9a, 0x58, 0x2e, 0x9f, 0x38, 0x09, 0x1f, 0x6d, 0x02,
             0x9a, 0xf6, 0x9c, 0x60, 0x3f, 0x16, 0x6c, 0xa5, 0x0e, 0xfb, 0xa3,
             0x08, 0xd6, 0xb6, 0x97, 0x5f, 0x2e, 0x94, 0xfb, 0x49, 0x56, 0xbe,
             0x2c, 0x58, 0x48, 0x15, 0x49, 0x73, 0xa2, 0xae, 0x09}};
        std::array<std::uint8_t, 32> const ed3SigningKey{
            {0x1b, 0x90, 0xf2, 0x2c, 0xd5, 0x5d, 0xcd, 0xe5, 0x2d, 0x71, 0x84,
             0x66, 0x10, 0xf0, 0xf1, 0x91, 0xfe, 0xb5, 0xbe, 0x94, 0x85, 0xc1,
             0xc8, 0xf3, 0x61, 0x91, 0xc3, 0xa4, 0x41, 0x58, 0xcd, 0xfc}};
        (void)ed3SigningKey;
        auto const thresh2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim4CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim4Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim4CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa5CondConditionFingerprint =
            "\x99\xfb\x0b\x38\x94\x4d\x20\x85\xc8\xda\x3a\x64\x31\x44\x6f"
            "\x6c\x3b\x46\x25\x50\xd7\x7f\xdf\xee\x75\x72\x71\xf9\x61\x40"
            "\x63\xfa"s;
        Condition const Rsa5Cond{Type::rsaSha256,
                                 65536,
                                 makeSlice(Rsa5CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const Ed6CondConditionFingerprint =
            "\x00\xd3\xc9\x24\x3f\x2d\x2e\x64\x93\xa8\x49\x29\x82\x75\xea"
            "\xbf\xe3\x53\x7f\x8e\x45\x16\xdb\x5e\xc6\xdf\x39\xd2\xcb\xea"
            "\x62\xfb"s;
        Condition const Ed6Cond{Type::ed25519Sha256,
                                131072,
                                makeSlice(Ed6CondConditionFingerprint),
                                std::bitset<5>{0}};
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto ed3 = std::make_unique<Ed25519>(ed3PublicKey, ed3Sig);
        std::vector<std::unique_ptr<Fulfillment>> thresh2Subfulfillments;
        thresh2Subfulfillments.emplace_back(std::move(ed3));
        std::vector<Condition> thresh2Subconditions{
            {Preim4Cond, Rsa5Cond, Ed6Cond}};
        auto thresh2 = std::make_unique<ThresholdSha256>(
            std::move(thresh2Subfulfillments), std::move(thresh2Subconditions));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(thresh2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x81\xfd\x80\x02\x50\x30\x81\x01\x0e\xa2\x81\xf3\xa1\x81"
                "\xf0\x80\x02\x50\x31\x81\x01\x0e\xa2\x81\xe6\xa2\x81\xe3\xa0"
                "\x66\xa4\x64\x80\x20\x42\x0a\x60\x63\x9e\xca\xaa\x48\xac\xcb"
                "\x2c\xd9\x46\x79\x69\x34\x35\xf3\x8e\x29\xd5\x4b\x24\x57\x9d"
                "\xd7\x3b\x04\x5d\x9c\x2f\x32\x81\x40\x8e\xa4\xd9\x6a\x9b\x3e"
                "\x19\xe8\x89\xa4\x4e\xf2\xa8\x1c\xc2\xbd\xd3\xe0\x6f\xe0\xd1"
                "\x6b\x85\x47\x9a\x58\x2e\x9f\x38\x09\x1f\x6d\x02\x9a\xf6\x9c"
                "\x60\x3f\x16\x6c\xa5\x0e\xfb\xa3\x08\xd6\xb6\x97\x5f\x2e\x94"
                "\xfb\x49\x56\xbe\x2c\x58\x48\x15\x49\x73\xa2\xae\x09\xa1\x79"
                "\xa0\x25\x80\x20\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f"
                "\x5e\x54\x2d\x6a\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd"
                "\x53\xee\x93\x58\xeb\x4e\x81\x01\x09\xa3\x27\x80\x20\x99\xfb"
                "\x0b\x38\x94\x4d\x20\x85\xc8\xda\x3a\x64\x31\x44\x6f\x6c\x3b"
                "\x46\x25\x50\xd7\x7f\xdf\xee\x75\x72\x71\xf9\x61\x40\x63\xfa"
                "\x81\x03\x01\x00\x00\xa4\x27\x80\x20\x00\xd3\xc9\x24\x3f\x2d"
                "\x2e\x64\x93\xa8\x49\x29\x82\x75\xea\xbf\xe3\x53\x7f\x8e\x45"
                "\x16\xdb\x5e\xc6\xdf\x39\xd2\xcb\xea\x62\xfb\x81\x03\x02\x00"
                "\x00"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\x25\xe5\xa0\xbc\x54\x5d\xb9\x2e\x6c\x49\x75"
                "\xc2\x9c\x8f\x93\x13\x52\x7d\x1a\x2a\x62\xf1\x08\x49\x27\x4e"
                "\xa9\xb1\xae\xbb\x61\xd7\x81\x03\x02\x18\x20\x82\x02\x03\xb8"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\x52\xfa\x6f\x2f\x9e\x05\x09\xc3\x76\x37\x3d\xfe\x5c\xaf\xca"
                "\xea\xa1\x3a\xd9\x64\xe2\xb6\x61\x65\x41\x4e\xac\x0a\x3e\x95"
                "\x49\x08\x81\x03\x02\x14\x10\x82\x02\x03\xb8"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix19()
    {
        testcase("Prefix19");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** thresh2
        // **** Preim9Cond
        // **** Rsa10Cond
        // **** Ed11Cond
        // **** ed3
        // **** thresh4
        // ***** Preim6Cond
        // ***** Rsa7Cond
        // ***** Ed8Cond
        // ***** ed5

        auto const ed3Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 32> const ed3PublicKey{
            {0x42, 0x0a, 0x60, 0x63, 0x9e, 0xca, 0xaa, 0x48, 0xac, 0xcb, 0x2c,
             0xd9, 0x46, 0x79, 0x69, 0x34, 0x35, 0xf3, 0x8e, 0x29, 0xd5, 0x4b,
             0x24, 0x57, 0x9d, 0xd7, 0x3b, 0x04, 0x5d, 0x9c, 0x2f, 0x32}};
        std::array<std::uint8_t, 64> const ed3Sig{
            {0x8e, 0xa4, 0xd9, 0x6a, 0x9b, 0x3e, 0x19, 0xe8, 0x89, 0xa4, 0x4e,
             0xf2, 0xa8, 0x1c, 0xc2, 0xbd, 0xd3, 0xe0, 0x6f, 0xe0, 0xd1, 0x6b,
             0x85, 0x47, 0x9a, 0x58, 0x2e, 0x9f, 0x38, 0x09, 0x1f, 0x6d, 0x02,
             0x9a, 0xf6, 0x9c, 0x60, 0x3f, 0x16, 0x6c, 0xa5, 0x0e, 0xfb, 0xa3,
             0x08, 0xd6, 0xb6, 0x97, 0x5f, 0x2e, 0x94, 0xfb, 0x49, 0x56, 0xbe,
             0x2c, 0x58, 0x48, 0x15, 0x49, 0x73, 0xa2, 0xae, 0x09}};
        std::array<std::uint8_t, 32> const ed3SigningKey{
            {0x1b, 0x90, 0xf2, 0x2c, 0xd5, 0x5d, 0xcd, 0xe5, 0x2d, 0x71, 0x84,
             0x66, 0x10, 0xf0, 0xf1, 0x91, 0xfe, 0xb5, 0xbe, 0x94, 0x85, 0xc1,
             0xc8, 0xf3, 0x61, 0x91, 0xc3, 0xa4, 0x41, 0x58, 0xcd, 0xfc}};
        (void)ed3SigningKey;
        auto const ed5Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 32> const ed5PublicKey{
            {0xae, 0xbc, 0xe5, 0x4b, 0x88, 0x09, 0x8d, 0x4f, 0xc4, 0xe1, 0x22,
             0xa0, 0x7c, 0x41, 0x05, 0xd7, 0x9f, 0xbe, 0xc8, 0x3d, 0x1d, 0x7e,
             0xd6, 0x55, 0xf4, 0x01, 0x67, 0x68, 0x93, 0x55, 0x85, 0xdf}};
        std::array<std::uint8_t, 64> const ed5Sig{
            {0x1a, 0xdc, 0xf3, 0xaa, 0xc3, 0x2e, 0xe4, 0xed, 0x1d, 0x40, 0xe0,
             0xac, 0xfa, 0x29, 0xec, 0xe4, 0xc4, 0x9e, 0x92, 0xbd, 0x49, 0x0d,
             0x18, 0xc7, 0xeb, 0x01, 0xa6, 0x6a, 0x92, 0xa3, 0xc4, 0xc2, 0x92,
             0xd6, 0x62, 0x26, 0x6a, 0x3f, 0x8b, 0x0b, 0xa7, 0x11, 0xe0, 0x37,
             0x3e, 0x71, 0x77, 0x97, 0x7e, 0x66, 0x69, 0x4f, 0x58, 0x2e, 0xc0,
             0x05, 0xd5, 0x2e, 0x3f, 0x2b, 0xec, 0xc7, 0x48, 0x02}};
        std::array<std::uint8_t, 32> const ed5SigningKey{
            {0x42, 0x67, 0x67, 0xc0, 0xba, 0xdf, 0xb4, 0xd3, 0xf5, 0xc5, 0x1f,
             0x71, 0x97, 0x8a, 0xb4, 0x8e, 0x9a, 0xea, 0x3e, 0xec, 0xaf, 0xdc,
             0xc7, 0x2b, 0x01, 0x1b, 0x06, 0x8f, 0x05, 0x56, 0x63, 0xbc}};
        (void)ed5SigningKey;
        auto const thresh4Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim6CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim6Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim6CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa7CondConditionFingerprint =
            "\x6c\x7b\xea\x83\xa1\xf4\x82\x3d\x36\xe7\x6e\xae\x1a\xbc\xa0"
            "\xba\x90\x3d\x96\xc1\xe6\xad\x3a\x47\xa5\xcb\x88\xab\x3c\x5f"
            "\xcc\xd5"s;
        Condition const Rsa7Cond{Type::rsaSha256,
                                 65536,
                                 makeSlice(Rsa7CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const Ed8CondConditionFingerprint =
            "\xf1\x68\x96\xa6\x2e\xef\x7f\x47\x06\x51\x4c\xc6\x7e\x24\xf7"
            "\x29\x84\x9c\xd6\xb0\xd9\x4b\xd9\x0f\xc9\x34\x01\x9d\x92\xeb"
            "\xbc\x0a"s;
        Condition const Ed8Cond{Type::ed25519Sha256,
                                131072,
                                makeSlice(Ed8CondConditionFingerprint),
                                std::bitset<5>{0}};
        auto const thresh2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim9CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim9Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim9CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa10CondConditionFingerprint =
            "\x3c\x73\x38\xcf\x23\xc6\x31\x53\x28\xc4\x27\xf8\x95\x87\x99"
            "\x83\x2d\x35\x3c\x03\x9b\xd1\xff\xff\x2e\x53\x20\xe9\x5e\x62"
            "\xb9\xb7"s;
        Condition const Rsa10Cond{Type::rsaSha256,
                                  65536,
                                  makeSlice(Rsa10CondConditionFingerprint),
                                  std::bitset<5>{0}};
        auto const Ed11CondConditionFingerprint =
            "\x41\x80\x08\xb2\x60\x74\x57\x6d\xac\xed\x74\x7f\x54\xdb\x96"
            "\x18\x91\x06\x0a\x95\xa1\x49\x17\xc7\x65\xe3\x94\xc8\x5e\x2c"
            "\x92\x20"s;
        Condition const Ed11Cond{Type::ed25519Sha256,
                                 131072,
                                 makeSlice(Ed11CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto ed3 = std::make_unique<Ed25519>(ed3PublicKey, ed3Sig);
        auto ed5 = std::make_unique<Ed25519>(ed5PublicKey, ed5Sig);
        std::vector<std::unique_ptr<Fulfillment>> thresh4Subfulfillments;
        thresh4Subfulfillments.emplace_back(std::move(ed5));
        std::vector<Condition> thresh4Subconditions{
            {Preim6Cond, Rsa7Cond, Ed8Cond}};
        auto thresh4 = std::make_unique<ThresholdSha256>(
            std::move(thresh4Subfulfillments), std::move(thresh4Subconditions));
        std::vector<std::unique_ptr<Fulfillment>> thresh2Subfulfillments;
        thresh2Subfulfillments.emplace_back(std::move(ed3));
        thresh2Subfulfillments.emplace_back(std::move(thresh4));
        std::vector<Condition> thresh2Subconditions{
            {Preim9Cond, Rsa10Cond, Ed11Cond}};
        auto thresh2 = std::make_unique<ThresholdSha256>(
            std::move(thresh2Subfulfillments), std::move(thresh2Subconditions));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(thresh2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x82\x01\xe9\x80\x02\x50\x30\x81\x01\x0e\xa2\x82\x01\xde"
                "\xa1\x82\x01\xda\x80\x02\x50\x31\x81\x01\x0e\xa2\x82\x01\xcf"
                "\xa2\x82\x01\xcb\xa0\x82\x01\x4c\xa2\x81\xe3\xa0\x66\xa4\x64"
                "\x80\x20\xae\xbc\xe5\x4b\x88\x09\x8d\x4f\xc4\xe1\x22\xa0\x7c"
                "\x41\x05\xd7\x9f\xbe\xc8\x3d\x1d\x7e\xd6\x55\xf4\x01\x67\x68"
                "\x93\x55\x85\xdf\x81\x40\x1a\xdc\xf3\xaa\xc3\x2e\xe4\xed\x1d"
                "\x40\xe0\xac\xfa\x29\xec\xe4\xc4\x9e\x92\xbd\x49\x0d\x18\xc7"
                "\xeb\x01\xa6\x6a\x92\xa3\xc4\xc2\x92\xd6\x62\x26\x6a\x3f\x8b"
                "\x0b\xa7\x11\xe0\x37\x3e\x71\x77\x97\x7e\x66\x69\x4f\x58\x2e"
                "\xc0\x05\xd5\x2e\x3f\x2b\xec\xc7\x48\x02\xa1\x79\xa0\x25\x80"
                "\x20\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d"
                "\x6a\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93"
                "\x58\xeb\x4e\x81\x01\x09\xa3\x27\x80\x20\x6c\x7b\xea\x83\xa1"
                "\xf4\x82\x3d\x36\xe7\x6e\xae\x1a\xbc\xa0\xba\x90\x3d\x96\xc1"
                "\xe6\xad\x3a\x47\xa5\xcb\x88\xab\x3c\x5f\xcc\xd5\x81\x03\x01"
                "\x00\x00\xa4\x27\x80\x20\xf1\x68\x96\xa6\x2e\xef\x7f\x47\x06"
                "\x51\x4c\xc6\x7e\x24\xf7\x29\x84\x9c\xd6\xb0\xd9\x4b\xd9\x0f"
                "\xc9\x34\x01\x9d\x92\xeb\xbc\x0a\x81\x03\x02\x00\x00\xa4\x64"
                "\x80\x20\x42\x0a\x60\x63\x9e\xca\xaa\x48\xac\xcb\x2c\xd9\x46"
                "\x79\x69\x34\x35\xf3\x8e\x29\xd5\x4b\x24\x57\x9d\xd7\x3b\x04"
                "\x5d\x9c\x2f\x32\x81\x40\x8e\xa4\xd9\x6a\x9b\x3e\x19\xe8\x89"
                "\xa4\x4e\xf2\xa8\x1c\xc2\xbd\xd3\xe0\x6f\xe0\xd1\x6b\x85\x47"
                "\x9a\x58\x2e\x9f\x38\x09\x1f\x6d\x02\x9a\xf6\x9c\x60\x3f\x16"
                "\x6c\xa5\x0e\xfb\xa3\x08\xd6\xb6\x97\x5f\x2e\x94\xfb\x49\x56"
                "\xbe\x2c\x58\x48\x15\x49\x73\xa2\xae\x09\xa1\x79\xa0\x25\x80"
                "\x20\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d"
                "\x6a\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93"
                "\x58\xeb\x4e\x81\x01\x09\xa3\x27\x80\x20\x3c\x73\x38\xcf\x23"
                "\xc6\x31\x53\x28\xc4\x27\xf8\x95\x87\x99\x83\x2d\x35\x3c\x03"
                "\x9b\xd1\xff\xff\x2e\x53\x20\xe9\x5e\x62\xb9\xb7\x81\x03\x01"
                "\x00\x00\xa4\x27\x80\x20\x41\x80\x08\xb2\x60\x74\x57\x6d\xac"
                "\xed\x74\x7f\x54\xdb\x96\x18\x91\x06\x0a\x95\xa1\x49\x17\xc7"
                "\x65\xe3\x94\xc8\x5e\x2c\x92\x20\x81\x03\x02\x00\x00"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\xeb\x6d\xd3\xa5\x8e\x9e\x20\xae\xbc\xe1\x7d"
                "\x0a\x5f\x8f\x9a\x6a\x7f\x0b\x24\x3a\x3c\x8d\x55\xaa\x37\xe6"
                "\x53\x45\xe7\x69\x63\xc6\x81\x03\x04\x2c\x20\x82\x02\x03\xb8"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\x8d\xb4\xf6\xeb\x0c\x98\x28\x00\x07\x9a\xd5\xbc\x2a\x75\x53"
                "\x37\x8c\xc0\x9e\xe7\xec\xae\x7e\x51\x3a\x5f\x52\x86\xaa\xac"
                "\x7d\x74\x81\x03\x04\x28\x10\x82\x02\x03\xb8"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    testPrefix20()
    {
        testcase("Prefix20");

        using namespace std::string_literals;
        using namespace ripple::cryptoconditions;

        // Fulfillment structure
        // * prefix0
        // ** prefix1
        // *** thresh2
        // **** Preim9Cond
        // **** Rsa10Cond
        // **** Ed11Cond
        // **** Thresh12Cond
        // **** ed3
        // **** thresh4
        // ***** Preim6Cond
        // ***** Rsa7Cond
        // ***** Ed8Cond
        // ***** ed5

        auto const ed3Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 32> const ed3PublicKey{
            {0x42, 0x0a, 0x60, 0x63, 0x9e, 0xca, 0xaa, 0x48, 0xac, 0xcb, 0x2c,
             0xd9, 0x46, 0x79, 0x69, 0x34, 0x35, 0xf3, 0x8e, 0x29, 0xd5, 0x4b,
             0x24, 0x57, 0x9d, 0xd7, 0x3b, 0x04, 0x5d, 0x9c, 0x2f, 0x32}};
        std::array<std::uint8_t, 64> const ed3Sig{
            {0x8e, 0xa4, 0xd9, 0x6a, 0x9b, 0x3e, 0x19, 0xe8, 0x89, 0xa4, 0x4e,
             0xf2, 0xa8, 0x1c, 0xc2, 0xbd, 0xd3, 0xe0, 0x6f, 0xe0, 0xd1, 0x6b,
             0x85, 0x47, 0x9a, 0x58, 0x2e, 0x9f, 0x38, 0x09, 0x1f, 0x6d, 0x02,
             0x9a, 0xf6, 0x9c, 0x60, 0x3f, 0x16, 0x6c, 0xa5, 0x0e, 0xfb, 0xa3,
             0x08, 0xd6, 0xb6, 0x97, 0x5f, 0x2e, 0x94, 0xfb, 0x49, 0x56, 0xbe,
             0x2c, 0x58, 0x48, 0x15, 0x49, 0x73, 0xa2, 0xae, 0x09}};
        std::array<std::uint8_t, 32> const ed3SigningKey{
            {0x1b, 0x90, 0xf2, 0x2c, 0xd5, 0x5d, 0xcd, 0xe5, 0x2d, 0x71, 0x84,
             0x66, 0x10, 0xf0, 0xf1, 0x91, 0xfe, 0xb5, 0xbe, 0x94, 0x85, 0xc1,
             0xc8, 0xf3, 0x61, 0x91, 0xc3, 0xa4, 0x41, 0x58, 0xcd, 0xfc}};
        (void)ed3SigningKey;
        auto const ed5Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        std::array<std::uint8_t, 32> const ed5PublicKey{
            {0xae, 0xbc, 0xe5, 0x4b, 0x88, 0x09, 0x8d, 0x4f, 0xc4, 0xe1, 0x22,
             0xa0, 0x7c, 0x41, 0x05, 0xd7, 0x9f, 0xbe, 0xc8, 0x3d, 0x1d, 0x7e,
             0xd6, 0x55, 0xf4, 0x01, 0x67, 0x68, 0x93, 0x55, 0x85, 0xdf}};
        std::array<std::uint8_t, 64> const ed5Sig{
            {0x1a, 0xdc, 0xf3, 0xaa, 0xc3, 0x2e, 0xe4, 0xed, 0x1d, 0x40, 0xe0,
             0xac, 0xfa, 0x29, 0xec, 0xe4, 0xc4, 0x9e, 0x92, 0xbd, 0x49, 0x0d,
             0x18, 0xc7, 0xeb, 0x01, 0xa6, 0x6a, 0x92, 0xa3, 0xc4, 0xc2, 0x92,
             0xd6, 0x62, 0x26, 0x6a, 0x3f, 0x8b, 0x0b, 0xa7, 0x11, 0xe0, 0x37,
             0x3e, 0x71, 0x77, 0x97, 0x7e, 0x66, 0x69, 0x4f, 0x58, 0x2e, 0xc0,
             0x05, 0xd5, 0x2e, 0x3f, 0x2b, 0xec, 0xc7, 0x48, 0x02}};
        std::array<std::uint8_t, 32> const ed5SigningKey{
            {0x42, 0x67, 0x67, 0xc0, 0xba, 0xdf, 0xb4, 0xd3, 0xf5, 0xc5, 0x1f,
             0x71, 0x97, 0x8a, 0xb4, 0x8e, 0x9a, 0xea, 0x3e, 0xec, 0xaf, 0xdc,
             0xc7, 0x2b, 0x01, 0x1b, 0x06, 0x8f, 0x05, 0x56, 0x63, 0xbc}};
        (void)ed5SigningKey;
        auto const thresh4Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim6CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim6Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim6CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa7CondConditionFingerprint =
            "\x6c\x7b\xea\x83\xa1\xf4\x82\x3d\x36\xe7\x6e\xae\x1a\xbc\xa0"
            "\xba\x90\x3d\x96\xc1\xe6\xad\x3a\x47\xa5\xcb\x88\xab\x3c\x5f"
            "\xcc\xd5"s;
        Condition const Rsa7Cond{Type::rsaSha256,
                                 65536,
                                 makeSlice(Rsa7CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const Ed8CondConditionFingerprint =
            "\xf1\x68\x96\xa6\x2e\xef\x7f\x47\x06\x51\x4c\xc6\x7e\x24\xf7"
            "\x29\x84\x9c\xd6\xb0\xd9\x4b\xd9\x0f\xc9\x34\x01\x9d\x92\xeb"
            "\xbc\x0a"s;
        Condition const Ed8Cond{Type::ed25519Sha256,
                                131072,
                                makeSlice(Ed8CondConditionFingerprint),
                                std::bitset<5>{0}};
        auto const thresh2Msg = "P1P0abcdefghijklmnopqrstuvwxyz"s;
        auto const Preim9CondConditionFingerprint =
            "\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d\x6a"
            "\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93\x58"
            "\xeb\x4e"s;
        Condition const Preim9Cond{Type::preimageSha256,
                                   9,
                                   makeSlice(Preim9CondConditionFingerprint),
                                   std::bitset<5>{0}};
        auto const Rsa10CondConditionFingerprint =
            "\x3c\x73\x38\xcf\x23\xc6\x31\x53\x28\xc4\x27\xf8\x95\x87\x99"
            "\x83\x2d\x35\x3c\x03\x9b\xd1\xff\xff\x2e\x53\x20\xe9\x5e\x62"
            "\xb9\xb7"s;
        Condition const Rsa10Cond{Type::rsaSha256,
                                  65536,
                                  makeSlice(Rsa10CondConditionFingerprint),
                                  std::bitset<5>{0}};
        auto const Ed11CondConditionFingerprint =
            "\x41\x80\x08\xb2\x60\x74\x57\x6d\xac\xed\x74\x7f\x54\xdb\x96"
            "\x18\x91\x06\x0a\x95\xa1\x49\x17\xc7\x65\xe3\x94\xc8\x5e\x2c"
            "\x92\x20"s;
        Condition const Ed11Cond{Type::ed25519Sha256,
                                 131072,
                                 makeSlice(Ed11CondConditionFingerprint),
                                 std::bitset<5>{0}};
        auto const Thresh12CondConditionFingerprint =
            "\x16\x7f\xbc\x09\xc8\x59\x71\x7b\x2e\xb0\x65\x72\xdf\x23\xe9"
            "\x85\x65\x99\x10\x58\x49\x09\x94\xda\x03\x4d\x77\xf8\xcc\x91"
            "\xac\x32"s;
        Condition const Thresh12Cond{
            Type::thresholdSha256,
            135168,
            makeSlice(Thresh12CondConditionFingerprint),
            std::bitset<5>{25}};
        auto const prefix1Prefix = "P1"s;
        auto const prefix1Msg = "P0abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix1MaxMsgLength = 14;
        auto const prefix0Prefix = "P0"s;
        auto const prefix0Msg = "abcdefghijklmnopqrstuvwxyz"s;
        auto const prefix0MaxMsgLength = 14;

        auto ed3 = std::make_unique<Ed25519>(ed3PublicKey, ed3Sig);
        auto ed5 = std::make_unique<Ed25519>(ed5PublicKey, ed5Sig);
        std::vector<std::unique_ptr<Fulfillment>> thresh4Subfulfillments;
        thresh4Subfulfillments.emplace_back(std::move(ed5));
        std::vector<Condition> thresh4Subconditions{
            {Preim6Cond, Rsa7Cond, Ed8Cond}};
        auto thresh4 = std::make_unique<ThresholdSha256>(
            std::move(thresh4Subfulfillments), std::move(thresh4Subconditions));
        std::vector<std::unique_ptr<Fulfillment>> thresh2Subfulfillments;
        thresh2Subfulfillments.emplace_back(std::move(ed3));
        thresh2Subfulfillments.emplace_back(std::move(thresh4));
        std::vector<Condition> thresh2Subconditions{
            {Preim9Cond, Rsa10Cond, Ed11Cond, Thresh12Cond}};
        auto thresh2 = std::make_unique<ThresholdSha256>(
            std::move(thresh2Subfulfillments), std::move(thresh2Subconditions));
        auto prefix1 = std::make_unique<PrefixSha256>(
            makeSlice(prefix1Prefix), prefix1MaxMsgLength, std::move(thresh2));
        PrefixSha256 const prefix0(
            makeSlice(prefix0Prefix), prefix0MaxMsgLength, std::move(prefix1));
        {
            auto const prefix0EncodedFulfillment =
                "\xa1\x82\x02\x17\x80\x02\x50\x30\x81\x01\x0e\xa2\x82\x02\x0c"
                "\xa1\x82\x02\x08\x80\x02\x50\x31\x81\x01\x0e\xa2\x82\x01\xfd"
                "\xa2\x82\x01\xf9\xa0\x82\x01\x4c\xa2\x81\xe3\xa0\x66\xa4\x64"
                "\x80\x20\xae\xbc\xe5\x4b\x88\x09\x8d\x4f\xc4\xe1\x22\xa0\x7c"
                "\x41\x05\xd7\x9f\xbe\xc8\x3d\x1d\x7e\xd6\x55\xf4\x01\x67\x68"
                "\x93\x55\x85\xdf\x81\x40\x1a\xdc\xf3\xaa\xc3\x2e\xe4\xed\x1d"
                "\x40\xe0\xac\xfa\x29\xec\xe4\xc4\x9e\x92\xbd\x49\x0d\x18\xc7"
                "\xeb\x01\xa6\x6a\x92\xa3\xc4\xc2\x92\xd6\x62\x26\x6a\x3f\x8b"
                "\x0b\xa7\x11\xe0\x37\x3e\x71\x77\x97\x7e\x66\x69\x4f\x58\x2e"
                "\xc0\x05\xd5\x2e\x3f\x2b\xec\xc7\x48\x02\xa1\x79\xa0\x25\x80"
                "\x20\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54\x2d"
                "\x6a\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee\x93"
                "\x58\xeb\x4e\x81\x01\x09\xa3\x27\x80\x20\x6c\x7b\xea\x83\xa1"
                "\xf4\x82\x3d\x36\xe7\x6e\xae\x1a\xbc\xa0\xba\x90\x3d\x96\xc1"
                "\xe6\xad\x3a\x47\xa5\xcb\x88\xab\x3c\x5f\xcc\xd5\x81\x03\x01"
                "\x00\x00\xa4\x27\x80\x20\xf1\x68\x96\xa6\x2e\xef\x7f\x47\x06"
                "\x51\x4c\xc6\x7e\x24\xf7\x29\x84\x9c\xd6\xb0\xd9\x4b\xd9\x0f"
                "\xc9\x34\x01\x9d\x92\xeb\xbc\x0a\x81\x03\x02\x00\x00\xa4\x64"
                "\x80\x20\x42\x0a\x60\x63\x9e\xca\xaa\x48\xac\xcb\x2c\xd9\x46"
                "\x79\x69\x34\x35\xf3\x8e\x29\xd5\x4b\x24\x57\x9d\xd7\x3b\x04"
                "\x5d\x9c\x2f\x32\x81\x40\x8e\xa4\xd9\x6a\x9b\x3e\x19\xe8\x89"
                "\xa4\x4e\xf2\xa8\x1c\xc2\xbd\xd3\xe0\x6f\xe0\xd1\x6b\x85\x47"
                "\x9a\x58\x2e\x9f\x38\x09\x1f\x6d\x02\x9a\xf6\x9c\x60\x3f\x16"
                "\x6c\xa5\x0e\xfb\xa3\x08\xd6\xb6\x97\x5f\x2e\x94\xfb\x49\x56"
                "\xbe\x2c\x58\x48\x15\x49\x73\xa2\xae\x09\xa1\x81\xa6\xa0\x25"
                "\x80\x20\x5d\xa0\x30\xef\xfd\xe1\x75\x11\x51\xe8\x5f\x5e\x54"
                "\x2d\x6a\x5b\xd1\x5c\xc9\x33\x21\x2c\xe2\x68\xfc\xfd\x53\xee"
                "\x93\x58\xeb\x4e\x81\x01\x09\xa2\x2b\x80\x20\x16\x7f\xbc\x09"
                "\xc8\x59\x71\x7b\x2e\xb0\x65\x72\xdf\x23\xe9\x85\x65\x99\x10"
                "\x58\x49\x09\x94\xda\x03\x4d\x77\xf8\xcc\x91\xac\x32\x81\x03"
                "\x02\x10\x00\x82\x02\x03\x98\xa3\x27\x80\x20\x3c\x73\x38\xcf"
                "\x23\xc6\x31\x53\x28\xc4\x27\xf8\x95\x87\x99\x83\x2d\x35\x3c"
                "\x03\x9b\xd1\xff\xff\x2e\x53\x20\xe9\x5e\x62\xb9\xb7\x81\x03"
                "\x01\x00\x00\xa4\x27\x80\x20\x41\x80\x08\xb2\x60\x74\x57\x6d"
                "\xac\xed\x74\x7f\x54\xdb\x96\x18\x91\x06\x0a\x95\xa1\x49\x17"
                "\xc7\x65\xe3\x94\xc8\x5e\x2c\x92\x20\x81\x03\x02\x00\x00"s;
            auto const prefix0EncodedCondition =
                "\xa1\x2b\x80\x20\xdd\xec\x1f\x76\xe2\xfb\x4f\xb5\x59\xda\x16"
                "\xe6\xe6\x5b\x86\x19\x6c\x6c\xc7\x56\x21\x6e\x8c\x4a\x22\xe7"
                "\xfd\x18\xdf\xcc\x1a\xc6\x81\x03\x04\x40\x20\x82\x02\x03\xb8"s;
            auto const prefix0EncodedFingerprint =
                "\x30\x36\x80\x02\x50\x30\x81\x01\x0e\xa2\x2d\xa1\x2b\x80\x20"
                "\x0a\xc0\xee\xd6\x11\x75\x8c\x80\x1f\xe0\xe2\xd1\x80\x9f\x62"
                "\xe8\xce\xb4\x69\xbc\x34\x7f\x0e\xc0\x20\x9e\xe2\x13\xfb\x2a"
                "\xa3\xf8\x81\x03\x04\x3c\x10\x82\x02\x03\xb8"s;
            check(
                &prefix0,
                prefix0Msg,
                prefix0EncodedFulfillment,
                prefix0EncodedCondition,
                prefix0EncodedFingerprint);
        }
    }

    void
    run()
    {
        testPrefix0();
        testPrefix1();
        testPrefix2();
        testPrefix3();
        testPrefix4();
        testPrefix5();
        testPrefix6();
        testPrefix7();
        testPrefix8();
        testPrefix9();
        testPrefix10();
        testPrefix11();
        testPrefix12();
        testPrefix13();
        testPrefix14();
        testPrefix15();
        testPrefix16();
        testPrefix17();
        testPrefix18();
        testPrefix19();
        testPrefix20();
    }
};

BEAST_DEFINE_TESTSUITE(Conditions_prefix, conditions, ripple);
}  // cryptoconditions
}  // ripple
