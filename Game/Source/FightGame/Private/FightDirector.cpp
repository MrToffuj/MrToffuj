#include "FightDirector.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "DrawDebugHelpers.h"
#include "FightGame.h"
#include "FightGameSettings.h"
#include "FightAppearance.h"
#include "FightProfile.h"
#include "FightShapeVisual.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "fightcore/Registry.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Techniques.h"

namespace
{
	const fc::BodyDef* BodyOf(EFightBody B)
	{
		switch (B)
		{
		case EFightBody::Heavy: return &fc::bodies::heavy();
		case EFightBody::Speedster: return &fc::bodies::speedster();
		case EFightBody::Caster: return &fc::bodies::caster();
		case EFightBody::Technician: return &fc::bodies::technician();
		case EFightBody::Summoner: return &fc::bodies::summoner();
		case EFightBody::HeavensBind: return &fc::bodies::heavensBind();
		default: return &fc::bodies::brawler();
		}
	}

	const fc::TechniqueDef* TechniqueOf(EFightTechnique T)
	{
		switch (T)
		{
		case EFightTechnique::Boundless: return &fc::techniques::boundless();
		case EFightTechnique::CalamityFlames: return &fc::techniques::calamityFlames();
		case EFightTechnique::ShadowPuppets: return &fc::techniques::shadowPuppets();
		case EFightTechnique::GraniteCannon: return &fc::techniques::graniteCannon();
		case EFightTechnique::SkyWeave: return &fc::techniques::skyWeave();
		case EFightTechnique::ThunderVessel: return &fc::techniques::thunderVessel();
		case EFightTechnique::StarMass: return &fc::techniques::starMass();
		case EFightTechnique::BlossomDraw: return &fc::techniques::blossomDraw();
		case EFightTechnique::SpiritHoard: return &fc::techniques::spiritHoard();
		case EFightTechnique::Jackpot: return &fc::techniques::jackpot();
		case EFightTechnique::StrawEffigy: return &fc::techniques::strawEffigy();
		case EFightTechnique::PuppetPilot: return &fc::techniques::puppetPilot();
		case EFightTechnique::LightMalice: return &fc::techniques::lightMalice();
		default: return nullptr;
		}
	}

	const fc::CompanionDef* AssistOf(EFightAssist A)
	{
		switch (A)
		{
		case EFightAssist::SparkOwl: return &fc::companions::sparkOwl();
		case EFightAssist::VineToad: return &fc::companions::vineToad();
		case EFightAssist::StoneWarden: return &fc::companions::stoneWarden();
		default: return &fc::companions::bladeWolf();
		}
	}

	const fc::MapDef* MapOf(EFightMap M)
	{
		switch (M)
		{
		case EFightMap::ShrineWard: return &fc::maps::shrineWard();
		case EFightMap::HarborDistrict: return &fc::maps::harborDistrict();
		case EFightMap::TempleSteps: return &fc::maps::templeSteps();
		case EFightMap::ShatteredDowntown: return &fc::maps::shatteredDowntown();
		case EFightMap::FrozenLake: return &fc::maps::frozenLake();
		default: return &fc::maps::oldCapital();
		}
	}

	fc::GameMode ToMode(EFightMode M)
	{
		switch (M)
		{
		case EFightMode::Teams: return fc::GameMode::Teams;
		case EFightMode::Brawl: return fc::GameMode::Brawl;
		case EFightMode::Custom: return fc::GameMode::Custom;
		default: return fc::GameMode::Duel;
		}
	}

	std::string ToStd(const FName& Name) { return std::string(TCHAR_TO_UTF8(*Name.ToString())); }
	std::string ToStd(const FString& Text) { return std::string(TCHAR_TO_UTF8(*Text)); }

	fc::ModeRules ToRules(const UFightGameSettings& S)
	{
		const fc::GameMode Mode = ToMode(S.Mode);
		fc::ModeRules R = fc::rulesFor(Mode);
		if (Mode != fc::GameMode::Custom) return R;

		const FFightCustomRules& C = S.Custom;
		R.fighters = static_cast<int8_t>(FMath::Clamp(C.Fighters, 2, fc::kMaxFighters));
		R.teamCount = static_cast<uint8_t>(FMath::Clamp(C.Teams, 0, static_cast<int32>(R.fighters)));
		R.objective = static_cast<fc::Objective>(C.Objective);
		R.map = MapOf(C.Map);
		R.friendlyFire = C.bFriendlyFire;
		R.scoreLimit = static_cast<int16_t>(C.ScoreLimit);
		R.timeLimitFrames = C.TimeLimitMinutes > 0 ? C.TimeLimitMinutes * 60 * 60 : -1;
		R.respawnFrames = R.objective == fc::Objective::Elimination ? -1 : static_cast<int16_t>(FMath::Max(1, C.RespawnSeconds) * 60);
		R.cursedObjectSlots = static_cast<uint8_t>(FMath::Clamp(C.CursedObjectSlots, 0, fc::kMaxCursedObjects));
		if (!C.ModMap.IsNone())
			if (const fc::MapDef* Modded = fc::Registry::instance().map(ToStd(C.ModMap))) R.map = Modded;
		return R;
	}

