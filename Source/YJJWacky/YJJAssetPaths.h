#pragma once

#include "CoreMinimal.h"

/** 코드에서 쓰는 에셋 경로. 머티리얼·소리·맵은 Tools/Content/build_content.py가 만든다. */
namespace YJJAssetPaths
{
	/** 단색 표면 (Color x 버텍스 컬러). 텍스처 없는 로우폴리 룩의 전부 */
	inline const TCHAR* FlatMaterial = TEXT("/Game/YJJWacky/Materials/M_YJJ_Flat.M_YJJ_Flat");
	/** 반투명 단색 (사거리 원, 바람 줄기, 이펙트) */
	inline const TCHAR* UnlitMaterial = TEXT("/Game/YJJWacky/Materials/M_YJJ_Unlit.M_YJJ_Unlit");
	inline const TCHAR* WaterMaterial = TEXT("/Game/YJJWacky/Materials/M_YJJ_Water.M_YJJ_Water");

	inline const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
	inline const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	inline const TCHAR* Cylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	inline const TCHAR* Cone = TEXT("/Engine/BasicShapes/Cone.Cone");
	inline const TCHAR* Plane = TEXT("/Engine/BasicShapes/Plane.Plane");

	inline const TCHAR* AudioDir = TEXT("/Game/YJJWacky/Audio");

	/** HUD 글꼴 (Content 기준). 배민 주아체, SIL OFL 1.1 */
	inline const TCHAR* GameFontFile = TEXT("YJJWacky/Fonts/Jua-Regular.ttf");
}
