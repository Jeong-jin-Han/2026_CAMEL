---
title: "ScaleSwap: A Scalable OS Swap System for All-Flash Swap Arrays"
aliases: [ScaleSwap]
description: "다수 core·SSD 환경에서 OS swap을 core-centric(1코어-1자원)으로 재설계해 LRU/swap metadata lock contention을 제거하고 all-flash swap array 대역폭을 끌어내는 시스템"
venue: FAST
year: 2026
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/swap
  - topic/os
  - topic/all-flash
  - venue/fast
  - year/2026
---

# ScaleSwap: A Scalable OS Swap System for All-Flash Swap Arrays

> **FAST 2026** · `cluster/fs` · Source: [ScaleSwap - A Scalable OS Swap System for All-Flash Swap Arrays.pdf](ScaleSwap%20-%20A%20Scalable%20OS%20Swap%20System%20for%20All-Flash%20Swap%20Arrays.pdf)
>
> 저자: Taehwan Ahn, Chanhyeong Yu, Sangjin Lee, Yongseok Son (Chung-Ang University, Systems and Storage Laboratory). 교신저자: Yongseok Son.

## TL;DR
Linux의 기존 swap system은 모든 core가 모든 swap/memory 자원에 접근하는 **all-to-all 모델**이라, core/SSD 수가 늘어도 `lru_lock`·`si_lock` 같은 중앙 lock 경합 때문에 all-flash swap array의 대역폭을 못 쓴다. ScaleSwap은 이를 **core-centric(one-core-to-one-resource)** 모델로 재설계한다. 세 가지 전략: (1) **core-centric swap resource management**(각 core가 자기 swap metadata/cache/space 전용 관리), (2) **opportunistic inter-core swap assistance**(필요 시 다른 core에 메타데이터 접근을 위임), (3) **core-affinity page & per-core LRU management**(per-core LRU로 `lru_lock` 경합 완화). Linux kernel 6.6.8에 구현, 128-core + 8 NVMe SSD all-flash swap array에서 최대 **3.4배 throughput**, **11.5배 낮은 평균 latency**, 선행 연구 TMO 대비 최대 64%, ExtMEM 대비 최대 5배 우위.

## 문제 & 동기
- OS swap은 anonymous page를 회수해 OOM으로 인한 application 실패를 막는 핵심 시스템 컴포넌트. ML/big-data/graph/VM/container 등 memory-intensive 워크로드가 수 TB 메모리를 요구하면서 swap에 대한 압박이 커지고 있다(p.1).
- DRAM 증설은 TCO 측면에서 비싸다(Meta 서버 비용의 37%, Microsoft Azure의 50%가 메모리). DDR4 DRAM 단가 $4.22/GB는 PCIe 4.0 NVMe SSD $0.16/GB의 약 26배. 따라서 여러 NVMe SSD를 묶은 **all-flash swap array**가 capacity·cost 양쪽에서 실용적 대안(p.2).
- 그러나 기존 Linux swap의 striping 기법은 **all-to-all 모델**이라 SSD/core 수가 늘어도 확장되지 않는다. Figure 1은 8 SSD/128 core 환경에서 raw device는 near-linear scaling하지만 Linux swap은 약 4 GB/s에서 정체됨을 보임(p.2).
- 두 가지 root cause: ① per-node **LRU list lock(`lru_lock`)** 경합(swap out/in 시 LRU에서 fetch/insert), ② per-swap-space **swap metadata lock(`si_lock`)** 경합(global locking으로 모든 core가 swap_info 공유)(p.6).

> [!quote]- 📄 원문 표현 (paper)
> "It is because the swap system adopts an all-to-all model, where each core can access any swap and memory resources through centralized management and locking mechanisms. Although it allows shared access across all cores, it incurs high lock contention on many cores and underutilizes the high bandwidth of all-flash swap arrays, especially when accessing shared resources such as 1) swap metadata and 2) least recently used (LRU) cache." (p.2)
>
> "Insight #1: As the number of cores per node increases, lock contention on the LRU list significantly increases. This decelerates the swap out/in process." (p.6)
>
> "Insight #2: High lock contention on swap metadata hampers the scalability of the swap system on many cores, thereby limiting SSD scalability." (p.6)

