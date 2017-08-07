//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2017 Ripple Labs Inc.

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

#ifndef RIPPLE_CONDITIONS_DER_H
#define RIPPLE_CONDITIONS_DER_H

#include <ripple/basics/Buffer.h>
#include <ripple/basics/Slice.h>
#include <ripple/conditions/impl/error.h>

#include <algorithm>
#include <bitset>
#include <limits>
#include <numeric>
#include <stack>
#include <vector>

#include <boost/container/flat_map.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/optional.hpp>

namespace ripple {
namespace cryptoconditions {

/**
  The `der` namespace contains a set of classes the implement ASN.1 der encoding
  and decoding for cryptoconditions.

  There are two keys to understanding how to use these coders: 1)
  DerCoderTraits<T>, and 2) `withTuple`.

  To encode or decode a type `T`, a specialization of DerCoderTraits<T> must
  exist. This specialization contains all the functions specific to streaming
  type `T`. The most important are: `encode`, `decode`, `length`, and `compare`.
  @see {@link #DerCoderTraits}.

  If a class defines the function `withTuple`, that class can use the helper
  functions `withTupleEncodeHelper`, `withTupleDecodeHelper`,
  `withTupleEncodedLengthHelper`, and `withTupleCompareHelper`. The `withTuple`
  function takes a callable, and the class will wrap its member variables in a
  tuple of references (usually using `tie`), and call the callable with the
  tuple as an argument. See one of the existing cryptocondtion implementations
  for an example of how `withTuple` is used.

  @note Efficiently encoding cryptocondition classes into ASN.1 has some challenges:

  1) The size of the preamble depends on the size of content being encoded. This
  makes it difficult to encode in a single pass. The most natural implementation
  would a) encode the content, b) encode the preamble, c) copy the result into
  some final buffer. The function `DerCoderTraits<T>::length` solves this
  problem. When encoding a value of type T, the length will return the number of
  bytes used to encode contents of the value (but does not include the
  preamble).

  2) Encoding der sets requires the elements of the set be encoded in sorted
  order (sorted by the encoding of the individual elements). The function
  `DerCoderTraits<T>::compare` solves this problem. This function returns a
  values less than 0 if the lhs < rhs, 0 if lhs == rhs, and a value greater than
  0 if lhs > rhs.

  3) When encoding cryptoconditions that contain other cryptoconditions in
  hierarchies (such as threshold and prefix), some values - like length and sort
  order - will be computed multiple times, and the number of times a value is
  computed grows as a function of the cryptocondition's depth in the hierarchy.
  This is solved with a TraitCache. This caches previously computed values of
  length and sort order so they do not need to be recomputed. Note that storing
  values in the cache is type dependent, and the address of the variable must be
  stable while encoding. It makes sense to cache higher level values, but not
  primitives.
 */

namespace der {

enum TagType {
    tagBoolean = 1,
    tagInteger = 2,
    tagBitString = 3,
    tagOctetString = 4,
    tagNull = 5,
    tagObjectIdentifier = 6,
    tagReal = 9,
    tagEnumerated = 10,
    tagUtf8String = 12,
    tagSequence = 16,
    tagSet = 17
};

/**  Type of the group.

     @note Sometimes this matches the asn.1 tag number, but not always. In
     particular, a coder in "auto" mode may use different tags, and some of
     these types (`autoSequence`, `sequenceChild`, `choice`, and `fuzzRoot`)
     will never match the tag type. However, the coders need to know the
     additional information, such as when a parent group is a sequence, or an
     autoSequence.
*/

enum class GroupType {
    boolean = tagBoolean,
    integer = tagInteger,
    bitString = tagBitString,
    octetString = tagOctetString,
    null = tagNull,
    objectIdentifier = tagObjectIdentifier,
    real = tagReal,
    enumerated = tagEnumerated,
    utf8String = tagUtf8String,
    sequence = tagSequence,
    set = tagSet,

    // The following are never tag ids.

    // a sequence that has auto generated tag numbers
    autoSequence = 252,
    // a child in an autogenerated sequence. This is useful as the parent when
    // the child is a "choice"
    sequenceChild = 253,
    choice = 254,
    // used in fuzz testing only
    fuzzRoot = 255
};

/** asn.1 class ids
 */
enum class ClassId { universal, application, contextSpecific, priv };

/** The coder's tag mode.
 */
enum class TagMode {
    /** direct corresponds to asn.1's `explicit`. Tags will not be automatically assigned.

        @note `explicit` is a c++ keyword, so an alternate name must be used.
    */
    direct,
    /// Tags will be automatically assigned
    automatic
};

struct Encoder;
struct Decoder;

/** cache for values that need to be repeatedly computed, and may be expensive to compute.

    @note the cache assume address of the object will not change. This is _not_
          true of some of the wrapper types (SetOfWrapper and
          SequenceOfWrapper), however it should be true of the collection being
          wrapped. In these cases, the address of the collection should be used
          as the key. It is not necessary to cache all types. Primitive types do
          not need to be cached, as their parents will be cached.
 */
class TraitsCache
{
    /** Collection of content lengths for the value at the given address */
    boost::container::flat_map<void const*, std::size_t> lengthCache_;

    /** Collection of sort orders (for der sets) for the value at the give address */
    boost::container::flat_map<void const*, boost::container::small_vector<size_t, 8>>
        sortOrderCache_;

public:
    /** Get the cached content length for the value at the given address

        @result If the value is stored in the cache, an optional set to the
        stored value, otherwise boost::none
     */
    boost::optional<std::size_t>
    length(void const* addr) const;

    /** Set the cached content length for the value at the given address
     */
    void
    length(void const* addr, size_t l);

    /** Get the cached content sort order for the value at the given address

        @result If the value is stored in the cache, an optional set to the
        stored value, otherwise boost::none

        @note sort order is only used for der sets
     */
    boost::optional<boost::container::small_vector<size_t, 8>>
    sortOrder(void const* addr) const;

    /** Set the cached content sort order for the value at the given address

        @note sort order is only used for der sets
     */
    void
    sortOrder(
        void const* addr,
        boost::container::small_vector<size_t, 8> const& so);
};

/** Interface for serializing and deserializing types into a der coder.

   Types that serialized into a der coder need to specialize a `DerCoderTraits`
   class and provide implementations of each of the member functions described
   in this unspecialized class.

   Specialized classes are provided for common C++ types, such as integers,
   strings, buffers, bitsets, etc.

   Since there are two types of collections in ans.1 - sets and sequences - some
   c++ collections like `std::vector` must be wrapped in the helper functions
   `make_sequence` or `make_set` so the coder knows which asn.1 collection type
   to use. `std::tuple` is always coded as an asn.1 sequence, since 100% of the
   cryptocondition use-cases serialized tuples as sequences.

   A typical user type is written as an asn.1 choice type, where each choice
   type is a asn.1 sequence. This would be implemented in C++ as a class
   hierarchy. Each concrete leaf class represents a choice. The trait is written
   in terms of a `unique_ptr` to the base class. When decoding, the choice is
   used to create the correct concrete leaf class, and the leaf class would
   serialize itself by using a `std::tuple` of each member of the sequence. For
   example `der_decoder >> std::tuple(member1, member2,...) >> der::eos;`
   Encoding is similar: the correct choice is written to the der stream, then
   the data members are serialized using a `std::tuple`. For example:
   `der_encoder << std::tuple(member1, member2,...) << der::eos;`. Since
   encoding and decoding are almost always exact copies, except decoding uses
   `operator>>` while encoding uses `operator<<`, the der encoders offer
   `operator&` which will decode when using a decoder, and encode with using an
   encoder (boost serialization library uses the same operator). Using this
   operator, both the encoder and decoder may be written as `coder &
   std::tuple(member1, member2,...) & der::eos;`.


   @note This unspecialized class should never be instantiated. If it is, a
         `static_assert` will fire. A specialized `DerCoderTraits` must be provided
         for each class that is to be serialized.
 */
template <class T>
struct DerCoderTraits
{
    static_assert(
        sizeof(T) == -1,
        "DerCoderTraits must be specialized for this type");

    /// ans.1 class id
    constexpr static
    ClassId
    classId();

    /// group type
    constexpr static
    GroupType
    groupType();

    /** ans.1 tag type, if known.

        The tagNum for choice types can only be known from the actual value being
        encoded. In these cases `boost::none` is returned.
    */ 
    static
    boost::optional<std::uint8_t> const&
    tagNum();

    /// ans.1 tag type for this given value.
    template <class TT>
    static
    std::uint8_t
    tagNum(TT);

    /** return true if this type is an asn.1 primitive. return false if this
        type is an asn.1 constructed type.
     */
    constexpr static
    bool
    primitive();

    /** return the number of bytes required to encode the value, not including
        the preamble

        @param v the value to find the length of
        @param parentGroupType type of the parent group.
        @param encoderTagMode tag mode of the encoder.
        @param traitsCache cache some values that need to be repeatably computed,
               and may be expensive to compute. Some value types will cache lengths and
               sort orders, other values types will not cache any values.
        @param childNumber if this value is stored in a set or a sequence, the child number of
               this value. This is needed to compute the tag size.

        @note Choice parents groups in automatic tag mode are treated specially.
     */
    template <class TT>
    static
    std::uint64_t
    length(
        TT const& v,
        boost::optional<GroupType> const& parentGroupType,
        TagMode encoderTagMode,
        TraitsCache& traitsCache,
        boost::optional<std::uint64_t> const& childNumber=boost::none);

