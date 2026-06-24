---
title: "DJFS: Directory-Granularity Filesystem Journaling for CMM-H SSDs"
aliases: [DJFS]
description: "디렉터리 단위로 저널 트랜잭션을 정의해 CMM-H SSD에서 파일시스템 저널링의 lock contention, 트랜잭션 lock-up, conflict, serial commit을 동시에 완화하는 EXT4 기반 저널링 파일시스템"
venue: FAST
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/cxl
  - topic/cmm-h
  - topic/journaling
  - topic/filesystem
  - venue/fast
  - year/2025
---

# DJFS: Directory-Granularity Filesystem Journaling for CMM-H SSDs

> **FAST 2025** · `cluster/cxl` · Source: [DJFS - Directory-Granularity Filesystem Journaling for CMM-H SSDs.pdf](DJFS%20-%20Directory-Granularity%20Filesystem%20Journaling%20for%20CMM-H%20SSDs.pdf)

저자: Seung Won Yoo, Myeongin Cheon, Bonmoo Koo, Youjip Won (KAIST); Joontaek Oh (University of Wisconsin–Madison); Wonseb Jeong, Hyunsub Song, Hyeonho Song, Donghun Lee (Samsung Electronics)

## TL;DR
DJFS는 저널 트랜잭션을 "디렉터리 단위(per-directory)"로 정의하는 저널링 파일시스템이다. 8개 인기 애플리케이션의 파일 접근 패턴이 대부분 자신의 디렉터리를 중심으로 일어난다는 관찰에 기반해, path-based transaction selection / transaction coalescing / transaction conflict resolution 세 기법으로 lock contention, 트랜잭션 lock-up, conflict, serial commit을 동시에 완화한다. CMM-H(CXL Memory Module-Hybrid) SSD에 EXT4(Linux 5.18) 위에 구현했고, 최신 기법 FastCommit 대비 Varmail 4.5×, MDTest 2.5×, Exim 3.7× throughput을 달성하면서 RocksDB-fillsync에서는 비슷한 성능을 유지한다.

## 문제 & 동기
파일시스템 저널링(Write-Ahead-Logging)은 멀티코어가 늘고(수백 코어) 스토리지가 빨라지면서 성능 병목이 된다. 저자들은 저널링 throughput을 막는 네 가지 원인을 든다: (i) lock contention(global 저널 구조의 락 경합), (ii) transaction lock-up(fsync 후 commit 동안 새 메타데이터 업데이트가 차단되는 구간), (iii) transaction conflict(여러 동시 트랜잭션이 같은 블록을 갱신), (iv) serial journal commit(JBD가 트랜잭션을 직렬로 commit).

특히 CMM-H 같은 빠른 메모리 시맨틱 SSD에서는 long write latency가 사라지면서 그동안 가려져 있던 transaction lock-up이 전체 트랜잭션 지연의 더 큰 비중을 차지하게 된다. 저자들은 cache-line granularity(64B) 저널링을 naive하게 도입하면 commit latency는 50% 줄지만 throughput은 20% 미만으로만 개선되고, lock-up 시간은 오히려 70% 길어진다는 점을 측정으로 보인다.

> [!quote]- 📄 원문 표현 (paper)
> "As the storage gets quicker, the transaction lock-up begins to account for a larger fraction of the entire transaction latency. We carefully argue that the transaction lock-up should be handled with more care in designing the journaling filesystem." (p.2)
>
> "The performance improvement of using finer journaling granularity from 4 KByte to 64 bytes is not as substantial as we expected. The performance improvement is merely 20%. ... We find that the transaction lock-up time becomes 70% longer as we use cache-line granularity journaling." (p.4)

## 핵심 통찰

> [!note]- 핵심 통찰 1: finer granularity만으로는 부족하고, lock-up이 진짜 병목
> cache-line granularity 저널링은 commit latency를 줄이지만 throughput 이득은 미미(1.1×~1.3×)하다. 근본 원인은 transaction lock-up 오버헤드 증가다. block granularity에서 lock-up은 commit latency의 6%지만, cache-line granularity에서는 commit latency가 35% 줄어드는 대신 lock-up 비중이 18%로 커진다(p.4, p.5).

> [!note]- 핵심 통찰 2: 파일 갱신은 디렉터리 중심으로 일어난다
> 8개 애플리케이션(Exim, RocksDB, SQLite, MySQL, Git, Mercurial, VMware, HDFS) 분석에서 세 가지 공통 성질을 발견: Dedicated Directory(앱은 전용 디렉터리에서 동작, 스레드별로 분리), Directory Update(파일 내용 갱신이 파일 생성/삭제와 부모 디렉터리 갱신을 동반), Shared Directory(같은 디렉터리에 속한 여러 파일을 함께 갱신). 따라서 directory가 저널 트랜잭션의 자연스러운 단위가 된다(p.3, §3.1).

