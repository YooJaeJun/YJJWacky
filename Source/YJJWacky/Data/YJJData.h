#pragma once

#include "CoreMinimal.h"
#include "Data/YJJTypes.h"
#include "Engine/DataTable.h"
#include "HAL/IConsoleManager.h"
#include "YJJData.generated.h"

/**
 * 게임 수치·배치·연출 값은 코드가 아니라 Content/YJJWacky/Data/*.csv 에 있다.
 * 게임이 시작할 때 CSV를 엔진 DataTable로 읽고, 콘솔 yjj.ReloadData 로 게임 중에 다시 읽는다.
 *
 *   Enemies.csv     몬스터 (행 이름 = EYJJWobblerKind)
 *   Towers.csv      타워 + 강화 단계별 값 (행 이름 = EYJJTowerType)
 *   Waves.csv       웨이브별 총 무게 예산과 몬스터 몫
 *   Tuning.csv      경제·연출·물리 감각 상수 (이름, 값)
 *   Sounds.csv      소리 큐 (파일, 볼륨, 음높이)
 *   Effects.csv     튀어나오는 조각 이펙트
 *   TextStyles.csv  떠오르는 글자·배너 색과 크기
 *
 * 글자는 Localization.csv (YJJLoc).
 */

/** Enemies.csv 한 줄 */
USTRUCT()
struct FYJJEnemyRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Localization.csv 키 */
	UPROPERTY(EditAnywhere)
	FString NameKey;

	UPROPERTY(EditAnywhere)
	float Scale = 1.f;

	UPROPERTY(EditAnywhere)
	float Health = 30.f;

	UPROPERTY(EditAnywhere)
	float Speed = 200.f;

	UPROPERTY(EditAnywhere)
	int32 Bounty = 5;

	/** 푸딩에 닿았을 때 깎는 체력 */
	UPROPERTY(EditAnywhere)
	int32 Bite = 1;

	/** 레벨 디자인의 중심. 맞고 날아가는 거리는 무게에 반비례 (기준 무게 대비) */
	UPROPERTY(EditAnywhere)
	float WeightKg = 60.f;

	/** sRGB 16진수 (예: E5484D) */
	UPROPERTY(EditAnywhere)
	FString Color = TEXT("E5484D");

	/** 피부색 (Colors.csv 이름 또는 16진수). Parts.csv 색 칸의 Skin */
	UPROPERTY(EditAnywhere)
	FString Skin = TEXT("Skin");

	UPROPERTY(EditAnywhere)
	FString Note;

	/** 기준 무게(Tuning: Game.ReferenceWeight) 대비. 맞을 때 속도 변화를 이걸로 나눈다 */
	float Heft() const;
	FLinearColor LinearColor() const;
};

/**
 * Towers.csv 한 줄. 강화 단계(Lv1~3)마다 Power·Cooldown·ParamA·ParamB 를 따로 둔다.
 * ParamA/ParamB 의 뜻은 타워마다 다르다 (CSV의 Note 열에 적혀 있다).
 */
