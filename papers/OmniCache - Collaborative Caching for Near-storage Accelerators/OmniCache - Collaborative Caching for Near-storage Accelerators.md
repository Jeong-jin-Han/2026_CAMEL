---
title: "OmniCache: Collaborative Caching for Near-storage Accelerators"
aliases: [OmniCache]
description: "호스트 캐시와 디바이스 캐시를 협업적으로 활용하는 near-cache I/O 원리로 근접 저장 가속기의 I/O와 데이터 처리를 가속하는 캐싱 설계"
venue: FAST
year: 2024
tier: deep
status: done
tags:
  - paper
  - cluster/isc
  - topic/near-storage
  - topic/caching
  - venue/fast
  - year/2024
---

# OmniCache: Collaborative Caching for Near-storage Accelerators

> **FAST 2024** · `cluster/isc` · Source: [OmniCache - Collaborative Caching for Near-storage Accelerators.pdf](<OmniCache - Collaborative Caching for Near-storage Accelerators.pdf>)

**저자**: Jian Zhang, Yujie Ren (Rutgers University); Marie Nguyen (Samsung); Changwoo Min (Igalia); Sudarsun Kannan (Rutgers University)

## TL;DR
근접 저장 가속기(near-storage accelerator)의 host 캐시(HostCache)와 device 캐시(DevCache)를 협업적으로 사용해, 요청된 데이터에 가장 가까운 캐시로 I/O와 데이터 처리를 수행하는 "near-cache" 원리를 제안한다. 확장 가능한 인덱스(OmniIndex), 동시 I/O를 위한 collaborative caching, 모델 기반 동적 오프로딩(OmniDynamic), CXL.mem 확장(OmniCXL)을 결합해 I/O는 최대 3.24x, 데이터 처리는 최대 3.06x, 실제 애플리케이션(KNN)은 최대 5.15x의 성능 향상을 달성한다.

## 문제 & 동기
근접 저장 장치(CSD)는 ARM/RISC-V 코어, FPGA, DRAM 등 연산 자원을 내장해 데이터 이동을 줄일 잠재력을 가지지만, 디바이스 메모리는 host RAM보다 작고 대역폭이 제한적이다. 기존 near-storage 설계는 (1) 디바이스 레벨 캐싱이 없거나 host 캐시와 협업하지 못하고(INSIDER, CrossFS, FusionFS), (2) host-only 캐싱은 OS 캐시의 coarse-grained inode 락과 eviction stall로 동시성 한계가 있으며(λ-IO), (3) 단순한 메트릭(연산량)만으로 오프로딩을 결정해 데이터 분포·이동·큐잉 비용을 무시한다. 그 결과 1KB 요청에 4KB 블록을 가져오는 등 불필요한 데이터 이동과 stall이 발생한다.

> [!quote]- 📄 원문 표현 (paper)
> "The absence of caching support or the failure to exploit near-storage memory for I/O and data processing increases storage access and data movement between the host and the device (e.g., fetching a 4KB block for a 1KB application request). Similarly, the absence of a collaborative host and device memory caching causes applications to stall due to cache eviction delays. Finally, prior designs use simplistic metrics to offload data processing (e.g., computing power) without considering storage-centric metrics ... leading to suboptimal performance." (p.1)
>
> "We observe that for 10 million keys and a 4KB value size (fillrandom and readrandom), over 99.99% of the 141 million total I/O requests were unaligned." (p.4, RocksDB 분석)

## 핵심 통찰

