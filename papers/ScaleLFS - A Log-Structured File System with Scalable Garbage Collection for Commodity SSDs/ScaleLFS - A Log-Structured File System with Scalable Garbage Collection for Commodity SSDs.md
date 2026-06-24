---
title: "ScaleLFS: A Log-Structured File System with Scalable Garbage Collection for Commodity SSDs"
aliases: [ScaleLFS]
description: "Commodity SSD에서 LFS의 GC를 per-core 전용화·동시화하여 sustained 성능을 F2FS 대비 최대 7배 끌어올린 로그 구조 파일시스템."
venue: FAST
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/log-structured
  - topic/garbage-collection
  - venue/fast
  - year/2025
---

# ScaleLFS: A Log-Structured File System with Scalable Garbage Collection for Commodity SSDs

> **FAST 2025** · `cluster/fs` · Source: [ScaleLFS - A Log-Structured File System with Scalable Garbage Collection for Commodity SSDs.pdf](ScaleLFS%20-%20A%20Log-Structured%20File%20System%20with%20Scalable%20Garbage%20Collection%20for%20Commodity%20SSDs.pdf)

저자: Jin Yong Ha (Seoul National University), Sangjin Lee (Chung-Ang University), Hyeonsang Eom (Seoul National University), Yongseok Son (Chung-Ang University, 교신저자)

## TL;DR
LFS(Log-Structured File System)는 GC(Garbage Collection)를 단일 GC 스레드가 모든 dirty segment를 처리하는 직렬화(one-to-all) 모델로 수행해, GC가 트리거되면 application I/O가 막히고 sustained 성능이 급락한다. ScaleLFS는 F2FS 위에 (1) per-core 전용 자원을 갖는 dedicated garbage collector(DGC), (2) atomic bitmap·loose-synchronization으로 victim을 동시 선택/갱신하는 scalable victim manager(SVM), (3) concurrent hash table로 file-level lock 없이 page 단위 GC를 가능케 하는 scalable victim protector(SVP) 세 기법을 도입한다. 결과적으로 F2FS, scalable LFS(MAX), parallel GC scheme(P-GC) 대비 sustained 성능을 각각 최대 3.5배, 4.6배, 7.0배 향상시킨다.

## 문제 & 동기
- LFS는 small/random write를 large/sequential write로 모아 SSD에 유리하지만, 노화될수록 free segment를 확보하기 위한 GC가 비싸진다. 기존 LFS는 GC를 foreground에서 직렬로 수행해 파일시스템 전체를 freeze시키고 latency 급등·대역폭 급락을 유발한다.
- micro-benchmark(FIO)에서 GC가 약 15초경 트리거되면, 기존 LFS의 application 대역폭이 최대 68배, device 대역폭이 최대 24.8배 급락한다. 동시에 SSD에는 추가 I/O를 처리할 idle 대역폭이 남아 있어 개선 여지가 크다.
- 근본 원인 세 가지: (1) 단일 GC 스레드의 직렬화(one-to-all 모델), (2) victim selection/segment metadata의 coarse-grained lock contention(`seglist_lock`, `sentry_lock`, `curseg_mutex`), (3) GC와 I/O 간 file-level lock(`i_gc_rwsem`)으로 인한 GC 동시성 제약.

> [!quote]- 📄 원문 표현 (paper)
> "Unfortunately, when the file system performs the garbage collection in the foreground, it freezes the entire file system until it completes, resulting in high latency, low bandwidth, and long execution time of application." (p.1)
>
> "When a GC thread wakes up, the application threads are blocked until the GC thread completes its work. Thus, the GC speed determines the blocking time of the application threads, which in turn affects the sustained performance." (p.2)
>
> "The main reason for low sustained performance comes from the serialized GC process with a single garbage collector in LFSs ... relying on a single garbage collector can induce a bottleneck." (p.2)

