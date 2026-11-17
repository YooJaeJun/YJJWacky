#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "YJJWackyGameMode.generated.h"

/** 기본 게임모드: 전장과 이펙트 액터를 만들고, 내려다보는 카메라를 세운다. */
UCLASS()
class YJJWACKY_API AYJJWackyGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AYJJWackyGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
};
