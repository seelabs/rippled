//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

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

#include <ripple/basics/chrono.h>
#include <ripple/ledger/Directory.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/jss.h>
#include <test/jtx.h>

#include <array>
#include <chrono>

namespace ripple {
namespace test {
struct StableCoin_test : public beast::unit_test::suite
{
    [[nodiscard]] static Json::Value
    createOracle(jtx::Account const& account, uint160 const& assetType)
    {
        using namespace jtx;
        Json::Value jv;
        jv[jss::TransactionType] = jss::OracleCreate;
        jv[jss::Flags] = tfUniversal;
        jv[jss::Account] = account.human();
        jv[jss::AssetType] = strHex(assetType);
        return jv;
    }

    [[nodiscard]] static Json::Value
    deleteOracle(jtx::Account const& account, uint160 const& assetType)
    {
        using namespace jtx;
        Json::Value jv;
        jv[jss::TransactionType] = jss::OracleDelete;
        jv[jss::Flags] = tfUniversal;
        jv[jss::Account] = account.human();
        jv[jss::AssetType] = strHex(assetType);
        return jv;
    }

    [[nodiscard]] static Json::Value
    updateOracle(
        jtx::Account const& account,
        uint160 const& assetType,
        std::uint32_t validAfter,
        std::uint32_t expiration,
        std::uint32_t assetCount,
        STAmount const& xrpVal)
    {
        using namespace jtx;
        Json::Value jv;
        auto const k = keylet::oracle(account, assetType);
        jv[jss::TransactionType] = jss::OracleUpdate;
        jv[jss::Flags] = tfUniversal;
        jv[jss::Account] = account.human();
        jv[jss::OracleID] = strHex(k.key);
        jv[jss::ValidAfter] = validAfter;
        jv[jss::Expiration] = expiration;
        jv[jss::OracleAssetCount] = assetCount;
        jv[jss::OracleXRPValue] = xrpVal.getJson(JsonOptions::none);
        return jv;
    }

    [[nodiscard]] static Json::Value
    updateOracle(
        jtx::Account const& account,
        uint160 const& assetType,
        NetClock::time_point const& validAfter,
        NetClock::time_point const& expiration,
        std::uint32_t assetCount,
        STAmount const& xrpVal)
    {
        return updateOracle(
            account,
            assetType,
            validAfter.time_since_epoch().count(),
            expiration.time_since_epoch().count(),
            assetCount,
            xrpVal);
    }

    [[nodiscard]] static Json::Value
    createStableCoin(
        jtx::Account const& account,
        uint160 const& assetType,
        uint256 const& oracleID,
        std::uint32_t issRatio,
        std::uint32_t lqdRatio,
        std::uint32_t lqdPenalty,
        std::uint32_t loanOrgFee,
        std::uint32_t depositFee)
    {
        using namespace jtx;
        Json::Value jv;
        jv[jss::TransactionType] = jss::StableCoinCreate;
        jv[jss::Flags] = tfUniversal;
        jv[jss::Account] = account.human();
        jv[jss::AssetType] = strHex(assetType);
        jv[jss::IssuanceRatio] = issRatio;
        jv[jss::LiquidationRatio] = lqdRatio;
        jv[jss::LoanOriginationFee] = loanOrgFee;
        jv[jss::DepositFee] = depositFee;
        jv[jss::LiquidationPenalty] = lqdPenalty;
        jv[jss::OracleID] = strHex(oracleID);
        return jv;
    }

    [[nodiscard]] static Json::Value
    deleteStableCoin(jtx::Account const& account, uint160 const& assetType)
    {
        using namespace jtx;
        Json::Value jv;
        jv[jss::TransactionType] = jss::StableCoinDelete;
        jv[jss::Flags] = tfUniversal;
        jv[jss::Account] = account.human();
        jv[jss::AssetType] = strHex(assetType);
        return jv;
    }

    [[nodiscard]] static Json::Value
    createCDP(
        jtx::Account const& account,
        jtx::Account const& stableCoinOwner,
        uint160 const& assetType,
        boost::optional<STAmount> const& amt = boost::none)
    {
        using namespace jtx;
        Json::Value jv;
        jv[jss::TransactionType] = jss::CDPCreate;
        jv[jss::Flags] = tfUniversal;
        jv[jss::Account] = account.human();
        jv[jss::StableCoinOwner] = stableCoinOwner.human();
        jv[jss::AssetType] = strHex(assetType);
        if (amt)
            jv[jss::Amount] = amt->getJson(JsonOptions::none);
        return jv;
    }

    [[nodiscard]] static Json::Value
    deleteCDP(
        jtx::Account const& account,
        jtx::Account const& stableCoinOwner,
        uint160 const& assetType)
    {
        using namespace jtx;
        Json::Value jv;
        jv[jss::TransactionType] = jss::CDPDelete;
        jv[jss::Flags] = tfUniversal;
        jv[jss::Account] = account.human();
        jv[jss::StableCoinOwner] = stableCoinOwner.human();
        jv[jss::AssetType] = strHex(assetType);
        return jv;
    }

    [[nodiscard]] static Json::Value
    depositCDP(
        jtx::Account const& account,
        jtx::Account const& stableCoinOwner,
        uint160 const& assetType,
        STAmount const& amt)
    {
        using namespace jtx;
        Json::Value jv;
        jv[jss::TransactionType] = jss::CDPDeposit;
        jv[jss::Flags] = tfUniversal;
        jv[jss::Account] = account.human();
        jv[jss::StableCoinOwner] = stableCoinOwner.human();
        jv[jss::AssetType] = strHex(assetType);
        jv[jss::Amount] = amt.getJson(JsonOptions::none);
        return jv;
    }

    [[nodiscard]] static Json::Value
    withdrawCDP(
        jtx::Account const& account,
        jtx::Account const& stableCoinOwner,
        uint160 const& assetType,
        STAmount const& amt)
    {
        using namespace jtx;
        Json::Value jv;
        jv[jss::TransactionType] = jss::CDPWithdraw;
        jv[jss::Flags] = tfUniversal;
        jv[jss::Account] = account.human();
        jv[jss::StableCoinOwner] = stableCoinOwner.human();
        jv[jss::AssetType] = strHex(assetType);
        jv[jss::Amount] = amt.getJson(JsonOptions::none);
        return jv;
    }

    [[nodiscard]] static Json::Value
    issueStableCoin(
        jtx::Account const& account,
        jtx::Account const& stableCoinOwner,
        uint160 const& assetType,
        std::uint32_t coinCount)
    {
        using namespace jtx;
        Json::Value jv;
        jv[jss::TransactionType] = jss::StableCoinIssue;
        jv[jss::Flags] = tfUniversal;
        jv[jss::Account] = account.human();
        jv[jss::StableCoinOwner] = stableCoinOwner.human();
        jv[jss::AssetType] = strHex(assetType);
        jv[jss::StableCoinCount] = coinCount;
        return jv;
    }