	void FillLoadout(fc::Loadout& Out, const FFightLoadout& In)
	{
		Out.body = BodyOf(In.Body);
		// Heaven's Bind has no technique slot, it always fights with the tool arsenal
		Out.technique = In.Body == EFightBody::HeavensBind ? &fc::techniques::toolArsenal() : TechniqueOf(In.Technique);
		Out.assist = AssistOf(In.Assist);
		Out.team = static_cast<uint8_t>(FMath::Clamp(In.Team, 0, fc::kMaxTeams - 1));
		Out.lockOnAtStart = false; // players lock on themselves with L3
		const fc::Registry& Reg = fc::Registry::instance();
		if (!In.ModTechnique.IsNone() && In.Body != EFightBody::HeavensBind)
			if (const fc::TechniqueDef* Modded = Reg.technique(ToStd(In.ModTechnique))) Out.technique = Modded;
		for (int32 k = 0; k < FMath::Min(In.CursedObjects.Num(), fc::kMaxCursedObjects); ++k)
			Out.cursed[k] = Reg.cursedObject(ToStd(In.CursedObjects[k]));
	}

	// What a console asks the host for. The host turns it back into a loadout the same way
	// on every peer, so only names travel, never pointers.
	fc::net::PlayerRequest ToRequest(const FFightLoadout& In)
	{
		fc::Loadout L;
		FillLoadout(L, In);
		fc::net::PlayerRequest R;
		R.body = L.body->name;
		R.technique = L.technique ? L.technique->id : "";
		R.assist = L.assist ? L.assist->name : "";
		R.team = L.team;
		R.oath = L.oath;
		R.profile = L.profile;
		for (const fc::CursedObjectDef* C : L.cursed)
			if (C) R.cursed.emplace_back(C->id);
		return R;
	}

	FLinearColor CompanionColor(fc::CompanionType T)
	{
		switch (T)
		{
		case fc::CompanionType::Zoner: return FLinearColor(0.9f, 0.85f, 0.2f);
		case fc::CompanionType::Support: return FLinearColor(0.3f, 0.85f, 0.35f);
		case fc::CompanionType::Guard: return FLinearColor(0.55f, 0.55f, 0.6f);
		default: return FLinearColor(0.85f, 0.25f, 0.6f);
		}
	}

	fc::Vec3 LocalToWorld(const fc::FighterState& F, fc::Vec3 Off)
	{
		const fc::Vec3 Right{ F.facing.z, fc::Fx{}, -F.facing.x };
		return F.pos + Right * Off.x + fc::Vec3{ fc::Fx{}, Off.y, fc::Fx{} } + F.facing * Off.z;
	}

	constexpr int32 BarrierSegments = 72;
	constexpr float BarrierHeightCm = 30000.f;
}

AFightDirector::AFightDirector()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetMobility(EComponentMobility::Movable);
	KeyLight->SetRelativeRotation(FRotator(-50.f, 30.f, 0.f));
	KeyLight->SetIntensity(4.f);

	FillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetRelativeRotation(FRotator(-20.f, 210.f, 0.f));
	FillLight->SetIntensity(1.5f);
	FillLight->SetCastShadows(false);
}

void AFightDirector::BeginPlay()
{
	Super::BeginPlay();
	NetMode = GetDefault<UFightGameSettings>()->NetMode;
	if (IsOnline())
	{
		StartOnline(); // the arena shows up once the lobby is done
		return;
	}
	BuildSession();
	StartPresentation();
}

void AFightDirector::StartPresentation()
{
	SpawnArena();
	SpawnFighterVisuals();
	ApplyAppearances();
}

void AFightDirector::LoadMods()
{
	static bool bLoaded = false;
	if (bLoaded) return;
	bLoaded = true;

	const FString Root = FPaths::Combine(FPaths::ProjectDir(), GetDefault<UFightGameSettings>()->ModsFolder);
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("mod.json"), true, false);
	Files.Sort(); // same order everywhere, the content hash depends on it
	for (const FString& File : Files)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *File)) continue;
		const FTCHARToUTF8 Utf8(*Text);
		std::string Error;
		if (fc::Registry::instance().loadMod(std::string_view(Utf8.Get(), Utf8.Length()), &Error))
			UE_LOG(LogFight, Log, TEXT("Loaded mod %s"), *File);
		else
			UE_LOG(LogFight, Warning, TEXT("Skipped mod %s: %s"), *File, UTF8_TO_TCHAR(Error.c_str()));
	}
}

void AFightDirector::BuildSession()
{
	LoadMods();
	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	const fc::GameMode Mode = ToMode(S->Mode);
	const fc::ModeRules Rules = ToRules(*S);
	LocalPlayers = FMath::Clamp(S->LocalPlayers, 1, FMath::Min<int32>(MaxLocalPlayers, Rules.fighters));
	for (int32 i = 0; i < MaxLocalPlayers; ++i) LocalFighters[i] = i;

	fc::Loadout Humans[MaxLocalPlayers];
	for (int32 i = 0; i < LocalPlayers; ++i)
		FillLoadout(Humans[i], S->Loadouts.IsValidIndex(i) ? S->Loadouts[i] : FFightLoadout());

	const fc::MatchSetup Setup = fc::buildMatch(Mode, Rules, Humans, LocalPlayers, static_cast<uint8_t>(S->BotLevel),
		static_cast<uint32_t>(FMath::Rand()));
	Session = MakeUnique<fc::Session>(Setup);
	bHasPrevious = false;
	RoundOverFrames = 0;
	bResultRecorded = false;
	FMemory::Memzero(BlackSparks);
	FMemory::Memzero(RealmsOpened);
	Accumulator = 0.0;
	UE_LOG(LogFight, Log, TEXT("Match on %s: %d fighters, %d local"), UTF8_TO_TCHAR(fc::mapOf(Setup).name), Setup.count, LocalPlayers);
}

