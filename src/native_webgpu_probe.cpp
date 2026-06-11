#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include "ast/assembler.hpp"
#include "ast/wgsl.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr const char* smoke_shader = R"wgsl(
@group(0) @binding(0) var<storage, read_write> out_data: array<u32, 4>;

@compute @workgroup_size(4)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    if (gid.x < 4u) {
        out_data[gid.x] = gid.x * 17u + 3u;
    }
}
)wgsl";

constexpr const char* asm_probe_source = R"asm(
.include <std/screen.inc>
tex r40, r41, r42, r43, 0, uv_x, uv_y
tex r44, r45, r46, r47, 1, uv_x, uv_y
tex r48, r49, r50, r51, 2, uv_x, uv_y
tex r52, r53, r54, r55, 3, uv_x, uv_y
out uv_x, uv_y, 0.25, 1.0
)asm";

constexpr std::uint32_t probe_width = 4;
constexpr std::uint32_t probe_height = 4;
constexpr std::uint64_t uniform_channel_offset = 16;
constexpr std::uint64_t uniform_float_count = uniform_channel_offset + 16 + 512 + 8 + 4 + 32 + 16;
constexpr std::uint64_t uniform_payload_byte_size = uniform_float_count * sizeof(float);
constexpr std::uint64_t uniform_byte_size = (uniform_payload_byte_size + 15) & ~std::uint64_t{15};

