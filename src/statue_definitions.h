#pragma once
#include <D2RLPlugin/item.h>
#include <array>
#include <span>
#include <string_view>

namespace CollectorStatues {
enum class Source { None, Descent, Act, Cow, Class };
struct DropRule {
    Source source{Source::None};
    unsigned denominator{},checkpointDenominator{};
    unsigned firstFloor{},lastFloor{};
    int selector{-1}; // Zero-based act or player class, depending on source.
    bool hellOnly{};
};
constexpr DropRule Descent(unsigned first,unsigned last,unsigned normal=10000,unsigned checkpoint=1000) noexcept {
    return {Source::Descent,normal,checkpoint,first,last,-1,false};
}
constexpr DropRule Act(int act,unsigned denominator=1000000) noexcept {
    return {Source::Act,denominator,0,0,0,act,true};
}
constexpr DropRule Cow(unsigned denominator=1000000) noexcept {
    return {Source::Cow,denominator,0,0,0,-1,true};
}
constexpr DropRule Class(int playerClass,unsigned denominator=1000000) noexcept {
    return {Source::Class,denominator,0,0,0,playerClass,true};
}
struct Definition { unsigned id{};uint32_t code{};DropRule drop{}; };

// The single registration list. Preserve existing IDs and codes forever.
// Add the matching game-data rows, localized names and art separately; see
// EXTENDING_STATUES.md. A default DropRule registers an opening-only statue.
inline constexpr auto Definitions=std::to_array<Definition>({
    { 1,D2RL::Items::MakeItemCode("s01"),Descent( 1,10)}, // Catacombs
    { 2,D2RL::Items::MakeItemCode("s02"),Descent(11,20)}, // Jail
    { 3,D2RL::Items::MakeItemCode("s03"),Descent(21,30)}, // Crypt
    { 4,D2RL::Items::MakeItemCode("s04"),Descent(31,40)}, // Caves
    { 5,D2RL::Items::MakeItemCode("s05"),Descent(41,50)}, // Ice Caves
    { 6,D2RL::Items::MakeItemCode("s06"),Descent(51,60)}, // Lava
    { 7,D2RL::Items::MakeItemCode("s07"),Act(0)},
    { 8,D2RL::Items::MakeItemCode("s08"),Act(1)},
    { 9,D2RL::Items::MakeItemCode("s09"),Act(2)},
    {10,D2RL::Items::MakeItemCode("s10"),Act(3)},
    {11,D2RL::Items::MakeItemCode("s11"),Act(4)},
    {12,D2RL::Items::MakeItemCode("s12"),Cow()},
    {13,D2RL::Items::MakeItemCode("s13"),Class(0)}, // Amazon
    {14,D2RL::Items::MakeItemCode("s14"),Class(1)}, // Sorceress
    {15,D2RL::Items::MakeItemCode("s15"),Class(2)}, // Necromancer
    {16,D2RL::Items::MakeItemCode("s16"),Class(3)}, // Paladin
    {17,D2RL::Items::MakeItemCode("s17"),Class(4)}, // Barbarian
    {18,D2RL::Items::MakeItemCode("s18"),Class(5)}, // Druid
    {19,D2RL::Items::MakeItemCode("s19"),Class(6)}, // Assassin
    {20,D2RL::Items::MakeItemCode("s20"),Class(7)}, // Warlock
});
inline constexpr size_t Count=Definitions.size();

// Some compiled banks expose a three-character code with a zero fourth byte.
// Normalize only that padding; four-character codes retain all four bytes.
constexpr uint32_t CanonicalCode(uint32_t code) noexcept {
    return (code&0xff000000u)?code:(code|0x20000000u);
}
constexpr bool ValidCode(uint32_t code) noexcept {
    for(unsigned i=0;i<3;++i) {
        const auto ch=(code>>(i*8))&255u;
        if(ch<0x21||ch>0x7e)return false;
    }
    const auto last=code>>24;
    return last>=0x20&&last<=0x7e;
}
constexpr uint32_t ParseCode(std::string_view text) noexcept {
    if(text.size()!=3&&text.size()!=4)return 0;
    const auto code=D2RL::Items::MakeItemCode(text[0],text[1],text[2],text.size()==4?text[3]:' ');
    return ValidCode(code)?code:0;
}
constexpr std::array<char,5> CodeText(uint32_t code) noexcept {
    code=CanonicalCode(code);
    std::array<char,5> text{};
    for(unsigned i=0;i<4;++i)text[i]=static_cast<char>((code>>(i*8))&255u);
    if(text[3]==' ')text[3]=0;
    return text;
}
constexpr bool ValidRule(const DropRule& rule) noexcept {
    if(rule.source==Source::None)
        return rule.denominator==0&&rule.checkpointDenominator==0&&rule.firstFloor==0&&rule.lastFloor==0&&rule.selector==-1;
    if(!rule.denominator)return false;
    if(rule.source==Source::Descent)
        return rule.firstFloor>=1&&rule.firstFloor<=rule.lastFloor&&rule.lastFloor<=60&&rule.selector==-1;
    if(rule.firstFloor||rule.lastFloor||rule.checkpointDenominator)return false;
    switch(rule.source) {
    case Source::Act:return rule.selector>=0&&rule.selector<5;
    case Source::Class:return rule.selector>=0&&rule.selector<8;
    case Source::Cow:return rule.selector==-1;
    default:return false;
    }
}
constexpr bool ValidDefinitions(std::span<const Definition> definitions) noexcept {
    if(definitions.empty())return false;
    for(size_t i=0;i<definitions.size();++i) {
        const auto& entry=definitions[i];
        if(!entry.id||!ValidCode(entry.code)||!ValidRule(entry.drop))return false;
        for(size_t j=0;j<i;++j)
            if(entry.id==definitions[j].id||CanonicalCode(entry.code)==CanonicalCode(definitions[j].code))return false;
    }
    return true;
}
static_assert(ValidDefinitions(Definitions),"Statue definitions require unique nonzero IDs, unique 3/4-character item codes and valid drop rules.");

constexpr size_t IndexById(unsigned id,std::span<const Definition> definitions=Definitions) noexcept {
    for(size_t i=0;i<definitions.size();++i)if(definitions[i].id==id)return i;
    return definitions.size();
}
constexpr size_t IndexByCode(uint32_t code,std::span<const Definition> definitions=Definitions) noexcept {
    for(size_t i=0;i<definitions.size();++i)if(CanonicalCode(definitions[i].code)==CanonicalCode(code))return i;
    return definitions.size();
}
constexpr uint32_t CodeById(unsigned id,std::span<const Definition> definitions=Definitions) noexcept {
    const auto index=IndexById(id,definitions);
    return index<definitions.size()?definitions[index].code:0;
}
constexpr bool RegisteredCode(uint32_t code,std::span<const Definition> definitions=Definitions) noexcept {
    return IndexByCode(code,definitions)<definitions.size();
}
}
