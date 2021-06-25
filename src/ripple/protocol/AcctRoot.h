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

#ifndef RIPPLE_PROTOCOL_ACCT_ROOT_H_INCLUDED
#define RIPPLE_PROTOCOL_ACCT_ROOT_H_INCLUDED

#include <ripple/basics/tl/expected.hpp>
#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/TER.h>

#include <type_traits>
#include <utility>

namespace ripple {

template <bool Writable>
class AcctRootWrapper
{
    using wrapped_t =
        std::shared_ptr<std::conditional_t<Writable, SLE, SLE const>>;
    wrapped_t wrapped_;

    [[nodiscard]] Blob
    getOptionalVL(SF_VL const& field) const;

    template <typename SF, typename T, bool W = Writable>
    void
    setOptional(SF const& field, T const& value)
    {
        static_assert(
            std::is_base_of_v<SField, SF>,
            "setOptional()requires an SField as its first argument.");

        if (!wrapped_->isFieldPresent(field))
            wrapped_->makeFieldPresent(field);
        wrapped_->at(field) = value;
    }

    template <typename SF, bool W = Writable>
    void
    clearOptional(SF const& field)
    {
        static_assert(
            std::is_base_of_v<SField, SF>,
            "setOptional()requires an SField as its argument.");

        if (wrapped_->isFieldPresent(field))
            wrapped_->makeFieldAbsent(field);
    }

    void
    setOrClearVLIfEmpty(SF_VL const& field, Blob const& value);

    AcctRootWrapper(AcctRootWrapper const&) = default;
    AcctRootWrapper&
    operator=(AcctRootWrapper const&) = default;

public:
    AcctRootWrapper() = delete;
    AcctRootWrapper(wrapped_t&& w);
    AcctRootWrapper(std::nullptr_t);
    AcctRootWrapper(AcctRootWrapper&&) = default;

    bool
    has_value() const;

    explicit operator bool() const;

    [[nodiscard]] wrapped_t const&
    slePtr() const;

    [[nodiscard]] AccountID
    accountID() const;

    [[nodiscard]] std::uint32_t
    flags() const;

    [[nodiscard]] bool
    isFlag(std::uint32_t flagsToCheck) const;

    void
    replaceAllFlags(std::uint32_t newFlags);

    void
    setFlag(std::uint32_t flagsToSet);

    void
    clearFlag(std::uint32_t flagsToClear);

    [[nodiscard]] std::uint32_t
    sequence() const;

    void
    setSequence(std::uint32_t seq);

    [[nodiscard]] STAmount
    balance() const;

    void
    setBalance(STAmount const& amount);

    [[nodiscard]] std::uint32_t
    ownerCount() const;

    void
    setOwnerCount(std::uint32_t newCount);

    [[nodiscard]] std::uint32_t
    previousTxnID() const;

    void
    setPreviousTxnID(uint256 prevTxID);

    [[nodiscard]] std::uint32_t
    previousTxnLgrSeq() const;

    void
    setPreviousTxnLgrSeq(std::uint32_t prevTxLgrSeq);

    [[nodiscard]] std::optional<uint256>
    accountTxnID() const;

    void
    setAccountTxnID(uint256 const& newAcctTxnID);

    void
    clearAccountTxnID();

    [[nodiscard]] std::optional<AccountID>
    regularKey() const;

    void
    setRegularKey(AccountID const& newRegKey);

    void
    clearRegularKey();

    [[nodiscard]] std::optional<uint128>
    emailHash() const;

    void
    setEmailHash(uint128 const& newEmailHash);

    void
    clearEmailHash();

    [[nodiscard]] std::optional<uint256>
    walletLocator() const;

    void
    setWalletLocator(uint256 const& newWalletLocator);

    void
    clearWalletLocator();

    [[nodiscard]] std::optional<std::uint32_t>
    walletSize();

    [[nodiscard]] Blob
    messageKey() const;

    void
    setMessageKey(Blob const& newMessageKey);

    [[nodiscard]] std::optional<std::uint32_t>
    transferRate() const;

    void
    setTransferRate(std::uint32_t newTransferRate);

    void
    clearTransferRate();

    [[nodiscard]] Blob
    domain() const;

    void
    setDomain(Blob const& newDomain);

    [[nodiscard]] std::optional<std::uint8_t>
    tickSize() const;

    void
    setTickSize(std::uint8_t newTickSize);

    void
    clearTickSize();

    [[nodiscard]] std::optional<std::uint32_t>
    ticketCount() const;

    void
    setTicketCount(std::uint32_t newTicketCount);

    void
    clearTicketCount();
};

using AcctRootRd = AcctRootWrapper<false>;
using AcctRoot = AcctRootWrapper<true>;

[[nodiscard]] std::pair<AcctRootRd const, NotTEC>
makeAcctRootRd(std::shared_ptr<STLedgerEntry const> slePtr);

[[nodiscard]] std::pair<AcctRoot, NotTEC>
makeAcctRoot(std::shared_ptr<STLedgerEntry> slePtr);

}  // namespace ripple

#endif  // RIPPLE_PROTOCOL_ACCT_ROOT_H_INCLUDED
