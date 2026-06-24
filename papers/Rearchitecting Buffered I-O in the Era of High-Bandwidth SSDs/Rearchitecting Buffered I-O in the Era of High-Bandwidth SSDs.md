---
title: "Rearchitecting Buffered I/O in the Era of High-Bandwidth SSDs"
aliases: [Rearchitecting Buffered I/O, WSBuffer]
description: "고대역폭 SSD에서 buffered I/O의 쓰기 병목을 scrap buffer로 재설계한 WSBuffer 제안"
venue: FAST
year: 2026
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/buffered-io
  - topic/io-path
  - topic/page-cache
  - venue/fast
  - year/2026
---

# Rearchitecting Buffered I/O in the Era of High-Bandwidth SSDs

> **FAST 2026** · `cluster/fs` · Source: [Rearchitecting Buffered I-O in the Era of High-Bandwidth SSDs.pdf](Rearchitecting%20Buffered%20I-O%20in%20the%20Era%20of%20High-Bandwidth%20SSDs.pdf)

**저자**: Yekang Zhan, Tianze Wang, Zheng Peng, Haichuan Hu, Jiahao Wu, Xiangrui Yang, Qiang Cao (교신, Huazhong University of Science and Technology), Hong Jiang (University of Texas at Arlington), Jie Yao (HUST)

## TL;DR
고대역폭 NVMe SSD 시대에 page cache 기반 buffered I/O의 **쓰기 경로**가 (1) 모든 쓰기를 critical path에서 buffering, (2) page 관리의 낮은 concurrency, (3) partial-page write의 read-before-write penalty 때문에 SSD 대역폭을 살리지 못한다. 저자들은 **WSBuffer (Write-Scrap Buffering)** 를 제안해, page cache의 쓰기 처리를 떼어내 **scrap buffer**로 작은/비정렬 쓰기만 merge-friendly하게 버퍼링하고 크고 정렬된 쓰기는 SSD로 직접 보내며, **OTflush(2단계 비동기 flushing)** 와 **concurrent page management**로 dirty 데이터를 빠르게 비운다. XFS 위에 약 4,500 LoC로 구현, EXT4/F2FS/BTRFS/XFS 및 ScaleCache 대비 throughput 최대 **3.91×**, latency 최대 **82.80×** 개선.

## 문제 & 동기
- NVMe SSD 대역폭이 PCIe3.0의 3GB/s 미만에서 PCIe5.0의 10GB/s 이상으로 급증하며 memory-SSD 대역폭 격차가 한 자릿수 배수로 좁혀졌다 (§2.1).
- buffered I/O의 **읽기**는 page cache 덕에 여전히 유리하지만 **쓰기**는 세 가지 병목을 가진다:
  - **C1 (과도한 page caching)**: 모든 incoming write를 critical path에서 버퍼링 → page 관리 비용(allocation, state 유지, LRU)이 좁아진 대역폭 이득을 상쇄. 공급 메모리가 written data의 70%일 때 100% 대비 throughput이 최대 **54.0%** 낮음 (Fig.2, §2.3.2).
  - **C2 (낮은 concurrency)**: page cache의 XArray가 non-scalable spinlock(`xa_lock`)으로 보호 → 고강도 쓰기 시 free page 삽입, clean page 삭제, page-state 갱신(tag `dirty`→`writeback`→`clean`) 간 심한 lock contention (§2.3.2).
  - **C3 (비싼 partial-page write)**: page cache miss된 partial write는 page fault → 느린 SSD-read로 채운 뒤 갱신(read-before-write). partial-page write latency가 대응 full-page write의 **1.51×–84.37×** (Fig.3, §2.3.3).
- 기존 해법은 두 갈래: page cache 최적화(ScaleCache, StreamCache, Uncached buffered I/O) — full buffering 구조 유지로 C1 미해결 / page cache 우회(direct I/O, AutoIO, OrchFS, SPDK) — buffered I/O의 장점·legacy 호환성 포기 (§2.4).