    enum class RedeemOwnerCDPFirst { no, yes };

    [[nodiscard]] static Json::Value
    redeemStableCoin(
        jtx::Account const& account,
        jtx::Account const& stableCoinOwner,
        uint160 const& assetType,
        std::uint32_t coinCount,
        RedeemOwnerCDPFirst ownerFirst = RedeemOwnerCDPFirst::no)
    {
        using namespace jtx;
        Json::Value jv;
        jv[jss::TransactionType] = jss::StableCoinRedeem;
        if (ownerFirst == RedeemOwnerCDPFirst::yes)
            jv[jss::Flags] = tfUniversal | tfOwnerCDP;
        else
            jv[jss::Flags] = tfUniversal;
        jv[jss::Account] = account.human();
        jv[jss::StableCoinOwner] = stableCoinOwner.human();
        jv[jss::AssetType] = strHex(assetType);
        jv[jss::StableCoinCount] = coinCount;
        return jv;
    }

    [[nodiscard]] static Json::Value
    transferStableCoin(
        jtx::Account const& account,
        jtx::Account const& dst,
        jtx::Account const& stableCoinOwner,
        uint160 const& assetType,
        std::uint32_t coinCount,
        bool accountCDPFirst = false)
    {
        using namespace jtx;
        Json::Value jv;
        jv[jss::TransactionType] = jss::StableCoinTransfer;

        if (accountCDPFirst)
            jv[jss::Flags] = tfUniversal | tfOwnerCDP;
        else
            jv[jss::Flags] = tfUniversal;
        jv[jss::Account] = account.human();
        jv[jss::Destination] = dst.human();
        jv[jss::StableCoinOwner] = stableCoinOwner.human();
        jv[jss::AssetType] = strHex(assetType);
        jv[jss::StableCoinCount] = coinCount;
        return jv;
    }