> [!note]- 핵심 통찰 3: per-directory 트랜잭션이 네 병목을 한 번에 공략
> Property D(전용 디렉터리) 덕분에 한 트랜잭션이 lock-up 되어도 다른 디렉터리의 파일 연산은 영향받지 않는다(lock contention, lock-up 완화). per-directory로 정의하면 서로 관련된 파일 연산만 한 트랜잭션에 묶여 트랜잭션이 작아지고(conflict 감소, commit latency 감소), 여러 running transaction을 병렬 commit할 수 있다(serial commit 완화)(p.6, §4.1).

## 설계 / 메커니즘

> [!abstract]- 전체 구조와 데이터 구조
> DJFS는 디렉터리당 최대 1개의 running transaction과 1개의 committing transaction을 가진다(디렉터리 inode에 두 개의 transaction pointer). 메타데이터(inode, file map, directory entry)만 저널링하고 crash recovery 시 재구성한다. 저널 영역은 단일 circular array로, CMM-H 내부 DRAM에 캐시될 만큼 작게(평가에서 100MB) 잡아 CXL.mem 속도로 접근한다(p.7, §5.1).
>
> Log Record: inode는 256B physical logging, index block과 directory block은 64B(cache-line) differential logging. block을 64B 단위로 나눠 bitmap으로 갱신 위치를 표현하고, shadow page에 갱신 내용을 기록한다. on-disk log record는 16B header + 192B bitmap+data 형태(Figure 9, p.8). in-memory transaction은 log record 집합 + page cache entry 집합 + inode 번호 + operation count를 유지. on-disk transaction은 header + log records + commit record(8B magic number)(Figure 10/11, p.8-9).

> [!abstract]- (1) Path-based Transaction Selection
> 파일이 여러 hard link(여러 부모 디렉터리)를 가질 수 있으므로, 파일 연산은 인자로 받은 path 문자열의 마지막 component의 부모 디렉터리 inode의 running transaction에 업데이트를 삽입한다. POSIX 연산은 path 또는 fd를 받는데, fd의 경우 struct file의 dentry로 path를 식별한다. inode만 갱신하는 연산(chmod(/d1/f1,777))은 d1의 running transaction에, truncate(/d1/f1)은 inode와 index block을 d1에 삽입. 예외: mkdir(path)는 새 디렉터리의 부모 트랜잭션에, link(old,new)는 new의 부모 디렉터리 트랜잭션에 삽입(p.6-7, §5.3, Figure 12).

> [!abstract]- (2) Transaction Coalescing
> 한 파일 연산이 서로 다른 두 디렉터리의 파일을 갱신할 때(예: rename) failure-atomicity가 깨질 수 있다. 두 트랜잭션을 하나로 병합하되, 둘 중 inode 번호가 더 작은 쪽을 master로, 다른 쪽을 subordinate로 지정한다. master는 subordinate들에 대한 포인터를 유지하고, coalesced transaction의 모든 트랜잭션은 atomic하게 commit된다. 절차: ① 두 inode의 spinlock을 inode 번호 오름차순으로 획득(deadlock 방지), ② subordinate의 operation count를 master에 더함, ③ subordinate의 새 파일 연산을 master로 redirect(reference pointer), ④ subordinate를 master의 subordinate list에 삽입, ⑤ spinlock 해제(p.10, §5.4, Figure 13).

> [!abstract]- (3) Resolving the Transaction Conflict
> 각 inode는 두 개의 container transaction pointer(running용/committing용)를 가져, 해당 inode의 메타데이터 shadow copy가 어느 트랜잭션에 속하는지 가리킨다. 파일 연산이 이미 다른 트랜잭션에 속한 메타데이터를 갱신하려 하면 conflict로 감지한다. 두 종류: R-to-R(running↔running)은 transaction coalescing으로 해소, R-to-Cmt(running↔committing)은 ordered commit으로 해소(committing transaction이 durable해질 때까지 running transaction의 commit을 지연; running transaction의 conflict list에 committing transaction을 삽입)(p.10-11, §5.5, Figure 14).

