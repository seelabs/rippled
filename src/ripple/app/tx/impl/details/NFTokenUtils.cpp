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

#include <ripple/app/tx/impl/details/NFTokenUtils.h>
#include <ripple/basics/algorithm.h>
#include <ripple/ledger/Directory.h>
#include <ripple/ledger/View.h>
#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/STArray.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/nftPageMask.h>
#include <functional>
#include <memory>

namespace ripple {

namespace nft {

static std::shared_ptr<SLE const>
locatePage(ReadView const& view, AccountID owner, uint256 const& id)
{
    auto const first = keylet::nftpage(keylet::nftpage_min(owner), id);
    auto const last = keylet::nftpage_max(owner);

    // This NFT can only be found in the first page with a key that's strictly
    // greater than `first`, so look for that, up until the maximum possible
    // page.
    return view.read(Keylet(
        ltNFTOKEN_PAGE,
        view.succ(first.key, last.key.next()).value_or(last.key)));
}

static std::shared_ptr<SLE>
locatePage(ApplyView& view, AccountID owner, uint256 const& id)
{
    auto const first = keylet::nftpage(keylet::nftpage_min(owner), id);
    auto const last = keylet::nftpage_max(owner);

    // This NFT can only be found in the first page with a key that's strictly
    // greater than `first`, so look for that, up until the maximum possible
    // page.
    return view.peek(Keylet(
        ltNFTOKEN_PAGE,
        view.succ(first.key, last.key.next()).value_or(last.key)));
}

static std::shared_ptr<SLE>
getPageForToken(
    ApplyView& view,
    AccountID const& owner,
    uint256 const& id,
    std::function<void(ApplyView&, AccountID const&)> const& createCallback)
{
    auto const base = keylet::nftpage_min(owner);
    auto const first = keylet::nftpage(base, id);
    auto const last = keylet::nftpage_max(owner);

    // This NFT can only be found in the first page with a key that's strictly
    // greater than `first`, so look for that, up until the maximum possible
    // page.
    auto cp = view.peek(Keylet(
        ltNFTOKEN_PAGE,
        view.succ(first.key, last.key.next()).value_or(last.key)));

    // A suitable page doesn't exist; we'll have to create one.
    if (!cp)
    {
        STArray arr;
        cp = std::make_shared<SLE>(last);
        cp->setFieldArray(sfNonFungibleTokens, arr);
        view.insert(cp);
        createCallback(view, owner);
        return cp;
    }

    STArray narr = cp->getFieldArray(sfNonFungibleTokens);

    // The right page still has space: we're good.
    if (narr.size() != dirMaxTokensPerPage)
        return cp;

    // We need to split the page in two: the first half of the items in this
    // page will go into the new page; the rest will stay with the existing
    // page.
    //
    // Note we can't always split the page exactly in half.  All equivalent
    // NFTs must be kept on the same page.  So when the page contains
    // equivalent NFTs, the split may be lopsided in order to keep equivalent
    // NFTs on the same page.
    STArray carr;
    {
        // We prefer to keep equivalent NFTs on a page boundary.  That gives
        // any additional equivalent NFTs maximum room for expansion.
        // Round up the boundary until there's a non-equivalent entry.
        uint256 const cmp =
            narr[(dirMaxTokensPerPage / 2) - 1].getFieldH256(sfTokenID) &
            nft::pageMask;

        // Note that the calls to find_if_not() and (later) find_if()
        // rely on the fact that narr is kept in sorted order.
        auto splitIter = std::find_if_not(
            narr.begin() + (dirMaxTokensPerPage / 2),
            narr.end(),
            [&cmp](STObject const& obj) {
                return (obj.getFieldH256(sfTokenID) & nft::pageMask) == cmp;
            });

        // If we get all the way from the middle to the end with only
        // equivalent NFTokens then check the front of the page for a
        // place to make the split.
        if (splitIter == narr.end())
            splitIter = std::find_if(
                narr.begin(), narr.end(), [&cmp](STObject const& obj) {
                    return (obj.getFieldH256(sfTokenID) & nft::pageMask) == cmp;
                });

        // If splitIter == begin(), then the entire page is filled with
        // equivalent tokens.  We cannot split the page, so we cannot
        // insert the requested token.
        //
        // There should be no circumstance when splitIter == end(), but if it
        // were to happen we should bail out because something is confused.
        if (splitIter == narr.begin() || splitIter == narr.end())
            return nullptr;

        // Split narr at splitIter.
        STArray newCarr(
            std::make_move_iterator(splitIter),
            std::make_move_iterator(narr.end()));
        narr.erase(splitIter, narr.end());
        std::swap(carr, newCarr);
    }

    auto np = std::make_shared<SLE>(
        keylet::nftpage(base, carr[0].getFieldH256(sfTokenID)));
    np->setFieldArray(sfNonFungibleTokens, narr);
    np->setFieldH256(sfNextPageMin, cp->key());

    if (auto ppm = (*cp)[~sfPreviousPageMin])
    {
        np->setFieldH256(sfPreviousPageMin, *ppm);

        if (auto p3 = view.peek(Keylet(ltNFTOKEN_PAGE, *ppm)))
        {
            p3->setFieldH256(sfNextPageMin, np->key());
            view.update(p3);
        }
    }

    view.insert(np);

    cp->setFieldArray(sfNonFungibleTokens, carr);
    cp->setFieldH256(sfPreviousPageMin, np->key());
    view.update(cp);

    createCallback(view, owner);

    return (first.key <= np->key()) ? np : cp;
}

static bool
compareTokens(uint256 const& a, uint256 const& b)
{
    return (a & nft::pageMask) < (b & nft::pageMask);
}

/** Insert the token in the owner's token directory. */
TER
insertToken(ApplyView& view, AccountID owner, STObject nft)
{
    assert(nft.isFieldPresent(sfTokenID));

    // First, we need to locate the page the NFT belongs to, creating it
    // if necessary. This operation may fail if it is impossible to insert
    // the NFT.
    std::shared_ptr<SLE> page = getPageForToken(
        view,
        owner,
        nft[sfTokenID],
        [](ApplyView& view, AccountID const& owner) {
            adjustOwnerCount(
                view,
                view.peek(keylet::account(owner)),
                1,
                beast::Journal{beast::Journal::getNullSink()});
        });

    if (!page)
        return tecNO_SUITABLE_PAGE;

    {
        auto arr = page->getFieldArray(sfNonFungibleTokens);
        arr.push_back(nft);

        arr.sort([](STObject const& o1, STObject const& o2) {
            return compareTokens(
                o1.getFieldH256(sfTokenID), o2.getFieldH256(sfTokenID));
        });

        page->setFieldArray(sfNonFungibleTokens, arr);
    }

    view.update(page);

    return tesSUCCESS;
}

static bool
mergePages(
    ApplyView& view,
    std::shared_ptr<SLE> const& p1,
    std::shared_ptr<SLE> const& p2)
{
    if (p1->key() >= p2->key())
        Throw<std::runtime_error>("mergePages: pages passed in out of order!");

    if ((*p1)[~sfNextPageMin] != p2->key())
        Throw<std::runtime_error>("mergePages: next link broken!");

    if ((*p2)[~sfPreviousPageMin] != p1->key())
        Throw<std::runtime_error>("mergePages: previous link broken!");

    auto const p1arr = p1->getFieldArray(sfNonFungibleTokens);
    auto const p2arr = p2->getFieldArray(sfNonFungibleTokens);

    // Now check whether to merge the two pages; it only makes sense to do
    // this it would mean that one of them can be deleted as a result of
    // the merge.

    if (p1arr.size() + p2arr.size() > dirMaxTokensPerPage)
        return false;

    STArray x(p1arr.size() + p2arr.size());

    std::merge(
        p1arr.begin(),
        p1arr.end(),
        p2arr.begin(),
        p2arr.end(),
        std::back_inserter(x),
        [](STObject const& a, STObject const& b) {
            return compareTokens(
                a.getFieldH256(sfTokenID), b.getFieldH256(sfTokenID));
        });

    p2->setFieldArray(sfNonFungibleTokens, x);

    // So, at this point we need to unlink "p1" (since we just emptied it) but
    // we need to first relink the directory: if p1 has a previous page (p0),
    // load it, point it to p2 and point p2 to it.

    p2->makeFieldAbsent(sfPreviousPageMin);

    if (auto const ppm = (*p1)[~sfPreviousPageMin])
    {
        auto p0 = view.peek(Keylet(ltNFTOKEN_PAGE, *ppm));

        if (!p0)
            Throw<std::runtime_error>("mergePages: p0 can't be located!");

        p0->setFieldH256(sfNextPageMin, p2->key());
        view.update(p0);

        p2->setFieldH256(sfPreviousPageMin, *ppm);
    }

    view.update(p2);
    view.erase(p1);

    return true;
}

/** Remove the token from the owner's token directory. */
TER
removeToken(ApplyView& view, AccountID const& owner, uint256 const& tokenID)
{
    auto curr = locatePage(view, owner, tokenID);

    // If the page couldn't be found, the given NFT isn't owned by this account
    if (!curr)
        return tecNO_ENTRY;

    // We found a page, but the given NFT may not be in it.
    auto arr = curr->getFieldArray(sfNonFungibleTokens);

    {
        auto x = std::find_if(
            arr.begin(), arr.end(), [&tokenID](STObject const& obj) {
                return (obj[sfTokenID] == tokenID);
            });

        if (x == arr.end())
            return tecNO_ENTRY;

        arr.erase(x);
    }

    // Page management:
    auto const loadPage = [&view](
                              std::shared_ptr<SLE> const& page1,
                              SF_UINT256 const& field) {
        std::shared_ptr<SLE> page2;

        if (auto const id = (*page1)[~field])
        {
            page2 = view.peek(Keylet(ltNFTOKEN_PAGE, *id));

            if (!page2)
                Throw<std::runtime_error>(
                    "page " + to_string(page1->key()) + " has a broken " +
                    field.getName() + " field pointing to " + to_string(*id));
        }

        return page2;
    };

    auto const prev = loadPage(curr, sfPreviousPageMin);
    auto const next = loadPage(curr, sfNextPageMin);

    if (!arr.empty())
    {
        // The current page isn't empty. Update it and then try to consolidate
        // pages. Note that this consolidation attempt may actually merge three
        // pages into one!
        curr->setFieldArray(sfNonFungibleTokens, arr);
        view.update(curr);

        int cnt = 0;

        if (prev && mergePages(view, prev, curr))
            cnt--;

        if (next && mergePages(view, curr, next))
            cnt--;

        if (cnt != 0)
            adjustOwnerCount(
                view,
                view.peek(keylet::account(owner)),
                cnt,
                beast::Journal{beast::Journal::getNullSink()});

        return tesSUCCESS;
    }

    // The page is empty, so we can just unlink it and then remove it.
    if (prev)
    {
        // Make our previous page point to our next page:
        if (next)
            prev->setFieldH256(sfNextPageMin, next->key());
        else
            prev->makeFieldAbsent(sfNextPageMin);

        view.update(prev);
    }

    if (next)
    {
        // Make our next page point to our previous page:
        if (prev)
            next->setFieldH256(sfPreviousPageMin, prev->key());
        else
            next->makeFieldAbsent(sfPreviousPageMin);

        view.update(next);
    }

    view.erase(curr);

    int cnt = 1;

    // Since we're here, try to consolidate the previous and current pages
    // of the page we removed (if any) into one.  mergePages() _should_
    // always return false.  Since tokens are burned one at a time, there
    // should never be a page containing one token sitting between two pages
    // that have few enough tokens that they can be merged.
    //
    // But, in case that analysis is wrong, it's good to leave this code here
    // just in case.
    if (prev && next &&
        mergePages(
            view,
            view.peek(Keylet(ltNFTOKEN_PAGE, prev->key())),
            view.peek(Keylet(ltNFTOKEN_PAGE, next->key()))))
        cnt++;

    adjustOwnerCount(
        view,
        view.peek(keylet::account(owner)),
        -1 * cnt,
        beast::Journal{beast::Journal::getNullSink()});

    return tesSUCCESS;
}

std::optional<STObject>
findToken(ReadView const& view, AccountID const& owner, uint256 const& tokenID)
{
    auto page = locatePage(view, owner, tokenID);

    // If the page couldn't be found, the given NFT isn't owned by this account
    if (!page)
        return std::nullopt;

    // We found a candidate page, but the given NFT may not be in it.
    for (auto const& t : page->getFieldArray(sfNonFungibleTokens))
    {
        if (t[sfTokenID] == tokenID)
            return t;
    }

    return std::nullopt;
}

void
removeAllTokenOffers(ApplyView& view, Keylet const& directory)
{
    view.dirDelete(directory, [&view](uint256 const& id) {
        auto offer = view.peek(Keylet{ltNFTOKEN_OFFER, id});

        if (!offer)
            Throw<std::runtime_error>(
                "Offer " + to_string(id) + " not found in ledger!");

        auto const owner = (*offer)[sfOwner];

        if (!view.dirRemove(
                keylet::ownerDir(owner),
                (*offer)[sfOwnerNode],
                offer->key(),
                false))
            Throw<std::runtime_error>(
                "Offer " + to_string(id) + " not found in owner directory!");

        adjustOwnerCount(
            view,
            view.peek(keylet::account(owner)),
            -1,
            beast::Journal{beast::Journal::getNullSink()});

        view.erase(offer);
    });
}

bool
deleteTokenOffer(ApplyView& view, std::shared_ptr<SLE> const& offer)
{
    if (offer->getType() != ltNFTOKEN_OFFER)
        return false;

    auto const owner = (*offer)[sfOwner];

    if (!view.dirRemove(
            keylet::ownerDir(owner),
            (*offer)[sfOwnerNode],
            offer->key(),
            false))
        return false;

    auto const tokenID = (*offer)[sfTokenID];

    if (!view.dirRemove(
            ((*offer)[sfFlags] & tfSellToken) ? keylet::nft_sells(tokenID)
                                              : keylet::nft_buys(tokenID),
            (*offer)[sfOfferNode],
            offer->key(),
            false))
        return false;

    adjustOwnerCount(
        view,
        view.peek(keylet::account(owner)),
        -1,
        beast::Journal{beast::Journal::getNullSink()});

    view.erase(offer);
    return true;
}

}  // namespace nft
}  // namespace ripple
