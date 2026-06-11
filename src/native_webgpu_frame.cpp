#define WEBGPU_CPP_IMPLEMENTATION
#include "ast/assembler.hpp"
#include "ast/runtime.hpp"
#include "ast/wgsl.hpp"

#include <SDL.h>
#include <SDL_image.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <webgpu/webgpu.hpp>

namespace {

constexpr std::uint64_t uniform_channel_offset = 16;
constexpr std::uint64_t uniform_float_count = uniform_channel_offset + 16 + 512 + 8 + 4 + 32 + 16;
constexpr std::uint64_t uniform_payload_byte_size = uniform_float_count * sizeof(float);
constexpr std::uint64_t uniform_byte_size = (uniform_payload_byte_size + 15U) & ~std::uint64_t{15};

struct Args {
    std::string program_path;
    std::string output_path;
    int width = 240;
    int height = 160;
    int frame = 0;
    int frames = 1;
    float time = 0.0F;
    float time_delta = 1.0F / 60.0F;
    int max_steps = 4096;
    bool compare_cpu = false;
    int tolerance = 1;
    std::array<std::string, 4> channel_paths;
    std::array<std::string, 4> noise_specs;
    std::array<std::string, 4> video_paths;
    std::array<std::string, 4> audio_paths;
    std::array<std::string, 4> buffer_paths;
};

struct SizePreset {
    std::string_view name;
    int width = 0;
    int height = 0;
};

constexpr std::array<SizePreset, 18> size_presets{{
    {"gb", 160, 144},
    {"gameboy", 160, 144},
    {"gbc", 160, 144},
    {"gameboycolor", 160, 144},
    {"gba", 240, 160},
    {"nes", 256, 240},
    {"snes", 256, 224},
    {"genesis", 320, 224},
    {"megadrive", 320, 224},
    {"sms", 256, 192},
    {"mastersystem", 256, 192},
    {"n64", 320, 240},
    {"ps1", 320, 240},
    {"psx", 320, 240},
    {"ds", 256, 192},
    {"nds", 256, 192},
    {"psp", 480, 272},
    {"spelunky", 320, 240},
}};

constexpr int audio_texture_width = 512;
constexpr int audio_texture_height = 2;
constexpr int audio_sample_rate = 44100;

struct WebGpuContext {
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;
};

std::string lower_ascii(std::string_view text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (const char ch : text) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return lowered;
}

std::optional<std::pair<int, int>> parse_size(std::string_view text) {
    const std::string lowered = lower_ascii(text);
    for (const SizePreset& preset : size_presets) {
        if (lowered == preset.name) {
            return std::pair{preset.width, preset.height};
        }
    }

    const std::size_t split = text.find('x');
    if (split == std::string_view::npos) {
        return std::nullopt;
    }

    const int width = std::atoi(std::string{text.substr(0, split)}.c_str());
    const int height = std::atoi(std::string{text.substr(split + 1)}.c_str());
    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }
    return std::pair{width, height};
}

void print_usage() {
    std::cerr
        << "usage: ast-webgpu-frame program.asm --output frame.ppm [--size gba|240x160]\n"
        << "       [--frame N] [--frames N] [--time seconds] [--max-steps N] [--compare-cpu]\n"
        << "       [--channel0 path] through [--channel3 path] load image inputs\n"
        << "       [--noise0 seed] through [--noise3 seed] load generated noise textures\n"
        << "       [--video0 path] through [--video3 path] sample video inputs with ffmpeg\n"
        << "       [--audio0 path] through [--audio3 path] sample audio inputs with ffmpeg\n"
        << "       [--buffer0 path] through [--buffer3 path] run feedback buffer passes\n";
}

std::optional<Args> parse_args(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        auto next = [&]() -> std::optional<std::string_view> {
            if (i + 1 >= argc) {
                return std::nullopt;
            }
            ++i;
            return std::string_view{argv[i]};
        };

        if (arg == "--help" || arg == "-h") {
            return std::nullopt;
        }
        if (arg == "--output" || arg == "-o") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.output_path = std::string{*value};
            continue;
        }
        if (arg == "--size") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            const auto size = parse_size(*value);
            if (!size.has_value()) {
                return std::nullopt;
            }
            args.width = size->first;
            args.height = size->second;
            continue;
        }
        if (arg == "--frame") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.frame = std::atoi(std::string{*value}.c_str());
            if (args.frame < 0) {
                return std::nullopt;
            }
            continue;
        }
        if (arg == "--frames") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.frames = std::atoi(std::string{*value}.c_str());
            if (args.frames <= 0) {
                return std::nullopt;
            }
            continue;
        }
        if (arg == "--time") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.time = std::strtof(std::string{*value}.c_str(), nullptr);
            continue;
        }
        if (arg == "--time-delta") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.time_delta = std::strtof(std::string{*value}.c_str(), nullptr);
            continue;
        }
        if (arg == "--max-steps") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.max_steps = std::atoi(std::string{*value}.c_str());
            if (args.max_steps <= 0) {
                return std::nullopt;
            }
            continue;
        }
        if (arg == "--compare-cpu") {
            args.compare_cpu = true;
            continue;
        }
        if (arg == "--tolerance") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.tolerance = std::atoi(std::string{*value}.c_str());
            if (args.tolerance < 0 || args.tolerance > 255) {
                return std::nullopt;
            }
            continue;
        }
        if (arg == "--channel0" || arg == "--channel1" || arg == "--channel2" ||
            arg == "--channel3") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            const int channel = static_cast<int>(arg.back() - '0');
            args.channel_paths[static_cast<std::size_t>(channel)] = std::string{*value};
            continue;
        }
        if (arg == "--noise0" || arg == "--noise1" || arg == "--noise2" || arg == "--noise3") {
            const int channel = static_cast<int>(arg.back() - '0');
            std::string seed = std::to_string(channel + 1);
            if (i + 1 < argc) {
                const std::string_view possible_seed{argv[i + 1]};
                if (possible_seed.empty() || possible_seed.front() != '-') {
                    ++i;
                    seed = std::string{possible_seed};
                }
            }
            args.noise_specs[static_cast<std::size_t>(channel)] = std::move(seed);
            continue;
        }
        if (arg == "--video0" || arg == "--video1" || arg == "--video2" || arg == "--video3") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            const int channel = static_cast<int>(arg.back() - '0');
            args.video_paths[static_cast<std::size_t>(channel)] = std::string{*value};
            continue;
        }
        if (arg == "--audio0" || arg == "--audio1" || arg == "--audio2" || arg == "--audio3") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            const int channel = static_cast<int>(arg.back() - '0');
            args.audio_paths[static_cast<std::size_t>(channel)] = std::string{*value};
            continue;
        }
        if (arg == "--buffer0" || arg == "--buffer1" || arg == "--buffer2" || arg == "--buffer3") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            const int buffer = static_cast<int>(arg.back() - '0');
            args.buffer_paths[static_cast<std::size_t>(buffer)] = std::string{*value};
            continue;
        }
        if (!arg.empty() && arg.front() != '-' && args.program_path.empty()) {
            args.program_path = std::string{arg};
            continue;
        }
        return std::nullopt;
    }

    if (args.program_path.empty()) {
        return std::nullopt;
    }
    return args;
}

