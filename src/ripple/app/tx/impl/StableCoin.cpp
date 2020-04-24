//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2020

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

#include <ripple/app/tx/impl/StableCoin.h>

#include <ripple/basics/XRPAmount.h>
#include <ripple/basics/mulDiv.h>
#include <ripple/ledger/View.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Quality.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/TxFlags.h>

#include <algorithm>

namespace ripple {

/*
    Stable Coin

        TBD: Description

        ## Create Stable Coin

        ### Parameters

        * `Asset Type`
        * `Issuance collateral ratio`
        * `Liquidation threshold`
        * `Price oracles`
        * `Loan origination fee`
        * `CDP deposit fee`
        * `Liquidation penalty`
        * `Locked`

        ### Success Postcondition

        * `Stable Coin` ledger object added
        * Stable Coin ID set to hash of (ST prefix, OwnerID, AssetType, SeqNum)
        * This stable coin is added to each of the price oracle's `Users` list
        * Usual account owner accounting
        * Following set to zero:
        * `Issued coins`
        * `Total CDP`
        * `Stability pool balance`
        * Following set to parameter values:
        * `Asset Type`
        * `Issuance collateral ratio`
        * `Liquidation threshold`
        * `Price oracles`
        * `Loan origination fee`
        * `CDP deposit fee`
        * `Liquidation penalty`
        * `Locked`

        ### Failures

        * Any of the following out of range
        * `Issuance collateral ratio`
        * `Liquidation threshold`
        * `Loan origination fee`
        * `CDP deposit fee`
        * `Liquidation penalty`
        * Price oracle does not exist

        ## Create Oracle

        ### Parameters

        * `Asset type`
        * `Public key` (Used to verify oracle updates)
        * `External Oracle ID` (optional: ID that will be part of the signature,
   if different from the ripple Oracle ID)

        ### Success Postcondition

        * `Oracle` ledger object created
        * `Oracle ID` set to hash of (O prefix, OwnerID, AssetType, SeqNum)
        * `Public Key` is set to the parameter value
        * `Asset Type` is set to the parameter value
        * `External Oracle ID` is set to the parameter value
        * Users list set to empty
        * The following fields remain unset:
        * `Asset Count`
        * `XRP Drops Count`
        * `Valid Start Time`
        * `Valid End Time`
        * Usual account owner accounting

        ### Failures

        * Invalid public key

        ### Notes

        * Once an oracle is created, it's values can not be changed. If an
   oracle is compromised, its values can be marked as "never valid" through an
   `Update Oracle` transaction.

        ## Update Oracle

        Any account may submit an `Update Oracle` transaction with a signed
   blob. Note that even the account that owns the oracle must submit a signed
   blob.

        ### Parameters

        * `Schema ID`
        * `Public Key`
        * `Ripple Oracle ID`
        * `Signature for blob`
        * `Blob`

        The `Schema ID` is used to support multiple blob formats that may be
   used by external oracles. The blob must contain an `External Oracle ID`
   field. However, the external oracle may refer to the value with a different
   ID than is used my the ledger. If so, this value is set when the oracle is
   created.

        The `Blob` parameter will be interpreted according to the schema ID, but
   must contain the following information:
        * `External Oracle ID` (may be different from `Ripple Oracle ID`)
        * `Asset Count`
        * `XRP Drops Count`
        * `Valid End Time`

        The `Blob` may also contain the `Valid Start Time`. If it does not, the
   valid start time is set to the last ledger close time.

        ### Success Postcondition

        * If the `Valid End Time` parameter is greater than the currently stored
   `Valid End Time`, then the following are set to the parameter values
           * `Asset Count`
           * `XRP Drops Count`
           * `Valid Start Time`
           * `Valid End Time`

        ### Failures

        * Oracle does not exist
        * The public key does not match the public key stored on the ledger.
        * `External Oracle ID` from blob does not match stored `External Oracle
   ID` (if present) or `Oracle ID` if `External Oracle ID` is null.
        * `Valid End Time` is in the past
        * The `Valid End Time` parameter is less than or equal to than the
   currently stored `Valid End Time`. A null `Valid End Time` is defined to be
   less than any non-null time.
        * Invalid blob format

        ### Notes

        * Updating an oracle value does not update the stable coins that depend
   on it

*/

//------------------------------------------------------------------------------

// This function does not update the sles in the view. The caller is expected to
// do this.
[[nodiscard]] static TER
updateCDPAssetRatio(SLE& scSLE, SLE const& cdpSLE, uint256 const& cdpKey)
{
    STVector256 const& cdps = scSLE.getFieldV256(sfCDPs);
    STVector64 rates = scSLE.getFieldV64(sfCDPAssetRatios);

    auto const it = std::find_if(
        cdps.begin(), cdps.end(), [&](auto const& h) { return h == cdpKey; });
    if (it == cdps.end())
    {
        // Logic error. cdp should always be part of the sc's cdps.
        assert(0);
        return tefBAD_LEDGER;
    }
    std::uint64_t const newRatio = [&] {
        // calculate the new ratio
        // Zero debt is treated specially
        decltype(sfIssuedCoins)::type::value_type const numCoins =
            cdpSLE[sfIssuedCoins];
        if (numCoins == 0)
            return std::numeric_limits<std::uint64_t>::max();
        STAmount const balance = cdpSLE[sfBalance];
        // rate is balance/numCoins
        return getRate(STAmount{numCoins}, balance);
    }();
    auto index = std::distance(cdps.begin(), it);
    rates[index] = newRatio;
    scSLE.setFieldV64(sfCDPAssetRatios, rates);
    return tesSUCCESS;
}

[[nodiscard]] static TER
checkReserve(
    ReadView const& view,
    STAmount const& balance,
    std::uint32_t ownerCount,
    boost::optional<STAmount> const& amt = boost::none)
{
    // Check reserve and funds availability
    auto const reserve = view.fees().accountReserve(ownerCount);

    if (balance < reserve)
        return tecINSUFFICIENT_RESERVE;

    if (amt && (balance < reserve + *amt))
        return tecUNFUNDED;
    return tesSUCCESS;
}

[[nodiscard]] static TER
checkValidOracle(SLE const& oSLE, std::uint32_t closeTime)
{
    auto const validAfter = oSLE[~sfValidAfter];
    auto const expiration = oSLE[~sfExpiration];
    auto const xrpValue = oSLE[~sfOracleXRPValue];
    auto const assetCount = oSLE[~sfOracleAssetCount];

    if (!(validAfter && expiration && xrpValue && assetCount))
        return TER{tecNO_ORACLE_VALUE};

    if (*validAfter > closeTime || *expiration < closeTime)
        return TER{tecNO_ORACLE_VALUE};

    return tesSUCCESS;
}

// Only use this function if the oracle has already been checked for validity
// (see `checkValidOracle`). The `checkValidOracle` can be called outside of a
// loop once, while this function can be used to calculate coin values for
// multipleCDPs in a loop.
[[nodiscard]] static XRPAmount
uncheckedCoinValue(
    std::uint32_t numCoins,
    XRPAmount const& xrpValue,
    std::uint32_t assetCount,
    bool roundUp)
{
    return mulRatio(xrpValue, numCoins, assetCount, roundUp);
}

[[nodiscard]] static std::pair<XRPAmount, TER>
coinValue(
    SLE const& oSLE,
    std::uint32_t numCoins,
    std::uint32_t closeTime,
    bool roundUp)
{
    if (auto const ter = checkValidOracle(oSLE, closeTime); ter != tesSUCCESS)
        return {XRPAmount{}, ter};

    auto const xrpValue = oSLE[sfOracleXRPValue];
    auto const assetCount = oSLE[sfOracleAssetCount];

    return {
        uncheckedCoinValue(numCoins, xrpValue.xrp(), assetCount, roundUp),
        tesSUCCESS};
}

[[nodiscard]] static TER
checkCollateralRatio(
    SLE const& oSLE,
    STAmount const& collateralValue,
    std::uint32_t colRatioThresh,
    std::uint32_t issuedCoins,
    std::uint32_t closeTime)
{
    if (auto const [debtAmt, ter] =
            coinValue(oSLE, issuedCoins, closeTime, /*round up*/ true);
        ter == tesSUCCESS && debtAmt)
    {
        auto [validMul, colRatio] =
            mulDiv(collateralValue.mantissa(), QUALITY_ONE, debtAmt.drops());
        // we are obviously over the issRatio on overflow
        if (validMul && colRatio < colRatioThresh)
            return tecSTABLECOIN_ISSUANCE_RATIO;
    }
    return tesSUCCESS;
}

// This code assumes `checkReserve` has succeeded
[[nodiscard]] static TER
cdpDeposit(SLE& accSLE, SLE& scSLE, SLE& cdpSLE, XRPAmount const& xrpAmt)
{
    // Calculate the fee
    std::uint32_t const depositFee = scSLE[sfDepositFee];
    XRPAmount const fee =
        mulRatio(xrpAmt, depositFee, QUALITY_ONE, /*roundUp*/ false);
    auto const bal = accSLE[sfBalance];
    // xrpAmt >= bal should already be checked in `checkReserve`; Check anyway.
    if (xrpAmt >= bal || fee >= xrpAmt)
    {
        return tecUNFUNDED_CDP_DEPOSIT;
    }

    accSLE[sfBalance] = bal - xrpAmt;
    auto const toDeposit = xrpAmt - fee;
    cdpSLE[sfBalance] = cdpSLE[sfBalance] + toDeposit;
    scSLE[sfCDPBalance] = scSLE[sfCDPBalance] + toDeposit;
    scSLE[sfStabilityPoolBalance] = scSLE[sfStabilityPoolBalance] + fee;
    return tesSUCCESS;
}

//------------------------------------------------------------------------------

// Iterate through CDPs in asset ratio order. This iterator will be used for
// prototype code only. There is no reason to bullet proof it or make it
// standard's compliant.
class CDPIter
{
    std::size_t curIndex_ = std::numeric_limits<std::size_t>::max();
    std::vector<uint256> cdps_;
    std::vector<std::uint64_t> rates_;
    // order the cdps should be redeemed against.
    // This is from lowest to highest rates, with the exception that the
    // redeemer's CDP may optionally be redeemed against first.
    std::vector<std::size_t> sortOrder_;

public:
    CDPIter() = default;
    // If redeemerCDP is specified, coins will be redeemed against this CDP
    // before the other CDPs
    CDPIter(
        SLE const& scSLE,
        boost::optional<uint256> const& redeemerCDP = boost::none);
    void
    operator++();
    uint256 const&
    operator*() const;
    friend bool
    operator==(CDPIter const& lhs, CDPIter const& rhs)
    {
        // While checking the cdp's for equality looks inefficient, most of the
        // time iterators are compared with end iterators, which have empty cdp
        // arrays.
        return lhs.curIndex_ == rhs.curIndex_ && lhs.cdps_ == rhs.cdps_;
    }
    friend bool
    operator!=(CDPIter const& lhs, CDPIter const& rhs)
    {
        return !operator==(lhs, rhs);
    }
};

CDPIter::CDPIter(SLE const& scSLE, boost::optional<uint256> const& redeemerCDP)
    : curIndex_{0}
    , cdps_{scSLE.getFieldV256(sfCDPs).value()}
    , rates_{scSLE.getFieldV64(sfCDPAssetRatios).value()}
{
    sortOrder_.resize(cdps_.size());
    std::iota(sortOrder_.begin(), sortOrder_.end(), 0);
    std::sort(
        sortOrder_.begin(),
        sortOrder_.end(),
        [&](std::size_t lhs, std::size_t rhs) {
            return rates_[lhs] < rates_[rhs];
        });

    if (!redeemerCDP)
        return;

    // return the index of the element in the collection or boost::none if the
    // element is not in the collection
    auto findIndex = [](auto const& col,
                        auto const& elem) -> boost::optional<std::size_t> {
        auto const it = std::find(col.begin(), col.end(), elem);
        if (it == col.end())
            return boost::none;
        return std::distance(col.begin(), it);
    };

    auto const redeemerCDPIndex = findIndex(cdps_, *redeemerCDP);
    if (!redeemerCDPIndex)
        return;
    auto const it =
        std::find(sortOrder_.begin(), sortOrder_.end(), *redeemerCDPIndex);
    if (it == sortOrder_.end())
    {
        // should always be part of the sort order
        assert(0);
        return;
    }

    // Move the redeemer's CDP to the front of the sort order
    std::rotate(sortOrder_.begin(), it, it + 1);
}

void
CDPIter::operator++()
{
    assert(curIndex_ != std::numeric_limits<std::size_t>::max());
    ++curIndex_;
    if (curIndex_ >= cdps_.size())
    {
        curIndex_ = std::numeric_limits<std::size_t>::max();
        cdps_.clear();
        rates_.clear();
        sortOrder_.clear();
    }
}

uint256 const&
CDPIter::operator*() const
{
    assert(curIndex_ < cdps_.size());
    return cdps_[sortOrder_[curIndex_]];
}

//------------------------------------------------------------------------------

NotTEC
StableCoinCreate::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureStableCoin))
        return temDISABLED;

    if (ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;


    if (isXRP(ctx.tx[sfAssetType]))
    {
        return temBAD_CURRENCY;
    }

    // TBD: Decide on valid ranges for these values
    constexpr std::uint32_t ratioOne = 1'000'000'000;
    constexpr std::uint32_t uint32Max =
        std::numeric_limits<std::uint32_t>::max();
    {
        constexpr std::uint32_t minIssRatio = ratioOne;
        constexpr std::uint32_t maxIssRatio = uint32Max;
        auto const v = ctx.tx[sfIssuanceRatio];
        if (v > maxIssRatio || v < minIssRatio)
            return temBAD_STABLECOIN_ISSUANCE_RATIO;
    }
    {
        constexpr std::uint32_t minLqdRatio = ratioOne;
        constexpr std::uint32_t maxLqdRatio = uint32Max;
        auto const v = ctx.tx[sfLiquidationRatio];
        if (v > maxLqdRatio || v < minLqdRatio)
            return temBAD_STABLECOIN_LIQUIDATION_RATIO;

        if (ctx.tx[sfIssuanceRatio] <= ctx.tx[sfLiquidationRatio])
            return temBAD_STABLECOIN_LIQUIDATION_RATIO;
    }
    {
        constexpr std::uint32_t minLoanOrgFee = 0;
        constexpr std::uint32_t maxLoanOrgFee = ratioOne;
        auto const v = ctx.tx[sfLoanOriginationFee];
        if (v > maxLoanOrgFee || v < minLoanOrgFee)
            return temBAD_STABLECOIN_LOAN_ORG_FEE;
    }
    {
        constexpr std::uint32_t minDepositFee = 0;
        constexpr std::uint32_t maxDepositFee = ratioOne;
        auto const v = ctx.tx[sfDepositFee];
        if (v > maxDepositFee || v < minDepositFee)
            return temBAD_STABLECOIN_DEPOSIT_FEE;
    }
    {
        constexpr std::uint32_t minLqdPenalty = 0;
        constexpr std::uint32_t maxLqdPenalty = ratioOne;
        auto const v = ctx.tx[sfLiquidationPenalty];
        if (v > maxLqdPenalty || v < minLqdPenalty)
            return temBAD_STABLECOIN_LIQUIDATION_PENALTY;
    }

    return preflight2(ctx);
}

