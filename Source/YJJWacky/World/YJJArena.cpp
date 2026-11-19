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

AYJJArena::AYJJArena()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(RootComponent);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetAtmosphereSunLight(true);

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(RootComponent);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(RootComponent);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(RootComponent);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(RootComponent);
	PostProcess->bUnbound = true;
}

void AYJJArena::ApplyEnvironment()
{
	// 조명·안개·색감은 Tuning.csv Env.* / Colors.csv
	Sun->SetIntensity(Tune(TEXT("Env.SunIntensity")));
	Sun->SetLightColor(Color(TEXT("Sun")));
	Sun->SetRelativeRotation(FRotator(Tune(TEXT("Env.SunPitch")), Tune(TEXT("Env.SunYaw")), 0.f));
	SkyLight->SetIntensity(Tune(TEXT("Env.SkyLight")));
	Fog->SetFogDensity(Tune(TEXT("Env.FogDensity")));
	Fog->SetFogHeightFalloff(Tune(TEXT("Env.FogFalloff")));
	Fog->SetStartDistance(Tune(TEXT("Env.FogStart")));
	Fog->SetFogInscatteringColor(Color(TEXT("Fog")));

	FPostProcessSettings& PP = PostProcess->Settings;
	// 노출 고정: 밝기가 출렁이지 않게
	PP.bOverride_AutoExposureMethod = true;
	PP.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = Tune(TEXT("Env.Exposure"));
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PP.AutoExposureApplyPhysicalCameraExposure = false;
	// 장난감 같은 색감: 채도만 살짝
	const float Saturation = Tune(TEXT("Env.Saturation"));
	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(Saturation, Saturation, Saturation, 1.f);
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = Tune(TEXT("Env.Bloom"));
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = Tune(TEXT("Env.Vignette"));
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
	UStaticMeshComponent* Disc = YJJShapes::Add(this, RootComponent, EYJJShape::Cylinder, TopCenter - FVector(0, 0, ColliderThickness * 0.5f),
		FVector(Radius * 2.f, Radius * 2.f, ColliderThickness), nullptr, true);
	Disc->SetHiddenInGame(true);
	Disc->SetCastShadow(false);
}

void AYJJArena::BeginPlay()
{
	Super::BeginPlay();
	ApplyEnvironment();
	BuildGround();
	BuildPudding();
	BuildScenery();

	// 발판 강조 고리와 사거리 표시
	PadRingMaterial = YJJShapes::Unlit(this, Color(TEXT("HighlightValid")), Tune(TEXT("Highlight.PadOpacity")));
	const float Ring = YJJMap::PadRadius() * 2.f + Tune(TEXT("Highlight.PadRingExtra"));
	PadRing = YJJShapes::Add(this, RootComponent, EYJJShape::Cylinder, FVector::ZeroVector, FVector(Ring, Ring, Tune(TEXT("Highlight.PadRingHeight"))), PadRingMaterial);
	PadRing->SetVisibility(false);
	RangeRingMaterial = YJJShapes::Unlit(this, Color(TEXT("HighlightValid")), Tune(TEXT("Highlight.RangeOpacity")));
	RangeRing = YJJShapes::Add(this, RootComponent, EYJJShape::Cylinder, FVector::ZeroVector, FVector(YJJShapes::MeshSize), RangeRingMaterial);
	RangeRing->SetVisibility(false);
	RangeStrip = YJJShapes::Add(this, RootComponent, EYJJShape::Cube, FVector::ZeroVector, FVector(YJJShapes::MeshSize), RangeRingMaterial);
	RangeStrip->SetVisibility(false);
}

