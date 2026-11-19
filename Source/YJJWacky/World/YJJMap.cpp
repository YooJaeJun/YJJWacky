#include "World/YJJMap.h"

#include "Data/YJJData.h"

int32 YJJMap::BuiltGeneration = INDEX_NONE;
TArray<FVector> YJJMap::PathPoints;
TArray<FYJJPad> YJJMap::PadList;
float YJJMap::Length = 0.f;

float YJJMap::PathWidth() { return YJJData::Tune(TEXT("Map.PathWidth")); }
float YJJMap::PadRadius() { return YJJData::Tune(TEXT("Map.PadRadius")); }
float YJJMap::WaterZ() { return YJJData::Tune(TEXT("Map.WaterZ")); }
float YJJMap::KillZ() { return YJJData::Tune(TEXT("Map.KillZ")); }
float YJJMap::SpawnRadius() { return YJJData::Tune(TEXT("Map.SpawnRadius")); }
float YJJMap::BaseRadius() { return YJJData::Tune(TEXT("Map.BaseRadius")); }

void YJJMap::EnsureBuilt()
{
	if (BuiltGeneration == YJJData::Generation())
	{
		return;
	}
	BuiltGeneration = YJJData::Generation();

	PathPoints = YJJData::PathPoints();
	Length = 0.f;
	for (int32 i = 0; i + 1 < PathPoints.Num(); ++i)
	{
		Length += FVector::Dist(PathPoints[i], PathPoints[i + 1]);
	}

	// 발판: 길 구간 Segment 의 T 지점에서 Side 쪽(+1 = 진행 방향 오른쪽)으로, 길 가장자리에 조금 겹쳐서
	PadList.Reset();
	const float Offset = PathWidth() * 0.5f + PadRadius() - YJJData::Tune(TEXT("Map.PadOverlap"));
	for (const FYJJPadRow& Row : YJJData::PadRows())
	{
		if (!PathPoints.IsValidIndex(Row.Segment) || !PathPoints.IsValidIndex(Row.Segment + 1))
		{
			continue;
		}
		const FVector Dir = (PathPoints[Row.Segment + 1] - PathPoints[Row.Segment]).GetSafeNormal();
		const FVector Right(-Dir.Y, Dir.X, 0.f);
		FPad& Pad = PadList.AddDefaulted_GetRef();
		Pad.PathPoint = FMath::Lerp(PathPoints[Row.Segment], PathPoints[Row.Segment + 1], Row.T);
		Pad.Location = Pad.PathPoint + Right * Row.Side * Offset;
		Pad.Facing = -Right * Row.Side;
	}
}

const TArray<FVector>& YJJMap::Path()
{
	EnsureBuilt();
	return PathPoints;
}

float YJJMap::PathLength()
{
	EnsureBuilt();
	return Length;
}

FVector YJJMap::PointAt(float Distance, FVector* OutDir)
{
	const TArray<FVector>& P = Path();
	if (P.Num() < 2)
	{
		return P.Num() ? P[0] : FVector::ZeroVector;
	}
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

const TArray<FYJJPad>& YJJMap::Pads()
{
	EnsureBuilt();
	return PadList;
}

FVector YJJMap::BaseLocation()
{
	const TArray<FVector>& P = Path();
	return (P.Num() ? P.Last() : FVector::ZeroVector) + FVector(0.f, YJJData::Tune(TEXT("Map.BaseOffsetY")), 0.f);
}
