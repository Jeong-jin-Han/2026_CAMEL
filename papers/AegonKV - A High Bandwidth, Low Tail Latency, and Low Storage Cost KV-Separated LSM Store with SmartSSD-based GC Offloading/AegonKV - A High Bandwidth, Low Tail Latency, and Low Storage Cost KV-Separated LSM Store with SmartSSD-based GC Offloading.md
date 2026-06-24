---
title: "AegonKV: A High Bandwidth, Low Tail Latency, and Low Storage Cost KV-Separated LSM Store with SmartSSD-based GC Offloading"
aliases: [AegonKV]
description: "SmartSSD(FPGA+SSD) 기반 GC offloading으로 KV-separated LSM의 throughput, tail latency, storage cost 세 가지를 동시에 개선한 KV store"
venue: FAST
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/kvlsm
  - topic/lsm
  - topic/kv-separation
  - topic/smartssd
  - topic/garbage-collection
  - venue/fast
  - year/2025
---

# AegonKV: A High Bandwidth, Low Tail Latency, and Low Storage Cost KV-Separated LSM Store with SmartSSD-based GC Offloading

> **FAST 2025** · cluster/kvlsm · Source: [AegonKV - A High Bandwidth, Low Tail Latency, and Low Storage Cost KV-Separated LSM Store with SmartSSD-based GC Offloading.pdf](<AegonKV - A High Bandwidth, Low Tail Latency, and Low Storage Cost KV-Separated LSM Store with SmartSSD-based GC Offloading.pdf>)

저자: Zhuohui Duan, Hao Feng, Haikun Liu (corresponding), Xiaofei Liao, Hai Jin, Bangyu Li — Huazhong University of Science and Technology, China.

## TL;DR

AegonKV는 KV-separated LSM store의 garbage collection(GC)을 SmartSSD(FPGA가 내장된 computational storage)로 offload하여, host의 read/write와 bandwidth/CPU를 두고 경쟁하지 않으면서 **throughput, tail latency, storage cost 세 가지를 동시에(three-birds-one-stone)** 개선한다. 기존 KV-separated 시스템(Titan의 Direct GC, DiffKV/BlobDB의 Compaction-triggered GC)은 이 세 지표 사이의 trade-off에서 벗어나지 못한다는 분석을 바탕으로, AegonKV는 GC Manager(ValidMap 데이터 구조), GC Scheduler(I/O ctl·CU ctl·Meta install), GC-friendly metadata install, FPGA Compute Unit(CU) pipeline을 설계했다. 결과적으로 기존 KV-separated 시스템 대비 throughput **1.28-3.3배** 향상, tail latency **37-66%** 감소, space overhead **15-85%** 감소를 달성했다.

> [!quote]- 📄 원문 표현 (paper)
> - "In this paper, we introduce AegonKV, a "three-birds-one-stone" solution that comprehensively enhances the throughput, tail latency, and space usage of KV separated systems." (p.1)
> - "AegonKV first proposes a SmartSSD-based GC offloading mechanism to enable asynchronous GC operations without competing with LSM read/write for bandwidth or CPU." (p.1)
> - "Experiments demonstrate that AegonKV achieves the largest throughput improvement of 1.28-3.3 times, a significant reduction of 37%-66% in tail latency, and 15%-85% in space overhead compared to existing KV separated systems." (p.1)

## 문제 & 동기

KV separation은 LSM-tree의 write amplification을 크게 줄이는 기법으로, key는 LSM 구조(MemTable/SSTable)에 두고 value는 별도 Value region(vLog/Blob)에 저장하여 compaction이 value를 다루지 않게 한다. 그러나 log-structured로 쌓이는 Value region에는 expired data가 누적되므로 이를 회수하는 GC가 필수다. 문제는 GC가 추가 I/O와 computational overhead를 유발한다는 점이다.

