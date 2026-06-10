#include "ast/assembler.hpp"
#include "ast/runtime.hpp"

#include <SDL.h>
#include <SDL_image.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

struct Args {
    std::string program_path = "examples/basics/plasma.asm";
    int width = 240;
    int height = 160;
    int scale = 4;
    bool scale_set = false;
    int max_steps = 4096;
    int frames = -1;
    bool dry_run = false;
    bool no_graphics = false;
    bool show_fps = false;
    int measure_fps_frames = 0;
    std::string save_frame_path;
    std::array<std::string, 4> channel_paths;
    std::array<std::string, 4> video_paths;
    std::array<std::string, 4> webcam_paths;
    std::array<std::string, 4> buffer_paths;
};

struct VideoChannel {
    int width = 0;
    int height = 0;
    double frames_per_second = 30.0;
    double duration_seconds = 0.0;
    FILE* pipe = nullptr;
    std::string path;
    std::vector<std::uint8_t> pending_frame;
    std::vector<std::uint32_t> pixels;
    long long current_frame_index = 0;

    [[nodiscard]] bool loaded() const {
        return pipe != nullptr && width > 0 && height > 0 && !pixels.empty();
    }

    VideoChannel() = default;
    VideoChannel(const VideoChannel&) = delete;
    VideoChannel& operator=(const VideoChannel&) = delete;

    ~VideoChannel() {
        close();
    }

    void close() {
        if (pipe != nullptr) {
            pclose(pipe);
            pipe = nullptr;
        }
    }
};

struct WebcamChannel {
    int width = 320;
    int height = 240;
    FILE* pipe = nullptr;
    std::vector<std::uint8_t> pending_frame;
    std::vector<std::uint32_t> pixels;
    std::size_t pending_bytes = 0;
    std::mutex mutex;
    std::thread worker;
    std::atomic_bool running{false};
    std::atomic_bool failed{false};
    std::atomic_bool has_frame{false};

    WebcamChannel() = default;
    WebcamChannel(const WebcamChannel&) = delete;
    WebcamChannel& operator=(const WebcamChannel&) = delete;

    ~WebcamChannel() {
        close();
    }

    [[nodiscard]] bool loaded() const {
        return pipe != nullptr && width > 0 && height > 0;
    }

    void close() {
        running = false;
        if (worker.joinable()) {
            worker.join();
        }
        if (pipe != nullptr) {
            pclose(pipe);
            pipe = nullptr;
        }
    }
};

struct GameControllerHandle {
    SDL_GameController* controller = nullptr;

    GameControllerHandle() = default;
    GameControllerHandle(const GameControllerHandle&) = delete;
    GameControllerHandle& operator=(const GameControllerHandle&) = delete;

    ~GameControllerHandle() {
        close();
    }

    [[nodiscard]] SDL_GameController* get() const {
        return controller;
    }

    void close() {
        if (controller != nullptr) {
            SDL_GameControllerClose(controller);
            controller = nullptr;
        }
    }

    void refresh() {
        if (controller != nullptr) {
            if (SDL_GameControllerGetAttached(controller) == SDL_TRUE) {
                return;
            }
            close();
        }

        const int joystick_count = SDL_NumJoysticks();
        for (int i = 0; i < joystick_count; ++i) {
            if (SDL_IsGameController(i) == SDL_TRUE) {
                controller = SDL_GameControllerOpen(i);
                if (controller != nullptr) {
                    return;
                }
            }
        }
    }
};

std::string lower_ascii(std::string_view text) {
    std::string lowered;
    lowered.reserve(text.size());
    for (const char ch : text) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return lowered;
}

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

std::optional<Args> parse_args(int argc, char** argv) {
    Args args;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
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
            if (!args.scale_set) {
                args.scale = 1;
            }
            continue;
        }
        if (arg == "--scale" || arg == "--dimscale") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.scale = std::atoi(std::string{*value}.c_str());
            if (args.scale <= 0) {
                return std::nullopt;
            }
            args.scale_set = true;
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
        if (arg == "--frames") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.frames = std::atoi(std::string{*value}.c_str());
            if (args.frames < 0) {
                return std::nullopt;
            }
            continue;
        }
        if (arg == "--dry-run" || arg == "--dryrun") {
            args.dry_run = true;
            args.no_graphics = true;
            if (args.frames < 0) {
                args.frames = 0;
            }
            continue;
        }
        if (arg == "--save-frame") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.save_frame_path = std::string{*value};
            args.no_graphics = true;
            if (args.frames < 0) {
                args.frames = 1;
            }
            continue;
        }
        if (arg == "--no-graphics" || arg == "--nographics") {
            args.no_graphics = true;
            if (args.frames < 0) {
                args.frames = 1;
            }
            continue;
        }
        if (arg == "--fps") {
            args.show_fps = true;
            continue;
        }
        if (arg == "--measure-fps") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            args.measure_fps_frames = std::atoi(std::string{*value}.c_str());
            if (args.measure_fps_frames <= 0) {
                return std::nullopt;
            }
            args.no_graphics = true;
            args.frames = args.measure_fps_frames;
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
        if (arg == "--video0" || arg == "--video1" || arg == "--video2" || arg == "--video3") {
            const auto value = next();
            if (!value.has_value()) {
                return std::nullopt;
            }
            const int channel = static_cast<int>(arg.back() - '0');
            args.video_paths[static_cast<std::size_t>(channel)] = std::string{*value};
            continue;
        }
        if (arg == "--webcam0" || arg == "--webcam1" || arg == "--webcam2" || arg == "--webcam3") {
            const int channel = static_cast<int>(arg.back() - '0');
            std::string device = "/dev/video" + std::to_string(channel);
            if (i + 1 < argc) {
                const std::string_view possible_device{argv[i + 1]};
                if (possible_device.empty() || possible_device.front() != '-') {
                    ++i;
                    device = std::string{possible_device};
                }
            }
            args.webcam_paths[static_cast<std::size_t>(channel)] = std::move(device);
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
        if (!arg.empty() && arg.front() == '-') {
            return std::nullopt;
        }
        args.program_path = std::string{arg};
    }

    return args;
}

