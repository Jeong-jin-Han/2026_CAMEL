---
title: "CXL Topology-Aware and Expander-Driven Prefetching: Unlocking SSD Performance"
aliases: [CXL Topology-Aware Prefetching, CXL Topology-Aware and Expander-Driven Prefetching, Topology-Aware Prefetching]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---

# CXL Topology-Aware and Expander-Driven Prefetching: Unlocking SSD Performance

> **Source PDF**: [CXL Topology-Aware Prefetching.pdf](CXL Topology-Aware Prefetching.pdf)
> **Authors**: Dongsuk Oh, Miryeong Kwon, Jiseon Kim, Eunjee Na, Junseok Moon, Hyunkyu Choi, Seonghyeon Jang, Hanjin Choi, Hongjoo Jung, Sangwon Lee, Myoungsoo Jung (Panmnesia, Inc. + KAIST)
> **Venue / Year**: IEEE Micro 2025 (extended version; arXiv:2505.18577v1, 24 May 2025)
> **arXiv / DOI**: arXiv:2505.18577
> **Length**: 11 pages
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL CXL 계보 Phase 4(2025)의 **topology awareness** 지점 확보. CXL switch topology(multi-tiered switching)를 시스템 설계의 1급 변수로 끌어올린 사례라서, 내 flat-topology / programming-model / feasibility-by-building 방향과 직접 닿는지 검증.
>
> **계보**: Phase 4 (2025) · topology awareness · [CAMEL Lab CXL 연구 계보](../CAMEL Lab CXL 연구 계보.md)

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

CXL로 SSD를 memory expander(CXL-SSD)로 붙이면 대용량 byte-addressable 메모리를 얻지만, backend SCM(Z-NAND, XL-Flash, PRAM)이 DRAM보다 느려 read latency가 병목이다. 이 논문은 **ExPAND** — LLC prefetching을 host CPU가 아니라 **CXL-SSD(expander) 쪽으로 offload**하는 expander-driven prefetcher를 제안한다. 핵심은 두 가지다. (1) prefetcher를 host 밖으로 옮겨 host의 hardware logic 면적 제약을 우회하고 EP의 큰 form factor에 heterogeneous ML predictor(transformer 기반 address predictor + rule 기반 timing predictor)를 얹는다. (2) **CXL multi-tiered switching topology를 인식**해서 각 CXL-SSD까지의 end-to-end latency를 PCIe enumeration/DOE로 측정하고, 이를 바탕으로 "지금 prefetch하면 CPU가 필요할 때 딱 맞게 도착하는가"라는 **prefetch timeliness**를 정밀 추정한다. host-side **reflector**와 EP-side **decider**가 CXL.mem 위에서 양방향 통신하며(하향: `MemRdPC`로 PC 전달, 상향: 새 BI opcode `BISnpData`로 prefetched data 삽입), CXL.mem back-invalidation(BI)으로 LLC-SSD 간 data consistency를 유지한다. 결과적으로 graph app **9.0×**, SPEC CPU **14.7×** 성능 향상.

---

## Core thesis

> "We present ExPAND, an expander-driven CXL prefetcher that offloads last-level cache (LLC) prefetching from host CPU to CXL-SSDs. ... ExPAND, being aware of CXL multi-tiered switching, provides end-to-end latency for each CXL-SSD and precise prefetch timeliness estimations." (Abstract, p.1)

추가 설명: prefetching 자체가 아니라 **prefetching을 어디에 두고, CXL topology를 어떻게 인식하느냐**가 논문의 축이다. host CPU는 LLC prefetcher를 넣을 hardware logic 면적이 부족하고(rule-based는 수십 MB 저장 필요), 반대로 CXL-SSD EP는 form factor가 커서 무거운 ML predictor를 감당한다. 여기에 CXL switch 계층이 만드는 **latency variation**을 topology-aware하게 보정해야 prefetch가 "too early(LLC 오염) / too late(stall)" 없이 timely하게 도착한다는 것.

---

## Why this matters to me

