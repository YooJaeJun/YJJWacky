#include "UI/YJJHUD.h"

#include "CanvasItem.h"
#include "Data/YJJLoc.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Fonts/CompositeFont.h"
#include "Fonts/FontMeasure.h"
#include "Gameplay/YJJBoulder.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"
#include "Player/YJJCommander.h"
#include "Rendering/SlateRenderer.h"
#include "TextureResource.h"
#include "World/YJJMap.h"
#include "YJJAssetPaths.h"

const UFont* AYJJHUD::BaseFont()
{
	return LoadObject<UFont>(nullptr, BaseFontPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

FLinearColor AYJJHUD::WithAlpha(FLinearColor Color, float Alpha)
{
	Color.A *= Alpha;
	return Color;
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
	if (!Base || Str.IsEmpty() || Color.A <= MinVisibleAlpha)
	{
		return;
	}
	const int32 FontSize = FMath::Max(FMath::RoundToInt(S(Size)), MinFontSize);
	const TSharedPtr<const FCompositeFont> Composite = GetGameFont();
	FSlateFontInfo Font = Composite.IsValid() ? FSlateFontInfo(Composite, FontSize) : FSlateFontInfo(Base, FontSize);
	if (Outline > 0.f)
	{
		const FLinearColor Ink = C(TEXT("HudInk"));
		Font.OutlineSettings.OutlineSize = FMath::Max(FMath::RoundToInt(S(Outline)), 1);
		Font.OutlineSettings.OutlineColor = FLinearColor(Ink.R, Ink.G, Ink.B, Color.A);
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
	const int32 FontSize = FMath::Max(FMath::RoundToInt(S(Size)), MinFontSize);
	FSlateFontInfo Font = Composite.IsValid() ? FSlateFontInfo(Composite, FontSize) : FSlateFontInfo(Base, FontSize);
	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
	{
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Str, Font);
	}
	return FVector2D(Str.Len() * S(Size) * GlyphWidthGuess, S(Size));
}

void AYJJHUD::Panel(float X, float Y, float W, float H, const FLinearColor& Color, float Corner)
{
	// 둥근 모서리 패널: 겹쳐 그리면 반투명이 진해지므로 테두리 다각형 하나를 부채꼴로 채운다
	const float R = FMath::Min(S(Corner), FMath::Min(W, H) * 0.5f);
	TArray<FVector2D> Outline;
	const FVector2D Corners[] = { FVector2D(X + W - R, Y + H - R), FVector2D(X + R, Y + H - R), FVector2D(X + R, Y + R), FVector2D(X + W - R, Y + R) };
	for (int32 c = 0; c < UE_ARRAY_COUNT(Corners); ++c)
	{
		for (int32 k = 0; k <= CornerSteps; ++k)
		{
			const float A = FMath::DegreesToRadians(c * QuarterTurn + k * QuarterTurn / CornerSteps);
			Outline.Add(Corners[c] + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
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
	// 원 두 개 + 아래 삼각형
	Circle(X - R * HeartLobeX, Y - R * HeartLobeY, R * HeartLobeR, Color, HeartSides);
	Circle(X + R * HeartLobeX, Y - R * HeartLobeY, R * HeartLobeR, Color, HeartSides);
	TArray<FCanvasUVTri> Tris;
	FCanvasUVTri& T = Tris.AddDefaulted_GetRef();
	T.V0_Pos = FVector2D(X - R * HeartWide, Y);
	T.V1_Pos = FVector2D(X + R * HeartWide, Y);
	T.V2_Pos = FVector2D(X, Y + R * HeartTip);
	T.V0_Color = T.V1_Color = T.V2_Color = Color;
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void AYJJHUD::Pie(float X, float Y, float R, float Fraction, const FLinearColor& Color)
{
	const int32 Sides = FMath::Max(1, FMath::CeilToInt(L(TEXT("Spell.PieSides")) * FMath::Clamp(Fraction, 0.f, 1.f)));
	if (Fraction <= 0.f)
	{
		return;
	}
	TArray<FCanvasUVTri> Tris;
	const float Start = FMath::DegreesToRadians(StarStartDeg);
	const float Sweep = TWO_PI * FMath::Clamp(Fraction, 0.f, 1.f);
	for (int32 i = 0; i < Sides; ++i)
	{
		const float A0 = Start + Sweep * i / Sides;
		const float A1 = Start + Sweep * (i + 1) / Sides;
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

void AYJJHUD::Star(float X, float Y, float R, const FLinearColor& Color)
{
	// 바깥 꼭짓점과 안쪽 오목점을 번갈아 이어 부채꼴로 채운다
	const float Inner = R * L(TEXT("Stars.Inner"));
	TArray<FCanvasUVTri> Tris;
	const int32 Corners = StarPoints * 2;
	for (int32 i = 0; i < Corners; ++i)
	{
		const float A0 = FMath::DegreesToRadians(StarStartDeg) + PI * i / StarPoints;
		const float A1 = FMath::DegreesToRadians(StarStartDeg) + PI * (i + 1) / StarPoints;
		const float R0 = (i % 2) ? Inner : R;
		const float R1 = (i % 2) ? R : Inner;
		FCanvasUVTri& T = Tris.AddDefaulted_GetRef();
		T.V0_Pos = FVector2D(X, Y);
		T.V1_Pos = FVector2D(X + FMath::Cos(A0) * R0, Y + FMath::Sin(A0) * R0);
		T.V2_Pos = FVector2D(X + FMath::Cos(A1) * R1, Y + FMath::Sin(A1) * R1);
		T.V0_Color = T.V1_Color = T.V2_Color = Color;
	}
	FCanvasTriangleItem Item(Tris, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void AYJJHUD::GroundRing(const FVector& Center, float Radius, const FLinearColor& Color)
{
	const int32 Sides = FMath::Max(3, FMath::RoundToInt(L(TEXT("Aim.Sides"))));
	const float Thickness = SL(TEXT("Aim.Thickness"));
	FVector Prev = Canvas->Project(Center + FVector(Radius, 0.f, 0.f));
	for (int32 i = 1; i <= Sides; ++i)
	{
		const float A = TWO_PI * i / Sides;
		const FVector Next = Canvas->Project(Center + FVector(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, 0.f));
		if (Prev.Z > 0.f && Next.Z > 0.f)
		{
			Canvas->K2_DrawLine(FVector2D(Prev.X, Prev.Y), FVector2D(Next.X, Next.Y), Thickness, Color);
		}
		Prev = Next;
	}
}

void AYJJHUD::DrawIcon(const FString& Icon, float X, float Y, float R)
{
	// HudIcons.csv: 조각마다 원 또는 사각형 (위치·크기는 R 의 배수)
	for (const FYJJIconRow* Piece : YJJData::Icon(Icon))
	{
		const FLinearColor Color = C(*Piece->Color);
		const int32 Sides = Piece->Sides > 0 ? Piece->Sides : CircleSidesDefault;
		const int32 Mirrors = Piece->Mirror ? 2 : 1;
		for (int32 m = 0; m < Mirrors; ++m)
		{
			const float Flip = m == 0 ? 1.f : -1.f;
			for (int32 k = 0; k < FMath::Max(1, Piece->Repeat); ++k)
			{
				const float Angle = FMath::DegreesToRadians(Piece->OrbitStart + k * Piece->OrbitStep);
				const float PX = X + R * (Piece->X * Flip + FMath::Cos(Angle) * Piece->Orbit);
				const float PY = Y + R * (Piece->Y + FMath::Sin(Angle) * Piece->Orbit);
				if (Piece->Shape.Equals(TEXT("Rect"), ESearchCase::IgnoreCase))
				{
					// 대칭이면 사각형은 왼쪽 위가 아니라 반대편 끝에서 시작한다
					const float W = R * Piece->A;
					Rect(m == 0 ? PX : PX - W, PY, W, R * Piece->B, Color);
				}
				else
				{
					Circle(PX, PY, R * Piece->A, Color, Sides);
				}
			}
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
	UIScale = FMath::Clamp(Canvas->ClipY / L(TEXT("Hud.ReferenceHeight")), L(TEXT("Hud.ScaleMin")), L(TEXT("Hud.ScaleMax")));
	Buttons.Reset();

	const AYJJCommander* Commander = Cast<AYJJCommander>(GetOwningPlayerController());
	const UYJJGame* Game = UYJJGame::Get(this);
	if (!Commander || !Game || Game->IsPhotoMode())
	{
		return;
	}
	float MouseX = 0.f, MouseY = 0.f;
	Mouse = Commander->GetMousePosition(MouseX, MouseY) ? FVector2D(MouseX, MouseY) : FVector2D(OffscreenMouse, OffscreenMouse);

	DrawWorld(*Game, *Commander);
	DrawStatus(*Game);
	DrawWave(*Game);
	const bool bEnded = Game->GetPhase() == EYJJPhase::Won || Game->GetPhase() == EYJJPhase::Lost;
	if (!bEnded)
	{
		DrawCards(*Game, *Commander);
		DrawStart(*Game);
		DrawFlick(*Game);
		DrawSpells(*Game, *Commander);
		DrawAim(*Game, *Commander);
	}
	DrawBanner(*Game);
	if (bEnded)
	{
		DrawEnd(*Game);
	}
}

void AYJJHUD::DrawStatus(const UYJJGame& Game)
{
	const float X = SL(TEXT("Status.X")), Y = SL(TEXT("Status.Y")), W = SL(TEXT("Status.W")), H = SL(TEXT("Status.H"));
	Panel(X, Y, W, H, C(TEXT("HudPanel")));

	// 푸딩 체력
	const float Ratio = static_cast<float>(Game.GetLives()) / FMath::Max(1, UYJJGame::GetMaxLives());
	const bool bLow = Ratio <= L(TEXT("Status.LowRatio"));
	const float RowY = Y + SL(TEXT("Status.RowY"));
	const float IconX = X + SL(TEXT("Status.IconX"));
	Heart(IconX, RowY, SL(TEXT("Status.HeartOuter")), C(TEXT("HudInk")));
	Heart(IconX, RowY, SL(TEXT("Status.HeartInner")), C(bLow ? TEXT("HudRed") : TEXT("HudHeart")));
	Text(YJJLoc::Get(TEXT("Hud.Pudding")), X + SL(TEXT("Status.LabelX")), RowY, L(TEXT("Status.LabelSize")), C(TEXT("HudCream")));
	const float BarX = X + SL(TEXT("Status.BarX")), BarW = SL(TEXT("Status.BarW")), BarH = SL(TEXT("Status.BarH")), Border = SL(TEXT("Status.BarBorder"));
	Panel(BarX - Border, RowY - BarH * 0.5f - Border, BarW + Border * 2.f, BarH + Border * 2.f, C(TEXT("HudInk")), L(TEXT("Status.BarCorner")));
	Panel(BarX, RowY - BarH * 0.5f, FMath::Max(BarW * Ratio, SL(TEXT("Status.BarMin"))), BarH, C(bLow ? TEXT("HudRed") : TEXT("HudBarFill")), L(TEXT("Status.BarFillCorner")));
	Text(FString::FromInt(Game.GetLives()), X + W - SL(TEXT("Status.LivesRight")), RowY, L(TEXT("Status.LivesSize")), C(TEXT("HudCream")), EAlign::Right);

	// 골드
	const float GoldY = Y + SL(TEXT("Status.GoldY"));
	DrawIcon(TEXT("Coin"), IconX, GoldY, SL(TEXT("Status.CoinR")));
	Text(FString::FromInt(Game.GetGold()), X + SL(TEXT("Status.LabelX")), GoldY, L(TEXT("Status.GoldSize")), C(TEXT("HudGold")));
}

void AYJJHUD::DrawWave(const UYJJGame& Game)
{
	const float W = SL(TEXT("Wave.W")), H = SL(TEXT("Wave.H"));
	const float CX = Canvas->ClipX * 0.5f;
	const float X = CX - W * 0.5f, Y = SL(TEXT("Wave.Y"));
	Panel(X, Y, W, H, C(TEXT("HudPanel")));
	const int32 Shown = FMath::Clamp(Game.GetPhase() == EYJJPhase::Build ? Game.GetWave() + 1 : Game.GetWave(), 1, FMath::Max(1, UYJJGame::GetWaveCount()));
	Text(YJJLoc::Format(TEXT("Hud.Wave"), { Shown, UYJJGame::GetWaveCount() }), CX, Y + SL(TEXT("Wave.TitleY")), L(TEXT("Wave.TitleSize")), C(TEXT("HudCream")), EAlign::Center);
	if (Game.GetPhase() == EYJJPhase::Wave)
	{
		Text(YJJLoc::Format(TEXT("Hud.WaveLeft"), { Game.GetEnemiesLeft(), FText::AsNumber(UYJJGame::WaveDef(Game.GetWave()).BudgetKg).ToString() }),
			CX, Y + SL(TEXT("Wave.SubY")), L(TEXT("Wave.LeftSize")), C(TEXT("HudWaveLeft")), EAlign::Center);
	}
	else if (Game.GetPhase() == EYJJPhase::Build)
	{
		Text(Game.GetWavePreview(Game.GetWave() + 1), CX, Y + SL(TEXT("Wave.SubY")), L(TEXT("Wave.PreviewSize")), C(TEXT("HudDim")), EAlign::Center);
	}
}

void AYJJHUD::DrawCards(const UYJJGame& Game, const AYJJCommander& Commander)
{
	const float CardW = SL(TEXT("Card.W")), CardH = SL(TEXT("Card.H")), Gap = SL(TEXT("Card.Gap"));
	const float Total = CardW * YJJTowerTypeCount + Gap * (YJJTowerTypeCount - 1);
	const float X0 = Canvas->ClipX * 0.5f - Total * 0.5f;
	const float Y0 = Canvas->ClipY - CardH - SL(TEXT("Card.Bottom"));

	const bool bAiming = Commander.GetPendingSpell() != INDEX_NONE;
	const FString Help = bAiming ? YJJLoc::Format(TEXT("Hud.SpellAim"), { UYJJGame::SpellName(static_cast<EYJJSpell>(Commander.GetPendingSpell())) })
		: YJJLoc::Get(TEXT("Hud.Controls"));
	Text(Help, Canvas->ClipX * 0.5f, Y0 - SL(TEXT("Card.HelpY")), L(TEXT("Card.HelpSize")),
		bAiming ? C(TEXT("HudGold")) : WithAlpha(C(TEXT("HudCream")), L(TEXT("Card.HelpAlpha"))), EAlign::Center);

	const float IconX = SL(TEXT("Card.IconX")), IconY = SL(TEXT("Card.IconY")), IconBack = SL(TEXT("Card.IconBack"));
	const float TextX = SL(TEXT("Card.TextX")), Key = SL(TEXT("Card.KeyXY"));
	for (int32 i = 0; i < YJJTowerTypeCount; ++i)
	{
		const EYJJTowerType Type = static_cast<EYJJTowerType>(i);
		const FYJJTowerRow& Info = UYJJGame::TowerInfo(Type);
		const bool bSelected = Commander.GetSelected() == Type;
		const bool bAfford = Game.GetGold() >= Info.Cost;
		const float X = X0 + i * (CardW + Gap);
		const float Y = Y0 - (bSelected ? SL(TEXT("Card.Lift")) : 0.f);
		const bool bHover = FBox2D(FVector2D(X, Y), FVector2D(X + CardW, Y + CardH)).IsInside(Mouse);
		AddButton(X, Y, CardW, CardH, i);

		if (bSelected)
		{
			const float Border = SL(TEXT("Card.Border"));
			Panel(X - Border, Y - Border, CardW + Border * 2.f, CardH + Border * 2.f, C(TEXT("HudGold")), L(TEXT("Card.BorderCorner")));
		}
		Panel(X, Y, CardW, CardH, C(bSelected ? TEXT("HudCardSelected") : (bHover ? TEXT("HudCardHover") : TEXT("HudPanel"))));

		const float Fade = bAfford ? 1.f : L(TEXT("Card.Dim"));
		Circle(X + IconX, Y + IconY, IconBack, WithAlpha(C(TEXT("HudIconBack")), Fade));
		DrawIcon(StaticEnum<EYJJTowerType>()->GetNameStringByValue(i), X + IconX, Y + IconY, SL(TEXT("Card.IconR")));
		if (!bAfford)
		{
			Circle(X + IconX, Y + IconY, IconBack, C(TEXT("HudIconShade")));
		}
		Text(UYJJGame::TowerName(Type), X + TextX, Y + SL(TEXT("Card.NameY")), L(TEXT("Card.NameSize")), WithAlpha(C(TEXT("HudCream")), Fade));
		Text(UYJJGame::TowerBlurb(Type), X + TextX, Y + SL(TEXT("Card.BlurbY")), L(TEXT("Card.BlurbSize")), WithAlpha(C(TEXT("HudDim")), Fade), EAlign::Left,
			L(TEXT("Card.BlurbOutline")));
		const float CostY = Y + SL(TEXT("Card.CoinY"));
		DrawIcon(TEXT("Coin"), X + SL(TEXT("Card.CoinX")), CostY, SL(TEXT("Card.CoinR")));
		Text(FString::FromInt(Info.Cost), X + SL(TEXT("Card.CostX")), CostY, L(TEXT("Card.CostSize")), C(bAfford ? TEXT("HudGold") : TEXT("HudRed")));

		// 단축키
		Circle(X + Key, Y + Key, SL(TEXT("Card.KeyOuter")), C(TEXT("HudInk")));
		Circle(X + Key, Y + Key, SL(TEXT("Card.KeyInner")), C(bSelected ? TEXT("HudGold") : TEXT("HudCream")));
		Text(FString::FromInt(i + 1), X + Key, Y + Key, L(TEXT("Card.KeySize")), C(TEXT("HudInk")), EAlign::Center, 0.f);
	}
}

void AYJJHUD::DrawStart(const UYJJGame& Game)
{
	if (!Game.CanCallWave())
	{
		return;
	}
	const float Pulse = 1.f + L(TEXT("Start.PulseAmount")) * FMath::Sin(GetWorld()->GetRealTimeSeconds() * L(TEXT("Start.PulseSpeed")));
	const float BaseW = SL(TEXT("Start.W")), BaseH = SL(TEXT("Start.H")), Margin = SL(TEXT("Start.Margin"));
	const float W = BaseW * Pulse, H = BaseH * Pulse;
	const float X = Canvas->ClipX - Margin - BaseW * 0.5f - W * 0.5f;
	const float Y = Canvas->ClipY - Margin - BaseH * 0.5f - H * 0.5f;
	const bool bHover = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H)).IsInside(Mouse);
	AddButton(X, Y, W, H, ButtonStartWave);
	const float Border = SL(TEXT("Start.Border"));
	Panel(X - Border, Y - Border, W + Border * 2.f, H + Border * 2.f, C(TEXT("HudInk")), L(TEXT("Start.BorderCorner")));
	Panel(X, Y, W, H, C(bHover ? TEXT("HudButtonHover") : TEXT("HudButton")), L(TEXT("Start.Corner")));
	Text(YJJLoc::Get(Game.GetWave() == 0 ? TEXT("Hud.Start") : TEXT("Hud.NextWave")), X + W * 0.5f, Y + H * L(TEXT("Start.LabelY")), L(TEXT("Start.LabelSize")),
		C(TEXT("HudCream")), EAlign::Center, L(TEXT("Start.LabelOutline")));
	const int32 Early = Game.GetEarlyBonus();
	Text(Early > 0 ? YJJLoc::Format(TEXT("Hud.CallBonus"), { YJJLoc::Get(TEXT("Hud.StartKey")), Early }) : YJJLoc::Get(TEXT("Hud.StartKey")),
		X + W * 0.5f, Y + H * L(TEXT("Start.KeyY")), L(TEXT("Start.KeySize")), Early > 0 ? C(TEXT("HudGold")) : WithAlpha(C(TEXT("HudCream")), L(TEXT("Start.KeyAlpha"))),
		EAlign::Center, L(TEXT("Card.BlurbOutline")));
	if (Game.GetWave() > 0 && Game.GetNextWaveTimer() > 0.f)
	{
		// 남은 시간: 버튼 위 글자 + 버튼 아래 줄어드는 막대
		const float Window = FMath::Max(YJJData::Tune(Game.GetPhase() == EYJJPhase::Build ? TEXT("Wave.BuildTime") : TEXT("Wave.CallWindow")), Game.GetNextWaveTimer());
		Text(YJJLoc::Format(TEXT("Hud.AutoWave"), { FMath::CeilToInt(Game.GetNextWaveTimer()) }), X + W * 0.5f, Y - SL(TEXT("Start.TimerY")), L(TEXT("Start.TimerSize")),
			C(TEXT("HudCream")), EAlign::Center);
		const float BarH = SL(TEXT("Start.TimerBarH"));
		Rect(X, Y + H + Border, W, BarH, C(TEXT("HudInk")));
		Rect(X, Y + H + Border, W * FMath::Clamp(Game.GetNextWaveTimer() / Window, 0.f, 1.f), BarH, C(TEXT("HudGold")));
	}

	if (Game.GetWave() == 0)
	{
		// 첫 안내
		const float PW = SL(TEXT("Intro.W")), PH = SL(TEXT("Intro.H"));
		const float CX = Canvas->ClipX * 0.5f;
		const float PY = SL(TEXT("Intro.Y"));
		Panel(CX - PW * 0.5f, PY, PW, PH, C(TEXT("HudPanel")));
		Text(YJJLoc::Get(TEXT("Hud.IntroTitle")), CX, PY + SL(TEXT("Intro.TitleY")), L(TEXT("Intro.TitleSize")), C(TEXT("HudGold")), EAlign::Center);
		Text(YJJLoc::Get(TEXT("Hud.IntroBody")), CX, PY + SL(TEXT("Intro.BodyY")), L(TEXT("Intro.BodySize")), C(TEXT("HudCream")), EAlign::Center);
	}
}

void AYJJHUD::DrawFlick(const UYJJGame& Game)
{
	// 딱밤 게이지: 다 차면 적을 클릭해 튕길 수 있다
	const float X = SL(TEXT("Flick.X")), Y = Canvas->ClipY - SL(TEXT("Flick.Bottom")), R = SL(TEXT("Flick.R"));
	const float Charge = Game.GetFlickCharge();
	const bool bReady = Charge >= 1.f;
	Circle(X, Y, R + SL(TEXT("Flick.Border")), C(TEXT("HudInk")));
	Circle(X, Y, R, C(TEXT("HudFlickBack")));
	// 차오르는 정도를 원 크기로
	Circle(X, Y, R * FMath::Sqrt(Charge), C(bReady ? TEXT("HudGold") : TEXT("HudFlickCharge")));
	Text(YJJLoc::Get(TEXT("Hud.Flick")), X, Y, L(TEXT("Flick.Size")), C(bReady ? TEXT("HudInk") : TEXT("HudCream")), EAlign::Center, bReady ? 0.f : OutlineDefault);
}

void AYJJHUD::DrawSpells(const UYJJGame& Game, const AYJJCommander& Commander)
{
	// 딱밤 옆에 주문 버튼: 아이콘 위로 남은 쿨타임만큼 그늘이 걷힌다
	const float Y = Canvas->ClipY - SL(TEXT("Flick.Bottom")), R = SL(TEXT("Spell.R"));
	const TCHAR* Keys[YJJSpellCount] = { TEXT("Hud.SpellKey0"), TEXT("Hud.SpellKey1") };
	for (int32 i = 0; i < YJJSpellCount; ++i)
	{
		const EYJJSpell Spell = static_cast<EYJJSpell>(i);
		const float X = SL(TEXT("Spell.X")) + i * SL(TEXT("Spell.Step"));
		const float Charge = Game.GetSpellCharge(Spell);
		const bool bReady = Game.CanCast(Spell);
		const bool bPending = Commander.GetPendingSpell() == i;
		const bool bHover = FVector2D::DistSquared(Mouse, FVector2D(X, Y)) < R * R;
		AddButton(X - R, Y - R, R * 2.f, R * 2.f, ButtonSpell + i);

		if (bPending)
		{
			Circle(X, Y, R + SL(TEXT("Spell.Selected")), C(TEXT("HudGold")));
		}
		Circle(X, Y, R + SL(TEXT("Spell.Border")), C(TEXT("HudInk")));
		Circle(X, Y, R, C(bHover ? TEXT("HudCardHover") : TEXT("HudFlickBack")));
		DrawIcon(StaticEnum<EYJJSpell>()->GetNameStringByValue(i), X, Y, SL(TEXT("Spell.IconR")));
		if (!bReady)
		{
			// 아직 안 찬 만큼 그늘
			Pie(X, Y, R, 1.f - Charge, C(TEXT("HudSpellShade")));
		}
		Text(UYJJGame::SpellName(Spell), X, Y + SL(TEXT("Spell.NameY")), L(TEXT("Spell.NameSize")), C(bReady ? TEXT("HudCream") : TEXT("HudDim")), EAlign::Center);

		// 단축키
		const float KX = X - SL(TEXT("Spell.KeyXY")), KY = Y - SL(TEXT("Spell.KeyXY"));
		Circle(KX, KY, SL(TEXT("Spell.KeyOuter")), C(TEXT("HudInk")));
		Circle(KX, KY, SL(TEXT("Spell.KeyInner")), C(bPending ? TEXT("HudGold") : TEXT("HudCream")));
		Text(YJJLoc::Get(Keys[i]), KX, KY, L(TEXT("Spell.KeySize")), C(TEXT("HudInk")), EAlign::Center, 0.f);
	}
}

void AYJJHUD::DrawAim(const UYJJGame& Game, const AYJJCommander& Commander)
{
	// 떨어지는 중인 바위 그림자
	for (TActorIterator<AYJJBoulder> It(GetWorld()); It; ++It)
	{
		if (It->IsFalling())
		{
			GroundRing(It->GetTarget(), UYJJGame::SpellTune(EYJJSpell::Boulder, TEXT("SlamRadius")), C(TEXT("HudAimFalling")));
		}
	}
	// 고른 주문이 닿는 범위
	FVector Aim;
	const int32 Spell = Commander.GetPendingSpell();
	if (Spell != INDEX_NONE && Commander.GetAim(Aim))
	{
		const float Radius = UYJJGame::SpellTune(static_cast<EYJJSpell>(Spell), TEXT("AimRadius"));
		GroundRing(FVector(Aim.X, Aim.Y, 0.f), Radius, C(TEXT("HudAim")));
		GroundRing(FVector(Aim.X, Aim.Y, 0.f), Radius * L(TEXT("Aim.Inner")), C(TEXT("HudAim")));
	}
}

void AYJJHUD::DrawWorld(const UYJJGame& Game, const AYJJCommander& Commander)
{
	// 덩치 큰 적은 몸무게를, 다친 적은 체력 막대를 달고 다닌다
	for (const AYJJWobbler* Enemy : Game.GetEnemies())
	{
		if (!Enemy || !Enemy->IsAlive())
		{
			continue;
		}
		const bool bHeavy = Enemy->GetWeight() >= L(TEXT("World.HeavyKg"));
		const bool bHurt = Enemy->GetHealthRatio() < 1.f;
		if (!bHeavy && !bHurt)
		{
			continue;
		}
		const FVector Screen = Canvas->Project(Enemy->GetBodyLocation() + FVector(0, 0, L(TEXT("World.BarHeightPerScale")) * Enemy->GetScale()));
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		if (bHurt)
		{
			const float W = S(L(TEXT("World.BarW")) + L(TEXT("World.BarWPerScale")) * Enemy->GetScale()), H = SL(TEXT("World.BarH")), Border = SL(TEXT("World.BarBorder"));
			Rect(Screen.X - W * 0.5f - Border, Screen.Y - Border, W + Border * 2.f, H + Border * 2.f, C(TEXT("HudInk")));
			Rect(Screen.X - W * 0.5f, Screen.Y, W * Enemy->GetHealthRatio(), H, C(TEXT("HudHealth")));
		}
		if (bHeavy)
		{
			// 잘 안 날아간다는 신호
			Text(YJJLoc::Format(TEXT("Hud.Weight"), { FMath::RoundToInt(Enemy->GetWeight()) }), Screen.X, Screen.Y - SL(TEXT("World.WeightY")), L(TEXT("World.WeightSize")),
				C(TEXT("HudWeight")), EAlign::Center);
		}
	}

	// 떠오르는 글자: 톡 커졌다가 떠오르며 사라진다
	const float Life = FMath::Max(YJJData::Tune(TEXT("Hud.FloaterLife")), KINDA_SMALL_NUMBER);
	for (const FYJJFloater& Floater : Game.GetFloaters())
	{
		const FVector Screen = Canvas->Project(Floater.Location);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		const float T = Floater.Age / Life;
		const float Pop = 1.f + L(TEXT("Floater.PopAmount")) * FMath::Exp(-Floater.Age * L(TEXT("Floater.PopSpeed")));
		const float Rise = SL(TEXT("Floater.Rise")) * (1.f - FMath::Square(1.f - T));
		Text(Floater.Text, Screen.X, Screen.Y - Rise, Floater.Size * Pop, WithAlpha(Floater.Color, FMath::Clamp((1.f - T) * L(TEXT("Floater.FadeSpeed")), 0.f, 1.f)),
			EAlign::Center, L(TEXT("Floater.Outline")));
	}

	// 발판 안내
	const int32 Pad = Commander.GetHoverPad();
	const TArray<YJJMap::FPad>& Pads = YJJMap::Pads();
	if (Pads.IsValidIndex(Pad))
	{
		const FVector Screen = Canvas->Project(Pads[Pad].Location + FVector(0, 0, L(TEXT("Pad.LabelHeight"))));
		if (Screen.Z > 0.f)
		{
			FString Line;
			FLinearColor Color = C(TEXT("HudCream"));
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
					Color = C(Game.GetGold() >= Cost ? TEXT("HudGold") : TEXT("HudCream"));
				}
				else
				{
					Line = YJJLoc::Format(TEXT("Hud.PadMax"), { Name, Level, Extra, Game.SellValue(Pad) });
				}
			}
			else
			{
				const FYJJTowerRow& Info = UYJJGame::TowerInfo(Commander.GetSelected());
				Line = YJJLoc::Format(TEXT("Hud.PadBuild"), { UYJJGame::TowerName(Commander.GetSelected()), Info.Cost });
				Color = C(Game.GetGold() >= Info.Cost ? TEXT("HudGold") : TEXT("HudRed"));
			}
			const float Size = L(TEXT("Pad.LabelSize"));
			const FVector2D Measured = Measure(Line, Size);
			const float PadX = SL(TEXT("Pad.PanelPadX")), PanelH = SL(TEXT("Pad.PanelH"));
			Panel(Screen.X - Measured.X * 0.5f - PadX, Screen.Y - PanelH * 0.5f, Measured.X + PadX * 2.f, PanelH, C(TEXT("HudPanel")), L(TEXT("Pad.PanelCorner")));
			Text(Line, Screen.X, Screen.Y, Size, Color, EAlign::Center);
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
	const float Alpha = FMath::Clamp((Life - Age) / L(TEXT("Banner.Fade")), 0.f, 1.f);
	const float Pop = 1.f + L(TEXT("Banner.PopAmount")) * FMath::Exp(-Age * L(TEXT("Banner.PopSpeed")));
	Text(Game.GetBannerText(), Canvas->ClipX * 0.5f, Canvas->ClipY * L(TEXT("Banner.Y")), Game.GetBannerSize() * Pop, WithAlpha(Game.GetBannerColor(), Alpha),
		EAlign::Center, L(TEXT("Banner.Outline")));
}

void AYJJHUD::DrawEnd(const UYJJGame& Game)
{
	const bool bWon = Game.GetPhase() == EYJJPhase::Won;
	const float Appear = FMath::Clamp((Game.GetEndAge() - L(TEXT("End.Delay"))) / L(TEXT("End.FadeIn")), 0.f, 1.f);
	if (Appear <= 0.f)
	{
		return;
	}
	Rect(0, 0, Canvas->ClipX, Canvas->ClipY, WithAlpha(C(TEXT("HudOverlay")), Appear));
	const float W = SL(TEXT("End.W")), H = SL(TEXT("End.H"));
	const float CX = Canvas->ClipX * 0.5f;
	const float X = CX - W * 0.5f;
	const float Y = Canvas->ClipY * 0.5f - H * 0.5f + SL(TEXT("End.Slide")) * (1.f - Appear);
	const FLinearColor Accent = C(bWon ? TEXT("HudGold") : TEXT("HudRed"));
	const float Border = SL(TEXT("End.Border"));
	Panel(X - Border, Y - Border, W + Border * 2.f, H + Border * 2.f, WithAlpha(Accent, Appear), L(TEXT("End.BorderCorner")));
	Panel(X, Y, W, H, WithAlpha(C(TEXT("HudEndPanel")), Appear), L(TEXT("End.Corner")));
	Text(YJJLoc::Get(bWon ? TEXT("Hud.Won") : TEXT("Hud.Lost")), CX, Y + SL(TEXT("End.TitleY")), L(TEXT("End.TitleSize")), WithAlpha(Accent, Appear), EAlign::Center,
		L(TEXT("End.TitleOutline")));
	Text(YJJLoc::Format(TEXT("Hud.Wave"), { Game.GetWave(), UYJJGame::GetWaveCount() }), CX, Y + SL(TEXT("End.WaveY")), L(TEXT("End.WaveSize")),
		WithAlpha(C(TEXT("HudDim")), Appear), EAlign::Center);

	// 별점: 하나씩 톡톡 떨어진다
	const int32 Stars = Game.GetStars();
	for (int32 i = 0; i < MaxStars; ++i)
	{
		const float Pop = FMath::Clamp((Game.GetEndAge() - L(TEXT("End.Delay")) - L(TEXT("Stars.Delay")) - i * L(TEXT("Stars.Step"))) / L(TEXT("Stars.PopTime")), 0.f, 1.f);
		const bool bEarned = i < Stars;
		const float Scale = bEarned ? 1.f + L(TEXT("Stars.Bounce")) * FMath::Sin(Pop * PI) : 1.f;
		const float R = SL(TEXT("Stars.R")) * Scale;
		const float SX = CX + (i - (MaxStars - 1) * 0.5f) * SL(TEXT("Stars.Gap"));
		const float SY = Y + SL(TEXT("Stars.Y")) - (i == 1 ? SL(TEXT("Stars.MiddleLift")) : 0.f);
		Star(SX, SY, R + SL(TEXT("Stars.Outline")), WithAlpha(C(TEXT("HudInk")), Appear));
		Star(SX, SY, R, WithAlpha(C(TEXT("HudStarOff")), Appear));
		if (bEarned && Pop > 0.f)
		{
			Star(SX, SY, R, WithAlpha(C(TEXT("HudGold")), Appear * Pop));
		}
	}

	const TPair<FString, FString> Lines[] = {
		{ YJJLoc::Get(TEXT("Hud.StatKills")), FString::FromInt(Game.GetKills()) },
		{ YJJLoc::Get(TEXT("Hud.StatDrowned")), FString::FromInt(Game.GetDrowned()) },
		{ YJJLoc::Get(TEXT("Hud.StatCombo")), YJJLoc::Format(TEXT("Hud.StatComboValue"), { Game.GetBestCombo() }) },
		{ YJJLoc::Get(TEXT("Hud.StatLeaked")), FString::FromInt(Game.GetLeaked()) } };
	const float Inset = SL(TEXT("End.StatsInset"));
	for (int32 i = 0; i < UE_ARRAY_COUNT(Lines); ++i)
	{
		const float LineY = Y + SL(TEXT("End.StatsY")) + i * SL(TEXT("End.StatsStep"));
		Text(Lines[i].Key, X + Inset, LineY, L(TEXT("End.StatLabelSize")), WithAlpha(C(TEXT("HudCream")), Appear));
		Text(Lines[i].Value, X + W - Inset, LineY, L(TEXT("End.StatValueSize")), WithAlpha(C(TEXT("HudGold")), Appear), EAlign::Right);
	}

	const float BW = SL(TEXT("End.ButtonW")), BH = SL(TEXT("End.ButtonH"));
	const float BX = CX - BW * 0.5f, BY = Y + H - BH - SL(TEXT("End.ButtonBottom"));
	const bool bHover = FBox2D(FVector2D(BX, BY), FVector2D(BX + BW, BY + BH)).IsInside(Mouse);
	AddButton(BX, BY, BW, BH, ButtonRestart);
	Panel(BX, BY, BW, BH, WithAlpha(C(bHover ? TEXT("HudButtonHover") : TEXT("HudButton")), Appear), L(TEXT("End.ButtonCorner")));
	Text(YJJLoc::Get(TEXT("Hud.Restart")), CX, BY + BH * 0.5f, L(TEXT("End.ButtonSize")), WithAlpha(C(TEXT("HudCream")), Appear), EAlign::Center, L(TEXT("End.ButtonOutline")));
}
