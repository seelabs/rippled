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

#ifndef RIPPLE_PROTOCOL_STLEDGERENTRY_H_INCLUDED
#define RIPPLE_PROTOCOL_STLEDGERENTRY_H_INCLUDED

#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STObject.h>

namespace ripple {

class Invariants_test;

// This class should only be used within STLedgerEntry constructors.
// The `val` member will be moved from after the constructor runs.
template <class T0, class T1>
struct SLEKV
{
    TypedField<T0> const& key;
    T1 val;
    SLEKV(TypedField<T0> const& k, T1 const& v) : key{k}, val{v}
    {
    }
    SLEKV(TypedField<T0> const& k, T1&& v) : key{k}, val{std::move(v)}
    {
    }
};

class STLedgerEntry final : public STObject, public CountedObject<STLedgerEntry>
{
    friend Invariants_test;  // this test wants access to the private type_

public:
    static char const*
    getCountedObjectName()
    {
        return "STLedgerEntry";
    }

    /** Create an empty object with the given key and type. */
    explicit STLedgerEntry(Keylet const& k);

    template <class T0, class T1, class... Ts>
    void
    populate(SLEKV<T0, T1>&& kv, Ts&&... ts)
    {
        (*this)[kv.key] = std::move(kv.val);
        if constexpr (sizeof...(ts) > 0)
        {
            populate(std::forward<Ts>(ts)...);
        }
    }

    template <class T0, class T1, class... Ts>
    STLedgerEntry(Keylet const& k, SLEKV<T0, T1>&& kv, Ts&&... ts)
        : STLedgerEntry(k)
    {
        populate(std::move(kv), std::forward<Ts>(ts)...);
    }

    [[deprecated("Prefer using a keylet instead")]] STLedgerEntry(
        LedgerEntryType type,
        uint256 const& key)
        : STLedgerEntry(Keylet(type, key))
    {
    }

    STLedgerEntry(SerialIter& sit, uint256 const& index);
    STLedgerEntry(SerialIter&& sit, uint256 const& index)
        : STLedgerEntry(sit, index)
    {
    }

    STLedgerEntry(STObject const& object, uint256 const& index);

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

    SerializedTypeID
    getSType() const override
    {
        return STI_LEDGERENTRY;
    }

    std::string
    getFullText() const override;

    std::string
    getText() const override;

    Json::Value
    getJson(JsonOptions options) const override;

    /** Returns the 'key' (or 'index') of this item.
        The key identifies this entry's position in
        the SHAMap associative container.
    */
    uint256 const&
    key() const
    {
        return key_;
    }

    LedgerEntryType
    getType() const
    {
        return type_;
    }

    // is this a ledger entry that can be threaded
    bool
    isThreadedType() const;

    bool
    thread(
        uint256 const& txID,
        std::uint32_t ledgerSeq,
        uint256& prevTxID,
        std::uint32_t& prevLedgerID);

private:
    /*  Make STObject comply with the template for this SLE type
        Can throw
    */
    void
    setSLEType();

private:
    uint256 key_;
    LedgerEntryType type_;
};

using SLE = STLedgerEntry;

}  // namespace ripple

#endif
