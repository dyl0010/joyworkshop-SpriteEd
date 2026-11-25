#include "sprite_sheet_builder.h"

#include <algorithm>
#include <string>

int maxSourceImageWidth(const std::vector<SourceImage>& images)
{
    int result = 1;
    for (const SourceImage& image : images) {
        result = std::max(result, image.width);
    }
    return result;
}

int maxSourceImageHeight(const std::vector<SourceImage>& images)
{
    int result = 1;
    for (const SourceImage& image : images) {
        result = std::max(result, image.height);
    }
    return result;
}

static void copyImageWithExtrudedBorder(const SourceImage& image, PackedSheet& sheet, int spriteX, int spriteY, int extrude);

static PackedSheet buildFixedGridSheet(const std::vector<SourceImage>& images, int columns, int padding, int margin, int cellWidth, int cellHeight, int extrude)
{
    PackedSheet sheet;
    if (images.empty()) {
        return sheet;
    }

    columns = std::clamp(columns, 1, static_cast<int>(images.size()));
    padding = std::max(0, padding);
    margin = std::max(0, margin);
    extrude = std::max(0, extrude);
    cellWidth = std::max(1, cellWidth);
    cellHeight = std::max(1, cellHeight);
    for (const SourceImage& image : images) {
        if (image.width <= 0 || image.height <= 0 || image.pixels.size() != static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * 4) {
            return PackedSheet{};
        }
        cellWidth = std::max(cellWidth, image.width);
        cellHeight = std::max(cellHeight, image.height);
    }

    const int footprintW = cellWidth + extrude * 2;
    const int footprintH = cellHeight + extrude * 2;
    const int rows = (static_cast<int>(images.size()) + columns - 1) / columns;
    sheet.width = margin * 2 + columns * footprintW + (columns - 1) * padding;
    sheet.height = margin * 2 + rows * footprintH + (rows - 1) * padding;
    sheet.pixels.assign(static_cast<size_t>(sheet.width) * static_cast<size_t>(sheet.height) * 4, 0);
    sheet.sprites.reserve(images.size());
    sheet.packingRects.reserve(images.size());

    for (int i = 0; i < static_cast<int>(images.size()); ++i) {
        const SourceImage& image = images[static_cast<size_t>(i)];
        const int column = i % columns;
        const int row = i / columns;
        const int footprintX = margin + column * (footprintW + padding);
        const int footprintY = margin + row * (footprintH + padding);
        const int spriteX = footprintX + extrude;
        const int spriteY = footprintY + extrude;

        copyImageWithExtrudedBorder(image, sheet, spriteX, spriteY, extrude);

        SpriteRect packingRect;
        packingRect.name = "cell_" + std::to_string(i);
        packingRect.x = footprintX;
        packingRect.y = footprintY;
        packingRect.w = footprintW;
        packingRect.h = footprintH;
        sheet.packingRects.push_back(std::move(packingRect));

        SpriteRect sprite;
        sprite.name = image.name.empty() ? ("sprite_" + std::to_string(i)) : image.name;
        sprite.x = spriteX;
        sprite.y = spriteY;
        sprite.w = image.width;
        sprite.h = image.height;
        sheet.sprites.push_back(std::move(sprite));
    }

    return sheet;
}

PackedSheet buildSpriteSheet(const std::vector<SourceImage>& images, int columns, int padding, int margin, int cellWidth, int cellHeight)
{
    return buildFixedGridSheet(images, columns, padding, margin, cellWidth, cellHeight, 0);
}

static int autoGridColumnCount(const std::vector<SourceImage>& images, int padding, int margin, int cellWidth, int cellHeight)
{
    // Auto Grid 算法原理：
    // 将每张源图视为占用一个统一网格单元，然后从 1 到 N 枚举所有可能的列数。
    // 对每个候选列数，计算所需行数，以及包含 margin 和 padding 后的输出 sheet 宽高。
    // 通过最小化 abs(width - height) 选择最接近正方形的输出矩形；若相同，则优先选择面积更小的方案。
    // 实际 sprite 放置仍委托给确定性的固定列网格构建逻辑，从而保持源图顺序不变。
    if (images.empty()) {
        return 1;
    }

    padding = std::max(0, padding);
    margin = std::max(0, margin);
    cellWidth = std::max(1, cellWidth);
    cellHeight = std::max(1, cellHeight);

    const int imageCount = static_cast<int>(images.size());
    int bestColumns = 1;
    int bestDifference = 0;
    int bestArea = 0;
    for (int columns = 1; columns <= imageCount; ++columns) {
        const int rows = (imageCount + columns - 1) / columns;
        const int width = margin * 2 + columns * cellWidth + (columns - 1) * padding;
        const int height = margin * 2 + rows * cellHeight + (rows - 1) * padding;
        const int difference = std::abs(width - height);
        const int area = width * height;
        if (columns == 1 || difference < bestDifference || (difference == bestDifference && area < bestArea)) {
            bestColumns = columns;
            bestDifference = difference;
            bestArea = area;
        }
    }
    return bestColumns;
}

