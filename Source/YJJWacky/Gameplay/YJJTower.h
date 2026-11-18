#pragma once

#include "CoreMinimal.h"
#include "Core/YJJGame.h"
#include "GameFramework/Actor.h"
#include "World/YJJShapes.h"
#include "YJJTower.generated.h"

class AYJJWobbler;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;

/**
 * 발판 위 타워. 액터의 +X가 길 쪽이다. 강화는 Lv1~3 (발판 뒤 금색 구슬 개수).
 *  펀치   : 장갑이 길을 가로질러 튀어나가 줄 선 적을 통째로 날린다. 강화하면 장갑이 커지고 연타
 *  대포   : 가장 앞선 적에게 진짜 물리 포탄을 쏜다. Lv3은 세 발
 *  선풍기 : 켜졌다 꺼졌다 하며 길 건너편으로 바람을 분다. Lv3은 회오리(공중으로 띄움)
 *  병사   : 깃발만 꽂는다 (병사 본인은 UYJJGame이 길 위에 세운다)
 *  새총   : 무게 한도 안의 적을 올가미로 낚아채 고무줄을 당겼다가, 앞선 무리에 탄알로 쏜다
 */
UCLASS()
class YJJWACKY_API AYJJTower : public AActor
{
	GENERATED_BODY()

public:
	AYJJTower();

	void Setup(EYJJTowerType InType, int32 InPad, int32 Cost);
	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	EYJJTowerType GetType() const { return Type; }
	int32 GetLevel() const { return Level; }
	/** 지금까지 들인 골드 (판매가 계산용) */
	int32 GetInvested() const { return Invested; }
	void SetLevel(int32 NewLevel, int32 AddedCost);
	/** 공격 횟수 (자동 테스트용) */
	int32 GetShots() const { return Shots; }

private:
	UStaticMeshComponent* Part(USceneComponent* Parent, EYJJShape Shape, const FVector& Location, const FVector& Size, uint32 ColorHex,
		const FRotator& Rotation = FRotator::ZeroRotator, bool bCollide = false);
	/** 월드 좌표 두 점 사이에 원기둥을 걸친다 (고무줄, 올가미) */
	static void Beam(UStaticMeshComponent* Mesh, const FVector& A, const FVector& B, float Thickness);

	void BuildPunch();
	void BuildCannon();
	void BuildFan();
	void BuildGuardPost();
	void BuildSling();
	void BuildLevelPips();
	void UpdateLevelLooks();
	void TickPunch(float DeltaTime);
	void TickCannon(float DeltaTime);
	void TickFan(float DeltaTime);
	void TickSling(float DeltaTime);
	void SetPunchLength(float Length);
	void FireCannonBall(const FVector& Origin, const FVector& Aim);
	/** Towers.csv 의 이 타워 줄 */
	const FYJJTowerRow& Row() const { return YJJData::Tower(Type); }

	EYJJTowerType Type = EYJJTowerType::Punch;
	int32 Pad = 0;
	int32 Level = 1;
	int32 Invested = 0;
	int32 Shots = 0;
	bool bReady = false;
	int32 State = 0;
	float StateTime = 0.f;
	float Cooldown = 0.f;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Pips;

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
	TArray<TObjectPtr<UStaticMeshComponent>> Blades;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Streaks;

	float RotorSpeed = 0.f;
	float WindTime = 0.f;

	// 새총
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Pouch;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Bands[2];

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Rope;

	TWeakObjectPtr<AYJJWobbler> Victim;
	FVector PouchLocal = FVector::ZeroVector;
	FVector AimPoint = FVector::ZeroVector;
	bool bAimAtWater = false;
	float HeavyHintTime = 0.f;
};

/** 대포알: 진짜로 굴러가며 흐느적이를 볼링핀처럼 쓰러뜨리고, 처음 닿은 곳에서 한 번 터진다 */
UCLASS()
class YJJWACKY_API AYJJCannonBall : public AActor
{
	GENERATED_BODY()

public:
	AYJJCannonBall();

	/** Power는 강화 배율: 포탄 크기·폭발 반경·힘이 함께 커진다 */
	void Launch(const FVector& Velocity, float Power);
	virtual void Tick(float DeltaTime) override;

private:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void Explode();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Ball;

	float Age = 0.f;
	float ExplodedAge = -1.f;
	float Power = 1.f;
};
