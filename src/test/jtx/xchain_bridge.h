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

#ifndef RIPPLE_TEST_JTX_XCHAINBRIDGE_H_INCLUDED
#define RIPPLE_TEST_JTX_XCHAINBRIDGE_H_INCLUDED

#include <ripple/json/json_value.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STXChainAttestationBatch.h>
#include <test/jtx/Account.h>
#include <test/jtx/amount.h>
#include <test/jtx/multisign.h>

namespace ripple {
namespace test {
namespace jtx {

Json::Value
bridge(
    Account const& lockingChainDoor,
    Issue const& lockingChainIssue,
    Account const& issuingChainDoor,
    Issue const& issuingChainIssue);

Json::Value
bridge_create(
    Account const& acc,
    Json::Value const& bridge,
    STAmount const& reward,
    std::optional<STAmount> const& minAccountCreate = std::nullopt);

Json::Value
bridge_modify(
    Account const& acc,
    Json::Value const& bridge,
    std::optional<STAmount> const& reward,
    std::optional<STAmount> const& minAccountCreate = std::nullopt);

Json::Value
xchain_create_claim_id(
    Account const& acc,
    Json::Value const& bridge,
    STAmount const& reward,
    Account const& otherChainSource);

Json::Value
xchain_commit(
    Account const& acc,
    Json::Value const& bridge,
    std::uint32_t claimID,
    AnyAmount const& amt,
    std::optional<Account> const& dst = std::nullopt);

Json::Value
xchain_claim(
    Account const& acc,
    Json::Value const& bridge,
    std::uint32_t claimID,
    AnyAmount const& amt,
    Account const& dst);

Json::Value
xchain_add_attestation_batch(Account const& acc, Json::Value const& batch);

Json::Value
sidechain_xchain_account_create(
    Account const& acc,
    Json::Value const& bridge,
    Account const& dst,
    AnyAmount const& amt,
    AnyAmount const& xChainFee);

Json::Value
sidechain_xchain_account_claim(
    Account const& acc,
    Json::Value const& bridge,
    Account const& dst,
    AnyAmount const& amt);

using attestation_cb = std::function<std::tuple<
    Buffer,
    jtx::AnyAmount>(size_t, Buffer, jtx::signer, jtx::AnyAmount)>;

extern attestation_cb default_attestation_cb;

void
attestation_add_batch_to_vector(
    std::vector<AttestationBatch::AttestationClaim>& claims,
    Json::Value const& jvBridge,
    jtx::Account const& sendingAccount,
    jtx::AnyAmount const& sendingAmount,
    jtx::Account const* rewardAccounts,
    bool wasLockingChainSend,
    std::uint64_t claimID,
    std::optional<jtx::Account> const& dst,
    jtx::signer const* signers,
    size_t num_signers,
    attestation_cb cb = default_attestation_cb);

Json::Value
attestation_claim_batch(
    Json::Value const& jvBridge,
    jtx::Account const& sendingAccount,
    jtx::AnyAmount const& sendingAmount,
    jtx::Account const* rewardAccounts,
    bool wasLockingChainSend,
    std::uint64_t claimID,
    std::optional<jtx::Account> const& dst,
    jtx::signer const* signers,
    size_t num_signers,
    attestation_cb cb = default_attestation_cb);

Json::Value
attestation_claim_batch(
    Json::Value const& jvBridge,
    jtx::Account const& sendingAccount,
    jtx::AnyAmount const& sendingAmount,
    std::vector<jtx::Account> const& rewardAccounts,
    bool wasLockingChainSend,
    std::uint64_t claimID,
    std::optional<jtx::Account> const& dst,
    std::vector<jtx::signer> const& signers);

void
create_account_batch_add_to_vector(
    std::vector<AttestationBatch::AttestationCreateAccount>& atts,
    Json::Value const& jvBridge,
    jtx::Account const& sendingAccount,
    jtx::AnyAmount const& sendingAmount,
    jtx::AnyAmount const& rewardAmount,
    jtx::Account const* rewardAccounts,
    bool wasLockingChainSend,
    std::uint64_t createCount,
    jtx::Account const& dst,
    jtx::signer const* signers,
    size_t num_signers);

Json::Value
attestation_create_account_batch(
    Json::Value const& jvBridge,
    jtx::Account const& sendingAccount,
    jtx::AnyAmount const& sendingAmount,
    jtx::AnyAmount const& rewardAmount,
    jtx::Account const* rewardAccounts,
    bool wasLockingChainSend,
    std::uint64_t createCount,
    jtx::Account const& dst,
    jtx::signer const* signers,
    size_t num_signers = 0);

struct XChainBridgeObjects
{
    // funded accounts
    Account const mcDoor;
    Account const mcAlice;
    Account const mcBob;
    Account const mcCarol;
    Account const mcGw;
    Account const scDoor;
    Account const scAlice;
    Account const scBob;
    Account const scCarol;
    Account const scGw;
    Account const scAttester;
    Account const scReward;

