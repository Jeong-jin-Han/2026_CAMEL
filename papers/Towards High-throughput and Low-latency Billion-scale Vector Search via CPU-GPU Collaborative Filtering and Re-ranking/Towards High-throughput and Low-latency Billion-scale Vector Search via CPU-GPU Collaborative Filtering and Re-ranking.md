---
title: "Towards High-throughput and Low-latency Billion-scale Vector Search via CPU/GPU Collaborative Filtering and Re-ranking"
aliases: [FusionANNS, Billion-scale Vector Search]
description: "SSD+엔트리급 GPU 단일 장비로 billion-scale ANNS를 다층 인덱스·CPU/GPU 협력 필터링·휴리스틱 재순위로 고처리량·저지연·고정확·저비용 동시 달성 (FusionANNS)"
venue: FAST
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/search
  - topic/vector-search
  - topic/ann
  - topic/cpu-gpu
  - venue/fast
  - year/2025
---

# Towards High-throughput and Low-latency Billion-scale Vector Search via CPU/GPU Collaborative Filtering and Re-ranking

> **FAST 2025** · `cluster/search` · Source: [Towards High-throughput and Low-latency Billion-scale Vector Search via CPU-GPU Collaborative Filtering and Re-ranking.pdf](<Towards High-throughput and Low-latency Billion-scale Vector Search via CPU-GPU Collaborative Filtering and Re-ranking.pdf>)

**저자**: Bing Tian, Haikun Liu(교신, hkliu@hust.edu.cn), Yuhang Tang, Shihai Xiao, Zhuohui Duan, Xiaofei Liao, Hai Jin, Xuecang Zhang, Junhua Zhu, Yu Zhang — Huazhong University of Science and Technology (HUST) & Huawei Technologies

## TL;DR
FusionANNS는 SSD + 엔트리급 GPU(예: V100) **단일 서버**로 billion-scale ANNS를 수행하면서 고처리량·저지연·고정확·저비용을 **동시에** 달성하는 시스템이다. 핵심은 (1) raw vector는 SSD, PQ 압축 vector는 GPU HBM, navigation graph와 vector-ID metadata만 host memory에 두는 **multi-tiered index**, (2) CPU가 후보 posting list의 vector-ID만 GPU로 전송해 PQ 거리 계산을 시키는 **CPU/GPU collaborative filtering**, (3) mini-batch 단위로 정확도 기여가 사라지면 조기 종료하는 **heuristic re-ranking**, (4) 유사 vector를 같은 SSD page에 배치해 read amplification을 줄이는 **redundancy-aware I/O deduplication**이다. SPANN 대비 QPS 9.4–13.1배·비용효율 5.7–8.8배, RUMMY 대비 QPS 2–4.9배·비용효율 2.3–6.8배 향상.

## 문제 & 동기
ANNS는 RAG·검색·추천의 핵심 인프라이나 데이터셋이 billion 규모로 커지면서 성능·비용·정확도를 동시에 만족하는 시스템이 없었다. 기존 접근은 모두 한계가 있다.
- **SPANN** (SSD 기반, IVF): 저지연이지만 동시 질의 처리량이 CPU 스레드 4개에서 포화해 확장성이 나쁨 (SSD I/O contention).
- **RUMMY** 등 GPU in-memory: 빠르지만 HBM 용량 한계로 GPU·host·SSD 간 막대한 data swapping 발생, billion-scale에서 성능 급락.
- HI + PQ + GPU를 단순 결합하면 오히려 SPANN보다 느림 — HBM이 PQ 압축 인덱스조차 다 못 담아 data swapping 발생, PQ 도입 시 re-ranking 때문에 I/O 횟수가 70% 증가하고 IOPS 병목으로 전환.

세 가지 challenge: (C1) IVF replication으로 인덱스가 raw 대비 8배 커져 HBM에 압축본도 안 들어감, (C2) 질의별 최소 re-ranking 수가 크게 달라(최대 7.2배 차이) 정적 설정이 비효율, (C3) raw vector(128~384B)가 NVMe 페이지(4KB)보다 작아 re-ranking 시 심각한 read amplification.

