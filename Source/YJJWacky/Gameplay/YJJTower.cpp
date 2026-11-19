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

const FVector AYJJTower::PouchRest(-5.f, 0.f, 255.f);
const FVector AYJJTower::PouchPulled(-150.f, 0.f, 225.f);
const FVector AYJJTower::ProngTip(0.f, 72.f, 262.f);
const FVector AYJJTower::PouchScale(0.5f, 0.5f, 0.3f);

AYJJTower::AYJJTower()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

FVector AYJJTower::Ballistic(const FVector& To, float T)
{
	return FVector(To.X / T, To.Y / T, To.Z / T + 0.5f * Gravity * T);
}

void AYJJTower::Setup(EYJJTowerType InType, int32 InPad, int32 Cost)
{
	Type = InType;
	Pad = InPad;
	Invested = Cost;
	BuildModel();
	UpdateLevelLooks();
	Cooldown = FirstCooldown;
	bReady = true;
}

void AYJJTower::BuildModel()
{
	// Parts.csv: Tower.Common (강화 구슬) + Tower.<종류>
	FYJJModelContext Context;
	const FYJJModelParts Common = YJJShapes::BuildModel(this, TEXT("Tower.Common"), Context);
	Pips = Common.Meshes(TEXT("Pip"));

	const FString Model = FString(TEXT("Tower.")) + StaticEnum<EYJJTowerType>()->GetNameStringByValue(static_cast<int64>(Type));
	const FYJJModelParts Parts = YJJShapes::BuildModel(this, Model, Context);
	Glove = Parts.First(TEXT("Glove"));
	Arm = Parts.FirstMesh(TEXT("Arm"));
	Turret = Parts.First(TEXT("Turret"));
	Barrel = Parts.First(TEXT("Barrel"));
	Rotor = Parts.First(TEXT("Rotor"));
	Blades = Parts.Meshes(TEXT("Blade"));
	Streaks = Parts.Meshes(TEXT("Streak"));
	Bands = Parts.Meshes(TEXT("Band"));
	Rope = Parts.FirstMesh(TEXT("Rope"));
	Pouch = Parts.FirstMesh(TEXT("Pouch"));

	if (Type == EYJJTowerType::Punch && Glove && Arm)
	{
		SetPunchLength(PunchRest);
	}
	if (Type == EYJJTowerType::Fan)
	{
		WindTime = FMath::FRandRange(0.f, WindPhaseRange);
	}
	PouchLocal = PouchRest;
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
	Mesh->SetWorldScale3D(FVector(Thickness, Thickness, Length) / YJJShapes::MeshSize);
}

