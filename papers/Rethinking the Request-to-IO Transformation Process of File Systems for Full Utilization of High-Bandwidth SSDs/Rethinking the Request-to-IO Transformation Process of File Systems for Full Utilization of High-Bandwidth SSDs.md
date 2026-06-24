---
title: "Rethinking the Request-to-IO Transformation Process of File Systems for Full Utilization of High-Bandwidth SSDs"
aliases: [OrchFS, Rethinking Request-to-IO]
description: "고대역폭 SSD의 쓰기 비효율 3대 원인(page-alignment·page cache·IO 동시성)을 규명하고, 소용량 NVM을 보조로 활용해 요청을 SSD-page 정렬 IO와 잔여 NVM IO로 분할·오케스트레이션하는 이종-IO 파일시스템 OrchFS 제안."
venue: FAST
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/io-path
  - topic/filesystem
  - venue/fast
  - year/2025
---

# Rethinking the Request-to-IO Transformation Process of File Systems for Full Utilization of High-Bandwidth SSDs

> **FAST 2025** · `cluster/fs` · Source: [Rethinking the Request-to-IO Transformation Process of File Systems for Full Utilization of High-Bandwidth SSDs.pdf](<Rethinking the Request-to-IO Transformation Process of File Systems for Full Utilization of High-Bandwidth SSDs.pdf>)

저자: Yekang Zhan, Haichuan Hu, Xiangrui Yang, Qiang Cao (교신, Huazhong University of Science and Technology), Hong Jiang (University of Texas at Arlington), Shaohua Wang, Jie Yao (HUST)

## TL;DR
기존 SSD 파일시스템은 사용자 요청을 memory-page 정렬된 동질적 block IO(bio)로 변환하는 legacy 변환 과정 때문에 고대역폭 SSD의 쓰기 대역폭을 거의 못 살린다(EXT4/F2FS/XFS/BTRFS의 random write가 raw 대역폭의 1/3~1/4 수준). 저자들은 쓰기 비효율의 3대 원인 — (1) SSD-page alignment cost, (2) page caching overhead, (3) insufficient IO concurrency — 을 실험으로 규명하고, 소용량 NVM을 보조 장치로 두어 임의 패턴 쓰기를 "SSD-page 정렬 SSD-IO" + "잔여 비정렬 소형 NVM-IO"로 능동 분할(alignment-based write-partition)하고 triple-data-path로 병렬 처리하는 **OrchFS**를 제안한다. write latency에서 SSD 파일시스템 대비 최대 29.76×, 읽기에서 최대 6.79× 개선.

## 문제 & 동기
SSD의 용량·대역폭은 빠르게 증가(PCIe4.0 NVMe 약 read 5~7GB/s, write 3~6GB/s, PCIe5.0은 그 2배)하지만, 기존 SSD 파일시스템(EXT4/F2FS/XFS/BTRFS)은 random write에서 raw 대역폭의 5% 미만(OrchFS 대비, Fig.1), 일반적으로 1/3~1/4 수준만 달성한다. 이는 작은 쓰기 때문만이 아니다 — 1MB random large write조차 raw 대역폭의 1/2 미만(Fig.1b). 근본 원인은 파일시스템이 임의 offset/size의 요청을 memory-page(4KB) 정렬된 동질적 bio로 변환하는 legacy block stack에 있다. NVM은 byte-accessibility와 정렬 비용 없는 빠른 소형 영속 쓰기를 제공해 SSD와 상보적이라는 점이 핵심 insight.

> [!quote]- 📄 원문 표현 (paper)
> "existing SSD file systems that transform user requests to memory-page aligned homogeneous block IO have by and large failed to make full use of the superior write bandwidth of SSDs even for large writes. Our experimental analysis identifies three main root causes of this write inefficiency, namely, 1) SSD-page alignment cost, 2) page caching overhead, and 3) insufficient IO concurrency." (p.1, Abstract)
>
> "the increasingly high write performance of SSDs can be maximally utilized if all IOs to SSDs are fully aligned while NVM can fast absorb residual unaligned small IOs." (p.2, §1 insight)
>
> "proactively partitioning an any-pattern write to an SSD-preferred SSD-page aligned part and a residual, small and unaligned part, so that the former and latter are strategically and fast served by SSD and NVM respectively." (p.5, §2.5 design principle)

