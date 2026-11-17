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

	FVector Focus = FVector(-360.f, 60.f, 0.f);
	float Yaw = -90.f;
	float Pitch = -54.f;
	float Distance = 5000.f;
	float TargetDistance = 5000.f;
};