USTRUCT()
struct FYJJTowerRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString NameKey;

	UPROPERTY(EditAnywhere)
	FString BlurbKey;

	UPROPERTY(EditAnywhere)
	FString Upgrade2Key;

	UPROPERTY(EditAnywhere)
	FString Upgrade3Key;

	UPROPERTY(EditAnywhere)
	int32 Cost = 60;

	UPROPERTY(EditAnywhere)
	float Range = 600.f;

	/** 강화 비용 = Cost x 비율 (Tuning: Game.UpgradeCostStep 단위로 반올림) */
	UPROPERTY(EditAnywhere)
	float Upgrade2CostRatio = 0.9f;

	UPROPERTY(EditAnywhere)
	float Upgrade3CostRatio = 1.5f;

	UPROPERTY(EditAnywhere) float Power1 = 1.f;
	UPROPERTY(EditAnywhere) float Power2 = 1.f;
	UPROPERTY(EditAnywhere) float Power3 = 1.f;
	UPROPERTY(EditAnywhere) float Cooldown1 = 1.f;
	UPROPERTY(EditAnywhere) float Cooldown2 = 1.f;
	UPROPERTY(EditAnywhere) float Cooldown3 = 1.f;
	UPROPERTY(EditAnywhere) float ParamA1 = 0.f;
	UPROPERTY(EditAnywhere) float ParamA2 = 0.f;
	UPROPERTY(EditAnywhere) float ParamA3 = 0.f;
	UPROPERTY(EditAnywhere) float ParamB1 = 0.f;
	UPROPERTY(EditAnywhere) float ParamB2 = 0.f;
	UPROPERTY(EditAnywhere) float ParamB3 = 0.f;

	UPROPERTY(EditAnywhere)
	FString Note;

	float Power(int32 Level) const { return Pick(Level, Power1, Power2, Power3); }
	float Cooldown(int32 Level) const { return Pick(Level, Cooldown1, Cooldown2, Cooldown3); }
	float ParamA(int32 Level) const { return Pick(Level, ParamA1, ParamA2, ParamA3); }
	float ParamB(int32 Level) const { return Pick(Level, ParamB1, ParamB2, ParamB3); }

private:
	static constexpr int32 Level2 = 2;
	static constexpr int32 Level3 = 3;
	static float Pick(int32 Level, float L1, float L2, float L3) { return Level >= Level3 ? L3 : (Level == Level2 ? L2 : L1); }
};

/** Waves.csv 한 줄: 한 웨이브 안에서 몬스터 하나의 몫 */
USTRUCT()
struct FYJJWaveRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 Wave = 1;

	/** 웨이브 총 무게 (같은 웨이브 줄들은 같은 값) */
	UPROPERTY(EditAnywhere)
	int32 BudgetKg = 500;

	/** 이 웨이브가 쏟아지는 시간(초) */
	UPROPERTY(EditAnywhere)
	float Duration = 10.f;

	/** EYJJWobblerKind 이름 (Grunt, Tiny ...) */
	UPROPERTY(EditAnywhere)
	FString Kind;

	/** 총 무게 중 이 몬스터 몫 (0~1). 마릿수 = 총 무게 x 몫 / 몬스터 무게 */
	UPROPERTY(EditAnywhere)
	float Share = 1.f;

	/** 웨이브 시간 중 나오는 구간 (0~1) */
	UPROPERTY(EditAnywhere)
	float From = 0.f;

	UPROPERTY(EditAnywhere)
	float To = 1.f;

	UPROPERTY(EditAnywhere)
	FString Note;
};

/** Tuning.csv 한 줄 */
USTRUCT()
struct FYJJTuningRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float Value = 0.f;

	UPROPERTY(EditAnywhere)
	FString Note;
};

/** Sounds.csv 한 줄: 소리 큐 (행 이름 = 코드가 부르는 이름) */
USTRUCT()
struct FYJJSoundRow : public FTableRowBase
{
	GENERATED_BODY()

	/** /Game/YJJWacky/Audio 안의 에셋 이름 */
	UPROPERTY(EditAnywhere)
	FString Asset;

	UPROPERTY(EditAnywhere)
	float Volume = 1.f;

	/** 재생할 때마다 이 사이에서 음높이를 고른다 */
	UPROPERTY(EditAnywhere)
	float PitchMin = 1.f;

	UPROPERTY(EditAnywhere)
	float PitchMax = 1.f;

	UPROPERTY(EditAnywhere)
	FString Note;
};