내 박사 방향은 CXL/coherence 중심의 **메모리 시스템 아키텍처**이고, 그 안에서 **topology awareness**가 flat-topology·programming model 관심과 직접 맞닿는다. 이 논문은 CXL의 multi-tiered switch topology(switch level에 따라 end-to-end latency가 달라지는 것)를 **시스템이 무시할 수 없는 1급 변수**로 정면 취급한 드문 사례다 — 그것도 "prefetch가 언제 도착하는가"라는 timing 문제로 구체화해서. 내가 topology를 어떻게 abstraction/programming model 위로 노출할지 고민할 때, 이 논문은 "topology를 hardware 아래(PCIe enumeration + DOE + config space)에서 자동 발견해 latency로 환산"하는 반대 방향(투명화) 접근의 레퍼런스가 된다. 또한 Panmnesia/KAIST 계보(DirectCXL → memory pooling → cache-in-hand → 본 논문)의 최신 지점이라, 내가 CAMEL/Panmnesia 맥락에서 CXL을 얘기할 때 배경으로 필수. reflector/decider의 CXL.mem 프로토콜 확장(custom opcode 신설)은 feasibility-by-building 관점에서 "프로토콜을 어디까지 만져야 이게 되는가"의 실증 사례.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| — | Abstract | p.1 | ExPAND = LLC prefetching을 CXL-SSD로 offload, topology-aware timeliness. graph 9.0×, SPEC 14.7× |
| 1 | Introduction | p.1–2 | CXL-SSD의 slow read latency 문제 + host prefetcher 2대 제약(logic size, latency variation) |
| 2 | Background | p.2–3 | CXL.io/.cache/.mem, memory pooling, CXL-SSD(DOE), multi-tiered switching(VH, 4K devices), back-invalidation(BI) |
| 3 | Motivation and Challenges | p.3–5 | locality/accuracy/coverage 분석, switch level당 2.7× degradation, 기존 prefetcher는 topology-unaware |
| 4 | Expander-Driven Prefetching | p.4–5 | reflector(host RC/LLC) + decider(EP ML predictor); address predictor(transformer) + timing predictor |
| 5 | CXL Cross-Layer Intersection | p.5–6 | topology-aware timeliness(switch discovery + DSLBIS latency), 양방향 통신(MemRdPC↓ / BISnpData↑) |
| 6 | Evaluation | p.6–8 | gem5+SimpleSSD; vs Rule1/2·ML1/2; ML 대비 2.4×, LocalDRAM 대비 개선, backend media/switch 민감도 |
| 7 | Conclusion | p.9 | expander-driven + topology-aware prefetching, patent 보호 |

---

## Section notes

### Abstract (p.1)

ExPAND는 host CPU→CXL-SSD로 LLC prefetching을 offload하는 expander-driven prefetcher. heterogeneous prediction algorithm을 쓰고, CXL.mem back-invalidation으로 consistency 보장. CXL multi-tiered switching을 인식해 각 CXL-SSD의 end-to-end latency와 prefetch timeliness를 정밀 추정. CXL-SSD 의존을 줄이고 대부분의 데이터를 host cache에서 직접 접근하게 함. graph **9.0×**, SPEC CPU **14.7×** 향상(diverse prefetching 전략을 쓴 CXL-SSD pool 대비).

### §1 Introduction (p.1–2)

CXL은 memory disaggregation의 key interface. SCM(PRAM, Z-NAND, XL-Flash)은 DRAM보다 용량은 크나 훨씬 느리다 — PRAM은 DRAM보다 최대 7× 높은 latency, Z-NAND/XL-Flash는 약 30× 느림. CXL-SSD는 file system을 거치는 block device와 달리 load/store로 직접 서비스되어 host storage stack을 우회한다. 그러나 현재 SSD 기술은 block-level 요청 처리에 맞춰져 있어 high-latency memory 요청에 취약. host CPU-side prefetcher가 부분적으로 돕지만 두 가지 근본 제약:

> "two primary challenges limit the effectiveness of current prefetchers ...: i) *hardware logic size constraints* that hinder the handling of diverse memory access patterns in the CXL memory pooling space, and ii) *latency variations* caused by the differing physical positions of CXL-SSDs within the CXL switch network." (§1, p.2)

Rule-based prefetcher(spatial/temporal)는 정확도를 위해 종종 수십 MB 저장(LLC 크기급)이 필요해 CPU에는 stream 같은 단순 알고리즘만 탑재됨 → CXL-SSD latency를 못 가린다.