void AYJJArena::BuildGround()
{
	FYJJLowPoly Mesh;
	const FLinearColor SandColor = Color(TEXT("Sand"));
	const FLinearColor PavingColor = Color(TEXT("Paving"));
	const FLinearColor CliffColor = Color(TEXT("Cliff"));
	const FLinearColor GrassColor = Color(TEXT("Grass"));
	const FLinearColor MarkerColor = Color(TEXT("Marker"));
	const TArray<FVector>& Path = YJJMap::Path();
	if (Path.Num() < 2)
	{
		return;
	}
	const float Depth = Tune(TEXT("Ground.Depth"));		// 윗면에서 물밑까지
	const float Flare = Tune(TEXT("Ground.Flare"));		// 아래로 갈수록 벌어지는 폭
	const float Jitter = Tune(TEXT("Ground.Jitter"));
	const float Overlap = Tune(TEXT("Ground.Overlap"));	// 길 위로 조금 솟게 (경계가 깜빡이지 않게)
	const FVector Down(0, 0, Depth);
	const float PathWidth = YJJMap::PathWidth();

	// 둑길
	for (int32 i = 0; i + 1 < Path.Num(); ++i)
	{
		const FVector A = Path[i];
		const FVector B = Path[i + 1];
		Mesh.Ridge(A, B, PathWidth, Depth, Flare, SandColor, CliffColor);
		const FVector Dir = (B - A).GetSafeNormal();
		AddCollisionBox((A + B) * 0.5f - FVector(0, 0, ColliderThickness * 0.5f), FVector(FVector::Dist(A, B), PathWidth, ColliderThickness), Dir.Rotation().Yaw);
	}
	// 모퉁이 광장 (길보다 살짝 넓다)
	const float Plaza = PathWidth * Tune(TEXT("Ground.PlazaRatio"));
	const int32 PlazaSides = YJJData::TuneInt(TEXT("Ground.PlazaSides"));
	for (int32 i = 1; i + 1 < Path.Num(); ++i)
	{
		Mesh.Frustum(Path[i] - Down, Plaza + Flare, Plaza, Depth + Overlap, PlazaSides, PavingColor, CliffColor, Jitter, i, Tune(TEXT("Ground.PlazaYaw")));
		AddCollisionDisc(Path[i], Plaza);
	}
	// 적이 나오는 곳, 푸딩이 놓인 곳
	const int32 SpawnSeed = YJJData::TuneInt(TEXT("Ground.SpawnSeed"));
	Mesh.Frustum(Path[0] - Down, YJJMap::SpawnRadius() + Flare, YJJMap::SpawnRadius(), Depth + Overlap, YJJData::TuneInt(TEXT("Ground.SpawnSides")),
		Color(TEXT("SpawnGround")), CliffColor, Jitter, SpawnSeed);
	AddCollisionDisc(Path[0], YJJMap::SpawnRadius());
	const FVector Base = YJJMap::BaseLocation();
	Mesh.Frustum(Base - Down, YJJMap::BaseRadius() + Flare, YJJMap::BaseRadius(), Depth + Overlap, YJJData::TuneInt(TEXT("Ground.BaseSides")),
		GrassColor, CliffColor, Jitter, SpawnSeed + 1);
	AddCollisionDisc(Base, YJJMap::BaseRadius());

	// 타워 발판: 풀밭 + 가운데 자리 표시
	const int32 PadSides = YJJData::TuneInt(TEXT("Ground.PadSides"));
	const float PadYaw = Tune(TEXT("Ground.PadYaw"));
	const float MarkerRadius = Tune(TEXT("Ground.MarkerRadius"));
	const float MarkerHeight = Tune(TEXT("Ground.MarkerHeight"));
	int32 Seed = YJJData::TuneInt(TEXT("Ground.PadSeed"));
	for (const YJJMap::FPad& Pad : YJJMap::Pads())
	{
		Mesh.Frustum(Pad.Location - Down, YJJMap::PadRadius() + Tune(TEXT("Ground.PadFlare")), YJJMap::PadRadius(), Depth + Overlap * 2.f, PadSides,
			GrassColor, CliffColor, Jitter, Seed++, PadYaw);
		Mesh.Frustum(Pad.Location + FVector(0, 0, Overlap * 2.f), MarkerRadius, MarkerRadius * Tune(TEXT("Ground.MarkerTaper")), MarkerHeight, PadSides,
			MarkerColor, MarkerColor, 0.f, 0, PadYaw);
		AddCollisionDisc(Pad.Location, YJJMap::PadRadius());
	}

	// 적 진영 문 (LowPoly.csv Gate, 출발점 기준)
	Mesh.AddModel(TEXT("Gate"), Path[0]);
	Mesh.Build(this, RootComponent, this);

	// 물
	const float WaterSize = Tune(TEXT("Ground.WaterSize"));
	UStaticMeshComponent* Water = YJJShapes::Add(this, RootComponent, EYJJShape::Plane, FVector(0, 0, YJJMap::WaterZ()), FVector(WaterSize, WaterSize, YJJShapes::MeshSize),
		YJJShapes::Water(this));
	Water->SetCastShadow(false);
}