void AYJJTower::UpdateLevelLooks()
{
	for (int32 i = 0; i < Pips.Num(); ++i)
	{
		Pips[i]->SetVisibility(i < Level);
	}
	// 단계별 크기는 Towers.csv 의 ParamB
	switch (Type)
	{
	case EYJJTowerType::Punch:
		if (Glove)
		{
			Glove->SetRelativeScale3D(FVector(Row().ParamB(Level)));
		}
		break;
	case EYJJTowerType::Cannon:
		if (Barrel)
		{
			Barrel->SetRelativeScale3D(FVector(Row().ParamB(Level)));
		}
		break;
	case EYJJTowerType::Fan:
	{
		if (Rotor)
		{
			Rotor->SetRelativeScale3D(FVector(Row().ParamB(Level)));
		}
		UMaterialInterface* Blade = YJJShapes::Solid(this, YJJData::Color(FString::Printf(TEXT("FanBlade%d"), Level)));
		for (UStaticMeshComponent* Mesh : Blades)
		{
			Mesh->SetMaterial(0, Blade);
		}
		break;
	}
	case EYJJTowerType::Sling:
		if (Pouch)
		{
			Pouch->SetWorldScale3D(PouchScale * Row().ParamB(Level));
		}
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

void AYJJTower::SetPunchLength(float Length)
{
	Glove->SetRelativeLocation(FVector(PunchArmStart + Length, 0.f, PunchHeight));
	const float ArmLength = FMath::Max(Length - PunchArmGap, PunchArmMin);
	Arm->SetRelativeLocation(FVector(PunchArmStart + ArmLength * 0.5f, 0.f, PunchHeight));
	Arm->SetRelativeScale3D(FVector(PunchArmThickness, PunchArmThickness, ArmLength) / YJJShapes::MeshSize);
}

void AYJJTower::TickPunch(float DeltaTime)
{
	UYJJGame* Game = UYJJGame::Get(this);
	if (!Game || !Glove || !Arm)
	{
		return;
	}
	const FTransform Transform = GetActorTransform();
	StateTime += DeltaTime;
	switch (State)
	{
	case StateIdle:
		Cooldown -= DeltaTime;
		if (Cooldown <= 0.f)
		{
			for (AYJJWobbler* Enemy : Game->GetEnemies())
			{
				if (Enemy && Enemy->IsAlive())
				{
					const FVector Local = Transform.InverseTransformPosition(Enemy->GetBodyLocation());
					if (Local.X > PunchTriggerStart && Local.X < Row().Range && FMath::Abs(Local.Y) < Row().ParamA(Level) - PunchTriggerNarrow
						&& FMath::Abs(Local.Z) < HeightBand)
					{
						State = StatePrepare;
						StateTime = 0.f;
						break;
					}
				}
			}
		}
		break;
	case StatePrepare:	// 움찔 당겼다가
		SetPunchLength(FMath::Lerp(PunchRest, PunchWindup, FMath::Min(StateTime / PunchWindupTime, 1.f)));
		if (StateTime >= PunchWindupTime)
		{
			State = StateStrike;
			StateTime = 0.f;
		}
		break;
	case StateStrike:	// 뻥!
		SetPunchLength(FMath::Lerp(PunchWindup, Tune(TEXT("Punch.Reach")), FMath::Min(StateTime / PunchStrikeTime, 1.f)));
		if (StateTime >= PunchStrikeTime)
		{
			const FVector Forward = GetActorForwardVector();
			const FVector Right = GetActorRightVector();
			const float Power = Row().Power(Level);
			int32 HitCount = 0;
			const TArray<TObjectPtr<AYJJWobbler>> Targets = Game->GetEnemies();	// 맞고 죽으면 목록이 바뀌므로 복사
			for (AYJJWobbler* Enemy : Targets)
			{
				if (!Enemy || !Enemy->IsAlive())
				{
					continue;
				}
				const FVector Local = Transform.InverseTransformPosition(Enemy->GetBodyLocation());
				if (Local.X > PunchHitStart && Local.X < Row().Range + PunchHitExtra
					&& FMath::Abs(Local.Y) < Row().ParamA(Level) + PunchHitPerScale * Enemy->GetScale() && FMath::Abs(Local.Z) < HeightBand)
				{
					const FVector Hit = Forward * Tune(TEXT("Punch.Forward")) + FVector(0, 0, Tune(TEXT("Punch.Up")))
						+ Right * FMath::FRandRange(-PunchSideJitter, PunchSideJitter);
					Enemy->Knock(Hit * Power, PunchLimp, Tune(TEXT("Punch.Damage")) * Power);
					AYJJEffects::Burst(this, TEXT("PunchHit"), Enemy->GetBodyLocation());
					HitCount++;
				}
			}
			// 강화할수록 소리가 묵직해진다
			YJJAudio::Play(this, HitCount > 0 ? TEXT("PunchHit") : TEXT("PunchMiss"), GetActorLocation(),
				1.f / (1.f + Tune(TEXT("Punch.PitchDropPerLevel")) * (Level - 1)));
			Shots++;
			State = StateHold;
			StateTime = 0.f;
		}
		break;
	case StateHold:
		if (StateTime >= PunchHoldTime)
		{
			State = StateReturn;
			StateTime = 0.f;
		}
		break;
	case StateReturn:	// 스르륵 돌아온다
	{
		const float T = FMath::Min(StateTime / PunchReturnTime, 1.f);
		SetPunchLength(FMath::Lerp(Tune(TEXT("Punch.Reach")), PunchRest, FMath::SmoothStep(0.f, 1.f, T)));
		if (T >= 1.f)
		{
			State = StateIdle;
			Cooldown = Row().Cooldown(Level);
		}
		break;
	}
	default:
		break;
	}
}

// ============================================================ 대포

void AYJJTower::FireCannonBall(const FVector& Origin, const FVector& Aim)
{
	const float T = FMath::Clamp(FVector::Dist2D(Aim, Origin) / CannonFlightSpeed, CannonFlightMin, CannonFlightMax);
	const FVector Launch = Ballistic(Aim - Origin, T);
	const FVector Muzzle = Origin + Launch.GetSafeNormal() * CannonMuzzle;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AYJJCannonBall* Shot = GetWorld()->SpawnActor<AYJJCannonBall>(AYJJCannonBall::StaticClass(), Muzzle, FRotator::ZeroRotator, Params))
	{
		Shot->Launch(Launch, Row().Power(Level));
	}
	AYJJEffects::Burst(this, TEXT("Smoke"), Muzzle);
	AYJJEffects::Burst(this, TEXT("MuzzleFlash"), Muzzle);
	YJJAudio::Play(this, TEXT("Cannon"), Muzzle);
}

void AYJJTower::TickCannon(float DeltaTime)
{
	UYJJGame* Game = UYJJGame::Get(this);
	if (!Game || !Turret || !Barrel)
	{
		return;
	}
	Cooldown -= DeltaTime;
	Recoil = FMath::Max(0.f, Recoil - DeltaTime * RecoilRecover);

	// 사거리 안에서 푸딩에 가장 가까이 간 놈
	const FVector Origin = GetActorLocation() + FVector(0, 0, CannonOriginHeight);
	const float Range = Row().Range;
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
		const float T = FMath::Clamp(FVector::Dist2D(Aim, Origin) / CannonFlightSpeed, CannonFlightMin, CannonFlightMax);
		if (Target->IsStanding())
		{
			const FVector V = Target->GetBodyVelocity();
			Aim += FVector(V.X, V.Y, 0.f) * T;
		}
		const FVector Dir = Ballistic(Aim - Origin, T).GetSafeNormal();
		const float Yaw = FMath::FixedTurn(Turret->GetComponentRotation().Yaw, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), CannonTurnSpeed * DeltaTime);
		Turret->SetWorldRotation(FRotator(0.f, Yaw, 0.f));
		Barrel->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(FMath::Asin(Dir.Z)), 0.f, 0.f));

		if (Cooldown <= 0.f)
		{
			Cooldown = Row().Cooldown(Level);
			Recoil = 1.f;
			Shots++;
			// 한 번에 여러 발이면 좌우로 벌려서 (Towers.csv ParamA = 포탄 수)
			const int32 Balls = FMath::Max(1, FMath::RoundToInt(Row().ParamA(Level)));
			const FVector Side = FVector::CrossProduct(FVector::UpVector, (Aim - Origin).GetSafeNormal2D());
			const float Spread = Tune(TEXT("Cannon.Spread"));
			for (int32 b = 0; b < Balls; ++b)
			{
				const float Offset = (b - (Balls - 1) * 0.5f) * Spread;
				FireCannonBall(Origin, Aim + Side * Offset);
			}
		}
	}
	Barrel->SetRelativeLocation(FVector(-RecoilDistance * Recoil, 0.f, BarrelHeight));
}