> [!note]- 통찰 토글
> - **Near-cache I/O**: 데이터를 host로 다 올리지 말고, 가장 가까운 캐시(HostCache 또는 DevCache)에서 처리하라. 1KB 요청에는 4KB 블록 전체가 아니라 요청된 바이트만 이동해 데이터 이동·write amplification을 제거.
> - **Horizontal(collaborative) paradigm vs hierarchical/tiering**: 계층적 캐시는 eviction이 끝날 때까지 스레드가 대기해야 하지만, OmniCache는 host와 device 캐시를 동시에 갱신·접근하게 해 application stall을 줄인다.
> - **Exclusive caching**: HostCache·DevCache·storage에 데이터를 배타적으로 저장해 캐시 커버리지를 높이고, inclusive cache의 일관성 통신 비용을 회피. 크기가 크게 다른 두 캐시(큰 HostCache, 작은 DevCache)의 eviction 빈도를 독립적으로 조절 가능.
> - **Host가 인덱스를 단독 관리(OmniIndex)**: 디바이스 CPU는 인덱스를 건드리지 않아 host-device 간 일관성/통신 오버헤드를 없애고 host의 멀티코어 병렬성을 활용.
> - **Storage-centric 모델 기반 오프로딩**: 연산량만이 아니라 데이터 분포 비율(R), 실행 시간(E), 데이터 전송 비용(B), 큐 지연(Q)을 함께 고려해 처리 위치(host/device)를 동적으로 결정.

## 설계 / 메커니즘

> [!note]- 설계 토글
> **3대 컴포넌트** (Figure 2, p.6)
> - **OmniLib (host, user-level)**: POSIX I/O를 가로채 per-file I/O 큐와 NVMe-like 명령으로 변환. HostCache 관리, eviction 결정, 모델 기반 오프로딩 엔진, read-checksum-write 같은 확장 처리 인터페이스 제공. jemalloc 확장으로 캐시 블록 할당.
> - **OmniIndex (user-level)**: per-file range tree(interval tree) 인덱스. 각 노드가 파일의 특정 range/segment를 HostCache(blue)·DevCache(green)·storage에 매핑. fine-grained range lock으로 비충돌 블록을 host·device에서 동시 접근. dirty bit + bitmap으로 dirty 데이터 추적. range-level ownership 모델로 충돌 방지(한 엔티티만 수정). 1TB 파일에 약 128MB(<0.001%) 메모리, 노드당 256바이트.
> - **OmniDev (device)**: near-storage 파일시스템(FusionFS 유사) + 데이터 처리 엔진 + DevCache 관리. 경량 bitmap 기반 메모리 할당기가 block ID를 반환(메모리 주소 노출 안 함). I/O 큐에서 요청을 꺼내 처리하고 NVMe 명령을 갱신, completion flag 설정. 권한 검사는 OS가 부여한 per-file 큐 credential로 수행.
>
> **I/O 연산** (§4.2.2, p.7-8)
> - Write: HostCache에 먼저 쓰고 OmniIndex 노드 갱신(dirty bit). eviction depth를 줄이려 최대 range(default 2MB)로 병합.
> - Overwrite: 캐시에 있으면 갱신+dirty 표시. cache miss 시 전체 4KB 블록을 host로 가져오지 않고 관련 블록만 DevCache로 읽어 직접 갱신 → write amplification 회피.
> - Read: OmniIndex로 위치 파악 후 HostCache/DevCache/storage에서 읽기. prefetch granularity 최대 2MB. 여러 노드에 걸친 블록은 fine-grained lock으로 동시 읽기.
> - fsync: range·per-block dirty bit로 블록 커밋. barrier로 취급.
> - **Two-step LRU eviction**: (1) file-level LRU + per-file range LRU(30초 epoch). HostCache가 가득 차면 DevCache로 쓰며 동시 eviction 시작. (2) free 캐시가 10% 미만이면 range-level LRU eviction; DevCache는 NVMe-like eviction 명령으로 OmniDev가 처리.
>
> **Collaborative processing (OmniDynamic, §4.4, p.8-9)**
> - read-cal_distance-nearestK(KNN) 같은 복합 연산을 host·device에서 동시 실행, intermediate 결과는 processing buffer에 저장.
> - 모델(Equation 1): T_h, T_d = 각각 host/device 처리 시간을 데이터 비율 R(R_hm, R_dm, R_s), 실행 시간 E(Eh_avg, Ed_avg), 전송 대역폭 B(B_hm_dm, B_ds_hm, B_ds_dm), 큐 지연(Cmd_avg × Q_len)로 계산해 T_h ≤ T_d면 host, 아니면 device에서 처리(Algorithm 1).
> - 데이터가 한쪽에만 있으면 그곳에서 처리(near-data 선호), 분산되어 있으면 더 작은 데이터를 가진 쪽으로 이동.
>
> **OmniCXL (§4.5, p.9)**: CXL.mem으로 DevCache를 추가 NUMA 노드로 매핑. OmniLib가 디바이스 메모리를 직접 load/store 접근(range ownership 획득 후), NVMe 큐의 packing/copy/queuing/polling 오버헤드 제거.
>
> **구현(§5, p.9)**: OmniDev를 디바이스 드라이버로 에뮬레이션(8K LOC, 프로그래머블 저장 HW 부재 때문). near-storage 백엔드 2종(Intel Optane PM, NVMe SSD+ext4 OS 캐시 우회). 디바이스 메모리는 원격 NUMA 노드로 매핑, thermal throttling/frequency scaling으로 느린 디바이스 에뮬레이션.

