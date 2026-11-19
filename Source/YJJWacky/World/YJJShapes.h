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
struct FYJJLowPolyRow;
struct FYJJPartRow;

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
	/** 발광 세기를 캐시 키에 넣을 때 쓰는 정밀도 */
	static constexpr float EmissiveKeyScale = 100.f;
	static constexpr int32 EmissiveKeyBits = 16;

	UPROPERTY()
	TMap<uint64, TObjectPtr<UMaterialInstanceDynamic>> Cache;
};

/** Parts.csv 로 모델을 지을 때 넘기는 것 */
struct FYJJModelContext
{
	/** 부품을 달 수 있는 곳 (Root, Torso, Head ...). 모델 안에서 만든 Pivot도 Tag 이름으로 추가된다 */
	TMap<FName, USceneComponent*> Parents;
	/** 위치·크기에 곱하는 배율 (흐느적이 몸 크기) */
	float Scale = 1.f;
	/** 강화 단계 (MinLevel~MaxLevel 걸러내기) */
	int32 Level = 1;
	/** ScaleBy = Club 인 Pivot의 배율 */
	float ClubScale = 1.f;
	/** 색 칸에 Body / Dark 라고 쓴 부품의 색 */
	FLinearColor Body = FLinearColor::White;
	FLinearColor Dark = FLinearColor::White;
};

/** Parts.csv 로 지은 모델: Tag 이름으로 부품을 찾는다 */
struct FYJJModelParts
{
	TMap<FName, TArray<USceneComponent*>> Tagged;
	/** Skin = 1 인 부품 (죽으면 색이 바랜다) */
	TArray<UStaticMeshComponent*> Skin;

	USceneComponent* First(const TCHAR* Tag) const;
	UStaticMeshComponent* FirstMesh(const TCHAR* Tag) const;
	TArray<UStaticMeshComponent*> Meshes(const TCHAR* Tag) const;
};

/** 기본 도형 + 단색 머티리얼로 캐릭터·타워를 조립하는 도우미 (정적 멤버만 있다) */
class YJJShapes
{
public:
	static UStaticMesh* Mesh(EYJJShape Shape);
	/** Cube, Sphere, Cylinder, Cone, Plane */
	static EYJJShape ParseShape(const FString& Name);

	/** 단색 표면. 같은 색은 같은 인스턴스를 돌려준다 */
	static UMaterialInstanceDynamic* Solid(const UObject* WorldContext, const FLinearColor& Color, float Emissive = 0.f);

	/** 반투명 단색. 색·투명도를 바꿔 가며 쓰도록 매번 새로 만든다 */
	static UMaterialInstanceDynamic* Unlit(UObject* Outer, const FLinearColor& Color, float Opacity);

	static UMaterialInstanceDynamic* Water(UObject* Outer);

	/** 도형 컴포넌트를 붙인다. Size는 cm 단위 실제 크기, Location/Rotation은 Parent 기준 */
	static UStaticMeshComponent* Add(AActor* Owner, USceneComponent* Parent, EYJJShape Shape, const FVector& Location, const FVector& Size,
		UMaterialInterface* Material, bool bCollide = false, const FRotator& Rotation = FRotator::ZeroRotator);

	/** 보이지 않는 회전축 */
	static USceneComponent* AddPivot(AActor* Owner, USceneComponent* Parent, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);

	/** Parts.csv 의 Model 줄들로 모델을 짓는다 */
	static FYJJModelParts BuildModel(AActor* Owner, const FString& Model, FYJJModelContext& Context);

	/** 0~1 사이 결정적 난수 (같은 Seed면 항상 같은 값) */
	static float Hash(int32 Seed);

	static FLinearColor Shade(const FLinearColor& Color, float Scale);

	/** 도형 메시 한 변 길이(cm). Size / 이 값 = 배율 */
	static constexpr float MeshSize = 100.f;

private:
	static constexpr const TCHAR* FallbackMaterial = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	static constexpr const TCHAR* BodyColorName = TEXT("Body");
	static constexpr const TCHAR* DarkColorName = TEXT("Dark");
	static constexpr const TCHAR* PivotType = TEXT("Pivot");
	static constexpr const TCHAR* RootParent = TEXT("Root");
	static constexpr const TCHAR* RollAxis = TEXT("Roll");
	static constexpr const TCHAR* ClubScaleName = TEXT("Club");

