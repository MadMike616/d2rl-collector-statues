// Exercise the real callbacks with SDK doubles; no game process is started.
#include "../src/plugin.cpp"
#include <cstdio>
#include <cstdlib>

namespace {
int edits{}, warnings{},infoLines{};
unsigned soundCount{},ackCount{};
uint64_t ackTicket{};
Inventory::Result cursorResult{Inventory::Result::NotFound};
ItemHandle cursorItem{};
bool missing{}, rejectOwner{}, gameThreadAvailable{true},inGame{},duplicateCell{},missingClass{};
Items::ItemInfo fixture{},serverFixture{};
Items::ItemEdit lastEdit{};
Threads::Callback scheduled{};
void* scheduledData{};
Threads::Callback uiScheduled{};
void* uiData{};
unsigned uiSchedules{};
bool uiAvailable{true};
void Check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
Items::Result __cdecl GetInfo(const PluginContext*, ItemHandle handle, Items::ItemInfo* output) noexcept {
    if (missing || handle != (inGame?110ULL:10ULL)) return Items::Result::StaleHandle;
    *output = inGame?serverFixture:fixture; return Items::Result::Success;
}
Items::Result __cdecl Edit(const PluginContext*, PlayerHandle player, ItemHandle handle, const Items::ItemEdit* edit) noexcept {
    if (player != 120 || handle != 110 || rejectOwner) return Items::Result::OwnerMismatch;
    ++edits; lastEdit = *edit; serverFixture.stateFlags = fixture.stateFlags = edit->stateFlags; return Items::Result::Success;
}
Threads::Result __cdecl Schedule(const PluginContext*, Threads::Callback callback, void* data) noexcept {
    if (!gameThreadAvailable) return Threads::Result::Unavailable;
    scheduled = callback; scheduledData = data; return Threads::Result::Success;
}
Threads::Result __cdecl ScheduleUi(const PluginContext*,Threads::Callback callback,void* data) noexcept {
    if(!uiAvailable)return Threads::Result::Unavailable;
    ++uiSchedules;uiScheduled=callback;uiData=data;return Threads::Result::Success;
}
Localization::Result __cdecl Text(const PluginContext*, const char* key, char* output, uint32_t capacity, uint32_t* needed) noexcept {
    if (std::strcmp(key, "d2r:GrailerRLCollectorStatueOpen")) return Localization::Result::NotFound;
    constexpr char line[] = "Right-Click to Open";
    *needed = sizeof(line);
    if (capacity < sizeof(line)) return Localization::Result::BufferTooSmall;
    std::memcpy(output, line, sizeof(line)); return Localization::Result::Success;
}
void __cdecl Warn(const PluginContext*, const char*) noexcept { ++warnings; }
void __cdecl InfoLog(const PluginContext*, const char*) noexcept { ++infoLines; }
void SoundFake(const PluginContext*) noexcept {++soundCount;}
Network::Result __cdecl Ack(const PluginContext*,Network::ChannelHandle received,Network::PeerHandle peer,
    uint16_t id,const void* data,uint32_t size) noexcept {
    Check(received==1&&peer==30&&id==2&&size==sizeof(uint64_t),"only the opening peer receives the success acknowledgement");
    std::memcpy(&ackTicket,data,size);++ackCount;return Network::Result::Success;
}
Network::Result __cdecl PeerPlayer(const PluginContext*, Network::ChannelHandle, Network::PeerHandle peer, PlayerHandle* player) noexcept {
    if (peer != 30) return Network::Result::NotFound;
    *player = 120; return Network::Result::Success;
}
Inventory::Result __cdecl LocalPlayer(const PluginContext*,PlayerHandle* player) noexcept {*player=inGame?120:20;return Inventory::Result::Success;}
Inventory::Result __cdecl Cursor(const PluginContext*,PlayerHandle,ItemHandle* item) noexcept {*item=cursorItem;return cursorResult;}
Inventory::Result __cdecl Each(const PluginContext* context, PlayerHandle player, const Inventory::ItemFilter*,
    Inventory::ItemCallback callback, void* data) noexcept {
    if (player != (inGame?120ULL:20ULL)) return Inventory::Result::OwnerMismatch;
    if(!missing)(void)callback(context, inGame?&serverFixture:&fixture, data);
    if(duplicateCell)(void)callback(context,inGame?&serverFixture:&fixture,data);
    return Inventory::Result::Success;
}
ItemService fakeItems{.serviceSize = ItemServiceSize, .serviceVersion = ItemService::AbiVersion,
    .getItemInfo = GetInfo, .editItem = Edit};
ThreadService fakeThreads{.serviceSize = ThreadServiceSize, .serviceVersion = ThreadService::AbiVersion, .runOnUiThread=ScheduleUi, .runOnGameThread = Schedule};
LocalizationService fakeText{.serviceSize = LocalizationServiceSize, .serviceVersion = LocalizationService::AbiVersion, .getStringByKey = Text};
NetworkService fakeNetwork{.serviceSize = NetworkServiceSize, .serviceVersion = NetworkService::AbiVersion,.sendToClient=Ack, .getPeerPlayer = PeerPlayer};
InventoryService fakeInventory{.serviceSize = InventoryServiceSize, .serviceVersion = InventoryService::AbiVersion,
    .getLocalPlayer=LocalPlayer,.getCursorItem=Cursor,.forEachInventoryItem = Each};
PluginApi api{.apiSize = PluginApiSize, .logInfo=InfoLog, .logWarn = Warn};
PluginContext context{.contextSize = PluginContextSize, .api = &api};
ItemInteractions::ItemInteractionEvent click{
    .structSize = ItemInteractions::ItemInteractionEventSize, .player = 20, .item = 10,
    .action = ItemInteractions::Action::Activate, .inputSource = ItemInteractions::InputSource::KeyboardMouse,
    .container = Items::ItemContainer::Inventory,
};
void Reset() {
    items = &fakeItems; threads = &fakeThreads; localization = &fakeText;
    network = &fakeNetwork; inventory = &fakeInventory; supportedStorage = Storage;
    edits = warnings = 0; missing = rejectOwner = inGame = duplicateCell = missingClass = false; gameThreadAvailable = true;
    pendingCount = 0; pumpQueued = false; scheduled = nullptr;
    uiScheduled=nullptr;uiData=nullptr;uiSchedules=0;uiAvailable=true;
    hovered={};mouseRequest={};mouseUiQueued=false;ownsRightUp=false;
    ResetFeedback();soundReady.store(false);soundPlayer=SoundFake;soundCount=ackCount=0;ackTicket=0;
    infoLines=0;cursorResult=Inventory::Result::NotFound;cursorItem=0;
    fixture = {.structSize = Items::ItemInfoSize, .handle = 10, .code = Items::MakeItemCode("s01"),
        .classId=42,.runtimeId = 100, .container = Items::ItemContainer::Inventory,.x=3,.y=4,.quality = Items::Quality::Unique,
        .itemLevel = 99, .quantity = 1, .generationSeed = 123, .itemSeed = 456,
        .stateFlags = Items::ItemStateEthereal};
    serverFixture=fixture;serverFixture.handle=110;serverFixture.runtimeId=999;
    serverFixture.generationSeed=789;serverFixture.itemSeed=987;
    click.modifiers = 0; click.container = Items::ItemContainer::Inventory;
    click.inputSource=ItemInteractions::InputSource::Controller;
}
void Run() { Check(scheduled != nullptr, "work scheduled"); auto work = scheduled; scheduled = nullptr;inGame=true; work(&context, scheduledData);inGame=false; }
void UiStep(uint64_t now,const CollectorNative::InventoryInputState& input,bool target=true) {
    Check(uiScheduled==MouseOpen,"retry is queued through the SDK UI callback");
    const auto serial=static_cast<uint64_t>(reinterpret_cast<uintptr_t>(uiData));uiScheduled=nullptr;
    AdvanceMouseRequest(&context,serial,now,input,target);
}
bool ClickAt(uint64_t now) {
    std::lock_guard lock(hoverMutex);return QueueMouseClick(&context,hovered,now);
}
void EnableSound() {soundReady.store(true);feedbackGameReady.store(true);}
void RunSound() {
    Check(uiScheduled==PlayFeedback,"successful identification queues a UI sound callback");
    const auto callback=uiScheduled;uiScheduled=nullptr;callback(&context,uiData);
}
DataTables::Result __cdecl FindClass(const PluginContext*,DataTables::Bank bank,DataTables::TableId id,uint32_t code,DataTables::RowView* row) noexcept {
    if(missingClass&&(code&0xffffffu)==(Items::MakeItemCode("s20")&0xffffffu))return DataTables::Result::NotFound;
    const auto tens=(code>>8)&255u,ones=(code>>16)&255u;
    row->tableId=id;row->bank=bank;row->rowIndex=static_cast<uint32_t>(bank)*100+(tens-'0')*10+ones-'0';
    return DataTables::Result::Success;
}
}

