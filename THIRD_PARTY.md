# 외부 코드·자료와 라이선스 경계

이 저장소의 자체 C++/C# 소스는 GPL-3.0-or-later 및 EXCEPTIONS.md의 추가 허용 조건으로 공개합니다. 외부 프로젝트의 저작권과 개별 조건은 그대로 유지합니다. 아래 기록은 출처 목록이며 전체 배포물의 법률적 적합성을 보증하는 문서는 아닙니다.

| 구성요소 | 사용 방식 | 공개 내용 / 확인 범위 |
|---|---|---|
| CommonLibSSE-NG | 정적 링크 | 실제 사용한 SDK의 include/src/cmake와 COPYING.txt·EXCEPTIONS.md를 vendor에 포함. GPLv3 계열+예외. 로컬 수정이 포함된 스냅샷이며 upstream 원본과 동일하다고 주장하지 않음 |
| Open Animation Replacer | 공개 API 헤더 및 실행 중 호출 | runtime/OARAnimations.h. upstream GPL-3.0-or-later+예외, API에 복사 사용 안내가 있음. 이 안내만으로 전체 GPL 의무가 사라진다고 해석하지 않음 |
| True Directional Movement | 공개 ABI 호출 | runtime/Interop.h. upstream MIT, 구현 DLL을 포함하지 않음 |
| SmoothCam | 공개 ABI 호출 | runtime/Interop.h. 구현 DLL을 포함하지 않음. 외부 프로젝트의 별도 라이선스 적용 범위를 배포 전 확인해야 함 |
| SKSE Menu Framework | 공개 C ABI 호출 | runtime/SettingsMenu.h. 프레임워크 구현/ImGui를 포함하지 않음. upstream 권리 조건 별도 적용 |
| SKSE | 인터페이스 및 CommonLib 의존성 | 게임 확장 환경. 원본 설치 프로그램과 DLL을 포함하지 않음 |
| nlohmann/json, fmt, spdlog, DirectXTK, rapidcsv | 빌드 의존성 | 소스/빌드 환경은 개발자가 설치. 확보된 라이선스 문서는 licenses에 포함. 새 바이너리 배포 시 해당 버전의 조건도 확인 필요 |
| HKX2E.dll | 편집기 외부 참조 | 바이너리·소스 미포함. 실제 편집기 입력 해시는 docs/external-editor-inputs.json에 기록. 사용한 로컬 수정본의 원본 소스/재배포 조건은 추가 확인 필요 |
| Matcher.dll | 압축 모션 디코더 | 편집기에서 MotionEditor.Core.SplinePreview.Decode를 호출. 원본 소스 위치를 이번 공개 준비 과정에서 확보하지 못함. 바이너리 미포함. 편집기 의존성 소스 공개가 완결됐다고 주장하지 않음 |
| MemoryPack.Core, Riok.Mapperly.Abstractions | 편집기 외부 참조 | 바이너리 미포함. 정확한 릴리스 입력 해시 기록. 원본 패키지의 개별 조건 적용 |
| 내장 skeleton_female.hkx | 편집기 빌드 리소스 | 외부 스켈레톤 자료이므로 이 Git 저장소에 재배포하지 않음. 빌드 시 권리를 갖춘 사용자가 원본을 제공해야 함 |
| 곡·음원·춤·카메라 HKX, 게임 ESL 생성용 자료 | 사용자 콘텐츠/게임 자료 | 저장소 미포함. 본 소스의 GPL을 해당 자료에 적용하지 않음 |

## 출처
- CommonLibSSE-NG: https://github.com/alandtse/CommonLibSSE-NG
- OAR: https://github.com/ersh1/OpenAnimationReplacer
- TDM: https://github.com/ersh1/TrueDirectionalMovement
- SmoothCam API: https://github.com/mwilsnd/SkyrimSE-SmoothCam/blob/master/SmoothCam/include/SmoothCamAPI.h
- SKSE: https://skse.silverlock.org/
- SKSE Menu Framework API: https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API
- HKX2 Enhanced Library (관련 upstream, 로컬 DLL과의 동일성 미확인): https://github.com/Monitor221hz/HKX2-Enhanced-Library

## 아직 해결하지 않은 의존성 소스 문제
자체 편집기 소스를 공개하는 것과, 편집기 바이너리에 연결된 모든 외부 라이브러리의 대응 소스를 확보하는 것은 별개입니다. HKX2E/Matcher의 실제 빌드 소스와 사용 권한을 확보해야 해당 부분의 완전한 대응 소스 제공/재빌드를 검증할 수 있습니다. 이 저장소에 소스가 없다는 이유로 임의 라이선스를 부여하거나 새로운 디코더로 대체하지 않았습니다.
