#include "sprite_sheet_extractor.h"

#include "image_ops.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>

namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

SpriteRect clampSpriteRectToImage(const SpriteRect& sprite, int imageWidth, int imageHeight)
{
    SpriteRect result = sprite;
    if (imageWidth <= 0 || imageHeight <= 0) {
        result.x = 0;
        result.y = 0;
        result.w = 0;
        result.h = 0;
        return result;
    }

    const int minX = std::clamp(result.x, 0, imageWidth);
    const int minY = std::clamp(result.y, 0, imageHeight);
    const int maxX = std::clamp(result.x + result.w, minX, imageWidth);
    const int maxY = std::clamp(result.y + result.h, minY, imageHeight);
    result.x = minX;
    result.y = minY;
    result.w = maxX - minX;
    result.h = maxY - minY;
    return result;
}

SpriteRect trimSpriteRectTransparentBorder(int imageWidth, int imageHeight, const std::vector<unsigned char>& pixels, const SpriteRect& sprite, const ColorKeySettings& colorKey, int alphaThreshold)
{
    SpriteRect clamped = clampSpriteRectToImage(sprite, imageWidth, imageHeight);
    if (clamped.w <= 0 || clamped.h <= 0 || pixels.empty()) {
        return clamped;
    }

    int minVisibleX = clamped.x + clamped.w;
    int minVisibleY = clamped.y + clamped.h;
    int maxVisibleX = clamped.x - 1;
    int maxVisibleY = clamped.y - 1;

    // 算法原理：透明边裁剪扫描候选矩形内所有像素，记录有效 Alpha 大于阈值的最小/最大坐标，最后用这些边界收紧矩形。
    for (int y = clamped.y; y < clamped.y + clamped.h; ++y) {
        for (int x = clamped.x; x < clamped.x + clamped.w; ++x) {
            const size_t pixelIndex = (static_cast<size_t>(y) * static_cast<size_t>(imageWidth) + static_cast<size_t>(x)) * 4;
            if (pixelIndex + 3 < pixels.size() && effectivePixelAlpha(pixels.data() + pixelIndex, colorKey) > alphaThreshold) {
                minVisibleX = std::min(minVisibleX, x);
                minVisibleY = std::min(minVisibleY, y);
                maxVisibleX = std::max(maxVisibleX, x);
                maxVisibleY = std::max(maxVisibleY, y);
            }
        }
    }

    if (maxVisibleX < minVisibleX || maxVisibleY < minVisibleY) {
        clamped.w = 0;
        clamped.h = 0;
        return clamped;
    }

    clamped.x = minVisibleX;
    clamped.y = minVisibleY;
    clamped.w = maxVisibleX - minVisibleX + 1;
    clamped.h = maxVisibleY - minVisibleY + 1;
    return clamped;
}

std::string applyNamePattern(const std::string& pattern, int index)
{
    const std::string fallback = "sprite_{index}";
    const std::string& source = pattern.empty() ? fallback : pattern;
    std::string result;
    result.reserve(source.size() + 8);

    for (size_t i = 0; i < source.size();) {
        if (source.compare(i, 7, "{index}") == 0) {
            result += std::to_string(index);
            i += 7;
        } else {
            result.push_back(source[i]);
            ++i;
        }
    }
    return result.empty() ? ("sprite_" + std::to_string(index)) : result;
}

} // namespace

std::vector<SpriteRect> extractGridSprites(int imageWidth, int imageHeight, const std::vector<unsigned char>& pixels, const GridExtractorSettings& settings, const ColorKeySettings& colorKey)
{
    std::vector<SpriteRect> result;
    if (imageWidth <= 0 || imageHeight <= 0) {
        return result;
    }

    int cellW = settings.cellW;
    int cellH = settings.cellH;
    int columns = 0;
    int rows = 0;

    // 算法原理：网格拆分先根据“固定格子尺寸”或“指定行列数”计算候选格子，再逐格生成矩形；如果启用透明过滤，则复用有效 Alpha 判断跳过全透明格子。
    if (settings.sizingMode == GridSizingMode::RowsColumns) {
        if (settings.rows <= 0 || settings.columns <= 0) {
            return result;
        }
        const int usableW = imageWidth - (settings.marginX * 2) - settings.offsetX - (settings.spacingX * (settings.columns - 1));
        const int usableH = imageHeight - (settings.marginY * 2) - settings.offsetY - (settings.spacingY * (settings.rows - 1));
        cellW = usableW / settings.columns;
        cellH = usableH / settings.rows;
        columns = settings.columns;
        rows = settings.rows;
    } else {
        if (cellW <= 0 || cellH <= 0) {
            return result;
        }
        columns = (imageWidth - (settings.marginX * 2) - settings.offsetX + settings.spacingX) / std::max(1, cellW + settings.spacingX);
        rows = (imageHeight - (settings.marginY * 2) - settings.offsetY + settings.spacingY) / std::max(1, cellH + settings.spacingY);
    }

    if (cellW <= 0 || cellH <= 0 || columns <= 0 || rows <= 0) {
        return result;
    }

    int index = 0;
    for (int row = 0; row < rows; ++row) {
        const int y = settings.marginY + settings.offsetY + row * (cellH + settings.spacingY);
        if (y + cellH > imageHeight - settings.marginY) {
            continue;
        }
        for (int column = 0; column < columns; ++column) {
            const int x = settings.marginX + settings.offsetX + column * (cellW + settings.spacingX);
            if (x + cellW > imageWidth - settings.marginX) {
                continue;
            }

            SpriteRect sprite;
            sprite.name = "grid_" + std::to_string(index++);
            sprite.x = x;
            sprite.y = y;
            sprite.w = cellW;
            sprite.h = cellH;
            if (!settings.skipTransparentCells || spriteRectHasVisiblePixel(imageWidth, imageHeight, pixels, sprite, settings.alphaThreshold, colorKey)) {
                result.push_back(std::move(sprite));
            }
        }
    }
    return result;
}

