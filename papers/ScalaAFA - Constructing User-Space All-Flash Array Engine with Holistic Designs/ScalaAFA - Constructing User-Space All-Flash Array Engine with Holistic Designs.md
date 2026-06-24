---
title: "ScalaAFA: Constructing User-Space All-Flash Array Engine with Holistic Designs"
aliases: [ScalaAFA]
description: "SPDK 기반 user-space AFA 엔진: lock-free 권한 관리 + conversion/패리티의 SSD 오프로딩 + OOB 메타데이터 영속화로 차세대 SSD 처리량을 낮은 CPU로 확장"
venue: ATC
year: 2024
tier: deep
status: done
tags:
  - paper
  - cluster/infra
  - topic/all-flash-array
  - venue/atc
  - year/2024
---

# ScalaAFA: Constructing User-Space All-Flash Array Engine with Holistic Designs

> **ATC 2024** · `cluster/infra` · Source: [ScalaAFA - Constructing User-Space All-Flash Array Engine with Holistic Designs.pdf](ScalaAFA%20-%20Constructing%20User-Space%20All-Flash%20Array%20Engine%20with%20Holistic%20Designs.pdf)

저자: Shushu Yi (Peking Univ. & Zhongguancun Lab), Xiurui Pan (Peking Univ.), Qiao Li (Xiamen Univ.), Qiang Li (Alibaba), Chenxi Wang (UCAS), Bo Mao (Xiamen Univ.), Myoungsoo Jung (KAIST & Panmnesia), Jie Zhang (Peking Univ. & Zhongguancun Lab)

## TL;DR
ScalaAFA는 SPDK 위에 올린 user-space **All-Flash Array (AFA)** 엔진으로, (1) lock 대신 message-passing 기반 쓰기 권한 관리로 SPDK의 lock-free 원칙을 지키고, (2) two-phase write의 conversion(데이터 수집·패리티 계산)을 **SSD 내부 자원(XOR engine)으로 투명하게 오프로딩**하며, (3) 매핑 메타데이터를 SSD **OOB 영역**에 영속화하고 SLC buffer eviction을 조절해 write amplification을 줄인다. 결과적으로 기존 최신 AFA 엔진 대비 write throughput을 **2.5배**, 평균 latency를 **52.7%** 개선한다.

## 문제 & 동기
차세대 고성능 SSD(예: Samsung PM1743 PCIe 5.0, 최대 13 GB/s)가 등장하면서, 기존 AFA 엔진은 I/O 경로의 소프트웨어 오버헤드 때문에 SSD 어레이의 성능을 끌어내지 못하고 오히려 병목이 된다. 구체적으로 (1) user-kernel context switch와 두꺼운 block layer, (2) AFA 내부 작업(패리티 준비 등)이 host CPU를 과도하게 소모한다. 최신 two-phase write 엔진 FusionRAID조차 4+1 어레이에서 replication 시 4.8 GB/s(이상치의 36.9%)만 달성하며, storage software stack이 CPU의 54.4%를 쓰고 SSD I/O 대기는 20.8%에 불과하다.

저자들은 4개의 challenge를 정리한다: **C1** 소프트웨어 stack 오버헤드(context switch + block layer), **C2** conversion phase의 background read/compute/write가 CPU 점유(Conv가 65.7%, 그중 read/write가 지배적), **C3** out-of-place 업데이트로 인한 매핑 메타데이터 영속화 비용(undo/redo log), **C4** replication으로 인한 write amplification(4+1에서 총 3.5배)이 SSD 수명 단축.

SPDK는 user-space lock-free stack으로 C1을 해결할 수 있으나, 기존 AFA는 다중 스레드 동기화에 lock을 쓰므로 SPDK의 lock-free 원칙과 충돌하고, two-phase write의 내재적 단점(C2~C4)도 그대로 남는다.

