---
title: "eZNS: An Elastic Zoned Namespace for Commodity ZNS SSDs"
aliases: [eZNS]
description: "정적·경직된 ZNS 인터페이스 위에 v-zone을 두어 zone striping·자원 할당·I/O 스케줄링을 워크로드에 맞춰 동적으로 조정하는 elastic zoned namespace 계층"
venue: OSDI
year: 2023
tier: deep
status: done
tags:
  - paper
  - cluster/zns
  - topic/zns
  - topic/elastic
  - venue/osdi
  - year/2023
---

# eZNS: An Elastic Zoned Namespace for Commodity ZNS SSDs

> **OSDI 2023** · `cluster/zns` · Source: [eZNS - An Elastic Zoned Namespace for Commodity ZNS SSDs.pdf](<eZNS - An Elastic Zoned Namespace for Commodity ZNS SSDs.pdf>)

저자: Jaehong Min, Chenxingyu Zhao (University of Washington), Ming Liu (University of Wisconsin-Madison), Arvind Krishnamurthy (University of Washington)

## TL;DR
ZNS SSD는 host-side GC와 낮은 over-provisioning으로 비용 효율적이지만, zone 인터페이스가 **정적이고 경직(static and inflexible)**되어 있어 워크로드 런타임 동작에 적응하지 못하고 zone 간 I/O 간섭을 일으킨다. eZNS는 commodity ZNS SSD를 정밀하게 특성화한 뒤, NVMe 드라이버 위에 올라가는 device-agnostic 계층으로 **v-zone**(적응형 logical zone)을 노출한다. control plane의 **zone arbiter**(zone ballooning으로 active resource를 동적 재분배)와 data plane의 **tenant-cognizant I/O scheduler**(read congestion control + write admission control)를 결합해, 정적 인터페이스 대비 throughput 최대 17.7%, tail latency 최대 80.3% 개선한다(RocksDB 기준).

## 문제 & 동기
ZNS SSD는 (1) controllable GC를 host로 이양해 WAF와 over-provisioning을 줄이고, (2) sequential write만 허용해 zone 내 간섭을 줄이는 두 가지 이점을 준다. 그러나 zone이 일단 할당·초기화되면 최대 성능, I/O 구성, 그리고 cross-zone 간섭 대응이 모두 **고정**된다. 그 결과 애플리케이션은 (a) worst-case 기준으로 zone 자원을 under-provision 해 throughput이 낮거나, (b) zone을 over-provision 해 비용을 치르거나, (c) 예측 불가한 I/O latency를 겪는다. 실제로 RocksDB-over-ZenFS는 maintain하는 zone 중 일부에만 write를 발생시켜(Figure 2) 귀중한 active resource를 낭비한다. 또한 zone은 performance-isolated domain이 아니어서, co-located zone들이 비결정적으로 서로 간섭한다.

> [!quote]- 📄 원문 표현 (paper)
> "existing ZNS SSDs have a static zoned interface, making them in-adaptable to workload runtime behavior, unscalable to underlying hardware capabilities, and interfering with co-located zones." (p.1, Abstract)
>
> "the zoned interface is static and inflexible. After a zone is allocated and initialized, its maximum performance is fixed regardless of the underlying device capability, its I/O configurations cannot adapt to run-time workload characteristics, and cross-zone I/O interference yields unpredictable I/O executions." (p.3, §2.4)
>
> "a zone is not a performance-isolated domain, and one could observe considerable I/O interference for inter-zone read and write requests." (p.1, Abstract)

## 핵심 통찰

> [!note]- 통찰 1: ZNS의 "no internal bookkeeping" 특성이 곧 예측 가능성의 원천
> ZNS SSD에는 internal GC나 background housekeeping I/O가 없으므로, die가 congested되지 않는 한 read latency가 안정적이고 낮게 유지된다. 따라서 host는 **latency(gradient)를 직접 신호로 사용**해 die의 congestion 수준을 추론할 수 있다. 이는 conventional SSD에서는 불가능한, ZNS 고유의 성질을 스케줄링에 직접 활용하는 핵심 착안이다. (p.8 §3.5, p.10 §4.5.1)

