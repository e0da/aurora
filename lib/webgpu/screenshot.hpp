#pragma once

#include "wgpu.hpp"

namespace aurora::screenshot {

void encode(const wgpu::CommandEncoder& encoder, const wgpu::Texture& composed);
void after_submit();

} // namespace aurora::screenshot
