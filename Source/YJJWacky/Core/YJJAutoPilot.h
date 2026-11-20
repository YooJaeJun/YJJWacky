#pragma once

#include "CoreMinimal.h"
#include "Data/YJJTypes.h"
#include "HAL/IConsoleManager.h"
#include "Subsystems/WorldSubsystem.h"
#include "YJJAutoPilot.generated.h"

class AYJJCommander;
class UYJJGame;

/** 타워 시험 한 가지: 발판 0에 짓고 졸병 무리를 보내 쓰러뜨린 수를 본다 */
struct FYJJTowerTrial
{
	EYJJTowerType Type;
	const TCHAR* Name;
	/** 지켜보는 시간(초) */
	float Watch;
	int32 MinDowns;
};

/**
 * 자동 테스트. 명령줄 -yjjplay 또는 콘솔 yjj.AutoPlay.
 * 걷기 -> 타워 종류별 -> 강화 -> 부하 -> 웨이브 방어를 차례로 돌리며
 * 로그([AutoPilot] PASS/FAIL)와 스크린샷(Saved/Screenshots/Play_*.png)을 남긴다.
 * -yjjquit 이 있으면 끝나고 게임을 끈다. -yjjtests=walk,towers,upgrade,stress,waves 로 일부만, -yjjwaves=N 으로 웨이브 수.
 */
UCLASS()
class YJJWACKY_API UYJJAutoPilot : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	void StartAutoPlay();

