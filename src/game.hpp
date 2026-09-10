#pragma once
#include "tuning.hpp"
#include <array>
#include <cstdint>

namespace GravelByte {
constexpr int FramebufferWidth = 120, FramebufferHeight = 120, NodeCount = 301;
constexpr float TrackSegmentLength = 6.0f, RoadHalfWidth = 4.1f;
constexpr int SectorCount = 5;
constexpr std::array<int, SectorCount> SectorEnds{60, 120, 180, 240, NodeCount - 4};
// Cumulative times from one braking-aware drive, with a 2.4% margin.
constexpr std::array<float, SectorCount> DefaultSplits{22.02f, 45.32f, 67.22f, 92.21f, 113.f};
constexpr uint32_t CourseVersion = 3;
struct Vector3 {
  float CoordinateX = 0, CoordinateY = 0, CoordinateZ = 0;
};
inline Vector3 operator+(Vector3 FirstVertex, Vector3 SecondVertex) {
  return {FirstVertex.CoordinateX + SecondVertex.CoordinateX,
          FirstVertex.CoordinateY + SecondVertex.CoordinateY,
          FirstVertex.CoordinateZ + SecondVertex.CoordinateZ};
}
inline Vector3 operator-(Vector3 FirstVertex, Vector3 SecondVertex) {
  return {FirstVertex.CoordinateX - SecondVertex.CoordinateX,
          FirstVertex.CoordinateY - SecondVertex.CoordinateY,
          FirstVertex.CoordinateZ - SecondVertex.CoordinateZ};
}
inline Vector3 operator*(Vector3 FirstVertex, float SecondValue) {
  return {FirstVertex.CoordinateX * SecondValue, FirstVertex.CoordinateY * SecondValue,
          FirstVertex.CoordinateZ * SecondValue};
}
float Clamp(float Value, float Minimum, float Maximum);
float AngleDelta(float TargetAngle, float CurrentAngle);
struct TrackNode {
  Vector3 Position;
  float Heading = 0;
  float Turn = 0;
  Vector3 Right{};
  float Bank = 0, VergeLeft = 0, VergeRight = 0;
  float HalfWidth = RoadHalfWidth, FarLeft = 100, FarRight = 100;
};
struct DrivingInput {
  bool Left = false, Right = false, Throttle = false, Brake = false, Handbrake = false,
       Action = false, Pause = false, Back = false, Auxiliary = false, Mute = false;
};
enum class GameMode { Title, CarSelect, TrackSelect, Countdown, Racing, Paused, Finished };
enum class Controls { Pico, Keyboard, Gamepad, Touch };
constexpr int CarCount = 3, TrackCount = 3;
struct CarSpecification {
  const char *Name;
  const char *Difficulty;
  float Acceleration, MaximumSpeed, Grip, Traction, Steering;
  float Width, Height, Length;
  int SpeedStatistic, AccelerationStatistic, DriftStatistic;
};
extern const std::array<CarSpecification, CarCount> Cars;
extern const std::array<const char *, TrackCount> TrackNames;
struct StageRecord {
  std::array<float, SectorCount> Splits{};
};

struct ReplayPose {
  uint16_t NodeIndex;
  int16_t CoordinateX, CoordinateY, CoordinateZ, Yaw;
  int8_t Pitch, Roll;
};
static_assert(sizeof(ReplayPose) == 12);
struct Game {
  std::array<TrackNode, NodeCount> Road{};
  std::array<std::array<Vector3, 10>, NodeCount> Terrain{};
  std::array<std::array<Vector3, 5>, Tuning::MountainSections + 1> Mountain{};
  Vector3 CarPosition{}, Velocity{};
  float Yaw = 0, CameraYaw = 0, Speed = 0, SteeringInput = 0, Lateral = 0, SegmentFraction = 0;
  float Elapsed = 0, Countdown = 3, Best = 0, PreviousBest = 0;
  float Impact = 0, Stranded = 0, RecoveryMessage = 0, Slip = 0;
  float GroundY = 0, VerticalSpeed = 0, Pitch = 0, Roll = 0, CameraHeight = 0;
  float SplitMessage = 0, SplitDelta = 0;
  std::array<float, SectorCount> Splits{}, BestSplits{}, ReferenceSplits = DefaultSplits;
  int SplitCount = 0, Jumps = 0;
  bool Airborne = false, SurfaceAvailable = true, StartHeld = true;
  int Segment = 0, Furthest = 0, Recoveries = 0;
  GameMode CurrentMode = GameMode::Title, ResumeMode = GameMode::Racing;
  bool NewRecord = false, SaveRequested = false;
  int SelectedCar = 1, SelectedTrack = 0;
  bool Muted = false, ShowRecords = false, DemoActive = false;
  int TitleCar = 1, TitleTrack = 0;
  float CinematicTime = 0, DemoTime = 0;
  std::array<float, SectorCount> PriorSplits{};
  std::array<ReplayPose, Tuning::ReplayCapacity> Replay{};
  int ReplayCount = 0;
  float ReplayInterval = Tuning::ReplayInterval, RecordClock = 0, ReplayDuration = 0;
  float ReplayTime = 0, NextSample = 0;
  void RecordPose(bool Final = false);
  void ReplayTick(float DeltaTimeSeconds);
  void DemoTick(float DeltaTimeSeconds);
  bool Unlocked(int Track) const;
  bool Bridge(int NodeIndex) const;
  bool Tunnel(int NodeIndex) const;
  bool Coast(int NodeIndex) const;
  bool HasScenery(int NodeIndex, int Sign) const;
  void ToggleAudio();
  Controls ControlScheme = Controls::Pico;
  float MenuRotation = 0;
  bool MenuLeft = false, MenuRight = false;
  std::array<StageRecord, CarCount * TrackCount> Records{};
  const CarSpecification &GetCarSpecification() const { return Cars[SelectedCar]; }
  void BuildTrack();
  void SelectCarAndTrack(int CarIndex, int TrackIndex);
  float SurfaceGrip(int NodeIndex) const;
  bool Icy(int NodeIndex) const;
  const std::array<float, SectorCount> &GetDefaultSplits() const;
  Game();
  void Restart();
  void Update(float DeltaTimeSeconds, const DrivingInput &PlayerInput);
  void SimulatePhysics(float DeltaTimeSeconds, const DrivingInput &PlayerInput);
  void Locate();
  bool SurfaceHeight(Vector3 Position, float &Height) const;
  void Recover();
  Vector3 Roadside(int Index, float Side) const;
  float TerrainHeight(int Index, float Side) const;
  float RoadWidth() const;
  Vector3 Scenery(int Index, int Sign) const;
  static int GetSectionIndex(int NodeIndex);
  const char *GetSectionName(int NodeIndex) const;
  void SelectReference();
  float Progress() const;
};
struct SaveData {
  uint32_t Magic = 0, Version = 0, CarSelectionAndFlags = 1, Track = 0;
  std::array<std::array<uint32_t, SectorCount>, CarCount * TrackCount> Times{};
  uint32_t Checksum = 0;
};
SaveData EncodeSave(const Game &GameState);
bool ValidSave(const SaveData &Save);
bool LoadSave(Game &GameState, const SaveData &Save);
struct SaveRecord {
  uint32_t Magic, Version, Milliseconds, Checksum;
  uint32_t Course = CourseVersion;
  std::array<uint32_t, SectorCount - 1> Splits{};
};
SaveRecord EncodeBest(float Seconds, const std::array<float, SectorCount> &Splits);
float DecodeBest(const SaveRecord &SavedRecord);
void LoadBest(Game &GameState, const SaveRecord &SavedRecord);

struct GeometryTelemetry {
  uint32_t Frames = 0, Dropped = 0, OverflowFrames = 0;
  void Observe(int FrameDropped) {
    ++Frames;
    if (FrameDropped > 0) {
      Dropped += uint32_t(FrameDropped);
      ++OverflowFrames;
    }
  }
};
struct Renderer {
  struct Triangle {
    int16_t CoordinateX[3], CoordinateY[3];
    uint16_t InverseDepth[3], SurfaceColor;
    uint16_t Shadow = 0;
  };
  struct CameraVertex {
    int32_t CoordinateX = 0, CoordinateY = 0, CoordinateZ = 0;
    int16_t ScreenX = 0, ScreenY = 0;
    uint16_t InverseDepth = 0;
  };
  struct CachedVertex {
    int32_t CoordinateX = 0, CoordinateY = 0, CoordinateZ = 0;
    uint32_t Frame = 0;
    CameraVertex Transformed{};
  };
  std::array<CachedVertex, Tuning::VertexCacheSize> VertexCache{};
  uint32_t RenderFrame = 0;
  int32_t CameraX = 0, CameraY = 0, CameraZ = 0, CameraSineFixed = 0,
          CameraCosineFixed = Tuning::BasisScale;
  int32_t PitchSineFixed = Tuning::ChasePitchSine, PitchCosineFixed = Tuning::ChasePitchCosine;
  int ProjectionY = Tuning::CenterY;
  struct ShadowPolygon {
    int16_t CoordinateX[8]{}, CoordinateY[8]{};
    int Count = 0, Left = 0, Right = 0, Top = 0, Bottom = 0;
  };
  std::array<ShadowPolygon, Tuning::ShadowCapacity> ShadowPolygons{};
  int ShadowCount = 0;
  std::array<Triangle, Tuning::FaceCapacity> Faces{};
  std::array<uint16_t, FramebufferWidth * FramebufferHeight> DepthBuffer{};
  std::array<uint8_t, Tuning::RidgeSamples> RidgeHeights{};
  uint32_t GeometryMicroseconds = 0, RasterMicroseconds = 0;
  Renderer();
  int FaceCount = 0, Dropped = 0;
  uint16_t *Pixels = nullptr;
  Vector3 Camera{};
  float CameraSine = 0, CameraCosine = 1;
  void Render(const Game &GameState, uint16_t *Target, int FramesPerSecond = 0,
              bool Diagnostics = false);
  float PrepareCamera(const Game &GameState);
  void PrepareShadow(const Game &GameState);
  void RenderBackground(const Game &GameState, float ViewYaw);
  void RenderMountain(const Game &GameState, int First, int Last);
  void RenderRoad(const Game &GameState, float ViewYaw);
  void RenderScenery(const Game &GameState, int NodeIndex);
  void RenderTrackObjects(const Game &GameState, int NodeIndex);
  void RenderCar(const Game &GameState);
  void RenderInterface(const Game &GameState, int FramesPerSecond, bool Diagnostics);
  Vector3 ShadowCenter{};
  float ShadowSine = 0, ShadowCosine = 1, ShadowWidth = 1.05f, ShadowLength = 1.8f;
  bool ShadowEnabled = false;
  float ShadowMinimumX = 0, ShadowMaximumX = 0, ShadowMinimumZ = 0, ShadowMaximumZ = 0;
  void DrawGroundTriangle(Vector3 FirstVertex, Vector3 SecondVertex, Vector3 ThirdVertex,
                          uint16_t SurfaceColor);
  void DrawGroundQuadrilateral(Vector3 FirstVertex, Vector3 SecondVertex, Vector3 ThirdVertex,
                               Vector3 FourthVertex, uint16_t SurfaceColor);
  void DrawTriangle(Vector3 FirstVertex, Vector3 SecondVertex, Vector3 ThirdVertex,
                    uint16_t SurfaceColor);
  CameraVertex Transform(Vector3 Vertex);
  void Project(CameraVertex &Vertex);
  void DrawQuadrilateral(Vector3 FirstVertex, Vector3 SecondVertex, Vector3 ThirdVertex,
                         Vector3 FourthVertex, uint16_t SurfaceColor);
  void DrawBox(Vector3 Position, Vector3 Size, float Yaw, uint16_t SurfaceColor);
  void DrawTree(Vector3 Position, float Height, int Seed);
  void DrawSnowTree(Vector3 Position, float Height, int Seed);
  void DrawPalmTree(Vector3 Position, float Height, int Seed);
  void DrawDistantSnowTree(Vector3 Position, float Height, int Seed, bool Snow = true);
  void RasterizeTriangle(const Triangle &DrawTriangle);
  void DrawRectangle(int CoordinateX, int CoordinateY, int Width, int Height,
                     uint16_t SurfaceColor);
  void DrawText(int CoordinateX, int CoordinateY, const char *Value, uint16_t SurfaceColor,
                int Scale = 1);
  void DrawCenteredText(int CoordinateY, const char *Value, uint16_t SurfaceColor, int Scale = 1);
};
uint16_t MakeColor(int RedComponent, int GreenComponent, int BlueComponent);
} // namespace GravelByte
