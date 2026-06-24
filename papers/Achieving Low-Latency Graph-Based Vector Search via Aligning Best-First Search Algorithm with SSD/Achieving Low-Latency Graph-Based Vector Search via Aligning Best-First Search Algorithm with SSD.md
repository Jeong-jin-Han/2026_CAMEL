---
title: "Achieving Low-Latency Graph-Based Vector Search via Aligning Best-First Search Algorithm with SSD"
aliases: [Graph-Based Vector Search OSDI25, PipeANN, PipeSearch]
description: "Best-first search를 SSD I/O 특성과 정렬해 compute-I/O를 겹치고 I/O 파이프라인 활용도를 높여 on-disk graph-based ANNS의 지연을 낮추는 시스템 PipeANN."
venue: OSDI
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/search
  - topic/vector-search
  - topic/ann
  - topic/graph
  - topic/ssd
  - venue/osdi
  - year/2025
---

# Achieving Low-Latency Graph-Based Vector Search via Aligning Best-First Search Algorithm with SSD

> **OSDI 2025** · cluster/search · Source: [Achieving Low-Latency Graph-Based Vector Search via Aligning Best-First Search Algorithm with SSD.pdf](<Achieving Low-Latency Graph-Based Vector Search via Aligning Best-First Search Algorithm with SSD.pdf>)

저자: Hao Guo, Youyou Lu (corresponding author) — Tsinghua University

---

## TL;DR

이 논문은 **PipeANN**을 제안한다. 이는 on-disk graph-based approximate nearest neighbor search (ANNS) 시스템으로, in-memory 인덱스와의 latency gap을 크게 줄인다. 핵심은 **best-first search 알고리즘을 SSD I/O 특성과 정렬(align)**하는 것이다. 기존 best-first search는 search step 간 *ordered compute and I/O*를 강제하고 각 step에서 *synchronous I/O*를 요구하기 때문에 compute와 I/O가 겹치지 못하고 I/O 파이프라인이 underutilized 된다. PipeANN은 search step 전반에 걸친 엄격한 compute-I/O 순서를 없애 두 문제를 해소한다.

실험상 PipeANN은 in-memory Vamana 대비 1.14x~2.02x search latency, billion-scale 데이터셋에서 on-disk DiskANN latency의 35.0% 수준을 정확도 손실 없이 달성한다.

> [!quote]- 📄 원문 표현 (paper)
> - "We propose PipeANN, an on-disk graph-based approximate nearest neighbor search (ANNS) system, which significantly bridges the latency gap with in-memory ones." (p.1)
> - "Experiments show that PipeANN has 1.14×–2.02× search latency compared to in-memory Vamana, and 35.0% of the latency of on-disk DiskANN in billion-scale datasets, without sacrificing search accuracy." (p.1)

---

## 문제 & 동기

Graph-based ANNS는 메모리에서는 낮은 latency를 보이지만 disk 위에서는 그 latency를 유지하지 못한다. 예를 들어 Figure 1에서 on-disk DiskANN은 in-memory Vamana보다 0.9 recall에서 4.18x, 0.99 recall에서 3.14x 높은 search latency를 보인다.

원인은 **graph-based ANNS 알고리즘과 SSD I/O 특성 간의 본질적 mismatch**다. SSD의 I/O 특성은 (1) **long I/O latency** (마이크로초 또는 수십 마이크로초)와 (2) **asynchronous, parallel I/O** (예: 32개의 in-flight 요청 병렬 처리)이다. 반면 best-first search는 그래프 내 vector를 best-first 순서로 탐색한다.

두 가지 issue가 disk에서 낮은 latency 달성을 막는다.

- **Issue 1: Ordered compute and I/O across search steps.** Best-first search는 자연히 data dependency를 만든다. 각 search step의 I/O batch는 이전 step의 compute와 이전 I/O batch에 의존한다. 메모리에서는 메모리 접근 latency가 compute보다 훨씬 낮아 문제가 없지만, disk에서는 I/O latency가 compute보다 높아 bottleneck이 I/O로 이동한다. W=1 (greedy search)일 때 compute latency는 I/O latency의 9.5%에 불과하고, W=8일 때도 45.6% 수준이라 긴 I/O latency가 compute와 겹치지 못해 낭비된다.
- **Issue 2: Synchronous I/O in each search step.** Best-first search는 각 step에서 W개 record를 batch read하지만, 모든 I/O가 끝날 때까지 synchronous하게 기다린다. 이로 인해 I/O 파이프라인이 underutilized 된다 — W=8에서 76%, W=32에서 58%만 채워진다 (SSD I/O latency가 fluctuate하고 best-first search는 tail latency를 기다리기 때문).

