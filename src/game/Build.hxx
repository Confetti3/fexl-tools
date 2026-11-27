#pragma once

// The one game build the trainer knows: EXF-Win64-Shipping.exe, Steam build 26967029
// (PE timestamp 2026-11-27). Every address here is for that build only; on any other the
// trainer stays off (IsSupportedBuild).

#include <cstdint>

namespace fexl::game {

// The game's executable in memory.
uintptr_t Exe();
bool IsSupportedBuild();

namespace rva {
// Battle.
constexpr uintptr_t PlayerTable = 0x2EEF270;   // CPlayerBase* [2]
constexpr uintptr_t BattleFrameCount = 0x2EEF258;  // uint64: +1 per battle frame, set anew at battle start
constexpr uintptr_t HurtLists = 0x2EF4558;     // hit object list head per player, 0x40 apart
constexpr uintptr_t ShellList = 0x2C345B8;     // the shells' (projectiles') hit objects
constexpr uintptr_t BattleKind = 0x2C34A08;    // int: which battle class runs (sub_1401DB380)
constexpr uintptr_t BoneMapA = 0x27CFA60;      // int[]: sphere bone id -> index into BoneMapB
constexpr uintptr_t BoneMapB = 0x27CF440;      // int[]: -> bone matrix index
constexpr uintptr_t CameraEye = 0x2EF3740 + 0x190;  // the battle camera's eye (3 floats)
// Functions.
constexpr uintptr_t HitCheck = 0x1F6950;       // once per battle tick, after the players update
constexpr uintptr_t BattleTick = 0x1F1C80;     // a battle's tick (this, delta time); other modes run it too
// The throw (catch) test, sub_1401E4F60(player, x, y, range, catch type): a throw calls it on its
// active frame (measured: a ground throw once, on its third frame), after that frame's hit check.
constexpr uintptr_t CatchTest = 0x1E4F60;
constexpr uintptr_t XInputGetState = 0x43E90C;  // the game's XInputGetState thunk, every reader's
// Unreal Engine.
constexpr uintptr_t GWorld = 0x2D9BCC8;        // UWorld*
constexpr uintptr_t GEngine = 0x2D981C0;       // UEngine*
constexpr uintptr_t LineBatchClass = 0x2DB8340;  // UClass* of ULineBatchComponent
constexpr uintptr_t Realloc = 0x481BD0;        // FMemory::Realloc(pointer, size, alignment)
constexpr uintptr_t MarkRenderStateDirty = 0x73D480;  // UActorComponent::MarkRenderStateDirty
}  // namespace rva

// The first bytes of each hooked function, checked before hooking it.
constexpr uint8_t HitCheckPrologue[5] = {0x40, 0x53, 0x57, 0x41, 0x54};
constexpr uint8_t BattleTickPrologue[5] = {0x48, 0x89, 0x5C, 0x24, 0x08};
constexpr uint8_t CatchTestPrologue[5] = {0x48, 0x8B, 0xC4, 0x48, 0x89};
constexpr uint8_t XInputGetStatePrologue[5] = {0xFF, 0x25, 0x16, 0x98, 0x9A};  // jmp [import]

// Battle kinds (sub_1401DB380): 2 is Training, measured. The trainer works in Training only,
// so nothing of it can show, or change anything, in an online match.
constexpr int TrainingBattle = 2;

}  // namespace fexl::game
