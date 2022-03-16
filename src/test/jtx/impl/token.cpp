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

#include <test/jtx/flags.h>
#include <test/jtx/token.h>

#include <ripple/app/tx/impl/NFTokenMint.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/jss.h>

namespace ripple {
namespace test {
namespace jtx {
namespace token {

Json::Value
mint(jtx::Account const& account, std::uint32_t tokenTaxon)
{
    Json::Value jv;
    jv[sfAccount.jsonName] = account.human();
    jv[sfTokenTaxon.jsonName] = tokenTaxon;
    jv[sfTransactionType.jsonName] = jss::NFTokenMint;
    return jv;
}

void
xferFee::operator()(Env& env, JTx& jt) const
{
    jt.jv[sfTransferFee.jsonName] = xferFee_;
}

void
issuer::operator()(Env& env, JTx& jt) const
{
    jt.jv[sfIssuer.jsonName] = issuer_;
}

void
uri::operator()(Env& env, JTx& jt) const
{
    jt.jv[sfURI.jsonName] = uri_;
}

uint256
getNextID(
    jtx::Env const& env,
    jtx::Account const& issuer,
    std::uint32_t tokenTaxon,
    std::uint16_t flags,
    std::uint16_t xferFee)
{
    // Get the nftSeq from the account root of the issuer.
    std::uint32_t const nftSeq = {
        env.le(issuer)->at(~sfMintedTokens).value_or(0)};
    return getID(issuer, tokenTaxon, nftSeq, flags, xferFee);
}

uint256
getID(
    jtx::Account const& issuer,
    std::uint32_t tokenTaxon,
    std::uint32_t nftSeq,
    std::uint16_t flags,
    std::uint16_t xferFee)
{
    return ripple::NFTokenMint::createTokenID(
        flags, xferFee, issuer, nft::toTaxon(tokenTaxon), nftSeq);
}

Json::Value
burn(jtx::Account const& account, uint256 const& tokenID)
{
    Json::Value jv;
    jv[sfAccount.jsonName] = account.human();
    jv[sfTokenID.jsonName] = to_string(tokenID);
    jv[jss::TransactionType] = jss::NFTokenBurn;
    return jv;
}

Json::Value
createOffer(
    jtx::Account const& account,
    uint256 const& tokenID,
    STAmount const& amount)
{
    Json::Value jv;
    jv[sfAccount.jsonName] = account.human();
    jv[sfTokenID.jsonName] = to_string(tokenID);
    jv[sfAmount.jsonName] = amount.getJson(JsonOptions::none);
    jv[jss::TransactionType] = jss::NFTokenCreateOffer;
    return jv;
}

void
owner::operator()(Env& env, JTx& jt) const
{
    jt.jv[sfOwner.jsonName] = owner_;
}

void
expiration::operator()(Env& env, JTx& jt) const
{
    jt.jv[sfExpiration.jsonName] = expires_;
}

void
destination::operator()(Env& env, JTx& jt) const
{
    jt.jv[sfDestination.jsonName] = dest_;
}

template <typename T>
static Json::Value
cancelOfferImpl(jtx::Account const& account, T const& tokenOffers)
{
    Json::Value jv;
    jv[sfAccount.jsonName] = account.human();
    if (!empty(tokenOffers))
    {
        jv[sfTokenOffers.jsonName] = Json::arrayValue;
        for (uint256 const& tokenOffer : tokenOffers)
            jv[sfTokenOffers.jsonName].append(to_string(tokenOffer));
    }
    jv[jss::TransactionType] = jss::NFTokenCancelOffer;
    return jv;
}

Json::Value
cancelOffer(
    jtx::Account const& account,
    std::initializer_list<uint256> const& tokenOffers)
{
    return cancelOfferImpl(account, tokenOffers);
}

Json::Value
cancelOffer(
    jtx::Account const& account,
    std::vector<uint256> const& tokenOffers)
{
    return cancelOfferImpl(account, tokenOffers);
}

void
rootIndex::operator()(Env& env, JTx& jt) const
{
    jt.jv[sfRootIndex.jsonName] = rootIndex_;
}

Json::Value
acceptBuyOffer(jtx::Account const& account, uint256 const& offerIndex)
{
    Json::Value jv;
    jv[sfAccount.jsonName] = account.human();
    jv[sfBuyOffer.jsonName] = to_string(offerIndex);
    jv[jss::TransactionType] = jss::NFTokenAcceptOffer;
    return jv;
}

Json::Value
acceptSellOffer(jtx::Account const& account, uint256 const& offerIndex)
{
    Json::Value jv;
    jv[sfAccount.jsonName] = account.human();
    jv[sfSellOffer.jsonName] = to_string(offerIndex);
    jv[jss::TransactionType] = jss::NFTokenAcceptOffer;
    return jv;
}

Json::Value
brokerOffers(
    jtx::Account const& account,
    uint256 const& buyOfferIndex,
    uint256 const& sellOfferIndex)
{
    Json::Value jv;
    jv[sfAccount.jsonName] = account.human();
    jv[sfBuyOffer.jsonName] = to_string(buyOfferIndex);
    jv[sfSellOffer.jsonName] = to_string(sellOfferIndex);
    jv[jss::TransactionType] = jss::NFTokenAcceptOffer;
    return jv;
}

void
brokerFee::operator()(Env& env, JTx& jt) const
{
    jt.jv[sfBrokerFee.jsonName] = brokerFee_.getJson(JsonOptions::none);
}

Json::Value
setMinter(jtx::Account const& account, jtx::Account const& minter)
{
    Json::Value jt = fset(account, asfAuthorizedMinter);
    jt[sfMinter.fieldName] = minter.human();
    return jt;
}

Json::Value
clearMinter(jtx::Account const& account)
{
    return fclear(account, asfAuthorizedMinter);
}

}  // namespace token
}  // namespace jtx
}  // namespace test
}  // namespace ripple
