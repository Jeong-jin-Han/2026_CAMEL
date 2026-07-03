---
title: "GraphTensor: Comprehensive GNN-Acceleration Framework for Efficient Parallel Processing of Massive Datasets"
aliases: [GraphTensor]
type: paper
status: read
tags: [paper, cluster/cxl, camel-cxl-lineage]
---

# GraphTensor: Comprehensive GNN-Acceleration Framework for Efficient Parallel Processing of Massive Datasets

> **Source PDF**: [GraphTensor.pdf](GraphTensor.pdf)
> **Authors**: Junhyeok Jang, Miryeong Kwon, Donghyun Gouk, Hanyeoreum Bae, Myoungsoo Jung (KAIST, Computer Architecture and Memory Systems Laboratory)
> **Venue / Year**: IEEE IPDPS 2023 (arXiv:2305.17469v1, 27 May 2023)
> **arXiv / DOI**: arXiv:2305.17469 · 오픈소스 https://graphtensor.camelab.org
> **Length**: 11 pages
> **Read status**: ☐ Skim · ☐ Partial · ☑ Full read (2026-07-04)
> **My reading purpose**: CAMEL Lab CXL 연구 계보 이해. 이 논문은 **CXL 직접 아님** — GNN 라인(PreGNN CAL'22 → GraphTensor → AutoGNN)의 앞단. '대규모 그래프가 단일 GPU 메모리에 안 들어가는 문제'를 소프트웨어(preprocessing·kernel scheduling)로 푼 사례로, CXL memory disaggregation이 **하드웨어로** 같은 문제를 어떻게 다르게 겨냥하는지 대조하기 위해 읽음.

> 계보: [CAMEL Lab CXL 연구 계보](../CAMEL Lab CXL 연구 계보.md) — Phase 2(2023) · 인접(GNN 라인).

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

**GraphTensor**는 대규모 그래프에 대한 GNN 학습을 GPU에서 처음(graph sampling)부터 끝(dense tensor 연산)까지 통합 가속하는 오픈소스 프레임워크다. 기존 GNN 확장 프레임워크가 (i) DL 연산을 재활용하거나 그래프 처리를 흉내내면서 GNN 본질을 제대로 못 살려 memory/cache 관리가 비효율적이고, (ii) aggregation을 먼저 하는 static kernel scheduling이 node embedding의 dimensionality reduction을 인지하지 못하며, (iii) graph sampling·embedding lookup·data transfer 같은 **GNN-specific preprocessing**이 end-to-end latency의 대부분을 차지한다는 세 가지 근본 문제를 지적한다. 이를 **NAPA(NeighborApply-Pull-and-Apply)** 라는 destination-centric·feature-wise programming model, **dynamic kernel placement(DKP)** 로 aggregation/combination 순서를 런타임에 재배치하는 kernel orchestrator, preprocessing을 subtask로 쪼개 파이프라인화하는 **service-wide tensor scheduler**로 해결한다. DGL·PyG 대비 학습 **1.4×**, multi-threaded graph sampling 대비 **2.4×** 빠르다.

---

## Core thesis

> "GraphTensor offers a set of easy-to-use programming primitives that appreciate both graph and neural network execution behaviors from the beginning (graph sampling) to the end (dense data processing)." (Abstract, p.1)

> "We observe that the preprocessing latency for large-scale graphs accounts for 84.2% of the total GNN processing time, on average." (§I, p.1)

추가 설명: GNN은 sparse한 그래프 처리와 dense한 신경망 연산이 한 파이프라인에 섞여 있다. 기존 프레임워크는 둘 중 한쪽(DL 연산 재활용 = DL-approach, 그래프 처리 흉내 = Graph-approach)에만 최적화돼 있어 나머지 쪽에서 memory bloat 또는 cache bloat이 생긴다. GraphTensor는 GNN 연산 자체를 destination(dst)-node 중심·feature-wise로 재정의하고(NAPA), 커널 실행 순서를 데이터 차원에 따라 자율적으로 바꾸며(DKP), 병목의 84.2%인 preprocessing을 서비스 전역에서 병렬·파이프라인화한다.

---

## Why this matters to me

이 논문의 출발점은 **"수백만~수십억 노드 규모의 그래프가 단일 GPU 메모리에 통째로 안 들어간다"** 는 memory capacity wall이다. 그래서 필요한 노드만 뽑아 오는 sampling·transfer(=preprocessing)가 강제되고, 이게 전체 시간의 84.2%를 잡아먹는다. GraphTensor는 이 문제를 **소프트웨어**로(sampling·kernel scheduling 최적화, 고정된 GPU 메모리 안에서) 우회한다. 반면 내 관심 방향인 **CXL memory disaggregation**은 같은 "데이터가 메모리에 안 들어감" 문제를 **하드웨어**로(byte-addressable하게 메모리 용량을 확장) 정면 돌파한다. 즉 이 논문은 CXL 직접 논문이 아니라, CXL이 겨냥하는 동기의 **소프트웨어적 대척점**으로서 계보 이해에 필요하다. "데이터가 안 들어가면 어디까지 소프트웨어로 버틸 수 있는가, 그 벽이 어디서 하드웨어(CXL)로 넘어가는가"를 대조하는 참조점이다.

---

## Structure overview

| § | Title | Pages | Key takeaway |
|---|---|---|---|
| I | Introduction | p.1 | GNN 데이터 처리 저성능의 세 근본 원인 + preprocessing이 84.2% 차지. 3개 연구 컴포넌트 제시 |
| II | Preliminaries | p.2-3 | GNN aggregation/combination, COO/CSR/CSC 저장 포맷, neighbor sampling·data preparation(reindex·lookup·transfer) |
| III | Emerging GNN Frameworks | p.4-5 | DL-approach(memory bloat 5.8×) vs Graph-approach(cache bloat 1.8×, format translation) 분석 |
| IV | GraphTensor (개요·Frontend) | p.5-6 | 전체 아키텍처(Fig 7), NAPA destination-centric·feature-wise primitives. preprocessed 그래프 degree가 3.4× 작음 |
| V | Backend: Kernel & Tensor Mgmt | p.6-8 | Kernel orchestrator(dimensionality reduction, DKP, cost model 12.5% 오차) + service-wide tensor scheduler(subtask 병렬·lock contention 완화) |
| VI | Evaluation | p.8-9 | Base-GT/Dynamic-GT/Prepro-GT. 학습 1.4×, preprocessing 48.5% 단축, FLOPs 5.4× 감소 |
| VII | Related Work | p.9-10 | DL-approach·Graph-approach·기타 프레임워크 비교(Table III) |
| VIII | Conclusion | p.10 | 1.4× 학습, 2.4× 종합. 오픈소스 |

---

## Section notes

### §I Introduction (p.1)

GNN은 그래프의 자연적 데이터 처리 때문에 CNN·Transformer와 다르고, 여러 node feature vector(embedding)를 sparse하게 순회·처리해야 한다. 기존 연구는 DL 프레임워크(TensorFlow/PyTorch)를 확장(DL-approach)하거나 그래프 처리를 흉내(Graph-approach)낸다. 논문은 저성능의 **세 근본 원인**을 든다: (1) 프레임워크가 GNN 본질을 완전히 이해하지 못한 채 부분적 vertex-centric 연산만 해 memory/cache 관리가 비효율 — DL-approach는 sparse tensor를 dense tensor로 바꾸며 GPU 내부 메모리를 낭비하고, graph-processing approach의 edge-wise scheduling은 GNN용이 아니다. (2) node feature를 먼저 aggregate한 뒤 MLP로 변환하는 static kernel scheduling이 node embedding의 dimensionality reduction을 모른다. (3) GNN-specific preprocessing(graph sampling, embedding lookup, data transfer)의 긴 latency.

> "We observe that the preprocessing latency for large-scale graphs accounts for 84.2% of the total GNN processing time, on average." (§I, p.1)

GraphTensor primitive는 **315K 이상의 서로 다른 GNN 설계**를 표현할 수 있고, 세 연구 컴포넌트로 구성: i) pure vertex-centric GNN computing, ii) dynamic kernel placement, iii) end-to-end latency reduction. GCN 대비 emerging 프레임워크보다 학습 2.0× 빠르고, advanced multi-threaded 버전 대비 서비스 레벨 end-to-end를 2.4× 단축한다.