> [!quote]- 📄 원문 표현 (paper)
> "None of modern ANNS systems can address these issues simultaneously. In this paper, we present FusionANNS, a high-throughput, low-latency, cost-efficient, and high-accuracy ANNS system for billion-scale datasets using SSDs and only one entry-level GPU." (Abstract, p.1)
>
> "The key idea of FusionANNS lies in CPU/GPU collaborative filtering and re-ranking mechanisms, which significantly reduce I/O operations across CPUs, GPU, and SSDs to break through the I/O performance bottleneck." (Abstract, p.1)
>
> "Our experiments show that the performance of ANNS even declines by 10% when SPANN directly adopts GPUs for distance calculations... The root cause is that the limited capacity of HBM causes extensive data movement across GPU's HBM, host memory, and SSDs." (§2.3, p.4)

## 핵심 통찰

> [!note]- 통찰 1 — 데이터를 적합한 계층에 배치하면 PCIe로는 가벼운 vector-ID만 흐른다
> billion-scale 인덱스 전체를 GPU HBM에 넣을 수 없다는 것이 모든 GPU ANNS의 근본 병목이다. FusionANNS는 raw vector→SSD, PQ 압축 vector→HBM, navigation graph와 posting list의 vector-ID(내용 제외)만→host memory로 분리한다. 그 결과 CPU가 GPU로 보내는 것은 후보 vector의 **ID 리스트뿐**이라 PCIe 대역폭 병목과 host↔HBM data swapping이 사라진다.

> [!note]- 통찰 2 — 질의마다 필요한 re-ranking 수가 크게 다르다 → 동적 조기 종료
> Figure 5 측정에서 re-ranking 수를 40으로 고정하면 42%의 질의만 Recall@10=1.0을 얻고 평균 Recall@10=0.9에 그친다. 질의별 최소 re-ranking 수는 최대 7.2배 차이 난다. 따라서 보수적으로 큰 값을 잡되 mini-batch 단위로 정확도 개선이 멈추면 즉시 종료하는 것이 I/O·계산을 최소화한다.

> [!note]- 통찰 3 — re-ranking 대상 vector들은 서로 매우 유사하다 → 같은 page에 배치
> re-ranking이 필요한 raw vector들은 query vector에 공간적으로 가깝고 서로도 유사하다. 이 유사성을 이용해 같은 centroid에 가까운 vector들을 같은 SSD page(bucket)에 모아두면, 한 page read로 여러 후보를 동시에 가져와 read amplification을 완화할 수 있다.

> [!note]- 통찰 4 — 필터링은 압축본으로 GPU에서, 정밀 재순위는 raw로 CPU에서
> PQ 거리 계산(대량·병렬)은 GPU의 massive kernel·HBM 대역폭에 적합하고, raw vector 기반 정밀 재순위(I/O 위주)는 CPU가 SSD에서 읽어 처리하는 것이 적합하다. 역할을 장치 특성에 맞춰 분리하면 두 라운드 필터링으로 대부분의 무관 vector를 걸러 SSD I/O를 최소화한다.

## 설계 / 메커니즘

> [!abstract]- 1. Multi-tiered Indexing (§3.1, p.5–6)
> - 오프라인에서 hierarchical balanced clustering(k-means 계열)으로 데이터셋을 posting list로 분할. SPANN을 따라 posting list 수는 전체 vector의 약 10%, boundary vector는 Eq.2 (`v∈Cᵢ ⟺ Dist(v,Cᵢ) ≤ (1+ε)·Dist(v,C₁)`)로 최대 8개 cluster에 복제.
> - **In-memory index**: 모든 posting list의 centroid로 SPTAG 기반 navigation graph를 구축(각 vertex는 top-k≈64 이웃과 연결), host memory에 저장. posting list의 **vector-ID만** metadata로 추출해 보관(vector 내용은 버림).
> - **PQ vectors in HBM**: PQ로 압축한 전체 vector를 HBM에 pin → host↔HBM swapping 제거. V100(32GB HBM)도 billion-scale 압축본 수용 가능.
> - **Raw vectors on SSD**: raw vector만 SSD에 저장. 압축 인덱스가 raw보다 약 8배 작으므로 storage 소비도 절감.