std::uint32_t align_to(std::uint32_t value, std::uint32_t alignment) {
    return ((value + alignment - 1U) / alignment) * alignment;
}

float target_channel_time(const Args& args) {
    return args.time + static_cast<float>(args.frames - 1) * args.time_delta;
}

std::uint32_t pack_rgba8(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
    return (static_cast<std::uint32_t>(a) << 24U) | (static_cast<std::uint32_t>(b) << 16U) |
           (static_cast<std::uint32_t>(g) << 8U) | static_cast<std::uint32_t>(r);
}

std::uint8_t sample_to_byte(float sample) {
    const float normalized = std::clamp(sample * 0.5F + 0.5F, 0.0F, 1.0F);
    return static_cast<std::uint8_t>(std::lround(normalized * 255.0F));
}

std::uint32_t hash_u32(std::uint32_t value) {
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    value *= 0x846ca68bU;
    value ^= value >> 16U;
    return value;
}

std::uint32_t seed_from_text(const std::string& text) {
    std::uint32_t seed = 2166136261U;
    for (unsigned char ch : text) {
        seed ^= static_cast<std::uint32_t>(ch);
        seed *= 16777619U;
    }
    return seed;
}

std::string shell_quote(const std::string& text) {
    std::string quoted = "'";
    for (char ch : text) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

std::optional<std::string> read_command_stdout(const std::string& command) {
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return std::nullopt;
    }

    std::string output;
    std::array<char, 4096> buffer{};
    while (true) {
        const std::size_t read = std::fread(buffer.data(), 1, buffer.size(), pipe);
        if (read > 0) {
            output.append(buffer.data(), read);
        }
        if (read < buffer.size()) {
            break;
        }
    }

    const int status = pclose(pipe);
    if (status != 0) {
        return std::nullopt;
    }
    return output;
}

std::optional<std::string> read_command_stdout_exact(const std::string& command,
                                                     std::size_t expected_size) {
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return std::nullopt;
    }

    std::string output;
    output.resize(expected_size);
    std::size_t offset = 0;
    while (offset < expected_size) {
        const std::size_t read =
            std::fread(output.data() + offset, 1, expected_size - offset, pipe);
        if (read == 0) {
            break;
        }
        offset += read;
    }

    const int status = pclose(pipe);
    if (status != 0 || offset != expected_size) {
        return std::nullopt;
    }
    return output;
}

double parse_frame_rate(std::string_view text) {
    const std::size_t split = text.find('/');
    if (split == std::string_view::npos) {
        const double value = std::atof(std::string{text}.c_str());
        return value > 0.0 ? value : 30.0;
    }

    const double numerator = std::atof(std::string{text.substr(0, split)}.c_str());
    const double denominator = std::atof(std::string{text.substr(split + 1)}.c_str());
    if (numerator <= 0.0 || denominator <= 0.0) {
        return 30.0;
    }
    return numerator / denominator;
}

bool load_channel_image(const std::string& path, ast::ImageChannel& out) {
    SDL_Surface* loaded = IMG_Load(path.c_str());
    if (loaded == nullptr) {
        std::cerr << "IMG_Load failed for " << path << ": " << IMG_GetError() << '\n';
        return false;
    }

    SDL_Surface* converted = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(loaded);
    if (converted == nullptr) {
        std::cerr << "SDL_ConvertSurfaceFormat failed for " << path << ": " << SDL_GetError()
                  << '\n';
        return false;
    }

    out.width = converted->w;
    out.height = converted->h;
    out.time = 0.0F;
    out.sample_rate = 0.0F;
    out.external_pixels = nullptr;
    out.pixels.resize(static_cast<std::size_t>(out.width * out.height));
    const auto* bytes = static_cast<const std::uint8_t*>(converted->pixels);
    for (int y = 0; y < out.height; ++y) {
        const auto* row = reinterpret_cast<const std::uint32_t*>(
            bytes + static_cast<std::size_t>(y) * static_cast<std::size_t>(converted->pitch));
        for (int x = 0; x < out.width; ++x) {
            out.pixels[static_cast<std::size_t>(y * out.width + x)] =
                row[static_cast<std::size_t>(x)];
        }
    }

    SDL_FreeSurface(converted);
    return true;
}

void load_noise_channel(const std::string& seed_text, ast::ImageChannel& out) {
    constexpr int noise_size = 256;
    const std::uint32_t seed = seed_from_text(seed_text);
    out.width = noise_size;
    out.height = noise_size;
    out.time = 0.0F;
    out.sample_rate = 0.0F;
    out.external_pixels = nullptr;
    out.pixels.resize(static_cast<std::size_t>(noise_size * noise_size));
    for (int y = 0; y < noise_size; ++y) {
        for (int x = 0; x < noise_size; ++x) {
            const std::uint32_t base = seed ^ (static_cast<std::uint32_t>(x) * 374761393U) ^
                                       (static_cast<std::uint32_t>(y) * 668265263U);
            const std::uint8_t r = static_cast<std::uint8_t>(hash_u32(base) & 0xFFU);
            const std::uint8_t g = static_cast<std::uint8_t>(hash_u32(base + 1U) & 0xFFU);
            const std::uint8_t b = static_cast<std::uint8_t>(hash_u32(base + 2U) & 0xFFU);
            out.pixels[static_cast<std::size_t>(y * noise_size + x)] = pack_rgba8(r, g, b);
        }
    }
}

