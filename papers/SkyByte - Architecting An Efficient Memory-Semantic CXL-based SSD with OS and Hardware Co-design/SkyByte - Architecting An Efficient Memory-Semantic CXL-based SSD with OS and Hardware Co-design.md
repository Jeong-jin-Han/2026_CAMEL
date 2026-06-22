---
title: "SkyByte: Architecting An Efficient Memory-Semantic CXL-based SSD with OS and Hardware Co-design"
aliases: [SkyByte]
description: "OS-하드웨어 co-design(coordinated context switch + cacheline 단위 write log + adaptive page migration)으로 CXL-SSD의 긴 flash 지연과 granularity mismatch를 해결해 6.11배 가속한 연구"
venue: HPCA
year: 2025
tier: deep
status: done
presenter: 정진
present-date: 2026-07-09
tags:
  - paper
  - cluster/mine
  - cluster/cxl
  - topic/cxl
  - topic/memory-semantic-ssd
  - topic/hw-sw-codesign
  - topic/os-codesign
  - venue/hpca
  - year/2025
---

# SkyByte: Architecting An Efficient Memory-Semantic CXL-based SSD
> **HPCA 2025** · `cluster/cxl` · Source: [SkyByte - Architecting An Efficient Memory-Semantic CXL-based SSD with OS and Hardware Co-design.pdf](<SkyByte - Architecting An Efficient Memory-Semantic CXL-based SSD with OS and Hardware Co-design.pdf>)
저자: Haoyang Zhang*, Yuqi Xue*, Yirui Eric Zhou, Shaobo Li, Jian Huang (University of Illinois Urbana Champaign) (*: co-primary authors)

## TL;DR
SkyByte는 CXL.mem을 통해 byte-granular load/store로 접근하는 CXL 기반 SSD(CXL-SSD)의 두 가지 근본 문제 — 긴 flash 접근 지연으로 인한 CPU pipeline stall과, byte-granular CXL interface와 page-granular flash 사이의 granularity mismatch로 인한 I/O amplification — 를 host OS와 SSD controller의 co-design으로 해결한다 (p.1). 핵심 메커니즘은 (1) 긴 SSD 지연을 감지해 다른 thread로 넘기는 coordinated context switch, (2) cacheline 단위 write log + page 단위 read-write cache로 SSD DRAM을 재설계, (3) hot page를 host로 올리는 adaptive page migration이다 (p.2, p.4). 실험 결과 state-of-the-art CXL-SSD 대비 평균 6.11배 성능 향상과 flash로의 I/O write traffic 23.08배 감소를 달성하고, 무한 host DRAM을 가정한 ideal case 성능의 75%에 도달한다 (p.1).

## 문제 & 동기 (Problem & Motivation)
CXL-SSD는 load/store 명령으로 SSD를 main memory처럼 쓸 수 있게 해주지만, 그대로 host memory의 확장으로 취급하면 심각한 성능 저하가 발생한다. 이유는 세 가지다 (p.1): (1) flash 접근 지연이 host DRAM보다 수 자릿수 크고, 내부 DRAM cache가 작아(수 GB) miss 시 긴 지연을 그대로 겪는다; (2) out-of-place update와 garbage collection(GC) 때문에 flash 관리가 read/write 요청을 지연시켜 긴 CPU stall을 유발한다; (3) CXL은 byte-granular 접근을 가능케 하지만 flash는 page-granular만 지원해 granularity mismatch가 높은 I/O amplification을 낳는다.

저자들의 분석에 따르면 워크로드가 DRAM 대비 CXL-SSD에서 1.5–31.4배 느려지며(p.3, Fig.2), memory-bounded cycle 비율이 DRAM의 62.9–98.7%에서 CXL-SSD의 77–99.8%로 증가한다(p.3, Fig.4). 또한 대부분 워크로드가 page의 75% 이상에서 cacheline의 40% 미만만 접근하여(p.2, Fig.5/Fig.6), 전체 page를 SSD DRAM에 caching하는 것이 공간을 낭비하고 write amplification을 일으킨다.