> [!abstract]- 2. CPU/GPU Collaborative Filtering (§3.2, p.6 / Fig.7)
> 질의 워크플로우(번호는 Fig.7 단계):
> 1. GPU가 query vector의 PQ distance table 생성.
> 2. CPU가 in-memory navigation graph를 순회해 top-m 최근접 posting list 식별.
> 3. CPU가 metadata에서 해당 posting list들의 vector-ID 수집.
> 4. CPU가 vector-ID(내용 제외)를 GPU로 전송 + GPU kernel 호출.
> 5. GPU가 parallel hash module로 vector-ID **중복 제거**(replication 때문).
> 6. GPU가 각 vector-ID의 PQ vector를 HBM에서 읽어 query와 PQ distance 계산(차원마다 thread, coordinator thread가 누적).
> 7. GPU가 거리 오름차순 정렬, **top-n 후보 ID**를 CPU로 반환.
> 8. CPU가 top-n vector-ID로 raw vector를 SSD에서 읽어(re-ranking) 최종 top-k 반환.

> [!abstract]- 3. Heuristic Re-ranking (§3.3, p.7 / Algorithm 1)
> - top-n 후보를 BatchSize 단위 mini-batch로 나눠 **순차** 처리. 후보가 거리 오름차순이라 앞 batch가 최종 top-k에 더 많이 기여.
> - max-heap(priority queue) Q로 현재 top-k 유지. mini-batch n과 n-1의 top-k ID 집합 변화율 `Δ = |Sₙ − Sₙ∩Sₙ₋₁| / k` 계산.
> - `Δ ≤ ε`이면 StabilityCounter++, `StabilityCounter ≥ β`면 re-ranking 조기 종료; 아니면 counter 리셋.
> - 실험 최적값: **ε=0.1, β=1, BatchSize=k**. 보수적으로 큰 re-ranking 수를 잡되 기여 없으면 즉시 종료.

> [!abstract]- 4. Redundancy-aware I/O Deduplication (§3.4, p.8 / Fig.8)
> - **Optimized storage layout**: 오프라인 인덱스 구축 시 각 centroid에 가장 가까운 raw vector들을 bucket으로 모음(bucket 간 중복 없음). page에 안 맞으면 max-min 알고리즘으로 bucket을 결합해 free space 최소화. 모든 bucket을 하나의 파일로 묶어 SSD에 저장, mapping table을 메모리에 유지. Direct I/O로 NVMe 저지연 활용.
> - **Intra-batch dedup**: 같은 SSD page에 매핑되는 여러 I/O 요청을 1회로 병합(예: V2·V6가 같은 page P0 → 한 번 read).
> - **Inter-batch dedup**: 이전 mini-batch가 읽어 DRAM buffer에 남은 page는 다시 읽지 않음(예: P2에 V5 포함 → batch1에서 재사용).

**Implementation**: C++/CUDA 약 22K LoC. contention-free GPU memory manager(질의별 독립 working block), 차원별 multi-thread GPU kernel, spinlock 기반 hash dedup kernel.

## 평가

> [!example]- 실험 환경 & 데이터셋 (§5, p.9)
> - 서버: Intel Xeon 2개(2.2GHz, 64코어), 1TB DRAM, **NVIDIA V100 32GB HBM(엔트리급)**, Samsung 990Pro 2TB SSD. SSD 기반 시스템은 in-memory index에 **64GB만** 사용.
> - 데이터셋(각 1 billion): SIFT1B(128dim, uint8, 119GB), SPACEV1B(100dim, int8, 93GB), DEEP1B(96dim, float32, 358GB).
> - 비교군: SPANN(SSD+IVF), DiskANN(SSD+graph), RUMMY(GPU in-memory+IVF, 고정확 IVF로 확장), SPANN-GPU/DiskANN-GPU.

