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
            std::cout << "****" << std::endl;
            auto lgrData = context.params[jss::ledger];
            std::cout << "****" << std::endl;
            LedgerInfo lgrInfo;
            lgrInfo.seq = lgrData[jss::ledger_index].asUInt();
            std::cout << "****" << std::endl;
            lgrInfo.parentCloseTime = NetClock::time_point(
                std::chrono::seconds(lgrData[jss::parent_close_time].asUInt()));
            std::cout << "****" << std::endl;
            lgrInfo.hash =
                from_hex_text<uint256>(lgrData[jss::ledger_hash].asString());
            std::cout << "****" << std::endl;
            lgrInfo.txHash = from_hex_text<uint256>(
                lgrData[jss::transaction_hash].asString());
            std::cout << "****" << std::endl;
            lgrInfo.accountHash =
                from_hex_text<uint256>(lgrData[jss::account_hash].asString());
            std::cout << "****" << std::endl;
            lgrInfo.parentHash =
                from_hex_text<uint256>(lgrData[jss::parent_hash].asString());
            std::cout << "****" << std::endl;
            lgrInfo.drops =
                XRPAmount(std::stoll(lgrData[jss::total_coins].asString()));
            std::cout << "****" << std::endl;
            lgrInfo.validated = true;
            std::cout << "****" << std::endl;
            lgrInfo.accepted = true;
            std::cout << "****" << std::endl;
            lgrInfo.closeFlags = lgrData[jss::close_flags].asUInt();
            std::cout << "****" << std::endl;
            lgrInfo.closeTimeResolution = NetClock::duration(
                lgrData[jss::close_time_resolution].asUInt());
            std::cout << "****" << std::endl;
            lgrInfo.closeTime = NetClock::time_point(
                std::chrono::seconds(lgrData[jss::close_time].asUInt()));
            std::cout << "****" << std::endl;
            if (!cachedLedger)
            {
                std::shared_ptr<Ledger> ledger = std::make_shared<Ledger>(
                    lgrInfo, context.app.config(), context.app.family());
                std::cout << "****" << std::endl;
                ledger->stateMap().clearSynching();

                cachedLedger = ledger;
            }
            else
            {
                cachedLedger = std::make_shared<Ledger>(
                    *cachedLedger, NetClock::time_point{});
            }

            jvResult["msg"] = "hi";

            std::cout << "****" << std::endl;
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
        // auto ledgerIndex = context.params[jss::ledger_index].asUInt();
        // std::shared_ptr<Ledger> Ledger =
        //    context.app.getLedgerMaster().getLedgerBySeq(ledgerIndex);

        auto data = context.params[jss::data].asString();
        auto key = context.params[jss::index].asString();

        auto dataBlob = strUnHex(data);
        auto key256 = from_hex_text<uint256>(key);

        SerialIter it{dataBlob->data(), dataBlob->size()};
        std::shared_ptr<SLE> sle = std::make_shared<SLE>(it, key256);
        cachedLedger->rawInsert(sle);

        jvResult["msg"] = "success";
        return jvResult;
    }
    else if (context.params.isMember("finish"))
    {
        std::cout << strHex(cachedLedger->stateMap().getHash().as_uint256())
                  << std::endl;
        // cachedLedger->updateSkipList();

        std::cout << strHex(cachedLedger->stateMap().getHash().as_uint256())
                  << std::endl;
        cachedLedger->setImmutable(context.app.config());
        std::cout << strHex(cachedLedger->stateMap().getHash().as_uint256())
                  << std::endl;
        cachedLedger->stateMap().flushDirty(
            hotACCOUNT_NODE, cachedLedger->info().seq);

        cachedLedger->txMap().flushDirty(
            hotTRANSACTION_NODE, cachedLedger->info().seq);

        std::cout << strHex(cachedLedger->stateMap().getHash().as_uint256())
                  << std::endl;
        context.app.setOpenLedger(cachedLedger);
        std::cout << "set open ledger" << std::endl;

        std::cout << strHex(cachedLedger->stateMap().getHash().as_uint256())
                  << std::endl;
        jvResult["stored"] =
            !context.app.getLedgerMaster().storeLedger(cachedLedger);

        std::cout << strHex(cachedLedger->stateMap().getHash().as_uint256())
                  << std::endl;
        context.app.getLedgerMaster().switchLCL(cachedLedger);

        std::cout << strHex(cachedLedger->stateMap().getHash().as_uint256())
                  << std::endl;
        jvResult["open_ledger"] = cachedLedger->info().seq + 1;

        jvResult[jss::ledger_current_index] =
            context.ledgerMaster.getCurrentLedgerIndex();
        jvResult["open_ledger_app"] =
            context.app.openLedger().current()->info().seq;
        return jvResult;
    }
    else if (context.params.isMember("meta"))
    {
        auto txn = strUnHex(context.params["tx_blob"].asString());

        SerialIter it{txn->data(), txn->size()};
        STTx sttx{it};

        auto blob = strUnHex(context.params["meta"].asString());
        TxMeta txMeta{
            sttx.getTransactionID(), context.params["ledger"].asUInt(), *blob};

        STArray& nodes = txMeta.getNodes();
        for (auto it = nodes.begin(); it != nodes.end(); ++it)
        {
            STObject& obj = *it;

            // ledger index
            uint256 ledgerIndex = obj.getFieldH256(sfLedgerIndex);

            // ledger entry type
            std::uint16_t lgrType = obj.getFieldU16(sfLedgerEntryType);
            LedgerEntryType entryType = safe_cast<LedgerEntryType>(lgrType);

            // modified node
            if (obj.getFName() == sfModifiedNode)
            {
                Keylet keylet{entryType, ledgerIndex};
                std::shared_ptr<SLE> sle =
                    std::make_shared<SLE>(*cachedLedger->read(keylet));

                if (obj.isFieldPresent(sfFinalFields))
                {
                    STObject& data =
                        obj.getField(sfFinalFields).downcast<STObject>();

                    for (auto i = 0; i < data.getCount(); ++i)
                    {
                        std::unique_ptr<STBase> base =
                            std::make_unique<STBase>(data.getIndex(i));
                        assert(base);
                        sle->set(std::move(base));
                    }
                    cachedLedger->rawReplace(sle);
                }
            }
            // created node
            else if (obj.getFName() == sfCreatedNode)
            {
                if (obj.isFieldPresent(sfNewFields))
                {
                    STObject& data =
                        obj.getField(sfNewFields).downcast<STObject>();
                    std::shared_ptr<SLE> sle =
                        std::make_shared<SLE>(data, ledgerIndex);
                    cachedLedger->rawInsert(sle);
                }
            }
            // deleted node
            else if (obj.getFName() == sfDeletedNode)
            {
                std::shared_ptr<SLE> sle =
                    std::make_shared<SLE>(entryType, ledgerIndex);
                cachedLedger->rawErase(sle);
            }
        }
    }
    else
    {
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
            context.ledgerMaster.getCurrentLedgerIndex ();
        if (ledgerIndex &&
            *ledgerIndex != (context.ledgerMaster.getCurrentLedgerIndex() - 1))
            jvResult[jss::error] = "specified ledger already closed";
    }

    return jvResult;
}

} // ripple
