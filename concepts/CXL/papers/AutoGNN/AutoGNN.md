---
title: "AutoGNN: End-to-End Hardware-Driven Graph Preprocessing for Enhanced GNN Performance"
aliases: [AutoGNN]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---
# AutoGNN: End-to-End Hardware-Driven Graph Preprocessing for Enhanced GNN Performance

> **Source PDF**: [AutoGNN.pdf](AutoGNN.pdf)
> **Authors**: Seungkwan Kang, Seungjun Lee, Donghyun Gouk, Miryeong Kwon, Hyunkyu Choi, Junhyeok Jang, Sangwon Lee, Huiwon Choi, Jie Zhang, Wonil Choi, Mahmut Taylan Kandemir, Myoungsoo Jung (CAMEL Lab KAIST · Panmnesia · Peking · Hanyang · Penn State)
> **Venue / Year**: IEEE HPCA 2026 (acceptance 19%)
> **arXiv / DOI**: arXiv:2602.00803v1 (31 Jan 2026)
> **Length**: 15 pages
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL Lab CXL 계보의 GNN 라인 결정판을 정독해, 내 방향(메모리 시스템 아키텍처 · HW 가속 · feasibility-by-building)에서 "**GNN preprocessing 병목이 정확히 무엇이고, 왜 GPU로 안 풀리며, 랩이 어떻게 full-HW 프로토타입으로 이걸 증명했는지**"를 확보. **CXL 직접 아님** — GNN 계보(PreGNN→GraphTensor→AutoGNN)의 하드웨어 가속 축.

> 계보: [CAMEL Lab CXL 연구 계보](../CAMEL Lab CXL 연구 계보.md) · 직전 GNN 노드 [GraphTensor](../GraphTensor/GraphTensor.md)(IPDPS 2023)

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

GNN inference의 실제 병목은 학습·추론 연산이 아니라 **그래프 preprocessing**(원본 그래프를 GPU가 먹을 수 있는 형태로 바꾸는 전처리)이며, 큰 그래프에서는 이게 전체 서비스 시간의 90% 이상을 잡아먹는다. AutoGNN은 이 preprocessing 전 과정 — **graph conversion**(COO→CSC 변환: edge ordering + data reshaping)과 **graph sampling**(unique random selection + subgraph reindexing) — 을 GPU 대신 **7nm FPGA 위에 통째로 하드웨어로 구현**해 critical path에서 제거한다. 핵심은 두 개의 재구성 가능한 블록: 병렬화 가능한 태스크를 prefix-sum/radix로 처리하는 **UPE(Unified Processing Element)**와, 원래 atomic/serial해서 안 풀리던 counting 태스크를 comparator+adder tree로 **한 사이클에** 끝내는 **SCR(Single-Cycle Reducer)**. 여기에 그래프 특성을 프로파일링해 cost model로 최적 HW 구성을 고르고 런타임에 partial reconfiguration까지 하는 소프트웨어 프레임워크(DGL 개조)를 얹어, 관습적(CPU) 대비 최대 **9.0×**, GPU-가속 대비 **2.1×** 빠르다.

---

## Core thesis

> "We propose AutoGNN, a fully automated preprocessing hardware designed toward enhancing GNN inference performance. AutoGNN executes the entire preprocessing workflow, from start to finish, directly in hardware, producing a subgraph optimized for use by GPUs or other GNN accelerators." (§I, p.1)

추가 설명: GNN preprocessing이 느린 근본 원인은 **직렬화(serialization)와 동기화(atomic/lock)** 다. 그래프 conversion·sampling에는 특정 vertex/edge를 세는 counting과 map 갱신이 끼어 있어 GPU의 수천 스레드가 mutual exclusion에 묶인다. AutoGNN의 논지는 "그러면 알고리즘을 재설계해서(set-partitioning / set-counting) atomic 연산을 없애고, 남는 non-parallelizable 부분은 **O(1) time에 reduction하는 전용 HW**로 처리하면 된다"는 것. 즉 소프트웨어 최적화가 아니라 **feasibility를 하드웨어로 증명**한다.

