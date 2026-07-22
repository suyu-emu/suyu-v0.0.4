// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/bcat/news/msgpack.h"

#include <algorithm>
#include <cstring>

// This file is a partial MsgPack implementation, only implementing the features
// needed for the News service. Can be extended but enough for the news use case.

namespace Service::News {

void MsgPack::Writer::WriteBytes(std::span<const u8> bytes) {
    out.insert(out.end(), bytes.begin(), bytes.end());
}

void MsgPack::Writer::WriteFixMap(size_t count) {
    if (count <= 15) {
        out.push_back(static_cast<u8>(0x80 | count));
    } else if (count <= 0xFFFF) {
        out.push_back(0xDE);
        out.push_back(static_cast<u8>((count >> 8) & 0xFF));
        out.push_back(static_cast<u8>(count & 0xFF));
    } else {
        WriteMap32(count);
    }
}

void MsgPack::Writer::WriteMap32(size_t count) {
    out.push_back(0xDF);
    out.push_back(static_cast<u8>((count >> 24) & 0xFF));
    out.push_back(static_cast<u8>((count >> 16) & 0xFF));
    out.push_back(static_cast<u8>((count >> 8) & 0xFF));
    out.push_back(static_cast<u8>(count & 0xFF));
}

void MsgPack::Writer::WriteKey(std::string_view s) {
    WriteString(s);
}

void MsgPack::Writer::WriteString(std::string_view s) {
    if (s.size() <= 31) {
        out.push_back(static_cast<u8>(0xA0 | s.size()));
    } else if (s.size() <= 0xFF) {
        out.push_back(0xD9);
        out.push_back(static_cast<u8>(s.size()));
    } else if (s.size() <= 0xFFFF) {
        out.push_back(0xDA);
        out.push_back(static_cast<u8>((s.size() >> 8) & 0xFF));
        out.push_back(static_cast<u8>(s.size() & 0xFF));
    } else {
        out.push_back(0xDB);
        out.push_back(static_cast<u8>((s.size() >> 24) & 0xFF));
        out.push_back(static_cast<u8>((s.size() >> 16) & 0xFF));
        out.push_back(static_cast<u8>((s.size() >> 8) & 0xFF));
        out.push_back(static_cast<u8>(s.size() & 0xFF));
    }
    WriteBytes({reinterpret_cast<const u8*>(s.data()), s.size()});
}

void MsgPack::Writer::WriteInt64(s64 v) {
    if (v >= 0) {
        WriteUInt(static_cast<u64>(v));
        return;
    }
    if (v >= -32) {
        out.push_back(static_cast<u8>(0xE0 | (v + 32)));
    } else if (v >= -128) {
        out.push_back(0xD0);
        out.push_back(static_cast<u8>(v & 0xFF));
    } else if (v >= -32768) {
        out.push_back(0xD1);
        out.push_back(static_cast<u8>((v >> 8) & 0xFF));
        out.push_back(static_cast<u8>(v & 0xFF));
    } else if (v >= INT32_MIN) {
        out.push_back(0xD2);
        out.push_back(static_cast<u8>((v >> 24) & 0xFF));
        out.push_back(static_cast<u8>((v >> 16) & 0xFF));
        out.push_back(static_cast<u8>((v >> 8) & 0xFF));
        out.push_back(static_cast<u8>(v & 0xFF));
    } else {
        out.push_back(0xD3);
        for (int i = 7; i >= 0; --i) {
            out.push_back(static_cast<u8>((static_cast<u64>(v) >> (8 * i)) & 0xFF));
        }
    }
}

void MsgPack::Writer::WriteUInt(u64 v) {
    if (v < 0x80) {
        out.push_back(static_cast<u8>(v));
    } else if (v <= 0xFF) {
        out.push_back(0xCC);
        out.push_back(static_cast<u8>(v));
    } else if (v <= 0xFFFF) {
        out.push_back(0xCD);
        out.push_back(static_cast<u8>((v >> 8) & 0xFF));
        out.push_back(static_cast<u8>(v & 0xFF));
    } else if (v <= 0xFFFFFFFFULL) {
        out.push_back(0xCE);
        out.push_back(static_cast<u8>((v >> 24) & 0xFF));
        out.push_back(static_cast<u8>((v >> 16) & 0xFF));
        out.push_back(static_cast<u8>((v >> 8) & 0xFF));
        out.push_back(static_cast<u8>(v & 0xFF));
    } else {
        out.push_back(0xCF);
        for (int i = 7; i >= 0; --i) {
            out.push_back(static_cast<u8>((v >> (8 * i)) & 0xFF));
        }
    }
}

void MsgPack::Writer::WriteNil() {
    out.push_back(0xC0);
}

void MsgPack::Writer::WriteFixArray(size_t count) {
    if (count <= 15) {
        out.push_back(static_cast<u8>(0x90 | count));
    } else if (count <= 0xFFFF) {
        out.push_back(0xDC);
        out.push_back(static_cast<u8>((count >> 8) & 0xFF));
        out.push_back(static_cast<u8>(count & 0xFF));
    } else {
        out.push_back(0xDD);
        out.push_back(static_cast<u8>((count >> 24) & 0xFF));
        out.push_back(static_cast<u8>((count >> 16) & 0xFF));
        out.push_back(static_cast<u8>((count >> 8) & 0xFF));
        out.push_back(static_cast<u8>(count & 0xFF));
    }
}

void MsgPack::Writer::WriteBinary(const std::vector<u8>& data) {
    if (data.size() <= 0xFF) {
        out.push_back(0xC4);
        out.push_back(static_cast<u8>(data.size()));
    } else if (data.size() <= 0xFFFF) {
        out.push_back(0xC5);
        out.push_back(static_cast<u8>((data.size() >> 8) & 0xFF));
        out.push_back(static_cast<u8>(data.size() & 0xFF));
    } else {
        out.push_back(0xC6);
        out.push_back(static_cast<u8>((data.size() >> 24) & 0xFF));
        out.push_back(static_cast<u8>((data.size() >> 16) & 0xFF));
        out.push_back(static_cast<u8>((data.size() >> 8) & 0xFF));
        out.push_back(static_cast<u8>(data.size() & 0xFF));
    }
    WriteBytes(data);
}

void MsgPack::Writer::WriteBool(bool v) {
    out.push_back(v ? 0xC3 : 0xC2);
}

std::vector<u8> MsgPack::Writer::Take() {
    return std::move(out);
}

MsgPack::Reader::Reader(std::span<const u8> buffer) : data(buffer) {}

bool MsgPack::Reader::Fail(const char* msg) {
    error = msg;
    return false;
}

bool MsgPack::Reader::Ensure(size_t n) const {
    return offset + n <= data.size();
}

u8 MsgPack::Reader::Peek() const {
    return Ensure(1) ? data[offset] : 0;
}

u8 MsgPack::Reader::ReadByte() {
    if (!Ensure(1)) {
        return 0;
    }
    return data[offset++];
}

bool MsgPack::Reader::ReadSize(size_t byte_count, size_t& out_size) {
    if (!Ensure(byte_count)) {
        return Fail("size out of range");
    }
    size_t value = 0;
    for (size_t i = 0; i < byte_count; ++i) {
        value = (value << 8) | data[offset + i];
    }
    offset += byte_count;
    out_size = value;
    return true;
}

bool MsgPack::Reader::SkipBytes(size_t n) {
    if (!Ensure(n)) {
        return Fail("skip out of range");
    }
    offset += n;
    return true;
}

bool MsgPack::Reader::SkipValue() {
    if (End()) {
        return Fail("unexpected end");
    }
    const u8 byte = ReadByte();

    if (byte <= 0x7F || (byte >= 0xE0)) {
        return true;
    }
    if ((byte & 0xE0) == 0xA0) {
        const size_t len = byte & 0x1F;
        return SkipBytes(len);
    }
    if ((byte & 0xF0) == 0x80) {
        const size_t count = byte & 0x0F;
        return SkipContainer(count * 2, true);
    }
    if ((byte & 0xF0) == 0x90) {
        const size_t count = byte & 0x0F;
        return SkipContainer(count, false);
    }

    switch (byte) {
    case 0xC0:
    case 0xC2:
    case 0xC3:
        return true;
    case 0xC4:
    case 0xC5:
    case 0xC6: {
        size_t size = 0;
        if (!ReadSize(static_cast<size_t>(1) << (byte - 0xC4), size)) {
            return false;
        }
        return SkipBytes(size);
    }
    case 0xC7:
    case 0xC8:
    case 0xC9:
        return Fail("ext not supported");
    case 0xCA:
        return SkipBytes(4);
    case 0xCB:
        return SkipBytes(8);
    case 0xCC:
        return SkipBytes(1);
    case 0xCD:
        return SkipBytes(2);
    case 0xCE:
        return SkipBytes(4);
    case 0xCF:
        return SkipBytes(8);
    case 0xD0:
        return SkipBytes(1);
    case 0xD1:
        return SkipBytes(2);
    case 0xD2:
        return SkipBytes(4);
    case 0xD3:
        return SkipBytes(8);
    case 0xD4:
    case 0xD5:
    case 0xD6:
    case 0xD7:
    case 0xD8:
        return Fail("fixext not supported");
    case 0xD9: {
        size_t size = 0;
        if (!ReadSize(1, size)) {
            return false;
        }
        return SkipBytes(size);
    }
    case 0xDA: {
        size_t size = 0;
        if (!ReadSize(2, size)) {
            return false;
        }
        return SkipBytes(size);
    }
    case 0xDB: {
        size_t size = 0;
        if (!ReadSize(4, size)) {
            return false;
        }
        return SkipBytes(size);
    }
    case 0xDC: {
        size_t count = 0;
        if (!ReadSize(2, count)) {
            return false;
        }
        return SkipContainer(count, false);
    }
    case 0xDD: {
        size_t count = 0;
        if (!ReadSize(4, count)) {
            return false;
        }
        return SkipContainer(count, false);
    }
    case 0xDE: {
        size_t count = 0;
        if (!ReadSize(2, count)) {
            return false;
        }
        return SkipContainer(count * 2, true);
    }
    case 0xDF: {
        size_t count = 0;
        if (!ReadSize(4, count)) {
            return false;
        }
        return SkipContainer(count * 2, true);
    }
    default:
        return Fail("unknown type");
    }
}

bool MsgPack::Reader::SkipContainer(size_t count, bool /*map_mode*/) {
    for (size_t i = 0; i < count; ++i) {
        if (!SkipValue()) {
            return false;
        }
    }
    return true;
}

bool MsgPack::Reader::SkipAll() {
    while (!End()) {
        if (!SkipValue()) {
            return false;
        }
    }
    return true;
}

bool MsgPack::Reader::ReadMapHeader(size_t& count) {
    if (End()) {
        return Fail("unexpected end");
    }
    const u8 byte = Peek();
    if ((byte & 0xF0) == 0x80) {
        count = byte & 0x0F;
        offset++;
        return true;
    }
    if (byte == 0xDE) {
        ReadByte();
        return ReadSize(2, count);
    }
    if (byte == 0xDF) {
        ReadByte();
        return ReadSize(4, count);
    }
    return Fail("not a map");
}

bool MsgPack::Reader::ReadArrayHeader(size_t& count) {
    if (End()) {
        return Fail("unexpected end");
    }
    const u8 byte = Peek();
    if ((byte & 0xF0) == 0x90) {
        count = byte & 0x0F;
        offset++;
        return true;
    }
    if (byte == 0xDC) {
        ReadByte();
        return ReadSize(2, count);
    }
    if (byte == 0xDD) {
        ReadByte();
        return ReadSize(4, count);
    }
    return Fail("not an array");
}

bool MsgPack::Reader::ReadUInt(u64& value) {
    if (End()) {
        return Fail("unexpected end");
    }
    const u8 byte = Peek();
    if (byte <= 0x7F) {
        value = byte;
        offset++;
        return true;
    }
    if (byte == 0xCC) {
        ReadByte();
        if (!Ensure(1)) {
            return Fail("uint8 truncated");
        }
        value = data[offset++];
        return true;
    }
    if (byte == 0xCD) {
        ReadByte();
        size_t tmp = 0;
        if (!ReadSize(2, tmp)) {
            return false;
        }
        value = tmp;
        return true;
    }
    if (byte == 0xCE) {
        ReadByte();
        size_t tmp = 0;
        if (!ReadSize(4, tmp)) {
            return false;
        }
        value = tmp;
        return true;
    }
    if (byte == 0xCF) {
        ReadByte();
        if (!Ensure(8)) {
            return Fail("uint64 truncated");
        }
        u64 tmp = 0;
        for (int i = 0; i < 8; ++i) {
            tmp = (tmp << 8) | data[offset + i];
        }
        offset += 8;
        value = tmp;
        return true;
    }
    return Fail("not uint");
}

bool MsgPack::Reader::ReadInt(s64& value) {
    if (End()) {
        return Fail("unexpected end");
    }
    const u8 byte = Peek();
    if (byte <= 0x7F) {
        value = byte;
        offset++;
        return true;
    }
    if (byte >= 0xE0) {
        value = static_cast<int8_t>(byte);
        offset++;
        return true;
    }
    if (byte == 0xD0) {
        ReadByte();
        if (!Ensure(1)) {
            return Fail("int8 truncated");
        }
        value = static_cast<int8_t>(data[offset]);
        offset += 1;
        return true;
    }
    if (byte == 0xD1) {
        ReadByte();
        if (!Ensure(2)) {
            return Fail("int16 truncated");
        }
        value = static_cast<int16_t>((data[offset] << 8) | data[offset + 1]);
        offset += 2;
        return true;
    }
    if (byte == 0xD2) {
        ReadByte();
        if (!Ensure(4)) {
            return Fail("int32 truncated");
        }
        const s32 tmp = static_cast<int32_t>((data[offset] << 24) | (data[offset + 1] << 16) |
                                             (data[offset + 2] << 8) | data[offset + 3]);
        offset += 4;
        value = tmp;
        return true;
    }
    if (byte == 0xD3) {
        ReadByte();
        if (!Ensure(8)) {
            return Fail("int64 truncated");
        }
        s64 tmp = 0;
        for (int i = 0; i < 8; ++i) {
            tmp = (tmp << 8) | data[offset + i];
        }
        offset += 8;
        value = tmp;
        return true;
    }
    return Fail("not int");
}

bool MsgPack::Reader::ReadBool(bool& value) {
    if (End()) {
        return Fail("unexpected end");
    }
    const u8 byte = ReadByte();
    if (byte == 0xC2) {
        value = false;
        return true;
    }
    if (byte == 0xC3) {
        value = true;
        return true;
    }
    return Fail("not bool");
}

bool MsgPack::Reader::ReadString(std::string& value) {
    if (End()) {
        return Fail("unexpected end");
    }
    const u8 byte = ReadByte();
    size_t len = 0;
    if ((byte & 0xE0) == 0xA0) {
        len = byte & 0x1F;
    } else if (byte == 0xD9) {
        if (!ReadSize(1, len)) {
            return false;
        }
    } else if (byte == 0xDA) {
        if (!ReadSize(2, len)) {
            return false;
        }
    } else if (byte == 0xDB) {
        if (!ReadSize(4, len)) {
            return false;
        }
    } else {
        return Fail("not string");
    }

    if (!Ensure(len)) {
        return Fail("string truncated");
    }
    value.assign(reinterpret_cast<const char*>(data.data() + offset), len);
    offset += len;
    return true;
}

bool MsgPack::Reader::ReadBinary(std::vector<u8>& value) {
    if (End()) {
        return Fail("unexpected end");
    }
    const u8 byte = ReadByte();
    size_t len = 0;
    if (byte == 0xC4) {
        if (!ReadSize(1, len)) {
            return false;
        }
    } else if (byte == 0xC5) {
        if (!ReadSize(2, len)) {
            return false;
        }
    } else if (byte == 0xC6) {
        if (!ReadSize(4, len)) {
            return false;
        }
    } else {
        return Fail("not binary");
    }
    if (!Ensure(len)) {
        return Fail("binary truncated");
    }
    value.assign(data.begin() + offset, data.begin() + offset + len);
    offset += len;
    return true;
}

bool MsgPack::Reader::ReadBinaryCompat(std::vector<u8>& value) {
    if (!ReadBinary(value)) {
        value.clear();
        return false;
    }
    return true;
}

bool MsgPack::Reader::ReadStringArray(std::vector<std::string>& out) {
    size_t count = 0;
    if (!ReadArrayHeader(count)) {
        return false;
    }
    out.clear();
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        std::string value;
        if (!ReadString(value)) {
            return false;
        }
        out.push_back(std::move(value));
    }
    return true;
}