> [!quote]- 📄 원문 표현 (paper)
> "existing AFA engines inflict substantial software overheads on the I/O path, such as the user-kernel context switches and AFA internal tasks (e.g., parity preparation), thereby failing to adopt next-generation high-performance SSDs." (p.1, Abstract)
>
> "During replication, FusionRAID only achieved 4.8 GB/s write throughput, a mere 36.9% of the ideal performance. This is attributed to the storage software stack (e.g., user-kernel context switches and tedious block layers) consuming 54.4% of the CPU cycles, whilst the CPU stall time for SSD I/O only accounts for 20.8%." (p.1, §1)
>
> "directly incorporating existing AFA solutions into SPDK poses significant challenges. First, existing AFA solutions rely on locks for concurrent multi-thread access, whereas SPDK operates based on a lock-free principle. Furthermore, SPDK cannot mitigate the intrinsic shortcomings of existing AFA schemes (e.g., two-phase write)." (p.1-2, §1)

## 핵심 통찰

> [!note]- 핵심 통찰 (토글)
> - **SPDK-호환 message-passing이 lock의 대안이 될 수 있다.** SPDK의 lock-free 원칙을 깨지 않으면서 다중 스레드 쓰기 권한 동기화를 하려면, lock 대신 batch 단위로 권한을 grant/retrieve하는 message-passing 권한 관리가 핵심이다. 이 통찰로 write throughput 58.4% 증가, 평균 latency 45.2% 감소.
> - **SSD에 이미 있는 가용 자원(XOR engine 등)이 AFA 엔진의 소프트웨어 제약을 극복할 수 있다.** 패리티 계산은 SSD 임베디드 XOR engine으로 거의 공짜(64 KB 패리티에 20 µs, 16 mW)지만, 진짜 병목인 conversion phase의 background read/write는 단순 계산 오프로딩만으로는 못 없앤다 → 데이터를 패리티가 저장될 SSD로 **투명하게 모으는 data placement policy**가 필요.
> - **redundant write(backup copy)는 transient하다.** replication phase의 backup copy는 conversion 직후 invalidate되므로, vulnerable한 MLC block으로 flush할 필요가 없다. SSD-internal 고내구성 SLC buffer에 머물게 하고 transient tag로 표시해 SLC evictor가 낮은 우선순위로 처리하면 write amplification 피해를 줄인다.

## 설계 / 메커니즘