---

## Why this matters to me

내 방향은 메모리 시스템 아키텍처와 HW 가속을 **직접 만들어 증명(feasibility-by-building)**하는 것이다. AutoGNN은 CXL 논문은 아니지만, "**데이터를 연산기가 쓸 수 있는 형태로 옮기고 재배치하는 전처리/데이터-이동 비용이 실제 병목**"이라는, 내가 관심 있는 memory-movement 병목 문제의 전형이다 — 그리고 그걸 GPU 소프트웨어로 우회하는 대신 **FPGA에 end-to-end 데이터패스를 물리적으로 만들어** 해결했다. 특히 GPU가 왜 안 되는지를 "64.1%가 직렬 실행, memory bandwidth의 30.3%만 사용"이라는 **정량 근거**로 못박은 점(§III-B), 그리고 kernel driver(AGNN-drv)가 `pci_ioremap_bar()`·scatter-gather DMA로 host↔accelerator 데이터 이동을 관리하는 **HW/AT + OS/kernel 접점**이 내 관심축과 정확히 겹친다. CAMEL 랩이 top-tier에서 "이런 규모의 full-HW 프로토타입을 실제로 만든다"는 실행력의 증거로도 읽는다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| I | Introduction | p.1–2 | preprocessing이 전체 서비스의 90.8% 차지, UPE+SCR로 end-to-end HW화. 4대 contribution |
| II | Background | p.2–3 | COO/CSC 표현, GNN inference(hop/aggregation), preprocessing = conversion + sampling, node explosion |
| II-B | Decomposition | p.3 | preprocessing을 4 태스크로 분해: edge ordering, data reshaping, uni-random selection, subgraph reindexing |
| III | Challenge & Motivation | p.4–6 | GPU도 preprocessing이 70% 잔존, 태스크별 breakdown, 64.1% 직렬화 → HW 필요성 |
| IV | Hardware-Driven Preprocessing | p.6–8 | 알고리즘 재설계(set-partitioning/set-counting), UPE·SCR 커널 설계, HW-kernel/HW-shell |
| V | End-to-End Operations | p.8–9 | 전체 dataflow, merge sort, precompiled bitstream 재구성, cost function(Table I), DGL 개조 SW |
| VI | Evaluation | p.9–14 | VPK180 프로토타입, 11 datasets, 9.0×/2.1× speedup, 전력 19.7×↓, dynamic graph |
| VII–IX | Related/Conclusion | p.14 | 기존 가속기는 단일 기능만, AutoGNN은 end-to-end. Samsung 지원, 정명수 교신 |

---

## Section notes

### §I Introduction (p.1–2)

GNN이 정확도는 높지만 real-world 배포가 어려운 이유를 preprocessing 오버헤드로 지목한다. GPU/CPU 같은 범용 유닛은 non-Euclidean 그래프 구조를 다룰 때 비효율적이고, preprocessing에는 (a) 입력 그래프 특성에 따라 연산량이 크게 요동치고 (b) 복잡한 동기화가 필요한 reduction이 끼어 **직렬 실행**을 강제한다는 두 특성이 있다. FPGA는 다양한 그래프 입력에 adapt하고 adder tree 같은 전용 컴포넌트로 reduction을 O(1)에 처리해 이 두 문제에 잘 맞는다. AutoGNN은 **UPE(Unified Processing Element)**와 **SCR(Single-Cycle Reducer)**로 구성된다: UPE는 edge sorting과 unique vertex selection을 한 HW 로직으로 수행(각각 conversion·sampling에 대응), SCR은 양쪽의 non-parallelizable 태스크를 담당. 4대 기여: ① preprocessing의 정량적 특성 분석·분류, ② 병렬 태스크용 통합 HW(UPE), ③ 비병렬 태스크 가속(SCR), ④ dynamic graph용 재구성 설계 + cost model.

> "we find that preprocessing overhead for large graph datasets accounts for 90.8% of the total GNN service time from an end-to-end perspective" (§I, p.1)

