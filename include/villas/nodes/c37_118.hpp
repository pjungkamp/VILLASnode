/* Node type: C37-118.
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2024 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <villas/jansson.hpp>
#include <villas/node.hpp>
#include <villas/node/config.hpp>
#include <villas/nodes/c37_118/parser.hpp>
#include <villas/queue_signalled.h>
#include <villas/signal.hpp>
#include <villas/signal_list.hpp>
#include <villas/socket_addr.hpp>
#include <villas/timing.hpp>

namespace villas::node::c37_118 {

class C37_118 final : public Node {
private:
  struct Input final {
    std::string addr;
    uint16_t port;
    uint16_t idcode;

    friend void jsonUnpack(Input &output, jansson::Value const &json) {
      json.object().unpack(jansson::bind("address", output.addr),
                           jansson::bind("port", output.port),
                           jansson::bind("idcode", output.idcode));
    }

    std::optional<Config> config;
    int connection_fd;
    Parser parser;

    void send(Frame::Message message, timespec ts = time_now());
    std::optional<Frame> recv();

    void prepare(NodeDirection &in);
    int _read(struct Sample *smps[], unsigned cnt);
    void start();
    void stop();
    int getPollFD() const;
  } input;

  struct Output final {
    std::string addr;
    uint16_t port;
    uint16_t idcode;
    Config config;

    friend void jsonUnpack(Output &output, jansson::Value const &json) {
      json.object().unpack(jansson::bind("address", output.addr),
                           jansson::bind("port", output.port),
                           jansson::bind("idcode", output.idcode),
                           jansson::bind("config", output.config));
    }

    bool data_stream;
    int listener_fd;
    int connection_fd;
    Parser parser;

    void send(Frame::Message message, timespec ts = time_now());
    std::optional<Frame> recv();

    void prepare(NodeDirection &out);
    int _write(struct Sample *smps[], unsigned cnt);
    void start();
    void stop();
  } output;

  virtual int _read(struct Sample *smps[], unsigned cnt) override;
  virtual int _write(struct Sample *smps[], unsigned cnt) override;

public:
  C37_118(const uuid_t &id = {}, const std::string &name = "")
      : Node{id, name} {}

  virtual ~C37_118() override;
  virtual int parse(json_t *json) override;
  virtual int prepare() override;
  virtual int start() override;
  virtual int stop() override;
  virtual std::vector<int> getPollFDs() override;
};

} // namespace villas::node::c37_118
