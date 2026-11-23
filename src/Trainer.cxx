#include "Trainer.hxx"

#include "Detour.hxx"
#include "Log.hxx"
#include "Playback.hxx"
#include "Settings.hxx"
#include "game/Battle.hxx"
#include "game/Build.hxx"
#include "game/Hooks.hxx"
#include "game/MeterSample.hxx"
#include "scene/HitboxScene.hxx"

#include <windows.h>

#include <array>
#include <optional>
#include <utility>
#include <vector>

namespace fexl {

namespace {

// Frames are on screen this long after the battle last ran or was held.
constexpr uint64_t OnScreenForMs = 250;

// Game thread only: the Training battle followed, begun by the battle tick on its first frame
// (or a frame whose identity differs) and ended by the gate.
struct Session {
  game::BattleIdentity identity;
  uint32_t number = 0;                         // TrainerView::battle
  // The frame the running tick's hit check read; the meter follows it once the tick has run.
  std::optional<game::BattleFrame> running;
  std::optional<game::BattleFrame> previous;  // the last frame followed, absent until the first
  std::array<training::MeterCell, 2> previousCells{};  // the cells that frame gave
  training::FrameMeter meter;
  bool meterChanged = true;  // since the last snapshot
  Playback playback;
};
std::optional<Session> s_session;
uint32_t s_battles;  // battles followed so far

// Shared with the overlay, each under its lock.
SRWLOCK s_viewLock = SRWLOCK_INIT;
TrainerView s_view;
uint64_t s_lastFrameMs;

SRWLOCK s_mailLock = SRWLOCK_INIT;
std::vector<std::pair<uint32_t, PlaybackCommand>> s_mail;
uint32_t s_menuBattle;

bool DebugLog() {
  static const bool on = [] {
    char value[4] = {};
    return GetEnvironmentVariableA("FEXL_DEBUG", value, sizeof(value)) > 0 && value[0] == '1';
  }();
  return on;
}

// The snapshot, in one piece, once per game frame of the session (run or held) and on every
// change of session. The meter is copied only when it changed; it is there exactly while a
// battle is (battle != 0).
void Publish(const PlaybackSettings& settings) {
  std::shared_ptr<const training::MeterView> meter;
  const bool newMeter = !s_session || s_session->meterChanged;
  if (s_session && s_session->meterChanged) {
    meter = std::make_shared<const training::MeterView>(s_session->meter.View());
    s_session->meterChanged = false;
  }
  const bool filled = scene::FilledSpheresAvailable();
  AcquireSRWLockExclusive(&s_viewLock);
  s_view.battle = s_session ? s_session->number : 0;
  s_view.paused = s_session && s_session->playback.Paused();
  s_view.holding = s_session && s_session->playback.Holding(settings);
  s_view.meshesAvailable = filled;
  if (newMeter) s_view.meter = std::move(meter);
  s_lastFrameMs = GetTickCount64();
  ReleaseSRWLockExclusive(&s_viewLock);
}

// Every change of session, beginning one or ending it: nothing of the last one stays, and no
// command meant for it is carried out.
void Switch(std::optional<Session> next) {
  scene::ClearHitboxes();
  game::ForgetThrowTests();
  s_session = std::move(next);
  AcquireSRWLockExclusive(&s_mailLock);
  s_mail.clear();
  ReleaseSRWLockExclusive(&s_mailLock);
  Publish(CurrentSettings().playback);
}

// The one Training check, first in the battle tick and the hit check. Every battle mode runs both
// hooked functions; outside Training the session ends and the hook does nothing more. Ending it takes the
// trainer's own spheres back out of the scene (scene::ClearHitboxes) and writes nothing else.
bool TrainingGate() {
  if (game::InTraining()) return true;
  if (s_session) Switch(std::nullopt);
  return false;
}

// The overlay's commands for this session, carried out now.
void ReadMail(Session& session) {
  std::vector<std::pair<uint32_t, PlaybackCommand>> mail;
  AcquireSRWLockExclusive(&s_mailLock);
  mail.swap(s_mail);
  const uint32_t menuBattle = s_menuBattle;
  ReleaseSRWLockExclusive(&s_mailLock);
  for (const auto& [battle, command] : mail) {
    if (battle != session.number) continue;
    if (command == PlaybackCommand::TogglePause) session.playback.TogglePause();
    else session.playback.Step();
  }
  session.playback.SetMenuOpen(menuBattle == session.number);
}

// A frame read: the meter fed.
void Follow(Session& session, const game::BattleFrame& frame) {
  std::array<training::MeterCell, 2> cells;
  for (int side = 0; side < 2; ++side) {
    const game::Fighter& now = frame.fighters[side];
    const game::Fighter& before = session.previous ? session.previous->fighters[side] : now;
    if (DebugLog() && before.act != now.act)
      Log("P%d act %d -> %d at motion frame %u", side + 1, before.act, now.act, now.motionFrame);
    cells[side] = game::MeterSample(now, before, session.previousCells[side]);
  }
  session.previousCells = cells;
  const bool wasOpen = session.meter.Open();
  session.meterChanged = session.meter.Push(cells) || session.meterChanged;
  // With FEXL_DEBUG=1, one line per finished exchange, for field reports and the test rig.
  if (DebugLog() && wasOpen && !session.meter.Open()) Log("exchange %s", training::Describe(session.meter.View()).c_str());
  session.previous = frame;
}

// The battle tick, every battle mode's. In Training, playback decides whether this game frame
// runs the battle; a frame that ran is followed once it is complete (the game tests throws after
// the hit check reads the frame). A held frame still redraws its spheres, as the settings are now.
using BattleTickFn = void(__fastcall*)(void*, float);
BattleTickFn o_battleTick;

void __fastcall BattleTick(void* battle, float delta) {
  if (!TrainingGate()) {
    o_battleTick(battle, delta);
    return;
  }
  const game::BattleIdentity identity = game::ReadBattleIdentity(battle);
  if (!s_session || !identity.Continues(s_session->identity)) {
    Session next;
    next.number = ++s_battles;
    next.identity = identity;
    Log("Training battle %u", next.number);
    Switch(std::move(next));
  }
  Session& session = *s_session;
  session.identity = identity;
  ReadMail(session);
  const Settings settings = CurrentSettings();
  if (session.playback.RunsThisFrame(settings.playback)) {
    o_battleTick(battle, delta);  // the hit check reads the frame
    // The gate in the hit check may have ended the session meanwhile.
    if (s_session && s_session->running) {
      game::ReadThrowTests(*s_session->running);
      Follow(*s_session, *s_session->running);
      s_session->running.reset();
    }
  } else if (session.previous) {
    scene::DrawHitboxes(*session.previous, settings.hitboxes);
  }
  if (s_session) Publish(settings.playback);
}

// The hit check, every battle mode's, once per battle frame that runs: the frame is read before
// the game tests it, for the battle tick to follow. A frame that does not read (a fighter between
// two of its objects) is skipped.
using HitCheckFn = uint64_t(__fastcall*)(void*, void*);
HitCheckFn o_hitCheck;

uint64_t __fastcall HitCheck(void* a, void* b) {
  game::BattleFrame frame;
  if (TrainingGate() && s_session && game::ReadBattleFrame(frame)) {
    // One hit check per tick is all that has been seen; should a tick run two, none is lost.
    if (s_session->running) Follow(*s_session, *s_session->running);
    s_session->running = frame;
    scene::DrawHitboxes(frame, CurrentSettings().hitboxes);
  }
  return o_hitCheck(a, b);
}

// The throw (catch) test, called from a throw's script on its active frame, and from elsewhere in
// the game (the CPU's choices among them). In Training it notes whose throw was tested, for the
// battle tick to read once the frame has run; it changes nothing, in any mode.
using CatchTestFn = uint64_t(__fastcall*)(void*, float, float, float, int);
CatchTestFn o_catchTest;

uint64_t __fastcall CatchTest(void* player, float x, float y, float range, int type) {
  if (game::InTraining()) game::NoteCatchTest(player);
  return o_catchTest(player, x, y, range, type);
}

}  // namespace

bool InstallTrainer() {
  // All or none: a tick hook without the hit check would hold Training frames for a trainer
  // that shows nothing, and without the catch test throws would show no active frames.
  if (game::HookGameFunction("hit check", game::rva::HitCheck, game::HitCheckPrologue, reinterpret_cast<void*>(&HitCheck),
                             reinterpret_cast<void**>(&o_hitCheck)) &&
      game::HookGameFunction("battle tick", game::rva::BattleTick, game::BattleTickPrologue,
                             reinterpret_cast<void*>(&BattleTick), reinterpret_cast<void**>(&o_battleTick)) &&
      game::HookGameFunction("catch test", game::rva::CatchTest, game::CatchTestPrologue, reinterpret_cast<void*>(&CatchTest),
                             reinterpret_cast<void**>(&o_catchTest)))
    return true;
  UnhookAll();
  return false;
}

TrainerView CurrentView() {
  AcquireSRWLockShared(&s_viewLock);
  TrainerView view = s_view;
  view.onScreen = view.battle != 0 && GetTickCount64() - s_lastFrameMs < OnScreenForMs;
  ReleaseSRWLockShared(&s_viewLock);
  return view;
}

void Post(uint32_t battle, PlaybackCommand command) {
  if (battle == 0) return;
  AcquireSRWLockExclusive(&s_mailLock);
  s_mail.emplace_back(battle, command);
  ReleaseSRWLockExclusive(&s_mailLock);
}

void SetMenuOpen(uint32_t battle) {
  AcquireSRWLockExclusive(&s_mailLock);
  s_menuBattle = battle;
  ReleaseSRWLockExclusive(&s_mailLock);
}

}  // namespace fexl
