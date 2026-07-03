---
title: "CAMEL Lab CXL 연구 계보"
aliases: [CAMEL Lab CXL 연구 계보, CXL 계보, CAMEL CXL lineage, CXL papers, CXL 논문 flow]
type: hub
tags: [meta/hub, cluster/cxl, camel-cxl-lineage]
---
# CAMEL Lab CXL 연구 계보 (시간순 flow)

> [!warning] 이 노트의 성격
> **세미나 논문 리스트 밖**의 계보 — *내가 CXL 연구를 하게 될 때 배경*으로 CAMEL Lab의 CXL 흐름을 추적한 것. 저자·연도·계보 해석은 **내가 정리한 것**(검증 전)이며, 각 논문의 상세·인용은 PDF를 [[pdf_summary]] 워크플로우로 정독한 뒤 채운다. 현재 대부분 `paper-stub`(정독 전).

> [!tip] 쓰는 법
> 각 `[[논문]]`은 `concepts/CXL/papers/<제목>/` 폴더의 노트로 연결됨. PDF를 그 폴더에 넣고 [[pdf_summary]]를 적용 → stub이 정식 요약으로 승격. 그래프 뷰로 보면 아래 분기 구조가 그대로 보인다.

---

## Phase 0 — 토대 (2022 이전)
- [[LightPC]] (ISCA 2022) — pure NVM으로 DRAM 없이 **full-system persistence**. "메모리를 다르게 구성할 수 있다"는 발상 = CXL 연구의 사상적 기반. ✅ 정독됨(insights)

## Phase 1 — CXL 진입 (2022)
- **[[DirectCXL]]** (ATC 2022, Donghyun Gouk) — ★ **뿌리**. OS page-fault 경로를 없앤 direct load/store로 CXL memory disaggregation을 실증. 이후 모든 branch가 여기서 갈라짐.
- [[Hello Bytes, Bye Blocks]] (HotStorage 2022, 정명수 단독) — CXL-SSD 개념 첫 제안(vision). block→**byte-addressable**.
- [[Memory Pooling with CXL]] (IEEE Micro 2023, Donghyun Gouk) — 여러 host가 CXL memory 공유하는 **pooling**.
- [[Practical Memory Disaggregation using CXL]] (WORDS 2022) — DirectCXL의 practical 보완(workshop).

## Phase 2 — 워크로드 특화 (2023) — "그래서 뭘 올리나?"
- **[[TrainingCXL]]** (IEEE Micro 2023, Miryeong Kwon) — ML training(추천모델)을 CXL disaggregated memory에. batch-aware checkpoint + relaxed training으로 fault tolerance. ✅ 정독됨 *(내가 감명받은 논문)*
- [[CXL-ANNS]] (ATC 2023, Junhyeok Jang) — billion-scale **ANN search**. 같은 방향, 다른 워크로드(inference/search).
- [[Cache in Hand]] (HotStorage 2023, Miryeong Kwon) — CXL-SSD에 **prefetcher**. Hello Bytes 후속.
- [[GraphTensor]] (IPDPS 2023, Junhyeok Jang) — 대규모 **GNN** (CXL 직접 아님, GNN 라인).

## Phase 3 — Hardware 구체화 (2024)
- [[DockerSSD]] (HPCA 2024, Gouk·Kwon) — SSD 안 container화 in-storage processing (CXL-SSD 라인과 합류).
- **[[Breaking Barriers]]** (HotStorage 2024, Donghyun Gouk) — ★ **CXL-GPU 첫 등장**. sub-two-digit ns latency controller로 GPU가 CXL memory 빠르게 접근 → "CXL은 GPU에 무의미" 반박.

## Phase 4 — Fabric과 Scale-up (2025) — device를 넘어 datacenter fabric으로
- [[CXL-GPU]] (IEEE Micro 2025, Donghyun Gouk) — Breaking Barriers full paper. GPU memory boundary 확장.
- **[[ScalePool]]** (DIMES@SOSP 2025, Hyein Woo) — **Hybrid XLink-CXL Fabric**으로 composable disaggregation. ← 내가 논의한 topology 문제.
- **[[MPI-over-CXL]]** (SPICE@MICRO 2025, Miryeong Kwon) — HPC MPI를 CXL 위에. *기존 모델 포팅*(새 programming model 아님) → 내 빈 자리가 보이는 지점.
- [[From Block to Byte]] (IEEE Micro 2025, Miryeong Kwon) — CXL-SSD 라인 **결정판**(protocol 변환 + instruction annotation).
- [[CXL Topology-Aware Prefetching]] (IEEE Micro 2025, Dongsuk Oh) — **topology-aware** SSD prefetcher.

## Phase 5 — Silicon과 Vision (2026)
- **[[Panmnesia CXL Controller]]** (ISCA 2026 Industry, Miryeong Kwon) — ★ **Silicon-proven** controller + port-based routing switch. 학술→산업 전환점, 집대성.
- **[[One-Chip-Like Datacenter]]** (Nature Reviews EE 2026, 정명수) — ★ **Vision**. CXL scale-up fabric으로 datacenter를 하나의 칩처럼. 모든 것의 종합.
- [[AutoGNN]] (HPCA 2026, 강승관) — GNN preprocessing HW 가속 (GNN 라인 결정판, CXL 직접 아님).

---