### §II Background (p.2–3)

그래프 표현: adjacency matrix는 희소해 비효율 → **COO**(coordinate, edge를 (src VID, dst VID) 쌍으로 정렬 안 된 채 저장; storage·update 유리)와 **CSC**(compressed sparse column, pointer array + index array; 특정 destination의 모든 source를 빠르게 조회; traversal 유리). GNN inference는 batch node에서 시작해 hop별로 이웃 embedding을 aggregation → DNN으로 transformation하는 사이클을 layer 수만큼 반복(Fig 2). 문제: 원본 그래프는 대개 COO로 저장되는데 inference traversal은 CSC를 선호 → **COO→CSC 변환(graph conversion)이 필수 preprocessing**. 또 layer/degree가 커지면 탐색 노드가 지수적으로 폭증하는 **node explosion** 때문에 원본 대신 일부를 sampling(graph sampling)해야 한다.

> "the Movie dataset may require traversing 99% of the total graph for a two-layer GNN, depending on the batch node." (§II, p.3)

### §II-B Decomposition of GNN Preprocessing (p.3)

핵심 분해: **graph conversion = edge ordering + data reshaping**, **graph sampling = unique random selection + subgraph reindexing**. ① edge ordering: destination VID 1차, source VID 2차로 radix sort → sorted COO(Fig 3). ② data reshaping: sorted COO를 훑어 같은 destination을 공유하는 edge 그룹의 range 정보를 담은 index/pointer array 생성 → 계산 집약적. ③ unique random selection: node-wise(정확도↑, 선호) vs layer-wise. 각 hop에서 중복 없이 무작위로 k 이웃 선택 — **uniqueness 보장 위해 synchronized dictionary(map) 조회**가 필요해 병목. ④ subgraph reindexing: 샘플된 vertex를 새 VID로 renumber, mutual-exclusive하게 mapping 관리 → 추가 지연. edge ordering·reshaping은 대량 요소를 다뤄 병렬화 필요, selection·reindexing은 소수지만 빈번한 update/동기화가 필요해 더 까다롭다.

### §III Challenge and Motivation (p.4–6)

**A. GPU-augmented preprocessing 분석.** 11개 실제 그래프 데이터셋(Table II)을 RTX 3090 + DGL로 측정. DGL이 GPU로 preprocessing을 최적화했음에도 **여전히 전체 inference 시간의 평균 70%**를 차지하고, 그래프가 커질수록 이 비중이 증가(Fig 5). Fig 6은 preprocessing을 Ordering/Reshaping/Selecting/Reindexing 4태스크로 breakdown: **작은 그래프(<500K edges)**는 sampling(Selecting 33.8% + Reindexing 22.1%)이 지배, **큰 그래프(수백만~수십억 edges)**는 conversion, 특히 **Reshaping이 86.1%**(Ordering은 1.8%)로 지배. 즉 단일 태스크가 항상 병목이 아니라 그래프 특성에 따라 이동 → adaptable HW 필요.

> "Despite the use of GPU acceleration in DGL to optimize preprocessing, it still accounts for an average of 70% of the total inference time." (§III-A, p.4)

**B. Removing preprocessing from critical path.** 두 재설계 아이디어: (1) edge ordering과 uni-random selection을 공통 연산 **set-partitioning**(조건 만족 요소를 뽑아 재배치; prefix-sum으로 O(1)에 위치 계산 → radix sort의 핵심)으로 통합 → UPE. (2) data reshaping과 subgraph reindexing을 **set-counting**(집합에서 조건 만족 요소 수를 셈)으로 환원 → 원래 counter가 atomic이라 병렬 불가였던 걸 SCR의 수천 comparator + adder/filter tree로 **한 사이클**에 처리. GPU에서 이 두 연산을 CUDA로 돌리면 **64.1%가 직렬**, memory bandwidth의 **30.3%만** 사용(Fig 10).

> "64.1% of the overall execution time remains serialized, on average, resulting in low GPU resource utilization. Specifically, only 30.3% of the GPU's memory bandwidth is utilized on average." (§III-B, p.6)

