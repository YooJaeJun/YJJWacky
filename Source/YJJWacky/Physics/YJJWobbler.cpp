#include "Physics/YJJWobbler.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YJJAudio.h"
#include "Core/YJJGame.h"
#include "Engine/World.h"
#include "Gameplay/YJJEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "World/YJJMap.h"
#include "World/YJJShapes.h"
#include "YJJWacky.h"

namespace
{
	// 크기 1 기준 몸 치수 (cm). 몸통 가운데가 원점
	constexpr float TorsoRadius = 30.f;
	constexpr float TorsoHalf = 44.f;
	constexpr float HeadRadius = 27.f;
	constexpr float ArmRadius = 9.f;
	constexpr float ArmHalf = 32.f;
	constexpr float LegRadius = 11.f;
	constexpr float LegHalf = 26.f;
	/** 서 있을 때 몸통 가운데 높이 */
	constexpr float HoverHeight = 92.f;

	// 버티는 힘. 일부러 무르게 잡아서 비틀거린다
	constexpr float HoverStiffness = 190.f;		// 1/s²
	constexpr float HoverDamping = 15.f;		// 1/s
	constexpr float UprightStiffness = 150.f;
	constexpr float UprightDamping = 9.f;
	constexpr float YawStiffness = 45.f;
	constexpr float MoveGain = 5.f;
	constexpr float MoveMaxAccel = 1300.f;

	/** 한 프레임에 이만큼 속도가 바뀌면 "세게 맞았다" */
	constexpr float JoltThreshold = 330.f;
	/** 서 있는 놈이 나동그라지는 충격 */
	constexpr float StumbleThreshold = 560.f;

	UPhysicalMaterial* BodyMaterial()
	{
		static TWeakObjectPtr<UPhysicalMaterial> Cached;
		if (!Cached.IsValid())
		{
			UPhysicalMaterial* Material = NewObject<UPhysicalMaterial>(GetTransientPackage(), TEXT("YJJWobblerBody"));
			Material->Friction = 0.35f;
			Material->Restitution = 0.3f;	// 통통 튀어야 웃기다
			Material->AddToRoot();
			Cached = Material;
		}
		return Cached.Get();
	}
}

const FYJJWobblerSpec& AYJJWobbler::Spec(EYJJWobblerKind Kind)
{
	//                                   크기   체력   속도  현상금 깨물기 무게   색
	static const FYJJWobblerSpec Grunt{ 1.2f, 30.f, 190.f, 5, 1, 1.f, 0xE5484D };
	static const FYJJWobblerSpec Runner{ 0.9f, 16.f, 310.f, 6, 1, 0.6f, 0xF47B3E };
	static const FYJJWobblerSpec Brute{ 1.7f, 110.f, 135.f, 14, 2, 2.6f, 0xB3262E };
	static const FYJJWobblerSpec Boss{ 2.6f, 480.f, 110.f, 70, 6, 7.f, 0x8E1F3A };
	static const FYJJWobblerSpec Guard{ 1.35f, 1.f, 260.f, 0, 0, 1.6f, 0x3B82F6 };
	switch (Kind)
	{
	case EYJJWobblerKind::Runner: return Runner;
	case EYJJWobblerKind::Brute: return Brute;
	case EYJJWobblerKind::Boss: return Boss;
	case EYJJWobblerKind::Guard: return Guard;
	default: return Grunt;
	}
}

AYJJWobbler::AYJJWobbler()
{
	PrimaryActorTick.bCanEverTick = true;

	Torso = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Torso"));
	Torso->InitCapsuleSize(TorsoRadius, TorsoHalf);
	Torso->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Torso->SetCanEverAffectNavigation(false);
	RootComponent = Torso;
}

FVector AYJJWobbler::GetBodyLocation() const
{
	return Torso->GetComponentLocation();
}

FVector AYJJWobbler::GetBodyVelocity() const
{
	return Torso->GetPhysicsLinearVelocity();
}

bool AYJJWobbler::IsStanding() const
{
	return IsAlive() && !bLimp && Balance > 0.9f && Torso->GetUpVector().Z > 0.7f;
}

// ============================================================ 조립

