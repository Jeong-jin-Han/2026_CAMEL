---
title: "Burstable Cloud Block Storage with Data Processing Units"
description: "Alibaba Cloud 프로덕션 CBS의 DPU(xDPU) 상 Storage Agent 병목을 해소하는 HW/SW 공동설계 I/O 스케줄링 시스템 BurstCBS"
venue: OSDI
year: 2024
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/infra
  - venue/osdi
  - year/2024
  - list/26s-v2
  - topic/dpu
  - topic/cloud-block-storage
  - topic/io-scheduling
  - topic/multi-tenancy
---

# Burstable Cloud Block Storage with Data Processing Units

> **OSDI 2024** · cluster/infra · Source: [Burstable Cloud Block Storage with Data Processing Units.pdf](<Burstable Cloud Block Storage with Data Processing Units.pdf>)

저자: Junyi Shu (School of Computer Science, Peking University and Alibaba Cloud), Kun Qian (Alibaba Cloud), Ennan Zhai (Alibaba Cloud), Xuanzhe Liu (School of Computer Science, Peking University), Xin Jin (School of Computer Science, Peking University)

## TL;DR
Alibaba Cloud의 프로덕션 Cloud Block Storage(CBS)를 분석한 결과, burst 트래픽을 처리하는 compute-node 측 DPU(xDPU) 위의 storage agent(SA)가 성능 변동의 주범임을 밝힌다. 저자들은 (i) FPGA 기반 queue scaling으로 스레드 간 부하 불균형을 해소하고, (ii) 사용 이력 기반 동적 rate limiting인 burstable I/O scheduler(BIOS)로 base-level 성능을 보장하면서 burst를 허용하며, (iii) CPU/ingress/egress/software 4차원 vectorized I/O cost estimator로 서로 다른 I/O 타입의 자원 소모를 정확히 추정하는 BurstCBS를 제안한다. xDPU 위 standalone 패키지로 구현되어 기존 WildCBS(WRR+per-tenant rate limiter) 대비 base-level tenant의 평균 지연을 최대 85% 줄이고 혼잡 상황에서 최대 5배의 처리량을 제공하며, 실제 database 서비스(RDS)에서 SQL 쿼리 평균 지연을 최대 83% 낮춘다.

## 문제 & 동기
Alibaba Cloud CBS는 partitioning cluster·persistence cluster로 백엔드를 분산시켜 부하를 잘 분산하지만(Figure 1, p.784), 정작 병목은 compute node 쪽 DPU(xDPU)의 SA로 옮겨간다. burst capability(크레딧 기반으로 base-level 이상 처리량을 일시적으로 허용)는 이 병목을 더 악화시키는데, 여러 VM이 동시에 burst하면 서로 자원을 경쟁하며 congestion을 유발한다(p.784). 근본 원인은 SA가 유저 큐를 DPU 코어에 정적으로 매핑하고 FCFS로 처리하기 때문에, 한 스레드가 congested되어도 다른 유휴 스레드가 도와줄 수 없다는 것(p.784)과, DPU 자체에 자원 스케줄링 기능이 없다는 것(p.784)이다.

정량적으로, 프로덕션 클러스터에서 backend node의 IOPS/BPS 이용률은 낮게 균형 잡혀 있지만(Figure 2, p.785) disk capacity 이용률은 78%에 달해(p.785) backend가 병목이 아님을 보여준다. 반대로 base-level throughput 이용률 분포(Figure 4, p.785)를 보면 disk의 80% 이상이 base-level 절반도 안 쓰는 반면 일부는 300% 이상 burst한다(p.786) — over-provisioning 성향이 burst 유휴 자원 재활용의 기회이자 동시에 간섭의 원인이 된다. Figure 5(p.786)의 실제 인시던트에서는, victim 두 VM이 base-level BPS 이하로 안정적으로 돌고 있음에도 다른 tenant의 burst로 인해 밀리초 단위 지연 스파이크를 겪는다. 이 확률은 co-located VM 수 $N$, burst 확률 $p$, 임계 동시burst 수 $k$에 대해 $P(X \geq k) = 1 - \sum_{i=0}^{k-1} \binom{N}{i}(1-p)^{N-i}p^i$ (Eq. 1, p.786)로 모델링되며, 프로덕션에서는 compute node의 95% 이상이 32개 이하 VM을 호스팅함에도(p.786) 여전히 간섭이 관측된다.

