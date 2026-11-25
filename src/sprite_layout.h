#pragma once

#include "sprite_sheet_builder.h"
#include "sprite_types.h"

#include <string>
#include <vector>

struct SpriteLayoutItem {
    std::string name;
    int sourceIndex = -1;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    std::vector<unsigned char> pixels;
};

struct SpriteLayout {
    int atlasWidth = 0;
    int atlasHeight = 0;
    std::vector<SpriteLayoutItem> items;

    [[nodiscard]] bool valid() const
    {
        return atlasWidth > 0 && atlasHeight > 0 && !items.empty();
    }
};

struct SpriteLayoutSettings {
    bool snapToGrid = true;
    int gridSize = 1;
    bool preventOverlap = true;
    bool preventOutOfBounds = true;
};

struct SpriteLayoutValidation {
    bool valid = true;
    bool outOfBounds = false;
    bool overlaps = false;
    std::vector<int> overlappingItems;
};

int snapCoordinate(int value, int gridSize);
bool rectsOverlap(const SpriteRect& a, const SpriteRect& b);
bool rectInsideAtlas(const SpriteRect& rect, int atlasWidth, int atlasHeight);
SpriteRect layoutItemRect(const SpriteLayoutItem& item);
SpriteLayoutValidation validateLayoutItemMove(const SpriteLayout& layout, int itemIndex, int candidateX, int candidateY, const SpriteLayoutSettings& settings);
bool moveLayoutItem(SpriteLayout& layout, int itemIndex, int newX, int newY, const SpriteLayoutSettings& settings, SpriteLayoutValidation* outValidation = nullptr);
SpriteLayout createSpriteLayoutFromSprites(int imageWidth, int imageHeight, const std::vector<unsigned char>& imagePixels, const std::vector<SpriteRect>& sprites, const ColorKeySettings& colorKey);
PackedSheet buildSheetFromLayout(const SpriteLayout& layout);
SpriteLayout autoRepackLayout(const SpriteLayout& layout, const SheetPackingSettings& packingSettings);
