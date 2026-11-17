#include "Player/YJJCommander.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "UI/YJJHUD.h"
#include "World/YJJArena.h"
#include "World/YJJMap.h"

AYJJCommanderPawn::AYJJCommanderPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetFieldOfView(55.f);
	RootComponent = Camera;
}

AYJJCommander::AYJJCommander()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void AYJJCommander::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameAndUI Mode;
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);
	ApplyCamera();
}

void AYJJCommander::SetView(const FVector& InFocus, float InYaw, float InPitch, float InDistance)
{
	Focus = InFocus;
	Yaw = InYaw;
	Pitch = InPitch;
	Distance = TargetDistance = InDistance;
	ApplyCamera();
}

void AYJJCommander::ApplyCamera()
{
	if (APawn* Mine = GetPawn())
	{
		const FRotator Rotation(Pitch, Yaw, 0.f);
		Mine->SetActorLocationAndRotation(Focus - Rotation.Vector() * Distance, Rotation);
		SetControlRotation(Rotation);
	}
}

void AYJJCommander::RestartArena()
{
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

void AYJJCommander::HandleButton(int32 Button)
{
	UYJJGame* Game = UYJJGame::Get(this);
	if (!Game)
	{
		return;
	}
	if (Button >= 0 && Button < YJJTowerTypeCount)
	{
		Selected = static_cast<EYJJTowerType>(Button);
	}
	else if (Button == AYJJHUD::ButtonStartWave)
	{
		Game->StartWave();
	}
	else if (Button == AYJJHUD::ButtonRestart)
	{
		RestartArena();
	}
}

void AYJJCommander::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UYJJGame* Game = UYJJGame::Get(this);
	if (!Game || !IsLocalController())
	{
		return;
	}
	// 슬로 모션 중에도 카메라는 제 속도로
	const float RealDelta = FMath::Min(static_cast<float>(FApp::GetDeltaTime()), 0.1f);

	// 카메라
	const FRotator Flat(0.f, Yaw, 0.f);
	FVector Pan = FVector::ZeroVector;
	if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up)) { Pan += Flat.Vector(); }
	if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) { Pan -= Flat.Vector(); }
	if (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) { Pan += FRotationMatrix(Flat).GetUnitAxis(EAxis::Y); }
	if (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) { Pan -= FRotationMatrix(Flat).GetUnitAxis(EAxis::Y); }
	Focus += Pan.GetSafeNormal() * Distance * 0.6f * RealDelta;
	Focus.X = FMath::Clamp(Focus.X, -2900.f, 2000.f);
	Focus.Y = FMath::Clamp(Focus.Y, -1600.f, 1400.f);
	if (IsInputKeyDown(EKeys::Q)) { Yaw += 70.f * RealDelta; }
	if (IsInputKeyDown(EKeys::E)) { Yaw -= 70.f * RealDelta; }
	if (WasInputKeyJustPressed(EKeys::MouseScrollUp)) { TargetDistance *= 0.85f; }
	if (WasInputKeyJustPressed(EKeys::MouseScrollDown)) { TargetDistance *= 1.18f; }
	TargetDistance = FMath::Clamp(TargetDistance, 1200.f, 7000.f);
	Distance = FMath::FInterpTo(Distance, TargetDistance, RealDelta, 10.f);
	ApplyCamera();

	// 단축키
	const FKey TowerKeys[YJJTowerTypeCount] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
	for (int32 i = 0; i < YJJTowerTypeCount; ++i)
	{
		if (WasInputKeyJustPressed(TowerKeys[i]))
		{
			Selected = static_cast<EYJJTowerType>(i);
		}
	}
	const bool bEnded = Game->GetPhase() == EYJJPhase::Won || Game->GetPhase() == EYJJPhase::Lost;
	if (WasInputKeyJustPressed(EKeys::SpaceBar) || WasInputKeyJustPressed(EKeys::Enter))
	{
		Game->StartWave();
	}
	if (bEnded && WasInputKeyJustPressed(EKeys::R))
	{
		RestartArena();
		return;
	}

	// 마우스
	AYJJArena* Arena = nullptr;
	for (TActorIterator<AYJJArena> It(GetWorld()); It; ++It)
	{
		Arena = *It;
		break;
	}
	float MouseX = 0.f, MouseY = 0.f;
	const bool bMouse = GetMousePosition(MouseX, MouseY);
	const FVector2D Mouse(MouseX, MouseY);
	AYJJHUD* Hud = Cast<AYJJHUD>(GetHUD());
	const int32 Button = (bMouse && Hud) ? Hud->HitTest(Mouse) : INDEX_NONE;

	HoverPad = INDEX_NONE;
	FVector RayOrigin, RayDir;
	if (bMouse && Button == INDEX_NONE && !bEnded && DeprojectScreenPositionToWorld(MouseX, MouseY, RayOrigin, RayDir) && RayDir.Z < -0.01f)
	{
		const FVector Ground = RayOrigin + RayDir * (-RayOrigin.Z / RayDir.Z);
		const TArray<YJJMap::FPad>& Pads = YJJMap::Pads();
		for (int32 i = 0; i < Pads.Num(); ++i)
		{
			if (FVector::DistSquared2D(Ground, Pads[i].Location) < FMath::Square(YJJMap::PadRadius + 30.f))
			{
				HoverPad = i;
				break;
			}
		}
	}
	if (Arena)
	{
		if (HoverPad != INDEX_NONE)
		{
			const bool bBuilt = Game->HasTower(HoverPad);
			const EYJJTowerType Type = bBuilt ? Game->GetPadTower(HoverPad) : Selected;
			Arena->SetHighlight(HoverPad, Type, bBuilt || Game->GetGold() >= UYJJGame::TowerInfo(Type).Cost);
		}
		else
		{
			Arena->SetHighlight(INDEX_NONE, Selected, true);
		}
	}

	if (WasInputKeyJustPressed(EKeys::LeftMouseButton) && bMouse)
	{
		if (Button != INDEX_NONE)
		{
			HandleButton(Button);
		}
		else if (HoverPad != INDEX_NONE)
		{
			if (!Game->HasTower(HoverPad))
			{
				Game->Build(HoverPad, Selected);
			}
			else
			{
				// 지은 타워를 누르면 강화
				Game->Upgrade(HoverPad);
			}
		}
		else if (!bEnded)
		{
			// 딱밤: 커서에 가장 가까운 적을 카메라 반대쪽으로 튕긴다
			AYJJWobbler* Best = nullptr;
			float BestSq = FMath::Square(60.f);
			for (AYJJWobbler* Enemy : Game->GetEnemies())
			{
				FVector2D Screen;
				if (Enemy && Enemy->IsAlive() && ProjectWorldLocationToScreen(Enemy->GetBodyLocation(), Screen))
				{
					const float DistSq = FVector2D::DistSquared(Screen, Mouse);
					if (DistSq < BestSq)
					{
						BestSq = DistSq;
						Best = Enemy;
					}
				}
			}
			if (Best)
			{
				Game->Flick(Best, Flat.Vector());
			}
		}
	}
	if (WasInputKeyJustPressed(EKeys::RightMouseButton) && HoverPad != INDEX_NONE)
	{
		Game->Sell(HoverPad);
	}
}