// ============================================================ 선풍기

void AYJJTower::TickFan(float DeltaTime)
{
	UYJJGame* Game = UYJJGame::Get(this);
	if (!Game || !Rotor)
	{
		return;
	}
	// 켜짐 Fan.OnTime 초, 꺼짐 Towers.csv Cooldown 초
	const float OnTime = Tune(TEXT("Fan.OnTime"));
	const float Period = OnTime + Row().Cooldown(Level);
	const bool bWasOn = FMath::Fmod(WindTime, Period) < OnTime;
	WindTime += DeltaTime;
	const bool bOn = FMath::Fmod(WindTime, Period) < OnTime && Game->GetPhase() == EYJJPhase::Wave;
	if (bOn && !bWasOn)
	{
		YJJAudio::Play(this, TEXT("FanOn"), GetActorLocation());
	}
	RotorSpeed = FMath::FInterpTo(RotorSpeed, bOn ? RotorSpeedBase + RotorSpeedPerLevel * (Level - 1) : RotorSpeedIdle, DeltaTime, RotorSpinUp);
	Rotor->AddLocalRotation(FRotator(0.f, 0.f, RotorSpeed * DeltaTime));

	const float Reach = Tune(TEXT("Fan.Reach"));
	const float Lift = Row().ParamA(Level);
	for (int32 i = 0; i < Streaks.Num(); ++i)
	{
		Streaks[i]->SetVisibility(bOn);
		if (bOn)
		{
			const float X = StreakStart + FMath::Fmod(WindTime * StreakSpeed + i * StreakSpacing, Reach - StreakEndMargin);
			// 회오리(위로 띄우는 단계)는 바람 줄기가 빙글빙글 위로 감아 오른다
			const bool bTornado = Lift > 0.f;
			const float Swirl = bTornado ? FMath::Sin(WindTime * SwirlFrequency + i) * SwirlAmplitude : 0.f;
			const float Rise = bTornado ? (X / Reach) * SwirlRise : 0.f;
			Streaks[i]->SetRelativeLocation(FVector(X,
				(YJJShapes::Hash(i * StreakSeedSide + Pad) - 0.5f) * StreakSpread + Swirl,
				StreakBaseHeight + YJJShapes::Hash(i * StreakSeedHeight + StreakSeedOffset) * StreakHeightRange + Rise));
		}
	}

	if (!bOn)
	{
		return;
	}
	const FTransform Transform = GetActorTransform();
	const FVector Forward = GetActorForwardVector();
	const float Width = Tune(TEXT("Fan.Width"));
	for (AYJJWobbler* Enemy : Game->GetEnemies())
	{
		if (!Enemy || !Enemy->IsAlive())
		{
			continue;
		}
		const FVector Local = Transform.InverseTransformPosition(Enemy->GetBodyLocation());
		if (Local.X > FanStart && Local.X < Reach && FMath::Abs(Local.Y) < Width && FMath::Abs(Local.Z) < HeightBand)
		{
			const float Strength = Row().Power(Level) * (1.f - FanFalloff * Local.X / Reach);
			// 회오리: 위로도 불어 올린다. 가벼운 놈은 둥실 떠서 날아간다
			Enemy->Push(Forward * Strength + FVector(0.f, 0.f, Lift));
		}
	}
}

