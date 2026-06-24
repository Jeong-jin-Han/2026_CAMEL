---
title: "AWUPF Rediscovered: Atomic Writes to Unleash Pivotal Fault-Tolerance in SSDs"
aliases: [AWUPF Rediscovered]
description: "SSD의 표준 atomic write(AWUPF) 기능을 호스트 측에서 활용해 Log-RAID의 journaling 오버헤드를 줄여 random write를 최대 3.6배 개선한 연구"
venue: FAST
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/atomic-write
  - topic/fault-tolerance
  - topic/crash-consistency
  - venue/fast
  - year/2025
---

# AWUPF Rediscovered: Atomic Writes to Unleash Pivotal Fault-Tolerance in SSDs

> **FAST 2025** · `cluster/fs` · Source: [AWUPF Rediscovered - Atomic Writes to Unleash Pivotal Fault-Tolerance in SSDs.pdf](AWUPF%20Rediscovered%20-%20Atomic%20Writes%20to%20Unleash%20Pivotal%20Fault-Tolerance%20in%20SSDs.pdf)

저자: Jiyune Jeon, Jongseok Kim (Sungkyunkwan University); Sam H. Noh (Virginia Tech); Euiseong Seo (Sungkyunkwan University)

## TL;DR
SSD는 출범 이래 flash page 단위의 atomic write를 보장해 왔고, 이는 NVMe 표준에서 **AWUPF(Atomic Write Unit Power Fail)**로 정형화되어 수 KB~수십 KB 범위를 제공한다. 그러나 호스트 측은 이 기능을 거의 활용하지 않았다. 이 논문은 transaction이 AWUPF 크기보다 작으면 WAL/journaling 없이도 atomicity를 SSD에 오프로드할 수 있음을 보이고, 이를 log-structured RAID(Log-RAID)의 crash consistency 구현에 적용한다. AWUPF 한도 내 mapping update는 journaling을 우회해 직접 쓰고(direct write), 더 큰 update만 기존 journaling을 적용하는 **dual update path**와 두 경로 간 **ordering 문제 해결** 메커니즘을 제안한다. Poseidon OS(POS)에 구현해 random write 성능을 최대 **3.6배** 개선했다.

## 문제 & 동기
File system과 DBMS는 power failure나 crash 시 on-disk consistency를 유지하기 위해 journaling 또는 WAL을 사용한다. 그러나 이 방식은 double write(먼저 journal/WAL, 그다음 실제 storage)를 요구해 성능 저하와 write amplification(WAF)을 유발한다. 한편 SSD는 flash page 단위로 atomic write를 보장하며 이는 NVMe AWUPF로 표준화되어 있으나, 저자들이 NVMe command로 조사한 결과 대부분 SSD가 4KB AWUPF를 제공하고(일부 512B, 본 연구에 쓰인 SSD는 256KB의 NAWUPF) 호스트는 이를 활용하지 않았다. transaction이 single page에 들어가면 SSD의 page-level atomicity로 transaction atomicity를 보장할 수 있는데도 이를 활용한 시도가 없었다.

저자들은 AWUPF만으로 transactional write를 달성할 수 있으며 SSD의 FTL이나 host interface 변경이 전혀 필요 없다고 주장한다. 호스트 software stack만 수정하면 일련의 write를 atomic하게 처리하는 부담을 SSD로 오프로드할 수 있다. 이를 입증하기 위해 disaggregated AFA(all-flash array)용 오픈소스 storage stack인 Poseidon OS의 Log-RAID에서 journaling 오버헤드를 줄인다.

> [!quote]- 📄 원문 표현 (paper)
> "Despite SSDs providing AWUPF ranging from several to tens of KBs, there has been little effort on the host side to utilize this capability. For instance, if a transaction is smaller than the AWUPF size, leveraging AWUPF can eliminate the need for write-ahead logging or journaling." (p.1, Abstract)
>
> "AWUPF is a feature that has been largely overlooked, and as such, it is not listed in most SSD specifications. However, by investigating the AWUPF of the SSDs we own through NVMe commands, we found that most SSDs provide a 4KB AWUPF, with some exceptions offering 512B, and in a unique case, a NAWUPF of 256KB." (p.2, §2.1)
>
> "This study is the first to show that, by adjusting the host-side metadata update mechanism, a significant portion of the crash consistency overhead can be offloaded to SSDs without altering the storage devices." (p.2, §2.1)

## 핵심 통찰

