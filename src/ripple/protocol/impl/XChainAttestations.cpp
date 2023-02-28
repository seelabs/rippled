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

#include <ripple/protocol/XChainAttestations.h>

#include <ripple/basics/Expected.h>
#include <ripple/basics/StringUtilities.h>
#include <ripple/json/json_get_or_throw.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/STAmount.h>
#include <ripple/protocol/STArray.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/STXChainAttestationBatch.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/jss.h>

#include <optional>

namespace ripple {

SField const& XChainClaimAttestation::ArrayFieldName{sfXChainClaimAttestations};
SField const& XChainCreateAccountAttestation::ArrayFieldName{
    sfXChainCreateAccountAttestations};

XChainClaimAttestation::XChainClaimAttestation(
    AccountID const& keyAccount_,
    PublicKey const& publicKey_,
    STAmount const& amount_,
    AccountID const& rewardAccount_,
    bool wasLockingChainSend_,
    std::optional<AccountID> const& dst_)
    : keyAccount(keyAccount_)
    , publicKey(publicKey_)
    , amount(sfAmount, amount_)
    , rewardAccount(rewardAccount_)
    , wasLockingChainSend(wasLockingChainSend_)
    , dst(dst_)
{
}

XChainClaimAttestation::XChainClaimAttestation(
    STAccount const& keyAccount_,
    PublicKey const& publicKey_,
    STAmount const& amount_,
    STAccount const& rewardAccount_,
    bool wasLockingChainSend_,
    std::optional<STAccount> const& dst_)
    : XChainClaimAttestation{
          keyAccount_.value(),
          publicKey_,
          amount_,
          rewardAccount_.value(),
          wasLockingChainSend_,
          dst_ ? std::optional<AccountID>{dst_->value()} : std::nullopt}
{
}

XChainClaimAttestation::XChainClaimAttestation(STObject const& o)
    : XChainClaimAttestation{
          o[sfAttestationSignerAccount],
          PublicKey{o[sfPublicKey]},
          o[sfAmount],
          o[sfAttestationRewardAccount],
          o[sfWasLockingChainSend] != 0,
          o[~sfDestination]} {};

XChainClaimAttestation::XChainClaimAttestation(Json::Value const& v)
    : XChainClaimAttestation{
          Json::getOrThrow<AccountID>(v, sfAttestationSignerAccount),
          Json::getOrThrow<PublicKey>(v, sfPublicKey),
          Json::getOrThrow<STAmount>(v, sfAmount),
          Json::getOrThrow<AccountID>(v, sfAttestationRewardAccount),
          Json::getOrThrow<bool>(v, sfWasLockingChainSend),
          std::nullopt}
{
    if (v.isMember(sfDestination.getJsonName()))
        dst = Json::getOrThrow<AccountID>(v, sfDestination);
};

XChainClaimAttestation::XChainClaimAttestation(
    XChainClaimAttestation::TBatchAttestation const& claimAtt)
    : XChainClaimAttestation{
          claimAtt.attestationSignerAccount,
          claimAtt.publicKey,
          claimAtt.sendingAmount,
          claimAtt.rewardAccount,
          claimAtt.wasLockingChainSend,
          claimAtt.dst}
{
}

STObject
XChainClaimAttestation::toSTObject() const
{
    STObject o{sfXChainClaimProofSig};
    o[sfAttestationSignerAccount] =
        STAccount{sfAttestationSignerAccount, keyAccount};
    o[sfPublicKey] = publicKey;
    o[sfAmount] = STAmount{sfAmount, amount};
    o[sfAttestationRewardAccount] =
        STAccount{sfAttestationRewardAccount, rewardAccount};
    o[sfWasLockingChainSend] = wasLockingChainSend;
    if (dst)
        o[sfDestination] = STAccount{sfDestination, *dst};
    return o;
}

bool
operator==(XChainClaimAttestation const& lhs, XChainClaimAttestation const& rhs)
{
    return std::tie(
               lhs.keyAccount,
               lhs.publicKey,
               lhs.amount,
               lhs.rewardAccount,
               lhs.wasLockingChainSend,
               lhs.dst) ==
        std::tie(
               rhs.keyAccount,
               rhs.publicKey,
               rhs.amount,
               rhs.rewardAccount,
               rhs.wasLockingChainSend,
               rhs.dst);
}

XChainClaimAttestation::MatchFields::MatchFields(
    XChainClaimAttestation::TBatchAttestation const& att)
    : amount{att.sendingAmount}
    , wasLockingChainSend{att.wasLockingChainSend}
    , dst{att.dst}
{
}

AttestationMatch
XChainClaimAttestation::match(
    XChainClaimAttestation::MatchFields const& rhs) const
{
    if (std::tie(amount, wasLockingChainSend) !=
        std::tie(rhs.amount, rhs.wasLockingChainSend))
        return AttestationMatch::nonDstMatch;
    if (dst != rhs.dst)
        return AttestationMatch::matchExceptDst;
    return AttestationMatch::match;
}

//------------------------------------------------------------------------------

XChainCreateAccountAttestation::XChainCreateAccountAttestation(
    AccountID const& keyAccount_,
    PublicKey const& publicKey_,
    STAmount const& amount_,
    STAmount const& rewardAmount_,
    AccountID const& rewardAccount_,
    bool wasLockingChainSend_,
    AccountID const& dst_)
    : keyAccount(keyAccount_)
    , publicKey(publicKey_)
    , amount(sfAmount, amount_)
    , rewardAmount(sfSignatureReward, rewardAmount_)
    , rewardAccount(rewardAccount_)
    , wasLockingChainSend(wasLockingChainSend_)
    , dst(dst_)
{
}

XChainCreateAccountAttestation::XChainCreateAccountAttestation(
    STObject const& o)
    : XChainCreateAccountAttestation{
          o[sfAttestationSignerAccount],
          PublicKey{o[sfPublicKey]},
          o[sfAmount],
          o[sfSignatureReward],
          o[sfAttestationRewardAccount],
          o[sfWasLockingChainSend] != 0,
          o[sfDestination]} {};

XChainCreateAccountAttestation ::XChainCreateAccountAttestation(
    Json::Value const& v)
    : XChainCreateAccountAttestation{
          Json::getOrThrow<AccountID>(v, sfAttestationSignerAccount),
          Json::getOrThrow<PublicKey>(v, sfPublicKey),
          Json::getOrThrow<STAmount>(v, sfAmount),
          Json::getOrThrow<STAmount>(v, sfSignatureReward),
          Json::getOrThrow<AccountID>(v, sfAttestationRewardAccount),
          Json::getOrThrow<bool>(v, sfWasLockingChainSend),
          Json::getOrThrow<AccountID>(v, sfDestination)}
{
}

XChainCreateAccountAttestation::XChainCreateAccountAttestation(
    XChainCreateAccountAttestation::TBatchAttestation const& createAtt)
    : XChainCreateAccountAttestation{
          createAtt.attestationSignerAccount,
          createAtt.publicKey,
          createAtt.sendingAmount,
          createAtt.rewardAmount,
          createAtt.rewardAccount,
          createAtt.wasLockingChainSend,
          createAtt.toCreate}
{
}

STObject
XChainCreateAccountAttestation::toSTObject() const
{
    STObject o{sfXChainCreateAccountProofSig};

    o[sfAttestationSignerAccount] =
        STAccount{sfAttestationSignerAccount, keyAccount};
    o[sfPublicKey] = publicKey;
    o[sfAmount] = STAmount{sfAmount, amount};
    o[sfSignatureReward] = STAmount{sfSignatureReward, rewardAmount};
    o[sfAttestationRewardAccount] =
        STAccount{sfAttestationRewardAccount, rewardAccount};
    o[sfWasLockingChainSend] = wasLockingChainSend;
    o[sfDestination] = STAccount{sfDestination, dst};

    return o;
}

XChainCreateAccountAttestation::MatchFields::MatchFields(
    XChainCreateAccountAttestation::TBatchAttestation const& att)
    : amount{att.sendingAmount}
    , rewardAmount(att.rewardAmount)
    , wasLockingChainSend{att.wasLockingChainSend}
    , dst{att.toCreate}
{
}

AttestationMatch
XChainCreateAccountAttestation::match(
    XChainCreateAccountAttestation::MatchFields const& rhs) const
{
    if (std::tie(amount, rewardAmount, wasLockingChainSend) !=
        std::tie(rhs.amount, rhs.rewardAmount, rhs.wasLockingChainSend))
        return AttestationMatch::nonDstMatch;
    if (dst != rhs.dst)
        return AttestationMatch::matchExceptDst;
    return AttestationMatch::match;
}

bool
operator==(
    XChainCreateAccountAttestation const& lhs,
    XChainCreateAccountAttestation const& rhs)
{
    return std::tie(
               lhs.keyAccount,
               lhs.publicKey,
               lhs.amount,
               lhs.rewardAmount,
               lhs.rewardAccount,
               lhs.wasLockingChainSend,
               lhs.dst) ==
        std::tie(
               rhs.keyAccount,
               rhs.publicKey,
               rhs.amount,
               rhs.rewardAmount,
               rhs.rewardAccount,
               rhs.wasLockingChainSend,
               rhs.dst);
}

//------------------------------------------------------------------------------
//
template <class TAttestation>
XChainAttestationsBase<TAttestation>::XChainAttestationsBase(
    XChainAttestationsBase<TAttestation>::AttCollection&& atts)
    : attestations_{std::move(atts)}
{
}

template <class TAttestation>
typename XChainAttestationsBase<TAttestation>::AttCollection::const_iterator
XChainAttestationsBase<TAttestation>::begin() const
{
    return attestations_.begin();
}

template <class TAttestation>
typename XChainAttestationsBase<TAttestation>::AttCollection::const_iterator
XChainAttestationsBase<TAttestation>::end() const
{
    return attestations_.end();
}

template <class TAttestation>
XChainAttestationsBase<TAttestation>::XChainAttestationsBase(
    Json::Value const& v)
{
    // TODO: Rewrite this whole thing in the style of the
    // STXChainAttestationBatch
    if (!v.isObject())
    {
        Throw<std::runtime_error>(
            "XChainAttestationsBase can only be specified with an 'object' "
            "Json "
            "value");
    }

    attestations_ = [&] {
        auto const jAtts = v[jss::attestations];

        if (jAtts.size() > maxAttestations)
            Throw<std::runtime_error>(
                "XChainAttestationsBase exceeded max number of attestations");

        std::vector<TAttestation> r;
        r.reserve(jAtts.size());
        for (auto const& a : jAtts)
            r.emplace_back(a);
        return r;
    }();
}

template <class TAttestation>
XChainAttestationsBase<TAttestation>::XChainAttestationsBase(STArray const& arr)
{
    if (arr.size() > maxAttestations)
        Throw<std::runtime_error>(
            "XChainAttestationsBase exceeded max number of attestations");

    attestations_.reserve(arr.size());
    for (auto const& o : arr)
        attestations_.emplace_back(o);
}

template <class TAttestation>
STArray
XChainAttestationsBase<TAttestation>::toSTArray() const
{
    STArray r{TAttestation::ArrayFieldName, attestations_.size()};
    for (auto const& e : attestations_)
        r.emplace_back(e.toSTObject());
    return r;
}

template <class TAttestation>
typename XChainAttestationsBase<TAttestation>::OnNewAttestationResult
XChainAttestationsBase<TAttestation>::onNewAttestations(
    ReadView const& view,
    typename TAttestation::TBatchAttestation const* attBegin,
    typename TAttestation::TBatchAttestation const* attEnd,
    std::uint32_t quorum,
    std::unordered_map<AccountID, std::uint32_t> const& signersList,
    beast::Journal j)
{
    if (attBegin == attEnd)
        return {};

    bool changed = false;
    for (auto att = attBegin; att != attEnd; ++att)
    {
        if (Attestations::checkAttestationPublicKey(
                view,
                signersList,
                att->attestationSignerAccount,
                att->publicKey,
                j) != tesSUCCESS)
        {
            // The checkAttestationPublicKey is not strictly nessisary here (it
            // should be checked in a preclaim step), but it would be bad to let
            // on slip through if that changes, and the check is relatively
            // cheap, so we check again
            continue;
        }

        auto const& claimSigningAccount = att->attestationSignerAccount;
        if (auto i = std::find_if(
                attestations_.begin(),
                attestations_.end(),
                [&](auto const& a) {
                    return a.keyAccount == claimSigningAccount;
                });
            i != attestations_.end())
        {
            // existing attestation
            // replace old attestation with new attestion
            *i = TAttestation{*att};
            changed = true;
        }
        else
        {
            attestations_.emplace_back(*att);
            changed = true;
        }
    }

    auto r = claimHelper(
        view,
        typename TAttestation::MatchFields{*attBegin},
        CheckDst::check,
        quorum,
        signersList,
        j);

    if (!r.has_value())
        return {std::nullopt, changed};

    return {std::move(r.value()), changed};
};

template <class TAttestation>
Expected<std::vector<AccountID>, TER>
XChainAttestationsBase<TAttestation>::claimHelper(
    ReadView const& view,
    typename TAttestation::MatchFields const& toMatch,
    CheckDst checkDst,
    std::uint32_t quorum,
    std::unordered_map<AccountID, std::uint32_t> const& signersList,
    beast::Journal j)
{
    {
        // Remove attestations that are valid signers. They may be no longer
        // part of the signers list, or their master key may have been disabled,
        // or their regular may have changed
        auto i = std::remove_if(
            attestations_.begin(), attestations_.end(), [&](auto const& a) {
                return Attestations::checkAttestationPublicKey(
                           view, signersList, a.keyAccount, a.publicKey, j) !=
                    tesSUCCESS;
            });
        attestations_.erase(i, attestations_.end());
    }

    // Check if we have quorum for the amount on specified on the new
    // claimAtt
    std::vector<AccountID> rewardAccounts;
    rewardAccounts.reserve(attestations_.size());
    std::uint32_t weight = 0;
    for (auto const& a : attestations_)
    {
        auto const matchR = a.match(toMatch);
        // The dest must match if claimHelper is being run as a result of an add
        // attestation transaction. The dst does not need to match if the
        // claimHelper is being run using an explicit claim transaction.
        if (matchR == AttestationMatch::nonDstMatch ||
            (checkDst == CheckDst::check && matchR != AttestationMatch::match))
            continue;
        auto i = signersList.find(a.keyAccount);
        if (i == signersList.end())
        {
            assert(0);  // should have already been checked
            continue;
        }
        weight += i->second;
        rewardAccounts.push_back(a.rewardAccount);
    }

    if (weight >= quorum)
        return rewardAccounts;

    return Unexpected(tecXCHAIN_CLAIM_NO_QUORUM);
}

Expected<std::vector<AccountID>, TER>
XChainClaimAttestations::onClaim(
    ReadView const& view,
    STAmount const& sendingAmount,
    bool wasLockingChainSend,
    std::uint32_t quorum,
    std::unordered_map<AccountID, std::uint32_t> const& signersList,
    beast::Journal j)
{
    XChainClaimAttestation::MatchFields toMatch{
        sendingAmount, wasLockingChainSend, std::nullopt};
    return claimHelper(view, toMatch, CheckDst::ignore, quorum, signersList, j);
}

template class XChainAttestationsBase<XChainClaimAttestation>;
template class XChainAttestationsBase<XChainCreateAccountAttestation>;

}  // namespace ripple
