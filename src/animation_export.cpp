#include "animation_export.h"

#include "image_ops.h"

#define GIF_IMPL
#include <gif.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

unsigned char clampColor(int value)
{
    return static_cast<unsigned char>(std::clamp(value, 0, 255));
}

void fillBackground(std::vector<unsigned char>& pixels, int width, int height, const AnimationGifSettings& settings)
{
    const unsigned char r = clampColor(settings.backgroundR);
    const unsigned char g = clampColor(settings.backgroundG);
    const unsigned char b = clampColor(settings.backgroundB);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
            pixels[offset + 0] = r;
            pixels[offset + 1] = g;
            pixels[offset + 2] = b;
            pixels[offset + 3] = 255;
        }
    }
}

} // namespace

int fpsToGifDelayCs(float fps)
{
    if (fps <= 0.0f || !std::isfinite(fps)) {
        return 10;
    }
    return std::clamp(static_cast<int>(std::lround(100.0f / fps)), 1, 65535);
}

std::vector<SpriteRect> collectAnimationSprites(const std::vector<SpriteRect>& sprites, const std::vector<int>& frameIndices)
{
    std::vector<SpriteRect> frames;
    frames.reserve(frameIndices.size());
    for (int frameIndex : frameIndices) {
        if (frameIndex >= 0 && frameIndex < static_cast<int>(sprites.size())) {
            frames.push_back(sprites[static_cast<size_t>(frameIndex)]);
        }
    }
    return frames;
}

void calculateMaxFrameSize(const std::vector<SpriteRect>& frames, int& outWidth, int& outHeight)
{
    outWidth = 0;
    outHeight = 0;
    for (const SpriteRect& frame : frames) {
        outWidth = std::max(outWidth, frame.w);
        outHeight = std::max(outHeight, frame.h);
    }
}

ComposedFrame composeSpriteFrame(int imageWidth, int imageHeight, const std::vector<unsigned char>& imagePixels, const SpriteRect& sprite, int canvasWidth, int canvasHeight, const AnimationGifSettings& settings, const ColorKeySettings& colorKey)
{
    ComposedFrame frame;
    if (imageWidth <= 0 || imageHeight <= 0 || imagePixels.empty() || sprite.w <= 0 || sprite.h <= 0 || canvasWidth <= 0 || canvasHeight <= 0) {
        return frame;
    }

    frame.width = canvasWidth;
    frame.height = canvasHeight;
    frame.pixels.assign(static_cast<size_t>(canvasWidth) * static_cast<size_t>(canvasHeight) * 4, 0);
    fillBackground(frame.pixels, canvasWidth, canvasHeight, settings);

    int croppedW = 0;
    int croppedH = 0;
    const std::vector<unsigned char> cropped = cropSpritePixels(imageWidth, imageHeight, imagePixels, sprite, croppedW, croppedH, colorKey);
    if (cropped.empty() || croppedW <= 0 || croppedH <= 0) {
        frame.pixels.clear();
        frame.width = 0;
        frame.height = 0;
        return frame;
    }

    const int dstX = settings.centerFrames ? std::max(0, (canvasWidth - croppedW) / 2) : 0;
    const int dstY = settings.centerFrames ? std::max(0, (canvasHeight - croppedH) / 2) : 0;
    const unsigned char bgR = clampColor(settings.backgroundR);
    const unsigned char bgG = clampColor(settings.backgroundG);
    const unsigned char bgB = clampColor(settings.backgroundB);

    for (int y = 0; y < croppedH && y + dstY < canvasHeight; ++y) {
        for (int x = 0; x < croppedW && x + dstX < canvasWidth; ++x) {
            const size_t srcOffset = (static_cast<size_t>(y) * static_cast<size_t>(croppedW) + static_cast<size_t>(x)) * 4;
            const size_t dstOffset = (static_cast<size_t>(y + dstY) * static_cast<size_t>(canvasWidth) + static_cast<size_t>(x + dstX)) * 4;
            const int alpha = cropped[srcOffset + 3];
            frame.pixels[dstOffset + 0] = static_cast<unsigned char>((static_cast<int>(cropped[srcOffset + 0]) * alpha + static_cast<int>(bgR) * (255 - alpha) + 127) / 255);
            frame.pixels[dstOffset + 1] = static_cast<unsigned char>((static_cast<int>(cropped[srcOffset + 1]) * alpha + static_cast<int>(bgG) * (255 - alpha) + 127) / 255);
            frame.pixels[dstOffset + 2] = static_cast<unsigned char>((static_cast<int>(cropped[srcOffset + 2]) * alpha + static_cast<int>(bgB) * (255 - alpha) + 127) / 255);
            frame.pixels[dstOffset + 3] = 255;
        }
    }

    return frame;
}

bool exportAnimationGif(const std::string& path, int imageWidth, int imageHeight, const std::vector<unsigned char>& imagePixels, const std::vector<SpriteRect>& sprites, const std::vector<int>& frameIndices, float fps, const AnimationGifSettings& settings, const ColorKeySettings& colorKey)
{
    const std::vector<SpriteRect> frames = collectAnimationSprites(sprites, frameIndices);
    if (path.empty() || frames.empty()) {
        return false;
    }

    int canvasWidth = 0;
    int canvasHeight = 0;
    if (settings.useMaxFrameSize) {
        calculateMaxFrameSize(frames, canvasWidth, canvasHeight);
    } else {
        canvasWidth = frames.front().w;
        canvasHeight = frames.front().h;
    }
    if (canvasWidth <= 0 || canvasHeight <= 0) {
        return false;
    }

    const int delayCs = fpsToGifDelayCs(fps);
    GifWriter writer{};
    if (!GifBegin(&writer, path.c_str(), static_cast<uint32_t>(canvasWidth), static_cast<uint32_t>(canvasHeight), static_cast<uint32_t>(delayCs))) {
        return false;
    }

    bool wroteAnyFrame = false;
    for (const SpriteRect& sprite : frames) {
        const ComposedFrame frame = composeSpriteFrame(imageWidth, imageHeight, imagePixels, sprite, canvasWidth, canvasHeight, settings, colorKey);
        if (!frame.valid()) {
            continue;
        }
        if (!GifWriteFrame(&writer, frame.pixels.data(), static_cast<uint32_t>(frame.width), static_cast<uint32_t>(frame.height), static_cast<uint32_t>(delayCs))) {
            GifEnd(&writer);
            return false;
        }
        wroteAnyFrame = true;
    }

    GifEnd(&writer);
    return wroteAnyFrame;
}
