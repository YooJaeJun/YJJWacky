#pragma once

#include "CoreMinimal.h"
#include "Core/YJJGame.h"
#include "GameFramework/Actor.h"
#include "YJJArena.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UProceduralMeshComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMeshComponent;

/**
 * 전장: 물 위 둑길, 타워 발판, 지켜야 할 푸딩, 하늘·조명.
 * 보이는 땅은 각진 로우폴리 메시 하나이고, 충돌은 안 보이는 상자·원기둥이 맡는다.
 */
UCLASS()
class YJJWACKY_API AYJJArena : public AActor
{
	GENERATED_BODY()

public:
	AYJJArena();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** 푸딩이 뜯기면 출렁 */
	void WobblePudding(float Strength);

	/** 발판 강조 + 사거리 (대포·병사는 원, 펀치·선풍기는 길을 가로지르는 띠). Pad < 0 이면 숨긴다 */
	void SetHighlight(int32 Pad, EYJJTowerType Type, bool bValid);

private:
	void BuildGround();
	void BuildPudding();
	void BuildScenery();
	void AddCollisionBox(const FVector& Center, const FVector& Size, float YawDeg);
	void AddCollisionDisc(const FVector& TopCenter, float Radius);

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY()
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY()
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY()
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY()
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> Pudding;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PadRing;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> RangeRing;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> RangeStrip;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PadRingMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> RangeRingMaterial;

	// 푸딩 출렁임 (감쇠 스프링)
	float Wobble = 0.f;
	float WobbleVelocity = 0.f;
	float Time = 0.f;
};
