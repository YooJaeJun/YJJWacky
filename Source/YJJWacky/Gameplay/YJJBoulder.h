#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YJJBoulder.generated.h"

class AYJJWobbler;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 바위 주문: 하늘에서 떨어져 쾅 터지고, 길을 거꾸로 (적이 오는 쪽으로) 굴러가며 볼링핀처럼 쓰러뜨린다.
 * 길이 꺾이면 그대로 굴러 물에 빠진다. 수치는 Tuning.csv Spell.Boulder.*
 */
UCLASS()
class YJJWACKY_API AYJJBoulder : public AActor
{
	GENERATED_BODY()

public:
	AYJJBoulder();

	/** Target 위 하늘에서 떨어뜨린다. RollDir 은 땅에 닿은 뒤 굴러갈 방향 */
	void Drop(const FVector& Target, const FVector& RollDir);
	virtual void Tick(float DeltaTime) override;

	/** 떨어지는 중이면 떨어질 자리 (HUD 그림자 표시) */
	bool IsFalling() const { return !bLanded; }
	const FVector& GetTarget() const { return Target; }

private:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void Slam();
	void Crumble();
	/** 이 반경 안에 있는 적·병사를 Velocity 쪽으로 친다 */
	void HitAround(float Radius, const FVector& Push, float Up, float Damage, float Limp);

	static float Tune(const TCHAR* Name);

	/** 크기 1 당 몸 반지름 (맞음 판정 여유) */
	static constexpr float BodyRadiusPerScale = 40.f;
	/** 굴러가며 같은 놈을 다시 치기까지 (초) */
	static constexpr float RehitTime = 0.6f;
	/** 떨어지자마자 아무것도 안 닿았을 때 터지는 높이 여유 (cm) */
	static constexpr float SlamHeight = 30.f;
	/** 바위 모양: 살짝 찌그러진 공 (지름 배율) */
	static const FVector LumpScale;

	UPROPERTY()
	TObjectPtr<USphereComponent> Body;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Rock;

	FVector Target = FVector::ZeroVector;
	FVector RollDir = FVector::ForwardVector;
	float Radius = 0.f;
	float Age = 0.f;
	float RollTime = 0.f;
	bool bLanded = false;
	TMap<TWeakObjectPtr<AYJJWobbler>, float> LastHit;
};
