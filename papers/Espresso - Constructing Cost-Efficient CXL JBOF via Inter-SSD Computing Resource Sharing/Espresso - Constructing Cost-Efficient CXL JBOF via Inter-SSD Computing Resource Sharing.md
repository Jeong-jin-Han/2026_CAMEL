---
title: "Espresso: Constructing Cost-Efficient CXL JBOF via Inter-SSD Computing Resource Sharing"
aliases: [Espresso]
description: "SSD를 compute-end/data-end로 disaggregate하고 CXL fabric으로 idle SSD의 processor·DRAM을 busy SSD가 빌려 쓰게 하여, 컴퓨팅 자원을 절반만 갖추고도 JBOF 성능을 유지하는 cost-efficient 설계"
venue: OSDI
year: 2026
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/cxl
  - topic/cxl
  - topic/jbof
  - topic/resource-harvesting
  - topic/ssd-architecture
  - venue/osdi
  - year/2026
  - list/26s-v2
---

# Espresso: Constructing Cost-Efficient CXL JBOF via Inter-SSD Computing Resource Sharing
> **OSDI 2026** · cluster/cxl · Source: [Espresso - Constructing Cost-Efficient CXL JBOF via Inter-SSD Computing Resource Sharing.pdf](<Espresso - Constructing Cost-Efficient CXL JBOF via Inter-SSD Computing Resource Sharing.pdf>)

저자: Shushu Yi, Yuda An, Li Peng, Xiurui Pan (Peking University), Qiao Li (Mohamed bin Zayed University of Artificial Intelligence), Jieming Yin (Nanjing University of Posts and Telecommunications), Guangyan Zhang (Tsinghua University), Wenfei Wu (Peking University), Chenxi Wang (University of Chinese Academy of Sciences), Diyu Zhou (Peking University), Zhenlin Wang (Michigan Tech), Xiaolin Wang, Yingwei Luo (Peking University), Ke Zhou (Huazhong University of Science and Technology, HUST), Jie Zhang (Peking University, corresponding author)

## TL;DR
JBOF(Just a Bunch Of Flash) 내 SSD들은 firmware 처리를 위해 ARM processor와 대용량 DRAM을 각자 내장하지만, I/O burst가 산발적이라 실사용률이 낮아 BOM(bill of material) 비용만 키운다 (p.1). Espresso는 SSD 내부를 compute-end(processor·DRAM)와 data-end(flash·DMA)로 disaggregate하고, cache-coherent CXL fabric을 통해 idle SSD(lender)의 processor·DRAM을 busy SSD(borrower)가 직접 빌려 쓰게 하는 decentralized resource-sharing 메커니즘을 제안한다. 이때 데이터는 옮기지 않고 오직 stateless한 컴퓨팅 자원(주소 변환용 mapping table 접근)만 빌려주므로 기존 storage virtualization의 write copyback overhead가 사라진다 (p.1, p.4). 그 결과 SSD당 컴퓨팅 자원을 절반만 갖추고도 SSD resource utilization을 50.4% 개선하고 BOM cost를 19.0% 절감하면서 성능 저하는 무시할 만한 수준(negligible)이다 (p.1).

## 문제 & 동기
Enterprise SSD는 firmware 처리(주소 변환, garbage collection 등)를 위해 ARM processor와 (1 GB/TB flash 기준) 대용량 온보드 DRAM을 갖추는데, 이 자원들이 SSD controller와 DRAM 원가의 상당 부분(23.2%, 31.8%)을 차지한다(p.2, Fig.3b). 반면 실제 클라우드 환경에서 SSD는 tenant별로 할당되어 I/O burst 시점이 서로 달라 극심하게 underutilize된다: Tencent 스토리지 서버(25 drive)의 94.6% uptime 구간에서 최소 20개 drive의 bandwidth 이용률이 75% 미만이며(p.2, Fig.1a; §2.2), Alibaba·Tencent·Fujitsu 클러스터의 평균 drive bandwidth 이용률은 각각 8.0%, 27.8%, 15.3%에 불과하다(p.2, Fig.3d).

