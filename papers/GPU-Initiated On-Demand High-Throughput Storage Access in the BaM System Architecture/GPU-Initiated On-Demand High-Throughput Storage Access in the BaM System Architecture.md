---
title: "GPU-Initiated On-Demand High-Throughput Storage Access in the BaM System Architecture"
aliases: [BaM, GPU-Initiated BaM]
description: "GPU 스레드가 CPU 없이 직접 스토리지(NVMe SSD)에 on-demand·fine-grained 접근하게 하는 accelerator-centric 시스템 아키텍처 BaM."
venue: ASPLOS
year: 2023
arxiv: "2203.04910"
tier: deep
status: done
tags: [paper, cluster/infra, topic/gpu-storage, topic/near-storage, topic/bam, venue/asplos, year/2023]
---

# GPU-Initiated On-Demand High-Throughput Storage Access in the BaM System Architecture

> **ASPLOS 2023** · cluster/infra · Source: [GPU-Initiated On-Demand High-Throughput Storage Access in the BaM System Architecture.pdf](<GPU-Initiated On-Demand High-Throughput Storage Access in the BaM System Architecture.pdf>)

**저자:** Zaid Qureshi*, Vikram Sharma Mailthody*, Isaac Gelado, Seungwon Min, Amna Masood, Jeongmin Park, Jinjun Xiong, C. J. Newburn, Dmitri Vainbrand, I-Hsin Chung, Michael Garland, William Dally, Wen-mei Hwu (NVIDIA / UIUC / University at Buffalo / AMD / IBM Research / Stanford) — *공동 1저자.

---

## TL;DR

BaM(**B**ig **a**ccelerator **M**emory)은 GPU 스레드가 CPU를 거치지 않고 **직접(GPU-initiated)** NVMe SSD에 **on-demand·fine-grained**하게 접근하도록 하는 accelerator-centric 시스템 아키텍처다. 핵심은 (1) GPU 메모리에 올려둔 highly-concurrent NVMe **I/O queue(SQ/CQ)**를 GPU 스레드가 직접 채우고 doorbell을 울리는 메커니즘, (2) redundant I/O를 합치고 locality를 살리는 **software-defined cache**(`bam::array<T>` 추상화)다. 결과적으로 host-memory DRAM-only 솔루션 대비 **on-par 성능을 최대 21.7배 저렴한 비용**으로 제공하고, 동일 하드웨어에서 CPU-centric 소프트웨어 대비 그래프 분석 BFS/CC를 **1.0×·1.49×**, data-analytics를 **최대 5.3×** 가속한다. 512B 랜덤 read에서 SSD당 peak IOPs(예: Optane 단일 SSD 4.55M, 10개 SSD 45.8M read / 10.6M write IOPs)에 도달하며 SSD 수에 선형 확장한다.

## 문제 & 동기

- GPU는 compute throughput·메모리 대역폭이 비약적으로 컸지만, GPU 메모리 용량(A100 80GB)은 graph analytics·recommender·GNN 등 **수십 GB~수십 TB** 데이터셋에 비해 턱없이 작다.
- 기존 두 가지 **CPU-centric** 접근의 한계:
  1. **Proactive tiling** — CPU/OS가 데이터셋을 타일로 쪼개 GPU로 미리 옮기고 커널을 여러 번 launch. data-dependent 접근에서는 어떤 부분이 언제 필요한지 CPU가 알 수 없어 불필요한 바이트까지 fetch → **I/O amplification**. kernel launch/termination마다 CPU-GPU 동기화 비용.
  2. **Memory-mapped + UVM page fault** — GPU가 page fault를 내면 CPU page fault handler가 데이터를 옮김. 측정 결과 UVM page fault handler가 CPU 100% 점유, 최대 ~500K IOPs로 saturate(Samsung 980pro peak의 절반 수준), 평균 PCIe 대역폭 ~14.52GBps로 Gen4 peak의 55.2%에 불과.
- **DRAM-only**(host memory 확장 또는 multi-GPU 메모리 pooling) 대안은 데이터를 미리 GPU로 로딩해야 하고(initial loading bottleneck), 용량 확장이 **금지될 정도로 비싸다**(예: 10TB는 A100 125개 필요).
- 선행 GPU-initiated 시도(GPUfs[57], Syscalls for GPU[58])는 throughput이 낮았다(~823K IOPs).
- **갭:** GPU-initiated storage access를 효율적으로 만드는 시스템 아키텍처·소프트웨어 스택이 부재. BaM이 이를 채운다.

