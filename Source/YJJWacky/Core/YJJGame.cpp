#include "Core/YJJGame.h"

#include "Core/YJJAudio.h"
#include "Data/YJJLoc.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gameplay/YJJEffects.h"
#include "Gameplay/YJJTower.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "World/YJJArena.h"
#include "World/YJJMap.h"
#include "World/YJJShapes.h"
#include "YJJWacky.h"

UYJJGame* UYJJGame::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UYJJGame>() : nullptr;
}

int32 UYJJGame::GetMaxLives()
{
	return FMath::RoundToInt(Tune(TEXT("Game.Lives")));
}

FString UYJJGame::TowerName(EYJJTowerType Type)
{
	return YJJLoc::Get(*TowerInfo(Type).NameKey);
}

FString UYJJGame::TowerBlurb(EYJJTowerType Type)
{
	return YJJLoc::Get(*TowerInfo(Type).BlurbKey);
}

FString UYJJGame::UpgradeText(EYJJTowerType Type, int32 Level)
{
	const FYJJTowerRow& Row = TowerInfo(Type);
	// 새총 강화 문구에는 다음 단계의 무게 한도가 들어간다 ({0}kg까지 집기)
	return YJJLoc::Format(Level <= 1 ? *Row.Upgrade2Key : *Row.Upgrade3Key, { FMath::RoundToInt(Row.ParamA(Level + 1)) });
}

int32 UYJJGame::UpgradeCost(EYJJTowerType Type, int32 Level)
{
	if (Level >= YJJMaxTowerLevel)
	{
		return 0;
	}
	const FYJJTowerRow& Row = TowerInfo(Type);
	const int32 Step = FMath::Max(1, YJJData::TuneInt(TEXT("Game.UpgradeCostStep")));
	return FMath::RoundToInt(Row.Cost * (Level == 1 ? Row.Upgrade2CostRatio : Row.Upgrade3CostRatio) / Step) * Step;
}

float UYJJGame::SlingLimitKg(int32 Level)
{
	return TowerInfo(EYJJTowerType::Sling).ParamA(Level);
}

void UYJJGame::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Gold = FMath::RoundToInt(Tune(TEXT("Game.StartGold")));
	Lives = GetMaxLives();
}

float UYJJGame::GetFlickCharge() const
{
	return 1.f - FMath::Clamp(FlickCooldown / FMath::Max(Tune(TEXT("Flick.Delay")), 0.01f), 0.f, 1.f);
}

TArray<TPair<EYJJWobblerKind, int32>> UYJJGame::WaveCounts(int32 WaveNumber)
{
	TArray<TPair<EYJJWobblerKind, int32>> Result;
	const FYJJWaveDef& Def = WaveDef(WaveNumber);
	for (const FYJJWaveShare& Share : Def.Shares)
	{
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Def.BudgetKg * Share.Share / AYJJWobbler::Spec(Share.Kind).WeightKg));
		Result.Add({ Share.Kind, Count });
	}
	return Result;
}

TStatId UYJJGame::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UYJJGame, STATGROUP_Tickables);
}

bool UYJJGame::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

AYJJArena* UYJJGame::Arena()
{
	if (!CachedArena)
	{
		for (TActorIterator<AYJJArena> It(GetWorld()); It; ++It)
		{
			CachedArena = *It;
			break;
		}
	}
	return CachedArena;
}

FString UYJJGame::GetWavePreview(int32 WaveNumber) const
{
	if (WaveNumber < 1 || WaveNumber > GetWaveCount())
	{
		return FString();
	}
	FString Text = YJJLoc::Format(TEXT("Hud.WavePreview"), { FText::AsNumber(WaveDef(WaveNumber).BudgetKg).ToString() });
	for (const TPair<EYJJWobblerKind, int32>& Count : WaveCounts(WaveNumber))
	{
		Text += TEXT("  ") + YJJLoc::Format(TEXT("Hud.WavePreviewItem"), { YJJLoc::Get(*AYJJWobbler::Spec(Count.Key).NameKey), Count.Value });
	}
	return Text;
}

// ============================================================ 타워

bool UYJJGame::HasTower(int32 Pad) const
{
	return Towers.IsValidIndex(Pad) && Towers[Pad] != nullptr;
}

