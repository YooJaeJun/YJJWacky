#include "UI/YJJHUD.h"

#include "CanvasItem.h"
#include "Data/YJJLoc.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Player/YJJCommander.h"
#include "Rendering/SlateRenderer.h"
#include "TextureResource.h"
#include "World/YJJMap.h"
#include "World/YJJShapes.h"
#include "YJJAssetPaths.h"

namespace
{
	namespace HudColor
	{
		const FLinearColor Ink(0.035f, 0.045f, 0.09f, 1.f);
		const FLinearColor PanelBg(0.05f, 0.07f, 0.14f, 0.8f);
		const FLinearColor Cream(1.f, 0.97f, 0.9f, 1.f);
		const FLinearColor Dim(0.72f, 0.76f, 0.86f, 1.f);
		const FLinearColor Gold(1.f, 0.8f, 0.2f, 1.f);
		const FLinearColor GoldDark(0.75f, 0.5f, 0.05f, 1.f);
		const FLinearColor Red(1.f, 0.32f, 0.32f, 1.f);
		const FLinearColor Green(0.55f, 0.9f, 0.4f, 1.f);
		const FLinearColor Blue(0.25f, 0.55f, 1.f, 1.f);
	}

	const UFont* BaseFont()
	{
		static const UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
		return Font;
	}

	FLinearColor WithAlpha(FLinearColor C, float A)
	{
		C.A *= A;
		return C;
	}
}

TSharedPtr<const FCompositeFont> AYJJHUD::GetGameFont()
{
	if (GameFont.IsValid())
	{
		return GameFont;
	}
	// TTF 파일을 직접 읽는다 (커맨드릿에선 폰트 임포트가 안 되므로 에셋 대신 파일)
	const FString FontPath = FPaths::ProjectContentDir() / YJJAssetPaths::GameFontFile;
	if (!FPaths::FileExists(FontPath))
	{
		return nullptr;
	}
	TSharedRef<FCompositeFont> Composite = MakeShared<FCompositeFont>();
	Composite->DefaultTypeface.Fonts.Add(FTypefaceEntry(TEXT("Regular"), FontPath, EFontHinting::Default, EFontLoadingPolicy::LazyLoad));
	if (const UFont* Base = BaseFont())
	{
		if (const FCompositeFont* Fallback = Base->GetCompositeFont())
		{
			Composite->FallbackTypeface.Typeface = Fallback->DefaultTypeface;
		}
	}
	GameFont = Composite;
	return GameFont;
}

void AYJJHUD::Text(const FString& Str, float X, float Y, float Size, const FLinearColor& Color, EAlign Align, float Outline)
{
	const UFont* Base = BaseFont();
	if (!Base || Str.IsEmpty() || Color.A <= 0.01f)
	{
		return;
	}
	const int32 FontSize = FMath::Max(FMath::RoundToInt(S(Size)), 6);
	const TSharedPtr<const FCompositeFont> Composite = GetGameFont();
	FSlateFontInfo Font = Composite.IsValid() ? FSlateFontInfo(Composite, FontSize) : FSlateFontInfo(Base, FontSize);
	if (Outline > 0.f)
	{
		Font.OutlineSettings.OutlineSize = FMath::Max(FMath::RoundToInt(S(Outline)), 1);
		Font.OutlineSettings.OutlineColor = FLinearColor(HudColor::Ink.R, HudColor::Ink.G, HudColor::Ink.B, Color.A);
	}

	float DrawX = X;
	if (Align != EAlign::Left)
	{
		const FVector2D Size2 = Measure(Str, Size);
		DrawX = (Align == EAlign::Center) ? X - Size2.X * 0.5f : X - Size2.X;
	}
	FCanvasTextItem Item(FVector2D(DrawX, Y), FText::FromString(Str), Font, Color);
	Item.Font = Base;
	Item.bCentreY = true;
	Canvas->DrawItem(Item);
}

