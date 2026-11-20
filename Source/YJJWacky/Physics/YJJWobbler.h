#pragma once

#include "CoreMinimal.h"
#include "Data/YJJData.h"
#include "GameFramework/Actor.h"
#include "YJJWobbler.generated.h"

class UCapsuleComponent;
class UPhysicalMaterial;
class UPhysicsConstraintComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

/**
 * 흐느적이 몸·걸음새·맞는 느낌. 전부 Tuning.csv (Body.*, Wobbler.*, Gait.*, Walk.*, Guard.*, Sling.*).
 * 매 프레임 이름으로 찾지 않도록 한 번 읽어 두고, yjj.ReloadData 로 다시 읽으면 갱신한다.
 */
struct FYJJWobblerTuning
{
	// 몸 치수 (크기 1 기준 cm, 몸통 가운데가 원점)
	float TorsoRadius = 0.f, TorsoHalf = 0.f, HeadRadius = 0.f, ArmRadius = 0.f, ArmHalf = 0.f, LegRadius = 0.f, LegHalf = 0.f;
	float HoverHeight = 0.f;
	float ShoulderInset = 0.f, ShoulderDrop = 0.f, HipWidth = 0.f, HipRaise = 0.f, HeadSink = 0.f, NeckDrop = 0.f, LimbOverlap = 0.f;
	// 몸 무게(kg)·관절 한계(도)·감쇠
	float TorsoMass = 0.f, HeadMass = 0.f, ArmMass = 0.f, LegMass = 0.f;
	float NeckSwing = 0.f, NeckTwist = 0.f, ShoulderSwing = 0.f, ShoulderTwist = 0.f, HipSwing = 0.f, HipTwist = 0.f;
	float LinearDamping = 0.f, LimbAngularDamping = 0.f, CoreAngularDamping = 0.f, MaxSpin = 0.f;
	float Friction = 0.f, Bounciness = 0.f;

	// 버티는 힘·충격
	float HoverStiffness = 0.f, HoverDamping = 0.f, UprightStiffness = 0.f, UprightDamping = 0.f, YawStiffness = 0.f, YawDamping = 0.f, FacingTurn = 0.f;
	float MoveGain = 0.f, MoveMaxAccel = 0.f, JoltThreshold = 0.f, StumbleThreshold = 0.f, ImpactDamage = 0.f, GetUpTime = 0.f, WindExponent = 0.f;
	float GroundReach = 0.f, LiftMin = 0.f, LiftMax = 0.f, GetUpSpeed = 0.f, StuckTime = 0.f, AirTime = 0.f, FallLimp = 0.f, ImpactHeight = 0.f;
	float SettleTime = 0.f, KnockWindow = 0.f, StumbleScale = 0.f, StumbleMin = 0.f, StumbleMax = 0.f, KnockSpin = 0.f, KnockSpinMax = 0.f;
	float LimpWind = 0.f, BlownTime = 0.f, DeadTime = 0.f, DeadPale = 0.f, DarkShade = 0.f;

	// 걸음새
	float Stride = 0.f, WalkSpeed = 0.f, Sway = 0.f, SwayBlown = 0.f, Lean = 0.f, BlownFirm = 0.f;
	float LegSwing = 0.f, LegStiffness = 0.f, LegDamping = 0.f, ArmStiffness = 0.f, ArmDamping = 0.f, ClubArmStiffness = 0.f, LooseArmStiffness = 0.f;
	float HeadStiffness = 0.f, HeadDamping = 0.f, ArmDroop = 0.f, ArmSwing = 0.f, ArmSpread = 0.f;
	float LooseArmSpread = 0.f, ClubPoseForward = 0.f, ClubPoseRight = 0.f, ClubPoseUp = 0.f, ClubSpinUp = 0.f;

	// 길 따라 걷기
	float Lookahead = 0.f, EdgeMargin = 0.f, EdgePerScale = 0.f, EdgeGain = 0.f, EdgeMax = 0.f, BaseReach = 0.f, OffPathMargin = 0.f;

	// 병사
	float PostGain = 0.f, Sight = 0.f, ClubInterval = 0.f, ClubRadius = 0.f, TargetRadius = 0.f, ClubLength = 0.f, ClubLimp = 0.f;
	float SpinTrigger = 0.f, SpinTriggerTarget = 0.f, SpinSpeed = 0.f, SpinGain = 0.f;
	float ClubSide = 0.f, ClubOut = 0.f, ClubUp = 0.f, ClubDamage = 0.f, Dizzy = 0.f;

