#include "FightDirector.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "DrawDebugHelpers.h"
#include "FightGame.h"
#include "FightGameSettings.h"
#include "FightShapeVisual.h"
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

	void FillLoadout(fc::Loadout& Out, const FFightLoadout& In)
	{
		Out.body = BodyOf(In.Body);
		Out.technique = In.Body == EFightBody::HeavensBind ? nullptr : TechniqueOf(In.Technique);
		Out.assist = AssistOf(In.Assist);
		Out.team = static_cast<uint8_t>(FMath::Clamp(In.Team, 0, 1));
	}

	fc::GameMode ToMode(EFightMode M)
	{
		switch (M)
		{
		case EFightMode::Teams: return fc::GameMode::Teams;
		case EFightMode::Brawl: return fc::GameMode::Brawl;
		default: return fc::GameMode::Duel;
		}
	}

	const FLinearColor TeamColors[2] = { FLinearColor(0.95f, 0.35f, 0.15f), FLinearColor(0.15f, 0.45f, 0.95f) };

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
	BuildSession();
	SpawnVisuals();
}

void AFightDirector::BuildSession()
{
	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	const fc::GameMode Mode = ToMode(S->Mode);
	LocalPlayers = FMath::Clamp(S->LocalPlayers, 1, FMath::Min<int32>(MaxLocalPlayers, fc::rulesFor(Mode).fighters));

	fc::Loadout Humans[MaxLocalPlayers];
	for (int32 i = 0; i < LocalPlayers; ++i)
		FillLoadout(Humans[i], S->Loadouts.IsValidIndex(i) ? S->Loadouts[i] : FFightLoadout());

	const fc::MatchSetup Setup = fc::buildMatch(Mode, Humans, LocalPlayers, static_cast<uint8_t>(S->BotLevel),
		static_cast<uint32_t>(FMath::Rand()));
	Session = MakeUnique<fc::Session>(Setup);
	bHasPrevious = false;
	RoundOverFrames = 0;
	Accumulator = 0.0;
	UE_LOG(LogFight, Log, TEXT("Match: Modus %d, %d Kaempfer, %d lokal"), static_cast<int32>(Mode), Setup.count, LocalPlayers);
}

void AFightDirector::RestartMatch()
{
	BuildSession();
}

void AFightDirector::SpawnVisuals()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.Owner = this;
	auto Spawn = [&](EFightShape Shape, const FLinearColor& Color, const FVector& SizeCm, const FVector& Offset)
	{
		AFightShapeVisual* V = World->SpawnActor<AFightShapeVisual>(AFightShapeVisual::StaticClass(), FTransform::Identity, Params);
		V->SetShape(Shape);
		V->SetColor(Color);
		V->SetSizeCm(SizeCm);
		V->SetMeshOffset(Offset);
		return V;
	};

	const int32 Count = GetSetup().count;
	for (int32 i = 0; i < Count; ++i)
	{
		AFightShapeVisual* F = Spawn(EFightShape::Cylinder, GetFighterColor(i), FVector(90.f, 90.f, 180.f), FVector(0.f, 0.f, 90.f));
		F->AddMarker(EFightShape::Cube, FVector(45.f, 0.f, 150.f), FVector(25.f), FLinearColor(0.05f, 0.05f, 0.05f));
		FighterVisuals.Add(F);
	}
	for (int32 i = 0; i < Count * fc::kCompanionSlots; ++i)
	{
		AFightShapeVisual* C = Spawn(EFightShape::Sphere, FLinearColor::White, FVector(100.f), FVector::ZeroVector);
		C->SetVisible(false);
		CompanionVisuals.Add(C);
	}
	for (int32 i = 0; i < fc::kMaxProjectiles; ++i)
	{
		AFightShapeVisual* P = Spawn(EFightShape::Sphere, FLinearColor::White, FVector(50.f), FVector::ZeroVector);
		P->SetVisible(false);
		ProjectileVisuals.Add(P);
	}

	// Arena: Boden + 4 zerstörbare Wände (Reihenfolge wie fc::MatchState::walls: +X, -X, +Z, -Z der Simulation)
	const float Half = FightUnits::ToCm(GetSetup().stage.halfExtent);
	const FLinearColor WallColor(0.35f, 0.3f, 0.27f);
	const FVector WallLocations[4] = {
		FVector(0.f, Half + 25.f, 150.f), FVector(0.f, -Half - 25.f, 150.f),
		FVector(Half + 25.f, 0.f, 150.f), FVector(-Half - 25.f, 0.f, 150.f) };
	const FVector WallSizes[4] = {
		FVector(2.f * Half, 50.f, 300.f), FVector(2.f * Half, 50.f, 300.f),
		FVector(50.f, 2.f * Half, 300.f), FVector(50.f, 2.f * Half, 300.f) };
	for (int32 i = 0; i < 4; ++i)
	{
		AFightShapeVisual* W = Spawn(EFightShape::Cube, WallColor, WallSizes[i], FVector::ZeroVector);
		W->SetActorLocation(WallLocations[i]);
		WallVisuals.Add(W);
	}
	FloorVisual = Spawn(EFightShape::Cube, FLinearColor(0.2f, 0.22f, 0.2f), FVector(4.f * Half + 400.f, 4.f * Half + 400.f, 20.f),
		FVector::ZeroVector);
	FloorVisual->SetActorLocation(FVector(0.f, 0.f, -10.f));

	const float Fov = GetDefault<UFightGameSettings>()->CameraFOV;
	for (int32 i = 0; i < LocalPlayers; ++i)
	{
		ACameraActor* Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
		Cam->GetCameraComponent()->SetFieldOfView(Fov);
		Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
		Cameras.Add(Cam);
	}
}

void AFightDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Session) return;

	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	APlayerController* PC0 = UGameplayStatics::GetPlayerController(this, 0);
	for (int32 i = 0; i < LocalPlayers; ++i)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(this, i);
		if (PC && Cameras.IsValidIndex(i) && PC->GetViewTarget() != Cameras[i]) PC->SetViewTarget(Cameras[i]);
		// Tastatur kommt immer am Spieler-0-Controller an; nur P1/P2 haben ein Tastatur-Layout
		static const FFightBindings NoKeys;
		const FFightBindings& Keys = i == 0 ? S->KeyboardP1 : (i == 1 ? S->KeyboardP2 : NoKeys);
		Samplers[i].Sample(PC, PC0, S->Gamepad, Keys);
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
	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	Previous = Session->state();
	bHasPrevious = true;

	const int32 Frame = Session->frame();
	for (int32 p = 0; p < LocalPlayers; ++p)
		Session->setInput(p, Frame, Samplers[p].Build(S->StickDeadzone));

	Events.clear();
	Session->advance(&Events);
	for (const fc::ImpactEvent& E : Events) HandleEvent(E);

	if (Session->state().winner != fc::kNoWinner && ++RoundOverFrames >= S->RestartDelayFrames)
		RestartMatch();
}

int32 AFightDirector::GetPlayerIndex(const APlayerController* PC) const
{
	for (int32 i = 0; i < LocalPlayers; ++i)
		if (PC && PC == UGameplayStatics::GetPlayerController(this, i)) return i;
	return -1;
}

FString AFightDirector::GetFighterLabel(int32 Fighter) const
{
	return GetSetup().loadout[Fighter].bot ? FString::Printf(TEXT("BOT %d"), Fighter + 1) : FString::Printf(TEXT("P%d"), Fighter + 1);
}

FLinearColor AFightDirector::GetFighterColor(int32 Fighter) const
{
	const fc::MatchSetup& Setup = GetSetup();
	if (Setup.mode == fc::GameMode::Teams) return TeamColors[Setup.loadout[Fighter].team & 1];
	if (Setup.mode == fc::GameMode::Duel) return TeamColors[Fighter & 1];
	// Brawl: jeder eine eigene Farbe
	return FLinearColor::MakeFromHSV8(static_cast<uint8>(Fighter * 255 / FMath::Max(1, static_cast<int32>(Setup.count))), 200, 230);
}

