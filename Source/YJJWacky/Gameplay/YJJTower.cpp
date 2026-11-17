#include "Gameplay/YJJTower.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YJJAudio.h"
#include "Engine/World.h"
#include "Gameplay/YJJEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "World/YJJMap.h"

namespace
{
	constexpr float PunchRest = 70.f;
	constexpr float PunchFull = 570.f;
	constexpr float FanOnTime = 3.5f;
	constexpr float FanPeriod = 5.5f;
	constexpr float FanReach = 900.f;

	USceneComponent* Pivot(AActor* Owner, USceneComponent* Parent, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator)
	{
		USceneComponent* Scene = NewObject<USceneComponent>(Owner);
		Scene->SetupAttachment(Parent);
		Scene->SetRelativeLocation(Location);
		Scene->SetRelativeRotation(Rotation);
		Scene->RegisterComponent();
		return Scene;
	}
}

AYJJTower::AYJJTower()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

UStaticMeshComponent* AYJJTower::Part(USceneComponent* Parent, EYJJShape Shape, const FVector& Location, const FVector& Size, uint32 ColorHex,
	const FRotator& Rotation, bool bCollide)
{
	return YJJShapes::Add(this, Parent, Shape, Location, Size, YJJShapes::Solid(this, YJJShapes::Hex(ColorHex)), bCollide, Rotation);
}

void AYJJTower::Setup(EYJJTowerType InType, int32 InPad)
{
	Type = InType;
	Pad = InPad;
	switch (Type)
	{
	case EYJJTowerType::Punch: BuildPunch(); break;
	case EYJJTowerType::Cannon: BuildCannon(); break;
	case EYJJTowerType::Fan: BuildFan(); break;
	case EYJJTowerType::Guard: BuildGuardPost(); break;
	}
	Cooldown = 0.4f;
	bReady = true;
}

void AYJJTower::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bReady)
	{
		return;
	}
	switch (Type)
	{
	case EYJJTowerType::Punch: TickPunch(DeltaTime); break;
	case EYJJTowerType::Cannon: TickCannon(DeltaTime); break;
	case EYJJTowerType::Fan: TickFan(DeltaTime); break;
	default: break;
	}
}

// ============================================================ 펀치

void AYJJTower::BuildPunch()
{
	// 깜짝 상자: 받침 + 파랑·노랑 상자 + 뒤로 젖혀진 뚜껑, 용수철 끝에 권투 장갑
	Part(RootComponent, EYJJShape::Cube, FVector(0, 0, 30), FVector(170, 170, 60), 0x64748B, FRotator::ZeroRotator, true);
	Part(RootComponent, EYJJShape::Cube, FVector(-10, 0, 112), FVector(140, 140, 104), 0x2563EB);
	Part(RootComponent, EYJJShape::Cube, FVector(-10, 0, 112), FVector(146, 146, 22), 0xFACC15);
	Part(RootComponent, EYJJShape::Cube, FVector(-10, 0, 168), FVector(146, 146, 10), 0x1E3A8A);
	// 앞면 물음표 대신 노란 별
	Part(RootComponent, EYJJShape::Sphere, FVector(62, 0, 112), FVector(10, 46, 46), 0xFACC15);
	USceneComponent* Hinge = Pivot(this, RootComponent, FVector(-82, 0, 172), FRotator(-115, 0, 0));
	Part(Hinge, EYJJShape::Cube, FVector(73, 0, 0), FVector(146, 146, 12), 0xFACC15);
	Arm = Part(RootComponent, EYJJShape::Cylinder, FVector(60, 0, 125), FVector(26, 26, 30), 0x94A3B8, FRotator(90, 0, 0));
	Glove = Pivot(this, RootComponent, FVector(PunchRest, 0, 125));
	Part(Glove, EYJJShape::Sphere, FVector::ZeroVector, FVector(135, 120, 125), 0xEF4444);
	Part(Glove, EYJJShape::Sphere, FVector(8, -54, 24), FVector(58, 48, 48), 0xEF4444);
	// 손목 띠 (원기둥 축을 X로 눕힌다)
	Part(Glove, EYJJShape::Cylinder, FVector(-62, 0, 0), FVector(84, 84, 30), 0xFFFFFF, FRotator(90, 0, 0));
	SetPunchLength(PunchRest);
}