> [!example]- 주요 성능 결과 (§5.1, p.9–10 / Fig.9, Table.2–3)
> - **vs SPANN**: QPS **9.4–13.1배** 향상, 저지연은 유지. (p.9)
> - **vs DiskANN**: QPS **3.2–4.3배** 향상. (p.9)
> - **vs RUMMY**: QPS **2–4.9배** 향상, 저지연 유지. 특히 DEEP1B(대용량)에서 RUMMY는 PCIe 대역폭 contention으로 DiskANN보다도 느림. (p.10)
> - SPANN-GPU/DiskANN-GPU는 data movement 비용으로 오히려 native보다 느림. (p.10)
> - **비용효율(QPS/$, Table.2)**: SPANN 대비 5.67–8.78배, DiskANN 대비 2–2.5배, RUMMY 대비 2.25–6.82배. DEEP1B에서 FusionANNS 1.01 vs SPANN 0.12, RUMMY 0.15. (서버≈$5000, DRAM≈$10/GB, SSD≈$400/2TB, V100≈$3000)
> - **메모리효율(QPS/GB, Table.3)**: DEEP1B에서 SPANN 대비 13.1배, DiskANN 대비 3.8배, RUMMY 대비 32.4배.

> [!example]- 확장성 & ablation (§5.2–5.4, p.10–11 / Fig.11–12)
> - **Scalability**: 스레드 1→64에서 FusionANNS QPS가 SPANN 대비 최대 **13.2배**, DiskANN 5.1배 향상하며 저지연 유지. SPANN은 4스레드, RUMMY는 16스레드에서 포화. (p.10)
> - **Ablation(SIFT1B)**: MI(CPU)만으로 SPANN 대비 QPS 1.5–4.2배(단 고지연), MI(GPU)는 SPANN 대비 5.9–6.8배·MI(CPU) 대비 저지연. heuristic re-ranking + I/O dedup 추가로 지연 추가 39%↓, QPS 17%↑. (p.11)
> - **I/O 감소(Fig.12c)**: multi-tiered indexing이 SPANN 대비 I/O 횟수 3.2–3.8배↓, heuristic re-ranking이 추가 최대 30%↓, redundancy-aware dedup이 추가 23%↓. FusionANNS는 I/O **횟수와 크기 모두** 줄임(SPANN은 질의당 여러 page, 타 시스템은 1 page). (p.11)
> - **정확도 변화**: Recall@10 0.90→0.98에서도 SPANN 대비 QPS 9.4–11.7배 유지(고정확일수록 격차↑). (p.9–10)

## 섹션 노트
- **§1 Introduction (p.1–3)**: RAG 프레임워크(Fig.1)에서 ANNS가 memory-hungry·compute-intensive. memory 절감 두 축 = Hierarchical Indexing(HI), Product Quantization(PQ). 세 challenge 제시.
- **§2 Background (p.3–5)**: §2.1 graph(DiskANN)·IVF(SPANN) 인덱스 비교, SPANN은 4스레드에서 포화(Fig.3). §2.2 PQ 원리(M개 sub-space, codebook 256 centroid, Eq.1 distance lookup). §2.3 HI+PQ+GPU 단순 조합 실패 분석, root cause = HBM 용량·data movement·IOPS 병목(Fig.4).
- **§3 Design (p.5–8)**: 4개 핵심 메커니즘(위 토글).
- **§4 Implementation (p.8–9)**: 22K LoC, contention-free GPU memory manager, efficient GPU kernel.
- **§5 Evaluation (p.9–11)**: 성능·확장성·기법별 효과·비용/메모리 효율.
- **§6 Related Work (p.12)**: in-memory(HNSW, SPTAG), SSD 기반(DiskANN, SPANN, Starling, BBANN, GRIP, SmartANNS), accelerator 기반(CAGRA, JUNO, SONG, RUMMY).
- **§7 Conclusion (p.12)**: "CPU + GPU" 협력 아키텍처로 4목표 동시 달성.