### §II Preliminaries (p.2-3)

**Graph data representation**: 그래프는 adjacency(sparse matrix)와 embedding table로 표현. 저장 포맷 세 가지 — **COO**(coordinate list, edge-centric, src/dst 배열, edge ID로 인덱싱, 저장 오버헤드 큼), **CSR**(compressed sparse row, dst VID로 인덱싱, vertex-centric), **CSC**(compressed sparse column, src VID로 인덱싱). per-vertex embedding은 n차원 dense vector(embedding table).

**GNN model**: 두 연산 — **aggregation**(이웃 embedding을 함수 f로 누적, 예: arithmetic mean)과 **combination**(누적 embedding을 MLP로 변환). layer마다 hop별 subgraph를 처리, batch로 여러 vertex 동시 처리. **edge weighting**(g, h 함수)으로 이웃별 중요도 반영(NGCF류). **Training**: FWP(forward propagation)는 CSR이, BWP(back propagation)는 전치·역순 순회 때문에 CSC가 적합.

**Preprocessing for GNNs**: 목표는 계산·전송할 vertex 수를 줄이는 것. **Neighbor sampling**은 dst의 인접 노드를 sampling priority(예: unique random)로 제한 수 뽑고, hash table로 sampled 노드에 새 VID를 0부터 할당. **Data preparation**은 graph reindexing(subgraph를 renumber해 COO/CSR/CSC로 GPU 연속 메모리에 복사), embedding lookup(원 VID로 global embedding table 스캔해 새 table 구성), transfer(host→GPU 복사)로 구성. sampling/preparation은 여러 table을 불규칙하게 순회·복사하는 time-consuming task.

