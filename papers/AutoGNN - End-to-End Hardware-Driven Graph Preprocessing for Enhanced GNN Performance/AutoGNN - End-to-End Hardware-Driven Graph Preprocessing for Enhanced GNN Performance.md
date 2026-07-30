---
title: "AutoGNN: End-to-End Hardware-Driven Graph Preprocessing for Enhanced GNN Performance"
aliases: [AutoGNN]
description: "GNN inference의 실병목인 graph preprocessing(COO→CSC 변환 + sampling) 전 과정을 7nm FPGA에 end-to-end로 구현한 재구성형 가속기. UPE(prefix-sum/radix)와 SCR(단일 사이클 reduction)로 GPU의 직렬화·atomic 병목을 제거, CPU 대비 9.0×·GPU 대비 2.1× (HPCA'26, 강승관 1저자)"
venue: HPCA
year: 2026
tier: deep
status: done
presenter: 정진
present-date:
tags:
  - paper
  - cluster/mine
  - cluster/camel
  - topic/gnn
  - topic/fpga
  - topic/hardware-acceleration
  - topic/graph-preprocessing
  - topic/near-data
  - venue/hpca
  - year/2026
---

# AutoGNN: End-to-End Hardware-Driven Graph Preprocessing for Enhanced GNN Performance
> **HPCA 2026** · `cluster/camel` · Source: [AutoGNN - End-to-End Hardware-Driven Graph Preprocessing for Enhanced GNN Performance.pdf](<AutoGNN - End-to-End Hardware-Driven Graph Preprocessing for Enhanced GNN Performance.pdf>)
> 📖 정독 노트(pdf_summary, 개인 주석 포함): [AutoGNN.md](<../../concepts/CXL/papers/AutoGNN/AutoGNN.md>) · arXiv:2602.00803v1 (31 Jan 2026)

저자: **Seungkwan Kang**(강승관, 1저자), Seungjun Lee, Donghyun Gouk, Miryeong Kwon, Hyunkyu Choi, Junhyeok Jang, Sangwon Lee, Huiwon Choi (KAIST CAMEL · Panmnesia), Jie Zhang (Peking Univ.), Wonil Choi (Hanyang Univ.), Mahmut Taylan Kandemir (Penn State), **Myoungsoo Jung** (KAIST · Panmnesia, 교신)

> [!tip] 내가 추가 발표하는 논문
> 리스트 밖이지만 **직접 발표 예정**으로 편입. 강승관 박사님(직접 contact·세미나 1차 evaluator) 1저자 + 정명수 교신 + 랩 core 총출동. 정독·개인 가설은 [concepts 노트](<../../concepts/CXL/papers/AutoGNN/AutoGNN.md>)에 있고, 이 노트는 발표용 paper-wiki 요약.

## TL;DR
GNN inference의 실제 병목은 학습·추론 연산이 아니라 **graph preprocessing** — 원본 그래프를 GPU가 소비할 수 있는 형태로 바꾸는 전처리 — 이며, 큰 그래프에서는 이게 전체 GNN 서비스 시간의 **90.8%**를 차지한다 (§I, p.1). AutoGNN은 이 preprocessing 전 과정(**graph conversion** = COO→CSC 변환, **graph sampling** = 이웃 무작위 추출)을 GPU가 아니라 **7nm FPGA 위에 end-to-end 하드웨어로** 구현해 critical path에서 제거한다. 핵심은 두 재구성 블록: 병렬화 가능한 태스크를 prefix-sum/radix로 처리하는 **UPE**(Unified Processing Element)와, 원래 atomic·직렬이라 안 풀리던 counting을 comparator+adder tree로 **한 사이클에** 끝내는 **SCR**(Single-Cycle Reducer). 여기에 그래프 특성을 프로파일링해 cost model로 최적 HW 구성을 고르고 런타임 partial reconfiguration까지 하는 소프트웨어(DGL 개조)를 얹어, conventional(CPU) 대비 최대 **9.0×**, GPU-가속 대비 **2.1×** 빠르다 (Abstract, p.1).