    /// serialize the value into the encoder
    template <class TT>
    static
    void
    encode(Encoder& s, TT v);

    /// deserialize the value from the decoder
    template <class TT>
    static
    void
    decode(Decoder& decoder, TT& v);

    /** compare two values so they sort appropriately for an asn.1 set. Returns a
       value less than 0 if lhs<rhs, 0 if lsh==rhs, a value greater than zero if
       lhs>rhs

        @param lhs first value to compare
        @param rhs second value to compare
        @param traitsCache cache some values that need to be repeatedly computed,
               and may be expensive to compute. Some value types will cache lengths and
               sort orders, other values types will not cache any values.

        @note asn.1 lexagraphically compares how the values would be encoded.
              asn.1 encodes in big endian order.
     */
    static 
    int
    compare(T const& lhs, T const& rhs, TraitsCache& traitsCache);
};

/// constructor tag to specify an asn.1 sequence
struct SequenceTag {};
/// constructor tag to specify an asn.1 set
struct SetTag {};

/// the type information part of an asn.1 preamble
struct Tag
{
    ClassId classId = ClassId::universal;
    std::uint64_t tagNum = 0;
    bool primitive = true;

    Tag() = default;

    Tag(ClassId classId_, std::uint64_t tagNum_, bool primitive_)
        : classId(classId_), tagNum(tagNum_), primitive(primitive_)
    {
    }

    template <class T>
    Tag(DerCoderTraits<T> t, std::uint64_t tn)
        : Tag(DerCoderTraits<T>::classId(), tn, DerCoderTraits<T>::primitive())
    {
    }

    explicit
    Tag(SequenceTag);

    explicit
    Tag(SetTag);

    /// return true if the tag represents an asn.1 set
    bool
    isSet() const;

    friend bool
    operator<(Tag const& lhs, Tag const& rhs)
    {
        return std::tie(lhs.classId, lhs.tagNum, lhs.primitive) <
            std::tie(rhs.classId, rhs.tagNum, rhs.primitive);
    }
    friend bool
    operator==(Tag const& lhs, Tag const& rhs)
    {
        return lhs.classId == rhs.classId && lhs.tagNum == rhs.tagNum &&
            lhs.primitive == rhs.primitive;
    }
    friend bool
    operator!=(Tag const& lhs, Tag const& rhs)
    {
        return !operator==(lhs, rhs);
    }
};

/** an ans.1 preamble

    values are encoded in ans.1 with a preamble that specifies how to interpret
    the content, followed by the content. This struct represents the preamble.
*/
struct Preamble
{
    /// type information
    Tag tag_;
    /// content length in bytes
    std::uint64_t contentLength_;
};

/** RAII class for coder groups

    asn.1 values are coded as a hierarchy. There are root values, which have
    sub-values as children. A `GroupGuard` organizes the serialization code so
    C++ scopes represent levels in the asn.1 hierarchy. The constructor pushes a
    new group onto the coders group stack, and the destructor pops the group.
    Entering a scope represents a new value that will be coded. New values will
    be descendants of this value coded in this scope until the scope is exited.
 */
template <class Coder>
class GroupGuard
{
    /// The encoder or decoder
    Coder& s_;

public:
    GroupGuard(Coder& s, Tag t, GroupType bt)
        : s_(s)
    {
        s_.startGroup(t, bt);
    }

    GroupGuard(Coder& s, Tag t, GroupType bt, std::uint64_t contentSize)
        : s_(s)
    {
        s_.startGroup(t, bt, contentSize);
    }

    GroupGuard(Coder& s, boost::optional<Tag> const& t, GroupType bt)
        : s_(s)
    {
        s_.startGroup(t, bt);
    }

    GroupGuard(Coder& s, SequenceTag t)
        : GroupGuard(s, Tag{t}, GroupType::sequence)
    {
    }

    GroupGuard(Coder& s, SetTag t)
        : GroupGuard(s, Tag{t}, GroupType::set)
    {
    }

    template <class T>
    GroupGuard(Coder& s, DerCoderTraits<T> t)
        : s_(s)
    {
        boost::optional<Tag> tag;
        if (auto const tagNum = t.tagNum())
            tag.emplace(t, *tagNum);
        s_.startGroup(tag, t.groupType());
    }

    template <class T>
    GroupGuard(Coder& s, T const& v, DerCoderTraits<T> t)
        : s_(s)
    {
        auto const tagNum = t.tagNum(v);
        Tag tag(t, tagNum);
        s_.startGroup(tag, t.groupType());
    }

    // Needed for fuzz testing
    GroupGuard(Coder& s, GroupType bt)
        : s_(s)
    {
        s_.startGroup(boost::none, bt);
    }

    ~GroupGuard()
    {
        s_.endGroup();
    }
};

/** End of stream guard

    Coders need to know when when a serialization is complete. Clients signal
    this by calling `eos`. This guard calls `eos` in the destructor so leaving
    a scope may be used to signal `eos`.

    @note This class is mostly used for testing. The usual way to signal `eos`
    is by adding `der::eos` at the end of a stream. For example: `coder << value
    << der::eos;`
 */
template <class Coder>
class EosGuard
{
    // Encoder or decoder
    Coder& s_;

public:
    explicit
    EosGuard(Coder& s)
        : s_(s)
    {
    }

    ~EosGuard()
    {
        s_.eos();
    }
};

template<std::uint64_t ChunkBitSize>
std::uint64_t
numLeadingZeroChunks(std::uint64_t v, std::uint64_t n)
{
    static_assert(ChunkBitSize <= 8, "Unsupported chunk bit size");

    std::uint64_t result = 0;
    while (n--)
    {
        auto b = static_cast<std::uint8_t>((v >> (n * ChunkBitSize)) & 0xFF);
        if (b)
            break;
        ++result;
    }
    return result;
}

/** decode the tag from asn.1 format
 */
void
decodeTag(Slice& slice, Tag& tag, std::error_code& ec);

/** Encode the integer in a format appropriate for an ans.1 tag number.

    Encode the integer in big endian form, in as few of bytes as possible. All
    but the last byte has the high order bit set. The number is encoded in base
    128 (7-bits each).
*/
void
encodeTagNum(MutableSlice& dst, std::uint64_t v, std::error_code& ec);

/** Return the number of bytes required to encode a tag with the given tag num */
std::uint64_t
tagNumLength(std::uint64_t v);

/** Decode the content length from asn.1 format
*/
void
decodeContentLength(Slice& slice, std::uint64_t& contentLength, std::error_code& ec);

/** Encode the integer in a format appropriate for an ans.1 content length

    Encode the integer in big endian form, in as few of bytes as possible.
*/
void
encodeContentLength(MutableSlice& dst, std::uint64_t v, std::error_code& ec);

/** return the number of bytes required to encode the given content length
 */
std::uint64_t
contentLengthLength(std::uint64_t);

/** return the number of bytes required to encode the given tag
 */
std::uint64_t
tagLength(Tag t);

/** return the number of bytes required to encode the value, including the preamble
 */
template <class Trait, class T>
std::uint64_t
totalLength(
    T const& v,
    boost::optional<GroupType> const& parentGroupType,
    TagMode encoderTagMode,
    TraitsCache& traitsCache,
    boost::optional<std::uint64_t> const& childNumber)
{
    auto const contentLength =
        Trait::length(v, parentGroupType, encoderTagMode, traitsCache);
    if (encoderTagMode == TagMode::automatic &&
        parentGroupType && *parentGroupType == GroupType::choice)
        return contentLength;

    auto const oneTagResult = tagNumLength(childNumber.value_or(0)) +
        contentLength + contentLengthLength(contentLength);

    if (parentGroupType && *parentGroupType == GroupType::autoSequence &&
        DerCoderTraits<T>::groupType() == GroupType::choice)
    {
        // auto sequences with a choice write a two tags: one for the sequence
        // number and one for the choice
        // note: This breaks down if the choice number is large enough to
        // require more than one byte for the tag (more than 30 choices)
        return tagNumLength(0) + oneTagResult + contentLengthLength(oneTagResult);
    }

    // all cryptocondition preambles are one byte
    return oneTagResult;
}

/** A value in a hierarchy of values when encoding

    asn.1 values are coded as a hierarchy. There is one root value, which has
    sub-values as children. When encoding, this class keeps track the type that
    is being encoded, what bytes in the stream represent content for this value,
    and child values.

    @note decoders use a different class to represent the hierarchy of values.
 */
class Group
{
    /// asn.1 type information for the value being encoded
    Tag id_;
    /// current number of children
    size_t numChildren_ = 0;
    /// asn.1 explicit (direct) or automatic tagging
    TagMode tagMode_;
    /// additional type information for the group
    GroupType groupType_;

