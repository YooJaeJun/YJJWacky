#include "Core/YJJAutoPilot.h"

#include "Core/YJJGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/YJJTower.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Data/YJJData.h"
#include "Physics/YJJWobbler.h"
#include "Player/YJJCommander.h"
#include "UnrealClient.h"
#include "World/YJJMap.h"
#include "YJJWacky.h"

FAutoConsoleCommandWithWorld UYJJAutoPilot::PlayCommand(TEXT("yjj.AutoPlay"), TEXT("자동 플레이 검증"),
	FConsoleCommandWithWorldDelegate::CreateStatic(&UYJJAutoPilot::StartFromConsole));
FAutoConsoleCommandWithWorldAndArgs UYJJAutoPilot::GoldCommand(TEXT("yjj.Gold"), TEXT("골드 추가"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&UYJJAutoPilot::AddGoldFromConsole));

const FYJJTowerTrial UYJJAutoPilot::Trials[] = {
	{ EYJJTowerType::Punch, TEXT("Punch"), 12.f, 3 },
	{ EYJJTowerType::Cannon, TEXT("Cannon"), 14.f, 2 },
	{ EYJJTowerType::Fan, TEXT("Fan"), 13.f, 0 },
	{ EYJJTowerType::Guard, TEXT("Guard"), 14.f, 2 },
	// 1웨이브처럼 졸병만 와도 Lv1 새총이 60kg 졸병을 집어야 한다
	{ EYJJTowerType::Sling, TEXT("Sling"), 16.f, 2 },
};
const int32 UYJJAutoPilot::TrialCount = UE_ARRAY_COUNT(UYJJAutoPilot::Trials);

const TPair<int32, EYJJTowerType> UYJJAutoPilot::OpeningBuilds[] = {
	{ 0, EYJJTowerType::Punch },
	{ 3, EYJJTowerType::Guard },
	{ 1, EYJJTowerType::Cannon },
};
const int32 UYJJAutoPilot::OpeningBuildCount = UE_ARRAY_COUNT(UYJJAutoPilot::OpeningBuilds);

const EYJJTowerType UYJJAutoPilot::WishList[] = {
	EYJJTowerType::Sling, EYJJTowerType::Fan, EYJJTowerType::Punch, EYJJTowerType::Cannon, EYJJTowerType::Guard, EYJJTowerType::Punch };
const int32 UYJJAutoPilot::WishCount = UE_ARRAY_COUNT(UYJJAutoPilot::WishList);

void UYJJAutoPilot::StartFromConsole(UWorld* World)
{
	if (UYJJAutoPilot* Pilot = World ? World->GetSubsystem<UYJJAutoPilot>() : nullptr)
	{
		Pilot->StartAutoPlay();
	}
}

void UYJJAutoPilot::AddGoldFromConsole(const TArray<FString>& Args, UWorld* World)
{
	if (UYJJGame* Game = UYJJGame::Get(World))
	{
		Game->DebugReset(Game->GetGold() + (Args.Num() > 0 ? FCString::Atoi(*Args[0]) : DefaultGoldGift), false);
	}
}

TStatId UYJJAutoPilot::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UYJJAutoPilot, STATGROUP_Tickables);
}

bool UYJJAutoPilot::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UYJJAutoPilot::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bQuitWhenDone = FParse::Param(FCommandLine::Get(), TEXT("yjjquit"));
	if (FParse::Param(FCommandLine::Get(), TEXT("yjjplay")))
	{
		StartAutoPlay();
	}
}

AYJJCommander* UYJJAutoPilot::Commander() const
{
	return Cast<AYJJCommander>(GetWorld()->GetFirstPlayerController());
}

void UYJJAutoPilot::Shot(const FString& Name)
{
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Screenshots") / (Name + TEXT(".png"));
	FScreenshotRequest::RequestScreenshot(Path, true, false);
	UE_LOG(LogYJJWacky, Display, TEXT("[AutoPilot] screenshot %s"), *Path);
}

void UYJJAutoPilot::View(const FVector& Focus, float Yaw, float Pitch, float Distance)
{
	if (AYJJCommander* C = Commander())
	{
		C->SetView(Focus, Yaw, Pitch, Distance);
	}
}

void UYJJAutoPilot::ViewOverview()
{
	View(FVector(YJJData::Tune(TEXT("Camera.StartX")), YJJData::Tune(TEXT("Camera.StartY")), 0.f), YJJData::Tune(TEXT("Camera.StartYaw")),
		YJJData::Tune(TEXT("Camera.StartPitch")), YJJData::Tune(TEXT("Camera.StartDistance")));
}