### §IV Hardware-Driven GNN Preprocessing (p.6–8)

**A. 알고리즘 재설계.** set-partitioning은 prefix-sum 결과(각 요소의 exclusive write index)로 요소를 한 pass에 scatter(Fig 8). set-counting은 index가 destination VID에 대응한다는 관찰로 pointer array를 counting만으로 채움(Fig 9).

**B. System architecture(Fig 11).** FPGA를 **HW-kernel**(재구성 가능 영역: UPE 커널 + SCR 커널)과 **HW-shell**(고정: PCIe controller, DMA, FPP/ICAP 재구성 포트)로 나눔. host↔accelerator는 PCIe-SYS가 두 DMA 영역을 노출: **DMA-main**(scatter-gather로 흩어진 대용량 COO를 효율 복사), **DMA-bypass**(MMIO처럼 작은 결과/subgraph 전송).

**C. 재구성 블록.** UPE 커널(Fig 12) = UPE controller + 다수 UPE + scheduler(scoreboard로 busy/idle 추적) + crossbar + scratchpad. 각 UPE는 **prefix-sum logic**(계층적 adder network, O(log n) adder layers)과 **relocation logic**(O(log n) routing layers)을 내장, boolean condition array로 필터링. SCR 커널(Fig 13) = reshaper + reindexer 컨트롤러, AXI crossbar. 각 SCR = comparator logic + reducer(reshaper는 adder tree, reindexer는 filter tree/OR gate; reducer adder width 최대 log n). VPK180에서 **UPE는 최대 240 instance, 각 width 64** 구성 가능. edge ordering은 COO를 UPE width에 맞춰 chunk로 쪼개 radix sort 후 merge.

### §V Details of End-to-End Operations (p.8–9)

**A. Dataflow(Fig 14).** UPE controller가 edge ordering으로 COO→CSC 변환 시작 → SCR reshaper가 pointer array 생성(independent set-counting으로 각 pointer 병렬 처리) → data reshaping으로 source node 식별 → UPE가 각 pointer에서 unique random selection → SCR reindexer가 SRAM의 mapping 정보로 renumbering해 sampled graph 확정. merge sort는 Algorithm 1(w/2 요소씩 병합).

**B. Dynamic reconfiguration & software.** 런타임 synthesis 지연(수 시간)을 없애기 위해 **precompiled bitstream을 소수만 준비**하고 그중 선택. power-of-two 원칙으로 큰 UPE 하나에서 반씩 쪼개 VMK180 기준 UPE 10 + SCR 10 = **20 kernel bitstream**(각 50MB, 총 1GB) 생성 → 내부 DRAM에 상주. 재구성은 bitstream 로딩(DRAM→3ms) + FPGA reconfig(ICAP IP, 100MHz, 225ms) = **~230ms**. **Cost function(Table I)**: ordering/selection/reshaping 각각 analytic model로 사이클 예측, cost 계산은 **0.1ms 미만**(end-to-end의 0.1% 미만). SW는 **DGL 개조** — AGNN-lib(user library: graph I/O, 재구성 결정)와 AGNN-drv(kernel driver: `pci_ioremap_bar()`로 DMA-main에 scatter-gather list 매핑). 전처리된 subgraph를 GPU로 옮기는 transfer는 **~2.8ms**(전체 GNN 지연의 1% 미만).

### §VI Evaluation (p.9–14)

**Prototype.** 7nm Xilinx **VPK180 FPGA**(4.1M LUT), floorplan은 SCR 8 + UPE 32 모듈(Fig 17). host = 128-core Xeon + RTX 3090. 2-layer GraphSAGE, k=10, 2-hop, 11 datasets(OGB/DGL/PyG; Citation·Interaction·Social·E-commerce). 비교군: CPU, GPU(DGL), GSamp, FPGA(sampling만, conversion은 GPU), 그리고 AutoGNN 3변형 — **AutoPre**(UPE 영역 정적 분할), **StatPre**(UPE 영역 time-multiplex 통합), **DynPre**(런타임 partial reconfiguration).

