---
title: "CXL-ANNS: Software-Hardware Collaborative Memory Disaggregation and Computation for Billion-Scale Approximate Nearest Neighbor Search"
aliases: [CXL-ANNS, CXL ANNS]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---

# CXL-ANNS: Software-Hardware Collaborative Memory Disaggregation and Computation for Billion-Scale Approximate Nearest Neighbor Search

> **Source PDF**: [CXL-ANNS.pdf](CXL-ANNS.pdf)
> **Authors**: Junhyeok Jang, Hanjin Choi, Hanyeoreum Bae, Seungjun Lee, Miryeong Kwon, Myoungsoo Jung (KAIST CAMEL Lab · Panmnesia, Inc.)
> **Venue / Year**: USENIX ATC 2023 (Boston, MA · July 10–12, 2023)
> **arXiv / DOI**: usenix.org/conference/atc23/presentation/jang · ISBN 978-1-939133-35-9
> **Length**: 17 pages (proceedings pp.585–600)
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL Lab CXL 연구 계보 Phase 2의 대표작. "워크로드 특화로 CXL far-memory 페널티를 극복한다"는 내 H1(워크로드 특화 가설)의 원형 사례로 읽는다. embedding table 수백GB급 워크로드에서 CXL memory pooling이 실효성을 갖는지, 그리고 SW/HW co-design이 그것을 어떻게 가능하게 하는지 확인.

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

계보: [CAMEL Lab CXL 연구 계보](../CAMEL Lab CXL 연구 계보.md)

---

## TL;DR

Billion-point 그래프에서 **approximate nearest neighbor search (ANNS)**는 embedding table이 수십 TB에 달해 host DRAM만으로는 감당이 안 된다. 기존 우회책(compression·hierarchical/SSD-PMEM)은 정확도 또는 성능을 희생한다. CXL-ANNS는 모든 dataset을 **CXL memory pool**(Type 3 EP)에 disaggregate해 용량 문제를 없앤 뒤, CXL의 far-memory 특성이 만드는 성능 저하를 **ANNS 워크로드 지식으로** 극복한다: (1) 그래프 진입점(entry-node) 근처 hop에 몰리는 접근을 local DRAM에 캐싱(relationship-aware caching), (2) 다음에 방문할 노드를 미리 prefetch, (3) distance 계산을 EP 내부 가속기(DSA)로 near-data 처리 + vector sharding, (4) kNN subtask를 urgent/deferrable로 나눠 fine-granular 스케줄링. 결과적으로 SOTA billion-scale ANNS 대비 **QPS 111.1×·query latency 93.3%↓**, 무제한 용량 DRAM-only oracle보다도 latency 68.0%·throughput 3.8× 우위. 16nm FPGA 실물 프로토타입 + gem5 full-system 시뮬로 검증.

---

## Core thesis

> "While this CXL memory pool can make ANNS feasible to handle billion-point graphs without an accuracy loss, we observe that the search performance significantly degrades because of CXL's far-memory-like characteristics. To address this, CXL-ANNS considers the node-level relationship and caches the neighbors in local memory, which are expected to visit most frequently." (Abstract, p.585)

CXL는 용량을 준다(feasibility). 하지만 접근 지연은 DRAM보다 나쁘다(far-memory). 핵심 주장은 **"워크로드의 접근 구조를 알면 그 지연을 숨길 수 있다"**는 것 — ANNS의 graph traversal이 entry-node 근처 소수 hop에 집중된다는 성질을 활용해 무엇을 local에 두고 무엇을 pool에 둘지, 무엇을 prefetch할지, 무엇을 near-data로 계산할지를 결정한다. SW(그래프 캐싱·스케줄링)와 HW(EP 내 distance 가속기)의 협업이 이 극복의 수단이다.

---

## Why this matters to me