> [!abstract]- 설계 / 메커니즘 (토글)
> **전체 구조 (Fig.4).** ScalaAFA는 user-space의 `ScalaAFA Bdev`(SPDK block device)와 `ScalaAFA Firmware`(SSD 펌웨어 수정)로 구성. Bdev는 Space Manager, Granted Heroes, Placement Policy, Mapping Tables를 담고, 펌웨어는 FTL에 SLC Evictor와 Conversion 모듈을 추가한다.
>
> **(C1) Storage Space Abstraction — Hero (Fig.5).** 사용자 관점은 표준 block device(user address space). 내부적으로 쓰기는 AFA address space에 순차 logging되고, 연속된 k slot이 한 stripe를 이룬다. SSD 저장 공간을 **normal area**(conversion 후 데이터)와 **transient area**(replicated 데이터, SLC buffer 용량으로 크기 결정)로 분할. k+m SSD에서 같은 SSD logical address를 가진 k+m chunk = **horizontal hero (HH)**; transient area에서 같은 SSD의 연속 k chunk = **vertical hero (VH)**. HH 1개 + VH m개 = **hero group (HG)**. HG로 stripe의 데이터·패리티 위치를 추적.
>
> **(C1) Lock-free 권한 관리 (Fig.6).** Space Manager(daemon thread)가 모든 hero의 쓰기 권한을 소유. I/O thread가 시작되면 VH/HH를 요청하고, Space Manager가 **batch 단위로 grant**(예: 1024×m VH + 1024 HH 한 번에). thread는 이미 grant된 hero로만 쓸 수 있어 lock 없이 충돌 방지. Retrieve 4가지: thread 종료 시 반납 / HG가 채워지면 Full HG List에 삽입 / CPU idle·conversion 시 회수 / hero 고갈 시 broadcast로 회수. AFA-level GC로 stale hero 재활용.
>
> **(C2) Conversion Offloading (Fig.7).** replication phase: 한 data chunk를 m+1 copy로 복제, primary copy는 HH에, backup copy m개는 VH에 저장. conversion phase: host가 NVMe command set을 확장한 **conversion command**(Src=VH offset, Dst=패리티 저장 logical addr, ChunkSize, Num=k, Ptype=패리티 종류)를 stripe switcher로 SSD에 전송. SSD가 로컬에서 Num개 chunk를 internal DRAM으로 읽어 Ptype에 따라 패리티 계산 후 로컬에 저장하고, VH의 데이터를 invalid 처리. → host-SSD I/O와 host의 데이터 전송 제거. 이로써 throughput 36.9% 증가, latency 36.1% 감소.
>
> **(C3) 메타데이터 영속화 (Fig.8).** host memory에 AFA Mapping Table(AMT: user addr→AFA chunk number)과 Hero Group Mapping Table(HGMT: stripe→VH list)을 유지. 영속화를 위해 둘을 segmentable **Persistent Mapping Table (PMT)**로 변환(각 entry = 8-bit slot number + 32-bit SSD LPN). PMT를 SSD LPN 기준으로 slice해 write request에 piggyback하여 데이터와 함께 **OOB 영역**에 single program operation으로 기록(추가 write 없음). chunk당 72-bit spare OOB 사용. battery-backed DRAM(비용·결함 한계) 대신 OOB 활용. 6+3, 2TB SSD에서 약 928 MB(0.005%) host memory만 필요. 복구 시 OOB scan + periodical checkpoint로 가속.
>
> **(C4) Write Amplification 완화 — SLC Evictor.** backup copy는 transient하므로 MLC로 flush 불필요. write request의 한 bit로 transient tag를 표시, SLC buffer가 가득 차면 evictor가 `score = factor*num_invalid − num_transient`(factor=1) 기준으로 evict 우선순위 결정 → transient page 많은 block은 늦게 evict(곧 invalidate되므로).
>
> **구현.** SPDK v22.05에 6K LOC, SSD emulator(FEMU)에 1K LOC. `spdk_env_thread_launch_pinned()`로 daemon을 고정 CPU에 bind. NVMe Protection Information feature로 user chunk number/slot/timestamp를 OOB에 piggyback. GitHub: ChaseLab-PKU/ScalaAFA.

## 평가