    /** data slice reserved for both the preamble and contents of the group

        @note it _must_ be the correct size. It will not be resized.
    */
    MutableSlice slice_;

public:
    Group(Group const&) = default;
    Group(Group&&) = default;
    Group&
    operator=(Group&&) = default;
    Group&
    operator=(Group const&) = default;

    Group(
        Tag t,
        TagMode tagMode,
        GroupType groupType,
        MutableSlice slice);

    /// the data slice reserved for both the preamble and contents of the group
    MutableSlice&
    slice();

    Slice
    slice() const;

    /** Increment the number of children this group has
     */
    void
    incrementNumChildren()
    {
        ++numChildren_;
    }

    /// return true if the group represents an asn.1 set
    bool
    isSet() const;

    /** return true if the group represents an auto sequence

        @note an auto sequence is an asn.1 sequence that has autogenerated
              tag numbers
     */
    bool
    isAutoSequence() const;

    /// return true if the group represents an asn.1 choice
    bool
    isChoice() const;

    /** set the groups type information

        @param primitive true is primitive, false if constructed
        @param bt the groups type information
     */
    void
    set(bool primitive, GroupType bt);

    /// return the number of sub-values
    size_t
    numChildren() const;

    GroupType groupType() const;
};

/** encode the preamble into the dst slice
 */
void
encodePreamble(MutableSlice& dst, Preamble const& p, std::error_code& ec);

/** decode the preamble from slice into p
 */
void
decodePreamble(Slice& slice, Preamble& p, std::error_code& ec);

/** type representing and end of stream

    Coders need to know when when a serialization is complete. Clients signal
    this by calling `eos`. The typical way of calling `eos` is by serializing a
    value of type Eos. There is a convenience global variable for this purpose.
    It will typically be used as follows: `coder << value << der::eos;`
*/
struct Eos {};
extern Eos eos;
/// constructor tag to specify a decoder in automatic mode
struct Automatic {};
extern Automatic automatic;
/** constructor tag to specify a type is being constructed for decoding into

    Often, it is convenient to create a type and then decode into that type.
    However, this would usually require that type to be default constructable
    (as the contents used to create are deserialized after the variable is
    constructed). This `Constructor` type is used to create constructors and
    specify that they should only be used for der decoding.
 */
struct Constructor {};
extern Constructor constructor;

/** Stream interface to encode values into asn.1 der format

    The encoder class has an interface similar to a c++ output stream. Values
    are added to the stream using the `<<` operator. After all the values are
    added to the encoder, it must be terminated with a call to `eos()`. As a
    convenience, there is a special variable called `eos` that when streamed will
    call the stream's `eos()` function. Typically, the code to encode values to a
    stream is: `encoder << value_1 << ... << value_n << eos;`.

    Every type to be streamed must specialize the DerCoderTraits class @see
    {@link #DerCoderTraits}. There exist specializations for some C++ types and
    primitive rippled types - including integers, strings, bitstrings, tuples,
    buffers, arrays, and wrappers for wrapping collections like vector into
    either asn.1 sets or sequences.

    After the values are written, the stream should be checked for errors. The
    function `ec` will return the error code of the first error encountered
    while streaming. Streaming will stop after the first error.

    Once the values are streamed, the actual encoding is retrieved by calling
    the `write` function.

    Encoding and decoding values often have the same code structure. The only
    difference is encoding will use `operator<<` and decoding will use
    `operator>>`. To allow writing generic code, both encoders and decoders
    support `operator&`. Typically, the generic code both encode and decode is:
    `coder & value_1 & ... & value_n & eos;`
 */
struct Encoder
{
    /// explicit or automatic tagging
    TagMode tagMode_ = TagMode::direct;

    /** values are coded as a hierarchy. `subgroups_` tracks the current
        position in the hierarchy.

        The bottom of the stack is the root value, the top of the stack is the
        current parent.
     */
    std::stack<Group> subgroups_;

    /** root of the tree of groups that were encoded

        @note This is not populated until after encoding is complete
    */
    boost::optional<Group> root_;

    /** Buffer to encode into */
    std::vector<char> rootBuf_;

    /** Slice to encode into

        @note rootBuf_ should contain the same information as rootSlice_, and
        `rootSlice_` may be removed in the future. It is kept as a debugging
        tool to make sure rootBuf_ is not resized after it is resized for the
        root group.
    */
    Slice rootSlice_;

    /** the error code of the first error encountered

        @note after the error code is set encoding stops
     */
    std::error_code ec_;

    /** true if the `eos` function has been called

        some error handling cannot happen until all the values have been coded.
        `atEos_` ensures every stream is terminated with an `eos` call so those
        error checks can be run.
     */
    bool atEos_ = false;

    /** traitsCache cache some values that need to be repeatedly computed and may be expensive to compute.

        Some value types will cache lengths and sort orders, other values types will not cache any values.
    */
    TraitsCache traitsCache_;

    explicit
    Encoder(TagMode tagMode);
    ~Encoder();

    /// prepare to add a new value as a child of the current value
    void
    startGroup(Tag t, GroupType groupType, std::uint64_t contentSize);

    /// finish adding the new value
    void
    endGroup();

    /** terminate the stream

        Streams must be terminated before the destructor is called. Certain error checks
        cannot occur until the encoder knows streaming is complete. Calling `eos()` runs these
        error checks. Failing to call `eos` before the destructor is an error.
     */
    void
    eos();

    /// total size in bytes of the content and all the preambles
    size_t
    size() const;

    /// return the portion of the buffer that represents the parent value
    MutableSlice&
    parentSlice();

    /** return the first error code encountered

        ec should be checked after streaming to ensure no errors occurred
     */
    std::error_code const&
    ec() const;

    /** get the serialization buffer that contains the values encoded as asn.1 der
     */
    std::vector<char> const&
    serializationBuffer(std::error_code& ec) const;

    /** return true if the group at the top of the stack represents an auto
        sequence

        @note an auto sequence is an asn.1 sequence that has autogenerated
              tag numbers
     */
    bool
    parentIsAutoSequence() const;

    /** return true if the group at the top of the stack represents an asn.1
        choice
     */ 
    bool
    parentIsChoice() const;

    /** Add values to the encoder
    @{
    */
    friend
    Encoder&
    operator&(Encoder& s, Eos e)
    {
        s.eos();
        return s;
    }

    template <class T>
    friend
    Encoder&
    operator&(Encoder& s, T const& v)
    {
        if (s.ec_)
            return s;

        using traits = DerCoderTraits<std::decay_t<T>>;
        auto const groupType = traits::groupType();

        auto contentSize = [&] {
            boost::optional<GroupType> parentGroupType;
            if (!s.subgroups_.empty())
                parentGroupType.emplace(s.subgroups_.top().groupType());
            return traits::length(v, parentGroupType, s.tagMode_, s.traitsCache_);
        };

        if (s.parentIsAutoSequence())
        {
            if (groupType == GroupType::choice)
            {
                Tag const tag1{ClassId::contextSpecific,
                               s.subgroups_.top().numChildren(),
                               traits::primitive()};
                Tag const tag2{traits{}, traits::tagNum(v)};
                auto const contentSize = traits::length(
                    v, GroupType::sequenceChild, s.tagMode_, s.traitsCache_);
                GroupGuard<Encoder> g1(s, tag1, GroupType::sequenceChild,
                    tagLength(tag2) + contentLengthLength(contentSize) + contentSize);
                if (s.ec_)
                    return s;
                GroupGuard<Encoder> g2(s, tag2, groupType, contentSize);
                if (s.ec_)
                    return s;
                traits::encode(s, v);
            }
            else
            {
                Tag const tag{ClassId::contextSpecific,
                              s.subgroups_.top().numChildren(),
                              traits::primitive()};
                GroupGuard<Encoder> g(s, tag, groupType, contentSize());
                if (s.ec_)
                    return s;
                traits::encode(s, v);
            }
        }
        else
        {
            Tag const tag{traits{}, traits::tagNum(v)};
            GroupGuard<Encoder> g(s, tag, groupType, contentSize());
            if (s.ec_)
                return s;
            traits::encode(s, v);
        }

        return s;
    }

    template <class T>
    friend Encoder&
    operator<<(Encoder& s, T const& t)
    {
        return s & t;
    }
    /** @} */
};

/** Stream interface to decode values from asn.1 der format

    The decode class has an interface similar to a c++ output stream. Values are
    decoded from the stream using the `>>` operator. After all the values are
    decoded, it must be terminated with a call to `eos()`. As a convenience, there
    is a special variable called `eos` that when streamed will call the stream's
    `eos()` function. Typically, the code to encode values to a stream is:
    `decoder >> value_1 >> ... >> value_n >> eos;`.

    Every type to be streamed must specialize the DerCoderTraits class @see
    {@link #DerCoderTraits}. There exist specializations for some C++ types and
    primitive rippled types - including integers, strings, bitstrings, tuples,
    buffers, arrays, and wrappers for wrapping collections like vector into
    either asn.1 sets or sequences.

    After the values are decoded, the stream should be checked for errors. The
    function `ec` will return the error code of the first error encountered
    while decoding. Decoding will stop after the first error.

    Encoding and decoding values often have the same code structure. The only
    difference is encoding will use `operator<<` and decoding will use
    `operator>>`. To allow writing generic code, both encoders and decoders
    support `operator&`. Typically, the generic code both encode and decode is:
    `coder & value_1 & ... & value_n & eos;`
 */
struct Decoder
{
    /** explicit or automatic tagging

        @note this must match the mode the values were encoded with
     */
    TagMode tagMode_;