> [!quote]- 📄 원문 표현 (paper)
> - "simply treating SSDs as an extension of host memory via CXL causes dramatic performance degradation and excessive CPU stalls" (p.1)
> - "The tail latency of CXL-SSD causes severe processor pipeline stalls, leading to both performance degradation and underutilization of CPU and SSD bandwidth." (p.3)
> - "our study in §II finds that most workloads access less than 40% of cachelines in more than 75% of pages. Caching the entire page in the SSD DRAM significantly wastes precious SSD DRAM space." (p.2)

## 핵심 통찰 (Key Insight)
현대 OS는 host DRAM에서 thread가 긴 접근으로 막히면 context switch로 다른 thread를 실행해 CPU를 활용하지만, CXL-SSD에서는 이 기회가 사라진다. OS가 host CPU에서 SSD로 직접 발행된 load/store memory 명령을 가로챌(intercept) 수 없기 때문이다 (p.1). SkyByte의 핵심 통찰은 host OS와 SSD controller가 협력해야만 어느 명령이 긴 SSD 지연으로 막혔는지 정확히 추적하고 context switch를 트리거할 수 있다는 것이다 (p.4). CPU도 SSD도 단독으로는 hit/miss 여부와 speculative 여부를 알 수 없으므로 양쪽의 coordination이 필수다.

> [!quote]- 📄 원문 표현 (paper)
> - "this context switch opportunity is missing for CXL-SSDs, because the OS cannot intercept the load/store memory instructions issued directly from the host CPU to the SSD device via the CXL protocol." (p.1)
> - "Neither CPU nor SSD can by itself decide whether to trigger a context switch. Therefore, to enable context switch on long CXL-SSD memory stalls, we coordinate between host OS and SSD controller." (p.4)

## 설계 / 메커니즘 (Design)
SkyByte는 세 가지 주요 컴포넌트로 구성된다 (p.4).

**1. Coordinated context switch (§III-A, Fig.7, p.5).** SSD DRAM cache miss로 긴 지연이 예상되면 SSD가 다른 thread로 context switch를 host OS에 요청한다. 동작 흐름:
- (C1) host CPU가 tracking 정보와 함께 CXL.mem `MemRd` 요청을 보낸다. shared LLC의 MSHR이 어느 load/store가 응답을 기다리는지 추적하고 memory access coalescing도 수행한다 (p.5).
- (C2) SSD가 cache miss를 감지하면 estimated access latency에 기반해 context switch 여부를 결정하고, CXL.mem No Data Response(NDR) message로 요청을 보낸다. SkyByte는 NDR 사양에 `SkyByte-Delay`라는 새 opcode를 추가하여(reserved opcode 활용, Fig.8) 해당 `MemRd`가 긴 지연을 겪을 것임을 표시한다 (p.5).
- (C3) host CXL controller가 이 NDR을 받으면 LLC MSHR과 상위 cache 계층(L1, L2)을 탐색해 이 응답을 기다리는 uncommitted memory 명령을 찾고, 그 명령이 retire stage에 진입할 때 새로 정의한 *SkyByte Long Delay Exception*을 트리거한다(Page Fault Exception과 유사). retire까지 지연시키므로 squash될 speculative 명령으로 인한 false-positive context switch를 추가 하드웨어 비용 없이 제거한다 (p.5).
- (C4) exception handler가 x86 IDT에 설치된 CXL-aware thread scheduling policy를 호출해 다음 thread를 선택하고 context switch를 수행한다. 원래 thread가 다시 스케줄되면 막혔던 명령부터 재개해 다시 SSD로 요청한다 (p.5).