Real-world 시스템(추천, large-scale search 등)은 ~10ms 같은 strict latency budget을 가지므로 낮은 latency가 중요하다.

> [!quote]- 📄 원문 표현 (paper)
> - "By analysis, we find the high latency is caused by the intrinsic mismatch between graph-based ANNS algorithms and the I/O characteristics of SSDs, namely long I/O latency and asynchronous, parallel I/O" (p.1)
> - "First, the best-first algorithm incurs ordered compute and I/O across search steps." (p.1)
> - "Second, the best-first algorithm forces synchronous I/O in each search step, where it batch-reads the nearest neighbors synchronously." (p.1)
> - "When W = 1 (i.e., greedy search), compute latency is only 9.5% of I/O latency." (p.4)
> - "During I/O time, the pipeline is only 76% full for W = 8, and 58% full for W = 32." (p.4)

---

## 핵심 통찰

핵심 관찰은 **best-first search에서 compute와 I/O의 pseudo-dependency**다. 각 search step에서 다음에 read할 neighbor는 in-memory candidate pool (neighbor ID 포함)만으로 결정될 수 있으며, ongoing I/O나 compute(neighbor exploration)가 끝나기를 기다릴 필요가 없다.

또한 저자들은 **best-first 알고리즘이 필수가 아니며 search convergence를 해치지 않고 tweak할 수 있다**고 주장한다. Scalar index(B+-tree 등)는 각 object당 하나의 search path만 있지만, graph-based ANNS index는 multiple in-edge 덕분에 각 vector에 여러 search path가 존재한다. Best-first 알고리즘은 하나의 짧은 path를 추정할 뿐 unique path는 아니므로, tweaked search 알고리즘이 다른 convergence path를 활용할 수 있다.

추가 통찰: **I/O waste는 search step이 진행됨에 따라 감소한다.** Search 초반(approach phase)에는 unexplored top-k neighbor가 적어 wide pipeline이 I/O waste를 유발하지만, 후반(converge phase)에는 candidate pool에 unexplored top-k neighbor가 많아져 little I/O waste로 wider pipeline을 쓸 수 있다.

> [!quote]- 📄 원문 표현 (paper)
> - "The key observation for such alignment is the pseudo-dependency of compute and I/O in best-first search: In each search step, the neighbors to be read can be decided by only the in-memory candidate pool containing the neighbor IDs, without waiting for ongoing I/O or compute (i.e., neighbor exploration) to finish." (p.3)
> - "we argue that the best-first algorithm is not a must and can be tweaked without affecting search convergence." (p.4)
> - "each vector can be found in multiple paths using its multiple in-edges. The best-first search only estimates one short search path in the graph, but not the unique path." (p.4)

---

## 설계 / 메커니즘

### PipeSearch 알고리즘 (§3)

PipeANN의 기반 알고리즘인 **PipeSearch**는 search step 전반에 걸친 엄격한 compute-I/O 순서를 없앤다. Best-first search처럼 고정 길이 L의 candidate pool을 유지하되, 추가로 specific width W의 **I/O pipeline Q** (ongoing I/O 보관)를 유지한다. I/O 파이프라인이 full이 아니면 PipeSearch는 current candidate pool 기반으로 nearest unread neighbor에 대해 **asynchronously I/O를 발행**해 파이프라인을 채운다. 이 I/O와 겹쳐서 unexplored set U의 nearest vector를 explore하고 candidate pool을 갱신한다. 그 후 I/O completion을 poll한다.

이로써 두 가지 효과를 얻는다.

- **Compute-I/O overlapping**: graph-based ANNS on disk에서 compute와 I/O latency가 같은 자릿수(order of magnitude)라서 overlapping이 효율적이다. W=32에서 compute latency가 I/O latency의 75.6%/72.7%로, 겹치면 1.7x 성능 향상이 가능하다.
- **Better-utilized I/O pipeline**: navigation graph의 각 vector가 수백 개 neighbor를 가져 첫 step 후 candidate pool에 수백 개 unexplored vector가 생기므로 충분한 I/O로 파이프라인을 saturate할 수 있다 (또 다른 1.7x 향상 가능).