TER
StableCoinCreate::preclaim(PreclaimContext const& ctx)
{
    auto const account = ctx.tx[sfAccount];
    auto const accSLE = ctx.view.read(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    // Ledger object will be added: +1 owner count when checking reserve
    if (auto const ter = checkReserve(
            ctx.view, (*accSLE)[sfBalance], (*accSLE)[sfOwnerCount] + 1);
        ter != tesSUCCESS)
        return ter;

    if (auto const oracleSLE =
            ctx.view.read(Keylet{ltORACLE, ctx.tx[sfOracleID]}))
    {
        if (ctx.tx[sfAssetType] != (*oracleSLE)[sfAssetType])
            return tecORACLE_ASSET_MISMATCH;
    }
    else
    {
        // Oracle must already exist
        return tecNO_ENTRY;
    }

    if (ctx.view.read(
            keylet::stableCoin(ctx.tx[sfAccount], ctx.tx[sfAssetType])))
    {
        // An account can only have one stable coin per asset type It may make
        // sense to allow multiple stable coins, as the parameters may differ.
        // For now we'll limit it to one.
        return tecDUPLICATE;
    }

    return tesSUCCESS;
}

TER
StableCoinCreate::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const accSLE = ctx_.view().peek(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    auto const oracleSLE =
        ctx_.view().peek(Keylet(ltORACLE, ctx_.tx[sfOracleID]));
    if (!oracleSLE)
        return tecNO_ENTRY;

    auto const assetType = ctx_.tx[sfAssetType];

    auto const scKeylet = keylet::stableCoin(account, assetType);
    auto const scSLE = std::make_shared<SLE>(scKeylet);
    {
        auto& sc = *scSLE;
        sc[sfAssetType] = ctx_.tx[sfAssetType];
        sc[sfIssuanceRatio] = ctx_.tx[sfIssuanceRatio];
        sc[sfLiquidationRatio] = ctx_.tx[sfLiquidationRatio];
        sc[sfOracleID] = ctx_.tx[sfOracleID];
        sc[sfLoanOriginationFee] = ctx_.tx[sfLoanOriginationFee];
        sc[sfDepositFee] = ctx_.tx[sfDepositFee];
        sc[sfLiquidationPenalty] = ctx_.tx[sfLiquidationPenalty];
        sc[sfIssuedCoins] = std::uint32_t{0};
        sc[sfCDPBalance] = STAmount{};
        sc.setFieldV256(sfCDPs, STVector256{});
        sc.setFieldV64(sfCDPAssetRatios, STVector64{});
    }

    {
        // This is not effiecent, but will only be used for the prototype
        STVector256 ou = oracleSLE->getFieldV256(sfOracleUsers);
        ou.push_back(scKeylet.key);
        oracleSLE->setFieldV256(sfOracleUsers, ou);
    }

    // Add to owner directory
    if (auto const page = dirAdd(
            ctx_.view(),
            keylet::ownerDir(account),
            scSLE->key(),
            false,
            describeOwnerDir(account),
            ctx_.app.journal("View")))
    {
        (*scSLE)[sfOwnerNode] = *page;
    }
    else
    {
        return tecDIR_FULL;
    }

    adjustOwnerCount(ctx_.view(), accSLE, 1, ctx_.journal);
    ctx_.view().insert(scSLE);
    ctx_.view().update(oracleSLE);

    return tesSUCCESS;
}

//------------------------------------------------------------------------------

NotTEC
StableCoinDelete::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureStableCoin))
        return temDISABLED;

    if (ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    return preflight2(ctx);
}