이를 해결하려는 기존 storage virtualization/harvesting 접근(호스트 hypervisor가 idle SSD를 busy SSD와 묶어 virtual SSD로 만드는 방식, p.2 Fig.1b)은 세 가지 근본 한계를 갖는다(§3.1, p.2-3): (1) SSD를 monolithic black box로 취급해 resource stranding이 발생 — 예를 들어 4KB 순차 읽기는 processor clock의 96%를 쓰지만 flash clock은 39%만 사용하고, 반대로 4KB 순차 쓰기는 flash를 99% 소모하면서 processor는 27%만 써서(p.4, Fig.4b) 어느 한쪽이 busy하면 SSD 전체가 busy로 간주되어 다른 자원도 빌려줄 수 없다; (2) read-dominated 워크로드에서 이익이 미미하다 — 읽기 대상 데이터가 lender의 flash backbone에 없어서 lender가 read 요청을 도울 수 없고, 실측상 Tencent·Alibaba 트레이스에서 처리량 개선이 각각 0.5%, 0.8%에 그친다(p.3); (3) write 요청을 lender로 리다이렉트하면 burst 종료 후 데이터를 borrower로 copyback해야 하며, 이로 인해 Tencent 트레이스 기준 0.29회/day 추가 쓰기(DWDP)가 발생해 1 DWDP 내구성 SSD의 수명이 22.5% 단축되고, 중앙집중식 virtual SSD 관리가 host CPU 병목이 되어 SuperMicro SSG-229J 구성에서 21.4%의 처리량 손실을 야기한다(p.3).

> [!quote]- 📄 원문 표현 (paper)
> - "Enterprise SSDs integrate substantial computing resources (e.g., ARM processor and onboard DRAM) to handle I/O bursts. However, these resources significantly raise SSD monetary cost and suffer severely underutilized in JBOF deployments due to the sporadic nature of I/O bursts." (p.1, Abstract)
> - "we identify that in any uptime of a Tencent storage server equipped with 25 drives [134], the probability of at least 20 drives being underutilized (i.e., under 75% bandwidth utilization) is 94.6%" (p.2)
> - "4 KB sequential reads consume 96% of the processor clocks while merely utilizing 39% of flash times. 4 KB sequential writes, in contrast, are flash-hungry (99%) while leaving the processor underutilized (27%..." (p.3)

## 핵심 통찰 (Key Insight)
**1. SSD 내부 자원의 기능별 disaggregation.** 기존 SSD는 processor·DRAM·flash·DMA가 하나의 black box로 묶여 있어 하나가 busy하면 전체가 busy로 취급된다. Espresso는 SSD 내부 하드웨어를 기능에 따라 compute-end(processor, DRAM, firmware 실행)와 data-end(flash, DMA engine, 데이터 전송)로 분리하여 각각을 독립적으로 host와 peer SSD에 노출한다(p.4, §4.2). 이렇게 하면 어느 한쪽이 busy해도 다른 쪽은 여전히 공유 가능해 resource stranding을 근본적으로 해소한다.

> [!quote]- 📄 원문 표현 (paper)
> - "Espresso first disaggregates SSD architecture into functionally distinct components, enabling fine-grained SSD internal resource management." (p.1)
> - "Conventional SSDs are black boxes in which the onboard computing and flash resources are tightly coupled and invisible to external systems. This agnostic causes resource stranding issues" (p.4)

**2. CXL cache-coherent fabric을 이용한 stateless 자원 대여(데이터 이동 없이).** 기존 virtualization은 write 데이터 자체를 lender로 옮기므로(copyback 필요) 수명 단축과 오버헤드가 크다. Espresso의 핵심은, lender의 processor가 CXL.mem을 통해 borrower의 온보드 DRAM에 있는 metadata(FTL mapping table)를 직접 read/write하여 command parsing·address translation을 대행해주되, 실제 flash I/O(데이터 경로)는 항상 borrower 자신의 data-end를 통해 수행된다는 점이다. 즉 오직 stateless한 컴퓨팅 자원만 빌리고 데이터 자체는 절대 옮기지 않으므로 copyback overhead가 원천적으로 없다(p.4, §4.1).

> [!quote]- 📄 원문 표현 (paper)
> - "the lender's processor can help the borrower handle I/O requests (e.g., command parsing and address translation) by directly operating the borrower's metadata stored in its onboard DRAM through CXL fabric... This method benefits both reads and writes and avoids data copyback overhead... as it only harvests the stateless computing resource to accelerate metadata processing, without redirecting data." (p.4)
> - "The borrower then executes these operations to transfer data directly between the host and its flash backbone, without passing through the lender." (p.4, Fig.5 caption 설명)

