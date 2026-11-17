#include "Core/YJJAutoPilot.h"

#include "Core/YJJGame.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Player/YJJCommander.h"
#include "UnrealClient.h"
#include "World/YJJMap.h"
#include "YJJWacky.h"

namespace
{
	FAutoConsoleCommandWithWorld PlayCommand(TEXT("yjj.AutoPlay"), TEXT("자동 플레이 검증"), FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UYJJAutoPilot* Pilot = World ? World->GetSubsystem<UYJJAutoPilot>() : nullptr)
		{
			Pilot->StartAutoPlay();
		}
	}));

	FAutoConsoleCommandWithWorldAndArgs GoldCommand(TEXT("yjj.Gold"), TEXT("골드 추가"), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (UYJJGame* Game = UYJJGame::Get(World))
		{
			Game->DebugReset(Game->GetGold() + (Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 500), false);
		}
	}));
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

void UYJJAutoPilot::Check(bool bOk, const FString& What)
{
	(bOk ? Passed : Failed)++;
	UE_LOG(LogYJJWacky, Display, TEXT("[AutoPilot] %s %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *What);
}

void UYJJAutoPilot::StartAutoPlay()
{
	Steps.Reset();
	StepIndex = -1;
	StepTimer = 0.f;
	Passed = Failed = 0;
	UYJJGame* Game = UYJJGame::Get(GetWorld());
	const TArray<YJJMap::FPad>& Pads = YJJMap::Pads();
	const FVector Start = YJJMap::Path()[0];

	auto SpawnSquad = [Game](int32 Grunts, int32 Runners, int32 Brutes, int32 Bosses)
	{
		for (int32 i = 0; i < Grunts; ++i) { Game->SpawnEnemy(EYJJWobblerKind::Grunt); }
		for (int32 i = 0; i < Runners; ++i) { Game->SpawnEnemy(EYJJWobblerKind::Runner); }
		for (int32 i = 0; i < Brutes; ++i) { Game->SpawnEnemy(EYJJWobblerKind::Brute); }
		for (int32 i = 0; i < Bosses; ++i) { Game->SpawnEnemy(EYJJWobblerKind::Boss); }
	};

	FString Tests;
	FParse::Value(FCommandLine::Get(), TEXT("yjjtests="), Tests, false);	// 쉼표에서 끊지 않는다
	auto Wants = [&Tests](const TCHAR* Name) { return Tests.IsEmpty() || Tests.Contains(Name); };

	// 화면이 자리 잡을 때까지
	Add(5.f, [] {});
	Add(0.5f, [this] { Shot(TEXT("Play_00_Arena")); });

	if (Wants(TEXT("walk")))
	{
	// 1. 걷기: 종류별로 풀어놓고 서서 걷는지 본다
	Add(0.2f, [this, SpawnSquad, Start] {
		SpawnSquad(3, 1, 0, 1);
		View(Start + FVector(350, 0, 80), -90.f, -22.f, 1300.f);
	});
	Add(1.f, [SpawnSquad] { SpawnSquad(2, 1, 1, 0); });
	Add(3.f, [this, Game] {
		int32 Standing = 0;
		for (const AYJJWobbler* Enemy : Game->GetEnemies())
		{
			Standing += (Enemy && Enemy->IsStanding()) ? 1 : 0;
		}
		Check(Game->GetEnemies().Num() == 9, FString::Printf(TEXT("9 enemies alive after spawn (%d)"), Game->GetEnemies().Num()));
		Check(Standing >= 7, FString::Printf(TEXT("wobblers stand up (%d/9 standing)"), Standing));
		Shot(TEXT("Play_01_Walk"));
	});
	Add(2.f, [this] { Shot(TEXT("Play_02_Walk")); });
	Add(5.f, [this, Game] {
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
		Check(Best > 1100.f, FString::Printf(TEXT("wobblers walk along the path (best %.0f cm in 10 s)"), Best));
		Check(Standing >= 6, FString::Printf(TEXT("still standing while walking (%d standing, %d alive)"), Standing, Game->GetEnemies().Num()));
		View(YJJMap::PointAt(Best - 200.f) + FVector(0, 0, 80), -60.f, -18.f, 1000.f);
	});
	Add(0.6f, [this] { Shot(TEXT("Play_03_WalkClose")); });
	}

	if (Wants(TEXT("towers")))
	{
	// 2. 타워 종류별: 발판 0(첫 구간 안쪽)에 하나 짓고 졸병 8마리를 보낸다
	struct FTrial { EYJJTowerType Type; const TCHAR* Name; float Watch; int32 MinKills; };
	const FTrial Trials[] = {
		{ EYJJTowerType::Punch, TEXT("Punch"), 12.f, 3 },
		{ EYJJTowerType::Cannon, TEXT("Cannon"), 14.f, 2 },
		{ EYJJTowerType::Fan, TEXT("Fan"), 13.f, 0 },
		{ EYJJTowerType::Guard, TEXT("Guard"), 14.f, 2 } };
	for (const FTrial& Trial : Trials)
	{
		Add(0.3f, [this, Game, Trial, Pads, SpawnSquad] {
			Game->DebugReset(1000, true);
			Check(Game->Build(0, Trial.Type), FString::Printf(TEXT("build %s"), Trial.Name));
			SpawnSquad(8, 0, 0, 0);
			MarkKills = Game->GetKills();
			View(Pads[0].PathPoint + FVector(-100, 0, 80), -125.f, -26.f, 1500.f);
		});
		Add(Trial.Watch * 0.4f, [this, Trial] { Shot(FString::Printf(TEXT("Play_1%s_a"), Trial.Name)); });
		Add(Trial.Watch * 0.12f, [this, Trial] { Shot(FString::Printf(TEXT("Play_1%s_b"), Trial.Name)); });
		Add(Trial.Watch * 0.12f, [this, Trial] { Shot(FString::Printf(TEXT("Play_1%s_c"), Trial.Name)); });
		Add(Trial.Watch * 0.36f, [this, Game, Trial] {
			const int32 Downs = Game->GetKills() - MarkKills;
			Check(Downs >= Trial.MinKills, FString::Printf(TEXT("%s tower downs %d of 8 (need %d), drowned total %d"), Trial.Name, Downs, Trial.MinKills, Game->GetDrowned()));
		});
	}
	}

	if (Wants(TEXT("stress")))
	{
	// 3. 부하: 60마리를 한꺼번에 풀어놓고 프레임을 잰다
	Add(0.3f, [this, Game] {
		Game->DebugReset(1000, true);
		View(FVector(-360, 60, 0), -90.f, -54.f, 5000.f);
		StressFrames = 0;
		StressTime = 0.f;
	});
	for (int32 Batch = 0; Batch < 6; ++Batch)
	{
		Add(0.6f, [SpawnSquad] { SpawnSquad(7, 2, 1, 0); });
	}
	Add(0.2f, [this] { StressFrames = 0; StressTime = 0.f; });
	Add(6.f, [this, Game] {
		const float Fps = StressFrames / FMath::Max(StressTime, 0.001f);
		Check(Fps > 45.f, FString::Printf(TEXT("60 wobblers at once: %.0f fps (%d alive)"), Fps, Game->GetEnemies().Num()));
		Shot(TEXT("Play_15_Crowd"));
	});
	}

	if (Wants(TEXT("waves")))
	{
	// 4. 실제 웨이브: 사람처럼 번 돈으로 짓는다 (기본 세 판, -yjjwaves=N 으로 지정)
	int32 WaveGoal = 3;
	FParse::Value(FCommandLine::Get(), TEXT("yjjwaves="), WaveGoal);
	WaveGoal = FMath::Clamp(WaveGoal, 1, UYJJGame::WaveCount);
	Add(0.3f, [this, Game] {
		Game->DebugReset(240, true);
		View(FVector(-360, 60, 0), -90.f, -54.f, 5000.f);
		Game->Build(0, EYJJTowerType::Punch);
		Game->Build(3, EYJJTowerType::Guard);
		Game->Build(1, EYJJTowerType::Cannon);
		Game->StartWave();
	});
	for (int32 WaveNumber = 1; WaveNumber <= WaveGoal; ++WaveNumber)
	{
		Add(9.f, [this, WaveNumber] { Shot(FString::Printf(TEXT("Play_2Wave%d_a"), WaveNumber)); });
		Add(9.f, [this, WaveNumber] { Shot(FString::Printf(TEXT("Play_2Wave%d_b"), WaveNumber)); });
		// 웨이브가 끝날 때까지 (최대 150초)
		for (int32 i = 0; i < 150; ++i)
		{
			Add(1.f, [] {});
			Steps.Last().Action = nullptr;
		}
		Add(0.2f, [this, Game, WaveNumber] {
			Check(Game->GetPhase() != EYJJPhase::Wave && Game->GetWave() == WaveNumber,
				FString::Printf(TEXT("wave %d cleared (lives %d, kills %d, drowned %d, leaked %d, gold %d)"), WaveNumber, Game->GetLives(), Game->GetKills(), Game->GetDrowned(), Game->GetLeaked(), Game->GetGold()));
			// 번 돈으로 더 짓는다
			const EYJJTowerType Wish[] = { EYJJTowerType::Fan, EYJJTowerType::Punch, EYJJTowerType::Cannon, EYJJTowerType::Guard, EYJJTowerType::Punch };
			int32 Pad = 2;
			for (const EYJJTowerType Type : Wish)
			{
				while (Pad < YJJMap::Pads().Num() && Game->HasTower(Pad)) { Pad++; }
				if (Pad < YJJMap::Pads().Num() && Game->GetGold() >= UYJJGame::TowerInfo(Type).Cost)
				{
					Game->Build(Pad, Type);
				}
			}
			Game->StartWave();
		});
	}
	Add(4.f, [this] { Shot(TEXT("Play_3_End")); });
	Add(0.1f, [this, Game, WaveGoal] {
		Check(Game->GetLives() > 0, FString::Printf(TEXT("pudding survives %d waves (lives %d, kills %d, drowned %d, leaked %d)"), WaveGoal, Game->GetLives(), Game->GetKills(), Game->GetDrowned(), Game->GetLeaked()));
	});
	}

	Add(0.1f, [this] {
		UE_LOG(LogYJJWacky, Display, TEXT("[AutoPilot] result passed=%d failed=%d avgfps=%.0f worstframe=%.0fms"), Passed, Failed, FrameCount / FMath::Max(FrameTime, 0.001f), WorstFrame * 1000.f);
	});
	UE_LOG(LogYJJWacky, Display, TEXT("[AutoPilot] start (%d steps)"), Steps.Num());
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
