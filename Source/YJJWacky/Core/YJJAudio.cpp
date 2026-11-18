#include "Core/YJJAudio.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "YJJAssetPaths.h"

TMap<FString, TWeakObjectPtr<USoundBase>> YJJAudio::Cache;
TMap<uint8, double> YJJAudio::LastPlayed;

const TCHAR* YJJAudio::SoundName(EYJJSound Sound)
{
	switch (Sound)
	{
	case EYJJSound::Punch: return TEXT("SFX_Punch");
	case EYJJSound::Cannon: return TEXT("SFX_Cannon");
	case EYJJSound::Boom: return TEXT("SFX_Boom");
	case EYJJSound::Bonk: return TEXT("SFX_Bonk");
	case EYJJSound::Whoosh: return TEXT("SFX_Whoosh");
	case EYJJSound::Splash: return TEXT("SFX_Splash");
	case EYJJSound::Pop: return TEXT("SFX_Pop");
	case EYJJSound::Build: return TEXT("SFX_Build");
	case EYJJSound::Sell: return TEXT("SFX_Sell");
	case EYJJSound::Coin: return TEXT("SFX_Coin");
	case EYJJSound::WaveStart: return TEXT("SFX_WaveStart");
	case EYJJSound::WaveClear: return TEXT("SFX_WaveClear");
	case EYJJSound::Chomp: return TEXT("SFX_Chomp");
	case EYJJSound::Combo: return TEXT("SFX_Combo");
	case EYJJSound::Win: return TEXT("SFX_Win");
	case EYJJSound::Lose: return TEXT("SFX_Lose");
	case EYJJSound::Deny: return TEXT("SFX_Deny");
	case EYJJSound::FanOn: return TEXT("SFX_FanOn");
	case EYJJSound::Upgrade: return TEXT("SFX_Upgrade");
	case EYJJSound::Sling: return TEXT("SFX_Sling");
	case EYJJSound::Grab: return TEXT("SFX_Grab");
	}
	return nullptr;
}

USoundBase* YJJAudio::LoadSound(const TCHAR* Name)
{
	if (TWeakObjectPtr<USoundBase>* Found = Cache.Find(Name))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}
	const FString Path = FString::Printf(TEXT("%s/%s.%s"), YJJAssetPaths::AudioDir, Name, Name);
	USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Sound)
	{
		Sound->AddToRoot();
		Cache.Add(Name, Sound);
	}
	return Sound;
}

bool YJJAudio::Throttle(const UWorld* World, EYJJSound Sound)
{
	const double Now = World->GetRealTimeSeconds();
	double& Last = LastPlayed.FindOrAdd(static_cast<uint8>(Sound), -1.0);
	if (Now >= Last && Now - Last < MinInterval)
	{
		return false;
	}
	Last = Now;
	return true;
}

void YJJAudio::Play(const UObject* WorldContext, EYJJSound Sound, const FVector& Location, float Volume, float Pitch)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const TCHAR* Name = SoundName(Sound);
	USoundBase* Asset = Name ? LoadSound(Name) : nullptr;
	if (!Asset || !World || !Throttle(World, Sound))
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
			Scale = FMath::Clamp(NearBoost - Dist / FadeDistance, MinDistanceScale, 1.f);
		}
	}
	UGameplayStatics::PlaySound2D(WorldContext, Asset, Volume * Scale, Pitch);
}

void YJJAudio::StartMusic(const UObject* WorldContext)
{
	if (USoundWave* Bgm = Cast<USoundWave>(LoadSound(MusicName)))
	{
		Bgm->bLooping = true;
		UGameplayStatics::SpawnSound2D(WorldContext, Bgm, MusicVolume);
	}
}
