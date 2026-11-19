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
struct FYJJLowPoly;

/**
 * 전장: 물 위 둑길, 타워 발판, 지켜야 할 푸딩, 하늘·조명.
 * 보이는 땅은 각진 로우폴리 메시 하나이고, 충돌은 안 보이는 상자·원기둥이 맡는다.
 * 배치는 MapPath/MapPads.csv, 풍경은 Scenery.csv, 푸딩·문은 LowPoly.csv, 색은 Colors.csv, 나머지는 Tuning.csv (Ground.*, Env.*, ...).
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

	/** 발판 강조 + 사거리 (대포·병사·새총은 원, 펀치·선풍기는 길을 가로지르는 띠). Pad < 0 이면 숨긴다 */
	void SetHighlight(int32 Pad, EYJJTowerType Type, bool bValid);

private:
	static float Tune(const TCHAR* Name) { return YJJData::Tune(Name); }
	static FLinearColor Color(const TCHAR* Name) { return YJJData::Color(Name); }

	void ApplyEnvironment();
	void BuildGround();
	void BuildPudding();
	void BuildScenery();
	void BuildIsle(FYJJLowPoly& Mesh, const FVector& Top, float Radius, int32 Seed);
	void BuildRocks(FYJJLowPoly& Mesh);
	void AddCollisionBox(const FVector& Center, const FVector& Size, float YawDeg);
	void AddCollisionDisc(const FVector& TopCenter, float Radius);

	/** 아주 얇은 원판·띠의 두께 배율 */
	static constexpr float FlatThickness = 0.04f;
	/** 충돌 상자·원판 두께(cm). 윗면이 길 높이(0)와 맞게 반만큼 내린다 */
	static constexpr float ColliderThickness = 120.f;

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