### §III Emerging GNN Frameworks (p.4-5)

**DL-leveraging approach(DL-approach)**: DL 연산 재활용 위해 sparse-to-dense data conversion 필요 — embedding table에 흩어진 이웃 embedding을 모아 matrix로 만든 뒤 scatter_sum/scatter_means 같은 DL 커널로 aggregate. 문제는 **memory bloat**: 여러 dst가 공유하는 이웃 embedding이 dense matrix에 중복 저장.

> "on average, the memory bloat increases the memory footprint by 5.8×." (§III, p.4, Fig 6a)

**Graph-simulation approach(Graph-approach)**: sparse matrix multiplication(SpMM, aggregation)과 sampled dense-dense matrix multiplication(SDDMM, edge weighting)으로 그래프+embedding 처리. memory bloat은 없지만 두 가지 다른 문제: (1) **format translation** — SDDMM은 COO를 쓰지만 SpMM은 src 정보가 필요해 COO→CSR 변환, BWP는 COO→CSC 변환 필요. (2) **cache bloat** — edge당 thread block 할당(edge-wise scheduling), 같은 dst의 embedding이 여러 SM에 중복 로드.

> "cache bloat needlessly loads an average of 81.9% more data to the cache." (§III, p.5, Fig 6b)

### §IV GraphTensor: 개요 & Frontend (p.5-6)

