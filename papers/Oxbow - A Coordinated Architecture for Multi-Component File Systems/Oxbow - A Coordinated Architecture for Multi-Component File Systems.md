---
title: "Oxbow: A Coordinated Architecture for Multi-component File Systems"
description: "커널·유저스페이스·CSD(computational storage device) 세 컴포넌트를 역할별로 조율해 성능·커널 상호운용성·낮은 CPU 소비·개발 속도를 동시에 달성하는 파일시스템 아키텍처"
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
  - topic/computational-storage
  - topic/journaling
  - topic/crash-consistency
  - topic/kernel-bypass
---

# Oxbow: A Coordinated Architecture for Multi-component File Systems

> **OSDI 2026** · cluster/fs · Source: [Oxbow - A Coordinated Architecture for Multi-Component File Systems.pdf](<Oxbow - A Coordinated Architecture for Multi-Component File Systems.pdf>)

저자: Jongyul Kim (University of Illinois Urbana-Champaign), Jaehwan Lee (KAIST), Inhoe Koo (KAIST), Peizhe Liu (University of Illinois Urbana-Champaign), Jiyuan Zhang (University of Illinois Urbana-Champaign), Junho Ahn (KAIST), Tianyin Xu (University of Illinois Urbana-Champaign), Youngjin Kwon (KAIST)

## TL;DR
현대 파일시스템은 유저레벨(성능↑, 커널서비스↓), 커널(통합↑, 느림), 디바이스 상주(호스트 CPU↓, PCIe 지연·약한 프로세서)로 파편화되어 각자 일부 목표만 달성한다. Oxbow는 이 세 컴포넌트를 "선택 대상"이 아닌 "상호보완 자원"으로 보고, oxLib(유저 라이브러리)·illuFS(커널 shim)·H-Server(신뢰된 유저레벨 FS 서버)·D-Server(CSD 상주 서버) 네 컴포넌트로 역할을 분할한다. 핵심 기법은 semi-kernel-bypassing I/O(읽기는 커널 page cache/readahead 경유, 쓰기는 커널 우회), shared-ownership metadata(inode 속성별 단일 writer 파티셔닝), Split Journaling(fsync를 호스트에서 즉시 완료시키고 background journaling만 CSD로 오프로드) 세 가지다. lwext4 기반으로 4개 컴포넌트·53K LOC를 구현하고 BlueField-2 DPU로 CSD를 에뮬레이션해 평가한 결과, Ext4 대비 최대 4.8배, μFS 대비 최대 86% 높은 처리량과 μFS 대비 최대 3.9배의 CPU 효율을 보였다.

## 문제 & 동기
Fast storage(14 GB/s급)와 computational SSD(CSD) 등장으로 기존 커널 중심 아키텍처가 병목이 되면서, 업계는 유저레벨 파일시스템(성능↑, 커널 서비스 상실), 커널 파일시스템(통합↑, 느림), 디바이스 상주 로직(호스트 부하↓, PCIe 지연·약한 프로세서로 지연/메타데이터 작업 불리)으로 파편화되었다(p.1425-1426). 저자들은 Filebench Varmail 워크로드로 이 트레이드오프를 정량화한다: μFS는 Ext4 대비 2.4배 처리량을 내지만 CPU 사이클을 3.4배 소비하며, 그중 50% 이상이 파일시스템 계층에서 소모된다(Fig.2, p.1427). 즉 어떤 단일 아키텍처도 고성능·효율적 자원 사용·견고한 커널 통합을 동시에 만족시키지 못한다는 것이 핵심 동기다.

> [!quote]- 📄 원문 표현 (paper)
> - "Fast storage hardware and computational SSDs have outpaced the traditional kernel-centric or kernel-bypass file system designs, fragmenting modern storage stacks across library file systems, kernel subsystems, and in-device file systems." (p.1425)
> - "Figure 2, with a mail server workload, shows the trade-off: μFS achieves 2.4× Ext4 throughput but consumes 3.4× CPU cycles, with over 50% spent in the file system layer." (p.1427)
> - "no single architecture delivers high performance, efficient resource utilization, and robust kernel integration simultaneously." (p.1426)

## 핵심 통찰 (Key Insight)

**1. 컴포넌트는 "택일 대상"이 아니라 "상호보완 자원"이다.** 커널은 캐싱·보호·공유에, 유저스페이스는 유연한 FS 로직과 빠른 I/O에, 디바이스는 CPU 집약적 백그라운드 작업에 특화시켜 각자 "잘하는 일"만 맡긴다. 이렇게 하면 하나의 아키텍처가 다른 아키텍처의 약점을 상속하지 않는다.

