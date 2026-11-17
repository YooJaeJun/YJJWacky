#include "World/YJJShapes.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "YJJAssetPaths.h"

namespace
{
	template <typename T>
	T* LoadCached(const TCHAR* Path)
	{
		// 엔진/게임 에셋은 GC 루트에 올려 두고 계속 쓴다
		static TMap<FString, TWeakObjectPtr<UObject>> Cache;
		if (TWeakObjectPtr<UObject>* Found = Cache.Find(Path))
		{
			if (Found->IsValid())
			{
				return Cast<T>(Found->Get());
			}
		}
		T* Loaded = LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Loaded)
		{
			Loaded->AddToRoot();
			Cache.Add(Path, Loaded);
		}
		return Loaded;
	}

	UMaterialInstanceDynamic* MakeMID(UObject* Outer, const TCHAR* Path)
	{
		UMaterialInterface* Parent = LoadCached<UMaterialInterface>(Path);
		if (!Parent)
		{
			Parent = LoadCached<UMaterialInterface>(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		}
		return Parent ? UMaterialInstanceDynamic::Create(Parent, Outer) : nullptr;
	}
}

UMaterialInstanceDynamic* UYJJPalette::Solid(const FLinearColor& Color, float Emissive)
{
	const uint64 Key = (static_cast<uint64>(Color.ToFColor(true).DWColor()) << 16) | static_cast<uint64>(FMath::Clamp(FMath::RoundToInt(Emissive * 100.f), 0, 65535));
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = Cache.Find(Key))
	{
		return *Found;
	}
	UMaterialInstanceDynamic* MID = MakeMID(this, YJJAssetPaths::FlatMaterial);
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetScalarParameterValue(TEXT("Emissive"), Emissive);
		Cache.Add(Key, MID);
	}
	return MID;
}

UStaticMesh* YJJShapes::Mesh(EYJJShape Shape)
{
	switch (Shape)
	{
	case EYJJShape::Cube: return LoadCached<UStaticMesh>(YJJAssetPaths::Cube);
	case EYJJShape::Sphere: return LoadCached<UStaticMesh>(YJJAssetPaths::Sphere);
	case EYJJShape::Cylinder: return LoadCached<UStaticMesh>(YJJAssetPaths::Cylinder);
	case EYJJShape::Cone: return LoadCached<UStaticMesh>(YJJAssetPaths::Cone);
	case EYJJShape::Plane: return LoadCached<UStaticMesh>(YJJAssetPaths::Plane);
	}
	return nullptr;
}

FLinearColor YJJShapes::Hex(uint32 RGB)
{
	return FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF, 255));
}

UMaterialInstanceDynamic* YJJShapes::Solid(const UObject* WorldContext, const FLinearColor& Color, float Emissive)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	UYJJPalette* Palette = World ? World->GetSubsystem<UYJJPalette>() : nullptr;
	return Palette ? Palette->Solid(Color, Emissive) : nullptr;
}

UMaterialInstanceDynamic* YJJShapes::Unlit(UObject* Outer, const FLinearColor& Color, float Opacity)
{
	UMaterialInstanceDynamic* MID = MakeMID(Outer, YJJAssetPaths::UnlitMaterial);
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	}
	return MID;
}

UMaterialInstanceDynamic* YJJShapes::Water(UObject* Outer)
{
	return MakeMID(Outer, YJJAssetPaths::WaterMaterial);
}

UStaticMeshComponent* YJJShapes::Add(AActor* Owner, USceneComponent* Parent, EYJJShape Shape, const FVector& Location, const FVector& Size,
	UMaterialInterface* Material, bool bCollide, const FRotator& Rotation)
{
	check(Owner);
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
	Comp->SetStaticMesh(Mesh(Shape));
	Comp->SetMobility(Parent ? Parent->Mobility.GetValue() : EComponentMobility::Movable);
	if (Parent)
	{
		Comp->SetupAttachment(Parent);
	}
	Comp->SetRelativeLocation(Location);
	Comp->SetRelativeRotation(Rotation);
	Comp->SetRelativeScale3D(Size / 100.f);
	if (Material)
	{
		Comp->SetMaterial(0, Material);
	}
	if (bCollide)
	{
		Comp->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetGenerateOverlapEvents(false);
	}
	Comp->SetCanEverAffectNavigation(false);
	// 반투명 머티리얼은 그림자를 만들지 않는다
	if (Material && Material->GetBlendMode() != BLEND_Opaque && Material->GetBlendMode() != BLEND_Masked)
	{
		Comp->SetCastShadow(false);
		Comp->bAffectDistanceFieldLighting = false;
		Comp->bAffectDynamicIndirectLighting = false;
	}
	Comp->RegisterComponent();
	return Comp;
}

