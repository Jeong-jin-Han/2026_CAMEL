---
title: "Design Principles — 논문을 읽는 고정 렌즈"
aliases: [Design Principles, 설계 원칙, 디자인 원칙, 논문 렌즈, 시스템 설계 원칙, Architecture Design Principles, 변하지 않는 것들]
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
> 아래 **성능 4렌즈**는 *performance 중심 아키텍처 논문*엔 거의 완전하다. 하지만 **correctness/verification**(예: [[WOFS]] formal proof, [[Ananke]] fault tolerance), **abstraction**, **security**, **cost/economics** 축은 이 4개로 안 잡힌다. 그런 논문엔 아래 **Tier 2 렌즈**가 필요하다. 렌즈는 *분류 도구가 아니라 이해 도구* — 한 기법이 여러 렌즈에 걸치는 게 정상이다.

---

## Tier 1 — 성능 렌즈 (Hennessy & Patterson류 quantitative principles)

| 원칙 | 한 줄 뜻 | 던지는 질문 |
|---|---|---|
| **① Amdahl's Law** | 지배적 비용(common case)을 공략, 병목이 speedup을 결정 | "전체 시간의 어디가 지배적인가? 그걸 줄였나?" |
| **② Data Movement 최소화** (Locality & Placement) | 이동은 비싸다: 재사용 있으면 데이터를 가까이(caching), 없으면 연산을 데이터로(NDP) — 쌍대 전략 | "재사용($I$)이 있나? 있으면 어느 계층에 캐싱, 없으면 연산을 어디로 보내나?" |
| **③ Parallelism** | 동시에 처리 (DLP/TLP/ILP, scale-out) | "무엇을 병렬화·확장 가능한 자원으로 옮기나?" |
| **④ Memory hierarchy + Trade-off** | 계층을 활용하고, 무언가를 팔아 무언가를 산다 | "어떤 계층에 담나? 무엇(용량·정확도·비용)을 팔아 무엇(대역폭·속도)을 사나?" |

> [!note] 마스터 원칙은 ①
> 많은 시스템 논문은 사실상 **①의 반복**이다: *지배 병목 제거 → 다음 병목 노출 → 또 제거.* ②③④는 그 병목을 벗기는 **수단**.

---

## 원칙별 심화 — 왜 안 변하나 · 살아남은 격변

### ① Amdahl's Law — 모든 최적화의 브레이크
시스템의 일부(비율 $f$)만 $s$배 빨라질 때
$$\text{Speedup} = \frac{1}{(1-f) + \frac{f}{s}}$$
직렬 부분 $(1-f)$이 상한을 결정한다. $s \to \infty$여도 speedup은 $\frac{1}{1-f}$를 못 넘는다.

- **왜 안 변하나**: 이건 기술이 아니라 **산수**다. 병렬화든 near-data든 가속기든, "일부만 빠르게 하면 전체 이득은 그 일부의 비중에 묶인다"는 명제는 반도체 공정과 무관하게 참
- **살아남은 격변**: 1967년 단일 프로세서 시대에 나온 법칙이 → 2000년대 multicore 전환기에 "코어를 늘려도 왜 선형으로 안 오르나"의 답이 되고 → 지금 near-data/PIM 시대에 "데이터 이동을 줄이는 게 얼마나 이득인가"의 기준이 된다. 기술이 세 번 바뀌는 동안 법칙은 글자 하나 안 바뀜 — **invariance의 교과서적 사례**

**따름 원칙 — Focus on the Common Case**: 자주 일어나는 경우를 빠르게, 드문 경우는 정확하게만 = Amdahl의 실천판(흔한 경우가 $f$ 큰 부분). 자원이 유한한 한 "모든 걸 다 빠르게"는 불가능하므로 이 우선순위 명제는 영구적이다. branch predictor·cache·TLB·fast/slow path 분리 — 구현 대상은 바뀌어도 판단 기준은 그대로. (Lampson의 "separate common and rare code paths" hint와 같은 뿌리, 아키텍처는 이를 정량적으로 다룸)

### ② Data Movement 최소화 — locality(재사용)와 placement(NDP)의 상위 원칙

상위 불변 원칙: **데이터 이동은 비싸다 — 이동을 최소화하라.** 그 아래 두 갈래 전략이 쌍대(dual)를 이룬다.

