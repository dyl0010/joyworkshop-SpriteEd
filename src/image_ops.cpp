#include "image_ops.h"

#include <algorithm>
#include <cmath>

bool pixelMatchesColorKey(const unsigned char* pixel, const ColorKeySettings& colorKey)
{
    if (!colorKey.enabled) {
        return false;
    }

    return std::abs(static_cast<int>(pixel[0]) - colorKey.r) <= colorKey.tolerance
        && std::abs(static_cast<int>(pixel[1]) - colorKey.g) <= colorKey.tolerance
        && std::abs(static_cast<int>(pixel[2]) - colorKey.b) <= colorKey.tolerance;
}

unsigned char effectivePixelAlpha(const unsigned char* pixel, const ColorKeySettings& colorKey)
{
    return pixelMatchesColorKey(pixel, colorKey) ? 0 : pixel[3];
}

bool spriteRectHasVisiblePixel(int imageWidth, int imageHeight, const std::vector<unsigned char>& pixels, const SpriteRect& sprite, int alphaThreshold, const ColorKeySettings& colorKey)
{
    if (pixels.empty()) {
        return true;
    }

    const int minX = std::clamp(sprite.x, 0, std::max(0, imageWidth - 1));
    const int minY = std::clamp(sprite.y, 0, std::max(0, imageHeight - 1));
    const int maxX = std::clamp(sprite.x + sprite.w, minX, imageWidth);
    const int maxY = std::clamp(sprite.y + sprite.h, minY, imageHeight);
    for (int y = minY; y < maxY; ++y) {
        for (int x = minX; x < maxX; ++x) {
            const size_t pixelIndex = (static_cast<size_t>(y) * static_cast<size_t>(imageWidth) + static_cast<size_t>(x)) * 4;
            if (pixelIndex + 3 < pixels.size() && effectivePixelAlpha(pixels.data() + pixelIndex, colorKey) > alphaThreshold) {
                return true;
            }
        }
    }
    return false;
}

std::string sanitizedSpriteFilename(const std::string& name)
{
    std::string result;
    result.reserve(name.size());
    for (const char ch : name) {
        const bool allowed = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
        result.push_back(allowed ? ch : '_');
    }
    while (!result.empty() && result.front() == '_') {
        result.erase(result.begin());
    }
    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }
    return result.empty() ? "sprite" : result;
}

std::vector<unsigned char> cropSpritePixels(int imageWidth, int imageHeight, const std::vector<unsigned char>& sourcePixels, const SpriteRect& sprite, int& outW, int& outH, const ColorKeySettings& colorKey)
{
    if (imageWidth <= 0 || imageHeight <= 0 || sourcePixels.empty()) {
        outW = 0;
        outH = 0;
        return {};
    }

    const int spriteX = std::clamp(sprite.x, 0, std::max(0, imageWidth - 1));
    const int spriteY = std::clamp(sprite.y, 0, std::max(0, imageHeight - 1));
    outW = std::clamp(sprite.w, 1, std::max(1, imageWidth - spriteX));
    outH = std::clamp(sprite.h, 1, std::max(1, imageHeight - spriteY));

    std::vector<unsigned char> cropped(static_cast<size_t>(outW) * static_cast<size_t>(outH) * 4, 0);
    for (int y = 0; y < outH; ++y) {
        const size_t srcOffset = (static_cast<size_t>(spriteY + y) * static_cast<size_t>(imageWidth) + static_cast<size_t>(spriteX)) * 4;
        const size_t dstOffset = static_cast<size_t>(y) * static_cast<size_t>(outW) * 4;
        std::copy_n(sourcePixels.data() + srcOffset, static_cast<size_t>(outW) * 4, cropped.data() + dstOffset);
        for (int x = 0; x < outW; ++x) {
            unsigned char* pixel = cropped.data() + dstOffset + static_cast<size_t>(x) * 4;
            pixel[3] = effectivePixelAlpha(pixel, colorKey);
        }
    }
    return cropped;
}
