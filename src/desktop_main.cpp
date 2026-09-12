#include "game.hpp"
#include "ghost.hpp"
#include "test_driver.hpp"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

using namespace GravelByte;
static Game GameState;
static Renderer SceneRenderer;
static std::array<uint16_t, FramebufferWidth * FramebufferHeight> Framebuffer;
static std::array<uint32_t, FramebufferWidth * FramebufferHeight> RedGreenBlueAlpha;
static std::string SavePath;
static float AudioPhase = 0;
static SDL_AudioDeviceID AudioDevice = 0;
static float SoundFrequency = 70, SoundVolume = 0;
static void GenerateAudio(void *, Uint8 *Stream, int Bytes) {
  auto *Samples = reinterpret_cast<int16_t *>(Stream);
  for (int Index = 0; Index < Bytes / 2; ++Index) {
    AudioPhase += SoundFrequency / float(Tuning::Audio::SampleRate);
    if (AudioPhase >= 1)
      AudioPhase -= 1;
#ifdef GRAVELBYTE_TEST_SILENT
    Samples[Index] = 0;
#else
    Samples[Index] = int16_t((AudioPhase < .45f ? 1 : -1) * SoundVolume * 1800);
#endif
  }
}
static GhostTransfer GhostBuffer;
static int LoadedGhostSlot = -1;
static void UpdateGhostStorage() {
  static uint32_t Generation = UINT32_MAX;
  const std::string Prefix = SavePath + ".ghost-" + std::to_string(GameState.RecordIndex()) + "-";
  if (GameState.GhostSaveRequested) {
    GameState.GhostSaveRequested = false;
    const std::string Path = Prefix + std::to_string(LoadedGhostSlot == 0 ? 1 : 0);
    FillGhostTransfer(GameState, GhostBuffer);
    const std::string Temporary = Path + ".tmp";
    if (FILE *File = std::fopen(Temporary.c_str(), "wb")) {
      const bool Written = std::fwrite(&GhostBuffer, sizeof(GhostBuffer), 1, File) == 1;
      const bool Closed = std::fclose(File) == 0;
      if (Written && Closed)
        std::rename(Temporary.c_str(), Path.c_str());
    }
    GameState.ClearGhost();
  }
  if (Generation == GameState.GhostLoadGeneration)
    return;
  Generation = GameState.GhostLoadGeneration;
  LoadedGhostSlot = -1;
  for (int Slot = 0; Slot < 2; ++Slot) {
    const std::string Path = Prefix + std::to_string(Slot);
    if (FILE *File = std::fopen(Path.c_str(), "rb")) {
      const bool Read = std::fread(&GhostBuffer, sizeof(GhostBuffer), 1, File) == 1;
      std::fclose(File);
      if (Read && AttachGhost(GameState, GhostBuffer)) {
        LoadedGhostSlot = Slot;
        break;
      }
    }
  }
}
static void Save() {
  UpdateGhostStorage();
  if (!GameState.SaveRequested)
    return;
  SaveData SavedRecord = EncodeSave(GameState);
  std::string Temporary = SavePath + ".tmp";
  if (FILE *File = std::fopen(Temporary.c_str(), "wb")) {
    const bool Written = std::fwrite(&SavedRecord, sizeof(SavedRecord), 1, File) == 1;
    const bool Closed = std::fclose(File) == 0;
    if (Written && Closed && std::rename(Temporary.c_str(), SavePath.c_str()) == 0)
      GameState.SaveRequested = false;
  }
}
static void Screenshot(const char *Path) {
  FILE *FileHandle = std::fopen(Path, "wb");
  if (!FileHandle) {
    std::perror(Path);
    std::exit(2);
  }
  std::fprintf(FileHandle, "P6\n%d %d\n255\n", FramebufferWidth, FramebufferHeight);
  for (uint16_t ColorValue : Framebuffer) {
    unsigned char RedGreenBlue[] = {uint8_t((ColorValue >> 12) * 17),
                                    uint8_t(((ColorValue >> 8) & 15) * 17),
                                    uint8_t(((ColorValue >> 4) & 15) * 17)};
    std::fwrite(RedGreenBlue, 1, 3, FileHandle);
  }
  std::fclose(FileHandle);
}
int main(int ArgumentCount, char **Arguments) {
  GameState.ControlScheme = Controls::Keyboard;
  std::random_device Random;
  GameState.ChallengeSeed = Random();
  if (ArgumentCount > 1 && std::strcmp(Arguments[1], "--capture") == 0) {
    int Segment = ArgumentCount > 3 ? std::atoi(Arguments[3]) : 20;
    Segment = std::max(0, std::min(NodeCount - 2, Segment));
    if (ArgumentCount > 6)
      GameState.SelectCarAndTrack(std::atoi(Arguments[5]), std::atoi(Arguments[6]));
    GameState.Segment = Segment;
    GameState.CarPosition = GameState.Road[Segment].Position;
    GameState.Yaw = GameState.Road[Segment].Heading;
    GameState.CameraYaw = GameState.Yaw;
    GameState.GroundY = GameState.CarPosition.CoordinateY;
    GameState.CameraHeight = GameState.CarPosition.CoordinateY;
    GameState.Pitch = (GameState.Road[Segment + 1].Position.CoordinateY -
                       GameState.Road[Segment].Position.CoordinateY) /
                      GameState.SegmentLength;
    GameState.Roll = GameState.Road[Segment].Bank;
    GameState.CurrentMode = GameMode::Racing;
    GameState.Elapsed = 32.45f;
    GameState.Speed = 22.8f;
    if (ArgumentCount > 4 && std::strcmp(Arguments[4], "cars") == 0)
      GameState.CurrentMode = GameMode::CarSelect;
    if (ArgumentCount > 4 && std::strcmp(Arguments[4], "tracks") == 0)
      GameState.CurrentMode = GameMode::TrackSelect;
    if (ArgumentCount > 4 && std::strcmp(Arguments[4], "title") == 0)
      GameState.CurrentMode = GameMode::Title;
    if (ArgumentCount > 4 && std::strcmp(Arguments[4], "finish") == 0) {
      GameState = Game{};
      for (int Index = 0; Index < 15000 && GameState.CurrentMode != GameMode::Finished; ++Index)
        GameState.Update(.02f, TestDriver(GameState));
    }
    if (ArgumentCount > 4 && std::strcmp(Arguments[4], "pause") == 0)
      GameState.CurrentMode = GameMode::Paused;
    if (ArgumentCount > 7)
      GameState.CinematicTime = std::atof(Arguments[7]);
    if (ArgumentCount > 8)
      GameState.MenuRotation = std::atof(Arguments[8]);
    SceneRenderer.Render(GameState, Framebuffer.data());
    Screenshot(ArgumentCount > 2 ? Arguments[2] : "frame.ppm");
    std::printf("Captured %d triangles; %d dropped\n", SceneRenderer.FaceCount,
                SceneRenderer.Dropped);
    return 0;
  }
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "%s\n", SDL_GetError());
    return 1;
  }
  char *Preferences = SDL_GetPrefPath("gravelbyte", "gravelbyte");
  if (!Preferences) {
    std::fprintf(stderr, "No writable save directory: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SavePath = std::string(Preferences) + "records-v4.best";
  SDL_free(Preferences);
  if (FILE *File = std::fopen(SavePath.c_str(), "rb")) {
    SaveData SavedRecord{};
    if (std::fread(&SavedRecord, sizeof(SavedRecord), 1, File) == 1)
      LoadSave(GameState, SavedRecord);
    std::fclose(File);
  }
  SDL_Window *Window = SDL_CreateWindow("gravelbyte — PicoSystem preview", SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, 720, 720,
                                        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Renderer *Screen =
      Window ? SDL_CreateRenderer(Window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
             : nullptr;
  if (!Screen) {
    std::fprintf(stderr, "%s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_RenderSetLogicalSize(Screen, FramebufferWidth, FramebufferHeight);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  SDL_Texture *Texture =
      SDL_CreateTexture(Screen, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                        FramebufferWidth, FramebufferHeight);
  if (!Texture) {
    std::fprintf(stderr, "%s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_AudioSpec AudioSpecification{};
  AudioSpecification.freq = Tuning::Audio::SampleRate;
  AudioSpecification.format = AUDIO_S16SYS;
  AudioSpecification.channels = 1;
  AudioSpecification.samples = 512;
  AudioSpecification.callback = GenerateAudio;
  AudioDevice = SDL_OpenAudioDevice(nullptr, 0, &AudioSpecification, nullptr, 0);
  if (AudioDevice)
    SDL_PauseAudioDevice(AudioDevice, 0);
  bool Running = true, Diagnostics = false;
  uint64_t Last = SDL_GetPerformanceCounter();
  while (Running) {
    DrivingInput PlayerInput{};
    SDL_Event Event;
    while (SDL_PollEvent(&Event)) {
      if (Event.type == SDL_QUIT)
        Running = false;
      if (Event.type == SDL_WINDOWEVENT && Event.window.event == SDL_WINDOWEVENT_FOCUS_LOST &&
          (GameState.CurrentMode == GameMode::Racing ||
           GameState.CurrentMode == GameMode::Countdown))
        PlayerInput.Pause = true;
      if (Event.type == SDL_KEYDOWN && !Event.key.repeat) {
        if (Event.key.keysym.sym == SDLK_RETURN || Event.key.keysym.sym == SDLK_z)
          PlayerInput.Action = true;
        if (Event.key.keysym.sym == SDLK_ESCAPE)
          PlayerInput.Back = true;
        if (Event.key.keysym.sym == SDLK_p)
          PlayerInput.Pause = true;
#ifdef GRAVELBYTE_DIAGNOSTICS
        if (Event.key.keysym.sym == SDLK_F1)
          Diagnostics = !Diagnostics;
#endif
        if (Event.key.keysym.sym == SDLK_SPACE)
          PlayerInput.Auxiliary = true;
        if (Event.key.keysym.sym == SDLK_m)
          PlayerInput.Mute = true;
      }
    }
    const Uint8 *Keys = SDL_GetKeyboardState(nullptr);
    PlayerInput.Up = Keys[SDL_SCANCODE_UP];
    PlayerInput.Down = Keys[SDL_SCANCODE_DOWN];
    PlayerInput.Left = Keys[SDL_SCANCODE_LEFT];
    PlayerInput.Right = Keys[SDL_SCANCODE_RIGHT];
    PlayerInput.Throttle = Keys[SDL_SCANCODE_Z] || Keys[SDL_SCANCODE_UP];
    PlayerInput.Brake = Keys[SDL_SCANCODE_X] || Keys[SDL_SCANCODE_DOWN];
    PlayerInput.Handbrake = Keys[SDL_SCANCODE_SPACE];
    uint64_t Now = SDL_GetPerformanceCounter();
    float DeltaTimeSeconds = float(double(Now - Last) / double(SDL_GetPerformanceFrequency()));
    Last = Now;
    GameState.Update(DeltaTimeSeconds, PlayerInput);
    if (GameState.RandomRequested)
      GameState.SetChallengeSeed(Random());
    Save();
    if (AudioDevice) {
      SDL_LockAudioDevice(AudioDevice);
      SoundFrequency = GameState.EngineFrequency();
      SoundVolume = (!GameState.Muted && (GameState.CurrentMode == GameMode::Racing ||
                                          GameState.CurrentMode == GameMode::Title ||
                                          GameState.CurrentMode == GameMode::Finished))
                        ? (GameState.Impact > 0 ? .7f : .25f)
                        : 0;
      SDL_UnlockAudioDevice(AudioDevice);
    }
    SceneRenderer.Render(GameState, Framebuffer.data(),
                         DeltaTimeSeconds > 0 ? int(1 / DeltaTimeSeconds) : 0, Diagnostics);
    for (int Index = 0; Index < FramebufferWidth * FramebufferHeight; ++Index) {
      uint16_t ColorValue = Framebuffer[Index];
      RedGreenBlueAlpha[Index] = 0xff000000u | ((ColorValue >> 12) * 17u << 16) |
                                 (((ColorValue >> 8) & 15) * 17u << 8) |
                                 (((ColorValue >> 4) & 15) * 17u);
    }
    SDL_UpdateTexture(Texture, nullptr, RedGreenBlueAlpha.data(), FramebufferWidth * 4);
    SDL_SetRenderDrawColor(Screen, 12, 18, 18, 255);
    SDL_RenderClear(Screen);
    SDL_RenderCopy(Screen, Texture, nullptr, nullptr);
    SDL_RenderPresent(Screen);
  }
  if (AudioDevice)
    SDL_CloseAudioDevice(AudioDevice);
  SDL_DestroyTexture(Texture);
  SDL_DestroyRenderer(Screen);
  SDL_DestroyWindow(Window);
  SDL_Quit();
}