> [!note]- 통찰 1: AWUPF는 이미 존재하는 무료 atomicity 자원이다
> SSD는 flash page mapping을 page write 완료 후에만 갱신하므로 본질적으로 page-level atomicity를 제공한다(unexpected crash 시 capacitor로 mapping 전송 완료 보장). 이는 NVMe AWUPF/NAWUPF로 표준화되어 있다. transaction이 AWUPF 크기 안에 들어가면 host-side journaling 없이 SSD가 "전부 옛 데이터 또는 전부 새 데이터"를 보장한다. 기존 연구(X-FTL, CFS 등)는 FTL이나 host interface 변경을 요구했지만, 이 논문은 변경 없이 host metadata update 패턴만 조정한다.

> [!note]- 통찰 2: metadata update 빈도와 크기에 따라 경로를 나눠라 (dual update path)
> POS Log-RAID에서 VSA map과 stripe map 두 metadata가 journaling을 필요로 한다. VSA map은 LBA가 쓰일 때마다 갱신되어 빈번하지만 update가 작고, stripe map은 stripe가 채워질 때만 갱신되어 드물다. 따라서 자주 갱신되어 journaling 부담이 큰 VSA map에 대해 AWUPF를 활용한다. 수정될 metadata 크기가 AWUPF 이내면 storage의 mpage를 직접 갱신(direct write), 초과하면 기존 journaling 경로를 택한다. 최악의 경우(모든 크기가 AWUPF 초과)에도 기존 방식보다 나쁘지 않음이 보장된다.

> [!note]- 통찰 3: 두 경로 혼용 시 ordering이 정확성의 핵심이다
> 같은 mpage가 journaling 경로와 direct 경로로 동시에 갱신되고 crash가 나면 partial update나 stale data가 발생할 수 있다. 핵심은 "mpage의 최신 버전은 write가 issue된 순서가 아니라 완료(completion) 순서로 결정된다"는 점이다(command queue가 out-of-order 실행). 이를 해결하기 위해 각 mpage에 64-bit header(63-bit version + 1-bit in-use flag)를 두고, recovery 시 commit log와 metadata area의 version을 비교해 최신본을 결정한다.

## 설계 / 메커니즘

> [!abstract]- Log-RAID와 metadata 구조 (Poseidon OS)
> POS는 여러 SSD를 하나의 Log-RAID로 묶어 NVMe-oF로 virtual block device를 노출하는 오픈소스 AFA stack(Inspur·Samsung 개발). data stripe를 log-structured 방식으로 순차 기록해 random write를 줄이고 WAF를 낮춘다. 단, stripe의 physical 위치가 매 update마다 바뀌므로 logical↔physical mapping을 유지해야 하고, partial update/crash가 RAID consistency를 깨지 않도록 mapping metadata의 journaling이 필수다.
> - AFA를 **metadata partition(RAID-10)**과 **data partition**으로 분할. metadata는 4KB segment인 **mpage** 단위로 checkpoint 시 기록.
> - **2-level mapping**: (1) virtual block device block → virtual address(VSID + virtual offset), 이는 **VSA map**. (2) virtual address → logical address(LSID + logical offset), 이는 **stripe map**. LSID/logical offset과 RAID chunk size의 division/modulus로 실제 SSD와 LBA 결정.
> - 각 VSA map mpage entry는 4KB logical block을 매핑하므로 한 mpage가 약 2MB의 virtual device block 매핑 정보를 담는다.

> [!abstract]- Dual update path (§3.1)
> AWUPF는 single write command에 대해서만 atomicity를 보장하므로, atomic 보장이 필요한 metadata 수정은 전부 AWUPF 크기의 연속 공간 안에 들어가야 한다. 단일 write command로 VSA map과 stripe map을 동시에 갱신할 수 없으므로(둘 다 storage에 직접 쓰면 consistency 위험), 자주 갱신되는 **VSA map만 AWUPF로 journaling을 우회해 storage의 mpage를 직접 갱신**한다.
> - 수정될 metadata 크기 ≤ AWUPF → metadata area를 직접 갱신(direct write).
> - 크기 > AWUPF (예: request size > 2MB, 또는 작아도 두 mpage에 걸침) → 기존 journaling 경로.
> - 예시(p.3, §3.1): 10KB(A, 3 entry)·12KB(B, 3 entry, 시작 offset 때문에 두 mpage에 걸침)·4KB(C, 1 entry) write 중 B만 journaling, A·C는 single mpage라 direct update.

