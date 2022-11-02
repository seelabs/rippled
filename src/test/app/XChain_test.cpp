//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2022 Ripple Labs Inc.

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

#include <ripple/beast/unit_test/suite.hpp>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/Issue.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STXChainAttestationBatch.h>
#include <ripple/protocol/STXChainBridge.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/TER.h>
#include <ripple/protocol/XChainAttestations.h>

#include <test/jtx.h>
#include <test/jtx/Env.h>
#include <test/jtx/attester.h>
#include <test/jtx/multisign.h>
#include <test/jtx/xchain_bridge.h>

#include <functional>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include <fstream>
#include <iostream>

namespace ripple::test {

// SEnv class - encapsulate jtx::Env to make it more user-friendly,
// for example having APIs that return a *this reference so that calls can be
// chained (fluent interface) allowing to create an environment and use it
// without encapsulating it in a curly brace block.
// ---------------------------------------------------------------------------
template <class T>
struct SEnv
{
    jtx::Env env_;

    SEnv(
        T& s,
        std::unique_ptr<Config> config,
        FeatureBitset features,
        std::unique_ptr<Logs> logs = nullptr,
        beast::severities::Severity thresh = beast::severities::kError)
        : env_(s, std::move(config), features, std::move(logs), thresh)
    {
    }

    SEnv&
    close()
    {
        env_.close();
        return *this;
    }

    SEnv&
    enableFeature(uint256 const feature)
    {
        env_.enableFeature(feature);
        return *this;
    }

    SEnv&
    disableFeature(uint256 const feature)
    {
        env_.app().config().features.erase(feature);
        return *this;
    }

    template <class Arg, class... Args>
    SEnv&
    fund(STAmount const& amount, Arg const& arg, Args const&... args)
    {
        env_.fund(amount, arg, args...);
        return *this;
    }

    template <class JsonValue, class... FN>
    SEnv&
    tx(JsonValue&& jv, FN const&... fN)
    {
        env_(std::forward<JsonValue>(jv), fN...);
        return *this;
    }

    TER
    ter() const
    {
        return env_.ter();
    }

    STAmount
    balance(jtx::Account const& account) const
    {
        return env_.balance(account).value();
    }

    XRPAmount
    reserve(std::uint32_t count)
    {
        return env_.current()->fees().accountReserve(count);
    }

    XRPAmount
    txFee()
    {
        return env_.current()->fees().base;
    }

    std::shared_ptr<SLE const>
    account(jtx::Account const& account)
    {
        return env_.le(account);
    }

    std::shared_ptr<SLE const>
    bridge(Json::Value const& jvb)
    {
        return env_.le(keylet::bridge(STXChainBridge(jvb)));
    }

    std::uint64_t
    claimCount(Json::Value const& jvb)
    {
        return (*bridge(jvb))[sfXChainAccountClaimCount];
    }

    std::uint64_t
    claimID(Json::Value const& jvb)
    {
        return (*bridge(jvb))[sfXChainClaimID];
    }

    std::shared_ptr<SLE const>
    claimID(Json::Value const& jvb, std::uint64_t seq)
    {
        return env_.le(keylet::xChainClaimID(STXChainBridge(jvb), seq));
    }

    std::shared_ptr<SLE const>
    caClaimID(Json::Value const& jvb, std::uint64_t seq)
    {
        return env_.le(
            keylet::xChainCreateAccountClaimID(STXChainBridge(jvb), seq));
    }
};

// XEnv class used for XChain tests. The only difference with SEnv<T> is that it
// funds some default accounts, and that it enables `supported_amendments() |
// FeatureBitset{featureXChainBridge}` by default.
// -----------------------------------------------------------------------------
template <class T>
struct XEnv : public jtx::XChainBridgeObjects, public SEnv<T>
{
    XEnv(T& s, bool side = false)
        : SEnv<T>(
              s,
              jtx::envconfig(jtx::port_increment, side ? 3 : 0),
              features)
    {
        using namespace jtx;
        STAmount xrp_funds{XRP(10000)};

        if (!side)
        {
            this->fund(xrp_funds, mcDoor, mcAlice, mcBob, mcCarol, mcGw);

            // Signer's list must match the attestation signers
            // env_(jtx::signers(mcDoor, quorum, signers));
            for (auto& s : signers)
                this->fund(xrp_funds, s.account);
        }
        else
        {
            this->fund(
                xrp_funds,
                scDoor,
                scAlice,
                scBob,
                scCarol,
                scGw,
                scAttester,
                scReward);

            for (auto& ra : payees)
                this->fund(xrp_funds, ra);

            for (auto& s : signers)
                this->fund(xrp_funds, s.account);

            // Signer's list must match the attestation signers
            // env_(jtx::signers(Account::master, quorum, signers));
        }
        this->close();
    }
};

// Tracks the xrp balance for one account
template <class T>
struct Balance
{
    jtx::Account const& account_;
    T& env_;
    STAmount startAmount;

    Balance(T& env, jtx::Account const& account) : account_(account), env_(env)
    {
        startAmount = env_.balance(account_);
    }

    STAmount
    diff() const
    {
        return env_.balance(account_) - startAmount;
    }
};

// Tracks the xrp balance for multiple accounts involved in a crosss-chain
// transfer
template <class T>
struct BalanceTransfer
{
    using balance = Balance<T>;

    balance from_;
    balance to_;
    balance payor_;                        // pays the rewards
    std::vector<balance> reward_accounts;  // receives the reward
    XRPAmount txFees_;

    BalanceTransfer(
        T& env,
        jtx::Account const& from_acct,
        jtx::Account const& to_acct,
        jtx::Account const& payor,
        jtx::Account const* payees,
        size_t num_payees,
        bool withClaim)
        : from_(env, from_acct)
        , to_(env, to_acct)
        , payor_(env, payor)
        , reward_accounts([&]() {
            std::vector<balance> r;
            r.reserve(num_payees);
            for (size_t i = 0; i < num_payees; ++i)
                r.emplace_back(env, payees[i]);
            return r;
        }())
        , txFees_(withClaim ? env.env_.current()->fees().base : XRPAmount(0))
    {
    }

    BalanceTransfer(
        T& env,
        jtx::Account const& from_acct,
        jtx::Account const& to_acct,
        jtx::Account const& payor,
        std::vector<jtx::Account> const& payees,
        bool withClaim)
        : BalanceTransfer(
              env,
              from_acct,
              to_acct,
              payor,
              &payees[0],
              payees.size(),
              withClaim)
    {
    }

    bool
    payees_received(STAmount const& reward) const
    {
        return std::all_of(
            reward_accounts.begin(),
            reward_accounts.end(),
            [&](const balance& b) { return b.diff() == reward; });
    }

    bool
    check_most_balances(STAmount const& amt, STAmount const& reward)
    {
        return from_.diff() == -amt && to_.diff() == amt &&
            payees_received(reward);
    }

    bool
    has_happened(
        STAmount const& amt,
        STAmount const& reward,
        bool check_payer = true)
    {
        auto reward_cost =
            multiply(reward, STAmount(reward_accounts.size()), reward.issue());
        return check_most_balances(amt, reward) &&
            (!check_payer || payor_.diff() == -(reward_cost + txFees_));
    }

    bool
    has_not_happened()
    {
        return check_most_balances(STAmount(0), STAmount(0)) &&
            payor_.diff() <= txFees_;  // could have paid fee for failed claim
    }
};

struct BridgeDef
{
    jtx::Account doorA;
    Issue issueA;
    jtx::Account doorB;
    Issue issueB;
    STAmount reward;
    STAmount minAccountCreate;
    uint32_t quorum;
    std::vector<jtx::signer> const& signers;
    Json::Value jvb;

    template <class ENV>
    void
    initBridge(ENV& mcEnv, ENV& scEnv)
    {
        jvb = bridge(doorA, issueA, doorB, issueB);

        mcEnv.tx(bridge_create(doorA, jvb, reward, minAccountCreate))
            .tx(jtx::signers(doorA, quorum, signers))
            .close();

        scEnv.tx(bridge_create(doorB, jvb, reward, minAccountCreate))
            .tx(jtx::signers(doorB, quorum, signers))
            .close();
    }
};

struct XChain_test : public beast::unit_test::suite,
                     public jtx::XChainBridgeObjects
{
    XRPAmount
    reserve(std::uint32_t count)
    {
        return XEnv(*this).env_.current()->fees().accountReserve(count);
    }

    XRPAmount
    txFee()
    {
        return XEnv(*this).env_.current()->fees().base;
    }

    void
    testXChainCreateBridge()
    {
        XRPAmount res1 = reserve(1);

        using namespace jtx;
        testcase("Create Bridge");

        // Normal create_bridge => should succeed
        XEnv(*this).tx(create_bridge(mcDoor)).close();

        // Bridge not owned by one of the door account.
        XEnv(*this).tx(create_bridge(mcBob), ter(temSIDECHAIN_NONDOOR_OWNER));

        // Create twice on the same account
        XEnv(*this)
            .tx(create_bridge(mcDoor))
            .close()
            .tx(create_bridge(mcDoor), ter(tecDUPLICATE));

        // Create USD bridge Alice -> Bob ... should succeed
        XEnv(*this).tx(
            create_bridge(
                mcAlice, bridge(mcAlice, mcAlice["USD"], mcBob, mcBob["USD"])),
            ter(tesSUCCESS));

        // Create where both door accounts are on the same chain. The second
        // bridge create should fail.
        XEnv(*this)
            .tx(create_bridge(
                mcAlice, bridge(mcAlice, mcAlice["USD"], mcBob, mcBob["USD"])))
            .close()
            .tx(create_bridge(
                    mcBob,
                    bridge(mcAlice, mcAlice["USD"], mcBob, mcBob["USD"])),
                ter(tecDUPLICATE));

        // Bridge where the two door accounts are equal.
        XEnv(*this).tx(
            create_bridge(
                mcBob, bridge(mcBob, mcBob["USD"], mcBob, mcBob["USD"])),
            ter(temEQUAL_DOOR_ACCOUNTS));

        // Create an bridge on an account with exactly enough balance to
        // meet the new reserve should succeed
        XEnv(*this)
            .fund(res1, mcuDoor)  // exact reserve for account + 1 object
            .close()
            .tx(create_bridge(mcuDoor, jvub), ter(tesSUCCESS));

        // Create an bridge on an account with no enough balance to meet the
        // new reserve
        XEnv(*this)
            .fund(res1 - 1, mcuDoor)  // just short of required reserve
            .close()
            .tx(create_bridge(mcuDoor, jvub), ter(tecINSUFFICIENT_RESERVE));

        // Reward amount is non-xrp
        XEnv(*this).tx(
            create_bridge(mcDoor, jvb, mcUSD(1)),
            ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT));

        // Reward amount is XRP and negative
        XEnv(*this).tx(
            create_bridge(mcDoor, jvb, XRP(-1)),
            ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT));

