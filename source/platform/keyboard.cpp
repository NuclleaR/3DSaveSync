#include "platform/keyboard.hpp"

#include <3ds.h>

namespace platform {

std::optional<std::string> askText(const char* hint, const std::string& initial, bool password) {
    char buf[256] = {};
    SwkbdState kb;
    swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, sizeof(buf) - 1);
    swkbdSetHintText(&kb, hint);
    swkbdSetInitialText(&kb, initial.c_str());
    swkbdSetValidation(&kb, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    if (password) {
        swkbdSetPasswordMode(&kb, SWKBD_PASSWORD_HIDE_DELAY);
    }
    if (swkbdInputText(&kb, buf, sizeof(buf)) != SWKBD_BUTTON_CONFIRM) {
        return std::nullopt;
    }
    return std::string(buf);
}

}  // namespace platform