## 평가

> [!note]- 평가 토글
> **환경**: dual-socket 64-core 2.7GHz Intel Xeon Gold, 32GB DRAM, 512GB(4×128GB) Optane PM(App-Direct), 512GB NVMe SSD. DevCache 4GB DRAM + OmniDev용 4 CPU. 비교군: NOVA(커널 PM FS), FusionFS(캐싱 없음), HostCache-user-level, lambda-IO-emulate(OS 캐싱). (p.10)
>
> **I/O 성능 (§6.2, p.10-11)**:
> - Microbenchmark(64GB workload, 1KB I/O, private files): non-block-aligned 접근에서 OmniCache가 일관되게 우위. sequential/random에서 FusionFS·HostCache-user-level 대비 각각 최대 **2.53x, 1.52x** 향상(Figure 4).
> - File sharing(16 reader/16 writer, 공유 64GB): readers 2323 MB/s, writers 1732 MB/s로 FusionFS(978/523) 대비 큰 향상(Table 3).
> - I/O size study(1K~5K): non-block-aligned에서 near-cache I/O 이점 일관(Figure 5).
> - Cache ratio 25%~100%: 낮은 비율에서도 DevCache 전환으로 stall 회피(Figure 6).
>
> **데이터 처리 (§6.3, p.11-12)**: read-CRC-write(checksum) 워크로드. OmniCache-dynamic이 FusionFS(요청마다 device 오프로딩 + 데이터 복사)와 HostCache-user-level(cache miss 시 host로 데이터 이동) 대비 우위. 레이턴시는 HostCache-user-level 대비 최대 **1.42x** 낮고 더 안정적(Figure 7). 전체 데이터 처리 최대 **3.06x** 향상.
>
> **OmniDynamic 모델 효과 (§6.4, p.12)**: data_move → exe_cost → queue_delay를 점진 추가하면 성능 개선(Figure 8). 느린 device CPU(1.2GHz)·낮은 메모리 대역폭(16GB/s, 8x 감소)에서도 host로 동적 분산해 HostCache 대비 최대 **1.22x** 향상. 디바이스 자원이 나빠질수록 host에서 실행되는 연산 비율 증가(Table 4: in-host 12.52M→15.24M, in-device 4.19M→834.32K).
>
> **CXL.mem (§6.5, p.13)**: OmniCXL이 DevCache를 CPU load/store로 직접 접근해 데이터 복사·polling 비용 제거 → HostCache-user-level 대비 **2.76x**, NVMe 큐 기반 OmniCache 대비 **1.21x** 향상(Figure 10).
>
> **실제 애플리케이션 (§6.6, p.13)**:
> - LevelDB(YCSB A-F, 512B value, 40M keys, 32 threads, 11 LOC 변경): write-intensive A/F에서 HostCache-user-level 대비 최대 우위, FusionFS 대비 db_bench 최대 **1.92x**(Figure 11a).
> - KNN(DiskANN류, 128GB workload > 캐시): host+device 협업 read-cal_distance-predict로 FusionFS 대비 최대 **5.15x**(Figure 11b).