내 박사 방향은 **메모리 시스템 아키텍처(CXL disaggregation / multi-node coherence / PGAS-over-CXL)**이고, 방법론은 feasibility-by-building이다. 이 논문은 "CXL을 범용으로 붙이면 far-memory 페널티(여기선 oracle 대비 최대 3.9×)로 실패하지만, **워크로드 특화 지식을 SW/HW co-design에 주입하면 oracle을 능가한다**"는 것을 실물 FPGA로 증명한 대표 사례다. 이는 내 **H1(워크로드 특화 가설)**의 교과서적 근거다. 또한 "무엇을 local에 캐싱하고 무엇을 pool에 disaggregate하는가"의 배치 결정 문제, EP-side near-data compute, 그리고 fine-granular subtask scheduling은 내가 관심 있는 데이터 배치·coherence·계산 오프로딩 설계와 직접 겹친다. 다만 이 논문은 single-host·read-mostly search에 국한되므로, 내 multi-node coherence/PGAS 방향과는 scope가 갈린다(아래 Connection 참조).

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| Abstract | — | p.585 | CXL pool로 용량 해결, 워크로드 특화로 far-memory 극복. QPS 111.1×, latency 93.3%↓ |
| 1 | Introduction | p.585–586 | 3대 관찰: node-level relationship, latency hiding, dependency relaxation. Contribution 요약 |
| 2 | Background | p.586–588 | ANNS(BFS/kNN), billion-scale 우회책(compression·hierarchical) 한계, CXL sub-protocol·EP type |
| 3 | High-level Viewpoint of CXL-ANNS | p.588–591 | Challenge 분석(compression 정확도·hierarchical 지연·baseline 3.6~4.6×), 3대 설계 동기, collaborative overview(Fig.12) |
| 4 | Software Stack Design & Implementation | p.591–592 | Local caching for graph(SSSP hop count), CXL memory pool data placement(CXL arena, pool manager) |
| 5 | Collaborative Query Service Acceleration | p.592–593 | Distance calc in EP + vector sharding, prefetching, fine-granular query scheduling(urgent/deferrable) |
| 6 | Evaluation | p.593–596 | 16nm FPGA+gem5, 6 datasets, EPax/Cache/CXLA 기여 분해, scalability(bigger dataset·multi-host) |
| 7 | Discussion | p.596 | GPU-based distance calc가 부적합한 이유 |
| 8 | Conclusion | p.596 | 요약 |

---

## Section notes

### §1 Introduction (p.585–586)

Dense retrieval(=nearest neighbor search)에서 brute-force kNN은 선형 시간이라 billion-point에서 비현실적이고, 그래서 **ANNS**(query를 일부 이웃 subset으로 제한)가 쓰인다. 하지만 ANNS는 메모리 압력을 크게 키운다 — Bing/Outlook search engine은 "100B+ vectors, each being replicated by 100 dimensions, which consume more than 40TB memory space" (p.585), Alibaba e-commerce는 "2B+ vectors (128 dimensions)" (p.585)를 요구한다. CXL로 dataset을 disaggregate하면 용량은 해결되나 search 성능이 oracle 대비 "as high as 3.9× (§3.1)" (p.586) 저하된다.

세 가지 관찰이 설계를 이끈다: (i) **Relationship-aware graph caching** — 그래프 접근이 entry-node로부터 소수 edge hop에 집중, (ii) **Hiding the latency of CXL memory pool** — 다음 kNN candidate update 단계에서 처리될 이웃 dataset을 미리 prefetch, (iii) **Collaborative kNN search design in CXL** — EP controller가 distance를 near-memory로 계산하고 host는 graph traverse/candidate update를 담당, (iv) **Dependency relaxation and scheduling** — serial하게 묶인 subtask를 urgent/deferrable로 분류해 fine-granular 스케줄링. CXL memory pool은 "up to 4PB" (p.586) 확장 가능하고 host root-complex가 이를 system memory space로 매핑한다.

### §2 Background (p.586–588)

**ANNS/kNN**: best-first search(BFS)가 entry-node에서 시작해 query vector에 가까워지는 이웃으로 이동(Algorithm 1). Distance는 L2(Euclidean) 또는 angular로 계산(Fig.2). Graph는 preprocess되어 entry-node에서 최소 평균 edge hop으로 모든 노드에 도달 가능. **Billion-scale 우회책 두 부류**: (a) *Compression*(product quantization 등) — 벡터를 cluster centroid로 대체, embedding table은 줄지만 정확도 하락하고 그래프 데이터는 이득 없음, (b) *Hierarchical*(DiskANN, HM-ANN) — 전체 그래프·벡터를 SSD/PMEM에 두고 low-accuracy search + high-accuracy re-rank 2단계로 처리, 정확도는 지키나 저장장치 접근으로 지연 폭증.

