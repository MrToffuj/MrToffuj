#include "FightMenu.h"

#include "Brushes/SlateColorBrush.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "FightDirector.h"
#include "FightOnline.h"
#include "FightUserSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SViewport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "fightcore/Body.h"
#include "fightcore/CursedObject.h"
#include "fightcore/Registry.h"
#include "fightcore/Simulation.h"
#include "fightcore/Technique.h"

#define LOCTEXT_NAMESPACE "FightMenu"

namespace
{
	// plain white placeholder look
	const FLinearColor Ink(0.02f, 0.02f, 0.02f);
	const FLinearColor Faded(0.55f, 0.55f, 0.55f);
	const FLinearColor Paper = FLinearColor::White;
	const FLinearColor Hover(0.9f, 0.9f, 0.9f);
	const FSlateColorBrush Plain(FLinearColor::White);

	FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	FText Num(int32 N) { return FText::AsNumber(N); }

	TArray<FText> NumberOptions(int32 From, int32 To)
	{
		TArray<FText> Out;
		for (int32 i = From; i <= To; ++i) Out.Add(Num(i));
		return Out;
	}

	FText Clock(float Seconds)
	{
		const int32 S = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%d:%02d"), S / 60, S % 60));
	}

	const TArray<FText>& ModeNames()
	{
		static const TArray<FText> Names = { LOCTEXT("Duel", "1v1"), LOCTEXT("Teams", "2v2"), LOCTEXT("Brawl", "Brawl"), LOCTEXT("Custom", "Custom") };
		return Names;
	}

	UFightUserSettings& User() { return *UFightUserSettings::Get(); }

	// most settings take effect right away and get written to disk
	void Commit()
	{
		User().ApplyNonResolutionSettings();
		User().SaveSettings();
	}
}

// ---------------------------------------------------------------- items

FFightMenuItem FFightMenuItem::Button(const TAttribute<FText>& Label, TFunction<void()> OnPress, TAttribute<bool> Enabled)
{
	FFightMenuItem I;
	I.Kind = EKind::Button;
	I.Label = Label;
	I.OnPress = MoveTemp(OnPress);
	I.Enabled = Enabled;
	return I;
}

FFightMenuItem FFightMenuItem::Choice(const FText& Label, TArray<FText> Options, TFunction<int32()> Get, TFunction<void(int32)> Set)
{
	FFightMenuItem I;
	I.Kind = EKind::Choice;
	I.Label = Label;
	I.Options = MoveTemp(Options);
	I.GetIndex = MoveTemp(Get);
	I.SetIndex = MoveTemp(Set);
	return I;
}

FFightMenuItem FFightMenuItem::Toggle(const FText& Label, TFunction<bool()> Get, TFunction<void(bool)> Set)
{
	return Choice(Label, { LOCTEXT("Off", "Off"), LOCTEXT("On", "On") },
		[Get] { return Get() ? 1 : 0; }, [Set](int32 V) { Set(V != 0); });
}

FFightMenuItem FFightMenuItem::Slider(const FText& Label, TFunction<float()> Get, TFunction<void(float)> Set, float Step)
{
	FFightMenuItem I;
	I.Kind = EKind::Slider;
	I.Label = Label;
	I.GetValue = MoveTemp(Get);
	I.SetValue = MoveTemp(Set);
	I.Step = Step;
	return I;
}

FFightMenuItem FFightMenuItem::Text(const FText& Label, TFunction<FString()> Get, TFunction<void(const FString&)> Set)
{
	FFightMenuItem I;
	I.Kind = EKind::Text;
	I.Label = Label;
	I.GetText = MoveTemp(Get);
	I.SetText = MoveTemp(Set);
	return I;
}

FFightMenuItem FFightMenuItem::Info(const TAttribute<FText>& Label)
{
	FFightMenuItem I;
	I.Kind = EKind::Info;
	I.Label = Label;
	return I;
}

// ---------------------------------------------------------------- row

void SFightMenuRow::Construct(const FArguments& InArgs)
{
	Item = InArgs._Item;
	const bool bInfo = Item.Kind == FFightMenuItem::EKind::Info;

	TSharedRef<SWidget> Value = SNullWidget::NullWidget;
	if (Item.Kind == FFightMenuItem::EKind::Text)
	{
		Value = SNew(SBox).WidthOverride(360.f)
		[
			SAssignNew(TextBox, SEditableTextBox)
			.Font(Font(20))
			// set once, a bound attribute would fight with the typing
			.Text(FText::FromString(Item.GetText ? Item.GetText() : FString()))
			.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
			{
				if (Item.SetText) Item.SetText(NewText.ToString());
				FSlateApplication::Get().SetAllUserFocus(AsShared(), EFocusCause::SetDirectly);
			})
		];
	}
	else if (Item.Kind == FFightMenuItem::EKind::Choice || Item.Kind == FFightMenuItem::EKind::Slider)
	{
		Value = SNew(STextBlock)
			.Font(Font(22))
			.ColorAndOpacity_Lambda([this]() -> FSlateColor { return IsLive() ? Ink : Faded; })
			.Text_Lambda([this] { return FText::Format(LOCTEXT("Value", "<   {0}   >"), ValueText()); });
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&Plain)
		.Padding(0.f)
		.BorderBackgroundColor_Lambda([this]() -> FSlateColor { return HasAnyUserFocus().IsSet() ? Hover : Paper; })
		[
			SNew(SHorizontalBox)
			// black bar on the line you're on
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBorder)
				.BorderImage(&Plain)
				.Padding(FMargin(3.f, 0.f))
				.BorderBackgroundColor_Lambda([this]() -> FSlateColor { return HasAnyUserFocus().IsSet() ? Ink : Paper; })
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(20.f, bInfo ? 6.f : 12.f))
			[
				SNew(STextBlock)
				.Font(Font(bInfo ? 18 : 24))
				.Text(Item.Label)
				.AutoWrapText(true)
				.ColorAndOpacity_Lambda([this, bInfo]() -> FSlateColor { return bInfo || !IsLive() ? Faded : Ink; })
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(20.f, 0.f))
			[
				Value
			]
		]
	];
}

bool SFightMenuRow::SupportsKeyboardFocus() const
{
	return IsLive();
}

FText SFightMenuRow::ValueText() const
{
	if (Item.Kind == FFightMenuItem::EKind::Choice)
	{
		const int32 Index = Item.GetIndex ? Item.GetIndex() : 0;
		return Item.Options.IsValidIndex(Index) ? Item.Options[Index] : FText::GetEmpty();
	}
	const float V = Item.GetValue ? Item.GetValue() : 0.f;
	if (Item.FormatValue) return Item.FormatValue(V);
	return FText::AsPercent(V);
}

