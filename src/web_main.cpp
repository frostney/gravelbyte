#include "game.hpp"
#include "ghost.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <emscripten/emscripten.h>
using namespace GravelByte;
static Game GameState;
static Renderer SceneRenderer;
static std::array<uint16_t, FramebufferWidth * FramebufferHeight> Pixels;
static SaveData Transfer;
static GhostTransfer GhostBuffer;
static unsigned Previous = 0;
static char StatusText[1024];
extern "C" {
EMSCRIPTEN_KEEPALIVE int GravelbyteRandomRequested() { return GameState.RandomRequested; }
EMSCRIPTEN_KEEPALIVE void GravelbyteInitialSeed(uint32_t Seed) { GameState.ChallengeSeed = Seed; }
EMSCRIPTEN_KEEPALIVE void GravelbyteSetSeed(uint32_t Seed) { GameState.SetChallengeSeed(Seed); }
EMSCRIPTEN_KEEPALIVE uint32_t GravelbyteSeed() { return GameState.ChallengeSeed; }
EMSCRIPTEN_KEEPALIVE int GravelbyteChallenge() { return GameState.Challenge; }
EMSCRIPTEN_KEEPALIVE int GravelbyteSeedEditor() { return GameState.SeedEditor; }
EMSCRIPTEN_KEEPALIVE int GravelbyteVariantsUnlocked() { return GameState.VariantUnlocked(); }
EMSCRIPTEN_KEEPALIVE int GravelbyteGhostPending() { return GameState.GhostSaveRequested; }
EMSCRIPTEN_KEEPALIVE void GravelbyteGhostMarkSaved() { GameState.GhostSaveRequested = false; }
EMSCRIPTEN_KEEPALIVE unsigned GravelbyteGhostGeneration() { return GameState.GhostLoadGeneration; }
EMSCRIPTEN_KEEPALIVE int GravelbyteRecordIndex() { return GameState.RecordIndex(); }
EMSCRIPTEN_KEEPALIVE int GravelbyteGhostBytes() { return sizeof(GhostBuffer); }
EMSCRIPTEN_KEEPALIVE GhostTransfer *GravelbyteGhostBuffer() {
  GameState.GhostPoses = nullptr;
  GameState.GhostCount = 0;
  return &GhostBuffer;
}
EMSCRIPTEN_KEEPALIVE GhostTransfer *GravelbyteExportGhost() {
  GameState.ClearGhost();
  FillGhostTransfer(GameState, GhostBuffer);
  AttachGhost(GameState, GhostBuffer);
  return &GhostBuffer;
}
EMSCRIPTEN_KEEPALIVE int GravelbyteImportGhost(unsigned Generation) {
  return Generation == GameState.GhostLoadGeneration && AttachGhost(GameState, GhostBuffer);
}
EMSCRIPTEN_KEEPALIVE const char *GravelbyteBuildIdentifier() { return GRAVELBYTE_BUILD_ID; }
EMSCRIPTEN_KEEPALIVE const char *GravelbyteStatus() {
  switch (GameState.CurrentMode) {
  case GameMode::Title:
    std::snprintf(StatusText, sizeof(StatusText),
                  "Gravelbyte. Press Enter or confirm to choose a car.");
    break;
  case GameMode::CarSelect:
    std::snprintf(StatusText, sizeof(StatusText),
                  "Choose car: %s, %s. Speed %d of 5, acceleration %d of 5, drift %d of 5. Higher "
                  "drift means more sliding. Steering assist %s. Left and right change car; "
                  "confirm chooses track.",
                  GameState.GetCarSpecification().Name, GameState.GetCarSpecification().Difficulty,
                  GameState.GetCarSpecification().SpeedStatistic,
                  GameState.GetCarSpecification().AccelerationStatistic,
                  GameState.GetCarSpecification().DriftStatistic,
                  GameState.SteeringAssist ? "on" : "off");
    break;
  case GameMode::TrackSelect:
    if (GameState.Challenge) {
      if (GameState.SeedEditor)
        std::snprintf(StatusText, sizeof(StatusText),
                      "Edit seed %08lX. Digit %d selected. Left and right choose digit; up and "
                      "down change it. Confirm sets seed; back cancels.",
                      static_cast<unsigned long>(GameState.EditingSeed), GameState.SeedDigit + 1);
      else
        std::snprintf(StatusText, sizeof(StatusText),
                      "Random challenge. Seed %08lX. Session best %.2f seconds. Confirm to race; "
                      "the New random stage button generates "
                      "another stage; left or right edits the seed. No medals or campaign unlocks.",
                      static_cast<unsigned long>(GameState.ChallengeSeed), GameState.Best);
      break;
    }
    if (GameState.Unlocked(GameState.SelectedTrack))
      std::snprintf(StatusText, sizeof(StatusText),
                    "Choose track: %s, %s. Unlocked. Bronze %.2f seconds, silver %.2f, gold %.2f. "
                    "Personal best %.2f seconds. Personal best ghost %s. Left and right choose "
                    "variant when unlocked. "
                    "Confirm to race; back to cars.",
                    TrackNames[GameState.SelectedTrack],
                    std::array<const char *, 4>{"Original", "Reverse", "Mirror",
                                                "Reverse mirror"}[GameState.SelectedVariant],
                    GameState.GetDefaultSplits().back(), GameState.MedalTarget(2),
                    GameState.MedalTarget(3), GameState.Best,
                    GameState.GhostCount > 1 ? "available" : "unavailable");
    else
      std::snprintf(StatusText, sizeof(StatusText),
                    "Choose track: %s. Locked. Beat %s to unlock. Up and down browse tracks; "
                    "back to cars.",
                    TrackNames[GameState.SelectedTrack], TrackNames[GameState.SelectedTrack - 1]);
    break;
  case GameMode::Countdown:
    std::snprintf(StatusText, sizeof(StatusText), "%sGet ready. Racing starts after the countdown.",
                  GameState.Practice ? "Practice. No records or medals. " : "");
    break;
  case GameMode::Racing:
    std::snprintf(StatusText, sizeof(StatusText), "%sRacing. Checkpoint %d of 5. P pauses.",
                  GameState.Practice ? "Practice. No records or medals. " : "",
                  std::min(5, GameState.SplitCount + 1));
    break;
  case GameMode::Paused:
    std::snprintf(StatusText, sizeof(StatusText),
                  "%s. Selected: %s. Up and down select; confirm activates; back returns.",
                  GameState.OptionsOpen ? "Options" : "Paused", GameState.MenuChoice());
    break;
  case GameMode::Finished:
    if (GameState.Practice) {
      std::snprintf(StatusText, sizeof(StatusText),
                    "Practice complete. No records or medals awarded. Confirm restarts a full "
                    "eligible run; back chooses course.");
      break;
    }
    if (GameState.Challenge) {
      std::snprintf(StatusText, sizeof(StatusText),
                    "Challenge complete. Seed %08lX. Time %.2f seconds; session best %.2f. Confirm "
                    "retries; back chooses course.",
                    static_cast<unsigned long>(GameState.ChallengeSeed), GameState.Elapsed,
                    GameState.Best);
      break;
    }
    std::snprintf(StatusText, sizeof(StatusText),
                  "Stage complete. Time %.2f seconds; target %.2f seconds. %s. Confirm retries; "
                  "back chooses a track.",
                  GameState.Elapsed, GameState.GetDefaultSplits().back(),
                  GameState.Elapsed <= GameState.GetDefaultSplits().back() ? "Target beaten"
                                                                           : "Target not beaten");
    if (GameState.ShowRecords) {
      std::size_t Used = std::strlen(StatusText);
      for (int Index = 0; Index < SectorCount && Used < sizeof(StatusText); ++Index)
        Used += std::snprintf(StatusText + Used, sizeof(StatusText) - Used,
                              " Checkpoint %d: %+.2f seconds against target.", Index + 1,
                              GameState.Splits[Index] - GameState.GetDefaultSplits()[Index]);
    }
    break;
  }
  return StatusText;
}
EMSCRIPTEN_KEEPALIVE int GravelbyteTrackUnlocked() {
  return GameState.Challenge || GameState.Unlocked(GameState.SelectedTrack);
}

EMSCRIPTEN_KEEPALIVE const uint16_t *GravelbyteFrame() {
  SceneRenderer.Render(GameState, Pixels.data());
  return Pixels.data();
}
EMSCRIPTEN_KEEPALIVE void GravelbyteUpdate(float DeltaTimeSeconds, unsigned Buttons,
                                           int ControlScheme) {
  GameState.ControlScheme = static_cast<Controls>(std::clamp(ControlScheme, 1, 3));
  const unsigned Edges = Buttons & ~Previous;
  Previous = Buttons;
  DrivingInput PlayerInput;
  PlayerInput.Up = Buttons & 512;
  PlayerInput.Down = Buttons & 1024;
  PlayerInput.Left = Buttons & 1;
  PlayerInput.Right = Buttons & 2;
  PlayerInput.Throttle = Buttons & 4;
  PlayerInput.Brake = Buttons & 8;
  PlayerInput.Handbrake = Buttons & 16;
  PlayerInput.Action = Edges & 32;
  PlayerInput.Pause = Edges & 64;
  PlayerInput.Back = Edges & 128;
  PlayerInput.Auxiliary = Edges & (16 | 256);
  GameState.Update(DeltaTimeSeconds, PlayerInput);
}
EMSCRIPTEN_KEEPALIVE void GravelbyteSuspend() {
  Previous = 0;
  if (GameState.CurrentMode == GameMode::Racing || GameState.CurrentMode == GameMode::Countdown) {
    GameState.ResumeMode = GameState.CurrentMode;
    GameState.OptionsOpen = false;
    GameState.MenuSelection = 0;
    GameState.CurrentMode = GameMode::Paused;
  }
}
EMSCRIPTEN_KEEPALIVE int GravelbyteCar() { return GameState.SelectedCar; }
EMSCRIPTEN_KEEPALIVE int GravelbyteTrack() { return GameState.SelectedTrack; }
EMSCRIPTEN_KEEPALIVE int GravelbyteMode() { return int(GameState.CurrentMode); }
EMSCRIPTEN_KEEPALIVE float GravelbyteEngineFrequency() { return GameState.EngineFrequency(); }
EMSCRIPTEN_KEEPALIVE float GravelbyteSpeed() { return GameState.Speed; }
EMSCRIPTEN_KEEPALIVE int GravelbyteMuted() { return GameState.Muted; }
EMSCRIPTEN_KEEPALIVE void GravelbyteToggleAudio() { GameState.ToggleAudio(); }
EMSCRIPTEN_KEEPALIVE int GravelbyteSavePending() { return GameState.SaveRequested; }
EMSCRIPTEN_KEEPALIVE void GravelbyteMarkSaved() { GameState.SaveRequested = false; }
EMSCRIPTEN_KEEPALIVE int GravelbyteSaveSize() { return sizeof(Transfer); }
EMSCRIPTEN_KEEPALIVE SaveData *GravelbyteSave() {
  Transfer = EncodeSave(GameState);
  return &Transfer;
}
EMSCRIPTEN_KEEPALIVE int GravelbyteLoad() { return LoadSave(GameState, Transfer); }
}