bool MsgPack::Reader::ReadNewsVersion(NewsStruct::Version& out) {
    size_t count = 0;
    if (!ReadMapHeader(count)) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        std::string key;
        if (!ReadString(key)) {
            return false;
        }
        if (key == "format") {
            if (!ReadUInt(out.format)) {
                return false;
            }
        } else if (key == "semantics") {
            if (!ReadUInt(out.semantics)) {
                return false;
            }
        } else {
            if (!SkipValue()) {
                return false;
            }
        }
    }
    return true;
}

bool MsgPack::Reader::ReadNewsSubject(NewsStruct::Subject& out) {
    size_t count = 0;
    if (!ReadMapHeader(count)) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        std::string key;
        if (!ReadString(key)) {
            return false;
        }
        if (key == "caption") {
            if (!ReadUInt(out.caption)) {
                return false;
            }
        } else if (key == "text") {
            if (!ReadString(out.text)) {
                return false;
            }
        } else {
            if (!SkipValue()) {
                return false;
            }
        }
    }
    return true;
}

bool MsgPack::Reader::ReadNewsFooter(NewsStruct::Footer& out) {
    size_t count = 0;
    if (!ReadMapHeader(count)) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        std::string key;
        if (!ReadString(key)) {
            return false;
        }
        if (key == "text") {
            if (!ReadString(out.text)) {
                return false;
            }
        } else {
            if (!SkipValue()) {
                return false;
            }
        }
    }
    return true;
}