    /** true if the `eos` function has been called

        some error handling cannot happen until all the values have been coded.
        `atEos_` ensures every stream is terminated with an `eos` call so those
        error checks can be run.
     */
    bool atEos_ = false;

    /** slice for the entire buffer to be decoded */
    Slice rootSlice_;

    /** values are coded as a hierarchy. `ancestors_` tracks the current
        position in the hierarchy.

        The bottom of the stack is the root value, the top of the stack is the
        current parent.

        The tuple contains the slice, ancestor tag, groupType, and number of children
     */
    std::stack<std::tuple<Slice, Tag, GroupType, std::uint32_t>> ancestors_;

    /** the error code of the first error encountered

        @note after the error code is set decoding stops
     */
    std::error_code ec_;

    Decoder() = delete;

    Decoder(Slice slice, TagMode tagMode);

    ~Decoder();

    /// prepare to decode a value as a child of the current value
    void
    startGroup(boost::optional<Tag> const& t, GroupType groupType);

    /** finish decoding the new value */
    void
    endGroup();

    /** terminate the stream

        Streams must be terminated before the destructor is called. Certain error checks
        cannot occur until the encoder knows streaming is complete. Calling `eos()` runs these
        error checks. Failing to call `eos` before the destructor is an error.
     */
    void
    eos();

    /** return the first error code encountered

        ec should be checked after streaming to ensure no errors occurred
     */
    std::error_code const&
    ec() const;

    /** return the tag at the top of the ancestors stack

        return boost::none if the stack is empty
     */
    boost::optional<Tag>
    parentTag() const;

    /** return true if the ancestor at the top of the stack represents an auto
        sequence

        @note an auto sequence is an asn.1 sequence that has autogenerated
              tag numbers
     */
    bool
    parentIsAutoSequence() const;

    /** return true if the ancestor at the top of the stack represents an asn.1
        choice
     */ 
    bool
    parentIsChoice() const;

    /** return the portion of the buffer that represents the parent value */
    Slice&
    parentSlice();

    /** Decode values from the encoder into variables

       @note The forwarding ref is used for some value types to support
       std::tie, SetWrapper, and SequenceWrapper (i.e. `s >> make_set(some_vec)`)

    @{
    */
    friend
    Decoder&
    operator&(Decoder& s, Eos e)
    {
        s.eos();
        return s;
    }

    friend
    Decoder&
    operator&(Decoder& s, Preamble& p)
    {
        if (s.ec_)
            return s;

        decodePreamble(s.parentSlice(), p, s.ec_);
        return s;
    }

    template <class T>
    friend
    Decoder&
    operator&(Decoder& s, T&& v)
    {
        if (s.ec_)
            return s;

        using traits = DerCoderTraits<std::decay_t<T>>;
        auto const groupType = traits::groupType();
        if (s.parentIsAutoSequence())
        {
            if (groupType == GroupType::choice)
            {
                auto& numChildren = std::get<std::uint32_t>(s.ancestors_.top());
                Tag const tag1{
                    ClassId::contextSpecific, numChildren++, traits::primitive()};
                GroupGuard<Decoder> g1(s, tag1, GroupType::sequenceChild);
                if (s.ec_)
                    return s;
                boost::optional<Tag> tag2;
                if (auto const tagNum = traits::tagNum())
                    tag2.emplace(traits{}, *tagNum);
                GroupGuard<Decoder> g2(s, tag2, groupType);
                if (s.ec_)
                    return s;
                traits::decode(s, v);
            }
            else
            {
                auto& numChildren = std::get<std::uint32_t>(s.ancestors_.top());
                Tag const tag{
                    ClassId::contextSpecific, numChildren++, traits::primitive()};
                GroupGuard<Decoder> g(s, tag, groupType);
                if (s.ec_)
                    return s;
                traits::decode(s, v);
            }
        }
        else
        {
            boost::optional<Tag> tag;
            if (auto const tagNum = traits::tagNum())
                tag.emplace(traits{}, *tagNum);
            GroupGuard<Decoder> g(s, tag, groupType);
            if (s.ec_)
                return s;
            traits::decode(s, v);
        }

        return s;
    }

    template <class T>
    friend Decoder&
    operator>>(Decoder& s, T&& t)
    {
        return s & std::forward<T>(t);
    }
    /** @} */
};

/** base class for DerCoderTraits for integer types

    @see {@link #DerCoderTraits}
*/
struct IntegerTraits
{
    constexpr static
    ClassId
    classId()
    {
        return ClassId::universal;
    }

    constexpr static
    GroupType
    groupType()
    {
        return GroupType::integer;
    }

    static
    boost::optional<std::uint8_t> const&
    tagNum()
    {
        static boost::optional<std::uint8_t> tn{tagInteger};
        return tn;
    }

    template <class T>
    static
    std::uint8_t
    tagNum(T)
    {
        return tagInteger;
    }

    constexpr static
    bool
    primitive()
    {
        return true;
    }

    template <class T>
    static
    std::uint64_t
    length(T const& v)
    {
        const auto isSigned = std::numeric_limits<T>::is_signed;
        if (!v || (isSigned && v == -1))
            return 1;

        std::uint64_t n = sizeof(v);
        signed char toSkip = (isSigned && v < 0) ? 0xff : 0;
        // skip leading 0xff for negative signed, otherwise skip leading zeros
        // when skipping 0xff, the next octet's high bit must be set
        // when skipping 0, the first octet's high bit must not be set
        while (n--)
        {
            auto const c = static_cast<signed char>((v >> (n * 8)) & 0xff);
            if (c == toSkip &&
                !(isSigned && v < 0 && n &&
                  (static_cast<signed char>((v >> ((n - 1) * 8)) & 0xff) >= 0)))
                continue;

            if (v > 0 && c < 0)
                return n + 2;
            else
                return n + 1;
        }
        assert(0);  // will never happen
        return 1;
    }

    template <class T>
    static
    std::uint64_t
    length(
        T const& v,
        boost::optional<GroupType> const& parentGroupType,
        TagMode encoderTagMode,
        TraitsCache& traitsCache)
    {
        return length(v);
    }

    template <class T>
    static
    void
    encode(Encoder& s, T v)
    {
        if (s.subgroups_.empty())
        {
            s.ec_ = make_error_code(Error::logicError);
            return;
        }

        auto& parentSlice = s.parentSlice();

        if (!v)
        {
            if (parentSlice.empty())
            {
                s.ec_ = make_error_code(Error::logicError);
                return;
            }
            parentSlice.push_back(0);
            return;
        }

        boost::optional<GroupType> parentGroupType;
        if (!s.subgroups_.empty())
            parentGroupType.emplace(s.subgroups_.top().groupType());
        std::size_t n = length(v, parentGroupType, s.tagMode_, s.traitsCache_);
        if (parentSlice.size() != n)
        {
            s.ec_ = make_error_code(Error::logicError);
            return;
        }
        while (n--)
        {
            if (n >= sizeof(T))
                parentSlice.push_back(static_cast<char>(0));
            else
                parentSlice.push_back(static_cast<char>((v >> (n * 8)) & 0xFF));
        }
    }

    template <class T>
    static
    void
    decode(Decoder& decoder, T& v)
    {
        auto& slice = decoder.parentSlice();
        std::error_code& ec = decoder.ec_;

        if (slice.empty())
        {
            // can never have zero sized integers
            ec = make_error_code(Error::contentLengthMismatch);
            return;
        }

        const bool isSigned = std::numeric_limits<T>::is_signed;
        // unsigned types may have a leading zero octet
        const size_t maxLength = isSigned ? sizeof(T) : sizeof(T) + 1;
        if (slice.size() > maxLength)
        {
            ec = make_error_code(Error::integerBounds);
            return;
        }

        if (!isSigned && (slice[0] & (1 << 7)))
        {
            // trying to decode a negative number into a positive value
            ec = make_error_code(Error::integerBounds);
            return;
        }

        if (!isSigned && slice.size() == sizeof(T) + 1 && slice[0])
        {
            // since integers are coded as two's complement, the first byte may
            // be zero for unsigned reps
            ec = make_error_code(Error::integerBounds);
            return;
        }

        v = 0;
        for (size_t i = 0; i < slice.size(); ++i)
            v = (v << 8) | (slice[i] & 0xff);

        if (isSigned && (slice[0] & (1 << 7)))
        {
            for (int i = slice.size(); i < sizeof(T); ++i)
                v |= (T(0xff) << (8 * i));
        }
        slice += slice.size();
    }

