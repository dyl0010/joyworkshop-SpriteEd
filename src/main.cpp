#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <nlohmann/json.hpp>

#include "animation_export.h"
#include "image_ops.h"
#include "sprite_layout.h"
#include "sprite_sheet_builder.h"
#include "sprite_sheet_extractor.h"
#include "sprite_types.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

struct AnimationState {
    std::string name = "anim_0";
    std::vector<int> frames;
    float fps = 8.0f;
    bool playing = false;
    bool loop = true;
    bool previewOnCanvas = false;
    bool previewOnCanvasLastFrame = false;
    int currentFrame = 0;
    double accumulator = 0.0;
};

enum class ChannelView {
    Rgba,
    Red,
    Green,
    Blue,
    Alpha
};

struct ImageTexture {
    GLuint id = 0;
    GLuint channelPreviewId = 0;
    ChannelView channelPreview = ChannelView::Rgba;
    ColorKeySettings channelPreviewColorKey;
    int width = 0;
    int height = 0;
    std::string path;
    std::vector<unsigned char> pixels;

    void release()
    {
        if (id != 0) {
            glDeleteTextures(1, &id);
            id = 0;
        }
        if (channelPreviewId != 0) {
            glDeleteTextures(1, &channelPreviewId);
            channelPreviewId = 0;
        }
        channelPreview = ChannelView::Rgba;
        channelPreviewColorKey = ColorKeySettings{};
        width = 0;
        height = 0;
        path.clear();
        pixels.clear();
    }

    [[nodiscard]] bool valid() const
    {
        return id != 0 && width > 0 && height > 0;
    }
};

struct SheetBuilderState {
    std::vector<SourceImage> images;
    int selectedIndex = -1;
    PackingAlgorithm algorithm = PackingAlgorithm::FixedColumns;
    int columns = 8;
    int padding = 2;
    int margin = 0;
    bool useCustomCellSize = false;
    int customCellWidth = 32;
    int customCellHeight = 32;
    int extrudeBorder = 0;
    int maxSheetWidth = 1024;
    PackingSortMode sortMode = PackingSortMode::Original;
    std::array<char, 512> exportPath{};
};

struct SpriteLayoutState {
    SpriteLayout layout;
    SpriteLayoutSettings settings;
    SpriteLayoutValidation dragValidation;
    int selectedIndex = -1;
    int dragIndex = -1;
    int dragStartX = 0;
    int dragStartY = 0;
    int dragCandidateX = 0;
    int dragCandidateY = 0;
    ImVec2 dragStartMouse = ImVec2(0.0f, 0.0f);
    float zoom = 1.0f;
    ImVec2 pan = ImVec2(16.0f, 16.0f);
};

struct CanvasState {
    float zoom = 1.0f;
    ImVec2 pan = ImVec2(40.0f, 40.0f);
    bool showChecker = true;
    bool pixelPerfect = true;
    bool integerZoom = true;
    bool showInfoOverlay = true;
    bool showPackingRects = true;
    ChannelView channelView = ChannelView::Rgba;
    ImVec2 framebufferScale = ImVec2(1.0f, 1.0f);
    int zoomLevelIndex = 3;
    bool draggingRect = false;
    ImVec2 dragStartImage = ImVec2(0.0f, 0.0f);
    ImVec2 dragEndImage = ImVec2(0.0f, 0.0f);
};

enum class SpriteExtractionMethod {
    Grid,
    TransparencyRegions,
    AnimationStrip,
    MetadataImport
};

struct AppState {
    ImageTexture texture;
    CanvasState canvas;
    SpriteExtractionMethod extractionMethod = SpriteExtractionMethod::Grid;
    GridExtractorSettings grid;
    TransparencyRegionSettings transparencyRegions;
    AnimationStripSettings animationStrip;
    MetadataImportSettings metadataImport;
    std::vector<SpriteRect> metadataSprites;
    std::vector<std::string> metadataWarnings;
    std::string metadataDetectedFormat;
    SpritePostProcessSettings extractorPostProcess;
    bool gridLivePreview = true;
    AnimationState animation;
    AnimationGifSettings animationGif;
    ColorKeySettings colorKey;
    SheetBuilderState sheetBuilder;
    SpriteLayoutState spriteLayout;

    std::vector<SpriteRect> sprites;
    std::vector<SpriteRect> gridPreview;
    std::vector<SpriteRect> packingPreviewRects;
    int selectedSprite = -1;
    int hoveredPixelX = -1;
    int hoveredPixelY = -1;

    std::array<char, 512> imagePath{};
    std::array<char, 512> projectPath{};
    std::array<char, 512> exportPath{};
    std::array<char, 512> spriteExportDir{};
    std::array<char, 512> metadataPath{};
    std::array<char, 512> animationGifPath{};
    std::array<char, 256> extractorNamePattern{"sprite_{index}"};
    std::array<char, 256> renameBuffer{};
    std::array<char, 128> animationNameBuffer{};
    std::vector<std::string> logs;

    bool showProject = false;
    bool showExtractor = false;
    bool showSprites = false;
    bool showInspector = false;
    bool showAnimation = false;
    bool showExport = false;
    bool showSheetBuilder = false;
    bool showSpriteLayout = false;
    bool showLog = false;
    bool showDemo = false;
};

template <size_t N>
static void setBuffer(std::array<char, N>& buffer, const std::string& value)
{
    std::fill(buffer.begin(), buffer.end(), '\0');
    const size_t count = std::min(value.size(), N - 1);
    std::copy_n(value.data(), count, buffer.data());
}

static void appendLog(AppState& app, const std::string& message)
{
    app.logs.push_back(message);
    if (app.logs.size() > 300) {
        app.logs.erase(app.logs.begin());
    }
}

#ifdef _WIN32
static std::wstring utf8ToWide(const std::string& value)
{
    if (value.empty()) {
        return {};
    }

    const int wideSize = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (wideSize <= 0) {
        return {};
    }

    std::wstring result(static_cast<size_t>(wideSize), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), wideSize);
    return result;
}

static std::string wideToUtf8(const wchar_t* value)
{
    if (value == nullptr || value[0] == L'\0') {
        return {};
    }

    const int byteSize = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (byteSize <= 1) {
        return {};
    }

    std::string result(static_cast<size_t>(byteSize), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), byteSize, nullptr, nullptr);
    if (!result.empty() && result.back() == '\0') {
        result.pop_back();
    }
    return result;
}
#endif

static bool nativeFileDialogsAvailable()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

static std::optional<std::string> openNativeFileDialog(AppState& app, const wchar_t* title, const wchar_t* filter, const std::string& initialPath)
{
#ifdef _WIN32
    std::array<wchar_t, 1024> fileName{};
    const std::wstring initial = utf8ToWide(initialPath);
    if (!initial.empty()) {
        const size_t count = std::min(initial.size(), fileName.size() - 1);
        std::copy_n(initial.data(), count, fileName.data());
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = fileName.data();
    ofn.nMaxFile = static_cast<DWORD>(fileName.size());
    ofn.lpstrTitle = title;
    ofn.lpstrFilter = filter;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn) != FALSE) {
        return wideToUtf8(fileName.data());
    }

    const DWORD error = CommDlgExtendedError();
    if (error != 0) {
        appendLog(app, "File dialog failed: " + std::to_string(error));
    }
    return std::nullopt;
#else
    (void)app;
    (void)title;
    (void)filter;
    (void)initialPath;
    return std::nullopt;
#endif
}