**High-level view(Fig 7)**: Frontend는 GNN-specific programming interface(NAPA), Backend는 kernel orchestrator + service-wide tensor scheduler. Frontend는 **NeighborApply-Pull-and-Apply(NAPA)** programming model로 edge weighting·aggregation·combination을 destination-centric하게 처리 — src node가 아니라 dst node 주변 그래프를 탐색하고 edge가 아닌 dst 중심으로 embedding 처리를 병렬화. 이로써 sparse-to-dense conversion(→memory bloat)과 feature-wise thread scheduling 위의 cache bloat을 제거.

**GNN graph aware scheduling**: preprocessed 그래프는 per-node edge 수가 적고 degree가 매우 균일하다.

> "the average degree of the preprocessed graphs is 3.4 times smaller than that of the original graphs." (§IV, p.5, Fig 8a)

따라서 edge당 병렬화(edge-wise)보다 **node당 embedding 처리 병렬화**가 GNN에 유리. 또한 GNN feature는 scalar가 아닌 고차원이므로 **feature-wise manner**로 dst node의 feature들을 같은 SM에 묶어 처리(Fig 9a).

**Primitives**: (i) **NeighborApply**(CSR만으로 SDDMM=edge weighting 완전 실현, sparse-to-dense 변환 없이 g를 embedding에 직접 적용, dst embedding을 한 번만 로드해 재사용), (ii) **Pull**(NeighborApply가 계산한 weight와 src embedding을 feature-wise로 aggregate, output 재활용), (iii) **Apply**(combination, dense matrix 변환이라 GPU MLP와 잘 조화, tf.matmul 등 활용). 사용자는 mode 재설정만으로 다양한 GNN 모델 구현 가능(Fig 10, NGCF 예시).

### §V Backend: Kernel & Tensor Management (p.6-8)

**A. GNN Kernel Orchestrator** — embedding 차원을 줄여 latency 단축.
- **Dimensionality reduction(Fig 11a)**: aggregation은 src embedding 수 n_Src를 n_Dst로 줄이고(높이 감소), combination은 feature 수 n_Feature를 hidden layer 폭 n_Hidden으로 줄인다(폭 감소). 어느 함수를 먼저 하느냐로 input tensor 크기가 달라짐.
> "all layers of wiki-talk can reduce the input tensor size by 31.7%, on average, while other layers can still take advantage by following the conventional execution order." (§V, p.6)
- **Dynamic kernel placement(DKP)**: combination이 aggregation보다 feature를 더 많이 줄일 수 있으면 두 커널 순서를 런타임에 재배치. DFG(dataflow graph)에서 Pull과 MatMul을 찾아 host-side에 **Cost-DKP** 노드를 미리 준비해 링크 교체. matrix 결합·전치 규칙(σ(Wf(h(X))+b), aggregation을 XA^T로 재작성)으로 정당화.
- **DKP cost model(Table I)**: reduction factor × kernel execution factor로 aggregation-first vs combination-first latency를 노드·embedding 수 기반으로 추정. least-squares estimation으로 파라미터 fitting.
> "the estimated times are close to the actual latency (only 12.5% error)." (§V, p.7)

**B. Service-Wide Tensor Scheduling** — preprocessing이 end-to-end의 대부분을 차지.
> "the latency of GNN computing (FWP+BWP) only accounts for 15.8% of the end-to-end latency, on average." (§V-B, p.7)

preprocessing을 **S(sampling)·R(reindex)·K(embedding lookup)·T(transfer)** subtask로 분해. 기존 프레임워크는 직렬 실행으로 자원 낭비(S·R·K는 PCIe 불필요, T는 코어 1개만 사용). **High-performance preprocessing**: 각 컴포넌트를 subtask로 쪼개 data type·dependency 인지하며 multi-thread로 병렬·파이프라인(Fig 13). **Relaxing contention(Fig 14)**: S·R이 hash table(공유 자원)을 갱신해 lock contention 발생(S1/S2 간 47.4%, S·R 간 39.0%). S subtask를 algorithm part(A)와 hash table update(H)로 나눠 A는 완전 병렬화. K의 output buffer를 page-locked(pinned) memory에 두어 T와 파이프라인.