    void
    testOracle()
    {
        testcase("Oracle");
        using namespace jtx;
        using namespace std::literals;

        auto oSle = [](ReadView const& view,
                       jtx::Account const& account,
                       uint160 const& assetType) {
            return view.read(keylet::oracle(account, assetType));
        };

        auto checkEmpty = [&](ReadView const& view,
                              jtx::Account const& account,
                              uint160 const& assetType) {
            auto const slep = oSle(view, account, assetType);
            BEAST_EXPECT(slep);
            if (!slep)
                return;
            BEAST_EXPECT((*slep)[sfAssetType] == assetType);

            BEAST_EXPECT(
                !(*slep)[~sfValidAfter] && !(*slep)[~sfExpiration] &&
                !(*slep)[~sfOracleXRPValue] && !(*slep)[~sfOracleAssetCount] &&
                slep->getFieldV256(sfOracleUsers).empty());
        };

        auto checkValues = [&](ReadView const& view,
                               jtx::Account const& account,
                               uint160 const& assetType,
                               NetClock::time_point const& validAfter,
                               NetClock::time_point const& expiration,
                               std::uint32_t assetCount,
                               STAmount const& xrpVal) {
            auto const slep = oSle(view, account, assetType);
            BEAST_EXPECT(slep);
            if (!slep)
                return;
            BEAST_EXPECT(
                (*slep)[~sfValidAfter] ==
                validAfter.time_since_epoch().count());
            BEAST_EXPECT(
                (*slep)[~sfExpiration] ==
                expiration.time_since_epoch().count());
            BEAST_EXPECT((*slep)[~sfOracleXRPValue] == xrpVal.value());
            BEAST_EXPECT((*slep)[~sfOracleAssetCount] == assetCount);
            BEAST_EXPECT(slep->getFieldV256(sfOracleUsers).empty());
            BEAST_EXPECT((*slep)[~sfAssetType] == assetType);
        };

        uint160 const assetType{to_currency("USD")};
        auto const alice = Account("alice");

        {
            Env env(*this);
            env.fund(XRP(10000), alice);
            env.close();
            env(createOracle(alice, assetType));
            env.close();
            checkEmpty(*env.current(), alice, assetType);
        }

        struct TestCase
        {
            NetClock::duration validAfterDelta;
            NetClock::duration expirationDelta;
            std::uint32_t assetCount;
            PrettyAmount xrpValue;
            TER ter{tesSUCCESS};
        };

        auto testCases = []()
            -> std::vector<std::pair<TestCase, boost::optional<TestCase>>> {
            std::uint32_t startAssetCount = 10;
            auto const startXRPValue = XRP(100);
            std::uint32_t updateAssetCount = 20;
            auto const updateXRPValue = XRP(200);
            TestCase startValue{0s, 1000s, startAssetCount, startXRPValue};

            // clang-format off
            /*
              Note: New value can't be in the past.
              Check if the old value should be replaced with the new value
              | In Range Old | In Range New | New Exp >= Old Exp | New Replaces Old
              |
              |--------------+--------------+-------------------+------------------|
              | No           | No           | No                | No               |
              | No           | No           | Yes               | Yes              |
              | No           | Yes          | No                | Yes              |
              | No           | Yes          | Yes               | Yes              |
              | Yes          | No           | No                | No               |
              | Yes          | No           | Yes               | No               |
              | Yes          | Yes          | No                | No               |
              | Yes          | Yes          | Yes               | Yes              |

              tested
              | No           | No           | No                | No               |
              | No           | No           | Yes               | Yes              |
              | No           | Yes          | Yes               | Yes              |
              | Yes          | No           | No                | No               |
              | Yes          | No           | Yes               | No               |
              | Yes          | Yes          | No                | No               |

              not tested
              | No           | Yes          | No                | Yes              |

            */
            // clang-format on

            std::vector<std::pair<TestCase, boost::optional<TestCase>>> r{

                // Normal case. New is valid and has a larger expiration time.
                {startValue,
                 TestCase{
                     0s,
                     startValue.expirationDelta + 200s,
                     updateAssetCount,
                     updateXRPValue}},

                // old succeeds, but is out of range when new updates.
                // New is also out of range, older exiration. Should fail.
                {TestCase{0s, 4s, startAssetCount, startXRPValue},
                 TestCase{
                     -100s,
                     -10s,
                     updateAssetCount,
                     updateXRPValue,
                     tecBAD_ORACLE_UPDATE}},

                // old succeeds, but is out of range when new updates.
                // New is also out of range, newer exiration. Should fail.
                {TestCase{0s, 1s, startAssetCount, startXRPValue},
                 TestCase{
                     5000s,
                     6000s,
                     updateAssetCount,
                     updateXRPValue,
                     tecBAD_ORACLE_UPDATE}},

                // Old is in range when new updates; New is out of range; New
                // has larger expiration. New should fail.
                {startValue,
                 TestCase{
                     200s,
                     startValue.expirationDelta + 200s,
                     updateAssetCount,
                     updateXRPValue,
                     tecBAD_ORACLE_UPDATE}},

                // Old is in range when new updates; New is out of range; New
                // has smaller expiration. New should fail.
                {startValue,
                 TestCase{
                     -100s,
                     -90s,
                     updateAssetCount,
                     updateXRPValue,
                     tecBAD_ORACLE_UPDATE}},

                // old succeeds, but is out of range when new updates.
                // New is in range. New should succeed.
                {TestCase{0s, 1s, startAssetCount, startXRPValue},
                 TestCase{0s, 100s, updateAssetCount, updateXRPValue}},

                // Old is in range when new updates; New is in range, but has a
                // smaller expiration date. New should fail.
                {startValue,
                 TestCase{
                     0s,
                     startValue.expirationDelta - 200s,
                     updateAssetCount,
                     updateXRPValue,
                     tecBAD_ORACLE_UPDATE}},

                // Expiration equal to valid after
                {TestCase{
                     0s, 0s, startAssetCount, startXRPValue, temBAD_EXPIRATION},
                 boost::none},

                // Expiration less than valid after
                {TestCase{
                     100s,
                     90s,
                     startAssetCount,
                     startXRPValue,
                     temBAD_EXPIRATION},
                 boost::none},

                // value in past
                {TestCase{
                     -200s,
                     -100s,
                     startAssetCount,
                     startXRPValue,
                     tecBAD_ORACLE_UPDATE},
                 boost::none},

                // value in future, should succeed
                {TestCase{1000s, 2000s, startAssetCount, startXRPValue},
                 boost::none},
            };
            return r;
        }();

        auto updateFromTestCase = [&alice, &assetType, &checkValues](
                                      Env& env, TestCase const& tc) {
            auto const parentClose = env.current()->info().parentCloseTime;
            NetClock::time_point const validAfter =
                parentClose + tc.validAfterDelta;
            NetClock::time_point const expiration =
                parentClose + tc.expirationDelta;
            env(updateOracle(
                    alice,
                    assetType,
                    validAfter,
                    expiration,
                    tc.assetCount,
                    tc.xrpValue),
                ter(tc.ter));

            if (tc.ter == tesSUCCESS)
            {
                checkValues(
                    *env.current(),
                    alice,
                    assetType,
                    validAfter,
                    expiration,
                    tc.assetCount,
                    tc.xrpValue);
            }
        };

        for (auto const& [start, update] : testCases)
        {
            Env env(*this);
            env.fund(XRP(10000), alice);
            env.close();
            env(createOracle(alice, assetType));
            // Close with a delta so subtractions don't underflow
            env.close(3000s);

            NetClock::time_point startCloseTime =
                env.current()->info().parentCloseTime;

            updateFromTestCase(env, start);

            if (start.ter != tesSUCCESS)
                checkEmpty(*env.current(), alice, assetType);

            if (!update)
                continue;

            updateFromTestCase(env, *update);

            if (update->ter == tesSUCCESS)
                continue;

            if (start.ter == tesSUCCESS)
            {
                NetClock::time_point const startValid =
                    startCloseTime + start.validAfterDelta;
                NetClock::time_point const startExpiration =
                    startCloseTime + start.expirationDelta;
                checkValues(
                    *env.current(),
                    alice,
                    assetType,
                    startValid,
                    startExpiration,
                    start.assetCount,
                    start.xrpValue);
            }
            else
            {
                checkEmpty(*env.current(), alice, assetType);
            }
        }

        for (auto withExistingValue : {true, false})
        {
            // Disable an oracle
            Env env(*this);
            env.fund(XRP(10000), alice);
            env.close();
            env(createOracle(alice, assetType));
            env.close();

            if (withExistingValue)
                updateFromTestCase(env, TestCase{0s, 1000s, 10, XRP(100)});

            env(updateOracle(
                alice,
                assetType,
                std::numeric_limits<std::uint32_t>::max(),
                std::numeric_limits<std::uint32_t>::max(),
                200,
                XRP(200)));

            // Shouldn't be able to update, even tho in range
            updateFromTestCase(
                env, TestCase{0s, 1000s, 20, XRP(200), tecBAD_ORACLE_UPDATE});

            // Can't disable twice
            env(updateOracle(
                    alice,
                    assetType,
                    std::numeric_limits<std::uint32_t>::max(),
                    std::numeric_limits<std::uint32_t>::max(),
                    200,
                    XRP(200)),
                ter(tecBAD_ORACLE_UPDATE));
        }

        for (auto withExistingValue : {true, false})
        {
            // Update the oracle with bad amounts/asset counts
            Env env(*this);
            env.fund(XRP(10000), alice);
            env.close();
            env(createOracle(alice, assetType));
            auto USDA = alice["USD"];
            env.close();

            if (withExistingValue)
                updateFromTestCase(env, TestCase{0s, 1000s, 10, XRP(100)});

            // Update an oracle with a non-xrp value
            env(updateOracle(
                    alice,
                    assetType,
                    0,
                    std::numeric_limits<std::uint32_t>::max(),
                    200,
                    USDA(200)),
                ter(temBAD_AMOUNT));

            // Update an oracle with a zero asset count
            env(updateOracle(
                    alice,
                    assetType,
                    0,
                    std::numeric_limits<std::uint32_t>::max(),
                    0,
                    XRP(200)),
                ter(temBAD_AMOUNT));

            // Update an oracle with a negative xrp amount
            env(updateOracle(
                    alice,
                    assetType,
                    0,
                    std::numeric_limits<std::uint32_t>::max(),
                    200,
                    XRP(-1)),
                ter(temBAD_AMOUNT));

            // Update an oracle with a zero xrp amount
            env(updateOracle(
                    alice,
                    assetType,
                    0,
                    std::numeric_limits<std::uint32_t>::max(),
                    200,
                    XRP(0)),
                ter(temBAD_AMOUNT));
        }

        {
            // TBD add an oracle and a stable coins that use the oracle and make
            // sure users is updated
        }
    }  // namespace test