**3. Decentralized self-governing resource management.** 중앙집중식 hypervisor 관리는 host CPU 병목(21.4% 손실)을 유발했다. Espresso는 각 SSD가 자신의 idle 자원을 idle resource descriptor라는 공유 자료구조에 기록하고, 자원이 부족한 SSD가 이를 reader-writer lock으로 스캔해 best-fit lender를 스스로 찾아 빌리는 완전 분산 구조를 채택해 host CPU 개입을 최소화한다(p.5-6, §4.3).

> [!quote]- 📄 원문 표현 (paper)
> - "Espresso then implements decentralized and self-governing resource management in SSDs to relieve host CPU burdens." (p.1)

## 설계 / 메커니즘 (Design)
**Disaggregated SSD architecture (§4.2, Fig.6, p.6).** 각 Espresso SSD는 compute-end(weaker processor + smaller DRAM + firmware: cmd. parse, addr. trans., GC, WL)와 data-end(data buffer, DMA engine, flash controller/backbone)로 나뉘며, 둘 다 Type-2 CXL controller[39]를 통해 CXL 동작을 수행한다. Compute-end에는 firmware로 동작하는 **Espresso daemon**이 있으며, resource monitor(processor 유틸 PMU 10ms 폴링, flash 채널 busy-clock 평균)·resource manager(자원 대여/차입 결정)·data-end agent(lender의 compute-end와 borrower의 data-end를 잇는 메시지 큐 브리지) 세 부분으로 구성된다(p.6). 시스템 초기화 시 각 SSD는 자신의 로컬 DRAM을 CXL fabric manager에 global fabric-attached memory(G-FAM)로 등록하며, peer SSD는 load/store(aarch64 LDR/STR)로 이를 CXL MemRd/MemWr 요청으로 접근하고 BISnp/BIRsp(HDM-DB 모드)로 coherence를 유지한다(p.6).

**Decentralized resource management (§4.3, Fig.7, Table 1, p.6-7).** 각 SSD는 idle resource table(다수의 idle resource descriptor로 구성, reader-writer lock으로 동기화)을 유지한다. Descriptor는 valid bit, type(processor/DRAM), borrower ID, 유틸리제이션, lender 주소/디렉토리 정보 등을 담는다. Table 1은 (processor, data-end) busy 상태 조합에 따른 4가지 트리거 조건을 정의한다: 둘 다 busy면 아무것도 안 함(mixed burst), processor만 underutilized면 lend out(write burst), 둘 다 underutilized면 lend out(no I/O), processor만 busy면 borrow in(read burst). 동기화 주기는 기본 10ms로 설정한다(p.7).

**Transparent processor harvesting (§4.4, Fig.8, p.7-8).** Borrower는 초기화 시 NVMe I/O queue pair 일부를 shadow QP로 예약해둔다. Lending 시 lender는 자신의 shadow QP의 CQID를 idle resource descriptor에 기록하고, borrower는 이를 borrower QP로 지정해 idle resource descriptor에 자신의 ID를 기록한다. Host NVMe driver는 borrower QP를 shadow QP에 바인딩하여 일부 NVMe I/O command를 shadow SQ로 리다이렉트하고, lender가 이를 fetch해 borrower의 mapping table을 조작·처리한 뒤 결과를 shadow CQ에 기록한다. Host driver는 both CQ의 완료를 상위 소프트웨어에 commit한다. 리다이렉션 비율은 holistic load balance 공식으로 결정된다(p.7-8):

$$\frac{N_{borrow}}{N_{lend}} = \frac{U_{lend}}{U_{borrow}} \times \frac{\sum_{lend} W}{W_{shadowSQ}} \times \frac{W_{borrowSQ}}{\sum_{borrow} W}$$

여기서 $N_{borrow}$, $N_{lend}$는 borrower·lender로 보내는 I/O command 수, $U$는 processor 유틸리제이션, $W$는 NVMe WRR(weighted round-robin) 큐 weight이다. Host는 10ms마다 idle resource descriptor를 읽어 이 비율에 따라 명령을 분배한다(p.8). 하우스키핑(GC, bad block management)은 자주 일어나지 않으므로 항상 borrower 자신이 처리한다(p.6).