void AFightDirector::RestartMatch()
{
	if (Peer)
	{
		Rematch();
		return;
	}
	if (IsOnline()) return; // still in the lobby
	BuildSession();
	ApplyAppearances();
}

void AFightDirector::ApplyAppearances()
{
	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	// outfits are cosmetic and stay on this console, remote players show their default look
	for (int32 i = 0; i < LocalPlayers; ++i)
		if (FighterVisuals.IsValidIndex(LocalFighters[i]) && S->Loadouts.IsValidIndex(i))
			UFightAppearance::Apply(FighterVisuals[LocalFighters[i]], S->Loadouts[i]);
}

void AFightDirector::RecordMatchResult(const fc::MatchState& State)
{
	const fc::MatchSetup& Setup = GetSetup();
	UFightProfile* Profile = UFightProfile::Load();
	for (int32 i = 0; i < LocalPlayers; ++i)
	{
		const fc::FighterState& F = State.f[LocalFighters[i]];
		const bool bWon = State.winner == F.team;
		const fc::TechniqueDef* Tech = Setup.loadout[LocalFighters[i]].technique;
		Profile->AddMatch(bWon, F.kos, BlackSparks[i], RealmsOpened[i], Tech ? FString(UTF8_TO_TCHAR(Tech->id)) : FString());
	}
	Profile->Save();
}

AFightShapeVisual* AFightDirector::SpawnShape(int32 Shape, const FLinearColor& Color, const FVector& SizeCm, const FVector& Offset)
{
	FActorSpawnParameters Params;
	Params.Owner = this;
	AFightShapeVisual* V = GetWorld()->SpawnActor<AFightShapeVisual>(AFightShapeVisual::StaticClass(), FTransform::Identity, Params);
	V->SetShape(static_cast<EFightShape>(Shape));
	V->SetColor(Color);
	V->SetSizeCm(SizeCm);
	V->SetMeshOffset(Offset);
	return V;
}

