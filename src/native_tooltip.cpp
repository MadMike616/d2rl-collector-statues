#include "native_tooltip.h"
#include "native_profile.h"
#include "statue_definitions.h"
#include <windows.h>
#include <atomic>
#include <array>
#include <cstdio>

extern "C" {
uintptr_t CollectorStatuesContinueA{}, CollectorStatuesContinueB{};
void CollectorStatuesTooltipA();
void CollectorStatuesTooltipB();
}
namespace {
using namespace D2RL;
using Lookup = const char*(__fastcall*)(uint32_t);
Lookup lookup{};
const PluginContext* nativeOwner{};
const DataTableService* tables{};
bool layoutAdmitted{}, inputAdmitted{};
std::array<std::array<std::atomic<uint32_t>,CollectorStatues::Count>,3> classes{};
std::array<std::atomic<bool>,3> ready{};

template<size_t N> bool Match(const PluginContext* c,uintptr_t rva,const std::array<uint8_t,N>& bytes) noexcept {
    return c->CheckExpectedBytes(rva,bytes.data(),static_cast<uint32_t>(N));
}
void __cdecl TablesLoaded(const PluginContext* c,const Lifecycle::DataTablesLoadedEvent*,void*) noexcept {
    CollectorNative::RefreshClasses(c,tables);
}
bool ReadIdentity(void* item,uint32_t& cls,uint8_t& bank) noexcept {
    if(!layoutAdmitted||!item)return false;
    std::array<uint32_t,2> head{};SIZE_T copied{};
    if(!ReadProcessMemory(GetCurrentProcess(),item,head.data(),sizeof(head),&copied)||copied!=sizeof(head)||head[0]!=4)return false;
    if(!ReadProcessMemory(GetCurrentProcess(),static_cast<const char*>(item)+0x1bd,&bank,1,&copied)||copied!=1)return false;
    cls=head[1];return bank>=1&&bank<=3;
}
}

extern "C" const char* __fastcall CollectorStatuesLookup(uint32_t stringId,void* item) noexcept {
    uint32_t cls{};uint8_t bank{};
    if(stringId==3455&&ReadIdentity(item,cls,bank)&&CollectorNative::HideUnidentified(stringId,bank,cls))return "";
    return lookup?lookup(stringId):"";
}

