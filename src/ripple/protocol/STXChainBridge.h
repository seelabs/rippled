//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2022 Ripple Labs Inc.

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

#ifndef RIPPLE_PROTOCOL_STXCHAINBRIDGE_H_INCLUDED
#define RIPPLE_PROTOCOL_STXCHAINBRIDGE_H_INCLUDED

#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/STBase.h>
#include <ripple/protocol/STIssue.h>

#include <string>
#include <variant>
#include <vector>

namespace ripple {

class Serializer;
class STObject;

class STXChainBridge final : public STBase
{
public:
    // Changing the chain type ids is transaction breaking
    enum ChainDataID : std::uint16_t { cid_unchecked, cid_xrpl };
    enum class ChainType { locking, issuing };

private:
    struct XRPLData
    {
        STAccount door;
        STIssue issue;

        XRPLData(ChainType ct, AccountID const& door, Issue const& issue);
        XRPLData(ChainType ct, SerialIter& sit);
        XRPLData(ChainType ct, Json::Value const& v);

        void
        add(Serializer& s) const;

        Json::Value
        getJson(JsonOptions jo) const;
    };

    using ChainSideBase = std::variant<std::vector<std::uint8_t>, XRPLData>;

    struct ChainSide : ChainSideBase
    {
        using ChainSideBase::ChainSideBase;
        ChainSide(ChainType ct, AccountID const& door, Issue const& issue);
        ChainSide(ChainType ct, SerialIter& sit);
        ChainSide(ChainType ct, Json::Value const& v);

        void
        add(Serializer& s) const;

        Json::Value
        getJson(JsonOptions jo) const;

        STObject
        toSTObject() const;

        bool
        isDefault() const;
    };

    ChainSide lockingChain_;
    ChainSide issuingChain_;

public:
    using value_type = STXChainBridge;

    static ChainType
    otherChain(ChainType ct);

    static ChainType
    srcChain(bool wasLockingChainSend);

    static ChainType
    dstChain(bool wasLockingChainSend);

    STXChainBridge();

    explicit STXChainBridge(SField const& name);

    STXChainBridge(STXChainBridge const& rhs) = default;

    STXChainBridge(STObject const& o);

    STXChainBridge(
        AccountID const& lockingChainDoor,
        Issue const& lockingChainIssue,
        AccountID const& issuingChainDoor,
        Issue const& issuingChainIssue);

    explicit STXChainBridge(Json::Value const& v);

    explicit STXChainBridge(SField const& name, Json::Value const& v);

    explicit STXChainBridge(SerialIter& sit, SField const& name);

    STXChainBridge&
    operator=(STXChainBridge const& rhs) = default;

    STObject
    toSTObject() const;

    // result may be null
    AccountID const*
    lockingChainDoor() const;

    // result may be null
    Issue const*
    lockingChainIssue() const;

    // result may be null
    AccountID const*
    issuingChainDoor() const;

    // result may be null
    Issue const*
    issuingChainIssue() const;

    // result may be null
    AccountID const*
    door(ChainType ct) const;

    // result may be null
    Issue const*
    issue(ChainType ct) const;

    SerializedTypeID
    getSType() const override;

    Json::Value getJson(JsonOptions) const override;

    void
    add(Serializer& s) const override;

    bool
    isEquivalent(const STBase& t) const override;

    bool
    isDefault() const override;

    value_type const&
    value() const noexcept;

private:
    static std::unique_ptr<STXChainBridge>
    construct(SerialIter&, SField const& name);

    STBase*
    copy(std::size_t n, void* buf) const override;
    STBase*
    move(std::size_t n, void* buf) override;

    friend auto
    operator==(
        STXChainBridge::XRPLData const& lhs,
        STXChainBridge::XRPLData const& rhs)
    {
        return std::tie(lhs.door, lhs.issue) == std::tie(rhs.door, rhs.issue);
    }

    friend auto
    operator<(
        STXChainBridge::XRPLData const& lhs,
        STXChainBridge::XRPLData const& rhs)
    {
        return std::tie(lhs.door, lhs.issue) == std::tie(rhs.door, rhs.issue);
    }

    friend auto
    operator==(STXChainBridge const& lhs, STXChainBridge const& rhs)
    {
        return std::tie(lhs.lockingChain_, lhs.issuingChain_) ==
            std::tie(rhs.lockingChain_, rhs.issuingChain_);
    }

    friend auto
    operator<(STXChainBridge const& lhs, STXChainBridge const& rhs)
    {
        return std::tie(lhs.lockingChain_, lhs.issuingChain_) <
            std::tie(rhs.lockingChain_, rhs.issuingChain_);
    }
};

inline AccountID const*
STXChainBridge::lockingChainDoor() const
{
    if (auto p = std::get_if<STXChainBridge::XRPLData>(&lockingChain_))
    {
        return &p->door.value();
    }
    return nullptr;
};

inline Issue const*
STXChainBridge::lockingChainIssue() const
{
    if (auto p = std::get_if<STXChainBridge::XRPLData>(&lockingChain_))
    {
        return &p->issue.value();
    }
    return nullptr;
};

inline AccountID const*
STXChainBridge::issuingChainDoor() const
{
    if (auto p = std::get_if<STXChainBridge::XRPLData>(&issuingChain_))
    {
        return &p->door.value();
    }
    return nullptr;
};

inline Issue const*
STXChainBridge::issuingChainIssue() const
{
    if (auto p = std::get_if<STXChainBridge::XRPLData>(&issuingChain_))
    {
        return &p->issue.value();
    }
    return nullptr;
};

inline STXChainBridge::value_type const&
STXChainBridge::value() const noexcept
{
    return *this;
}

inline AccountID const*
STXChainBridge::door(ChainType ct) const
{
    if (ct == ChainType::locking)
        return lockingChainDoor();
    return issuingChainDoor();
}

inline Issue const*
STXChainBridge::issue(ChainType ct) const
{
    if (ct == ChainType::locking)
        return lockingChainIssue();
    return issuingChainIssue();
}

inline STXChainBridge::ChainType
STXChainBridge::otherChain(ChainType ct)
{
    if (ct == ChainType::locking)
        return ChainType::issuing;
    return ChainType::locking;
}

inline STXChainBridge::ChainType
STXChainBridge::srcChain(bool wasLockingChainSend)
{
    if (wasLockingChainSend)
        return ChainType::locking;
    return ChainType::issuing;
}

inline STXChainBridge::ChainType
STXChainBridge::dstChain(bool wasLockingChainSend)
{
    if (wasLockingChainSend)
        return ChainType::issuing;
    return ChainType::locking;
}

}  // namespace ripple

#endif
