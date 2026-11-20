#pragma once

// The one game build the trainer knows: EXF-Win64-Shipping.exe, Steam build 21126030
// (PE timestamp 2025-12-10), appver 1.5.7. Every address here is for that build only; on any
// other the trainer stays off (IsSupportedBuild).

#include <cstdint>

namespace fexl::game {

// The game's executable in memory.
uintptr_t Exe();
bool IsSupportedBuild();

namespace rva {
// Battle.
constexpr uintptr_t PlayerTable = 0x2EED170;   // CPlayerBase* [2]
constexpr uintptr_t BattleFrameCount = 0x2EED158;  // uint64: +1 per battle frame, set anew at battle start
constexpr uintptr_t HurtLists = 0x2EF2458;     // hit object list head per player, 0x40 apart
constexpr uintptr_t ShellList = 0x2C324E8;     // the shells' (projectiles') hit objects
constexpr uintptr_t BattleKind = 0x2C32938;    // int: which battle class runs (sub_1401DB380)
constexpr uintptr_t BoneMapA = 0x27CDA60;      // int[]: sphere bone id -> index into BoneMapB
constexpr uintptr_t BoneMapB = 0x27CD440;      // int[]: -> bone matrix index
constexpr uintptr_t CameraEye = 0x2EF1640 + 0x190;  // the battle camera's eye (3 floats)
// Functions.
constexpr uintptr_t HitCheck = 0x1F6950;       // once per battle tick, after the players update
constexpr uintptr_t BattleTick = 0x1F1C80;     // a battle's tick (this, delta time); other modes run it too
constexpr uintptr_t XInputGetState = 0x43DDCC;  // the game's XInputGetState thunk, every reader's
// Unreal Engine.
constexpr uintptr_t GWorld = 0x2D99BC8;        // UWorld*
constexpr uintptr_t GEngine = 0x2D960C0;       // UEngine*
constexpr uintptr_t LineBatchClass = 0x2DB6240;  // UClass* of ULineBatchComponent
constexpr uintptr_t Realloc = 0x480640;        // FMemory::Realloc(pointer, size, alignment)
constexpr uintptr_t MarkRenderStateDirty = 0x73C0E0;  // UActorComponent::MarkRenderStateDirty
}  // namespace rva

// The first bytes of each hooked function, checked before hooking it.
constexpr uint8_t HitCheckPrologue[5] = {0x40, 0x53, 0x57, 0x41, 0x54};
constexpr uint8_t BattleTickPrologue[5] = {0x48, 0x89, 0x5C, 0x24, 0x08};
constexpr uint8_t XInputGetStatePrologue[5] = {0xFF, 0x25, 0x56, 0x93, 0x9A};  // jmp [import]

// Battle kinds (sub_1401DB380): 2 is Training, measured. The trainer works in Training only,
// so nothing of it can show, or change anything, in an online match.
constexpr int TrainingBattle = 2;

}  // namespace fexl::game