> [!quote]- 📄 원문 표현 (paper)
> - "The central idea behind Oxbow is to partition responsibilities strategically and minimize shared mutable state, allowing each component to excel at the tasks it is naturally suited for." (p.1426)
> - "unlike prior device-centric systems (e.g., DevFS [33], CrossFS [47], and OmniCache [57]) that migrate substantial foreground logic into the device, the coordinated architecture retains foreground file-system operations on the host to avoid PCIe round trips and preserve low latency." (p.1427)

**2. Semi-kernel-bypassing I/O — 읽기와 쓰기의 비대칭적 가치를 이용한다.** 읽기는 커널의 page cache·readahead·mode-switch 오버헤드 상각 효과가 크므로 커널을 거치게 하고, 쓰기는 커널 서비스로부터 얻는 이득이 적으므로 커널을 우회해 유저레벨 속도로 스트리밍한다. 이는 "커널 전체를 켜거나 끄는" 이분법 대신 오퍼레이션 단위로 커널 관여를 선택하는 설계다.

> [!quote]- 📄 원문 표현 (paper)
> - "Operations that depend on kernel services or state (e.g., page faults, eviction decisions, access-control checks) traverse the kernel, while others bypass the kernel to achieve user-level latency and throughput. In particular, reads benefit from kernel caching and readahead, whereas writes benefit from bypassing the kernel on the persistence path." (p.1427-1428)

**3. Shared-ownership metadata — inode 속성별 단일 writer로 동기화를 제거한다.** inode를 통째로 한 컴포넌트가 소유하게 하면 lease 등 값비싼 동기화가 필요해지므로, 속성(field) 단위로 분할해 각 속성마다 오직 한 컴포넌트만 업데이트하게 만든다(예: oxLib가 size/mtime, kernel이 uid/gid). 공유 메모리에 저장해 복사 없이 여러 컴포넌트가 읽되, 잘못된 값이 들어와도 재검증하여 오염을 막는다.

> [!quote]- 📄 원문 표현 (paper)
> - "Instead of assigning an inode or metadata structure exclusively to a single component, the coordinated architecture partitions metadata fields and assigns each partition to the component best suited to maintain it." (p.1427)
> - "Ownership is enforced rather than advisory: Oxbow treats the kernel and H-Server as trusted and oxLib as untrusted... H-Server also re-validates owned fields on use, so fabricated values stay contained—an inflated size, for instance, fails at block resolution instead of exposing unallocated blocks." (p.1429)

**4. Split Journaling — fsync를 background commit으로부터 구조적으로 분리한다.** 기존 저널링(메타데이터/데이터/로지컬 저널링 모두)은 fsync와 백그라운드 커밋 사이에 in-place update dependency와 POSIX ordering dependency가 남아 CSD 오프로드 시 fsync가 느린 디바이스 커밋을 기다려야 한다(Fig.7). Split Journaling은 fsync 시 self-contained staging transaction을 별도 staging area에 기록해 이 의존성을 완전히 제거한다.

> [!quote]- 📄 원문 표현 (paper)
> - "Split Journaling eliminates both dependencies by using data journaling for its two paths and introducing a staging area that decouples fsync from background commits." (p.1431)
> - "As a result, host and device can operate in parallel (Figure 7d)." (p.1431)

## 설계 / 메커니즘 (Design)
Oxbow는 4개 컴포넌트로 구성된다(Fig.3, p.1428):
- **oxLib** — 애플리케이션에 링크되는 유저스페이스 라이브러리. POSIX API를 노출하고 read/write 시스템콜을 mmap 기반 load/store로 가로챈다. fsync·네임스페이스 연산은 H-Server로 포워딩하며, 공유메모리에 per-file dirty-page/page-lock 비트맵을 유지한다(p.1428-1429).
- **illuFS** — 커널에 추가된 얇은 in-kernel FS(kernel shim). VFS 관점에서는 일반 커널 FS처럼 mount/unmount·page cache·readahead·eviction에 참여하지만, 자체 FS 레이어를 구현하지 않고 event-driven 인터페이스(epoll 기반)로 I/O·메타데이터 요청을 H-Server에 포워딩한다(p.1429).
- **H-Server** — 신뢰된 유저레벨 FS 서버. on-disk layout·block/inode allocation·indexing·저널링을 담당하는 "유저스페이스용 VFS" 역할. 모든 디바이스 I/O가 유저레벨 드라이버를 통해 H-Server를 거친다. fsync 데이터를 staging하고 D-Server와 background journaling을 조율한다(p.1429).
- **D-Server** — CSD(computational storage device) 상에서 실행되며 background journaling과 checkpointing만 수행한다. 파일 의미론은 모르고 순수 block address/extent 단위로만 동작해 file-semantics-agnostic device offloading을 구현한다(p.1429).