EYJJTowerType UYJJGame::GetPadTower(int32 Pad) const
{
	return HasTower(Pad) ? Towers[Pad]->GetType() : EYJJTowerType::Punch;
}

int32 UYJJGame::GetPadLevel(int32 Pad) const
{
	return HasTower(Pad) ? Towers[Pad]->GetLevel() : 0;
}

int32 UYJJGame::SellValue(int32 Pad) const
{
	return HasTower(Pad) ? FMath::RoundToInt(Towers[Pad]->GetInvested() * Tune(TEXT("Game.SellRatio"))) : 0;
}

bool UYJJGame::Upgrade(int32 Pad)
{
	if (!HasTower(Pad) || Phase == EYJJPhase::Won || Phase == EYJJPhase::Lost)
	{
		return false;
	}
	AYJJTower* Tower = Towers[Pad];
	const int32 Level = Tower->GetLevel();
	if (Level >= YJJMaxTowerLevel)
	{
		return false;
	}
	const int32 Cost = UpgradeCost(Tower->GetType(), Level);
	const FVector Location = Tower->GetActorLocation();
	if (Gold < Cost)
	{
		YJJAudio::Play(this, TEXT("Deny"));
		AddFloater(Location, YJJLoc::Get(TEXT("Float.NoGold")), TEXT("NoGold"));
		return false;
	}
	Gold -= Cost;
	Tower->SetLevel(Level + 1, Cost);
	if (Tower->GetType() == EYJJTowerType::Guard)
	{
		// 병사는 더 큰 몽둥이를 든 새 병사로 바꾼다
		if (IsValid(Guards[Pad]))
		{
			AYJJEffects::Burst(this, TEXT("GuardPoof"), Guards[Pad]->GetBodyLocation());
			Guards[Pad]->Destroy();
		}
		SpawnGuard(Pad, Level + 1);
	}
	AYJJEffects::Burst(this, TEXT("Upgrade"), Location);
	AddFloater(Location, YJJLoc::Format(TEXT("Float.Level"), { Level + 1 }), TEXT("Level"));
	YJJAudio::Play(this, TEXT("Upgrade"));
	UE_LOG(LogYJJWacky, Display, TEXT("[Game] upgrade %s on pad %d -> Lv%d (gold %d)"), *TowerName(Tower->GetType()), Pad, Level + 1, Gold);
	return true;
}

bool UYJJGame::Build(int32 Pad, EYJJTowerType Type)
{
	const TArray<YJJMap::FPad>& Pads = YJJMap::Pads();
	if (!Pads.IsValidIndex(Pad) || HasTower(Pad) || Phase == EYJJPhase::Won || Phase == EYJJPhase::Lost)
	{
		return false;
	}
	const FYJJTowerRow& Info = TowerInfo(Type);
	if (Gold < Info.Cost)
	{
		YJJAudio::Play(this, TEXT("Deny"));
		AddFloater(Pads[Pad].Location, YJJLoc::Get(TEXT("Float.NoGold")), TEXT("NoGold"));
		return false;
	}
	Gold -= Info.Cost;
	Towers.SetNum(Pads.Num());
	Guards.SetNum(Pads.Num());
	GuardRespawn.SetNumZeroed(Pads.Num());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AYJJTower* Tower = GetWorld()->SpawnActor<AYJJTower>(AYJJTower::StaticClass(), Pads[Pad].Location, Pads[Pad].Facing.Rotation(), Params);
	Tower->Setup(Type, Pad, Info.Cost);
	Towers[Pad] = Tower;
	if (Type == EYJJTowerType::Guard)
	{
		SpawnGuard(Pad, 1);
	}
	AYJJEffects::Burst(this, TEXT("BuildPoof"), Pads[Pad].Location);
	YJJAudio::Play(this, TEXT("Build"));
	UE_LOG(LogYJJWacky, Display, TEXT("[Game] build %s on pad %d (gold %d)"), *TowerName(Type), Pad, Gold);
	return true;
}