void SFightMenuRow::Press()
{
	switch (Item.Kind)
	{
	case FFightMenuItem::EKind::Button:
		if (Item.OnPress)
		{
			// the press may rebuild the page and with it this row, so work on a copy
			const TFunction<void()> OnPress = Item.OnPress;
			OnPress();
		}
		break;
	case FFightMenuItem::EKind::Choice:
		Change(+1);
		break;
	case FFightMenuItem::EKind::Text:
		if (TextBox) FSlateApplication::Get().SetAllUserFocus(TextBox, EFocusCause::SetDirectly);
		break;
	default:
		break;
	}
}

void SFightMenuRow::Change(int32 Direction)
{
	if (Item.Kind == FFightMenuItem::EKind::Choice && Item.Options.Num() > 0 && Item.GetIndex && Item.SetIndex)
	{
		const int32 Count = Item.Options.Num();
		const TFunction<void(int32)> Set = Item.SetIndex;
		Set((Item.GetIndex() + Direction + Count) % Count);
	}
	else if (Item.Kind == FFightMenuItem::EKind::Slider && Item.GetValue && Item.SetValue)
	{
		const TFunction<void(float)> Set = Item.SetValue;
		Set(FMath::Clamp(Item.GetValue() + Direction * Item.Step, 0.f, 1.f));
	}
}

FReply SFightMenuRow::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (IsLive() && FSlateApplication::Get().GetNavigationActionFromKey(KeyEvent) == EUINavigationAction::Accept)
	{
		Press();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FNavigationReply SFightMenuRow::OnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event)
{
	const EUINavigation Dir = Event.GetNavigationType();
	const bool bAdjustable = Item.Kind == FFightMenuItem::EKind::Choice || Item.Kind == FFightMenuItem::EKind::Slider;
	if (bAdjustable && IsLive() && (Dir == EUINavigation::Left || Dir == EUINavigation::Right))
	{
		Change(Dir == EUINavigation::Left ? -1 : +1);
		return FNavigationReply::Stop();
	}
	return SCompoundWidget::OnNavigation(Geometry, Event);
}

FReply SFightMenuRow::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!IsLive() || Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	if (Item.Kind == FFightMenuItem::EKind::Slider)
	{
		// left half lowers, right half raises
		const float X = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()).X;
		Change(X < Geometry.GetLocalSize().X * 0.5f ? -1 : +1);
	}
	else
	{
		Press();
	}
	return FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
}

void SFightMenuRow::OnMouseEnter(const FGeometry& Geometry, const FPointerEvent& Event)
{
	SCompoundWidget::OnMouseEnter(Geometry, Event);
	if (IsLive()) FSlateApplication::Get().SetAllUserFocus(AsShared(), EFocusCause::Mouse);
}

// ---------------------------------------------------------------- screen

void SFightMenu::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&Plain)
		.BorderBackgroundColor(InArgs._Opaque ? Paper : FLinearColor(1.f, 1.f, 1.f, 0.9f))
		.Padding(FMargin(120.f, 80.f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Font(Font(52, true)).ColorAndOpacity(Ink).Text_Lambda([this] { return Title; })
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 6.f, 0.f, 36.f))
			[
				SNew(STextBlock).Font(Font(20)).ColorAndOpacity(Faded).AutoWrapText(true).Text_Lambda([this] { return Subtitle.Get(); })
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SBox).MaxDesiredWidth(980.f).HAlign(HAlign_Left)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(Rows, SVerticalBox)
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.f, 24.f, 0.f, 0.f))
			[
				SNew(STextBlock).Font(Font(16)).ColorAndOpacity(Faded)
				.Text(LOCTEXT("Hints", "A / Enter  select        B / Esc  back        Left / Right  change"))
			]
		]
	];
}

void SFightMenu::ShowPage(const FFightMenuPage& Page, bool bKeepFocus)
{
	int32 FocusIndex = 0;
	if (bKeepFocus)
		for (int32 i = 0; i < RowWidgets.Num(); ++i)
			if (RowWidgets[i]->HasAnyUserFocus().IsSet()) FocusIndex = i;

	Current = Page;
	Title = Page.Title;
	Subtitle = Page.Subtitle;
	Rows->ClearChildren();
	RowWidgets.Reset();
	for (const FFightMenuItem& Item : Page.Items)
	{
		TSharedRef<SFightMenuRow> Row = SNew(SFightMenuRow).Item(Item);
		RowWidgets.Add(Row);
		Rows->AddSlot().AutoHeight().Padding(FMargin(0.f, 2.f))[Row];
	}

	// land on the same line again, or the next one that can take focus
	for (int32 i = FocusIndex; i < RowWidgets.Num(); ++i)
		if (RowWidgets[i]->SupportsKeyboardFocus())
		{
			FSlateApplication::Get().SetAllUserFocus(RowWidgets[i], EFocusCause::SetDirectly);
			return;
		}
	FocusFirst();
}

void SFightMenu::FocusFirst()
{
	for (const TSharedRef<SFightMenuRow>& Row : RowWidgets)
		if (Row->SupportsKeyboardFocus())
		{
			FSlateApplication::Get().SetAllUserFocus(Row, EFocusCause::SetDirectly);
			return;
		}
	FSlateApplication::Get().SetAllUserFocus(AsShared(), EFocusCause::SetDirectly);
}

FReply SFightMenu::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (Current.OnBack && FSlateApplication::Get().GetNavigationActionFromKey(KeyEvent) == EUINavigationAction::Back)
	{
		const TFunction<void()> Back = Current.OnBack;
		Back();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SFightMenu::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	// a click on the empty white area would otherwise hand keyboard and pad to the game viewport
	for (const TSharedRef<SFightMenuRow>& Row : RowWidgets)
		if (Row->SupportsKeyboardFocus()) return FReply::Handled().SetUserFocus(Row, EFocusCause::Mouse);
	return FReply::Handled();
}

FReply SFightMenu::OnFocusReceived(const FGeometry& Geometry, const FFocusEvent& Event)
{
	// the screen itself got focus (clicked the background, input mode change): hand it to a row
	for (const TSharedRef<SFightMenuRow>& Row : RowWidgets)
		if (Row->SupportsKeyboardFocus()) return FReply::Handled().SetUserFocus(Row, EFocusCause::SetDirectly);
	return FReply::Handled();
}

// ---------------------------------------------------------------- pages

FFightMenus::FFightMenus(UWorld* InWorld, ERoot InRoot) : World(InWorld), Root(InRoot)
{
	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	Custom = S->Custom;
	LocalPlayers = FMath::Clamp(S->LocalPlayers, 1, 4);
	// start the character select where everyone left off
	Picks = User().LastLoadouts;
	for (int32 i = Picks.Num(); i < 4; ++i) Picks.Add(S->Loadouts.IsValidIndex(i) ? S->Loadouts[i] : FFightLoadout());
}

