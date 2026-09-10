#pragma once
#include <cstddef>
namespace GravelByte::Tuning {
constexpr float HalfTurnRadians = 3.14159265f, FullTurnRadians = 6.2831853f;
constexpr float PhysicsStep = .01f, MaximumFrameDeltaSeconds = .25f, CountdownSeconds = 3.f;
constexpr float SteeringResponse = 7.f, CameraResponse = 4.f, Gravity = 16.f;
constexpr float Milliseconds = 1000.f, SecondsPerMicrosecond = .000001f;
constexpr int WorldScale = 64, BasisScale = 16384, BasisShift = 14;
constexpr int NearPlane = 42, FarPlane = 170 * WorldScale, FocalLength = 85;
constexpr int CenterX = 60, CenterY = 53, DepthNumerator = 2097152;
constexpr int VertexCacheSize = 512, FaceCapacity = 1800, ShadowCapacity = 64, RidgeSamples = 512;
constexpr int RoadBehind = 5, RoadAhead = 24;
constexpr int SnowRoadAhead = 20;
constexpr float ChaseDistance = 7.5f, ChaseHeight = 3.6f, ShowroomDistance = 5.f;
constexpr float ShowroomHeight = 2.5f, RoadsideHeight = 2.5f, RoadsideMinimumHeight = 3.5f;
constexpr float RoadsideOffset = 2.f, HighShotBack = 13.f, HighShotOffset = 5.f,
                HighShotHeight = 9.f;
constexpr float CarAimHeight = .8f;
constexpr int ShowroomCenterY = 43, RoadsideLookAhead = 4, PortalCameraMargin = 8,
              CameraShotCount = 3;
constexpr float ShotSeconds = 5.f, ShowcaseSeconds = 15.f;
constexpr std::size_t ReplayCapacity = 2048;
constexpr float ReplayInterval = .1f, PoseScale = 64.f, AngleScale = 32767.f / HalfTurnRadians;
constexpr int BridgeStart = 42, BridgeEnd = 52, TunnelStart = 115, TunnelEnd = 131;
constexpr int CoastStart = 0, CoastEnd = 300;
constexpr float PalmLean = .8f, PalmCrownRadius = 3.f, PalmTrunkWidth = .32f;
constexpr int MountainSections = 4;
constexpr float MountainWidth = 36.f, MountainPeak = 34.f;
constexpr float RailMargin = .8f, CarClearance = .95f, TunnelHeight = 6.5f;
constexpr float RiverDrop = 7.f, BeachDrop = 2.f;
constexpr int ChasePitchSine = 3213, ChasePitchCosine = 16066;
constexpr float SceneryDetailDistance = 72.f;
constexpr float SnowSceneryDetailDistance = 48.f;
constexpr int StatisticsTop = 84, StatisticsRowHeight = 8, StatisticBarCount = 5;
constexpr unsigned SaveMagic = 0x4752564c, SaveVersion = 2, MutedFlag = 0x100;
constexpr unsigned SelectionMask = 0xff, HashOffsetBasis = 2166136261u, HashPrime = 16777619u;
constexpr unsigned MaximumRecordMilliseconds = 86400000;
} // namespace GravelByte::Tuning

namespace GravelByte::Tuning::Physics {
constexpr float BrakeDeceleration = 22.f, ReverseAcceleration = 5.f, HandbrakeDrag = 5.f;
constexpr float HandbrakeGrip = 1.35f, GripFalloffSpeed = 18.f, GripFalloff = .03f;
constexpr float BrakingGrip = .9f, OffroadGrip = 3.8f, OffroadTraction = 5.5f;
constexpr float RoadDrag = .007f, OffroadDrag = .095f, RollingDrag = .08f, SlopeGravity = 9.f;
constexpr float ReverseSpeed = -4.f, SteeringFalloff = .035f, Wheelbase = 2.6f;
constexpr float HandbrakeTurn = 1.45f, AirborneTurn = .15f;
constexpr float JumpSpeed = 16.f, JumpRise = .6f, JumpSupportDrop = .35f, LandingImpact = .14f;
constexpr float PitchLimit = .25f, PitchResponse = 9.f, RollLean = .0008f;
constexpr float CameraJumpRise = .4f, TreeCollisionRadiusSquared = 2.f, TreeClearance = 1.5f;
constexpr float TreeBounce = -.12f, TreeImpact = .7f, StrandedDistance = 22.f;
constexpr float StrandedSpeed = 1.2f, RecoveryDelay = 2.5f, RecoveryDistance = 45.f;
constexpr float RecoveryPenalty = 3.f, MessageSeconds = 2.5f;
constexpr float OffroadStep = .12f, MaximumBankSlope = 1.5f, MaximumSupportSpeed = 12.f;
} // namespace GravelByte::Tuning::Physics
namespace GravelByte::Tuning::Audio {
constexpr unsigned UpdateIntervalMicroseconds = 50000, SampleRate = 22050;
constexpr float BaseFrequency = 65.f, SpeedFrequency = 8.f;
} // namespace GravelByte::Tuning::Audio
namespace GravelByte::Tuning::Telemetry {
constexpr unsigned MinimumFrameRateIntervalMicroseconds = 33334,
                   ReportIntervalMicroseconds = 2000000;
}
