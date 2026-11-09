#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "FightShapeVisual.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

UENUM()
enum class EFightShape : uint8 { Cube, Sphere, Cylinder };

// Stand-in visuals made from the engine's basic shapes until real models and the toon shader exist.
UCLASS()
class FIGHTGAME_API AFightShapeVisual : public AActor
{
	GENERATED_BODY()

public:
	AFightShapeVisual();

	void SetShape(EFightShape Shape);
	void SetColor(const FLinearColor& Color);
	// size in cm, the engine's basic shapes are 100 cm
	void SetSizeCm(const FVector& Size);
	void SetVisible(bool bVisible);
	void SetMeshOffset(const FVector& OffsetCm);

	UStaticMeshComponent* AddMarker(EFightShape Shape, const FVector& RelativeLocation, const FVector& SizeCm,
		const FLinearColor& Color);

private:
	UStaticMesh* MeshFor(EFightShape Shape) const;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
	UPROPERTY() TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;

	// only push changes to the GPU
	FLinearColor CurrentColor = FLinearColor::Transparent;
	FVector CurrentSize = FVector::ZeroVector;
};
