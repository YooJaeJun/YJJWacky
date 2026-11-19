#pragma once

#include "CoreMinimal.h"

/** 타워 발판 하나 */
struct FYJJPad
{
	FVector Location = FVector::ZeroVector;		// 발판 윗면 가운데
	FVector Facing = FVector::ForwardVector;	// 길 쪽을 보는 방향
	FVector PathPoint = FVector::ZeroVector;	// 가장 가까운 길 가운데 점
};

/**
 * 맵 배치. 물 위에 솟은 둑길을 따라 적이 걸어오고, 길옆 발판에 타워를 짓는다.
 * 길에는 난간이 없어서 밀려난 적은 물에 빠진다.
 * 길 꼭짓점은 MapPath.csv, 발판은 MapPads.csv, 폭·높이는 Tuning.csv (Map.*).
 */
class YJJMap
{
public:
	using FPad = FYJJPad;

	static float PathWidth();
	static float PadRadius();
	static float WaterZ();
	/** 몸통이 이 높이 아래로 내려가면 물에 빠진 것 */
	static float KillZ();
	static float SpawnRadius();
	static float BaseRadius();

	/** 길 가운데 선 (윗면 높이 Z = 0) */
	static const TArray<FVector>& Path();
	static float PathLength();

	/** 출발점에서 길을 따라 Distance만큼 간 점 */
	static FVector PointAt(float Distance, FVector* OutDir = nullptr);

	/** 길 위 가장 가까운 점까지의 진행 거리. OutLateral은 길 가운데에서 오른쪽으로 벗어난 거리 */
	static float Project(const FVector& Location, FVector* OutPoint = nullptr, float* OutLateral = nullptr);

	static const TArray<FPad>& Pads();

	/** 지켜야 하는 푸딩 */
	static FVector BaseLocation();

private:
	/** 데이터를 다시 읽었으면 길·발판을 다시 만든다 */
	static void EnsureBuilt();

	static int32 BuiltGeneration;
	static TArray<FVector> PathPoints;
	static TArray<FPad> PadList;
	static float Length;
};
