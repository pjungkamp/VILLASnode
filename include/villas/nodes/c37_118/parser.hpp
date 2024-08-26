/* Parser for C37-118.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2024 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstring>
#include <optional>
#include <string>

#include <villas/exceptions.hpp>
#include <villas/nodes/c37_118/types.hpp>

namespace villas::node::c37_118 {

class Parser {
public:
  Parser() = default;
  Parser(unsigned char *buf, size_t count) : de_buffer(buf, buf + count) {}

  std::optional<Frame> deserialize(const Config *config);

  std::vector<unsigned char> serialize(const Frame &frame,
                                       const Config *config);

  template <typename F> int read_with(size_t count, F func) {
    std::copy(de_buffer.begin() + de_parsed, de_buffer.end(),
              de_buffer.begin());
    de_buffer.resize(de_buffer.size() + count - de_parsed);

    const ssize_t ret = func(&*de_buffer.end() - count, count);
    if (ret > 0)
      de_buffer.resize(de_buffer.size() - count + ret);
    else
      de_buffer.resize(de_buffer.size() - count);

    return ret;
  }

private:
  template <typename T> void de_copy(T *value, size_t count = sizeof(T)) {
    if (de_cursor + count > de_end)
      throw RuntimeError{"c37_118: broken frame"};

    std::memcpy((void *)value, &de_buffer[de_cursor], count);
    de_cursor += count;
  }

  template <typename T>
  void se_copy(const T *value, std::size_t count = sizeof(T)) {
    auto index = se_buffer.size();
    se_buffer.insert(se_buffer.end(), count, 0);
    std::memcpy(se_buffer.data() + index, (const void *)value, count);
  }

  uint16_t deserialize_uint16_t();
  uint32_t deserialize_uint32_t();
  int16_t deserialize_int16_t();
  float deserialize_float();
  std::string deserialize_name1();
  std::complex<float> deserialize_phasor(uint16_t format,
                                         const PhasorInfo &info);
  float deserialize_freq(const PmuConfig &pmu);
  float deserialize_dfreq(uint16_t format);
  float deserialize_analog(uint16_t format, const AnalogInfo &aninfo);
  uint16_t deserialize_digital(const DigitalInfo &dginfo);
  PmuData deserialize_pmu_data(const PmuConfig &pmu_config);
  PmuConfig deserialize_pmu_config();
  Config deserialize_config();
  Data deserialize_data(const Config &config);
  Header deserialize_header();
  Command deserialize_command();
  std::optional<Frame> try_deserialize_frame(const Config *config);

  void serialize_uint16_t(const uint16_t &value);
  void serialize_uint32_t(const uint32_t &value);
  void serialize_int16_t(const int16_t &value);
  void serialize_float(const float &value);
  void serialize_name1(const std::string &value);
  void serialize_phasor(const std::complex<float> &value, uint16_t format,
                        const PhasorInfo &phinfo);
  void serialize_freq(const float &value, const PmuConfig &pmu);
  void serialize_dfreq(const float &value, uint16_t format);
  void serialize_analog(const float &value, uint16_t format,
                        const AnalogInfo &aninfo);
  void serialize_digital(const uint16_t &value, const DigitalInfo &dginfo);
  void serialize_pmu_data(const PmuData &value, const PmuConfig &pmu_config);
  void serialize_pmu_config(const PmuConfig &value);
  void serialize_config(const Config &value);
  void serialize_data(const Data &value, const Config &config);
  void serialize_header(const Header &value);
  void serialize_command(const Command &value);
  void serialize_frame(const Frame &value, const Config *config);

  size_t de_cursor;
  size_t de_end;
  size_t de_parsed;
  std::vector<unsigned char> se_buffer;
  std::vector<unsigned char> de_buffer;
};

uint16_t calculate_crc(const unsigned char *frame, uint16_t size);

} // namespace villas::node::c37_118
