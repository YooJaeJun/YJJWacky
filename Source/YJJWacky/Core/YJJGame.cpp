#include "Core/YJJGame.h"

#include "Core/YJJAudio.h"
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

namespace
{
	struct FGroup
	{
		EYJJWobblerKind Kind;
		int32 Count;
		float Interval;
		float Delay;
	};

	using K = EYJJWobblerKind;

	const TArray<TArray<FGroup>>& Waves()
	{
		static const TArray<TArray<FGroup>> Table = {
			{ { K::Grunt, 8, 1.1f, 0.f } },
			{ { K::Grunt, 12, 0.9f, 0.f } },
			{ { K::Grunt, 10, 0.9f, 0.f }, { K::Runner, 6, 0.6f, 6.f } },
			{ { K::Grunt, 14, 0.8f, 0.f }, { K::Brute, 3, 3.f, 4.f } },
			{ { K::Runner, 12, 0.5f, 0.f }, { K::Grunt, 10, 0.8f, 3.f }, { K::Boss, 1, 1.f, 12.f } },
			{ { K::Grunt, 20, 0.6f, 0.f }, { K::Brute, 6, 2.2f, 3.f } },
			{ { K::Runner, 20, 0.45f, 0.f }, { K::Brute, 8, 2.f, 2.f } },
			{ { K::Grunt, 30, 0.5f, 0.f }, { K::Runner, 10, 0.5f, 6.f }, { K::Brute, 6, 2.f, 8.f } },
			{ { K::Runner, 24, 0.4f, 0.f }, { K::Brute, 12, 1.5f, 2.f } },
			{ { K::Grunt, 36, 0.45f, 0.f }, { K::Brute, 12, 1.6f, 4.f }, { K::Boss, 3, 6.f, 10.f } },
		};
		return Table;
	}

	const TCHAR* KindName(EYJJWobblerKind Kind)
	{
		switch (Kind)
		{
		case K::Runner: return TEXT("날쌘돌이");
		case K::Brute: return TEXT("뚱보");
		case K::Boss: return TEXT("대장");
		default: return TEXT("졸병");
		}
	}

	/** 동시에 살아 있는 적 상한 (물리 부하) */
	constexpr int32 MaxAlive = 60;
}

UYJJGame* UYJJGame::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UYJJGame>() : nullptr;
}

