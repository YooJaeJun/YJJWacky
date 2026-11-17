#pragma once

#include "CoreMinimal.h"

/**
 * 맵 배치. 물 위에 솟은 둑길을 따라 적이 걸어오고, 길옆 발판에 타워를 짓는다.
 * 길에는 난간이 없어서 밀려난 적은 물에 빠진다.
 */
namespace YJJMap
{
	inline constexpr float PathWidth = 480.f;
	inline constexpr float PadRadius = 180.f;
	inline constexpr float WaterZ = -330.f;
	/** 몸통이 이 높이 아래로 내려가면 물에 빠진 것 */
	inline constexpr float KillZ = -250.f;
	inline constexpr float SpawnRadius = 380.f;
	inline constexpr float BaseRadius = 520.f;

	struct FPad
	{
		FVector Location;	// 발판 윗면 가운데
		FVector Facing;		// 길 쪽을 보는 방향
		FVector PathPoint;	// 가장 가까운 길 가운데 점
	};

	/** 길 가운데 선 (윗면 높이 Z = 0) */
	const TArray<FVector>& Path();
	float PathLength();

	/** 출발점에서 길을 따라 Distance만큼 간 점 */
	FVector PointAt(float Distance, FVector* OutDir = nullptr);

	/** 길 위 가장 가까운 점까지의 진행 거리. OutLateral은 길 가운데에서 오른쪽으로 벗어난 거리 */
	float Project(const FVector& Location, FVector* OutPoint = nullptr, float* OutLateral = nullptr);

	const TArray<FPad>& Pads();

	/** 지켜야 하는 푸딩 */
	FVector BaseLocation();
}