> [!quote]- 📄 원문 표현 (paper)
> "the existing buffered I/O architecture fails to effectively utilize high-bandwidth Solid-State Drives (SSDs) caused by 1) costly page caching overused for buffering all incoming writes in the critical path, 2) the limited concurrency of page management operations, and 3) the high read-before-write penalty for partial-page writes." (Abstract, p.1)
>
> "the bandwidth advantage of the memory over SSDs is insufficient to offset, much less overshadow, the cost of page cache management ... the management cost of page cache generally was ignored in the slow-storage era." (§2.3.1, p.3, cases 1.10×–4.46×)

## 핵심 통찰

> [!note]- 핵심 통찰 (펼치기)
> 1. **쓰기를 critical path에서 빼라**: page cache는 읽기에 강하고 쓰기에 약하다. buffered I/O의 진짜 병목은 "모든 쓰기를 buffer에 담는" full-buffering 설계 그 자체. 큰/정렬 쓰기는 SSD로 직접 보내고 page cache는 읽기 캐싱에만 집중시키면 SSD 대역폭과 메모리 이점을 동시에 누린다.
> 2. **작은 쓰기만 모아서 정렬하라**: read-before-write penalty(C3)는 작은 partial write에서만 발생. 전용 **scrap buffer**로 작은/비정렬 쓰기를 모아 merge하면 read-before-write를 비동기로 미루고(OTflush) penalty를 숨길 수 있다.
> 3. **clean/dirty page 분리로 lock을 풀어라**: read-only memory-page(clean)는 XArray, dirty scrap-page는 별도 SXArray + per-page lock으로 관리 → page-state 유지에 필요한 다중 lock 획득 제거(C2 완화).
> 4. **idle SSD를 기회적으로 활용하라**: RAID 내 SSD들의 busyness(Bcount)를 감지해 바쁜 SSD로의 flushing을 피하고 idle SSD를 골라 read-before-write/writeback 수행.

## 설계 / 메커니즘