> [!quote]- 📄 원문 표현 (paper)
> - "the distributed nature of cloud block end design and the over-provisioning tendency of cloud users result in relatively low utilization of storage servers and devices in terms of throughput" (p.783)
> - "lack of resource scheduling at data processing units (DPUs) is the root cause of performance interference." (p.784)
> - "Although Victim1 and Victim2 ran below their base-level BPS steadily, they both observed many unexpected millisecond-scale average latency spikes during that time." (p.786)

## 핵심 통찰 (Key Insight)
**1. Static thread-queue binding → FPGA 기반 1:N egress-queue scaling.** SA control thread가 유저 큐에 정적으로 매핑되어 있으면 한 스레드만 burst해도 그 스레드가 congested되고 다른 스레드는 놀게 된다(Figure 6, p.786). 소프트웨어 기반 centralized dispatcher나 work stealing은 스레드 간 메시징 오버헤드가 커서 실제로는 최대 35%의 처리량 손실을 일으킨다(Figure 11, p.788). 대신 최신 xDPU FPGA가 하나의 ingress queue를 여러 egress queue에 매핑하는 기능(1:N binding, Figure 10b, p.788)을 지원하게 만들어, 소프트웨어 동기화 없이 line rate에서 거의 완벽한 load balancing을 달성한다. 효과적인 이유는 부하 분산 로직을 소프트웨어 스레드가 아니라 하드웨어 매치-액션 테이블(FPGA)에 두어 폴링 오버헤드와 스레드 간 통신 자체를 제거하기 때문이다.

> [!quote]- 📄 원문 표현 (paper)
> - "FPGA is capable of performing lookup operations at a very high rate, which resembles a programmable switch that controls packets through match-action tables, making it an attractive candidate for offloading logic such as load balancing and rate control" (p.788)
> - "Figure 11 shows a 35% throughput loss if we switch to a work stealing prototype" (p.788)

**2. Base-level 보장 + burst 허용은 상충 — 사용 이력 기반 동적 rate limiting(BIOS).** 순수 work-conserving fair queuing(WildCBS류)은 하나의 tenant가 극단적으로 높은 I/O parallelism으로 자원을 독점하면 다른 base-level tenant를 굶길 수 있다(Figure 14, p.789 — VM2의 burst가 VM1의 모든 I/O를 지연시키는 사례). 반대로 정적 상한(BaseCBS류)은 유휴 자원을 재활용하지 못해 burst capacity가 $Res_{burst}$로 제한된다(p.789). BIOS는 매 스케줄링 주기마다 사용 이력에 따라 quota를 재분배하고(Algorithm 1, p.790), base-level 미만으로 사용하다 갑자기 튀는 tenant에게는 `power_of_two_choices`로 burst tenant 중 하나를 골라 즉시 자원을 회수하는 fast recovery 메커니즘(THROTTLE_IO, p.790)을 추가해, 정렬(sorting) 없이도 예측 오류를 빠르게 보정한다. 전체 자원의 일부를 공용 pool로 남겨두는 이중 안전장치(p.791)도 곁들인다.

> [!quote]- 📄 원문 표현 (paper)
> - "BIOS actively collects usage data and allocates resources in proportion to user demands. It provides strong protection on base-level performance by (i) enforcing the total resource allocation limit and (ii) resuming base-level provisioning as soon as it discovers insufficient resource allocation to under-utilizing tenants." (p.790)

**3. I/O 비용을 스칼라가 아니라 벡터로 모델링.** 4KB I/O는 128KB I/O보다 바이트당 CPU 비용이 8배 높은 반면 egress bandwidth 소모는 동일하다(p.791) — 즉 병목 자원 종류가 I/O 크기·타입마다 다르다. I/O 비용을 단일 스칼라로 표현하면 가장 많이 소모되는 자원 기준으로 과다 계상되어 자원을 낭비하고 burst 여력을 줄인다(Figure 16, p.791). BurstCBS는 CPU time·ingress·egress·software limit의 4차원 벡터로 비용을 분리 추정하고, 그중 프로파일링이 필요한 CPU time만 (product_type, rw, size) 튜플에 대한 선형 모델로 근사한다(Figure 17, p.791). 이 덕분에 자원 이용률을 높이면서도 지연 목표를 위반하지 않는다.