언제 context switch를 트리거할지는 *threshold-based policy*(Algorithm 1)로 결정한다. FTL mapping table로 PPA를 구해 flash channel queue 상태(큐에 있는 요청 수)를 질의하고 estimated latency = read_lat·(num_read+1) + write_lat·num_write + erase_lat·num_erase를 계산하여 threshold(기본 2 µs)보다 크면 switch한다. GC로 막힌 요청은 GC가 보통 milliseconds 지속되므로 즉시 switch한다 (p.6, p.5). 스케줄링은 Round-Robin / Random / CFS 중 Linux 표준인 CFS를 기본 사용한다 (p.6, Fig.10).

**2. CXL-aware SSD DRAM management (§III-B, Fig.11).** SSD DRAM을 cacheline-granular *double-buffered write log* + page-granular read-write *data cache*로 재구성한다. 모든 cacheline write는 원본 page를 먼저 fetch하지 않고 write log의 tail에 append되어 critical path에서 flash 접근을 제거한다(W1). data cache에 해당 page가 있으면 병렬로 갱신하고(W2), log index도 갱신한다(W3) (p.7). write log lookup은 two-level hash table(Fig.12)로 amortized O(1)을 달성한다 — 1st level은 LPA로 색인하고 2nd level은 page 내 offset별 cacheline을 추적하여 같은 page의 모든 cacheline을 한 번에 찾아 compaction에 유리하다. log index의 memory footprint는 평균 5.6MB이다 (p.7). read는 data cache와 write log를 병렬 lookup하며, 둘 다 miss면 flash에서 page를 가져온 뒤 write log의 최신 cacheline을 merge한다(R1–R3) (p.7).

**3. Write log compaction (Fig.13, p.7-8).** log가 가득 차면 background에서 같은 page로의 write를 coalesce하여 flash write traffic을 크게 줄인다. indexing table의 최신 갱신만 추적하므로 오래된 갱신은 compaction에서 drop된다. dirty cacheline을 가진 page를 찾아 cached면 직접 flush, 아니면 coalescing buffer로 load 후 merge하여 flash로 batch write한다. double-buffered log로 compaction 중에도 incoming write를 새 log로 받아 blocking을 피하며, 한 번 compaction은 평균 146 µs이다 (p.7-8).

**4. Adaptive page migration (§III-C, p.8).** SSD가 flash page의 access count를 추적해 threshold를 넘는 hot page를 PCIe MSI-X interrupt로 host에 migrate한다(SSD DRAM cache에 있는 page만 대상). data consistency를 위해 root complex의 Promotion Look-aside Buffer(PLB, 64 entry)로 진행 중 migration을 추적하며, 완료 후 OS가 PTE와 TLB를 갱신한다. host space가 부족하면 Linux page reclamation policy로 cold page를 SSD로 evict한다 (p.8). NUMA 및 huge page(2MB)도 지원한다 (p.8).

> [!quote]- 📄 원문 표현 (paper)
> - "SkyByte extends the NDR message specification by introducing a new opcode called SkyByte-Delay. This opcode indicates that the corresponding MemRd request will suffer from a long access delay (e.g., an SSD DRAM cache miss)." (p.5)
> - "Such a design eliminates false-positive context switches, where a load/store instruction triggers a context switch that is later squashed, at no extra hardware cost" (p.5)
> - "All cacheline writes are directly appended to the log without flash access along the critical path. They are flushed to the flash memory later." (p.6)
> - "SkyByte breaks the indexing structure into two levels. At the first level, we employ a hash table indexed by the logical page address (LPA). Each valid entry points to a second level hash table that tracks all logged cache lines in this logical flash page with their offset within the page." (p.7)

## 평가 (Evaluation)
구현은 MacSim + SimpleSSD 기반 CXL-SSD 시뮬레이터이며, FPGA SoC(Xilinx Zynq UltraScale+ ZU3EG) prototype으로 write log/cache의 lookup 지연(64MB write log 72ns, 512MB data cache 49ns)과 처리량(cacheline read/write 11.93/9.37 GB/s)을 검증했다 (p.9). 워크로드는 Rodinia, GAP, Splash3, WHISPER, DLRM 등 8개 멀티스레드 데이터 집약 워크로드이다 (Table I, p.9).

