// Font.ixx - TrueType fonts baked to a signed distance field atlas (layout math, no graphics API)
export module LB.RenderingSystem:Font;

import std;
import :BitmapFont;
import :Model;

export namespace LB::Systems::Rendering {

    /// <summary>
    /// Handle to a font loaded by RenderingSystem::LoadFont (0: the built-in pixel font)
    /// </summary>
    using FontId = std::uint32_t;
    inline constexpr FontId PixelFontId = 0;

    /// <summary>
    /// Where one character's quad goes relative to the pen (on the baseline) and where it sits in
    /// the atlas, in pixels at the bake size
    /// </summary>
    struct FontGlyph {
        float atlasX{0.0f};
        float atlasY{0.0f};
        float width{0.0f};
        float height{0.0f};
        float offsetX{0.0f};      // quad's left edge from the pen
        float offsetY{0.0f};      // quad's top edge from the baseline (negative: above it)
        float advance{0.0f};      // pen movement after the character
    };

    /// <summary>
    /// A TrueType font baked at one pixel size into a signed distance field (alpha: 0.5 on the
    /// outline, rising inside). Text is laid out at any size by scaling the baked metrics; the
    /// distance field keeps edges sharp and makes outlines cheap.
    /// </summary>
    struct FontData {
        std::string name;                         // file name without extension
        float bakeSize{48.0f};                    // pixel height the glyphs were baked at
        float capHeight{0.0f};                    // height of 'H' at the bake size
        float ascent{0.0f};
        float descent{0.0f};                      // negative: below the baseline
        float spread{6.0f};                       // distance field reach, in bake pixels
        std::map<char32_t, FontGlyph> glyphs;
        std::map<std::pair<char32_t, char32_t>, float> kerning;   // extra advance between two characters
        ModelTexture atlas;                       // one level, distance in alpha

        [[nodiscard]] const FontGlyph* Find(char32_t c) const noexcept {
            const auto it = glyphs.find(c);
            if (it != glyphs.end()) return &it->second;
            const auto fallback = glyphs.find(U'?');
            return fallback != glyphs.end() ? &fallback->second : nullptr;
        }

        [[nodiscard]] float Kerning(char32_t left, char32_t right) const noexcept {
            const auto it = kerning.find({ left, right });
            return it != kerning.end() ? it->second : 0.0f;
        }

        /// <summary>
        /// Factor from bake pixels to screen pixels for text whose capitals are `capitalHeight` tall
        /// </summary>
        [[nodiscard]] float ScaleFor(float capitalHeight) const noexcept {
            return capHeight > 0.0f ? capitalHeight / capHeight : capitalHeight / bakeSize;
        }
    };

    /// <summary>
    /// Decode UTF-8 to code points (invalid bytes become '?')
    /// </summary>
    [[nodiscard]] inline std::u32string DecodeUtf8(std::string_view text) {
        std::u32string out;
        out.reserve(text.size());
        for (std::size_t i = 0; i < text.size();) {
            const auto byte = static_cast<unsigned char>(text[i]);
            const int length = byte < 0x80 ? 1 : (byte >> 5) == 0x6 ? 2 : (byte >> 4) == 0xE ? 3 : (byte >> 3) == 0x1E ? 4 : 0;
            if (length == 0 || i + static_cast<std::size_t>(length) > text.size()) {
                out.push_back(U'?');
                ++i;
                continue;
            }
            char32_t c = length == 1 ? byte : byte & (0x7F >> length);
            bool valid = true;
            for (int k = 1; k < length; ++k) {
                const auto next = static_cast<unsigned char>(text[i + static_cast<std::size_t>(k)]);
                if ((next & 0xC0) != 0x80) { valid = false; break; }
                c = (c << 6) | (next & 0x3F);
            }
            out.push_back(valid ? c : U'?');
            i += valid ? static_cast<std::size_t>(length) : 1;
        }
        return out;
    }

    /// <summary>
    /// Size of a line of text: width, and the capital height it was asked for (the font's
    /// capitals fill it; descenders hang below)
    /// </summary>
    [[nodiscard]] inline TextSize MeasureLine(const FontData& font, std::string_view text, float capitalHeight) {
        const auto codepoints = DecodeUtf8(text);
        const float scale = font.ScaleFor(capitalHeight);
        float width = 0.0f;
        for (std::size_t i = 0; i < codepoints.size(); ++i) {
            if (const auto* glyph = font.Find(codepoints[i])) width += glyph->advance;
            if (i + 1 < codepoints.size()) width += font.Kerning(codepoints[i], codepoints[i + 1]);
        }
        return { width * scale, codepoints.empty() ? 0.0f : capitalHeight };
    }

} // namespace LB::Systems::Rendering
