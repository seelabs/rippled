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

#ifndef RIPPLE_BASICS_TAGGEDCACHE_H_INCLUDED
#define RIPPLE_BASICS_TAGGEDCACHE_H_INCLUDED

#include <ripple/basics/Log.h>
#include <ripple/basics/UnorderedContainers.h>
#include <ripple/basics/hardened_hash.h>
#include <ripple/beast/clock/abstract_clock.h>
#include <ripple/beast/insight/Insight.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

namespace ripple {

/** Map/cache combination.
    This class implements a cache and a map. The cache keeps objects alive
    in the map. The map allows multiple code paths that reference objects
    with the same tag to get the same actual object.

    So long as data is in the cache, it will stay in memory.
    If it stays in memory even after it is ejected from the cache,
    the map will track it.

    @note Callers must not modify data objects that are stored in the cache
          unless they hold their own lock over all cache operations.
*/
template <
    class Key,
    class T,
    bool IsKeyCache = false,
    class Hash = hardened_hash<>,
    class KeyEqual = std::equal_to<Key>,
    class Mutex = std::recursive_mutex>
class TaggedCache
{
public:
    using mutex_type = Mutex;
    using key_type = Key;
    using mapped_type = T;
    using clock_type = beast::abstract_clock<std::chrono::steady_clock>;

    TaggedCache(
        std::string const& name,
        int size,
        clock_type::duration expiration,
        clock_type& clock,
        beast::Journal journal,
        beast::insight::Collector::ptr const& collector =
            beast::insight::NullCollector::New());

    /** Return the clock associated with the cache. */
    auto
    clock() -> clock_type&;

    /** Returns the number of items in the container. */
    auto
    size() const -> std::size_t;

    auto
    setTargetSize(int s) -> void;

    auto
    getTargetAge() -> clock_type::duration const;

    auto
    setTargetAge(clock_type::duration s) -> void;

    auto
    getCacheSize() const -> int;

    auto
    getTrackSize() const -> int;

    auto
    getHitRate() -> float;

    auto
    clear() -> void;

    auto
    reset() -> void;

    /** Refresh the last access time on a key if present.
        @return `true` If the key was found.
    */
    template <class KeyComparable>
    auto
    touch_if_exists(KeyComparable const& key) -> bool;

    auto
    sweep() -> void;

    auto
    del(const key_type& key, bool valid) -> bool;

    /** Replace aliased objects with originals.

        Due to concurrency it is possible for two separate objects with
        the same content and referring to the same unique "thing" to exist.
        This routine eliminates the duplicate and performs a replacement
        on the callers shared pointer if needed.

        @param key The key corresponding to the object
        @param data A shared pointer to the data corresponding to the object.
        @param replace `true` if `data` is the up to date version of the object.

        @return `true` If the key already existed.
    */
private:
    template <bool replace>
    auto
    canonicalize(
        const key_type& key,
        std::conditional_t<
            replace,
            std::shared_ptr<T> const,
            std::shared_ptr<T>>& data) -> bool;

public:
    auto
    canonicalize_replace_cache(
        const key_type& key,
        std::shared_ptr<T> const& data) -> bool;

    auto
    canonicalize_replace_client(const key_type& key, std::shared_ptr<T>& data)
        -> bool;

    auto
    fetch(const key_type& key) -> std::shared_ptr<T>;

    /** Insert the element into the container.
        If the key already exists, nothing happens.
        @return `true` If the element was inserted
    */
    template <class ReturnType = bool>
    auto
    insert(key_type const& key, T const& value)
        -> std::enable_if_t<!IsKeyCache, ReturnType>;

    template <class ReturnType = bool>
    auto
    insert(key_type const& key) -> std::enable_if_t<IsKeyCache, ReturnType>;

    // VFALCO NOTE It looks like this returns a copy of the data in
    //             the output parameter 'data'. This could be expensive.
    //             Perhaps it should work like standard containers, which
    //             simply return an iterator.
    //
    auto
    retrieve(const key_type& key, T& data) -> bool;

    auto
    peekMutex() -> mutex_type&;

    auto
    getKeys() const -> std::vector<key_type>;

    // CachedSLEs functions.
    /** Returns the fraction of cache hits. */
    auto
    rate() const -> double;