**Persistent DRAM harvesting (§4.5, p.8-9).** DRAM은 2MB 단위 segment로 관리되며 mapping table 캐싱에는 LRU를 사용한다. SHARDS[117] 알고리즘으로 miss ratio curve(MRC)를 온라인 예측해, cache pollution 우려가 없는(즉 예측상 근시일 내 재접근 안 될) segment만 lend하고, borrower는 miss ratio를 목표 threshold(예: 10%) 이하로 낮추는 데 필요한 만큼만 DRAM을 빌린다. Crash consistency를 위해 harvesting 시작 시 borrower는 각 harvested segment마다 4KB log page를 로컬에 확보해두고, offsite metadata가 수정될 때마다 redo log를 이 log page에 커밋한다(cacheline flush 지시로 lender→borrower log 반영 보장). Log page가 차면 해당 segment가 borrower의 flash backbone으로 flush된다. Lender가 (다중 I/O timeout 등으로) 실패하면 host NVMe driver가 borrower에게 log page를 replay하도록 알리고 in-flight 명령을 shadow SQ에서 borrower QP로 재제출한다(p.8-9).

**Implementation (§4.6, p.9).** Host-side(I/O redirection, load balance)는 Linux kernel v5.15 NVMe driver에 구현. Firmware-side는 CXL 3.0 하드웨어 부재로 DaisyPlus OpenSSD board(quad-core ARM Cortex-A53, 2GB DRAM, FPGA)로 프로토타이핑했고, data-end agent의 dequeue+unwrap 평균 지연은 114.2ns, redo log commit은 321.9ns이다. 성능 모델은 SimpleSSD 기반 simulator(Xerxes[6] cycle-accurate CXL fabric 시뮬레이터 통합, McPAT/DRAMPower로 에너지 측정)로 cross-validate했다(p.9). Cache coherence는 directory-based 방식(기본 1K directory entries로 SSD당 64KB 캐시 추적)을 사용해 CXL 표준이 강제하지 않는 부분을 자체 구현했다(p.5, §4.6).

> [!quote]- 📄 원문 표현 (paper)
> - "Espresso replaces conventional PCIe interconnections with CXL to enjoy its high performance and cache coherence; ... Espresso breaks the black-box constraint of traditional SSDs and enables fine-grained management of SSD internal resources" (p.5)
> - "SHARDS [117], a lightweight and efficient algorithm, to predict MRC online. Based on the predicted MRC, SSD can lend out the spare DRAM segments, which has no help on a lower miss ratio (i.e., the cached mapping table will not be accessed in the near future), minimizing the effect of cache pollution from the borrower." (p.8)
> - "Espresso employs a directory-based approach [1], in which the hardware overhead primarily depends on the number of cachelines that need to be tracked simultaneously." (p.9)

## 평가 (Evaluation)
시뮬레이션 대상은 SuperMicro SSG-229J-5BU24JBF 구성(DPU당 최대 12 SSD, 16-core 2.1GHz ARM + 16GB DDR5-5600) 기반이며, 비교 대상은 Conv(자원 풍족한 conventional JBOF), OC(OCSSD 기반, 최소 자원+host가 firmware/cache 수행), Shrunk(Conv 대비 컴퓨팅 자원 절반, 3-core/0.5GB per TB flash), VH(단순 virtualization+harvesting), VH(ideal)(copyback 불필요 가정), ProcH(Shrunk+processor harvesting만), Espresso(Shrunk와 동일한 절반 자원)이다(Table 2, p.10, §5.1). 워크로드는 Tencent, Alibaba, Fuji, src, DAP, MSNFS, mds, DB 등 실제 production 트레이스(Table 3, p.10)이다.