## 핵심 통찰
> [!note]- 토글: 3대 Insight
> - **Insight #1 (p.2):** LFS는 가용한 device-level 대역폭을 활용해 GC 과정을 가속하면 더 높은 sustained 성능을 얻을 수 있다 — GC 중에도 SSD는 idle 대역폭이 남는다.
> - **Insight #2 (p.3):** one-to-all 모델(단일 collector가 모든 victim 처리)은 multi-core 시스템에서 modern SSD의 high bandwidth를 활용하기에 비효율적이다.
> - **Insight #3 (p.3):** GC 과정을 더 scale 하려면 file-level lock을 넘어선 추가적인 concurrent·scalable 기법이 필수다.
> - **활용 기회:** GC를 기다리며 막힌 application 스레드가 코어를 점유하지 않으므로, 이 unoccupied core를 써서 GC를 가속해도 application을 방해하지 않는다(CPU idle time 활용).

## 설계 / 메커니즘
> [!abstract]- 토글: DGC · SVM · SVP 구조와 동작
> ScaleFS는 F2FS(Linux 6.0.0) 기반 세 컴포넌트로 구성된다. 설계 목표 4가지: efficient parallel GC, concurrent victim selection, concurrent segment metadata access/update, finer & lightweight GC.
>
> **1) Dedicated Garbage Collector (DGC) — Strategy #1 (p.5~6)**
> - per-core로 GC를 두고, 각 DGC가 전용 자원(victim segment, page buffer, write stream)을 가져 one-to-one 모델 실현(one-to-all 탈피).
> - 절차 3단계: ① SVM에서 전용 victim 확보 → ② victim의 valid page를 dedicated page buffer(DPB)로 읽기 → ③ dedicated write stream(DWS, GC stream)으로 valid page 기록.
> - **DPB:** page cache를 거치지 않아 lock contention/page cache pollution 회피. 크기 2MB(=segment 크기), 32 DGC에서 총 64MB.
> - **DWS:** DGC별 전용 write stream으로 LBA 할당 시 `curseg_mutex` contention 제거. power-off 대비 checkpoint에 write stream 상태 저장이 필요해 checkpoint 크기가 F2FS 192B → ScaleLFS 528B(48 코어)로 증가하나 영향은 미미.
>
> **2) Scalable Victim Manager (SVM) — Strategy #2, #3 (p.6~7)**
> - **Concurrent Victim Selection (CVS):** dirty segment bitmap을 atomic하게 스캔하고, segment별로 `atomic_test_and_set_bit()`으로 victim을 grab. 여러 DGC가 lock 없이 서로 다른 victim을 동시에 선택. 각 DGC는 GC cost가 낮은 segment를 M개까지 선택(M은 전체 용량의 0.1%, 예: 30GB→16 segments, 7.68TB→4096 segments가 최적).
> - **Loose-synchronization Update (LSU):** segment metadata를 valid page count(VPC)와 valid page bitmap(VPB)로 분할, atomic bitmap·atomic operation으로 개별 갱신해 `sentry_lock` contention 제거.
> - 부작용 2가지(p.7, Table 1): ① less-optimal victim selection(VPC가 outdated면 최소 VPC 아닌 victim 선택 → WAF 소폭 증가), ② false-positive GC read(VPB가 outdated면 무효화된 page를 valid로 읽는 추가 read). 둘 다 빈도가 낮아 영향 무시 가능. crash consistency는 LFS의 node-level sync·checkpoint로 보장.
>
> **3) Scalable Victim Protector (SVP) — Strategy #4 (p.8~9)**
> - file-level GC lock(`i_gc_rwsem`) 대신 **concurrent hash table** 기반 page-level 보호. 같은 file 내 서로 다른 page를 여러 스레드가 동시 접근 가능(range lock 유사).
> - 절차: 스레드가 target page에 해당하는 bucket을 찾아 linked list를 head→tail로 스캔, 이미 사용 중이면 release 대기, 아니면 `compare_and_swap(CAS)`로 list tail에 삽입. 논리적 삭제(deletion flag) 후 RCU로 물리 삭제.
> - **Data/Crash consistency (p.9):** DPB와 page cache 간, VPB와 VPC의 독립 갱신으로 생길 수 있는 inconsistency를 node lock 기반 규칙(DGC가 victim LBA 미변경 확인 후 기록)으로 해소. checkpoint reader-writer lock(`cp_rwsem`)으로 VPC/VPB를 함께 flush해 crash consistency 보장.
>
> 소스 공개: https://github.com/syslab-CAU/ScaleLFS