### §2 Background (p.2–3)

**Memory Pooling using CXL.** CXL은 cache-coherent interconnect, 3개 sub-protocol: CXL.io(PCIe 대응), CXL.cache(accelerator→host memory), CXL.mem(host→device memory). CXL.cache 없이도 EP를 cacheable memory space에 map 가능. CXL-SSD는 큰 internal DRAM cache로 backend SCM 데이터를 미리 저장(Samsung PoC: Z-NAND + 16GB DRAM cache, NVMe 대비 18× write latency 개선; Kioxia: XL-Flash + DRAM prefetch buffer).

**Go Beyond Pooling.** CXL.cache는 EP 내부 메모리 관리에 큰 overhead. CXL 3.0의 **back-invalidation(BI)** — CXL.mem이 EP(CXL-SSD 등)가 host cache line을 자율적으로 snoop/invalidate하게 해 CXL.cache 의존 없이 coherence 유지. **Multi-tiered switching** — EP expander들이 CXL switch(upstream/downstream ports)로 연결, fabric manager(FM)가 virtual hierarchy(VH)를 구성. CXL 3.0/3.1의 multi-tiered switching으로 switch가 추가 switch에 연결 가능 → VH당 최대 **4K devices**까지 확장.

### §3 Motivation and Challenges (p.3–5)

**Prefetching Impact Analysis.** Figure 1은 LocalDRAM vs CXL-SSD의 locality별 latency. global data access benchmark로 L(vector length=spatial locality), α(temporal locality)를 조절.

> low-locality(α=0.1~1, L=4~16)에서 "CXL-SSD performance is, on average, 738% slower than that of LocalDRAM" (§3, p.3). high-locality(α≤0.01, L≥16)에서는 "the performance gap narrows significantly, with CXL-SSD being only 35% slower than LocalDRAM on average" (§3, p.3).

즉 데이터가 backend SCM이 아니라 LLC에서 서비스되면 gap이 극적으로 준다 → cache hit rate 개선이 CXL-SSD user experience에 결정적. prefetch effectiveness가 80% 미만이면 최대 4.5× 느리고, 90% 초과하면 hit rate 상승으로 크게 개선. perfect prefetch면 CXL-SSD가 LocalDRAM을 2.5×~3.9× 능가(Figure 2a: 2.5×/3.0×/3.9×). MPKI 관점(Figure 2b): SSSP는 12 MPKI로 3.9× 개선. prefetcher effectiveness는 **prefetch accuracy**(prefetch된 데이터가 실제 쓰인 비율) + **prefetch coverage**(전체 요청 중 prefetch가 서비스한 비율) 둘 다 중요.

**Latency Variation with CXL Switch Topology.** effectiveness가 높아도 multi-tiered switching의 latency variation은 못 가린다. switch layer를 1→4로 늘리며 측정(effectiveness 90% 가정):

> "the four graph workloads (CC, PR, TC, SSSP) experienced a 2.7× performance degradation per additional CXL topology switch layer in average." (§3, p.4)

CC/PR/TC는 switch당 1.3×, SSSP는 1.4× degradation. cache hit이어야 할 요청이 switch latency 때문에 miss로 전환됨. **결론: topology를 인식해 latency variation을 보정하는 prefetcher 설계가 필요.**

### §4 Expander-Driven Prefetching (p.4–5)

기존 CPU-side rule-based는 large/irregular/random pattern에서 정확도 9%~76%로 CXL-SSD를 Local DRAM 수준(≈90%)으로 못 끌어올림. ML-based는 정확도는 되나 on-chip CPU 구현에 model 연산·metadata overhead가 커 비현실적. → **expander-driven prefetcher**로 결정을 EP 쪽에 위임, CXL EP의 큰 form factor 활용.

- **Reflector** (host-side, CXL **root complex(RC)** + LLC controller): decider에 PC와 connected CXL-SSD의 switch depth 제공, prefetch 결과 통신. **16 KB** buffer로 decider가 prefetch한 cache line update를 로깅 → 각 host LLC controller가 이 buffer를 먼저 확인해 CXL-SSD pool 순회 없이 데이터 제공.
- **Decider** (EP-side, CXL-SSD controller): irregular pattern에 최적화된 heterogeneous ML prefetcher. PC + memory address를 입력으로 데이터를 reflector buffer로 전송하고 online refinement용으로 입력을 기록.

