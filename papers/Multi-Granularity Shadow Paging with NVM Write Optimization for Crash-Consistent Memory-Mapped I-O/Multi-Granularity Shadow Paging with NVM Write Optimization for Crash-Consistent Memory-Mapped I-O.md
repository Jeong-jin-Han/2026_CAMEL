---
title: "Multi-Granularity Shadow Paging with NVM Write Optimization for Crash-Consistent Memory-Mapped I/O"
description: "NVM 기반 memory-mapped I/O를 위해 redo/undo 로그를 상호 전환하는 shadow logging과 radix-tree 기반 다중 입도(multi-granularity) 로그 관리로 write amplification과 double-write를 동시에 줄인 crash-consistent 메커니즘"
venue: HPCA
year: 2023
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/fs
  - venue/hpca
  - year/2023
  - list/26s-v2
  - topic/nvm
  - topic/crash-consistency
  - topic/shadow-paging
  - topic/mmio
---

# Multi-Granularity Shadow Paging with NVM Write Optimization for Crash-Consistent Memory-Mapped I/O

> **HPCA 2023** · cluster/fs · Source: [Multi-Granularity Shadow Paging with NVM Write Optimization for Crash-Consistent Memory-Mapped I-O.pdf](<Multi-Granularity Shadow Paging with NVM Write Optimization for Crash-Consistent Memory-Mapped I-O.pdf>)

저자: Hongchao Du (City University of Hong Kong), Qiao Li (Xiamen University), Riwei Pan (City University of Hong Kong), Tei-Wei Kuo (National Taiwan University), Chun Jason Xue (City University of Hong Kong)

## TL;DR
NVM 상의 memory-mapped I/O(MMIO)는 커널 스택 우회로 고성능을 낼 수 있지만 crash consistency를 위한 logging/shadow paging은 double-write나 write amplification을 초래한다. MGSP(Multi-Granularity Shadow Paging)는 redo/undo 로그의 역할을 상황에 따라 바꿔주는 "shadow logging" 개념으로 double-write를 없애고, radix tree 기반 Multi-granularity Shadow Log(MSL)로 요청 크기에 맞는 로그 입도를 선택해 write amplification과 metadata overhead를 동시에 줄인다. 여기에 bitmap 기반 lock-free metadata log와 Multiple Granularity Locking(MGL)을 더해 강한 consistency와 높은 concurrency를 함께 확보한다. Libnvmmio 기반 구현으로 FIO에서 Ext4-DAX/Libnvmmio/NOVA 대비 1.1~4.21배(write), 멀티스레드에서 2.56~3.76배 향상을 보였고, SQLite에서 Mobibench 29.4%, TPCC 36.5% 성능 향상을 달성했다.

## 문제 & 동기
NVM의 byte-addressability 덕에 DAX 파일시스템은 페이지 캐시를 우회해 raw NVM 성능을 그대로 활용할 수 있지만(p.108), crash consistency를 위한 두 주류 기법 모두 한계가 있다. Logging은 실제 갱신 전에 old/new data를 기록하는데, undo/redo 로깅 모두 "double-write" 문제(로그 한 번, 실제 데이터 한 번)를 가지고, 고정 단위(페이지/블록) 로깅은 일부만 바뀌어도 전체 블록을 로깅해야 하는 write amplification을 겪는다(p.109). Shadow paging(CoW)은 double-write는 피하지만 입도가 크면(huge page 등) write amplification이 발생하고, 트리 기반 인덱싱에서는 leaf에서 root까지 메타데이터를 전파해야 하는 wandering tree 문제가 생긴다(p.109). 기존 대표작 Libnvmmio는 hybrid logging + differential logging으로 완화하지만, fsync 기반 checkpointing 때문에 sync가 잦은 워크로드(예: 데이터베이스)에서는 double-write 문제가 되살아나 성능이 급락한다(Fig.1, p.108).

