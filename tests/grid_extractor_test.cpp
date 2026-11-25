#include "../src/sprite_sheet_extractor.h"

#include <cstdlib>
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

static void setAlpha(std::vector<unsigned char>& pixels, int imageWidth, int x, int y, unsigned char alpha)
{
    const size_t alphaIndex = (static_cast<size_t>(y) * static_cast<size_t>(imageWidth) + static_cast<size_t>(x)) * 4 + 3;
    pixels[alphaIndex] = alpha;
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

    GridExtractorSettings grid;
    grid.cellW = 16;
    grid.cellH = 16;
    grid.spacingX = 0;
    grid.spacingY = 0;

    const std::vector<SpriteRect> simple = extractGridSprites(64, 32, {}, grid, {});
    ok &= expect(simple.size() == 8, "64x32 image with 16x16 cells should produce 8 sprites when pixels are unavailable");
    ok &= expect(simple.front().name == "grid_0", "first simple sprite name");
    ok &= expect(simple.front().x == 0, "first simple sprite x");
    ok &= expect(simple.front().y == 0, "first simple sprite y");
    ok &= expect(simple.back().name == "grid_7", "last simple sprite name");
    ok &= expect(simple.back().x == 48, "last simple sprite x");
    ok &= expect(simple.back().y == 16, "last simple sprite y");

    grid.spacingX = 2;
    grid.spacingY = 4;
    grid.marginX = 1;
    grid.marginY = 2;
    grid.offsetX = 3;
    grid.offsetY = 5;

    const std::vector<SpriteRect> offset = extractGridSprites(40, 45, {}, grid, {});
    ok &= expect(offset.size() == 4, "offset grid should produce two columns and two rows");
    ok &= expect(offset[0].x == 4, "first offset sprite x");
    ok &= expect(offset[0].y == 7, "first offset sprite y");
    ok &= expect(offset[1].x == 22, "second offset sprite x");
    ok &= expect(offset[1].y == 7, "second offset sprite y");
    ok &= expect(offset[2].x == 4, "third offset sprite x");
    ok &= expect(offset[2].y == 27, "third offset sprite y");

    GridExtractorSettings invalid;
    invalid.cellW = 0;
    invalid.cellH = 16;
    const std::vector<SpriteRect> invalidResult = extractGridSprites(64, 64, {}, invalid, {});
    ok &= expect(invalidResult.empty(), "invalid zero-width cell should produce no sprites");

    GridExtractorSettings rowsColumns;
    rowsColumns.sizingMode = GridSizingMode::RowsColumns;
    rowsColumns.rows = 2;
    rowsColumns.columns = 4;
    const std::vector<SpriteRect> byRowsColumns = extractGridSprites(64, 32, {}, rowsColumns, {});
    ok &= expect(byRowsColumns.size() == 8, "2x4 rows-columns grid should produce 8 sprites");
    ok &= expect(byRowsColumns.front().w == 16, "rows-columns computed cell width");
    ok &= expect(byRowsColumns.front().h == 16, "rows-columns computed cell height");
    ok &= expect(byRowsColumns.back().x == 48, "rows-columns last sprite x");
    ok &= expect(byRowsColumns.back().y == 16, "rows-columns last sprite y");

    rowsColumns.marginX = 2;
    rowsColumns.marginY = 1;
    rowsColumns.spacingX = 2;
    rowsColumns.spacingY = 2;
    const std::vector<SpriteRect> byRowsColumnsWithSpacing = extractGridSprites(70, 37, {}, rowsColumns, {});
    ok &= expect(byRowsColumnsWithSpacing.size() == 8, "rows-columns grid with margins and spacing should produce 8 sprites");
    ok &= expect(byRowsColumnsWithSpacing.front().x == 2, "rows-columns margin x applied");
    ok &= expect(byRowsColumnsWithSpacing.front().y == 1, "rows-columns margin y applied");
    ok &= expect(byRowsColumnsWithSpacing.front().w == 15, "rows-columns computed width with spacing");
    ok &= expect(byRowsColumnsWithSpacing.front().h == 16, "rows-columns computed height with spacing");
    ok &= expect(byRowsColumnsWithSpacing[1].x == 19, "rows-columns spacing x applied");
    ok &= expect(byRowsColumnsWithSpacing[4].y == 19, "rows-columns spacing y applied");

    rowsColumns.columns = 0;
    ok &= expect(extractGridSprites(64, 32, {}, rowsColumns, {}).empty(), "invalid rows-columns count should produce no sprites");

    GridExtractorSettings alphaGrid;
    alphaGrid.cellW = 2;
    alphaGrid.cellH = 2;
    alphaGrid.skipTransparentCells = true;
    std::vector<unsigned char> pixels(4 * 2 * 4, 0);
    setAlpha(pixels, 4, 0, 0, 255);
    setAlpha(pixels, 4, 3, 1, 16);

    const std::vector<SpriteRect> filtered = extractGridSprites(4, 2, pixels, alphaGrid, {});
    ok &= expect(filtered.size() == 2, "two cells with alpha above threshold should be kept");
    ok &= expect(filtered[0].name == "grid_0", "first kept transparent-filtered sprite keeps candidate name");
    ok &= expect(filtered[1].name == "grid_1", "second kept transparent-filtered sprite keeps candidate name");

    alphaGrid.alphaThreshold = 16;
    const std::vector<SpriteRect> thresholded = extractGridSprites(4, 2, pixels, alphaGrid, {});
    ok &= expect(thresholded.size() == 1, "alpha equal to threshold should be treated as transparent");
    ok &= expect(thresholded[0].x == 0, "thresholded grid should keep only the left visible cell");

    alphaGrid.skipTransparentCells = false;
    const std::vector<SpriteRect> unfiltered = extractGridSprites(4, 2, pixels, alphaGrid, {});
    ok &= expect(unfiltered.size() == 2, "disabled transparent filtering should keep all cells");

    GridExtractorSettings alphaRowsColumns;
    alphaRowsColumns.sizingMode = GridSizingMode::RowsColumns;
    alphaRowsColumns.rows = 1;
    alphaRowsColumns.columns = 2;
    alphaRowsColumns.skipTransparentCells = true;
    const std::vector<SpriteRect> filteredRowsColumns = extractGridSprites(4, 2, pixels, alphaRowsColumns, {});
    ok &= expect(filteredRowsColumns.size() == 2, "rows-columns mode should reuse transparent filtering");
    alphaRowsColumns.alphaThreshold = 16;
    const std::vector<SpriteRect> thresholdedRowsColumns = extractGridSprites(4, 2, pixels, alphaRowsColumns, {});
    ok &= expect(thresholdedRowsColumns.size() == 1, "rows-columns threshold should filter transparent cells");

    GridExtractorSettings colorKeyGrid;
    colorKeyGrid.cellW = 2;
    colorKeyGrid.cellH = 2;
    colorKeyGrid.skipTransparentCells = true;
    std::vector<unsigned char> keyPixels(4 * 2 * 4, 255);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 4; ++x) {
            setPixel(keyPixels, 4, x, y, 255, 0, 255, 255);
        }
    }
    setPixel(keyPixels, 4, 2, 0, 10, 20, 30, 255);
    ColorKeySettings colorKey;
    colorKey.enabled = true;
    colorKey.r = 255;
    colorKey.g = 0;
    colorKey.b = 255;
    const std::vector<SpriteRect> colorKeyFiltered = extractGridSprites(4, 2, keyPixels, colorKeyGrid, colorKey);
    ok &= expect(colorKeyFiltered.size() == 1, "color-key-only cell should be treated as transparent");
    ok &= expect(colorKeyFiltered[0].x == 2, "color-key filtering keeps visible right cell");

    SourceImage sourceImage;
    sourceImage.width = 4;
    sourceImage.height = 2;
    sourceImage.pixels = keyPixels;
    const std::vector<SpriteRect> sourceImageFiltered = extractGridSprites(sourceImage, colorKeyGrid, colorKey);
    ok &= expect(sourceImageFiltered.size() == 1, "SourceImage overload should use shared grid extraction logic");
    ok &= expect(sourceImageFiltered[0].x == colorKeyFiltered[0].x, "SourceImage overload should preserve sprite rects");

    SpritePostProcessSettings trimSettings;
    trimSettings.trimTransparentBorder = true;
    std::vector<unsigned char> trimPixels(6 * 6 * 4, 0);
    setAlpha(trimPixels, 6, 2, 1, 255);
    setAlpha(trimPixels, 6, 4, 3, 255);
    SpriteRect padded;
    padded.name = "padded";
    padded.x = 1;
    padded.y = 1;
    padded.w = 4;
    padded.h = 4;
    const std::vector<SpriteRect> trimmed = postProcessSprites(6, 6, trimPixels, {padded}, trimSettings, {}, 0);
    ok &= expect(trimmed.size() == 1, "trim post process should keep visible sprite");
    ok &= expect(trimmed[0].x == 2 && trimmed[0].y == 1 && trimmed[0].w == 3 && trimmed[0].h == 3, "trim post process should shrink transparent border");
    ok &= expect(trimmed[0].name == "padded", "trim post process should preserve name by default");

    SpritePostProcessSettings renameSettings;
    renameSettings.renameSprites = true;
    renameSettings.namePattern = "idle_{index}";
    const std::vector<SpriteRect> renamed = postProcessSprites(6, 6, trimPixels, {padded, padded}, renameSettings, {}, 0);
    ok &= expect(renamed.size() == 2, "rename post process should keep valid sprites");
    ok &= expect(renamed[0].name == "idle_0" && renamed[1].name == "idle_1", "rename post process should apply index pattern");

    SpritePostProcessSettings clampSettings;
    clampSettings.clampToImageBounds = true;
    SpriteRect overflow;
    overflow.name = "overflow";
    overflow.x = 4;
    overflow.y = 4;
    overflow.w = 4;
    overflow.h = 4;
    const std::vector<SpriteRect> clamped = postProcessSprites(6, 6, trimPixels, {overflow}, clampSettings, {}, 0);
    ok &= expect(clamped.size() == 1, "clamp post process should keep partially visible sprite");
    ok &= expect(clamped[0].x == 4 && clamped[0].y == 4 && clamped[0].w == 2 && clamped[0].h == 2, "clamp post process should clamp to image bounds");

    GridExtractorSettings postGrid;
    postGrid.cellW = 6;
    postGrid.cellH = 6;
    postGrid.skipTransparentCells = true;
    std::vector<SpriteRect> postGridSprites = extractGridSprites(6, 6, trimPixels, postGrid, {});
    SpritePostProcessSettings combinedSettings;
    combinedSettings.trimTransparentBorder = true;
    combinedSettings.renameSprites = true;
    combinedSettings.namePattern = "sprite_{index}";
    const std::vector<SpriteRect> combined = postProcessSprites(6, 6, trimPixels, postGridSprites, combinedSettings, {}, postGrid.alphaThreshold);
    ok &= expect(combined.size() == 1, "combined grid and post process should keep visible extracted cell");
    ok &= expect(combined[0].name == "sprite_0", "combined post process should rename extracted sprite");
    ok &= expect(combined[0].x == 2 && combined[0].y == 1 && combined[0].w == 3 && combined[0].h == 3, "combined post process should trim extracted sprite");

    TransparencyRegionSettings regionSettings;
    regionSettings.minimumArea = 1;
    std::vector<unsigned char> regionPixels(6 * 4 * 4, 0);
    setAlpha(regionPixels, 6, 1, 1, 255);
    setAlpha(regionPixels, 6, 2, 1, 255);
    setAlpha(regionPixels, 6, 4, 2, 255);
    const std::vector<SpriteRect> regions = extractTransparencyRegionSprites(6, 4, regionPixels, regionSettings, {});
    ok &= expect(regions.size() == 2, "transparency regions should find separate visible components");
    ok &= expect(regions[0].name == "region_0" && regions[0].x == 1 && regions[0].y == 1 && regions[0].w == 2 && regions[0].h == 1, "first transparency region bounds");
    ok &= expect(regions[1].name == "region_1" && regions[1].x == 4 && regions[1].y == 2 && regions[1].w == 1 && regions[1].h == 1, "second transparency region bounds");

    std::vector<unsigned char> diagonalPixels(3 * 3 * 4, 0);
    setAlpha(diagonalPixels, 3, 0, 0, 255);
    setAlpha(diagonalPixels, 3, 1, 1, 255);
    regionSettings.connectivity = RegionConnectivity::FourWay;
    const std::vector<SpriteRect> fourWay = extractTransparencyRegionSprites(3, 3, diagonalPixels, regionSettings, {});
    ok &= expect(fourWay.size() == 2, "4-way connectivity should keep diagonal pixels separate");
    regionSettings.connectivity = RegionConnectivity::EightWay;
    const std::vector<SpriteRect> eightWay = extractTransparencyRegionSprites(3, 3, diagonalPixels, regionSettings, {});
    ok &= expect(eightWay.size() == 1, "8-way connectivity should connect diagonal pixels");
    ok &= expect(eightWay[0].x == 0 && eightWay[0].y == 0 && eightWay[0].w == 2 && eightWay[0].h == 2, "8-way connected region bounds");

    regionSettings = TransparencyRegionSettings{};
    regionSettings.minimumArea = 2;
    const std::vector<SpriteRect> minimumAreaRegions = extractTransparencyRegionSprites(6, 4, regionPixels, regionSettings, {});
    ok &= expect(minimumAreaRegions.size() == 1, "minimum area should filter small noise regions");
    regionSettings.minimumArea = 1;
    regionSettings.minimumWidth = 2;
    const std::vector<SpriteRect> minimumWidthRegions = extractTransparencyRegionSprites(6, 4, regionPixels, regionSettings, {});
    ok &= expect(minimumWidthRegions.size() == 1, "minimum width should filter narrow regions");
    regionSettings.minimumWidth = 1;
    regionSettings.minimumHeight = 2;
    ok &= expect(extractTransparencyRegionSprites(6, 4, regionPixels, regionSettings, {}).empty(), "minimum height should filter short regions");

    regionSettings = TransparencyRegionSettings{};
    regionSettings.minimumArea = 1;
    regionSettings.alphaThreshold = 128;
    std::vector<unsigned char> thresholdPixels(3 * 1 * 4, 0);
    setAlpha(thresholdPixels, 3, 0, 0, 128);
    setAlpha(thresholdPixels, 3, 2, 0, 129);
    const std::vector<SpriteRect> thresholdRegions = extractTransparencyRegionSprites(3, 1, thresholdPixels, regionSettings, {});
    ok &= expect(thresholdRegions.size() == 1, "region alpha threshold should require alpha greater than threshold");
    ok &= expect(thresholdRegions[0].x == 2, "region alpha threshold should keep only high-alpha pixel");

    regionSettings.alphaThreshold = 0;
    const std::vector<SpriteRect> colorKeyRegions = extractTransparencyRegionSprites(4, 2, keyPixels, regionSettings, colorKey);
    ok &= expect(colorKeyRegions.size() == 1, "transparency regions should treat color key pixels as transparent");
    ok &= expect(colorKeyRegions[0].x == 2 && colorKeyRegions[0].y == 0 && colorKeyRegions[0].w == 1 && colorKeyRegions[0].h == 1, "color key transparency region bounds");

    SourceImage regionSource;
    regionSource.width = 6;
    regionSource.height = 4;
    regionSource.pixels = regionPixels;
    const std::vector<SpriteRect> sourceRegions = extractTransparencyRegionSprites(regionSource, regionSettings, {});
    ok &= expect(sourceRegions.size() == 2, "SourceImage overload should use transparency region extraction logic");

    SpritePostProcessSettings regionPostProcess;
    regionPostProcess.renameSprites = true;
    regionPostProcess.namePattern = "region_sprite_{index}";
    const std::vector<SpriteRect> processedRegions = postProcessSprites(6, 4, regionPixels, regions, regionPostProcess, {}, regionSettings.alphaThreshold);
    ok &= expect(processedRegions.size() == 2, "region extraction should compose with post process");
    ok &= expect(processedRegions[0].name == "region_sprite_0" && processedRegions[1].name == "region_sprite_1", "region post process should rename regions");

    AnimationStripSettings stripSettings;
    stripSettings.frameCount = 3;
    stripSettings.useAutoFrameSize = true;
    const std::vector<SpriteRect> horizontalStrip = extractAnimationStripSprites(12, 4, {}, stripSettings, {});
    ok &= expect(horizontalStrip.size() == 3, "horizontal animation strip should produce frame count sprites");
    ok &= expect(horizontalStrip[0].name == "frame_0" && horizontalStrip[0].x == 0 && horizontalStrip[0].y == 0 && horizontalStrip[0].w == 4 && horizontalStrip[0].h == 4, "first horizontal strip frame bounds");
    ok &= expect(horizontalStrip[2].x == 8 && horizontalStrip[2].w == 4, "last horizontal strip frame bounds");

    stripSettings.direction = StripDirection::Vertical;
    const std::vector<SpriteRect> verticalStrip = extractAnimationStripSprites(4, 12, {}, stripSettings, {});
    ok &= expect(verticalStrip.size() == 3, "vertical animation strip should produce frame count sprites");
    ok &= expect(verticalStrip[0].w == 4 && verticalStrip[0].h == 4, "vertical strip auto frame size");
    ok &= expect(verticalStrip[2].x == 0 && verticalStrip[2].y == 8, "last vertical strip frame position");

    AnimationStripSettings manualStrip;
    manualStrip.frameCount = 3;
    manualStrip.useAutoFrameSize = false;
    manualStrip.frameWidth = 2;
    manualStrip.frameHeight = 3;
    manualStrip.startX = 1;
    manualStrip.startY = 2;
    manualStrip.spacing = 1;
    const std::vector<SpriteRect> manualFrames = extractAnimationStripSprites(10, 6, {}, manualStrip, {});
    ok &= expect(manualFrames.size() == 3, "manual horizontal strip should respect frame count");
    ok &= expect(manualFrames[0].x == 1 && manualFrames[0].y == 2 && manualFrames[0].w == 2 && manualFrames[0].h == 3, "manual strip first frame bounds");
    ok &= expect(manualFrames[1].x == 4 && manualFrames[2].x == 7, "manual strip spacing should advance frames");

    AnimationStripSettings transparentStrip;
    transparentStrip.frameCount = 3;
    transparentStrip.useAutoFrameSize = false;
    transparentStrip.frameWidth = 2;
    transparentStrip.frameHeight = 2;
    transparentStrip.skipTransparentFrames = true;
    std::vector<unsigned char> stripPixels(6 * 2 * 4, 0);
    setAlpha(stripPixels, 6, 0, 0, 255);
    setAlpha(stripPixels, 6, 5, 1, 16);
    const std::vector<SpriteRect> visibleFrames = extractAnimationStripSprites(6, 2, stripPixels, transparentStrip, {});
    ok &= expect(visibleFrames.size() == 2, "strip transparent filtering should keep frames with visible pixels");
    transparentStrip.alphaThreshold = 16;
    const std::vector<SpriteRect> thresholdFrames = extractAnimationStripSprites(6, 2, stripPixels, transparentStrip, {});
    ok &= expect(thresholdFrames.size() == 1, "strip alpha threshold should filter weak visible frames");
    ok &= expect(thresholdFrames[0].x == 0, "strip alpha threshold should keep first frame");

    AnimationStripSettings colorKeyStrip;
    colorKeyStrip.frameCount = 2;
    colorKeyStrip.useAutoFrameSize = false;
    colorKeyStrip.frameWidth = 2;
    colorKeyStrip.frameHeight = 2;
    colorKeyStrip.skipTransparentFrames = true;
    const std::vector<SpriteRect> colorKeyFrames = extractAnimationStripSprites(4, 2, keyPixels, colorKeyStrip, colorKey);
    ok &= expect(colorKeyFrames.size() == 1, "strip transparent filtering should treat color key pixels as transparent");
    ok &= expect(colorKeyFrames[0].x == 2, "strip color key filtering should keep visible frame");

    SourceImage stripSource;
    stripSource.width = 6;
    stripSource.height = 2;
    stripSource.pixels = stripPixels;
    transparentStrip.alphaThreshold = 0;
    const std::vector<SpriteRect> sourceStripFrames = extractAnimationStripSprites(stripSource, transparentStrip, {});
    ok &= expect(sourceStripFrames.size() == 2, "SourceImage overload should use animation strip extraction logic");

    SpritePostProcessSettings stripPostProcess;
    stripPostProcess.renameSprites = true;
    stripPostProcess.namePattern = "walk_{index}";
    const std::vector<SpriteRect> processedStripFrames = postProcessSprites(12, 4, {}, horizontalStrip, stripPostProcess, {}, stripSettings.alphaThreshold);
    ok &= expect(processedStripFrames.size() == 3, "animation strip should compose with post process");
    ok &= expect(processedStripFrames[0].name == "walk_0" && processedStripFrames[2].name == "walk_2", "animation strip post process should rename frames");

    MetadataImportSettings metadataSettings;
    const std::string asepriteArrayJson = R"json({
        "frames": [
            {"filename": "idle_0.png", "frame": {"x": 0, "y": 1, "w": 16, "h": 24}, "rotated": false, "duration": 100},
            {"filename": "idle_1.png", "frame": {"x": 16, "y": 1, "w": 16, "h": 24}, "rotated": false, "duration": 100}
        ],
        "meta": {"app": "https://www.aseprite.org/", "image": "sheet.png"}
    })json";
    const MetadataImportResult asepriteArray = importMetadataSpritesFromJson(asepriteArrayJson, metadataSettings);
    ok &= expect(asepriteArray.sprites.size() == 2, "aseprite array metadata should import frames");
    ok &= expect(asepriteArray.detectedFormat == "Aseprite JSON", "aseprite array metadata should detect Aseprite format");
    ok &= expect(asepriteArray.sprites[0].name == "idle_0" && asepriteArray.sprites[0].x == 0 && asepriteArray.sprites[0].y == 1 && asepriteArray.sprites[0].w == 16 && asepriteArray.sprites[0].h == 24, "aseprite array first frame data");

    const std::string texturePackerHashJson = R"json({
        "frames": {
            "run_0.png": {"frame": {"x": 2, "y": 3, "w": 10, "h": 11}, "rotated": false},
            "run_1.png": {"frame": {"x": 12, "y": 3, "w": 10, "h": 11}, "rotated": true}
        },
        "meta": {"app": "TexturePacker", "image": "sheet.png"}
    })json";
    MetadataImportResult texturePackerHash = importMetadataSpritesFromJson(texturePackerHashJson, metadataSettings);
    ok &= expect(texturePackerHash.sprites.size() == 1, "texturepacker hash metadata should skip rotated frames by default");
    ok &= expect(texturePackerHash.detectedFormat == "TexturePacker JSON", "texturepacker hash metadata should detect TexturePacker format");
    ok &= expect(texturePackerHash.sprites[0].name == "run_0" && texturePackerHash.sprites[0].x == 2 && texturePackerHash.sprites[0].w == 10, "texturepacker hash first frame data");
    ok &= expect(texturePackerHash.warnings.size() == 1, "texturepacker hash metadata should warn for skipped rotated frame");

    metadataSettings.preserveNames = false;
    const MetadataImportResult generatedNames = importMetadataSpritesFromJson(asepriteArrayJson, metadataSettings);
    ok &= expect(generatedNames.sprites.size() == 2, "metadata import should still import when preserve names is off");
    ok &= expect(generatedNames.sprites[0].name == "metadata_0" && generatedNames.sprites[1].name == "metadata_1", "metadata import should generate names when preserve names is off");

    metadataSettings.preserveNames = true;
    metadataSettings.skipRotatedFrames = false;
    const MetadataImportResult rotatedImported = importMetadataSpritesFromJson(texturePackerHashJson, metadataSettings);
    ok &= expect(rotatedImported.sprites.size() == 2, "metadata import should include rotated frames when skip is disabled");
    ok &= expect(!rotatedImported.warnings.empty(), "metadata import should warn when importing rotated frame without transform");

    const MetadataImportResult invalidJson = importMetadataSpritesFromJson("not json", MetadataImportSettings{});
    ok &= expect(invalidJson.sprites.empty(), "invalid metadata JSON should import no frames");
    ok &= expect(!invalidJson.warnings.empty(), "invalid metadata JSON should report warning");

    const MetadataImportResult unsupportedJson = importMetadataSpritesFromJson(R"json({"sprites": []})json", MetadataImportSettings{});
    ok &= expect(unsupportedJson.sprites.empty(), "SpriteEd/simple JSON should not be imported in third-party-only metadata MVP");
    ok &= expect(!unsupportedJson.warnings.empty(), "unsupported metadata schema should report warning");

    const std::vector<SpriteRect> metadataPostProcessed = postProcessSprites(32, 32, {}, asepriteArray.sprites, stripPostProcess, {}, 0);
    ok &= expect(metadataPostProcessed.size() == 2, "metadata import should compose with post process");
    ok &= expect(metadataPostProcessed[0].name == "walk_0" && metadataPostProcessed[1].name == "walk_1", "metadata post process should rename frames");

    return ok ? 0 : 1;
}
