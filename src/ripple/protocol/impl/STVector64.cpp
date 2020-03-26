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
#include <ripple/basics/StringUtilities.h>
#include <ripple/protocol/STVector64.h>
#include <ripple/protocol/jss.h>

namespace ripple {

STVector64::STVector64(SerialIter& sit, SField const& name) : STBase(name)
{
    Blob data = sit.getVL();
    auto const count = data.size() / sizeof(std::uint64_t);
    mValue.resize(count);
    memcpy(mValue.data(), data.data(), data.size());
}

void
STVector64::add(Serializer& s) const
{
    assert(fName->isBinary());
    assert(fName->fieldType == STI_VECTOR64);
    s.addVL(
        reinterpret_cast<void const*>(mValue.data()),
        mValue.size() * sizeof(std::uint64_t));
}

bool
STVector64::isEquivalent(const STBase& t) const
{
    const STVector64* v = dynamic_cast<const STVector64*>(&t);
    return v && (mValue == v->mValue);
}

Json::Value STVector64::getJson(JsonOptions) const
{
    Json::Value ret(Json::arrayValue);

    for (auto const& vEntry : mValue)
        ret.append(std::to_string(vEntry));

    return ret;
}

}  // namespace ripple
