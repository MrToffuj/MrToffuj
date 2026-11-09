#include "FightTypes.h"

static_assert(static_cast<int>(EFightImpactKind::Respawn) == static_cast<int>(fc::ImpactKind::Respawn),
	"EFightImpactKind muss fc::ImpactKind spiegeln");

FFightImpact FightUnits::ToImpact(const fc::ImpactEvent& E)
{
	FFightImpact Out;
	Out.Frame = E.frame;
	Out.Kind = static_cast<EFightImpactKind>(E.kind);
	Out.Source = E.source;
	Out.Strength = static_cast<int32>(E.strength);
	Out.Tag = E.tag;
	Out.Location = ToWorld(E.pos);
	Out.Direction = ToWorldDir(E.dir);
	Out.Force = E.force.toFloat();
	return Out;
}
