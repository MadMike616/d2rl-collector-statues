#pragma once
#include <D2RLPlugin/api.h>
namespace CollectorSound {
bool Install(const D2RL::PluginContext*) noexcept;
void Play(const D2RL::PluginContext*) noexcept;
void Stop() noexcept;
}