void UYJJGame::SpawnGuard(int32 Pad, int32 Level)
{
	const YJJMap::FPad& Info = YJJMap::Pads()[Pad];
	// 발판 앞 길 가장자리에 선다
	const FVector Post = Info.PathPoint - Info.Facing * (YJJMap::PathWidth() * 0.5f - Tune(TEXT("Map.GuardInset")));
	const float Scale = AYJJWobbler::Spec(EYJJWobblerKind::Guard).Scale;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AYJJWobbler* Guard = GetWorld()->SpawnActor<AYJJWobbler>(AYJJWobbler::StaticClass(), Post + FVector(0, 0, StandHeightPerScale * Scale + GuardSpawnLift), Info.Facing.Rotation(), Params);
	Guard->Setup(EYJJWobblerKind::Guard, 0.f, Post, Level);
	Guards[Pad] = Guard;
}

bool UYJJGame::Sell(int32 Pad)
{
	if (!HasTower(Pad))
	{
		return false;
	}
	const int32 Refund = SellValue(Pad);
	Gold += Refund;
	const FVector Location = Towers[Pad]->GetActorLocation();
	Towers[Pad]->Destroy();
	Towers[Pad] = nullptr;
	if (Guards[Pad])
	{
		AYJJEffects::Burst(this, TEXT("GuardPoof"), Guards[Pad]->GetBodyLocation());
		Guards[Pad]->Destroy();
		Guards[Pad] = nullptr;
	}
	AYJJEffects::Burst(this, TEXT("SellPoof"), Location);
	AddFloater(Location, YJJLoc::Format(TEXT("Float.Gold"), { Refund }), TEXT("Refund"));
	YJJAudio::Play(this, TEXT("Sell"));
	return true;
}

// ============================================================ 웨이브

void UYJJGame::StartWave()
{
	if (Phase != EYJJPhase::Build || Wave >= GetWaveCount())
	{
		return;
	}
	Wave++;
	Phase = EYJJPhase::Wave;
	WaveTime = 0.f;
	SpawnIndex = 0;
	SpawnQueue.Reset();
	// 몬스터별 마릿수를 정하고, 각자 맡은 구간에 고르게 흩뿌린다
	const FYJJWaveDef& Def = WaveDef(Wave);
	const TArray<TPair<EYJJWobblerKind, int32>> Counts = WaveCounts(Wave);
	for (int32 g = 0; g < Counts.Num(); ++g)
	{
		const FYJJWaveShare& Share = Def.Shares[g];
		const int32 Count = Counts[g].Value;
		for (int32 i = 0; i < Count; ++i)
		{
			const float T = Count > 1 ? static_cast<float>(i) / (Count - 1) : 0.5f;
			SpawnQueue.Add({ Def.Duration * FMath::Lerp(Share.From, Share.To, T) + FMath::FRandRange(0.f, SpawnTimeJitter), Share.Kind });
		}
	}
	SpawnQueue.Sort([](const FSpawn& A, const FSpawn& B) { return A.Time < B.Time; });
	ShowBanner(YJJLoc::Format(TEXT("Banner.Wave"), { Wave }), TEXT("BannerWave"));
	YJJAudio::Play(this, TEXT("WaveStart"));
	UE_LOG(LogYJJWacky, Display, TEXT("[Game] wave %d start (%d enemies, %d kg)"), Wave, SpawnQueue.Num(), Def.BudgetKg);
}

