#pragma once

#include <string>
#include <vector>

struct SpriteRect {
    std::string name = "sprite";
    int x = 0;
    int y = 0;
    int w = 16;
    int h = 16;
};

struct ColorKeySettings {
    bool enabled = false;
    int r = 255;
    int g = 0;
    int b = 255;
    int tolerance = 0;
};

struct SourceImage {
    std::string path;
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
};

struct PackedSheet {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
    std::vector<SpriteRect> sprites;
    std::vector<SpriteRect> packingRects;

    [[nodiscard]] bool valid() const
    {
        return width > 0 && height > 0 && !pixels.empty();
    }
};