TER
StableCoinDelete::preclaim(PreclaimContext const& ctx)
{
    auto const account = ctx.tx[sfAccount];
    auto const accSLE = ctx.view.read(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    auto const scSLE = ctx.view.read(
        keylet::stableCoin(ctx.tx[sfAccount], ctx.tx[sfAssetType]));
    if (!scSLE)
    {
        // An account can only have one stable coin per asset type It may make
        // sense to allow multiple stable coins, as the parameters may differ.
        // For now we'll limit it to one.
        return tecNO_ENTRY;
    }

    if (auto const cdps = scSLE->getFieldV256(sfCDPs); !cdps.empty())
    {
        return tecHAS_OBLIGATIONS;
    }

    if ((*scSLE)[sfCDPBalance].signum() != 0 || (*scSLE)[sfIssuedCoins] != 0)
    {
        // If it doesn't have any cdps shouldn't have any other obligations
        assert(0);
        return tefINTERNAL;
    }

    return tesSUCCESS;
}

TER
StableCoinDelete::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const accSLE = ctx_.view().peek(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    auto const scKey =
        keylet::stableCoin(ctx_.tx[sfAccount], ctx_.tx[sfAssetType]);
    auto const scSLE = ctx_.view().peek(scKey);
    if (!scSLE)
        return tecNO_ENTRY;

    {
        // Remove the stable coin from the oracle
        auto const oKey = keylet::unchecked((*scSLE)[sfOracleID]);
        auto const oSLE = ctx_.view().peek(oKey);
        if (!oSLE)
        {
            assert(0);
            return tefINTERNAL;
        }

        STVector256 ou = oSLE->getFieldV256(sfOracleUsers);
        auto const it = std::find(ou.begin(), ou.end(), scKey.key);
        if (it == ou.end())
        {
            assert(0);
            return tefINTERNAL;
        }
        // remove the element by replacing it with the last element and resizing
        // the vector
        *it = ou.back();
        ou.resize(ou.size() - 1);
        oSLE->setFieldV256(sfOracleUsers, ou);
        ctx_.view().update(oSLE);
    }

    {
        // Credit the stability pool balance to the owner
        STAmount const accBal = (*accSLE)[sfBalance];
        STAmount const poolBal = (*scSLE)[sfCDPBalance];
        // cdp will be removed, but set balance to zero anyway
        (*scSLE)[sfCDPBalance] = STAmount{};
        (*accSLE)[sfBalance] = accBal + poolBal;
    }

    {
        // Remove from owner directory
        auto const page = (*scSLE)[sfOwnerNode];
        if (!ctx_.view().dirRemove(
                keylet::ownerDir(account), page, scKey.key, true))
        {
            JLOG(ctx_.journal.fatal())
                << "Could not remove stable coin from owner directory";
            return tefBAD_LEDGER;
        }
        adjustOwnerCount(ctx_.view(), accSLE, -1, ctx_.journal);
    }

    ctx_.view().erase(scSLE);
    ctx_.view().update(accSLE);

    return tesSUCCESS;
}

//------------------------------------------------------------------------------

NotTEC
OracleCreate::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureStableCoin))
        return temDISABLED;

    if (ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    return preflight2(ctx);
}

