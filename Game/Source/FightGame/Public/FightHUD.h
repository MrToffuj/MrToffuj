#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "FightHUD.generated.h"

class AFightDirector;

// Placeholder canvas HUD, one per local viewport: you on the left, your lock-on target on the right,
// plus scores for the mode. Scales with the viewport so it stays readable in splitscreen and on handhelds.
UCLASS()
class FIGHTGAME_API AFightHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawBar(float X, float Y, float W, float H, float Fill, const FLinearColor& Color, bool bMirror);
	void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale, bool bAlignRight = false);
	void DrawCentered(const FString& Text, float Y, const FLinearColor& Color, float Scale, bool bLarge);
	void DrawFighterPanel(int32 Fighter, bool bRight, bool bDetailed);
	void DrawKoBoard();
	void DrawObjective(int32 Self);

	TWeakObjectPtr<AFightDirector> Director;
	float UiScale = 1.f;
};
