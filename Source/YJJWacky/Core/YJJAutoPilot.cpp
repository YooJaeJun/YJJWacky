#include "Core/YJJAutoPilot.h"

#include "Core/YJJGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/YJJTower.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
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

	FString Tests;
	FParse::Value(FCommandLine::Get(), TEXT("yjjtests="), Tests, false);	// 쉼표에서 끊지 않는다
	auto Wants = [&Tests](const TCHAR* Name) { return Tests.IsEmpty() || Tests.Contains(Name); };

	Add(SettleTime, [] {});
	Add(ShotWait, [this] { Shot(TEXT("Play_00_Arena")); });
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

void UYJJAutoPilot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
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
