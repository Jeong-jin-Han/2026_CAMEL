---
title: "Cache in Hand: Expander-Driven CXL Prefetcher for Next Generation CXL-SSDs"
aliases: [Cache in Hand]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---
# Cache in Hand: Expander-Driven CXL Prefetcher for Next Generation CXL-SSDs

> **Source PDF**: [Cache in Hand.pdf](Cache in Hand.pdf)
> **Authors**: Miryeong Kwon, Sangwon Lee, Myoungsoo Jung (KAIST CAMEL Lab / Panmnesia, inc.)
> **Venue / Year**: ACM HotStorage '23 (Boston, MA, USA · 2023-07-09)
> **arXiv / DOI**: [10.1145/3599691.3603406](https://doi.org/10.1145/3599691.3603406)
> **Length**: 7 pages (본문 p.24–30)
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL Lab CXL 연구 계보 이해 — Phase 2(2023)에서 CXL-SSD가 "topology-aware" 방향으로 진화하는 지점 확인. **switch-network 위치에 따른 latency variation**과 그 대응(prefetch timing)이 내 접근 A(profiling 기반 배치)와 같은 문제·다른 layer를 다루는지 확인.

계보: [CAMEL Lab CXL 연구 계보](../CAMEL Lab CXL 연구 계보.md)

---

## 📋 목차

- [TL;DR](#tldr)
- [Core thesis](#core-thesis)
- [Why this matters to me](#why-this-matters-to-me)
- [Structure overview](#structure-overview)
- [Section notes](#section-notes)
- [Key vocabulary (for own writing)](#key-vocabulary-for-own-writing)
- [Citable quantitative data](#citable-quantitative-data)
- [🎯 Strategic anchor](#-strategic-anchor)
- [Connection to my research direction](#connection-to-my-research-direction)
- [Open questions / gaps](#open-questions--gaps)
- [References worth following up](#references-worth-following-up)
- [Personal annotations](#personal-annotations)

---

## TL;DR

CXL-SSD는 대용량 byte-addressable 메모리를 주지만 backend media(SCM/flash)가 DRAM보다 느려 **long read latency**가 문제다. 이 position paper는 **ExPAND**라는 prefetcher를 제안하는데, host CPU가 하던 LLC prefetch 결정을 **CXL-SSD(expander) 쪽으로 offload**한다. 핵심은 두 가지다: (1) EP의 넉넉한 form factor·연산력을 활용해 on-chip CPU엔 못 넣는 무거운 **heterogeneous ML prefetcher**(transformer 기반 address predictor)를 SSD 안에서 돌리고, (2) **multi-tiered CXL switch에서 device 위치마다 달라지는 end-to-end latency**를 PCIe enumeration으로 알아내 **prefetch timeliness**(언제 미리 가져올지)를 device별로 정확히 맞춘다. host-EP 통신은 CXL.mem 위에 custom opcode(MemRdPC, BISnpData)와 CXL 3.0 **back-invalidation(BI)**을 얹어 구현. graph workload에서 no-prefetch 대비 **3.5×** 향상.

---

## Core thesis

> "We present ExPAND, an expander-driven CXL prefetcher that offloads last-level cache (LLC) prefetching from host CPU to CXL-SSDs." (Abstract, p.24)

추가 설명: prefetcher를 host의 hardware logic size 제약에서 해방시키려면 결정 주체를 CXL-SSD로 옮겨야 한다는 것이 1차 논지. 2차 논지는, CXL의 multi-tiered switching이 만드는 **latency variation** 때문에 정확도 90% 이상 oracle prefetcher조차 timing을 못 맞추면 무력해지므로, prefetcher가 **CXL topology를 인지**해 device별 end-to-end latency를 반영해야 한다는 것.

---

## Why this matters to me

내 방향(메모리 시스템 아키텍처: CXL disaggregation, multi-node coherence, PGAS-over-CXL)에서 **multi-tiered switch fabric 위 device의 물리적 위치가 latency를 결정한다**는 사실은 핵심 제약이다. 이 논문은 그 제약을 **정량화**(switch layer +4마다 성능 1% 저하, §3 p.26)하고, "topology를 런타임에 discovery해서 latency-aware하게 대응한다"는 하나의 답을 보여준다. 나의 접근 A는 같은 문제를 *다른 layer*—**profiling 기반 데이터 배치**—로 푼다. ExPAND가 "언제 prefetch할지"를 topology-aware하게 조정한다면, 나는 "어디에 데이터를 둘지"를 topology/profiling-aware하게 정하는 셈이다. 즉 이 논문은 내 문제 정의의 **정당성 근거이자 상보적 baseline**이다. 또한 CXL.mem opcode를 확장하고 real CXL RTL로 cycle-level 검증한 방식은 내 feasibility-by-building 성향과 정확히 맞닿는다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| Abstract / 1 | Introduction | p.24–25 | CXL-SSD의 long read latency 문제. host prefetcher의 2대 한계: (i) hardware logic size 제약, (ii) switch 위치별 latency variation |
| 2 | New Cache/Memory Hierarchy | p.25–26 | CXL 3종 프로토콜, memory pooling, BI(back-invalidation), multi-tiered switching·VH(virtual hierarchy) |
| 3 | Motivation and Challenges | p.26 | locality가 낮으면 CXL-SSD가 LocalDRAM 대비 738% 느림; 정확도>90%면 거의 동등; 그러나 switch latency가 oracle prefetcher도 무력화 |
| 4 | Expander-Driven Prefetching | p.26–27 | ExPAND 구조 = **reflector(host) + decider(EP)**; decider는 address predictor(transformer)+timing predictor(rule) |
| 5 | Cross-Layer Interaction on CXL | p.27–28 | topology-aware timeliness(PCIe enumeration + DOE/DSLBIS); CXL.mem 위 양방향 통신(MemRdPC↓, BISnpData↑) |
| 6 | Evaluation | p.28–29 | gem5+SimpleSSD, graph workload 4종; 3.5×(vs NoPrefetch), 2.1×(rule), 1.5×(ML); timeliness로 4.1× |
| 7 | Conclusion | p.29 | offload + topology-aware timeliness로 CXL-SSD 의존도↓ |

---

## Section notes

### §1 Introduction (p.24–25)

CXL-SSD는 SCM(PRAM·Z-NAND·XL-Flash)을 byte-addressable하게 CXL pool에 붙여 대용량 확장을 노리지만 backend media가 느리다. 업계 PoC는 SSD-side DRAM buffer를 내부 캐시로 써 write latency는 잡지만 **SCM의 긴 read latency는 못 가린다**. 저자들은 CXL-SSD가 host storage stack 없이 load/store를 직접 서빙해야 하므로 **host 실행 behavior와 CPU cache hierarchy를 이해**해야 한다고 지적한다. host-side prefetcher를 그대로 쓰기엔 두 가지 미해결 난제가 있다.

> "i) hardware logic size constraints in handling a wide range of memory access patterns ... and ii) latency variations experienced by different CXL-SSDs located in diverse positions within the CXL switch network." (§1, p.24)

rule-based(spatial/temporal) prefetcher는 LLC급 수십 MB 저장공간을 요구해 CPU 안에 못 넣고, 그래서 현대 CPU는 단순 stream prefetcher를 쓰는데 이건 CXL latency를 못 가린다.

### §2 New Cache/Memory Hierarchy (p.25–26)

CXL 3종(CXL.io, CXL.cache, CXL.mem) 정리. 핵심: **CXL.cache 없이 CXL.mem+CXL.io만으로도** EP를 cacheable memory space에 매핑해 로컬 메모리처럼 pooling 가능(PCIe는 noncacheable에만 붙는 것과 대조). §2.2에서 두 가지 CXL 3.0 기능을 끌어온다: (1) **back-invalidation(BI)** — CXL.cache의 무거운 coherence 오버헤드 없이, EP가 host cache line을 autonomously invalidate/back-snoop; (2) **multi-tiered switching** — switch가 다른 switch에 연결돼 VH(virtual hierarchy)당 device 수를 크게 늘림(최대 4K).

> "the recent implementation of back-invalidation (BI) in CXL 3.0 [16] allows CXL.mem to back-snoop the host's cache lines. This feature enables EPs to autonomously invalidate host cache lines ..." (§2.2, p.25)

### §3 Motivation and Challenges (p.26)

**Locality impact.** Apex-map 벤치마크로 spatial(L)·temporal(α) locality를 조절. 낮은 locality에서 CXL-SSD는 LocalDRAM 대비 평균 738% 느리지만, locality가 높으면(α≤0.01, L≥16) 데이터가 주로 LLC에서 로드돼 **단 35% 차이**로 좁혀진다. 즉 저자 주장: 기존 CXL-SSD 연구는 internal DRAM cache 최적화에만 집중했지만, 진짜 관건은 **cache hit rate 향상**이다.

**Prefetching + latency variation.** 정확도가 90%를 넘으면 CXL-SSD latency가 LocalDRAM에 근접(Figure 2a). 그러나 multi-tiered switching이 유발하는 latency variation 앞에선 oracle prefetcher조차 무력하다 — switch 처리시간을 고려 안 하면 데이터가 CPU가 필요한 시점에 도착하지 못해 hit이 miss로 바뀐다.

> "With each increment by 4 in the switch layer, there is a 1% decrease in performance. Thus, deeper switch layers would result in even more significant performance degradation." (§3, p.26)

### §4 Expander-Driven Prefetching (p.26–27)

rule-based는 irregular 패턴에서 정확도 9~76%로 부족하고, ML prefetcher는 정확하지만 model·metadata 저장공간 때문에 on-chip CPU에 못 넣는다. **해법: 결정을 EP로 위임.** ExPAND는 두 컴포넌트:

- **reflector** (host-side, CXL root complex + LLC controller): decider에 PC와 switch depth를 공급하고 prefetch 결과를 받아 host에 반영. **16KB buffer**로 prefetch된 cache line update를 로깅. 각 host LLC controller가 먼저 이 buffer를 확인 → 있으면 CXL-SSD pool 전체를 순회 안 하고 즉시 서빙.
- **decider** (EP-side, CXL-SSD controller): heterogeneous ML prefetcher. 제공받은 PC+주소로 prefetch 위치를 정하고 데이터를 reflector buffer로 전송, online fine-tuning용 이력 유지.

§4.2 decider의 두 predictor:
- **address predictor**: multi-modality **transformer** sequence model + multi-modality attention network(PC↔access pattern 관계 학습) + **decision tree classifier**로 실행 phase 변화 감지.
- **timing predictor**: rule-based. **80B buffer**에 과거 arrival time을 저장하고 평균내 다음 요청 시각 예측. LLC hit로 요청이 안 보이면 reflector가 CXL.io로 hit event를 알려 보정.

### §5 Cross-Layer Interaction on CXL (p.27–28)

**§5.1 Topology-aware timeliness.** reflector가 **PCIe enumeration** 중 각 CXL-SSD의 switch level을 식별(switch는 자체 bus number를 가진 PCIe bridge이므로 host와 target 사이 switch 수를 셀 수 있음). device latency는 PCIe config space의 **DOE(data object exchange)** 안 **DSLBIS**(device scoped latency and bandwidth information structure)에서 추출. 하지만 이 raw latency는 switching으로 인한 variation을 반영 못 하므로, reflector가 **VH latency(RC↔target)를 더해 end-to-end latency를 계산**해 각 device config space에 기록. decider는 timing predictor 예측 시각에서 이 end-to-end latency를 빼서 정확한 prefetch timeliness를 얻는다.

**§5.2 Bidirectional communication.**
- **Downward (piggyback on CXL.mem M2S)**: LLC miss 시 reflector가 새 custom opcode **MemRdPC**(RwD의 13개 custom opcode 중 하나)로 현재 PC를 실어 보냄 → decider가 host 실행환경의 주소+PC 확보.
- **Upward (leveraging BI, S2M)**: prefetch 시점에 decider가 host on-chip buffer를 갱신해야 하는데 CXL.mem엔 그 기능이 없음. 그래서 새 BI opcode **BISnpData**(S2M의 10개 custom opcode 활용)를 도입해 payload로 prefetch 데이터를 실어 reflector buffer에 삽입.

### §6 Evaluation (p.28–29)

gem5 + SimpleSSD 풀시스템 시뮬레이션, CXL RTL 모듈은 real CXL end-to-end system(DirectCXL 계열 [36])으로 cycle-level 검증됨. Workload: **BFS, CC, PR, TC** (Amazon 상품 co-purchasing network). Baseline: Rule1(spatial [8]), Rule2(temporal [11]), ML1(LSTM [28]), ML2(transformer [27]).

결과: ExPAND는 NoPrefetch 대비 **3.5×**(geometric mean), rule-based 대비 **2.1×**, 기본 ML 대비 **1.5×**. Rule2(temporal)는 BFS에서만 잘 맞음(부모→자식 노드 접근이 record-and-replay와 궁합). Timeliness 효과: switch level 1→4에서도 ExPAND가 평균 **4.1×**(VH level 2~4) 유지.

### §7 Conclusion (p.29)

offloaded prefetching + 정확한 topology-aware timeliness로 CXL-SSD 의존도를 낮추고 graph 성능 3.5× 향상. 논문은 특허로 보호됨(ack). corresponding author: Myoungsoo Jung.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "offloads last-level cache prefetching from host CPU to CXL-SSDs"
- "latency variations experienced by different CXL-SSDs located in diverse positions within the CXL switch network"
- "reducing dependence on CXL-SSDs" / "direct host cache access for most data"

**Technical concepts:**
- "prefetch timeliness" (topology 기반 end-to-end latency로 보정된 prefetch 시각)
- "multi-tiered switching" / "virtual hierarchy (VH)"
- "back-invalidation (BI)" / "back-snoop the host's cache lines"
- "device scoped latency and bandwidth information structure (DSLBIS)" / "data object exchange (DOE)"
- "heterogeneous ML prefetcher" / "multi-modality transformer"

**Value language:**
- "enhancing cache hit rates is vital for improving user experience with CXL-SSDs"
- "leveraging the larger form factor and computing power of SSD EPs compared to on-chip CPUs"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo 금지):
> - "expander-driven CXL prefetcher" / "ExPAND"
> - "reflector and decider"
> - "Cache in Hand"
> - "prefetch timeliness"라는 조어를 내 것처럼 쓰는 것(반드시 이 논문 인용과 함께)

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract / §6, p.24·p.28 | graph app 성능 **3.5×** (vs NoPrefetch), 2.1× (rule-based), 1.5× (ML-based) | CXL-SSD에 prefetch offload가 주는 이득 규모 |
| §1, p.24 | PRAM은 DRAM 대비 **7× 느림** [7], 신형 flash는 **30× 느림** [2] | CXL-SSD backend latency 문제의 크기 |
| §2.1, p.25 | Samsung PoC: Z-NAND + 16GB DRAM cache → NVMe 대비 **write latency 18× 개선** [4] | 업계 CXL-SSD가 write는 잡아도 read latency는 미해결 |
| §3, p.26 | 낮은 locality(α=0.1~1, L=4~16)에서 CXL-SSD가 LocalDRAM 대비 **738% 느림** | topology/locality 무시 시 disaggregated memory 비용 |
| §3, p.26 | 높은 locality(α≤0.01, L≥16)에선 **35% 차이**로 근접 | cache hit rate가 CXL-SSD 체감성능을 지배 |
| §3, p.26 | prefetcher 정확도 **>90%**면 CXL-SSD ≈ LocalDRAM | prefetch 정확도 목표선 |
| §3, p.26 | switch layer **+4마다 성능 1% 저하** | multi-tiered switch의 latency variation을 정량화 |
| §4, p.26 | rule-based prefetcher 정확도 irregular 패턴에서 **9~76%** [21] | 왜 ML/offload가 필요한가 |
| §2.2, p.26 | CXL 3.0 multi-tiered switching → VH당 device **최대 4K** | disaggregation scale의 상한 |
| §4.1/§4.2, p.27 | reflector buffer **16KB**, timing predictor buffer **80B** | on-host 오버헤드가 작음(feasibility) |

---

## 🎯 Strategic anchor

> "ExPAND identifies the underlying CXL network topology and device latencies during PCIe enumeration and device discovery. Utilizing this information, it calculates more precise end-to-end latency for each CXL-SSD in the network ..." (§1, p.24; 메커니즘 상세 §5.1, p.27–28)

→ **본인 활용**: 면담·자소서에서 "CXL multi-tiered switch fabric에서는 device의 물리적 위치가 end-to-end latency를 결정하고, 이 논문(§3 p.26)은 그것을 switch layer +4당 1% 저하로 정량화했다. ExPAND는 이 topology를 런타임 discovery해 *prefetch timing*을 보정하지만, 나는 같은 topology 정보를 *profiling 기반 데이터 배치*(접근 A)에 써서 애초에 latency variation이 작아지도록 배치한다 — 두 접근은 layer가 다르고 상보적이다"로 사용. 이 한 문장이 "topology-awareness가 CXL 메모리 시스템의 1급 설계축"이라는 내 전제를 CAMEL 계보 안에서 직접 뒷받침한다.

---

## Connection to my research direction

| 차원 | 이 paper (ExPAND) | 본인 방향 |
|---|---|---|
| Scope | 단일 host + CXL-SSD pool의 LLC prefetch 가속 | CXL disaggregation / multi-node coherence / PGAS-over-CXL 전반 |
| Mechanism | 런타임 topology discovery → **prefetch timing** 보정 (동적, when) | profiling 기반 **데이터 배치** (정적/사전, where) — 접근 A |
| Topology awareness | PCIe enumeration + DSLBIS로 device별 end-to-end latency 계산 | 동일 latency map을 배치 결정 입력으로 재사용 가능 |
| Coherence | 단일 VH 내 LLC↔CXL-SSD 일관성만 (CXL.mem BI) | **multi-node/multi-host coherence**가 핵심 관심 — 이 논문 범위 밖 |
| Workload | irregular graph (BFS/CC/PR/TC) | 동일 graph/HPC 워크로드 재활용 가능 |
| Open space | 배치는 고정 가정, host 여러 개면 BI/timeliness 상호작용 미검토 | 배치·coherence·placement가 내 진입점 |

ExPAND는 "데이터가 어디 있는지는 고정"이라 보고 **접근 시점(timing)**을 최적화한다. 내 접근 A는 정반대 축, 즉 **접근 지점(placement)**을 profiling으로 최적화해 latency variation 자체를 줄인다. 둘은 충돌이 아니라 orthogonal·상보적이다 — profiling으로 hot object를 얕은 switch layer에 배치하면 ExPAND의 timeliness가 다뤄야 할 variation이 애초에 줄어든다. 또한 ExPAND의 coherence는 **단일 host BI**에 국한되므로, 여러 host가 같은 CXL-SSD pool을 공유할 때의 multi-node coherence는 명백한 미개척 영역이고 이것이 내 포지션이다. 방법론적으로 CXL.mem opcode 확장 + real CXL RTL cycle-level 검증은 내 feasibility-by-building을 위한 좋은 선례이자 재사용 가능한 실험 인프라(gem5+SimpleSSD, DirectCXL RTL)를 제공한다.

---

## Open questions / gaps

- [ ] **Multi-host / multi-node coherence**: BI 기반 일관성이 단일 VH·단일 host 가정. 여러 host가 같은 CXL-SSD를 공유하면 BISnpData 갱신과 host 간 일관성이 어떻게 상호작용하는가? (내 진입점)
- [ ] **데이터 배치(placement)는 고정 가정**: topology-aware timing만 다루고, hot data를 얕은 switch layer로 옮기는 profiling 기반 배치는 미검토 (접근 A와 정확히 상보)
- [ ] **decider의 EP-side 자원 비용**: transformer(dim 128) 추론을 SSD controller에서 실시간 수행하는 실제 latency·전력·면적 예산이 position paper라 미제시
- [ ] **online fine-tuning의 수렴/안정성**: phase 급변 시 decision tree classifier + online 학습의 mispredict 비용 정량화 없음
- [ ] **Custom opcode(MemRdPC/BISnpData)의 상호운용성**: CXL spec 표준 밖 확장이라 다른 벤더 host/switch와의 호환 경로 불명
- [ ] **DSLBIS latency의 정확도**: DOE가 보고하는 latency와 실제 tail latency의 괴리가 timeliness 추정 오차로 전이될 여지

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [6] | Myoungsoo Jung, "Hello bytes, bye blocks: PCIe storage meets CXL for memory expansion (CXL-SSD)", HotStorage 2022 | 이 논문의 직전 계보(CXL-SSD 개념 원류). 이미 계보 stub 존재 |
| ☐ | [36] | Gouk, Lee, Kwon, Jung, "DirectCXL: Direct access, high-performance memory disaggregation", USENIX ATC 2022 | CAMEL의 CXL disaggregation 핵심 논문 + 본 실험의 RTL 검증 기반 |
| ☐ | [16][31][32] | CXL Consortium, CXL 3.0 spec (pp.128–130, 398–399, 129) | BI·DOE·custom opcode의 1차 근거. 내 mechanism 설계 시 필독 |
| ☐ | [21] | P. Zhang et al., "Phases, modalities, temporal and spatial locality: Domain specific ML prefetcher for graph analytics", arXiv 2022 | decider의 address predictor 원형(multi-modality transformer) |
| ☐ | [35] | Gouk et al., "Amber: precise full-system SSD simulation", MICRO 2018 | SimpleSSD/Amber — 재사용 가능한 실험 인프라 |
| ☐ | [17] | Strohmaier & Shan, "Apex-map: global data access benchmark", SC'05 | locality(L, α) 조절 방법론 — 내 배치 실험 설계에 유용 |

---

## Personal annotations

<자유 형식 메모 영역. 아래는 초기 생성 시 관찰 — user가 이어서 추가.>

- 이 논문은 **position paper**다(§1 "This position paper introduces..."). 수치는 시뮬레이션 기반이고 실제 CXL-SSD 하드웨어는 아직 없음(§6 "Since no CXL-SSDs are currently available"). 면담에서 인용 시 "시뮬레이션 결과"라고 정확히 표현할 것.
- 계보상 핵심: [Hello Bytes, Bye Blocks](../Hello Bytes, Bye Blocks/Hello Bytes, Bye Blocks.md)(개념 제안) → **Cache in Hand**(topology-aware prefetch로 성능화). 즉 "CXL-SSD를 쓸만하게 만드는" Phase 2의 성능 논문.
- 내 관점 정리: ExPAND = *timing* 축, 나 = *placement* 축, 미래 = *coherence(multi-node)* 축. 세 축이 CXL-SSD 메모리 시스템의 직교 설계공간을 이룬다는 프레이밍이 유용.