기존 접근은 두 갈래다. (1) **Direct GC**(Titan): Blob file을 직접 스캔하면서 각 offset에서 LSM에 valid 여부를 query하고, valid value를 새 Blob에 재배치한 뒤 새 주소를 LSM에 다시 write(GC metadata). (2) **Compaction-triggered GC**(DiffKV, BlobDB): single-layer Blob을 multi-layer LSM 구조로 바꾸고 compaction 시점에 value migration을 수행. AegonKV는 동기 실험(2x18-core Xeon, Samsung SmartSSD, 20GB dataset/200M inserts, key 24B/value 1KB)으로 두 방식 모두 throughput·tail latency·space의 세 지표를 동시에 만족하지 못함을 보였다. 예를 들어 Compaction-triggered GC는 timeliness를 희생하여 redundant space와 compaction overhead가 폭증하고(BlobDB 155GB, DiffKV 73GB space vs RocksDB 29GB), Direct GC(Titan)는 GC가 index data I/O를 위해 host bandwidth/CPU와 경쟁해 RocksDB 대비 throughput이 18.3% 떨어진다. Titan에서 GC를 끄면 throughput 67.7% 상승·tail latency 69.1% 감소가 나타나 GC가 핵심 bottleneck임이 드러났고, GC time breakdown은 대부분이 LSM index의 read(25.3%)/write(65.93%)에서 발생함을 보였다.

> [!quote]- 📄 원문 표현 (paper)
> - "our analysis indicates that such solution based on trade-offs between CPU and I/O overheads cannot simultaneously satisfy the three requirements of KV separated systems in terms of throughput, tail latency, and space usage." (p.1)
> - "the performance results of existing KV separated systems exhibit trade-offs and do not achieve simultaneous advantages in throughput, tail latency, and space usage." (p.4)
> - "the primary bottleneck in Direct GC arises from index data I/O competing with system read and write requests for compute and bandwidth resources." (p.6)

## 핵심 통찰

핵심 통찰은 GC의 computation과 I/O를 SmartSSD에서 co-optimize하면 세 지표를 동시에 만족할 수 있다는 것이다. SmartSSD는 SSD 안에 FPGA를 통합하고 NVMe SSD-FPGA DRAM 간 P2P(peer-to-peer) 경로를 제공하므로, GC를 offload하면 (1) SSD 내부 bandwidth로 host bandwidth contention을 피하고, (2) PCIe bus를 우회해 data movement를 줄이며, (3) FPGA로 GC pipeline을 구성해 host CPU 압력을 완화할 수 있다. 또한 GC의 value file 처리는 sequentially independent streaming 특성을 가져 stage별(data encoding/decoding, filtering, validation, merging, CRC validation) 병렬 pipeline화에 적합하다.

다만 SmartSSD offloading에는 세 가지 challenge가 있다. (1) **Read Back Travel**: FPGA에서 query validity를 위해 host LSM을 back-query하는 로직 복제가 hardware 제약상 어렵다. (2) **Resource Limitation**: SmartSSD의 FPGA는 자원(Flip-Flop, LUT, BRAM)과 DRAM(4GB)이 제한적이고 offloading unit 과다 배치 시 contention/crash 위험이 있다. (3) **Extra Data Movement**: GC 결과 일부를 hardware에서 host로 다시 전송해야 하므로 channel congestion이 생길 수 있다.

> [!quote]- 📄 원문 표현 (paper)
> - "We find that computation and I/O co-optimization of GC operations is possible on SmartSSD, which provides an opportunity to build KV separated systems with high bandwidth, low tail latency, and low storage overhead." (p.2)
> - "Direct GC operations can be optimized through near-data processing on SmartSSD, overcoming the trade-off dilemma to simultaneously achieve optimal throughput, tail latency, and storage efficiency." (p.6)
> - "Treating value file data as sequentially independent streaming data enables highly parallelized pipeline optimizations." (p.6)

## 설계 / 메커니즘

AegonKV는 최신 Titan 위에 구축되며 host-side memory 구조와 dataflow는 유지하면서 GC computation을 SmartSSD로 옮긴다. 핵심 구성요소는 다음과 같다.