void AFightDirector::SpawnArena()
{
	const fc::MapDef& Map = fc::mapOf(GetSetup());
	const float Radius = FightUnits::ToCm(Map.radius);

	FloorVisual = SpawnShape(static_cast<int32>(EFightShape::Cylinder), FLinearColor(0.2f, 0.22f, 0.2f),
		FVector(2.f * Radius + 400.f, 2.f * Radius + 400.f, 20.f), FVector::ZeroVector);
	FloorVisual->SetActorLocation(FVector(0.f, 0.f, -10.f));

	// The colony barrier: a black curtain all the way around and up into the sky.
	// Placeholder geometry until the proper barrier material exists.
	const float SegmentWidth = 2.f * UE_PI * Radius / BarrierSegments * 1.04f;
	for (int32 k = 0; k < BarrierSegments; ++k)
	{
		const float Angle = 2.f * UE_PI * k / BarrierSegments;
		AFightShapeVisual* Segment = SpawnShape(static_cast<int32>(EFightShape::Cube), FLinearColor::Black,
			FVector(100.f, SegmentWidth, BarrierHeightCm), FVector(0.f, 0.f, BarrierHeightCm * 0.5f));
		const FVector Dir(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		Segment->SetActorLocation(Dir * (Radius + 50.f));
		Segment->SetActorRotation(Dir.Rotation());
		BarrierVisuals.Add(Segment);
	}

	for (const fc::ObstacleDef& O : Map.obstacles)
	{
		const FVector Size(FightUnits::ToCm(O.halfZ) * 2.f, FightUnits::ToCm(O.halfX) * 2.f, FightUnits::ToCm(O.height));
		AFightShapeVisual* V = SpawnShape(static_cast<int32>(EFightShape::Cube), FLinearColor(0.35f, 0.3f, 0.27f), Size,
			FVector(0.f, 0.f, Size.Z * 0.5f));
		V->SetActorLocation(FightUnits::ToWorld(O.center));
		ObstacleVisuals.Add(V);
	}

	for (int32 t = 0; t < fc::kMaxFlags; ++t)
	{
		AFightShapeVisual* Flag = SpawnShape(static_cast<int32>(EFightShape::Cylinder), GetTeamColor(t), FVector(15.f, 15.f, 250.f),
			FVector(0.f, 0.f, 125.f));
		Flag->AddMarker(EFightShape::Cube, FVector(0.f, 40.f, 220.f), FVector(5.f, 70.f, 45.f), GetTeamColor(t));
		Flag->SetVisible(false);
		FlagVisuals.Add(Flag);
	}
	for (const fc::Vec3& Z : Map.zones)
	{
		AFightShapeVisual* Zone = SpawnShape(static_cast<int32>(EFightShape::Cylinder), FLinearColor(0.5f, 0.5f, 0.5f),
			FVector(600.f, 600.f, 4.f), FVector::ZeroVector);
		Zone->SetActorLocation(FightUnits::ToWorld(Z) + FVector(0.f, 0.f, 2.f));
		Zone->SetVisible(false);
		ZoneVisuals.Add(Zone);
	}
	BallVisual = SpawnShape(static_cast<int32>(EFightShape::Sphere), FLinearColor(1.f, 0.85f, 0.3f), FVector(60.f), FVector::ZeroVector);
	BallVisual->SetVisible(false);

	FActorSpawnParameters Params;
	Params.Owner = this;
	const float Fov = GetDefault<UFightGameSettings>()->CameraFOV;
	for (int32 i = 0; i < LocalPlayers; ++i)
	{
		ACameraActor* Cam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
		UCameraComponent* CamComp = Cam->GetCameraComponent();
		CamComp->SetFieldOfView(Fov);
		CamComp->SetConstraintAspectRatio(false);
		// strip everything that makes a frame look like a 3D render
		FPostProcessSettings& PP = CamComp->PostProcessSettings;
		PP.bOverride_MotionBlurAmount = true;          PP.MotionBlurAmount = 0.f;
		PP.bOverride_AutoExposureMinBrightness = true; PP.AutoExposureMinBrightness = 1.f;
		PP.bOverride_AutoExposureMaxBrightness = true; PP.AutoExposureMaxBrightness = 1.f;
		PP.bOverride_AmbientOcclusionIntensity = true; PP.AmbientOcclusionIntensity = 0.f;
		PP.bOverride_VignetteIntensity = true;         PP.VignetteIntensity = 0.f;
		PP.bOverride_SceneFringeIntensity = true;      PP.SceneFringeIntensity = 0.f;
		PP.bOverride_BloomIntensity = true;            PP.BloomIntensity = 0.15f;
		CamComp->PostProcessBlendWeight = 1.f;
		Cameras.Add(Cam);
	}
}

void AFightDirector::SpawnFighterVisuals()
{
	const int32 Count = GetSetup().count;
	for (int32 i = 0; i < Count; ++i)
	{
		AFightShapeVisual* F = SpawnShape(static_cast<int32>(EFightShape::Cylinder), GetFighterColor(i), FVector(90.f, 90.f, 180.f),
			FVector(0.f, 0.f, 90.f));
		F->AddMarker(EFightShape::Cube, FVector(45.f, 0.f, 150.f), FVector(25.f), FLinearColor(0.05f, 0.05f, 0.05f));
		FighterVisuals.Add(F);
	}
	for (int32 i = 0; i < Count * fc::kCompanionSlots; ++i)
	{
		AFightShapeVisual* C = SpawnShape(static_cast<int32>(EFightShape::Sphere), FLinearColor::White, FVector(100.f), FVector::ZeroVector);
		C->SetVisible(false);
		CompanionVisuals.Add(C);
	}
	for (int32 i = 0; i < fc::kMaxProjectiles; ++i)
	{
		AFightShapeVisual* P = SpawnShape(static_cast<int32>(EFightShape::Sphere), FLinearColor::White, FVector(50.f), FVector::ZeroVector);
		P->SetVisible(false);
		ProjectileVisuals.Add(P);
	}
}

void AFightDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (LobbyHost || LobbyClient || (Eos && !Peer)) PollLobby(DeltaSeconds);
	if (!HasSession()) return;

	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	APlayerController* PC0 = UGameplayStatics::GetPlayerController(this, 0);
	static const FFightBindings NoKeys;
	for (int32 i = 0; i < LocalPlayers; ++i)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, i);
		if (PC && Cameras.IsValidIndex(i) && PC->GetViewTarget() != Cameras[i]) PC->SetViewTarget(Cameras[i]);
		// Keyboard events always arrive on the first controller, only P1 and P2 have a keyboard layout
		const FFightBindings& Keys = i == 0 ? S->KeyboardP1 : (i == 1 ? S->KeyboardP2 : NoKeys);
		Samplers[i].Sample(PC, PC0, S->Gamepad, Keys, i == 0);
	}

	Accumulator += FMath::Min(static_cast<double>(DeltaSeconds), 0.25);
	while (Accumulator >= FixedStep)
	{
		SimulateFrame();
		Accumulator -= FixedStep;
	}
	Present(static_cast<float>(Accumulator / FixedStep), DeltaSeconds);
}

void AFightDirector::SimulateFrame()
{
	if (Peer)
	{
		SimulateOnlineFrame();
		return;
	}
	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	Previous = Session->state();
	bHasPrevious = true;

	const int32 Frame = Session->frame();
	for (int32 p = 0; p < LocalPlayers; ++p)
		Session->setInput(p, Frame, Samplers[p].Build(S->StickDeadzone, CameraState[p].Yaw));

	Events.clear();
	Session->advance(&Events);
	for (const fc::ImpactEvent& E : Events) HandleEvent(E);

	if (Session->state().winner != fc::kNoWinner)
	{
		if (!bResultRecorded)
		{
			bResultRecorded = true;
			RecordMatchResult(Session->state());
		}
		if (++RoundOverFrames >= S->RestartDelayFrames) RestartMatch();
	}
}

int32 AFightDirector::GetPlayerIndex(const APlayerController* PC) const
{
	for (int32 i = 0; i < LocalPlayers; ++i)
		if (PC && PC == UGameplayStatics::GetPlayerController(this, i)) return i;
	return -1;
}

int32 AFightDirector::GetPlayerFighter(const APlayerController* PC) const
{
	const int32 Local = GetPlayerIndex(PC);
	return Local >= 0 ? LocalFighters[Local] : -1;
}

int32 AFightDirector::FindLocalIndex(int32 Fighter) const
{
	for (int32 i = 0; i < LocalPlayers; ++i)
		if (LocalFighters[i] == Fighter) return i;
	return -1;
}

FString AFightDirector::GetFighterLabel(int32 Fighter) const
{
	return GetSetup().loadout[Fighter].bot ? FString::Printf(TEXT("BOT %d"), Fighter + 1) : FString::Printf(TEXT("P%d"), Fighter + 1);
}

