# LDA:R / Dance Studio / Facial Studio

현재 소스: **LDA:R 0.7.7 RC / Dance Studio 0.6.1 RC / Facial Studio 0.6.0**

|구성|소스|주요 변경|
|---|---|---|
|LDA:R|[runtime](runtime)|Native Alias/XMarker 제어·반환, 준비·재생 분리, 슬롯별 복구 격리|
|Dance Studio|[editor](editor)|독립 춤·카메라·표정 타임라인, 성별 하위 폴더 전용 모션 출력, 파일·출력 안정화|
|Facial Studio|[facial_studio](facial_studio)|RaceMenu 자동 얼굴 연결, 원자적 경로 재연결, 자동 안정 ID, 얼굴 미리보기 조작|

[최신 변경·검증](docs/LATEST-RELEASE-KO.md) · [빌드](BUILDING-KO.md) · [라이선스와 외부 의존성](THIRD_PARTY.md)

## 사용과 호환성

- 플레이어·NPC를 합쳐 최대 5명이며 NPC-only 공연을 지원합니다. F10 지정·배치·준비·재생, ESC 배치 취소, 공연 중 Space 일시정지를 유지합니다.
- NPC는 전용 Quest·Alias·Package로 제어하고 종료 시 C++ XMarker 교체로 반환합니다. Papyrus 해제 스크립트에 의존하지 않습니다. 공연 중 위치 고정을 해제하며 원래 퀘스트 Scene의 완전한 재개는 보장하지 않습니다.
- 곡 확인과 저장 무대 재배치는 준비만 수행합니다. 준비 완료 후 재생키로 시작하며, 준비 도중 명시적인 재생 입력은 예약합니다.
- 복구되지 않은 Alias는 해당 슬롯만 격리합니다. NPC 참조 불명을 분리 성공으로 처리하지 않으며 Package 변화만으로 공연을 취소하지 않습니다.
- 새 곡은 female/mt_idle.hkx와 male/mt_idle.hkx만 생성합니다. 기존 최상위 모션 참조·해시는 사용하지 않으며 하위 파일의 무결성 검증은 유지합니다.
- Steam SE 1.5.97 / 1.6.640 / 1.6.1170 대상 빌드입니다. 실제 Skyrim/MO2 및 모든 버전의 플레이 검증 완료를 뜻하지 않습니다.
- 기존 플레이어 전용 곡은 NPC 공연을 위해 명시적으로 재출력이 필요합니다. 원본 HKX·음악·개인 프로젝트를 자동 변경하지 않습니다.

## 공개 범위

현재 반영은 소스·한국어 안내와 자체 제작 DanceStudioData.esl입니다. 과거 dist 파일은 파일명에 적힌 구버전입니다. 최신 실행 파일은 이번에 GitHub Releases/dist에 추가하지 않았습니다. 외부 DLL과 내장 XPMSSE의 정확한 재배포 허가 확인이 남아 있습니다.

곡·음악·개인 .jslot·NIF/TRI/DDS·스켈레톤 바이너리·인증 정보·사용자 설정은 올리지 않습니다. **내장 스켈레톤 기능은 소스에 유지**하며 실제 빌드 입력은 사용자가 허가된 원본으로 준비해야 합니다.

자체 소스의 기존 GPL 라이선스와 [추가 허용 조건](EXCEPTIONS.md), 각 외부 코드의 고지를 유지합니다. 저장소 갱신만으로 모든 외부 의존성의 소스·재배포 조건이 충족됐다고 주장하지 않습니다.
