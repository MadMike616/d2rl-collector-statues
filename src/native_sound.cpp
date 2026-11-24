#include "native_sound.h"
#include "sound_profile.h"
#include <windows.h>
#include <array>
#include <atomic>
#include <cstring>

namespace {
// Independently validated from the loader-hosted capture, including the
// nullptr-unit call path. The game's own mixer handles volume and muting.
using Playback=uint64_t(__fastcall*)(int,void*,int,int,bool);
using Lookup=void*(__fastcall*)(int,void*,int);
Playback playback{};
Lookup lookup{};
std::atomic<bool> admitted{},warned{};
constexpr int IdentifySound=13;
bool NativePlay() noexcept {
    __try {
        const auto row=lookup(IdentifySound,nullptr,1); // Raw row, no HD redirect.
        std::array<char,32> name{};SIZE_T copied{};
        if(!row||!ReadProcessMemory(GetCurrentProcess(),static_cast<const char*>(row)+2,name.data(),name.size(),&copied)
            ||copied!=name.size()||!std::memchr(name.data(),0,name.size())
            ||std::strcmp(name.data(),"cursor_identify_item")!=0)return false;
        (void)playback(IdentifySound,nullptr,0,0,false);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
}
namespace CollectorSound {
bool Install(const D2RL::PluginContext* context) noexcept {
    using namespace CollectorSoundProfile;
    admitted.store(false);warned.store(false);
    if(!context->CheckExpectedBytes(PlaybackRva,PlaybackBytes.data(),static_cast<uint32_t>(PlaybackBytes.size()))
        ||!context->CheckExpectedBytes(LookupRva,LookupBytes.data(),static_cast<uint32_t>(LookupBytes.size()))) {
        context->LogWarn("Collector Statues: opening sound disabled because the native audio code did not match its validated layout.");return false;
    }
    playback=reinterpret_cast<Playback>(context->exeBase+PlaybackRva);
    lookup=reinterpret_cast<Lookup>(context->exeBase+LookupRva);
    admitted.store(true);
    context->LogInfo("Collector Statues: opening sound enabled.");
    return true;
}
void Play(const D2RL::PluginContext* context) noexcept {
    if(!admitted.load())return;
    if(!NativePlay()) {
        admitted.store(false);
        if(!warned.exchange(true))context->LogWarn("Collector Statues: the box opened, but its sound was disabled because the native sound row or playback call was unavailable.");
    }
}
void Stop() noexcept {admitted.store(false);}
}
