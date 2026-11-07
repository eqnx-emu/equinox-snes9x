#include "core.hpp"

#include <cstdarg>
#include <cstdio>

#include "libretro.h"

namespace {

Core* g_core = nullptr;
std::string g_systemDir;
bool g_rgb565 = false;

void logCb(enum retro_log_level level, const char* fmt, ...) {
    if (level < RETRO_LOG_WARN) return;
    std::va_list ap;
    va_start(ap, fmt);
    std::fprintf(stderr, "[snes9x] ");
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
}

bool envCb(unsigned cmd, void* data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            const auto fmt = *static_cast<const enum retro_pixel_format*>(data);
            g_rgb565 = fmt == RETRO_PIXEL_FORMAT_RGB565;
            return g_rgb565;
        }
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            *static_cast<const char**>(data) = g_systemDir.c_str();
            return true;
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
            static_cast<retro_log_callback*>(data)->log = logCb;
            return true;
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            *static_cast<bool*>(data) = true;
            return true;
        case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
            return true;
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            *static_cast<bool*>(data) = false;
            return true;
        default:
            return false;  // core options and the rest: defaults
    }
}

void videoCb(const void* data, unsigned w, unsigned h, size_t pitch) {
    if (g_core && g_core->onVideo && data) g_core->onVideo(data, w, h, pitch);
}

void audioSampleCb(int16_t l, int16_t r) {
    if (g_core && g_core->onAudio) {
        const int16_t s[2] = {l, r};
        g_core->onAudio(s, 1);
    }
}

size_t audioBatchCb(const int16_t* data, size_t frames) {
    if (g_core && g_core->onAudio) g_core->onAudio(data, frames);
    return frames;
}

void inputPollCb() {}

int16_t inputStateCb(unsigned port, unsigned device, unsigned /*index*/, unsigned id) {
    if (device != RETRO_DEVICE_JOYPAD || !g_core || !g_core->onInput) return 0;
    return g_core->onInput(port, id);
}

}  // namespace

Core::~Core() { unload(); }

bool Core::load(const Rom& rom, const std::string& systemDir, const std::string& romPath) {
    if (loaded_ || g_core) return false;
    g_core = this;
    g_systemDir = systemDir;

    retro_set_environment(envCb);
    retro_init();
    retro_set_video_refresh(videoCb);
    retro_set_audio_sample(audioSampleCb);
    retro_set_audio_sample_batch(audioBatchCb);
    retro_set_input_poll(inputPollCb);
    retro_set_input_state(inputStateCb);

    retro_system_info info{};
    retro_get_system_info(&info);
    if (info.need_fullpath) {
        std::fprintf(stderr, "core needs a full path, unsupported\n");
        retro_deinit();
        g_core = nullptr;
        return false;
    }

    retro_game_info game{};
    const std::string& name = rom.innerName.empty() ? romPath : rom.innerName;
    game.path = name.c_str();
    game.data = rom.data.data();
    game.size = rom.data.size();
    if (!retro_load_game(&game)) {
        retro_deinit();
        g_core = nullptr;
        return false;
    }
    retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    retro_set_controller_port_device(1, RETRO_DEVICE_JOYPAD);

    retro_system_av_info av{};
    retro_get_system_av_info(&av);
    if (av.timing.fps > 0) fps_ = av.timing.fps;
    if (av.timing.sample_rate > 0) sampleRate_ = av.timing.sample_rate;
    loaded_ = true;
    return true;
}

void Core::runFrame() {
    if (loaded_) retro_run();
}

void Core::unload() {
    if (!loaded_) return;
    retro_unload_game();
    retro_deinit();
    loaded_ = false;
    g_core = nullptr;
}

std::span<uint8_t> Core::saveRam() {
    if (!loaded_) return {};
    const size_t n = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    void* p = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (!n || !p) return {};
    return {static_cast<uint8_t*>(p), n};
}

std::vector<uint8_t> Core::serialize() {
    if (!loaded_) return {};
    std::vector<uint8_t> out(retro_serialize_size());
    if (out.empty() || !retro_serialize(out.data(), out.size())) return {};
    return out;
}

bool Core::unserialize(std::span<const uint8_t> state) {
    return loaded_ && !state.empty() && retro_unserialize(state.data(), state.size());
}
