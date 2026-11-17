#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YJJEffects.generated.h"

class UStaticMeshComponent;

/**
 * 파티클 에셋 없이 쓰는 이펙트: 작은 공·상자가 튀어나와 날아가며 줄어든다.
 * 물보라, 타격 별, 연기, 사라지는 뽕 전부 이걸로.
 */
UCLASS()
class YJJWACKY_API AYJJEffects : public AActor
{
	GENERATED_BODY()

public:
	AYJJEffects();

	virtual void Tick(float DeltaTime) override;

	/** UpBias: 0 = 사방으로, 1 = 거의 위로만 */
	static void Burst(const UObject* WorldContext, const FVector& Location, const FLinearColor& Color, int32 Count, float Speed, float Size,
		float Life, float Gravity, bool bCube, float UpBias);

private:
	struct FPuff
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FVector Spin = FVector::ZeroVector;
		float Size = 10.f;
		float Age = 0.f;
		float Life = 0.f;
		float Gravity = 0.f;
	};

	void Spawn(const FVector& Location, const FLinearColor& Color, int32 Count, float Speed, float Size, float Life, float Gravity, bool bCube, float UpBias);

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Meshes;

	TArray<FPuff> Puffs;	// Meshes와 같은 인덱스
};