## 섹션 노트
- §1 Introduction: near-cache I/O, collaborative caching, dynamic offloading, CXL 확장의 4대 기여 제시.
- §2 Background/Motivation: Table 1(기존 시스템 비교: direct-I/O, host/device cache, 동시 처리, dynamic offload, CXL 지원). Table 2(unaligned I/O 비율: RocksDB 99.99%, MySQL 93.32%, DiskANN 95.89%). Figure 1(NOVA/FusionFS/HostCache-user-level 대비 동기 분석).
- §3 Goals & Overview: design goals + Figure 2 컴포넌트 흐름.
- §4 Design: Cache architecture(host userspace 캐싱, exclusive device cache), OmniIndex, I/O 연산, collaborative processing, OmniDynamic 모델, OmniCXL.
- §5 Implementation: 에뮬레이션 상세.
- §6 Evaluation: I/O, 데이터 처리, 모델, CXL, 실제 앱.
- §7 Conclusion.

## 핵심 용어
- **near-cache I/O**: 요청 데이터에 가장 가까운 캐시(HostCache/DevCache)에서 I/O·처리를 수행하고, 블록 정렬 없이 요청된 바이트만 이동해 데이터 이동을 최소화하는 원리.
- **HostCache / DevCache**: host의 user-level 캐시(큰 용량, 많은 CPU)와 device near-storage 캐시(저장소에 가까움). 둘을 협업적으로 사용.
- **OmniIndex**: host가 단독 관리하는 per-file range tree(interval tree) 인덱스. fine-grained range lock과 dirty bit로 동시 접근·일관성 관리.
- **Collaborative caching**: 계층적 tiering 대신 host·device 캐시를 동시에 갱신/접근하는 horizontal paradigm으로 eviction stall 감소.
- **Exclusive caching**: 데이터를 HostCache/DevCache/storage 중 한 곳에만 보관해 커버리지를 높이고 일관성 통신 비용을 회피.
- **OmniDynamic**: 데이터 비율(R)·실행시간(E)·전송비용(B)·큐 지연(Q)을 고려해 처리 위치를 동적으로 정하는 모델 기반 오프로딩(Equation 1, Algorithm 1).
- **CISC operation**: I/O와 데이터 처리를 합쳐 NVMe 명령 벡터로 묶어 오프로딩하는 추상화(FusionFS에서 차용·확장).
- **OmniCXL**: CXL.mem으로 DevCache를 NUMA 노드로 매핑해 host CPU가 직접 load/store, NVMe 큐 오버헤드 제거.
- **Two-step LRU eviction**: file-level + range-level(per-file) LRU의 2단계 eviction으로 동시 eviction과 stall 감소.

## 강점 · 한계 · 열린 질문
- **강점**: host·device 캐시를 모두 활용하는 첫 협업 캐싱; unaligned I/O가 지배적인 실제 워크로드(99%+)에 잘 맞는 near-cache I/O; storage-centric 모델로 단순 연산량 기반 오프로딩 한계 극복; CXL.mem까지 확장성 입증; 최대 5.15x의 실측 향상.
- **한계**: 프로그래머블 저장 HW 부재로 OmniDev를 디바이스 드라이버로 **에뮬레이션**(원격 NUMA + throttling) — 실 HW 성능 미검증; user-level direct-access FS의 공통 취약점(쓰기 권한 프로세스가 공유 OmniIndex를 corrupt 가능) 존재; 본 연구는 단일 멀티스레드 앱에 집중(inter-process sharing 최적화는 future work); OmniDynamic 모델의 평균 기반 파라미터(Eh_avg 등)가 변동성 큰 워크로드에서 정확도 한계 가능.
- **열린 질문**: 실제 CSD HW(ScaleFlux/Newport)에서의 재현성? OmniIndex의 huge page/더 큰 range 적용 시 메모리·성능 트레이드오프? multi-tenant·다중 프로세스 공유에서의 보안·격리 비용?

## ❓ Q&A (자가 점검)

> [!question]- Q1. OmniCache의 핵심 "near-cache I/O" 원리는 기존 설계와 무엇이 다른가?
> 답: 요청 데이터를 host로 전부 올리지 않고 가장 가까운 캐시(HostCache/DevCache)에서 처리하며, 블록 정렬 없이 요청된 바이트만 이동한다. 1KB 요청에 4KB 블록 전체를 가져오던 기존 방식의 데이터 이동·write amplification을 제거한다.

