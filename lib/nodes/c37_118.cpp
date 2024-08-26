/* Node type: C37-118.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2024 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cerrno>
#include <cstdint>
#include <memory>

#include <fmt/core.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <villas/exceptions.hpp>
#include <villas/nodes/c37_118.hpp>
#include <villas/nodes/c37_118/parser.hpp>
#include <villas/socket_addr.hpp>
#include <villas/utils.hpp>

#include "villas/node_direction.hpp"
#include "villas/nodes/c37_118/types.hpp"
#include "villas/tagged_union.hpp"

namespace villas::node::c37_118 {

using namespace std::literals::string_view_literals;

void C37_118::Input::send(Frame::Message message, timespec ts) {
  const std::vector send_buf = parser.serialize(
      {
          .idcode = idcode,
          .soc = static_cast<uint32_t>(ts.tv_sec & 0xFFFFFF),
          .fracsec = static_cast<uint32_t>(
                         ts.tv_nsec * static_cast<int64_t>(config->time_base)) /
                     1'000'000'000,
          .message = message,
      },
      config ? &*config : nullptr);

  ssize_t ret;
  size_t sent = 0;

  do {
    ret =
        ::write(connection_fd, send_buf.data() + sent, send_buf.size() - sent);

    if (ret == 0)
      throw RuntimeError{"end of stream"};

    if (ret < 0)
      throw RuntimeError{"send error {}", ::strerror(errno)};

    sent += ret;
  } while (sent < send_buf.size());
}

std::optional<Frame> C37_118::Input::recv() {
  const int ret = parser.read_with(1500, [&](unsigned char *buf, size_t n) {
    return ::read(connection_fd, buf, n);
  });

  if (ret == 0)
    throw RuntimeError{"end of stream"};

  if (ret < 0)
    throw RuntimeError{"socket error {}", ::strerror(errno)};

  return parser.deserialize(config ? &*config : nullptr);
}

void C37_118::Input::prepare(NodeDirection &in) {
  addrinfo hints{
      .ai_family = AF_UNSPEC,
      .ai_socktype = SOCK_STREAM,
  };

  addrinfo *addrs_ptr;
  std::string service = std::to_string(port);
  if (auto err =
          ::getaddrinfo(addr.c_str(), service.c_str(), &hints, &addrs_ptr))
    throw RuntimeError{"c37.118: getaddrinfo({}:{}) {}", addr, service,
                       ::gai_strerror(err)};

  auto addrs = std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>{addrs_ptr,
                                                                  freeaddrinfo};
  addrinfo *ai;
  for (ai = addrs.get(); ai != nullptr; ai = ai->ai_next) {
    int sock = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

    if (sock < 0)
      continue;

    if (::connect(sock, ai->ai_addr, ai->ai_addrlen) == 0) {
      connection_fd = sock;
      break;
    }

    ::close(sock);
  }

  if (ai == nullptr)
    throw RuntimeError{"could not connect to port {} of {}", port, addr};

  send({in_place_tag<Frame::Type::COMMAND>, Command::Type::GET_CONFIG2});

  for (;;) {
    const auto frame = recv();
    if (!frame.has_value())
      continue;

    const auto *config2 = frame->message.get_if<Frame::Type::CONFIG2>();
    if (config2 == nullptr)
      continue;

    config = *config2;
    break;
  }

  in.signals->clear();
  for (auto const &pmu : config->pmus) {
    in.signals->push_back(
        std::make_shared<Signal>("freq", "Hz", SignalType::FLOAT));
    in.signals->push_back(
        std::make_shared<Signal>("dfreq", "Hz/s", SignalType::FLOAT));

    for (auto const &p : pmu.phinfo)
      in.signals->push_back(std::make_shared<Signal>(
          fmt::format("phasor:{}", p.chnam), std::string{p.unit_str()},
          SignalType::COMPLEX));

    for (auto const &a : pmu.aninfo)
      in.signals->push_back(std::make_shared<Signal>(
          fmt::format("analog:{}", a.chnam), a.unit_str(), SignalType::FLOAT));

    for (auto const &d : pmu.dginfo)
      for (unsigned int i = 0; i < 8 * sizeof(uint16_t); i++) {
        if ((d.dgunit > i) & 1)
          in.signals->push_back(std::make_shared<Signal>(
              fmt::format("digital:{}", d.chnam[i]), "", SignalType::BOOLEAN));
        else
          in.signals->push_back(std::make_shared<Signal>("digital:disabled", "",
                                                         SignalType::BOOLEAN));
      }
  }
}

int C37_118::Input::_read(struct Sample *smps[], unsigned cnt) {
  auto smp = smps[0];

  const auto frame = recv();
  if (!frame.has_value())
    return 0;

  const auto *data = frame->message.get_if<Frame::Type::DATA>();
  if (data == nullptr)
    return 0;

  std::size_t s = 0;
  for (const auto &pmu : data->pmus) {
    if (s < smp->capacity)
      smp->data[s++].f = pmu.freq;

    if (s < smp->capacity)
      smp->data[s++].f = pmu.dfreq;

    for (auto p = pmu.phasor.begin();
         p != pmu.phasor.end() && s < smp->capacity; ++p)
      smp->data[s++].z = *p;

    for (auto a = pmu.analog.begin();
         a != pmu.analog.end() && s < smp->capacity; ++a)
      smp->data[s++].f = *a;

    for (auto d = pmu.digital.begin();
         d != pmu.digital.end() && s < smp->capacity; ++d)
      for (auto i = 0; i < 16 && s < smp->capacity; i++)
        smp->data[s++].b = (*d >> i) & 1;
  }

  smp->length = s;
  smp->ts.origin = {
      .tv_sec = frame->soc,
      .tv_nsec = static_cast<int64_t>(frame->fracsec & 0xFFFFFF) *
                 1'000'000'000 / config->time_base,
  };
  smp->flags = (int)SampleFlags::HAS_DATA | (int)SampleFlags::HAS_TS_ORIGIN;

  return 1;
}

void C37_118::Input::start() {
  send({in_place_tag<Frame::Type::COMMAND>, Command::Type::DATA_START});
}

void C37_118::Input::stop() {
  send({in_place_tag<Frame::Type::COMMAND>, Command::Type::DATA_STOP});
}

int C37_118::Input::getPollFD() const { return connection_fd; }

void C37_118::Output::send(Frame::Message message, timespec ts) {
  const std::vector send_buf = parser.serialize(
      {
          .idcode = idcode,
          .soc = static_cast<uint32_t>(ts.tv_sec & 0xFFFFFF),
          .fracsec = static_cast<uint32_t>(
                         ts.tv_nsec * static_cast<int64_t>(config.time_base)) /
                     1'000'000'000,
          .message = message,
      },
      &config);

  ssize_t ret;
  size_t sent = 0;

  do {
    ret =
        ::write(connection_fd, send_buf.data() + sent, send_buf.size() - sent);
    sent += ret;
  } while (ret > 0 && sent < send_buf.size());

  if (ret == 0)
    throw RuntimeError{"end of stream"};

  if (ret < 0)
    throw RuntimeError{"send error {}", ::strerror(errno)};
}

std::optional<Frame> C37_118::Output::recv() {
  const int ret = parser.read_with(1500, [&](unsigned char *buf, size_t n) {
    return ::read(connection_fd, buf, n);
  });

  if (ret == 0)
    throw RuntimeError{"end of stream"};

  if (ret < 0)
    throw RuntimeError{"socket error {}", ::strerror(errno)};

  return parser.deserialize(&config);
}

void C37_118::Output::prepare(NodeDirection &out) {
  addrinfo hints{
      .ai_flags = AI_PASSIVE,
      .ai_family = AF_UNSPEC,
      .ai_socktype = SOCK_STREAM,
  };

  addrinfo *addrs_ptr;
  std::string service = std::to_string(port);
  if (auto err =
          ::getaddrinfo(addr.c_str(), service.c_str(), &hints, &addrs_ptr);
      err != 0)
    throw RuntimeError{"c37.118: getaddrinfo({}:{}) {}", addr, service,
                       ::gai_strerror(err)};

  auto addrs = std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>{addrs_ptr,
                                                                  freeaddrinfo};
  int sock;
  addrinfo *ai;
  for (ai = addrs.get(); ai != nullptr; ai = ai->ai_next) {
    sock = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

    if (sock < 0)
      continue;

    if (::bind(sock, ai->ai_addr, ai->ai_addrlen) == 0) {
      listener_fd = sock;
      break;
    }

    ::close(sock);
  }

  if (ai == nullptr)
    throw RuntimeError{"could not bind port {} on {}", port, addr};

  if (::listen(listener_fd, 0) != 0)
    throw RuntimeError{"could not listen on {}:{}", addr, port};

  const int ret = ::accept(listener_fd, NULL, NULL);
  if (ret < 0)
    throw RuntimeError{"socket error {}", ::strerror(errno)};

  data_stream = false;
  connection_fd = ret;

  // todo move connection reader to its own thread
  for (;;) {
    const auto frame = recv();
    if (!frame.has_value())
      continue;

    const auto *command = frame->message.get_if<Frame::Type::COMMAND>();
    if (command == nullptr)
      continue;

    switch (command->cmd) {
      using enum Command::Type;
    case GET_CONFIG1:
      send(Frame::Message::make<Frame::Type::CONFIG1>(config));
      continue;
    case GET_CONFIG2:
      send(Frame::Message::make<Frame::Type::CONFIG2>(config));
      continue;
    case DATA_START:
      data_stream = true;
      return;
    default:
      continue;
    }
  }
}

int C37_118::Output::_write(struct Sample *smps[], unsigned cnt) {
  auto smp = smps[0];

  if (not data_stream)
    return 1;

  auto data = Data{};

  std::size_t s = 0;
  for (auto const &pmu_config : config.pmus) {
    auto &pmu_data = data.pmus.emplace_back();

    if (s < smp->length)
      pmu_data.freq = smp->data[s++].f;

    if (s < smp->length)
      pmu_data.dfreq = smp->data[s++].f;

    for (auto p = pmu_config.phinfo.begin();
         p != pmu_config.phinfo.end() and s < smp->length; ++p)
      pmu_data.phasor.push_back(smp->data[s++].z);

    for (auto a = pmu_config.aninfo.begin();
         a != pmu_config.aninfo.end() and s < smp->length; ++a)
      pmu_data.analog.push_back(smp->data[s++].f);

    for (auto d = pmu_config.dginfo.begin();
         d != pmu_config.dginfo.end() and s < smp->length; ++d) {
      auto &dg = pmu_data.digital.emplace_back();
      for (auto i = 0; i < 16 && s < smp->length; i++)
        if (smp->data[s++].b)
          dg |= (1 << i);
    }
  }

  send({in_place_tag<Frame::Type::DATA>, data});

  return 1;
}

void C37_118::Output::start() {}

void C37_118::Output::stop() {}

int C37_118::_read(struct Sample *smps[], unsigned cnt) {
  return input._read(smps, cnt);
}

int C37_118::_write(struct Sample *smps[], unsigned cnt) {
  return output._write(smps, cnt);
}

C37_118::~C37_118() {}

int C37_118::parse(json_t *json) {
  auto object = jansson::Value::borrow(json).object();

  if (auto const in = object.get("in"))
    jansson::unpack(input, *in);

  if (auto const out = object.get("out"))
    jansson::unpack(output, *out);

  return Node::parse(json);
}

int C37_118::prepare() {
  if (in.enabled)
    input.prepare(in);

  if (out.enabled)
    output.prepare(out);

  return Node::prepare();
}

int C37_118::start() {
  int ret = Node::start();
  if (ret)
    return ret;

  if (in.enabled)
    input.start();

  if (out.enabled)
    output.start();

  return 0;
}

int C37_118::stop() {
  int ret = Node::stop();
  if (ret)
    return ret;

  if (in.enabled)
    input.stop();

  if (out.enabled)
    output.stop();

  return 0;
}

std::vector<int> C37_118::getPollFDs() {
  if (in.enabled)
    return {input.getPollFD()};
  else
    return {};
}

// Register node
static char n[] = "c37.118";
static char d[] = "A node for a C37.118 server / client on TCP / UDP";
static NodePlugin<C37_118, n, d,
                  (int)NodeFactory::Flags::PROVIDES_SIGNALS |
                      (int)NodeFactory::Flags::SUPPORTS_READ |
                      (int)NodeFactory::Flags::SUPPORTS_WRITE |
                      (int)NodeFactory::Flags::SUPPORTS_POLL>
    p;

} // namespace villas::node::c37_118
