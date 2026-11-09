#pragma once
#include "fightcore/Types.h"

namespace fc::chars {

// Beispiel-Charakter / Vorlage. Eigenes IP.
// Kael – Technik "Schwerkraftknoten": setzt Knoten, die Gegner anziehen.
const CharacterDef& kael();

// Move-IDs, damit Tests / Präsentation darauf zugreifen können
enum KaelMove : int16_t { kJab, kHeavy, kLauncher, kAirLight, kKnot, kRepelShot, kRealm, kMoveCount };

} // namespace fc::chars
