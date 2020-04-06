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
            if (cachedLedger && lgrInfo.seq <= cachedLedger->info().seq)
            {
                jvResult["msg"] = "already loaded";
                return jvResult;
            }
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
                std::cout
                    << strHex(cachedLedger->stateMap().getHash().as_uint256())
                    << std::endl;
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
    }
    else if (context.params.isMember("load_txns"))
    {
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
        if (context.params["ledger_index"].asUInt() != cachedLedger->info().seq)
        {
            jvResult["msg"] = "already_finished";
            return jvResult;
        }

        std::cout << strHex(cachedLedger->stateMap().getHash().as_uint256())
                  << std::endl;
        std::cout << strHex(cachedLedger->info().accountHash) << std::endl;

        std::cout << strHex(cachedLedger->txMap().getHash().as_uint256())
                  << std::endl;
        std::cout << strHex(cachedLedger->info().txHash) << std::endl;

        std::cout << strHex(cachedLedger->stateMap().getHash().as_uint256())
                  << std::endl;
        cachedLedger->setImmutable(context.app.config());
        std::cout << strHex(cachedLedger->stateMap().getHash().as_uint256())
                  << std::endl;
        cachedLedger->stateMap().flushDirty(
            hotACCOUNT_NODE, cachedLedger->info().seq);

        std::cout << strHex(cachedLedger->stateMap().getHash().as_uint256());

        // TODO add in the txs
        // TOD why is this wrong?
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
        std::cout << strHex(cachedLedger->txMap().getHash().as_uint256())
                  << std::endl;
        std::cout << strHex(cachedLedger->info().txHash) << std::endl;

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
    else if (context.params.isMember("load_meta"))
    {
        std::cout << "loading meta" << std::endl;
        auto arr = context.params["transactions"];
        std::vector<TxMeta> metasSorted;
        metasSorted.resize(arr.size());
        for (auto i = 0; i < arr.size(); ++i)
        {
            std::cout << "processing meta" << std::endl;
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
            std::cout << "assigning" << std::endl;
            metasSorted[txMeta.getIndex()] = txMeta;
            std::cout << "assigned" << std::endl;
        }

        for (auto& txMeta : metasSorted)
        {
            std::cout << txMeta.getJson(JsonOptions::none).toStyledString()
                      << std::endl;

            // cachedLedger->rawTxInsert(
            //    sttx.getTransactionID(), txSerializer, metaSerializer);

            STArray& nodes = txMeta.getNodes();
            for (auto it = nodes.begin(); it != nodes.end(); ++it)
            {
                std::cout << "processing node" << std::endl;
                STObject& obj = *it;
                std::cout << obj.getJson(JsonOptions::none).toStyledString()
                          << std::endl;

                // ledger index
                uint256 ledgerIndex = obj.getFieldH256(sfLedgerIndex);

                // ledger entry type
                std::uint16_t lgrType = obj.getFieldU16(sfLedgerEntryType);
                LedgerEntryType entryType = safe_cast<LedgerEntryType>(lgrType);

                // modified node
                if (obj.getFName() == sfModifiedNode)
                {
                    Keylet keylet{entryType, ledgerIndex};
                    auto sle = cachedLedger->peek(keylet);
                    assert(sle);
                    std::vector<std::string> fields;
                    if (obj.isFieldPresent(sfPreviousTxnID))
                    {
                        std::cout << "setting previous txn id : "
                                  << sle->isFieldPresent(sfPreviousTxnID)
                                  << std::endl;
                    }

                    if (obj.isFieldPresent(sfFinalFields))
                    {
                        std::cout << "has final fields" << std::endl;
                        STObject& data =
                            obj.getField(sfFinalFields).downcast<STObject>();

                        for (auto i = 0; i < data.getCount(); ++i)
                        {
                            auto ptr = data.getPIndex(i);
                            assert(ptr);
                            fields.push_back(ptr->getFName().getName());
                            sle->set(ptr);
                            // ATTN ptr is moved from after this call
                            // TODO how to copy
                        }
                        std::cout << "processed final fields" << std::endl;
                    }
                    if (obj.isFieldPresent(sfPreviousFields))
                    {
                        std::cout << "has previous fields" << std::endl;
                        STObject& data =
                            obj.getField(sfPreviousFields).downcast<STObject>();

                        for (auto i = 0; i < data.getCount(); ++i)
                        {
                            auto ptr = data.getPIndex(i);
                            assert(ptr);
                            if (std::find(
                                    fields.begin(),
                                    fields.end(),
                                    ptr->getFName().getName()) == fields.end())
                            {
                                sle->makeFieldAbsent(ptr->getFName());
                            }
                        }
                        std::cout << "processed previous fields" << std::endl;
                    }
                    if (obj.isFieldPresent(sfPreviousTxnID))
                    {
                        std::cout << "setting previous txn id : "
                                  << sle->isFieldPresent(sfPreviousTxnID)
                                  << std::endl;
                        try
                        {
                            sle->setFieldH256(
                                sfPreviousTxnID, txMeta.getTxID());
                            std::cout << "set" << std::endl;
                        }
                        catch (std::exception const& e)
                        {
                            std::cout << "caught exception : " << e.what()
                                      << std::endl;
                            throw e;
                        }
                        /*
                                                try
                                                {
                                                    sle->setFieldH128(
                                                        sfPreviousTxnID,
                           txMeta.getTxID());

                                                    std::cout << "set" <<
                           std::endl;
                                                }
                                                catch(std::exception const& e)
                                                {
                                                    std::cout << "caught
                           exception" << std::endl;
                                                }

                                                try
                                                {
                                                    sle->setFieldU64(
                                                        sfPreviousTxnID,
                           txMeta.getTxID());

                                                    std::cout << "set" <<
                           std::endl;
                                                }
                                                catch(std::exception const& e)
                                                {
                                                    std::cout << "caught
                           exception" << std::endl;
                                                }


                                                try
                                                {
                                                    sle->setFieldU32(
                                                        sfPreviousTxnID,
                           txMeta.getTxID());

                                                    std::cout << "set" <<
                           std::endl;
                                                }
                                                catch(std::exception const& e)
                                                {
                                                    std::cout << "caught
                           exception" << std::endl;
                                                }


                                                try
                                                {
                                                    sle->setFieldU16(
                                                        sfPreviousTxnID,
                           txMeta.getTxID());

                                                    std::cout << "set" <<
                           std::endl;
                                                }
                                                catch(std::exception const& e)
                                                {
                                                    std::cout << "caught
                           exception" << std::endl;
                                                }

                                                try
                                                {
                                                    sle->setFieldU8(
                                                        sfPreviousTxnID,
                           txMeta.getTxID());

                                                    std::cout << "set" <<
                           std::endl;
                                                }
                                                catch(std::exception const& e)
                                                {
                                                    std::cout << "caught
                           exception" << std::endl;
                                                }
                                                */
                    }
                    if (obj.isFieldPresent(sfPreviousTxnLgrSeq))
                    {
                        std::cout << "setting previous txn lgr seq : "
                                  << sle->isFieldPresent(sfPreviousTxnLgrSeq)
                                  << std::endl;

                        try
                        {
                            sle->setFieldU32(
                                sfPreviousTxnLgrSeq, txMeta.getLgrSeq());

                            std::cout << "set" << std::endl;
                        }
                        catch (std::exception const& e)
                        {
                            std::cout << "caught exception : " << e.what()
                                      << std::endl;
                            throw e;
                        }
                    }
                    std::cout
                        << "replacing node : key = " << strHex(ledgerIndex)
                        << std::endl;

                    std::cout
                        << sle->getJson(JsonOptions::none).toStyledString()
                        << std::endl;

                    cachedLedger->rawReplace(sle);
                }
                // created node
                else if (obj.getFName() == sfCreatedNode)
                {
                    if (obj.isFieldPresent(sfNewFields))
                    {
                        STObject& data =
                            obj.getField(sfNewFields).downcast<STObject>();
                        data.makeFieldPresent(sfLedgerEntryType);
                        data.setFieldU16(sfLedgerEntryType, entryType);
                        std::shared_ptr<SLE> sle =
                            std::make_shared<SLE>(data, ledgerIndex);
                        cachedLedger->rawInsert(sle);
                    }

                    std::cout << "adding node : key = " << strHex(ledgerIndex)
                              << std::endl;
                }
                // deleted node
                else if (obj.getFName() == sfDeletedNode)
                {
                    std::cout << "deleting node : key = " << strHex(ledgerIndex)
                              << std::endl;
                    std::shared_ptr<SLE> sle =
                        std::make_shared<SLE>(entryType, ledgerIndex);
                    cachedLedger->rawErase(sle);
                }
            }
        }
        cachedLedger->updateSkipList();
        jvResult["msg"] = "hi";
        return jvResult;
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
            context.ledgerMaster.getCurrentLedgerIndex();
        if (ledgerIndex &&
            *ledgerIndex != (context.ledgerMaster.getCurrentLedgerIndex() - 1))
            jvResult[jss::error] = "specified ledger already closed";
    }

    return jvResult;
}

} // ripple