bool load_video_channel(const std::string& path, float sample_time, ast::ImageChannel& out) {
    const std::string quoted_path = shell_quote(path);
    const std::string probe_command =
        "ffprobe -v error -select_streams v:0 "
        "-show_entries stream=width,height,avg_frame_rate:format=duration "
        "-of default=noprint_wrappers=1 " +
        quoted_path;
    const std::optional<std::string> probe_output = read_command_stdout(probe_command);
    if (!probe_output.has_value()) {
        std::cerr << "ffprobe failed for " << path << '\n';
        return false;
    }

    int width = 0;
    int height = 0;
    double frames_per_second = 30.0;
    double duration_seconds = 0.0;
    std::size_t offset = 0;
    while (offset < probe_output->size()) {
        const std::size_t end = probe_output->find('\n', offset);
        const std::string_view line{probe_output->data() + offset,
                                    (end == std::string::npos ? probe_output->size() : end) -
                                        offset};
        if (line.starts_with("width=")) {
            width = std::atoi(std::string{line.substr(6)}.c_str());
        } else if (line.starts_with("height=")) {
            height = std::atoi(std::string{line.substr(7)}.c_str());
        } else if (line.starts_with("avg_frame_rate=")) {
            frames_per_second = parse_frame_rate(line.substr(15));
        } else if (line.starts_with("duration=")) {
            duration_seconds = std::atof(std::string{line.substr(9)}.c_str());
        }
        if (end == std::string::npos) {
            break;
        }
        offset = end + 1;
    }

    if (width <= 0 || height <= 0) {
        std::cerr << "could not determine video dimensions for " << path << '\n';
        return false;
    }

    const double playback_time = duration_seconds > 0.0
                                     ? std::fmod(std::max(0.0F, sample_time), duration_seconds)
                                     : std::max(0.0F, sample_time);
    const long long target_frame =
        std::max(0LL, static_cast<long long>(std::floor(playback_time * frames_per_second)));
    const double snapped_time = frames_per_second > 0.0
                                    ? static_cast<double>(target_frame) / frames_per_second
                                    : playback_time;

    const std::string decode_command = "ffmpeg -v error -ss " + std::to_string(snapped_time) +
                                       " -i " + quoted_path +
                                       " -frames:v 1 -f rawvideo -pix_fmt rgba - 2>/dev/null";
    const std::size_t frame_bytes =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
    const std::optional<std::string> raw_frame =
        read_command_stdout_exact(decode_command, frame_bytes);
    if (!raw_frame.has_value()) {
        std::cerr << "ffmpeg video frame decode failed for " << path << '\n';
        return false;
    }

    out.width = width;
    out.height = height;
    out.time = static_cast<float>(snapped_time);
    out.sample_rate = 0.0F;
    out.external_pixels = nullptr;
    out.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (std::size_t pixel = 0; pixel < out.pixels.size(); ++pixel) {
        const std::uint8_t r = static_cast<std::uint8_t>((*raw_frame)[pixel * 4U + 0U]);
        const std::uint8_t g = static_cast<std::uint8_t>((*raw_frame)[pixel * 4U + 1U]);
        const std::uint8_t b = static_cast<std::uint8_t>((*raw_frame)[pixel * 4U + 2U]);
        const std::uint8_t a = static_cast<std::uint8_t>((*raw_frame)[pixel * 4U + 3U]);
        out.pixels[pixel] = pack_rgba8(r, g, b, a);
    }
    return true;
}

float audio_sample_at(const std::vector<float>& samples, std::size_t index) {
    if (samples.empty()) {
        return 0.0F;
    }
    return samples[index % samples.size()];
}

void build_audio_texture(const std::vector<float>& samples, int sample_rate, double time,
                         std::vector<std::uint32_t>& pixels) {
    pixels.resize(static_cast<std::size_t>(audio_texture_width * audio_texture_height));
    const auto center =
        static_cast<long long>(std::max(0.0, time) * static_cast<double>(sample_rate));
    const long long half_width = audio_texture_width / 2;
    for (int x = 0; x < audio_texture_width; ++x) {
        const long long source = std::max(0LL, center - half_width + x);
        const float wave = audio_sample_at(samples, static_cast<std::size_t>(source));
        const std::uint8_t wave_byte = sample_to_byte(wave);
        pixels[static_cast<std::size_t>(x)] = pack_rgba8(wave_byte, wave_byte, wave_byte);
    }

    constexpr int spectrum_bins = 128;
    constexpr int window = 512;
    constexpr float tau = 6.28318530717958647692F;
    std::array<float, spectrum_bins> magnitudes{};
    for (int bin = 0; bin < spectrum_bins; ++bin) {
        float real = 0.0F;
        float imag = 0.0F;
        for (int n = 0; n < window; ++n) {
            const long long source = std::max(0LL, center - window + n);
            const float sample = audio_sample_at(samples, static_cast<std::size_t>(source));
            const float phase = tau * static_cast<float>(bin + 1) * static_cast<float>(n) /
                                static_cast<float>(window);
            real += sample * std::cos(phase);
            imag -= sample * std::sin(phase);
        }
        magnitudes[static_cast<std::size_t>(bin)] =
            std::clamp(std::sqrt(real * real + imag * imag) / 32.0F, 0.0F, 1.0F);
    }

    for (int x = 0; x < audio_texture_width; ++x) {
        const int bin = (x * spectrum_bins) / audio_texture_width;
        const std::uint8_t magnitude = static_cast<std::uint8_t>(
            std::lround(magnitudes[static_cast<std::size_t>(bin)] * 255.0F));
        pixels[static_cast<std::size_t>(audio_texture_width + x)] =
            pack_rgba8(magnitude, magnitude, magnitude);
    }
}