AYJJWobbler* UYJJGame::SpawnEnemy(EYJJWobblerKind Kind)
{
	const TArray<FVector>& Path = YJJMap::Path();
	const FYJJEnemyRow& Info = AYJJWobbler::Spec(Kind);
	const FVector Dir = (Path[1] - Path[0]).GetSafeNormal();
	const FVector Right(-Dir.Y, Dir.X, 0.f);
	// 덩치가 클수록 길 가운데로
	const float LaneWidth = FMath::Clamp(SpawnLaneWidth * (1.f - (Info.Scale - SpawnLaneReferenceScale)), SpawnLaneMin, SpawnLaneMax);
	// 방금 나온 놈과 겹치면 서로 튕겨 나가므로, 몇 군데 찍어 보고 가장 한적한 자리를 고른다
	float Lane = 0.f;
	FVector Location = Path[0] + FVector(0, 0, StandHeightPerScale * Info.Scale + SpawnLift);
	float BestGap = TNumericLimits<float>::Lowest();
	for (int32 Try = 0; Try < SpawnTries; ++Try)
	{
		const float TryLane = FMath::FRandRange(-1.f, 1.f) * LaneWidth;
		const FVector TryLocation = Path[0] + Dir * FMath::FRandRange(-SpawnBehind, SpawnAhead) + Right * TryLane + FVector(0, 0, StandHeightPerScale * Info.Scale + SpawnLift);
		float Gap = TNumericLimits<float>::Max();
		for (const AYJJWobbler* Other : Enemies)
		{
			if (Other)
			{
				Gap = FMath::Min(Gap, FVector::Dist2D(Other->GetBodyLocation(), TryLocation) - BodyRadiusPerScale * (Other->GetScale() + Info.Scale));
			}
		}
		if (Gap > BestGap)
		{
			BestGap = Gap;
			Lane = TryLane;
			Location = TryLocation;
		}
	}
	if (BestGap < 0.f)
	{
		// 자리가 꽉 찼다: 겹치는 놈들 머리 위에서 떨어뜨린다 (몸끼리 파묻힌 채 생기면 물리가 터진다)
		float Top = 0.f;
		for (const AYJJWobbler* Other : Enemies)
		{
			if (Other && FVector::Dist2D(Other->GetBodyLocation(), Location) < BodyRadiusPerScale * (Other->GetScale() + Info.Scale))
			{
				Top = FMath::Max(Top, Other->GetBodyLocation().Z + HeadTopPerScale * Other->GetScale());
			}
		}
		Location.Z = FMath::Max(Location.Z, Top + DropClearancePerScale * Info.Scale + DropClearance);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AYJJWobbler* Enemy = GetWorld()->SpawnActor<AYJJWobbler>(AYJJWobbler::StaticClass(), Location, Dir.Rotation(), Params);
	Enemy->Setup(Kind, Lane, FVector::ZeroVector);
	return Enemy;
}

void UYJJGame::RegisterWobbler(AYJJWobbler* Wobbler)
{
	if (Wobbler && Wobbler->IsEnemy())
	{
		Enemies.Add(Wobbler);
	}
}

void UYJJGame::OnWobblerDown(AYJJWobbler* Wobbler, bool bDrowned)
{
	if (!Wobbler || !Wobbler->IsEnemy())
	{
		return;
	}
	if (Enemies.Remove(Wobbler) == 0)
	{
		return;
	}
	const FYJJEnemyRow& Info = AYJJWobbler::Spec(Wobbler->GetKind());
	const FVector Location = Wobbler->GetBodyLocation();
	Gold += Info.Bounty;
	Kills++;
	if (bDrowned)
	{
		Drowned++;
		AddFloater(FVector(Location.X, Location.Y, YJJMap::WaterZ()), YJJLoc::Get(TEXT("Float.Splash")), TEXT("Splash"));
	}
	AddFloater(Location, YJJLoc::Format(TEXT("Float.Gold"), { Info.Bounty }), TEXT("Bounty"), Info.Scale);
	YJJAudio::Play(this, TEXT("Coin"));

	// 떼죽음: 짧은 시간에 여럿이 쓰러지면 슬로 모션
	const double Now = GetWorld()->GetRealTimeSeconds();
	RecentDowns.Add(Now);
	const double Window = Tune(TEXT("Combo.Window"));
	RecentDowns.RemoveAll([Now, Window](double T) { return Now - T > Window; });
	BestCombo = FMath::Max(BestCombo, RecentDowns.Num());
	const bool bBossDown = Wobbler->GetKind() == EYJJWobblerKind::Boss;
	if ((RecentDowns.Num() >= FMath::RoundToInt(Tune(TEXT("Combo.Count"))) || bBossDown) && Now - LastComboTime > Tune(TEXT("Combo.Cooldown")))
	{
		LastComboTime = Now;
		SlowMoLeft = Tune(TEXT("Combo.SlowMoTime"));
		UGameplayStatics::SetGlobalTimeDilation(this, Tune(TEXT("Combo.SlowMo")));
		const int32 Bonus = FMath::RoundToInt(bBossDown ? Tune(TEXT("Combo.BossBonus")) : RecentDowns.Num() * Tune(TEXT("Combo.BonusPerKill")));
		Gold += Bonus;
		ShowBanner(bBossDown ? YJJLoc::Get(TEXT("Banner.Boss")) : YJJLoc::Format(TEXT("Banner.Combo"), { RecentDowns.Num() }), TEXT("BannerCombo"));
		YJJAudio::Play(this, TEXT("Combo"));
	}
}

void UYJJGame::OnEnemyReachedBase(AYJJWobbler* Wobbler)
{
	if (!Wobbler || Enemies.Remove(Wobbler) == 0)
	{
		return;
	}
	const int32 Bite = AYJJWobbler::Spec(Wobbler->GetKind()).Bite;
	Leaked++;
	const FVector Base = YJJMap::BaseLocation();
	AYJJEffects::Burst(this, TEXT("Chomp"), Wobbler->GetBodyLocation());
	YJJAudio::Play(this, TEXT("Chomp"), Base);
	if (AYJJArena* A = Arena())
	{
		A->WobblePudding(PuddingWobbleBase + PuddingWobblePerBite * Bite);
	}
	if (Phase == EYJJPhase::Won || Phase == EYJJPhase::Lost)
	{
		return;
	}
	Lives = FMath::Max(0, Lives - Bite);
	AddFloater(Base, YJJLoc::Format(TEXT("Float.Lives"), { Bite }), TEXT("Lives"));
	if (Lives <= 0)
	{
		EndGame(false);
	}
}

void UYJJGame::EndGame(bool bWon)
{
	Phase = bWon ? EYJJPhase::Won : EYJJPhase::Lost;
	EndAge = 0.f;
	YJJAudio::Play(this, bWon ? TEXT("Win") : TEXT("Lose"));
	UE_LOG(LogYJJWacky, Display, TEXT("[Game] %s wave=%d kills=%d drowned=%d leaked=%d lives=%d"), bWon ? TEXT("WON") : TEXT("LOST"), Wave, Kills, Drowned, Leaked, Lives);
}

AYJJWobbler* UYJJGame::FindNearestEnemy(const FVector& Location, float Radius) const
{
	AYJJWobbler* Best = nullptr;
	float BestSq = Radius * Radius;
	for (AYJJWobbler* Enemy : Enemies)
	{
		if (Enemy && Enemy->IsAlive())
		{
			const float DistSq = FVector::DistSquared2D(Enemy->GetBodyLocation(), Location);
			if (DistSq < BestSq)
			{
				BestSq = DistSq;
				Best = Enemy;
			}
		}
	}
	return Best;
}

bool UYJJGame::Flick(AYJJWobbler* Enemy, const FVector& Direction)
{
	if (!Enemy || !Enemy->IsAlive() || FlickCooldown > 0.f)
	{
		return false;
	}
	FlickCooldown = Tune(TEXT("Flick.Delay"));
	const FVector Location = Enemy->GetBodyLocation();
	Enemy->Knock(Direction.GetSafeNormal2D() * Tune(TEXT("Flick.Side")) + FVector(0, 0, Tune(TEXT("Flick.Up"))), Tune(TEXT("Flick.Limp")), Tune(TEXT("Flick.Damage")));
	AYJJEffects::Burst(this, TEXT("Flick"), Location);
	AddFloater(Location, YJJLoc::Get(TEXT("Float.Flick")), TEXT("Flick"));
	YJJAudio::Play(this, TEXT("Flick"), Location);
	return true;
}

void UYJJGame::DebugReset(int32 InGold, bool bClear)
{
	Gold = InGold;
	if (!bClear)
	{
		return;
	}
	for (AYJJWobbler* Enemy : Enemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->Destroy();
		}
	}
	Enemies.Reset();
	for (int32 Pad = 0; Pad < Towers.Num(); ++Pad)
	{
		if (Towers[Pad])
		{
			Towers[Pad]->Destroy();
			Towers[Pad] = nullptr;
		}
		if (IsValid(Guards[Pad]))
		{
			Guards[Pad]->Destroy();
		}
		Guards[Pad] = nullptr;
	}
	SpawnQueue.Reset();
	SpawnIndex = 0;
	Phase = EYJJPhase::Build;
	Lives = GetMaxLives();
	Wave = 0;
}