> [!quote]- 📄 원문 표현 (paper)
> - "If we describe I/O cost as a scalar, it creates resource underutilization... In contrast, if we decouple the costs of different resource types, higher resource utilization can be achieved without breaching the latency target." (p.791)

## 설계 / 메커니즘 (Design)
**xDPU/SA 배경.** xDPU는 Alibaba Cloud가 설계한 SoC로 compute node의 infra 서비스를 오프로드하며, 최신 버전은 2.0GHz 코어 8개(스토리지/네트워크/administration이 공유), 100Gbps 네트워크 포트 2개, guest VM 메모리에 직접 접근 가능한 DMA 엔진을 갖는다(p.785). SA는 control plane(wimpy CPU 코어, 그중 2~4개가 SA 전용 스레드)과 data plane(FPGA에 구현, DMA로 데이터 이동)으로 나뉘며, NVMe WRITE의 경우 커맨드는 FPGA로 직접 전달되고 control thread가 패킷 헤더를 구성, FPGA가 DMA로 실제 데이터를 fetch해 백엔드로 전송한다(Figure 3, p.785). CPU-only가 아니라 FPGA-CPU 협업 설계를 택한 이유는 (1) I/O 분할·패킷 캡슐화는 분기가 복잡해 FPGA 병렬성을 활용하기 어렵고, (2) 수천 개 커넥션 상태를 유지하려면 메모리가 필요한데 FPGA엔 부족하며, (3) FPGA 코드 개발·테스트 비용이 크기 때문이다(p.785).

**BurstCBS 개요(Figure 9, p.788).** xDPU 위 standalone 시스템 패키지로 세 컴포넌트를 통합: §5.1 high-performance queue scaling(inter-thread load balance), §5.2 burstable I/O scheduler(intra-thread scheduling), §5.3 vectorized I/O cost estimator.

**§5.1 High-performance queue scaling.** 초기 xDPU는 FPGA에 load balancing 기능이 없어 ingress queue마다 하나의 egress queue를 두고 SA control thread에 균등 배정하는 1:1 binding(Figure 10a, p.788)에 머물렀는데, 이는 vCPU별 I/O 강도 편차 때문에 부하 불균형을 유발했다(Figure 6, 7 — 4-core VM의 가장 바쁜 코어가 전체 I/O의 80.51%를 차지, p.787). 최신 xDPU는 하나의 ingress queue를 여러 egress queue에 매핑하는 1:N binding(Figure 10b)을 지원해 큐 재구성(수 초 소요, transient burst 처리 불가) 없이 스레드 간 재분배가 가능하다. 이를 뒷받침하기 위해 두 계층 메모리 풀(Figure 12, p.789)을 도입: 각 큐는 자신의 dedicated buffer pool(빠른 I/O를 위해 미리 채워짐)을 유지하되, I/O depth가 커지면 global shared pool에서 buffer를 빌려오고 burst가 끝나면 반납한다.

**§5.2 Burstable I/O scheduler(BIOS, Figure 15, p.791).** 매 스케줄링 주기마다 Usage Monitoring이 사용량을 수집하고 Token Allocation Algorithm(Algorithm 1)이 quota를 재계산한다. 1차 라운드: burst 중이거나 throttle된 적 있는 tenant는 `res_base`를, 그렇지 않으면 이력 기반 `alloc_hist × α`를 할당(lines 4-8). 2차 라운드: 남은 `unused` 자원을 가중치 $w_i = \dfrac{usage_i + throttle_i \times weight_{throttle}}{\sum(usage + throttle \times weight_{throttle})}$ (line 10)에 비례해 분배하되 `res_burst_i`를 넘지 않게 한다(lines 9-13). Throttle된 I/O에 더 큰 가중치를 주는 이유는 follow-up I/O가 많을 가능성이 높기 때문(p.790). Fast recovery: 자원이 바닥난 base-level tenant는 `power_of_two_choices`로 고른 burst tenant로부터 자원을 우선 회수하고(lines 18-20), 그래도 부족하면 예비로 남겨둔 `reserved` shared pool을 사용한다(lines 21-22, p.791). Dynamic rate limiter + global rate limiter + I/O Processing Engine 구조는 WRR(WildCBS)의 work-conserving한 장점과 Gimbal류의 base-level 보호 장점을 결합한다.

