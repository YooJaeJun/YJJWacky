#include "Gameplay/YJJTower.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/YJJAudio.h"
#include "Data/YJJLoc.h"
#include "Engine/World.h"
#include "Gameplay/YJJEffects.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Physics/YJJWobbler.h"
#include "World/YJJMap.h"

namespace
{
	/** 장갑이 접혀 있을 때 길이 (모양) */
	constexpr float PunchRest = 70.f;

	// 나머지 수치는 Towers.csv(단계별) / Tuning.csv
	float Tune(const TCHAR* Name, float Fallback) { return YJJData::Tune(Name, Fallback); }
	float PunchFull() { return Tune(TEXT("Punch.Reach"), 570.f); }
	float FanReach() { return Tune(TEXT("Fan.Reach"), 900.f); }

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

void AYJJTower::Setup(EYJJTowerType InType, int32 InPad, int32 Cost)
{
	Type = InType;
	Pad = InPad;
	Invested = Cost;
	switch (Type)
	{
	case EYJJTowerType::Punch: BuildPunch(); break;
	case EYJJTowerType::Cannon: BuildCannon(); break;
	case EYJJTowerType::Fan: BuildFan(); break;
	case EYJJTowerType::Guard: BuildGuardPost(); break;
	case EYJJTowerType::Sling: BuildSling(); break;
	}
	BuildLevelPips();
	UpdateLevelLooks();
	Cooldown = 0.4f;
	bReady = true;
}

void AYJJTower::SetLevel(int32 NewLevel, int32 AddedCost)
{
	Level = FMath::Clamp(NewLevel, 1, YJJMaxTowerLevel);
	Invested += AddedCost;
	UpdateLevelLooks();
}

void AYJJTower::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 팔리면서 붙잡고 있던 놈을 놓아준다
	if (AYJJWobbler* Held = Victim.Get())
	{
		Held->Release();
	}
	Super::EndPlay(EndPlayReason);
}

void AYJJTower::Beam(UStaticMeshComponent* Mesh, const FVector& A, const FVector& B, float Thickness)
{
	const FVector Mid = (A + B) * 0.5f;
	const FVector Dir = B - A;
	const float Length = FMath::Max(Dir.Size(), 1.f);
	Mesh->SetWorldLocationAndRotation(Mid, FRotationMatrix::MakeFromZ(Dir / Length).Rotator());
	Mesh->SetWorldScale3D(FVector(Thickness / 100.f, Thickness / 100.f, Length / 100.f));
}

void AYJJTower::BuildLevelPips()
{
	// 발판 뒤쪽 금색 구슬 = 강화 단계
	for (int32 i = 0; i < YJJMaxTowerLevel; ++i)
	{
		Pips.Add(Part(RootComponent, EYJJShape::Sphere, FVector(-112.f, (i - 1) * 46.f, 22.f), FVector(34.f), 0xFFC83D));
	}
}

