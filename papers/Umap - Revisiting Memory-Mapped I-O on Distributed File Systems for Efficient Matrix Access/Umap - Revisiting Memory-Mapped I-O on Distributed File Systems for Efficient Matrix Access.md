---
title: "Umap: Revisiting Memory-mapped I/O on Distributed File Systems for Efficient Matrix Access"
description: "분산 파일시스템(DFS) 위 mmap-IO의 livelock·저성능·과다 메모리 문제를 network-friendly I/O 병합과 lock-free concurrency-aware 캐시 프로토콜로 해결하는 드롭인 런타임"
venue: OSDI
year: 2026
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/fs
  - venue/osdi
  - year/2026
  - list/26s-v2
  - topic/mmap-io
  - topic/page-cache
  - topic/disaggregated-storage
  - topic/lock-free
---

# Umap: Revisiting Memory-mapped I/O on Distributed File Systems for Efficient Matrix Access

> **OSDI 2026** · cluster/fs · Source: [Umap - Revisiting Memory-Mapped I-O on Distributed File Systems for Efficient Matrix Access.pdf](<Umap - Revisiting Memory-Mapped I-O on Distributed File Systems for Efficient Matrix Access.pdf>)

저자: Yongchao He (Unaffiliated; work done while at ScitiX AI) · Guangyan Zhang (Tsinghua University, corresponding) · Zane Cao (ScitiX AI) · Wenfei Wu (Peking University, corresponding)

## TL;DR
행렬(matrix) 데이터를 다루는 ML/금융/과학computing 워크로드는 out-of-core 접근을 위해 file-backed matrix(FBM)를 `mmap`으로 로드하는데, 저자들의 18개월 프로덕션 경험상 이 mmap-IO가 disaggregated storage(DFS) 위에서는 write-heavy livelock, 낮은 멀티스레드 처리량, 과도한 메모리 점유(OOM)로 심각하게 망가진다. 원인은 mmap-IO가 local page-granularity backing store를 가정하는 반면 DFS는 block-oriented·분산 metadata/lock/coherence를 노출하는 근본적 abstraction mismatch에 있다. umap은 이 문제를 (1) 페이지 폴트를 network-friendly한 coarse-grained I/O로 병합하는 Communication Manager(CoM), (2) lock-free하게 선형 확장하는 concurrency-aware 캐시 프로토콜(CaM), (3) 런타임에 최적 용량을 계산하는 lazy-expansion 캐시 관리로 대체한 drop-in mmap 대체 런타임이다. 18개월 프로덕션 배포에서 livelock과 OOM으로 인한 job 실패를 제거했고, 다양한 workload에서 처리량을 최대 6.7배 향상시켰다.

## 문제 & 동기
행렬 접근은 ML 학습, LLM 추론, 금융 backtesting, 과학 컴퓨팅 등 현대 데이터센터 워크로드의 핵심이며, GPU로 전송되는 데이터 중 행렬 접근이 종종 FLOP 자체보다 비용을 지배한다(p.1461). 실무에서는 물리 메모리를 초과하는 행렬을 다루기 위해 file-backed matrix(FBM)를 `mmap`으로 매핑하는 패턴(NumPy `memmap`, PyTorch 디스크 기반 데이터 로더, vLLM의 모델 가중치 mmap 로딩 등)이 널리 쓰인다(p.1461).

문제는 현대 클러스터가 compute와 storage를 분리하는 disaggregated DFS(GPFS, NFSv4 등)로 전환하면서 나타난다. mmap-IO는 원래 local, low-latency storage와 page-granularity VM을 전제로 설계되었지만, DFS는 block-oriented semantics·분산 metadata·locking·cache coherence를 노출한다. 저자들의 프로덕션 클러스터(대규모 금융 backtesting, 과학 컴퓨팅, AI 워크로드 지원)에서 LFS를 DFS로 교체한 후 FBM 워크로드가 극심하게 느려지는 것을 관찰했다: DFS가 로컬 SSD보다 25GB/s 이상의 per-node 대역폭을 제공함에도 FBM 워크로드는 3배~10배 낮은 처리량, 낮은 대역폭 활용률, 극단적인 tail latency를 보인다(p.1462).

