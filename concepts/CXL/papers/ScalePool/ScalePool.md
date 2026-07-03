---
title: "ScalePool: Hybrid XLink-CXL Fabric for Composable Resource Disaggregation in Unified Scale-up Domains"
aliases: [ScalePool, Hybrid XLink-CXL Fabric, XLink-CXL hybrid fabric]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---
# ScalePool: Hybrid XLink-CXL Fabric for Composable Resource Disaggregation in Unified Scale-up Domains

> **Source PDF**: [ScalePool.pdf](ScalePool.pdf)
> **Authors**: Hyein Woo, Miryeong Kwon, Jiseon Kim, Eunjee Na, Hanjin Choi, Seonghyeon Jang, Myoungsoo Jung (Panmnesia, Inc.; corresponding: mj@panmnesia.com)
> **Venue / Year**: DIMES workshop @ SOSP 2025 (계보 기준). PDF는 arXiv:2510.14580v1, 2025-10-16.
> **arXiv / DOI**: arXiv:2510.14580v1
> **Length**: 9 pages
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL/Panmnesia CXL 계보 Phase 4(2025) · fabric/scale-up 노드. 내가 논의한 **topology awareness**와 **scale-up domain 확장** 문제를 XLink+CXL hybrid로 정면으로 다루는 논문이라, multi-node coherence·PGAS-over-CXL 방향의 배경/대비군으로 정독.

계보: [CAMEL Lab CXL 연구 계보](../CAMEL Lab CXL 연구 계보.md) · Phase 4 (2025) · fabric/scale-up

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

**ScalePool**은 수천~수만 개 accelerator를 long-distance network(InfiniBand/Ethernet) 대신 **hardware interconnect**로 묶어 하나의 **unified scale-up domain**을 만드는 cluster architecture다. 핵심은 **XLink(=NVLink/UALink 등 accelerator-centric link)와 CXL을 결합한 hybrid fabric**: intra-cluster의 저지연 accelerator 통신은 XLink가, cluster를 넘어서는 scalable·coherent memory sharing은 **hierarchical CXL switching fabric**이 담당한다. 여기에 **explicit memory tiering**을 얹어, tier-1(accelerator-local memory + coherence-centric CXL/XLink)과 tier-2(dedicated memory node로 구성된 capacity-oriented CXL pool)로 성능/용량을 분리한다. 평가에서 LLM training을 RDMA baseline 대비 평균 1.22×(최대 1.84×) 가속, memory-intensive workload는 tier-2 disaggregation으로 latency 최대 4.5× 단축.

---

## Core thesis

> "ScalePool integrates Accelerator-Centric Links (XLink) and Compute Express Link (CXL) into a unified XLink-CXL hybrid fabric. Specifically, ScalePool employs XLink for intra-cluster, low-latency accelerator communication, while using hierarchical CXL-based switching fabrics for scalable and coherent inter-cluster memory sharing." (Abstract, p.1)

추가 설명: scale-up(고속 HW interconnect, 소수 accelerator)과 scale-out(long-distance network, 다수 accelerator)의 **경계 자체를 허무는 것**이 목표다. XLink는 저지연·고대역이지만 **single-hop topology 때문에 확장성이 없고 cache coherence/memory sharing이 closely-coupled cluster를 넘지 못한다**. CXL은 **flexible fabric·multi-level switch·cache coherence·open standard(interoperability)**를 제공하지만 latency가 XLink보다 크다. ScalePool의 논지는 두 기술을 계층적으로 결합해 **각자의 약점을 상대가 덮게** 하고(§3, p.3), CXL로 interface를 abstraction해 NVLink/UALink 간 interoperability 제약까지 해소한다는 것이다.

---

## Why this matters to me

