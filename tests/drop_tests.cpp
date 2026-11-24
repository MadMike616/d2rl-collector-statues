// Exercises the real observer, catalog, SDK queue and native creation path with
// native/SDK doubles. Reads active data read-only; never starts the game.
#define COLLECTOR_STATUES_UNIT_TEST
#include "../src/statue_drops.cpp"
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace {
void Check(bool condition,const char* text) {
    if(!condition) {std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}
}
template<class F> void Reject(F&& action,const char* text) {
    try {action();}catch(const std::exception&) {return;}
    Check(false,text);
}
std::vector<char> monstats;
bool badStride{},badBoss{},offline=true,rejectCreation{};
unsigned edits{},warnings{};
struct NativeRecord { uint32_t code{};Items::Quality quality{};uint32_t stateFlags{};struct {unsigned x{},y{};} destination;void* room{}; };
std::vector<NativeRecord> specifications;
struct FakeNativeItem {std::array<char,0x200> unit{};std::array<char,0x100> data{};std::array<char,0x40> staticPath{};};
std::vector<std::unique_ptr<FakeNativeItem>> nativeItems;
unsigned nativeCreates{},nativePlacements{},nativeFlagEdits{};
bool sourceMissing{},separateRooms{},noValidRoom{},badQuality{},badIdentification{},badPlacement{};
Threads::Callback work{};
void* workData{};
bool scheduleAvailable=true;
std::array<char,0x200> game{},source{},server{},client{},pet{},summon{},ownerA{},ownerB{},dataA{},dataB{},path{},playerPath{};
unsigned playerLevel=138,sourceLevel=138;
void* active{};
template<class T,size_t N> void Put(std::array<char,N>& bytes,size_t offset,const T& value) {
    std::memcpy(bytes.data()+offset,&value,sizeof(value));
}
void Identity(std::array<char,0x200>& bytes,unsigned type,unsigned cls,unsigned id) {
    Put(bytes,0,UnitHead{type,cls,id});Put(bytes,0x38,static_cast<void*>((&bytes==&source?path:playerPath).data()));
}
void __cdecl Log(const PluginContext*,const char* text) noexcept {std::puts(text);}
void __cdecl Warn(const PluginContext*,const char* text) noexcept {++warnings;std::puts(text);}
bool __fastcall Single() {return offline;}
unsigned __fastcall ContextIndex() {return 0;}
void* __fastcall Client(unsigned) {return client.data();}
void* __fastcall Find(void* g,unsigned type,unsigned id) {
    if(g!=game.data())return nullptr;
    if(type==0&&id==1)return server.data();
    if(type==1&&id==2)return pet.data();
    if(type==1&&id==3)return summon.data();
    if(type==1&&id==5&&!sourceMissing)return source.data();
    if(type==4)for(auto& item:nativeItems) {UnitHead head;if(Head(item->unit.data(),head)&&head.id==id)return item->unit.data();}
    return nullptr;
}
void* __fastcall Room(void* unit) {
    for(auto& item:nativeItems)if(unit==item->unit.data()) {void* room{};Read(item->staticPath.data(),0,room);return room;}
    return unit==source.data()?&sourceLevel:&playerLevel;
}
unsigned __fastcall RoomId(void* room) {return *static_cast<unsigned*>(room);}
int __fastcall Contains(void* room,int x,int y) {
    if(noValidRoom||!room||y<5600||y>=5800)return 0;
    if(room==&sourceLevel)return x>=1200&&x<1250;
    if(room==&playerLevel)return separateRooms?(x>=1250&&x<1350):(x>=1200&&x<1250);
    return 0;
}
Inventory::Result __cdecl Player(const PluginContext*,PlayerHandle* player) noexcept {*player=55;return Inventory::Result::Success;}
void* __fastcall NativeCreateFake(void* player,unsigned cls,void* g,int target,int quality,int noSockets,int noEth,int level,int seed1,int seed2,int seed3) {
    ++nativeCreates;
    Check(player==server.data()&&g==game.data()&&target==4&&quality==7&&noSockets==1&&noEth==1&&level==99
        &&seed1==0&&seed2==0&&seed3==0,"native creator ABI: authoritative player, numeric class, unplaced unique, no sockets/ethereal, level 99");
    if(rejectCreation)return nullptr;
    Check(cls>=901&&cls<=920,"native creator receives resolved numeric class ID, not packed item code");
    auto item=std::make_unique<FakeNativeItem>();
    Put(item->unit,0,UnitHead{4,cls,1000+static_cast<unsigned>(nativeItems.size())});
    Put(item->unit,0x10,static_cast<void*>(item->data.data()));Put(item->unit,0x38,static_cast<void*>(item->staticPath.data()));
    Put(item->data,0,badQuality?6u:7u);Put(item->data,0x18,0x10u);
    auto result=item->unit.data();nativeItems.push_back(std::move(item));return result;
}
void __fastcall NativeFlagFake(void* unit,unsigned flag,int enabled) {
    ++nativeFlagEdits;Check(flag==0x10&&enabled==0,"native identification is cleared before publication");
    void* data{};uint32_t flags{};Read(unit,0x10,data);Read(data,0x18,flags);
    if(!badIdentification) {flags&=~flag;std::memcpy(static_cast<char*>(data)+0x18,&flags,4);}
}
void __fastcall NativePlaceFake(void* g,void* optionalPlayer,void* unit,void* room,int x,int y) {
    ++nativePlacements;
    Check(g==game.data()&&!optionalPlayer&&room&&Contains(room,x,y),"native placement receives a real matching room and positive in-room coordinates");
    UnitHead head;void* data{};void* pathPointer{};uint32_t flags{},quality{};
    Head(unit,head);Read(unit,0x10,data);Read(unit,0x38,pathPointer);Read(data,0,quality);Read(data,0x18,flags);
    Check(!(flags&0x10),"box must be unidentified at the instant it is published");
    if(badPlacement)return;
    std::memcpy(pathPointer,&room,sizeof(room));std::memcpy(static_cast<char*>(pathPointer)+0x10,&x,4);std::memcpy(static_cast<char*>(pathPointer)+0x14,&y,4);
    specifications.push_back({StatueCode(head.classId-900),static_cast<Items::Quality>(quality),0,{static_cast<unsigned>(x),static_cast<unsigned>(y)},room});++edits;
}
Threads::Result __cdecl Schedule(const PluginContext*,Threads::Callback callback,void* data) noexcept {
    if(!scheduleAvailable)return Threads::Result::Unavailable;
    work=callback;workData=data;return Threads::Result::Success;
}
DataTables::Result __cdecl Table(const PluginContext*,DataTables::Bank bank,DataTables::TableId table,DataTables::TableView* output) noexcept {
    if(bank!=DataTables::Bank::Rotw||table!=DataTables::TableId::MonStats)return DataTables::Result::NotFound;
    *output={.structSize=DataTables::TableViewSize,.tableId=table,.bank=bank,.revision=1,.rows=monstats.data(),
        .rowCount=static_cast<unsigned>(monstats.size()/0x1fc),.rowSize=badStride?0x1fbu:0x1fcu};
    if(badBoss)output->rowCount=1000;
    return DataTables::Result::Success;
}
DataTables::Result __cdecl ByCode(const PluginContext*,DataTables::Bank bank,DataTables::TableId table,uint32_t code,DataTables::RowView* row) noexcept {
    if(bank!=DataTables::Bank::Rotw)return DataTables::Result::NotFound;
    const auto statue=(((code>>8)&255)-'0')*10+((code>>16)&255)-'0';
    *row={.structSize=DataTables::RowViewSize,.tableId=table,.bank=bank,.rowIndex=900+statue,.rowSize=0x100};
    return DataTables::Result::Success;
}
DataTables::Result __cdecl ById(const PluginContext*,DataTables::Bank bank,DataTables::TableId table,uint32_t id,DataTables::RowView* row) noexcept {
    if(id>198)return DataTables::Result::NotFound;
    *row={.structSize=DataTables::RowViewSize,.tableId=table,.bank=bank,.rowIndex=id,.rowSize=0x18c};
    return DataTables::Result::Success;
}
InventoryService fakeInventory{.serviceSize=InventoryServiceSize,.serviceVersion=InventoryService::AbiVersion,.getLocalPlayer=Player};
ThreadService fakeThreads{.serviceSize=ThreadServiceSize,.serviceVersion=ThreadService::AbiVersion,.runOnGameThread=Schedule};
DataTableService fakeTables{.serviceSize=DataTableServiceSize,.serviceVersion=DataTableService::AbiVersion,.getTable=Table,.findRowById=ById,.findRowByCode=ByCode};
PluginApi fakeApi{.apiSize=PluginApiSize,.logInfo=Log,.logWarn=Warn,.logError=Warn
};
PluginContext context{.contextSize=PluginContextSize,.api=&fakeApi};

void LoadMonstats(const std::filesystem::path& file) {
    std::ifstream input(file);std::string line;
    Check(static_cast<bool>(std::getline(input,line)),"read active monstats");
    const auto header=Fields(line);
    const auto name=std::find(header.begin(),header.end(),"Id")-header.begin();
    const auto index=std::find(header.begin(),header.end(),"*hcIdx")-header.begin();
    monstats.resize(65536*0x1fc);
    unsigned maximum{},bosses{};
    for(unsigned i=0;i<65536;++i) {const auto id=static_cast<uint16_t>(i);std::memcpy(monstats.data()+i*0x1fc,&id,2);}
    while(std::getline(input,line)) {
        const auto row=Fields(line);
        if(row.size()!=header.size()||row[index].empty())continue;
        const auto id=Number(row[index]);
        Check(id<65536&&row[name].size()<48,"runtime monster fixture bounds");
        std::memcpy(monstats.data()+id*0x1fc+2,row[name].c_str(),row[name].size()+1);
        maximum=std::max(maximum,id);
        if(row[name].starts_with("descent")) {
            Check(CheckpointFloor(row[name])!=0,"all real checkpoint models recognized");++bosses;
        }
    }
    Check(bosses==324,"324 real checkpoint models across six sections");
    monstats.resize((static_cast<size_t>(maximum)+1)*0x1fc);
}
void Reset() {
    GameLeft();edits=warnings=0;specifications.clear();work=nullptr;workData=nullptr;offline=true;rejectCreation=false;scheduleAvailable=true;
    installed.store(true);nativeItems.clear();nativeCreates=nativePlacements=nativeFlagEdits=0;
    sourceMissing=separateRooms=noValidRoom=badQuality=badIdentification=badPlacement=false;
    game={};source={};server={};client={};pet={};summon={};ownerA={};ownerB={};dataA={};dataB={};path={};
    Put(game,0x104,uint8_t{2});Put(game,0x106,uint8_t{3});
    Identity(source,1,30,5);Identity(server,0,4,1);Identity(client,0,4,1);Identity(pet,1,271,2);Identity(summon,1,289,3);
    Put(path,2,uint16_t{1234});Put(path,6,uint16_t{5678});playerPath=path;
    active=game.data();activeGameSlot=reinterpret_cast<uintptr_t>(&active);
    playerLevel=sourceLevel=138;
    randomEngine.seed(567);
}
void Owned(std::array<char,0x200>& unit,std::array<char,0x200>& data,std::array<char,0x200>& owner,unsigned type,unsigned id) {
    Put(unit,0x10,static_cast<void*>(data.data()));Put(data,0x30,static_cast<void*>(owner.data()));
    Put(owner,0x60,static_cast<void*>(game.data()));Put(owner,0x68,id);Put(owner,0x6c,type);
}
void Run() {Check(work!=nullptr,"authoritative work queued");const auto callback=work;const auto data=workData;work=nullptr;workData=nullptr;callback(&context,data);}
void WinningSeed(unsigned denominator) {
    std::mt19937_64 scan(123);
    for(unsigned i=0;i<10000000;++i) {
        const auto before=scan;
        if(std::uniform_int_distribution<unsigned>(0,denominator-1)(scan)==0) {randomEngine=before;return;}
    }
    Check(false,"find deterministic winning RNG state");
}
}

