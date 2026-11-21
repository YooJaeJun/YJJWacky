#pragma once

#include "CoreMinimal.h"
#include "Core/YJJGame.h"
#include "GameFramework/HUD.h"
#include "YJJHUD.generated.h"

class AYJJCommander;
class UFont;
struct FCompositeFont;

/**
 * 게임 HUD (Canvas, 위젯 에셋 없음).
 * 푸딩 체력·골드, 웨이브, 타워 카드, 출격 버튼(일찍 부르기), 딱밤·주문 버튼, 주문 조준 원, 떠오르는 글자, 결과 카드(별점).
 * 배치·글자 크기는 HudLayout.csv, 아이콘은 HudIcons.csv, 색은 Colors.csv (Hud*), 글자는 Localization.csv.
 */
UCLASS()
class YJJWACKY_API AYJJHUD : public AHUD
{
	GENERATED_BODY()

public:
	static constexpr int32 ButtonStartWave = 10;
	static constexpr int32 ButtonRestart = 11;
	/** 주문 버튼: ButtonSpell + EYJJSpell */
	static constexpr int32 ButtonSpell = 20;

	virtual void DrawHUD() override;

	/** 화면 좌표에 있는 버튼 번호 (0~4 타워 카드). 없으면 INDEX_NONE */
	int32 HitTest(const FVector2D& Screen) const;

private:
	enum class EAlign : uint8 { Left, Center, Right };

	struct FButton
	{
		FBox2D Box;
		int32 Id;
	};

	static float L(const TCHAR* Name) { return YJJData::Layout(Name); }
	static FLinearColor C(const TCHAR* Name) { return YJJData::Color(Name); }
	static FLinearColor WithAlpha(FLinearColor Color, float Alpha);
	static const UFont* BaseFont();

	/** 화면 높이에 맞춘 크기 */
	float S(float V) const { return V * UIScale; }
	/** HudLayout.csv 값을 화면 크기에 맞춰서 */
	float SL(const TCHAR* Name) const { return S(L(Name)); }

	void Text(const FString& Str, float X, float Y, float Size, const FLinearColor& Color, EAlign Align = EAlign::Left, float Outline = OutlineDefault);
	FVector2D Measure(const FString& Str, float Size);
	void Panel(float X, float Y, float W, float H, const FLinearColor& Color, float Corner = PanelCornerDefault);
	void Rect(float X, float Y, float W, float H, const FLinearColor& Color);
	void Circle(float X, float Y, float R, const FLinearColor& Color, int32 Sides = CircleSidesDefault);
	void Heart(float X, float Y, float R, const FLinearColor& Color);
	/** 12시부터 시계 방향으로 Fraction 만큼 채운 부채꼴 */
	void Pie(float X, float Y, float R, float Fraction, const FLinearColor& Color);
	/** 별 (꼭짓점 다섯) */
	void Star(float X, float Y, float R, const FLinearColor& Color);
	/** 땅 위 원 (월드 좌표 Center, 반지름 Radius cm) */
	void GroundRing(const FVector& Center, float Radius, const FLinearColor& Color);
	/** HudIcons.csv 의 Icon 을 (X, Y) 가운데 반지름 R 로 그린다 */
	void DrawIcon(const FString& Icon, float X, float Y, float R);
	void AddButton(float X, float Y, float W, float H, int32 Id);

	void DrawStatus(const UYJJGame& Game);
	void DrawWave(const UYJJGame& Game);
	void DrawCards(const UYJJGame& Game, const AYJJCommander& Commander);
	void DrawStart(const UYJJGame& Game);
	void DrawFlick(const UYJJGame& Game);
	void DrawSpells(const UYJJGame& Game, const AYJJCommander& Commander);
	void DrawAim(const UYJJGame& Game, const AYJJCommander& Commander);
	void DrawWorld(const UYJJGame& Game, const AYJJCommander& Commander);
	void DrawBanner(const UYJJGame& Game);
	void DrawEnd(const UYJJGame& Game);

	TSharedPtr<const FCompositeFont> GetGameFont();

	// 그리기 기본값
	static constexpr float OutlineDefault = 2.f;
	static constexpr float PanelCornerDefault = 12.f;
	static constexpr int32 CircleSidesDefault = 24;
	/** 둥근 모서리 하나를 이만큼의 조각으로 */
	static constexpr int32 CornerSteps = 5;
	static constexpr float QuarterTurn = 90.f;
	static constexpr int32 MinFontSize = 6;
	/** 이보다 투명하면 그리지 않는다 */
	static constexpr float MinVisibleAlpha = 0.01f;
	/** 글자 크기를 못 잴 때 글자 하나 폭 어림 */
	static constexpr float GlyphWidthGuess = 0.6f;
	/** 마우스가 화면 밖일 때 쓰는 좌표 */
	static constexpr float OffscreenMouse = -1000.f;
	static constexpr const TCHAR* BaseFontPath = TEXT("/Engine/EngineFonts/Roboto.Roboto");

	// 하트: 원 둘 + 아래 삼각형 (반지름 R 의 배수)
	static constexpr float HeartLobeX = 0.5f;
	static constexpr float HeartLobeY = 0.15f;
	static constexpr float HeartLobeR = 0.56f;
	static constexpr float HeartWide = 1.03f;
	static constexpr float HeartTip = 1.05f;
	static constexpr int32 HeartSides = 16;

	// 별: 꼭짓점 다섯, 12시 방향부터
	static constexpr int32 StarPoints = 5;
	static constexpr float StarStartDeg = -90.f;
	/** 결과 카드 별 수 */
	static constexpr int32 MaxStars = 3;

	TSharedPtr<const FCompositeFont> GameFont;
	TArray<FButton> Buttons;
	float UIScale = 1.f;
	FVector2D Mouse = FVector2D(OffscreenMouse, OffscreenMouse);
};