FVector2D AYJJHUD::Measure(const FString& Str, float Size)
{
	const UFont* Base = BaseFont();
	const TSharedPtr<const FCompositeFont> Composite = GetGameFont();
	const int32 FontSize = FMath::Max(FMath::RoundToInt(S(Size)), 6);
	FSlateFontInfo Font = Composite.IsValid() ? FSlateFontInfo(Composite, FontSize) : FSlateFontInfo(Base, FontSize);
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
	{
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Str, Font);
	}
	return FVector2D(Str.Len() * S(Size) * 0.6f, S(Size));
}

void AYJJHUD::Panel(float X, float Y, float W, float H, const FLinearColor& Color, float Corner)
{
	// 둥근 모서리 패널: 겹쳐 그리면 반투명이 진해지므로 테두리 다각형 하나를 부채꼴로 채운다
	const float C = FMath::Min(S(Corner), FMath::Min(W, H) * 0.5f);
	TArray<FVector2D> Outline;
	const FVector2D Corners[4] = { FVector2D(X + W - C, Y + H - C), FVector2D(X + C, Y + H - C), FVector2D(X + C, Y + C), FVector2D(X + W - C, Y + C) };
	for (int32 c = 0; c < 4; ++c)
	{
		for (int32 k = 0; k <= 5; ++k)
		{
			const float A = (c * 90.f + k * 18.f) * PI / 180.f;
			Outline.Add(Corners[c] + FVector2D(FMath::Cos(A), FMath::Sin(A)) * C);
		}
	}
	const FVector2D Center(X + W * 0.5f, Y + H * 0.5f);
	TArray<FCanvasUVTri> Tris;
	for (int32 i = 0; i < Outline.Num(); ++i)
	{
		FCanvasUVTri& T = Tris.AddDefaulted_GetRef();
		T.V0_Pos = Center;
		T.V1_Pos = Outline[i];
		T.V2_Pos = Outline[(i + 1) % Outline.Num()];
		T.V0_Color = T.V1_Color = T.V2_Color = Color;
	}
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void AYJJHUD::Rect(float X, float Y, float W, float H, const FLinearColor& Color)
{
	FCanvasTileItem Item(FVector2D(X, Y), GWhiteTexture, FVector2D(W, H), Color);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void AYJJHUD::Circle(float X, float Y, float R, const FLinearColor& Color, int32 Sides)
{
	TArray<FCanvasUVTri> Tris;
	for (int32 i = 0; i < Sides; ++i)
	{
		const float A0 = i * TWO_PI / Sides;
		const float A1 = (i + 1) * TWO_PI / Sides;
		FCanvasUVTri& T = Tris.AddDefaulted_GetRef();
		T.V0_Pos = FVector2D(X, Y);
		T.V1_Pos = FVector2D(X + FMath::Cos(A0) * R, Y + FMath::Sin(A0) * R);
		T.V2_Pos = FVector2D(X + FMath::Cos(A1) * R, Y + FMath::Sin(A1) * R);
		T.V0_Color = T.V1_Color = T.V2_Color = Color;
	}
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void AYJJHUD::Heart(float X, float Y, float R, const FLinearColor& Color)
{
	Circle(X - R * 0.5f, Y - R * 0.15f, R * 0.56f, Color, 16);
	Circle(X + R * 0.5f, Y - R * 0.15f, R * 0.56f, Color, 16);
	TArray<FCanvasUVTri> Tris;
	FCanvasUVTri& T = Tris.AddDefaulted_GetRef();
	T.V0_Pos = FVector2D(X - R * 1.03f, Y);
	T.V1_Pos = FVector2D(X + R * 1.03f, Y);
	T.V2_Pos = FVector2D(X, Y + R * 1.05f);
	T.V0_Color = T.V1_Color = T.V2_Color = Color;
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void AYJJHUD::Coin(float X, float Y, float R)
{
	Circle(X, Y, R, HudColor::Ink);
	Circle(X, Y, R * 0.84f, HudColor::Gold);
	Circle(X, Y, R * 0.56f, HudColor::GoldDark);
	Circle(X - R * 0.1f, Y - R * 0.1f, R * 0.44f, HudColor::Gold);
}

void AYJJHUD::TowerIcon(EYJJTowerType Type, float X, float Y, float R)
{
	const FLinearColor Ink = HudColor::Ink;
	switch (Type)
	{
	case EYJJTowerType::Punch:
		// 권투 장갑
		Rect(X - R * 1.0f, Y - R * 0.36f, R * 0.62f, R * 0.72f, Ink);
		Rect(X - R * 0.92f, Y - R * 0.28f, R * 0.46f, R * 0.56f, FLinearColor::White);
		Circle(X + R * 0.2f, Y, R * 0.78f, Ink);
		Circle(X + R * 0.2f, Y, R * 0.68f, YJJShapes::Hex(0xEF4444));
		Circle(X + R * 0.02f, Y - R * 0.62f, R * 0.34f, Ink);
		Circle(X + R * 0.02f, Y - R * 0.62f, R * 0.25f, YJJShapes::Hex(0xEF4444));
		break;
	case EYJJTowerType::Cannon:
		Rect(X - R * 0.5f, Y - R * 0.5f, R * 1.5f, R * 0.66f, Ink);
		Rect(X - R * 0.42f, Y - R * 0.42f, R * 1.34f, R * 0.5f, YJJShapes::Hex(0x334155));
		Circle(X - R * 0.4f, Y - R * 0.17f, R * 0.42f, Ink);
		Circle(X - R * 0.4f, Y - R * 0.17f, R * 0.33f, YJJShapes::Hex(0x334155));
		Circle(X - R * 0.15f, Y + R * 0.45f, R * 0.46f, Ink);
		Circle(X - R * 0.15f, Y + R * 0.45f, R * 0.36f, YJJShapes::Hex(0x9A6B3F));
		Circle(X - R * 0.15f, Y + R * 0.45f, R * 0.12f, Ink);
		break;
	case EYJJTowerType::Fan:
		Circle(X, Y, R * 0.95f, Ink);
		Circle(X, Y, R * 0.85f, YJJShapes::Hex(0xE2E8F0));
		for (int32 i = 0; i < 3; ++i)
		{
			const float A = i * TWO_PI / 3.f - 0.5f;
			Circle(X + FMath::Cos(A) * R * 0.42f, Y + FMath::Sin(A) * R * 0.42f, R * 0.32f, YJJShapes::Hex(0x60A5FA), 12);
		}
		Circle(X, Y, R * 0.2f, HudColor::Gold, 12);
		break;
	case EYJJTowerType::Guard:
		// 몽둥이 든 파란 흐느적이
		Rect(X + R * 0.42f, Y - R * 0.95f, R * 0.3f, R * 1.3f, Ink);
		Rect(X + R * 0.48f, Y - R * 0.89f, R * 0.18f, R * 1.18f, YJJShapes::Hex(0x9A6B3F));
		Circle(X - R * 0.15f, Y + R * 0.4f, R * 0.6f, Ink);
		Circle(X - R * 0.15f, Y + R * 0.4f, R * 0.5f, YJJShapes::Hex(0x3B82F6));
		Circle(X - R * 0.15f, Y - R * 0.38f, R * 0.5f, Ink);
		Circle(X - R * 0.15f, Y - R * 0.38f, R * 0.41f, YJJShapes::Hex(0x3B82F6));
		Circle(X - R * 0.3f, Y - R * 0.42f, R * 0.14f, FLinearColor::White, 10);
		Circle(X + R * 0.02f, Y - R * 0.42f, R * 0.14f, FLinearColor::White, 10);
		Circle(X - R * 0.27f, Y - R * 0.4f, R * 0.06f, Ink, 8);
		Circle(X + R * 0.05f, Y - R * 0.4f, R * 0.06f, Ink, 8);
		break;
	case EYJJTowerType::Sling:
	{
		// Y자 가지 + 주머니 속 분홍 꼬마
		const FLinearColor Wood = YJJShapes::Hex(0x9A6B3F);
		Rect(X - R * 0.16f, Y - R * 0.05f, R * 0.32f, R * 1.0f, Ink);
		Rect(X - R * 0.09f, Y + R * 0.0f, R * 0.18f, R * 0.9f, Wood);
		for (const float Side : { -1.f, 1.f })
		{
			Circle(X + Side * R * 0.5f, Y - R * 0.55f, R * 0.2f, Ink, 12);
			Circle(X + Side * R * 0.5f, Y - R * 0.55f, R * 0.13f, YJJShapes::Hex(0xEF4444), 12);
			Circle(X + Side * R * 0.27f, Y - R * 0.25f, R * 0.2f, Ink, 10);
			Circle(X + Side * R * 0.27f, Y - R * 0.25f, R * 0.13f, Wood, 10);
		}
		Rect(X - R * 0.5f, Y - R * 0.6f, R * 1.0f, R * 0.08f, YJJShapes::Hex(0xF97316));
		Circle(X, Y - R * 0.7f, R * 0.3f, Ink, 14);
		Circle(X, Y - R * 0.7f, R * 0.23f, YJJShapes::Hex(0xFF7FA0), 14);
		Circle(X - R * 0.08f, Y - R * 0.74f, R * 0.07f, FLinearColor::White, 8);
		Circle(X + R * 0.08f, Y - R * 0.74f, R * 0.07f, FLinearColor::White, 8);
		break;
	}
	}
}

void AYJJHUD::AddButton(float X, float Y, float W, float H, int32 Id)
{
	Buttons.Add({ FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H)), Id });
}

int32 AYJJHUD::HitTest(const FVector2D& Screen) const
{
	for (const FButton& Button : Buttons)
	{
		if (Button.Box.IsInside(Screen))
		{
			return Button.Id;
		}
	}
	return INDEX_NONE;
}

// ============================================================

void AYJJHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	UIScale = FMath::Clamp(Canvas->ClipY / 1080.f, 0.5f, 2.f);
	Buttons.Reset();

	const AYJJCommander* Commander = Cast<AYJJCommander>(GetOwningPlayerController());
	const UYJJGame* Game = UYJJGame::Get(this);
	if (!Commander || !Game)
	{
		return;
	}
	float MouseX = 0.f, MouseY = 0.f;
	Mouse = Commander->GetMousePosition(MouseX, MouseY) ? FVector2D(MouseX, MouseY) : FVector2D(-1000.f, -1000.f);

	DrawWorld(*Game, *Commander);
	DrawStatus(*Game);
	DrawWave(*Game);
	const bool bEnded = Game->GetPhase() == EYJJPhase::Won || Game->GetPhase() == EYJJPhase::Lost;
	if (!bEnded)
	{
		DrawCards(*Game, *Commander);
		DrawStart(*Game);
		DrawFlick(*Game);
	}
	DrawBanner(*Game);
	if (bEnded)
	{
		DrawEnd(*Game);
	}
}

void AYJJHUD::DrawStatus(const UYJJGame& Game)
{
	const float X = S(24), Y = S(24), W = S(330), H = S(118);
	Panel(X, Y, W, H, HudColor::PanelBg);

	// 푸딩 체력
	const float Ratio = static_cast<float>(Game.GetLives()) / FMath::Max(1, UYJJGame::GetMaxLives());
	Heart(X + S(38), Y + S(34), S(15), HudColor::Ink);
	Heart(X + S(38), Y + S(34), S(12), Ratio > 0.35f ? YJJShapes::Hex(0xFF5C7A) : HudColor::Red);
	Text(YJJLoc::Get(TEXT("Hud.Pudding")), X + S(66), Y + S(34), 24, HudColor::Cream);
	const float BarX = X + S(128), BarW = S(128), BarH = S(18);
	Panel(BarX - S(3), Y + S(34) - BarH * 0.5f - S(3), BarW + S(6), BarH + S(6), HudColor::Ink, 9.f);
	Panel(BarX, Y + S(34) - BarH * 0.5f, FMath::Max(BarW * Ratio, S(4)), BarH, Ratio > 0.35f ? YJJShapes::Hex(0xFFD766) : HudColor::Red, 7.f);
	Text(FString::Printf(TEXT("%d"), Game.GetLives()), X + W - S(20), Y + S(34), 26, HudColor::Cream, EAlign::Right);

	// 골드
	Coin(X + S(38), Y + S(84), S(16));
	Text(FString::Printf(TEXT("%d"), Game.GetGold()), X + S(66), Y + S(84), 32, HudColor::Gold);
}

void AYJJHUD::DrawWave(const UYJJGame& Game)
{
	const float W = S(660), H = S(92);
	const float X = Canvas->ClipX * 0.5f - W * 0.5f, Y = S(24);
	Panel(X, Y, W, H, HudColor::PanelBg);
	const int32 Shown = FMath::Clamp(Game.GetPhase() == EYJJPhase::Build ? Game.GetWave() + 1 : Game.GetWave(), 1, FMath::Max(1, UYJJGame::GetWaveCount()));
	Text(YJJLoc::Format(TEXT("Hud.Wave"), { Shown, UYJJGame::GetWaveCount() }), Canvas->ClipX * 0.5f, Y + S(30), 32, HudColor::Cream, EAlign::Center);
	if (Game.GetPhase() == EYJJPhase::Wave)
	{
		Text(YJJLoc::Format(TEXT("Hud.WaveLeft"), { Game.GetEnemiesLeft(), FText::AsNumber(UYJJGame::WaveDef(Game.GetWave()).BudgetKg).ToString() }),
			Canvas->ClipX * 0.5f, Y + S(68), 22, YJJShapes::Hex(0xFF9A9A), EAlign::Center);
	}
	else if (Game.GetPhase() == EYJJPhase::Build)
	{
		Text(Game.GetWavePreview(Game.GetWave() + 1), Canvas->ClipX * 0.5f, Y + S(68), 19, HudColor::Dim, EAlign::Center);
	}
}

void AYJJHUD::DrawCards(const UYJJGame& Game, const AYJJCommander& Commander)
{
	const float CardW = S(218), CardH = S(124), Gap = S(10);
	const float Total = CardW * YJJTowerTypeCount + Gap * (YJJTowerTypeCount - 1);
	const float X0 = Canvas->ClipX * 0.5f - Total * 0.5f;
	const float Y0 = Canvas->ClipY - CardH - S(26);

	Text(YJJLoc::Get(TEXT("Hud.Controls")), Canvas->ClipX * 0.5f, Y0 - S(22), 18, WithAlpha(HudColor::Cream, 0.85f), EAlign::Center);

	for (int32 i = 0; i < YJJTowerTypeCount; ++i)
	{
		const EYJJTowerType Type = static_cast<EYJJTowerType>(i);
		const FYJJTowerRow& Info = UYJJGame::TowerInfo(Type);
		const bool bSelected = Commander.GetSelected() == Type;
		const bool bAfford = Game.GetGold() >= Info.Cost;
		const float X = X0 + i * (CardW + Gap);
		const float Y = Y0 - (bSelected ? S(12) : 0.f);
		const bool bHover = FBox2D(FVector2D(X, Y), FVector2D(X + CardW, Y + CardH)).IsInside(Mouse);
		AddButton(X, Y, CardW, CardH, i);

		if (bSelected)
		{
			Panel(X - S(5), Y - S(5), CardW + S(10), CardH + S(10), HudColor::Gold, 18.f);
		}
		Panel(X, Y, CardW, CardH, bSelected ? FLinearColor(0.1f, 0.14f, 0.28f, 0.96f) : (bHover ? FLinearColor(0.09f, 0.12f, 0.22f, 0.9f) : HudColor::PanelBg));

		const float Fade = bAfford ? 1.f : 0.45f;
		Circle(X + S(48), Y + S(56), S(38), WithAlpha(FLinearColor(1.f, 1.f, 1.f, 0.1f), Fade));
		TowerIcon(Type, X + S(48), Y + S(56), S(28));
		if (!bAfford)
		{
			Circle(X + S(48), Y + S(56), S(38), FLinearColor(0.05f, 0.07f, 0.14f, 0.6f));
		}
		Text(UYJJGame::TowerName(Type), X + S(92), Y + S(30), 20, WithAlpha(HudColor::Cream, Fade));
		Text(UYJJGame::TowerBlurb(Type), X + S(92), Y + S(58), 14, WithAlpha(HudColor::Dim, Fade), EAlign::Left, 1.f);
		Coin(X + S(106), Y + S(92), S(11));
		Text(FString::Printf(TEXT("%d"), Info.Cost), X + S(124), Y + S(92), 24, bAfford ? HudColor::Gold : HudColor::Red);

		// 단축키
		Circle(X + S(14), Y + S(14), S(15), HudColor::Ink);
		Circle(X + S(14), Y + S(14), S(12), bSelected ? HudColor::Gold : HudColor::Cream);
		Text(FString::Printf(TEXT("%d"), i + 1), X + S(14), Y + S(14), 16, HudColor::Ink, EAlign::Center, 0.f);
	}
}

void AYJJHUD::DrawStart(const UYJJGame& Game)
{
	if (Game.GetPhase() != EYJJPhase::Build)
	{
		return;
	}
	const float Pulse = 1.f + 0.04f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * 5.f);
	const float W = S(250) * Pulse, H = S(92) * Pulse;
	const float X = Canvas->ClipX - S(40) - S(250) * 0.5f - W * 0.5f;
	const float Y = Canvas->ClipY - S(40) - S(92) * 0.5f - H * 0.5f;
	const bool bHover = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H)).IsInside(Mouse);
	AddButton(X, Y, W, H, ButtonStartWave);
	Panel(X - S(5), Y - S(5), W + S(10), H + S(10), HudColor::Ink, 22.f);
	Panel(X, Y, W, H, bHover ? YJJShapes::Hex(0xFF7A45) : YJJShapes::Hex(0xF2542D), 18.f);
	Text(YJJLoc::Get(Game.GetWave() == 0 ? TEXT("Hud.Start") : TEXT("Hud.NextWave")), X + W * 0.5f, Y + H * 0.42f, 34, HudColor::Cream, EAlign::Center, 3.f);
	Text(TEXT("Space"), X + W * 0.5f, Y + H * 0.8f, 16, WithAlpha(HudColor::Cream, 0.85f), EAlign::Center, 1.f);

	if (Game.GetWave() == 0)
	{
		// 첫 안내
		const float PW = S(760), PH = S(84);
		const float PX = Canvas->ClipX * 0.5f - PW * 0.5f, PY = S(140);
		Panel(PX, PY, PW, PH, HudColor::PanelBg);
		Text(YJJLoc::Get(TEXT("Hud.IntroTitle")), Canvas->ClipX * 0.5f, PY + S(28), 26, HudColor::Gold, EAlign::Center);
		Text(YJJLoc::Get(TEXT("Hud.IntroBody")), Canvas->ClipX * 0.5f, PY + S(60), 20, HudColor::Cream, EAlign::Center);
	}
}

