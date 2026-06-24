---
title: "Getting the MOST out of your Storage Hierarchy with Mirror-Optimized Storage Tiering"
aliases: [Getting the MOST, MOST]
description: "소량의 hot data만 미러링해 tiering의 공간 효율과 mirroring의 부하 분산을 결합하는 storage tiering 기법 MOST와 그 구현 Cerberus"
venue: FAST
year: 2026
arxiv: "2512.03279"
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/storage-tiering
  - topic/mirroring
  - venue/fast
  - year/2026
---

# Getting the MOST out of your Storage Hierarchy with Mirror-Optimized Storage Tiering

> **FAST 2026** · cluster/fs · Source: [Getting the MOST out of your Storage Hierarchy with Mirror-Optimized Storage Tiering.pdf](<Getting the MOST out of your Storage Hierarchy with Mirror-Optimized Storage Tiering.pdf>)

**저자:** Kaiwei Tu, Kan Wu (Google), Andrea C. Arpaci-Dusseau, Remzi H. Arpaci-Dusseau — University of Wisconsin–Madison, †Google

## TL;DR
MOST(Mirror-Optimized Storage Tiering)는 고전 tiering에 **소량의 hot data를 tier 간에 mirror**하는 것을 더해, tiering의 공간 효율은 유지하면서 mirroring의 부하 분산 이점을 얻는 storage 관리 기법이다. mirror된 데이터에 대한 읽기를 두 device로 동적 라우팅(`offloadRatio`)하여 load balancing을 비싼 data migration 없이 수행한다. CacheLib 기반 구현 Cerberus는 static workload에서 기존 기법 대비 최대 2.34배 throughput, P99 latency 75% 감소를, 4개 production workload에서 최대 1.86배 throughput, 90% GET latency 감소, 동적 workload에서 Colloid 대비 최대 84% write 감소를 달성한다.

## 문제 & 동기
현대 storage hierarchy는 성능/용량 프로파일이 겹치는(overlapping) 새 device들(NVM, low-latency SSD, NVMe Flash SSD, remote storage over NVMeoF, disaggregated SSD, EBS)로 구성되어 strict hierarchy로 배치하기 어렵다. 기존 기법들은 모두 단점이 있다 — single-copy 기법(striping, tiering, exclusive caching)은 부하 분산을 위해 data migration에 의존해 동적 workload에 느리고 write가 과다하며, multiple-copy 기법(mirroring, inclusive/non-hierarchical caching)은 capacity 활용도가 낮고 write-intensive workload에 약하다.

> [!quote]- 핵심 인용 (p.1, p.2)
> "MOST dynamically mirrors a small amount of hot data across storage tiers to efficiently balance load, avoiding costly migrations." (p.1)
>
> "with emerging technologies such as non-volatile memory, low-latency SSDs, NVMe Flash SSDs, SATA Flash SSDs, and remote storage with NVMeoF, disaggregated SSDs, and EBS, the performance and capacity devices have overlapping characteristics. As such, arranging these devices into a strict hierarchy limits their potential." (p.2)

MOST가 mirroring을 추가하면 좋은 세 가지 이유(p.1): (1) mirror는 라우팅 조정만으로 workload 변화에 빠르게 반응(pure tiering은 느린 migration 필요), (2) 동적 workload에서 device write를 줄임(migration은 광범위한 write 유발), (3) device 성능 fluctuation에 robust(불필요한 migration으로 인한 overreaction 방지).

## 핵심 통찰
핵심은 **load balancing 수단으로 migration 대신 라우팅을 쓰는 것**이다. 가장 hot한 소량의 데이터만 양쪽 device에 복제(mirrored class)해 두면, 부하에 따라 그 데이터의 읽기를 동적으로 양 device에 분배할 수 있다. 나머지 데이터는 단일 복사본(tiered class)으로 두어 공간 효율을 지킨다.

> [!quote]- 통찰 인용 (p.4)
> "MOST uses a hybrid layout with two classes: the mirrored class and the tiered class. Data in the mirrored class is replicated across two devices to enable fast load balancing; data in the tiered class is stored as a single copy to maximize space efficiency." (p.4)

storage hierarchy가 memory hierarchy와 다른 점(p.4): 더 큰 dataset(migration 수렴 시간 증가), 낮은 write bandwidth, read/write interference, device endurance(잦은 migration이 수명 단축), software-based indirection. 따라서 storage에서는 migration에 덜 의존하는 기법이 필요하다.