/** Effects.csv 한 줄: 작은 공·상자가 튀어나와 날아가며 줄어드는 이펙트 */
USTRUCT()
struct FYJJEffectRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString Color = TEXT("FFFFFF");

	UPROPERTY(EditAnywhere)
	int32 Count = 6;

	/** 크기 배율 1당 더 나오는 개수 (물보라처럼 큰 놈일수록 많이) */
	UPROPERTY(EditAnywhere)
	float CountPerScale = 0.f;

	UPROPERTY(EditAnywhere)
	float Speed = 400.f;

	UPROPERTY(EditAnywhere)
	float Size = 30.f;

	UPROPERTY(EditAnywhere)
	float Life = 0.5f;

	/** 양수면 떨어지고 음수면 떠오른다 (cm/s²) */
	UPROPERTY(EditAnywhere)
	float Gravity = 0.f;

	/** 1이면 상자, 0이면 공 */
	UPROPERTY(EditAnywhere)
	int32 Cube = 0;

	/** 0 = 사방으로, 1 = 거의 위로만 */
	UPROPERTY(EditAnywhere)
	float UpBias = 0.5f;

	/** 부른 위치에서 이만큼 위에서 터진다 (크기 배율을 곱함) */
	UPROPERTY(EditAnywhere)
	float OffsetZ = 0.f;

	UPROPERTY(EditAnywhere)
	FString Note;

	FLinearColor LinearColor() const;
};

/** TextStyles.csv 한 줄: 떠오르는 글자·배너 */
USTRUCT()
struct FYJJTextStyleRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString Color = TEXT("FFFFFF");

	UPROPERTY(EditAnywhere)
	float Size = 28.f;

	/** 크기 배율 1당 커지는 글자 크기 */
	UPROPERTY(EditAnywhere)
	float SizePerScale = 0.f;

	/** 부른 위치에서 이만큼 위에 뜬다 */
	UPROPERTY(EditAnywhere)
	float OffsetZ = 0.f;

	/** 배너가 떠 있는 시간(초). 떠오르는 글자는 Tuning: Hud.FloaterLife */
	UPROPERTY(EditAnywhere)
	float Life = 2.f;

	UPROPERTY(EditAnywhere)
	FString Note;

	FLinearColor LinearColor() const;
};

/** Colors.csv 한 줄: 이름 붙은 색 (다른 표의 색 칸에 이름을 써도 된다) */
USTRUCT()
struct FYJJColorRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString Hex = TEXT("FFFFFF");

	UPROPERTY(EditAnywhere)
	float Alpha = 1.f;

	UPROPERTY(EditAnywhere)
	FString Note;
};

/**
 * Parts.csv 한 줄: 기본 도형 부품 하나 (타워·흐느적이 모델).
 * 같은 Model 줄들이 CSV 순서대로 만들어진다. Parent는 Root, 몸 부위(Torso, Head, Arms, Legs, ClubArm),
 * 또는 앞에서 만든 Pivot의 Tag.
 */