**CXL**: 세 sub-protocol(CXL.io / CXL.cache / CXL.mem)과 EP type(Type1/2/3). Type 3 EP가 CXL-ANNS에 최적 — 내부 메모리가 **host-managed device memory (HDM)**로 노출되어 host physical address(HPA)에 매핑, load/store로 접근 가능(Fig.4). Scaling-out 시 CXL switch는 Type 3만 가능(CXL.cache의 virtual address는 switch가 목적지를 못 찾음). "up to 4095 CXL EPs" (Fig.4b, p.588).

### §3 High-level Viewpoint of CXL-ANNS (p.588–591)

**Challenge 분석**: Compression은 "It cannot even reach the threshold accuracy that ANNS needs to support (90%, recommended by [30]) after having 45.8% less data than the original" (§3.1, p.589) — 정확도 부족. Hierarchical은 "the storage accesses of the high-accuracy search account for 87.6% of the total kNN query latency, which makes the search latency of DiskANN and HM-ANN worse than that of the oracle ANNS by 29.4× and 64.6×, respectively, on average" (§3.1, p.589) — 지연 폭증. 반면 CXL로 그냥 disaggregate한 **baseline**(Fig.7)조차 oracle 대비 "3.6~4.6× performance degradation" (p.589)을 겪는데, 이는 모든 HDM 접근이 host RC에서 CXL flit로/부터 memory-to-flit 변환을 거치기 때문.

**3대 설계 동기**(§3.2): (i) *Node-level relationship* — "the nodes most frequently accessed during the 1M kNN searches reside in the 2~3 edge hops" (p.590, Fig.9b) → entry-node 근처는 local DRAM, 나머지는 CXL EP. (ii) *Distance calculation* — end-to-end를 4 subtask로 분해해 측정하니 distance calc가 "an average of 81.8%" (p.590, Fig.10) 차지, 그러나 연산량은 작아 단순 HW로 가속하기 좋음(embedding table lookup이 병목). (iii) *Reducing data vector transfers* — distance는 scalar 하나면 되므로 벡터 전체 대신 EP에서 계산하면 전송량을 "73.3×, on average" (p.590, Fig.11b) 줄임(data vector 길이가 graph data의 "2.0× greater", p.590). Collaborative overview(Fig.12): RC-side SW stack(query scheduler·pool manager·kernel driver) + EP-side HW stack(DSA·CXL engine·PHY).

### §4 Software Stack Design & Implementation (p.591–592)

**Local caching for graph**(§4.1): pool manager가 **SSSP**(single source shortest path)로 entry-node에서 각 노드까지 edge hop count를 계산, hop count 오름차순으로 정렬해 hop이 가장 작은(가장 자주 접근되는) 노드부터 local DRAM 용량이 허용하는 만큼 캐싱. Local 용량은 `sysconf()`의 `_SC_AVPHYS_PAGES`·`_SC_PAGESIZE`로 추정. **Data placement**(§4.2): kernel driver가 PCIe enumeration에서 각 HDM을 HPA에 contiguous 매핑, pool manager는 이를 **CXL arena**(per-arena 연속 virtual address space)로 노출. 벡터(embedding table)는 stack-like allocator로 대용량 연속 공간에, variable-length(16B~1KB) 이웃 리스트는 buddy-like allocator로 관리, 벡터는 여러 CXL arena에 round-robin sharding(Fig.14).

### §5 Collaborative Query Service Acceleration (p.592–593)

**Distance calc in EP**(§5.1): DSA의 processing element(PE)가 multiplier+subtractor arithmetic tree로 L2/angular를 element-wise 계산(Fig.15a). **Vector sharding** — 각 벡터를 EP I/O granularity(256B) 단위 sub-vector로 쪼개 여러 EP에 분산, 각 EP가 sub-distance를 계산하고 CPU가 누적(EP당 backend DRAM bandwidth 최대화). **Interface**(Fig.16): doorbell/command buffer/result register를 user-level virtual address에 매핑해 context switch 최소화. **Prefetching**(§5.2): query scheduler가 다음 iteration에서 방문할 노드를 speculate해 이웃 정보를 미리 로드 — "82.3% of the total visiting nodes are coming from the candidate array (even though its information is not updated for the next step)" (p.593, Fig.18)라는 관찰에 기반. **Fine-granular scheduling**(§5.3): distance 결과를 기다리며 CPU가 "42% of the total execution time" (p.593, Fig.19) idle → candidate update를 urgent(insert)/deferrable(sort·node selection)로 나눠 distance 계산 중에 deferrable을 겹쳐 실행.