> [!quote]- 📄 원문 표현 (paper)
> - "Our measurements further show that mmap-IO on DFS is 3×–10× slower than on local file systems (LFS) for matrix random-access workloads, primarily because page-granularity network I/O underutilizes high-speed networks and deferred write-back behavior incurs expensive distributed flushes and metadata operations." (p.1461, Abstract)
> - "In our production cluster, such misbehavior caused tasks to remain idle for tens of minutes, cascade into livelock, and even trigger system-level miskill events—affecting SLAs and service availability." (p.1462)
> - "Deployed for 18 months in production, umap has eliminated livelocks and out-of-memory-induced job failures while improving throughput by up to 6.7× across diverse matrix-access workloads." (p.1461, Abstract)

두 번째 동기는 스케일링 문제다: 스레드 수를 늘려도 mmap-IO 처리량은 8~16 스레드를 넘으면 오히려 악화된다(Fig.3, p.1463). 32 스레드에서 태스크는 시간의 88.9%를 iowait에서 소비하며, 이 중 상당 부분은 `native_queue_spin_lock_slowpath` 커널 spin lock 경합이다(p.1463). 세 번째 동기는 mmap-IO의 greedy-expansion 캐싱 정책이 multi-tenant 클러스터에서 메모리 압박·noisy-neighbor·OOM kill을 유발한다는 것이다(p.1463, §2.3 Challenge 3).

## 핵심 통찰 (Key Insight)

1. **페이지 단위 암묵적 I/O를 명시적·coarse-grained network I/O로 재구성한다.** mmap-IO는 4KB 페이지 폴트마다 fragmented remote I/O·metadata lookup·RPC를 유발해 대역폭을 심각히 과소활용한다. umap의 Communication Manager(CoM)는 4KB 페이지 크기 자체는 유지하되 인접·병합 가능한 요청을 rank 기반 PIAO 큐로 병합하고 다중 I/O 채널에 공정하게 스케줄링함으로써 network 활용도를 회복한다(p.1464, Solution 1).
   > [!quote]- 📄 원문 표현 (paper)
   > - "umap preserves the 4 KB page size but introduces network-friendly communication management that merges page requests using a rank-based, time-aware policy and fairly schedules them across I/O channels." (p.1464)

2. **락 대신 스레드 동시성 인지(state-machine 기반)로 캐시 교체를 lock-free화한다.** mmap-IO는 `tree_lock` 같은 spin lock으로 원자적 페이지 교체를 강제하는데, 이는 빠른 DFS 네트워크(200Gbps) 대비 상대적으로 훨씬 비싼 750~2500 clock cycles를 소모해 병목이 된다(p.1463-1464). CaM은 스레드별 thread-local `tid`와 파일별 non-overlapping reference map(`rmap`)으로 런타임에 동시성을 감지해, 같은 CB(Cache Block)를 참조 중인 스레드가 있으면 절대 교체 대상에서 제외하는 방식으로 lock 없이 WAR(write-after-read) hazard를 방지한다(p.1466-1467).
   > [!quote]- 📄 원문 표현 (paper)
   > - "In the concurrency-aware method, CaM attaches a unique identifier (tid) to each running thread to identify the current concurrency degree... A CB with a non-empty rmap will never be evicted, thus avoiding false eviction." (p.1466)

3. **LRU 캐시 최적 용량에 대한 이론적 상한(Theorem 1)을 이용한 lazy-expansion.** 무제한 greedy caching은 multi-tenant 환경에서 메모리 과점유·OOM을 유발한다. umap은 접근 시퀀스의 "재등장까지 거리"($R_i$)를 관찰해 $N_c = \max(R_i)$가 캐시 미스를 없애는 최소 LRU 용량임을 증명하고, 이를 런타임에 추정해 필요할 때만 캐시를 확장하고 메모리 압박 시(`kswapd` 신호) virtual capacity를 절반으로 축소하는 dynamic reclamation을 수행한다(p.1468).
   > [!quote]- 📄 원문 표현 (paper)
   > - "Theorem 1 Nc = max(Ri), i ∈ N, is the optimal number of CBs for minimizing the cache capacity in a Least Recently Used (LRU) cache." (p.1468)