float YJJShapes::Hash(int32 Seed)
{
	uint32 X = static_cast<uint32>(Seed) * 747796405u + 2891336453u;
	X = ((X >> ((X >> 28u) + 4u)) ^ X) * 277803737u;
	X = (X >> 22u) ^ X;
	return static_cast<float>(X & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

FLinearColor YJJShapes::Shade(const FLinearColor& Color, float Scale)
{
	return FLinearColor(Color.R * Scale, Color.G * Scale, Color.B * Scale, Color.A);
}

// ============================================================ 로우폴리

void FYJJLowPoly::Tri(const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Color, const FVector& Outward)
{
	// 언리얼은 (C-A)x(B-A) 쪽이 앞면이다. Outward와 반대면 B, C를 바꾼다
	FVector N = FVector::CrossProduct(C - A, B - A);
	const bool bFlip = FVector::DotProduct(N, Outward) < 0.f;
	N = (bFlip ? -N : N).GetSafeNormal();

	// 면마다 밝기를 아주 조금씩 달리해서 각진 면이 살아 보이게
	const FLinearColor Face = YJJShapes::Shade(Color, 0.95f + 0.1f * YJJShapes::Hash(Vertices.Num() * 7 + 13));
	const int32 Index = Vertices.Num();
	Vertices.Add(A);
	Vertices.Add(bFlip ? C : B);
	Vertices.Add(bFlip ? B : C);
	for (int32 i = 0; i < 3; ++i)
	{
		Normals.Add(N);
		Colors.Add(Face);
		Triangles.Add(Index + i);
	}
}

void FYJJLowPoly::Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Color, const FVector& Outward)
{
	Tri(A, B, C, Color, Outward);
	Tri(A, C, D, Color, Outward);
}

void FYJJLowPoly::Frustum(const FVector& Base, float BottomRadius, float TopRadius, float Height, int32 Sides,
	const FLinearColor& TopColor, const FLinearColor& SideColor, float Jitter, int32 Seed, float YawDeg)
{
	TArray<FVector> Bottom;
	TArray<FVector> Top;
	for (int32 i = 0; i < Sides; ++i)
	{
		const float A = FMath::DegreesToRadians(YawDeg) + i * TWO_PI / Sides;
		const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.f);
		const float J0 = 1.f + Jitter * (YJJShapes::Hash(Seed * 31 + i * 2) - 0.5f) * 2.f;
		const float J1 = 1.f + Jitter * (YJJShapes::Hash(Seed * 31 + i * 2 + 1) - 0.5f) * 2.f;
		Bottom.Add(Base + Dir * BottomRadius * J0);
		Top.Add(Base + FVector(0, 0, Height) + Dir * TopRadius * J1);
	}
	const FVector TopCenter = Base + FVector(0, 0, Height);
	for (int32 i = 0; i < Sides; ++i)
	{
		const int32 j = (i + 1) % Sides;
		const FVector Out = ((Bottom[i] + Bottom[j] + Top[i] + Top[j]) * 0.25f - (Base + FVector(0, 0, Height * 0.5f))).GetSafeNormal2D() + FVector(0, 0, 0.01f);
		if (TopRadius > 1.f)
		{
			Quad(Bottom[i], Bottom[j], Top[j], Top[i], SideColor, Out);
			Tri(TopCenter, Top[i], Top[j], TopColor, FVector::UpVector);
		}
		else
		{
			Tri(Bottom[i], Bottom[j], TopCenter, SideColor, Out);
		}
	}
}

void FYJJLowPoly::Ridge(const FVector& A, const FVector& B, float TopWidth, float Depth, float Flare, const FLinearColor& TopColor, const FLinearColor& SideColor)
{
	const FVector Dir = (B - A).GetSafeNormal2D();
	const FVector Right(-Dir.Y, Dir.X, 0.f);
	const float H = TopWidth * 0.5f;
	const FVector Down(0, 0, -Depth);
	const FVector AL = A - Right * H, AR = A + Right * H, BL = B - Right * H, BR = B + Right * H;
	Quad(AL, BL, BR, AR, TopColor, FVector::UpVector);
	Quad(AR, BR, BR + Right * Flare + Down, AR + Right * Flare + Down, SideColor, Right);
	Quad(AL, BL, BL - Right * Flare + Down, AL - Right * Flare + Down, SideColor, -Right);
}

