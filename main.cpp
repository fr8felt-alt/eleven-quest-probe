#include <android/log.h>

__attribute__((constructor))
static void iniciar_teste() {
    __android_log_print(
        ANDROID_LOG_INFO,
        "ElevenQuestProbe",
        "Biblioteca ARM64 carregada com sucesso!"
    );
}
