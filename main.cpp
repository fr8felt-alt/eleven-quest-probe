#include <android/log.h>
#include "scotland2/shared/modloader.h"

namespace {
constexpr const char* TAG = "ElevenQuestProbe";
modloader::ModInfo modInfo{"eleven-quest-probe", "0.1.0", 0};
}

MOD_EXTERN_FUNC void setup(CModInfo* info) noexcept {
    *info = modInfo.to_c();
    __android_log_print(ANDROID_LOG_INFO, TAG, "setup(): biblioteca ARM64 carregada pelo Scotland2");
}

MOD_EXTERN_FUNC void late_load() noexcept {
    __android_log_print(ANDROID_LOG_INFO, TAG, "late_load(): inicializacao Unity concluida; primeiro teste OK");
}