## 핵심 용어
- **ANNS (Approximate Nearest Neighbor Search)**: 고차원 query vector에 대해 가장 유사한 top-k vector를 근사 탐색하는 작업. RAG·검색·추천의 핵심.
- **IVF (Inverted File) index**: 데이터셋을 clustering해 posting list로 나누고 centroid로 표현하는 인덱스. billion-scale에 효율적.
- **Posting list**: 한 cluster에 속한 vector-ID와 내용의 묶음.
- **Navigation graph**: posting list centroid들로 만든 proximity graph(SPTAG). query에 대해 top-m posting list를 빠르게 찾는 데 사용.
- **PQ (Product Quantization)**: vector를 M개 sub-space로 나눠 각 sub-vector를 codebook centroid ID(1바이트)로 압축하는 손실 압축. distance lookup table로 근사 거리 계산.
- **Multi-tiered index**: raw vector(SSD)·PQ vector(HBM)·navigation graph+vector-ID(host memory)를 계층별 장치에 분산 배치하는 FusionANNS의 인덱스 구조.
- **Collaborative filtering**: CPU가 후보 vector-ID만 GPU로 보내고 GPU가 PQ 거리로 후보를 거르는 2라운드 협력 필터링.
- **Heuristic re-ranking**: re-ranking을 mini-batch로 나눠 top-k 변화율(Δ)이 ε 이하로 β회 지속되면 조기 종료하는 동적 재순위.
- **Re-ranking number (top-n)**: GPU가 PQ로 거른 후 raw vector로 정밀 재계산할 후보 수(보통 top-k보다 큼).
- **Read amplification**: raw vector(128~384B)가 SSD page(4KB)보다 작아 작은 데이터를 위해 page 전체를 읽는 비효율.
- **Redundancy-aware I/O deduplication**: 유사 vector를 같은 page에 배치 + intra/inter-batch에서 중복 page read 제거.
- **HBM (High Bandwidth Memory)**: GPU의 고대역폭 메모리(V100=32GB). 용량 한계가 GPU ANNS의 병목.

## 강점 · 한계 · 열린 질문
**강점**
- 단일 장비 + 엔트리급 GPU로 billion-scale 4목표(처리량·지연·정확도·비용) 동시 달성, 명확한 SOTA 대비 수치.
- 장치 특성에 맞춘 역할 분리(GPU=PQ 필터링, CPU=raw 재순위)와 "vector-ID만 전송"으로 PCIe·swapping 병목을 근본 제거.
- ablation으로 세 기법 각각의 기여(I/O 횟수·크기·지연)를 분리 입증.

**한계**
- in-memory index 64GB 제약 하 실험 — navigation graph + vector-ID metadata가 더 큰 데이터셋(수십 billion)에서 host memory를 초과할 가능성.
- PQ 압축본을 V100 32GB HBM에 전량 pin하는 가정 — 차원·정밀도가 더 큰 데이터셋에서 성립 여부 불확실.
- heuristic re-ranking 파라미터(ε, β, BatchSize)가 실험적으로 고정 — 데이터셋/분포 변화 시 재튜닝 필요.
- index 구축(clustering·graph·storage layout)이 오프라인이라 동적 삽입/갱신 비용은 다루지 않음.

**열린 질문**
- 수십~수백 billion 규모에서 host memory의 metadata가 병목이 될 때 어떻게 계층화할 것인가?
- 멀티 GPU나 multi-SSD로 확장 시 collaborative filtering이 어떻게 일반화되나?
- ε/β를 질의별·온라인으로 적응시키면 추가 이득이 있나?

## ❓ Q&A (자가 점검)

> [!question]- Q1. FusionANNS가 동시에 달성하려는 4가지 목표는?
> 고처리량(high-throughput), 저지연(low-latency), 비용효율(cost-efficient), 고정확(high-accuracy). 기존 ANNS는 이 중 일부만 만족했다.

