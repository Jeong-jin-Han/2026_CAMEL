---
title: "ByteFS: System Support for (CXL-based) Memory-Semantic Solid-State Drives"
aliases: [ByteFS]
type: paper-ref
venue: ASPLOS
year: 2025
tags:
  - paper
  - cluster/cxl
  - topic/byte-addressable-ssd
  - topic/memory-semantic-ssd
  - topic/filesystem
  - topic/crash-consistency
  - venue/asplos
  - year/2025
---

# ByteFS: System Support for (CXL-based) Memory-Semantic Solid-State Drives

> **Source PDF**: [ByteFS - System Support for (CXL-based) Memory-Semantic Solid-State Drives.pdf](<ByteFS - System Support for (CXL-based) Memory-Semantic Solid-State Drives.pdf>)
> 🕸️ NodeGraph: [ByteFS.html (새 탭에서 렌더링, push 후 유효)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/SkyByte%20-%20Architecting%20An%20Efficient%20Memory-Semantic%20CXL-based%20SSD%20with%20OS%20and%20Hardware%20Co-design/refs/ByteFS%20-%20System%20Support%20for%20%28CXL-based%29%20Memory-Semantic%20Solid-State%20Drives/ByteFS.html)
> **Authors**: Shaobo Li, Yirui Eric Zhou (co-primary), Hao Ren, **Jian Huang** — University of Illinois Urbana-Champaign (UIUC)
> **Venue / Year**: ASPLOS 2025 (Rotterdam)
> **DOI**: 10.1145/3669940.3707250 · **arXiv**: 2501.04993 · **Length**: 17 pages
> **Read status**: ☑ Full read (2026-07-14)
> **My reading purpose**: [[SkyByte]]의 **자매 논문**(같은 UIUC Jian Huang 그룹, SkyByte ref [39]). SkyByte가 memory-semantic SSD를 **device(HW/OS co-design)** 층에서 설계했다면 ByteFS는 그 위 **filesystem/OS support** 층. "SSD 이중역할(byte↔block) × transparent co-design" 발표축에서 **소프트웨어 스택 계층**을 채우는 조각. dual byte+block FS 설계·SSD 내부 DRAM 로그 구조·crash consistency가 multi-host CXL로 갈 때 무엇이 깨지는지 파악.

---