**§5.3 Vectorized I/O cost estimator.** I/O 비용을 스칼라 대신 (CPU time, ingress, egress, software limit) 4차원 벡터로 표현(Figure 16b, p.791). Ingress/egress/software limit은 스레드 수로 균등 분할되는 global limit이라 profiling이 필요 없고, CPU time만 (product_type, rw, size) 조합별로 4KB~16KB 구간에서 실측 후 선형 모델로 전체 크기에 대해 보간한다(그 이상은 병목이 CPU가 아니므로, p.791). Unpredictable misestimation 대응: FPGA 하드웨어 장애 등으로 예상 밖의 heavy error-handling 경로를 타면 I/O당 비용이 최대 2배까지 뛸 수 있어(p.792), delay-based cost adjustment를 도입 — target delay를 기준으로 지연이 초과되면 cost를 점진적으로 올리고 떨어지면 낮추며, SSD tail latency로 인한 오탐을 막기 위해 backend delay는 이 계산에서 제외한다(p.792). Table 3(p.792)에서 조정 적용 시 read/write 지연이 WildCBS 대비 크게 감소함을 보인다(WildCBS 1096.89/868.79us → BurstCBS w/ adjustment 248.97/270.49us).

> [!quote]- 📄 원문 표현 (paper)
> - "The newest version of xDPU adds support for load balancing by allowing mapping one ingress queue to multiple egress queues" (p.788)
> - "we describe the cost of an I/O as a vector of 4 dimensions: CPU time, ingress, egress, and software limit" (p.791)
> - "A fast recovery mechanism is added as compensation before the algorithm catches its mis-prediction in the next scheduling cycle." (p.790)

## 평가 (Evaluation)
실험은 최신 xDPU 탑재 compute node에서 FIO(IOPS-intensive: 4KB-16KB 혼합, BPS-intensive: 4KB-128KB 혼합) 워크로드로 진행, baseline은 WildCBS(WRR + per-tenant rate limiter, 현재 프로덕션 주력 버전)와 BaseCBS(Gimbal 변형, 강한 성능 격리)이다(§6, p.793).

- **Inter-thread load balancing (§6.1, Figure 18, p.793):** FPGA 기반 load balancing은 4KB/IOPS-intensive 워크로드에서 1~2 스레드로 near-linear scaling을 달성하며, 6개 SA control thread로 측정한 처리량 uniformity(Max/Min)는 사용자가 vCPU를 직접 균등 배분한 경우와 거의 동등(Figure 18c).
- **Base-level tenant latency (§6.2, Figure 19, 20, p.793):** BPS-intensive 배경 burst 하에서 BurstCBS는 WildCBS 대비 평균 지연을 68%–85% 감소시키며 BaseCBS(이상적 격리)에 근접. IOPS-intensive 배경에서는 40%–66% 감소(CPU 코어가 병목이 되어 BaseCBS와의 격차가 줄어듦).
- **Base-level tenant throughput (§6.3, Figure 21, 22, p.794):** base-level 20k IOPS 목표에서 BurstCBS는 4가지 I/O 타입 모두 depth 8 이내에 도달하는 반면, WildCBS는 8개 케이스 중 7개에서 목표에 실패하고 최저 4,000 IOPS까지 떨어짐.
- **Burst resource utilization (§6.4, Figure 23, p.794):** BurstCBS는 fast-recovery용 예비 pool(전체 자원의 5%) 때문에 WildCBS 대비 약 5%–8% 처리량을 손해봄(설계상 예상된 trade-off).
- **Responsiveness (§6.5, Figure 24, p.794):** base-level 45k IOPS VM을 유휴 후 재시작해도 60k IOPS 스트림 전까지는 throttled I/O가 발생하지 않음.
- **Scalability (§6.6, Figure 25, p.795):** VM 수를 4~1024로 늘려도 스케줄링 시간은 선형 증가하며 항상 100μs 이하; idle VM을 스케줄링에서 제외하는 최적화 적용 시 64개 active VM 기준 5μs 미만.
- **Application performance (§6.7, Figure 26, 27, p.795):** MySQL/RocksDB 100k operation 벤치마크에서 모든 write 연산(insert/update/put/delete) 지연이 WildCBS 대비 약 60% 감소; YCSB-RocksDB에서 2배 burst 시 BaseCBS 대비 1.7×–2.5× 처리량 향상(WildCBS에 근접).
- **Practical benefits — production RDS (§6.8, Figure 28, 29, p.795-796):** 내부 RDS(ProductB 메인 + ProductA 버퍼풀) 노드에 배포해 8/16개 burst neighbor 하에서 query 평균 지연 최대 83% 감소; 30분 프로덕션 트레이스 재생 시 WildCBS는 20-50ms 지연 스파이크를 보이는 반면 BurstCBS는 10ms 이하로 억제.

