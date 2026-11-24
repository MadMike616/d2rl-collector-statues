#pragma once
#include "drop_policy.h"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace CollectorDrops {
inline std::vector<std::string> Fields(std::string line) {
    if(!line.empty() && line.back()=='\r')line.pop_back();
    std::vector<std::string> result;
    size_t start{};
    for(;;) {
        const auto end=line.find('\t',start);
        result.push_back(line.substr(start,end==std::string::npos?end:end-start));
        if(end==std::string::npos)return result;
        start=end+1;
    }
}
inline unsigned Number(std::string_view text) {
    unsigned value{};
    const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    if(text.empty()||result.ec!=std::errc{}||result.ptr!=text.data()+text.size())
        throw std::runtime_error("invalid level number");
    return value;
}
inline std::map<unsigned,Area> ReadAreas(std::istream& input) {
    std::string line;
    if(!std::getline(input,line))throw std::runtime_error("missing levels header");
    if(line.starts_with("\xef\xbb\xbf"))line.erase(0,3);
    const auto header=Fields(line);
    std::map<std::string,size_t> columns;
    for(size_t i=0;i<header.size();++i)
        if(header[i].empty()||!columns.emplace(header[i],i).second)
            throw std::runtime_error("empty or duplicate levels column");
    for(const char* key:{"Id","Act","Name","LevelName","LevelType"})
        if(!columns.contains(key))throw std::runtime_error("missing required levels column");
    std::map<unsigned,Area> areas;
    std::set<unsigned> floors;
    unsigned entrances{},cows{};
    constexpr unsigned types[]{10,8,4,3,33,35};
    while(std::getline(input,line)) {
        if(line.empty()||line=="\r")continue;
        const auto row=Fields(line);
        if(row.size()!=header.size())throw std::runtime_error("invalid levels row width");
        auto get=[&](const char* key)->const std::string& {return row[columns.at(key)];};
        // Separator/end-marker rows do not define levels.
        if(get("Id").empty())continue;
        const auto id=Number(get("Id"));
        const auto act=Number(get("Act"));
        Area area{.act=act<5?static_cast<int>(act):-1};
        const auto& name=get("LevelName");
        if(name.starts_with("GrailerDescentFloor")) {
            const auto floor=Number(std::string_view(name).substr(std::string_view("GrailerDescentFloor").size()));
            if(floor<1||floor>60||!floors.insert(floor).second||act!=4
                ||Number(get("LevelType"))!=types[(floor-1)/10])
                throw std::runtime_error("inconsistent Descent floor/tileset");
            area.floor=floor;area.descent=true;
        } else if(name=="GrailerDescentEntrance") {
            ++entrances;area.descent=true;
        } else if(get("Name")=="Act 1 - Moo Moo Farm") {
            if(act!=0)throw std::runtime_error("invalid Cow Level act");
            ++cows;area.cow=true;
        }
        if(!areas.emplace(id,area).second)throw std::runtime_error("duplicate level id");
    }
    if(floors.size()!=60||entrances!=1||cows!=1)throw std::runtime_error("incomplete Descent/Cow level catalog");
    return areas;
}

inline void ValidateRepeatableStatues(std::istream& input,std::span<const CollectorStatues::Definition> definitions=CollectorStatues::Definitions) {
    if(!CollectorStatues::ValidDefinitions(definitions))throw std::runtime_error("invalid statue registration list");
    std::string line;
    if(!std::getline(input,line))throw std::runtime_error("missing statue uniqueitems.txt");
    if(line.starts_with("\xef\xbb\xbf"))line.erase(0,3);
    const auto header=Fields(line);
    std::map<std::string,size_t> columns;
    for(size_t i=0;i<header.size();++i)
        if(header[i].empty()||!columns.emplace(header[i],i).second)throw std::runtime_error("invalid uniqueitems header");
    for(const char* key:{"index","code","nolimit"})
        if(!columns.contains(key))throw std::runtime_error("missing uniqueitems key column");
    std::vector<unsigned> counts(definitions.size());
    while(std::getline(input,line)) {
        const auto row=Fields(line);
        if(row.size()!=header.size())continue;
        const auto& code=row[columns.at("code")];
        const auto index=CollectorStatues::IndexByCode(CollectorStatues::ParseCode(code),definitions);
        if(index<definitions.size()) {
            if(row[columns.at("nolimit")]!="1")throw std::runtime_error("statue unique definitions require nolimit=1 for repeated drops");
            ++counts[index];
        }
    }
    for(auto count:counts)if(count!=1)throw std::runtime_error("incomplete or duplicated statue unique definitions");
}

// Returns the checkpoint floor only for one of the 54 actual boss models in
// each section. Escorts and other monsters on a checkpoint retain 1/10,000.
inline unsigned CheckpointFloor(std::string_view id) noexcept {
    if(!id.starts_with("descent"))return 0;
    id.remove_prefix(7);
    unsigned floor=10;
    if(!id.starts_with('_')) {
        const auto separator=id.find('_');
        if(separator==std::string_view::npos)return 0;
        const auto digits=id.substr(0,separator);
        const auto result=std::from_chars(digits.data(),digits.data()+digits.size(),floor);
        if(result.ec!=std::errc{}||result.ptr!=digits.data()+digits.size())return 0;
        id.remove_prefix(separator);
        if(floor<20||floor>60||floor%10)return 0;
    }
    id.remove_prefix(1);
    constexpr std::string_view council[]{"ismail","geleb","toorc","bremm","wyand","maffer"};
    for(auto name:council)if(id==name)return floor;
    if(id.ends_with("_2")||id.ends_with("_3"))id.remove_suffix(2);
    constexpr std::string_view solo[]{"griswold","smith","radament","temptress","andariel","duriel",
        "mephisto","diablo","baal","bloodraven","summoner","izual","deseis","infector","vizier","lister"};
    for(auto name:solo)if(id==name)return floor;
    return 0;
}
}