	// 새총에 붙잡힘 / 탄알
	float HoldGain = 0.f, HoldMaxSpeed = 0.f, HoldSpinDamp = 0.f, KickSpeed = 0.f, KickStrength = 0.f, FlingSpin = 0.f, FlingLimp = 0.f;
	float ProjectileTime = 0.f, BowlMinSpeed = 0.f, BowlReach = 0.f, BowlMaxSpeed = 0.f, BowlForward = 0.f, BowlSide = 0.f, BowlLimp = 0.f;
	float BowlForce = 0.f, BowlUp = 0.f, BowlDamage = 0.f, StrikeCount = 0.f, ReleaseLimp = 0.f, HeldLimp = 0.f;

	void Load();
};

/**
 * 흐느적이: 몸통·머리·팔·다리 여섯 덩어리를 관절로 이은 물리 래그돌.
 * 애니메이션이 없다. 몸통을 띄우는 스프링과 바로 세우는 토크, 다리를 흔드는 토크만으로 비틀비틀 걷는다.
 * 세게 맞으면 힘이 풀려(Limp) 그냥 래그돌이 되어 날아가고, 땅에 닿아 멈추면 다시 일어난다.
 * 모양은 Parts.csv (Wobbler + Wobbler.<종류>), 수치는 Enemies.csv + Tuning.csv.
 */
UCLASS()
class YJJWACKY_API AYJJWobbler : public AActor
{
	GENERATED_BODY()

public:
	AYJJWobbler();

	/** Enemies.csv 한 줄 */
	static const FYJJEnemyRow& Spec(EYJJWobblerKind Kind);

	/** 스폰 직후 한 번. Lateral은 길 가운데에서 벗어나 걷는 거리(적), Post는 지키는 자리(병사), Level은 병사 강화 단계 */
	void Setup(EYJJWobblerKind InKind, float InLateral, const FVector& InPost, int32 InLevel = 1);

	virtual void Tick(float DeltaTime) override;

	/** 몸 전체 속도를 바꿔 날려 보낸다 (cm/s). 무거운 놈은 덜 날아간다 */
	void Knock(const FVector& VelocityChange, float LimpSeconds, float Damage);

	/** 선풍기 바람처럼 계속 미는 가속 (이번 프레임만) */
	void Push(const FVector& Acceleration);

	void TakeHit(float Damage);

	/** 새총에 붙잡힘: 버둥거리며 Target 쪽으로 끌려간다 (매 프레임 갱신) */
	void Hold(const FVector& Target);
	/** 붙잡힌 채로 놓아준다 (새총이 사라졌을 때) */
	void Release();
	/** 탄알이 되어 날아간다. 날아가는 동안 부딪힌 적을 볼링핀처럼 쓰러뜨린다 */
	void Fling(const FVector& Velocity, float Power);
	bool IsHeld() const { return bHeld; }
	bool IsProjectile() const { return ProjectileTime > 0.f; }
	float GetWeight() const { return Spec(Kind).WeightKg; }

	bool IsEnemy() const { return Kind != EYJJWobblerKind::Guard; }
	bool IsAlive() const { return bReady && !bDead; }
	bool IsLimp() const { return bLimp; }
	bool IsStanding() const;
	EYJJWobblerKind GetKind() const { return Kind; }
	float GetScale() const { return S; }
	float GetHealthRatio() const { return FMath::Clamp(Health / FMath::Max(MaxHealth, 1.f), 0.f, 1.f); }
	FVector GetBodyLocation() const;
	FVector GetBodyVelocity() const;
	/** 몸통 회전 속도 (rad/s) */
	FVector GetBodySpin() const;
	/** 길을 따라 얼마나 왔나 (cm) */
	float GetProgress() const { return Progress; }

	/** 전시용: 길을 걷지 않고 Spot 에 서서 Facing 쪽을 본다 (자동 테스트 gallery) */
	void Showcase(const FVector& Spot, const FVector& Facing);