> [!success]- 평가 (토글)
> **환경.** FEMU QEMU 기반 SSD emulator, Intel Xeon 5320 (32 vCPU), 32 GB DRAM, 최대 8 NVMe SSD(7500/4890 MB/s peak read/write), SLC buffer 4 GB. 비교 대상: mdraid, ScalaRAID, stRAID(stripe write), RAID5F(불완전 stripe write, full-stripe만), FusionRAID(two-phase write SOTA), ScalaAFA. chunk size 64 KB. (p.9, §6.1, Table 1)
>
> **Microbenchmark throughput (Fig.9).** stRAID는 mdraid 대비 64 KB sequential write에서 평균 17.9% 높음(run-to-complete). FusionRAID는 stRAID 대비 64 KB random write에서 평균 3.6배. ScalaAFA는 FusionRAID 대비 64 KB sequential write에서 2+1/4+1/6+1/6+2 AFA에서 각각 1.6×/2.4×/2.4×/2.5× throughput. RAID5F는 2+1에서 ScalaAFA보다 12.4% 우위지만 4+1/6+1에서는 17.4%/13.1% 열위. (p.10, §6.2)
>
> **Microbenchmark latency (Fig.10).** ScalaRAID는 mdraid 대비 full-stripe random write에서 평균/99th latency를 22.9%/15.0% 감소. ScalaAFA는 FusionRAID 대비 64 KB sequential에서 평균/99th를 66.1%/48.2% 감소, full-stripe random에서 평균/99th를 52.7%/41.9% 감소. RAID5F(이상적 software-only) 대비도 99th tail latency 4.8% 단축. (p.10, §6.2)
>
> **Scalability (Fig.11).** I/O thread 1→12 변화. mdraid는 8 thread에서 peak(lock 오버헤드). ScalaAFA는 **1 thread만으로 11.4 GB/s** 달성(4+1 이상치와 거의 동일), stRAID/ScalaRAID는 12 thread로 11.0/11.3 GB/s. 6+1에서 FusionRAID는 1 thread로 6.4 GB/s(이상치의 37.5%)인 반면 ScalaAFA는 1/2 thread로 12.0/15.7 GB/s. (p.10-11, §6.2)
>
> **Real workloads (MSRC/MSPC/FIU traces, Fig.12-13).** ScalaAFA는 FusionRAID 대비 throughput 평균 2.9×/2.2×/1.2×(prn/src2/DAP), 완료 시간 평균 30.0% 단축. latency CDF 모든 워크로드에서 최고. FusionRAID 대비 99th latency를 src2/CFS/webmail에서 59.1%/65.1%/72.5% 감소. (p.11, §6.3)
>
> **End-to-end RocksDB (db_bench, Fig.14).** ScalaAFA는 FusionRAID 대비 평균 throughput을 fillrandom/fillseq/overwrite에서 31.9%/41.1%/23.8% 개선(fillseekseg는 8.1%, DRAM cache로 I/O 흡수). (p.11, §6.4)
>
> **개별 기여 분석 (Fig.15-16).** lock-free user-space 설계(Scala-CO vs FusionRAID): 평균 latency 45.2% 감소, throughput 58.4% 증가. conversion offloading(ScalaAFA vs Scala-CO): 평균 latency 36.1% 감소, throughput 39.6%/36.1%/37.9%/34.0% 개선(64 KB seq/rand, full-stripe seq/rand). Write count(ScalaAFA vs Scala-WR): MLC write를 proxy0/prn/src2/webmail에서 38.6%/12.2%/37.7%/28.4% 감소. (p.11-12, §6.5)
>
> **종합 (Table 3).** ScalaAFA만 C1(소프트웨어 stack), C2(conversion), C3(메타데이터 영속화), C4(write amplification)를 모두 Solved/Mitigated. 종합적으로 write throughput 2.5×, 평균 latency 52.7% 개선. (p.12, §7)

## 섹션 노트
- **§1 Introduction**: AFA 배경, FusionRAID 재현 실험으로 문제 정량화, 3대 기여(user-space lock-free 엔진, conversion 오프로딩, two-phase write의 holistic 최적화).
- **§2 Background**: SSD internal(controller/XOR engine/flash backbone, SLC vs MLC, OOB, FTL/GC), AFA의 두 유형(stripe write의 read-construct-write 문제 vs two-phase write의 replication+conversion).
- **§3 Preliminary Study**: replication phase challenge(C1), conversion phase challenge(C2, Conv 65.7% CPU), 내재 이슈(C3 메타데이터, C4 write amplification 3.5×).
- **§4 Overview**: SPDK 채택 + SSD-internal 자원 활용, 4 challenge 대응 개요, write path에 집중(read path는 기존 설계 따름).
- **§5 Design**: 5.1 Hero 공간 추상화, 5.2 lock-free message-passing 권한, 5.3 write path 진화(placement policy + conversion offloading), 5.4 OOB 메타데이터 영속화, 5.5 SLC evictor, 5.6 구현.
- **§6 Evaluation**: microbenchmark/real workload/RocksDB/scalability/개별 기여.
- **§7 Related Work**: AFA 오버헤드(ScalaRAID, stRAID, FusionRAID, EAR), in-storage processing(HolisticGNN, Cognitive SSD, Willow), trade-off(SSD 펌웨어 수정 필요).