void AYJJTower::UpdateLevelLooks()
{
	for (int32 i = 0; i < Pips.Num(); ++i)
	{
		Pips[i]->SetVisibility(i < Level);
	}
	switch (Type)
	{
	case EYJJTowerType::Punch:
		Glove->SetRelativeScale3D(FVector(Row().ParamB(Level)));
		break;
	case EYJJTowerType::Cannon:
		Barrel->SetRelativeScale3D(FVector(Row().ParamB(Level)));
		break;
	case EYJJTowerType::Fan:
	{
		Rotor->SetRelativeScale3D(FVector(Row().ParamB(Level)));
		UMaterialInterface* Blade = YJJShapes::Solid(this, YJJShapes::Hex(Level >= 3 ? 0xF87171 : (Level == 2 ? 0x93C5FD : 0xF8FAFC)));
		for (UStaticMeshComponent* Mesh : Blades)
		{
			Mesh->SetMaterial(0, Blade);
		}
		break;
	}
	case EYJJTowerType::Sling:
		Pouch->SetWorldScale3D(FVector(0.5f, 0.5f, 0.3f) * Row().ParamB(Level));
		break;
	default:
		break;
	}
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
	case EYJJTowerType::Sling: TickSling(DeltaTime); break;
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
					if (Local.X > 140.f && Local.X < Row().Range && FMath::Abs(Local.Y) < Row().ParamA(Level) - 20.f && FMath::Abs(Local.Z) < 400.f)
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
		SetPunchLength(FMath::Lerp(20.f, PunchFull(), FMath::Min(StateTime / 0.08f, 1.f)));
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
				if (Local.X > 90.f && Local.X < Row().Range + 50.f && FMath::Abs(Local.Y) < Row().ParamA(Level) + 35.f * Enemy->GetScale() && FMath::Abs(Local.Z) < 420.f)
				{
					const float Power = Row().Power(Level);
					const FVector Hit = Forward * Tune(TEXT("Punch.Forward"), 1350.f) + FVector(0, 0, Tune(TEXT("Punch.Up"), 680.f)) + Right * FMath::FRandRange(-160.f, 160.f);
					Enemy->Knock(Hit * Power, 2.2f, Tune(TEXT("Punch.Damage"), 12.f) * Power);
					AYJJEffects::Burst(this, Enemy->GetBodyLocation(), YJJShapes::Hex(0xFFE066), 5, 480.f, 24.f, 0.4f, 700.f, true, 0.5f);
					HitCount++;
				}
			}
			YJJAudio::Play(this, EYJJSound::Punch, GetActorLocation(), HitCount > 0 ? 1.f : 0.5f, FMath::FRandRange(0.92f, 1.08f) / (1.f + 0.08f * (Level - 1)));
			Shots++;
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
		SetPunchLength(FMath::Lerp(PunchFull(), PunchRest, T * T * (3.f - 2.f * T)));
		if (T >= 1.f)
		{
			State = 0;
			Cooldown = Row().Cooldown(Level);
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

void AYJJTower::FireCannonBall(const FVector& Origin, const FVector& Aim)
{
	const float T = FMath::Clamp(FVector::Dist2D(Aim, Origin) / 1500.f, 0.75f, 1.7f);
	const FVector To = Aim - Origin;
	const FVector Launch(To.X / T, To.Y / T, To.Z / T + 0.5f * 980.f * T);
	const FVector Muzzle = Origin + Launch.GetSafeNormal() * 190.f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AYJJCannonBall* Shot = GetWorld()->SpawnActor<AYJJCannonBall>(AYJJCannonBall::StaticClass(), Muzzle, FRotator::ZeroRotator, Params))
	{
		Shot->Launch(Launch, Row().Power(Level));
	}
	AYJJEffects::Burst(this, Muzzle, YJJShapes::Hex(0xF1F5F9), 9, 380.f, 70.f, 0.6f, -200.f, false, 0.2f);
	AYJJEffects::Burst(this, Muzzle, YJJShapes::Hex(0xFFB020), 5, 520.f, 36.f, 0.25f, 0.f, false, 0.1f);
	YJJAudio::Play(this, EYJJSound::Cannon, Muzzle, 0.9f, FMath::FRandRange(0.92f, 1.08f));
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
			Cooldown = Row().Cooldown(Level);
			Recoil = 1.f;
			Shots++;
			// 한 번에 여러 발이면 좌우로 벌려서
			const int32 Balls = FMath::Max(1, FMath::RoundToInt(Row().ParamA(Level)));
			const FVector Side = FVector::CrossProduct(FVector::UpVector, (Aim - Origin).GetSafeNormal2D());
			const float Spread = Tune(TEXT("Cannon.Spread"), 260.f);
			for (int32 b = 0; b < Balls; ++b)
			{
				const float Offset = Balls > 1 ? (b - (Balls - 1) * 0.5f) * Spread : 0.f;
				FireCannonBall(Origin, Aim + Side * Offset);
			}
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
		Blades.Add(Part(Blade, EYJJShape::Cube, FVector(0, 82, 0), FVector(8, 150, 60), 0xF8FAFC, FRotator(0, 0, 18)));
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
	WindTime = FMath::FRandRange(0.f, 5.f);
}

