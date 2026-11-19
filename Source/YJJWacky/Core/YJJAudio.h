#pragma once

#include "CoreMinimal.h"

class USoundBase;
class UWorld;

/**
 * 효과음·배경음. 소리는 Tools/Audio/generate_audio.py가 직접 합성한다 (외부 음원 없음).
 * 소리 큐(이름)마다 파일·볼륨·음높이는 Content/YJJWacky/Data/Sounds.csv.
 */
class YJJAudio
{
public:
	/** Sounds.csv 의 Cue 를 재생한다. Location이 0이면 거리와 상관없이. PitchScale은 큰 놈일수록 낮게 할 때 */
	static void Play(const UObject* WorldContext, const TCHAR* Cue, const FVector& Location = FVector::ZeroVector, float PitchScale = 1.f);

	/** 배경음 (Sounds.csv 의 Music, 반복) */
	static void StartMusic(const UObject* WorldContext);

private:
	static constexpr const TCHAR* MusicCue = TEXT("Music");
	/** 아직 한 번도 안 울린 소리의 마지막 재생 시각 */
	static constexpr double NeverPlayed = -1.0;

	static USoundBase* LoadSound(const FString& Name);
	static bool Throttle(const UWorld* World, const TCHAR* Cue);

	static TMap<FString, TWeakObjectPtr<USoundBase>> Cache;
	static TMap<FName, double> LastPlayed;
};
