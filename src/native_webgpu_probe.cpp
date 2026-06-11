#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>

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

wgpu::ShaderModule make_shader_module(wgpu::Device device) {
    wgpu::ShaderModuleWGSLDescriptor wgsl{};
    wgsl.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    wgsl.code = smoke_shader;

    wgpu::ShaderModuleDescriptor descriptor{};
    descriptor.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl.chain);
    return device.createShaderModule(descriptor);
}

} // namespace

int main() {
    wgpu::InstanceDescriptor instance_descriptor{};
    wgpu::Instance instance = wgpu::createInstance(instance_descriptor);
    if (instance == WGPUInstance(nullptr)) {
        std::cerr << "could not create WebGPU instance\n";
        return 1;
    }

    wgpu::RequestAdapterOptions adapter_options{};
    wgpu::Adapter adapter = instance.requestAdapter(adapter_options);
    if (adapter == WGPUAdapter(nullptr)) {
        std::cerr << "could not request WebGPU adapter\n";
        return 1;
    }

    wgpu::DeviceDescriptor device_descriptor{};
    wgpu::Device device = adapter.requestDevice(device_descriptor);
    if (device == WGPUDevice(nullptr)) {
        std::cerr << "could not request WebGPU device\n";
        return 1;
    }

    auto error_callback = device.setUncapturedErrorCallback(
        [](wgpu::ErrorType type, char const* message) {
            std::cerr << "WebGPU error " << static_cast<int>(type) << ": "
                      << (message != nullptr ? message : "<no message>") << '\n';
        });

    constexpr std::uint64_t data_size = sizeof(std::uint32_t) * 4;
    wgpu::BufferDescriptor storage_descriptor{};
    storage_descriptor.size = data_size;
    storage_descriptor.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
    wgpu::Buffer storage = device.createBuffer(storage_descriptor);

    wgpu::BufferDescriptor readback_descriptor{};
    readback_descriptor.size = data_size;
    readback_descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    wgpu::Buffer readback = device.createBuffer(readback_descriptor);

    wgpu::BindGroupLayoutEntry layout_entry{};
    layout_entry.binding = 0;
    layout_entry.visibility = wgpu::ShaderStage::Compute;
    layout_entry.buffer.type = wgpu::BufferBindingType::Storage;

    wgpu::BindGroupLayoutDescriptor layout_descriptor{};
    layout_descriptor.entryCount = 1;
    layout_descriptor.entries = &layout_entry;
    wgpu::BindGroupLayout bind_group_layout = device.createBindGroupLayout(layout_descriptor);

    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor{};
    WGPUBindGroupLayout raw_bind_group_layout = bind_group_layout;
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &raw_bind_group_layout;
    wgpu::PipelineLayout pipeline_layout = device.createPipelineLayout(pipeline_layout_descriptor);

    wgpu::ShaderModule shader_module = make_shader_module(device);

    wgpu::ComputePipelineDescriptor pipeline_descriptor{};
    pipeline_descriptor.layout = pipeline_layout;
    pipeline_descriptor.compute.module = shader_module;
    pipeline_descriptor.compute.entryPoint = "main";
    wgpu::ComputePipeline pipeline = device.createComputePipeline(pipeline_descriptor);
    if (pipeline == WGPUComputePipeline(nullptr)) {
        std::cerr << "could not create WebGPU compute pipeline\n";
        return 1;
    }

    wgpu::BindGroupEntry bind_entry{};
    bind_entry.binding = 0;
    bind_entry.buffer = storage;
    bind_entry.size = data_size;

    wgpu::BindGroupDescriptor bind_descriptor{};
    bind_descriptor.layout = bind_group_layout;
    bind_descriptor.entryCount = 1;
    bind_descriptor.entries = &bind_entry;
    wgpu::BindGroup bind_group = device.createBindGroup(bind_descriptor);

    wgpu::CommandEncoder encoder = device.createCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.beginComputePass();
    pass.setPipeline(pipeline);
    pass.setBindGroup(0, bind_group, 0, nullptr);
    pass.dispatchWorkgroups(1, 1, 1);
    pass.end();
    encoder.copyBufferToBuffer(storage, 0, readback, 0, data_size);
    wgpu::CommandBuffer commands = encoder.finish();

    wgpu::Queue queue = device.getQueue();
    queue.submit(commands);

    bool mapped = false;
    bool map_failed = false;
    auto map_callback = readback.mapAsync(wgpu::MapMode::Read, 0, data_size,
        [&](wgpu::BufferMapAsyncStatus status) {
            mapped = true;
            map_failed = status != wgpu::BufferMapAsyncStatus::Success;
        });
    while (!mapped) {
        device.poll(true);
    }
    if (map_failed) {
        std::cerr << "WebGPU readback map failed\n";
        return 1;
    }

    std::array<std::uint32_t, 4> values{};
    const void* mapped_data = readback.getConstMappedRange(0, data_size);
    std::memcpy(values.data(), mapped_data, data_size);
    readback.unmap();

    const std::array<std::uint32_t, 4> expected{3, 20, 37, 54};
    if (values != expected) {
        std::cerr << "unexpected WebGPU compute result: " << values[0] << ", " << values[1]
                  << ", " << values[2] << ", " << values[3] << '\n';
        return 1;
    }

    (void)error_callback;
    (void)map_callback;
    std::cout << "native WebGPU compute smoke passed: " << values[0] << ", " << values[1]
              << ", " << values[2] << ", " << values[3] << '\n';
    return 0;
}
