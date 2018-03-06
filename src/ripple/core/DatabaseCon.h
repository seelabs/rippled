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

#ifndef RIPPLE_APP_DATA_DATABASECON_H_INCLUDED
#define RIPPLE_APP_DATA_DATABASECON_H_INCLUDED

#include <ripple/core/Config.h>
#include <ripple/core/SociDB.h>

#include <boost/filesystem/path.hpp>
#include <boost/optional.hpp>

#include <mutex>
#include <string>

namespace soci {
class session;
}

namespace ripple {

class LockedSociSession
{
private:
    soci::connection_pool* pool_{nullptr};
    std::size_t poolPosition_;

public:
    explicit LockedSociSession(
        soci::connection_pool& pool,
        std::size_t poolPosition)
        : pool_{&pool}, poolPosition_{poolPosition}
    {
    }
    LockedSociSession(LockedSociSession&& rhs)
        : pool_{rhs.pool_}, poolPosition_{rhs.poolPosition_}
    {
        rhs.pool_ = nullptr;
    }
    LockedSociSession() = delete;
    LockedSociSession(LockedSociSession const& rhs) = delete;
    LockedSociSession&
    operator=(LockedSociSession const& rhs) = delete;

    ~LockedSociSession()
    {
        if (!pool_)
            return;

        pool_->give_back(poolPosition_);
    }

    soci::session*
    get()
    {
        assert(pool_);
        return &pool_->at(poolPosition_);
    }
    soci::session& operator*()
    {
        assert(pool_);
        return pool_->at(poolPosition_);
    }
    soci::session* operator->()
    {
        return get();
    }
};

class DatabaseCon
{
public:
    enum class Backend { sqlite, postgresql };
    struct PostgresqlSetup
    {
        std::string host;
        std::string user;
        std::string port;
        std::string dbName;
        std::size_t staticPoolSize{0};
    };

    struct Setup
    {
        Config::StartUpType startUp = Config::NORMAL;
        bool standAlone = false;
        boost::filesystem::path dataDir;
        Backend backend{Backend::sqlite};
        std::size_t poolSize{2};
        boost::optional<PostgresqlSetup> postgresql;
    };

    DatabaseCon(
        Setup const& setup,
        std::string const& name,
        std::vector<std::string> const& initStrings);

    LockedSociSession
    checkoutDb();

    void
    setupCheckpointing(JobQueue*, Logs&);

    static void
    initStaticPool(Setup const& setup);

    static bool
    useSqlite(Setup const& setup);
private:
    static bool
    useTempFiles(Setup const& setup);

    static void
    initPool(
        soci::connection_pool& pool,
        Setup const& setup,
        std::size_t poolSize,
        std::string const& strName);

    /** Connection pool for the exclusive use of this instance

        @note The exclusive pool serves two purposes: 1) If not all instances
        have the same connection parameters (when using the sqlite db, they do
        not), then the static pool can not be used. 2) If all connections were
        part of the static pool, then some databases could starve other
        databases of connections. Reserving some connections for the exclusive
        use of this instance prevents this.

        There must always be at least one connection in the pool.
     */
    soci::connection_pool pool_;

    /** Connection pool shared by all database connections

        @note This is useful for backends where all the instances have the
        same connection parameters (like rippled does with postgresql, but does
        not with sqlite). Backends that do not have the same connection
        parameters should not initialize the static pool.
     */
    static boost::optional<soci::connection_pool> staticPool_;
    std::unique_ptr<Checkpointer> checkpointer_;
};

DatabaseCon::Setup
setup_DatabaseCon(Config const& c);

}  // namespace ripple

#endif
