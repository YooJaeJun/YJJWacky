#include "Physics/YJJWobbler.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YJJAudio.h"
#include "Core/YJJGame.h"
#include "Data/YJJLoc.h"
#include "Engine/World.h"
#include "Gameplay/YJJEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "World/YJJMap.h"
#include "World/YJJShapes.h"
#include "YJJWacky.h"

FYJJWobblerTuning AYJJWobbler::CachedTuning;
int32 AYJJWobbler::CachedGeneration = INDEX_NONE;
TWeakObjectPtr<UPhysicalMaterial> AYJJWobbler::CachedMaterial;

void FYJJWobblerTuning::Load()
{
	TorsoRadius = YJJData::Tune(TEXT("Body.TorsoRadius"));
	TorsoHalf = YJJData::Tune(TEXT("Body.TorsoHalf"));
	HeadRadius = YJJData::Tune(TEXT("Body.HeadRadius"));
	ArmRadius = YJJData::Tune(TEXT("Body.ArmRadius"));
	ArmHalf = YJJData::Tune(TEXT("Body.ArmHalf"));
	LegRadius = YJJData::Tune(TEXT("Body.LegRadius"));
	LegHalf = YJJData::Tune(TEXT("Body.LegHalf"));
	HoverHeight = YJJData::Tune(TEXT("Body.HoverHeight"));
	ShoulderInset = YJJData::Tune(TEXT("Body.ShoulderInset"));
	ShoulderDrop = YJJData::Tune(TEXT("Body.ShoulderDrop"));
	HipWidth = YJJData::Tune(TEXT("Body.HipWidth"));
	HipRaise = YJJData::Tune(TEXT("Body.HipRaise"));
	HeadSink = YJJData::Tune(TEXT("Body.HeadSink"));
	NeckDrop = YJJData::Tune(TEXT("Body.NeckDrop"));
	LimbOverlap = YJJData::Tune(TEXT("Body.LimbOverlap"));
	TorsoMass = YJJData::Tune(TEXT("Body.TorsoMass"));
	HeadMass = YJJData::Tune(TEXT("Body.HeadMass"));
	ArmMass = YJJData::Tune(TEXT("Body.ArmMass"));
	LegMass = YJJData::Tune(TEXT("Body.LegMass"));
	NeckSwing = YJJData::Tune(TEXT("Body.NeckSwing"));
	NeckTwist = YJJData::Tune(TEXT("Body.NeckTwist"));
	ShoulderSwing = YJJData::Tune(TEXT("Body.ShoulderSwing"));
	ShoulderTwist = YJJData::Tune(TEXT("Body.ShoulderTwist"));
	HipSwing = YJJData::Tune(TEXT("Body.HipSwing"));
	HipTwist = YJJData::Tune(TEXT("Body.HipTwist"));
	LinearDamping = YJJData::Tune(TEXT("Body.LinearDamping"));
	LimbAngularDamping = YJJData::Tune(TEXT("Body.LimbAngularDamping"));
	CoreAngularDamping = YJJData::Tune(TEXT("Body.CoreAngularDamping"));
	MaxSpin = YJJData::Tune(TEXT("Body.MaxSpin"));
	Friction = YJJData::Tune(TEXT("Wobbler.Friction"));
	Bounciness = YJJData::Tune(TEXT("Wobbler.Bounciness"));

	HoverStiffness = YJJData::Tune(TEXT("Wobbler.HoverStiffness"));
	HoverDamping = YJJData::Tune(TEXT("Wobbler.HoverDamping"));
	UprightStiffness = YJJData::Tune(TEXT("Wobbler.UprightStiffness"));
	UprightDamping = YJJData::Tune(TEXT("Wobbler.UprightDamping"));
	YawStiffness = YJJData::Tune(TEXT("Wobbler.YawStiffness"));
	MoveGain = YJJData::Tune(TEXT("Wobbler.MoveGain"));
	MoveMaxAccel = YJJData::Tune(TEXT("Wobbler.MoveMaxAccel"));
	JoltThreshold = YJJData::Tune(TEXT("Wobbler.JoltThreshold"));
	StumbleThreshold = YJJData::Tune(TEXT("Wobbler.StumbleThreshold"));
	ImpactDamage = YJJData::Tune(TEXT("Wobbler.ImpactDamage"));
	GetUpTime = YJJData::Tune(TEXT("Wobbler.GetUpTime"));
	WindExponent = YJJData::Tune(TEXT("Wobbler.WindExponent"));
	GroundReach = YJJData::Tune(TEXT("Wobbler.GroundReach"));
	LiftMin = YJJData::Tune(TEXT("Wobbler.LiftMin"));
	LiftMax = YJJData::Tune(TEXT("Wobbler.LiftMax"));
	GetUpSpeed = YJJData::Tune(TEXT("Wobbler.GetUpSpeed"));
	StuckTime = YJJData::Tune(TEXT("Wobbler.StuckTime"));
	AirTime = YJJData::Tune(TEXT("Wobbler.AirTime"));
	FallLimp = YJJData::Tune(TEXT("Wobbler.FallLimp"));
	ImpactHeight = YJJData::Tune(TEXT("Wobbler.ImpactHeight"));
	SettleTime = YJJData::Tune(TEXT("Wobbler.SettleTime"));
	KnockWindow = YJJData::Tune(TEXT("Wobbler.KnockWindow"));
	StumbleScale = YJJData::Tune(TEXT("Wobbler.StumbleScale"));
	StumbleMin = YJJData::Tune(TEXT("Wobbler.StumbleMin"));
	StumbleMax = YJJData::Tune(TEXT("Wobbler.StumbleMax"));
	KnockSpin = YJJData::Tune(TEXT("Wobbler.KnockSpin"));
	KnockSpinMax = YJJData::Tune(TEXT("Wobbler.KnockSpinMax"));
	LimpWind = YJJData::Tune(TEXT("Wobbler.LimpWind"));
	BlownTime = YJJData::Tune(TEXT("Wobbler.BlownTime"));
	DeadTime = YJJData::Tune(TEXT("Wobbler.DeadTime"));
	DeadPale = YJJData::Tune(TEXT("Wobbler.DeadPale"));
	DarkShade = YJJData::Tune(TEXT("Wobbler.DarkShade"));

	StepBase = YJJData::Tune(TEXT("Gait.StepBase"));
	StepPerSpeed = YJJData::Tune(TEXT("Gait.StepPerSpeed"));
	WalkSpeed = YJJData::Tune(TEXT("Gait.WalkSpeed"));
	WalkMin = YJJData::Tune(TEXT("Gait.WalkMin"));
	Sway = YJJData::Tune(TEXT("Gait.Sway"));
	SwayBlown = YJJData::Tune(TEXT("Gait.SwayBlown"));
	Lean = YJJData::Tune(TEXT("Gait.Lean"));
	BlownFirm = YJJData::Tune(TEXT("Gait.BlownFirm"));
	LegSwing = YJJData::Tune(TEXT("Gait.LegSwing"));
	LegStiffness = YJJData::Tune(TEXT("Gait.LegStiffness"));
	LegDamping = YJJData::Tune(TEXT("Gait.LegDamping"));
	ArmStiffness = YJJData::Tune(TEXT("Gait.ArmStiffness"));
	ArmDamping = YJJData::Tune(TEXT("Gait.ArmDamping"));
	ClubArmStiffness = YJJData::Tune(TEXT("Gait.ClubArmStiffness"));
	LooseArmStiffness = YJJData::Tune(TEXT("Gait.LooseArmStiffness"));
	HeadStiffness = YJJData::Tune(TEXT("Gait.HeadStiffness"));
	HeadDamping = YJJData::Tune(TEXT("Gait.HeadDamping"));
	ArmReach = YJJData::Tune(TEXT("Gait.ArmReach"));
	ArmLift = YJJData::Tune(TEXT("Gait.ArmLift"));
	ArmFlail = YJJData::Tune(TEXT("Gait.ArmFlail"));
	ArmFlailSpeed = YJJData::Tune(TEXT("Gait.ArmFlailSpeed"));
	ArmPhase = YJJData::Tune(TEXT("Gait.ArmPhase"));
	ArmSpread = YJJData::Tune(TEXT("Gait.ArmSpread"));
	LooseArmSpread = YJJData::Tune(TEXT("Gait.LooseArmSpread"));
	ClubPoseForward = YJJData::Tune(TEXT("Gait.ClubPoseForward"));
	ClubPoseRight = YJJData::Tune(TEXT("Gait.ClubPoseRight"));
	ClubPoseUp = YJJData::Tune(TEXT("Gait.ClubPoseUp"));
	ClubSpinUp = YJJData::Tune(TEXT("Gait.ClubSpinUp"));

	Lookahead = YJJData::Tune(TEXT("Walk.Lookahead"));
	EdgeMargin = YJJData::Tune(TEXT("Walk.EdgeMargin"));
	EdgePerScale = YJJData::Tune(TEXT("Walk.EdgePerScale"));
	EdgeGain = YJJData::Tune(TEXT("Walk.EdgeGain"));
	EdgeMax = YJJData::Tune(TEXT("Walk.EdgeMax"));
	BaseReach = YJJData::Tune(TEXT("Walk.BaseReach"));
	OffPathMargin = YJJData::Tune(TEXT("Walk.OffPathMargin"));

	PostGain = YJJData::Tune(TEXT("Guard.PostGain"));
	Sight = YJJData::Tune(TEXT("Guard.Sight"));
	ClubInterval = YJJData::Tune(TEXT("Guard.ClubInterval"));
	ClubRadius = YJJData::Tune(TEXT("Guard.ClubRadius"));
	TargetRadius = YJJData::Tune(TEXT("Guard.TargetRadius"));
	ClubLength = YJJData::Tune(TEXT("Guard.ClubLength"));
	ClubLimp = YJJData::Tune(TEXT("Guard.ClubLimp"));
	SpinTrigger = YJJData::Tune(TEXT("Guard.SpinTrigger"));
	SpinTriggerTarget = YJJData::Tune(TEXT("Guard.SpinTriggerTarget"));
	SpinSpeed = YJJData::Tune(TEXT("Guard.SpinSpeed"));
	SpinGain = YJJData::Tune(TEXT("Guard.SpinGain"));
	ClubSide = YJJData::Tune(TEXT("Guard.ClubSide"));
	ClubOut = YJJData::Tune(TEXT("Guard.ClubOut"));
	ClubUp = YJJData::Tune(TEXT("Guard.ClubUp"));
	ClubDamage = YJJData::Tune(TEXT("Guard.ClubDamage"));
	Dizzy = YJJData::Tune(TEXT("Guard.Dizzy"));

	HoldGain = YJJData::Tune(TEXT("Sling.HoldGain"));
	HoldMaxSpeed = YJJData::Tune(TEXT("Sling.HoldMaxSpeed"));
	HoldSpinDamp = YJJData::Tune(TEXT("Sling.HoldSpinDamp"));
	KickSpeed = YJJData::Tune(TEXT("Sling.KickSpeed"));
	KickStrength = YJJData::Tune(TEXT("Sling.KickStrength"));
	FlingSpin = YJJData::Tune(TEXT("Sling.FlingSpin"));
	FlingLimp = YJJData::Tune(TEXT("Sling.FlingLimp"));
	ProjectileTime = YJJData::Tune(TEXT("Sling.ProjectileTime"));
	BowlMinSpeed = YJJData::Tune(TEXT("Sling.BowlMinSpeed"));
	BowlReach = YJJData::Tune(TEXT("Sling.BowlReach"));
	BowlMaxSpeed = YJJData::Tune(TEXT("Sling.BowlMaxSpeed"));
	BowlForward = YJJData::Tune(TEXT("Sling.BowlForward"));
	BowlSide = YJJData::Tune(TEXT("Sling.BowlSide"));
	BowlLimp = YJJData::Tune(TEXT("Sling.BowlLimp"));
	BowlForce = YJJData::Tune(TEXT("Sling.BowlForce"));
	BowlUp = YJJData::Tune(TEXT("Sling.BowlUp"));
	BowlDamage = YJJData::Tune(TEXT("Sling.BowlDamage"));
	StrikeCount = YJJData::Tune(TEXT("Sling.StrikeCount"));
	ReleaseLimp = YJJData::Tune(TEXT("Sling.ReleaseLimp"));
	HeldLimp = YJJData::Tune(TEXT("Sling.HeldLimp"));
}