**Shared ownership (Fig.4, p.1429)**: inode 상태를 공유메모리에 두고 mtime/size는 oxLib가 W, uid/gid/mode는 kernel이 W로 소유하는 single-writer 구조. Dirty-page/page-lock 비트맵도 oxLib·H-Server만 접근하며 커널은 관여하지 않는다(p.1429-1430).

**Read path (Fig.5, p.1430)**: oxLib가 read를 mmap 영역 load로 변환 → page fault → illuFS가 H-Server에 블록 번호 요청 → 유저레벨 드라이버로 I/O 후 page cache 채움 → 커널이 page fault를 resolve. 커널 readahead도 그대로 활용된다.

**Write/writeback path (Fig.6, p.1430)**: 애플리케이션은 mmap된 페이지에 store, oxLib가 dirty bit·page-lock 설정. fsync 시 H-Server가 파일 메타데이터를 스냅샷하고 dirty page를 블록에 매핑, 전용 staging area에 기록 후 즉시 반환(background journaling 완료를 기다리지 않음). Background commit은 D-Server가 별도로 비동기 수행한다.

**DMA snapshot as shadow copy (Fig.8, p.1431)**: CSD 오프로드를 위해 트랜잭션의 모든 페이지를 하나의 연속 DMA 버퍼로 복사하는데, 이 여분의 복사를 shadow copy로 재활용해 (1) page 업데이트와 background commit 간 간섭을 제거하고 (2) 글로벌 락의 세분성(page→transaction)을 낮춰 잠금 경합을 줄인다.

**Crash consistency (p.1432)**: 불변식은 "매 fsync-ed 파일은 자신의 self-contained staging transaction + 그 staging transaction이 기록한 ID까지의 저널 prefix로부터 복구 가능"하다는 것. Partial failure는 fail-stop으로 가정하며, H-Server 실패 시 재시작만 필요(D-Server 상태 유지), D-Server 실패 시 H-Server 세션 무효화 후 둘 다 재시작, CSD 컴퓨트 능력만 상실 시 D-Server가 호스트에서 block device로 재구동 가능(p.1432).

**File sharing/protection (p.1432-1433)**: 커널의 page cache와 discretionary access control(DAC)을 그대로 활용해 파일 공유·보호를 구현하고, 동시쓰기 조율만 oxLib/H-Server의 page-lock 비트맵으로 처리한다. 디바이스 접근은 H-Server/D-Server로만 제한되어 악성 애플리케이션의 직접 I/O를 차단한다.

> [!quote]- 📄 원문 표현 (paper)
> - "The kernel communicates with H-Server using an event-driven mechanism based on epoll... H-Server and D-Server communicate over PCIe using an RPC mechanism for control (e.g., 'transaction ready') and DMA for bulk data transfer." (p.1429)
> - "Durability is established at fsync via a self-contained staging transaction persisted to the on-disk staging area, so a host-side failure (oxLib, illuFS, or H-Server) loses only data not yet fsync-ed, exactly as under POSIX." (p.1432)

## 평가 (Evaluation)
**Testbed**: dual-socket Intel Xeon Gold 5218 (2.30GHz), 128GB DRAM, Samsung PM1735 3.2TB NVMe SSD (SR-IOV/Namespace Sharing 지원). CSD는 NVIDIA BlueField-2 DPU(8× 2.0GHz ARMv8 A72, 16GB DRAM, 100Gbps 이더넷)로 에뮬레이션(p.1433). 비교 대상: Ext4, μFS(SOSP'21), OmniCache(FAST'24).

**Microbenchmark 처리량/CPU 효율**: "Across microbenchmarks, Oxbow delivers up to 4.8× the write throughput of Ext4 and up to 86% higher throughput than μFS, while reducing host CPU consumption by up to 55% relative to μFS and improving throughput-per-CPU efficiency by up to 3.9× over μFS (4.7× over Ext4)."(p.1425, Abstract) Write throughput은 append/seq/rand write에서 Ext4 대비 2.7-4.6배, 1.4-4.5배, 1.3-4.8배(p.1435 본문 서술). Fsync 시간은 1클라이언트 기준 2위 시스템 대비 16.8-19.2배 낮음(Table 1, p.1435). Random read는 4KB에서 Ext4·μFS 대비 5.5배/7.7배 낮은 지연(p.1434), sequential read latency는 μFS 대비 18.2배 낮음(p.1434).

