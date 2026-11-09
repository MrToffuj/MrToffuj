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

// Platzhalter-Optik (Engine-Grundformen), bis echte Modelle/Anime-Shader da sind.
UCLASS()
class FIGHTGAME_API AFightShapeVisual : public AActor
{
	GENERATED_BODY()

public:
	AFightShapeVisual();

	void SetShape(EFightShape Shape);
	void SetColor(const FLinearColor& Color);
	// Größe in cm (Grundformen sind 100 cm groß)
	void SetSizeCm(const FVector& Size);
	void SetVisible(bool bVisible);
	// Mesh relativ zum Actor-Ursprung verschieben (z. B. Zylinder auf den Boden stellen)
	void SetMeshOffset(const FVector& OffsetCm);

	// Zusatz-Mesh (z. B. "Nase" für die Blickrichtung)
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

	// Nur bei Änderung an die GPU schicken
	FLinearColor CurrentColor = FLinearColor::Transparent;
	FVector CurrentSize = FVector::ZeroVector;
};
