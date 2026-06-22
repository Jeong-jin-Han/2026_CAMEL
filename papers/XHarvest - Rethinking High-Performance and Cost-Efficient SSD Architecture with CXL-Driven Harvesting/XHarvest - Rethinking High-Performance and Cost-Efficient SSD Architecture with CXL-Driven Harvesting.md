---
title: "XHarvest: Rethinking High-Performance and Cost-Efficient SSD Architecture with CXL-Driven Harvesting"
aliases: [XHarvest]
description: "CXL와 TEE(SGX)로 host CPU·memory를 동적 harvesting하여 SSD 내부 자원을 줄이고도 고성능을 유지하는 비용효율 SSD 아키텍처 (ISCA'25)"
venue: ISCA
year: 2025
tier: deep
status: done
presenter: 정진
present-date: 2026-07-23
tags:
  - paper
  - cluster/mine
  - cluster/cxl
  - topic/cxl
  - topic/ssd-architecture
  - topic/resource-harvesting
  - topic/cost-efficiency
  - venue/isca
  - year/2025
---

# XHarvest: Rethinking High-Performance and Cost-Efficient SSD Architecture with CXL-Driven Harvesting
> **ISCA 2025** · `cluster/cxl` · Source: [XHarvest - Rethinking High-Performance and Cost-Efficient SSD Architecture with CXL-Driven Harvesting.pdf](<XHarvest - Rethinking High-Performance and Cost-Efficient SSD Architecture with CXL-Driven Harvesting.pdf>)

저자: Li Peng, Wenbo Wu, Shushu Yi, Xianzhang Chen, Chenxi Wang, Shengwen Liang, Zhe Wang, Nong Xiao, Qiao Li, Mingzhe Zhang, Jie Zhang (Peking University 외)

## TL;DR
프로덕션 클러스터에서 I/O burst는 가끔만 발생하므로 비싼 SSD 내부 자원(computation·memory)이 항상 underutilized된다. **XHarvest**는 SSD 내부 자원을 *moderate* 수준만 남기고, I/O burst 시점에만 **CXL**과 **TEE(Intel SGX)**를 이용해 **host의 CPU·memory를 동적으로 harvesting**한다. firmware를 host의 enclave 안에서 실행해 firmware leakage를 막고, CXL의 cache-coherent·fine-grained 인터페이스로 host memory와 SSD 내부 DRAM을 통합한 metadata cache(FTL cache)를 구축한다. 결과적으로 conventional SSD 대비 hardware cost를 31.50% 절감하면서 동등하거나 더 높은 throughput을 달성하고, memory-intensive 작업에서 OCSSD 대비 execution time을 2.27배 줄인다. (Abstract, p.1)

> [!quote]- 📄 원문 표현 (paper)
> - "we propose XHarvest, a new cost-efficient and high-performance SSD architecture, which harnesses compute express link (CXL) and trusted execution environment (TEE) to facilitate dynamic, efficient, and secure host resource harvesting." (p.1)
> - "The evaluation results show that XHarvest reduces the hardware cost by 31.50% while achieving the same or even higher performance than conventional SSDs. It relieves resource contention via dynamic harvesting, reducing 2.27× execution time over OCSSD in memory-intensive tasks." (p.1)

## 문제 & 동기 (Problem & Motivation)
- **Cost-utilization dilemma**: 고성능 SSD는 수백 개 NAND die, 16 channel, 10s GB/s bandwidth를 위해 많은 computation·DRAM을 내장한다(예: 7.5 TB SSD에 10 GB DRAM). 그러나 I/O burst의 occasional한 성격 때문에 이 자원이 심하게 underutilized된다. Alibaba 4,000 서버 8일 추적에서 **96.64%의 runtime 동안 I/O bandwidth utilization이 25% 미만**이었다. (§1, §3.1, p.2, Fig 3a, p.5)
- DRAM·computation은 1 TB SSD 비용의 각각 약 30%·10%를 차지한다. (§3.1, p.4)
- **OCSSD(Open-Channel SSD)**는 SSD 내부 자원을 모두 제거하고 host 자원(LightNVM/pblk)으로 firmware·metadata를 처리하지만, (1) user application과의 심각한 resource contention(memory를 정적으로 과다 예약, Alibaba에서 메모리 사용 ~90%), (2) hampered host-SSD collaboration(PCIe가 host/SSD를 분리된 cache-coherency domain으로 둠 → host가 내부 DRAM 직접 접근 불가, OS stack 경유로 ~10us 지연 vs single memory shot 200ns), (3) compromised host CPU exploitation(unprotected firmware의 leakage risk로 vendor가 OS kernel 통합을 거부)이라는 채택 장벽이 있다. Linux 5.15에서 LightNVM/pblk이 deprecate되었다. (§3.2, p.4–5)
- **Naive solution**(OCSSD에 moderate DRAM·ARM 추가)도 같은 3가지 문제를 못 푼다(static memory 예약으로 contention 지속, hampered collaboration, security risk). pblk을 host CPU로 직접 포팅하면 FTL processing throughput이 95.81% 저하된다. (§3.2, p.5)