핵심 수치 (Base-CSSD = 최신 기법 다 적용한 SOTA CXL-SSD 기준):
- SkyByte-Full이 Base-CSSD 대비 평균 **6.11배** (최대 16.35배) 성능 향상 (p.10, Fig.14).
- average memory access time을 **14.19배** 개선, flash write traffic을 **23.08배** 감소 (p.9, p.1).
- 무한 host DRAM을 가정한 DRAM-Only(ideal) 성능의 **75%**에 도달, 즉 end-to-end 성능 저하가 평균 1.33배에 불과 (p.1, p.10).
- ablation: SkyByte-P(page migration only) 1.84배, SkyByte-W(write log only) 2.16배, SkyByte-CP 2.79배, SkyByte-WP 2.95배 (p.10, Fig.14).
- 비용: DDR5 DRAM \$4.28/GB, ULL SSD \$0.27/GB(2024 여름) 기준 SkyByte-Full은 DRAM-only 대비 **15.9배 저렴**하고 cost-effectiveness를 **11.8배** 개선 (p.10).
- context switch 이점은 thread 수와 memory bandwidth utilization에 비례해 증가하나, context switch overhead가 flash read latency를 넘으면 이점이 줄어든다 (p.10-11, Fig.15).
- alternative page migration 비교: SkyByte-CP가 TPP 기반 SkyByte-CT보다, AstriFlash-CXL보다 우수(최대 1.21배, 평균 1.09배) (p.12, Fig.23).

> [!quote]- 📄 원문 표현 (paper)
> - "Our experiments show that SkyByte outperforms current CXL-based SSD by 6.11×, and reduces the I/O traffic to flash chips by 23.08× on average. SkyByte also reaches 75% of the performance of the ideal case that assumes unlimited DRAM capacity in the host" (p.1)
> - "(3) SkyByte reduces the average memory access time by 14.19× and the flash write traffic by 23.08× over the state-of-the-art CXL-SSD design" (p.9)
> - "SkyByte-Full costs 15.9× less than the DRAM-only setup and improves cost-effectiveness by 11.8×." (p.10)

## 섹션 노트 (Section notes)
- **II. Background & Motivation**: CXL의 세 protocol(.io/.cache/.mem)과 Type-1/2/3 device 설명. SSD는 Type-3(HDM)로 전체가 host physical memory에 매핑된다. CXL-SSD의 세 문제(long tail latency, pipeline stall, granularity mismatch)를 측정으로 입증 (p.2-4).
- **III. Design**: 세 컴포넌트(coordinated context switch / CXL-aware SSD DRAM / adaptive page migration)와 각 메커니즘의 하드웨어·OS 수정 사항 기술 (p.4-8).
- **IV. Discussion**: data persistence는 `clwb`로 SSD DRAM 도달 보장 + page pin 옵션 제공. NUMA·multiple page size 지원 (p.8).
- **V. Implementation**: MacSim에 context switch 스케줄링 확장, PLB 구현, FPGA prototype으로 성능 모델 검증 (p.9).
- **VI. Evaluation**: ablation, thread 수/write log 크기/SSD DRAM 크기/flash latency sensitivity, alternative migration 비교 (p.9-12).
- **VII. Related Work**: CXL memory architecture, memory-semantic SSD(ByteFS 등), informing memory operations, AstriFlash와의 차별점(SSD를 black box·page granularity로 취급 vs. SkyByte의 co-design) (p.12).

