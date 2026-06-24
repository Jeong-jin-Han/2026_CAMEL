---
title: "Fast and Synchronous Crash Consistency with Metadata Write-Once File System"
aliases: [Fast and Synchronous, WOFS, WOLVES]
description: "파일 연산마다 메타데이터를 체크섬 보호 package로 묶어 단일 ordering point로 한 번만 쓰는 metadata write-once 파일 시스템(WOFS/WOLVES)으로 PM에서 빠르고 동기적인 crash consistency 달성"
venue: OSDI
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/crash-consistency
  - topic/filesystem
  - topic/persistent-memory
  - venue/osdi
  - year/2025
---

# Fast and Synchronous Crash Consistency with Metadata Write-Once File System

> **OSDI 2025** · `cluster/fs` · Source: [Fast and Synchronous Crash Consistency with Metadata Write-Once File System.pdf](Fast and Synchronous Crash Consistency with Metadata Write-Once File System.pdf)

저자: Yanqi Pan, Wen Xia(교신저자), Yifeng Zhang, Xiangyu Zou, Hao Huang (Harbin Institute of Technology, Shenzhen) · Zhenhua Li (Tsinghua University) · Chentao Wu (Shanghai Jiao Tong University)

## TL;DR
Persistent Memory(PM)에서 기존 crash consistency 기법(journaling, log-structured, soft update)은 파일 연산마다 작고 random하며 ordered된 metadata I/O를 다수 유발해 PM I/O 시간의 70% 이상을 잡아먹는다. 이 논문은 **파일 연산 하나당 모든 메타데이터를 체크섬으로 보호되는 단일 package로 모아 단 한 번의 ordering point(`PCOMMIT` 1회)로 쓰는** metadata write-once file system(WOFS) 모델을 제안하고, Linux 커널 프로토타입 WOLVES로 구현해 기존 PM 파일 시스템 대비 처리량을 1.20~6.73배 개선하고 PM I/O 대역폭 상한(97.3~99.1%)에 근접한다.

## 문제 & 동기
PM은 byte-addressable + persistent buffer 덕분에 블록 계층/디스크 flush 없이 `clwb+sfence`(PCOMMIT, 약 50~300ns)만으로 동기 영속화가 가능하다. 이에 PM 파일 시스템은 fsync 없이도 각 연산이 반환 즉시 durable한 synchronous crash consistency를 지향한다. 그러나 기존 두 갈래 접근은 모두 부족하다.
- **추가 쓰기를 동반하는 방식(JFS/transactional checksum)**: metadata를 journal로 백업 후 in-place 갱신 → redundant write. JFS는 ordering point 3개·metadata I/O 3개 이상.
- **추가 쓰기 없는 방식(LFS, synchronous soft update)**: inode/dentry/log tail 등 여러 metadata object에 대한 순서를 강제 → 작고 random하며 ordered된 metadata I/O 다수, 그리고 copy 기반 GC.

저자들의 motivational 실험(PMFS, SplitFS, NOVA)에서 metadata I/O가 전체 I/O 시간의 70% 이상을 차지하며, SW 워크로드에서 기존 PM 파일 시스템은 PM write 대역폭의 절반(약 2.26 GiB/s) 미만만 활용한다.

> [!quote]- 📄 원문 표현 (paper)
> "Our motivational experiments on three representative PM file systems with different crash consistency techniques show that these metadata I/Os (for crash consistency) dominate more than 70% of the total I/O time, causing a severe PM bandwidth waste." (p.1)
>
> "Random metadata I/Os lead to persistent buffer hit misses, which causes I/O amplifications ... increased from a theoretical ∼2.9 GiB to a measured ∼8.0 GiB, leading to 2.8× amplifications." (p.4)

