#include "World/YJJArena.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "World/YJJMap.h"
#include "World/YJJShapes.h"

namespace
{
	// 파스텔 로우폴리 팔레트 (sRGB)
	constexpr uint32 Sand = 0xEAD9A7;
	constexpr uint32 Paving = 0xDCC48A;
	constexpr uint32 Cliff = 0xC4A77D;
	constexpr uint32 Grass = 0x9BCF5A;
	constexpr uint32 GrassDark = 0x7FBF4D;
	constexpr uint32 Marker = 0xF4EBD0;
	constexpr uint32 Stone = 0xB9B4AE;
	constexpr uint32 Trunk = 0x8B6B4A;
	constexpr uint32 Leaf = 0x58A85A;
	constexpr uint32 LeafWarm = 0x8DC455;

	constexpr float GroundDepth = 420.f;	// 윗면에서 물밑까지
	constexpr float Flare = 85.f;			// 아래로 갈수록 벌어지는 폭
}

AYJJArena::AYJJArena()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(RootComponent);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);
	Sun->SetIntensity(8.f);
	Sun->SetLightColor(FLinearColor(1.f, 0.93f, 0.8f));
	Sun->SetRelativeRotation(FRotator(-50.f, 35.f, 0.f));

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(RootComponent);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(RootComponent);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SetIntensity(1.6f);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(RootComponent);
	Fog->SetFogDensity(0.003f);
	Fog->SetFogHeightFalloff(0.05f);
	Fog->SetStartDistance(6000.f);
	Fog->SetFogInscatteringColor(FLinearColor(0.45f, 0.68f, 0.95f));

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(RootComponent);
	PostProcess->bUnbound = true;

	FPostProcessSettings& PP = PostProcess->Settings;
	// 노출 고정: 밝기가 출렁이지 않게
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = -0.5f;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = false;
	// 장난감 같은 색감: 채도만 살짝
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(1.15f, 1.15f, 1.15f, 1.f);
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.4f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.25f;
	PP.bOverride_MotionBlurAmount = true;
	PP.MotionBlurAmount = 0.f;
}

void AYJJArena::AddCollisionBox(const FVector& Center, const FVector& Size, float YawDeg)
{
	UStaticMeshComponent* Box = YJJShapes::Add(this, RootComponent, EYJJShape::Cube, Center, Size, nullptr, true, FRotator(0.f, YawDeg, 0.f));
	Box->SetHiddenInGame(true);
	Box->SetCastShadow(false);
}

void AYJJArena::AddCollisionDisc(const FVector& TopCenter, float Radius)
{
	UStaticMeshComponent* Disc = YJJShapes::Add(this, RootComponent, EYJJShape::Cylinder, TopCenter - FVector(0, 0, 60), FVector(Radius * 2.f, Radius * 2.f, 120.f), nullptr, true);
	Disc->SetHiddenInGame(true);
	Disc->SetCastShadow(false);
}

void AYJJArena::BeginPlay()
{
	Super::BeginPlay();
	BuildGround();
	BuildPudding();
	BuildScenery();

	PadRingMaterial = YJJShapes::Unlit(this, FLinearColor::White, 0.45f);
	PadRing = YJJShapes::Add(this, RootComponent, EYJJShape::Cylinder, FVector::ZeroVector, FVector(YJJMap::PadRadius * 2.f + 30.f, YJJMap::PadRadius * 2.f + 30.f, 6.f), PadRingMaterial);
	PadRing->SetVisibility(false);
	RangeRingMaterial = YJJShapes::Unlit(this, FLinearColor::White, 0.14f);
	RangeRing = YJJShapes::Add(this, RootComponent, EYJJShape::Cylinder, FVector::ZeroVector, FVector(100.f, 100.f, 4.f), RangeRingMaterial);
	RangeRing->SetVisibility(false);
	RangeStrip = YJJShapes::Add(this, RootComponent, EYJJShape::Cube, FVector::ZeroVector, FVector(100.f, 100.f, 4.f), RangeRingMaterial);
	RangeStrip->SetVisibility(false);
}