bool load_audio_channel(const std::string& path, float sample_time, ast::ImageChannel& out) {
    const std::string command =
        "ffmpeg -v error -i " + shell_quote(path) + " -ac 1 -ar 44100 -f f32le -";
    const std::optional<std::string> raw_audio = read_command_stdout(command);
    if (!raw_audio.has_value() || raw_audio->size() < sizeof(float)) {
        std::cerr << "ffmpeg audio decode failed for " << path << '\n';
        return false;
    }

    std::vector<float> samples(raw_audio->size() / sizeof(float));
    std::memcpy(samples.data(), raw_audio->data(), samples.size() * sizeof(float));
    const double duration =
        static_cast<double>(samples.size()) / static_cast<double>(audio_sample_rate);
    const double audio_time =
        duration > 0.0 ? std::fmod(std::max(0.0F, sample_time), duration) : 0.0;

    out.width = audio_texture_width;
    out.height = audio_texture_height;
    out.time = static_cast<float>(audio_time);
    out.sample_rate = static_cast<float>(audio_sample_rate);
    out.external_pixels = nullptr;
    build_audio_texture(samples, audio_sample_rate, audio_time, out.pixels);
    return true;
}

bool load_channels(const Args& args, ast::ChannelSet& channels) {
    for (std::size_t i = 0; i < args.channel_paths.size(); ++i) {
        if (!args.channel_paths[i].empty() &&
            !load_channel_image(args.channel_paths[i], channels.image[i])) {
            return false;
        }
    }
    for (std::size_t i = 0; i < args.noise_specs.size(); ++i) {
        if (!args.noise_specs[i].empty()) {
            load_noise_channel(args.noise_specs[i], channels.image[i]);
        }
    }
    const float channel_time = target_channel_time(args);
    for (std::size_t i = 0; i < args.video_paths.size(); ++i) {
        if (!args.video_paths[i].empty() &&
            !load_video_channel(args.video_paths[i], channel_time, channels.image[i])) {
            return false;
        }
    }
    for (std::size_t i = 0; i < args.audio_paths.size(); ++i) {
        if (!args.audio_paths[i].empty() &&
            !load_audio_channel(args.audio_paths[i], channel_time, channels.image[i])) {
            return false;
        }
    }
    return true;
}

bool has_buffer_paths(const Args& args) {
    for (const std::string& path : args.buffer_paths) {
        if (!path.empty()) {
            return true;
        }
    }
    return false;
}

ast::ImageChannel make_buffer_channel(int width, int height,
                                      const std::vector<std::uint32_t>& pixels) {
    ast::ImageChannel channel;
    channel.width = width;
    channel.height = height;
    channel.time = 0.0F;
    channel.sample_rate = 0.0F;
    channel.external_pixels = &pixels;
    return channel;
}

ast::ImageChannel make_buffer_metadata_channel(int width, int height) {
    ast::ImageChannel channel;
    channel.width = width;
    channel.height = height;
    channel.time = 0.0F;
    channel.sample_rate = 0.0F;
    channel.external_pixels = nullptr;
    channel.pixels = {0};
    return channel;
}

wgpu::ShaderModule make_shader_module(wgpu::Device device, const char* source) {
    wgpu::ShaderModuleWGSLDescriptor wgsl{};
    wgsl.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    wgsl.code = source;

    wgpu::ShaderModuleDescriptor descriptor{};
    descriptor.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl.chain);
    return device.createShaderModule(descriptor);
}

bool make_context(WebGpuContext& out) {
    wgpu::InstanceDescriptor instance_descriptor{};
    out.instance = wgpu::createInstance(instance_descriptor);
    if (out.instance == WGPUInstance(nullptr)) {
        std::cerr << "could not create WebGPU instance\n";
        return false;
    }

    wgpu::RequestAdapterOptions adapter_options{};
    out.adapter = out.instance.requestAdapter(adapter_options);
    if (out.adapter == WGPUAdapter(nullptr)) {
        std::cerr << "could not request WebGPU adapter\n";
        return false;
    }

    wgpu::DeviceDescriptor device_descriptor{};
    out.device = out.adapter.requestDevice(device_descriptor);
    if (out.device == WGPUDevice(nullptr)) {
        std::cerr << "could not request WebGPU device\n";
        return false;
    }
    out.queue = out.device.getQueue();
    return true;
}