const FYJJWobblerTuning& AYJJWobbler::Tun()
{
	if (CachedGeneration != YJJData::Generation())
	{
		CachedGeneration = YJJData::Generation();
		CachedTuning.Load();
	}
	return CachedTuning;
}

UPhysicalMaterial* AYJJWobbler::BodyMaterial()
{
	if (!CachedMaterial.IsValid())
	{
		UPhysicalMaterial* Material = NewObject<UPhysicalMaterial>(GetTransientPackage(), MaterialName);
		Material->AddToRoot();
		CachedMaterial = Material;
	}
	// 통통 튀어야 웃기다 (다시 읽으면 바로 반영)
	CachedMaterial->Friction = Tun().Friction;
	CachedMaterial->Restitution = Tun().Bounciness;
	return CachedMaterial.Get();
}

const FYJJEnemyRow& AYJJWobbler::Spec(EYJJWobblerKind Kind)
{
	return YJJData::Enemy(Kind);
}

AYJJWobbler::AYJJWobbler()
{
	PrimaryActorTick.bCanEverTick = true;

	Torso = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Torso"));
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
	// 거의 다 일어섰고 몸통이 거의 똑바로
	return IsAlive() && !bLimp && Balance >= 1.f && Torso->GetUpVector().Z > UE_HALF_SQRT_2;
}