// ============================================================ 새총

FVector AYJJTower::PouchWorld() const
{
	return Turret->GetComponentTransform().TransformPosition(PouchLocal);
}

void AYJJTower::TickSling(float DeltaTime)
{
	UYJJGame* Game = UYJJGame::Get(this);
	if (!Game || !Turret || !Pouch || !Rope || Bands.Num() == 0)
	{
		return;
	}
	StateTime += DeltaTime;
	AYJJWobbler* Held = Victim.Get();
	if ((State == StatePrepare || State == StateStrike) && (!Held || !Held->IsAlive() || !Held->IsHeld()))
	{
		// 놓쳤다 (다른 타워에 맞아 죽었거나)
		Victim = nullptr;
		Held = nullptr;
		State = StateHold;
		StateTime = 0.f;
	}

	const float Limit = UYJJGame::SlingLimitKg(Level);
	switch (State)
	{
	case StateIdle:	// 가까운 들 수 있는 놈을 찾는다
	{
		PouchLocal = PouchRest;
		Cooldown -= DeltaTime;
		HeavyHintTime -= DeltaTime;
		if (Cooldown > 0.f)
		{
			break;
		}
		AYJJWobbler* Best = nullptr;
		AYJJWobbler* TooHeavy = nullptr;
		const float RangeSq = FMath::Square(Row().Range);
		float BestSq = RangeSq;
		for (AYJJWobbler* Enemy : Game->GetEnemies())
		{
			if (!Enemy || !Enemy->IsAlive() || Enemy->IsHeld() || Enemy->IsProjectile())
			{
				continue;
			}
			const float DistSq = FVector::DistSquared2D(Enemy->GetBodyLocation(), GetActorLocation());
			if (DistSq >= RangeSq)
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
		if (!Best && TooHeavy && HeavyHintTime <= 0.f)
		{
			HeavyHintTime = Tune(TEXT("Sling.HeavyHint"));
			Game->AddFloater(GetActorLocation(), YJJLoc::Get(TEXT("Float.Heavy")), TEXT("Heavy"));
			YJJAudio::Play(this, TEXT("SlingHeavy"), GetActorLocation());
		}
		if (Best)
		{
			// 고르는 순간 붙잡는다 (다음 프레임의 "놓쳤나?" 검사가 IsHeld를 본다)
			Best->Hold(PouchWorld() + FVector(0, 0, HoldLiftPerScale * Best->GetScale()));
			Victim = Best;
			State = StatePrepare;
			StateTime = 0.f;
			YJJAudio::Play(this, TEXT("SlingLasso"), GetActorLocation());
		}
		break;
	}
	case StatePrepare:	// 올가미로 끌어온다
	{
		const FVector Target = PouchWorld() + FVector(0, 0, HoldLiftPerScale * Held->GetScale());
		Held->Hold(Target);
		if (FVector::Dist(Held->GetBodyLocation(), Target) < LassoArrive || StateTime > LassoMaxTime)
		{
			State = StateStrike;
			StateTime = 0.f;
			Game->AddFloater(Target, YJJLoc::Get(TEXT("Float.Grab")), TEXT("Grab"));
			YJJAudio::Play(this, TEXT("SlingGrab"), Target, 1.f / FMath::Sqrt(Held->GetScale()));
		}
		break;
	}
	case StateStrike:	// 겨누면서 고무줄을 당긴다
	{
		// 표적: 사거리 안에서 푸딩에 가장 가까이 간 놈. 없으면 뒤쪽 물로 던져 버린다
		const float AimRangeSq = FMath::Square(Tune(TEXT("Sling.AimRange")));
		AYJJWobbler* Target = nullptr;
		for (AYJJWobbler* Enemy : Game->GetEnemies())
		{
			if (Enemy && Enemy != Held && Enemy->IsAlive() && !Enemy->IsHeld())
			{
				const float DistSq = FVector::DistSquared2D(Enemy->GetBodyLocation(), GetActorLocation());
				if (DistSq < AimRangeSq && DistSq > FMath::Square(AimMinDistance) && (!Target || Enemy->GetProgress() > Target->GetProgress()))
				{
					Target = Enemy;
				}
			}
		}
		bAimAtWater = Target == nullptr;
		if (Target)
		{
			AimPoint = Target->GetBodyLocation();
			if (Target->IsStanding())
			{
				const FVector V = Target->GetBodyVelocity();
				AimPoint += FVector(V.X, V.Y, 0.f) * AimLead;
			}
		}
		else
		{
			AimPoint = GetActorLocation() - GetActorForwardVector() * TossDistance + FVector(0, 0, YJJMap::WaterZ());
		}
		const FVector ToAim = (AimPoint - GetActorLocation()).GetSafeNormal2D();
		const float Yaw = FMath::FixedTurn(Turret->GetComponentRotation().Yaw, FMath::RadiansToDegrees(FMath::Atan2(ToAim.Y, ToAim.X)), AimTurnSpeed * DeltaTime);
		Turret->SetWorldRotation(FRotator(0.f, Yaw, 0.f));

		const float Draw = FMath::Min(StateTime / DrawTime, 1.f);
		PouchLocal = FMath::Lerp(PouchRest, PouchPulled, Draw * Draw);
		Held->Hold(PouchWorld() + FVector(0, 0, HoldLiftPerScale * Held->GetScale()));

		if (StateTime > FireTime)
		{
			// 슝! 포물선으로 표적에 꽂는다
			const FVector From = Held->GetBodyLocation();
			const float T = FMath::Clamp(FVector::Dist2D(AimPoint, From) / SlingFlightSpeed, SlingFlightMin, SlingFlightMax);
			const FVector To = AimPoint + FVector(0, 0, bAimAtWater ? 0.f : AimBodyHeight) - From;
			Held->Fling(Ballistic(To, T), Row().Power(Level));
			Game->AddFloater(From, YJJLoc::Get(bAimAtWater ? TEXT("Float.Toss") : TEXT("Float.Fire")), TEXT("Fire"));
			YJJAudio::Play(this, TEXT("SlingFire"), From);
			Victim = nullptr;
			Held = nullptr;
			Shots++;
			State = StateHold;
			StateTime = 0.f;
		}
		break;
	}
	case StateHold:	// 고무줄이 앞으로 튕겼다가 돌아온다
	{
		const float T = FMath::Min(StateTime / SnapTime, 1.f);
		const float Spring = FMath::Sin(T * PI * SnapCycles) * (1.f - T);
		PouchLocal = PouchRest + FVector(SnapForward * Spring, 0.f, SnapUp * Spring);
		if (T >= 1.f)
		{
			State = StateIdle;
			Cooldown = Row().Cooldown(Level);
		}
		break;
	}
	default:
		break;
	}

	// 주머니·고무줄·올가미 모양
	const FTransform TurretTransform = Turret->GetComponentTransform();
	const FVector PouchAt = PouchWorld();
	Pouch->SetWorldLocationAndRotation(PouchAt, TurretTransform.GetRotation());
	for (int32 i = 0; i < Bands.Num(); ++i)
	{
		const float Side = (i % 2) ? 1.f : -1.f;
		const FVector Tip = TurretTransform.TransformPosition(FVector(ProngTip.X, ProngTip.Y * Side, ProngTip.Z));
		Beam(Bands[i], Tip, PouchAt, BandThickness);
	}
	const bool bLasso = State == StatePrepare && Held;
	Rope->SetVisibility(bLasso);
	if (bLasso)
	{
		Beam(Rope, PouchAt, Held->GetBodyLocation(), RopeThickness);
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
	Ball->SetWorldScale3D(FVector(BallScale * FMath::Sqrt(Power)));
	Ball->SetMaterial(0, YJJShapes::Solid(this, YJJData::Color(TEXT("CannonBall"))));
	Ball->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Ball->SetCollisionObjectType(ECC_WorldDynamic);
	Ball->SetCollisionResponseToAllChannels(ECR_Block);
	Ball->SetGenerateOverlapEvents(false);
	Ball->SetMassOverrideInKg(NAME_None, BallMassKg * Power, true);
	Ball->SetSimulatePhysics(true);
	Ball->SetUseCCD(true);
	Ball->SetNotifyRigidBodyCollision(true);
	Ball->OnComponentHit.AddDynamic(this, &AYJJCannonBall::OnHit);
	Ball->SetPhysicsLinearVelocity(Velocity);
}

void AYJJCannonBall::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 쏘자마자 자기 대포에 닿은 건 무시
	if (ExplodedAge < 0.f && Age > ArmTime)
	{
		Explode();
	}
}

void AYJJCannonBall::Explode()
{
	ExplodedAge = Age;
	const FVector Location = GetActorLocation();
	const float Radius = YJJData::Tune(TEXT("Cannon.Radius")) * FMath::Sqrt(Power);
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
			if (Dist < Radius + BodyRadiusPerScale * Target->GetScale())
			{
				const float Falloff = FMath::Clamp(1.f - Dist / (Radius * FalloffReach), MinFalloff, 1.f) * Power;
				Target->Knock(To.GetSafeNormal2D() * YJJData::Tune(TEXT("Cannon.Forward")) * Falloff + FVector(0, 0, YJJData::Tune(TEXT("Cannon.Up")) * Falloff),
					BlastLimp, YJJData::Tune(TEXT("Cannon.Damage")) * Falloff);
			}
		}
	}
	AYJJEffects::Burst(this, TEXT("BoomSmoke"), Location);
	AYJJEffects::Burst(this, TEXT("BoomFire"), Location);
	YJJAudio::Play(this, TEXT("Boom"), Location);
}

void AYJJCannonBall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Age += DeltaTime;
	const FVector Location = GetActorLocation();
	if (Location.Z < YJJMap::KillZ())
	{
		const FVector Surface(Location.X, Location.Y, YJJMap::WaterZ());
		AYJJEffects::Burst(this, TEXT("BallSplash"), Surface);
		YJJAudio::Play(this, TEXT("BallSplash"), Surface);
		Destroy();
		return;
	}
	if ((ExplodedAge >= 0.f && Age - ExplodedAge > LingerTime) || Age > MaxLife)
	{
		AYJJEffects::Burst(this, TEXT("BallPoof"), Location);
		Destroy();
	}
}