## 핵심 통찰

- **Latency tolerance:** GPU의 massive thread-level parallelism(CPU보다 최대 3 orders of magnitude 많음)을 이용하면 storage latency를 hide할 수 있다. Little's Law(T×L=Q_d)에 따라 충분한 동시 in-flight 요청만 유지하면 device peak throughput에 도달 — 예: 512B Optane에서 51M accesses 유지에 561 requests, 980pro에서 16,524 requests 필요.
- **Critical section 최소화:** 기존 NVMe queue 삽입/doorbell 갱신을 critical section으로 구현하면 수천 GPU 스레드의 serialization 비용이 막대하다. BaM은 fine-grained memory synchronization(ticket counter, turn_counter, mark bit-vector, lock)으로 lock을 cache line insert/evict 시에만 사용하고, doorbell write를 **여러 스레드에 대해 batch(coalesce)**한다.
- **Software cache로 I/O amplification 억제:** redundant 요청을 합치고(coalesce), warp 내 스레드를 cache line 단위 그룹으로 나눠(`__match_any_sync`) leader만 cache를 probe → fine-grained 접근으로 실제 사용한 바이트만 가져온다.
- **Accelerator-centric 트렌드:** GPUDirect Async 계열의 control plane 자율화 연장선. BaM은 GPU kernel이 initiate하는 새 변종 — *GPUDirect Async KI (Kernel Initiated) Storage*.

## 설계 / 메커니즘

- **`bam::array<T>` 추상화:** mmap된 파일처럼 array 형태로 storage를 노출. overloaded subscript operator가 cache probe, miss 시 I/O 요청, coalescing을 처리. legacy GPU 커널 수정 최소화.
- **GPU 스레드의 생애(Figure 2):**
  1. *BaM Lookup* — offset 계산 → coalesce → cache lookup. hit이면 GPU 메모리 직접 접근.
  2. *I/O Stack* — miss 시 SQ에 command 준비·enqueue, doorbell 갱신, CQ에서 completion poll.
  3. *Storage Controller* — doorbell 수신 → SQ entry fetch → SSD↔GPU 메모리 DMA → CQ에 completion post. 스레드가 cache state·SQ/CQ state 갱신 후 데이터 접근.
- **High-throughput I/O queue:** SQ당 metadata로 head/tail, atomic **ticket counter**, **turn_counter** 배열, **mark bit-vector**, **lock** 유지. 스레드는 ticket을 atomic하게 받아 가상 큐(2^32 entry) 위치를 얻고, turn 값으로 물리 큐 안에서 순서를 정렬(sub-queue). `move_tail` 루틴에서 한 스레드가 lock을 얻어 연속된 새 entry들의 tail을 한 번에 옮기고 doorbell write를 **coalesce**. CQ도 유사하게 관리.
- **Software cache:** 시작 시 virtual·physical 메모리를 모두 미리 할당해 critical section을 insert/evict로 한정. **clock replacement** 알고리즘, reference count(pinning), dirty bit(write-back cache). warp 내 **warp coalescing**으로 동일 cache line 접근 스레드를 그룹화 → leader만 probe(선행 연구처럼 serialize하지 않음).
- **Prototype:** off-the-shelf NVIDIA A100 + NVMe SSD array. custom Linux driver(`libnvm`)가 NVMe당 character device 생성, GPUDirect RDMA로 NVMe queue·I/O buffer를 GPU 메모리에 pin/map, GPUDirect Async로 doorbell을 CUDA 주소공간에 map(`cudaHostRegister`). PCIe expansion chassis(H3 Falcon-4016)로 SSD 수 확장.

## 평가