## 설계 / 메커니즘
**Data Layout (Figure 1).** 데이터를 두 class로 구분: mirrored class(hot 데이터, 양 device 복제)와 tiered class(warm은 performance device, cold는 capacity device에 단일 복사본). hot data를 mirror에 두면 요청이 performance device에서 처리될 확률을 최대화한다. 고부하 시 mirrored class를 키우고 일부 요청을 capacity 복사본으로 라우팅.

**MOST Optimizer (Algorithm 1, p.5).** `offloadRatio`라는 변수로 load 분배·write traffic·allocation을 제어. 200ms 주기로 양 device의 end-to-end latency를 측정해 feedback 기반으로 조정:
- `L_P > (1+θ)·L_C` (performance가 더 느림): offload를 늘림(capacity로 더 보냄). mirrored class를 max까지 키우거나 hotness 개선, `offloadRatio += ratioStep`.
- `L_P < (1-θ)·L_C` (capacity가 더 느림): `offloadRatio -= ratioStep`, performance로만 migrate.
- 두 latency 근사 시: 모든 migration 중단.
- 파라미터: θ=0.05, ratioStep=0.02, EWMA로 latency 평활화(Linux block-layer counter 사용).

**Balancing Reads (§3.2.1).** mirror된 hot data의 읽기를 확률 `offloadRatio`로 capacity device에, 나머지는 performance device로 라우팅(probability-based routing).

**Mirror-Class Migration (§3.2.3).** segment 단위로 read/write counter로 hot/warm/cold 식별(HeMem 유사). mirrored class는 capacity의 최대 20%까지 확장. tiered class의 가장 hot한 segment를 mirrored class로 복제; mirror가 max에 도달하면 cold segment와 swap. system capacity의 watermark(2.5%) 이하로 떨어지면 reclamation. **bidirectional migration**: 항상 latency가 더 높은 device로부터 *멀어지는* 방향으로 migrate.

**Dynamic Write Allocation (§3.2.2).** storage를 2MB segment로 분할, in-memory 메타데이터로 추적. 새로 쓰는 데이터는 `offloadRatio` 확률로 capacity에 배치(performance 고부하 시 capacity로 더 많이).

**Write Balancing & Subpage tracking (§3.2.4).** mirror에 write 시 한 복사본만 갱신하고 4KB subpage 단위로 valid/invalid 추적(clean/invalid-on-capacity/invalid 3상태, 2비트/subpage). 2TB hierarchy 50% mirroring 극단 경우에도 메타데이터 overhead는 128MB로 무시할 수준. **Selective cleaning**: rewrite distance가 큰 block만 background로 cleaning(rewrite distance 작으면 곧 다시 쓰여 cleaning 무의미).

**Tail Latency Protection (§3.2.5).** 사용자가 최대 `offloadRatio`를 설정해 capacity로의 offload를 제한, hot data의 tail latency 보호.

**Cerberus 구현 (§3.3).** CacheLib(DRAM cache + flash cache + storage management layer)의 storage management layer에 MOST를 통합한 user-level system. segment 76 bytes 메타데이터(Table 3), 2MB segment(메타데이터 footprint와 공간 낭비 균형). HeMem tiering 로직을 확장한 약 1.5k LOC 추가.

> [!quote]- 메커니즘 인용 (p.5, p.6)
> "MOST routes an incoming read with probability offloadRatio to the capacity device, and otherwise, to the performance device. OffloadRatio is the key variable controlling load distribution" (p.5)
>
> "if the write updates only one copy, then future reads must be directed only to that clean, valid copy. In order to perform load-balancing of writes, MOST updates only one copy and carefully tracks which portions of each segment are valid." (p.6)

## 평가
**환경:** 40-core Intel Xeon Gold 5218R, 64GB DRAM, Ubuntu 20.04. 두 hierarchy — Optane(750GB P4800X)/NVMe(1TB 960), NVMe/SATA(1TB 870). 비교 대상: Striping, Orthus(caching), HeMem(tiering), BATMAN, Colloid/Colloid+/Colloid++.