    template <class T>
    static
    int
    compare(T const& lhs, T const& rhs, TraitsCache& traitsCache)
    {
        if (lhs >= 0 && rhs >= 0)
        {
            // fast common case
            // since the length is encoded, comparing the values directly will be
            // the same as comparing the encoded values
            return (lhs > rhs) - (lhs < rhs);
        }
        auto const lhsL = length(lhs);
        auto const rhsL = length(rhs);
        if (lhsL != rhsL)
        {
            if (lhsL < rhsL)
                return -1;
            return 1;
        }

        // lengths are equal
        auto n = std::min<T>(lhsL, sizeof(T) - 1);
        while (n--)
        {
            auto const lhsV = (static_cast<unsigned char>((lhs >> (n * 8)) & 0xFF));
            auto const rhsV = (static_cast<unsigned char>((rhs >> (n * 8)) & 0xFF));
            if (lhsV != rhsV)
            {
                if (lhsV < rhsV)
                    return -1;
                return 1;
            }
        }

        return 0;
    }
};

template <>
struct DerCoderTraits<std::uint8_t> : IntegerTraits
{
};
template <>
struct DerCoderTraits<std::uint16_t> : IntegerTraits
{
};
template <>
struct DerCoderTraits<std::uint32_t> : IntegerTraits
{
};
template <>
struct DerCoderTraits<std::uint64_t> : IntegerTraits
{
};
template <>
struct DerCoderTraits<std::int8_t> : IntegerTraits
{
};
template <>
struct DerCoderTraits<std::int16_t> : IntegerTraits
{
};
template <>
struct DerCoderTraits<std::int32_t> : IntegerTraits
{
};
template <>
struct DerCoderTraits<std::int64_t> : IntegerTraits
{
};

/** base class for DerCoderTraits for types that will be coded as asn.1 octet
    strings

    @see {@link #DerCoderTraits}

    @note this includes std::string and std::array<std::uintt_t, N>, Buffer, and
    boost small_vector<std::uint8_t, ...>
*/
struct OctetStringTraits
{
    constexpr static
    ClassId
    classId()
    {
        return ClassId::universal;
    }

    constexpr static
    GroupType
    groupType()
    {
        return GroupType::octetString;
    }

    static
    boost::optional<std::uint8_t> const&
    tagNum()
    {
        static boost::optional<std::uint8_t> tn{tagOctetString};
        return tn;
    }

    template <class T>
    static
    std::uint8_t
    tagNum(T const&)
    {
        return tagOctetString;
    }

    constexpr static
    bool
    primitive()
    {
        return true;
    }

protected:
    static
    void
    encode(Encoder& encoder, Slice s)
    {
        if (s.empty())
            return;

        auto& parentSlice = encoder.parentSlice();
        if (parentSlice.size() != s.size())
        {
            encoder.ec_ = make_error_code(Error::logicError);
            return;
        }
        memcpy(parentSlice.data(), s.data(), s.size());
        parentSlice += s.size();
    }

    static
    void
    decode(Decoder& decoder, void* dstData, std::size_t dstSize)
    {
        auto& slice = decoder.parentSlice();
        std::error_code& ec = decoder.ec_;

        if (dstSize != slice.size())
        {
            ec = make_error_code(Error::contentLengthMismatch);
            return;
        }

        if (!slice.empty())
            memcpy(dstData, slice.data(), slice.size());

        slice += slice.size();
    }
};

template <>
struct DerCoderTraits<std::string> : OctetStringTraits
{
    static
    void
    encode(Encoder& encoder, std::string const& s)
    {
        OctetStringTraits::encode(encoder, makeSlice(s));
    }

    static
    void
    decode(Decoder& decoder, std::string& v)
    {
        auto& slice = decoder.parentSlice();
        v.resize(slice.size());
        if (!v.empty())
            OctetStringTraits::decode(decoder, &v[0], v.size());
    }

    static
    std::uint64_t
    length(
        std::string const& v,
        boost::optional<GroupType> const& parentGroupType,
        TagMode encoderTagMode,
        TraitsCache& traitsCache)
    {
        return v.size();
    }

    static
    int
    compare(
        std::string const& lhs,
        std::string const& rhs,
        TraitsCache& traitsCache)
    {
        auto const lhsL = lhs.size();
        auto const rhsL = rhs.size();
        if (lhsL != rhsL)
        {
            if (lhsL < rhsL)
                return -1;
            return 1;
        }
        return lhs.compare(rhs);
    }
};

template <std::uint64_t S>
struct DerCoderTraits<std::array<std::uint8_t, S>> : OctetStringTraits
{
    static
    void
    encode(Encoder& encoder, std::array<std::uint8_t, S> const& s)
    {
        OctetStringTraits::encode(encoder, makeSlice(s));
    }

    static
    void
    decode(Decoder& decoder, std::array<std::uint8_t, S>& v)
    {
        OctetStringTraits::decode(decoder, v.data(), v.size());
    }

    static
    std::uint64_t
    length(
        std::array<std::uint8_t, S> const& v,
        boost::optional<GroupType> const& parentGroupType,
        TagMode encoderTagMode,
        TraitsCache& traitsCache)
    {
        return S;
    }

    static
    int
    compare(
        std::array<std::uint8_t, S> const& lhs,
        std::array<std::uint8_t, S> const& rhs,
        TraitsCache& traitsCache)
    {
        for(size_t i=0; i<S; ++i)
        {
            if (lhs[i] != rhs[i])
            {
                if (lhs[i] < rhs[i])
                    return -1;
                return 1;
            }
        }
        return 0;
    }
};

template <std::size_t S>
struct DerCoderTraits<boost::container::small_vector<std::uint8_t, S>> : OctetStringTraits
{
    static void
    encode(
        Encoder& encoder,
        boost::container::small_vector<std::uint8_t, S> const& s)
    {
        OctetStringTraits::encode(encoder, makeSlice(s));
    }

    static
    void
    decode(Decoder& decoder, boost::container::small_vector<std::uint8_t, S>& v)
    {
        auto& slice = decoder.parentSlice();
        v.resize(slice.size());
        if (!v.empty())
            OctetStringTraits::decode(decoder, v.data(), v.size());
    }

    static
    std::uint64_t
    length(
        boost::container::small_vector<std::uint8_t, S> const& v,
        boost::optional<GroupType> const& parentGroupType,
        TagMode encoderTagMode,
        TraitsCache& traitsCache)
    {
        return v.size();
    }

    static
    int
    compare(
        boost::container::small_vector<std::uint8_t, S> const& lhs,
        boost::container::small_vector<std::uint8_t, S> const& rhs,
        TraitsCache& traitsCache)
    {
        if (lhs.size() != rhs.size())
        {
            if (lhs.size() < rhs.size())
                return -1;
            return 1;
        }
        auto const s = lhs.size();
        for (size_t i = 0; i < s; ++i)
        {
            if (lhs[i] != rhs[i])
            {
                if (lhs[i] < rhs[i])
                    return -1;
                return 1;
            }
        }
        return 0;
    }
};

template <>
struct DerCoderTraits<Buffer> : OctetStringTraits
{
    static
    void
    encode(Encoder& encoder, Buffer const& b)
    {
        OctetStringTraits::encode(encoder, b);
    }

    static
    void
    decode(Decoder& decoder, Buffer& v)
    {
        auto& slice = decoder.parentSlice();
        v.alloc(slice.size());
        if (!v.empty())
            OctetStringTraits::decode(decoder, v.data(), v.size());
    }

    static
    std::uint64_t
    length(
        Buffer const& v,
        boost::optional<GroupType> const& parentGroupType,
        TagMode encoderTagMode,
        TraitsCache& traitsCache)
    {
        return v.size();
    }

    static
    int
    compare(Buffer const& lhs, Buffer const& rhs, TraitsCache& traitsCache)
    {
        if (lhs.size() != rhs.size())
        {
            if (lhs.size() < rhs.size())
                return -1;
            return 1;
        }
        auto const s = lhs.size();
        auto const lhsD = lhs.data();
        auto const rhsD = rhs.data();
        for (size_t i = 0; i < s; ++i)
        {
            if (lhsD[i] != rhsD[i])
            {
                if (lhsD[i] < rhsD[i])
                    return -1;
                return 1;
            }
        }
        return 0;
    }
};

/** Wrapper for a size constrained der OctetString

    The size of the string must be equal to the specified constraint.
 */
template <class T>
struct OctetStringCheckEqualSize
{
    T& col_;
    std::size_t constraint_;
    OctetStringCheckEqualSize(T& col, std::size_t s) : col_{col}, constraint_{s}
    {
    }
};

/** Wrapper for a size constrained der OctetString

    The size of the string must be less than the specified constraint.
 */
template <class T>
struct OctetStringCheckLessSize
{
    T& col_;
    std::size_t constraint_;
    OctetStringCheckLessSize(T& col, std::size_t s) : col_{col}, constraint_{s}
    {
    }
};

/** convenience function to create an equal-size constrained octet string

    @note the template parameter T must be one of the types OctetStringTraits is
          specialized on. @see {@link #OctetStringTraits}
*/
template <class T>
OctetStringCheckEqualSize<T>
make_octet_string_check_equal(T& t, std::size_t s)
{
    return OctetStringCheckEqualSize<T>(t, s);
}

/** convenience function to create a "less size" constrained octet string

    @note the template parameter T must be one of the types OctetStringTraits is
          specialized on. @see {@link #OctetStringTraits}
*/
template <class T>
OctetStringCheckLessSize<T>
make_octet_string_check_less(T& t, std::size_t s)
{
    return OctetStringCheckLessSize<T>(t, s);
}

/** DerCoderTraits for types that will be coded as "equal size" constrained
    asn.1 octet strings

    @see {@link #DerCoderTraits}
    @see {@link #make_octet_string_check_equal}
*/
template <class T>
struct DerCoderTraits<OctetStringCheckEqualSize<T>> : OctetStringTraits
{
    static
    void
    encode(Encoder& encoder, OctetStringCheckEqualSize<T> const& v)
    {
        DerCoderTraits<T>::encode(encoder, v.col_);
    }

