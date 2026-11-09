#pragma once
#include "fightcore/Technique.h"

namespace fc::techniques {

const TechniqueDef& boundless();
enum BoundlessMove : int16_t { kRepel, kAttract, kFoldStep, kSingularity, kBoundlessMoveCount };

const TechniqueDef& calamityFlames();
enum CalamityFlamesMove : int16_t { kEmberMoths, kMagmaPillar, kHeatMantle, kFallingStar, kCalamityFlamesMoveCount };

} // namespace fc::techniques

namespace fc::techniques {

const TechniqueDef& shadowPuppets();
enum ShadowMove : int16_t { kCallHounds, kCallWing, kCallMaw, kFloodColossus, kShadowMoveCount };

} // namespace fc::techniques

namespace fc::techniques {

const TechniqueDef& graniteCannon();
enum GraniteMove : int16_t { kGraniteBeam, kShockwave, kChargeGuard, kFullOutput, kGraniteMoveCount };

const TechniqueDef& skyWeave();
enum SkyMove : int16_t { kSkyDrop, kSkyPull, kSkyWall, kSkyTear, kSkyMoveCount };

const TechniqueDef& thunderVessel();
enum ThunderMove : int16_t { kBolt, kJoltDash, kArcCounter, kAmberForm, kArcStrike, kThunderMoveCount };

// Heaven's Bind only
const TechniqueDef& toolArsenal();
enum ArsenalMove : int16_t { kCloudStaff, kEndlessChain, kReversalSpear, kToolStorm, kArsenalMoveCount };

const TechniqueDef& starMass();
enum StarMove : int16_t { kMassSlam, kWeightless, kStarWingCall, kBlackHole, kStarMoveCount };

const TechniqueDef& blossomDraw();
enum BlossomMove : int16_t { kDrawCut, kFallingPetal, kStepIn, kAbsoluteCounter, kIai, kZoneCut, kBlossomMoveCount };

const TechniqueDef& spiritHoard();
enum HoardMove : int16_t { kPrismSerpent, kRelease, kAbsorb, kSpiralCollapse, kHoardMoveCount };

const TechniqueDef& jackpot();
enum JackpotMove : int16_t { kBallRush, kShutterWall, kReelDash, kFeverMode, kJackpotMoveCount };

const TechniqueDef& strawEffigy();
enum EffigyMove : int16_t { kNail, kPinBurst, kDecoyDoll, kFullResonance, kResonance, kTotalResonance, kEffigyMoveCount };

const TechniqueDef& puppetPilot();
enum PuppetMove : int16_t { kCannon, kStrayDrone, kDashBlade, kOverdriveFrame, kPuppetMoveCount };

const TechniqueDef& lightMalice();
enum LightMove : int16_t { kLightBeam, kLightStep, kKillingIntent, kReversalBeam, kLightMoveCount };

} // namespace fc::techniques