FFightMenus::~FFightMenus()
{
	Close();
}

UFightOnline* FFightMenus::Online() const
{
	return UFightOnline::Get(World.Get());
}

void FFightMenus::Open()
{
	UWorld* W = World.Get();
	if (bOpen || !W || !W->GetGameViewport()) return;
	if (!Widget) Widget = SNew(SFightMenu).Opaque(Root == ERoot::Title);
	W->GetGameViewport()->AddViewportWidgetContent(Widget.ToSharedRef(), 100);
	bOpen = true;

	Stack.Reset();
	switch (Root)
	{
	case ERoot::Title: Push([this] { return TitlePage(); }); break;
	case ERoot::Lobby: Push([this] { return LobbyPage(); }); break;
	case ERoot::Pause: Push([this] { return PausePage(); }); break;
	}

	if (APlayerController* PC = W->GetFirstPlayerController())
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Widget);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
	Widget->FocusFirst();
}

void FFightMenus::Close()
{
	if (!bOpen) return;
	bOpen = false;
	UWorld* W = World.Get();
	if (W && W->GetGameViewport() && Widget) W->GetGameViewport()->RemoveViewportWidgetContent(Widget.ToSharedRef());
	if (APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}

void FFightMenus::Push(FBuilder Builder)
{
	Stack.Add(MoveTemp(Builder));
	Show();
}

void FFightMenus::Pop()
{
	if (Stack.Num() > 1)
	{
		Stack.Pop();
		Show();
	}
	else if (Root == ERoot::Pause)
	{
		Close();
	}
}

void FFightMenus::Show(bool bKeepFocus)
{
	if (!Widget || Stack.Num() == 0) return;
	if (UFightOnline* O = Online()) PartyShown = O->GetPartyStatus() + (O->IsPartyLeader() ? TEXT("L") : TEXT(""));
	Widget->ShowPage(Stack.Last()(), bKeepFocus);
}

void FFightMenus::Refresh()
{
	Show(true);
}

void FFightMenus::Tick(float DeltaSeconds)
{
	if (!bOpen) return;

	// If the game viewport grabbed focus (a click, alt-tab, a controller waking up), give it back
	// to the menu. Only from the viewport, so editor panels around a PIE window stay usable.
	UWorld* W = World.Get();
	UGameViewportClient* Viewport = W ? W->GetGameViewport() : nullptr;
	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetUserFocusedWidget(0);
	if (Widget && Viewport && Focused.IsValid() && Focused == Viewport->GetGameViewportWidget() &&
		!Widget->HasAnyUserFocusOrFocusedDescendants())
		Widget->FocusFirst();

	UFightOnline* O = Online();
	if (!O) return;

	// party members follow their leader into matches
	FString Code;
	EFightMode Mode = EFightMode::Duel;
	if (Root == ERoot::Title && O->TakePartyMatch(Code, Mode))
	{
		FFightMatchRequest Request;
		Request.Mode = Mode;
		Request.Custom = Custom;
		Request.LocalPlayers = LocalPlayers;
		Request.Loadouts = Picks;
		Request.Net = EFightNetMode::Join;
		Request.JoinCode = Code;
		O->StartMatch(Request);
		return;
	}

	// rebuild when the party changes, the pages show different things for leader and members
	RefreshTimer -= DeltaSeconds;
	if (RefreshTimer > 0.f) return;
	RefreshTimer = 0.5f;
	const FString Party = O->GetPartyStatus() + (O->IsPartyLeader() ? TEXT("L") : TEXT(""));
	if (Party != PartyShown) Refresh();
}

void FFightMenus::StartMatch(int32 Mode, bool bOnline)
{
	UFightOnline* O = Online();
	if (!O) return;
	const UFightGameSettings* S = GetDefault<UFightGameSettings>();
	FFightMatchRequest Request;
	Request.Mode = static_cast<EFightMode>(Mode);
	Request.Custom = Custom;
	Request.LocalPlayers = LocalPlayers;
	Request.Loadouts = Picks;
	User().LastLoadouts = Picks;
	User().SaveSettings();
	if (!bOnline)
	{
		Request.Net = EFightNetMode::Offline;
	}
	else if (S->Backend == EFightNetBackend::DirectIp)
	{
		// LAN: whatever the project settings say (host, or join that address)
		Request.Net = S->NetMode == EFightNetMode::Join ? EFightNetMode::Join : EFightNetMode::Host;
		Request.JoinCode = S->HostAddress;
	}
	else if (O->IsPartyLeader())
	{
		// host so the party has a lobby to follow us into, strangers fill the rest
		Request.Net = EFightNetMode::Host;
		Request.bForParty = true;
	}
	else
	{
		Request.Net = EFightNetMode::QuickMatch;
	}
	O->StartMatch(Request);
}

FFightMenuPage FFightMenus::TitlePage()
{
	FFightMenuPage P;
	P.Title = LOCTEXT("GameTitle", "CARNAGE GAME");
	P.Subtitle = TAttribute<FText>::CreateLambda([this]
	{
		const UFightOnline* O = Online();
		return O && O->InParty() ? FText::FromString(O->GetPartyStatus()) : FText::GetEmpty();
	});
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Play", "Play"), [this] { Push([this] { return PlayPage(); }); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Settings", "Settings"), [this] { Push([this] { return SettingsPage(); }); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Exit", "Exit"), [this] { Push([this] { return QuitPage(); }); }));
	return P;
}

FFightMenuPage FFightMenus::PlayPage()
{
	FFightMenuPage P;
	P.Title = LOCTEXT("PlayTitle", "Play");
	P.OnBack = [this] { Pop(); };
	UFightOnline* O = Online();
	const bool bFollower = O && O->InParty() && !O->IsPartyLeader();
	P.Subtitle = O && O->InParty() ? FText::FromString(O->GetPartyStatus()) : FText::GetEmpty();

	if (bFollower)
	{
		P.Items.Add(FFightMenuItem::Info(LOCTEXT("Follower", "Your party leader picks the match, you'll join automatically.")));
	}
	else
	{
		for (int32 Mode = 0; Mode < ModeNames().Num(); ++Mode)
			P.Items.Add(FFightMenuItem::Button(ModeNames()[Mode], [this, Mode] { Push([this, Mode] { return ModePage(Mode); }); }));
	}
	const FText PartyLabel = O && O->InParty()
		? FText::Format(LOCTEXT("PartyN", "Party ({0})"), Num(O->GetPartySize()))
		: LOCTEXT("PlayWithFriend", "Play with a friend");
	P.Items.Add(FFightMenuItem::Button(PartyLabel, [this] { Push([this] { return PartyPage(); }); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Back", "Back"), [this] { Pop(); }));
	return P;
}

FFightMenuPage FFightMenus::ModePage(int32 Mode)
{
	static const TArray<FText> About = {
		LOCTEXT("AboutDuel", "One on one."),
		LOCTEXT("AboutTeams", "Two teams of two."),
		LOCTEXT("AboutBrawl", "An open colony for up to 20. No time limit, no lives, just fight."),
		LOCTEXT("AboutCustom", "Your rules: up to 20 fighters, custom teams and objectives."),
	};
	FFightMenuPage P;
	P.Title = ModeNames()[Mode];
	P.Subtitle = About[Mode];
	P.OnBack = [this] { Pop(); };

	const int32 MaxLocal = Mode == static_cast<int32>(EFightMode::Duel) ? 2 : 4;
	LocalPlayers = FMath::Min(LocalPlayers, MaxLocal);
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("LocalPlayers", "Players on this console"), NumberOptions(1, MaxLocal),
		[this] { return LocalPlayers - 1; }, [this](int32 I) { LocalPlayers = I + 1; }));
	if (Mode == static_cast<int32>(EFightMode::Custom))
		P.Items.Add(FFightMenuItem::Button(LOCTEXT("Rules", "Rules"), [this] { Push([this] { return CustomRulesPage(); }); }));

	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Offline", "Offline  -  local players and bots"), [this, Mode] { PickCharacters(Mode, false); }));
	const UFightOnline* O = Online();
	const FText OnlineLabel = O && O->IsPartyLeader()
		? LOCTEXT("OnlineParty", "Online  -  with your party")
		: LOCTEXT("Online", "Online  -  find players");
	P.Items.Add(FFightMenuItem::Button(OnlineLabel, [this, Mode] { PickCharacters(Mode, true); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Back", "Back"), [this] { Pop(); }));
	return P;
}

FFightMenuPage FFightMenus::CustomRulesPage()
{
	static const TArray<FText> Objectives = { LOCTEXT("ObjNone", "Free fight"), LOCTEXT("ObjElim", "Elimination"),
		LOCTEXT("ObjDm", "Deathmatch"), LOCTEXT("ObjCtf", "Capture the flag"), LOCTEXT("ObjOdd", "Oddball"), LOCTEXT("ObjCp", "Checkpoints") };
	static const TArray<FText> Maps = { LOCTEXT("MapShrine", "Shrine Ward (small)"), LOCTEXT("MapHarbor", "Harbor District"),
		LOCTEXT("MapCapital", "Old Capital (huge)"), LOCTEXT("MapTemple", "Temple Steps"), LOCTEXT("MapDowntown", "Shattered Downtown"),
		LOCTEXT("MapLake", "Frozen Lake") };
	static const TArray<int32> Scores = { 10, 20, 30, 50, 100, 0 };
	static const TArray<int32> Minutes = { 5, 10, 15, 20, 30, 0 };
	static const TArray<int32> Respawns = { 3, 5, 10 };

	auto Labels = [](const TArray<int32>& Values, const FText& Zero)
	{
		TArray<FText> Out;
		for (int32 V : Values) Out.Add(V == 0 ? Zero : Num(V));
		return Out;
	};
	auto IndexOf = [](const TArray<int32>& Values, int32 V) { return FMath::Max(0, Values.IndexOfByKey(V)); };

	FFightMenuPage P;
	P.Title = LOCTEXT("RulesTitle", "Custom rules");
	P.OnBack = [this] { Pop(); };
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("Fighters", "Fighters"), NumberOptions(2, 20),
		[this] { return Custom.Fighters - 2; }, [this](int32 I) { Custom.Fighters = I + 2; }));
	TArray<FText> TeamOptions = { LOCTEXT("FFA", "Everyone for themselves") };
	for (int32 T = 2; T <= 10; ++T) TeamOptions.Add(FText::Format(LOCTEXT("NTeams", "{0} teams"), Num(T)));
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("Teams", "Teams"), TeamOptions,
		[this] { return Custom.Teams < 2 ? 0 : Custom.Teams - 1; }, [this](int32 I) { Custom.Teams = I == 0 ? 0 : I + 1; }));
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("Objective", "Objective"), Objectives,
		[this] { return static_cast<int32>(Custom.Objective); }, [this](int32 I) { Custom.Objective = static_cast<EFightObjective>(I); }));
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("Map", "Map"), Maps,
		[this] { return static_cast<int32>(Custom.Map); }, [this](int32 I) { Custom.Map = static_cast<EFightMap>(I); }));
	P.Items.Add(FFightMenuItem::Toggle(LOCTEXT("FriendlyFire", "Friendly fire"),
		[this] { return Custom.bFriendlyFire; }, [this](bool B) { Custom.bFriendlyFire = B; }));
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("ScoreLimit", "Score limit"), Labels(Scores, LOCTEXT("NoLimit", "No limit")),
		[this, IndexOf] { return IndexOf(Scores, Custom.ScoreLimit); }, [this](int32 I) { Custom.ScoreLimit = Scores[I]; }));
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("TimeLimit", "Time limit (minutes)"), Labels(Minutes, LOCTEXT("NoLimit", "No limit")),
		[this, IndexOf] { return IndexOf(Minutes, Custom.TimeLimitMinutes); }, [this](int32 I) { Custom.TimeLimitMinutes = Minutes[I]; }));
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("Respawn", "Respawn (seconds)"), Labels(Respawns, FText::GetEmpty()),
		[this, IndexOf] { return IndexOf(Respawns, Custom.RespawnSeconds); }, [this](int32 I) { Custom.RespawnSeconds = Respawns[I]; }));
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("CursedSlots", "Cursed objects per player"), NumberOptions(0, 5),
		[this] { return Custom.CursedObjectSlots; }, [this](int32 I) { Custom.CursedObjectSlots = I; }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Done", "Done"), [this] { Pop(); }));
	return P;
}