- **GC Manager (host)**: compaction과 GC를 잇는 bridge. compaction에서 정보를 모아 hardware GC용 데이터 구조 **ValidMap**(Bitmap, bit 0=valid/1=invalid)을 준비한다. 각 Blob file당 ValidMap을 두며, 표준 32MB Blob(24B+1KB KV 기준)에 대해 이론적으로 약 4KB만 필요해 매우 작다. compaction 시점의 merge가 invisible expired KV를 걸러내므로, GC Manager가 filtered bit만 1로 세팅한 temporary ValidMap을 만들고 현재 ValidMap과 bit-wise OR하여 update한다.
- **GC Scheduler (host)**: 세 unit으로 구성. *I/O Control Unit*(SmartSSD 내부 data path 활용, SSD-DRAM 간 Blob read/write 및 ValidMap·write-back metadata 전송을 P2P 4KB 단위로 조율, 그래서 Blob file에 4KB boundary padding 추가), *Compute Unit Control Unit*(CU 개수와 FPGA DRAM 메모리 할당을 software에서 관리, CU 시작/종료 및 callback), *Metadata Install Unit*(GC 결과 LSM write-back을 buffer queue로 모아 batch flushing).
- **GC Friendly Metadata Install**: GC write-back 시 각 metadata를 lookup하는 비용을 줄이기 위해 PinK의 delayed update 아이디어를 확장, validation 없이 MemTable에 batch insert한다. 이때 생기는 version conflict(Figure 7)/data consistency 문제는 **deferred validation**으로 해결한다. Get/compaction 시 GC write-back state flag를 두어 더 오래된 version을 한 단계 더 backward query하여 GC 중 새 key가 쓰였는지 비교·판정한다. metadata를 normal data로 promote하여 redundancy를 LSM의 garbage rate로 제어, deferred validation의 overhead를 작게 유지한다.
- **Compute Unit for GC (FPGA)**: CU는 *Input Decode → Data Compute → Output Encode* 세 stage pipeline(Vitis-HLS 구현). 내부 모듈은 Fetcher/Decoder/Filter/Encoder/Buffer/Meta Collector. Filter는 ValidMap으로 stream 형태로 valid/invalid 판정, Fetcher는 valid KV만 새 file로 이동(invalid면 offset만 전진). KV migration은 CRC 결과를 분해/재검증해 무결성을 보장한다. CU 입력은 unlimited stream-type으로 두어 병렬성을 높였다. 자원 제약을 고려해 8개의 CU를 배치(CLB가 limiting factor, utilization CLB 97.6%/LUT 55.9%/Flip Flop 44.6%/BRAM 45.2%).

> [!quote]- 📄 원문 표현 (paper)
> - "In AegonKV, for each Blob file, we create a Bitmap data structure called ValidMap to indicate the validity of data at each offset point (bit "0" and "1" mark valid and invalid, respectively)." (p.7)
> - "Extending the idea of delayed updates in PinK, we further propose to batch insert the metadata directly into the MemTable without validation in order to achieve performance-intrusive GC write-back." (p.8)
> - "we abstract the process into three separate stages: Input Decode, Data Compute, and Output Encode ... we implement the corresponding logic and optimizations using Vitis-HLS development flow." (p.8)
> - "We fully utilize the hardware resources of SmartSSD and deploy 8 CUs, which is enough to support the system to run normally." (p.11)

## 평가

Testbed은 2x18-core Intel Xeon Gold 5220 @2.20GHz(two-way HT), 64GB DDR4-2666, Xilinx UltraScale+ FPGA를 내장한 Samsung SmartSSD(3.84TB SSD), Ubuntu 20.04.4 LTS. Workload은 YCSB(key 24B/value 1KB), db_bench Social Graph(key 48B/value 228B), Twitter cache cluster trace(cluster 39 write-intensive, 51 read-intensive, 19 mixed). 비교 대상은 Titan, DiffKV, BlobDB, RocksDB, 그리고 ideal target인 Titan w/o GC.