> [!quote]- 📄 원문 표현 (paper)
> - "BurstCBS reduces average latency by up to 85% and provides up to 5× throughput for base-level tenants under congestion with minimal overhead." (p.783, Abstract)
> - "BurstCBS loses about 5%–8% throughput compared to WildCBS, which meets our expectation because we keep 5% of the total resources in the shared pool for fast recovery." (p.794)
> - "while WildCBS creates latency spikes of 20-50ms, BurstCBS is able to keep it under 10ms." (p.796)

## 섹션 노트
- **§1 Introduction:** CBS는 disaggregated 아키텍처 위에서 burst capability를 제공하지만, DPU(xDPU)에서 실행되는 SA가 성능 변동의 주 원인임을 프로덕션 통계로 제시하고 BurstCBS의 3대 기법을 예고한다.
- **§2 Background:** CBS의 3계층(compute/partitioning/persistence cluster) 아키텍처와 xDPU 하드웨어 구성, SA의 control/data plane 분리를 설명한다.
- **§3 Key Observations and Implications:** 두 가지 관찰(burst가 compute node를 공통 병목으로 만든다는 것, inter-thread 불균형·intra-thread 경쟁이 간섭의 주 원인) 및 Eq.1 확률 모델과 관련 데이터베이스의 병렬 실행 지원 부족(Table 2)을 다룬다.
- **§4 BurstCBS Overview:** 세 컴포넌트(queue scaling, BIOS, vectorized cost estimator)의 역할을 요약한다.
- **§5 BurstCBS Design:** 5.1 큐 스케일링의 하드웨어 진화 과정과 two-tier memory pool, 5.2 BIOS 알고리즘과 fast recovery, 5.3 vectorized cost estimator와 misestimation 보정을 상세히 다룬다.
- **§6 Evaluation:** 8개 하위실험으로 load balancing, base-level latency/throughput, burst utilization, responsiveness, scalability, DB 애플리케이션(MySQL/RocksDB/YCSB), 프로덕션 RDS 사례를 검증한다.
- **§7 Discussion:** OS kernel과의 co-optimization(NVMe WRR), 자동화된 cost profiling, inter-server(VM 마이그레이션) 스케줄링을 향후 과제로 제시한다.
- **§8 Related Work:** 클라우드 스토리지 시스템(RDMA 기반 등)과 storage I/O 스케줄링(Gimbal 등) 계열 연구를 정리하며, BurstCBS는 client-side DPU에서의 burst·cost estimation 문제를 다룬다는 점에서 차별화된다고 밝힌다.
- **§9 Conclusion:** BurstCBS가 inter-thread load balancing과 intra-thread resource scheduling을 함께 달성하는 HW-SW 공동설계 시스템임을 재확인한다.