## 핵심 용어
- **All-Flash Array (AFA)**: 여러 SSD를 단일 논리 단위로 묶어 용량·처리량을 집약하고 결함 내성을 제공하는 스토리지 조직.
- **Stripe write AFA**: 데이터를 k data + m parity chunk의 stripe로 k+m SSD에 분산 저장. partial write 시 read-construct-write로 지연.
- **Two-phase write AFA**: replication phase(m+1 복제로 small write 흡수) + conversion phase(복제본을 읽어 패리티 계산 후 space-efficient striping으로 재배치). small write에 유리하나 메타데이터·write amplification 문제 존재.
- **SPDK**: user-space, lock-free, polling 기반 NVMe driver를 제공하는 스토리지 프레임워크. Bdev block device 추상화 사용.
- **Hero / Horizontal Hero (HH) / Vertical Hero (VH) / Hero Group (HG)**: ScalaAFA의 저장 공간 추상 단위. HH=k+m SSD의 동일 logical addr chunk, VH=transient area의 동일 SSD 연속 k chunk, HG=HH 1개+VH m개.
- **Conversion command**: NVMe command set을 확장해 SSD가 로컬에서 데이터를 모으고 패리티를 계산·저장하도록 하는 명령(Src, Dst, ChunkSize, Num, Ptype).
- **OOB (out-of-band area)**: flash page에 부속된 메타데이터 영역(ECC 등). ScalaAFA가 PMT slice를 추가 write 없이 영속화하는 데 활용.
- **PMT (Persistent Mapping Table)**: AMT+HGMT를 OOB에 저장 가능하도록 변환한 segmentable 매핑 테이블(8-bit slot + 32-bit SSD LPN).
- **SLC buffer / SLC evictor**: 고내구성 SLC block을 write buffer로 사용. evictor가 transient tag 기반 score로 evict 우선순위를 정해 transient backup copy의 MLC flush를 회피.
- **Write amplification**: 한 번의 논리 write가 유발하는 실제 물리 write의 배수. two-phase write의 replication이 m+1배를 유발(4+1에서 총 3.5×).

## 강점 · 한계 · 열린 질문
- **강점**: SPDK lock-free 원칙을 유지하면서 AFA를 user-space로 옮긴 최초 연구. 계산뿐 아니라 conversion의 background I/O까지 SSD로 투명 오프로딩해 진짜 병목 제거. OOB 활용으로 메타데이터 영속화를 추가 write 없이 달성(host memory 0.005%). 1 thread만으로 multi-thread 이상치에 근접하는 확장성. artifact evaluated(available+functional).
- **한계**: SSD 펌웨어 수정(conversion 모듈, SLC evictor, NVMe command 확장)이 필수 → off-the-shelf SSD에 적용 불가, SSD 벤더 협력 필요(저자도 trade-off로 인정). 평가가 실제 하드웨어가 아닌 FEMU emulator 기반. read path 최적화는 다루지 않음(기존 설계 차용). RAID5F가 2+1 같은 작은 어레이에서는 ScalaAFA보다 우위.
- **열린 질문**: 실제 PCIe 5.0 SSD 하드웨어에서 emulator 결과가 재현되는가? conversion command 표준화/벤더 채택 경로는? SSD 내부 XOR/연산 자원이 더 복잡한 erasure code(예: LRC)나 더 큰 m에서도 충분한가? CXL/disaggregated 환경으로의 확장은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 기존 AFA 엔진을 SPDK에 그냥 올리지 못하는 두 가지 이유는?
> > (1) 기존 AFA는 다중 스레드 동시 접근에 lock을 쓰는데 SPDK는 lock-free 원칙으로 동작해 충돌한다. (2) SPDK는 two-phase write의 내재적 단점(conversion의 background I/O, 메타데이터 영속화, write amplification)을 해결해주지 못한다. (p.1-2, §1)

> [!question]- Q2. ScalaAFA가 lock 없이 다중 스레드 쓰기 충돌을 막는 방법은?
> > Space Manager(daemon thread)가 모든 hero의 쓰기 권한을 소유하고, message-passing으로 I/O thread에 **batch 단위로 권한을 grant**한다. thread는 이미 grant된 HH/VH로만 쓸 수 있어 서로 다른 user addr가 같은 SSD 위치에 매핑되지 않는다. 권한은 4가지 방식으로 retrieve된다.