FLinearColor AFightDirector::GetTeamColor(int32 Team) const
{
	static const FLinearColor Fixed[4] = {
		FLinearColor(0.95f, 0.35f, 0.15f), FLinearColor(0.15f, 0.45f, 0.95f),
		FLinearColor(0.3f, 0.85f, 0.3f), FLinearColor(0.9f, 0.8f, 0.15f) };
	if (Team < 4) return Fixed[Team];
	return FLinearColor::MakeFromHSV8(static_cast<uint8>((Team * 47) % 256), 200, 230);
}

FLinearColor AFightDirector::GetFighterColor(int32 Fighter) const
{
	return GetTeamColor(GetSetup().loadout[Fighter].team);
}

void AFightDirector::HandleEvent(const fc::ImpactEvent& E)
{
	const FFightImpact Impact = FightUnits::ToImpact(E);
	OnImpact.Broadcast(Impact);
	const int32 Local = FindLocalIndex(E.source);
	if (Local >= 0)
	{
		if (E.kind == fc::ImpactKind::BlackSpark) ++BlackSparks[Local];
		if (E.kind == fc::ImpactKind::RealmOpen) ++RealmsOpened[Local];
	}

	// Only shake the camera for things that happen close to a local player
	bool bNearLocal = false;
	for (int32 i = 0; i < LocalPlayers && !bNearLocal; ++i)
		bNearLocal = FVector::DistSquared(Impact.Location, FightUnits::ToWorld(GetState().f[LocalFighters[i]].pos)) < FMath::Square(1500.f);
	if (!bNearLocal) return;

	using K = fc::ImpactKind;
	float Shake = 0.f;
	switch (E.kind)
	{
	case K::Hit: case K::GroundSlam: case K::WallSplat: case K::Block: case K::BarrierHit:
		Shake = 2.f + 3.f * Impact.Strength;
		break;
	case K::WallBreak: case K::BlackSpark: case K::RealmOpen: case K::RealmClash:
		Shake = 14.f;
		ImpactFlash = 1.f;
		break;
	default:
		break;
	}
	if (E.kind == K::Hit && E.strength == fc::ImpactClass::Crushing) ImpactFlash = 1.f;
	if (Shake > ShakeStrength || ShakeTime <= 0.f)
	{
		ShakeStrength = Shake;
		ShakeTime = Shake > 0.f ? 0.08f + Shake * 0.012f : 0.f;
	}
}

void AFightDirector::Present(float Alpha, float DeltaSeconds)
{
	const fc::MatchState& State = GetState();
	const fc::MatchSetup& Setup = GetSetup();

	for (int32 i = 0; i < State.count; ++i)
	{
		const fc::FighterState& Cur = State.f[i];
		AFightShapeVisual* V = FighterVisuals[i];
		V->SetVisible(!Cur.out);
		for (int32 Slot = 0; Slot < fc::kCompanionSlots; ++Slot)
		{
			const fc::CompanionState& C = Cur.comp[Slot];
			AFightShapeVisual* CV = CompanionVisuals[i * fc::kCompanionSlots + Slot];
			CV->SetVisible(C.onField());
			if (!C.onField()) continue;
			CV->SetActorLocation(FightUnits::ToWorld(C.pos));
			const FLinearColor CC = CompanionColor(fc::companionDef(Setup, i, C).type);
			CV->SetColor(C.action >= 0 ? CC : CC * 0.6f);
		}
		if (Cur.out) continue;

		// don't slide over from the old spot after a respawn
		const fc::FighterState& Prev = bHasPrevious && !Previous.f[i].out ? Previous.f[i] : Cur;
		V->SetActorLocation(FMath::Lerp(FightUnits::ToWorld(Prev.pos), FightUnits::ToWorld(Cur.pos), Alpha));
		V->SetActorRotation(FightUnits::ToWorldDir(Cur.facing).Rotation());

		FLinearColor Color = GetFighterColor(i);
		switch (Cur.state)
		{
		case fc::FState::Hitstun: case fc::FState::Knockdown: Color = FLinearColor::White; break;
		case fc::FState::Guard: case fc::FState::Blockstun: Color = Color * 0.5f + FLinearColor(0.f, 0.5f, 0.5f); break;
		case fc::FState::Locked: Color = FLinearColor(0.5f, 0.1f, 0.7f); break;
		case fc::FState::Heal: Color = FLinearColor(0.3f, 0.9f, 0.4f); break;
		case fc::FState::SimpleRealm: Color = FLinearColor(0.9f, 0.9f, 0.6f); break;
		case fc::FState::Attack:
			if (fc::moveOf(Setup, i, Cur.moveId).isActiveFrame(Cur.moveFrame)) Color = FLinearColor(1.f, 0.9f, 0.1f);
			break;
		default: break;
		}
		if (Cur.burnout > 0) Color *= 0.5f;
		V->SetColor(Color);
	}

	for (int32 i = 0; i < ProjectileVisuals.Num(); ++i)
	{
		const fc::Projectile& P = State.proj[i];
		AFightShapeVisual* PV = ProjectileVisuals[i];
		PV->SetVisible(P.alive);
		if (!P.alive) continue;
		PV->SetActorLocation(FightUnits::ToWorld(P.pos));
		PV->SetSizeCm(FVector(FightUnits::ToCm(P.radius) * 2.f));
		const FLinearColor Base = P.source > 0 ? FLinearColor(0.9f, 0.9f, 0.9f) : GetFighterColor(P.owner);
		PV->SetColor(P.armDelay > 0 ? Base * 0.3f : Base); // dim while telegraphing
	}

	for (int32 k = 0; k < ObstacleVisuals.Num(); ++k) ObstacleVisuals[k]->SetVisible(!State.obstacles[k].broken);
	PresentObjective();

	ImpactFlash = FMath::Max(0.f, ImpactFlash - DeltaSeconds * 8.f);
	UpdateCameras(DeltaSeconds);

#if !UE_BUILD_SHIPPING
	if (GetDefault<UFightGameSettings>()->bDrawHitboxes) DrawHitboxes();
#endif
}