> [!quote]- 📄 원문 표현 (paper)
> - "The occasional nature of I/O bursts in production clusters makes the substantial and expensive SSD internal hardware resources (e.g., computation and memory resources) always underutilized, resulting in cost inefficiency." (p.1)
> - "Figure 3a shows the distribution of I/O bandwidth utilization for 4,000 Alibaba servers ... These servers utilize less than 25% of the I/O bandwidth during more than 96.64% of runtime." (p.4)
> - "this hampered host-SSD collaboration results in 95.81% degradation in FTL processing throughput" (p.5)

## 핵심 통찰 (Key Insight)
- **Host resource harvesting**: I/O burst는 *소수의 SSD에서만 가끔* 발생하고 나머지는 idle하다(burst interleaving). active VM(I/O bandwidth >50%) 수가 5개를 동시에 넘는 경우는 runtime의 2.20%뿐 → on-demand로 host memory를 SSD에 동적 할당해도 동시 memory 사용은 낮게 유지된다. CPU도 I/O load가 85~100%일 때 Low utilization이 45.69%로 우세 → high I/O load 동안 idle host CPU를 harvest할 기회가 있다. (§3.3, p.6, Fig 3c/3d, p.5)
- **CXL**: CXL.mem(HDM, load/store로 cacheline 단위 접근)과 CXL.cache(cache-coherent)로 host·device 간 unified memory space와 fine-grained·direct memory access를 OS 개입 없이 제공 → host-SSD collaboration을 streamline하고 불필요한 copy를 제거한다. (§3.3, p.6, Fig 5a)
- **TEE(Intel SGX)**: enclave 안에서 sensitive firmware를 실행해 compromised OS/malicious entity로부터 firmware leakage를 막는다. EPC, attestation 제공. 하지만 enclave는 user level이라 SSD를 직접 접근 못 하고 ecall/ocall에 20K cycle 이상 overhead가 든다 → CXL로 해결. (§3.3, p.6; §4, p.6)
- **CXL 3.1 TEE security protocol(TSP)**: trusted entity(enclave↔SSD) 간 CXL traffic을 암호화 → 추가 en/decryption 없이 metadata 보호. (§4, p.6; §5.2, p.7)

> [!quote]- 📄 원문 표현 (paper)
> - "The resource usage in datacenters shows that the I/O burst occurs occasionally on a minority of SSDs while the majority remains idle, indicating that only a few SSDs need host memory to buffer their metadata at any given time." (p.2)
> - "This presents an opportunity for SSDs to harvest the idle host CPU during high I/O loads. By harvesting host resources during peak loads, the SSD can avoid substantial resource aggregation and only needs to retain moderate resources to serve regular I/O loads." (p.5)

## 설계 / 메커니즘 (Design)
**기본 구성**: XHarvest는 conventional SSD computing power의 25%와 FTL mapping table의 10%를 수용할 memory만 남기고(modest DRAM·modest comp.) 나머지 underutilized 자원을 제거한다. low I/O load에서는 내부 자원으로 isolation·base performance를 보장하고, I/O burst 시 host를 harvest한다. (§4, p.6, Fig 7)