### §6 Evaluation (p.593–596)

**Setup**: 16nm FPGA 실물 프로토타입 — RISC-V CPU, 4 ANNS EP(각 4 memory controller), CXL switch 연결, Linux 5.15.36, Meta FAISS v1.7.2. 유연성 위해 gem5 hardware-validated full-system 시뮬 병행(Table 1: 40 O3 core ARM v8 3.6GHz, local 128GiB, CXL pool 256GiB/device, 4× Intel Optane 900P). 6개 billion-scale dataset(Table 2: BigANN·Yandex-T·Yandex-D·Meta-S·MS-T·MS-S, 모두 1B vector, dim 96~256), recall@k 0.9 기준. 비교군: Comp(product quantization)·Hr-D(DiskANN)·Hr-H(HM-ANN)·Orcl(무제한 DRAM)·Base(CXL pool + CPU가 subtask)·EPax(+DSA distance)·Cache(+relationship-aware caching·prefetch)·CXLA(+fine-granular scheduling, 전체 제안).

**결과 분해**(Fig.22, 24, Table 3): Base는 Hr-D/H 대비 QPS "9.4× and 20.3×" (p.595) 개선하나 여전히 Orcl보다 "3.9× lower throughput" (p.595, memory-to-flit 변환 탓 graph traverse·distance calc가 각 2.6×·4.3× 느림). EPax는 distance calc 시간을 "a factor of 119.4×" 줄이고 query latency를 "7.5× on average" 개선, 결과적으로 "EPax's latency is 1.9× lower than Orcl's" (p.595), data vector 전송을 "21.1×" 감소. Cache는 graph traversal을 "3.3×", query latency를 "32.7%" 개선(BigANN/Yandex-T는 그래프가 작아 92.0% local 처리). CXLA는 Cache 대비 QPS "15.5%" 추가, idle을 "1.3×" 줄이고 utilization "20.9%" 향상. 종합 CXLA는 Orcl을 "an average factor of 3.8×" (p.596) QPS로 능가. **Scalability**: bigger dataset(4B)에서도 CXLA가 Orcl 대비 "2.7× lower latency" (§6.4, p.596); multi-host는 4 CXL host까지 QPS 증가하나 6 host에선 PE 부족이 병목(더 많은 EP로 해소).

### §7 Discussion (p.596)

GPU-based distance calc가 부적합한 이유 두 가지: (1) GPU는 host SW/HW layer와의 상호작용이 필요해 data transfer overhead 발생, (2) ANNS distance는 몇 개의 단순 lightweight vector unit이면 충분해 GPU는 cost-inefficient. CXL-ANNS는 데이터 실제 위치 근처에서 처리하고 compact result만 반환하므로 data movement 부담이 없다.

### §8 Conclusion (p.596)

CXL memory pool에 전체 dataset을 두어 billion-point 그래프를 다루면서, inter-node relationship과 ANNS-aware prefetch·EP-side distance 계산·fine-granular scheduling으로 (local-DRAM only) oracle에 필적하거나 이를 능가. QPS 111.1× / oracle 대비 3.8×.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "software-hardware collaborative memory disaggregation and computation"
- "make ANNS feasible to handle billion-point graphs without an accuracy loss"
- "CXL's far-memory-like characteristics"
- "considers the node-level relationship"

