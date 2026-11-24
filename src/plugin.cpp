#include <D2RLPlugin/api.h>
#include "native_tooltip.h"
#include "statue_drops.h"
#include "statue_definitions.h"
#include "native_sound.h"
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace {
using namespace D2RL;

constexpr PluginInfo Info{
    .infoSize = PluginInfoSize,
    .abiVersion = D2RL_PLUGIN_ABI_VERSION,
    .id = "collector-statues",
    .name = "Collector Statues",
    .version = "1.0.0",
    .author = "MadMike",
    .description = "Handles collection statue logic",
    .flags = PluginFlags::Shared | PluginFlags::ModScopedOnly | PluginFlags::NativeHooks,
};

const PluginContext* owner{};
const ItemService* items{};
const InventoryService* inventory{};
const ThreadService* threads{};
const ItemInteractionService* interactions{};
const SharedEventService* events{};
const LocalizationService* localization{};
const LifecycleService* lifecycle{};
const NetworkService* network{};
ItemInteractions::ListenerHandle activation{};
SharedEvents::ListenerHandle tooltip{};
Lifecycle::ListenerHandle leftGame{}, playerReady{};
Network::ChannelHandle channel{};

// Only storage surfaces covered by the SDK's item-activation contract.
constexpr uint32_t Storage = ItemInteractions::DefaultContainerMask
    | Items::ContainerBit(Items::ItemContainer::SharedStash);
uint32_t supportedStorage = ItemInteractions::DefaultContainerMask;

constexpr bool StatueCode(uint32_t code) noexcept {
    return CollectorStatues::RegisteredCode(code);
}

bool Unopened(const Items::ItemInfo& item) noexcept {
    return item.structSize >= Items::ItemInfoRequiredSize && StatueCode(item.code)
        && item.quality == Items::Quality::Unique
        && (item.stateFlags & Items::ItemStateIdentified) == 0;
}

bool InStorage(const Items::ItemInfo& item) noexcept {
    return (supportedStorage & Items::ContainerBit(item.container)) != 0;
}

// Peer messages contain identity, never a native pointer or a foreign handle.
struct OpenMessage {
    uint32_t code, classId;
    Items::ItemContainer container;
    int32_t inventoryPage;
    uint32_t sharedStashPage;
    int32_t x,y;
    uint32_t soundTicketLow{},soundTicketHigh{};
};
static_assert(sizeof(OpenMessage) == 36); // Explicit halves avoid wire padding.
struct Request {
    PlayerHandle player{};
    ItemHandle item{};
    Network::PeerHandle peer{};
    OpenMessage identity{};
};
std::array<Request, 32> pending{};
size_t pendingCount{};
bool pumpQueued{};
std::mutex pendingMutex;

bool SameItem(const Items::ItemInfo& item, const OpenMessage& message) noexcept {
    return item.code == message.code && item.classId == message.classId
        && item.container == message.container && item.inventoryPage == message.inventoryPage
        && (item.container != Items::ItemContainer::SharedStash || item.sharedStashPage == message.sharedStashPage)
        && item.x == message.x && item.y == message.y;
}

std::atomic<bool> soundReady{},feedbackGameReady{};
void(*soundPlayer)(const PluginContext*) noexcept=CollectorSound::Play;
struct Feedback { uint64_t ticket{};bool queued{}; };
std::array<Feedback,32> feedback{};
std::mutex feedbackMutex;
uint64_t nextFeedbackTicket{};
uint64_t RegisterFeedback() noexcept {
    if(!soundReady.load()||!feedbackGameReady.load())return 0;
    std::lock_guard lock(feedbackMutex);
    const auto ticket=++nextFeedbackTicket;
    feedback[static_cast<size_t>(ticket%feedback.size())]={ticket,false};
    return ticket;
}
void ForgetFeedback(uint64_t ticket) noexcept {
    if(!ticket)return;
    std::lock_guard lock(feedbackMutex);
    for(auto& entry:feedback)if(entry.ticket==ticket){entry={};break;}
}
void ResetFeedback() noexcept {
    feedbackGameReady.store(false);
    std::lock_guard lock(feedbackMutex);feedback={};
}
void __cdecl PlayFeedback(const PluginContext* context,void* token) noexcept {
    const auto ticket=static_cast<uint64_t>(reinterpret_cast<uintptr_t>(token));
    bool found{};
    {
        std::lock_guard lock(feedbackMutex);
        for(auto& entry:feedback)if(entry.ticket==ticket&&entry.queued){entry={};found=true;break;}
    }
    PlayerHandle player{};
    if(found&&soundReady.load()&&feedbackGameReady.load()
        &&inventory->getLocalPlayer(context,&player)==Inventory::Result::Success)soundPlayer(context);
}
void QueueFeedback(const PluginContext* context,uint64_t ticket) noexcept {
    if(!ticket||!soundReady.load()||!feedbackGameReady.load())return;
    {
        std::lock_guard lock(feedbackMutex);
        bool found{};
        for(auto& entry:feedback)if(entry.ticket==ticket&&!entry.queued){entry.queued=true;found=true;break;}
        if(!found)return;
    }
    if(threads->runOnUiThread(context,PlayFeedback,reinterpret_cast<void*>(static_cast<uintptr_t>(ticket)))!=Threads::Result::Success)
        ForgetFeedback(ticket);
}
uint64_t SoundTicket(const OpenMessage& message) noexcept {
    return (static_cast<uint64_t>(message.soundTicketHigh)<<32)|message.soundTicketLow;
}
struct Search { OpenMessage identity{}; ItemHandle found{}; unsigned count{}; };
Inventory::IterationAction __cdecl FindOwnedItem(const PluginContext*, const Items::ItemInfo* item, void* data) noexcept {
    auto& search = *static_cast<Search*>(data);
    if (item && Unopened(*item) && InStorage(*item) && SameItem(*item, search.identity)) {
        search.found = item->handle;
        ++search.count;
    }
    return Inventory::IterationAction::Continue;
}

void Open(const PluginContext* context, Request request) noexcept {
    if (request.peer != Network::InvalidPeerHandle) {
        if (!network || network->getPeerPlayer(context, channel, request.peer, &request.player) != Network::Result::Success)
            return;
    } else if(inventory->getLocalPlayer(context,&request.player)!=Inventory::Result::Success) return;
    // UI and server copies do not share item handles or necessarily unit IDs.
    // Resolve exactly one owned server item in the selected storage cell.
    Search search{request.identity};
    const Inventory::ItemFilter filter{.structSize = Inventory::ItemFilterSize, .containerMask = Items::ContainerBit(request.identity.container)};
    if(inventory->forEachInventoryItem(context,request.player,&filter,FindOwnedItem,&search)!=Inventory::Result::Success
        ||search.count!=1||search.found==InvalidItemHandle) {
        return;
    }
    request.item=search.found;
    Items::ItemInfo current{.structSize = Items::ItemInfoSize};
    if (items->getItemInfo(context, request.item, &current) != Items::Result::Success
        || !Unopened(current) || !InStorage(current) || !SameItem(current, request.identity)) return;
    // Edit only identification; preserve item identity, location, seeds and stats.
    const Items::ItemEdit edit{
        .structSize = Items::ItemEditSize,
        .fields = Items::EditFieldBit(Items::EditField::Identified),
        .stateFlags = current.stateFlags | Items::ItemStateIdentified,
    };
    const auto result=items->editItem(context, request.player, request.item, &edit);
    if(result!=Items::Result::Success) {
        char line[160]{};std::snprintf(line,sizeof(line),"Collector Statues: identification edit failed (SDK result %u); the item was left in place.",static_cast<unsigned>(result));context->LogWarn(line);
    } else {
        const auto ticket=SoundTicket(request.identity);
        if(request.peer==Network::InvalidPeerHandle)QueueFeedback(context,ticket);
        else if(ticket&&network&&network->sendToClient)
            (void)network->sendToClient(context,channel,request.peer,2,&ticket,sizeof(ticket));
    }
}

void __cdecl Pump(const PluginContext* context, void*) noexcept {
    for (;;) {
        Request request;
        {
            std::lock_guard lock(pendingMutex);
            if (!pendingCount) { pumpQueued = false; return; }
            request = pending[0];
            for (size_t i = 1; i < pendingCount; ++i) pending[i - 1] = pending[i];
            --pendingCount;
        }
        Open(context, request);
    }
}

Threads::Result Queue(const PluginContext* context, const Request& request) noexcept {
    std::lock_guard lock(pendingMutex);
    if (pendingCount == pending.size()) return Threads::Result::Busy;
    pending[pendingCount++] = request;
    if (pumpQueued) return Threads::Result::Success;
    const auto result = threads->runOnGameThread(context, Pump, nullptr);
    if (result == Threads::Result::Success) pumpQueued = true;
    else --pendingCount;
    return result;
}

void __cdecl ReceiveOpen(const PluginContext* context, Network::ChannelHandle receivedChannel,
    Network::PeerHandle peer, uint16_t messageId, const void* data, uint32_t size, void*) noexcept {
    if (receivedChannel != channel || messageId != 1 || !data || size != sizeof(OpenMessage)
        || peer == Network::InvalidPeerHandle) return;
    Request request{.peer = peer};
    std::memcpy(&request.identity, data, sizeof(OpenMessage));
    if (StatueCode(request.identity.code)) (void)Queue(context, request);
}
void __cdecl ReceiveOpened(const PluginContext* context,Network::ChannelHandle receivedChannel,
    uint16_t messageId,const void* data,uint32_t size,void*) noexcept {
    if(receivedChannel!=channel||messageId!=2||!data||size!=sizeof(uint64_t))return;
    uint64_t ticket{};std::memcpy(&ticket,data,sizeof(ticket));QueueFeedback(context,ticket);
}

void Submit(const PluginContext* context,PlayerHandle player,const Items::ItemInfo& item) noexcept {
    const auto ticket=RegisterFeedback();
    const Request request{
        .player = player, .item = item.handle,
        .identity = {item.code,item.classId,item.container,item.inventoryPage,item.sharedStashPage,item.x,item.y,
            static_cast<uint32_t>(ticket),static_cast<uint32_t>(ticket>>32)},
    };
    const auto result = Queue(context, request);
    if (result == Threads::Result::Unavailable && network && channel) {
        // Remote clients ask the host to identify an item owned by that peer.
        (void)network->connectToHost(context, channel);
        if (network->sendToHost(context, channel, 1, &request.identity, sizeof(OpenMessage)) != Network::Result::Success) {
            ForgetFeedback(ticket);
            context->LogWarn("Collector Statues: host connection is not ready; retry opening the box.");
        }
    } else if (result != Threads::Result::Success) {
        ForgetFeedback(ticket);
        context->LogWarn("Collector Statues: no authoritative game update is available; retry opening the box.");
    }
}

ItemInteractions::Decision __cdecl Activate(const PluginContext* context,
    const ItemInteractions::ItemInteractionEvent* event, void*) noexcept {
    if (!event || event->structSize < ItemInteractions::ItemInteractionEventRequiredSize
        || event->action != ItemInteractions::Action::Activate || event->modifiers
        // SDK Activate has no physical mouse-button field and also arrives for
        // left-click selection. Mouse opening belongs to the right-button
        // adapter below; retain semantic activation for controllers only.
        || event->inputSource != ItemInteractions::InputSource::Controller
        || (supportedStorage & Items::ContainerBit(event->container)) == 0)
        return ItemInteractions::Decision::Continue;
    Items::ItemInfo item{.structSize = Items::ItemInfoSize};
    if (items->getItemInfo(context, event->item, &item) != Items::Result::Success || !Unopened(item))
        return ItemInteractions::Decision::Continue;
    Submit(context,event->player,item);
    return ItemInteractions::Decision::Consume;
}

struct HoverSnapshot { Items::ItemInfo item{};PlayerHandle player{};uint64_t tick{};POINT point{};bool valid{}; };
HoverSnapshot hovered{};
struct MouseRequest { HoverSnapshot target{};uint64_t serial{},clicked{};unsigned attempts{}; } mouseRequest;
uint64_t nextMouseSerial{};
constexpr uint64_t MouseOpenTimeoutMs=1500;
constexpr unsigned MouseOpenMaxAttempts=256;
std::mutex hoverMutex;
HHOOK mouseHook{};HWND gameWindow{};
bool mouseUiQueued{},ownsRightUp{};

bool FreshHover(uint64_t seen,uint64_t now,LONG dx,LONG dy) noexcept {
    return now>=seen&&now-seen<=400&&dx>=-4&&dx<=4&&dy>=-4&&dy<=4;
}
bool CursorEmpty(const PluginContext* c,PlayerHandle player) noexcept {
    ItemHandle cursor{};const auto result=inventory->getCursorItem(c,player,&cursor);
    return (result==Inventory::Result::Success&&cursor==InvalidItemHandle)||result==Inventory::Result::NotFound;
}
bool ReadHoveredItem(const PluginContext* c,const HoverSnapshot& snapshot,Items::ItemInfo& item) noexcept {
    if(!snapshot.valid||!CursorEmpty(c,snapshot.player))return false;
    if(items->getItemInfo(c,snapshot.item.handle,&item)!=Items::Result::Success||!Unopened(item)||!InStorage(item)
        ||item.runtimeId!=snapshot.item.runtimeId||item.code!=snapshot.item.code||item.classId!=snapshot.item.classId
        ||item.container!=snapshot.item.container||item.inventoryPage!=snapshot.item.inventoryPage
        ||item.sharedStashPage!=snapshot.item.sharedStashPage||item.x!=snapshot.item.x||item.y!=snapshot.item.y
        ||item.generationSeed!=snapshot.item.generationSeed||item.itemSeed!=snapshot.item.itemSeed)return false;
    return true;
}
void OpenHovered(const PluginContext* c,const HoverSnapshot& snapshot,bool safeInput) noexcept {
    Items::ItemInfo item{.structSize=Items::ItemInfoSize};
    if(!safeInput||!ReadHoveredItem(c,snapshot,item))return;
    Submit(c,snapshot.player,item);
}
void __cdecl MouseOpen(const PluginContext* c,void* token) noexcept;
void CancelMouseRequest(uint64_t serial) noexcept {
    std::lock_guard lock(hoverMutex);
    if(mouseRequest.serial==serial){mouseRequest={};mouseUiQueued=false;}
}
// The SDK schedules each retry during a later root-widget update. Never spin,
// sleep on the UI thread, or identify an item while native input is busy.
void AdvanceMouseRequest(const PluginContext* c,uint64_t serial,uint64_t now,
    const CollectorNative::InventoryInputState& input,bool targetStillHovered) noexcept {
    MouseRequest request;
    {
        std::lock_guard lock(hoverMutex);
        if(!serial||mouseRequest.serial!=serial||!mouseUiQueued)return;
        ++mouseRequest.attempts;request=mouseRequest;
    }
    if(!input.contextAllowed()||!targetStillHovered||now<request.clicked) {CancelMouseRequest(serial);return;}
    if(now-request.clicked>MouseOpenTimeoutMs||request.attempts>MouseOpenMaxAttempts) {
        CancelMouseRequest(serial);
        c->LogWarn("Collector Statues: opening cancelled because inventory input stayed busy; retry the box.");return;
    }
    Items::ItemInfo item{.structSize=Items::ItemInfoSize};
    if(!ReadHoveredItem(c,request.target,item)){CancelMouseRequest(serial);return;}
    if(input.blocked) {
        if(threads->runOnUiThread(c,MouseOpen,reinterpret_cast<void*>(static_cast<uintptr_t>(serial)))!=Threads::Result::Success) {
            CancelMouseRequest(serial);c->LogWarn("Collector Statues: opening could not wait for the next UI update; retry the box.");
        }
        return;
    }
    CancelMouseRequest(serial);
    OpenHovered(c,request.target,input.ready());
}
bool SameHover(const HoverSnapshot& a,const HoverSnapshot& b) noexcept {
    return a.valid&&b.valid&&a.player==b.player&&a.item.handle==b.item.handle
        &&a.item.runtimeId==b.item.runtimeId&&a.item.code==b.item.code&&a.item.classId==b.item.classId
        &&a.item.container==b.item.container&&a.item.inventoryPage==b.item.inventoryPage
        &&a.item.sharedStashPage==b.item.sharedStashPage&&a.item.x==b.item.x&&a.item.y==b.item.y
        &&a.item.generationSeed==b.item.generationSeed&&a.item.itemSeed==b.item.itemSeed;
}
// Caller holds hoverMutex. Repeated clicks on a pending target neither stack
// callbacks nor extend the deadline. A new target gets a new callback token.
bool QueueMouseClick(const PluginContext* c,const HoverSnapshot& target,uint64_t now) noexcept {
    if(mouseUiQueued&&SameHover(mouseRequest.target,target))return true;
    mouseRequest={target,++nextMouseSerial,now,0};mouseUiQueued=true;
    if(threads->runOnUiThread(c,MouseOpen,reinterpret_cast<void*>(static_cast<uintptr_t>(mouseRequest.serial)))==Threads::Result::Success) {
            return true;
    }
    mouseRequest={};mouseUiQueued=false;return false;
}
void __cdecl MouseOpen(const PluginContext* c,void* token) noexcept {
    const auto serial=static_cast<uint64_t>(reinterpret_cast<uintptr_t>(token));
    MouseRequest request;HoverSnapshot latest;
    {std::lock_guard lock(hoverMutex);if(mouseRequest.serial!=serial||!mouseUiQueued)return;request=mouseRequest;latest=hovered;}
    const auto now=GetTickCount64();POINT point{};
    const bool current=GetForegroundWindow()==gameWindow&&GetCursorPos(&point)&&SameHover(request.target,latest)
        &&FreshHover(latest.tick,now,point.x-latest.point.x,point.y-latest.point.y)
        &&point.x-request.target.point.x>=-4&&point.x-request.target.point.x<=4
        &&point.y-request.target.point.y>=-4&&point.y-request.target.point.y<=4
        &&!(GetKeyState(VK_SHIFT)&0x8000)&&!(GetKeyState(VK_CONTROL)&0x8000)&&!(GetKeyState(VK_MENU)&0x8000)
        &&!(GetKeyState(VK_LBUTTON)&0x8000)&&!(GetKeyState(VK_MBUTTON)&0x8000);
    AdvanceMouseRequest(c,serial,now,CollectorNative::InspectInventoryInput(),current);
}
LRESULT CALLBACK MouseMessage(int code,WPARAM removed,LPARAM message) {
    if(code>=0&&removed==PM_REMOVE&&message) {
        auto& msg=*reinterpret_cast<MSG*>(message);
        std::lock_guard lock(hoverMutex);
        if(msg.message==WM_RBUTTONUP&&ownsRightUp) {ownsRightUp=false;msg.message=WM_NULL;}
        else if((msg.message==WM_RBUTTONDOWN||msg.message==WM_RBUTTONDBLCLK)
            &&GetForegroundWindow()==gameWindow&&GetAncestor(msg.hwnd,GA_ROOT)==gameWindow
            &&CollectorNative::InspectInventoryInput().contextAllowed()
            &&!(msg.wParam&(MK_SHIFT|MK_CONTROL|MK_LBUTTON|MK_MBUTTON))&&!(GetKeyState(VK_MENU)&0x8000)
            &&hovered.valid&&FreshHover(hovered.tick,GetTickCount64(),msg.pt.x-hovered.point.x,msg.pt.y-hovered.point.y)) {
            if(QueueMouseClick(owner,hovered,GetTickCount64())) {
                ownsRightUp=true;msg.message=WM_NULL;
            }
        }
    }
    return CallNextHookEx(mouseHook,code,removed,message);
}
void InstallMouse(const PluginContext* c) noexcept {
    if(mouseHook)return;
    struct WindowSearch { HWND window{};uint64_t area{}; } search;
    EnumWindows([](HWND window,LPARAM argument)->BOOL {
        DWORD process{};GetWindowThreadProcessId(window,&process);RECT client{};
        if(process==GetCurrentProcessId()&&IsWindowVisible(window)&&!GetWindow(window,GW_OWNER)&&GetClientRect(window,&client)) {
            const auto area=static_cast<uint64_t>(client.right-client.left)*static_cast<uint64_t>(client.bottom-client.top);
            auto& found=*reinterpret_cast<WindowSearch*>(argument);
            if(area>found.area)found={window,area};
        }return TRUE;
    },reinterpret_cast<LPARAM>(&search));
    if(!search.window)return;
    gameWindow=search.window;
    mouseHook=SetWindowsHookExW(WH_GETMESSAGE,MouseMessage,nullptr,GetWindowThreadProcessId(gameWindow,nullptr));
    if(!mouseHook)c->LogWarn("Collector Statues: game-window mouse adapter could not be installed.");
}

void RememberHover(const PluginContext* context,const Items::ItemInfo& item,
    const CollectorNative::InventoryInputState& input,const POINT& point,uint64_t now) noexcept {
    PlayerHandle player{};
    if(Unopened(item)&&InStorage(item)&&input.contextAllowed()
        &&inventory->getLocalPlayer(context,&player)==Inventory::Result::Success&&CursorEmpty(context,player)) {
        std::lock_guard lock(hoverMutex);hovered={item,player,now,point,true};
    } else {std::lock_guard lock(hoverMutex);hovered.valid=false;}
}
void __cdecl Describe(const PluginContext* context, SharedEvents::ItemTooltipEvent* event, void*) noexcept {
    if (!event || event->structSize < SharedEvents::ItemTooltipEventRequiredSize || !event->text || !event->capacity) return;
    Items::ItemInfo item{.structSize = Items::ItemInfoSize};
    if (items->getItemInfo(context, event->item, &item) != Items::Result::Success || !Unopened(item)) {
        std::lock_guard lock(hoverMutex);hovered.valid=false;return;
    }
    // A transient busy flag must not let a visible item's hover expire before
    // the first click. Only the deferred UI callback requires ready input.
    POINT point{};
    if(GetCursorPos(&point))RememberHover(context,item,CollectorNative::InspectInventoryInput(),point,GetTickCount64());
    else {std::lock_guard lock(hoverMutex);hovered.valid=false;}
    uint32_t required{};
    if (localization->getStringByKey(context, "d2r:GrailerRLCollectorStatueOpen", event->text,
            event->capacity, &required) == Localization::Result::Success && required && required <= event->capacity)
        event->length = required - 1;
}

void __cdecl GameState(const PluginContext* context, const Lifecycle::GameplayEvent* event, void*) noexcept {
    if (!event || !Lifecycle::HasGameplayEventField(event, Lifecycle::GameplayEventRequiredSize)) return;
    if (event->kind == Lifecycle::GameplayEventKind::GameLeft) {
        ResetFeedback();
        CollectorDrops::GameLeft();
        std::lock_guard lock(pendingMutex);
        pendingCount = 0;
        pumpQueued = false;
        {std::lock_guard hoverLock(hoverMutex);hovered={};mouseRequest={};mouseUiQueued=false;ownsRightUp=false;}
    } else if (event->kind == Lifecycle::GameplayEventKind::LocalPlayerReady) {
        feedbackGameReady.store(true);
        InstallMouse(context);
        if(network&&channel)(void)network->connectToHost(context, channel);
    }
}

void Cleanup() noexcept {
    ResetFeedback();soundReady.store(false);CollectorSound::Stop();
    CollectorDrops::Stop();
    if(mouseHook){UnhookWindowsHookEx(mouseHook);mouseHook=nullptr;}
    if (owner) {
        if (activation) (void)interactions->unregisterListener(owner, activation);
        if (tooltip) (void)events->unregisterItemTooltipListener(owner, tooltip);
        if (leftGame) (void)lifecycle->unregisterGameplayEventListener(owner, leftGame);
        if (playerReady) (void)lifecycle->unregisterGameplayEventListener(owner, playerReady);
        if (channel) (void)network->unregisterChannel(owner, channel);
    }
    activation = tooltip = leftGame = playerReady = channel = 0;
    std::lock_guard lock(pendingMutex);
    pendingCount = 0;
    pumpQueued = false;
    {std::lock_guard hoverLock(hoverMutex);hovered={};mouseRequest={};mouseUiQueued=false;ownsRightUp=false;}
    owner = nullptr;
}
}