	// PCG 계열 정수 해시 상수
	static constexpr uint32 HashMultiplier = 747796405u;
	static constexpr uint32 HashIncrement = 2891336453u;
	static constexpr uint32 HashShiftBase = 4u;
	static constexpr uint32 HashShiftSelect = 28u;
	static constexpr uint32 HashMixMultiplier = 277803737u;
	static constexpr uint32 HashFinalShift = 22u;
	static constexpr uint32 HashMask = 0xFFFFFFu;

	static UMaterialInstanceDynamic* MakeMID(UObject* Outer, const TCHAR* Path);
	static FLinearColor PartColor(const FString& Name, const FYJJModelContext& Context);
	static void BuildPart(AActor* Owner, const FYJJPartRow& Row, FYJJModelContext& Context, FYJJModelParts& Out);

	template <typename T>
	static T* LoadCached(const TCHAR* Path)
	{
		// 엔진/게임 에셋은 GC 루트에 올려 두고 계속 쓴다
		if (TWeakObjectPtr<UObject>* Found = AssetCache.Find(Path))
		{
			if (Found->IsValid())
			{
				return Cast<T>(Found->Get());
			}
		}
		T* Loaded = LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Loaded)
		{
			Loaded->AddToRoot();
			AssetCache.Add(Path, Loaded);
		}
		return Loaded;
	}

	static TMap<FString, TWeakObjectPtr<UObject>> AssetCache;
};

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

	/** LowPoly.csv 의 Model 줄들을 Origin 기준으로 붙인다 */
	void AddModel(const FString& Model, const FVector& Origin);

	UProceduralMeshComponent* Build(AActor* Owner, USceneComponent* Parent, const UObject* WorldContext, bool bCastShadow = true);

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FLinearColor> Colors;

private:
	void AddRow(const FYJJLowPolyRow& Row, const FVector& Origin, float Side);

	/** 면마다 밝기를 이 범위에서 흔든다 (각진 면이 살아 보이게) */
	static constexpr float FaceShadeMin = 0.95f;
	static constexpr float FaceShadeRange = 0.1f;
	static constexpr int32 FaceSeedMultiplier = 7;
	static constexpr int32 FaceSeedOffset = 13;
	static constexpr int32 JitterSeedMultiplier = 31;
	/** 옆면 바깥 방향을 살짝 위로 (밑면과 구분) */
	static constexpr float OutwardLift = 0.01f;
	/** 이보다 작은 윗면 반지름은 뾰족한 원뿔로 */
	static constexpr float PointedRadius = 1.f;

	// 바위: 정이십면체를 흔들고 납작하게
	static constexpr float GoldenRatio = 1.618034f;
	static constexpr int32 IcoVertexCount = 12;
	static constexpr int32 IcoFaceCount = 20;
	static constexpr int32 IcoFaces[IcoFaceCount][3] = {
		{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
		{3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1} };
	static constexpr float RockJitterMin = 0.75f;
	static constexpr float RockJitterRange = 0.45f;
	static constexpr float RockFlatten = 0.75f;
	static constexpr int32 RockSeedMultiplier = 17;

	// 나무: 줄기 + 원뿔 세 층 (높이 대비 비율)
	static constexpr float TreeCrownRadius = 0.3f;
	static constexpr float TrunkBottom = 0.07f;
	static constexpr float TrunkTop = 0.05f;
	static constexpr float TrunkHeight = 0.35f;
	static constexpr int32 TrunkSides = 5;
	static constexpr int32 CrownLayers = 3;
	static constexpr float CrownStart = 0.25f;
	static constexpr float CrownStep = 0.22f;
	static constexpr float CrownShrink = 0.25f;
	static constexpr float CrownHeight = 0.4f;
	static constexpr int32 CrownSides = 6;
	static constexpr float CrownBrighten = 0.08f;
	static constexpr float CrownJitter = 0.12f;
	static constexpr int32 CrownSeedStep = 5;
	static constexpr float CrownYawRange = 60.f;
};