하지만 PipeSearch는 latency는 낮추지만 throughput을 희생한다 (I/O waste로 인해). 고정 pipeline width로는 low latency와 high throughput을 동시에 달성하지 못하는 dilemma가 있다.

### PipeANN 설계 (§4)

PipeANN은 PipeSearch를 (1) **dynamic pipeline width** (§4.2)와 (2) **algorithm optimization** (§4.3)으로 개선한다. Vector search를 **approach phase**와 **converge phase** 두 단계로 분리한다.

- **Approach phase: Entry point optimization** — I/O waste가 큰 초반에는 in-memory graph-based index (Vamana, max out-degree 32, DiskANN처럼 1% sample rate로 entry point sampling)로 entry point를 최적화하고 small pipeline width로 PipeSearch를 시작한다.
- **Converge phase: Pipeline width adjustment** — recall된 vector 수 n_v를 추정해 threshold(평가에서 5)에 도달하면 pipeline width를 동적으로 키운다. 그 전에는 pipeline width를 고정값 4로 둬 I/O waste를 줄인다. Dynamic approach가 default(finished I/O의 fetched-in-candidate-pool 비율이 threshold 0.9 초과 시 width를 1 증가).
- **Algorithm optimization** — I/O waste의 근본 원인은 fetched vector의 missing neighbor information이다. 여러 I/O가 동시에 끝날 때 한 번에 여러 I/O를 발행하면 W개 record 이상의 neighbor 정보를 놓치므로, **한 번에 하나의 I/O만 발행**(one record explore 후 하나 I/O)해 I/O를 timeline에 고르게 분산시킨다 (Figure 10). 이로써 모든 disk I/O가 최대 W개 record의 neighbor 정보만 놓치게 한다.

### Implementation (§4.4)

- **Overlapping initialization**: 첫 disk I/O 대기(NVMe SSD에서 ~50us)를 local PQ table initialization과 overlap. Cache pollution 방지를 위해 AVX512 non-temporal load 사용.
- **Asynchronous I/O**: `io_uring` 사용, thread마다 private io_uring로 `prep_read` 비동기 발행, non-blocking `peek_batch_cqe`로 completion poll.
- **Polling-based I/O**: 기존 시스템은 interrupt-based I/O를 쓰지만 PipeANN은 saved I/O time을 compute에 쓰므로 io_uring engine의 **SQ polling**을 활성화.
- **Memory usage**: billion-scale에서 <40GB (PQ-compressed vector용 32GB, 32 bytes/vector; in-memory graph index용 <4GB). DiskANN(~32GB) 대비 memory-to-disk size ratio ~1:15.

> [!quote]- 📄 원문 표현 (paper)
> - "PipeSearch avoids strict compute-I/O order across search steps. Specifically, it maintains a candidate pool with a fixed length L, similar to best-first search. Also, it maintains an I/O pipeline Q with a specific width W, containing ongoing I/O." (p.5)
> - "It separates a single vector search into two phases, approach phase and converge phase, based on the key observation that I/O waste decreases across steps (§4.2)." (p.6)
> - "Specifically, after multiple I/Os finishes, we repeatedly send one I/O, explore one nearest vector, and update the neighbor list." (p.8)
> - "PipeANN uses io_uring [11] to issue I/O requests, due to its performance and compatibility." (p.8)

---

## 평가

**Setup**: 2x 28-core Intel Xeon Gold 6330, 512GB RAM, Samsung PM9A3 3.84TB SSD, Ubuntu 22.04 (kernel 5.15.0). 비교 대상: DiskANN, Starling (둘 다 graph-based on-disk), SPANN (cluster-based on-disk). 데이터셋: SIFT1B, SPACEV1B, SIFT100M, DEEP100M, SPACEV100M. Metric: recall10@10, 주로 0.9 recall에서 latency/throughput 비교. Latency는 1 thread, throughput은 56 thread.

**Latency (100M, §5.2.1)**: 0.9 recall10@10 달성 시 PipeANN은 DiskANN/Starling 대비 평균 39.1%/48.5% latency. Cluster-based SPANN 대비 recall >= 0.9에서 70.6% latency (단, recall 0.8 같은 낮은 recall에서는 approach phase overhead로 SPANN보다 느림).

**Throughput (100M, §5.2.2)**: recall=0.9에서 PipeANN이 가장 높은 throughput, 다른 시스템 대비 평균 1.35x. 단, recall 0.99에서는 wasted disk I/O로 Starling 대비 0.80x (Starling은 reordering 기술로 disk I/O 감소). DiskANN 대비 유사 average I/O (0.98x for 0.99 recall)지만 pipelining으로 disk bandwidth를 더 잘 활용해 더 높은 throughput.

