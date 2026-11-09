#include "FightGameMode.h"

#include "FightDirector.h"
#include "FightGameSettings.h"
#include "FightHUD.h"
#include "FightOnline.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// what "skip the menu" plays: the match described in the project settings
	FFightMatchRequest RequestFromSettings()
	{
		const UFightGameSettings* S = GetDefault<UFightGameSettings>();
		FFightMatchRequest R;
		R.bValid = true;
		R.Mode = S->Mode;
		R.Custom = S->Custom;
		R.LocalPlayers = S->LocalPlayers;
		R.Loadouts = S->Loadouts;
		R.Net = S->NetMode;
		R.JoinCode = S->Backend == EFightNetBackend::Eos ? S->LobbyCode : S->HostAddress;
		return R;
	}
}

AFightGameMode::AFightGameMode()
{
	DefaultPawnClass = nullptr;
	HUDClass = AFightHUD::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void AFightGameMode::BeginPlay()
{
	Super::BeginPlay();
	UFightOnline* Online = UFightOnline::Get(this);
	if (Online && !Online->IsInMatch() && GetDefault<UFightGameSettings>()->bSkipMenu)
		Online->SetRequestWithoutTravel(RequestFromSettings());

	if (!Online || !Online->IsInMatch())
	{
		// back on the title screen: splitscreen players from the last match go away
		for (int32 i = AFightDirector::MaxLocalPlayers - 1; i > 0; --i)
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, i)) UGameplayStatics::RemovePlayer(PC, true);
		OpenMenu(FFightMenus::ERoot::Title);
		return;
	}

	const int32 Locals = FMath::Clamp(Online->GetRequest().LocalPlayers, 1, AFightDirector::MaxLocalPlayers);
	for (int32 i = 1; i < Locals; ++i)
		if (!UGameplayStatics::GetPlayerController(this, i))
			UGameplayStatics::CreatePlayer(this, i, true);

	// The menu switched the viewport to UI-only input, and that setting lives on the viewport,
	// not the level, so it survives the reload. Hand input back to the game explicitly.
	for (int32 i = 0; i < Locals; ++i)
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, i))
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->SetShowMouseCursor(false);
		}
	Director = GetWorld()->SpawnActor<AFightDirector>(AFightDirector::StaticClass(), FTransform::Identity);
}

void AFightGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	CloseMenu();
	Super::EndPlay(Reason);
}

void AFightGameMode::OpenMenu(FFightMenus::ERoot Root)
{
	CloseMenu();
	MenuRoot = Root;
	Menus = MakeShared<FFightMenus>(GetWorld(), Root);
	Menus->SetDirector(Director.Get());
	Menus->Open();
}

void AFightGameMode::CloseMenu()
{
	if (Menus) Menus->Close();
	Menus.Reset();
}

void AFightGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Menus) Menus->Tick(DeltaSeconds);

	AFightDirector* D = Director.Get();
	if (!D) return;

	// looking for players: the lobby screen stays up until the match starts
	if (D->IsInLobby())
	{
		if (!Menus || MenuRoot != FFightMenus::ERoot::Lobby) OpenMenu(FFightMenus::ERoot::Lobby);
		return;
	}
	if (Menus && MenuRoot == FFightMenus::ERoot::Lobby) CloseMenu();

	// pause: Start / Esc opens it, the menu closes itself (Resume, B)
	if (Menus && MenuRoot == FFightMenus::ERoot::Pause && !Menus->IsOpen())
	{
		Menus.Reset();
		D->SetMenuOpen(false);
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!Menus && PC && (PC->WasInputKeyJustPressed(EKeys::Gamepad_Special_Right) || PC->WasInputKeyJustPressed(EKeys::Escape)))
	{
		OpenMenu(FFightMenus::ERoot::Pause);
		D->SetMenuOpen(true);
	}
}