bool MsgPack::Reader::ReadByteArray(std::vector<u8>& out) {
    if (!ReadBinary(out)) {
        out.clear();
        return false;
    }
    return true;
}

bool MsgPack::Reader::ReadNewsBody(NewsStruct::Body& out) {
    size_t count = 0;
    if (!ReadMapHeader(count)) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        std::string key;
        if (!ReadString(key)) {
            return false;
        }
        if (key == "text") {
            if (!ReadString(out.text)) {
                return false;
            }
        } else if (key == "main_image_height") {
            if (!ReadUInt(out.main_image_height)) {
                return false;
            }
        } else if (key == "movie_url") {
            if (!ReadString(out.movie_url)) {
                return false;
            }
        } else if (key == "main_image") {
            if (!ReadByteArray(out.main_image)) {
                return false;
            }
        } else {
            if (!SkipValue()) {
                return false;
            }
        }
    }
    return true;
}

bool MsgPack::Reader::ReadNewsBrowser(NewsStruct::More::Browser& out) {
    size_t count = 0;
    if (!ReadMapHeader(count)) {
        return false;
    }
    out.present = true;
    for (size_t i = 0; i < count; ++i) {
        std::string key;
        if (!ReadString(key)) {
            return false;
        }
        if (key == "url") {
            if (!ReadString(out.url)) {
                return false;
            }
        } else if (key == "text") {
            if (!ReadString(out.text)) {
                return false;
            }
        } else {
            if (!SkipValue()) {
                return false;
            }
        }
    }
    return true;
}