## 핵심 용어 (Key terms)
- **CBS (Cloud Block Storage)**: 클라우드에서 동적으로 생성/조정/삭제 가능한 가상화 블록 스토리지 볼륨 서비스.
- **xDPU**: Alibaba Cloud가 설계한 SoC로, wimpy CPU 코어·FPGA·NIC·DMA 엔진을 포함해 compute node의 infra 서비스를 오프로드.
- **SA (Storage Agent)**: xDPU에서 실행되며 user VM을 백엔드 스토리지에 연결하는 control plane(소프트웨어)/data plane(FPGA) 결합 컴포넌트.
- **Burstable VM/Credit-based burst**: base-level 처리량을 제공하되, 낮게 쓸 때 쌓인 크레딧으로 필요 시 그 이상 처리량을 일시 허용하는 모델.
- **BurstCBS**: 본 논문이 제안하는 xDPU 위 standalone I/O 스케줄링 시스템(queue scaling + BIOS + vectorized cost estimator).
- **BIOS (Burstable I/O Scheduler)**: 사용 이력 기반으로 dynamic rate limiter의 quota를 주기적으로 재계산해 base-level 보장과 burst를 동시에 지원하는 스케줄러.
- **WildCBS**: WRR(weighted round-robin) + per-tenant rate limiter를 결합한 work-conserving 스케줄러(현 프로덕션 주력, 주요 baseline).
- **BaseCBS**: Gimbal 변형으로, tenant별 정적 자원 상한을 둬 강한 성능 격리를 제공하지만 burst를 지원 못하는 baseline.
- **Two-tier memory pool**: 큐별 dedicated buffer pool + global shared pool로 구성되어, burst 시 buffer를 빌리고 반납하는 메모리 관리 기법.
- **Vectorized I/O cost estimator**: I/O 비용을 CPU time/ingress/egress/software limit의 4차원 벡터로 분리 추정하는 기법.
- **1:N egress-queue binding**: 하나의 ingress queue를 여러 SA control thread의 egress queue에 매핑해 FPGA 레벨에서 load balancing을 수행하는 메커니즘.

## 강점 · 한계 · 열린 질문
**강점**: 실제 Alibaba Cloud 프로덕션 클러스터의 정량 데이터로 문제를 규명(§2-3)하고, 프로덕션 xDPU에 standalone 패키지로 실배포·A/B 검증(§6.8 RDS)까지 완료한 강한 산업 실증. 알고리즘(BIOS)의 스케줄링 시간이 선형이면서도 1024 VM까지 100μs 이하로 유지되어 wimpy DPU 코어에서도 실용적.

**한계**: BIOS의 fast recovery는 목표 지연에 anchor된 delay-based adjustment에 의존하는데, 이는 heuristic 성격이 강해 이론적 fairness나 최적성 보장이 없다(저자도 §7에서 자동화된 cost profiling을 향후 과제로 언급). 평가가 특정 xDPU 세대(FPGA 기반, 8코어)에 특화되어 있어 다른 DPU 아키텍처(예: RISC-V DPA 기반 BlueField-3)로의 일반화는 검증되지 않음. 자원의 5%를 상시 shared pool로 예약해 burst utilization을 8%까지 희생하는 trade-off가 고정 파라미터로 남아있음.

**열린 질문**: (1) inter-server 스케줄링(§7에서 언급된 VM 마이그레이션)과 BurstCBS의 intra-node 스케줄링을 어떻게 통합할 것인가? (2) NVMe WRR 등 host OS kernel의 우선순위 힌트를 BIOS의 cost estimator/scheduler와 어떻게 co-design할 것인가? (3) FPGA 자원이 제한된 더 저가형 DPU에서도 1:N queue binding이 확장 가능한가?

## ❓ Q&A (자가 점검)
> [!question]- CBS에서 병목이 백엔드(SSD/스토리지 서버)가 아니라 compute node로 옮겨간 이유는?
> 프로덕션 클러스터에서 backend node는 storage I/O가 균등 분산되고 disk capacity 이용률(78%)에 비해 IOPS/BPS 이용률이 낮게 유지되는 반면(Figure 2, p.785), compute node의 SA는 burst 트래픽이 몰리면 static thread mapping과 FCFS 처리 방식 때문에 특정 스레드만 congested되고 전체 노드가 병목이 된다(p.784-786).

> [!question]- 1:1 binding과 1:N binding의 차이와 BurstCBS가 후자를 택한 이유는?
> 1:1은 ingress queue마다 하나의 egress queue를 두고 SA thread에 균등 배정하는 방식(Figure 10a)으로 정적이라 vCPU별 I/O 강도 편차에 대응 못한다. 1:N은 하나의 ingress queue를 여러 egress queue(스레드)에 매핑(Figure 10b)해 FPGA 레벨에서 실시간 부하 재분배가 가능하다(p.788).

