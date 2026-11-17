#include "Gameplay/YJJEffects.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/YJJShapes.h"

namespace
{
	constexpr int32 MaxPuffs = 220;
}

AYJJEffects::AYJJEffects()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AYJJEffects::Burst(const UObject* WorldContext, const FVector& Location, const FLinearColor& Color, int32 Count, float Speed, float Size,
	float Life, float Gravity, bool bCube, float UpBias)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	for (TActorIterator<AYJJEffects> It(World); It; ++It)
	{
		It->Spawn(Location, Color, Count, Speed, Size, Life, Gravity, bCube, UpBias);
		return;
	}
}

void AYJJEffects::Spawn(const FVector& Location, const FLinearColor& Color, int32 Count, float Speed, float Size, float Life, float Gravity, bool bCube, float UpBias)
{
	UMaterialInterface* Material = YJJShapes::Solid(this, Color, 0.25f);
	UStaticMesh* Shape = YJJShapes::Mesh(bCube ? EYJJShape::Cube : EYJJShape::Sphere);
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
			Meshes.Add(YJJShapes::Add(this, RootComponent, EYJJShape::Sphere, FVector::ZeroVector, FVector(10.f), nullptr));
			Meshes[Index]->SetCastShadow(false);
		}

		FVector Dir = FMath::VRand();
		Dir.Z = FMath::Abs(Dir.Z);
		Dir = FMath::Lerp(Dir, FVector::UpVector, UpBias).GetSafeNormal();

		FPuff& Puff = Puffs[Index];
		Puff.Location = Location + Dir * Size * 0.5f;
		Puff.Velocity = Dir * Speed * FMath::FRandRange(0.5f, 1.f);
		Puff.Spin = FMath::VRand() * 360.f;
		Puff.Size = Size * FMath::FRandRange(0.6f, 1.f);
		Puff.Age = 0.f;
		Puff.Life = Life * FMath::FRandRange(0.7f, 1.f);
		Puff.Gravity = Gravity;

		UStaticMeshComponent* Mesh = Meshes[Index];
		Mesh->SetStaticMesh(Shape);
		Mesh->SetMaterial(0, Material);
		Mesh->SetWorldLocation(Puff.Location);
		Mesh->SetWorldScale3D(FVector(Puff.Size / 100.f));
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
		Puff.Velocity *= FMath::Max(0.f, 1.f - 1.5f * DeltaTime);
		Puff.Location += Puff.Velocity * DeltaTime;
		// 톡 튀어나왔다가 줄어들며 사라진다
		const float T = Puff.Age / Puff.Life;
		const float Scale = FMath::Min(1.f, T * 8.f) * (1.f - T * T);
		Mesh->SetWorldLocationAndRotation(Puff.Location, FRotator(Puff.Spin.X * Puff.Age, Puff.Spin.Y * Puff.Age, 0.f));
		Mesh->SetWorldScale3D(FVector(FMath::Max(Puff.Size * Scale, 0.1f) / 100.f));
	}
}
