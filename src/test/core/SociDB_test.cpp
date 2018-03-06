//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2015 Ripple Labs Inc.

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

#include <ripple/core/ConfigSections.h>
#include <ripple/core/SociDB.h>
#include <ripple/basics/contract.h>
#include <test/jtx/TestSuite.h>
#include <ripple/basics/BasicConfig.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

namespace ripple {

class SociDBTestBase : public TestSuite
{
private:
    virtual BasicConfig config () = 0;
    virtual void removeDB(SociConfig const& sc) = 0;
public:
    void testSession ()
    {
        testcase ("open");
        BasicConfig const c = config();
        SociConfig sc (c, "socitestdb");
        std::vector<int> const keys{0,1,2};
        std::vector<std::string> const stringData (
            {"String1", "String2", "String3"});
        std::vector<int> const intData ({1, 2, 3});
        auto checkValues = [this, &stringData, &intData](soci::session& s)
        {
            // Check values in db
            std::vector<std::string> stringResult (20 * stringData.size ());
            std::vector<int> intResult (20 * intData.size ());
            s << "SELECT StringData, IntData FROM SociTestTable;",
                soci::into (stringResult), soci::into (intResult);
            BEAST_EXPECT(stringResult.size () == stringData.size () &&
                    intResult.size () == intData.size ());
            for (int i = 0; i < stringResult.size (); ++i)
            {
                auto si = std::distance (stringData.begin (),
                                         std::find (stringData.begin (),
                                                    stringData.end (),
                                                    stringResult[i]));
                auto ii = std::distance (
                    intData.begin (),
                    std::find (intData.begin (), intData.end (), intResult[i]));
                BEAST_EXPECT(si == ii && si < stringResult.size ());
            }
        };

        {
            soci::session s;
            sc.open (s);

            s << "DROP TABLE IF EXISTS SociTestTable;";

            s << "CREATE TABLE SociTestTable ("
                 "  Key                    INTEGER PRIMARY KEY,"
                 "  StringData             TEXT,"
                 "  IntData                INTEGER"
                 ");";

            s << "INSERT INTO SociTestTable (Key, StringData, IntData) VALUES "
                 "(:keys, :stringData, :intData);",
                soci::use(keys), soci::use(stringData), soci::use(intData);
            checkValues (s);
        }
        {
            // Check values in db after session was closed
            soci::session s;
            sc.open (s);
            checkValues (s);
        }
        removeDB(sc);
    }