## 핵심 통찰
> [!note]- 핵심 통찰 토글
> - **통찰 1 — metadata를 연산 단위 package로 재설계**: inode·dentry·log tail 같은 흩어진 metadata object들을 각자 순서 맞춰 갱신하는 대신, 한 파일 연산의 모든 metadata를 하나의 package로 모으면 `m_op = J̄M|J̄C` 형태로 만들 수 있다. 즉 metadata 갱신($m$)과 commit($J_C$)을 transactional checksum처럼 한 번에 쓰되, 단일 ordering point로 압축한다. I/O 순서는 `D → J̄M|J̄C(m_cp)` 단 하나(MIOs ≈ 1, MOrd = 1).
> - **통찰 2 — checksum이 곧 atomicity·완전성 검증 수단**: package header에 magic number + type + timestamp + CRC32를 두어, 부분 기록된 package는 복구 시 checksum 불일치로 감지·폐기. 별도 journal/commit block 없이 atomic·durable.
> - **통찰 3 — non-log layout + reallocation으로 GC 회피**: package/data block을 log가 아닌 malloc 방식으로 PM 전역에 배치하고, 무효화된 공간은 free처럼 reuse(Reuse-GC). LFS의 copy 기반 GC 오버헤드 제거.
> - **통찰 4 — causal order만 보존하면 충분**: WOFS의 crash safety는 issue order가 아니라 causal order를 보존한다. 의존성 없는 package들은 timestamp 순서로 병렬 영속화 가능 → multi-core 확장성.
> - **통찰 5 — coarse persistence로 빠른 복구**: 개별 package를 bitmap에 등록하면 critical path에 small random I/O가 생기므로, 4 KiB pkg-group 단위(coarse)로 bitmap 비트 하나만 두어 복구 시 group을 찾고 그 안의 package를 probe.

## 설계 / 메커니즘
> [!note]- 설계 / 메커니즘 토글
> **Package 설계 (CRUD 추상화)**: 15개 이상 Linux 파일 연산을 DB의 CRUD처럼 추상화. read는 상태를 안 바꾸므로 package 없음. 4종 atomic package:
> - **Create pkg (256 byte)**: create/link 등. 64B 정적 attr(create time, ino) + 128B parent ino·linked ino·name entry + 64B parent attr·header.
> - **Write pkg (64 byte)**: write/fallocate 등 새 data block 할당. extent로 인덱싱, time·size 기록.
> - **Attr pkg (64 byte)**: chmod/chown/truncate 등 속성 변경.
> - **Unlink pkg (64 byte)**: unlink/rmdir/rm 등 link 감소. parent·unlinked inode 변경 attr 포함.
>
> **Compound package**: rename(=create+unlink), symlink(=create+write) 같은 복합 연산은 sub-package들을 header의 **forward-pointer**로 연결(`P1→P2→...→PN`, 각 Pi는 J̄M|J̄C). rename은 "새 파일을 old에 hard link 후 old unlink"로 모델링하며, create pkg가 부분 기록돼도 forward-pointer로 미완성 감지·복구.
>
> **Package write-once I/O (Listing 1)**: header에 type/ts 채우고 `hdr->CRC32 = CRC32(pkg, size)` 계산 후 `PCOMMIT(pkg, size)` (clwb+sfence) 한 번. data 연산은 `D → J̄M|J̄C`로 data 먼저, 그다음 package.
>
> **Package Translation Layer (PTL, Fig.4)**: package를 파싱해 전통적 metadata object(inode/dentry) 제공. pkg-node(C-node/A-node/W-node)로 파싱 결과+package 주소(reclamation용) 보관. inode table(create+attr pkg 결합), data list(W-node로 low-level file 추상화), dent list(C-node로 디렉터리 하위 inode 위치, hash table로 이름 검색).
>
> **Non-log layout & reclamation(§4.4)**: malloc 방식 배치. unlink pkg가 create pkg를 무효화하면 C-node의 pkg addr로 free 표시 후 재할당. truncate→write pkg 무효화. COW write는 overlap 시 old write pkg 무효화. unlink pkg는 자신이 가리키는 create pkg가 재할당될 때까지 reclaim 불가하므로 **order map**(create pkg 주소→unlink pkg 주소)으로 추적.
>
> **Coarse Persistence(CP, §4.5)**: dump-restore(unmount 시 bitmap dump)는 crash 시 PM 전체 scan 필요. 대신 4 KiB pkg-group을 bitmap 1비트로 표현(약 0.012% PM 공간). 복구 시 group 찾아 valid package probe 후 PTL 재구축. m_cp(bitmap 갱신)와 P 사이엔 ordering 불필요(m_cp durable이면 bitmap으로 P 완전성 검증, 아니면 단순 폐기 = 일반 data crash와 동일).
>
> **WOLVES 구현(§5)**: Linux 5.1.0 커널, 12,000+ LoC. PM을 4 KiB block 단위 관리, 첫 2블록 superblock+사본, 다음 몇 블록 bitmap. per-core PTL(PTL-Core)로 multi-core 확장. 각 CPU에 two-level allocator(tl-allocator, red-black tree free list, round-robin). inode table은 global hash table + per-bucket lock. directory hierarchy는 128-slot dentry hash table. **causal-ordering concurrency protocol**: 독립 package는 timestamp 순서면 병렬 영속화, causal 관련 package는 직렬화(VFS 호환).
>
> **추가 최적화(§5.4)**: Huge allocation(HA, 2 MiB huge block append), Read ahead(RA, XPLine 256B stride prefetch), vmovntdq 커널 통합(WOLVES-AVX).
>
> **복구(§5.3)**: atomic package는 create→unlink→write→attr bitmap 순으로 scan하며 causal order로 inconsistency 수정·PTL 재구축. compound는 forward-pointer로 모든 linked package valid 여부 검증.