> [!abstract]- 구현 (EXT4 / Linux 5.18)
> 메타데이터 업데이트 4단계: ① 객체 락 획득(inode rw semaphore), ② journal area 예약, ③ running transaction 선택(lock-up 시 committing이 될 때까지 대기), ④ log record 삽입. Transaction Commit: commit thread(fsync 또는 kworker)가 진행 중인 commit이 끝나길 기다린 뒤 running list에서 제거→committing으로 전환, log record를 durable화 후 commit record 작성, 여러 스레드로 병렬 commit. Checkpoint: 저널 영역이 부족하거나 checkpoint thread가 깨어날 때, 모든 running transaction을 commit한 뒤 dirty page를 disk에 반영. Crash Recovery: 저널 영역 스캔→committed but not checkpointed 트랜잭션을 checkpoint→메타데이터 재구성. counter 메타데이터는 shadow counter를 저널링해 crash-consistent하게 복구(p.11-12, §6).

## 평가

> [!success]- 환경 및 주요 수치
> **환경**: EXT4 over Linux 5.18, 2TB CMM-H prototype, 88 core(2×44 Sapphire Rapids), 128GB memory. 비교 대상: JBD(mem, cache-line granularity JBD), FastCommit, Z-Journal, CJFS, iJournaling. 벤치마크: Varmail, MDTest, Exim, RocksDB-fillsync. EXT4/iJournaling/FastCommit은 CXL.mem 인터페이스로 포팅. Z-Journal은 172GB 저널 필요로 CMM-H DRAM에 안 맞고, CJFS는 order-preserving I/O stack 기반이라 포팅 안 함(p.12, §7.1).
>
> **Manycore scalability (80 threads)** (Figure 15, p.12-13): Varmail에서 DJFS가 JBD(mem) 4.3×, FastCommit 4.5×, Z-Journal 4.3×, iJournaling 5.7×, CJFS 3.1× 우위.
>
> **Transaction lock-up (80 threads, Varmail)** (Figure 16, p.13): 평균 lock-up이 FastCommit 211us, JBD(mem) 120us, CJFS 65us, Z-Journal 39us, iJournaling 0.2us, **DJFS 26us**.
>
> **Transaction conflict** (Figure 17, p.13): Varmail에서 평균 conflict/tx가 Z-Journal 2.8, **DJFS/iJournaling은 거의 0**.
>
> **fsync() latency (Varmail, 80 threads)** (Figure 18, p.14): DJFS 0.2ms, iJournaling 0.16ms, JBD(mem) 0.9ms, FastCommit 0.56ms, CJFS 3.6ms, Z-Journal 8.3ms. 트랜잭션 크기: DJFS 1.6KB, iJournaling 5.2KB, JBD(mem) 18.3KB, FastCommit 1.8KB, CJFS 324KB, Z-Journal 32.6KB. FastCommit은 트랜잭션 크기가 비슷한데도 lock-up이 8× 길어 DJFS보다 2.8× 긴 latency.
>
> **Coalescing degree (file op/tx)** (Table 3, p.14): DJFS는 Varmail 4.2, MDTest 2.0, Exim 1.9, fillsync 1.0 → iJournaling 대비 평균 2.4× 더 많은 파일 연산을 한 트랜잭션에 담음.
>
> **메모리 소비** (Table 4, p.15): DJFS transaction structure는 13.6MB(Varmail)~22.1MB(RocksDB)로 JBD/CJFS(KB 단위)보다 두 자릿수 크지만 서버 메모리 기준 허용 범위. log record(shadow copy)는 250KB 미만. transaction structure 1개 크기는 240B.
>
> **Crash recovery** (§7.4, p.15): CrashMonkey/xfstests(35, 106, 321 + rename_root_to_sub, create_delete) 1,000 테스트 통과. Varmail 80 threads force shutdown 시 복구 2.5초.
>
> **Cache miss** (§8, p.15): 모든 접근이 cache miss여도 DJFS는 block granularity JBD 대비 1.24× throughput. cache-line access 없이 plain block granularity로 동작 시 1.96×.