- **Throughput**: write-intensive(YCSB A, F)에서 BlobDB/Titan/DiffKV/RocksDB 대비 **1.11-4.66배** 향상하며 no-GC(Titan w/o GC)에 근접. read-intensive(B, C, D)는 다른 시스템과 거의 동일. production Social Graph에서 1.07-2.16배, Twitter cluster39에서 7.6-97.7% 향상.
- **Tail Latency**: write-intensive(A, F)에서 DiffKV/Titan/RocksDB/BlobDB 대비 각각 **14.7%, 85.9%, 31.2%, 36.8%** 낮은 99% latency. GC를 critical path에서 제거(off-loading)한 덕분이며 Titan w/o GC와 유사.
- **Space Usage**: Direct GC 정책의 실시간 garbage 모니터링·timely GC 덕분에 write-intensive에서 Titan/AegonKV는 RocksDB의 0.5-0.75배 추가 redundant space만 사용. 반면 Compaction-triggered GC인 BlobDB/DiffKV는 각각 **1.8x, 4.15x** redundancy. production에서 redundant space **31-55%** 감소.
- **Compaction I/O**: KV separation으로 RocksDB 대비 크게 감소. Titan w/o GC 대비 AegonKV는 +15%, Titan은 +51% (AegonKV가 GC-friendly metadata install로 LSM write-back을 덜 sift). DiffKV/RocksDB의 GC compaction I/O는 AegonKV의 **5.4-14.4배**.
- **Write Stall**: AegonKV와 Titan은 write stall이 0(KV separation이 compaction write stall 해결). DiffKV/BlobDB의 compaction-triggered GC는 stall 빈도/overhead를 RocksDB stall time의 18-20% 추가.
- **CPU/Power**: AegonKV의 normalized CPU utilization 77.17%로 다른 시스템(95-97%) 대비 약 20% 절감. energy efficiency는 GC computation을 저전력 SmartSSD로 offload하여 **15.9-69.8%** 향상.
- **Sensitivity**: garbage_ratio, gc_thread, batch_size 등 system parameter 변화에도 AegonKV의 throughput/tail latency가 거의 불변(GC를 critical path에서 분리). value_size/db_size/workload_ratio/client_thread 변화에도 최적 적응성과 multi-thread scalability 유지.

> [!quote]- 📄 원문 표현 (paper)
> - "AegonKV has exhibited throughput enhancements ranging from 1.11 to 4.66 times in write-intensive workloads (A and F) and closely approaches the scenario of no GC overhead (Titan w/o GC)." (p.9)
> - "AegonKV achieves the lowest tail latency, which is reduced by 14.7%, 85.9%, 31.2%, and 36.8% compared to DiffKV, Titan, RocksDB, and BlobDB, respectively." (p.9)
> - "AegonKV achieves approximately a 20% savings ... AegonKV achieves the highest energy efficiency, improving by 15.9% to 69.8%." (p.11)

## 섹션 노트

- **Section 1 Introduction**: KV separation의 장단점과 세 지표 동시 만족의 어려움 제기, SmartSSD 기반 GC offloading을 최초 제안한다고 명시.
- **Section 2 Background**: 2.1 KV-Separated LSM(Figure 1, key는 LSM·value는 vLog/Blob, flush 시 value를 vLog에 append하고 offset을 SSTable에 기록), GC 방식(hash-based, key-range-based, Titan/PinK의 vLog→Blob file 분할). Figure 2는 Direct GC와 Compaction-triggered GC 비교. 2.2 SmartSSD(FPGA+SSD+FPGA DRAM, NVMe/FPGA/FPGA DRAM을 PCIe Switch로 노출, P2P communication).
- **Section 3 Motivation and Challenge**: Table 1-3, Figure 3 동기 실험. Direct GC bottleneck = index data I/O, SmartSSD offloading 기회와 세 challenge(Read Back Travel, Resource Limitation, Extra Data Movement).
- **Section 4 Design**: GC Manager(ValidMap), GC Scheduler(I/O ctl·CU ctl·Meta install), GC Friendly Metadata Install(deferred validation, version conflict Figure 7-8), Compute Unit for GC(Figure 9 pipeline).
- **Section 5 Evaluation**: setup, YCSB, production, overhead(resource/CPU/power), sensitivity.
- **Section 6 Related Works**: DiffKV/SpacKV/Parallax/SineKV(LSM 기반 KV separation), PinK(KV-SSD, write-back 최적화+compaction HW accel), Bourbon/DumpKV(ML 기반). AegonKV는 GC offloading에 초점(compaction보다 GC overhead가 더 큼)이라는 차별점.
- **Section 7 Conclusion + Artifact Appendix**: 코드 https://github.com/CGCL-codes/AegonKV.

