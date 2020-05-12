#include <ripple/protocol/Serializer.h>
#include <ripple/rpc/GRPCHandlers.h>
#include <ripple/rpc/impl/RPCHelpers.h>

namespace ripple {
std::pair<org::xrpl::rpc::v1::GetLedgerDiffResponse, grpc::Status>
doLedgerDiffGrpc(
    RPC::GRPCContext<org::xrpl::rpc::v1::GetLedgerDiffRequest>& context)
{
    org::xrpl::rpc::v1::GetLedgerDiffRequest& request = context.params;
    org::xrpl::rpc::v1::GetLedgerDiffResponse response;
    grpc::Status status = grpc::Status::OK;

    std::shared_ptr<ReadView const> baseLedgerRv;
    std::shared_ptr<ReadView const> desiredLedgerRv;

    if (RPC::ledgerFromSpecifier(
            baseLedgerRv, request.base_ledger(), context) != RPC::Status::OK)
    {
        grpc::Status errorStatus{grpc::StatusCode::NOT_FOUND,
                                 "base ledger not found"};
        return {response, errorStatus};
    }

    if (RPC::ledgerFromSpecifier(
            desiredLedgerRv, request.desired_ledger(), context) !=
        RPC::Status::OK)
    {
        grpc::Status errorStatus{grpc::StatusCode::NOT_FOUND,
                                 "desired ledger not found"};
        return {response, errorStatus};
    }

    std::shared_ptr<Ledger const> baseLedger =
        std::dynamic_pointer_cast<Ledger const>(baseLedgerRv);
    if (!baseLedger)
    {
        grpc::Status errorStatus{grpc::StatusCode::NOT_FOUND,
                                 "base ledger not validated"};
        return {response, errorStatus};
    }

    std::shared_ptr<Ledger const> desiredLedger =
        std::dynamic_pointer_cast<Ledger const>(desiredLedgerRv);
    if (!desiredLedger)
    {
        grpc::Status errorStatus{grpc::StatusCode::NOT_FOUND,
                                 "base ledger not validated"};
        return {response, errorStatus};
    }

    SHAMap::Delta differences;

    int maxDifferences = std::numeric_limits<int>::max();

    bool res = baseLedger->stateMap().compare(
        desiredLedger->stateMap(), differences, maxDifferences);
    if (!res)
    {
        grpc::Status errorStatus{
            grpc::StatusCode::RESOURCE_EXHAUSTED,
            "too many differences between specified ledgers"};
        return {response, errorStatus};
    }

    for (auto& [k, v] : differences)
    {
        auto diff = response.add_diffs();
        auto inBase = v.first;
        auto inDesired = v.second;
        assert(inBase || inDesired);
        {
            auto const opType = [&] {
                using namespace org::xrpl::rpc::v1;
                if (!inDesired)
                    return Diff_OperationType_otDelete;
                else if (!inBase)
                    return Diff_OperationType_otAdd;
                else
                    return Diff_OperationType_otModify;
            }();
            diff->set_operation_type(opType);
        }
        {
            using namespace org::xrpl::rpc::v1;
            auto leType = [&]() -> Diff_LedgerEntryType {
                auto s = [&] {
                    if (inBase)
                        return SerialIter{inBase->data(), inBase->size()};
                    return SerialIter{inDesired->data(), inDesired->size()};
                }();
                int type;
                int field;
                s.getFieldID(type, field);
                if (type == STI_UINT16 && field == 1)
                {
                    return Diff_LedgerEntryType{s.get16()};
                }
                else
                {
                    assert(0);
                    return Diff_LedgerEntryType{0xffff};
                }
            }();
            diff->set_ledger_entry_type(leType);
        }

        // key does not exist in desired
        if (!inDesired)
        {
            diff->set_key(k.data(), k.size());
        }
        else
        {
            assert(inDesired->size() > 0);
            diff->set_key(k.data(), k.size());
            if (request.include_blobs())
            {
                diff->set_blob(inDesired->data(), inDesired->size());
            }
        }
    }
    return {response, status};
}

}  // namespace ripple