int main() {
    constexpr CollectorNative::InventoryInputState busy{true,true,0x1002},ready{true,false,0x1002};
    Reset();RememberHover(&context,fixture,ready,{12,34},1000);
    RememberHover(&context,fixture,busy,{12,34},1453);
    Check(hovered.valid&&hovered.tick==1453&&FreshHover(hovered.tick,1453,0,0),
        "busy inventory refreshes a visible box hover instead of expiring it at 453 ms");
    Check(ClickAt(1453)&&mouseUiQueued&&uiSchedules==1,"first click is retained even while inventory is busy");
    const auto firstSerial=mouseRequest.serial;
    Check(ClickAt(1490)&&uiSchedules==1&&mouseRequest.serial==firstSerial&&mouseRequest.clicked==1453,
        "extra clicks do not stack callbacks or extend the wait");
    UiStep(1460,busy);Check(mouseUiQueued&&uiSchedules==2&&!scheduled&&edits==0,
        "busy UI queues a later update without identifying or falling through");
    UiStep(1480,ready);Check(!mouseUiQueued&&scheduled&&edits==0,"the retained first click resumes when input is ready");
    Run();Check(edits==1,"one physical click opens the original box once after the busy flag clears");
    Reset();RememberHover(&context,fixture,ready,{12,34},1000);ClickAt(1000);
    UiStep(1001,ready);Run();Check(edits==1&&uiSchedules==1,"ready inventory still opens without a retry");
    Reset();RememberHover(&context,fixture,busy,{12,34},1000);ClickAt(1000);
    UiStep(1000+MouseOpenTimeoutMs+1,busy);
    Check(!mouseUiQueued&&!scheduled&&edits==0&&warnings==1,"permanently busy input times out with no mutation");
    Reset();RememberHover(&context,fixture,busy,{12,34},1000);ClickAt(1000);mouseRequest.attempts=MouseOpenMaxAttempts;
    UiStep(1001,busy);Check(!mouseUiQueued&&!scheduled&&warnings==1,"retry count is bounded even if time does not advance");
    Reset();RememberHover(&context,fixture,busy,{12,34},1000);ClickAt(1000);
    UiStep(1001,ready,false);Check(!mouseUiQueued&&!scheduled,"moving away cancels a retained click");
    Reset();RememberHover(&context,fixture,busy,{12,34},1000);ClickAt(1000);
    UiStep(1001,{true,false,0});Check(!mouseUiQueued&&!scheduled,"closing storage cancels a retained click");
    Reset();RememberHover(&context,fixture,busy,{12,34},1000);ClickAt(1000);++fixture.x;
    UiStep(1001,ready);Check(!mouseUiQueued&&!scheduled,"moving the item cancels a retained click");
    Reset();RememberHover(&context,fixture,busy,{12,34},1000);ClickAt(1000);
    cursorResult=Inventory::Result::Success;cursorItem=10;
    UiStep(1001,ready);Check(!mouseUiQueued&&!scheduled,"an occupied cursor cancels a retained click");
    Reset();RememberHover(&context,fixture,busy,{12,34},1000);ClickAt(1000);const auto oldSerial=mouseRequest.serial;
    CancelMouseRequest(oldSerial);ClickAt(1020);const auto newSerial=mouseRequest.serial;
    AdvanceMouseRequest(&context,oldSerial,1021,ready,true);
    Check(mouseUiQueued&&mouseRequest.serial==newSerial&&!scheduled,"stale callback cannot consume a newer click");
    UiStep(1022,ready);Run();Check(edits==1,"new click survives a discarded old callback");
    Reset();RememberHover(&context,fixture,busy,{12,34},1000);uiAvailable=false;
    Check(!ClickAt(1000)&&!mouseUiQueued&&!scheduled,"unavailable UI service does not leave a stuck request");
    Reset();EnableSound();Activate(&context,&click,nullptr);
    Check(!soundCount&&!uiScheduled,"clicking does not play sound before authoritative success");
    Run();Check(edits==1&&!soundCount,"game callback schedules sound instead of playing on the server thread");
    RunSound();Check(soundCount==1,"confirmed local opening sounds exactly once");
    Check(Activate(&context,&click,nullptr)==ItemInteractions::Decision::Continue&&!uiScheduled,"opened statue cannot sound again");
    Reset();EnableSound();Activate(&context,&click,nullptr);Activate(&context,&click,nullptr);Run();RunSound();
    Check(edits==1&&soundCount==1,"duplicate requests do not duplicate opening sounds");
    Reset();EnableSound();Activate(&context,&click,nullptr);rejectOwner=true;Run();
    Check(!edits&&!soundCount&&!uiScheduled,"failed edit remains silent");
    Reset();EnableSound();Activate(&context,&click,nullptr);++serverFixture.x;Run();
    Check(!edits&&!uiScheduled,"moved or stale server item remains silent");
    Reset();EnableSound();Activate(&context,&click,nullptr);Run();ResetFeedback();RunSound();
    Check(edits==1&&!soundCount,"queued success sound is discarded after leaving the game");
    Reset();EnableSound();channel=1;const auto ticket=RegisterFeedback();
    ReceiveOpened(&context,2,2,&ticket,sizeof(ticket),nullptr);
    auto unknown=ticket+1000;ReceiveOpened(&context,1,2,&unknown,sizeof(unknown),nullptr);
    Check(!uiScheduled&&!soundCount,"foreign or unknown success acknowledgements cannot play sound");
    ReceiveOpened(&context,1,2,&ticket,sizeof(ticket),nullptr);ReceiveOpened(&context,1,2,&ticket,sizeof(ticket),nullptr);
    Check(uiSchedules==1,"duplicate acknowledgements queue only one sound");RunSound();
    ReceiveOpened(&context,1,2,&ticket,sizeof(ticket),nullptr);Check(soundCount==1&&!uiScheduled,"consumed sound ticket cannot be replayed");
    Reset();EnableSound();channel=1;
    const OpenMessage remote{fixture.code,fixture.classId,fixture.container,fixture.inventoryPage,fixture.sharedStashPage,fixture.x,fixture.y,0x12345678u,9};
    ReceiveOpen(&context,1,30,1,&remote,sizeof(remote),nullptr);Run();
    Check(edits==1&&ackCount==1&&ackTicket==0x912345678ULL&&!soundCount&&!uiScheduled,
        "host acknowledges the opening client without playing that client's sound locally");
    Reset();EnableSound();const auto oldTicket=RegisterFeedback();ResetFeedback();EnableSound();const auto currentTicket=RegisterFeedback();
    ReceiveOpened(&context,channel,2,&oldTicket,sizeof(oldTicket),nullptr);
    Check(!uiScheduled&&currentTicket!=oldTicket,"old-session sound ticket cannot acknowledge a new game");
    Reset();click.inputSource=ItemInteractions::InputSource::KeyboardMouse;
    Check(Activate(&context,&click,nullptr)==ItemInteractions::Decision::Continue&&!scheduled&&edits==0,
        "generic mouse activation/left-click cannot open or consume a box");
    click.inputSource=ItemInteractions::InputSource::Unknown;
    Check(Activate(&context,&click,nullptr)==ItemInteractions::Decision::Continue&&!scheduled,
        "unknown activation source cannot open a box");
    Check(CollectorNative::InventoryInputAllowed(true,false,0x1002),
        "observed normal inventory with mode 0x0c permits right-click");
    Check(!CollectorNative::InventoryInputAllowed(false,false,2)
        &&!CollectorNative::InventoryInputAllowed(true,true,2)
        &&!CollectorNative::InventoryInputAllowed(true,false,2|(1u<<0x0b))
        &&!CollectorNative::InventoryInputAllowed(true,false,2|(1u<<0x17))
        &&!CollectorNative::InventoryInputAllowed(true,false,0x1000),
        "unadmitted, blocked, configuration, trade and closed storage remain rejected");
    Reset();const HoverSnapshot rightClick{fixture,20,1000,{12,34},true};
    OpenHovered(&context,rightClick,CollectorNative::InventoryInputAllowed(true,false,0x1002));Run();
    Check(edits==1,"right-button UI revalidation opens one owned box through the authoritative pump");
    Reset();OpenHovered(&context,{fixture,20,1000,{12,34},true},false);
    Check(!scheduled&&edits==0,"UI state becoming blocked cancels the right-click");
    Reset();cursorResult=Inventory::Result::Success;cursorItem=10;
    OpenHovered(&context,{fixture,20,1000,{12,34},true},true);
    Check(!scheduled&&edits==0,"occupied cursor cancels the right-click");
    Reset();auto moved=HoverSnapshot{fixture,20,1000,{12,34},true};++fixture.x;
    OpenHovered(&context,moved,true);Check(!scheduled&&edits==0,"moving a hovered box cancels opening");
    Reset();moved={fixture,20,1000,{12,34},true};++fixture.itemSeed;
    OpenHovered(&context,moved,true);Check(!scheduled&&edits==0,"replaced hovered item cannot be opened");
    Reset();
    Check(Activate(&context, &click, nullptr) == ItemInteractions::Decision::Consume && edits == 0, "UI activation queues work without mutating");
    const auto before = serverFixture;
    Run();
    Check(edits == 1 && lastEdit.fields == Items::EditFieldBit(Items::EditField::Identified), "only identification is edited");
    Check(serverFixture.stateFlags == (before.stateFlags | Items::ItemStateIdentified)
        && serverFixture.code == before.code && serverFixture.runtimeId == before.runtimeId
        && serverFixture.generationSeed == before.generationSeed && serverFixture.itemSeed == before.itemSeed,
        "authoritative item identity and other flags survive distinct UI handles/IDs/seeds");
    Check(Activate(&context, &click, nullptr) == ItemInteractions::Decision::Continue, "opened statue cannot be opened twice");

    Reset(); Activate(&context, &click, nullptr); Activate(&context, &click, nullptr); Run();
    Check(edits == 1, "rapid duplicate activation identifies only once");
    Reset(); Activate(&context, &click, nullptr); missing = true; Run(); Check(edits == 0, "stale item is untouched");
    Reset(); Activate(&context, &click, nullptr); ++serverFixture.x; Run(); Check(edits == 0, "moved item is untouched");
    Reset(); Activate(&context, &click, nullptr); serverFixture.container = Items::ItemContainer::Trade; Run(); Check(edits == 0, "item moved to trade is untouched");
    Reset(); Activate(&context, &click, nullptr); duplicateCell=true; Run();Check(edits==0,"ambiguous storage lookup is rejected");
    Reset(); Activate(&context, &click, nullptr); rejectOwner = true; Run(); Check(edits == 0 && warnings == 1, "owner mismatch leaves item untouched");
    Reset(); fixture.code = Items::MakeItemCode("cm1");
    Check(Activate(&context, &click, nullptr) == ItemInteractions::Decision::Continue, "ordinary unidentified charm is unaffected");
    Reset(); fixture.quality = Items::Quality::Normal;
    Check(Activate(&context, &click, nullptr) == ItemInteractions::Decision::Continue, "non-unique lookalike is unaffected");
    Reset(); click.modifiers = ItemInteractions::ModifierBit(ItemInteractions::Modifier::Control);
    Check(Activate(&context, &click, nullptr) == ItemInteractions::Decision::Continue, "modified movement gesture is unaffected");
    Reset(); click.container = Items::ItemContainer::Ground;
    Check(Activate(&context, &click, nullptr) == ItemInteractions::Decision::Continue, "ground interaction is unaffected");
    Reset(); for (unsigned n = 0; n <= 21; ++n) {
        const auto code = Items::MakeItemCode('s', static_cast<char>('0' + n / 10), static_cast<char>('0' + n % 10));
        Check(StatueCode(code) == (n >= 1 && n <= 20), "exactly the twenty statue codes are accepted");
    }
    Reset(); char buffer[128]{};
    SharedEvents::ItemTooltipEvent text{.structSize = SharedEvents::ItemTooltipEventSize, .item = 10, .text = buffer, .capacity = sizeof(buffer)};
    Describe(&context, &text, nullptr);
    Check(text.length == 19 && !std::strcmp(buffer, "Right-Click to Open"), "unopened box gets localized action text");
    fixture.stateFlags |= Items::ItemStateIdentified; text.length = 0; buffer[0] = 0;
    Describe(&context, &text, nullptr); Check(text.length == 0, "opening instruction disappears after identification");

    Reset(); const OpenMessage message{fixture.code,fixture.classId,fixture.container,fixture.inventoryPage,fixture.sharedStashPage,fixture.x,fixture.y};
    channel = 1; ReceiveOpen(&context, channel, 30, 1, &message, sizeof(message), nullptr); Run();
    Check(edits == 1, "host opens matching item owned by sending peer");
    Reset(); ReceiveOpen(&context, channel, 31, 1, &message, sizeof(message), nullptr); Run();
    Check(edits == 0, "unknown peer cannot identify someone else's item");
    Reset(); auto forged = message; ++forged.classId;
    ReceiveOpen(&context, channel, 30, 1, &forged, sizeof(forged), nullptr); Run();
    Check(edits == 0, "forged or stale peer identity cannot select an item");
    Reset(); Activate(&context, &click, nullptr);
    const Lifecycle::GameplayEvent leave{.structSize = Lifecycle::GameplayEventSize, .kind = Lifecycle::GameplayEventKind::GameLeft};
    GameState(&context, &leave, nullptr); Run(); Check(edits == 0 && pendingCount == 0, "game exit clears queued openings");
    Reset();RememberHover(&context,fixture,busy,{12,34},1000);ClickAt(1000);const auto leftSerial=mouseRequest.serial;
    GameState(&context,&leave,nullptr);AdvanceMouseRequest(&context,leftSerial,1001,ready,true);
    Check(!mouseUiQueued&&!scheduled&&edits==0,"game exit discards retained mouse clicks and their callbacks");
    Reset();Check(CursorEmpty(&context,20),"NotFound cursor permits opening");
    cursorResult=Inventory::Result::Success;Check(CursorEmpty(&context,20),"successful empty cursor permits opening");
    cursorItem=10;Check(!CursorEmpty(&context,20),"occupied cursor blocks opening");
    cursorItem=0;cursorResult=Inventory::Result::Unsupported;
    Check(!CursorEmpty(&context,20),"unknown cursor state fails closed");
    Check(FreshHover(1000,1400,4,-4)&&!FreshHover(1000,1401,0,0)&&!FreshHover(1000,1100,5,0)&&!FreshHover(1000,999,0,0),"mouse target must be fresh and unmoved");
    DataTableService fakeTables{.serviceSize=DataTableServiceSize,.serviceVersion=DataTableService::AbiVersion,.findRowByCode=FindClass};
    CollectorNative::RefreshClasses(&context,&fakeTables);
    for(unsigned bank=1;bank<=3;++bank)for(unsigned cls=0;cls<400;++cls)
        Check(CollectorNative::HideUnidentified(3455,bank,cls)==(cls>bank*100&&cls<=bank*100+20),"unidentified suppression is isolated to each bank's twenty statue classes");
    Check(!CollectorNative::HideUnidentified(3454,3,301)&&!CollectorNative::HideUnidentified(3455,0,301),"other strings and invalid banks retain original behavior");
    missingClass=true;CollectorNative::RefreshClasses(&context,&fakeTables);
    Check(!CollectorNative::HideUnidentified(3455,3,301),"incomplete class lookup fails closed");
    Check(!std::strcmp(D2RLoaderGetPluginInfo()->description,"Handles collection statue logic"),"extension description matches requested wording");
    std::puts("PASS: UI/server domain separation, identity preservation, moved/foreign/ambiguous items, input isolation, tooltip lifecycle, mouse freshness, bank-specific label suppression and game exit.");
}
