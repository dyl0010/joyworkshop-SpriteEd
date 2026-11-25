#include <cmath>
#include <iostream>
#include <string>
#include <vector>

struct ColorKeySettings {
    bool enabled = false;
    int r = 255;
    int g = 0;
    int b = 255;
    int tolerance = 0;
};

enum class ChannelView {
    Rgba,
    Red,
    Green,
    Blue,
    Alpha
};

struct CanvasState {
    float zoom = 1.0f;
    ChannelView channelView = ChannelView::Rgba;
};

struct ImageTexture {
    int width = 0;
    int height = 0;
    std::string path;

    [[nodiscard]] bool valid() const
    {
        return width > 0 && height > 0;
    }
};

struct AppState {
    ImageTexture texture;
    CanvasState canvas;
    ColorKeySettings colorKey;
    std::vector<int> sprites;
    int hoveredPixelX = -1;
    int hoveredPixelY = -1;
};

static const char* channelViewLabel(ChannelView view)
{
    switch (view) {
    case ChannelView::Rgba:
        return "RGBA";
    case ChannelView::Red:
        return "R";
    case ChannelView::Green:
        return "G";
    case ChannelView::Blue:
        return "B";
    case ChannelView::Alpha:
        return "A";
    }
    return "RGBA";
}

static std::string filenameOnly(const std::string& path)
{
    const size_t slash = path.find_last_of("/");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static std::string currentPngFilename(const AppState& app)
{
    if (!app.texture.valid() || app.texture.path.empty()) {
        return "none";
    }

    const std::string filename = filenameOnly(app.texture.path);
    return filename.empty() ? "none" : filename;
}

static std::vector<std::string> buildCanvasInfoLines(const AppState& app)
{
    std::vector<std::string> lines;
    lines.push_back("PNG: " + currentPngFilename(app));
    if (app.texture.valid()) {
        lines.push_back("Image: " + std::to_string(app.texture.width) + " x " + std::to_string(app.texture.height));
    } else {
        lines.push_back("Image: none");
    }
    lines.push_back("Sprites: " + std::to_string(app.sprites.size()));
    lines.push_back("Zoom: " + std::to_string(static_cast<int>(std::round(app.canvas.zoom * 100.0f))) + "%");
    lines.push_back(std::string("View: ") + channelViewLabel(app.canvas.channelView));
    lines.push_back(std::string("Color Key: ") + (app.colorKey.enabled ? (std::to_string(app.colorKey.r) + "," + std::to_string(app.colorKey.g) + "," + std::to_string(app.colorKey.b) + " tol " + std::to_string(app.colorKey.tolerance)) : "off"));
    if (app.hoveredPixelX >= 0 && app.hoveredPixelY >= 0) {
        lines.push_back("Hover: " + std::to_string(app.hoveredPixelX) + ", " + std::to_string(app.hoveredPixelY));
    } else {
        lines.push_back("Hover: outside image");
    }
    return lines;
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

    AppState empty;
    const std::vector<std::string> emptyLines = buildCanvasInfoLines(empty);
    ok &= expect(emptyLines.size() == 7, "empty overlay line count");
    ok &= expect(emptyLines[0] == "PNG: none", "empty png line");
    ok &= expect(emptyLines[1] == "Image: none", "empty image line");
    ok &= expect(emptyLines[5] == "Color Key: off", "empty color key line");
    ok &= expect(emptyLines[6] == "Hover: outside image", "empty hover line");

    AppState app;
    app.texture.width = 128;
    app.texture.height = 64;
    app.texture.path = "C:/art/hero.png";
    app.canvas.zoom = 2.25f;
    app.canvas.channelView = ChannelView::Alpha;
    app.colorKey.enabled = true;
    app.colorKey.r = 255;
    app.colorKey.g = 0;
    app.colorKey.b = 255;
    app.colorKey.tolerance = 4;
    app.sprites = {1, 2, 3};
    app.hoveredPixelX = 12;
    app.hoveredPixelY = 34;

    const std::vector<std::string> lines = buildCanvasInfoLines(app);
    ok &= expect(lines[0] == "PNG: hero.png", "png filename line");
    ok &= expect(lines[1] == "Image: 128 x 64", "image dimensions line");
    ok &= expect(lines[2] == "Sprites: 3", "sprites count line");
    ok &= expect(lines[3] == "Zoom: 225%", "zoom line");
    ok &= expect(lines[4] == "View: A", "view line");
    ok &= expect(lines[5] == "Color Key: 255,0,255 tol 4", "color key line");
    ok &= expect(lines[6] == "Hover: 12, 34", "hover line");

    return ok ? 0 : 1;
}
