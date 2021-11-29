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

#include <boost/icl/type_traits/is_element_container.hpp>
namespace ripple {

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::TaggedCache(
    std::string const& name,
    int size,
    clock_type::duration expiration,
    clock_type& clock,
    beast::Journal journal,
    beast::insight::Collector::ptr const& collector)
    : m_journal(journal)
    , m_clock(clock)
    , m_stats(name, std::bind(&TaggedCache::collect_metrics, this), collector)
    , m_name(name)
    , m_target_size(size)
    , m_target_age(expiration)
    , m_cache_count(0)
    , m_hits(0)
    , m_misses(0)
{
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::clock() -> clock_type&
{
    return m_clock;
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::size() const
    -> std::size_t
{
    return m_cache.size();
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::setTargetSize(int s)
    -> void
{
    m_cache.reserve(s);
    JLOG(m_journal.debug()) << m_name << " target size set to " << s;
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::getTargetAge()
    -> clock_type::duration const
{
    return m_target_age;
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::setTargetAge(
    clock_type::duration s) -> void
{
    m_target_age = s;
    JLOG(m_journal.debug())
        << m_name << " target age set to " << m_target_age.load().count();
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::getCacheSize() const
    -> int
{
    return m_cache_count;
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::getTrackSize() const
    -> int
{
    return m_cache.size();
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::getHitRate() -> float
{
    auto const total = static_cast<float>(m_hits + m_misses);
    return m_hits * (100.0f / std::max(1.0f, total));
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::clear() -> void
{
    m_cache.clear();
    m_cache_count = 0;
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::reset() -> void
{
    m_cache.clear();
    m_cache_count = 0;
    m_hits = 0;
    m_misses = 0;
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
template <class KeyComparable>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::touch_if_exists(
    KeyComparable const& key) -> bool
{
    while (1)
    {
        auto const iter(m_cache.find(key));
        if (iter == m_cache.end())
        {
            ++m_stats.misses;
            return false;
        }
        Entry const oldValue = iter->second;
        Entry newValue = oldValue;
        newValue.touch(m_clock.now());

        // Check if overwritten in meantime, repeat if it has been
        if (m_cache.assign_if_equal(key, oldValue, std::move(newValue)))
        {
            ++m_stats.hits;
            return true;
        }
    }
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::sweep() -> void
{
    clock_type::time_point const now(m_clock.now());
    clock_type::time_point when_expire;

    auto const start = std::chrono::steady_clock::now();

    auto const target_age = m_target_age.load();

    if (m_target_size == 0 ||
        (static_cast<int>(m_cache.size()) <= m_target_size))
    {
        when_expire = now - target_age;
    }
    else
    {
        when_expire = now - target_age * m_target_size / m_cache.size();

        clock_type::duration const minimumAge(std::chrono::seconds(1));
        if (when_expire > (now - minimumAge))
            when_expire = now - minimumAge;

        JLOG(m_journal.trace())
            << m_name << " is growing fast " << m_cache.size() << " of "
            << m_target_size << " aging at " << (now - when_expire).count()
            << " of " << target_age.count();
    }

    auto const allRemovals = sweepHelper(when_expire, now, m_cache);

    m_cache_count -= allRemovals;

    JLOG(m_journal.debug())
        << m_name << " TaggedCache sweep lock duration "
        << std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start)
               .count()
        << "ms";
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::del(
    const key_type& key,
    bool valid) -> bool
{
    // Remove from cache, if !valid, remove from map too. Returns true if
    // removed from cache
    while (1)
    {
        auto cit = m_cache.find(key);

        if (cit == m_cache.end())
            return false;

        Entry const oldEntry = cit->second;
        std::optional<Entry> newEntry;

        bool ret = false;

        if (oldEntry.isCached())
        {
            newEntry = oldEntry;
            newEntry->ptr.reset();
            ret = true;
        }

        if (!valid || oldEntry.isExpired())
        {
            m_cache.erase(cit);
        }
        else if (newEntry)
        {
            if (!m_cache.assign_if_equal(key, oldEntry, std::move(*newEntry)))
                continue;
            else
                --m_cache_count;
        }

        return ret;
    }
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
template <bool replace>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::canonicalize(
    const key_type& key,
    std::conditional_t<replace, std::shared_ptr<T> const, std::shared_ptr<T>>&
        data) -> bool
{
    // Return canonical value, store if needed, refresh in cache
    // Return values: true=we had the data already

    auto const now = m_clock.now();
    auto [cit, inserted] = m_cache.try_emplace(key, now, data);

    if (inserted)
    {
        ++m_cache_count;
        return false;
    }

    Entry entry = cit->second;
    entry.touch(now);

    if (entry.isCached())
    {
        if constexpr (replace)
        {
            entry.ptr = data;
            entry.weak_ptr = data;
        }
        else
        {
            data = entry.ptr;
        }

        m_cache.insert_or_assign(key, std::move(entry));
        return true;
    }

    auto cachedData = entry.lock();

    if (cachedData)
    {
        if constexpr (replace)
        {
            entry.ptr = data;
            entry.weak_ptr = data;
        }
        else
        {
            entry.ptr = cachedData;
            data = cachedData;
        }

        ++m_cache_count;
        m_cache.insert_or_assign(key, std::move(entry));
        return true;
    }

    entry.ptr = data;
    entry.weak_ptr = data;
    m_cache.insert_or_assign(key, std::move(entry));
    ++m_cache_count;

    return false;
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::
    canonicalize_replace_cache(
        const key_type& key,
        std::shared_ptr<T> const& data) -> bool
{
    return canonicalize<true>(key, data);
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::
    canonicalize_replace_client(const key_type& key, std::shared_ptr<T>& data)
        -> bool
{
    return canonicalize<false>(key, data);
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::fetch(
    const key_type& key) -> std::shared_ptr<T>
{
    auto ret = initialFetch(key);
    if (!ret)
        ++m_misses;
    return ret;
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
template <class ReturnType>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::insert(
    key_type const& key,
    T const& value) -> std::enable_if_t<!IsKeyCache, ReturnType>
{
    auto p = std::make_shared<T>(std::cref(value));
    return canonicalize_replace_client(key, p);
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
template <class ReturnType>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::insert(
    key_type const& key) -> std::enable_if_t<IsKeyCache, ReturnType>
{
    clock_type::time_point const now(m_clock.now());
    auto [it, inserted] = m_cache.insert(key, now);
    if (!inserted)
    {
        Entry entry = it->second;
        entry.touch(now);
        m_cache.insert_or_assign(key, std::move(entry));
    }
    return inserted;
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::retrieve(
    const key_type& key,
    T& data) -> bool
{
    // retrieve the value of the stored data
    auto entry = fetch(key);

    if (!entry)
        return false;

    data = *entry;
    return true;
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::getKeys() const
    -> std::vector<key_type>
{
    std::vector<key_type> v;
    v.reserve(m_cache.size());

    for (auto const& _ : m_cache)
        v.push_back(_.first);

    return v;
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::rate() const -> double
{
    auto const tot = m_hits + m_misses;
    if (tot == 0)
        return 0;
    return double(m_hits) / tot;
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
template <class Handler>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::fetch(
    key_type const& digest,
    Handler const& h) -> std::shared_ptr<T>
{
    if (auto ret = initialFetch(digest))
        return ret;

    auto sle = h();
    if (!sle)
        return {};

    ++m_misses;
    while (1)
    {
        auto const [it, inserted] =
            m_cache.try_emplace(digest, m_clock.now(), sle);
        if (inserted)
            return it->second.ptr;

        Entry const oldEntry = it->second;
        Entry newEntry = it->second;
        newEntry.touch(m_clock.now());
        if (m_cache.assign_if_equal(digest, oldEntry, std::move(newEntry)))
        {
            // return oldEntry b/c newEntry has been moved, and oldEntry has the
            // same ptr
            return oldEntry.ptr;
        }
    }
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::initialFetch(
    key_type const& key) -> std::shared_ptr<T>
{
    auto cit = m_cache.find(key);
    if (cit == m_cache.end())
        return {};

    Entry entry = cit->second;
    if (entry.isCached())
    {
        ++m_hits;
        entry.touch(m_clock.now());
        auto result = entry.ptr;  // make copy before moved from
        m_cache.insert_or_assign(key, std::move(entry));
        return result;
    }
    entry.ptr = entry.lock();
    if (entry.isCached())
    {
        // independent of cache size, so not counted as a hit
        ++m_cache_count;
        entry.touch(m_clock.now());
        auto result = entry.ptr;  // make copy before moved from
        m_cache.insert_or_assign(key, std::move(entry));
        return result;
    }

    m_cache.erase(cit);
    return {};
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::collect_metrics()
    -> void
{
    m_stats.size.set(getCacheSize());

    beast::insight::Gauge::value_type hit_rate(0);
    auto const total(m_hits + m_misses);
    if (total != 0)
        hit_rate = (m_hits * 100) / total;
    m_stats.hit_rate.set(hit_rate);
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::sweepHelper(
    clock_type::time_point const& when_expire,
    [[maybe_unused]] clock_type::time_point const& now,
    KeyValueCacheType& partition) -> std::size_t
{
    std::size_t cacheRemovals = 0;
    int mapRemovals = 0;

    auto cit = partition.begin();
    while (cit != partition.end())
    {
        if (cit->second.isWeak())
        {
            // weak
            if (cit->second.isExpired())
            {
                ++mapRemovals;
                cit = partition.erase(cit);
            }
            else
            {
                ++cit;
            }
        }
        else if (cit->second.last_access <= when_expire)
        {
            // strong, expired
            ++cacheRemovals;
            if (cit->second.ptr.unique())
            {
                ++mapRemovals;
                cit = partition.erase(cit);
            }
            else
            {
                // remains weakly cached
                Entry entry = cit->second;
                entry.ptr.reset();
                std::tie(cit, std::ignore) =
                    m_cache.insert_or_assign(cit->first, std::move(entry));
                ++cit;
            }
        }
        else
        {
            // strong, not expired
            ++cit;
        }
    }

    if (mapRemovals || cacheRemovals)
    {
        JLOG(m_journal.debug()) << "TaggedCache partition sweep " << m_name
                                << ": cache = " << partition.size() << "-"
                                << cacheRemovals << ", map-=" << mapRemovals;
    }

    return cacheRemovals;
}

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
auto
TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::sweepHelper(
    clock_type::time_point const& when_expire,
    clock_type::time_point const& now,
    KeyOnlyCacheType& partition) -> std::size_t
{
    int cacheRemovals = 0;
    int mapRemovals = 0;

    {
        auto cit = partition.begin();
        while (cit != partition.end())
        {
            if (cit->second.last_access > now)
            {
                Entry entry = cit->second;
                entry.touch(now);
                std::tie(cit, std::ignore) =
                    m_cache.insert_or_assign(cit->first, std::move(entry));
                ++cit;
            }
            else if (cit->second.last_access <= when_expire)
            {
                cit = partition.erase(cit);
            }
            else
            {
                ++cit;
            }
        }
    }

    if (mapRemovals || cacheRemovals)
    {
        JLOG(m_journal.debug()) << "TaggedCache partition sweep " << m_name
                                << ": cache = " << partition.size() << "-"
                                << cacheRemovals << ", map-=" << mapRemovals;
    }

    return cacheRemovals;
};

template <
    class Key,
    class T,
    bool IsKeyCache,
    class Hash,
    class KeyEqual,
    class Mutex>
inline Hash
    TaggedCache<Key, T, IsKeyCache, Hash, KeyEqual, Mutex>::HashWrapper::h_;

}  // namespace ripple