void UYJJAutoPilot::Check(bool bOk, const FString& What)
{
	(bOk ? Passed : Failed)++;
	UE_LOG(LogYJJWacky, Display, TEXT("[AutoPilot] %s %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *What);
}

void UYJJAutoPilot::Spawn(EYJJWobblerKind Kind, int32 Count)
{
	if (UYJJGame* Game = UYJJGame::Get(GetWorld()))
	{
		for (int32 i = 0; i < Count; ++i)
		{
			Game->SpawnEnemy(Kind);
		}
	}
}

void UYJJAutoPilot::StartAutoPlay()
{
	Steps.Reset();
	StepIndex = INDEX_NONE;
	StepTimer = 0.f;
	Passed = Failed = 0;

	// 영웅은 영웅 시험에서만 (다른 시험 숫자가 흔들리지 않게)
	if (UYJJGame* Game = UYJJGame::Get(GetWorld()))
	{
		Game->SetHeroEnabled(false);
	}

	FString Tests;
	FParse::Value(FCommandLine::Get(), TEXT("yjjtests="), Tests, false);	// 쉼표에서 끊지 않는다
	auto Wants = [&Tests](const TCHAR* Name) { return Tests.IsEmpty() || Tests.Contains(Name); };

	Add(SettleTime, [] {});
	Add(ShotWait, [this] { Shot(TEXT("Play_00_Arena")); });
	if (Tests.Contains(TEXT("gallery")))	// 모양 확인용: 이름을 적었을 때만
	{
		AddGalleryTest();
	}
	if (Wants(TEXT("walk")) && YJJMap::Path().Num() > 0)
	{
		AddWalkTest(YJJMap::Path()[0]);
	}
	if (Wants(TEXT("towers")))
	{
		AddTowerTests();
	}
	if (Wants(TEXT("upgrade")))
	{
		AddUpgradeTests();
	}
	if (Wants(TEXT("spells")) && YJJMap::Path().Num() > 0)
	{
		AddSpellTest();
	}
	if (Wants(TEXT("hero")) && YJJMap::Path().Num() > 0)
	{
		AddHeroTest();
	}
	if (Wants(TEXT("stress")))
	{
		AddStressTest();
	}
	if (Wants(TEXT("waves")))
	{
		AddWaveTest();
	}
	Add(LastWait, [this] {
		UE_LOG(LogYJJWacky, Display, TEXT("[AutoPilot] result passed=%d failed=%d avgfps=%.0f worstframe=%.0fms"),
			Passed, Failed, FrameCount / FMath::Max(FrameTime, MinMeasureTime), WorstFrame * MillisecondsPerSecond);
	});
	UE_LOG(LogYJJWacky, Display, TEXT("[AutoPilot] start (%d steps)"), Steps.Num());
}

FVector UYJJAutoPilot::GallerySpot(int32 Index)
{
	// 옆 놈과 몸 크기만큼 띄운다 (왕이 기사를 가리지 않게)
	const TArray<FVector>& Path = YJJMap::Path();
	float Along = GalleryStart;
	for (int32 i = 1; i <= Index; ++i)
	{
		Along += GalleryGap * (AYJJWobbler::Spec(static_cast<EYJJWobblerKind>(i - 1)).Scale + AYJJWobbler::Spec(static_cast<EYJJWobblerKind>(i)).Scale) * 0.5f;
	}
	return Path[0] + (Path[1] - Path[0]).GetSafeNormal2D() * Along;
}

void UYJJAutoPilot::AddGalleryTest()
{
	UYJJGame* Game = UYJJGame::Get(GetWorld());
	const TArray<FVector>& Path = YJJMap::Path();
	if (!Game || Path.Num() < 2)
	{
		return;
	}
	const FVector Along = (Path[1] - Path[0]).GetSafeNormal2D();
	const FVector Toward(-Along.Y, Along.X, 0.f);	// 카메라 쪽
	const int32 Kinds = static_cast<int32>(EYJJWobblerKind::Hero) + 1;
	const float CameraYaw = (-Toward).Rotation().Yaw;

	Add(SetupWait, [this, Game, Kinds, Toward] {
		Game->DebugReset(TestGold, true);
		Game->SetPhotoMode(true);
		for (int32 i = 0; i < Kinds; ++i)
		{
			const EYJJWobblerKind Kind = static_cast<EYJJWobblerKind>(i);
			const float Scale = AYJJWobbler::Spec(Kind).Scale;
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector Spot = GallerySpot(i);
			AYJJWobbler* Model = GetWorld()->SpawnActor<AYJJWobbler>(AYJJWobbler::StaticClass(),
				Spot + FVector(0, 0, YJJData::Tune(TEXT("Body.HoverHeight")) * Scale + GalleryLift), Toward.Rotation(), Params);
			Model->Setup(Kind, 0.f, Spot, (Kind == EYJJWobblerKind::Guard || Kind == EYJJWobblerKind::Hero) ? YJJMaxTowerLevel : 1);
			Model->Showcase(Spot, Toward);
			GalleryModels.Add(Model);
		}
	});
	const FVector Middle = (GallerySpot(0) + GallerySpot(Kinds - 1)) * 0.5f + FVector(0, 0, GalleryFocusHeight * GalleryWideLift);
	Add(GalleryStandTime, [this, Middle, CameraYaw] { View(Middle, CameraYaw, GalleryPitch, GalleryWideDistance); });
	Add(ShotWait, [this] { Shot(TEXT("Play_G0_Lineup")); });
	for (int32 i = 0; i < Kinds; ++i)
	{
		const float Scale = AYJJWobbler::Spec(static_cast<EYJJWobblerKind>(i)).Scale;
		const FVector Focus = GallerySpot(i) + FVector(0, 0, GalleryFocusHeight * Scale);
		Add(ShortWait, [this, Focus, CameraYaw, Scale] { View(Focus, CameraYaw + GalleryCloseYaw, GalleryPitch, GalleryCloseDistance * Scale); });
		Add(ShotWait, [this, i] { Shot(FString::Printf(TEXT("Play_G1_%s"), *StaticEnum<EYJJWobblerKind>()->GetNameStringByValue(i))); });
	}

	// 타워 다섯 종: Lv1 -> Lv3
	for (int32 Level = 1; Level <= GalleryTowerLevel; Level += GalleryTowerLevel - 1)
	{
		Add(SetupWait, [this, Game, Level] {
			Game->DebugReset(TestGold * YJJTowerTypeCount * GalleryTowerLevel, true);
			for (int32 t = 0; t < YJJTowerTypeCount && t < YJJMap::Pads().Num(); ++t)
			{
				Game->Build(t, static_cast<EYJJTowerType>(t));
				for (int32 Up = 1; Up < Level; ++Up)
				{
					Game->Upgrade(t);
				}
			}
		});
		Add(GalleryBuildWait, [] {});
		for (int32 t = 0; t < YJJTowerTypeCount && t < YJJMap::Pads().Num(); ++t)
		{
			const YJJMap::FPad& Pad = YJJMap::Pads()[t];
			const float Yaw = (-Pad.Facing).Rotation().Yaw + GalleryTowerYaw;
			Add(ShortWait, [this, Pad, Yaw] { View(Pad.Location + FVector(0, 0, GalleryTowerHeight), Yaw, GalleryTowerPitch, GalleryTowerDistance); });
			Add(ShotWait, [this, t, Level] { Shot(FString::Printf(TEXT("Play_G2_Tower%d_Lv%d"), t, Level)); });
		}
	}
	Add(ShortWait, [this, Game] {
		for (const TWeakObjectPtr<AActor>& Model : GalleryModels)
		{
			if (Model.IsValid())
			{
				Model->Destroy();
			}
		}
		GalleryModels.Reset();
		Game->DebugReset(TestGold, true);
		Game->SetPhotoMode(false);
		ViewOverview();
	});
}

void UYJJAutoPilot::AddWalkTest(const FVector& Start)
{
	// 종류별로 풀어놓고 서서 걷는지 본다
	UYJJGame* Game = UYJJGame::Get(GetWorld());
	Add(ShortWait, [this, Start] {
		Spawn(EYJJWobblerKind::Grunt, WalkFirstGrunts);
		Spawn(EYJJWobblerKind::Runner, WalkFirstRunners);
		Spawn(EYJJWobblerKind::Boss, WalkFirstBosses);
		View(Start + FVector(WalkViewAhead, 0, ViewLift), WalkViewYaw, WalkViewPitch, WalkViewDistance);
	});
	Add(WalkSecondBatch, [this] {
		Spawn(EYJJWobblerKind::Grunt, WalkSecondGrunts);
		Spawn(EYJJWobblerKind::Runner, WalkSecondRunners);
		Spawn(EYJJWobblerKind::Brute, WalkSecondBrutes);
	});
	Add(WalkCheckWait, [this, Game] {
		int32 Standing = 0;
		for (const AYJJWobbler* Enemy : Game->GetEnemies())
		{
			Standing += (Enemy && Enemy->IsStanding()) ? 1 : 0;
		}
		Check(Game->GetEnemies().Num() == WalkTotal, FString::Printf(TEXT("%d enemies alive after spawn (%d)"), WalkTotal, Game->GetEnemies().Num()));
		Check(Standing >= WalkMinStanding, FString::Printf(TEXT("wobblers stand up (%d/%d standing)"), Standing, WalkTotal));
		Shot(TEXT("Play_01_Walk"));
		bMeasureWobble = true;
		YawSpinSum = TiltSpinSum = 0.0;
		SpinSamples = 0;
		for (int32 k = 0; k < KindCount; ++k)
		{
			KindYawSum[k] = 0.0;
			KindSamples[k] = 0;
		}
	});
	Add(WalkShotGap, [this] { Shot(TEXT("Play_02_Walk")); });
	Add(WalkLaterWait, [this, Game] {
		float Best = 0.f;
		int32 Standing = 0;
		for (const AYJJWobbler* Enemy : Game->GetEnemies())
		{
			if (Enemy)
			{
				Best = FMath::Max(Best, Enemy->GetProgress());
				Standing += Enemy->IsStanding() ? 1 : 0;
			}
		}
		Check(Best > WalkMinProgress, FString::Printf(TEXT("wobblers walk along the path (best %.0f cm)"), Best));
		Check(Standing >= WalkMinStandingLater, FString::Printf(TEXT("still standing while walking (%d standing, %d alive)"), Standing, Game->GetEnemies().Num()));
		bMeasureWobble = false;
		for (int32 k = 0; k < KindCount; ++k)
		{
			if (KindSamples[k] > 0)
			{
				UE_LOG(LogYJJWacky, Display, TEXT("[AutoPilot] walk yaw spin %s %.2f rad/s"), *StaticEnum<EYJJWobblerKind>()->GetNameStringByValue(k), KindYawSum[k] / KindSamples[k]);
			}
		}
		const double Samples = FMath::Max(SpinSamples, 1);
		Check(YawSpinSum / Samples < WalkMaxYawSpin, FString::Printf(TEXT("walk yaw spin %.2f rad/s (max %.2f)"), YawSpinSum / Samples, WalkMaxYawSpin));
		Check(TiltSpinSum / Samples < WalkMaxTiltSpin, FString::Printf(TEXT("walk tilt spin %.2f rad/s (max %.2f)"), TiltSpinSum / Samples, WalkMaxTiltSpin));
		View(YJJMap::PointAt(Best - WalkCloseBehind) + FVector(0, 0, ViewLift), WalkCloseYaw, WalkClosePitch, WalkCloseDistance);
	});
	Add(WalkCloseShot, [this] { Shot(TEXT("Play_03_WalkClose")); });
}

void UYJJAutoPilot::AddTowerTests()
{
	// 발판 0(첫 구간 안쪽)에 하나 짓고 졸병 무리를 보낸다
	UYJJGame* Game = UYJJGame::Get(GetWorld());
	for (int32 t = 0; t < TrialCount; ++t)
	{
		const FYJJTowerTrial Trial = Trials[t];
		Add(SetupWait, [this, Game, Trial] {
			Game->DebugReset(TestGold, true);
			Check(Game->Build(0, Trial.Type), FString::Printf(TEXT("build %s"), Trial.Name));
			Spawn(EYJJWobblerKind::Grunt, TrialSquad);
			MarkKills = Game->GetKills();
			if (YJJMap::Pads().Num() > 0)
			{
				View(YJJMap::Pads()[0].PathPoint + FVector(-PadViewBack, 0, ViewLift), PadViewYaw, PadViewPitch, PadViewDistance);
			}
		});
		Add(Trial.Watch * TrialShotA, [this, Trial] { Shot(FString::Printf(TEXT("Play_1%s_a"), Trial.Name)); });
		Add(Trial.Watch * TrialShotB, [this, Trial] { Shot(FString::Printf(TEXT("Play_1%s_b"), Trial.Name)); });
		Add(Trial.Watch * TrialShotC, [this, Trial] { Shot(FString::Printf(TEXT("Play_1%s_c"), Trial.Name)); });
		Add(Trial.Watch * TrialCheck, [this, Game, Trial] {
			const int32 Downs = Game->GetKills() - MarkKills;
			const AYJJTower* Tower = Game->GetTower(0);
			const int32 Shots = Tower ? Tower->GetShots() : INDEX_NONE;
			Check(Downs >= Trial.MinDowns, FString::Printf(TEXT("%s tower downs %d of %d (need %d), shots %d, drowned total %d"),
				Trial.Name, Downs, TrialSquad, Trial.MinDowns, Shots, Game->GetDrowned()));
			if (Trial.Type == EYJJTowerType::Sling)
			{
				Check(Shots >= SlingMinShots, FString::Printf(TEXT("sling grabbed and flung (%d shots)"), Shots));
			}
		});
	}
}

void UYJJAutoPilot::AddUpgradeTests()
{
	// 펀치를 Lv3까지 올리고, 무거운 뚱보도 날리는지 본다. 새총 Lv3은 뚱보를 집는다
	UYJJGame* Game = UYJJGame::Get(GetWorld());
	Add(SetupWait, [this, Game] {
		Game->DebugReset(TestGold, true);
		Game->Build(0, EYJJTowerType::Punch);
		const int32 Before = Game->GetGold();
		bool bAllUpgraded = true;
		for (int32 Level = 1; Level < YJJMaxTowerLevel; ++Level)
		{
			bAllUpgraded = Game->Upgrade(0) && bAllUpgraded;
		}
		const bool bPastMax = Game->Upgrade(0);
		const int32 Spent = Before - Game->GetGold();
		int32 Expected = 0;
		for (int32 Level = 1; Level < YJJMaxTowerLevel; ++Level)
		{
			Expected += UYJJGame::UpgradeCost(EYJJTowerType::Punch, Level);
		}
		Check(bAllUpgraded && !bPastMax && Game->GetPadLevel(0) == YJJMaxTowerLevel && Spent == Expected,
			FString::Printf(TEXT("punch upgrades to Lv%d and stops (level %d, spent %d, expected %d)"), YJJMaxTowerLevel, Game->GetPadLevel(0), Spent, Expected));
		const int32 Invested = UYJJGame::TowerInfo(EYJJTowerType::Punch).Cost + Expected;
		Check(Game->SellValue(0) == FMath::RoundToInt(Invested * YJJData::Tune(TEXT("Game.SellRatio"))),
			FString::Printf(TEXT("sell value counts upgrades (%d)"), Game->SellValue(0)));
		Spawn(EYJJWobblerKind::Brute, UpgradeBrutes);
		MarkKills = Game->GetKills();
		View(YJJMap::Pads()[0].PathPoint + FVector(-PadViewBack, 0, ViewLift), PadViewYaw, PadViewPitch, PadViewDistance);
	});
	Add(UpgradeWatch, [this] { Shot(TEXT("Play_4PunchLv3")); });
	Add(UpgradeWatch, [this, Game] {
		Check(Game->GetKills() - MarkKills >= UpgradeMinDowns, FString::Printf(TEXT("Lv3 punch downs brutes (%d of %d)"), Game->GetKills() - MarkKills, UpgradeBrutes));
	});
	Add(SetupWait, [this, Game] {
		Game->DebugReset(TestGold, true);
		Game->Build(0, EYJJTowerType::Sling);
		for (int32 Level = 1; Level < YJJMaxTowerLevel; ++Level)
		{
			Game->Upgrade(0);
		}
		Spawn(EYJJWobblerKind::Brute, SlingBrutes);
		Spawn(EYJJWobblerKind::Grunt, SlingGrunts);
		View(YJJMap::Pads()[0].PathPoint + FVector(-PadViewBack, 0, ViewLift), PadViewYaw, PadViewPitch, SlingViewDistance);
	});
	Add(SlingFirstShot, [this] { Shot(TEXT("Play_4SlingLv3_a")); });
	Add(SlingShotGap, [this] { Shot(TEXT("Play_4SlingLv3_b")); });
	Add(SlingCheckWait, [this, Game] {
		const AYJJTower* Tower = Game->GetTower(0);
		const int32 Shots = Tower ? Tower->GetShots() : INDEX_NONE;
		Check(Shots >= SlingMinShots, FString::Printf(TEXT("Lv3 sling throws (%d shots)"), Shots));
	});
}

void UYJJAutoPilot::AddStressTest()
{
	// 한꺼번에 많이 풀어놓고 프레임을 잰다
	UYJJGame* Game = UYJJGame::Get(GetWorld());
	Add(SetupWait, [this, Game] {
		Game->DebugReset(TestGold, true);
		ViewOverview();
	});
	for (int32 Batch = 0; Batch < StressBatches; ++Batch)
	{
		Add(StressBatchGap, [this] {
			Spawn(EYJJWobblerKind::Grunt, StressGrunts);
			Spawn(EYJJWobblerKind::Runner, StressRunners);
			Spawn(EYJJWobblerKind::Brute, StressBrutes);
		});
	}
	Add(ShortWait, [this] { StressFrames = 0; StressTime = 0.f; });
	Add(StressMeasure, [this, Game] {
		const float Fps = StressFrames / FMath::Max(StressTime, MinMeasureTime);
		const int32 Spawned = StressBatches * (StressGrunts + StressRunners + StressBrutes);
		Check(Fps > StressMinFps, FString::Printf(TEXT("%d wobblers at once: %.0f fps (%d alive)"), Spawned, Fps, Game->GetEnemies().Num()));
		Shot(TEXT("Play_15_Crowd"));
	});
}

void UYJJAutoPilot::AddSpellTest()
{
	UYJJGame* Game = UYJJGame::Get(GetWorld());
	// 바위: 졸병 무리가 걸어오는 앞에 떨어뜨린다
	const FVector BoulderAt = YJJMap::PointAt(SpellBoulderAt);
	Add(SetupWait, [this, Game, BoulderAt] {
		Game->DebugReset(TestGold, true);
		View(BoulderAt, SpellViewYaw, SpellViewPitch, SpellViewDistance);
		Spawn(EYJJWobblerKind::Grunt, SpellSquad);
		SpellKills = Game->GetKills();
		SpellLimpMax = 0;
	});
	Add(SpellWalkWait, [this, Game, BoulderAt] { Check(Game->CastSpell(EYJJSpell::Boulder, BoulderAt), TEXT("boulder cast")); });
	for (int32 i = 0; i < SpellSamples; ++i)
	{
		Add(SpellSampleGap, [this, Game, i] {
			int32 Limp = 0;
			for (const AYJJWobbler* Enemy : Game->GetEnemies())
			{
				Limp += (Enemy && Enemy->IsLimp()) ? 1 : 0;
			}
			SpellLimpMax = FMath::Max(SpellLimpMax, Limp);
			if (i == SpellShotSample)
			{
				Shot(TEXT("Play_40_Boulder"));
			}
		});
	}
	Add(ShortWait, [this, Game] {
		const int32 Downs = Game->GetKills() - SpellKills;
		Check(SpellLimpMax + Downs >= SpellMinHits, FString::Printf(TEXT("boulder knocked %d of %d (limp at once %d, down %d)"), SpellLimpMax + Downs, SpellSquad, SpellLimpMax, Downs));
		Check(!Game->CanCast(EYJJSpell::Boulder) && Game->GetSpellCharge(EYJJSpell::Boulder) < 1.f, TEXT("boulder is on cooldown after use"));
	});

	// 지원군: 하늘에서 떨어져 서고, 시간이 다 되면 사라진다
	const FVector HelpAt = YJJMap::PointAt(SpellReinforceAt);
	Add(ShortWait, [this, Game, HelpAt] {
		Game->DebugReset(TestGold, true);
		View(HelpAt, SpellViewYaw, SpellViewPitch, SpellViewDistance);
		Check(Game->CastSpell(EYJJSpell::Reinforce, HelpAt), TEXT("reinforce cast"));
	});
	Add(SpellLandWait, [this, Game] {
		int32 Standing = 0;
		for (const AYJJWobbler* Helper : Game->GetHelpers())
		{
			Standing += (Helper && Helper->IsStanding()) ? 1 : 0;
		}
		const int32 Want = FMath::RoundToInt(UYJJGame::SpellTune(EYJJSpell::Reinforce, TEXT("Count")));
		Check(Game->GetHelpers().Num() == Want && Standing >= SpellMinStanding, FString::Printf(TEXT("reinforcements landed (%d of %d, %d standing)"), Game->GetHelpers().Num(), Want, Standing));
		Shot(TEXT("Play_41_Reinforce"));
	});
	Add(UYJJGame::SpellTune(EYJJSpell::Reinforce, TEXT("Life")) - SpellLandWait + SpellExpireExtra, [this, Game] {
		Check(Game->GetHelpers().Num() == 0, FString::Printf(TEXT("reinforcements went home (%d left)"), Game->GetHelpers().Num()));
	});

	// 일찍 부르기: 1웨이브가 다 나온 뒤 다음 웨이브를 부르면 남은 시간만큼 골드
	Add(ShortWait, [this, Game] {
		Game->DebugReset(TestGold, true);
		ViewOverview();
		Game->StartWave();
	});
	Add(UYJJGame::WaveDef(1).Duration + SpellCallExtra, [this, Game] {
		const bool bCan = Game->CanCallWave();
		const int32 Bonus = Game->GetEarlyBonus();
		const int32 Before = Game->GetGold();
		Game->StartWave();
		Check(bCan && Bonus > 0 && Game->GetWave() == 2 && Game->GetGold() == Before + Bonus,
			FString::Printf(TEXT("call wave 2 early (can %d, bonus %d, wave %d, gold %d -> %d)"), bCan ? 1 : 0, Bonus, Game->GetWave(), Before, Game->GetGold()));
		Shot(TEXT("Play_42_CallEarly"));
	});
	Add(ShortWait, [this, Game] { Game->DebugReset(TestGold, true); });
}

void UYJJAutoPilot::AddHeroTest()
{
	UYJJGame* Game = UYJJGame::Get(GetWorld());
	const FVector Start = UYJJGame::SnapToPath(YJJMap::PointAt(YJJData::Tune(TEXT("Hero.StartAt"))));
	Add(SetupWait, [this, Game, Start] {
		Game->DebugReset(TestGold, true);
		Game->SetHeroEnabled(true);
		View(Start, SpellViewYaw, SpellViewPitch, HeroViewDistance);
	});
	Add(YJJData::Tune(TEXT("Hero.FirstDelay")) + HeroSpawnWait, [this, Game] {
		const AYJJWobbler* Hero = Game->GetHero();
		Check(Hero && Hero->IsStanding(), TEXT("hero stands at start"));
		Shot(TEXT("Play_50_Hero"));
	});

	// 보내기
	const FVector Goal = UYJJGame::SnapToPath(YJJMap::PointAt(HeroMoveAt));
	Add(ShortWait, [this, Game, Goal] {
		Check(Game->MoveHero(Goal), TEXT("move hero"));
		View(Goal, SpellViewYaw, SpellViewPitch, HeroViewDistance);
	});
	Add(HeroWalkWait, [this, Game, Goal] {
		const AYJJWobbler* Hero = Game->GetHero();
		const float Dist = Hero ? FVector::Dist2D(Hero->GetBodyLocation(), Goal) : -1.f;
		Check(Hero && Hero->IsStanding() && Dist < HeroArriveDist, FString::Printf(TEXT("hero walked to the goal (%.0f cm off)"), Dist));
		HeroStompsSeen = Game->GetHeroStomps();
		Spawn(EYJJWobblerKind::Grunt, HeroSquad);
	});

	// 졸병 무리가 오면 배치기
	Add(HeroFightWait - HeroFightSamples * HeroFightGap, [] {});
	for (int32 i = 0; i < HeroFightSamples; ++i)
	{
		Add(HeroFightGap, [this, i] {
			if (i == HeroShotSample)
			{
				Shot(TEXT("Play_51_HeroFight"));
			}
		});
	}
	Add(ShortWait, [this, Game] {
		Check(Game->GetHeroStomps() > HeroStompsSeen, FString::Printf(TEXT("hero belly bumped (%d)"), Game->GetHeroStomps() - HeroStompsSeen));
		Game->DebugReset(TestGold, false);
	});

	// 물에 빠뜨리면 잠시 뒤 다시 온다
	Add(ShortWait, [this, Game] {
		if (AYJJWobbler* Hero = Game->GetHero())
		{
			FVector Dir = FVector::ForwardVector;
			YJJMap::PointAt(YJJMap::Project(Hero->GetBodyLocation()), &Dir);
			Hero->Knock(FVector(-Dir.Y, Dir.X, 0.f) * HeroShoveSpeed + FVector(0, 0, HeroShoveUp), HeroShoveLimp, 0.f);
		}
	});
	Add(HeroDrownWait, [this, Game] {
		Check(!Game->GetHero() && Game->GetHeroRespawnLeft() > 0.f, FString::Printf(TEXT("hero fell in the water (back in %.1fs)"), Game->GetHeroRespawnLeft()));
	});
	Add(YJJData::Tune(TEXT("Hero.RespawnTime")) - HeroDrownWait + HeroBackExtra, [this, Game] {
		const AYJJWobbler* Hero = Game->GetHero();
		Check(Hero != nullptr, TEXT("hero came back"));
		Game->SetHeroEnabled(false);
		Game->DebugReset(TestGold, true);
	});
}

void UYJJAutoPilot::AddWaveTest()
{
	// 실제 웨이브: 사람처럼 번 돈으로 짓는다
	UYJJGame* Game = UYJJGame::Get(GetWorld());
	int32 WaveGoal = DefaultWaveGoal;
	FParse::Value(FCommandLine::Get(), TEXT("yjjwaves="), WaveGoal);
	WaveGoal = FMath::Clamp(WaveGoal, 1, UYJJGame::GetWaveCount());
	Add(SetupWait, [this, Game] {
		Game->DebugReset(YJJData::TuneInt(TEXT("Game.StartGold")), true);
		ViewOverview();
		for (int32 i = 0; i < OpeningBuildCount; ++i)
		{
			Game->Build(OpeningBuilds[i].Key, OpeningBuilds[i].Value);
		}
		Game->StartWave();
	});
	for (int32 WaveNumber = 1; WaveNumber <= WaveGoal; ++WaveNumber)
	{
		Add(WaveShotGap, [this, WaveNumber] { Shot(FString::Printf(TEXT("Play_2Wave%d_a"), WaveNumber)); });
		Add(WaveShotGap, [this, WaveNumber] { Shot(FString::Printf(TEXT("Play_2Wave%d_b"), WaveNumber)); });
		// 웨이브가 끝날 때까지 (Action 이 없는 단계 = 웨이브가 끝나면 건너뛰는 1초 대기)
		for (int32 i = 0; i < WaveMaxWait; ++i)
		{
			Add(1.f, nullptr);
		}
		Add(ShortWait, [this, Game, WaveNumber] {
			Check(Game->GetPhase() != EYJJPhase::Wave && Game->GetWave() == WaveNumber,
				FString::Printf(TEXT("wave %d cleared (lives %d, kills %d, drowned %d, leaked %d, gold %d)"), WaveNumber, Game->GetLives(), Game->GetKills(),
					Game->GetDrowned(), Game->GetLeaked(), Game->GetGold()));
			// 번 돈으로 더 짓는다
			const int32 PadCount = YJJMap::Pads().Num();
			int32 Pad = WishFirstPad;
			for (int32 w = 0; w < WishCount; ++w)
			{
				while (Pad < PadCount && Game->HasTower(Pad)) { Pad++; }
				if (Pad < PadCount && Game->GetGold() >= UYJJGame::TowerInfo(WishList[w]).Cost)
				{
					Game->Build(Pad, WishList[w]);
				}
			}
			// 남은 돈으로 낮은 단계부터 강화
			for (int32 Pass = 1; Pass < YJJMaxTowerLevel; ++Pass)
			{
				for (int32 i = 0; i < PadCount; ++i)
				{
					if (Game->GetPadLevel(i) == Pass && Game->GetGold() >= UYJJGame::UpgradeCost(Game->GetPadTower(i), Pass) + UpgradeReserve)
					{
						Game->Upgrade(i);
					}
				}
			}
			Game->StartWave();
		});
	}
	Add(EndShotWait, [this] { Shot(TEXT("Play_3_End")); });
	Add(LastWait, [this, Game, WaveGoal] {
		Check(Game->GetLives() > 0, FString::Printf(TEXT("pudding survives %d waves (lives %d, kills %d, drowned %d, leaked %d)"), WaveGoal, Game->GetLives(),
			Game->GetKills(), Game->GetDrowned(), Game->GetLeaked()));
	});
}

void UYJJAutoPilot::SampleWobble()
{
	// 서서 걷는 놈만: 맞고 날아가는 회전은 빼고 걸음 자체의 흔들림만 잰다
	UYJJGame* Game = UYJJGame::Get(GetWorld());
	if (!Game)
	{
		return;
	}
	for (const AYJJWobbler* Enemy : Game->GetEnemies())
	{
		if (Enemy && Enemy->IsStanding())
		{
			const FVector Spin = Enemy->GetBodySpin();
			YawSpinSum += FMath::Abs(Spin.Z);
			const int32 Kind = static_cast<int32>(Enemy->GetKind());
			KindYawSum[Kind] += FMath::Abs(Spin.Z);
			KindSamples[Kind]++;
			TiltSpinSum += FVector2D(Spin.X, Spin.Y).Size();
			SpinSamples++;
		}
	}
}

void UYJJAutoPilot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bMeasureWobble)
	{
		SampleWobble();
	}
	if (Steps.Num() == 0)
	{
		return;
	}
	const float RealDelta = static_cast<float>(FApp::GetDeltaTime());
	if (StepIndex >= 1)
	{
		FrameCount++;
		FrameTime += RealDelta;
		WorstFrame = FMath::Max(WorstFrame, RealDelta);
	}
	StressFrames++;
	StressTime += RealDelta;
	StepTimer -= RealDelta;

	// Action이 없는 단계는 "웨이브가 끝나면 건너뛰는 대기"
	UYJJGame* Game = UYJJGame::Get(GetWorld());
	while (StepIndex >= 0 && StepIndex < Steps.Num() && !Steps[StepIndex].Action && Game && Game->GetPhase() != EYJJPhase::Wave)
	{
		StepIndex++;
		StepTimer = StepIndex < Steps.Num() ? Steps[StepIndex].Wait : 0.f;
	}
	if (StepTimer > 0.f)
	{
		return;
	}
	if (StepIndex >= 0 && StepIndex < Steps.Num() && Steps[StepIndex].Action)
	{
		Steps[StepIndex].Action();
	}
	StepIndex++;
	if (StepIndex < Steps.Num())
	{
		StepTimer = Steps[StepIndex].Wait;
	}
	else
	{
		Steps.Reset();
		if (bQuitWhenDone)
		{
			UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
		}
	}
}
