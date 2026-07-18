#pragma once
#include "../world/BlockType.h"
#include <array>
#include <cstdint>

namespace vc {

// 9-slot hotbar
struct Inventory {
    std::array<BlockId, 9> hotbar = {
        BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE, BLOCK_COBBLE,
        BLOCK_PLANKS, BLOCK_WOOD, BLOCK_LEAVES, BLOCK_SAND,
        BLOCK_GLASS
    };
    int selected = 0;

    BlockId current() const { return hotbar[selected]; }
    void select(int slot) {
        if (slot < 0) slot = 0;
        if (slot > 8) slot = 8;
        selected = slot;
    }
    void cycle(int delta) {
        selected = (selected + delta + 9) % 9;
    }
    void setSlot(int slot, BlockId b) {
        if (slot >= 0 && slot < 9) hotbar[slot] = b;
    }
};

} // namespace vc
