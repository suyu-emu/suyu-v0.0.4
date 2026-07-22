// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "common/common_types.h"

namespace Service::News {

struct NewsStruct {
    struct Version {
        u64 format{};
        u64 semantics{};
    };

    struct Subject {
        u64 caption{};
        std::string text;
    };

    struct Footer {
        std::string text;
    };

    struct Body {
        std::string text;
        u64 main_image_height{};
        std::string movie_url;
        std::vector<u8> main_image;
    };

    struct More {
        struct Browser {
            std::string url;
            std::string text;
            bool present{};
        } browser;
        bool has_browser{};
    };

    Version version;
    u64 news_id{};
    u64 published_at{};
    u64 pickup_limit{};
    u64 priority{};
    u64 deletion_priority{};
    std::string language;
    std::vector<std::string> supported_languages;
    std::string display_type;
    std::string topic_id;
    u64 no_photography{};
    u64 surprise{};
    u64 bashotorya{};
    u64 movie{};
    Subject subject;
    std::string topic_name;
    std::vector<u8> list_image;
    Footer footer;
    std::string allow_domains;
    More more;
    Body body;
    std::string contents_descriptors;
    std::string interactive_elements;
};

class MsgPack {
public:
    class Writer {
    public:
        void WriteFixMap(size_t count);
        void WriteMap32(size_t count);
        void WriteKey(std::string_view key);
        void WriteString(std::string_view value);
        void WriteInt64(s64 value);
        void WriteUInt(u64 value);
        void WriteNil();
        void WriteFixArray(size_t count);
        void WriteBinary(const std::vector<u8>& data);
        void WriteBool(bool value);

        std::vector<u8> Take();
        const std::vector<u8>& Buffer() const { return out; }
        void Clear() { out.clear(); }

    private:
        void WriteBytes(std::span<const u8> bytes);

        std::vector<u8> out;
    };

    class Reader {
    public:
        explicit Reader(std::span<const u8> buffer);

        template <typename T>
        bool Read(T& out) {
            if constexpr (std::is_same_v<T, NewsStruct>) {
                return ReadNewsStruct(out);
            } else {
                static_assert(sizeof(T) == 0, "Unsupported MsgPack::Reader::Read type");
            }
        }

        bool SkipValue();
        bool SkipAll();

        bool ReadMapHeader(size_t& count);
        bool ReadArrayHeader(size_t& count);
        bool ReadUInt(u64& value);
        bool ReadInt(s64& value);
        bool ReadBool(bool& value);
        bool ReadString(std::string& value);
        bool ReadBinary(std::vector<u8>& value);

        bool End() const { return offset >= data.size(); }
        std::string_view Error() const { return error; }

    private:
        bool Fail(const char* msg);
        bool Ensure(size_t n) const;
        u8 Peek() const;
        u8 ReadByte();
        bool ReadSize(size_t byte_count, size_t& out_size);
        bool SkipBytes(size_t n);
        bool SkipContainer(size_t count, bool map_mode);
        bool ReadNewsStruct(NewsStruct& out);
        bool ReadNewsVersion(NewsStruct::Version& out);
        bool ReadNewsSubject(NewsStruct::Subject& out);
        bool ReadNewsFooter(NewsStruct::Footer& out);
        bool ReadNewsBody(NewsStruct::Body& out);
        bool ReadNewsMore(NewsStruct::More& out);
        bool ReadNewsBrowser(NewsStruct::More::Browser& out);
        bool ReadStringArray(std::vector<std::string>& out);
        bool ReadBinaryCompat(std::vector<u8>& out);
        bool ReadByteArray(std::vector<u8>& out);

        std::span<const u8> data;
        size_t offset{0};
        std::string error;
    };
};

} // namespace Service::News