USTRUCT()
struct FYJJPartRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString Model;

	UPROPERTY(EditAnywhere)
	FString Parent = TEXT("Root");

	/** Mesh = 도형, Pivot = 보이지 않는 회전축 (자식 부품을 단다) */
	UPROPERTY(EditAnywhere)
	FString Type = TEXT("Mesh");

	/** Cube, Sphere, Cylinder, Cone, Plane */
	UPROPERTY(EditAnywhere)
	FString Shape = TEXT("Cube");

	UPROPERTY(EditAnywhere) float X = 0.f;
	UPROPERTY(EditAnywhere) float Y = 0.f;
	UPROPERTY(EditAnywhere) float Z = 0.f;
	/** 크기(cm). Pivot이면 배율 */
	UPROPERTY(EditAnywhere) float SX = 100.f;
	UPROPERTY(EditAnywhere) float SY = 100.f;
	UPROPERTY(EditAnywhere) float SZ = 100.f;
	UPROPERTY(EditAnywhere) float Pitch = 0.f;
	UPROPERTY(EditAnywhere) float Yaw = 0.f;
	UPROPERTY(EditAnywhere) float Roll = 0.f;

	/** 16진수, Colors.csv 이름, 또는 Body/Dark (흐느적이 몸 색) */
	UPROPERTY(EditAnywhere)
	FString Color = TEXT("FFFFFF");

	/** 코드가 찾는 이름 (Glove, Turret, Pupil ...) */
	UPROPERTY(EditAnywhere)
	FString Tag;

	/** 1이면 좌우(Y) 대칭으로 하나 더 */
	UPROPERTY(EditAnywhere)
	int32 Mirror = 0;

	/** 1보다 크면 RepeatAxis(Yaw/Roll) 둘레로 RepeatStep 도씩 돌려 가며 여러 개 */
	UPROPERTY(EditAnywhere)
	int32 Repeat = 1;

	UPROPERTY(EditAnywhere)
	FString RepeatAxis = TEXT("Yaw");

	UPROPERTY(EditAnywhere)
	float RepeatStep = 0.f;

	/** 이 강화 단계 사이일 때만 만든다 (흐느적이는 병사 단계) */
	UPROPERTY(EditAnywhere)
	int32 MinLevel = 1;

	UPROPERTY(EditAnywhere)
	int32 MaxLevel = 3;

	UPROPERTY(EditAnywhere)
	int32 Collide = 0;

	UPROPERTY(EditAnywhere)
	int32 Shadow = 1;

	/** 1이면 죽을 때 색이 바래는 피부 */
	UPROPERTY(EditAnywhere)
	int32 Skin = 0;

	/** 0보다 크면 반투명 (Unlit 머티리얼) */
	UPROPERTY(EditAnywhere)
	float Opacity = 0.f;

	UPROPERTY(EditAnywhere)
	int32 Hidden = 0;

	/** 1이면 월드 좌표로 코드가 직접 움직인다 (고무줄, 주머니) */
	UPROPERTY(EditAnywhere)
	int32 Loose = 0;

	/** Club = 병사 몽둥이 크기 배율(Towers.csv Guard ParamA)을 곱한다 (Pivot) */
	UPROPERTY(EditAnywhere)
	FString ScaleBy;

	UPROPERTY(EditAnywhere)
	FString Note;
};

/** LowPoly.csv 한 줄: 각진 로우폴리 덩어리 (푸딩, 적 진영 문) */
USTRUCT()
struct FYJJLowPolyRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString Model;

	/** Frustum(A=아래 반지름, B=위 반지름, C=높이), Rock(A=반지름), Box(A,B,C=크기), Tree(C=높이) */
	UPROPERTY(EditAnywhere)
	FString Kind = TEXT("Frustum");

	UPROPERTY(EditAnywhere) float X = 0.f;
	UPROPERTY(EditAnywhere) float Y = 0.f;
	UPROPERTY(EditAnywhere) float Z = 0.f;
	UPROPERTY(EditAnywhere) float A = 0.f;
	UPROPERTY(EditAnywhere) float B = 0.f;
	UPROPERTY(EditAnywhere) float C = 0.f;

	UPROPERTY(EditAnywhere)
	int32 Sides = 8;

	UPROPERTY(EditAnywhere)
	FString TopColor = TEXT("FFFFFF");

	UPROPERTY(EditAnywhere)
	FString SideColor = TEXT("FFFFFF");

	UPROPERTY(EditAnywhere)
	float Jitter = 0.f;

	UPROPERTY(EditAnywhere)
	int32 Seed = 0;

	UPROPERTY(EditAnywhere)
	float Yaw = 0.f;

	UPROPERTY(EditAnywhere)
	int32 Mirror = 0;

	UPROPERTY(EditAnywhere)
	FString Note;
};

