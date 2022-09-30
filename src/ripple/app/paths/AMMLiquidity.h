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

#ifndef RIPPLE_APP_TX_AMMOFFERMAKER_H_INCLUDED
#define RIPPLE_APP_TX_AMMOFFERMAKER_H_INCLUDED

#include "ripple/app/misc/AMM.h"
#include "ripple/app/misc/AMM_formulae.h"
#include "ripple/app/paths/AMMOfferCounter.h"
#include "ripple/basics/Log.h"
#include "ripple/ledger/ReadView.h"
#include "ripple/ledger/View.h"
#include "ripple/protocol/Quality.h"
#include "ripple/protocol/STLedgerEntry.h"

namespace ripple {

namespace detail {

/** Generate AMM offers with the offer size based on Fibonacci sequence.
 * The sequence corresponds to the payment engine iterations with AMM
 * liquidity. Iterations that don't consume AMM offers don't count.
 * We max out at four iterations with AMM offers.
 */
// I don't understand this
// Question: how are partially consumed offers handled?
// Question: What's the worst case?
class FibSeqHelper
{
private:
    // Every member variable is mutable?
    // Current sequence amounts.
    Amounts curSeq_{};
    // Latest sequence number.
    std::uint16_t lastNSeq_{0};
    Number x_{0};
    Number y_{0};

public:
    FibSeqHelper() = default;
    ~FibSeqHelper() = default;
    FibSeqHelper(FibSeqHelper const&) = delete;
    FibSeqHelper&
    operator=(FibSeqHelper const&) = delete;
    /** Generate first sequence.
     * @param balances current AMM pool balances.
     * @param tfee trading fee in basis points.
     * @return
     */
    // Don't define functions in the class
    // This also shouldn't be const
    // So the first offer is 5/20000? And we can only have 4 offers? The largest
    // offer isn't that large.
    Amounts const&
    firstSeq(Amounts const& balances, std::uint16_t tfee)
    {
        curSeq_.in = toSTAmount(
            balances.in.issue(),
            // what's with the /2? Why not just /20000 to begin with?
            (Number(5) / 10000) * balances.in / 2,
            Number::rounding_mode::upward);
        curSeq_.out = swapAssetIn(balances, curSeq_.in, tfee);
        y_ = curSeq_.out;
        return curSeq_;
    }
    /** Generate next sequence.
     * @param n sequence to generate
     * @param balances current AMM pool balances.
     * @param tfee trading fee in basis points.
     * @return
     */
    // This shouldn't be const
    // rename this nthOffer. Lose the "next". Next could be current+n
    Amounts const&
    nextNthSeq(std::uint16_t n, Amounts const& balances, std::uint16_t tfee)
    {
        // We are at the same payment engine iteration when executing
        // a limiting step. Have to generate the same sequence.
        if (n == lastNSeq_)
            return curSeq_;
        if (n < lastNSeq_)
            // Let's make sure we're handling this exception
            Throw<std::runtime_error>(
                std::string("nextNthSeq: invalid sequence ") +
                std::to_string(n) + " " + std::to_string(lastNSeq_));
        auto const total = [&]() {
            Number total{};
            do
            {
                // There's nothing special about fib. How about choosing a
                // function that isn't computed iteratively?
                total = x_ + y_;
                x_ = y_;
                y_ = total;
            } while (++lastNSeq_ < n);
            return total;
        }();
        curSeq_.out = toSTAmount(
            balances.out.issue(), total, Number::rounding_mode::downward);
        curSeq_.in = swapAssetOut(balances, curSeq_.out, tfee);
        return curSeq_;
    }
};

}  // namespace detail

/** AMMLiquidity class provides AMM offers to BookStep class.
 * The offers are generated in two ways. If there are multiple
 * paths specified to the payment transaction then the offers
 * are generated based on the Fibonacci sequence with
 * at most four payment engine iterations consuming AMM offers.
 * These offers behave the same way as CLOB offers in that if
 * there is a limiting step, then the offers are adjusted
 * based on their quality.
 * If there is only one path specified in the payment transaction
 * then the offers are generated based on the competing CLOB offer
 * quality. In this case, the offer's size is set in such a way
 * that the new AMM's pool spot price quality is equal to the CLOB's
 * offer quality.
 */
// So there are at most 4 interactions? After that is uses the offer book?
// I don't love that there are two separate algorithms here: one for one path
// and one for multi-path
// Are the offer persistent? I.e. do the two different paths see the partially
// consumed offer? What happens to overpaid fees? Does the AMM keep them?
class AMMLiquidity
{
private:
    // This isn't const
    AMMOfferCounter& offerCounter_;
    AccountID const ammAccountID_;
    std::uint32_t const tradingFee_;
    // Lots of mutable here
    // Cached AMM pool balances as of last getOffer()
    // why not optional and clear when dirty? (and remove the dirty flag)
    mutable Amounts balances_;
    // Is seated in case of multi-path. Generates Fibonacci
    // sequence offer.
    // Rewrite this so it doesn't need to be mutable
    mutable std::optional<detail::FibSeqHelper> fibSeqHelper_;
    // Indicates that the balances may have changed
    // since the last fetchBalances()
    mutable bool dirty_;
    beast::Journal const j_;

public:
    AMMLiquidity(
        ReadView const& view,
        AccountID const& ammAccountID,
        std::uint32_t tradingFee,
        Issue const& in,
        Issue const& out,
        AMMOfferCounter& offerCounter,
        beast::Journal j);
    ~AMMLiquidity() = default;
    AMMLiquidity(AMMLiquidity const&) = delete;
    AMMLiquidity&
    operator=(AMMLiquidity const&) = delete;

    /** Generate AMM offer. Returns nullopt if clobQuality is provided
     * and it is better than AMM offer quality. Otherwise returns AMM offer.
     * If clobQuality is provided then AMM offer size is set based on the
     * quality.
     */
    std::optional<Amounts>
    getOffer(ReadView const& view, std::optional<Quality> const& clobQuality)
        const;

    /** Called when AMM offer is consumed. Sets dirty flag
     * to indicate that the balances may have changed and
     * increments offer counter to indicate that AMM offer
     * is used in the strand.
     */
    void
    consumed()
    {
        dirty_ = true;
        offerCounter_.incrementCounter();
    }

    AccountID const&
    ammAccount() const
    {
        return ammAccountID_;
    }

    bool
    multiPath() const
    {
        return offerCounter_.multiPath();
    }

    template <typename TOut>
    STAmount
    swapOut(TOut const& out) const
    {
        return swapAssetOut(balances_, out, tradingFee_);
    }

    template <typename TIn>
    STAmount
    swapIn(TIn const& in) const
    {
        return swapAssetIn(balances_, in, tradingFee_);
    }

private:
    /** Fetches AMM balances if dirty flag is set.
     */
    Amounts
    fetchBalances(ReadView const& view) const;

    /** Returns total amount held by AMM for the given token.
     */
    STAmount
    ammAccountHolds(
        ReadView const& view,
        AccountID const& ammAccountID,
        Issue const& issue) const;

    /** Generate offer based on Fibonacci sequence.
     * @param balances current AMM balances
     */
    Amounts
    generateFibSeqOffer(Amounts const& balances) const;
};
}  // namespace ripple

#endif  // RIPPLE_APP_TX_AMMOFFERMAKER_H_INCLUDED