    void testSelect ()
    {
        testcase ("select");
        BasicConfig const c = config();
        SociConfig sc (c, "socitestdb");
        std::vector<std::uint64_t> const ubid (
            {std::numeric_limits<std::uint64_t>::max (), 20, 30});
        std::vector<std::int64_t> const bid ({-10, -20, -30});
        std::vector<std::uint32_t> const uid (
            {std::numeric_limits<std::uint32_t>::max (), 2, 3});
        std::vector<std::int32_t> const id ({-1, -2, -3});

        {
            using namespace std::string_literals;

            soci::session s;
            sc.open (s);

            s << "DROP TABLE IF EXISTS STT;";

            if (s.get_backend_name() == "postgresql"s)
            {
                // postgres doesn't support unsigned types.
                // Use BIGINT (signed 64-bit int) for unsigned 32-bit int
                // USE NUMERIC(20,0) (20 digits of percision, scale of zero) for unsigned 64-bit int
                s << "CREATE TABLE STT ("
                     "  I              INTEGER,"
                     "  UI             BIGINT,"
                     "  BI             BIGINT,"
                     "  UBI            NUMERIC(20, 0)"
                     ");";
            }
            else
            {
                s << "CREATE TABLE STT ("
                     "  I              INTEGER,"
                     "  UI             INTEGER UNSIGNED,"
                     "  BI             BIGINT,"
                     "  UBI            BIGINT UNSIGNED"
                     ");";
            }

            s << "INSERT INTO STT (I, UI, BI, UBI) VALUES "
                 "(:id, :idu, :bid, :bidu);",
                soci::use (id), soci::use (uid), soci::use (bid),
                soci::use (ubid);

            try
            {
                std::int32_t ig = 0;
                std::uint32_t uig = 0;
                std::int64_t big = 0;
                std::uint64_t ubig = 0;
                s << "SELECT I, UI, BI, UBI from STT;", soci::into (ig),
                    soci::into (uig), soci::into (big), soci::into (ubig);
                BEAST_EXPECT(ig == id[0] && uig == uid[0] && big == bid[0] &&
                        ubig == ubid[0]);
            }
            catch (std::exception&)
            {
                fail ();
            }
            try
            {
                boost::optional<std::int32_t> ig;
                boost::optional<std::uint32_t> uig;
                boost::optional<std::int64_t> big;
                boost::optional<std::uint64_t> ubig;
                s << "SELECT I, UI, BI, UBI from STT;", soci::into (ig),
                    soci::into (uig), soci::into (big), soci::into (ubig);
                BEAST_EXPECT(*ig == id[0] && *uig == uid[0] && *big == bid[0] &&
                        *ubig == ubid[0]);
            }
            catch (std::exception&)
            {
                fail ();
            }
            // There are too many issues when working with soci::row and boost::tuple. DO NOT USE
            // soci row! I had a set of workarounds to make soci row less error prone, I'm keeping
            // these tests in case I try to add soci::row and boost::tuple back into soci.
#if 0
            try
            {
                std::int32_t ig = 0;
                std::uint32_t uig = 0;
                std::int64_t big = 0;
                std::uint64_t ubig = 0;
                soci::row r;
                s << "SELECT I, UI, BI, UBI from STT", soci::into (r);
                ig = r.get<std::int32_t>(0);
                uig = r.get<std::uint32_t>(1);
                big = r.get<std::int64_t>(2);
                ubig = r.get<std::uint64_t>(3);
                BEAST_EXPECT(ig == id[0] && uig == uid[0] && big == bid[0] &&
                        ubig == ubid[0]);
            }
            catch (std::exception&)
            {
                fail ();
            }
            try
            {
                std::int32_t ig = 0;
                std::uint32_t uig = 0;
                std::int64_t big = 0;
                std::uint64_t ubig = 0;
                soci::row r;
                s << "SELECT I, UI, BI, UBI from STT", soci::into (r);
                ig = r.get<std::int32_t>("I");
                uig = r.get<std::uint32_t>("UI");
                big = r.get<std::int64_t>("BI");
                ubig = r.get<std::uint64_t>("UBI");
                BEAST_EXPECT(ig == id[0] && uig == uid[0] && big == bid[0] &&
                        ubig == ubid[0]);
            }
            catch (std::exception&)
            {
                fail ();
            }
            try
            {
                boost::tuple<std::int32_t,
                             std::uint32_t,
                             std::int64_t,
                             std::uint64_t> d;
                s << "SELECT I, UI, BI, UBI from STT", soci::into (d);
                BEAST_EXPECT(get<0>(d) == id[0] && get<1>(d) == uid[0] &&
                        get<2>(d) == bid[0] && get<3>(d) == ubid[0]);
            }
            catch (std::exception&)
            {
                fail ();
            }
#endif
        }
        removeDB(sc);
    }