## 설계 / 메커니즘 (Design)
umap은 두 핵심 컴포넌트로 구성된다: **Cache Manager(CaM)**와 **Communication Manager(CoM)**(Figure 5, p.1464). CaM은 전역 고정 크기 Cache Block(CB) 풀인 Cache Set과, 매핑된 파일마다 index array를 갖는 Cache Entry Table(CET)을 관리한다. CaM 내부의 `tracker`는 캐시 할당/교체 정책을 결정하고 `maintainer`는 그 결정을 실행하는 백그라운드 스레드다. 파일을 `umap()`으로 매핑하면 CET에 새 index array가 추가되고, 각 엔트리는 (4바이트 포인터 × 4KB 블록 기준) 파일 크기의 약 1/1024에 해당하는 메모리 오버헤드를 갖는다(p.1465).

**4.1 Network-friendly I/O Management.** CoM은 Push-In Admission-Out(PIAO) 큐를 사용해 각 요청에 (timestamp, rank) 튜플을 부여하고, rank가 인접한 요청끼리 자동 병합한다. 병합에 의한 starvation을 막기 위해 별도 FIFO 큐를 병행 유지하며, dequeue는 항상 FIFO 순서를 따르되 병합된 요청의 대응 엔트리를 PIAO에서 함께 제거한다(Figure 6, p.1465). 다중 I/O 채널 활용을 위해 CoM은 min-heap 기반 **least-first** 스케줄링(각 큐의 누적 전송량 $T_q$, 힙 루트의 $T_r$ 추적)으로 큐 간 공정성을 보장한다(p.1465-1466). Write-back 시에는 **shadow-copy 메커니즘**(Figure 7, p.1466)을 사용한다: 각 CB는 `data_ptr`/`shadow_ptr` 두 개의 동일 크기 버퍼를 가지며, write-back 시작 시 두 포인터를 원자적으로 swap해 애플리케이션 접근과 백그라운드 동기화를 계속 오버랩시키면서 WAR hazard를 방지한다.

**4.2 Concurrency-Aware Cache Protocol.** CB는 Active(CET에 참조되는 CB, cache hit 시 바로 반환) / Semiactive(참조는 없지만 index array가 여전히 가리킴, 재활성화 가능) / Inactive(참조도 index도 없음, 교체 후보) 3개 상태를 갖는 FSA(Finite-State Automaton)로 관리된다(Figure 8, p.1467). 캐시 미스 시 CaM은 inactive 목록에서 CB를 골라 active로 전환하며, 이 전이 외에는 동기화가 필요 없어 lock-free하게 선형 스케일링이 가능하다. 특별히 **Sentry CB**라는 전역 unique 더미 CB를 두어 CET 엔트리의 기본 포인터로 사용함으로써, cache hit 경로에서 NULL 체크 단계를 하나 줄인다(p.1470, §5).

**4.3 Lazy-Expansion Cache Management.** Algorithm 1은 `counter`, `lru_list`, `lru_map`, `last_appearance` 상태를 유지하며 각 접근 시 hit/miss/expansion-miss 세 경우로 나누어 처리한다: cache hit이면 LRU 갱신만, 처음 보는 원소면 일반 LRU 교체, 이전에 본 적 있는 원소가 다시 miss라면($R_i$가 현재 $N_c$보다 큼) LRU 용량을 $N_c' = I+1-i$로 확장한다(p.1468, Algorithm 1). 이렇게 얻은 $N_c$는 Theorem 1에 의해 그 접근 시퀀스에 대해 이론적으로 최적이며, `kswapd`가 신호하는 메모리 압박 시 tracker가 virtual capacity $N_v = N_c/2$로 줄여 실제 CB를 회수한다(Dynamic Cache Reclamation, p.1468).

