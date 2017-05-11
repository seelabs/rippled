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

#include <ripple/conditions/impl/Der.h>
#include <test/conditions/DerChoice.h>

#include <boost/filesystem.hpp>

#include <fstream>
#include <string>

// Create a corpus for fuzz testing
void createCorpus(boost::filesystem::path const& outDir)
{
    using namespace boost::filesystem;
    using namespace ripple::cryptoconditions::der;

    if (exists(outDir))
    {
        std::cerr << "Corpus directory already exists. Exiting.\n";
        return;
    }
    create_directory(outDir);
    auto writeData = [](
        boost::filesystem::path const& fileName, Encoder const& encoder) {

        std::vector<char> data;
        data.reserve(encoder.size());
        encoder.write(data);
        std::ofstream ostr(fileName.native(), std::ofstream::binary);
        ostr.write(data.data(), data.size());
        ostr.close();
    };

    auto fileName = [&outDir] {
        static int fileNum = 0;
        char buf[64];
        snprintf(buf, 64, "corpus%d.dat", ++fileNum);
        return outDir / buf;
    };

    auto addCorpus = [&](auto const& v) {
        Encoder encoder{TagMode::direct};
        encoder << v << eos;
        writeData(fileName(), encoder);
    };

    auto stringCorpus = [&](size_t n) {
        std::string s;
        s.resize(n);
        std::fill_n(s.begin(), n, 'a');
        addCorpus(s);
    };

    addCorpus(0u);
    addCorpus(1u);
    addCorpus(0xffu);
    addCorpus(0xfeu);
    addCorpus(-1);
    addCorpus(-2);
    addCorpus(std::int32_t(0xffffff00));
    addCorpus(std::uint32_t(0xfffffffe));
    addCorpus(std::int32_t(210));
    addCorpus(0x101u);
    addCorpus(0x1000u);
    addCorpus(0x10001u);
    addCorpus(0x100000u);
    addCorpus(0x1001001u);
    addCorpus(0x1001001u);
    addCorpus(0x1000000000000000u);

    stringCorpus(1);
    stringCorpus(127);
    stringCorpus(128);
    stringCorpus(66000);
    {
        Encoder encoder{TagMode::direct};
        {
            EosGuard<Encoder> eg{encoder};
            GroupGuard<Encoder> sq1{encoder, SequenceTag{}};
            encoder << 10;
        }
        writeData(fileName(), encoder);
    }
    {
        Encoder encoder{TagMode::direct};
        {
            EosGuard<Encoder> eg{encoder};
            GroupGuard<Encoder> sq1{encoder, SequenceTag{}};
            encoder << 10 << 100000 << 100000000000;
        }
        writeData(fileName(), encoder);
    }
    {
        Encoder encoder{TagMode::direct};
        {
            EosGuard<Encoder> eg{encoder};
            GroupGuard<Encoder> sq1{encoder, SetTag{}};
            encoder << 10;
        }
        writeData(fileName(), encoder);
    }
    {
        Encoder encoder{TagMode::direct};
        {
            EosGuard<Encoder> eg{encoder};
            GroupGuard<Encoder> sq1{encoder, SetTag{}};
            encoder << 10 << 100000 << 100000000000;
        }
        writeData(fileName(), encoder);
    }
    {
        using namespace ripple::test;
        std::vector<char> buf({'a', 'a'});
        std::string str("AA");
        int signedInt = -3;
        std::uint64_t id = 66000;
        int childIndex = 0;  // even indexes are of type derived1, odd
                             // indexes are of type derived2
        // choice
        std::function<std::unique_ptr<DerChoiceBaseClass>(int)> createDerived =
            [&](int level) -> std::unique_ptr<DerChoiceBaseClass> {
            ++childIndex;
            if (childIndex % 2)
            {
                std::vector<std::unique_ptr<DerChoiceBaseClass>> children;
                if (level > 1)
                {
                    for (int i = 0; i < 5; ++i)
                        children.emplace_back(createDerived(level - 1));
                }
                ++signedInt;
                ++buf[0];
                return std::make_unique<DerChoiceDerived1>(
                    buf, std::move(children), signedInt);
            }
            else
            {
                if (str[1] == 'Z')
                {
                    ++str[0];
                    str[1] = 'A';
                }
                else
                    ++str[1];
                ++id;
                return std::make_unique<DerChoiceDerived2>(str, id);
            }
        };

        auto root = createDerived(/*levels*/ 5);
        Encoder encoder{TagMode::direct};
        encoder << root << eos;
        writeData(fileName(), encoder);
    }
}

int main(int argc, char const* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Must specify output directory\n";
        return 1;
    }
    createCorpus(argv[1]);
    return 0;
}
