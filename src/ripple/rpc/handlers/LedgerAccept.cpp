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
#include <ripple/app/ledger/OpenLedger.h>
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

static std::shared_ptr<Ledger> cachedLedger;

Json::Value doLedgerAccept (RPC::JsonContext& context)
{
    std::unique_lock lock{context.app.getMasterMutex()};
    Json::Value jvResult;

    if (!context.app.config().standalone())
    {
        jvResult[jss::error] = "notStandAlone";
    }
    else if (context.params.isMember(jss::ledger))
    {
        try
        {
            auto lgrData = context.params[jss::ledger];
            LedgerInfo lgrInfo;
            lgrInfo.seq = lgrData[jss::ledger_index].asUInt();
            if (cachedLedger && lgrInfo.seq <= cachedLedger->info().seq)
            {
                jvResult["msg"] = "already loaded";
                return jvResult;
            }
            lgrInfo.parentCloseTime = NetClock::time_point(
                std::chrono::seconds(lgrData[jss::parent_close_time].asUInt()));
            lgrInfo.hash =
                from_hex_text<uint256>(lgrData[jss::ledger_hash].asString());
            lgrInfo.txHash = from_hex_text<uint256>(
                lgrData[jss::transaction_hash].asString());
            lgrInfo.accountHash =
                from_hex_text<uint256>(lgrData[jss::account_hash].asString());
            lgrInfo.parentHash =
                from_hex_text<uint256>(lgrData[jss::parent_hash].asString());
            lgrInfo.drops =
                XRPAmount(std::stoll(lgrData[jss::total_coins].asString()));
            lgrInfo.validated = true;
            lgrInfo.accepted = true;
            lgrInfo.closeFlags = lgrData[jss::close_flags].asUInt();
            lgrInfo.closeTimeResolution = NetClock::duration(
                lgrData[jss::close_time_resolution].asUInt());
            lgrInfo.closeTime = NetClock::time_point(
                std::chrono::seconds(lgrData[jss::close_time].asUInt()));
            if (!cachedLedger)
            {
                std::shared_ptr<Ledger> ledger = std::make_shared<Ledger>(
                    lgrInfo, context.app.config(), context.app.family());
                ledger->stateMap().clearSynching();
                ledger->txMap().clearSynching();

                cachedLedger = ledger;
            }
            else
            {
                cachedLedger = std::make_shared<Ledger>(
                    *cachedLedger, NetClock::time_point{});
                cachedLedger->setLedgerInfo(lgrInfo);

                cachedLedger->stateMap().clearSynching();
                cachedLedger->txMap().clearSynching();
            }

            jvResult["msg"] = "hi";

            return jvResult;
        }
        catch (std::exception e)
        {
            jvResult[jss::error] = "An exception was thrown";
            jvResult["msg"] = e.what();
            return jvResult;
        }
    }
    else if (context.params.isMember(jss::ledger_data))
    {
        assert(cachedLedger);

        // load each ledger object into the account state tree
        // This RPC is called several times before calling finish
        auto arr = context.params["state"];
        for (auto elt : arr)
        {
            auto data = elt[jss::data].asString();
            auto key = elt[jss::index].asString();

            auto dataBlob = strUnHex(data);
            auto key256 = from_hex_text<uint256>(key);

            SerialIter it{dataBlob->data(), dataBlob->size()};
            std::shared_ptr<SLE> sle = std::make_shared<SLE>(it, key256);
            if (!cachedLedger->exists(key256))
                cachedLedger->rawInsert(sle);
        }
        jvResult["msg"] = "success";
        return jvResult;
    }
    else if (context.params.isMember("load_diff"))
    {
        // load ledger objects modified in this ledger version

        auto objs = context.params["objs"];
        std::cout << "processing objs" << std::endl;

        for (auto& x : objs)
        {
            std::cout << "index = " << x["index"].asString() << std::endl;

            auto index = from_hex_text<uint256>(x["index"].asString());
            if (x.isMember("node_binary"))
            {
                auto objRaw = strUnHex(x["node_binary"].asString());
                SerialIter it{objRaw->data(), objRaw->size()};

                std::shared_ptr<SLE> sle = std::make_shared<SLE>(it, index);
                if (cachedLedger->exists(index))
                {
                    std::cout << "replacing" << std::endl;
                    cachedLedger->rawReplace(sle);
                }
                else
                {
                    std::cout << "inserting" << std::endl;
                    cachedLedger->rawInsert(sle);
                }
            }
            else
            {
                if (cachedLedger->exists(index))
                {
                    std::cout << "erasing" << std::endl;
                    cachedLedger->rawErase(index);
                }
                else
                {
                    std::cout << "noop" << std::endl;
                }
            }
        }
        std::cout << "done processing objs" << std::endl;

        cachedLedger->updateSkipList();
        jvResult["msg"] = "success";
        return jvResult;
    }
    else if (context.params.isMember("load_txns"))
    {
        // load txns as blobs into ledger
        auto arr = context.params["transactions"];
        std::cout << "loading txns" << std::endl;
        for (auto i = 0; i < arr.size(); ++i)
        {
            auto val = arr[i];

            auto txn = strUnHex(val["tx_blob"].asString());

            SerialIter it{txn->data(), txn->size()};
            STTx sttx{it};
            auto txSerializer =
                std::make_shared<Serializer>(sttx.getSerializer());

            auto blob = strUnHex(val["meta"].asString());
            TxMeta txMeta{
                sttx.getTransactionID(), cachedLedger->info().seq, *blob};

            auto metaSerializer = std::make_shared<Serializer>(
                txMeta.getAsObject().getSerializer());
            std::cout << "inserting " << strHex(sttx.getTransactionID())
                      << std::endl;

            if (!cachedLedger->txExists(sttx.getTransactionID()))
                cachedLedger->rawTxInsert(
                    sttx.getTransactionID(), txSerializer, metaSerializer);
        }

        jvResult["msg"] = "hi";
        return jvResult;
    }
    else if (context.params.isMember("finish"))
    {
        // recompute hashes and save ledger to disk

        // TODO: there is a similar if condition below
        if (context.params["ledger_index"].asUInt() != cachedLedger->info().seq)
        {
            jvResult["msg"] = "wrong sequence";
            return jvResult;
        }

        if (context.ledgerMaster.getCurrentLedgerIndex() >
            cachedLedger->info().seq)
        {
            jvResult["msg"] = "already_finished";

            jvResult[jss::ledger_current_index] =
                context.ledgerMaster.getCurrentLedgerIndex();
            jvResult["open_ledger_app"] =
                context.app.openLedger().current()->info().seq;

            return jvResult;
        }

        std::cout << strHex(cachedLedger->stateMap().getHash().as_uint256())
                  << std::endl;
        std::cout << strHex(cachedLedger->info().accountHash) << std::endl;

        std::cout << strHex(cachedLedger->txMap().getHash().as_uint256())
                  << std::endl;
        std::cout << strHex(cachedLedger->info().txHash) << std::endl;

        cachedLedger->setImmutable(context.app.config());

        cachedLedger->stateMap().flushDirty(
            hotACCOUNT_NODE, cachedLedger->info().seq);

        cachedLedger->txMap().flushDirty(
            hotTRANSACTION_NODE, cachedLedger->info().seq);

        if (cachedLedger->txMap().getHash().as_uint256() !=
            cachedLedger->info().txHash)
            jvResult["tx_hash"] = "wrong";
        else
            jvResult["tx_hash"] = "correct";

        if (cachedLedger->stateMap().getHash().as_uint256() !=
            cachedLedger->info().accountHash)
            jvResult["account_hash"] = "wrong";
        else
            jvResult["account_hash"] = "correct";


        context.app.setOpenLedger(cachedLedger);

        jvResult["stored"] =
            !context.app.getLedgerMaster().storeLedger(cachedLedger);

        context.app.getLedgerMaster().switchLCL(cachedLedger);

        jvResult["open_ledger"] = cachedLedger->info().seq + 1;

        jvResult[jss::ledger_current_index] =
            context.ledgerMaster.getCurrentLedgerIndex();
        jvResult["open_ledger_app"] =
            context.app.openLedger().current()->info().seq;
        return jvResult;
    }
    else
    {
        // Modified ledger accept functionality. Not being used
        boost::optional<uint32_t> ledgerIndex;
        boost::optional<std::chrono::seconds> closeTime;
        boost::optional<uint256> parentHash;
        if (context.params.isMember(jss::ledger_index))
        {
            ledgerIndex = context.params[jss::ledger_index].asUInt();
        }

        if (context.params.isMember(jss::close_time))
        {
            auto ctJv = context.params[jss::close_time];
            std::chrono::seconds ct(ctJv.asUInt());
            closeTime = ct;
        }
        if (context.params.isMember(jss::parent_hash))
        {
            parentHash = from_hex_text<uint256>(
                context.params[jss::parent_hash].asString());
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

        if (ledgerIndex && cachedLedger->info().seq == ledgerIndex)
        {
            std::cout << "setting parent hash" << std::endl;
            parentHash = cachedLedger->info().parentHash;
        }

        context.netOps.acceptLedger({}, closeTime, ledgerIndex, parentHash);
        jvResult[jss::ledger_current_index] =
            context.ledgerMaster.getCurrentLedgerIndex();
        if (ledgerIndex &&
            *ledgerIndex != (context.ledgerMaster.getCurrentLedgerIndex() - 1))
            jvResult[jss::error] = "specified ledger already closed";
    }

    return jvResult;
}

} // ripple