내 박사 방향은 **메모리 시스템 아키텍처(CXL/coherence, multi-node)** 이고, 특히 한정진 논의에서 반복된 **topology awareness**와 **scale-up domain을 어떻게 넓히느냐**가 핵심이었다. ScalePool은 바로 그 질문 — "single-hop XLink의 확장 한계를 CXL fabric으로 어떻게 넘기고, 그 위에서 coherence를 어디까지 유지할 것인가" — 를 시스템 레벨에서 구성한 청사진이다. 특히 **inter-cluster cache coherence를 CXL.cache로 instruction-level granularity에서** 제공한다는 부분(§4, p.4)과 **selective coherence(coherent 영역만 지정해 fabric에 노출)**(§5, p.6)은 내가 관심 있는 **multi-node coherence의 실용적 절충안**의 구체적 사례다. 또한 이 논문이 **KAIST 랩이 아니라 Panmnesia, Inc. affiliation**이라는 점은 내 메모 속 "랩/회사 분리(disaggregation류는 회사로)" 구도와 정확히 맞아떨어져, 계보 상 이 노드의 성격을 규정하는 데 유용하다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| Abstract | — | p.1 | XLink+CXL hybrid fabric + 2-tier memory. LLM 1.22×(max 1.84×), tier-2 latency 최대 4.5× |
| 1 | Introduction | p.1–2 | scale-up vs scale-out 경계 문제. RDMA의 copy/serialization overhead. ScalePool 제안 |
| 2 | Background and Motivation | p.2–3 | CXL sub-protocol(mem/cache/io), PBR·switch cascading vs XLink single-hop 한계·interoperability |
| 3 | Overview of Hybrid Fabric-based ScalePool | p.3 | 두 축: accelerator-centric clusters + tiered memory (Fig.2) |
| 4 | Accelerator-Centric Cluster Architecture | p.4 | intra=XLink(NVLink ~72GPU / UALink ~1024이론·~72실측), inter=CXL(Clos/3D-Torus/DragonFly), CXL.cache coherence |
| 5 | Memory Tiers with CXL Optimizations | p.5–6 | tier-1 accelerator-local(coherence-centric CXL) / tier-2 capacity-oriented pool(no-coherence 최적화) |
| 6 | Preliminary Evaluation | p.6–7 | Calculon 기반 modeling. comm time 3.79× 단축이 주효. Fig.6/Fig.7 결과 |
| 7–8 | Ack. / Conclusion | p.7 | IITP/MSIT 지원, Panmnesia, patents. future work=large-scale |

---

## Section notes

### §1 Introduction (p.1–2)

두 가지 scaling 방식을 대비한다: **scale-up**은 high-speed hardware interconnect로 소수 accelerator를 하나의 system처럼 묶고, **scale-out**은 InfiniBand/Ethernet 같은 long-distance network로 더 많은 수를 연결한다. 문제는 **이 둘의 경계가 그대로라는 점** — scale-up은 실질적으로 수십 개 accelerator 연결에 제약된다.

> "Despite the high-bandwidth capabilities of scale-out networks, their performance falls short of that of scale-up architectures. During the data transfers, in particular, software interventions are inevitable. Even performance-optimized frameworks such as RDMA cannot completely eliminate performance degradation due to unnecessary data copying across different computing domains, serialization/deserialization, and computational overhead." (§1, p.1)

ScalePool은 이 gap을 hardware interconnect로 메우는 **large-scale cluster architecture**로, unified XLink-CXL interconnect(=hybrid fabric)를 제안한다.

### §2 Background and Motivation (p.2–3)

**Table 1 (p.2)** 이 논문 전체의 프레임을 요약한다 — CXL/UALink/NVLink 3자 비교:

| Feature | CXL | UALink | NVLink |
|---|---|---|---|
| Main purpose | Memory sharing | Accelerator comm. | Accelerator comm. |
| Latency | Medium (ns) | Low (sub-µs) | Very low (ns) |
| Coherence | Cache-coherent | Non-coherent | Limited coherence |
| Topology | Flexible fabric | Single-hop | Single-hop |
| Compatibility | Open standard | Vendor-neutral | NVIDIA-centric |
| PHY | PCIe-based | Ethernet-based | Proprietary |