> [!abstract]- WSBuffer 아키텍처 & 4대 메커니즘 (펼치기)
> **전체 구조 (§3.1, Fig.4)**: POSIX 앱 ↔ buffered I/O 인터페이스 (변경 없음) ↔ WSBuffer. 구성요소 = scrap buffer(쓰기) + page cache(읽기 캐싱) + OTflush(2단계 flushing) + concurrent page management. 4가지 enabling technique:
>
> **1) Scrap Buffer (§3.2, Fig.5)** — 작은/partial 쓰기 전용 메모리 구조.
> - **scrap-page** = data-zone(page 데이터) + 128B header(4B counter, 1B number, 2B SSD-id, 1B tag, 15×8B index entry로 data-segment의 offset/size 기록). 평가에서 95%+ 케이스가 segment 15개 미만.
> - **data-zone 크기 = 채널 수 × SSD-page 크기**의 배수. 평가 SSD(8채널, 16KB SSD-page) 기준 기본 **256KB (2×128KB)**. SSD 내부 parallelism 극대화 + fragmentation 방지.
> - **할당**: 4KB page 단위 할당으로 인한 fragmentation을 피해, 한 번에 **32개 scrap-page를 batch** 할당(4KB header 영역 + 8MB data-zone 영역으로 분리 배치).
> - **scrap buffer write**: 도착 순서가 아니라 offset 기준으로 해당 scrap-page에 read-before-write 없이 직접 쓰고, 겹치는 data-segment와 merge → page가 full이 되면 tag 갱신 후 OTflush 큐에 삽입.
>
> **2) Buffer-Minimized Data Access (§3.3, Algorithm 1, Fig.6)** — 쓰기를 scrap buffer/page cache/SSD에 분배.
> - **쓰기**: request size < threshold(기본 **1MB**)이면 전부 scrap buffer로 흡수. 크면 partial-scrap-page 부분 / scrap-page 정렬 부분으로 분할 → 정렬 부분은 SSD에 직접(C1 해결), partial 부분만 scrap buffer로. 정렬 granularity는 scrap-page-data-zone 크기(예 256KB)라 모든 SSD-write가 그 배수 → fragmentation 저항 + OTflush 친화.
> - SSD-write 완료 후 겹치던 obsolete scrap/memory-page는 background에서 reclaim.
> - **읽기**: scrap buffer를 먼저 조회(최신 데이터 보장) → 나머지는 page cache/SSD에서 읽음 → page cache 읽기 최적화(folio 등) 그대로 활용.
> - **데이터 일관성**: dirty scrap-page와 clean memory-page를 분리. scrap-page만 최신, memory-page는 항상 clean(읽기 캐싱용).
>
> **3) Opportunistic Two-stage Flushing — OTflush (§3.4, Algorithm 2, Fig.6)**
> - **SSD busyness 인지**: per-SSD **Bcount**(submit 시 +I/O크기, 완료 시 −I/O크기)로 대역폭 사용 감지. 임계값 기본 **4MB**(4MB write로 SSD 대역폭 거의 포화). 바쁜 SSD로의 flushing 회피.
> - **Stage-1 (Queue-1, read)**: partial-page write 때문에 unfilled된 scrap-page를 SSD에서 read-before-write로 채움. SSD가 idle이면 SSD-read 후 tag 갱신하고 Stage-2 큐로, 바쁘면 Queue-1 끝에 재삽입. full page는 read 없이 바로 Stage-2로.
> - **Stage-2 (Queue-2, writeback)**: full scrap-page를 SSD에 writeback. SSD-id=0(미할당)이면 delayed allocation으로 가장 덜 바쁜 SSD 선택. 완료 후 scrap-page reclaim. scrap-page 간 의존성 없어 Queue-1/2를 sub-queue로 쪼개 thread 할당 → concurrent flushing(기본은 Stage-1/2 각 1 thread).
>
> **4) Concurrent Page Management (§3.5, Fig.7)**
> - **memory-page(read-only)**: 기존 XArray 유지. clean only라 page-state 유지 불필요 → page-fault tree 갱신이 해당 thread만 막음.
> - **scrap-page**: 변형 XArray인 **SXArray** — 삽입/삭제만 관리하고 state는 안 맡음. 삭제는 index entry를 NULL로 바꾸는 index-entry-level lock(경량), tree 갱신은 기회적(load 낮거나 file close 시).
> - **per-scrap-page lock**: scrap buffer write와 OTflush Stage-1이 SXArray lock 없이 per-page lock으로 동시 수행. Stage-2만 index-entry-level lock 획득.
>
> **구현 (§3.6)**: XFS(Linux kernel 6.8) 위 약 **4,500 LoC** 커널 모듈. fsync()는 scrap-page만 우선 flush 후 dedicated thread가 처리. durability/crash consistency는 하부 FS의 journaling에 위임. XFS 외 FS에도 이식 가능.

## 평가

