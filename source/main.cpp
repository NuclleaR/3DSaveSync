#include <3ds.h>
#include <citro2d.h>

int main() {
    romfsInit();
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C2D_TextBuf textBuf = C2D_TextBufNew(256);
    C2D_Text title;
    C2D_TextParse(&title, textBuf, "3DSaveSync");
    C2D_TextOptimize(&title);

    const u32 background = C2D_Color32(0x10, 0x14, 0x18, 0xFF);
    const u32 foreground = C2D_Color32(0xD8, 0xDE, 0xE4, 0xFF);

    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) {
            break;
        }
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top, background);
        C2D_SceneBegin(top);
        C2D_DrawText(&title, C2D_WithColor, 20.0f, 20.0f, 0.5f, 1.0f, 1.0f, foreground);
        C3D_FrameEnd(0);
    }

    C2D_TextBufDelete(textBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    romfsExit();
    return 0;
}