void AYJJTower::SetPunchLength(float Length)
{
	const float Start = 45.f;
	Glove->SetRelativeLocation(FVector(Start + Length, 0, 125));
	const float ArmLength = FMath::Max(Length - 60.f, 4.f);
	Arm->SetRelativeLocation(FVector(Start + ArmLength * 0.5f, 0, 125));
	Arm->SetRelativeScale3D(FVector(0.26f, 0.26f, ArmLength / 100.f));
}

void AYJJTower::TickPunch(float DeltaTime)
{
	UYJJGame* Game = UYJJGame::Get(this);
	if (!Game)
	{
		return;
	}
	const FTransform Transform = GetActorTransform();
	StateTime += DeltaTime;
	switch (State)
	{
	case 0:
		Cooldown -= DeltaTime;
		if (Cooldown <= 0.f)
		{
			for (AYJJWobbler* Enemy : Game->GetEnemies())
			{
				if (Enemy && Enemy->IsAlive())
				{
					const FVector Local = Transform.InverseTransformPosition(Enemy->GetBodyLocation());
					if (Local.X > 140.f && Local.X < 640.f && FMath::Abs(Local.Y) < 130.f && FMath::Abs(Local.Z) < 400.f)
					{
						State = 1;
						StateTime = 0.f;
						break;
					}
				}
			}
		}
		break;
	case 1:	// 움찔 당겼다가
		SetPunchLength(FMath::Lerp(PunchRest, 20.f, FMath::Min(StateTime / 0.22f, 1.f)));
		if (StateTime >= 0.22f)
		{
			State = 2;
			StateTime = 0.f;
		}
		break;
	case 2:	// 뻥!
		SetPunchLength(FMath::Lerp(20.f, PunchFull, FMath::Min(StateTime / 0.08f, 1.f)));
		if (StateTime >= 0.08f)
		{
			const FVector Forward = GetActorForwardVector();
			const FVector Right = GetActorRightVector();
			int32 HitCount = 0;
			const TArray<TObjectPtr<AYJJWobbler>> Targets = Game->GetEnemies();	// 맞고 죽으면 목록이 바뀌므로 복사
			for (AYJJWobbler* Enemy : Targets)
			{
				if (!Enemy || !Enemy->IsAlive())
				{
					continue;
				}
				const FVector Local = Transform.InverseTransformPosition(Enemy->GetBodyLocation());
				if (Local.X > 90.f && Local.X < 690.f && FMath::Abs(Local.Y) < 150.f + 35.f * Enemy->GetScale() && FMath::Abs(Local.Z) < 420.f)
				{
					Enemy->Knock(Forward * 1350.f + FVector(0, 0, 680.f) + Right * FMath::FRandRange(-160.f, 160.f), 2.2f, 12.f);
					AYJJEffects::Burst(this, Enemy->GetBodyLocation(), YJJShapes::Hex(0xFFE066), 5, 480.f, 24.f, 0.4f, 700.f, true, 0.5f);
					HitCount++;
				}
			}
			YJJAudio::Play(this, EYJJSound::Punch, GetActorLocation(), HitCount > 0 ? 1.f : 0.5f, FMath::FRandRange(0.92f, 1.08f));
			State = 3;
			StateTime = 0.f;
		}
		break;
	case 3:
		if (StateTime >= 0.2f)
		{
			State = 4;
			StateTime = 0.f;
		}
		break;
	case 4:	// 스르륵 돌아온다
	{
		const float T = FMath::Min(StateTime / 0.4f, 1.f);
		SetPunchLength(FMath::Lerp(PunchFull, PunchRest, T * T * (3.f - 2.f * T)));
		if (T >= 1.f)
		{
			State = 0;
			Cooldown = 1.4f;
		}
		break;
	}
	}
}

// ============================================================ 대포