## 핵심 용어

- **KV separation**: key와 value를 분리 저장하여 compaction이 value를 다루지 않게 함으로써 write amplification을 줄이는 LSM 최적화 아키텍처.
- **Value region / vLog / Blob file**: value가 log-structured로 저장되는 영역. vLog를 작게 나눈 단위가 Blob file.
- **Direct GC**: Blob file을 직접 스캔하며 LSM에 valid 여부를 query하고 valid value를 새 Blob로 재배치, 새 주소를 LSM에 write(GC metadata)하는 방식(Titan).
- **Compaction-triggered GC**: Blob을 multi-layer LSM으로 만들고 compaction 시 value migration을 수행하는 방식(DiffKV, BlobDB). timeliness↓, space/compaction overhead↑.
- **SmartSSD**: FPGA를 SSD 내부에 통합한 computational/near-data storage. NVMe SSD-FPGA DRAM 간 P2P 경로로 PCIe bus 우회.
- **P2P (peer-to-peer)**: SmartSSD 내부에서 NVMe SSD와 FPGA DRAM 사이의 직접 data path(기본 단위 4KB).
- **ValidMap**: 각 Blob file의 offset별 valid/invalid를 표현하는 Bitmap 데이터 구조(bit 0=valid, 1=invalid). hardware GC의 보조 구조.
- **CU (Compute Unit)**: FPGA 상의 GC computing logic. Input Decode→Data Compute→Output Encode 3-stage pipeline.
- **GC-friendly metadata install / deferred validation**: validation 없이 MemTable에 batch insert하고, Get/compaction 시 한 단계 더 backward query로 version 정합성을 늦게 판정하는 기법.
- **Write stall / write amplification**: compaction이 foreground write를 막아 발생하는 지연 / 실제 사용자 write 대비 실제 storage write 증가량.

## 강점 · 한계 · 열린 질문

- **강점**: KV-separated GC를 SmartSSD로 offload한 최초 연구로, throughput·tail latency·space cost의 trade-off를 동시에 깨뜨림. GC를 critical path에서 분리해 parameter 민감도가 낮고 multi-thread scalability·energy efficiency가 우수. ValidMap이 매우 작아(32MB Blob당 ~4KB) hardware 친화적. Artifact가 공개(Available/Functional 평가).
- **한계**: SmartSSD라는 특정 hardware(Xilinx UltraScale+ FPGA, 4GB DRAM)에 의존하며 CU 수(8개)와 자원(CLB 97.6%)이 hardware 제약에 묶임. read-intensive workload에서는 GC trigger가 적어 이점이 작거나 일부 지표에서 Titan/Titan w/o GC 대비 약간의 성능 손실(read-only에서 1.0/4.3/10.5%). deferred validation은 작지만 추가 backward query overhead를 가짐.
- **열린 질문**: 더 큰 value나 다양한 FPGA/computational storage 세대에서의 일반화? compaction까지 함께 offload하면 추가 이득이 있는지(현재는 GC만 offload)? multi-device/distributed 환경 확장성?

## ❓ Q&A (자가 점검)

> [!question]- Q1. AegonKV가 "three-birds-one-stone"이라 부르는 세 가지 목표는?
> KV-separated 시스템의 throughput, tail latency, storage cost(space usage)를 동시에 개선하는 것. 기존 시스템은 이 셋 사이 trade-off를 벗어나지 못했다.

> [!question]- Q2. 기존 GC 방식 두 가지와 각각의 약점은?
> (1) Direct GC(Titan): Blob을 직접 스캔하며 LSM index를 query/write → index data I/O가 host bandwidth·CPU와 경쟁해 throughput·tail latency 악화. (2) Compaction-triggered GC(DiffKV/BlobDB): compaction 시 value migration → GC timeliness 희생으로 redundant space(1.8x-4.15x)와 compaction overhead 폭증.