- **Processor harvesting (micro-benchmark, §5.2, Fig.10, p.11):** OC와 Shrunk는 Conv 대비 평균 27.8%, 29.2% throughput 손실(latency는 44.1%, 46.4% 증가). VH·VH(ideal)도 read 워크로드에선 별 도움 안 됨. Espresso는 Conv 대비 comparable 성능을 절반 자원으로 달성하며, 256KB 순차 읽기에서 borrower·lender 평균 processor utilization이 Shrunk보다 **50.4%** 높다(p.11, Fig.10c).
- **DRAM harvesting (Fig.11, p.11):** 충분한 DRAM이 없으면 OC/Shrunk/ProcH는 각각 66.2%, 49.7%, 49.7%의 miss ratio를 겪는 반면, Espresso는 DRAM harvesting으로 Conv에 준하는 latency를 달성한다.
- **실제 워크로드 처리량 (Fig.12, p.11):** Conv 대비 OC/Shrunk는 평균 16.2%, 13.4% throughput 손실. VH(ideal)은 write-dominated 워크로드(src)에서 copyback 리다이렉션 덕에 15.5% 이득을 보이나 실제 VH는 copyback overhead로 Conv보다 14.0% 뒤처진다. **Espresso는 Shrunk·VH 대비 각각 19.2%, 20.0% 개선**하고 일부 read-dominated 워크로드(Ali-0)에서는 Conv 대비 **2.0% 더 높은** 처리량을 보인다(CXL lane이 NVMe command fetching에 2배로 쓰이는 부수 효과, p.11).
- **BOM cost (§5.3 관련, Fig.13, p.12):** Espresso는 2TB SSD 기준 Conv 대비 BOM cost **19.0%** 절감. cost efficiency(IOPS/$)는 OC보다 Ali-0 워크로드에서 **19.7%** 우수. CXL 비용 프리미엄이 40%를 넘지 않는 한 Espresso가 OC를 능가한다(p.12, Fig.13b).
- **Overhead analysis (§5.3, Fig.15, p.12):** lender에게 자원을 빌려주는 것 자체의 성능 영향은 평균 **1.3%**로 미미(Fig.14a). borrower의 throughput은 lender의 I/O depth가 낮을수록(32→16→1) 15.5%, 23.3%, 30.0% 개선(Fig.14b). Fuji-0 워크로드 에너지 소모는 Espresso가 Conv 대비 3.5% 더 높음(Espresso daemon·CXL 통신 오버헤드, Fig.15b). Inter-SSD 통신 지연은 최대 2.9%.
- **Sensitivity study (§5.4, Fig.16, Fig.17, p.13):** processor 코어 수를 1~3으로 줄인 Shrunk는 1-core 설정에서 Ali-0 처리량이 최대 **54.6%** 저하되지만, Espresso는 2-core에서 borrower:lender=1:2일 때 Conv 성능의 **97.7%**를 달성. DRAM 0.25/0.5/0.75 GB per TB 조건에서 Shrunk는 각각 44.0%, 22.3%, 10.0% 더 높은 latency를 겪지만 Espresso는 평균 3.4% 증가에 그친다.
- **Complex scenario (§5.5, Fig.18, p.13):** 12개 SSD가 각기 다른 Tencent 워크로드를 무작위 실행하는 시나리오(120회 반복)에서 Espresso는 peak throughput **12.3 GB/s**(Shrunk는 8.1 GB/s), workload completion time을 Shrunk 대비 최대 **34.3%** 단축.
- **NUMA platform 검증 (§5.6, Fig.19, p.13):** 2-socket NUMA 에뮬레이션(Intel Xeon 8562Y+, NVMeVirt)에서 Ext4+filebench, RocksDB+db_bench 실행 시 Espresso가 Shrunk 대비 **24.8%** 우수, 절반 자원으로도 Conv에 comparable한 처리량 달성.

> [!quote]- 📄 원문 표현 (paper)
> - "The evaluation results show that Espresso can improve SSD resource utilization by 50.4% and reduce monetary cost by 19.0%, with negligible performance degradation, compared to the state-of-the-art JBOF designs." (p.1, Abstract)
> - "Resource lending causes negligible performance loss (1.3% on average, cf. Figure 14a) for lenders." (p.12)
> - "Espresso succeeds in fulfilling the burst I/O performance demands, even with only halved computing resources. To be specific, SSDs in Espresso achieve 12.3 GB/s peak throughput, while this value is only 8.1 GB/s in Shrunk." (p.13)