        // Reward amount is zero
        XEnv(*this).tx(
            create_bridge(mcDoor, jvb, XRP(0)),
            ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT));

        // Reward amount is 1 xrp => should succeed
        XEnv(*this).tx(create_bridge(mcDoor, jvb, XRP(1)), ter(tesSUCCESS));

        // Min create amount is 1 xrp, mincreate is 1 xrp => should succeed
        XEnv(*this).tx(
            create_bridge(mcDoor, jvb, XRP(1), XRP(1)), ter(tesSUCCESS));

        // Min create amount is non-xrp
        XEnv(*this).tx(
            create_bridge(mcDoor, jvb, XRP(1), mcUSD(100)),
            ter(temXCHAIN_BRIDGE_BAD_MIN_ACCOUNT_CREATE_AMOUNT));

        // Min create amount is zero (should fail, currently succeeds)
        XEnv(*this).tx(
            create_bridge(mcDoor, jvb, XRP(1), XRP(0)),
            ter(temXCHAIN_BRIDGE_BAD_MIN_ACCOUNT_CREATE_AMOUNT));

        // Min create amount is negative
        XEnv(*this).tx(
            create_bridge(mcDoor, jvb, XRP(1), XRP(-1)),
            ter(temXCHAIN_BRIDGE_BAD_MIN_ACCOUNT_CREATE_AMOUNT));

        // coverage test: BridgeCreate::preflight() - create bridge when feature
        // disabled.
        {
            Env env(*this);
            env(create_bridge(Account::master, jvb), ter(temDISABLED));
        }

        // coverage test: BridgeCreate::preclaim() returns tecNO_ISSUER.
        XEnv(*this).tx(
            create_bridge(
                mcAlice, bridge(mcAlice, mcuAlice["USD"], mcBob, mcBob["USD"])),
            ter(tecNO_ISSUER));

        // coverage test: create_bridge transaction with incorrect flag
        XEnv(*this).tx(
            create_bridge(mcAlice, jvb),
            txflags(tfFillOrKill),
            ter(temINVALID_FLAG));

        // coverage test: create_bridge transaction with xchain feature disabled
        XEnv(*this)
            .disableFeature(featureXChainBridge)
            .tx(create_bridge(mcAlice, jvb), ter(temDISABLED));
    }

    void
    testXChainCreateBridgeMatrix()
    {
        using namespace jtx;
        testcase("Create Bridge Matrix");

        // Test all combinations of the following:`
        // --------------------------------------
        // - Locking chain is IOU with locking chain door account as issuer
        // - Locking chain is IOU with issuing chain door account that
        //   exists on the locking chain as issuer
        // - Locking chain is IOU with issuing chain door account that does
        //   not exists on the locking chain as issuer
        // - Locking chain is IOU with non-door account (that exists on the
        //   locking chain ledger) as issuer
        // - Locking chain is IOU with non-door account (that does not exist
        //   exists on the locking chain ledger) as issuer
        // - Locking chain is XRP
        // ---------------------------------------------------------------------
        // - Issuing chain is IOU with issuing chain door account as the
        //   issuer
        // - Issuing chain is IOU with locking chain door account (that
        //   exists on the issuing chain ledger) as the issuer
        // - Issuing chain is IOU with locking chain door account (that does
        //   not exist on the issuing chain ledger) as the issuer
        // - Issuing chain is IOU with non-door account (that exists on the
        //   issuing chain ledger) as the issuer
        // - Issuing chain is IOU with non-door account (that does not
        //   exists on the issuing chain ledger) as the issuer
        // - Issuing chain is XRP and issuing chain door account is not the
        //   root account
        // - Issuing chain is XRP and issuing chain door account is the root
        //   account
        // ---------------------------------------------------------------------
        // That's 42 combinations. The only combinations that should succeed
        // are:
        // - Locking chain is any IOU,
        // - Issuing chain is IOU with issuing chain door account as the issuer
        //   Locking chain is XRP,
        // - Issuing chain is XRP with issuing chain is the root account.
        // ---------------------------------------------------------------------
        Account a, b;
        Issue ia, ib;

        std::tuple lcs{
            std::make_pair(
                "Locking chain is IOU(locking chain door)",
                [&](auto& env, bool) {
                    a = mcDoor;
                    ia = mcDoor["USD"];
                }),
            std::make_pair(
                "Locking chain is IOU(issuing chain door funded on locking "
                "chain)",
                [&](auto& env, bool shouldFund) {
                    a = mcDoor;
                    ia = scDoor["USD"];
                    if (shouldFund)
                        env.fund(XRP(10000), scDoor);
                }),
            std::make_pair(
                "Locking chain is IOU(issuing chain door account unfunded "
                "on locking chain)",
                [&](auto& env, bool) {
                    a = mcDoor;
                    ia = scDoor["USD"];
                }),
            std::make_pair(
                "Locking chain is IOU(bob funded on locking chain)",
                [&](auto& env, bool) {
                    a = mcDoor;
                    ia = mcGw["USD"];
                }),
            std::make_pair(
                "Locking chain is IOU(bob unfunded on locking chain)",
                [&](auto& env, bool) {
                    a = mcDoor;
                    ia = mcuGw["USD"];
                }),
            std::make_pair("Locking chain is XRP", [&](auto& env, bool) {
                a = mcDoor;
                ia = xrpIssue();
            })};

        std::tuple ics{
            std::make_pair(
                "Issuing chain is IOU(issuing chain door account)",
                [&](auto& env, bool) {
                    b = scDoor;
                    ib = scDoor["USD"];
                }),
            std::make_pair(
                "Issuing chain is IOU(locking chain door funded on issuing "
                "chain)",
                [&](auto& env, bool shouldFund) {
                    b = scDoor;
                    ib = mcDoor["USD"];
                    if (shouldFund)
                        env.fund(XRP(10000), mcDoor);
                }),
            std::make_pair(
                "Issuing chain is IOU(locking chain door unfunded on "
                "issuing chain)",
                [&](auto& env, bool) {
                    b = scDoor;
                    ib = mcDoor["USD"];
                }),
            std::make_pair(
                "Issuing chain is IOU(bob funded on issuing chain)",
                [&](auto& env, bool) {
                    b = scDoor;
                    ib = mcGw["USD"];
                }),
            std::make_pair(
                "Issuing chain is IOU(bob unfunded on issuing chain)",
                [&](auto& env, bool) {
                    b = scDoor;
                    ib = mcuGw["USD"];
                }),
            std::make_pair(
                "Issuing chain is XRP and issuing chain door account is "
                "not the root account",
                [&](auto& env, bool) {
                    b = scDoor;
                    ib = xrpIssue();
                }),
            std::make_pair(
                "Issuing chain is XRP and issuing chain door account is "
                "the root account ",
                [&](auto& env, bool) {
                    b = Account::master;
                    ib = xrpIssue();
                })};

        std::vector<std::pair<int, int>> expected_result{
            {tesSUCCESS, tesSUCCESS},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {tesSUCCESS, tesSUCCESS},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {tecNO_ISSUER, tesSUCCESS},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {tesSUCCESS, tesSUCCESS},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {tecNO_ISSUER, tesSUCCESS},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {temSIDECHAIN_BAD_ISSUES, temSIDECHAIN_BAD_ISSUES},
            {tesSUCCESS, tesSUCCESS}};

        std::vector<std::tuple<TER, TER, bool>> test_result;

        auto testcase = [&](auto const& lc, auto const& ic) {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            lc.second(mcEnv, true);
            lc.second(scEnv, false);

            ic.second(mcEnv, false);
            ic.second(scEnv, true);

            auto const& expected = expected_result[test_result.size()];

            mcEnv.tx(
                create_bridge(a, bridge(a, ia, b, ib)),
                ter(TER::fromInt(expected.first)));
            TER mcTER = mcEnv.env_.ter();

            scEnv.tx(
                create_bridge(b, bridge(a, ia, b, ib)),
                ter(TER::fromInt(expected.second)));
            TER scTER = scEnv.env_.ter();

            bool pass = mcTER == tesSUCCESS && scTER == tesSUCCESS;

            test_result.emplace_back(mcTER, scTER, pass);
        };

        auto apply_ics = [&](auto const& lc, auto const& ics) {
            std::apply(
                [&](auto const&... ic) { (testcase(lc, ic), ...); }, ics);
        };

        std::apply([&](auto const&... lc) { (apply_ics(lc, ics), ...); }, lcs);

#if GENERATE_MTX_OUTPUT
        // optional output of matrix results in markdown format
        // ----------------------------------------------------
        std::string fname{std::tmpnam(nullptr)};
        fname += ".md";
        std::cout << "Markdown output for matrix test: " << fname << "\n";

        auto print_res = [](auto tup) -> std::string {
            std::string status = std::string(transToken(std::get<0>(tup))) +
                " / " + transToken(std::get<1>(tup));

            if (std::get<2>(tup))
                return status;
            else
            {
                // red
                return std::string("`") + status + "`";
            }
        };

        auto output_table = [&](auto print_res) {
            size_t test_idx = 0;
            std::string res;
            res.reserve(10000);  // should be enough :-)

            // first two header lines
            res += "|  `issuing ->` | ";
            std::apply(
                [&](auto const&... ic) {
                    ((res += ic.first, res += " | "), ...);
                },
                ics);
            res += "\n";

            res += "| :--- | ";
            std::apply(
                [&](auto const&... ic) {
                    (((void)ic.first, res += ":---: |  "), ...);
                },
                ics);
            res += "\n";

            auto output = [&](auto const& lc, auto const& ic) {
                res += print_res(test_result[test_idx]);
                res += " | ";
                ++test_idx;
            };

            auto output_ics = [&](auto const& lc, auto const& ics) {
                res += "| ";
                res += lc.first;
                res += " | ";
                std::apply(
                    [&](auto const&... ic) { (output(lc, ic), ...); }, ics);
                res += "\n";
            };

            std::apply(
                [&](auto const&... lc) { (output_ics(lc, ics), ...); }, lcs);

            return res;
        };

        std::ofstream(fname) << output_table(print_res);

        std::string ter_fname{std::tmpnam(nullptr)};
        std::cout << "ter output for matrix test: " << ter_fname << "\n";

        std::ofstream ofs(ter_fname);
        for (auto& t : test_result)
        {
            ofs << "{ " << std::string(transToken(std::get<0>(t))) << ", "
                << std::string(transToken(std::get<1>(t))) << "}\n,";
        }
#endif
    }

    void
    testXChainModifyBridge()
    {
        using namespace jtx;
        testcase("Modify Bridge");

        // Changing a non-existent bridge should fail
        XEnv(*this).tx(
            bridge_modify(
                mcAlice,
                bridge(mcAlice, mcAlice["USD"], mcBob, mcBob["USD"]),
                XRP(2),
                XRP(10)),
            ter(tecNO_ENTRY));

        // must change something
        // XEnv(*this)
        //    .tx(create_bridge(mcDoor, jvb, XRP(1), XRP(1)))
        //    .tx(bridge_modify(mcDoor, jvb, XRP(1), XRP(1)),
        //    ter(temMALFORMED));

        // must change something
        XEnv(*this)
            .tx(create_bridge(mcDoor, jvb, XRP(1), XRP(1)))
            .close()
            .tx(bridge_modify(mcDoor, jvb, {}, {}), ter(temMALFORMED));

        // Reward amount is non-xrp
        XEnv(*this).tx(
            bridge_modify(mcDoor, jvb, mcUSD(2), XRP(10)),
            ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT));

        // Reward amount is XRP and negative
        XEnv(*this).tx(
            bridge_modify(mcDoor, jvb, XRP(-2), XRP(10)),
            ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT));

        // Reward amount is zero
        XEnv(*this).tx(
            bridge_modify(mcDoor, jvb, XRP(0), XRP(10)),
            ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT));

        // Min create amount is non-xrp
        XEnv(*this).tx(
            bridge_modify(mcDoor, jvb, XRP(2), mcUSD(10)),
            ter(temXCHAIN_BRIDGE_BAD_MIN_ACCOUNT_CREATE_AMOUNT));

        // Min create amount is zero
        XEnv(*this).tx(
            bridge_modify(mcDoor, jvb, XRP(2), XRP(0)),
            ter(temXCHAIN_BRIDGE_BAD_MIN_ACCOUNT_CREATE_AMOUNT));

        // Min create amount is negative
        XEnv(*this).tx(
            bridge_modify(mcDoor, jvb, XRP(2), XRP(-10)),
            ter(temXCHAIN_BRIDGE_BAD_MIN_ACCOUNT_CREATE_AMOUNT));

        // First check the regular claim process (without bridge_modify)
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            Json::Value batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scBob))
                    .close();
            }

            BEAST_EXPECT(transfer.has_happened(amt, split_reward));
        }

        // Check that the reward paid from a claim Id was the reward when
        // the claim id was created, not the reward since the bridge was
        // modified.
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            // Now modify the reward on the bridge
            mcEnv.tx(bridge_modify(mcDoor, jvb, XRP(2), XRP(10))).close();
            scEnv.tx(bridge_modify(Account::master, jvb, XRP(2), XRP(10)))
                .close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            Json::Value batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scBob))
                    .close();
            }

            // make sure the reward accounts indeed received the original
            // split reward (1 split 5 ways) instead of the updated 2 XRP.
            BEAST_EXPECT(transfer.has_happened(amt, split_reward));
        }

        // Check that the signatures used to verify attestations and decide
        // if there is a quorum are the current signer's list on the door
        // account, not the signer's list that was in effect when the claim
        // id was created.
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            // change signers - claim should not be processed is the batch is
            // signed by original signers
            scEnv.tx(jtx::signers(Account::master, quorum, alt_signers))
                .close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            // submit claim using outdated signers - should fail
            Json::Value batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            scEnv
                .tx(xchain_add_attestation_batch(scAttester, batch),
                    ter(tecXCHAIN_PROOF_UNKNOWN_KEY))
                .close();

            if (withClaim)
            {
                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scAlice, jvb, claimID, amt, scBob),
                        ter(tecXCHAIN_CLAIM_NO_QUORUM))
                    .close();
            }

            // make sure transfer has not happened as we sent attestations using
            // outdated signers
            BEAST_EXPECT(transfer.has_not_happened());

            // submit claim using current signers - should succeed
            batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, alt_signers);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scBob))
                    .close();
            }

            // make sure the transfer went through as we sent attestations using
            // new signers
            BEAST_EXPECT(transfer.has_happened(amt, split_reward, false));
        }

        // coverage test: bridge_modify transaction with incorrect flag
        XEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .close()
            .tx(bridge_modify(mcDoor, jvb, XRP(1), XRP(2)),
                txflags(tfFillOrKill),
                ter(temINVALID_FLAG));

        // coverage test: bridge_modify transaction with xchain feature disabled
        XEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .disableFeature(featureXChainBridge)
            .close()
            .tx(bridge_modify(mcDoor, jvb, XRP(1), XRP(2)), ter(temDISABLED));

        // coverage test: bridge_modify return temSIDECHAIN_NONDOOR_OWNER;
        XEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .close()
            .tx(bridge_modify(mcAlice, jvb, XRP(1), XRP(2)),
                ter(temSIDECHAIN_NONDOOR_OWNER));
    }

    void
    testXChainCreateClaimID()
    {
        using namespace jtx;
        XRPAmount res1 = reserve(1);
        XRPAmount tx_fee = txFee();

        testcase("Create ClaimID");

        // normal bridge create for sanity check with the exact necessary
        // account balance
        XEnv(*this, true)
            .tx(create_bridge(Account::master, jvb))
            .fund(res1, scuAlice)  // acct reserve + 1 object
            .close()
            .tx(xchain_create_claim_id(scuAlice, jvb, reward, mcAlice))
            .close();

        // check reward not deducted when claim id is created
        {
            XEnv xenv(*this, true);

            Balance scAlice_bal(xenv, scAlice);

            xenv.tx(create_bridge(Account::master, jvb))
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            BEAST_EXPECT(scAlice_bal.diff() == -tx_fee);
        }

        // Non-existent bridge
        XEnv(*this, true)
            .tx(xchain_create_claim_id(
                    scAlice,
                    bridge(mcAlice, mcAlice["USD"], scBob, scBob["USD"]),
                    reward,
                    mcAlice),
                ter(tecNO_ENTRY))
            .close();

        // Creating the new object would put the account below the reserve
        XEnv(*this, true)
            .tx(create_bridge(Account::master, jvb))
            .fund(res1 - xrp_dust, scuAlice)  // barely not enough
            .close()
            .tx(xchain_create_claim_id(scuAlice, jvb, reward, mcAlice),
                ter(tecINSUFFICIENT_RESERVE))
            .close();

        // The specified reward doesn't match the reward on the bridge (test by
        // giving the reward amount for the other side, as well as a completely
        // non-matching reward)
        XEnv(*this, true)
            .tx(create_bridge(Account::master, jvb))
            .close()
            .tx(xchain_create_claim_id(scAlice, jvb, split_reward, mcAlice),
                ter(tecXCHAIN_REWARD_MISMATCH))
            .close();

        // A reward amount that isn't XRP
        XEnv(*this, true)
            .tx(create_bridge(Account::master, jvb))
            .close()
            .tx(xchain_create_claim_id(scAlice, jvb, mcUSD(1), mcAlice),
                ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT))
            .close();

        // coverage test: xchain_create_claim_id transaction with incorrect flag
        XEnv(*this, true)
            .tx(create_bridge(Account::master, jvb))
            .close()
            .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice),
                txflags(tfFillOrKill),
                ter(temINVALID_FLAG))
            .close();

        // coverage test: xchain_create_claim_id transaction with xchain feature
        // disabled
        XEnv(*this, true)
            .tx(create_bridge(Account::master, jvb))
            .disableFeature(featureXChainBridge)
            .close()
            .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice),
                ter(temDISABLED))
            .close();
    }

    void
    testXChainCommit()
    {
        using namespace jtx;
        XRPAmount res0 = reserve(0);
        XRPAmount tx_fee = txFee();

        testcase("Commit");

        // Commit to a non-existent bridge
        XEnv(*this).tx(
            xchain_commit(mcAlice, jvb, 1, one_xrp, scBob), ter(tecNO_ENTRY));

        // check that reward not deducted when doing the commit
        {
            XEnv xenv(*this);

            Balance alice_bal(xenv, mcAlice);
            auto const amt = XRP(1000);

            xenv.tx(create_bridge(mcDoor, jvb))
                .close()
                .tx(xchain_commit(mcAlice, jvb, 1, amt, scBob))
                .close();

            STAmount claim_cost = amt;
            BEAST_EXPECT(alice_bal.diff() == -(claim_cost + tx_fee));
        }

        // Commit a negative amount
        XEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .close()
            .tx(xchain_commit(mcAlice, jvb, 1, XRP(-1), scBob),
                ter(temBAD_AMOUNT));

        // Commit an amount whose issue that does not match the expected issue
        // on the bridge (either LockingChainIssue or IssuingChainIssue,
        // depending on the chain).
        XEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .close()
            .tx(xchain_commit(mcAlice, jvb, 1, mcUSD(100), scBob),
                ter(tecBAD_XCHAIN_TRANSFER_ISSUE));

        // Commit an amount that would put the sender below the required reserve
        // (if XRP)
        XEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .fund(res0 + one_xrp - xrp_dust, mcuAlice)  // barely not enough
            .close()
            .tx(xchain_commit(mcuAlice, jvb, 1, one_xrp, scBob),
                ter(tecINSUFFICIENT_FUNDS));

        XEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .fund(
                res0 + one_xrp + xrp_dust,  // "xrp_dust" for tx fees
                mcuAlice)                   // exactly enough => should succeed
            .close()
            .tx(xchain_commit(mcuAlice, jvb, 1, one_xrp, scBob));

        // Commit an amount above the account's balance (for both XRP and IOUs)
        XEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .fund(res0, mcuAlice)  // barely not enough
            .close()
            .tx(xchain_commit(mcuAlice, jvb, 1, res0 + one_xrp, scBob),
                ter(tecINSUFFICIENT_FUNDS));

        auto jvb_USD = bridge(mcDoor, mcUSD, scGw, scUSD);

        // commit sent from iou issuer (mcGw) succeeds - should it?
        XEnv(*this)
            .tx(trust(mcDoor, mcUSD(10000)))  // door needs to have a trustline
            .tx(create_bridge(mcDoor, jvb_USD))
            .close()
            .tx(xchain_commit(mcGw, jvb_USD, 1, mcUSD(1), scBob));

        // commit to a door account from the door account. This should fail.
        XEnv(*this)
            .tx(trust(mcDoor, mcUSD(10000)))  // door needs to have a trustline
            .tx(create_bridge(mcDoor, jvb_USD))
            .close()
            .tx(xchain_commit(mcDoor, jvb_USD, 1, mcUSD(1), scBob),
                ter(tecXCHAIN_SELF_COMMIT));

        // commit sent from mcAlice which has no IOU balance => should fail
        XEnv(*this)
            .tx(trust(mcDoor, mcUSD(10000)))  // door needs to have a trustline
            .tx(create_bridge(mcDoor, jvb_USD))
            .close()
            .tx(xchain_commit(mcAlice, jvb_USD, 1, mcUSD(1), scBob),
                ter(terNO_LINE));

        // commit sent from mcAlice which has no IOU balance => should fail
        // just changed the destination to scGw (which is the door account and
        // may not make much sense)
        XEnv(*this)
            .tx(trust(mcDoor, mcUSD(10000)))  // door needs to have a trustline
            .tx(create_bridge(mcDoor, jvb_USD))
            .close()
            .tx(xchain_commit(mcAlice, jvb_USD, 1, mcUSD(1), scGw),
                ter(terNO_LINE));

        // commit sent from mcAlice which has a IOU balance => should succeed
        XEnv(*this)
            .tx(trust(mcDoor, mcUSD(10000)))
            .tx(trust(mcAlice, mcUSD(10000)))
            .close()
            .tx(pay(mcGw, mcAlice, mcUSD(10)))
            .tx(create_bridge(mcDoor, jvb_USD))
            .close()
            //.tx(pay(mcAlice, mcDoor, mcUSD(10)));
            .tx(xchain_commit(mcAlice, jvb_USD, 1, mcUSD(10), scAlice));

        // coverage test: xchain_commit transaction with incorrect flag
        XEnv(*this)
            .tx(create_bridge(mcDoor))
            .close()
            .tx(xchain_commit(mcAlice, jvb, 1, one_xrp, scBob),
                txflags(tfFillOrKill),
                ter(temINVALID_FLAG));

        // coverage test: xchain_commit transaction with xchain feature disabled
        XEnv(*this)
            .tx(create_bridge(mcDoor))
            .disableFeature(featureXChainBridge)
            .close()
            .tx(xchain_commit(mcAlice, jvb, 1, one_xrp, scBob),
                ter(temDISABLED));
    }

    void
    testXChainAddAttestation()
    {
        using namespace jtx;

        testcase("Add Attestation");
        XRPAmount res0 = reserve(0);
        XRPAmount tx_fee = txFee();
        STAmount tx_fee_2 = multiply(tx_fee, STAmount(2), xrpIssue());

        // Add an attestation to a claim id that has already reached quorum.
        // This should succeed and share in the reward.
        // note: this is true only when either:
        //       1. dest account is not specified, so transfer requires a clain
        //       2. or the extra attestation is sent in the same batch as the
        //          one reaching quorum
        for (auto withClaim : {true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);
            std::uint32_t const claimID = 1;

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, claimID));  // claim id present

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            auto batch = attestation_claim_batch(
                jvb,
                mcAlice,
                amt,
                &payees[0],
                true,
                claimID,
                dst,
                &signers[0],
                quorum);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            batch = attestation_claim_batch(
                jvb,
                mcAlice,
                amt,
                &payees[quorum],
                true,
                claimID,
                dst,
                &signers[quorum],
                signers.size() - quorum);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scBob))
                    .close();
                BEAST_EXPECT(!scEnv.claimID(jvb, claimID));  // claim id deleted
                BEAST_EXPECT(scEnv.claimID(jvb) == claimID);
            }

            BEAST_EXPECT(transfer.has_happened(amt, split_reward));
        }

        // Add a batch of attestations where one has an invalid signature. The
        // entire transaction should fail.
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);
            std::uint32_t const claimID = 1;

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, claimID));  // claim id present

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            auto messup_sig =
                [](size_t i, Buffer sig, jtx::signer s, AnyAmount amount) {
                    if (i == 2)
                    {
                        Buffer b(64);
                        std::memset(b.data(), 'a', 64);
                        return std::make_tuple(std::move(b), amount);
                    }
                    else
                        return std::make_tuple(std::move(sig), amount);
                };
            auto batch = attestation_claim_batch(
                jvb,
                mcAlice,
                amt,
                &payees[0],
                true,
                claimID,
                dst,
                &signers[0],
                signers.size(),
                messup_sig);

            scEnv
                .tx(xchain_add_attestation_batch(scAttester, batch),
                    ter(temBAD_XCHAIN_PROOF))
                .close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scAlice, jvb, claimID, amt, scBob),
                        ter(tecXCHAIN_CLAIM_NO_QUORUM))
                    .close();
            }

            BEAST_EXPECT(
                !!scEnv.claimID(jvb, claimID));  // claim id still present

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Test combinations of the following when adding a batch of
        // attestations for different claim ids: All the claim id exist One
        // claim id exists and other has already been claimed None of the claim
        // ids exist When the claim ids exist, test for both reaching quorum,
        // going over quorum, and not reaching qurorum (see following tests)
        // ---------------------------------------------------------------------

        // Add a batch of attestations for different claim ids. All the claim id
        // exist and reach quorum
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close()
                .tx(xchain_create_claim_id(scBob, jvb, reward, mcBob))
                .close()
                .tx(xchain_create_claim_id(scCarol, jvb, reward, mcCarol))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id present
            BEAST_EXPECT(!!scEnv.claimID(jvb, 2));  // claim id present
            BEAST_EXPECT(!!scEnv.claimID(jvb, 3));  // claim id present

            // the xchain_commit is not really necessary for the test, as the
            // test is really on the sidechain side
            auto const amt = XRP(1000);
            auto dstAlice(
                withClaim ? std::nullopt : std::optional<Account>{scAlice});
            auto dstBob(
                withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto dstCarol(
                withClaim ? std::nullopt : std::optional<Account>{scCarol});

            mcEnv.tx(xchain_commit(mcAlice, jvb, 1, amt, dstAlice))
                .tx(xchain_commit(mcBob, jvb, 2, amt, dstBob))
                .tx(xchain_commit(mcCarol, jvb, 3, amt, dstCarol))
                .close();

            Balance attester(scEnv, scAttester);
            Balance alice(scEnv, scAlice);
            Balance bob(scEnv, scBob);
            Balance carol(scEnv, scCarol);

            std::vector<AttestationBatch::AttestationClaim> claims;
            att_claim_add_n(claims, mcBob, 2, amt, dstBob, 0, 3);
            att_claim_add_n(claims, mcCarol, 3, amt, dstCarol, 1, 2);
            att_claim_add_n(claims, mcAlice, 1, amt, dstAlice, 2, 3);

            scEnv.tx(att_claim_json(jvb, claims, {})).close();

            STAmount added_amt = amt;
            added_amt -= reward;

            if (withClaim)
            {
                scEnv.tx(xchain_claim(scAlice, jvb, 1, amt, scAlice))
                    .tx(xchain_claim(scCarol, jvb, 3, amt, scCarol))
                    .tx(xchain_claim(scBob, jvb, 2, amt, scBob))
                    .close();
                added_amt -= tx_fee;
            }

            BEAST_EXPECT(!scEnv.claimID(jvb, 1));   // claim id deleted
            BEAST_EXPECT(!scEnv.claimID(jvb, 2));   // claim id deleted
            BEAST_EXPECT(!scEnv.claimID(jvb, 3));   // claim id deleted
            BEAST_EXPECT(scEnv.claimID(jvb) == 3);  // current idx is 3

            BEAST_EXPECT(attester.diff() == -tx_fee);
            // >= because of reward drops left when dividing by 3 attestations
            BEAST_EXPECT(alice.diff() >= added_amt);
            BEAST_EXPECT(bob.diff() >= added_amt);
            BEAST_EXPECT(carol.diff() == added_amt);
        }

        // Add a batch of attestations for different claim ids. One claim id
        // exists and the other has already been claimed
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close()
                .tx(xchain_create_claim_id(scBob, jvb, reward, mcBob))
                .close()
                .tx(xchain_create_claim_id(scCarol, jvb, reward, mcCarol))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id present
            BEAST_EXPECT(!!scEnv.claimID(jvb, 2));  // claim id present
            BEAST_EXPECT(!!scEnv.claimID(jvb, 3));  // claim id present

            // the xchain_commit is not really necessary for the test, as the
            // test is really on the sidechain side
            auto const amt = XRP(1000);
            auto dstAlice(
                withClaim ? std::nullopt : std::optional<Account>{scAlice});
            auto dstBob(
                withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto dstCarol(
                withClaim ? std::nullopt : std::optional<Account>{scCarol});

            mcEnv.tx(xchain_commit(mcAlice, jvb, 1, amt, dstAlice))
                .tx(xchain_commit(mcBob, jvb, 2, amt, dstBob))
                .tx(xchain_commit(mcCarol, jvb, 3, amt, dstCarol))
                .close();

            Balance attester(scEnv, scAttester);
            Balance alice(scEnv, scAlice);
            Balance bob(scEnv, scBob);
            Balance carol(scEnv, scCarol);

            std::vector<AttestationBatch::AttestationClaim> claims;
            STAmount attester_expense = STAmount(0);
            STAmount carol_expense = STAmount(0);

            // claim Carol first
            att_claim_add_n(claims, mcCarol, 3, amt, dstCarol, 1, 2);
            scEnv.tx(att_claim_json(jvb, claims, {})).close();
            attester_expense += tx_fee;

            if (withClaim)
            {
                scEnv.tx(xchain_claim(scCarol, jvb, 3, amt, scCarol)).close();
                carol_expense += tx_fee;
            }

            claims.clear();

            att_claim_add_n(claims, mcBob, 2, amt, dstBob, 0, 3);
            att_claim_add_n(claims, mcCarol, 3, amt, dstCarol, 1, 2);
            att_claim_add_n(claims, mcAlice, 1, amt, dstAlice, 2, 3);

            scEnv.tx(att_claim_json(jvb, claims, {})).close();
            attester_expense += tx_fee;

            STAmount added_amt = amt;
            added_amt -= reward;

            if (withClaim)
            {
                scEnv.tx(xchain_claim(scAlice, jvb, 1, amt, scAlice))
                    .tx(xchain_claim(scCarol, jvb, 3, amt, scCarol),
                        ter(tecXCHAIN_NO_CLAIM_ID))
                    .tx(xchain_claim(scBob, jvb, 2, amt, scBob))
                    .close();
                added_amt -= tx_fee;
            }

            BEAST_EXPECT(!scEnv.claimID(jvb, 1));   // claim id deleted
            BEAST_EXPECT(!scEnv.claimID(jvb, 2));   // claim id deleted
            BEAST_EXPECT(!scEnv.claimID(jvb, 3));   // claim id deleted
            BEAST_EXPECT(scEnv.claimID(jvb) == 3);  // current idx is 3

            BEAST_EXPECT(attester.diff() == -attester_expense);
            // >= because of reward drops left when dividing by 3 attestations
            BEAST_EXPECT(alice.diff() >= added_amt);
            BEAST_EXPECT(bob.diff() >= added_amt);
            BEAST_EXPECT(carol.diff() == added_amt - carol_expense);
        }

        // Add a batch of attestations for different claim ids. None of the
        // claim ids exist. No transfer should occur.
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close();

            // the xchain_commit is not really necessary for the test, as the
            // test is really on the sidechain side
            auto const amt = XRP(1000);
            auto dstAlice(
                withClaim ? std::nullopt : std::optional<Account>{scAlice});
            auto dstBob(
                withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto dstCarol(
                withClaim ? std::nullopt : std::optional<Account>{scCarol});

            mcEnv.tx(xchain_commit(mcAlice, jvb, 1, amt, dstAlice))
                .tx(xchain_commit(mcBob, jvb, 2, amt, dstBob))
                .tx(xchain_commit(mcCarol, jvb, 3, amt, dstCarol))
                .close();

            Balance attester(scEnv, scAttester);
            Balance alice(scEnv, scAlice);
            Balance bob(scEnv, scBob);
            Balance carol(scEnv, scCarol);

            std::vector<AttestationBatch::AttestationClaim> claims;
            att_claim_add_n(claims, mcBob, 2, amt, dstBob, 0, 3);
            att_claim_add_n(claims, mcCarol, 3, amt, dstCarol, 1, 2);
            att_claim_add_n(claims, mcAlice, 1, amt, dstAlice, 2, 3);

            scEnv
                .tx(att_claim_json(jvb, claims, {}), ter(tecXCHAIN_NO_CLAIM_ID))
                .close();

            STAmount added_amt = drops(0);

            if (withClaim)
            {
                scEnv
                    .tx(xchain_claim(scAlice, jvb, 1, amt, scAlice),
                        ter(tecXCHAIN_NO_CLAIM_ID))
                    .tx(xchain_claim(scCarol, jvb, 3, amt, scCarol),
                        ter(tecXCHAIN_NO_CLAIM_ID))
                    .tx(xchain_claim(scBob, jvb, 2, amt, scBob),
                        ter(tecXCHAIN_NO_CLAIM_ID))
                    .close();
                added_amt -= tx_fee;
            }

            BEAST_EXPECT(scEnv.claimID(jvb) == 0);  // current idx is 0

            BEAST_EXPECT(attester.diff() == -tx_fee);
            BEAST_EXPECT(alice.diff() == added_amt);
            BEAST_EXPECT(bob.diff() == added_amt);
            BEAST_EXPECT(carol.diff() == added_amt);
        }

        // Add a batch of attestations for different claim ids. All the claim id
        // exist. Test for both reaching quorum
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close()
                .tx(xchain_create_claim_id(scBob, jvb, reward, mcBob))
                .close()
                .tx(xchain_create_claim_id(scCarol, jvb, reward, mcCarol))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id present
            BEAST_EXPECT(!!scEnv.claimID(jvb, 2));  // claim id present
            BEAST_EXPECT(!!scEnv.claimID(jvb, 3));  // claim id present

            // the xchain_commit is not really necessary for the test, as the
            // test is really on the sidechain side
            auto const amt = XRP(1000);
            auto dstAlice(
                withClaim ? std::nullopt : std::optional<Account>{scAlice});
            auto dstBob(
                withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto dstCarol(
                withClaim ? std::nullopt : std::optional<Account>{scCarol});

            mcEnv.tx(xchain_commit(mcAlice, jvb, 1, amt, dstAlice))
                .tx(xchain_commit(mcBob, jvb, 2, amt, dstBob))
                .tx(xchain_commit(mcCarol, jvb, 3, amt, dstCarol))
                .close();

            Balance attester(scEnv, scAttester);
            Balance alice(scEnv, scAlice);
            Balance bob(scEnv, scBob);
            Balance carol(scEnv, scCarol);

            std::vector<AttestationBatch::AttestationClaim> claims;
            STAmount attester_expense = STAmount(0);

            att_claim_add_n(claims, mcBob, 2, amt, dstBob, 0, 2);
            att_claim_add_n(claims, mcCarol, 3, amt, dstCarol, 1, 2);
            att_claim_add_n(claims, mcAlice, 1, amt, dstAlice, 1, 4);

            scEnv.tx(att_claim_json(jvb, claims, {})).close();
            attester_expense += tx_fee;

            STAmount added_amt = amt;
            added_amt -= reward;

            if (withClaim)
            {
                scEnv.tx(xchain_claim(scAlice, jvb, 1, amt, scAlice))
                    .tx(xchain_claim(scCarol, jvb, 3, amt, scCarol))
                    .tx(xchain_claim(scBob, jvb, 2, amt, scBob))
                    .close();
                added_amt -= tx_fee;
            }

            BEAST_EXPECT(!scEnv.claimID(jvb, 1));   // claim id deleted
            BEAST_EXPECT(!scEnv.claimID(jvb, 2));   // claim id deleted
            BEAST_EXPECT(!scEnv.claimID(jvb, 3));   // claim id deleted
            BEAST_EXPECT(scEnv.claimID(jvb) == 3);  // current idx is 3

            BEAST_EXPECT(attester.diff() == -attester_expense);
            BEAST_EXPECT(alice.diff() == added_amt);
            BEAST_EXPECT(bob.diff() == added_amt);
            BEAST_EXPECT(carol.diff() == added_amt);
        }

        // Add a batch of attestations for different claim ids. All the claim id
        // exist. Test for both going over quorum.
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close()
                .tx(xchain_create_claim_id(scBob, jvb, reward, mcBob))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id present
            BEAST_EXPECT(!!scEnv.claimID(jvb, 2));  // claim id present

            // the xchain_commit is not really necessary for the test, as the
            // test is really on the sidechain side
            auto const amt = XRP(1000);
            auto dstAlice(
                withClaim ? std::nullopt : std::optional<Account>{scAlice});
            auto dstBob(
                withClaim ? std::nullopt : std::optional<Account>{scBob});

            mcEnv.tx(xchain_commit(mcAlice, jvb, 1, amt, dstAlice))
                .tx(xchain_commit(mcBob, jvb, 2, amt, dstBob))
                .close();

            Balance attester(scEnv, scAttester);
            Balance alice(scEnv, scAlice);
            Balance bob(scEnv, scBob);

            std::vector<AttestationBatch::AttestationClaim> claims;
            STAmount attester_expense = STAmount(0);

            att_claim_add_n(claims, mcBob, 2, amt, dstBob, 0, 4);
            att_claim_add_n(claims, mcAlice, 1, amt, dstAlice, 1, 4);

            scEnv.tx(att_claim_json(jvb, claims, {})).close();
            attester_expense += tx_fee;

            STAmount added_amt = amt;
            added_amt -= reward;

            if (withClaim)
            {
                scEnv.tx(xchain_claim(scAlice, jvb, 1, amt, scAlice))
                    .tx(xchain_claim(scBob, jvb, 2, amt, scBob))
                    .close();
                added_amt -= tx_fee;
            }

            BEAST_EXPECT(!scEnv.claimID(jvb, 1));   // claim id deleted
            BEAST_EXPECT(!scEnv.claimID(jvb, 2));   // claim id deleted
            BEAST_EXPECT(scEnv.claimID(jvb) == 2);  // current idx is 2

            BEAST_EXPECT(attester.diff() == -attester_expense);
            BEAST_EXPECT(alice.diff() == added_amt);
            BEAST_EXPECT(bob.diff() == added_amt);
        }

        // Add a batch of attestations for different claim ids. All the claim id
        // exist. Test for one reaching quorum and not the other
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close()
                .tx(xchain_create_claim_id(scBob, jvb, reward, mcBob))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id present
            BEAST_EXPECT(!!scEnv.claimID(jvb, 2));  // claim id present

            // the xchain_commit is not really necessary for the test, as the
            // test is really on the sidechain side
            auto const amt = XRP(1000);
            auto dstAlice(
                withClaim ? std::nullopt : std::optional<Account>{scAlice});
            auto dstBob(
                withClaim ? std::nullopt : std::optional<Account>{scBob});

            mcEnv.tx(xchain_commit(mcAlice, jvb, 1, amt, dstAlice))
                .tx(xchain_commit(mcBob, jvb, 2, amt, dstBob))
                .close();

            Balance attester(scEnv, scAttester);
            Balance alice(scEnv, scAlice);
            Balance bob(scEnv, scBob);

            std::vector<AttestationBatch::AttestationClaim> claims;
            STAmount attester_expense = STAmount(0);

            att_claim_add_n(claims, mcBob, 2, amt, dstBob, 0, 4);
            att_claim_add_n(claims, mcAlice, 1, amt, dstAlice, 1, 1);

            scEnv.tx(att_claim_json(jvb, claims, {})).close();
            attester_expense += tx_fee;

            STAmount added_amt = amt;
            added_amt -= reward;

            if (withClaim)
            {
                scEnv
                    .tx(xchain_claim(scAlice, jvb, 1, amt, scAlice),
                        ter(tecXCHAIN_CLAIM_NO_QUORUM))
                    .tx(xchain_claim(scBob, jvb, 2, amt, scBob))
                    .close();
                added_amt -= tx_fee;
            }

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // Alice's claim remains
            BEAST_EXPECT(!scEnv.claimID(jvb, 2));   // Bob's claim id deleted
            BEAST_EXPECT(scEnv.claimID(jvb) == 2);  // current idx is 2

            BEAST_EXPECT(attester.diff() == -attester_expense);
            BEAST_EXPECT(alice.diff() == (withClaim ? -tx_fee : STAmount(0)));
            BEAST_EXPECT(bob.diff() == added_amt);
        }

        // Add attestations where some of the attestations are inconsistent with
        // each other. The entire transaction should fail. Being inconsistent
        // means attesting to different values.
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close()
                .tx(xchain_create_claim_id(scBob, jvb, reward, mcBob))
                .close()
                .tx(xchain_create_claim_id(scCarol, jvb, reward, mcCarol))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id present
            BEAST_EXPECT(!!scEnv.claimID(jvb, 2));  // claim id present
            BEAST_EXPECT(!!scEnv.claimID(jvb, 3));  // claim id present

            // the xchain_commit is not really necessary for the test, as the
            // test is really on the sidechain side
            auto const amt = XRP(1000);
            auto dstAlice(
                withClaim ? std::nullopt : std::optional<Account>{scAlice});
            auto dstBob(
                withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto dstCarol(
                withClaim ? std::nullopt : std::optional<Account>{scCarol});

            mcEnv.tx(xchain_commit(mcAlice, jvb, 1, amt, dstAlice))
                .tx(xchain_commit(mcBob, jvb, 2, amt, dstBob))
                .tx(xchain_commit(mcCarol, jvb, 3, amt, dstCarol))
                .close();

            Balance attester(scEnv, scAttester);
            Balance alice(scEnv, scAlice);
            Balance bob(scEnv, scBob);
            Balance carol(scEnv, scCarol);

            auto messup_amount =
                [&](size_t i, Buffer sig, jtx::signer s, AnyAmount amount) {
                    if (i == 0)
                    {
                        jtx::AnyAmount const new_amt = XRP(1001);
                        auto sig = sign_claim_attestation(
                            s.account.pk(),
                            s.account.sk(),
                            STXChainBridge(jvb),
                            mcAlice,
                            new_amt.value,
                            payees[i],
                            true,
                            3,
                            dstCarol);
                        return std::make_tuple(std::move(sig), new_amt);
                    }
                    else
                        return std::make_tuple(std::move(sig), amount);
                };

            std::vector<AttestationBatch::AttestationClaim> claims;
            att_claim_add_n(claims, mcBob, 2, amt, dstBob, 0, 3);
            att_claim_add_n(
                claims, mcCarol, 3, amt, dstCarol, 1, 2, messup_amount);
            att_claim_add_n(claims, mcAlice, 1, amt, dstAlice, 2, 3);

            scEnv.tx(att_claim_json(jvb, claims, {}), ter(temBAD_XCHAIN_PROOF))
                .close();

            STAmount added_amt = STAmount(0);

            if (withClaim)
            {
                scEnv
                    .tx(xchain_claim(scAlice, jvb, 1, amt, scAlice),
                        ter(tecXCHAIN_CLAIM_NO_QUORUM))
                    .tx(xchain_claim(scCarol, jvb, 3, amt, scCarol),
                        ter(tecXCHAIN_CLAIM_NO_QUORUM))
                    .tx(xchain_claim(scBob, jvb, 2, amt, scBob),
                        ter(tecXCHAIN_CLAIM_NO_QUORUM))
                    .close();
                added_amt -= tx_fee;
            }

            // because one attestation was inconsistent, no transfer should have
            // happened and the claim ids should remain.

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id remains
            BEAST_EXPECT(!!scEnv.claimID(jvb, 2));  // claim id remains
            BEAST_EXPECT(!!scEnv.claimID(jvb, 3));  // claim id remains
            BEAST_EXPECT(scEnv.claimID(jvb) == 3);  // current idx is 3

            BEAST_EXPECT(alice.diff() == added_amt);
            BEAST_EXPECT(bob.diff() == added_amt);
            BEAST_EXPECT(carol.diff() == added_amt);
        }

        // Test that signature weights are correctly handled. Assign signature
        // weights of 1,2,4,4 and a quorum of 7. Check that the 4,4 signatures
        // reach a quorum, the 1,2,4, reach a quorum, but the 4,2, 4,1 and 1,2
        // do not.

        // 1,2,4 => should succeed
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            std::uint32_t const quorum_7 = 7;
            std::vector<signer> const signers_ = [] {
                constexpr int numSigners = 4;
                std::uint32_t weights[] = {1, 2, 4, 4};

                std::vector<signer> result;
                result.reserve(numSigners);
                for (int i = 0; i < numSigners; ++i)
                {
                    using namespace std::literals;
                    auto const a = Account("signer_"s + std::to_string(i));
                    result.emplace_back(a, weights[i]);
                }
                return result;
            }();

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum_7, signers_))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id present

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv,
                Account::master,
                scBob,
                scAlice,
                &payees[0],
                3,
                withClaim);

            auto batch = attestation_claim_batch(
                jvb,
                mcAlice,
                amt,
                &payees[0],
                true,
                claimID,
                dst,
                &signers_[0],
                3);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scBob))
                    .close();
            }

            BEAST_EXPECT(!scEnv.claimID(jvb, 1));  // claim id deleted

            BEAST_EXPECT(transfer.has_happened(
                amt, divide(reward, STAmount(3), reward.issue())));
        }

        // 4,4 => should succeed
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            std::uint32_t const quorum_7 = 7;
            std::vector<signer> const signers_ = [] {
                constexpr int numSigners = 4;
                std::uint32_t weights[] = {1, 2, 4, 4};

                std::vector<signer> result;
                result.reserve(numSigners);
                for (int i = 0; i < numSigners; ++i)
                {
                    using namespace std::literals;
                    auto const a = Account("signer_"s + std::to_string(i));
                    result.emplace_back(a, weights[i]);
                }
                return result;
            }();
            STAmount const split_reward_ =
                divide(reward, STAmount(signers_.size()), reward.issue());

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum_7, signers_))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id present

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv,
                Account::master,
                scBob,
                scAlice,
                &payees[2],
                2,
                withClaim);

            auto batch = attestation_claim_batch(
                jvb,
                mcAlice,
                amt,
                &payees[2],
                true,
                claimID,
                dst,
                &signers_[2],
                2);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scBob))
                    .close();
            }

            BEAST_EXPECT(!scEnv.claimID(jvb, 1));  // claim id deleted

            BEAST_EXPECT(transfer.has_happened(
                amt, divide(reward, STAmount(2), reward.issue())));
        }

        // 1,2 => should fail
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            std::uint32_t const quorum_7 = 7;
            std::vector<signer> const signers_ = [] {
                constexpr int numSigners = 4;
                std::uint32_t weights[] = {1, 2, 4, 4};

                std::vector<signer> result;
                result.reserve(numSigners);
                for (int i = 0; i < numSigners; ++i)
                {
                    using namespace std::literals;
                    auto const a = Account("signer_"s + std::to_string(i));
                    result.emplace_back(a, weights[i]);
                }
                return result;
            }();

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum_7, signers_))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id present

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv,
                Account::master,
                scBob,
                scAlice,
                &payees[0],
                2,
                withClaim);

            auto batch = attestation_claim_batch(
                jvb,
                mcAlice,
                amt,
                &payees[0],
                true,
                claimID,
                dst,
                &signers_[0],
                2);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scAlice, jvb, claimID, amt, scBob),
                        ter(tecXCHAIN_CLAIM_NO_QUORUM))
                    .close();
            }

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id still present
            BEAST_EXPECT(transfer.has_not_happened());
        }

        // 2,4 => should fail
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            std::uint32_t const quorum_7 = 7;
            std::vector<signer> const signers_ = [] {
                constexpr int numSigners = 4;
                std::uint32_t weights[] = {1, 2, 4, 4};

                std::vector<signer> result;
                result.reserve(numSigners);
                for (int i = 0; i < numSigners; ++i)
                {
                    using namespace std::literals;
                    auto const a = Account("signer_"s + std::to_string(i));
                    result.emplace_back(a, weights[i]);
                }
                return result;
            }();

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum_7, signers_))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id present

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv,
                Account::master,
                scBob,
                scAlice,
                &payees[1],
                2,
                withClaim);

            auto batch = attestation_claim_batch(
                jvb,
                mcAlice,
                amt,
                &payees[1],
                true,
                claimID,
                dst,
                &signers_[1],
                2);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scAlice, jvb, claimID, amt, scBob),
                        ter(tecXCHAIN_CLAIM_NO_QUORUM))
                    .close();
            }

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id still present
            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Add more than the maximum number of allowed attestations (8). This
        // should fail.
        for (auto withClaim : {false})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id present

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst))
                .tx(xchain_commit(mcAlice, jvb, claimID + 1, amt, dst))
                .close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            std::vector<AttestationBatch::AttestationClaim> claims;
            claims.reserve(signers.size() * 2);
            attestation_add_batch_to_vector(
                claims,
                jvb,
                mcAlice,
                amt,
                &payees[0],
                true,
                claimID,
                dst,
                &signers[0],
                signers.size());
            attestation_add_batch_to_vector(
                claims,
                jvb,
                mcAlice,
                amt,
                &payees[0],
                true,
                claimID + 1,
                dst,
                &signers[0],
                signers.size());

            STXChainBridge const stBridge(jvb);
            // batch of 10 claims... should fail
            STXChainAttestationBatch attn_batch{
                stBridge, claims.begin(), claims.end()};

            auto batch = attn_batch.getJson(JsonOptions::none);
            scEnv
                .tx(xchain_add_attestation_batch(scAttester, batch),
                    ter(temXCHAIN_TOO_MANY_ATTESTATIONS))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id still present
            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Add attestations for both account create and claims.
        for (auto withClaim : {false})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);
            auto const amt = XRP(1000);

            {
                Balance door(mcEnv, mcDoor);
                Balance carol(mcEnv, mcCarol);

                mcEnv.tx(create_bridge(mcDoor, jvb))
                    .close()
                    .tx(sidechain_xchain_account_create(
                        mcCarol, jvb, scuAlice, amt, reward))
                    .close();

                BEAST_EXPECT(door.diff() == (amt + reward - tx_fee));
                BEAST_EXPECT(carol.diff() == -(amt + reward + tx_fee));
            }

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));  // claim id present

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            scEnv.tx(att_claim_batch1(mcAlice, claimID, amt, dst))
                .tx(att_create_acct_batch2(1, amt, scuAlice))
                .close();

            BEAST_EXPECT(transfer.has_not_happened());
            BEAST_EXPECT(!!scEnv.claimID(jvb, 1));     // claim id present
            BEAST_EXPECT(!!scEnv.caClaimID(jvb, 1));   // ca claim id present
            BEAST_EXPECT(scEnv.claimCount(jvb) == 0);  // claim count still 0

            // now complete attestations for both account create and claim
            Balance attester(scEnv, scAttester);

            scEnv.tx(att_claim_batch2(mcAlice, claimID, amt, dst))
                .tx(att_create_acct_batch1(1, amt, scuAlice))
                .close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scBob))
                    .close();
            }

            // OK, both the CreateAccount and transfer should have happened now.
            BEAST_EXPECT(!scEnv.claimID(jvb, 1));    // claim id deleted
            BEAST_EXPECT(!scEnv.caClaimID(jvb, 1));  // ca claim id deleted
            BEAST_EXPECT(
                scEnv.claimCount(jvb) == 1);  // claim count was incremented

            // all payees (signers) received 2 split_reward, as they attested
            // for both the account_create and the transfer
            BEAST_EXPECT(transfer.payees_received(
                multiply(split_reward, STAmount(2), split_reward.issue())));

            // Account::master paid amt twice, plus the signer fees for the
            // account create
            BEAST_EXPECT(transfer.from_.diff() == -(reward + XRP(2000)));

            // the attester just paid for the two transactions
            BEAST_EXPECT(
                attester.diff() == -multiply(tx_fee, STAmount(2), xrpIssue()));
        }

        // Confirm that account create transactions happen in the correct order.
        // If they reach quorum out of order they should not execute until until
        // all the previous create transactions have occurred. Re-adding an
        // attestation should move funds.
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);
            auto const amt = XRP(1000);
            auto const amt_plus_reward = amt + reward;

            {
                Balance door(mcEnv, mcDoor);
                Balance carol(mcEnv, mcCarol);

                mcEnv.tx(create_bridge(mcDoor, jvb))
                    .close()
                    .tx(sidechain_xchain_account_create(
                        mcAlice, jvb, scuAlice, amt, reward))
                    .tx(sidechain_xchain_account_create(
                        mcBob, jvb, scuBob, amt, reward))
                    .tx(sidechain_xchain_account_create(
                        mcCarol, jvb, scuCarol, amt, reward))
                    .close();

                BEAST_EXPECT(
                    door.diff() ==
                    (multiply(amt_plus_reward, STAmount(3), xrpIssue()) -
                     tx_fee));
                BEAST_EXPECT(carol.diff() == -(amt + reward + tx_fee));
            }

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close();

            {
                // send first batch of account create attest for all 3 account
                // create
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                scEnv.tx(att_create_acct_batch1(1, amt, scuAlice))
                    .tx(att_create_acct_batch1(3, amt, scuCarol))
                    .tx(att_create_acct_batch1(2, amt, scuBob))
                    .close();

                BEAST_EXPECT(door.diff() == STAmount(0));
                BEAST_EXPECT(
                    attester.diff() ==
                    -multiply(tx_fee, STAmount(3), xrpIssue()));

                BEAST_EXPECT(!!scEnv.caClaimID(jvb, 1));  // ca claim id present
                BEAST_EXPECT(!!scEnv.caClaimID(jvb, 2));  // ca claim id present
                BEAST_EXPECT(!!scEnv.caClaimID(jvb, 3));  // ca claim id present
                BEAST_EXPECT(
                    scEnv.claimCount(jvb) == 0);  // claim count still 0
            }

            {
                // complete attestations for 2nd account create => should not
                // complete
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                scEnv.tx(att_create_acct_batch2(2, amt, scuBob)).close();

                BEAST_EXPECT(door.diff() == STAmount(0));
                BEAST_EXPECT(attester.diff() == -tx_fee);

                BEAST_EXPECT(!!scEnv.caClaimID(jvb, 2));  // ca claim id present
                BEAST_EXPECT(
                    scEnv.claimCount(jvb) == 0);  // claim count still 0
            }

            {
                // complete attestations for 3rd account create => should not
                // complete
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                scEnv.tx(att_create_acct_batch2(3, amt, scuCarol)).close();

                BEAST_EXPECT(door.diff() == STAmount(0));
                BEAST_EXPECT(attester.diff() == -tx_fee);

                BEAST_EXPECT(!!scEnv.caClaimID(jvb, 3));  // ca claim id present
                BEAST_EXPECT(
                    scEnv.claimCount(jvb) == 0);  // claim count still 0
            }

            {
                // complete attestations for 1st account create => account
                // should be created
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                scEnv.tx(att_create_acct_batch2(1, amt, scuAlice)).close();

                BEAST_EXPECT(door.diff() == -amt_plus_reward);
                BEAST_EXPECT(attester.diff() == -tx_fee);
                BEAST_EXPECT(scEnv.balance(scuAlice) == amt);

                BEAST_EXPECT(!scEnv.caClaimID(jvb, 1));    // claim id 1 deleted
                BEAST_EXPECT(!!scEnv.caClaimID(jvb, 2));   // claim id 2 present
                BEAST_EXPECT(!!scEnv.caClaimID(jvb, 3));   // claim id 3 present
                BEAST_EXPECT(scEnv.claimCount(jvb) == 1);  // claim count now 1
            }

            {
                // resend attestations for 3rd account create => still should
                // not complete
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                scEnv.tx(att_create_acct_batch2(3, amt, scuCarol)).close();

                BEAST_EXPECT(door.diff() == STAmount(0));
                BEAST_EXPECT(attester.diff() == -tx_fee);

                BEAST_EXPECT(!!scEnv.caClaimID(jvb, 2));  // claim id 2 present
                BEAST_EXPECT(!!scEnv.caClaimID(jvb, 3));  // claim id 3 present
                BEAST_EXPECT(
                    scEnv.claimCount(jvb) == 1);  // claim count still 1
            }

            {
                // resend attestations for 2nd account create => account
                // should be created
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                scEnv.tx(att_create_acct_batch1(2, amt, scuBob)).close();

                BEAST_EXPECT(door.diff() == -amt_plus_reward);
                BEAST_EXPECT(attester.diff() == -tx_fee);
                BEAST_EXPECT(scEnv.balance(scuBob) == amt);

                BEAST_EXPECT(!scEnv.caClaimID(jvb, 2));    // claim id 2 deleted
                BEAST_EXPECT(!!scEnv.caClaimID(jvb, 3));   // claim id 3 present
                BEAST_EXPECT(scEnv.claimCount(jvb) == 2);  // claim count now 2
            }

            {
                // resend attestations for 3rc account create => account
                // should be created
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                scEnv.tx(att_create_acct_batch2(3, amt, scuCarol)).close();

                BEAST_EXPECT(door.diff() == -amt_plus_reward);
                BEAST_EXPECT(attester.diff() == -tx_fee);
                BEAST_EXPECT(scEnv.balance(scuCarol) == amt);

                BEAST_EXPECT(!scEnv.caClaimID(jvb, 3));    // claim id 3 deleted
                BEAST_EXPECT(scEnv.claimCount(jvb) == 3);  // claim count now 3
            }
        }

        // Check that creating an account with less than the minimum create
        // amount fails.
        {
            XEnv mcEnv(*this);
            auto const amt = XRP(19);

            mcEnv.tx(create_bridge(mcDoor, jvb, XRP(1), XRP(20))).close();

            Balance door(mcEnv, mcDoor);
            Balance carol(mcEnv, mcCarol);

            mcEnv
                .tx(sidechain_xchain_account_create(
                        mcCarol, jvb, scuAlice, XRP(19), reward),
                    ter(tecXCHAIN_INSUFF_CREATE_AMOUNT))
                .close();

            BEAST_EXPECT(door.diff() == STAmount(0));
            BEAST_EXPECT(carol.diff() == -tx_fee);
        }

        // Check that creating an account with less than the minimum reserve
        // fails.
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            auto const amt = res0 - XRP(1);
            auto const amt_plus_reward = amt + reward;

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            {
                Balance door(mcEnv, mcDoor);
                Balance carol(mcEnv, mcCarol);

                mcEnv
                    .tx(sidechain_xchain_account_create(
                        mcCarol, jvb, scuAlice, amt, reward))
                    .close();

                BEAST_EXPECT(door.diff() == amt_plus_reward);
                BEAST_EXPECT(carol.diff() == -(amt_plus_reward + tx_fee));
            }

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close();

            Balance attester(scEnv, scAttester);
            Balance door(scEnv, Account::master);

            scEnv.tx(att_create_acct_batch1(1, amt, scuAlice)).close();
            BEAST_EXPECT(!!scEnv.caClaimID(jvb, 1));  // claim id present
            BEAST_EXPECT(
                scEnv.claimCount(jvb) == 0);  // claim count is one less

            scEnv.tx(att_create_acct_batch2(1, amt, scuAlice)).close();
            BEAST_EXPECT(!scEnv.caClaimID(jvb, 1));  // claim id deleted
            BEAST_EXPECT(
                scEnv.claimCount(jvb) == 1);  // claim count was incremented

            BEAST_EXPECT(attester.diff() == -tx_fee_2);
            BEAST_EXPECT(door.diff() == -reward);
            BEAST_EXPECT(!scEnv.account(scuAlice));
        }

        // Check that sending funds with an account create txn to an existing
        // account works.
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            auto const amt = XRP(111);
            auto const amt_plus_reward = amt + reward;

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            {
                Balance door(mcEnv, mcDoor);
                Balance carol(mcEnv, mcCarol);

                mcEnv
                    .tx(sidechain_xchain_account_create(
                        mcCarol, jvb, scAlice, amt, reward))
                    .close();

                BEAST_EXPECT(door.diff() == amt_plus_reward);
                BEAST_EXPECT(carol.diff() == -(amt_plus_reward + tx_fee));
            }

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close();

            Balance attester(scEnv, scAttester);
            Balance door(scEnv, Account::master);
            Balance alice(scEnv, scAlice);

            scEnv.tx(att_create_acct_batch1(1, amt, scAlice)).close();
            BEAST_EXPECT(!!scEnv.caClaimID(jvb, 1));  // claim id present
            BEAST_EXPECT(
                scEnv.claimCount(jvb) == 0);  // claim count is one less

            scEnv.tx(att_create_acct_batch2(1, amt, scAlice)).close();
            BEAST_EXPECT(!scEnv.caClaimID(jvb, 1));  // claim id deleted
            BEAST_EXPECT(
                scEnv.claimCount(jvb) == 1);  // claim count was incremented

            BEAST_EXPECT(door.diff() == -amt_plus_reward);
            BEAST_EXPECT(attester.diff() == -tx_fee_2);
            BEAST_EXPECT(alice.diff() == amt);
        }

        // Check that sending funds to an existing account with deposit auth set
        // fails for account create transactions.
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            auto const amt = XRP(1000);
            auto const amt_plus_reward = amt + reward;

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            {
                Balance door(mcEnv, mcDoor);
                Balance carol(mcEnv, mcCarol);

                mcEnv
                    .tx(sidechain_xchain_account_create(
                        mcCarol, jvb, scAlice, amt, reward))
                    .close();

                BEAST_EXPECT(door.diff() == amt_plus_reward);
                BEAST_EXPECT(carol.diff() == -(amt_plus_reward + tx_fee));
            }

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .tx(fset("scAlice", asfDepositAuth))  // set deposit auth
                .close();

            Balance attester(scEnv, scAttester);
            Balance door(scEnv, Account::master);
            Balance alice(scEnv, scAlice);

            scEnv.tx(att_create_acct_batch1(1, amt, scAlice)).close();
            BEAST_EXPECT(!!scEnv.caClaimID(jvb, 1));  // claim id present
            BEAST_EXPECT(
                scEnv.claimCount(jvb) == 0);  // claim count is one less

            scEnv.tx(att_create_acct_batch2(1, amt, scAlice)).close();
            BEAST_EXPECT(!scEnv.caClaimID(jvb, 1));  // claim id deleted
            BEAST_EXPECT(
                scEnv.claimCount(jvb) == 1);  // claim count was incremented

            BEAST_EXPECT(door.diff() == -reward);
            BEAST_EXPECT(attester.diff() == -tx_fee_2);
            BEAST_EXPECT(alice.diff() == STAmount(0));
        }

        // If an account is unable to pay the reserve, check that it fails.
        // [greg todo] I don't know what this should test??

        // Create several accounts with a single batch attestation. This should
        // succeed.
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);
            auto const amt = XRP(1000);
            auto const amt_plus_reward = amt + reward;

            {
                Balance door(mcEnv, mcDoor);
                Balance carol(mcEnv, mcCarol);

                mcEnv.tx(create_bridge(mcDoor, jvb))
                    .close()
                    .tx(sidechain_xchain_account_create(
                        mcAlice, jvb, scuAlice, amt, reward))
                    .close()  // make sure Alice gets claim #1
                    .tx(sidechain_xchain_account_create(
                        mcBob, jvb, scuBob, amt, reward))
                    .close()  // make sure Bob gets claim #2
                    .tx(sidechain_xchain_account_create(
                        mcCarol, jvb, scuCarol, amt, reward))
                    .close();  // and Carol will get claim #3

                BEAST_EXPECT(
                    door.diff() ==
                    (multiply(amt_plus_reward, STAmount(3), xrpIssue()) -
                     tx_fee));
                BEAST_EXPECT(carol.diff() == -(amt + reward + tx_fee));
            }

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close();

            {
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                std::vector<AttestationBatch::AttestationCreateAccount> atts;
                atts.reserve(8);
                att_create_acct_add_n(atts, 1, amt, scuAlice, 0, red_quorum);
                att_create_acct_add_n(atts, 2, amt, scuBob, 2, red_quorum);
                att_create_acct_add_n(atts, 3, amt, scuCarol, 1, red_quorum);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(
                    scEnv.claimCount(jvb) == 3, "processed 3 account create");
                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 1), "claim id 1 deleted");
                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 2), "claim id 2 deleted");
                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 3), "claim id 3 deleted");

                BEAST_EXPECT(
                    door.diff() ==
                    -multiply(amt_plus_reward, STAmount(3), xrpIssue()));
                BEAST_EXPECT(attester.diff() == -tx_fee);
                BEAST_EXPECT(scEnv.balance(scuAlice) == amt);
                BEAST_EXPECT(scEnv.balance(scuBob) == amt);
                BEAST_EXPECT(scEnv.balance(scuCarol) == amt);
            }
        }

        // Create several accounts with a single batch attestation, with
        // attestations not in order. This should succeed.
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);
            auto const amt = XRP(1000);
            auto const amt_plus_reward = amt + reward;

            {
                Balance door(mcEnv, mcDoor);
                Balance carol(mcEnv, mcCarol);

                mcEnv.tx(create_bridge(mcDoor, jvb))
                    .close()
                    .tx(sidechain_xchain_account_create(
                        mcAlice, jvb, scuAlice, amt, reward))
                    .close()  // make sure Alice gets claim #1
                    .tx(sidechain_xchain_account_create(
                        mcBob, jvb, scuBob, amt, reward))
                    .close()  // make sure Bob gets claim #2
                    .tx(sidechain_xchain_account_create(
                        mcCarol, jvb, scuCarol, amt, reward))
                    .close();  // and Carol will get claim #3

                BEAST_EXPECT(
                    door.diff() ==
                    (multiply(amt_plus_reward, STAmount(3), xrpIssue()) -
                     tx_fee));
                BEAST_EXPECT(carol.diff() == -(amt + reward + tx_fee));
            }

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close();

            {
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                std::vector<AttestationBatch::AttestationCreateAccount> atts;
                atts.reserve(8);
                att_create_acct_add_n(atts, 2, amt, scuBob, 2, red_quorum);
                att_create_acct_add_n(atts, 1, amt, scuAlice, 0, red_quorum);
                att_create_acct_add_n(atts, 3, amt, scuCarol, 1, red_quorum);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(
                    scEnv.claimCount(jvb) == 3, "processed 3 account create");
                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 1), "claim id 1 deleted");
                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 2), "claim id 2 deleted");
                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 3), "claim id 3 deleted");

                BEAST_EXPECT(
                    door.diff() ==
                    -multiply(amt_plus_reward, STAmount(3), xrpIssue()));
                BEAST_EXPECT(attester.diff() == -tx_fee);
                BEAST_EXPECT(scEnv.balance(scuAlice) == amt);
                BEAST_EXPECT(scEnv.balance(scuBob) == amt);
                BEAST_EXPECT(scEnv.balance(scuCarol) == amt);
            }
        }

        // try even more mixed up attestations
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);
            auto const amt = XRP(1000);
            auto const amt_plus_reward = amt + reward;

            {
                Balance door(mcEnv, mcDoor);
                Balance carol(mcEnv, mcCarol);

                mcEnv.tx(create_bridge(mcDoor, jvb))
                    .close()
                    .tx(sidechain_xchain_account_create(
                        mcAlice, jvb, scuAlice, amt, reward))
                    .close()  // make sure Alice gets claim #1
                    .tx(sidechain_xchain_account_create(
                        mcBob, jvb, scuBob, amt, reward))
                    .close()  // make sure Bob gets claim #2
                    .tx(sidechain_xchain_account_create(
                        mcCarol, jvb, scuCarol, amt, reward))
                    .close();  // and Carol will get claim #3

                BEAST_EXPECT(
                    door.diff() ==
                    (multiply(amt_plus_reward, STAmount(3), xrpIssue()) -
                     tx_fee));
                BEAST_EXPECT(carol.diff() == -(amt + reward + tx_fee));
            }

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close();

            {
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                std::vector<AttestationBatch::AttestationCreateAccount> atts;
                atts.reserve(8);
                att_create_acct_add_n(atts, 2, amt, scuBob, 2, 1);
                att_create_acct_add_n(atts, 3, amt, scuCarol, 1, 1);
                att_create_acct_add_n(atts, 1, amt, scuAlice, 0, 1);
                att_create_acct_add_n(atts, 3, amt, scuCarol, 4, 1);
                att_create_acct_add_n(atts, 3, amt, scuCarol, 0, 1);
                att_create_acct_add_n(atts, 1, amt, scuAlice, 2, 1);
                att_create_acct_add_n(atts, 2, amt, scuBob, 3, 1);
                att_create_acct_add_n(atts, 1, amt, scuAlice, 3, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(
                    scEnv.claimCount(jvb) == 3, "processed 3 account create");
                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 1), "claim id 1 deleted");
                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 2), "claim id 2 deleted");
                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 3), "claim id 3 deleted");

                // because of the division of the rewards among attesters,
                // sometimes a couple drops are left over unspent in the door
                // account (here 2 drops)
                BEAST_EXPECT(
                    multiply(amt_plus_reward, STAmount(3), xrpIssue()) +
                        door.diff() <
                    drops(3));
                BEAST_EXPECT(attester.diff() == -tx_fee);
                BEAST_EXPECT(scEnv.balance(scuAlice) == amt);
                BEAST_EXPECT(scEnv.balance(scuBob) == amt);
                BEAST_EXPECT(scEnv.balance(scuCarol) == amt);
            }
        }

        // try multiple batches of attestations, with the quorum reached for
        // multiple account create in the second and third batch
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);
            auto const amt = XRP(1000);
            auto const amt_plus_reward = amt + reward;

            {
                Balance door(mcEnv, mcDoor);
                Balance carol(mcEnv, mcCarol);

                mcEnv.tx(create_bridge(mcDoor, jvb))
                    .close()
                    .tx(sidechain_xchain_account_create(
                        mcAlice, jvb, scuAlice, amt, reward))
                    .close()  // make sure Alice gets claim #1
                    .tx(sidechain_xchain_account_create(
                        mcBob, jvb, scuBob, amt, reward))
                    .close()  // make sure Bob gets claim #2
                    .tx(sidechain_xchain_account_create(
                        mcCarol, jvb, scuCarol, amt, reward))
                    .close();  // and Carol will get claim #3

                BEAST_EXPECT(
                    door.diff() ==
                    (multiply(amt_plus_reward, STAmount(3), xrpIssue()) -
                     tx_fee));
                BEAST_EXPECT(carol.diff() == -(amt + reward + tx_fee));
            }

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close();

            {
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                std::vector<AttestationBatch::AttestationCreateAccount> atts;
                atts.reserve(8);
                att_create_acct_add_n(atts, 2, amt, scuBob, 2, 1);
                att_create_acct_add_n(atts, 3, amt, scuCarol, 1, 1);
                att_create_acct_add_n(atts, 1, amt, scuAlice, 0, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(!!scEnv.caClaimID(jvb, 1), "claim id 1 created");
                BEAST_EXPECTS(!!scEnv.caClaimID(jvb, 2), "claim id 2 created");
                BEAST_EXPECTS(!!scEnv.caClaimID(jvb, 3), "claim id 3 created");

                atts.clear();
                att_create_acct_add_n(atts, 3, amt, scuCarol, 4, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(
                    !!scEnv.caClaimID(jvb, 3), "present because out of order");

                atts.clear();
                att_create_acct_add_n(atts, 3, amt, scuCarol, 0, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(
                    !!scEnv.caClaimID(jvb, 3), "present because out of order");

                atts.clear();
                att_create_acct_add_n(atts, 1, amt, scuAlice, 2, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 1), "claim id 1 deleted");
                BEAST_EXPECTS(
                    scEnv.claimCount(jvb) == 1,
                    "processed Alice account create");

                atts.clear();
                att_create_acct_add_n(atts, 2, amt, scuBob, 3, 1);
                att_create_acct_add_n(atts, 1, amt, scuAlice, 3, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 2), "claim id 2 deleted");
                BEAST_EXPECTS(
                    scEnv.claimCount(jvb) == 2,
                    "processed Alice & Bob account create");

                atts.clear();
                att_create_acct_add_n(atts, 3, amt, scuCarol, 0, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 3), "claim id 3 deleted");
                BEAST_EXPECTS(
                    scEnv.claimCount(jvb) == 3,
                    "processed all 3 account create");

                // because of the division of the rewards among attesters,
                // sometimes a couple drops are left over unspent in the door
                // account (here 2 drops)
                BEAST_EXPECT(
                    multiply(amt_plus_reward, STAmount(3), xrpIssue()) +
                        door.diff() <
                    drops(3));
                BEAST_EXPECT(
                    attester.diff() ==
                    -multiply(tx_fee, STAmount(6), xrpIssue()));
                BEAST_EXPECT(scEnv.balance(scuAlice) == amt);
                BEAST_EXPECT(scEnv.balance(scuBob) == amt);
                BEAST_EXPECT(scEnv.balance(scuCarol) == amt);
            }
        }

        // If an attestation already exists for that server and claim id, the
        // new attestation should replace the old attestation
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);
            auto const amt = XRP(1000);
            auto const amt_plus_reward = amt + reward;

            {
                Balance door(mcEnv, mcDoor);
                Balance carol(mcEnv, mcCarol);

                mcEnv.tx(create_bridge(mcDoor, jvb))
                    .close()
                    .tx(sidechain_xchain_account_create(
                        mcAlice, jvb, scuAlice, amt, reward))
                    .close()  // make sure Alice gets claim #1
                    .tx(sidechain_xchain_account_create(
                        mcBob, jvb, scuBob, amt, reward))
                    .close()  // make sure Bob gets claim #2
                    .tx(sidechain_xchain_account_create(
                        mcCarol, jvb, scuCarol, amt, reward))
                    .close();  // and Carol will get claim #3

                BEAST_EXPECT(
                    door.diff() ==
                    (multiply(amt_plus_reward, STAmount(3), xrpIssue()) -
                     tx_fee));
                BEAST_EXPECT(carol.diff() == -(amt + reward + tx_fee));
            }

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close();

            {
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);
                auto const bad_amt = XRP(10);

                std::vector<AttestationBatch::AttestationCreateAccount> atts;
                atts.reserve(8);

                // send attestations with incorrect amounts to for all 3
                // AccountCreate. They will be replaced later
                att_create_acct_add_n(atts, 1, bad_amt, scuAlice, 0, 1);
                att_create_acct_add_n(atts, 2, bad_amt, scuBob, 2, 1);
                att_create_acct_add_n(atts, 3, bad_amt, scuCarol, 1, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(!!scEnv.caClaimID(jvb, 1), "claim id 1 created");
                BEAST_EXPECTS(!!scEnv.caClaimID(jvb, 2), "claim id 2 created");
                BEAST_EXPECTS(!!scEnv.caClaimID(jvb, 3), "claim id 3 created");

                // note: if we send inconsistent attestations in the same batch,
                // the transaction errors.

                // from now on we send correct attestations
                atts.clear();
                att_create_acct_add_n(atts, 1, amt, scuAlice, 0, 1);
                att_create_acct_add_n(atts, 2, amt, scuBob, 2, 1);
                att_create_acct_add_n(atts, 3, amt, scuCarol, 4, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(
                    !!scEnv.caClaimID(jvb, 1), "claim id 1 still there");
                BEAST_EXPECTS(
                    !!scEnv.caClaimID(jvb, 2), "claim id 2 still there");
                BEAST_EXPECTS(
                    !!scEnv.caClaimID(jvb, 3), "claim id 3 still there");
                BEAST_EXPECTS(
                    scEnv.claimCount(jvb) == 0, "No account created yet");

                atts.clear();
                att_create_acct_add_n(atts, 3, amt, scuCarol, 1, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(
                    !!scEnv.caClaimID(jvb, 3), "claim id 3 still there");
                BEAST_EXPECTS(
                    scEnv.claimCount(jvb) == 0, "No account created yet");

                atts.clear();
                att_create_acct_add_n(atts, 1, amt, scuAlice, 2, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 1), "claim id 1 deleted");
                BEAST_EXPECTS(scEnv.claimCount(jvb) == 1, "scuAlice created");

                atts.clear();
                att_create_acct_add_n(atts, 2, amt, scuBob, 3, 1);
                att_create_acct_add_n(atts, 1, amt, scuAlice, 3, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 2), "claim id 2 deleted");
                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 1), "claim id 1 not added");
                BEAST_EXPECTS(
                    scEnv.claimCount(jvb) == 2, "scuAlice & scuBob created");

                atts.clear();
                att_create_acct_add_n(atts, 3, amt, scuCarol, 0, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                BEAST_EXPECTS(!scEnv.caClaimID(jvb, 3), "claim id 3 deleted");
                BEAST_EXPECTS(
                    scEnv.claimCount(jvb) == 3, "All 3 accounts created");

                // because of the division of the rewards among attesters,
                // sometimes a couple drops are left over unspent in the door
                // account (here 2 drops)
                BEAST_EXPECT(
                    multiply(amt_plus_reward, STAmount(3), xrpIssue()) +
                        door.diff() <
                    drops(3));
                BEAST_EXPECT(
                    attester.diff() ==
                    -multiply(tx_fee, STAmount(6), xrpIssue()));
                BEAST_EXPECT(scEnv.balance(scuAlice) == amt);
                BEAST_EXPECT(scEnv.balance(scuBob) == amt);
                BEAST_EXPECT(scEnv.balance(scuCarol) == amt);
            }
        }

        // If attestation moves funds, confirm the claim ledger objects are
        // removed (for both account create and "regular" transactions)
        // [greg] we do this in all attestation tests

        // run two create account processes from the same source account,
        // attempting to create two separate accounts on the sidechain.
        // Attestations from each signer are sent in one batch. Should succeed.
        {
            using namespace jtx;

            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            Account a{"a"};
            Account doorA{"doorA"};

            STAmount funds{XRP(10000)};
            mcEnv.fund(funds, a);
            mcEnv.fund(funds, doorA);

            Account ua1{"ua1"};  // unfunded account we want to create
            Account ua2{"ua2"};  // unfunded account we want to create

            BridgeDef xrp_b{
                doorA,
                xrpIssue(),
                Account::master,
                xrpIssue(),
                XRP(1),   // reward
                XRP(20),  // minAccountCreate
                4,        // quorum
                signers,
                Json::nullValue};

            xrp_b.initBridge(mcEnv, scEnv);

            auto const amt = XRP(777);
            auto const amt_plus_reward = amt + xrp_b.reward;
            auto const amt_plus_reward_x2 = amt_plus_reward + amt_plus_reward;

            {
                Balance bal_master(scEnv, Account::master);

                // send 4 attestations from 4 signers... account creation should
                // occur on the last attestation.
                for (size_t i = 0; i < xrp_b.quorum; ++i)
                {
                    auto& s = signers[i];
                    std::vector<AttestationBatch::AttestationCreateAccount>
                        attns;
                    create_account_batch_add_to_vector(
                        attns,
                        xrp_b.jvb,
                        a,
                        amt,
                        xrp_b.reward,
                        &s.account,
                        true,
                        1,
                        ua1,
                        &s,
                        1);

                    create_account_batch_add_to_vector(
                        attns,
                        xrp_b.jvb,
                        a,
                        amt,
                        xrp_b.reward,
                        &s.account,
                        true,
                        2,
                        ua2,
                        &s,
                        1);

                    AttestationBatch::AttestationClaim* nullc = nullptr;
                    STXChainBridge const stBridge(xrp_b.jvb);
                    STXChainAttestationBatch attn_batch{
                        stBridge, nullc, nullc, attns.begin(), attns.end()};
                    auto batch = attn_batch.getJson(JsonOptions::none);
                    auto attn_json =
                        xchain_add_attestation_batch(s.account, batch);
                    scEnv.tx(attn_json).close();

                    if (i < xrp_b.quorum - 1)
                    {
                        BEAST_EXPECT(!!scEnv.caClaimID(
                            xrp_b.jvb, 1));  // claim id present
                        BEAST_EXPECT(!!scEnv.caClaimID(
                            xrp_b.jvb, 2));  // claim id present
                    }
                    else
                    {
                        BEAST_EXPECT(!scEnv.caClaimID(
                            xrp_b.jvb, 1));  // claim id deleted
                        BEAST_EXPECT(!scEnv.caClaimID(
                            xrp_b.jvb, 2));  // claim id deleted
                    }
                }
                BEAST_EXPECT(
                    scEnv.claimCount(xrp_b.jvb) ==
                    2);  // after 2 account create

                BEAST_EXPECT(bal_master.diff() == -amt_plus_reward_x2);
            }
        }

        // coverage test: add_attestation transaction with incorrect flag
        {
            XEnv scEnv(*this, true);
            Json::Value batch = attestation_claim_batch(
                jvb, mcAlice, XRP(1000), payees, true, 1, {}, signers);
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_add_attestation_batch(scAttester, batch),
                    txflags(tfFillOrKill),
                    ter(temINVALID_FLAG))
                .close();
        }

        // coverage test: add_attestation with xchain feature
        // disabled
        {
            XEnv scEnv(*this, true);
            Json::Value batch = attestation_claim_batch(
                jvb, mcAlice, XRP(1000), payees, true, 1, {}, signers);
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .disableFeature(featureXChainBridge)
                .close()
                .tx(xchain_add_attestation_batch(scAttester, batch),
                    ter(temDISABLED))
                .close();
        }
    }

    void
    testXChainClaim()
    {
        using namespace jtx;

        XRPAmount res0 = reserve(0);
        XRPAmount tx_fee = txFee();

        testcase("Claim");

        // Claim where the amount matches what is attested to, to an account
        // that exists, and there are enough attestations to reach a quorum
        // => should succeed
        //
        // This also verifies that if a batch of attestations brings the
        // signatures over quorum (say the quorum is 4 and there are 5
        // attestations) then the reward should be split among the five
        // accounts.
        // -----------------------------------------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scBob))
                    .close();
            }

            BEAST_EXPECT(transfer.has_happened(amt, split_reward));
        }

        // Claim against non-existent bridge
        // ---------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            auto jvb_unknown =
                bridge(mcBob, xrpIssue(), Account::master, xrpIssue());

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(
                        scAlice, jvb_unknown, reward, mcAlice),
                    ter(tecNO_ENTRY))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv
                .tx(xchain_commit(mcAlice, jvb_unknown, claimID, amt, dst),
                    ter(tecNO_ENTRY))
                .close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            auto batch = attestation_claim_batch(
                jvb_unknown, mcAlice, amt, payees, true, claimID, dst, signers);
            scEnv
                .tx(xchain_add_attestation_batch(scAttester, batch),
                    ter(tecNO_ENTRY))
                .close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scAlice, jvb_unknown, claimID, amt, scBob),
                        ter(tecNO_ENTRY))
                    .close();
            }

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Claim against non-existent claim id
        // -----------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            // attest using non-existent claim id
            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, 999, dst, signers);
            scEnv
                .tx(xchain_add_attestation_batch(scAttester, batch),
                    ter(tecXCHAIN_NO_CLAIM_ID))
                .close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // claim using non-existent claim id
                scEnv
                    .tx(xchain_claim(scAlice, jvb, 999, amt, scBob),
                        ter(tecXCHAIN_NO_CLAIM_ID))
                    .close();
            }

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Claim against a claim id owned by another account
        // -------------------------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            // submit attestations to the wrong account (scGw instead of
            // scAlice)
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // submit a claim transaction with the wrong account (scGw
                // instead of scAlice)
                scEnv
                    .tx(xchain_claim(scGw, jvb, claimID, amt, scBob),
                        ter(tecXCHAIN_BAD_CLAIM_ID))
                    .close();
                BEAST_EXPECT(transfer.has_not_happened());
            }
            else
            {
                BEAST_EXPECT(transfer.has_happened(amt, split_reward));
            }
        }

        // Claim against a claim id with no attestations
        // ---------------------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            // don't send any attestations

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scAlice, jvb, claimID, amt, scBob),
                        ter(tecXCHAIN_CLAIM_NO_QUORUM))
                    .close();
            }

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Claim against a claim id with attestations, but not enough to make a
        // quorum
        // --------------------------------------------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            auto tooFew = quorum - 1;
            auto batch = attestation_claim_batch(
                jvb,
                mcAlice,
                amt,
                &payees[0],
                true,
                claimID,
                dst,
                &signers[0],
                tooFew);

            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scAlice, jvb, claimID, amt, scBob),
                        ter(tecXCHAIN_CLAIM_NO_QUORUM))
                    .close();
            }

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Claim id of zero
        // ----------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, 0, dst, signers);

            scEnv
                .tx(xchain_add_attestation_batch(scAttester, batch),
                    ter(tecXCHAIN_NO_CLAIM_ID))
                .close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scAlice, jvb, 0, amt, scBob),
                        ter(tecXCHAIN_NO_CLAIM_ID))
                    .close();
            }

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Claim issue that does not match the expected issue on the bridge
        // (either LockingChainIssue or IssuingChainIssue, depending on the
        // chain). The claim id should already have enough attestations to reach
        // a quorum for this amount (for a different issuer).
        // ---------------------------------------------------------------------
        for (auto withClaim : {true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scAlice, jvb, claimID, scUSD(1000), scBob),
                        ter(temBAD_AMOUNT))
                    .close();
            }

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Claim to a destination that does not already exist on the chain
        // -----------------------------------------------------------------
        for (auto withClaim : {true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scuBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scAlice, jvb, claimID, amt, scuBob),
                        ter(tecNO_DST))
                    .close();
            }

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Claim where the claim id owner does not have enough XRP to pay the
        // reward
        // ------------------------------------------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();
            STAmount huge_reward{XRP(20000)};
            BEAST_EXPECT(huge_reward > scEnv.balance(scAlice));

            scEnv.tx(create_bridge(Account::master, jvb, huge_reward))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, huge_reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            if (withClaim)
            {
                scEnv.tx(xchain_add_attestation_batch(scAttester, batch))
                    .close();

                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scAlice, jvb, claimID, amt, scBob),
                        ter(tecINSUFFICIENT_FUNDS))
                    .close();
            }
            else
            {
                scEnv
                    .tx(xchain_add_attestation_batch(scAttester, batch),
                        ter(tecINSUFFICIENT_FUNDS))
                    .close();
            }

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Claim where the claim id owner has enough XRP to pay the reward, but
        // it would put his balance below the reserve
        // --------------------------------------------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .fund(
                    res0 + reward, scuAlice)  // just not enough because of fees
                .close()
                .tx(xchain_create_claim_id(scuAlice, jvb, reward, mcAlice),
                    ter(tecINSUFFICIENT_RESERVE))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scuAlice, payees, withClaim);

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            scEnv
                .tx(xchain_add_attestation_batch(scAttester, batch),
                    ter(tecXCHAIN_NO_CLAIM_ID))
                .close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scuAlice, jvb, claimID, amt, scBob),
                        ter(tecXCHAIN_NO_CLAIM_ID))
                    .close();
            }

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Pay to an account with deposit auth set
        // ---------------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .tx(fset("scBob", asfDepositAuth))  // set deposit auth
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            if (withClaim)
            {
                scEnv.tx(xchain_add_attestation_batch(scAttester, batch))
                    .close();

                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scAlice, jvb, claimID, amt, scBob),
                        ter(tecNO_PERMISSION))
                    .close();

                // the transfer failed, but check that we can still use the
                // claimID with a different account
                Balance scCarol_bal(scEnv, scCarol);

                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scCarol))
                    .close();
                BEAST_EXPECT(scCarol_bal.diff() == amt);
            }
            else
            {
                scEnv
                    .tx(xchain_add_attestation_batch(scAttester, batch),
                        ter(tecNO_PERMISSION))
                    .close();

                // A way would be to remove deposit auth and resubmit the
                // attestations (even though the witness servers won't do it)
                scEnv
                    .tx(fset("scBob", 0, asfDepositAuth))  // clear deposit auth
                    .close();

                Balance scBob_bal(scEnv, scBob);

                scEnv.tx(xchain_add_attestation_batch(scAttester, batch))
                    .close();
                BEAST_EXPECT(scBob_bal.diff() == amt);
            }
        }

        // Pay to an account with Destination Tag set
        // ------------------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .tx(fset("scBob", asfRequireDest))  // set dest tag
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            if (withClaim)
            {
                scEnv.tx(xchain_add_attestation_batch(scAttester, batch))
                    .close();

                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv
                    .tx(xchain_claim(scAlice, jvb, claimID, amt, scBob),
                        ter(tecDST_TAG_NEEDED))
                    .close();

                // the transfer failed, but check that we can still use the
                // claimID with a different account
                Balance scCarol_bal(scEnv, scCarol);

                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scCarol))
                    .close();
                BEAST_EXPECT(scCarol_bal.diff() == amt);
            }
            else
            {
                scEnv
                    .tx(xchain_add_attestation_batch(scAttester, batch),
                        ter(tecDST_TAG_NEEDED))
                    .close();

                // A way would be to remove the destination tag requirement and
                // resubmit the attestations (even though the witness servers
                // won't do it)
                scEnv
                    .tx(fset("scBob", 0, asfRequireDest))  // clear dest tag
                    .close();

                Balance scBob_bal(scEnv, scBob);

                scEnv.tx(xchain_add_attestation_batch(scAttester, batch))
                    .close();
                BEAST_EXPECT(scBob_bal.diff() == amt);
            }
        }

