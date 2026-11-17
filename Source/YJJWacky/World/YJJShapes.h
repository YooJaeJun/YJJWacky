#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "YJJShapes.generated.h"

class AActor;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** 엔진 기본 도형 (모두 100cm 크기, 피벗은 가운데) */
enum class EYJJShape : uint8
{
	Cube,
	Sphere,
	Cylinder,
	Cone,
	Plane,
};

/** 색마다 머티리얼 인스턴스를 하나만 만들어 돌려 쓴다 (흐느적이 수십 마리가 같은 색을 쓴다) */
UCLASS()
class YJJWACKY_API UYJJPalette : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UMaterialInstanceDynamic* Solid(const FLinearColor& Color, float Emissive);

private:
	UPROPERTY()
	TMap<uint64, TObjectPtr<UMaterialInstanceDynamic>> Cache;
};

/** 기본 도형 + 단색 머티리얼로 캐릭터·타워를 조립하는 도우미 */
namespace YJJShapes
{
	UStaticMesh* Mesh(EYJJShape Shape);

	/** 0xRRGGBB (sRGB) -> 선형 색 */
	FLinearColor Hex(uint32 RGB);

	/** 단색 표면. 같은 색은 같은 인스턴스를 돌려준다 */
	UMaterialInstanceDynamic* Solid(const UObject* WorldContext, const FLinearColor& Color, float Emissive = 0.f);

	/** 반투명 단색. 색·투명도를 바꿔 가며 쓰도록 매번 새로 만든다 */
	UMaterialInstanceDynamic* Unlit(UObject* Outer, const FLinearColor& Color, float Opacity);

	UMaterialInstanceDynamic* Water(UObject* Outer);

	/** 도형 컴포넌트를 붙인다. Size는 cm 단위 실제 크기, Location/Rotation은 Parent 기준 */
	UStaticMeshComponent* Add(AActor* Owner, USceneComponent* Parent, EYJJShape Shape, const FVector& Location, const FVector& Size,
		UMaterialInterface* Material, bool bCollide = false, const FRotator& Rotation = FRotator::ZeroRotator);

	/** 0~1 사이 결정적 난수 (같은 Seed면 항상 같은 값) */
	float Hash(int32 Seed);

	FLinearColor Shade(const FLinearColor& Color, float Scale);
}

/**
 * 면마다 색이 다른 각진(플랫 셰이딩) 로우폴리 메시를 모아 프로시저럴 메시 하나로 만든다.
 * 섬, 길, 바위, 나무, 푸딩이 모두 이걸로 만들어진다.
 */
struct FYJJLowPoly
{
	/** Outward 쪽에서 보이는 삼각형 */
	void Tri(const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Color, const FVector& Outward);
	void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Color, const FVector& Outward);

	/** 다각 기둥(위아래 반지름이 다른 원뿔대). Base는 바닥 가운데. Jitter는 꼭짓점을 흩뜨리는 비율 */
	void Frustum(const FVector& Base, float BottomRadius, float TopRadius, float Height, int32 Sides,
		const FLinearColor& TopColor, const FLinearColor& SideColor, float Jitter = 0.f, int32 Seed = 0, float YawDeg = 0.f);

	/** 선분 A-B를 따라 놓인 둑(윗면 폭 TopWidth, 아래로 갈수록 Flare만큼 넓어짐). 윗면 높이는 A.Z */
	void Ridge(const FVector& A, const FVector& B, float TopWidth, float Depth, float Flare, const FLinearColor& TopColor, const FLinearColor& SideColor);

	/** 축에 정렬된 상자. Center는 가운데 */
	void Box(const FVector& Center, const FVector& Size, const FLinearColor& TopColor, const FLinearColor& SideColor, float YawDeg = 0.f);

	/** 울퉁불퉁한 바위 */
	void Rock(const FVector& Center, float Radius, const FLinearColor& Color, int32 Seed);

	/** 원뿔을 쌓은 나무 */
	void Tree(const FVector& Base, float Height, const FLinearColor& Trunk, const FLinearColor& Leaf, int32 Seed);

	UProceduralMeshComponent* Build(AActor* Owner, USceneComponent* Parent, const UObject* WorldContext, bool bCastShadow = true);

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FLinearColor> Colors;
};