static void ConfigureBody(UPrimitiveComponent* Body, float MassKg, bool bLimb)
{
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// 팔다리끼리, 팔다리와 남의 몸은 서로 통과한다 (접촉 수를 줄이고 덜 엉킨다). 몸통·머리끼리는 부딪혀서 서로 밀친다
	Body->SetCollisionObjectType(bLimb ? ECC_Destructible : ECC_PhysicsBody);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Body->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	if (!bLimb)
	{
		Body->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	}
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	Body->SetPhysMaterialOverride(BodyMaterial());
	Body->SetMassOverrideInKg(NAME_None, MassKg, true);
	Body->SetLinearDamping(0.05f);
	Body->SetAngularDamping(bLimb ? 1.2f : 0.4f);
}

UPrimitiveComponent* AYJJWobbler::AddBody(bool bSphere, const FVector& LocalCenter, float Radius, float HalfHeight, float MassKg, bool bLimb)
{
	const FTransform Root = Torso->GetComponentTransform();
	UPrimitiveComponent* Body = nullptr;
	if (bSphere)
	{
		USphereComponent* Sphere = NewObject<USphereComponent>(this);
		Sphere->InitSphereRadius(Radius * S);
		Body = Sphere;
	}
	else
	{
		UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(this);
		Capsule->InitCapsuleSize(Radius * S, HalfHeight * S);
		Body = Capsule;
	}
	Body->SetWorldLocationAndRotation(Root.TransformPosition(LocalCenter * S), Root.GetRotation());
	ConfigureBody(Body, MassKg * S * S * S, bLimb);
	Body->RegisterComponent();
	Body->SetSimulatePhysics(true);
	Body->SetPhysicsMaxAngularVelocityInRadians(25.f);
	TotalMass += MassKg * S * S * S;
	return Body;
}

void AYJJWobbler::AddJoint(UPrimitiveComponent* Child, const FVector& LocalPivot, float SwingDeg, float TwistDeg)
{
	UPhysicsConstraintComponent* Joint = NewObject<UPhysicsConstraintComponent>(this);
	Joint->SetupAttachment(Torso);
	Joint->SetRelativeLocation(LocalPivot * S);
	// 관절의 꼬임 축(X)이 팔다리를 따라 아래를 보게
	Joint->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Joint->SetDisableCollision(true);
	Joint->SetLinearXLimit(LCM_Locked, 0.f);
	Joint->SetLinearYLimit(LCM_Locked, 0.f);
	Joint->SetLinearZLimit(LCM_Locked, 0.f);
	Joint->SetAngularSwing1Limit(ACM_Limited, SwingDeg);
	Joint->SetAngularSwing2Limit(ACM_Limited, SwingDeg);
	Joint->SetAngularTwistLimit(ACM_Limited, TwistDeg);
	Joint->RegisterComponent();
	Joint->SetConstrainedComponents(Torso, NAME_None, Child, NAME_None);
	Joints.Add(Joint);
}

void AYJJWobbler::Setup(EYJJWobblerKind InKind, float InLateral, const FVector& InPost)
{
	Kind = InKind;
	Lateral = InLateral;
	Post = InPost;
	const FYJJWobblerSpec& Info = Spec(Kind);
	S = Info.Scale;
	Health = MaxHealth = Info.Health;
	Phase = FMath::FRand() * TWO_PI;

	const float Cube = S * S * S;
	Torso->SetCapsuleSize(TorsoRadius * S, TorsoHalf * S);
	ConfigureBody(Torso, 28.f * Cube, false);
	Torso->SetSimulatePhysics(true);
	Torso->SetPhysicsMaxAngularVelocityInRadians(25.f);
	TotalMass = 28.f * Cube;

	const float ShoulderY = TorsoRadius + ArmRadius - 3.f;
	const float ShoulderZ = TorsoHalf - 14.f;
	const float HipY = 15.f;
	const float HipZ = -TorsoHalf + 6.f;

	Head = AddBody(true, FVector(0, 0, TorsoHalf + HeadRadius - 7.f), HeadRadius, 0.f, 6.f, false);
	AddJoint(Head, FVector(0, 0, TorsoHalf - 2.f), 38.f, 35.f);
	for (int32 i = 0; i < 2; ++i)
	{
		const float Side = i == 0 ? -1.f : 1.f;
		Arms[i] = AddBody(false, FVector(0, Side * ShoulderY, ShoulderZ - ArmHalf + 4.f), ArmRadius, ArmHalf, 3.f, true);
		AddJoint(Arms[i], FVector(0, Side * ShoulderY, ShoulderZ), 170.f, 60.f);
		Legs[i] = AddBody(false, FVector(0, Side * HipY, HipZ - LegHalf + 4.f), LegRadius, LegHalf, 5.f, true);
		AddJoint(Legs[i], FVector(0, Side * HipY, HipZ), 75.f, 25.f);
	}

	BuildLooks();
	PrevVelocity = FVector::ZeroVector;
	bReady = true;

	if (UYJJGame* Game = UYJJGame::Get(this))
	{
		Game->RegisterWobbler(this);
	}
}

