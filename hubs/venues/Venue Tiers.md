---
title: Venue Tiers
aliases: [Venue Tiers, Venue Tier Guide, 학회 Tier 가이드, Conference Tiers, 탑티어 학회]
tags: [meta/hub, hub/venue, reference]
---
# 탑티어 학회 Tier 가이드 — 내 field (Architecture + Storage Systems)

> **문서 성격**: Reference. 학회 판단 기준이 바뀌거나 새 정보가 오면 업데이트.
> **최초 작성**: 2026-07-10
> **허브 연결**: [[Home]] · [[Hubs]] (venue 개별 노트는 [[#1. 한 눈에 보는 Tier 지도|아래 지도]]에서 각각 wikilink)
> **근거 데이터**: CAMEL Lab publication list, 세미나 시트, lab_member.md

---

## 0. 먼저 — "공식 랭킹"은 없다

컴퓨터 아키텍처/시스템 분야에 **단 하나의 공인 순위표는 없다.** 대신 여러
기준이 공존하고, 상위 학회에 대해서는 합의가 매우 강하다.

세 가지 대표 기준:

- **CSRankings** (미국·글로벌 표준으로 가장 많이 인용됨)
  - Architecture 영역 venue: **[[ASPLOS]], [[ISCA]], [[MICRO]], [[HPCA]]** (이 4개만)
  - Operating Systems 영역 venue: **[[OSDI]], [[SOSP]], [[EuroSys]], [[FAST]], [[ATC|USENIX ATC]]**
- **CCF (중국컴퓨터학회) A/B/C 등급** — 아시아권에서 자주 참조. 위 학회들
  대부분이 CCF-A.
- **비공식 "Big 4" 관행** — 아키텍처 하는 사람들이 입으로 쓰는 표현.
  [[ISCA]] / [[MICRO]] / [[ASPLOS]] / [[HPCA]].

**결론**: 학회 간 미세 서열(예: HPCA가 나머지 셋보다 아래냐)에 집착할
필요 없다. "탑티어냐 아니냐"의 경계만 확실히 알면 된다.

---

## 1. 한 눈에 보는 Tier 지도

| Tier | 학회 | 성격 | 나와의 거리 |
|------|------|------|----------------|
| **아키텍처 4대장** | [[ISCA]], [[MICRO]], [[ASPLOS]], [[HPCA]] | 하드웨어 아키텍처의 정점. 채용·테뉴어 평가 동급 취급 | ⭐ 박사 1저자 메인 target |
| **시스템 정점** | [[OSDI]], [[SOSP]] | OS/시스템 SW 최고봉 (역사적으로 격년제) | CAMEL 홈그라운드 아님 (각 1편) |
| **스토리지·시스템 특화** | [[FAST]], [[ATC|USENIX ATC]], [[NSDI]], [[EuroSys]] | 스토리지·분산·넓은 시스템 | ⭐ FAST/ATC는 CAMEL 제2의 홈 |
| **인접·전문** | SC, SIGMETRICS, PLDI, SIGMOD, PACT | HPC / 성능모델링 / PL / DB / 병렬 | 상황에 따라 |

> ※ 서열은 대략적 consensus이며 절대적이지 않다. 특히 4대장 내부 순서와
> FAST↔ATC↔NSDI 사이는 사람마다 다르게 본다.

---

## 2. 아키텍처 4대장 — 각 학회 성격

### [[ISCA]] (International Symposium on Computer Architecture)
- 주최: ACM SIGARCH. **아키텍처 학회의 원조이자 최고 권위.** 1973년~.
- 성격: 프로세서·메모리·가속기·인터커넥트 등 아키텍처 전반. 가장 "정통".
- 최근 acceptance rate: 약 16~20%.
- CAMEL 실적: **9편** (최다). LightPC, ASAP, Revamping SCM, Silicon-Proven
  CXL Controller('26) 등. → **내 field의 1순위 target.**

### [[MICRO]] (International Symposium on Microarchitecture)
- 주최: IEEE/ACM. **마이크로아키텍처** 초점 (파이프라인·캐시·가속기 내부).
- 성격: ISCA와 쌍벽. 하드웨어 디테일·회로 근접 주제에 강함.
- CAMEL 실적: 4편 (Amber, Ohm-GPU, MemLLM '26 등).

### [[ASPLOS]] (Architectural Support for PL and OS)
- 주최: ACM. **아키텍처 ∩ OS ∩ PL ∩ 컴파일러** 교차점 venue.
- 성격: "시스템 스택을 관통하는" 연구를 좋아함. HW-SW co-design의 성지.
- 특징: 내 KECC(컴파일러) + 시스템 배경이 겹치는 유일한 4대장.
- CAMEL 실적: 5편.

### [[HPCA]] (High-Performance Computer Architecture)
- 주최: IEEE. 4대장 중 가장 "실전 성능·구현" 지향.
- 성격: 최근 CXL·in-storage·GNN 가속 등 emerging HW에 개방적. 내 발표 논문
  [[Smart-Infinity]](HPCA'24)가 여기.
- CAMEL 실적: **8편** (DockerSSD '24, AutoGNN '26 등). 강승관 박사님
  1저자 AutoGNN이 여기(HPCA'26).

---

## 3. 시스템 / 스토리지 학회

### [[OSDI]] (Operating Systems Design and Implementation) · USENIX
### [[SOSP]] (Symposium on Operating Systems Principles) · ACM
- **시스템 SW의 정점 둘.** 역사적으로 격년제로 번갈아(최근 연례화 추세).
- 성격: OS·분산·신뢰성·검증 등 "시스템의 원리". 매우 selective(15~18%).
- **CAMEL 실적: OSDI 1편(FlashShare '18), SOSP 1편(BIZA '24).**
  → 랩의 메인 출구가 **아니다.** 통념("systems=OSDI/SOSP")에 끌려가면
  랩 실제 라인과 어긋남. 세미나 리스트에 OSDI'25 논문들이 있는 건
  "읽을 가치"의 문제이지 "우리가 내는 곳"의 문제가 아님. (단, 내 발표
  [[WOFS]](OSDI'25)는 crash consistency formal proof라 읽기 대상으로 최상.)

### [[FAST]] (File and Storage Technologies) · USENIX
- **스토리지 연구의 정점.** SSD·파일시스템·플래시·NVM의 홈.
- 성격: 측정·실증·디바이스 내부에 강함. 스토리지 하면 여기가 1순위.
- CAMEL 실적: 4편 + 세미나 인턴 리스트의 절반이 FAST. → **내
  FS·crash consistency 주제가 실제로 나갈 가장 유력한 출구.** 발표 논문
  [[DJFS]]·[[Ananke]]가 여기(FAST'25).

### [[ATC|USENIX ATC]] (Annual Technical Conference)
- 성격: 넓은 시스템 (스토리지·네트워크·가상화·ML시스템 등). 실용 지향.
- CAMEL 실적: 5편 ([[CXL-ANNS]], DirectCXL, Vigil-KV 등). **FAST와 함께
  CAMEL 제2의 홈.** CXL-ANNS(Junhyeok Jang, mentor 후보)가 여기.

### [[NSDI]] (Networked Systems Design and Implementation) · USENIX
- 성격: 네트워크·분산 시스템의 정점. distributed training/serving과 접점.
- CAMEL 실적: 리스트 상 없음. 관심사가 distributed로 확장되면 관련.

### [[EuroSys]] · ACM
- 성격: 유럽 시스템 학회. 근래 위상이 크게 올라 top-tier 편입.
  CSRankings OS 영역에 포함. (세미나 리스트에 EuroSys'26 있음)

---

## 4. 인접·전문 학회 (알아만 두기)

| 학회 | 영역 | 비고 |
|------|------|------|
| **SC** (Supercomputing) | HPC | 대규모 병렬·과학계산. CAMEL 2편 |
| **SIGMETRICS** | 성능 측정·모델링 | 정량 분석 강함. CAMEL 4편 |
| **PLDI** | 프로그래밍 언어 | PL 정점. 컴파일러/분석. CAMEL 1편 |
| **SIGMOD / VLDB** | 데이터베이스 | vector search·인덱싱이 여기서도 나옴 (세미나 SIGMOD'25) |
| **PACT** | 병렬 아키텍처·컴파일 | 아키텍처 인접 |

---

## 5. CAMEL Lab이 "실제로" 노는 곳 (데이터)

publication list를 venue별로 집계:

| 학회 | 편수 | 학회 | 편수 |
|------|------|------|------|
| [[ISCA]] | 9 | [[ATC|USENIX ATC]] | 5 |
| [[HPCA]] | 8 | [[ASPLOS]] | 5 |
| [[MICRO]] | 4 | [[FAST]] | 4 |
| SIGMETRICS | 4 | SC | 2 |
| **[[OSDI]]** | **1** | **[[SOSP]]** | **1** |
| [[EuroSys]] | 1 | PLDI | 1 |
| PACT | 1 | Nature Reviews | 1 |

**해석:**

1. **아키텍처 4대장(ISCA+HPCA+MICRO+ASPLOS = 26편)이 압도적 정체성.**
   CAMEL은 근본적으로 **아키텍처 랩**이다.
2. **FAST + ATC (9편)가 스토리지 쪽 제2의 홈.**
3. **OSDI/SOSP는 각 1편** — OS-heavy 시스템 venue는 랩의 메인이 아니다.
4. 따라서 내 **FS·crash consistency 주제도 실제 출구는
   FAST/ATC/아키텍처 4대장**이지 OSDI/SOSP가 아니다.

---

## 6. 내가 target할 학회 (현실적 경로)

lab_member.md의 "CAMEL 1저자 = 4년차 자격증" 패턴과 결합하면:

```
[학부 seminar/인턴 지금]
   → 코드·인프라 기여, 공저자 기회
[박사 1~3년차]
   → 공저자 (ATC/FAST/HPCA 등) + 빌딩 단계. noisy해도 정상
[박사 4년차~]
   → 1저자 target: ISCA / MICRO / HPCA / ASPLOS  또는  FAST / ATC
```

**주력 target 세트 (관심사 기준 우선순위):**

1. **[[FAST]]** — FS·crash consistency·스토리지가 가장 자연스럽게 나가는 곳
2. **[[ISCA]] / [[HPCA]]** — CXL·memory·near-data를 아키텍처 각도로 올릴 때
3. **[[ATC|USENIX ATC]]** — ML systems·CXL 실증을 넓게 풀 때 (CXL-ANNS 라인)
4. **[[ASPLOS]]** — 컴파일러/스택 관통 각도가 살아있을 때 (KECC 경험 연결)
5. **[[MICRO]]** — HW 디테일이 무거워질 때

**[[OSDI]]/[[SOSP]]**: 읽고 배우는 대상으로는 최상. 단 "내가 낼 곳"으로 1순위에
두지는 말 것 — 랩 출력 라인과 다름.

---

## 7. 트렌드 & 커뮤니티가 인정하는 가치

이 분야가 **지금 보상하는 것** (세미나 논문 리스트 + CAMEL 궤적 근거):

### (1) Real system / silicon > simulation-only
- Panmnesia ISCA'26이 제목에 **"Silicon-Proven"**을 박음 = 실제 하드웨어
  실증이 최고 가치 중 하나라는 신호.
- 내 "시뮬레이션의 한계" 서사와 정확히 맞물림. 이 커뮤니티가
  내가 원하던 "직접 구현하고 검증하는"(feasibility-by-building) 문화를
  실제로 보상한다. → 발표 프레임 [[Home|feasibility-by-building]]과 동일.

### (2) LLM inference/training 시스템이 압도적 주류 (지금 이 순간)
- 세미나 리스트: [[SwiftSpec]](speculative decoding), RPU(reasoning),
  FastTTS(test-time scaling), KV-Cache, [[Mooncake]], [[Smart-Infinity]]...
- serving·KV cache·near-storage LLM이 현재 가장 뜨거운 영역.

### (3) 새 인터페이스를 초기에 타는 것 (CXL 파도)
- DirectCXL('22) → TrainingCXL('23) → [[CXL-ANNS]]('23) → Panmnesia('26).
  CXL·memory disaggregation을 남보다 먼저 판 사람이 탑 논문을 가져감.
  → 계보 정리: [[CAMEL Lab CXL 연구 계보]].
- 다음 파도(무엇이 될지)를 초기에 잡는 감각이 곧 1저자 경쟁력.

### (4) Hardware-Software Co-Design / Full-stack
- CAMEL 논문 제목에 "Co-Design", "Hardware-Software"가 반복됨.
- 한 레이어만 건드리는 연구보다 스택을 관통하는 연구가 높게 쳐짐.
- 내 coupled-system 구현 + KECC + Verilog 경험이 이 "full-stack 사고"의 자산.

### (5) 문제 실증 + 정량적 speedup
- "왜 이게 병목인가"를 실측으로 보이고, 해결 후 배수 개선(예: 5.2×)을
  제시하는 서사가 이 커뮤니티의 표준 문법. → 병목이 compute가 아니라
  communication+memory라는 큰 그림은 [[Communication Tax]].

---

## 8. 한 줄 요약

> 내 field의 탑티어는 **아키텍처 4대장([[ISCA]]·[[MICRO]]·[[ASPLOS]]·[[HPCA]])**과
> **스토리지·시스템([[FAST]]·[[ATC]] 중심, [[OSDI]]/[[SOSP]]는 읽기용)**. CAMEL은
> 데이터상 아키텍처 랩 + FAST/ATC가 제2의 홈이며 OSDI/SOSP는 각 1편뿐.
> 내 FS·crash consistency는 **FAST**로, CXL·near-data는
> **ISCA/HPCA/ATC**로 나가는 게 현실 경로. 지금 커뮤니티가 보상하는 건
> **real silicon 실증 · LLM 시스템 · CXL 초기 진입 · full-stack
> co-design**이고, 이건 내 "직접 구현·검증" 지향과 정확히 맞는다.