FFightMenuPage FFightMenus::PartyPage()
{
	FFightMenuPage P;
	P.Title = LOCTEXT("PartyTitle", "Play with a friend");
	P.OnBack = [this] { Pop(); };
	UFightOnline* O = Online();
	if (!O) return P;

	if (!O->InParty())
	{
		P.Subtitle = LOCTEXT("PartyAbout", "Team up first, then pick a mode under Play. Your friend comes along.");
		P.Items.Add(FFightMenuItem::Button(LOCTEXT("Invite", "Invite a friend  -  get a code"), [this]
		{
			if (UFightOnline* On = Online()) On->CreateParty();
			Refresh();
		}));
		P.Items.Add(FFightMenuItem::Text(LOCTEXT("FriendCode", "Friend's code"),
			[this] { return PartyCodeInput; }, [this](const FString& T) { PartyCodeInput = T.TrimStartAndEnd(); }));
		P.Items.Add(FFightMenuItem::Button(LOCTEXT("JoinFriend", "Join friend"), [this]
		{
			if (UFightOnline* On = Online()) On->JoinParty(PartyCodeInput);
			Refresh();
		}, TAttribute<bool>::CreateLambda([this] { return !PartyCodeInput.IsEmpty(); })));
	}
	else
	{
		P.Subtitle = TAttribute<FText>::CreateLambda([this]
		{
			const UFightOnline* On = Online();
			return On ? FText::FromString(On->GetPartyStatus()) : FText::GetEmpty();
		});
		P.Items.Add(FFightMenuItem::Info(TAttribute<FText>::CreateLambda([this]
		{
			const UFightOnline* On = Online();
			const FString Code = On ? On->GetPartyCode() : FString();
			return Code.IsEmpty() ? LOCTEXT("NoCodeYet", "Getting a code ...")
				: FText::Format(LOCTEXT("PartyCode", "Code: {0}"), FText::FromString(Code));
		})));
		P.Items.Add(FFightMenuItem::Button(LOCTEXT("CopyCode", "Copy code"), [this]
		{
			if (const UFightOnline* On = Online()) FPlatformApplicationMisc::ClipboardCopy(*On->GetPartyCode());
		}, TAttribute<bool>::CreateLambda([this] { const UFightOnline* On = Online(); return On && !On->GetPartyCode().IsEmpty(); })));
		P.Items.Add(FFightMenuItem::Info(O->IsPartyLeader()
			? LOCTEXT("LeaderHint", "You lead. Pick a mode under Play and choose Online, your party joins you.")
			: LOCTEXT("MemberHint", "Waiting for the leader to start a match.")));
		P.Items.Add(FFightMenuItem::Button(LOCTEXT("LeaveParty", "Leave party"), [this]
		{
			if (UFightOnline* On = Online()) On->LeaveParty();
			Refresh();
		}));
	}
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Back", "Back"), [this] { Pop(); }));
	return P;
}

