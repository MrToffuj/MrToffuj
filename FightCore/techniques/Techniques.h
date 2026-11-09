#pragma once
#include "fightcore/Technique.h"

namespace fc::techniques {

// Nullraum: Raum-/Distanz-Technik. Core Rule "Distanzfeld" (Auto-Block).
const TechniqueDef& nullraum();
enum NullraumMove : int16_t { kRepel, kAttract, kFoldStep, kSingularity, kNullraumMoveCount };

// Glutkern: Feuer-Technik. Core Rule "Hitze-Stacks" (Brennen). Entzündet Holz in der Welt.
const TechniqueDef& glutkern();
enum GlutkernMove : int16_t { kEmberMoths, kMagmaPillar, kHeatMantle, kStarfall, kGlutkernMoveCount };

} // namespace fc::techniques