> [!question]- Q3. 패리티 계산만 SSD로 오프로딩하면 안 되고 conversion 전체를 오프로딩해야 하는 이유는?
> > 패리티 계산은 XOR engine으로 거의 공짜(64 KB에 20 µs, 16 mW)지만, conversion phase의 진짜 병목은 다른 SSD에 흩어진 데이터를 host CPU가 읽어 target SSD로 복사하는 background read/write(Conv-Rd/Conv-Wr)다. 따라서 데이터를 패리티가 저장될 SSD로 투명하게 모으는 placement policy + conversion command로 host-SSD I/O 자체를 제거해야 한다. (p.6, §4)

> [!question]- Q4. 매핑 메타데이터를 battery-backed DRAM이 아니라 OOB에 저장하는 이유와 방법은?
> > battery-backed DRAM은 비용이 크고 DRAM 결함 시 데이터 손실 위험이 있으며 internal DRAM은 용량(수십 MB)이 부족하다. 대신 AMT+HGMT를 segmentable PMT로 변환하고 SSD LPN 기준으로 slice한 뒤 write request에 piggyback하여, 데이터를 flash에 쓸 때 OOB 영역에 **같은 program operation으로**(추가 write 없이) 영속화한다. chunk당 72-bit spare OOB만 사용. (p.7-8, §5.4)

> [!question]- Q5. SLC evictor가 write amplification을 줄이는 원리는?
> > replication phase의 backup copy는 conversion 직후 invalidate되는 transient 데이터다. write request의 한 bit로 transient tag를 표시하고, SLC buffer가 가득 차면 `score = factor*num_invalid − num_transient`로 evict 우선순위를 정한다. transient page가 많은 block은 score가 낮아 늦게 evict → vulnerable한 MLC로의 불필요한 flush를 피한다. (p.8, §5.5)

> [!question]- Q6. ScalaAFA의 확장성이 mdraid보다 우수한 핵심 이유는?
> > mdraid는 multi-thread 접근 시 lock 오버헤드가 storage stack 시간을 지배해 8 thread에서 포화한다. ScalaAFA는 lock-free user-space 설계 + run-to-complete + conversion 오프로딩으로 host CPU 부담이 작아, **1 thread만으로 11.4 GB/s**(4+1 이상치 근접)를 달성한다. (p.10-11, §6.2)

> [!question]- Q7. 각 핵심 기법이 기여하는 정량적 이득은?
> > lock-free user-space 설계: throughput +58.4%, 평균 latency −45.2%. conversion offloading: throughput +34~39.6%, 평균 latency −36.1%. SLC evictor(write amp 완화): MLC write를 워크로드별 12.2~38.6% 감소. (p.11-12, §6.5)

> [!question]- Q8. ScalaAFA가 실무 적용에 갖는 가장 큰 제약은?
> > SSD 펌웨어 수정(conversion 모듈, SLC evictor, NVMe command 확장)이 필수라서 off-the-shelf SSD에는 적용할 수 없고 SSD 벤더의 협력이 필요하다. 저자도 이를 main obstacle / trade-off로 명시한다. (p.12, §7)

## 🔗 Connections
[[Infra]] · [[ATC]] · [[2024]]

## References worth following
- **FusionRAID** (Jiang et al., FAST '21, [36]): two-phase write AFA의 SOTA, ScalaAFA의 주요 baseline이자 비교 기준.
- **ScalaRAID** (Yi et al., HotStorage '22, [71]) / **stRAID** (Wang et al., ATC '22, [63]): fine-grained lock·run-to-complete로 multi-thread 오버헤드를 줄인 stripe write AFA, lock-free 설계의 출발점.
- **SPDK** (Yang et al., CloudCom '17, [70]): ScalaAFA가 올라탄 user-space lock-free 스토리지 프레임워크.
- **EAR** (Li et al., DSN '15, [50]): flow graph matching으로 conversion phase의 read/write를 줄이려는 접근, conversion offloading과 대비.
- **FEMU** (Li et al., FAST '18, [47]): 평가에 사용된 QEMU 기반 SSD emulator.
- **HolisticGNN / Cognitive SSD / Willow** ([43,53,56]): in-storage processing 선행 연구, SSD 내부 자원 활용 아이디어의 계보.

## Personal annotations
<본인 메모 영역>