**구현(§5, p.1469-1470).** umap은 두 통합 모드를 지원한다: (1) source-compatible 모드 — 얇은 wrapper/매크로로 `mmap` 호출을 `umap()`으로 치환, (2) binary-compatible 모드 — `LD_PRELOAD` 기반 interposition으로 libc `mmap`을 가로채 소스 수정 없이 기존 워크로드를 가속. Consistency model은 mmap 스타일의 weak consistency로, 노드 간 강한 coherence를 의도적으로 강제하지 않으며 cross-writer 충돌이 드문 data-parallel 워크로드를 타깃으로 한다(p.1465, p.1470).

## 평가 (Evaluation)
**테스트베드(§6.1, p.1469):** 노드당 200Gbps ConnectX-6 NIC 2개(스토리지용/컴퓨트용), 1.82TB DRAM, dual Intel Xeon 8260 (128 논리 코어), 3.84TB Optane NVMe SSD, NVIDIA A100 GPU 8개. DFS 백엔드로 GPFS와 NFSv4를 사용. Baseline은 SSD-backed LFS 위 mmap-IO(사실상 de facto)와 FastMap(학술 SOTA); mmap-IO/FastMap을 DFS에서 직접 돌리는 구성은 성능이 수 자릿수 낮아 실용적 비교에서 제외(p.1469).

**실사용 워크로드(§6.2):**
- AI 학습(AlexNet/ResNet/VGG, PyTorch dataset loader): 계산이 end-to-end 시간을 지배함에도 umap이 1.2~1.9배 speedup(Fig.9a, p.1469-1470). FastMap은 mmap-IO와 비슷한 성능(ImageNet 샘플이 200~300KB로 상대적으로 커 lock-free 최적화 이득이 작음).
- vLLM serverless LLM 추론 weight loading: umap이 mmap-IO 대비 2.3배 빠른 bulk loading(Fig.9b, p.1470).
- OpenBLAS 과학 컴퓨팅(6개 행렬 루틴, $2^{15}\times2^{15}$ float64): umap이 mmap-IO·FastMap 대비 13%~28% 우수(Fig.10, p.1470).
- Backtrader 금융 backtesting(2020-2025 다국 시장 데이터): umap이 mmap-IO 대비 최대 **6.7배** JCT speedup(Fig.11a, p.1470), 메모리 사용률은 mmap-IO가 거의 100%인 반면 umap은 8%~31%(Fig.11b, p.1470).
- Operational Resilience: 프로덕션에서 재정 job은 하루 10건 이상의 livelock을 겪었고, 건당 100 core-hour 이상 손실되었으나 umap 도입 후 18개월간 job termination 0건(p.1470).

**마이크로벤치마크(§6.3, p.1470-1471):** 128GB FBM 기준, 32스레드에서 umap+GPFS는 mmap-IO+LFS 대비 **2.8배 read**, **8.3배 write** 처리량(Fig.12a-b). mmap-IO는 8~16스레드를 넘으면 `tree_lock` 경합으로 성능이 악화되는 반면, 대역폭 무제한 시뮬레이션(Fig.12c)에서 umap은 64스레드까지 확장. Peak memory(Fig.12d)는 write 벤치마크 32스레드 기준 umap이 mmap-IO/FastMap 대비 **10.4% 미만**의 메모리만 사용.

**Ablation(§6.4, p.1471-1472):** CoM·CaM을 모두 끈 `umap-CoM-CaM`은 성능이 낮고, CoM만 켜면 단일 스레드에서 3.5배 개선되지만 8스레드를 넘어서면 확장하지 못함; CoM+CaM 모두 있는 완전한 umap만 선형 확장(Fig.13a). PIAO 병합 크기는 spatial locality 파라미터 $\lambda$에 따라 17.2KB($\lambda=0$, 완전 랜덤)에서 127.9KB($\lambda=1$, 완전 순차)까지 증가(Fig.14, p.1471). 캐시 관리 ablation(Fig.13b)은 memory-to-file 비율 4:1~1:4 전 범위에서 안정적 성능을 보여 Fig.4의 mmap-IO 급격한 성능 저하와 대조.