#if 0
        // enable after Scott changes: "I'll have to change add_attestation to
        // return success. The funds didn't transfer, but attestations were
        // added.".

        // Pay to an account with deposit auth set. Check that the attestations
        // are still validated and that we can used the claimID to transfer the
        // funds to a different account (which doesn't have deposit auth set)
        // --------------------------------------------------------------------
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .tx(fset("scBob", asfDepositAuth))  // set deposit auth
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);

            // we should be able to submit the attestations, but the transfer
            // should not occur because dest account has deposit auth set
            Balance scBob_bal(scEnv, scBob);

            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();
            BEAST_EXPECT(scBob_bal.diff() == STAmount(0));

            // Check that check that we still can use the claimID to transfer
            // the amount to a different account
            Balance scCarol_bal(scEnv, scCarol);

            scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scCarol)).close();
            BEAST_EXPECT(scCarol_bal.diff() == amt);
        }
#endif

        // Claim where the amount different from what is attested to
        // ---------------------------------------------------------
        for (auto withClaim : {true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // claim wrong amount
                scEnv
                    .tx(xchain_claim(scAlice, jvb, claimID, one_xrp, scBob),
                        ter(tecXCHAIN_CLAIM_NO_QUORUM))
                    .close();
            }

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Verify that rewards are paid from the account that owns the claim id
        // --------------------------------------------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);
            Balance scAlice_bal(scEnv, scAlice);

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            STAmount claim_cost = reward;

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scBob))
                    .close();
                claim_cost += tx_fee;
            }

            BEAST_EXPECT(transfer.has_happened(amt, split_reward));
            BEAST_EXPECT(
                scAlice_bal.diff() == -claim_cost);  // because reward % 5 == 0
        }

        // Verify that if a reward is not evenly divisible amung the reward
        // accounts, the remaining amount goes to the claim id owner.
        // ----------------------------------------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb, tiny_reward)).close();

            scEnv.tx(create_bridge(Account::master, jvb, tiny_reward))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, tiny_reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);
            Balance scAlice_bal(scEnv, scAlice);

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();
            STAmount claim_cost = tiny_reward;

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scBob))
                    .close();
                claim_cost += tx_fee;
            }

            BEAST_EXPECT(transfer.has_happened(amt, tiny_reward_split));
            BEAST_EXPECT(
                scAlice_bal.diff() == -(claim_cost - tiny_reward_remainder));
        }

        // If a reward distribution fails for one of the reward accounts (the
        // reward account doesn't exist or has deposit auth set), then the txn
        // should still succeed, but that portion should go to the claim id
        // owner.
        // -------------------------------------------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            std::vector<Account> alt_payees = payees;
            alt_payees.back() = Account("inexistent");

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, alt_payees, true, claimID, dst, signers);

            BalanceTransfer transfer(
                scEnv,
                Account::master,
                scBob,
                scAlice,
                &payees[0],
                payees.size() - 1,
                withClaim);

            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scBob))
                    .close();
            }

            // this also checks that only 4 * split_reward was deducted from
            // scAlice (the payor account), since we passed alt_payees to
            // BalanceTransfer
            BEAST_EXPECT(transfer.has_happened(amt, split_reward));
        }

        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .tx(fset(payees.back(), asfDepositAuth))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            auto batch = attestation_claim_batch(
                jvb, mcAlice, amt, payees, true, claimID, dst, signers);

            // balance of last signer should not change (has deposit auth)
            Balance last_signer(scEnv, payees.back());

            // make sure all signers except the last one get the split_reward
            BalanceTransfer transfer(
                scEnv,
                Account::master,
                scBob,
                scAlice,
                &payees[0],
                payees.size() - 1,
                withClaim);

            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scBob))
                    .close();
            }

            // this also checks that only 4 * split_reward was deducted from
            // scAlice (the payor account), since we passed payees.size() - 1 to
            // BalanceTransfer
            BEAST_EXPECT(transfer.has_happened(amt, split_reward));

            // and make sure the account with deposit auth received nothing
            BEAST_EXPECT(last_signer.diff() == STAmount(0));
        }

        // Verify that if a batch of attestations brings the signatures over
        // quorum (say the quorum is 4 and there are only 4  attestations) then
        // the reward is split among the accounts which provided attestations.
        // ------------------------------------------------------------------
        for (auto withClaim : {false, true})
        {
            XEnv mcEnv(*this);
            XEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            auto const amt = XRP(1000);
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            // balance of last signer should not change (does not submit
            // attestation)
            Balance last_signer(scEnv, payees.back());

            // the quorum signers should get the reward
            BalanceTransfer transfer(
                scEnv,
                Account::master,
                scBob,
                scAlice,
                &payees[0],
                quorum,
                withClaim);

            // submit only the exact quorum of attestations
            auto batch = attestation_claim_batch(
                jvb,
                mcAlice,
                amt,
                &payees[0],
                true,
                claimID,
                dst,
                &signers[0],
                quorum);
            scEnv.tx(xchain_add_attestation_batch(scAttester, batch)).close();

            if (withClaim)
            {
                BEAST_EXPECT(transfer.has_not_happened());

                // need to submit a claim transactions
                scEnv.tx(xchain_claim(scAlice, jvb, claimID, amt, scBob))
                    .close();
            }

            // this also checks that only 1/4th of the total reward was paid to
            // each signer, and that the total reward was deducted from scAlice
            // (the payor account)
            BEAST_EXPECT(transfer.has_happened(
                amt, divide(reward, STAmount(quorum), reward.issue())));

            // and make sure the account that didn't attest received nothing
            BEAST_EXPECT(last_signer.diff() == STAmount(0));
        }

        // coverage test: xchain_claim transaction with incorrect flag
        XEnv(*this, true)
            .tx(create_bridge(Account::master, jvb))
            .close()
            .tx(xchain_claim(scAlice, jvb, 1, XRP(1000), scBob),
                txflags(tfFillOrKill),
                ter(temINVALID_FLAG))
            .close();

        // coverage test: xchain_claim transaction with xchain feature
        // disabled
        XEnv(*this, true)
            .tx(create_bridge(Account::master, jvb))
            .disableFeature(featureXChainBridge)
            .close()
            .tx(xchain_claim(scAlice, jvb, 1, XRP(1000), scBob),
                ter(temDISABLED))
            .close();

        // coverage test: XChainClaim::preclaim - isLockingChain = true;
        XEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .close()
            .tx(xchain_claim(mcAlice, jvb, 1, XRP(1000), mcBob),
                ter(tecXCHAIN_NO_CLAIM_ID));
    }

    void
    testXChainCreateAccount()
    {
        using namespace jtx;

        testcase("Bridge Create Account");

        // coverage test: transferHelper() - dst == src
        {
            XEnv scEnv(*this, true);

            auto const amt = XRP(111);
            auto const amt_plus_reward = amt + reward;

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close();

            Balance door(scEnv, Account::master);

            scEnv.tx(att_create_acct_batch1(1, amt, Account::master)).close();
            BEAST_EXPECT(!!scEnv.caClaimID(jvb, 1));  // claim id present
            BEAST_EXPECT(
                scEnv.claimCount(jvb) == 0);  // claim count is one less

            scEnv.tx(att_create_acct_batch2(1, amt, Account::master)).close();
            BEAST_EXPECT(!scEnv.caClaimID(jvb, 1));  // claim id deleted
            BEAST_EXPECT(
                scEnv.claimCount(jvb) == 1);  // claim count was incremented

            BEAST_EXPECT(door.diff() == -reward);
        }

        // coverage test: getSignersListAndQuorum() returns
        // tecXCHAIN_NO_SIGNERS_LIST because no signer list was set.
        {
            XEnv scEnv(*this, true);

            auto const amt = XRP(111);

            scEnv.tx(create_bridge(Account::master, jvb)).close();

            scEnv
                .tx(att_create_acct_batch1(1, amt, scAlice),
                    ter(tecXCHAIN_NO_SIGNERS_LIST))
                .close();
        }

        // coverage test: xchain_account_create with incorrect flag
        XEnv(*this)
            .tx(create_bridge(Account::master, jvb))
            .tx(sidechain_xchain_account_create(
                    mcAlice, jvb, scuAlice, XRP(100), XRP(20)),
                txflags(tfFillOrKill),
                ter(temINVALID_FLAG));

        // coverage test: xchain_account_create with xchain feature disabled
        XEnv(*this)
            .tx(create_bridge(Account::master, jvb))
            .disableFeature(featureXChainBridge)
            .close()
            .tx(sidechain_xchain_account_create(
                    mcAlice, jvb, scuAlice, XRP(100), XRP(20)),
                ter(temDISABLED));
    }

    void
    testXChainDeleteDoor()
    {
        using namespace jtx;

        testcase("Bridge Delete Door Account");

        auto const acctDelFee{
            drops(XEnv(*this).env_.current()->fees().increment)};

        // Deleting a account that owns bridge should fail
        {
            XEnv mcEnv(*this);

            mcEnv.tx(create_bridge(mcDoor, jvb, XRP(1), XRP(1))).close();

            // We don't allow an account to be deleted if its sequence number
            // is within 256 of the current ledger.
            for (size_t i = 0; i < 256; ++i)
                mcEnv.close();

            // try to delete mcDoor, send funds to mcAlice
            mcEnv.tx(
                acctdelete(mcDoor, mcAlice),
                fee(acctDelFee),
                ter(tecHAS_OBLIGATIONS));
        }

        // Deleting an account that owns a claim id should fail
        {
            XEnv scEnv(*this, true);

            scEnv.tx(create_bridge(Account::master, jvb))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

            // We don't allow an account to be deleted if its sequence number
            // is within 256 of the current ledger.
            for (size_t i = 0; i < 256; ++i)
                scEnv.close();

            // try to delete scAlice, send funds to scBob
            scEnv.tx(
                acctdelete(scAlice, scBob),
                fee(acctDelFee),
                ter(tecHAS_OBLIGATIONS));
        }
    }

    void
    run() override
    {
        testXChainCreateBridge();
        testXChainCreateBridgeMatrix();
        testXChainModifyBridge();
        testXChainCreateClaimID();
        testXChainCommit();
        testXChainAddAttestation();
        testXChainClaim();
        testXChainDeleteDoor();
    }
};