**CXL sub-protocols**: `CXL.mem`(host memory controller를 network 어디에나 배치, asynchronous memory access), `CXL.cache`(computing resource 간 coherence 관리), `CXL.io`(bulk I/O, PCIe 유사). 최신 spec의 **switch cascading**(switch를 계층적으로 연결)과 **port-based routing(PBR)**(각 switch port에서 routing 결정)이 multi-level fabric을 가능케 한다(§2, p.2).

**XLink(=UALink+NVLink 총칭)**: UALink는 Ethernet PHY·point-to-point, **port당 최대 100 GB/s, sub-µs latency**; NVLink는 **500 ns 미만 latency**, proprietary signaling. 둘 다 single-hop switched topology(one-stage Clos/mesh), 큰 flit(**UALink 640B, NVLink 48B–272B**).

XLink의 두 한계를 명시한다:

> "First, its point-to-point, single-hop topology constrains scalability, preventing fabric-level interconnection across numerous devices. Second, XLink does not support cache coherence and memory sharing beyond closely coupled accelerator clusters." (§2, p.3)

**Interoperability limitation**: UALink는 open·multi-vendor, NVLink는 NVIDIA-centric. NVLink Fusion은 C2C(open)와 GPU-to-GPU(proprietary) 두 interface를 제공하지만 여전히 **최소 하나의 NVIDIA component 포함을 강제**한다.

### §3 Overview of Hybrid Fabric-based ScalePool (p.3)

CXL와 XLink 결합의 이점: XLink는 efficient low-overhead intra-cluster 통신, CXL는 scalable coherent memory sharing + interoperability 해소. 아키텍처의 두 설계 축을 제시한다 — **(i) "accelerator-centric clusters"**(intra-cluster accelerator 통신 특화), **(ii) "tiered memory architectures"**(disaggregated memory pool로 대규모 데이터 관리) (Figure 2, p.3).

### §4 Accelerator-Centric Cluster Architecture (p.4)

여기서 "cluster"는 **rack-scale computing domain**(multiple accelerator nodes)이다. **Intra-cluster는 XLink**: NVLink cluster는 NVSwitch로 **rack당 최대 ~72 GPU**(NVLink C2C로 CPU 연결), UALink cluster는 이론상 **최대 1,024 accelerator**(single-hop)지만 실무에선 GPU 등 대형 accelerator 때문에 **~72개/rack** 수준으로 수렴, CPU는 UCIe로 연결(Figure 3, p.4). NVLink flit 48B–272B(proprietary PHY) vs UALink 640B(Ethernet PHY).

**Inter-cluster는 CXL**: cluster들을 CXL fabric으로 묶어 unified hierarchical system 구성. PBR routing + multi-level cascading으로 **multi-level Clos, 3D-Torus, DragonFly** 등 다양한 topology 지원(Figure 4a, p.4). 그리고 hybrid fabric의 또 다른 장점으로 **inter-cluster cache coherence**를 강조한다:

> "Utilizing CXL.cache, accelerators can directly access remote memory at instruction-level granularity without software involvement, aggregating distributed accelerator-local memories into unified memory spaces." (§4, p.4)

protocol-level coherence는 broadcast/scatter-gather/all-reduce 같은 collective communication을 explicit synchronization·redundant copy 없이 가능하게 한다.

### §5 Memory Tiers with CXL Optimizations (p.5–6)

두 tier를 정의한다.

**Tier-1: Accelerator-local memory pool** — accelerator cluster는 XLink로 연결되고 on-package HBM/DDR 사용. XLink가 accelerator-internal memory를 **static partitioning**으로 unified linear address space를 만든다(UALink=NUMA-like, NVLink=virtualization). 문제: **static partition을 넘는 공유는 explicit software-managed copying이 필요**하고 cross-cluster 접근은 latency가 크다. 해법으로 CXL의 **selective coherence** — 특정 memory region만 cache-coherent로 지정해 inter-cluster CXL fabric에 노출:

> "Without modifying existing protocols, clusters can designate specific memory regions within accelerators as cache-coherent and expose them to the inter-cluster CXL fabric. This selective coherence approach enables cache-coherent data sharing for targeted datasets or applications... Frequently accessed data remains within accelerator caches, eliminating unnecessary inter-cluster transfers." (§5, p.5)