    void testDeleteWithSubselect()
    {
        testcase ("deleteWithSubselect");
        BasicConfig const c = config();
        SociConfig sc (c, "socitestdb");
        {
            using namespace std::string_literals;
            soci::session s;
            sc.open (s);

            s << "DROP TABLE IF EXISTS Ledgers;";
            s << "DROP TABLE IF EXISTS Validations;";
            s << "DROP TABLE IF EXISTS ValidationsByHash;";

            std::vector<std::string> dbInit;
            dbInit.push_back("BEGIN TRANSACTION;"s);
            if (s.get_backend_name() == "postgresql"s)
            {
                dbInit.push_back(
                    "CREATE TABLE Ledgers (                 \
                LedgerHash      CHARACTER(64) PRIMARY KEY,  \
                LedgerSeq       NUMERIC(20,0)               \
                );"s);
            }
            else
            {
                dbInit.push_back(
                    "CREATE TABLE Ledgers (                 \
                LedgerHash      CHARACTER(64) PRIMARY KEY,  \
                LedgerSeq       BIGINT UNSIGNED             \
                );"s);
            }
            dbInit.push_back("CREATE INDEX SeqLedger ON Ledgers(LedgerSeq);");
            dbInit.push_back(
                "CREATE TABLE Validations   (  \
                LedgerHash  CHARACTER(64)      \
                );"s);
            dbInit.push_back(
                "CREATE INDEX ValidationsByHash ON \
                Validations(LedgerHash);"s);
            dbInit.push_back("END TRANSACTION;"s);
            for (auto const& sqlStmt : dbInit)
                s << sqlStmt;

            char lh[65];
            memset (lh, 'a', 64);
            lh[64] = '\0';
            int toIncIndex = 63;
            int const numRows = 16;
            std::vector<std::string> ledgerHashes;
            std::vector<int> ledgerIndexes;
            ledgerHashes.reserve(numRows);
            ledgerIndexes.reserve(numRows);
            for (int i = 0; i < numRows; ++i)
            {
                ++lh[toIncIndex];
                if (lh[toIncIndex] == 'z')
                    --toIncIndex;
                ledgerHashes.emplace_back(lh);
                ledgerIndexes.emplace_back(i);
            }
            s << "INSERT INTO Ledgers (LedgerHash, LedgerSeq) VALUES "
                 "(:lh, :li);",
                soci::use (ledgerHashes), soci::use (ledgerIndexes);
            s << "INSERT INTO Validations (LedgerHash) VALUES "
                 "(:lh);", soci::use (ledgerHashes);

            std::vector<int> ledgersLS (numRows * 2);
            std::vector<std::string> validationsLH (numRows * 2);
            s << "SELECT LedgerSeq FROM Ledgers;", soci::into (ledgersLS);
            s << "SELECT LedgerHash FROM Validations;",
                soci::into (validationsLH);
            BEAST_EXPECT(ledgersLS.size () == numRows &&
                    validationsLH.size () == numRows);
        }
        removeDB(sc);
    }

    void testBlob()
    {
        testcase("blob");
        BasicConfig const c = config();
        SociConfig sc(c, "socitestdb");
        {
            using namespace std::string_literals;
            soci::session s;
            sc.open(s);

            s << "DROP TABLE IF EXISTS Blobs;";
            int const toWrite = 42;
            {
                // postgres blob operations must happen in a transaction
                soci::transaction tr(s);
                soci::blob rawData(s);
                if (s.get_backend_name() == "postgresql"s)
                {
                    s << "CREATE TABLE Blobs (rawData oid);"s;
                    s << "SELECT lo_creat(-1);", soci::into(rawData);
                }
                else
                {
                    s << "CREATE TABLE Blobs (rawData BLOB);"s;
                }
                rawData.append(
                    reinterpret_cast<const char*>(&toWrite), sizeof(toWrite));
                s << "insert into Blobs(rawData) values(:rawData)",
                    soci::use(rawData);
                tr.commit();
            }
            {
                // postgres blob operations must happen in a transaction
                soci::transaction tr(s);
                soci::blob rawData(s);
                s << "SELECT RawData FROM Blobs;", soci::into(rawData);

                int toRead = -1;
                if (rawData.get_len() == sizeof(toRead))
                {
                    rawData.read(
                        0, reinterpret_cast<char*>(&toRead), rawData.get_len());
                    BEAST_EXPECT(toRead == toWrite);
                }
                else
                    fail();
                tr.commit();
            }
            pass();
        }
        removeDB(sc);
    }

    virtual void
    run()
    {
        testSession();
        testSelect();
        testDeleteWithSubselect();
        testBlob();
    }
};

class SQLiteSociDB_test final : public SociDBTestBase
{
private:
    BasicConfig config () override
    {
        BasicConfig config;
        boost::filesystem::path const& dbPath = getDatabasePath();
        config.overwrite ("sqdb", "backend", "sqlite");
        auto value = dbPath.string ();
        if (!value.empty ())
            config.legacy ("database_path", value);
        return config;
    }

