#include "../src/image_ops.h"

#include <cstdlib>
#include <iostream>
#include <vector>

struct ImageTexture {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
};

static void setPixel(ImageTexture& texture, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    const size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(texture.width) + static_cast<size_t>(x)) * 4;
    texture.pixels[offset + 0] = r;
    texture.pixels[offset + 1] = g;
    texture.pixels[offset + 2] = b;
    texture.pixels[offset + 3] = a;
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

    ok &= expect(sanitizedSpriteFilename("hero idle/0") == "hero_idle_0", "sanitize spaces and slash");
    ok &= expect(sanitizedSpriteFilename("<>:*?") == "sprite", "sanitize all invalid fallback");
    ok &= expect(sanitizedSpriteFilename("walk-01_A") == "walk-01_A", "preserve safe filename chars");

    const unsigned char magenta[] = {255, 0, 255, 255};
    const unsigned char nearMagenta[] = {250, 3, 251, 255};
    ColorKeySettings colorKey;
    colorKey.enabled = true;
    colorKey.r = 255;
    colorKey.g = 0;
    colorKey.b = 255;
    colorKey.tolerance = 0;
    ok &= expect(pixelMatchesColorKey(magenta, colorKey), "exact color key match");
    ok &= expect(!pixelMatchesColorKey(nearMagenta, colorKey), "near color should not match zero tolerance");
    colorKey.tolerance = 5;
    ok &= expect(pixelMatchesColorKey(nearMagenta, colorKey), "near color should match tolerance");
    ok &= expect(effectivePixelAlpha(magenta, colorKey) == 0, "color key alpha becomes zero");

    ImageTexture texture;
    texture.width = 4;
    texture.height = 3;
    texture.pixels.resize(static_cast<size_t>(texture.width) * static_cast<size_t>(texture.height) * 4, 0);
    for (int y = 0; y < texture.height; ++y) {
        for (int x = 0; x < texture.width; ++x) {
            setPixel(texture, x, y, static_cast<unsigned char>(x), static_cast<unsigned char>(y), static_cast<unsigned char>(x + y), 255);
        }
    }

    ColorKeySettings disabledColorKey;
    int croppedW = 0;
    int croppedH = 0;
    const std::vector<unsigned char> cropped = cropSpritePixels(texture.width, texture.height, texture.pixels, SpriteRect{"piece", 1, 1, 2, 2}, croppedW, croppedH, disabledColorKey);
    ok &= expect(croppedW == 2, "cropped width");
    ok &= expect(croppedH == 2, "cropped height");
    ok &= expect(cropped.size() == 16, "cropped rgba byte count");
    ok &= expect(cropped[0] == 1 && cropped[1] == 1 && cropped[2] == 2 && cropped[3] == 255, "cropped first pixel maps source 1,1");
    ok &= expect(cropped[4] == 2 && cropped[5] == 1 && cropped[6] == 3 && cropped[7] == 255, "cropped second pixel maps source 2,1");
    ok &= expect(cropped[8] == 1 && cropped[9] == 2 && cropped[10] == 3 && cropped[11] == 255, "cropped third pixel maps source 1,2");
    ok &= expect(cropped[12] == 2 && cropped[13] == 2 && cropped[14] == 4 && cropped[15] == 255, "cropped fourth pixel maps source 2,2");

    ColorKeySettings cropColorKey;
    cropColorKey.enabled = true;
    cropColorKey.r = 1;
    cropColorKey.g = 1;
    cropColorKey.b = 2;
    const std::vector<unsigned char> keyedCrop = cropSpritePixels(texture.width, texture.height, texture.pixels, SpriteRect{"piece", 1, 1, 2, 2}, croppedW, croppedH, cropColorKey);
    ok &= expect(keyedCrop[3] == 0, "color-keyed crop first pixel alpha zero");
    ok &= expect(keyedCrop[7] == 255, "non-keyed crop second pixel alpha preserved");

    const std::vector<unsigned char> clamped = cropSpritePixels(texture.width, texture.height, texture.pixels, SpriteRect{"edge", 3, 2, 4, 4}, croppedW, croppedH, disabledColorKey);
    ok &= expect(croppedW == 1, "edge crop clamps width");
    ok &= expect(croppedH == 1, "edge crop clamps height");
    ok &= expect(clamped.size() == 4, "edge crop byte count");
    ok &= expect(clamped[0] == 3 && clamped[1] == 2 && clamped[2] == 5 && clamped[3] == 255, "edge crop maps bottom-right pixel");

    const std::vector<unsigned char> invalid = cropSpritePixels(0, 0, {}, SpriteRect{}, croppedW, croppedH, disabledColorKey);
    ok &= expect(invalid.empty(), "invalid texture crop returns empty");
    ok &= expect(croppedW == 0 && croppedH == 0, "invalid texture crop dimensions reset");

    SpriteRect visibleRect{"visible", 0, 0, 2, 2};
    ok &= expect(spriteRectHasVisiblePixel(texture.width, texture.height, texture.pixels, visibleRect, 0, disabledColorKey), "visible rect has alpha pixels");
    ColorKeySettings hideFirstPixel;
    hideFirstPixel.enabled = true;
    hideFirstPixel.r = 0;
    hideFirstPixel.g = 0;
    hideFirstPixel.b = 0;
    ok &= expect(spriteRectHasVisiblePixel(texture.width, texture.height, texture.pixels, visibleRect, 0, hideFirstPixel), "rect still has non-keyed visible pixels");

    return ok ? 0 : 1;
}