Figure 5b(p.5): dedicated CXL coherence logic을 accelerator 안에 XLink controller와 나란히 embed → fully unified coherent memory. **bulk data는 XLink, CXL.cache는 coherence transaction만** 처리하는 절충.

**Tier-2: Capacity-oriented memory pool** — accelerator cluster에서 **물리적으로 분리된 memory node**들을 dedicated CXL fabric으로 연결. CPU/accelerator를 배제해 density·resource efficiency 극대화. 기존 external storage/distributed FS의 ms~s latency를 **tens~hundreds of nanoseconds로 단축**. tier-2는 coherence가 불필요 → **CXL.cache/CXL.io를 switch·endpoint에서 selective deactivation**해 비용 절감. tier-1이 cache 역할을 하면 tier-2는 **CXL.mem을 아예 생략하고 CXL.io만으로 bulk transfer** 가능(Figure 5c: controller가 CXL.mem 64B / CXL.io 4KB, p.6).

### §6 Preliminary Evaluation (p.6–7)

**Methodology**: link latency를 flit size·PHY·packetization/queuing에서 도출, **switch latency는 실리콘 prototype 실측 + hop count**로 계산, 이를 **LLM co-design simulation framework [41](=Calculon)** 에 통합. Baseline = intra-rack은 XLink, inter-rack은 **InfiniBand RDMA**; 구체적으로 **36 GB200 module(72 GPU, NVLink 5.0) cluster**. Baseline은 offloaded 데이터를 GB200의 CPU-attached memory에, ScalePool은 dedicated CXL memory pool에 배치. weight/optimizer offloading(ZeRO-offload [43]) 공통 적용.

**Workloads**: GPT-3, Gopher, Llama 3, PaLM, Megatron (5개 transformer LLM).

**Figure 6 (p.7)** — training execution time(baseline normalized) speedup:

| Model | Speedup |
|---|---|
| GPT3-175B | 1.13× |
| Gopher-280B | 1.19× |
| Llama3-405B | 1.05× |
| PaLM-540B | **1.84×** |
| Megatron-1T | 1.04× |
| **Geomean** | **1.22×** |

이득의 대부분은 **communication time 단축**(평균 3.79×)에서 온다 — InfiniBand long-distance network의 software overhead(synchronization 등)를 CXL의 hardware-based 통신으로 제거. computation time은 config 무관하게 일정.

**Figure 7 (p.7)** — tiered memory, working set size별 latency. 3 config(baseline / accelerator clusters / tiered memory) 순차 평가. working set이 **개별 accelerator 용량 초과** 시 baseline 대비 **1.4×**, **cluster 전체 용량 초과** 시 baseline 대비 **4.5×**, accelerator clusters 대비 **1.6×** 가속. tier-1(accelerator-local)·tier-2(capacity-oriented) pool 덕분.

### §7–8 Acknowledgment / Conclusion (p.7)

IITP/MSIT·KIAT·MOTIE 등 한국 정부 과제 지원, "protected by one or more patents", corresponding author Myoungsoo Jung(mj@panmnesia.com). 결론: hybrid ScalePool = accelerator-centric XLink + scalable CXL fabric + tiered memory + CXL customization. Future work는 large-scale 평가와 추가 최적화.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "unified XLink-CXL hybrid fabric"
- "composable resource disaggregation in unified scale-up domains"
- "resolving the boundary between scale-up and scale-out"
- "hardware-based interconnects rather than long-distance network technologies"

**Technical concepts:**
- "hierarchical CXL-based switching fabrics"
- "inter-cluster cache coherence at instruction-level granularity" (CXL.cache)
- "selective coherence" (coherent 영역만 fabric에 노출)
- "explicit memory tiering" / "tier-1 accelerator-local memory pool" / "tier-2 capacity-oriented memory pool"
- "port-based routing (PBR)" · "switch cascading"
- "accelerator-centric link (XLink)"

