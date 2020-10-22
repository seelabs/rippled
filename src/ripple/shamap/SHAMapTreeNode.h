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

#ifndef RIPPLE_SHAMAP_SHAMAPTREENODE_H_INCLUDED
#define RIPPLE_SHAMAP_SHAMAPTREENODE_H_INCLUDED

#include <ripple/basics/CountedObject.h>
#include <ripple/basics/TaggedCache.h>
#include <ripple/beast/utility/Journal.h>
#include <ripple/shamap/SHAMapItem.h>
#include <ripple/shamap/SHAMapNodeID.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace ripple {

// A SHAMapHash is the hash of a node in a SHAMap, and also the
// type of the hash of the entire SHAMap.
class SHAMapHash
{
    uint256 hash_;

public:
    SHAMapHash() = default;
    explicit SHAMapHash(uint256 const& hash) : hash_(hash)
    {
    }

    uint256 const&
    as_uint256() const
    {
        return hash_;
    }
    uint256&
    as_uint256()
    {
        return hash_;
    }
    bool
    isZero() const
    {
        return hash_.isZero();
    }
    bool
    isNonZero() const
    {
        return hash_.isNonZero();
    }
    int
    signum() const
    {
        return hash_.signum();
    }
    void
    zero()
    {
        hash_.zero();
    }

    friend bool
    operator==(SHAMapHash const& x, SHAMapHash const& y)
    {
        return x.hash_ == y.hash_;
    }

    friend bool
    operator<(SHAMapHash const& x, SHAMapHash const& y)
    {
        return x.hash_ < y.hash_;
    }

    friend std::ostream&
    operator<<(std::ostream& os, SHAMapHash const& x)
    {
        return os << x.hash_;
    }

    friend std::string
    to_string(SHAMapHash const& x)
    {
        return to_string(x.hash_);
    }

    template <class H>
    friend void
    hash_append(H& h, SHAMapHash const& x)
    {
        hash_append(h, x.hash_);
    }
};

inline bool
operator!=(SHAMapHash const& x, SHAMapHash const& y)
{
    return !(x == y);
}

enum class SHAMapNodeType {
    tnINNER = 1,
    tnTRANSACTION_NM = 2,  // transaction, no metadata
    tnTRANSACTION_MD = 3,  // transaction, with metadata
    tnACCOUNT_STATE = 4
};

class SHAMapAbstractNode
{
protected:
    SHAMapHash hash_;

    /** Determines the owning SHAMap, if any. Used for copy-on-write semantics.

        If this value is 0, the node is not dirty and does not need to be
        flushed. It is eligible for sharing and may be included multiple
        SHAMap instances.
     */
    std::uint32_t cowid_;

protected:
    SHAMapAbstractNode(SHAMapAbstractNode const&) = delete;
    SHAMapAbstractNode&
    operator=(SHAMapAbstractNode const&) = delete;

    explicit SHAMapAbstractNode(std::uint32_t seq) : cowid_(seq)
    {
    }

    explicit SHAMapAbstractNode(std::uint32_t seq, SHAMapHash const& hash)
        : hash_(hash), cowid_(seq)
    {
    }

public:
    virtual ~SHAMapAbstractNode() = default;

    /** \defgroup SHAMap Copy-on-Write Support

        By nature, a node may appear in multiple SHAMap instances. Rather than
        actually duplicating these nodes, SHAMap opts to be memory efficient
        and uses copy-on-write semantics for nodes.

        Only nodes that are not modified and don't need to be flushed back can
        be shared. Once a node needs to be changed, it must first be copied and
        the copy must marked as not shareable.

        Note that just because a node may not be *owned* by a given SHAMap
        instance does not mean that the node is NOT a part of any SHAMap. It
        only means that the node is not owned exclusively by any one SHAMap.

        For more on copy-on-write, check out:
            https://en.wikipedia.org/wiki/Copy-on-write
     */
    /** @{ */
    /** Returns the SHAMap that owns this node.

        @return the ID of the SHAMap that owns this node, or 0 if the node
                is not owned by any SHAMap and is a candidate for sharing.
     */
    std::uint32_t
    owner() const;

    /** Mark this node as shareable.

        Only nodes that are not modified and do not need to be flushed back
        should be marked as shareable.
     */
    void
    share();

    /** Make a copy of this node, setting the owner. */
    virtual std::shared_ptr<SHAMapAbstractNode>
    clone(std::uint32_t owner) const = 0;
    /** @} */

    /** Recalculate the hash of this node. */
    virtual void
    updateHash() = 0;

    /** Return the hash of this node. */
    SHAMapHash const&
    getHash() const;

    /** Determines the type of node. */
    virtual SHAMapNodeType
    getType() const = 0;

    /** Determines if this is a leaf node. */
    virtual bool
    isLeaf() const = 0;