TER
OracleCreate::preclaim(PreclaimContext const& ctx)
{
    auto const account = ctx.tx[sfAccount];
    auto const accSLE = ctx.view.read(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    if (ctx.view.read(keylet::oracle(account, ctx.tx[sfAssetType])))
        return tecDUPLICATE;

    // Ledger object will be added: +1 owner count when checking reserve
    return checkReserve(
        ctx.view, (*accSLE)[sfBalance], (*accSLE)[sfOwnerCount] + 1);
}

TER
OracleCreate::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const accSLE = ctx_.view().peek(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    auto const assetType = ctx_.tx[sfAssetType];

    auto const oracleSLE =
        std::make_shared<SLE>(keylet::oracle(account, assetType));
    (*oracleSLE)[sfAssetType] = assetType;
    oracleSLE->setFieldV256(sfOracleUsers, STVector256{});

    // Add to owner directory
    if (auto const page = dirAdd(
            ctx_.view(),
            keylet::ownerDir(account),
            oracleSLE->key(),
            false,
            describeOwnerDir(account),
            ctx_.app.journal("View")))
    {
        (*oracleSLE)[sfOwnerNode] = *page;
    }
    else
    {
        return tecDIR_FULL;
    }

    adjustOwnerCount(ctx_.view(), accSLE, 1, ctx_.journal);
    ctx_.view().insert(oracleSLE);

    return tesSUCCESS;
}

//------------------------------------------------------------------------------

NotTEC
OracleDelete::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureStableCoin))
        return temDISABLED;

    if (ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    return preflight2(ctx);
}

TER
OracleDelete::preclaim(PreclaimContext const& ctx)
{
    auto const account = ctx.tx[sfAccount];
    auto const accSLE = ctx.view.read(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    auto const oSLE =
        ctx.view.read(keylet::oracle(account, ctx.tx[sfAssetType]));
    if (!oSLE)
        return tecNO_ENTRY;

    if (auto const ou = (*oSLE)[~sfOracleUsers]; ou && ou->size() > 0)
        return tecHAS_OBLIGATIONS;

    return tesSUCCESS;
}

TER
OracleDelete::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const accSLE = ctx_.view().peek(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;
    auto const oKey = keylet::oracle(account, ctx_.tx[sfAssetType]);
    auto const oSLE = ctx_.view().peek(oKey);
    if (!oSLE)
        return tecNO_ENTRY;

    {
        // Remove from owner directory
        auto const page = (*oSLE)[sfOwnerNode];
        if (!ctx_.view().dirRemove(
                keylet::ownerDir(account), page, oKey.key, true))
        {
            JLOG(ctx_.journal.fatal())
                << "Could not remove oracle from owner directory";
            return tefBAD_LEDGER;
        }
        adjustOwnerCount(ctx_.view(), accSLE, -1, ctx_.journal);
    }

    ctx_.view().erase(oSLE);

    return tesSUCCESS;
}

//------------------------------------------------------------------------------

static bool
willDisableOracle(STTx const& tx)
{
    return tx[sfValidAfter] == std::numeric_limits<std::uint32_t>::max() &&
        tx[sfExpiration] == std::numeric_limits<std::uint32_t>::max();
}

NotTEC
OracleUpdate::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureStableCoin))
        return temDISABLED;

    if (ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    if (auto const amt = ctx.tx[sfOracleXRPValue];
        !isXRP(amt) || (amt <= beast::zero))
        return temBAD_AMOUNT;

    if (ctx.tx[sfOracleAssetCount] == 0)
        return temBAD_AMOUNT;

    if (ctx.tx[sfValidAfter] >= ctx.tx[sfExpiration] &&
        !willDisableOracle(ctx.tx))
    {
        return temBAD_EXPIRATION;
    }

    return preflight2(ctx);
}

TER
OracleUpdate::preclaim(PreclaimContext const& ctx)
{
    return tesSUCCESS;
}

