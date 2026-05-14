#pragma once

#include <cstdint>
#include <string>

namespace monitor {

struct SnapshotSessionInfo {
  std::string session_id;
  std::string label;
  std::uint64_t started_unix_ms = 0;
};

std::string make_session_id(std::uint64_t started_unix_ms);
std::string session_start_json(const SnapshotSessionInfo& session);
std::string session_snapshot_json(const std::string& snapshot_json);
std::string session_end_json(const SnapshotSessionInfo& session, std::uint64_t ended_unix_ms);

}  // namespace monitor