FFightMenuPage FFightMenus::SettingsPage()
{
	FFightMenuPage P;
	P.Title = LOCTEXT("SettingsTitle", "Settings");
	P.OnBack = [this] { Pop(); };
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Display", "Display"), [this] { Push([this] { return DisplayPage(); }); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Graphics", "Graphics"), [this] { Push([this] { return GraphicsPage(); }); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Audio", "Audio"), [this] { Push([this] { return AudioPage(); }); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Controls", "Controls"), [this] { Push([this] { return ControlsPage(); }); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("OnlineSettings", "Online"), [this] { Push([this] { return OnlinePage(); }); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Reset", "Reset everything to defaults"), [this]
	{
		User().SetToDefaults();
		User().ApplySettings(false);
		User().SaveSettings();
		Refresh();
	}));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Back", "Back"), [this] { Pop(); }));
	return P;
}

FFightMenuPage FFightMenus::DisplayPage()
{
	static const TArray<EWindowMode::Type> WindowModes = { EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed };
	static const TArray<float> FrameLimits = { 30.f, 60.f, 90.f, 120.f, 144.f, 0.f };

	Resolutions.Reset();
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
	if (Resolutions.Num() == 0) Resolutions.Add(User().GetScreenResolution());
	TArray<FText> ResolutionNames;
	for (const FIntPoint& R : Resolutions) ResolutionNames.Add(FText::FromString(FString::Printf(TEXT("%d x %d"), R.X, R.Y)));
	TArray<FText> LimitNames;
	for (float L : FrameLimits) LimitNames.Add(L <= 0.f ? LOCTEXT("Unlimited", "Unlimited") : Num(FMath::RoundToInt(L)));

	FFightMenuPage P;
	P.Title = LOCTEXT("DisplayTitle", "Display");
	P.Subtitle = LOCTEXT("DisplayHint", "Window mode and resolution change when you pick Apply.");
	P.OnBack = [this] { Pop(); };
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("WindowMode", "Window mode"),
		{ LOCTEXT("Fullscreen", "Fullscreen"), LOCTEXT("Borderless", "Borderless window"), LOCTEXT("Windowed", "Window") },
		[] { return FMath::Max(0, WindowModes.IndexOfByKey(User().GetFullscreenMode())); },
		[](int32 I) { User().SetFullscreenMode(WindowModes[I]); }));
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("Resolution", "Resolution"), ResolutionNames,
		[this] { return FMath::Max(0, Resolutions.IndexOfByKey(User().GetScreenResolution())); },
		[this](int32 I) { User().SetScreenResolution(Resolutions[I]); }));
	P.Items.Add(FFightMenuItem::Toggle(LOCTEXT("VSync", "V-Sync"),
		[] { return User().IsVSyncEnabled(); }, [](bool B) { User().SetVSyncEnabled(B); Commit(); }));
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("FrameLimit", "Frame rate limit"), LimitNames,
		[] { return FMath::Max(0, FrameLimits.IndexOfByKey(User().GetFrameRateLimit())); },
		[](int32 I) { User().SetFrameRateLimit(FrameLimits[I]); Commit(); }));
	FFightMenuItem Scale = FFightMenuItem::Slider(LOCTEXT("RenderScale", "Render scale"),
		[]
		{
			float Normalized = 1.f, Value = 100.f, Min = 0.f, Max = 100.f;
			User().GetResolutionScaleInformationEx(Normalized, Value, Min, Max);
			return Normalized;
		},
		[](float V) { User().SetResolutionScaleNormalized(V); Commit(); }, 0.1f);
	P.Items.Add(Scale);
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Apply", "Apply"), []
	{
		User().ApplyResolutionSettings(false);
		User().ConfirmVideoMode();
		User().SaveSettings();
	}));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Back", "Back"), [this] { Pop(); }));
	return P;
}