1. **Secure CPU harvesting (§5.1, p.7)** — SSD가 high I/O load를 감지하면 daemon thread가 host에서 enclave를 launch → attestation service로 무결성 검증 → SSD가 암호화된 firmware binary와 decryption key를 enclave에 전달. enclave는 단일 host CPU core에 pin되어 NVMe controller의 request queue를 polling하며 I/O를 처리한다. FTL cache에서 LPN→PPN 변환 후 PPN을 flash backbone에 dispatch. (Fig 8)
2. **Load detector + Dynamic enclave launch (§5.4, p.8)** — SSD 내부 load detector가 time window(예: 5ms)로 I/O load를 모니터링, threshold(예: 60%) 초과 시 host memory의 flag를 set. daemon thread가 저빈도(예: 1ms)로 polling(CPU 0.1%). 지연 큰 enclave instantiation을 critical path에서 분리: startup 시 enclave를 미리 만들되 EPC는 할당 안 하고, high load 감지 시 ecall로 재진입해 EPC를 할당. harvesting을 5ms 안에 활성화하며 tail latency에 영향 없음. (§1, p.3)
3. **CXL-driven efficient communication (§5.2, p.7)** — enclave와 SSD는 CXL TSP의 CMA(component measurement and authentication)로 mutual authentication 후 AES-GCM 대칭키 교환. *secure CXL traffic*은 byte당 약 1 CPU cycle만 들어 CXL memory 접근 대비 약 5% latency overhead. SSD 내부 memory 일부를 ring buffer로 쓰는 message-passing(64 byte message, ready bit, head/tail pointer) 메커니즘으로 ocall/ecall 없이 통신. (Fig 9)
4. **Memory harvesting (§5.3, p.8)** — CXL로 host memory(EPC)와 SSD 내부 DRAM을 cache-coherent하게 통합한 **unified FTL cache** 구축. enclave가 EPC+내부 memory로 combined FTL cache 생성, translation page를 LRU로 관리(hash table로 추적). SSD firmware도 host memory로 FTL cache 구축 가능하되 일반 memory라 mapping entry를 개별 암호화(AES) + CRC/ECC integrity 검증. (Fig 10)
5. **Harmonized host-SSD coordination (§5.4, p.8)** — enclave(host CPU)와 SSD firmware가 동시에 I/O를 처리 시 memory·flash contention 발생 → computing power 비례로 flash channel·memory 분배(예: firmware:enclave = 1:6). logical address space를 2MB 단위로 나눠 round-robin partitioning(예: 7개 중 1개는 SSD, 6개는 enclave)으로 I/O load를 자원 비례로 분산. (Fig 22)

**Implementation**: NVMe·CXL spec을 따라 hardware violation 없이 구현, NUMA로 CXL.mem/cache emulation(NVMeVirt로 PCIe/NVMe emulation). firmware는 5K LOC, enclave는 3K LOC. OS/application 수정 불필요. (§5.5, p.9; §6.1, p.9, Fig 11)

> [!quote]- 📄 원문 표현 (paper)
> - "By default, XHarvest reserves 25% of the computing power of the conventional SSD and memory that can accommodate 10% of the FTL mapping table." (p.6)
> - "We further design a secure and efficient communication mechanism that empowers the enclave and the SSD to handle I/O bursts cooperatively." (p.3)
> - "This symmetric encryption requires only almost one CPU cycle per byte, thus introducing a mere 5% latency overhead compared to the latency of accessing CXL memory." (p.7)
> - "This mechanism can activate harvesting within 5 ms without affecting the tail latency for prolonged workloads." (p.3)