> [!abstract]- Ordering 문제와 해결 (§3.2, Fig.3·4)
> 두 가지 complication:
> 1. **journaling vs direct (Fig.3a)**: A가 mpage 0·1을 메모리에서 수정·commit 진행 중 crash. in-memory metadata는 journaling 시작 *전*에 갱신되므로 mpage 1이 A₁을 반영한 채 B의 direct write로 storage에 영속화될 수 있음 → partial update. 해결: in-memory mpage 복사본을 journal commit이 write를 *완료한 뒤*에 수정. crash가 commit 완료 전이면 B의 데이터는 AWUPF 속성으로 폐기되어 consistency 유지.
> 2. **stale version (Fig.3b)**: A에 대한 commit 완료 후 mpage 0이 다시 direct 수정되고 crash → recovery가 journal의 마지막 entry(A₀, 낡은 값)로 잘못 복원할 수 있음.
> - 해결: mpage마다 **64-bit header (63-bit version + 1-bit in-use flag)**. commit/direct update 시작 시 version +1. recovery에서 commit log version과 metadata area의 mpage version을 비교해 최신본 결정.
> - **in-use flag**: commit 시작 시 set. direct write가 in-use mpage를 노릴 때는 version을 올리지 않아(Fig.4a), 더 낮은 version의 commit이 잘못 폐기되는 것을 방지하고 두 write 모두 보존.
> - recovery: commit log의 mpage version ≥ metadata area version일 때만 replay. 같은 version이면 동일 entry가 동시에 수정되지 않았다는 뜻(VFS의 page-level lock 보장). version이 같거나 클 때 merge해 최신본 획득.

## 평가

> [!success]- 평가 요약 (수치)
> **플랫폼(Table 1, p.5)**: Target — Xeon Gold 6336Y 24C/48T ×2, DDR4 32GB ×16, Samsung PM9A3 E1.S 3.84TB ×10 storage array, ConnectX-5 100GbE, Linux 5.3.0. Initiator — Xeon Gold 6342 ×2, DDR4 32GB ×32, E810-CQDA2 100GbE, Linux 5.4.0. 사용 SSD: Western Digital Ultrastar DC ZN540 (4KB AWUPF). 비교군: **Journaled**(원본 POS), **Direct**(consistency 보장 없이 VSA map을 metadata area에 직접 기록), **Our Approach**. 결과는 5회 평균.
>
> **Micro-benchmark (Fig.5, p.5–6)**: FIO block size 고정, metadata reactor 수 변화.
> - 4KB random write: Direct가 reactor 증가에 따라 Journaled 대비 최대 **3.3배** throughput. Our Approach는 모든 write가 single entry 수정이라 AWUPF direct에 의존, Direct와 거의 동일.
> - 1MB write size: random write에서 Our Approach가 Journaled 대비 평균 **20%** 우위(ext4 기본 stripe로 큰 request가 분할되어 direct 비중 증가). sequential write는 세 구성 모두 6 reactor에서 saturate, 비슷.
>
> **FIO request size (Fig.6, p.6)**: sequential은 세 구성 유사. **4KB random write에서 Our Approach가 Journaled 대비 3.6배** 개선. request size 증가 시 Journaled의 checkpoint 오버헤드 감소로 gap 축소, 2MB write에서는 direct 빈번해 Our Approach가 Direct에 근접.
>
> **Comprehensive (Filebench, Fig.7, p.6)**: varmail, fileserver, OLTP, tpcso.
> - **varmail**: Our Approach가 Journaled 대비 32 thread에서 **14%**, Direct는 **31%** 우위.
> - **OLTP**: Our Approach가 Direct와 유사, Journaled 대비 16 thread에서 **21%**~32 thread에서 **73%** 개선.
> - **tpcso**: write intensity가 낮아 개선폭은 작으나, 여러 mpage 갱신 시 Direct는 mpage index별 thread 분산으로 straggler가 생기는 반면 Our Approach는 작은 journal commit + checkpoint 시 단일 mpage write 병합으로 Direct를 능가하기도 함.
> - **fileserver**: 큰 sequential write(>512KB가 요청 수의 45%, write volume의 94%) 비중이 높아 세 구성 모두 큰 차이 없음.