**(a) Locality → caching** — 프로그램은 최근 쓴 데이터를(temporal), 그 근처를(spatial) **다시 쓴다**(재사용 성질).
- **왜 안 변하나**: 하드웨어가 아니라 **프로그램(과 그걸 짜는 인간)의 성질** — 루프·배열 순회·스택 프레임이 locality를 만든다
- **살아남은 격변**: 캐시(SRAM) → DRAM → NVM → CXL far memory. 매체·계층 수는 늘어도 **계층을 두는 이유는 항상 locality**
- 전략 논리: 재사용이 있으니 **데이터를 연산 가까이 복사하는 비용이 재사용으로 상환**된다

**(b) Placement → near-data** — 재사용이 **없으면**(element-wise·streaming, $I$ 작음) 복사가 순손실 → **아예 안 옮기고 연산을 데이터 쪽으로** 보낸다.
- MapReduce의 "data locality" 스케줄링·NUMA-aware 배치·NDP가 이 갈래 — 관용적으로 "locality"라 불리지만 (a)의 재사용 원칙과는 **다른 것**(affinity/배치)

> [!warning] NDP를 "Locality 원칙의 인스턴스"라 부르지 말 것 — 인과가 거꾸로다
> [[Smart-Infinity]]가 update를 CSD로 내린 건 locality가 있어서가 아니라 **없어서**다: element-wise라 재사용 0 → caching 무력 → 연산을 옮김. **$I$(arithmetic intensity) = locality의 정량 지표**이고, $I$가 작을 때의 해법이 NDP다 ([[Roofline & FLOPs]]와 맞물림). 두 전략을 고르는 질문: "이 워크로드에 재사용이 있는가?"

### ③ Parallelism — 여러 층위에서 반복되는 한 아이디어
독립적인 일을 동시에. 층위: **ILP**(pipeline·superscalar·OoO) · **DLP**(SIMD·vector·GPU) · **TLP**(multicore) · **RLP**(데이터센터 규모) · 메모리의 bank interleaving · 스토리지의 RAID — **같은 원리의 다른 표현**.

- **왜 안 변하나**: "독립 작업은 겹쳐 처리하면 빠르다"는 물리 법칙에 가깝다
- **살아남은 격변 (여기가 미묘하고 중요)**: ~2004년엔 주파수↑+ILP 심화로 성능을 뽑다가 → Dennard scaling 종료 후 **무게중심이 ILP → TLP/DLP로 이동** → 지금은 가속기·PIM으로 또 이동 중. 즉 **"병렬성을 활용한다"는 원칙은 불변, "어디서 뽑는가"는 기술 종속** ← 원칙(invariant)과 구현(technology-dependent)을 분리해서 말할 수 있어야 논지가 반박을 견딘다
- 경험 연결: MIPS 5-stage pipeline + forwarding/hazard unit RTL 구현 = ILP의 가장 기본형 — "병렬성을 뽑으려니 hazard가 생기고 그 대가를 설계로 치른다"는 trade-off를 직접 경험

### (②-b의 현재형) Near-data / PIM — placement 갈래의 최신 재현
현대 시스템은 데이터를 연산 쪽으로 옮기도록 설계돼 있는데, **데이터 이동 자체가 에너지·시간의 지배적 비용**이 됐다(memory wall / von Neumann bottleneck) → 연산을 데이터 쪽으로 = near-data processing / PIM. (processing-**near**-memory: 컨트롤러·3D-stacked logic layer에 연산 유닛 / processing-**using**-memory: 메모리 칩의 아날로그 특성으로 in-situ 병렬 연산)

> [!warning] 논지의 함정 — near-data는 "고전 원칙"이 아니다
> near-data processing 자체는 ①~④ 같은 수십 년 된 고전 원칙이 **아니라** 비교적 최근의 memory-centric 운동이다. 불변인 것은 그 *동기*(**data movement 최소화**, ②의 상위 원칙)이고, NDP는 그중 placement 갈래(②-b)가 CXL·PIM이라는 새 기술에서 재현된 것이다 — locality(②-a, 재사용)의 확장이 아니라 **재사용이 없을 때의 쌍대 전략**임에 주의. → 정확한 표현: **"오래된 원칙(data movement 최소화)이 새 기술(CXL·PIM)에서 어떻게 재현되는가"의 사례.** [[TrainingCXL]]·ScalePool·PIM 연구가 전부 이 원칙의 현재형 — "오래된 원칙이 최신 하드웨어에서 다시 문제로 떠오른다"는 게 CAMEL에 끌린 이유와 정확히 연결.

