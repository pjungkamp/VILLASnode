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
#include <vector>

#include <villas/jansson.hpp>
#include <villas/tagged_union.hpp>

namespace villas::node::c37_118 {

struct PmuData final {
  uint16_t stat;
  std::vector<std::complex<float>> phasor;
  float freq;
  float dfreq;
  std::vector<float> analog;
  std::vector<uint16_t> digital;

  friend jansson::Value jsonPack(PmuData const &value) {
    return jansson::Object::pack(jansson::bind("stat", value.stat),
                                 jansson::bind("phasor", value.phasor),
                                 jansson::bind("freq", value.freq),
                                 jansson::bind("dfreq", value.dfreq),
                                 jansson::bind("analog", value.analog),
                                 jansson::bind("digital", value.digital));
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
    json.object().unpack(jansson::bind("chnam", phasor_info.chnam),
                         jansson::bind("phunit", phasor_info.phunit));
  }

  friend jansson::Value jsonPack(PhasorInfo const &value) {
    return jansson::Object::pack(jansson::bind("chnam", value.chnam),
                                 jansson::bind("phunit", value.phunit));
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
    json.object().unpack(jansson::bind("chnam", analog_info.chnam),
                         jansson::bind("anunit", analog_info.anunit));
  }

  friend jansson::Value jsonPack(AnalogInfo const &value) {
    return jansson::Object::pack(jansson::bind("chnam", value.chnam),
                                 jansson::bind("anunit", value.anunit));
  }
};

struct DigitalInfo final {
  std::array<std::string, 16> chnam;
  uint32_t dgunit;

  friend void jsonUnpack(DigitalInfo &digital_info,
                         jansson::Value const &json) {
    json.object().unpack(jansson::bind("chnam", digital_info.chnam),
                         jansson::bind("dgunit", digital_info.dgunit));
  }

  friend jansson::Value jsonPack(DigitalInfo const &value) {
    auto chnam_array = jansson::Array{};
    for (auto const &chnam : value.chnam)
      chnam_array.append(jansson::string(chnam));

    return jansson::Object::pack(jansson::bind("chnam", std::move(chnam_array)),
                                 jansson::bind("dgunit", value.dgunit));
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
    json.object().unpack(jansson::bind("stn", pmu_config.stn),
                         jansson::bind("idcode", pmu_config.idcode),
                         jansson::bind("format", pmu_config.format),
                         jansson::bind("phinfo", pmu_config.phinfo),
                         jansson::bind("aninfo", pmu_config.aninfo),
                         jansson::bind("dginfo", pmu_config.dginfo),
                         jansson::bind("fnom", pmu_config.fnom),
                         jansson::bind("cfgcnt", pmu_config.cfgcnt));
  }

  friend jansson::Value jsonPack(PmuConfig const &value) {
    return jansson::Object::pack(jansson::bind("stn", value.stn),
                                 jansson::bind("idcode", value.idcode),
                                 jansson::bind("format", value.format),
                                 jansson::bind("phinfo", value.phinfo),
                                 jansson::bind("aninfo", value.aninfo),
                                 jansson::bind("dginfo", value.dginfo),
                                 jansson::bind("fnom", value.fnom),
                                 jansson::bind("cfgcnt", value.cfgcnt));
  }
};

struct Config {
  uint32_t time_base;
  std::vector<PmuConfig> pmus;
  uint16_t data_rate;

  friend void jsonUnpack(Config &config, jansson::Value const &json) {
    json.object().unpack(jansson::bind("time_base", config.time_base),
                         jansson::bind("pmus", config.pmus),
                         jansson::bind("data_rate", config.data_rate));
  }

  friend jansson::Value jsonPack(Config const &value) {
    return jansson::Object::pack(jansson::bind("time_base", value.time_base),
                                 jansson::bind("pmus", value.pmus),
                                 jansson::bind("data_rate", value.data_rate));
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
        jansson::bind("version", value.version),
        jansson::bind("idcode", value.idcode), jansson::bind("soc", value.soc),
        jansson::bind("fracsec", value.fracsec),
        jansson::bind(typeString(value.message),
                      value.message.visit(jansson::pack)));
  }
};

} // namespace villas::node::c37_118
