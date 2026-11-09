#include "FightHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "FightDirector.h"
#include "FightGameSettings.h"

void AFightHUD::DrawBar(float X, float Y, float W, float H, float Fill, const FLinearColor& Color, bool bMirror)
{
	Fill = FMath::Clamp(Fill, 0.f, 1.f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 2.f, Y - 2.f, W + 4.f, H + 4.f);
	const float FillW = W * Fill;
	DrawRect(Color, bMirror ? X + W - FillW : X, Y, FillW, H);
}

void AFightHUD::DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale, bool bAlignRight)
{
	UFont* Font = GEngine->GetMediumFont();
	if (bAlignRight)
	{
		float TW = 0.f, TH = 0.f;
		GetTextSize(Text, TW, TH, Font, Scale * UiScale);
		X -= TW;
	}
	DrawText(Text, Color, X, Y, Font, Scale * UiScale);
}

void AFightHUD::DrawFighterPanel(int32 Fighter, bool bRight, bool bDetailed)
{
	const fc::MatchState& State = Director->GetState();
	const fc::MatchSetup& Setup = Director->GetSetup();
	const fc::FighterState& F = State.f[Fighter];
	const fc::BodyStats& Stats = Setup.stats[Fighter];
	const fc::Loadout& L = Setup.loadout[Fighter];
	const float S = UiScale;
	const float W = Canvas->ClipX;
	const float Margin = 40.f * S;
	const float BarW = W * 0.38f;
	const float X = bRight ? W - Margin - BarW : Margin;
	const float TextX = bRight ? W - Margin : Margin;

	const FString Name = FString::Printf(TEXT("%s  %s / %s"), *Director->GetFighterLabel(Fighter), UTF8_TO_TCHAR(L.body->name),
		L.technique ? UTF8_TO_TCHAR(L.technique->name) : TEXT("Heaven's Bind"));
	DrawLabel(Name, TextX, Margin - 4.f * S, Director->GetFighterColor(Fighter), 1.1f, bRight);

	float Y = Margin + 30.f * S;
	DrawBar(X, Y, BarW, 26.f * S, static_cast<float>(F.health) / Stats.maxHealth, FLinearColor(0.9f, 0.2f, 0.15f), bRight);
	Y += 34.f * S;

	if (Stats.ceMax > 0)
	{
		const FLinearColor CEColor = F.burnout > 0 ? FLinearColor(0.4f, 0.4f, 0.4f) : FLinearColor(0.2f, 0.6f, 1.f);
		DrawBar(X, Y, BarW * 0.8f, 14.f * S, static_cast<float>(F.ce) / Stats.ceMax, CEColor, bRight);
	}
	Y += 20.f * S;
	const bool bRealmReady = F.realmMeter >= fc::kRealmMeterMax;
	DrawBar(X, Y, BarW * 0.6f, 10.f * S, static_cast<float>(F.realmMeter) / fc::kRealmMeterMax,
		bRealmReady ? FLinearColor(1.f, 0.85f, 0.1f) : FLinearColor(0.7f, 0.55f, 0.1f), bRight);
	Y += 18.f * S;
	if (!bDetailed) return;

	FString Status;
	if (F.burnout > 0) Status += TEXT("BURNOUT  ");
	if (bRealmReady) Status += TEXT("REALM READY  ");
	if (F.reinforce) Status += TEXT("REINFORCED  ");
	if (!F.maxOutputUsed && L.technique) Status += TEXT("MAX OUTPUT  ");
	Status += FString::Printf(TEXT("Bond %d"), fc::bondLevel(F));
	DrawLabel(Status, TextX, Y, FLinearColor(1.f, 1.f, 0.8f), 0.8f, bRight);
	Y += 24.f * S;

	for (int32 Slot = 0; Slot < 3; ++Slot)
	{
		const float Box = 22.f * S;
		const float BX = bRight ? W - Margin - (Slot + 1) * (Box + 6.f * S) : Margin + Slot * (Box + 6.f * S);
		const bool bReady = L.technique && F.cooldown[Slot] == 0 && F.burnout == 0;
		DrawRect(bReady ? FLinearColor(0.3f, 0.9f, 0.5f) : FLinearColor(0.25f, 0.25f, 0.25f), BX, Y, Box, Box);
	}
	Y += 32.f * S;

	for (int32 Slot = 0; Slot < fc::kCompanionSlots; ++Slot)
	{
		const fc::CompanionState& C = F.comp[Slot];
		if (C.roster < 0) continue;
		const fc::CompanionDef& Def = fc::companionDef(Setup, Fighter, C);
		FString Label = UTF8_TO_TCHAR(Def.name);
		if (C.down > 0) Label += FString::Printf(TEXT("  back in %ds"), C.down / 60 + 1);
		DrawLabel(Label, TextX, Y, FLinearColor(0.8f, 0.8f, 0.8f), 0.7f, bRight);
		Y += 18.f * S;
		DrawBar(X, Y, BarW * 0.3f, 8.f * S, static_cast<float>(C.hp) / Def.maxHp, FLinearColor(0.6f, 0.9f, 0.6f), bRight);
		Y += 14.f * S;
	}
}