## 섹션 노트
- **§1 Introduction**: cost-utilization dilemma 제시 — BOM 상승과 severe underutilization의 모순. black-box SSD의 resource stranding을 핵심 원인으로 지목하고 세 가지 기여를 요약(p.1-2).
- **§2 Background**: JBOF/NVMe SSD 아키텍처(compute controller, I/O path 11단계)와 cost-utilization dilemma 정량 분석(computing power 추세, BOM breakdown, drive utilization 실측)(p.2-3).
- **§3 Preliminary Study**: HMSSD 기반 단순 harvesting 해법의 세 가지 challenge(resource stranding, 낮은 read 이득, copyback overhead)를 실측으로 규명하고, 해법으로 CXL의 cache-coherent 특성을 제시(p.3-4).
- **§4 Design and Implementation**: overview→disaggregated architecture→decentralized resource management→processor harvesting→DRAM harvesting→implementation 순으로 6개 하위 절 구성(p.4-9).
- **§5 Evaluation**: micro-benchmark(processor/DRAM harvesting), real workload throughput, BOM cost, overhead(latency/energy breakdown), sensitivity, complex scenario, NUMA 실증까지 6개 하위 절(p.10-13).
- **§6 Related Work and Discussion**: SSD architecture 개조 연구(Decoupled SSD, XHarvest), NVMe/load-store 통신 프로토콜, storage virtualization(BlockFlex, FleetIO), RAID, heterogeneous SSD 확장 가능성, future CXL(부분 coherence 트렌드)과의 정합성 논의(p.13-14).
- **§7 Conclusion**: cost-utilization dilemma 재확인과 50.4% utilization 개선·19.0% BOM 절감 요약(p.14).

## 핵심 용어 (Key terms)
- **JBOF (Just a Bunch Of Flash)**: 다수의 고성능 SSD를 CPU/DRAM/NIC(또는 DPU)로 묶어 scale-out 스토리지 서버를 구성하는 아키텍처 (p.1-2).
- **Cost-utilization dilemma**: SSD 성능을 위한 풍부한 컴퓨팅 자원이 BOM 비용을 올리지만, I/O burst의 산발성으로 인해 실제 utilization은 낮은 모순 (p.1-2).
- **Resource stranding**: SSD를 monolithic black box로 취급해, 한 자원(예: flash)이 busy하면 다른 자원(processor)도 빌려줄 수 없게 되는 현상 (p.3-4).
- **Compute-end / Data-end**: Espresso가 SSD를 기능별로 나눈 두 부분 — compute-end(processor·DRAM·firmware), data-end(flash·DMA·데이터 전송) (p.4, p.6).
- **Borrower / Lender**: I/O burst로 자원이 부족한 SSD(borrower)와 idle 자원을 빌려주는 SSD(lender) (p.5).
- **Idle resource descriptor / table**: 각 SSD가 자신의 idle processor/DRAM 정보를 기록해 peer들에게 노출하는 CXL 공유 자료구조 (p.6-7, Fig.7).
- **Shadow QP / Borrower QP**: I/O redirection을 위해 lender가 예약하는 shadow queue pair와 borrower가 지정하는 borrower QP (p.7-8, Fig.8).
- **Holistic load balance**: processor utilization과 NVMe WRR 가중치를 함께 고려해 borrower/lender로의 명령 분배 비율을 정하는 공식 (p.8).
- **MRC (Miss Ratio Curve) / SHARDS**: DRAM 캐시 크기별 miss ratio를 예측하는 곡선과, 이를 경량으로 온라인 추정하는 샘플링 알고리즘 (p.8).
- **G-FAM (global fabric-attached memory)**: 각 SSD가 CXL fabric manager에 등록하는, peer가 load/store로 접근 가능한 로컬 DRAM 영역 (p.6).
- **Data-end agent**: lender의 compute-end 요청(DMA/flash operation)을 borrower의 message queue로 전달·언랩해 borrower의 실제 data-end에 실행시키는 브리지 컴포넌트 (p.6, p.7).

## 강점 · 한계 · 열린 질문
**강점**
- 문제 진단이 정량적이고 계층적이다: cost-utilization dilemma → resource stranding/read 이득 부족/copyback overhead라는 세 가지 구체적 challenge로 세분화하고 각각을 preliminary study(§3.1)로 실측 입증했다(p.2-4).
- 데이터를 옮기지 않고 오직 stateless computing(주소 변환용 metadata access)만 공유한다는 설계 원칙이 명확해, storage virtualization의 고질적 문제(copyback, write amplification, endurance 저하)를 구조적으로 회피한다(p.4).
- 시뮬레이터(SimpleSSD+Xerxes) 검증에 그치지 않고 실제 CXL 3.0 하드웨어 부재를 감안해 DaisyPlus FPGA 프로토타입으로 firmware-side 지연을 실측하고, 2-socket NUMA 에뮬레이션 플랫폼(NVMeVirt)에서 Ext4/RocksDB application-level 검증까지 수행해 evaluation 다각화가 돋보인다(§4.6, §5.6, p.9, p.13).
- decentralized self-governing 관리로 중앙집중식 hypervisor 방식의 host CPU 병목(기존 연구의 21.4% 손실, p.3)을 원천 회피하는 설계 선택이 명확한 동기를 갖는다.