**Prefetch Address and Timing Speculation.** decider는 두 predictor:
- **Address predictor**: multi-modality transformer(ref [33]) 기반 sequence model. multi-modality attention network(ref [42])로 memory access pattern과 PC 관계 분석, decision tree classifier(ref [43])로 실행 behavior 변화를 감지해 정확도 동적 refine. classifier는 memory trace를 **64 categories**로 사전학습. sliding window(recent addresses + PCs)로 online inference, classifier 추론이 바뀌면 **behavior-change event**로 기록해 transformer에 hint → 급변에 신속 대응.
- **Timing predictor**: request arrival time을 작은 buffer(**80B**)에 유지, history window 내 arrival time 평균으로 미래 요청 시각 추정(rule-based). LLC가 직접 서비스하면 정보가 안 닿으므로 reflector가 cache hit event를 CXL.io로 통지.

실제 timeliness = timing predictor 결과 + 각 device latency variation 결합(§5에서 상술).

### §5 CXL Cross-Layer Intersection (p.5–6)

**CXL Topology-Aware Prefetch Timeliness.**
- *Switch hierarchy discovery*: reflector가 **PCIe enumeration** 중 CXL-SSD의 switch level 식별. host가 config space(CXL.io)를 접근해 system bus를 조직, 새 device마다 unique bus number 부여. CXL switch가 PCIe bridge로 동작하므로 host CPU~target CXL-SSD 사이 switch 개수를 판별, RC side에 저장.
- *Timeliness speculation*: 너무 이르면 LLC 오염, 너무 늦으면 delay. 각 CXL-SSD는 **DOE(data object exchange)**(PCIe config space)로 device latency 결정. reflector가 DOE에서 **DSLBIS(device scoped latency and bandwidth information structure)**를 추출해 device latency를 얻고, RC~target CXL-SSD 간 **VH latency**를 계산. 둘을 합쳐 end-to-end latency를 device config space에 기록. decider는 timing predictor 예측 시각에서 이 end-to-end latency를 빼서 prefetch timeliness 추정.

**Bidirectional Communication on CXL.**
- *Downward (piggybacking on CXL.mem)*: PC를 timely하게 전달해야 정확한 address 예측 가능. CXL.mem M2S transaction = Req(MemRd, payload 없음), RwD(MemWr, payload), BIRsp. RwD가 13개 custom opcode 허용 → memory read + PC용 opcode **`MemRdPC`** 신설. read가 LLC miss나면 reflector가 현재 PC를 포함한 `MemRdPC` M2S 전송 → target decider가 host 실행 환경의 memory address+PC 접근.
- *Upward (leveraging BI)*: prefetch 시점에 decider가 address predictor 결과로 reflector buffer를 갱신해야 하나 기존 CXL.mem은 host on-chip storage 갱신 능력이 없음. → S2M BISnp에 새 BI opcode **`BISnpData`** 도입(최대 10개 custom opcode), payload로 host 갱신용 데이터 동반. reflector가 `BISnpData` 감지 시 payload 대기 후 prefetched data를 buffer에 삽입 → LLC controller가 fetch.

### §6 Evaluation (p.6–8)

**Methodology.** CXL-SSD 실물이 없어 full system simulation: **gem5 + SimpleSSD**, CXL RTL module을 cycle-level로 validation. 비교 대상: **Rule1**(spatial best-offset [16]), **Rule2**(temporal [19]), **ML1**(LSTM [39]), **ML2**(transformer [32]). Config(Table 1): O3 12-core@3.6GHz, L2 1.25MB, DRAM tRP=tRCD=tCAS=22ns, PMEM Intel P5800X, PCIe 6.0(64 GT/s)/CXL 3.0; NAND Samsung 983 ZET 2TB(tRd 3µs); address predictor attention dim 64 / fusion dim 128 / transformer dim 128; timing buffer 10 entries. workload: graph(CC/PR/SSSP/TC) × 5 dataset + SPEC(bwaves, leslie3d, libquantum, mcf, lbm).