## 평가
> [!example]- 토글: 실험 결과 (수치 + p.X)
> **셋업(p.9~10):** Intel Xeon E5-2650(24 physical / 48 logical cores), 160GB DRAM, 7.68TB Samsung 9A3 SSD. 비교: F2FS, F2FS-L(LFS mode), MAX(scalable LFS), P-GC(parallel GC). FIO micro / filebench macro / MySQL+YCSB real-world. filesystem-level GC 격리를 위해 DRAM 8GB·partition 30GB로 device-level GC 억제.
>
> **Micro-benchmark (FIO 48-thread random write, 28GB 파일, 총 120GB write) (p.10~11):**
> - Application 대역폭/실행시간: F2FS, F2FS-L, MAX, P-GC 대비 각각 3.5×/71.1%, 2.4×/58.1%, 4.6×/78.1%, 7.0×/85.8% 향상. 기존 LFS 실행시간 823s/578s/1094s/1772s vs ScaleLFS 236s.
> - Device 대역폭: 각각 15.2×, 2.7×, 19.6×, 8.1× 향상. ScaleLFS는 2.9GB/s 달성(peak 3.4GB/s 근접).
> - Latency QoS: 99th percentile에서 F2FS/F2FS-L/MAX/P-GC 대비 99.95%/99.64%/99.95%/99.96% latency 감소.
> - Core scalability: 2~48 cores에서 최대 3.4×~5.8× 향상, 48 cores에서 saturate.
> - CPU 사용률: GC 시 최대 29.1%까지 증가(기존 LFS는 single-core GC라 2.7~15.7%)나 실행시간을 크게 단축.
> - Device-level GC 시나리오(7.68TB full, 2TB 파일, 10.3TB write): 실행시간 ScaleLFS 13,110s vs F2FS 15,340s, MAX 16,490s (F2FS 대비 약 37분, MAX 대비 56분 단축). F2FS-L/P-GC는 20,000s 내 device-level GC를 트리거조차 못 함(약 11MB/s).
>
> **Macro-benchmark (filebench, fileserver/varmail/OLTP, 95% 활용률) (p.12):** throughput 최대 30.3%/40.6%/83.1% 향상. micro보다 향상폭 작음(잦은 파일 삭제로 free segment 생성 → GC 빈도 낮음).
>
> **Real-world (MySQL + YCSB) (p.12):** workload A throughput 최대 3.38×·실행시간 70.4% 감소; workload B(read-intensive)도 1.37×; update-only는 3.22×·실행시간 68.9% 감소, update latency 11.0/9.8/13.4/12.9ms → 4.1ms. 32 GC threads에서 최대 4.82× 향상.
>
> **개별 기법 기여(Table 3, 32 cores) (p.12~13):** SPGC(단순 per-core GC)는 `i_gc_rwsem`(21%) contention으로 오히려 악화. +SVP가 GC time 최대 67.0% 감소(병목이 `curseg_mutex` 45.7%로 이동) → +DWS가 contention 제거(병목 `sentry_lock` 32.8%로) → +LSU(병목 `seglist_lock` 31.8%로) → +CVS가 `seglist_lock` 제거 → +DPB가 GC time 추가 최대 7.8% 감소. baseline F2FS-L 213.5MB/s → 최종 509.3MB/s.
>
> **Side effects (p.13):** WAF는 fileserver에서 1.0036→1.0040, YCSB에서 9.05→9.37(3.6% 증가)로 미미. false-positive GC read는 fileserver에서 0회, YCSB 1M I/O에서 5890회(I/O당 0.006회)로 무시 가능.
>
> **Overhead (p.13):** Redis(YCSB-C, non-I/O intensive) 동시 실행 시 Redis 실행시간 9.8% 증가하나 FIO는 3.5× 가속. 메모리: DPB 64MB(32 DGC) + SVP, FIO 32 DGC 실험에서 F2FS 대비 최대 40MB 추가.

