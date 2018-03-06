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
#include <ripple/core/DatabaseCon.h>
#include <ripple/core/SociDB.h>
#include <ripple/basics/contract.h>
#include <ripple/basics/Log.h>
#include <memory>

namespace ripple {

boost::optional<soci::connection_pool> DatabaseCon::staticPool_;

// Use temporary files or regular DB files?
bool
DatabaseCon::useTempFiles(Setup const& setup)
{
    return setup.standAlone && setup.startUp != Config::LOAD &&
        setup.startUp != Config::LOAD_FILE && setup.startUp != Config::REPLAY;
}

bool
DatabaseCon::useSqlite(Setup const& setup)
{
    return (setup.backend == Backend::sqlite || useTempFiles(setup));
}

void
DatabaseCon::initPool(
    soci::connection_pool& pool,
    Setup const& setup,
    std::size_t poolSize,
    std::string const& strName)
{
    if (useSqlite(setup))
    {
        assert(!strName.empty());
        boost::filesystem::path pPath =
            useTempFiles(setup) ? "" : (setup.dataDir / strName);

        for (size_t i = 0; i < poolSize; ++i)
        {
            auto& poolSession = pool.at(i);
            open(poolSession, "sqlite", pPath.string());
        }
    }

    if (setup.backend == Backend::postgresql)
    {
        if (setup.postgresql)
        {
            auto& pg = *setup.postgresql;
            std::stringstream s;
            s << "host=" << pg.host << " port=" << pg.port
              << " dbname=" << pg.dbName << " user=" << pg.user;

            for (size_t i = 0; i < poolSize; ++i)
            {
                auto& poolSession = pool.at(i);
                open(poolSession, "postgresql", s.str());
            }
        }
    }
}

void
DatabaseCon::initStaticPool(Setup const& setup)
{
    assert(!staticPool_);
    std::size_t const poolSize =
        setup.postgresql ? setup.postgresql->staticPoolSize : 0;

    if (useSqlite(setup) || poolSize <= 0)
    {
        // sqlite uses different connection parameters so cannot use a static
        // pool
        return;
    }
    staticPool_.emplace(poolSize);
    initPool(*staticPool_, setup, poolSize, std::string{});
}

DatabaseCon::DatabaseCon(
    Setup const& setup,
    std::string const& strName,
    std::vector<std::string> const& initStrings)
    : pool_{setup.poolSize}
{
    using namespace std::string_literals;

    assert(setup.poolSize > 0);

    initPool(pool_, setup, setup.poolSize, strName);

    soci::session session{pool_};
    for (auto const sqlStmt : initStrings)
    {
        try
        {
            soci::statement st = session.prepare << sqlStmt;
            st.execute(true);
        }
        catch (soci::soci_error&)
        {
            // ignore errors
        }
    }
}

LockedSociSession
DatabaseCon::checkoutDb()
{
    if (staticPool_)
    {
        size_t poolPosition{};
        if (staticPool_->try_lease(poolPosition, /*timeout*/0))
            return LockedSociSession(*staticPool_, poolPosition);
    }

    auto const poolPosition = pool_.lease();
    return LockedSociSession(pool_, poolPosition);
}

DatabaseCon::Setup setup_DatabaseCon (Config const& c)
{
    using namespace std::string_literals;

    DatabaseCon::Setup setup;

    setup.startUp = c.START_UP;
    setup.standAlone = c.standalone();
    setup.dataDir = c.legacy ("database_path");
    if (!setup.standAlone && setup.dataDir.empty())
    {
        Throw<std::runtime_error>(
            "database_path must be set.");
    }

    auto const& section = c.section("sqdb");
    auto const backendName = get<std::string>(section, "backend", "sqlite");
    setup.poolSize =
        std::max<std::size_t>(1, get<std::size_t>(section, "pool_size", 1));

    if (backendName == "postgresql"s)
    {
        setup.backend = DatabaseCon::Backend::postgresql;
        setup.postgresql.emplace();
        auto& pg = *setup.postgresql;
        pg.user = get<std::string>(section, "user", "");
        pg.host = get<std::string>(section, "host", "");
        pg.port = get<std::string>(section, "port", "");
        pg.dbName = get<std::string>(section, "database_name", "rippled");
        pg.staticPoolSize = get<std::size_t>(section, "pool_size", 0);
        return setup;
    }

    if (backendName == "sqlite"s)
    {
        setup.backend = DatabaseCon::Backend::sqlite;
        return setup;
    }

    Throw<std::runtime_error> ("Unsupported soci backend: " + backendName);
}

void DatabaseCon::setupCheckpointing (JobQueue* q, Logs& l)
{
    if (! q)
        Throw<std::logic_error> ("No JobQueue");
    auto db = checkoutDb();
    checkpointer_ = makeCheckpointer (*db, *q, l);
}

} // ripple