    static
    void
    decode(Decoder& decoder, OctetStringCheckEqualSize<T>& v)
    {
        auto& slice = decoder.parentSlice();
        if (slice.size() != v.constraint_)
        {
            decoder.ec_ = make_error_code(Error::contentLengthMismatch);
            return;
        }
        DerCoderTraits<T>::decode(decoder, v.col_);
    }

    static
    std::uint64_t
    length(
        OctetStringCheckEqualSize<T> const& v,
        boost::optional<GroupType> const& parentGroupType,
        TagMode encoderTagMode,
        TraitsCache& traitsCache)
    {
        return DerCoderTraits<T>::length(v.col_, parentGroupType, encoderTagMode, traitsCache);
    }

    static
    int
    compare(T const& lhs, T const& rhs, TraitsCache& traitsCache)
    {
        return DerCoderTraits<T>::compare(lhs, rhs, traitsCache);
    }
};

/** DerCoderTraits for types that will be coded as "less size" constrained asn.1
    octet strings

    @see {@link #DerCoderTraits}
*/
template <class T>
struct DerCoderTraits<OctetStringCheckLessSize<T>> : OctetStringTraits
{
    static
    void
    encode(Encoder& encoder, OctetStringCheckLessSize<T> const& v)
    {
        DerCoderTraits<T>::encode(encoder, v.col_);
    }

    static
    void
    decode(Decoder& decoder, OctetStringCheckLessSize<T>& v)
    {
        auto& slice = decoder.parentSlice();
        if (slice.size() > v.constraint_)
        {
            // Return unsupported rather than contentLengthMismatch
            // because this constraint is an implementation limit rather
            // than a parser constraint
            decoder.ec_ = make_error_code(Error::unsupported);
            return;
        }
        DerCoderTraits<T>::decode(decoder, v.col_);
    }

    static
    std::uint64_t
    length(
        OctetStringCheckLessSize<T> const& v,
        boost::optional<GroupType> const& parentGroupType,
        TagMode encoderTagMode,
        TraitsCache& traitsCache)
    {
        return DerCoderTraits<T>::length(
            v.col_, parentGroupType, encoderTagMode, traitsCache);
    }

    static
    int
    compare(T const& lhs, T const& rhs, TraitsCache& traitsCache)
    {
        return DerCoderTraits<T>::compare(lhs, rhs, traitsCache);
    }
};

/** DerCoderTraits for std::bitset

    bitsets will be coded as ans.1 bitStrings

    @see {@link #DerCoderTraits}
    @see {@link #make_octet_string_check_less}
*/
template <std::size_t S>
struct DerCoderTraits<std::bitset<S>>
{
    constexpr static std::uint8_t mod8 = S % 8;
    constexpr static std::uint8_t const minUnusedBits = mod8 ? 8 - mod8 : 0;
    constexpr static std::uint8_t const maxBytes = mod8 ? 1 + S / 8 : S / 8;

    constexpr static
    GroupType
    groupType()
    {
        return GroupType::bitString;
    }

    constexpr static
    ClassId
    classId()
    {
        return ClassId::universal;
    }
    
    static
    boost::optional<std::uint8_t> const&
    tagNum()
    {
        static boost::optional<std::uint8_t> tn{tagBitString};
        return tn;
    }

    static
    std::uint8_t
    tagNum(std::bitset<S> const&)
    {
        return tagBitString;
    }

    constexpr static
    bool
    primitive()
    {
        return true;
    }

    static
    std::uint8_t
    reverseBits(std::uint8_t b)
    {
        static constexpr std::uint8_t lut[256] = 
        {
          0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0, 
          0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 
          0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4, 
          0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC, 
          0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 
          0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
          0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6, 
          0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
          0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
          0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9, 
          0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
          0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
          0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3, 
          0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
          0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7, 
          0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
        };
        return lut[b];
    }

    /** return the number of leading zero bytes before the last byte

        @note If no bits are set on a 64-bit integer, this function returns 7
        _not_ 8 because der will always consider the last byte, even if it is
        zero.
     */
    static
    std::uint64_t
    numLeadingZeroBytes(std::bitset<S> const& s)
    {
        auto const result = numLeadingZeroChunks<8>(s.to_ulong(), maxBytes);
        // Always consider the last byte, even if it is zero
        return std::min<std::uint64_t>(result, maxBytes - 1);
    }

    static
    std::uint8_t
    numUnusedBits(
        std::bitset<S> const& s,
        std::size_t leadingZeroBytes)
    {
        // b is first non-zero byte
        auto const bits = s.to_ulong ();
        std::uint8_t const b =
            (bits >> (maxBytes - leadingZeroBytes - 1) * 8) & 0xff;
        if (b & 0x80)
            return 0;
        if (b & 0x40)
            return 1;
        if (b & 0x20)
            return 2;
        if (b & 0x10)
            return 3;
        if (b & 0x08)
            return 4;
        if (b & 0x04)
            return 5;
        if (b & 0x02)
            return 6;
        if (b & 0x01)
            return 7;
        // der always considers the last bit, even if no bits are set
        return 7;
    }

    static
    void
    encode(Encoder& encoder, std::bitset<S> const& s)
    {
        static_assert(
            maxBytes > 0 && maxBytes <= sizeof(unsigned long),
            "Unsupported bitset size");

        auto& parentSlice = encoder.parentSlice();
        auto const bits = s.to_ulong();

        if (bits == 0)
        {
            if (parentSlice.size() != 2)
            {
                encoder.ec_ = make_error_code(Error::logicError);
                return;
            }
            parentSlice.push_back(7);
            parentSlice.push_back(0);
            return;
        }

        std::size_t const leadingZeroBytes = numLeadingZeroBytes(s);
        std::uint8_t const unusedBits = numUnusedBits(s, leadingZeroBytes);

        if (parentSlice.size() != 1 + maxBytes - leadingZeroBytes)
        {
            encoder.ec_ = make_error_code(Error::logicError);
            return;
        }

        parentSlice.push_back(unusedBits);

        for (size_t curByte = 0; curByte < maxBytes - leadingZeroBytes; ++curByte)
        {
            uint8_t const v = (bits >> curByte * 8) & 0xff;
            parentSlice.push_back(reverseBits(v));
        }
    }

    static
    void
    decode(Decoder& decoder, std::bitset<S>& v)
    {
        static_assert(
            maxBytes > 0 && maxBytes <= sizeof(unsigned long),
            "Unsupported bitset size");

        auto& slice = decoder.parentSlice();
        std::error_code& ec = decoder.ec_;

        if (slice.empty() || slice.size() > maxBytes + 1)
        {
            ec = make_error_code(Error::contentLengthMismatch);
            return;
        }

        auto const unused = slice[0];
        slice += 1;

        if (unused < minUnusedBits)
        {
            ec = make_error_code(Error::contentLengthMismatch);
            return;
        }

        if (unused >= 8)
        {
            ec = make_error_code(Error::badDerEncoding);
            return;
        }

        unsigned long bits = 0;
        auto const numBytes = slice.size();
        size_t curByteIndex = 0;
        for (; !slice.empty(); ++curByteIndex, slice += 1)
        {
            std::uint8_t const curByte = reverseBits(slice[0]);
            bits |= curByte << (curByteIndex * 8);

            if ((curByteIndex == numBytes - 1) && unused)
            {
                // check last byte for correct zero padding
                std::uint8_t const mask = 0xff & ~((1 << (8 - unused)) - 1);
                if (curByte & mask)
                {
                    // last byte has incorrect padding
                    ec = make_error_code(Error::badDerEncoding);
                    return;
                }
            }
        }

        v = bits;
    }

    static
    std::uint64_t
    length(
        std::bitset<S> const& s,
        boost::optional<GroupType> const& parentGroupType,
        TagMode encoderTagMode,
        TraitsCache& traitsCache)
    {
        static_assert(
            maxBytes > 0 && maxBytes <= sizeof(unsigned long),
            "Unsupported bitset size");
        auto const bits = s.to_ulong();

        if (bits == 0)
        {
            return 2;
        }

        std::size_t const leadingZeroBytes = numLeadingZeroBytes(s);
        // +1 to store unusedBits
        return 1 + maxBytes - leadingZeroBytes;
    }