## 핵심 통찰

> [!note]- 통찰 토글
> - **3대 비효율 원인(실험으로 규명, §2.4)**:
>   - **Observation #1 (IO Alignment)**: SSD-page(예: 16KB) 비정렬 direct write는 read-modify-write(RMW)로 정렬 write보다 최대 10.71× 높은 지연. 16KB 비정렬은 인접 두 SSD-page를 읽어 정렬 비용이 가장 큼. 1MB large write조차 head/tail의 두 SSD-page를 읽어야 해 1.85× 높음. (p.3, Fig.3)
>   - **Observation #2 (Page Cache)**: buffered IO는 page caching software overhead(allocation, locking, LRU 관리, application↔cache 복사)로 비싸다 — 비정렬 9.5~56.0%, 정렬 15.9~65.8%. direct IO는 strict alignment 조건 하에서 이 overhead를 회피 가능. (p.4, Fig.4)
>   - **Observation #3 (IO Concurrency)**: 이상적 정렬 large write라도 single-thread는 SSD 대역폭의 약 89% 이상 영역을 못 채운다(내부 복잡 IO 스케줄링·fairness 때문). large IO를 다수의 32KB 정렬 IO로 split해 multi-thread 처리하면 write 1.02~1.56×, read 1.06~1.65× 향상. (p.4, Fig.5)
> - **핵심 결론**: 고대역폭 SSD의 최적 write path는 (1) SSD-page alignment, (2) direct IO mode, (3) explicit multi-threaded IO 세 조건을 동시 만족할 때만 가능. 임의 패턴 요청이 이 엄격한 조건을 동시에 만족하는 일은 드물다.
> - **상보성 insight**: NVM은 정렬 비용 없는 byte-addressable 소형 영속 쓰기에 강하므로, 임의 쓰기에서 비정렬 잔여 부분만 NVM이 흡수하면 SSD는 항상 최적 정렬 IO만 받게 된다. NVM은 대용량/고대역폭일 필요 없고 소용량 보조로 충분.
> - **기존 hybrid NVM-SSD와의 차이**: hierarchical(NVM을 SSD 캐시)은 상위 NVM이 부하를 떠안아 SSD를 underutilize. parallel(size-threshold로 분기)은 SSD 쓰기 비효율 자체를 무시. OrchFS는 "한 쓰기 내부"를 정렬/비정렬로 능동 분할한다는 점이 근본적으로 다름.

## 설계 / 메커니즘

