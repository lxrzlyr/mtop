#include "monitor/model/workload_detector.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <vector>

namespace monitor {
namespace {

std::string lowercase(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return text;
}

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

std::optional<std::string> value_after_flag(const std::vector<std::string>& tokens,
                                            const std::string& flag) {
  for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
    if (tokens[i] == flag) {
      return tokens[i + 1];
    }
  }
  return std::nullopt;
}

std::vector<std::string> split_words(const std::string& text) {
  std::vector<std::string> words;
  std::string current;
  for (char ch : text) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      if (!current.empty()) {
        words.push_back(current);
        current.clear();
      }
    } else {
      current.push_back(ch);
    }
  }
  if (!current.empty()) {
    words.push_back(current);
  }
  return words;
}

std::string basename_like(const std::string& path) {
  const std::size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string generic_model_hint(const std::string& command) {
  static constexpr const char* kExtensions[] = {
      ".gguf", ".safetensors", ".onnx", ".mlmodelc",
  };
  const std::vector<std::string> words = split_words(command);
  for (const std::string& word : words) {
    const std::string lowered = lowercase(word);
    for (const char* extension : kExtensions) {
      if (contains(lowered, extension)) {
        return basename_like(word);
      }
    }
  }
  return "";
}

std::string model_flag_hint(const std::string& command) {
  const std::vector<std::string> words = split_words(command);
  if (auto value = value_after_flag(words, "--model")) {
    return basename_like(*value);
  }
  if (auto value = value_after_flag(words, "-m")) {
    return basename_like(*value);
  }
  return generic_model_hint(command);
}

WorkloadDetection make_detection(WorkloadKind kind,
                                 WorkloadRole role,
                                 std::string label,
                                 std::string model_hint,
                                 std::string reason,
                                 double confidence) {
  WorkloadDetection detection;
  detection.kind = kind;
  detection.role = role;
  detection.label = std::move(label);
  detection.model_hint = std::move(model_hint);
  detection.reason = std::move(reason);
  detection.confidence = confidence;
  return detection;
}

}  // namespace

const char* workload_kind_name(WorkloadKind kind) {
  switch (kind) {
    case WorkloadKind::Ollama: return "ollama";
    case WorkloadKind::LlamaCpp: return "llama_cpp";
    case WorkloadKind::Mlx: return "mlx";
    case WorkloadKind::PythonInference: return "python_inference";
    case WorkloadKind::LmStudio: return "lm_studio";
    case WorkloadKind::ComfyUi: return "comfyui";
    case WorkloadKind::GenericModel: return "generic_model";
    case WorkloadKind::Unknown: return "unknown";
  }
  return "unknown";
}

const char* workload_kind_label(WorkloadKind kind) {
  switch (kind) {
    case WorkloadKind::Ollama: return "Ollama";
    case WorkloadKind::LlamaCpp: return "llama.cpp";
    case WorkloadKind::Mlx: return "MLX";
    case WorkloadKind::PythonInference: return "Python";
    case WorkloadKind::LmStudio: return "LM Studio";
    case WorkloadKind::ComfyUi: return "ComfyUI";
    case WorkloadKind::GenericModel: return "Model";
    case WorkloadKind::Unknown: return "Unknown";
  }
  return "Unknown";
}

const char* workload_role_name(WorkloadRole role) {
  switch (role) {
    case WorkloadRole::Manager: return "manager";
    case WorkloadRole::Server: return "server";
    case WorkloadRole::Runner: return "runner";
    case WorkloadRole::Worker: return "worker";
    case WorkloadRole::Frontend: return "frontend";
    case WorkloadRole::Unknown: return "unknown";
  }
  return "unknown";
}

