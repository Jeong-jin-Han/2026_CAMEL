---
title: "D2FS: Device-Driven Filesystem Garbage Collection"
aliases: [D2FS]
description: "SSD의 device-level GC가 host filesystem의 free section까지 회수하게 만들어 비싼 filesystem-level GC를 제거하는 log-structured filesystem"
venue: FAST
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/garbage-collection
  - topic/filesystem
  - venue/fast
  - year/2025
---

# D2FS: Device-Driven Filesystem Garbage Collection

> **FAST 2025** · `cluster/fs` · Source: [D2FS - Device-Driven Filesystem Garbage Collection.pdf](D2FS%20-%20Device-Driven%20Filesystem%20Garbage%20Collection.pdf)

저자: Juwon Kim, Seungjae Lee (KAIST), Joontaek Oh (Univ. of Wisconsin–Madison), Dongkun Shin (Sungkyunkwan Univ.), Youjip Won (KAIST)

## TL;DR
Log-structured filesystem(LFS)은 free section을 회수하는 **filesystem-level garbage collection(GC)** 때문에 host CPU를 점유하고 throughput을 1/3까지 떨어뜨리며 tail latency를 폭증시킨다. D2FS는 발상을 뒤집어, **storage device(FTL)가 자신의 device-level GC를 수행하면서 동시에 filesystem partition의 free section까지 회수**하도록 만든다. 세 가지 핵심 기법 — Coupled Garbage Collection(CGC, valid flash page를 새 filesystem 위치로 remap), Migration Upcall(device→host 통보를 NVMe completion signal에 piggyback), Virtual Overprovisioning(filesystem partition을 물리 용량보다 크게 잡아 device GC가 항상 제때 free section을 공급)으로 filesystem-level GC를 완전히 제거한다. 결과적으로 FIO에서 F2FS 대비 3×, zoned F2FS 대비 1.7×, IPLFS 대비 1.5×(YCSB-F) 빠르고, IPLFS 대비 device memory를 1/14로 줄인다.

## 문제 & 동기
LFS(F2FS 등)는 데이터를 append-only로 순차 기록해 flash/SMR에 유리하지만, invalid해진 filesystem block을 회수하는 **filesystem-level GC**가 치명적 오버헤드다. GC는 exclusive lock을 잡고 write/unlink/create 같은 모든 filesystem 상태 갱신을 suspend하며, 전후로 checkpoint를 강제해 crash consistency를 보장한다(Fig.1: throughput을 1/3로 하락, tail latency 폭증). 한편 flash storage는 별도로 **device-level GC**를 돌리는데, host와 device가 서로 조율되지 않은 채(uncoordinated) 각자 GC를 수행해 write amplification이 중복된다.

기존 완화책은 두 갈래다. (1) host에서만 GC를 수행해 device-level GC를 없애지만(IPLFS) 여전히 filesystem GC 비용이 남거나, ZByte급 거대 partition을 요구해 FTL의 L2P mapping이 비대해진다. (2) ZNS SSD는 device-level GC를 없애지만 GC 책임을 application에 떠넘겨 설계를 복잡하게 한다. D2FS는 "**storage device가 host filesystem을 대신해 free section을 회수한다**"는 정반대 방향을 택한다.

> [!quote]- 📄 원문 표현 (paper)
> "the storage device reclaims the free sections at the filesystem on behalf of the filesystem with minimal interference to the foreground filesystem activity. We exploit the garbage collection mechanism of the underlying storage in reclaiming the free sections at the log-structured filesystem. Our work distinguishes itself from the earlier works in that we delegate the garbage collection from the filesystem to the storage device, not vice versa." (p.3, §1)
>
> "filesystem-level garbage collection has a more drastic impact, lowering benchmark throughput to 1/3. Device-level garbage collection reduces the performance by up to 20%." (p.3, §1)

## 핵심 통찰

> [!note]- 통찰 1 — Device-level GC를 host filesystem GC와 "결합(couple)"할 수 있다
> filesystem block의 물리 위치는 두 mapping 계층으로 결정된다: F2L mapping ([fd,offset]→LBA, host가 관리)과 L2P mapping (LBA→PPA, FTL이 관리). 기존엔 둘이 배타적으로 관리됐다. CGC는 valid flash page를 새 flash block으로 옮길 때 **L2P뿐 아니라 F2L의 LBA까지 함께 갱신(remap)**한다. 그 결과 valid page가 물리적으로(같은 flash block)도, 논리적으로(같은 filesystem section)도 clustering되어, host의 데이터 이동 없이 filesystem free section이 회수된다.

