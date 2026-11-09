#pragma once

#include "CoreMinimal.h"
#include "FightGameSettings.h"
#include "Widgets/SCompoundWidget.h"

class AFightDirector;
class UFightOnline;
class SEditableTextBox;
class SVerticalBox;

// Placeholder menus: plain white, black text, no assets. Built in Slate so they work
// with a controller from the first frame (d-pad/stick to move, A/Enter to pick, B/Esc
// back, left/right to change a value) and need nothing from the editor.

struct FFightMenuItem
{
	enum class EKind : uint8 { Button, Choice, Slider, Text, Info };

	EKind Kind = EKind::Button;
	TAttribute<FText> Label;
	TAttribute<bool> Enabled = true;

	TFunction<void()> OnPress;                       // Button

	TArray<FText> Options;                           // Choice
	TFunction<int32()> GetIndex;
	TFunction<void(int32)> SetIndex;

	TFunction<float()> GetValue;                     // Slider, 0..1
	TFunction<void(float)> SetValue;
	TFunction<FText(float)> FormatValue;             // optional, defaults to percent
	float Step = 0.05f;

	TFunction<FString()> GetText;                    // Text
	TFunction<void(const FString&)> SetText;

	static FFightMenuItem Button(const TAttribute<FText>& Label, TFunction<void()> OnPress, TAttribute<bool> Enabled = true);
	static FFightMenuItem Choice(const FText& Label, TArray<FText> Options, TFunction<int32()> Get, TFunction<void(int32)> Set);
	static FFightMenuItem Toggle(const FText& Label, TFunction<bool()> Get, TFunction<void(bool)> Set);
	static FFightMenuItem Slider(const FText& Label, TFunction<float()> Get, TFunction<void(float)> Set, float Step = 0.05f);
	static FFightMenuItem Text(const FText& Label, TFunction<FString()> Get, TFunction<void(const FString&)> Set);
	static FFightMenuItem Info(const TAttribute<FText>& Label);
};

struct FFightMenuPage
{
	FText Title;
	TAttribute<FText> Subtitle;
	TArray<FFightMenuItem> Items;
	// B / Esc. Unset means the page can't be left that way (e.g. the title screen).
	TFunction<void()> OnBack;
};

// One line in a menu. Focusable unless it's an Info line or disabled.
class FIGHTGAME_API SFightMenuRow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFightMenuRow) {}
		SLATE_ARGUMENT(FFightMenuItem, Item)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FNavigationReply OnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event) override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void OnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	void Press();
	void Change(int32 Direction);
	FText ValueText() const;
	bool IsLive() const { return Item.Kind != FFightMenuItem::EKind::Info && Item.Enabled.Get(); }

	FFightMenuItem Item;
	TSharedPtr<SEditableTextBox> TextBox;
};

// The whole screen: white background, a title and a column of rows.
class FIGHTGAME_API SFightMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFightMenu) : _Opaque(true) {}
		// false draws a translucent white sheet over the game instead (pause, lobby)
		SLATE_ARGUMENT(bool, Opaque)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	// bKeepFocus: stay on the same line (for refreshes), otherwise start at the top
	void ShowPage(const FFightMenuPage& Page, bool bKeepFocus = false);
	void FocusFirst();

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply OnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event) override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	FFightMenuPage Current;
	TSharedPtr<SVerticalBox> Rows;
	TArray<TSharedRef<SFightMenuRow>> RowWidgets;
	FText Title;
	TAttribute<FText> Subtitle;
};

// Which pages exist and what they do. Pages are rebuilt from scratch whenever they are
// shown, so they always reflect the current settings, party and lobby state.
class FIGHTGAME_API FFightMenus : public TSharedFromThis<FFightMenus>
{
public:
	enum class ERoot : uint8 { Title, Lobby, Pause };

	FFightMenus(UWorld* InWorld, ERoot Root);
	~FFightMenus();

	void Open();
	void Close();
	bool IsOpen() const { return bOpen; }
	// Rebuild the page on screen (party joined, lobby changed ...)
	void Refresh();
	void SetDirector(AFightDirector* InDirector) { Director = InDirector; }
	void Tick(float DeltaSeconds);

private:
	using FBuilder = TFunction<FFightMenuPage()>;
	void Push(FBuilder Builder);
	void Pop();
	void Show(bool bKeepFocus = false);

	FFightMenuPage TitlePage();
	FFightMenuPage PlayPage();
	FFightMenuPage ModePage(int32 Mode);
	FFightMenuPage CustomRulesPage();
	FFightMenuPage PartyPage();
	FFightMenuPage SettingsPage();
	FFightMenuPage DisplayPage();
	FFightMenuPage GraphicsPage();
	FFightMenuPage AudioPage();
	FFightMenuPage ControlsPage();
	FFightMenuPage OnlinePage();
	FFightMenuPage QuitPage();
	FFightMenuPage CharacterPage(int32 Player);
	FFightMenuPage LobbyPage();
	FFightMenuPage PausePage();

	void StartMatch(int32 Mode, bool bOnline);
	// after offline / online: everyone on this console picks a fighter, then the match starts
	void PickCharacters(int32 Mode, bool bOnline);
	UFightOnline* Online() const;

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AFightDirector> Director;
	ERoot Root;
	TSharedPtr<SFightMenu> Widget;
	TArray<FBuilder> Stack;
	bool bOpen = false;
	float RefreshTimer = 0.f;
	FString PartyShown; // party state the page on screen was built with

	// menu choices that only matter until the match starts
	int32 LocalPlayers = 1;
	FFightCustomRules Custom;
	TArray<FFightLoadout> Picks;
	int32 PendingMode = 0;
	bool bPendingOnline = false;
	FString PartyCodeInput;
	TArray<FIntPoint> Resolutions;
};
