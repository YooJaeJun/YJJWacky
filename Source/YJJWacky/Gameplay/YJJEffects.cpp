#include "Gameplay/YJJEffects.h"

#include "Components/StaticMeshComponent.h"
#include "Data/YJJData.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/YJJShapes.h"

AYJJEffects::AYJJEffects()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AYJJEffects::Burst(const UObject* WorldContext, const TCHAR* Name, const FVector& Location, float Scale)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const FYJJEffectRow* Row = YJJData::Effect(Name);
	if (!World || !Row)
	{
		return;
	}
	for (TActorIterator<AYJJEffects> It(World); It; ++It)
	{
		It->Spawn(*Row, Location, Scale);
		return;
	}
}

void AYJJEffects::Spawn(const FYJJEffectRow& Row, const FVector& Location, float Scale)
{
	UMaterialInterface* Material = YJJShapes::Solid(this, Row.LinearColor(), PuffEmissive);
	UStaticMesh* Shape = YJJShapes::Mesh(Row.Cube ? EYJJShape::Cube : EYJJShape::Sphere);
	// 크기 배율: 조각 크기는 그대로, 속도는 제곱근만큼, 개수는 CountPerScale 만큼 늘어난다
	const int32 Count = Row.Count + FMath::RoundToInt(Row.CountPerScale * Scale);
	const float Speed = Row.Speed * FMath::Sqrt(Scale);
	const float Size = Row.Size * Scale;
	const FVector Origin = Location + FVector(0.f, 0.f, Row.OffsetZ * Scale);
	for (int32 n = 0; n < Count; ++n)
	{
		// 쉬는 조각을 찾고, 없으면 새로 만든다
		int32 Index = INDEX_NONE;
		for (int32 i = 0; i < Puffs.Num(); ++i)
		{
			if (Puffs[i].Life <= 0.f)
			{
				Index = i;
				break;
			}
		}
		if (Index == INDEX_NONE)
		{
			if (Puffs.Num() >= MaxPuffs)
			{
				return;
			}
			Index = Puffs.AddDefaulted();
			Meshes.Add(YJJShapes::Add(this, RootComponent, EYJJShape::Sphere, FVector::ZeroVector, FVector(MeshSize * MinVisibleSize), nullptr));
			Meshes[Index]->SetCastShadow(false);
		}

		FVector Dir = FMath::VRand();
		Dir.Z = FMath::Abs(Dir.Z);
		Dir = FMath::Lerp(Dir, FVector::UpVector, Row.UpBias).GetSafeNormal();

		FPuff& Puff = Puffs[Index];
		Puff.Location = Origin + Dir * Size * 0.5f;
		Puff.Velocity = Dir * Speed * FMath::FRandRange(MinSpeedRatio, 1.f);
		Puff.Spin = FMath::VRand() * SpinDegrees;
		Puff.Size = Size * FMath::FRandRange(MinSizeRatio, 1.f);
		Puff.Age = 0.f;
		Puff.Life = Row.Life * FMath::FRandRange(MinLifeRatio, 1.f);
		Puff.Gravity = Row.Gravity;

		UStaticMeshComponent* Mesh = Meshes[Index];
		Mesh->SetStaticMesh(Shape);
		Mesh->SetMaterial(0, Material);
		Mesh->SetWorldLocation(Puff.Location);
		Mesh->SetWorldScale3D(FVector(Puff.Size / MeshSize));
		Mesh->SetVisibility(true);
	}
}

void AYJJEffects::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	for (int32 i = 0; i < Puffs.Num(); ++i)
	{
		FPuff& Puff = Puffs[i];
		if (Puff.Life <= 0.f)
		{
			continue;
		}
		Puff.Age += DeltaTime;
		UStaticMeshComponent* Mesh = Meshes[i];
		if (Puff.Age >= Puff.Life)
		{
			Puff.Life = 0.f;
			Mesh->SetVisibility(false);
			continue;
		}
		Puff.Velocity.Z -= Puff.Gravity * DeltaTime;
		Puff.Velocity *= FMath::Max(0.f, 1.f - Drag * DeltaTime);
		Puff.Location += Puff.Velocity * DeltaTime;
		// 톡 튀어나왔다가 줄어들며 사라진다
		const float T = Puff.Age / Puff.Life;
		const float Grow = FMath::Min(1.f, T * PopIn) * (1.f - T * T);
		Mesh->SetWorldLocationAndRotation(Puff.Location, FRotator(Puff.Spin.X * Puff.Age, Puff.Spin.Y * Puff.Age, 0.f));
		Mesh->SetWorldScale3D(FVector(FMath::Max(Puff.Size * Grow, MinVisibleSize) / MeshSize));
	}
}
