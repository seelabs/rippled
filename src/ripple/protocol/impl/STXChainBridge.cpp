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

#include <ripple/protocol/STXChainBridge.h>

#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/Issue.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STAccount.h>
#include <ripple/protocol/STObject.h>
#include <ripple/protocol/STXChainBridge.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/UintTypes.h>
#include <ripple/protocol/jss.h>
#include <ripple/protocol/tokens.h>

namespace ripple {

STXChainBridge::XRPLData::XRPLData(
    ChainType ct,
    AccountID const& door_,
    Issue const& issue_)
    : door{ct == ChainType::locking ? sfLockingChainDoor : sfIssuingChainDoor, door_}
    , issue{
          ct == ChainType::locking ? sfIssuingChainIssue : sfIssuingChainIssue,
          issue_}
{
}

STXChainBridge::XRPLData::XRPLData(ChainType ct, SerialIter& sit)
    : door{sit, ct == ChainType::locking ? sfLockingChainDoor : sfIssuingChainDoor}
    , issue{
          sit,
          ct == ChainType::locking ? sfIssuingChainIssue : sfIssuingChainIssue}
{
}

STXChainBridge::ChainSide::ChainSide(
    ChainType ct,
    AccountID const& door,
    Issue const& issue)
    : ChainSide{XRPLData{ct, door, issue}}
{
}

STXChainBridge::ChainSide::ChainSide(ChainType ct, SerialIter& sit)
{
    std::uint16_t const cid = sit.get16();
    switch (cid)
    {
        case cid_unchecked:
            break;
        case cid_xrpl:
            *this = XRPLData{ct, sit};
            break;
        default:
            break;
    }
}

STXChainBridge::STXChainBridge() : STBase{sfXChainBridge}
{
}

STXChainBridge::STXChainBridge(SField const& name) : STBase{name}
{
}

STXChainBridge::STXChainBridge(
    AccountID const& lockingChainDoor,
    Issue const& lockingChainIssue,
    AccountID const& issuingChainDoor,
    Issue const& issuingChainIssue)
    : STBase{sfXChainBridge}
    , lockingChain_{ChainType::locking, lockingChainDoor, lockingChainIssue}
    , issuingChain_{ChainType::issuing, issuingChainDoor, issuingChainIssue}
{
}

STXChainBridge::STXChainBridge(STObject const& o)
    : STBase{sfXChainBridge}
    , lockingChain_{ChainType::locking, o[sfLockingChainDoor], o[sfLockingChainIssue]}
    , issuingChain_{
          ChainType::issuing,
          o[sfIssuingChainDoor],
          o[sfIssuingChainIssue]}
{
}

STXChainBridge::STXChainBridge(Json::Value const& v)
    : STXChainBridge{sfXChainBridge, v}
{
}

STXChainBridge::STXChainBridge(SField const& name, Json::Value const& v)
    : STBase{name}
{
    // TODO; Check that there are no extra fields
    if (!v.isObject())
    {
        Throw<std::runtime_error>(
            "STXChainBridge can only be specified with a 'object' "
            "Json value");
    }

    Json::Value const lockingChainDoorStr = v[jss::LockingChainDoor];
    Json::Value const lockingChainIssue = v[jss::LockingChainIssue];
    Json::Value const issuingChainDoorStr = v[jss::IssuingChainDoor];
    Json::Value const issuingChainIssue = v[jss::IssuingChainIssue];

    if (!lockingChainDoorStr.isString())
    {
        Throw<std::runtime_error>(
            "STXChainBridge LockingChainDoor must be a string Json "
            "value");
    }
    if (!issuingChainDoorStr.isString())
    {
        Throw<std::runtime_error>(
            "STXChainBridge IssuingChainDoor must be a string Json "
            "value");
    }

    auto const lockingChainDoor =
        parseBase58<AccountID>(lockingChainDoorStr.asString());
    auto const issuingChainDoor =
        parseBase58<AccountID>(issuingChainDoorStr.asString());
    if (!lockingChainDoor)
    {
        Throw<std::runtime_error>(
            "STXChainBridge LockingChainDoor must be a valid "
            "account");
    }
    if (!issuingChainDoor)
    {
        Throw<std::runtime_error>(
            "STXChainBridge IssuingChainDoor must be a valid "
            "account");
    }

    lockingChain_ = XRPLData{
        STAccount{sfLockingChainDoor, *lockingChainDoor},
        STIssue{sfLockingChainIssue, issueFromJson(lockingChainIssue)}};
    issuingChain_ = XRPLData{
        STAccount{sfIssuingChainDoor, *issuingChainDoor},
        STIssue{sfIssuingChainIssue, issueFromJson(issuingChainIssue)}};
}

STXChainBridge::STXChainBridge(SerialIter& sit, SField const& name)
    : STBase{name}
    , lockingChainDoor_{sit, sfLockingChainDoor}
    , lockingChainIssue_{sit, sfLockingChainIssue}
    , issuingChainDoor_{sit, sfIssuingChainDoor}
    , issuingChainIssue_{sit, sfIssuingChainIssue}
{
}

void
STXChainBridge::add(Serializer& s) const
{
    lockingChainDoor_.add(s);
    lockingChainIssue_.add(s);
    issuingChainDoor_.add(s);
    issuingChainIssue_.add(s);
}

Json::Value
STXChainBridge::getJson(JsonOptions jo) const
{
    Json::Value v;
    v[jss::LockingChainDoor] = lockingChainDoor_.getJson(jo);
    v[jss::LockingChainIssue] = lockingChainIssue_.getJson(jo);
    v[jss::IssuingChainDoor] = issuingChainDoor_.getJson(jo);
    v[jss::IssuingChainIssue] = issuingChainIssue_.getJson(jo);
    return v;
}

STObject
STXChainBridge::toSTObject() const
{
    STObject o{sfXChainBridge};
    o[sfLockingChainDoor] = lockingChainDoor_;
    o[sfLockingChainIssue] = lockingChainIssue_;
    o[sfIssuingChainDoor] = issuingChainDoor_;
    o[sfIssuingChainIssue] = issuingChainIssue_;
    return o;
}

SerializedTypeID
STXChainBridge::getSType() const
{
    return STI_XCHAIN_BRIDGE;
}

bool
STXChainBridge::isEquivalent(const STBase& t) const
{
    const STXChainBridge* v = dynamic_cast<const STXChainBridge*>(&t);
    return v && (*v == *this);
}

bool
STXChainBridge::isDefault() const
{
    return lockingChainDoor_.isDefault() && lockingChainIssue_.isDefault() &&
        issuingChainDoor_.isDefault() && issuingChainIssue_.isDefault();
}

std::unique_ptr<STXChainBridge>
STXChainBridge::construct(SerialIter& sit, SField const& name)
{
    return std::make_unique<STXChainBridge>(sit, name);
}

STBase*
STXChainBridge::copy(std::size_t n, void* buf) const
{
    return emplace(n, buf, *this);
}

STBase*
STXChainBridge::move(std::size_t n, void* buf)
{
    return emplace(n, buf, std::move(*this));
}
}  // namespace ripple