struct ShelfPlacement {
    int sourceIndex = 0;
    int x = 0;
    int y = 0;
};

static bool validateSourceImages(const std::vector<SourceImage>& images)
{
    for (const SourceImage& image : images) {
        if (image.width <= 0 || image.height <= 0 || image.pixels.size() != static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * 4) {
            return false;
        }
    }
    return true;
}

static void copyPixel(const SourceImage& image, PackedSheet& sheet, int srcX, int srcY, int dstX, int dstY)
{
    const size_t srcOffset = (static_cast<size_t>(srcY) * static_cast<size_t>(image.width) + static_cast<size_t>(srcX)) * 4;
    const size_t dstOffset = (static_cast<size_t>(dstY) * static_cast<size_t>(sheet.width) + static_cast<size_t>(dstX)) * 4;
    std::copy_n(image.pixels.data() + srcOffset, 4, sheet.pixels.data() + dstOffset);
}

static void copyImagePixels(const SourceImage& image, PackedSheet& sheet, int dstX, int dstY)
{
    for (int y = 0; y < image.height; ++y) {
        const size_t srcOffset = static_cast<size_t>(y) * static_cast<size_t>(image.width) * 4;
        const size_t dstOffset = (static_cast<size_t>(dstY + y) * static_cast<size_t>(sheet.width) + static_cast<size_t>(dstX)) * 4;
        std::copy_n(image.pixels.data() + srcOffset, static_cast<size_t>(image.width) * 4, sheet.pixels.data() + dstOffset);
    }
}

static void copyImageWithExtrudedBorder(const SourceImage& image, PackedSheet& sheet, int spriteX, int spriteY, int extrude)
{
    // Extrude / Duplicate Border 算法原理：
    // sprite 的逻辑矩形仍然只覆盖原始图像区域，不把扩展边缘算入 SpriteRect。
    // 在原始图像四周预留 extrude 像素的透明间隔，并把最靠近边缘的源像素复制到这些间隔中。
    // 这样纹理线性采样或缩放时，采样到 sprite 外侧的像素仍是边缘颜色，可减少相邻 sprite 或透明背景造成的 bleeding。
    // 四个角落通过同时钳制 X/Y 源坐标来复制最近的角像素，保证整个扩展边框都被填满。
    if (extrude <= 0) {
        copyImagePixels(image, sheet, spriteX, spriteY);
        return;
    }

    for (int y = -extrude; y < image.height + extrude; ++y) {
        const int srcY = std::clamp(y, 0, image.height - 1);
        const int dstY = spriteY + y;
        for (int x = -extrude; x < image.width + extrude; ++x) {
            const int srcX = std::clamp(x, 0, image.width - 1);
            const int dstX = spriteX + x;
            copyPixel(image, sheet, srcX, srcY, dstX, dstY);
        }
    }
}

static std::vector<int> sortedSourceOrder(const std::vector<SourceImage>& images, PackingSortMode sortMode)
{
    std::vector<int> order(images.size());
    for (int i = 0; i < static_cast<int>(order.size()); ++i) {
        order[static_cast<size_t>(i)] = i;
    }
    switch (sortMode) {
    case PackingSortMode::HeightDesc:
        std::stable_sort(order.begin(), order.end(), [&images](int a, int b) {
            return images[static_cast<size_t>(a)].height > images[static_cast<size_t>(b)].height;
        });
        break;
    case PackingSortMode::AreaDesc:
        std::stable_sort(order.begin(), order.end(), [&images](int a, int b) {
            const int areaA = images[static_cast<size_t>(a)].width * images[static_cast<size_t>(a)].height;
            const int areaB = images[static_cast<size_t>(b)].width * images[static_cast<size_t>(b)].height;
            return areaA > areaB;
        });
        break;
    case PackingSortMode::Original:
        break;
    }
    return order;
}

