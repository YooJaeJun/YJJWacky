#pragma once

#include "CoreMinimal.h"
#include "Core/YJJGame.h"
#include "GameFramework/Actor.h"
#include "World/YJJShapes.h"
#include "YJJTower.generated.h"

class AYJJWobbler;
class USceneComponent;
class UStaticMeshComponent;

/**
 * 발판 위 타워. 액터의 +X가 길 쪽이다. 강화는 Lv1~3 (발판 뒤 금색 구슬 개수).
 * 모양은 Parts.csv (Tower.<종류>, 구슬은 Tower.Common), 단계별 위력은 Towers.csv, 나머지 힘은 Tuning.csv.
 *  펀치   : 장갑이 길을 가로질러 튀어나가 줄 선 적을 통째로 날린다
 *  대포   : 가장 앞선 적에게 진짜 물리 포탄을 쏜다
 *  선풍기 : 켜졌다 꺼졌다 하며 길 건너편으로 바람을 분다
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
	/** 월드 좌표 두 점 사이에 원기둥을 걸친다 (고무줄, 올가미) */
	static void Beam(UStaticMeshComponent* Mesh, const FVector& A, const FVector& B, float Thickness);
	static float Tune(const TCHAR* Name) { return YJJData::Tune(Name); }
	/** 포물선: 날아가는 시간 T 동안 To 만큼 가는 발사 속도 */
	static FVector Ballistic(const FVector& To, float T);

	/** Towers.csv 의 이 타워 줄 */
	const FYJJTowerRow& Row() const { return YJJData::Tower(Type); }
	void BuildModel();
	void UpdateLevelLooks();
	void TickPunch(float DeltaTime);
	void TickCannon(float DeltaTime);
	void TickFan(float DeltaTime);
	void TickSling(float DeltaTime);
	void SetPunchLength(float Length);
	void FireCannonBall(const FVector& Origin, const FVector& Aim);
	FVector PouchWorld() const;

	// ---------------------------------------------------------------- 상태 번호
	static constexpr int32 StateIdle = 0;
	static constexpr int32 StatePrepare = 1;	// 펀치: 움찔 당김 / 새총: 올가미
	static constexpr int32 StateStrike = 2;		// 펀치: 뻥 / 새총: 당기기
	static constexpr int32 StateHold = 3;		// 펀치: 뻗은 채 / 새총: 고무줄 튕김
	static constexpr int32 StateReturn = 4;		// 펀치: 돌아옴

	/** 처음 지었을 때 첫 공격까지(초) */
	static constexpr float FirstCooldown = 0.4f;
	/** 이 위아래(cm) 안에 있는 적만 친다 */
	static constexpr float HeightBand = 400.f;
	static constexpr float Gravity = 980.f;

	// ---------------------------------------------------------------- 펀치 (모양·박자)
	/** 장갑이 접혀 있을 때 / 움찔 당겼을 때 길이 */
	static constexpr float PunchRest = 70.f;
	static constexpr float PunchWindup = 20.f;
	/** 팔이 나오는 자리(상자 앞면)와 높이 */
	static constexpr float PunchArmStart = 45.f;
	static constexpr float PunchHeight = 125.f;
	/** 장갑 뒤 팔이 안 보이는 길이, 최소 팔 길이, 팔 굵기 */
	static constexpr float PunchArmGap = 60.f;
	static constexpr float PunchArmMin = 4.f;
	static constexpr float PunchArmThickness = 26.f;
	static constexpr float PunchWindupTime = 0.22f;
	static constexpr float PunchStrikeTime = 0.08f;
	static constexpr float PunchHoldTime = 0.2f;
	static constexpr float PunchReturnTime = 0.4f;
	/** 판정: 상자 앞 이 거리부터 / 감지 폭은 맞는 폭보다 이만큼 좁게 / 맞는 범위는 사거리보다 이만큼 길게 */
	static constexpr float PunchTriggerStart = 140.f;
	static constexpr float PunchTriggerNarrow = 20.f;
	static constexpr float PunchHitStart = 90.f;
	static constexpr float PunchHitExtra = 50.f;
	/** 큰 놈은 몸이 넓어서 이만큼(크기 1당) 더 잘 맞는다 */
	static constexpr float PunchHitPerScale = 35.f;
	/** 날아가는 방향을 옆으로 흔드는 범위(cm/s) */
	static constexpr float PunchSideJitter = 160.f;
	static constexpr float PunchLimp = 2.2f;

	// ---------------------------------------------------------------- 대포
	static constexpr float CannonOriginHeight = 140.f;
	/** 포탄 비행 시간 = 거리 / 이 속도 (최소~최대) */
	static constexpr float CannonFlightSpeed = 1500.f;
	static constexpr float CannonFlightMin = 0.75f;
	static constexpr float CannonFlightMax = 1.7f;
	static constexpr float CannonTurnSpeed = 240.f;
	static constexpr float CannonMuzzle = 190.f;
	static constexpr float BarrelHeight = 78.f;
	static constexpr float RecoilDistance = 45.f;
	static constexpr float RecoilRecover = 3.5f;

	// ---------------------------------------------------------------- 선풍기
	static constexpr float RotorSpeedBase = 1500.f;
	static constexpr float RotorSpeedPerLevel = 650.f;
	static constexpr float RotorSpeedIdle = 90.f;
	static constexpr float RotorSpinUp = 3.f;
	/** 처음 켜지는 때를 흩뜨린다(초) */
	static constexpr float WindPhaseRange = 5.f;
	/** 바람 줄기: 시작 거리, 속도, 줄기 사이 간격, 끝 여유, 퍼짐, 높이 */
	static constexpr float StreakStart = 150.f;
	static constexpr float StreakSpeed = 1200.f;
	static constexpr float StreakSpacing = 173.f;
	static constexpr float StreakEndMargin = 200.f;
	static constexpr float StreakSpread = 380.f;
	static constexpr float StreakBaseHeight = 70.f;
	static constexpr float StreakHeightRange = 210.f;
	static constexpr int32 StreakSeedSide = 3;
	static constexpr int32 StreakSeedHeight = 7;
	static constexpr int32 StreakSeedOffset = 50;
	/** 회오리 줄기: 감기는 빠르기·폭, 끝에서 올라가는 높이 */
	static constexpr float SwirlFrequency = 9.f;
	static constexpr float SwirlAmplitude = 90.f;
	static constexpr float SwirlRise = 160.f;
	static constexpr float FanStart = 100.f;
	/** 멀수록 약해지는 비율 (끝에서 이만큼 줄어든다) */
	static constexpr float FanFalloff = 0.35f;

	// ---------------------------------------------------------------- 새총
	static const FVector PouchRest;
	static const FVector PouchPulled;
	static const FVector ProngTip;
	/** 주머니 기본 크기 (Parts.csv Pouch 와 같은 비율) */
	static const FVector PouchScale;
	static constexpr float BandThickness = 10.f;
	static constexpr float RopeThickness = 6.f;
	/** 붙잡은 놈 몸통을 주머니 위 이만큼(크기 1당)에 둔다 */
	static constexpr float HoldLiftPerScale = 25.f;
	/** 올가미: 이만큼 가까워지거나 이 시간이 지나면 잡은 걸로 */
	static constexpr float LassoArrive = 60.f;
	static constexpr float LassoMaxTime = 1.3f;
	/** 너무 가까운 표적은 고르지 않는다 */
	static constexpr float AimMinDistance = 350.f;
	/** 걸어오는 표적은 이만큼(초) 앞을 겨눈다 */
	static constexpr float AimLead = 0.8f;
	/** 표적이 없으면 뒤로 이만큼 던진다 */
	static constexpr float TossDistance = 1100.f;
	static constexpr float AimTurnSpeed = 360.f;
	static constexpr float DrawTime = 0.7f;
	static constexpr float FireTime = 0.85f;
	static constexpr float SlingFlightSpeed = 1500.f;
	static constexpr float SlingFlightMin = 0.55f;
	static constexpr float SlingFlightMax = 1.2f;
	static constexpr float AimBodyHeight = 30.f;
	/** 쏜 뒤 고무줄이 출렁이는 시간·횟수·폭 */
	static constexpr float SnapTime = 0.35f;
	static constexpr float SnapCycles = 2.5f;
	static constexpr float SnapForward = 120.f;
	static constexpr float SnapUp = 15.f;

	EYJJTowerType Type = EYJJTowerType::Punch;
	int32 Pad = 0;
	int32 Level = 1;
	int32 Invested = 0;
	int32 Shots = 0;
	bool bReady = false;
	int32 State = StateIdle;
	float StateTime = 0.f;
	float Cooldown = 0.f;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Pips;

	// 펀치
	UPROPERTY()
	TObjectPtr<USceneComponent> Glove;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Arm;

	// 대포·새총
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
	TArray<TObjectPtr<UStaticMeshComponent>> Bands;

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

	/** 포탄 크기 배율(지름 84cm)과 무게 */
	static constexpr float BallScale = 0.84f;
	static constexpr float BallMassKg = 90.f;
	/** 쏘자마자 자기 대포에 닿은 건 무시하는 시간 */
	static constexpr float ArmTime = 0.1f;
	/** 터진 뒤 남아 있는 시간, 최대 수명 */
	static constexpr float LingerTime = 2.2f;
	static constexpr float MaxLife = 8.f;
	/** 폭발: 크기 1당 몸 반지름 더하기, 끝까지 가도 이 비율은 남는다, 반경 밖으로 이만큼 퍼진다 */
	static constexpr float BodyRadiusPerScale = 40.f;
	static constexpr float MinFalloff = 0.3f;
	static constexpr float FalloffReach = 1.3f;
	static constexpr float BlastLimp = 2.f;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Ball;

	float Age = 0.f;
	float ExplodedAge = -1.f;
	float Power = 1.f;
};
