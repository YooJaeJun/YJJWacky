#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "YJJWobbler.generated.h"

class UCapsuleComponent;
class UPhysicsConstraintComponent;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;

UENUM()
enum class EYJJWobblerKind : uint8
{
	Grunt,		// 졸병
	Runner,		// 날쌘돌이: 작고 빠르고 가볍다 (멀리 날아간다)
	Brute,		// 뚱보: 크고 무겁고 느리다
	Boss,		// 대장: 거대
	Guard,		// 아군 몽둥이 병사
};

struct FYJJWobblerSpec
{
	float Scale = 1.f;
	float Health = 30.f;
	float Speed = 200.f;
	int32 Bounty = 8;
	int32 Bite = 1;		// 푸딩에 닿았을 때 깎는 체력
	float Heft = 1.f;	// 클수록 덜 날아간다
	uint32 ColorHex = 0xE5484D;
};

/**
 * 흐느적이: 몸통·머리·팔·다리 여섯 덩어리를 관절로 이은 물리 래그돌.
 * 애니메이션이 없다. 몸통을 띄우는 스프링과 바로 세우는 토크, 다리를 흔드는 토크만으로 비틀비틀 걷는다.
 * 세게 맞으면 힘이 풀려(Limp) 그냥 래그돌이 되어 날아가고, 땅에 닿아 멈추면 다시 일어난다.
 */
UCLASS()
class YJJWACKY_API AYJJWobbler : public AActor
{
	GENERATED_BODY()

public:
	AYJJWobbler();

	static const FYJJWobblerSpec& Spec(EYJJWobblerKind Kind);

	/** 스폰 직후 한 번. Lateral은 길 가운데에서 벗어나 걷는 거리(적), Post는 지키는 자리(병사) */
	void Setup(EYJJWobblerKind InKind, float InLateral, const FVector& InPost);

	virtual void Tick(float DeltaTime) override;

	/** 몸 전체 속도를 바꿔 날려 보낸다 (cm/s). 무거운 놈은 덜 날아간다 */
	void Knock(const FVector& VelocityChange, float LimpSeconds, float Damage);

	/** 선풍기 바람처럼 계속 미는 가속 (이번 프레임만) */
	void Push(const FVector& Acceleration);

	void TakeHit(float Damage);

	bool IsEnemy() const { return Kind != EYJJWobblerKind::Guard; }
	bool IsAlive() const { return bReady && !bDead; }
	bool IsLimp() const { return bLimp; }
	bool IsStanding() const;
	EYJJWobblerKind GetKind() const { return Kind; }
	float GetScale() const { return S; }
	float GetHealthRatio() const { return FMath::Clamp(Health / FMath::Max(MaxHealth, 1.f), 0.f, 1.f); }
	FVector GetBodyLocation() const;
	FVector GetBodyVelocity() const;
	/** 길을 따라 얼마나 왔나 (cm) */
	float GetProgress() const { return Progress; }

	/** 몽둥이에 연달아 맞지 않게 */
	float LastClubHitTime = -100.f;

private:
	UPrimitiveComponent* AddBody(bool bSphere, const FVector& LocalCenter, float Radius, float HalfHeight, float MassKg, bool bLimb);
	void AddJoint(UPrimitiveComponent* Child, const FVector& LocalPivot, float SwingDeg, float TwistDeg);
	void BuildLooks();
	void Align(UPrimitiveComponent* Body, const FVector& Current, const FVector& Desired, float Stiffness, float Damping, const FVector& ReferenceAngularVelocity);
	void GoLimp(float Seconds);
	void Die(bool bDrowned);
	void TickGuard(float DeltaTime, const FVector& Location, FVector& OutDesiredVelocity, FVector& OutFacing);
	void TickEyes(float DeltaTime);

	UPROPERTY()
	TObjectPtr<UCapsuleComponent> Torso;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Head;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Arms[2];

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Legs[2];

	UPROPERTY()
	TArray<TObjectPtr<UPhysicsConstraintComponent>> Joints;

	/** 죽으면 색이 바래는 몸 조각들 */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Skin;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Pupils[2];

	EYJJWobblerKind Kind = EYJJWobblerKind::Grunt;
	float S = 1.f;				// 크기 배율
	float TotalMass = 50.f;
	float Health = 30.f;
	float MaxHealth = 30.f;
	float Lateral = 0.f;
	FVector Post = FVector::ZeroVector;
	float Progress = 0.f;

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
	float KnockedTime = 100.f;	// 타워에 마지막으로 맞고 흐른 시간
	bool bLaunchLogged = false;
	FVector PrevVelocity = FVector::ZeroVector;
	FVector PendingPush = FVector::ZeroVector;
	FVector2D PupilOffset = FVector2D::ZeroVector;
	FVector PupilBase[2];

	// 병사
	bool bSpinning = false;
	float SpinTime = 0.f;
	float SpinCooldown = 0.f;
	float Dizzy = 0.f;
};
