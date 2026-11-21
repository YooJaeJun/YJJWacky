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
	Hero,		// 영웅 프라이팬 아저씨: 플레이어가 보낸 자리에서 싸운다 (Tuning.csv Hero.*)
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

/** 주문 (킹덤러쉬식: 쿨타임이 차면 버튼을 누르고 땅을 찍는다). Tuning.csv Spell.<이름>.* */
UENUM()
enum class EYJJSpell : uint8
{
	Boulder,	// 하늘에서 큰 바위가 떨어져 쾅, 길을 거꾸로 굴러간다
	Reinforce,	// 농민 지원군 둘을 하늘에서 떨어뜨린다 (잠깐 싸우고 사라짐)
};

inline constexpr int32 YJJTowerTypeCount = 5;
inline constexpr int32 YJJSpellCount = 2;
inline constexpr int32 YJJMaxTowerLevel = 3;
