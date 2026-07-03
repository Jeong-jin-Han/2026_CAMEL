---
title: "PGAS — Partitioned Global Address Space (분할 전역 주소 공간)"
aliases: [PGAS, Partitioned Global Address Space, 분할 전역 주소 공간, place, release consistency, UPC, Chapel, one-sided communication]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
  - topic/pgas
---

# PGAS — Partitioned Global Address Space

> [!abstract] 이 노트
> multi-node 메모리를 **하나의 전역 주소 공간처럼 쓰되, "로컬 vs 원격"을 프로그래밍 모델에 드러내는** 30년 된 HPC 분야. multi-node coherence 비용([[CXL Multi-node Coherence]])을 *프로그래머/컴파일러가 명시적으로 다루게* 하는 대표적 답이라 여기에 정리한다. (CXL 위 재해석 가능성은 가설 [[H2 — CXL 위에서 PGAS 재해석]].)

## 한 문장
> 전역 주소 공간(global address space)을 **place(장소) 단위로 분할(partitioned)** 해서, 프로그래머가 "이 데이터는 로컬(싸다) vs 원격(비싸다)"을 **구별하며** 쓰게 하는 모델. — shared-memory의 편함과 message-passing의 지역성(locality) 인식을 절충.

## 두 극단 사이의 절충
```
순수 shared memory   ── 전부 균일하게 접근(편함)   ── but 원격 비용이 숨겨짐 → 성능 예측 불가
PGAS                 ── 전역 주소지만 place로 분할  ── 로컬/원격을 코드에서 의식
순수 message passing ── 명시적 send/recv(MPI)      ── 지역성 완전 노출 but 프로그래밍 고통
```
- PGAS = 가운데. **주소는 전역(포인터 하나로 원격도 가리킴)** 이지만, **접근 비용이 place에 따라 다름을 감춤 없이 노출.**
- place = 보통 한 노드(또는 그 노드의 메모리). 로컬 place 접근은 싸고, 원격 place 접근은 (느리지만) 같은 문법으로 가능.

## coherence 모델 — release consistency
- PGAS는 보통 **release consistency**를 채택: 원격 업데이트는 **동기화 지점(synchronization point) 이후에만** 다른 place에 보이도록 보장.
- 즉 평소엔 relaxed(전파 안 함) → 동기화 때만 일관성 맞춤. → multi-node에서 "매 write마다 전 노드 무효화"하는 강한 coherence의 비용 폭발([[CXL Multi-node Coherence]])을 **모델 차원에서 회피.**
- **one-sided communication**: 원격 데이터를 상대 CPU 개입 없이 put/get(한쪽만 관여) — MPI의 양방향 send/recv와 대비되는 PGAS 통신 원형.

## 자동 vs 명시 — PGAS의 오래된 두 축
"place 배치와 원격 접근을 **누가** 정하나"를 두고 스펙트럼이 갈린다.
```
자동(컴파일러/런타임) ─ 배열을 "distributed"로 선언만 → 매핑·이동 자동
명시(프로그래머)      ─ 원격 접근을 코드에서 직접 제어(로컬/원격 구분해 작성)
```
- **자동 쪽**: Chapel(배열 `distributed` 선언 → 자동 매핑) — prefetching·page migration·NUMA 배치 같은 시스템 자동화와 결이 닿음.
- **명시 쪽**: UPC(C99 확장), Co-Array Fortran, Titanium(Java 계열) — 원격 접근을 명시적으로 통제.
- 이 "얼마나 자동 vs 얼마나 프로그래머에게"의 균형이 PGAS 연구의 핵심 논쟁축.

## 대표 언어/라이브러리
| 이름 | 기반 | 성격 |
|---|---|---|
| **UPC** | C99 확장 | 명시적 제어 |
| **Co-Array Fortran (CAF)** | Fortran | 명시적 제어 |
| **Titanium** | Java 계열 | 명시적 제어 |
| **Chapel** | 신규 언어(Cray) | 자동 매핑 지향 |
| **Global Arrays** | 라이브러리 | 분산 배열 추상화 |

## 전통 PGAS의 전제 — 원격 = 네트워크 메시지
- 전통 PGAS 구현은 원격 place 접근을 **네트워크 메시지**(InfiniBand/Ethernet 위 one-sided RDMA 등)로 처리 — 밑바탕이 message-passing.
- 즉 "전역 주소처럼 보이지만" 실제 원격 접근은 네트워크 왕복 → 느리고, cache-coherent가 아님.
- → 이 전제가 바뀌면(원격이 네트워크가 아니라 **CXL로 직접 cache-coherent 접근**이면) PGAS 설계 공간이 다시 열린다는 게 CXL 시대의 관전 포인트. (그 재해석 = 가설 [[H2 — CXL 위에서 PGAS 재해석]].)

## 인접 개념 (혼동 주의 — 자동 배치 계열)
PGAS의 "자동" 축과 목적이 겹쳐 보이지만 전통적으로 **목적이 "속도"** 였던 것들:
- **prefetching**: 다음 쓸 데이터 미리 가져오기(latency 숨기기). CPU 캐시가 HW로 수십 년.
- **page migration**: NUMA/tiered memory에서 hot page를 가까운 곳으로 이동.
- **access pattern profiling**: 접근 추적(PEBS 등 HW 지원) 기반 최적화.
- → 이들은 전통적으로 *속도* 최적화. "coherence 비용 최소화"를 목적으로, multi-node·CXL 맥락에서 재조준하는 건 덜 탐구된 지점(→ [[H2 — CXL 위에서 PGAS 재해석]]).

## 한 줄 요약
> PGAS = 전역 주소 공간을 place로 분할해 "로컬 vs 원격"을 프로그래밍 모델에 노출하고, release consistency로 동기화 지점에서만 일관성을 맞추는 30년 HPC 모델(UPC·Chapel·CAF). 전통 구현은 원격=네트워크 메시지 — CXL cache-coherent 원격이 이 전제를 바꾼다.

---
**관련**: [[CXL Multi-node Coherence]] · [[CXL Coherence]] · [[CXL Distributed Translation]] · [[H2 — CXL 위에서 PGAS 재해석]] · [[H1 — 워크로드 특화로 multi-node coherence 줄이기]]