static std::vector<std::string> openNativePngFilesDialog(AppState& app, const std::string& initialPath)
{
#ifdef _WIN32
    std::vector<wchar_t> fileNames(32768, L'\0');
    const std::wstring initial = utf8ToWide(initialPath);
    if (!initial.empty()) {
        const size_t count = std::min(initial.size(), fileNames.size() - 1);
        std::copy_n(initial.data(), count, fileNames.data());
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = fileNames.data();
    ofn.nMaxFile = static_cast<DWORD>(fileNames.size());
    ofn.lpstrTitle = L"Add PNG Images";
    ofn.lpstrFilter = L"PNG Images (*.png)\0*.png\0All Files (*.*)\0*.*\0";
    ofn.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    std::vector<std::string> paths;
    if (GetOpenFileNameW(&ofn) == FALSE) {
        const DWORD error = CommDlgExtendedError();
        if (error != 0) {
            appendLog(app, "Multi-file dialog failed: " + std::to_string(error));
        }
        return paths;
    }

    const wchar_t* cursor = fileNames.data();
    const std::wstring first(cursor);
    cursor += first.size() + 1;
    if (*cursor == L'\0') {
        paths.push_back(wideToUtf8(first.c_str()));
        return paths;
    }

    const fs::path directory = wideToUtf8(first.c_str());
    while (*cursor != L'\0') {
        const std::wstring filename(cursor);
        const fs::path path = directory / wideToUtf8(filename.c_str());
        paths.push_back(path.string());
        cursor += filename.size() + 1;
    }
    return paths;
#else
    (void)app;
    (void)initialPath;
    return {};
#endif
}

static void setPixelTextureFiltering(GLuint textureId)
{
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

struct RawImageDrawCommand {
    GLuint textureId = 0;
    ImVec2 p0 = ImVec2(0.0f, 0.0f);
    ImVec2 p1 = ImVec2(0.0f, 0.0f);
    ImVec2 uv0 = ImVec2(0.0f, 0.0f);
    ImVec2 uv1 = ImVec2(1.0f, 1.0f);
    ImVec4 clip = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
};

static std::vector<RawImageDrawCommand> g_rawImageDrawCommands;
static ImVec2 g_rawFramebufferScale = ImVec2(1.0f, 1.0f);

static void rawOpenGLImageCallback(const ImDrawList*, const ImDrawCmd* cmd)
{
    const auto index = static_cast<size_t>(reinterpret_cast<intptr_t>(cmd->UserCallbackData));
    if (index >= g_rawImageDrawCommands.size()) {
        return;
    }

    const RawImageDrawCommand& data = g_rawImageDrawCommands[index];
    if (data.textureId == 0) {
        return;
    }

    using GlUseProgramFn = void(APIENTRY*)(GLuint);
    using GlBindVertexArrayFn = void(APIENTRY*)(GLuint);
    using GlActiveTextureFn = void(APIENTRY*)(GLenum);
    using GlBindSamplerFn = void(APIENTRY*)(GLuint, GLuint);

    auto glUseProgramPtr = reinterpret_cast<GlUseProgramFn>(glfwGetProcAddress("glUseProgram"));
    auto glBindVertexArrayPtr = reinterpret_cast<GlBindVertexArrayFn>(glfwGetProcAddress("glBindVertexArray"));
    auto glActiveTexturePtr = reinterpret_cast<GlActiveTextureFn>(glfwGetProcAddress("glActiveTexture"));
    auto glBindSamplerPtr = reinterpret_cast<GlBindSamplerFn>(glfwGetProcAddress("glBindSampler"));

    if (glUseProgramPtr != nullptr) {
        glUseProgramPtr(0);
    }
    if (glBindVertexArrayPtr != nullptr) {
        glBindVertexArrayPtr(0);
    }
    if (glActiveTexturePtr != nullptr) {
        glActiveTexturePtr(0x84C0);
    }
    if (glBindSamplerPtr != nullptr) {
        glBindSamplerPtr(0, 0);
    }

    const float scaleX = g_rawFramebufferScale.x;
    const float scaleY = g_rawFramebufferScale.y;

    const ImGuiIO& io = ImGui::GetIO();
    const int framebufferW = static_cast<int>(io.DisplaySize.x * scaleX);
    const int framebufferH = static_cast<int>(io.DisplaySize.y * scaleY);
    glViewport(0, 0, framebufferW, framebufferH);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(framebufferW), static_cast<double>(framebufferH), 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const bool scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
    if (data.clip.z > data.clip.x && data.clip.w > data.clip.y) {
        const int clipX = static_cast<int>(data.clip.x * scaleX);
        const int clipY = static_cast<int>((io.DisplaySize.y - data.clip.w) * scaleY);
        const int clipW = static_cast<int>((data.clip.z - data.clip.x) * scaleX);
        const int clipH = static_cast<int>((data.clip.w - data.clip.y) * scaleY);
        glEnable(GL_SCISSOR_TEST);
        glScissor(clipX, clipY, clipW, clipH);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, data.textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    const float x0 = data.p0.x * scaleX;
    const float y0 = data.p0.y * scaleY;
    const float x1 = data.p1.x * scaleX;
    const float y1 = data.p1.y * scaleY;

    glBegin(GL_QUADS);
    glTexCoord2f(data.uv0.x, data.uv0.y);
    glVertex2f(x0, y0);
    glTexCoord2f(data.uv1.x, data.uv0.y);
    glVertex2f(x1, y0);
    glTexCoord2f(data.uv1.x, data.uv1.y);
    glVertex2f(x1, y1);
    glTexCoord2f(data.uv0.x, data.uv1.y);
    glVertex2f(x0, y1);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    if (!scissorWasEnabled) {
        glDisable(GL_SCISSOR_TEST);
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

static void addRawOpenGLImage(ImDrawList* drawList, GLuint textureId, ImVec2 p0, ImVec2 p1, ImVec2 uv0 = ImVec2(0.0f, 0.0f), ImVec2 uv1 = ImVec2(1.0f, 1.0f))
{
    const ImVec4 clip = drawList->_ClipRectStack.empty()
        ? ImVec4(0.0f, 0.0f, 0.0f, 0.0f)
        : drawList->_ClipRectStack.back();
    const size_t index = g_rawImageDrawCommands.size();
    g_rawImageDrawCommands.push_back(RawImageDrawCommand{textureId, p0, p1, uv0, uv1, clip});
    drawList->AddCallback(rawOpenGLImageCallback, reinterpret_cast<void*>(static_cast<intptr_t>(index)));
    drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

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

static int channelViewComponentIndex(ChannelView view)
{
    switch (view) {
    case ChannelView::Red:
        return 0;
    case ChannelView::Green:
        return 1;
    case ChannelView::Blue:
        return 2;
    case ChannelView::Alpha:
        return 3;
    case ChannelView::Rgba:
        break;
    }
    return 0;
}

static bool colorKeySettingsEqual(const ColorKeySettings& a, const ColorKeySettings& b)
{
    return a.enabled == b.enabled && a.r == b.r && a.g == b.g && a.b == b.b && a.tolerance == b.tolerance;
}

static void invalidateChannelPreview(ImageTexture& texture)
{
    if (texture.channelPreviewId != 0) {
        glDeleteTextures(1, &texture.channelPreviewId);
        texture.channelPreviewId = 0;
    }
    texture.channelPreview = ChannelView::Rgba;
    texture.channelPreviewColorKey = ColorKeySettings{};
}

static GLuint channelPreviewTexture(AppState& app)
{
    if (!app.texture.valid() || app.texture.pixels.empty()) {
        return app.texture.id;
    }

    const bool needsProcessedTexture = app.canvas.channelView != ChannelView::Rgba || app.colorKey.enabled;
    if (!needsProcessedTexture) {
        return app.texture.id;
    }

    if (app.texture.channelPreviewId != 0 && app.texture.channelPreview == app.canvas.channelView && colorKeySettingsEqual(app.texture.channelPreviewColorKey, app.colorKey)) {
        return app.texture.channelPreviewId;
    }

    invalidateChannelPreview(app.texture);

    std::vector<unsigned char> preview(app.texture.pixels.size(), 255);
    if (app.canvas.channelView == ChannelView::Rgba) {
        preview = app.texture.pixels;
        for (size_t i = 0; i + 3 < preview.size(); i += 4) {
            preview[i + 3] = effectivePixelAlpha(preview.data() + i, app.colorKey);
        }
    } else {
        const int component = channelViewComponentIndex(app.canvas.channelView);
        for (size_t i = 0; i + 3 < app.texture.pixels.size(); i += 4) {
            const unsigned char value = app.texture.pixels[i + static_cast<size_t>(component)];
            preview[i + 0] = value;
            preview[i + 1] = value;
            preview[i + 2] = value;
            preview[i + 3] = effectivePixelAlpha(app.texture.pixels.data() + i, app.colorKey) == 0 ? 96 : 255;
        }
    }

    glGenTextures(1, &app.texture.channelPreviewId);
    glBindTexture(GL_TEXTURE_2D, app.texture.channelPreviewId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, app.texture.width, app.texture.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, preview.data());
    glBindTexture(GL_TEXTURE_2D, 0);
    app.texture.channelPreview = app.canvas.channelView;
    app.texture.channelPreviewColorKey = app.colorKey;
    return app.texture.channelPreviewId;
}

static void drawChannelButton(AppState& app, ChannelView view, const ImVec4& color)
{
    ImGui::PushID(channelViewLabel(view));
    const bool active = app.canvas.channelView == view;
    ImGui::PushStyleColor(ImGuiCol_Button, active ? color : ImVec4(color.x * 0.45f, color.y * 0.45f, color.z * 0.45f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(std::min(color.x + 0.2f, 1.0f), std::min(color.y + 0.2f, 1.0f), std::min(color.z + 0.2f, 1.0f), 1.0f));
    if (ImGui::Button(channelViewLabel(view), ImVec2(24.0f, 22.0f))) {
        app.canvas.channelView = active ? ChannelView::Rgba : view;
    }
    ImGui::PopStyleColor(3);
    ImGui::PopID();
}

static bool uploadTextureFromPixels(ImageTexture& texture, const std::vector<unsigned char>& pixels, int width, int height, const std::string& path)
{
    if (width <= 0 || height <= 0 || pixels.size() != static_cast<size_t>(width) * static_cast<size_t>(height) * 4) {
        return false;
    }

    GLuint textureId = 0;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    texture.release();
    texture.id = textureId;
    texture.width = width;
    texture.height = height;
    texture.path = path;
    texture.pixels = pixels;
    return true;
}

static void resetEditedSheetState(AppState& app)
{
    app.canvas.channelView = ChannelView::Rgba;
    app.gridPreview.clear();
    app.packingPreviewRects.clear();
    app.animation.frames.clear();
    app.animation.currentFrame = 0;
    app.animation.accumulator = 0.0;
    app.animation.playing = false;
    app.animation.previewOnCanvasLastFrame = false;
    app.selectedSprite = app.sprites.empty() ? -1 : 0;
    if (app.selectedSprite >= 0) {
        setBuffer(app.renameBuffer, app.sprites[static_cast<size_t>(app.selectedSprite)].name);
    } else {
        setBuffer(app.renameBuffer, "");
    }
    app.hoveredPixelX = -1;
    app.hoveredPixelY = -1;
}

static std::optional<SourceImage> loadSourceImage(const std::string& path, std::string& error)
{
    int width = 0;
    int height = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, nullptr, 4);
    if (pixels == nullptr) {
        error = stbi_failure_reason() != nullptr ? stbi_failure_reason() : "unknown PNG load failure";
        return std::nullopt;
    }

    SourceImage image;
    image.path = path;
    image.name = fs::path(path).stem().string();
    if (image.name.empty()) {
        image.name = "sprite";
    }
    image.width = width;
    image.height = height;
    image.pixels.assign(pixels, pixels + static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    stbi_image_free(pixels);
    return image;
}

static SheetPackingSettings sheetPackingSettingsFromBuilder(const SheetBuilderState& builder)
{
    SheetPackingSettings settings;
    settings.algorithm = builder.algorithm;
    settings.columns = builder.columns;
    settings.padding = builder.padding;
    settings.margin = builder.margin;
    settings.useCustomCellSize = builder.useCustomCellSize;
    settings.customCellWidth = builder.customCellWidth;
    settings.customCellHeight = builder.customCellHeight;
    settings.extrudeBorder = builder.extrudeBorder;
    settings.maxSheetWidth = builder.maxSheetWidth;
    settings.sortMode = builder.sortMode;
    return settings;
}

static PackedSheet buildSpriteSheet(const SheetBuilderState& builder)
{
    return buildSpriteSheet(builder.images, sheetPackingSettingsFromBuilder(builder));
}

static bool loadPngTexture(AppState& app, const std::string& path)
{
    std::string error;
    std::optional<SourceImage> image = loadSourceImage(path, error);
    if (!image.has_value()) {
        appendLog(app, "PNG load failed: " + error);
        return false;
    }

    app.sprites.clear();
    if (!uploadTextureFromPixels(app.texture, image->pixels, image->width, image->height, path)) {
        appendLog(app, "PNG load failed: invalid RGBA pixel data.");
        return false;
    }
    resetEditedSheetState(app);
    setBuffer(app.imagePath, path);

    appendLog(app, "Loaded PNG: " + path + " (" + std::to_string(image->width) + "x" + std::to_string(image->height) + ")");
    return true;
}

static void browseAndLoadPng(AppState& app)
{
    if (!nativeFileDialogsAvailable()) {
        appendLog(app, "Native file dialogs are currently only available on Windows.");
        return;
    }

    const auto path = openNativeFileDialog(app, L"Open PNG", L"PNG Images (*.png)\0*.png\0All Files (*.*)\0*.*\0", app.imagePath.data());
    if (path.has_value()) {
        loadPngTexture(app, *path);
    }
}

static bool openPngShortcutPressed(const ImGuiIO& io)
{
    return io.KeyCtrl && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_O, false);
}

static std::string currentPngFilename(const AppState& app)
{
    if (!app.texture.valid() || app.texture.path.empty()) {
        return "none";
    }

    const std::string filename = fs::path(app.texture.path).filename().string();
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

static void drawCanvasInfoOverlay(ImDrawList* drawList, const AppState& app, const ImVec2& canvasMin)
{
    if (!app.canvas.showInfoOverlay) {
        return;
    }

    const std::vector<std::string> lines = buildCanvasInfoLines(app);
    const float lineHeight = ImGui::GetTextLineHeight();
    ImVec2 textPos(canvasMin.x + 16.0f, canvasMin.y + 16.0f);
    for (const std::string& line : lines) {
        drawList->AddText(textPos, IM_COL32(225, 235, 245, 235), line.c_str());
        textPos.y += lineHeight;
    }
}

static json spriteToJson(const SpriteRect& sprite)
{
    return json{{"name", sprite.name}, {"x", sprite.x}, {"y", sprite.y}, {"w", sprite.w}, {"h", sprite.h}};
}

static SpriteRect spriteFromJson(const json& value)
{
    SpriteRect sprite;
    sprite.name = value.value("name", "sprite");
    sprite.x = value.value("x", 0);
    sprite.y = value.value("y", 0);
    sprite.w = value.value("w", 16);
    sprite.h = value.value("h", 16);
    return sprite;
}

static bool saveProject(AppState& app, const std::string& path)
{
    json root;
    root["format"] = "SpriteEd Project";
    root["version"] = 1;
    root["image"] = app.texture.path;
    root["sprites"] = json::array();
    for (const SpriteRect& sprite : app.sprites) {
        root["sprites"].push_back(spriteToJson(sprite));
    }
    root["animation"] = {
        {"name", app.animation.name},
        {"fps", app.animation.fps},
        {"loop", app.animation.loop},
        {"frames", app.animation.frames}
    };

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        appendLog(app, "Project save failed: cannot write " + path);
        return false;
    }
    file << root.dump(4);
    setBuffer(app.projectPath, path);
    appendLog(app, "Saved project: " + path);
    return true;
}

static bool loadProject(AppState& app, const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        appendLog(app, "Project load failed: cannot read " + path);
        return false;
    }

    json root;
    try {
        file >> root;
    } catch (const std::exception& e) {
        appendLog(app, std::string("Project JSON parse failed: ") + e.what());
        return false;
    }

    const std::string imagePath = root.value("image", "");
    if (!imagePath.empty() && !loadPngTexture(app, imagePath)) {
        appendLog(app, "Project was loaded, but its image could not be loaded. Sprite data was restored.");
    }

    app.sprites.clear();
    for (const json& item : root.value("sprites", json::array())) {
        app.sprites.push_back(spriteFromJson(item));
    }

    const json anim = root.value("animation", json::object());
    app.animation.name = anim.value("name", "anim_0");
    app.animation.fps = anim.value("fps", 8.0f);
    app.animation.loop = anim.value("loop", true);
    app.animation.frames = anim.value("frames", std::vector<int>{});
    app.animation.currentFrame = 0;
    app.animation.accumulator = 0.0;
    app.selectedSprite = app.sprites.empty() ? -1 : 0;
    if (app.selectedSprite >= 0) {
        setBuffer(app.renameBuffer, app.sprites[static_cast<size_t>(app.selectedSprite)].name);
    }
    setBuffer(app.animationNameBuffer, app.animation.name);

    setBuffer(app.projectPath, path);
    appendLog(app, "Loaded project: " + path);
    return true;
}

static void browseAndLoadProject(AppState& app)
{
    if (!nativeFileDialogsAvailable()) {
        appendLog(app, "Native file dialogs are currently only available on Windows.");
        return;
    }

    const auto path = openNativeFileDialog(app, L"Open SpriteEd Project", L"SpriteEd Projects (*.sprited.json)\0*.sprited.json\0JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0", app.projectPath.data());
    if (path.has_value()) {
        loadProject(app, *path);
    }
}

static bool loadMetadataPreview(AppState& app, const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        appendLog(app, "Metadata import failed: cannot open " + path);
        return false;
    }

    const std::string jsonText((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    MetadataImportResult result = importMetadataSpritesFromJson(jsonText, app.metadataImport);
    app.metadataSprites = std::move(result.sprites);
    app.metadataWarnings = std::move(result.warnings);
    app.metadataDetectedFormat = std::move(result.detectedFormat);
    setBuffer(app.metadataPath, path);

    for (const std::string& warning : app.metadataWarnings) {
        appendLog(app, "Metadata import: " + warning);
    }
    appendLog(app, "Loaded metadata preview: " + std::to_string(app.metadataSprites.size()) + " sprites from " + path);
    return !app.metadataSprites.empty();
}

static void browseAndLoadMetadata(AppState& app)
{
    if (!nativeFileDialogsAvailable()) {
        appendLog(app, "Native file dialogs are currently only available on Windows.");
        return;
    }

    const auto path = openNativeFileDialog(app, L"Open Sprite Metadata", L"JSON Metadata (*.json)\0*.json\0All Files (*.*)\0*.*\0", app.metadataPath.data());
    if (path.has_value()) {
        loadMetadataPreview(app, *path);
    }
}

static bool exportMetadata(AppState& app, const std::string& path)
{
    json root;
    root["image"] = fs::path(app.texture.path).filename().string();
    root["size"] = {{"w", app.texture.width}, {"h", app.texture.height}};
    root["sprites"] = json::array();
    for (const SpriteRect& sprite : app.sprites) {
        root["sprites"].push_back(spriteToJson(sprite));
    }
    root["animation"] = {
        {"name", app.animation.name},
        {"fps", app.animation.fps},
        {"loop", app.animation.loop},
        {"frames", app.animation.frames}
    };

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        appendLog(app, "Metadata JSON export failed: cannot write " + path);
        return false;
    }
    file << root.dump(4);
    setBuffer(app.exportPath, path);
    appendLog(app, "Exported metadata JSON: " + path);
    return true;
}

static bool exportSpritesAsPngs(AppState& app, const std::string& directory)
{
    if (!app.texture.valid() || app.texture.pixels.empty()) {
        appendLog(app, "Sprite PNG export failed: no source RGBA pixels are available.");
        return false;
    }
    if (app.sprites.empty()) {
        appendLog(app, "Sprite PNG export failed: no sprites to export.");
        return false;
    }

    const fs::path outputDir = directory.empty() ? fs::path("exported_sprites") : fs::path(directory);
    std::error_code error;
    fs::create_directories(outputDir, error);
    if (error) {
        appendLog(app, "Sprite PNG export failed: cannot create directory " + outputDir.string());
        return false;
    }

    int exported = 0;
    for (size_t i = 0; i < app.sprites.size(); ++i) {
        int width = 0;
        int height = 0;
        const std::vector<unsigned char> pixels = cropSpritePixels(app.texture.width, app.texture.height, app.texture.pixels, app.sprites[i], width, height, app.colorKey);
        if (pixels.empty() || width <= 0 || height <= 0) {
            appendLog(app, "Skipped invalid sprite for PNG export: " + app.sprites[i].name);
            continue;
        }

        const std::string filename = std::to_string(i) + "_" + sanitizedSpriteFilename(app.sprites[i].name) + ".png";
        const fs::path outputPath = outputDir / filename;
        if (stbi_write_png(outputPath.string().c_str(), width, height, 4, pixels.data(), width * 4) == 0) {
            appendLog(app, "Sprite PNG export failed: cannot write " + outputPath.string());
            return false;
        }
        ++exported;
    }

    setBuffer(app.spriteExportDir, outputDir.string());
    appendLog(app, "Exported " + std::to_string(exported) + " sprite PNGs to: " + outputDir.string());
    return exported > 0;
}

static bool exportCurrentAnimationGif(AppState& app, const std::string& path)
{
    if (!app.texture.valid() || app.texture.pixels.empty()) {
        appendLog(app, "Animation GIF export failed: no source RGBA pixels are available.");
        return false;
    }
    if (app.animation.frames.empty()) {
        appendLog(app, "Animation GIF export failed: animation has no frames.");
        return false;
    }

    const std::string outputPath = path.empty() ? "animation.gif" : path;
    const bool ok = exportAnimationGif(outputPath, app.texture.width, app.texture.height, app.texture.pixels, app.sprites, app.animation.frames, app.animation.fps, app.animationGif, app.colorKey);
    if (ok) {
        setBuffer(app.animationGifPath, outputPath);
        appendLog(app, "Exported animation GIF: " + outputPath);
    } else {
        appendLog(app, "Animation GIF export failed: " + outputPath);
    }
    return ok;
}

static int currentExtractorAlphaThreshold(const AppState& app)
{
    if (app.extractionMethod == SpriteExtractionMethod::MetadataImport) {
        return 0;
    }
    if (app.extractionMethod == SpriteExtractionMethod::AnimationStrip) {
        return app.animationStrip.alphaThreshold;
    }
    return app.extractionMethod == SpriteExtractionMethod::TransparencyRegions ? app.transparencyRegions.alphaThreshold : app.grid.alphaThreshold;
}

static std::vector<SpriteRect> buildExtractorPreview(const AppState& app)
{
    if (!app.texture.valid()) {
        return {};
    }

    std::vector<SpriteRect> sprites;
    if (app.extractionMethod == SpriteExtractionMethod::MetadataImport) {
        sprites = app.metadataSprites;
    } else if (app.extractionMethod == SpriteExtractionMethod::AnimationStrip) {
        sprites = extractAnimationStripSprites(app.texture.width, app.texture.height, app.texture.pixels, app.animationStrip, app.colorKey);
    } else if (app.extractionMethod == SpriteExtractionMethod::TransparencyRegions) {
        sprites = extractTransparencyRegionSprites(app.texture.width, app.texture.height, app.texture.pixels, app.transparencyRegions, app.colorKey);
    } else {
        sprites = extractGridSprites(app.texture.width, app.texture.height, app.texture.pixels, app.grid, app.colorKey);
    }

    SpritePostProcessSettings postProcess = app.extractorPostProcess;
    postProcess.namePattern = app.extractorNamePattern.data();
    return postProcessSprites(app.texture.width, app.texture.height, app.texture.pixels, sprites, postProcess, app.colorKey, currentExtractorAlphaThreshold(app));
}

static constexpr std::array<float, 14> PhysicalPixelZoomLevels = {
    0.125f, 0.25f, 0.5f,
    1.0f, 2.0f, 3.0f, 4.0f,
    6.0f, 8.0f, 12.0f, 16.0f,
    24.0f, 32.0f, 48.0f
};

static float effectiveFramebufferScale(const AppState& app)
{
    return std::max(0.001f, (app.canvas.framebufferScale.x + app.canvas.framebufferScale.y) * 0.5f);
}

static float logicalZoomForPhysicalZoom(const AppState& app, float physicalZoom)
{
    return physicalZoom / effectiveFramebufferScale(app);
}

static float physicalZoomForLogicalZoom(const AppState& app, float logicalZoom)
{
    return logicalZoom * effectiveFramebufferScale(app);
}

static int nearestZoomLevelIndex(const AppState& app, float logicalZoom)
{
    const float physicalZoom = physicalZoomForLogicalZoom(app, logicalZoom);
    int bestIndex = 0;
    float bestDistance = std::abs(PhysicalPixelZoomLevels[0] - physicalZoom);
    for (int i = 1; i < static_cast<int>(PhysicalPixelZoomLevels.size()); ++i) {
        const float distance = std::abs(PhysicalPixelZoomLevels[static_cast<size_t>(i)] - physicalZoom);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    return bestIndex;
}

static ImVec2 snapScreenPixel(const AppState& app, const ImVec2& value)
{
    if (!app.canvas.pixelPerfect) {
        return value;
    }
    const float scaleX = std::max(0.001f, app.canvas.framebufferScale.x);
    const float scaleY = std::max(0.001f, app.canvas.framebufferScale.y);
    return ImVec2(std::floor(value.x * scaleX) / scaleX, std::floor(value.y * scaleY) / scaleY);
}

static ImVec2 imageToScreen(const AppState& app, const ImVec2& canvasMin, const ImVec2& imagePoint)
{
    return ImVec2(canvasMin.x + app.canvas.pan.x + imagePoint.x * app.canvas.zoom,
                  canvasMin.y + app.canvas.pan.y + imagePoint.y * app.canvas.zoom);
}

static ImVec2 imageToScreenSnapped(const AppState& app, const ImVec2& canvasMin, const ImVec2& imagePoint)
{
    return snapScreenPixel(app, imageToScreen(app, canvasMin, imagePoint));
}

static ImVec2 screenToImage(const AppState& app, const ImVec2& canvasMin, const ImVec2& screenPoint)
{
    return ImVec2((screenPoint.x - canvasMin.x - app.canvas.pan.x) / app.canvas.zoom,
                  (screenPoint.y - canvasMin.y - app.canvas.pan.y) / app.canvas.zoom);
}

static bool pointInSprite(const SpriteRect& sprite, const ImVec2& p)
{
    return p.x >= static_cast<float>(sprite.x) && p.y >= static_cast<float>(sprite.y)
        && p.x <= static_cast<float>(sprite.x + sprite.w) && p.y <= static_cast<float>(sprite.y + sprite.h);
}

static SpriteRect normalizedDragRect(const ImVec2& a, const ImVec2& b, int imageW, int imageH)
{
    const float minX = std::clamp(std::min(a.x, b.x), 0.0f, static_cast<float>(imageW));
    const float minY = std::clamp(std::min(a.y, b.y), 0.0f, static_cast<float>(imageH));
    const float maxX = std::clamp(std::max(a.x, b.x), 0.0f, static_cast<float>(imageW));
    const float maxY = std::clamp(std::max(a.y, b.y), 0.0f, static_cast<float>(imageH));

    SpriteRect sprite;
    sprite.x = static_cast<int>(std::floor(minX));
    sprite.y = static_cast<int>(std::floor(minY));
    sprite.w = std::max(1, static_cast<int>(std::ceil(maxX - minX)));
    sprite.h = std::max(1, static_cast<int>(std::ceil(maxY - minY)));
    return sprite;
}

static void drawChecker(ImDrawList* drawList, ImVec2 min, ImVec2 max, float cellSize)
{
    const ImU32 a = IM_COL32(70, 70, 70, 255);
    const ImU32 b = IM_COL32(48, 48, 48, 255);
    cellSize = std::max(4.0f, cellSize);

    for (float y = min.y; y < max.y; y += cellSize) {
        for (float x = min.x; x < max.x; x += cellSize) {
            const int ix = static_cast<int>((x - min.x) / cellSize);
            const int iy = static_cast<int>((y - min.y) / cellSize);
            const ImVec2 cellMax(std::min(x + cellSize, max.x), std::min(y + cellSize, max.y));
            drawList->AddRectFilled(ImVec2(x, y), cellMax, ((ix + iy) % 2 == 0) ? a : b);
        }
    }
}

static void drawSpriteRect(ImDrawList* drawList, const AppState& app, const ImVec2& canvasMin, const SpriteRect& sprite, ImU32 color, bool filled)
{
    const ImVec2 a = imageToScreenSnapped(app, canvasMin, ImVec2(static_cast<float>(sprite.x), static_cast<float>(sprite.y)));
    const ImVec2 b = imageToScreenSnapped(app, canvasMin, ImVec2(static_cast<float>(sprite.x + sprite.w), static_cast<float>(sprite.y + sprite.h)));
    if (filled) {
        drawList->AddRectFilled(a, b, IM_COL32(255, 210, 40, 32));
    }
    drawList->AddRect(a, b, color, 0.0f, 0, 2.0f);
}

static std::optional<ImVec2> hoveredImagePixel(const AppState& app, const ImVec2& imagePoint)
{
    if (!app.texture.valid() || imagePoint.x < 0.0f || imagePoint.y < 0.0f || imagePoint.x >= static_cast<float>(app.texture.width) || imagePoint.y >= static_cast<float>(app.texture.height)) {
        return std::nullopt;
    }

    return ImVec2(std::floor(imagePoint.x), std::floor(imagePoint.y));
}

static void drawHoveredPixelIndicator(ImDrawList* drawList, const AppState& app, const ImVec2& canvasMin, const ImVec2& canvasMax, const ImVec2& pixel)
{
    const ImVec2 pixelMin = imageToScreenSnapped(app, canvasMin, pixel);
    const ImVec2 pixelMax = imageToScreenSnapped(app, canvasMin, ImVec2(pixel.x + 1.0f, pixel.y + 1.0f));
    drawList->AddRectFilled(pixelMin, pixelMax, IM_COL32(255, 255, 255, 48));
    drawList->AddRect(pixelMin, pixelMax, IM_COL32(255, 255, 0, 255), 0.0f, 0, 1.5f);

    const float centerX = (pixelMin.x + pixelMax.x) * 0.5f;
    const float centerY = (pixelMin.y + pixelMax.y) * 0.5f;
    drawList->AddLine(ImVec2(centerX, canvasMin.y), ImVec2(centerX, canvasMax.y), IM_COL32(255, 255, 0, 96), 1.0f);
    drawList->AddLine(ImVec2(canvasMin.x, centerY), ImVec2(canvasMax.x, centerY), IM_COL32(255, 255, 0, 96), 1.0f);
}

static void fitCanvasToImage(AppState& app, const ImVec2& canvasSize)
{
    if (!app.texture.valid() || canvasSize.x <= 1.0f || canvasSize.y <= 1.0f) {
        return;
    }
    const float zoomX = canvasSize.x / static_cast<float>(app.texture.width);
    const float zoomY = canvasSize.y / static_cast<float>(app.texture.height);
    app.canvas.zoom = std::clamp(std::min(zoomX, zoomY) * 0.90f, 0.05f, 64.0f);
    app.canvas.zoomLevelIndex = nearestZoomLevelIndex(app, app.canvas.zoom);
    app.canvas.pan = ImVec2(
        (canvasSize.x - static_cast<float>(app.texture.width) * app.canvas.zoom) * 0.5f,
        (canvasSize.y - static_cast<float>(app.texture.height) * app.canvas.zoom) * 0.5f
    );
}

static void updateCanvasFramebufferScale(AppState& app, GLFWwindow* window)
{
    int windowW = 0;
    int windowH = 0;
    int framebufferW = 0;
    int framebufferH = 0;
    glfwGetWindowSize(window, &windowW, &windowH);
    glfwGetFramebufferSize(window, &framebufferW, &framebufferH);
    if (windowW > 0 && windowH > 0 && framebufferW > 0 && framebufferH > 0) {
        app.canvas.framebufferScale = ImVec2(
            static_cast<float>(framebufferW) / static_cast<float>(windowW),
            static_cast<float>(framebufferH) / static_cast<float>(windowH)
        );
    }
    g_rawFramebufferScale = app.canvas.framebufferScale;
}

static void updateAnimation(AnimationState& animation, double deltaSeconds)
{
    if (!animation.playing || animation.frames.empty() || animation.fps <= 0.0f) {
        return;
    }

    animation.accumulator += deltaSeconds;
    const double frameTime = 1.0 / static_cast<double>(animation.fps);
    while (animation.accumulator >= frameTime) {
        animation.accumulator -= frameTime;
        animation.currentFrame++;
        if (animation.currentFrame >= static_cast<int>(animation.frames.size())) {
            if (animation.loop) {
                animation.currentFrame = 0;
            } else {
                animation.currentFrame = static_cast<int>(animation.frames.size()) - 1;
                animation.playing = false;
                break;
            }
        }
    }
}

static std::optional<SpriteRect> currentAnimationSprite(const AppState& app)
{
    if (!app.animation.previewOnCanvas || app.animation.frames.empty()) {
        return std::nullopt;
    }
    const int frameSlot = std::clamp(app.animation.currentFrame, 0, static_cast<int>(app.animation.frames.size()) - 1);
    const int spriteIndex = app.animation.frames[static_cast<size_t>(frameSlot)];
    if (spriteIndex < 0 || spriteIndex >= static_cast<int>(app.sprites.size())) {
        return std::nullopt;
    }
    return app.sprites[static_cast<size_t>(spriteIndex)];
}

static void fitCanvasToRect(CanvasState& canvas, const SpriteRect& sprite, const ImVec2& canvasSize)
{
    if (sprite.w <= 0 || sprite.h <= 0 || canvasSize.x <= 1.0f || canvasSize.y <= 1.0f) {
        return;
    }

    const float availableW = std::max(1.0f, canvasSize.x - 40.0f);
    const float availableH = std::max(1.0f, canvasSize.y - 40.0f);
    canvas.zoom = std::max(0.01f, std::min(availableW / static_cast<float>(sprite.w), availableH / static_cast<float>(sprite.h)));
    canvas.zoomLevelIndex = 3;
    canvas.pan = ImVec2(
        (canvasSize.x - static_cast<float>(sprite.w) * canvas.zoom) * 0.5f,
        (canvasSize.y - static_cast<float>(sprite.h) * canvas.zoom) * 0.5f
    );
}

static std::pair<ImVec2, ImVec2> zoomedPreviewRect(const AppState& app, const SpriteRect& sprite, const ImVec2& canvasMin)
{
    const ImVec2 pos(canvasMin.x + app.canvas.pan.x, canvasMin.y + app.canvas.pan.y);
    const ImVec2 end(pos.x + static_cast<float>(sprite.w) * app.canvas.zoom, pos.y + static_cast<float>(sprite.h) * app.canvas.zoom);
    return {pos, end};
}

static bool deleteSelectedSprite(AppState& app)
{
    if (app.selectedSprite < 0 || app.selectedSprite >= static_cast<int>(app.sprites.size())) {
        return false;
    }

    const int removed = app.selectedSprite;
    const std::string removedName = app.sprites[static_cast<size_t>(removed)].name;
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
    appendLog(app, "Deleted sprite: " + removedName);
    return true;
}

static void drawCanvasWindow(AppState& app)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    const ImGuiWindowFlags canvasFlags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("MainCanvas", nullptr, canvasFlags);

    bool fitRequested = false;
    if (ImGui::Button("Fit")) {
        fitRequested = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("100%")) {
        app.canvas.zoom = 1.0f;
        app.canvas.zoomLevelIndex = nearestZoomLevelIndex(app, app.canvas.zoom);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Checker", &app.canvas.showChecker);
    ImGui::SameLine();
    ImGui::Checkbox("Pixel Perfect", &app.canvas.pixelPerfect);
    ImGui::SameLine();
    ImGui::Checkbox("Integer Zoom", &app.canvas.integerZoom);
    ImGui::SameLine();
    ImGui::Text("Channels:");
    ImGui::SameLine();
    drawChannelButton(app, ChannelView::Red, ImVec4(0.85f, 0.18f, 0.18f, 1.0f));
    ImGui::SameLine();
    drawChannelButton(app, ChannelView::Green, ImVec4(0.18f, 0.75f, 0.24f, 1.0f));
    ImGui::SameLine();
    drawChannelButton(app, ChannelView::Blue, ImVec4(0.22f, 0.38f, 0.95f, 1.0f));
    ImGui::SameLine();
    drawChannelButton(app, ChannelView::Alpha, ImVec4(0.72f, 0.72f, 0.72f, 1.0f));
    ImGui::SameLine();
    ImGui::Text("View: %s", channelViewLabel(app.canvas.channelView));
    ImGui::SameLine();
    ImGui::Text("Zoom: %.3gx / %.3gpx", app.canvas.zoom, physicalZoomForLogicalZoom(app, app.canvas.zoom));
    ImGui::SameLine();
    ImGui::Text("FB: %.3gx", effectiveFramebufferScale(app));
    ImGui::SameLine();
    ImGui::Text("Sprites: %d", static_cast<int>(app.sprites.size()));
    ImGui::Separator();

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
    const std::optional<SpriteRect> animSprite = currentAnimationSprite(app);
    if (fitRequested) {
        if (animSprite.has_value()) {
            fitCanvasToRect(app.canvas, *animSprite, canvasSize);
        } else {
            fitCanvasToImage(app, canvasSize);
        }
    }
    if (animSprite.has_value() && app.animation.previewOnCanvas && !app.animation.previewOnCanvasLastFrame) {
        fitCanvasToRect(app.canvas, *animSprite, canvasSize);
    }
    app.animation.previewOnCanvasLastFrame = app.animation.previewOnCanvas;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(28, 28, 30, 255));

    ImGui::InvisibleButton("canvas_input", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 hoveredImagePoint = screenToImage(app, canvasMin, io.MousePos);
    const std::optional<ImVec2> hoveredPixel = hovered ? hoveredImagePixel(app, hoveredImagePoint) : std::nullopt;
    if (hoveredPixel.has_value()) {
        app.hoveredPixelX = static_cast<int>(hoveredPixel->x);
        app.hoveredPixelY = static_cast<int>(hoveredPixel->y);
    } else if (hovered) {
        app.hoveredPixelX = -1;
        app.hoveredPixelY = -1;
    }

    if (hovered && app.texture.valid() && io.MouseWheel != 0.0f) {
        const ImVec2 before = screenToImage(app, canvasMin, io.MousePos);
        if (app.canvas.integerZoom) {
            app.canvas.zoomLevelIndex = nearestZoomLevelIndex(app, app.canvas.zoom);
            const int direction = io.MouseWheel > 0.0f ? 1 : -1;
            app.canvas.zoomLevelIndex = std::clamp(app.canvas.zoomLevelIndex + direction, 0, static_cast<int>(PhysicalPixelZoomLevels.size()) - 1);
            app.canvas.zoom = logicalZoomForPhysicalZoom(app, PhysicalPixelZoomLevels[static_cast<size_t>(app.canvas.zoomLevelIndex)]);
        } else {
            const float factor = std::pow(1.15f, io.MouseWheel);
            app.canvas.zoom = std::clamp(app.canvas.zoom * factor, 0.05f, 64.0f);
            app.canvas.zoomLevelIndex = nearestZoomLevelIndex(app, app.canvas.zoom);
        }
        app.canvas.pan.x = io.MousePos.x - canvasMin.x - before.x * app.canvas.zoom;
        app.canvas.pan.y = io.MousePos.y - canvasMin.y - before.y * app.canvas.zoom;
    }

    const bool spaceDown = ImGui::IsKeyDown(ImGuiKey_Space);
    if (hovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || (spaceDown && ImGui::IsMouseDragging(ImGuiMouseButton_Left)))) {
        app.canvas.pan.x += io.MouseDelta.x;
        app.canvas.pan.y += io.MouseDelta.y;
    }

    if (hovered && app.texture.valid() && !spaceDown && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ImVec2 p = screenToImage(app, canvasMin, io.MousePos);
        app.selectedSprite = -1;
        for (int i = static_cast<int>(app.sprites.size()) - 1; i >= 0; --i) {
            if (pointInSprite(app.sprites[static_cast<size_t>(i)], p)) {
                app.selectedSprite = i;
                setBuffer(app.renameBuffer, app.sprites[static_cast<size_t>(i)].name);
                break;
            }
        }
        if (app.selectedSprite < 0 && p.x >= 0.0f && p.y >= 0.0f && p.x <= app.texture.width && p.y <= app.texture.height) {
            app.canvas.draggingRect = true;
            app.canvas.dragStartImage = p;
            app.canvas.dragEndImage = p;
        }
    }

    if (app.canvas.draggingRect) {
        app.canvas.dragEndImage = screenToImage(app, canvasMin, io.MousePos);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            app.canvas.draggingRect = false;
            appendLog(app, "Cancelled manual sprite draft");
        } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            SpriteRect sprite = normalizedDragRect(app.canvas.dragStartImage, app.canvas.dragEndImage, app.texture.width, app.texture.height);
            if (sprite.w >= 2 && sprite.h >= 2) {
                sprite.name = "manual_" + std::to_string(app.sprites.size());
                app.sprites.push_back(sprite);
                app.selectedSprite = static_cast<int>(app.sprites.size()) - 1;
                setBuffer(app.renameBuffer, sprite.name);
                appendLog(app, "Created manual sprite: " + sprite.name);
            }
            app.canvas.draggingRect = false;
        }
    } else if (!io.WantTextInput && app.selectedSprite >= 0 && (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))) {
        deleteSelectedSprite(app);
    }

    drawList->PushClipRect(canvasMin, canvasMax, true);

    if (app.texture.valid()) {
        const GLuint canvasTextureId = channelPreviewTexture(app);
        setPixelTextureFiltering(canvasTextureId);
        if (animSprite.has_value()) {
            const SpriteRect& sprite = *animSprite;
            const auto [pos, end] = zoomedPreviewRect(app, sprite, canvasMin);
            if (app.canvas.showChecker) {
                drawChecker(drawList, pos, end, 8.0f * app.canvas.zoom);
            }
            const ImVec2 uv0(static_cast<float>(sprite.x) / app.texture.width, static_cast<float>(sprite.y) / app.texture.height);
            const ImVec2 uv1(static_cast<float>(sprite.x + sprite.w) / app.texture.width, static_cast<float>(sprite.y + sprite.h) / app.texture.height);
            addRawOpenGLImage(drawList, canvasTextureId, pos, end, uv0, uv1);
            drawList->AddRect(pos, end, IM_COL32(255, 210, 40, 255), 0.0f, 0, 2.0f);
            drawList->AddText(ImVec2(canvasMin.x + 20.0f, canvasMin.y + 20.0f), IM_COL32(255, 210, 40, 255), "Animation Preview On Canvas");
        } else {
            const ImVec2 imageMin = imageToScreenSnapped(app, canvasMin, ImVec2(0.0f, 0.0f));
            const ImVec2 imageMax = imageToScreenSnapped(app, canvasMin, ImVec2(static_cast<float>(app.texture.width), static_cast<float>(app.texture.height)));
            if (app.canvas.showChecker) {
                drawChecker(drawList, imageMin, imageMax, 8.0f * app.canvas.zoom);
            }
            addRawOpenGLImage(drawList, canvasTextureId, imageMin, imageMax);
            drawList->AddRect(imageMin, imageMax, IM_COL32(180, 180, 180, 255));

            for (const SpriteRect& sprite : app.gridPreview) {
                drawSpriteRect(drawList, app, canvasMin, sprite, IM_COL32(80, 180, 255, 160), false);
            }
            for (size_t i = 0; i < app.sprites.size(); ++i) {
                const bool selected = static_cast<int>(i) == app.selectedSprite;
                drawSpriteRect(drawList, app, canvasMin, app.sprites[i], selected ? IM_COL32(255, 210, 40, 255) : IM_COL32(80, 255, 140, 220), selected);
            }
            if (app.canvas.draggingRect) {
                const SpriteRect draft = normalizedDragRect(app.canvas.dragStartImage, app.canvas.dragEndImage, app.texture.width, app.texture.height);
                drawSpriteRect(drawList, app, canvasMin, draft, IM_COL32(255, 120, 80, 255), true);
            }
            if (hoveredPixel.has_value()) {
                drawHoveredPixelIndicator(drawList, app, canvasMin, canvasMax, *hoveredPixel);
            }
            if (app.canvas.showPackingRects) {
                for (const SpriteRect& rect : app.packingPreviewRects) {
                    drawSpriteRect(drawList, app, canvasMin, rect, IM_COL32(255, 40, 40, 220), false);
                }
            }
        }
    } else {
        drawList->AddText(ImVec2(canvasMin.x + 20.0f, canvasMin.y + 20.0f), IM_COL32(220, 220, 220, 255), "Enter a PNG path in the Project window and click Load PNG.");
    }

    drawList->PopClipRect();
    drawCanvasInfoOverlay(drawList, app, canvasMin);

    if (hovered && app.texture.valid()) {
        ImGui::BeginTooltip();
        if (hoveredPixel.has_value()) {
            ImGui::Text("Pixel: %d, %d", static_cast<int>(hoveredPixel->x), static_cast<int>(hoveredPixel->y));
        } else {
            ImGui::Text("Pixel: outside image");
        }
        ImGui::Text("Image: %.2f, %.2f", hoveredImagePoint.x, hoveredImagePoint.y);
        ImGui::Text("Zoom: %.0f%%", app.canvas.zoom * 100.0f);
        ImGui::Text("Pan: %.1f, %.1f", app.canvas.pan.x, app.canvas.pan.y);
        ImGui::EndTooltip();
    }

    ImGui::End();
}

static void drawProjectWindow(AppState& app)
{
    if (!app.showProject) {
        return;
    }
    ImGui::Begin("Project", &app.showProject);
    ImGui::TextWrapped("Use path input or open a native file dialog. Recommended project extension: .sprited.json");
    ImGui::InputText("PNG Path", app.imagePath.data(), app.imagePath.size());
    if (ImGui::Button("Load PNG")) {
        loadPngTexture(app, app.imagePath.data());
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse PNG...")) {
        browseAndLoadPng(app);
    }
    ImGui::Separator();
    ImGui::Checkbox("Enable Color Key", &app.colorKey.enabled);
    ImGui::InputInt("Color Key R", &app.colorKey.r);
    ImGui::InputInt("Color Key G", &app.colorKey.g);
    ImGui::InputInt("Color Key B", &app.colorKey.b);
    ImGui::InputInt("Color Key Tolerance", &app.colorKey.tolerance);
    app.colorKey.r = std::clamp(app.colorKey.r, 0, 255);
    app.colorKey.g = std::clamp(app.colorKey.g, 0, 255);
    app.colorKey.b = std::clamp(app.colorKey.b, 0, 255);
    app.colorKey.tolerance = std::clamp(app.colorKey.tolerance, 0, 255);
    if (ImGui::Button("Pick Color Key From Hovered Pixel")) {
        if (app.texture.valid() && app.hoveredPixelX >= 0 && app.hoveredPixelY >= 0) {
            const size_t pixelIndex = (static_cast<size_t>(app.hoveredPixelY) * static_cast<size_t>(app.texture.width) + static_cast<size_t>(app.hoveredPixelX)) * 4;
            if (pixelIndex + 2 < app.texture.pixels.size()) {
                app.colorKey.r = app.texture.pixels[pixelIndex + 0];
                app.colorKey.g = app.texture.pixels[pixelIndex + 1];
                app.colorKey.b = app.texture.pixels[pixelIndex + 2];
                app.colorKey.enabled = true;
                invalidateChannelPreview(app.texture);
                appendLog(app, "Picked color key from pixel " + std::to_string(app.hoveredPixelX) + "," + std::to_string(app.hoveredPixelY));
            }
        } else {
            appendLog(app, "Color key pick failed: hover a pixel on the canvas first.");
        }
    }
    ImGui::TextDisabled("Color key affects canvas preview, grid transparent filtering, and sprite PNG export.");
    ImGui::Separator();
    ImGui::InputText("Project Path", app.projectPath.data(), app.projectPath.size());
    if (ImGui::Button("Save Project")) {
        saveProject(app, app.projectPath.data()[0] ? app.projectPath.data() : "project.sprited.json");
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Project")) {
        loadProject(app, app.projectPath.data());
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse Project...")) {
        browseAndLoadProject(app);
    }
    ImGui::Separator();
    ImGui::Checkbox("Project", &app.showProject);
    ImGui::Checkbox("Sprite Extractor", &app.showExtractor);
    ImGui::Checkbox("Sprites", &app.showSprites);
    ImGui::Checkbox("Sprite Inspector", &app.showInspector);
    ImGui::Checkbox("Animation", &app.showAnimation);
    ImGui::Checkbox("Export", &app.showExport);
    ImGui::Checkbox("Sprite Sheet Builder", &app.showSheetBuilder);
    ImGui::Checkbox("Sprite Layout", &app.showSpriteLayout);
    ImGui::Checkbox("Log", &app.showLog);
    ImGui::Checkbox("ImGui Demo", &app.showDemo);
    ImGui::End();
}

static void drawExtractorWindow(AppState& app)
{
    if (!app.showExtractor) {
        app.gridPreview.clear();
        return;
    }

    ImGui::Begin("Sprite Extractor", &app.showExtractor);

    ImGui::Text("Source");
    if (app.texture.valid()) {
        ImGui::Text("PNG: %s", currentPngFilename(app).c_str());
        ImGui::Text("Size: %d x %d", app.texture.width, app.texture.height);
    } else {
        ImGui::TextDisabled("PNG: none");
        ImGui::TextDisabled("Size: none");
    }
    ImGui::Text("Sprites: %d", static_cast<int>(app.sprites.size()));
    ImGui::Text("Color Key: %s", app.colorKey.enabled ? "Enabled" : "Disabled");

    bool extractorSettingsChanged = false;
    bool metadataSettingsChanged = false;

    ImGui::Separator();
    ImGui::Text("Method");
    const char* methodLabels[] = {"Grid", "Transparency Regions", "Animation Strip", "Metadata Import"};
    int methodIndex = 0;
    if (app.extractionMethod == SpriteExtractionMethod::TransparencyRegions) {
        methodIndex = 1;
    } else if (app.extractionMethod == SpriteExtractionMethod::AnimationStrip) {
        methodIndex = 2;
    } else if (app.extractionMethod == SpriteExtractionMethod::MetadataImport) {
        methodIndex = 3;
    }
    if (ImGui::Combo("Method", &methodIndex, methodLabels, static_cast<int>(std::size(methodLabels)))) {
        if (methodIndex == 3) {
            app.extractionMethod = SpriteExtractionMethod::MetadataImport;
        } else if (methodIndex == 2) {
            app.extractionMethod = SpriteExtractionMethod::AnimationStrip;
        } else if (methodIndex == 1) {
            app.extractionMethod = SpriteExtractionMethod::TransparencyRegions;
        } else {
            app.extractionMethod = SpriteExtractionMethod::Grid;
        }
        extractorSettingsChanged = true;
    }

    ImGui::Separator();
    if (app.extractionMethod == SpriteExtractionMethod::Grid) {
        ImGui::Text("Grid Settings");
        int sizingMode = app.grid.sizingMode == GridSizingMode::RowsColumns ? 1 : 0;
        ImGui::Text("Grid Mode");
        if (ImGui::RadioButton("Cell Size", sizingMode == 0)) {
            if (app.grid.sizingMode != GridSizingMode::CellSize) {
                app.grid.sizingMode = GridSizingMode::CellSize;
                extractorSettingsChanged = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Rows x Columns", sizingMode == 1)) {
            if (app.grid.sizingMode != GridSizingMode::RowsColumns) {
                app.grid.sizingMode = GridSizingMode::RowsColumns;
                extractorSettingsChanged = true;
            }
        }

        if (app.grid.sizingMode == GridSizingMode::RowsColumns) {
            extractorSettingsChanged |= ImGui::InputInt("Rows", &app.grid.rows);
            extractorSettingsChanged |= ImGui::InputInt("Columns", &app.grid.columns);
        } else {
            extractorSettingsChanged |= ImGui::InputInt("Cell Width", &app.grid.cellW);
            extractorSettingsChanged |= ImGui::InputInt("Cell Height", &app.grid.cellH);
        }
        extractorSettingsChanged |= ImGui::InputInt("Spacing X", &app.grid.spacingX);
        extractorSettingsChanged |= ImGui::InputInt("Spacing Y", &app.grid.spacingY);
        extractorSettingsChanged |= ImGui::InputInt("Margin X", &app.grid.marginX);
        extractorSettingsChanged |= ImGui::InputInt("Margin Y", &app.grid.marginY);
        extractorSettingsChanged |= ImGui::InputInt("Offset X", &app.grid.offsetX);
        extractorSettingsChanged |= ImGui::InputInt("Offset Y", &app.grid.offsetY);

        ImGui::Separator();
        ImGui::Text("Filtering");
        extractorSettingsChanged |= ImGui::Checkbox("Skip Transparent Cells", &app.grid.skipTransparentCells);
        extractorSettingsChanged |= ImGui::InputInt("Alpha Threshold", &app.grid.alphaThreshold);
    } else if (app.extractionMethod == SpriteExtractionMethod::TransparencyRegions) {
        ImGui::Text("Transparency Region Settings");
        extractorSettingsChanged |= ImGui::InputInt("Alpha Threshold", &app.transparencyRegions.alphaThreshold);
        int connectivity = app.transparencyRegions.connectivity == RegionConnectivity::EightWay ? 1 : 0;
        ImGui::Text("Connectivity");
        if (ImGui::RadioButton("4-way", connectivity == 0)) {
            if (app.transparencyRegions.connectivity != RegionConnectivity::FourWay) {
                app.transparencyRegions.connectivity = RegionConnectivity::FourWay;
                extractorSettingsChanged = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("8-way", connectivity == 1)) {
            if (app.transparencyRegions.connectivity != RegionConnectivity::EightWay) {
                app.transparencyRegions.connectivity = RegionConnectivity::EightWay;
                extractorSettingsChanged = true;
            }
        }
        extractorSettingsChanged |= ImGui::InputInt("Minimum Area", &app.transparencyRegions.minimumArea);
        extractorSettingsChanged |= ImGui::InputInt("Minimum Width", &app.transparencyRegions.minimumWidth);
        extractorSettingsChanged |= ImGui::InputInt("Minimum Height", &app.transparencyRegions.minimumHeight);
        ImGui::TextDisabled("Finds connected visible pixels using Alpha and Color Key.");
    } else if (app.extractionMethod == SpriteExtractionMethod::AnimationStrip) {
        ImGui::Text("Animation Strip Settings");
        int direction = app.animationStrip.direction == StripDirection::Vertical ? 1 : 0;
        ImGui::Text("Direction");
        if (ImGui::RadioButton("Horizontal", direction == 0)) {
            if (app.animationStrip.direction != StripDirection::Horizontal) {
                app.animationStrip.direction = StripDirection::Horizontal;
                extractorSettingsChanged = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Vertical", direction == 1)) {
            if (app.animationStrip.direction != StripDirection::Vertical) {
                app.animationStrip.direction = StripDirection::Vertical;
                extractorSettingsChanged = true;
            }
        }
        extractorSettingsChanged |= ImGui::InputInt("Frame Count", &app.animationStrip.frameCount);
        extractorSettingsChanged |= ImGui::Checkbox("Auto Frame Size", &app.animationStrip.useAutoFrameSize);
        if (!app.animationStrip.useAutoFrameSize) {
            extractorSettingsChanged |= ImGui::InputInt("Frame Width", &app.animationStrip.frameWidth);
            extractorSettingsChanged |= ImGui::InputInt("Frame Height", &app.animationStrip.frameHeight);
        } else {
            ImGui::TextDisabled("Auto size uses image bounds, start position, frame count, and spacing.");
        }
        extractorSettingsChanged |= ImGui::InputInt("Start X", &app.animationStrip.startX);
        extractorSettingsChanged |= ImGui::InputInt("Start Y", &app.animationStrip.startY);
        extractorSettingsChanged |= ImGui::InputInt("Spacing", &app.animationStrip.spacing);
        extractorSettingsChanged |= ImGui::Checkbox("Skip Transparent Frames", &app.animationStrip.skipTransparentFrames);
        extractorSettingsChanged |= ImGui::InputInt("Alpha Threshold", &app.animationStrip.alphaThreshold);
    } else {
        ImGui::Text("Metadata Import Settings");
        metadataSettingsChanged |= ImGui::InputText("Metadata Path", app.metadataPath.data(), app.metadataPath.size());
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            browseAndLoadMetadata(app);
            extractorSettingsChanged = true;
        }
        const char* metadataFormatLabels[] = {"Auto Detect", "Aseprite JSON", "TexturePacker JSON"};
        int metadataFormat = 0;
        if (app.metadataImport.format == MetadataImportFormat::AsepriteJson) {
            metadataFormat = 1;
        } else if (app.metadataImport.format == MetadataImportFormat::TexturePackerJson) {
            metadataFormat = 2;
        }
        if (ImGui::Combo("Format", &metadataFormat, metadataFormatLabels, static_cast<int>(std::size(metadataFormatLabels)))) {
            app.metadataImport.format = metadataFormat == 2 ? MetadataImportFormat::TexturePackerJson : (metadataFormat == 1 ? MetadataImportFormat::AsepriteJson : MetadataImportFormat::AutoDetect);
            metadataSettingsChanged = true;
        }
        metadataSettingsChanged |= ImGui::Checkbox("Preserve Names", &app.metadataImport.preserveNames);
        metadataSettingsChanged |= ImGui::Checkbox("Skip Rotated Frames", &app.metadataImport.skipRotatedFrames);
        if (ImGui::Button("Load Metadata Preview")) {
            loadMetadataPreview(app, app.metadataPath.data());
            extractorSettingsChanged = true;
        }
        ImGui::Text("Loaded Metadata Sprites: %d", static_cast<int>(app.metadataSprites.size()));
        if (!app.metadataDetectedFormat.empty()) {
            ImGui::Text("Detected Format: %s", app.metadataDetectedFormat.c_str());
        }
        if (!app.metadataWarnings.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "Warnings: %d", static_cast<int>(app.metadataWarnings.size()));
            if (ImGui::BeginItemTooltip()) {
                for (const std::string& warning : app.metadataWarnings) {
                    ImGui::TextWrapped("%s", warning.c_str());
                }
                ImGui::EndTooltip();
            }
        }
        ImGui::TextDisabled("Supports third-party Aseprite/TexturePacker JSON frames only.");
    }

    const GridExtractorSettings gridBeforeClamp = app.grid;
    app.grid.cellW = std::max(1, app.grid.cellW);
    app.grid.cellH = std::max(1, app.grid.cellH);
    app.grid.rows = std::max(1, app.grid.rows);
    app.grid.columns = std::max(1, app.grid.columns);
    app.grid.spacingX = std::max(0, app.grid.spacingX);
    app.grid.spacingY = std::max(0, app.grid.spacingY);
    app.grid.marginX = std::max(0, app.grid.marginX);
    app.grid.marginY = std::max(0, app.grid.marginY);
    app.grid.alphaThreshold = std::clamp(app.grid.alphaThreshold, 0, 255);
    extractorSettingsChanged = extractorSettingsChanged
        || gridBeforeClamp.cellW != app.grid.cellW
        || gridBeforeClamp.cellH != app.grid.cellH
        || gridBeforeClamp.rows != app.grid.rows
        || gridBeforeClamp.columns != app.grid.columns
        || gridBeforeClamp.spacingX != app.grid.spacingX
        || gridBeforeClamp.spacingY != app.grid.spacingY
        || gridBeforeClamp.marginX != app.grid.marginX
        || gridBeforeClamp.marginY != app.grid.marginY
        || gridBeforeClamp.offsetX != app.grid.offsetX
        || gridBeforeClamp.offsetY != app.grid.offsetY
        || gridBeforeClamp.alphaThreshold != app.grid.alphaThreshold;

    const TransparencyRegionSettings transparencyBeforeClamp = app.transparencyRegions;
    app.transparencyRegions.alphaThreshold = std::clamp(app.transparencyRegions.alphaThreshold, 0, 255);
    app.transparencyRegions.minimumArea = std::max(1, app.transparencyRegions.minimumArea);
    app.transparencyRegions.minimumWidth = std::max(1, app.transparencyRegions.minimumWidth);
    app.transparencyRegions.minimumHeight = std::max(1, app.transparencyRegions.minimumHeight);
    extractorSettingsChanged = extractorSettingsChanged
        || transparencyBeforeClamp.alphaThreshold != app.transparencyRegions.alphaThreshold
        || transparencyBeforeClamp.minimumArea != app.transparencyRegions.minimumArea
        || transparencyBeforeClamp.minimumWidth != app.transparencyRegions.minimumWidth
        || transparencyBeforeClamp.minimumHeight != app.transparencyRegions.minimumHeight;

    const AnimationStripSettings stripBeforeClamp = app.animationStrip;
    app.animationStrip.frameCount = std::max(1, app.animationStrip.frameCount);
    app.animationStrip.frameWidth = std::max(1, app.animationStrip.frameWidth);
    app.animationStrip.frameHeight = std::max(1, app.animationStrip.frameHeight);
    app.animationStrip.startX = std::max(0, app.animationStrip.startX);
    app.animationStrip.startY = std::max(0, app.animationStrip.startY);
    app.animationStrip.spacing = std::max(0, app.animationStrip.spacing);
    app.animationStrip.alphaThreshold = std::clamp(app.animationStrip.alphaThreshold, 0, 255);
    extractorSettingsChanged = extractorSettingsChanged
        || stripBeforeClamp.frameCount != app.animationStrip.frameCount
        || stripBeforeClamp.frameWidth != app.animationStrip.frameWidth
        || stripBeforeClamp.frameHeight != app.animationStrip.frameHeight
        || stripBeforeClamp.startX != app.animationStrip.startX
        || stripBeforeClamp.startY != app.animationStrip.startY
        || stripBeforeClamp.spacing != app.animationStrip.spacing
        || stripBeforeClamp.alphaThreshold != app.animationStrip.alphaThreshold;

    if (app.extractionMethod == SpriteExtractionMethod::Grid && app.grid.sizingMode == GridSizingMode::RowsColumns && app.texture.valid()) {
        const int usableW = app.texture.width - (app.grid.marginX * 2) - app.grid.offsetX - (app.grid.spacingX * (app.grid.columns - 1));
        const int usableH = app.texture.height - (app.grid.marginY * 2) - app.grid.offsetY - (app.grid.spacingY * (app.grid.rows - 1));
        ImGui::Text("Computed Cell: %dx%d", usableW / app.grid.columns, usableH / app.grid.rows);
    }
    if (app.extractionMethod == SpriteExtractionMethod::AnimationStrip && app.animationStrip.useAutoFrameSize && app.texture.valid()) {
        int computedFrameW = 0;
        int computedFrameH = 0;
        if (app.animationStrip.direction == StripDirection::Horizontal) {
            computedFrameW = (app.texture.width - app.animationStrip.startX - app.animationStrip.spacing * (app.animationStrip.frameCount - 1)) / app.animationStrip.frameCount;
            computedFrameH = app.texture.height - app.animationStrip.startY;
        } else {
            computedFrameW = app.texture.width - app.animationStrip.startX;
            computedFrameH = (app.texture.height - app.animationStrip.startY - app.animationStrip.spacing * (app.animationStrip.frameCount - 1)) / app.animationStrip.frameCount;
        }
        ImGui::Text("Computed Frame: %dx%d", computedFrameW, computedFrameH);
    }
    if (app.extractionMethod == SpriteExtractionMethod::MetadataImport && metadataSettingsChanged && app.metadataPath[0] != '\0') {
        loadMetadataPreview(app, app.metadataPath.data());
        extractorSettingsChanged = true;
    }

    ImGui::Separator();
    ImGui::Text("Post Process");
    extractorSettingsChanged |= ImGui::Checkbox("Trim Transparent Border", &app.extractorPostProcess.trimTransparentBorder);
    extractorSettingsChanged |= ImGui::Checkbox("Rename Sprites", &app.extractorPostProcess.renameSprites);
    if (app.extractorPostProcess.renameSprites) {
        extractorSettingsChanged |= ImGui::InputText("Name Pattern", app.extractorNamePattern.data(), app.extractorNamePattern.size());
        ImGui::TextDisabled("Use {index}, for example sprite_{index} or idle_{index}.");
    }
    extractorSettingsChanged |= ImGui::Checkbox("Clamp To Image Bounds", &app.extractorPostProcess.clampToImageBounds);

    ImGui::Separator();
    ImGui::Text("Preview");
    ImGui::Checkbox("Live Preview", &app.gridLivePreview);
    ImGui::SameLine();
    const bool rebuildPreviewRequested = ImGui::Button("Rebuild Preview");
    if (rebuildPreviewRequested || app.gridLivePreview || extractorSettingsChanged) {
        app.gridPreview = buildExtractorPreview(app);
    }
    ImGui::Text("Rect Count: %d", static_cast<int>(app.gridPreview.size()));

    ImGui::Separator();
    ImGui::Text("Actions");
    if (ImGui::Button("Replace Sprites")) {
        app.sprites = buildExtractorPreview(app);
        app.gridPreview.clear();
        app.selectedSprite = app.sprites.empty() ? -1 : 0;
        if (app.selectedSprite >= 0) {
            setBuffer(app.renameBuffer, app.sprites[0].name);
        }
        appendLog(app, "Applied sprite extraction: " + std::to_string(app.sprites.size()) + " sprites");
    }
    ImGui::SameLine();
    if (ImGui::Button("Append Sprites")) {
        std::vector<SpriteRect> generated = buildExtractorPreview(app);
        const size_t oldSize = app.sprites.size();
        app.sprites.insert(app.sprites.end(), generated.begin(), generated.end());
        appendLog(app, "Appended sprite extraction: " + std::to_string(app.sprites.size() - oldSize) + " sprites");
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Preview")) {
        app.gridPreview.clear();
    }
    ImGui::End();
}

static void drawSpritesWindow(AppState& app)
{
    if (!app.showSprites) {
        return;
    }

    ImGui::Begin("Sprites", &app.showSprites);
    ImGui::Text("Sprites: %d", static_cast<int>(app.sprites.size()));

    if (!app.texture.valid()) {
        ImGui::Separator();
        ImGui::TextWrapped("No PNG loaded. Load a PNG from the Project window before browsing sliced sprites.");
        ImGui::End();
        return;
    }

    if (app.sprites.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("No sprites yet. Use Sprite Extractor to replace or append grid slices, or drag on the canvas to create manual sprite rectangles.");
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::InputText("Export Directory", app.spriteExportDir.data(), app.spriteExportDir.size());
    ImGui::SameLine();
    if (ImGui::Button("Export Sprites as PNGs")) {
        exportSpritesAsPngs(app, app.spriteExportDir.data()[0] ? app.spriteExportDir.data() : "exported_sprites");
    }
    ImGui::TextDisabled("Exports one PNG per sprite using filenames like 0_sprite.png.");
    ImGui::Separator();

    const float thumbnailSize = 72.0f;
    const float cardWidth = 150.0f;
    const float cardHeight = 128.0f;
    const float padding = 8.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float availableWidth = std::max(cardWidth, ImGui::GetContentRegionAvail().x);
    const int columns = std::max(1, static_cast<int>((availableWidth + spacing) / (cardWidth + spacing)));

    ImGui::BeginChild("SpriteCollectionScroll", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (int i = 0; i < static_cast<int>(app.sprites.size()); ++i) {
        if ((i % columns) != 0) {
            ImGui::SameLine();
        }

        ImGui::PushID(i);

        const bool selected = app.selectedSprite == i;
        const SpriteRect& sprite = app.sprites[static_cast<size_t>(i)];
        const ImVec2 cardMin = ImGui::GetCursorScreenPos();
        const ImVec2 cardMax(cardMin.x + cardWidth, cardMin.y + cardHeight);
        const ImVec2 thumbFrameMin(cardMin.x + padding, cardMin.y + padding);
        const ImVec2 thumbFrameMax(thumbFrameMin.x + thumbnailSize, thumbFrameMin.y + thumbnailSize);

        drawList->AddRectFilled(cardMin, cardMax, ImGui::GetColorU32(selected ? ImGuiCol_Header : ImGuiCol_FrameBg), 6.0f);

        if (ImGui::InvisibleButton("sprite_item", ImVec2(cardWidth, cardHeight))) {
            app.selectedSprite = i;
            setBuffer(app.renameBuffer, sprite.name);
        }

        if (ImGui::IsItemHovered() && !selected) {
            drawList->AddRect(cardMin, cardMax, ImGui::GetColorU32(ImGuiCol_HeaderHovered), 6.0f, 0, 2.0f);
        }

        drawList->AddRectFilled(thumbFrameMin, thumbFrameMax, IM_COL32(36, 36, 40, 255), 3.0f);

        const int spriteX = std::clamp(sprite.x, 0, std::max(0, app.texture.width - 1));
        const int spriteY = std::clamp(sprite.y, 0, std::max(0, app.texture.height - 1));
        const int spriteW = std::clamp(sprite.w, 1, std::max(1, app.texture.width - spriteX));
        const int spriteH = std::clamp(sprite.h, 1, std::max(1, app.texture.height - spriteY));
        const float previewScale = std::min(thumbnailSize / static_cast<float>(spriteW), thumbnailSize / static_cast<float>(spriteH));
        const float previewW = std::max(1.0f, static_cast<float>(spriteW) * previewScale);
        const float previewH = std::max(1.0f, static_cast<float>(spriteH) * previewScale);
        const ImVec2 imageMin(thumbFrameMin.x + (thumbnailSize - previewW) * 0.5f, thumbFrameMin.y + (thumbnailSize - previewH) * 0.5f);
        const ImVec2 imageMax(imageMin.x + previewW, imageMin.y + previewH);
        const ImVec2 uv0(static_cast<float>(spriteX) / static_cast<float>(app.texture.width), static_cast<float>(spriteY) / static_cast<float>(app.texture.height));
        const ImVec2 uv1(static_cast<float>(spriteX + spriteW) / static_cast<float>(app.texture.width), static_cast<float>(spriteY + spriteH) / static_cast<float>(app.texture.height));

        addRawOpenGLImage(drawList, app.texture.id, imageMin, imageMax, uv0, uv1);
        drawList->AddRect(thumbFrameMin, thumbFrameMax, IM_COL32(90, 90, 96, 255), 3.0f);

        const std::string nameLine = std::to_string(i) + ": " + sprite.name;
        const std::string sizeLine = std::to_string(sprite.w) + "x" + std::to_string(sprite.h) + " @ " + std::to_string(sprite.x) + "," + std::to_string(sprite.y);
        drawList->AddText(ImVec2(cardMin.x + padding, cardMin.y + padding + thumbnailSize + 8.0f), ImGui::GetColorU32(ImGuiCol_Text), nameLine.c_str());
        drawList->AddText(ImVec2(cardMin.x + padding, cardMin.y + padding + thumbnailSize + 26.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), sizeLine.c_str());

        drawList->AddRect(cardMin, cardMax, selected ? IM_COL32(255, 210, 40, 255) : IM_COL32(70, 70, 76, 255), 6.0f, 0, selected ? 2.0f : 1.0f);

        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::End();
}

static void drawInspectorWindow(AppState& app)
{
    if (!app.showInspector) {
        return;
    }
    ImGui::Begin("Sprite Inspector", &app.showInspector);
    if (app.selectedSprite < 0 || app.selectedSprite >= static_cast<int>(app.sprites.size())) {
        ImGui::TextWrapped("No sprite selected. Click an existing rectangle on the canvas, or drag on the image to create a manual rect.");
        ImGui::End();
        return;
    }

    SpriteRect& sprite = app.sprites[static_cast<size_t>(app.selectedSprite)];
    ImGui::Text("Index: %d", app.selectedSprite);
    if (ImGui::InputText("Name", app.renameBuffer.data(), app.renameBuffer.size())) {
        sprite.name = app.renameBuffer.data();
    }
    ImGui::DragInt("X", &sprite.x, 1.0f, 0, std::max(0, app.texture.width - 1));
    ImGui::DragInt("Y", &sprite.y, 1.0f, 0, std::max(0, app.texture.height - 1));
    ImGui::DragInt("W", &sprite.w, 1.0f, 1, std::max(1, app.texture.width));
    ImGui::DragInt("H", &sprite.h, 1.0f, 1, std::max(1, app.texture.height));
    sprite.x = std::clamp(sprite.x, 0, std::max(0, app.texture.width - 1));
    sprite.y = std::clamp(sprite.y, 0, std::max(0, app.texture.height - 1));
    sprite.w = std::clamp(sprite.w, 1, std::max(1, app.texture.width - sprite.x));
    sprite.h = std::clamp(sprite.h, 1, std::max(1, app.texture.height - sprite.y));

    if (ImGui::Button("Add To Animation")) {
        app.animation.frames.push_back(app.selectedSprite);
        appendLog(app, "Added animation frame: " + sprite.name);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Sprite")) {
        deleteSelectedSprite(app);
    }
    ImGui::End();
}

static void drawAnimationWindow(AppState& app)
{
    if (!app.showAnimation) {
        return;
    }
    ImGui::Begin("Animation", &app.showAnimation);
    if (ImGui::InputText("Name", app.animationNameBuffer.data(), app.animationNameBuffer.size())) {
        app.animation.name = app.animationNameBuffer.data();
    }
    ImGui::DragFloat("FPS", &app.animation.fps, 0.25f, 1.0f, 60.0f, "%.2f");
    ImGui::Checkbox("Loop", &app.animation.loop);
    if (ImGui::Checkbox("Preview On Canvas", &app.animation.previewOnCanvas)) {
        app.animation.previewOnCanvasLastFrame = false;
    }

    if (ImGui::Button(app.animation.playing ? "Pause" : "Play")) {
        app.animation.playing = !app.animation.playing;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        if (!app.animation.frames.empty()) {
            app.animation.currentFrame = (app.animation.currentFrame + 1) % static_cast<int>(app.animation.frames.size());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        app.animation.frames.clear();
        app.animation.currentFrame = 0;
    }

    ImGui::Separator();
    ImGui::Text("Frames: %d", static_cast<int>(app.animation.frames.size()));
    for (int i = 0; i < static_cast<int>(app.animation.frames.size()); ++i) {
        const int spriteIndex = app.animation.frames[static_cast<size_t>(i)];
        const char* name = (spriteIndex >= 0 && spriteIndex < static_cast<int>(app.sprites.size())) ? app.sprites[static_cast<size_t>(spriteIndex)].name.c_str() : "<missing>";
        ImGui::PushID(i);
        if (i == app.animation.currentFrame) {
            ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.2f, 1.0f), "%02d: %s", i, name);
        } else {
            ImGui::Text("%02d: %s", i, name);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Select")) {
            app.selectedSprite = spriteIndex;
            if (app.selectedSprite >= 0 && app.selectedSprite < static_cast<int>(app.sprites.size())) {
                setBuffer(app.renameBuffer, app.sprites[static_cast<size_t>(app.selectedSprite)].name);
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) {
            app.animation.frames.erase(app.animation.frames.begin() + i);
            app.animation.currentFrame = std::min(app.animation.currentFrame, std::max(0, static_cast<int>(app.animation.frames.size()) - 1));
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    ImGui::End();
}

static void drawExportWindow(AppState& app)
{
    if (!app.showExport) {
        return;
    }
    ImGui::Begin("Export", &app.showExport);
    ImGui::InputText("Export JSON Path", app.exportPath.data(), app.exportPath.size());
    if (ImGui::Button("Export Metadata JSON")) {
        exportMetadata(app, app.exportPath.data()[0] ? app.exportPath.data() : "sprites.metadata.json");
    }
    ImGui::TextWrapped("Metadata JSON contains image, size, sprites, and animation. It is the first-version game-facing format.");

    ImGui::Separator();
    ImGui::Text("Animation GIF");
    ImGui::Text("Frames: %d", static_cast<int>(app.animation.frames.size()));
    ImGui::Text("FPS: %.2f  Delay: %d cs", app.animation.fps, fpsToGifDelayCs(app.animation.fps));
    ImGui::InputText("GIF Path", app.animationGifPath.data(), app.animationGifPath.size());
    ImGui::Checkbox("Use Max Frame Size", &app.animationGif.useMaxFrameSize);
    ImGui::Checkbox("Center Frames", &app.animationGif.centerFrames);
    ImGui::InputInt("Background R", &app.animationGif.backgroundR);
    ImGui::InputInt("Background G", &app.animationGif.backgroundG);
    ImGui::InputInt("Background B", &app.animationGif.backgroundB);
    app.animationGif.backgroundR = std::clamp(app.animationGif.backgroundR, 0, 255);
    app.animationGif.backgroundG = std::clamp(app.animationGif.backgroundG, 0, 255);
    app.animationGif.backgroundB = std::clamp(app.animationGif.backgroundB, 0, 255);
    if (ImGui::Button("Export Animation GIF")) {
        exportCurrentAnimationGif(app, app.animationGifPath.data()[0] ? app.animationGifPath.data() : "animation.gif");
    }
    ImGui::TextDisabled("GIF uses a solid background because gif-h ignores RGBA alpha.");
    ImGui::End();
}

static void addImagesToSheetBuilder(AppState& app, const std::vector<std::string>& paths)
{
    int added = 0;
    for (const std::string& path : paths) {
        std::string error;
        std::optional<SourceImage> image = loadSourceImage(path, error);
        if (!image.has_value()) {
            appendLog(app, "Sheet Builder skipped PNG: " + path + " (" + error + ")");
            continue;
        }
        app.sheetBuilder.images.push_back(std::move(*image));
        ++added;
    }
    if (added > 0) {
        app.sheetBuilder.selectedIndex = static_cast<int>(app.sheetBuilder.images.size()) - 1;
        appendLog(app, "Sheet Builder added " + std::to_string(added) + " PNG image(s).");
    }
}

static void browseAndAddSheetBuilderImages(AppState& app)
{
    if (!nativeFileDialogsAvailable()) {
        appendLog(app, "Native file dialogs are currently only available on Windows.");
        return;
    }

    const std::vector<std::string> paths = openNativePngFilesDialog(app, app.imagePath.data());
    addImagesToSheetBuilder(app, paths);
}

static bool usePackedSheetAsCurrentCanvas(AppState& app, const PackedSheet& sheet, bool preserveAnimation = false)
{
    if (!sheet.valid()) {
        appendLog(app, "Sheet Builder failed: no valid packed sheet to use.");
        return false;
    }

    const std::string generatedPath = "generated_sheet.png";
    if (!uploadTextureFromPixels(app.texture, sheet.pixels, sheet.width, sheet.height, generatedPath)) {
        appendLog(app, "Sheet Builder failed: could not upload generated sheet texture.");
        return false;
    }

    const AnimationState preservedAnimation = app.animation;
    app.sprites = sheet.sprites;
    resetEditedSheetState(app);
    if (preserveAnimation) {
        app.animation = preservedAnimation;
        app.animation.currentFrame = std::clamp(app.animation.currentFrame, 0, std::max(0, static_cast<int>(app.animation.frames.size()) - 1));
    }
    app.packingPreviewRects = sheet.packingRects;
    setBuffer(app.imagePath, generatedPath);
    app.showSprites = true;
    appendLog(app, "Generated sheet is now the current canvas: " + std::to_string(sheet.width) + "x" + std::to_string(sheet.height) + ", " + std::to_string(app.sprites.size()) + " sprites.");
    return true;
}

static bool exportPackedSheetPng(AppState& app, const PackedSheet& sheet, const std::string& path)
{
    if (!sheet.valid()) {
        appendLog(app, "Sheet PNG export failed: no valid packed sheet.");
        return false;
    }

    const fs::path outputPath = path.empty() ? fs::path("generated_sheet.png") : fs::path(path);
    if (outputPath.has_parent_path()) {
        std::error_code error;
        fs::create_directories(outputPath.parent_path(), error);
        if (error) {
            appendLog(app, "Sheet PNG export failed: cannot create directory " + outputPath.parent_path().string());
            return false;
        }
    }

    if (stbi_write_png(outputPath.string().c_str(), sheet.width, sheet.height, 4, sheet.pixels.data(), sheet.width * 4) == 0) {
        appendLog(app, "Sheet PNG export failed: cannot write " + outputPath.string());
        return false;
    }

    setBuffer(app.sheetBuilder.exportPath, outputPath.string());
    appendLog(app, "Exported generated sheet PNG: " + outputPath.string());
    return true;
}

static void drawSheetBuilderWindow(AppState& app)
{
    if (!app.showSheetBuilder) {
        return;
    }

    ImGui::Begin("Sprite Sheet Builder", &app.showSheetBuilder);
    ImGui::TextWrapped("Build one editable sprite sheet from multiple PNG files. The current MVP uses a fixed-column transparent grid layout.");

    if (ImGui::Button("Add Images...")) {
        browseAndAddSheetBuilderImages(app);
    }
    ImGui::SameLine();
    const bool hasSelection = app.sheetBuilder.selectedIndex >= 0 && app.sheetBuilder.selectedIndex < static_cast<int>(app.sheetBuilder.images.size());
    if (!hasSelection) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Remove Selected")) {
        app.sheetBuilder.images.erase(app.sheetBuilder.images.begin() + app.sheetBuilder.selectedIndex);
        app.sheetBuilder.selectedIndex = app.sheetBuilder.images.empty() ? -1 : std::clamp(app.sheetBuilder.selectedIndex, 0, static_cast<int>(app.sheetBuilder.images.size()) - 1);
    }
    if (!hasSelection) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        app.sheetBuilder.images.clear();
        app.sheetBuilder.selectedIndex = -1;
    }

    ImGui::Separator();
    ImGui::Text("Input Images: %d", static_cast<int>(app.sheetBuilder.images.size()));
    ImGui::BeginChild("SheetBuilderImages", ImVec2(0.0f, 150.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
    for (int i = 0; i < static_cast<int>(app.sheetBuilder.images.size()); ++i) {
        const SourceImage& image = app.sheetBuilder.images[static_cast<size_t>(i)];
        const std::string label = std::to_string(i) + ": " + image.name + "  " + std::to_string(image.width) + "x" + std::to_string(image.height);
        if (ImGui::Selectable(label.c_str(), app.sheetBuilder.selectedIndex == i)) {
            app.sheetBuilder.selectedIndex = i;
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::Text("Packing");
    const char* algorithmLabels[] = {"Fixed Columns", "Auto Grid", "Shelf", "Skyline", "Guillotine", "MaxRects"};
    int algorithmIndex = 0;
    if (app.sheetBuilder.algorithm == PackingAlgorithm::AutoGrid) {
        algorithmIndex = 1;
    } else if (app.sheetBuilder.algorithm == PackingAlgorithm::Shelf) {
        algorithmIndex = 2;
    } else if (app.sheetBuilder.algorithm == PackingAlgorithm::Skyline) {
        algorithmIndex = 3;
    } else if (app.sheetBuilder.algorithm == PackingAlgorithm::Guillotine) {
        algorithmIndex = 4;
    } else if (app.sheetBuilder.algorithm == PackingAlgorithm::MaxRects) {
        algorithmIndex = 5;
    }
    if (ImGui::Combo("Algorithm", &algorithmIndex, algorithmLabels, static_cast<int>(std::size(algorithmLabels)))) {
        app.sheetBuilder.algorithm = algorithmIndex == 5 ? PackingAlgorithm::MaxRects : (algorithmIndex == 4 ? PackingAlgorithm::Guillotine : (algorithmIndex == 3 ? PackingAlgorithm::Skyline : (algorithmIndex == 2 ? PackingAlgorithm::Shelf : (algorithmIndex == 1 ? PackingAlgorithm::AutoGrid : PackingAlgorithm::FixedColumns))));
    }

    ImGui::InputInt("Padding", &app.sheetBuilder.padding);
    ImGui::InputInt("Margin", &app.sheetBuilder.margin);
    ImGui::InputInt("Extrude Border", &app.sheetBuilder.extrudeBorder);
    ImGui::TextDisabled("Extrude duplicates edge pixels around each sprite without changing SpriteRect size.");
    if (app.sheetBuilder.algorithm == PackingAlgorithm::FixedColumns) {
        ImGui::InputInt("Columns", &app.sheetBuilder.columns);
    } else if (app.sheetBuilder.algorithm == PackingAlgorithm::AutoGrid) {
        ImGui::TextDisabled("Auto Grid chooses the column count whose output size is closest to square.");
    } else {
        const char* variablePackingHelp = "Shelf packs each row left-to-right and starts a new row when Max Width would be exceeded.";
        if (app.sheetBuilder.algorithm == PackingAlgorithm::Skyline) {
            variablePackingHelp = "Skyline places each sprite at the lowest fitting skyline position.";
        } else if (app.sheetBuilder.algorithm == PackingAlgorithm::Guillotine) {
            variablePackingHelp = "Guillotine places each sprite into the best free rectangle, then cuts the leftover space.";
        } else if (app.sheetBuilder.algorithm == PackingAlgorithm::MaxRects) {
            variablePackingHelp = "MaxRects places each sprite into the best maximal free rectangle and splits all overlaps.";
        }
        ImGui::TextDisabled(variablePackingHelp);
        ImGui::InputInt("Max Width", &app.sheetBuilder.maxSheetWidth);
        const char* sortLabels[] = {"Original Order", "Height Desc", "Area Desc"};
        int sortIndex = 0;
        if (app.sheetBuilder.sortMode == PackingSortMode::HeightDesc) {
            sortIndex = 1;
        } else if (app.sheetBuilder.sortMode == PackingSortMode::AreaDesc) {
            sortIndex = 2;
        }
        if (ImGui::Combo("Sort", &sortIndex, sortLabels, static_cast<int>(std::size(sortLabels)))) {
            app.sheetBuilder.sortMode = sortIndex == 2 ? PackingSortMode::AreaDesc : (sortIndex == 1 ? PackingSortMode::HeightDesc : PackingSortMode::Original);
        }
    }
    if (app.sheetBuilder.algorithm != PackingAlgorithm::Shelf && app.sheetBuilder.algorithm != PackingAlgorithm::Skyline && app.sheetBuilder.algorithm != PackingAlgorithm::Guillotine && app.sheetBuilder.algorithm != PackingAlgorithm::MaxRects) {
        ImGui::Checkbox("Use Custom Cell Size", &app.sheetBuilder.useCustomCellSize);
        if (app.sheetBuilder.useCustomCellSize) {
            ImGui::InputInt("Cell Width", &app.sheetBuilder.customCellWidth);
            ImGui::InputInt("Cell Height", &app.sheetBuilder.customCellHeight);
        } else {
            ImGui::TextDisabled("Cell Size: max source image size (%dx%d)", maxSourceImageWidth(app.sheetBuilder.images), maxSourceImageHeight(app.sheetBuilder.images));
        }
    } else {
        ImGui::TextDisabled("This algorithm uses each source image's actual size instead of uniform cells.");
    }

    app.sheetBuilder.columns = std::max(1, app.sheetBuilder.columns);
    app.sheetBuilder.padding = std::max(0, app.sheetBuilder.padding);
    app.sheetBuilder.margin = std::max(0, app.sheetBuilder.margin);
    app.sheetBuilder.customCellWidth = std::max(1, app.sheetBuilder.customCellWidth);
    app.sheetBuilder.customCellHeight = std::max(1, app.sheetBuilder.customCellHeight);
    app.sheetBuilder.extrudeBorder = std::clamp(app.sheetBuilder.extrudeBorder, 0, 64);
    app.sheetBuilder.maxSheetWidth = std::max(1, app.sheetBuilder.maxSheetWidth);

    const PackedSheet previewSheet = buildSpriteSheet(app.sheetBuilder);
    ImGui::Separator();
    ImGui::Text("Output");
    ImGui::Checkbox("Show Debug Packing Rects", &app.canvas.showPackingRects);
    if (previewSheet.valid()) {
        ImGui::Text("Sheet Size: %d x %d", previewSheet.width, previewSheet.height);
        ImGui::Text("Sprites: %d", static_cast<int>(previewSheet.sprites.size()));
        if (!app.sheetBuilder.images.empty()) {
            const int firstY = previewSheet.sprites.empty() ? 0 : previewSheet.sprites.front().y;
            int computedColumns = 0;
            for (const SpriteRect& sprite : previewSheet.sprites) {
                if (sprite.y == firstY) {
                    ++computedColumns;
                }
            }
            const int computedRows = computedColumns > 0 ? (static_cast<int>(previewSheet.sprites.size()) + computedColumns - 1) / computedColumns : 0;
            ImGui::Text("Layout: %d columns x %d rows", computedColumns, computedRows);
        }
    } else {
        ImGui::TextDisabled("Sheet Size: none");
        ImGui::TextDisabled("Sprites: 0");
    }

    if (!previewSheet.valid()) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Use As Current Canvas")) {
        usePackedSheetAsCurrentCanvas(app, previewSheet);
    }
    ImGui::InputText("Sheet PNG Path", app.sheetBuilder.exportPath.data(), app.sheetBuilder.exportPath.size());
    ImGui::SameLine();
    if (ImGui::Button("Export Sheet PNG")) {
        exportPackedSheetPng(app, previewSheet, app.sheetBuilder.exportPath.data()[0] ? app.sheetBuilder.exportPath.data() : "generated_sheet.png");
    }
    if (!previewSheet.valid()) {
        ImGui::EndDisabled();
    }

    ImGui::End();
}

static ImVec2 layoutToScreen(const SpriteLayoutState& state, const ImVec2& origin, int x, int y)
{
    return ImVec2(origin.x + state.pan.x + static_cast<float>(x) * state.zoom, origin.y + state.pan.y + static_cast<float>(y) * state.zoom);
}

static int hitTestLayoutItem(const SpriteLayoutState& state, const ImVec2& origin, const ImVec2& mouse)
{
    for (int i = static_cast<int>(state.layout.items.size()) - 1; i >= 0; --i) {
        const SpriteLayoutItem& item = state.layout.items[static_cast<size_t>(i)];
        const ImVec2 min = layoutToScreen(state, origin, item.x, item.y);
        const ImVec2 max = layoutToScreen(state, origin, item.x + item.w, item.y + item.h);
        if (mouse.x >= min.x && mouse.x <= max.x && mouse.y >= min.y && mouse.y <= max.y) {
            return i;
        }
    }
    return -1;
}

static void drawSpriteLayoutPreview(AppState& app, const ImVec2& previewSize)
{
    SpriteLayoutState& state = app.spriteLayout;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 max = ImVec2(origin.x + previewSize.x, origin.y + previewSize.y);
    ImGui::InvisibleButton("SpriteLayoutPreview", previewSize, ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImVec2 mouse = ImGui::GetIO().MousePos;

    drawList->AddRectFilled(origin, max, IM_COL32(28, 28, 32, 255));
    drawList->AddRect(origin, max, IM_COL32(95, 95, 105, 255));
    if (!state.layout.valid()) {
        drawList->AddText(ImVec2(origin.x + 12.0f, origin.y + 12.0f), IM_COL32(220, 220, 220, 255), "Build a layout from current sprites first.");
        return;
    }

    const ImVec2 atlasMin = layoutToScreen(state, origin, 0, 0);
    const ImVec2 atlasMax = layoutToScreen(state, origin, state.layout.atlasWidth, state.layout.atlasHeight);
    drawList->AddRectFilled(atlasMin, atlasMax, IM_COL32(12, 12, 16, 255));
    drawList->AddRect(atlasMin, atlasMax, IM_COL32(180, 180, 190, 255));

    if (state.settings.snapToGrid && state.settings.gridSize > 1) {
        const float gridStep = static_cast<float>(state.settings.gridSize) * state.zoom;
        if (gridStep >= 4.0f) {
            for (float x = atlasMin.x; x <= atlasMax.x; x += gridStep) {
                drawList->AddLine(ImVec2(x, atlasMin.y), ImVec2(x, atlasMax.y), IM_COL32(70, 70, 78, 120));
            }
            for (float y = atlasMin.y; y <= atlasMax.y; y += gridStep) {
                drawList->AddLine(ImVec2(atlasMin.x, y), ImVec2(atlasMax.x, y), IM_COL32(70, 70, 78, 120));
            }
        }
    }

    for (int i = 0; i < static_cast<int>(state.layout.items.size()); ++i) {
        const SpriteLayoutItem& item = state.layout.items[static_cast<size_t>(i)];
        const bool draggingThis = state.dragIndex == i;
        const int drawX = draggingThis ? state.dragCandidateX : item.x;
        const int drawY = draggingThis ? state.dragCandidateY : item.y;
        const ImVec2 itemMin = layoutToScreen(state, origin, drawX, drawY);
        const ImVec2 itemMax = layoutToScreen(state, origin, drawX + item.w, drawY + item.h);
        ImU32 color = IM_COL32(80, 170, 255, 230);
        if (state.selectedIndex == i) {
            color = IM_COL32(255, 215, 70, 255);
        }
        if (draggingThis && !state.dragValidation.valid) {
            color = IM_COL32(255, 70, 70, 255);
        }
        drawList->AddRect(itemMin, itemMax, color, 0.0f, 0, 2.0f);
        drawList->AddText(ImVec2(itemMin.x + 3.0f, itemMin.y + 3.0f), color, item.name.c_str());
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const int hit = hitTestLayoutItem(state, origin, mouse);
        state.selectedIndex = hit;
        if (hit >= 0) {
            const SpriteLayoutItem& item = state.layout.items[static_cast<size_t>(hit)];
            state.dragIndex = hit;
            state.dragStartX = item.x;
            state.dragStartY = item.y;
            state.dragCandidateX = item.x;
            state.dragCandidateY = item.y;
            state.dragStartMouse = mouse;
            state.dragValidation = SpriteLayoutValidation{};
        }
    }

    if (state.dragIndex >= 0 && active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const int candidateX = state.dragStartX + static_cast<int>(std::lround((mouse.x - state.dragStartMouse.x) / state.zoom));
        const int candidateY = state.dragStartY + static_cast<int>(std::lround((mouse.y - state.dragStartMouse.y) / state.zoom));
        state.dragCandidateX = state.settings.snapToGrid ? snapCoordinate(candidateX, state.settings.gridSize) : candidateX;
        state.dragCandidateY = state.settings.snapToGrid ? snapCoordinate(candidateY, state.settings.gridSize) : candidateY;
        state.dragValidation = validateLayoutItemMove(state.layout, state.dragIndex, state.dragCandidateX, state.dragCandidateY, state.settings);
    }

    if (state.dragIndex >= 0 && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        SpriteLayoutValidation validation;
        if (!moveLayoutItem(state.layout, state.dragIndex, state.dragCandidateX, state.dragCandidateY, state.settings, &validation)) {
            appendLog(app, "Sprite Layout move rejected: overlap or out of bounds.");
        }
        state.dragValidation = validation;
        state.dragIndex = -1;
    }
}

static void drawSpriteLayoutWindow(AppState& app)
{
    if (!app.showSpriteLayout) {
        return;
    }

    ImGui::Begin("Sprite Layout", &app.showSpriteLayout);
    ImGui::Text("Atlas: current");
    ImGui::Text("Size: %d x %d", app.texture.width, app.texture.height);
    ImGui::Text("Sprites: %d", static_cast<int>(app.sprites.size()));

    if (ImGui::Button("Build Layout From Current Sprites")) {
        app.spriteLayout.layout = createSpriteLayoutFromSprites(app.texture.width, app.texture.height, app.texture.pixels, app.sprites, app.colorKey);
        app.spriteLayout.selectedIndex = app.spriteLayout.layout.items.empty() ? -1 : 0;
        app.spriteLayout.dragIndex = -1;
        appendLog(app, "Built sprite layout: " + std::to_string(app.spriteLayout.layout.items.size()) + " items.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Layout")) {
        app.spriteLayout.layout = {};
        app.spriteLayout.selectedIndex = -1;
        app.spriteLayout.dragIndex = -1;
    }

    ImGui::Checkbox("Snap To Grid", &app.spriteLayout.settings.snapToGrid);
    ImGui::InputInt("Grid Size", &app.spriteLayout.settings.gridSize);
    app.spriteLayout.settings.gridSize = std::max(1, app.spriteLayout.settings.gridSize);
    ImGui::DragFloat("Preview Zoom", &app.spriteLayout.zoom, 0.05f, 0.25f, 16.0f, "%.2f");
    app.spriteLayout.zoom = std::clamp(app.spriteLayout.zoom, 0.25f, 16.0f);

    if (ImGui::Button("Auto Repack")) {
        SpriteLayout repacked = autoRepackLayout(app.spriteLayout.layout, sheetPackingSettingsFromBuilder(app.sheetBuilder));
        if (repacked.valid()) {
            app.spriteLayout.layout = std::move(repacked);
            app.spriteLayout.selectedIndex = app.spriteLayout.layout.items.empty() ? -1 : 0;
            appendLog(app, "Sprite Layout auto repack complete.");
        } else {
            appendLog(app, "Sprite Layout auto repack failed.");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply Layout To Current Canvas")) {
        const PackedSheet sheet = buildSheetFromLayout(app.spriteLayout.layout);
        usePackedSheetAsCurrentCanvas(app, sheet, true);
    }

    ImGui::Separator();
    const SpriteLayoutState& state = app.spriteLayout;
    if (state.selectedIndex >= 0 && state.selectedIndex < static_cast<int>(state.layout.items.size())) {
        const SpriteLayoutItem& selected = state.layout.items[static_cast<size_t>(state.selectedIndex)];
        ImGui::Text("Selected: %d %s (%d,%d %dx%d)", state.selectedIndex, selected.name.c_str(), selected.x, selected.y, selected.w, selected.h);
    } else {
        ImGui::TextDisabled("Selected: none");
    }
    if (state.dragIndex >= 0 && !state.dragValidation.valid) {
        if (state.dragValidation.outOfBounds) {
            ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Warning: out of atlas bounds");
        }
        if (state.dragValidation.overlaps) {
            ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Warning: overlaps %d sprite(s)", static_cast<int>(state.dragValidation.overlappingItems.size()));
        }
    } else {
        ImGui::Text("Status: Valid");
    }

    ImGui::Separator();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    drawSpriteLayoutPreview(app, ImVec2(std::max(320.0f, available.x), 420.0f));
    ImGui::End();
}

static void drawLogWindow(AppState& app)
{
    if (!app.showLog) {
        return;
    }
    ImGui::Begin("Log", &app.showLog);
    if (ImGui::Button("Clear Log")) {
        app.logs.clear();
    }
    ImGui::Separator();
    for (const std::string& line : app.logs) {
        ImGui::TextWrapped("%s", line.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::End();
}

static void setupStyle()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
}

static void glfwErrorCallback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW Error %d: %s", error, description);
    std::fputc(10, stderr);
}

static void enableDpiAwareness()
{
#ifdef _WIN32
    HMODULE user32 = LoadLibraryA("user32.dll");
    if (user32 != nullptr) {
        using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto setProcessDpiAwarenessContext = reinterpret_cast<SetProcessDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setProcessDpiAwarenessContext != nullptr) {
            if (setProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != 0) {
                FreeLibrary(user32);
                return;
            }
            if (setProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE) != 0) {
                FreeLibrary(user32);
                return;
            }
        }

        using SetProcessDPIAwareFn = BOOL(WINAPI*)();
        auto setProcessDPIAware = reinterpret_cast<SetProcessDPIAwareFn>(GetProcAddress(user32, "SetProcessDPIAware"));
        if (setProcessDPIAware != nullptr) {
            setProcessDPIAware();
        }
        FreeLibrary(user32);
    }
#endif
}

int main(int, char**)
{
    enableDpiAwareness();
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        return 1;
    }

    const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#ifdef GLFW_SCALE_FRAMEBUFFER
    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1600, 960, "SpriteEd - Dear ImGui Sprite Sheet Workbench", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    setupStyle();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    AppState app;
    setBuffer(app.projectPath, "project.sprited.json");
    setBuffer(app.exportPath, "sprites.metadata.json");
    setBuffer(app.sheetBuilder.exportPath, "generated_sheet.png");
    setBuffer(app.animationNameBuffer, app.animation.name);
    appendLog(app, "SpriteEd started. Goal: center canvas plus floating controls.");

    auto lastTime = std::chrono::steady_clock::now();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const auto now = std::chrono::steady_clock::now();
        const double deltaSeconds = std::chrono::duration<double>(now - lastTime).count();
        lastTime = now;
        updateAnimation(app.animation, deltaSeconds);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        updateCanvasFramebufferScale(app, window);
        g_rawImageDrawCommands.clear();

        if (openPngShortcutPressed(io)) {
            browseAndLoadPng(app);
        }

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open PNG...", "Ctrl+O")) {
                    browseAndLoadPng(app);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Windows")) {
                ImGui::MenuItem("Project", nullptr, &app.showProject);
                ImGui::MenuItem("Sprite Extractor", nullptr, &app.showExtractor);
                ImGui::MenuItem("Sprites", nullptr, &app.showSprites);
                ImGui::MenuItem("Sprite Inspector", nullptr, &app.showInspector);
                ImGui::MenuItem("Animation", nullptr, &app.showAnimation);
                ImGui::MenuItem("Export", nullptr, &app.showExport);
                ImGui::MenuItem("Sprite Sheet Builder", nullptr, &app.showSheetBuilder);
                ImGui::MenuItem("Sprite Layout", nullptr, &app.showSpriteLayout);
                ImGui::MenuItem("Log", nullptr, &app.showLog);
                ImGui::MenuItem("ImGui Demo", nullptr, &app.showDemo);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Canvas")) {
                if (ImGui::MenuItem("100% Zoom")) {
                    app.canvas.zoom = 1.0f;
                    app.canvas.zoomLevelIndex = nearestZoomLevelIndex(app, app.canvas.zoom);
                }
                ImGui::MenuItem("Checker", nullptr, &app.canvas.showChecker);
                ImGui::MenuItem("Pixel Perfect", nullptr, &app.canvas.pixelPerfect);
                ImGui::MenuItem("Integer Zoom", nullptr, &app.canvas.integerZoom);
                ImGui::MenuItem("Show Info Overlay", nullptr, &app.canvas.showInfoOverlay);
                ImGui::MenuItem("Show Packing Rects", nullptr, &app.canvas.showPackingRects);
                ImGui::EndMenu();
            }
            const std::string pngFilename = currentPngFilename(app);
            ImGui::TextDisabled("  PNG: %s | LMB: select/create rect | MMB or Space+LMB: pan | Wheel: zoom | Ctrl+O: open PNG", pngFilename.c_str());
            ImGui::EndMainMenuBar();
        }

        drawCanvasWindow(app);
        drawProjectWindow(app);
        drawExtractorWindow(app);
        drawSpritesWindow(app);
        drawInspectorWindow(app);
        drawAnimationWindow(app);
        drawExportWindow(app);
        drawSheetBuilderWindow(app);
        drawSpriteLayoutWindow(app);
        drawLogWindow(app);
        if (app.showDemo) {
            ImGui::ShowDemoWindow(&app.showDemo);
        }

        ImGui::Render();
        int displayW = 0;
        int displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    app.texture.release();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