    void removeDB(SociConfig const& sc) override
    {
        namespace bfs = boost::filesystem;
        // Remove the database
        bfs::path dbPath(sc.connectionString());
        if (bfs::is_regular_file(dbPath))
            bfs::remove(dbPath);
    }

    static void cleanupDatabaseDir (boost::filesystem::path const& dbPath)
    {
        using namespace boost::filesystem;
        if (!exists (dbPath) || !is_directory (dbPath) || !is_empty (dbPath))
            return;
        remove (dbPath);
    }

    static void setupDatabaseDir (boost::filesystem::path const& dbPath)
    {
        using namespace boost::filesystem;
        if (!exists (dbPath))
        {
            create_directory (dbPath);
            return;
        }

        if (!is_directory (dbPath))
        {
            // someone created a file where we want to put out directory
            Throw<std::runtime_error> (
                "Cannot create directory: " + dbPath.string ());
        }
    }
    static boost::filesystem::path getDatabasePath ()
    {
        return boost::filesystem::current_path () / "socidb_test_databases";
    }

public:
    SQLiteSociDB_test ()
    {
        try
        {
            setupDatabaseDir (getDatabasePath ());
        }
        catch (std::exception const&)
        {
        }
    }
    ~SQLiteSociDB_test ()
    {
        try
        {
            cleanupDatabaseDir (getDatabasePath ());
        }
        catch (std::exception const&)
        {
        }
    }
    void testFileNames ()
    {
        // confirm that files are given the correct exensions
        testcase ("sqliteFileNames");
        BasicConfig const c = config();
        std::vector<std::pair<std::string, std::string>> const d (
            {{"peerfinder", ".sqlite"},
             {"state", ".db"},
             {"random", ".db"},
             {"validators", ".sqlite"}});

        for (auto const& i : d)
        {
            SociConfig sc (c, i.first);
            BEAST_EXPECT(boost::ends_with (sc.connectionString (),
                                      i.first + i.second));
        }
    }
    void run () override
    {
        testFileNames ();
        SociDBTestBase::run();
    }
};

// Note: socitestdb must already exist (TBD - create/drop db in test)
class PostgresqlSociDB_test final : public SociDBTestBase
{
private:
    BasicConfig
    config() override
    {
        BasicConfig config;
        std::string const host = "10.0.3.147";  // TBD - this is my local setup
        std::string const user = "postgres";    // TBD- this is my local setup
        std::string const port = "5432";        // Default port
        config.overwrite("sqdb", "backend", "postgresql");
        if (!host.empty())
            config.overwrite("sqdb", "host", host);
        if (!user.empty())
            config.overwrite("sqdb", "user", user);
        if (!port.empty())
            config.overwrite("sqdb", "port", port);
        return config;
    }

    static void
    cleanupDatabase(std::string const& dbname)
    {
        std::string const cmd = "drop database " + dbname + ';';
        (void)cmd;
        // TBD - remove the database;
    }

    static void
    setupDatabase(std::string const& dbname)
    {
        std::string const cmd = "create database " + dbname + ';';
        (void)cmd;
        // TBD - create the database;
        // For now the database must already exist
    }

    void
    removeDB(SociConfig const& sc) override
    {
        // TBD - remove the database;
    }

public:
    PostgresqlSociDB_test()
    {
        try
        {
            setupDatabase("socitestdb");
        }
        catch (std::exception const&)
        {
        }
    }

    ~PostgresqlSociDB_test()
    {
        try
        {
            cleanupDatabase("socitestdb");
        }
        catch (std::exception const&)
        {
        }
    }
};

BEAST_DEFINE_TESTSUITE(SQLiteSociDB,core,ripple);
BEAST_DEFINE_TESTSUITE(PostgresqlSociDB,core,ripple);

}  // ripple
