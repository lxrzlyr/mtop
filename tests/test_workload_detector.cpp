#include <cassert>
#include <string>

#include "monitor/model/workload_detector.hpp"

namespace {

monitor::ProcessSnapshot process(std::string name, std::string command) {
  monitor::ProcessSnapshot snapshot;
  snapshot.name = std::move(name);
  snapshot.command = std::move(command);
  return snapshot;
}

void expect_kind(const monitor::ProcessSnapshot& process, monitor::WorkloadKind kind) {
  const monitor::WorkloadDetection detection = monitor::detect_workload_process(process);
  assert(detection.kind == kind);
  assert(detection.confidence >= 0.60);
  assert(!detection.reason.empty());
}

}  // namespace

int main() {
  expect_kind(process("ollama", "ollama serve"), monitor::WorkloadKind::Ollama);
  expect_kind(process("llama-server", "llama-server --model /models/tiny.gguf"), monitor::WorkloadKind::LlamaCpp);
  expect_kind(process("mlx-lm", "python -m mlx_lm.server --model mistral"), monitor::WorkloadKind::Mlx);
  expect_kind(process("uvicorn", "uvicorn app:api --workers 1"), monitor::WorkloadKind::PythonInference);
  expect_kind(process("LM Studio", "/Applications/LM Studio.app/Contents/MacOS/LM Studio"),
              monitor::WorkloadKind::LmStudio);
  expect_kind(process("python", "/srv/ComfyUI/main.py --ckpt flux.safetensors"),
              monitor::WorkloadKind::ComfyUi);
  expect_kind(process("runner", "/usr/bin/runner /models/model.onnx"), monitor::WorkloadKind::GenericModel);

  const monitor::WorkloadDetection unknown = monitor::detect_workload_process(process("Safari", "Safari"));
  assert(unknown.kind == monitor::WorkloadKind::Unknown);
  assert(unknown.confidence == 0.0);
  return 0;
}
