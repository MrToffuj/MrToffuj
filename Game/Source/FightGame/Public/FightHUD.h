#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "FightHUD.generated.h"

class AFightDirector;

// Platzhalter-HUD (Canvas), eins pro Spieler-Viewport: eigene Werte links, Gegner rechts.
// Skaliert mit der Viewport-Höhe -> auch im Splitscreen und auf 7"-Handhelds lesbar.
// Wird später durch UMG im Anime-Stil ersetzt.
UCLASS()
class FIGHTGAME_API AFightHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawBar(float X, float Y, float W, float H, float Fill, const FLinearColor& Color, bool bMirror);
	void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale, bool bAlignRight = false);

	TWeakObjectPtr<AFightDirector> Director;
	float UiScale = 1.f;
};
