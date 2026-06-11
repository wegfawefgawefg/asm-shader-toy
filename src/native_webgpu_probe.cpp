#include <webgpu/webgpu.h>

#include <iostream>

int main() {
    WGPUInstanceDescriptor descriptor{};
    std::cout << "native WebGPU headers linked; descriptor size=" << sizeof(descriptor) << '\n';
    return 0;
}
