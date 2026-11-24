#pragma once
#include <D2RLPlugin/api.h>
namespace CollectorNative {
bool Install(const D2RL::PluginContext*) noexcept;
constexpr bool InventoryInputAllowed(bool admitted,bool blocked,uint32_t modes) noexcept {
    // Mode 0x0c was observed set with only normal inventory open. It cannot
    // identify a vendor screen in this UI. Item storage/ownership checks still
    // exclude vendor-owned items; configuration and trade remain blocked.
    constexpr uint32_t rejected=(1u<<0x0b)|(1u<<0x17);
    constexpr uint32_t storage=(1u<<1)|(1u<<0x18)|(1u<<0x19);
    return admitted&&!blocked&&!(modes&rejected)&&(modes&storage)!=0;
}
bool SafeInventoryInput() noexcept;
struct InventoryInputState {
    bool admitted{},blocked{};
    uint32_t modes{};
    constexpr bool contextAllowed() const noexcept {return InventoryInputAllowed(admitted,false,modes);}
    constexpr bool ready() const noexcept {return InventoryInputAllowed(admitted,blocked,modes);}
};
InventoryInputState InspectInventoryInput() noexcept;
void RefreshClasses(const D2RL::PluginContext*, const D2RL::DataTableService*) noexcept;
bool HideUnidentified(unsigned stringId, unsigned bank, unsigned classId) noexcept;
}
