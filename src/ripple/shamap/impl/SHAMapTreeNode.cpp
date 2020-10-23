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

#include <ripple/basics/Log.h>
#include <ripple/basics/Slice.h>
#include <ripple/basics/contract.h>
#include <ripple/basics/safe_cast.h>
#include <ripple/beast/core/LexicalCast.h>
#include <ripple/protocol/HashPrefix.h>
#include <ripple/protocol/digest.h>
#include <ripple/shamap/SHAMapTreeNode.h>
#include <mutex>

#include <openssl/sha.h>

namespace ripple {

// These are wire-protocol identifiers used during serialization to encode the
// type of a node. They should not be arbitrarily be changed.
static constexpr unsigned char const wireTypeTransaction = 0;
static constexpr unsigned char const wireTypeAccountState = 1;
static constexpr unsigned char const wireTypeInner = 2;
static constexpr unsigned char const wireTypeCompressedInner = 3;
static constexpr unsigned char const wireTypeTransactionWithMeta = 4;

std::mutex SHAMapInnerNode::childLock;

std::shared_ptr<SHAMapAbstractNode>
SHAMapInnerNode::clone(std::uint32_t owner) const
{
    auto p = std::make_shared<SHAMapInnerNode>(owner);
    p->hash_ = hash_;
    p->mIsBranch = mIsBranch;
    p->mFullBelowGen = mFullBelowGen;
    p->mHashes = mHashes;
    std::lock_guard lock(childLock);
    for (int i = 0; i < 16; ++i)
        p->mChildren[i] = mChildren[i];
    return p;
}

SHAMapTreeNode::SHAMapTreeNode(
    std::shared_ptr<SHAMapItem const> item,
    std::uint32_t owner)
    : SHAMapAbstractNode(owner), item_(std::move(item))
{
    assert(item_->peekData().size() >= 12);
}

SHAMapTreeNode::SHAMapTreeNode(
    std::shared_ptr<SHAMapItem const> item,
    std::uint32_t owner,
    SHAMapHash const& hash)
    : SHAMapAbstractNode(owner, hash), item_(std::move(item))
{
    assert(item_->peekData().size() >= 12);
}

std::shared_ptr<SHAMapAbstractNode>
SHAMapAbstractNode::makeTransaction(
    Slice data,
    std::uint32_t seq,
    SHAMapHash const& hash,
    bool hashValid)
{
    // FIXME: using a Serializer results in a copy; avoid it?
    Serializer s(data.begin(), data.size());

    auto item = std::make_shared<SHAMapItem const>(
        sha512Half(HashPrefix::transactionID, data), s);

    if (hashValid)
        return std::make_shared<SHAMapTxLeafNode>(std::move(item), seq, hash);

    return std::make_shared<SHAMapTxLeafNode>(std::move(item), seq);
}

std::shared_ptr<SHAMapAbstractNode>
SHAMapAbstractNode::makeTransactionWithMeta(
    Slice data,
    std::uint32_t seq,
    SHAMapHash const& hash,
    bool hashValid)
{
    Serializer s(data.data(), data.size());

    uint256 tag;

    if (s.size() < tag.bytes)
        Throw<std::runtime_error>("Short TXN+MD node");

    // FIXME: improve this interface so that the above check isn't needed
    if (!s.getBitString(tag, s.size() - tag.bytes))
        Throw<std::out_of_range>(
            "Short TXN+MD node (" + std::to_string(s.size()) + ")");

    s.chop(tag.bytes);

    auto item = std::make_shared<SHAMapItem const>(tag, s.peekData());

    if (hashValid)
        return std::make_shared<SHAMapTxPlusMetaLeafNode>(
            std::move(item), seq, hash);

    return std::make_shared<SHAMapTxPlusMetaLeafNode>(std::move(item), seq);
}

std::shared_ptr<SHAMapAbstractNode>
SHAMapAbstractNode::makeAccountState(
    Slice data,
    std::uint32_t seq,
    SHAMapHash const& hash,
    bool hashValid)
{
    Serializer s(data.data(), data.size());

    uint256 tag;

    if (s.size() < tag.bytes)
        Throw<std::runtime_error>("short AS node");

    // FIXME: improve this interface so that the above check isn't needed
    if (!s.getBitString(tag, s.size() - tag.bytes))
        Throw<std::out_of_range>(
            "Short AS node (" + std::to_string(s.size()) + ")");

    s.chop(tag.bytes);

    if (tag.isZero())
        Throw<std::runtime_error>("Invalid AS node");

    auto item = std::make_shared<SHAMapItem const>(tag, s.peekData());

    if (hashValid)
        return std::make_shared<SHAMapAccountStateLeafNode>(
            std::move(item), seq, hash);

    return std::make_shared<SHAMapAccountStateLeafNode>(std::move(item), seq);
}

std::shared_ptr<SHAMapAbstractNode>
SHAMapInnerNode::makeFullInner(
    Slice data,
    std::uint32_t seq,
    SHAMapHash const& hash,
    bool hashValid)
{
    if (data.size() != 512)
        Throw<std::runtime_error>("Invalid FI node");

    auto ret = std::make_shared<SHAMapInnerNode>(seq);

    Serializer s(data.data(), data.size());

    for (int i = 0; i < 16; ++i)
    {
        s.getBitString(ret->mHashes[i].as_uint256(), i * 32);

        if (ret->mHashes[i].isNonZero())
            ret->mIsBranch |= (1 << i);
    }

    if (hashValid)
        ret->hash_ = hash;
    else
        ret->updateHash();
    return ret;
}

std::shared_ptr<SHAMapAbstractNode>
SHAMapInnerNode::makeCompressedInner(Slice data, std::uint32_t seq)
{
    Serializer s(data.data(), data.size());

    int len = s.getLength();

    auto ret = std::make_shared<SHAMapInnerNode>(seq);

    for (int i = 0; i < (len / 33); ++i)
    {
        int pos;

        if (!s.get8(pos, 32 + (i * 33)))
            Throw<std::runtime_error>("short CI node");

        if ((pos < 0) || (pos >= 16))
            Throw<std::runtime_error>("invalid CI node");

        s.getBitString(ret->mHashes[pos].as_uint256(), i * 33);

        if (ret->mHashes[pos].isNonZero())
            ret->mIsBranch |= (1 << pos);
    }

    ret->updateHash();

    return ret;
}

std::shared_ptr<SHAMapAbstractNode>
SHAMapAbstractNode::makeFromWire(Slice rawNode)
{
    if (rawNode.empty())
        return {};

    auto const type = rawNode[rawNode.size() - 1];

    rawNode.remove_suffix(1);

    bool const hashValid = false;
    SHAMapHash const hash;

    std::uint32_t const seq = 0;

    if (type == wireTypeTransaction)
        return makeTransaction(rawNode, seq, hash, hashValid);

    if (type == wireTypeAccountState)
        return makeAccountState(rawNode, seq, hash, hashValid);

    if (type == wireTypeInner)
        return SHAMapInnerNode::makeFullInner(rawNode, seq, hash, hashValid);

    if (type == wireTypeCompressedInner)
        return SHAMapInnerNode::makeCompressedInner(rawNode, seq);

    if (type == wireTypeTransactionWithMeta)
        return makeTransactionWithMeta(rawNode, seq, hash, hashValid);

    Throw<std::runtime_error>(
        "wire: Unknown type (" + std::to_string(type) + ")");
}

std::shared_ptr<SHAMapAbstractNode>
SHAMapAbstractNode::makeFromPrefix(Slice rawNode, SHAMapHash const& hash)
{
    if (rawNode.size() < 4)
        Throw<std::runtime_error>("prefix: short node");

    // FIXME: Use SerialIter::get32?
    // Extract the prefix
    auto const type = safe_cast<HashPrefix>(
        (safe_cast<std::uint32_t>(rawNode[0]) << 24) +
        (safe_cast<std::uint32_t>(rawNode[1]) << 16) +
        (safe_cast<std::uint32_t>(rawNode[2]) << 8) +
        (safe_cast<std::uint32_t>(rawNode[3])));

    rawNode.remove_prefix(4);

    bool const hashValid = true;
    std::uint32_t const seq = 0;

    if (type == HashPrefix::transactionID)
        return makeTransaction(rawNode, seq, hash, hashValid);

    if (type == HashPrefix::leafNode)
        return makeAccountState(rawNode, seq, hash, hashValid);

    if (type == HashPrefix::innerNode)
        return SHAMapInnerNode::makeFullInner(rawNode, seq, hash, hashValid);

    if (type == HashPrefix::txNode)
        return makeTransactionWithMeta(rawNode, seq, hash, hashValid);

    Throw<std::runtime_error>(
        "prefix: unknown type (" +
        std::to_string(safe_cast<std::underlying_type_t<HashPrefix>>(type)) +
        ")");
}

void
SHAMapInnerNode::updateHash()
{
    uint256 nh;
    if (mIsBranch != 0)
    {
        sha512_half_hasher h;
        using beast::hash_append;
        hash_append(h, HashPrefix::innerNode);
        for (auto const& hh : mHashes)
            hash_append(h, hh);
        nh = static_cast<typename sha512_half_hasher::result_type>(h);
    }
    hash_ = SHAMapHash{nh};
}

void
SHAMapInnerNode::updateHashDeep()
{
    for (auto pos = 0; pos < 16; ++pos)
    {
        if (mChildren[pos] != nullptr)
            mHashes[pos] = mChildren[pos]->getHash();
    }
    updateHash();
}

void
SHAMapTxLeafNode::updateHash()
{
    hash_ = SHAMapHash{
        sha512Half(HashPrefix::transactionID, makeSlice(item_->peekData()))};
}

void
SHAMapTxPlusMetaLeafNode::updateHash()
{
    hash_ = SHAMapHash{sha512Half(
        HashPrefix::txNode, makeSlice(item_->peekData()), item_->key())};
}

void
SHAMapAccountStateLeafNode::updateHash()
{
    hash_ = SHAMapHash{sha512Half(
        HashPrefix::leafNode, makeSlice(item_->peekData()), item_->key())};
}

void
SHAMapInnerNode::serializeForWire(Serializer& s) const
{
    assert(!isEmpty());

    // If the node is sparse, then only send non-empty branches:
    if (getBranchCount() < 12)
    {
        // compressed node
        for (int i = 0; i < mHashes.size(); ++i)
        {
            if (!isEmptyBranch(i))
            {
                s.addBitString(mHashes[i].as_uint256());
                s.add8(i);
            }
        }

        s.add8(wireTypeCompressedInner);
    }
    else
    {
        for (auto const& hh : mHashes)
            s.addBitString(hh.as_uint256());

        s.add8(wireTypeInner);
    }
}

void
SHAMapInnerNode::serializeWithPrefix(Serializer& s) const
{
    assert(!isEmpty());

    s.add32(HashPrefix::innerNode);
    for (auto const& hh : mHashes)
        s.addBitString(hh.as_uint256());
}

void
SHAMapTxLeafNode::serializeForWire(Serializer& s) const
{
    s.addRaw(item_->peekData());
    s.add8(wireTypeTransaction);
}
void
SHAMapTxPlusMetaLeafNode::serializeForWire(Serializer& s) const
{
    s.addRaw(item_->peekData());
    s.addBitString(item_->key());
    s.add8(wireTypeTransactionWithMeta);
}
void
SHAMapAccountStateLeafNode::serializeForWire(Serializer& s) const
{
    s.addRaw(item_->peekData());
    s.addBitString(item_->key());
    s.add8(wireTypeAccountState);
}

void
SHAMapTxLeafNode::serializeWithPrefix(Serializer& s) const
{
    s.add32(HashPrefix::transactionID);
    s.addRaw(item_->peekData());
}

void
SHAMapTxPlusMetaLeafNode::serializeWithPrefix(Serializer& s) const
{
    s.add32(HashPrefix::txNode);
    s.addRaw(item_->peekData());
    s.addBitString(item_->key());
}

void
SHAMapAccountStateLeafNode::serializeWithPrefix(Serializer& s) const
{
    s.add32(HashPrefix::leafNode);
    s.addRaw(item_->peekData());
    s.addBitString(item_->key());
}

bool
SHAMapTreeNode::setItem(std::shared_ptr<SHAMapItem const> i)
{
    assert(cowid_ != 0);
    item_ = std::move(i);

    auto const oldHash = hash_;

    updateHash();

    return (oldHash != hash_);
}

bool
SHAMapInnerNode::isEmpty() const
{
    return mIsBranch == 0;
}

int
SHAMapInnerNode::getBranchCount() const
{
    int count = 0;

    for (int i = 0; i < 16; ++i)
        if (!isEmptyBranch(i))
            ++count;

    return count;
}

std::string
SHAMapAbstractNode::getString(const SHAMapNodeID& id) const
{
    return to_string(id);
}

std::string
SHAMapInnerNode::getString(const SHAMapNodeID& id) const
{
    std::string ret = SHAMapAbstractNode::getString(id);
    for (int i = 0; i < mHashes.size(); ++i)
    {
        if (!isEmptyBranch(i))
        {
            ret += "\n";
            ret += std::to_string(i);
            ret += " = ";
            ret += to_string(mHashes[i]);
        }
    }
    return ret;
}

std::string
SHAMapTreeNode::getString(const SHAMapNodeID& id) const
{
    std::string ret = SHAMapAbstractNode::getString(id);

    auto const type = getType();

    if (type == SHAMapNodeType::tnTRANSACTION_NM)
        ret += ",txn\n";
    else if (type == SHAMapNodeType::tnTRANSACTION_MD)
        ret += ",txn+md\n";
    else if (type == SHAMapNodeType::tnACCOUNT_STATE)
        ret += ",as\n";
    else
        ret += ",leaf\n";

    ret += "  Tag=";
    ret += to_string(peekItem()->key());
    ret += "\n  Hash=";
    ret += to_string(hash_);
    ret += "/";
    ret += std::to_string(item_->size());
    return ret;
}

// We are modifying an inner node
void
SHAMapInnerNode::setChild(
    int m,
    std::shared_ptr<SHAMapAbstractNode> const& child)
{
    assert((m >= 0) && (m < 16));
    assert(cowid_ != 0);
    assert(child.get() != this);
    mHashes[m].zero();
    hash_.zero();
    if (child)
        mIsBranch |= (1 << m);
    else
        mIsBranch &= ~(1 << m);
    mChildren[m] = child;
}

// finished modifying, now make shareable
void
SHAMapInnerNode::shareChild(
    int m,
    std::shared_ptr<SHAMapAbstractNode> const& child)
{
    assert((m >= 0) && (m < 16));
    assert(cowid_ != 0);
    assert(child);
    assert(child.get() != this);

    mChildren[m] = child;
}

SHAMapAbstractNode*
SHAMapInnerNode::getChildPointer(int branch)
{
    assert(branch >= 0 && branch < 16);

    std::lock_guard lock(childLock);
    return mChildren[branch].get();
}

std::shared_ptr<SHAMapAbstractNode>
SHAMapInnerNode::getChild(int branch)
{
    assert(branch >= 0 && branch < 16);

    std::lock_guard lock(childLock);
    return mChildren[branch];
}

std::shared_ptr<SHAMapAbstractNode>
SHAMapInnerNode::canonicalizeChild(
    int branch,
    std::shared_ptr<SHAMapAbstractNode> node)
{
    assert(branch >= 0 && branch < 16);
    assert(node);
    assert(node->getHash() == mHashes[branch]);

    std::lock_guard lock(childLock);
    if (mChildren[branch])
    {
        // There is already a node hooked up, return it
        node = mChildren[branch];
    }
    else
    {
        // Hook this node up
        mChildren[branch] = node;
    }
    return node;
}

void
SHAMapInnerNode::invariants(bool is_root) const
{
    unsigned count = 0;
    for (int i = 0; i < 16; ++i)
    {
        if (mHashes[i].isNonZero())
        {
            assert((mIsBranch & (1 << i)) != 0);
            if (mChildren[i] != nullptr)
                mChildren[i]->invariants();
            ++count;
        }
        else
        {
            assert((mIsBranch & (1 << i)) == 0);
        }
    }
    if (!is_root)
    {
        assert(hash_.isNonZero());
        assert(count >= 1);
    }
    assert((count == 0) ? hash_.isZero() : hash_.isNonZero());
}

void
SHAMapTreeNode::invariants(bool) const
{
    assert(hash_.isNonZero());
    assert(item_ != nullptr);
}

}  // namespace ripple