> [!quote]- 📄 원문 표현 (paper)
> - "Logging with fixed units like a page or a block may cause write amplification, where the entire data block must be logged, even if only a small part is updated." (p.108)
> - "Even though write amplification is alleviated with differential logging, the double-write problem still exists with frequent sync operations." (p.109)
> - "Shadow paging could suffer from severe write amplification problems when the access granularity is small... In addition, shadow paging needs to constantly modify the data pointer, which could cause TLB-shootdown." (p.108)

## 핵심 통찰 (Key Insight)
1. **Shadow logging = redo/undo 로그의 역할을 동적으로 교환**: 같은 데이터 블록에 연속으로 두 번 쓰기가 일어나면, 첫 쓰기의 redo 로그(새 데이터 $d_1$)가 두 번째 쓰기 시점에는 이미 "old data" 역할을 하게 되어 undo 로그의 조건을 만족한다. 따라서 로그를 새로 만들지 않고 그대로 재사용하면 두 번의 쓰기 요청을 데이터 블록 두 번 쓰기만으로 완결할 수 있어(zero-copy) double-write를 근본적으로 없앤다.
2. **Radix tree 기반 multi-granularity 관리**: 트리의 각 레벨이 서로 다른 로그 입도(블록 단위부터 파일 전체까지, 예: 4K/8K/16K/32K)를 담당하고, 필요한 노드만 on-demand로 생성한다. 이를 통해 작은 갱신엔 fine-grained 로그로 write amplification을 줄이고, 큰 갱신엔 coarse-grained 로그로 metadata overhead를 줄이는 상반된 요구를 동시에 만족시킨다.
3. **Bitmap(valid/existing bit) + lock-free metadata log로 8-byte atomic 갱신**: NVM은 8-byte 단위 atomic write만 보장하므로, 메타데이터(비트맵) 자체의 원자성을 보장하기 위해 별도의 lock-free metadata log를 먼저 기록한 뒤 비트맵을 갱신한다. hash+CAS 기반 per-thread 로그 엔트리로 global lock 경쟁 없이 atomicity를 확보한다.

> [!quote]- 📄 원문 표현 (paper)
> - "The core of the logging idea is to keep a copy of old or new data in the log before writing file data... In the process, we only write two data blocks to complete two write operations and achieve zero copy. We call this kind of log that mixes redo and undo logging as the shadow log." (p.109-110)
> - "A radix tree-based structure is introduced to manage the logs of different granularities. Each level of the tree is responsible for one granularity... From the root node to the leaf node, the granularity of the log decreases sequentially." (p.110)
> - "To avoid lock competition caused by global metadata, MGSP uses hash and compare-and-swap (CAS) instructions to achieve lock-free metadata logging." (p.113)

## 설계 / 메커니즘 (Design)
MGSP는 user-space 라이브러리로 동작하며 Libnvmmio를 기반으로 read-write 흐름을 재설계했다(p.113-114, Fig.2). 구성 요소는 **Multi-granularity Shadow Log(MSL)**, **Multi-granularity Locking(MGL)**, **lock-free metadata log** 세 가지다.

