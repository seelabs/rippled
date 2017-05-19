//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2016 Ripple Labs Inc.

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

#ifndef RIPPLE_TEST_CONDITIONS_DERCHOICE_H_INCLUDED
#define RIPPLE_TEST_CONDITIONS_DERCHOICE_H_INCLUDED

#include <ripple/conditions/impl/Der.h>

#include <algorithm>
#include <fstream>
#include <string>

#include <boost/filesystem.hpp>

namespace ripple {

namespace test {

struct DerChoiceBaseClass
{
    virtual std::uint8_t
    type() const = 0;

    virtual void
    encode(cryptoconditions::der::Encoder& s) const = 0;

    virtual void
    decode(cryptoconditions::der::Decoder& s) = 0;

    // for debugging
    virtual void
    print(std::ostream& ostr) const = 0;
};

bool
equal(DerChoiceBaseClass const* lhs, DerChoiceBaseClass const* rhs);

inline
bool
equal(
    std::unique_ptr<DerChoiceBaseClass> const& lhs,
    std::unique_ptr<DerChoiceBaseClass> const& rhs)
{
    return equal(lhs.get(), rhs.get());
}

inline
bool
operator==(
    std::unique_ptr<DerChoiceBaseClass> const& lhs,
    std::unique_ptr<DerChoiceBaseClass> const& rhs)
{
    return equal(lhs, rhs);
}

inline
bool
operator!=(
    std::unique_ptr<DerChoiceBaseClass> const& lhs,
    std::unique_ptr<DerChoiceBaseClass> const& rhs)
{
    return !operator==(lhs, rhs);
}

// Needed because der sets are sorted, so comparing deserialized der sets
// with the original need to ignore order
bool
operator<(
    std::unique_ptr<DerChoiceBaseClass> const& lhs,
    std::unique_ptr<DerChoiceBaseClass> const& rhs);

struct DerChoiceDerived1 : DerChoiceBaseClass
{
    Buffer buf_;
    std::vector<std::unique_ptr<DerChoiceBaseClass>> subChoices_;
    std::int32_t signedInt_;

    DerChoiceDerived1() = default;

    DerChoiceDerived1(
        std::vector<char> const& b,
        std::vector<std::unique_ptr<DerChoiceBaseClass>> sub,
        std::int32_t si)
        : buf_(makeSlice(b)), subChoices_(std::move(sub)), signedInt_(si)
    {
    }

    std::uint8_t
    type() const override
    {
        return 1;
    }

    template <class Coder>
    void
    serialize(Coder& c)
    {
        auto subAsSeq = cryptoconditions::der::make_sequence(subChoices_);
        c& std::tie(buf_, subAsSeq, signedInt_);
    }

    void
    encode(cryptoconditions::der::Encoder& encoder) const override
    {
        const_cast<DerChoiceDerived1*>(this)->serialize(encoder);
    }

    void
    decode(cryptoconditions::der::Decoder& decoder) override
    {
        serialize(decoder);
    }

    void
    print(std::ostream& ostr) const override
    {
        ostr << "{d1;\n" << signedInt_ << ";\n";
        auto const d = buf_.data();
        ostr << '{';
        for (std::size_t i = 0; i < buf_.size(); ++i)
        {
            if (i)
                ostr << ", ";
            ostr << int(d[i]);
        }
        ostr << "};";
        ostr << '{';
        for (auto const& c : subChoices_)
            c->print(ostr);
        ostr << "}\n}\n";
    }

    friend bool
    operator==(DerChoiceDerived1 const& lhs, DerChoiceDerived1 const& rhs)
    {
        if (lhs.buf_ != rhs.buf_ || lhs.signedInt_ != rhs.signedInt_ ||
            lhs.subChoices_.size() != rhs.subChoices_.size())
            return false;

        for (size_t i = 0; i < lhs.subChoices_.size(); ++i)
            if (!equal(lhs.subChoices_[i], rhs.subChoices_[i]))
                return false;
        return true;
    }

    friend bool
    operator!=(DerChoiceDerived1 const& lhs, DerChoiceDerived1 const& rhs)
    {
        return !(lhs == rhs);
    }
};

struct DerChoiceDerived2 : DerChoiceBaseClass
{
    std::string name_;
    std::uint64_t id_;

    DerChoiceDerived2() = default;
    DerChoiceDerived2(std::string const& n, std::uint64_t i) : name_(n), id_(i)
    {
    }

    std::uint8_t
    type() const override
    {
        return 2;
    }

    template <class Coder>
    void
    serialize(Coder& c)
    {
        c& std::tie(name_, id_);
    }

    void
    encode(cryptoconditions::der::Encoder& encoder) const override
    {
        const_cast<DerChoiceDerived2*>(this)->serialize(encoder);
    }

    void
    decode(cryptoconditions::der::Decoder& decoder) override
    {
        serialize(decoder);
    }

