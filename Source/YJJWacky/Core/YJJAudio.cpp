#include "Core/YJJAudio.h"

#include "Camera/PlayerCameraManager.h"
#include "Data/YJJData.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "YJJAssetPaths.h"

TMap<FString, TWeakObjectPtr<USoundBase>> YJJAudio::Cache;
TMap<FName, double> YJJAudio::LastPlayed;

USoundBase* YJJAudio::LoadSound(const FString& Name)
{
	if (TWeakObjectPtr<USoundBase>* Found = Cache.Find(Name))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}
	const FString Path = FString::Printf(TEXT("%s/%s.%s"), YJJAssetPaths::AudioDir, *Name, *Name);
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Sound)
	{
		Sound->AddToRoot();
		Cache.Add(Name, Sound);
	}
	return Sound;
}

bool YJJAudio::Throttle(const UWorld* World, const TCHAR* Cue)
{
	// 같은 소리가 한 프레임에 수십 개 겹치면 귀가 찢어진다. 소리마다 최소 간격을 둔다
	const double Now = World->GetRealTimeSeconds();
	double& Last = LastPlayed.FindOrAdd(FName(Cue), NeverPlayed);
	if (Now >= Last && Now - Last < YJJData::Tune(TEXT("Audio.MinInterval")))
	{
		return false;
	}
	Last = Now;
	return true;
}

void YJJAudio::Play(const UObject* WorldContext, const TCHAR* Cue, const FVector& Location, float PitchScale)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const FYJJSoundRow* Row = YJJData::Sound(Cue);
	USoundBase* Asset = Row ? LoadSound(Row->Asset) : nullptr;
	if (!Asset || !World || !Throttle(World, Cue))
	{
		return;
	}
	// 내려다보는 카메라라 3D 감쇠 대신 2D로 재생하고, 카메라에서 멀면 조금 줄인다
	float Scale = 1.f;
	if (!Location.IsZero())
	{
		if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(WorldContext, 0))
		{
			const float Dist = FVector::Dist(Camera->GetCameraLocation(), Location);
			Scale = FMath::Clamp(YJJData::Tune(TEXT("Audio.NearBoost")) - Dist / YJJData::Tune(TEXT("Audio.FadeDistance")),
				YJJData::Tune(TEXT("Audio.MinDistanceScale")), 1.f);
		}
	}
	const float Pitch = FMath::FRandRange(Row->PitchMin, Row->PitchMax) * PitchScale;
	UGameplayStatics::PlaySound2D(WorldContext, Asset, Row->Volume * Scale, Pitch);
}

void YJJAudio::StartMusic(const UObject* WorldContext)
{
	const FYJJSoundRow* Row = YJJData::Sound(MusicCue);
	if (USoundWave* Bgm = Row ? Cast<USoundWave>(LoadSound(Row->Asset)) : nullptr)
	{
		Bgm->bLooping = true;
		UGameplayStatics::SpawnSound2D(WorldContext, Bgm, Row->Volume);
	}
}