**End-to-end latency(Fig 18).** CPU 대비: GPU 3.4×, GSamp 4.5×, FPGA 4.1×, AutoPre 7.3×, StatPre 8.4×, **DynPre 9.0×**. GPU는 edge ordering만 CPU 대비 3421× 빠르지만 나머지 atomic 태스크가 발목 → 평균 3.4×에 그침. StatPre가 UPE 통합으로 AutoPre 대비 14%↓, DynPre가 partial reconfig로 추가 21.6%↓(MV에서 preprocessing 53.6%↓). memory bandwidth utilization: DynPre 평균 **59.8%** vs GPU 30.3%(e-commerce 91.6%).

**Power/energy(Fig 19).** DynPre는 FPGA에서 **9.3W**, GPU는 같은 워크로드에 **183W** → 전력 **19.7× 낮음**, 총 에너지 **3.3× 낮음**.

**Detailed(§VI-B).** transfer overhead: AutoPre가 GPU/FPGA 대비 13.6×/20× 감소(원본 그래프가 sampled subgraph보다 평균 1230× 커서 residual transfer는 0.6%). LUT utilization: AutoPre 47% → StatPre **82.2%**(1.7×). HW 재구성: SCR:UPE를 **30:70** 고정, DynSCR이 AX/SO/AM을 23%/51%/15%↓. cost model 정확도: SCR **98%**, UPE **94%**. 모델 민감도: 가장 무거운 GAT에서도 preprocessing이 51%, DynPre가 GPU 대비 1.67×; layer 1→6이면 sampling 51.1× 증가하며 DynPre speedup 3.7→4.5×. LUT/price(Fig 26): 400K→4M LUT에서 GPU 대비 1.9×→9.6×, 저가 FPGA 비용효율 21.8×. dynamic graph(Fig 30): TB에서 edge/degree가 112×/9.2× 증가, DynPre가 StatPre 대비 end-to-end 35%↓. mixed edges(Fig 31): 동일 카테고리 98.9%↓, 교차 74.1%↓.

### §VII–IX Related Work / Conclusion / Ack (p.14)

기존 가속기(GPU framework, FPGA sorting, ASIC)는 inference나 sampling·sorting 등 **단일 기능**에 자원을 집중해 end-to-end preprocessing에 부적합. AutoGNN은 재구성 가능한 통합 PE로 preprocessing 전체를 가속하는 첫 시도. Samsung Research 지원(SRFC-IT2302-05), ISCA'25·MICRO'25·HPCA'26 리뷰어 감사, **Myoungsoo Jung 교신저자**.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "preprocessing overhead ... dominate overall inference latency"
- "executes the entire preprocessing workflow ... directly in hardware"
- "removing preprocessing from the critical path"
- "adapting to varied graph inputs and efficiently executing reduction operations"

**Technical concepts:**
- "graph conversion (COO→CSC)" / "graph sampling"
- "edge ordering · data reshaping · unique random selection · subgraph reindexing"
- "node explosion"
- "reduction operations in constant time" / "O(1) time"
- "scatter-gather DMA" / "partial reconfiguration"
- "cost function / analytic model for hardware configuration"