void AFightDirector::PresentObjective()
{
	const fc::MatchState& State = GetState();
	const fc::MatchSetup& Setup = GetSetup();
	const fc::Objective Objective = Setup.rules.objective;

	const int32 Flags = Objective == fc::Objective::CaptureTheFlag ? FMath::Min<int32>(Setup.rules.teamCount, fc::kMaxFlags) : 0;
	for (int32 t = 0; t < FlagVisuals.Num(); ++t)
	{
		FlagVisuals[t]->SetVisible(t < Flags);
		if (t < Flags) FlagVisuals[t]->SetActorLocation(FightUnits::ToWorld(State.objective.flags[t].pos));
	}

	const bool bZones = Objective == fc::Objective::Checkpoints;
	for (int32 z = 0; z < ZoneVisuals.Num(); ++z)
	{
		ZoneVisuals[z]->SetVisible(bZones && z < fc::kMaxZones);
		if (!bZones || z >= fc::kMaxZones) continue;
		const fc::ZoneState& Zone = State.objective.zones[z];
		FLinearColor Color = Zone.owner >= 0 ? GetTeamColor(Zone.owner) : FLinearColor(0.5f, 0.5f, 0.5f);
		if (Zone.capturing >= 0) Color = FMath::Lerp(Color, GetTeamColor(Zone.capturing), Zone.progress / 240.f);
		ZoneVisuals[z]->SetColor(Color);
	}

	const bool bBall = Objective == fc::Objective::Oddball;
	BallVisual->SetVisible(bBall);
	if (bBall) BallVisual->SetActorLocation(FightUnits::ToWorld(State.objective.ballPos) + FVector(0.f, 0.f, 30.f));
}

void AFightDirector::UpdateCameras(float DeltaSeconds)
{
	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	FVector ShakeOffset = FVector::ZeroVector;
	if (ShakeTime > 0.f)
	{
		ShakeTime -= DeltaSeconds;
		ShakeOffset = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * ShakeStrength;
	}

	for (int32 i = 0; i < Cameras.Num(); ++i)
	{
		FFightChaseCamera& Cam = CameraState[i];
		const int32 Fighter = LocalFighters[i];
		const fc::FighterState& F = GetState().f[Fighter];
		const FVector Self = FighterVisuals[Fighter]->GetActorLocation();
		const bool bLocked = F.lockOn && F.target >= 0;
		float Distance = 600.f;
		FVector LookAt;

		if (bLocked)
		{
			// sit behind our fighter and look toward the lock-on target
			FVector ToOpponent = FighterVisuals[F.target]->GetActorLocation() - Self;
			ToOpponent.Z = 0.f;
			Distance = ToOpponent.Size();
			if (Distance > 1.f)
			{
				const FVector WantedForward = ToOpponent / Distance;
				const float Blend = Cam.bInitialized ? FMath::Min(1.f, DeltaSeconds * S->CameraFollowSpeed * 0.8f) : 1.f;
				Cam.Forward = (Cam.Forward + (WantedForward - Cam.Forward) * Blend).GetSafeNormal2D();
			}
			Cam.Yaw = Cam.Forward.Rotation().Yaw;
			LookAt = Self + Cam.Forward * FMath::Clamp(Distance * 0.6f, 200.f, 900.f) + FVector(0.f, 0.f, 110.f);
		}
		else
		{
			// free camera: the right stick turns it
			if (!Cam.bInitialized) Cam.Yaw = FightUnits::ToWorldDir(F.facing).Rotation().Yaw;
			Cam.Yaw += Samplers[i].GetLookYaw() * S->CameraTurnSpeed * DeltaSeconds;
			Cam.Forward = FRotator(0.f, Cam.Yaw, 0.f).Vector();
			LookAt = Self + Cam.Forward * 600.f + FVector(0.f, 0.f, 110.f);
		}
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Cam.Forward);

		// back off a little when we're right on top of the target
		const float Back = S->CameraDistance + FMath::Max(0.f, 300.f - Distance) * 0.5f;
		const FVector WantedLocation = Self - Cam.Forward * Back + Right * S->CameraShoulderOffset + FVector(0.f, 0.f, S->CameraHeight);

		Cam.Location = Cam.bInitialized ? FMath::VInterpTo(Cam.Location, WantedLocation, DeltaSeconds, S->CameraFollowSpeed) : WantedLocation;
		Cam.bInitialized = true;

		Cameras[i]->SetActorLocation(Cam.Location + ShakeOffset);
		Cameras[i]->SetActorRotation((LookAt - Cam.Location).Rotation());
	}
}