	/** 몽둥이에 연달아 맞지 않게 */
	float LastClubHitTime = TNumericLimits<float>::Lowest();

private:
	/** 지금 Tuning.csv 값 (다시 읽었으면 갱신) */
	static const FYJJWobblerTuning& Tun();
	static UPhysicalMaterial* BodyMaterial();
	static void ConfigureBody(UPrimitiveComponent* Body, float MassKg, bool bLimb);

	UPrimitiveComponent* AddBody(bool bSphere, const FVector& LocalCenter, float Radius, float HalfHeight, float MassKg, bool bLimb);
	void AddJoint(UPrimitiveComponent* Child, const FVector& LocalPivot, float SwingDeg, float TwistDeg);
	void BuildLooks();
	void Align(UPrimitiveComponent* Body, const FVector& Current, const FVector& Desired, float Stiffness, float Damping, const FVector& ReferenceAngularVelocity);
	void GoLimp(float Seconds);
	void Die(bool bDrowned);
	void TickGuard(float DeltaTime, const FVector& Location, FVector& OutDesiredVelocity, FVector& OutFacing);
	void TickEyes(float DeltaTime);
	void TickHeld(float DeltaTime);
	void TickProjectile(float DeltaTime, const FVector& Location, const FVector& Velocity);
	void SetAllVelocity(const FVector& Velocity);

	static FYJJWobblerTuning CachedTuning;
	static int32 CachedGeneration;
	static TWeakObjectPtr<UPhysicalMaterial> CachedMaterial;
	static constexpr const TCHAR* MaterialName = TEXT("YJJWobblerBody");

	// 눈동자: 머리 움직임을 한 박자 늦게 따라가며 덜렁거린다 (모양)
	static constexpr float PupilVelocityFollow = 0.016f;
	static constexpr float PupilGravity = 2.5f;
	static constexpr float PupilMaxOffset = 5.5f;
	static constexpr float PupilCatchUp = 14.f;
	/** 관절 꼬임 축(X)을 아래로 돌리는 각도 */
	static constexpr float JointPitch = -90.f;
	static constexpr int32 Limbs = 2;
	static constexpr float Gravity = 980.f;

	UPROPERTY()
	TObjectPtr<UCapsuleComponent> Torso;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Head;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Arms[Limbs];

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Legs[Limbs];

	UPROPERTY()
	TArray<TObjectPtr<UPhysicsConstraintComponent>> Joints;

	/** 죽으면 색이 바래는 몸 조각들 */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Skin;
	/** Skin 부품의 본래 색 (같은 순서) */
	TArray<FLinearColor> SkinColors;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Pupils;

	TArray<FVector> PupilBase;

	EYJJWobblerKind Kind = EYJJWobblerKind::Grunt;
	float S = 1.f;				// 크기 배율
	float TotalMass = 0.f;
	float Health = 0.f;
	float MaxHealth = 0.f;
	float Lateral = 0.f;
	FVector Post = FVector::ZeroVector;
	float Progress = 0.f;
	bool bShowcase = false;
	FVector ShowFacing = FVector::ForwardVector;
	/** 실제로 몸을 돌리는 방향: 가고 싶은 방향을 천천히 따라간다 (목표가 흔들려도 몸은 안 흔들리게) */
	FVector SmoothFacing = FVector::ZeroVector;

	bool bReady = false;
	bool bDead = false;
	bool bLimp = false;
	float LimpTime = 0.f;
	float Balance = 1.f;		// 0 = 힘 풀림, 1 = 온 힘으로 버팀
	float AirTime = 0.f;
	float DeadTime = 0.f;
	float Age = 0.f;
	float Phase = 0.f;			// 걸음 박자
	float Blown = 0.f;			// 바람에 휘청이는 시간
	float KnockedTime = TNumericLimits<float>::Max();	// 타워에 마지막으로 맞고 흐른 시간
	FVector PrevVelocity = FVector::ZeroVector;
	FVector PendingPush = FVector::ZeroVector;
	FVector2D PupilOffset = FVector2D::ZeroVector;

	// 새총
	bool bHeld = false;
	FVector HoldTarget = FVector::ZeroVector;
	float HeldTime = 0.f;
	float ProjectileTime = 0.f;
	float ProjectilePower = 1.f;
	TArray<TWeakObjectPtr<AYJJWobbler>> Bowled;

	// 병사
	int32 Level = 1;
	bool bSpinning = false;
	float SpinTime = 0.f;
	float SpinCooldown = 0.f;
	float Dizzy = 0.f;
};
