#include <cmath>
#include <iostream>
#include <optional>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

static std::optional<Vec2> hoveredImagePixel(int imageWidth, int imageHeight, const Vec2& imagePoint)
{
    if (imageWidth <= 0 || imageHeight <= 0 || imagePoint.x < 0.0f || imagePoint.y < 0.0f || imagePoint.x >= static_cast<float>(imageWidth) || imagePoint.y >= static_cast<float>(imageHeight)) {
        return std::nullopt;
    }

    return Vec2{std::floor(imagePoint.x), std::floor(imagePoint.y)};
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

    const std::optional<Vec2> origin = hoveredImagePixel(16, 16, Vec2{0.0f, 0.0f});
    ok &= expect(origin.has_value(), "origin should be inside image");
    ok &= expect(origin.has_value() && origin->x == 0.0f && origin->y == 0.0f, "origin pixel coordinate");

    const std::optional<Vec2> middle = hoveredImagePixel(16, 16, Vec2{7.99f, 8.01f});
    ok &= expect(middle.has_value(), "fractional point should be inside image");
    ok &= expect(middle.has_value() && middle->x == 7.0f && middle->y == 8.0f, "fractional point floors to pixel");

    const std::optional<Vec2> last = hoveredImagePixel(16, 16, Vec2{15.99f, 15.99f});
    ok &= expect(last.has_value(), "last fractional pixel should be inside image");
    ok &= expect(last.has_value() && last->x == 15.0f && last->y == 15.0f, "last pixel coordinate");

    ok &= expect(!hoveredImagePixel(16, 16, Vec2{16.0f, 15.0f}).has_value(), "x at width should be outside image");
    ok &= expect(!hoveredImagePixel(16, 16, Vec2{15.0f, 16.0f}).has_value(), "y at height should be outside image");
    ok &= expect(!hoveredImagePixel(16, 16, Vec2{-0.01f, 0.0f}).has_value(), "negative x should be outside image");
    ok &= expect(!hoveredImagePixel(0, 16, Vec2{0.0f, 0.0f}).has_value(), "invalid width should be outside image");

    return ok ? 0 : 1;
}