## 핵심 통찰
> [!note]- 핵심 통찰 (토글)
> - **all-to-all → one-to-one 전환이 핵심**: swap 자원(metadata, cache, space)과 swap in/out 연산을 가능한 한 core 단위로 "orchestrate"하면 중앙 lock 경합 없이 all-flash array 대역폭을 풀어낼 수 있다(p.3).
> - **두 병목은 lock**, I/O가 아니다. Performance breakdown(Table 5)에서 Linux swap 실행시간의 **53.27%가 `lru_lock`**에 소비됨. ScaleSwap은 이를 0.16%까지 낮춤(p.10).
> - **순수 분리만으로는 부족하다**: shared page·process migration·swap space full 같은 불가피한 cross-core 상황이 존재 → "opportunistic" 위임으로 memory consistency를 깨지 않으면서 협력. 위임은 메모리 연산만 포함(평균 29.99 ns)이라 I/O가 없어 저렴(p.8).
> - **core-affinity로 locality 확보**: page를 할당한 core 번호를 page flag의 미사용 비트에 기록해, 그 core의 per-core LRU에만 들어가고 나가게 함 → LRU lock 경합 완화 + page fault 9.15% 감소(p.9, p.12).

## 설계 / 메커니즘
> [!abstract]- 설계 / 메커니즘 (토글)
> **전체 구조 (Figure 5, p.6)**: 각 core가 자기 swap resource(swap delegator, swap cache, swap metadata, task queue, LRU list, swap space)를 전용 보유하는 decentralized core-centric 아키텍처.
>
> **전략 1 — Core-centric swap resource management (§4.4)**
> - 각 core가 자기 swap metadata/cache/space를 배타적으로 관리하는 one(core)-to-one(resource) 모델. Linux의 round-robin striping은 swap space 선택 시 `si_lock` 경합을 유발(Figure 6a)하지만, ScaleSwap은 각 core가 자기 swap info의 cluster를 직접 접근해 lock 없이 swap slot을 채움(Figure 6b, p.7).
> - **Core-dedicated swap space 확장**: Linux는 swap entry type field가 5 bits라 최대 23개 swap space만 지원(Figure 7). ScaleSwap은 type field를 3 bits 추가(총 8 bits)해 **최대 247개** dedicated swap space를 지원하고, offset은 47 bits로 per-core 최대 **128 TB**까지 표현(p.8).
>
> **전략 2 — Opportunistic inter-core swap assistance (§4.5)**
> - 두 경우 위임 발생: ① swap-out 시 자기 dedicated swap space가 full, ② swap-in 시 page가 다른 core의 swap space에 있음(shared page·migration).
> - **per-core delegator**가 위임 요청을 받아 해당 core의 swap metadata 접근/갱신을 단일 consumer로 처리 → metadata에 대한 lock 자체를 제거. 요청 thread는 데이터(page)를 예정된 swap location에 직접 read/write(Figure 8, p.8).
> - **swap task queue**: 96-byte swap task 구조, concurrent list로 구현, configurable pre-allocated pool로 high memory pressure 하 할당 실패 방지. 단일 delegator(consumer)가 FIFO로 처리해 ordering·consistency 보장(p.8).
> - **Cooperative swapping**: 위임은 I/O가 없어(평균 29.99 ns) thread가 busy-wait. delegator가 sleep이면 대기 길어짐 → 요청 thread가 자기 core task queue의 task를 대신 처리해 CPU 낭비/context switch 방지(p.9).
> - **NUMA-aware**: 같은 node 내 core를 round-robin으로 먼저 탐색, 없으면 다음 node로(p.8).
>
> **전략 3 — Core-affinity page & per-core LRU (§4.6)**
> - **per-core shared LRU list**, 각각 별도 per-core spinlock 보호 → `lru_lock` 중앙 경합 제거.
> - page flag의 미사용 4 bits를 3 bits 확장해 **core number(7 bits = 128 core 지원)** 기록. node bits는 10→7로 축소(64 node 지원). page는 할당 core의 LRU에만 insert/evict → core/cache locality 강화, page fault 감소(p.9).
>
> **ScaleSwap Procedure (§4.7, Figure 10/11)**
> - **위임 없는 swap-out**: thread가 자기 LRU에서 page 선택 → swap slot으로 swap entry 획득 → swap map 갱신 → swap cache에 insert 후 flush → PTE를 SWE로 교체(Figure 10a, p.9).
> - **위임 swap-out**: swap space full 시 다른 core task queue에 task enqueue + delegator wake-up. delegator가 metadata 갱신/cache insert 후 completion signal; 대기 중 요청 thread는 자기 task queue를 처리(cooperative)(Figure 11a, p.10).
> - **Consistency**: swap-in의 비동기성으로 일시적으로 공간이 "있다"고 보고할 수 있으나(보수적), 없는데 있다고 보고하는 일은 없음 → Linux swap과 동일 수준 consistency 유지(p.11).