## 📋 목차
- [TL;DR](#tldr)
- [Core thesis](#core-thesis)
- [Why this matters to me](#why-this-matters-to-me)
- [Structure overview](#structure-overview)
- [Section notes](#section-notes)
- [Key vocabulary](#key-vocabulary)
- [Citable quantitative data](#citable-quantitative-data)
- [🎯 Strategic anchor](#-strategic-anchor)
- [Connection to my research direction](#connection-to-my-research-direction)
- [Open questions / gaps](#open-questions--gaps)
- [References worth following up](#references-worth-following-up)
- [Personal annotations](#personal-annotations)

---

## TL;DR
ByteFS는 **memory-semantic SSD(M-SSD)** — PCIe/CXL의 byte-addressability로 **byte와 block 둘 다** 접근되는 NAND flash SSD($\approx$ \$0.22/GB) — 를 위한 **새 커널 파일시스템**이다. 기존 block FS(Ext4/F2FS)는 128B inode 하나 갱신에도 4KB write를 강제해 **I/O amplification(write 최대 6.2×)**을 내고, NVM FS(PMFS/NOVA)는 순수 byte 인터페이스라 flash의 page-granular 특성·spatial locality를 못 살린다. ByteFS는 (1) FS 핵심 자료구조별로 byte/block 선호를 **정량 분석(§3, Table 3)**해, metadata write는 byte·read는 block으로 **적응적 선택**하고, (2) SSD 내부 DRAM을 **cacheline 단위 log-structured write buffer**로 재조직해 작은 write를 **coalescing**(3-layer skip list 인덱스, 89ns lookup)하며, (3) host page cache와 SSD DRAM 사이 **coordinated caching**(SSD 안엔 page cache 안 둠)으로 귀한 SSD DRAM을 persistent write에 몰아주고, (4) firmware-level write log를 **redo log**로 재사용해 double-write 없이 **crash consistency·fast recovery(4.2s)**를 제공한다. host FS(software)와 SSD firmware(hardware)의 **co-design**. OpenSSD FPGA 실물 프로토타입 + FEMU 기반 에뮬레이터로 구현. SOTA(Ext4/F2FS/NOVA/PMFS) 대비 **최대 2.7×** 성능, SSD write traffic **최대 5.1×** 감소.

---

## Core thesis
> "we develop a new filesystem, named ByteFS, by rethinking the design primitives of filesystems and SSD firmware to exploit the advantages of both byte and block-granular data accesses." (Abstract)
> "none of the current file systems is a natural fit for M-SSDs." (§2.2)

M-SSD는 byte와 block을 **동시에** 노출하지만, 수십 년간 FS는 둘 중 하나만 가정해 설계됐다. ByteFS는 FS 자료구조마다 접근 granularity 선호가 다르다는 것을 정량적으로 밝히고(작은 metadata는 byte-write, locality 있는 read는 block), FS와 SSD firmware를 **함께** 재설계해 I/O amplification을 스택 전체에서 줄이면서 crash consistency를 보존한다.

---

## Why this matters to me
ByteFS는 [[SkyByte]]의 **자매 논문**이다 — 같은 UIUC Jian Huang 그룹, SkyByte가 인용하는 ref [39]. 두 논문은 memory-semantic SSD 스택의 **다른 층**을 맡는다: **SkyByte = device 층**(memory-semantic SSD를 HW/OS co-design으로 만드는 아키텍처, context switch·PLB류 latency 은닉), **ByteFS = filesystem/OS support 층**(그런 device 위에서 byte+block을 쓰는 FS, SSD firmware 로그·crash consistency). 내 "SSD 이중역할 × transparent co-design" 축에서 ByteFS는 **소프트웨어 계층의 transparent co-design** 사례다 — 애플리케이션엔 표준 POSIX FS로 보이지만 내부에서 byte/block을 적응 선택. 결정적으로 ByteFS는 §4.2에서 "host always contains the latest data, M-SSD firmware does not modify host-written data라서 **cache-coherency가 필요 없다**"고 명시하는데, 이건 **single-host 전제**를 그대로 드러낸다. 여러 host가 같은 CXL-SSD 파일시스템을 공유하면 이 전제가 정확히 깨지고 — 그 빈칸이 내 **multi-node CXL coherence** 방향이다.

---

## Structure overview
| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.1-2 | M-SSD는 byte+block 동시 지원·저가 NAND. 기존 FS는 block 전용→I/O amplification. ByteFS 제안 |
| 2 | Background & Motivation | p.2-3 | M-SSD 정의(BAR/MMIO, battery-backed DRAM, dual interface), Table 1 device 비교. block FS·NVM FS 모두 부적합 |
| 3 | Quantitative Study of Block I/O | p.3-5 | Ext4/F2FS 프로파일. Table 2 amplification, Table 3 자료구조별 byte/block 선호 |
| 4 | Design & Implementation | p.5-9 | 4대 도전. §4.2 byte access/persistence, §4.3 log-structured SSD DRAM, §4.4 dual r/w, §4.5 metadata, §4.6 data I/O(CoW ratio), §4.7 crash consistency, §4.9 구현(3.9K+1.5K LoC) |
| 5 | Evaluation | p.10-13 | 최대 2.7×, write traffic 5.1×↓, metadata traffic 25.3×↓, flash 2.9×↓, recovery 4.2s, CXL sensitivity |
| 6 | Related Work | p.13 | SCM(Optane 2022 종료), M-SSD(FlatFlash[10], Jung CXL-SSD[26]), PM FS(BPFS/PMFS/NOVA/SplitFS) |
| 7 | Conclusion | p.13 | dual byte/block 커널 FS로 스택 전체 I/O amplification 감소 |

---

## Section notes

### §1-2 Introduction & Background (p.1-3)
**Memory-semantic SSD(M-SSD)**: PCIe(NVMe)/CXL의 byte-addressability + MMIO로 **byte와 block 둘 다** 접근되는 commodity flash SSD. PCIe **BAR(Base Address Register)**로 device buffer를 host memory space에 매핑 → host가 load/store로 접근. 내부 DRAM이 byte-granular 요청의 data buffer, **battery-backed DRAM**이 power loss 시 persistence 보장. **normal NVMe block interface도 유지** → dual interface. Samsung의 CXL-based SSD(CMM-H)[40]가 실제 사례. NVM(Optane, PCM 기반, processor memory bus)과 달리 M-SSD는 **PCIe attached + 성숙한 NAND**라 저가($\approx$ \$0.22/GB)·TB급 확장.

> "memory-semantic SSDs (M-SSDs) provide the byte interface by leveraging the in-device DRAM and the PCIe memory-mapped interface." (§2.1)

**Table 1 — device 비교**: DRAM 100ns / 31.8GB/s / 비영속. NVM 300/90ns / 6.6·2.3GB/s / 영속. M-SSD **4.8/0.6µs** cacheline R/W / 3.5·2.5GB/s / **\$0.22/GB** / 영속. → M-SSD는 NVM보다 느리지만(특히 SSD DRAM 미적중 시 µs급 flash 접근) **압도적으로 싸다**.

**왜 새 FS가 필요한가(§2.2)**: block FS(Ext4/F2FS)는 고정 block(512B/4KB) 관리라 byte 접근을 못 씀. NVM FS(PMFS/NOVA/SplitFS)는 byte load/store·DAX로 SW overhead 최소화에 집중했지만, M-SSD는 **높은 PCIe latency + 불가피한 flash 접근**이 병목이라 NVM FS를 그대로 얹으면 부적합. → ByteFS 동기.

### §3 Quantitative Study of Block I/O Interface (p.3-5)
Ext4·F2FS를 Filebench+OLTP로 프로파일. **각 FS 자료구조 단위**로 I/O amplification 분석(기존 연구와 차별점).

**Table 2 — I/O amplification(block interface)**: Ext4 write **3.85×**(Varmail)·**6.21×**(Fileserver)·1.43-2.17×, read 1.15-1.71×. F2FS write 1.06-2.14×, read 1.13-1.67×. metadata op이 write amplification 주범.

**핵심 관찰**:
- **Inode**: write의 **35%**(Ext4)·24.4%(F2FS). 128B inode 갱신 하나에 **4KB write** 필요 → byte interface로 크게 감소 여지. read는 inode block 로드해 host 캐싱(같은 block 내 locality).
- **Journaling**: Ext4 ordered mode에서 전체 traffic의 **30.7%**(critical metadata double-write).
- **Bitmap(block/inode list)**: 갱신 시 몇 바이트만 flip → byte 유리. read는 캐싱.
- **Directory entry**: micro-bench에서 write의 23% → byte 유리.
- **Data pointer**: F2FS out-of-place update로 write 26%·read 16%.

**Table 3 — 자료구조별 preferred interface**: Superblock(R:Block/W:Block), BlockList·InodeList·Inode·DataPointer·DirectoryEntry(R:**Block**/W:**Byte**), PageCache(R:Block/W:Block·Byte), DataBlock(R:Block·Byte/W:Block·Byte), DataJournal(R:Block/W:Block·Byte).

> "most metadata updates in file systems are small, so the byte interface is suitable. For their reads, we can use the block interface to exploit data locality." (§3.2)

### §4.1 System Overview & Challenges (p.5)
목표: **dual byte/block interface를 transparent하게** 지원하는 FS, host FS(SW)+SSD firmware(HW) **co-design**. 4대 도전: (1) byte interface와 flash chip의 **granularity mismatch** → I/O amplification 최소화; (2) FS가 byte/block을 **어떻게** 쓸지 불명확 → 유연한 선택; (3) 최소 overhead로 **data consistency**; (4) **essential FS properties**(crash consistency·recovery) 보존.

### §4.2 Enable Byte-granular Access/Persistence (p.5-6)
BAR로 **SSD 전체를 memory region으로** host에 매핑 → MMIO(byte interface)로 임의 SSD 주소 접근. **중요**: ByteFS는 **SSD DRAM↔host CPU cache 간 cache-coherency를 요구하지 않는다** — host가 항상 최신 데이터를 갖고, M-SSD firmware가 host가 쓴 데이터를 수정하지 않기 때문. byte interface는 **CXL.mem**로도 실현 가능(cacheable load/store).

**Persistence**: battery-backed DRAM. 단, host CPU의 **Write Combining(WC)** 모드가 small write를 버퍼링·coalescing해 dirty cacheline/pending PCIe transaction을 남길 수 있음. 2단계 보장: (1) MMIO write 후 **clflush/clwb**로 CPU cache flush; (2) write 뒤 **write-verify read**(zero-byte read)로 posted PCIe transaction 완료 강제(root complex에서 read/write serialize).

> "ByteFS do not require cache-coherency between the SSD DRAM and the host CPU cache because the host always contains the latest data and the M-SSD firmware does not modify the data written by the host." (§4.2)

### §4.3 SSD DRAM as Log-Structured Memory (p.6-7)
Flash는 page-granular만 가능한데 host는 byte 접근 → mismatch. ByteFS는 SSD DRAM cache를 **cacheline 단위 log-structured region**으로 재조직 → 모든 byte write를 log에 append(critical path의 flash 접근 회피). **Coordinated caching**: SSD 안엔 page cache를 **두지 않고** loaded page는 host DRAM에만 캐싱 → 귀한 SSD DRAM을 persistent write에 몰아줌. log utilization **85%** 초과 시 background cleaning이 log entry를 coalesce해 flash로 flush.

**Write log 구조**: global log region = circular buffer(**256MB** 기본, head/tail, 64B-aligned entry) + indexing structure. **3-layer skip list 인덱스**: (L1) SSD 주소공간을 16MB partition으로 분할(partition table, LPA/partition-size로 즉시 인덱싱); (L2) LPA로 인덱싱된 skip list; (L3) block offset 순 ordered chunk list — chunk entry = block offset(1B)+log offset(4B)+data length(4B). $O(\log n)$ lookup. 256MB log fully-utilized 시 평균 lookup **89ns**(flash가 µs급이라 무시 가능), 인덱스는 SSD DRAM **21MB** 차지.

**Firmware-level logging = redo log**: 모든 metadata update를 log에 append → write log를 **redo log로 직접 재사용**해 **double logging 회피**. atomic·crash-consistent update.

### §4.4 Dual Byte/Block Read/Write (p.7)
**Byte read**: host CPU가 MMIO load → firmware가 write log lookup. log에 있으면 직접 반환, 없으면 flash에서 page fetch 후 **요청 cacheline만** 반환. **Byte write**: 64B entry로 정렬. 단일 64B write는 직접 write+clflush/clwb. 다중 cacheline atomic → FS transaction으로 wrap. 같은 데이터 concurrent write 시 chunk entry로 최신 버전 추적.
**Block r/w**: 표준 4KB NVMe. read는 flash→transfer buffer 로드 후 skip list 조회, log에 dirty cacheline 있으면 **merge**. write는 4KB를 FTL write buffer로, flash write 후 firmware가 skip list 스캔해 해당 page의 log entry **invalidate**(host page cache write-back은 최신이므로 즉시 무효화 가능). Log cleaning은 double buffering으로 background 수행(Algorithm 1).

### §4.5 Metadata Management (p.8)
host-side metadata caching, miss 시 block interface로 로드.
- **Inode**: 128B entry를 4KB page로 그룹화. 각 inode를 **upper/lower 64B로 분할** — lower = 자주 갱신(file size, mtime, access rights). inode update를 **64B byte write**로 atomic 처리. host에 radix tree 캐싱, miss 시 inode page 전체를 block으로 로드.
- **Directory entry**: inode number(4B)+file type(2B)+name length(2B)+filename(최대 256B). lookup 시 directory block 전체를 block interface로 로드, hashed dir name radix tree 캐싱. create/rename은 단일 entry(64-320B) byte write.
- **Block/Inode bitmap**: 64B group 단위 갱신. boot 시 block으로 로드. per-CPU free list + extent 기반 할당. 작은 bitmap update가 byte로 큰 이득.
- **Data pointer**: Ext4-like **extent** 구조. leaf extent node(16B)=file offset(8B)+LBA(4B)+length(4B). in-place update, byte-granular persistent write.

### §4.6 Data I/O Management (p.8-9)
- **Direct I/O**(O_DIRECT): 접근 size $\leq$ 512B면 byte, 아니면 block(512B 미만은 cacheline write가 4KB page persist보다 빠름).
- **Buffered I/O**: host page cache가 흡수. dirty page write-back 시 **modified ratio $R$**로 byte/block 선택. **CoW**로 원본 page를 duplicate에 복사(XArray로 추적, per-inode `address_space`). write-back 시 원본⊕duplicate **XOR**로 수정된 64B chunk 식별. $R = N_{Modified}/N_{Total}$. $R < 1/8$(4KB 중 512B)이면 byte, 아니면 block. **avx2** 256-bit XOR(평균 936 cycle, 14GB/s). duplicate page는 page cache의 평균 **16%**(max 8GB).
- **mmap I/O**: M-SSD를 memory expander로. cached DRAM page를 user space에 매핑.
- **Data journaling**: metadata logging = Ext4 ordered mode와 동일 보장 + data journaling. 작은 write는 §4.3 transaction, 큰 write는 **JBD2 + ByteFS transaction** 결합(commit 시 TxID 담은 commit entry를 JBD2 record 끝에 append).

### §4.7 Crash Consistency & Recovery (p.9)
- **Crash consistency**: transaction으로 metadata/data update의 atomicity·write ordering 강제. firmware-level log + battery-backed DRAM이 commit 가속. 4B **TxID**(monotonic counter), host의 **TxTable**로 ongoing transaction 추적, transaction-level lock. commit은 custom NVMe `COMMIT(TxID)` → firmware가 2MB **TxLog**에 4B commit entry append.
- **Data recovery**: battery-backed SSD DRAM이 내용 보존. custom NVMe `RECOVER()` → firmware가 log region 전체 스캔, 각 entry 끝 4B TxID가 TxLog에 없으면 **uncommitted로 폐기**, 있으면 TxID 순서대로 flash에 flush. 이후 data journal 복구.

### §4.9 Implementation (p.9-10)
- **ByteFS**: Ext4 기반 커널 FS, **3.9K LoC**(Linux 5.1). on-disk metadata 재조직 1.3K LoC, CoW용 XArray+writepage() 수정 0.6K LoC.
- **M-SSD 프로토타입**: **OpenSSD FPGA**(1TB flash, 16채널, 1GB DRAM, onboard ARM). SSD firmware **1.5K LoC 수정, HW 변경 없음**. custom NVMe(byte access) 0.1K, 256MB write log+3-layer skip list 0.8K, log cleaning+transaction 0.4K. address mapping table 512MB·write buffer 16MB SSD DRAM.
- **에뮬레이터**: **FEMU** 기반 2.1K LoC. FTL thread를 1 CPU core에 pin, memmap으로 DRAM 예약 + I/O latency 주입.

### §5 Evaluation (p.10-13)
Setup: 28-core Intel E5-2683 v3 2.7GHz, 128GB. 에뮬레이터 32GB, 4KB page, 8채널, flash 40/60µs, cacheline 4.8/0.6µs(Table 4). Baseline: Ext4·F2FS(block), NOVA·PMFS(byte NVM FS, M-SSD 매핑). 모두 256MB SSD DRAM 캐싱, ByteFS는 그걸 log-structured로.

- **§5.2 Overall(Fig.6)**: micro에서 Ext4 대비 **2.5×**, F2FS 대비 1.48×. file **create 6.0×**(vs Ext4)·2.4×(vs F2FS). Macro: Varmail 1.9×(vs F2FS), Fileserver 2.2×(vs Ext4), Webproxy 1.3×(vs Ext4), **OLTP 4.1×**(vs Ext4). YCSB(RocksDB): F2FS 대비 2.4× throughput, read avg/tail 2.3×/2.0×. NOVA/PMFS는 flash용 설계 아니라 spatial locality 못 살려 Ext4/F2FS보다도 나쁜 경우 많음.
- **§5.3 I/O traffic(Fig.8-11)**: metadata traffic Ext4 대비 **최대 25.3×**·F2FS 17.2×↓(평균 11.4×/6.1×). NOVA 대비 metadata read ~43%↓. flash traffic 평균 Ext4/F2FS/NOVA/PMFS 대비 **2.9×/2.1×/3.2×/2.2×↓**(small write coalescing). abstract의 **write traffic 5.1×↓**가 여기서 옴.
- **§5.4 Breakdown(Fig.12)**: ByteFS-Dual(dual interface)·ByteFS-Log(+firmware log)·ByteFS(full). Varmail/Fileserver는 둘 다, Webproxy는 dual interface, OLTP는 log+선택 모두에서 이득.
- **§5.5 Recovery**: 평균 **4.2s**(SSD DRAM 로드 0.9s + log/TxLog 스캔·flush 2.7s).
- **§5.6 Sensitivity(Fig.13-14)**: flash latency 무관하게 F2FS/NOVA 우위. **CXL-based SSD(cacheline 175ns, 3/80*)** 에뮬레이트 시 CXL이 byte latency를 줄여 ByteFS·NOVA 모두 개선하나 NOVA는 여전히 느림(높은 flash latency 미최적화). log size 64M→512M로 키우면 coalescing↑ → 성능 scale.

### §6-7 Related Work & Conclusion (p.13)
SCM(PCM/ReRAM/FeRAM, Optane는 2019 출시·**2022 종료**). M-SSD: **FlatFlash[10]**(SW 기반 M-SSD, unified virtual memory space + unified address translation), Jung의 CXL-SSD FPGA 에뮬레이터[26]. 이들은 **device를 어떻게 system software가 관리할지**는 거의 안 다룸 → ByteFS가 그 공백을 채움. PM FS(BPFS/PMFS/NOVA/SplitFS/Ziggurat/Strata)와 달리 ByteFS는 **근본적으로 다른 device 특성**의 M-SSD용.

---

## Key vocabulary
**Thesis / framing:**
- "memory-semantic SSD (M-SSD)"
- "dual byte/block interface"
- "rethinking the design primitives of filesystems and SSD firmware"

**Technical concepts:**
- "log-structured memory in the SSD firmware" / "in-device write log"
- "data coalescing" (small writes → fewer flash accesses)
- "coordinated data caching" (host page cache ↔ SSD DRAM)
- "modified ratio ($R = N_{Modified}/N_{Total}$)" + copy-on-write interface selection
- "firmware-level logging as a redo log" (avoid double logging)
- "three-layer skip list" write-log index
- "byte-granular data persistence" (clflush/clwb + write-verify read)

**Value language:**
- "transparently supports dual byte/block interface"
- "preserving the essential properties of file systems"
- "cost-effective solution (\$0.22/GB)"

> ⚠ **피해야 할 어휘** (ByteFS-signature, echo 주의):
> - "memory-semantic SSD" (이 그룹 고유 브랜딩 — Samsung/UIUC 계열)
> - "coordinated data caching"
> - "log-structured memory in the SSD firmware"

---

## Citable quantitative data
| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| Table 1, p.3 | M-SSD cacheline R/W 4.8/0.6µs, ~\$0.22/GB vs DRAM 100ns / NVM 300/90ns | M-SSD 성능·비용 위치 |
| §3.2, p.4 | 128B inode 갱신 → 4KB write; inode가 write의 35%(Ext4) | block interface amplification 근거 |
| Table 2, p.3 | Ext4 write amplification 최대 6.21×(Fileserver) | block FS의 낭비 |
| §4.3, p.7 | 256MB log, 3-layer skip list lookup 89ns, index 21MB | log 구조 경량성 |
| §5.2-5.3, p.10-12 | 최대 2.7× 성능·write traffic 5.1×↓·metadata 25.3×↓·flash 2.9×↓ | ByteFS 종합 성과 |
| §5.5, p.12 | crash recovery 평균 4.2s | firmware redo log의 실효 |
| §5.6, p.13 | CXL cacheline latency 175ns 에뮬 | CXL-SSD 미래 지향 |

---

## 🎯 Strategic anchor
> "ByteFS do not require cache-coherency between the SSD DRAM and the host CPU cache because the host always contains the latest data and the M-SSD firmware does not modify the data written by the host. The byte interface can also be realized with CXL.mem protocol, with which the host CPU can issue cacheable load/store accesses to the device." (§4.2, p.6)

→ **본인 활용**: ByteFS의 crash-consistency·caching 전체가 "**host가 항상 최신본을 갖는다**"는 **single-host 불변식** 위에 서 있다는 자백. 면담에서 "ByteFS는 coherency가 필요 없다고 명시하는데, 이건 정확히 **한 host** 전제이고, CXL 3.0에서 **여러 host가 같은 M-SSD 파일시스템을 공유**하면 이 불변식이 깨진다 — 누가 최신본을 갖는지, 누가 flush/commit 권한을 갖는지가 미정의가 되고, 그때 back-invalidate/HDM-DB coherence가 ByteFS의 TxTable·write log에 편입돼야 한다"로 내 multi-host 방향을 정확히 안착시킬 수 있음.

---

## Connection to my research direction
| 차원 | ByteFS (2025) | SkyByte (2025) | 내 방향 |
|---|---|---|---|
| 스택 층 | **filesystem / OS support** | **device (HW/OS co-design)** | 공유 memory **coherence** 층 |
| 인터페이스 | dual byte+block FS | pure memory-semantic device | 공유 memory pool |
| SSD DRAM | log-structured write buffer + coalescing | promotion cache(PLB 계승) | multi-host directory |
| 일관성 | firmware redo log·TxLog (**single host**) | PLB/promotion consistency (**single host**) | **multi-host (HDM-DB/BI)** |
| 명시적 전제 | "no cache-coherency needed" | single host↔device | 이 전제를 깨는 게 목표 |

ByteFS와 SkyByte는 **같은 device(memory-semantic SSD)의 서로 다른 층**이다 — ByteFS는 그 위 파일시스템, SkyByte는 그 device 자체. 둘을 겹치면 "byte↔block 이중역할을 transparent co-design으로 다룬다"는 축이 **device↔FS 두 층**에서 동시에 나타난다. 내 연구는 이 스택에 **세 번째 축(single→multi host coherence)**을 더한다. ByteFS의 "host가 최신본 소유 → coherency 불필요" 불변식, SkyByte의 single host↔device promotion 모두 **한 host** 전제 위에 있고, 공유 CXL-SSD로 가면 FS namespace·write log·commit 권한의 **cross-host consistency**가 새 문제로 열린다. (→ [[SkyByte]], [[FlatFlash - Exploiting the Byte-Accessibility of SSDs within a Unified Memory-Storage Hierarchy]], [[CXL Multi-node Coherence]], [[CXL Overview]])

---

## Open questions / gaps
- [ ] ByteFS는 "cache-coherency 불필요"를 **single-host** 전제로 명시 — **multi-host 공유 M-SSD**에선 이 전제가 깨짐(누가 최신본·flush 권한?).
- [ ] write log·TxTable·TxLog가 단일 host FS 소속 — 여러 host가 같은 namespace를 쓰면 **cross-host transaction/commit ordering**이 미정의.
- [ ] coordinated caching(SSD엔 page cache 없음, host DRAM에만)은 **한 host의 page cache**가 최신본이라는 가정 — 다중 host page cache 간 일관성 문제.
- [ ] byte interface를 CXL.mem로 realize 가능하다고 언급하지만(§4.2) 실제 CXL coherence(BI) 위에서의 FS 재설계는 future work.
- [ ] SkyByte(device)와 ByteFS(FS)의 **결합** — SkyByte device 위에 ByteFS를 얹었을 때 promotion과 log-structured write buffer가 어떻게 상호작용하는지 미평가.

---

## References worth following up
| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☑ | [10] | Abulila et al., **FlatFlash** (ASPLOS 2019) | ByteFS가 인용하는 M-SSD 원형(SW 기반, unified address translation). 이미 정독 |
| ☐ | [26] | Jung, **Hello Bytes, Bye Blocks: CXL-SSD** (HotStorage 2022) | CXL-SSD FPGA 에뮬레이터 — memory expansion용 M-SSD |
| ☐ | [49] | Yang et al., **CXL-Enabled SSDs** (USENIX ATC 2023) | CXL SSD로 memory wall 극복 — device 층 비교 |
| ☐ | [48] | Xu & Swanson, **NOVA** (FAST 2016) | log-structured NVM FS — ByteFS baseline, crash consistency 비교 |
| ☐ | [27] | Kadekodi et al., **SplitFS** (SOSP 2019) | user/kernel 분리 PM FS — 설계 대안 |
| ☐ | [31] | Li et al., **FEMU** (FAST 2018) | ByteFS 에뮬레이터 토대 |
| ☐ | [40] | Samsung, **CMM-H (CXL Memory Module-Hybrid)** | 실제 CXL-based SSD 제품 |

---

## Personal annotations
<!-- 본인 메모 영역 -->