> [!quote]- 📄 원문 표현 (paper)
> - "we find that preprocessing overhead for large graph datasets accounts for 90.8% of the total GNN service time from an end-to-end perspective" (§I, p.1)
> - "AutoGNN executes the entire preprocessing workflow, from start to finish, directly in hardware, producing a subgraph optimized for use by GPUs or other GNN accelerators." (§I, p.1)

## 문제 & 동기 (Problem & Motivation)
- **Preprocessing이 critical path의 실병목**: GNN inference는 batch node에서 hop별로 이웃 embedding을 aggregation → DNN transformation하는데, 원본 그래프는 저장·갱신 효율 때문에 대개 **COO**(unsorted edge array)로 보관되지만 traversal은 **CSC**(vertex-centric, pointer+index array)를 선호한다 → **COO→CSC 변환(graph conversion)이 필수 preprocessing** (§II, p.2–3). 또 layer·degree가 커지면 탐색 노드가 지수적으로 폭증하는 **node explosion** 때문에 일부만 뽑는 **graph sampling**이 필요하다. Movie 데이터셋은 2-layer GNN에서 batch node에 따라 전체 그래프의 **99%**를 순회할 수 있다 (§II, p.3).
- **GPU로도 안 풀린다**: DGL이 GPU로 preprocessing을 최적화했음에도 여전히 전체 inference 시간의 **평균 70%**를 차지하고, 그래프가 커질수록 이 비중이 증가한다 (§III-A, p.4, Fig 5). 근본 원인은 preprocessing에 끼는 **counting·map 갱신이 atomic/lock**이라 GPU 수천 스레드가 **직렬 실행**에 묶이는 것.
- **태스크별 병목이 그래프 크기에 따라 이동**한다: 작은 그래프(<500K edges)는 sampling(Selecting 33.8% + Reindexing 22.1%)이 지배, 큰 그래프(수백만~수십억 edges)는 conversion, 특히 **Reshaping이 86.1%**(Ordering 1.8%)로 지배 (§III-A, p.4, Fig 6) → 단일 고정 구성으로 다 못 잡음 → **adaptable HW** 필요.

> [!quote]- 📄 원문 표현 (paper)
> - "Despite the use of GPU acceleration in DGL to optimize preprocessing, it still accounts for an average of 70% of the total inference time." (§III-A, p.4)
> - "GNN preprocessing involves extensive result reduction processes with complex synchronization operations (e.g., locks and atomic transactions). Unfortunately, this often forces GNN preprocessing into a serialized execution" (§I, p.1)

## 핵심 통찰 (Key Insight)
1. **Preprocessing을 4 태스크로 분해**하면 두 부류로 갈린다 (§II-B, p.3): **병렬화 가능**(edge ordering, data reshaping — 대량 요소 처리) vs **비병렬·동기화 필요**(unique random selection, subgraph reindexing — 소수지만 빈번한 map 갱신). 이 분류가 targeted acceleration의 근거.
2. **두 이질적 태스크를 하나의 공통 연산으로 환원**: edge ordering과 uni-random selection은 "조건 만족 요소를 뽑아 재배치"하는 **set-partitioning**으로, data reshaping과 subgraph reindexing은 "조건 만족 요소 수를 셈"하는 **set-counting**으로 통합된다 (§III-B, §IV-A). set-partitioning은 **prefix-sum**으로 각 요소의 exclusive write index를 구해 한 pass에 scatter → radix sort의 핵심. set-counting은 pointer array index가 destination VID에 대응한다는 관찰로 atomic counter 없이 병렬 counting.
3. **GPU에서 이 재설계 알고리즘을 CUDA로 돌려도 부족**: 동기화(counter·map)로 인해 **64.1%가 직렬**, memory bandwidth의 **30.3%만** 사용된다 → 범용 HW의 한계를 정량으로 못박고 전용 HW의 필요성을 논증 (§III-B, p.6, Fig 10). 비병렬 태스크 기여: selection 27.9% / reshaping 41% / reindexing 31.1%.

> [!quote]- 📄 원문 표현 (paper)
> - "64.1% of the overall execution time remains serialized, on average, resulting in low GPU resource utilization. Specifically, only 30.3% of the GPU's memory bandwidth is utilized on average" (§III-B, p.6)
> - "both edge sorting and uni-random selection can be implemented using a common operation ... called set-partitioning in this work." (§III-B, p.6)