void AFightDirector::DrawHitboxes() const
{
	const fc::MatchState& State = GetState();
	const fc::MatchSetup& Setup = GetSetup();
	UWorld* World = GetWorld();
	static const float HurtY[3] = { 0.5f, 1.2f, 1.7f };
	static const float HurtR[3] = { 0.45f, 0.45f, 0.30f };

	for (int32 i = 0; i < State.count; ++i)
	{
		const fc::FighterState& F = State.f[i];
		if (F.out) continue;
		for (int32 h = 0; h < 3; ++h)
		{
			const FVector C = FightUnits::ToWorld(F.pos) + FVector(0.f, 0.f, HurtY[h] * 100.f);
			DrawDebugSphere(World, C, HurtR[h] * 100.f, 10, FColor(40, 200, 60), false, -1.f, 0, 0.8f);
		}
		if (F.state != fc::FState::Attack) continue;
		const fc::MoveDef& Move = fc::moveOf(Setup, i, F.moveId);
		if (!Move.isActiveFrame(F.moveFrame)) continue;
		for (const fc::HitboxDef& Hb : Move.hitboxes)
		{
			fc::Vec3 Off = Hb.offset;
			if (F.moveId < fc::kTechMoveOffset) Off.z = Off.z * Setup.stats[i].reach;
			DrawDebugSphere(World, FightUnits::ToWorld(LocalToWorld(F, Off)), FightUnits::ToCm(Hb.radius), 12,
				FColor(230, 40, 40), false, -1.f, 0, 1.5f);
		}
	}
}

// ---------------------------------------------------------------- online

void AFightDirector::StartOnline()
{
	LoadMods(); // the content hash has to be final before the first Hello
	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	LocalPlayers = FMath::Clamp(S->LocalPlayers, 1, MaxLocalPlayers);
	std::vector<fc::net::PlayerRequest> Requests;
	for (int32 i = 0; i < LocalPlayers; ++i)
		Requests.push_back(ToRequest(S->Loadouts.IsValidIndex(i) ? S->Loadouts[i] : FFightLoadout()));

	const bool bHost = NetMode == EFightNetMode::Host;
	LobbyTime = 0.f;

	if (S->Backend == EFightNetBackend::Eos)
	{
		// Matchmaking first, the fc::net handshake starts once we sit in an EOS lobby.
		// The bucket keeps different builds, mod sets and modes apart.
		FFightEos::FConfig Config;
		Config.ProductId = S->EosProductId;
		Config.SandboxId = S->EosSandboxId;
		Config.DeploymentId = S->EosDeploymentId;
		Config.ClientId = S->EosClientId;
		Config.ClientSecret = S->EosClientSecret;
		Config.DisplayName = S->DisplayName;
		Config.LobbyCode = S->LobbyCode.TrimStartAndEnd();
		Config.bHost = bHost;
		Config.MaxMembers = ToRules(*S).fighters; // one console has at least one fighter
		Config.Bucket = FString::Printf(TEXT("carnage:%u:%d:%016llx"), static_cast<uint32>(fc::net::kProtocolVersion), static_cast<int32>(S->Mode),
			static_cast<unsigned long long>(fc::Registry::instance().contentHash()));
		PendingRequests = std::move(Requests);
		Eos = MakeUnique<FFightEos>();
		Eos->Begin(Config);
		return;
	}

	Udp = MakeUnique<FFightUdpTransport>();
	if (!Udp->Open(bHost ? S->Port : 0))
	{
		NetError = TEXT("Couldn't open a UDP socket");
		return;
	}
	Link = Udp.Get();
	if (bHost)
	{
		Link->SetAcceptNewPeers(true);
		LobbyHost = MakeUnique<fc::net::LobbyHost>(*Link, std::move(Requests));
		UE_LOG(LogFight, Log, TEXT("Hosting on port %d, waiting for %d consoles"), Udp->GetBoundPort(), S->WaitForConsoles);
	}
	else
	{
		if (!Link->SetPeer(0, S->HostAddress))
		{
			NetError = FString::Printf(TEXT("Bad host address '%s'"), *S->HostAddress);
			return;
		}
		LobbyClient = MakeUnique<fc::net::LobbyClient>(*Link, std::move(Requests));
		UE_LOG(LogFight, Log, TEXT("Joining %s"), *S->HostAddress);
	}
}

void AFightDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	// the sessions hold references to the transport, so they go first
	Peer.Reset();
	LobbyHost.Reset();
	LobbyClient.Reset();
	Link = nullptr;
	if (Eos) Eos->Shutdown();
	Eos.Reset();
	Udp.Reset();
	Super::EndPlay(Reason);
}