### §VI Evaluation (p.8-9)

**Method**: PyG 1.7.0, DGL 0.8.2, GNNAdvisor 비교. 세 버전 — **Base-GT**(NAPA, DKP 없음), **Dynamic-GT**(DKP+kernel orchestrator), **Prepro-GT**(DKP+service-wide tensor scheduler). TensorFlow 2.4.0, NVIDIA RTX 3090(82 SM, 24GB GDDR6X), Intel Xeon Gold 5317. 모델: GCN, NGCF(hidden dim 64). 10개 그래프(OGB/GraphSAINT/SNAP), feature dim 4K 기준 light/heavy feature graph 분류. batch = 300 vertices.

**Performance**:
> "Base-GT shows 1.5× and 1.3× shorter GCN/NGCF training latency across all the workloads." (§VI, p.8) — NAPA의 destination-centric·feature-wise scheduling이 format translation·memory/cache bloat 제거.
> "NAPA's scheduling reduces the memory footprint of FWP/BWP by 81.8%, on average ... reduces the amount of data loaded to cache by 44.8%, on average." (§VI, p.8, Fig 17)
> "GCN and NGCF training latency of our Dynamic-GT outperforms even Base-GT by 47.7% and 74.2%, respectively." (§VI, p.8)
> "Dynamic-GT rearranges the kernel to follow the best execution order, reducing the dimension of embeddings by 4.1×, on average." (§VI, p.8)
> "Dynamic-GT reduces the FLOPs by 5.4× as well as global memory access by 1.4×, on average." (§VI, p.8, Fig 18)

**End-to-end(§VI-B, p.9)**: DGL·Dynamic-GT는 PyG보다 7.4% 우수. SALIENT(SOTA preprocessing overlap)는 end-to-end를 light/heavy 각 19.7%/51.1% 단축.
> "Prepro-GT can further reduce the end-to-end latency by 1.7×, on average." (§VI-B, p.9) — service-wide tensor scheduler가 dependency chain을 완화·병렬화.
> "it can shorten the preprocessing latency by 48.5%, on average." (§VI-B, p.9, Fig 20) — Prepro-GT가 embedding lookup·data transfer를 Dynamic-GT보다 14.9%/48.5% 먼저 완료.

### §VII Related Work (p.9-10)

DL-approach(GNNAdvisor, NeuGraph, FlexGraph)는 sparse-to-dense 변환의 memory/cache bloat 문제. Graph-approach(FeatGraph, ROC)는 memory bloat은 없으나 edge-wise scheduling으로 cache bloat·format translation. DGL·Featgraph는 preprocessing에 multi-thread를 쓰나 성능 제한. FlexGraph·NextDoor는 전체 그래프가 GPU 메모리에 들어간다고 가정 → 대규모 그래프 처리 불가. PaGraph는 embedding caching으로 transfer 단축하나 dataset·user behavior 의존. Table III에 프레임워크별 initial format·memory bloat·format translation·cache bloat·preprocessing overhead 유무 비교. **어느 프레임워크도 dimensionality reduction을 인지해 커널 순서를 재배치하지 않음** — GraphTensor의 차별점.

### §VIII Conclusion (p.10)

GraphTensor는 modern GNN 프레임워크(DGL, PyG) 대비 학습 **1.4×**, 다양한 large-scale 그래프 워크로드에서 **2.4×** 향상. 오픈소스, Samsung(SRFC-IT2101-04)·IITP·NRF 지원, Myoungsoo Jung 교신저자, 특허 보호.

---

## Key vocabulary (for own writing)

**Thesis / framing:**
- "appreciate both graph and neural network execution behaviors from the beginning to the end"
- "pure vertex-centric GNN computing"
- "GNN-specific preprocessing sits on the critical path in GNN computing"