### (①~④의 통합) Roofline — 렌즈들을 하나로 묶는 정량 그림
달성 성능 $= \min(\text{peak compute},\ I \times \text{bandwidth})$. arithmetic intensity $I$ 축 하나로 **compute-bound vs memory-bound**를 판정 — parallelism(peak↑)·locality($I$↑)·data movement(bandwidth 지붕)를 **한 그림에서 동시에** 본다. CAMEL 논문 대부분의 "왜 이 워크로드에 이 접근인가"의 답이 roofline상의 위치(bandwidth-bound: embedding·graph·sparse·element-wise update). 상세·그래프·A100 수치: **[[Roofline & FLOPs]]**.
- ⚠️ **적용 단위 주의**: Roofline 1장 = 한 하드웨어 × 한 phase. **여러 phase의 합성(end-to-end speedup)은 ① Amdahl의 일** — end-to-end 숫자를 roofline 평면에 찍는 건 범주 착오. 실수 목록·판정 체크리스트: [[Roofline & FLOPs]] §5.

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
> | **② Data Movement 최소화** | update는 element-wise라 재사용 0($I{\approx}0.4$) → caching(②-a) 무력 → **placement(②-b)**: 데이터(optimizer state)가 있는 **CSD 내부 FPGA**로 연산을 보냄 = NDP |
> | **③ Parallelism** | **여러 CSD**의 aggregate 내부 대역폭 선형↑ (공유 interconnect 대신), FPGA 내 **SIMD**(AXPBY) |
> | **④ Hierarchy + Trade-off** | SSD→DRAM→BRAM **tiling**(subgroup) / **DRAM 용량을 팔아 대역폭을 삼**(double-buffer overlap) · **정확도 약간 팔아 트래픽을 삼**(SmartComp lossy) |

추가로 걸치는 Tier 2: **⑨ Cost** — 상용 SmartSSD + DeepSpeed drop-in(feasibility-by-building) / **⑦ Correctness** — SmartUpdate는 baseline과 algorithmically identical(정확도 보존).

---

## 재사용법 — 같은 렌즈로 다른 논문 읽기

> [!tip] 원칙은 고정, 인스턴스만 바뀐다
> - **CXL disaggregation** ([[DirectCXL]]·[[TrainingCXL]]): ② Data movement(원격 메모리를 복사 없이 load/store로) + ③ Parallelism(pooling) + ④ Trade-off(latency ↔ capacity)
> - **KV cache offload** ([[Efficiently Scaling Transformer Inference]] 등): ① Amdahl(memory-bandwidth 병목) + ④ Hierarchy(tier)
> - **[[WOFS]]** (formal proof): Tier 1로는 거의 안 잡힘 → **⑦ Correct-by-construction**이 주 렌즈. ← *성능 4렌즈의 한계를 보여주는 사례*

즉 논문을 만나면: (1) performance 논문인가 correctness/abstraction 논문인가 판별 → (2) 해당 렌즈로 기법을 분해 → (3) "이 논문의 새로움 = 어느 렌즈의 어떤 인스턴스인가"로 요약.

## 근거 출처 (읽고 인용 대비)
- **고전 4원칙**: Hennessy & Patterson, *Computer Architecture: A Quantitative Approach*, Ch.1
- **near-data / PIM**: O. Mutlu et al., *A Modern Primer on Processing in Memory* (arXiv:2012.03112) · Mutlu ETH 강의(YouTube, 무료)
- **Roofline**: Williams, Waterman, Patterson, *Roofline: An Insightful Visual Performance Model*, CACM 2009
- **시스템 설계 원칙(보조 계열)**: Saltzer & Kaashoek, *Principles of Computer System Design* (MIT OCW) · Lampson, *Hints for Computer System Design* (1983)

## 연결
- 방법론: feasibility-by-building(내 발표 프레임) — Tier 2의 ⑨ Cost·⑦ Correctness와 연결
- 적용 사례: [[Smart-Infinity]] · [[CAMEL Lab CXL 연구 계보]] · 정량 버전: [[Roofline & FLOPs]]
- 허브: [[Concepts]]
- 내 연구 렌즈(memory-system architecture): 공유 vs 전용 대역폭(② ③) = CXL의 shared fabric vs per-device path 문제
