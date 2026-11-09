#include "FightGameMode.h"

#include "FightDirector.h"
#include "FightGameSettings.h"
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
	const int32 Locals = FMath::Clamp(GetDefault<UFightGameSettings>()->LocalPlayers, 1, AFightDirector::MaxLocalPlayers);
	for (int32 i = 1; i < Locals; ++i)
		if (!UGameplayStatics::GetPlayerController(this, i))
			UGameplayStatics::CreatePlayer(this, i, true);
	GetWorld()->SpawnActor<AFightDirector>(AFightDirector::StaticClass(), FTransform::Identity);
}