## 섹션 노트
- **§2 Background**: EXT4/JBD의 트랜잭션 3상태(running/committing/checkpointing)와 4대 병목(lock contention, lock-up, conflict, serial commit). CMM-H는 CXL.mem(64B, 0.6us latency, 2GB/s)와 CXL.io(4KB, 10us, 4GB/s) 둘 다 지원하는 메모리 시맨틱 SSD. throughput이 내부 DRAM cache hit rate에 크게 의존(working set 8GB에서 16MIOPS→50% 하락, dataset 크기에 따라 최대 74× 변동)(Figure 3, p.4).
- **§2.2 Scalability 분류**: per-core(ScaleFS, Z-Journal), per-partition(IceFS, SpanFS), per-file(iJournaling), per-commit/concurrent(CJFS). CJFS만 trylock으로 conflict로 인한 extended lock-up을 다루지만 commit latency 증가.
- **§3.1**: 8개 앱의 파일 연산 패턴(Figure 4). Exim journaling/update mail, RocksDB switching manifest/compaction, SQLite undo logging, MySQL add log file, Git add and commit, Mercurial changeset, VMware write & flush, HDFS checkpoint. 모두 atomic rename, multi-file transaction, fsync 다발을 사용.
- **§4.2 Challenges**: (i) multiple link 파일, (ii) multi-directory atomic 갱신, (iii) transaction conflict 해소(Figure 7).
- **§9 Related Work**: Scalable Journal(IceFS, SpanFS, Z-Journal, MQFS, XFS, CJFS), Reduce commit latency(iJournaling, FastCommit, BarrierFS, HORAE), NVM FS(BPFS, NOVA, Aerie, SplitFS, MadFS), Heterogeneous FS(Ziggurat, SPFS, Strata).

## 핵심 용어
- **CMM-H (CXL Memory Module-Hybrid)**: CXL.mem(cache-line, 64B, 저지연)과 CXL.io(block, 4KB, 고대역폭)를 모두 지원하는 메모리 시맨틱 SSD. 상위 DRAM이 하위 NAND 내용을 캐시하는 계층 구조라 접근 지연이 내부 DRAM cache hit rate에 좌우됨. HDM(Host-managed Device Memory)을 NVMe 스토리지와 1:1로 노출.
- **Per-directory transaction**: 저널 트랜잭션을 디렉터리 단위로 정의. 디렉터리 inode가 running/committing transaction pointer를 하나씩 가짐.
- **Transaction lock-up**: fsync 호출 후 commit이 시작되면, 진행 중 파일 연산이 끝날 때까지 새 메타데이터 갱신이 차단되는 구간. 빠른 스토리지일수록 전체 지연에서 비중이 커짐.
- **Path-based Transaction Selection**: 파일 연산이 인자 path의 마지막 component의 부모 디렉터리 inode의 running transaction을 선택해 업데이트를 삽입하는 기법.
- **Transaction Coalescing**: 서로 다른 디렉터리 트랜잭션 두 개를 master/subordinate로 묶어 atomic하게 commit. R-to-R conflict 해소에도 사용.
- **Container transaction**: 한 inode의 메타데이터 shadow copy가 속한 트랜잭션. inode마다 running용/committing용 두 포인터로 conflict 감지.
- **R-to-R / R-to-Cmt conflict**: running↔running conflict(coalescing으로 해소) / running↔committing conflict(ordered commit으로 해소).
- **Differential logging**: index/directory block을 64B 단위로 분할해 갱신된 부분만 bitmap+shadow page로 기록(inode는 256B physical logging).

## 강점 · 한계 · 열린 질문
- **강점**: 단일 설계(per-directory)로 lock contention, lock-up, conflict, serial commit 네 병목을 동시에 공략. 실제 8개 앱의 파일 접근 패턴 측정에 기반해 동기가 설득력 있음. EXT4에 실구현 + CMM-H 실제 prototype 평가 + CrashMonkey/xfstests로 crash consistency 검증. cache miss 상황에서도 JBD 대비 우위.
- **한계**: transaction structure 메모리가 JBD/CJFS보다 두 자릿수 큼(checkpointing transaction이 page cache entry를 늦게 해제). RocksDB-fillsync에서는 FastCommit 대비 5% 느림(shadow copy 생성 오버헤드). Z-Journal/CJFS는 CMM-H에 포팅 불가/안 함이라 동일 인터페이스 직접 비교가 일부 제한적.
- **열린 질문**: 디렉터리 수가 매우 적은(공유 디렉터리 집중) 워크로드에서는 per-directory 병렬성이 약화될 텐데 그 경계는? CMM-H가 아닌 일반 NVMe/PMEM에서 per-directory 이득이 어느 정도 유지되는가? coalescing degree가 높아질수록 atomic commit 단위가 커져 lock-up이 다시 늘 가능성은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. DJFS가 트랜잭션을 디렉터리 단위로 정의하는 근거는 무엇인가?
> 8개 인기 애플리케이션 분석 결과 (i) 각 앱이 전용 디렉터리에서 동작(Dedicated Directory), (ii) 파일 내용 갱신이 파일 생성/삭제 + 부모 디렉터리 갱신을 동반(Directory Update), (iii) 같은 디렉터리의 여러 파일을 함께 갱신(Shared Directory)한다는 세 공통 성질을 발견했기 때문. 따라서 directory가 서로 관련된 파일 연산을 묶는 자연스러운 트랜잭션 단위가 된다.