## 섹션 노트
- **§1 Introduction:** LFS의 GC가 sustained 성능의 핵심 병목임을 micro-benchmark로 제시. ScaleLFS 3대 기여 요약.
- **§2 Background & Motivation:** LFS/GC 동작 설명, GC 시 application thread blocking → unoccupied core·idle device bandwidth 활용 기회 도출.
- **§3 Sustained Performance Analysis:** modern LFS의 sustained 성능 측정, 근본 원인 — serialized GC, scaling 장애(single GC stream/lock contention, victim selection/segment metadata over-strict sync, coarse-grained file-level protection).
- **§4 Design & Implementation:** 4 설계 목표, 4 strategies, DGC/SVM/SVP 상세, data/crash consistency.
- **§5 Evaluation:** micro/macro/real-world, 개별 기법 분해(Table 3), side effect, overhead.
- **§6 Discussion:** ScaleLFS는 I/O-intensive하고 GC로 자주 blocking되는 환경(disaggregated storage, data center file server)에 적합. non-I/O-intensive 워크로드 공존 시 영향 미미.
- **§7 Related Work:** concurrent data structure(Clever, SEPH, range lock), scalable file system(MAX, CJFS, Z-Journal, RFUSE, KucoFS, uFS), LFS GC 개선(ParaFS, IPLFS, SFS, F2FS, P-GC).

## 핵심 용어
- **LFS (Log-Structured File System):** append-only로 small/random write를 large/sequential write로 모아 기록하는 파일시스템. 노화 시 GC 필요.
- **GC (Garbage Collection):** free segment 부족 시 dirty/victim segment의 valid page를 옮겨 invalid page를 회수하는 과정.
- **Sustained performance:** LFS-level GC가 실제로 트리거되는 상태에서의 지속 성능(본 논문 정의, p.2 각주).
- **DGC (Dedicated Garbage Collector):** per-core로 두고 전용 victim·page buffer·write stream을 갖는 GC. one-to-one 모델.
- **SVM (Scalable Victim Manager):** atomic bitmap·instruction으로 victim을 동시 선택(CVS)하고 metadata를 느슨히 동기화(LSU)하는 관리자.
- **CVS (Concurrent Victim Selection):** `atomic_test_and_set_bit()`으로 lock 없이 서로 다른 victim segment를 동시 선택.
- **LSU (Loose-synchronization Update):** segment metadata(VPC/VPB)를 개별 atomic 갱신해 `sentry_lock` 제거. less-optimal victim·false-positive read 부작용 동반.
- **SVP (Scalable Victim Protector):** concurrent hash table로 file-level lock 대신 page 단위 보호 → page-level GC concurrency.
- **DPB / DWS:** Dedicated Page Buffer / Dedicated Write Stream. page cache·`curseg_mutex` contention을 피하는 DGC 전용 자원.
- **VPC / VPB:** Valid Page Count / Valid Page Bitmap. segment metadata의 두 구성요소.
- **WAF (Write Amplification Factor):** 실제 SSD write / 사용자 write 비율. less-optimal victim 선택으로 소폭 증가.

## 강점 · 한계 · 열린 질문
- **강점:** customized SSD 없이 commodity SSD에서 동작(ParaFS/IPLFS와 차별). F2FS 기반 software-only 구현으로 적용성 높음. 직렬 GC 병목을 단계적으로 제거(Table 3)해 기여를 명확히 분해. tail latency·sustained 성능 모두 큰 폭 개선. 소스 공개.
- **한계:** GC 가속을 위해 다수 코어(최대 48 DGC) CPU를 소비 → non-I/O-intensive 워크로드 공존 시 CPU 자원 경쟁(Redis 9.8% 저하). loose-synchronization으로 WAF 소폭 증가·false-positive read 발생. DPB/SVP로 추가 메모리(수십 MB) 소요. unoccupied core가 충분치 않은 환경에서는 효과 제한적.
- **열린 질문:** DGC 수/M 값의 동적 튜닝(워크로드 적응)은? mixed(I/O + compute) 워크로드에서 CPU 자원 스케줄링 정책은? ZNS SSD나 FDP 등 새 인터페이스로의 확장 가능성은? NVMe multi-stream과의 결합 효과는?

## ❓ Q&A (자가 점검)
> [!question]- Q1. 기존 LFS의 sustained 성능이 급락하는 근본 원인 세 가지는?
> > (1) 단일 GC 스레드의 직렬화(one-to-all) 모델, (2) victim selection·segment metadata의 coarse-grained lock contention(`seglist_lock`/`sentry_lock`/`curseg_mutex`), (3) GC-I/O 간 file-level lock(`i_gc_rwsem`)으로 인한 동시성 제약. (§3.2)