// ============================================================ HUD

void UYJJGame::AddFloater(const FVector& Location, const FString& Text, const TCHAR* Style, float Scale)
{
	if (Floaters.Num() > YJJData::TuneInt(TEXT("Hud.MaxFloaters")))
	{
		Floaters.RemoveAt(0);
	}
	const FYJJTextStyleRow& Row = YJJData::TextStyle(Style);
	FYJJFloater& Floater = Floaters.AddDefaulted_GetRef();
	Floater.Location = Location + FVector(0, 0, Row.OffsetZ);
	Floater.Text = Text;
	Floater.Color = Row.LinearColor();
	Floater.Size = Row.Size + Row.SizePerScale * Scale;
}

void UYJJGame::ShowBanner(const FString& Text, const TCHAR* Style)
{
	const FYJJTextStyleRow& Row = YJJData::TextStyle(Style);
	BannerText = Text;
	BannerColor = Row.LinearColor();
	BannerSize = Row.Size;
	BannerAge = 0.f;
	BannerLife = Row.Life;
}

// ============================================================ 매 프레임

void UYJJGame::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const float RealDelta = FMath::Min(static_cast<float>(FApp::GetDeltaTime()), MaxRealDelta);

	if (SlowMoLeft > 0.f)
	{
		SlowMoLeft -= RealDelta;
		if (SlowMoLeft <= 0.f)
		{
			UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
		}
	}

	for (int32 i = Floaters.Num() - 1; i >= 0; --i)
	{
		Floaters[i].Age += RealDelta;
		if (Floaters[i].Age > Tune(TEXT("Hud.FloaterLife")))
		{
			Floaters.RemoveAt(i);
		}
	}
	BannerAge += RealDelta;
	FlickCooldown = FMath::Max(0.f, FlickCooldown - RealDelta);
	Enemies.RemoveAll([](const TObjectPtr<AYJJWobbler>& Enemy) { return !IsValid(Enemy) || !Enemy->IsAlive(); });

	// 물에 빠진 병사는 잠시 뒤 새로 온다
	for (int32 Pad = 0; Pad < Towers.Num(); ++Pad)
	{
		if (Towers[Pad] && Towers[Pad]->GetType() == EYJJTowerType::Guard && !IsValid(Guards[Pad]))
		{
			GuardRespawn[Pad] += DeltaTime;
			if (GuardRespawn[Pad] > Tune(TEXT("Game.GuardRespawn")))
			{
				GuardRespawn[Pad] = 0.f;
				SpawnGuard(Pad, Towers[Pad]->GetLevel());
			}
		}
	}

	if (Phase == EYJJPhase::Won || Phase == EYJJPhase::Lost)
	{
		EndAge += RealDelta;
		return;
	}
	if (Phase != EYJJPhase::Wave)
	{
		return;
	}

	WaveTime += DeltaTime;
	while (SpawnIndex < SpawnQueue.Num() && SpawnQueue[SpawnIndex].Time <= WaveTime && Enemies.Num() < FMath::RoundToInt(Tune(TEXT("Game.MaxAlive"))))
	{
		SpawnEnemy(SpawnQueue[SpawnIndex].Kind);
		SpawnIndex++;
	}
	if (SpawnIndex >= SpawnQueue.Num() && Enemies.Num() == 0)
	{
		SpawnQueue.Reset();
		SpawnIndex = 0;
		if (Wave >= GetWaveCount())
		{
			EndGame(true);
			return;
		}
		Phase = EYJJPhase::Build;
		const int32 Bonus = FMath::RoundToInt(Tune(TEXT("Game.WaveBonusBase")) + Wave * Tune(TEXT("Game.WaveBonusPerWave")));
		Gold += Bonus;
		ShowBanner(YJJLoc::Format(TEXT("Banner.WaveClear"), { Wave, Bonus }), TEXT("BannerClear"));
		YJJAudio::Play(this, TEXT("WaveClear"));
		UE_LOG(LogYJJWacky, Display, TEXT("[Game] wave %d clear kills=%d drowned=%d leaked=%d lives=%d gold=%d"), Wave, Kills, Drowned, Leaked, Lives, Gold);
	}
}
