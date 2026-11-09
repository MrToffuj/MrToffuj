#include "FightProfile.h"

#include "Kismet/GameplayStatics.h"
#include "fightcore/Registry.h"

namespace
{
	const TCHAR* SlotName = TEXT("Profile");
}

UFightProfile* UFightProfile::Load()
{
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
		if (UFightProfile* Loaded = Cast<UFightProfile>(UGameplayStatics::LoadGameFromSlot(SlotName, 0))) return Loaded;
	return Cast<UFightProfile>(UGameplayStatics::CreateSaveGameObject(UFightProfile::StaticClass()));
}

void UFightProfile::Save()
{
	UGameplayStatics::SaveGameToSlot(this, SlotName, 0);
}

void UFightProfile::AddMatch(bool bWon, int32 MatchKOs, int32 MatchBlackSparks, int32 MatchRealms, const FString& TechniqueId)
{
	++Matches;
	Wins += bWon ? 1 : 0;
	KOs += MatchKOs;
	BlackSparks += MatchBlackSparks;
	RealmsOpened += MatchRealms;
	if (!TechniqueId.IsEmpty()) ++MatchesWithTechnique.FindOrAdd(TechniqueId);
}

fc::PlayerRecord UFightProfile::ToRecord() const
{
	fc::PlayerRecord R;
	R.wins = Wins;
	R.kos = KOs;
	R.matches = Matches;
	R.blackSparks = BlackSparks;
	R.realmsOpened = RealmsOpened;
	for (const TPair<FString, int32>& It : MatchesWithTechnique) R.matchesWithTechnique[TCHAR_TO_UTF8(*It.Key)] = It.Value;
	return R;
}

bool UFightProfile::IsUnlocked(FName OutfitId) const
{
	const fc::OutfitDef* Item = fc::Registry::instance().outfit(TCHAR_TO_UTF8(*OutfitId.ToString()));
	return Item && fc::isUnlocked(*Item, ToRecord());
}

TArray<FName> UFightProfile::GetUnlockedOutfits() const
{
	TArray<FName> Out;
	const fc::PlayerRecord R = ToRecord();
	for (const fc::OutfitDef* Item : fc::Registry::instance().outfits())
		if (fc::isUnlocked(*Item, R)) Out.Add(FName(UTF8_TO_TCHAR(Item->id)));
	return Out;
}
