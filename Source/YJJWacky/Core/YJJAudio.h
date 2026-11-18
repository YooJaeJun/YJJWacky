#pragma once

#include "CoreMinimal.h"

class USoundBase;
class UWorld;

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
	Upgrade,	// 강화 띠리링
	Sling,		// 새총 고무줄 퉁
	Grab,		// 붙잡힌 놈의 비명
};

/** 소리 재생 (정적 멤버만 있는 도우미) */
class YJJAudio
{
public:
	/** Location이 0이면 거리와 상관없이 재생 */
	static void Play(const UObject* WorldContext, EYJJSound Sound, const FVector& Location = FVector::ZeroVector, float Volume = 1.f, float Pitch = 1.f);

	/** 배경음 (반복) */
	static void StartMusic(const UObject* WorldContext);

private:
	/** 같은 소리끼리 최소 간격(실시간 초). 한 프레임에 수십 개 겹치면 귀가 찢어진다 */
	static constexpr double MinInterval = 0.05;
	/** 카메라에서 이만큼(cm) 멀어지면 소리가 가장 작아진다 */
	static constexpr float FadeDistance = 12000.f;
	static constexpr float NearBoost = 1.2f;
	static constexpr float MinDistanceScale = 0.35f;
	static constexpr float MusicVolume = 0.45f;
	static constexpr const TCHAR* MusicName = TEXT("BGM_Wacky");

	static const TCHAR* SoundName(EYJJSound Sound);
	static USoundBase* LoadSound(const TCHAR* Name);
	static bool Throttle(const UWorld* World, EYJJSound Sound);

	static TMap<FString, TWeakObjectPtr<USoundBase>> Cache;
	static TMap<uint8, double> LastPlayed;
};