std::vector<std::uint8_t> map_readback(wgpu::Device device, wgpu::Buffer buffer,
                                       std::uint64_t size) {
    bool mapped = false;
    bool map_failed = false;
    auto map_callback =
        buffer.mapAsync(wgpu::MapMode::Read, 0, size, [&](wgpu::BufferMapAsyncStatus status) {
            mapped = true;
            map_failed = status != wgpu::BufferMapAsyncStatus::Success;
        });
    while (!mapped) {
        device.poll(true);
    }
    if (map_failed) {
        return {};
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    const void* mapped_data = buffer.getConstMappedRange(0, size);
    std::memcpy(bytes.data(), mapped_data, bytes.size());
    buffer.unmap();
    (void)map_callback;
    return bytes;
}

wgpu::Texture make_empty_channel_texture(WebGpuContext& context) {
    wgpu::TextureDescriptor descriptor{wgpu::Default};
    descriptor.size = wgpu::Extent3D(1, 1, 1);
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    wgpu::Texture texture = context.device.createTexture(descriptor);

    const std::array<std::uint8_t, 4> transparent_black{0, 0, 0, 0};
    wgpu::ImageCopyTexture destination{};
    destination.texture = texture;
    wgpu::TextureDataLayout layout{};
    layout.bytesPerRow = 256;
    layout.rowsPerImage = 1;
    context.queue.writeTexture(destination, transparent_black.data(), transparent_black.size(),
                               layout, wgpu::Extent3D(1, 1, 1));
    return texture;
}

wgpu::Texture make_channel_texture(WebGpuContext& context, const ast::ImageChannel& channel) {
    if (!channel.loaded()) {
        return make_empty_channel_texture(context);
    }

    wgpu::TextureDescriptor descriptor{wgpu::Default};
    descriptor.size = wgpu::Extent3D(static_cast<std::uint32_t>(channel.width),
                                     static_cast<std::uint32_t>(channel.height), 1);
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    wgpu::Texture texture = context.device.createTexture(descriptor);

    constexpr std::uint32_t bytes_per_row_alignment = 256;
    const std::uint32_t unpadded_bytes_per_row = static_cast<std::uint32_t>(channel.width) * 4U;
    const std::uint32_t bytes_per_row = align_to(unpadded_bytes_per_row, bytes_per_row_alignment);
    std::vector<std::uint8_t> padded(static_cast<std::size_t>(bytes_per_row) *
                                     static_cast<std::size_t>(channel.height));
    const std::vector<std::uint32_t>& pixels = channel.pixel_data();
    for (int y = 0; y < channel.height; ++y) {
        std::uint8_t* dst = padded.data() + static_cast<std::size_t>(y) * bytes_per_row;
        const auto* src = reinterpret_cast<const std::uint8_t*>(
            pixels.data() + static_cast<std::size_t>(y * channel.width));
        std::memcpy(dst, src, static_cast<std::size_t>(unpadded_bytes_per_row));
    }

    wgpu::ImageCopyTexture destination{};
    destination.texture = texture;
    wgpu::TextureDataLayout layout{};
    layout.bytesPerRow = bytes_per_row;
    layout.rowsPerImage = static_cast<std::uint32_t>(channel.height);
    context.queue.writeTexture(destination, padded.data(), padded.size(), layout,
                               wgpu::Extent3D(static_cast<std::uint32_t>(channel.width),
                                              static_cast<std::uint32_t>(channel.height), 1));
    return texture;
}

wgpu::Texture make_render_texture(WebGpuContext& context, const Args& args) {
    wgpu::TextureDescriptor descriptor{wgpu::Default};
    descriptor.size = wgpu::Extent3D(static_cast<std::uint32_t>(args.width),
                                     static_cast<std::uint32_t>(args.height), 1);
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::TextureBinding |
                       wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst;
    return context.device.createTexture(descriptor);
}

wgpu::Texture make_zero_render_texture(WebGpuContext& context, const Args& args) {
    wgpu::Texture texture = make_render_texture(context, args);
    constexpr std::uint32_t bytes_per_row_alignment = 256;
    const std::uint32_t unpadded_bytes_per_row = static_cast<std::uint32_t>(args.width) * 4U;
    const std::uint32_t bytes_per_row = align_to(unpadded_bytes_per_row, bytes_per_row_alignment);
    std::vector<std::uint8_t> zeroes(static_cast<std::size_t>(bytes_per_row) *
                                     static_cast<std::size_t>(args.height));
    wgpu::ImageCopyTexture destination{};
    destination.texture = texture;
    wgpu::TextureDataLayout layout{};
    layout.bytesPerRow = bytes_per_row;
    layout.rowsPerImage = static_cast<std::uint32_t>(args.height);
    context.queue.writeTexture(destination, zeroes.data(), zeroes.size(), layout,
                               wgpu::Extent3D(static_cast<std::uint32_t>(args.width),
                                              static_cast<std::uint32_t>(args.height), 1));
    return texture;
}

wgpu::BindGroupLayout make_asm_bind_group_layout(wgpu::Device device) {
    std::array<wgpu::BindGroupLayoutEntry, 6> entries{};
    entries[0].binding = 0;
    entries[0].visibility = wgpu::ShaderStage::Compute;
    entries[0].storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
    entries[0].storageTexture.format = wgpu::TextureFormat::RGBA8Unorm;
    entries[0].storageTexture.viewDimension = wgpu::TextureViewDimension::_2D;

    entries[1].binding = 1;
    entries[1].visibility = wgpu::ShaderStage::Compute;
    entries[1].buffer.type = wgpu::BufferBindingType::Uniform;

    for (std::uint32_t i = 0; i < 4; ++i) {
        wgpu::BindGroupLayoutEntry& entry = entries[static_cast<std::size_t>(i + 2U)];
        entry.binding = i + 2U;
        entry.visibility = wgpu::ShaderStage::Compute;
        entry.texture.sampleType = wgpu::TextureSampleType::Float;
        entry.texture.viewDimension = wgpu::TextureViewDimension::_2D;
    }

    wgpu::BindGroupLayoutDescriptor descriptor{};
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    return device.createBindGroupLayout(descriptor);
}

std::vector<float> make_uniforms(const Args& args, const ast::ChannelSet& channels) {
    std::vector<float> values(static_cast<std::size_t>(uniform_float_count), 0.0F);
    values[0] = args.time;
    values[1] = args.time_delta;
    values[2] = static_cast<float>(args.frame);
    values[3] = static_cast<float>(args.width);
    values[4] = static_cast<float>(args.height);
    values[13] = 1970.0F;
    values[14] = 1.0F;
    values[15] = 1.0F;
    for (std::size_t i = 0; i < channels.image.size(); ++i) {
        const ast::ImageChannel& channel = channels.image[i];
        if (!channel.loaded()) {
            continue;
        }
        const std::size_t offset = static_cast<std::size_t>(uniform_channel_offset) + i * 4U;
        values[offset + 0U] = static_cast<float>(channel.width);
        values[offset + 1U] = static_cast<float>(channel.height);
        values[offset + 2U] = channel.time;
        values[offset + 3U] = channel.sample_rate;
    }
    return values;
}

bool dispatch_program(WebGpuContext& context, const ast::Program& program, const Args& args,
                      const ast::ChannelSet& channel_set,
                      const std::array<wgpu::TextureView, 4>& channel_views,
                      wgpu::TextureView output_view) {
    const ast::WgslCompileResult compiled =
        ast::compile_wgsl(program, ast::WgslOptions{args.max_steps, 32});
    if (!compiled.ok()) {
        for (const ast::Diagnostic& diagnostic : compiled.diagnostics) {
            std::cerr << diagnostic.file << ':' << diagnostic.line << ": " << diagnostic.message
                      << '\n';
        }
        return false;
    }

    wgpu::BufferDescriptor uniform_descriptor{};
    uniform_descriptor.size = uniform_byte_size;
    uniform_descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer uniform = context.device.createBuffer(uniform_descriptor);
    const std::vector<float> uniforms = make_uniforms(args, channel_set);
    context.queue.writeBuffer(uniform, 0, uniforms.data(), uniform_payload_byte_size);

    wgpu::BindGroupLayout bind_group_layout = make_asm_bind_group_layout(context.device);
    WGPUBindGroupLayout raw_bind_group_layout = bind_group_layout;
    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor{};
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &raw_bind_group_layout;
    wgpu::PipelineLayout pipeline_layout =
        context.device.createPipelineLayout(pipeline_layout_descriptor);

    wgpu::ComputePipelineDescriptor pipeline_descriptor{};
    pipeline_descriptor.layout = pipeline_layout;
    pipeline_descriptor.compute.module =
        make_shader_module(context.device, compiled.source.c_str());
    pipeline_descriptor.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = context.device.createComputePipeline(pipeline_descriptor);
    if (pipeline == WGPUComputePipeline(nullptr)) {
        std::cerr << "could not create emitted asm WGSL compute pipeline\n";
        return false;
    }

    std::array<wgpu::BindGroupEntry, 6> entries{};
    entries[0].binding = 0;
    entries[0].textureView = output_view;
    entries[1].binding = 1;
    entries[1].buffer = uniform;
    entries[1].size = uniform_byte_size;
    for (std::size_t i = 0; i < channel_views.size(); ++i) {
        entries[i + 2].binding = static_cast<std::uint32_t>(i + 2);
        entries[i + 2].textureView = channel_views[i];
    }

    wgpu::BindGroupDescriptor bind_descriptor{};
    bind_descriptor.layout = bind_group_layout;
    bind_descriptor.entryCount = entries.size();
    bind_descriptor.entries = entries.data();
    wgpu::BindGroup bind_group = context.device.createBindGroup(bind_descriptor);
    if (bind_group == WGPUBindGroup(nullptr)) {
        std::cerr << "could not create emitted asm WGSL bind group\n";
        return false;
    }

    wgpu::CommandEncoder encoder = context.device.createCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.beginComputePass();
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bind_group, 0, nullptr);
    pass.dispatchWorkgroups((static_cast<std::uint32_t>(args.width) + 7U) / 8U,
                            (static_cast<std::uint32_t>(args.height) + 7U) / 8U, 1);
    pass.end();
    context.queue.submit(encoder.finish());
    return true;
}

