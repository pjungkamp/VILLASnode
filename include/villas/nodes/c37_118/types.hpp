/* Node type: C37-118.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2024-2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <array>
#include <complex>
#include <cstdint>
#include <ctime>
#include <vector>

#include <villas/jansson.hpp>
#include <villas/bytes.hpp>
#include <villas/tagged_union.hpp>

namespace villas::node::c37_118 {

struct TimeQuality {
  enum class IndicatorCode : std::uint8_t {
    LOCKED = 0x0,
    UNLOCKED_ACCURACY_NANOS_1 = 0x1,
    UNLOCKED_ACCURACY_NANOS_10 = 0x2,
    UNLOCKED_ACCURACY_NANOS_100 = 0x3,
    UNLOCKED_ACCURACY_MICROS_1 = 0x4,
    UNLOCKED_ACCURACY_MICROS_10 = 0x5,
    UNLOCKED_ACCURACY_MICROS_100 = 0x6,
    UNLOCKED_ACCURACY_MILLIS_1 = 0x7,
    UNLOCKED_ACCURACY_MILLIS_10 = 0x8,
    UNLOCKED_ACCURACY_MILLIS_100 = 0x9,
    UNLOCKED_ACCURACY_SECS_1 = 0xA,
    UNLOCKED_ACCURACY_SECS_10 = 0xB,
    /* invalid */
    FAILED = 0xF,
  };

  enum class LeapSecondDirection : std::uint8_t {
    ADD = 0x0,
    DELETE = 0x1,
  };

  IndicatorCode indicator_code : 4;
  std::uint8_t leap_second_pending : 1;
  std::uint8_t leap_second_occured : 1;
  LeapSecondDirection leap_second_direction : 1;
  std::uint8_t : 1;

  friend void toBytes(bytes::Buffer &bytes, TimeQuality const &tq) {
    bytes.add(std::bit_cast<std::byte>(tq));
  }

  friend void fromBytes(bytes::Cursor &bytes, TimeQuality &tq) {
    tq = std::bit_cast<TimeQuality>(bytes.take<std::byte>());
  }
};

static_assert(requires(std::uint8_t time_quality) {
  std::bit_cast<TimeQuality>(time_quality);
});

struct PmuData final {
  uint16_t stat;
  std::vector<std::complex<float>> phasor;
  float freq;
  float dfreq;
  std::vector<float> analog;
  std::vector<uint16_t> digital;

  friend jansson::Value jsonPack(PmuData const &value) {
    return jansson::Object::pack(jansson::required("stat", value.stat),
                                 jansson::required("freq", value.freq),
                                 jansson::required("dfreq", value.dfreq),
                                 jansson::optional("phasor", value.phasor),
                                 jansson::optional("analog", value.analog),
                                 jansson::optional("digital", value.digital));
  }
};

struct Data final {
  std::vector<PmuData> pmus;

  friend jansson::Value jsonPack(Data const &value) {
    return jansson::pack(value.pmus);
  }
};

struct Header final {
  std::string data;

  friend jansson::Value jsonPack(Header const &value) {
    return jansson::pack(value.data);
  }
};

struct PhasorInfo final {
  static constexpr uint8_t UNIT_VOLT = 0;
  static constexpr uint8_t UNIT_AMPERE = 1;

  std::string chnam;
  uint32_t phunit;

  uint8_t unit() const noexcept { return phunit >> 24; }

  std::string_view unit_str() const noexcept {
    using namespace std::string_view_literals;

    switch (unit()) {
    case UNIT_VOLT:
      return "volt"sv;
    case UNIT_AMPERE:
      return "ampere"sv;
    default:
      return "other"sv;
    }
  }

  float scale() const noexcept {
    return static_cast<float>(phunit & 0xFFFFFF) / 100'000;
  }

  friend void jsonUnpack(PhasorInfo &phasor_info, jansson::Value const &json) {
    json.object().unpack(jansson::required("chnam", phasor_info.chnam),
                         jansson::required("phunit", phasor_info.phunit));
  }

  friend jansson::Value jsonPack(PhasorInfo const &value) {
    return jansson::Object::pack(jansson::required("chnam", value.chnam),
                                 jansson::required("phunit", value.phunit));
  }
};

struct AnalogInfo final {
  static constexpr uint8_t UNIT_POINT_ON_WAVE = 0;
  static constexpr uint8_t UNIT_RMS = 1;
  static constexpr uint8_t UNIT_PEAK = 2;

  std::string chnam;
  uint32_t anunit;

  uint8_t unit() const noexcept { return anunit >> 24; }

  const char *unit_str() const noexcept {
    switch (unit()) {
    case UNIT_POINT_ON_WAVE:
      return "point-on-wave";
    case UNIT_RMS:
      return "rms";
    case UNIT_PEAK:
      return "peak";
    default:
      return "<unknown>";
    }
  }

  float scale() const noexcept { return static_cast<float>(anunit & 0xFFFFFF); }

  friend void jsonUnpack(AnalogInfo &analog_info, jansson::Value const &json) {
    json.object().unpack(jansson::required("chnam", analog_info.chnam),
                         jansson::required("anunit", analog_info.anunit));
  }

