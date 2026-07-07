---
title: "Design Principles — 논문을 읽는 고정 렌즈"
aliases: [Design Principles, 설계 원칙, 디자인 원칙, 논문 렌즈, 시스템 설계 원칙]
type: concept
tags:
  - concept
  - concept/methodology
  - topic/design-principles
---

# Design Principles — 논문을 읽는 고정 렌즈

> [!tip] 이 노트의 목적
> 논문마다 **기법(technique)**은 다르지만, 그 기법들이 인스턴스화하는 **보편 원칙(principle)**은 소수다. 원칙을 **고정된 렌즈**로 두고 여러 논문을 같은 축에서 비교·해석하기 위한 노트. (계기: [[Smart-Infinity]]를 읽다가 "원칙이 원래 이렇게 많나?" → 기법과 원칙을 분리하면서 정리)

> [!warning] 정직한 범위 — "4개로 항상 모든 것"은 아니다
> 아래 **성능 4렌즈**는 *performance 중심 아키텍처 논문*엔 거의 완전하다. 하지만 **correctness/verification**(예: 발표의 [[WOFS]] formal proof, [[Ananke]] fault tolerance), **abstraction**, **security**, **cost/economics** 축은 이 4개로 안 잡힌다. 그런 논문엔 아래 **Tier 2 렌즈**가 필요하다. 렌즈는 *분류 도구가 아니라 이해 도구* — 한 기법이 여러 렌즈에 걸치는 게 정상이다.

---

## Tier 1 — 성능 렌즈 (Hennessy & Patterson류 quantitative principles)

| 원칙 | 한 줄 뜻 | 던지는 질문 |
|---|---|---|
| **① Amdahl's Law** | 지배적 비용(common case)을 공략, 병목이 speedup을 결정 | "전체 시간의 어디가 지배적인가? 그걸 줄였나?" |
| **② Locality** | 데이터를 재사용·근처에서 처리 (move compute to data) | "연산을 데이터 가까이 옮길 수 있나?" |
| **③ Parallelism** | 동시에 처리 (DLP/TLP/ILP, scale-out) | "무엇을 병렬화·확장 가능한 자원으로 옮기나?" |
| **④ Memory hierarchy + Trade-off** | 계층을 활용하고, 무언가를 팔아 무언가를 산다 | "어떤 계층에 담나? 무엇(용량·정확도·비용)을 팔아 무엇(대역폭·속도)을 사나?" |

> [!note] 마스터 원칙은 ①
> 많은 시스템 논문은 사실상 **①의 반복**이다: *지배 병목 제거 → 다음 병목 노출 → 또 제거.* ②③④는 그 병목을 벗기는 **수단**.

---

## Tier 2 — 비-성능 렌즈 (성능만으론 안 잡히는 축)

| 원칙 | 한 줄 뜻 | 언제 필요 |
|---|---|---|
| **⑤ Abstraction / Indirection** | 한 겹의 간접층으로 문제를 분리 ("모든 문제는 indirection으로") | API·인터페이스·가상화 논문 |
| **⑥ Mechanism / Policy 분리** | "무엇을 하는가"와 "어떻게 정할까"를 분리 | 정책 유연성이 기여인 논문 |
| **⑦ Correctness / Correct-by-construction** | 성능이 아니라 *틀리지 않음*을 설계로 보장 | crash consistency·formal proof·fault tolerance ([[WOFS]]·[[Ananke]]·[[DJFS]]) |
| **⑧ End-to-end argument** | 기능은 끝단이 책임, 하위 계층은 최소·보조만 | 계층 경계 설계 논문 |
| **⑨ Cost / Economics** | 성능/달러, 실용성, 배포 가능성 | off-the-shelf vs custom, 상용화 논문 |

---

## Worked example — [[Smart-Infinity]]를 4렌즈로

> [!example] performance 논문이라 Tier 1로 거의 다 설명됨
> | 렌즈 | Smart-Infinity의 인스턴스 |
> |---|---|
> | **① Amdahl** | 학습 시간의 88%가 데이터 전송 → SmartUpdate로 6M 제거 → 드러난 잔여 병목(공유 링크 2M)을 SmartComp로 또 제거 (병목 순차 벗기기) |
> | **② Locality** | update를 host CPU가 아니라 데이터(optimizer state)가 있는 **CSD 내부 FPGA**에서 = near-data processing |
> | **③ Parallelism** | **여러 CSD**의 aggregate 내부 대역폭 선형↑ (공유 interconnect 대신), FPGA 내 **SIMD**(AXPBY) |
> | **④ Hierarchy + Trade-off** | SSD→DRAM→BRAM **tiling**(subgroup) / **DRAM 용량을 팔아 대역폭을 삼**(double-buffer overlap) · **정확도 약간 팔아 트래픽을 삼**(SmartComp lossy) |

추가로 걸치는 Tier 2: **⑨ Cost** — 상용 SmartSSD + DeepSpeed drop-in(feasibility-by-building) / **⑦ Correctness** — SmartUpdate는 baseline과 algorithmically identical(정확도 보존).

---

## 재사용법 — 같은 렌즈로 다른 논문 읽기

> [!tip] 원칙은 고정, 인스턴스만 바뀐다
> - **CXL disaggregation** ([[DirectCXL]]·[[TrainingCXL]]): ② Locality(원격 메모리를 load/store로) + ③ Parallelism(pooling) + ④ Trade-off(latency ↔ capacity)
> - **KV cache offload** ([[Efficiently Scaling Transformer Inference]] 등): ① Amdahl(memory-bandwidth 병목) + ④ Hierarchy(tier)
> - **[[WOFS]]** (formal proof): Tier 1로는 거의 안 잡힘 → **⑦ Correct-by-construction**이 주 렌즈. ← *성능 4렌즈의 한계를 보여주는 사례*

즉 논문을 만나면: (1) performance 논문인가 correctness/abstraction 논문인가 판별 → (2) 해당 렌즈로 기법을 분해 → (3) "이 논문의 새로움 = 어느 렌즈의 어떤 인스턴스인가"로 요약.

## 연결
- 방법론: feasibility-by-building(내 발표 프레임) — Tier 2의 ⑨ Cost·⑦ Correctness와 연결
- 적용 사례: [[Smart-Infinity]] · [[CAMEL Lab CXL 연구 계보]]
- 허브: [[Concepts]]
- 내 연구 렌즈(memory-system architecture): 공유 vs 전용 대역폭(② ③) = CXL의 shared fabric vs per-device path 문제