int wmain(int argc,wchar_t** argv) {
    Check(argc==2,"active testing data folder argument");
    const std::filesystem::path dataRoot=argv[1];
    context.modDirectory=argv[1];
    LoadMonstats(dataRoot/L"data/global/excel/monstats.txt");
    std::ifstream levels(dataRoot/L"data/global/excel/levels.txt");
    const auto areas=ReadAreas(levels);
    Check(areas.at(39).cow&&areas.at(198).descent&&!areas.at(198).floor,"Cow and entrance separated from acts");
    for(unsigned floor=1;floor<=60;++floor) {
        const auto area=areas.at(137+floor);
        Check(area.floor==floor&&area.descent,"all real floors mapped");
        for(unsigned difficulty=0;difficulty<3;++difficulty) {
            auto chance=Eligible({area,difficulty,4,false});
            Check(chance.values[0].statue==1+(floor-1)/10&&chance.values[0].denominator==10000,"floor statue and 0.01 percent");
            Check(chance.count==(difficulty==2?2u:1u),"Descent unaffected by act chance or difficulty");
        }
        const auto checkpoint=Eligible({area,2,4,floor%10==0});
        Check(checkpoint.values[0].denominator==(floor%10==0?1000u:10000u),"checkpoint rate only for actual checkpoint boss");
    }
    for(int act=0;act<5;++act)for(int cls=0;cls<8;++cls) {
        const auto chances=Eligible({Area{.act=act},2,cls,false});
        Check(chances.count==2&&chances.values[0].statue==7+static_cast<unsigned>(act)
            &&chances.values[1].statue==13+static_cast<unsigned>(cls),"all five acts and eight matching classes");
        Check(chances.values[0].denominator==1000000&&chances.values[1].denominator==1000000,"0.0001 percent act/class");
        Check(Eligible({Area{.act=act},0,cls,false}).count==0&&Eligible({Area{.act=act},1,cls,false}).count==0,"Hell-only act/class");
    }
    Check(Eligible({areas.at(39),2,1,false}).values[0].statue==12,"Cow replaces act statue");
    Check(Eligible({areas.at(39),0,1,false}).count==0,"Cow requires Hell");
    Check(Eligible({areas.at(198),2,1,false}).count==1,"entrance has class only");
    Check(Eligible({areas.at(138),2,-1,false}).count==1,"unknown class never defaults to local class");
    Check(Eligible({Area{.act=2},2,8,false}).count==1,"unknown future class never maps to another statue");
    unsigned draws{},awards{};
    Roll({Area{.act=0},2,0,false},[&](unsigned denominator) {++draws;return draws==1?denominator-1:0;},
        [&](const Chance& value) {Check(value.statue==13,"class rolls independently when act misses");++awards;});
    Check(draws==2&&awards==1,"one independent draw per eligible category");
    draws=awards=0;
    Roll({Area{.act=0},2,0,false},[&](unsigned) {++draws;return 0;},[&](const Chance&) {++awards;});
    Check(draws==2&&awards==2,"both independent categories can win");
    Check(!CheckpointFloor("descent20_escort")&&!CheckpointFloor("descent20_ismail_2")&&!CheckpointFloor("descent70_diablo"),"escorts and nonexistent variants are not checkpoint bosses");
    for(unsigned statue=1;statue<=20;++statue)
        Check((StatueCode(statue)&255)=='s',"all 20 statue codes remain mapped");
    std::ifstream original(dataRoot/L"data/global/excel/levels.txt");
    std::string contents((std::istreambuf_iterator<char>(original)),{});
    auto missing=contents;missing.replace(missing.find("LevelName"),9,"NoNameKey");
    Reject([&] {std::istringstream input(missing);ReadAreas(input);},"missing required column rejected");
    Reject([&] {std::istringstream input("Id\tAct\tName\tLevelName\tLevelType\tId\n");ReadAreas(input);},"duplicate header rejected");
    Reject([&] {std::istringstream input("Id\tAct\tName\tLevelName\tLevelType\n");ReadAreas(input);},"incomplete catalog rejected");
    std::ostringstream reordered;
    reordered<<"LevelType\tLevelName\tName\tAct\tId\n";
    constexpr unsigned types[]{10,8,4,3,33,35};
    for(unsigned floor=1;floor<=60;++floor)
        reordered<<types[(floor-1)/10]<<"\tGrailerDescentFloor"<<floor<<"\tFloor\t4\t"<<(137+floor)<<'\n';
    reordered<<"10\tGrailerDescentEntrance\tEntrance\t4\t198\n2\tMoo Moo Farm\tAct 1 - Moo Moo Farm\t0\t39\n";
    std::istringstream reorderedInput(reordered.str());
    Check(ReadAreas(reorderedInput).at(197).floor==60,"reordered source columns resolve by exact header name");

    dropOwner=&context;dropInventory=&fakeInventory;dropThreads=&fakeThreads;dropTables=&fakeTables;
    singlePlayer=Single;findServerUnit=Find;unitRoom=Room;roomLevel=RoomId;localContext=ContextIndex;localPlayer=Client;createNativeItem=NativeCreateFake;placeNativeItem=NativePlaceFake;setNativeItemFlag=NativeFlagFake;roomContains=Contains;
    Refresh(&context,nullptr,nullptr);
    Check(catalog.load()&&catalog.load()->banks[2].ready,"actual data resolves compiled catalog");
    badStride=true;Refresh(&context,nullptr,nullptr);Check(!catalog.load(),"unknown compiled layout fails closed");badStride=false;
    badBoss=true;Refresh(&context,nullptr,nullptr);Check(!catalog.load(),"incomplete checkpoint catalog fails closed");badBoss=false;
    Refresh(&context,nullptr,nullptr);installed.store(true);

    Reset();
    Check(PlayerOwner(server.data(),game.data())==server.data(),"direct player credit");
    Owned(pet,dataA,ownerA,0,1);
    Check(PlayerOwner(pet.data(),game.data())==server.data(),"mercenary/summon player owner credit");
    Owned(summon,dataB,ownerB,1,2);
    Check(PlayerOwner(summon.data(),game.data())==server.data(),"nested summon owner credit");
    Put(ownerA,0x6c,1u);Put(ownerA,0x68,3u);
    Check(!PlayerOwner(pet.data(),game.data()),"ownership cycle fails closed");

    Reset();WinningSeed(10000);
    CollectorStatuesObserveDeath(game.data(),source.data(),server.data(),0);
    Check(edits==0&&eligible[0]==1&&eligible[3]==1,"observer queues rather than mutates and counts one kill per category");
    Run();Check(edits==1&&specifications[0].code==Items::MakeItemCode("s01"),"real observer/queue produces matching box");
    Check(created[0]==1&&specifications[0].destination.x==1234&&specifications[0].destination.y==5678,"real native request uses victim position");

    // Choose a deterministic draw that wins 1/1,000 but misses 1/10,000.
    std::mt19937_64 bossEngine(765),bossState;
    for(;;) {
        bossState=bossEngine;auto ordinary=bossState;
        if(std::uniform_int_distribution<unsigned>(0,999)(bossEngine)==0
            &&std::uniform_int_distribution<unsigned>(0,9999)(ordinary)!=0)break;
    }
    const auto& bossMap=catalog.load()->banks[2].checkpointFloors;
    const auto firstBoss=std::find_if(bossMap.begin(),bossMap.end(),[](auto pair) {return pair.second==10;})->first;
    Reset();sourceLevel=playerLevel=147;Put(source,4,firstBoss);randomEngine=bossState;
    CollectorStatuesObserveDeath(game.data(),source.data(),server.data(),0);
    Check(won[0]==1,"actual checkpoint boss uses enhanced 1/1,000 roll");Run();
    Reset();sourceLevel=playerLevel=148;Put(source,4,firstBoss);randomEngine=bossState;
    CollectorStatuesObserveDeath(game.data(),source.data(),server.data(),0);
    Check(won[0]==0,"same boss model outside its checkpoint retains normal rate");
    Reset();sourceLevel=playerLevel=147;randomEngine=bossState;
    CollectorStatuesObserveDeath(game.data(),source.data(),server.data(),0);
    Check(won[0]==0,"checkpoint escorts retain normal rate");

    for(unsigned cls=0;cls<8;++cls) {
        Reset();sourceLevel=playerLevel=2;Put(server,4,cls);Put(client,4,cls);
        Owned(pet,dataA,ownerA,0,1);Owned(summon,dataB,ownerB,1,2);
        WinningSeed(1000000);
        // Unknown act deliberately leaves the real observer only a class roll.
        auto classCatalog=std::make_shared<Catalog>(*catalog.load());classCatalog->areas[2].act=-1;
        const auto previous=catalog.exchange(classCatalog);
        CollectorStatuesObserveDeath(game.data(),source.data(),summon.data(),0);
        Check(eligible[3]==1&&won[3]==1,"all eight classes receive their owned summon kill credit");
        Run();Check(edits==1&&specifications[0].code==StatueCode(13+cls),"actual owned summon class reward matches credited class only");
        catalog.store(previous);
    }

    Reset();sourceLevel=playerLevel=39;
    CollectorStatuesObserveDeath(game.data(),source.data(),server.data(),0);
    Check(eligible[2]==1&&eligible[3]==1&&eligible[1]==0&&eligible[0]==0,"actual observer Cow excludes Act I");
    Reset();sourceLevel=playerLevel=198;
    CollectorStatuesObserveDeath(game.data(),source.data(),server.data(),0);
    Check(eligible[3]==1&&eligible[1]==0&&eligible[0]==0,"actual entrance excludes Act V and floor statue");
    Reset();sourceLevel=playerLevel=2;Put(game,0x104,uint8_t{1});
    CollectorStatuesObserveDeath(game.data(),source.data(),server.data(),0);
    Check(eligible[1]==0&&eligible[3]==0,"actual non-Hell campaign does not roll");
    Reset();offline=false;
    CollectorStatuesObserveDeath(game.data(),source.data(),server.data(),0);
    Check(eligible[0]==0&&eligible[3]==0,"unsupported nonoffline mode untouched");
    Reset();Owned(source,dataA,ownerA,0,1);
    CollectorStatuesObserveDeath(game.data(),source.data(),server.data(),0);
    Check(eligible[0]==0&&eligible[3]==0,"player pet deaths cannot generate statues");

    Reset();QueueAward({game.data(),epoch.load(),138,111,222,20,Category::Class});playerLevel=139;Run();
    Check(edits==0&&failed==1,"level transition cannot place box in wrong map");
    Reset();QueueAward({game.data(),epoch.load(),138,111,222,20,Category::Class});active=nullptr;Run();
    Check(edits==0,"stale game cannot create items");
    Reset();QueueAward({game.data(),epoch.load(),138,111,222,20,Category::Class});GameLeft();
    Check(queueSize==0&&!queued,"game exit clears queued awards");
    Reset();rejectCreation=true;QueueAward({game.data(),epoch.load(),138,111,222,20,Category::Class});Run();
    Check(edits==0&&failed==1&&warnings==1,"native creation failure reported without an inventory fallback");
    Reset();
    QueueAward({game.data(),epoch.load(),138,111,222,20,Category::Class});
    QueueAward({game.data(),epoch.load(),138,111,222,20,Category::Class});Run();
    Check(edits==2&&specifications[0].code==specifications[1].code&&specifications[0].quality==Items::Quality::Unique&&specifications[0].stateFlags==0,"repeated statue wins remain unique and allowed");
    std::ostringstream repeatDefinitions;
    repeatDefinitions<<"code\tnolimit\tindex\n";
    for(unsigned n=1;n<=20;++n)repeatDefinitions<<'s'<<(n/10)<<(n%10)<<"\t1\tStatue "<<n<<'\n';
    std::istringstream repeatInput(repeatDefinitions.str());ValidateRepeatableStatues(repeatInput);
    auto missingRepeat=repeatDefinitions.str();missingRepeat.replace(missingRepeat.find("s01\t1"),5,"s01\t0");
    Reject([&] {std::istringstream input(missingRepeat);ValidateRepeatableStatues(input);},"repeat-unique data must be enabled before native spawning");
    auto sourceAward=[&] {
        Award award{game.data(),epoch.load(),138,1234,5678,1,Category::Descent};
        award.sourceId=5;award.sourceClass=30;return award;
    };
    Reset();separateRooms=true;Put(playerPath,2,uint16_t{1300});
    QueueAward(sourceAward());Run();
    Check(edits==1&&specifications[0].room==&sourceLevel&&specifications[0].destination.x==1234,
        "distant kill uses the freshly resolved corpse room, not the player's different room");
    Reset();separateRooms=true;sourceMissing=true;Put(playerPath,2,uint16_t{1300});
    QueueAward(sourceAward());Run();
    Check(edits==1&&specifications[0].room==&playerLevel&&specifications[0].destination.x==1300,
        "vanished corpse falls back to valid player coordinates and matching room");
    Reset();auto zero=sourceAward();zero.x=0;QueueAward(zero);Run();
    Check(edits==1&&specifications[0].destination.x==1234&&specifications[0].destination.y==5678,
        "zero source coordinate never reaches native placement");
    Reset();noValidRoom=true;QueueAward(sourceAward());Run();
    Check(edits==0&&nativeCreates==0&&nativePlacements==0&&failed==1,
        "invalid rooms are rejected before any native item allocation or publication");
    Reset();badQuality=true;QueueAward(sourceAward());Run();
    Check(edits==0&&nativeCreates==1&&nativePlacements==0&&!installed,
        "unexpected native quality is never published and disables further drops");
    QueueAward(sourceAward());Run();Check(nativeCreates==1,"fail-closed state also stops already queued follow-up awards");
    Reset();badIdentification=true;QueueAward(sourceAward());Run();
    Check(edits==0&&nativeFlagEdits==1&&nativePlacements==0&&!installed,
        "an item that remains identified cannot be published as a box");
    Reset();badPlacement=true;QueueAward(sourceAward());Run();
    Check(edits==0&&nativeCreates==1&&nativePlacements==1&&created[0]==0&&!installed,
        "native return alone is not success: actual room and coordinates must be verified");
    Reset();QueueAward(sourceAward());Run();
    Check(edits==1&&nativeCreates==1&&nativeFlagEdits==1&&nativePlacements==1
        &&specifications[0].quality==Items::Quality::Unique&&specifications[0].stateFlags==0,
        "creation, unidentification and room publication occur in the required order");
    std::puts("PASS: native numeric class IDs, real source rooms, coordinate fallback, pre-publication identification, post-placement verification and fail-closed state.");
    Stop();
    std::puts("PASS: statue probabilities, active floor/boss catalog, exclusions, 8 classes, ownership, RNG, authoritative native room-validated ground creation and lifecycle.");
}