/** Scenery.csv 한 줄: 풍경 배치 (Isle = 먼 섬, YardTree = 푸딩 뒤뜰 나무) */
USTRUCT()
struct FYJJSceneryRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString Kind;

	UPROPERTY(EditAnywhere) float X = 0.f;
	UPROPERTY(EditAnywhere) float Y = 0.f;
	/** 섬 반지름 */
	UPROPERTY(EditAnywhere) float Radius = 0.f;
	/** 섬 윗면 높이 (Prop: 놓는 높이) */
	UPROPERTY(EditAnywhere) float Height = 0.f;

	/** Kind = Prop 일 때 Parts.csv 모델 이름 */
	UPROPERTY(EditAnywhere)
	FString Model;

	UPROPERTY(EditAnywhere) float Yaw = 0.f;
	UPROPERTY(EditAnywhere) float Scale = 1.f;

	/** X, Y 기준점: World(원점), Base(푸딩 자리), Spawn(적 출발점) */
	UPROPERTY(EditAnywhere)
	FString Anchor = TEXT("World");

	UPROPERTY(EditAnywhere)
	FString Note;
};

/** MapPath.csv 한 줄: 길 가운데 선의 꼭짓점 (Order 순서) */
USTRUCT()
struct FYJJPathPointRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 Order = 0;

	UPROPERTY(EditAnywhere) float X = 0.f;
	UPROPERTY(EditAnywhere) float Y = 0.f;

	UPROPERTY(EditAnywhere)
	FString Note;
};

/** MapPads.csv 한 줄: 타워 발판 (길 구간 Segment 의 T 지점, Side +1 = 진행 방향 오른쪽) */
USTRUCT()
struct FYJJPadRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	int32 Segment = 0;

	UPROPERTY(EditAnywhere)
	float T = 0.5f;

	UPROPERTY(EditAnywhere)
	float Side = 1.f;

	UPROPERTY(EditAnywhere)
	FString Note;
};

/**
 * HudIcons.csv 한 줄: HUD 아이콘 조각 (원 또는 사각형). 위치·크기는 아이콘 반지름 R 의 배수.
 * Mirror = 1 이면 좌우(X) 대칭으로 하나 더, Repeat > 1 이면 가운데 둘레 Orbit 거리에 OrbitStep 도씩 돌려 가며.
 */
USTRUCT()
struct FYJJIconRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString Icon;

	/** Circle 또는 Rect */
	UPROPERTY(EditAnywhere)
	FString Shape = TEXT("Circle");

	/** Circle: 가운데, Rect: 왼쪽 위 */
	UPROPERTY(EditAnywhere) float X = 0.f;
	UPROPERTY(EditAnywhere) float Y = 0.f;
	/** Circle: 반지름, Rect: 폭 */
	UPROPERTY(EditAnywhere) float A = 0.f;
	/** Rect: 높이 */
	UPROPERTY(EditAnywhere) float B = 0.f;

	UPROPERTY(EditAnywhere)
	FString Color = TEXT("Ink");

	/** 원 변 수 (0이면 기본) */
	UPROPERTY(EditAnywhere)
	int32 Sides = 0;

	UPROPERTY(EditAnywhere)
	int32 Mirror = 0;

	UPROPERTY(EditAnywhere)
	int32 Repeat = 1;

	UPROPERTY(EditAnywhere)
	float Orbit = 0.f;

	UPROPERTY(EditAnywhere)
	float OrbitStart = 0.f;

	UPROPERTY(EditAnywhere)
	float OrbitStep = 0.f;

	UPROPERTY(EditAnywhere)
	FString Note;
};

/** 웨이브 한 줄을 게임이 쓰기 좋게 푼 것 */
struct FYJJWaveShare
{
	EYJJWobblerKind Kind = EYJJWobblerKind::Grunt;
	float Share = 1.f;
	float From = 0.f;
	float To = 1.f;
};

struct FYJJWaveDef
{
	int32 BudgetKg = 0;
	float Duration = 0.f;
	TArray<FYJJWaveShare> Shares;
};

/** CSV 데이터 테이블 창구 (정적 멤버만 있는 도우미) */
class YJJData
{
public:
	static const FYJJEnemyRow& Enemy(EYJJWobblerKind Kind);
	static const FYJJTowerRow& Tower(EYJJTowerType Type);

