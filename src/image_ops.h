#pragma once

#include "sprite_types.h"

#include <string>
#include <vector>

bool pixelMatchesColorKey(const unsigned char* pixel, const ColorKeySettings& colorKey);
unsigned char effectivePixelAlpha(const unsigned char* pixel, const ColorKeySettings& colorKey);
bool spriteRectHasVisiblePixel(int imageWidth, int imageHeight, const std::vector<unsigned char>& pixels, const SpriteRect& sprite, int alphaThreshold, const ColorKeySettings& colorKey);
std::string sanitizedSpriteFilename(const std::string& name);
std::vector<unsigned char> cropSpritePixels(int imageWidth, int imageHeight, const std::vector<unsigned char>& sourcePixels, const SpriteRect& sprite, int& outW, int& outH, const ColorKeySettings& colorKey);