// -----------------------------------------------------------
// -----------------------------------------------------------
struct XChainSim_test : public beast::unit_test::suite,
                        public jtx::XChainBridgeObjects
{
private:
    static constexpr size_t num_signers = 5;

    // --------------------------------------------------
    struct Transfer
    {
        jtx::Account from;
        jtx::Account to;
        jtx::Account finaldest;
        STAmount amt;
        bool a2b;  // direction of transfer
        bool with_claim{false};
        uint32_t claim_id{0};
        std::array<bool, num_signers> attested{};
    };

    struct AccountCreate
    {
        jtx::Account from;
        jtx::Account to;
        STAmount amt;
        STAmount reward;
        bool a2b;
        uint32_t claim_id{0};
        std::array<bool, num_signers> attested{};
    };

    using ENV = XEnv<XChainSim_test>;
    using BridgeID = BridgeDef const*;

    // tracking chain state
    // --------------------
    struct AccountStateTrack
    {
        STAmount startAmount{0};
        STAmount expectedDiff{0};

        void
        init(ENV& env, jtx::Account const& acct)
        {
            startAmount = env.balance(acct);
            expectedDiff = STAmount(0);
        }

        bool
        verify(ENV& env, jtx::Account const& acct) const
        {
            STAmount diff{env.balance(acct) - startAmount};
            bool check = diff == expectedDiff;
            return check;
        }
    };

    // --------------------------------------------------
    struct ChainStateTrack
    {
        using ClaimAttn = AttestationBatch::AttestationClaim;
        using CreateClaimAttn = AttestationBatch::AttestationCreateAccount;

        using ClaimVec = std::vector<ClaimAttn>;
        using CreateClaimVec = std::vector<CreateClaimAttn>;
        using CreateClaimMap = std::map<uint32_t, CreateClaimVec>;

        ChainStateTrack(ENV& env)
            : env(env), tx_fee(env.env_.current()->fees().base)
        {
        }

        void
        sendAttestations(size_t signer_idx, BridgeID bridge, ClaimVec& claims)
        {
            STXChainBridge const stBridge(bridge->jvb);
            while (!claims.empty())
            {
                size_t cnt = (claims.size() > 8) ? 8 : claims.size();
                STXChainAttestationBatch attn_batch{
                    stBridge, claims.begin(), claims.begin() + cnt};
                auto batch = attn_batch.getJson(JsonOptions::none);
                auto signer = bridge->signers[signer_idx].account;
                auto attn_json = xchain_add_attestation_batch(signer, batch);
                // std::cout << to_string(attn_json) << '\n';
                env.tx(attn_json);
                spendFee(signer);
                claims.erase(claims.begin(), claims.begin() + cnt);
            }
        }

        uint32_t
        sendCreateAttestations(
            size_t signer_idx,
            BridgeID bridge,
            CreateClaimVec& claims)
        {
            STXChainBridge const stBridge(bridge->jvb);
            AttestationBatch::AttestationClaim* nullc = nullptr;
            size_t num_successful = 0;
            while (!claims.empty())
            {
                uint32_t cnt = (claims.size() > 8) ? 8 : claims.size();
                STXChainAttestationBatch attn_batch{
                    stBridge,
                    nullc,
                    nullc,
                    claims.begin(),
                    claims.begin() + cnt};
                auto batch = attn_batch.getJson(JsonOptions::none);
                auto signer = bridge->signers[signer_idx].account;
                auto attn_json = xchain_add_attestation_batch(signer, batch);
                // std::cout << to_string(attn_json) << '\n';
                env.tx(attn_json, jtx::ter(std::ignore));
                if (env.ter() == tesSUCCESS)
                {
                    counters[bridge].signers.push_back(signer_idx);
                    num_successful += cnt;
                }
                spendFee(signer);
                claims.erase(claims.begin(), claims.begin() + cnt);
            }
            return num_successful;
        }

        void
        sendAttestations()
        {
            bool callback_called;

            // we have this "do {} while" loop because we want to process all
            // the account create which can reach quorum at this time stamp.
            do
            {
                callback_called = false;
                for (size_t i = 0; i < signers_attns.size(); ++i)
                {
                    for (auto& [bridge, claims] : signers_attns[i])
                    {
                        sendAttestations(i, bridge, claims.xfer_claims);

                        auto& c = counters[bridge];
                        auto& create_claims =
                            claims.create_claims[c.claim_count];
                        auto num_attns = create_claims.size();
                        if (num_attns)
                        {
                            c.num_create_attn_sent += sendCreateAttestations(
                                i, bridge, create_claims);
                        }
                        assert(claims.create_claims[c.claim_count].empty());
                    }
                }
                for (auto& [bridge, c] : counters)
                {
                    if (c.num_create_attn_sent >= bridge->quorum)
                    {
                        callback_called = true;
                        c.create_callbacks[c.claim_count](c.signers);
                        ++c.claim_count;
                        c.num_create_attn_sent = 0;
                        c.signers.clear();
                    }
                }
            } while (callback_called);
        }

        void
        init(jtx::Account const& acct)
        {
            accounts[acct].init(env, acct);
        }

        void
        reinit_accounts()
        {
            for (auto& a : accounts)
                init(a.first);
        }

        void
        receive(
            jtx::Account const& acct,
            STAmount amt,
            std::uint64_t divisor = 1)
        {
            if (amt.issue() != xrpIssue())
                return;
            auto it = accounts.find(acct);
            if (it == accounts.end())
            {
                accounts[acct].init(env, acct);
                // we just looked up the account, so expectedDiff == 0
            }
            else
            {
                it->second.expectedDiff +=
                    (divisor == 1 ? amt
                                  : divide(
                                        amt,
                                        STAmount(amt.issue(), divisor),
                                        amt.issue()));
            }
        }

        void
        spend(jtx::Account const& acct, STAmount amt, std::uint64_t times = 1)
        {
            if (amt.issue() != xrpIssue())
                return;
            receive(
                acct,
                times == 1
                    ? -amt
                    : -multiply(
                          amt, STAmount(amt.issue(), times), amt.issue()));
        }

        void
        transfer(jtx::Account const& from, jtx::Account const& to, STAmount amt)
        {
            spend(from, amt);
            receive(to, amt);
        }

        void
        spendFee(jtx::Account const& acct, size_t times = 1)
        {
            spend(acct, tx_fee, times);
        }

        bool
        verify() const
        {
            for (auto const& [acct, state] : accounts)
                if (!state.verify(env, acct))
                    return false;
            return true;
        }

        struct BridgeCounters
        {
            using complete_cb =
                std::function<void(std::vector<size_t> const& signers)>;

            uint32_t claim_id{0};
            uint32_t create_count{0};  // for account create. First should be 1
            uint32_t claim_count{
                0};  // for account create. Increments after quorum for current
                     // create_count (starts at 1) is reached.

            uint32_t num_create_attn_sent{0};  // for current claim_count
            std::vector<size_t> signers;
            std::vector<complete_cb> create_callbacks;
        };

        struct Claims
        {
            ClaimVec xfer_claims;
            CreateClaimMap create_claims;
        };

        using SignerAttns = std::unordered_map<BridgeID, Claims>;
        using SignersAttns = std::array<SignerAttns, num_signers>;

        ENV& env;
        std::map<jtx::Account, AccountStateTrack> accounts;
        SignersAttns signers_attns;
        std::map<BridgeID, BridgeCounters> counters;
        STAmount tx_fee;
    };

    struct ChainStateTracker
    {
        ChainStateTracker(ENV& a_env, ENV& b_env) : a_(a_env), b_(b_env)
        {
        }

        bool
        verify() const
        {
            return a_.verify() && b_.verify();
        }

        void
        sendAttestations()
        {
            a_.sendAttestations();
            b_.sendAttestations();
        }

        void
        init(jtx::Account const& acct)
        {
            a_.init(acct);
            b_.init(acct);
        }

        void
        reinit_accounts()
        {
            a_.reinit_accounts();
            b_.reinit_accounts();
        }

        ChainStateTrack a_;
        ChainStateTrack b_;
    };

    enum SmState {
        st_initial,
        st_claimid_created,
        st_attesting,
        st_attested,
        st_completed,
        st_closed,
    };

    enum Act_Flags { af_a2b = 1 << 0 };

    // --------------------------------------------------
    template <class T>
    class SmBase
    {
    public:
        SmBase(
            const std::shared_ptr<ChainStateTracker>& chainstate,
            const BridgeDef& bridge)
            : bridge_(bridge), st_(chainstate)
        {
        }

        ChainStateTrack&
        srcState()
        {
            return static_cast<T&>(*this).a2b() ? st_->a_ : st_->b_;
        }

        ChainStateTrack&
        destState()
        {
            return static_cast<T&>(*this).a2b() ? st_->b_ : st_->a_;
        }

        jtx::Account const&
        srcDoor()
        {
            return static_cast<T&>(*this).a2b() ? bridge_.doorA : bridge_.doorB;
        }

        jtx::Account const&
        dstDoor()
        {
            return static_cast<T&>(*this).a2b() ? bridge_.doorB : bridge_.doorA;
        }

    protected:
        const BridgeDef& bridge_;
        std::shared_ptr<ChainStateTracker> st_;
    };

    // --------------------------------------------------
    class SmCreateAccount : public SmBase<SmCreateAccount>
    {
    public:
        using Base = SmBase<SmCreateAccount>;

        SmCreateAccount(
            const std::shared_ptr<ChainStateTracker>& chainstate,
            const BridgeDef& bridge,
            AccountCreate create)
            : Base(chainstate, bridge)
            , sm_state(st_initial)
            , cr(std::move(create))
        {
        }

        bool
        a2b() const
        {
            return cr.a2b;
        }

        uint32_t
        issue_account_create()
        {
            ChainStateTrack& st = srcState();
            jtx::Account const& srcdoor = srcDoor();

            st.env
                .tx(sidechain_xchain_account_create(
                    cr.from, bridge_.jvb, cr.to, cr.amt, cr.reward))
                .close();  // needed for claim_id sequence to be correct'
            st.spendFee(cr.from);
            st.transfer(cr.from, srcdoor, cr.amt);
            st.transfer(cr.from, srcdoor, cr.reward);

            return ++st.counters[&bridge_].create_count;
        }

        void
        attest(uint64_t time, uint32_t rnd)
        {
            ChainStateTrack& st = destState();

            // check all signers, but start at a random one
            size_t i;
            for (i = 0; i < num_signers; ++i)
            {
                size_t signer_idx = (rnd + i) % num_signers;

                if (!(cr.attested[signer_idx]))
                {
                    // enqueue one attestation for this signer
                    cr.attested[signer_idx] = true;
                    create_account_batch_add_to_vector(
                        st.signers_attns[signer_idx][&bridge_]
                            .create_claims[cr.claim_id - 1],
                        bridge_.jvb,
                        cr.from,
                        cr.amt,
                        cr.reward,
                        &bridge_.signers[signer_idx].account,
                        cr.a2b,
                        cr.claim_id,
                        cr.to,
                        &bridge_.signers[signer_idx],
                        1);
                    break;
                }
            }

            if (i == num_signers)
                return;  // did not attest

            auto& counters = st.counters[&bridge_];
            if (counters.create_callbacks.size() < cr.claim_id)
                counters.create_callbacks.resize(cr.claim_id);

            auto complete_cb = [&](std::vector<size_t> const& signers) {
                auto num_attestors = signers.size();
                st.env.close();
                assert(
                    num_attestors <=
                    std::count(cr.attested.begin(), cr.attested.end(), true));
                assert(num_attestors >= bridge_.quorum);
                assert(cr.claim_id - 1 == counters.claim_count);

                auto r = cr.reward;
                auto reward = divide(r, STAmount(num_attestors), r.issue());

                for (auto i : signers)
                    st.receive(bridge_.signers[i].account, reward);

                st.spend(dstDoor(), reward, num_attestors);
                st.transfer(dstDoor(), cr.to, cr.amt);
                st.env.env_.memoize(cr.to);
                sm_state = st_completed;
            };

            counters.create_callbacks[cr.claim_id - 1] = std::move(complete_cb);
        }

        SmState
        advance(uint64_t time, uint32_t rnd)
        {
            switch (sm_state)
            {
                case st_initial:
                    cr.claim_id = issue_account_create();
                    sm_state = st_attesting;
                    break;

                case st_attesting:
                    attest(time, rnd);
                    break;

                default:
                    assert(0);
                    break;

                case st_completed:
                    break;  // will get this once
            }
            return sm_state;
        }

    private:
        SmState sm_state;
        AccountCreate cr;
    };

    // --------------------------------------------------
    class SmTransfer : public SmBase<SmTransfer>
    {
    public:
        using Base = SmBase<SmTransfer>;

        SmTransfer(
            const std::shared_ptr<ChainStateTracker>& chainstate,
            const BridgeDef& bridge,
            Transfer xfer)
            : Base(chainstate, bridge)
            , xfer(std::move(xfer))
            , sm_state(st_initial)
        {
        }

        bool
        a2b() const
        {
            return xfer.a2b;
        }

        uint32_t
        create_claim_id()
        {
            ChainStateTrack& st = destState();

            st.env
                .tx(xchain_create_claim_id(
                    xfer.to, bridge_.jvb, bridge_.reward, xfer.from))
                .close();  // needed for claim_id sequence to be
                           // correct'
            st.spendFee(xfer.to);
            return ++st.counters[&bridge_].claim_id;
        }

        void
        commit()
        {
            ChainStateTrack& st = srcState();
            jtx::Account const& srcdoor = srcDoor();

            if (xfer.amt.issue() != xrpIssue())
            {
                st.env.tx(pay(srcdoor, xfer.from, xfer.amt));
                st.spendFee(srcdoor);
            }
            st.env.tx(xchain_commit(
                xfer.from,
                bridge_.jvb,
                xfer.claim_id,
                xfer.amt,
                xfer.with_claim ? std::nullopt
                                : std::optional<jtx::Account>(xfer.finaldest)));
            st.spendFee(xfer.from);
            st.transfer(xfer.from, srcdoor, xfer.amt);
        }

        bool
        attest(uint64_t time, uint32_t rnd)
        {
            ChainStateTrack& st = destState();

            // check all signers, but start at a random one
            for (size_t i = 0; i < num_signers; ++i)
            {
                size_t signer_idx = (rnd + i) % num_signers;
                if (!(xfer.attested[signer_idx]))
                {
                    // enqueue one attestation for this signer
                    xfer.attested[signer_idx] = true;
                    attestation_add_batch_to_vector(
                        st.signers_attns[signer_idx][&bridge_].xfer_claims,
                        bridge_.jvb,
                        xfer.from,
                        xfer.amt,
                        &bridge_.signers[signer_idx].account,
                        xfer.a2b,
                        xfer.claim_id,
                        xfer.with_claim
                            ? std::nullopt
                            : std::optional<jtx::Account>(xfer.finaldest),
                        &bridge_.signers[signer_idx],
                        1);
                    break;
                }
            }

            // return true if quorum was reached, false otherwise
            bool quorum =
                std::count(xfer.attested.begin(), xfer.attested.end(), true) >=
                bridge_.quorum;
            if (quorum)
            {
                auto r = bridge_.reward;
                auto reward = divide(r, STAmount(bridge_.quorum), r.issue());

                for (size_t i = 0; i < num_signers; ++i)
                {
                    if (xfer.attested[i])
                        st.receive(bridge_.signers[i].account, reward);
                }
                st.spend(xfer.to, reward, bridge_.quorum);
                if (!xfer.with_claim)
                    st.transfer(dstDoor(), xfer.finaldest, xfer.amt);
            }
            return quorum;
        }

        void
        claim()
        {
            ChainStateTrack& st = destState();
            st.env.tx(xchain_claim(
                xfer.to, bridge_.jvb, xfer.claim_id, xfer.amt, xfer.finaldest));
            st.transfer(dstDoor(), xfer.finaldest, xfer.amt);
            st.spendFee(xfer.to);
        }

        SmState
        advance(uint64_t time, uint32_t rnd)
        {
            switch (sm_state)
            {
                case st_initial:
                    xfer.claim_id = create_claim_id();
                    sm_state = st_claimid_created;
                    break;

                case st_claimid_created:
                    commit();
                    sm_state = st_attesting;
                    break;

                case st_attesting:
                    sm_state = attest(time, rnd)
                        ? (xfer.with_claim ? st_attested : st_completed)
                        : st_attesting;
                    break;

                case st_attested:
                    assert(xfer.with_claim);
                    claim();
                    sm_state = st_completed;
                    break;

                default:
                case st_completed:
                    assert(0);  // should have been removed
                    break;
            }
            return sm_state;
        }

    private:
        Transfer xfer;
        SmState sm_state;
    };

    // --------------------------------------------------
    using Sm = std::variant<SmCreateAccount, SmTransfer>;
    using SmCont = std::list<std::pair<uint64_t, Sm>>;

    SmCont sm_;

    void
    xfer(
        uint64_t time,
        const std::shared_ptr<ChainStateTracker>& chainstate,
        BridgeDef const& bridge,
        Transfer transfer)
    {
        sm_.emplace_back(
            time, SmTransfer(chainstate, bridge, std::move(transfer)));
    }

    void
    ac(uint64_t time,
       const std::shared_ptr<ChainStateTracker>& chainstate,
       BridgeDef const& bridge,
       AccountCreate ac)
    {
        sm_.emplace_back(
            time, SmCreateAccount(chainstate, bridge, std::move(ac)));
    }

public:
    void
    runSimulation(
        std::shared_ptr<ChainStateTracker> const& st,
        bool verify_balances = true)
    {
        using namespace jtx;

        uint64_t time = 0;
        std::mt19937 gen(27);  // Standard mersenne_twister_engine
        std::uniform_int_distribution<uint32_t> distrib(0, 9);

        while (!sm_.empty())
        {
            ++time;
            for (auto it = sm_.begin(); it != sm_.end();)
            {
                auto vis = [&](auto& sm) {
                    uint32_t rnd = distrib(gen);
                    return sm.advance(time, rnd);
                };
                auto& [t, sm] = *it;
                if (t <= time && std::visit(vis, sm) == st_completed)
                    it = sm_.erase(it);
                else
                    ++it;
            }

            // send attestations
            st->sendAttestations();

            // make sure all transactions have been applied
            st->a_.env.close();
            st->b_.env.close();

            if (verify_balances)
            {
                BEAST_EXPECT(st->verify());
            }
        }
    }

    void
    testXChainSimulation()
    {
        using namespace jtx;

        testcase("Bridge usage simulation");

        XEnv mcEnv(*this);
        XEnv scEnv(*this, true);

        auto st = std::make_shared<ChainStateTracker>(mcEnv, scEnv);

        // create 10 accounts + door funded on both chains, and store
        // in ChainStateTracker the initial amount of these accounts
        Account doorA, doorB;

        constexpr size_t num_acct = 10;
        auto a = [&doorA, &doorB]() {
            using namespace std::literals;
            std::vector<Account> result;
            result.reserve(num_acct);
            for (int i = 0; i < num_acct; ++i)
                result.emplace_back(
                    "a"s + std::to_string(i),
                    (i % 2) ? KeyType::ed25519 : KeyType::secp256k1);
            result.emplace_back("doorA");
            doorA = result.back();
            result.emplace_back("doorB");
            doorB = result.back();
            return result;
        }();

        for (auto& acct : a)
        {
            STAmount amt{XRP(100000)};

            mcEnv.fund(amt, acct);
            scEnv.fund(amt, acct);
        }

        IOU usdA{doorA["USD"]};
        IOU usdB{doorB["USD"]};

        for (int i = 0; i < a.size(); ++i)
        {
            auto& acct{a[i]};
            if (i < num_acct)
            {
                mcEnv.tx(trust(acct, usdA(100000)));
                scEnv.tx(trust(acct, usdB(100000)));
            }
            st->init(acct);
        }
        for (auto& s : signers)
            st->init(s.account);

        st->b_.init(Account::master);

        // also create some unfunded accounts
        constexpr size_t num_ua = 20;
        auto ua = []() {
            using namespace std::literals;
            std::vector<Account> result;
            result.reserve(num_ua);
            for (int i = 0; i < num_ua; ++i)
                result.emplace_back(
                    "ua"s + std::to_string(i),
                    (i % 2) ? KeyType::ed25519 : KeyType::secp256k1);
            return result;
        }();

        // initialize a bridge from a BridgeDef
        auto initBridge = [&mcEnv, &scEnv, &st](BridgeDef& bd) {
            bd.initBridge(mcEnv, scEnv);
            st->a_.spendFee(bd.doorA, 2);
            st->b_.spendFee(bd.doorB, 2);
        };

        // create XRP -> XRP bridge
        // ------------------------
        BridgeDef xrp_b{
            doorA,
            xrpIssue(),
            Account::master,
            xrpIssue(),
            XRP(1),
            XRP(20),
            quorum,
            signers,
            Json::nullValue};

        initBridge(xrp_b);

        // create USD -> USD bridge
        // ------------------------
        BridgeDef usd_b{
            doorA,
            usdA,
            doorB,
            usdB,
            XRP(1),
            XRP(20),
            quorum,
            signers,
            Json::nullValue};

        initBridge(usd_b);

        // try a single account create + transfer to validate the simulation
        // engine. Do the transfer 8 time steps after the account create, to
        // give  time enough for ua[0] to be funded now so it can reserve the
        // claimID
        // -----------------------------------------------------------------
        ac(0, st, xrp_b, {a[0], ua[0], XRP(777), xrp_b.reward, true});
        xfer(8, st, xrp_b, {a[0], ua[0], a[2], XRP(3), true});
        runSimulation(st);

        // try the same thing in the other direction
        // -----------------------------------------
        ac(0, st, xrp_b, {a[0], ua[0], XRP(777), xrp_b.reward, false});
        xfer(8, st, xrp_b, {a[0], ua[0], a[2], XRP(3), false});
        runSimulation(st);

        // run multiple XRP transfers
        // --------------------------
        xfer(0, st, xrp_b, {a[0], a[0], a[1], XRP(6), true});
        xfer(1, st, xrp_b, {a[0], a[0], a[1], XRP(8), false});
        xfer(1, st, xrp_b, {a[1], a[1], a[1], XRP(1), true});
        xfer(2, st, xrp_b, {a[0], a[0], a[1], XRP(3), false});
        xfer(2, st, xrp_b, {a[1], a[1], a[1], XRP(5), false});
        xfer(2, st, xrp_b, {a[0], a[0], a[1], XRP(7), false});
        xfer(2, st, xrp_b, {a[1], a[1], a[1], XRP(9), true});
        runSimulation(st);

        // run one USD transfer
        // --------------------
        xfer(0, st, usd_b, {a[0], a[1], a[2], usdA(3), true});
        runSimulation(st);

        // run multiple USD transfers
        // --------------------------
        xfer(0, st, usd_b, {a[0], a[0], a[1], usdA(6), true});
        xfer(1, st, usd_b, {a[0], a[0], a[1], usdB(8), false});
        xfer(1, st, usd_b, {a[1], a[1], a[1], usdA(1), true});
        xfer(2, st, usd_b, {a[0], a[0], a[1], usdB(3), false});
        xfer(2, st, usd_b, {a[1], a[1], a[1], usdB(5), false});
        xfer(2, st, usd_b, {a[0], a[0], a[1], usdB(7), false});
        xfer(2, st, usd_b, {a[1], a[1], a[1], usdA(9), true});
        runSimulation(st);

        // run mixed transfers
        // -------------------
        xfer(0, st, xrp_b, {a[0], a[0], a[0], XRP(1), true});
        xfer(0, st, usd_b, {a[1], a[3], a[3], usdB(3), false});
        xfer(0, st, usd_b, {a[3], a[2], a[1], usdB(5), false});

        xfer(1, st, xrp_b, {a[0], a[0], a[0], XRP(4), false});
        xfer(1, st, xrp_b, {a[1], a[1], a[0], XRP(8), true});
        xfer(1, st, usd_b, {a[4], a[1], a[1], usdA(7), true});

        xfer(3, st, xrp_b, {a[1], a[1], a[0], XRP(7), true});
        xfer(3, st, xrp_b, {a[0], a[4], a[3], XRP(2), false});
        xfer(3, st, xrp_b, {a[1], a[1], a[0], XRP(9), true});
        xfer(3, st, usd_b, {a[3], a[1], a[1], usdB(11), false});
        runSimulation(st);

        // run multiple account create to stress attestation batching
        // ----------------------------------------------------------
        ac(0, st, xrp_b, {a[0], ua[1], XRP(301), xrp_b.reward, true});
        ac(0, st, xrp_b, {a[1], ua[2], XRP(302), xrp_b.reward, true});
        ac(1, st, xrp_b, {a[0], ua[3], XRP(303), xrp_b.reward, true});
        ac(2, st, xrp_b, {a[1], ua[4], XRP(304), xrp_b.reward, true});
        ac(3, st, xrp_b, {a[0], ua[5], XRP(305), xrp_b.reward, true});
        ac(4, st, xrp_b, {a[1], ua[6], XRP(306), xrp_b.reward, true});
        ac(6, st, xrp_b, {a[0], ua[7], XRP(307), xrp_b.reward, true});
        ac(7, st, xrp_b, {a[2], ua[8], XRP(308), xrp_b.reward, true});
        ac(9, st, xrp_b, {a[0], ua[9], XRP(309), xrp_b.reward, true});
        ac(9, st, xrp_b, {a[0], ua[9], XRP(309), xrp_b.reward, true});
        ac(10, st, xrp_b, {a[0], ua[10], XRP(310), xrp_b.reward, true});
        ac(12, st, xrp_b, {a[0], ua[11], XRP(311), xrp_b.reward, true});
        ac(12, st, xrp_b, {a[3], ua[12], XRP(312), xrp_b.reward, true});
        ac(12, st, xrp_b, {a[4], ua[13], XRP(313), xrp_b.reward, true});
        ac(12, st, xrp_b, {a[3], ua[14], XRP(314), xrp_b.reward, true});
        ac(12, st, xrp_b, {a[6], ua[15], XRP(315), xrp_b.reward, true});
        ac(13, st, xrp_b, {a[7], ua[16], XRP(316), xrp_b.reward, true});
        ac(15, st, xrp_b, {a[3], ua[17], XRP(317), xrp_b.reward, true});
        runSimulation(st, true);  // balances verification not working?
    }

    void
    run() override
    {
        testXChainSimulation();
    }
};