- **Static workloads (§4.1, Figure 4):** Cerberus는 모든 기법과 동등하거나 우수. Orthus는 Cerberus와 유사 throughput을 내지만 **690GB를 mirror(Cerberus는 50GB)** (p.7). intensity 2.0×에서 Colloid/Colloid+/Colloid++는 read-only 시 134/122/62GB migrate(Cerberus는 read-only mirror 0GB지만 표기상 134GB total), write-only 시 0/93/80GB; Colloid·Colloid++는 Cerberus 대비 2.68×·1.24× 더 많은 migration traffic (p.7).
- **Dynamic workloads (§4.2, Figure 5):** 15분마다 2분 burst. Colloid++는 read/write/RW에서 performance tier에 282/214/260GB, capacity tier에 262/186/239GB migrate; **Cerberus는 capacity tier에 87/107/64GB만 migrate** (p.8). burst 중 Cerberus는 HeMem 대비 read-only 1.53×, write-only 1.48× throughput (p.8). Cerberus는 부하 변화에 **10초 미만** 적응, Colloid는 100MB/s migration 제한 시 800초 초과 소요 (p.9, Figure 6).
- **Endurance (§4.2, p.9):** Colloid는 read-only workload에서 performance에 평균 252GB·capacity에 229GB write, Cerberus는 평균 86GB만 mirror. 하루 운영 시 Colloid의 migration write로 capacity-tier device 수명이 3.0년→129일(88% 감소), Cerberus는 6.6/3.1 DWPD로 performance 5.0년·capacity 4.1년(18% 감소) (p.9).
- **In-depth (§4.3, Figure 7):** working set이 system capacity의 95%에 도달해도 Cerberus는 전체의 **1.8%만 mirror** (p.11). subpage 사용 시 부하 급감에 즉시 적응(미사용 시 2MB segment 전체 migrate 필요). selective cleaning은 non-selective 대비 cleaning 25% 감소, clean block은 5%만 증가 (p.11).
- **CacheLib (§4.4):** static에서 striping/Orthus/HeMem 대비 최대 1.34× (Optane/NVMe), 1.54× (NVMe/SATA) throughput (p.11). production workload 4종(Meta)에서 Colloid 대비 평균 1.24× (Optane/NVMe), 1.17× (NVMe/SATA) throughput; **평균 latency 14%, P99 latency 19% 감소** (Table 5, p.12). YCSB에서 striping 대비 최대 1.43× throughput, 30% 적은 P99 latency (p.12). CPU overhead는 4KB random workload(256 threads)에서 Colloid++ 대비 0~1.5% 증가 (p.11).

> [!quote]- 평가 인용 (p.7, p.9)
> "Orthus achieves this by mirroring 690GB of data compared to only 50GB in Cerberus." (p.7)
>
> "Colloid introduces 3.1 DWPD of migration writes, shortening the lifespan from 3.0 years to 129 days, an 88% reduction." (p.9)

## 섹션 노트
- **§1 Introduction:** single-copy(striping/tiering) vs multiple-copy(caching/mirroring) 분류와 한계, MOST 제안.
- **§2 Motivation & Background:** §2.1 multi-device hierarchy, Table 1 device 성능(예: Optane/PCIe3.0 NVMe 4KB read bandwidth ratio 1.5:1, 4KB read 시 2.2:1까지 증가), §2.2 기존 기법(Table 2 정성 비교), §2.3 storage vs memory hierarchy 차이.
- **§3 MOST:** 세 design goal(최대 bandwidth 활용, 동적 workload 빠른 반응, device·workload 독립성), basic design, design details(Algorithm 1), Cerberus 구현.
- **§4 Evaluation:** §4.1 기존 기법 비교, §4.2 동적 workload, §4.3 in-depth, §4.4 CacheLib(production, YCSB).
- **§5 Discussion:** multi-tier 확장, consistency(WAL), performance isolation(QoS) 모두 future work.
- **§6 Related Work / §7 Conclusion:** "unified theory of storage hierarchy management"으로의 방향 제시.

## 핵심 용어
- **MOST (Mirror-Optimized Storage Tiering):** tiering + 소량 mirroring을 결합한 기법.
- **Mirrored class / Tiered class:** hot 데이터를 양 device 복제하는 부분 / 단일 복사본으로 두는 부분(warm→performance, cold→capacity).
- **offloadRatio:** mirror 읽기를 capacity device로 보내는 확률이자 load 분배의 핵심 제어 변수.
- **Cerberus:** CacheLib에 MOST를 통합한 user-level storage management system.
- **Subpage tracking:** 4KB 단위 valid/invalid 추적으로 write도 load balancing.
- **Rewrite distance:** 한 block의 두 write 사이 평균 read 수; selective cleaning 판단 기준.
- **DWPD (Drive Writes Per Day):** device endurance 지표.
- **Orthus / Colloid / HeMem / BATMAN:** 비교 대상 — non-hierarchical caching / access-latency 기반 tiering / classic tiering / static-ratio tiering.