void AYJJWobbler::BuildLooks()
{
	const FLinearColor Color = YJJShapes::Hex(Spec(Kind).ColorHex);
	UMaterialInterface* Body = YJJShapes::Solid(this, Color);
	UMaterialInterface* Dark = YJJShapes::Solid(this, YJJShapes::Shade(Color, 0.72f));
	UMaterialInterface* White = YJJShapes::Solid(this, YJJShapes::Hex(0xFFFFFF));
	UMaterialInterface* Black = YJJShapes::Solid(this, YJJShapes::Hex(0x15151A));

	auto Piece = [this](USceneComponent* Parent, EYJJShape Shape, const FVector& Location, const FVector& Size, UMaterialInterface* Material, bool bSkin, bool bShadow = true)
	{
		UStaticMeshComponent* Mesh = YJJShapes::Add(this, Parent, Shape, Location * S, Size * S, Material);
		Mesh->SetCastShadow(bShadow);
		if (bSkin)
		{
			Skin.Add(Mesh);
		}
		return Mesh;
	};

	// 콩 모양 몸통, 동그란 머리
	Piece(Torso, EYJJShape::Sphere, FVector::ZeroVector, FVector(66, 60, 100), Body, true);
	Piece(Head, EYJJShape::Sphere, FVector::ZeroVector, FVector(58, 58, 56), Body, true);

	// 왕눈이 눈: 흰자 + 덜렁거리는 눈동자, 멍하게 벌린 입
	for (int32 i = 0; i < 2; ++i)
	{
		const float Side = i == 0 ? -1.f : 1.f;
		Piece(Head, EYJJShape::Sphere, FVector(19, Side * 11.5f, 5), FVector(21, 21, 23), White, false, false);
		PupilBase[i] = FVector(28.5f, Side * 11.5f, 5) * S;
		Pupils[i] = Piece(Head, EYJJShape::Sphere, FVector(28.5f, Side * 11.5f, 5), FVector(8, 10, 10), Black, false, false);
	}
	Piece(Head, EYJJShape::Sphere, FVector(25, 0, -12), FVector(8, 15, 9), Black, false, false);

	for (int32 i = 0; i < 2; ++i)
	{
		Piece(Arms[i], EYJJShape::Cylinder, FVector::ZeroVector, FVector(16, 16, 60), Body, true);
		Piece(Arms[i], EYJJShape::Sphere, FVector(0, 0, -ArmHalf), FVector(23, 23, 23), Dark, true);
		Piece(Legs[i], EYJJShape::Cylinder, FVector::ZeroVector, FVector(20, 20, 48), Body, true);
		Piece(Legs[i], EYJJShape::Sphere, FVector(7, 0, -LegHalf + 2.f), FVector(32, 23, 17), Dark, true);
	}

	// 종류별 모자
	switch (Kind)
	{
	case EYJJWobblerKind::Runner:
		Piece(Head, EYJJShape::Cone, FVector(0, 0, 38), FVector(30, 30, 40), YJJShapes::Solid(this, YJJShapes::Hex(0xFFD23F)), false);
		break;
	case EYJJWobblerKind::Brute:
		Piece(Head, EYJJShape::Sphere, FVector(0, 0, 13), FVector(66, 66, 42), YJJShapes::Solid(this, YJJShapes::Hex(0x8B929C)), false);
		Piece(Head, EYJJShape::Sphere, FVector(0, 0, 36), FVector(14, 14, 14), YJJShapes::Solid(this, YJJShapes::Hex(0x6B7280)), false);
		break;
	case EYJJWobblerKind::Boss:
	{
		UMaterialInterface* Gold = YJJShapes::Solid(this, YJJShapes::Hex(0xFFC83D));
		Piece(Head, EYJJShape::Cylinder, FVector(0, 0, 27), FVector(46, 46, 14), Gold, false);
		for (int32 i = 0; i < 5; ++i)
		{
			const float A = i * TWO_PI / 5.f;
			Piece(Head, EYJJShape::Cone, FVector(FMath::Cos(A) * 18.f, FMath::Sin(A) * 18.f, 41), FVector(13, 13, 18), Gold, false, false);
		}
		break;
	}
	case EYJJWobblerKind::Guard:
	{
		// 양동이 투구 + 오른손 몽둥이
		Piece(Head, EYJJShape::Cylinder, FVector(0, 0, 19), FVector(64, 64, 30), YJJShapes::Solid(this, YJJShapes::Hex(0xCBD5E1)), false);
		UMaterialInterface* Wood = YJJShapes::Solid(this, YJJShapes::Hex(0x9A6B3F));
		Piece(Arms[1], EYJJShape::Cylinder, FVector(0, 0, -ArmHalf - 55.f), FVector(15, 15, 120), Wood, false);
		Piece(Arms[1], EYJJShape::Sphere, FVector(0, 0, -ArmHalf - 112.f), FVector(40, 40, 46), Wood, false);
		break;
	}
	default:
		break;
	}
}