D2RL_PLUGIN_EXPORT const D2RL::PluginInfo* D2RLoaderGetPluginInfo() noexcept { return &Info; }

D2RL_PLUGIN_EXPORT bool D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept {
    using namespace D2RL;
    if (!HasContext(context)) return false;
    owner = context;
    if (context->QueryService(&items) != ServiceQueryResult::Success || !HasItemServiceField(items, ItemServiceRequiredSize)
        || !items->getItemInfo || !items->editItem
        || context->QueryService(&threads) != ServiceQueryResult::Success || !HasThreadServiceField(threads, ThreadServiceRequiredSize)
        || !threads->runOnGameThread
        || !threads->runOnUiThread
        || context->QueryService(&inventory) != ServiceQueryResult::Success || !HasInventoryServiceField(inventory,InventoryServiceRequiredSize)
        || !inventory->getLocalPlayer || !inventory->getCursorItem || !inventory->forEachInventoryItem
        || context->QueryService(&interactions) != ServiceQueryResult::Success || !HasItemInteractionServiceField(interactions, ItemInteractionServiceRequiredSize)
        || !interactions->registerListener || !interactions->unregisterListener
        || context->QueryService(&events) != ServiceQueryResult::Success || !HasSharedEventServiceField(events, SharedEventServiceRequiredSize)
        || !events->registerItemTooltipListener || !events->unregisterItemTooltipListener
        || context->QueryService(&localization) != ServiceQueryResult::Success || !HasLocalizationServiceField(localization, LocalizationServiceRequiredSize)
        || !localization->getStringByKey
        || context->QueryService(&lifecycle) != ServiceQueryResult::Success || !HasLifecycleServiceField(lifecycle, LifecycleServiceRequiredSize)
        || !lifecycle->registerGameplayEventListener || !lifecycle->unregisterGameplayEventListener) {
        context->LogError("Collector Statues requires item editing, item activation, tooltip, localization and lifecycle SDK services.");
        Cleanup(); return false;
    }
    supportedStorage = ItemInteractions::DefaultContainerMask;
    if (HasItemInteractionServiceField(interactions, ItemInteractionServiceSupportedContainerMaskFieldEnd)
        && HasItemServiceCapability(items, Items::ItemServiceCapability::SharedStashWrite))
        supportedStorage = Storage & interactions->supportedContainerMask;

    // Networking is optional for local play; do not call absent service fields.
    if (context->QueryService(&network) == ServiceQueryResult::Success && HasNetworkServiceField(network, NetworkServiceRequiredSize)
        && network->registerChannel && network->unregisterChannel && network->connectToHost && network->sendToHost
        && network->getPeerPlayer && network->sendToClient && context->QueryService(&inventory) == ServiceQueryResult::Success
        && HasInventoryServiceField(inventory, InventoryServiceRequiredSize) && inventory->forEachInventoryItem) {
        const Network::ChannelRegistration registration{
            .structSize = Network::ChannelRegistrationSize, .localChannelId = 1,
            .compatibilityToken = 0x5354415455450003ULL, .hostMessage = ReceiveOpen,.clientMessage=ReceiveOpened,
        };
        if (network->registerChannel(context, &registration, &channel) != Network::Result::Success) channel = 0;
    } else network = nullptr;
    const ItemInteractions::ItemInteractionListener click{
        .structSize = ItemInteractions::ItemInteractionListenerSize, .priority = 200,
        .callback = Activate, .containerMask = supportedStorage,
    };
    const SharedEvents::ItemTooltipListener description{
        .structSize = SharedEvents::ItemTooltipListenerSize, .priority = 0, .slot = 0,
        .region = SharedEvents::ItemTooltipRegion::ActionFooter,
        .position = SharedEvents::ItemTooltipPosition::Bottom, .callback = Describe,
    };
    const Lifecycle::GameplayEventListener left{
        .structSize = Lifecycle::GameplayEventListenerSize, .kind = Lifecycle::GameplayEventKind::GameLeft, .callback = GameState,
    };
    const Lifecycle::GameplayEventListener ready{
        .structSize = Lifecycle::GameplayEventListenerSize, .kind = Lifecycle::GameplayEventKind::LocalPlayerReady, .callback = GameState,
    };
    if (interactions->registerListener(context, &click, &activation) != ItemInteractions::Result::Success
        || events->registerItemTooltipListener(context, &description, &tooltip) != SharedEvents::Result::Success
        || lifecycle->registerGameplayEventListener(context, &left, &leftGame) != Lifecycle::Result::Success
        || lifecycle->registerGameplayEventListener(context, &ready, &playerReady) != Lifecycle::Result::Success) {
        context->LogError("Collector Statues could not register its opening and tooltip callbacks.");
        Cleanup(); return false;
    }
    (void)CollectorNative::Install(context);
    soundReady.store(CollectorSound::Install(context));
    (void)CollectorDrops::Install(context);
    return true;
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept { Cleanup(); }