void AYJJArena::BuildGround()
{
	FYJJLowPoly Mesh;
	const FLinearColor SandColor = YJJShapes::Hex(Sand);
	const FLinearColor PavingColor = YJJShapes::Hex(Paving);
	const FLinearColor CliffColor = YJJShapes::Hex(Cliff);
	const TArray<FVector>& Path = YJJMap::Path();
	const FVector Down(0, 0, GroundDepth);

	// 둑길
	for (int32 i = 0; i + 1 < Path.Num(); ++i)
	{
		const FVector A = Path[i];
		const FVector B = Path[i + 1];
		Mesh.Ridge(A, B, YJJMap::PathWidth, GroundDepth, Flare, SandColor, CliffColor);
		const FVector Dir = (B - A).GetSafeNormal();
		AddCollisionBox((A + B) * 0.5f - FVector(0, 0, 60), FVector(FVector::Dist(A, B), YJJMap::PathWidth, 120.f), Dir.Rotation().Yaw);
	}
	// 모퉁이 광장 (길보다 살짝 넓다)
	const float Plaza = YJJMap::PathWidth * 0.62f;
	for (int32 i = 1; i + 1 < Path.Num(); ++i)
	{
		Mesh.Frustum(Path[i] - Down, Plaza + Flare, Plaza, GroundDepth + 3.f, 10, PavingColor, CliffColor, 0.03f, i, 18.f);
		AddCollisionDisc(Path[i], Plaza);
	}
	// 적이 나오는 곳, 푸딩이 놓인 곳
	Mesh.Frustum(Path[0] - Down, YJJMap::SpawnRadius + Flare, YJJMap::SpawnRadius, GroundDepth + 3.f, 12, YJJShapes::Hex(0xD9A98C), CliffColor, 0.03f, 50);
	AddCollisionDisc(Path[0], YJJMap::SpawnRadius);
	const FVector Base = YJJMap::BaseLocation();
	Mesh.Frustum(Base - Down, YJJMap::BaseRadius + Flare, YJJMap::BaseRadius, GroundDepth + 3.f, 14, YJJShapes::Hex(Grass), CliffColor, 0.03f, 51);
	AddCollisionDisc(Base, YJJMap::BaseRadius);

	// 타워 발판: 풀밭 + 가운데 자리 표시
	int32 Seed = 100;
	for (const YJJMap::FPad& Pad : YJJMap::Pads())
	{
		Mesh.Frustum(Pad.Location - Down, YJJMap::PadRadius + 65.f, YJJMap::PadRadius, GroundDepth + 4.f, 8, YJJShapes::Hex(Grass), CliffColor, 0.03f, Seed++, 22.5f);
		Mesh.Frustum(Pad.Location + FVector(0, 0, 4), 100.f, 94.f, 5.f, 8, YJJShapes::Hex(Marker), YJJShapes::Hex(Marker), 0.f, 0, 22.5f);
		AddCollisionDisc(Pad.Location, YJJMap::PadRadius);
	}

	// 적 진영 문: 돌기둥 둘 + 빨간 현수막
	const FVector Gate = Path[0] + FVector(-250, 0, 0);
	for (const float Side : { -1.f, 1.f })
	{
		Mesh.Frustum(Gate + FVector(0, Side * 250.f, 0), 70.f, 50.f, 400.f, 6, YJJShapes::Hex(Stone), YJJShapes::Hex(Stone), 0.06f, Seed++);
	}
	Mesh.Box(Gate + FVector(0, 0, 370), FVector(40, 580, 110), YJJShapes::Hex(0xE5484D), YJJShapes::Hex(0xE5484D));

	Mesh.Build(this, RootComponent, this);

	// 물
	UStaticMeshComponent* Water = YJJShapes::Add(this, RootComponent, EYJJShape::Plane, FVector(0, 0, YJJMap::WaterZ), FVector(120000.f, 120000.f, 100.f), YJJShapes::Water(this));
	Water->SetCastShadow(false);
}