    /** Fetch an item from the cache.
        If the digest was not found, Handler
        will be called with this signature:
            std::shared_ptr<SLE const>(void)
    */
    template <class Handler>
    auto
    fetch(key_type const& digest, Handler const& h) -> std::shared_ptr<T>;

    // End CachedSLEs functions.

private:
    auto
    initialFetch(key_type const& key, std::lock_guard<mutex_type> const& l)
        -> std::shared_ptr<T>;

    auto
    collect_metrics() -> void;

private:
    struct Stats
    {
        template <class Handler>
        Stats(
            std::string const& prefix,
            Handler const& handler,
            beast::insight::Collector::ptr const& collector)
            : hook(collector->make_hook(handler))
            , size(collector->make_gauge(prefix, "size"))
            , hit_rate(collector->make_gauge(prefix, "hit_rate"))
            , hits(0)
            , misses(0)
        {
        }

        beast::insight::Hook hook;
        beast::insight::Gauge size;
        beast::insight::Gauge hit_rate;

        std::size_t hits;
        std::size_t misses;
    };

    class KeyOnlyEntry
    {
    public:
        clock_type::time_point last_access;

        explicit KeyOnlyEntry(clock_type::time_point const& last_access_)
            : last_access(last_access_)
        {
        }

        auto
        touch(clock_type::time_point const& now) -> void
        {
            last_access = now;
        }
    };

    class ValueEntry
    {
    public:
        std::shared_ptr<mapped_type> ptr;
        std::weak_ptr<mapped_type> weak_ptr;
        clock_type::time_point last_access;

        ValueEntry(
            clock_type::time_point const& last_access_,
            std::shared_ptr<mapped_type> const& ptr_)
            : ptr(ptr_), weak_ptr(ptr_), last_access(last_access_)
        {
        }

        auto
        isWeak() const -> bool
        {
            return ptr == nullptr;
        }
        auto
        isCached() const -> bool
        {
            return ptr != nullptr;
        }
        auto
        isExpired() const -> bool
        {
            return weak_ptr.expired();
        }
        auto
        lock() -> std::shared_ptr<mapped_type>
        {
            return weak_ptr.lock();
        }
        auto
        touch(clock_type::time_point const& now) -> void
        {
            last_access = now;
        }
    };

    typedef
        typename std::conditional<IsKeyCache, KeyOnlyEntry, ValueEntry>::type
            Entry;

    using KeyOnlyCacheType =
        hardened_partitioned_hash_map<key_type, KeyOnlyEntry, Hash, KeyEqual>;

    using KeyValueCacheType =
        hardened_partitioned_hash_map<key_type, ValueEntry, Hash, KeyEqual>;

    using cache_type =
        hardened_partitioned_hash_map<key_type, Entry, Hash, KeyEqual>;

    [[nodiscard]] auto
    sweepHelper(
        clock_type::time_point const& when_expire,
        [[maybe_unused]] clock_type::time_point const& now,
        typename KeyValueCacheType::map_type& partition,
        std::vector<std::shared_ptr<mapped_type>>& stuffToSweep,
        std::atomic<int>& allRemovals,
        std::lock_guard<std::recursive_mutex> const& lock) -> std::thread;

    [[nodiscard]] auto
    sweepHelper(
        clock_type::time_point const& when_expire,
        clock_type::time_point const& now,
        typename KeyOnlyCacheType::map_type& partition,
        std::vector<std::shared_ptr<mapped_type>>& stuffToSweep,
        std::atomic<int>& allRemovals,
        std::lock_guard<std::recursive_mutex> const& lock) -> std::thread;

    beast::Journal m_journal;
    clock_type& m_clock;
    Stats m_stats;

    mutex_type mutable m_mutex;

    // Used for logging
    std::string m_name;

    // Desired number of cache entries (0 = ignore)
    int m_target_size;

    // Desired maximum cache age
    clock_type::duration m_target_age;

    // Number of items cached
    int m_cache_count;
    cache_type m_cache;  // Hold strong reference to recent objects
    std::uint64_t m_hits;
    std::uint64_t m_misses;
};

}  // namespace ripple

#include <ripple/basics/impl/TaggedCache.ipp>

#endif