> [!note]- 통찰 2 — Device→host 통보는 새 interrupt 없이 기존 NVMe completion에 얹을 수 있다
> 기존 IO interface(interrupt, polling)는 host-centric이라 device가 시작한 활동을 알리기 어렵다. Migration Upcall은 device가 outstanding IO의 completion signal에 **UPCALL flag를 세워 piggyback**한다. device-level GC는 host write가 빈번할 때(=completion signal이 풍부할 때) 주로 발동하므로, piggyback할 신호 부족이 없다. 새 data structure나 interrupt vector가 불필요하다.

> [!note]- 통찰 3 — 적당한 Virtual Overprovisioning만으로 device GC가 제때 free section을 공급한다
> filesystem partition 크기를 물리 storage 용량보다 ρ_v배 크게 잡으면(virtual = 실제 존재하지 않음), filesystem이 free section을 소진하기 전에 device가 먼저 free flash block을 소진해 GC를 발동한다. ρ_v ≈ 2.4 정도의 "moderate" 수준이면 충분하며, IPLFS처럼 ZByte급 거대 partition(ρ_v=2^33)이 필요 없다. 단, free section fault를 막으려면 invalid block을 가능한 한 빨리 알려야 하므로 D2FS는 **immediate discard** 정책을 쓴다.

## 설계 / 메커니즘

> [!abstract]- ① Coupled Garbage Collection (CGC) — §4
> Block Associative Mapping + valid page 통합 + F2L 동기화의 3요소.
> - **Block Associative Mapping**: L2P를 block mapping으로 두되, victim block의 valid page를 target flash block의 임의 offset에 놓을 수 있게 한다(set-associative cache/virtual memory와 유사). page를 옮기며 block offset이 바뀌면 victim page의 LBA를 새 LBA로 갱신하는 것이 **remap**.
> - **세 단계 통합**: (i) Victim Selection (greedy 정책), (ii) Block Associative Migration (free flash block을 destination으로 할당, valid page를 sequential하게 배치), (iii) Remap (destination block을 GC region에서 뽑은 새 mapping entry L_1에 매핑; host와 LBA 충돌을 막기 위해 filesystem 안에 CGC 전용 **garbage collection region**을 예약).
> - 예: block α의 LBA 108(offset 8) page를 block β로 옮겨 offset 1이 되면, LBA는 108→201로 갱신. host와 device 간 data traffic 없이 filesystem free section을 회수한다.

> [!abstract]- ② Migration Upcall — §5
> device가 remap한 page의 <old LBA, new LBA>를 **migration record**로 만들어 host에 batch로 보낸다(한 upcall당 최대 256 record).
> - **Migration Queue pair**(SQ/CQ)를 NVMe queue 위에 둠. control flow가 일반 IO와 반대(device→host). device가 SQ에 upcall을 놓고(step①), **Upcall Piggybacking**으로 completion signal의 UPCALL flag를 세워 host에 알림(step②). NVMe driver가 이 flag를 보면 upcall handler를 깨운다.
> - migration record 3상태: **buffered**(생성~전송 전) → **in-flight**(전송~host 확인 전) → **synced**(host가 filesystem 갱신 완료 통보 후).
> - **read-redirect / discard-redirect**: remap 직후 host filesystem이 아직 갱신 안 된 짧은 구간 동안, host가 obsolete LBA로 read/discard하면 FTL이 mapping entry가 NULL임을 보고 outstanding migration record를 찾아 새 LBA로 redirect한다(예: 0x000101→0x800101). mapping entry는 host가 record를 반영한 뒤에만 recycle.

> [!abstract]- ③ Migration Upcall 처리 & Crash Recovery — §5.3, §7
> 전용 thread가 upcall을 (i) filesystem 상태 갱신(file mapping/block bitmap=SIT/reverse mapping=SSA를 old·new LBA에 맞춰 수정), (ii) checkpoint(F2FS와 동일 조건; threshold 초과 시), (iii) 완료 통보의 3단계로 처리.
> - **Order 보존**: CGC가 migration한 순서대로 record를 SQ에 넣고, host도 그 순서대로 처리해 일관성 보장.
> - **Failure-atomic**: ① outstanding record는 supercap/spare area로 device DRAM 보호(가정), ② 단일 record를 failure-atomic하게 처리(upcall handler가 checkpoint를 disable, global rwsemaphore의 shared lock으로 checkpoint를 막되 다른 thread는 filesystem 사용 가능), ③ migration record를 생성 순서대로 처리. **redo 의미론**의 crash recovery: checkpoint pack에서 마지막 처리 record를 식별해 in-flight outstanding record를 device가 재제출하고 host가 redo.