## 핵심 용어 (Key terms)
- **CXL-SSD (memory-semantic SSD)**: CXL.mem을 통해 load/store로 직접 접근 가능한 byte-addressable SSD. 전체를 host physical memory에 매핑하는 Type-3 HDM device로 노출 (p.1-2).
- **Coordinated context switch**: 긴 SSD 접근으로 막힌 thread를 OS와 SSD controller가 협력 감지하여 다른 thread로 전환해 CPU를 활용하는 메커니즘 (p.1, p.4).
- **NDR / SkyByte-Delay opcode**: No Data Response(slave-to-master) message. SkyByte가 reserved opcode를 활용해 긴 지연을 host에 알리는 새 opcode `SkyByte-Delay` 추가 (p.5, Fig.8).
- **SkyByte Long Delay Exception**: NDR 수신 후 해당 명령이 retire될 때 트리거되는 새 하드웨어 exception. x86 IDT에 handler 설치 (p.5).
- **Write log**: cacheline(64B) 단위 circular buffer. 모든 write를 flash 접근 없이 append하고 나중에 flush. two-level hash table로 색인 (p.6-7).
- **Data cache (read-write cache)**: page(4KB) 단위 read-write cache. read 시 spatial locality 활용 (p.6-7).
- **Log compaction**: 같은 page로의 write를 coalesce해 flash write traffic을 줄이는 background 작업. 평균 146 µs (p.7-8).
- **Adaptive page migration**: hot page를 host DRAM으로 promote해 SSD DRAM을 확장. PLB로 consistency 보장 (p.8).
- **PLB (Promotion Look-aside Buffer)**: root complex에 있는 64-entry 구조로 진행 중 page migration을 추적 (p.8).

## 강점 · 한계 · 열린 질문
**강점**
- OS와 하드웨어 co-design으로 CXL-SSD의 두 근본 문제(긴 지연 + granularity mismatch)를 동시에 공략. context switch 이점과 write log/migration 이점이 orthogonal함을 ablation으로 입증 (p.10).
- false-positive context switch를 retire-time exception으로 추가 비용 없이 제거하는 설계가 깔끔 (p.5).
- FPGA prototype으로 시뮬레이터 성능 모델을 실측 검증, ULL/SLC/MLC flash까지 sensitivity 분석 (p.9, p.12).
- 비용 효율(11.8배) 정량화로 실용성 강조 (p.10).

**한계 / 열린 질문**
- 평가가 PIN trace replay 기반 시뮬레이션 중심. 실제 OS 스케줄러·full system 상호작용의 완전한 영향은 prototype에서 제한적으로만 검증 (p.9).
- `SkyByte-Delay` opcode와 SkyByte Long Delay Exception은 CPU/CXL controller의 ISA·하드웨어 변경을 요구하여 실배포 장벽이 있다 (p.5).
- 기본적으로 data persistence를 가정하지 않으며 persistence는 page pin/`clwb`로만 보장 — pin된 page는 promote되지 않아 성능 trade-off 발생 (p.8).
- adaptive page migration의 access count threshold·PLB 용량(64 entry)이 워크로드별로 얼마나 robust한지 추가 분석 여지.

## ❓ Q&A (자가 점검)
> [!question]- Q1. CXL-SSD에서 기존 OS가 context switch를 못 하는 근본 이유는?
> load/store memory 명령이 host CPU에서 CXL protocol로 SSD에 직접 발행되어 OS가 이를 intercept할 수 없기 때문이다. 따라서 어느 명령이 긴 SSD 지연으로 막혔는지 OS가 알 수 없다 (p.1).

> [!question]- Q2. SkyByte는 긴 지연을 어떻게 host에 알리고 context switch를 트리거하는가?
> SSD가 cache miss 감지 시 CXL.mem NDR message에 새 opcode `SkyByte-Delay`를 실어 보낸다. host CXL controller가 이를 받아 MSHR과 cache 계층을 탐색, 해당 명령이 retire될 때 SkyByte Long Delay Exception을 트리거하고 exception handler가 CXL-aware scheduling policy로 context switch를 수행한다 (p.5).

> [!question]- Q3. retire stage까지 기다려 exception을 트리거하는 이유는?
> speculative load/store나 hardware prefetch는 결국 squash될 수 있으므로, retire 시점에 트리거하면 false-positive context switch를 추가 하드웨어 비용 없이 제거할 수 있다 (p.5).