void AYJJArena::BuildPudding()
{
	// 커스터드 푸딩: 접시 + 노란 몸 + 캐러멜 + 체리
	FYJJLowPoly Mesh;
	const FLinearColor Custard = YJJShapes::Hex(0xFFD766);
	const FLinearColor Caramel = YJJShapes::Hex(0x9A5B21);
	Mesh.Frustum(FVector(0, 0, 0), 345.f, 380.f, 24.f, 16, YJJShapes::Hex(0xFFFFFF), YJJShapes::Hex(0xE5E7EB));
	Mesh.Frustum(FVector(0, 0, 24), 285.f, 215.f, 215.f, 16, Custard, Custard);
	Mesh.Frustum(FVector(0, 0, 239), 222.f, 196.f, 42.f, 16, Caramel, Caramel);
	Mesh.Rock(FVector(0, 0, 310), 44.f, YJJShapes::Hex(0xE11D48), 7);
	Mesh.Box(FVector(8, 0, 365), FVector(7, 7, 60), YJJShapes::Hex(0x4D7C0F), YJJShapes::Hex(0x4D7C0F));
	Pudding = Mesh.Build(this, RootComponent, this);
	Pudding->SetRelativeLocation(YJJMap::BaseLocation() + FVector(0, 110, 4));
}

void AYJJArena::BuildScenery()
{
	FYJJLowPoly Mesh;
	const FLinearColor CliffColor = YJJShapes::Hex(Cliff);

	// 먼 섬들 (충돌 없음, 풍경)
	struct FIsle { float X, Y, Radius, Top; };
	const FIsle Isles[] = {
		{ -2400, 1300, 560, 60 }, { 200, 2000, 520, 120 }, { 2900, -300, 500, 30 }, { 2800, 1600, 470, 150 },
		{ -500, -2200, 540, 90 }, { 1800, -2300, 480, 40 }, { -3500, -2100, 500, 110 }, { -3800, 200, 440, 60 } };
	int32 Seed = 500;
	for (const FIsle& Isle : Isles)
	{
		const FVector Top(Isle.X, Isle.Y, Isle.Top);
		const float Height = Isle.Top - YJJMap::WaterZ + 90.f;
		Mesh.Frustum(Top - FVector(0, 0, Height), Isle.Radius + 140.f, Isle.Radius, Height, 9, YJJShapes::Hex(Seed % 2 ? Grass : GrassDark), CliffColor, 0.1f, Seed);
		const int32 Trees = 3 + FMath::FloorToInt(YJJShapes::Hash(Seed) * 4.f);
		for (int32 t = 0; t < Trees; ++t)
		{
			const float A = YJJShapes::Hash(Seed * 7 + t) * TWO_PI;
			const float R = YJJShapes::Hash(Seed * 11 + t) * Isle.Radius * 0.65f;
			const FVector At = Top + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0);
			if (t % 4 == 3)
			{
				Mesh.Rock(At + FVector(0, 0, 30), 70.f + 50.f * YJJShapes::Hash(Seed + t), YJJShapes::Hex(Stone), Seed + t);
			}
			else
			{
				Mesh.Tree(At, 260.f + 200.f * YJJShapes::Hash(Seed * 3 + t), YJJShapes::Hex(Trunk), YJJShapes::Hex(t % 2 ? Leaf : LeafWarm), Seed * 13 + t);
			}
		}
		Seed++;
	}

	// 푸딩 뒤뜰의 나무 몇 그루
	const FVector Base = YJJMap::BaseLocation();
	const FVector2D Yard[] = { { -400, 200 }, { 390, 230 }, { -260, 400 }, { 220, 420 }, { 0, 460 } };
	for (const FVector2D& P : Yard)
	{
		Mesh.Tree(Base + FVector(P.X, P.Y, 3), 240.f + 130.f * YJJShapes::Hash(Seed), YJJShapes::Hex(Trunk), YJJShapes::Hex(Seed % 2 ? Leaf : LeafWarm), Seed);
		Seed++;
	}

	// 물 위로 솟은 바위. 길·발판과 겹치는 자리는 건너뛴다
	for (int32 i = 0; i < 46; ++i)
	{
		const FVector At(-3700.f + YJJShapes::Hash(900 + i * 2) * 7200.f, -2600.f + YJJShapes::Hash(901 + i * 2) * 5000.f, YJJMap::WaterZ + 10.f);
		FVector Closest;
		YJJMap::Project(At, &Closest);
		bool bClear = FVector::Dist2D(At, Closest) > YJJMap::PathWidth * 0.5f + 260.f
			&& FVector::Dist2D(At, Base) > YJJMap::BaseRadius + 260.f
			&& FVector::Dist2D(At, YJJMap::Path()[0]) > YJJMap::SpawnRadius + 260.f;
		for (const YJJMap::FPad& Pad : YJJMap::Pads())
		{
			bClear = bClear && FVector::Dist2D(At, Pad.Location) > YJJMap::PadRadius + 260.f;
		}
		if (bClear)
		{
			Mesh.Rock(At, 55.f + 100.f * YJJShapes::Hash(950 + i), YJJShapes::Hex(Stone), 960 + i);
		}
	}
	Mesh.Build(this, RootComponent, this);
}