void AYJJArena::BuildPudding()
{
	// 커스터드 푸딩 (LowPoly.csv Pudding)
	FYJJLowPoly Mesh;
	Mesh.AddModel(TEXT("Pudding"), FVector::ZeroVector);
	Pudding = Mesh.Build(this, RootComponent, this);
	Pudding->SetRelativeLocation(YJJMap::BaseLocation() + FVector(0, Tune(TEXT("Pudding.OffsetY")), Tune(TEXT("Pudding.OffsetZ"))));
}

void AYJJArena::BuildIsle(FYJJLowPoly& Mesh, const FVector& Top, float Radius, int32 Seed)
{
	// 먼 섬: 물밑까지 내려가는 원뿔대 + 나무·바위 몇 개를 흩뿌린다
	const float Height = Top.Z - YJJMap::WaterZ() + Tune(TEXT("Scenery.IsleSink"));
	Mesh.Frustum(Top - FVector(0, 0, Height), Radius + Tune(TEXT("Scenery.IsleFlare")), Radius, Height, YJJData::TuneInt(TEXT("Scenery.IsleSides")),
		Color(Seed % 2 ? TEXT("Grass") : TEXT("GrassDark")), Color(TEXT("Cliff")), Tune(TEXT("Scenery.IsleJitter")), Seed);
	const int32 Trees = YJJData::TuneInt(TEXT("Scenery.TreesMin")) + FMath::FloorToInt(YJJShapes::Hash(Seed) * Tune(TEXT("Scenery.TreesRandom")));
	const int32 RockEvery = FMath::Max(1, YJJData::TuneInt(TEXT("Scenery.RockEvery")));
	for (int32 t = 0; t < Trees; ++t)
	{
		const float A = YJJShapes::Hash(Seed * YJJData::TuneInt(TEXT("Scenery.AngleSeed")) + t) * TWO_PI;
		const float R = YJJShapes::Hash(Seed * YJJData::TuneInt(TEXT("Scenery.RadiusSeed")) + t) * Radius * Tune(TEXT("Scenery.TreeSpread"));
		const FVector At = Top + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0);
		if (t % RockEvery == RockEvery - 1)
		{
			Mesh.Rock(At + FVector(0, 0, Tune(TEXT("Scenery.IsleRockLift"))),
				Tune(TEXT("Scenery.IsleRockRadius")) + Tune(TEXT("Scenery.IsleRockRandom")) * YJJShapes::Hash(Seed + t), Color(TEXT("Stone")), Seed + t);
		}
		else
		{
			Mesh.Tree(At, Tune(TEXT("Scenery.TreeHeight")) + Tune(TEXT("Scenery.TreeRandom")) * YJJShapes::Hash(Seed * YJJData::TuneInt(TEXT("Scenery.HeightSeed")) + t),
				Color(TEXT("Trunk")), Color(t % 2 ? TEXT("Leaf") : TEXT("LeafWarm")), Seed * YJJData::TuneInt(TEXT("Scenery.TreeSeed")) + t);
		}
	}
}

void AYJJArena::BuildRocks(FYJJLowPoly& Mesh)
{
	// 물 위로 솟은 바위. 길·발판과 겹치는 자리는 건너뛴다
	const FVector Base = YJJMap::BaseLocation();
	const float Clear = Tune(TEXT("Scenery.RockClear"));
	const int32 Count = YJJData::TuneInt(TEXT("Scenery.Rocks"));
	const int32 Seed = YJJData::TuneInt(TEXT("Scenery.RockSeed"));
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector At(Tune(TEXT("Scenery.RockAreaX")) + YJJShapes::Hash(Seed + i * 2) * Tune(TEXT("Scenery.RockAreaWidth")),
			Tune(TEXT("Scenery.RockAreaY")) + YJJShapes::Hash(Seed + 1 + i * 2) * Tune(TEXT("Scenery.RockAreaHeight")),
			YJJMap::WaterZ() + Tune(TEXT("Scenery.RockLift")));
		FVector Closest;
		YJJMap::Project(At, &Closest);
		bool bClear = FVector::Dist2D(At, Closest) > YJJMap::PathWidth() * 0.5f + Clear
			&& FVector::Dist2D(At, Base) > YJJMap::BaseRadius() + Clear
			&& FVector::Dist2D(At, YJJMap::Path()[0]) > YJJMap::SpawnRadius() + Clear;
		for (const YJJMap::FPad& Pad : YJJMap::Pads())
		{
			bClear = bClear && FVector::Dist2D(At, Pad.Location) > YJJMap::PadRadius() + Clear;
		}
		if (bClear)
		{
			Mesh.Rock(At, Tune(TEXT("Scenery.RockRadius")) + Tune(TEXT("Scenery.RockRandom")) * YJJShapes::Hash(Seed + Count * 2 + i), Color(TEXT("Stone")),
				Seed + Count * 3 + i);
		}
	}
}

