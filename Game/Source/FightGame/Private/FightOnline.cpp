#include "FightOnline.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FightGame.h"
#include "FightUserSettings.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* GMap = TEXT("/Engine/Maps/Entry");
	constexpr int32 MaxPartySize = 4;
}

UFightOnline* UFightOnline::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UFightOnline>() : nullptr;
}

void UFightOnline::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UFightOnline::Tick), 0.1f);
}

void UFightOnline::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Party.Reset();
	Platform.Shutdown();
	Super::Deinitialize();
}

void UFightOnline::StartMatch(const FFightMatchRequest& InRequest)
{
	Request = InRequest;
	Request.bValid = true;
	UGameplayStatics::OpenLevel(this, GMap);
}

void UFightOnline::ReturnToMenu()
{
	Request = FFightMatchRequest();
	UGameplayStatics::OpenLevel(this, GMap);
}

void UFightOnline::EnsureLoggedIn()
{
	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	FFightEosPlatform::FConfig Config;
	Config.ProductId = S->EosProductId;
	Config.SandboxId = S->EosSandboxId;
	Config.DeploymentId = S->EosDeploymentId;
	Config.ClientId = S->EosClientId;
	Config.ClientSecret = S->EosClientSecret;
	if (const UFightUserSettings* User = UFightUserSettings::Get()) Config.DisplayName = User->DisplayName;
	Platform.Begin(Config); // no-op once started
}

bool UFightOnline::Tick(float DeltaSeconds)
{
	if (!Party) return true;
	Party->Tick(DeltaSeconds);
	if (!bPartySeen && Party->IsReady())
	{
		// whatever match the leader was in before we joined is none of our business
		bPartySeen = true;
		SeenMatch = Party->GetAttribute(TEXT("match"));
	}
	return true;
}

void UFightOnline::CreateParty()
{
	EnsureLoggedIn();
	FFightEosLobby::FConfig Config;
	Config.Join = FFightEosLobby::EJoin::Host;
	Config.Bucket = FString::Printf(TEXT("party:%u"), static_cast<uint32>(fc::net::kProtocolVersion));
	Config.MaxMembers = MaxPartySize;
	Config.bTransport = false;
	Party = MakeUnique<FFightEosLobby>(Platform);
	Party->Begin(Config);
	bPartySeen = false;
}

void UFightOnline::JoinParty(const FString& Code)
{
	EnsureLoggedIn();
	FFightEosLobby::FConfig Config;
	Config.Join = FFightEosLobby::EJoin::Join;
	Config.LobbyCode = Code.TrimStartAndEnd();
	Config.Bucket = FString::Printf(TEXT("party:%u"), static_cast<uint32>(fc::net::kProtocolVersion));
	Config.bTransport = false;
	Party = MakeUnique<FFightEosLobby>(Platform);
	Party->Begin(Config);
	bPartySeen = false;
}

void UFightOnline::LeaveParty()
{
	Party.Reset();
	bPartySeen = false;
	SeenMatch.Reset();
}

bool UFightOnline::IsPartyLeader() const
{
	return Party && Party->IsReady() && Party->IsHost();
}

int32 UFightOnline::GetPartySize() const
{
	return Party && Party->IsReady() ? FMath::Max(1, Party->GetMemberCount()) : 1;
}

FString UFightOnline::GetPartyCode() const
{
	return Party ? Party->GetLobbyId() : FString();
}

FString UFightOnline::GetPartyStatus() const
{
	if (!Party) return FString();
	if (!Party->IsReady()) return Party->GetStatus();
	return FString::Printf(TEXT("Party: %d / %d"), GetPartySize(), MaxPartySize);
}

void UFightOnline::AnnounceMatch(const FString& MatchCode, EFightMode Mode)
{
	if (!IsPartyLeader()) return;
	Party->SetAttribute(TEXT("mode"), FString::FromInt(static_cast<int32>(Mode)));
	Party->SetAttribute(TEXT("match"), MatchCode);
}

bool UFightOnline::TakePartyMatch(FString& OutCode, EFightMode& OutMode)
{
	if (!Party || !bPartySeen || Party->IsHost()) return false;
	const FString Match = Party->GetAttribute(TEXT("match"));
	if (Match.IsEmpty() || Match == SeenMatch) return false;
	SeenMatch = Match;
	OutCode = Match;
	OutMode = static_cast<EFightMode>(FCString::Atoi(*Party->GetAttribute(TEXT("mode"))));
	return true;
}