**Value language:**
- "scalable yet composable resource disaggregation within a large-scale, unified scale-up architecture"
- "eliminating unnecessary inter-cluster transfers"
- "structurally resolves interoperability constraints"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 모방으로 보임):
> - "unified XLink-CXL hybrid fabric" (이 논문의 시그니처 네이밍)
> - "ScalePool" (당연히 고유명)
> - "accelerator-centric clusters" + "tiered memory architectures" 조합을 그대로 두 축으로 제시하는 서술 구조

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract, p.1 | "accelerates LLM training by 1.22× on average and up to 1.84×" | CXL hybrid fabric이 RDMA 대비 주는 training 이득 |
| Abstract, p.1 | "reduces latency by up to 4.5× for memory-intensive workloads" | tier-2 memory disaggregation의 효과 |
| §6, p.6 | communication time "average speedup of 3.79×" | 이득의 근원이 comm time(=long-distance network 제거)임을 뒷받침 |
| §2, p.2 | UALink "up to 100 GB/s bandwidth per port at sub-µs latency"; NVLink "latency below 500 ns" | XLink류 저지연·고대역 스펙 인용 |
| §2, p.3 | flit size "640B for UALink and 48B–272B for NVLink" | XLink PHY/flit 차이 → interoperability 논거 |
| §4, p.4 | NVLink "up to 72 GPUs per rack"; UALink "theoretical scales of up to 1,024 accelerators" | scale-up cluster 규모의 실측 vs 이론 |
| §5, p.6 | tier-2가 storage의 ms~s latency를 "tens or hundreds of nanoseconds"로 | disaggregated memory pool의 latency 우위 |
| §5, p.6 | controller granularity "64B (CXL.mem) / 4KB (CXL.io)" (Fig.5c) | protocol별 transfer 단위 |

---

## 🎯 Strategic anchor

> "Without modifying existing protocols, clusters can designate specific memory regions within accelerators as cache-coherent and expose them to the inter-cluster CXL fabric. This selective coherence approach enables cache-coherent data sharing for targeted datasets or applications, improving data locality and performance." (§5, p.5)

→ **본인 활용**: multi-node coherence를 "전부 coherent" 아니면 "전부 비coherent"의 이분법이 아니라 **영역 단위로 선택**하는 것이 실전 절충임을 보여주는 지점. 면담/자소서에서 "저는 scale-up domain 확장에서 coherence를 어디까지·어느 granularity로 유지할지가 핵심 설계변수라 보며, ScalePool의 selective coherence(§5, p.5)처럼 topology-aware하게 coherent 영역을 지정하는 방향에 관심 있다"로 연결. 내 **PGAS-over-CXL**·**topology awareness** 관심과 직접 맞닿는 문장.

---

## Connection to my research direction

| 차원 | 이 paper (ScalePool) | 본인 방향 |
|---|---|---|
| Scope | scale-up domain 전체를 XLink+CXL로 묶는 **system/architecture 청사진** (workshop-scale, high-level modeling) | memory system architecture, **multi-node coherence의 메커니즘 레벨** 설계·검증 (feasibility-by-building) |
| Mechanism | XLink=intra, CXL fabric=inter, CXL.cache selective coherence, 2-tier memory | coherence protocol/디렉토리·topology-aware placement·PGAS-over-CXL 주소공간을 **직접 구현/측정** |
| Workload | LLM training/inference (GPT-3, PaLM, Megatron 등), Calculon 기반 **simulation** | 실제 HW/FPGA 프로토타입에서 memory-intensive·shared-memory 패턴 구동 |
| Open space | large-scale 실측·coherence traffic의 실제 비용·PBR routing 정책은 future work로 남김 | 바로 그 **coherence 비용/디렉토리 확장성·topology별 성능**을 build해서 채우기 |

