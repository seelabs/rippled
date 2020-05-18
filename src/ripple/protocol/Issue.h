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

#ifndef RIPPLE_PROTOCOL_ISSUE_H_INCLUDED
#define RIPPLE_PROTOCOL_ISSUE_H_INCLUDED

#include <boost/optional.hpp>

#include <cassert>
#include <functional>
#include <type_traits>

#include <ripple/protocol/UintTypes.h>

namespace ripple {

enum class AssetType { xrp, iou, stable_coin };

inline static std::string const StableCoinHashSuffix{"SC"};

/** A currency issued by an account.
    @see Currency, AccountID, Issue, Book
*/
class Issue
{
    boost::optional<AssetType> assetType_;
    Currency currency_;
    AccountID account_;

    void
    updateAssetType()
    {
        assert(assetType_ != AssetType::stable_coin);
        if (isXRP(currency_))
            assetType_ = AssetType::xrp;
        else
            assetType_ = AssetType::iou;
    }

public:
    Issue()
    {
    }

    Issue(Currency const& c, AccountID const& a) : currency_(c), account_(a)
    {
        updateAssetType();
    }

    Issue(Currency const& c, AccountID const& a, AssetType t) : Issue(c, a)
    {
        assetType_ = t;
    }

    Currency const&
    currency() const
    {
        return currency_;
    }
    AccountID const&
    account() const
    {
        return account_;
    }

    void
    setAccount(AccountID const& a)
    {
        account_ = a;
        updateAssetType();
    }

    void
    setCurrency(Currency const& c)
    {
        currency_ = c;
        updateAssetType();
    }

    void
    setAssetType(AssetType a)
    {
        assetType_ = a;
    }

    AssetType
    assetType() const;

    bool
    isStableCoin() const
    {
        return assetType() == AssetType::stable_coin;
    }

    friend bool
    isConsistent(Issue const& ac);
};

std::string
to_string(Issue const& ac);

std::ostream&
operator<<(std::ostream& os, Issue const& x);

template <class Hasher>
void
hash_append(Hasher& h, Issue const& r)
{
    using beast::hash_append;
    if (!r.isStableCoin())
        hash_append(h, r.currency(), r.account());
    else
        hash_append(h, r.currency(), r.account(), StableCoinHashSuffix);
}

/** Ordered comparison.
    The assets are ordered first by currency and then by account,
    if the currency is not XRP.
*/
int
compare(Issue const& lhs, Issue const& rhs);

/** Equality comparison. */
/** @{ */
bool
operator==(Issue const& lhs, Issue const& rhs);
bool
operator!=(Issue const& lhs, Issue const& rhs);
/** @} */

/** Strict weak ordering. */
/** @{ */
bool
operator<(Issue const& lhs, Issue const& rhs);
bool
operator>(Issue const& lhs, Issue const& rhs);
bool
operator>=(Issue const& lhs, Issue const& rhs);
bool
operator<=(Issue const& lhs, Issue const& rhs);
/** @} */

//------------------------------------------------------------------------------

/** Returns an asset specifier that represents XRP. */
inline Issue const&
xrpIssue()
{
    static Issue issue{xrpCurrency(), xrpAccount()};
    return issue;
}

/** Returns an asset specifier that represents no account and currency. */
inline Issue const&
noIssue()
{
    static Issue issue{noCurrency(), noAccount()};
    return issue;
}

}  // namespace ripple

#endif