> [!note]- 설계 토글
> OrchFS는 LibFS-KernelFS 구조(Strata/ArckFS 계열)를 확장한 SSD-NVM 이종-IO 오케스트레이션 파일시스템. 3대 설계 과제(C1: 임의 패턴 분할·매핑, C2: NVM/SSD 통합 인덱싱, C3: triple-path 오케스트레이션+일관성)를 해결. (Fig.6)
>
> - **Heterogeneous Data Layout (§3.3)**: 세 종류 storage unit.
>   - **SSD Block**: 기본 32KB(NVM page 크기의 정수배, 두 SSD-page). 파일 데이터의 대부분 저장.
>   - **NVM Page**: 4KB. 정렬된 부분 저장.
>   - **NVM Upage(unaligned page)**: 비정렬 잔여(chunk) 저장용. 56B header + 4KB - 8B 본문. header는 14 entry, 각 entry는 chunk의 page 내 offset(2B)+size(2B) 기록. metadata 영역(superblock, bitmap, inode table, HRtree, directory, journal)은 NVM에만 저장. (Fig.7)
>   - **Merge of NVM chunks**: 새 chunk 삽입 시 주소 겹치는 기존 chunk와 기회적 병합. Upage가 full-page로 채워지면 SSD block에 write back하고 Upage 회수.
> - **Block-Page-Aligned File Write Partition (§3.4)**: alignment-prioritizing(AP) + fragmentation-minimizing(FM) 정책. 임의 쓰기를 block-aligned SSD-IO + page-aligned NVM-IO + page-unaligned chunk-IO로 순차 분할(O(1)). Append write와 Overwrite를 구분(Algorithm 1). Overwrite는 세 case(NVM page 내 overwrite / SSD block 내 / 양쪽 걸침)로 처리. (Fig.8 예시: 154KB 파일에 89~194KB 쓰기를 7개 IO로 분할)
> - **Unified Per-File Mapping Structure — HRtree (§3.5)**: legacy radix tree 하단에 heterogeneous layer를 추가해 NVM/SSD 별도 인덱스를 병합. 두 종류 extended leaf node(ELN) — Append-write ELN(A-ELN, NVM page 위치), Overwrite ELN(O-ELN, SSD block + NVM page/Upage 위치). NVM/SSD 분리 인덱스의 복잡 병렬 검색·느린 직렬 검색 회피. B+tree처럼 leaf 연결로 range query 가속, readers-writer range lock으로 disjoint 영역 동시 접근. 메모리 상주(업데이트는 NVM journal). (Fig.9: 2176KB 파일 예시)
> - **Parallel IO Engine (§3.6)**: 전용 IO thread pool(기본 NVM 4개 + SSD 32개). 주소공간을 32KB 정렬 단위로 interleaved 분배, 각 thread는 exclusive address range를 ring buffer queue로 순차 처리 → application thread 의존 없이 explicit multi-threaded IO + data consistency 확보. SSD-write는 direct IO, SSD-read는 buffered(page cache 활용), NVM은 memory-semantic path. OdinFS의 spin-then-park 전략 상속. SSD 32 thread는 부하·CPU·대역폭 균형점.
> - **Data Migration (§3.7)**: NVM 공간 고사용/유휴 시 logical-block 단위로 NVM page/Upage를 SSD로 기회적 마이그레이션·병합(fragmentation·NVM usage 감소). HRtree range lock으로 fine-grained block-level migration → foreground 영향 최소.
> - **Implementation/Consistency (§4)**: NOVA/Strata/OdinFS/ArckFS/F2FS 참고하여 from scratch 구현. Linux radix tree 변형으로 HRtree. KernelFS가 LibFS의 metadata journal을 비동기 검증(ArckFS integrity verifier 유사). address-aligned shared memory로 direct IO write-buffer 정렬 보장(overhead <10%). 기본 atomic metadata-op(EXT4/NOVA relaxed/OdinFS/ArckFS와 동일 일관성), logical journaling으로 강화 가능. (GitHub: github.com/YekangZhan/OrchFS)

## 평가

