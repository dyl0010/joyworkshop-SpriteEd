#include "sprite_layout.h"

#include "image_ops.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

int snapCoordinate(int value, int gridSize)
{
    if (gridSize <= 1) {
        return value;
    }
    return static_cast<int>(std::lround(static_cast<double>(value) / static_cast<double>(gridSize))) * gridSize;
}

bool rectsOverlap(const SpriteRect& a, const SpriteRect& b)
{
    return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

bool rectInsideAtlas(const SpriteRect& rect, int atlasWidth, int atlasHeight)
{
    return rect.x >= 0 && rect.y >= 0 && rect.w > 0 && rect.h > 0 && rect.x + rect.w <= atlasWidth && rect.y + rect.h <= atlasHeight;
}

SpriteRect layoutItemRect(const SpriteLayoutItem& item)
{
    SpriteRect rect;
    rect.name = item.name;
    rect.x = item.x;
    rect.y = item.y;
    rect.w = item.w;
    rect.h = item.h;
    return rect;
}

SpriteLayoutValidation validateLayoutItemMove(const SpriteLayout& layout, int itemIndex, int candidateX, int candidateY, const SpriteLayoutSettings& settings)
{
    SpriteLayoutValidation validation;
    if (itemIndex < 0 || itemIndex >= static_cast<int>(layout.items.size())) {
        validation.valid = false;
        validation.outOfBounds = true;
        return validation;
    }

    SpriteRect candidate = layoutItemRect(layout.items[static_cast<size_t>(itemIndex)]);
    candidate.x = settings.snapToGrid ? snapCoordinate(candidateX, settings.gridSize) : candidateX;
    candidate.y = settings.snapToGrid ? snapCoordinate(candidateY, settings.gridSize) : candidateY;

    if (settings.preventOutOfBounds && !rectInsideAtlas(candidate, layout.atlasWidth, layout.atlasHeight)) {
        validation.valid = false;
        validation.outOfBounds = true;
    }

    if (settings.preventOverlap) {
        for (int i = 0; i < static_cast<int>(layout.items.size()); ++i) {
            if (i == itemIndex) {
                continue;
            }
            if (rectsOverlap(candidate, layoutItemRect(layout.items[static_cast<size_t>(i)]))) {
                validation.valid = false;
                validation.overlaps = true;
                validation.overlappingItems.push_back(i);
            }
        }
    }

    return validation;
}

bool moveLayoutItem(SpriteLayout& layout, int itemIndex, int newX, int newY, const SpriteLayoutSettings& settings, SpriteLayoutValidation* outValidation)
{
    SpriteLayoutValidation validation = validateLayoutItemMove(layout, itemIndex, newX, newY, settings);
    if (outValidation != nullptr) {
        *outValidation = validation;
    }
    if (!validation.valid) {
        return false;
    }

    SpriteLayoutItem& item = layout.items[static_cast<size_t>(itemIndex)];
    item.x = settings.snapToGrid ? snapCoordinate(newX, settings.gridSize) : newX;
    item.y = settings.snapToGrid ? snapCoordinate(newY, settings.gridSize) : newY;
    return true;
}

SpriteLayout createSpriteLayoutFromSprites(int imageWidth, int imageHeight, const std::vector<unsigned char>& imagePixels, const std::vector<SpriteRect>& sprites, const ColorKeySettings& colorKey)
{
    SpriteLayout layout;
    layout.atlasWidth = imageWidth;
    layout.atlasHeight = imageHeight;
    if (imageWidth <= 0 || imageHeight <= 0 || imagePixels.empty()) {
        return layout;
    }

    layout.items.reserve(sprites.size());
    for (int i = 0; i < static_cast<int>(sprites.size()); ++i) {
        const SpriteRect& sprite = sprites[static_cast<size_t>(i)];
        if (!rectInsideAtlas(sprite, imageWidth, imageHeight)) {
            continue;
        }

        int croppedW = 0;
        int croppedH = 0;
        std::vector<unsigned char> cropped = cropSpritePixels(imageWidth, imageHeight, imagePixels, sprite, croppedW, croppedH, colorKey);
        if (cropped.empty() || croppedW <= 0 || croppedH <= 0) {
            continue;
        }

        SpriteLayoutItem item;
        item.name = sprite.name.empty() ? ("sprite_" + std::to_string(i)) : sprite.name;
        item.sourceIndex = i;
        item.x = sprite.x;
        item.y = sprite.y;
        item.w = croppedW;
        item.h = croppedH;
        item.pixels = std::move(cropped);
        layout.items.push_back(std::move(item));
    }
    return layout;
}

PackedSheet buildSheetFromLayout(const SpriteLayout& layout)
{
    PackedSheet sheet;
    if (!layout.valid()) {
        return sheet;
    }

    sheet.width = layout.atlasWidth;
    sheet.height = layout.atlasHeight;
    sheet.pixels.assign(static_cast<size_t>(sheet.width) * static_cast<size_t>(sheet.height) * 4, 0);
    sheet.sprites.reserve(layout.items.size());

    // 算法原理：Layout 应用阶段不直接修改旧 atlas，而是把每个已裁剪 sprite 像素按 layout item 的目标坐标重新绘制到一张新的透明 atlas 上。
    for (const SpriteLayoutItem& item : layout.items) {
        if (!rectInsideAtlas(layoutItemRect(item), layout.atlasWidth, layout.atlasHeight) || item.pixels.size() != static_cast<size_t>(item.w) * static_cast<size_t>(item.h) * 4) {
            return {};
        }
        for (int y = 0; y < item.h; ++y) {
            const size_t srcOffset = static_cast<size_t>(y) * static_cast<size_t>(item.w) * 4;
            const size_t dstOffset = (static_cast<size_t>(item.y + y) * static_cast<size_t>(sheet.width) + static_cast<size_t>(item.x)) * 4;
            std::copy_n(item.pixels.data() + srcOffset, static_cast<size_t>(item.w) * 4, sheet.pixels.data() + dstOffset);
        }

        SpriteRect sprite;
        sprite.name = item.name;
        sprite.x = item.x;
        sprite.y = item.y;
        sprite.w = item.w;
        sprite.h = item.h;
        sheet.sprites.push_back(std::move(sprite));
    }
    return sheet;
}

SpriteLayout autoRepackLayout(const SpriteLayout& layout, const SheetPackingSettings& packingSettings)
{
    if (!layout.valid()) {
        return {};
    }

    std::vector<SourceImage> sources;
    sources.reserve(layout.items.size());
    for (const SpriteLayoutItem& item : layout.items) {
        if (item.w <= 0 || item.h <= 0 || item.pixels.size() != static_cast<size_t>(item.w) * static_cast<size_t>(item.h) * 4) {
            continue;
        }
        SourceImage source;
        source.name = item.name;
        source.width = item.w;
        source.height = item.h;
        source.pixels = item.pixels;
        sources.push_back(std::move(source));
    }
    if (sources.empty()) {
        return {};
    }

    SheetPackingSettings settings = packingSettings;
    settings.sortMode = PackingSortMode::Original;
    const PackedSheet packed = buildSpriteSheet(sources, settings);
    if (!packed.valid() || packed.sprites.size() != layout.items.size()) {
        return {};
    }

    SpriteLayout repacked;
    repacked.atlasWidth = packed.width;
    repacked.atlasHeight = packed.height;
    repacked.items.reserve(layout.items.size());
    for (int i = 0; i < static_cast<int>(layout.items.size()); ++i) {
        SpriteLayoutItem item = layout.items[static_cast<size_t>(i)];
        const SpriteRect& packedSprite = packed.sprites[static_cast<size_t>(i)];
        item.x = packedSprite.x;
        item.y = packedSprite.y;
        item.w = packedSprite.w;
        item.h = packedSprite.h;
        item.name = packedSprite.name;
        repacked.items.push_back(std::move(item));
    }
    return repacked;
}