std::vector<SpriteRect> extractGridSprites(const SourceImage& image, const GridExtractorSettings& settings, const ColorKeySettings& colorKey)
{
    return extractGridSprites(image.width, image.height, image.pixels, settings, colorKey);
}

std::vector<SpriteRect> extractTransparencyRegionSprites(int imageWidth, int imageHeight, const std::vector<unsigned char>& pixels, const TransparencyRegionSettings& settings, const ColorKeySettings& colorKey)
{
    std::vector<SpriteRect> result;
    if (imageWidth <= 0 || imageHeight <= 0 || pixels.empty()) {
        return result;
    }

    const size_t expectedPixelBytes = static_cast<size_t>(imageWidth) * static_cast<size_t>(imageHeight) * 4;
    if (pixels.size() < expectedPixelBytes) {
        return result;
    }

    const int alphaThreshold = std::clamp(settings.alphaThreshold, 0, 255);
    const int minimumArea = std::max(1, settings.minimumArea);
    const int minimumWidth = std::max(1, settings.minimumWidth);
    const int minimumHeight = std::max(1, settings.minimumHeight);
    std::vector<unsigned char> visited(static_cast<size_t>(imageWidth) * static_cast<size_t>(imageHeight), 0);
    std::vector<int> stack;
    stack.reserve(256);

    const int fourWayDirections[4][2] = {
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1}
    };
    const int eightWayDirections[8][2] = {
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1},
        {1, 1},
        {1, -1},
        {-1, 1},
        {-1, -1}
    };
    const int (*directions)[2] = settings.connectivity == RegionConnectivity::EightWay ? eightWayDirections : fourWayDirections;
    const int directionCount = settings.connectivity == RegionConnectivity::EightWay ? 8 : 4;

    auto isVisiblePixel = [&](int x, int y) {
        const size_t pixelIndex = (static_cast<size_t>(y) * static_cast<size_t>(imageWidth) + static_cast<size_t>(x)) * 4;
        return pixelIndex + 3 < pixels.size() && effectivePixelAlpha(pixels.data() + pixelIndex, colorKey) > alphaThreshold;
    };

    int regionIndex = 0;
    // 算法原理：透明区域拆分把 Alpha/ColorKey 后仍可见的像素视为前景；从每个未访问前景像素出发做栈式 flood fill，按 4/8 邻接收集一个连通块，并用连通块的最小/最大坐标生成紧包围 SpriteRect。
    for (int y = 0; y < imageHeight; ++y) {
        for (int x = 0; x < imageWidth; ++x) {
            const int startIndex = y * imageWidth + x;
            if (visited[static_cast<size_t>(startIndex)] != 0) {
                continue;
            }
            visited[static_cast<size_t>(startIndex)] = 1;
            if (!isVisiblePixel(x, y)) {
                continue;
            }

            int minX = x;
            int minY = y;
            int maxX = x;
            int maxY = y;
            int area = 0;
            stack.clear();
            stack.push_back(startIndex);

            while (!stack.empty()) {
                const int current = stack.back();
                stack.pop_back();
                const int currentX = current % imageWidth;
                const int currentY = current / imageWidth;
                ++area;
                minX = std::min(minX, currentX);
                minY = std::min(minY, currentY);
                maxX = std::max(maxX, currentX);
                maxY = std::max(maxY, currentY);

                for (int i = 0; i < directionCount; ++i) {
                    const int neighborX = currentX + directions[i][0];
                    const int neighborY = currentY + directions[i][1];
                    if (neighborX < 0 || neighborY < 0 || neighborX >= imageWidth || neighborY >= imageHeight) {
                        continue;
                    }
                    const int neighborIndex = neighborY * imageWidth + neighborX;
                    if (visited[static_cast<size_t>(neighborIndex)] != 0) {
                        continue;
                    }
                    visited[static_cast<size_t>(neighborIndex)] = 1;
                    if (isVisiblePixel(neighborX, neighborY)) {
                        stack.push_back(neighborIndex);
                    }
                }
            }

            SpriteRect sprite;
            sprite.name = "region_" + std::to_string(regionIndex++);
            sprite.x = minX;
            sprite.y = minY;
            sprite.w = maxX - minX + 1;
            sprite.h = maxY - minY + 1;
            if (area >= minimumArea && sprite.w >= minimumWidth && sprite.h >= minimumHeight) {
                result.push_back(std::move(sprite));
            }
        }
    }

    return result;
}

