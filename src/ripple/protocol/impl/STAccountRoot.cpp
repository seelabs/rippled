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

#include <ripple/protocol/STAccountRoot.h>

#include <ripple/protocol/STAccount.h>

namespace ripple {
template <typename SF, typename T>
void
STAccountRoot::setOptional(SF const& field, T const& value)
{
    static_assert(
        std::is_base_of_v<SField, SF>,
        "setOptional()requires an SField as its first argument.");

    if (!isFieldPresent(field))
        makeFieldPresent(field);
    at(field) = value;
}

template <typename SF>
void
STAccountRoot::clearOptional(SF const& field)
{
    static_assert(
        std::is_base_of_v<SField, SF>,
        "setOptional()requires an SField as its argument.");

    if (isFieldPresent(field))
        makeFieldAbsent(field);
}

Blob
STAccountRoot::getOptionalVL(SF_VL const& field) const
{
    Blob ret;
    if (isFieldPresent(field))
        ret = getFieldVL(field);
    return ret;
}

void
STAccountRoot::setOrClearVLIfEmpty(SF_VL const& field, Blob const& value)
{
    if (value.empty())
    {
        clearOptional(field);
        return;
    }
    if (!isFieldPresent(field))
        makeFieldPresent(field);
    setFieldVL(field, value);
}

AccountID
STAccountRoot::accountID() const
{
    return at(sfAccount);
}

std::uint32_t
STAccountRoot::sequence() const
{
    return at(sfSequence);
}

void
STAccountRoot::setSequence(std::uint32_t seq)
{
    at(sfSequence) = seq;
}

STAmount
STAccountRoot::balance() const
{
    return at(sfBalance);
}

void
STAccountRoot::setBalance(STAmount const& amount)
{
    at(sfBalance) = amount;
}

std::uint32_t
STAccountRoot::ownerCount() const
{
    return at(sfOwnerCount);
}

void
STAccountRoot::setOwnerCount(std::uint32_t newCount)
{
    at(sfOwnerCount) = newCount;
}

std::uint32_t
STAccountRoot::previousTxnID() const
{
    return at(sfOwnerCount);
}

void
STAccountRoot::setPreviousTxnID(uint256 prevTxID)
{
    at(sfPreviousTxnID) = prevTxID;
}

std::uint32_t
STAccountRoot::previousTxnLgrSeq() const
{
    return at(sfPreviousTxnLgrSeq);
}

void
STAccountRoot::setPreviousTxnLgrSeq(std::uint32_t prevTxLgrSeq)
{
    at(sfPreviousTxnLgrSeq) = prevTxLgrSeq;
}

std::optional<uint256>
STAccountRoot::accountTxnID() const
{
    return at(~sfAccountTxnID);
}

void
STAccountRoot::setAccountTxnID(uint256 const& newAcctTxnID)
{
    setOptional(sfAccountTxnID, newAcctTxnID);
}

void
STAccountRoot::clearAccountTxnID()
{
    clearOptional(sfAccountTxnID);
}

std::optional<AccountID>
STAccountRoot::regularKey() const
{
    return at(~sfRegularKey);
}

void
STAccountRoot::setRegularKey(AccountID const& newRegKey)
{
    setOptional(sfRegularKey, newRegKey);
}

void
STAccountRoot::clearRegularKey()
{
    clearOptional(sfRegularKey);
}

std::optional<uint128>
STAccountRoot::emailHash() const
{
    return at(~sfEmailHash);
}

void
STAccountRoot::setEmailHash(uint128 const& newEmailHash)
{
    setOptional(sfEmailHash, newEmailHash);
}

void
STAccountRoot::clearEmailHash()
{
    clearOptional(sfEmailHash);
}

std::optional<uint256>
STAccountRoot::walletLocator() const
{
    return at(~sfWalletLocator);
}

void
STAccountRoot::setWalletLocator(uint256 const& newWalletLocator)
{
    setOptional(sfWalletLocator, newWalletLocator);
}

void
STAccountRoot::clearWalletLocator()
{
    clearOptional(sfWalletLocator);
}

std::optional<std::uint32_t>
STAccountRoot::walletSize()
{
    return at(~sfWalletSize);
}

void
STAccountRoot::setWalletSize(std::uint32_t newWalletSize)
{
    setOptional(sfWalletSize, newWalletSize);
}

void
STAccountRoot::clearWalletSize()
{
    clearOptional(sfWalletSize);
}

Blob
STAccountRoot::messageKey() const
{
    return getOptionalVL(sfMessageKey);
}

void
STAccountRoot::setMessageKey(Blob const& newMessageKey)
{
    setOrClearVLIfEmpty(sfMessageKey, newMessageKey);
}

std::optional<std::uint32_t>
STAccountRoot::transferRate() const
{
    return at(~sfTransferRate);
}

void
STAccountRoot::setTransferRate(std::uint32_t newTransferRate)
{
    setOptional(sfTransferRate, newTransferRate);
}

void
STAccountRoot::clearTransferRate()
{
    clearOptional(sfTransferRate);
}

Blob
STAccountRoot::domain() const
{
    return getOptionalVL(sfDomain);
}

void
STAccountRoot::setDomain(Blob const& newDomain)
{
    setOrClearVLIfEmpty(sfDomain, newDomain);
}

std::optional<std::uint8_t>
STAccountRoot::tickSize() const
{
    return at(sfTickSize);
}

void
STAccountRoot::setTickSize(std::uint8_t newTickSize)
{
    setOptional(sfTickSize, newTickSize);
}

void
STAccountRoot::clearTickSize()
{
    clearOptional(sfTickSize);
}

std::optional<std::uint32_t>
STAccountRoot::ticketCount()
{
    return at(~sfTicketCount);
}

void
STAccountRoot::setTicketCount(std::uint32_t newTicketCount)
{
    setOptional(sfTicketCount, newTicketCount);
}

void
STAccountRoot::clearTicketCount()
{
    clearOptional(sfTicketCount);
}

}  // namespace ripple
