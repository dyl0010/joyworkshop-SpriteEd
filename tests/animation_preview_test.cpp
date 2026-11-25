#include <algorithm>
#include <iostream>
#include <utility>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct SpriteRect {
    int w = 0;
    int h = 0;
};

struct CanvasState {
    float zoom = 1.0f;
    Vec2 pan = Vec2{40.0f, 40.0f};
};

static void fitCanvasToRect(CanvasState& canvas, const SpriteRect& sprite, const Vec2& canvasSize)
{
    if (sprite.w <= 0 || sprite.h <= 0 || canvasSize.x <= 1.0f || canvasSize.y <= 1.0f) {
        return;
    }

    const float availableW = std::max(1.0f, canvasSize.x - 40.0f);
    const float availableH = std::max(1.0f, canvasSize.y - 40.0f);
    canvas.zoom = std::max(0.01f, std::min(availableW / static_cast<float>(sprite.w), availableH / static_cast<float>(sprite.h)));
    canvas.pan = Vec2{
        (canvasSize.x - static_cast<float>(sprite.w) * canvas.zoom) * 0.5f,
        (canvasSize.y - static_cast<float>(sprite.h) * canvas.zoom) * 0.5f
    };
}

static std::pair<Vec2, Vec2> zoomedPreviewRect(const CanvasState& canvas, const SpriteRect& sprite, const Vec2& canvasMin)
{
    const Vec2 pos{canvasMin.x + canvas.pan.x, canvasMin.y + canvas.pan.y};
    const Vec2 end{pos.x + static_cast<float>(sprite.w) * canvas.zoom, pos.y + static_cast<float>(sprite.h) * canvas.zoom};
    return {pos, end};
}

static bool nearlyEqual(float a, float b)
{
    return std::abs(a - b) < 0.001f;
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

    CanvasState smallCanvas;
    fitCanvasToRect(smallCanvas, SpriteRect{16, 16}, Vec2{200.0f, 100.0f});
    ok &= expect(nearlyEqual(smallCanvas.zoom, 3.75f), "small sprite fit zoom");
    ok &= expect(nearlyEqual(smallCanvas.pan.x, 70.0f), "small sprite fit pan x");
    ok &= expect(nearlyEqual(smallCanvas.pan.y, 20.0f), "small sprite fit pan y");
    const auto [smallMin, smallMax] = zoomedPreviewRect(smallCanvas, SpriteRect{16, 16}, Vec2{0.0f, 0.0f});
    ok &= expect(nearlyEqual(smallMax.x - smallMin.x, 60.0f), "small sprite fit width");
    ok &= expect(nearlyEqual(smallMax.y - smallMin.y, 60.0f), "small sprite fit height");

    CanvasState wideCanvas;
    fitCanvasToRect(wideCanvas, SpriteRect{128, 32}, Vec2{300.0f, 180.0f});
    ok &= expect(nearlyEqual(wideCanvas.zoom, 2.03125f), "wide sprite fit zoom");
    ok &= expect(nearlyEqual(wideCanvas.pan.x, 20.0f), "wide sprite fit pan x");
    ok &= expect(nearlyEqual(wideCanvas.pan.y, 57.5f), "wide sprite fit pan y");

    CanvasState largeCanvas;
    fitCanvasToRect(largeCanvas, SpriteRect{500, 500}, Vec2{100.0f, 100.0f});
    ok &= expect(nearlyEqual(largeCanvas.zoom, 0.12f), "large sprite should scale down to fit available area");
    ok &= expect(nearlyEqual(largeCanvas.pan.x, 20.0f), "large sprite fit pan x");
    ok &= expect(nearlyEqual(largeCanvas.pan.y, 20.0f), "large sprite fit pan y");

    CanvasState zoomCanvas;
    zoomCanvas.zoom = 2.0f;
    zoomCanvas.pan = Vec2{10.0f, 20.0f};
    const auto [zoomMin, zoomMax] = zoomedPreviewRect(zoomCanvas, SpriteRect{12, 8}, Vec2{100.0f, 200.0f});
    ok &= expect(nearlyEqual(zoomMin.x, 110.0f), "zoomed rect min x uses pan");
    ok &= expect(nearlyEqual(zoomMin.y, 220.0f), "zoomed rect min y uses pan");
    ok &= expect(nearlyEqual(zoomMax.x - zoomMin.x, 24.0f), "zoomed rect width uses current zoom");
    ok &= expect(nearlyEqual(zoomMax.y - zoomMin.y, 16.0f), "zoomed rect height uses current zoom");

    CanvasState invalidCanvas;
    invalidCanvas.zoom = 2.0f;
    invalidCanvas.pan = Vec2{3.0f, 4.0f};
    fitCanvasToRect(invalidCanvas, SpriteRect{0, 16}, Vec2{100.0f, 100.0f});
    ok &= expect(nearlyEqual(invalidCanvas.zoom, 2.0f), "invalid sprite keeps zoom unchanged");
    ok &= expect(nearlyEqual(invalidCanvas.pan.x, 3.0f) && nearlyEqual(invalidCanvas.pan.y, 4.0f), "invalid sprite keeps pan unchanged");

    return ok ? 0 : 1;
}