void AYJJHUD::DrawFlick(const UYJJGame& Game)
{
	// 딱밤 게이지: 다 차면 적을 클릭해 튕길 수 있다
	const float X = S(70), Y = Canvas->ClipY - S(78), R = S(40);
	const float Charge = Game.GetFlickCharge();
	Circle(X, Y, R + S(5), HudColor::Ink);
	Circle(X, Y, R, FLinearColor(0.12f, 0.15f, 0.26f, 1.f));
	// 아래에서부터 차오르는 부채꼴 대신 크기로 표시
	Circle(X, Y, R * FMath::Sqrt(Charge), Charge >= 1.f ? HudColor::Gold : YJJShapes::Hex(0x8B93A7));
	Text(YJJLoc::Get(TEXT("Hud.Flick")), X, Y, 22, Charge >= 1.f ? HudColor::Ink : HudColor::Cream, EAlign::Center, Charge >= 1.f ? 0.f : 2.f);
}

void AYJJHUD::DrawWorld(const UYJJGame& Game, const AYJJCommander& Commander)
{
	// 덩치 큰 적의 체력 막대
	for (const AYJJWobbler* Enemy : Game.GetEnemies())
	{
		if (!Enemy || !Enemy->IsAlive())
		{
			continue;
		}
		const bool bHeavy = Enemy->GetWeight() >= 150.f;
		const bool bHurt = Enemy->GetHealthRatio() < 0.999f;
		if (!bHeavy && !bHurt)
		{
			continue;
		}
		const FVector Screen = Canvas->Project(Enemy->GetBodyLocation() + FVector(0, 0, 125.f * Enemy->GetScale()));
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		if (bHurt)
		{
			const float W = S(26.f + 22.f * Enemy->GetScale()), H = S(7);
			Rect(Screen.X - W * 0.5f - S(2), Screen.Y - S(2), W + S(4), H + S(4), HudColor::Ink);
			Rect(Screen.X - W * 0.5f, Screen.Y, W * Enemy->GetHealthRatio(), H, YJJShapes::Hex(0xFF5C5C));
		}
		if (bHeavy)
		{
			// 무거운 놈은 몸무게를 달고 다닌다 (잘 안 날아간다는 신호)
			Text(YJJLoc::Format(TEXT("Hud.Weight"), { FMath::RoundToInt(Enemy->GetWeight()) }), Screen.X, Screen.Y - S(16), 17, YJJShapes::Hex(0xFFD6A5), EAlign::Center, 2.f);
		}
	}

	// 떠오르는 글자
	for (const FYJJFloater& Floater : Game.GetFloaters())
	{
		const FVector Screen = Canvas->Project(Floater.Location);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		const float T = Floater.Age / 1.2f;
		const float Pop = 1.f + 0.5f * FMath::Exp(-Floater.Age * 14.f);
		Text(Floater.Text, Screen.X, Screen.Y - S(70.f) * (1.f - FMath::Square(1.f - T)), Floater.Size * Pop, WithAlpha(Floater.Color, FMath::Clamp((1.f - T) * 3.f, 0.f, 1.f)), EAlign::Center, 3.f);
	}

	// 발판 안내
	const int32 Pad = Commander.GetHoverPad();
	const TArray<YJJMap::FPad>& Pads = YJJMap::Pads();
	if (Pads.IsValidIndex(Pad))
	{
		const FVector Screen = Canvas->Project(Pads[Pad].Location + FVector(0, 0, 330));
		if (Screen.Z > 0.f)
		{
			FString Line;
			FLinearColor Color = HudColor::Cream;
			if (Game.HasTower(Pad))
			{
				const EYJJTowerType Type = Game.GetPadTower(Pad);
				const FString Name = UYJJGame::TowerName(Type);
				const int32 Level = Game.GetPadLevel(Pad);
				FString Extra;
				if (Type == EYJJTowerType::Sling)
				{
					Extra = YJJLoc::Format(TEXT("Hud.SlingLimit"), { FMath::RoundToInt(UYJJGame::SlingLimitKg(Level)) });
				}
				if (Level < YJJMaxTowerLevel)
				{
					const int32 Cost = UYJJGame::UpgradeCost(Type, Level);
					Line = YJJLoc::Format(TEXT("Hud.PadUpgrade"), { Name, Level, Extra, Cost, UYJJGame::UpgradeText(Type, Level), Game.SellValue(Pad) });
					Color = Game.GetGold() >= Cost ? HudColor::Gold : HudColor::Cream;
				}
				else
				{
					Line = YJJLoc::Format(TEXT("Hud.PadMax"), { Name, Level, Extra, Game.SellValue(Pad) });
				}
			}
			else
			{
				const FYJJTowerRow& Info = UYJJGame::TowerInfo(Commander.GetSelected());
				const bool bAfford = Game.GetGold() >= Info.Cost;
				Line = YJJLoc::Format(TEXT("Hud.PadBuild"), { UYJJGame::TowerName(Commander.GetSelected()), Info.Cost });
				Color = bAfford ? HudColor::Gold : HudColor::Red;
			}
			const FVector2D Size = Measure(Line, 22);
			Panel(Screen.X - Size.X * 0.5f - S(14), Screen.Y - S(20), Size.X + S(28), S(40), HudColor::PanelBg, 12.f);
			Text(Line, Screen.X, Screen.Y, 22, Color, EAlign::Center);
		}
	}
}

