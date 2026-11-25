#include "../src/sprite_layout.h"

#include <iostream>
#include <vector>

static void setPixel(std::vector<unsigned char>& pixels, int imageWidth, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    const size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(imageWidth) + static_cast<size_t>(x)) * 4;
    pixels[offset + 0] = r;
    pixels[offset + 1] = g;
    pixels[offset + 2] = b;
    pixels[offset + 3] = a;
}

static bool expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
        return false;
    }
    return true;
}

int main()
{
    bool ok = true;

    ok &= expect(snapCoordinate(13, 8) == 16, "snap should round to nearest grid line");
    ok &= expect(snapCoordinate(12, 1) == 12, "grid size 1 should preserve coordinate");

    SpriteRect a;
    a.x = 0;
    a.y = 0;
    a.w = 4;
    a.h = 4;
    SpriteRect b;
    b.x = 4;
    b.y = 0;
    b.w = 2;
    b.h = 4;
    ok &= expect(!rectsOverlap(a, b), "edge-touching rects should not overlap");
    b.x = 3;
    ok &= expect(rectsOverlap(a, b), "intersecting rects should overlap");
    ok &= expect(rectInsideAtlas(a, 8, 8), "rect should be inside atlas");
    ok &= expect(!rectInsideAtlas(b, 4, 4), "rect exceeding atlas should be out of bounds");

    std::vector<unsigned char> pixels(8 * 4 * 4, 0);
    setPixel(pixels, 8, 0, 0, 255, 0, 0, 255);
    setPixel(pixels, 8, 1, 0, 0, 255, 0, 255);
    setPixel(pixels, 8, 4, 0, 0, 0, 255, 255);
    setPixel(pixels, 8, 5, 0, 255, 255, 0, 255);

    SpriteRect first;
    first.name = "first";
    first.x = 0;
    first.y = 0;
    first.w = 2;
    first.h = 1;
    SpriteRect second;
    second.name = "second";
    second.x = 4;
    second.y = 0;
    second.w = 2;
    second.h = 1;

    SpriteLayout layout = createSpriteLayoutFromSprites(8, 4, pixels, {first, second}, {});
    ok &= expect(layout.valid(), "layout should be created from current sprites");
    ok &= expect(layout.items.size() == 2, "layout should preserve valid sprite count");
    ok &= expect(layout.items[0].name == "first" && layout.items[1].sourceIndex == 1, "layout should preserve names and source indices");

    SpriteLayoutSettings settings;
    settings.snapToGrid = false;
    SpriteLayoutValidation overlap = validateLayoutItemMove(layout, 1, 1, 0, settings);
    ok &= expect(!overlap.valid && overlap.overlaps, "validation should detect overlap during move");
    SpriteLayoutValidation outOfBounds = validateLayoutItemMove(layout, 1, 7, 0, settings);
    ok &= expect(!outOfBounds.valid && outOfBounds.outOfBounds, "validation should detect out of bounds during move");
    ok &= expect(moveLayoutItem(layout, 1, 6, 0, settings), "valid move should commit");
    ok &= expect(layout.items[1].x == 6, "valid move should update item x");
    ok &= expect(!moveLayoutItem(layout, 1, 1, 0, settings), "invalid overlap move should be rejected");
    ok &= expect(layout.items[1].x == 6, "rejected move should not change item x");

    settings.snapToGrid = true;
    settings.gridSize = 2;
    ok &= expect(moveLayoutItem(layout, 1, 5, 2, settings), "snap-valid move should commit");
    ok &= expect(layout.items[1].x == 6 && layout.items[1].y == 2, "move should snap to grid before commit");

    PackedSheet sheet = buildSheetFromLayout(layout);
    ok &= expect(sheet.valid(), "buildSheetFromLayout should generate atlas pixels");
    ok &= expect(sheet.sprites.size() == 2, "built sheet should preserve layout item count");
    const size_t movedPixel = (static_cast<size_t>(2) * static_cast<size_t>(8) + static_cast<size_t>(6)) * 4;
    ok &= expect(sheet.pixels[movedPixel + 2] == 255, "built sheet should redraw moved sprite pixels at target position");

    SheetPackingSettings packing;
    packing.algorithm = PackingAlgorithm::FixedColumns;
    packing.columns = 2;
    packing.padding = 1;
    packing.margin = 1;
    SpriteLayout repacked = autoRepackLayout(layout, packing);
    ok &= expect(repacked.valid(), "autoRepackLayout should generate a valid layout");
    ok &= expect(repacked.items.size() == 2, "auto repack should preserve item count");
    ok &= expect(repacked.items[0].name == "first" && repacked.items[1].name == "second", "auto repack should preserve original order and names");
    ok &= expect(repacked.items[0].x == 1 && repacked.items[1].x == 4, "auto repack should apply packing margin and padding");

    return ok ? 0 : 1;
}