void FYJJLowPoly::Box(const FVector& Center, const FVector& Size, const FLinearColor& TopColor, const FLinearColor& SideColor, float YawDeg)
{
	const FQuat Q(FVector::UpVector, FMath::DegreesToRadians(YawDeg));
	const FVector X = Q.RotateVector(FVector(Size.X * 0.5f, 0, 0));
	const FVector Y = Q.RotateVector(FVector(0, Size.Y * 0.5f, 0));
	const FVector Z(0, 0, Size.Z * 0.5f);
	Quad(Center - X - Y + Z, Center + X - Y + Z, Center + X + Y + Z, Center - X + Y + Z, TopColor, FVector::UpVector);
	Quad(Center + X - Y - Z, Center + X + Y - Z, Center + X + Y + Z, Center + X - Y + Z, SideColor, X);
	Quad(Center - X - Y - Z, Center - X + Y - Z, Center - X + Y + Z, Center - X - Y + Z, SideColor, -X);
	Quad(Center - X + Y - Z, Center + X + Y - Z, Center + X + Y + Z, Center - X + Y + Z, SideColor, Y);
	Quad(Center - X - Y - Z, Center + X - Y - Z, Center + X - Y + Z, Center - X - Y + Z, SideColor, -Y);
}

void FYJJLowPoly::Rock(const FVector& Center, float Radius, const FLinearColor& Color, int32 Seed)
{
	const float T = 1.618034f;
	const FVector Base[12] = {
		FVector(-1, T, 0), FVector(1, T, 0), FVector(-1, -T, 0), FVector(1, -T, 0),
		FVector(0, -1, T), FVector(0, 1, T), FVector(0, -1, -T), FVector(0, 1, -T),
		FVector(T, 0, -1), FVector(T, 0, 1), FVector(-T, 0, -1), FVector(-T, 0, 1) };
	const int32 Faces[20][3] = {
		{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
		{3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1} };
	FVector P[12];
	for (int32 i = 0; i < 12; ++i)
	{
		const float J = 0.75f + 0.45f * YJJShapes::Hash(Seed * 17 + i);
		FVector V = Base[i].GetSafeNormal() * Radius * J;
		V.Z *= 0.75f;
		P[i] = Center + V;
	}
	for (const int32* F : Faces)
	{
		Tri(P[F[0]], P[F[1]], P[F[2]], Color, (P[F[0]] + P[F[1]] + P[F[2]]) / 3.f - Center);
	}
}

void FYJJLowPoly::Tree(const FVector& Base, float Height, const FLinearColor& Trunk, const FLinearColor& Leaf, int32 Seed)
{
	const float R = Height * 0.3f;
	Frustum(Base, Height * 0.07f, Height * 0.05f, Height * 0.35f, 5, Trunk, Trunk, 0.f, Seed);
	for (int32 i = 0; i < 3; ++i)
	{
		const float Z = Height * (0.25f + i * 0.22f);
		const float Scale = 1.f - i * 0.25f;
		Frustum(Base + FVector(0, 0, Z), R * Scale, 0.f, Height * 0.4f, 6, Leaf, YJJShapes::Shade(Leaf, 1.f + i * 0.08f), 0.12f, Seed + i * 5, YJJShapes::Hash(Seed + i) * 60.f);
	}
}

UProceduralMeshComponent* FYJJLowPoly::Build(AActor* Owner, USceneComponent* Parent, const UObject* WorldContext, bool bCastShadow)
{
	check(Owner);
	UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(Owner);
	Mesh->SetMobility(EComponentMobility::Movable);
	if (Parent)
	{
		Mesh->SetupAttachment(Parent);
	}
	Mesh->bUseAsyncCooking = false;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetCastShadow(bCastShadow);
	Mesh->RegisterComponent();
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, TArray<FVector2D>(), Colors, TArray<FProcMeshTangent>(), false, false);
	if (UMaterialInterface* Material = YJJShapes::Solid(WorldContext, FLinearColor::White))
	{
		Mesh->SetMaterial(0, Material);
	}
	return Mesh;
}
