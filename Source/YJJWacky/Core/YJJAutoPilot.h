#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "YJJAutoPilot.generated.h"

class AYJJCommander;

/**
 * 자동 테스트. 명령줄 -yjjplay 또는 콘솔 yjj.AutoPlay.
 * 타워 없이 걷기 -> 타워 종류별 처치 -> 몇 웨이브 방어를 차례로 돌리며
 * 로그([AutoPilot] PASS/FAIL)와 스크린샷(Saved/Screenshots/Play_*.png)을 남긴다.
 * -yjjquit 이 있으면 끝나고 게임을 끈다.
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

	void Add(float Wait, TFunction<void()> Action) { Steps.Add({ Wait, MoveTemp(Action) }); }
	void Shot(const FString& Name);
	void View(const FVector& Focus, float Yaw, float Pitch, float Distance);
	void Check(bool bOk, const FString& What);
	AYJJCommander* Commander() const;

	TArray<FStep> Steps;
	int32 StepIndex = -1;
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