**CPU 효율 (Fig.11, p.1435-1436)**: write 연산에서 Oxbow는 μFS 대비 1.8-3.9배 효율(GB/BCycle) 달성. μFS는 client-server busy-waiting 때문에 클라이언트 증가 시 최대 7.6 cores까지 소비(10 clients).

**Ablation (Fig.12/13, p.1436-1437)**: host journaling(D-Server를 호스트에서 실행)은 기본 Oxbow 대비 최대 44% 더 많은 호스트 CPU 소비. no-staging(staging 비활성화)은 latency가 기본 대비 약 7.8배. no-background-journaling은 처리량이 최대 33% 낮음.

**LevelDB YCSB (Fig.14, p.1437)**: read-dominated workload B/C에서 μFS 대비 각각 1 프로세스에서 83%/89%, 8 프로세스에서 34%/37% 높은 처리량. Workload E(range-heavy)에서 Ext4 대비 1/8 프로세스에서 각각 41%/17% 높은 처리량. Workload D(read-latest, cache-heavy)에서는 8 프로세스일 때 μFS보다 20% 낮음(μFS의 zero-copy 이점 때문).

**RAG retrieval (Table 2, p.1437)**: Oxbow의 I/O 지연은 Ext4 대비 50% 낮고, tail(P99.9) I/O 지연은 최대 45% 낮음.

**LLM checkpointing (Table 3, p.1438)**: Oxbow는 Ext4 대비 58% 높은 처리량, 46% 낮은 평균 지연.

**Nginx sendfile (Fig.15, p.1438)**: 커널 VFS 레이어를 그대로 유지하므로 sendfile 활성화 시 Ext4와 유사하게 3.3배 높은 처리량(비활성 대비).

**Metadata microbenchmark (Appendix B, Fig.16, p.1439-1440)**: stat/statall은 10 클라이언트에서 μFS 대비 19배/10배, Ext4 대비 57배/30배 처리량. create/unlink/rename은 단일 클라이언트에서 가장 낮지만(H-Server와 kernel 양쪽을 거치므로), 유저레벨 병렬성 덕분에 1→10 클라이언트로 갈수록 rename이 5.5배 스케일.

> [!quote]- 📄 원문 표현 (paper)
> - "Oxbow achieves the best throughput across read- and range-heavy workloads, improving throughput by up to 89% over μFS and 41% over Ext4." (p.1425)
> - "By retaining the kernel's VFS layer, Oxbow can transparently leverage sendfile, achieving 3.3× higher Nginx throughput than the same configuration without it." (p.1425)

## 섹션 노트
- **§1 Introduction**: 파편화된 저장 스택(유저레벨/커널/디바이스 상주) 각각의 한계를 제시하고 coordinated architecture와 3대 조율 기법(semi-kernel-bypassing I/O, shared-ownership metadata, split journaling)을 예고.
- **§2 Trends**: Fig.1로 6가지 FS 아키텍처(모놀리식 커널, FUSE, 유저레벨, 커널 보조 유저레벨, in-device, coordinated)를 비교. §2.3에서 CSD의 CPU 오프로드가 background task에 적합함을 논증.
- **§3 Coordinated Architecture**: 배치 원칙(안정적 기능은 커널에, 동적 기능은 유저스페이스에)과 조율의 3대 난제(kernel involvement balancing, metadata synchronization, cross-component data movement, crash-consistency dependency)를 정리.
- **§4 Oxbow File System**: 4개 컴포넌트의 역할과 통신 채널(§4.1), state ownership(§4.2), end-to-end 흐름(§4.3), Split Journaling(§4.4), Crash Consistency(§4.5), File Sharing/Protection(§4.6)을 상세 기술.
- **§5 Implementation**: lwext4를 H-Server의 FS 로직으로 사용(멀티스레딩 추가, 내장 저널링 비활성화), C/C++로 총 53K LOC(lwext4 제외).
- **§6 Evaluation**: microbenchmark(§6.1-6.2), ablation study(§6.3), real-world workload(§6.4)로 구성.
- **§7 Related Work**: FUSE류 유저레벨 FS, SoC 기반 CSD 오프로드(DevFS, CrossFS, OmniCache 등), logical journaling(SplitFS, iJournaling, FastCommit), 개발 비용 절감 연구와 비교.
- **§8 Conclusion**: "위치 선택"이 아닌 "조율"이 파편화 문제의 근본 해법이라는 주장으로 마무리.
- **Appendix A-B**: 아티팩트 스코프·호스팅(GitHub, osdi26-ae 브랜치) 정보와 metadata microbenchmark(stat/statall/listdir/create/unlink/rename) 상세 결과.