**Table 1 (실행 프로파일, p.1472):** mmap-IO+DFS는 CPU cycles 3.4×10^12, iowait/JCT 88.9%, lock 경합/(JCT−iowait) 76.1%인 반면 umap+DFS는 CPU cycles 6.3×10^10(약 98.2% 감소), lock 경합 비율 1.2%로 급감. iowait/JCT 비율 자체는 umap(15.0%)이 mmap-IO+LFS(1.7%)보다 높지만, 절대 JCT가 훨씬 짧아 절대 iowait 시간은 더 낮다고 저자는 설명한다(p.1471-1472).

**Latency trade-off(§6.5, p.1472):** umap은 요청 병합·스케줄링 로직 때문에 개별 4KB read의 per-access latency는 mmap-IO보다 높다(Fig.16). 그러나 FBM 워크로드는 단일 접근 latency가 아니라 aggregate throughput(또는 vLLM 같은 서비스의 cold-start loading time)에 지배되므로 이 trade-off는 대부분 워크로드에 유리하다. latency-critical한 4KB 미만 random I/O에서는 로컬 NVMe SSD 위 mmap-IO가 여전히 이상적이라고 명시한다(p.1472).

## 섹션 노트
- **§1 Introduction**: 행렬 접근이 ML/금융/과학 컴퓨팅의 공통 병목이며 FBM+mmap-IO가 표준 관행임을 소개하고, disaggregated storage에서 이 abstraction이 깨진다는 문제의식을 제시.
- **§2 Motivation, Challenges, Solutions**: FBM 워크로드 특성(C1 random access, C2 high concurrency, C3 large scale)을 정의하고, 단일/멀티스레드/멀티잡 실험으로 각각 storage underutilization, non-scalable throughput, 과도한 caching의 문제를 정량 검증.
- **§3 Overview**: umap 아키텍처(CaM+CoM)와 실행 흐름, 메모리 오버헤드(~1/1024), consistency model을 소개.
- **§4 Design**: 4.1 네트워크 친화적 I/O(PIAO+FIFO+least-first, shadow-copy), 4.2 concurrency-aware lock-free 캐시 프로토콜(FSA, rmap), 4.3 lazy-expansion 캐시 관리(Theorem 1, Algorithm 1)를 상세 설계.
- **§5 Implementation**: source-compatible/binary-compatible(LD_PRELOAD) 두 통합 모드와 Sentry CB를 이용한 fast-path 최적화를 설명.
- **§6 Performance Evaluation**: 실사용 워크로드 4종(AI/과학computing/금융/microbenchmark) 전반에서 mmap-IO·FastMap 대비 우위를 검증하고 ablation·fairness·latency trade-off까지 포괄.
- **§7 Related Work**: page cache 관리, prefetching, workload-specific 캐시 관리 연구들과 대비하며 umap이 DFS 네트워크 행동을 명시적으로 고려한 첫 mmap 런타임이라고 주장.
- **§8 Discussion**: 두 conceptual observation(VM 재고, coarse-grained access의 가치)과 세 실무 lesson(observability, greedy caching의 위험성, predictability의 우선순위), 그리고 umap의 적용 범위(NFSv4/GPFS류 네트워크 파일시스템에 한정, EBS 등 block storage는 대상 아님)를 논의.
- **§9 Conclusion**: disaggregated storage에서 FBM 워크로드를 위한 memory mapping 런타임으로서 umap의 기여를 요약.