## 섹션 노트
- **§1 Introduction**: SSD page-level atomicity의 본질과 AWUPF 표준 소개, 호스트 미활용 지적, Log-RAID 대상 dual update path 제안 요약. POS/FIO/Filebench로 평가.
- **§2 Background**: §2.1 SSD atomic write(capacitor 기반 mapping 보장, AWUPF/NAWUPF 정의, NVMe command 조사 결과, X-FTL[13]·CFS[20] 등 선행연구와 차별점), §2.2 POS Log-RAID 설계(metadata/data partition, mpage, 2-level mapping, VSA/stripe map, JBD2 변형 journal). Fig.1 mapping 구조, Fig.2 journaling on/off에 따른 write 성능(작은 request일수록 격차 큼).
- **§3 Our Approach**: §3.1 Design Overview(dual update path, AWUPF 한도 판단, 최악 case 보장), §3.2 Ordering Complications and Resolutions(Fig.3 두 complication, Fig.4 64-bit version/in-use flag로 해결, recovery 절차).
- **§4 Evaluation**: §4.1 환경, §4.2 micro-benchmark(reactor 확장성), §4.3 comprehensive workloads(varmail/fileserver/OLTP/tpcso).
- **§5 Conclusion**: AWUPF를 host consistency에 활용한 최초 시도, Log-RAID에서 최대 3.6x. file system도 metadata update를 AWUPF 안에 들도록 재설계하면 이득 가능.

## 핵심 용어
- **AWUPF (Atomic Write Unit Power Fail)**: power failure 상황에서 SSD가 atomicity를 보장하는 write 크기. NVMe 표준의 mandatory 기능으로, 이 크기 내 write는 전부 옛 값 또는 전부 새 값만 반환한다.
- **NAWUPF (Namespace AWUPF)**: 단일 namespace 내에서의 AWUPF 크기를 명시하는 값. 본 연구 SSD는 256KB NAWUPF.
- **Log-RAID**: data stripe를 log-structured 방식으로 순차 기록하는 RAID 아키텍처. random write와 WAF를 줄이지만 매 update마다 stripe의 physical 위치가 바뀌어 mapping journaling이 필요하다.
- **Poseidon OS (POS)**: Inspur·Samsung이 개발한 오픈소스 disaggregated AFA storage stack. NVMe-oF interface 제공, SPDK 기반.
- **VSA map (Virtual Stripe Address map)**: virtual block device block을 virtual address(VSID+offset)로 매핑하는 first-level mapping. 자주 갱신되며 본 연구가 AWUPF로 직접 갱신하는 대상.
- **stripe map**: virtual address를 logical address(LSID+offset)로 매핑하는 second-level mapping. stripe가 채워질 때만 드물게 갱신.
- **mpage**: metadata를 storage에 기록하는 4KB 단위 segment. 각 entry는 64-bit header(version 63-bit + in-use flag 1-bit)를 갖는다.
- **dual update path**: AWUPF 이내 metadata update는 journaling을 우회해 직접 쓰고(direct write), 초과 update는 기존 journaling을 적용하는 전략.
- **in-use flag**: commit이 진행 중인 mpage 표시. direct write가 in-use mpage를 노릴 때 version을 올리지 않아 두 write를 모두 보존한다.

## 강점 · 한계 · 열린 질문
- **강점**: SSD/FTL/host interface 변경 없이 software stack만 수정해 기존 표준 기능을 재발견·활용. random write 3.6x 개선. ordering correctness를 version+in-use flag로 명료하게 해결. 오픈소스 POS에 실제 구현.
- **한계**: AWUPF가 작으면(다수 SSD가 4KB) 효과를 보는 transaction 크기가 제한적. fileserver처럼 큰 sequential write가 지배하는 워크로드에선 이득이 미미. Log-RAID/POS라는 특정 시스템에 특화된 평가. file system 일반화는 향후 과제로만 언급.
- **열린 질문**: 일반 file system(ext4/XFS) metadata update를 AWUPF 안에 packing하도록 재설계할 때의 실효성은? AWUPF가 더 큰(256KB급) SSD가 보편화되면 적용 범위가 얼마나 넓어지는가? DBMS WAL을 AWUPF로 대체하는 것의 이득/제약은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. AWUPF가 정확히 무엇이고 왜 "rediscovered"인가?
> 답: AWUPF는 NVMe 표준의 mandatory 기능으로, power failure 시 해당 크기 내 write의 atomicity(전부 옛 값 또는 전부 새 값)를 SSD가 보장하는 단위다. SSD가 flash page mapping을 page write 완료 후에만 갱신하는 본질적 특성에서 비롯된다. 대부분 SSD가 이미 제공하지만(주로 4KB) host가 거의 활용하지 않아 spec에도 잘 안 적히는 "간과된" 기능이라, 이를 host consistency에 처음 활용한다는 의미로 rediscovered라 부른다.

