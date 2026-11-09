#pragma once

#include "CoreMinimal.h"
#include "FightGameSettings.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "FightAppearance.generated.h"

// Builds a fighter's look: base body (male or female) plus one skeletal mesh per outfit piece, all
// following the body's animation. Both bodies share one skeleton, so either cut of an item fits
// either body. Missing assets are skipped, the placeholder stays visible until the art exists.
UCLASS()
class FIGHTGAME_API UFightAppearance : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Fight|Outfits")
	static void Apply(AActor* Target, const FFightLoadout& Loadout);
};
