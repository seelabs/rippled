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
#include <ripple/rpc/GRPCHandlers.h>

#include <org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>

#include <mutex>

namespace ripple {


static std::thread worker;
static std::shared_ptr<Ledger> cachedLedger;


Json::Value doLedgerAccept (RPC::JsonContext& context)
{
    std::unique_lock lock{context.app.getMasterMutex()};
    Json::Value jvResult;

    if (!context.app.config().standalone())
    {
        jvResult[jss::error] = "notStandAlone";
    }
    else if(context.params.isMember("start_grpc"))
    {

        auto port = context.params["port"].asString();
        auto ip = context.params["ip"].asString();
        auto startIndex = context.params["ledger_index"].asUInt();
        std::cout << "ledger index = " << startIndex
            << std::endl;
        auto& app = context.app;
        //This call should spawn a thread
        worker = std::thread([ip, port, startIndex, &app]() {
            std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub> stub(
                org::xrpl::rpc::v1::XRPLedgerAPIService::NewStub(
                    grpc::CreateChannel(
                        beast::IP::Endpoint(
                            boost::asio::ip::make_address(ip), std::stoi(port))
                            .to_string(),
                        grpc::InsecureChannelCredentials())));

            auto ledgerIndex = startIndex;
            std::shared_ptr<Ledger> ledger;

            auto loadLedger =
                [&ledger, &stub,&app](uint32_t& ledgerIndex) {
                    // ledger header with txns and metadata
                    org::xrpl::rpc::v1::GetLedgerResponse reply;
                    org::xrpl::rpc::v1::GetLedgerRequest request;
                    if(ledgerIndex == 0)
                    {
                        request.mutable_ledger()->set_shortcut(
                            org::xrpl::rpc::v1::LedgerSpecifier::
                                SHORTCUT_VALIDATED);
                    }
                    else
                    {
                        request.mutable_ledger()->set_sequence(ledgerIndex);
                    }
                    request.set_transactions(true);
                    request.set_expand(true);
                    std::cout << "calling get ledger" << std::endl;
                    std::cout << "ledger index = " << ledgerIndex
                        << std::endl;

                    bool validated = false;

                    int toWait = 1;
                    while(not validated)
                    {
                        grpc::ClientContext grpcContext;
                        grpc::Status status =
                            stub->GetLedger(&grpcContext, request, &reply);
                        if(status.ok())
                        {
                            validated = reply.validated();
                            if (!validated)
                                std::this_thread::sleep_for(
                                    std::chrono::seconds(2));
                            toWait = 1;
                            continue;
                        }
                        std::cout << "reply = " << reply.DebugString()
                            << std::endl;
                        std::cout << "status = " << status.error_code()
                            << " msg = " << status.error_message()
                            << std::endl;
                        std::cout << "request = " << request.DebugString()
                            << std::endl;
                        std::this_thread::sleep_for(
                            std::chrono::seconds(toWait));
                        toWait *= 2;
                    }
                    

                    std::cout << reply.DebugString() << std::endl;

                    std::cout << "deserialize header" << std::endl;
                    LedgerInfo lgrInfo = InboundLedger::deserializeHeader(
                        makeSlice(reply.ledger_header()), false);
                    std::cout << "make ledger" << std::endl;
                    ledgerIndex = lgrInfo.seq;

                    if (!ledger)
                    {
                        ledger =
                            std::make_shared<Ledger>(
                                lgrInfo, app.config(), app.family());
                    }
                    else
                    {
                        ledger = std::make_shared<Ledger>(
                            *ledger, NetClock::time_point{});
                        ledger->setLedgerInfo(lgrInfo);
                    }

                    ledger->stateMap().clearSynching();
                    ledger->txMap().clearSynching();
                    std::vector<TxMeta> metas;

                    std::cout << "process txns" << std::endl;
                    for (auto& txn : reply.transactions_list().transactions())
                    {
                        std::cout << "process txn" << std::endl;
                        auto& raw = txn.transaction_blob();

                        SerialIter it{raw.data(), raw.size()};
                        STTx sttx{it};

                        std::cout << "made sttx" << std::endl;
                        auto txSerializer =
                            std::make_shared<Serializer>(sttx.getSerializer());
                        std::cout << "made sttx serializer" << std::endl;

                        TxMeta txMeta{sttx.getTransactionID(),
                                      ledger->info().seq,
                                      txn.metadata_blob()};
                        metas.push_back(txMeta);

                        std::cout << "made txMeta" << std::endl;

                        auto metaSerializer = std::make_shared<Serializer>(
                            txMeta.getAsObject().getSerializer());

                        std::cout << "made txMeta serializer" << std::endl;

                        if (!ledger->txExists(sttx.getTransactionID()))
                            ledger->rawTxInsert(
                                sttx.getTransactionID(),
                                txSerializer,
                                metaSerializer);
                    }
                    return metas;
                };

            loadLedger(ledgerIndex);

            // all of the ledger data
            org::xrpl::rpc::v1::GetLedgerDataResponse replyData;
            org::xrpl::rpc::v1::GetLedgerDataRequest requestData;

            grpc::ClientContext grpcContextData;
            requestData.mutable_ledger()->set_sequence(ledgerIndex);

            std::cout << "starting data" << std::endl;
            grpc::Status status =
                stub->GetLedgerData(&grpcContextData, requestData, &replyData);

            /*
            std::mutex mtx;
            std::condition_variable cv;
            std::queue<std::pair<std::string, std::string>> data;
            std::atomic_bool done{false};
*/


            while(true)
            {
                std::cout << "marker = " << strHex(replyData.marker())
                          << std::endl;
                


                for (auto& state : replyData.state_objects())
                {
                    auto& index = state.index();
                    auto& data = state.data();

                    auto key = uint256::fromVoid(index.data());

                    SerialIter it{data.data(), data.size()};
                    std::shared_ptr<SLE> sle = std::make_shared<SLE>(it, key);
                    if (!ledger->exists(key))
                        ledger->rawInsert(sle);
                }

                if(replyData.marker().size() == 0)
                    break;

                grpc::ClientContext grpcContextData2;
                requestData.set_marker(replyData.marker());
                status = stub->GetLedgerData(
                    &grpcContextData2, requestData, &replyData);
            }
            std::cout << "finished data. storing" << std::endl;

            auto storeLedger = [&ledger,&app]() {
                ledger->setImmutable(app.config());

                std::cout << strHex(ledger->stateMap().getHash().as_uint256())
                          << std::endl;
                std::cout << strHex(ledger->info().accountHash) << std::endl;

                std::cout << strHex(ledger->txMap().getHash().as_uint256())
                          << std::endl;
                std::cout << strHex(ledger->info().txHash) << std::endl;

                ledger->stateMap().flushDirty(
                    hotACCOUNT_NODE, ledger->info().seq);

                ledger->txMap().flushDirty(
                    hotTRANSACTION_NODE, ledger->info().seq);

                std::cout << strHex(ledger->stateMap().getHash().as_uint256())
                          << std::endl;
                std::cout << strHex(ledger->info().accountHash) << std::endl;

                std::cout << strHex(ledger->txMap().getHash().as_uint256())
                          << std::endl;
                std::cout << strHex(ledger->info().txHash) << std::endl;

                assert(
                    ledger->txMap().getHash().as_uint256() ==
                    ledger->info().txHash);

                assert(
                    ledger->stateMap().getHash().as_uint256() ==
                    ledger->info().accountHash);

                app.setOpenLedger(ledger);

                app.getLedgerMaster().storeLedger(ledger);

                app.getLedgerMaster().switchLCL(ledger);
            };

            storeLedger();
            std::cout << "stored initial ledger!" << std::endl;

            // sleep for ten seconds to prevent throttling
            std::this_thread::sleep_for(std::chrono::seconds(10));
            std::cout << "starting continous update" << std::endl;

            while (true)
            {
                ++ledgerIndex;
                auto metas = loadLedger(ledgerIndex);
                std::set<uint256> indices;
                for (auto& meta : metas)
                {
                    STArray& nodes = meta.getNodes();
                    for (auto it = nodes.begin(); it != nodes.end(); ++it)
                    {
                        // ledger index
                        indices.insert(it->getFieldH256(sfLedgerIndex));
                    }
                }

                for (auto iter = indices.begin(); iter != indices.end();)
                {
                    auto& idx = *iter;
                    org::xrpl::rpc::v1::GetLedgerEntryResponse replyEntry;
                    org::xrpl::rpc::v1::GetLedgerEntryRequest requestEntry;

                    grpc::ClientContext grpcContextEntry;
                    requestEntry.mutable_ledger()->set_sequence(ledgerIndex);
                    requestEntry.set_index(idx.data(), idx.size());
                    status = stub->GetLedgerEntry(
                        &grpcContextEntry, requestEntry, &replyEntry);

                    std::cout << status.error_message()
                        << " : "
                        << status.error_code()
                        << " : "
                        << replyEntry.DebugString()
                        << " index : " << strHex(idx)
                        << std::endl;

                    if (status.error_code() == grpc::StatusCode::NOT_FOUND)
                    {
                        if (ledger->exists(idx))
                        {
                            std::cout << "erasing" << std::endl;
                            ledger->rawErase(idx);
                        }
                        else
                        {
                            std::cout << "noop" << std::endl;
                        }
                        ++iter;
                    }
                    else if(status.ok())
                    {
                        auto& objRaw = replyEntry.object_binary();
                        SerialIter it{objRaw.data(), objRaw.size()};

                        std::shared_ptr<SLE> sle =
                            std::make_shared<SLE>(it, idx);
                        if (ledger->exists(idx))
                        {
                            std::cout << "replacing" << std::endl;
                            ledger->rawReplace(sle);
                        }
                        else
                        {
                            std::cout << "inserting" << std::endl;
                            ledger->rawInsert(sle);
                        }
                        ++iter;
                    }
                    else if (status.error_code() == 8)
                    {
                        std::cout << "usage balance. pausing" << std::endl;

                        std::this_thread::sleep_for(std::chrono::seconds(2));
                    }
                    else
                    {
                        std::cout << "unexpected error, trying again"
                            << std::endl;
                    
                    }
                }

                ledger->updateSkipList();
                storeLedger();
                std::cout << "Stored ledger " + std::to_string(ledgerIndex)
                    << std::endl;
            }

            // save the ledger, create next

            // fetch ledger header with txns and metadata
            // store txns
            // process metadata to get list of object ids
            // fetch each object and store
            // repeat
        });
        std::cout << "finished" << std::endl;
        jvResult["msg"] = "finished all";
        return jvResult;
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
