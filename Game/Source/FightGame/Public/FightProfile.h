#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "fightcore/Cosmetics.h"

#include "FightProfile.generated.h"

// Lifetime stats of the player on this device, used for outfit unlocks.
UCLASS()
class FIGHTGAME_API UFightProfile : public USaveGame
{
	GENERATED_BODY()

public:
	static UFightProfile* Load();
	void Save();

	void AddMatch(bool bWon, int32 MatchKOs, int32 MatchBlackSparks, int32 MatchRealms, const FString& TechniqueId);
	fc::PlayerRecord ToRecord() const;
	bool IsUnlocked(FName OutfitId) const;

	UFUNCTION(BlueprintCallable, Category = "Fight|Outfits")
	TArray<FName> GetUnlockedOutfits() const;

	UPROPERTY() int32 Wins = 0;
	UPROPERTY() int32 KOs = 0;
	UPROPERTY() int32 Matches = 0;
	UPROPERTY() int32 BlackSparks = 0;
	UPROPERTY() int32 RealmsOpened = 0;
	UPROPERTY() TMap<FString, int32> MatchesWithTechnique;
};