## 평가 (Evaluation)
- **Setup**: dual Intel Xeon 8562Y+ CPU, 480 GB DRAM. emulated SSD read/write latency 25us/50us, 200K Random IOPS. 비교 대상: ConvSSD(conventional), OCSSD, DLSSD(DRAMless), DLSSD+LocalMem, Base(XHarvest 베이스, 내부 comp 25%·memory 10%만), Base+CPU, XHarvest. workload: MSR/FIU/SYSTOR/Alibaba block trace. (§6.1, p.9, Table 1/2, Fig 11)
- **Cost (Fig 17, p.11)**: 1TB·4TB·16TB SSD에서 XHarvest는 ConvSSD 대비 hardware cost를 각각 **31.50%·19.83%·15.49%** 절감하면서 동등 이상 성능. cost efficiency(performance per unit cost)는 Terasort에서 OCSSD-M·OCSSD-H 대비 각각 **19.21%·28.15%** 향상. (Fig 16c, p.10)
- **Microbenchmark (Fig 12, p.10)**: XHarvest는 host memory harvesting으로 Base를 최대 **63.04%** 능가, 64K-RandRd에서 Base+CPU 대비 **36.80%** 향상. ConvSSD에 **11.52%** 뒤지지만 OCSSD를 **39.29%** 능가(OCSSD는 host 자원 전적 의존 → contention). (§6.2)
- **Macrobenchmark (Fig 13, p.10)**: Base+CPU·XHarvest가 각 counterpart 대비 bandwidth 17.39%·6.31% 향상. casa에서 Base+CPU가 DLSSD 대비 77.28% bandwidth, XHarvest가 ConvSSD 대비 5.02% bandwidth 향상, OCSSD에 0.70%만 뒤짐. (§6.2)
- **50th latency (Fig 14, p.10)**: DLSSD w/o HMB는 ConvSSD 대비 83.28% 증가, XHarvest는 host CPU·CXL로 ConvSSD 대비 latency 증가를 **37.84%**로 억제. (§6.2)
- **Real-world (Rocksdb mixgraph, Fig 16a/16b, p.10)**: XHarvest가 OCSSD-H 대비 throughput **11.59%** 향상, average latency **8.55%** 감소. Terasort에서 OCSSD-M·OCSSD-H 대비 execution time을 64GB·16GB memory에서 각각 **2.27배·36.71%** 감소. (§6.2)
- **Performance analysis (§6.3, p.11)**: XHarvest-SSD/Host가 100% hit ratio에서 DLSSD 대비 throughput을 2.75배·3.63배 향상. XHarvest-SSD/Host가 metadata traffic을 DLSSD 대비 33.97%·33.89% 절감, FTL latency를 55.95%·60.16% 감소. CXL latency 75ns~135ns 변화 시 communication(32 depth) latency는 51.16% 증가하지만 throughput은 32.93% 감소에 그쳐 bottleneck 아님. (Fig 20, Fig 25)
- **Energy (Fig 18, p.11)**: prn1에서 DLSSD는 OCSSD 대비 SSDmem·PCIe·Hostmem을 30.19%·26.62%·26.18% 더 소비, XHarvest는 CXL로 metadata movement를 줄여 SSDctrl·CPU energy overhead를 64.03% 절감. Terasort에서 OCSSD-H·DLSSD가 XHarvest 대비 energy efficiency를 36.29%·14.07% 저하. (§6.2)

> [!quote]- 📄 원문 표현 (paper)
> - "For 1TB, 4TB, and 16TB SSD, XHarvest reduces the hardware cost by 31.50%, 19.83%, and 15.49%, respectively, while achieving the same or even higher performance than ConvSSD" (p.11)
> - "Figure 16a shows that XHarvest outperforms OCSSD-H by 11.59% in throughput and reduces average latency by 8.55%, benefiting from moderate resource harvesting." (p.10)
> - "It reduces execution time by 2.27× and 36.71% with 64 and 16GB memory over OCSSD-H, respectively, due to reduced memory contention." (p.10)
> - "XHarvest harvests the host-side memory by 63.04% by host memory harvesting." (p.10)

## 섹션 노트 (Section notes)
- **§1 Introduction**: cost-utilization dilemma 제기, OCSSD/naive solution의 3대 문제(memory contention vs cost, hampered host-SSD collaboration, compromised host CPU), 3대 emerging technique(host resource harvesting, CXL, TEE)로 해결. Fig 1: OCSSD(a)/naive(b)/XHarvest(c) 비교.
- **§2 Background**: SSD 아키텍처(flash backbone, ARM controller, NVMe controller, DMA, DRAM controller), firmware/FTL(LPN→PPN 매핑, WL, GC), ONFi/PCIe 발전(PCIe 5.0 14 GB/s, PCIe 5.0이 3.0 대비 4.43배 computing 필요).
- **§3 Motivation**: Alibaba cluster 측정(I/O BW <25%가 96.64%, memory ~90%, active VM>5 동시는 2.20%), OCSSD/naive 한계, key insights(harvesting, CXL, SGX).
- **§4 Overview**: Fig 7. workflow(load detector → daemon → enclave launch → CXL-driven comm → FTL cache).
- **§5 Design**: secure CPU harvesting, CXL-driven communication, memory harvesting, harmonized coordination, implementation.
- **§6 Evaluation**: cost/throughput/latency/energy.
- **§7 Related Work**: ZNS SSD, LMB(CXL로 DMA 대체), BlockFlex(flash harvesting), CXL-SSD, TEE. XHarvest는 commercially deployed CXL·TEE 기반이라 firmware 수정만으로 배포 가능(practicality).
- **§8 Conclusion**: cost saving 31.50%, throughput 5.02% 향상.

