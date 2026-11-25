#pragma once

#include "sprite_types.h"

#include <string>
#include <vector>

enum class GridSizingMode {
    CellSize,
    RowsColumns
};

struct GridExtractorSettings {
    GridSizingMode sizingMode = GridSizingMode::CellSize;
    int cellW = 32;
    int cellH = 32;
    int rows = 1;
    int columns = 1;
    int spacingX = 0;
    int spacingY = 0;
    int marginX = 0;
    int marginY = 0;
    int offsetX = 0;
    int offsetY = 0;
    int alphaThreshold = 0;
    bool skipTransparentCells = true;
};

enum class RegionConnectivity {
    FourWay,
    EightWay
};

struct TransparencyRegionSettings {
    int alphaThreshold = 0;
    RegionConnectivity connectivity = RegionConnectivity::FourWay;
    int minimumArea = 4;
    int minimumWidth = 1;
    int minimumHeight = 1;
};

enum class StripDirection {
    Horizontal,
    Vertical
};

struct AnimationStripSettings {
    StripDirection direction = StripDirection::Horizontal;
    int frameCount = 1;
    bool useAutoFrameSize = true;
    int frameWidth = 32;
    int frameHeight = 32;
    int startX = 0;
    int startY = 0;
    int spacing = 0;
    bool skipTransparentFrames = false;
    int alphaThreshold = 0;
};

enum class MetadataImportFormat {
    AutoDetect,
    AsepriteJson,
    TexturePackerJson
};

struct MetadataImportSettings {
    MetadataImportFormat format = MetadataImportFormat::AutoDetect;
    bool preserveNames = true;
    bool skipRotatedFrames = true;
};

struct MetadataImportResult {
    std::vector<SpriteRect> sprites;
    std::vector<std::string> warnings;
    std::string detectedFormat;

    [[nodiscard]] bool valid() const
    {
        return !sprites.empty();
    }
};

struct SpritePostProcessSettings {
    bool trimTransparentBorder = false;
    bool renameSprites = false;
    std::string namePattern = "sprite_{index}";
    bool clampToImageBounds = true;
};

std::vector<SpriteRect> extractGridSprites(int imageWidth, int imageHeight, const std::vector<unsigned char>& pixels, const GridExtractorSettings& settings, const ColorKeySettings& colorKey);
std::vector<SpriteRect> extractGridSprites(const SourceImage& image, const GridExtractorSettings& settings, const ColorKeySettings& colorKey);
std::vector<SpriteRect> extractTransparencyRegionSprites(int imageWidth, int imageHeight, const std::vector<unsigned char>& pixels, const TransparencyRegionSettings& settings, const ColorKeySettings& colorKey);
std::vector<SpriteRect> extractTransparencyRegionSprites(const SourceImage& image, const TransparencyRegionSettings& settings, const ColorKeySettings& colorKey);
std::vector<SpriteRect> extractAnimationStripSprites(int imageWidth, int imageHeight, const std::vector<unsigned char>& pixels, const AnimationStripSettings& settings, const ColorKeySettings& colorKey);
std::vector<SpriteRect> extractAnimationStripSprites(const SourceImage& image, const AnimationStripSettings& settings, const ColorKeySettings& colorKey);
MetadataImportResult importMetadataSpritesFromJson(const std::string& jsonText, const MetadataImportSettings& settings);
std::vector<SpriteRect> postProcessSprites(int imageWidth, int imageHeight, const std::vector<unsigned char>& pixels, const std::vector<SpriteRect>& sprites, const SpritePostProcessSettings& settings, const ColorKeySettings& colorKey, int alphaThreshold);
std::vector<SpriteRect> postProcessSprites(const SourceImage& image, const std::vector<SpriteRect>& sprites, const SpritePostProcessSettings& settings, const ColorKeySettings& colorKey, int alphaThreshold);
