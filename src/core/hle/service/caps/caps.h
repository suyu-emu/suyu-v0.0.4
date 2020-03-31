// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::SM {
class ServiceManager;
}

namespace Service::Capture {

enum AlbumImageOrientation {
    Orientation0 = 0,
    Orientation1 = 1,
    Orientation2 = 2,
    Orientation3 = 3,
};

enum AlbumReportOption {
    Disable = 0,
    Enable = 1,
};

enum ContentType : u8 {
    Screenshot = 0,
    Movie = 1,
    ExtraMovie = 3,
};

enum AlbumStorage : u8 {
    NAND = 0,
    SD = 1,
};

struct AlbumFileDateTime {
    u16 year;
    u8 month;
    u8 day;
    u8 hour;
    u8 minute;
    u8 second;
    u8 uid;
};

struct AlbumEntry {
    u64 size;
    u64 application_id;
    AlbumFileDateTime datetime;
    AlbumStorage storage;
    ContentType content;
    u8 padding[6];
};

struct AlbumFileEntry {
    u64 size;
    u64 hash;
    AlbumFileDateTime datetime;
    AlbumStorage storage;
    ContentType content;
    u8 padding[5];
    u8 unknown;
};

struct ApplicationAlbumEntry {
    u64 size;
    u64 hash;
    AlbumFileDateTime datetime;
    AlbumStorage storage;
    ContentType content;
    u8 padding[5];
    u8 unknown;
};

struct ApplicationAlbumFileEntry {
    ApplicationAlbumEntry entry;
    AlbumFileDateTime datetime;
    u64 unknown;
};

/// Registers all Capture services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& sm);

} // namespace Service::Capture