TER
OracleUpdate::doApply()
{
    auto const oracleSLE =
        ctx_.view().peek(Keylet{ltORACLE, ctx_.tx[sfOracleID]});
    if (!oracleSLE)
        return tecNO_ENTRY;

    auto shouldReplace = [&]() -> bool {
        auto const closeTime =
            ctx_.view().info().parentCloseTime.time_since_epoch().count();

        // never replace a value with an expiration in the past
        if (ctx_.tx[sfExpiration] < closeTime)
            return false;

        auto const expOld = (*oracleSLE)[~sfExpiration];
        auto const vldOld = (*oracleSLE)[~sfValidAfter];

        // If the current values are both at their max values, the oracle will
        // never be valid
        if (expOld == std::numeric_limits<std::uint32_t>::max() &&
            vldOld == std::numeric_limits<std::uint32_t>::max())
            return false;

        if (willDisableOracle(ctx_.tx))
            return true;

        /*
          Check if the old value should be replaced with the new value
          Note: New value can't be in the past.
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
        */

        auto const inRangeOld =
            expOld && vldOld && *expOld >= closeTime && *vldOld <= closeTime;
        auto const inRangeNew = ctx_.tx[sfExpiration] >= closeTime &&
            ctx_.tx[sfValidAfter] <= closeTime;
        // If '<' was used instead of '<=', then it would be possibl to lock in
        // a value for a time slot. '<=' allows for replacement.
        auto const expGreaterNew = !expOld || *expOld <= ctx_.tx[sfExpiration];

        return (
            (!inRangeOld && inRangeNew) || (!inRangeOld && expGreaterNew) ||
            (inRangeNew && expGreaterNew));
    };

    if (!shouldReplace())
        return tecBAD_ORACLE_UPDATE;

    {
        auto& o = *oracleSLE;
        o[sfValidAfter] = ctx_.tx[sfValidAfter];
        o[sfExpiration] = ctx_.tx[sfExpiration];
        o[sfOracleXRPValue] = ctx_.tx[sfOracleXRPValue];
        o[sfOracleAssetCount] = ctx_.tx[sfOracleAssetCount];
    }

    ctx_.view().update(oracleSLE);

    return tesSUCCESS;
}

NotTEC
CDPCreate::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureStableCoin))
        return temDISABLED;

    if (ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    if (auto const amt = ctx.tx[~sfAmount];
        amt && (!isXRP(*amt) || (*amt <= beast::zero)))
        return temBAD_AMOUNT;

    return preflight2(ctx);
}

TER
CDPCreate::preclaim(PreclaimContext const& ctx)
{
    auto const account = ctx.tx[sfAccount];
    auto const accSLE = ctx.view.read(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    auto const scKeylet =
        keylet::stableCoin(ctx.tx[sfStableCoinOwner], ctx.tx[sfAssetType]);

    if (!ctx.view.read(scKeylet))
        return tecNO_ENTRY;

    auto const cdpKeylet = keylet::cdp(account, scKeylet.key);
    if (ctx.view.read(cdpKeylet))
        return tecDUPLICATE;

    // Ledger object will be added: +1 owner count when checking reserve
    return checkReserve(
        ctx.view,
        (*accSLE)[sfBalance],
        (*accSLE)[sfOwnerCount] + 1,
        ctx.tx[~sfAmount]);
}

TER
CDPCreate::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const accSLE = ctx_.view().peek(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    auto const scKeylet =
        keylet::stableCoin(ctx_.tx[sfStableCoinOwner], ctx_.tx[sfAssetType]);

    auto scSLE = ctx_.view().peek(scKeylet);
    if (!scSLE)
        return tecNO_ENTRY;

    auto const cdpKeylet = keylet::cdp(account, scKeylet.key);
    auto const cdpSLE = std::make_shared<SLE>(cdpKeylet);
    (*cdpSLE)[sfStableCoinID] = scKeylet.key;
    if (auto const amt = ctx_.tx[~sfAmount])
    {
        auto const xrpAmt = (*amt).xrp();
        auto const ter = cdpDeposit(*accSLE, *scSLE, *cdpSLE, xrpAmt);
        if (ter != tesSUCCESS)
            return ter;
    }
    else
    {
        (*cdpSLE)[sfBalance] = STAmount{};
    }
    (*cdpSLE)[sfIssuedCoins] = 0;

    // This is not effiecent, but will only be used for the prototype
    STVector256 cdps = scSLE->getFieldV256(sfCDPs);
    // Limit the number of cdps allowed in the array, just for the prototype
    // The real implementation will use a different design
    if (cdps.size() > 64)
        return tecSTABLECOIN_PROTOTYPE_LIMIT_EXCEEDED;
    cdps.push_back(cdpKeylet.key);
    scSLE->setFieldV256(sfCDPs, cdps);
    STVector64 rates = scSLE->getFieldV64(sfCDPAssetRatios);
    rates.push_back(std::numeric_limits<std::uint64_t>::max());
    scSLE->setFieldV64(sfCDPAssetRatios, rates);

    // Add to owner directory
    if (auto const page = dirAdd(
            ctx_.view(),
            keylet::ownerDir(account),
            cdpSLE->key(),
            false,
            describeOwnerDir(account),
            ctx_.app.journal("View")))
    {
        (*cdpSLE)[sfOwnerNode] = *page;
    }
    else
    {
        return tecDIR_FULL;
    }

    adjustOwnerCount(ctx_.view(), accSLE, 1, ctx_.journal);
    ctx_.view().insert(cdpSLE);
    ctx_.view().update(scSLE);

    return tesSUCCESS;
}

//------------------------------------------------------------------------------

NotTEC
CDPDelete::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureStableCoin))
        return temDISABLED;

    if (ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    return preflight2(ctx);
}