> [!success]- 평가 결과 (펼치기, 수치+p.X)
> **환경 (§4.1, p.8)**: 2× Intel Xeon Gold 6348(2.60GHz, 28코어), 256GB DDR4. 8× PCIe4.0 Samsung 990 PRO SSD(read 7GB/s, write 6.9GB/s)를 mdRAID RAID0(stripe 512KB)로 구성. Ubuntu 22.04, kernel 6.8(ScaleCache만 5.4). baseline = EXT4, F2FS, BTRFS, XFS, ScaleCache-XFS(SOTA buffered I/O 최적화), AutoIO(userspace 재구현, hybrid I/O). 5회 이상 평균.
>
> **Microbenchmark (§4.2)**:
> - full-page write latency: baseline 대비 **1.03×–3.29×** 개선. <1MB는 scrap buffer batch 할당·data layout 덕, ≥1MB는 SSD 직접 전송으로 1.14×–3.29× (Fig.8a, p.7).
> - partial-page write latency: **1.70×–82.80×** 개선. <1MB는 read-before-write 제거로 2.11×–82.80×; 1/2/4MB는 각각 32.4%/82.3%/91.1% 데이터를 SSD 직접 write하며 1.70×–4.06× (Fig.8b, p.7).
> - direct I/O/hybrid 비교: WSBuffer가 XFS-direct I/O·XFS-AutoIO 대비 **1.59×–231.28×** (RMW 회피, Fig.9, p.8).
> - multi-threaded: FIO random write(4KB~4MB) throughput **1.21×–3.91×**; Filebench read/write=1/2 throughput **1.19×–2.51×** (Fig.10, p.8). peak throughput은 전체 SSD 대역폭(~55GB/s)과 unaligned 부분의 memory copy에 제약.
>
> **Macrobenchmark (Filebench, §4.3)**:
> - 충분 메모리: Fileserver(write-heavy) **1.23×–2.51×**, Webproxy(read-heavy) **1.08×–1.65×**, Varmail(metadata, fsync 시간 60%+) **1.06×–2.84×** (Fig.11, p.9).
> - 제한 메모리: Fileserver **1.23×–4.48×**, Webproxy **1.07×–4.37×** throughput (Fig.12, p.9). 메모리 공급 늘수록 OTflush가 따라잡으며 성능 향상.
>
> **Real-world apps (§4.4)**:
> - KV store(LevelDB+YCSB, SSTable 64MB): RunA(50/50)·RunF(read-modify-write) **1.32×–2.02×** (Fig.13, p.9).
> - Graph(GridGraph, PageRank, LiveJournal/Twitter/Friendster): **1.09×–4.37×** OPS (Fig.14, p.10).
> - HPC(Nek5000, CFD, 585GB write, 256GB 메모리): throughput **1.74×–3.09×** (Fig.15, p.10).
>
> **CPU/Memory 분석 (§4.5–4.6)**: WSBuffer가 baseline 대비 CPU 사용률 **3.2%–28.4% 낮음**(80%+ 데이터를 SSD 직접 전송 → DMA로 CPU 절감, Table 3, p.10). real-world에서 foreground write memory 소비 LiveJournal 0.59% / Twitter 1.45% / Friendster 0.34% / Nek5000 1.67%로 극히 적음(Table 5, p.11).
>
> **Sensitivity (§4.7, Fig.16)**: 성능 이득이 RAID stripe 크기에 무관, SSD 개수 증가 시 throughput 향상(XFS는 SSD 증가 이득 못 봄).

## 섹션 노트
- **§1 Introduction**: full buffering이 write critical path에 buffer를 두어 SSD 대역폭 활용을 막음 → WSBuffer로 재설계 동기.
- **§2 Background/Motivation**: §2.1 SSD 대역폭 추세, §2.2 buffered vs direct I/O, §2.3 buffered write 3대 병목 실험(C1/C2/C3), §2.4 기존 해법 분류와 설계 목표 G1(buffered write 최소화)/G2(fluent dirty flushing)/G3(efficient partial-page write).
- **§3 Design**: §3.1 overview, §3.2 scrap buffer, §3.3 buffer-minimized data access(Algorithm 1), §3.4 OTflush(Algorithm 2), §3.5 concurrent page management(SXArray + per-page lock), §3.6 구현/durability/이식성.
- **§4 Evaluation**: micro/macro/real-world/CPU/memory/sensitivity.
- **§5 Related Works**: storage I/O stack 최적화(Falcon, AIOS, λ-IO), SSD 성능 proactive 활용(NHC, OrchFS, SPDK, GPU-centric), SSD용 cache system(AFCM, P2Cache, CSAL).
- **§6 Conclusion**: buffered I/O 실패 원인 규명 + WSBuffer로 더 높은 성능·적은 메모리·낮은 CPU.