	/** Tuning.csv 값 (CSV가 원본이라 코드에 기본값을 두지 않는다). 없는 이름이면 경고를 한 번 찍고 0 */
	static float Tune(const TCHAR* Name);
	/** HudLayout.csv 값 (HUD 배치·글자 크기·연출) */
	static float Layout(const TCHAR* Name);
	/** HudIcons.csv 에서 Icon 조각들 (CSV 순서) */
	static TArray<const FYJJIconRow*> Icon(const FString& Icon);
	static int32 TuneInt(const TCHAR* Name);

	/** 없으면 경고를 한 번 찍고 nullptr */
	static const FYJJSoundRow* Sound(const TCHAR* Cue);
	static const FYJJEffectRow* Effect(const TCHAR* Name);
	static const FYJJTextStyleRow& TextStyle(const TCHAR* Name);

	static int32 WaveCount();
	/** 1부터 */
	static const FYJJWaveDef& Wave(int32 WaveNumber);

	/** Colors.csv 이름 또는 16진수 -> 선형 색 (알파 포함) */
	static FLinearColor Color(const FString& NameOrHex);

	/** Parts.csv / LowPoly.csv 에서 Model 줄들 (CSV 순서) */
	static TArray<const FYJJPartRow*> Parts(const FString& Model);
	/** 경고 없이 모델이 있는지만 (모자 없는 졸병처럼 없어도 되는 모델) */
	static bool HasParts(const FString& Model);
	static TArray<const FYJJLowPolyRow*> LowPoly(const FString& Model);
	/** Scenery.csv 에서 Kind 줄들 */
	static TArray<const FYJJSceneryRow*> Scenery(const FString& Kind);
	/** MapPath.csv 꼭짓점 (Order 순) */
	static TArray<FVector> PathPoints();
	static TArray<FYJJPadRow> PadRows();

	/** 다시 읽을 때마다 1씩 늘어난다 (캐시를 다시 만들 때 쓴다) */
	static int32 Generation();

	/** CSV를 다시 읽는다 (콘솔 yjj.ReloadData) */
	static void Reload();

	/** 0xRRGGBB 문자열 -> 선형 색 */
	static FLinearColor ParseColor(const FString& Hex);

private:
	static constexpr const TCHAR* DataDir = TEXT("YJJWacky/Data");
	static constexpr const TCHAR* Context = TEXT("YJJData");
	/** RRGGBB */
	static constexpr int32 HexLength = 6;

	static void EnsureLoaded();
	static void Load();
	static void BuildWaves();
	static UDataTable* LoadTable(const TCHAR* File, UScriptStruct* RowStruct);
	static void WarnOnce(const FString& What);
	template <typename EnumType>
	static FName EnumName(EnumType Value)
	{
		return FName(*StaticEnum<EnumType>()->GetNameStringByValue(static_cast<int64>(Value)));
	}

	static bool bLoaded;
	static TObjectPtr<UDataTable> Enemies;
	static TObjectPtr<UDataTable> Towers;
	static TObjectPtr<UDataTable> Waves;
	static TObjectPtr<UDataTable> Tuning;
	static TObjectPtr<UDataTable> Sounds;
	static TObjectPtr<UDataTable> Effects;
	static TObjectPtr<UDataTable> TextStyles;
	static TObjectPtr<UDataTable> Colors;
	static TObjectPtr<UDataTable> PartsTable;
	static TObjectPtr<UDataTable> LowPolyTable;
	static TObjectPtr<UDataTable> SceneryTable;
	static TObjectPtr<UDataTable> MapPath;
	static TObjectPtr<UDataTable> MapPads;
	static TObjectPtr<UDataTable> HudLayout;
	static TObjectPtr<UDataTable> HudIcons;
	static int32 LoadCount;
	static TArray<FYJJWaveDef> WaveDefs;
	static TSet<FString> Warned;
	static const FYJJEnemyRow DefaultEnemy;
	static const FYJJTowerRow DefaultTower;
	static const FYJJTextStyleRow DefaultTextStyle;
	static const FYJJWaveDef EmptyWave;
	static FAutoConsoleCommand ReloadCommand;
};