WorkloadDetection detect_workload_process(const ProcessSnapshot& process) {
  const std::string name = lowercase(process.name);
  const std::string command = lowercase(process.command);
  const std::string text = name + " " + command;

  if (name == "ollama" || contains(command, "ollama serve")) {
    return make_detection(WorkloadKind::Ollama, WorkloadRole::Manager, "ollama", "",
                          "ollama manager process", 0.96);
  }
  if (contains(name, "ollama") && contains(command, "runner")) {
    return make_detection(WorkloadKind::Ollama, WorkloadRole::Runner, "ollama runner",
                          model_flag_hint(process.command), "ollama runner process", 0.95);
  }
  if (contains(command, ".ollama") || contains(command, "ollama run")) {
    return make_detection(WorkloadKind::Ollama, WorkloadRole::Runner, "ollama",
                          model_flag_hint(process.command), "ollama command or model path", 0.72);
  }

  if (name == "llama-server" || name == "llama-cli" || name == "llama-bench") {
    return make_detection(WorkloadKind::LlamaCpp, WorkloadRole::Server, process.name,
                          model_flag_hint(process.command), "llama.cpp executable name", 0.94);
  }
  if (contains(command, ".gguf") || contains(command, "ggml") ||
      (contains(command, "--model") && contains(text, "llama"))) {
    return make_detection(WorkloadKind::LlamaCpp, WorkloadRole::Runner,
                          process.name.empty() ? "llama.cpp" : process.name,
                          model_flag_hint(process.command), "model path or llama.cpp flag", 0.70);
  }

  if (contains(name, "mlx-lm") || contains(name, "mlx_lm") ||
      contains(command, "python -m mlx_lm") || contains(command, "mlx_lm.server")) {
    return make_detection(WorkloadKind::Mlx, WorkloadRole::Server,
                          process.name.empty() ? "mlx" : process.name,
                          model_flag_hint(process.command), "MLX LM process", 0.93);
  }
  if (contains(text, "mlx-vlm") || contains(text, "mlx_lm") || contains(text, " mlx ")) {
    return make_detection(WorkloadKind::Mlx, WorkloadRole::Worker,
                          process.name.empty() ? "mlx" : process.name,
                          model_flag_hint(process.command), "MLX command hint", 0.68);
  }

  if (contains(command, "lm studio.app") || contains(command, "lmstudio") || contains(name, "lm studio")) {
    return make_detection(WorkloadKind::LmStudio, WorkloadRole::Server, "LM Studio",
                          model_flag_hint(process.command), "LM Studio app or server path", 0.92);
  }

  if (contains(text, "comfyui") || contains(command, "custom_nodes")) {
    return make_detection(WorkloadKind::ComfyUi, WorkloadRole::Server, "ComfyUI",
                          generic_model_hint(process.command), "ComfyUI path or custom nodes", 0.92);
  }
  if (contains(command, "safetensors") && (contains(command, "diffusion") || contains(command, "main.py"))) {
    return make_detection(WorkloadKind::ComfyUi, WorkloadRole::Worker, "ComfyUI",
                          generic_model_hint(process.command), "diffusion model path", 0.72);
  }

  if (contains(name, "uvicorn") || contains(command, "uvicorn") || contains(command, "fastapi") ||
      contains(command, "gradio") || contains(command, "vllm") || contains(command, "torchserve")) {
    return make_detection(WorkloadKind::PythonInference, WorkloadRole::Server,
                          process.name.empty() ? "python server" : process.name,
                          model_flag_hint(process.command), "Python inference server", 0.84);
  }
  if (contains(command, "transformers") || contains(command, "diffusers") ||
      contains(command, "sentence_transformers")) {
    return make_detection(WorkloadKind::PythonInference, WorkloadRole::Worker,
                          process.name.empty() ? "python inference" : process.name,
                          model_flag_hint(process.command), "Python model library hint", 0.66);
  }

  const std::string model_hint = generic_model_hint(process.command);
  if (!model_hint.empty()) {
    return make_detection(WorkloadKind::GenericModel, WorkloadRole::Unknown,
                          process.name.empty() ? "model process" : process.name,
                          model_hint, "model file extension", 0.62);
  }

  return make_detection(WorkloadKind::Unknown, WorkloadRole::Unknown, "", "", "no AI workload signal", 0.0);
}

}  // namespace monitor