## 핵심 용어 (Key terms)
- **FBM (File-Backed Matrix)**: 파일시스템에 데이터를 저장하되 mmap을 통해 메모리처럼 접근하는 행렬 추상화.
- **mmap-IO**: `mmap` syscall, page cache, page fault 경로, 파일시스템 통합을 포괄하는 Linux 메모리매핑 I/O 서브시스템 전체를 가리키는 논문 용어.
- **CoM (Communication Manager)**: 페이지 폴트를 병합·스케줄링해 network-friendly한 coarse-grained I/O로 변환하는 umap 컴포넌트.
- **CaM (Cache Manager)**: Cache Set/CET을 관리하고 concurrency-aware lock-free 캐시 교체를 수행하는 umap 컴포넌트.
- **PIAO (Push-In Admission-Out) queue**: rank 기반으로 정렬 삽입하며 인접 요청을 자동 병합하는 큐.
- **CB (Cache Block) / CET (Cache Entry Table)**: 고정 크기 캐시 블록과, 매핑된 파일별 포인터 index array.
- **FSA-based cache protocol**: CB를 Active/Semiactive/Inactive 3상태로 전이시켜 lock 없이 교체를 관리하는 상태 머신.
- **rmap (reference map)**: CB를 참조 중인 스레드 tid 집합으로, 비어있지 않으면 해당 CB는 절대 교체되지 않음(false eviction 방지).
- **Shadow-copy mechanism**: data_ptr/shadow_ptr 두 버퍼를 atomic swap하여 write-back과 접근을 오버랩시키는 기법.
- **Lazy-expansion cache management**: 접근 재등장 거리($R_i$)를 관찰해 필요한 만큼만 LRU 용량을 확장하는 정책(Theorem 1 기반).
- **Sentry CB**: 항상 inactive인 전역 더미 CB로, cache hit fast path의 NULL 체크 단계를 제거.

## 강점 · 한계 · 열린 질문
**강점**: 18개월 프로덕션 배포와 실제 job termination/livelock 통계로 뒷받침된 practical impact가 강력하며, 단순 벤치마크를 넘어 AI/과학/금융 4개 이질적 워크로드 클래스에서 일관된 이득을 보임. Theorem 1처럼 최적 캐시 용량에 대한 수학적 근거를 제시하고, ablation(Fig.13)으로 CoM/CaM 각각의 기여를 분리 검증한 점도 견고하다.

**한계**: umap은 mmap 스타일 weak consistency만 제공하며 노드 간 강한 coherence를 강제하지 않아, cross-node 동시 write가 빈번한 워크로드에는 적합하지 않다(저자도 data-parallel·write-conflict가 드문 워크로드를 명시적으로 타깃함, p.1465,1470). 단일 스레드 random read에서는 오히려 mmap-IO/FastMap보다 20% 낮은 성능을 보이며(§6.3, p.1470), 이는 internal prefetching이나 lock-free 최적화가 단일 스레드에는 도움이 되지 않기 때문. 또한 저자들 스스로 §8 Applicability에서 umap이 EBS 같은 block storage나 로컬 마운트 FS에는 적용 대상이 아니며, NFSv4/GPFS류 distributed metadata/locking을 가진 network file system에 특화됨을 명시한다(p.1473).

**열린 질문**: prefetching 기법(Lynx, HoPP, FastMap 등)과의 결합이 orthogonal하다고 저자가 언급했는데(p.1473, §7) 실제 통합 시 read-heavy 시나리오에서 얼마나 추가 이득이 있는지는 미검증. 또한 실제 하드웨어 실험은 200Gbps NIC 대역폭 한계(190Gbps 근방 plateau, p.1471)에 갇혀 있어, 64스레드 이상 확장성은 bandwidth-unconstrained 시뮬레이션(Fig.12c)에만 의존한다는 점도 향후 실증이 필요한 부분이다.

## ❓ Q&A (자가 점검)
> [!question]- mmap-IO가 DFS에서 LFS 대비 왜 3~10배 느린가?
> 4KB 페이지 단위 I/O가 DFS에서 조각화(fragment)되어 매 페이지 폴트마다 metadata lookup·distributed lock·RPC를 유발하고, 이는 LFS 대비 자릿수 단위로 높은 latency를 낳기 때문이다(p.1463).

> [!question]- PIAO 큐는 무엇이고 왜 FIFO 큐를 병행하는가?
> PIAO는 rank 기반 정렬 삽입 큐로 인접한 요청을 자동 병합해 network I/O 효율을 높인다. 다만 순수 병합만 하면 늦게 도착한 낮은-rank 요청이 무한정 대기(starvation)할 수 있어, 별도 FIFO 큐로 도착 순서를 함께 관리해 dequeue를 FIFO 순서로 강제한다(Figure 6, p.1465).

