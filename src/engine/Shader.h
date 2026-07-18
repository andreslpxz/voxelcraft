#pragma once
#include "VulkanContext.h"
#include <vector>
#include <cstdint>

namespace vc {

// Create a VkShaderModule from SPIR-V bytes
VkShaderModule createShaderModule(const VulkanContext& ctx, const uint32_t* code, size_t bytes);

#ifdef VOXELCRAFT_EMBEDDED_SHADERS
// Generated header symbols (e.g. block_vert_spv / block_vert_spv_len)
#include "block.vert.h"
#include "block.frag.h"
#include "sky.vert.h"
#include "sky.frag.h"
#include "ui.vert.h"
#include "ui.frag.h"
#endif

} // namespace vc