> [!question]- Q4. SSD DRAM을 write log + data cache로 나눈 이유는?
> 대부분 워크로드가 page의 75% 이상에서 cacheline의 40% 미만만 접근하므로(p.2), 전체 page를 caching하면 공간 낭비와 write amplification이 발생한다. cacheline-granular write log는 (1) finer granularity로 SSD DRAM 공간을 절약하고 (2) 더 큰 write coalescing window를 제공해 flash write traffic을 줄인다 (p.6).

> [!question]- Q5. write log lookup이 빠른 이유와 two-level hash table의 역할은?
> hash table로 amortized O(1) lookup을 달성하되, plain hash table은 같은 page의 모든 cacheline을 찾을 때 여러 번 lookup이 필요하다. 1st level은 LPA로 색인, 2nd level은 page 내 offset별 cacheline을 추적해 compaction 시 같은 page의 cacheline을 한 번에 merge할 수 있다 (p.7).

> [!question]- Q6. context switch threshold는 어떻게 정하며 기본값은?
> flash channel queue 상태로 estimated latency를 계산(read/write/erase latency × 큐 내 요청 수)하여 threshold보다 크면 switch한다. flash page read latency(3 µs)가 regular context switch overhead(2 µs)보다 길므로 threshold를 2 µs로 설정했다 (p.6).

> [!question]- Q7. SkyByte-Full의 핵심 정량 성과 3가지는?
> Base-CSSD 대비 평균 6.11배 성능 향상, flash write traffic 23.08배 감소, ideal(무한 host DRAM) 성능의 75% 도달. 비용은 DRAM-only 대비 15.9배 저렴(11.8배 cost-effective) (p.1, p.10).

> [!question]- Q8. context switch와 write log/page migration 이점은 서로 어떤 관계인가?
> orthogonal하다. SkyByte-C의 Base-CSSD 대비 향상과 SkyByte-Full의 SkyByte-WP 대비 향상이 모두 평균 1.49배로, context switch 이점이 write log·migration 이점과 독립적임을 보인다(단 tpcc는 예외로 page migration이 지배) (p.10).

## 🔗 Connections
[[CXL]] · [[HPCA]] · [[2025]]
관련: [[XHarvest]], [[Smart-Infinity]], [[Sparse Checkpointing for Fast and Reliable MoE Training]]

## References worth following
- **ByteFS: System support for (cxl-based) memory-semantic solid-state drives** (S. Li et al., ASPLOS 2025) [ref 39] — SSD의 byte-accessibility를 OS·storage architecture 차원에서 지원, SkyByte와 직접 비교되는 핵심 선행/동시기 연구 (p.12).
- **Overcoming the memory wall with CXL-Enabled SSDs** (S-P. Yang et al., USENIX ATC 2023) [ref 62] — CXL을 SSD에 적용한 대표 선행 연구로 SkyByte가 출발점으로 삼는 CXL-SSD architecture (p.1-2).
- **TPP: Transparent page placement for cxl-enabled tiered-memory** (H. A. Maruf et al., ASPLOS 2023) [ref 43] — Linux NUMA balancing 기반 page migration. SkyByte-CT/WCT 비교 baseline (p.12).
- **AstriFlash: A flash-based cache for online services** (S. Gupta et al., HPCA 2023) [ref 23] — host DRAM을 SSD의 set-associative cache로, user-level thread switch로 I/O 숨김. SkyByte와의 차별점(SSD를 black box·page granularity로 취급) 강조 (p.12).
- **Informing memory operations** (M. Horowitz et al., ISCA 1996) [ref 25] — cache miss 같은 memory event를 software에 알리는 primitive. SkyByte의 SSD→CPU 알림 설계에 영감 (p.12).
- **FlatFlash: Exploiting the Byte-Accessibility of SSDs within a Unified Memory-Storage Hierarchy** (H. A. Abulila et al., ASPLOS 2019) [ref 7] — byte-accessible SSD와 host bridge promotion buffer, SkyByte의 migration consistency 설계 기반 (p.2, p.8).

## Personal annotations
<!-- 본인 메모 영역 -->
