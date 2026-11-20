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
struct FYJJLowPoly;
struct FYJJLowPolyRow;
struct FYJJPartRow;

/**
 * 부품 도형 (모두 100cm 상자에 꽉 차고, 피벗은 가운데).
 * Cube·Plane·Disc 는 엔진 기본 도형, 나머지는 면이 각진 로우폴리를 게임이 직접 만든다 (TABS 처럼 각진 맛).
 */
enum class EYJJShape : uint8
{
	Cube,
	Sphere,		// 각진 공 (머리, 손, 배)
	Cylinder,	// 팔각 기둥 (팔다리, 기둥)
	Cone,		// 팔각 원뿔 (고깔, 지붕)
	Plane,
	Disc,		// 매끈한 엔진 원기둥 (사거리 원, 충돌 원판)
	Dome,		// 반구: 아래가 평평 (투구, 지붕, 배)
	Prism,		// 삼각 기둥: XZ 단면이 이등변 삼각형, Y 로 뻗음 (지붕, 코, 칼날)
	Wedge,		// 직각 삼각 기둥: 앞(+X)이 낮은 비탈 (망토, 쐐기)
	Taper,		// 팔각 원뿔대: 위가 좁다 (몸통, 치마, 양동이)
	Trapezoid,	// 사다리꼴 상자: 아래가 좁다 (어깨 넓은 몸통, 방패)
	Pyramid,	// 사각뿔 (징, 지붕)
	Tube,		// 속이 빈 팔각 고리 (왕관 테, 바퀴 테, 허리띠)
	Count,
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
	/** 색 칸에 Body / Dark / Skin 이라고 쓴 부품의 색 (옷 색, 옷 그림자 색, 피부색) */
	FLinearColor Body = FLinearColor::White;
	FLinearColor Dark = FLinearColor::White;
	FLinearColor Skin = FLinearColor::White;
};

/** Parts.csv 로 지은 모델: Tag 이름으로 부품을 찾는다 */
struct FYJJModelParts
{
	TMap<FName, TArray<USceneComponent*>> Tagged;
	/** Skin = 1 인 부품 (죽으면 색이 바랜다)와 그 본래 색 */
	TArray<UStaticMeshComponent*> Skin;
	TArray<FLinearColor> SkinColors;

	USceneComponent* First(const TCHAR* Tag) const;
	UStaticMeshComponent* FirstMesh(const TCHAR* Tag) const;
	TArray<UStaticMeshComponent*> Meshes(const TCHAR* Tag) const;
};

/** 기본 도형 + 단색 머티리얼로 캐릭터·타워를 조립하는 도우미 (정적 멤버만 있다) */
class YJJShapes
{
public:
	static UStaticMesh* Mesh(EYJJShape Shape);
	/** EYJJShape 이름 (Cube, Sphere, Cylinder, Cone, Plane, Disc, Dome, Prism, Wedge, Taper, Trapezoid, Pyramid, Tube) */
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
	static constexpr const TCHAR* SkinColorName = TEXT("Skin");
	static constexpr const TCHAR* PivotType = TEXT("Pivot");
	static constexpr const TCHAR* RootParent = TEXT("Root");
	static constexpr const TCHAR* RollAxis = TEXT("Roll");
	static constexpr const TCHAR* PitchAxis = TEXT("Pitch");
	static constexpr const TCHAR* ClubScaleName = TEXT("Club");

	// PCG 계열 정수 해시 상수
	static constexpr uint32 HashMultiplier = 747796405u;
	static constexpr uint32 HashIncrement = 2891336453u;
	static constexpr uint32 HashShiftBase = 4u;
	static constexpr uint32 HashShiftSelect = 28u;
	static constexpr uint32 HashMixMultiplier = 277803737u;
	static constexpr uint32 HashFinalShift = 22u;
	static constexpr uint32 HashMask = 0xFFFFFFu;

	// 각진 도형 (100cm 상자 기준)
	static constexpr float HalfSize = MeshSize * 0.5f;
	static constexpr int32 BallSegments = 10;
	static constexpr int32 BallRings = 6;
	static constexpr int32 DomeRings = 3;
	static constexpr int32 RoundSides = 8;
	/** 팔각형 면 하나가 정면을 보게 (반 칸 돌림) */
	static constexpr float RoundYaw = 22.5f;
	static constexpr float TaperTop = 0.62f;
	static constexpr float TrapezoidBottomX = 0.72f;
	static constexpr float TrapezoidBottomY = 0.8f;
	static constexpr int32 PyramidSides = 4;
	static constexpr float PyramidYaw = 45.f;
	static constexpr int32 TubeSides = 12;
	static constexpr float TubeInner = 0.74f;
	static constexpr const TCHAR* FacetedSlot = TEXT("Flat");
	/** EYJJShape 순서대로 */
	static constexpr const TCHAR* ShapeNames[static_cast<int32>(EYJJShape::Count)] = {
		TEXT("Cube"), TEXT("Sphere"), TEXT("Cylinder"), TEXT("Cone"), TEXT("Plane"), TEXT("Disc"), TEXT("Dome"),
		TEXT("Prism"), TEXT("Wedge"), TEXT("Taper"), TEXT("Trapezoid"), TEXT("Pyramid"), TEXT("Tube") };

	static UMaterialInstanceDynamic* MakeMID(UObject* Outer, const TCHAR* Path);
	/** 각진 도형을 만들어 둔다 (한 번만, GC 루트) */
	static UStaticMesh* Faceted(EYJJShape Shape);
	static void BuildFacetedGeometry(EYJJShape Shape, FYJJLowPoly& Poly);
	/** 닫힌 다각 원뿔대 (Z -50~50) */
	static void AddRound(FYJJLowPoly& Poly, float BottomRadius, float TopRadius, int32 Sides, float YawDeg);
	/** 각진 공. bHalf 면 아래가 평평한 반구 (Z -50~50 에 꽉 차게) */
	static void AddBall(FYJJLowPoly& Poly, bool bHalf);
	static FVector BallPoint(int32 Ring, int32 Segment, int32 Rings, bool bHalf);
	/** XZ 단면 다각형을 Y 로 -50~50 밀어낸 기둥 */
	static void AddExtrusion(FYJJLowPoly& Poly, const TArray<FVector2D>& Profile);
	static void AddTrapezoid(FYJJLowPoly& Poly);
	static void AddTube(FYJJLowPoly& Poly);
	/** 삼각형들을 런타임 스태틱 메시로 굽는다 (충돌은 100cm 상자) */
	static UStaticMesh* Bake(const FYJJLowPoly& Poly, const TCHAR* Name);

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
	static TWeakObjectPtr<UStaticMesh> FacetedCache[static_cast<int32>(EYJJShape::Count)];
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