> [!note]- 평가 토글
> - **환경(§5.1)**: 2× Intel Xeon Gold 6348(2.60GHz, 28코어), 256GB DDR4. NVM = 128GB Intel Optane PM. SSD = PCIe4.0 3.84TB Samsung PM9A3(read 6.8GB/s, write 4.2GB/s); PCIe5.0 실험은 3.84TB Samsung PM1743(read 14GB/s, write 6GB/s). Ubuntu 20.04, Linux 5.18.18. 비교군: SSD-FS(EXT4, F2FS), NVM-FS(NOVA, OdinFS, ArckFS), hybrid HFS(Strata, SPFS, PHFS). (p.10)
> - **종합 성능 주장(p.2)**: SSD-FS/NVM-FS/HFS 통틀어 write latency 최대 29.76×, read 최대 6.79× 개선. write latency 세부: EXT4/F2FS 대비 최대 29.76×, NOVA/OdinFS/ArckFS 대비 3.49×, Strata/SPFS/PHFS 대비 7.16×; peak throughput 3.08×/6.79×/6.34×.
> - **Single-thread write latency(§5.2.1, Fig.10)**: OrchFS는 baseline의 0.96~29.76×. 소형(<32KB)은 NVM에 전량 기록해 NVM-FS/HFS와 유사. large(>64KB)는 1.41~7.16× 개선. 32KB는 데이터의 2.4%, 64KB는 48.1%만 SSD로 가, SSD-FS 대비 4.94~8.89× 우위.
> - **Parallel IO engine 효과(§5.2.1, Fig.11b)**: 병렬 엔진 on/off로 write latency 최대 48.6% 개선(부작용 없이). SSD-IO thread 수는 32가 최적, 8개로도 거의 SSD 포화(Fig.12).
> - **Storage 공간 사용(Table 2)**: 1MB random write 시 데이터의 약 4.7%만 NVM으로 → SSD 대역폭 극대화, EXT4/F2FS(전량 SSD) 대비 latency 68.4%/72.4% 개선.
> - **Read throughput(§5.2.1, Fig.13)**: 1MB(±10%) case에서 OrchFS는 baseline 대비 최대 6.79× peak throughput, page cache 빠르게 활용. 1B-128KB(절반 NVM/절반 SSD) case 최대 6.33×.
> - **Multi-thread(§5.2.2, Fig.14/15)**: multiple files에서 1.00~7.84× aggregate throughput. single file 동시 접근에서 baseline 대비 1.86~7.84×, 2 user-thread에서 SSD 대역폭 포화. PHFS/SSD-FS는 단일 파일 concurrent fsync로 부진하나 OrchFS는 direct IO로 회피.
> - **Macrobenchmark Filebench(§5.3, Fig.16)**: single-thread에서 Fileserver/Webproxy/Varmail throughput 각 2.33~10.25×, 1.81~7.44×, 1.04~13.35× 우위. 16-thread에서 1.76~15.12×.
> - **Real-world(§5.4)**: YCSB+LevelDB(Fig.17) — Load에서 SSD-FS 대비 3.28~9.82×, NVM-FS 대비 1.3~1.43×. read-only RunC/RunE는 NVM-FS와 경쟁적(0.99~1.64×). GridGraph Pagerank(Fig.18, Livejournal/Twitter/Friendster) — baseline 대비 1.69~3.20×.
> - **Fragmentation/Aging(§5.5, Fig.19)**: SSD-side는 항상 32KB 정렬 배치로 fragmentation 없음. NVM Upage 비율은 쓰기량 증가에 따라 증가 후 감소(chunk가 full-page로 병합). data migration 활성 시 NVM page/Upage를 SSD로 점진 마이그레이션(1KB 극단 case도 12초 내 전량 이동). WineFS의 anti-aging 채택 가능.

## 섹션 노트
- §1 Introduction: 문제 정의 + insight + 3대 challenge(C1/C2/C3) + 기여 3개.
- §2 Background and Motivation: SSD/NVM 특성(§2.1), 파일시스템 legacy 변환(§2.2), SSD-FS write 성능 측정(§2.3), 쓰기 비효율 분석=Observation #1/#2/#3(§2.4), design principle 도출(§2.5).
- §3 OrchFS Design: 설계 과제(§3.1), overview(§3.2), heterogeneous data layout(§3.3), write-partition+Algorithm 1(§3.4), HRtree(§3.5), parallel IO engine(§3.6), data migration(§3.7).
- §4 Implementation: 프로토타입, LibFS-KernelFS metadata 업데이트, sharing, crash consistency(logical journaling).
- §5 Evaluation: setup(§5.1), microbenchmark(§5.2), macrobenchmark Filebench(§5.3), real-world YCSB/GridGraph(§5.4), fragmentation·aging(§5.5).
- §6 Related Work: SSD-FS, NVM-FS, hybrid NVM-SSD, multiple IO paths, alignment optimization(NVStore/Re-aligning/WAFLASH/iBridge/WineFS와 차별).
- §7 Conclusion + Artifact Appendix(github.com/YekangZhan/OrchFS).

