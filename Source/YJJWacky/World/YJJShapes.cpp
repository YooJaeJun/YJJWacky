#include "World/YJJShapes.h"

#include "Components/StaticMeshComponent.h"
#include "Data/YJJData.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "YJJAssetPaths.h"
#include "YJJWacky.h"

TMap<FString, TWeakObjectPtr<UObject>> YJJShapes::AssetCache;

UMaterialInstanceDynamic* UYJJPalette::Solid(const FLinearColor& Color, float Emissive)
{
	const uint64 Key = (static_cast<uint64>(Color.ToFColor(true).DWColor()) << EmissiveKeyBits)
		| static_cast<uint64>(FMath::Clamp(FMath::RoundToInt(Emissive * EmissiveKeyScale), 0, (1 << EmissiveKeyBits) - 1));
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = Cache.Find(Key))
	{
		return *Found;
	}
	UMaterialInterface* Parent = LoadObject<UMaterialInterface>(nullptr, YJJAssetPaths::FlatMaterial, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UMaterialInstanceDynamic* MID = Parent ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr;
	if (MID)
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetScalarParameterValue(TEXT("Emissive"), Emissive);
		Cache.Add(Key, MID);
	}
	return MID;
}

// ============================================================ 모델 부품 찾기

USceneComponent* FYJJModelParts::First(const TCHAR* Tag) const
{
	const TArray<USceneComponent*>* Found = Tagged.Find(FName(Tag));
	return (Found && Found->Num()) ? (*Found)[0] : nullptr;
}

UStaticMeshComponent* FYJJModelParts::FirstMesh(const TCHAR* Tag) const
{
	return Cast<UStaticMeshComponent>(First(Tag));
}

TArray<UStaticMeshComponent*> FYJJModelParts::Meshes(const TCHAR* Tag) const
{
	TArray<UStaticMeshComponent*> Result;
	if (const TArray<USceneComponent*>* Found = Tagged.Find(FName(Tag)))
	{
		for (USceneComponent* Comp : *Found)
		{
			if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Comp))
			{
				Result.Add(Mesh);
			}
		}
	}
	return Result;
}

// ============================================================ 도형