  friend jansson::Value jsonPack(AnalogInfo const &value) {
    return jansson::Object::pack(jansson::required("chnam", value.chnam),
                                 jansson::required("anunit", value.anunit));
  }
};

struct DigitalInfo final {
  std::array<std::string, 16> chnam;
  uint32_t dgunit;

  friend void jsonUnpack(DigitalInfo &digital_info,
                         jansson::Value const &json) {
    json.object().unpack(jansson::required("chnam", digital_info.chnam),
                         jansson::required("dgunit", digital_info.dgunit));
  }

  friend jansson::Value jsonPack(DigitalInfo const &value) {
    auto chnam_array = jansson::Array{};
    for (auto const &chnam : value.chnam)
      chnam_array.append(jansson::string(chnam));

    return jansson::Object::pack(
        jansson::required("chnam", std::move(chnam_array)),
        jansson::required("dgunit", value.dgunit));
  }
};

struct PmuConfig final {
  std::string stn;
  uint16_t idcode;
  uint16_t format;
  std::vector<PhasorInfo> phinfo;
  std::vector<AnalogInfo> aninfo;
  std::vector<DigitalInfo> dginfo;
  uint16_t fnom;
  uint16_t cfgcnt;

  friend void jsonUnpack(PmuConfig &pmu_config, jansson::Value const &json) {
    json.object().unpack(jansson::required("stn", pmu_config.stn),
                         jansson::required("idcode", pmu_config.idcode),
                         jansson::required("format", pmu_config.format),
                         jansson::optional("phinfo", pmu_config.phinfo),
                         jansson::optional("aninfo", pmu_config.aninfo),
                         jansson::optional("dginfo", pmu_config.dginfo),
                         jansson::required("fnom", pmu_config.fnom),
                         jansson::required("cfgcnt", pmu_config.cfgcnt));
  }

  friend jansson::Value jsonPack(PmuConfig const &value) {
    return jansson::Object::pack(jansson::required("stn", value.stn),
                                 jansson::required("idcode", value.idcode),
                                 jansson::required("format", value.format),
                                 jansson::optional("phinfo", value.phinfo),
                                 jansson::optional("aninfo", value.aninfo),
                                 jansson::optional("dginfo", value.dginfo),
                                 jansson::required("fnom", value.fnom),
                                 jansson::required("cfgcnt", value.cfgcnt));
  }
};

struct Config {
  uint32_t time_base;
  std::vector<PmuConfig> pmus;
  uint16_t data_rate;

  friend void jsonUnpack(Config &config, jansson::Value const &json) {
    json.object().unpack(jansson::required("time_base", config.time_base),
                         jansson::required("pmus", config.pmus),
                         jansson::required("data_rate", config.data_rate));
  }

  friend jansson::Value jsonPack(Config const &value) {
    return jansson::Object::pack(
        jansson::required("time_base", value.time_base),
        jansson::required("pmus", value.pmus),
        jansson::required("data_rate", value.data_rate));
  }
};

struct Command final {
  enum class Type : uint16_t {
    DATA_STOP = 0x1,
    DATA_START = 0x2,
    GET_HEADER = 0x3,
    GET_CONFIG1 = 0x4,
    GET_CONFIG2 = 0x5,
    // GET_CONFIG3 = 0X6;
  };

  Type cmd;
  std::vector<unsigned char> ext;

  const char *typeToString() const noexcept {
    switch (cmd) {
      using enum Type;
    case DATA_START:
      return "DATA_START";
    case DATA_STOP:
      return "DATA_STOP";
    case GET_HEADER:
      return "GET_HEADER";
    case GET_CONFIG1:
      return "GET_CONFIG1";
    case GET_CONFIG2:
      return "GET_CONFIG2";
    //case get_config3:
    //  return "get_config3";
    default:
      return "<invalid>";
    }
  }

  friend jansson::Value jsonPack(Command const &value) {
    return jansson::string(value.typeToString());
  }
};

enum class VersionTag {
  C37_118_2__2003,
  C37_118_2__2011,
  C37_118_2__2024,
  npos,
};

struct Frame final {
  enum class Type { DATA, HEADER, CONFIG1, CONFIG2, COMMAND, npos };
  using Message =
      tagged_union<Type::npos, Data, Header, Config, Config, Command>;

  uint16_t version;
  uint16_t framesize;
  uint16_t idcode;
  uint32_t soc;
  uint32_t fracsec;
  Message message;

  static const char *typeString(Message const &message) noexcept {
    switch (message) {
      using enum Type;
    case DATA:
      return "data";
    case HEADER:
      return "header";
    case CONFIG1:
      return "config1";
    case CONFIG2:
      return "config2";
    case COMMAND:
      return "command";
    default:
      return "<invalid>";
    }
  }

  friend jansson::Value jsonPack(Frame const &value) {
    return jansson::Object::pack(
        jansson::required("version", value.version),
        jansson::required("idcode", value.idcode),
        jansson::required("soc", value.soc),
        jansson::required("fracsec", value.fracsec),
        jansson::required(typeString(value.message),
                          value.message.visit(jansson::pack)));
  }
};

} // namespace villas::node::c37_118