## 핵심 용어
- **Request-to-IO transformation**: 파일시스템이 임의 offset/size 사용자 요청을 장치가 허용하는(전통적으로 memory-page 정렬·동질적) block IO(bio)로 변환하는 과정.
- **SSD-page alignment cost**: SSD-page(예: 16KB) 경계에 정렬되지 않은 쓰기가 read-modify-write를 강제당해 발생하는 추가 read latency·IO amplification.
- **RMW (Read-Modify-Write)**: 비정렬 쓰기를 정렬하기 위해 해당 page를 먼저 읽고→수정→다시 쓰는 과정. host(파일시스템) 측과 SSD 내부 양쪽에서 발생 가능.
- **Alignment-based write-partition (AP/FM policy)**: 임의 쓰기를 block-aligned SSD-IO + page-aligned NVM-IO + page-unaligned chunk-IO로 분할. AP=alignment-prioritizing, FM=fragmentation-minimizing.
- **NVM Upage (unaligned page)**: 비정렬 잔여 chunk를 header(14 entry)와 함께 저장하는 NVM 4KB page.
- **chunk**: 한 logical page의 비정렬 잔여 데이터 조각. Upage에 저장되며 주소 겹치는 chunk끼리 병합.
- **HRtree (Heterogeneous Radix tree)**: legacy radix tree 하단에 NVM/SSD 통합 인덱싱 layer(A-ELN/O-ELN)를 추가한 per-file 통합 매핑 구조.
- **ELN (Extended Leaf Node)**: HRtree의 heterogeneous leaf. A-ELN(append-write, NVM page 위치), O-ELN(overwrite, SSD block+NVM page/Upage 위치).
- **Parallel IO engine**: 전용 IO thread pool(NVM 4 + SSD 32)로 주소공간을 32KB 정렬 interleaved 분배해 explicit multi-threaded IO와 consistency를 확보하는 엔진.
- **triple-data-path**: 최적 SSD-write path(direct IO) + 전통 SSD-read path(buffered, page cache) + memory-semantic NVM path.
- **LibFS-KernelFS architecture**: userspace LibFS가 데이터 path를, 신뢰 KernelFS가 metadata 검증·전역 상태를 담당하는 분리 구조(Strata/ArckFS 계열).