void print_usage() {
    std::cerr
        << "usage: asm-shader-toy [program.asm] [--size 240x160|gba|gb|n64|ps1|psp] [--scale N]\n"
        << "       --dimscale is accepted as an alias for --scale\n"
        << "       --size presets: gb, gbc, gba, nes, snes, genesis, sms, n64, ps1, ds, psp\n"
        << "       --channel0 path through --channel3 path load image inputs\n"
        << "       --video0 path through --video3 path load video inputs with ffmpeg\n"
        << "       --webcam0 [device] through --webcam3 [device] load webcam inputs with "
           "ffmpeg/v4l2\n"
        << "       --buffer0 path through --buffer3 path run feedback buffer passes into channels\n"
        << "       --dry-run assembles and validates image inputs without rendering\n"
        << "       --no-graphics --frames N renders N CPU frames without a window\n"
        << "       --save-frame path.png renders one headless frame to a PNG\n"
        << "       --fps shows a small FPS overlay in graphical runs\n"
        << "       --measure-fps N renders N CPU frames and prints average FPS\n"
        << "       graphical runs hot reload the program and its includes on save\n";
}

bool has_channel_paths(const Args& args) {
    for (const std::string& path : args.channel_paths) {
        if (!path.empty()) {
            return true;
        }
    }
    return false;
}

bool has_buffer_paths(const Args& args) {
    for (const std::string& path : args.buffer_paths) {
        if (!path.empty()) {
            return true;
        }
    }
    return false;
}

bool has_video_paths(const Args& args) {
    for (const std::string& path : args.video_paths) {
        if (!path.empty()) {
            return true;
        }
    }
    return false;
}