> [!question]- Q2. 왜 계층적(tiering) 캐싱 대신 horizontal collaborative 캐싱을 택했나?
> 답: 계층적 캐시는 HostCache eviction이 끝날 때까지 스레드가 대기해야 한다. OmniCache는 host와 device 캐시를 동시에 갱신·접근(비충돌 블록은 fine-grained range lock)해 eviction으로 인한 application stall을 줄인다.

> [!question]- Q3. OmniIndex를 host가 단독으로 관리하는 이유는?
> 답: 디바이스 CPU가 인덱스를 건드리지 않게 해 host-device 간 일관성/통신 오버헤드를 없애고, host의 멀티코어 병렬성으로 동시 lookup을 빠르게 한다. 메모리 오버헤드도 1TB 파일에 ~128MB(<0.001%)로 작다.

> [!question]- Q4. exclusive caching을 inclusive 대신 쓰는 근거 두 가지는?
> 답: (1) 중복 저장이 없어 캐시 커버리지가 커진다. (2) inclusive 일관성 유지에 드는 host-device 통신 비용을 피하고, 크기가 크게 다른 HostCache·DevCache의 eviction 빈도를 독립적으로 조절할 수 있다.

> [!question]- Q5. OmniDynamic은 어떤 메트릭으로 처리 위치를 결정하나?
> 답: 데이터 분포 비율 R(host/device/storage), 실행 시간 E, 데이터 전송 대역폭 B, 큐 지연(Cmd_avg×Q_len)을 Equation 1로 합쳐 T_h와 T_d를 계산하고, T_h ≤ T_d면 host, 아니면 device에서 처리한다(near-data 선호).

> [!question]- Q6. CXL.mem(OmniCXL)이 NVMe 큐 기반 설계 대비 어떤 오버헤드를 줄이나?
> 답: DevCache를 NUMA 노드로 매핑해 host CPU가 직접 load/store로 접근하므로, NVMe 명령의 packing/copy, queuing delay, completion polling 비용을 제거한다. OmniCache 대비 1.21x, HostCache-user-level 대비 2.76x 향상.

> [!question]- Q7. 평가의 가장 큰 방법론적 한계는?
> 답: 프로그래머블 저장 HW가 없어 OmniDev를 디바이스 드라이버로 에뮬레이션했다(디바이스 메모리를 원격 NUMA 노드로 매핑, thermal throttling/frequency scaling으로 느린 디바이스 모사). 실제 CSD HW에서의 성능은 검증되지 않았다.

> [!question]- Q8. 실제 워크로드에서 near-cache I/O가 효과적이라는 근거 수치는?
> 답: RocksDB는 1410만 중 99.99%, MySQL 93.32%, DiskANN 95.89%의 I/O 요청이 unaligned(non-block-aligned)였다(Table 2). 따라서 블록 정렬 없이 요청 바이트만 이동하는 near-cache I/O가 데이터 이동을 크게 줄인다.

## 🔗 Connections
[[In-Storage Computing]] · [[FAST]] · [[2024]]

## References worth following
- **FusionFS** (FAST '22, [9]): CISCOps abstraction과 firmware file system. OmniCache가 인터페이스·near-storage FS를 차용·확장한 직접적 기반.
- **λ-IO** (FAST '23, [44]): eBPF 기반 host/device 통합 IO 스택. host-only 캐싱·dynamic offloading의 비교 대상(한계 분석의 핵심).
- **CrossFS** (OSDI '20, [31]): cross-layered direct-access file system, near-storage 인덱싱. host 분담 설계의 선행 연구.
- **INSIDER** (ATC '19, [33]): FPGA 기반 CSD에 block 인터페이스로 연산 오프로딩. Table 1 비교군.
- **KEVIN** (OSDI '21, [24]) / **PINK** (ATC '20, [17]): in-storage 인덱싱·KV store. near-storage 데이터 처리 계열.
- **Pond** (ASPLOS '23, [26]): CXL 기반 메모리 풀링. OmniCXL의 CXL 동향 맥락.

## Personal annotations
<본인 메모 영역>