const FYJJTowerInfo& UYJJGame::TowerInfo(EYJJTowerType Type)
{
	static const FYJJTowerInfo Infos[YJJTowerTypeCount] = {
		{ TEXT("펀치 타워"), TEXT("길 밖으로 뻥!"), 60, 640.f },
		{ TEXT("대포"), TEXT("멀리서 쾅!"), 110, 1700.f },
		{ TEXT("선풍기"), TEXT("후우~ 밀어내기"), 80, 900.f },
		{ TEXT("몽둥이 병사"), TEXT("빙글빙글 퍽퍽"), 70, 300.f },
	};
	return Infos[static_cast<int32>(Type)];
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
	if (WaveNumber < 1 || WaveNumber > WaveCount)
	{
		return FString();
	}
	FString Text;
	for (const FGroup& Group : Waves()[WaveNumber - 1])
	{
		if (!Text.IsEmpty())
		{
			Text += TEXT("  ");
		}
		Text += FString::Printf(TEXT("%s x%d"), KindName(Group.Kind), Group.Count);
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

int32 UYJJGame::SellValue(int32 Pad) const
{
	return HasTower(Pad) ? FMath::RoundToInt(TowerInfo(Towers[Pad]->GetType()).Cost * 0.6f) : 0;
}

bool UYJJGame::Build(int32 Pad, EYJJTowerType Type)
{
	const TArray<YJJMap::FPad>& Pads = YJJMap::Pads();
	if (!Pads.IsValidIndex(Pad) || HasTower(Pad) || Phase == EYJJPhase::Won || Phase == EYJJPhase::Lost)
	{
		return false;
	}
	const FYJJTowerInfo& Info = TowerInfo(Type);
	if (Gold < Info.Cost)
	{
		YJJAudio::Play(this, EYJJSound::Deny);
		AddFloater(Pads[Pad].Location + FVector(0, 0, 150), TEXT("골드 부족!"), YJJShapes::Hex(0xFF6B6B), 28.f);
		return false;
	}
	Gold -= Info.Cost;
	Towers.SetNum(Pads.Num());
	Guards.SetNum(Pads.Num());
	GuardRespawn.SetNumZeroed(Pads.Num());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AYJJTower* Tower = GetWorld()->SpawnActor<AYJJTower>(AYJJTower::StaticClass(), Pads[Pad].Location, Pads[Pad].Facing.Rotation(), Params);
	Tower->Setup(Type, Pad);
	Towers[Pad] = Tower;
	if (Type == EYJJTowerType::Guard)
	{
		SpawnGuard(Pad);
	}
	AYJJEffects::Burst(this, Pads[Pad].Location + FVector(0, 0, 40), YJJShapes::Hex(0xFFFFFF), 12, 420.f, 60.f, 0.5f, -100.f, false, 0.3f);
	YJJAudio::Play(this, EYJJSound::Build);
	UE_LOG(LogYJJWacky, Display, TEXT("[Game] build %s on pad %d (gold %d)"), Info.Name, Pad, Gold);
	return true;
}

void UYJJGame::SpawnGuard(int32 Pad)
{
	const YJJMap::FPad& Info = YJJMap::Pads()[Pad];
	// 발판 앞 길 가장자리에 선다
	const FVector Post = Info.PathPoint - Info.Facing * (YJJMap::PathWidth * 0.5f - 110.f);
	const float Scale = AYJJWobbler::Spec(EYJJWobblerKind::Guard).Scale;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AYJJWobbler* Guard = GetWorld()->SpawnActor<AYJJWobbler>(AYJJWobbler::StaticClass(), Post + FVector(0, 0, 92.f * Scale + 60.f), Info.Facing.Rotation(), Params);
	Guard->Setup(EYJJWobblerKind::Guard, 0.f, Post);
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
		AYJJEffects::Burst(this, Guards[Pad]->GetBodyLocation(), YJJShapes::Hex(0xFFFFFF), 8, 300.f, 50.f, 0.5f, -100.f, false, 0.4f);
		Guards[Pad]->Destroy();
		Guards[Pad] = nullptr;
	}
	AYJJEffects::Burst(this, Location + FVector(0, 0, 60), YJJShapes::Hex(0xFFFFFF), 12, 380.f, 60.f, 0.5f, -100.f, false, 0.4f);
	AddFloater(Location + FVector(0, 0, 160), FString::Printf(TEXT("+%d"), Refund), YJJShapes::Hex(0xFFD23F), 30.f);
	YJJAudio::Play(this, EYJJSound::Sell);
	return true;
}

// ============================================================ 웨이브

void UYJJGame::StartWave()
{
	if (Phase != EYJJPhase::Build || Wave >= WaveCount)
	{
		return;
	}
	Wave++;
	Phase = EYJJPhase::Wave;
	WaveTime = 0.f;
	SpawnIndex = 0;
	SpawnQueue.Reset();
	for (const FGroup& Group : Waves()[Wave - 1])
	{
		for (int32 i = 0; i < Group.Count; ++i)
		{
			SpawnQueue.Add({ Group.Delay + i * Group.Interval, Group.Kind });
		}
	}
	SpawnQueue.Sort([](const FSpawn& A, const FSpawn& B) { return A.Time < B.Time; });
	ShowBanner(FString::Printf(TEXT("웨이브 %d"), Wave), YJJShapes::Hex(0xFFFFFF), 1.8f);
	YJJAudio::Play(this, EYJJSound::WaveStart);
	UE_LOG(LogYJJWacky, Display, TEXT("[Game] wave %d start (%d enemies)"), Wave, SpawnQueue.Num());
}

AYJJWobbler* UYJJGame::SpawnEnemy(EYJJWobblerKind Kind)
{
	const TArray<FVector>& Path = YJJMap::Path();
	const FYJJWobblerSpec& Info = AYJJWobbler::Spec(Kind);
	const FVector Dir = (Path[1] - Path[0]).GetSafeNormal();
	const FVector Right(-Dir.Y, Dir.X, 0.f);
	// 덩치가 클수록 길 가운데로
	const float LaneWidth = FMath::Clamp(130.f - 130.f * (Info.Scale - 1.2f), 25.f, 150.f);
	// 방금 나온 놈과 겹치면 서로 튕겨 나가므로, 몇 군데 찍어 보고 가장 한적한 자리를 고른다
	float Lane = 0.f;
	FVector Location = Path[0] + FVector(0, 0, 92.f * Info.Scale + 30.f);
	float BestGap = TNumericLimits<float>::Lowest();
	for (int32 Try = 0; Try < 12; ++Try)
	{
		const float TryLane = FMath::FRandRange(-1.f, 1.f) * LaneWidth;
		const FVector TryLocation = Path[0] + Dir * FMath::FRandRange(-260.f, 60.f) + Right * TryLane + FVector(0, 0, 92.f * Info.Scale + 30.f);
		float Gap = 100000.f;
		for (const AYJJWobbler* Other : Enemies)
		{
			if (Other)
			{
				Gap = FMath::Min(Gap, FVector::Dist2D(Other->GetBodyLocation(), TryLocation) - 45.f * (Other->GetScale() + Info.Scale));
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
			if (Other && FVector::Dist2D(Other->GetBodyLocation(), Location) < 45.f * (Other->GetScale() + Info.Scale))
			{
				Top = FMath::Max(Top, Other->GetBodyLocation().Z + 95.f * Other->GetScale());
			}
		}
		Location.Z = FMath::Max(Location.Z, Top + 50.f * Info.Scale + 40.f);
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
	const FYJJWobblerSpec& Info = AYJJWobbler::Spec(Wobbler->GetKind());
	const FVector Location = Wobbler->GetBodyLocation();
	Gold += Info.Bounty;
	Kills++;
	if (bDrowned)
	{
		Drowned++;
		AddFloater(FVector(Location.X, Location.Y, YJJMap::WaterZ + 150.f), TEXT("풍덩!"), YJJShapes::Hex(0xBFEFFF), 30.f);
	}
	AddFloater(Location + FVector(0, 0, 120.f), FString::Printf(TEXT("+%d"), Info.Bounty), YJJShapes::Hex(0xFFD23F), 24.f + Info.Scale * 4.f);
	YJJAudio::Play(this, EYJJSound::Coin, FVector::ZeroVector, 0.5f, FMath::FRandRange(0.95f, 1.15f));

	// 떼죽음: 짧은 시간에 여럿이 쓰러지면 슬로 모션
	const double Now = GetWorld()->GetRealTimeSeconds();
	RecentDowns.Add(Now);
	RecentDowns.RemoveAll([Now](double T) { return Now - T > 1.3; });
	BestCombo = FMath::Max(BestCombo, RecentDowns.Num());
	const bool bBossDown = Wobbler->GetKind() == EYJJWobblerKind::Boss;
	if ((RecentDowns.Num() >= 5 || bBossDown) && Now - LastComboTime > 4.0)
	{
		LastComboTime = Now;
		SlowMoLeft = 1.1f;
		UGameplayStatics::SetGlobalTimeDilation(this, 0.3f);
		const int32 Bonus = bBossDown ? 25 : RecentDowns.Num();
		Gold += Bonus;
		ShowBanner(bBossDown ? FString(TEXT("대장 격파!")) : FString::Printf(TEXT("떼죽음 x%d!"), RecentDowns.Num()), YJJShapes::Hex(0xFFD23F), 1.8f);
		YJJAudio::Play(this, EYJJSound::Combo);
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
	AYJJEffects::Burst(this, Wobbler->GetBodyLocation(), YJJShapes::Hex(0xFFE9A8), 10, 420.f, 40.f, 0.6f, 900.f, false, 0.7f);
	YJJAudio::Play(this, EYJJSound::Chomp, Base);
	if (AYJJArena* A = Arena())
	{
		A->WobblePudding(0.5f + 0.2f * Bite);
	}
	if (Phase == EYJJPhase::Won || Phase == EYJJPhase::Lost)
	{
		return;
	}
	Lives = FMath::Max(0, Lives - Bite);
	AddFloater(Base + FVector(0, 0, 420.f), FString::Printf(TEXT("-%d"), Bite), YJJShapes::Hex(0xFF6B6B), 40.f);
	if (Lives <= 0)
	{
		EndGame(false);
	}
}

void UYJJGame::EndGame(bool bWon)
{
	Phase = bWon ? EYJJPhase::Won : EYJJPhase::Lost;
	EndAge = 0.f;
	YJJAudio::Play(this, bWon ? EYJJSound::Win : EYJJSound::Lose);
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
	FlickCooldown = FlickDelay;
	const FVector Location = Enemy->GetBodyLocation();
	Enemy->Knock(Direction.GetSafeNormal2D() * 420.f + FVector(0, 0, 520.f), 1.5f, 6.f);
	AYJJEffects::Burst(this, Location, YJJShapes::Hex(0xFFE066), 7, 460.f, 24.f, 0.4f, 700.f, true, 0.6f);
	AddFloater(Location + FVector(0, 0, 130.f), TEXT("딱!"), YJJShapes::Hex(0xFFFFFF), 30.f);
	YJJAudio::Play(this, EYJJSound::Bonk, Location, 1.f, FMath::FRandRange(1.1f, 1.4f));
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
	Lives = MaxLives;
	Wave = 0;
}

// ============================================================ HUD

void UYJJGame::AddFloater(const FVector& Location, const FString& Text, const FLinearColor& Color, float Size)
{
	if (Floaters.Num() > 60)
	{
		Floaters.RemoveAt(0);
	}
	FYJJFloater& Floater = Floaters.AddDefaulted_GetRef();
	Floater.Location = Location;
	Floater.Text = Text;
	Floater.Color = Color;
	Floater.Size = Size;
}

void UYJJGame::ShowBanner(const FString& Text, const FLinearColor& Color, float Life)
{
	BannerText = Text;
	BannerColor = Color;
	BannerAge = 0.f;
	BannerLife = Life;
}

// ============================================================ 매 프레임

void UYJJGame::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const float RealDelta = FMath::Min(static_cast<float>(FApp::GetDeltaTime()), 0.1f);

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
		if (Floaters[i].Age > 1.2f)
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
			if (GuardRespawn[Pad] > 3.f)
			{
				GuardRespawn[Pad] = 0.f;
				SpawnGuard(Pad);
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
	while (SpawnIndex < SpawnQueue.Num() && SpawnQueue[SpawnIndex].Time <= WaveTime && Enemies.Num() < MaxAlive)
	{
		SpawnEnemy(SpawnQueue[SpawnIndex].Kind);
		SpawnIndex++;
	}
	if (SpawnIndex >= SpawnQueue.Num() && Enemies.Num() == 0)
	{
		SpawnQueue.Reset();
		SpawnIndex = 0;
		if (Wave >= WaveCount)
		{
			EndGame(true);
			return;
		}
		Phase = EYJJPhase::Build;
		const int32 Bonus = 20 + Wave * 4;
		Gold += Bonus;
		ShowBanner(FString::Printf(TEXT("웨이브 %d 막았다!  +%d"), Wave, Bonus), YJJShapes::Hex(0xB8F28A), 2.4f);
		YJJAudio::Play(this, EYJJSound::WaveClear);
		UE_LOG(LogYJJWacky, Display, TEXT("[Game] wave %d clear kills=%d drowned=%d leaked=%d lives=%d gold=%d"), Wave, Kills, Drowned, Leaked, Lives, Gold);
	}
}