## 핵심 용어
- **Buffered I/O**: page cache를 거쳐 user read/write를 흡수·merge·정렬·prefetch하는 기본 I/O 모드. 사용자 친화적이나 쓰기 critical path에 buffer가 끼어 고대역폭 SSD를 못 살림.
- **Direct I/O**: page cache를 우회해 user buffer와 SSD 간 직접 전송. SSD 대역폭은 살리나 strict alignment(offset/size/buffer 주소) 요구로 프로그래밍 복잡.
- **WSBuffer (Write-Scrap Buffering)**: 본 논문의 buffered I/O 재설계. 쓰기 처리를 page cache에서 떼어내 scrap buffer로 옮긴 구조.
- **Scrap buffer / scrap-page**: 작은·비정렬 쓰기를 merge-friendly하게 모으는 메모리 구조. scrap-page = data-zone + 128B header(index entry로 data-segment 관리).
- **Data-segment / data-zone**: scrap-page 내 유효 데이터 조각(segment)과 그것을 담는 영역(zone, 기본 256KB = 채널수×SSD-page의 배수).
- **Read-before-write (RMW)**: partial-page write 시 page를 채우려 느린 SSD-read를 먼저 하는 penalty. C3의 원인.
- **OTflush (Opportunistic Two-stage Flushing)**: Stage-1(SSD-read로 scrap-page 채움) + Stage-2(full page writeback)의 2단계 비동기 flushing. Bcount로 idle SSD 선택.
- **Bcount**: per-SSD 미완료 I/O 바이트 누적치. SSD busyness 추정용(임계 4MB).
- **SXArray**: scrap-page 인덱싱용 변형 XArray. 삽입/삭제만 관리, page-state 미관리, index-entry-level lock + 기회적 tree 갱신으로 경량화.
- **Per-scrap-page lock**: scrap buffer write와 OTflush Stage-1을 SXArray lock 없이 동시 수행하게 하는 fine-grained lock.

## 강점 · 한계 · 열린 질문
**강점**
- buffered I/O 인터페이스/legacy 코드 무변경으로 SOTA direct I/O 수준 쓰기 성능 + 읽기·메모리 이점 유지.
- C1/C2/C3 병목을 실험으로 정량 규명한 뒤 각 병목에 1:1 대응하는 메커니즘 설계(논리적 일관성).
- CPU·메모리까지 동시 절감(80%+ DMA 직접 전송), 기존 page cache 최적화(ccXArray, folio)와 직교/통합 가능.

**한계**
- XFS 위 4,500 LoC 커널 모듈 — 다른 FS 이식에 "engineering effort" 필요, AutoIO는 공정성 위해 userspace로 재구현(완전 동일 비교 아님).
- peak throughput이 전체 SSD 대역폭(~55GB/s)과 unaligned 부분의 memory copy에 제약 → 정렬도가 낮은 워크로드에서 이득 축소.
- threshold(1MB)·data-zone(256KB)·Bcount(4MB) 등 하드웨어·워크로드 의존 파라미터 다수, 자동 튜닝은 미제시.
- crash consistency를 하부 FS journaling에 위임 — scrap buffer의 휘발성 dirty 데이터에 대한 독자적 crash 안전성 분석은 제한적.

**열린 질문**
- CXL 메모리/NVM이나 ZNS·FDP SSD 위에서 scrap buffer/OTflush 설계가 어떻게 바뀌나?
- 동일 인터페이스로 read-before-write를 더 줄이거나 erasure-coding 쓰기에 확장 가능한가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. WSBuffer가 해결하려는 buffered I/O 쓰기의 세 병목(C1/C2/C3)은 무엇인가?
> 답: C1 모든 incoming write를 critical path에서 page caching하는 과도한 버퍼링(SSD 대역폭 미활용), C2 page 관리(free 삽입/clean 삭제/state 갱신)의 non-scalable spinlock 기반 낮은 concurrency, C3 partial-page write의 read-before-write penalty(full 대비 1.51×–84.37× latency).

> [!question]- Q2. scrap buffer가 page cache와 다른 핵심 차이는?
> 답: page cache는 모든 메모리가 page로 채워지지만(주로 읽기 캐싱), scrap-page는 128B header의 index entry로 다양한 offset/size의 partial write data-segment를 merge-friendly하게 관리하고 read-before-write 없이 직접 쓴다. clean이 아니라 dirty 쓰기 전용이며 SSD로 빠르게 flush되어 짧은 시간만 메모리 점유.

