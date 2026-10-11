// FontLoader.cpp - TrueType font baking with stb_truetype
//
// Implementation unit of LB.RenderingSystem (declared in the interface as Detail::LoadTrueTypeFont):
// stb_truetype stays in this file's global module fragment. Bakes printable ASCII and Latin-1
// (accented letters) to signed distance field glyphs packed into one atlas, and reads the
// metrics and kerning text layout needs.
module;

#pragma warning(push, 0)
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <stb_truetype.h>
#pragma warning(pop)

#include <algorithm>
#include <memory>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

module LB.RenderingSystem;

namespace LB::Systems::Rendering::Detail {

    namespace {
        // Characters baked: printable ASCII and the Latin-1 supplement
        std::vector<char32_t> BakedCharacters() {
            std::vector<char32_t> characters;
            for (char32_t c = 32; c < 127; ++c) characters.push_back(c);
            for (char32_t c = 160; c < 256; ++c) characters.push_back(c);
            return characters;
        }

        struct BakedGlyph {
            char32_t character{};
            int width{0};
            int height{0};
            int offsetX{0};
            int offsetY{0};
            std::vector<std::uint8_t> distance;   // width * height, 0.5 * 255 on the outline
        };
    }

    TrueTypeLoadResult LoadTrueTypeFont(const std::filesystem::path& path, float bakeSize) {
        TrueTypeLoadResult result;
        const auto fail = [&](std::string_view reason) {
            result.error = std::format("{}: {}", path.filename().string(), reason);
            return std::move(result);
        };

        std::ifstream in(path, std::ios::binary);
        if (!in) return fail("file not found");
        const std::vector<unsigned char> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        stbtt_fontinfo info{};
        const int offset = stbtt_GetFontOffsetForIndex(file.data(), 0);
        if (offset < 0 || !stbtt_InitFont(&info, file.data(), offset)) return fail("not a TrueType font");

        auto& font = result.font;
        font.name = path.stem().string();
        font.bakeSize = bakeSize;
        font.spread = std::max(4.0f, std::round(bakeSize / 8.0f));
        const float scale = stbtt_ScaleForPixelHeight(&info, bakeSize);

        int ascent = 0, descent = 0, lineGap = 0;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        font.ascent = static_cast<float>(ascent) * scale;
        font.descent = static_cast<float>(descent) * scale;

        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        if (stbtt_GetCodepointBox(&info, 'H', &x0, &y0, &x1, &y1)) {
            font.capHeight = static_cast<float>(y1) * scale;
        }

        // Distance fields: 128 on the outline, one step per (128 / spread) of a bake pixel
        const auto characters = BakedCharacters();
        const int padding = static_cast<int>(font.spread);
        std::vector<BakedGlyph> baked;
        for (const char32_t c : characters) {
            if (stbtt_FindGlyphIndex(&info, static_cast<int>(c)) == 0 && c != U' ') continue;   // not in the font

            int advance = 0, bearing = 0;
            stbtt_GetCodepointHMetrics(&info, static_cast<int>(c), &advance, &bearing);
            FontGlyph glyph{};
            glyph.advance = static_cast<float>(advance) * scale;

            BakedGlyph bitmap{ c };
            // Owned from the start: freed even if copying it out throws
            const auto freeSdf = [](unsigned char* bitmap) { stbtt_FreeSDF(bitmap, nullptr); };
            const std::unique_ptr<unsigned char, decltype(freeSdf)> sdf(
                stbtt_GetCodepointSDF(&info, scale, static_cast<int>(c), padding, 128, 128.0f / static_cast<float>(padding),
                                      &bitmap.width, &bitmap.height, &bitmap.offsetX, &bitmap.offsetY),
                freeSdf);
            if (sdf) {
                bitmap.distance.assign(sdf.get(), sdf.get() + static_cast<std::size_t>(bitmap.width * bitmap.height));
            }
            font.glyphs[c] = glyph;
            if (!bitmap.distance.empty()) baked.push_back(std::move(bitmap));
        }
        if (font.glyphs.empty()) return fail("the font has none of the baked characters");

        // Shelf packing, tallest first, into a 512-pixel-wide atlas
        std::ranges::sort(baked, [](const BakedGlyph& a, const BakedGlyph& b) { return a.height > b.height; });
        constexpr int atlasWidth = 512;
        int penX = 1, penY = 1, shelf = 0;
        std::vector<std::pair<int, int>> places;
        for (const auto& bitmap : baked) {
            if (penX + bitmap.width + 1 > atlasWidth) {
                penX = 1;
                penY += shelf + 1;
                shelf = 0;
            }
            places.emplace_back(penX, penY);
            penX += bitmap.width + 1;
            shelf = std::max(shelf, bitmap.height);
        }
        const int atlasHeight = std::bit_ceil(static_cast<unsigned>(penY + shelf + 1));

        ModelTexture::MipLevel level;
        level.width = atlasWidth;
        level.height = static_cast<std::uint32_t>(atlasHeight);
        level.pixels.assign(static_cast<std::size_t>(atlasWidth * atlasHeight * 4), 0);
        for (std::size_t i = 0; i < baked.size(); ++i) {
            const auto& bitmap = baked[i];
            const auto [px, py] = places[i];
            for (int y = 0; y < bitmap.height; ++y) {
                for (int x = 0; x < bitmap.width; ++x) {
                    auto* p = &level.pixels[static_cast<std::size_t>(((py + y) * atlasWidth + px + x) * 4)];
                    p[0] = p[1] = p[2] = 255;
                    p[3] = bitmap.distance[static_cast<std::size_t>(y * bitmap.width + x)];
                }
            }

            auto& glyph = font.glyphs[bitmap.character];
            glyph.atlasX = static_cast<float>(px);
            glyph.atlasY = static_cast<float>(py);
            glyph.width = static_cast<float>(bitmap.width);
            glyph.height = static_cast<float>(bitmap.height);
            glyph.offsetX = static_cast<float>(bitmap.offsetX);
            glyph.offsetY = static_cast<float>(bitmap.offsetY);
        }
        font.atlas.mips.push_back(std::move(level));

        // Kerning between every pair of baked characters the font adjusts
        if (info.kern || info.gpos) {
            for (const auto& [left, unusedLeft] : font.glyphs) {
                for (const auto& [right, unusedRight] : font.glyphs) {
                    const int kern = stbtt_GetCodepointKernAdvance(&info, static_cast<int>(left), static_cast<int>(right));
                    if (kern != 0) font.kerning[{ left, right }] = static_cast<float>(kern) * scale;
                }
            }
        }
        return result;
    }

} // namespace LB::Systems::Rendering::Detail