> [!note]- 통찰 2: zone 인터페이스의 경직성은 striping·allocation·scheduling 세 축으로 분해된다
> 특성화 결과 세 가지 challenge로 정리된다 — (1) application-agnostic striping(최적 stripe size/width가 워크로드 I/O 프로파일에 의존), (2) device-agnostic placement(채널/die overlap으로 parallelism 손실), (3) tenant-agnostic scheduling(zone 간 isolation·fairness 부재). eZNS의 세 메커니즘(zone ballooning, serial allocator, I/O scheduler)이 각각 이 세 challenge에 대응한다. (p.6~p.9)

> [!note]- 통찰 3: active resource(active zone 수)는 희소 자원이며 가상 메모리처럼 풍선화(ballooning)할 수 있다
> ZNS SSD의 maximum active zone 수(MAR)는 NAND die 수에 비례하며 매우 제한적이다(testbed 256, large-zone SSD는 14). eZNS는 이를 essential/spare 두 그룹으로 나누고, idle namespace의 spare를 busy namespace로 빌려주는 VMware ESX 식 **memory ballooning** 비유를 적용한다. (p.9 §4.4, p.13 §5.1)

> [!note]- 통찰 4: HAL은 정확한 die-channel geometry 없이도 단 3개 하드웨어 contract만으로 동작 가능
> eZNS는 2차원 physical NAND geometry를 알 필요 없이 (1) MAR(active zone 최대 수, die 수에 비례), (2) NAND page size(stripe alignment 기준), (3) physical zone size 세 가지만으로 shadow device view를 만든다. 이는 vendor가 internal 구조를 공개하지 않는 commodity 환경에서 핵심적인 실용적 선택이다. (p.8~9 §4.2)

## 설계 / 메커니즘

> [!abstract]- 전체 아키텍처: zone arbiter(control plane) + I/O scheduler(data plane)
> eZNS는 NVMe 드라이버 위에 올라가 raw block access를 제공하며 **v-zone** 인터페이스를 노출한다(Figure 10). 두 주요 구성요소:
> - **Zone arbiter (control plane)**: HAL에서 device shadow view를 유지하고, serial zone allocation으로 overlapped placement를 회피하며, ballooning(harvesting)으로 zone 자원과 I/O 구성을 동적 스케일링.
> - **Tenant-cognizant I/O scheduler (data plane)**: delay-based congestion control로 read를 조정하고, token-based admission control로 write를 규제.
> SPDK framework에 thin layer로 구현되어 NVMe-over-RDMA disaggregated storage로 노출된다(p.10 §5).

> [!abstract]- Zone Striping & HAL hardware contract
> logical zone은 RAID 0처럼 여러 physical zone에 data block을 striping한다. 두 파라미터: **stripe size**(최소 data placement 단위)와 **stripe width**(active physical zone 수, write bandwidth 결정). 특성화 결과 stripe size는 NAND page size(예: 16KB)의 배수/약수일 때 효율 최대(Table 2). HAL은 3개 contract — MAR, NAND page size, physical zone size — 로 consistent shadow view(예: MAR 16, zone size 2GB)를 underlying device와 무관하게 제공. (p.5~6 §3.3, p.8~9 §4.2)

> [!abstract]- Serial Zone Allocator
> 세 가지 보장: (1) 각 stripe group은 firmware 내부 순서를 따르는 연속·serial physical zone들로 구성, (2) stripe group 내 die collision 없음, (3) stripe group 간에는 die collision 가능하나 모든 die가 fully populated될 때만 발생. allocator는 per-device request queue로 모든 logical zone의 OPEN command를 buffering하고 각 logical zone 요청을 atomic하게 serve. zone reservation 메커니즘(한 data block을 flush해 die를 zone에 binding)으로 high load에서도 write가 즉시 완료되게 함. concurrently opened logical zone의 interleaved allocation을 막아 channel-overlap을 회피. (p.9 §4.3)