> [!question]- Q3. 큰 쓰기와 작은 쓰기를 어떻게 다르게 처리하나?
> 답: request size가 threshold(1MB) 미만이면 전부 scrap buffer로 흡수. 이상이면 partial-scrap-page 부분과 scrap-page 정렬(256KB 배수) 부분으로 분할해, 정렬 부분은 SSD에 직접 write(C1 해결)하고 partial 부분만 scrap buffer에 버퍼링한다.

> [!question]- Q4. OTflush의 2단계와 SSD busyness 인지 방식은?
> 답: Stage-1은 unfilled scrap-page를 SSD-read로 채우고(read-before-write를 비동기로 미룸), Stage-2는 full scrap-page를 SSD에 writeback한다. per-SSD Bcount(submit 시 +, 완료 시 − I/O 크기)를 임계 4MB와 비교해 SSD가 바쁘면 flushing을 미루고 idle SSD를 골라 처리한다.

> [!question]- Q5. concurrent page management가 lock contention을 줄이는 원리는?
> 답: read-only memory-page는 기존 XArray로(clean이라 state 관리 불필요), dirty scrap-page는 SXArray로 분리한다. SXArray는 삽입/삭제만 맡고 page-state는 안 맡으며 삭제를 index-entry-level lock(경량)으로 처리, tree 갱신은 기회적으로 한다. 또 per-scrap-page lock으로 scrap write와 OTflush Stage-1을 SXArray lock 없이 동시에 수행한다.

> [!question]- Q6. WSBuffer가 direct I/O 대비 갖는 장점은? 수치로?
> 답: buffered I/O 인터페이스를 그대로 유지(프로그래밍 변경 불필요)하면서 direct I/O의 RMW/partial-write penalty를 피한다. FIO random write에서 XFS-direct I/O·AutoIO 대비 1.59×–231.28× latency 개선(Fig.9).

> [!question]- Q7. WSBuffer가 CPU와 메모리까지 줄이는 이유는?
> 답: 데이터의 80% 이상을 SSD로 직접 전송 → DMA가 처리해 CPU copy 절감(baseline 대비 CPU 3.2%–28.4% 낮음). 큰 쓰기는 SSD 직접, 작은 쓰기는 scrap-page에 모이나 이후 큰 overwrite로 obsolete되어 reclaim되므로 foreground write memory 소비가 real-world에서 0.34%–1.67%에 불과.

> [!question]- Q8. 전체 throughput 개선 범위와 가장 큰 이득이 나는 상황은?
> 답: throughput 최대 3.91×, latency 최대 82.80× 개선. 가장 큰 latency 이득은 partial-page write(read-before-write 제거, <1MB에서 최대 82.80×)와 제한 메모리·고강도 쓰기 워크로드(Fileserver 최대 4.48×)에서 나타난다.

## 🔗 Connections
[[File System]] · [[FAST]] · [[2026]]

## References worth following
- **[73] Zhan et al., "Rethinking the Request-to-IO transformation process of file systems for full utilization of High-Bandwidth SSDs", FAST 2025** — 같은 그룹의 직전 연구, 본 논문의 직접 선행.
- **[45] Pham et al., "ScaleCache: A scalable page cache for multiple SSDs", EuroSys 2024** — SOTA buffered I/O 최적화 baseline(ccXArray).
- **[38] Li & Zhang, "StreamCache: Revisiting page cache for file scanning on fast storage", USENIX ATC 2024** — page 관리 concurrency 개선 비교군.
- **[46] Qian et al., "Combining buffered i/o and direct i/o in distributed file systems", FAST 2024** — hybrid I/O(AutoIO 원리) 비교군.
- **[14][63] Wilcox, "Page folios" / folios** — 읽기 측 page 관리 효율의 기반, WSBuffer가 활용.
- **[74] Zhan et al., "Romefs: A cxl-ssd aware file system", SoCC 2024** — 같은 그룹, CXL-SSD dual-path 확장 방향.

## Personal annotations
<본인 메모 영역>
