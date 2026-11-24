#include "statue_drops.h"
#include "drop_catalog.h"
#include "drop_profile.h"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <random>

extern "C" {
uintptr_t CollectorStatuesDropEntry{},CollectorStatuesDropReturn{};
void CollectorStatuesDeath();
}

namespace {
using namespace D2RL;
using namespace CollectorDrops;
const PluginContext* dropOwner{};
const InventoryService* dropInventory{};
const ThreadService* dropThreads{};
const DataTableService* dropTables{};
std::atomic<bool> installed{};
uintptr_t activeGameSlot{};
bool(__fastcall* singlePlayer)(){};
void*(__fastcall* findServerUnit)(void*,unsigned,unsigned){};
void*(__fastcall* unitRoom)(void*){};
unsigned(__fastcall* roomLevel)(void*){};
unsigned(__fastcall* localContext)(){};
void*(__fastcall* localPlayer)(unsigned){};
using CreateItemFn=void*(__fastcall*)(void*,uint32_t,void*,int,int,int,int,int,int,int,int);
using PlaceItemFn=void(__fastcall*)(void*,void*,void*,void*,int,int);
CreateItemFn createNativeItem{};
PlaceItemFn placeNativeItem{};
void(__fastcall* setNativeItemFlag)(void*,uint32_t,int){};
int(__fastcall* roomContains)(void*,int,int){};

struct Bank {
    bool ready{};
    std::array<unsigned,CollectorStatues::Count> statueClasses{};
    std::map<unsigned,unsigned> checkpointFloors;
};
struct Catalog { std::map<unsigned,Area> areas;std::array<Bank,3> banks; };
std::atomic<std::shared_ptr<const Catalog>> catalog;
std::mt19937_64 randomEngine;
std::mutex randomMutex;
std::atomic<uint64_t> epoch{1};
#ifdef COLLECTOR_STATUES_UNIT_TEST
std::array<std::atomic<uint64_t>,4> eligible{},won{},created{};
std::atomic<uint64_t> failed{};
#endif
// Unit-test observations compile out of the plugin; they do not log or retain
// gameplay metrics in the installed DLL.
enum class Observation { Eligible, Won, Created, Failed };
inline void Observe(Observation event,Category category=Category::Descent) noexcept {
#ifdef COLLECTOR_STATUES_UNIT_TEST
    const auto index=static_cast<unsigned>(category);
    switch(event) {
    case Observation::Eligible:++eligible[index];break;
    case Observation::Won:++won[index];break;
    case Observation::Created:++created[index];break;
    case Observation::Failed:++failed;break;
    }
#else
    (void)event;(void)category;
#endif
}


template<class T> bool Read(const void* pointer,size_t offset,T& value) noexcept {
    if(!pointer)return false;
    SIZE_T copied{};
    return ReadProcessMemory(GetCurrentProcess(),static_cast<const char*>(pointer)+offset,&value,sizeof(T),&copied)
        &&copied==sizeof(T);
}
struct UnitHead { uint32_t type{},classId{},id{}; };
bool Head(const void* unit,UnitHead& head) noexcept {return Read(unit,0,head)&&head.type<=5;}

template<class Function> Function Native(uintptr_t rva) noexcept {
    return reinterpret_cast<Function>(dropOwner->exeBase+rva);
}
bool Offline() noexcept {
    return singlePlayer&&singlePlayer();
}
void* FindUnit(void* game,unsigned type,unsigned id) noexcept {
    return findServerUnit(game,type,id);
}
unsigned Level(void* unit) noexcept {
    void* path{};
    if(!Read(unit,0x38,path)||!path)return 0;
    auto room=unitRoom(unit);
    return room?roomLevel(room):0;
}
void* PlayerOwner(void* unit,void* game) noexcept {
    std::array<void*,8> visited{};
    for(size_t depth=0;unit&&depth<visited.size();++depth) {
        if(std::find(visited.begin(),visited.begin()+depth,unit)!=visited.begin()+depth)return nullptr;
        visited[depth]=unit;
        UnitHead head;
        if(!Head(unit,head))return nullptr;
        if(head.type==0)return FindUnit(game,0,head.id)==unit?unit:nullptr;
        if(head.type!=1)return nullptr;
        // The guarded native ownership helper documents these fields. Read
        // defensively instead of invoking its assertions for unowned monsters.
        void* data{};void* ownerData{};void* ownerGame{};
        uint32_t ownerId{},ownerType{};
        if(!Read(unit,0x10,data)||!Read(data,0x30,ownerData)||!Read(ownerData,0x60,ownerGame)
            ||ownerGame!=game||!Read(ownerData,0x68,ownerId)||!Read(ownerData,0x6c,ownerType)||ownerType>1)
            return nullptr;
        unit=FindUnit(game,ownerType,ownerId);
    }
    return nullptr;
}
void* LocalServerPlayer(void* game) noexcept {
    const auto index=localContext();
    if(index>=8)return nullptr;
    auto client=localPlayer(index);
    UnitHead head;
    if(!Head(client,head)||head.type!=0)return nullptr;
    auto server=FindUnit(game,0,head.id);
    UnitHead serverHead;
    return Head(server,serverHead)&&serverHead.type==0&&serverHead.classId==head.classId?server:nullptr;
}

struct Award {
    void* game{};
    uint64_t generation{};
    unsigned level{},x{},y{},statue{};
    Category category{};
    unsigned sourceId{UINT32_MAX},sourceClass{UINT32_MAX};
};
std::mutex queueMutex;
std::array<Award,(CollectorStatues::Count*2>32?CollectorStatues::Count*2:32)> queue;
size_t queueSize{};
bool queued{};

struct Location { void* room{};unsigned x{},y{};bool fallback{}; };
bool ValidLocation(void* room,unsigned level,unsigned x,unsigned y) noexcept {
    return room&&x&&y&&x<=UINT16_MAX&&y<=UINT16_MAX
        &&roomLevel(room)==level&&roomContains(room,static_cast<int>(x),static_cast<int>(y))!=0;
}
bool Position(void* unit,unsigned& x,unsigned& y) noexcept {
    void* path{};uint16_t px{},py{};
    if(!Read(unit,0x38,path)||!Read(path,2,px)||!Read(path,6,py)||!px||!py)return false;
    x=px;y=py;return true;
}
bool ResolveLocation(const Award& award,void* player,Location& result) noexcept {
    // Resolve fresh server units/rooms in this callback. Do not retain a room
    // pointer from the death hook across the queued update.
    if(award.sourceId!=UINT32_MAX) {
        auto source=FindUnit(award.game,1,award.sourceId);UnitHead head;
        if(Head(source,head)&&head.type==1&&head.id==award.sourceId&&head.classId==award.sourceClass) {
            auto room=unitRoom(source);
            if(ValidLocation(room,award.level,award.x,award.y)) {result={room,award.x,award.y,false};return true;}
        }
    }
    auto room=unitRoom(player);
    if(ValidLocation(room,award.level,award.x,award.y)) {result={room,award.x,award.y,false};return true;}
    unsigned x{},y{};
    if(Position(player,x,y)&&ValidLocation(room,award.level,x,y)) {result={room,x,y,true};return true;}
    return false;
}
enum class SpawnResult { Success,NoItem,InvalidItem,IdentificationFailed,PlacementFailed,NativeException };
__declspec(noinline) SpawnResult SpawnNative(void* game,void* player,unsigned cls,const Location& at) noexcept {
    // POD-only SEH boundary. These native helpers are byte-validated at load.
    __try {
        auto item=createNativeItem(player,cls,game,4,7,1,1,99,0,0,0);
        if(!item)return SpawnResult::NoItem;
        UnitHead head;void* data{};uint32_t quality{},flags{};
        if(!Head(item,head)||head.type!=4||head.classId!=cls||FindUnit(game,4,head.id)!=item
            ||!Read(item,0x10,data)||!Read(data,0,quality)||quality!=7||!Read(data,0x18,flags))return SpawnResult::InvalidItem;
        // The native creator sets IFLAG_IDENTIFIED (0x10). Clear it before
        // room insertion/network publication so the client receives a box.
        setNativeItemFlag(item,0x10,0);
        if(!Read(data,0x18,flags)||(flags&0x10))return SpawnResult::IdentificationFailed;
        placeNativeItem(game,nullptr,item,at.room,static_cast<int>(at.x),static_cast<int>(at.y));
        void* path{};void* actualRoom{};int x{},y{};
        if(!Read(item,0x38,path)||!Read(path,0,actualRoom)||actualRoom!=at.room
            ||!Read(path,0x10,x)||!Read(path,0x14,y)||x!=static_cast<int>(at.x)||y!=static_cast<int>(at.y)
            ||!Read(data,0,quality)||quality!=7||!Read(data,0x18,flags)||(flags&0x10)
            ||!roomContains(actualRoom,x,y))return SpawnResult::PlacementFailed;
        return SpawnResult::Success;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return SpawnResult::NativeException;}
}

// Called only inside the SDK's authoritative game callback. Never mutate
// native loot tables, the game's RNG, or the monster's existing treasure.
void Create(const PluginContext* c,const Award& award) noexcept {
    if(!installed.load())return;
    void* active{};
    if(award.generation!=epoch.load()||!Offline()
        ||!Read(reinterpret_cast<void*>(activeGameSlot),0,active)||active!=award.game)return;
    auto nativePlayer=LocalServerPlayer(active);
    // Ground coordinates must resolve in the victim's level. If the player
    // left that level before this queued update, do not drop into another map.
    if(!nativePlayer||Level(nativePlayer)!=award.level) {Observe(Observation::Failed);return;}
    const auto data=catalog.load();uint8_t bank{};
    const auto statueIndex=CollectorStatues::IndexById(award.statue);
    if(!data||statueIndex==CollectorStatues::Count||!Read(active,0x106,bank)||bank<1||bank>3||!data->banks[bank-1].ready) {Observe(Observation::Failed);return;}
    PlayerHandle player{};
    if(dropInventory->getLocalPlayer(c,&player)!=Inventory::Result::Success) {Observe(Observation::Failed);return;}
    Location location;
    if(!ResolveLocation(award,nativePlayer,location)) {
        Observe(Observation::Failed);c->LogWarn("Collector Statues: ground drop skipped; no current room contains a valid drop position.");return;
    }
    const auto result=SpawnNative(active,nativePlayer,data->banks[bank-1].statueClasses[statueIndex],location);
    if(result==SpawnResult::Success) {
        Observe(Observation::Created,award.category);
    } else {
        Observe(Observation::Failed);
        char message[192]{};
        std::snprintf(message,sizeof(message),"Collector Statues: statue %u native ground creation failed (stage %u, level %u).",
            award.statue,static_cast<unsigned>(result),award.level);
        c->LogWarn(message);
        if(result!=SpawnResult::NoItem) {
            installed.store(false);
            c->LogError("Collector Statues: further drops disabled after an unexpected native item state; restart after checking the drop data and log.");
        }
    }
}
void __cdecl PumpDrops(const PluginContext* c,void*) noexcept {
    for(;;) {
        Award award;
        {
            std::lock_guard lock(queueMutex);
            if(!queueSize) {queued=false;return;}
            award=queue[0];
            for(size_t i=1;i<queueSize;++i)queue[i-1]=queue[i];
            --queueSize;
        }
        Create(c,award);
    }
}
void QueueAward(const Award& award) noexcept {
    std::lock_guard lock(queueMutex);
    if(queueSize==queue.size()) {Observe(Observation::Failed);return;}
    queue[queueSize++]=award;
    if(queued)return;
    if(dropThreads->runOnGameThread(dropOwner,PumpDrops,nullptr)==Threads::Result::Success)queued=true;
    else {--queueSize;Observe(Observation::Failed);}
}


std::filesystem::path LevelFile(const PluginContext* c) {
    for(const auto root:{c->modDirectory,c->scopeRootDirectory}) {
        if(!root||!*root)continue;
        const std::filesystem::path base(root);
        const auto direct=base/L"data/global/excel/levels.txt";
        if(std::filesystem::is_regular_file(direct))return direct;
        if(c->activeMod&&*c->activeMod) {
            const auto mod=std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(c->activeMod)));
            const auto package=std::filesystem::path(mod.native()+L".mpq");
            for(const auto candidate:{base/package,base/mod/package,base/L"mods"/mod/package}) {
                const auto nested=candidate/L"data/global/excel/levels.txt";
                if(std::filesystem::is_regular_file(nested))return nested;
            }
        }
    }
    throw std::runtime_error("active mod levels.txt not found");
}
void __cdecl Refresh(const PluginContext* c,const Lifecycle::DataTablesLoadedEvent*,void*) noexcept {
    catalog.store(nullptr);
    try {
        auto next=std::make_shared<Catalog>();
        std::ifstream input(LevelFile(c));
        next->areas=ReadAreas(input);
        std::ifstream uniques(LevelFile(c).parent_path()/L"uniqueitems.txt");
        ValidateRepeatableStatues(uniques);
        unsigned readyCount{};
        for(unsigned bank=1;bank<=3;++bank) {
            auto& current=next->banks[bank-1];
            bool valid=true;
            const auto dataBank=static_cast<DataTables::Bank>(bank);
            for(size_t index=0;index<CollectorStatues::Count;++index) {
                DataTables::RowView row{.structSize=DataTables::RowViewSize};
                const auto code=CollectorStatues::Definitions[index].code;
                auto found=dropTables->findRowByCode(c,dataBank,DataTables::TableId::Items,code,&row);
                if(found==DataTables::Result::NotFound&&(code>>24)==' ')
                    found=dropTables->findRowByCode(c,dataBank,DataTables::TableId::Items,code&0x00ffffffu,&row);
                if(found!=DataTables::Result::Success||row.bank!=dataBank||row.tableId!=DataTables::TableId::Items) {valid=false;break;}
                current.statueClasses[index]=row.rowIndex;
            }
            if(!valid)continue;
            // MonStats compiled layout: u16 class ID, char identifier[48],
            // total 0x1FC bytes. Match names, not assumed appended row IDs.
            DataTables::TableView monsters{.structSize=DataTables::TableViewSize};
            if(dropTables->getTable(c,dataBank,DataTables::TableId::MonStats,&monsters)!=DataTables::Result::Success
                ||monsters.rowSize!=0x1fc||!monsters.rows||monsters.bank!=dataBank
                ||monsters.tableId!=DataTables::TableId::MonStats||monsters.rowCount>65536)continue;
            std::array<unsigned,6> bosses{};
            for(unsigned i=0;i<monsters.rowCount;++i) {
                const auto row=static_cast<const char*>(monsters.rows)+static_cast<size_t>(i)*monsters.rowSize;
                uint16_t cls{};std::memcpy(&cls,row,2);
                const auto end=static_cast<const char*>(std::memchr(row+2,0,48));
                if(!end)continue;
                const auto floor=CheckpointFloor(std::string_view(row+2,static_cast<size_t>(end-row-2)));
                if(!floor)continue;
                if(cls!=i||!current.checkpointFloors.emplace(cls,floor).second) {valid=false;break;}
                ++bosses[floor/10-1];
            }
            for(auto count:bosses)if(count!=54)valid=false;
            // Confirm each source level ID exists in the active compiled bank.
            for(const auto& [id,area]:next->areas) {
                (void)area;
                DataTables::RowView row{.structSize=DataTables::RowViewSize};
                if(dropTables->findRowById(c,dataBank,DataTables::TableId::Levels,id,&row)!=DataTables::Result::Success
                    ||row.rowSize!=0x18c||row.bank!=dataBank||row.tableId!=DataTables::TableId::Levels) {valid=false;break;}
            }
            current.ready=valid;
            if(valid)++readyCount;
        }
        if(!readyCount)throw std::runtime_error("compiled statue, checkpoint or level tables do not match");
        catalog.store(std::move(next));
        char message[192]{};std::snprintf(message,sizeof(message),"Collector Statues: drop catalog ready (%zu registered statues; 60 floors, six checkpoint sets, five separate acts, Cow Level and eight classes).",CollectorStatues::Count);
        c->LogInfo(message);
    } catch(const std::exception& error) {
        char message[240]{};std::snprintf(message,sizeof(message),"Collector Statues: natural drops disabled: %s.",error.what());
        c->LogWarn(message);
    } catch(...) {c->LogWarn("Collector Statues: natural drop catalog could not be loaded.");}
}

