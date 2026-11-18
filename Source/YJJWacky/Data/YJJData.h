#pragma once

#include "CoreMinimal.h"
#include "Data/YJJTypes.h"
#include "Engine/DataTable.h"
#include "YJJData.generated.h"

/**
 * 게임 수치는 코드가 아니라 Content/YJJWacky/Data/*.csv 에 있다.
 * 게임이 시작할 때 CSV를 엔진 DataTable로 읽고, 콘솔 yjj.ReloadData 로 게임 중에 다시 읽는다.
 *
 *   Enemies.csv  몬스터 (행 이름 = EYJJWobblerKind)
 *   Towers.csv   타워 + 강화 단계별 값 (행 이름 = EYJJTowerType)
 *   Waves.csv    웨이브별 총 무게 예산과 몬스터 몫
 *   Tuning.csv   경제·연출·물리 감각 상수 (이름, 값)
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

	/** 레벨 디자인의 중심. 맞고 날아가는 거리는 무게에 반비례 (졸병 60kg 기준) */
	UPROPERTY(EditAnywhere)
	float WeightKg = 60.f;

	/** sRGB 16진수 (예: E5484D) */
	UPROPERTY(EditAnywhere)
	FString Color = TEXT("E5484D");

	UPROPERTY(EditAnywhere)
	FString Note;

	/** 졸병(60kg) 대비 무게. 맞을 때 속도 변화를 이걸로 나눈다 */
	float Heft() const { return WeightKg / 60.f; }
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

	/** 강화 비용 = Cost x 비율 (5 단위로 반올림) */
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
	static float Pick(int32 Level, float L1, float L2, float L3) { return Level >= 3 ? L3 : (Level == 2 ? L2 : L1); }
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
	float Duration = 10.f;
	TArray<FYJJWaveShare> Shares;
};

namespace YJJData
{
	const FYJJEnemyRow& Enemy(EYJJWobblerKind Kind);
	const FYJJTowerRow& Tower(EYJJTowerType Type);

	/** Tuning.csv 값. 없는 이름이면 경고를 한 번 찍고 Fallback */
	float Tune(const TCHAR* Name, float Fallback = 0.f);

	int32 WaveCount();
	/** 1부터 */
	const FYJJWaveDef& Wave(int32 WaveNumber);

	/** CSV를 다시 읽는다 (콘솔 yjj.ReloadData) */
	void Reload();
}
