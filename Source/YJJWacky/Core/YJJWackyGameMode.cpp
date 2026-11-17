#include "Core/YJJWackyGameMode.h"

#include "Core/YJJAudio.h"
#include "Engine/World.h"
#include "Gameplay/YJJEffects.h"
#include "Player/YJJCommander.h"
#include "UI/YJJHUD.h"
#include "World/YJJArena.h"

AYJJWackyGameMode::AYJJWackyGameMode()
{
	DefaultPawnClass = AYJJCommanderPawn::StaticClass();
	PlayerControllerClass = AYJJCommander::StaticClass();
	HUDClass = AYJJHUD::StaticClass();
}

void AYJJWackyGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GetWorld()->SpawnActor<AYJJArena>(AYJJArena::StaticClass(), FTransform::Identity, Params);
	GetWorld()->SpawnActor<AYJJEffects>(AYJJEffects::StaticClass(), FTransform::Identity, Params);
}

void AYJJWackyGameMode::StartPlay()
{
	Super::StartPlay();
	YJJAudio::StartMusic(this);
}