> [!abstract]- Zone Ballooning: Local / Global Overdrive + Reclaim
> v-zone은 physical zone들을 하나 이상의 **stripe group**으로 나누며, 새 stripe group을 열 때마다 width를 동적으로 조정한다(Figure 11).
> - **Initial provisioning**: physical zone을 essential(SSD write bandwidth를 max out하는 최소 active zone, N_essential)과 spare(N_spare)로 분할.
> - **Local overdrive (zone expanding)**: namespace 내에서 자신의 spare를 끌어와 write 능력 강화. N_ActiveZoneHistory의 EWMA로 자원 강도를 추정해, active v-zone을 적게 열수록 더 많은 spare를 받음. width는 2의 거듭제곱으로 round-down. baseline stripe size 32KB에서 width가 2배 넓어지면 stripe size를 절반으로 줄여 I/O scheduler fairness 보조.
> - **Global overdrive (namespace expanding)**: arbiter가 inactive namespace(최근 N_essential allocation 동안 할당 이력 없음)의 spare를 active namespace로 재분배. inactive가 다시 active되면 recall.
> - **Reclaim (compaction)**: overdriven v-zone이 FINISH 없이 inactive되면 자원 누수를 막기 위해 zone compaction(기존 zone finish → 좁은 width로 새 stripe group open → data copy). GC-like overhead가 있으므로 두 cycle의 global overdrive 동안 write activity 없을 때만 lazy하게 background로 수행. (p.9~10 §4.4)

> [!abstract]- Zone I/O Scheduler (Algorithm 1)
> 두 구성요소가 협력:
> - **Congestion-avoid read scheduler**: per-namespace NVMe queue pair를 만들어 round-robin을 device로 offload. Swift-like delay-based congestion control로 stripe group마다 bandwidth 할당. read completion에서 io_lat이 lat_thresh(500us)보다 크면 cwnd를 multiplicative하게 줄이고, 아니면 additively 증가. cwnd 상한은 4×stripe_width로 제한해 software overhead 최소화. cwnd가 stripe_width로 설정되면 sequential read는 device per-die bandwidth로 빠르게 ramp up.
> - **Cache-aware write admission control**: write congestion은 write cache를 통해 모든 namespace에 global하게 발생하므로, global write latency를 monitor하고 token-based admission으로 규제. 1ms마다 token generator가 (now − last_refill)/block_admission_rate × stripe_width 만큼 token 보충. block_admission_rate는 정규화된 평균 latency 기반으로 조정해 모든 write zone의 latency를 균등화(self-clocking). (p.10~11 §4.5, Algorithm 1)

## 평가

> [!example]- 특성화(§3): commodity ZNS SSD 미시 벤치마크
> - Testbed: capacity 3.816GB(논문 Table 1; 본문에서는 40,704 physical zone × 96MB), 16 channels, 128 NAND dies, NAND page 16KB, physical zone 96MB, per-zone read B/W ~200MB/s, write B/W ~40MB/s, max active zones 256. (p.4 Table 1)
> - Stripe size 효과: per-die bandwidth는 16KB stripe size 이후 증가가 둔화. 4KB stripe(8-zone)는 8KB(4-zone)보다 read bandwidth 37.3% 높음. Table 2: 4KB→64KB에서 avg lat 64→314us, p99.9 76→619us, B/W 59→198MB/s. (p.6, Table 2)
> - Die overlap: 4KB random read / 128KB seq write에서 no-overlap 시 1,128MB/s·2,051MB/s, full overlap 시 bandwidth 47.2%·23.8% 감소, tail latency 87.1%·28.0% 증가. (p.7 §3.4.2)
> - Mixed I/O: write bandwidth 1,000MB/s일 때 conventional SSD의 p99.9 read/write latency가 ZNS보다 4.3×·2.8× 나쁨. (p.8 §3.5.1)

> [!example]- Zone Ballooning (§5.1)
> - 512KB I/O, QD=1, 4-writer 케이스: v-zone이 static logical zone 대비 **2.0×** throughput (4 static logical zone은 16 physical zone만 enable, v-zone은 width를 8로 overdrive해 32 physical zone 확장). 8/16-writer에서는 static과 동일한 physical zone 수 활용. (p.11 §5.1, Figure 12)
> - Global overdrive: 다른 세 namespace가 idle되면 NS4의 v-zone이 **3×** 더 많은 spare를 끌어와 write bandwidth를 ~2.3GB/s로 max out, 다른 zone이 write 재개 시 빠르게 release. (p.11 §5.1, Figure 13~14)