    // unfunded accounts
    Account const mcuDoor;
    Account const mcuAlice;
    Account const mcuBob;
    Account const mcuCarol;
    Account const mcuGw;
    Account const scuDoor;
    Account const scuAlice;
    Account const scuBob;
    Account const scuCarol;
    Account const scuGw;

    IOU const mcUSD;
    IOU const scUSD;

    Json::Value const jvXRPBridgeRPC;
    Json::Value jvb;   // standard xrp bridge def for tx
    Json::Value jvub;  // standard xrp bridge def for tx, unfunded accounts

    FeatureBitset const features;
    std::vector<signer> const signers;
    std::vector<signer> const alt_signers;
    std::vector<Account> const payee;
    std::vector<Account> const payees;
    std::uint32_t const quorum;

    STAmount const reward;        // 1 xrp
    STAmount const split_reward;  // 200,000 drops

    const STAmount tiny_reward;            // 37 drops
    const STAmount tiny_reward_split;      // 7 drops
    const STAmount tiny_reward_remainder;  // 2 drops

    const STAmount one_xrp;
    const STAmount xrp_dust;

    static constexpr int drop_per_xrp = 1000000;

    XChainBridgeObjects();

    void
    createBridgeObjects(Env& mcEnv, Env& scEnv);

    Json::Value
    att_claim_batch1(
        std::uint64_t claimID,
        jtx::AnyAmount const& amt,
        std::optional<jtx::Account> const& dst)
    {
        return xchain_add_attestation_batch(
            scAttester,
            attestation_claim_batch(
                jvb,
                mcAlice,
                amt,
                &payees[0],
                true,
                claimID,
                dst,
                &signers[0],
                3));
    }

    Json::Value
    att_claim_batch2(
        std::uint64_t claimID,
        jtx::AnyAmount const& amt,
        std::optional<jtx::Account> const& dst)
    {
        return xchain_add_attestation_batch(
            scAttester,
            attestation_claim_batch(
                jvb,
                mcAlice,
                amt,
                &payees[3],
                true,
                claimID,
                dst,
                &signers[3],
                2));
    }

    Json::Value
    att_create_acct_batch1(
        std::uint64_t createCount,
        jtx::AnyAmount const& amt,
        jtx::Account const& dst)
    {
        return xchain_add_attestation_batch(
            scAttester,
            attestation_create_account_batch(
                jvb,
                mcCarol,
                amt,
                reward,
                &payees[0],
                true,
                createCount,
                dst,
                &signers[0],
                2));
    }

    Json::Value
    att_create_acct_batch2(
        std::uint64_t createCount,
        jtx::AnyAmount const& amt,
        jtx::Account const& dst)
    {
        return xchain_add_attestation_batch(
            scAttester,
            attestation_create_account_batch(
                jvb,
                mcCarol,
                amt,
                reward,
                &payees[2],
                true,
                createCount,
                dst,
                &signers[2],
                3));
    }

    void
    att_create_acct_add_n(
        std::vector<AttestationBatch::AttestationCreateAccount>& atts,
        std::uint64_t createCount,
        jtx::AnyAmount const& amt,
        jtx::Account const& dst,
        size_t first_attest,
        size_t num_attest)
    {
        create_account_batch_add_to_vector(
            atts,
            jvb,
            mcCarol,
            amt,
            reward,
            &payees[first_attest],
            true,
            createCount,
            dst,
            &signers[first_attest],
            num_attest);
    }

    Json::Value
    att_create_acct_json(
        Json::Value const& jvBridge,
        std::vector<AttestationBatch::AttestationCreateAccount>& atts)
    {
        STXChainBridge const stBridge(jvBridge);
        AttestationBatch::AttestationClaim* nullClaimRange = nullptr;
        STXChainAttestationBatch batch{
            stBridge, nullClaimRange, nullClaimRange, atts.begin(), atts.end()};
        return xchain_add_attestation_batch(
            scAttester, batch.getJson(JsonOptions::none));
    }

    Json::Value
    create_bridge(
        Account const& acc,
        Json::Value const& bridge = Json::nullValue,
        STAmount const& _reward = XRP(1),
        std::optional<STAmount> const& minAccountCreate = XRP(20))
    {
        return bridge_create(
            acc,
            bridge == Json::nullValue ? jvb : bridge,
            _reward,
            minAccountCreate);
    }
};

}  // namespace jtx
}  // namespace test
}  // namespace ripple

#endif
