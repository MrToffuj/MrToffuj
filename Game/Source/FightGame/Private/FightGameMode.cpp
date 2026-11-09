#include "FightGameMode.h"

#include "FightDirector.h"
#include "FightHUD.h"
#include "Kismet/GameplayStatics.h"

AFightGameMode::AFightGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = AFightHUD::StaticClass();
}

void AFightGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (!UGameplayStatics::GetPlayerController(this, 1))
		UGameplayStatics::CreatePlayer(this, 1, true);
	GetWorld()->SpawnActor<AFightDirector>(AFightDirector::StaticClass(), FTransform::Identity);
}