void AYJJTower::BuildCannon()
{
	Part(RootComponent, EYJJShape::Cylinder, FVector(0, 0, 30), FVector(210, 210, 60), 0x64748B, FRotator::ZeroRotator, true);
	Turret = Pivot(this, RootComponent, FVector(0, 0, 60));
	Part(Turret, EYJJShape::Cube, FVector(-10, 0, 30), FVector(140, 110, 50), 0x9A6B3F);
	for (const float Side : { -1.f, 1.f })
	{
		Part(Turret, EYJJShape::Cylinder, FVector(-10, Side * 68.f, 34), FVector(96, 96, 18), 0x6B4423, FRotator(0, 0, 90));
	}
	Barrel = Pivot(this, Turret, FVector(0, 0, 78));
	Part(Barrel, EYJJShape::Cylinder, FVector(55, 0, 0), FVector(82, 82, 200), 0x1F2937, FRotator(90, 0, 0));
	Part(Barrel, EYJJShape::Cylinder, FVector(152, 0, 0), FVector(98, 98, 26), 0x111827, FRotator(90, 0, 0));
	Part(Barrel, EYJJShape::Sphere, FVector(-45, 0, 0), FVector(88, 88, 88), 0x1F2937);
}

void AYJJTower::TickCannon(float DeltaTime)
{
	UYJJGame* Game = UYJJGame::Get(this);
	if (!Game)
	{
		return;
	}
	Cooldown -= DeltaTime;
	Recoil = FMath::Max(0.f, Recoil - DeltaTime * 3.5f);

	// 사거리 안에서 푸딩에 가장 가까이 간 놈
	const FVector Origin = GetActorLocation() + FVector(0, 0, 140);
	const float Range = UYJJGame::TowerInfo(Type).Range;
	AYJJWobbler* Target = nullptr;
	for (AYJJWobbler* Enemy : Game->GetEnemies())
	{
		if (Enemy && Enemy->IsAlive() && FVector::DistSquared2D(Enemy->GetBodyLocation(), Origin) < Range * Range
			&& (!Target || Enemy->GetProgress() > Target->GetProgress()))
		{
			Target = Enemy;
		}
	}
	if (Target)
	{
		// 포물선: 날아가는 시간 T를 정하고, 그동안 적이 걸어갈 곳을 겨눈다
		FVector Aim = Target->GetBodyLocation();
		const float T = FMath::Clamp(FVector::Dist2D(Aim, Origin) / 1500.f, 0.75f, 1.7f);
		if (Target->IsStanding())
		{
			const FVector V = Target->GetBodyVelocity();
			Aim += FVector(V.X, V.Y, 0.f) * T;
		}
		const FVector To = Aim - Origin;
		const FVector Launch(To.X / T, To.Y / T, To.Z / T + 0.5f * 980.f * T);
		const FVector Dir = Launch.GetSafeNormal();

		const FRotator Current = Turret->GetComponentRotation();
		const float Yaw = FMath::FixedTurn(Current.Yaw, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 240.f * DeltaTime);
		Turret->SetWorldRotation(FRotator(0.f, Yaw, 0.f));
		Barrel->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(FMath::Asin(Dir.Z)), 0.f, 0.f));

		if (Cooldown <= 0.f)
		{
			Cooldown = 3.4f;
			Recoil = 1.f;
			const FVector Muzzle = Origin + Dir * 190.f;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AYJJCannonBall* Shot = GetWorld()->SpawnActor<AYJJCannonBall>(AYJJCannonBall::StaticClass(), Muzzle, FRotator::ZeroRotator, Params))
			{
				Shot->Launch(Launch);
			}
			AYJJEffects::Burst(this, Muzzle, YJJShapes::Hex(0xF1F5F9), 9, 380.f, 70.f, 0.6f, -200.f, false, 0.2f);
			AYJJEffects::Burst(this, Muzzle, YJJShapes::Hex(0xFFB020), 5, 520.f, 36.f, 0.25f, 0.f, false, 0.1f);
			YJJAudio::Play(this, EYJJSound::Cannon, Muzzle, 0.9f, FMath::FRandRange(0.92f, 1.08f));
		}
	}
	Barrel->SetRelativeLocation(FVector(-45.f * Recoil, 0, 78));
}

// ============================================================ 선풍기