- **MSL (radix tree)**: 쓰기 발생 시 offset·size에 맞는 granularity를 선택해 로그를 쓴다. 트리 루트 노드는 파일 전체 mmap이며, 자식 노드는 필요할 때만 생성된다(Fig.4, p.110-111). 예: 32KB 파일에 32KB→16KB(offset)→14KB 세 번 쓰기가 들어오면 4KB 로그 2개 + 8KB 로그 1개 조합으로 처리되고, fine-grained 로그는 재사용되어 낭비가 없다(p.111, Alg.1).
- **최소 갱신 입도(minimum update granularity)**: leaf 노드의 valid bit을 여러 비트로 확장해 leaf 내부를 더 잘게(예: 4KB leaf를 2KB 단위로) 표현할 수 있다(p.111-112).
- **Bitmap 기반 metadata**: 비-leaf 노드는 valid bit(현재 노드 로그 유효)과 existing bit(자손 노드에 로그 존재) 2비트를 가지며(Fig.5, p.112), leaf 노드는 valid bit만 여러 개 가진다. Lazy cleaning으로 coarse-grained 갱신 시 하위 트리 전체를 무효화하지 않고 갱신된 노드만 무효화해 이후 쓰기로 비용을 분산시킨다(p.113).
- **최소 검색 트리(minimum search tree)**: locality를 이용해 이전 접근을 커버하는 가장 작은 서브트리를 캐싱해 루트부터 매번 검색하는 비용을 줄인다(p.111).
- **Lock-free metadata log**: 128바이트 고정 크기 로그 엔트리(tid, inumber, offset, len, fsize, checksum, bitmap[10])를 스레드 ID 해시로 얻고, 충돌은 linear probing으로 해결한다(Fig.6, p.113). 비트맵 변경 전 반드시 metadata log를 먼저 persist해 크래시 시에도 원자성을 보장한다.
- **MGL (Multiple Granularity Locking)**: 트리의 서로 다른 레벨을 잠가 range lock을 근사하며, Intention Read(IR)/Intention Write(IW) 락을 도입해 서로 다른 granularity 락 간 충돌을 조정한다(Table I, p.114). Lazy cleaning for intention lock으로 매 연산마다 lock-unlock을 반복하지 않고, greedy locking으로 파일 reference가 1개뿐일 때는 minimum search tree 루트 노드만 잠가 range lock 효과를 낸다(p.114).
- **Write/Read/Close-Recovery 흐름**: write는 metadata log→data 기록(checksum 계산)→비트맵 갱신→언락 순서로 진행되고, close 시 모든 로그를 원본 파일에 되돌려 쓴다. 크래시 후에는 metadata log의 비트맵과 실제 비트맵을 비교해 미완료 연산을 이어서 완료한다(p.114).

> [!quote]- 📄 원문 표현 (paper)
> - "MSL first checks whether it is beyond the maximum file size supported by the current radix tree... Then MSL will start traversing the radix tree from the root node." (p.111)
> - "MGL includes Intention Read (IR) and Intention Write (IW) locks. These two types of locks indicate that although a read/write lock does not lock the current range, a part of the range is locked by a more fine-grained read/write lock." (p.114)
> - "It takes 186ms to restore a 1GB file, of which 153ms is used to write a total of 48K entries (189MB of data) back to the original file." (p.114)

## 평가 (Evaluation)
실험은 2× Intel Xeon Gold 5317 + 128GB DRAM + 512G Intel Optane DC PM(4×128G, interleaved) 환경에서 Ext4-DAX, Libnvmmio, NOVA를 baseline으로 비교했다(p.115).

- **FIO 미세벤치마크**: fine-grained(<4KB) sequential write에서 MGSP는 Ext4-DAX 대비 3.31~4.21×, Libnvmmio 대비 3.43~4.53×, NOVA 대비 1.69~2.06× 성능 향상(p.115, Fig.8). ≥4KB write에서는 Libnvmmio 대비 3.23~4.3×, Ext4-DAX 대비 1.1~2.52×, NOVA 대비 1.01~1.43×. Random write는 Ext4-DAX 대비 2.52~2.97×(fine), 1.11~2.33×(coarse), NOVA 대비 fine에서 1.15~1.4×이나 coarse에서는 NOVA가 최대 17.9% 더 나은 경우도 있음(p.115).
- **Read 성능**: MGSP는 read 전용 최적화가 아님에도 Ext4-DAX 대비 fine-grained sequential read 1.89~3.07×, coarse-grained 1.26~1.33× 향상, Libnvmmio 대비 fine-grained random read 12.4~21.6% 향상(p.115).
- **Mixed read/write (Fig.9, p.116)**: write ratio 9:1에서 Libnvmmio는 Ext4-DAX 대비 50.2% 향상되지만 write 비율 50% 이상에서는 Ext4-DAX보다 나빠지는 반면, NOVA/MGSP는 write 비율 전반에서 각각 58.7~92.2%, 113.1~141.3% 안정적 향상을 유지.
- **Scalability (Fig.10, p.116)**: 4KB granularity 기준 fine-grained sequential write에서 MGSP는 Ext4-DAX/Libnvmmio/NOVA 대비 3.81~8.51×/3.14~57.6×/1.89~6.16× 향상, random access에서 2.56~3.76×/2.13~3.51× 향상.
- **SQLite 실응용**: Mobibench(WAL mode)에서 Ext4-DAX 대비 insert/update/delete 각각 18.3%/7.9%/32.5%, Libnvmmio 대비 25.7%/9.2%/20.6% 향상(Fig.11, p.117). TPCC(OFF mode)에서 Ext4-DAX/Libnvmmio/NOVA 대비 36.5%/41.3%/14.6% 향상(Fig.12, p.117) — 논문 초록에서 이 평균값을 "36.5% for TPCC"로 요약.
- **Write amplification (Table II, p.118)**: 10초간 random write 기준 write amplification ratio가 Libnvmmio 2.048(1K)/2.013(4K)/2.002(16K)인 반면 MGSP는 1.088/1.021/1.014로 Libnvmmio-wo-sync(이상적 하한, 1에 근접)에 근접.
- **Performance breakdown (Fig.13, p.118)**: 1-thread 1KB write에서 MGSP는 Ext4-DAX 대비 4.06×이며 주 기여는 shadow logging(double-write 회피), 4-thread 4KB write에서는 fine-grained locking이 가장 크게 기여해 총 3.42× 향상.