> [!question]- Q3. SmartSSD가 GC offloading에 적합한 이유는?
> 내부 FPGA의 독립 bandwidth로 host bandwidth contention을 피하고, NVMe SSD-FPGA DRAM 간 P2P로 PCIe bus를 우회해 data movement를 줄이며, FPGA로 GC를 pipeline화해 host CPU 압력을 완화할 수 있다. value file은 sequentially independent streaming이라 병렬 pipeline에 적합하다.

> [!question]- Q4. ValidMap은 무엇이고 왜 hardware에 적합한가?
> Blob file의 offset별 valid(0)/invalid(1)를 나타내는 Bitmap. KV당 1 bit만 쓰므로 32MB Blob당 이론상 ~4KB로 매우 작아, FPGA의 제한된 메모리에서 빠른 traversal/lookup이 가능한 보조 GC 구조다.

> [!question]- Q5. GC-friendly metadata install의 consistency 문제와 해결책은?
> validation 없이 MemTable에 batch insert하면 GC 중 더 새로운 version을 덮어쓰는 version conflict가 생길 수 있다(Figure 7). 이를 deferred validation으로 해결: Get/compaction 시 GC write-back state flag를 보고 한 단계 더 backward query하여 GC 중 새 key가 쓰였는지 비교·판정하고, metadata를 normal data로 promote해 redundancy를 작게 유지한다.

> [!question]- Q6. AegonKV의 대표 성능 수치는?
> 기존 KV-separated 시스템 대비 throughput 1.28-3.3배 향상, tail latency 37-66% 감소, space overhead 15-85% 감소. write-intensive에서 throughput 1.11-4.66배, CPU 약 20% 절감, energy efficiency 15.9-69.8% 향상, write stall 0.

> [!question]- Q7. read-intensive workload에서 AegonKV의 특징은?
> GC trigger rate가 낮고 read stream이 시스템 간 본질적으로 동일하여 throughput/tail latency 차이가 작다. read-only에서는 ValidMap 유지 등으로 Titan/Titan w/o GC 대비 1.0/4.3/10.5%의 소폭 성능 손실이 있고, scan(YCSB-E)은 index-optimized DiffKV가 가장 좋다.

> [!question]- Q8. CU(Compute Unit)는 어떻게 구성되며 몇 개를 배치했는가?
> Input Decode→Data Compute→Output Encode 3-stage pipeline(Vitis-HLS)으로, Fetcher/Decoder/Filter/Encoder/Buffer/Meta Collector 모듈을 갖는다. CLB가 limiting factor이며 8개의 CU를 배치했다(CLB utilization 97.6%).

## 🔗 Connections

[[KV-LSM]] · [[FAST]] · [[2025]] · 관련: [[HotRAP]] · [[Titan]] · [[DiffKV]] · [[BlobDB]] · [[PinK]] · [[WiscKey]] · [[SmartSSD]]

## References worth following

- WiscKey: Separating Keys from Values in SSD-conscious Storage (FAST '16) — KV separation의 원조 [3].
- PinK: High-speed in-storage key-value store with bounded tails (USENIX ATC '20) — KV-SSD, write-back 최적화·HW compaction accel, deferred update의 기반 [9].
- DiffKV / Differentiated Key-Value Storage Management for Balanced I/O Performance (USENIX ATC '21) — compaction-triggered GC 대표 비교 대상 [8].
- HashKV: Enabling Efficient Updates in KV Storage via Hashing (USENIX ATC '18) — hash-based value 관리·GC overhead 분석 [4].
- SmartSSD: FPGA Accelerated Near-Storage Data Analytics on SSD (IEEE CAL '20) — 본 연구 hardware 플랫폼 [28].
- From WiscKey to bourbon: A learned index for Log-Structured merge trees (OSDI '20) — ML 기반 KV separation 대안 [42].

## Personal annotations

<본인 메모 영역>