private:
	struct FStep
	{
		float Wait = 1.f;
		TFunction<void()> Action;
	};

	static void StartFromConsole(UWorld* World);
	static void AddGoldFromConsole(const TArray<FString>& Args, UWorld* World);

	void Add(float Wait, TFunction<void()> Action) { Steps.Add({ Wait, MoveTemp(Action) }); }
	void Shot(const FString& Name);
	void View(const FVector& Focus, float Yaw, float Pitch, float Distance);
	/** 처음 카메라 (Tuning.csv Camera.Start*) */
	void ViewOverview();
	void Check(bool bOk, const FString& What);
	AYJJCommander* Commander() const;
	void Spawn(EYJJWobblerKind Kind, int32 Count);

	void AddWalkTest(const FVector& Start);
	void AddTowerTests();
	void AddUpgradeTests();
	void AddStressTest();
	void AddWaveTest();
	/** 몹 여섯 종을 한 줄로 세우고, 타워 다섯 종을 Lv1·Lv3 로 지어 가까이서 찍는다 (모양 확인용, 판정 없음) */
	void AddGalleryTest();
	/** 전시 줄의 Index 번째 자리 */
	static FVector GallerySpot(int32 Index);

	static FAutoConsoleCommandWithWorld PlayCommand;
	static FAutoConsoleCommandWithWorldAndArgs GoldCommand;

	// ---------------------------------------------------------------- 공통
	static constexpr float SettleTime = 5.f;		// 화면이 자리 잡을 때까지
	static constexpr float ShortWait = 0.2f;
	static constexpr float SetupWait = 0.3f;
	static constexpr float ShotWait = 0.5f;
	static constexpr float LastWait = 0.1f;
	static constexpr int32 TestGold = 1000;
	static constexpr int32 DefaultGoldGift = 500;
	static constexpr float MinMeasureTime = 0.001f;
	static constexpr float MillisecondsPerSecond = 1000.f;
	/** 발판 0 옆에서 내려다보는 카메라 */
	static constexpr float PadViewBack = 100.f;
	static constexpr float ViewLift = 80.f;
	static constexpr float PadViewYaw = -125.f;
	static constexpr float PadViewPitch = -26.f;
	static constexpr float PadViewDistance = 1500.f;
	static constexpr float SlingViewDistance = 1700.f;

	// ---------------------------------------------------------------- 걷기
	static constexpr int32 WalkFirstGrunts = 3;
	static constexpr int32 WalkFirstRunners = 1;
	static constexpr int32 WalkFirstBosses = 1;
	static constexpr int32 WalkSecondGrunts = 2;
	static constexpr int32 WalkSecondRunners = 1;
	static constexpr int32 WalkSecondBrutes = 1;
	static constexpr int32 WalkTotal = 9;
	static constexpr float WalkSecondBatch = 1.f;
	static constexpr float WalkCheckWait = 3.f;
	static constexpr int32 WalkMinStanding = 7;
	static constexpr float WalkShotGap = 2.f;
	static constexpr float WalkLaterWait = 5.f;
	static constexpr float WalkMinProgress = 1100.f;
	static constexpr int32 WalkMinStandingLater = 6;
	static constexpr float WalkViewAhead = 350.f;
	static constexpr float WalkViewYaw = -90.f;
	static constexpr float WalkViewPitch = -22.f;
	static constexpr float WalkViewDistance = 1300.f;
	static constexpr float WalkCloseBehind = 200.f;
	static constexpr float WalkCloseYaw = -60.f;
	static constexpr float WalkClosePitch = -18.f;
	static constexpr float WalkCloseDistance = 1000.f;
	static constexpr float WalkCloseShot = 0.6f;

	// ---------------------------------------------------------------- 타워
	static const FYJJTowerTrial Trials[];
	static const int32 TrialCount;
	static constexpr int32 TrialSquad = 8;
	/** 지켜보는 시간 중 스크린샷 세 장을 찍는 때 (비율) */
	static constexpr float TrialShotA = 0.4f;
	static constexpr float TrialShotB = 0.12f;
	static constexpr float TrialShotC = 0.12f;
	static constexpr float TrialCheck = 0.36f;
	static constexpr int32 SlingMinShots = 2;

	// ---------------------------------------------------------------- 강화
	static constexpr int32 UpgradeBrutes = 3;
	static constexpr float UpgradeWatch = 9.f;
	static constexpr int32 UpgradeMinDowns = 1;
	static constexpr int32 SlingBrutes = 1;
	static constexpr int32 SlingGrunts = 5;
	static constexpr float SlingFirstShot = 7.f;
	static constexpr float SlingShotGap = 1.5f;
	static constexpr float SlingCheckWait = 6.f;

	// ---------------------------------------------------------------- 부하
	static constexpr int32 StressBatches = 6;
	static constexpr float StressBatchGap = 0.6f;
	static constexpr int32 StressGrunts = 7;
	static constexpr int32 StressRunners = 2;
	static constexpr int32 StressBrutes = 1;
	static constexpr float StressMeasure = 6.f;
	static constexpr float StressMinFps = 45.f;

	// ---------------------------------------------------------------- 전시
	/** 첫 길(P0 -> P1) 위, 출발점에서 이만큼 떨어진 곳부터 한 줄로 */
	static constexpr float GalleryStart = 420.f;
	/** 크기 1 당 옆 놈과의 간격 */
	static constexpr float GalleryGap = 175.f;
	static constexpr float GalleryLift = 40.f;
	static constexpr float GalleryStandTime = 3.5f;
	static constexpr float GalleryWideDistance = 1500.f;
	/** 크기 1 당 거리 (한 종씩 가까이) */
	static constexpr float GalleryCloseDistance = 380.f;
	/** 가까이 찍을 때 살짝 비스듬히 */
	static constexpr float GalleryCloseYaw = -18.f;
	static constexpr float GalleryPitch = -8.f;
	/** 다 같이 찍을 때 바라보는 높이 배율 (왕 머리까지) */
	static constexpr float GalleryWideLift = 2.2f;
	/** 크기 1 당 바라보는 높이 */
	static constexpr float GalleryFocusHeight = 60.f;
	static constexpr int32 GalleryTowerLevel = 3;
	static constexpr float GalleryTowerDistance = 1000.f;
	static constexpr float GalleryTowerPitch = -18.f;
	static constexpr float GalleryTowerYaw = 35.f;
	static constexpr float GalleryTowerHeight = 120.f;
	static constexpr float GalleryBuildWait = 1.5f;

	// ---------------------------------------------------------------- 웨이브
	static constexpr int32 DefaultWaveGoal = 3;
	static constexpr float WaveShotGap = 9.f;
	/** 웨이브가 끝날 때까지 기다리는 최대 초 (1초씩) */
	static constexpr int32 WaveMaxWait = 150;
	static constexpr float EndShotWait = 4.f;
	/** 시작할 때 짓는 타워 (발판, 종류) */
	static const TPair<int32, EYJJTowerType> OpeningBuilds[];
	static const int32 OpeningBuildCount;
	/** 웨이브 사이에 돈이 되는 대로 짓는 순서 */
	static const EYJJTowerType WishList[];
	static const int32 WishCount;
	static constexpr int32 WishFirstPad = 2;
	/** 강화하고도 이만큼은 남긴다 */
	static constexpr int32 UpgradeReserve = 60;

	/** 전시용으로 세운 흐느적이 (끝나면 치운다. 병사는 게임이 따로 세지 않는다) */
	TArray<TWeakObjectPtr<AActor>> GalleryModels;

	TArray<FStep> Steps;
	int32 StepIndex = INDEX_NONE;
	float StepTimer = 0.f;
	bool bQuitWhenDone = false;
	int32 Passed = 0;
	int32 Failed = 0;
	int32 FrameCount = 0;
	float FrameTime = 0.f;
	float WorstFrame = 0.f;
	int32 MarkKills = 0;
	int32 StressFrames = 0;
	float StressTime = 0.f;
};