TER
CDPDelete::preclaim(PreclaimContext const& ctx)
{
    auto const account = ctx.tx[sfAccount];
    auto const accSLE = ctx.view.read(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    auto const scKeylet =
        keylet::stableCoin(ctx.tx[sfStableCoinOwner], ctx.tx[sfAssetType]);

    if (!ctx.view.read(scKeylet))
        return tecNO_ENTRY;

    auto const cdpKeylet = keylet::cdp(account, scKeylet.key);
    if (!ctx.view.read(cdpKeylet))
        return tecNO_ENTRY;

    return tesSUCCESS;
}

TER
CDPDelete::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const accSLE = ctx_.view().peek(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    auto const scKeylet =
        keylet::stableCoin(ctx_.tx[sfStableCoinOwner], ctx_.tx[sfAssetType]);

    auto scSLE = ctx_.view().peek(scKeylet);
    if (!scSLE)
        return tecNO_ENTRY;

    auto const cdpKeylet = keylet::cdp(account, scKeylet.key);
    auto const cdpSLE = ctx_.view().peek(cdpKeylet);
    if (!cdpSLE)
        return tecNO_ENTRY;

    if ((*cdpSLE)[sfIssuedCoins] != 0)
        return tecHAS_OBLIGATIONS;

    {
        // Remove the cdp from the stable coin object
        STVector256 cdps = scSLE->getFieldV256(sfCDPs);
        STVector64 rates = scSLE->getFieldV64(sfCDPAssetRatios);
        auto const it = std::find(cdps.begin(), cdps.end(), cdpKeylet.key);
        if (it == cdps.end())
        {
            assert(0);
            return tefINTERNAL;
        }
        // Remove the element by replacing this element by the one on the end
        // and shrinking the collection
        auto const index = std::distance(cdps.begin(), it);
        cdps[index] = cdps.back();
        rates[index] = rates.back();
        cdps.resize(cdps.size() - 1);
        rates.resize(rates.size() - 1);
        scSLE->setFieldV256(sfCDPs, cdps);
        scSLE->setFieldV64(sfCDPAssetRatios, rates);
    }

    {
        // Return the xrp to the owning account
        STAmount const accBal = (*accSLE)[sfBalance];
        STAmount const cdpBal = (*cdpSLE)[sfBalance];
        // cdp will be removed, but set balance to zero anyway
        (*cdpSLE)[sfBalance] = STAmount{};
        (*accSLE)[sfBalance] = accBal + cdpBal;
        (*scSLE)[sfCDPBalance] = (*scSLE)[sfCDPBalance] - cdpBal;
    }

    {
        // Remove from owner directory
        auto const page = (*cdpSLE)[sfOwnerNode];
        if (!ctx_.view().dirRemove(
                keylet::ownerDir(account), page, cdpKeylet.key, true))
        {
            JLOG(ctx_.journal.fatal())
                << "Could not remove cdp from owner directory";
            return tefBAD_LEDGER;
        }
        adjustOwnerCount(ctx_.view(), accSLE, -1, ctx_.journal);
    }

    ctx_.view().erase(cdpSLE);
    ctx_.view().update(scSLE);
    ctx_.view().update(accSLE);

    return tesSUCCESS;
}

//------------------------------------------------------------------------------

NotTEC
CDPDeposit::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureStableCoin))
        return temDISABLED;

    if (ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    if (!isXRP(ctx.tx[sfAmount]) || (ctx.tx[sfAmount] <= beast::zero))
        return temBAD_AMOUNT;

    return preflight2(ctx);
}

TER
CDPDeposit::preclaim(PreclaimContext const& ctx)
{
    auto const accSLE = ctx.view.read(keylet::account(ctx.tx[sfAccount]));
    if (!accSLE)
        return tefINTERNAL;

    return checkReserve(
        ctx.view,
        (*accSLE)[sfBalance],
        (*accSLE)[sfOwnerCount],
        ctx.tx[sfAmount]);
}

TER
CDPDeposit::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const accSLE = ctx_.view().peek(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    auto const scKeylet =
        keylet::stableCoin(ctx_.tx[sfStableCoinOwner], ctx_.tx[sfAssetType]);

    auto const cdpKeylet = keylet::cdp(account, scKeylet.key);
    auto const cdpSLE = ctx_.view().peek(cdpKeylet);

    if (!cdpSLE)
        return tecNO_ENTRY;

    auto const scSLE = ctx_.view().peek(scKeylet);
    if (!scSLE)
        // if the CDP exisit, the sc should exist
        return tefINTERNAL;

    auto const xrpAmt = ctx_.tx[sfAmount].xrp();
    {
        auto const ter = cdpDeposit(*accSLE, *scSLE, *cdpSLE, xrpAmt);
        if (ter != tesSUCCESS)
            return ter;
    }
    {
        auto const ter = updateCDPAssetRatio(*scSLE, *cdpSLE, cdpKeylet.key);
        if (ter != tesSUCCESS)
            return ter;
    }

    ctx_.view().update(accSLE);
    ctx_.view().update(scSLE);
    ctx_.view().update(cdpSLE);

    return tesSUCCESS;
}

NotTEC
CDPWithdraw::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureStableCoin))
        return temDISABLED;

    if (ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    if (!isXRP(ctx.tx[sfAmount]) || (ctx.tx[sfAmount] <= beast::zero))
        return temBAD_AMOUNT;

    return preflight2(ctx);
}

TER
CDPWithdraw::preclaim(PreclaimContext const& ctx)
{
    auto const accSLE = ctx.view.read(keylet::account(ctx.tx[sfAccount]));
    if (!accSLE)
        return tefINTERNAL;
    return tesSUCCESS;
}

TER
CDPWithdraw::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const accSLE = ctx_.view().peek(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    auto const scKeylet =
        keylet::stableCoin(ctx_.tx[sfStableCoinOwner], ctx_.tx[sfAssetType]);

    auto const cdpKeylet = keylet::cdp(account, scKeylet.key);
    auto const cdpSLE = ctx_.view().peek(cdpKeylet);

    if (!cdpSLE)
        return tecNO_ENTRY;

    auto const scSLE = ctx_.view().peek(scKeylet);
    if (!scSLE)
        // if the CDP exisit, the sc should exist
        return tefINTERNAL;

    auto const amt = ctx_.tx[sfAmount].xrp();
    if (amt > (*cdpSLE)[sfBalance])
        return tecUNFUNDED;
    auto const newBalance = (*cdpSLE)[sfBalance] - amt;

    {
        // check the collateral ratio
        auto const oracleID = (*scSLE)[sfOracleID];
        auto const oSLE = ctx_.view().peek(keylet::unchecked(oracleID));
        if (!oSLE)
            return tefINTERNAL;

        auto const closeTime =
            ctx_.view().info().parentCloseTime.time_since_epoch().count();
        auto const issRatio = (*scSLE)[sfIssuanceRatio];
        auto const issuedCoins = (*cdpSLE)[sfIssuedCoins];
        auto ter = checkCollateralRatio(
            *oSLE, newBalance, issRatio, issuedCoins, closeTime);
        if (ter != tesSUCCESS)
            return ter;
    }

    (*cdpSLE)[sfBalance] = newBalance;
    if (amt > (*scSLE)[sfCDPBalance])
        return tefINTERNAL;
    (*scSLE)[sfCDPBalance] = (*scSLE)[sfCDPBalance] - amt;
    (*accSLE)[sfBalance] = (*accSLE)[sfBalance] + amt;

    {
        auto const ter = updateCDPAssetRatio(*scSLE, *cdpSLE, cdpKeylet.key);
        if (ter != tesSUCCESS)
            return ter;
    }

    ctx_.view().update(accSLE);
    ctx_.view().update(scSLE);
    ctx_.view().update(cdpSLE);

    return tesSUCCESS;
}

NotTEC
StableCoinIssue::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureStableCoin))
        return temDISABLED;

    if (ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    return preflight2(ctx);
}

TER
StableCoinIssue::preclaim(PreclaimContext const& ctx)
{
    return tesSUCCESS;
}