## 평가
> [!note]- 평가 토글
> **테스트베드**: 16-core Intel Xeon Gold 5218, 128 GiB DRAM, 2×256 GiB Intel Optane PM(non-interleaved), Linux 5.1.0. 비교군: PMFS, NOVA, NOVA-RELAX, SplitFS, MadFS, EXT4-DAX, XFS-DAX, soft update(SoupFS/HUNTER/SquirrelFS) (p.11).
>
> - **종합 처리량**: 기존 PM 파일 시스템 대비 1.20~6.73× throughput; RocksDB 실세계로는 1.20~6.73× (p.2, p.13). PM I/O 대역폭의 **97.3~99.1%** 달성(SW, p.13).
> - **File size scalability(Fig.6)**: SW write throughput 2.20~2.24 GiB/s로 안정(raw PM의 97.3~99.1%); RW는 HA 혜택 없어도 1.65~9.44× (p.13).
> - **Tail latency(Table 4, 32 GiB SW, 4 KiB/op)**: WOLVES 99% 3.16 µs로 NOVA(7.12)·PMFS(6.00)보다 낮음. SplitFS/MadFS는 90~99%는 좋으나 99.99%에서 1125~1302 µs로 급증(WOLVES 18.92 µs) (p.13).
> - **Concurrency(Fig.7)**: 1~8 thread에서 우위, thread≥9에서 PM 동시성 한계로 저하(I/O latency>3000ns 시 delay 삽입으로 완화).
> - **FxMark(Fig.8)**: 대부분 metadata 연산에서 최고 처리량(write-once package 덕분). MRPL(rename)은 VFS traversal 의존으로 비슷.
> - **Filebench(Fig.9, Table 5 vs MadFS)**: Fileserver 14.4×, Varmail 61.4×, Webserver 9.14×, Webproxy 35.8× (single-thread) (p.14).
> - **I/O breakdown(Fig.11)**: NOVA/SplitFS/PMFS 대비 metadata I/O 시간 5~86% 감소, CP 오버헤드 <0.5%. ipmctl로 per-op metadata I/O를 PMFS·NOVA 대비 70%~17.3× 감소(p.14).
> - **Technique breakdown(Fig.12)**: WO가 SW/RW에서 1.01~1.66× 주효; HA가 SW +20.2%; RA가 SR/RR +22~27%.
> - **Recovery(Table 6)**: 실패 복구 시간 NOVA에 필적(Fileserver WOLVES 3.99s vs NOVA 2.48s vs DR 71.9s). 최악(256 GiB 가득, ~60M 파일)에도 ~21.6s — 전통 fsck(60M 파일에 2분+)보다 빠름(p.15).
> - **자원 소비**: PTL 위해 모든 파일 close 시에도 3.8~5.12 MiB 메모리 유지(워크로드 크기의 0.3~1.6%). space overhead 0.0015~0.7%(256 GiB PM)(p.15).
> - **Aging(Fig.14, Agrawal/Geriatrix 80% util)**: fragmentation으로 SW 1.85~1.95 GiB/s(여전히 우위); 극단(블록≤8)에도 1.70~1.82 GiB/s.
> - **Case studies**: ASU(async soft update) 대비 occasional fsync 워크로드에서 21~52% 우위(§6.11); Memory-Semantic SSD(emulated, ~1900 MiB/s max)에서도 최고 write throughput(§6.12, Fig.16).
> - **Crash consistency 증명(Appendix A)**: Sivathanu의 logical framework로 data consistency + version consistency 형식 증명. 1000개 random crash point(append-only, create_delete, rename_root_to_sub)에서 항상 최신 일관 상태로 복구(p.13).