    /** Determines if this is an inner node. */
    virtual bool
    isInner() const = 0;

    /** Serialize the node in a format appropriate for sending over the wire */
    virtual void
    serializeForWire(Serializer&) const = 0;

    /** Serialize the node in a format appropriate for hashing */
    virtual void
    serializeWithPrefix(Serializer&) const = 0;

    virtual std::string
    getString(SHAMapNodeID const&) const;

    virtual void
    invariants(bool is_root = false) const = 0;

    static std::shared_ptr<SHAMapAbstractNode>
    makeFromPrefix(Slice rawNode, SHAMapHash const& hash);

    static std::shared_ptr<SHAMapAbstractNode>
    makeFromWire(Slice rawNode);

private:
    static std::shared_ptr<SHAMapAbstractNode>
    makeTransaction(
        Slice data,
        std::uint32_t seq,
        SHAMapHash const& hash,
        bool hashValid);

    static std::shared_ptr<SHAMapAbstractNode>
    makeAccountState(
        Slice data,
        std::uint32_t seq,
        SHAMapHash const& hash,
        bool hashValid);

    static std::shared_ptr<SHAMapAbstractNode>
    makeTransactionWithMeta(
        Slice data,
        std::uint32_t seq,
        SHAMapHash const& hash,
        bool hashValid);
};

class SHAMapInnerNode : public SHAMapAbstractNode,
                        public CountedObject<SHAMapInnerNode>
{
    std::array<SHAMapHash, 16> mHashes;
    std::shared_ptr<SHAMapAbstractNode> mChildren[16];
    int mIsBranch = 0;
    std::uint32_t mFullBelowGen = 0;

    static std::mutex childLock;

public:
    SHAMapInnerNode(std::uint32_t seq);
    std::shared_ptr<SHAMapAbstractNode>
    clone(std::uint32_t owner) const override;

    SHAMapNodeType
    getType() const override
    {
        return SHAMapNodeType::tnINNER;
    }

    bool
    isLeaf() const override
    {
        return false;
    }

    bool
    isInner() const override
    {
        return true;
    }

    bool
    isEmpty() const;
    bool
    isEmptyBranch(int m) const;
    int
    getBranchCount() const;
    SHAMapHash const&
    getChildHash(int m) const;

    void
    setChild(int m, std::shared_ptr<SHAMapAbstractNode> const& child);
    void
    shareChild(int m, std::shared_ptr<SHAMapAbstractNode> const& child);
    SHAMapAbstractNode*
    getChildPointer(int branch);
    std::shared_ptr<SHAMapAbstractNode>
    getChild(int branch);
    virtual std::shared_ptr<SHAMapAbstractNode>
    canonicalizeChild(int branch, std::shared_ptr<SHAMapAbstractNode> node);

    // sync functions
    bool
    isFullBelow(std::uint32_t generation) const;
    void
    setFullBelowGen(std::uint32_t gen);

    void
    updateHash() override;

    /** Recalculate the hash of all children and this node. */
    void
    updateHashDeep();

    void
    serializeForWire(Serializer&) const override;

    void
    serializeWithPrefix(Serializer&) const override;

    std::string
    getString(SHAMapNodeID const&) const override;

    void
    invariants(bool is_root = false) const override;

    static std::shared_ptr<SHAMapAbstractNode>
    makeFullInner(
        Slice data,
        std::uint32_t seq,
        SHAMapHash const& hash,
        bool hashValid);

    static std::shared_ptr<SHAMapAbstractNode>
    makeCompressedInner(Slice data, std::uint32_t seq);
};

// SHAMapTreeNode represents a leaf, and may eventually be renamed to reflect
// that.
class SHAMapTreeNode : public SHAMapAbstractNode
{
protected:
    std::shared_ptr<SHAMapItem const> mItem;

public:
    SHAMapTreeNode(const SHAMapTreeNode&) = delete;
    SHAMapTreeNode&
    operator=(const SHAMapTreeNode&) = delete;

    SHAMapTreeNode(std::shared_ptr<SHAMapItem const> item, std::uint32_t seq);
    SHAMapTreeNode(
        std::shared_ptr<SHAMapItem const> item,
        std::uint32_t seq,
        SHAMapHash const& hash);

    bool
    isLeaf() const override
    {
        return true;
    }

    bool
    isInner() const override
    {
        return false;
    }

    void
    invariants(bool is_root = false) const override;

public:  // public only to SHAMap
    // item node function
    std::shared_ptr<SHAMapItem const> const&
    peekItem() const;

    /** Set the item that this node points to and update the node's hash.

        @param i the new item
        @return false if the change was, effectively, a noop (that is, if the
                hash was unchanged); true otherwise.
     */
    bool
    setItem(std::shared_ptr<SHAMapItem const> i);

    std::string
    getString(SHAMapNodeID const&) const override;
};