> [!example]- Zone I/O Fairness (§5.2)
> - Read-Read: CC off 시 high-QD zone(Zone B 1287MB/s)이 low-QD zone(Zone A 76MB/s)을 압도. CC on 시 두 zone 모두 ~290MB/s로 동일화. (p.12 §5.2, Figure 15a)
> - Write-Write: admission control로 regular zone에 per-thread bandwidth 35.7% 향상; collision-less에서는 ~7.2% 약간 감소 대신 전체 device bandwidth 24.7% 증가. (p.12 §5.2, Figure 15b)
> - Read-Write: CC+AC 시 zone A·B·C가 각 최대 bandwidth의 71.6%·57.5%·70.1% 유지(CC off 시 write zone은 19.3%·27.3%만). (p.12 §5.2, Figure 15c)

> [!example]- RocksDB / YCSB (§5.3) 및 Overhead (§5.4)
> - Single-tenant db_bench(readwhilewriting): eZNS가 16KB·4KB stripe static 대비 p99.9·p99.99 read latency 28.7%·11.3% 개선, throughput 11.5%·2.5% 향상. (p.12~13 §5.3, Figure 16)
> - Multi-tenant db_bench(2 overwrite + 2 randomread): read p99.9·p99.99 latency 71.1%·20.5% 감소, write/read throughput 7.5%·17.7% 향상. (p.13 §5.3, Figure 17~18)
> - YCSB(A/B/C/F): read-intensive(B,C) p99.9 latency 79.1%·80.3%, read-modify-write(F) 76.8% 개선; global overdrive로 write-most(A) throughput 최대 10.9% 향상. (p.13 §5.3, Figure 19~20)
> - Overhead: I/O depth 8까지 추가 latency 없음, 16 초과 시 scheduler delay로 최대 14.0% overhead. Memory footprint: 4-namespace×1TB에서 v-zone metadata 2MB, physical zone info 2.5MB로 conventional page-mapping 대비 negligible. (p.13~14 §5.4, Figure 21)

## 섹션 노트
- **§1 Introduction**: ZNS의 이점(WAF 해소, random write 제거, over-provisioning 최소화)과 한계(zone interface가 device 내부를 어떻게 노출하는지, cost/performance trade-off가 불명확)를 제시. eZNS = device-agnostic zoned namespace 계층, zone arbiter + tenant-cognizant scheduler.
- **§2 Background**: NAND SSD 구조(HIL, controller, DRAM, multi-channel), FTL 3대 기능(mapping, GC, wear-leveling)과 오버헤드(1TB당 1GB DRAM, WAF). ZNS는 OC SSD의 후계로 coarse-grained mapping·host-side reclaim·isolated I/O 제공. small-zone(96MB, 256 zone) vs large-zone(striping 고정, 14 active zone) 구분. zone 6-state.
- **§3 Characterization**: 5-layer system model(tenant→namespace→logical zone→physical zone→SSD ch/die), ZBD layer. 세 challenge(striping, placement, scheduling) 도출.
- **§4 eZNS Design**: overview, HAL hardware contract, serial allocator, zone ballooning(local/global overdrive, reclaim), I/O scheduler(read CC, write AC).
- **§5 Evaluation**: SPDK 구현, FIO 미시 벤치, RocksDB-over-ZenFS, YCSB.
- **§6 Related Work**: ZNS exploration(ZNS+, Gimbal, Bae et al., IODA), conventional SSD parallelism, OC SSD(SDF, LOCS, RAIL). eZNS as firmware 대안 논의(disaggregation에는 software 방식이 future-proof).
- **§7 Conclusion**: commodity ZNS 심층 분석 + v-zone 기반 elastic zoned view로 device capability max out 및 fair bandwidth share.