void AFightHUD::DrawCentered(const FString& Text, float Y, const FLinearColor& Color, float Scale, bool bLarge)
{
	UFont* Font = bLarge ? GEngine->GetLargeFont() : GEngine->GetMediumFont();
	float TW = 0.f, TH = 0.f;
	GetTextSize(Text, TW, TH, Font, Scale * UiScale);
	DrawText(Text, Color, (Canvas->ClipX - TW) * 0.5f, Y, Font, Scale * UiScale);
}

void AFightHUD::DrawKoBoard()
{
	const fc::MatchState& State = Director->GetState();
	TArray<int32, TInlineAllocator<fc::kMaxFighters>> Order;
	for (int32 i = 0; i < State.count; ++i) Order.Add(i);
	Order.Sort([&State](int32 A, int32 B) { return State.f[A].kos != State.f[B].kos ? State.f[A].kos > State.f[B].kos : A < B; });

	const float S = UiScale;
	float Y = Canvas->ClipY * 0.55f;
	const float X = Canvas->ClipX - 40.f * S;
	DrawLabel(TEXT("KOs"), X, Y, FLinearColor(1.f, 0.85f, 0.2f), 0.9f, true);
	Y += 24.f * S;
	for (int32 k = 0; k < FMath::Min(5, Order.Num()); ++k)
	{
		const int32 i = Order[k];
		DrawLabel(FString::Printf(TEXT("%s  %d"), *Director->GetFighterLabel(i), State.f[i].kos), X, Y,
			Director->GetFighterColor(i), 0.8f, true);
		Y += 20.f * S;
	}
}

void AFightHUD::DrawObjective(int32 Self)
{
	const fc::MatchState& State = Director->GetState();
	const fc::MatchSetup& Setup = Director->GetSetup();
	const fc::ModeRules& Rules = Setup.rules;
	if (Rules.objective == fc::Objective::None || Rules.objective == fc::Objective::Elimination) return;

	static const TCHAR* Names[] = { TEXT(""), TEXT(""), TEXT("DEATHMATCH"), TEXT("CAPTURE THE FLAG"), TEXT("ODDBALL"), TEXT("CHECKPOINTS") };
	FString Line = Names[static_cast<int32>(Rules.objective)];
	if (Rules.timeLimitFrames > 0)
	{
		const int32 Left = FMath::Max(0, Rules.timeLimitFrames - State.frame) / 60;
		Line += FString::Printf(TEXT("   %d:%02d"), Left / 60, Left % 60);
	}
	if (Rules.scoreLimit > 0) Line += FString::Printf(TEXT("   first to %d"), Rules.scoreLimit);
	DrawCentered(Line, 24.f * UiScale, FLinearColor::White, 0.9f, false);

	// team scores in a row under the title (FFA deathmatch shows your own KOs instead)
	if (Rules.teamCount == 0)
	{
		DrawCentered(FString::Printf(TEXT("Your KOs: %d"), State.f[Self].kos), 50.f * UiScale, FLinearColor::White, 1.1f, false);
	}
	else
	{
		const float Step = 90.f * UiScale;
		float X = Canvas->ClipX * 0.5f - Step * (Rules.teamCount - 1) * 0.5f;
		for (int32 t = 0; t < Rules.teamCount; ++t, X += Step)
		{
			const FString Score = FString::Printf(TEXT("%d"), State.objective.score[t]);
			float TW = 0.f, TH = 0.f;
			GetTextSize(Score, TW, TH, GEngine->GetMediumFont(), 1.3f * UiScale);
			DrawText(Score, Director->GetTeamColor(t), X - TW * 0.5f, 48.f * UiScale, GEngine->GetMediumFont(), 1.3f * UiScale);
		}
	}

	if (fc::carriesObjective(State, Self))
	{
		const TCHAR* What = Rules.objective == fc::Objective::Oddball ? TEXT("YOU HAVE THE BALL") : TEXT("YOU HAVE THE FLAG - BRING IT HOME");
		DrawCentered(What, Canvas->ClipY * 0.75f, FLinearColor(1.f, 0.85f, 0.3f), 1.1f, false);
	}
}

void AFightHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Director.IsValid())
		for (TActorIterator<AFightDirector> It(GetWorld()); It; ++It) Director = *It;
	if (!Director.IsValid() || !Director->HasSession()) return;

	const fc::MatchState& State = Director->GetState();
	const fc::MatchSetup& Setup = Director->GetSetup();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	// splitscreen viewports are smaller, scale by whichever side is tighter
	UiScale = FMath::Min(H / 1080.f, W / 960.f);
	const float S = UiScale;

	const int32 Self = FMath::Max(0, Director->GetPlayerIndex(PlayerOwner));
	const fc::FighterState& Me = State.f[Self];

	DrawFighterPanel(Self, false, true);
	if (Me.target >= 0) DrawFighterPanel(Me.target, true, Setup.mode == fc::GameMode::Duel);

	if (Setup.mode == fc::GameMode::Brawl) DrawKoBoard();
	DrawObjective(Self);

	// your own realm, or an enemy realm you are standing in
	for (int32 o = 0; o < State.count; ++o)
	{
		const fc::RealmState& R = State.f[o].realm;
		const bool bMine = o == Self;
		if (R.clashTimer > 0 && (bMine || R.clashWith == Self))
		{
			DrawCentered(FString::Printf(TEXT("REALM CLASH!  Mash Light  %d : %d"), R.clashCount,
				State.f[R.clashWith].realm.clashCount), H * 0.16f, FLinearColor(1.f, 0.85f, 0.2f), 1.3f, false);
			break;
		}
		if (R.timer > 0 && R.clashTimer == 0 && (bMine || fc::isEnemy(State, o, Self)))
		{
			const fc::Loadout& L = Setup.loadout[o];
			const FVector Center = FightUnits::ToWorld(R.open ? State.f[o].pos : R.center);
			const float Radius = (R.open ? fc::kOpenRealmRadius : fc::kRealmRadius) * FightUnits::CmPerUnit;
			if (!bMine && FVector::Dist2D(Center, FightUnits::ToWorld(Me.pos)) > Radius) continue;
			DrawCentered(FString::Printf(TEXT("REALM  %s  %s  %ds%s"), *Director->GetFighterLabel(o),
				L.technique ? UTF8_TO_TCHAR(L.technique->name) : TEXT(""), R.timer / 60 + 1,
				R.open ? TEXT("  (open)") : TEXT("")), H * 0.16f, FLinearColor(1.f, 0.85f, 0.2f), 1.3f, false);
			break;
		}
	}

	if (Me.out && Me.respawn > 0)
		DrawCentered(FString::Printf(TEXT("KO  -  respawn in %d"), Me.respawn / 60 + 1), H * 0.45f, FLinearColor::White, 1.6f, true);

	if (State.winner != fc::kNoWinner)
	{
		FString Text;
		if (State.winner == fc::kDraw) Text = TEXT("DRAW");
		else if (Setup.rules.teamCount > 0) Text = FString::Printf(TEXT("TEAM %d WINS"), State.winner + 1);
		else Text = FString::Printf(TEXT("%s WINS"), *Director->GetFighterLabel(State.winner));
		DrawCentered(Text, H * 0.4f, FLinearColor::White, 2.f, true);
	}

	if (GetDefault<UFightGameSettings>()->bShowFrameInfo)
	{
		DrawLabel(FString::Printf(TEXT("Frame %d   Rollbacks %d"), State.frame, Director->GetRollbackCount()),
			40.f * S, H - 60.f * S, FLinearColor(0.7f, 0.7f, 0.7f), 0.7f);
	}

	// impact frame placeholder, the real one is an inverted ink frame
	if (Director->GetImpactFlash() > 0.f)
		DrawRect(FLinearColor(1.f, 1.f, 1.f, Director->GetImpactFlash() * 0.55f), 0.f, 0.f, W, H);
}