> [!question]- 소프트웨어 기반 work stealing/centralized dispatcher를 쓰지 않은 이유는?
> 두 방식 모두 스레드 간 intensive messaging이 필요해 wimpy DPU 코어에서 상당한 오버헤드를 유발한다. DPDK lockless ring buffer 기반 work stealing 프로토타입은 실측 결과 35%의 처리량 손실을 보였다(Figure 11, p.788).

> [!question]- BIOS의 Algorithm 1에서 throttle된 I/O에 더 큰 가중치를 주는 이유는?
> throttle된 I/O는 follow-up I/O를 동반할 가능성이 높기 때문에, 가중치 $w_i$ 계산에서 $throttle_i \times weight_{throttle}$ 항을 추가로 반영해 향후 수요를 선반영한다(p.790).

> [!question]- I/O 비용을 스칼라가 아니라 벡터로 모델링해야 하는 근거는?
> 4KB write I/O는 128KB write I/O보다 바이트당 CPU 비용이 8배 높지만 바이트당 egress bandwidth 소모는 동일하다(p.791). 이처럼 I/O 타입마다 병목 자원이 다르므로 스칼라 비용(최댓값 기준)을 쓰면 다른 자원을 불필요하게 낭비해 utilization과 burst 여력이 줄어든다(Figure 16, 17).

> [!question]- BurstCBS가 WildCBS 대비 감수하는 trade-off는 무엇인가?
> fast recovery를 위해 전체 자원의 약 5%를 shared pool로 예약해두기 때문에, burst 시 최대 처리량이 WildCBS보다 5%–8% 낮다(Figure 23, p.794). 이는 base-level tenant 보호를 위한 설계상 의도된 손실이다.

> [!question]- 프로덕션 RDS 실험에서 관측된 실질적 개선 폭은?
> BurstCBS를 배포한 RDS 노드에서 8/16개 burst neighbor 상황에 대해 SQL 쿼리 평균 지연이 최대 83% 감소했고, 30분 프로덕션 트레이스 재생 실험에서는 WildCBS의 20-50ms 지연 스파이크가 BurstCBS에서 10ms 이하로 억제되었다(Figure 28, 29, p.795-796).

## 🔗 Connections
[[Infra]] · [[OSDI]] · [[2024]]
관련: [[LightPool - A NVMe-oF-based High-performance and Lightweight Storage Pool Architecture for Cloud-Native Distributed Database]] · [[Heimdall - Optimizing Storage I-O Admission with Extensive Machine Learning Pipeline]]

## References worth following
- J. Min, M. Liu, T. Chugh, C. Zhao, A. Wei, I. H. Doh, A. Krishnamurthy, "Gimbal: Enabling Multi-Tenant Storage Disaggregation on SmartNIC JBOFs," ACM SIGCOMM 2021 — BurstCBS가 BaseCBS의 원형으로 삼는 SmartNIC 기반 fair queuing 성능 격리 시스템, server-side 대비 client-side DPU 문제의 차이를 이해하는 데 핵심.
- T. Heo, D. Schatzberg, A. Newell, et al., "IOCost: Block IO Control for Containers in Datacenters," ACM ASPLOS 2022 — I/O cost estimation을 다루는 선행 연구로 vectorized cost estimator와 비교할 만한 접근.
- H. Li, D. S. Berger, L. Hsu, et al., "Pond: CXL-Based Memory Pooling Systems for Cloud Platforms," ACM ASPLOS 2023 — 클라우드 유저의 over-provisioning 성향에 대한 근거로 인용되며(참고문헌 [30]), 유휴 자원 재활용이라는 공통 모티프.
- J. Shu, R. Zhu, Y. Ma, et al., "Disaggregated RAID Storage in Modern Datacenters," ACM ASPLOS 2023 — 동일 1저자 그룹의 선행 disaggregated storage 연구로 CBS 아키텍처 배경 이해에 도움.
- K. Joshi, K. Yadav, P. Choudhary, "Enabling NVMe WRR support in Linux Block Layer," USENIX HotStorage 2017 — §7 Discussion에서 언급된 host OS kernel co-optimization(NVMe WRR)의 근거.

## Personal annotations
<!-- 본인 메모 영역 -->