**Value language:**
- "high-performance GNN preprocessing across diverse datasets"
- "maximizing FPGA resource utilization"
- "robust preprocessing ... across dynamic environments"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 모방으로 보임):
> - "Unified Processing Element (UPE)" / "Single-Cycle Reducer (SCR)"
> - "set-partitioning" / "set-counting" (이 논문이 명명한 프레이밍)
> - "hardware-driven graph preprocessing" (제목 그 자체)
> - "AutoPre / StatPre / DynPre" (이 논문 고유 변형 이름)

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §I, p.1 | preprocessing이 large graph에서 총 GNN 서비스 시간의 **90.8%** | "데이터 준비/이동이 연산이 아니라 병목" motivation |
| §III-A, p.4 | GPU 가속에도 preprocessing이 여전히 inference 시간의 평균 **70%** | 소프트웨어 최적화의 한계 논거 |
| §III-B, p.6 | GPU set-partition/counting의 **64.1% 직렬**, memory BW **30.3%만** 사용 | 범용 HW가 memory-movement 병목을 못 푸는 정량 근거 |
| §III-B, p.6 | 비병렬 태스크 기여: selection 27.9% / reshaping 41% / reindexing 31.1% | 어느 부분이 atomic 때문에 안 풀리는지 |
| Abstract, p.1 | CPU 대비 **9.0×**, GPU 대비 **2.1×** speedup | full-HW로 얻은 end-to-end 이득 |
| §VI-A, p.10 | DynPre **9.3W** vs GPU **183W** → 전력 19.7×↓, 에너지 3.3×↓ | HW 가속의 전력효율 |
| §VI-B, p.10 | LUT utilization StatPre **82.2%** (AutoPre 47% 대비 1.7×) | time-multiplex로 자원 활용 극대화 |
| §VI-B, p.11 | cost model 정확도 SCR **98%**, UPE **94%** | 프로파일 기반 자동 구성의 신뢰성 |
| §V-B, p.9 | 재구성 ~**230ms**(load 3ms + ICAP 225ms), cost 계산 <0.1ms | 런타임 reconfiguration 실현 가능성 |

---

## 🎯 Strategic anchor

> "As shown in Figure 10a, when GNN preprocessing is accelerated on a GPU (RTX 3090) using set-partitioning and set-counting in a CUDA kernel, the execution time is divided into parallelized and serialized portions. ... 64.1% of the overall execution time remains serialized, on average, resulting in low GPU resource utilization. Specifically, only 30.3% of the GPU's memory bandwidth is utilized on average, negatively impacting performance." (§III-B, p.6)

→ **본인 활용**: 면담·자소서에서 "**범용 프로세서는 memory-movement/데이터-재배치 병목을 못 푼다 — atomic·직렬화 때문에 대역폭의 30%도 못 쓴다**"는 근거로 인용하고, "그래서 나는 이런 병목을 **전용 HW를 직접 만들어(feasibility-by-building)** 해결하는 방향"이라고 연결. 이 한 문장이 "왜 소프트웨어가 아니라 아키텍처인가"를 정량으로 못박는 가장 강력한 지점.

---

## Connection to my research direction

| 차원 | 이 paper (AutoGNN) | 본인 방향 |
|---|---|---|
| Scope | GNN preprocessing(그래프 conversion·sampling) 전용 가속 | 메모리 시스템 아키텍처 일반(CXL/coherence 포함) |
| Mechanism | FPGA 위 UPE(prefix-sum/radix) + SCR(단일 사이클 reduction), partial reconfig | HW/AT + OS/kernel 공동설계로 데이터 이동·일관성 병목 제거 |
| Workload | 그래프(COO/CSC), node explosion, dynamic graph | 메모리 disaggregation/공유, 다양한 데이터 구조 |
| Interconnect | PCIe + DMA(scatter-gather / MMIO) | CXL.mem/.cache 기반 load-store 공유 |
| Open space | preprocessing을 accelerator 내부로; 결과는 여전히 GPU로 transfer | 데이터가 애초에 이동하지 않아도 되는 shared/coherent memory |

AutoGNN은 **CXL 논문이 아니다** — interconnect는 PCIe+DMA이고, 전처리 결과를 결국 GPU 메모리로 복사한다(2.8ms). 하지만 문제의식은 나와 정확히 겹친다: "**데이터를 연산기가 쓸 수 있는 형태로 옮기고 재배치하는 비용이 진짜 병목**"이라는 것. AutoGNN은 그 재배치를 accelerator 내부 HW로 빠르게 하는 접근이고, 내 방향은 한 걸음 더 나아가 **CXL coherent/shared memory로 그 이동 자체를 없애는** 쪽이다. 즉 AutoGNN의 SCR/UPE 같은 "reduction·counting 전용 near-data HW"를 **CXL fabric 위의 shared memory에 붙이면** transfer overhead(현재 0.6%지만 non-coherent) 자체가 사라진다 — 이게 내가 확장할 여지. 또 AGNN-drv의 kernel driver·DMA 설계는 내가 다룰 OS/kernel 계층의 좋은 레퍼런스다.