// ============================================================ 조립

void AYJJWobbler::ConfigureBody(UPrimitiveComponent* Body, float MassKg, bool bLimb)
{
	const FYJJWobblerTuning& T = Tun();
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
	Body->SetLinearDamping(T.LinearDamping);
	Body->SetAngularDamping(bLimb ? T.LimbAngularDamping : T.CoreAngularDamping);
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
	const float Mass = MassKg * S * S * S;
	ConfigureBody(Body, Mass, bLimb);
	Body->RegisterComponent();
	Body->SetSimulatePhysics(true);
	Body->SetPhysicsMaxAngularVelocityInRadians(Tun().MaxSpin);
	TotalMass += Mass;
	return Body;
}

void AYJJWobbler::AddJoint(UPrimitiveComponent* Child, const FVector& LocalPivot, float SwingDeg, float TwistDeg)
{
	UPhysicsConstraintComponent* Joint = NewObject<UPhysicsConstraintComponent>(this);
	Joint->SetupAttachment(Torso);
	Joint->SetRelativeLocation(LocalPivot * S);
	// 관절의 꼬임 축(X)이 팔다리를 따라 아래를 보게
	Joint->SetRelativeRotation(FRotator(JointPitch, 0.f, 0.f));
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

void AYJJWobbler::Setup(EYJJWobblerKind InKind, float InLateral, const FVector& InPost, int32 InLevel)
{
	const FYJJWobblerTuning& T = Tun();
	Kind = InKind;
	Level = FMath::Clamp(InLevel, 1, YJJMaxTowerLevel);
	Lateral = InLateral;
	Post = InPost;
	const FYJJEnemyRow& Info = Spec(Kind);
	S = Info.Scale;
	Health = MaxHealth = Info.Health;
	Phase = FMath::FRand() * TWO_PI;

	const float Cube = S * S * S;
	Torso->SetCapsuleSize(T.TorsoRadius * S, T.TorsoHalf * S);
	ConfigureBody(Torso, T.TorsoMass * Cube, false);
	Torso->SetSimulatePhysics(true);
	Torso->SetPhysicsMaxAngularVelocityInRadians(T.MaxSpin);
	TotalMass = T.TorsoMass * Cube;

	const float ShoulderY = T.TorsoRadius + T.ArmRadius - T.ShoulderInset;
	const float ShoulderZ = T.TorsoHalf - T.ShoulderDrop;
	const float HipZ = -T.TorsoHalf + T.HipRaise;

	Head = AddBody(true, FVector(0, 0, T.TorsoHalf + T.HeadRadius - T.HeadSink), T.HeadRadius, 0.f, T.HeadMass, false);
	AddJoint(Head, FVector(0, 0, T.TorsoHalf - T.NeckDrop), T.NeckSwing, T.NeckTwist);
	for (int32 i = 0; i < Limbs; ++i)
	{
		const float Side = i == 0 ? -1.f : 1.f;
		Arms[i] = AddBody(false, FVector(0, Side * ShoulderY, ShoulderZ - T.ArmHalf + T.LimbOverlap), T.ArmRadius, T.ArmHalf, T.ArmMass, true);
		AddJoint(Arms[i], FVector(0, Side * ShoulderY, ShoulderZ), T.ShoulderSwing, T.ShoulderTwist);
		Legs[i] = AddBody(false, FVector(0, Side * T.HipWidth, HipZ - T.LegHalf + T.LimbOverlap), T.LegRadius, T.LegHalf, T.LegMass, true);
		AddJoint(Legs[i], FVector(0, Side * T.HipWidth, HipZ), T.HipSwing, T.HipTwist);
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
	// Parts.csv: 모두 같은 몸(Wobbler) + 종류별 모자(Wobbler.<종류>)
	FYJJModelContext Context;
	Context.Parents.Add(TEXT("Torso"), Torso);
	Context.Parents.Add(TEXT("Head"), Head);
	Context.Parents.Add(TEXT("Arm0"), Arms[0]);
	Context.Parents.Add(TEXT("Arm1"), Arms[1]);
	Context.Parents.Add(TEXT("Leg0"), Legs[0]);
	Context.Parents.Add(TEXT("Leg1"), Legs[1]);
	Context.Scale = S;
	Context.Level = Level;
	Context.ClubScale = YJJData::Tower(EYJJTowerType::Guard).ParamA(Level);
	Context.Body = Spec(Kind).LinearColor();
	Context.Dark = YJJShapes::Shade(Context.Body, Tun().DarkShade);

	const FYJJModelParts Body = YJJShapes::BuildModel(this, TEXT("Wobbler"), Context);
	const FString Hat = FString(TEXT("Wobbler.")) + StaticEnum<EYJJWobblerKind>()->GetNameStringByValue(static_cast<int64>(Kind));
	const FYJJModelParts HatParts = YJJData::HasParts(Hat) ? YJJShapes::BuildModel(this, Hat, Context) : FYJJModelParts();

	Skin.Append(Body.Skin);
	Skin.Append(HatParts.Skin);
	Pupils = Body.Meshes(TEXT("Pupil"));
	PupilBase.Reset();
	for (const UStaticMeshComponent* Pupil : Pupils)
	{
		PupilBase.Add(Pupil->GetRelativeLocation());
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
	const FYJJWobblerTuning& T = Tun();
	const float Heft = Spec(Kind).Heft();
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
	Torso->AddAngularImpulseInRadians(FMath::VRand() * FMath::Min(Change.Size() * T.KnockSpin, T.KnockSpinMax), NAME_None, true);
	PrevVelocity += Change;	// 내가 준 속도 변화는 충격으로 세지 않는다
	KnockedTime = 0.f;
	GoLimp(LimpSeconds / FMath::Sqrt(Heft));
	TakeHit(Damage);
}

void AYJJWobbler::Push(const FVector& Acceleration)
{
	PendingPush += Acceleration / FMath::Pow(Spec(Kind).Heft(), Tun().WindExponent);
	Blown = Tun().BlownTime;
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

// ============================================================ 새총

void AYJJWobbler::SetAllVelocity(const FVector& NewVelocity)
{
	UPrimitiveComponent* Bodies[] = { Torso, Head, Arms[0], Arms[1], Legs[0], Legs[1] };
	for (UPrimitiveComponent* Body : Bodies)
	{
		if (Body)
		{
			Body->SetPhysicsLinearVelocity(NewVelocity);
		}
	}
}

void AYJJWobbler::Hold(const FVector& Target)
{
	if (!bHeld)
	{
		bHeld = true;
		HeldTime = 0.f;
		bSpinning = false;
		GoLimp(Tun().HeldLimp);
	}
	HoldTarget = Target;
}

void AYJJWobbler::Release()
{
	if (bHeld)
	{
		bHeld = false;
		LimpTime = Tun().ReleaseLimp;
		KnockedTime = 0.f;
	}
}

void AYJJWobbler::Fling(const FVector& NewVelocity, float Power)
{
	const FYJJWobblerTuning& T = Tun();
	bHeld = false;
	SetAllVelocity(NewVelocity);
	// 날아가며 빙글빙글
	Torso->SetPhysicsAngularVelocityInRadians(FMath::VRand() * T.FlingSpin);
	PrevVelocity = NewVelocity;
	KnockedTime = 0.f;
	GoLimp(T.FlingLimp);
	LimpTime = T.FlingLimp;
	ProjectileTime = T.ProjectileTime;
	ProjectilePower = Power;
	Bowled.Reset();
}

void AYJJWobbler::TickHeld(float DeltaTime)
{
	const FYJJWobblerTuning& T = Tun();
	HeldTime += DeltaTime;
	bLimp = true;
	LimpTime = FMath::Max(LimpTime, T.ReleaseLimp);
	// 몸통만 끌어당긴다. 팔다리는 관절에 매달려 덜렁덜렁
	const FVector To = HoldTarget - Torso->GetComponentLocation();
	Torso->SetPhysicsLinearVelocity((To * T.HoldGain).GetClampedToMaxSize(T.HoldMaxSpeed));
	Torso->SetPhysicsAngularVelocityInRadians(Torso->GetPhysicsAngularVelocityInRadians() * T.HoldSpinDamp);
	// 버둥버둥
	for (int32 i = 0; i < Limbs; ++i)
	{
		const float Kick = FMath::Sin(Age * T.KickSpeed + i * PI) * T.KickStrength;
		Legs[i]->AddAngularImpulseInRadians(Torso->GetRightVector() * Kick * DeltaTime, NAME_None, true);
		Arms[i]->AddAngularImpulseInRadians(Torso->GetForwardVector() * Kick * DeltaTime * (i ? 1.f : -1.f), NAME_None, true);
	}
}

void AYJJWobbler::TickProjectile(float DeltaTime, const FVector& Location, const FVector& Velocity)
{
	const FYJJWobblerTuning& T = Tun();
	ProjectileTime -= DeltaTime;
	const float Speed = Velocity.Size();
	UYJJGame* Game = UYJJGame::Get(this);
	if (!Game || Speed < T.BowlMinSpeed)
	{
		return;
	}
	// 날아가는 동안 닿은 적은 볼링핀처럼 쓰러진다. 무거운 탄알일수록 세다
	const FVector Dir = Velocity.GetSafeNormal2D();
	const float MyHeft = FMath::Sqrt(Spec(Kind).Heft());
	int32 NewHits = 0;
	const TArray<TObjectPtr<AYJJWobbler>> Targets = Game->GetEnemies();
	for (AYJJWobbler* Other : Targets)
	{
		if (!Other || Other == this || !Other->IsAlive() || Other->IsHeld() || Bowled.Contains(Other))
		{
			continue;
		}
		const FVector OtherLocation = Other->GetBodyLocation();
		if (FVector::DistSquared(OtherLocation, Location) < FMath::Square(T.BowlReach * (S + Other->GetScale())))
		{
			Bowled.Add(Other);
			NewHits++;
			const FVector Side = (OtherLocation - Location).GetSafeNormal2D();
			const float Force = FMath::Min(Speed, T.BowlMaxSpeed) * T.BowlForce * MyHeft * ProjectilePower;
			Other->Knock((Dir * T.BowlForward + Side * T.BowlSide).GetSafeNormal() * Force + FVector(0, 0, T.BowlUp * ProjectilePower), T.BowlLimp,
				T.BowlDamage * MyHeft * ProjectilePower);
			AYJJEffects::Burst(this, TEXT("Bowl"), OtherLocation);
			YJJAudio::Play(this, TEXT("Bowl"), OtherLocation);
		}
	}
	const int32 Strike = FMath::RoundToInt(T.StrikeCount);
	if (NewHits > 0 && Bowled.Num() >= Strike && Bowled.Num() - NewHits < Strike)
	{
		Game->AddFloater(Location, YJJLoc::Get(TEXT("Float.Strike")), TEXT("Strike"));
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
		const FLinearColor Pale = FMath::Lerp(Spec(Kind).LinearColor(), YJJData::Color(TEXT("Pale")), Tun().DeadPale);
		UMaterialInterface* Material = YJJShapes::Solid(this, Pale);
		for (UStaticMeshComponent* Mesh : Skin)
		{
			if (Mesh)
			{
				Mesh->SetMaterial(0, Material);
			}
		}
		YJJAudio::Play(this, TEXT("Pop"), GetBodyLocation(), 1.f / FMath::Sqrt(S));
	}
	const FVector At = GetBodyLocation();
	UE_LOG(LogYJJWacky, Verbose, TEXT("[Wobbler] down kind=%d drowned=%d at (%.0f, %.0f, %.0f) age=%.1f progress=%.0f"),
		static_cast<int32>(Kind), bDrowned ? 1 : 0, At.X, At.Y, At.Z, Age, Progress);
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
	const FYJJWobblerTuning& T = Tun();
	UYJJGame* Game = UYJJGame::Get(this);
	const FVector Location = Torso->GetComponentLocation();
	const FVector Velocity = Torso->GetPhysicsLinearVelocity();
	Age += DeltaTime;

	// 풍덩
	if (Location.Z < YJJMap::KillZ())
	{
		const FVector Surface(Location.X, Location.Y, YJJMap::WaterZ());
		AYJJEffects::Burst(this, TEXT("Splash"), Surface, S);
		AYJJEffects::Burst(this, TEXT("SplashMist"), Surface, S);
		YJJAudio::Play(this, TEXT("Splash"), Surface, 1.f / FMath::Sqrt(S));
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

	if (bHeld && !bDead)
	{
		TickHeld(DeltaTime);
		PrevVelocity = Torso->GetPhysicsLinearVelocity();
		return;
	}
	if (ProjectileTime > 0.f)
	{
		TickProjectile(DeltaTime, Location, Velocity);
	}

	if (bDead)
	{
		DeadTime += DeltaTime;
		if (DeadTime > T.DeadTime)
		{
			AYJJEffects::Burst(this, TEXT("Vanish"), Location, S);
			Destroy();
		}
		return;
	}

	// 충격 감지: 내가 준 힘으로는 한 프레임에 이만큼 속도가 바뀌지 않는다
	const float Jolt = (Velocity - PrevVelocity).Size();
	PrevVelocity = Velocity;
	KnockedTime += DeltaTime;

	// 발밑 땅
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(YJJWobblerGround), false, this);
	const float Reach = T.HoverHeight * S * T.GroundReach;
	const bool bGround = GetWorld()->LineTraceSingleByObjectType(Hit, Location, Location - FVector(0, 0, Reach), FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	const float Height = bGround ? Hit.Distance : Reach;

	if (Age > T.SettleTime)
	{
		// 공중에서는 관절이 휘둘리며 몸통 속도가 출렁이므로, 땅에 닿을 만큼 낮을 때만 충돌로 친다
		// (공중에서 체력이 다 깎여 죽으면 물에 빠져도 풍덩으로 안 쳐진다)
		if (bLimp && KnockedTime < T.KnockWindow && Jolt > T.JoltThreshold && bGround && Height < T.ImpactHeight * S)
		{
			// 날아가다 땅에 처박혔다
			AYJJEffects::Burst(this, TEXT("Impact"), Location, S);
			TakeHit((Jolt - T.JoltThreshold) * T.ImpactDamage / Spec(Kind).Heft());
			if (bDead)
			{
				return;
			}
		}
		else if (!bLimp && Jolt > T.StumbleThreshold)
		{
			// 서 있다가 날아온 놈에게 치였다: 다치진 않고 나동그라진다 (도미노)
			GoLimp(FMath::Clamp(Jolt / T.StumbleScale, T.StumbleMin, T.StumbleMax) / FMath::Sqrt(Spec(Kind).Heft()));
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
		Torso->AddForce(PendingPush * TotalMass * T.LimpWind);
		PendingPush = FVector::ZeroVector;
		if (LimpTime <= 0.f && bGround && Velocity.SizeSquared() < FMath::Square(T.GetUpSpeed))
		{
			// 비틀비틀 다시 일어난다
			bLimp = false;
			Balance = 0.f;
			AirTime = 0.f;
		}
		else
		{
			// 어디 끼어서 영영 못 일어나는 놈은 정리
			if (IsEnemy() && LimpTime < -T.StuckTime)
			{
				Die(false);
			}
			return;
		}
	}
	Balance = FMath::Min(1.f, Balance + DeltaTime / FMath::Max(T.GetUpTime, KINDA_SMALL_NUMBER));

	if (bGround)
	{
		AirTime = 0.f;
	}
	else
	{
		// 발밑이 허공이면 버둥거리다 떨어진다
		AirTime += DeltaTime;
		if (AirTime > T.AirTime)
		{
			GoLimp(T.FallLimp);
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
		if (Progress >= YJJMap::PathLength() - T.BaseReach)
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
		FVector Carrot = YJJMap::PointAt(Progress + T.Lookahead, &Dir);
		// 길 위에 있으면 자기 줄을 지키고, 길 밖(발판 위)으로 밀려났으면 길 가운데로 돌아온다
		const float HalfWidth = YJJMap::PathWidth() * 0.5f;
		const bool bOnPath = FMath::Abs(Offset) < HalfWidth + T.OffPathMargin;
		Carrot += FVector(-Dir.Y, Dir.X, 0.f) * (bOnPath ? Lateral : 0.f);
		const FVector To = (Carrot - Location).GetSafeNormal2D();
		DesiredVelocity = To * Spec(Kind).Speed;
		Facing = To;
		// 가장자리에서는 길 안쪽으로 버틴다 (자기들끼리 밀려서 우수수 떨어지지 않게)
		const float Edge = FMath::Abs(Offset) - (HalfWidth - T.EdgeMargin - T.EdgePerScale * S);
		if (bOnPath && Edge > 0.f)
		{
			FVector Along;
			YJJMap::PointAt(Progress, &Along);
			DesiredVelocity -= FVector(-Along.Y, Along.X, 0.f) * FMath::Sign(Offset) * FMath::Min(Edge * T.EdgeGain, T.EdgeMax);
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
		Lift = FMath::Clamp(T.HoverStiffness * (T.HoverHeight * S - Height) - T.HoverDamping * Velocity.Z, T.LiftMin, T.LiftMax) + Gravity;
	}
	const FVector Move = ((DesiredVelocity - Velocity2D) * T.MoveGain).GetClampedToMaxSize(T.MoveMaxAccel);
	Torso->AddForce((FVector(0, 0, Lift) + Move) * TotalMass * Balance + PendingPush * TotalMass);
	PendingPush = FVector::ZeroVector;

	// 걸음 박자: 빨리 걸을수록 빨리 뒤뚱거린다
	const float Speed2D = Velocity2D.Size();
	Phase += DeltaTime * (T.StepBase + Speed2D * T.StepPerSpeed / FMath::Sqrt(S));
	const float Walk = FMath::Clamp(Speed2D / T.WalkSpeed, T.WalkMin, 1.f);
	Blown = FMath::Max(0.f, Blown - DeltaTime);
	Dizzy = FMath::Max(0.f, Dizzy - DeltaTime);

	// 몸통 세우기. 목표 자세 자체를 좌우로 기울여서 뒤뚱뒤뚱 걷게 한다
	const bool bWobbly = Blown > 0.f || Dizzy > 0.f;
	const FVector Right(-Facing.Y, Facing.X, 0.f);
	const float Sway = bWobbly ? T.SwayBlown : T.Sway;
	const FVector UpTarget = (FVector::UpVector + Right * Sway * FMath::Sin(Phase) * Walk + Facing * T.Lean * Walk).GetSafeNormal();
	const FVector Up = Torso->GetUpVector();
	const FVector Spin = Torso->GetPhysicsAngularVelocityInRadians();
	const float Firm = bWobbly ? T.BlownFirm : 1.f;
	Align(Torso, Up, UpTarget, T.UprightStiffness * Firm, T.UprightDamping, FVector::ZeroVector);

	// 가는 쪽 보기
	const FVector Forward = Torso->GetForwardVector();
	if (bSpinning)
	{
		// 몽둥이 회오리
		Torso->AddTorqueInRadians(FVector(0, 0, (T.SpinSpeed - Spin.Z) * T.SpinGain) * Balance, NAME_None, true);
	}
	else
	{
		const float YawError = FMath::FindDeltaAngleRadians(FMath::Atan2(Forward.Y, Forward.X), FMath::Atan2(Facing.Y, Facing.X));
		Torso->AddTorqueInRadians(FVector(0, 0, YawError * T.YawStiffness) * Balance, NAME_None, true);
	}

	// 다리: 번갈아 앞뒤로
	for (int32 i = 0; i < Limbs; ++i)
	{
		const float Swing = FMath::Sin(Phase + i * PI) * T.LegSwing * Walk;
		const FVector Target = (Up * FMath::Cos(Swing) - Forward * FMath::Sin(Swing)).GetSafeNormal();
		Align(Legs[i], Legs[i]->GetUpVector(), Target, T.LegStiffness, T.LegDamping, Spin);
	}

	// 팔
	const FVector BodyRight = Torso->GetRightVector();
	for (int32 i = 0; i < Limbs; ++i)
	{
		const float Side = i == 0 ? -1.f : 1.f;
		FVector Hand;
		float Stiffness = T.ArmStiffness;
		if (IsEnemy())
		{
			// 좀비처럼 앞으로 뻗고 위아래로 휘적휘적
			Hand = Forward * T.ArmReach + Up * (T.ArmLift + T.ArmFlail * FMath::Sin(Phase * T.ArmFlailSpeed + i * T.ArmPhase)) + BodyRight * Side * T.ArmSpread;
		}
		else if (i == 1)
		{
			// 몽둥이 든 손: 돌 때는 옆으로 쭉, 평소엔 앞으로 치켜든다
			Hand = bSpinning ? BodyRight + Up * T.ClubSpinUp : Forward * T.ClubPoseForward + BodyRight * T.ClubPoseRight + Up * T.ClubPoseUp;
			Stiffness = T.ClubArmStiffness;
		}
		else
		{
			Hand = -Up + BodyRight * Side * T.LooseArmSpread;
			Stiffness = T.LooseArmStiffness;
		}
		Align(Arms[i], Arms[i]->GetUpVector(), -Hand.GetSafeNormal(), Stiffness, T.ArmDamping, Spin);
	}

	// 머리: 목이 물러서 까딱까딱
	Align(Head, Head->GetUpVector(), Up, T.HeadStiffness, T.HeadDamping, Spin);
}

void AYJJWobbler::TickGuard(float DeltaTime, const FVector& Location, FVector& OutDesiredVelocity, FVector& OutFacing)
{
	const FYJJWobblerTuning& T = Tun();
	const FYJJTowerRow& Row = YJJData::Tower(EYJJTowerType::Guard);
	UYJJGame* Game = UYJJGame::Get(this);
	// 자리로 돌아가려는 힘
	const FVector ToPost = FVector(Post.X - Location.X, Post.Y - Location.Y, 0.f);
	OutDesiredVelocity = (ToPost * T.PostGain).GetClampedToMaxSize(Spec(Kind).Speed);

	AYJJWobbler* Target = Game ? Game->FindNearestEnemy(Location, T.Sight * S) : nullptr;
	if (Target)
	{
		OutFacing = (Target->GetBodyLocation() - Location).GetSafeNormal2D();
	}

	SpinCooldown -= DeltaTime;
	if (bSpinning)
	{
		SpinTime -= DeltaTime;
		// 몽둥이(손에서 끝까지)에 닿은 적을 날린다
		const FVector Hand = Arms[1]->GetComponentLocation() - Arms[1]->GetUpVector() * T.ArmHalf * S;
		const FVector Tip = Hand - Arms[1]->GetUpVector() * T.ClubLength * S * Row.ParamA(Level);
		const float Now = GetWorld()->GetTimeSeconds();
		if (Game)
		{
			const TArray<TObjectPtr<AYJJWobbler>> Targets = Game->GetEnemies();	// 맞고 죽으면 목록이 바뀌므로 복사
			for (AYJJWobbler* Enemy : Targets)
			{
				if (!Enemy || !Enemy->IsAlive() || Now - Enemy->LastClubHitTime < T.ClubInterval)
				{
					continue;
				}
				const FVector P = Enemy->GetBodyLocation();
				const float HitReach = T.ClubRadius * S + T.TargetRadius * Enemy->GetScale();
				if (FMath::PointDistToSegmentSquared(P, Hand, Tip) < FMath::Square(HitReach))
				{
					Enemy->LastClubHitTime = Now;
					// 몽둥이가 휘둘리는 방향 + 바깥쪽
					const FVector Out = (P - Location).GetSafeNormal2D();
					const FVector Tangent(-Out.Y, Out.X, 0.f);
					const float Power = Row.Power(Level);
					const FVector Club = Tangent * T.ClubSide + Out * T.ClubOut + FVector(0, 0, T.ClubUp);
					Enemy->Knock(Club * Power, T.ClubLimp, T.ClubDamage * Power);
					AYJJEffects::Burst(this, TEXT("Club"), P);
					YJJAudio::Play(this, TEXT("Club"), P);
				}
			}
		}
		if (SpinTime <= 0.f)
		{
			bSpinning = false;
			SpinCooldown = Row.Cooldown(Level);
			Dizzy = T.Dizzy;	// 돌고 나면 어지럽다
		}
	}
	else if (Target && SpinCooldown <= 0.f
		&& FVector::DistSquared2D(Target->GetBodyLocation(), Location) < FMath::Square(T.SpinTrigger * S + T.SpinTriggerTarget * Target->GetScale()))
	{
		bSpinning = true;
		SpinTime = Row.ParamB(Level);
		YJJAudio::Play(this, TEXT("Spin"), Location);
	}
}

void AYJJWobbler::TickEyes(float DeltaTime)
{
	if (!Head || Pupils.Num() == 0)
	{
		return;
	}
	// 눈동자는 머리 움직임을 한 박자 늦게 따라가며 덜렁거린다
	const FTransform HeadTransform = Head->GetComponentTransform();
	const FVector LocalVelocity = HeadTransform.InverseTransformVectorNoScale(Head->GetPhysicsLinearVelocity());
	const FVector LocalDown = HeadTransform.InverseTransformVectorNoScale(FVector::DownVector);
	FVector2D Target(-LocalVelocity.Y * PupilVelocityFollow + LocalDown.Y * PupilGravity, -LocalVelocity.Z * PupilVelocityFollow + LocalDown.Z * PupilGravity);
	Target = Target.GetSafeNormal() * FMath::Min(Target.Size(), PupilMaxOffset);
	PupilOffset = FMath::Lerp(PupilOffset, Target, FMath::Clamp(DeltaTime * PupilCatchUp, 0.f, 1.f));
	for (int32 i = 0; i < Pupils.Num() && i < PupilBase.Num(); ++i)
	{
		Pupils[i]->SetRelativeLocation(PupilBase[i] + FVector(0, PupilOffset.X, PupilOffset.Y) * S);
	}
}