static PackedSheet buildShelfSheet(const std::vector<SourceImage>& images, const SheetPackingSettings& settings)
{
    // Shelf Packing（货架算法）原理：
    // 将输出图集看作从上到下排列的多层“货架”，每层从左到右依次摆放 sprite。
    // 当前行剩余宽度不足以容纳下一张图时，就在当前行最大高度下方开启新行，并继续从左侧摆放。
    // maxSheetWidth 限制每层的最大宽度；单张图片如果超过该宽度，则允许该行扩展到图片自身宽度以避免丢弃。
    // 可选排序会先改变摆放顺序以提高利用率，但生成的 sprite 仍记录各自源图名称和实际矩形位置。
    PackedSheet sheet;
    if (images.empty()) {
        return sheet;
    }
    if (!validateSourceImages(images)) {
        return sheet;
    }

    const int padding = std::max(0, settings.padding);
    const int margin = std::max(0, settings.margin);
    const int maxWidth = std::max(1, settings.maxSheetWidth);
    const int extrude = std::max(0, settings.extrudeBorder);

    const std::vector<int> order = sortedSourceOrder(images, settings.sortMode);

    std::vector<ShelfPlacement> placements;
    std::vector<SpriteRect> shelfRows;
    placements.reserve(images.size());
    int cursorX = margin;
    int cursorY = margin;
    int rowHeight = 0;
    int rowStartY = margin;
    int rowUsedRight = margin;
    int usedWidth = margin;
    for (const int sourceIndex : order) {
        const SourceImage& image = images[static_cast<size_t>(sourceIndex)];
        const int footprintW = image.width + extrude * 2;
        const int footprintH = image.height + extrude * 2;
        const int rowLimit = std::max(maxWidth, margin * 2 + footprintW);
        if (cursorX > margin && cursorX + footprintW > rowLimit - margin) {
            if (rowHeight > 0) {
                shelfRows.push_back(SpriteRect{"shelf_row_" + std::to_string(shelfRows.size()), margin, rowStartY, rowUsedRight - margin, rowHeight});
            }
            cursorX = margin;
            cursorY += rowHeight + padding;
            rowStartY = cursorY;
            rowUsedRight = margin;
            rowHeight = 0;
        }

        placements.push_back(ShelfPlacement{sourceIndex, cursorX, cursorY});
        cursorX += footprintW + padding;
        rowHeight = std::max(rowHeight, footprintH);
        rowUsedRight = std::max(rowUsedRight, cursorX - padding);
        usedWidth = std::max(usedWidth, cursorX - padding + margin);
    }
    if (rowHeight > 0) {
        shelfRows.push_back(SpriteRect{"shelf_row_" + std::to_string(shelfRows.size()), margin, rowStartY, rowUsedRight - margin, rowHeight});
    }

    sheet.width = std::max(1, usedWidth);
    sheet.height = std::max(1, cursorY + rowHeight + margin);
    sheet.pixels.assign(static_cast<size_t>(sheet.width) * static_cast<size_t>(sheet.height) * 4, 0);
    sheet.sprites.reserve(images.size());
    sheet.packingRects = std::move(shelfRows);

    for (const ShelfPlacement& placement : placements) {
        const SourceImage& image = images[static_cast<size_t>(placement.sourceIndex)];
        const int spriteX = placement.x + extrude;
        const int spriteY = placement.y + extrude;
        copyImageWithExtrudedBorder(image, sheet, spriteX, spriteY, extrude);

        SpriteRect sprite;
        sprite.name = image.name.empty() ? ("sprite_" + std::to_string(placement.sourceIndex)) : image.name;
        sprite.x = spriteX;
        sprite.y = spriteY;
        sprite.w = image.width;
        sprite.h = image.height;
        sheet.sprites.push_back(std::move(sprite));
    }

    return sheet;
}

struct SkylineNode {
    int x = 0;
    int y = 0;
    int width = 0;
};