## 평가
> [!example]- 평가 (토글)
> **환경**: 128-core(AMD EPYC 7713 ×2), 96 GB DRAM, 8× FireCuda 530 NVMe SSD(각 2 TB). raw RAID-0 집계 mixed R/W throughput 11.4 GB/s. Ubuntu 22.04 LTS, Linux kernel 6.6.8. 128 swap files on EXT4. micro-benchmark는 stress tool, 실세계는 6개 memory-intensive application(p.10).
>
> **Micro-benchmark (128 thread, 8 SSD, thread당 2.25 GB R/W = 총 288 GB)**:
> - SSD scalability(Figure 12a): SSD 1→8 증가 시 Linux 대비 최대 **1.31×/2.34×/2.85×/3.41×** throughput. 실행시간 1.55×/1.94×/2.29×/2.49× 단축(p.10).
> - Core scalability(Figure 12b): ScaleSwap은 linear scaling, Linux는 32 core 초과 시 정체(p.10).
> - Latency(Figure 12c/d, Table 1): 평균 최대 **11.5×**, tail(99.9th) 최대 **27.2×** 낮음. Table 1 tail 예: 8 SSD에서 Linux 2395.20 µs vs ScaleSwap 87.94 µs(p.10).
> - CPU utilization(Table 2): kernel(system) CPU 평균 25% 감소, idle time 최대 16% 증가(p.11~12).
>
> **Memory consumption (Table 3, p.12)**: per-core 1,500 swap task(각 96 byte) preallocate, 128 thread 시 총 task 192,000개 ≈ 17.57 MB. 실행 중 peak 6.0~13.7 MB. peak memory는 Linux와 거의 동일(총 367.79 vs 367.78 GB).
>
> **Page fault (p.12)**: Linux 100,827,862 → ScaleSwap 92,280,466 (**9.15% 감소**), per-core LRU localization 덕분.
>
> **End-to-end apps (Figure 13, p.12)**: 8 SSD에서 BFS 2.4×, DNA visualization 2.57×, Python list 1.70×, gray-scale 1.72×, flip 1.91× throughput 향상. DNA visualization이 가장 큰 gain(가장 빈번한 memory access).
>
> **Apache Spark (Figure 14, §5.4, p.12~13)**: Common Crawl(CC-MAIN-2025-13) 128 WARC 파일. 100,000 records/core 워크로드에서 6.3 GB/s, Linux 대비 **1.75×** throughput.
>
> **선행 연구 비교 (§5.5)**: TMO 대비 최대 **64%** 우위(Figure 15, compression ratio 높을수록 격차 확대 — TMO의 압축/해제 CPU overhead). ExtMEM 대비 8 SSD에서 최대 **5.02×** ops/s(Figure 16, ExtMEM은 32 core 초과 시 정체)(p.13).
>
> **Performance breakdown (Table 5, p.13~14)**: Linux는 `lru_lock`이 실행시간의 **53.27%**(최대 병목). ScaleSwap은 core-affinity+per-core LRU로 `lru_lock` 0.16%까지 낮추고, 병목이 `si_lock`으로 이동했으나 core-centric+위임으로 그것도 0%로 제거.
>
> **Delegation overhead (Table 6, §5.7, p.14)**: dedicated swap file을 0/16/32/64/96개까지 강제로 소진시켜도 peak throughput의 약 **84% 유지**, 실행시간은 52→63초로 소폭 증가. 위임이 메모리 연산만 포함하기 때문.

