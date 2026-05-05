#pragma once

#include "data/GameBalance.hpp"
#include <array>
#include <cstddef>

namespace majo {

enum class TileType : unsigned char {
    Empty = 0,
    Dirt = 1,
    Rock = 2,
    Ore = 3,
    HardRock = 4
};

struct Tile {
    TileType type = TileType::Dirt;
    unsigned char hp = balance::DirtHp;
    unsigned char flash = 0;
};

struct Chunk {
    int cx = 0;
    int cy = 0;
    std::array<Tile, balance::ChunkTiles * balance::ChunkTiles> tiles{};

    Tile& at(int x, int y) { return tiles[static_cast<std::size_t>(y * balance::ChunkTiles + x)]; }
    const Tile& at(int x, int y) const { return tiles[static_cast<std::size_t>(y * balance::ChunkTiles + x)]; }
};

}
