#pragma once
#include <array>
#include <cstdint>
#include "statue_definitions.h"

namespace CollectorDrops {
enum class Category : unsigned { Descent, Act, Cow, Class };
struct Area {
    int act{-1};
    unsigned floor{};
    bool descent{}, cow{};
};
struct Kill {
    Area area{};
    unsigned difficulty{};
    int playerClass{-1};
    bool checkpointBoss{};
};
struct Chance { unsigned statue{}, denominator{}; Category category{}; };
template<size_t N> struct ChanceList { std::array<Chance,N> values{}; unsigned count{}; };
using Chances=ChanceList<CollectorStatues::Count>;

template<size_t N>
constexpr ChanceList<N> EligibleFrom(const Kill& kill,const std::array<CollectorStatues::Definition,N>& definitions) noexcept {
    using namespace CollectorStatues;
    ChanceList<N> result;
    for(const auto& definition:definitions) {
        const auto& rule=definition.drop;
        if(rule.hellOnly&&kill.difficulty!=2)continue;
        bool matches{};Category category{};
        switch(rule.source) {
        case Source::Descent:
            matches=kill.area.descent&&kill.area.floor>=rule.firstFloor&&kill.area.floor<=rule.lastFloor;
            category=Category::Descent;break;
        case Source::Act:
            matches=!kill.area.descent&&!kill.area.cow&&kill.area.act==rule.selector;
            category=Category::Act;break;
        case Source::Cow:matches=kill.area.cow;category=Category::Cow;break;
        case Source::Class:matches=kill.playerClass==rule.selector;category=Category::Class;break;
        case Source::None:break;
        }
        if(matches) {
            const auto denominator=rule.source==Source::Descent&&kill.checkpointBoss&&rule.checkpointDenominator
                ?rule.checkpointDenominator:rule.denominator;
            result.values[result.count++]={definition.id,denominator,category};
        }
    }
    return result;
}
constexpr Chances Eligible(const Kill& kill) noexcept {return EligibleFrom(kill,CollectorStatues::Definitions);}

// One independent, uniformly distributed integer in [0, denominator) per
// eligible statue. Do not multiply chances by treasure picks or /players.
template<class Draw, class Award>
void Roll(const Kill& kill,Draw&& draw,Award&& award) {
    const auto chances=Eligible(kill);
    for(unsigned i=0;i<chances.count;++i)
        if(draw(chances.values[i].denominator)==0)award(chances.values[i]);
}

constexpr uint32_t StatueCode(unsigned statue) noexcept {
    return CollectorStatues::CodeById(statue);
}
}