**Technical concepts:**
- "destination-centric, feature-wise" (processing)
- "dimensionality reduction for node embeddings"
- "dynamic kernel placement (DKP)"
- "service-wide tensor scheduler"
- "memory bloat" / "cache bloat" / "format translation"
- "sparse-to-dense data conversion"
- "neighbor sampling", "graph reindexing", "embedding lookup"

**Value language:**
- "removes unnecessary global memory accesses"
- "shorten the preprocessing latency"
- "efficient parallel processing of massive datasets"

> ⚠ **피해야 할 어휘** (paper-signature — 그대로 echo하면 모방으로 보임):
> - "NeighborApply-Pull-and-Apply (NAPA)" — 이 논문 고유 primitive 이름
> - "Base-GT / Dynamic-GT / Prepro-GT" — 이 논문 고유 시스템 명명
> - "self-governing manner" — 이 논문 특유의 표현

---

## Citable quantitative data

| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §I, p.1 | "preprocessing latency ... accounts for 84.2% of the total GNN processing time, on average" | 대규모 그래프에서 데이터 이동/준비가 연산을 압도한다는 memory-wall 동기 |
| §V-B, p.7 | "GNN computing (FWP+BWP) only accounts for 15.8% of the end-to-end latency" | 위와 짝. 실제 연산은 소수, 나머지는 데이터 준비 |
| §III, p.4 | "memory bloat increases the memory footprint by 5.8×" | GPU 메모리 용량 압박 = disaggregation 동기 방증 |
| §III, p.5 | "cache bloat ... loads an average of 81.9% more data to the cache" | edge-wise scheduling의 메모리 비효율 |
| §IV, p.5 | "average degree of the preprocessed graphs is 3.4 times smaller than ... original" | sampling이 그래프 구조를 바꾼다는 정량 근거 |
| §VI, p.8 | "reduces the memory footprint of FWP/BWP by 81.8%" | destination-centric 재설계의 메모리 절감 |
| §VI, p.8 | "reducing the dimension of embeddings by 4.1×" / "reduces the FLOPs by 5.4×" | 커널 순서 재배치의 연산 절감 |
| §VI-B, p.9 | "shorten the preprocessing latency by 48.5%, on average" | preprocessing 파이프라인화 효과 |
| Abstract/§VIII | 학습 "1.4×", 종합 "2.4×" | 프레임워크 전체 성능 이득 |

---

## 🎯 Strategic anchor

> "We observe that the preprocessing latency for large-scale graphs accounts for 84.2% of the total GNN processing time, on average." (§I, p.1)
> — 그리고 그 preprocessing의 정체는 "reducing the number of vertices to compute and transfer the input graph between those two memories [host↔GPU]" (§II-B, p.3), 즉 **그래프가 GPU 메모리에 안 들어가서** host에서 필요한 부분만 sampling·transfer해야 하는 데서 온다.

→ **본인 활용**: 면담·자소서에서 "GraphTensor는 대규모 그래프가 GPU 메모리에 안 들어가 생기는 preprocessing 병목(84.2%)을 **소프트웨어**로 완화했지만, 근본 원인인 host-device 메모리 분리는 그대로 둔다. 저는 이 memory capacity wall을 **CXL memory disaggregation**으로 하드웨어에서 해소하는 쪽에 관심이 있다 — 데이터를 옮기지 않고 byte-addressable하게 접근하면 sampling·transfer 병목 자체가 재정의된다"로 계보 대조에 사용.

---

## Connection to my research direction

| 차원 | 이 paper (GraphTensor) | 본인 방향 (메모리 시스템 아키텍처 / CXL) |
|---|---|---|
| Scope | 단일 GPU 위 GNN 학습 프레임워크(SW) | 메모리 계층·상호연결 아키텍처(HW/AT + OS/kernel) |
| Root cause 인식 | 대규모 그래프가 GPU 메모리에 안 들어감 → preprocessing 84.2% | 동일 인식: memory capacity/bandwidth wall |
| Mechanism | sampling·kernel scheduling 최적화로 **데이터를 줄이거나 겹쳐** 옮김 | CXL로 메모리를 **확장·공유**해 옮김 자체를 줄임 (feasibility-by-building) |
| Workload | GNN(GCN, NGCF), sparse graph + dense NN | 메모리 집약 워크로드 일반 (GNN 포함 가능) |
| Open space | host↔GPU 메모리 분리는 전제로 고정 | 그 분리를 CXL로 재구성 (disaggregation, coherence) |

