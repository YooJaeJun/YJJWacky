#pragma once

#include "CoreMinimal.h"
#include "Core/YJJGame.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "YJJCommander.generated.h"

class UCameraComponent;

/** 전장을 내려다보는 카메라 */
UCLASS()
class YJJWACKY_API AYJJCommanderPawn : public APawn
{
	GENERATED_BODY()

public:
	AYJJCommanderPawn();

	/** 카메라 화각 (도) */
	static constexpr float DefaultFieldOfView = 55.f;

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;
};

/**
 * 플레이어 조작.
 *  WASD/방향키 이동 · 휠 확대 · Q/E 회전
 *  1~5 타워 고르기 · 빈 발판 좌클릭 건설 · 지은 타워 좌클릭 강화 · 우클릭 판매 · 적 좌클릭 딱밤
 *  Space 다음 웨이브 · R 다시 하기 (끝난 뒤)
 */
UCLASS()
class YJJWACKY_API AYJJCommander : public APlayerController
{
	GENERATED_BODY()

public:
	AYJJCommander();

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	EYJJTowerType GetSelected() const { return Selected; }
	int32 GetHoverPad() const { return HoverPad; }

	/** 자동 테스트·연출용 카메라 지정 */
	void SetView(const FVector& InFocus, float InYaw, float InPitch, float InDistance);

	void RestartArena();

private:
	void ApplyCamera();
	void HandleButton(int32 Button);

	EYJJTowerType Selected = EYJJTowerType::Punch;
	int32 HoverPad = INDEX_NONE;

	/** 한 프레임 실시간 최대 (멈췄다 풀릴 때 카메라가 튀지 않게) */
	static constexpr float MaxRealDelta = 0.1f;

	/** 시작 위치는 BeginPlay 에서 Tuning.csv Camera.Start* 로 */
	FVector Focus = FVector::ZeroVector;
	float Yaw = 0.f;
	float Pitch = 0.f;
	float Distance = 0.f;
	float TargetDistance = 0.f;
};