bool MsgPack::Reader::ReadNewsMore(NewsStruct::More& out) {
    size_t count = 0;
    if (!ReadMapHeader(count)) {
        return false;
    }
    out.has_browser = false;
    for (size_t i = 0; i < count; ++i) {
        std::string key;
        if (!ReadString(key)) {
            return false;
        }
        if (key == "browser") {
            out.has_browser = true;
            if (!ReadNewsBrowser(out.browser)) {
                return false;
            }
        } else {
            if (!SkipValue()) {
                return false;
            }
        }
    }
    return true;
}

bool MsgPack::Reader::ReadNewsStruct(NewsStruct& out) {
    size_t count = 0;
    if (!ReadMapHeader(count)) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        std::string key;
        if (!ReadString(key)) {
            return false;
        }

        auto read_u64 = [&](u64& target) -> bool {
            return ReadUInt(target);
        };

        if (key == "version") {
            if (!ReadNewsVersion(out.version)) {
                return false;
            }
        } else if (key == "news_id") {
            if (!read_u64(out.news_id)) {
                return false;
            }
        } else if (key == "published_at") {
            if (!read_u64(out.published_at)) {
                return false;
            }
        } else if (key == "pickup_limit") {
            if (!read_u64(out.pickup_limit)) {
                return false;
            }
        } else if (key == "priority") {
            if (!read_u64(out.priority)) {
                return false;
            }
        } else if (key == "deletion_priority") {
            if (!read_u64(out.deletion_priority)) {
                return false;
            }
        } else if (key == "language") {
            if (!ReadString(out.language)) {
                return false;
            }
        } else if (key == "supported_languages") {
            if (!ReadStringArray(out.supported_languages)) {
                return false;
            }
        } else if (key == "display_type") {
            if (!ReadString(out.display_type)) {
                return false;
            }
        } else if (key == "topic_id") {
            if (!ReadString(out.topic_id)) {
                return false;
            }
        } else if (key == "no_photography") {
            if (!read_u64(out.no_photography)) {
                return false;
            }
        } else if (key == "surprise") {
            if (!read_u64(out.surprise)) {
                return false;
            }
        } else if (key == "bashotorya") {
            if (!read_u64(out.bashotorya)) {
                return false;
            }
        } else if (key == "movie") {
            if (!read_u64(out.movie)) {
                return false;
            }
        } else if (key == "subject") {
            if (!ReadNewsSubject(out.subject)) {
                return false;
            }
        } else if (key == "topic_name") {
            if (!ReadString(out.topic_name)) {
                return false;
            }
        } else if (key == "list_image") {
            if (!ReadByteArray(out.list_image)) {
                return false;
            }
        } else if (key == "footer") {
            if (!ReadNewsFooter(out.footer)) {
                return false;
            }
        } else if (key == "allow_domains") {
            if (!ReadString(out.allow_domains)) {
                return false;
            }
        } else if (key == "more") {
            if (!ReadNewsMore(out.more)) {
                return false;
            }
        } else if (key == "body") {
            if (!ReadNewsBody(out.body)) {
                return false;
            }
        } else if (key == "contents_descriptors") {
            if (!ReadString(out.contents_descriptors)) {
                return false;
            }
        } else if (key == "interactive_elements") {
            if (!ReadString(out.interactive_elements)) {
                return false;
            }
        } else {
            if (!SkipValue()) {
                return false;
            }
        }
    }
    return true;
}

} // namespace Service::News