## 핵심 용어 (Key terms)
- **oxLib**: 애플리케이션에 링크되는 유저스페이스 라이브러리. POSIX API를 mmap 기반 load/store로 매핑하고 dirty-page/page-lock 비트맵을 관리.
- **illuFS**: 커널에 추가된 얇은 in-kernel FS shim. VFS에 참여하지만 자체 FS 로직 없이 H-Server에 요청을 이벤트 기반으로 포워딩.
- **H-Server**: 신뢰된 유저레벨 파일시스템 서버. on-disk layout·allocation·저널링을 담당하며 "유저스페이스용 VFS" 역할.
- **D-Server**: CSD에서 실행되는 서버. file-semantics-agnostic하게 background journaling/checkpointing만 수행.
- **Semi-kernel-bypassing I/O**: 읽기는 커널(page cache/readahead) 경유, 쓰기는 커널 우회하는 선택적 커널 관여 전략.
- **Shared-ownership metadata**: inode 속성을 갱신 주체별로 파티셔닝해 속성당 단일 writer를 부여하는 메타데이터 동기화 기법.
- **Split Journaling**: fsync를 위한 fast staging path(호스트)와 background commit path(D-Server)를 분리해 in-place update dependency·POSIX ordering dependency를 제거하는 host-device journaling.
- **Staging area / staging transaction**: fsync 데이터를 임시로 기록하는 전용 on-disk 영역/트랜잭션. self-contained하여 crash 시 그 자체로 복구 가능.
- **DMA snapshot (shadow copy)**: 트랜잭션의 페이지를 연속 DMA 버퍼로 복사하며 이를 shadow copy로 재활용해 락 세분성을 낮추는 기법.
- **CSD (Computational Storage Device)**: ARM 코어·DRAM·가속기를 내장한 스토리지 디바이스로 host CPU 부하를 오프로드하는 대상.
- **Fail-stop**: 컴포넌트가 실패 시 정지한다는 가정 하의 부분 실패(partial failure) 모델.

## 강점 · 한계 · 열린 질문
- **강점**: 성능·커널 상호운용성·낮은 CPU 소비·개발 속도라는 서로 상충하는 4개 목표를 단일 아키텍처에서 동시에 달성. Microbenchmark부터 LevelDB/RAG/LLM checkpointing/Nginx까지 폭넓은 실증. 오픈소스 아티팩트(GitHub, USENIX artifact evaluated 배지) 공개로 재현성 확보.
- **한계**: 단일 H-Server 스레드가 모든 디렉터리 연산(listdir 등)을 처리해 단일 클라이언트에서 create/unlink/rename 처리량이 가장 낮음(Appendix B, p.1440). Workload D(read-latest)에서 μFS 대비 20% 낮은 성능(zero-copy 부재, p.1437). BlueField-2 DPU + SR-IOV/NVMe Namespace Sharing 지원 SSD라는 특수 하드웨어에 의존하며, lwext4(임베디드용 ext4 구현) 기반이라 실제 프로덕션 ext4의 모든 기능(예: 저널 체크섬, 확장 속성 전체)을 다루는지는 불명확.
- **열린 질문**: H-Server를 다중 인스턴스로 확장하거나 분산 파일시스템으로 일반화할 수 있는가? Trusted H-Server를 컴퓨팅 신뢰 기반(TEE 등) 없이 신뢰하는 보안 모델이 실제 배포에서 충분한가? Split Journaling을 ext4 이외의 온디스크 레이아웃(예: B-tree 기반)에도 그대로 적용 가능한가?

## ❓ Q&A (자가 점검)
> [!question]- 왜 read는 커널을 거치고 write는 우회하는가?
> 읽기는 커널의 page cache·readahead로부터 큰 이득을 얻지만, 쓰기는 그 이득이 적고 오히려 커널 크로싱 지연이 손해가 크기 때문. Oxbow는 "커널 전체를 켜거나 끄는" 대신 오퍼레이션 단위로 비대칭적 가치를 활용한다(p.1427-1428).

