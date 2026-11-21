#include "Gameplay/YJJBoulder.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YJJAudio.h"
#include "Core/YJJGame.h"
#include "Data/YJJData.h"
#include "Data/YJJLoc.h"
#include "Gameplay/YJJEffects.h"
#include "Physics/YJJWobbler.h"
#include "World/YJJMap.h"
#include "World/YJJShapes.h"

const FVector AYJJBoulder::LumpScale(1.f, 0.92f, 0.86f);

AYJJBoulder::AYJJBoulder()
{
	PrimaryActorTick.bCanEverTick = true;
	Body = CreateDefaultSubobject<USphereComponent>(TEXT("Body"));
	Body->SetCanEverAffectNavigation(false);
	RootComponent = Body;
	Rock = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rock"));
	Rock->SetupAttachment(Body);
	Rock->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rock->SetCanEverAffectNavigation(false);
}

float AYJJBoulder::Tune(const TCHAR* Name)
{
	return YJJData::Tune(*FString::Printf(TEXT("Spell.Boulder.%s"), Name));
}

void AYJJBoulder::Drop(const FVector& InTarget, const FVector& InRollDir)
{
	Target = InTarget;
	RollDir = InRollDir.GetSafeNormal2D();
	Radius = Tune(TEXT("Radius"));
	RollTime = Tune(TEXT("RollTime"));

	// 각진 공(10x6 면)이라 그대로 울퉁불퉁한 바위처럼 보인다. 판정은 매끈한 공
	Rock->SetStaticMesh(YJJShapes::Mesh(EYJJShape::Sphere));
	Rock->SetRelativeScale3D(LumpScale * (Radius * 2.f / YJJShapes::MeshSize));
	Rock->SetRelativeRotation(FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f));
	Rock->SetMaterial(0, YJJShapes::Solid(this, YJJData::Color(TEXT("Boulder"))));

	Body->SetSphereRadius(Radius);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionObjectType(ECC_WorldDynamic);
	Body->SetCollisionResponseToAllChannels(ECR_Block);
	Body->SetGenerateOverlapEvents(false);
	Body->SetSimulatePhysics(true);
	Body->SetMassOverrideInKg(NAME_None, Tune(TEXT("Mass")), true);
	Body->SetUseCCD(true);
	Body->SetNotifyRigidBodyCollision(true);
	Body->OnComponentHit.AddDynamic(this, &AYJJBoulder::OnHit);
	Body->SetPhysicsLinearVelocity(FVector(0, 0, -Tune(TEXT("DropSpeed"))));
	// 떨어지면서 데굴데굴
	Body->SetPhysicsAngularVelocityInDegrees(FVector::UpVector.Cross(RollDir) * Tune(TEXT("DropSpin")));
}

void AYJJBoulder::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bLanded)
	{
		Slam();
	}
}

void AYJJBoulder::Slam()
{
	bLanded = true;
	const FVector Location = GetActorLocation();
	HitAround(Tune(TEXT("SlamRadius")), FVector::ZeroVector, Tune(TEXT("SlamUp")), Tune(TEXT("SlamDamage")), Tune(TEXT("SlamLimp")));
	AYJJEffects::Burst(this, TEXT("BoulderDust"), Location - FVector(0, 0, Radius));
	AYJJEffects::Burst(this, TEXT("BoulderChips"), Location - FVector(0, 0, Radius));
	YJJAudio::Play(this, TEXT("BoulderSlam"), Location);
	if (UYJJGame* Game = UYJJGame::Get(this))
	{
		Game->AddFloater(Location, YJJLoc::Get(TEXT("Float.Boulder")), TEXT("Boulder"));
	}
	// 땅에 닿으면 길을 거꾸로 굴러간다
	const float Speed = Tune(TEXT("RollSpeed"));
	const FVector Velocity = RollDir * Speed;
	Body->SetPhysicsLinearVelocity(Velocity);
	Body->SetPhysicsAngularVelocityInRadians(FVector::UpVector.Cross(Velocity) / FMath::Max(Radius, KINDA_SMALL_NUMBER));
}

