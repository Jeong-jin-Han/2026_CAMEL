---
title: "Design Principles — 8 Great Ideas & 논문을 읽는 고정 렌즈"
aliases: [Design Principles, 설계 원칙, 디자인 원칙, 논문 렌즈, 시스템 설계 원칙, Architecture Design Principles, 변하지 않는 것들, 8 Great Ideas, 8대 아이디어, Patterson Hennessy]
type: concept
tags:
  - concept
  - concept/methodology
  - topic/design-principles
---

# Design Principles — 8 Great Ideas & 논문을 읽는 고정 렌즈

> [!tip] 이 노트의 목적
> 아키텍처의 **불변 원칙**을 고정 렌즈로 두고 여러 논문을 같은 축에서 비교·해석. 뼈대 = Patterson & Hennessy의 **"8 Great Ideas in Computer Architecture"**(*Computer Organization and Design* Ch.1). 컴퓨터가 60년간 갈아엎어져도 살아남은 8개 아이디어 — 이게 내 아키텍처 미학(**"규칙을 만들고 적용하는 것"**, [[user-research-identity]])의 정전이다.

> [!quote]- 📄 Patterson (2014, Elsevier)
> "These are eight great ideas that computer architects have invented in the last 60 years of computer design. They are so powerful they have lasted long after the first computer that used them, with newer architects demonstrating their admiration by imitating their predecessors."

---

## Part 1 — 8 Great Ideas (Patterson & Hennessy)

각 아이디어 = 아이콘 + 뜻 + Patterson의 아이콘 비유 + (해당되면) *왜 안 변하나*.