## 섹션 노트
- §1 Introduction: OS swap의 중요성, all-to-all 모델 한계, Alibaba/Meta의 swap 재조명(p.1).
- §2 Motivation: emerging memory architecture(disaggregated/tiered, CXL/PM)와의 관계, all-flash swap array 필요성, TCO 논거, Google Cloud/Solidigm 사례, Apache Spark 사례 연구(OOM 발생)(p.2~3).
- §3 Scalability Analysis: swap space 구성(partition/file/device) 비교(파일 채택), SSD/core scalability 측정, direct reclaim 시 latency spike, root cause 2개(LRU lock, swap metadata lock)(p.3~6).
- §4 Design: 3대 design goal, 3대 strategy, 전체 아키텍처(Figure 5), swap entry layout 확장(Figure 7), opportunistic 위임(Figure 8), page flag layout(Figure 9), procedure(Figure 10/11)(p.6~10).
- §5 Evaluation: micro-benchmark, end-to-end, Spark, TMO/ExtMEM 비교, breakdown, delegation overhead(p.10~14).
- §6 Related Work: all-flash array scaling(ScaleCache, Falcon), disaggregated(Infiniswap, AIFM, TeRM), tiered(NOMAD, Colloid), scaling memory(Hydra, scalable range lock, ExtMEM), OS swap(FlashVM, SSDAlloc, TMO, ZNSwap)(p.14).
- §7 Conclusion + Acknowledgments: NRF(No. RS-2025-00554650) 지원. Source open: https://github.com/syslab-CAU/ScaleSwap (p.14).

## 핵심 용어
- **All-flash swap array**: 여러 NVMe SSD를 묶어 swap space로 쓰는 구성. capacity 확대 + TCO 절감 + sharp performance degradation 방지 목적.
- **All-to-all model**: 모든 core가 모든 swap/memory 자원에 중앙 lock 통해 접근하는 기존 Linux swap 모델. 공유성은 좋으나 many-core에서 lock 경합 심각.
- **Core-centric (one-to-one) model**: 각 core가 자기 swap 자원을 배타·독립 관리하는 ScaleSwap의 핵심 모델.
- **`lru_lock`**: per-node LRU list를 보호하는 lock. swap out/in 시 LRU fetch/insert에서 경합 발생(Linux 실행시간 53.27% 소비).
- **`si_lock` (swap info lock)**: per-swap-space swap metadata(`swap_info`)를 보호하는 spinlock. 다중 swap space여도 global locking이라 동시성 제한.
- **Swap metadata (`swap_info`)**: swap map(page의 swap 내 위치), `inuse_pages`, first/last empty bit 등을 담는 구조. swap entry는 cluster(=512 entry) 단위로 관리.
- **Opportunistic inter-core swap assistance**: swap space full/shared page 같은 불가피한 cross-core 상황에서 per-core delegator에게 metadata 접근을 위임하는 메커니즘.
- **Per-core delegator**: 각 core의 swap metadata를 단일 consumer로 갱신해 metadata lock을 제거하는 thread. task queue(FIFO)로 ordering 보장.
- **Cooperative swapping**: delegator 응답 대기 중 요청 thread가 자기 core task queue의 task를 대신 처리해 CPU 낭비/context switch를 피하는 기법.
- **Core-affinity page allocation**: page를 할당한 core 번호를 page flag(7 bits 확장)에 기록해, 그 core의 per-core LRU에만 들고 나게 하는 기법.

## 강점 · 한계 · 열린 질문
- **강점**: 병목을 lock(특히 `lru_lock` 53.27%)으로 정량 규명하고, all-to-all→core-centric이라는 근본적 재설계로 제거. 실제 Linux kernel 6.6.8 구현 + open source + Artifact Evaluated(Available/Functional/Reproduced). micro/end-to-end/Spark/선행 연구 비교까지 폭넓은 평가. memory overhead 미미(<18 MB), consistency 동일 수준 유지.
- **한계**: per-core 전용 자원 분할이라 swap space full 시 위임이 불가피 — 평가에서 84% throughput은 유지하나 강제 소진 시 실행시간 증가. swap entry type field 확장(247 space)·core number(7 bit=128 core)·node(7 bit=64 node)로 인한 하드코딩된 상한 존재. 96 GB DRAM 단일 머신·8 SSD 규모 평가로, 더 큰 array/노드에서의 일반화는 미검증.
- **열린 질문**: shared page가 매우 빈번한 워크로드에서 위임 비율이 높아지면 busy-wait/cooperative 비용이 어떻게 변하나? CXL/disaggregated memory tier와 결합 시(논문이 last memory tier로 언급) 동일 core-centric 원리가 유효한가? per-core LRU가 global page aging 정책 정확도에 주는 영향은?

## ❓ Q&A (자가 점검)
> [!question]- Q1. 기존 Linux swap이 all-flash swap array에서 확장되지 않는 근본 원인은?
> 답: all-to-all 모델 때문. 모든 core가 모든 swap/memory 자원을 중앙 lock으로 공유해, core/SSD가 늘면 `lru_lock`(per-node LRU)과 `si_lock`(swap metadata) 경합이 폭증. Figure 1처럼 raw device는 선형 확장하나 Linux swap은 ~4 GB/s에서 정체(p.2).

