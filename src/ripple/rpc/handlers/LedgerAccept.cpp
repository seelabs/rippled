//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2014 Ripple Labs Inc.

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

#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/misc/AmendmentTable.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/core/Config.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/Context.h>

#include <mutex>

namespace ripple {

Json::Value doLedgerAccept (RPC::JsonContext& context)
{
    std::unique_lock lock{context.app.getMasterMutex()};
    Json::Value jvResult;

    if (!context.app.config().standalone())
    {
        jvResult[jss::error] = "notStandAlone";
    }
    else
    {
        if (context.params.isMember(jss::ledger_index))
        {
            if (context.params[jss::ledger_index].asUInt() !=
                context.ledgerMaster.getCurrentLedgerIndex())
            {
                jvResult[jss::error] = "old ledger";
                return jvResult;
            }
        }

        if (context.params.isMember(jss::amendments))
        {
            auto arr = context.params[jss::amendments];
            for (auto it = arr.begin(); it != arr.end(); ++it)
            {
                auto value = *it;
                auto amendment =
                    from_hex_text<uint256>(value["amendment"].asString());
                uint32_t flags = value[jss::flags].asUInt();
                AmendmentTable::HardcodedVote hv{amendment, flags};
                context.app.getAmendmentTable().hardcodedVotes_.push_back(hv);
            }
        }

        if (context.params.isMember(jss::close_time))
        {
            auto closeTime = context.params[jss::close_time];
            std::chrono::seconds ct(closeTime.asUInt());
            context.netOps.acceptLedger({}, ct);
        }
        else
        {
            context.netOps.acceptLedger();
        }
        jvResult[jss::ledger_current_index] =
            context.ledgerMaster.getCurrentLedgerIndex ();
    }

    return jvResult;
}

} // ripple