FFightMenuPage FFightMenus::GraphicsPage()
{
	const TArray<FText> Levels = { LOCTEXT("Low", "Low"), LOCTEXT("Medium", "Medium"), LOCTEXT("High", "High"), LOCTEXT("Epic", "Epic") };
	auto Quality = [&Levels](const FText& Label, int32 (UGameUserSettings::*Get)() const, void (UGameUserSettings::*Set)(int32))
	{
		return FFightMenuItem::Choice(Label, Levels,
			[Get] { return FMath::Clamp((User().*Get)(), 0, 3); },
			[Set](int32 I) { (User().*Set)(I); Commit(); });
	};

	FFightMenuPage P;
	P.Title = LOCTEXT("GraphicsTitle", "Graphics");
	P.Subtitle = LOCTEXT("GraphicsHint", "Handhelds run best on Low or Medium.");
	P.OnBack = [this] { Pop(); };
	TArray<FText> Presets = Levels;
	Presets.Add(LOCTEXT("CustomPreset", "Custom"));
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("Preset", "Quality preset"), Presets,
		[] { const int32 L = User().GetOverallScalabilityLevel(); return L < 0 ? 4 : FMath::Min(L, 3); },
		[this](int32 I)
		{
			if (I > 3) return; // "custom" is what you get by changing single values
			User().SetOverallScalabilityLevel(I);
			Commit();
			Refresh();
		}));
	P.Items.Add(Quality(LOCTEXT("ViewDistance", "View distance"), &UGameUserSettings::GetViewDistanceQuality, &UGameUserSettings::SetViewDistanceQuality));
	P.Items.Add(Quality(LOCTEXT("AntiAliasing", "Anti-aliasing"), &UGameUserSettings::GetAntiAliasingQuality, &UGameUserSettings::SetAntiAliasingQuality));
	P.Items.Add(Quality(LOCTEXT("Shadows", "Shadows"), &UGameUserSettings::GetShadowQuality, &UGameUserSettings::SetShadowQuality));
	P.Items.Add(Quality(LOCTEXT("PostProcess", "Post processing"), &UGameUserSettings::GetPostProcessingQuality, &UGameUserSettings::SetPostProcessingQuality));
	P.Items.Add(Quality(LOCTEXT("Textures", "Textures"), &UGameUserSettings::GetTextureQuality, &UGameUserSettings::SetTextureQuality));
	P.Items.Add(Quality(LOCTEXT("Effects", "Effects"), &UGameUserSettings::GetVisualEffectQuality, &UGameUserSettings::SetVisualEffectQuality));
	P.Items.Add(Quality(LOCTEXT("Foliage", "Foliage"), &UGameUserSettings::GetFoliageQuality, &UGameUserSettings::SetFoliageQuality));
	P.Items.Add(Quality(LOCTEXT("Shading", "Shading"), &UGameUserSettings::GetShadingQuality, &UGameUserSettings::SetShadingQuality));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("AutoDetect", "Detect best settings"), [this]
	{
		User().RunHardwareBenchmark();
		User().ApplyHardwareBenchmarkResults();
		User().SaveSettings();
		Refresh();
	}));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Back", "Back"), [this] { Pop(); }));
	return P;
}

