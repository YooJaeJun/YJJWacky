# YJJWacky 도구

엔진: `D:\source\repos\UnrealEngine` (UE 5.8.1 소스 빌드, YJJJump·YJJFantasy와 같은 엔진)

## 빌드

```
D:\source\repos\UnrealEngine\Engine\Build\BatchFiles\Build.bat YJJWackyEditor Win64 Development -project="D:\source\repos\YJJWacky\YJJWacky.uproject" -WaitMutex
```

## 콘텐츠 (에디터를 끈 상태에서)

```
powershell -File Tools/Content/BuildContent.ps1
```

- `Tools/Audio/generate_audio.py` : 효과음·배경음 합성 (외부 음원 없음)
- `Tools/Content/build_content.py` : 머티리얼(단색 표면·반투명·물), 소리 임포트, 빈 맵

전장·캐릭터·타워는 에셋이 아니라 코드가 게임 시작 때 조립한다.
캐릭터에는 애니메이션이 없다. 전부 물리다 (`Source/YJJWacky/Physics/YJJWobbler.cpp`).

## 실행·테스트

```
powershell -File Tools/RunGame.ps1               # 그냥 플레이
powershell -File Tools/RunGame.ps1 -Mode play    # 자동 플레이 검증 -> 로그 [AutoPilot] PASS/FAIL, Saved/Screenshots/Play_*.png
```

게임 중 콘솔(`~`): `yjj.AutoPlay`, `yjj.Gold 500`

## 게임: 흐느적 디펜스

흐느적거리는 빨간 놈들이 둑길을 따라 푸딩을 뜯어 먹으러 온다. 길에는 난간이 없다.
길옆 발판에 타워를 지어 길 밖(물)으로 날려 버리면 된다. 10 웨이브를 버티면 승리.

| 타워 | 값 | 하는 일 |
| --- | --- | --- |
| 펀치 타워 | 60 | 권투 장갑이 길을 가로질러 튀어나가 줄 선 적을 통째로 날린다 |
| 대포 | 110 | 멀리서 진짜 물리 포탄을 쏜다. 굴러가며 볼링핀처럼 쓰러뜨리고 한 번 터진다 |
| 선풍기 | 80 | 켜졌다 꺼졌다 하며 길 건너편으로 바람을 분다. 가벼운 놈일수록 잘 날린다 |
| 몽둥이 병사 | 70 | 파란 흐느적이가 길목에 서서 빙글빙글 몽둥이를 돌린다. 돌고 나면 어지러워한다 |

| 적 | 특징 |
| --- | --- |
| 졸병 | 기본 |
| 날쌘돌이 | 작고 빠르고 가볍다 (멀리 날아간다) |
| 뚱보 | 크고 무겁다. 잘 안 날아간다 |
| 대장 | 거대하다. 5·10 웨이브 |

한꺼번에 다섯 이상 쓰러뜨리면 "떼죽음" 슬로 모션 + 보너스 골드.

## 조작

발판 좌클릭 짓기 · 우클릭 팔기 · 적 좌클릭 딱밤(카메라 반대쪽으로 튕김) · 1~4 타워 고르기
Space 다음 웨이브 · WASD/방향키 이동 · 휠 확대 · Q/E 회전 · R 다시 하기(끝난 뒤)
