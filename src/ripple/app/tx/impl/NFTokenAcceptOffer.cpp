//------------------------------------------------------------------------------
/*
  This file is part of rippled: https://github.com/ripple/rippled
  Copyright (c) 2021 Ripple Labs Inc.

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

#include <ripple/app/tx/impl/NFTokenAcceptOffer.h>
#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/ledger/View.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Rate.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/st.h>

namespace ripple {

NotTEC
NFTokenAcceptOffer::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureNonFungibleTokensV1))
        return temDISABLED;

    if (auto const ret = preflight1(ctx); !isTesSuccess(ret))
        return ret;

    if (ctx.tx.getFlags() & tfNFTokenAcceptOfferMask)
        return temINVALID_FLAG;

    auto const bo = ctx.tx[~sfBuyOffer];
    auto const so = ctx.tx[~sfSellOffer];

    // At least one of these MUST be specified
    if (!bo && !so)
        return temMALFORMED;

    // The `BrokerFee` field must not be present in direct mode but may be
    // present and greater than zero in brokered mode.
    if (auto const bf = ctx.tx[~sfBrokerFee])
    {
        if (!bo || !so)
            return temMALFORMED;

        if (*bf <= beast::zero)
            return temMALFORMED;
    }

    return preflight2(ctx);
}

TER
NFTokenAcceptOffer::preclaim(PreclaimContext const& ctx)
{
    auto const checkOffer = [&ctx](std::optional<uint256> id) -> TER {
        if (id)
        {
            auto const offer = ctx.view.read(keylet::nftoffer(*id));

            if (!offer)
                return tecOBJECT_NOT_FOUND;

            if (hasExpired(ctx.view, (*offer)[~sfExpiration]))
                return tecEXPIRED;
        }

        return tesSUCCESS;
    };

    auto const buy = ctx.tx[~sfBuyOffer];
    auto const sell = ctx.tx[~sfSellOffer];

    if (auto const ret = checkOffer(buy); !isTesSuccess(ret))
        return ret;

    if (auto const ret = checkOffer(sell); !isTesSuccess(ret))
        return ret;

    if (buy && sell)
    {  // Brokered mode:
        auto const bo = ctx.view.read(keylet::nftoffer(*buy));
        auto const so = ctx.view.read(keylet::nftoffer(*sell));

        // The two offers being brokered must be for the same token:
        if ((*bo)[sfTokenID] != (*so)[sfTokenID])
            return tecBUY_SELL_MISMATCH;

        // The two offers being brokered must be for the same asset:
        if ((*bo)[sfAmount].issue() != (*so)[sfAmount].issue())
            return tecBUY_SELL_MISMATCH;

        // Ensure that the buyer is willing to pay at least as much as the
        // seller is requesting:
        if ((*so)[sfAmount] > (*bo)[sfAmount])
            return tecINSUFFICIENT_PAYMENT;

        // If the seller specified a destination, that destination must be
        // the buyer.
        if (auto const dest = so->at(~sfDestination))
        {
            if (*dest != bo->at(sfOwner))
                return tecBUY_SELL_MISMATCH;
        }

        // The broker can specify an amount that represents their cut; if they
        // have, ensure that the seller will get at least as much as they want
        // to get *after* this fee is accounted for (but before the issuer's
        // cut, if any).
        if (auto const brokerFee = ctx.tx[~sfBrokerFee])
        {
            if (brokerFee->issue() != (*bo)[sfAmount].issue())
                return tecBUY_SELL_MISMATCH;

            if (brokerFee >= (*bo)[sfAmount])
                return tecINSUFFICIENT_PAYMENT;

            if ((*so)[sfAmount] > (*bo)[sfAmount] - *brokerFee)
                return tecINSUFFICIENT_PAYMENT;
        }
    }

    if (buy)
    {
        auto const bo = ctx.view.read(keylet::nftoffer(*buy));

        if (((*bo)[sfFlags] & lsfSellToken) == lsfSellToken)
            return tecOFFER_TYPE_MISMATCH;

        // An account can't accept an offer it placed:
        if ((*bo)[sfOwner] == ctx.tx[sfAccount])
            return tecCANT_ACCEPT_OWN_OFFER;

        // If not in bridged mode, the account must own the token:
        if (!sell &&
            !nft::findToken(ctx.view, ctx.tx[sfAccount], (*bo)[sfTokenID]))
            return tecNO_PERMISSION;

        // The account offering to buy must have funds:
        auto const needed = bo->at(sfAmount);

        if (accountHolds(
                ctx.view,
                (*bo)[sfOwner],
                needed.getCurrency(),
                needed.getIssuer(),
                fhZERO_IF_FROZEN,
                ctx.j) < needed)
            return tecINSUFFICIENT_FUNDS;
    }

    if (sell)
    {
        auto const so = ctx.view.read(keylet::nftoffer(*sell));

        if (((*so)[sfFlags] & lsfSellToken) != lsfSellToken)
            return tecOFFER_TYPE_MISMATCH;

        // An account can't accept an offer it placed:
        if ((*so)[sfOwner] == ctx.tx[sfAccount])
            return tecCANT_ACCEPT_OWN_OFFER;

        // The seller must own the token.
        if (!nft::findToken(ctx.view, (*so)[sfOwner], (*so)[sfTokenID]))
            return tecNO_PERMISSION;

        // If not in bridged mode...
        if (!buy)
        {
            // If the offer has a Destination field, the acceptor must be the
            // Destination.
            if (auto const dest = so->at(~sfDestination);
                dest.has_value() && *dest != ctx.tx[sfAccount])
                return tecNO_PERMISSION;
        }

        // The account offering to buy must have funds:
        auto const needed = so->at(sfAmount);

        if (accountHolds(
                ctx.view,
                ctx.tx[sfAccount],
                needed.getCurrency(),
                needed.getIssuer(),
                fhZERO_IF_FROZEN,
                ctx.j) < needed)
            return tecINSUFFICIENT_FUNDS;
    }

    return tesSUCCESS;
}

TER
NFTokenAcceptOffer::pay(
    AccountID const& from,
    AccountID const& to,
    STAmount amount)
{
    // This should never happen, but it's easy and quick to check.
    if (amount < beast::zero)
        return tecINTERNAL;

    return accountSend(view(), from, to, amount, j_);
}

TER
NFTokenAcceptOffer::acceptOffer(std::shared_ptr<SLE> const& offer)
{
    bool const isSell = offer->isFlag(lsfSellToken);
    AccountID const owner = (*offer)[sfOwner];
    AccountID const& seller = isSell ? owner : account_;
    AccountID const& buyer = isSell ? account_ : owner;

    auto const tokenID = (*offer)[sfTokenID];

    if (auto amount = offer->getFieldAmount(sfAmount); amount != beast::zero)
    {
        // Calculate the issuer's cut from this sale, if any:
        if (auto const fee = nft::getTransferFee(tokenID); fee != 0)
        {
            auto const cut = multiply(amount, transferFeeAsRate(fee));

            if (auto const issuer = nft::getIssuer(tokenID);
                cut != beast::zero && seller != issuer && buyer != issuer)
            {
                if (auto const r = pay(buyer, issuer, cut); !isTesSuccess(r))
                    return r;
                amount -= cut;
            }
        }

        // Send the remaining funds to the seller of the NFT
        if (auto const r = pay(buyer, seller, amount); !isTesSuccess(r))
            return r;
    }

    // Now transfer the NFT:
    auto nft = nft::findToken(view(), seller, tokenID);

    if (!nft)
        return tecINTERNAL;

    if (auto const ret = nft::removeToken(view(), seller, tokenID);
        !isTesSuccess(ret))
        return ret;

    return nft::insertToken(view(), buyer, *nft);
}

TER
NFTokenAcceptOffer::doApply()
{
    auto const loadToken = [this](std::optional<uint256> id) {
        std::shared_ptr<SLE> sle;
        if (id)
            sle = view().peek(keylet::nftoffer(*id));
        return sle;
    };

    auto bo = loadToken(ctx_.tx[~sfBuyOffer]);
    auto so = loadToken(ctx_.tx[~sfSellOffer]);

    if (bo && !nft::deleteTokenOffer(view(), bo))
    {
        JLOG(j_.fatal()) << "Unable to delete buy offer '"
                         << to_string(bo->key()) << "': ignoring";
        return tecINTERNAL;
    }

    if (so && !nft::deleteTokenOffer(view(), so))
    {
        JLOG(j_.fatal()) << "Unable to delete sell offer '"
                         << to_string(so->key()) << "': ignoring";
        return tecINTERNAL;
    }

    // Bridging two different offers
    if (bo && so)
    {
        AccountID const buyer = (*bo)[sfOwner];
        AccountID const seller = (*so)[sfOwner];

        auto const tokenID = (*so)[sfTokenID];

        // The amount is what the buyer of the NFT pays:
        STAmount amount = (*bo)[sfAmount];

        // Three different folks may be paid.  The order of operations is
        // important.
        //
        // o The broker is paid the cut they requested.
        // o The issuer's cut is calculated from what remains after the
        //   broker is paid.  The issuer can take up to 50% of the remainder.
        // o Finally, the seller gets whatever is left.
        //
        // It is important that the issuer's cut be calculated after the
        // broker's portion is already removed.  Calculating the issuer's
        // cut before the broker's cut it removed can result in more money
        // being paid out than the seller authorized.  That would be bad!

        // Send the broker the amount they requested
        if (auto const cut = ctx_.tx[~sfBrokerFee];
            cut && cut.value() != beast::zero)
        {
            if (auto const r = pay(buyer, account_, cut.value());
                !isTesSuccess(r))
                return r;

            amount -= cut.value();
        }

        // Calculate the issuer's cut, if any:
        if (auto const fee = nft::getTransferFee(tokenID);
            amount != beast::zero && fee != 0)
        {
            auto cut = multiply(amount, transferFeeAsRate(fee));

            if (auto const issuer = nft::getIssuer(tokenID);
                seller != issuer && buyer != issuer)
            {
                if (auto const r = pay(buyer, issuer, cut); !isTesSuccess(r))
                    return r;

                amount -= cut;
            }
        }

        // And send whatever remains to the seller
        if (amount > beast::zero)
        {
            if (auto const r = pay(buyer, seller, amount); !isTesSuccess(r))
                return r;
        }

        auto token = nft::findToken(view(), seller, tokenID);

        if (!token)
            return tecINTERNAL;

        if (auto const ret = nft::removeToken(view(), seller, tokenID);
            !isTesSuccess(ret))
            return ret;

        return nft::insertToken(view(), buyer, *token);
    }

    if (bo)
        return acceptOffer(bo);

    if (so)
        return acceptOffer(so);

    return tecINTERNAL;
}

}  // namespace ripple
