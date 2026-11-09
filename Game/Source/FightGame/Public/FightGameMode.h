#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "FightGameMode.generated.h"

// Creates the local players and spawns the director. No pawns, the fighters live in the simulation.
UCLASS()
class FIGHTGAME_API AFightGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFightGameMode();

	virtual void BeginPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override {}
};