void AYJJArena::WobblePudding(float Strength)
{
	WobbleVelocity += Strength * 6.f;
}

void AYJJArena::SetHighlight(int32 Pad, EYJJTowerType Type, bool bValid)
{
	const TArray<YJJMap::FPad>& Pads = YJJMap::Pads();
	const bool bShow = Pads.IsValidIndex(Pad);
	const bool bStrip = Type == EYJJTowerType::Punch || Type == EYJJTowerType::Fan;
	PadRing->SetVisibility(bShow);
	RangeRing->SetVisibility(bShow && !bStrip);
	RangeStrip->SetVisibility(bShow && bStrip);
	if (!bShow)
	{
		return;
	}
	const YJJMap::FPad& Info = Pads[Pad];
	const float Range = UYJJGame::TowerInfo(Type).Range;
	const FLinearColor Color = bValid ? FLinearColor::White : FLinearColor(1.f, 0.25f, 0.2f);
	PadRingMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	RangeRingMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	PadRing->SetWorldLocation(Info.Location + FVector(0, 0, 9));
	if (bStrip)
	{
		const float Width = Type == EYJJTowerType::Punch ? 230.f : 440.f;
		RangeStrip->SetWorldLocationAndRotation(Info.Location + Info.Facing * (Range * 0.5f + 60.f) + FVector(0, 0, 14), Info.Facing.Rotation());
		RangeStrip->SetWorldScale3D(FVector(Range / 100.f, Width / 100.f, 0.04f));
	}
	else
	{
		// 병사는 발판이 아니라 길 위 자기 자리 둘레를 지킨다
		const FVector Center = Type == EYJJTowerType::Guard ? Info.PathPoint - Info.Facing * (YJJMap::PathWidth * 0.5f - 110.f) : Info.Location;
		RangeRing->SetWorldLocation(Center + FVector(0, 0, 14));
		RangeRing->SetWorldScale3D(FVector(Range * 2.f / 100.f, Range * 2.f / 100.f, 0.04f));
	}
}

void AYJJArena::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Time += DeltaTime;
	// 푸딩은 가만히 있어도 살짝 떨고, 뜯기면 크게 출렁인다
	WobbleVelocity += (-240.f * Wobble - 5.f * WobbleVelocity) * DeltaTime;
	Wobble = FMath::Clamp(Wobble + WobbleVelocity * DeltaTime, -1.2f, 1.2f);
	if (Pudding)
	{
		const float W = Wobble + 0.04f * FMath::Sin(Time * 2.3f);
		Pudding->SetRelativeScale3D(FVector(1.05f * (1.f + W * 0.14f), 1.05f * (1.f + W * 0.14f), 1.05f * (1.f - W * 0.2f)));
	}
}