## 설계 / 메커니즘 (Design)
**시스템 구성 (§IV-B, Fig 11).** FPGA를 **HW-kernel**(재구성 영역: UPE 커널 + SCR 커널)과 **HW-shell**(고정: PCIe controller, DMA, FPP/ICAP 재구성 포트)로 분리. host↔accelerator는 PCIe-SYS가 두 DMA 영역을 노출: **DMA-main**(scatter-gather descriptor로 user memory에 흩어진 대용량 COO를 효율 복사), **DMA-bypass**(PCIe BAR 통한 MMIO처럼 작은 subgraph 결과 전송).

**① UPE 커널 (§IV-C, Fig 12).** UPE controller + 다수 UPE + scheduler(scoreboard로 busy/idle 추적) + crossbar + scratchpad. 각 UPE = **prefix-sum logic**(계층적 adder network, 입력이 boolean이라 adder width $\log n$, $O(\log n)$ adder layers로 displacement array 생성) + **relocation logic**($O(\log n)$ routing layers, 이동거리를 2의 거듭제곱으로 분해해 좌측 shift). condition array(boolean)로 필터링 후 displacement대로 재배치 → set-partitioning을 몇 사이클에 완료. VPK180에서 **UPE 최대 240 instance, 각 width 64**. edge ordering은 COO를 UPE width로 chunk 분할 → 각 UPE가 radix sort → merge sort(Algorithm 1, 매 사이클 $w/2$ 병합)로 전역 정렬.

**② SCR 커널 (§IV-C, Fig 13).** reshaper(reshaping controller) + reindexer(reindexing controller) + AXI crossbar. 각 SCR = **comparator logic**(입력 배열을 target과 일괄 비교) + **reducer**. reshaper의 reducer는 **adder tree**(비교 결과를 합산해 pointer array 값 생성, adder width $\log n$), reindexer의 reducer는 **filter tree(OR gate)**(원본 VID 존재 여부 반환, width $32{+}1$ bit). 수천 comparator로 **한 사이클에** counting → data reshaping·subgraph reindexing의 atomic 병목 제거. reindexer는 SRAM bank에 (원본 VID / renumbered VID) 두 배열을 두고 hit이면 renumbered VID 반환, miss면 counter 증가시켜 새 매핑 등록.

**③ Dynamic reconfiguration & SW (§V-B).** 런타임 synthesis(수 시간)를 피해 **precompiled bitstream 소수**만 준비: power-of-two 원칙으로 큰 UPE 하나를 반씩 쪼개 VMK180 기준 **UPE 10종 + SCR 10종 = 20 bitstream**(각 50MB, 총 1GB)을 내부 DRAM에 상주. device를 **SCR:UPE = 30:70** 영역으로 정적 분할(bitstream 조합 폭발 방지). 재구성 = bitstream load(DRAM→3ms) + ICAP FPGA reconfig(100MHz, 225ms) = **~230ms**, 필요한 영역만 재프로그램. **Cost function(Table I)**: ordering/selection/reshaping 각각 analytic model로 사이클 예측 —
$$m=\log_2(e/w_{upe})-1,\quad \text{cycle}_{Ordering}=\frac{2\,m\,e}{n_{upe}\,w_{upe}}$$
$$\text{cycle}_{Selecting}=\frac{s}{n_{upe}}\ (s=b\,k^{\,l+1}),\quad \text{cycle}_{Reshaping}=\max\!\Big(\tfrac{n}{n_{scr}},\ \tfrac{e}{w_{scr}}\Big)$$
cost 계산은 **<0.1ms**(end-to-end의 0.1% 미만). SW는 **DGL 개조**: AGNN-lib(user library — graph I/O, 재구성 결정, `uploadgraph()`)와 AGNN-drv(kernel driver — `pci_ioremap_bar()`로 DMA-main에 scatter-gather list 매핑). 전처리된 subgraph를 GPU로 옮기는 transfer는 **~2.8ms**(전체 GNN 지연의 1% 미만) — sampled subgraph가 원본보다 훨씬 작아 무시할 수준.

> [!quote]- 📄 원문 표현 (paper)
> - "The SCR ... utilizing thousands of comparators and an adder/filter tree to aggregate the comparator outputs in a single cycle." (§III-B, p.6)
> - "the reconfiguration process takes ∼230 ms, including 3 ms to load the bitstream from DRAM and 225 ms for FPGA reconfiguration through the Xilinx ICAP IP" (§V-B, p.9)