### 1. Design for Moore's Law — 변화를 겨냥해 설계하라
![[moore.png|190]]
- IC 자원은 18~24개월마다 2배(**Moore's Law**, Gordon Moore 1965 예측). 설계엔 수년이 걸리니, 착수 시점이 아니라 **완료 시점의 기술을 겨냥**해야 한다. Patterson의 비유: *"클레이 사격수처럼 지금이 아니라 앞을 쏜다."*
- 아이콘 = **우상향 그래프**(rapid change).
- ※ 최근 Moore 둔화·Dennard scaling 종료로 "공짜 속도"는 끝났지만, **"완성 시점을 겨냥하라"**는 메타 원칙은 유효 — 가속기·chiplet·CXL이 그 대응.

### 2. Use Abstraction to Simplify Design — 추상화로 복잡도를 감춰라
![[abstraction.png|150]]
- 자원이 Moore로 폭증하는 만큼 **설계 생산성**도 같이 올려야 함. 방법 = **여러 표현 수준(abstraction)**을 두어, 하위 디테일을 감추고 상위엔 단순 모델만 노출. (ISA ↔ microarchitecture, HLL ↔ assembly)
- 아이콘 = **추상화 그림**(하위 디테일을 숨긴 얼굴).

### 3. Make the Common Case Fast — 흔한 경우를 빠르게
![[common-case.png|190]]
- 드문 경우보다 **자주 일어나는 경우**를 빠르게 = 성능에 더 이득. 역설적으로 common case가 rare case보다 **단순**해 최적화도 쉽다. 단 "무엇이 common인지"는 **측정·실험**으로만 알 수 있다.
- 아이콘 = **스포츠카**(가장 흔한 여행 = 1~2인 → 빠른 스포츠카가 빠른 미니밴보다 만들기 쉬움).
- → 이게 **Amdahl's Law의 실천판** (아래 [심화 §Amdahl](#-amdahls-law--모든-최적화의-브레이크)).

### 4. Performance via Parallelism — 병렬성으로 성능을
![[parallelism.png|190]]
- 독립적인 연산을 **동시에** 수행. 층위: **ILP**(superscalar·OoO)·**DLP**(SIMD/vector/GPU)·**TLP**(multicore)·**RLP**(datacenter) + 메모리 bank interleaving·스토리지 RAID.
- 아이콘 = **비행기의 다중 제트엔진**.
- → [심화 §Parallelism](#-parallelism--여러-층위에서-반복되는-한-아이디어)

### 5. Performance via Pipelining — 파이프라이닝
![[pipelining.png|150]]
- 병렬성의 특별한 패턴이라 별도 이름을 받음. 불 끄는 **bucket brigade**(사람 사슬로 물통을 넘김 — 각자 뛰어다니는 것보다 빠름)처럼, 단계를 **겹쳐** 처리량을 올린다.
- 아이콘 = **파이프 구간들**(각 구간 = 한 stage).
- 경험: MIPS 5-stage pipeline + forwarding/hazard unit을 RTL로 구현 — "병렬성을 뽑으니 hazard가 생기고 그 대가를 설계로 치른다"는 trade-off 직접 체득.

### 6. Performance via Prediction — 예측
![[prediction.png|150]]
- *"허락을 구하기보다 용서를 구하는 게 낫다"* — 확실해질 때까지 기다리지 말고 **추측하고 먼저 시작**. 성립 조건: ① 오예측(misprediction) 복구가 **싸고** ② 예측이 **충분히 정확**할 때. (branch prediction이 대표)
- 아이콘 = **점쟁이의 수정구슬**.
- ※ **[[PF-LLM - Large Language Model Hinted Hardware Prefetching|PF-LLM]]**이 이 갈래의 최신형 — LLM이 prefetch 정책을 offline 예측 → HW가 적용. "규칙을 예측해 만들고 적용."

### 7. Hierarchy of Memories — 메모리 계층
![[hierarchy.png|170]]
- 빠르고·크고·싼 메모리를 동시에 원하는 모순을 **계층**으로 해결. 위 = 빠르고 작고 비쌈, 아래 = 느리고 크고 쌈. **캐시**가 "main memory가 최상위만큼 빠르고 최하위만큼 크고 싸다"는 착각(illusion)을 준다.
- 아이콘 = **층진 삼각형**(폭 = 용량, 높이 = 속도·비용/bit).
- → [심화 §Data Movement](#-data-movement-최소화--locality재사용와-placementndp의-상위-원칙) · 정량 [[Roofline & FLOPs]]

### 8. Dependability via Redundancy — 중복으로 신뢰성을
![[dependability.png|190]]
- 컴퓨터는 빠를 뿐 아니라 **믿을 수 있어야** 한다. 물리 부품은 반드시 고장 나므로, **중복(redundancy) 부품**으로 고장 시 대체하고 검출한다.
- 아이콘 = **트레일러 트럭의 이중 타이어**(하나 터져도 주행 → 정비소로 가서 redundancy 복구).
- → **correctness/reliability 렌즈**와 직결: RAID, ECC, replication, 그리고 [[WOFS]]·[[Ananke]]의 crash recovery.

---

## Part 2 — 8대를 논문 읽는 렌즈로

논문을 만나면 각 아이디어를 **질문**으로 던진다:

| # | 아이디어 | 던지는 질문 | 대표 논문 |
|---|---|---|---|
| 1 | Moore's Law | "완성 시점 기술을 겨냥했나? 확장성은?" | 신기술 채택 논문 |
| 2 | Abstraction | "어떤 간접층으로 복잡도를 감췄나?" | 인터페이스·가상화 |
| 3 | Common Case Fast | "지배적 경우가 뭐고 그걸 빠르게 했나?" ← **Amdahl** | 거의 모든 성능 논문 |
| 4 | Parallelism | "무엇을 병렬화·확장 자원으로 옮기나?" | [[Smart-Infinity]] |
| 5 | Pipelining | "단계를 겹쳐 throughput을 올렸나?" | 파이프라인·overlap |
| 6 | Prediction | "추측+복구가 대기보다 싼가?" | [[PF-LLM - Large Language Model Hinted Hardware Prefetching|PF-LLM]] |
| 7 | Hierarchy | "어느 계층에 담고 무엇을 팔아 무엇을 사나?" | tiered memory·CXL |
| 8 | Dependability | "무엇을 중복시켜 어떤 고장을 견디나?" | [[WOFS]]·[[Ananke]]·RAID |

> [!warning] 8대로도 안 잡히는 축 (추가 렌즈)
> 8대는 **성능(3~7)+생산성(1,2)+신뢰성(8)**을 덮지만, 아래는 별도로 봐야 한다:
> - **⑨ Cost / Economics** — 성능/달러, 배포 가능성 (off-the-shelf vs custom). [[Roofline & FLOPs|Roofline]]의 GFLOPS/$·feasibility-by-building.
> - **⑩ Mechanism / Policy 분리** — "무엇을 하나" vs "어떻게 정하나".
> - **⑪ End-to-end argument** — 기능은 끝단 책임, 하위는 최소·보조.
> - **⑫ Correct-by-construction** — #8(redundancy로 *견딤*)을 넘어, 설계로 *틀리지 않음을 보장*(formal proof). [[WOFS]]·[[SquirrelFS - using the Rust compiler to check file-system crash consistency|SquirrelFS]]. ← 내 dream.

---

## Part 3 — 원칙별 심화 (왜 안 변하나 · 살아남은 격변)

### ▸ Amdahl's Law — 모든 최적화의 브레이크 (idea 3의 정량형)
시스템의 일부(비율 $f$)만 $s$배 빨라질 때
$$\text{Speedup} = \frac{1}{(1-f) + \frac{f}{s}}$$
직렬 부분 $(1-f)$이 상한을 결정. $s \to \infty$여도 $\frac{1}{1-f}$를 못 넘는다.
- **왜 안 변하나**: 기술이 아니라 **산수**. "일부만 빠르게 하면 전체 이득은 그 일부의 비중에 묶인다"는 공정과 무관하게 참.
- **살아남은 격변**: 1967(단일 프로세서)→2000s multicore("코어 늘려도 왜 선형 아닌가")→now near-data("데이터 이동 줄이는 게 얼마나 이득인가"). 세 번 바뀌는 동안 글자 하나 안 바뀜 = **invariance의 교과서**.

### ▸ Data Movement 최소화 — locality(재사용)와 placement(NDP)의 상위 원칙 (idea 7의 확장)
상위 불변 원칙: **데이터 이동은 비싸다 — 이동을 최소화하라.** 아래 두 갈래가 쌍대(dual):
- **(a) Locality → caching**: 프로그램은 최근·근처 데이터를 **다시 쓴다** → 연산 가까이 복사(복사 비용이 재사용으로 상환). 매체는 SRAM→DRAM→NVM→CXL로 바뀌어도 **계층을 두는 이유는 항상 locality**.
- **(b) Placement → near-data(NDP)**: 재사용이 **없으면**(element-wise, $I$ 작음) 복사가 순손실 → **연산을 데이터 쪽으로** 보냄.

> [!warning] NDP를 "Locality의 인스턴스"라 부르지 말 것 — 인과가 거꾸로
> [[Smart-Infinity]]가 update를 CSD로 내린 건 locality가 있어서가 아니라 **없어서**다(재사용 0 → caching 무력 → 연산 이동). **$I$(arithmetic intensity) = locality의 정량 지표**, $I$ 작을 때의 해법이 NDP. 판별 질문: "이 워크로드에 재사용이 있나?" (near-data 자체는 고전 원칙이 아니라, *data movement 최소화*라는 오래된 동기가 CXL·PIM에서 재현된 것.)

### ▸ Parallelism — 여러 층위에서 반복되는 한 아이디어 (idea 4~5)
- **왜 안 변하나**: "독립 작업은 겹쳐 처리하면 빠르다"는 물리 법칙에 가까움.
- **살아남은 격변(중요)**: ~2004 주파수↑+ILP → Dennard 종료 후 **무게중심 ILP→TLP/DLP 이동** → now 가속기·PIM. 즉 **"병렬성 활용"은 불변, "어디서 뽑나"는 기술 종속** — 원칙(invariant)과 구현(technology-dependent)을 분리해 말할 수 있어야 반박을 견딘다.

### ▸ Roofline — idea 3·4·7을 한 그림에 통합
달성 성능 $= \min(\text{peak compute},\ I \times \text{bandwidth})$. $I$ 축 하나로 **compute-bound vs memory-bound** 판정 — parallelism(peak↑)·locality($I$↑)·hierarchy(bandwidth 지붕)를 동시에 본다. 상세·그래프·A100 수치·실수 방지 체크리스트: **[[Roofline & FLOPs]]**.
- ⚠️ 적용 단위: Roofline 1장 = **한 HW × 한 phase**. 여러 phase 합성(end-to-end)은 **Amdahl의 일** — 섞지 말 것.

---

## Worked example — [[Smart-Infinity]]를 8대 아이디어로

> [!example]
> | 아이디어 | Smart-Infinity의 인스턴스 |
> |---|---|
> | **3. Common Case Fast (Amdahl)** | 학습 시간 88%가 데이터 전송 → SmartUpdate로 6M 제거 → 잔여 병목(2M)을 SmartComp로 또 제거 (병목 순차 벗기기) |
> | **7. Hierarchy + Data Movement** | update는 재사용 0($I{\approx}0.4$) → caching 무력 → **placement**: optimizer state가 있는 **CSD 내부 FPGA**로 연산 이동(NDP) / SSD→DRAM→BRAM tiling |
> | **4. Parallelism** | **여러 CSD**의 aggregate 내부 대역폭 선형↑, FPGA 내 SIMD(AXPBY) |
> | **1. Moore/확장** · **⑨ Cost** | 상용 SmartSSD + DeepSpeed drop-in = feasibility-by-building, GFLOPS/$ 이득 |
> | **⑫ Correctness** | SmartUpdate는 baseline과 algorithmically identical(정확도 보존) |

---

## 재사용법 — 같은 렌즈로 다른 논문
> [!tip] 원칙은 고정, 인스턴스만 바뀐다
> - **CXL disaggregation** ([[DirectCXL]]·[[TrainingCXL]]): 7 Hierarchy(복사 없이 load/store) + 4 Parallelism(pooling) + Trade-off(latency↔capacity)
> - **[[PF-LLM - Large Language Model Hinted Hardware Prefetching|PF-LLM]]**: 6 Prediction(정책 예측) + 2 Abstraction(LLM이 assembly를 해석)
> - **[[WOFS]]** (formal proof): 8 Dependability를 넘어 **⑫ Correct-by-construction**이 주 렌즈 — 8대 성능축의 한계를 보여주는 사례

논문을 만나면: (1) 성능 논문인가 correctness/abstraction 논문인가 → (2) 해당 렌즈로 기법 분해 → (3) "새로움 = 어느 아이디어의 어떤 인스턴스인가"로 요약.

## ★ rule-making 미학과의 연결
**8 Great Ideas = "아키텍트가 만들어 60년째 적용 중인 규칙"** = 내 아키텍처 미학(**"규칙을 만들고 적용하는 것"**)의 정전. 내가 끌린 모든 게 이 "규칙 만들기+적용"의 인스턴스다: Amdahl·Roofline(판정 규칙)·Five-Minute Rule(계층 결정 규칙)·correct-by-construction(틀리지 않음의 규칙)·[[H1 — 워크로드 특화로 multi-node coherence 줄이기|H1]]/[[H2 — CXL 위에서 PGAS 재해석|H2]]/[[H3 — DockerGPU, in-GPU control plane|H3]](coherence/분할 규칙). → 발표 [[Five-Minute Rule 40 Years Later - A First-Principles Revisit for Modern Memory Hierarchy|Five-Minute Rule]]·[[PF-LLM - Large Language Model Hinted Hardware Prefetching|PF-LLM]] 픽의 근거.

## 근거 출처
- **8 Great Ideas**: D. Patterson, *8 Great Ideas in Computer Architecture* (Elsevier Connect, 2014) — *Computer Organization and Design* Ch.1 발췌. 원문: https://www.elsevier.com/connect/8-great-ideas-in-computer-architecture
- **정량 원칙**: Hennessy & Patterson, *Computer Architecture: A Quantitative Approach*, Ch.1
- **Roofline**: Williams·Waterman·Patterson, CACM 2009 / **near-data**: O. Mutlu, *A Modern Primer on PIM* (arXiv:2012.03112)
- **시스템 설계 원칙**: Saltzer & Kaashoek (MIT OCW) · Lampson, *Hints for Computer System Design* (1983)

## 연결
- 방법론: feasibility-by-building — ⑨ Cost·⑫ Correctness와 연결
- 적용: [[Smart-Infinity]] · [[PF-LLM - Large Language Model Hinted Hardware Prefetching|PF-LLM]] · [[CAMEL Lab CXL 연구 계보]] · 정량: [[Roofline & FLOPs]]
- 허브: [[Concepts]]