    void
    print(std::ostream& ostr) const override
    {
        ostr << "{d2;\n" << name_ << ";\n" << id_ << ";}\n";
    }

    friend bool
    operator==(DerChoiceDerived2 const& lhs, DerChoiceDerived2 const& rhs)
    {
        return lhs.name_ == rhs.name_ && lhs.id_ == rhs.id_;
    }
    friend bool
    operator!=(DerChoiceDerived2 const& lhs, DerChoiceDerived2 const& rhs)
    {
        return !(lhs == rhs);
    }
};

struct DerChoiceDerived3 : DerChoiceBaseClass
{
    // DER set
    std::vector<std::unique_ptr<DerChoiceBaseClass>> subChoices_;

    DerChoiceDerived3() = default;

    DerChoiceDerived3(std::vector<std::unique_ptr<DerChoiceBaseClass>> sub)
        : subChoices_(std::move(sub))
    {
    }

    std::uint8_t
    type() const override
    {
        return 3;
    }

    template <class Coder>
    void
    serialize(Coder& c)
    {
        auto subAsSet = cryptoconditions::der::make_set(subChoices_);
        c& std::tie(subAsSet);
    }

    void
    encode(cryptoconditions::der::Encoder& encoder) const override
    {
        const_cast<DerChoiceDerived3*>(this)->serialize(encoder);
    }

    void
    decode(cryptoconditions::der::Decoder& decoder) override
    {
        serialize(decoder);
    }

    void
    print(std::ostream& ostr) const override
    {
        ostr << "{d3;\n";
        ostr << '{';
        for (auto const& c : subChoices_)
            c->print(ostr);
        ostr << "}\n}\n";
    }

    friend bool
    operator==(DerChoiceDerived3 const& lhs, DerChoiceDerived3 const& rhs)
    {
        if (lhs.subChoices_.size() != rhs.subChoices_.size())
            return false;

        // Order doesn't matter (these are der sets
        std::vector<DerChoiceBaseClass const*> rhsChoices;
        rhsChoices.reserve(rhs.subChoices_.size());
        for (auto const& s : rhs.subChoices_)
            rhsChoices.push_back(s.get());

        for (size_t i = 0; i < lhs.subChoices_.size(); ++i)
        {
            auto rhsIt = std::find_if(
                rhsChoices.begin(), rhsChoices.end(), [&](auto elem) {
                    return equal(elem, lhs.subChoices_[i].get());
                });
            if (rhsIt == rhsChoices.end())
                return false;
            *rhsIt = nullptr;  // let it be found exactly once
        }
        return true;
    }

    friend bool
    operator!=(DerChoiceDerived3 const& lhs, DerChoiceDerived3 const& rhs)
    {
        return !(lhs == rhs);
    }
};

struct DerChoiceDerived4 : DerChoiceBaseClass
{
    // DER set
    std::vector<std::unique_ptr<DerChoiceBaseClass>> subChoices_;

    DerChoiceDerived4() = default;

    DerChoiceDerived4(std::vector<std::unique_ptr<DerChoiceBaseClass>> sub)
        : subChoices_(std::move(sub))
    {
    }

    std::uint8_t
    type() const override
    {
        return 4;
    }

    template <class Coder>
    void
    serialize(Coder& c)
    {
        auto subAsSeq = cryptoconditions::der::make_sequence(subChoices_);
        c& std::tie(subAsSeq);
    }

    void
    encode(cryptoconditions::der::Encoder& encoder) const override
    {
        const_cast<DerChoiceDerived4*>(this)->serialize(encoder);
    }

    void
    decode(cryptoconditions::der::Decoder& decoder) override
    {
        serialize(decoder);
    }

    void
    print(std::ostream& ostr) const override
    {
        ostr << "{d4;\n";
        ostr << '{';
        for (auto const& c : subChoices_)
            c->print(ostr);
        ostr << "}\n}\n";
    }

    friend bool
    operator==(DerChoiceDerived4 const& lhs, DerChoiceDerived4 const& rhs)
    {
        if (lhs.subChoices_.size() != rhs.subChoices_.size())
            return false;

        for (size_t i = 0; i < lhs.subChoices_.size(); ++i)
            if (!equal(lhs.subChoices_[i], rhs.subChoices_[i]))
                return false;
        return true;
    }

    friend bool
    operator!=(DerChoiceDerived4 const& lhs, DerChoiceDerived4 const& rhs)
    {
        return !(lhs == rhs);
    }
};

struct DerChoiceDerived5 : DerChoiceBaseClass
{
    std::unique_ptr<DerChoiceBaseClass> subChoice_;
    std::string name_;
    std::uint64_t id_;

    DerChoiceDerived5() = default;

    DerChoiceDerived5(
        std::unique_ptr<DerChoiceBaseClass> sub,
        std::string const& n,
        std::uint64_t i)
        : subChoice_(std::move(sub)), name_{n}, id_{i}
    {
    }