TER
StableCoinIssue::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const accSLE = ctx_.view().peek(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    auto const scKeylet =
        keylet::stableCoin(ctx_.tx[sfStableCoinOwner], ctx_.tx[sfAssetType]);
    auto const cdpKeylet = keylet::cdp(account, scKeylet.key);
    auto const cdpSLE = ctx_.view().peek(cdpKeylet);

    if (!cdpSLE)
        return tecNO_ENTRY;

    auto const scSLE = ctx_.view().peek(scKeylet);
    if (!scSLE)
        // if the cdp exists, so should the stable coin
        return tefINTERNAL;

    auto const oracleID = (*scSLE)[sfOracleID];
    auto const oSLE = ctx_.view().peek(keylet::unchecked(oracleID));
    if (!oSLE)
        return tefINTERNAL;

    auto const curIssuedCoins = (*cdpSLE)[sfIssuedCoins];
    auto const coinsToAdd = ctx_.tx[sfStableCoinCount];
    auto const closeTime =
        ctx_.view().info().parentCloseTime.time_since_epoch().count();

    auto const [issAmtXrp, ter] =
        coinValue(*oSLE, coinsToAdd, closeTime, /*round up*/ true);
    if (ter)
        return ter;

    // Lambda can not capture structured binding; use &x=x as workaround.
    XRPAmount const loanOrgFee = [&scSLE, &issAmtXrp = issAmtXrp] {
        // TBD
        auto const fee = (*scSLE)[sfLoanOriginationFee];
        return mulRatio(issAmtXrp, fee, QUALITY_ONE, /*roundUp*/ false);
    }();

    if (loanOrgFee >= (*cdpSLE)[sfBalance])
        return tecSTABLECOIN_ISSUANCE_RATIO;

    {
        // check the collateral ratio
        auto const cdpBalance = (*cdpSLE)[sfBalance];

        auto const issRatio = (*scSLE)[sfIssuanceRatio];
        auto const proposedIssuedCoins = curIssuedCoins + coinsToAdd;
        if (proposedIssuedCoins < curIssuedCoins)
        {
            // overflow
            return tecSTABLECOIN_MAX_ISSUED_EXCEEDED;
        }
        auto ter = checkCollateralRatio(
            *oSLE,
            cdpBalance - loanOrgFee,
            issRatio,
            proposedIssuedCoins,
            closeTime);
        if (ter != tesSUCCESS)
            return ter;
    }

    auto const balKeylet = keylet::stableCoinBalance(account, scKeylet.key);
    auto balSLE = ctx_.view().peek(balKeylet);
    bool const insertBalSLE = !balSLE;
    if (!balSLE)
    {
        if (auto const ter = checkReserve(
                ctx_.view(), (*accSLE)[sfBalance], (*accSLE)[sfOwnerCount] + 1);
            ter != tesSUCCESS)
            return ter;
        balSLE = std::make_shared<SLE>(balKeylet);
        (*balSLE)[sfStableCoinID] = scKeylet.key;
        (*balSLE)[sfStableCoinBalance] = 0;

        // Add to owner directory
        if (auto const page = dirAdd(
                ctx_.view(),
                keylet::ownerDir(account),
                balSLE->key(),
                false,
                describeOwnerDir(account),
                ctx_.app.journal("View")))
        {
            (*balSLE)[sfOwnerNode] = *page;
        }
        else
        {
            return tecDIR_FULL;
        }
    }

    // return false if adding overflows, otherwise return true
    auto addCoins =
        [](SLE& sle, auto const& field, std::uint32_t coinsToAdd) -> bool {
        sle[field] = sle[field] + coinsToAdd;
        return (sle[field] >= coinsToAdd);
    };

    if (!addCoins(*scSLE, sfIssuedCoins, coinsToAdd))
        return tecSTABLECOIN_MAX_ISSUED_EXCEEDED;
    (*scSLE)[sfStabilityPoolBalance] =
        (*scSLE)[sfStabilityPoolBalance] + loanOrgFee;
    (*cdpSLE)[sfBalance] = (*cdpSLE)[sfBalance] - loanOrgFee;
    if (!addCoins(*cdpSLE, sfIssuedCoins, coinsToAdd))
        return tecSTABLECOIN_MAX_ISSUED_EXCEEDED;
    if (!addCoins(*balSLE, sfStableCoinBalance, coinsToAdd))
        return tecSTABLECOIN_MAX_ISSUED_EXCEEDED;

    {
        auto const ter = updateCDPAssetRatio(*scSLE, *cdpSLE, cdpKeylet.key);
        if (ter != tesSUCCESS)
            return ter;
    }

    ctx_.view().update(scSLE);
    ctx_.view().update(cdpSLE);
    if (insertBalSLE)
    {
        adjustOwnerCount(ctx_.view(), accSLE, 1, ctx_.journal);
        ctx_.view().insert(balSLE);
    }
    else
    {
        ctx_.view().update(balSLE);
    }

    return tesSUCCESS;
}

NotTEC
StableCoinRedeem::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureStableCoin))
        return temDISABLED;

    if (ctx.tx.getFlags() & tfStableCoinRedeemMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    return preflight2(ctx);
}

TER
StableCoinRedeem::preclaim(PreclaimContext const& ctx)
{
    return tesSUCCESS;
}

