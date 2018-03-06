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

#include <BeastConfig.h>
#include <ripple/app/main/DBInit.h>
#include <boost/format.hpp>
#include <type_traits>

namespace ripple {

// Transaction database holds transactions and public keys
std::vector<std::string>
TxnDBInit(DatabaseCon::Backend backend)
{
    using namespace std::string_literals;
    std::string const blobType =
        backend == DatabaseCon::Backend::postgresql ? "OID" : "BLOB";

    std::vector<std::string> const commonSql{
        {"BEGIN TRANSACTION;"s,

         str(boost::format(
                 R"sql(CREATE TABLE IF NOT EXISTS Transactions (
                      TransID     CHARACTER(64) PRIMARY KEY,
                      TransType   CHARACTER(24),
                      FromAcct    CHARACTER(35),
                      FromSeq     NUMERIC(20, 0),
                      LedgerSeq   NUMERIC(20, 0),
                      Status      CHARACTER(1),
                      RawTxn      %1%,
                      TxnMeta     %1%
                      );
                      )sql"s) %
             blobType),

         R"sql(CREATE INDEX IF NOT EXISTS TxLgrIndex ON
              Transactions(LedgerSeq);
         )sql"s,

         R"sql(CREATE TABLE IF NOT EXISTS AccountTransactions (
               TransID     CHARACTER(64),
               Account     CHARACTER(64),
               LedgerSeq   NUMERIC(20, 0),
               TxnSeq      INTEGER
               );
         )sql"s,

         R"sql(CREATE INDEX IF NOT EXISTS AcctTxIDIndex ON
               AccountTransactions(TransID);
         )sql"s,

         R"sql(CREATE INDEX IF NOT EXISTS AcctTxIndex ON
               AccountTransactions(Account, LedgerSeq, TxnSeq, TransID);
         )sql"s,

         R"sql(CREATE INDEX IF NOT EXISTS AcctLgrIndex ON
               AccountTransactions(LedgerSeq, Account, TransID);
         )sql"s,

         "END TRANSACTION;"s}};  // namespace ripple

    if (backend != DatabaseCon::Backend::sqlite)
        return commonSql;

    std::vector<std::string> sqliteSql{{
        "PRAGMA synchronous=NORMAL;"s,
        "PRAGMA journal_mode=WAL;"s,
        "PRAGMA journal_size_limit=1582080;"s,
        "PRAGMA max_page_count=2147483646;"s

#if (ULONG_MAX > UINT_MAX) && !defined(NO_SQLITE_MMAP)
        "PRAGMA mmap_size=17179869184;"s,
#endif
    }};

    sqliteSql.insert(sqliteSql.end(), commonSql.begin(), commonSql.end());
    return sqliteSql;
}

// Ledger database holds ledgers and ledger confirmations
std::vector<std::string>
LedgerDBInit(DatabaseCon::Backend backend)
{
    using namespace std::string_literals;

    std::string const blobType =
        backend == DatabaseCon::Backend::postgresql ? "OID" : "BLOB";

    std::vector<std::string> const commonSql{
        {"BEGIN TRANSACTION;"s,

         R"sql(CREATE TABLE IF NOT EXISTS Ledgers (
               LedgerHash      CHARACTER(64) PRIMARY KEY,
               LedgerSeq       NUMERIC(20, 0),
               PrevHash        CHARACTER(64),
               TotalCoins      NUMERIC(20, 0),
               ClosingTime     NUMERIC(20, 0),
               PrevClosingTime NUMERIC(20, 0),
               CloseTimeRes    NUMERIC(20, 0),
               CloseFlags      NUMERIC(20, 0),
               AccountSetHash  CHARACTER(64),
               TransSetHash    CHARACTER(64)
               );
        )sql"s,

         "CREATE INDEX IF NOT EXISTS SeqLedger ON Ledgers(LedgerSeq);"s,

         // InitialSeq field is the current ledger seq when the row
         // is inserted. Only relevant during online delete
         str(boost::format(
                 R"sql(CREATE TABLE IF NOT EXISTS Validations   (
                       LedgerSeq   NUMERIC(20, 0),
                       InitialSeq  NUMERIC(20, 0),
                       LedgerHash  CHARACTER(64),
                       NodePubKey  CHARACTER(56),
                       SignTime    NUMERIC(20, 0),
                       RawData     %1%
                       );
                 )sql"s) %
             blobType),

         R"sql(CREATE INDEX IF NOT EXISTS ValidationsByHash ON
               Validations(LedgerHash);
        )sql"s,

         R"sql(CREATE INDEX IF NOT EXISTS ValidationsBySeq ON
               Validations(LedgerSeq);
        )sql"s,

         R"sql(CREATE INDEX IF NOT EXISTS ValidationsByInitialSeq ON
               Validations(InitialSeq, LedgerSeq);
        )sql"s,

         R"sql(CREATE INDEX IF NOT EXISTS ValidationsByTime ON
               Validations(SignTime);
        )sql"s,

         "END TRANSACTION;"s}};

    if (backend != DatabaseCon::Backend::sqlite)
        return commonSql;

    std::vector<std::string> sqliteSql{{"PRAGMA synchronous=NORMAL;"s,
                                        "PRAGMA journal_mode=WAL;"s,
                                        "PRAGMA journal_size_limit=1582080;"s}};

    sqliteSql.insert(sqliteSql.end(), commonSql.begin(), commonSql.end());
    return sqliteSql;
}

std::vector<std::string>
WalletDBInit(DatabaseCon::Backend backend)
{
    using namespace std::string_literals;
    std::string const blobType =
        backend == DatabaseCon::Backend::postgresql ? "OID" : "BLOB";

    std::vector<std::string> const result{
        {"BEGIN TRANSACTION;"s,

         // A node's identity must be persisted, including
         // for clustering purposes. This table holds one
         // entry: the server's unique identity, but the
         // value can be overriden by specifying a node
         // identity in the config file using a [node_seed]
         // entry.
         R"sql(CREATE TABLE IF NOT EXISTS NodeIdentity (
               PublicKey       CHARACTER(53),
               PrivateKey      CHARACTER(52)
               );
        )sql"s,

         // Validator Manifests
         str(boost::format(
                 R"sql(CREATE TABLE IF NOT EXISTS ValidatorManifests (
                       RawData          %1% NOT NULL
                       );
                 )sql"s) %
             blobType),

         str(boost::format(
                 R"sql(CREATE TABLE IF NOT EXISTS PublisherManifests (
                       RawData          %1% NOT NULL
                       );
                 )sql"s) %
             blobType),

         "END TRANSACTION;"s}};
    return result;
}

} // ripple