## 평가 (Evaluation)
- **Setup (§VI)**: 7nm Xilinx **VPK180 FPGA**(4.1M LUT), floorplan SCR 8 + UPE 32 모듈. host = 128-core Xeon + **RTX 3090**. 2-layer **GraphSAGE**, $k{=}10$, 2-hop. **11 datasets**(OGB/DGL/PyG; Citation·Interaction·Social·E-commerce). 비교군: CPU, GPU(DGL), GSamp, FPGA(sampling만·conversion은 GPU), + AutoGNN 3변형 — **AutoPre**(UPE 영역 정적 분할), **StatPre**(UPE 영역 time-multiplex 통합), **DynPre**(런타임 partial reconfiguration).
- **End-to-end latency (Fig 18)**: CPU 대비 GPU 3.4×, GSamp 4.5×, FPGA 4.1×, AutoPre 7.3×, StatPre 8.4×, **DynPre 9.0×**. GPU는 edge ordering만 CPU 대비 3421× 빠르지만 나머지 atomic 태스크에 발목 잡혀 평균 3.4×에 그침. StatPre가 UPE 통합으로 AutoPre 대비 14%↓, DynPre가 partial reconfig로 추가 21.6%↓. memory BW util: **DynPre 59.8%** vs GPU 30.3%.
- **Power/energy (Fig 19)**: DynPre **9.3W** vs GPU **183W** → 전력 **19.7×↓**, 총 에너지 **3.3×↓**.
- **자원·모델 (§VI-B)**: LUT util AutoPre 47% → **StatPre 82.2%**(1.7×). cost model 정확도 **SCR 98% / UPE 94%**. 가장 무거운 GAT에서도 preprocessing 51%, DynPre가 GPU 대비 1.67×. LUT 규모 400K→4M에서 GPU 대비 1.9×→9.6×, 저가 FPGA 비용효율 21.8×. dynamic graph(Fig 30): DynPre가 StatPre 대비 end-to-end 35%↓.

> [!quote]- 📄 원문 표현 (paper)
> - "Evaluation results show performance improvements of 9.0× and 2.1× compared to conventional and GPU-accelerated preprocessing systems, respectively." (§I, p.2)

## 섹션 노트 (Section notes)
- **§I Introduction (p.1–2)**: preprocessing이 90.8% 차지, UPE+SCR로 end-to-end HW화. 4대 기여(특성 분석·분류 / 병렬 태스크 통합 HW / 비병렬 태스크 가속 / dynamic graph 재구성 + cost model).
- **§II Background (p.2–3)**: COO vs CSC, GNN inference(hop·aggregation·transformation), preprocessing = conversion + sampling, node explosion. §II-B: 4 태스크 분해(edge ordering·data reshaping·uni-random selection·subgraph reindexing).
- **§III Challenge & Motivation (p.4–6)**: GPU도 preprocessing 70% 잔존, 태스크별 breakdown(그래프 크기 따라 이동), 64.1% 직렬·30.3% BW → HW 필요성. set-partitioning/set-counting 재설계 소개.
- **§IV Hardware-Driven Preprocessing (p.6–8)**: 알고리즘 재설계(prefix-sum scatter / index=destination VID 관찰), HW-kernel/HW-shell, UPE·SCR 커널 상세.
- **§V End-to-End Operations (p.8–9)**: 전체 dataflow, edge ordering workflow(concat→split→merge→deconcat), precompiled bitstream 20종·~230ms 재구성, cost function(Table I), DGL 개조(AGNN-lib/AGNN-drv).
- **§VI Evaluation (p.9–14)**: VPK180 프로토타입, 9.0×/2.1×, 전력 19.7×↓, LUT 82.2%, cost model 98%/94%, dynamic graph.
- **§VII–IX Related/Conclusion (p.14)**: 기존 가속기는 단일 기능(inference·sampling·sorting)에 집중, AutoGNN은 end-to-end 첫 시도. Samsung Research 지원(SRFC-IT2302-05), Myoungsoo Jung 교신.