// ============================================================ 맞기

void AYJJWobbler::GoLimp(float Seconds)
{
	bLimp = true;
	Balance = 0.f;
	LimpTime = FMath::Max(LimpTime, Seconds);
	bSpinning = false;
}

void AYJJWobbler::Knock(const FVector& VelocityChange, float LimpSeconds, float Damage)
{
	if (!bReady)
	{
		return;
	}
	const float Heft = Spec(Kind).Heft;
	const FVector Change = VelocityChange / Heft;
	UPrimitiveComponent* Bodies[] = { Torso, Head, Arms[0], Arms[1], Legs[0], Legs[1] };
	for (UPrimitiveComponent* Body : Bodies)
	{
		if (Body)
		{
			Body->AddImpulse(Change, NAME_None, true);
		}
	}
	// 빙글빙글 돌면서 날아가야 제맛
	Torso->AddAngularImpulseInRadians(FMath::VRand() * FMath::Min(Change.Size() * 0.008f, 9.f), NAME_None, true);
	PrevVelocity += Change;	// 내가 준 속도 변화는 충격으로 세지 않는다
	KnockedTime = 0.f;
	GoLimp(LimpSeconds / FMath::Sqrt(Heft));
	TakeHit(Damage);
}

void AYJJWobbler::Push(const FVector& Acceleration)
{
	PendingPush += Acceleration / FMath::Sqrt(Spec(Kind).Heft);
	Blown = 0.3f;
}

void AYJJWobbler::TakeHit(float Damage)
{
	if (bDead || !IsEnemy() || Damage <= 0.f)
	{
		return;
	}
	Health -= Damage;
	if (Health <= 0.f)
	{
		Die(false);
	}
}

void AYJJWobbler::Die(bool bDrowned)
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	bLimp = true;
	Balance = 0.f;
	if (!bDrowned)
	{
		// 축 늘어지고 색이 바랜다
		const FLinearColor Color = YJJShapes::Hex(Spec(Kind).ColorHex);
		const FLinearColor Pale = FMath::Lerp(Color, YJJShapes::Hex(0xB9B4AE), 0.65f);
		UMaterialInterface* Material = YJJShapes::Solid(this, Pale);
		for (UStaticMeshComponent* Mesh : Skin)
		{
			if (Mesh)
			{
				Mesh->SetMaterial(0, Material);
			}
		}
		YJJAudio::Play(this, EYJJSound::Pop, GetBodyLocation(), 0.8f, FMath::FRandRange(0.85f, 1.25f) / FMath::Sqrt(S));
	}
	const FVector At = GetBodyLocation();
	UE_LOG(LogYJJWacky, Verbose, TEXT("[Wobbler] down kind=%d drowned=%d at (%.0f, %.0f, %.0f) age=%.1f knocked=%.1f progress=%.0f"),
		static_cast<int32>(Kind), bDrowned ? 1 : 0, At.X, At.Y, At.Z, Age, KnockedTime, Progress);
	if (UYJJGame* Game = UYJJGame::Get(this))
	{
		Game->OnWobblerDown(this, bDrowned);
	}
}

// ============================================================ 매 프레임

