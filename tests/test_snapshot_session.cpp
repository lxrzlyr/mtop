#include <cassert>
#include <string>

#include "monitor/output/snapshot_session.hpp"

int main() {
  monitor::SnapshotSessionInfo session;
  session.started_unix_ms = 1000;
  session.session_id = monitor::make_session_id(session.started_unix_ms);
  session.label = "llama \"bench\"";

  const std::string start = monitor::session_start_json(session);
  const std::string snapshot = monitor::session_snapshot_json("{\"schema_version\":2}");
  const std::string end = monitor::session_end_json(session, 1450);

  assert(start.find("\"event\":\"session_start\"") != std::string::npos);
  assert(start.find("\"label\":\"llama \\\"bench\\\"\"") != std::string::npos);
  assert(snapshot.find("\"event\":\"snapshot\"") != std::string::npos);
  assert(snapshot.find("\"snapshot\":{\"schema_version\":2}") != std::string::npos);
  assert(end.find("\"event\":\"session_end\"") != std::string::npos);
  assert(end.find("\"duration_ms\":450") != std::string::npos);
  return 0;
}
