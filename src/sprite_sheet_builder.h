#pragma once

#include "sprite_types.h"

#include <vector>

enum class PackingAlgorithm {
    FixedColumns,
    AutoGrid,
    Shelf,
    Skyline,
    Guillotine,
    MaxRects
};

enum class PackingSortMode {
    Original,
    HeightDesc,
    AreaDesc
};

struct SheetPackingSettings {
    PackingAlgorithm algorithm = PackingAlgorithm::FixedColumns;
    int columns = 8;
    int padding = 2;
    int margin = 0;
    bool useCustomCellSize = false;
    int customCellWidth = 32;
    int customCellHeight = 32;
    int extrudeBorder = 0;
    int maxSheetWidth = 1024;
    PackingSortMode sortMode = PackingSortMode::Original;
};

int maxSourceImageWidth(const std::vector<SourceImage>& images);
int maxSourceImageHeight(const std::vector<SourceImage>& images);
PackedSheet buildSpriteSheet(const std::vector<SourceImage>& images, int columns, int padding, int margin, int cellWidth, int cellHeight);
PackedSheet buildSpriteSheet(const std::vector<SourceImage>& images, const SheetPackingSettings& settings);
