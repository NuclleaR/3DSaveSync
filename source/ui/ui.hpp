// citro2d drawing helpers. Colors follow docs/superpowers/mockups-2026-11-12-screens.html.
#pragma once

#include <3ds.h>
#include <citro2d.h>

#include <string>
#include <string_view>

namespace ui {

constexpr float kTopWidth = 400.0f;
constexpr float kBottomWidth = 320.0f;
constexpr float kScreenHeight = 240.0f;

constexpr u32 rgb(u32 hex) {
    // C2D_Color32 layout (0xAABBGGRR) from 0xRRGGBB.
    return 0xFF000000u | ((hex & 0xFF) << 16) | (hex & 0xFF00) | ((hex >> 16) & 0xFF);
}

inline constexpr u32 kBackground = rgb(0x101418);
inline constexpr u32 kText = rgb(0xd8dee4);
inline constexpr u32 kMuted = rgb(0x8b949e);
inline constexpr u32 kLine = rgb(0x30363d);
inline constexpr u32 kAccent = rgb(0x1f6feb);
inline constexpr u32 kOk = rgb(0x3fb950);
inline constexpr u32 kError = rgb(0xf85149);
inline constexpr u32 kCurrent = rgb(0xd29922);
inline constexpr u32 kButton = rgb(0x1c2128);
inline constexpr u32 kButtonBorder = rgb(0x3d444d);
inline constexpr u32 kButtonText = rgb(0xe6edf3);
inline constexpr u32 kDanger = rgb(0xb62324);
inline constexpr u32 kWhite = rgb(0xffffff);

struct Button {
    float x, y, w, h;
    std::string label;
    bool primary = false;
    bool danger = false;
};

class Ui {
public:
    Ui() = default;
    ~Ui();
    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;

    bool init();
    void beginFrame();
    void endFrame();

    // wrap > 0 breaks lines at that width.
    void text(gfxScreen_t s, float x, float y, std::string_view t, u32 color, float scale = 0.5f, float wrap = 0.0f);
    float textWidth(std::string_view t, float scale = 0.5f);
    void rect(gfxScreen_t s, float x, float y, float w, float h, u32 color);
    // Top-screen bar: "3DSaveSync" on the left, status on the right.
    void topBar(std::string_view status, u32 statusColor);
    // A bottom-screen button; focused draws the D-pad selection outline.
    void button(const Button& b, bool focused = false);
    static bool hit(const Button& b, const touchPosition& p);

private:
    void target(gfxScreen_t s);

    bool ready_ = false;
    C3D_RenderTarget* top_ = nullptr;
    C3D_RenderTarget* bottom_ = nullptr;
    C3D_RenderTarget* current_ = nullptr;
    C2D_TextBuf textBuf_ = nullptr;
};

}  // namespace ui