bool has_webcam_paths(const Args& args) {
    for (const std::string& path : args.webcam_paths) {
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
    channel.external_pixels = &pixels;
    return channel;
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

bool load_video_channel(const std::string& path, VideoChannel& out) {
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

    out.close();
    out.width = width;
    out.height = height;
    out.frames_per_second = frames_per_second;
    out.duration_seconds = duration_seconds > 0.0 ? duration_seconds : 0.0;
    out.path = path;
    out.current_frame_index = 0;

    const std::string decode_command = "ffmpeg -v error -stream_loop -1 -i " + quoted_path +
                                       " -f rawvideo -pix_fmt rgba - 2>/dev/null";
    out.pipe = popen(decode_command.c_str(), "r");
    if (out.pipe == nullptr) {
        std::cerr << "failed to start video decode for " << path << '\n';
        return false;
    }

    const std::size_t frame_bytes =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
    out.pending_frame.resize(frame_bytes);
    out.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

    std::size_t pending_bytes = 0;
    while (pending_bytes < frame_bytes) {
        const ssize_t bytes_read = read(fileno(out.pipe), out.pending_frame.data() + pending_bytes,
                                        frame_bytes - pending_bytes);
        if (bytes_read <= 0) {
            if (bytes_read < 0 && errno == EINTR) {
                continue;
            }
            std::cerr << "video has no decoded frames: " << path << '\n';
            out.close();
            return false;
        }
        pending_bytes += static_cast<std::size_t>(bytes_read);
    }

    for (std::size_t pixel = 0; pixel < out.pixels.size(); ++pixel) {
        const std::uint8_t r = out.pending_frame[pixel * 4U + 0U];
        const std::uint8_t g = out.pending_frame[pixel * 4U + 1U];
        const std::uint8_t b = out.pending_frame[pixel * 4U + 2U];
        const std::uint8_t a = out.pending_frame[pixel * 4U + 3U];
        out.pixels[pixel] = (static_cast<std::uint32_t>(a) << 24U) |
                            (static_cast<std::uint32_t>(b) << 16U) |
                            (static_cast<std::uint32_t>(g) << 8U) | static_cast<std::uint32_t>(r);
    }

    return true;
}

bool read_next_video_frame(VideoChannel& video) {
    if (video.pipe == nullptr) {
        return false;
    }

    const std::size_t frame_bytes =
        static_cast<std::size_t>(video.width) * static_cast<std::size_t>(video.height) * 4U;
    video.pending_frame.resize(frame_bytes);

    std::size_t pending_bytes = 0;
    while (pending_bytes < frame_bytes) {
        const ssize_t bytes_read =
            read(fileno(video.pipe), video.pending_frame.data() + pending_bytes,
                 frame_bytes - pending_bytes);
        if (bytes_read <= 0) {
            if (bytes_read < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }
        pending_bytes += static_cast<std::size_t>(bytes_read);
    }

    video.pixels.resize(static_cast<std::size_t>(video.width) *
                        static_cast<std::size_t>(video.height));
    for (std::size_t pixel = 0; pixel < video.pixels.size(); ++pixel) {
        const std::uint8_t r = video.pending_frame[pixel * 4U + 0U];
        const std::uint8_t g = video.pending_frame[pixel * 4U + 1U];
        const std::uint8_t b = video.pending_frame[pixel * 4U + 2U];
        const std::uint8_t a = video.pending_frame[pixel * 4U + 3U];
        video.pixels[pixel] = (static_cast<std::uint32_t>(a) << 24U) |
                              (static_cast<std::uint32_t>(b) << 16U) |
                              (static_cast<std::uint32_t>(g) << 8U) | static_cast<std::uint32_t>(r);
    }
    ++video.current_frame_index;
    return true;
}

bool update_video_channels(ast::ChannelSet& channels, std::array<VideoChannel, 4>& video_channels,
                           double time) {
    for (std::size_t i = 0; i < video_channels.size(); ++i) {
        VideoChannel& video = video_channels[i];
        if (!video.loaded()) {
            continue;
        }

        const long long target_frame =
            std::max(0LL, static_cast<long long>(std::floor(time * video.frames_per_second)));
        while (video.current_frame_index < target_frame) {
            if (!read_next_video_frame(video)) {
                std::cerr << "video decode ended on channel " << i << '\n';
                return false;
            }
        }

        const double playback_time =
            video.duration_seconds > 0.0
                ? std::fmod(static_cast<double>(video.current_frame_index) /
                                video.frames_per_second,
                            video.duration_seconds)
                : static_cast<double>(video.current_frame_index) / video.frames_per_second;
        channels.image[i].width = video.width;
        channels.image[i].height = video.height;
        channels.image[i].time = static_cast<float>(playback_time);
        channels.image[i].pixels.clear();
        channels.image[i].external_pixels = &video.pixels;
    }
    return true;
}

void pack_webcam_frame(WebcamChannel& webcam) {
    std::vector<std::uint32_t> packed;
    packed.resize(static_cast<std::size_t>(webcam.width) * static_cast<std::size_t>(webcam.height));
    for (int y = 0; y < webcam.height; ++y) {
        for (int x = 0; x < webcam.width; ++x) {
            const auto src_pixel =
                static_cast<std::size_t>(y * webcam.width + (webcam.width - 1 - x));
            const auto dst_pixel = static_cast<std::size_t>(y * webcam.width + x);
            const std::uint8_t r = webcam.pending_frame[src_pixel * 4U + 0U];
            const std::uint8_t g = webcam.pending_frame[src_pixel * 4U + 1U];
            const std::uint8_t b = webcam.pending_frame[src_pixel * 4U + 2U];
            const std::uint8_t a = webcam.pending_frame[src_pixel * 4U + 3U];
            packed[dst_pixel] =
                (static_cast<std::uint32_t>(a) << 24U) | (static_cast<std::uint32_t>(b) << 16U) |
                (static_cast<std::uint32_t>(g) << 8U) | static_cast<std::uint32_t>(r);
        }
    }

    const std::lock_guard<std::mutex> lock(webcam.mutex);
    webcam.pixels = std::move(packed);
    webcam.has_frame = true;
}

bool read_webcam_initial_frame(WebcamChannel& webcam) {
    if (webcam.pipe == nullptr) {
        return false;
    }

    const std::size_t frame_bytes =
        static_cast<std::size_t>(webcam.width) * static_cast<std::size_t>(webcam.height) * 4U;
    webcam.pending_frame.resize(frame_bytes);
    webcam.pending_bytes = 0;

    const int fd = fileno(webcam.pipe);
    while (webcam.pending_bytes < frame_bytes) {
        const ssize_t bytes_read = read(fd, webcam.pending_frame.data() + webcam.pending_bytes,
                                        frame_bytes - webcam.pending_bytes);
        if (bytes_read <= 0) {
            if (bytes_read < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }
        webcam.pending_bytes += static_cast<std::size_t>(bytes_read);
    }

    pack_webcam_frame(webcam);
    webcam.pending_bytes = 0;
    return true;
}

bool set_pipe_nonblocking(FILE* pipe) {
    const int fd = fileno(pipe);
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool drain_webcam_frame(WebcamChannel& webcam);

bool open_webcam_channel(const std::string& device, WebcamChannel& out) {
    out.close();
    out.width = 320;
    out.height = 240;
    const std::string command =
        "ffmpeg -v error -fflags nobuffer -flags low_delay -f v4l2 -framerate 30 "
        "-video_size 320x240 -i " +
        shell_quote(device) + " -f rawvideo -pix_fmt rgba - 2>/dev/null";
    out.pipe = popen(command.c_str(), "r");
    if (out.pipe == nullptr) {
        std::cerr << "failed to start webcam capture for " << device << '\n';
        return false;
    }

    if (!read_webcam_initial_frame(out)) {
        std::cerr << "failed to read webcam frame from " << device << '\n';
        pclose(out.pipe);
        out.pipe = nullptr;
        return false;
    }
    if (!set_pipe_nonblocking(out.pipe)) {
        std::cerr << "failed to set webcam pipe nonblocking for " << device << '\n';
        pclose(out.pipe);
        out.pipe = nullptr;
        return false;
    }
    out.failed = false;
    out.running = true;
    out.worker = std::thread([&out]() {
        while (out.running) {
            if (!drain_webcam_frame(out)) {
                out.failed = true;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
        }
    });
    return true;
}

bool drain_webcam_frame(WebcamChannel& webcam) {
    if (webcam.pipe == nullptr) {
        return false;
    }

    const std::size_t frame_bytes =
        static_cast<std::size_t>(webcam.width) * static_cast<std::size_t>(webcam.height) * 4U;
    webcam.pending_frame.resize(frame_bytes);
    const int fd = fileno(webcam.pipe);
    bool got_complete_frame = false;

    while (true) {
        const ssize_t bytes_read = read(fd, webcam.pending_frame.data() + webcam.pending_bytes,
                                        frame_bytes - webcam.pending_bytes);
        if (bytes_read > 0) {
            webcam.pending_bytes += static_cast<std::size_t>(bytes_read);
            if (webcam.pending_bytes == frame_bytes) {
                pack_webcam_frame(webcam);
                webcam.pending_bytes = 0;
                got_complete_frame = true;
            }
            continue;
        }

        if (bytes_read == 0) {
            return false;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }
        if (errno == EINTR) {
            continue;
        }
        return false;
    }

    return got_complete_frame || webcam.has_frame;
}

bool update_webcam_channels(ast::ChannelSet& channels,
                            std::array<WebcamChannel, 4>& webcam_channels, float time) {
    for (std::size_t i = 0; i < webcam_channels.size(); ++i) {
        WebcamChannel& webcam = webcam_channels[i];
        if (webcam.pipe == nullptr) {
            continue;
        }
        if (webcam.failed) {
            std::cerr << "webcam stream ended on channel " << i << '\n';
            return false;
        }
        channels.image[i].width = webcam.width;
        channels.image[i].height = webcam.height;
        channels.image[i].time = time;
        channels.image[i].external_pixels = nullptr;
        {
            const std::lock_guard<std::mutex> lock(webcam.mutex);
            channels.image[i].pixels = webcam.pixels;
        }
    }
    return true;
}

int mouse_button_index(std::uint8_t sdl_button) {
    switch (sdl_button) {
    case SDL_BUTTON_LEFT:
        return 0;
    case SDL_BUTTON_RIGHT:
        return 1;
    case SDL_BUTTON_MIDDLE:
        return 2;
    case SDL_BUTTON_X1:
        return 3;
    case SDL_BUTTON_X2:
        return 4;
    default:
        return -1;
    }
}

void update_keyboard_input(ast::InputState& input_state) {
    int count = 0;
    const Uint8* keys = SDL_GetKeyboardState(&count);
    input_state.keys.fill(0.0F);
    const int key_count = std::min(count, ast::key_input_count);
    for (int i = 0; i < key_count; ++i) {
        input_state.keys[static_cast<std::size_t>(i)] = keys[i] != 0 ? 1.0F : 0.0F;
    }
}

float normalize_controller_axis(Sint16 value) {
    if (value < 0) {
        return static_cast<float>(value) / 32768.0F;
    }
    return static_cast<float>(value) / 32767.0F;
}

void update_gamepad_input(GameControllerHandle& controller_handle, ast::InputState& input_state) {
    input_state.gamepad_buttons.fill(0.0F);
    input_state.gamepad_axes.fill(0.0F);

    controller_handle.refresh();
    SDL_GameController* controller = controller_handle.get();
    if (controller == nullptr) {
        return;
    }

    const int button_count =
        std::min(static_cast<int>(SDL_CONTROLLER_BUTTON_MAX), ast::gamepad_button_input_count);
    for (int i = 0; i < button_count; ++i) {
        input_state.gamepad_buttons[static_cast<std::size_t>(i)] =
            SDL_GameControllerGetButton(controller, static_cast<SDL_GameControllerButton>(i)) != 0
                ? 1.0F
                : 0.0F;
    }

    const int axis_count =
        std::min(static_cast<int>(SDL_CONTROLLER_AXIS_MAX), ast::gamepad_axis_input_count);
    for (int i = 0; i < axis_count; ++i) {
        input_state.gamepad_axes[static_cast<std::size_t>(i)] = normalize_controller_axis(
            SDL_GameControllerGetAxis(controller, static_cast<SDL_GameControllerAxis>(i)));
    }
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

bool save_frame_png(const std::string& path, int width, int height,
                    std::vector<std::uint32_t>& pixels) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels.data(), width, height, 32, width * static_cast<int>(sizeof(std::uint32_t)),
        SDL_PIXELFORMAT_ABGR8888);
    if (surface == nullptr) {
        std::cerr << "SDL_CreateRGBSurfaceWithFormatFrom failed: " << SDL_GetError() << '\n';
        return false;
    }

    const int result = IMG_SavePNG(surface, path.c_str());
    SDL_FreeSurface(surface);
    if (result != 0) {
        std::cerr << "IMG_SavePNG failed for " << path << ": " << IMG_GetError() << '\n';
        return false;
    }
    return true;
}

ast::FrameInputs make_frame_inputs(const Args& args, const ast::ChannelSet& channels, int frame,
                                   float time, float time_delta, float mouse_x = 0.0F,
                                   float mouse_y = 0.0F, float mouse_down = 0.0F,
                                   float mouse_click_x = 0.0F, float mouse_click_y = 0.0F,
                                   const ast::InputState* input_state = nullptr) {
    const std::time_t wall_time = std::time(nullptr);
    const std::tm* local_time = std::localtime(&wall_time);

    ast::FrameInputs frame_inputs;
    frame_inputs.width = args.width;
    frame_inputs.height = args.height;
    frame_inputs.time = time;
    frame_inputs.time_delta = time_delta;
    frame_inputs.frame = frame;
    frame_inputs.mouse_x = mouse_down != 0.0F ? mouse_x : 0.0F;
    frame_inputs.mouse_y = mouse_down != 0.0F ? mouse_y : 0.0F;
    frame_inputs.mouse_down = mouse_down;
    frame_inputs.mouse_click_x = mouse_click_x;
    frame_inputs.mouse_click_y = mouse_click_y;
    frame_inputs.wall_clock_seconds =
        local_time != nullptr
            ? static_cast<float>((local_time->tm_hour * 60 + local_time->tm_min) * 60 +
                                 local_time->tm_sec)
            : 0.0F;
    frame_inputs.year = local_time != nullptr ? local_time->tm_year + 1900 : 1970;
    frame_inputs.month = local_time != nullptr ? local_time->tm_mon + 1 : 1;
    frame_inputs.day = local_time != nullptr ? local_time->tm_mday : 1;
    frame_inputs.channels = &channels;
    frame_inputs.input_state = input_state;
    return frame_inputs;
}

using FileWriteTimes =
    std::map<std::filesystem::path, std::optional<std::filesystem::file_time_type>>;

std::optional<std::filesystem::file_time_type> file_write_time(const std::filesystem::path& path) {
    std::error_code error;
    const auto time = std::filesystem::last_write_time(path, error);
    if (error) {
        return std::nullopt;
    }
    return time;
}

FileWriteTimes snapshot_dependencies(const std::vector<std::filesystem::path>& dependencies) {
    FileWriteTimes snapshot;
    for (const std::filesystem::path& dependency : dependencies) {
        snapshot[dependency] = file_write_time(dependency);
    }
    return snapshot;
}

std::vector<std::filesystem::path>
collect_dependencies(const ast::AssembleResult& main_program,
                     const std::array<std::optional<ast::AssembleResult>, 4>& buffers) {
    std::vector<std::filesystem::path> dependencies = main_program.dependencies;
    for (const auto& buffer : buffers) {
        if (!buffer.has_value()) {
            continue;
        }
        dependencies.insert(dependencies.end(), buffer->dependencies.begin(),
                            buffer->dependencies.end());
    }
    return dependencies;
}

bool dependencies_changed(const FileWriteTimes& snapshot) {
    for (const auto& [path, previous_time] : snapshot) {
        if (file_write_time(path) != previous_time) {
            return true;
        }
    }
    return false;
}

void print_diagnostics(const std::vector<ast::Diagnostic>& diagnostics) {
    for (const ast::Diagnostic& diagnostic : diagnostics) {
        std::cerr << diagnostic.file << ":" << diagnostic.line << ": " << diagnostic.message
                  << '\n';
    }
}

bool assemble_buffers(const Args& args,
                      std::array<std::optional<ast::AssembleResult>, 4>& out_buffers) {
    for (std::size_t i = 0; i < args.buffer_paths.size(); ++i) {
        if (args.buffer_paths[i].empty()) {
            out_buffers[i].reset();
            continue;
        }

        ast::AssembleResult assembled = ast::assemble_file(args.buffer_paths[i]);
        if (!assembled.ok()) {
            print_diagnostics(assembled.diagnostics);
            return false;
        }
        out_buffers[i] = std::move(assembled);
    }
    return true;
}

void ensure_buffer_storage(const Args& args,
                           std::array<std::vector<std::uint32_t>, 4>& previous_buffers,
                           std::array<std::vector<std::uint32_t>, 4>& current_buffers) {
    const std::size_t pixel_count = static_cast<std::size_t>(args.width * args.height);
    for (std::size_t i = 0; i < args.buffer_paths.size(); ++i) {
        if (args.buffer_paths[i].empty()) {
            continue;
        }
        previous_buffers[i].resize(pixel_count);
        current_buffers[i].resize(pixel_count);
    }
}

void render_pipeline(const Args& args, const ast::Program& image_program,
                     const std::array<std::optional<ast::AssembleResult>, 4>& buffer_programs,
                     const ast::ChannelSet& base_channels,
                     std::array<std::vector<std::uint32_t>, 4>& previous_buffers,
                     std::array<std::vector<std::uint32_t>, 4>& current_buffers, int frame,
                     float time, float time_delta, std::vector<std::uint32_t>& pixels,
                     float mouse_x = 0.0F, float mouse_y = 0.0F, float mouse_down = 0.0F,
                     float mouse_click_x = 0.0F, float mouse_click_y = 0.0F,
                     const ast::InputState* input_state = nullptr) {
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
        const ast::FrameInputs buffer_inputs =
            make_frame_inputs(args, previous_channels, frame, time, time_delta, mouse_x, mouse_y,
                              mouse_down, mouse_click_x, mouse_click_y, input_state);
        ast::render_frame(buffer_programs[i]->program, buffer_inputs, current_buffers[i],
                          ast::RunLimits{args.max_steps});
    }

    ast::ChannelSet final_channels = base_channels;
    for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
        if (buffer_programs[i].has_value()) {
            final_channels.image[i] =
                make_buffer_channel(args.width, args.height, current_buffers[i]);
        }
    }

    const ast::FrameInputs image_inputs =
        make_frame_inputs(args, final_channels, frame, time, time_delta, mouse_x, mouse_y,
                          mouse_down, mouse_click_x, mouse_click_y, input_state);
    ast::render_frame(image_program, image_inputs, pixels, ast::RunLimits{args.max_steps});

    for (std::size_t i = 0; i < buffer_programs.size(); ++i) {
        if (buffer_programs[i].has_value()) {
            std::swap(previous_buffers[i], current_buffers[i]);
        }
    }
}

const char* glyph_rows(char ch) {
    switch (ch) {
    case '0':
        return "111"
               "101"
               "101"
               "101"
               "111";
    case '1':
        return "010"
               "110"
               "010"
               "010"
               "111";
    case '2':
        return "111"
               "001"
               "111"
               "100"
               "111";
    case '3':
        return "111"
               "001"
               "111"
               "001"
               "111";
    case '4':
        return "101"
               "101"
               "111"
               "001"
               "001";
    case '5':
        return "111"
               "100"
               "111"
               "001"
               "111";
    case '6':
        return "111"
               "100"
               "111"
               "101"
               "111";
    case '7':
        return "111"
               "001"
               "010"
               "010"
               "010";
    case '8':
        return "111"
               "101"
               "111"
               "101"
               "111";
    case '9':
        return "111"
               "101"
               "111"
               "001"
               "111";
    case 'F':
        return "111"
               "100"
               "110"
               "100"
               "100";
    case 'P':
        return "110"
               "101"
               "110"
               "100"
               "100";
    case 'S':
        return "111"
               "100"
               "111"
               "001"
               "111";
    case ':':
        return "000"
               "010"
               "000"
               "010"
               "000";
    case '.':
        return "000"
               "000"
               "000"
               "000"
               "010";
    default:
        return "000"
               "000"
               "000"
               "000"
               "000";
    }
}

void draw_debug_text(SDL_Renderer* renderer, const std::string& text, int x, int y, int scale,
                     SDL_Color color) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
    const SDL_Rect background{x - 4, y - 4, static_cast<int>(text.size()) * 4 * scale + 6,
                              5 * scale + 8};
    SDL_RenderFillRect(renderer, &background);

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    int cursor_x = x;
    for (char ch : text) {
        if (ch == ' ') {
            cursor_x += 2 * scale;
            continue;
        }

        const char* rows = glyph_rows(ch);
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 3; ++col) {
                if (rows[row * 3 + col] != '1') {
                    continue;
                }
                const SDL_Rect rect{cursor_x + col * scale, y + row * scale, scale, scale};
                SDL_RenderFillRect(renderer, &rect);
            }
        }
        cursor_x += 4 * scale;
    }
}

} // namespace

