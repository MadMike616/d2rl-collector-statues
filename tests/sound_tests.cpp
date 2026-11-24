#include "../src/native_sound.cpp"
#include <cstdio>
#include <cstdlib>
namespace {
unsigned played{},warnings{};
bool missing{},fault{};
std::array<char,200> row{};
void Check(bool ok,const char* message) {if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
void* __fastcall SoundRow(int id,void* redirect,int mode) noexcept {
    Check(id==13&&!redirect&&mode==1,"sound identity is checked against the raw native row");
    return missing?nullptr:row.data();
}
uint64_t __fastcall Emit(int id,void* unit,int a,int b,bool c) noexcept {
    Check(id==13&&!unit&&!a&&!b&&!c,"native item sound uses the proven 2D call shape");
    if(fault)RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);
    ++played;return 1;
}
void __cdecl Warning(const D2RL::PluginContext*,const char*) noexcept {++warnings;}
D2RL::PluginApi api{.apiSize=D2RL::PluginApiSize,.logWarn=Warning};
D2RL::PluginContext context{.contextSize=D2RL::PluginContextSize,.api=&api};
void Reset() {
    row={};constexpr char name[]="cursor_identify_item";std::memcpy(row.data()+2,name,sizeof(name));
    played=warnings=0;missing=fault=false;playback=Emit;lookup=SoundRow;admitted.store(true);warned.store(false);
}
}
int main() {
    Reset();CollectorSound::Play(&context);Check(played==1&&!warnings,"valid native identification sound plays once");
    CollectorSound::Stop();CollectorSound::Play(&context);Check(played==1,"stopped audio cannot execute native calls");
    Reset();missing=true;CollectorSound::Play(&context);CollectorSound::Play(&context);
    Check(!played&&warnings==1&&!admitted.load(),"missing sound row disables audio with one warning");
    Reset();row[2]='x';CollectorSound::Play(&context);Check(!played&&warnings==1,"reordered/wrong sound row is never played");
    Reset();fault=true;CollectorSound::Play(&context);Check(!played&&warnings==1&&!admitted.load(),"native audio exception disables only the optional sound");
    std::puts("PASS: native opening sound ABI, raw sound identity, missing/wrong rows, shutdown disable and native fault isolation.");
}