**한계 / 열린 질문**
- CXL 3.0 하드웨어가 아직 상용화되지 않아(p.9) 실 프로토타입은 quad-core Cortex-A53 DaisyPlus board + FPGA로 firmware-side만 검증했고, 전체 시스템 성능 수치는 시뮬레이터 기반이다. CXL 3.0 실제 스위치/코히런시 지연이 모델과 다를 가능성은 남아있다.
- 동기화 주기를 10ms로 고정한 설계는 저자들도 인정하듯 lender가 burst로 갑자기 자원 회수 시 borrower/host가 제때 반영하지 못하면 일시적 resource contention이 발생할 수 있다(trade-off, p.7).
- 이질적(heterogeneous) SSD firmware·제조사 간 적용은 §6에서 향후 방향으로만 논의되며(예: TEE 기반 non-decompilable firmware 노출), 실제 멀티벤더 JBOF에서의 실증은 없다(p.14).
- Crash consistency를 위한 log 방식은 lender가 완전히 실패(unplug 등)할 때 borrower의 offsite metadata 복구를 log replay에 의존하는데, 다중 lender 동시 실패나 log page 자체의 손상 시나리오에 대한 분석은 제한적이다(p.9).
- CXL cache coherence region이 SSD당 in-flight request 수에 비례한 directory entry(기본 1K entries/64KB 캐시)로 제한되는데, 매우 높은 concurrency 워크로드에서 이 directory 용량이 병목이 될 가능성에 대한 심층 분석은 없다(p.9).

## ❓ Q&A (자가 점검)
> [!question]- Q1. Cost-utilization dilemma란 무엇이며 왜 발생하는가?
> SSD 제조사가 burst I/O 성능을 위해 ARM processor·대용량 DRAM 등 컴퓨팅 자원을 늘릴수록 BOM 비용이 오르지만, 클라우드에서 SSD가 tenant별로 할당되어 I/O burst 시점이 제각각이라 실 utilization은 매우 낮다(예: Tencent 서버의 94.6% uptime에서 20개 이상 drive가 75% 미만 이용률). 늘어난 자원이 대부분 놀고 있는 모순이다 (p.1-2).

> [!question]- Q2. 기존 storage virtualization/harvesting 방식이 겪는 세 가지 challenge는?
> (1) SSD를 black box로 취급해 한 자원(예: flash)이 busy하면 다른 자원(processor)도 빌려줄 수 없는 resource stranding, (2) 읽기 대상 데이터가 lender의 flash에 없어 read-dominated 워크로드에서 이득이 미미(0.5~0.8% throughput 개선), (3) write redirect 후 burst 종료 시 데이터를 borrower로 copyback해야 해 DWDP 증가(0.29회/day)·수명 단축(22.5%)·host CPU 병목(21.4% 손실)이 발생 (p.2-3).

> [!question]- Q3. Espresso가 compute-end/data-end를 분리하는 이유는?
> 기존 SSD는 processor·DRAM·flash·DMA가 하나로 묶여 있어 하나가 busy하면 SSD 전체가 busy로 간주돼 다른 자원까지 stranded된다. 기능별로 분리해 각각을 CXL을 통해 독립적으로 host·peer에 노출하면 fine-grained 자원 관리와 harvesting이 가능해진다 (p.4, p.6).

> [!question]- Q4. Espresso가 기존 harvesting과 달리 copyback overhead를 없앨 수 있는 이유는?
> 데이터 자체를 옮기지 않고, lender의 processor가 CXL cache-coherent 접근으로 borrower의 온보드 DRAM에 있는 metadata(FTL mapping table)만 직접 조작해 command parsing·address translation을 대행하기 때문이다. 실제 flash I/O는 항상 borrower 자신의 data-end로 수행되어 stateful한 데이터는 이동하지 않는다 (p.4).