template<size_t N> bool Match(const PluginContext* c,uintptr_t rva,const std::array<uint8_t,N>& bytes) noexcept {
    return c->CheckExpectedBytes(rva,bytes.data(),static_cast<uint32_t>(N));
}
}

extern "C" void __fastcall CollectorStatuesObserveDeath(void* game,void* monster,void* killer,unsigned) noexcept {
    if(!installed.load()||!game||!monster||!Offline())return;
    try {
        const auto data=catalog.load();
        uint8_t bank{},difficulty{};UnitHead head;
        if(!data||!Read(game,0x106,bank)||bank<1||bank>3||!data->banks[bank-1].ready
            ||!Read(game,0x104,difficulty)||difficulty>2||!Head(monster,head)||head.type!=1)return;
        if(PlayerOwner(monster,game))return; // Player-owned pets/mercenaries are not loot targets.
        const auto level=Level(monster);
        const auto found=data->areas.find(level);
        if(found==data->areas.end())return;
        int playerClass=-1;
        if(auto credited=PlayerOwner(killer,game)) {
            UnitHead player;
            if(Head(credited,player)&&player.classId<8)playerClass=static_cast<int>(player.classId);
        }
        const auto& bosses=data->banks[bank-1].checkpointFloors;
        const auto boss=bosses.find(head.classId);
        const Kill kill{found->second,difficulty,playerClass,
            boss!=bosses.end()&&boss->second==found->second.floor&&found->second.floor!=0};
        const auto chances=Eligible(kill);
        for(unsigned i=0;i<chances.count;++i)Observe(Observation::Eligible,chances.values[i].category);
        std::lock_guard lock(randomMutex);
        Roll(kill,[](unsigned denominator) {
            return std::uniform_int_distribution<unsigned>(0,denominator-1)(randomEngine);
        },[&](const Chance& chance) {
            Observe(Observation::Won,chance.category);
            void* path{};uint16_t x{},y{};
            if(!Read(monster,0x38,path)||!Read(path,2,x)||!Read(path,6,y)) {x=0;y=0;}
            Award award{game,epoch.load(),level,x,y,chance.statue,chance.category};
            award.sourceId=head.id;award.sourceClass=head.classId;QueueAward(award);
        });
    } catch(...) {
        // A failed observation must never prevent the original treasure call.
        Observe(Observation::Failed);
    }
}