**Overall (Figure 4a).** Rule1은 NoPrefetch 대비 2×, Rule2는 1.8×(graph의 spatial locality 덕). ML-based는 rule 대비 1.6×, NoPrefetch 대비 4.4×. **ExPAND는 ML 대비 추가 2.4×, NoPrefetch 대비 4.3×~71.8×**. stencil 지배 workload(bwaves, leslie3d, lbm)에서 특히 강함. mixed workload(Figure 4b)에서 ExPAND가 Rule1/Rule2/ML1/ML2 대비 **7.0× / 10.2× / 3.7× / 3.5×**.

**Timeliness model.** 성능 이득은 timeliness accuracy **68%** 부근에서 saturate, 84% 넘으면 marginal(LLC associativity가 minor 오차 흡수). ExPAND timeliness model은 **90% accuracy** 달성. online tuning(Figure 4e)은 behavior 변화 후 LLC hit rate 회복을 가속(decision tree classifier가 변화를 즉시 감지 → transformer가 최근 access 우선).

**vs LocalDRAM (Figure 5).** ExPAND는 graph 4종에서 NoPrefetch 대비 **9.0×**. LocalDRAM 대비로는 14% cache miss로 48% degradation(ExPAND LLC hit **86%**). 그러나 SPEC의 leslie3d/libquantum/lbm은 LocalDRAM을 **3.9× / 1.2× / 2.8×** 능가(hit rate 최대 96%).

**Backend media (Figure 7).** ExPAND-Z(Z-NAND, PMEM보다 6× 느림)는 ExPAND-P(PMEM)보다 평균 3× 높은 degradation. 그럼에도 leslie3d/lbm은 ExPAND-Z가 LocalDRAM보다 3.9×/1.8× 우수. **ExPAND-D(DRAM backend)는 전 workload에서 LocalDRAM 능가(1.3×~3.9×, 평균 1.9×)** → backend media가 좋아질수록 memory expander의 이득이 훨씬 커짐. switch-level 민감도: libquantum(high LLC hit)은 switch latency가 지배, TC(low hit)는 backend latency가 지배. ExPAND-D는 level 1에서 54% drop(평균 32%)으로 switch latency에 민감.

### §7 Conclusion (p.9)

expander-driven CXL prefetcher로 LLC prefetching을 CXL-SSD에 offload, heterogeneous prediction + BI로 consistency + 정밀 timeliness 추정 → CXL-SSD 의존 감소, graph 9.0× / SPEC 14.7×. **"This work is protected by one or more patents."** (Panmnesia).

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "memory disaggregation"
- "CXL multi-tiered switching topology"
- "latency variation caused by the differing physical positions of ... within the CXL switch network"
- "prefetch timeliness"

**Technical concepts:**
- "byte-addressable access / load-store semantics (bypassing the host-side storage stack)"
- "virtual hierarchy (VH)" / "fabric manager (FM)"
- "back-invalidation (BI)" / "BI snoop (BISnp)"
- "DOE (data object exchange)" / "DSLBIS (device scoped latency and bandwidth information structure)"
- "end-to-end latency estimation"
- "prefetch accuracy vs prefetch coverage"
- "behavior-change event" / "multi-modality transformer" / "decision tree classifier"

**Value language:**
- "storage class memory (SCM)"
- "hardware logic size constraints"
- "direct host cache access for most data"
- "scalable access to large-capacity memory"