TER
StableCoinRedeem::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const accSLE = ctx_.view().peek(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;

    auto const scKeylet =
        keylet::stableCoin(ctx_.tx[sfStableCoinOwner], ctx_.tx[sfAssetType]);

    auto const scSLE = ctx_.view().peek(scKeylet);
    if (!scSLE)
        return tecNO_ENTRY;

    auto const numCoinToRedeem = ctx_.tx[sfStableCoinCount];

    auto const balKeylet = keylet::stableCoinBalance(account, scKeylet.key);
    auto balSLE = ctx_.view().peek(balKeylet);
    if (!balSLE || (*balSLE)[sfStableCoinBalance] < numCoinToRedeem)
        return tecSTABLECOIN_UNFUNDED_REDEEM;

    auto const ownerCDP = [&]() -> boost::optional<uint256> {
        if ((ctx_.tx[sfFlags] & tfOwnerCDP) == 0)
            return boost::none;
        return keylet::cdp(account, scKeylet.key).key;
    }();

    auto const oracleID = (*scSLE)[sfOracleID];
    auto const oSLE = ctx_.view().read(keylet::unchecked(oracleID));
    if (!oSLE)
        return tefINTERNAL;
    auto const closeTime =
        ctx_.view().info().parentCloseTime.time_since_epoch().count();
    if (auto const ter = checkValidOracle(*oSLE, closeTime); ter != tesSUCCESS)
        return ter;
    XRPAmount const xrpValue = (*oSLE)[sfOracleXRPValue].xrp();
    auto const assetCount = (*oSLE)[sfOracleAssetCount];

    auto coinValue = [&xrpValue,
                      &assetCount](std::uint32_t toRedeem) -> XRPAmount {
        return uncheckedCoinValue(
            toRedeem, xrpValue, assetCount, /*round up*/ false);
    };

    // return false if subtracting underflows, otherwise return true
    auto subtractValue =
        [](SLE& sle, auto const& field, auto const& toSubtract) -> bool {
        if (toSubtract > sle[field])
            return false;
        sle[field] = sle[field] - toSubtract;
        return true;
    };

    auto remainingCoinsToRedeem = numCoinToRedeem;
    for (auto i = CDPIter{*scSLE, ownerCDP}, e = CDPIter{};
         remainingCoinsToRedeem > 0 && i != e;
         ++i)
    {
        auto const cdpKey = keylet::unchecked(*i);
        auto const cdpSLE = ctx_.view().peek(cdpKey);
        if (!cdpSLE)
            return tefINTERNAL;
        auto const curCDPToRedeem = std::min<decltype(numCoinToRedeem)>(
            (*cdpSLE)[sfIssuedCoins], numCoinToRedeem);
        if (!curCDPToRedeem)
            continue;

        auto const xrpValue = coinValue(curCDPToRedeem);
        if (xrpValue > (*cdpSLE)[sfBalance])
        {
            // TBD: Undercolaterized CDP. Skip
            continue;
        }

        remainingCoinsToRedeem -= curCDPToRedeem;
        {
            // Remove the coins being redeemed from the ledger objects
            if (!subtractValue(*cdpSLE, sfIssuedCoins, curCDPToRedeem))
                return tefINTERNAL;
            if (!subtractValue(*scSLE, sfIssuedCoins, curCDPToRedeem))
                return tefINTERNAL;
            if (!subtractValue(*balSLE, sfStableCoinBalance, curCDPToRedeem))
                return tefINTERNAL;
        }
        {
            // Remove the xrp value from the CDP (and SC, which tracks totals)
            // and add it to the account balance
            if (!subtractValue(*cdpSLE, sfBalance, xrpValue))
                return tefINTERNAL;
            if (!subtractValue(*scSLE, sfCDPBalance, xrpValue))
                return tefINTERNAL;
            (*accSLE)[sfBalance] = (*accSLE)[sfBalance] + xrpValue;
        }

        if (auto t = updateCDPAssetRatio(*scSLE, *cdpSLE, cdpKey.key);
            t != tesSUCCESS)
            return t;

        ctx_.view().update(cdpSLE);
    }

    ctx_.view().update(scSLE);
    ctx_.view().update(accSLE);

    if (remainingCoinsToRedeem > 0)
        return tecCDP_DRY;

    if (!(*balSLE)[sfStableCoinBalance])
    {
        // balance is zero, remove the balance object
        // Remove from owner directory
        {
            auto const page = (*balSLE)[sfOwnerNode];
            if (!ctx_.view().dirRemove(
                    keylet::ownerDir(account), page, balKeylet.key, true))
            {
                JLOG(ctx_.journal.fatal()) << "Could not remove stable coin "
                                              "balance from owner directory";
                return tefBAD_LEDGER;
            }
            adjustOwnerCount(ctx_.view(), accSLE, -1, ctx_.journal);
            ctx_.view().erase(balSLE);
        }
    }
    else
    {
        ctx_.view().update(balSLE);
    }
    return tesSUCCESS;
}

NotTEC
StableCoinTransfer::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureStableCoin))
        return temDISABLED;

    if (ctx.tx.getFlags() & tfUniversalMask)
        return temINVALID_FLAG;

    auto const ret = preflight1(ctx);
    if (!isTesSuccess(ret))
        return ret;

    return preflight2(ctx);
}

TER
StableCoinTransfer::preclaim(PreclaimContext const& ctx)
{
    return tesSUCCESS;
}

TER
StableCoinTransfer::doApply()
{
    auto const account = ctx_.tx[sfAccount];
    auto const accSLE = ctx_.view().peek(keylet::account(account));
    if (!accSLE)
        return tefINTERNAL;
    auto const dst = ctx_.tx[sfDestination];
    auto const dstSLE = ctx_.view().peek(keylet::account(dst));
    if (!dstSLE)
        return tecNO_DST;

    auto const scKeylet =
        keylet::stableCoin(ctx_.tx[sfStableCoinOwner], ctx_.tx[sfAssetType]);
    auto const srcBalKeylet = keylet::stableCoinBalance(account, scKeylet.key);
    auto srcBalSLE = ctx_.view().peek(srcBalKeylet);
    decltype(sfStableCoinCount)::type::value_type coinsToTransfer =
        ctx_.tx[sfStableCoinCount];
    if (!srcBalSLE || (*srcBalSLE)[sfStableCoinBalance] < coinsToTransfer)
        // TBD: tecUNFUNDED_STABLECOIN_TRANSFER instead???
        return tecUNFUNDED_PAYMENT;

    auto const dstBalKeylet = keylet::stableCoinBalance(dst, scKeylet.key);
    auto dstBalSLE = ctx_.view().peek(dstBalKeylet);
    bool const insertDstBalSLE = !dstBalSLE;
    if (!dstBalSLE)
    {
        // Ledger object will be added: +1 owner count when checking reserve
        if (auto const ter = checkReserve(
                ctx_.view(), (*dstSLE)[sfBalance], (*dstSLE)[sfOwnerCount] + 1);
            ter != tesSUCCESS)
            return ter;

        // create the sle
        dstBalSLE = std::make_shared<SLE>(dstBalKeylet);
        (*dstBalSLE)[sfStableCoinID] = scKeylet.key;
        (*dstBalSLE)[sfStableCoinBalance] = 0;
        // Add to owner directory
        if (auto const page = dirAdd(
                ctx_.view(),
                keylet::ownerDir(dst),
                dstBalSLE->key(),
                false,
                describeOwnerDir(dst),
                ctx_.app.journal("View")))
        {
            (*dstBalSLE)[sfOwnerNode] = *page;
        }
        else
        {
            return tecDIR_FULL;
        }
    }

    (*srcBalSLE)[sfStableCoinBalance] =
        (*srcBalSLE)[sfStableCoinBalance] - coinsToTransfer;
    (*dstBalSLE)[sfStableCoinBalance] =
        (*dstBalSLE)[sfStableCoinBalance] + coinsToTransfer;

    ctx_.view().update(srcBalSLE);

    if (insertDstBalSLE)
    {
        adjustOwnerCount(ctx_.view(), dstSLE, 1, ctx_.journal);
        ctx_.view().insert(dstBalSLE);
    }
    else
    {
        ctx_.view().update(dstBalSLE);
    }

    return tesSUCCESS;
}

}  // namespace ripple
