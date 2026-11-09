#pragma once

#include "CoreMinimal.h"
#include "FightMenu.h"
#include "GameFramework/GameModeBase.h"

#include "FightGameMode.generated.h"

class AFightDirector;

// Two jobs, depending on what UFightOnline says:
// - no match requested: show the title menu
// - match requested: create the local players and spawn the director, then show the
//   lobby screen while it looks for players and the pause menu when someone presses Start.
// No pawns, the fighters live in the simulation.
UCLASS()
class FIGHTGAME_API AFightGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFightGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void RestartPlayer(AController* NewPlayer) override {}

private:
	void OpenMenu(FFightMenus::ERoot Root);
	void CloseMenu();

	TSharedPtr<FFightMenus> Menus;
	FFightMenus::ERoot MenuRoot = FFightMenus::ERoot::Title;
	TWeakObjectPtr<AFightDirector> Director;
};