FFightMenuPage FFightMenus::AudioPage()
{
	auto Volume = [](const FText& Label, float UFightUserSettings::*Field)
	{
		return FFightMenuItem::Slider(Label, [Field] { return User().*Field; }, [Field](float V) { User().*Field = V; Commit(); });
	};
	FFightMenuPage P;
	P.Title = LOCTEXT("AudioTitle", "Audio");
	P.OnBack = [this] { Pop(); };
	P.Items.Add(Volume(LOCTEXT("Master", "Master"), &UFightUserSettings::MasterVolume));
	P.Items.Add(Volume(LOCTEXT("Music", "Music"), &UFightUserSettings::MusicVolume));
	P.Items.Add(Volume(LOCTEXT("SoundEffects", "Effects"), &UFightUserSettings::EffectsVolume));
	P.Items.Add(Volume(LOCTEXT("Voice", "Voice"), &UFightUserSettings::VoiceVolume));
	P.Items.Add(FFightMenuItem::Toggle(LOCTEXT("MuteBackground", "Mute when the game is in the background"),
		[] { return User().bMuteInBackground; }, [](bool B) { User().bMuteInBackground = B; Commit(); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Back", "Back"), [this] { Pop(); }));
	return P;
}

FFightMenuPage FFightMenus::ControlsPage()
{
	FFightMenuPage P;
	P.Title = LOCTEXT("ControlsTitle", "Controls");
	P.OnBack = [this] { Pop(); };

	FFightMenuItem Deadzone = FFightMenuItem::Slider(LOCTEXT("Deadzone", "Stick deadzone"),
		[] { return (User().StickDeadzone - 0.05f) / 0.85f; },
		[](float V) { User().StickDeadzone = 0.05f + V * 0.85f; Commit(); });
	Deadzone.FormatValue = [](float V) { return FText::AsNumber(FMath::RoundToInt((0.05f + V * 0.85f) * 100.f)); };
	P.Items.Add(Deadzone);

	FFightMenuItem Camera = FFightMenuItem::Slider(LOCTEXT("CameraSpeed", "Camera speed"),
		[] { return (User().CameraTurnSpeed - 30.f) / 690.f; },
		[](float V) { User().CameraTurnSpeed = 30.f + V * 690.f; Commit(); });
	Camera.FormatValue = [](float V) { return FText::AsNumber(FMath::RoundToInt(30.f + V * 690.f)); };
	P.Items.Add(Camera);

	P.Items.Add(FFightMenuItem::Toggle(LOCTEXT("InvertX", "Invert camera left / right"),
		[] { return User().bInvertCameraX; }, [](bool B) { User().bInvertCameraX = B; Commit(); }));
	P.Items.Add(FFightMenuItem::Toggle(LOCTEXT("Vibration", "Vibration"),
		[] { return User().bVibration; }, [](bool B) { User().bVibration = B; Commit(); }));
	P.Items.Add(FFightMenuItem::Info(LOCTEXT("Remap", "Button remapping comes later. For now: Project Settings > Game > Fight Game.")));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Back", "Back"), [this] { Pop(); }));
	return P;
}

FFightMenuPage FFightMenus::OnlinePage()
{
	FFightMenuPage P;
	P.Title = LOCTEXT("OnlineTitle", "Online");
	P.Subtitle = TAttribute<FText>::CreateLambda([this]
	{
		const UFightOnline* O = Online();
		return O ? FText::FromString(O->GetOnlineStatus()) : FText::GetEmpty();
	});
	P.OnBack = [this] { Pop(); };
	P.Items.Add(FFightMenuItem::Text(LOCTEXT("Name", "Name"),
		[] { return User().DisplayName; },
		[](const FString& T) { User().DisplayName = T.Left(32); User().SaveSettings(); }));
	TArray<FText> Delays;
	for (int32 D = 0; D <= 6; ++D) Delays.Add(FText::Format(LOCTEXT("Frames", "{0} frames"), Num(D)));
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("InputDelay", "Input delay"), Delays,
		[] { return FMath::Clamp(User().InputDelay, 0, 6); }, [](int32 I) { User().InputDelay = I; User().SaveSettings(); }));
	P.Items.Add(FFightMenuItem::Info(LOCTEXT("DelayHint", "More delay = fewer corrections on a bad connection. 2 is a good start.")));
	P.Items.Add(FFightMenuItem::Toggle(LOCTEXT("FrameInfo", "Show frame and rollback counter"),
		[] { return User().bShowFrameInfo; }, [](bool B) { User().bShowFrameInfo = B; User().SaveSettings(); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Back", "Back"), [this] { Pop(); }));
	return P;
}

// ---------------------------------------------------------------- character select

namespace
{
	struct FBodyInfo
	{
		EFightBody Body;
		const fc::BodyDef& (*Def)();
		const TCHAR* About;
	};

	const TArray<FBodyInfo>& Bodies()
	{
		static const TArray<FBodyInfo> List = {
			{ EFightBody::Brawler, &fc::bodies::brawler, TEXT("All-rounder. Good at everything, best at nothing.") },
			{ EFightBody::Heavy, &fc::bodies::heavy, TEXT("Tanky and hits hard, slow on its feet.") },
			{ EFightBody::Speedster, &fc::bodies::speedster, TEXT("Fast and slippery, can't take much.") },
			{ EFightBody::Caster, &fc::bodies::caster, TEXT("Huge CE pool, built around the technique.") },
			{ EFightBody::Technician, &fc::bodies::technician, TEXT("Long reach and precise, rewards spacing.") },
			{ EFightBody::Summoner, &fc::bodies::summoner, TEXT("Fights together with a familiar.") },
			{ EFightBody::HeavensBind, &fc::bodies::heavensBind, TEXT("No CE at all. A monster body with cursed tools instead.") },
		};
		return List;
	}

	int32 BodyIndex(EFightBody Body)
	{
		return FMath::Max(0, Bodies().IndexOfByPredicate([Body](const FBodyInfo& B) { return B.Body == Body; }));
	}

	FString Pips(int32 Value)
	{
		FString Out;
		for (int32 i = 0; i < 4; ++i) Out += i < Value ? TEXT("#") : TEXT("-");
		return Out;
	}

	// techniques a player can pick: everything loaded, mods included, minus the Heaven's Bind tools
	TArray<const fc::TechniqueDef*> PickableTechniques()
	{
		TArray<const fc::TechniqueDef*> Out;
		for (const fc::TechniqueDef* T : fc::Registry::instance().techniques())
			if (T && !T->ceFree) Out.Add(T);
		return Out;
	}

	fc::GameMode CoreMode(int32 Mode)
	{
		switch (static_cast<EFightMode>(Mode))
		{
		case EFightMode::Teams: return fc::GameMode::Teams;
		case EFightMode::Brawl: return fc::GameMode::Brawl;
		case EFightMode::Custom: return fc::GameMode::Custom;
		default: return fc::GameMode::Duel;
		}
	}
}

void FFightMenus::PickCharacters(int32 Mode, bool bOnline)
{
	AFightDirector::LoadMods(); // modded techniques and cursed objects belong in the lists
	PendingMode = Mode;
	bPendingOnline = bOnline;
	Push([this] { return CharacterPage(0); });
}

FFightMenuPage FFightMenus::CharacterPage(int32 Player)
{
	static const TArray<FText> Assists = { LOCTEXT("BladeWolf", "Blade Wolf  (striker)"), LOCTEXT("SparkOwl", "Spark Owl  (zoner)"),
		LOCTEXT("VineToad", "Vine Toad  (support)"), LOCTEXT("StoneWarden", "Stone Warden  (guard)") };

	FFightLoadout& Pick = Picks[Player];
	const TArray<const fc::TechniqueDef*> Techniques = PickableTechniques();
	const bool bCustom = PendingMode == static_cast<int32>(EFightMode::Custom);
	const int32 CursedSlots = bCustom ? Custom.CursedObjectSlots : fc::rulesFor(CoreMode(PendingMode)).cursedObjectSlots;
	const int32 Teams = bCustom ? Custom.Teams : fc::rulesFor(CoreMode(PendingMode)).teamCount;

	FFightMenuPage P;
	P.Title = LocalPlayers > 1 ? FText::Format(LOCTEXT("PickN", "Player {0}"), Num(Player + 1)) : LOCTEXT("PickOne", "Choose your fighter");
	P.Subtitle = FText::Format(LOCTEXT("PickMode", "{0}  -  {1}"), ModeNames()[PendingMode],
		bPendingOnline ? LOCTEXT("PickOnline", "online") : LOCTEXT("PickOffline", "offline"));
	P.OnBack = [this] { Pop(); };

	// body
	TArray<FText> BodyNames;
	for (const FBodyInfo& B : Bodies()) BodyNames.Add(FText::FromString(UTF8_TO_TCHAR(B.Def().name)));
	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("Body", "Body"), BodyNames,
		[&Pick] { return BodyIndex(Pick.Body); },
		[this, &Pick](int32 I)
		{
			Pick.Body = Bodies()[I].Body;
			Refresh(); // Heaven's Bind and Summoner change which lines exist
		}));
	P.Items.Add(FFightMenuItem::Info(TAttribute<FText>::CreateLambda([&Pick]
	{
		const FBodyInfo& B = Bodies()[BodyIndex(Pick.Body)];
		const fc::BodyDef& D = B.Def();
		return FText::FromString(FString::Printf(TEXT("%s\nHP %s   Speed %s   CE %s   Range %s"),
			B.About, *Pips(D.hp), *Pips(D.speed), *Pips(D.ce), *Pips(D.range)));
	})));

	// technique
	if (Pick.Body == EFightBody::HeavensBind)
	{
		P.Items.Add(FFightMenuItem::Info(LOCTEXT("ToolsOnly", "Technique: Tool Arsenal  -  Heaven's Bind fights with cursed tools, no technique.")));
	}
	else if (Techniques.Num() > 0)
	{
		TArray<FText> TechniqueNames;
		for (const fc::TechniqueDef* T : Techniques) TechniqueNames.Add(FText::FromString(UTF8_TO_TCHAR(T->name)));
		P.Items.Add(FFightMenuItem::Choice(LOCTEXT("Technique", "Technique"), TechniqueNames,
			[&Pick, Techniques]
			{
				// the pick is stored by id, built-in and modded techniques alike
				const FString Id = AFightDirector::TechniqueIdOf(Pick);
				return FMath::Max(0, Techniques.IndexOfByPredicate([&Id](const fc::TechniqueDef* T) { return Id == UTF8_TO_TCHAR(T->id); }));
			},
			[&Pick, Techniques](int32 I) { Pick.ModTechnique = FName(UTF8_TO_TCHAR(Techniques[I]->id)); }));
	}

	// the summoner's familiar
	if (Pick.Body == EFightBody::Summoner)
		P.Items.Add(FFightMenuItem::Choice(LOCTEXT("Familiar", "Familiar"), Assists,
			[&Pick] { return static_cast<int32>(Pick.Assist); },
			[&Pick](int32 I) { Pick.Assist = static_cast<EFightAssist>(I); }));

	// team, where the mode has teams
	if (Teams > 1)
	{
		TArray<FText> TeamNames;
		for (int32 T = 0; T < Teams; ++T) TeamNames.Add(FText::Format(LOCTEXT("TeamN", "Team {0}"), Num(T + 1)));
		P.Items.Add(FFightMenuItem::Choice(LOCTEXT("Team", "Team"), TeamNames,
			[&Pick, Teams] { return FMath::Clamp(Pick.Team, 0, Teams - 1); },
			[&Pick](int32 I) { Pick.Team = I; }));
	}

	// cursed objects, as many as the mode allows
	TArray<const fc::CursedObjectDef*> Cursed;
	for (const fc::CursedObjectDef* C : fc::Registry::instance().cursedObjects()) Cursed.Add(C);
	if (CursedSlots > 0 && Cursed.Num() > 0)
	{
		TArray<FText> CursedNames = { LOCTEXT("NoCursed", "None") };
		for (const fc::CursedObjectDef* C : Cursed) CursedNames.Add(FText::FromString(UTF8_TO_TCHAR(C->name)));
		Pick.CursedObjects.SetNum(FMath::Min(Pick.CursedObjects.Num(), CursedSlots));
		for (int32 Slot = 0; Slot < CursedSlots; ++Slot)
		{
			P.Items.Add(FFightMenuItem::Choice(FText::Format(LOCTEXT("CursedSlot", "Cursed object {0}"), Num(Slot + 1)), CursedNames,
				[&Pick, Cursed, Slot]
				{
					if (!Pick.CursedObjects.IsValidIndex(Slot)) return 0;
					const FString Id = Pick.CursedObjects[Slot].ToString();
					return 1 + Cursed.IndexOfByPredicate([&Id](const fc::CursedObjectDef* C) { return Id == UTF8_TO_TCHAR(C->id); });
				},
				[&Pick, Cursed, Slot](int32 I)
				{
					if (Pick.CursedObjects.Num() <= Slot) Pick.CursedObjects.SetNum(Slot + 1);
					Pick.CursedObjects[Slot] = I == 0 ? NAME_None : FName(UTF8_TO_TCHAR(Cursed[I - 1]->id));
				}));
		}
		P.Items.Add(FFightMenuItem::Info(TAttribute<FText>::CreateLambda([&Pick, Cursed]
		{
			FString Text;
			for (const FName& Id : Pick.CursedObjects)
				for (const fc::CursedObjectDef* C : Cursed)
					if (!Id.IsNone() && Id.ToString() == UTF8_TO_TCHAR(C->id))
						Text += FString::Printf(TEXT("%s: %s\n"), UTF8_TO_TCHAR(C->name), UTF8_TO_TCHAR(C->description));
			return FText::FromString(Text.TrimEnd());
		})));
	}

	P.Items.Add(FFightMenuItem::Choice(LOCTEXT("BodyType", "Body type"), { LOCTEXT("Male", "Male"), LOCTEXT("Female", "Female") },
		[&Pick] { return static_cast<int32>(Pick.BodyType); },
		[&Pick](int32 I) { Pick.BodyType = static_cast<EFightBodyType>(I); }));

	const bool bLast = Player + 1 >= LocalPlayers;
	P.Items.Add(FFightMenuItem::Button(bLast ? LOCTEXT("Fight", "Ready  -  fight!") : LOCTEXT("NextPlayer", "Ready  -  next player"), [this, Player, bLast]
	{
		if (bLast) StartMatch(PendingMode, bPendingOnline);
		else Push([this, Player] { return CharacterPage(Player + 1); });
	}));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Back", "Back"), [this] { Pop(); }));
	return P;
}

FFightMenuPage FFightMenus::QuitPage()
{
	FFightMenuPage P;
	P.Title = LOCTEXT("QuitTitle", "Quit?");
	P.OnBack = [this] { Pop(); };
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Cancel", "Cancel"), [this] { Pop(); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Quit", "Quit"), [this]
	{
		UWorld* W = World.Get();
		UKismetSystemLibrary::QuitGame(W, W ? W->GetFirstPlayerController() : nullptr, EQuitPreference::Quit, false);
	}));
	return P;
}

