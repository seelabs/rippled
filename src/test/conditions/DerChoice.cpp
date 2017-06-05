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

#include <test/conditions/DerChoice.h>

namespace ripple {

namespace test {

template<class TChoice>
static
void
encodeTupHelper(TChoice const& c, cryptoconditions::der::Encoder& encoder)
{
    c.withTuple([&encoder](auto const& tup){
        encoder << tup;
    });
}

template<class TChoice>
static
void
decodeTupHelper(TChoice& c, cryptoconditions::der::Decoder& decoder)
{
    c.withTuple([&decoder](auto&& tup){
        decoder >> tup;
    });
}

template<class TChoice>
static
std::uint64_t
derEncodedLengthHelper(TChoice const& c)
{
    std::uint64_t result = 0;
    c.withTuple([&result](auto const& tup) {
        using T = std::decay_t<decltype(tup)>;
        using Traits = cryptoconditions::der::DerCoderTraits<T>;
        result = Traits::length(tup);
    });
    return result;
}

DerChoiceDerived1::DerChoiceDerived1(
    std::vector<char> const& b,
    std::vector<std::unique_ptr<DerChoiceBaseClass>> sub,
    std::int32_t si)
    : buf_(makeSlice(b)), subChoices_(std::move(sub)), signedInt_(si)
{
}

std::uint8_t
DerChoiceDerived1::type() const
{
    return 1;
}

template<class F>
void
DerChoiceDerived1::withTuple(F&& f)
{
    auto subAsSeq = cryptoconditions::der::make_sequence(subChoices_);
    f(std::tie(buf_, subAsSeq, signedInt_));
}

std::uint64_t
DerChoiceDerived1::derEncodedLength() const
{
    return derEncodedLengthHelper(*this);
}

void
DerChoiceDerived1::encode(cryptoconditions::der::Encoder& encoder) const
{
    encodeTupHelper(*this, encoder);
}

void
DerChoiceDerived1::decode(cryptoconditions::der::Decoder& decoder)
{
    decodeTupHelper(*this, decoder);
}

void
DerChoiceDerived1::print(std::ostream& ostr) const
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

bool
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

bool
operator!=(DerChoiceDerived1 const& lhs, DerChoiceDerived1 const& rhs)
{
    return !(lhs == rhs);
}

DerChoiceDerived2::DerChoiceDerived2(std::string const& n, std::uint64_t i) : name_(n), id_(i)
{
}

std::uint8_t
DerChoiceDerived2::type() const
{
    return 2;
}

template<class F>
void
DerChoiceDerived2::withTuple(F&& f)
{
    f(std::tie(name_, id_));
}

std::uint64_t
DerChoiceDerived2::derEncodedLength() const
{
    return derEncodedLengthHelper(*this);
}

void
DerChoiceDerived2::encode(cryptoconditions::der::Encoder& encoder) const
{
    encodeTupHelper(*this, encoder);
}

void
DerChoiceDerived2::decode(cryptoconditions::der::Decoder& decoder)
{
    decodeTupHelper(*this, decoder);
}

void
DerChoiceDerived2::print(std::ostream& ostr) const
{
    ostr << "{d2;\n" << name_ << ";\n" << id_ << ";}\n";
}

bool
operator==(DerChoiceDerived2 const& lhs, DerChoiceDerived2 const& rhs)
{
    return lhs.name_ == rhs.name_ && lhs.id_ == rhs.id_;
}

bool
operator!=(DerChoiceDerived2 const& lhs, DerChoiceDerived2 const& rhs)
{
    return !(lhs == rhs);
}

DerChoiceDerived3::DerChoiceDerived3(std::vector<std::unique_ptr<DerChoiceBaseClass>> sub)
    : subChoices_(std::move(sub))
{
}

std::uint8_t
DerChoiceDerived3::type() const
{
    return 3;
}

template<class F>
void
DerChoiceDerived3::withTuple(F&& f)
{
    auto subAsSet = cryptoconditions::der::make_set(subChoices_);
    f(std::tie(subAsSet));
}

std::uint64_t
DerChoiceDerived3::derEncodedLength() const
{
    return derEncodedLengthHelper(*this);
}

void
DerChoiceDerived3::encode(cryptoconditions::der::Encoder& encoder) const
{
    encodeTupHelper(*this, encoder);
}

void
DerChoiceDerived3::decode(cryptoconditions::der::Decoder& decoder)
{
    decodeTupHelper(*this, decoder);
}

void
DerChoiceDerived3::print(std::ostream& ostr) const
{
    ostr << "{d3;\n";
    ostr << '{';
    for (auto const& c : subChoices_)
        c->print(ostr);
    ostr << "}\n}\n";
}

bool
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

bool
operator!=(DerChoiceDerived3 const& lhs, DerChoiceDerived3 const& rhs)
{
    return !(lhs == rhs);
}

DerChoiceDerived4::DerChoiceDerived4(std::vector<std::unique_ptr<DerChoiceBaseClass>> sub)
    : subChoices_(std::move(sub))
{
}

std::uint8_t
DerChoiceDerived4::type() const
{
    return 4;
}

template<class F>
void
DerChoiceDerived4::withTuple(F&& f)
{
    auto subAsSeq = cryptoconditions::der::make_sequence(subChoices_);
    f(std::tie(subAsSeq));
}

std::uint64_t
DerChoiceDerived4::derEncodedLength() const
{
    return derEncodedLengthHelper(*this);
}

void
DerChoiceDerived4::encode(cryptoconditions::der::Encoder& encoder) const
{
    encodeTupHelper(*this, encoder);
}

void
DerChoiceDerived4::decode(cryptoconditions::der::Decoder& decoder)
{
    decodeTupHelper(*this, decoder);
}

void
DerChoiceDerived4::print(std::ostream& ostr) const
{
    ostr << "{d4;\n";
    ostr << '{';
    for (auto const& c : subChoices_)
        c->print(ostr);
    ostr << "}\n}\n";
}

bool
operator==(DerChoiceDerived4 const& lhs, DerChoiceDerived4 const& rhs)
{
    if (lhs.subChoices_.size() != rhs.subChoices_.size())
        return false;

    for (size_t i = 0; i < lhs.subChoices_.size(); ++i)
        if (!equal(lhs.subChoices_[i], rhs.subChoices_[i]))
            return false;
    return true;
}

bool
operator!=(DerChoiceDerived4 const& lhs, DerChoiceDerived4 const& rhs)
{
    return !(lhs == rhs);
}

DerChoiceDerived5::DerChoiceDerived5(
    std::unique_ptr<DerChoiceBaseClass> sub,
    std::string const& n,
    std::uint64_t i)
    : subChoice_(std::move(sub)), name_{n}, id_{i}
{
}

std::uint8_t
DerChoiceDerived5::type() const
{
    return 5;
}

template<class F>
void
DerChoiceDerived5::withTuple(F&& f)
{
    f(std::tie(subChoice_, name_, id_));
}

std::uint64_t
DerChoiceDerived5::derEncodedLength() const
{
    return derEncodedLengthHelper(*this);
}

void
DerChoiceDerived5::encode(cryptoconditions::der::Encoder& encoder) const
{
    encodeTupHelper(*this, encoder);
}

void
DerChoiceDerived5::decode(cryptoconditions::der::Decoder& decoder)
{
    decodeTupHelper(*this, decoder);
}

void
DerChoiceDerived5::print(std::ostream& ostr) const
{
    ostr << "{d5;\n" << name_ << ";\n" << id_ << ";";
    ostr << '{';
    if (subChoice_)
        subChoice_->print(ostr);
    ostr << "}\n}\n";
}

bool
operator==(DerChoiceDerived5 const& lhs, DerChoiceDerived5 const& rhs)
{
    return lhs.name_ == rhs.name_ && lhs.id_ == rhs.id_ &&
        lhs.subChoice_ == rhs.subChoice_;
}

bool
operator!=(DerChoiceDerived5 const& lhs, DerChoiceDerived5 const& rhs)
{
    return !(lhs == rhs);
}

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
}  // ripple