> [!question]- Shared-ownership metadata에서 어떤 필드를 누가 소유하는가?
> Fig.4 기준 mtime/size는 oxLib(유저레벨)가 W, uid/gid/mode는 kernel이 W. 각 속성은 단일 writer만 가지며 나머지 컴포넌트는 read-only로 공유메모리에서 접근한다(p.1429).

> [!question]- Split Journaling이 어떻게 fsync latency를 background commit으로부터 분리하는가?
> fsync 시 self-contained staging transaction을 별도 staging area에 기록해 durable로 만들고 즉시 반환하며, background journaling(D-Server)은 이후 비동기적으로 진행한다. 이 staging transaction은 최근 커밋된 journal transaction ID를 기록해 자체적으로 복구 가능하다(p.1430-1431).

> [!question]- Partial failure 시 각 컴포넌트 실패는 어떻게 처리되는가?
> H-Server 실패(호스트측: oxLib/illuFS/H-Server)는 fsync-ed 데이터만 안전, 재시작만 필요(D-Server 세션 유지). D-Server 실패는 H-Server 세션을 무효화하고 둘 다 재시작 및 recovery 절차 수행. CSD 컴퓨트 능력만 손실되면 D-Server가 호스트에서 block device로 재구동 가능(p.1432).

> [!question]- Oxbow가 μFS보다 성능이 뒤처지는 경우는?
> LevelDB YCSB workload D(read-latest, 캐시 히트 위주)에서 8 프로세스일 때 μFS보다 20% 낮음 — μFS의 유저-FS 서버 간 zero-copy 이점 때문(p.1437). Metadata microbenchmark의 create/unlink/rename도 단일 클라이언트에서는 H-Server+kernel 양쪽을 거쳐 셋 중 가장 낮음(p.1440).

> [!question]- sendfile 같은 zero-copy 기능을 Oxbow는 어떻게 유지하는가?
> 커널 VFS 레이어를 그대로 재사용하므로(illuFS가 VFS에 참여) 추가 개발 비용 없이 sendfile을 활용할 수 있고, 이를 통해 Nginx에서 3.3배 높은 처리량을 달성한다(p.1438).

> [!question]- CSD는 실제 논문에서 어떤 하드웨어로 에뮬레이션했는가?
> NVIDIA BlueField-2 DPU(8× 2.0GHz ARMv8 A72 코어, 16GB DRAM)를 이용하고, SR-IOV와 NVMe Namespace Sharing을 지원하는 Samsung PM1735 SSD로 호스트와 SmartNIC이 동일 네임스페이스를 공유하도록 구성했다(p.1433).

> [!question]- Ablation에서 staging area를 없애면 어떤 일이 일어나는가?
> fsync가 background-journaling path만 사용하게 되어 호스트 CPU 소비는 크게 줄지만, latency는 기본 Oxbow 대비 약 7.8배로 증가한다 — staging이 fsync 지연 감소의 핵심 기여임을 보여준다(p.1436-1437).

## 🔗 Connections
[[File System]] · [[OSDI]] · [[2026]]
관련: [[OmniCache - Collaborative Caching for Near-storage Accelerators]] · [[FastCommit - resource-efficient, performant and cost-effective file system journaling]] · [[Fast, Transparent Filesystem Microkernel Recovery with Ananke]]

## References worth following
- Liu, J. et al., "Scale and Performance in a Filesystem Semi-Microkernel" (μFS, SOSP'21) — Oxbow의 핵심 성능 baseline이자 유저레벨 FS 설계의 비교 대상.
- Zhang, J. et al., "OmniCache: Collaborative Caching for Near-storage Accelerators" (FAST'24) — device-centric 아키텍처의 대표 비교군, Oxbow의 coordinated 설계와 대조.
- Kannan, S. et al., "Designing a True Direct-Access File System with DevFS" (FAST'18) — 디바이스에 FS 로직을 전량 이관하는 선행 device-resident 접근.
- Kadekodi, R. et al., "SplitFS: Reducing Software Overhead in File Systems for Persistent Memory" (SOSP'19) — logical journaling 계열 선행 연구로 Split Journaling의 아이디어적 뿌리.
- Shirwadkar, H. et al., "FastCommit: resource-efficient, performant and cost-effective file system journaling" (USENIX ATC'24) — 저널링 효율화라는 동일 문제의식을 공유하는 최신 연구.

## Personal annotations
<!-- 본인 메모 영역 -->