- **시스템:** Supermicro AS-4124GS-TNR, 2× EPYC 7702, 1TB DDR4, A100-80GB, Ubuntu 20.04 / CUDA 11.4. SSD: Intel Optane P5800X, Samsung Z-NAND(PM1735/DC S1735), Samsung 980pro.
- **Raw throughput(Figure 4):** 512B 랜덤. 단일 Optane SSD에서 약 16K~64K 스레드로 peak 도달. 10개 Optane에서 **45.8M read / 10.6M write IOPs**(22.9GBps read = Gen4 ×16 측정 peak의 90%, 5.3GBps write). SSD 수에 선형 확장. write는 아직 PCIe 한계 미도달.
- **GDS·ActivePointers 대비(Figure 5,6):** GDS는 32KB 미만 granularity에서 PCIe saturate 실패(4KB에서 peak의 23.6%), BaM은 **4KB에서도 ~25GBps saturate**. ActivePointers(+GPUfs) 대비 peak miss-handling **17 MIOPs(20.7×↑)**, peak hot cache 대역폭 **430GBps(11.2×↑)**.
- **그래프 분석(Figure 7, Table 3):** BFS·CC, 5개 대형 그래프(K/U/F/M=SuiteSparse, Uk=LAW). 단일 Optane은 ×4 Gen4 대역폭 제약으로 target T보다 BFS 1.43×·CC 1.27× 느림. 4개 SSD(B_4I)로 확장 시 host-memory file-loading 기준 T 대비 **평균 BFS 1.00× / CC 1.49× speedup**, single-SSD 대비 **BFS 3.48× / CC 4×** 확장.
- **Cache 효과(Figure 8):** naive cache만으로 BFS 11.9× / CC 12.65×, warp coalescing·reference reuse까지 활용(Optimized) 시 추가로 BFS 6.07× / CC 11.24×.
- **SSD 종류·민감도:** consumer 980pro는 Optane 대비 BFS 3.21× / CC 2.68× 느리지만 비용이 압도적으로 저렴. cache 1GB로도 8GB와 동일 성능, queue pair는 40개까지 무손실(그 이하부터 저하).
- **Data analytics(Figure 12,14):** NYC Taxi 1.7B rows, RAPIDS 대비 Q0에서 1.22×, data-dependent metric(Q1~Q5) 추가될수록 격차 확대 **최대 5.3×**(4 SSD). RAPIDS는 컬럼별 I/O amplification이 6×까지 선형 증가.
- **비용:** SSD 종류 무관 DRAM-only 대비 **cost-per-GB 4.3~21.7배 우위**(expansion chassis·riser 포함), 용량 증가 시 격차 확대.
- **VectorAdd(write-intensive):** read-miss와 write-back overlap 미지원으로 baseline 대비 1.51× 느림(future work). register 사용량은 늘지만 storage I/O bound라 occupancy 병목 아님.

> [!quote]- 📄 원문 표현 (paper)
> - "Graphics Processing Units (GPUs) have traditionally relied on the host CPU to initiate access to the data storage. ... However, emerging applications such as graph and data analytics, recommender systems, or graph neural networks (GNNs), require fine-grained, data-dependent access to storage." (p.1)
> - "GPU-initiated storage removes these overheads from the storage control path and, thus, can potentially support these applications at much higher speed. However, there is a lack of systems architecture and software stack that enable efficient GPU-initiated storage access. This work presents a novel system architecture, BaM, that fills this gap." (p.1)
> - "Experimental results show that BaM delivers 1.0× and 1.49× end-to-end speed up for BFS and CC graph analytics benchmarks while reducing hardware costs by up to 21.7× over accessing the graph data from the host memory. Furthermore, BaM speeds up data-analytics workloads by 5.3× over CPU-initiated storage access on the same hardware." (p.1)
> - "BaM provides a user-level GPU library of highly concurrent submission/completion queues in GPU memory that enables GPU threads whose on-demand accesses do not hit in the software cache to make storage accesses in a high-throughput manner." (p.2)
> - "This coalesces the doorbell writes, which are expensive operations over the PCIe interconnect, for multiple calling threads and improves the effective throughput." (p.5)

## 섹션 노트

- **§2 Background:** Little's Law로 latency tolerance를 정량화. user-level I/O queue(io_uring 류)는 kernel crossing을 없애지만 queue insertion을 critical section으로 두면 수천 GPU 스레드에서 serialization이 치명적 — §3에서 해결.
- **§3 BaM System:** 3대 도전 — (1) GPU parallelism으로 latency tolerate(§3.3), (2) 제한된 자원(GPU 메모리·SSD 대역폭) 최적 활용(§3.4 cache), (3) 프로그래머에게 쉬운 추상화(§3.5 `bam::array<T>`).
- **§4 Prototype:** PCIe ×16 Gen4 한 링크 대역폭에 NVMe 여러 개를 묶어 매칭. expansion chassis 2 drawer, drawer당 ×16 슬롯 1개에 A100, 나머지에 SSD(PCIe bifurcation으로 drawer당 16+ M.2). GPUDirect RDMA consistency 이슈는 shared global virtual queue + lock으로 처리(<8% overhead).
- **§5 Evaluation:** §B(부록)에 proactive tiling·reactive page fault 동기 상세. arXiv full version[50]에 추가 분석.
- **§6 Related work:** SPIN/NVMMU(P2P SSD↔GPU), GAIA, Gullfoss, Hippogriffdb, GDS는 모두 CPU가 데이터 이동을 orchestrate. ActivePointers/GPUfs/GPUNet/Syscalls는 accelerator-centric 시도이나 CPU가 transfer를 service. BaM은 GPU thread가 직접 access를 initiate.