> [!question]- Q2. 왜 stripe map이 아니라 VSA map에 AWUPF를 적용했나?
> 답: VSA map은 LBA가 쓰일 때마다 갱신되어 매우 빈번하고 update가 작은 반면, stripe map은 stripe가 채워질 때만 드물게 갱신된다. journaling 오버헤드가 큰 것은 빈번한 VSA map이므로 여기에 AWUPF direct write를 적용해야 효과가 크다. 또한 single write command는 두 map을 동시에 atomic하게 갱신할 수 없으므로 한쪽만 직접 갱신해야 한다.

> [!question]- Q3. dual update path에서 경로 선택 기준은?
> 답: 수정될 metadata 크기가 AWUPF 이내이고 single mpage에 들어가면 storage mpage를 직접 갱신(direct, journaling 우회). 크기가 AWUPF를 초과하거나 두 mpage에 걸치면 기존 journaling 경로를 사용한다. 예로 single mpage만 건드리는 4KB·10KB write는 direct, 시작 offset 때문에 두 mpage에 걸치는 12KB write는 journaling.

> [!question]- Q4. journaling과 direct 경로를 섞으면 어떤 정확성 문제가 생기나?
> 답: (1) Partial update — A가 두 mpage를 commit 중일 때 B의 direct write가 그중 한 mpage를 영속화하면, A의 in-memory 변경이 섞여 partial 상태로 남을 수 있다. (2) Stale version — A commit 완료 후 같은 mpage가 direct로 다시 갱신되고 crash 나면, recovery가 journal의 오래된 값으로 복원할 위험이 있다.

> [!question]- Q5. version과 in-use flag는 각각 어떻게 정확성을 보장하나?
> 답: 각 mpage entry에 64-bit header(63-bit version + 1-bit in-use)를 둔다. commit/direct update 시작 시 version을 +1 하고, recovery 때 commit log version과 metadata area version을 비교해 더 크거나 같은 쪽을 최신으로 택한다. in-use flag는 commit 진행 중인 mpage를 표시해, direct write가 그 mpage를 노리면 version을 올리지 않아 낮은 version의 commit이 잘못 폐기되지 않고 두 write가 모두 보존되도록 한다. 핵심 통찰은 최신본이 issue 순서가 아닌 completion 순서로 결정된다는 점이다.

> [!question]- Q6. 핵심 성능 수치는?
> 답: 4KB random write(FIO)에서 Our Approach가 원본 Journaled 대비 3.6배. micro-benchmark에서 Direct가 Journaled 대비 최대 3.3배(reactor 확장). Filebench varmail에서 14%(32 thread), OLTP에서 21~73% 개선. 큰 sequential write 지배 워크로드(fileserver)는 차이 미미.

> [!question]- Q7. 이 접근의 가장 큰 실용적 장점은?
> 답: SSD 펌웨어(FTL)도, host interface도 바꾸지 않고 오직 host-side software stack(metadata update 메커니즘)만 수정한다. 선행 연구(X-FTL, CFS)가 FTL/interface 변경을 요구한 것과 대비되며, 기존 표준 SSD에 즉시 적용 가능하다.

> [!question]- Q8. 한계와 일반화 가능성은?
> 답: 다수 SSD의 AWUPF가 4KB로 작아 적용 가능한 transaction이 작은 경우로 제한된다. 큰 sequential write 워크로드에선 이득이 거의 없다. 평가가 Log-RAID/POS에 특화되어 있고, 일반 file system이 metadata update를 AWUPF 안에 packing하도록 재설계하면 이득 가능하다는 점은 향후 과제로만 제시된다.

## 🔗 Connections
[[File System]] · [[FAST]] · [[2025]]

## References worth following
- [13] Woon-Hak Kang et al. **X-FTL: transactional FTL for SQLite databases.** SIGMOD 2013 — FTL을 transaction 단위로 다루는 선행 연구, 본 논문의 핵심 대비군.
- [20] Changwoo Min et al. **Lightweight application-level crash consistency on transactional flash storage.** ATC 2015 — SSD transactional write interface(CFS)를 활용한 crash consistency.
- [21] Xiangyong Ouyang et al. **Beyond block I/O: Rethinking traditional storage primitives.** HPCA 2011 — atomic write를 storage primitive로 재고하는 관점.
- [22] Sunhwa Park et al. **Atomic write FTL for robust flash file system.** ISCE 2005 — atomic write FTL 초기 연구.
- [28] Youjip Won et al. **Barrier-enabled IO stack for flash storage.** FAST 2018 — out-of-order completion과 ordering 문제 배경.
- [5] **NVM express NVM command set specification 1.0c**, 2022 — AWUPF/NAWUPF 표준 정의 원전.

## Personal annotations
<본인 메모 영역>