    static
    int
    compare(
        std::bitset<S> const& lhs,
        std::bitset<S> const& rhs,
        TraitsCache& traitsCache)
    {
        static_assert(
            maxBytes > 0 && maxBytes <= sizeof(unsigned long),
            "Unsupported bitset size");
        unsigned long const bits[2] = {lhs.to_ulong(), rhs.to_ulong()};

        std::size_t const leadingZeroBytes[2]{
            numLeadingZeroBytes (lhs), numLeadingZeroBytes (rhs)};

        if (leadingZeroBytes[0] != leadingZeroBytes[1])
        {
            if (leadingZeroBytes[0] < leadingZeroBytes[1])
                // when leadingZeroBytes is less, size will be greater
                return 1;
            return -1;
        }

        std::uint8_t const unusedBits[2]{
            numUnusedBits (lhs, leadingZeroBytes[0]),
            numUnusedBits (rhs, leadingZeroBytes[1])};

        if (unusedBits[0] != unusedBits[1])
        {
            if (unusedBits[0] < unusedBits[1])
                return -1;
            return 1;
        }

        // leadingZeroBytes and unusedBits are equal
        for (size_t curByte = 0; curByte < maxBytes - leadingZeroBytes[0];
             ++curByte)
        {
            uint8_t const v[2] = {
                reverseBits(static_cast<uint8_t>((bits[0] >> curByte * 8) & 0xff)),
                reverseBits(static_cast<uint8_t>((bits[1] >> curByte * 8) & 0xff))};
            if (v[0] != v[1])
            {
                if (v[0] < v[1])
                    return -1;
                return 1;
            }
        }
        return 0;
    }
};

/** wrapper class for coding c++ collections as ans.1 sets

    @see {@link #SequenceWrapper} for the wrapper for sequences
    @see {@link #make_set} for the convenience factory function

    @note There are two types of collections in ans.1 - sets and sequences.
          Given a c++ collection - i.e. a std::vector - the coders need to know
          if the collection should be coded as a set or a sequence. This class
          is the way to tell the coder which collection to use.
 */
template <class T>
struct SetOfWrapper
{
    using value_type = typename T::value_type;

    T& col_;
    boost::container::small_vector<size_t, 8> sortOrder_;

    /** wrap the collection as a DER set

        @param col Collection to wrap
        @param sorted Flag that determines if the collection is already sorted
     */
    SetOfWrapper(T& col, TraitsCache& traitsCache, bool sorted = false)
        : col_(col), sortOrder_(col.size())
    {
        if (auto const cached = traitsCache.sortOrder(&col))
        {
            sortOrder_ = *cached;
            return;
        }

        // contains the indexes into subChoices_ so if the elements will be
        // sorted if accessed in the order specified by sortIndex_
        std::iota(sortOrder_.begin(), sortOrder_.end(), 0);
        if (!sorted)
        {
            std::sort(
                sortOrder_.begin(),
                sortOrder_.end(),
                [&col, &traitsCache](std::size_t lhs, std::size_t rhs) {
                    using Traits = cryptoconditions::der::DerCoderTraits<
                        std::decay_t<decltype(col[0])>>;
                    return Traits::compare(col[lhs], col[rhs], traitsCache) < 0;
                });
            traitsCache.sortOrder(&col, sortOrder_);
        }
    }
};

/** wrapper class for coding c++ collections as ans.1 sets

    @see {@link #make_sequence} for the convenience factory function
    @see {@link #SetOfWrapper} for the wrapper for sets

    @note There are two types of collections in ans.1 - sets and sequences.
          Given a c++ collection - i.e. a std::vector - the coders need to know
          if the collection should be coded as a set or a sequence. This class
          is the way to tell the coder which collection to use.
 */
template <class T>
struct SequenceOfWrapper
{
    /** the collection being wrapped

       @note col_ may be a homogeneous collection like vector or a heterogeneous
             collection like tuple
    */
    T& col_;
    SequenceOfWrapper(T& col) : col_(col)
    {
    }
};

/** convenience function to wrap a c++ collection as it will be coded as an asn.1 set

    @param col Collection to wrap
    @param sorted Flag that determines if the collection is already sorted

    @{
 */
template <class T>
SetOfWrapper<T>
make_set(T& t, TraitsCache& traitsCache, bool sorted = false)
{
    return SetOfWrapper<T>(t, traitsCache, sorted);
}

template <class T>
SetOfWrapper<T>
make_set(T& t, Encoder& encoder, bool sorted = false)
{
    return SetOfWrapper<T>(t, encoder.traitsCache_, sorted);
}

template <class T>
SetOfWrapper<T>
make_set(T& t, Decoder& decoder, bool sorted = false)
{
    TraitsCache dummy; // cache traits are not used in decoding
    return SetOfWrapper<T>(t, dummy, sorted);
}
/** @} */

/// convenience function to wrap a c++ collection as it will be coded as an asn.1 sequence
template <class T>
auto
make_sequence(T& t)
{
    return SequenceOfWrapper<T>(t);
}

/// convenience function to wrap a c++ tuple as it will be coded as an asn.1 sequence
template <class... T>
auto
make_sequence(std::tuple<T...>& t)
{
    return SequenceOfWrapper<std::tuple<T...>>(t);
}

/** DerCoderTraits for types that will be coded as asn.1 sets

    @see {@link #DerCoderTraits}
    @see {@link #make_set}
    @see {@link #SetOfWrapper}
*/
template <class T>
struct DerCoderTraits<SetOfWrapper<T>>
{
    constexpr static GroupType
    groupType()
    {
        return GroupType::set;
    }

    constexpr static ClassId
    classId()
    {
        return ClassId::universal;
    }

    static boost::optional<std::uint8_t> const&
    tagNum()
    {
        static boost::optional<std::uint8_t> tn{tagSet};
        return tn;
    }

    static
    std::uint8_t
    tagNum(SetOfWrapper<T> const&)
    {
        return tagSet;
    }

    constexpr static
    bool
    primitive()
    {
        return false;
    }

    static
    void
    encode(Encoder& encoder, SetOfWrapper<T> const& v)
    {
        for(auto const i : v.sortOrder_)
        {
            encoder << v.col_[i];
            if (encoder.ec())
                return;
        }
    }

    static
    void
    decode(Decoder& decoder, SetOfWrapper<T>& v)
    {
        v.col_.clear();

        auto& slice = decoder.parentSlice();

        while (!slice.empty())
        {
            typename T::value_type val{};
            decoder >> val;
            if (decoder.ec())
                return;
            v.col_.emplace_back(std::move(val));
        }
    }

    static
    std::uint64_t
    length(
        SetOfWrapper<T> const& v,
        boost::optional<GroupType> const& parentGroupType,
        TagMode encoderTagMode,
        TraitsCache& traitsCache)
    {
        using ValueTraits = DerCoderTraits<typename T::value_type>;
        std::uint64_t l = 0;
        boost::optional<GroupType> const thisGroupType(groupType());
        std::uint64_t childNum = 0;
        for (auto const& e : v.col_)
        {
            l += totalLength<ValueTraits>(
                e, thisGroupType, encoderTagMode, traitsCache, childNum);
            ++childNum;
        }
        return l;
    }

    static
    int
    compare(
        SetOfWrapper<T> const& lhs,
        SetOfWrapper<T> const& rhs,
        TraitsCache& traitsCache)
    {
        auto const lhsSize = lhs.col_.size();
        auto const rhsSize = rhs.col_.size();

        if (lhsSize != rhsSize)
        {
            if (lhsSize < rhsSize)
                return -1;
            return 1;
        }

        auto const& lhsSortOrder = lhs.sortOrder_;
        auto const& rhsSortOrder = rhs.sortOrder_;

        using elementType = std::decay_t<decltype(lhs.col_[0])>;
        // sizes are equal
        for (size_t i = 0; i < lhsSize; ++i)
        {
            auto const r = DerCoderTraits<elementType>::compare(
                lhs.col_[lhsSortOrder[i]],
                rhs.col_[rhsSortOrder[i]],
                traitsCache);
            if (r != 0)
                return r;
        }

        return (lhsSize > rhsSize) - (lhsSize < rhsSize);
    }
};

/** DerCoderTraits for types that will be coded as asn.1 sequences

    @see {@link #DerCoderTraits}
    @see {@link #make_sequence}
    @see {@link #SequenceOfWrapper}
*/
template <class T>
struct DerCoderTraits<SequenceOfWrapper<T>>
{
    constexpr static
    GroupType
    groupType()
    {
        return GroupType::sequence;
    }

    constexpr static
    ClassId
    classId()
    {
        return ClassId::universal;
    }

    static
    boost::optional<std::uint8_t> const&
    tagNum()
    {
        static boost::optional<std::uint8_t> tn{tagSequence};
        return tn;
    }

    static
    std::uint8_t
    tagNum(SequenceOfWrapper<T> const&)
    {
        return tagSequence;
    }

    constexpr static
    bool
    primitive()
    {
        return false;
    }

    static
    void
    encode(Encoder& encoder, SequenceOfWrapper<T> const& v)
    {
        for (auto const& e : v.col_)
        {
            encoder << e;
            if (encoder.ec())
                return;
        }
    }

    static
    void
    decode(Decoder& decoder, SequenceOfWrapper<T>& v)
    {
        v.col_.clear();

        auto& slice = decoder.parentSlice();

        while (!slice.empty())
        {
            typename T::value_type val;
            decoder >> val;
            if (decoder.ec())
                return;
            v.col_.emplace_back(std::move(val));
        }
    }

