#include "../src/drop_catalog.h"
#include <cstdio>
#include <cstdlib>
#include <sstream>

using namespace CollectorStatues;
using namespace CollectorDrops;
namespace {
void Check(bool condition,const char* message) {
    if(!condition){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
template<class F> void Reject(F&& operation,const char* message) {
    try{operation();}catch(const std::exception&){return;}
    Check(false,message);
}
constexpr auto Expanded() {
    std::array<Definition,Count+5> result{};
    for(size_t i=0;i<Count;++i)result[i]=Definitions[i];
    result[Count+0]={42,D2RL::Items::MakeItemCode("s21"),Act(0,12345)};
    result[Count+1]={105,D2RL::Items::MakeItemCode("z99"),Act(0,54321)};
    result[Count+2]={777,D2RL::Items::MakeItemCode("t021"),Class(4,98765)};
    result[Count+3]={900,D2RL::Items::MakeItemCode("mbx"),{}};
    result[Count+4]={901,D2RL::Items::MakeItemCode("lv2"),Descent(11,20,3333,333)};
    return result;
}
constexpr auto expanded=Expanded();
static_assert(ValidDefinitions(expanded));
template<size_t N> bool Has(const ChanceList<N>& list,unsigned id,unsigned denominator) {
    for(unsigned i=0;i<list.count;++i)
        if(list.values[i].statue==id&&list.values[i].denominator==denominator)return true;
    return false;
}
std::string UniqueRows(std::span<const Definition> definitions) {
    std::ostringstream text;text<<"nolimit\tindex\tcode\n";
    for(const auto& definition:definitions)
        text<<"1\tTest "<<definition.id<<'\t'<<CodeText(definition.code).data()<<'\n';
    return text.str();
}
}
int main() {
    for(unsigned id=1;id<=20;++id) {
        const auto old=D2RL::Items::MakeItemCode('s',static_cast<char>('0'+id/10),static_cast<char>('0'+id%10));
        Check(CodeById(id)==old,"existing statue IDs and item codes are unchanged");
    }
    Check(!CodeById(0)&&!CodeById(999999),"unknown IDs cannot generate an accidental item code");
    Check(RegisteredCode(D2RL::Items::MakeItemCode("s21"),expanded)
        &&RegisteredCode(D2RL::Items::MakeItemCode("z99"),expanded)
        &&RegisteredCode(D2RL::Items::MakeItemCode("t021"),expanded),"added, non-s and four-character item codes are recognized");
    Check(RegisteredCode(D2RL::Items::MakeItemCode("s01")&0xffffffu)
        &&!RegisteredCode(D2RL::Items::MakeItemCode("t02"),expanded),"three-character padding normalization never aliases a four-character code");
    Check(IndexById(105,expanded)==Count+1&&CodeById(777,expanded)==D2RL::Items::MakeItemCode("t021"),
        "sparse stable IDs are resolved by catalog entry, never array position");
    const auto act=EligibleFrom({Area{.act=0},2,4,false},expanded);
    Check(act.count==5&&Has(act,7,1000000)&&Has(act,17,1000000)
        &&Has(act,42,12345)&&Has(act,105,54321)&&Has(act,777,98765),
        "more than two eligible statues retain independent definitions and odds");
    Check(EligibleFrom({Area{.act=0},1,4,false},expanded).count==0,"new act/class definitions remain Hell-only");
    const auto cow=EligibleFrom({Area{.act=0,.cow=true},2,4,false},expanded);
    Check(cow.count==3&&Has(cow,12,1000000)&&Has(cow,777,98765)&&!Has(cow,42,12345)&&!Has(cow,105,54321),
        "added act statues stay excluded from Cow Level");
    const auto descent=EligibleFrom({Area{.act=4,.floor=20,.descent=true},2,4,true},expanded);
    Check(descent.count==4&&Has(descent,2,1000)&&Has(descent,901,333)
        &&Has(descent,17,1000000)&&Has(descent,777,98765),"additional Descent rules use their own checkpoint odds");
    Check(RegisteredCode(D2RL::Items::MakeItemCode("mbx"),expanded)&&!Has(act,900,0),"opening-only registration has no automatic drop");
    auto reordered=expanded;std::swap(reordered[0],reordered[Count+2]);
    Check(CodeById(1,reordered)==CodeById(1)&&CodeById(777,reordered)==CodeById(777,expanded),"reordering never changes stable ID/code mapping");
    auto invalid=expanded;invalid.back().id=invalid.front().id;Check(!ValidDefinitions(invalid),"duplicate IDs rejected");
    invalid=expanded;invalid.back().code=invalid.front().code;Check(!ValidDefinitions(invalid),"duplicate codes rejected");
    invalid=expanded;invalid.back().drop=Act(0,0);Check(!ValidDefinitions(invalid),"zero drop denominator rejected");
    invalid=expanded;invalid.back().drop=Class(8);Check(!ValidDefinitions(invalid),"unvalidated future class index rejected");
    invalid=expanded;invalid.back().drop=Descent(20,10);Check(!ValidDefinitions(invalid),"reversed floor range rejected");
    invalid=expanded;invalid.back().drop=Descent(1,61);Check(!ValidDefinitions(invalid),"unsupported dungeon floors rejected");
    invalid=expanded;invalid.back().code=D2RL::Items::MakeItemCode("x");Check(!ValidDefinitions(invalid),"short malformed statue codes rejected");
    Check(!ValidDefinitions(std::array<Definition,0>{}),"empty registration list rejected");
    const auto rows=UniqueRows(expanded);
    std::istringstream source(rows);ValidateRepeatableStatues(source,expanded);
    auto missing=rows;missing.erase(missing.find("1\tTest 777"),missing.find('\n',missing.find("1\tTest 777"))-missing.find("1\tTest 777")+1);
    Reject([&]{std::istringstream input(missing);ValidateRepeatableStatues(input,expanded);},"new statue missing from game data fails closed");
    auto repeated=rows+"1\tDuplicate\tt021\n";
    Reject([&]{std::istringstream input(repeated);ValidateRepeatableStatues(input,expanded);},"duplicate four-character unique definition rejected");
    auto limited=rows;limited.replace(limited.find("1\tTest 777"),1,"0");
    Reject([&]{std::istringstream input(limited);ValidateRepeatableStatues(input,expanded);},"new statue without repeat permission rejected");
    std::puts("PASS: expanded catalog beyond twenty, sparse IDs, arbitrary 3/4-character codes, five simultaneous rules, exclusions, checkpoint odds, opening-only entries and strict game-data validation.");
}