void AYJJTower::BuildFan()
{
	Part(RootComponent, EYJJShape::Cube, FVector(0, 0, 20), FVector(140, 140, 40), 0x64748B, FRotator::ZeroRotator, true);
	Part(RootComponent, EYJJShape::Cylinder, FVector(0, 0, 110), FVector(36, 36, 180), 0x94A3B8);
	Part(RootComponent, EYJJShape::Cylinder, FVector(22, 0, 205), FVector(150, 150, 22), 0xE2E8F0, FRotator(90, 0, 0));
	Part(RootComponent, EYJJShape::Sphere, FVector(-12, 0, 205), FVector(96, 96, 96), 0x3B82F6);
	Rotor = Pivot(this, RootComponent, FVector(48, 0, 205));
	for (int32 i = 0; i < 3; ++i)
	{
		USceneComponent* Blade = Pivot(this, Rotor, FVector::ZeroVector, FRotator(0, 0, i * 120.f));
		Part(Blade, EYJJShape::Cube, FVector(0, 82, 0), FVector(8, 150, 60), 0xF8FAFC, FRotator(0, 0, 18));
	}
	Part(Rotor, EYJJShape::Sphere, FVector(6, 0, 0), FVector(52, 52, 52), 0xFACC15);

	// 바람 줄기
	UMaterialInstanceDynamic* Wind = YJJShapes::Unlit(this, FLinearColor::White, 0.5f);
	for (int32 i = 0; i < 9; ++i)
	{
		UStaticMeshComponent* Streak = YJJShapes::Add(this, RootComponent, EYJJShape::Cube, FVector::ZeroVector, FVector(150, 7, 7), Wind);
		Streak->SetVisibility(false);
		Streaks.Add(Streak);
	}
	WindTime = FMath::FRandRange(0.f, FanPeriod);
}

void AYJJTower::TickFan(float DeltaTime)
{
	UYJJGame* Game = UYJJGame::Get(this);
	const bool bWasOn = FMath::Fmod(WindTime, FanPeriod) < FanOnTime;
	WindTime += DeltaTime;
	const bool bOn = FMath::Fmod(WindTime, FanPeriod) < FanOnTime && Game && Game->GetPhase() == EYJJPhase::Wave;
	if (bOn && !bWasOn)
	{
		YJJAudio::Play(this, EYJJSound::FanOn, GetActorLocation(), 0.6f);
	}
	RotorSpeed = FMath::FInterpTo(RotorSpeed, bOn ? 1500.f : 90.f, DeltaTime, 3.f);
	Rotor->AddLocalRotation(FRotator(0, 0, RotorSpeed * DeltaTime));

	for (int32 i = 0; i < Streaks.Num(); ++i)
	{
		Streaks[i]->SetVisibility(bOn);
		if (bOn)
		{
			const float X = 150.f + FMath::Fmod(WindTime * 1200.f + i * 173.f, FanReach - 200.f);
			Streaks[i]->SetRelativeLocation(FVector(X, (YJJShapes::Hash(i * 3 + Pad) - 0.5f) * 380.f, 70.f + YJJShapes::Hash(i * 7 + 50) * 210.f));
		}
	}

	if (!bOn)
	{
		return;
	}
	const FTransform Transform = GetActorTransform();
	const FVector Forward = GetActorForwardVector();
	for (AYJJWobbler* Enemy : Game->GetEnemies())
	{
		if (!Enemy || !Enemy->IsAlive())
		{
			continue;
		}
		const FVector Local = Transform.InverseTransformPosition(Enemy->GetBodyLocation());
		if (Local.X > 100.f && Local.X < FanReach && FMath::Abs(Local.Y) < 220.f && FMath::Abs(Local.Z) < 380.f)
		{
			Enemy->Push(Forward * 950.f * (1.f - 0.35f * Local.X / FanReach));
		}
	}
}

// ============================================================ 병사 초소

