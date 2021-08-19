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

#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/STLedgerEntry.h>

#include <type_traits>

namespace ripple {

// This is just a dummy class to show how strongly typed keylet would work.
// The wrapper isn't important for this; This patch is focused on the strongly
// typed keylets and how they are meant to work.
template <bool Writable>
class AccountRootWrapper
{
    using pointer = std::conditional_t<
        Writable,
        std::shared_ptr<STLedgerEntry>,
        std::shared_ptr<const STLedgerEntry>>;

    pointer wrapped_;

    template <typename SF, typename T>
    void
    setOptional(SF const& field, T const& value)
    {
        static_assert(
            std::is_base_of_v<SField, SF>,
            "setOptional()requires an SField as its first argument.");

        if (!isFieldPresent(field))
            makeFieldPresent(field);
        wrapped_->at(field) = value;
    }

    template <typename SF>
    void
    clearOptional(SF const& field)
    {
        static_assert(
            std::is_base_of_v<SField, SF>,
            "setOptional()requires an SField as its argument.");

        if (wrapped_->isFieldPresent(field))
            wrapped_->makeFieldAbsent(field);
    }

    Blob
    getOptionalVL(SF_VL const& field) const
    {
        Blob ret;
        if (wrapped_->isFieldPresent(field))
            ret = wrapped_->getFieldVL(field);
        return ret;
    }

    void
    setOrClearVLIfEmpty(SF_VL const& field, Blob const& value)
    {
        if (value.empty())
        {
            clearOptional(field);
            return;
        }
        if (!wrapped_->isFieldPresent(field))
            wrapped_->makeFieldPresent(field);
        wrapped_->setFieldVL(field, value);
    }

public:
    AccountRootWrapper(pointer w) : wrapped_(std::move(w))
    {
    }

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

    [[nodiscard]] std::uint32_t
    flags() const;

    void
    setFlags(std::uint32_t newFlags);

    bool
    isFieldPresent(SField const& field) const
    {
        return wrapped_->isFieldPresent(field);
    }

    STBase*
    makeFieldPresent(SField const& field)
    {
        return wrapped_->makeFieldPresent(field);
    }

    void
    makeFieldAbsent(SField const& field)
    {
        return wrapped_->makeFieldAbsent(field);
    }
};

template <bool Writable>
AccountID
AccountRootWrapper<Writable>::accountID() const
{
    return wrapped_->at(sfAccount);
}

template <bool Writable>
std::uint32_t
AccountRootWrapper<Writable>::sequence() const
{
    return wrapped_->at(sfSequence);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setSequence(std::uint32_t seq)
{
    wrapped_->at(sfSequence) = seq;
}

template <bool Writable>
STAmount
AccountRootWrapper<Writable>::balance() const
{
    return wrapped_->at(sfBalance);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setBalance(STAmount const& amount)
{
    wrapped_->at(sfBalance) = amount;
}

template <bool Writable>
std::uint32_t
AccountRootWrapper<Writable>::ownerCount() const
{
    return wrapped_->at(sfOwnerCount);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setOwnerCount(std::uint32_t newCount)
{
    wrapped_->at(sfOwnerCount) = newCount;
}

template <bool Writable>
std::uint32_t
AccountRootWrapper<Writable>::previousTxnID() const
{
    return wrapped_->at(sfOwnerCount);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setPreviousTxnID(uint256 prevTxID)
{
    wrapped_->at(sfPreviousTxnID) = prevTxID;
}

template <bool Writable>
std::uint32_t
AccountRootWrapper<Writable>::previousTxnLgrSeq() const
{
    return wrapped_->at(sfPreviousTxnLgrSeq);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setPreviousTxnLgrSeq(std::uint32_t prevTxLgrSeq)
{
    wrapped_->at(sfPreviousTxnLgrSeq) = prevTxLgrSeq;
}

template <bool Writable>
std::optional<uint256>
AccountRootWrapper<Writable>::accountTxnID() const
{
    return wrapped_->at(~sfAccountTxnID);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setAccountTxnID(uint256 const& newAcctTxnID)
{
    setOptional(sfAccountTxnID, newAcctTxnID);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::clearAccountTxnID()
{
    clearOptional(sfAccountTxnID);
}

template <bool Writable>
std::optional<AccountID>
AccountRootWrapper<Writable>::regularKey() const
{
    return wrapped_->at(~sfRegularKey);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setRegularKey(AccountID const& newRegKey)
{
    setOptional(sfRegularKey, newRegKey);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::clearRegularKey()
{
    clearOptional(sfRegularKey);
}

template <bool Writable>
std::optional<uint128>
AccountRootWrapper<Writable>::emailHash() const
{
    return wrapped_->at(~sfEmailHash);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setEmailHash(uint128 const& newEmailHash)
{
    setOptional(sfEmailHash, newEmailHash);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::clearEmailHash()
{
    clearOptional(sfEmailHash);
}

template <bool Writable>
std::optional<uint256>
AccountRootWrapper<Writable>::walletLocator() const
{
    return wrapped_->at(~sfWalletLocator);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setWalletLocator(uint256 const& newWalletLocator)
{
    setOptional(sfWalletLocator, newWalletLocator);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::clearWalletLocator()
{
    clearOptional(sfWalletLocator);
}

template <bool Writable>
std::optional<std::uint32_t>
AccountRootWrapper<Writable>::walletSize()
{
    return wrapped_->at(~sfWalletSize);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setWalletSize(std::uint32_t newWalletSize)
{
    setOptional(sfWalletSize, newWalletSize);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::clearWalletSize()
{
    clearOptional(sfWalletSize);
}

template <bool Writable>
Blob
AccountRootWrapper<Writable>::messageKey() const
{
    return wrapped_->getOptionalVL(sfMessageKey);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setMessageKey(Blob const& newMessageKey)
{
    setOrClearVLIfEmpty(sfMessageKey, newMessageKey);
}

template <bool Writable>
std::optional<std::uint32_t>
AccountRootWrapper<Writable>::transferRate() const
{
    return wrapped_->at(~sfTransferRate);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setTransferRate(std::uint32_t newTransferRate)
{
    setOptional(sfTransferRate, newTransferRate);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::clearTransferRate()
{
    clearOptional(sfTransferRate);
}

template <bool Writable>
Blob
AccountRootWrapper<Writable>::domain() const
{
    return wrapped_->getOptionalVL(sfDomain);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setDomain(Blob const& newDomain)
{
    setOrClearVLIfEmpty(sfDomain, newDomain);
}

template <bool Writable>
std::optional<std::uint8_t>
AccountRootWrapper<Writable>::tickSize() const
{
    return wrapped_->at(sfTickSize);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setTickSize(std::uint8_t newTickSize)
{
    setOptional(sfTickSize, newTickSize);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::clearTickSize()
{
    clearOptional(sfTickSize);
}

template <bool Writable>
std::optional<std::uint32_t>
AccountRootWrapper<Writable>::ticketCount()
{
    return wrapped_->at(~sfTicketCount);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setTicketCount(std::uint32_t newTicketCount)
{
    setOptional(sfTicketCount, newTicketCount);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::clearTicketCount()
{
    clearOptional(sfTicketCount);
}

template <bool Writable>
std::uint32_t
AccountRootWrapper<Writable>::flags() const
{
    return wrapped_->at(sfFlags);
}

template <bool Writable>
void
AccountRootWrapper<Writable>::setFlags(std::uint32_t newFlags)
{
    wrapped_->at(sfFlags) = newFlags;
}

}  // namespace ripple
#endif