    void
    testCreateStableCoin()
    {
        testcase("Create Stable Coin");
        using namespace jtx;
        using namespace std::literals;

        auto scSle = [](ReadView const& view,
                        jtx::Account const& account,
                        uint160 const& assetType) {
            return view.read(keylet::stableCoin(account, assetType));
        };

        uint160 const assetType{to_currency("USD")};
        auto const alice = Account("alice");
        auto const oracleID = keylet::oracle(alice, assetType);

        auto checkValues = [&](ReadView const& view,
                               jtx::Account const& account,
                               uint160 const& assetType,
                               uint256 const& oracleID,
                               std::uint32_t issRatio,
                               std::uint32_t lqdRatio,
                               std::uint32_t lqdPenalty,
                               std::uint32_t loanOrgFee,
                               std::uint32_t depositFee) {
            auto const slep = scSle(view, account, assetType);
            BEAST_EXPECT(slep);
            if (!slep)
                return;
            BEAST_EXPECT((*slep)[sfAssetType] == assetType);
            BEAST_EXPECT((*slep)[sfCDPBalance] == XRP(0).value());
            BEAST_EXPECT((*slep)[sfIssuedCoins] == 0);
            BEAST_EXPECT((*slep)[sfIssuanceRatio] == issRatio);
            BEAST_EXPECT((*slep)[sfLiquidationRatio] == lqdRatio);
            BEAST_EXPECT((*slep)[sfOracleID] == oracleID);
            BEAST_EXPECT((*slep)[sfLoanOriginationFee] == loanOrgFee);
            BEAST_EXPECT((*slep)[sfDepositFee] == depositFee);
            BEAST_EXPECT((*slep)[sfLiquidationPenalty] == lqdPenalty);
            BEAST_EXPECT((*slep)[sfDepositFee] == depositFee);
            BEAST_EXPECT((*slep)[sfCDPs].empty());
            BEAST_EXPECT((*slep)[sfCDPAssetRatios].empty());
        };

        struct TestCase
        {
            std::uint32_t lqdRatio_ = 1'000'000'001;
            // issRatio must be greater than lqdRatio
            std::uint32_t issRatio_ = 1'000'000'002;
            std::uint32_t depositFee_ = 1;
            std::uint32_t loanOrgFee_ = 2;
            std::uint32_t lqdPenalty_ = 3;
            bool createOracle_ = true;
            uint160 assetType_{to_currency("USD")};
            TER ter_ = tesSUCCESS;

            TestCase() = default;
            TestCase&
            lqdRatio(std::uint32_t v)
            {
                lqdRatio_ = v;
                return *this;
            }
            TestCase&
            issRatio(std::uint32_t v)
            {
                issRatio_ = v;
                return *this;
            }
            TestCase&
            depositFee(std::uint32_t v)
            {
                depositFee_ = v;
                return *this;
            }
            TestCase&
            loanOrgFee(std::uint32_t v)
            {
                loanOrgFee_ = v;
                return *this;
            }
            TestCase&
            lqdPenalty(std::uint32_t v)
            {
                lqdPenalty_ = v;
                return *this;
            }
            TestCase&
            createOracle(bool v)
            {
                createOracle_ = v;
                return *this;
            }
            TestCase&
            assetType(uint160 const& v)
            {
                assetType_ = v;
                return *this;
            }
            TestCase&
            ter(TER v)
            {
                ter_ = v;
                return *this;
            }
        };

        std::vector<TestCase> testCases;
        testCases.reserve(32);
        testCases.emplace_back();
        // Non-existant oracle
        testCases.emplace_back().createOracle(false).ter(tecNO_ENTRY);
        // Oracle asset type mismatch
        testCases.emplace_back()
            .assetType(uint160{to_currency("EUR")})
            .ter(tecORACLE_ASSET_MISMATCH);
        // lqdRatio > issRatio
        testCases.emplace_back()
            .issRatio(1'000'000'002)
            .lqdRatio(1'000'000'003)
            .ter(temBAD_STABLECOIN_LIQUIDATION_RATIO);
        // Create with out of range parameters
        testCases.emplace_back()
            .issRatio(0'000'000'002)
            .ter(temBAD_STABLECOIN_ISSUANCE_RATIO);
        testCases.emplace_back()
            .depositFee(1'000'000'002)
            .ter(temBAD_STABLECOIN_DEPOSIT_FEE);
        testCases.emplace_back()
            .lqdPenalty(1'000'000'002)
            .ter(temBAD_STABLECOIN_LIQUIDATION_PENALTY);

        for (auto const& tc : testCases)
        {
            Env env(*this);
            env.fund(XRP(10000), alice);
            env.close();
            if (tc.createOracle_)
            {
                env(createOracle(alice, assetType));
                env.close();
            }
            BEAST_EXPECT(!scSle(*env.current(), alice, assetType));
            env(createStableCoin(
                    alice,
                    tc.assetType_,
                    oracleID.key,
                    tc.issRatio_,
                    tc.lqdRatio_,
                    tc.lqdPenalty_,
                    tc.loanOrgFee_,
                    tc.depositFee_),
                ter(tc.ter_));
            if (tc.ter_ == tesSUCCESS)
            {
                BEAST_EXPECT(scSle(*env.current(), alice, assetType));
                checkValues(
                    *env.current(),
                    alice,
                    tc.assetType_,
                    oracleID.key,
                    tc.issRatio_,
                    tc.lqdRatio_,
                    tc.lqdPenalty_,
                    tc.loanOrgFee_,
                    tc.depositFee_);
            }
            else
            {
                BEAST_EXPECT(!scSle(*env.current(), alice, assetType));
            }
        }
        {
            // Try to create a duplicate stable coin object
            Env env(*this);
            env.fund(XRP(10000), alice);
            env.close();
            env(createOracle(alice, assetType));
            env.close();
            BEAST_EXPECT(!scSle(*env.current(), alice, assetType));
            TestCase tc;
            env(createStableCoin(
                alice,
                tc.assetType_,
                oracleID.key,
                tc.issRatio_,
                tc.lqdRatio_,
                tc.lqdPenalty_,
                tc.loanOrgFee_,
                tc.depositFee_));
            env(createStableCoin(
                    alice,
                    tc.assetType_,
                    oracleID.key,
                    tc.issRatio_,
                    tc.lqdRatio_,
                    tc.lqdPenalty_,
                    tc.loanOrgFee_,
                    tc.depositFee_),
                ter(tecDUPLICATE));
        }
    }
    void
    testCDP()
    {
        testcase("Stable Coin CDP");
        using namespace jtx;
        using namespace std::literals;

        auto scSle = [](ReadView const& view,
                        jtx::Account const& account,
                        uint160 const& assetType) {
            return view.read(keylet::stableCoin(account, assetType));
        };

        auto cdpSle = [](ReadView const& view,
                         jtx::Account const& account,
                         uint256 const& scID) {
            return view.read(keylet::cdp(account, scID));
        };

        uint160 const assetType{to_currency("USD")};
        auto const alice = Account("alice");
        auto const bob = Account("bob");
        auto const carol = Account("carol");
        auto const oracleID = keylet::oracle(alice, assetType);
        auto const scID = keylet::stableCoin(alice, assetType);
        auto const bobCDPKey = keylet::cdp(bob, scID.key);
        auto const carolCDPKey = keylet::cdp(carol, scID.key);
        std::uint32_t issRatio = 1'200'000'000;
        std::uint32_t lqdRatio = 1'100'000'000;
        std::uint32_t lqdPenalty = 3;
        // 10% deposit fee
        std::uint32_t depositFee = 100'000'000;
        std::uint32_t loanOrgFee = 200'000'000;
        STAmount const initialOracleValue{2};

        auto setupEnv = [&](Env& env) {
            env.fund(XRP(10000), alice, bob, carol);
            env.close();
            env(createOracle(alice, assetType));
            env.close();
            env(createStableCoin(
                alice,
                assetType,
                oracleID.key,
                issRatio,
                lqdRatio,
                lqdPenalty,
                loanOrgFee,
                depositFee));
            env.close();
            env(updateOracle(
                alice,
                assetType,
                /*validAfter*/ 0,
                /*expiration*/ std::numeric_limits<std::uint32_t>::max(),
                /*asset count*/ 1,
                initialOracleValue));
            env.close();
        };

        auto checkSCValues = [&scSle, this](
                                 Env& env,
                                 Account const& scOwner,
                                 uint160 const assetType,
                                 STAmount const& cdpBal,
                                 STAmount const& stabilityPoolBal,
                                 std::vector<uint256> const& cdps) {
            auto const scPtr = scSle(*env.current(), scOwner, assetType);
            BEAST_EXPECT(scPtr);
            if (!scPtr)
                return;
            auto const& sc = *scPtr;
            BEAST_EXPECT(sc[sfCDPBalance] == cdpBal);
            BEAST_EXPECT(sc[sfStabilityPoolBalance] == stabilityPoolBal);
            STVector256 const v = sc.getFieldV256(sfCDPs);
            BEAST_EXPECT(cdps.size() == v.size());
            for (auto const& id : cdps)
                BEAST_EXPECT(std::find(v.begin(), v.end(), id) != v.end());
            STVector64 cdpRatios = sc.getFieldV64(sfCDPAssetRatios);
            BEAST_EXPECT(cdps.size() == cdpRatios.size());

            auto checkCDPAssetRatio = [&](SLE const& scSLE,
                                          SLE const& cdpSLE,
                                          uint256 const& cdpKey,
                                          std::uint64_t expected) -> void {
                decltype(sfIssuedCoins)::type::value_type const numCoins =
                    cdpSLE[sfIssuedCoins];
                if (numCoins == 0)
                {
                    BEAST_EXPECT(
                        expected == std::numeric_limits<std::uint64_t>::max());
                    return;
                }
                STAmount const balance = cdpSLE[sfBalance];
                // rate is balance/numCoins
                BEAST_EXPECT(expected == getRate(STAmount{numCoins}, balance));
            };
            for (int i = 0, e = cdps.size(); i != e; ++i)
            {
                // Check the cdp ratios
                auto const cdpSLE =
                    env.current()->read(keylet::unchecked(cdps[i]));
                BEAST_EXPECT(cdpSLE);
                if (!cdpSLE)
                    continue;
                checkCDPAssetRatio(sc, *cdpSLE, cdps[i], cdpRatios[i]);
            }
        };

        auto scNumIssued =
            [&scSle](Env& env, Account const& scOwner, uint160 const assetType)
            -> boost::optional<std::uint32_t> {
            auto const sle = scSle(*env.current(), scOwner, assetType);
            if (!sle)
                return boost::none;
            return (*sle)[sfIssuedCoins];
        };

        auto scStabilityPoolBal =
            [&scSle](Env& env, Account const& scOwner, uint160 const assetType)
            -> boost::optional<STAmount> {
            auto const sle = scSle(*env.current(), scOwner, assetType);
            if (!sle)
                return boost::none;
            return (*sle)[sfStabilityPoolBalance];
        };

        auto cdpBal = [&cdpSle](
                          Env& env,
                          Account const& cdpOwner,
                          uint256 const& scID) -> boost::optional<STAmount> {
            auto const sle = cdpSle(*env.current(), cdpOwner, scID);
            if (!sle)
                return boost::none;
            return (*sle)[sfBalance];
        };

        auto cdpNumIssued =
            [&cdpSle](Env& env, Account const& cdpOwner, uint256 const& scID)
            -> boost::optional<std::uint32_t> {
            auto const sle = cdpSle(*env.current(), cdpOwner, scID);
            if (!sle)
                return boost::none;
            return (*sle)[sfIssuedCoins];
        };

        auto scAccBal =
            [](Env& env,
               Account const& account,
               uint256 const& scID) -> boost::optional<std::uint32_t> {
            auto const accSCBalSLE =
                env.current()->read(keylet::stableCoinBalance(account, scID));
            if (!accSCBalSLE)
                return boost::none;
            return (*accSCBalSLE)[sfStableCoinBalance];
        };

        {
            // CDP Create w/ no initial balance
            Env env(*this);
            setupEnv(env);
            auto const preBobBalance = env.balance(bob);
            auto const txnFee = env.current()->fees().base;
            env(createCDP(bob, alice, assetType));
            env.close();
            checkSCValues(
                env,
                alice,
                assetType,
                STAmount{0},
                STAmount{0},
                std::vector{bobCDPKey.key});

            BEAST_EXPECT(cdpBal(env, bob, scID.key) == STAmount{0});
            BEAST_EXPECT(env.balance(bob) == preBobBalance - txnFee);
        }

        {
            // CDP create w/ initial balance
            Env env(*this);
            setupEnv(env);
            auto const txnFee = env.current()->fees().base;
            auto const accDebitAmt = STAmount{10};
            // 10% deposit fee.
            auto const depositFee = STAmount{1};
            auto const cdpCreditAmt = accDebitAmt - depositFee;
            auto const preBobBalance = env.balance(bob);
            env(createCDP(bob, alice, assetType, accDebitAmt));
            env.close();
            checkSCValues(
                env,
                alice,
                assetType,
                cdpCreditAmt,
                depositFee,
                std::vector{bobCDPKey.key});
            BEAST_EXPECT(cdpBal(env, bob, scID.key) == cdpCreditAmt);
            BEAST_EXPECT(
                env.balance(bob) == preBobBalance - txnFee - accDebitAmt);
        }

        {
            // CDP create with bad amounts
            Env env(*this);
            setupEnv(env);
            auto USDA = alice["USD"];
            // non xrp
            env(createCDP(bob, alice, assetType, USDA(10).value()),
                ter(temBAD_AMOUNT));
            // negative xrp
            env(createCDP(bob, alice, assetType, XRP(-1).value()),
                ter(temBAD_AMOUNT));
            env.close();
        }

        {
            // create two cdps, deposit and withdraw from them
            Env env(*this);
            setupEnv(env);
            auto const txnFee = env.current()->fees().base;
            auto const accDebitAmt = STAmount{10};
            // 10% deposit fee.
            auto const depositFee = STAmount{1};
            auto const cdpCreditAmt = accDebitAmt - depositFee;
            auto const accWithdrawAmt = STAmount{5};

            STAmount cdpRunningTotal{0};  // total of all xrp in cdps
            STAmount stabilityPoolRunningTotal{
                0};  // total fees contributed to the stability pool
            {
                // Bob creates a cdp
                cdpRunningTotal += cdpCreditAmt;
                stabilityPoolRunningTotal += depositFee;
                auto const preBobBalance = env.balance(bob);
                env(createCDP(bob, alice, assetType, accDebitAmt));
                env.close();
                checkSCValues(
                    env,
                    alice,
                    assetType,
                    cdpRunningTotal,
                    stabilityPoolRunningTotal,
                    std::vector{bobCDPKey.key});
                BEAST_EXPECT(cdpBal(env, bob, scID.key) == cdpCreditAmt);
                BEAST_EXPECT(
                    env.balance(bob) == preBobBalance - txnFee - accDebitAmt);
            }

            {
                // carol creates a cdp
                cdpRunningTotal += cdpCreditAmt;
                stabilityPoolRunningTotal += depositFee;
                auto const preCarolBalance = env.balance(carol);
                env(createCDP(carol, alice, assetType, accDebitAmt));
                env.close();
                checkSCValues(
                    env,
                    alice,
                    assetType,
                    cdpRunningTotal,
                    stabilityPoolRunningTotal,
                    std::vector{bobCDPKey.key, carolCDPKey.key});
                BEAST_EXPECT(cdpBal(env, carol, scID.key) == cdpCreditAmt);
                BEAST_EXPECT(
                    env.balance(carol) ==
                    preCarolBalance - txnFee - accDebitAmt);
            }

            {
                // carol deposits into her cdp
                cdpRunningTotal += cdpCreditAmt;
                stabilityPoolRunningTotal += depositFee;
                auto const preCarolBalance = env.balance(carol);
                env(depositCDP(carol, alice, assetType, accDebitAmt));
                env.close();
                checkSCValues(
                    env,
                    alice,
                    assetType,
                    cdpRunningTotal,
                    stabilityPoolRunningTotal,
                    std::vector{bobCDPKey.key, carolCDPKey.key});

                // carol deposited twice (once when creating, once here)
                BEAST_EXPECT(
                    cdpBal(env, carol, scID.key) ==
                    cdpCreditAmt + cdpCreditAmt);
                BEAST_EXPECT(
                    env.balance(carol) ==
                    preCarolBalance - txnFee - accDebitAmt);
            }

            {
                // Deposit bad amounts
                auto USDA = alice["USD"];
                env(depositCDP(carol, alice, assetType, USDA(100)),
                    ter(temBAD_AMOUNT));
                env(depositCDP(carol, alice, assetType, XRP(-1)),
                    ter(temBAD_AMOUNT));
            }

            {
                // carol withdraws from her cdp
                cdpRunningTotal -= accWithdrawAmt;
                auto const preCarolBalance = env.balance(carol);
                env(withdrawCDP(carol, alice, assetType, accWithdrawAmt));
                env.close();
                checkSCValues(
                    env,
                    alice,
                    assetType,
                    cdpRunningTotal,
                    stabilityPoolRunningTotal,
                    std::vector{bobCDPKey.key, carolCDPKey.key});
                // deposit when created, + one deposit, - one withdrawal
                BEAST_EXPECT(
                    cdpBal(env, carol, scID.key) ==
                    cdpCreditAmt + cdpCreditAmt - accWithdrawAmt);
                BEAST_EXPECT(
                    env.balance(carol) ==
                    preCarolBalance - txnFee + accWithdrawAmt);
            }

            {
                // Withdraw bad amounts
                auto USDA = alice["USD"];
                env(withdrawCDP(carol, alice, assetType, USDA(100)),
                    ter(temBAD_AMOUNT));
                env(withdrawCDP(carol, alice, assetType, XRP(-1)),
                    ter(temBAD_AMOUNT));
            }

            {
                {
                    // Carol issues three coins from her cdp
                    BEAST_EXPECT(scNumIssued(env, alice, assetType) == 0u);
                    BEAST_EXPECT(
                        scStabilityPoolBal(env, alice, assetType) ==
                        stabilityPoolRunningTotal);
                    BEAST_EXPECT(!scAccBal(env, carol, scID.key));
                    BEAST_EXPECT(cdpNumIssued(env, carol, scID.key) == 0u);
                    auto const preCarolBalance = env.balance(carol);
                    auto const preCarolCDPBal = cdpBal(env, carol, scID.key);
                    BEAST_EXPECT(preCarolCDPBal);
                    env(issueStableCoin(carol, alice, assetType, 3u));
                    env.close();

                    // 3 coins issued. That's 6 drops. Loan org fee is 20%. fee
                    // should be 1.2 drops, which is rounded down to 1 drop.
                    STAmount const issueFee{1};
                    // Isn't much of a test is issue fee is zero
                    BEAST_EXPECT(issueFee);
                    stabilityPoolRunningTotal += issueFee;

                    BEAST_EXPECT(scNumIssued(env, alice, assetType) == 3u);
                    BEAST_EXPECT(
                        scStabilityPoolBal(env, alice, assetType) ==
                        stabilityPoolRunningTotal);
                    BEAST_EXPECT(scAccBal(env, carol, scID.key) == 3u);
                    BEAST_EXPECT(cdpNumIssued(env, carol, scID.key) == 3u);
                    BEAST_EXPECT(
                        cdpBal(env, carol, scID.key) ==
                        *preCarolCDPBal - issueFee);
                }

                {
                    // Change the colateral ratio to under the iss thresh by
                    // chnaging the oracle value and issue again, should fail
                    // There is 12 XRP in the CDP with 3 coins issued (debt ==
                    // 6xrp) The issuance ratio is 120%, so this reqires 8 xrp,
                    // which we have However, if the oracle is updated so the
                    // debt is above 10, the required collateral will be above
                    // 12, and new issues should fail. If one coin > 10/3 drops,
                    // the debt will be above 10.

                    BEAST_EXPECT(
                        cdpBal(env, carol, scID.key) == drops(12).value());
                    env(updateOracle(
                        alice,
                        assetType,
                        /*validAfter*/ 0,
                        /*expiration*/
                        std::numeric_limits<std::uint32_t>::max(),
                        /*asset count*/ 3,
                        drops(11).value()));
                    env.close();

                    env(issueStableCoin(carol, alice, assetType, 1u),
                        ter(tecSTABLECOIN_ISSUANCE_RATIO));
                    env.close();

                    // Chanage back to the original value
                    env(updateOracle(
                        alice,
                        assetType,
                        /*validAfter*/ 0,
                        /*expiration*/
                        std::numeric_limits<std::uint32_t>::max(),
                        /*asset count*/ 1,
                        drops(2).value()));
                    env.close();

                    // This time issuing should work
                    env(issueStableCoin(carol, alice, assetType, 1u));
                    env.close();

                    // Fee is 2*.2, or .4 drops, which rounds down to zero. But
                    // keep this calculation here in case the test changes and
                    // the fee needs to be accounted for.
                    STAmount const issueFee{0};
                    stabilityPoolRunningTotal += issueFee;
                }

                // Change the colateral ratio to above the iss thresh by
                // chnaging the oracle value and issue again, should work

                {
                    // Carol transfers one of her coins to bob
                    env(transferStableCoin(carol, bob, alice, assetType, 1));
                    env.close();
                    BEAST_EXPECT(
                        scAccBal(env, carol, scID.key) ==
                        *cdpNumIssued(env, carol, scID.key) - 1);
                    BEAST_EXPECT(scAccBal(env, bob, scID.key) == 1u);
                }

                {
                    // Try to redeem more coins than the account owns
                    env(redeemStableCoin(bob, alice, assetType, 2),
                        ter(tecSTABLECOIN_UNFUNDED_REDEEM));
                    env.close();
                    BEAST_EXPECT(scAccBal(env, bob, scID.key));
                }
                {
                    env(redeemStableCoin(bob, alice, assetType, 1));
                    env.close();
                    BEAST_EXPECT(!scAccBal(env, bob, scID.key));
                }
                {
                    // carol redeems all her coins
                    auto const preCarolNumCoins =
                        *scAccBal(env, carol, scID.key);
                    BEAST_EXPECT(preCarolNumCoins > 0);
                    env(redeemStableCoin(
                        carol, alice, assetType, preCarolNumCoins));
                    env.close();
                    BEAST_EXPECT(!scAccBal(env, carol, scID.key));
                }
            }
        }

        {
            // test realistic redeem
            Env env(*this);
            setupEnv(env);
            auto const txnFee = env.current()->fees().base;
            enum { idBob, idCarol, idLast };
            std::array<Account, idLast> accounts{bob, carol};
            std::array<STAmount, idLast> accDebitAmt{
                STAmount{10}, STAmount{100}};
            // 10% deposit fee.
            std::array<STAmount, idLast> depositFee{STAmount{1}, STAmount{10}};
            std::array<STAmount, idLast> cdpCreditAmt{
                accDebitAmt[0] - depositFee[0], accDebitAmt[1] - depositFee[1]};
            // vector so it can be fed to checkSCValues
            std::vector<uint256> cdpKeys{bobCDPKey.key, carolCDPKey.key};

            STAmount cdpRunningTotal{0};  // total of all xrp in cdps
            // total fees contributed to the stability pool
            STAmount stabilityPoolRunningTotal{0};
            for (int id = 0; id < idLast; ++id)
            {
                // creates a cdp for each id
                cdpRunningTotal += cdpCreditAmt[id];
                stabilityPoolRunningTotal += depositFee[id];
                auto const preBalance = env.balance(accounts[id]);
                env(createCDP(accounts[id], alice, assetType, accDebitAmt[id]));
                env.close();
                std::vector<uint256> existingCdps{
                    cdpKeys.begin(), cdpKeys.begin() + id + 1};
                checkSCValues(
                    env,
                    alice,
                    assetType,
                    cdpRunningTotal,
                    stabilityPoolRunningTotal,
                    existingCdps);
                BEAST_EXPECT(
                    cdpBal(env, accounts[id], scID.key) == cdpCreditAmt[id]);
                BEAST_EXPECT(
                    env.balance(accounts[id]) ==
                    preBalance - txnFee - accDebitAmt[id]);
            }
            BEAST_EXPECT(
                scStabilityPoolBal(env, alice, assetType) ==
                stabilityPoolRunningTotal);
            for (int id = 0; id < idLast; ++id)
            {
                // issues three coins from each cdp
                BEAST_EXPECT(
                    scNumIssued(env, accounts[id], assetType).value_or(0u) ==
                    0u);
                BEAST_EXPECT(!scAccBal(env, accounts[id], scID.key));
                BEAST_EXPECT(cdpNumIssued(env, accounts[id], scID.key) == 0u);
                auto const preBalance = env.balance(accounts[id]);
                auto const preCDPBal = cdpBal(env, accounts[id], scID.key);
                BEAST_EXPECT(preCDPBal);
                env(issueStableCoin(accounts[id], alice, assetType, 3u));
                env.close();

                // 3 coins issued. That's 6 drops. Loan org fee is 20%. fee
                // should be 1.2 drops, which is rounded down to 1 drop.
                STAmount const issueFee{1};
                // Isn't much of a test is issue fee is zero
                BEAST_EXPECT(issueFee);
                stabilityPoolRunningTotal += issueFee;

                BEAST_EXPECT(
                    scNumIssued(env, alice, assetType) ==
                    static_cast<std::uint32_t>((1 + id) * 3));
                BEAST_EXPECT(
                    scStabilityPoolBal(env, alice, assetType) ==
                    stabilityPoolRunningTotal);
                BEAST_EXPECT(scAccBal(env, accounts[id], scID.key) == 3u);
                BEAST_EXPECT(cdpNumIssued(env, accounts[id], scID.key) == 3u);
                BEAST_EXPECT(
                    cdpBal(env, accounts[id], scID.key) ==
                    *preCDPBal - issueFee);

                checkSCValues(
                    env,
                    alice,
                    assetType,
                    cdpRunningTotal,
                    stabilityPoolRunningTotal,
                    cdpKeys);
            }

            {
                // redeem one coin and confrim it uses the cdp with the lowest
                // asset ratio (bob)
                cdpRunningTotal -= initialOracleValue;
                auto const preBalance = env.balance(carol);
                auto const preSCBalance =
                    scAccBal(env, carol, scID.key).value_or(0);
                auto const preBobCDPBalance =
                    cdpBal(env, bob, scID.key).value_or(STAmount{0});
                auto const txnFee = env.current()->fees().base;
                env(redeemStableCoin(carol, alice, assetType, 1));
                env.close();
                BEAST_EXPECT(
                    env.balance(carol) ==
                    preBalance + initialOracleValue - txnFee);
                BEAST_EXPECT(
                    scAccBal(env, carol, scID.key) == preSCBalance - 1);
                // Confirm redeemed from Bob's CDP
                BEAST_EXPECT(
                    cdpBal(env, bob, scID.key) ==
                    preBobCDPBalance - initialOracleValue);

                BEAST_EXPECT(
                    scAccBal(env, carol, scID.key) ==
                    *cdpNumIssued(env, carol, scID.key) - 1);

                checkSCValues(
                    env,
                    alice,
                    assetType,
                    cdpRunningTotal,
                    stabilityPoolRunningTotal,
                    cdpKeys);
            }

            {
                // carol redeem one coin against her cdp, even tho bob's cdp has
                // a lower asset ratio
                cdpRunningTotal -= initialOracleValue;
                auto const preBalance = env.balance(carol);
                auto const preSCBalance =
                    scAccBal(env, carol, scID.key).value_or(0);
                auto const preCarolCDPBalance =
                    cdpBal(env, carol, scID.key).value_or(STAmount{0});
                auto const txnFee = env.current()->fees().base;
                env(redeemStableCoin(
                    carol, alice, assetType, 1, RedeemOwnerCDPFirst::yes));
                env.close();
                BEAST_EXPECT(
                    env.balance(carol) ==
                    preBalance + initialOracleValue - txnFee);
                BEAST_EXPECT(
                    scAccBal(env, carol, scID.key) == preSCBalance - 1);
                // Confirm redeemed from Carol's CDP
                BEAST_EXPECT(
                    cdpBal(env, carol, scID.key) ==
                    preCarolCDPBalance - initialOracleValue);

                BEAST_EXPECT(
                    scAccBal(env, carol, scID.key) ==
                    *cdpNumIssued(env, carol, scID.key) - 1);

                checkSCValues(
                    env,
                    alice,
                    assetType,
                    cdpRunningTotal,
                    stabilityPoolRunningTotal,
                    cdpKeys);
            }

            {
                // Carol transfers her remaining coins to bob, and bob redeems
                // them all
                auto const preBobSCBalance =
                    scAccBal(env, bob, scID.key).value_or(0);
                auto const preCarolSCBalance =
                    scAccBal(env, carol, scID.key).value_or(0);
                BEAST_EXPECT(preCarolSCBalance > 0);
                env(transferStableCoin(
                    carol, bob, alice, assetType, preCarolSCBalance));
                env.close();
                auto const postBobSCBalance =
                    scAccBal(env, bob, scID.key).value_or(0);
                BEAST_EXPECT(
                    postBobSCBalance == preBobSCBalance + preCarolSCBalance);
                BEAST_EXPECT(scAccBal(env, carol, scID.key).value_or(0) == 0);
                // Redeem all the coins, should span both CDPs
                auto const preBobBalance = env.balance(bob);
                auto const txnFee = env.current()->fees().base;
                env(redeemStableCoin(bob, alice, assetType, postBobSCBalance));
                for (auto const& a : {bob, carol})
                {
                    BEAST_EXPECT(
                        cdpNumIssued(env, a, scID.key).value_or(0) == 0);
                    BEAST_EXPECT(scAccBal(env, a, scID.key).value_or(0) == 0);
                }

                BEAST_EXPECT(
                    preBobBalance - txnFee +
                        multiply(
                            STAmount{postBobSCBalance},
                            initialOracleValue,
                            xrpIssue()) ==
                    env.balance(bob));
            }
        }
    }

    void
    testRm()
    {
        testcase("Stable Coin Rm");
        using namespace jtx;
        using namespace std::literals;

        auto cdpSle = [](ReadView const& view,
                         jtx::Account const& account,
                         uint256 const& scID) {
            return view.read(keylet::cdp(account, scID));
        };

        auto cdpBal = [&cdpSle](
                          Env& env,
                          Account const& cdpOwner,
                          uint256 const& scID) -> boost::optional<STAmount> {
            auto const sle = cdpSle(*env.current(), cdpOwner, scID);
            if (!sle)
                return boost::none;
            return (*sle)[sfBalance];
        };

        uint160 const assetType{to_currency("USD")};
        auto const alice = Account("alice");
        auto const bob = Account("bob");
        auto const carol = Account("carol");
        auto const oracleID = keylet::oracle(alice, assetType);
        auto const scID = keylet::stableCoin(alice, assetType);
        std::uint32_t issRatio = 1'200'000'000;
        std::uint32_t lqdRatio = 1'100'000'000;
        std::uint32_t lqdPenalty = 3;
        // 10% deposit fee
        std::uint32_t depositFee = 100'000'000;
        std::uint32_t loanOrgFee = 200'000'000;

        // Create an oracle and stable coin for alice
        auto setupEnv = [&](Env& env) {
            env.fund(XRP(10000), alice, bob, carol);
            env.close();
            env(createOracle(alice, assetType));
            env.close();
            env(createStableCoin(
                alice,
                assetType,
                oracleID.key,
                issRatio,
                lqdRatio,
                lqdPenalty,
                loanOrgFee,
                depositFee));
            env.close();
            env(updateOracle(
                alice,
                assetType,
                /*validAfter*/ 0,
                /*expiration*/ std::numeric_limits<std::uint32_t>::max(),
                /*asset count*/ 1,
                drops(2).value()));
            env.close();
        };

        {
            // Normal case - create and oracle, stable coin and cdp
            // and delete them in reverse order
            Env env(*this);
            setupEnv(env);
            env(createCDP(bob, alice, assetType));
            env.close();
            env(deleteCDP(bob, alice, assetType));
            env.close();
            env(deleteStableCoin(alice, assetType));
            env.close();
            env(deleteOracle(alice, assetType));
            env.close();
        }

        {
            // Try to remove object that still have obligations
            Env env(*this);
            setupEnv(env);
            env(createCDP(bob, alice, assetType));
            env.close();

            env(deleteOracle(alice, assetType), ter(tecHAS_OBLIGATIONS));
            env.close();
            env(deleteStableCoin(alice, assetType), ter(tecHAS_OBLIGATIONS));
            env.close();

            // Remove them normally
            env(deleteCDP(bob, alice, assetType));
            env.close();
            env(deleteStableCoin(alice, assetType));
            env.close();
            env(deleteOracle(alice, assetType));
            env.close();
        }

        {
            // Remove CDP with a balance, but no issued coins
            Env env(*this);
            setupEnv(env);
            STAmount const accDebitAmt = XRP(10);
            env(createCDP(bob, alice, assetType, accDebitAmt));
            env.close();
            auto const preBobBalance = env.balance(bob);
            auto const preCDPBalance =
                cdpBal(env, bob, scID.key).value_or(STAmount{0});
            BEAST_EXPECT(preCDPBalance > STAmount{0});
            auto const txnFee = env.current()->fees().base;
            env(deleteCDP(bob, alice, assetType));
            env.close();
            BEAST_EXPECT(
                env.balance(bob) == preBobBalance + preCDPBalance - txnFee);
        }
        {
            // Remove CDP with with issued coins, should fail
            Env env(*this);
            setupEnv(env);
            STAmount const accDebitAmt = XRP(10);
            env(createCDP(bob, alice, assetType, accDebitAmt));
            env(updateOracle(
                alice,
                assetType,
                /*validAfter*/ 0,
                /*expiration*/ std::numeric_limits<std::uint32_t>::max(),
                /*asset count*/ 1,
                drops(2).value()));
            env.close();
            env(issueStableCoin(bob, alice, assetType, 3u));
            env.close();
            env(deleteCDP(bob, alice, assetType), ter(tecHAS_OBLIGATIONS));
        }
    }

    // TBD: to test:
    // account reserve violations
    // cdp balance violations
    // xrp creation invariant violations
    // cdp withdraw colateral ratio violation
    // audit StableCoin code for overflow errors
    // Honor deposit auth when transfering stable coins
    // Interactions with deposit auth and cdp creation?
    // Test overflows
    // Redeem against a CDP without enough collateral
    // Redeem againsta  CDP that didn't issue enough coins

    void
    run() override
    {
        testOracle();
        testCreateStableCoin();
        testCDP();
        testRm();
    }
};  // namespace test

BEAST_DEFINE_TESTSUITE(StableCoin, app, ripple);
}  // namespace test
}  // namespace ripple
