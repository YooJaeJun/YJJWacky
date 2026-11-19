#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YJJEffects.generated.h"

class UStaticMeshComponent;
struct FYJJEffectRow;

/**
 * 파티클 에셋 없이 쓰는 이펙트: 작은 공·상자가 튀어나와 날아가며 줄어든다.
 * 이펙트마다 색·개수·속도는 Content/YJJWacky/Data/Effects.csv.
 */
UCLASS()
class YJJWACKY_API AYJJEffects : public AActor
{
	GENERATED_BODY()

public:
	AYJJEffects();

	virtual void Tick(float DeltaTime) override;

	/** Effects.csv 의 Name 이펙트를 터뜨린다. Scale은 크기 배율 (큰 놈이 빠지면 물보라도 크게) */
	static void Burst(const UObject* WorldContext, const TCHAR* Name, const FVector& Location, float Scale = 1.f);

private:
	struct FPuff
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FVector Spin = FVector::ZeroVector;
		float Size = 0.f;
		float Age = 0.f;
		float Life = 0.f;
		float Gravity = 0.f;
	};

	/** 한꺼번에 떠 있을 수 있는 조각 수 */
	static constexpr int32 MaxPuffs = 220;
	/** 조각 무작위 범위 (원래 값 대비 최소 비율) */
	static constexpr float MinSpeedRatio = 0.5f;
	static constexpr float MinSizeRatio = 0.6f;
	static constexpr float MinLifeRatio = 0.7f;
	/** 초당 빙글빙글 도는 각도 */
	static constexpr float SpinDegrees = 360.f;
	/** 공기 저항 (1/s) */
	static constexpr float Drag = 1.5f;
	/** 톡 튀어나오는 속도: 수명의 1/이 값 만에 다 커진다 */
	static constexpr float PopIn = 8.f;
	static constexpr float MinVisibleSize = 0.1f;
	static constexpr float MeshSize = 100.f;
	/** 조각 머티리얼의 은은한 발광 */
	static constexpr float PuffEmissive = 0.25f;

	void Spawn(const FYJJEffectRow& Row, const FVector& Location, float Scale);

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Meshes;

	TArray<FPuff> Puffs;	// Meshes와 같은 인덱스
};