> [!question]- Q2. cache-line granularity 저널링만으로 충분하지 않은 이유는?
> commit latency는 50%(또는 35%) 줄지만 throughput 이득은 20% 미만(1.1~1.3×)에 그친다. 빠른 스토리지에서 가려져 있던 transaction lock-up이 드러나, cache-line granularity로 가면 lock-up 시간이 70% 길어지고 commit latency 대비 비중이 6%→18%로 커지기 때문이다.

> [!question]- Q3. per-directory 트랜잭션이 네 가지 병목을 어떻게 동시에 완화하는가?
> Property D(전용 디렉터리)로 한 디렉터리 트랜잭션이 lock-up 되어도 다른 디렉터리 연산은 진행 가능 → lock contention과 lock-up 완화. 관련된 파일 연산만 한 트랜잭션에 묶여 트랜잭션이 작아짐 → conflict와 commit latency 감소. 디렉터리별로 여러 running transaction을 병렬 commit → serial commit 완화.

> [!question]- Q4. 한 파일이 여러 hard link를 가질 때 어느 트랜잭션을 선택하는가?
> Path-based Transaction Selection으로 파일 연산이 받은 path 문자열의 마지막 component의 부모 디렉터리 inode의 running transaction에 업데이트를 삽입한다. fd를 받으면 struct file의 dentry로 path를 식별한다. link(old,new)는 new의 부모, mkdir(path)는 새 디렉터리의 부모 트랜잭션을 사용한다.

> [!question]- Q5. rename처럼 두 디렉터리를 갱신하는 연산의 atomicity는 어떻게 보장하는가?
> Transaction Coalescing으로 두 디렉터리 트랜잭션을 하나로 병합한다. inode 번호가 작은 쪽을 master, 다른 쪽을 subordinate로 정하고, deadlock 방지를 위해 inode spinlock을 번호 오름차순으로 획득한 뒤 operation count 합산, 새 연산 redirect, subordinate list 삽입을 거친다. coalesced transaction의 모든 트랜잭션은 atomic하게 commit된다.

> [!question]- Q6. transaction conflict 두 종류와 각 해소 방법은?
> R-to-R(running↔running): 같은 메타데이터를 두 running transaction이 갱신 → transaction coalescing으로 병합. R-to-Cmt(running↔committing): running transaction이 committing transaction의 메타데이터를 갱신 → committing transaction이 durable해질 때까지 running transaction의 commit을 지연하는 ordered commit으로 해소(running transaction의 conflict list 사용).

> [!question]- Q7. DJFS의 메모리 소비가 다른 저널링보다 큰 이유와 그 정도는?
> DJFS transaction structure는 13.6~22.1MB로 JBD/CJFS(KB 단위)보다 두 자릿수 크다. checkpointing transaction이 page cache entry를 늦게(checkpoint 완료 시까지) 해제하기 때문이다. transaction structure 1개는 240B, log record(shadow copy)는 250KB 미만이며, 저자들은 서버 메모리 기준 허용 범위라고 본다.

> [!question]- Q8. FastCommit 대비 fsync latency가 짧은 핵심 이유는?
> 트랜잭션 크기는 DJFS 1.6KB, FastCommit 1.8KB로 거의 같지만, FastCommit의 transaction lock-up이 DJFS보다 8× 길다(211us vs 26us, Varmail 80 threads). lock-up 차이 때문에 FastCommit의 fsync latency가 DJFS보다 2.8× 길어진다.

## 🔗 Connections
[[CXL]] · [[FAST]] · [[2025]]

## References worth following
- **iJournaling** (Park & Shin, ATC'17) [48]: per-file 트랜잭션으로 fsync latency 감소. DJFS가 직접 비교/대비하는 핵심 baseline.
- **FastCommit** (Shirwadkar et al., ATC'24) [51]: resource-efficient 저널링, DJFS의 최신 SOTA 비교 대상.
- **CJFS: Concurrent Journaling** (Oh et al., FAST'23) [47]: trylock으로 conflict 기반 extended lock-up을 다룬 유일한 선행 연구.
- **Z-Journal: Scalable Per-Core Journaling** (Kim et al., ATC'21) [32]: per-core 저널링 baseline.
- **Overcoming the Memory Wall with CXL-Enabled SSDs** (Yang et al., ATC'23) [66]: CMM-H류 CXL-SSD의 DRAM cache hit rate 의존성 특성화.
- **Samsung MSL. CXL Memory Module - Hybrid (CMM-H)** [50]: 본 논문 타깃 디바이스 자료.

## Personal annotations
<본인 메모 영역>