    static
    std::uint64_t
    length(
        SequenceOfWrapper<T> const& v,
        boost::optional<GroupType> const& parentGroupType,
        TagMode encoderTagMode,
        TraitsCache& traitsCache)
    {
        using ValueTraits = DerCoderTraits<typename T::value_type>;
        std::uint64_t l = 0;
        boost::optional<GroupType> thisGroupType(groupType());
        std::uint64_t childNum = 0;
        for (auto const& e : v.col_)
        {
            l += totalLength<ValueTraits>(
                e, thisGroupType, encoderTagMode, traitsCache, childNum);
            ++childNum;
        }
        return l;
    }

    static
    int
    compare(
        SequenceOfWrapper<T> const& lhs,
        SequenceOfWrapper<T> const& rhs,
        TraitsCache& traitsCache)
    {
        auto const lhsSize = lhs.col_.size();
        auto const rhsSize = rhs.col_.size();
        if (lhsSize != rhsSize)
        {
            if (lhsSize < rhsSize)
                return -1;
            return 1;
        }

        // sizes are equal
        using elementType = std::decay_t<decltype(lhs.col_[0])>;
        for (size_t i = 0; i < lhsSize; ++i)
        {
            auto const r = DerCoderTraits<elementType>::compare(
                lhs.col_[i], rhs.col_[i], traitsCache);
            if (r != 0)
                return r;
        }

        return 0;
    }
};


#if ! GENERATING_DOCS // quickbook uses `..` as a parameter separator and cannot parse this decl
/** DerCoderTraits for std::tuples

    @note tuples will be decoded as ans.1 sequences

    @see {@link #DerCoderTraits}
*/
template <class... Ts>
struct DerCoderTraits<std::tuple<Ts&...>>
{
    using Tuple = std::tuple<Ts&...>;

    constexpr static
    GroupType
    groupType()
    {
        return GroupType::autoSequence;
    }

    constexpr static
    ClassId
    classId()
    {
        return ClassId::universal;
    }

    static
    boost::optional<std::uint8_t> const&
    tagNum()
    {
        static boost::optional<std::uint8_t> tn{tagSequence};
        return tn;
    }

    static
    std::uint8_t
    tagNum(Tuple const&)
    {
        return tagSequence;
    }

    constexpr static bool
    primitive()
    {
        return false;
    }

    template <class F, std::size_t... Is>
    static
    void
    forEachElement(
        Tuple const& elements,
        std::index_sequence<Is...>,
        F&& f)
    {
        // Sean Parent for_each_argument trick
        (void)std::array<int, sizeof...(Ts)>{
            {(f(std::get<Is>(elements)), 0)...}};
    }

    template <class F, std::size_t... Is>
    static
    void
    forEachIndex(
        std::index_sequence<Is...>,
        F&& f)
    {
        // Sean Parent for_each_argument trick
        (void)std::array<int, sizeof...(Ts)>{
            {(f(std::integral_constant<std::size_t, Is>{}), 0)...}};
    }

    template <std::size_t... Is>
    static
    void
    decodeElementsHelper(
        Decoder& decoder,
        Tuple const& elements,
        std::index_sequence<Is...>)
    {
        // Sean Parent for_each_argument trick
        (void)std::array<int, sizeof...(Ts)>{
            {((decoder >> std::get<Is>(elements)), 0)...}};
    }

    static
    void
    encode(Encoder& encoder, Tuple const& elements)
    {
        forEachElement(
            elements,
            std::index_sequence_for<Ts...>{},
            [&encoder](auto const& e) { encoder << e; });
    }

    static
    void
    decode(Decoder& decoder, Tuple const& elements)
    {
        forEachElement(
            elements, std::index_sequence_for<Ts...>{},
            [&decoder](auto& e) {decoder >> e;});
    }

    static
    std::uint64_t
    length(
        Tuple const& elements,
        boost::optional<GroupType> const& parentGroupType,
        TagMode encoderTagMode,
        TraitsCache& traitsCache)
    {
        std::uint64_t l = 0;
        boost::optional<GroupType> thisGroupType(groupType());
        forEachIndex(std::index_sequence_for<Ts...>{}, [&](auto indexParam) {
            // visual studio can't handle index as a constexpr
            constexpr typename decltype(indexParam)::value_type index =
                decltype(indexParam)::value;
            auto const& e = std::get<index>(elements);
            using ElementTraits = DerCoderTraits<std::decay_t<decltype(e)>>;
            std::uint64_t childNum = index;
            l += totalLength<ElementTraits>(
                e, thisGroupType, encoderTagMode, traitsCache, childNum);
        });
        return l;
    }

    template <class T>
    static
    void
    compareElementsHelper(
        T const& lhs,
        T const& rhs,
        TraitsCache& traitsCache,
        int* cmpResult)
    {
        if (*cmpResult)
            return;
        *cmpResult = DerCoderTraits<T>::compare(lhs, rhs, traitsCache);
    }

    template <std::size_t... Is>
    static
    int
    compareElementsHelper(
        Tuple const& lhs,
        Tuple const& rhs,
        TraitsCache& traitsCache,
        std::index_sequence<Is...>)
    {
        int result = 0;
        // Sean Parent for_each_argument trick
        (void)std::array<int, sizeof...(Ts)>{
            {((compareElementsHelper(std::get<Is>(lhs), std::get<Is>(rhs), traitsCache, &result)), 0)...}};
        return result;
    }

    static int
    compare(Tuple const& lhs, Tuple const& rhs, TraitsCache& traitsCache)
    {
        {
            // compare lengths even though the parent tag or tag mode are
            // unknown. Hard coding no parent tag and automatic tag mode
            // will still reveal differences in length.
            auto const lhsL = length(lhs, boost::none, TagMode::automatic, traitsCache);
            auto const rhsL = length(rhs, boost::none, TagMode::automatic, traitsCache);
            if (lhsL != rhsL)
            {
                if (lhsL < rhsL)
                    return -1;
                return 1;
            }
        }
        return compareElementsHelper(
            lhs, rhs, traitsCache, std::index_sequence_for<Ts...>{});
    }
};
#endif // ! GENERATING_DOCS

/** For types that define `withTuple`, encode the type.

    @note If the user defined type defines the function `withTuple`, then
    `withTupleEncodeHelper`, `withTupleDecodeHelper`, and
    `withTupleEncodeHelper` may be used to help implement the DerCoderTraits
    functions `encode`, `decode`, and `length`. The `withTuple` function should
    take a single parameter: a callback function. That callback function should
    take a single parameter, a tuple of references that represent the object being
    coded.
 */
template<class TChoice>
static
void
withTupleEncodeHelper(TChoice const& c, cryptoconditions::der::Encoder& encoder)
{
    c.withTuple(
        [&encoder](auto const& tup) { encoder << tup; },
        encoder.traitsCache_);
}

/** For types that define `withTuple`, decode the type.

    @see note on {@link #withTupleEncodeHelper}
 */
template<class TChoice>
static
void
withTupleDecodeHelper(TChoice& c, cryptoconditions::der::Decoder& decoder)
{
    TraitsCache dummy; // traits cache is not used in decoding
    c.withTuple([&decoder](auto&& tup) { decoder >> tup; },
                dummy);
}

/** For types that define `withTuple`, find the length, in bytes, of the encoded content.

    @see note on {@link #withTupleEncodeHelper}
 */
template <class TChoice>
static
std::uint64_t
withTupleEncodedLengthHelper(
    TChoice const& c,
    boost::optional<GroupType> const& parentGroupType,
    TagMode encoderTagMode,
    TraitsCache& traitsCache)
{
    std::uint64_t result = 0;
    boost::optional<GroupType> thisGroupType(GroupType::sequence);
    c.withTuple(
        [&](auto const& tup) {
            using T = std::decay_t<decltype(tup)>;
            using Traits = cryptoconditions::der::DerCoderTraits<T>;
            result =
                Traits::length(tup, thisGroupType, encoderTagMode, traitsCache);
        },
        traitsCache);
    return result;
}

/** For types that define `withTuple`, compare the type.

    @see note on {@link #withTupleEncodeHelper}
 */
template <class TChoiceDerived, class TChoiceBase>
static
int
withTupleCompareHelper(
    TChoiceDerived const& lhs,
    TChoiceBase const& rhs,
    TraitsCache& traitsCache)
{
    auto const lhsType = lhs.type();
    auto const rhsType = rhs.type();
    if (lhsType != rhsType)
    {
        if (lhsType < rhsType)
            return -1;
        return 1;
    }

    auto const pRhs = dynamic_cast<TChoiceDerived const*>(&rhs);
    if (!pRhs)
    {
        assert(0);
        return -1;
    }

    int result = 0;
    lhs.withTuple(
        [&](auto const& lhsTup) {
            pRhs->withTuple(
                [&](auto const& rhsTup) {
                    using traits =
                        DerCoderTraits<std::decay_t<decltype(lhsTup)>>;
                    result = traits::compare(lhsTup, rhsTup, traitsCache);
                },
                traitsCache);
        },
        traitsCache);
    return result;
}


}  // der
}  // cryptoconditions
}  // ripple

#endif