void AYJJTower::BuildGuardPost()
{
	Part(RootComponent, EYJJShape::Cylinder, FVector(0, 0, 6), FVector(220, 220, 12), 0x1E40AF);
	Part(RootComponent, EYJJShape::Cone, FVector(-40, -50, 70), FVector(170, 170, 140), 0xF1F5F9, FRotator::ZeroRotator, true);
	Part(RootComponent, EYJJShape::Cylinder, FVector(50, 60, 150), FVector(14, 14, 300), 0x9A6B3F);
	Part(RootComponent, EYJJShape::Cube, FVector(50, 108, 255), FVector(6, 90, 60), 0x3B82F6);
	Part(RootComponent, EYJJShape::Sphere, FVector(50, 60, 306), FVector(28, 28, 28), 0xFFD23F);
}

// ============================================================ 대포알

AYJJCannonBall::AYJJCannonBall()
{
	PrimaryActorTick.bCanEverTick = true;
	Ball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
	Ball->SetCanEverAffectNavigation(false);
	RootComponent = Ball;
}

void AYJJCannonBall::Launch(const FVector& Velocity)
{
	Ball->SetStaticMesh(YJJShapes::Mesh(EYJJShape::Sphere));
	Ball->SetWorldScale3D(FVector(0.84f));
	Ball->SetMaterial(0, YJJShapes::Solid(this, YJJShapes::Hex(0x1F2937)));
	Ball->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Ball->SetCollisionObjectType(ECC_WorldDynamic);
	Ball->SetCollisionResponseToAllChannels(ECR_Block);
	Ball->SetGenerateOverlapEvents(false);
	Ball->SetMassOverrideInKg(NAME_None, 90.f, true);
	Ball->SetSimulatePhysics(true);
	Ball->SetUseCCD(true);
	Ball->SetNotifyRigidBodyCollision(true);
	Ball->OnComponentHit.AddDynamic(this, &AYJJCannonBall::OnHit);
	Ball->SetPhysicsLinearVelocity(Velocity);
}

void AYJJCannonBall::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 쏘자마자 자기 대포에 닿은 건 무시
	if (ExplodedAge < 0.f && Age > 0.1f)
	{
		Explode();
	}
}

void AYJJCannonBall::Explode()
{
	ExplodedAge = Age;
	const FVector Location = GetActorLocation();
	const float Radius = 300.f;
	if (UYJJGame* Game = UYJJGame::Get(this))
	{
		TArray<TObjectPtr<AYJJWobbler>> Targets = Game->GetEnemies();
		Targets.Append(Game->GetGuards());
		for (AYJJWobbler* Target : Targets)
		{
			if (!IsValid(Target) || !Target->IsAlive())
			{
				continue;
			}
			const FVector To = Target->GetBodyLocation() - Location;
			const float Dist = To.Size();
			if (Dist < Radius + 40.f * Target->GetScale())
			{
				const float Power = FMath::Clamp(1.f - Dist / (Radius * 1.3f), 0.3f, 1.f);
				Target->Knock(To.GetSafeNormal2D() * 880.f * Power + FVector(0, 0, 640.f * Power), 2.f, 15.f * Power);
			}
		}
	}
	AYJJEffects::Burst(this, Location, YJJShapes::Hex(0xF1F5F9), 14, 620.f, 90.f, 0.7f, -150.f, false, 0.3f);
	AYJJEffects::Burst(this, Location, YJJShapes::Hex(0xFFB020), 8, 760.f, 46.f, 0.35f, 500.f, true, 0.4f);
	YJJAudio::Play(this, EYJJSound::Boom, Location, 1.f, FMath::FRandRange(0.9f, 1.1f));
}

void AYJJCannonBall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Age += DeltaTime;
	const FVector Location = GetActorLocation();
	if (Location.Z < YJJMap::KillZ)
	{
		const FVector Surface(Location.X, Location.Y, YJJMap::WaterZ);
		AYJJEffects::Burst(this, Surface, YJJShapes::Hex(0xE8F7FF), 9, 480.f, 34.f, 0.8f, 1500.f, false, 0.9f);
		YJJAudio::Play(this, EYJJSound::Splash, Surface, 0.6f, 0.8f);
		Destroy();
		return;
	}
	if ((ExplodedAge >= 0.f && Age - ExplodedAge > 2.2f) || Age > 8.f)
	{
		AYJJEffects::Burst(this, Location, YJJShapes::Hex(0xFFFFFF), 6, 220.f, 40.f, 0.4f, -100.f, false, 0.5f);
		Destroy();
	}
}
