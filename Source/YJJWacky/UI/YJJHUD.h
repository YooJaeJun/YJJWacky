#pragma once

#include "CoreMinimal.h"
#include "Core/YJJGame.h"
#include "GameFramework/HUD.h"
#include "YJJHUD.generated.h"

class AYJJCommander;
struct FCompositeFont;

/**
 * 게임 HUD (Canvas, 위젯 에셋 없음).
 * 푸딩 체력·골드, 웨이브, 타워 카드, 출격 버튼, 딱밤 게이지, 떠오르는 글자, 결과 카드.
 */
UCLASS()
class YJJWACKY_API AYJJHUD : public AHUD
{
	GENERATED_BODY()

public:
	static constexpr int32 ButtonStartWave = 10;
	static constexpr int32 ButtonRestart = 11;

	virtual void DrawHUD() override;

	/** 화면 좌표에 있는 버튼 번호 (0~3 타워 카드). 없으면 INDEX_NONE */
	int32 HitTest(const FVector2D& Screen) const;

private:
	enum class EAlign : uint8 { Left, Center, Right };

	float S(float V) const { return V * UIScale; }
	void Text(const FString& Str, float X, float Y, float Size, const FLinearColor& Color, EAlign Align = EAlign::Left, float Outline = 2.f);
	FVector2D Measure(const FString& Str, float Size);
	void Panel(float X, float Y, float W, float H, const FLinearColor& Color, float Corner = 14.f);
	void Rect(float X, float Y, float W, float H, const FLinearColor& Color);
	void Circle(float X, float Y, float R, const FLinearColor& Color, int32 Sides = 24);
	void Heart(float X, float Y, float R, const FLinearColor& Color);
	void Coin(float X, float Y, float R);
	void TowerIcon(EYJJTowerType Type, float X, float Y, float R);
	void AddButton(float X, float Y, float W, float H, int32 Id);

	void DrawStatus(const UYJJGame& Game);
	void DrawWave(const UYJJGame& Game);
	void DrawCards(const UYJJGame& Game, const AYJJCommander& Commander);
	void DrawStart(const UYJJGame& Game);
	void DrawFlick(const UYJJGame& Game);
	void DrawWorld(const UYJJGame& Game, const AYJJCommander& Commander);
	void DrawBanner(const UYJJGame& Game);
	void DrawEnd(const UYJJGame& Game);

	TSharedPtr<const FCompositeFont> GetGameFont();
	TSharedPtr<const FCompositeFont> GameFont;

	struct FButton
	{
		FBox2D Box;
		int32 Id;
	};
	TArray<FButton> Buttons;

	float UIScale = 1.f;
	FVector2D Mouse = FVector2D(-1000.f, -1000.f);
};
