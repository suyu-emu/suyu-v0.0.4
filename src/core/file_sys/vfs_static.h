// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <memory>
#include <string_view>

#include "core/file_sys/vfs.h"

namespace FileSys {

class StaticVfsFile : public VfsFile {
public:
    explicit StaticVfsFile(u8 value, std::size_t size = 0, std::string name = "",
                           VirtualDir parent = nullptr)
        : value{value}, size{size}, name{std::move(name)}, parent{std::move(parent)} {}

    std::string GetName() const override {
        return name;
    }

    std::size_t GetSize() const override {
        return size;
    }

    bool Resize(std::size_t new_size) override {
        size = new_size;
        return true;
    }

    VirtualDir GetContainingDirectory() const override {
        return parent;
    }

    bool IsWritable() const override {
        return false;
    }

    bool IsReadable() const override {
        return true;
    }

    std::size_t Read(u8* data, std::size_t length, std::size_t offset) const override {
        const auto read = std::min(length, size - offset);
        std::fill(data, data + read, value);
        return read;
    }

    std::size_t Write(const u8* data, std::size_t length, std::size_t offset) override {
        return 0;
    }

    std::optional<u8> ReadByte(std::size_t offset) const override {
        if (offset >= size) {
            return std::nullopt;
        }

        return value;
    }

    std::vector<u8> ReadBytes(std::size_t length, std::size_t offset) const override {
        const auto read = std::min(length, size - offset);
        return std::vector<u8>(read, value);
    }

    bool Rename(std::string_view new_name) override {
        name = new_name;
        return true;
    }

private:
    u8 value;
    std::size_t size;
    std::string name;
    VirtualDir parent;
};

} // namespace FileSys