ScalePool은 "XLink+CXL을 이렇게 조합하면 좋다"는 **system-level 제안이자 high-level modeling**(silicon prototype 실측 latency를 Calculon에 넣은 수준)이다. 내 방향은 그 위 계층의 주장이 실제 HW에서 성립하는지 — 특히 **selective/instruction-level coherence를 multi-node로 확장할 때의 디렉토리·traffic 비용, topology(Clos/3D-Torus/DragonFly)별 coherence 성능** — 를 **직접 만들어 측정**하는 것이다. 즉 ScalePool은 내 연구의 **배경·정당화(scale-up 확장이 실제 needed)** 이자, "modeling으로 그친 부분을 feasibility-by-building으로 메운다"는 **차별화 포인트**를 동시에 제공한다.

---

## Open questions / gaps

- [ ] **Coherence 비용의 실측 부재**: CXL.cache instruction-level coherence의 실제 latency/traffic overhead는 modeling만 있고 silicon 검증이 없음(§6은 switch latency만 prototype 실측).
- [ ] **PBR routing 정책·congestion**: multi-level Clos/3D-Torus/DragonFly에서 어떤 routing policy가 coherence traffic에 최적인지 미해결(§4).
- [ ] **Selective coherence의 관리 주체**: coherent 영역을 누가(compiler? runtime? OS/kernel?) 언제 지정하는지 — 내 OS/kernel 관심과 연결되는 빈칸(§5).
- [ ] **Tier-1 static partition 넘는 공유의 software copy 비용**을 CXL로 얼마나 줄였는지 정량 분리 안 됨(§5).
- [ ] **Fault/coherence consistency at scale**: 수천 노드에서 coherence directory 확장성·failure 처리는 다루지 않음.
- [ ] **실측 규모**: 평가는 72 GPU(36 GB200) baseline modeling — "large-scale"은 future work로 미룸(§8).

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [41] | Isaev et al., "Calculon: A Methodology and Tool for High-Level Co-Design of Systems and LLMs", SC'23 | 이 논문의 평가 엔진. 내가 CXL 시스템 modeling할 때 baseline 툴로 검토 |
| ☐ | [10] | CXL Consortium, "Compute Express Link Specification 3.2", 2024 | PBR·switch cascading·CXL.cache 원전. multi-node coherence 설계 근거 |
| ☐ | [11] | UALink Consortium, "UALink 200 Rev 1.0 Specification", 2025 | XLink측 스펙. NUMA-like partition·single-hop 제약의 1차 자료 |
| ☐ | [30] | NVIDIA, "NVLink Fusion", 2025 | interoperability 논거의 핵심. C2C(open)/GPU-GPU(proprietary) 구분 |
| ☐ | [43] | Ren et al., "ZeRO-offload: Democratizing Billion-scale Model Training", USENIX ATC'21 | 평가의 offloading 방법론. memory pool 배치 실험 설계 참고 |
| ☐ | [37] | Lerner & Alonso, "CXL and the return of scale-up database engines", 2024 | scale-up 부활 담론. 내 motivation narrative에 인용 가치 |
| ☐ | [40] | Chen et al., "Next-gen interconnection systems with CXL", 2024 | CXL fabric topology 관련 후속 서베이 |
| ☐ | [26] | Sharma & Berger, "An introduction to CXL", Comput. Surveys 2024 | CXL 전반 서베이. 배경 정리용 |

---

## Personal annotations

<자유 형식 메모 — user 전용 영역>

- 이 폴더 stub의 title은 PDF 표지 제목과 **완전히 일치**해서 정정 불필요였음 (2026-07-04 확인). 저자·affiliation(Panmnesia, Inc.)만 frontmatter에 반영.
- affiliation이 KAIST가 아니라 **Panmnesia, Inc.** 라는 게 핵심 관찰 — 내 메모의 "랩/회사 분리(disaggregation류는 회사로)" 구도와 정확히 일치. 계보에서 이 노드는 "회사 트랙"으로 태깅해두면 좋겠다.
- venue: 표지엔 arXiv 스탬프만 있고 DIMES/SOSP 표기가 인쇄돼 있진 않음. 계보 기준(DIMES workshop @ SOSP 2025)은 stub의 외부 정보 → frontmatter에 "계보 기준" 명시하고 arXiv:2510.14580v1 병기.