std::vector<SpriteRect> extractTransparencyRegionSprites(const SourceImage& image, const TransparencyRegionSettings& settings, const ColorKeySettings& colorKey)
{
    return extractTransparencyRegionSprites(image.width, image.height, image.pixels, settings, colorKey);
}

std::vector<SpriteRect> extractAnimationStripSprites(int imageWidth, int imageHeight, const std::vector<unsigned char>& pixels, const AnimationStripSettings& settings, const ColorKeySettings& colorKey)
{
    std::vector<SpriteRect> result;
    if (imageWidth <= 0 || imageHeight <= 0 || settings.frameCount <= 0) {
        return result;
    }

    const int frameCount = std::max(1, settings.frameCount);
    const int startX = std::clamp(settings.startX, 0, imageWidth);
    const int startY = std::clamp(settings.startY, 0, imageHeight);
    const int spacing = std::max(0, settings.spacing);
    int frameWidth = std::max(1, settings.frameWidth);
    int frameHeight = std::max(1, settings.frameHeight);

    // 算法原理：动画条拆分把图片看作横向或纵向的一维帧序列；先根据帧数、起点、间距和自动/手动帧尺寸计算每一帧矩形，再按顺序生成 frame_N，并可用有效 Alpha 跳过全透明帧。
    if (settings.useAutoFrameSize) {
        if (settings.direction == StripDirection::Horizontal) {
            const int usableWidth = imageWidth - startX - spacing * (frameCount - 1);
            frameWidth = usableWidth / frameCount;
            frameHeight = imageHeight - startY;
        } else {
            frameWidth = imageWidth - startX;
            const int usableHeight = imageHeight - startY - spacing * (frameCount - 1);
            frameHeight = usableHeight / frameCount;
        }
    }

    if (frameWidth <= 0 || frameHeight <= 0) {
        return result;
    }

    int candidateIndex = 0;
    for (int frame = 0; frame < frameCount; ++frame) {
        SpriteRect sprite;
        sprite.name = "frame_" + std::to_string(candidateIndex++);
        sprite.x = startX;
        sprite.y = startY;
        sprite.w = frameWidth;
        sprite.h = frameHeight;
        if (settings.direction == StripDirection::Horizontal) {
            sprite.x = startX + frame * (frameWidth + spacing);
        } else {
            sprite.y = startY + frame * (frameHeight + spacing);
        }

        if (sprite.x >= imageWidth || sprite.y >= imageHeight || sprite.x + sprite.w > imageWidth || sprite.y + sprite.h > imageHeight) {
            continue;
        }
        if (!settings.skipTransparentFrames || spriteRectHasVisiblePixel(imageWidth, imageHeight, pixels, sprite, settings.alphaThreshold, colorKey)) {
            result.push_back(std::move(sprite));
        }
    }

    return result;
}

std::vector<SpriteRect> extractAnimationStripSprites(const SourceImage& image, const AnimationStripSettings& settings, const ColorKeySettings& colorKey)
{
    return extractAnimationStripSprites(image.width, image.height, image.pixels, settings, colorKey);
}

