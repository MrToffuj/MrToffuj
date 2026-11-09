#include "FightShapeVisual.h"

#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AFightShapeVisual::AFightShapeVisual()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	CubeMesh = Cube.Object;
	SphereMesh = Sphere.Object;
	CylinderMesh = Cylinder.Object;
	BaseMaterial = Mat.Object;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetupAttachment(Root);
}

UStaticMesh* AFightShapeVisual::MeshFor(EFightShape Shape) const
{
	switch (Shape)
	{
	case EFightShape::Sphere: return SphereMesh;
	case EFightShape::Cylinder: return CylinderMesh;
	default: return CubeMesh;
	}
}

void AFightShapeVisual::SetShape(EFightShape Shape)
{
	Mesh->SetStaticMesh(MeshFor(Shape));
	if (BaseMaterial)
	{
		Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Mesh->SetMaterial(0, Material);
	}
}

void AFightShapeVisual::SetColor(const FLinearColor& Color)
{
	if (Material) Material->SetVectorParameterValue(TEXT("Color"), Color);
}

void AFightShapeVisual::SetSizeCm(const FVector& Size)
{
	Mesh->SetRelativeScale3D(Size / 100.f);
}

void AFightShapeVisual::SetMeshOffset(const FVector& OffsetCm)
{
	Mesh->SetRelativeLocation(OffsetCm);
}

void AFightShapeVisual::SetVisible(bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
}

UStaticMeshComponent* AFightShapeVisual::AddMarker(EFightShape Shape, const FVector& RelativeLocation, const FVector& SizeCm,
	const FLinearColor& Color)
{
	UStaticMeshComponent* Marker = NewObject<UStaticMeshComponent>(this);
	Marker->SetStaticMesh(MeshFor(Shape));
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetMobility(EComponentMobility::Movable);
	Marker->SetupAttachment(Root);
	Marker->RegisterComponent();
	Marker->SetRelativeLocation(RelativeLocation);
	Marker->SetRelativeScale3D(SizeCm / 100.f);
	if (BaseMaterial)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		M->SetVectorParameterValue(TEXT("Color"), Color);
		Marker->SetMaterial(0, M);
	}
	return Marker;
}