void AYJJWobbler::Align(UPrimitiveComponent* Body, const FVector& Current, const FVector& Desired, float Stiffness, float Damping, const FVector& ReferenceAngularVelocity)
{
	// Current 축을 Desired 축으로 돌리는 스프링 토크 (각가속도 단위라 덩어리 무게와 상관없다)
	const FVector Cross = FVector::CrossProduct(Current, Desired);
	const float Sine = Cross.Size();
	const float Angle = FMath::Atan2(Sine, FVector::DotProduct(Current, Desired));
	const FVector Axis = Sine > KINDA_SMALL_NUMBER ? Cross / Sine : FVector::ZeroVector;
	const FVector Alpha = Axis * Angle * Stiffness - (Body->GetPhysicsAngularVelocityInRadians() - ReferenceAngularVelocity) * Damping;
	Body->AddTorqueInRadians(Alpha * Balance, NAME_None, true);
}

void AYJJWobbler::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bReady || DeltaTime <= 0.f)
	{
		return;
	}
	UYJJGame* Game = UYJJGame::Get(this);
	const FVector Location = Torso->GetComponentLocation();
	const FVector Velocity = Torso->GetPhysicsLinearVelocity();
	Age += DeltaTime;

	// 풍덩
	if (Location.Z < YJJMap::KillZ)
	{
		const FVector Surface(Location.X, Location.Y, YJJMap::WaterZ);
		AYJJEffects::Burst(this, Surface, YJJShapes::Hex(0xE8F7FF), 10 + FMath::RoundToInt(6 * S), 520.f * FMath::Sqrt(S), 34.f * S, 0.9f, 1500.f, false, 0.9f);
		AYJJEffects::Burst(this, Surface, YJJShapes::Hex(0x8FDBF5), 8, 300.f, 46.f * S, 0.7f, 900.f, false, 0.35f);
		YJJAudio::Play(this, EYJJSound::Splash, Surface, 0.9f, FMath::FRandRange(0.9f, 1.15f) / FMath::Sqrt(S));
		if (IsEnemy())
		{
			Die(true);
		}
		else if (Game)
		{
			Game->OnWobblerDown(this, true);
		}
		Destroy();
		return;
	}

	TickEyes(DeltaTime);

	if (bDead)
	{
		DeadTime += DeltaTime;
		if (DeadTime > 2.6f)
		{
			AYJJEffects::Burst(this, Location, YJJShapes::Hex(0xFFFFFF), 7, 240.f, 40.f * S, 0.45f, -150.f, false, 0.6f);
			Destroy();
		}
		return;
	}

	// 충격 감지: 내가 준 힘으로는 한 프레임에 이만큼 속도가 바뀌지 않는다
	const float Jolt = (Velocity - PrevVelocity).Size();
	PrevVelocity = Velocity;
	KnockedTime += DeltaTime;

	// 진단: 아무도 안 쳤는데 빨라졌다면 물리가 튄 것
	if (!bLaunchLogged && KnockedTime > 3.f && FVector(Velocity.X, Velocity.Y, 0.f).Size() > 650.f)
	{
		bLaunchLogged = true;
		float Nearest = 100000.f;
		if (const UYJJGame* G = UYJJGame::Get(this))
		{
			for (const AYJJWobbler* Other : G->GetEnemies())
			{
				if (Other && Other != this)
				{
					Nearest = FMath::Min(Nearest, FVector::Dist(Other->GetBodyLocation(), Location));
				}
			}
		}
		const FVector HeadV = Head ? Head->GetPhysicsLinearVelocity() : FVector::ZeroVector;
		const FVector LegV = Legs[0] ? Legs[0]->GetPhysicsLinearVelocity() : FVector::ZeroVector;
		UE_LOG(LogYJJWacky, Verbose, TEXT("[Wobbler] launch kind=%d age=%.2f at (%.0f, %.0f, %.0f) vel (%.0f, %.0f, %.0f) jolt=%.0f limp=%d bal=%.2f spin=%.1f head=%.0f leg=%.0f nearest=%.0f"),
			static_cast<int32>(Kind), Age, Location.X, Location.Y, Location.Z, Velocity.X, Velocity.Y, Velocity.Z, Jolt, bLimp ? 1 : 0, Balance,
			Torso->GetPhysicsAngularVelocityInRadians().Size(), HeadV.Size(), LegV.Size(), Nearest);
	}
	// 발밑 땅
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YJJWobblerGround), false, this);
	const float Reach = HoverHeight * S * 1.6f;
	const bool bGround = GetWorld()->LineTraceSingleByObjectType(Hit, Location, Location - FVector(0, 0, Reach), FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	const float Height = bGround ? Hit.Distance : Reach;

	if (Age > 1.2f)
	{
		// 공중에서는 관절이 휘둘리며 몸통 속도가 출렁이므로, 땅에 닿을 만큼 낮을 때만 충돌로 친다
		// (공중에서 체력이 다 깎여 죽으면 물에 빠져도 풍덩으로 안 쳐진다)
		if (bLimp && KnockedTime < 5.f && Jolt > JoltThreshold && bGround && Height < 70.f * S)
		{
			// 날아가다 땅에 처박혔다
			AYJJEffects::Burst(this, Location + FVector(0, 0, 40.f * S), YJJShapes::Hex(0xFFE066), 4, 380.f, 20.f, 0.35f, 600.f, true, 0.6f);
			TakeHit((Jolt - JoltThreshold) * 0.03f / Spec(Kind).Heft);
			if (bDead)
			{
				return;
			}
		}
		else if (!bLimp && Jolt > StumbleThreshold)
		{
			// 서 있다가 날아온 놈에게 치였다: 다치진 않고 나동그라진다 (도미노)
			GoLimp(FMath::Clamp(Jolt / 700.f, 0.6f, 1.8f) / FMath::Sqrt(Spec(Kind).Heft));
		}
	}

	if (IsEnemy())
	{
		Progress = YJJMap::Project(Location);
	}

	if (bLimp)
	{
		LimpTime -= DeltaTime;
		// 널브러진 놈도 바람에는 밀린다
		Torso->AddForce(PendingPush * TotalMass * 0.6f);
		PendingPush = FVector::ZeroVector;
		if (LimpTime <= 0.f && bGround && Velocity.SizeSquared() < FMath::Square(260.f))
		{
			// 비틀비틀 다시 일어난다
			bLimp = false;
			Balance = 0.f;
			AirTime = 0.f;
		}
		else
		{
			// 어디 끼어서 영영 못 일어나는 놈은 정리
			if (IsEnemy() && LimpTime < -8.f)
			{
				Die(false);
			}
			return;
		}
	}
	Balance = FMath::Min(1.f, Balance + DeltaTime / 0.7f);

	if (bGround)
	{
		AirTime = 0.f;
	}
	else
	{
		// 발밑이 허공이면 버둥거리다 떨어진다
		AirTime += DeltaTime;
		if (AirTime > 0.3f)
		{
			GoLimp(0.7f);
			return;
		}
	}

	// 어디로 갈까
	FVector DesiredVelocity = FVector::ZeroVector;
	FVector Facing = Torso->GetForwardVector().GetSafeNormal2D();
	if (IsEnemy())
	{
		FVector Closest;
		float Offset = 0.f;
		YJJMap::Project(Location, &Closest, &Offset);
		if (Progress >= YJJMap::PathLength() - 60.f)
		{
			// 푸딩 도착
			if (Game)
			{
				Game->OnEnemyReachedBase(this);
			}
			Destroy();
			return;
		}
		FVector Dir;
		FVector Carrot = YJJMap::PointAt(Progress + 220.f, &Dir);
		// 길 위에 있으면 자기 줄을 지키고, 길 밖(발판 위)으로 밀려났으면 길 가운데로 돌아온다
		const bool bOnPath = FMath::Abs(Offset) < YJJMap::PathWidth * 0.5f + 40.f;
		Carrot += FVector(-Dir.Y, Dir.X, 0.f) * (bOnPath ? Lateral : 0.f);
		const FVector To = (Carrot - Location).GetSafeNormal2D();
		DesiredVelocity = To * Spec(Kind).Speed;
		Facing = To;
		const float Edge = FMath::Abs(Offset) - (YJJMap::PathWidth * 0.5f - 40.f - 30.f * S);
		if (bOnPath && Edge > 0.f)
		{
			FVector Along;
			YJJMap::PointAt(Progress, &Along);
			DesiredVelocity -= FVector(-Along.Y, Along.X, 0.f) * FMath::Sign(Offset) * FMath::Min(Edge * 6.f, 320.f);
		}
	}
	else
	{
		TickGuard(DeltaTime, Location, DesiredVelocity, Facing);
	}

	// 몸통을 띄우는 스프링 + 가고 싶은 쪽으로 미는 힘
	const FVector Velocity2D(Velocity.X, Velocity.Y, 0.f);
	float Lift = 0.f;
	if (bGround)
	{
		Lift = FMath::Clamp(HoverStiffness * (HoverHeight * S - Height) - HoverDamping * Velocity.Z, -600.f, 4500.f) + 980.f;
	}
	const FVector Move = ((DesiredVelocity - Velocity2D) * MoveGain).GetClampedToMaxSize(MoveMaxAccel);
	Torso->AddForce((FVector(0, 0, Lift) + Move) * TotalMass * Balance + PendingPush * TotalMass);
	PendingPush = FVector::ZeroVector;

	// 걸음 박자: 빨리 걸을수록 빨리 뒤뚱거린다
	const float Speed2D = Velocity2D.Size();
	Phase += DeltaTime * (3.f + Speed2D * 0.045f / FMath::Sqrt(S));
	const float Walk = FMath::Clamp(Speed2D / 120.f, 0.25f, 1.f);
	Blown = FMath::Max(0.f, Blown - DeltaTime);
	Dizzy = FMath::Max(0.f, Dizzy - DeltaTime);

	// 몸통 세우기. 목표 자세 자체를 좌우로 기울여서 뒤뚱뒤뚱 걷게 한다
	const FVector Right(-Facing.Y, Facing.X, 0.f);
	const float Sway = (Blown > 0.f || Dizzy > 0.f) ? 0.45f : 0.2f;
	const FVector UpTarget = (FVector::UpVector + Right * Sway * FMath::Sin(Phase) * Walk + Facing * 0.12f * Walk).GetSafeNormal();
	const FVector Up = Torso->GetUpVector();
	const FVector Spin = Torso->GetPhysicsAngularVelocityInRadians();
	const float Firm = (Blown > 0.f || Dizzy > 0.f) ? 0.45f : 1.f;
	Align(Torso, Up, UpTarget, UprightStiffness * Firm, UprightDamping, FVector::ZeroVector);

	// 가는 쪽 보기
	const FVector Forward = Torso->GetForwardVector();
	if (bSpinning)
	{
		// 몽둥이 회오리
		Torso->AddTorqueInRadians(FVector(0, 0, (11.f - Spin.Z) * 14.f) * Balance, NAME_None, true);
	}
	else
	{
		const float YawError = FMath::FindDeltaAngleRadians(FMath::Atan2(Forward.Y, Forward.X), FMath::Atan2(Facing.Y, Facing.X));
		Torso->AddTorqueInRadians(FVector(0, 0, YawError * YawStiffness) * Balance, NAME_None, true);
	}

	// 다리: 번갈아 앞뒤로
	for (int32 i = 0; i < 2; ++i)
	{
		const float Swing = FMath::Sin(Phase + i * PI) * 0.75f * Walk;
		const FVector Target = (Up * FMath::Cos(Swing) - Forward * FMath::Sin(Swing)).GetSafeNormal();
		Align(Legs[i], Legs[i]->GetUpVector(), Target, 260.f, 10.f, Spin);
	}

	// 팔
	const FVector BodyRight = Torso->GetRightVector();
	for (int32 i = 0; i < 2; ++i)
	{
		const float Side = i == 0 ? -1.f : 1.f;
		FVector Hand;
		float Stiffness = 90.f;
		if (IsEnemy())
		{
			// 좀비처럼 앞으로 뻗고 위아래로 휘적휘적
			Hand = Forward * 0.9f + Up * (0.3f + 0.55f * FMath::Sin(Phase * 0.7f + i * 2.1f)) + BodyRight * Side * 0.35f;
		}
		else if (i == 1)
		{
			// 몽둥이 든 손: 돌 때는 옆으로 쭉, 평소엔 앞으로 치켜든다
			Hand = bSpinning ? BodyRight + Up * 0.1f : Forward * 0.6f + BodyRight * 0.5f + Up * 0.6f;
			Stiffness = 320.f;
		}
		else
		{
			Hand = -Up + BodyRight * Side * 0.4f;
			Stiffness = 30.f;
		}
		Align(Arms[i], Arms[i]->GetUpVector(), -Hand.GetSafeNormal(), Stiffness, 7.f, Spin);
	}

	// 머리: 목이 물러서 까딱까딱
	Align(Head, Head->GetUpVector(), Up, 70.f, 4.f, Spin);
}