> [!question]- Q2. ScaleSwap의 세 가지 핵심 전략은?
> 답: ① core-centric swap resource management(각 core가 자기 swap metadata/cache/space 배타 관리, one-to-one 모델), ② opportunistic inter-core swap assistance(per-core delegator로 cross-core metadata 접근 위임), ③ core-affinity page & per-core LRU management(per-core LRU로 `lru_lock` 경합 완화)(p.3).

> [!question]- Q3. per-core delegator는 어떻게 swap metadata lock을 없애나?
> 답: 각 core의 swap metadata를 그 core의 delegator라는 단일 consumer만 갱신하게 함. 다른 core thread는 metadata를 직접 건드리지 않고 task queue에 위임 요청을 넣고, 데이터(page)는 예정된 location에 직접 read/write. 단일 consumer가 FIFO 처리하므로 lock 없이 ordering·consistency 보장(p.8).

> [!question]- Q4. 위임(delegation)이 성능에 큰 부담이 되지 않는 이유는?
> 답: 위임은 metadata 접근/갱신 같은 **메모리 연산만** 포함하고 I/O는 없어 평균 **29.99 ns**로 매우 짧다. 또 cooperative swapping으로 대기 thread가 놀지 않고 자기 task queue를 처리. swap file을 96개까지 강제 소진해도 peak throughput의 84% 유지(Table 6, p.14).

> [!question]- Q5. core-affinity page allocation은 어떻게 구현되며 무슨 효과가 있나?
> 답: page flag의 미사용 비트를 3 bits 확장해 할당 core 번호(7 bits, 128 core)를 기록. 해당 page는 그 core의 per-core LRU에만 insert/evict되어 core/cache locality가 좋아지고 `lru_lock` 경합이 줄며, page fault가 9.15% 감소(p.9, p.12).

> [!question]- Q6. swap entry/page flag layout을 왜 수정했나?
> 답: Linux swap entry의 type field는 5 bits라 최대 23개 swap space만 지원 → many-core에 부족. ScaleSwap은 type을 3 bits 더해 8 bits(247 space), offset 47 bits(per-core 128 TB)로 확장. page flag는 core number(7 bits) 기록을 위해 node bits를 10→7로 축소(64 node 지원)(p.8~9).

> [!question]- Q7. 핵심 수치 요약은?
> 답: Linux swap 대비 최대 3.4× throughput, 11.5× 낮은 평균 latency, 27.2× 낮은 tail latency, page fault 9.15% 감소, kernel CPU 평균 25% 절감. 선행 연구 대비 TMO 최대 64%, ExtMEM 최대 5.02× 우위. `lru_lock` 점유율 53.27%→0.16%(p.10~14).

> [!question]- Q8. ScaleSwap은 consistency를 어떻게 보장하나?
> 답: 단일 delegator가 core별 metadata를 FIFO로 갱신하므로 cross-core 충돌이 없다. swap-in의 비동기성으로 공간을 보수적으로(있는데 없다고는 가능, 없는데 있다고는 불가) 보고할 수 있어 Linux swap과 동일한 consistency 수준 유지(p.8, p.11).

## 🔗 Connections
[[File System]] · [[FAST]] · [[2026]]

## References worth following
- [11] S. Bergman et al. **ZNSwap: un-block your swap.** USENIX ATC 2022. — ZNS SSD용 swap subsystem, host-side GC와 co-design.
- [88] **TMO: Transparent memory offloading.** — PSI 기반 실시간 모니터링으로 이기종 디바이스 offload량 조절(직접 비교 대상).
- [41] S. Jalalian et al. **ExtMEM: Enabling Application-Aware virtual memory management.** USENIX ATC 2024. — user-space로 page 관리 정책 위임(직접 비교 대상).
- [74] **FlashVM.** — flash 기반 VM swap, GC를 flash-specific 기법으로 최적화(OS-based swap 계보).
- [47] A. Kogan et al. **Scalable range locks for scalable address spaces and beyond.** EuroSys 2020. — non-conflicting 주소공간 연산용 scalable lock(task queue concurrent list 근거).
- [36] J. Gu et al. **Efficient memory disaggregation with Infiniswap.** NSDI 2017. — RDMA 기반 remote paging(disaggregated swap 비교 맥락).

## Personal annotations
<본인 메모 영역>
