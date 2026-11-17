#include "World/YJJMap.h"

namespace
{
	struct FPadSpec
	{
		int32 Segment;
		float T;
		float Side;	// +1 = 진행 방향 오른쪽
	};
}

const TArray<FVector>& YJJMap::Path()
{
	static const TArray<FVector> Points = {
		FVector(-2400, -800, 0), FVector(-1000, -800, 0), FVector(-1000, 400, 0), FVector(200, 400, 0),
		FVector(200, -800, 0), FVector(1400, -800, 0), FVector(1400, 350, 0) };
	return Points;
}

float YJJMap::PathLength()
{
	static float Length = 0.f;
	if (Length <= 0.f)
	{
		const TArray<FVector>& P = Path();
		for (int32 i = 0; i + 1 < P.Num(); ++i)
		{
			Length += FVector::Dist(P[i], P[i + 1]);
		}
	}
	return Length;
}

FVector YJJMap::PointAt(float Distance, FVector* OutDir)
{
	const TArray<FVector>& P = Path();
	Distance = FMath::Clamp(Distance, 0.f, PathLength());
	for (int32 i = 0; i + 1 < P.Num(); ++i)
	{
		const float Len = FVector::Dist(P[i], P[i + 1]);
		if (Distance <= Len || i + 2 == P.Num())
		{
			const FVector Dir = (P[i + 1] - P[i]) / Len;
			if (OutDir)
			{
				*OutDir = Dir;
			}
			return P[i] + Dir * FMath::Min(Distance, Len);
		}
		Distance -= Len;
	}
	return P.Last();
}

float YJJMap::Project(const FVector& Location, FVector* OutPoint, float* OutLateral)
{
	const TArray<FVector>& P = Path();
	float BestDistSq = TNumericLimits<float>::Max();
	float BestProgress = 0.f;
	float Walked = 0.f;
	for (int32 i = 0; i + 1 < P.Num(); ++i)
	{
		const FVector Seg = P[i + 1] - P[i];
		const float Len = Seg.Size();
		const FVector Dir = Seg / Len;
		const float T = FMath::Clamp(FVector::DotProduct(Location - P[i], Dir), 0.f, Len);
		const FVector Closest = P[i] + Dir * T;
		const float DistSq = FVector::DistSquared2D(Location, Closest);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestProgress = Walked + T;
			if (OutPoint)
			{
				*OutPoint = Closest;
			}
			if (OutLateral)
			{
				*OutLateral = FVector::DotProduct(Location - Closest, FVector(-Dir.Y, Dir.X, 0.f));
			}
		}
		Walked += Len;
	}
	return BestProgress;
}

const TArray<YJJMap::FPad>& YJJMap::Pads()
{
	static TArray<FPad> Result;
	if (Result.Num() == 0)
	{
		// 구불구불한 길의 안쪽·바깥쪽에 번갈아. 서로 겹치지 않게 T를 어긋나게 잡았다
		const FPadSpec Specs[] = {
			{ 0, 0.45f, 1 }, { 0, 0.75f, -1 }, { 1, 0.5f, 1 }, { 1, 0.3f, -1 }, { 2, 0.5f, 1 }, { 2, 0.5f, -1 },
			{ 3, 0.7f, -1 }, { 3, 0.3f, 1 }, { 4, 0.5f, 1 }, { 4, 0.5f, -1 }, { 5, 0.7f, 1 }, { 5, 0.4f, -1 } };
		const TArray<FVector>& P = Path();
		for (const FPadSpec& S : Specs)
		{
			const FVector Dir = (P[S.Segment + 1] - P[S.Segment]).GetSafeNormal();
			const FVector Right(-Dir.Y, Dir.X, 0.f);
			FPad Pad;
			Pad.PathPoint = FMath::Lerp(P[S.Segment], P[S.Segment + 1], S.T);
			Pad.Location = Pad.PathPoint + Right * S.Side * (PathWidth * 0.5f + PadRadius - 30.f);
			Pad.Facing = -Right * S.Side;
			Result.Add(Pad);
		}
	}
	return Result;
}

FVector YJJMap::BaseLocation()
{
	return Path().Last() + FVector(0, 440, 0);
}