std::vector<std::uint8_t> read_texture_rgba(WebGpuContext& context, wgpu::Texture texture,
                                            const Args& args) {
    constexpr std::uint32_t bytes_per_row_alignment = 256;
    const std::uint32_t unpadded_bytes_per_row = static_cast<std::uint32_t>(args.width) * 4U;
    const std::uint32_t bytes_per_row = align_to(unpadded_bytes_per_row, bytes_per_row_alignment);
    const std::uint64_t readback_size =
        static_cast<std::uint64_t>(bytes_per_row) * static_cast<std::uint64_t>(args.height);
    wgpu::BufferDescriptor readback_descriptor{};
    readback_descriptor.size = readback_size;
    readback_descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    wgpu::Buffer readback = context.device.createBuffer(readback_descriptor);

    wgpu::ImageCopyTexture source{};
    source.texture = texture;
    wgpu::ImageCopyBuffer destination{};
    destination.buffer = readback;
    destination.layout.bytesPerRow = bytes_per_row;
    destination.layout.rowsPerImage = static_cast<std::uint32_t>(args.height);
    wgpu::CommandEncoder encoder = context.device.createCommandEncoder();
    encoder.copyTextureToBuffer(source, destination,
                                wgpu::Extent3D(static_cast<std::uint32_t>(args.width),
                                               static_cast<std::uint32_t>(args.height), 1));
    context.queue.submit(encoder.finish());

    const std::vector<std::uint8_t> padded = map_readback(context.device, readback, readback_size);
    if (padded.size() != readback_size) {
        std::cerr << "WebGPU texture readback map failed\n";
        return {};
    }

    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(args.width * args.height * 4));
    for (int y = 0; y < args.height; ++y) {
        const std::uint8_t* src = padded.data() + static_cast<std::size_t>(y) * bytes_per_row;
        std::uint8_t* dst = rgba.data() + static_cast<std::size_t>(y * args.width) * 4U;
        std::memcpy(dst, src, static_cast<std::size_t>(unpadded_bytes_per_row));
    }
    return rgba;
}

std::array<wgpu::Texture, 4> make_base_channel_textures(WebGpuContext& context,
                                                        const ast::ChannelSet& channel_set) {
    return {
        make_channel_texture(context, channel_set.image[0]),
        make_channel_texture(context, channel_set.image[1]),
        make_channel_texture(context, channel_set.image[2]),
        make_channel_texture(context, channel_set.image[3]),
    };
}

std::array<wgpu::TextureView, 4> make_views(std::array<wgpu::Texture, 4>& textures) {
    return {
        textures[0].createView(),
        textures[1].createView(),
        textures[2].createView(),
        textures[3].createView(),
    };
}

std::vector<std::uint8_t> render_gpu_frame(WebGpuContext& context, const ast::Program& program,
                                           const Args& args, const ast::ChannelSet& channel_set) {
    std::array<wgpu::Texture, 4> channels = make_base_channel_textures(context, channel_set);
    std::array<wgpu::TextureView, 4> channel_views = make_views(channels);
    wgpu::Texture output = make_render_texture(context, args);
    wgpu::TextureView output_view = output.createView();
    if (!dispatch_program(context, program, args, channel_set, channel_views, output_view)) {
        return {};
    }
    return read_texture_rgba(context, output, args);
}

