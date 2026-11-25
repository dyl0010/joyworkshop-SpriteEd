#pragma once

#include "sprite_types.h"

#include <string>
#include <vector>

struct AnimationGifSettings {
    int backgroundR = 0;
    int backgroundG = 0;
    int backgroundB = 0;
    bool useMaxFrameSize = true;
    bool centerFrames = true;
};

struct ComposedFrame {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;

    [[nodiscard]] bool valid() const
    {
        return width > 0 && height > 0 && !pixels.empty();
    }
};

int fpsToGifDelayCs(float fps);
std::vector<SpriteRect> collectAnimationSprites(const std::vector<SpriteRect>& sprites, const std::vector<int>& frameIndices);
void calculateMaxFrameSize(const std::vector<SpriteRect>& frames, int& outWidth, int& outHeight);
ComposedFrame composeSpriteFrame(int imageWidth, int imageHeight, const std::vector<unsigned char>& imagePixels, const SpriteRect& sprite, int canvasWidth, int canvasHeight, const AnimationGifSettings& settings, const ColorKeySettings& colorKey);
bool exportAnimationGif(const std::string& path, int imageWidth, int imageHeight, const std::vector<unsigned char>& imagePixels, const std::vector<SpriteRect>& sprites, const std::vector<int>& frameIndices, float fps, const AnimationGifSettings& settings, const ColorKeySettings& colorKey);