## 핵심 용어

- **BaM (Big accelerator Memory):** GPU-initiated on-demand 스토리지 접근을 위한 accelerator-centric 시스템 아키텍처.
- **GPU-initiated (KI, Kernel-Initiated):** GPU 커널 스레드가 CPU 개입 없이 직접 I/O를 발생·trigger.
- **`bam::array<T>`:** mmap된 파일처럼 array 형태로 스토리지를 노출하는 high-level C++/Python/Rust 친화 추상화.
- **I/O amplification:** 실제 필요한 바이트보다 많이 전송되는 비효율(coarse-grain tiling/UVM의 폐해).
- **Software-defined cache:** GPU 메모리 내 사용자 정의 cache; clock replacement, reference count, write-back, warp coalescing.
- **Warp coalescing:** `__match_any_sync`로 같은 cache line 접근 스레드를 그룹화, leader만 probe.
- **Ticket/turn_counter:** lock-free에 가까운 queue 삽입을 위한 가상 큐 인덱싱·순서 정렬 메커니즘.
- **Little's Law (T×L=Q_d):** 목표 throughput 유지에 필요한 큐 깊이(동시 in-flight 요청 수).
- **GPUDirect RDMA / Async:** NVMe queue·doorbell을 GPU 주소공간에 매핑하는 NVIDIA 기술.
- **CPU-centric / DRAM-only:** 비교 baseline — CPU가 데이터 이동 주도 / host·multi-GPU 메모리로 용량 확장.

## 강점 · 한계 · 열린 질문

**강점**
- off-the-shelf 하드웨어만으로 DRAM-only 대비 21.7배 저렴하게 on-par 성능 — 강력한 cost-performance 논거.
- fine-grained·data-dependent 접근(graph, analytics)에서 I/O amplification을 근본적으로 줄임.
- SSD 수에 선형 확장하는 raw throughput, consumer-grade SSD로도 동작(접근성).
- 완전 open-source(하드웨어·소프트웨어).

**한계**
- write-intensive 워크로드에서 read-miss와 write-back overlap 미지원 → VectorAdd 1.51× 느림(async write-back은 future work).
- 단일 SSD는 PCIe ×4 Gen4 대역폭에 묶여 target보다 느릴 수 있음(SSD 다수 필요).
- 충분한 thread-level parallelism이 없는 워크로드(Uk처럼 frontier가 작고 깊은 BFS)는 latency hiding이 약해 확장 저하.
- crash consistency는 애플리케이션의 check-pointing 책임(보장 없음).

**열린 질문**
- async write-back 도입 시 write-intensive 워크로드 성능은 얼마나 회복되나?
- CXL·disaggregated 메모리/스토리지 패브릭으로 BaM 모델이 어떻게 확장되나?
- GNN training/inference 등 더 복잡한 파이프라인에서 end-to-end 이득은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. BaM이 풀려는 핵심 문제는 무엇인가?
> 큰 데이터셋(수십 GB~TB)을 다루는 emerging GPU 워크로드(graph/data analytics, recommender, GNN)는 fine-grained·data-dependent 스토리지 접근이 필요한데, 기존 CPU-centric 방식(proactive tiling, UVM page fault)은 CPU-GPU 동기화·I/O amplification·OS page fault handler 병목으로 throughput이 낮다. BaM은 GPU 스레드가 CPU 없이 직접 고throughput으로 스토리지에 접근하게 한다.

> [!question]- Q2. GPU가 어떻게 storage latency를 견디나?
> GPU의 massive thread-level parallelism(CPU 대비 최대 1000배)을 이용해 수많은 요청을 in-flight로 유지한다. Little's Law(T×L=Q_d)에 따라 충분한 동시 요청만 있으면 device peak throughput에 도달하고, 한 스레드의 latency를 다른 스레드의 compute로 overlap한다.

