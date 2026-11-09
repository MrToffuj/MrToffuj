#pragma once
#include "fightcore/Technique.h"

namespace fc::techniques {

// Boundless: Raum-/Distanz-Technik. Core Rule "Distanzfeld" (Auto-Block).
const TechniqueDef& boundless();
enum BoundlessMove : int16_t { kRepel, kAttract, kFoldStep, kSingularity, kBoundlessMoveCount };

// CalamityFlames: Feuer-Technik. Core Rule "Hitze-Stacks" (Brennen). Entzündet Holz in der Welt.
const TechniqueDef& calamityFlames();
enum CalamityFlamesMove : int16_t { kEmberMoths, kMagmaPillar, kHeatMantle, kFallingStar, kCalamityFlamesMoveCount };

} // namespace fc::techniques

namespace fc::techniques {

// ShadowPuppets: Beschwörer-Technik auf Basis des Begleiter-Systems.
const TechniqueDef& shadowPuppets();
enum ShadowMove : int16_t { kCallHounds, kCallWing, kCallMaw, kFloodColossus, kShadowMoveCount };

} // namespace fc::techniques
