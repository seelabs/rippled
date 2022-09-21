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

#include <ripple/basics/StringUtilities.h>
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

STXChainBridge::XRPLData::XRPLData(ChainType ct, Json::Value const& v)
{
    if (!v.isObject())
    {
        Throw<std::runtime_error>(
            "STXChainBridge::XRPLData can only be specified with a 'object' "
            "Json value");
    }

    Json::Value const doorStr = v[jss::Door];
    Json::Value const issueJson = v[jss::Issue];

    if (!doorStr.isString())
    {
        Throw<std::runtime_error>(
            "STXChainBridge door must be a string Json value");
    }
    auto const doorOpt = parseBase58<AccountID>(doorStr.asString());
    if (!doorOpt)
    {
        Throw<std::runtime_error>(
            "STXChainBridge door must be a valid account");
    }

    door = STAccount{
        ct == ChainType::locking ? sfLockingChainDoor : sfIssuingChainDoor,
        *doorOpt};
    issue = STIssue{
        ct == ChainType::locking ? sfIssuingChainIssue : sfIssuingChainIssue,
        issueFromJson(issueJson)};
}

void
STXChainBridge::XRPLData::add(Serializer& s) const
{
    door.add(s);
    issue.add(s);
}

Json::Value
STXChainBridge::XRPLData::getJson(JsonOptions jo) const
{
    Json::Value v;
    v[jss::Door] = door.getJson(jo);
    v[jss::Issue] = issue.getJson(jo);
    return v;
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
            *this = sit.getVL();
            break;
        case cid_xrpl:
            *this = XRPLData{ct, sit};
            break;
        default:
            break;
    }
}

STXChainBridge::ChainSide::ChainSide(ChainType ct, Json::Value const& v)
{
    if (!v.isObject())
    {
        Throw<std::runtime_error>(
            "STXChainBridge::ChainSide can only be specified with a 'object' "
            "Json value");
    }

    if (!v.isMember(jss::ChainKind))
    {
        Throw<std::runtime_error>(
            "STXChainBridge::ChainSide must specify a ChainKind");
    }
    auto const ckv = v[jss::ChainKind];
    if (!ckv.isIntegral())
    {
        Throw<std::runtime_error>(
            "STXChainBridge::ChainSide must be an integer");
    }

    switch (ckv.asUInt())
    {
        case cid_unchecked: {
            // decode the hex into data
            if (!v.isMember(jss::Data))
            {
                Throw<std::runtime_error>(
                    "STXChainBridge::ChainSide must specify a Data field for "
                    "unchecked chain kinds");
            }
            auto const& dataV = v[jss::Data];
            if (!dataV.isString())
            {
                Throw<std::runtime_error>(
                    "STXChainBridge::ChainSide must specify a Data as a hex "
                    "string");
            }
            auto const dataOpt = strUnHex(dataV.asString());
            if (!dataOpt)
            {
                Throw<std::runtime_error>(
                    "STXChainBridge::ChainSide must specify a Data as a hex "
                    "string");
            }
            *this = *dataOpt;
            return;
        }
        break;
        case cid_xrpl: {
            *this = STXChainBridge::XRPLData{ct, v};
            return;
        }
        break;
        default: {
            Throw<std::runtime_error>(
                "STXChainBridge::ChainSide unknown chain kind");
        }
        break;
    }
}

void
STXChainBridge::ChainSide::add(Serializer& s) const
{
    std::visit(
        [&s](const auto& v) {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, STXChainBridge::XRPLData>)
            {
                s.add16(cid_xrpl);
                v.add(s);
            }
            else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>)
            {
                s.add16(cid_unchecked);
                s.addVL(v);
            }
            else
            {
                static_assert(sizeof(T) == -1, "non-exhaustive visitor");
            }
        },
        *this);
};

Json::Value
STXChainBridge::ChainSide::getJson(JsonOptions jo) const
{
    return std::visit(
        [&](const auto& v) -> Json::Value {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, STXChainBridge::XRPLData>)
            {
                auto r = v.getJson(jo);
                r[jss::ChainKind] = cid_xrpl;
                return r;
            }
            else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>)
            {
                Json::Value r;
                r[jss::ChainKind] = cid_unchecked;
                r[jss::Data] = strHex(v);
                return r;
            }
            else
            {
                static_assert(sizeof(T) == -1, "non-exhaustive visitor");
            }
        },
        *this);
}

bool
STXChainBridge::ChainSide::isDefault() const
{
    return std::visit(
        [&](const auto& v) -> bool {
            using T = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<T, STXChainBridge::XRPLData>)
            {
                return v.door.isDefault() && v.issue.isDefault();
            }
            else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>)
            {
                return v.empty();
            }
            else
            {
                static_assert(sizeof(T) == -1, "non-exhaustive visitor");
            }
        },
        *this);
}

STObject
STXChainBridge::ChainSide::toSTObject() const
{
    // TBD
    STObject o{sfXChainBridge};
    return o;
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

    lockingChain_ =
        STXChainBridge::ChainSide{ChainType::locking, v[jss::LockingChain]};
    lockingChain_ =
        STXChainBridge::ChainSide{ChainType::issuing, v[jss::IssuingChain]};
}

STXChainBridge::STXChainBridge(SerialIter& sit, SField const& name)
    : STBase{name}
    , lockingChain_{ChainType::locking, sit}
    , issuingChain_{ChainType::issuing, sit}
{
}

void
STXChainBridge::add(Serializer& s) const
{
    lockingChain_.add(s);
    issuingChain_.add(s);
}

Json::Value
STXChainBridge::getJson(JsonOptions jo) const
{
    Json::Value v;
    v[jss::LockingChain] = lockingChain_.getJson(jo);
    v[jss::IssuingChain] = issuingChain_.getJson(jo);
    return v;
}

STObject
STXChainBridge::toSTObject() const
{
    // TBD
    STObject o{sfXChainBridge};
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
    return lockingChain_.isDefault() && issuingChain_.isDefault();
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