> [!question]- umap의 캐시 교체가 왜 lock-free한가?
> CaM이 스레드별 thread-local `tid`와 파일별 non-overlapping `rmap`으로 실시간 동시성을 추적해, 참조 중인(rmap 비어있지 않은) CB는 절대 교체 대상에서 제외하는 FSA 상태 전이(Active/Semiactive/Inactive)만으로 correctness를 보장하기 때문이다(p.1466-1467).

> [!question]- Theorem 1이 말하는 최적 LRU 캐시 용량은 무엇인가?
> $N_c = \max(R_i)$, 여기서 $R_i$는 접근 시퀀스에서 $i$번째 원소가 다시 등장하기까지의 거리(랜덤성 정도)다. 이 값이 캐시 미스를 없애는 데 필요한 최소 LRU 용량임을 증명한다(p.1468).

> [!question]- 프로덕션에서 관찰된 livelock의 근본 원인은?
> mmap-IO의 deferred asynchronous write-back이 DFS의 coarse-grained(수백 KB~수십 MB 단위) 분산 lock/consistency 메커니즘과 충돌해, 스레드들이 iowait에 갇히고 batch scheduler가 이를 deadlock으로 오인해 강제 종료(forced termination)시키는 현상이다(p.1470).

> [!question]- umap은 write-after-read(WAR) hazard를 어떻게 방지하는가?
> shadow-copy 메커니즘으로 각 CB에 data_ptr/shadow_ptr 두 버퍼를 두고, write-back 시작 시 두 포인터를 원자적으로 swap해 애플리케이션 접근용 버퍼와 백그라운드 동기화용 버퍼가 항상 분리되도록 한다(Figure 7, p.1466).

> [!question]- umap은 EBS 같은 block storage 시스템에도 적용 가능한가?
> 아니다. §8 Applicability에서 저자들은 umap이 다루는 pathology는 distributed metadata와 locking을 가진 network file system(NFSv4, GPFS 등)에 국한되며, block storage(EBS)나 로컬 마운트 FS는 페이지 폴트가 분산 coordination을 유발하지 않으므로 대상이 아니라고 명시한다(p.1473).

> [!question]- 단일 스레드 read 성능에서 umap의 약점은?
> DFS 위 단일 스레드 random small read에서 umap은 LFS 위 mmap-IO/FastMap 대비 약 20% 낮은 성능을 보인다. internal prefetching이나 lock-free 최적화가 단일 스레드 접근에는 이득을 주지 않기 때문이다(§6.3, p.1470).

## 🔗 Connections
[[File System]] · [[OSDI]] · [[2026]]
관련: [[Multi-Granularity Shadow Paging with NVM Write Optimization for Crash-Consistent Memory-Mapped I-O]] · [[Ethane - An Asymmetric File System for Disaggregated Persistent Memory]]

## References worth following
- Papagiannis et al., "Memory-mapped I/O on steroids," EuroSys 2021 [52] — mmap 성능 최적화 선행 연구, umap의 baseline 비교군인 FastMap과 계보가 닿는 mmap 최적화 계열.
- Papagiannis et al., "Optimizing memory-mapped I/O for fast storage devices" (FastMap), USENIX ATC 2020 [54] — 논문의 핵심 academic SOTA baseline.
- Feng et al., "Tricache: A user-transparent block cache enabling high-performance out-of-core processing with in-memory programs," ACM TOS 2023 [28] — user-space 캐시 투명성 측면에서 umap과 비교되는 관련 연구(§7).
- Peng et al., "Umap: Enabling application-driven optimizations for page management," MCHPC 2019 [56] — 동명이인이지만 다른 프로젝트로, application-driven page management라는 유사 문제의식의 선행 연구.
- Kwon et al. (vLLM), "Efficient memory management for large language model serving with PagedAttention," SOSP 2023 [40] — 논문의 실사용 evaluation 워크로드(vLLM weight loading)의 근거 시스템.

## Personal annotations
<!-- 본인 메모 영역 -->