void AFightDirector::PollLobby(float DeltaSeconds)
{
	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	if (Eos)
	{
		Eos->Tick(DeltaSeconds);
		if (!Link && Eos->IsReady())
		{
			Link = Eos->GetTransport();
			if (NetMode == EFightNetMode::Host) LobbyHost = MakeUnique<fc::net::LobbyHost>(*Link, std::move(PendingRequests));
			else LobbyClient = MakeUnique<fc::net::LobbyClient>(*Link, std::move(PendingRequests));
			LobbyTime = 0.f; // the wait for players starts now
		}
		if (!Link) return;
	}
	LobbyTime += DeltaSeconds;
	LobbyPollTimer -= DeltaSeconds;
	if (LobbyPollTimer > 0.f) return;
	LobbyPollTimer = 0.1f; // 10 Hz is plenty for a handshake

	if (LobbyClient)
	{
		if (!LobbyClient->poll()) return;
		const fc::net::LobbyConfig Config = LobbyClient->config();
		const uint8 Me = LobbyClient->localPeer();
		LobbyClient.Reset();
		// full mesh: the host told us where everyone else is
		for (int32 p = 1; p < static_cast<int32>(Config.peerAddresses.size()); ++p)
			if (p != Me && !Config.peerAddresses[p].empty())
				Link->SetPeer(static_cast<uint8>(p), UTF8_TO_TCHAR(Config.peerAddresses[p].c_str()));
		BeginOnlineMatch(Config, Me);
		return;
	}

	if (!LobbyHost) return;
	if (Peer)
	{
		// The match is on and the PeerSession owns the socket now, so no more poll() here.
		// Anyone who missed the Start packet gets a few more copies.
		if (StartResends-- > 0) LobbyHost->resendStart();
		else LobbyHost.Reset();
		return;
	}
	LobbyHost->poll();
	const bool bFull = LobbyHost->peerCount() >= S->WaitForConsoles;
	const bool bTimeout = S->LobbyTimeoutSeconds > 0.f && LobbyTime >= S->LobbyTimeoutSeconds;
	if (!bFull && !bTimeout) return;

	Link->SetAcceptNewPeers(false);
	if (Eos) Eos->LockLobby();
	std::vector<std::string> Addresses;
	for (const FString& A : Link->GetPeerAddresses()) Addresses.push_back(ToStd(A));
	const fc::net::LobbyConfig Config = LobbyHost->start(ToMode(S->Mode), ToRules(*S), static_cast<uint8_t>(S->BotLevel),
		static_cast<uint32_t>(FMath::Rand()), static_cast<uint8_t>(S->InputDelay), Addresses);
	StartResends = 30; // 3 seconds
	BeginOnlineMatch(Config, 0);
}

bool AFightDirector::BeginOnlineMatch(const fc::net::LobbyConfig& Config, uint8 LocalPeer)
{
	// the host builds its setup from the same lobby description as everyone else,
	// so all consoles end up with byte-identical matches
	std::string Error;
	if (!fc::net::buildSetup(Config, OnlineSetup, &Error))
	{
		NetError = FString::Printf(TEXT("Can't join: %s"), UTF8_TO_TCHAR(Error.c_str()));
		UE_LOG(LogFight, Error, TEXT("%s"), *NetError);
		return false;
	}
	Lobby = Config;
	OwnPeer = LocalPeer;
	Round = 0;
	Peer = MakeUnique<fc::net::PeerSession>(Lobby, OnlineSetup, OwnPeer, *Link, Round);

	const std::vector<int>& Slots = Peer->localSlots();
	LocalPlayers = FMath::Clamp(static_cast<int32>(Slots.size()), 1, MaxLocalPlayers);
	for (int32 i = 0; i < MaxLocalPlayers; ++i) LocalFighters[i] = i < static_cast<int32>(Slots.size()) ? Slots[i] : 0;

	bHasPrevious = false;
	RoundOverFrames = 0;
	bResultRecorded = false;
	bDesyncReported = false;
	FMemory::Memzero(BlackSparks);
	FMemory::Memzero(RealmsOpened);
	Accumulator = 0.0;
	UE_LOG(LogFight, Log, TEXT("Online match on %s: %d fighters, %d consoles, we are peer %d"),
		UTF8_TO_TCHAR(fc::mapOf(OnlineSetup).name), OnlineSetup.count, Lobby.peerCount, OwnPeer);
	StartPresentation();
	return true;
}

void AFightDirector::SimulateOnlineFrame()
{
	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	for (int32 p = 0; p < LocalPlayers; ++p)
		Peer->setLocalInput(p, Samplers[p].Build(S->StickDeadzone, CameraState[p].Yaw));

	Previous = Peer->session().state();
	bHasPrevious = true;
	Events.clear();
	Peer->tick(&Events);
	// a rollback can replay a hit, effects only care about the newest frame anyway
	for (const fc::ImpactEvent& E : Events) HandleEvent(E);

	if (Peer->desynced() && !bDesyncReported)
	{
		bDesyncReported = true;
		UE_LOG(LogFight, Error, TEXT("Desync at frame %d: the consoles disagree about the match"), Peer->frame());
	}

	// only end the round on a confirmed state, a predicted KO might get rolled back
	const fc::MatchState* Settled = Peer->confirmedState();
	if (!Settled || Settled->winner == fc::kNoWinner) return;
	if (!bResultRecorded)
	{
		bResultRecorded = true;
		RecordMatchResult(*Settled);
	}
	// counted in ticks, not frames: everyone keeps sending until they restart too
	if (++RoundOverFrames >= S->RestartDelayFrames) Rematch();
}

void AFightDirector::Rematch()
{
	Peer = MakeUnique<fc::net::PeerSession>(Lobby, OnlineSetup, OwnPeer, *Link, ++Round);
	bHasPrevious = false;
	RoundOverFrames = 0;
	bResultRecorded = false;
	FMemory::Memzero(BlackSparks);
	FMemory::Memzero(RealmsOpened);
	ApplyAppearances();
}

FString AFightDirector::GetNetStatus() const
{
	if (!NetError.IsEmpty()) return NetError;
	if (Eos && (!Link || Eos->GetStep() == FFightEos::EStep::Failed)) return Eos->GetStatus();
	if (LobbyClient) return TEXT("Waiting for the host ...");
	if (LobbyHost && !Peer)
		return FString::Printf(TEXT("Lobby: %d / %d consoles  (%s)"), LobbyHost->peerCount(),
			GetDefault<UFightGameSettings>()->WaitForConsoles, Link ? *Link->Describe() : TEXT(""));
	if (!Peer) return FString();
	if (Peer->desynced()) return TEXT("DESYNC - the consoles disagree, check that everyone has the same mods");
	if (Peer->stalled()) return TEXT("Waiting for other players ...");
	return FString();
}