GraphTensor는 "메모리가 고정돼 있다"는 전제 하에 데이터 이동을 **소프트웨어로 최소화**한다. 내 방향은 그 전제 자체(host-device 메모리 경계)를 **하드웨어로 재설계**한다. 두 접근은 배타적이지 않고 상보적이다 — CXL로 메모리를 확장해도 어떤 데이터를 어떤 순서로 접근할지는 여전히 스케줄링 문제이므로, GraphTensor의 dimensionality-aware kernel placement·subtask 파이프라인은 CXL 위에서도 유효한 상위 계층 최적화가 된다. 즉 이 논문은 CXL 논문은 아니지만 "SW로 어디까지 버티는가"의 상한을 보여주며, 그 상한을 넘는 지점이 CXL disaggregation의 진입점이라는 대조 축을 제공한다.

---

## Open questions / gaps

- [ ] 이 논문은 host↔GPU 메모리 분리를 **고정 전제**로 둔다. CXL로 memory pool을 확장하면 sampling·transfer 병목이 어떻게 재정의되는가? (preprocessing 84.2%가 얼마나 남는가)
- [ ] GNN 그래프+embedding table이 CXL memory pool에 byte-addressable하게 상주하면 graph reindexing·embedding lookup을 host 복사 없이 수행 가능한가?
- [ ] DKP cost model(Table I)은 GPU 로컬 메모리 기준. CXL의 non-uniform latency(local vs CXL-attached)를 cost model에 넣으면 kernel placement가 어떻게 바뀌나?
- [ ] 전체 그래프가 GPU에 들어간다고 가정하는 FlexGraph/NextDoor 계열과 sampling 강제 계열의 경계를, CXL 용량 확장이 어디까지 밀어내는가?
- [ ] service-wide tensor scheduler의 lock contention(hash table) 문제는 CXL multi-node coherence 환경에서 더 심각해지는가?

---

## References worth following up

| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [13] | Wang et al., "Gnnadvisor: An adaptive and efficient runtime system for GNN acceleration on GPUs" (OSDI '21) | DL-approach 대표. GraphTensor의 주 비교 대상, memory bloat 근원 이해 |
| ☐ | [18] | Hu et al., "Featgraph: A flexible and efficient backend for graph neural network systems" (SC '20) | Graph-approach 대표(SpMM/SDDMM). cache bloat·format translation 이해 |
| ☐ | [19] | Jia et al., "Improving accuracy, scalability, and performance of GNN with ROC" (MLSys '20) | 유일하게 CSR 쓰는 graph framework, multi-GPU load balancing |
| ☐ | [38] | Lin et al., "Pagraph: Scaling GNN training on large graphs via computation-aware caching" (SoCC '20) | embedding caching으로 transfer 단축 — CXL caching과 대조점 |
| ☐ | [36] | Kaler et al., "Accelerating training and inference of GNNs with fast sampling and pipelining" (SALIENT, MLSys '22) | preprocessing overlap SOTA, Prepro-GT 비교 baseline |
| ☐ | — | PreGNN (CAL'22, CAMEL) | 계보상 GraphTensor의 앞단. 같은 랩 GNN 라인 시작점 |
| ☐ | — | AutoGNN (CAMEL) | 계보상 GraphTensor의 뒷단. GNN 라인 자동화 방향 |

---

## Personal annotations

<자유 형식 메모 — user 전용 영역. 아직 비어 있음.>
