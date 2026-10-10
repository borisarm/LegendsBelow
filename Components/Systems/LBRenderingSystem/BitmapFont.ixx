// BitmapFont.ixx - Built-in 5x7 pixel font for overlay text (damage numbers, HUD labels)
//
// Pure data and layout math, no graphics API: the renderer bakes the glyphs into an atlas texture
// and draws each glyph as a quad, scaled by whole pixels so the font stays crisp.
export module LB.RenderingSystem:BitmapFont;

import std;

export namespace LB::Systems::Rendering {

    /// <summary>
    /// Size of a run of text in pixels
    /// </summary>
    struct TextSize {
        float width{0.0f};
        float height{0.0f};
    };

    /// <summary>
    /// A fixed-width 5x7 pixel font covering ASCII 32-95: space, digits, capital letters, and common
    /// punctuation. Lower case prints as upper case; other characters print as '?'.
    /// </summary>
    struct BitmapFont {
        static constexpr int GlyphWidth = 5;
        static constexpr int GlyphHeight = 7;
        static constexpr int Advance = 6;          // glyph plus one pixel of spacing
        static constexpr char FirstChar = 32;
        static constexpr int GlyphCount = 64;      // ' ' (32) to '_' (95)

        // Atlas layout: 8 x 8 cells of 8 x 10 pixels (glyph at +1, +1), then a white strip that
        // solid rectangles sample
        static constexpr int CellWidth = 8;
        static constexpr int CellHeight = 10;
        static constexpr int Columns = 8;
        static constexpr int AtlasWidth = CellWidth * Columns;
        static constexpr int WhiteTop = CellHeight * (GlyphCount / Columns);
        static constexpr int AtlasHeight = WhiteTop + 8;

        /// <summary>
        /// The glyph a character is drawn with (index from FirstChar)
        /// </summary>
        [[nodiscard]] static constexpr int GlyphIndex(char c) noexcept {
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
            if (c < FirstChar || c >= FirstChar + GlyphCount) c = '?';
            return c - FirstChar;
        }

        /// <summary>
        /// Rows of a glyph, top to bottom; bit 4 is the leftmost pixel
        /// </summary>
        [[nodiscard]] static const std::array<std::uint8_t, GlyphHeight>& Rows(char c) noexcept {
            return Glyphs()[static_cast<std::size_t>(GlyphIndex(c))];
        }

        [[nodiscard]] static bool Pixel(char c, int x, int y) noexcept {
            if (x < 0 || x >= GlyphWidth || y < 0 || y >= GlyphHeight) return false;
            return (Rows(c)[static_cast<std::size_t>(y)] >> (GlyphWidth - 1 - x)) & 1u;
        }

        /// <summary>
        /// Size of a line of text where each font pixel is `scale` screen pixels
        /// </summary>
        [[nodiscard]] static TextSize Measure(std::string_view text, float scale) noexcept {
            if (text.empty()) return {};
            return { static_cast<float>(text.size() * Advance - 1) * scale, static_cast<float>(GlyphHeight) * scale };
        }

        /// <summary>
        /// The atlas as 8-bit RGBA: white pixels with the glyph shapes in alpha, and an opaque strip
        /// at WhiteTop
        /// </summary>
        [[nodiscard]] static std::vector<std::uint8_t> BuildAtlas() {
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(AtlasWidth * AtlasHeight * 4), 0);
            const auto set = [&](int x, int y) {
                auto* p = &pixels[static_cast<std::size_t>((y * AtlasWidth + x) * 4)];
                p[0] = p[1] = p[2] = p[3] = 255;
            };
            for (int glyph = 0; glyph < GlyphCount; ++glyph) {
                const int left = (glyph % Columns) * CellWidth + 1;
                const int top = (glyph / Columns) * CellHeight + 1;
                for (int y = 0; y < GlyphHeight; ++y) {
                    for (int x = 0; x < GlyphWidth; ++x) {
                        if (Pixel(static_cast<char>(FirstChar + glyph), x, y)) set(left + x, top + y);
                    }
                }
            }
            for (int y = WhiteTop; y < AtlasHeight; ++y) {
                for (int x = 0; x < AtlasWidth; ++x) set(x, y);
            }
            return pixels;
        }

        /// <summary>
        /// Top-left atlas pixel of a glyph
        /// </summary>
        [[nodiscard]] static constexpr std::pair<int, int> GlyphOrigin(char c) noexcept {
            const int glyph = GlyphIndex(c);
            return { (glyph % Columns) * CellWidth + 1, (glyph / Columns) * CellHeight + 1 };
        }

    private:
        using Glyph = std::array<std::uint8_t, GlyphHeight>;

        static const std::array<Glyph, GlyphCount>& Glyphs() noexcept {
            static constexpr std::array<Glyph, GlyphCount> glyphs = {{
                { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },   // ' '
                { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 },   // !
                { 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00 },   // "
                { 0x0A, 0x1F, 0x0A, 0x0A, 0x0A, 0x1F, 0x0A },   // #
                { 0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04 },   // $
                { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 },   // %
                { 0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D },   // &
                { 0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00 },   // '
                { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 },   // (
                { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 },   // )
                { 0x00, 0x04, 0x15, 0x0E, 0x15, 0x04, 0x00 },   // *
                { 0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00 },   // +
                { 0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08 },   // ,
                { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 },   // -
                { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C },   // .
                { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00 },   // /
                { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E },   // 0
                { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E },   // 1
                { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F },   // 2
                { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E },   // 3
                { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 },   // 4
                { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E },   // 5
                { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E },   // 6
                { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },   // 7
                { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E },   // 8
                { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C },   // 9
                { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 },   // :
                { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08 },   // ;
                { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 },   // <
                { 0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00 },   // =
                { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 },   // >
                { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 },   // ?
                { 0x0E, 0x11, 0x01, 0x0D, 0x15, 0x15, 0x0E },   // @
                { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 },   // A
                { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E },   // B
                { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E },   // C
                { 0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C },   // D
                { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F },   // E
                { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 },   // F
                { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F },   // G
                { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 },   // H
                { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E },   // I
                { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C },   // J
                { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 },   // K
                { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F },   // L
                { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 },   // M
                { 0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11 },   // N
                { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E },   // O
                { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 },   // P
                { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D },   // Q
                { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 },   // R
                { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E },   // S
                { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 },   // T
                { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E },   // U
                { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 },   // V
                { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A },   // W
                { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 },   // X
                { 0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04 },   // Y
                { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F },   // Z
                { 0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E },   // [
                { 0x00, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00 },   // backslash
                { 0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E },   // ]
                { 0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00 },   // ^
                { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F },   // _
            }};
            return glyphs;
        }
    };

} // namespace LB::Systems::Rendering
