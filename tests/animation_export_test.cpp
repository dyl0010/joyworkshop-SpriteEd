#include "../src/animation_export.h"

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

    ok &= expect(fpsToGifDelayCs(10.0f) == 10, "10 fps should map to 10 centiseconds");
    ok &= expect(fpsToGifDelayCs(8.0f) == 13, "8 fps should round to 13 centiseconds");
    ok &= expect(fpsToGifDelayCs(60.0f) == 2, "60 fps should round to 2 centiseconds");
    ok &= expect(fpsToGifDelayCs(0.0f) == 10, "invalid fps should use default delay");

    std::vector<SpriteRect> sprites(3);
    sprites[0].name = "a";
    sprites[0].w = 2;
    sprites[0].h = 3;
    sprites[1].name = "b";
    sprites[1].w = 4;
    sprites[1].h = 1;
    sprites[2].name = "c";
    sprites[2].w = 1;
    sprites[2].h = 5;

    const std::vector<SpriteRect> frames = collectAnimationSprites(sprites, {1, -1, 2, 9});
    ok &= expect(frames.size() == 2, "collectAnimationSprites should skip invalid frame indices");
    ok &= expect(frames[0].name == "b" && frames[1].name == "c", "collectAnimationSprites should preserve valid frame order");

    int maxW = 0;
    int maxH = 0;
    calculateMaxFrameSize(frames, maxW, maxH);
    ok &= expect(maxW == 4 && maxH == 5, "calculateMaxFrameSize should use max width and height");

    std::vector<unsigned char> imagePixels(4 * 4 * 4, 0);
    setPixel(imagePixels, 4, 1, 1, 255, 0, 0, 255);
    setPixel(imagePixels, 4, 2, 1, 0, 255, 0, 128);
    setPixel(imagePixels, 4, 1, 2, 255, 0, 255, 255);
    setPixel(imagePixels, 4, 2, 2, 0, 0, 255, 0);

    SpriteRect sprite;
    sprite.x = 1;
    sprite.y = 1;
    sprite.w = 2;
    sprite.h = 2;

    AnimationGifSettings settings;
    settings.backgroundR = 10;
    settings.backgroundG = 20;
    settings.backgroundB = 30;
    settings.centerFrames = true;
    ComposedFrame composed = composeSpriteFrame(4, 4, imagePixels, sprite, 4, 4, settings, {});
    ok &= expect(composed.valid(), "composeSpriteFrame should produce a valid frame");
    ok &= expect(composed.width == 4 && composed.height == 4, "composeSpriteFrame should preserve requested canvas size");
    const size_t centeredRedOffset = (static_cast<size_t>(1) * 4 + 1) * 4;
    ok &= expect(composed.pixels[centeredRedOffset + 0] == 255 && composed.pixels[centeredRedOffset + 1] == 0 && composed.pixels[centeredRedOffset + 2] == 0 && composed.pixels[centeredRedOffset + 3] == 255, "opaque source pixel should copy over background");
    const size_t blendedGreenOffset = (static_cast<size_t>(1) * 4 + 2) * 4;
    ok &= expect(composed.pixels[blendedGreenOffset + 0] == 5 && composed.pixels[blendedGreenOffset + 1] == 138 && composed.pixels[blendedGreenOffset + 2] == 15, "semi-transparent source pixel should alpha blend over background");
    const size_t transparentOffset = (static_cast<size_t>(2) * 4 + 2) * 4;
    ok &= expect(composed.pixels[transparentOffset + 0] == 10 && composed.pixels[transparentOffset + 1] == 20 && composed.pixels[transparentOffset + 2] == 30 && composed.pixels[transparentOffset + 3] == 255, "transparent source pixel should leave background color");

    ColorKeySettings colorKey;
    colorKey.enabled = true;
    colorKey.r = 255;
    colorKey.g = 0;
    colorKey.b = 255;
    ComposedFrame colorKeyComposed = composeSpriteFrame(4, 4, imagePixels, sprite, 2, 2, settings, colorKey);
    const size_t colorKeyOffset = (static_cast<size_t>(1) * 2 + 0) * 4;
    ok &= expect(colorKeyComposed.pixels[colorKeyOffset + 0] == 10 && colorKeyComposed.pixels[colorKeyOffset + 1] == 20 && colorKeyComposed.pixels[colorKeyOffset + 2] == 30, "color key source pixel should blend as transparent");

    return ok ? 0 : 1;
}