> [!quote]- 📄 원문 표현 (paper)
> - "MGSP can write only one copy of data and a small amount of metadata, effectively reducing write amplification and approaching the ideal result of Libnvmmio without sync (close to 1) while ensuring the atomicity of operations." (p.118)
> - "MGSP improves performance by 36.5%, 41.3%, and 14.6% compared to Ext4-DAX, Libnvmmio, and NOVA, respectively." (p.117)
> - "It should be noted that although MGSP provides file-system-level atomicity, it does not have a transaction-level atomic mechanism. We hope to add related designs in future work..." (p.117)

## 섹션 노트
- **I. Introduction**: NVM MMIO의 성능 잠재력과 기존 logging/shadow paging의 한계(double-write, write amplification, wandering tree)를 제시하고, MGSP의 4대 기여(shadow logging, multi-granularity shadow log, lock-free metadata + multi-granularity locking, 최적화 기법)를 요약.
- **II. Background and Motivation**: DAX/MMIO 배경, logging(undo/redo/differential/Libnvmmio hybrid)과 shadow paging(BPFS/NOVA/SplitFS) 각각의 drawback, 기존 crash-consistent MMIO(Libnvmmio 등)가 atomicity를 msync에만 의존해 sync 빈도에 취약함을 지적.
- **III. Multi-Granularity Shadow Paging**: MGSP 전체 아키텍처(Fig.2)와 MSL(radix tree, bitmap metadata, lazy cleaning), lock-free metadata log, MGL(IR/IW, lazy cleaning, greedy locking), write/read/close-recovery 흐름을 상세 서술.
- **IV. Evaluation**: 구현(Libnvmmio 기반 재설계, LD_PRELOAD + O_ATOMIC), FIO 마이크로벤치마크, scalability, SQLite(Mobibench/TPCC) 실응용, write amplification, performance breakdown 순으로 결과 제시.
- **V. Related Work**: NVM-aware 파일시스템(Ext4-DAX, BPFS, PMFS, NOVA, Strata, KucoFS)과 crash-consistent MMIO(SCMFS, NOVA CoW, SplitFS, Libnvmmio, Shadow Sub-Paging(SSP))를 비교하며 MGSP의 차별점(트리 기반 다중 입도 shadow log)을 정리.
- **VI. Conclusion & Appendix**: 기여 요약, artifact 공개(GitHub MIoTLab/MGSP), 설치·재현 스크립트 안내.

