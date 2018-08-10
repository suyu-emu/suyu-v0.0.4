// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_funcs.h"
#include "common/swap.h"
#include "content_archive.h"
#include "core/file_sys/nca_metadata.h"

namespace FileSys {

CNMT::CNMT(VirtualFile file_) : file(std::move(file_)), header(std::make_unique<CNMTHeader>()) {
    if (file->ReadObject(header.get()) != sizeof(CNMTHeader))
        return;

    // If type is {Application, Update, AOC} has opt-header.
    if (static_cast<u8>(header->type) >= 0x80 && static_cast<u8>(header->type) <= 0x82) {
        opt_header = std::make_unique<OptionalHeader>();
        if (file->ReadObject(opt_header.get(), sizeof(CNMTHeader)) != sizeof(OptionalHeader)) {
            opt_header = nullptr;
        }
    }

    for (u16 i = 0; i < header->number_content_entries; ++i) {
        auto& next = content_records.emplace_back(ContentRecord{});
        if (file->ReadObject(&next, sizeof(CNMTHeader) + i * sizeof(ContentRecord) +
                                        header->table_offset) != sizeof(ContentRecord)) {
            content_records.erase(content_records.end() - 1);
        }
    }

    for (u16 i = 0; i < header->number_meta_entries; ++i) {
        auto& next = meta_records.emplace_back(MetaRecord{});
        if (file->ReadObject(&next, sizeof(CNMTHeader) + i * sizeof(MetaRecord) +
                                        header->table_offset) != sizeof(MetaRecord)) {
            meta_records.erase(meta_records.end() - 1);
        }
    }
}

CNMT::CNMT(CNMTHeader header, OptionalHeader opt_header, std::vector<ContentRecord> content_records,
           std::vector<MetaRecord> meta_records)
    : file(nullptr), header(std::make_unique<CNMTHeader>(std::move(header))),
      opt_header(std::make_unique<OptionalHeader>(std::move(opt_header))),
      content_records(std::move(content_records)), meta_records(std::move(meta_records)) {}

u64 CNMT::GetTitleID() const {
    return header->title_id;
}

u32 CNMT::GetTitleVersion() const {
    return header->title_version;
}

TitleType CNMT::GetType() const {
    return header->type;
}

const std::vector<ContentRecord>& CNMT::GetContentRecords() const {
    return content_records;
}

const std::vector<MetaRecord>& CNMT::GetMetaRecords() const {
    return meta_records;
}

bool CNMT::UnionRecords(const CNMT& other) {
    bool change = false;
    for (const auto& rec : other.content_records) {
        const auto iter = std::find_if(
            content_records.begin(), content_records.end(),
            [rec](const ContentRecord& r) { return r.nca_id == rec.nca_id && r.type == rec.type; });
        if (iter == content_records.end()) {
            content_records.emplace_back(rec);
            ++header->number_content_entries;
            change = true;
        }
    }
    for (const auto& rec : other.meta_records) {
        const auto iter =
            std::find_if(meta_records.begin(), meta_records.end(), [rec](const MetaRecord& r) {
                return r.title_id == rec.title_id && r.title_version == rec.title_version &&
                       r.type == rec.type;
            });
        if (iter == meta_records.end()) {
            meta_records.emplace_back(rec);
            ++header->number_meta_entries;
            change = true;
        }
    }
    return change;
}

std::vector<u8> CNMT::Serialize() const {
    if (header == nullptr)
        return {};
    std::vector<u8> out(sizeof(CNMTHeader));
    out.reserve(0x100); // Avoid resizing -- average size.
    memcpy(out.data(), header.get(), sizeof(CNMTHeader));
    if (opt_header != nullptr) {
        out.resize(out.size() + sizeof(OptionalHeader));
        memcpy(out.data() + sizeof(CNMTHeader), opt_header.get(), sizeof(OptionalHeader));
    }

    auto offset = header->table_offset;

    const auto dead_zone = offset + sizeof(CNMTHeader) - out.size();
    if (dead_zone > 0)
        out.resize(offset + sizeof(CNMTHeader));

    for (const auto& rec : content_records) {
        out.resize(out.size() + sizeof(ContentRecord));
        memcpy(out.data() + offset + sizeof(CNMTHeader), &rec, sizeof(ContentRecord));
        offset += sizeof(ContentRecord);
    }

    for (const auto& rec : meta_records) {
        out.resize(out.size() + sizeof(MetaRecord));
        memcpy(out.data() + offset + sizeof(CNMTHeader), &rec, sizeof(MetaRecord));
        offset += sizeof(MetaRecord);
    }

    return out;
}
} // namespace FileSys
