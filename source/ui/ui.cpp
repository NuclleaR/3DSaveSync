#include "ui/ui.hpp"

namespace ui {

namespace {

constexpr size_t kGlyphsPerFrame = 4096;
constexpr float kDepth = 0.5f;

}  // namespace

Ui::~Ui() {
    if (!ready_) {
        return;
    }
    C2D_TextBufDelete(textBuf_);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
}

bool Ui::init() {
    gfxInitDefault();
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        gfxExit();
        return false;
    }
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
        C3D_Fini();
        gfxExit();
        return false;
    }
    C2D_Prepare();
    top_ = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom_ = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    textBuf_ = C2D_TextBufNew(kGlyphsPerFrame);
    ready_ = true;
    return true;
}

void Ui::beginFrame() {
    C2D_TextBufClear(textBuf_);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TargetClear(top_, kBackground);
    C2D_TargetClear(bottom_, kBackground);
    current_ = nullptr;
}

void Ui::endFrame() {
    C3D_FrameEnd(0);
}

void Ui::target(gfxScreen_t s) {
    C3D_RenderTarget* want = s == GFX_TOP ? top_ : bottom_;
    if (want != current_) {
        C2D_SceneBegin(want);
        current_ = want;
    }
}

void Ui::text(gfxScreen_t s, float x, float y, std::string_view t, u32 color, float scale, float wrap) {
    target(s);
    const std::string str(t);
    C2D_Text txt;
    C2D_TextParse(&txt, textBuf_, str.c_str());
    C2D_TextOptimize(&txt);
    if (wrap > 0.0f) {
        C2D_DrawText(&txt, C2D_WithColor | C2D_WordWrap, x, y, kDepth, scale, scale, color, wrap);
    } else {
        C2D_DrawText(&txt, C2D_WithColor, x, y, kDepth, scale, scale, color);
    }
}

float Ui::textWidth(std::string_view t, float scale) {
    const std::string str(t);
    C2D_Text txt;
    C2D_TextParse(&txt, textBuf_, str.c_str());
    float w = 0.0f;
    float h = 0.0f;
    C2D_TextGetDimensions(&txt, scale, scale, &w, &h);
    return w;
}

void Ui::rect(gfxScreen_t s, float x, float y, float w, float h, u32 color) {
    target(s);
    C2D_DrawRectSolid(x, y, kDepth, w, h, color);
}

void Ui::topBar(std::string_view status, u32 statusColor) {
    text(GFX_TOP, 10.0f, 6.0f, "3DSaveSync", kMuted);
    text(GFX_TOP, kTopWidth - 10.0f - textWidth(status), 6.0f, status, statusColor);
    rect(GFX_TOP, 10.0f, 24.0f, kTopWidth - 20.0f, 1.0f, kLine);
}

void Ui::button(const Button& b, bool focused) {
    const u32 fill = b.danger ? kDanger : b.primary ? kAccent : kButton;
    const u32 border = b.danger ? kDanger : b.primary ? kAccent : kButtonBorder;
    const u32 label = b.primary || b.danger ? kWhite : kButtonText;
    if (focused) {
        rect(GFX_BOTTOM, b.x - 3.0f, b.y - 3.0f, b.w + 6.0f, b.h + 6.0f, kCurrent);
        rect(GFX_BOTTOM, b.x - 1.0f, b.y - 1.0f, b.w + 2.0f, b.h + 2.0f, kBackground);
    }
    rect(GFX_BOTTOM, b.x, b.y, b.w, b.h, border);
    rect(GFX_BOTTOM, b.x + 1.0f, b.y + 1.0f, b.w - 2.0f, b.h - 2.0f, fill);
    const float scale = 0.5f;
    const float lineHeight = 30.0f * scale;  // system font line height at scale 1 is 30 px
    text(GFX_BOTTOM, b.x + (b.w - textWidth(b.label, scale)) / 2.0f, b.y + (b.h - lineHeight) / 2.0f, b.label, label,
         scale);
}

bool Ui::hit(const Button& b, const touchPosition& p) {
    return p.px >= b.x && p.px < b.x + b.w && p.py >= b.y && p.py < b.y + b.h;
}

}  // namespace ui
