#include "game.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <emscripten/emscripten.h>
using namespace Rally;
static Game GameState;
static Renderer SceneRenderer;
static std::array<uint16_t, FramebufferWidth * FramebufferHeight> Pixels;
static SaveData Transfer;
static unsigned Previous = 0;
static char StatusText[1024];
extern "C" {
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
                  "drift means more sliding. Left and right change car; confirm chooses track.",
                  GameState.GetCarSpecification().Name, GameState.GetCarSpecification().Difficulty,
                  GameState.GetCarSpecification().SpeedStatistic,
                  GameState.GetCarSpecification().AccelerationStatistic,
                  GameState.GetCarSpecification().DriftStatistic);
    break;
  case GameMode::TrackSelect:
    if (GameState.Unlocked(GameState.SelectedTrack))
      std::snprintf(StatusText, sizeof(StatusText),
                    "Choose track: %s. Unlocked. Target %.2f seconds. Personal best %.2f seconds. "
                    "Confirm to race; back to cars.",
                    TrackNames[GameState.SelectedTrack], GameState.GetDefaultSplits().back(),
                    GameState.Best);
    else
      std::snprintf(StatusText, sizeof(StatusText),
                    "Choose track: %s. Locked. Beat %s to unlock. Left and right browse tracks; "
                    "back to cars.",
                    TrackNames[GameState.SelectedTrack], TrackNames[GameState.SelectedTrack - 1]);
    break;
  case GameMode::Countdown:
    std::snprintf(StatusText, sizeof(StatusText), "Get ready. Racing starts after the countdown.");
    break;
  case GameMode::Racing:
    std::snprintf(StatusText, sizeof(StatusText), "Racing. Checkpoint %d of 5. P pauses.",
                  std::min(5, GameState.SplitCount + 1));
    break;
  case GameMode::Paused:
    std::snprintf(StatusText, sizeof(StatusText),
                  "Paused. Resume, retry, or return to car selection.");
    break;
  case GameMode::Finished:
    std::snprintf(StatusText, sizeof(StatusText),
                  "Stage complete. Time %.2f seconds; target %.2f seconds. %s. Confirm retries; "
                  "back chooses a track.",
                  GameState.Elapsed, GameState.GetDefaultSplits().back(),
                  GameState.Elapsed < GameState.GetDefaultSplits().back() ? "Target beaten"
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
  return GameState.Unlocked(GameState.SelectedTrack);
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
    GameState.CurrentMode = GameMode::Paused;
  }
}
EMSCRIPTEN_KEEPALIVE int GravelbyteCar() { return GameState.SelectedCar; }
EMSCRIPTEN_KEEPALIVE int GravelbyteTrack() { return GameState.SelectedTrack; }
EMSCRIPTEN_KEEPALIVE int GravelbyteMode() { return int(GameState.CurrentMode); }
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