void AYJJArena::BuildScenery()
{
	FYJJLowPoly Mesh;
	// 먼 섬들 (충돌 없음, 풍경)
	int32 Seed = YJJData::TuneInt(TEXT("Scenery.IsleSeed"));
	for (const FYJJSceneryRow* Isle : YJJData::Scenery(TEXT("Isle")))
	{
		BuildIsle(Mesh, FVector(Isle->X, Isle->Y, Isle->Height), Isle->Radius, Seed++);
	}
	// 푸딩 뒤뜰의 나무 (푸딩 자리 기준)
	const FVector Base = YJJMap::BaseLocation();
	for (const FYJJSceneryRow* Tree : YJJData::Scenery(TEXT("YardTree")))
	{
		Mesh.Tree(Base + FVector(Tree->X, Tree->Y, Tune(TEXT("Ground.Overlap"))), Tree->Height + Tune(TEXT("Scenery.YardTreeRandom")) * YJJShapes::Hash(Seed),
			Color(TEXT("Trunk")), Color(Seed % 2 ? TEXT("Leaf") : TEXT("LeafWarm")), Seed);
		Seed++;
	}
	BuildRocks(Mesh);
	Mesh.Build(this, RootComponent, this);
}

void AYJJArena::WobblePudding(float Strength)
{
	WobbleVelocity += Strength * Tune(TEXT("Pudding.WobbleKick"));
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
	const FLinearColor Tint = Color(bValid ? TEXT("HighlightValid") : TEXT("HighlightInvalid"));
	PadRingMaterial->SetVectorParameterValue(TEXT("Color"), Tint);
	RangeRingMaterial->SetVectorParameterValue(TEXT("Color"), Tint);
	PadRing->SetWorldLocation(Info.Location + FVector(0, 0, Tune(TEXT("Highlight.PadLift"))));
	const float Lift = Tune(TEXT("Highlight.RangeLift"));
	if (bStrip)
	{
		const float Width = Tune(Type == EYJJTowerType::Punch ? TEXT("Highlight.PunchWidth") : TEXT("Highlight.FanWidth"));
		RangeStrip->SetWorldLocationAndRotation(Info.Location + Info.Facing * (Range * 0.5f + Tune(TEXT("Highlight.StripStart"))) + FVector(0, 0, Lift), Info.Facing.Rotation());
		RangeStrip->SetWorldScale3D(FVector(Range / YJJShapes::MeshSize, Width / YJJShapes::MeshSize, FlatThickness));
	}
	else
	{
		// 병사는 발판이 아니라 길 위 자기 자리 둘레를 지킨다
		const FVector Center = Type == EYJJTowerType::Guard ? Info.PathPoint - Info.Facing * (YJJMap::PathWidth() * 0.5f - Tune(TEXT("Map.GuardInset"))) : Info.Location;
		RangeRing->SetWorldLocation(Center + FVector(0, 0, Lift));
		RangeRing->SetWorldScale3D(FVector(Range * 2.f / YJJShapes::MeshSize, Range * 2.f / YJJShapes::MeshSize, FlatThickness));
	}
}

void AYJJArena::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Time += DeltaTime;
	// 푸딩은 가만히 있어도 살짝 떨고, 뜯기면 크게 출렁인다
	const float MaxWobble = Tune(TEXT("Pudding.MaxWobble"));
	WobbleVelocity += (-Tune(TEXT("Pudding.Spring")) * Wobble - Tune(TEXT("Pudding.Damping")) * WobbleVelocity) * DeltaTime;
	Wobble = FMath::Clamp(Wobble + WobbleVelocity * DeltaTime, -MaxWobble, MaxWobble);
	if (Pudding)
	{
		const float W = Wobble + Tune(TEXT("Pudding.IdleAmount")) * FMath::Sin(Time * Tune(TEXT("Pudding.IdleSpeed")));
		const float Size = Tune(TEXT("Pudding.Scale"));
		const float Wide = Size * (1.f + W * Tune(TEXT("Pudding.SquashWide")));
		Pudding->SetRelativeScale3D(FVector(Wide, Wide, Size * (1.f - W * Tune(TEXT("Pudding.SquashTall")))));
	}
}
