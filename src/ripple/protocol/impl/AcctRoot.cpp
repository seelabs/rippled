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

template <bool Writable>
Blob
AcctRootWrapper<Writable>::getOptionalVL(SF_VL const& field) const
{
    Blob ret;
    if (wrapped_->isFieldPresent(field))
        ret = wrapped_->getFieldVL(field);
    return ret;
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setOrClearVLIfEmpty(
    SF_VL const& field,
    Blob const& value)
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

template <bool Writable>
AcctRootWrapper<Writable>::AcctRootWrapper(wrapped_t&& w)
    : wrapped_(std::move(w))
{
}

template <bool Writable>
auto
AcctRootWrapper<Writable>::slePtr() const -> wrapped_t const&
{
    return wrapped_;
}

template <bool Writable>
AccountID
AcctRootWrapper<Writable>::accountID() const
{
    return wrapped_->at(sfAccount);
}

template <bool Writable>
std::uint32_t
AcctRootWrapper<Writable>::flags() const
{
    return wrapped_->at(sfFlags);
}

template <bool Writable>
bool
AcctRootWrapper<Writable>::isFlag(std::uint32_t flagsToCheck) const
{
    return (flags() & flagsToCheck) == flagsToCheck;
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::replaceAllFlags(std::uint32_t newFlags)
{
    wrapped_->at(sfFlags) = newFlags;
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setFlag(std::uint32_t flagsToSet)
{
    replaceAllFlags(flags() | flagsToSet);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::clearFlag(std::uint32_t flagsToClear)
{
    replaceAllFlags(flags() & ~flagsToClear);
}

template <bool Writable>
std::uint32_t
AcctRootWrapper<Writable>::sequence() const
{
    return wrapped_->at(sfSequence);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setSequence(std::uint32_t seq)
{
    wrapped_->at(sfSequence) = seq;
}

template <bool Writable>
STAmount
AcctRootWrapper<Writable>::balance() const
{
    return wrapped_->at(sfBalance);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setBalance(STAmount const& amount)
{
    wrapped_->at(sfBalance) = amount;
}

template <bool Writable>
std::uint32_t
AcctRootWrapper<Writable>::ownerCount() const
{
    return wrapped_->at(sfOwnerCount);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setOwnerCount(std::uint32_t newCount)
{
    wrapped_->at(sfOwnerCount) = newCount;
}

template <bool Writable>
std::uint32_t
AcctRootWrapper<Writable>::previousTxnID() const
{
    return wrapped_->at(sfOwnerCount);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setPreviousTxnID(uint256 prevTxID)
{
    wrapped_->at(sfPreviousTxnID) = prevTxID;
}

template <bool Writable>
std::uint32_t
AcctRootWrapper<Writable>::previousTxnLgrSeq() const
{
    return wrapped_->at(sfPreviousTxnLgrSeq);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setPreviousTxnLgrSeq(std::uint32_t prevTxLgrSeq)
{
    wrapped_->at(sfPreviousTxnLgrSeq) = prevTxLgrSeq;
}

template <bool Writable>
std::optional<uint256>
AcctRootWrapper<Writable>::accountTxnID() const
{
    return wrapped_->at(~sfAccountTxnID);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setAccountTxnID(uint256 const& newAcctTxnID)
{
    setOptional(sfAccountTxnID, newAcctTxnID);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::clearAccountTxnID()
{
    clearOptional(sfAccountTxnID);
}

template <bool Writable>
std::optional<AccountID>
AcctRootWrapper<Writable>::regularKey() const
{
    return wrapped_->at(~sfRegularKey);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setRegularKey(AccountID const& newRegKey)
{
    setOptional(sfRegularKey, newRegKey);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::clearRegularKey()
{
    clearOptional(sfRegularKey);
}

template <bool Writable>
std::optional<uint128>
AcctRootWrapper<Writable>::emailHash() const
{
    return wrapped_->at(~sfEmailHash);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setEmailHash(uint128 const& newEmailHash)
{
    setOptional(sfEmailHash, newEmailHash);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::clearEmailHash()
{
    clearOptional(sfEmailHash);
}

template <bool Writable>
std::optional<uint256>
AcctRootWrapper<Writable>::walletLocator() const
{
    return wrapped_->at(~sfWalletLocator);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setWalletLocator(uint256 const& newWalletLocator)
{
    setOptional(sfWalletLocator, newWalletLocator);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::clearWalletLocator()
{
    clearOptional(sfWalletLocator);
}

template <bool Writable>
std::optional<std::uint32_t>
AcctRootWrapper<Writable>::walletSize()
{
    return wrapped_->at(~sfWalletSize);
}

template <bool Writable>
Blob
AcctRootWrapper<Writable>::messageKey() const
{
    return getOptionalVL(sfMessageKey);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setMessageKey(Blob const& newMessageKey)
{
    setOrClearVLIfEmpty(sfMessageKey, newMessageKey);
}

template <bool Writable>
std::optional<std::uint32_t>
AcctRootWrapper<Writable>::transferRate() const
{
    return wrapped_->at(~sfTransferRate);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setTransferRate(std::uint32_t newTransferRate)
{
    setOptional(sfTransferRate, newTransferRate);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::clearTransferRate()
{
    clearOptional(sfTransferRate);
}

template <bool Writable>
Blob
AcctRootWrapper<Writable>::domain() const
{
    return getOptionalVL(sfDomain);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setDomain(Blob const& newDomain)
{
    setOrClearVLIfEmpty(sfDomain, newDomain);
}

template <bool Writable>
std::optional<std::uint8_t>
AcctRootWrapper<Writable>::tickSize() const
{
    return wrapped_->at(sfTickSize);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setTickSize(std::uint8_t newTickSize)
{
    setOptional(sfTickSize, newTickSize);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::clearTickSize()
{
    clearOptional(sfTickSize);
}

template <bool Writable>
std::optional<std::uint32_t>
AcctRootWrapper<Writable>::ticketCount() const
{
    return wrapped_->at(~sfTicketCount);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::setTicketCount(std::uint32_t newTicketCount)
{
    setOptional(sfTicketCount, newTicketCount);
}

template <>
template <bool W>
std::enable_if_t<W>
AcctRootWrapper<true>::clearTicketCount()
{
    clearOptional(sfTicketCount);
}

tl::expected<AcctRootRd, NotTEC>
makeAcctRootRd(std::shared_ptr<STLedgerEntry const> slePtr)
{
    if (!slePtr)
        return tl::unexpected(terNO_ACCOUNT);

    std::uint16_t const type = {slePtr->at(sfLedgerEntryType)};
    assert(type == ltACCOUNT_ROOT);
    if (type != ltACCOUNT_ROOT)
        return tl::unexpected(tefINTERNAL);

    return AcctRootRd(std::move(slePtr));
}

tl::expected<AcctRoot, NotTEC>
makeAcctRoot(std::shared_ptr<STLedgerEntry> slePtr)
{
    if (!slePtr)
        return tl::unexpected(terNO_ACCOUNT);

    std::uint16_t const type = {slePtr->at(sfLedgerEntryType)};
    assert(type == ltACCOUNT_ROOT);
    if (type != ltACCOUNT_ROOT)
        return tl::unexpected(tefINTERNAL);

    return AcctRoot(std::move(slePtr));
}

template class AcctRootWrapper<true>;
template class AcctRootWrapper<false>;
template void
AcctRootWrapper<true>::setOrClearVLIfEmpty<true>(
    SF_VL const& field,
    Blob const& value);

template void
AcctRootWrapper<true>::replaceAllFlags<true>(std::uint32_t newFlags);

template void
AcctRootWrapper<true>::setFlag<true>(std::uint32_t flagsToSet);

template void
AcctRootWrapper<true>::clearFlag<true>(std::uint32_t flagsToClear);

template void
AcctRootWrapper<true>::setSequence<true>(std::uint32_t seq);

template void
AcctRootWrapper<true>::setBalance<true>(STAmount const& amount);

template void
AcctRootWrapper<true>::setOwnerCount<true>(std::uint32_t newCount);

template void
AcctRootWrapper<true>::setPreviousTxnID<true>(uint256 prevTxID);

template void
AcctRootWrapper<true>::setPreviousTxnLgrSeq<true>(std::uint32_t prevTxLgrSeq);

template void
AcctRootWrapper<true>::setAccountTxnID<true>(uint256 const& newAcctTxnID);

template void
AcctRootWrapper<true>::clearAccountTxnID<true>();

template void
AcctRootWrapper<true>::setRegularKey<true>(AccountID const& newRegKey);

template void
AcctRootWrapper<true>::clearRegularKey<true>();

template void
AcctRootWrapper<true>::setEmailHash<true>(uint128 const& newEmailHash);

template void
AcctRootWrapper<true>::clearEmailHash<true>();

template void
AcctRootWrapper<true>::setWalletLocator<true>(uint256 const& newWalletLocator);

template void
AcctRootWrapper<true>::clearWalletLocator<true>();

template void
AcctRootWrapper<true>::setMessageKey<true>(Blob const& newMessageKey);

template void
AcctRootWrapper<true>::setTransferRate<true>(std::uint32_t newTransferRate);

template void
AcctRootWrapper<true>::clearTransferRate<true>();

template void
AcctRootWrapper<true>::setDomain<true>(Blob const& newDomain);

template void
AcctRootWrapper<true>::setTickSize<true>(std::uint8_t newTickSize);

template void
AcctRootWrapper<true>::clearTickSize<true>();

template void
AcctRootWrapper<true>::setTicketCount<true>(std::uint32_t newTicketCount);

template void
AcctRootWrapper<true>::clearTicketCount<true>();

}  // namespace ripple