## 강점 · 한계 · 열린 질문
**강점:**
- migration 없이 라우팅으로 부하 분산 → 동적 workload에 빠르게(10초 미만) 적응하고 write를 대폭 줄여 device 수명 보호.
- 소량(1.8~20%) mirror만으로 mirroring의 이점 획득, 공간 효율 유지.
- prior workload 지식 불필요(self-adjusting), 다양한 hierarchy에 적용 가능. 실제 production workload·YCSB·CacheLib 통합으로 실용성 입증.

**한계 (논문 명시 — §5 모두 future work):**
- 현재 2-tier에 집중; multi-tier 확장은 더 정교한 optimization policy 필요.
- consistency 보장 미흡(WAL 기반 강화는 future work).
- performance isolation/QoS 미지원(tenant 인지 불가, block 단위 관리).

**열린 질문:**
- 3개 이상 tier로 확장 시 mirroring 조합과 라우팅 정책의 복잡도/이득은?
- mirror class 크기(20%), watermark(2.5%) 등 파라미터가 workload 다양성에 얼마나 robust한가?
- write-heavy + 빈번한 invalidation workload에서 subpage 메타데이터/cleaning 비용의 worst case는?

## ❓ Q&A (자가 점검)
> [!question]- Q1. MOST가 tiering·mirroring과 다른 핵심은?
> 전체가 아닌 *소량의 hot data만* mirror하고, 그 mirror 데이터의 읽기를 동적으로 라우팅해 부하를 분산한다. tiering의 공간 효율과 mirroring의 부하 분산을 동시에 얻는다.

> [!question]- Q2. load balancing을 migration 없이 어떻게 하나?
> mirror된 데이터의 읽기를 `offloadRatio` 확률로 capacity device에 보내는 probability-based routing으로 한다(data를 옮기지 않음).

> [!question]- Q3. `offloadRatio`는 어떻게 조정되나?
> Optimizer가 200ms마다 양 device의 end-to-end latency를 측정, performance가 더 느리면 늘리고 capacity가 더 느리면 줄이고 비슷하면 migration을 멈춘다(θ=0.05, ratioStep=0.02, EWMA 평활화).

> [!question]- Q4. mirror에 write가 들어오면?
> 한 복사본만 갱신하고 4KB subpage 단위로 valid 여부를 추적해, write도 load balancing 가능하게 한다.

> [!question]- Q5. storage hierarchy가 memory와 달리 migration에 덜 의존해야 하는 이유는?
> dataset이 커 migration 수렴이 느리고, write bandwidth가 낮으며 read/write interference·device wear가 있어 migration의 비용이 크기 때문.

> [!question]- Q6. Cerberus가 Colloid 대비 갖는 endurance 이점은?
> read-only workload에서 Colloid가 migration으로 capacity device 수명을 88%(3.0년→129일) 줄이는 반면 Cerberus는 18% 감소에 그친다.

> [!question]- Q7. mirror class는 얼마나 작은가?
> 설정 최대 20%, 실제로는 working set이 capacity의 95%여도 전체의 1.8%만 mirror한다.

> [!question]- Q8. selective cleaning은 무엇을 거르나?
> rewrite distance가 작은(곧 다시 쓰일) block은 cleaning하지 않고, 오래 안 쓰일 block만 cleaning해 불필요한 cleaning을 25% 줄인다.

## 🔗 Connections
[[File System]] · [[FAST]] · [[2026]]

## References worth following
- **[64] Colloid — Vuppalapati & Agarwal, "Tiered memory management: Access latency is the key!" (SOSP 2024):** MOST의 주 비교 대상, latency 기반 tiering.
- **[69] Orthus — Wu et al., "The Storage Hierarchy is Not a Hierarchy" (FAST 2021):** non-hierarchical caching, probability-based routing의 기반.
- **[56] HeMem — Raybuck et al. (SOSP 2021):** Cerberus의 tiering·migration 로직 기반.
- **[66] AutoRAID — Wilkes et al. (ACM TOCS 1996):** mirroring + RAID-5의 고전, hybrid 관리의 선구.
- **[68] Wu et al., "Towards an unwritten contract of Intel Optane SSD" (HotStorage 2019):** device read/write interference 근거.
- **[18] Berg et al., CacheLib (OSDI 2020):** Cerberus 구현 기반 라이브러리.
- **[23] BATMAN (MEMSYS 2017):** static-ratio tiering 비교 대상.

## Personal annotations
<본인 메모 영역>