    std::uint8_t
    type() const override
    {
        return 5;
    }

    template <class Coder>
    void
    serialize(Coder& c)
    {
        c& std::tie(subChoice_, name_, id_);
    }

    void
    encode(cryptoconditions::der::Encoder& encoder) const override
    {
        const_cast<DerChoiceDerived5*>(this)->serialize(encoder);
    }

    void
    decode(cryptoconditions::der::Decoder& decoder) override
    {
        serialize(decoder);
    }

    void
    print(std::ostream& ostr) const override
    {
        ostr << "{d5;\n" << name_ << ";\n" << id_ << ";";
        ostr << '{';
        if (subChoice_)
            subChoice_->print(ostr);
        ostr << "}\n}\n";
    }

    friend bool
    operator==(DerChoiceDerived5 const& lhs, DerChoiceDerived5 const& rhs)
    {
        return lhs.name_ == rhs.name_ && lhs.id_ == rhs.id_ &&
            lhs.subChoice_ == rhs.subChoice_;
    }

    friend bool
    operator!=(DerChoiceDerived5 const& lhs, DerChoiceDerived5 const& rhs)
    {
        return !(lhs == rhs);
    }
};

inline
bool
equal(DerChoiceBaseClass const* lhs, DerChoiceBaseClass const* rhs)
{
    if (bool(lhs) != bool(rhs))
        return false;
    if (!lhs && !rhs)
        return true;
    if (auto plhs = dynamic_cast<DerChoiceDerived1 const*>(lhs))
    {
        if (auto prhs = dynamic_cast<DerChoiceDerived1 const*>(rhs))
        {
            return *plhs == *prhs;
        }
        return false;
    }
    if (auto plhs = dynamic_cast<DerChoiceDerived2 const*>(lhs))
    {
        if (auto prhs = dynamic_cast<DerChoiceDerived2 const*>(rhs))
        {
            return *plhs == *prhs;
        }
        return false;
    }
    if (auto plhs = dynamic_cast<DerChoiceDerived3 const*>(lhs))
    {
        if (auto prhs = dynamic_cast<DerChoiceDerived3 const*>(rhs))
        {
            return *plhs == *prhs;
        }
        return false;
    }
    if (auto plhs = dynamic_cast<DerChoiceDerived4 const*>(lhs))
    {
        if (auto prhs = dynamic_cast<DerChoiceDerived4 const*>(rhs))
        {
            return *plhs == *prhs;
        }
        return false;
    }
    if (auto plhs = dynamic_cast<DerChoiceDerived5 const*>(lhs))
    {
        if (auto prhs = dynamic_cast<DerChoiceDerived5 const*>(rhs))
        {
            return *plhs == *prhs;
        }
        return false;
    }
    return false;
}

}  // test

namespace cryptoconditions {
namespace der {
template <>
struct DerCoderTraits<std::unique_ptr<test::DerChoiceBaseClass>>
{
    constexpr static GroupType
    groupType()
    {
        return GroupType::choice;
    }
    constexpr static ClassId
    classId()
    {
        return ClassId::contextSpecific;
    }
    static boost::optional<std::uint8_t> const&
    tagNum()
    {
        static boost::optional<std::uint8_t> tn;
        return tn;
    }
    static std::uint8_t
    tagNum(std::unique_ptr<test::DerChoiceBaseClass> const& f)
    {
        assert(f);
        return f->type();
    }
    constexpr static bool
    primitive()
    {
        return false;
    }

    static void
    encode(Encoder& encoder, std::unique_ptr<test::DerChoiceBaseClass> const& b)
    {
        b->encode(encoder);
    }

    static void
    decode(Decoder& decoder, std::unique_ptr<test::DerChoiceBaseClass>& v)
    {
        auto const parentTag = decoder.parentTag();
        if (!parentTag)
        {
            decoder.ec_ = make_error_code(Error::logicError);
            return;
        }

        if (parentTag->classId != classId())
        {
            decoder.ec_ = make_error_code(Error::preambleMismatch);
            return;
        }

        switch (parentTag->tagNum)
        {
            case 1:
                v = std::make_unique<test::DerChoiceDerived1>();
                break;
            case 2:
                v = std::make_unique<test::DerChoiceDerived2>();
                break;
            case 3:
                v = std::make_unique<test::DerChoiceDerived3>();
                break;
            case 4:
                v = std::make_unique<test::DerChoiceDerived4>();
                break;
            case 5:
                v = std::make_unique<test::DerChoiceDerived5>();
                break;
            default:
                decoder.ec_ = make_error_code(der::Error::unknownChoiceTag);
        }

        if (v)
            v->decode(decoder);

        if (decoder.ec_)
            v.reset();
    }
};
}  // der
}  // cryptoconditions
}  // ripple

#endif