## 핵심 용어
- **ZNS (Zoned Namespace) SSD**: logical address space를 고정 크기 zone들로 나눠 sequential write·explicit reset을 강제하는 NVMe 인터페이스. device-side GC를 host로 이양.
- **physical zone**: 단일 die 위 하나 이상의 erase block으로 구성된 최소 zone 할당 단위(device-backed).
- **logical zone**: 여러 physical zone에 striping된 zone 영역(higher bandwidth).
- **v-zone**: eZNS가 노출하는 적응형 logical zone. 하나 이상의 stripe group으로 나뉘며 striping 구성과 hardware 자원을 런타임에 스케일.
- **stripe size / stripe width**: 각각 striping의 최소 data 단위와 active physical zone 수(write bandwidth 결정).
- **active resource / MAR**: 동시에 open·write 가능한 zone 수와 그 최대치(maximum active zones), die 수에 비례하는 희소 자원.
- **essential / spare group**: SSD write bandwidth를 max out하는 최소 active zone(essential)과 나머지(spare).
- **zone ballooning**: idle namespace의 spare를 busy namespace로 빌려주는 동적 자원 재분배(VM memory ballooning 비유).
- **local / global overdrive**: namespace 내(spare→v-zone) / namespace 간(arbiter가 inactive→active) 자원 확장.
- **reclaim**: overdriven v-zone이 release 없이 inactive될 때 spare를 회수하는 zone compaction.
- **HAL (hardware abstraction layer)**: 3개 contract(MAR, NAND page size, physical zone size)로 device shadow view 제공.
- **WAF (Write Amplification Factor)**: GC·wear-leveling이 유발하는 내부 write 증폭.

## 강점 · 한계 · 열린 질문
**강점**
- commodity ZNS SSD를 vendor 내부 명세 없이도 동작하는, device-agnostic·application-transparent 계층(ZenFS·RocksDB 무수정 호환).
- 특성화→설계 연결이 탄탄: 세 challenge가 세 메커니즘에 1:1로 매핑.
- ZNS 고유의 "no internal GC" 특성을 latency 기반 congestion 신호로 직접 활용한 점이 독창적.
- memory footprint(수 MB)와 I/O depth 8 이하 zero-overhead로 실용적.

**한계**
- single device·single commodity SSD(특정 vendor) 기반 — 다른 ZNS SSD geometry/정책에 대한 일반화는 contract 가정(round-robin wear-leveling)에 의존(저자도 "may not always be followed" 인정, p.8).
- reclaim(zone compaction)은 GC-like overhead를 다시 도입; ZNS가 없애려던 비용을 일부 재현.
- I/O depth 16 초과 시 최대 14.0% scheduler overhead.
- write admission이 global write cache 가정에 의존 — write cache 구조가 다른 device에서 모델 오차 가능.

**열린 질문**
- QLC ZNS(multi-pass programming으로 active zone 더 적음, p.16 §6)에서 ballooning 효과가 유지될까?
- Endurance Group(EG)/NVM Set로 physical isolation을 함께 노출하는 SSD가 나오면 software와 firmware 역할 분담이 어떻게 바뀔까?
- 여러 device에 걸친 elastic capacity scaling(저자가 software 방식의 미래 방향으로 제시)의 실제 구현 비용은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 기존 ZNS 인터페이스가 "static and inflexible"하다는 것은 구체적으로 무엇을 의미하는가?
> 답: zone이 일단 할당·초기화되면 (1) 최대 성능이 device capability와 무관하게 고정되고, (2) I/O 구성(stripe size/width)이 런타임 워크로드에 적응하지 못하며, (3) cross-zone 간섭에 대한 isolation/fairness가 없어 예측 불가한 실행을 낳는다. 애플리케이션은 worst-case로 under/over-provision 하거나 예측 불가 latency를 겪는다. (p.3 §2.4)

> [!question]- Q2. eZNS가 특성화로 도출한 세 가지 challenge와 각각의 대응 메커니즘은?
> 답: (1) application-agnostic striping → zone ballooning의 동적 stripe size/width 조정, (2) device-agnostic placement(채널/die overlap) → serial zone allocator(stripe group 내 die collision 없음), (3) tenant-agnostic scheduling → zone I/O scheduler(read congestion control + write admission control). (p.6~9)

> [!question]- Q3. zone ballooning의 local overdrive와 global overdrive는 어떻게 다른가?
> 답: local overdrive는 **한 namespace 내**에서 자신의 spare group을 끌어와 v-zone의 width를 넓힌다(N_ActiveZoneHistory EWMA로 강도 추정, width는 2의 거듭제곱). global overdrive는 **namespace 간**에서 arbiter가 inactive namespace(최근 N_essential 할당 동안 이력 없음)의 spare를 active namespace로 빌려주고, inactive가 재활성화되면 recall한다. (p.9~10 §4.4.2~4.4.3)