**Billion-scale (§5.3)**: 0.9 recall10@10에서 PipeANN latency는 SIFT1B 0.719ms, SPACEV1B 0.578ms로 SIFT100M/SPACEV100M 대비 1.28x/1.09x. DiskANN 대비 SIFT에서 35.0% latency. Throughput은 SIFT1B 19.4K QPS, SPACEV1B 26.1K QPS, DiskANN 대비 1.71x 더 높음.

**vs In-memory (§5.4)**: recall 0.8에서는 small L(10)로 approach phase가 dominant해 Vamana보다 3.38x 느리지만, recall >= 0.9에서는 in-memory에 근접 — 0.9 recall에서 Vamana 대비 2.02x/1.14x latency. DEEP100M에서는 distance comparison이 더 비싸 slowdown이 덜함.

**Breakdown (§5.5)**: Baseline(best-first, W=8) → +Pipe(PipeSearch, W=8): 0.9 recall latency 55.1%로 감소, throughput 88.5%로 감소(I/O waste). → +AlgOpt: throughput 1.08x(average I/O per search 91.8%로 감소). → PipeANN(dynamic pipeline, W=8): latency 81.1% for 0.99 recall, throughput 1.07x.

**Tradeoffs (§5.7)**: W>1로 인해 ideal best-first search(W=1)보다 throughput은 낮음. recall=0.8에서 throughput drop이 31.6%/34.1%/17.5% (approach phase 비중 큼), recall=0.95에서는 14.7%/6.15%/4.90%로 감소. Search accuracy: DiskANN(W=8) 대비 동일 L에서 PipeANN은 최소 95.9% recall, recall >= 0.9에서는 98.8%. PipeANN은 더 큰 L을 쓰지만 lower latency 이점.

> [!quote]- 📄 원문 표현 (paper)
> - "To achieve 0.9 recall10@10, PipeANN has 39.1%/48.5% latency on average, compared to DiskANN/Starling." (p.10)
> - "When recall = 0.9, PipeANN consistently shows the highest throughput. To achieve 0.9 recall10@10, PipeANN outperforms other systems by 1.35× on average." (p.10)
> - "PipeANN has latencies of 0.719 ms and 0.578 ms in recall10@10, to achieve 0.9 recall10@10, in SIFT1B and SPACEV1B" (p.11)
> - "Compared to DiskANN, PipeANN achieves 35.0% latency in SIFT." (p.11)
> - "PipeANN shows at least 95.9% recall compared to DiskANN. When recall ≥ 0.9, this value is further increased to 98.8%." (p.12)

---

## 섹션 노트

- **§1 Introduction**: 문제 정의 — graph-based ANNS가 disk에서 latency를 유지 못함 (DiskANN이 Vamana 대비 4.18x/3.14x). 두 가지 issue (ordered compute/I/O, synchronous I/O)와 alignment 아이디어 제시.
- **§2 Background and Motivation**: Graph-based ANNS와 best-first search 설명 (Algorithm 1, candidate pool length L, I/O pipeline width W, W=1 greedy / W>1 beam search). Best-first search와 SSD의 mismatch 정량 분석 (Figure 3: latency breakdown, I/O utilization).
- **§3 PipeSearch**: best-first tweaking 가능성 논증, PipeSearch 알고리즘(Figure 4 비교), latency 감소 분석, latency-throughput dilemma 제시.
- **§4 PipeANN Design**: 2-phase search (approach/converge), dynamic pipeline width(I/O waste가 step 따라 감소, Figure 8), entry point optimization, algorithm optimization, io_uring/SQ polling 구현, beyond-SSD discussion (RDMA, CXL).
- **§5 Evaluation**: 100M/billion-scale latency·throughput, in-memory 비교, breakdown, pipeline adjustment(static vs dynamic), tradeoffs.
- **§6 Related Work**: graph search optimization (FINGER, Proxima, learned early termination), on-disk ANNS (graph-based: DiskANN, Starling; cluster-based: SPANN, SmartSSD).

---

## 핵심 용어