namespace CollectorNative {
bool HideUnidentified(unsigned stringId,unsigned bank,unsigned cls) noexcept {
    if(stringId!=3455||bank<1||bank>3||!ready[bank-1].load(std::memory_order_acquire))return false;
    for(const auto& entry:classes[bank-1])if(entry.load(std::memory_order_relaxed)==cls)return true;
    return false;
}
void RefreshClasses(const PluginContext* c,const DataTableService* service) noexcept {
    for(unsigned bank=1;bank<=3;++bank) {
        ready[bank-1].store(false,std::memory_order_release);
        unsigned count{};
        for(size_t i=0;i<CollectorStatues::Count;++i) {
            classes[bank-1][i].store(UINT32_MAX,std::memory_order_relaxed);
            DataTables::RowView row{.structSize=DataTables::RowViewSize};
            const auto code=CollectorStatues::Definitions[i].code;
            auto found=service->findRowByCode(c,static_cast<DataTables::Bank>(bank),DataTables::TableId::Items,code,&row);
            if(found==DataTables::Result::NotFound&&(code>>24)==' ') {
                const auto zeroPadded=code&0x00ffffffu;
                found=service->findRowByCode(c,static_cast<DataTables::Bank>(bank),DataTables::TableId::Items,zeroPadded,&row);
            }
            if(found==DataTables::Result::Success
                &&row.tableId==DataTables::TableId::Items&&static_cast<unsigned>(row.bank)==bank) {
                classes[bank-1][i].store(row.rowIndex,std::memory_order_relaxed);++count;
            }
        }
        ready[bank-1].store(count==CollectorStatues::Count,std::memory_order_release);

    }
}

bool SafeInventoryInput() noexcept {
    return InspectInventoryInput().ready();
}

InventoryInputState InspectInventoryInput() noexcept {
    if(!inputAdmitted||!nativeOwner)return {};
    using ModeFn=bool(__fastcall*)(int);
    using BlockedFn=bool(__fastcall*)();
    const auto mode=reinterpret_cast<ModeFn>(nativeOwner->exeBase+CollectorProfile::ModeRva);
    const auto blocked=reinterpret_cast<BlockedFn>(nativeOwner->exeBase+CollectorProfile::BlockedRva);
    InventoryInputState state{true,blocked(),0};
    for(const int id:{1,0x0b,0x0c,0x17,0x18,0x19})if(mode(id))state.modes|=1u<<id;
    return state;
}

bool Install(const PluginContext* c) noexcept {
    using namespace CollectorProfile;
    nativeOwner=c;
    inputAdmitted=Match(c,ModeRva,Mode)&&Match(c,BlockedRva,Blocked);
    if(!inputAdmitted)c->LogWarn("Collector Statues: mouse input disabled because inventory-state code did not match its validated layout.");
    layoutAdmitted=Match(c,ClassLayoutRva,ClassLayout)&&Match(c,FormatLayoutRva,FormatLayout);
    if(!layoutAdmitted||!Match(c,EntryARva,EntryA)||!Match(c,EntryBRva,EntryB)
        ||!Match(c,WindowARva,WindowA)||!Match(c,WindowBRva,WindowB)) {
        c->LogWarn("Collector Statues: unidentified-label suppression skipped because tooltip code did not match its validated layout.");return false;
    }
    const LifecycleService* life{};const MutationService* mutations{};
    if(c->QueryService(&tables)!=ServiceQueryResult::Success||!HasDataTableServiceField(tables,DataTableServiceRequiredSize)||!tables->findRowByCode
        ||c->QueryService(&life)!=ServiceQueryResult::Success||!HasLifecycleServiceField(life,LifecycleServiceRequiredSize)||!life->registerDataTablesLoadedListener
        ||c->QueryService(&mutations)!=ServiceQueryResult::Success||!HasMutationServiceField(mutations,MutationServiceRequiredSize)
        ||!mutations->beginTransaction||!mutations->stageInlineHook||!mutations->commit||!mutations->cancelTransaction) {
        c->LogWarn("Collector Statues: required SDK services for selective tooltip suppression are unavailable.");return false;
    }
    const Lifecycle::DataTablesLoadedListener listener{.structSize=Lifecycle::DataTablesLoadedListenerSize,.callback=TablesLoaded};
    Lifecycle::ListenerHandle listenerHandle{};
    if(life->registerDataTablesLoadedListener(c,&listener,&listenerHandle)!=Lifecycle::Result::Success)return false;
    lookup=reinterpret_cast<Lookup>(c->exeBase+0x3e2b730);
    CollectorStatuesContinueA=c->exeBase+CallARva+5;
    CollectorStatuesContinueB=c->exeBase+CallBRva+5;
    Mutations::TransactionHandle transaction{};
    if(mutations->beginTransaction(c,&transaction)!=Mutations::Result::Success)return false;
    const std::array<Mutations::InlineHookRequest,2> hooks{{
        {.structSize=Mutations::InlineHookRequestSize,.rva=CallARva,.expected=CallA.data(),.expectedSize=static_cast<uint32_t>(CallA.size()),.target=reinterpret_cast<void*>(CollectorStatuesTooltipA)},
        {.structSize=Mutations::InlineHookRequestSize,.rva=CallBRva,.expected=CallB.data(),.expectedSize=static_cast<uint32_t>(CallB.size()),.target=reinterpret_cast<void*>(CollectorStatuesTooltipB)},
    }};
    for(const auto& hook:hooks) {
        Mutations::OperationHandle operation{};
        if(mutations->stageInlineHook(c,transaction,&hook,&operation)!=Mutations::Result::Success) {
            (void)mutations->cancelTransaction(c,transaction);return false;
        }
    }
    Mutations::CommitResult result{.structSize=Mutations::CommitResultSize};
    if(mutations->commit(c,transaction,&result)!=Mutations::Result::Success) {
        c->LogWarn("Collector Statues: tooltip patches could not be committed; original labels retained.");return false;
    }
    c->LogInfo("Collector Statues: selective unidentified-label suppression installed.");
    return true;
}
}
