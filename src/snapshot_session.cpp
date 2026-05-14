#include "monitor/output/snapshot_session.hpp"

#include "monitor/snapshot_json.hpp"

#include <cstdio>

namespace monitor {

std::string make_session_id(std::uint64_t started_unix_ms) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "mtop-%llu",
                static_cast<unsigned long long>(started_unix_ms));
  return buffer;
}

std::string session_start_json(const SnapshotSessionInfo& session) {
  return std::string("{\"event\":\"session_start\",\"schema_version\":2,\"session_id\":\"") +
         json_escape(session.session_id) +
         "\",\"label\":\"" + json_escape(session.label) +
         "\",\"started_unix_ms\":" + std::to_string(session.started_unix_ms) + "}";
}

std::string session_snapshot_json(const std::string& snapshot_json) {
  if (snapshot_json.empty() || snapshot_json.front() != '{') {
    return "{\"event\":\"snapshot\",\"schema_version\":2,\"snapshot\":null}";
  }
  return std::string("{\"event\":\"snapshot\",\"schema_version\":2,\"snapshot\":") +
         snapshot_json + "}";
}

std::string session_end_json(const SnapshotSessionInfo& session, std::uint64_t ended_unix_ms) {
  const std::uint64_t duration_ms = ended_unix_ms >= session.started_unix_ms
                                        ? ended_unix_ms - session.started_unix_ms
                                        : 0;
  return std::string("{\"event\":\"session_end\",\"schema_version\":2,\"session_id\":\"") +
         json_escape(session.session_id) +
         "\",\"ended_unix_ms\":" + std::to_string(ended_unix_ms) +
         ",\"duration_ms\":" + std::to_string(duration_ms) + "}";
}

}  // namespace monitor