## 핵심 용어 (Key terms)
- **Graph conversion (COO→CSC)**: 저장용 unsorted edge array(COO)를 traversal용 pointer+index 구조(CSC)로 변환. = edge ordering + data reshaping. (§II)
- **Graph sampling**: node explosion을 막기 위해 각 hop에서 $k$ 이웃만 무작위 추출. = unique random selection + subgraph reindexing. (§II)
- **UPE (Unified Processing Element)**: prefix-sum + relocation logic으로 **set-partitioning**(edge ordering·selection)을 수행하는 재구성형 병렬 PE. (§IV-C)
- **SCR (Single-Cycle Reducer)**: 수천 comparator + adder/filter tree로 **set-counting**(reshaping·reindexing)을 한 사이클에 처리, atomic counter/hash map 제거. (§IV-C)
- **set-partitioning / set-counting**: 이 논문이 명명한 두 공통 연산 프레이밍. 전자는 prefix-sum scatter, 후자는 병렬 counting. (§III-B)
- **node explosion**: layer·degree 증가로 탐색 노드가 지수 폭증하는 현상. (§II)
- **HW-shell / HW-kernel**: 고정 주변부(PCIe·DMA·ICAP) vs 재구성 연산부(UPE·SCR). (§IV-B)
- **Partial reconfiguration (ICAP)**: 필요한 영역만 precompiled bitstream으로 재프로그램(~230ms). (§V-B)

## 강점 · 한계 · 열린 질문
**강점**
- "GNN이 느린 건 모델이 아니라 **데이터 준비**"라는 재프레이밍을 정량(90.8%·70%·64.1%/30.3%)으로 못박음 — motivation이 탄탄.
- 이질적 4 태스크를 **set-partitioning/set-counting 두 연산으로 환원**하는 통찰이 HW 통합(UPE·SCR)을 가능케 함.
- 7nm VPK180에 **end-to-end full-HW 프로토타입**(20 bitstream, cost model, partial reconfig)을 실제 구현 → 랩의 실행력 증거이자 feasibility-by-building 표본.
- 성능(9.0×/2.1×) + 전력(19.7×↓) + 비용효율(저가 FPGA 21.8×)을 동시에.

**한계 / 주의**
- preprocessing 결과를 결국 **GPU 메모리로 명시 transfer**(2.8ms, non-coherent) — accelerator 내부에서 끝나지 않음.
- **FPGA 재구성 가능성에 의존** — ASIC화하면 dynamic graph 적응력 상실. 20 bitstream 1GB 상주 비용.
- cost model이 **static 프로파일** 기반 — 실시간 변화 그래프에서 SCR 98% 정확도 유지 여부는 열림.
- interconnect가 PCIe+DMA — **CXL 논문 아님**(내 방향과의 접점은 "데이터 이동 병목"이라는 문제의식).

**열린 질문**
- SCR/UPE 같은 near-data reduction HW를 **CXL Type-2 device**로 shared/coherent memory에 붙이면 마지막 2.8ms transfer를 없앨 수 있나? (개인 가설 — [concepts 노트](<../../concepts/CXL/papers/AutoGNN/AutoGNN.md>) Personal annotations)
- multi-node/multi-GPU 분산 그래프에서 preprocessing HW 공유·조율 → coherence 문제로 연결.

## ❓ Q&A (자가 점검 · 발표 대비)
> [!question]- Q1. AutoGNN이 푸는 핵심 문제는?
> GNN inference의 실병목은 연산이 아니라 **graph preprocessing**(COO→CSC 변환 + sampling)이며 큰 그래프에서 전체 서비스 시간의 90.8%. GPU로 최적화해도 70% 잔존하는데, 원인은 counting·map 갱신이 atomic이라 64.1%가 직렬화(BW 30.3%만 사용)되기 때문. 이를 FPGA end-to-end HW로 제거. (§I, §III)

> [!question]- Q2. 왜 GPU가 아니라 FPGA인가?
> preprocessing 연산량이 그래프 특성에 따라 크게 요동치고(adapt 필요), reduction에 복잡한 동기화가 끼어 직렬 실행이 강제됨. FPGA는 (a) 재구성으로 다양한 입력에 adapt하고 (b) adder tree로 reduction을 $O(1)$에 처리해 두 문제에 맞음. (§I)