void AYJJBoulder::HitAround(float Reach, const FVector& Push, float Up, float Damage, float Limp)
{
	UYJJGame* Game = UYJJGame::Get(this);
	if (!Game)
	{
		return;
	}
	const FVector Center = GetActorLocation();
	const bool bRadial = Push.IsNearlyZero();
	TArray<TObjectPtr<AYJJWobbler>> Victims = Game->GetEnemies();
	Victims.Append(Game->GetGuards());
	for (AYJJWobbler* Victim : Victims)
	{
		if (!IsValid(Victim) || !Victim->IsAlive())
		{
			continue;
		}
		const FVector To = Victim->GetBodyLocation() - Center;
		const float Dist = To.Size();
		if (Dist > Reach + BodyRadiusPerScale * Victim->GetScale())
		{
			continue;
		}
		const float* Last = LastHit.Find(Victim);
		if (Last && Age - *Last < RehitTime)
		{
			continue;
		}
		LastHit.Add(Victim, Age);
		FVector Side;
		if (bRadial)
		{
			// 쾅: 가운데일수록 세게 사방으로
			const float Falloff = FMath::Clamp(1.f - Dist / FMath::Max(Reach, KINDA_SMALL_NUMBER), Tune(TEXT("SlamMinFalloff")), 1.f);
			Side = To.GetSafeNormal2D() * Tune(TEXT("SlamOut")) * Falloff;
			Victim->Knock(Side + FVector(0, 0, Up * Falloff), Limp, Damage * Falloff);
		}
		else
		{
			// 데굴데굴: 굴러가는 쪽 + 옆으로 튕겨 길 밖으로
			Side = (Push.GetSafeNormal2D() + To.GetSafeNormal2D() * Tune(TEXT("RollSide"))).GetSafeNormal2D() * Push.Size();
			Victim->Knock(Side + FVector(0, 0, Up), Limp, Damage);
			AYJJEffects::Burst(this, TEXT("Bowl"), Victim->GetBodyLocation());
			YJJAudio::Play(this, TEXT("Bowl"), Victim->GetBodyLocation());
		}
	}
}

void AYJJBoulder::Crumble()
{
	AYJJEffects::Burst(this, TEXT("BoulderChips"), GetActorLocation());
	AYJJEffects::Burst(this, TEXT("BoulderDust"), GetActorLocation());
	Destroy();
}

void AYJJBoulder::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Age += DeltaTime;
	const FVector Location = GetActorLocation();
	if (Location.Z < YJJMap::KillZ())
	{
		const FVector Surface(Location.X, Location.Y, YJJMap::WaterZ());
		AYJJEffects::Burst(this, TEXT("Splash"), Surface, Radius / YJJShapes::MeshSize * Tune(TEXT("SplashScale")));
		YJJAudio::Play(this, TEXT("BallSplash"), Surface);
		Destroy();
		return;
	}
	if (!bLanded)
	{
		// 아무것도 안 맞고 땅 높이까지 오면 (판정이 빠졌을 때) 그 자리에서 쾅
		if (Location.Z < Radius + SlamHeight && Age > Tune(TEXT("MinFall")))
		{
			Slam();
		}
		return;
	}
	RollTime -= DeltaTime;
	if (RollTime <= 0.f)
	{
		Crumble();
		return;
	}

	// 땅에 붙어 있으면 길을 거꾸로 따라가려고 밀어 준다 (꺾이는 곳에선 미끄러져 물에 빠지기도)
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	const FVector Flat(Velocity.X, Velocity.Y, 0.f);
	if (Location.Z < Radius * Tune(TEXT("GroundedHeight")))
	{
		const float Along = YJJMap::Project(Location);
		const float Lookahead = Tune(TEXT("Lookahead"));
		FVector Want = RollDir;
		if (Along > Lookahead)
		{
			Want = (YJJMap::PointAt(Along - Lookahead) - Location).GetSafeNormal2D();
			RollDir = Want.IsNearlyZero() ? RollDir : Want;
		}
		const FVector Accel = ((RollDir * Tune(TEXT("RollSpeed")) - Flat) * Tune(TEXT("SteerGain"))).GetClampedToMaxSize(Tune(TEXT("RollAccel")));
		Body->AddForce(Accel, NAME_None, true);
	}
	if (Flat.Size() > Tune(TEXT("HitMinSpeed")))
	{
		HitAround(Radius, Flat.GetSafeNormal() * Tune(TEXT("RollPush")), Tune(TEXT("RollUp")), Tune(TEXT("RollDamage")), Tune(TEXT("RollLimp")));
	}
}