---

## Open questions / gaps

- [ ] preprocessing 결과가 여전히 **GPU 메모리로 명시적 transfer** — CXL coherent memory였다면 이 복사가 필요했을까? (near-data preprocessing + shared memory 결합 여지)
- [ ] UPE/SCR은 **FPGA 재구성 가능성**에 의존 — ASIC화하면 dynamic graph 적응력을 잃는데, CXL로 여러 accelerator를 pooling해 구성을 바꾸는 대안은?
- [ ] cost model이 **static 프로파일** 기반 — 그래프가 실시간으로 변하는 온라인 환경에서 예측 정확도(SCR 98%)가 유지되나?
- [ ] multi-node/multi-GPU로 그래프가 분산될 때(GraphTensor 라인) preprocessing HW를 어떻게 공유·조율? — coherence 문제로 연결
- [ ] 20개 bitstream 1GB를 내부 DRAM에 상주 — 더 큰 구성 공간이 필요하면 이 저장 비용이 병목이 되지 않나?

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [12] | Chen et al., "Regnn: A redundancy-eliminated graph augmentation ... accelerator," HPCA 2022 | 랩 GNN 라인의 직전 HPCA 가속기, 비교축 |
| ☐ | [52] | S. Li et al., "Hyperscale FPGA-as-a-service ... distributed GNN," ISCA 2022 | 분산 GNN + FPGA-as-a-service → 내 multi-node 관심 |
| ☐ | [53] | S. Li et al., "Hyperscale FPGA-as-a-service ...," ISCA 2022 (v2) | 위와 동일 라인, 분산 그래프 |
| ☐ | [29] | Y. Gui et al., "An FPGA-HBM-based streaming HW accelerator for GNN sampling," ASAP 2024 | AutoGNN이 baseline(FPGA)으로 쓴 sampling-only 가속기 |
| ☐ | [33] | Y. Han et al., "FLAG: an FPGA-based system for low-latency GNN inference ... vector quantization," DAC 2025 | 랩의 최신 GNN-FPGA, AutoGNN과 병렬 라인 |
| ☐ | [82] | Wilcox & Cox, "Bus-independent device accesses" (kernel.org) | AGNN-drv의 `pci_ioremap_bar()` 근거 — OS/kernel 계층 |
| ☐ | [86] | Xilinx, "Logicore IP AXI HWICAP product spec" | 런타임 partial reconfiguration 구현 디테일 |
| ☐ | — | GraphTensor (IPDPS 2023) | 계보 직전 노드, [GraphTensor](../GraphTensor/GraphTensor.md) |

---

## Personal annotations

<자유 형식 메모 — user 전용 영역. 아래는 pdf_summary 최초 작성 시 남긴 seed 관찰.>

- 이 논문의 진짜 메시지는 "GNN이 느린 건 모델이 아니라 **데이터를 준비하는 과정**"이라는 재프레이밍. 내가 CXL/memory 병목을 말할 때 그대로 빌릴 수 있는 논리 구조 — "연산이 아니라 데이터 이동/준비가 병목"이라는 프레임의 좋은 GNN 버전.
- 강승관(Seungkwan Kang) 1저자 + 구동현·권미령·장준혁 등 랩 core 총출동 + Panmnesia 공저. 계보상 GNN 라인의 **결정판**이자, 랩이 "이 규모의 full-HW 프로토타입(20 bitstream, 7nm VPK180)을 실제로 만든다"는 실행력 증거.
- 아이디어 노트: SCR(단일 사이클 reduction HW)를 **CXL Type-2 device**로 만들어 host memory에 coherent하게 붙이면, AutoGNN의 마지막 transfer(2.8ms, non-coherent)를 없앨 수 있을까? → hypotheses에 후속 검토.
