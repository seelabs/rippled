//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2023 Ripple Labs Inc.

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

#ifndef RIPPLE_BASICS_INTRUSIVEPOINTER_IPP_INCLUDED
#define RIPPLE_BASICS_INTRUSIVEPOINTER_IPP_INCLUDED

#include <ripple/basics/IntrusivePointer.h>

#include <ripple/basics/IntrusiveRefCounts.h>

#include <utility>

namespace ripple {

template <SharedIntrusiveRefCounted T>
template <CAdoptTag TAdoptTag>
SharedIntrusive<T>::SharedIntrusive(T* p, TAdoptTag) noexcept : ptr_{p}
{
    if constexpr (std::is_same_v<
                      TAdoptTag,
                      SharedIntrusiveAdoptIncrementStrongTag>)
    {
        if (p)
            p->addStrongRef();
    }
}

template <SharedIntrusiveRefCounted T>
SharedIntrusive<T>::SharedIntrusive(SharedIntrusive const& rhs)
    : ptr_{[&] {
        auto p = rhs.unsafeGetRawPtr();
        if (p)
            p->addStrongRef();
        return p;
    }()}
{
}

template <SharedIntrusiveRefCounted T>
template <class TT>
requires std::convertible_to<TT*, T*>
SharedIntrusive<T>::SharedIntrusive(SharedIntrusive<TT> const& rhs)
    : ptr_{[&] {
        auto p = rhs.unsafeGetRawPtr();
        if (p)
            p->addStrongRef();
        return p;
    }()}
{
}

template <SharedIntrusiveRefCounted T>
SharedIntrusive<T>::SharedIntrusive(SharedIntrusive&& rhs)
    : ptr_{rhs.unsafeExchange(nullptr)}
{
}

template <SharedIntrusiveRefCounted T>
template <class TT>
requires std::convertible_to<TT*, T*>
SharedIntrusive<T>::SharedIntrusive(SharedIntrusive<TT>&& rhs)
    : ptr_{rhs.unsafeExchange(nullptr)}
{
}

template <SharedIntrusiveRefCounted T>
SharedIntrusive<T>&
SharedIntrusive<T>::operator=(SharedIntrusive const& rhs)
{
    if (this == &rhs)
        return *this;
    auto p = rhs.unsafeGetRawPtr();
    if (p)
        p->addStrongRef();
    unsafeReleaseAndStore(p);
    return *this;
}

template <SharedIntrusiveRefCounted T>
template <class TT>
// clang-format off
requires std::convertible_to<TT*, T*>
    // clang-format on
    SharedIntrusive<T>&
    SharedIntrusive<T>::operator=(SharedIntrusive<TT> const& rhs)
{
    if constexpr (std::is_same_v<T, TT>)
    {
        if (this == &rhs)
            return *this;
    }
    auto p = rhs.unsafeGetRawPtr();
    if (p)
        p->addStrongRef();
    unsafeReleaseAndStore(p);
    return *this;
}

template <SharedIntrusiveRefCounted T>
SharedIntrusive<T>&
SharedIntrusive<T>::operator=(SharedIntrusive&& rhs)
{
    if (this == &rhs)
        return *this;

    unsafeReleaseAndStore(rhs.unsafeExchange(nullptr));
    return *this;
}

template <SharedIntrusiveRefCounted T>
template <class TT>
// clang-format off
requires std::convertible_to<TT*, T*>
    // clang-format on
    SharedIntrusive<T>&
    SharedIntrusive<T>::operator=(SharedIntrusive<TT>&& rhs)
{
    if constexpr (std::is_same_v<T, TT>)
    {
        if (this == &rhs)
            return *this;
    }

    unsafeReleaseAndStore(rhs.unsafeExchange(nullptr));
    return *this;
}

template <SharedIntrusiveRefCounted T>
template <CAdoptTag TAdoptTag>
void
SharedIntrusive<T>::adopt(T* p)
{
    if constexpr (std::is_same_v<
                      TAdoptTag,
                      SharedIntrusiveAdoptIncrementStrongTag>)
    {
        if (p)
            p->addStrongRef();
    }
    unsafeReleaseAndStore(p);
}

template <SharedIntrusiveRefCounted T>
SharedIntrusive<T>::~SharedIntrusive()
{
    unsafeReleaseAndStore(nullptr);
};

template <SharedIntrusiveRefCounted T>
template <SharedIntrusiveRefCounted TT>
SharedIntrusive<T>::SharedIntrusive(
    StaticCastTagSharedIntrusive,
    SharedIntrusive<TT> const& rhs)
    : ptr_{[&] {
        auto p = static_cast<T*>(rhs.unsafeGetRawPtr());
        if (p)
            p->addStrongRef();
        return p;
    }()}
{
}

template <SharedIntrusiveRefCounted T>
template <SharedIntrusiveRefCounted TT>
SharedIntrusive<T>::SharedIntrusive(
    StaticCastTagSharedIntrusive,
    SharedIntrusive<TT>&& rhs)
    : ptr_{static_cast<T*>(rhs.unsafeExchange(nullptr))}
{
}

template <SharedIntrusiveRefCounted T>
template <SharedIntrusiveRefCounted TT>
SharedIntrusive<T>::SharedIntrusive(
    DynamicCastTagSharedIntrusive,
    SharedIntrusive<TT> const& rhs)
    : ptr_{[&] {
        auto p = dynamic_cast<T*>(rhs.unsafeGetRawPtr());
        if (p)
            p->addStrongRef();
        return p;
    }()}
{
}

template <SharedIntrusiveRefCounted T>
template <SharedIntrusiveRefCounted TT>
SharedIntrusive<T>::SharedIntrusive(
    DynamicCastTagSharedIntrusive,
    SharedIntrusive<TT>&& rhs)
{
    auto toSet = rhs.unsafeExchange(nullptr);
    if (toSet)
    {
        ptr_ = dynamic_cast<T*>(toSet);
        if (!ptr_)
            // need to set the pointer back or will leak
            rhs.unsafeExchange(toSet);
    }
}

template <SharedIntrusiveRefCounted T>
T&
SharedIntrusive<T>::operator*() const noexcept
{
    return *unsafeGetRawPtr();
}

template <SharedIntrusiveRefCounted T>
T*
SharedIntrusive<T>::operator->() const noexcept
{
    return unsafeGetRawPtr();
}

template <SharedIntrusiveRefCounted T>
SharedIntrusive<T>::operator bool() const noexcept
{
    return bool(unsafeGetRawPtr());
}

template <SharedIntrusiveRefCounted T>
void
SharedIntrusive<T>::reset()
{
    unsafeReleaseAndStore(nullptr);
}

template <SharedIntrusiveRefCounted T>
T*
SharedIntrusive<T>::get() const
{
    return unsafeGetRawPtr();
}

template <SharedIntrusiveRefCounted T>
std::size_t
SharedIntrusive<T>::use_count() const
{
    if (auto p = unsafeGetRawPtr())
        return p->use_count();
    return 0;
}

template <SharedIntrusiveRefCounted T>
T*
SharedIntrusive<T>::unsafeGetRawPtr() const
{
    return ptr_;
}

template <SharedIntrusiveRefCounted T>
void
SharedIntrusive<T>::unsafeSetRawPtr(T* p)
{
    ptr_ = p;
}

template <SharedIntrusiveRefCounted T>
T*
SharedIntrusive<T>::unsafeExchange(T* p)
{
    return std::exchange(ptr_, p);
}

template <SharedIntrusiveRefCounted T>
void
SharedIntrusive<T>::unsafeReleaseAndStore(T* next)
{
    auto prev = unsafeExchange(next);
    if (!prev)
        return;

    using enum ReleaseRefAction;
    auto action = prev->releaseStrongRef();
    switch (action)
    {
        case noop:
            break;
        case destroy:
            delete prev;
            break;
        case partialDestroy:
            prev->partialDestructor();
            partialDestructorFinished(&prev);
            // prev is null and may no longer be used
            break;
    }
}

//------------------------------------------------------------------------------

template <SharedIntrusiveRefCounted T>
WeakIntrusive<T>::WeakIntrusive(WeakIntrusive const& rhs) : ptr_{rhs.ptr_}
{
    if (ptr_)
        ptr_->addWeakRef();
}

template <SharedIntrusiveRefCounted T>
WeakIntrusive<T>::WeakIntrusive(WeakIntrusive&& rhs) : ptr_{rhs.ptr_}
{
    rhs.ptr_ = nullptr;
}

template <SharedIntrusiveRefCounted T>
WeakIntrusive<T>::WeakIntrusive(SharedIntrusive<T> const& rhs)
    : ptr_{rhs.unsafeGetRawPtr()}
{
    if (ptr_)
        ptr_->addWeakRef();
}

template <SharedIntrusiveRefCounted T>
template <class TT>
// clang-format off
requires std::convertible_to<TT*, T*>
    // clang-format on
    WeakIntrusive<T>&
    WeakIntrusive<T>::operator=(SharedIntrusive<TT> const& rhs)
{
    unsafeReleaseNoStore();
    auto p = rhs.unsafeGetRawPtr();
    if (p)
        p->addWeakRef();
    return *this;
}

template <SharedIntrusiveRefCounted T>
void
WeakIntrusive<T>::adopt(T* ptr)
{
    unsafeReleaseNoStore();
    if (ptr)
        ptr->addWeakRef();
    ptr_ = ptr;
}

template <SharedIntrusiveRefCounted T>
WeakIntrusive<T>::~WeakIntrusive()
{
    unsafeReleaseNoStore();
}

template <SharedIntrusiveRefCounted T>
SharedIntrusive<T>
WeakIntrusive<T>::lock() const
{
    if (ptr_ && ptr_->checkoutStrongRefFromWeak())
    {
        return SharedIntrusive<T>{ptr_, SharedIntrusiveAdoptNoIncrementTag{}};
    }
    return {};
}

template <SharedIntrusiveRefCounted T>
bool
WeakIntrusive<T>::expired() const
{
    return (!ptr_ || ptr_->expired());
}

template <SharedIntrusiveRefCounted T>
void
WeakIntrusive<T>::reset()
{
    if (!ptr_)
        return;

    unsafeReleaseNoStore();
    ptr_ = nullptr;
}

template <SharedIntrusiveRefCounted T>
void
WeakIntrusive<T>::unsafeReleaseNoStore()
{
    if (!ptr_)
        return;

    using enum ReleaseRefAction;
    auto action = ptr_->releaseWeakRef();
    switch (action)
    {
        case noop:
            break;
        case destroy:
            delete ptr_;
            break;
        case partialDestroy:
            assert(0);  // only a strong pointer should case a
                        // partialDestruction
            ptr_->partialDestructor();
            partialDestructorFinished(&ptr_);
            // ptr_ is null and may no longer be used
            break;
    }
}

//------------------------------------------------------------------------------

template <SharedIntrusiveRefCounted T>
SharedWeakUnion<T>::SharedWeakUnion(SharedWeakUnion const& rhs) : tp_{rhs.tp_}
{
    auto p = rhs.unsafeGetRawPtr();
    if (!p)
        return;

    if (rhs.isStrong())
        p->addStrongRef();
    else
        p->addWeakRef();
}

template <SharedIntrusiveRefCounted T>
template <class TT>
requires std::convertible_to<TT*, T*>
SharedWeakUnion<T>::SharedWeakUnion(SharedIntrusive<TT> const& rhs)
{
    auto p = rhs.unsafeGetRawPtr();
    if (p)
        p->addStrongRef();
    unsafeSetRawPtr(p, /*isStrong*/ true);
}

template <SharedIntrusiveRefCounted T>
SharedWeakUnion<T>::SharedWeakUnion(SharedWeakUnion&& rhs) : tp_{rhs.tp_}
{
    rhs.unsafeSetRawPtr(nullptr);
}

template <SharedIntrusiveRefCounted T>
template <class TT>
requires std::convertible_to<TT*, T*>
SharedWeakUnion<T>::SharedWeakUnion(SharedIntrusive<TT>&& rhs)
{
    auto p = rhs.unsafeGetRawPtr();
    if (p)
        unsafeSetRawPtr(p, /*isStrong*/ true);
    rhs.unsafeSetRawPtr(nullptr);
}

template <SharedIntrusiveRefCounted T>
SharedWeakUnion<T>&
SharedWeakUnion<T>::operator=(SharedWeakUnion const& rhs)
{
    if (this == &rhs)
        return *this;
    unsafeReleaseNoStore();

    if (auto p = rhs.unsafeGetRawPtr())
    {
        if (rhs.isStrong())
        {
            p->addStrongRef();
            unsafeSetRawPtr(p, /*isStrong*/ true);
        }
        else
        {
            p->addWeakRef();
            unsafeSetRawPtr(p, /*isStrong*/ false);
        }
    }
    else
    {
        unsafeSetRawPtr(nullptr);
    }
    return *this;
}

template <SharedIntrusiveRefCounted T>
template <class TT>
// clang-format off
requires std::convertible_to<TT*, T*>
    // clang-format on
    SharedWeakUnion<T>&
    SharedWeakUnion<T>::operator=(SharedIntrusive<TT> const& rhs)
{
    unsafeReleaseNoStore();
    auto p = rhs.unsafeGetRawPtr();
    if (p)
        p->addStrongRef();
    unsafeSetRawPtr(p, /*isStrong*/ true);
    return *this;
}

template <SharedIntrusiveRefCounted T>
template <class TT>
// clang-format off
requires std::convertible_to<TT*, T*>
    // clang-format on
    SharedWeakUnion<T>&
    SharedWeakUnion<T>::operator=(SharedIntrusive<TT>&& rhs)
{
    unsafeReleaseNoStore();
    unsafeSetRawPtr(rhs.unsafeGetRawPtr(), /*isStrong*/ true);
    rhs.unsafeSetRawPtr(nullptr);
    return *this;
}

template <SharedIntrusiveRefCounted T>
SharedWeakUnion<T>::~SharedWeakUnion()
{
    unsafeReleaseNoStore();
};

// Return a strong pointer if this is already a strong pointer (i.e. don't
// lock the weak pointer. Use the `lock` method if that's what's needed)
template <SharedIntrusiveRefCounted T>
SharedIntrusive<T>
SharedWeakUnion<T>::getStrong() const
{
    SharedIntrusive<T> result;
    auto p = unsafeGetRawPtr();
    if (p && isStrong())
    {
        result.template adopt<SharedIntrusiveAdoptIncrementStrongTag>(p);
    }
    return result;
}

template <SharedIntrusiveRefCounted T>
SharedWeakUnion<T>::operator bool() const noexcept
{
    return bool(get());
}

template <SharedIntrusiveRefCounted T>
void
SharedWeakUnion<T>::reset()
{
    unsafeReleaseNoStore();
    unsafeSetRawPtr(nullptr);
}

template <SharedIntrusiveRefCounted T>
T*
SharedWeakUnion<T>::get() const
{
    return isStrong() ? unsafeGetRawPtr() : nullptr;
}

template <SharedIntrusiveRefCounted T>
std::size_t
SharedWeakUnion<T>::use_count() const
{
    if (auto p = get())
        return p->use_count();
    return 0;
}

template <SharedIntrusiveRefCounted T>
bool
SharedWeakUnion<T>::expired() const
{
    auto p = unsafeGetRawPtr();
    return (!p || p->expired());
}

template <SharedIntrusiveRefCounted T>
SharedIntrusive<T>
SharedWeakUnion<T>::lock() const
{
    SharedIntrusive<T> result;
    auto p = unsafeGetRawPtr();
    if (!p)
        return result;

    if (isStrong())
    {
        result.template adopt<SharedIntrusiveAdoptIncrementStrongTag>(p);
        return result;
    }

    if (p->checkoutStrongRefFromWeak())
    {
        result.template adopt<SharedIntrusiveAdoptNoIncrementTag>(p);
        return result;
    }
    return result;
}

template <SharedIntrusiveRefCounted T>
bool
SharedWeakUnion<T>::isStrong() const
{
    return !(tp_ & tagMask);
}

template <SharedIntrusiveRefCounted T>
bool
SharedWeakUnion<T>::isWeak() const
{
    return tp_ & tagMask;
}

template <SharedIntrusiveRefCounted T>
bool
SharedWeakUnion<T>::convertToStrong()
{
    if (isStrong())
        return true;

    auto p = unsafeGetRawPtr();
    if (p && p->checkoutStrongRefFromWeak())
    {
        auto action = p->releaseWeakRef();
        (void)action;
        assert(action == ReleaseRefAction::noop);
        unsafeSetRawPtr(p, /*isStrong*/ true);
        return true;
    }
    return false;
}

template <SharedIntrusiveRefCounted T>
bool
SharedWeakUnion<T>::convertToWeak()
{
    if (isWeak())
        return true;

    auto p = unsafeGetRawPtr();
    if (!p)
        return false;

    using enum ReleaseRefAction;
    auto action = p->addWeakReleaseStrongRef();
    switch (action)
    {
        case noop:
            break;
        case destroy:
            // We just added a weak ref. How could we destroy?
            assert(0);
            delete p;
            unsafeSetRawPtr(nullptr);
            return true;  // Should never happen
        case partialDestroy:
            // This is a weird case. We just converted the last strong
            // pointer to a weak pointer.
            p->partialDestructor();
            partialDestructorFinished(&p);
            // p is null and may no longer be used
            break;
    }
    unsafeSetRawPtr(p, /*isStrong*/ false);
    return true;
}

template <SharedIntrusiveRefCounted T>
T*
SharedWeakUnion<T>::unsafeGetRawPtr() const
{
    return reinterpret_cast<T*>(tp_ & ptrMask);
}

template <SharedIntrusiveRefCounted T>
void
SharedWeakUnion<T>::unsafeSetRawPtr(T* p, bool isStrong)
{
    tp_ = reinterpret_cast<std::uintptr_t>(p);
    if (tp_ && !isStrong)
        tp_ |= tagMask;
}

template <SharedIntrusiveRefCounted T>
void SharedWeakUnion<T>::unsafeSetRawPtr(std::nullptr_t)
{
    tp_ = 0;
}

template <SharedIntrusiveRefCounted T>
void
SharedWeakUnion<T>::unsafeReleaseNoStore()
{
    auto p = unsafeGetRawPtr();
    if (!p)
        return;

    using enum ReleaseRefAction;
    auto action = isStrong() ? p->releaseStrongRef() : p->releaseWeakRef();
    switch (action)
    {
        case noop:
            break;
        case destroy:
            delete p;
            break;
        case partialDestroy:
            p->partialDestructor();
            partialDestructorFinished(&p);
            // p is null and may no longer be used
            break;
    }
}

}  // namespace ripple
#endif
