#pragma once

#include "CoreMinimal.h"

/** 효과음·배경음. 소리는 Tools/Audio/generate_audio.py가 직접 합성한다 (외부 음원 없음) */
enum class EYJJSound : uint8
{
	Punch,		// 권투 장갑
	Cannon,		// 대포 발사
	Boom,		// 포탄 착탄
	Bonk,		// 몽둥이·딱밤
	Whoosh,		// 몽둥이 회오리
	Splash,		// 풍덩
	Pop,		// 적 쓰러짐
	Build,
	Sell,
	Coin,
	WaveStart,
	WaveClear,
	Chomp,		// 푸딩 뜯김
	Combo,		// 떼죽음
	Win,
	Lose,
	Deny,		// 돈 없음
	FanOn,
};

namespace YJJAudio
{
	/** Location이 0이면 거리와 상관없이 재생 */
	void Play(const UObject* WorldContext, EYJJSound Sound, const FVector& Location = FVector::ZeroVector, float Volume = 1.f, float Pitch = 1.f);

	/** 배경음 (반복) */
	void StartMusic(const UObject* WorldContext);
}