> [!abstract]- ④ Virtual Overprovisioning & Partition Layout — §6
> - filesystem partition = ρ_v × (storage device capacity). regular region = (ρ_v − 1) × capacity, **GC region 크기 = storage device capacity**(CGC가 destination LBA를 여기서만 할당). 실험상 ρ_regular=1.4, GC region=device capacity → ρ_v=2.4.
> - **GC region 규칙**: filesystem은 GC region의 block을 read/update/discard할 수 있으나 **LBA를 할당할 수는 없다**. GC region의 file block이 update되면 새 내용은 regular region에 쓰이고, 옛 LBA(L_0)는 discard되어 CGC가 재활용(Fig.7).
> - **Immediate discard**: 기존 filesystem(F2FS/BTRFS)은 idle 시 batch discard하지만, D2FS는 free section fault(invalid를 device가 몰라 회수 못 함)를 막기 위해 invalidate 직후 discard를 발행. 최근 SSD는 discard 비용이 낮아 가능하다고 주장.
> - **Block type 지정**: data block과 filemap block을 NVMe stream interface로 구분 → CGC가 동종 block을 같은 flash block에 clustering.
> - F2FS 위에 구현(metadata 구조는 F2FS와 동일: Super Block, Checkpoint, Node Address Table, Reverse Mapping, Block Bitmap).

## 평가

> [!success]- 평가 요약 (수치 + p.X)
> **환경**: Linux v5.11.0, F2FS baseline, NVMeVirt SSD emulator로 Samsung 970 Pro 모델링(8 channel, 16 chip, 16KB flash page, 2MB flash block, 32MB superblock, 7% OVP). emulated SSD 256GB, host 최대 메모리 64GB. baseline: F2FS, zoned F2FS, IPLFS(Interval Mapping FTL). LoC 약 5.4K. (p.10, §8.1)
> - **FIO throughput**: D2FS가 zoned F2FS 대비 **1.7×**, IPLFS 대비 약 **15%** 우위. D2FS는 device가 flash block 회수 시 throughput 30% 하락에 그치나, F2FS는 GC 시작 시 80% 하락, zoned F2FS는 75% 하락. WAF는 D2FS/IPLFS/zoned F2FS 모두 약 1.4, F2FS는 더 높음. (p.11, §8.3, Fig.12)
> - **Macrobenchmark throughput**: TPC-C에서 zoned F2FS 대비 최대 **1.4×**. YCSB에서 IPLFS는 zoned F2FS보다 낮음(Interval Mapping FTL의 주기적 L2P compaction이 fsync-intensive YCSB에서 application을 멈춤). (p.12, §macrobenchmark, Fig.13)
> - **Latency**: FIO 4KB write 99.99th tail latency를 zoned F2FS 대비 **1/3**로 감소(p.13, §8.4, Fig.14). macrobench에서 F2FS 대비 평균·99.99th tail을 각각 최대 **1/5, 1/11**(YCSB-F update); IPLFS 대비 최대 **1/2, 1/8**(YCSB-F). (p.12-13, Fig.15)
> - **GC latency breakdown**(단일 32MB superblock): filesystem-level GC가 device-level GC보다 한 section 회수에 **3×~10×** 비쌈. filesystem GC latency 중 filemap traverse+page cache 할당이 60%, write dispatch가 최대 13%(YCSB-F), valid block read는 5%(대부분 이미 page cache에 존재), checkpoint+section write가 약 20%. (p.13, §8.5, Fig.16)
> - **FTL memory(256GB SSD, Table 1)**: F2FS 256.0MB, IPLFS 392.0MB, zoned F2FS 10.4MB, **D2FS 27.2MB**. D2FS는 zoned F2FS 대비 2.4×(Virtual Overprovisioning 때문)지만 IPLFS 대비 **1/14**. migration record buffer는 모든 벤치마크에서 2MB면 충분. (p.14, §8.6, Fig.17)
> - **Virtual Overprovisioning degree**: ρ_regular ≥ 1.4면 CGC가 항상 free section을 충분히 남김. region utilization은 CGC 시작(regular 70%) 후 1000초쯤 steady state 도달. (p.11, §8.2, Fig.10-11)
> - **Emulator fidelity**: NVMeVirt가 실제 Samsung 970 Pro와 합리적으로 일치(p.11, Fig.9). Artifact: ESOS-Lab/D2FS (Available/Functional/Reproduced 인증). (p.16, Appendix A)