## 분기 구조 (뿌리 하나 → 여러 branch → 재합류)
- **[[DirectCXL]]** (2022) — 뿌리
  - **CXL-SSD 라인**: [[Hello Bytes, Bye Blocks|Hello Bytes]] → [[Cache in Hand]] → [[From Block to Byte]] → [[CXL Topology-Aware Prefetching]]
  - **ML/워크로드 라인**: [[TrainingCXL]] (training) · [[CXL-ANNS]] (search)
  - **GPU 라인**: [[Breaking Barriers]] → [[CXL-GPU]]
  - **Fabric 라인**: [[ScalePool]] (hybrid fabric) · [[MPI-over-CXL]] (통신)
  - **인접(GNN/storage)**: [[GraphTensor]] → [[AutoGNN]] · [[DockerSSD]]
  - **↓ 재합류 (2026)**
  - **[[Panmnesia CXL Controller]]** (silicon) → **[[One-Chip-Like Datacenter]]** (vision = 종합)

> [!note]- 원본 ASCII 트리 (내 정리, 시각용)
> ```
> 2022  DirectCXL ─────────────────────────────────────┐
>         ├── Hello Bytes (CXL-SSD vision)              │
>         │     ├── Cache in Hand (2023)                │
>         │     │     └── From Block to Byte (2025)     │
>         │     │           └── CXL Topology Prefetch   │
>         ├── TrainingCXL (2023, ML training)           │
>         ├── CXL-ANNS (2023, ANN search)               │
>         ├── Breaking Barriers (2024, CXL-GPU)         │
>         │     └── CXL-GPU full paper (2025)           │
>         ├── ScalePool (2025, hybrid fabric)           │
>         ├── MPI-over-CXL (2025, HPC comm)             │
>         └── Panmnesia Silicon (ISCA 2026) ◄───────────┘
>               └── One-Chip-Like Datacenter (Nature Rev. 2026)
> ```

---

## 읽어야 할 패턴 (내 관점)
1. **뿌리 하나(DirectCXL), 여러 branch로 분화 후 2026 재합류** — CAMEL의 전략: 하나의 기반 기술에서 여러 방향 탐색 → 성숙한 것들을 silicon·vision으로 통합.
2. **workshop → conference → journal → silicon 성장 경로** — 아이디어를 workshop에서 먼저 던지고 검증 후 full paper로. 예: Hello Bytes(WS)→Cache in Hand(WS)→From Block to Byte(journal); Breaking Barriers(WS)→CXL-GPU(journal).
3. **★ 빈 자리 = programming model.** 이 계보에 **programming model 연구가 없다.** [[MPI-over-CXL]]은 기존 MPI를 *포팅*한 것이지 새 모델 제안이 아님. → 내가 논의한 [[H2 — CXL 위에서 PGAS 재해석|PGAS-over-CXL]] · CXL-aware runtime · flat-topology programming model이 이 계보의 **빈 branch**.

## 내 연구와의 접점
- 빈 자리(programming model): [[H2 — CXL 위에서 PGAS 재해석]] · [[PGAS]]
- multi-node coherence 비용: [[H1 — 워크로드 특화로 multi-node coherence 줄이기]] · [[CXL Multi-node Coherence]]
- topology awareness: [[CXL Topology-Aware Prefetching]] · [[ScalePool]]

## 추천 정독 순서 (내 관심 = fabric/programming model 기준)
[[DirectCXL]] → [[TrainingCXL]] → [[CXL-ANNS]] → [[Breaking Barriers]] → [[ScalePool]] → [[MPI-over-CXL]] → [[Panmnesia CXL Controller]] → [[One-Chip-Like Datacenter]]
(병렬로 CXL-SSD 라인: [[Hello Bytes, Bye Blocks|Hello Bytes]] → [[Cache in Hand]] → [[From Block to Byte]])

---

## 정독 현황 (2026-07-04 기준)
**✅ Full read 17편** (PDF 정독 + [[pdf_summary]] 적용 완료):
- Phase 1: [[DirectCXL]] · [[Hello Bytes, Bye Blocks]] · [[Memory Pooling with CXL]] · [[Practical Memory Disaggregation using CXL]]
- Phase 2: [[TrainingCXL]] · [[CXL-ANNS]] · [[Cache in Hand]] · [[GraphTensor]]
- Phase 3: [[DockerSSD]] · [[Breaking Barriers]]
- Phase 4: [[CXL-GPU]] · [[ScalePool]] · [[MPI-over-CXL]] · [[From Block to Byte]] · [[CXL Topology-Aware Prefetching]]
- Phase 5: [[AutoGNN]] · (+ Phase 0 [[LightPC]], insights/)

**☐ 미정독 2편** (공개 PDF 없음 = 내부/회사 자료 추정, stub 유지):
- [[Panmnesia CXL Controller]] (ISCA'26 Industry) · [[One-Chip-Like Datacenter]] (Nature Reviews'26)

> [!note] 정독 중 발견 — 같은 시스템의 workshop→journal 판본
> [[Cache in Hand]] (HotStorage'23)와 [[CXL Topology-Aware Prefetching]] (IEEE Micro'25)은 **둘 다 ExPAND** 시스템 — 워크숍판 → 저널 확장판 관계. "workshop→journal 성장 경로" 패턴의 실제 사례.

> 각 노트는 한국어+영문용어, verbatim 인용+page, 내 연구(multi-node coherence·PGAS-over-CXL·topology·feasibility-by-building) 연결, 계보 상대링크 포함. Panmnesia·One-Chip은 PDF 확보 시 [[pdf_summary]] 적용 예정.
