#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
// Independently extracted from the local loader-hosted runtime text capture.
// Each call site and register-liveness witness must match before installation.
namespace CollectorProfile {
template<size_t N> consteval auto Hex(const char (&text)[N]) {
    std::array<uint8_t,(N-1)/2> result{};
    auto digit=[](char c) { return c<='9'?c-'0':c-'a'+10; };
    for(size_t i=0;i<result.size();++i)result[i]=static_cast<uint8_t>(digit(text[i*2])*16+digit(text[i*2+1]));
    return result;
}
inline constexpr uintptr_t ClassLayoutRva=0x349860;
inline constexpr auto ClassLayout=Hex("4883ec284885c9751d884c2430488d4c2430e849cbffff84c07401ccb8ffffffff4883c428c38b41044883c428c3");
inline constexpr uintptr_t FormatLayoutRva=0x34a0e0;
inline constexpr auto FormatLayout=Hex("4883ec284885c9751a884c2430488d4c2430e849c7ffff84c07401cc32c04883c428c30fb681bd0100004883c428c3");
inline constexpr uintptr_t ModeRva=0xce500;
inline constexpr auto Mode=Hex("4863c1488d0d96c895020fb60408c3");
inline constexpr uintptr_t BlockedRva=0x8460d0;
inline constexpr auto Blocked=Hex("488b0599a0bf024885c074080fb680b8000000c3c3");
inline constexpr uintptr_t EntryARva=0x2bd480;
inline constexpr auto EntryA=Hex("40555356574154415541564157488dac24f8b1ffffb8084f0000e8413c0101482be0488b051fde70024833c4488985f04d0000488b85884e0000498bd94c8bad904e00004d8be048894598488bfa4c896c2470");
inline constexpr uintptr_t EntryBRva=0x2be560;
inline constexpr auto EntryB=Hex("48895c24185556574154415541564157488dac24d0b1ffffb8304f0000e85e2b0101482be0488b053ccd70024833c4488985204e0000488b85904e0000488bf24c8bbd984e0000488bd948894424604c897c2470");
inline constexpr uintptr_t WindowARva=0x2be108;
inline constexpr auto WindowA=Hex("41b82f070000488bcbe8ba010b0085c090904c8bc7488d55d0488bcbe8273d0000410fb6450888458084c07409498b45004889442478488d4424784533e44889442430488d95f0090000488b459841b101488944242841b800040000488bcb4c89642420e86fbf0100eb37b97f0d0000e8b3d5b6034c8bc0488d8df00d0000ba00040000e8af96f6004c8b442448488d8df00d0000ba00040000e8f994f6004533e480bd804e000000751e");
inline constexpr uintptr_t WindowBRva=0x2bed99;
inline constexpr auto WindowB=Hex("448b442450488bd7488bcee8b7e5ffff4c8d0db044a301ba1000000041b817080000488bce448bf0e80af50a0085c0742dba000800004c8d0d8a44a301488bce448d421ae8eef40a0085c075434c8bc7488d5500488bcee85b300000eb32b97f0d0000e82fc9b6034c8bc0488d8d200a0000ba00040000e82b8af6004d8bc4488d8d200a0000ba00040000e87788f6008b4424404c8d45000fb64c24304c8bcf488bd689442420e83b2d0000488d9500");
inline constexpr uintptr_t CallARva=0x2be178;
inline constexpr auto CallA=Hex("e8b3d5b603");
inline constexpr uintptr_t CallBRva=0x2bedfc;
inline constexpr auto CallB=Hex("e82fc9b603");
}