bool write_ppm(const std::string& path, int width, int height,
               const std::vector<std::uint8_t>& rgba) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "could not open output file: " << path << '\n';
        return false;
    }
    out << "P6\n" << width << ' ' << height << "\n255\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>(y * width + x) * 4U;
            const std::array<char, 3> rgb{
                static_cast<char>(rgba[offset + 0U]),
                static_cast<char>(rgba[offset + 1U]),
                static_cast<char>(rgba[offset + 2U]),
            };
            out.write(rgb.data(), rgb.size());
        }
    }
    return static_cast<bool>(out);
}

std::vector<std::uint8_t> render_cpu_frame(const ast::Program& program, const Args& args,
                                           const ast::ChannelSet& channels) {
    ast::FrameInputs inputs;
    inputs.width = args.width;
    inputs.height = args.height;
    inputs.time = args.time;
    inputs.time_delta = args.time_delta;
    inputs.frame = args.frame;
    inputs.year = 1970;
    inputs.month = 1;
    inputs.day = 1;
    inputs.channels = &channels;

    std::vector<std::uint32_t> packed;
    ast::render_frame(program, inputs, packed, ast::RunLimits{args.max_steps, 32});

    std::vector<std::uint8_t> rgba(packed.size() * 4U);
    for (std::size_t i = 0; i < packed.size(); ++i) {
        const std::uint32_t pixel = packed[i];
        rgba[i * 4U + 0U] = static_cast<std::uint8_t>(pixel & 0xFFU);
        rgba[i * 4U + 1U] = static_cast<std::uint8_t>((pixel >> 8U) & 0xFFU);
        rgba[i * 4U + 2U] = static_cast<std::uint8_t>((pixel >> 16U) & 0xFFU);
        rgba[i * 4U + 3U] = static_cast<std::uint8_t>((pixel >> 24U) & 0xFFU);
    }
    return rgba;
}

Args args_for_frame(const Args& args, int offset) {
    Args frame_args = args;
    frame_args.frame = args.frame + offset;
    frame_args.time = args.time + static_cast<float>(offset) * args.time_delta;
    frame_args.frames = 1;
    return frame_args;
}

std::vector<std::uint8_t>
render_cpu_pipeline(const ast::Program& image_program,
                    const std::array<std::optional<ast::Program>, 4>& buffer_programs,
                    const Args& args, const ast::ChannelSet& base_channels) {
    std::array<std::vector<std::uint32_t>, 4> previous_buffers;
    std::array<std::vector<std::uint32_t>, 4> current_buffers;
    const std::size_t pixel_count = static_cast<std::size_t>(args.width * args.height);
    for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
        if (buffer_programs[i].has_value()) {
            previous_buffers[i].resize(pixel_count);
            current_buffers[i].resize(pixel_count);
        }
    }

    std::vector<std::uint32_t> packed;
    for (int offset = 0; offset < args.frames; ++offset) {
        const Args frame_args = args_for_frame(args, offset);
        ast::ChannelSet previous_channels = base_channels;
        for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
            if (buffer_programs[i].has_value()) {
                previous_channels.image[i] =
                    make_buffer_channel(args.width, args.height, previous_buffers[i]);
            }
        }

        for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
            if (!buffer_programs[i].has_value()) {
                continue;
            }
            ast::FrameInputs buffer_inputs;
            buffer_inputs.width = frame_args.width;
            buffer_inputs.height = frame_args.height;
            buffer_inputs.time = frame_args.time;
            buffer_inputs.time_delta = frame_args.time_delta;
            buffer_inputs.frame = frame_args.frame;
            buffer_inputs.year = 1970;
            buffer_inputs.month = 1;
            buffer_inputs.day = 1;
            buffer_inputs.channels = &previous_channels;
            ast::render_frame(*buffer_programs[i], buffer_inputs, current_buffers[i],
                              ast::RunLimits{args.max_steps, 32});
        }

        ast::ChannelSet final_channels = base_channels;
        for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
            if (buffer_programs[i].has_value()) {
                final_channels.image[i] =
                    make_buffer_channel(args.width, args.height, current_buffers[i]);
            }
        }

        ast::FrameInputs image_inputs;
        image_inputs.width = frame_args.width;
        image_inputs.height = frame_args.height;
        image_inputs.time = frame_args.time;
        image_inputs.time_delta = frame_args.time_delta;
        image_inputs.frame = frame_args.frame;
        image_inputs.year = 1970;
        image_inputs.month = 1;
        image_inputs.day = 1;
        image_inputs.channels = &final_channels;
        ast::render_frame(image_program, image_inputs, packed, ast::RunLimits{args.max_steps, 32});

        for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
            if (buffer_programs[i].has_value()) {
                std::swap(previous_buffers[i], current_buffers[i]);
            }
        }
    }

    std::vector<std::uint8_t> rgba(packed.size() * 4U);
    for (std::size_t i = 0; i < packed.size(); ++i) {
        const std::uint32_t pixel = packed[i];
        rgba[i * 4U + 0U] = static_cast<std::uint8_t>(pixel & 0xFFU);
        rgba[i * 4U + 1U] = static_cast<std::uint8_t>((pixel >> 8U) & 0xFFU);
        rgba[i * 4U + 2U] = static_cast<std::uint8_t>((pixel >> 16U) & 0xFFU);
        rgba[i * 4U + 3U] = static_cast<std::uint8_t>((pixel >> 24U) & 0xFFU);
    }
    return rgba;
}