- **ANNS (Approximate Nearest Neighbor Search)**: target vector에 대한 top-k nearest vector의 approximate set을 찾는 문제. Curse of dimensionality 때문에 정확 검색 대신 근사.
- **Best-first search**: candidate pool에서 target에 가장 가까운(top-W) unexplored vector를 우선 탐색하는 그래프 traversal 알고리즘. accessed vector 수를 줄임.
- **Greedy search / Beam search**: best-first search with W=1 (greedy) / W>1 (beam).
- **Candidate pool length (L)**: 현재 top-L nearest vector를 담는 고정 길이 pool.
- **I/O pipeline width (W)**: 동시에 발행 가능한 최대 병렬 I/O 요청 수 (best-first search의 beam width에 해당).
- **Pseudo-dependency of compute and I/O**: 다음 read할 neighbor가 in-memory candidate pool만으로 결정 가능해, ongoing I/O/compute 완료를 기다릴 필요 없는 성질. PipeSearch의 핵심 근거.
- **PipeSearch**: compute-I/O ordering을 없애 compute-I/O overlapping과 더 나은 I/O 파이프라인 활용을 달성하는 low-latency 알고리즘.
- **PipeANN**: PipeSearch에 dynamic pipeline width와 algorithm optimization을 더한 high-throughput 시스템.
- **Approach phase / Converge phase**: search 초반(target에 접근, I/O waste 큼) / 후반(target 근처 vector recall, I/O waste 작음).
- **I/O waste**: search당 average I/O 증가를 유발하는 비효율 (large pipeline width와 read-but-unexplored neighbor 누적에서 기인).
- **PQ-compressed vectors**: in-memory에서 neighbor distance 비교에 쓰는 Product Quantization 압축 vector (32 bytes/vector).
- **Vamana**: DiskANN의 in-memory graph index 알고리즘 (MRNG 근사). PipeANN의 entry point optimization에 사용.

---

## 강점 · 한계 · 열린 질문

**강점**
- Best-first search라는 ANNS의 핵심 알고리즘과 SSD I/O 특성 간 mismatch를 정량적으로 규명 (compute vs I/O latency 비율, pipeline utilization).
- 알고리즘 자체를 SSD에 맞춰 tweak하면서도 search convergence를 해치지 않음(multiple search path 근거). 정확도 손실 거의 없음(>= 95.9% recall vs DiskANN).
- 2-phase 분리로 I/O waste가 search step 따라 감소하는 특성을 활용해 latency-throughput dilemma 완화.
- io_uring + SQ polling 등 systems 수준 최적화로 실효 성능 확보. RDMA/CXL 등 us-scale storage로 확장 가능성 논의.

**한계 (저자 인정)**
- Speculative I/O로 인한 throughput 저하 — high recall(0.99)에서 Starling 대비 0.80x throughput, W>1로 인해 ideal best-first(W=1)보다 낮은 throughput.
- Low recall(예: 0.8)에서는 approach phase overhead로 SPANN보다 느리고 in-memory Vamana 대비 3.38x 느림.
- Latency-throughput tradeoff가 ms-scale latency budget 애플리케이션에서만 worthwhile.

**열린 질문**
- Starling의 reordering 기술을 PipeANN에 결합(orthogonal)하면 high-recall throughput을 얼마나 회복할 수 있는가? (저자는 billion-scale에서 reordering의 time/memory overhead 때문에 채택 안 함.)
- RaBitQ 같은 memory-efficient quantization으로 memory usage를 더 줄일 수 있는가?
- RDMA/CXL 실측에서도 SSD와 동일한 이득이 재현되는가?

---

## ❓ Q&A (자가 점검)

> [!question]- Q1. PipeANN이 해결하는 핵심 mismatch는 무엇인가?
> Graph-based ANNS의 best-first search 알고리즘과 SSD I/O 특성(long I/O latency, asynchronous/parallel I/O) 간의 mismatch다. Best-first search는 (1) search step 간 ordered compute and I/O를 강제하고 (2) 각 step에서 synchronous I/O를 요구해, compute-I/O가 겹치지 못하고 I/O 파이프라인이 underutilized 된다.

> [!question]- Q2. "pseudo-dependency of compute and I/O"란 무엇이며 왜 중요한가?
> 각 search step에서 다음에 read할 neighbor가 in-memory candidate pool(neighbor ID 보유)만으로 결정 가능하다는 성질이다. 즉 ongoing I/O나 neighbor exploration(compute) 완료를 기다릴 필요가 없다. 이 때문에 엄격한 compute-I/O 순서를 깨도 되고, asynchronous하게 I/O를 발행해 compute와 overlap하며 파이프라인을 채울 수 있다.