/** A leaf node for a transaction. No metadata is included. */
class SHAMapTxLeafNode : public SHAMapTreeNode,
                         public CountedObject<SHAMapTxLeafNode>
{
public:
    SHAMapTxLeafNode(std::shared_ptr<SHAMapItem const> item, std::uint32_t seq)
        : SHAMapTreeNode(std::move(item), seq)
    {
        updateHash();
    }

    SHAMapTxLeafNode(
        std::shared_ptr<SHAMapItem const> item,
        std::uint32_t seq,
        SHAMapHash const& hash)
        : SHAMapTreeNode(std::move(item), seq, hash)
    {
    }

    std::shared_ptr<SHAMapAbstractNode>
    clone(std::uint32_t owner) const override
    {
        return std::make_shared<SHAMapTxLeafNode>(mItem, owner, hash_);
    }

    SHAMapNodeType
    getType() const override
    {
        return SHAMapNodeType::tnTRANSACTION_NM;
    }

    void
    updateHash() override;

    void
    serializeForWire(Serializer&) const override;

    void
    serializeWithPrefix(Serializer&) const override;
};

/** A leaf node for a transaction and its associated metadata. */
class SHAMapTxPlusMetaLeafNode : public SHAMapTreeNode,
                                 public CountedObject<SHAMapTxPlusMetaLeafNode>
{
public:
    SHAMapTxPlusMetaLeafNode(
        std::shared_ptr<SHAMapItem const> item,
        std::uint32_t seq)
        : SHAMapTreeNode(std::move(item), seq)
    {
        updateHash();
    }

    SHAMapTxPlusMetaLeafNode(
        std::shared_ptr<SHAMapItem const> item,
        std::uint32_t seq,
        SHAMapHash const& hash)
        : SHAMapTreeNode(std::move(item), seq, hash)
    {
    }

    std::shared_ptr<SHAMapAbstractNode>
    clone(std::uint32_t owner) const override
    {
        return std::make_shared<SHAMapTxPlusMetaLeafNode>(mItem, owner, hash_);
    }

    SHAMapNodeType
    getType() const override
    {
        return SHAMapNodeType::tnTRANSACTION_MD;
    }

    void
    updateHash() override;

    void
    serializeForWire(Serializer&) const override;

    void
    serializeWithPrefix(Serializer&) const override;
};

/** A leaf node for a state object. */
class SHAMapAccountStateLeafNode
    : public SHAMapTreeNode,
      public CountedObject<SHAMapAccountStateLeafNode>
{
public:
    SHAMapAccountStateLeafNode(
        std::shared_ptr<SHAMapItem const> item,
        std::uint32_t seq)
        : SHAMapTreeNode(std::move(item), seq)
    {
        updateHash();
    }

    SHAMapAccountStateLeafNode(
        std::shared_ptr<SHAMapItem const> item,
        std::uint32_t seq,
        SHAMapHash const& hash)
        : SHAMapTreeNode(std::move(item), seq, hash)
    {
    }

    std::shared_ptr<SHAMapAbstractNode>
    clone(std::uint32_t owner) const override
    {
        return std::make_shared<SHAMapAccountStateLeafNode>(mItem, owner, hash_);
    }

    SHAMapNodeType
    getType() const override
    {
        return SHAMapNodeType::tnACCOUNT_STATE;
    }

    void
    updateHash() override;

    void
    serializeForWire(Serializer&) const override;

    void
    serializeWithPrefix(Serializer&) const override;
};

// SHAMapAbstractNode
inline std::uint32_t
SHAMapAbstractNode::owner() const
{
    return cowid_;
}

inline void
SHAMapAbstractNode::share()
{
    cowid_ = 0;
}

inline SHAMapHash const&
SHAMapAbstractNode::getHash() const
{
    return hash_;
}

// SHAMapInnerNode

inline SHAMapInnerNode::SHAMapInnerNode(std::uint32_t seq)
    : SHAMapAbstractNode(seq)
{
}

inline bool
SHAMapInnerNode::isEmptyBranch(int m) const
{
    return (mIsBranch & (1 << m)) == 0;
}

inline SHAMapHash const&
SHAMapInnerNode::getChildHash(int m) const
{
    assert((m >= 0) && (m < 16) && (getType() == SHAMapNodeType::tnINNER));
    return mHashes[m];
}

inline bool
SHAMapInnerNode::isFullBelow(std::uint32_t generation) const
{
    return mFullBelowGen == generation;
}

inline void
SHAMapInnerNode::setFullBelowGen(std::uint32_t gen)
{
    mFullBelowGen = gen;
}

// SHAMapTreeNode
inline std::shared_ptr<SHAMapItem const> const&
SHAMapTreeNode::peekItem() const
{
    return mItem;
}

}  // namespace ripple

#endif