void AYJJTower::TickFan(float DeltaTime)
{
	UYJJGame* Game = UYJJGame::Get(this);
	const float OnTime = Tune(TEXT("Fan.OnTime"), 3.5f);
	const float Period = OnTime + Row().Cooldown(Level);
	const bool bWasOn = FMath::Fmod(WindTime, Period) < OnTime;
	WindTime += DeltaTime;
	const bool bOn = FMath::Fmod(WindTime, Period) < OnTime && Game && Game->GetPhase() == EYJJPhase::Wave;
	if (bOn && !bWasOn)
	{
		YJJAudio::Play(this, EYJJSound::FanOn, GetActorLocation(), 0.6f);
	}
	RotorSpeed = FMath::FInterpTo(RotorSpeed, bOn ? 1500.f + 650.f * (Level - 1) : 90.f, DeltaTime, 3.f);
	Rotor->AddLocalRotation(FRotator(0, 0, RotorSpeed * DeltaTime));

	for (int32 i = 0; i < Streaks.Num(); ++i)
	{
		Streaks[i]->SetVisibility(bOn);
		if (bOn)
		{
			const float X = 150.f + FMath::Fmod(WindTime * 1200.f + i * 173.f, FanReach() - 200.f);
			// 회오리(Lv3)는 바람 줄기가 빙글빙글 위로 감아 오른다
			const bool bTornado = Row().ParamA(Level) > 0.f;
			const float Swirl = bTornado ? FMath::Sin(WindTime * 9.f + i) * 90.f : 0.f;
			const float Rise = bTornado ? (X / FanReach()) * 160.f : 0.f;
			Streaks[i]->SetRelativeLocation(FVector(X, (YJJShapes::Hash(i * 3 + Pad) - 0.5f) * 380.f + Swirl, 70.f + YJJShapes::Hash(i * 7 + 50) * 210.f + Rise));
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
		if (Local.X > 100.f && Local.X < FanReach() && FMath::Abs(Local.Y) < Tune(TEXT("Fan.Width"), 220.f) && FMath::Abs(Local.Z) < 380.f)
		{
			const float Strength = Row().Power(Level) * (1.f - 0.35f * Local.X / FanReach());
			// 회오리(Lv3): 위로도 불어 올린다. 가벼운 놈은 둥실 떠서 날아간다
			Enemy->Push(Forward * Strength + FVector(0, 0, Row().ParamA(Level)));
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

// ============================================================ 새총

namespace
{
	const FVector PouchRest(-5.f, 0.f, 255.f);
	const FVector PouchPulled(-150.f, 0.f, 225.f);
	const FVector ProngTip(0.f, 72.f, 262.f);
}

void AYJJTower::BuildSling()
{
	Part(RootComponent, EYJJShape::Cube, FVector(0, 0, 30), FVector(160, 160, 60), 0x8B5A2B, FRotator::ZeroRotator, true);
	Part(RootComponent, EYJJShape::Cube, FVector(0, 0, 63), FVector(166, 166, 8), 0x6B4423);
	Turret = Pivot(this, RootComponent, FVector::ZeroVector);
	// Y자 나무 가지: 줄기 + 두 갈래 + 빨간 끝
	Part(Turret, EYJJShape::Cylinder, FVector(0, 0, 120), FVector(34, 34, 120), 0x9A6B3F);
	for (const float Side : { -1.f, 1.f })
	{
		UStaticMeshComponent* Prong = Part(Turret, EYJJShape::Cylinder, FVector::ZeroVector, FVector(28, 28, 100), 0x9A6B3F);
		const FVector Base(0, 0, 175);
		const FVector Tip(ProngTip.X, ProngTip.Y * Side, ProngTip.Z);
		Prong->SetRelativeLocationAndRotation((Base + Tip) * 0.5f, FRotationMatrix::MakeFromZ((Tip - Base).GetSafeNormal()).Rotator());
		Prong->SetRelativeScale3D(FVector(0.28f, 0.28f, (Tip - Base).Size() / 100.f));
		Part(Turret, EYJJShape::Sphere, Tip + FVector(0, 0, 6), FVector(34, 34, 34), 0xEF4444);
	}

	// 고무줄·주머니·올가미는 월드 좌표로 직접 움직인다
	auto Loose = [this](uint32 ColorHex)
	{
		UStaticMeshComponent* Mesh = Part(RootComponent, EYJJShape::Cylinder, FVector::ZeroVector, FVector(10, 10, 100), ColorHex);
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		Mesh->SetUsingAbsoluteScale(true);
		Mesh->SetCastShadow(false);
		return Mesh;
	};
	Bands[0] = Loose(0xF97316);
	Bands[1] = Loose(0xF97316);
	Rope = Loose(0xFDE68A);
	Rope->SetVisibility(false);
	Pouch = Part(RootComponent, EYJJShape::Sphere, FVector::ZeroVector, FVector(50, 50, 30), 0x5B3A1E);
	Pouch->SetUsingAbsoluteLocation(true);
	Pouch->SetUsingAbsoluteRotation(true);
	Pouch->SetUsingAbsoluteScale(true);
	PouchLocal = PouchRest;
}

void AYJJTower::TickSling(float DeltaTime)
{
	UYJJGame* Game = UYJJGame::Get(this);
	if (!Game)
	{
		return;
	}
	StateTime += DeltaTime;
	AYJJWobbler* Held = Victim.Get();
	if ((State == 1 || State == 2) && (!Held || !Held->IsAlive() || !Held->IsHeld()))
	{
		// 놓쳤다 (다른 타워에 맞아 죽었거나)
		Victim = nullptr;
		Held = nullptr;
		State = 3;
		StateTime = 0.f;
	}

	const float Limit = UYJJGame::SlingLimitKg(Level);
	switch (State)
	{
	case 0:	// 가까운 가벼운 놈을 찾는다
	{
		PouchLocal = PouchRest;
		Cooldown -= DeltaTime;
		if (Cooldown > 0.f)
		{
			break;
		}
		AYJJWobbler* Best = nullptr;
		AYJJWobbler* TooHeavy = nullptr;
		float BestSq = FMath::Square(Row().Range);
		for (AYJJWobbler* Enemy : Game->GetEnemies())
		{
			if (!Enemy || !Enemy->IsAlive() || Enemy->IsHeld() || Enemy->IsProjectile())
			{
				continue;
			}
			const float DistSq = FVector::DistSquared2D(Enemy->GetBodyLocation(), GetActorLocation());
			if (DistSq >= FMath::Square(Row().Range))
			{
				continue;
			}
			if (Enemy->GetWeight() > Limit)
			{
				TooHeavy = Enemy;
			}
			else if (DistSq < BestSq)
			{
				BestSq = DistSq;
				Best = Enemy;
			}
		}
		// 사거리 안에 너무 무거운 놈만 있으면 끙끙대며 "무거워!" (강화하라는 신호)
		HeavyHintTime -= DeltaTime;
		if (!Best && TooHeavy && HeavyHintTime <= 0.f)
		{
			HeavyHintTime = Tune(TEXT("Sling.HeavyHint"), 2.5f);
			Game->AddFloater(GetActorLocation() + FVector(0, 0, 330.f), YJJLoc::Get(TEXT("Float.Heavy")), YJJShapes::Hex(0xFFB4A2), 28.f);
			YJJAudio::Play(this, EYJJSound::Deny, GetActorLocation(), 0.5f, 0.8f);
		}
		if (Best)
		{
			// 고르는 순간 붙잡는다 (다음 프레임의 "놓쳤나?" 검사가 IsHeld를 본다)
			Best->Hold(Turret->GetComponentTransform().TransformPosition(PouchLocal) + FVector(0, 0, 25.f * Best->GetScale()));
			Victim = Best;
			State = 1;
			StateTime = 0.f;
			YJJAudio::Play(this, EYJJSound::Whoosh, GetActorLocation(), 0.6f, 1.4f);
		}
		break;
	}
	case 1:	// 올가미로 끌어온다
	{
		const FVector Target = Turret->GetComponentTransform().TransformPosition(PouchLocal) + FVector(0, 0, 25.f * Held->GetScale());
		Held->Hold(Target);
		if (FVector::Dist(Held->GetBodyLocation(), Target) < 60.f || StateTime > 1.3f)
		{
			State = 2;
			StateTime = 0.f;
			Game->AddFloater(Target + FVector(0, 0, 90.f), YJJLoc::Get(TEXT("Float.Grab")), YJJShapes::Hex(0xFFFFFF), 28.f);
			YJJAudio::Play(this, EYJJSound::Grab, Target, 0.9f, FMath::FRandRange(0.9f, 1.3f) / FMath::Sqrt(Held->GetScale()));
		}
		break;
	}
	case 2:	// 겨누면서 고무줄을 당긴다
	{
		// 표적: 사거리 안에서 푸딩에 가장 가까이 간 놈. 없으면 뒤쪽 물로 던져 버린다
		AYJJWobbler* Target = nullptr;
		for (AYJJWobbler* Enemy : Game->GetEnemies())
		{
			if (Enemy && Enemy != Held && Enemy->IsAlive() && !Enemy->IsHeld()
				&& FVector::DistSquared2D(Enemy->GetBodyLocation(), GetActorLocation()) < FMath::Square(Tune(TEXT("Sling.AimRange"), 1900.f))
				&& FVector::DistSquared2D(Enemy->GetBodyLocation(), GetActorLocation()) > FMath::Square(350.f)
				&& (!Target || Enemy->GetProgress() > Target->GetProgress()))
			{
				Target = Enemy;
			}
		}
		bAimAtWater = Target == nullptr;
		if (Target)
		{
			AimPoint = Target->GetBodyLocation();
			if (Target->IsStanding())
			{
				const FVector V = Target->GetBodyVelocity();
				AimPoint += FVector(V.X, V.Y, 0.f) * 0.8f;
			}
		}
		else
		{
			AimPoint = GetActorLocation() - GetActorForwardVector() * 1100.f + FVector(0, 0, YJJMap::WaterZ);
		}
		const FVector ToAim = (AimPoint - GetActorLocation()).GetSafeNormal2D();
		const float Yaw = FMath::FixedTurn(Turret->GetComponentRotation().Yaw, FMath::RadiansToDegrees(FMath::Atan2(ToAim.Y, ToAim.X)), 360.f * DeltaTime);
		Turret->SetWorldRotation(FRotator(0.f, Yaw, 0.f));

		const float Draw = FMath::Min(StateTime / 0.7f, 1.f);
		PouchLocal = FMath::Lerp(PouchRest, PouchPulled, Draw * Draw);
		Held->Hold(Turret->GetComponentTransform().TransformPosition(PouchLocal) + FVector(0, 0, 25.f * Held->GetScale()));

		if (StateTime > 0.85f)
		{
			// 슝! 포물선으로 표적에 꽂는다
			const FVector From = Held->GetBodyLocation();
			const float T = FMath::Clamp(FVector::Dist2D(AimPoint, From) / 1500.f, 0.55f, 1.2f);
			const FVector To = AimPoint + FVector(0, 0, bAimAtWater ? 0.f : 30.f) - From;
			const FVector Launch(To.X / T, To.Y / T, To.Z / T + 0.5f * 980.f * T);
			Held->Fling(Launch, Row().Power(Level));
			Game->AddFloater(From + FVector(0, 0, 120.f), YJJLoc::Get(bAimAtWater ? TEXT("Float.Toss") : TEXT("Float.Fire")), YJJShapes::Hex(0xFFD23F), 32.f);
			YJJAudio::Play(this, EYJJSound::Sling, From, 1.f, FMath::FRandRange(0.95f, 1.1f));
			Victim = nullptr;
			Shots++;
			State = 3;
			StateTime = 0.f;
		}
		break;
	}
	case 3:	// 고무줄이 앞으로 튕겼다가 돌아온다
	{
		const float T = FMath::Min(StateTime / 0.35f, 1.f);
		const float Spring = FMath::Sin(T * PI * 2.5f) * (1.f - T);
		PouchLocal = PouchRest + FVector(120.f * Spring, 0.f, 15.f * Spring);
		if (T >= 1.f)
		{
			State = 0;
			Cooldown = Row().Cooldown(Level);
		}
		break;
	}
	}

	// 주머니·고무줄·올가미 모양
	const FTransform TurretTransform = Turret->GetComponentTransform();
	const FVector PouchWorld = TurretTransform.TransformPosition(PouchLocal);
	Pouch->SetWorldLocationAndRotation(PouchWorld, TurretTransform.GetRotation());
	for (int32 i = 0; i < 2; ++i)
	{
		const FVector Tip = TurretTransform.TransformPosition(FVector(ProngTip.X, ProngTip.Y * (i ? 1.f : -1.f), ProngTip.Z));
		Beam(Bands[i], Tip, PouchWorld, 10.f);
	}
	const bool bLasso = State == 1 && Held;
	Rope->SetVisibility(bLasso);
	if (bLasso)
	{
		Beam(Rope, PouchWorld, Held->GetBodyLocation(), 6.f);
	}
}

// ============================================================ 대포알

AYJJCannonBall::AYJJCannonBall()
{
	PrimaryActorTick.bCanEverTick = true;
	Ball = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ball"));
	Ball->SetCanEverAffectNavigation(false);
	RootComponent = Ball;
}

void AYJJCannonBall::Launch(const FVector& Velocity, float InPower)
{
	Power = InPower;
	Ball->SetStaticMesh(YJJShapes::Mesh(EYJJShape::Sphere));
	Ball->SetWorldScale3D(FVector(0.84f * FMath::Sqrt(Power)));
	Ball->SetMaterial(0, YJJShapes::Solid(this, YJJShapes::Hex(0x1F2937)));
	Ball->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Ball->SetCollisionObjectType(ECC_WorldDynamic);
	Ball->SetCollisionResponseToAllChannels(ECR_Block);
	Ball->SetGenerateOverlapEvents(false);
	Ball->SetMassOverrideInKg(NAME_None, 90.f * Power, true);
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
	const float Radius = Tune(TEXT("Cannon.Radius"), 300.f) * FMath::Sqrt(Power);
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
				const float Falloff = FMath::Clamp(1.f - Dist / (Radius * 1.3f), 0.3f, 1.f) * Power;
				Target->Knock(To.GetSafeNormal2D() * Tune(TEXT("Cannon.Forward"), 880.f) * Falloff + FVector(0, 0, Tune(TEXT("Cannon.Up"), 640.f) * Falloff), 2.f,
					Tune(TEXT("Cannon.Damage"), 15.f) * Falloff);
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