## 핵심 용어 (Key terms)
- **Shadow logging**: redo/undo 로그를 상황에 따라 서로의 역할로 전환해 재사용함으로써 double-write를 없애는 MGSP의 핵심 로깅 기법
- **Multi-Granularity Shadow Log (MSL)**: radix tree의 레벨마다 다른 granularity(블록~파일)의 shadow log를 관리하는 자료구조
- **Write amplification**: 실제 갱신량보다 더 큰 단위(고정 블록/페이지)로 로깅·쓰기하여 발생하는 초과 쓰기
- **Wandering tree problem**: 트리 기반 CoW 인덱스에서 leaf 갱신이 root까지 메타데이터 갱신을 연쇄적으로 전파시키는 문제
- **Valid bit / Existing bit**: MSL 각 노드가 자신의 로그 유효성(valid)과 자손 노드의 로그 존재 여부(existing)를 나타내는 2비트 메타데이터
- **Lock-free metadata log**: 스레드 ID 해시 + CAS + linear probing으로 global lock 없이 비트맵 갱신의 원자성을 보장하는 로그
- **Multiple Granularity Locking (MGL)**: Intention Read(IR)/Intention Write(IW) 락을 포함해 트리의 여러 레벨(입도)을 잠가 range lock을 근사하는 락 기법
- **Greedy locking**: 파일에 reference가 하나뿐일 때 minimum search tree 루트 노드만 잠가 range lock처럼 동작시키는 최적화
- **Minimum search tree**: locality에 기반해 직전 접근을 커버하는 가장 작은 서브트리를 캐싱해 검색 비용을 줄이는 기법
- **DAX (Direct Access)**: 페이지 캐시를 우회해 NVM 데이터에 직접 접근하는 파일시스템 모드

## 강점 · 한계 · 열린 질문
- **강점**: shadow logging이라는 단순하지만 강력한 아이디어(redo/undo 역할 교환)로 double-write 문제를 구조적으로 제거했고, radix tree의 granularity를 로그 관리에 직접 매핑한 설계가 write amplification과 metadata overhead라는 상충하는 두 목표를 함께 해결. Table II의 write amplification ratio(~1.01~1.09)가 이론적 하한에 근접함을 정량적으로 보여줌.
- **한계**: MGSP는 file-system-level atomicity만 제공하며 transaction-level atomicity는 없음(저자 스스로 명시, p.117). Intra-process parallelism에 최적화되어 있고 inter-process 공유(다른 프로세스가 열어놓은 파일에 대한 접근)는 파일이 닫히기를 기다려야 함(p.114) — 멀티프로세스 공유 파일 워크로드에는 제약. Greedy locking은 scalability에 영향을 줄 수 있어 reference가 1개인 경우로 제한적으로만 적용.
- **열린 질문**: coarse-grained random write에서 NOVA가 MGSP보다 최대 17.9% 더 나은 경우가 있는데(p.115), 이 격차의 근본 원인(로그 입도 선택 정책 vs. NOVA의 log-structured 구조)이 무엇인지. Transaction-level atomicity를 어떻게 file-system-level 위에 얹을 수 있을지(저자들도 future work로 남김). Multi-tenant/inter-process 공유 시나리오로 MGL을 확장할 수 있는지.

## ❓ Q&A (자가 점검)
> [!question]- Shadow logging은 왜 double-write를 없앨 수 있는가?
> 같은 데이터 블록에 연속 쓰기가 발생하면, 첫 쓰기의 redo 로그(새 데이터)는 두 번째 쓰기 시점에는 이미 "예전 데이터"가 되어 undo 로그의 조건을 만족한다. 따라서 새 로그를 또 쓸 필요 없이 두 번째 새 데이터를 원본 블록에 직접 쓰면 되어, 두 번의 쓰기 요청을 데이터 블록 두 번 쓰기만으로 완결(zero-copy)할 수 있다(p.109-110).

> [!question]- MSL의 radix tree에서 각 레벨은 무엇을 의미하는가?
> 트리의 각 레벨이 서로 다른 shadow log granularity를 담당한다(root=파일 전체 mmap, leaf=최소 블록 단위). 자식 노드는 필요할 때만(on-demand) 생성되며, write 크기에 맞춰 적절한 레벨(들)에서 로그를 쓰거나 여러 자식 노드로 분할 처리한다(Fig.4, p.110-111).

> [!question]- Bitmap의 valid bit과 existing bit은 각각 무엇을 뜻하는가?
> valid bit은 "현재 노드의 로그가 유효한 최신 데이터인지", existing bit은 "자손 노드 어딘가에 최신 데이터가 있는지"를 나타낸다. 둘 다 0이면 이 노드 이하에는 로그가 없어 원본 파일이 최신이다(p.112).

