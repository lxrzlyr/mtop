#include <cassert>
#include <algorithm>
#include <string>
#include <vector>

#include "monitor/model/workload_model.hpp"

namespace {

monitor::ProcessSnapshot process(int pid,
                                 int ppid,
                                 std::string name,
                                 std::string command,
                                 double cpu,
                                 std::uint64_t rss) {
  monitor::ProcessSnapshot snapshot;
  snapshot.pid = pid;
  snapshot.parent_pid = ppid;
  snapshot.name = std::move(name);
  snapshot.command = std::move(command);
  snapshot.cpu_percent = cpu;
  snapshot.memory_percent = 1.0;
  snapshot.resident_bytes = rss;
  snapshot.virtual_bytes = rss * 2;
  snapshot.power = "12";
  snapshot.io = "1M/2M";
  snapshot.core_mix = "P:80% E:20%";
  return snapshot;
}

}  // namespace

int main() {
  monitor::SystemSnapshot snapshot;
  snapshot.processes.push_back(process(100, 1, "ollama", "ollama serve", 5.0, 100));
  snapshot.processes.push_back(process(101, 100, "ollama", "ollama runner --model llama3.1:8b", 40.0, 900));
  snapshot.processes.push_back(process(200, 1, "llama-server", "llama-server --model tiny.gguf", 10.0, 500));
  snapshot.processes.push_back(process(300, 1, "Safari", "Safari", 1.0, 50));

  const std::vector<monitor::WorkloadSnapshot> workloads = monitor::build_workloads(snapshot);
  assert(workloads.size() == 2);
  const std::vector<monitor::AiProcessRow> alpha_rows = monitor::build_ai_process_rows(snapshot);
  assert(alpha_rows.size() == 4);
  assert(alpha_rows[0].is_ai);
  assert(alpha_rows[1].is_ai);
  assert(alpha_rows[2].is_ai);
  assert(!alpha_rows.back().is_ai);
  assert(alpha_rows.back().process->name == "Safari");

  const auto ollama = std::find_if(workloads.begin(), workloads.end(), [](const auto& workload) {
    return workload.kind == monitor::WorkloadKind::Ollama;
  });
  assert(ollama != workloads.end());
  assert(ollama->processes.size() == 2);
  assert(ollama->cpu_percent == 45.0);
  assert(ollama->resident_bytes == 1000);

  const auto llama = std::find_if(workloads.begin(), workloads.end(), [](const auto& workload) {
    return workload.kind == monitor::WorkloadKind::LlamaCpp;
  });
  assert(llama != workloads.end());
  assert(llama->processes.size() == 1);
  assert(llama->model_hint.find("tiny.gguf") != std::string::npos);

  monitor::SystemSnapshot no_ai;
  no_ai.processes.push_back(process(300, 1, "Safari", "Safari", 1.0, 50));
  no_ai.processes.push_back(process(301, 1, "Finder", "Finder", 0.5, 40));
  const std::vector<monitor::AiProcessRow> no_ai_rows = monitor::build_ai_process_rows(no_ai);
  assert(no_ai_rows.size() == 2);
  assert(!no_ai_rows[0].is_ai);
  assert(!no_ai_rows[1].is_ai);
  return 0;
}