> ⚠ **피해야 할 어휘** (paper-signature — 그대로 echo하면 이 논문 모방으로 보임):
> - "ExPAND" / "expander-driven (CXL) prefetcher"
> - "reflector and decider"
> - "topology-aware and expander-driven prefetching"
> - "MemRdPC" / "BISnpData" (이 논문이 신설한 custom opcode 이름)
> - "unlocking SSD performance"

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract, p.1 | "graph application performance and SPEC CPU's performance by **9.0× and 14.7×**" | expander offload + topology-aware prefetching의 최종 성능 |
| §3, p.3 | low-locality에서 "CXL-SSD ... **738% slower** than ... LocalDRAM"; high-locality에선 "**only 35% slower**" | CXL-SSD 병목은 locality/cache-hit에 극도로 민감 |
| §3, p.4 | "**2.7× performance degradation per additional CXL topology switch layer** in average" | topology(switch level)를 무시하면 성능이 무너진다 — 내 topology-awareness 논거 |
| §1, p.2 | rule-based prefetcher는 "**tens of megabytes** of storage—comparable to ... LLC" | host CPU에 강한 prefetcher를 못 넣는 이유(logic size) |
| §1/§3, p.2/p.4 | rule-based accuracy "**9% to 76%**" for large/irregular/random patterns | 기존 prefetcher가 CXL-SSD엔 부족 |
| §6, p.7 | ExPAND는 ML-based 대비 **2.4×**, NoPrefetch 대비 **4.3×~71.8×**; mixed에서 Rule/ML 대비 7.0×/10.2×/3.7×/3.5× | prefetcher 비교 우위 |
| §6, p.7 | timeliness 이득은 **68%** accuracy에서 saturate, ExPAND는 **90%** 달성 | timeliness 정확도의 실효 임계 |
| §6, p.8 | ExPAND-D(DRAM backend)가 LocalDRAM 대비 1.3×~3.9×(평균 1.9×) | backend media 개선 시 expander 이득 확대 |
| Table 1d, p.6 | ExPAND: memory overhead 839.2KB, 13.3M IOPs, **92% accuracy** (vs ML2 865KB/89%, Rule1 4KB/82%) | 정확도·overhead trade-off |

---

## 🎯 Strategic anchor

> "Even an prefetcher is designed towards having high effectiveness, it cannot fully mitigate performance degradation caused by latency variation in multi-tiered switching environments. Specifically, conventional prefetchers fail to account for the additional latency introduced by CXL switches, resulting in data being unavailable when needed by the CPU." (§3 *Latency Variation with CXL Switch Topology*, p.4)

→ **본인 활용**: 면담/자소서에서 "CXL을 쓰는 순간 **topology(switch level)가 latency를 결정하는 1급 변수**가 되는데, 기존 시스템 계층은 이를 무시한다 — 이 논문(p.4)이 prefetch 관점에서 그 gap을 2.7×/switch-layer로 정량화했다"고 인용. 내 방향은 이 topology-awareness를 prefetch timeliness라는 개별 최적화가 아니라 **programming model/abstraction 레벨에서 topology를 어떻게 노출/은폐할지**로 한 단계 올리는 것. "topology를 hardware 밑에서 자동 발견해 latency로 환산"하는 이 논문의 투명화 접근과 대비되는 포지션.

---

## Connection to my research direction

| 차원 | 이 paper (ExPAND) | 본인 방향 |
|---|---|---|
| Scope | 단일 host + CXL-SSD pool의 **LLC prefetch 최적화** | memory system architecture 전반(coherence, multi-node, PGAS-over-CXL) |
| Mechanism | topology를 hardware 밑에서 자동 발견(PCIe enum/DOE/DSLBIS)해 latency로 **은폐** | topology를 abstraction/programming model 위로 **노출**할지 vs 은폐할지의 설계 원칙 |
| Workload | graph app + SPEC CPU (single-node) | multi-node coherence, disaggregated shared memory |
| Open space | multi-tiered switching latency **인지·보정**까지 | flat-topology 지향 시 이 latency-variation을 아예 **구조적으로 줄이거나 programming model로 흡수** |

ExPAND는 "topology는 어차피 존재하니 그 latency를 정밀 측정해 timely하게 감춘다"는 **투명화(transparency)** 철학이다. 내 관심은 정반대 축도 포함한다 — topology를 시스템/프로그래밍 모델의 명시적 개념으로 올려 사용자가 locality를 제어하게 하거나(PGAS 계열), 혹은 flat-topology로 variation 자체를 줄이는 것. 그래서 이 논문은 **경쟁이 아니라 대비 레퍼런스**다: 같은 topology-awareness 문제를 hardware/protocol 레이어에서 푸는 극단을 잘 보여주고(심지어 CXL.mem에 `MemRdPC`/`BISnpData` opcode를 신설하는 feasibility-by-building 실증), 내가 "그럼 이걸 상위 레이어에서 다루면?"이라는 질문을 세울 발판이 된다. 또한 single-node 범위라 multi-node coherence로의 확장은 열려 있다(reflector/decider 통신이 여러 host VH에 걸치면 consistency·timeliness가 어떻게 되나?).

