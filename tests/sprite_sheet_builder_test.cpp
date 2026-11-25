#include "../src/sprite_sheet_builder.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

static SourceImage makeImage(std::string name, int width, int height, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    SourceImage image;
    image.name = std::move(name);
    image.width = width;
    image.height = height;
    image.pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (size_t i = 0; i + 3 < image.pixels.size(); i += 4) {
        image.pixels[i + 0] = r;
        image.pixels[i + 1] = g;
        image.pixels[i + 2] = b;
        image.pixels[i + 3] = a;
    }
    return image;
}

static SourceImage makeCornerImage(std::string name)
{
    SourceImage image;
    image.name = std::move(name);
    image.width = 2;
    image.height = 2;
    image.pixels = {
        10, 0, 0, 255, 20, 0, 0, 255,
        30, 0, 0, 255, 40, 0, 0, 255
    };
    return image;
}

static const unsigned char* pixelAt(const PackedSheet& sheet, int x, int y)
{
    return sheet.pixels.data() + (static_cast<size_t>(y) * static_cast<size_t>(sheet.width) + static_cast<size_t>(x)) * 4;
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

    const PackedSheet empty = buildSpriteSheet({}, 4, 2, 1, 8, 8);
    ok &= expect(!empty.valid(), "empty input returns invalid sheet");
    ok &= expect(empty.sprites.empty(), "empty input has no sprites");

    const SourceImage red = makeImage("red", 2, 2, 255, 0, 0, 200);
    const PackedSheet single = buildSpriteSheet({red}, 4, 2, 1, 8, 8);
    ok &= expect(single.valid(), "single image sheet is valid");
    ok &= expect(single.width == 10 && single.height == 10, "single image respects margin and cell size");
    ok &= expect(single.sprites.size() == 1, "single image creates one sprite");
    ok &= expect(single.sprites[0].name == "red", "single sprite keeps source name");
    ok &= expect(single.sprites[0].x == 1 && single.sprites[0].y == 1 && single.sprites[0].w == 2 && single.sprites[0].h == 2, "single sprite rect uses source size at margin");
    ok &= expect(pixelAt(single, 1, 1)[0] == 255 && pixelAt(single, 1, 1)[3] == 200, "single image copies RGBA pixel");
    ok &= expect(pixelAt(single, 0, 0)[3] == 0, "margin remains transparent");

    const SourceImage green = makeImage("green", 3, 1, 0, 255, 0, 255);
    const SourceImage blue = makeImage("blue", 1, 3, 0, 0, 255, 128);
    const PackedSheet grid = buildSpriteSheet({red, green, blue}, 2, 2, 1, 1, 1);
    ok &= expect(grid.width == 10 && grid.height == 10, "grid uses max source dimensions and row wrapping");
    ok &= expect(grid.sprites.size() == 3, "grid creates three sprites");
    ok &= expect(grid.sprites[0].x == 1 && grid.sprites[0].y == 1, "first sprite position");
    ok &= expect(grid.sprites[1].x == 6 && grid.sprites[1].y == 1, "second sprite position with padding");
    ok &= expect(grid.sprites[2].x == 1 && grid.sprites[2].y == 6, "third sprite wraps to next row");
    ok &= expect(grid.sprites[1].w == 3 && grid.sprites[1].h == 1, "sprite rect preserves non-square source size");
    ok &= expect(grid.packingRects.size() == 3, "fixed grid records one packing cell per image");
    ok &= expect(grid.packingRects[1].x == 6 && grid.packingRects[1].y == 1 && grid.packingRects[1].w == 3 && grid.packingRects[1].h == 3, "fixed grid packing rect records full cell size");
    ok &= expect(pixelAt(grid, 6, 1)[1] == 255, "green image copied at packed position");
    ok &= expect(pixelAt(grid, 1, 6)[2] == 255 && pixelAt(grid, 1, 6)[3] == 128, "blue image alpha preserved");
    ok &= expect(pixelAt(grid, 4, 1)[3] == 0, "padding gap remains transparent");

    SheetPackingSettings settings;
    settings.columns = 2;
    settings.padding = 2;
    settings.margin = 1;
    const PackedSheet configured = buildSpriteSheet({red, green, blue}, settings);
    ok &= expect(configured.width == grid.width && configured.height == grid.height, "settings overload matches fixed-column packing");
    ok &= expect(maxSourceImageWidth({red, green, blue}) == 3, "max source image width");
    ok &= expect(maxSourceImageHeight({red, green, blue}) == 3, "max source image height");

    SheetPackingSettings autoGridSettings;
    autoGridSettings.algorithm = PackingAlgorithm::AutoGrid;
    autoGridSettings.padding = 0;
    autoGridSettings.margin = 0;
    autoGridSettings.useCustomCellSize = true;
    autoGridSettings.customCellWidth = 10;
    autoGridSettings.customCellHeight = 10;
    const PackedSheet autoGrid = buildSpriteSheet({
        makeImage("a", 1, 1, 1, 0, 0, 255),
        makeImage("b", 1, 1, 2, 0, 0, 255),
        makeImage("c", 1, 1, 3, 0, 0, 255),
        makeImage("d", 1, 1, 4, 0, 0, 255),
        makeImage("e", 1, 1, 5, 0, 0, 255),
        makeImage("f", 1, 1, 6, 0, 0, 255),
        makeImage("g", 1, 1, 7, 0, 0, 255),
        makeImage("h", 1, 1, 8, 0, 0, 255),
        makeImage("i", 1, 1, 9, 0, 0, 255)
    }, autoGridSettings);
    ok &= expect(autoGrid.width == 30 && autoGrid.height == 30, "auto grid chooses 3x3 for nine square cells");
    ok &= expect(autoGrid.sprites[3].x == 0 && autoGrid.sprites[3].y == 10, "auto grid wraps after chosen square column count");
    ok &= expect(autoGrid.packingRects.size() == 9, "auto grid records one packing cell per image");
    ok &= expect(autoGrid.packingRects[8].x == 20 && autoGrid.packingRects[8].y == 20 && autoGrid.packingRects[8].w == 10 && autoGrid.packingRects[8].h == 10, "auto grid packing rect records computed cell");
    ok &= expect(pixelAt(autoGrid, 20, 20)[0] == 9, "auto grid preserves source order pixel placement");

    SheetPackingSettings wideAutoGridSettings = autoGridSettings;
    wideAutoGridSettings.customCellWidth = 20;
    wideAutoGridSettings.customCellHeight = 10;
    const PackedSheet wideAutoGrid = buildSpriteSheet({
        makeImage("a", 1, 1, 1, 0, 0, 255),
        makeImage("b", 1, 1, 2, 0, 0, 255),
        makeImage("c", 1, 1, 3, 0, 0, 255),
        makeImage("d", 1, 1, 4, 0, 0, 255),
        makeImage("e", 1, 1, 5, 0, 0, 255),
        makeImage("f", 1, 1, 6, 0, 0, 255)
    }, wideAutoGridSettings);
    ok &= expect(wideAutoGrid.width == 40 && wideAutoGrid.height == 30, "auto grid accounts for non-square cell dimensions");
    ok &= expect(wideAutoGrid.sprites[2].x == 0 && wideAutoGrid.sprites[2].y == 10, "wide auto grid chooses two columns for closest square output");

    SheetPackingSettings shelfSettings;
    shelfSettings.algorithm = PackingAlgorithm::Shelf;
    shelfSettings.padding = 1;
    shelfSettings.margin = 1;
    shelfSettings.maxSheetWidth = 8;
    const PackedSheet shelf = buildSpriteSheet({
        makeImage("wide", 4, 2, 10, 0, 0, 255),
        makeImage("tall", 3, 4, 20, 0, 0, 255),
        makeImage("small", 2, 1, 30, 0, 0, 255)
    }, shelfSettings);
    ok &= expect(shelf.width == 8 && shelf.height == 9, "shelf packs rows using actual image sizes and max width");
    ok &= expect(shelf.sprites[0].x == 1 && shelf.sprites[0].y == 1 && shelf.sprites[0].w == 4 && shelf.sprites[0].h == 2, "shelf first sprite position");
    ok &= expect(shelf.sprites[1].x == 1 && shelf.sprites[1].y == 4 && shelf.sprites[1].w == 3 && shelf.sprites[1].h == 4, "shelf wraps second sprite to next row");
    ok &= expect(shelf.sprites[2].x == 5 && shelf.sprites[2].y == 4 && shelf.sprites[2].w == 2 && shelf.sprites[2].h == 1, "shelf places third sprite on second row");
    ok &= expect(shelf.packingRects.size() == 2, "shelf records one packing rect per used row");
    ok &= expect(shelf.packingRects[0].x == 1 && shelf.packingRects[0].y == 1 && shelf.packingRects[0].w == 4 && shelf.packingRects[0].h == 2, "shelf first row packing rect");
    ok &= expect(shelf.packingRects[1].x == 1 && shelf.packingRects[1].y == 4 && shelf.packingRects[1].w == 6 && shelf.packingRects[1].h == 4, "shelf second row packing rect spans used row width");
    ok &= expect(pixelAt(shelf, 1, 1)[0] == 10, "shelf copies first image pixels");
    ok &= expect(pixelAt(shelf, 1, 4)[0] == 20, "shelf copies wrapped image pixels");
    ok &= expect(pixelAt(shelf, 5, 4)[0] == 30, "shelf copies same-row image pixels");
    ok &= expect(pixelAt(shelf, 0, 0)[3] == 0, "shelf margin remains transparent");

    SheetPackingSettings oversizedShelfSettings;
    oversizedShelfSettings.algorithm = PackingAlgorithm::Shelf;
    oversizedShelfSettings.maxSheetWidth = 4;
    const PackedSheet oversizedShelf = buildSpriteSheet({makeImage("oversized", 6, 2, 40, 0, 0, 255)}, oversizedShelfSettings);
    ok &= expect(oversizedShelf.width == 6 && oversizedShelf.height == 2, "shelf allows a single image wider than max width");

    SheetPackingSettings sortedShelfSettings = shelfSettings;
    sortedShelfSettings.sortMode = PackingSortMode::HeightDesc;
    const PackedSheet sortedShelf = buildSpriteSheet({
        makeImage("short", 2, 1, 1, 0, 0, 255),
        makeImage("tall", 2, 5, 2, 0, 0, 255),
        makeImage("mid", 2, 3, 3, 0, 0, 255)
    }, sortedShelfSettings);
    ok &= expect(sortedShelf.sprites[0].name == "tall", "height-desc shelf places tallest sprite first");
    ok &= expect(sortedShelf.sprites[1].name == "mid", "height-desc shelf keeps sorted order in output sprites");
    ok &= expect(sortedShelf.sprites[2].name == "short", "height-desc shelf places shortest sprite last");

    SheetPackingSettings skylineSettings;
    skylineSettings.algorithm = PackingAlgorithm::Skyline;
    skylineSettings.padding = 0;
    skylineSettings.margin = 0;
    skylineSettings.maxSheetWidth = 6;
    const PackedSheet skyline = buildSpriteSheet({
        makeImage("left_tall", 3, 4, 11, 0, 0, 255),
        makeImage("right_short", 3, 2, 22, 0, 0, 255),
        makeImage("fills_gap", 3, 2, 33, 0, 0, 255)
    }, skylineSettings);
    ok &= expect(skyline.width == 6 && skyline.height == 4, "skyline fills the lower gap before growing height");
    ok &= expect(skyline.sprites.size() == 3, "skyline creates three sprites");
    ok &= expect(skyline.sprites[0].x == 0 && skyline.sprites[0].y == 0, "skyline first sprite at origin");
    ok &= expect(skyline.sprites[1].x == 3 && skyline.sprites[1].y == 0, "skyline second sprite placed to the right");
    ok &= expect(skyline.sprites[2].x == 3 && skyline.sprites[2].y == 2, "skyline third sprite fills low right-side gap");
    ok &= expect(skyline.packingRects.size() == 3, "skyline records one debug rect per placed footprint");
    ok &= expect(skyline.packingRects[2].x == 3 && skyline.packingRects[2].y == 2 && skyline.packingRects[2].w == 3 && skyline.packingRects[2].h == 2, "skyline debug rect records footprint");
    ok &= expect(pixelAt(skyline, 3, 2)[0] == 33, "skyline copies gap-filling sprite pixels");

    SheetPackingSettings sortedSkylineSettings = skylineSettings;
    sortedSkylineSettings.sortMode = PackingSortMode::AreaDesc;
    const PackedSheet sortedSkyline = buildSpriteSheet({
        makeImage("tiny", 1, 1, 1, 0, 0, 255),
        makeImage("large", 3, 3, 2, 0, 0, 255),
        makeImage("mid", 2, 2, 3, 0, 0, 255)
    }, sortedSkylineSettings);
    ok &= expect(sortedSkyline.sprites[0].name == "large", "area-desc skyline places largest sprite first");
    ok &= expect(sortedSkyline.sprites[1].name == "mid", "area-desc skyline places middle sprite second");
    ok &= expect(sortedSkyline.sprites[2].name == "tiny", "area-desc skyline places smallest sprite last");

    SheetPackingSettings extrudeSkylineSettings;
    extrudeSkylineSettings.algorithm = PackingAlgorithm::Skyline;
    extrudeSkylineSettings.padding = 0;
    extrudeSkylineSettings.margin = 0;
    extrudeSkylineSettings.maxSheetWidth = 4;
    extrudeSkylineSettings.extrudeBorder = 1;
    const PackedSheet extrudedSkyline = buildSpriteSheet({makeCornerImage("corner")}, extrudeSkylineSettings);
    ok &= expect(extrudedSkyline.width == 4 && extrudedSkyline.height == 4, "extrude skyline expands footprint");
    ok &= expect(extrudedSkyline.sprites[0].x == 1 && extrudedSkyline.sprites[0].y == 1, "extrude skyline keeps sprite rect inside border");
    ok &= expect(extrudedSkyline.packingRects[0].x == 0 && extrudedSkyline.packingRects[0].y == 0 && extrudedSkyline.packingRects[0].w == 4 && extrudedSkyline.packingRects[0].h == 4, "extrude skyline debug rect covers footprint");
    ok &= expect(pixelAt(extrudedSkyline, 0, 0)[0] == 10 && pixelAt(extrudedSkyline, 3, 3)[0] == 40, "extrude skyline duplicates corner pixels");

    SheetPackingSettings guillotineSettings;
    guillotineSettings.algorithm = PackingAlgorithm::Guillotine;
    guillotineSettings.padding = 0;
    guillotineSettings.margin = 0;
    guillotineSettings.maxSheetWidth = 6;
    const PackedSheet guillotine = buildSpriteSheet({
        makeImage("left_tall", 3, 4, 44, 0, 0, 255),
        makeImage("right_short", 3, 2, 55, 0, 0, 255),
        makeImage("fills_bottom", 3, 2, 66, 0, 0, 255)
    }, guillotineSettings);
    ok &= expect(guillotine.width == 6 && guillotine.height == 4, "guillotine uses cut free rectangles compactly");
    ok &= expect(guillotine.sprites.size() == 3, "guillotine creates three sprites");
    ok &= expect(guillotine.sprites[0].x == 0 && guillotine.sprites[0].y == 0, "guillotine first sprite at origin");
    ok &= expect(guillotine.sprites[1].x == 3 && guillotine.sprites[1].y == 0, "guillotine second sprite in right split");
    ok &= expect(guillotine.sprites[2].x == 3 && guillotine.sprites[2].y == 2, "guillotine third sprite fills remaining lower right split");
    ok &= expect(guillotine.packingRects.size() == 3, "guillotine records one debug rect per placed footprint");
    ok &= expect(guillotine.packingRects[2].x == 3 && guillotine.packingRects[2].y == 2 && guillotine.packingRects[2].w == 3 && guillotine.packingRects[2].h == 2, "guillotine debug rect records footprint");
    ok &= expect(pixelAt(guillotine, 3, 2)[0] == 66, "guillotine copies lower split sprite pixels");

    SheetPackingSettings sortedGuillotineSettings = guillotineSettings;
    sortedGuillotineSettings.sortMode = PackingSortMode::AreaDesc;
    const PackedSheet sortedGuillotine = buildSpriteSheet({
        makeImage("tiny", 1, 1, 1, 0, 0, 255),
        makeImage("large", 3, 3, 2, 0, 0, 255),
        makeImage("mid", 2, 2, 3, 0, 0, 255)
    }, sortedGuillotineSettings);
    ok &= expect(sortedGuillotine.sprites[0].name == "large", "area-desc guillotine places largest sprite first");
    ok &= expect(sortedGuillotine.sprites[1].name == "mid", "area-desc guillotine places middle sprite second");
    ok &= expect(sortedGuillotine.sprites[2].name == "tiny", "area-desc guillotine places smallest sprite last");

    SheetPackingSettings extrudeGuillotineSettings;
    extrudeGuillotineSettings.algorithm = PackingAlgorithm::Guillotine;
    extrudeGuillotineSettings.padding = 0;
    extrudeGuillotineSettings.margin = 0;
    extrudeGuillotineSettings.maxSheetWidth = 4;
    extrudeGuillotineSettings.extrudeBorder = 1;
    const PackedSheet extrudedGuillotine = buildSpriteSheet({makeCornerImage("corner")}, extrudeGuillotineSettings);
    ok &= expect(extrudedGuillotine.width == 4 && extrudedGuillotine.height == 4, "extrude guillotine expands footprint");
    ok &= expect(extrudedGuillotine.sprites[0].x == 1 && extrudedGuillotine.sprites[0].y == 1, "extrude guillotine keeps sprite rect inside border");
    ok &= expect(extrudedGuillotine.packingRects[0].x == 0 && extrudedGuillotine.packingRects[0].y == 0 && extrudedGuillotine.packingRects[0].w == 4 && extrudedGuillotine.packingRects[0].h == 4, "extrude guillotine debug rect covers footprint");
    ok &= expect(pixelAt(extrudedGuillotine, 0, 0)[0] == 10 && pixelAt(extrudedGuillotine, 3, 3)[0] == 40, "extrude guillotine duplicates corner pixels");

    SheetPackingSettings maxRectsSettings;
    maxRectsSettings.algorithm = PackingAlgorithm::MaxRects;
    maxRectsSettings.padding = 0;
    maxRectsSettings.margin = 0;
    maxRectsSettings.maxSheetWidth = 6;
    const PackedSheet maxRects = buildSpriteSheet({
        makeImage("left_tall", 3, 4, 77, 0, 0, 255),
        makeImage("right_short", 3, 2, 88, 0, 0, 255),
        makeImage("fills_bottom", 3, 2, 99, 0, 0, 255)
    }, maxRectsSettings);
    ok &= expect(maxRects.width == 6 && maxRects.height == 4, "maxrects uses maximal free rectangles compactly");
    ok &= expect(maxRects.sprites.size() == 3, "maxrects creates three sprites");
    ok &= expect(maxRects.sprites[0].x == 0 && maxRects.sprites[0].y == 0, "maxrects first sprite at origin");
    ok &= expect(maxRects.sprites[1].x == 3 && maxRects.sprites[1].y == 0, "maxrects second sprite fills right maximal rect");
    ok &= expect(maxRects.sprites[2].x == 3 && maxRects.sprites[2].y == 2, "maxrects third sprite fills remaining lower right rect");
    ok &= expect(maxRects.packingRects.size() == 3, "maxrects records one debug rect per placed footprint");
    ok &= expect(maxRects.packingRects[2].x == 3 && maxRects.packingRects[2].y == 2 && maxRects.packingRects[2].w == 3 && maxRects.packingRects[2].h == 2, "maxrects debug rect records footprint");
    ok &= expect(pixelAt(maxRects, 3, 2)[0] == 99, "maxrects copies lower free-rect sprite pixels");

    SheetPackingSettings sortedMaxRectsSettings = maxRectsSettings;
    sortedMaxRectsSettings.sortMode = PackingSortMode::AreaDesc;
    const PackedSheet sortedMaxRects = buildSpriteSheet({
        makeImage("tiny", 1, 1, 1, 0, 0, 255),
        makeImage("large", 3, 3, 2, 0, 0, 255),
        makeImage("mid", 2, 2, 3, 0, 0, 255)
    }, sortedMaxRectsSettings);
    ok &= expect(sortedMaxRects.sprites[0].name == "large", "area-desc maxrects places largest sprite first");
    ok &= expect(sortedMaxRects.sprites[1].name == "mid", "area-desc maxrects places middle sprite second");
    ok &= expect(sortedMaxRects.sprites[2].name == "tiny", "area-desc maxrects places smallest sprite last");

    SheetPackingSettings extrudeMaxRectsSettings;
    extrudeMaxRectsSettings.algorithm = PackingAlgorithm::MaxRects;
    extrudeMaxRectsSettings.padding = 0;
    extrudeMaxRectsSettings.margin = 0;
    extrudeMaxRectsSettings.maxSheetWidth = 4;
    extrudeMaxRectsSettings.extrudeBorder = 1;
    const PackedSheet extrudedMaxRects = buildSpriteSheet({makeCornerImage("corner")}, extrudeMaxRectsSettings);
    ok &= expect(extrudedMaxRects.width == 4 && extrudedMaxRects.height == 4, "extrude maxrects expands footprint");
    ok &= expect(extrudedMaxRects.sprites[0].x == 1 && extrudedMaxRects.sprites[0].y == 1, "extrude maxrects keeps sprite rect inside border");
    ok &= expect(extrudedMaxRects.packingRects[0].x == 0 && extrudedMaxRects.packingRects[0].y == 0 && extrudedMaxRects.packingRects[0].w == 4 && extrudedMaxRects.packingRects[0].h == 4, "extrude maxrects debug rect covers footprint");
    ok &= expect(pixelAt(extrudedMaxRects, 0, 0)[0] == 10 && pixelAt(extrudedMaxRects, 3, 3)[0] == 40, "extrude maxrects duplicates corner pixels");

    SheetPackingSettings extrudeGridSettings;
    extrudeGridSettings.columns = 1;
    extrudeGridSettings.padding = 0;
    extrudeGridSettings.margin = 0;
    extrudeGridSettings.useCustomCellSize = true;
    extrudeGridSettings.customCellWidth = 2;
    extrudeGridSettings.customCellHeight = 2;
    extrudeGridSettings.extrudeBorder = 1;
    const PackedSheet extrudedGrid = buildSpriteSheet({makeCornerImage("corner")}, extrudeGridSettings);
    ok &= expect(extrudedGrid.width == 4 && extrudedGrid.height == 4, "extrude grid expands sheet footprint");
    ok &= expect(extrudedGrid.sprites[0].x == 1 && extrudedGrid.sprites[0].y == 1 && extrudedGrid.sprites[0].w == 2 && extrudedGrid.sprites[0].h == 2, "extrude keeps sprite rect on original image area");
    ok &= expect(extrudedGrid.packingRects[0].x == 0 && extrudedGrid.packingRects[0].y == 0 && extrudedGrid.packingRects[0].w == 4 && extrudedGrid.packingRects[0].h == 4, "extrude packing rect covers duplicated border footprint");
    ok &= expect(pixelAt(extrudedGrid, 0, 0)[0] == 10, "extrude duplicates top-left corner");
    ok &= expect(pixelAt(extrudedGrid, 3, 0)[0] == 20, "extrude duplicates top-right corner");
    ok &= expect(pixelAt(extrudedGrid, 0, 3)[0] == 30, "extrude duplicates bottom-left corner");
    ok &= expect(pixelAt(extrudedGrid, 3, 3)[0] == 40, "extrude duplicates bottom-right corner");
    ok &= expect(pixelAt(extrudedGrid, 1, 1)[0] == 10 && pixelAt(extrudedGrid, 2, 2)[0] == 40, "extrude preserves original pixels inside sprite rect");

    SheetPackingSettings extrudeShelfSettings;
    extrudeShelfSettings.algorithm = PackingAlgorithm::Shelf;
    extrudeShelfSettings.padding = 0;
    extrudeShelfSettings.margin = 0;
    extrudeShelfSettings.maxSheetWidth = 8;
    extrudeShelfSettings.extrudeBorder = 1;
    const PackedSheet extrudedShelf = buildSpriteSheet({makeCornerImage("corner"), makeImage("small", 1, 1, 90, 0, 0, 255)}, extrudeShelfSettings);
    ok &= expect(extrudedShelf.width == 7 && extrudedShelf.height == 4, "extrude shelf uses expanded sprite footprints");
    ok &= expect(extrudedShelf.sprites[0].x == 1 && extrudedShelf.sprites[0].y == 1, "extrude shelf offsets first sprite rect inside border");
    ok &= expect(extrudedShelf.sprites[1].x == 5 && extrudedShelf.sprites[1].y == 1, "extrude shelf offsets second sprite rect inside border");
    ok &= expect(extrudedShelf.packingRects[0].x == 0 && extrudedShelf.packingRects[0].y == 0 && extrudedShelf.packingRects[0].w == 7 && extrudedShelf.packingRects[0].h == 4, "extrude shelf row rect covers expanded footprints");
    ok &= expect(pixelAt(extrudedShelf, 4, 0)[0] == 90 && pixelAt(extrudedShelf, 6, 2)[0] == 90, "extrude shelf duplicates single-pixel sprite border");

    const PackedSheet clampedColumns = buildSpriteSheet({red, green}, 0, -5, -3, 0, 0);
    ok &= expect(clampedColumns.width == 3 && clampedColumns.height == 4, "invalid columns padding margin and cell size are clamped");
    ok &= expect(clampedColumns.sprites[1].x == 0 && clampedColumns.sprites[1].y == 2, "clamped columns places one sprite per row");

    SourceImage invalid = makeImage("bad", 2, 2, 1, 2, 3, 4);
    invalid.pixels.pop_back();
    const PackedSheet invalidSheet = buildSpriteSheet({invalid}, 1, 0, 0, 2, 2);
    ok &= expect(!invalidSheet.valid(), "invalid pixel count rejects sheet");

    const SourceImage unnamed = makeImage("", 1, 1, 9, 8, 7, 6);
    const PackedSheet fallbackName = buildSpriteSheet({unnamed}, 1, 0, 0, 1, 1);
    ok &= expect(fallbackName.sprites[0].name == "sprite_0", "empty source name gets fallback sprite name");

    return ok ? 0 : 1;
}
