// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <memory>
#include <cstdint>
#include <vector>

namespace Tzif {

typedef struct {
  char magic[4];
  std::uint8_t version;
  std::uint8_t reserved[15];
  std::uint32_t isutcnt;
  std::uint32_t isstdcnt;
  std::uint32_t leapcnt;
  std::uint32_t timecnt;
  std::uint32_t typecnt;
  std::uint32_t charcnt;
} Header;
static_assert(sizeof(Header) == 0x2c);

class Footer {
public:
  explicit Footer() = default;
  ~Footer() = default;

  const char nl_a{'\n'};
  std::unique_ptr<char[]> tz_string;
  const char nl_b{'\n'};

  std::size_t footer_string_length;
};

#pragma pack(push, 1)
typedef struct {
  std::uint32_t utoff;
  std::uint8_t dst;
  std::uint8_t idx;
} TimeTypeRecord;
#pragma pack(pop)
static_assert(sizeof(TimeTypeRecord) == 0x6);

class Data {
public:
  explicit Data() = default;
  virtual ~Data() = default;

  virtual void ReformatNintendo(std::vector<std::uint8_t> &buffer) const = 0;
};

class DataImpl : public Data {
public:
  explicit DataImpl() = default;
  ~DataImpl() override = default;

  void ReformatNintendo(std::vector<std::uint8_t> &buffer) const override;

  Header header;
  Footer footer;

  std::unique_ptr<int64_t[]> transition_times;
  std::unique_ptr<std::uint8_t[]> transition_types;
  std::unique_ptr<TimeTypeRecord[]> local_time_type_records;
  std::unique_ptr<int8_t[]> time_zone_designations;
  std::unique_ptr<std::uint8_t[]> standard_indicators;
  std::unique_ptr<std::uint8_t[]> ut_indicators;
};

std::unique_ptr<DataImpl> ReadData(const std::uint8_t *data, std::size_t size);

} // namespace Tzif