## 핵심 용어 (Key terms)
- **CXL (Compute Express Link)**: host와 peripheral을 unified cache-coherency domain으로 묶는 인터커넥트. CXL.io(=PCIe), CXL.cache(device가 host memory cache), CXL.mem(host가 device memory를 HDM으로 load/store) 프로토콜. (§3.3)
- **Harvesting**: I/O burst 시 idle한 host CPU·memory를 동적으로 빌려 SSD가 사용하는 기법. (§1)
- **OCSSD (Open-Channel SSD)**: 내부 computation·DRAM을 제거하고 host(LightNVM/pblk)가 firmware·FTL을 처리하는 SSD. Linux 5.15에서 deprecate. (§3.2)
- **TEE / enclave (Intel SGX)**: sensitive code·data를 EPC에 암호화 격리해 OS·타 SW로부터 보호하는 실행 환경. attestation으로 무결성 증명. (§3.3)
- **FTL (Flash Translation Layer) / FTL cache**: LPN→PPN 매핑 indirection layer. XHarvest는 host memory+내부 DRAM을 통합한 unified FTL(metadata) cache로 구성. (§2, §5.3)
- **Load detector**: SSD 내부에서 time window로 I/O load를 모니터링, threshold 초과 시 harvesting을 trigger하는 컴포넌트. (§5.4)
- **CXL TSP (TEE Security Protocol)**: CXL 3.1 기능, trusted entity 간 CXL flit traffic을 암호화. CMA로 mutual authentication. (§4, §5.2)
- **Host-SSD coordination (harmonized)**: enclave(host CPU)와 SSD firmware 간 flash channel·memory를 computing power 비례로 partition하고 I/O load를 분산하는 프레임워크. (§5.4)
- **EPC (Enclave Page Cache)**: SGX가 sensitive 데이터를 암호화 저장하는 보호된 host memory 영역. (§3.3)
- **DLSSD (DRAMless SSD)**: 내부 DRAM을 생략하고 host memory에 on-demand FTL cache를 두되 SRAM으로 hot metadata buffering, 전송 시 암호화하는 비용효율 SSD. (§6.1)

## 강점 · 한계 · 열린 질문
**강점**
- 실제 production cluster(Alibaba 4,000 서버) 측정으로 underutilization·burst interleaving을 입증해 harvesting 동기를 탄탄히 함.
- CXL(성능)+TEE(보안)를 결합해 OCSSD의 3대 문제(contention·collaboration·security)를 동시에 해결.
- commercially deployed 기술 기반 + OS/application 수정 불필요 → practicality 강조(§7).
- cost 31.50% 절감과 성능 유지를 동시에 달성, energy도 개선.

**한계 / 주의**
- 실제 CXL/TEE 통합 하드웨어 부재로 **NUMA + NVMeVirt emulation**으로 평가 → secure CXL traffic의 marginal overhead를 무시했다고 명시(slightly impacting evaluation). (§6.1, p.9)
- TSP(CXL 3.1) 같은 일부 기능은 prototype 검증 단계로, ready-to-integrate hardware 부족. (§6.1)
- 단일 harvested core 기준 분석이 많음(XHarvest-1C/2C); 3 SSD에서 6.99% avg / 31.51% median latency 증가(다만 inadequate computing power 때문이며 rare case라고 주장). (§6.2, p.11)
- SGX 의존 → SGX deprecation/제약(다른 플랫폼 AMD SEV·ARM TrustZone·RISC-V Keystone로의 포팅은 future work로 언급). (§7)

**열린 질문**
- 실제 CXL 3.1 TSP 하드웨어에서 5% overhead 가정이 유지되는가?
- multi-tenant 환경에서 host CPU/memory harvesting이 application SLA를 침해하지 않는다는 보장은?
- load detector threshold/window 튜닝의 워크로드 민감도는?

## ❓ Q&A (자가 점검)
> [!question]- Q1. XHarvest가 풀려는 핵심 문제(cost-utilization dilemma)는?
> 고성능 SSD는 I/O burst를 위해 많은 computation·DRAM을 내장하지만, burst는 가끔만 발생해(Alibaba에서 I/O BW <25%가 96.64% runtime) 이 비싼 자원이 항상 underutilized된다. DRAM·comp은 1TB SSD 비용의 ~30%·10%. 성능을 희생하지 않고 underutilized 자원·비용을 줄이는 것이 목표. (§1, §3.1)