---

## Open questions / gaps

- [ ] **Multi-node/multi-host 확장**: reflector는 host RC에, decider는 EP에 있는데, 여러 host가 한 CXL-SSD pool을 공유하는 multi-VH 환경에서 prefetch timeliness와 BI 기반 consistency가 어떻게 상호작용하나? (논문은 사실상 single-host 관점)
- [ ] **Programming model 부재**: topology를 전부 hardware가 숨긴다 — application/runtime이 locality나 device 위치를 힌트로 줄 여지는? PGAS-over-CXL와의 접점.
- [ ] **Protocol 변경 비용**: `MemRdPC`(RwD custom opcode), `BISnpData`(S2M BI opcode) 신설은 CXL spec/HW 변경을 전제. 표준화·상호운용성 측면의 현실성?
- [ ] **CXL-SSD 실물 부재**: gem5+SimpleSSD 시뮬레이션 검증뿐 — 실제 EP controller에 transformer decider를 넣는 면적/전력 예산의 실증은 미공개.
- [ ] **Security/isolation**: reflector가 PC(program counter)를 CXL.mem으로 EP에 흘리는데, multi-tenant pool에서 side-channel/isolation 우려는?
- [ ] **Timeliness saturation의 일반성**: 68% saturate가 LLC associativity 가정에 의존 — 다른 cache 구성/workload에서도 성립?

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [46] | Gouk, Lee, Kwon, Jung. *DirectCXL: Direct access, high-performance memory disaggregation with DirectCXL*. USENIX ATC 2022 | 계보의 뿌리 — CXL disaggregation 원형 |
| ☐ | [1] | Gouk, Kwon, Bae, Lee, Jung. *Memory pooling with CXL*. IEEE Micro 2023 | 계보 중간 — pooling 단계 |
| ☐ | [9] | Kwon, Lee, Jung. *Cache in hand: Expander-driven CXL prefetcher for next generation CXL-SSD*. ACM HotStorage 2023 | 본 논문의 직접 선행(expander-driven prefetch의 초기 PoC) |
| ☐ | [28] | CXL Consortium. *Compute Express Link 3.0 Specification*, 2022 | BI, multi-tiered switching, VH의 원전 |
| ☐ | [23] | Das Sharma. *Compute Express Link (CXL): Enabling heterogeneous data-centric computing*. IEEE Micro 2022 | CXL topology/latency 배경 정리 |
| ☐ | [33] | Zhang, Kannan, Prasanna. *Phases, modalities, spatial and temporal locality: Domain specific ML prefetcher for graph analytics*. SC | address predictor(multi-modality transformer)의 기반 |
| ☐ | [26] | Berger et al. *Design tradeoffs in CXL-based memory pools for public cloud platforms*. IEEE Micro | CXL pool의 topology/latency trade-off(내 multi-node 관점) |
| ☐ | [3] | Sun et al. *Demystifying CXL memory with genuine CXL-ready systems and devices*. MICRO 2023 | 실측 CXL latency 특성 |
| ☐ | [45] | Gouk et al. *SimpleSSD: precise full-system simulation ...*. MICRO 2018 | 평가 인프라(내 feasibility-by-building 툴체인) |

---

## Personal annotations

<자유 형식 메모. 본인이 paper 읽으며 떠올린 생각·이견·후속 아이디어. user가 직접 추가하는 영역.>

- (초기 메모) 이 논문의 topology-awareness는 "latency를 측정해 감춘다"에 가깝다. 내 flat-topology 관심과는 **문제의식은 공유(topology가 성능을 지배)하되 해법 방향이 반대**(은폐 vs 노출/평탄화)라는 점을 면담에서 명확히 구분해 말할 것.
- Panmnesia 소속 논문 + "protected by one or more patents" — 회사 IP 맥락. CAMEL 랩(HW/AT+OS/kernel) vs Panmnesia(disaggregation 제품) 분리 구도와 일치.