void AFightDirector::HandleEvent(const fc::ImpactEvent& E)
{
	const FFightImpact Impact = FightUnits::ToImpact(E);
	OnImpact.Broadcast(Impact);

	// Kamera-Feedback nur, wenn es nah an einem lokalen Spieler passiert (Brawl: 20 Leute auf der Map)
	bool bNearLocal = false;
	for (int32 i = 0; i < LocalPlayers && !bNearLocal; ++i)
		bNearLocal = FVector::DistSquared(Impact.Location, FightUnits::ToWorld(GetState().f[i].pos)) < FMath::Square(1500.f);
	if (!bNearLocal) return;

	using K = fc::ImpactKind;
	float Shake = 0.f;
	switch (E.kind)
	{
	case K::Hit: case K::GroundSlam: case K::WallSplat: case K::Block:
		Shake = 2.f + 3.f * Impact.Strength;
		break;
	case K::WallBreak: case K::BlackSpark: case K::RealmOpen: case K::RealmClash:
		Shake = 14.f;
		ImpactFlash = 1.f; // Impact-Frame (später: invertiertes Tusche-Bild)
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
	UE_LOG(LogFight, Verbose, TEXT("Frame %d Event %d von P%d"), E.frame, static_cast<int32>(E.kind), E.source + 1);
}

void AFightDirector::Present(float Alpha, float DeltaSeconds)
{
	const fc::MatchState& State = Session->state();
	const fc::MatchSetup& Setup = Session->setup();

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

		// Nach Respawn nicht von der alten Position herüberinterpolieren
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
		PV->SetColor(P.armDelay > 0 ? Base * 0.3f : Base); // Warnung bevor es aktiv wird
	}

	for (int32 i = 0; i < 4; ++i) WallVisuals[i]->SetVisible(!State.walls[i].broken);

	ImpactFlash = FMath::Max(0.f, ImpactFlash - DeltaSeconds * 8.f);
	UpdateCameras(DeltaSeconds);

#if !UE_BUILD_SHIPPING
	if (GetDefault<UFightGameSettings>()->bDrawHitboxes) DrawHitboxes();
#endif
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
		const int32 Target = GetState().f[i].target;
		const FVector Self = FighterVisuals[i]->GetActorLocation();
		const FVector Opponent = Target >= 0 ? FighterVisuals[Target]->GetActorLocation() : Self + Cam.Forward * 500.f;

		// Lock-on: Kamera steht hinter dem eigenen Kämpfer und blickt Richtung Gegner
		FVector ToOpponent = Opponent - Self;
		ToOpponent.Z = 0.f;
		const float Distance = ToOpponent.Size();
		if (Distance > 1.f)
		{
			const FVector Wanted = ToOpponent / Distance;
			const float Blend = Cam.bInitialized ? FMath::Min(1.f, DeltaSeconds * S->CameraFollowSpeed * 0.8f) : 1.f;
			Cam.Forward = (Cam.Forward + (Wanted - Cam.Forward) * Blend).GetSafeNormal2D();
		}
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Cam.Forward);

		// Ganz nah am Gegner etwas zurückziehen, damit beide im Bild bleiben
		const float Back = S->CameraDistance + FMath::Max(0.f, 300.f - Distance) * 0.5f;
		const FVector Target = Self - Cam.Forward * Back + Right * S->CameraShoulderOffset + FVector(0.f, 0.f, S->CameraHeight);
		const FVector LookAt = Self + Cam.Forward * FMath::Clamp(Distance * 0.6f, 200.f, 900.f) + FVector(0.f, 0.f, 110.f);

		Cam.Location = Cam.bInitialized ? FMath::VInterpTo(Cam.Location, Target, DeltaSeconds, S->CameraFollowSpeed) : Target;
		Cam.bInitialized = true;

		Cameras[i]->SetActorLocation(Cam.Location + ShakeOffset);
		Cameras[i]->SetActorRotation((LookAt - Cam.Location).Rotation());
	}
}

void AFightDirector::DrawHitboxes() const
{
	const fc::MatchState& State = Session->state();
	const fc::MatchSetup& Setup = Session->setup();
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
