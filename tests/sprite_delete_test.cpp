#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <vector>

struct SpriteRect {
    std::string name;
};

struct AnimationState {
    std::vector<int> frames;
    int currentFrame = 0;
};

struct AppState {
    std::vector<SpriteRect> sprites;
    AnimationState animation;
    int selectedSprite = -1;
    std::array<char, 256> renameBuffer{};
};

template <size_t N>
static void setBuffer(std::array<char, N>& buffer, const std::string& value)
{
    std::fill(buffer.begin(), buffer.end(), '\0');
    const size_t count = std::min(value.size(), N - 1);
    std::copy_n(value.data(), count, buffer.data());
}

static bool deleteSelectedSprite(AppState& app)
{
    if (app.selectedSprite < 0 || app.selectedSprite >= static_cast<int>(app.sprites.size())) {
        return false;
    }

    const int removed = app.selectedSprite;
    app.sprites.erase(app.sprites.begin() + removed);
    app.animation.frames.erase(std::remove(app.animation.frames.begin(), app.animation.frames.end(), removed), app.animation.frames.end());
    for (int& frame : app.animation.frames) {
        if (frame > removed) {
            --frame;
        }
    }
    app.selectedSprite = app.sprites.empty() ? -1 : std::clamp(removed, 0, static_cast<int>(app.sprites.size()) - 1);
    if (app.selectedSprite >= 0) {
        setBuffer(app.renameBuffer, app.sprites[static_cast<size_t>(app.selectedSprite)].name);
    } else {
        setBuffer(app.renameBuffer, "");
    }
    app.animation.currentFrame = std::clamp(app.animation.currentFrame, 0, std::max(0, static_cast<int>(app.animation.frames.size()) - 1));
    return true;
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

    AppState app;
    app.sprites = {SpriteRect{"idle"}, SpriteRect{"run"}, SpriteRect{"jump"}, SpriteRect{"fall"}};
    app.animation.frames = {0, 1, 2, 3, 1};
    app.animation.currentFrame = 4;
    app.selectedSprite = 1;
    setBuffer(app.renameBuffer, "run");

    ok &= expect(deleteSelectedSprite(app), "selected sprite should be deleted");
    ok &= expect(app.sprites.size() == 3, "sprite count after delete");
    ok &= expect(app.sprites[0].name == "idle", "first sprite kept");
    ok &= expect(app.sprites[1].name == "jump", "next sprite selected after delete");
    ok &= expect(app.selectedSprite == 1, "selection clamps to replacement index");
    ok &= expect(std::string(app.renameBuffer.data()) == "jump", "rename buffer syncs to selected sprite");
    ok &= expect((app.animation.frames == std::vector<int>{0, 1, 2}), "animation frames remove deleted index and remap later indices");
    ok &= expect(app.animation.currentFrame == 2, "animation current frame clamps to valid slot");

    app.selectedSprite = 2;
    ok &= expect(deleteSelectedSprite(app), "last sprite should be deleted");
    ok &= expect(app.selectedSprite == 1, "selection clamps to previous sprite when deleting last");
    ok &= expect(std::string(app.renameBuffer.data()) == "jump", "rename buffer remains in sync after deleting last");

    app.selectedSprite = 0;
    ok &= expect(deleteSelectedSprite(app), "first remaining sprite should be deleted");
    app.selectedSprite = 0;
    ok &= expect(deleteSelectedSprite(app), "final sprite should be deleted");
    ok &= expect(app.selectedSprite == -1, "selection clears when no sprites remain");
    ok &= expect(std::string(app.renameBuffer.data()).empty(), "rename buffer clears when no sprites remain");
    ok &= expect(!deleteSelectedSprite(app), "delete should fail with no selected sprite");

    return ok ? 0 : 1;
}
