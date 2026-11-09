#include "FightAppearance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "FightProfile.h"
#include "fightcore/Registry.h"

namespace
{
	const FName OutfitTag(TEXT("Outfit"));

	USkeletalMesh* LoadMesh(FString Path)
	{
		if (Path.IsEmpty()) return nullptr;
		// "/Game/Outfits/Coat/Coat_M" -> "/Game/Outfits/Coat/Coat_M.Coat_M"
		if (!Path.Contains(TEXT(".")))
		{
			FString Folder, Name;
			Path.Split(TEXT("/"), &Folder, &Name, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			Path += TEXT(".") + Name;
		}
		return Cast<USkeletalMesh>(FSoftObjectPath(Path).TryLoad());
	}

	USkeletalMeshComponent* AddPiece(AActor* Target, USceneComponent* Parent, USkeletalMesh* Mesh)
	{
		USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(Target);
		C->ComponentTags.Add(OutfitTag);
		C->SetSkeletalMeshAsset(Mesh);
		C->SetupAttachment(Parent);
		C->RegisterComponent();
		// limited animation: poses update 12 times a second and hold in between, like drawn animation.
		// The simulation still runs at 60 Hz, only the look is stepped.
		C->SetComponentTickInterval(1.f / 12.f);
		return C;
	}
}

void UFightAppearance::Apply(AActor* Target, const FFightLoadout& Loadout)
{
	if (!Target) return;

	TArray<USkeletalMeshComponent*> Existing;
	Target->GetComponents(Existing);
	for (USkeletalMeshComponent* C : Existing)
		if (C->ComponentHasTag(OutfitTag)) C->DestroyComponent();

	const bool bFemale = Loadout.BodyType == EFightBodyType::Female;
	USkeletalMesh* BodyMesh = LoadMesh(bFemale ? TEXT("/Game/Characters/Body_Female") : TEXT("/Game/Characters/Body_Male"));
	if (!BodyMesh) return;

	USkeletalMeshComponent* Body = AddPiece(Target, Target->GetRootComponent(), BodyMesh);
	const UFightProfile* Profile = UFightProfile::Load();
	for (const FFightOutfitPick& Pick : Loadout.Outfit)
	{
		const fc::OutfitDef* Item = fc::Registry::instance().outfit(TCHAR_TO_UTF8(*Pick.Item.ToString()));
		if (!Item || !Profile->IsUnlocked(Pick.Item)) continue;
		USkeletalMesh* Mesh = LoadMesh(UTF8_TO_TCHAR(Pick.bFeminineCut ? Item->meshFeminine : Item->meshMasculine));
		if (!Mesh) continue;
		AddPiece(Target, Body, Mesh)->SetLeaderPoseComponent(Body);
	}
}