> [!question]- Q3. UPE와 SCR의 역할 분담은?
> **UPE** = 병렬화 가능한 set-partitioning(edge ordering·uni-random selection)을 prefix-sum + relocation logic으로. **SCR** = 비병렬 set-counting(data reshaping·subgraph reindexing)을 수천 comparator + adder/filter tree로 한 사이클에. 원래 atomic이라 GPU가 못 풀던 부분을 SCR이 담당. (§IV-C)

> [!question]- Q4. set-partitioning / set-counting이란?
> 이질적 4 태스크를 두 공통 연산으로 환원한 프레이밍. **set-partitioning**: 조건 만족 요소를 뽑아 재배치 — prefix-sum이 각 요소의 exclusive write index를 줘 한 pass에 scatter(radix sort의 핵심). **set-counting**: 조건 만족 요소 수를 셈 — pointer array index가 destination VID에 대응한다는 관찰로 atomic counter/hash map 없이 병렬 counting. (§III-B, §IV-A)

> [!question]- Q5. dynamic graph에 어떻게 적응하나?
> 태스크별 병목이 그래프 크기·시간에 따라 이동하므로 고정 구성이 부적합. 런타임 cost function(<0.1ms)으로 최적 UPE/SCR 구성을 고르고, precompiled bitstream 20종(1GB, 내부 DRAM 상주) 중 선택해 ICAP partial reconfiguration(~230ms, 필요한 영역만). DynPre가 StatPre 대비 end-to-end 35%↓. (§V-B, §VI-B)

> [!question]- Q6. 핵심 정량 결과 3가지는?
> conventional(CPU) 대비 9.0×·GPU 대비 2.1× speedup, 전력 GPU 대비 19.7×↓(9.3W vs 183W)·에너지 3.3×↓, cost model 정확도 SCR 98%/UPE 94%. (Abstract, §VI)

## 🔗 Connections
[[HPCA]] · [[2026]]
계보: [CAMEL Lab CXL 연구 계보](<../../concepts/CXL/CAMEL Lab CXL 연구 계보.md>) · GNN 라인 직전 노드 [GraphTensor](<../../concepts/CXL/papers/GraphTensor/GraphTensor.md>)(IPDPS'23) → [BeaconGNN](<../BeaconGNN - Large-Scale GNN Acceleration with Asynchronous In-Storage Computing/BeaconGNN - Large-Scale GNN Acceleration with Asynchronous In-Storage Computing.md>)(HPCA'24) → **AutoGNN**(HPCA'26)
관련: [[Smart-Infinity]](near-storage 학습, 데이터-이동 병목 프레임)

## References worth following
- Chen et al., "ReGNN: A redundancy-eliminated graph augmentation accelerator" (HPCA 2022) — 랩 GNN 라인 직전 가속기, 비교축. (ref [12])
- S. Li et al., "Hyperscale FPGA-as-a-service ... distributed GNN" (ISCA 2022) — 분산 GNN + FPGA-as-a-service → multi-node 관심. (ref [52][53])
- Y. Gui et al., "An FPGA-HBM-based streaming HW accelerator for GNN sampling" (ASAP 2024) — AutoGNN이 baseline(FPGA)으로 쓴 sampling-only 가속기. (ref [29])
- Y. Han et al., "FLAG: FPGA-based low-latency GNN inference w/ vector quantization" (DAC 2025) — 랩 최신 GNN-FPGA 병렬 라인. (ref [33])
- Xilinx AXI HWICAP product spec (ref [86]) — 런타임 partial reconfiguration 구현 디테일.

## Personal annotations
<!-- 발표용 노트. 정독·개인 가설은 concepts/CXL/papers/AutoGNN/AutoGNN.md 에 별도 보존. -->
- 발표 각도 후보: **"연산이 아니라 데이터 준비/이동이 병목"** 프레임을 GNN 버전으로 제시 → 내 CXL/memory-movement 관심축과 자연스럽게 연결. Smart-Infinity(near-storage 학습)와 같은 프레임의 다른 workload.
- 강승관 박사님 1저자 → 발표 시 "이 논문의 SCR/UPE 통합 통찰이 어떻게 나왔는지" 질문 준비하면 contact 심화에 유리.
