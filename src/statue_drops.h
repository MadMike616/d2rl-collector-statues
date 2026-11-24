#pragma once
#include <D2RLPlugin/api.h>
namespace CollectorDrops {
bool Install(const D2RL::PluginContext*) noexcept;
void GameLeft() noexcept;
void Stop() noexcept;
}