namespace {

bool readAtlasFrameRect(const json& frameObject, SpriteRect& sprite)
{
    if (!frameObject.is_object() || !frameObject.contains("frame") || !frameObject["frame"].is_object()) {
        return false;
    }
    const json& frame = frameObject["frame"];
    if (!frame.contains("x") || !frame.contains("y") || !frame.contains("w") || !frame.contains("h")) {
        return false;
    }

    sprite.x = frame.value("x", 0);
    sprite.y = frame.value("y", 0);
    sprite.w = frame.value("w", 0);
    sprite.h = frame.value("h", 0);
    return sprite.w > 0 && sprite.h > 0;
}

std::string metadataFrameName(const std::string& rawName, int index, bool preserveNames)
{
    if (!preserveNames || rawName.empty()) {
        return "metadata_" + std::to_string(index);
    }
    return fs::path(rawName).stem().string();
}

void appendAtlasFrame(const json& frameObject, const std::string& rawName, int candidateIndex, const MetadataImportSettings& settings, MetadataImportResult& result)
{
    const bool rotated = frameObject.is_object() && frameObject.value("rotated", false);
    if (rotated && settings.skipRotatedFrames) {
        result.warnings.push_back("Skipped rotated frame: " + (rawName.empty() ? std::to_string(candidateIndex) : rawName));
        return;
    }

    SpriteRect sprite;
    if (!readAtlasFrameRect(frameObject, sprite)) {
        result.warnings.push_back("Skipped invalid metadata frame: " + (rawName.empty() ? std::to_string(candidateIndex) : rawName));
        return;
    }
    if (rotated) {
        result.warnings.push_back("Imported rotated frame without rotation transform: " + (rawName.empty() ? std::to_string(candidateIndex) : rawName));
    }
    sprite.name = metadataFrameName(rawName, static_cast<int>(result.sprites.size()), settings.preserveNames);
    result.sprites.push_back(std::move(sprite));
}

} // namespace

MetadataImportResult importMetadataSpritesFromJson(const std::string& jsonText, const MetadataImportSettings& settings)
{
    MetadataImportResult result;
    json root;
    try {
        root = json::parse(jsonText);
    } catch (const std::exception& e) {
        result.warnings.push_back(std::string("Failed to parse metadata JSON: ") + e.what());
        return result;
    }

    if (!root.is_object() || !root.contains("frames")) {
        result.warnings.push_back("Unsupported metadata JSON: expected top-level frames from Aseprite or TexturePacker.");
        return result;
    }

    const json& frames = root["frames"];
    const bool looksLikeTexturePacker = root.contains("meta") && root["meta"].is_object() && root["meta"].value("app", std::string{}).find("TexturePacker") != std::string::npos;
    if (settings.format == MetadataImportFormat::TexturePackerJson || (settings.format == MetadataImportFormat::AutoDetect && looksLikeTexturePacker)) {
        // TexturePacker JSON atlas format: https://www.codeandweb.com/texturepacker/documentation/json
        result.detectedFormat = "TexturePacker JSON";
    } else {
        // Aseprite JSON sprite sheet data format: https://www.aseprite.org/docs/sprite-sheet/
        result.detectedFormat = "Aseprite JSON";
    }

    if (frames.is_object()) {
        int candidateIndex = 0;
        for (auto it = frames.begin(); it != frames.end(); ++it) {
            appendAtlasFrame(it.value(), it.key(), candidateIndex++, settings, result);
        }
    } else if (frames.is_array()) {
        for (int i = 0; i < static_cast<int>(frames.size()); ++i) {
            const json& frameObject = frames[static_cast<size_t>(i)];
            const std::string rawName = frameObject.is_object() ? frameObject.value("filename", std::string{}) : std::string{};
            appendAtlasFrame(frameObject, rawName, i, settings, result);
        }
    } else {
        result.warnings.push_back("Unsupported metadata JSON: frames must be an object or array.");
    }

    if (result.sprites.empty() && result.warnings.empty()) {
        result.warnings.push_back("Metadata JSON contained no importable frames.");
    }
    return result;
}

std::vector<SpriteRect> postProcessSprites(int imageWidth, int imageHeight, const std::vector<unsigned char>& pixels, const std::vector<SpriteRect>& sprites, const SpritePostProcessSettings& settings, const ColorKeySettings& colorKey, int alphaThreshold)
{
    std::vector<SpriteRect> result;
    result.reserve(sprites.size());

    int outputIndex = 0;
    for (const SpriteRect& sprite : sprites) {
        SpriteRect processed = settings.clampToImageBounds ? clampSpriteRectToImage(sprite, imageWidth, imageHeight) : sprite;
        if (settings.trimTransparentBorder) {
            processed = trimSpriteRectTransparentBorder(imageWidth, imageHeight, pixels, processed, colorKey, alphaThreshold);
        }
        if (processed.w <= 0 || processed.h <= 0) {
            continue;
        }
        if (settings.renameSprites) {
            processed.name = applyNamePattern(settings.namePattern, outputIndex);
        }
        result.push_back(std::move(processed));
        ++outputIndex;
    }

    return result;
}

std::vector<SpriteRect> postProcessSprites(const SourceImage& image, const std::vector<SpriteRect>& sprites, const SpritePostProcessSettings& settings, const ColorKeySettings& colorKey, int alphaThreshold)
{
    return postProcessSprites(image.width, image.height, image.pixels, sprites, settings, colorKey, alphaThreshold);
}