> [!question]- Q3. 기존 NVMe queue를 그대로 쓰면 왜 안 되고 BaM은 어떻게 해결했나?
> queue 삽입+doorbell 갱신을 critical section으로 두면 수천 GPU 스레드가 serialize되어 막대한 latency가 생긴다. BaM은 ticket counter·turn_counter·mark bit-vector·lock으로 fine-grained synchronization을 구현, lock을 cache insert/evict로 한정하고 doorbell write를 여러 스레드에 대해 coalesce한다.

> [!question]- Q4. software cache는 어떤 역할을 하고 어떻게 동작하나?
> redundant I/O를 합쳐 I/O amplification을 줄이고 locality를 살린다. 시작 시 모든 메모리를 미리 할당해 critical section을 줄이고, clock replacement·reference count(pinning)·dirty bit(write-back)을 쓴다. warp coalescing(`__match_any_sync`)으로 같은 cache line 스레드를 그룹화해 leader만 probe한다.

> [!question]- Q5. 대표적인 정량 성능·비용 수치는?
> 그래프 분석에서 host-memory(DRAM-only) 대비 BFS 1.0× / CC 1.49× on-par이면서 비용은 최대 21.7배 저렴. 동일 하드웨어 CPU-centric 대비 data analytics 최대 5.3×. raw throughput은 10 Optane SSD에서 45.8M read / 10.6M write IOPs(512B), SSD 수에 선형 확장.

> [!question]- Q6. GDS 및 ActivePointers와 어떻게 다른가?
> GDS는 CPU가 데이터 path를 orchestrate하고 32KB 미만 granularity에서 PCIe를 saturate 못한다(4KB에서 peak의 23.6%). BaM은 4KB에서도 ~25GBps saturate. ActivePointers(+GPUfs)는 CPU가 transfer를 service해 miss-handling이 느린데, BaM은 17 MIOPs(20.7×)·hot cache 430GBps(11.2×)로 압도한다.

> [!question]- Q7. BaM의 한계는?
> read-miss와 write-back을 overlap하지 못해 write-intensive(VectorAdd)에서 1.51× 느림. 단일 SSD는 PCIe ×4 Gen4에 묶여 target보다 느릴 수 있음. parallelism이 부족한 워크로드(깊고 frontier 작은 BFS, Uk)는 latency hiding이 약함. crash consistency는 앱 책임.

> [!question]- Q8. 어떤 하드웨어/소프트웨어로 prototype을 만들었나?
> off-the-shelf NVIDIA A100 + NVMe SSD array. custom Linux driver(`libnvm`)가 NVMe당 char device 생성, GPUDirect RDMA로 NVMe queue·I/O buffer를 GPU 메모리에 pin/map, GPUDirect Async로 doorbell을 CUDA 주소공간에 map. PCIe expansion chassis(H3 Falcon-4016)로 SSD 다수 연결. 전부 open-source.

## 🔗 Connections

[[Infra]] · [[ASPLOS]] · [[2023]] · 관련 [[Smart-Infinity]]

- **[[Smart-Infinity]]** — near-storage/CSD로 GPU 메모리 한계를 우회하는 또 다른 접근. BaM은 GPU가 직접 SSD를 read/write, Smart-Infinity는 storage-side accelerator로 연산 offload — 데이터셋 > GPU 메모리 문제에 대한 상보적 해법.
- accelerator-centric storage 계열: SPIN, NVMMU, GPUfs, ActivePointers, GPUDirect Storage(GDS).
- 응용: graph analytics(BFS/CC), GNN, recommender, RAPIDS data analytics.

## References worth following

- [57] ActivePointers — software address translation on GPUs (Silberstein et al., ISCA 2016).
- [58] GPUfs — file system support for GPUs (Silberstein et al., ASPLOS 2013).
- [8] SPIN — P2P DMA SSD↔GPU를 OS file stack에 통합 (USENIX ATC 2017).
- [70] NVMMU — non-volatile memory management unit for GPU-SSD (PACT 2015).
- [39] EMOGI — out-of-memory graph traversal, host-memory fine-grain access(target baseline 근간, VLDB 2020).
- [47] GPUDirect Storage(GDS) — NVIDIA 비교 대상.
- [46] RAPIDS — GPU-accelerated data analytics(비교 대상).
- [50] arXiv full version — appendix 추가 분석 (arxiv.org/abs/2203.04910).
- [48] UVM — Unified Virtual Memory.
- [42] GPUDirect Async family.

## Personal annotations

<본인 메모 영역>