int main(int argc, char** argv) {
    const auto parsed_args = parse_args(argc, argv);
    if (!parsed_args.has_value()) {
        print_usage();
        return 2;
    }
    const Args args = *parsed_args;

    ast::AssembleResult assembled = ast::assemble_file(args.program_path);
    if (!assembled.ok()) {
        print_diagnostics(assembled.diagnostics);
        return 1;
    }
    std::array<std::optional<ast::AssembleResult>, 4> buffer_programs;
    if (!assemble_buffers(args, buffer_programs)) {
        return 1;
    }

    if (args.dry_run && !has_channel_paths(args) && !has_video_paths(args) &&
        !has_webcam_paths(args) && !has_buffer_paths(args)) {
        std::cout << "ok: assembled " << args.program_path << '\n';
        return 0;
    }

    const std::uint32_t sdl_init_flags =
        args.no_graphics
            ? static_cast<std::uint32_t>(SDL_INIT_TIMER)
            : static_cast<std::uint32_t>(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);
    if (SDL_Init(sdl_init_flags) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    if (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG) == 0) {
        std::cerr << "IMG_Init failed: " << IMG_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    ast::ChannelSet channels;
    for (std::size_t i = 0; i < args.channel_paths.size(); ++i) {
        if (!args.channel_paths[i].empty() &&
            !load_channel_image(args.channel_paths[i], channels.image[i])) {
            IMG_Quit();
            SDL_Quit();
            return 1;
        }
    }

    std::array<VideoChannel, 4> video_channels;
    for (std::size_t i = 0; i < args.video_paths.size(); ++i) {
        if (!args.video_paths[i].empty() &&
            !load_video_channel(args.video_paths[i], video_channels[i])) {
            IMG_Quit();
            SDL_Quit();
            return 1;
        }
    }
    if (!update_video_channels(channels, video_channels, 0.0)) {
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    std::array<WebcamChannel, 4> webcam_channels;
    for (std::size_t i = 0; i < args.webcam_paths.size(); ++i) {
        if (!args.webcam_paths[i].empty() &&
            !open_webcam_channel(args.webcam_paths[i], webcam_channels[i])) {
            IMG_Quit();
            SDL_Quit();
            return 1;
        }
    }
    if (!update_webcam_channels(channels, webcam_channels, 0.0F)) {
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    if (args.dry_run) {
        std::cout << "ok: assembled " << args.program_path;
        if (has_channel_paths(args)) {
            std::cout << " and loaded image channels";
        }
        if (has_video_paths(args)) {
            std::cout << " and loaded video channels";
        }
        if (has_webcam_paths(args)) {
            std::cout << " and opened webcam channels";
        }
        if (has_buffer_paths(args)) {
            std::cout << " and assembled buffer passes";
        }
        std::cout << '\n';
        IMG_Quit();
        SDL_Quit();
        return 0;
    }

    if (args.no_graphics) {
        std::vector<std::uint32_t> pixels;
        std::array<std::vector<std::uint32_t>, 4> previous_buffers;
        std::array<std::vector<std::uint32_t>, 4> current_buffers;
        const bool use_buffers = has_buffer_paths(args);
        ensure_buffer_storage(args, previous_buffers, current_buffers);
        const int frame_count = args.frames >= 0 ? args.frames : 1;
        const auto measure_start = std::chrono::steady_clock::now();
        for (int frame = 0; frame < frame_count; ++frame) {
            const float time = static_cast<float>(frame) / 60.0F;
            if (!update_video_channels(channels, video_channels, static_cast<double>(time))) {
                IMG_Quit();
                SDL_Quit();
                return 1;
            }
            if (!update_webcam_channels(channels, webcam_channels, time)) {
                IMG_Quit();
                SDL_Quit();
                return 1;
            }
            if (use_buffers) {
                render_pipeline(args, assembled.program, buffer_programs, channels,
                                previous_buffers, current_buffers, frame, time, 1.0F / 60.0F,
                                pixels);
            } else {
                const ast::FrameInputs frame_inputs =
                    make_frame_inputs(args, channels, frame, time, 1.0F / 60.0F);
                ast::render_frame(assembled.program, frame_inputs, pixels,
                                  ast::RunLimits{args.max_steps});
            }
        }
        const auto measure_end = std::chrono::steady_clock::now();
        if (!args.save_frame_path.empty() &&
            !save_frame_png(args.save_frame_path, args.width, args.height, pixels)) {
            IMG_Quit();
            SDL_Quit();
            return 1;
        }
        if (args.measure_fps_frames > 0) {
            const std::chrono::duration<double> elapsed = measure_end - measure_start;
            const double seconds = elapsed.count();
            const double fps = seconds > 0.0 ? static_cast<double>(frame_count) / seconds : 0.0;
            const double ms_per_frame =
                seconds > 0.0 ? (seconds * 1000.0) / static_cast<double>(frame_count) : 0.0;
            std::cout << "frames: " << frame_count << '\n'
                      << "seconds: " << seconds << '\n'
                      << "average_fps: " << fps << '\n'
                      << "ms_per_frame: " << ms_per_frame << '\n';
            IMG_Quit();
            SDL_Quit();
            return 0;
        }
        std::cout << "ok: rendered " << frame_count << " frame";
        if (frame_count != 1) {
            std::cout << "s";
        }
        if (!args.save_frame_path.empty()) {
            std::cout << " to " << args.save_frame_path;
        }
        std::cout << " without graphics\n";
        IMG_Quit();
        SDL_Quit();
        return 0;
    }

    SDL_Window* window =
        SDL_CreateWindow("asm-shader-toy", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         args.width * args.scale, args.height * args.scale, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (renderer == nullptr) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                             SDL_TEXTUREACCESS_STREAMING, args.width, args.height);
    if (texture == nullptr) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);

    bool running = true;
    std::vector<std::uint32_t> pixels;
    std::array<std::vector<std::uint32_t>, 4> previous_buffers;
    std::array<std::vector<std::uint32_t>, 4> current_buffers;
    const bool use_buffers = has_buffer_paths(args);
    ensure_buffer_storage(args, previous_buffers, current_buffers);
    const auto start = std::chrono::steady_clock::now();
    auto previous_frame_time = start;
    auto previous_reload_check = start;
    FileWriteTimes dependency_snapshot =
        snapshot_dependencies(collect_dependencies(assembled, buffer_programs));
    int frame = 0;
    float fps_accumulator = 0.0F;
    int fps_frame_count = 0;
    float displayed_fps = 0.0F;
    float mouse_x = 0.0F;
    float mouse_y = 0.0F;
    float mouse_down = 0.0F;
    float mouse_click_x = 0.0F;
    float mouse_click_y = 0.0F;
    ast::InputState input_state;
    GameControllerHandle controller_handle;

    while (running) {
        input_state.mouse_wheel_x = 0.0F;
        input_state.mouse_wheel_y = 0.0F;
        SDL_Event event{};
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
            if (event.type == SDL_MOUSEMOTION) {
                mouse_x = static_cast<float>(event.motion.x) / static_cast<float>(args.scale);
                mouse_y = static_cast<float>(event.motion.y) / static_cast<float>(args.scale);
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                const int button_index = mouse_button_index(event.button.button);
                if (button_index >= 0 && button_index < ast::mouse_button_input_count) {
                    input_state.mouse_buttons[static_cast<std::size_t>(button_index)] = 1.0F;
                }
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouse_down = 1.0F;
                    mouse_click_x =
                        static_cast<float>(event.button.x) / static_cast<float>(args.scale);
                    mouse_click_y =
                        static_cast<float>(event.button.y) / static_cast<float>(args.scale);
                    mouse_x = mouse_click_x;
                    mouse_y = mouse_click_y;
                }
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                const int button_index = mouse_button_index(event.button.button);
                if (button_index >= 0 && button_index < ast::mouse_button_input_count) {
                    input_state.mouse_buttons[static_cast<std::size_t>(button_index)] = 0.0F;
                }
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouse_down = 0.0F;
                }
            }
            if (event.type == SDL_MOUSEWHEEL) {
                input_state.mouse_wheel_x += static_cast<float>(event.wheel.x);
                input_state.mouse_wheel_y += static_cast<float>(event.wheel.y);
            }
        }
        update_keyboard_input(input_state);
        update_gamepad_input(controller_handle, input_state);

        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<float> elapsed = now - start;
        const std::chrono::duration<float> delta = now - previous_frame_time;
        if (!update_video_channels(channels, video_channels,
                                   static_cast<double>(elapsed.count()))) {
            running = false;
            break;
        }
        if (!update_webcam_channels(channels, webcam_channels, elapsed.count())) {
            running = false;
            break;
        }
        previous_frame_time = now;
        fps_accumulator += std::max(delta.count(), 0.0F);
        ++fps_frame_count;
        if (fps_accumulator >= 0.5F) {
            displayed_fps = static_cast<float>(fps_frame_count) / fps_accumulator;
            fps_accumulator = 0.0F;
            fps_frame_count = 0;
        }

        if (now - previous_reload_check >= std::chrono::milliseconds{250}) {
            previous_reload_check = now;
            if (dependencies_changed(dependency_snapshot)) {
                ast::AssembleResult reloaded = ast::assemble_file(args.program_path);
                std::array<std::optional<ast::AssembleResult>, 4> reloaded_buffers;
                const bool buffers_ok = assemble_buffers(args, reloaded_buffers);
                if (reloaded.ok() && buffers_ok) {
                    assembled = std::move(reloaded);
                    buffer_programs = std::move(reloaded_buffers);
                    dependency_snapshot =
                        snapshot_dependencies(collect_dependencies(assembled, buffer_programs));
                    std::cout << "hot reloaded " << args.program_path << '\n';
                } else {
                    print_diagnostics(reloaded.diagnostics);
                    dependency_snapshot =
                        snapshot_dependencies(collect_dependencies(assembled, buffer_programs));
                }
            }
        }

        if (use_buffers) {
            render_pipeline(args, assembled.program, buffer_programs, channels, previous_buffers,
                            current_buffers, frame, elapsed.count(), delta.count(), pixels, mouse_x,
                            mouse_y, mouse_down, mouse_click_x, mouse_click_y, &input_state);
        } else {
            const ast::FrameInputs frame_inputs =
                make_frame_inputs(args, channels, frame, elapsed.count(), delta.count(), mouse_x,
                                  mouse_y, mouse_down, mouse_click_x, mouse_click_y, &input_state);
            ast::render_frame(assembled.program, frame_inputs, pixels,
                              ast::RunLimits{args.max_steps});
        }
        ++frame;
        if (args.frames >= 0 && frame >= args.frames) {
            running = false;
        }

        SDL_UpdateTexture(texture, nullptr, pixels.data(),
                          args.width * static_cast<int>(sizeof(std::uint32_t)));
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        if (args.show_fps) {
            char fps_text[32]{};
            std::snprintf(fps_text, sizeof(fps_text), "FPS: %.1f",
                          static_cast<double>(displayed_fps));
            draw_debug_text(renderer, fps_text, 12, 12, 3, SDL_Color{235, 245, 210, 255});
        }
        SDL_RenderPresent(renderer);
    }

    controller_handle.close();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}