## 섹션 노트
- **§1 Introduction**: WOFS 모델 + 4가지 핵심 기술(package 생성/파싱/조직/복구) 제시. 기여 3가지(기존 설계 원리 분석, WOFS 모델 제안, WOLVES 구현·평가).
- **§2 Background**: Table 1로 JFS/CK/LFS/SSU/WOFS의 MOrd·MIOs·GC 비교. WOFS만 MIOs≈1, MOrd=1, Reuse-GC.
- **§3 Observations & Motivations**: PMFS/SplitFS/NOVA의 I/O path 분석(Fig.1). metadata I/O가 22.9~76.5% 시간 점유, random I/O amplification 2.8×, NOVA GC가 metadata access ~13/block. Insight: 흩어진 metadata object 오케스트레이션이 근본 문제.
- **§4 WOFS Model**: package 설계, PTL, non-log layout/reclamation, coarse persistence.
- **§5 Implementation**: WOLVES 아키텍처(Fig.5), 연산별 package 매핑(Table 3), 복구, 최적화.
- **§6 Evaluation**: 위 평가 토글.
- **§7~9 Discussion/Related/Conclusion**: data checksum은 user-to-kernel copy 오버헤드(~40.1%)로 미채택; checkpoint는 foreground I/O 간섭으로 미사용. LFS와의 차별점은 연산별 aggregated package. 소스: https://github.com/WOFS-for-PM/

## 핵심 용어
- **WOFS (metadata write-once file system)**: 파일 연산마다 특정 metadata를 체크섬 보호 package로 생성해 단일 ordering point로 한 번만 쓰는 파일 시스템 모델.
- **WOLVES**: WOFS의 Linux 커널 프로토타입(12,000+ LoC).
- **Package**: 한 파일 연산의 모든 metadata를 모은 체크섬 보호 단위(header에 magic/type/ts/CRC32). atomic 4종(Create/Write/Attr/Unlink) + compound.
- **PCOMMIT**: `clwb + sfence`로 PM 영속화하는 단일 ordering point 연산.
- **Ordering point / metadata I/O**: crash consistency를 위한 쓰기 순서 강제 지점 / 메타데이터 갱신 I/O. WOFS는 각각 1개·~1개로 최소화.
- **Forward-pointer**: compound package의 sub-package들을 순서대로 연결해 부분 기록을 복구 가능케 하는 header 내 포인터.
- **PTL (Package Translation Layer)**: package를 파싱해 inode/dentry 등 전통 metadata object 추상을 제공하고 reclamation을 관리하는 계층.
- **Order map**: create pkg 주소 → 그것을 무효화한 unlink pkg 주소 매핑(unlink pkg reclaim 추적).
- **Coarse Persistence (CP)**: 4 KiB pkg-group을 bitmap 1비트로 표현해 빠른 복구를 위해 package 위치를 coarse하게 영속화.
- **Reuse-GC vs Copy-GC**: 무효 공간을 free처럼 재할당(WOFS) vs valid 데이터를 복사 압축(LFS).
- **Causal order**: 연산 간 인과 의존성 순서. WOFS crash safety는 issue order가 아닌 causal order를 보존.

## 강점 · 한계 · 열린 질문
- **강점**: metadata I/O와 ordering point를 이론적 최소(1)로 압축해 PM 대역폭 상한에 근접; non-log layout으로 LFS GC 오버헤드 제거; formal proof로 data+version consistency 보장; PM뿐 아니라 MS-SSD에서도 일반성 입증.
- **한계**: thread≥9에서 PM 하드웨어 동시성 한계로 성능 저하; data checksum 미적용(data consistency는 CoW로만); aging 시 defragmentation 기법 부재(future work); rename 등 compound는 VFS traversal 의존으로 이득 작음; PTL 유지에 메모리 상시 점유(0.3~1.6%).
- **열린 질문**: idle 시 PTL checkpoint로 복구 더 가속 가능한가? PM-DRAM 하이브리드 PTL로 메모리 줄이면서 성능 유지 가능한가(논문은 RW 3.1~12.5% 저하 보고)? 다중 노드·PM array(OdinFS/ArckFS류)와 결합 시 효과는?

## ❓ Q&A (자가 점검)
> [!question]- Q1. WOFS가 기존 PM crash consistency 기법과 다른 근본 아이디어는?
> 답: inode·dentry·log tail 등 흩어진 metadata object를 각각 순서 맞춰 갱신하는 대신, 한 파일 연산의 모든 metadata를 하나의 체크섬 보호 package로 모아 단일 ordering point(PCOMMIT 1회)로 "한 번만" 쓴다. metadata I/O ≈ 1, ordering point = 1.

