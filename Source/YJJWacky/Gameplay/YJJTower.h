#pragma once

#include "CoreMinimal.h"
#include "Core/YJJGame.h"
#include "GameFramework/Actor.h"
#include "World/YJJShapes.h"
#include "YJJTower.generated.h"

class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;

/**
 * 발판 위 타워. 액터의 +X가 길 쪽이다.
 *  펀치   : 장갑이 길을 가로질러 튀어나가 줄 선 적을 통째로 날린다
 *  대포   : 가장 앞선 적에게 진짜 물리 포탄을 쏜다
 *  선풍기 : 켜졌다 꺼졌다 하며 길 건너편으로 바람을 분다
 *  병사   : 깃발만 꽂는다 (병사 본인은 UYJJGame이 길 위에 세운다)
 */
UCLASS()
class YJJWACKY_API AYJJTower : public AActor
{
	GENERATED_BODY()

public:
	AYJJTower();

	void Setup(EYJJTowerType InType, int32 InPad);
	virtual void Tick(float DeltaTime) override;

	EYJJTowerType GetType() const { return Type; }

private:
	UStaticMeshComponent* Part(USceneComponent* Parent, EYJJShape Shape, const FVector& Location, const FVector& Size, uint32 ColorHex,
		const FRotator& Rotation = FRotator::ZeroRotator, bool bCollide = false);

	void BuildPunch();
	void BuildCannon();
	void BuildFan();
	void BuildGuardPost();
	void TickPunch(float DeltaTime);
	void TickCannon(float DeltaTime);
	void TickFan(float DeltaTime);
	void SetPunchLength(float Length);

	EYJJTowerType Type = EYJJTowerType::Punch;
	int32 Pad = 0;
	bool bReady = false;
	int32 State = 0;
	float StateTime = 0.f;
	float Cooldown = 0.f;

	// 펀치
	UPROPERTY()
	TObjectPtr<USceneComponent> Glove;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Arm;

	// 대포
	UPROPERTY()
	TObjectPtr<USceneComponent> Turret;

	UPROPERTY()
	TObjectPtr<USceneComponent> Barrel;

	float Recoil = 0.f;

	// 선풍기
	UPROPERTY()
	TObjectPtr<USceneComponent> Rotor;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Streaks;

	float RotorSpeed = 0.f;
	float WindTime = 0.f;
};

/** 대포알: 진짜로 굴러가며 흐느적이를 볼링핀처럼 쓰러뜨리고, 처음 닿은 곳에서 한 번 터진다 */
UCLASS()
class YJJWACKY_API AYJJCannonBall : public AActor
{
	GENERATED_BODY()

public:
	AYJJCannonBall();

	void Launch(const FVector& Velocity);
	virtual void Tick(float DeltaTime) override;

private:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void Explode();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Ball;

	float Age = 0.f;
	float ExplodedAge = -1.f;
};