> [!question]- Q2. DGC가 one-to-all 대신 one-to-one 모델을 쓰는 이유와 전용 자원 3가지는?
> > 단일 collector가 모든 victim을 처리하면 multi-core·high-bandwidth SSD를 활용 못 한다(Insight #2). DGC는 per-core로 두고 전용 victim segment, dedicated page buffer(DPB), dedicated write stream(DWS)을 가져 자원 공유·contention을 최소화한다.

> [!question]- Q3. SVM의 CVS는 어떻게 lock 없이 victim을 동시 선택하나?
> > dirty segment bitmap을 atomic 스캔하면서 segment별 `atomic_test_and_set_bit()`으로 grab을 시도, 경쟁에서 이긴 DGC가 victim을 가져간다. 진 DGC는 다음 segment부터 스캔을 재개해 서로 다른 victim을 동시 확보한다. (§4.5.1)

> [!question]- Q4. LSU가 유발하는 두 부작용은 무엇이고 왜 허용 가능한가?
> > ① less-optimal victim selection(VPC outdated → 최소 VPC 아닌 victim, WAF 소폭↑), ② false-positive GC read(VPB outdated → 무효 page를 valid로 읽는 추가 read). 실측 WAF 증가 3.6%, false-positive read I/O당 0.006회로 빈도가 낮아 성능 영향이 무시 가능하다. (§4.5.2, §5.5)

> [!question]- Q5. SVP가 file-level lock을 대체하는 메커니즘은?
> > concurrent hash table에 page를 bucket→linked list로 매핑하고 `CAS`로 tail에 삽입, 사용 중이면 대기, 논리 삭제 후 RCU로 물리 삭제. 같은 file 내 서로 다른 page를 여러 스레드가 동시 접근해 page-level GC concurrency를 얻는다. (§4.6)

> [!question]- Q6. ScaleLFS가 ParaFS/IPLFS와 구별되는 핵심 차이는?
> > ParaFS/IPLFS는 customized SSD(OpenSSD, FTL 협조 등) 하드웨어 변경을 요구한다. ScaleLFS는 commodity SSD에서 동작하는 software-only GC 접근으로 적용 범위가 넓다. (§7, Abstract)

> [!question]- Q7. Table 3의 기법별 기여에서 SPGC가 오히려 성능을 악화시킨 이유는?
> > 단순 per-core GC(SPGC)는 여전히 file-level lock `i_gc_rwsem`에 높은 contention(전체 실행시간의 21%)을 일으켜 GC/실행 시간이 증가한다. 이후 SVP가 이 file-level lock을 제거하면서 비로소 개선이 시작된다. (§5.4)

> [!question]- Q8. ScaleLFS가 부적합한 환경은?
> > non-I/O-intensive(compute/memory-intensive) 워크로드 — application이 LFS-level GC로 거의 blocking되지 않아 GC에 쓸 unoccupied core가 부족하다. 또한 CPU 자원을 file I/O에 우선 할당하는 것이 수용 가능해야 한다. (§6, §5.6)

## 🔗 Connections
[[File System]] · [[FAST]] · [[2025]]

## References worth following
- [25] X. Lin et al. **MAX: A Multicore-Accelerated File System for Flash Storage**, USENIX ATC 2021 — ScaleLFS의 주요 비교 대상(scalable LFS).
- [35] D. Seo et al. **Is garbage collection overhead gone? Case study of F2FS on ZNS SSDs (P-GC)**, HotStorage 2023 — parallel GC scheme 비교 대상.
- [16] J. Kim et al. **IPLFS: Log-Structured File System without Garbage Collection**, USENIX ATC 2022 — GC 제거 접근(customized SSD).
- [45] J. Zhang et al. **ParaFS: A Log-Structured File System to Exploit the Internal Parallelism of Flash Devices**, USENIX ATC 2016 — FS-FTL GC 협조.
- [22] C. Lee et al. **F2FS: A New File System for Flash Storage**, FAST 2015 — ScaleLFS의 기반 파일시스템.
- [17] A. Kogan et al. **Scalable Range Locks for Scalable Address Spaces and Beyond**, EuroSys 2020 — SVP의 concurrent protection 영감.

## Personal annotations
<본인 메모 영역>
