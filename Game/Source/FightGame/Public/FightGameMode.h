#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "FightGameMode.generated.h"

// Lokales 1v1: legt den zweiten lokalen Spieler an und startet den FightDirector.
// Es gibt keine Pawns – die Kämpfer leben in der deterministischen Simulation.
UCLASS()
class FIGHTGAME_API AFightGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFightGameMode();

	virtual void BeginPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override {}
};