struct XChainCoverage_test : public beast::unit_test::suite,
                             public jtx::XChainBridgeObjects
{
    void
    CreateAccountIssue()
    {
        using namespace jtx;

        XEnv mcEnv(*this);
        XEnv scEnv(*this, true);

        XRPAmount tx_fee = mcEnv.txFee();

        Account a{"a"};
        Account doorA{"doorA"};

        STAmount funds{XRP(10000)};
        mcEnv.fund(funds, a);
        mcEnv.fund(funds, doorA);

        Account ua{"ua"};  // unfunded account we want to create

        BridgeDef xrp_b{
            doorA,
            xrpIssue(),
            Account::master,
            xrpIssue(),
            XRP(1),   // reward
            XRP(20),  // minAccountCreate
            4,        // quorum
            signers,
            Json::nullValue};

        xrp_b.initBridge(mcEnv, scEnv);

        auto const amt = XRP(777);
        auto const amt_plus_reward = amt + xrp_b.reward;
        {
            Balance bal_doorA(mcEnv, doorA);
            Balance bal_a(mcEnv, a);

            mcEnv
                .tx(sidechain_xchain_account_create(
                    a, xrp_b.jvb, ua, amt, xrp_b.reward))
                .close();

            BEAST_EXPECT(bal_doorA.diff() == amt_plus_reward);
            BEAST_EXPECT(bal_a.diff() == -(amt_plus_reward + tx_fee));
        }

        {
            Balance bal_master(scEnv, Account::master);

            // send 4 attestations from 4 signers in one batch. account creation
            // should occur when processing the batch
            std::vector<AttestationBatch::AttestationCreateAccount> attns;
            for (size_t i = 0; i < xrp_b.quorum; ++i)
            {
                create_account_batch_add_to_vector(
                    attns,
                    xrp_b.jvb,
                    a,
                    amt,
                    xrp_b.reward,
                    &signers[i].account,
                    true,
                    1,
                    ua,
                    &signers[i],
                    1);
            }
            AttestationBatch::AttestationClaim* nullc = nullptr;
            STXChainBridge const stBridge(xrp_b.jvb);
            STXChainAttestationBatch attn_batch{
                stBridge, nullc, nullc, attns.begin(), attns.end()};
            auto batch = attn_batch.getJson(JsonOptions::none);
            auto attn_json =
                xchain_add_attestation_batch(signers[0].account, batch);
            scEnv.tx(attn_json).close();

            // on receiving the batch, the quorum is reached and the account
            // create is executed in XChainBridge.cpp. It executes
            // XChainAddAttestation::applyCreateAccountAtt, however
            // finalizeClaimHelper returns an error when trying to delete the
            // sleCID (because it never was created)

            BEAST_EXPECT(bal_master.diff() == -amt_plus_reward);
        }
    }

    void
    CoverageTests()
    {
        using namespace jtx;

        // coverage test: BridgeCreate::preclaim() returns terNO_ACCOUNT.
        //
        // [greg] I don't think hitting this is possible - When trying the
        // below it hits a previous check in BridgeCreate::preflight()
        // returning temSIDECHAIN_NONDOOR_OWNER;
        //
        // XEnv(*this).tx(
        //    create_bridge(
        //        mcAlice, bridge(mcuAlice, mcAlice["USD"], mcBob,
        //        mcBob["USD"])),
        //    ter(terNO_ACCOUNT));

        // coverage test: BridgeCreate::preclaim() returns tecNO_ISSUER.

        // coverage test: transferHelper() - dst == src
    }

    void
    run() override
    {
        CoverageTests();
        // CreateAccountIssue();
    }
};

BEAST_DEFINE_TESTSUITE(XChain, app, ripple);
BEAST_DEFINE_TESTSUITE(XChainSim, app, ripple);
BEAST_DEFINE_TESTSUITE(XChainCoverage, app, ripple);

}  // namespace ripple::test