void AYJJWobbler::TickGuard(float DeltaTime, const FVector& Location, FVector& OutDesiredVelocity, FVector& OutFacing)
{
	UYJJGame* Game = UYJJGame::Get(this);
	// 자리로 돌아가려는 힘
	const FVector ToPost = FVector(Post.X - Location.X, Post.Y - Location.Y, 0.f);
	OutDesiredVelocity = (ToPost * 3.f).GetClampedToMaxSize(Spec(Kind).Speed);

	AYJJWobbler* Target = Game ? Game->FindNearestEnemy(Location, 520.f * S) : nullptr;
	if (Target)
	{
		OutFacing = (Target->GetBodyLocation() - Location).GetSafeNormal2D();
	}

	SpinCooldown -= DeltaTime;
	if (bSpinning)
	{
		SpinTime -= DeltaTime;
		// 몽둥이(손에서 끝까지)에 닿은 적을 날린다
		const FVector Hand = Arms[1]->GetComponentLocation() - Arms[1]->GetUpVector() * ArmHalf * S;
		const FVector Tip = Hand - Arms[1]->GetUpVector() * 125.f * S;
		const float Now = GetWorld()->GetTimeSeconds();
		if (Game)
		{
			const TArray<TObjectPtr<AYJJWobbler>> Targets = Game->GetEnemies();	// 맞고 죽으면 목록이 바뀌므로 복사
			for (AYJJWobbler* Enemy : Targets)
			{
				if (!Enemy || !Enemy->IsAlive() || Now - Enemy->LastClubHitTime < 0.6f)
				{
					continue;
				}
				const FVector P = Enemy->GetBodyLocation();
				const float Reach = 30.f * S + 48.f * Enemy->GetScale();
				if (FMath::PointDistToSegmentSquared(P, Hand, Tip) < FMath::Square(Reach))
				{
					Enemy->LastClubHitTime = Now;
					// 몽둥이가 휘둘리는 방향 + 바깥쪽
					const FVector Out = (P - Location).GetSafeNormal2D();
					const FVector Tangent(-Out.Y, Out.X, 0.f);
					Enemy->Knock(Tangent * 780.f + Out * 420.f + FVector(0, 0, 520.f), 1.6f, 14.f);
					AYJJEffects::Burst(this, P, YJJShapes::Hex(0xFFE066), 6, 420.f, 22.f, 0.4f, 700.f, true, 0.6f);
					YJJAudio::Play(this, EYJJSound::Bonk, P, 0.9f, FMath::FRandRange(0.9f, 1.2f));
				}
			}
		}
		if (SpinTime <= 0.f)
		{
			bSpinning = false;
			SpinCooldown = 1.3f;
			Dizzy = 1.1f;	// 돌고 나면 어지럽다
		}
	}
	else if (Target && SpinCooldown <= 0.f && FVector::DistSquared2D(Target->GetBodyLocation(), Location) < FMath::Square(250.f * S + 60.f * Target->GetScale()))
	{
		bSpinning = true;
		SpinTime = 1.25f;
		YJJAudio::Play(this, EYJJSound::Whoosh, Location, 0.7f);
	}
}

void AYJJWobbler::TickEyes(float DeltaTime)
{
	if (!Head || !Pupils[0])
	{
		return;
	}
	// 눈동자는 머리 움직임을 한 박자 늦게 따라가며 덜렁거린다
	const FTransform HeadTransform = Head->GetComponentTransform();
	const FVector LocalVelocity = HeadTransform.InverseTransformVectorNoScale(Head->GetPhysicsLinearVelocity());
	const FVector LocalDown = HeadTransform.InverseTransformVectorNoScale(FVector(0, 0, -1));
	FVector2D Target(-LocalVelocity.Y * 0.016f + LocalDown.Y * 2.5f, -LocalVelocity.Z * 0.016f + LocalDown.Z * 2.5f);
	Target = Target.GetSafeNormal() * FMath::Min(Target.Size(), 5.5f);
	PupilOffset = FMath::Lerp(PupilOffset, Target, FMath::Clamp(DeltaTime * 14.f, 0.f, 1.f));
	for (int32 i = 0; i < 2; ++i)
	{
		Pupils[i]->SetRelativeLocation(PupilBase[i] + FVector(0, PupilOffset.X, PupilOffset.Y) * S);
	}
}