> [!question]- Lock-free metadata log는 어떻게 원자성을 보장하는가?
> 비트맵을 직접 수정하기 전에, 스레드 ID를 해시하여 얻은 개인 로그 엔트리(tid, inumber, offset, len, fsize, checksum, bitmap)에 먼저 필요한 정보를 persist한다. 해시 충돌은 linear probing으로 처리하며, 이 로그가 성공적으로 기록된 뒤에만 실제 비트맵을 갱신해 8-byte atomic write 제약 하에서도 크래시 안전성을 확보한다(Fig.6, p.113).

> [!question]- Greedy locking은 언제, 왜 사용하는가?
> 파일에 reference가 하나뿐이어서 다른 스레드/프로세스와의 동시 접근 충돌 우려가 적을 때, minimum search tree의 루트 노드 하나만 잠가 필요한 범위보다 넓게 락을 거는 방식이다. 이는 lazy cleaning의 반복적 lock-unlock 오버헤드를 없애지만 scalability에 영향을 줄 수 있어 reference가 1개인 조건에서만 활성화한다(p.114).

> [!question]- MGSP는 crash 후 어떻게 복구하는가, 그리고 얼마나 걸리는가?
> Close 시 모든 로그를 원본 파일에 되돌려 쓰는데, 크래시가 나면 metadata log에 저장된 비트맵과 실제 비트맵을 비교해 중단된 연산을 마저 완료한다. 논문은 1GB 파일에 대해 랜덤 지점에서 크래시를 유발한 실험에서 복구에 186ms(그중 153ms는 48K 로그 엔트리·189MB 데이터를 원본에 되돌려 쓰는 시간)가 걸렸고, 최악의 경우에도 1초 이내에 완료됨을 보였다(p.114).

> [!question]- SQLite 평가에서 MGSP의 이득이 제한적인 이유는 무엇인가?
> SQLite는 애초에 persistent memory 전용으로 설계되지 않았고 자체 로깅(WAL 등)으로 이미 일부 consistency를 보장하므로, 하부 파일시스템의 강한 synchronization 요구가 완전히 발휘되지 않는다. 그럼에도 OFF mode에서는 Ext4-DAX/Libnvmmio 대비 각각 30% 안팎의 향상을 얻어 file-system 레벨 consistency 보장이 애플리케이션 자체 로깅 부담을 줄여줄 수 있음을 보인다(p.117).

## 🔗 Connections
[[File System]] · [[HPCA]] · [[2023]]

## References worth following
- **Libnvmmio** ("Reconstructing software I/O path with Failure-Atomic Memory-Mapped Interface", USENIX ATC 2020) — MGSP가 직접 기반으로 삼아 재설계한 baseline이자 hybrid logging 비교 대상.
- **NOVA** ("A log-structured file system for hybrid volatile/non-volatile main memories", FAST 2016) — CoW 기반 강한 consistency를 제공하는 커널 log-structured FS로, MGSP의 성능·구조 비교축.
- **SplitFS** ("Reducing software overhead in file systems for persistent memory", SOSP 2019) — DAX-MMIO에 유연한 crash-consistency 보장을 제공하지만 strict mode에서 CoW로 인한 write amplification이 남는 사례로 인용.
- **BPFS** ("Better I/O through byte-addressable, persistent memory", SOSP 2009) — short-circuit shadow paging으로 propagation cost를 줄이는 원조 격 기법, MGSP의 shadow paging 계열 비교 대상.
- **Shadow Sub-Paging (SSP)** ("Eliminating redundant writes in failure-atomic nvrams via shadow sub-paging", MICRO 2019) — bitmap 기반 failure-atomic 메커니즘으로 redundant write 제거를 목표로 하는 유사 아이디어지만 cache-line 단위 remapping에 한정된다는 점에서 MGSP의 tree 기반 multi-granularity 설계와 대비됨.

## Personal annotations
<!-- 본인 메모 영역 -->