> [!question]- Q2. package의 atomicity와 durability는 어떻게 보장되나?
> 답: header에 magic number·type·timestamp·CRC32를 두고 PCOMMIT(clwb+sfence)으로 영속화한다. 부분 기록된 package는 복구 시 checksum 불일치로 감지·폐기되므로 별도 journal/commit block 없이 atomic·durable하다.

> [!question]- Q3. 4종 atomic package와 compound package의 역할은?
> 답: Create(256B, 새 inode), Write(64B, 새 data block), Attr(64B, 속성 변경), Unlink(64B, link 감소). rename·symlink 같은 복합 연산은 sub-package를 forward-pointer로 연결한 compound package로 표현해 부분 기록도 복구 가능하게 한다.

> [!question]- Q4. log를 안 쓰면 무효 공간 회수(GC)는 어떻게 하나?
> 답: malloc식 non-log layout으로 배치하고, 무효화된 package를 free처럼 reuse(Reuse-GC). unlink pkg가 create pkg를 무효화하면 free 표시 후 재할당하며, unlink pkg 자신은 order map으로 추적해 가리키던 create pkg 재할당 후 회수한다. LFS의 copy 기반 GC가 없다.

> [!question]- Q5. Coarse Persistence가 왜 필요하고 어떻게 동작하나?
> 답: non-log layout이라 package가 PM 전역에 흩어져 복구 시 위치 파악이 어렵다. 개별 package를 bitmap에 등록하면 critical path에 small random I/O가 생기므로, 4 KiB pkg-group 단위로 bitmap 1비트만 두어(약 0.012% 공간) 복구 시 group을 찾아 내부 valid package를 probe하고 PTL을 재구축한다. m_cp와 package 사이엔 ordering이 불필요하다.

> [!question]- Q6. WOFS의 crash safety가 보존하는 순서는 무엇인가?
> 답: issue order가 아니라 causal order다. 예: Thread-1이 create C1 후 write W1, Thread-2가 write W2를 다른 파일에 했을 때(C1<W2<W1), crash로 W2가 부분 기록돼도 C1·W1만 valid면 인과 순서가 보존되어 정상이다. W2는 W1과 인과 의존이 없어 복구에 불필요.

> [!question]- Q7. 대표 성능 수치는?
> 답: 기존 PM 파일 시스템 대비 1.20~6.73× throughput, SW에서 PM write 대역폭의 97.3~99.1% 달성. Filebench는 MadFS 대비 9.14~61.4×. 최악 복구도 ~21.6s로 전통 fsck보다 빠르다.

> [!question]- Q8. 왜 data까지 checksum/write-once로 만들지 않았나?
> 답: data checksum은 큰 콘텐츠에서 user-to-kernel copy와 CRC32/xxHash 연산 오버헤드(~40.1%/~32.3%)가 커서 fast PM에서 이득보다 손해다. 게다가 WOFS는 data checksum 없이도 PM 대역폭 상한에 도달하므로, data consistency는 CoW write로 보장하고 checksum은 future work로 남겼다.

## 🔗 Connections
[[File System]] · [[OSDI]] · [[2025]]

## References worth following
- NOVA: A Log-structured File System for Hybrid Volatile/Non-volatile Main Memories (FAST 2016, [21]) — 대표 PM LFS, 주요 비교군.
- SplitFS: Reducing Software Overhead in File Systems for Persistent Memory (SOSP 2019, [20]) — transactional checksum 기반, write-once 비교 기준.
- SquirrelFS: Using the Rust Compiler to Check File-System Crash Consistency (OSDI 2024, [25]) — synchronous soft update(SSU) 대표.
- Optimistic Crash Consistency (SOSP 2013, [15]) / Consistency Without Ordering ([37]) — ordering 제거·checksum 기반 일관성의 사상적 기반.
- A Logic of File Systems (FAST 2005, [54]) / Consistency Without Ordering TR1709 ([55]) — Appendix A formal proof의 논리 프레임워크.
- MadFS: Per-File Virtualization for Userspace Persistent Memory Filesystems (FAST 2023, [28]) — 최신 user-space PM LFS 비교군.

## Personal annotations
<본인 메모 영역>