void AYJJHUD::DrawBanner(const UYJJGame& Game)
{
	const float Age = Game.GetBannerAge();
	const float Life = Game.GetBannerLife();
	if (Game.GetBannerText().IsEmpty() || Age >= Life)
	{
		return;
	}
	const float Alpha = FMath::Clamp((Life - Age) / 0.4f, 0.f, 1.f);
	const float Pop = 1.f + 0.7f * FMath::Exp(-Age * 10.f);
	Text(Game.GetBannerText(), Canvas->ClipX * 0.5f, Canvas->ClipY * 0.3f, 76.f * Pop, WithAlpha(Game.GetBannerColor(), Alpha), EAlign::Center, 6.f);
}

void AYJJHUD::DrawEnd(const UYJJGame& Game)
{
	const bool bWon = Game.GetPhase() == EYJJPhase::Won;
	const float Appear = FMath::Clamp((Game.GetEndAge() - 0.6f) / 0.5f, 0.f, 1.f);
	if (Appear <= 0.f)
	{
		return;
	}
	Rect(0, 0, Canvas->ClipX, Canvas->ClipY, FLinearColor(0.02f, 0.03f, 0.08f, 0.5f * Appear));
	const float W = S(640), H = S(460);
	const float X = Canvas->ClipX * 0.5f - W * 0.5f;
	const float Y = Canvas->ClipY * 0.5f - H * 0.5f + S(40) * (1.f - Appear);
	Panel(X - S(6), Y - S(6), W + S(12), H + S(12), WithAlpha(bWon ? HudColor::Gold : HudColor::Red, Appear), 26.f);
	Panel(X, Y, W, H, WithAlpha(FLinearColor(0.06f, 0.08f, 0.16f, 0.97f), Appear), 22.f);
	const float CX = Canvas->ClipX * 0.5f;
	Text(YJJLoc::Get(bWon ? TEXT("Hud.Won") : TEXT("Hud.Lost")), CX, Y + S(70), 54, WithAlpha(bWon ? HudColor::Gold : HudColor::Red, Appear), EAlign::Center, 4.f);
	Text(YJJLoc::Format(TEXT("Hud.Wave"), { Game.GetWave(), UYJJGame::GetWaveCount() }), CX, Y + S(132), 26, WithAlpha(HudColor::Dim, Appear), EAlign::Center);

	const FString Lines[4][2] = {
		{ YJJLoc::Get(TEXT("Hud.StatKills")), FString::FromInt(Game.GetKills()) },
		{ YJJLoc::Get(TEXT("Hud.StatDrowned")), FString::FromInt(Game.GetDrowned()) },
		{ YJJLoc::Get(TEXT("Hud.StatCombo")), FString::Printf(TEXT("x%d"), Game.GetBestCombo()) },
		{ YJJLoc::Get(TEXT("Hud.StatLeaked")), FString::FromInt(Game.GetLeaked()) } };
	for (int32 i = 0; i < 4; ++i)
	{
		const float LineY = Y + S(190) + i * S(42);
		Text(Lines[i][0], X + S(110), LineY, 26, WithAlpha(HudColor::Cream, Appear));
		Text(Lines[i][1], X + W - S(110), LineY, 30, WithAlpha(HudColor::Gold, Appear), EAlign::Right);
	}

	const float BW = S(280), BH = S(70);
	const float BX = CX - BW * 0.5f, BY = Y + H - BH - S(30);
	const bool bHover = FBox2D(FVector2D(BX, BY), FVector2D(BX + BW, BY + BH)).IsInside(Mouse);
	AddButton(BX, BY, BW, BH, ButtonRestart);
	Panel(BX, BY, BW, BH, WithAlpha(bHover ? YJJShapes::Hex(0xFF7A45) : YJJShapes::Hex(0xF2542D), Appear), 16.f);
	Text(YJJLoc::Get(TEXT("Hud.Restart")), CX, BY + BH * 0.5f, 30, WithAlpha(HudColor::Cream, Appear), EAlign::Center, 3.f);
}
