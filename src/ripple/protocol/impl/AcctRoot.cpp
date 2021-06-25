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

#include <ripple/protocol/AcctRoot.h>

namespace ripple {

Blob
AcctRoot::getOptionalVL(SF_VL const& field) const
{
    Blob ret;
    if (wrapped_->isFieldPresent(field))
        ret = wrapped_->getFieldVL(field);
    return ret;
}

void
AcctRoot::setOrClearVLIfEmpty(SF_VL const& field, Blob const& value)
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

AcctRoot::AcctRoot(std::shared_ptr<SLE>&& w) : wrapped_(std::move(w))
{
}

AcctRoot::AcctRoot(std::shared_ptr<SLE const>&& w)
    : wrapped_(std::const_pointer_cast<SLE>(std::move(w)))
{
}

AcctRoot::AcctRoot(std::nullptr_t)
{
}

bool
AcctRoot::has_value() const
{
    return bool(wrapped_);
}

AcctRoot::operator bool() const
{
    return has_value();
}

std::shared_ptr<SLE>
AcctRoot::slePtr()
{
    return wrapped_;
}

std::shared_ptr<SLE const>
AcctRoot::slePtr() const
{
    return wrapped_;
}

AccountID
AcctRoot::accountID() const
{
    return wrapped_->at(sfAccount);
}

std::uint32_t
AcctRoot::flags() const
{
    return wrapped_->at(sfFlags);
}

bool
AcctRoot::isFlag(std::uint32_t flagsToCheck) const
{
    return (flags() & flagsToCheck) == flagsToCheck;
}

void
AcctRoot::replaceAllFlags(std::uint32_t newFlags)
{
    wrapped_->at(sfFlags) = newFlags;
}

void
AcctRoot::setFlag(std::uint32_t flagsToSet)
{
    replaceAllFlags(flags() | flagsToSet);
}

void
AcctRoot::clearFlag(std::uint32_t flagsToClear)
{
    replaceAllFlags(flags() & ~flagsToClear);
}

std::uint32_t
AcctRoot::sequence() const
{
    return wrapped_->at(sfSequence);
}

void
AcctRoot::setSequence(std::uint32_t seq)
{
    wrapped_->at(sfSequence) = seq;
}

STAmount
AcctRoot::balance() const
{
    return wrapped_->at(sfBalance);
}

void
AcctRoot::setBalance(STAmount const& amount)
{
    wrapped_->at(sfBalance) = amount;
}

std::uint32_t
AcctRoot::ownerCount() const
{
    return wrapped_->at(sfOwnerCount);
}

void
AcctRoot::setOwnerCount(std::uint32_t newCount)
{
    wrapped_->at(sfOwnerCount) = newCount;
}

std::uint32_t
AcctRoot::previousTxnID() const
{
    return wrapped_->at(sfOwnerCount);
}

void
AcctRoot::setPreviousTxnID(uint256 prevTxID)
{
    wrapped_->at(sfPreviousTxnID) = prevTxID;
}

std::uint32_t
AcctRoot::previousTxnLgrSeq() const
{
    return wrapped_->at(sfPreviousTxnLgrSeq);
}

void
AcctRoot::setPreviousTxnLgrSeq(std::uint32_t prevTxLgrSeq)
{
    wrapped_->at(sfPreviousTxnLgrSeq) = prevTxLgrSeq;
}

std::optional<uint256>
AcctRoot::accountTxnID() const
{
    return wrapped_->at(~sfAccountTxnID);
}

void
AcctRoot::setAccountTxnID(uint256 const& newAcctTxnID)
{
    setOptional(sfAccountTxnID, newAcctTxnID);
}

void
AcctRoot::clearAccountTxnID()
{
    clearOptional(sfAccountTxnID);
}

std::optional<AccountID>
AcctRoot::regularKey() const
{
    return wrapped_->at(~sfRegularKey);
}

void
AcctRoot::setRegularKey(AccountID const& newRegKey)
{
    setOptional(sfRegularKey, newRegKey);
}

void
AcctRoot::clearRegularKey()
{
    clearOptional(sfRegularKey);
}

std::optional<uint128>
AcctRoot::emailHash() const
{
    return wrapped_->at(~sfEmailHash);
}

void
AcctRoot::setEmailHash(uint128 const& newEmailHash)
{
    setOptional(sfEmailHash, newEmailHash);
}

void
AcctRoot::clearEmailHash()
{
    clearOptional(sfEmailHash);
}

std::optional<uint256>
AcctRoot::walletLocator() const
{
    return wrapped_->at(~sfWalletLocator);
}

void
AcctRoot::setWalletLocator(uint256 const& newWalletLocator)
{
    setOptional(sfWalletLocator, newWalletLocator);
}

void
AcctRoot::clearWalletLocator()
{
    clearOptional(sfWalletLocator);
}

std::optional<std::uint32_t>
AcctRoot::walletSize()
{
    return wrapped_->at(~sfWalletSize);
}

Blob
AcctRoot::messageKey() const
{
    return getOptionalVL(sfMessageKey);
}

void
AcctRoot::setMessageKey(Blob const& newMessageKey)
{
    setOrClearVLIfEmpty(sfMessageKey, newMessageKey);
}

std::optional<std::uint32_t>
AcctRoot::transferRate() const
{
    return wrapped_->at(~sfTransferRate);
}

void
AcctRoot::setTransferRate(std::uint32_t newTransferRate)
{
    setOptional(sfTransferRate, newTransferRate);
}

void
AcctRoot::clearTransferRate()
{
    clearOptional(sfTransferRate);
}

Blob
AcctRoot::domain() const
{
    return getOptionalVL(sfDomain);
}

void
AcctRoot::setDomain(Blob const& newDomain)
{
    setOrClearVLIfEmpty(sfDomain, newDomain);
}

std::optional<std::uint8_t>
AcctRoot::tickSize() const
{
    return wrapped_->at(sfTickSize);
}

void
AcctRoot::setTickSize(std::uint8_t newTickSize)
{
    setOptional(sfTickSize, newTickSize);
}

void
AcctRoot::clearTickSize()
{
    clearOptional(sfTickSize);
}

std::optional<std::uint32_t>
AcctRoot::ticketCount() const
{
    return wrapped_->at(~sfTicketCount);
}

void
AcctRoot::setTicketCount(std::uint32_t newTicketCount)
{
    setOptional(sfTicketCount, newTicketCount);
}

void
AcctRoot::clearTicketCount()
{
    clearOptional(sfTicketCount);
}

std::pair<AcctRoot const, NotTEC>
makeAcctRootRd(std::shared_ptr<STLedgerEntry const> slePtr)
{
    using R = std::pair<AcctRoot, NotTEC>;
    if (!slePtr)
        return R{AcctRoot{nullptr}, terNO_ACCOUNT};

    std::uint16_t const type = {slePtr->at(sfLedgerEntryType)};
    assert(type == ltACCOUNT_ROOT);
    if (type != ltACCOUNT_ROOT)
        return R{AcctRoot{nullptr}, tefINTERNAL};

    return R{AcctRoot(std::move(slePtr)), tesSUCCESS};
}

std::pair<AcctRoot, NotTEC>
makeAcctRoot(std::shared_ptr<STLedgerEntry> slePtr)
{
    using R = std::pair<AcctRoot, NotTEC>;
    if (!slePtr)
        return R{AcctRoot{nullptr}, terNO_ACCOUNT};

    std::uint16_t const type = {slePtr->at(sfLedgerEntryType)};
    assert(type == ltACCOUNT_ROOT);
    if (type != ltACCOUNT_ROOT)
        return R{AcctRoot{nullptr}, tefINTERNAL};

    return R{AcctRoot(std::move(slePtr)), tesSUCCESS};
}

}  // namespace ripple