std::vector<std::uint8_t>
render_gpu_pipeline(WebGpuContext& context, const ast::Program& image_program,
                    const std::array<std::optional<ast::Program>, 4>& buffer_programs,
                    const Args& args, const ast::ChannelSet& base_channels) {
    std::array<wgpu::Texture, 4> base_textures = make_base_channel_textures(context, base_channels);
    std::array<wgpu::TextureView, 4> base_views = make_views(base_textures);

    std::array<wgpu::Texture, 4> previous_buffers{};
    std::array<wgpu::Texture, 4> current_buffers{};
    std::array<wgpu::TextureView, 4> previous_buffer_views{};
    std::array<wgpu::TextureView, 4> current_buffer_views{};
    for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
        if (!buffer_programs[i].has_value()) {
            continue;
        }
        previous_buffers[i] = make_zero_render_texture(context, args);
        current_buffers[i] = make_zero_render_texture(context, args);
        previous_buffer_views[i] = previous_buffers[i].createView();
        current_buffer_views[i] = current_buffers[i].createView();
    }

    wgpu::Texture output = make_render_texture(context, args);
    wgpu::TextureView output_view = output.createView();

    for (int offset = 0; offset < args.frames; ++offset) {
        const Args frame_args = args_for_frame(args, offset);
        ast::ChannelSet previous_channels = base_channels;
        std::array<wgpu::TextureView, 4> previous_views = base_views;
        for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
            if (buffer_programs[i].has_value()) {
                previous_channels.image[i] = make_buffer_metadata_channel(args.width, args.height);
                previous_views[i] = previous_buffer_views[i];
            }
        }

        for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
            if (!buffer_programs[i].has_value()) {
                continue;
            }
            if (!dispatch_program(context, *buffer_programs[i], frame_args, previous_channels,
                                  previous_views, current_buffer_views[i])) {
                return {};
            }
        }

        ast::ChannelSet final_channels = base_channels;
        std::array<wgpu::TextureView, 4> final_views = base_views;
        for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
            if (buffer_programs[i].has_value()) {
                final_channels.image[i] = make_buffer_metadata_channel(args.width, args.height);
                final_views[i] = current_buffer_views[i];
            }
        }

        if (!dispatch_program(context, image_program, frame_args, final_channels, final_views,
                              output_view)) {
            return {};
        }

        for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
            if (buffer_programs[i].has_value()) {
                std::swap(previous_buffers[i], current_buffers[i]);
                std::swap(previous_buffer_views[i], current_buffer_views[i]);
            }
        }
    }

    return read_texture_rgba(context, output, args);
}

bool compare_frames(const std::vector<std::uint8_t>& gpu, const std::vector<std::uint8_t>& cpu,
                    int tolerance) {
    if (gpu.size() != cpu.size()) {
        std::cerr << "CPU/GPU frame sizes differ\n";
        return false;
    }

    std::size_t mismatches = 0;
    int worst = 0;
    std::size_t worst_offset = 0;
    for (std::size_t i = 0; i < gpu.size(); ++i) {
        const int delta = std::abs(static_cast<int>(gpu[i]) - static_cast<int>(cpu[i]));
        if (delta > tolerance) {
            ++mismatches;
            if (delta > worst) {
                worst = delta;
                worst_offset = i;
            }
        }
    }

    if (mismatches == 0) {
        std::cout << "CPU/GPU comparison passed within tolerance " << tolerance << '\n';
        return true;
    }

    const std::size_t pixel = worst_offset / 4U;
    const std::size_t channel = worst_offset % 4U;
    std::cerr << "CPU/GPU comparison failed: " << mismatches << " byte mismatches, worst delta "
              << worst << " at pixel " << pixel << " channel " << channel << " (gpu "
              << static_cast<int>(gpu[worst_offset]) << ", cpu "
              << static_cast<int>(cpu[worst_offset]) << ")\n";
    return false;
}

void print_diagnostics(const std::vector<ast::Diagnostic>& diagnostics) {
    for (const ast::Diagnostic& diagnostic : diagnostics) {
        std::cerr << diagnostic.file << ':' << diagnostic.line << ": " << diagnostic.message
                  << '\n';
    }
}

bool assemble_buffer_programs(const Args& args,
                              std::array<std::optional<ast::Program>, 4>& buffer_programs) {
    for (std::size_t i = 0; i < args.buffer_paths.size(); ++i) {
        if (args.buffer_paths[i].empty()) {
            continue;
        }
        ast::AssembleResult assembled = ast::assemble_file(args.buffer_paths[i]);
        if (!assembled.ok()) {
            print_diagnostics(assembled.diagnostics);
            return false;
        }
        buffer_programs[i] = std::move(assembled.program);
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const std::optional<Args> parsed = parse_args(argc, argv);
    if (!parsed.has_value()) {
        print_usage();
        return 2;
    }
    const Args args = *parsed;

    ast::AssembleResult assembled = ast::assemble_file(args.program_path);
    if (!assembled.ok()) {
        print_diagnostics(assembled.diagnostics);
        return 1;
    }
    std::array<std::optional<ast::Program>, 4> buffer_programs;
    if (!assemble_buffer_programs(args, buffer_programs)) {
        return 1;
    }

    if (SDL_Init(SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }
    if (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG) == 0) {
        std::cerr << "IMG_Init failed: " << IMG_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    ast::ChannelSet channels;
    if (!load_channels(args, channels)) {
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    WebGpuContext context;
    if (!make_context(context)) {
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    auto error_callback =
        context.device.setUncapturedErrorCallback([](wgpu::ErrorType type, char const* message) {
            std::cerr << "WebGPU error " << static_cast<int>(type) << ": "
                      << (message != nullptr ? message : "<no message>") << '\n';
        });

    const std::vector<std::uint8_t> gpu =
        has_buffer_paths(args)
            ? render_gpu_pipeline(context, assembled.program, buffer_programs, args, channels)
            : render_gpu_frame(context, assembled.program, args_for_frame(args, args.frames - 1),
                               channels);
    if (gpu.empty()) {
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    if (args.compare_cpu) {
        const std::vector<std::uint8_t> cpu =
            has_buffer_paths(args)
                ? render_cpu_pipeline(assembled.program, buffer_programs, args, channels)
                : render_cpu_frame(assembled.program, args_for_frame(args, args.frames - 1),
                                   channels);
        if (!compare_frames(gpu, cpu, args.tolerance)) {
            IMG_Quit();
            SDL_Quit();
            return 1;
        }
    }

    if (!args.output_path.empty() && !write_ppm(args.output_path, args.width, args.height, gpu)) {
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    (void)error_callback;
    IMG_Quit();
    SDL_Quit();
    std::cout << "ok: rendered GPU frame";
    if (!args.output_path.empty()) {
        std::cout << " to " << args.output_path;
    }
    std::cout << '\n';
    return 0;
}
