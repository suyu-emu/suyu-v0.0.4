#pragma once

#include <array>
#include <memory>
#include <sys/types.h>
#include <vector>

namespace Tzif {

typedef struct {
  char magic[4];
  u_int8_t version;
  u_int8_t reserved[15];
  u_int32_t isutcnt;
  u_int32_t isstdcnt;
  u_int32_t leapcnt;
  u_int32_t timecnt;
  u_int32_t typecnt;
  u_int32_t charcnt;
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
  u_int32_t utoff;
  u_int8_t dst;
  u_int8_t idx;
} TimeTypeRecord;
#pragma pack(pop)
static_assert(sizeof(TimeTypeRecord) == 0x6);

class Data {
public:
  explicit Data() = default;
  virtual ~Data() = default;

  virtual void ReformatNintendo(std::vector<u_int8_t> &buffer) const = 0;
};

class DataImpl : public Data {
public:
  explicit DataImpl() = default;
  ~DataImpl() override = default;

  void ReformatNintendo(std::vector<u_int8_t> &buffer) const override;

  Header header;
  Footer footer;

  std::unique_ptr<int64_t[]> transition_times;
  std::unique_ptr<u_int8_t[]> transition_types;
  std::unique_ptr<TimeTypeRecord[]> local_time_type_records;
  std::unique_ptr<int8_t[]> time_zone_designations;
  std::unique_ptr<u_int8_t[]> standard_indicators;
  std::unique_ptr<u_int8_t[]> ut_indicators;
};

std::unique_ptr<DataImpl> ReadData(const u_int8_t *data, std::size_t size);

} // namespace Tzif
