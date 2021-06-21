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

#ifndef RIPPLE_PROTOCOL_STACCOUNTROOT_H_INCLUDED
#define RIPPLE_PROTOCOL_STACCOUNTROOT_H_INCLUDED

#include <ripple/protocol/STLedgerEntry.h>

namespace ripple {
class STAccountRoot final : public STLedgerEntry
{
public:
    using STLedgerEntry::STLedgerEntry;

    using pointer = std::shared_ptr<STGenericLedgerEntry>;
    using ref = const std::shared_ptr<STGenericLedgerEntry>&;

    STBase*
    copy(std::size_t n, void* buf) const override
    {
        return emplace(n, buf, *this);
    }

    STBase*
    move(std::size_t n, void* buf) override
    {
        return emplace(n, buf, std::move(*this));
    }

private:
    template <typename SF, typename T>
    void
    setOptional(SF const& field, T const& value);

    template <typename SF>
    void
    clearOptional(SF const& field);

    Blob
    getOptionalVL(SF_VL const& field) const;

    void
    setOrClearVLIfEmpty(SF_VL const& field, Blob const& value);

public:
    [[nodiscard]] AccountID
    accountID() const;

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

    void
    setWalletSize(std::uint32_t newWalletSize);

    void
    clearWalletSize();

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
    ticketCount();

    void
    setTicketCount(std::uint32_t newTicketCount);

    void
    clearTicketCount();
};

}  // namespace ripple
#endif