## 섹션 노트
- **§1 Introduction**: filesystem GC vs device GC 오버헤드 비교(Fig.1), 세 기술 도전(filemap update, lightweight device-host interface, timely activation)과 세 기여 제시.
- **§2 Background**: flash 3연산/3단위, FTL device-level GC, LFS의 section(= GC unit) 개념, F2FS의 section/segment 분리, device-managed filesystem partition(Range Write, Nameless Write, ZNS zone_append).
- **§3 Design Principle**: Device-Driven Filemap Management, Device-Centric IO Interface, Timely Activation → CGC/Migration Upcall/Virtual Overprovisioning으로 대응.
- **§4 CGC**: F2L/L2P mapping 정의, Fig.2(원래/filesystem GC/device GC/Coupled GC 비교), Block Associative Mapping과 remap.
- **§5 Migration Upcall**: Migration Queue pair, Upcall Piggybacking, 처리 3단계, read/discard-redirect.
- **§6 Virtual Overprovisioning**: ρ_v 정의, free section fault, GC region, immediate discard.
- **§7 Crash Recovery**: redo 기반 복구의 3요소.
- **§8 Evaluation**: NVMeVirt 기반, FIO/TPC-C/YCSB/Fileserver.
- **§9 Related Work / §10 Conclusion**: append-only LFS, host L2P 관리(ZNS+, learned-index FTL) 대비.

## 핵심 용어
- **Coupled Garbage Collection (CGC)**: device-level GC가 flash free block과 filesystem free section을 동시에 회수하도록, valid page를 옮기며 L2P와 F2L(LBA)을 함께 갱신하는 기법.
- **F2L / L2P mapping**: [fd,offset]→LBA(filesystem 관리)와 LBA→PPA(FTL 관리). CGC가 둘을 모두 건드린다.
- **Block Associative Mapping**: block 단위 L2P에서 valid page를 target block의 임의 offset에 둘 수 있게 한 매핑(Block Associative Migration + remap).
- **remap**: page 이동으로 block offset이 바뀐 victim page의 LBA를 새 LBA로 갱신하는 활동. CGC의 핵심.
- **Migration record / Migration Upcall**: <old LBA, new LBA> 갱신 기록과, 이를 device→host로 NVMe queue를 통해 batch 전송하는 메커니즘.
- **Upcall Piggybacking**: upcall 통보를 NVMe completion signal의 UPCALL flag에 얹어 interrupt/polling 없이 host에 알리는 기법.
- **Migration Queue pair**: migration record 전송용 NVMe SQ/CQ. 일반 IO와 control flow가 반대.
- **read-redirect / discard-redirect**: remap과 host 반영 사이 비동기 구간에서, obsolete LBA로 온 read/discard를 outstanding record로 새 LBA에 리다이렉트.
- **Virtual Overprovisioning (ρ_v)**: filesystem partition을 물리 용량의 ρ_v배로 잡아 device가 host보다 먼저 GC를 발동하게 하는 기법.
- **GC region / regular region**: filesystem partition을 CGC 전용 LBA 할당 영역(=device capacity)과 일반 영역으로 분리.
- **Free section fault**: invalid filesystem block을 device가 몰라 회수하지 못해 device가 멈추는 상황. immediate discard로 방지.
- **Section**: LFS의 GC 단위(F2FS에서 1개 이상 segment의 집합).

## 강점 · 한계 · 열린 질문
- **강점**: filesystem-level GC를 host CPU 소모 없이 완전 제거; 새 NVMe data structure/interrupt 없이 기존 IO stack에 통합; IPLFS의 ZByte partition 대신 ρ_v≈2.4의 moderate overprovisioning으로 FTL memory를 IPLFS 대비 1/14로 절감; tail latency 대폭 개선; Artifact 3종 인증.
- **한계**: NVMeVirt **emulator** 기반 평가(실제 SSD 미검증, fidelity는 Fig.9로 주장). FTL이 filesystem의 F2L/block type을 인지하도록 수정해야 해 vendor 협력·표준화 의존. Virtual Overprovisioning으로 zoned F2FS 대비 FTL memory 2.4×. immediate discard 비용이 낮다는 가정은 "carefully suspect" 수준으로 강하게 입증되진 않음. outstanding migration record의 power-fail 보호를 supercap/spare area 존재로 가정.
- **열린 질문**: 실제 상용 SSD에서 Upcall Piggybacking의 latency/queue 경합은? read/discard-redirect가 redirect-heavy 워크로드에서 FTL 부하를 얼마나 키우는가? device GC 시점이 host write 패턴에 강하게 묶일 때(write-idle 구간 길 때) free section 공급 보장은? multi-tenant/multi-filesystem 환경으로 확장 가능한가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. D2FS가 기존 host-centric 접근(IPLFS 등)과 근본적으로 다른 점은?
> > 기존은 GC를 filesystem(host)으로 모으거나 host가 L2P까지 관리한다. D2FS는 반대로 GC 책임을 **device로 위임**하여, device-level GC가 자신의 free flash block과 host filesystem의 free section을 한 번에 회수한다(p.3 "not vice versa").