UMaterialInstanceDynamic* YJJShapes::MakeMID(UObject* Outer, const TCHAR* Path)
{
	UMaterialInterface* Parent = LoadCached<UMaterialInterface>(Path);
	if (!Parent)
	{
		Parent = LoadCached<UMaterialInterface>(FallbackMaterial);
	}
	return Parent ? UMaterialInstanceDynamic::Create(Parent, Outer) : nullptr;
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

EYJJShape YJJShapes::ParseShape(const FString& Name)
{
	if (Name.Equals(TEXT("Sphere"), ESearchCase::IgnoreCase)) { return EYJJShape::Sphere; }
	if (Name.Equals(TEXT("Cylinder"), ESearchCase::IgnoreCase)) { return EYJJShape::Cylinder; }
	if (Name.Equals(TEXT("Cone"), ESearchCase::IgnoreCase)) { return EYJJShape::Cone; }
	if (Name.Equals(TEXT("Plane"), ESearchCase::IgnoreCase)) { return EYJJShape::Plane; }
	return EYJJShape::Cube;
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
	Comp->SetRelativeScale3D(Size / MeshSize);
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

USceneComponent* YJJShapes::AddPivot(AActor* Owner, USceneComponent* Parent, const FVector& Location, const FRotator& Rotation)
{
	USceneComponent* Scene = NewObject<USceneComponent>(Owner);
	Scene->SetupAttachment(Parent);
	Scene->SetRelativeLocation(Location);
	Scene->SetRelativeRotation(Rotation);
	Scene->RegisterComponent();
	return Scene;
}

FLinearColor YJJShapes::PartColor(const FString& Name, const FYJJModelContext& Context)
{
	if (Name.Equals(BodyColorName, ESearchCase::IgnoreCase))
	{
		return Context.Body;
	}
	if (Name.Equals(DarkColorName, ESearchCase::IgnoreCase))
	{
		return Context.Dark;
	}
	return YJJData::Color(Name);
}

void YJJShapes::BuildPart(AActor* Owner, const FYJJPartRow& Row, FYJJModelContext& Context, FYJJModelParts& Out)
{
	USceneComponent* const* ParentFound = Context.Parents.Find(FName(*Row.Parent));
	USceneComponent* Parent = ParentFound ? *ParentFound : nullptr;
	if (!Parent)
	{
		UE_LOG(LogYJJWacky, Warning, TEXT("[Parts] %s: 부모 '%s' 가 없어서 Root 에 단다"), *Row.Model, *Row.Parent);
		USceneComponent* const* Root = Context.Parents.Find(FName(RootParent));
		Parent = Root ? *Root : Owner->GetRootComponent();
	}
	const bool bPivot = Row.Type.Equals(PivotType, ESearchCase::IgnoreCase);
	const bool bRoll = Row.RepeatAxis.Equals(RollAxis, ESearchCase::IgnoreCase);
	const int32 Sides = Row.Mirror ? 2 : 1;
	const FName Tag = Row.Tag.IsEmpty() ? NAME_None : FName(*Row.Tag);

	for (int32 Side = 0; Side < Sides; ++Side)
	{
		// 대칭: 좌우(Y)를 뒤집으면 Yaw·Roll 도 뒤집힌다
		const float Flip = Side == 0 ? 1.f : -1.f;
		const FVector BaseLocation(Row.X, Row.Y * Flip, Row.Z);
		const FRotator BaseRotation(Row.Pitch, Row.Yaw * Flip, Row.Roll * Flip);
		for (int32 k = 0; k < FMath::Max(1, Row.Repeat); ++k)
		{
			const float Step = Row.RepeatStep * k;
			const FQuat Around = bRoll ? FRotator(0.f, 0.f, Step).Quaternion() : FRotator(0.f, Step, 0.f).Quaternion();
			const FVector Location = Around.RotateVector(BaseLocation) * Context.Scale;
			const FRotator Rotation = (Around * BaseRotation.Quaternion()).Rotator();

			USceneComponent* Made = nullptr;
			if (bPivot)
			{
				Made = AddPivot(Owner, Parent, Location, Rotation);
				if (Row.ScaleBy.Equals(ClubScaleName, ESearchCase::IgnoreCase))
				{
					Made->SetRelativeScale3D(FVector(Context.ClubScale));
				}
				if (Tag != NAME_None && !Context.Parents.Contains(Tag))
				{
					Context.Parents.Add(Tag, Made);
				}
			}
			else
			{
				const FLinearColor Color = PartColor(Row.Color, Context);
				UMaterialInterface* Material = Row.Opacity > 0.f ? static_cast<UMaterialInterface*>(Unlit(Owner, Color, Row.Opacity)) : Solid(Owner, Color);
				const FVector Size = FVector(Row.SX, Row.SY, Row.SZ) * Context.Scale;
				UStaticMeshComponent* Mesh = Add(Owner, Parent, ParseShape(Row.Shape), Location, Size, Material, Row.Collide != 0, Rotation);
				Mesh->SetCastShadow(Row.Shadow != 0 && Row.Opacity <= 0.f);
				if (Row.Hidden)
				{
					Mesh->SetVisibility(false);
				}
				if (Row.Loose)
				{
					Mesh->SetUsingAbsoluteLocation(true);
					Mesh->SetUsingAbsoluteRotation(true);
					Mesh->SetUsingAbsoluteScale(true);
				}
				if (Row.Skin)
				{
					Out.Skin.Add(Mesh);
				}
				Made = Mesh;
			}
			if (Tag != NAME_None)
			{
				Out.Tagged.FindOrAdd(Tag).Add(Made);
			}
		}
	}
}

FYJJModelParts YJJShapes::BuildModel(AActor* Owner, const FString& Model, FYJJModelContext& Context)
{
	check(Owner);
	FYJJModelParts Out;
	if (!Context.Parents.Contains(FName(RootParent)))
	{
		Context.Parents.Add(FName(RootParent), Owner->GetRootComponent());
	}
	for (const FYJJPartRow* Row : YJJData::Parts(Model))
	{
		if (Context.Level >= Row->MinLevel && Context.Level <= Row->MaxLevel)
		{
			BuildPart(Owner, *Row, Context, Out);
		}
	}
	return Out;
}

float YJJShapes::Hash(int32 Seed)
{
	uint32 X = static_cast<uint32>(Seed) * HashMultiplier + HashIncrement;
	X = ((X >> ((X >> HashShiftSelect) + HashShiftBase)) ^ X) * HashMixMultiplier;
	X = (X >> HashFinalShift) ^ X;
	return static_cast<float>(X & HashMask) / static_cast<float>(HashMask);
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

	const FLinearColor Face = YJJShapes::Shade(Color, FaceShadeMin + FaceShadeRange * YJJShapes::Hash(Vertices.Num() * FaceSeedMultiplier + FaceSeedOffset));
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
		const float J0 = 1.f + Jitter * (YJJShapes::Hash(Seed * JitterSeedMultiplier + i * 2) - 0.5f) * 2.f;
		const float J1 = 1.f + Jitter * (YJJShapes::Hash(Seed * JitterSeedMultiplier + i * 2 + 1) - 0.5f) * 2.f;
		Bottom.Add(Base + Dir * BottomRadius * J0);
		Top.Add(Base + FVector(0, 0, Height) + Dir * TopRadius * J1);
	}
	const FVector TopCenter = Base + FVector(0, 0, Height);
	for (int32 i = 0; i < Sides; ++i)
	{
		const int32 j = (i + 1) % Sides;
		const FVector Out = ((Bottom[i] + Bottom[j] + Top[i] + Top[j]) * 0.25f - (Base + FVector(0, 0, Height * 0.5f))).GetSafeNormal2D() + FVector(0, 0, OutwardLift);
		if (TopRadius > PointedRadius)
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
	// 정이십면체 꼭짓점: (±1, ±φ, 0), (0, ±1, ±φ), (±φ, 0, ±1)
	const float T = GoldenRatio;
	const FVector Base[IcoVertexCount] = {
		FVector(-1, T, 0), FVector(1, T, 0), FVector(-1, -T, 0), FVector(1, -T, 0),
		FVector(0, -1, T), FVector(0, 1, T), FVector(0, -1, -T), FVector(0, 1, -T),
		FVector(T, 0, -1), FVector(T, 0, 1), FVector(-T, 0, -1), FVector(-T, 0, 1) };
	FVector P[IcoVertexCount];
	for (int32 i = 0; i < IcoVertexCount; ++i)
	{
		const float J = RockJitterMin + RockJitterRange * YJJShapes::Hash(Seed * RockSeedMultiplier + i);
		FVector V = Base[i].GetSafeNormal() * Radius * J;
		V.Z *= RockFlatten;
		P[i] = Center + V;
	}
	for (const int32* F : IcoFaces)
	{
		Tri(P[F[0]], P[F[1]], P[F[2]], Color, (P[F[0]] + P[F[1]] + P[F[2]]) / 3.f - Center);
	}
}

void FYJJLowPoly::Tree(const FVector& Base, float Height, const FLinearColor& Trunk, const FLinearColor& Leaf, int32 Seed)
{
	const float R = Height * TreeCrownRadius;
	Frustum(Base, Height * TrunkBottom, Height * TrunkTop, Height * TrunkHeight, TrunkSides, Trunk, Trunk, 0.f, Seed);
	for (int32 i = 0; i < CrownLayers; ++i)
	{
		const float Z = Height * (CrownStart + i * CrownStep);
		const float Shrink = 1.f - i * CrownShrink;
		Frustum(Base + FVector(0, 0, Z), R * Shrink, 0.f, Height * CrownHeight, CrownSides, Leaf, YJJShapes::Shade(Leaf, 1.f + i * CrownBrighten),
			CrownJitter, Seed + i * CrownSeedStep, YJJShapes::Hash(Seed + i) * CrownYawRange);
	}
}

void FYJJLowPoly::AddRow(const FYJJLowPolyRow& Row, const FVector& Origin, float Side)
{
	const FVector At = Origin + FVector(Row.X, Row.Y * Side, Row.Z);
	const FLinearColor Top = YJJData::Color(Row.TopColor);
	const FLinearColor SideColor = YJJData::Color(Row.SideColor);
	if (Row.Kind.Equals(TEXT("Rock"), ESearchCase::IgnoreCase))
	{
		Rock(At, Row.A, Top, Row.Seed);
	}
	else if (Row.Kind.Equals(TEXT("Box"), ESearchCase::IgnoreCase))
	{
		Box(At, FVector(Row.A, Row.B, Row.C), Top, SideColor, Row.Yaw * Side);
	}
	else if (Row.Kind.Equals(TEXT("Tree"), ESearchCase::IgnoreCase))
	{
		Tree(At, Row.C, SideColor, Top, Row.Seed);
	}
	else
	{
		Frustum(At, Row.A, Row.B, Row.C, Row.Sides, Top, SideColor, Row.Jitter, Row.Seed, Row.Yaw * Side);
	}
}

void FYJJLowPoly::AddModel(const FString& Model, const FVector& Origin)
{
	for (const FYJJLowPolyRow* Row : YJJData::LowPoly(Model))
	{
		AddRow(*Row, Origin, 1.f);
		if (Row->Mirror)
		{
			AddRow(*Row, Origin, -1.f);
		}
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
