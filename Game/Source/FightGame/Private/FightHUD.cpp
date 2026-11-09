#include "FightHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"
#include "FightDirector.h"
#include "FightGameSettings.h"
#include "Kismet/GameplayStatics.h"

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

void AFightHUD::DrawHUD()
{
	Super::DrawHUD();
	// Nur einmal zeichnen (zweiter lokaler Spieler teilt sich den Bildschirm)
	if (PlayerOwner != UGameplayStatics::GetPlayerController(this, 0)) return;

	if (!Director.IsValid())
		for (TActorIterator<AFightDirector> It(GetWorld()); It; ++It) Director = *It;
	if (!Director.IsValid() || !Director->HasSession()) return;

	const fc::MatchState& State = Director->GetState();
	const fc::MatchSetup& Setup = Director->GetSetup();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	UiScale = H / 1080.f;
	const float S = UiScale;
	const float Margin = 40.f * S;
	const float BarW = W * 0.38f;

	for (int32 i = 0; i < 2; ++i)
	{
		const fc::FighterState& F = State.f[i];
		const fc::BodyStats& Stats = Setup.stats[i];
		const bool bRight = i == 1;
		const float X = bRight ? W - Margin - BarW : Margin;
		const float TextX = bRight ? W - Margin : Margin;

		const fc::Loadout& L = Setup.loadout[i];
		const FString Name = FString::Printf(TEXT("P%d  %s / %s"), i + 1, UTF8_TO_TCHAR(L.body->name),
			L.technique ? UTF8_TO_TCHAR(L.technique->name) : TEXT("Nullgeboren"));
		DrawLabel(Name, TextX, Margin - 4.f * S, FLinearColor::White, 1.1f, bRight);

		float Y = Margin + 30.f * S;
		DrawBar(X, Y, BarW, 26.f * S, static_cast<float>(F.health) / Stats.maxHealth, FLinearColor(0.9f, 0.2f, 0.15f), bRight);
		Y += 34.f * S;

		if (Stats.ceMax > 0)
		{
			const FLinearColor EssenceColor = F.burnout > 0 ? FLinearColor(0.4f, 0.4f, 0.4f) : FLinearColor(0.2f, 0.6f, 1.f);
			DrawBar(X, Y, BarW * 0.8f, 14.f * S, static_cast<float>(F.ce) / Stats.ceMax, EssenceColor,
				bRight);
		}
		Y += 20.f * S;
		const bool bRealmReady = F.realmMeter >= fc::kRealmMeterMax;
		DrawBar(X, Y, BarW * 0.6f, 10.f * S, static_cast<float>(F.realmMeter) / fc::kRealmMeterMax,
			bRealmReady ? FLinearColor(1.f, 0.85f, 0.1f) : FLinearColor(0.7f, 0.55f, 0.1f), bRight);
		Y += 18.f * S;

		FString Status;
		if (F.burnout > 0) Status += TEXT("BURNOUT  ");
		if (bRealmReady) Status += TEXT("REALM BEREIT  ");
		if (F.reinforce) Status += TEXT("VERSTAERKT  ");
		if (!F.apexUsed && L.technique) Status += TEXT("APEX  ");
		Status += FString::Printf(TEXT("Bond %d"), fc::bondLevel(F));
		DrawLabel(Status, TextX, Y, FLinearColor(1.f, 1.f, 0.8f), 0.8f, bRight);
		Y += 24.f * S;

		// Skill-Cooldowns
		for (int32 Slot = 0; Slot < 3; ++Slot)
		{
			const float Box = 22.f * S;
			const float BX = bRight ? W - Margin - (Slot + 1) * (Box + 6.f * S) : Margin + Slot * (Box + 6.f * S);
			const bool bReady = L.technique && F.cooldown[Slot] == 0 && F.burnout == 0;
			DrawRect(bReady ? FLinearColor(0.3f, 0.9f, 0.5f) : FLinearColor(0.25f, 0.25f, 0.25f), BX, Y, Box, Box);
		}
		Y += 32.f * S;

		// Begleiter
		for (int32 Slot = 0; Slot < fc::kCompanionSlots; ++Slot)
		{
			const fc::CompanionState& C = F.comp[Slot];
			if (C.roster < 0) continue;
			const fc::CompanionDef& Def = fc::companionDef(Setup, i, C);
			FString Label = UTF8_TO_TCHAR(Def.name);
			if (C.down > 0) Label += FString::Printf(TEXT("  zurueck in %ds"), C.down / 60 + 1);
			DrawLabel(Label, TextX, Y, FLinearColor(0.8f, 0.8f, 0.8f), 0.7f, bRight);
			Y += 18.f * S;
			DrawBar(X, Y, BarW * 0.3f, 8.f * S, static_cast<float>(C.hp) / Def.maxHp, FLinearColor(0.6f, 0.9f, 0.6f), bRight);
			Y += 14.f * S;
		}
	}

	// Realm / Clash
	if (State.realm.owner >= 0)
	{
		const fc::Loadout& L = Setup.loadout[State.realm.owner];
		const FString Text = State.realm.clashTimer > 0
			? FString::Printf(TEXT("REALM-CLASH!  Light haemmern  %d : %d"), State.realm.clashCount[0], State.realm.clashCount[1])
			: FString::Printf(TEXT("REALM  P%d  %s  %ds%s"), State.realm.owner + 1,
				L.technique ? UTF8_TO_TCHAR(L.technique->name) : TEXT(""), State.realm.timer / 60 + 1,
				State.realm.open ? TEXT("  (offen)") : TEXT(""));
		float TW = 0.f, TH = 0.f;
		GetTextSize(Text, TW, TH, GEngine->GetMediumFont(), 1.3f * S);
		DrawLabel(Text, (W - TW) * 0.5f, H * 0.16f, FLinearColor(1.f, 0.85f, 0.2f), 1.3f);
	}

	if (State.winner != 255)
	{
		const FString Text = State.winner == 2 ? TEXT("UNENTSCHIEDEN") : FString::Printf(TEXT("P%d GEWINNT"), State.winner + 1);
		float TW = 0.f, TH = 0.f;
		GetTextSize(Text, TW, TH, GEngine->GetLargeFont(), 2.f * S);
		DrawText(Text, FLinearColor::White, (W - TW) * 0.5f, H * 0.4f, GEngine->GetLargeFont(), 2.f * S);
	}

	if (GetDefault<UFightGameSettings>()->bShowFrameInfo)
	{
		DrawLabel(FString::Printf(TEXT("Frame %d   Rollbacks %d"), State.frame, Director->GetRollbackCount()),
			Margin, H - Margin - 20.f * S, FLinearColor(0.7f, 0.7f, 0.7f), 0.7f);
	}

	// Impact-Frame: kurzer Weiß-Blitz (Platzhalter für invertiertes Tusche-Bild)
	if (Director->GetImpactFlash() > 0.f)
		DrawRect(FLinearColor(1.f, 1.f, 1.f, Director->GetImpactFlash() * 0.55f), 0.f, 0.f, W, H);
}
