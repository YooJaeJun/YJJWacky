#pragma once

#include "CoreMinimal.h"
#include "YJJTypes.generated.h"

/** 흐느적이 종류. Enemies.csv의 행 이름이 이 이름과 같아야 한다 */
UENUM()
enum class EYJJWobblerKind : uint8
{
	Grunt,		// 졸병
	Runner,		// 날쌘돌이: 작고 빠르고 가볍다 (멀리 날아간다)
	Brute,		// 뚱보: 크고 무겁고 느리다
	Boss,		// 대장: 거대
	Tiny,		// 꼬마: 아주 작고 가볍다 (새총 탄알 1순위)
	Guard,		// 아군 몽둥이 병사
};

/** 타워 종류. Towers.csv의 행 이름이 이 이름과 같아야 한다 */
UENUM()
enum class EYJJTowerType : uint8
{
	Punch,		// 권투 장갑: 길 밖으로 날려 버린다
	Cannon,		// 대포: 멀리서 포탄을 굴린다
	Fan,		// 선풍기: 바람으로 밀어낸다
	Guard,		// 몽둥이 병사: 길목에서 빙글빙글
	Sling,		// 새총: 가벼운 놈을 붙잡아 탄알로 쏜다
};

inline constexpr int32 YJJTowerTypeCount = 5;
inline constexpr int32 YJJMaxTowerLevel = 3;