struct WebGpuContext {
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;
};

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
    auto map_callback = buffer.mapAsync(wgpu::MapMode::Read, 0, size,
        [&](wgpu::BufferMapAsyncStatus status) {
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

bool run_storage_buffer_smoke(WebGpuContext& context) {
    constexpr std::uint64_t data_size = sizeof(std::uint32_t) * 4;
    wgpu::BufferDescriptor storage_descriptor{};
    storage_descriptor.size = data_size;
    storage_descriptor.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
    wgpu::Buffer storage = context.device.createBuffer(storage_descriptor);

    wgpu::BufferDescriptor readback_descriptor{};
    readback_descriptor.size = data_size;
    readback_descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    wgpu::Buffer readback = context.device.createBuffer(readback_descriptor);

    wgpu::BindGroupLayoutEntry layout_entry{};
    layout_entry.binding = 0;
    layout_entry.visibility = wgpu::ShaderStage::Compute;
    layout_entry.buffer.type = wgpu::BufferBindingType::Storage;

    wgpu::BindGroupLayoutDescriptor layout_descriptor{};
    layout_descriptor.entryCount = 1;
    layout_descriptor.entries = &layout_entry;
    wgpu::BindGroupLayout bind_group_layout = context.device.createBindGroupLayout(layout_descriptor);

    WGPUBindGroupLayout raw_bind_group_layout = bind_group_layout;
    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor{};
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &raw_bind_group_layout;
    wgpu::PipelineLayout pipeline_layout = context.device.createPipelineLayout(pipeline_layout_descriptor);

    wgpu::ComputePipelineDescriptor pipeline_descriptor{};
    pipeline_descriptor.layout = pipeline_layout;
    pipeline_descriptor.compute.module = make_shader_module(context.device, smoke_shader);
    pipeline_descriptor.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = context.device.createComputePipeline(pipeline_descriptor);
    if (pipeline == WGPUComputePipeline(nullptr)) {
        std::cerr << "could not create WebGPU compute pipeline\n";
        return false;
    }

    wgpu::BindGroupEntry bind_entry{};
    bind_entry.binding = 0;
    bind_entry.buffer = storage;
    bind_entry.size = data_size;

    wgpu::BindGroupDescriptor bind_descriptor{};
    bind_descriptor.layout = bind_group_layout;
    bind_descriptor.entryCount = 1;
    bind_descriptor.entries = &bind_entry;
    wgpu::BindGroup bind_group = context.device.createBindGroup(bind_descriptor);

    wgpu::CommandEncoder encoder = context.device.createCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.beginComputePass();
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bind_group, 0, nullptr);
    pass.dispatchWorkgroups(1, 1, 1);
    pass.end();
    encoder.copyBufferToBuffer(storage, 0, readback, 0, data_size);
    context.queue.submit(encoder.finish());

    const std::vector<std::uint8_t> bytes = map_readback(context.device, readback, data_size);
    if (bytes.size() != data_size) {
        std::cerr << "WebGPU buffer readback map failed\n";
        return false;
    }

    std::array<std::uint32_t, 4> values{};
    std::memcpy(values.data(), bytes.data(), bytes.size());
    const std::array<std::uint32_t, 4> expected{3, 20, 37, 54};
    if (values != expected) {
        std::cerr << "unexpected WebGPU compute result: " << values[0] << ", " << values[1]
                  << ", " << values[2] << ", " << values[3] << '\n';
        return false;
    }
    return true;
}

wgpu::Texture make_fallback_channel_texture(WebGpuContext& context) {
    wgpu::TextureDescriptor descriptor{wgpu::Default};
    descriptor.size = wgpu::Extent3D(1, 1, 1);
    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    descriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    wgpu::Texture texture = context.device.createTexture(descriptor);

    const std::array<std::uint8_t, 4> white{255, 255, 255, 255};
    wgpu::ImageCopyTexture destination{};
    destination.texture = texture;
    wgpu::TextureDataLayout layout{};
    layout.bytesPerRow = 256;
    layout.rowsPerImage = 1;
    context.queue.writeTexture(destination, white.data(), white.size(), layout,
                               wgpu::Extent3D(1, 1, 1));
    return texture;
}

std::vector<float> make_uniforms() {
    std::vector<float> values(static_cast<std::size_t>(uniform_float_count), 0.0F);
    values[1] = 1.0F / 60.0F;
    values[3] = static_cast<float>(probe_width);
    values[4] = static_cast<float>(probe_height);
    values[13] = 1.0F;
    for (std::size_t channel = 0; channel < 4; ++channel) {
        const std::size_t offset = static_cast<std::size_t>(uniform_channel_offset) + channel * 4;
        values[offset] = 1.0F;
        values[offset + 1] = 1.0F;
    }
    return values;
}

bool close_u8(std::uint8_t actual, std::uint8_t expected) {
    const int delta = static_cast<int>(actual) - static_cast<int>(expected);
    return std::abs(delta) <= 1;
}

bool run_asm_wgsl_texture_smoke(WebGpuContext& context) {
    ast::AssembleResult assembled = ast::assemble_source(asm_probe_source, "native-webgpu-probe.asm");
    if (!assembled.ok()) {
        for (const ast::Diagnostic& diagnostic : assembled.diagnostics) {
            std::cerr << diagnostic.file << ':' << diagnostic.line << ": " << diagnostic.message
                      << '\n';
        }
        return false;
    }

    const ast::WgslCompileResult compiled = ast::compile_wgsl(assembled.program);
    if (!compiled.ok()) {
        for (const ast::Diagnostic& diagnostic : compiled.diagnostics) {
            std::cerr << diagnostic.file << ':' << diagnostic.line << ": " << diagnostic.message
                      << '\n';
        }
        return false;
    }

    wgpu::TextureDescriptor output_descriptor{wgpu::Default};
    output_descriptor.size = wgpu::Extent3D(probe_width, probe_height, 1);
    output_descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    output_descriptor.usage = wgpu::TextureUsage::StorageBinding | wgpu::TextureUsage::CopySrc;
    wgpu::Texture output = context.device.createTexture(output_descriptor);
    wgpu::TextureView output_view = output.createView();

    wgpu::BufferDescriptor uniform_descriptor{};
    uniform_descriptor.size = uniform_byte_size;
    uniform_descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer uniform = context.device.createBuffer(uniform_descriptor);
    const std::vector<float> uniforms = make_uniforms();
    context.queue.writeBuffer(uniform, 0, uniforms.data(), uniform_payload_byte_size);

    std::array<wgpu::Texture, 4> channels{
        make_fallback_channel_texture(context),
        make_fallback_channel_texture(context),
        make_fallback_channel_texture(context),
        make_fallback_channel_texture(context),
    };
    std::array<wgpu::TextureView, 4> channel_views{
        channels[0].createView(),
        channels[1].createView(),
        channels[2].createView(),
        channels[3].createView(),
    };

    wgpu::ComputePipelineDescriptor pipeline_descriptor{};
    pipeline_descriptor.compute.module = make_shader_module(context.device, compiled.source.c_str());
    pipeline_descriptor.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = context.device.createComputePipeline(pipeline_descriptor);
    if (pipeline == WGPUComputePipeline(nullptr)) {
        std::cerr << "could not create emitted asm WGSL compute pipeline\n";
        return false;
    }

    wgpu::BindGroupLayout bind_group_layout = pipeline.getBindGroupLayout(0);
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

    constexpr std::uint32_t bytes_per_row = 256;
    constexpr std::uint64_t readback_size = bytes_per_row * probe_height;
    wgpu::BufferDescriptor readback_descriptor{};
    readback_descriptor.size = readback_size;
    readback_descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    wgpu::Buffer readback = context.device.createBuffer(readback_descriptor);

    wgpu::CommandEncoder encoder = context.device.createCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.beginComputePass();
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bind_group, 0, nullptr);
    pass.dispatchWorkgroups(1, 1, 1);
    pass.end();

    wgpu::ImageCopyTexture source{};
    source.texture = output;
    wgpu::ImageCopyBuffer destination{};
    destination.buffer = readback;
    destination.layout.bytesPerRow = bytes_per_row;
    destination.layout.rowsPerImage = probe_height;
    encoder.copyTextureToBuffer(source, destination, wgpu::Extent3D(probe_width, probe_height, 1));
    context.queue.submit(encoder.finish());

    const std::vector<std::uint8_t> bytes =
        map_readback(context.device, readback, readback_size);
    if (bytes.size() != readback_size) {
        std::cerr << "WebGPU texture readback map failed\n";
        return false;
    }

    const auto pixel_at = [&](std::uint32_t x, std::uint32_t y) -> const std::uint8_t* {
        return bytes.data() + y * bytes_per_row + x * 4;
    };
    const std::uint8_t* top_left = pixel_at(0, 0);
    const std::uint8_t* bottom_right = pixel_at(3, 3);
    if (!close_u8(top_left[0], 0) || !close_u8(top_left[1], 0) ||
        !close_u8(top_left[2], 64) || !close_u8(top_left[3], 255) ||
        !close_u8(bottom_right[0], 191) || !close_u8(bottom_right[1], 191) ||
        !close_u8(bottom_right[2], 64) || !close_u8(bottom_right[3], 255)) {
        std::cerr << "unexpected emitted WGSL pixels: top-left "
                  << static_cast<int>(top_left[0]) << ',' << static_cast<int>(top_left[1])
                  << ',' << static_cast<int>(top_left[2]) << ','
                  << static_cast<int>(top_left[3]) << " bottom-right "
                  << static_cast<int>(bottom_right[0]) << ','
                  << static_cast<int>(bottom_right[1]) << ','
                  << static_cast<int>(bottom_right[2]) << ','
                  << static_cast<int>(bottom_right[3]) << '\n';
        return false;
    }
    return true;
}

} // namespace

int main() {
    WebGpuContext context;
    if (!make_context(context)) {
        return 1;
    }

    auto error_callback = context.device.setUncapturedErrorCallback(
        [](wgpu::ErrorType type, char const* message) {
            std::cerr << "WebGPU error " << static_cast<int>(type) << ": "
                      << (message != nullptr ? message : "<no message>") << '\n';
        });

    if (!run_storage_buffer_smoke(context)) {
        return 1;
    }
    if (!run_asm_wgsl_texture_smoke(context)) {
        return 1;
    }

    (void)error_callback;
    std::cout << "native WebGPU emitted asm WGSL smoke passed\n";
    return 0;
}
