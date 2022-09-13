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

#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <fstream>
#include <iostream>

namespace ripple::test {

template <class T>
struct xEnv : public jtx::XChainBridgeObjects
{
    jtx::Env env_;

    xEnv(T& s, bool side = false)
        : env_(s, jtx::envconfig(jtx::port_increment, side ? 3 : 0), features)
    {
        using namespace jtx;
        STAmount xrp_funds{XRP(10000)};

        if (!side)
        {
            env_.fund(xrp_funds, mcDoor, mcAlice, mcBob, mcCarol, mcGw);

            // Signer's list must match the attestation signers
            // env_(jtx::signers(mcDoor, quorum, signers));
        }
        else
        {
            env_.fund(
                xrp_funds,
                scDoor,
                scAlice,
                scBob,
                scCarol,
                scGw,
                scAttester,
                scReward);

            for (auto& ra : payees)
                env_.fund(xrp_funds, ra);

            // Signer's list must match the attestation signers
            // env_(jtx::signers(Account::master, quorum, signers));
        }
        env_.close();
    }

    xEnv&
    close()
    {
        env_.close();
        return *this;
    }

    template <class Arg, class... Args>
    xEnv&
    fund(STAmount const& amount, Arg const& arg, Args const&... args)
    {
        env_.fund(amount, arg, args...);
        return *this;
    }

    template <class JsonValue, class... FN>
    xEnv&
    tx(JsonValue&& jv, FN const&... fN)
    {
        env_(std::forward<JsonValue>(jv), fN...);
        return *this;
    }

    STAmount
    balance(jtx::Account const& account) const
    {
        return env_.balance(account).value();
    }
};

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

struct XChain_test : public beast::unit_test::suite,
                     public jtx::XChainBridgeObjects
{
    XRPAmount
    reserve(std::uint32_t count)
    {
        return xEnv(*this).env_.current()->fees().accountReserve(count);
    }

    XRPAmount
    txFee()
    {
        return xEnv(*this).env_.current()->fees().base;
    }

    void
    testXChainCreateBridge()
    {
        XRPAmount res1 = reserve(1);

        using namespace jtx;
        testcase("Create Bridge");

        // Bridge not owned by one of the door account.
        xEnv(*this).tx(create_bridge(mcBob), ter(temSIDECHAIN_NONDOOR_OWNER));

        // Create twice on the same account
        xEnv(*this)
            .tx(create_bridge(mcDoor))
            .close()
            .tx(create_bridge(mcDoor), ter(tecDUPLICATE));

        // Create USD bridge Alice -> Bob ... should succeed
        xEnv(*this).tx(
            create_bridge(
                mcAlice, bridge(mcAlice, mcAlice["USD"], mcBob, mcBob["USD"])),
            ter(tesSUCCESS));

        // Create where both door accounts are on the same chain. The second
        // bridge create should fail.
        xEnv(*this)
            .tx(create_bridge(
                mcAlice, bridge(mcAlice, mcAlice["USD"], mcBob, mcBob["USD"])))
            .close()
            .tx(create_bridge(
                    mcBob,
                    bridge(mcAlice, mcAlice["USD"], mcBob, mcBob["USD"])),
                ter(tecDUPLICATE));

        // Bridge where the two door accounts are equal.
        xEnv(*this).tx(
            create_bridge(
                mcBob, bridge(mcBob, mcBob["USD"], mcBob, mcBob["USD"])),
            ter(temEQUAL_DOOR_ACCOUNTS));

        // Create an bridge on an account with exactly enough balance to
        // meet the new reserve should succeed
        xEnv(*this)
            .fund(res1, mcuDoor)  // exact reserve for account + 1 object
            .close()
            .tx(create_bridge(mcuDoor, jvub), ter(tesSUCCESS));

        // Create an bridge on an account with no enough balance to meet the
        // new reserve
        xEnv(*this)
            .fund(res1 - 1, mcuDoor)  // just short of required reserve
            .close()
            .tx(create_bridge(mcuDoor, jvub), ter(tecINSUFFICIENT_RESERVE));

        // Reward amount is non-xrp
        xEnv(*this).tx(
            create_bridge(mcDoor, jvb, mcUSD(1)),
            ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT));

        // Reward amount is XRP and negative
        xEnv(*this).tx(
            create_bridge(mcDoor, jvb, XRP(-1)),
            ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT));

