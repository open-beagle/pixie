#pragma once

#include <chrono>
#include <string>

namespace pl {
namespace stirling {
namespace mysql {

/**
 * Raw MySQLPacket from MySQL Parser
 */
struct Packet {
  uint64_t timestamp_ns;
  std::chrono::time_point<std::chrono::steady_clock> creation_timestamp;

  uint8_t sequence_id;
  // TODO(oazizi): Convert to std::basic_string<uint8_t>.
  std::string msg;

  size_t ByteSize() const { return sizeof(Packet) + msg.size(); }
};

}  // namespace mysql
}  // namespace stirling
}  // namespace pl
