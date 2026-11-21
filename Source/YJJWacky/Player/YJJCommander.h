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
 *  Z/X 주문 고르기 -> 땅 좌클릭으로 쓰기 (우클릭·Esc 취소)
 *  H 또는 영웅 좌클릭 -> 땅 좌클릭으로 영웅 보내기 (우클릭·Esc 취소)
 *  Space 다음 웨이브 (일찍 부르면 골드) · R 다시 하기 (끝난 뒤)
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
	/** 쓰려고 고른 주문 (땅을 찍기를 기다리는 중). 없으면 INDEX_NONE */
	int32 GetPendingSpell() const { return PendingSpell; }
	/** 커서 아래 땅 (주문 조준) */
	bool GetAim(FVector& OutPoint) const { OutPoint = AimPoint; return bAimValid; }
	/** 영웅을 골라 보낼 곳을 찍기를 기다리는 중 */
	bool IsHeroSelected() const { return bHeroSelected; }
	/** 커서가 영웅 위에 있다 */
	bool IsOverHero() const { return bOverHero; }

	/** 자동 테스트·연출용 카메라 지정 */
	void SetView(const FVector& InFocus, float InYaw, float InPitch, float InDistance);

	void RestartArena();

private:
	void ApplyCamera();
	void HandleButton(int32 Button);
	/** 주문을 고르거나, 이미 골랐으면 취소 */
	void ToggleSpell(int32 Spell);
	/** 영웅을 고르거나, 이미 골랐으면 취소 */
	void ToggleHero();

	EYJJTowerType Selected = EYJJTowerType::Punch;
	int32 HoverPad = INDEX_NONE;
	int32 PendingSpell = INDEX_NONE;
	FVector AimPoint = FVector::ZeroVector;
	bool bAimValid = false;
	bool bHeroSelected = false;
	bool bOverHero = false;

	/** 한 프레임 실시간 최대 (멈췄다 풀릴 때 카메라가 튀지 않게) */
	static constexpr float MaxRealDelta = 0.1f;

	/** 시작 위치는 BeginPlay 에서 Tuning.csv Camera.Start* 로 */
	FVector Focus = FVector::ZeroVector;
	float Yaw = 0.f;
	float Pitch = 0.f;
	float Distance = 0.f;
	float TargetDistance = 0.f;
};