**Technical concepts:**
- "relationship-aware graph caching"
- "host-managed device memory (HDM)" / "host physical address (HPA)"
- "domain specific accelerator (DSA)" for near-data distance calculation
- "vector sharding" (split each feature vector into sub-vectors per EP I/O granularity)
- "dependency relaxation and scheduling" / "urgent vs. deferrable subtasks"
- "CXL arena" (per-arena continuous memory space at user-level)
- "foreseeing technique" (ANNS-aware prefetch of next neighbor's dataset)

**Value language:**
- "highly scalable" / "billion-scale"
- "even better than those of the oracle system"
- "collaboratively search for nearest neighbors in parallel"

> ⚠ **피해야 할 어휘** (paper-signature, 직접 echo하면 모방으로 보임):
> - "CXL-ANNS" (제품명)
> - "relationship-aware graph caching" (이 논문 고유 명명 — 개념은 재구성해서 쓰되 이 phrase 그대로는 지양)
> - "urgent/deferrable subtasks" (이 논문 scheduling 시그니처)
> - "111.1× higher QPS" (이 논문 대표 수치 — 내 것처럼 인용 금지, 반드시 출처 표기)

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Abstract, p.585 | "111.1× higher QPS with 93.3% lower query latency than state-of-the-art ANNS platforms" | CXL 워크로드 특화 co-design의 대표 성과 |
| Abstract, p.585 | oracle DRAM-only 대비 "68.0% and 3.8×, in terms of latency and throughput" 우위 | "CXL이 무제한 DRAM보다 나을 수 있다"는 반직관적 근거 |
| §1/§3.1, p.586/589 | 순진한 CXL disaggregation은 oracle 대비 "as high as 3.9×" 성능 저하 | far-memory 페널티의 정량화 → 특화가 필요한 이유 |
| §3.1, p.589 | hierarchical(DiskANN/HM-ANN)은 high-accuracy search 저장접근이 "87.6% of the total kNN query latency", oracle 대비 "29.4× and 64.6×" 느림 | SSD/PMEM 계층화 우회의 한계 |
| §3.1, p.589 | compression은 "45.8% less data" 시점에도 "90%" 정확도 임계 미달 | 압축 우회의 한계 |
| §1, p.585 | Bing/Outlook: "100B+ vectors ... consume more than 40TB memory space"; Alibaba "2B+ vectors (128 dimensions)" | embedding 워크로드가 왜 수십TB를 요구하는가(모티베이션) |
| §3.2, p.590 | distance calculation이 end-to-end의 "an average of 81.8%"; graph 접근은 "2~3 edge hops"에 집중 | 워크로드 접근 구조 → 배치·오프로딩 설계 근거 |
| §3.2, p.590 | EP near-data 계산으로 전송량 "73.3×" 감소 (data vector가 graph의 "2.0×") | near-data compute의 정량 이득 |
| §6.2, p.595 | EPax: distance calc "119.4×"↓, latency "7.5×"↓, Orcl보다 "1.9× lower" | HW distance 가속 기여 분해 |
| §5.3/§6.3, p.593/596 | CPU idle "42%" → CXLA가 utilization "20.9%" 향상 | fine-granular scheduling 기여 |
| §6.1, Table 1 | 16nm FPGA 프로토타입 + gem5, 4 ANNS EP, 6 billion-scale datasets, recall@0.9 | feasibility-by-building 증거로 인용 |

---

## 🎯 Strategic anchor

> "While this CXL memory pool can make ANNS feasible to handle billion-point graphs without an accuracy loss, we observe that the search performance significantly degrades because of CXL's far-memory-like characteristics. To address this, CXL-ANNS considers the node-level relationship and caches the neighbors in local memory, which are expected to visit most frequently." (Abstract, §Abstract, p.585)

→ **본인 활용**: 면담·자소서에서 **H1(워크로드 특화 가설)**의 근거로. "CXL은 용량이라는 feasibility는 주지만 far-memory 페널티를 동반한다. CXL-ANNS(ATC'23, p.585)가 보인 것은 *워크로드의 접근 구조(여기선 ANNS의 2~3 hop 집중)를 알면 그 페널티를 SW/HW co-design으로 숨겨 oracle을 능가할 수 있다*는 것이다. 나는 이 '워크로드 특화로 disaggregation 페널티를 극복한다'는 명제를 [내 대상 워크로드]로 확장하려 한다"로 사용. paper의 최대 강점 문장이 곧 내 방향의 정당화.

---

## Connection to my research direction

| 차원 | 이 paper (CXL-ANNS) | 본인 방향 |
|---|---|---|
| Scope | single-host, read-mostly ANN search | multi-node coherence / PGAS-over-CXL, read-write 공유 |
| Mechanism | 워크로드 지식 기반 static 배치(hop count 캐싱) + EP near-data distance | coherence protocol·주소 변환·계산 배치 (동적 공유 상태) |
| Workload | billion-scale ANNS(embedding table) — 특정 워크로드 특화 | 워크로드 특화를 방법론으로 채택하되 대상은 재선정 |
| Data placement | SSSP hop count로 local vs. pool 결정 | 유사한 배치 문제를 coherence·write 존재 하에서 |
| Open space | multi-host는 rerank로만 처리, coherence 없음 | 바로 이 공백 — multi-node에서 공유·일관성 |

CXL-ANNS는 "**한 워크로드의 접근 성질을 알면 CXL far-memory 페널티를 극복한다**"를 single-host·read-only 환경에서 완결적으로 증명했다. 내 방향은 이 명제를 (a) **write/공유가 존재하는 multi-node coherence** 환경으로 끌어올리는 것이다 — CXL-ANNS의 multi-host 절(§6.4)은 embedding table을 partition해 각 host가 독립적으로 kNN을 찾고 마지막에 rerank로 합치는, 사실상 **shared-nothing partition**이지 coherent shared memory가 아니다. 즉 이 논문이 남긴 가장 큰 공백은 "여러 host가 같은 CXL 데이터를 일관성 있게 공유하며 계산"하는 문제이고, 그것이 내 자리다. 방법론(FPGA 실물 + gem5 검증, SW/HW co-design)은 그대로 계승한다.

---

## Open questions / gaps

- [ ] Multi-host가 embedding partition + rerank(shared-nothing)에 그침 — **write-shared / coherent** multi-node ANNS나 update-heavy 워크로드(streaming insert)에서의 CXL 일관성은 미해결
- [ ] 배치가 SSSP hop count 기반 **static** — dynamic graph(FreshDiskANN류 스트리밍 삽입) 하에서 재캐싱·재배치 비용 미다룸
- [ ] Distance 가속을 EP-side DSA로 고정 — 다른 워크로드(GNN, recommendation embedding aggregation)로의 일반화 여지
- [ ] CXL.cache 기반 coherent 공유는 명시적으로 배제(Type 3만 사용, switch 제약) — coherent pooling 자체의 설계 공간은 열려 있음
- [ ] Fault tolerance는 별도 논문([62] Failure tolerant training over CXL)으로 분리 — disaggregation의 신뢰성/복구는 본문 밖

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [42] | Li et al., "Pond: CXL-based memory pooling systems for cloud platforms," ASPLOS 2023 | Cloud CXL pooling 대표작 — 내 disaggregation 배경 |
| ☐ | [43] | Al Maruf et al., "TPP: Transparent page placement for CXL-enabled tiered-memory," ASPLOS 2023 | OS/kernel tiered-memory 배치 — 내 OS/kernel 관심축 |
| ☐ | [62] | Kwon, Jang, Choi, Lee, Jung, "Failure tolerant training with persistent memory disaggregation over CXL," IEEE Micro 2023 | 같은 그룹, CXL disaggregation 신뢰성 — 계보 인접작 |
| ☐ | [41] | CXL Consortium, "Compute Express Link 3.0 white paper," 2022 | CXL 3.0 spec — coherence/switch 근거 원전 |
| ☐ | [24] | Subramanya et al., "DiskANN," NeurIPS 2019 | hierarchical baseline 원전 |
| ☐ | [25] | Ren, Zhang, Deng, "HM-ANN," NeurIPS 2020 | heterogeneous memory ANN baseline |
| ☐ | [60] | Kwon, Gouk, Lee, Jung, "Hardware/software co-programmable framework for computational SSDs (DeepStore류)," FAST 2022 | 같은 그룹 near-data 방법론 계보 |
| ☐ | [57] | Gupta et al., "The architectural implications of Facebook's DNN-based personalized recommendation," HPCA 2020 | embedding 워크로드 특성 근거 |

---

## Personal annotations

<자유 형식 메모. 본인이 paper 읽으며 떠올린 생각·이견·후속 아이디어. user가 직접 추가하는 영역.>