> [!question]- Q2. OCSSD/naive solution이 왜 충분치 않은가?
> (1) static memory 예약으로 user app과 contention(메모리 ~90%), (2) PCIe가 host/SSD를 분리 도메인으로 둬 host가 내부 DRAM 직접 접근 불가(hampered collaboration, OS stack ~10us), (3) unprotected firmware의 leakage risk로 vendor가 통합 거부. pblk을 host로 포팅 시 FTL throughput 95.81% 저하. (§3.2)

> [!question]- Q3. CXL과 TEE는 각각 어떤 역할을 하는가?
> CXL은 cache-coherent·fine-grained load/store로 host memory와 SSD 내부 DRAM을 unified metadata cache로 통합해 OS stack을 우회(collaboration·copy 제거). TEE(SGX enclave)는 firmware를 host에서 격리 실행해 firmware leakage를 막고, CXL 3.1 TSP로 enclave↔SSD traffic을 암호화한다. (§3.3, §4)

> [!question]- Q4. harvesting은 언제, 어떻게 trigger되는가?
> SSD 내부 load detector가 time window(예: 5ms)로 I/O load를 보고 threshold(예: 60%) 초과 시 host memory flag를 set. daemon thread가 저빈도(1ms, CPU 0.1%) polling으로 flag를 보고 enclave를 활성화. enclave를 미리 만들어두되 EPC는 high load 시에만 할당해 critical path에서 분리, 5ms 내 활성화하며 tail latency 영향 없음. (§5.4)

> [!question]- Q5. host CPU와 SSD firmware가 동시에 일할 때 contention은 어떻게 처리하나?
> harmonized host-SSD coordination이 flash channel·memory를 computing power 비례(예: firmware:enclave=1:6)로 분배하고, logical address space를 2MB 단위로 나눠 round-robin(7중 1은 SSD, 6은 enclave) partitioning해 I/O load를 자원 비례로 분산한다. (§5.4)

> [!question]- Q6. CXL-driven communication이 ocall/ecall 대비 나은 이유는?
> ecall/ocall은 20K cycle 이상 overhead. XHarvest는 CXL TSP로 mutual auth 후 AES-GCM 키 교환, SSD 내부 memory ring buffer 기반 64-byte message-passing(ready bit·head/tail pointer)으로 OS 개입 없이 통신한다. secure CXL traffic은 byte당 ~1 cycle(약 5% overhead). XHarvest-CXL이 SGXnaive를 6.85배 능가. (§5.2, §6.3)

> [!question]- Q7. 핵심 정량 결과 3가지는?
> hardware cost 31.50% 절감(1TB, ConvSSD 대비)하며 동등 이상 성능, memory-intensive Terasort에서 OCSSD 대비 execution time 2.27배 감소, conventional SSD 대비 throughput 5.02% 향상. (Abstract, §6, §8)

> [!question]- Q8. emulation 기반 평가의 한계는?
> 실제 CXL/TEE 통합 HW 부재로 NUMA로 CXL.mem/cache, NVMeVirt로 PCIe/NVMe를 emulation했고, secure CXL traffic의 marginal overhead를 무시해 평가에 약간 영향이 있다고 저자가 명시. (§6.1)

## 🔗 Connections
[[CXL]] · [[ISCA]] · [[2025]]
관련: [[SkyByte]] (memory-semantic CXL-SSD), [[Smart-Infinity]], [[Sparse Checkpointing for Fast and Reliable MoE Training]]

## References worth following
- BlockFlex: Enabling Storage Harvesting with Software-Defined Flash in Modern Cloud Platforms (OSDI '22) — flash 자원 harvesting (cf. §7, [86]).
- LMB: Augmenting PCIe Devices with CXL-Linked Memory Buffer (arXiv 2024) — CXL로 DMA 대체, DRAMless FTL cache (§7, [119]).
- LightNVM: The Linux Open-Channel SSD Subsystem (FAST '17) — OCSSD/pblk 기반 (§3.2, [8]).
- Intel SGX 관련 attestation/Iceclave: A trusted execution environment for in-storage computing (MICRO '21) — in-storage TEE (§3.3/§7, [44]).
- CXL 3.1 Specification & TEE Security Protocol(TSP)/CXL IDE — secure CXL traffic 근거 (§4/§5.2, [15][16][66][106]).

## Personal annotations