FFightMenuPage FFightMenus::LobbyPage()
{
	FFightMenuPage P;
	P.Title = LOCTEXT("LobbyTitle", "Finding players");
	P.Subtitle = TAttribute<FText>::CreateLambda([this]
	{
		const AFightDirector* D = Director.Get();
		return D ? FText::FromString(D->GetNetStatus()) : FText::GetEmpty();
	});
	auto Leave = [this] { if (UFightOnline* O = Online()) O->ReturnToMenu(); };
	P.OnBack = Leave;

	P.Items.Add(FFightMenuItem::Info(TAttribute<FText>::CreateLambda([this]
	{
		const AFightDirector* D = Director.Get();
		if (!D || D->GetLobbyTarget() == 0) return FText::GetEmpty();
		return FText::Format(LOCTEXT("Players", "Players: {0} / {1}"), Num(D->GetLobbyPlayers()), Num(D->GetLobbyTarget()));
	})));
	P.Items.Add(FFightMenuItem::Info(TAttribute<FText>::CreateLambda([this]
	{
		const AFightDirector* D = Director.Get();
		return D ? FText::Format(LOCTEXT("Waiting", "Waiting {0}"), Clock(D->GetWaitSeconds())) : FText::GetEmpty();
	})));
	P.Items.Add(FFightMenuItem::Button(
		TAttribute<FText>::CreateLambda([this]
		{
			const AFightDirector* D = Director.Get();
			if (D && D->CanFillWithBots())
				return D->IsLobbyHost() ? LOCTEXT("FillBots", "Fill lobby with bots and start")
				                        : LOCTEXT("PlayBots", "Leave and play against bots");
			const float Left = D ? D->GetBotOfferSeconds() - D->GetWaitSeconds() : 0.f;
			return FText::Format(LOCTEXT("BotsIn", "Bots can fill the lobby in {0}"), Clock(Left));
		}),
		[this] { if (AFightDirector* D = Director.Get()) D->FillWithBots(); },
		TAttribute<bool>::CreateLambda([this] { const AFightDirector* D = Director.Get(); return D && D->CanFillWithBots(); })));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("CancelSearch", "Cancel"), Leave));
	return P;
}

FFightMenuPage FFightMenus::PausePage()
{
	FFightMenuPage P;
	P.Title = LOCTEXT("PauseTitle", "Paused");
	const AFightDirector* D = Director.Get();
	P.Subtitle = D && D->IsOnline() ? LOCTEXT("StillRunning", "Online matches keep running while this is open.") : FText::GetEmpty();
	P.OnBack = [this] { Close(); };
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Resume", "Resume"), [this] { Close(); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("Settings", "Settings"), [this] { Push([this] { return SettingsPage(); }); }));
	P.Items.Add(FFightMenuItem::Button(LOCTEXT("LeaveMatch", "Leave match"), [this] { if (UFightOnline* O = Online()) O->ReturnToMenu(); }));
	return P;
}

#undef LOCTEXT_NAMESPACE