        // Reward amount is zero
        xEnv(*this).tx(
            create_bridge(mcDoor, jvb, XRP(0)),
            ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT));

        // Reward amount is 1 xrp => should succeed
        xEnv(*this).tx(create_bridge(mcDoor, jvb, XRP(1)), ter(tesSUCCESS));

        // Min create amount is 1 xrp, mincreate is 1 xrp => should succeed
        xEnv(*this).tx(
            create_bridge(mcDoor, jvb, XRP(1), XRP(1)), ter(tesSUCCESS));

        // Min create amount is non-xrp
        xEnv(*this).tx(
            create_bridge(mcDoor, jvb, XRP(1), mcUSD(100)),
            ter(temXCHAIN_BRIDGE_BAD_MIN_ACCOUNT_CREATE_AMOUNT));

        // Min create amount is zero (should fail, currently succeeds)
        xEnv(*this).tx(
            create_bridge(mcDoor, jvb, XRP(1), XRP(0)),
            ter(temXCHAIN_BRIDGE_BAD_MIN_ACCOUNT_CREATE_AMOUNT));

        // Min create amount is negative
        xEnv(*this).tx(
            create_bridge(mcDoor, jvb, XRP(1), XRP(-1)),
            ter(temXCHAIN_BRIDGE_BAD_MIN_ACCOUNT_CREATE_AMOUNT));
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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
        xEnv(*this).tx(
            bridge_modify(
                mcAlice,
                bridge(mcAlice, mcAlice["USD"], mcBob, mcBob["USD"]),
                XRP(2),
                XRP(10)),
            ter(tecNO_ENTRY));

        // must change something
        // xEnv(*this)
        //    .tx(create_bridge(mcDoor, jvb, XRP(1), XRP(1)))
        //    .tx(bridge_modify(mcDoor, jvb, XRP(1), XRP(1)),
        //    ter(temMALFORMED));

        // must change something
        xEnv(*this)
            .tx(create_bridge(mcDoor, jvb, XRP(1), XRP(1)))
            .close()
            .tx(bridge_modify(mcDoor, jvb, {}, {}), ter(temMALFORMED));

        // Reward amount is non-xrp
        xEnv(*this).tx(
            bridge_modify(mcDoor, jvb, mcUSD(2), XRP(10)),
            ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT));

        // Reward amount is XRP and negative
        xEnv(*this).tx(
            bridge_modify(mcDoor, jvb, XRP(-2), XRP(10)),
            ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT));

        // Reward amount is zero
        xEnv(*this).tx(
            bridge_modify(mcDoor, jvb, XRP(0), XRP(10)),
            ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT));

        // Min create amount is non-xrp
        xEnv(*this).tx(
            bridge_modify(mcDoor, jvb, XRP(2), mcUSD(10)),
            ter(temXCHAIN_BRIDGE_BAD_MIN_ACCOUNT_CREATE_AMOUNT));

        // Min create amount is zero
        xEnv(*this).tx(
            bridge_modify(mcDoor, jvb, XRP(2), XRP(0)),
            ter(temXCHAIN_BRIDGE_BAD_MIN_ACCOUNT_CREATE_AMOUNT));

        // Min create amount is negative
        xEnv(*this).tx(
            bridge_modify(mcDoor, jvb, XRP(2), XRP(-10)),
            ter(temXCHAIN_BRIDGE_BAD_MIN_ACCOUNT_CREATE_AMOUNT));

        // First check the regular claim process (without bridge_modify)
        for (auto withClaim : {false, true})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
    }

    void
    testXChainCreateClaimID()
    {
        using namespace jtx;
        XRPAmount res1 = reserve(1);

        testcase("Create ClaimID");

        // normal bridge create for sanity check with the exact necessary
        // account balance
        xEnv(*this, true)
            .tx(create_bridge(Account::master, jvb))
            .fund(res1, scuAlice)  // acct reserve + 1 object
            .close()
            .tx(xchain_create_claim_id(scuAlice, jvb, reward, mcAlice))
            .close();

        // Non-existent bridge
        xEnv(*this, true)
            .tx(xchain_create_claim_id(
                    scAlice,
                    bridge(mcAlice, mcAlice["USD"], scBob, scBob["USD"]),
                    reward,
                    mcAlice),
                ter(tecNO_ENTRY))
            .close();

        // Creating the new object would put the account below the reserve
        xEnv(*this, true)
            .tx(create_bridge(Account::master, jvb))
            .fund(res1 - xrp_dust, scuAlice)  // barely not enough
            .close()
            .tx(xchain_create_claim_id(scuAlice, jvb, reward, mcAlice),
                ter(tecINSUFFICIENT_RESERVE))
            .close();

        // The specified reward doesn't match the reward on the bridge (test by
        // giving the reward amount for the other side, as well as a completely
        // non-matching reward)
        xEnv(*this, true)
            .tx(create_bridge(Account::master, jvb))
            .close()
            .tx(xchain_create_claim_id(scAlice, jvb, split_reward, mcAlice),
                ter(tecXCHAIN_REWARD_MISMATCH))
            .close();

        // A reward amount that isn't XRP
        xEnv(*this, true)
            .tx(create_bridge(Account::master, jvb))
            .close()
            .tx(xchain_create_claim_id(scAlice, jvb, mcUSD(1), mcAlice),
                ter(temXCHAIN_BRIDGE_BAD_REWARD_AMOUNT))
            .close();
    }

    void
    testXChainCommit()
    {
        using namespace jtx;
        XRPAmount res0 = reserve(0);

        testcase("Commit");

        // Commit to a non-existent bridge
        xEnv(*this).tx(
            xchain_commit(mcAlice, jvb, 1, one_xrp, scBob), ter(tecNO_ENTRY));

        // Commit a negative amount
        xEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .close()
            .tx(xchain_commit(mcAlice, jvb, 1, XRP(-1), scBob),
                ter(temBAD_AMOUNT));

        // Commit an amount whose issue that does not match the expected issue
        // on the bridge (either LockingChainIssue or IssuingChainIssue,
        // depending on the chain).
        xEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .close()
            .tx(xchain_commit(mcAlice, jvb, 1, mcUSD(100), scBob),
                ter(tecBAD_XCHAIN_TRANSFER_ISSUE));

        // Commit an amount that would put the sender below the required reserve
        // (if XRP)
        xEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .fund(res0 + one_xrp - xrp_dust, mcuAlice)  // barely not enough
            .close()
            .tx(xchain_commit(mcuAlice, jvb, 1, one_xrp, scBob),
                ter(tecINSUFFICIENT_FUNDS));

        xEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .fund(
                res0 + one_xrp + xrp_dust,  // "xrp_dust" for tx fees
                mcuAlice)                   // exactly enough => should succeed
            .close()
            .tx(xchain_commit(mcuAlice, jvb, 1, one_xrp, scBob));

        // Commit an amount above the account's balance (for both XRP and IOUs)
        xEnv(*this)
            .tx(create_bridge(mcDoor, jvb))
            .fund(res0, mcuAlice)  // barely not enough
            .close()
            .tx(xchain_commit(mcuAlice, jvb, 1, res0 + one_xrp, scBob),
                ter(tecINSUFFICIENT_FUNDS));

        auto jvb_USD = bridge(mcDoor, mcUSD, scGw, scUSD);

        // commit sent from iou issuer (mcGw) succeeds - should it?
        xEnv(*this)
            .tx(trust(mcDoor, mcUSD(10000)))  // door needs to have a trustline
            .tx(create_bridge(mcDoor, jvb_USD))
            .close()
            .tx(xchain_commit(mcGw, jvb_USD, 1, mcUSD(1), scBob));

        // commit to a door account from the door account. This should fail.
        xEnv(*this)
            .tx(trust(mcDoor, mcUSD(10000)))  // door needs to have a trustline
            .tx(create_bridge(mcDoor, jvb_USD))
            .close()
            .tx(xchain_commit(mcDoor, jvb_USD, 1, mcUSD(1), scBob),
                ter(tecXCHAIN_SELF_COMMIT));

        // commit sent from mcAlice which has no IOU balance => should fail
        xEnv(*this)
            .tx(trust(mcDoor, mcUSD(10000)))  // door needs to have a trustline
            .tx(create_bridge(mcDoor, jvb_USD))
            .close()
            .tx(xchain_commit(mcAlice, jvb_USD, 1, mcUSD(1), scBob),
                ter(terNO_LINE));

        // commit sent from mcAlice which has no IOU balance => should fail
        // just changed the destination to scGw (which is the door account and
        // may not make much sense)
        xEnv(*this)
            .tx(trust(mcDoor, mcUSD(10000)))  // door needs to have a trustline
            .tx(create_bridge(mcDoor, jvb_USD))
            .close()
            .tx(xchain_commit(mcAlice, jvb_USD, 1, mcUSD(1), scGw),
                ter(terNO_LINE));

        // commit sent from mcAlice which has a IOU balance => should succeed
        xEnv(*this)
            .tx(trust(mcDoor, mcUSD(10000)))
            .tx(trust(mcAlice, mcUSD(10000)))
            .close()
            .tx(pay(mcGw, mcAlice, mcUSD(10)))
            .tx(create_bridge(mcDoor, jvb_USD))
            .close()
            //.tx(pay(mcAlice, mcDoor, mcUSD(10)));
            .tx(xchain_commit(mcAlice, jvb_USD, 1, mcUSD(10), scAlice));
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
        for (auto withClaim : {true})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            }

            BEAST_EXPECT(transfer.has_happened(amt, split_reward));
        }

        // Add a batch of attestations where one has an invalid signature. The
        // entire transaction should fail.
        for (auto withClaim : {false, true})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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

            BEAST_EXPECT(attester.diff() == -attester_expense);
            // >= because of reward drops left when dividing by 3 attestations
            BEAST_EXPECT(alice.diff() >= added_amt);
            BEAST_EXPECT(bob.diff() >= added_amt);
            BEAST_EXPECT(carol.diff() == added_amt - carol_expense);
        }

        // Add a batch of attestations for different claim ids. None of the
        // claim ids exist
        for (auto withClaim : {false, true})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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

            BEAST_EXPECT(attester.diff() == -tx_fee);
            BEAST_EXPECT(alice.diff() == added_amt);
            BEAST_EXPECT(bob.diff() == added_amt);
            BEAST_EXPECT(carol.diff() == added_amt);
        }

        // ++++++++++++todo

        // Add a batch of attestations for different claim ids. All the claim id
        // exist. Test for both reaching quorum
        for (auto withClaim : {false, true})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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

            BEAST_EXPECT(attester.diff() == -attester_expense);
            BEAST_EXPECT(alice.diff() == added_amt);
            BEAST_EXPECT(bob.diff() == added_amt);
            BEAST_EXPECT(carol.diff() == added_amt);
        }

        // Add a batch of attestations for different claim ids. All the claim id
        // exist. Test for both going over quorum.
        for (auto withClaim : {false, true})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close()
                .tx(xchain_create_claim_id(scBob, jvb, reward, mcBob))
                .close();

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

            BEAST_EXPECT(attester.diff() == -attester_expense);
            BEAST_EXPECT(alice.diff() == added_amt);
            BEAST_EXPECT(bob.diff() == added_amt);
        }

        // Add a batch of attestations for different claim ids. All the claim id
        // exist. Test for one reaching quorum and not the other
        for (auto withClaim : {false, true})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            std::uint32_t const red_quorum = 2;
            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, red_quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close()
                .tx(xchain_create_claim_id(scBob, jvb, reward, mcBob))
                .close();

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

            BEAST_EXPECT(attester.diff() == -attester_expense);
            BEAST_EXPECT(alice.diff() == (withClaim ? -tx_fee : STAmount(0)));
            BEAST_EXPECT(bob.diff() == added_amt);
        }

        // Add attestations where some of the attestations are inconsistent with
        // each other. The entire transaction should fail. Being inconsistent
        // means attesting to different values.
        for (auto withClaim : {false, true})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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

            auto messup_sig =
                [&](size_t i, Buffer sig, jtx::signer s, AnyAmount amount) {
                    if (i == 2)
                    {
                        jtx::AnyAmount const new_amt = XRP(1001);
                        auto const sig = sign_claim_attestation(
                            s.account.pk(),
                            s.account.sk(),
                            STXChainBridge(jvb),
                            mcAlice,
                            new_amt.value,
                            payees[i],
                            true,
                            claimID,
                            dst);
                        return std::make_tuple(std::move(sig), new_amt);
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

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Test that signature weights are correctly handled. Assign signature
        // weights of 1,2,4,4 and a quorum of 7. Check that the 4,4 signatures
        // reach a quorum, the 1,2,4, reach a quorum, but the 4,2, 4,1 and 1,2
        // do not.

        // 1,2,4 => should succeed
        for (auto withClaim : {false, true})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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

            BEAST_EXPECT(transfer.has_happened(
                amt, divide(reward, STAmount(3), reward.issue())));
        }

        // 4,4 => should succeed
        for (auto withClaim : {false, true})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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

            BEAST_EXPECT(transfer.has_happened(
                amt, divide(reward, STAmount(2), reward.issue())));
        }

        // 1,2 => should fail
        for (auto withClaim : {false, true})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // 2,4 => should fail
        for (auto withClaim : {false, true})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Add more than the maximum number of allowed attestations (8). This
        // should fail.
        for (auto withClaim : {false})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

            mcEnv.tx(create_bridge(mcDoor, jvb)).close();

            scEnv.tx(create_bridge(Account::master, jvb))
                .tx(jtx::signers(Account::master, quorum, signers))
                .close()
                .tx(xchain_create_claim_id(scAlice, jvb, reward, mcAlice))
                .close();

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

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Add attestations for both account create and claims.
        for (auto withClaim : {false})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);
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

            auto dst(withClaim ? std::nullopt : std::optional<Account>{scBob});
            std::uint32_t const claimID = 1;
            mcEnv.tx(xchain_commit(mcAlice, jvb, claimID, amt, dst)).close();

            BalanceTransfer transfer(
                scEnv, Account::master, scBob, scAlice, payees, withClaim);

            scEnv.tx(att_claim_batch1(mcAlice, claimID, amt, dst))
                .tx(att_create_acct_batch2(1, amt, scuAlice))
                .close();

            BEAST_EXPECT(transfer.has_not_happened());

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);
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
            }

            {
                // complete attestations for 2nd account create => should not
                // complete
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                scEnv.tx(att_create_acct_batch2(2, amt, scuBob)).close();

                BEAST_EXPECT(door.diff() == STAmount(0));
                BEAST_EXPECT(attester.diff() == -tx_fee);
            }

            {
                // complete attestations for 3rd account create => should not
                // complete
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                scEnv.tx(att_create_acct_batch2(3, amt, scuCarol)).close();

                BEAST_EXPECT(door.diff() == STAmount(0));
                BEAST_EXPECT(attester.diff() == -tx_fee);
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
            }

            {
                // resend attestations for 3rd account create => still should
                // not complete
                Balance attester(scEnv, scAttester);
                Balance door(scEnv, Account::master);

                scEnv.tx(att_create_acct_batch2(3, amt, scuCarol)).close();

                BEAST_EXPECT(door.diff() == STAmount(0));
                BEAST_EXPECT(attester.diff() == -tx_fee);
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
            }
        }

        // Check that creating an account with less than the minimum create
        // amount fails.
        {
            xEnv mcEnv(*this);
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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
                .tx(jtx::signers(Account::master, quorum, signers));

            Balance attester(scEnv, scAttester);
            Balance door(scEnv, Account::master);

            scEnv.close()
                .tx(att_create_acct_batch1(1, amt, scuAlice))
                .tx(att_create_acct_batch2(1, amt, scuAlice))
                .close();

            BEAST_EXPECT(attester.diff() == -tx_fee_2);
            BEAST_EXPECT(door.diff() == -reward);
            BEAST_EXPECT(!scEnv.env_.le(scuAlice));
        }

        // Check that sending funds with an account create txn to an existing
        // account works.
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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

            scEnv.tx(att_create_acct_batch1(1, amt, scAlice))
                .tx(att_create_acct_batch2(1, amt, scAlice))
                .close();

            BEAST_EXPECT(door.diff() == -amt_plus_reward);
            BEAST_EXPECT(attester.diff() == -tx_fee_2);
            BEAST_EXPECT(alice.diff() == amt);
        }

        // Check that sending funds to an existing account with deposit auth set
        // fails for account create transactions.
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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

            scEnv.tx(att_create_acct_batch1(1, amt, scAlice))
                .tx(att_create_acct_batch2(1, amt, scAlice))
                .close();

            BEAST_EXPECT(door.diff() == -reward);
            BEAST_EXPECT(attester.diff() == -tx_fee_2);
            BEAST_EXPECT(alice.diff() == STAmount(0));
        }

        // If an account is unable to pay the reserve, check that it fails.
        // [greg todo] I don't know what this should test??

        // Create several accounts with a single batch attestation. This should
        // succeed.
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);
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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);
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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);
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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);
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

                atts.clear();
                att_create_acct_add_n(atts, 3, amt, scuCarol, 4, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                atts.clear();
                att_create_acct_add_n(atts, 3, amt, scuCarol, 0, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                atts.clear();
                att_create_acct_add_n(atts, 1, amt, scuAlice, 2, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                atts.clear();
                att_create_acct_add_n(atts, 2, amt, scuBob, 3, 1);
                att_create_acct_add_n(atts, 1, amt, scuAlice, 3, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                atts.clear();
                att_create_acct_add_n(atts, 3, amt, scuCarol, 0, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);
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

                // note: if we send inconsistent attestations in the same batch,
                // the transaction errors.

                // from now on we send correct attestations
                atts.clear();
                att_create_acct_add_n(atts, 1, amt, scuAlice, 0, 1);
                att_create_acct_add_n(atts, 2, amt, scuBob, 2, 1);
                att_create_acct_add_n(atts, 3, amt, scuCarol, 4, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                atts.clear();
                att_create_acct_add_n(atts, 3, amt, scuCarol, 1, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                atts.clear();
                att_create_acct_add_n(atts, 1, amt, scuAlice, 2, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                atts.clear();
                att_create_acct_add_n(atts, 2, amt, scuBob, 3, 1);
                att_create_acct_add_n(atts, 1, amt, scuAlice, 3, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

                atts.clear();
                att_create_acct_add_n(atts, 3, amt, scuCarol, 0, 1);
                scEnv.tx(att_claim_json(jvb, {}, atts)).close();

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            }
            else
            {
                scEnv
                    .tx(xchain_add_attestation_batch(scAttester, batch),
                        ter(tecNO_PERMISSION))
                    .close();
            }

            BEAST_EXPECT(transfer.has_not_happened());
        }

        // Claim where the amount different from what is attested to
        // ---------------------------------------------------------
        for (auto withClaim : {true})
        {
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
            xEnv mcEnv(*this);
            xEnv scEnv(*this, true);

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
    }

    void
    testXChainCreateAccount()
    {
        using namespace jtx;

        testcase("Bridge Create Account");
    }

    void
    testXChainDeleteDoor()
    {
        using namespace jtx;

        testcase("Bridge Delete Door Account");

        auto const acctDelFee{
            drops(xEnv(*this).env_.current()->fees().increment)};

        // Deleting a account that owns bridge should fail
        {
            xEnv mcEnv(*this);

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
            xEnv scEnv(*this, true);

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

BEAST_DEFINE_TESTSUITE(XChain, app, ripple);

}  // namespace ripple::test
