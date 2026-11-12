# LDA:R 0.7.7 RC — Native Alias 안정화 및 준비·재생 분리

이 설치본은 NPC 공연 제어를 Quest/Reference Alias/Package와 C++로 처리합니다.
새로운 Papyrus 해제 스크립트 또는 Papyrus VM 호출에 의존하지 않습니다.
DLL과 DanceStudioData.esl을 함께 교체해야 합니다. 기존 사용자 설정과 곡 패키지는 포함하지 않습니다.
두 편집기 및 기존 HKX·음악은 변경하지 않았습니다. 게임을 완전히 종료한 상태에서 설치하세요.

## 이번 안정화 변경
- plugin.cpp BeginPrepare/ProcessCommands: 곡 확인과 자동 재배치는 준비만 수행합니다. 준비 중 명시적인 재생키 입력만 시작 예약을 만듭니다. ESC 및 종료 시 예약을 정리합니다.
- PerformanceAlias.h Acquire: 전역 orphan 차단을 제거했습니다. 복구 슬롯은 기존 AliasLeaseTable의 점유 Lease로 격리되며 나머지 슬롯은 사용합니다.
- PerformanceAlias.h Detached: Detached/Attached/Unresolved/Invalid를 구분합니다. 참조 조회 실패와 삭제 참조를 성공으로 처리하지 않습니다. 불확실한 슬롯은 격리하며 60회 제한을 유지합니다.
- PerformanceAlias.h Maintain: Alias 소유권과 Package의 Unknown/Performance/Other/Reevaluating을 별도 관측합니다. 제한된 재평가만 유지하며 Package 차이로 공연을 취소하지 않습니다.
- AliasLease.h: 확인된 Detached 상태만 해제 완료로 인정합니다.
- PerformanceActors/SessionCore/SessionInputPolicy/ESL은 기존 RC와 동일합니다. Quest/Alias/PACK 구조를 교체하지 않았습니다.

## 구현
- QUST 0x803, PACK 0x804, 저장용 CELL 0x805, 영속 비액터 XMarker 0x806~0x80A.
- 기존 0x800/0x801/0x802 레코드 그룹은 바이트 단위로 동일합니다.
- 5개 Alias는 Optional이며 ActorsOnly가 아닙니다. 각 슬롯의 전용 마커로 초기화합니다.
- NPC 등록과 실제 공연 제어 획득을 분리하고, 배치 시 Alias 할당 결과를 확인합니다.
- 공연 종료 시 ForceRefTo(marker)로 NPC를 밀어냅니다. nullptr 또는 플레이어를 교체 대상으로 쓰지 않습니다.
- 마커 점유와 기존 NPC의 ExtraAliasInstanceArray에서 공연 Quest 연결이 제거됐는지 모두 확인합니다.
- 서로 다른 엔진 갱신에서 2프레임 안정화 확인 후에만 슬롯을 재사용합니다.
- 이전 세이브의 남은 Alias 할당은 안전한 월드 갱신에서 제한적으로 정리합니다.
- 다른 참조가 예상 밖에 점유한 슬롯은 강제로 덮어쓰지 않고 재사용을 보류합니다.
- 원래 AI·위치 복구는 Alias 해제 확인 후 진행합니다. 이미 소실된 참조의 AI 복구를 성공으로 간주하지 않습니다.
- 복구 재시도는 제한되며, 실패 기록을 임의로 성공 처리하지 않습니다.
- 공연 중 위치 고정은 해제하여 원본 안무 이동을 허용합니다.

## 확인 결과
PASS: MSVC x64 Release DLL compile/resource/link.
PASS: 실제 ESL 생성 및 Mutagen 재읽기, 5개 Alias/마커/PACK 연결 검사.
PASS: 생산 AliasLease/PerformanceActors 코드의 모의 배치·복구·세션 검사 104개.
PASS: 소스 배포 레이아웃에 옮긴 동일 검사 104개.
PASS: DLL/소스 SHA-256 일치 및 ZIP 무결성.
PASS: 실제 plugin.cpp의 준비 예약/취소 경로 및 변경 범위 소스 검사 14개 (0.8.0 RC 안정화 당시 비교 기준). 버전만 0.7.7로 변경했습니다.
사용자 관측: 이전 RC에서 NPC 3인 실게임 공연 성공. 이번 수정본의 실게임 재검증과는 구분합니다.
104개 모의 검사는 실제 AliasLease/PerformanceActors/SessionCore를 실행하며 엔진 참조·Package·위치·Alias API는 Mock입니다.
Detached 엔진 참조 조회 분기는 코드 검토 및 DLL 컴파일 대상입니다. Mock의 참조 불명/삭제 결과를 이용해 해제 정책을 실행 검사했습니다.
plugin.cpp 전체 게임 입력 루프는 실게임 미검증이며, 해당 실제 소스 경로의 정적 검사와 SessionCore 모의 흐름을 구분합니다.
NOT TESTED: Skyrim SE 1.5.97 실게임 Alias 교체, AI 반환, 실제 브레뉴인 공연.
NOT TESTED: Steam AE 런타임에서의 실게임 동작.
모의 검사 통과는 실제 엔진의 Alias 연결 제거 또는 시각적인 춤 재생을 증명하지 않습니다.

## 사용자 실게임 확인 — 브레뉴인 1명부터
1. 테스트 전 별도 저장을 만들고, 브레뉴인을 등록·배치해 공연을 준비합니다.
2. 공연을 수동 정지한 뒤 NPC가 일반 행동으로 돌아가는지 확인합니다.
3. 같은 NPC로 연속 공연, 준비 중 취소, 자연 종료를 각각 확인합니다.
4. 세이브 전환 후 NPC에게 이전 공연 제어가 남는지 확인합니다.
5. DanceStudio.log에서 Native alias eviction의 parked=true/detached=true와
   Alias native release confirmed 또는 Saved alias swept를 확인합니다.
   parked=true만으로 해제 성공을 판정하지 마세요.
6. 문제가 있으면 해당 로그와 취소/종료/로드 중 어느 단계였는지 전달하세요.

퀘스트 Scene의 완전한 재개는 보장하지 않습니다. 원본 모드의 과거 세이브 상태를
새 데이터만으로 전부 복구한다고 보장하지 않습니다.
이전 Papyrus 실험 파일은 로컬 개발 백업으로만 유지하며 이 설치본에는 포함하지 않습니다.

참고 사례: https://github.com/marthofdoom/MFO/blob/main/Docs/ENGINE_NOTES.md
해당 사례의 다른 게임 버전에서 성공한 결과를 SE 1.5.97 검증으로 취급하지 않습니다.

## 공개 소스 구성

Dance Studio 0.6.1 / Facial Studio 0.6.0은 변경하지 않았습니다. data/DanceStudioData.esl과 data/base/DanceStudioData.esl은 자체 제작 데이터이며 제3자 곡·얼굴 자산은 포함하지 않습니다. DLL과 설치 ZIP은 이번 소스 갱신에서 업로드하지 않습니다. 이전 dist 파일은 구버전입니다.