namespace CollectorDrops {
void GameLeft() noexcept {
    ++epoch;
    {std::lock_guard lock(queueMutex);queueSize=0;queued=false;}
#ifdef COLLECTOR_STATUES_UNIT_TEST
    for(auto& count:eligible)count.store(0);
    for(auto& count:won)count.store(0);
    for(auto& count:created)count.store(0);
    failed.store(0);
#endif
}
void Stop() noexcept {installed.store(false);GameLeft();catalog.store(nullptr);}

bool Install(const PluginContext* c) noexcept {
    using namespace CollectorDropProfile;
    dropOwner=c;
    const MutationService* mutations{};const LifecycleService* life{};
    if(c->QueryService(&dropInventory)!=ServiceQueryResult::Success||!HasInventoryServiceField(dropInventory,InventoryServiceRequiredSize)||!dropInventory->getLocalPlayer
        ||c->QueryService(&dropThreads)!=ServiceQueryResult::Success||!HasThreadServiceField(dropThreads,ThreadServiceRequiredSize)||!dropThreads->runOnGameThread
        ||c->QueryService(&dropTables)!=ServiceQueryResult::Success||!HasDataTableServiceField(dropTables,DataTableServiceRequiredSize)
        ||!dropTables->getTable||!dropTables->findRowByCode||!dropTables->findRowById
        ||c->QueryService(&life)!=ServiceQueryResult::Success||!HasLifecycleServiceField(life,LifecycleServiceRequiredSize)||!life->registerDataTablesLoadedListener
        ||c->QueryService(&mutations)!=ServiceQueryResult::Success||!HasMutationServiceField(mutations,MutationServiceRequiredSize)
        ||!mutations->beginTransaction||!mutations->stageInlineHook||!mutations->commit||!mutations->cancelTransaction) {
        c->LogWarn("Collector Statues: natural drops disabled because required SDK services are unavailable.");return false;
    }
    if(!Match(c,DeathWindowRva,DeathWindow)||!Match(c,GameBankRva,GameBank)||!Match(c,GameDifficultyRva,GameDifficulty)
        ||!Match(c,UnitTypeRva,UnitType)||!Match(c,CollectorProfile::ClassLayoutRva,CollectorProfile::ClassLayout)
        ||!Match(c,UnitRoomRva,UnitRoom)||!Match(c,RoomLevelRva,RoomLevel)||!Match(c,LevelIdRva,LevelId)
        ||!Match(c,DynamicPathRva,DynamicPath)||!Match(c,PathXRva,PathX)||!Match(c,PathYRva,PathY)
        ||!Match(c,SinglePlayerRva,SinglePlayer)||!Match(c,MonsterOwnerRva,MonsterOwner)||!Match(c,FindServerUnitRva,FindServerUnit)
        ||!Match(c,LocalContextRva,LocalContext)||!Match(c,LocalPlayerRva,LocalPlayer)||!Match(c,ActiveGameStoreRva,ActiveGameStore)
        ||!Match(c,CreateItemRva,CreateItem)||!Match(c,PlaceItemRva,PlaceItem)||!Match(c,SetItemFlagRva,SetItemFlag)
        ||!Match(c,ItemQualityRva,ItemQuality)||!Match(c,ItemDataRva,ItemData)||!Match(c,RoomContainsRva,RoomContains)
        ||!Match(c,StaticPathLayoutRva,StaticPathLayout)) {
        c->LogWarn("Collector Statues: natural drops disabled because native death/ownership/ground-placement witnesses did not match.");return false;
    }
    uint64_t seed{};
    if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&seed),sizeof(seed),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0) {
        c->LogWarn("Collector Statues: natural drops disabled because a private random seed was unavailable.");return false;
    }
    randomEngine.seed(seed);
    singlePlayer=Native<decltype(singlePlayer)>(SinglePlayerRva);
    findServerUnit=Native<decltype(findServerUnit)>(FindServerUnitRva);
    unitRoom=Native<decltype(unitRoom)>(UnitRoomRva);
    roomLevel=Native<decltype(roomLevel)>(RoomLevelRva);
    localContext=Native<decltype(localContext)>(LocalContextRva);
    localPlayer=Native<decltype(localPlayer)>(LocalPlayerRva);
    createNativeItem=Native<CreateItemFn>(CreateItemRva);
    placeNativeItem=Native<PlaceItemFn>(PlaceItemRva);
    setNativeItemFlag=Native<decltype(setNativeItemFlag)>(SetItemFlagRva);
    roomContains=Native<decltype(roomContains)>(RoomContainsRva);
    int32_t displacement{};std::memcpy(&displacement,ActiveGameStore.data()+3,4);
    activeGameSlot=c->exeBase+ActiveGameStoreRva+7+displacement;
    CollectorStatuesDropEntry=c->exeBase+0x441300;
    CollectorStatuesDropReturn=c->exeBase+DeathCallRva+5;
    const Lifecycle::DataTablesLoadedListener listener{.structSize=Lifecycle::DataTablesLoadedListenerSize,.callback=Refresh};
    Lifecycle::ListenerHandle handle{};
    if(life->registerDataTablesLoadedListener(c,&listener,&handle)!=Lifecycle::Result::Success)return false;
    Mutations::TransactionHandle transaction{};
    if(mutations->beginTransaction(c,&transaction)!=Mutations::Result::Success)return false;
    const Mutations::InlineHookRequest hook{.structSize=Mutations::InlineHookRequestSize,.rva=DeathCallRva,
        .expected=DeathCall.data(),.expectedSize=static_cast<uint32_t>(DeathCall.size()),.target=reinterpret_cast<void*>(CollectorStatuesDeath)};
    Mutations::OperationHandle operation{};
    if(mutations->stageInlineHook(c,transaction,&hook,&operation)!=Mutations::Result::Success) {
        (void)mutations->cancelTransaction(c,transaction);return false;
    }
    Mutations::CommitResult result{.structSize=Mutations::CommitResultSize};
    if(mutations->commit(c,transaction,&result)!=Mutations::Result::Success) {
        c->LogWarn("Collector Statues: natural drop observer could not be committed; original treasure retained.");return false;
    }
    installed.store(true);
    c->LogInfo("Collector Statues: drops enabled for local offline games.");
    return true;
}
}