> [!question]- Q2. multi-tiered index는 데이터를 어느 계층에 배치하나?
> raw vector → SSD, PQ 압축 vector → GPU HBM(pin), navigation graph + posting list의 vector-ID(내용 제외) → host memory. 이로써 CPU가 GPU로 vector-ID만 보내 PCIe 병목과 host↔HBM swapping을 제거한다.

> [!question]- Q3. CPU와 GPU는 질의 처리에서 각각 무슨 역할을 하나?
> CPU는 navigation graph 순회로 top-m posting list를 찾고, vector-ID를 수집·전송하며, top-n 후보의 raw vector를 SSD에서 읽어 최종 재순위한다. GPU는 PQ distance table 생성, vector-ID 중복 제거, PQ 거리 계산, 정렬 후 top-n ID 반환을 담당한다.

> [!question]- Q4. heuristic re-ranking은 언제 조기 종료하나?
> top-n 후보를 mini-batch로 순차 처리하며, 인접 batch 간 top-k 집합 변화율 Δ = |Sₙ−Sₙ∩Sₙ₋₁|/k 가 ε(=0.1) 이하인 상태가 β(=1)회 연속되면 종료한다. 정확도 기여가 멈추면 불필요한 I/O·계산을 중단하는 것이다.

> [!question]- Q5. raw vector가 page보다 작아 생기는 read amplification을 어떻게 줄이나?
> redundancy-aware I/O deduplication. 오프라인에서 같은 centroid에 가까운 유사 vector를 같은 SSD page(bucket)에 배치하고, intra-batch에서 같은 page 요청을 1회로 병합, inter-batch에서 DRAM buffer에 남은 page는 재읽기 생략한다.

> [!question]- Q6. HI+PQ+GPU 단순 결합이 SPANN보다 느린 근본 원인은?
> GPU HBM이 PQ 압축 인덱스조차 다 못 담아 GPU·host·SSD 간 data swapping이 발생하고, PQ 도입으로 re-ranking이 추가돼 I/O 횟수가 70% 늘며 병목이 SSD 대역폭에서 IOPS로 이동한다. 또 대량 posting list가 CPU↔GPU로 전송돼 GPU 가속 이득을 상쇄한다.

> [!question]- Q7. billion-scale 데이터셋(SIFT1B/SPACEV1B/DEEP1B)에서 SPANN·RUMMY 대비 QPS 향상은?
> SPANN 대비 9.4–13.1배, RUMMY 대비 2–4.9배(DiskANN 대비 3.2–4.3배). 특히 대용량 DEEP1B에서 RUMMY는 PCIe contention으로 DiskANN보다도 느려진다.

> [!question]- Q8. RUMMY 같은 GPU in-memory가 대용량에서 약한 이유는?
> 데이터셋이 클수록 main memory→HBM data transfer가 PCIe 대역폭을 소진해 contention이 심해진다. FusionANNS는 vector 내용 대신 가벼운 vector-ID만 전송하므로 이 병목을 피한다.

## 🔗 Connections
[[Vector Search]] · [[FAST]] · [[2025]]

## References worth following
- **SPANN** (Chen et al., NeurIPS 2021, [15]): SSD 기반 IVF ANNS, FusionANNS의 주요 비교·기반(replication 메커니즘 차용).
- **DiskANN** (Subramanya et al., NeurIPS 2019, [22]): SSD 기반 graph ANNS, 고처리량·고지연 비교군.
- **RUMMY** (Zhang et al., NSDI 2024, [9]): reordered pipeline으로 GPU 메모리 확장하는 GPU in-memory ANNS, 핵심 비교군.
- **PQ** (Jégou et al., TPAMI 2011, [32]): product quantization 원조, 압축·HBM 인덱싱 기반.
- **SPTAG** (Chen et al., [36]): navigation graph 구축에 사용한 space partition tree+graph 라이브러리.
- **SmartANNS** (Tian et al., ATC 2024, [19]): 동일 1저자의 SmartSSD 기반 ANNS, 후속·연계 연구.

## Personal annotations
<본인 메모 영역>