## 강점 · 한계 · 열린 질문
- **강점**: 쓰기 비효율의 3대 원인을 정량 실험으로 분리 규명(Observation #1~3)한 점이 설득력 있음. "한 쓰기 내부를 정렬/비정렬로 분할"한다는 발상이 기존 size-threshold hybrid와 명확히 차별화. 소용량 NVM(전체의 ~4.7%만 사용)으로 SSD를 최적 path에 유지 → 비용효율. write 29.76×, read 6.79× 등 광범위 baseline 대비 큰 폭 개선, artifact 공개(USENIX Artifact Available/Evaluated).
- **한계**: Intel Optane PM에 의존하는데 Optane은 단종(논문도 §2.1에서 언급); RRAM/PCRAM/MRAM/memory-semantic SSD로 대체 가능하다 주장하나 실측은 Optane만. SSD가 NVM보다 빠르거나 NVM이 매우 작을 때의 성능은 미검증(저자는 결론 불변이라 주장). crash consistency 기본 모드는 atomic data-op 미보장(relaxed). small-large 경계(32KB)·SSD block 크기 등 다수 파라미터가 SSD 세대/특성 의존.
- **열린 질문**: Optane 부재 시대에 어떤 NVM/CXL-attached memory가 동등 효과를 낼까? CXL memory의 더 높은 지연이 비정렬 흡수 효과를 상쇄하지 않는가? read-heavy/range workload에서 NVM/SSD 분산 데이터의 read 손실(page cache 미적용)을 더 줄일 수 있나? 32KB SSD block·thread 수의 자동 튜닝 가능성은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 고대역폭 SSD에서 기존 파일시스템의 쓰기 비효율 3대 근본 원인은?
> 답: (1) SSD-page alignment cost — 비정렬 쓰기의 RMW로 최대 10.71× 지연(Observation #1), (2) page caching overhead — buffered IO의 software overhead 9.5~65.8%(Observation #2), (3) insufficient IO concurrency — single-thread가 SSD 대역폭의 ~89% 영역을 못 채움(Observation #3).

> [!question]- Q2. OrchFS의 핵심 design principle(insight)은 무엇인가?
> 답: SSD는 모든 IO가 완전히 정렬될 때 고대역폭을 최대 활용할 수 있고, NVM은 정렬 비용 없이 잔여 비정렬 소형 IO를 빠르게 흡수한다. 따라서 임의 패턴 쓰기를 "SSD-선호 정렬 부분"과 "잔여 비정렬 소형 부분"으로 능동 분할해 각각 SSD와 NVM이 전략적으로 처리한다. NVM은 대용량/고대역폭이 아니라 소용량 보조로 충분.

> [!question]- Q3. write-partition은 임의 쓰기를 어떤 세 종류 IO로 나누는가?
> 답: block-aligned SSD-IO(SSD block, 기본 32KB), page-aligned NVM-IO(NVM page, 4KB), page-unaligned chunk-IO(NVM Upage에 저장). alignment-prioritizing + fragmentation-minimizing 정책으로 O(1) 분할.

> [!question]- Q4. HRtree가 기존 hybrid 파일시스템의 인덱싱과 다른 점은?
> 답: 기존은 NVM/SSD에 별도 인덱스를 두어 복잡한 병렬 검색이나 느린 직렬 검색이 필요. HRtree는 legacy radix tree 하단에 heterogeneous layer(A-ELN/O-ELN)를 추가해 하나의 per-file 트리로 NVM page/Upage와 SSD block을 통합 매핑하고, leaf 연결로 range query를 가속한다.

> [!question]- Q5. parallel IO engine은 IO concurrency 문제를 어떻게 해결하는가?
> 답: 전용 IO thread pool(NVM 4 + SSD 32)을 두고 주소공간을 32KB 정렬 단위로 interleaved 분배. 각 thread가 exclusive address range를 ring buffer queue로 순차 처리하여, application thread에 의존하지 않고 large IO를 다수의 32KB 정렬 IO로 split·multi-thread 실행. write latency 최대 48.6% 개선.

> [!question]- Q6. OrchFS는 read와 write에서 각각 어떤 data path를 쓰는가?
> 답: SSD-write는 direct IO(정렬 보장 하에 page cache overhead 회피), SSD-read는 buffered IO(SSD-page 정렬 write 덕에 page cache가 자연히 이득), NVM은 memory-semantic path. 이를 triple-data-path라 부른다.

> [!question]- Q7. 1MB random write에서 NVM은 데이터의 얼마를 흡수하며 그 효과는?
> 답: 약 4.7%만 NVM으로 가고 나머지는 SSD에 정렬 기록. 그 결과 전량 SSD를 쓰는 EXT4/F2FS 대비 write latency를 각각 68.4%/72.4% 개선(Table 2).

> [!question]- Q8. OrchFS의 주요 의존성·한계는?
> 답: Intel Optane PM(단종) 기반 NVM에 실측 의존(RRAM/PCRAM/MRAM/memory-semantic SSD로 대체 가능하다 주장하나 미실측). 기본 일관성 모드는 atomic metadata-op만 보장(data-op 비보장, EXT4/NOVA relaxed 동일). 32KB 경계·SSD block 크기 등 파라미터가 SSD 특성에 의존.

## 🔗 Connections
[[File System]] · [[FAST]] · [[2025]]

## References worth following
- Strata: A cross media file system (SOSP 2017) — LibFS-KernelFS 이종 매체 파일시스템 원형.
- ArckFS: Enabling high-performance and secure userspace nvm file systems with the trio architecture (SOSP 2023) — OrchFS의 보안·trio 구조 기반.
- OdinFS: Scaling pm performance with opportunistic delegation (OSDI 2022) — parallel IO engine의 delegation·spin-then-park 전략 출처.
- NOVA: A log-structured file system for hybrid volatile/non-volatile main memories (FAST 2016) — 대표 in-kernel NVM 파일시스템 비교군.
- Orthus(orthus): The storage hierarchy is not a hierarchy (FAST 2021) — NVM-SSD 계층을 캐시가 아닌 병렬로 보는 관점.
- WineFS: a hugepage-aware file system for persistent memory that ages gracefully (SOSP 2021) — NVM-side anti-aging 기법, OrchFS가 채택 가능.

## Personal annotations
<본인 메모 영역>
