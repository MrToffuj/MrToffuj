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