> [!question]- Q2. CGC는 어떻게 host 데이터 이동 없이 filesystem free section을 회수하나?
> > valid flash page를 새 flash block으로 옮길 때 L2P뿐 아니라 **F2L의 LBA까지 remap**한다. 그래서 valid page가 물리적·논리적으로 동시에 clustering되어, host와 device 간 data traffic 없이 filesystem section이 비워진다(§4).

> [!question]- Q3. Migration Upcall은 왜 새 interrupt를 안 만들고도 device→host 통보가 가능한가?
> > device-level GC는 host write가 빈번할 때(=NVMe completion signal이 많을 때) 주로 발동한다. 그 completion signal에 UPCALL flag를 세워 piggyback하면 충분한 통보 기회가 보장되므로, 별도 interrupt/polling이 불필요하다(§5.2).

> [!question]- Q4. remap과 host 반영 사이의 불일치 구간은 어떻게 처리하나?
> > 그 구간에 obsolete LBA로 들어온 read/discard에 대해 FTL이 mapping entry가 NULL임을 보고 outstanding migration record를 찾아 새 LBA로 **read-redirect/discard-redirect**한다. mapping entry는 host가 record를 반영한 뒤에만 회수한다(§4.4).

> [!question]- Q5. Virtual Overprovisioning의 ρ_v는 어떻게 구성되며 왜 IPLFS보다 작아도 되나?
> > partition = ρ_v × device capacity, GC region = device capacity, ρ_regular=1.4 → ρ_v=2.4. device가 host보다 먼저 free flash block을 소진해 GC를 제때 발동하기만 하면 되므로, IPLFS의 ZByte급(ρ_v=2^33) 대신 moderate 수준이면 충분하다(§6.1, §8.2).

> [!question]- Q6. free section fault란? 어떻게 막나?
> > filesystem이 invalid로 만든 block을 device가 통보받지 못해 회수할 invalid flash page가 없어 device가 멈추는 상황. D2FS는 invalidate 직후 **immediate discard**를 발행해 device가 가능한 한 빨리 알도록 한다(§6.2, §7 immediate discard).

> [!question]- Q7. 평가에서 filesystem GC와 device GC의 비용 차이는 정량적으로 얼마인가?
> > 단일 32MB superblock 회수 시 filesystem-level GC가 device-level GC보다 **3×~10×** 비싸다(Fig.16, p.13). filesystem GC 비용의 60%는 filemap traverse + page cache 할당, 약 20%는 checkpoint + section write다.

> [!question]- Q8. D2FS의 FTL memory가 zoned F2FS보다 큰데도 IPLFS보다 유리한 이유는?
> > Virtual Overprovisioning 때문에 zoned F2FS(10.4MB) 대비 2.4× (27.2MB)이지만, IPLFS(392MB, Interval Mapping FTL)의 1/14에 불과하다. IPLFS는 거대 partition 매핑 비용과 주기적 L2P compaction stall까지 부담한다(Table 1, p.14).

## 🔗 Connections
[[File System]] · [[FAST]] · [[2025]]

## References worth following
- IPLFS: Log-Structured File System without Garbage Collection (ATC 2022) — host로 GC를 모으는 정반대 접근, D2FS의 주요 baseline. [ref 40]
- F2FS: A New File System for Flash Storage (FAST 2015) — D2FS의 구현 baseline. [ref 46]
- ZNS+: Advanced Zoned Namespace Interface for In-Storage Zone Compaction (OSDI 2021) — device가 GC를 돕는 또 다른 방향. [ref 28]
- NVMeVirt: A Versatile Software-defined Virtual NVMe Device (FAST 2023) — 평가에 사용한 SSD emulator. [ref 41]
- IODA: A Host/Device Co-Design for Strong Predictability Contract on Modern Flash Storage (SOSP 2021) — host/device 협력 predictability. [ref 50]
- De-Indirection for Flash-Based SSDs with Nameless Writes (FAST 2012) — device가 LBA를 정해 host에 반환하는 선행 아이디어. [ref 84]

## Personal annotations
<본인 메모 영역>