struct SkylinePlacement {
    int sourceIndex = 0;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

static bool skylineFits(const std::vector<SkylineNode>& skyline, size_t index, int rectW, int maxUsableWidth, int& outY)
{
    const int startX = skyline[index].x;
    if (startX + rectW > maxUsableWidth) {
        return false;
    }

    int widthLeft = rectW;
    int y = 0;
    for (size_t i = index; i < skyline.size() && widthLeft > 0; ++i) {
        y = std::max(y, skyline[i].y);
        widthLeft -= skyline[i].width;
    }
    if (widthLeft > 0) {
        return false;
    }

    outY = y;
    return true;
}

static void mergeSkylineNodes(std::vector<SkylineNode>& skyline)
{
    for (size_t i = 0; i + 1 < skyline.size();) {
        if (skyline[i].y == skyline[i + 1].y) {
            skyline[i].width += skyline[i + 1].width;
            skyline.erase(skyline.begin() + static_cast<std::ptrdiff_t>(i + 1));
        } else {
            ++i;
        }
    }
}

static void addSkylineLevel(std::vector<SkylineNode>& skyline, size_t index, int x, int y, int rectW, int rectH)
{
    skyline.insert(skyline.begin() + static_cast<std::ptrdiff_t>(index), SkylineNode{x, y + rectH, rectW});

    const int newRight = x + rectW;
    for (size_t i = index + 1; i < skyline.size();) {
        const int nodeRight = skyline[i].x + skyline[i].width;
        if (skyline[i].x < newRight) {
            const int shrink = newRight - skyline[i].x;
            skyline[i].x += shrink;
            skyline[i].width -= shrink;
            if (skyline[i].width <= 0) {
                skyline.erase(skyline.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }
            if (nodeRight <= newRight) {
                skyline.erase(skyline.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }
        }
        break;
    }

    mergeSkylineNodes(skyline);
}

struct FreeRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct GuillotinePlacement {
    int sourceIndex = 0;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

static bool rectContains(const FreeRect& outer, const FreeRect& inner)
{
    return inner.x >= outer.x
        && inner.y >= outer.y
        && inner.x + inner.w <= outer.x + outer.w
        && inner.y + inner.h <= outer.y + outer.h;
}

static void pruneFreeRects(std::vector<FreeRect>& freeRects)
{
    for (size_t i = 0; i < freeRects.size();) {
        bool removed = false;
        for (size_t j = 0; j < freeRects.size(); ++j) {
            if (i != j && rectContains(freeRects[j], freeRects[i])) {
                freeRects.erase(freeRects.begin() + static_cast<std::ptrdiff_t>(i));
                removed = true;
                break;
            }
        }
        if (!removed) {
            ++i;
        }
    }
}

static void splitFreeRectGuillotine(std::vector<FreeRect>& freeRects, size_t index, int usedW, int usedH)
{
    const FreeRect freeRect = freeRects[index];
    freeRects.erase(freeRects.begin() + static_cast<std::ptrdiff_t>(index));

    const int leftoverW = freeRect.w - usedW;
    const int leftoverH = freeRect.h - usedH;
    if (leftoverW < 0 || leftoverH < 0) {
        return;
    }

    if (leftoverH >= leftoverW) {
        if (leftoverW > 0) {
            freeRects.push_back(FreeRect{freeRect.x + usedW, freeRect.y, leftoverW, usedH});
        }
        if (leftoverH > 0) {
            freeRects.push_back(FreeRect{freeRect.x, freeRect.y + usedH, freeRect.w, leftoverH});
        }
    } else {
        if (leftoverH > 0) {
            freeRects.push_back(FreeRect{freeRect.x, freeRect.y + usedH, usedW, leftoverH});
        }
        if (leftoverW > 0) {
            freeRects.push_back(FreeRect{freeRect.x + usedW, freeRect.y, leftoverW, freeRect.h});
        }
    }
    pruneFreeRects(freeRects);
}

static PackedSheet buildGuillotineSheet(const std::vector<SourceImage>& images, const SheetPackingSettings& settings)
{
    // Guillotine Packing（切割算法）原理：
    // 维护一组尚未被占用的空闲矩形，每次从中选择一个能容纳当前 sprite footprint 的矩形来放置。
    // 本实现使用 Best Area Fit：优先选择浪费面积最小的空闲矩形；若相同，则选择更靠上、更靠左的位置以保持确定性。
    // 放置后，沿着已占用 footprint 的右侧和下侧把原空闲矩形切成两个剩余矩形，类似用一刀横切/竖切分割空间。
    // 切割方向根据剩余宽高决定，倾向让较大的剩余方向保持完整；最后移除被其它空闲矩形完全包含的冗余矩形。
    PackedSheet sheet;
    if (images.empty()) {
        return sheet;
    }
    if (!validateSourceImages(images)) {
        return sheet;
    }

    const int padding = std::max(0, settings.padding);
    const int margin = std::max(0, settings.margin);
    const int maxWidth = std::max(1, settings.maxSheetWidth);
    const int extrude = std::max(0, settings.extrudeBorder);
    const int widestFootprint = maxSourceImageWidth(images) + extrude * 2;
    const int sheetWidthLimit = std::max(maxWidth, margin * 2 + widestFootprint);
    const int usableWidth = std::max(1, sheetWidthLimit - margin * 2);
    const int totalFootprintArea = std::max(1, [&images, extrude, padding]() {
        int area = 0;
        for (const SourceImage& image : images) {
            area += (image.width + extrude * 2 + padding) * (image.height + extrude * 2 + padding);
        }
        return area;
    }());
    const int tallestFootprint = maxSourceImageHeight(images) + extrude * 2 + padding;
    const int initialUsableHeight = std::max(tallestFootprint, (totalFootprintArea + usableWidth - 1) / usableWidth + tallestFootprint);

    std::vector<FreeRect> freeRects{{margin, margin, usableWidth, initialUsableHeight}};
    std::vector<GuillotinePlacement> placements;
    placements.reserve(images.size());
    int usedWidth = margin;
    int usedHeight = margin;

    const std::vector<int> order = sortedSourceOrder(images, settings.sortMode);
    for (const int sourceIndex : order) {
        const SourceImage& image = images[static_cast<size_t>(sourceIndex)];
        const int footprintW = image.width + extrude * 2;
        const int footprintH = image.height + extrude * 2;
        const int paddedW = footprintW + padding;
        const int paddedH = footprintH + padding;

        size_t bestIndex = 0;
        int bestWaste = 0;
        bool found = false;
        for (size_t i = 0; i < freeRects.size(); ++i) {
            const FreeRect& freeRect = freeRects[i];
            if (paddedW > freeRect.w || paddedH > freeRect.h) {
                continue;
            }
            const int waste = freeRect.w * freeRect.h - paddedW * paddedH;
            if (!found
                || waste < bestWaste
                || (waste == bestWaste && freeRect.y < freeRects[bestIndex].y)
                || (waste == bestWaste && freeRect.y == freeRects[bestIndex].y && freeRect.x < freeRects[bestIndex].x)) {
                found = true;
                bestIndex = i;
                bestWaste = waste;
            }
        }

        if (!found) {
            return PackedSheet{};
        }

        const FreeRect chosen = freeRects[bestIndex];
        placements.push_back(GuillotinePlacement{sourceIndex, chosen.x, chosen.y, footprintW, footprintH});
        splitFreeRectGuillotine(freeRects, bestIndex, paddedW, paddedH);
        usedWidth = std::max(usedWidth, chosen.x + footprintW + margin);
        usedHeight = std::max(usedHeight, chosen.y + footprintH + margin);
    }

    sheet.width = std::max(1, usedWidth);
    sheet.height = std::max(1, usedHeight);
    sheet.pixels.assign(static_cast<size_t>(sheet.width) * static_cast<size_t>(sheet.height) * 4, 0);
    sheet.sprites.reserve(images.size());
    sheet.packingRects.reserve(images.size());

    for (const GuillotinePlacement& placement : placements) {
        const SourceImage& image = images[static_cast<size_t>(placement.sourceIndex)];
        const int spriteX = placement.x + extrude;
        const int spriteY = placement.y + extrude;
        copyImageWithExtrudedBorder(image, sheet, spriteX, spriteY, extrude);

        SpriteRect packingRect;
        packingRect.name = "guillotine_" + std::to_string(sheet.packingRects.size());
        packingRect.x = placement.x;
        packingRect.y = placement.y;
        packingRect.w = placement.w;
        packingRect.h = placement.h;
        sheet.packingRects.push_back(std::move(packingRect));

        SpriteRect sprite;
        sprite.name = image.name.empty() ? ("sprite_" + std::to_string(placement.sourceIndex)) : image.name;
        sprite.x = spriteX;
        sprite.y = spriteY;
        sprite.w = image.width;
        sprite.h = image.height;
        sheet.sprites.push_back(std::move(sprite));
    }

    return sheet;
}

static bool rectsIntersect(const FreeRect& a, const FreeRect& b)
{
    return a.x < b.x + b.w
        && a.x + a.w > b.x
        && a.y < b.y + b.h
        && a.y + a.h > b.y;
}

static void splitFreeRectMaxRects(const FreeRect& freeRect, const FreeRect& usedRect, std::vector<FreeRect>& output)
{
    if (!rectsIntersect(freeRect, usedRect)) {
        output.push_back(freeRect);
        return;
    }

    if (usedRect.x > freeRect.x) {
        output.push_back(FreeRect{freeRect.x, freeRect.y, usedRect.x - freeRect.x, freeRect.h});
    }
    if (usedRect.x + usedRect.w < freeRect.x + freeRect.w) {
        output.push_back(FreeRect{usedRect.x + usedRect.w, freeRect.y, freeRect.x + freeRect.w - (usedRect.x + usedRect.w), freeRect.h});
    }
    if (usedRect.y > freeRect.y) {
        output.push_back(FreeRect{freeRect.x, freeRect.y, freeRect.w, usedRect.y - freeRect.y});
    }
    if (usedRect.y + usedRect.h < freeRect.y + freeRect.h) {
        output.push_back(FreeRect{freeRect.x, usedRect.y + usedRect.h, freeRect.w, freeRect.y + freeRect.h - (usedRect.y + usedRect.h)});
    }
}

static PackedSheet buildMaxRectsSheet(const std::vector<SourceImage>& images, const SheetPackingSettings& settings)
{
    // MaxRects Packing（最大矩形算法）原理：
    // 维护一组“最大空闲矩形”，它们表示当前 atlas 中所有仍可能容纳 sprite 的候选区域。
    // 每放置一个 sprite footprint 时，遍历所有能容纳它的空闲矩形，并使用 Best Short Side Fit 选择短边剩余最小的位置。
    // 放置后，并不是只切割被选中的矩形，而是将所有与已占用 footprint 相交的空闲矩形都按上下左右拆分。
    // 拆分后移除被其它空闲矩形完全包含的冗余项，使集合尽量保持为“最大”空闲矩形，后续 sprite 可以利用更细碎的剩余空间。
    PackedSheet sheet;
    if (images.empty()) {
        return sheet;
    }
    if (!validateSourceImages(images)) {
        return sheet;
    }

    const int padding = std::max(0, settings.padding);
    const int margin = std::max(0, settings.margin);
    const int maxWidth = std::max(1, settings.maxSheetWidth);
    const int extrude = std::max(0, settings.extrudeBorder);
    const int widestFootprint = maxSourceImageWidth(images) + extrude * 2;
    const int sheetWidthLimit = std::max(maxWidth, margin * 2 + widestFootprint);
    const int usableWidth = std::max(1, sheetWidthLimit - margin * 2);
    const int totalFootprintArea = std::max(1, [&images, extrude, padding]() {
        int area = 0;
        for (const SourceImage& image : images) {
            area += (image.width + extrude * 2 + padding) * (image.height + extrude * 2 + padding);
        }
        return area;
    }());
    const int tallestFootprint = maxSourceImageHeight(images) + extrude * 2 + padding;
    const int initialUsableHeight = std::max(tallestFootprint, (totalFootprintArea + usableWidth - 1) / usableWidth + tallestFootprint);

    std::vector<FreeRect> freeRects{{margin, margin, usableWidth, initialUsableHeight}};
    std::vector<GuillotinePlacement> placements;
    placements.reserve(images.size());
    int usedWidth = margin;
    int usedHeight = margin;

    const std::vector<int> order = sortedSourceOrder(images, settings.sortMode);
    for (const int sourceIndex : order) {
        const SourceImage& image = images[static_cast<size_t>(sourceIndex)];
        const int footprintW = image.width + extrude * 2;
        const int footprintH = image.height + extrude * 2;
        const int paddedW = footprintW + padding;
        const int paddedH = footprintH + padding;

        size_t bestIndex = 0;
        int bestShortSide = 0;
        int bestLongSide = 0;
        bool found = false;
        for (size_t i = 0; i < freeRects.size(); ++i) {
            const FreeRect& freeRect = freeRects[i];
            if (paddedW > freeRect.w || paddedH > freeRect.h) {
                continue;
            }
            const int leftoverW = freeRect.w - paddedW;
            const int leftoverH = freeRect.h - paddedH;
            const int shortSide = std::min(leftoverW, leftoverH);
            const int longSide = std::max(leftoverW, leftoverH);
            if (!found
                || shortSide < bestShortSide
                || (shortSide == bestShortSide && longSide < bestLongSide)
                || (shortSide == bestShortSide && longSide == bestLongSide && freeRect.y < freeRects[bestIndex].y)
                || (shortSide == bestShortSide && longSide == bestLongSide && freeRect.y == freeRects[bestIndex].y && freeRect.x < freeRects[bestIndex].x)) {
                found = true;
                bestIndex = i;
                bestShortSide = shortSide;
                bestLongSide = longSide;
            }
        }

        if (!found) {
            return PackedSheet{};
        }

        const FreeRect chosen = freeRects[bestIndex];
        const FreeRect usedRect{chosen.x, chosen.y, paddedW, paddedH};
        placements.push_back(GuillotinePlacement{sourceIndex, chosen.x, chosen.y, footprintW, footprintH});

        std::vector<FreeRect> nextFreeRects;
        nextFreeRects.reserve(freeRects.size() * 2 + 4);
        for (const FreeRect& freeRect : freeRects) {
            splitFreeRectMaxRects(freeRect, usedRect, nextFreeRects);
        }
        freeRects = std::move(nextFreeRects);
        pruneFreeRects(freeRects);

        usedWidth = std::max(usedWidth, chosen.x + footprintW + margin);
        usedHeight = std::max(usedHeight, chosen.y + footprintH + margin);
    }

    sheet.width = std::max(1, usedWidth);
    sheet.height = std::max(1, usedHeight);
    sheet.pixels.assign(static_cast<size_t>(sheet.width) * static_cast<size_t>(sheet.height) * 4, 0);
    sheet.sprites.reserve(images.size());
    sheet.packingRects.reserve(images.size());

    for (const GuillotinePlacement& placement : placements) {
        const SourceImage& image = images[static_cast<size_t>(placement.sourceIndex)];
        const int spriteX = placement.x + extrude;
        const int spriteY = placement.y + extrude;
        copyImageWithExtrudedBorder(image, sheet, spriteX, spriteY, extrude);

        SpriteRect packingRect;
        packingRect.name = "maxrects_" + std::to_string(sheet.packingRects.size());
        packingRect.x = placement.x;
        packingRect.y = placement.y;
        packingRect.w = placement.w;
        packingRect.h = placement.h;
        sheet.packingRects.push_back(std::move(packingRect));

        SpriteRect sprite;
        sprite.name = image.name.empty() ? ("sprite_" + std::to_string(placement.sourceIndex)) : image.name;
        sprite.x = spriteX;
        sprite.y = spriteY;
        sprite.w = image.width;
        sprite.h = image.height;
        sheet.sprites.push_back(std::move(sprite));
    }

    return sheet;
}

static PackedSheet buildSkylineSheet(const std::vector<SourceImage>& images, const SheetPackingSettings& settings)
{
    // Skyline Packing（天际线算法）原理：
    // 将已经占用的区域抽象成从左到右的一组“天际线”高度节点，每个节点表示一段连续 X 区间的当前顶部高度。
    // 放置新 sprite 时，遍历所有节点作为候选左边界，检查从该位置开始的连续宽度是否足够容纳 footprint。
    // 对所有可放置候选，选择 Y 最低的位置；若 Y 相同则选择更靠左的位置，从而形成 bottom-left 风格的紧凑布局。
    // 放入后把该 footprint 覆盖范围的天际线抬高到新顶部，并裁剪/合并被覆盖或同高的节点，供后续 sprite 继续贴合。
    PackedSheet sheet;
    if (images.empty()) {
        return sheet;
    }
    if (!validateSourceImages(images)) {
        return sheet;
    }

    const int padding = std::max(0, settings.padding);
    const int margin = std::max(0, settings.margin);
    const int maxWidth = std::max(1, settings.maxSheetWidth);
    const int extrude = std::max(0, settings.extrudeBorder);
    const int widestFootprint = maxSourceImageWidth(images) + extrude * 2;
    const int sheetWidthLimit = std::max(maxWidth, margin * 2 + widestFootprint);
    const int usableWidth = std::max(1, sheetWidthLimit - margin * 2);

    std::vector<SkylineNode> skyline{{margin, margin, usableWidth}};
    std::vector<SkylinePlacement> placements;
    placements.reserve(images.size());
    int usedWidth = margin;
    int usedHeight = margin;

    const std::vector<int> order = sortedSourceOrder(images, settings.sortMode);
    for (const int sourceIndex : order) {
        const SourceImage& image = images[static_cast<size_t>(sourceIndex)];
        const int footprintW = image.width + extrude * 2;
        const int footprintH = image.height + extrude * 2;
        const int paddedW = footprintW + padding;
        const int paddedH = footprintH + padding;

        size_t bestIndex = 0;
        int bestX = 0;
        int bestY = 0;
        bool found = false;
        for (size_t i = 0; i < skyline.size(); ++i) {
            int candidateY = 0;
            if (!skylineFits(skyline, i, paddedW, margin + usableWidth, candidateY)) {
                continue;
            }
            const int candidateX = skyline[i].x;
            if (!found || candidateY < bestY || (candidateY == bestY && candidateX < bestX)) {
                found = true;
                bestIndex = i;
                bestX = candidateX;
                bestY = candidateY;
            }
        }

        if (!found) {
            return PackedSheet{};
        }

        placements.push_back(SkylinePlacement{sourceIndex, bestX, bestY, footprintW, footprintH});
        addSkylineLevel(skyline, bestIndex, bestX, bestY, paddedW, paddedH);
        usedWidth = std::max(usedWidth, bestX + footprintW + margin);
        usedHeight = std::max(usedHeight, bestY + footprintH + margin);
    }

    sheet.width = std::max(1, usedWidth);
    sheet.height = std::max(1, usedHeight);
    sheet.pixels.assign(static_cast<size_t>(sheet.width) * static_cast<size_t>(sheet.height) * 4, 0);
    sheet.sprites.reserve(images.size());
    sheet.packingRects.reserve(images.size());

    for (const SkylinePlacement& placement : placements) {
        const SourceImage& image = images[static_cast<size_t>(placement.sourceIndex)];
        const int spriteX = placement.x + extrude;
        const int spriteY = placement.y + extrude;
        copyImageWithExtrudedBorder(image, sheet, spriteX, spriteY, extrude);

        SpriteRect packingRect;
        packingRect.name = "skyline_" + std::to_string(sheet.packingRects.size());
        packingRect.x = placement.x;
        packingRect.y = placement.y;
        packingRect.w = placement.w;
        packingRect.h = placement.h;
        sheet.packingRects.push_back(std::move(packingRect));

        SpriteRect sprite;
        sprite.name = image.name.empty() ? ("sprite_" + std::to_string(placement.sourceIndex)) : image.name;
        sprite.x = spriteX;
        sprite.y = spriteY;
        sprite.w = image.width;
        sprite.h = image.height;
        sheet.sprites.push_back(std::move(sprite));
    }

    return sheet;
}

PackedSheet buildSpriteSheet(const std::vector<SourceImage>& images, const SheetPackingSettings& settings)
{
    const int cellWidth = settings.useCustomCellSize ? settings.customCellWidth : maxSourceImageWidth(images);
    const int cellHeight = settings.useCustomCellSize ? settings.customCellHeight : maxSourceImageHeight(images);
    const int extrude = std::max(0, settings.extrudeBorder);
    switch (settings.algorithm) {
    case PackingAlgorithm::AutoGrid: {
        const int footprintW = std::max(1, cellWidth) + extrude * 2;
        const int footprintH = std::max(1, cellHeight) + extrude * 2;
        return buildFixedGridSheet(images, autoGridColumnCount(images, settings.padding, settings.margin, footprintW, footprintH), settings.padding, settings.margin, cellWidth, cellHeight, extrude);
    }
    case PackingAlgorithm::Shelf:
        return buildShelfSheet(images, settings);
    case PackingAlgorithm::Skyline:
        return buildSkylineSheet(images, settings);
    case PackingAlgorithm::Guillotine:
        return buildGuillotineSheet(images, settings);
    case PackingAlgorithm::MaxRects:
        return buildMaxRectsSheet(images, settings);
    case PackingAlgorithm::FixedColumns:
        break;
    }
    return buildFixedGridSheet(images, settings.columns, settings.padding, settings.margin, cellWidth, cellHeight, extrude);
}