> [!question]- Q5. Transparent I/O redirection은 구체적으로 어떻게 동작하는가?
> Borrower는 초기화 시 shadow QP를 예약하고, lending 시 idle resource descriptor에 lender의 shadow QP CQID와 borrower QP ID를 기록한다. Host NVMe driver가 borrower QP를 shadow QP에 바인딩해 일부 NVMe command를 shadow SQ로 리다이렉트하면 lender가 이를 fetch해 borrower의 metadata를 처리하고, 완료 결과는 양쪽 CQ에서 상위 소프트웨어로 커밋된다 (p.7-8, Fig.8).

> [!question]- Q6. Holistic load balance 공식에서 각 항의 의미는?
> $N_{borrow}/N_{lend} = (U_{lend}/U_{borrow}) \times (\sum_{lend}W / W_{shadowSQ}) \times (W_{borrowSQ}/\sum_{borrow}W)$ 로, processor 유틸리제이션 비율과 NVMe WRR 큐 weight 비율을 함께 반영해 borrower/lender로 보낼 I/O command 비율을 결정한다. 이를 통해 lender에 minimal 성능 영향을 주면서 borrower를 돕는 정도를 조절한다 (p.8).

> [!question]- Q7. DRAM harvesting에서 MRC/SHARDS는 어떤 역할을 하는가?
> SHARDS로 mapping table 캐시의 miss ratio curve를 경량으로 온라인 예측해, lender는 미래에 재접근되지 않을(캐시 오염 우려 없는) DRAM segment만 빌려주고, borrower는 목표 miss ratio(예: 10%) 이하로 낮추는 데 필요한 최소한의 DRAM만 빌린다 (p.8).

> [!question]- Q8. Espresso의 핵심 정량 성과와 Shrunk 대비 개선 폭은?
> Conv 대비 SSD당 자원을 절반만 쓰면서 resource utilization을 50.4% 개선, BOM cost를 19.0% 절감했다. 실제 워크로드에서 Shrunk·VH 대비 각각 19.2%, 20.0% throughput 개선을 보였고, 자원을 빌려주는 lender의 성능 손실은 평균 1.3%에 불과하다 (p.1, p.11-12).

## 🔗 Connections
[[CXL]] · [[OSDI]] · [[2026]]
관련: [[XHarvest - Rethinking High-Performance and Cost-Efficient SSD Architecture with CXL-Driven Harvesting]], [[SkyByte - Architecting An Efficient Memory-Semantic CXL-based SSD with OS and Hardware Co-design]]

## References worth following
- **XHarvest: Rethinking high-performance and cost-efficient SSD architecture with CXL-driven harvesting** (L. Peng et al., ISCA 2025) [ref 83] — CXL-driven secure host resource harvesting을 통한 SSD 재설계로 Espresso와 방향은 반대(host가 SSD를 도움)지만 목표(비용-효율 SSD)는 동일한 직접 비교 대상 (p.13-14, 본 vault에 deep 노트 존재).
- **BlockFlex: Enabling storage harvesting with software-defined flash in modern cloud platforms** (B. Reidys et al., OSDI 22) [ref 86] — Espresso가 여러 차례 비교하는 기존 storage virtualization/harvesting의 대표 선행 연구, limited read profit과 copyback overhead 문제의 근거(p.2-3, p.14).
- **FleetIO: Managing multi-tenant cloud storage with multi-agent reinforcement learning** (J. Sun et al., ASPLOS 25) [ref 107] — hypervisor 기반 virtual SSD harvesting의 또 다른 대표 사례로 Espresso의 storage virtualization 관련 연구 비교 대상 (p.3, p.14).
- **Decoupled SSD: Rethinking SSD architecture through network-based flash controllers** (J. Kim et al., ISCA 23) [ref 47] — SSD를 front-end/back-end로 분리하는 선행 아키텍처로 Espresso의 compute-end/data-end disaggregation과 비교되는 SSD architecture 재설계 계열 연구 (p.13).
- **SHARDS: Efficient MRC construction with shards** (C. Waldspurger et al., FAST 15) [ref 117] — Espresso의 DRAM harvesting에서 miss ratio curve를 경량 예측하는 데 직접 사용하는 알고리즘 (p.8).
- **Demystifying CXL memory with genuine CXL-ready systems and devices** (Y. Sun et al., MICRO 23) [ref 109] — CXL fabric의 실제 하드웨어 특성 분석으로 Espresso의 CXL 3.0 기반 설계 가정의 근거 자료.

## Personal annotations
<!-- 본인 메모 영역 -->
</content>
