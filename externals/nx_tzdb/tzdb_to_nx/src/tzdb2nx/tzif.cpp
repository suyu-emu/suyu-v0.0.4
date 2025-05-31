#include "tzif.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <sys/types.h>

namespace Tzif {

static std::size_t SkipToVersion2(const u_int8_t *data, std::size_t size) {
  char magic[5];
  const u_int8_t *p{data};

  std::memcpy(magic, data, 4);
  magic[4] = '\0';

  if (std::strcmp(magic, "TZif") != 0) {
    return -1;
  }

  do {
    p++;
    if (p >= data + size) {
      return -1;
    }
  } while (std::strncmp(reinterpret_cast<const char *>(p), "TZif", 4) != 0);

  return p - data;
}

template <typename Type> constexpr static void SwapEndianess(Type *value) {
  u_int8_t *data = reinterpret_cast<u_int8_t *>(value);

  union {
    u_int8_t data[sizeof(Type)];
    Type value;
  } temp;

  for (u_int32_t i = 0; i < sizeof(Type); i++) {
    u_int32_t alt_index = sizeof(Type) - i - 1;
    temp.data[alt_index] = data[i];
  }

  *value = temp.value;
}

static void FlipHeader(Header &header) {
  SwapEndianess(&header.isutcnt);
  SwapEndianess(&header.isstdcnt);
  SwapEndianess(&header.leapcnt);
  SwapEndianess(&header.timecnt);
  SwapEndianess(&header.typecnt);
  SwapEndianess(&header.charcnt);
}

std::unique_ptr<DataImpl> ReadData(const u_int8_t *data, std::size_t size) {
  const std::size_t v2_offset = SkipToVersion2(data, size);
  if (v2_offset == static_cast<std::size_t>(-1)) {
    return nullptr;
  }

  const u_int8_t *p = data + v2_offset;

  Header header;
  std::memcpy(&header, p, sizeof(header));
  p += sizeof(header);

  FlipHeader(header);

  const std::size_t data_block_length =
      header.timecnt * sizeof(int64_t) + header.timecnt * sizeof(u_int8_t) +
      header.typecnt * sizeof(TimeTypeRecord) +
      header.charcnt * sizeof(int8_t) + header.isstdcnt * sizeof(u_int8_t) +
      header.isutcnt * sizeof(u_int8_t);

  if (v2_offset + data_block_length + sizeof(Header) > size) {
    return nullptr;
  }

  std::unique_ptr<DataImpl> impl = std::make_unique<DataImpl>();
  impl->header = header;

  const auto copy =
      []<typename Type>(std::unique_ptr<Type[]> &array, int length,
                        const u_int8_t *const &ptr) -> const u_int8_t * {
    const std::size_t region_length = length * sizeof(Type);
    array = std::make_unique<Type[]>(length);
    std::memcpy(array.get(), ptr, region_length);
    return ptr + region_length;
  };

  p = copy(impl->transition_times, header.timecnt, p);
  p = copy(impl->transition_types, header.timecnt, p);
  p = copy(impl->local_time_type_records, header.typecnt, p);
  p = copy(impl->time_zone_designations, header.charcnt, p);
  p = copy(impl->standard_indicators, header.isstdcnt, p);
  p = copy(impl->ut_indicators, header.isutcnt, p);

  const std::size_t footer_string_length = data + size - p - 2;
  p++;

  if (p + footer_string_length > data + size ||
      p + footer_string_length < data) {
    return nullptr;
  }

  impl->footer.tz_string = std::make_unique<char[]>(footer_string_length);
  std::memcpy(impl->footer.tz_string.get(), p, footer_string_length);
  impl->footer.footer_string_length = footer_string_length;

  return impl;
}

static void PushToBuffer(std::vector<u_int8_t> &buffer, const void *data,
                         std::size_t size) {
  const u_int8_t *p{reinterpret_cast<const u_int8_t *>(data)};
  for (std::size_t i = 0; i < size; i++) {
    buffer.push_back(*p);
    p++;
  }
}

void DataImpl::ReformatNintendo(std::vector<u_int8_t> &buffer) const {
  buffer.clear();

  Header header_copy{header};
  header_copy.isstdcnt = 0;
  header_copy.isutcnt = 0;
  FlipHeader(header_copy);

  PushToBuffer(buffer, &header_copy, sizeof(Header));
  PushToBuffer(buffer, transition_times.get(),
               header.timecnt * sizeof(int64_t));
  PushToBuffer(buffer, transition_types.get(),
               header.timecnt * sizeof(u_int8_t));
  PushToBuffer(buffer, local_time_type_records.get(),
               header.typecnt * sizeof(TimeTypeRecord));
  PushToBuffer(buffer, time_zone_designations.get(),
               header.charcnt * sizeof(int8_t));
  // omit standard_indicators
  // omit ut_indicators
  PushToBuffer(buffer, &footer.nl_a, 1);
  PushToBuffer(buffer, footer.tz_string.get(), footer.footer_string_length);
  PushToBuffer(buffer, &footer.nl_b, 1);
}

} // namespace Tzif