> [!question]- Q3. best-first 알고리즘을 tweak해도 search가 수렴하는 이유는?
> Scalar index(B+-tree)는 object당 search path가 하나지만, graph-based ANNS index는 multiple in-edge 덕에 각 vector를 여러 path로 도달할 수 있다. Best-first search는 하나의 짧은 path를 추정할 뿐 unique path가 아니므로, 다른 path를 활용해도 수렴이 막히지 않는다(다소 길어질 수 있음).

> [!question]- Q4. PipeSearch가 latency는 줄이지만 throughput을 희생하는 dilemma는 무엇이고, PipeANN은 어떻게 푸는가?
> 고정 pipeline width로는 low latency(wide pipeline)와 high throughput(narrow pipeline, less I/O waste)을 동시에 못 얻는다. PipeANN은 (1) dynamic pipeline width로 I/O waste가 큰 approach phase에서는 좁게(4), waste가 작은 converge phase에서는 동적으로 넓게 키우고, (2) algorithm optimization으로 한 번에 하나의 I/O만 발행해 missing neighbor information을 줄인다.

> [!question]- Q5. PipeANN의 2-phase search에서 각 phase의 역할은?
> Approach phase: target vector에 점진 접근하는 초반으로 I/O waste가 커서 PipeSearch가 비효율적 → in-memory graph index로 entry point를 최적화하고 small pipeline width로 시작. Converge phase: target 근처 vector를 recall하며 I/O waste가 작은 후반 → recall된 vector 수 n_v를 추정해 pipeline width를 동적으로 키워 compute-I/O overlapping 극대화.

> [!question]- Q6. 대표 성능 수치는?
> in-memory Vamana 대비 1.14x~2.02x search latency, billion-scale에서 on-disk DiskANN latency의 35.0%(SIFT). 0.9 recall10@10에서 DiskANN/Starling 대비 39.1%/48.5% latency, throughput은 평균 1.35x(100M)·DiskANN 대비 1.71x(billion-scale). 정확도는 동일 L에서 DiskANN 대비 최소 95.9% recall.

> [!question]- Q7. PipeANN이 채택한 systems 수준 I/O 최적화는?
> io_uring을 thread별 private으로 사용해 prep_read로 asynchronous I/O 발행, non-blocking peek_batch_cqe로 completion poll. interrupt-based 대신 io_uring의 SQ polling을 활성화해 saved I/O time을 compute에 사용. 첫 disk I/O 대기(~50us)를 local PQ table initialization과 overlap하며 AVX512 non-temporal load로 cache pollution 회피.

> [!question]- Q8. PipeANN의 한계는 무엇인가?
> Speculative I/O로 인한 throughput 저하 — high recall(0.99)에서 Starling 대비 0.80x throughput, W>1로 ideal best-first(W=1)보다 낮은 throughput. Low recall(0.8)에서는 approach phase overhead 때문에 SPANN/Vamana보다 느릴 수 있다. Latency-throughput tradeoff는 ms-scale latency budget 애플리케이션에서만 가치 있다.

---

## 🔗 Connections

[[Vector Search]] · [[OSDI]] · [[2025]] · [[ANN]] · [[Graph Index]] · [[SSD]] · [[DiskANN]]

---

## References worth following

- [23] S. J. Subramanya et al. **DiskANN**: fast accurate billion-point nearest neighbor search on a single node. NeurIPS '19. — 핵심 비교 대상이자 on-disk graph-based ANNS의 기준선.
- [25] M. Wang et al. **Starling**: An I/O-Efficient Disk-Resident Graph Index Framework. SIGMOD '24. — on-disk record reordering로 I/O 감소, PipeANN과 orthogonal.
- [4] Q. Chen et al. **SPANN**: highly-efficient billion-scale ANNS. NeurIPS '21. — cluster-based on-disk index 비교 대상.
- [31] Q. Zhang et al. **VBASE**: Unifying Online Vector Similarity Search via Relaxed Monotonicity. OSDI '23. — two-phase search/relaxed monotonicity, PipeANN이 인용한 유사 현상.
- [6] C. Fu et al. Fast ANN search with the **navigating spreading-out graph (NSG)**. VLDB '19. — MRNG 기반 graph index 근거.
- [16] Y. Malkov, D. Yashunin. **HNSW**. TPAMI '20. — 대표 in-memory graph-based ANNS.

---

## Personal annotations

<본인 메모 영역>