> [!question]- Q4. read scheduler가 ZNS에서 latency를 congestion 신호로 쓸 수 있는 이유는?
> 답: ZNS SSD는 internal GC나 background housekeeping I/O가 없어 bandwidth를 소비하는 숨은 device I/O가 없다. 따라서 die가 congested되지 않는 한 read latency가 안정적이고 낮으며, latency(gradient)가 곧 die congestion 수준을 직접 반영한다. Swift-like AIMD로 io_lat > 500us면 cwnd를 줄이고 아니면 늘린다. (p.10 §4.5.1, Algorithm 1)

> [!question]- Q5. write admission control이 read와 달리 global하게 동작하는 이유는?
> 답: write congestion은 모든 namespace가 공유하는 write cache를 통해 global하게 발생하기 때문이다(per-die local 제어로는 mitigate 불가). 따라서 global write latency를 monitor하고, token-based admission으로 1ms마다 (now−last_refill)/block_admission_rate × stripe_width 만큼 token을 보충해 모든 write zone의 latency를 균등화한다. (p.10~11 §4.5.2)

> [!question]- Q6. HAL이 정확한 die-channel geometry 없이 동작할 수 있는 근거는?
> 답: 세 hardware contract만 필요하다 — (1) MAR(active zone 최대 수, die 수에 비례), (2) NAND page size(stripe alignment, 예 16KB), (3) physical zone size. 이로부터 consistent MAR(예 16)과 zone size(예 2GB)를 가진 shadow device view를 만들어 underlying device와 무관하게 동작한다. wear-leveling이 round-robin이라는 contract는 항상 보장되진 않지만 chip 수와 wear-leveling 제약상 위반은 드물다. (p.8~9 §4.2)

> [!question]- Q7. 정적 인터페이스 대비 핵심 성능 개선 수치는?
> 답: RocksDB 기준 throughput 최대 17.7%, tail latency 최대 80.3% 개선. 세부적으로 single-tenant db_bench p99.9 read latency 28.7%↓, multi-tenant p99.9 71.1%↓, YCSB read-intensive p99.9 79.1~80.3%↓. zone ballooning은 4-writer에서 2.0× throughput, global overdrive는 idle 시 3× spare 확보. (p.1 Abstract, p.11~13 §5)

> [!question]- Q8. reclaim이 다시 GC-like overhead를 도입하는데, eZNS는 이를 어떻게 완화하는가?
> 답: reclaim은 overdriven v-zone이 FINISH 없이 inactive될 때만 필요하며, namespace가 두 cycle의 global overdrive 동안 write activity가 없을 때(read-only T_ReadOnly 윈도우 모니터링) lazy하게 background로 trigger되고 scheduler가 그 영향을 제한한다. 정상 동작에서는 reclaim을 피해 높은 성능을 유지한다. (p.10 §4.4.4)

## 🔗 Connections
[[ZNS]] · [[OSDI]] · [[2023]]

## References worth following
- ZNS+: Advanced Zoned Namespace Interface for Supporting In-Storage Zone Compaction (OSDI 2021) — zone 인터페이스에 copy-back 등 primitive를 추가한 선행 연구. [16]
- Gimbal: Enabling multi-tenant storage disaggregation on smartNIC JBOFs (SIGCOMM 2021) — 동일 그룹의 multi-tenant disaggregation·간섭 연구. [28]
- ZNS: Avoiding the Block Interface Tax for Flash-based SSDs (USENIX ATC 2021) — ZNS 인터페이스의 정의·ZenFS. [7,8]
- IODA: A Host/Device Co-Design for Strong Predictability Contract on Modern Flash Storage (SOSP 2021) — I/O determinism 기반 latency 예측성 대비점. [26]
- Bae et al., What You Can't Forget: Exploiting Parallelism for Zoned Namespaces (HotStorage 2022) — interference map 기반 zone parallelism. [3]
- Swift: Delay is Simple and Effective for Congestion Control in the Datacenter (SIGCOMM 2020) — read scheduler가 차용한 delay-based CC. [24]

## Personal annotations
<본인 메모 영역>
