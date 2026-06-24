---
title: "Zebra: Efficient Redundant Array of Zoned Namespace SSDs Enabled by Zone Random Write Area (ZRWA)"
aliases: [Zebra]
description: "ZNS SSD RAID에서 PPU(부분 패리티 업데이트)를 ZRWA의 in-place 덮어쓰기로 처리해 metadata zone과 GC를 제거한 RAID 아키텍처."
venue: HPCA
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/zns
  - topic/zns
  - topic/raid
  - topic/zrwa
  - venue/hpca
  - year/2025
---

# Zebra: Efficient Redundant Array of Zoned Namespace SSDs Enabled by Zone Random Write Area (ZRWA)

> **HPCA 2025** · `cluster/zns` · Source: [Zebra - Efficient Redundant Array of Zoned Namespace SSDs Enabled by Zone Random Write Area (ZRWA).pdf](<Zebra - Efficient Redundant Array of Zoned Namespace SSDs Enabled by Zone Random Write Area (ZRWA).pdf>)

저자: Tianyang Jiang (Huawei), Guangyan Zhang† (Tsinghua University, BNRist), Xiaojian Liao (Beihang University), Yuqi Zhou (China University of Geosciences). †교신저자.

## TL;DR
ZNS SSD를 RAID로 묶을 때 가장 큰 골칫거리는 **PPU(Partial Parity Update)** 다. 부분 stripe 쓰기마다 패리티를 read-modify-write 해야 하는데, ZNS의 append-only(no-overwrite) 특성 때문에 패리티를 제자리에서 갱신할 수 없다. 기존 RAIZN은 PPU를 전용 **metadata zone**에 append하지만, 이 metadata zone이 병목(throughput 최대 26~59% 하락)과 RAID-level GC, log header 쓰기 증폭(WAR 2.98)을 유발한다. Zebra는 현대 ZNS SSD가 노출하는 **ZRWA(Zone Random Write Area, 쓰기 버퍼 영역)** 를 활용한다. chunk 크기를 ZRWA window 이내로 잡으면 패리티의 반복 덮어쓰기(PPU)가 ZRWA 안에서 일어나 ZNS의 append-only 의미를 유지하면서도 **in-place 패리티 갱신**이 가능해진다. 그 결과 metadata zone과 GC가 완전히 사라진다. RAIZN 대비 처리량 1.1~4.2x, RocksDB 최대 5.4x 향상.

## 문제 & 동기
- ZNS SSD는 device-level GC를 없애고 zone abstraction(append-only write)으로 예측 가능한 성능을 준다. 그러나 신뢰성/처리량을 위해 RAID로 묶으려 하면 문제가 생긴다.
- RAID의 부분 stripe 쓰기는 patial parity update(PPU)를 요구하는데(데이터센터 block-level 쓰기의 75%가 full stripe보다 작음), ZNS는 패리티를 in-place로 덮어쓸 수 없어 이를 처리하기 곤란하다.
- RAIZN은 PPU를 per-ZNS **metadata zone**에 append해 해결하지만, 본 논문 실측 결과 metadata zone이 (1) multi-zone PPU aggregation 경합, (2) RAID-level GC, (3) log header 쓰기 증폭으로 병목이 된다. PPU 비율이 높을수록 RAIZN throughput 최대 59% 하락.
- 핵심 통찰: 현대 SSD의 RAM 기반 write buffer를 노출하는 ZRWA는 **반복 sequential overwrite** 패턴에 이상적이며, PPU가 바로 그 패턴이다.

> [!quote]- 📄 원문 표현 (paper)
> "However, integrating ZNS SSDs into RAID configurations presents challenges, particularly concerning partial parity updates (PPUs) due to the no-overwrite property of ZNS SSDs. Existing solutions introduce dedicated metadata zones to manage PPUs, but these zones become performance bottlenecks, especially under high PPU workloads." (p.1, Abstract)
>
> "Our key insight is that the SSD's write buffer (or ZRWA) in front of each zone is ideal for PPUs that exhibit a repeated sequential overwrite I/O pattern. By ensuring that the parity chunk fits within the ZRWA window, Zebra enables efficient PPUs within the ZRWA." (p.1, Abstract)
>
> "We find that the metadata zone, similar to the file system journaling, suffers from the contention of multi-zone PPU aggregation, the RAID-level GC used to reclaim obsolete PPUs, and the write amplification caused by PPU log headers." (p.1, §I)

## 핵심 통찰
> [!note]- 토글: 왜 ZRWA가 PPU에 맞는가
> - **ZRWA(Zone Random Write Area)** 는 NVMe 명세(2024)와 상용 ZNS SSD가 지원하는, zone 앞단의 고정 크기 연속 LBA 영역이다. 이 영역 안에서는 random/concurrent write와 **overwrite(덮어쓰기)** 가 허용되며, ZRWA window를 벗어나는 쓰기가 들어오면 앞쪽 데이터가 flash로 commit되면서 window가 전진한다(implicit commit).
> - 패리티 chunk는 같은 stripe가 채워질 때까지 계속 갱신(덮어쓰기)되며, 그 목적지는 항상 동일한 패리티 chunk다 → "repeated sequential overwrite"라는 ZRWA에 완벽히 맞는 패턴.
> - chunk 크기를 ZRWA size(상용 SSD에서 보통 1MB~64MB) 이내로 설정하면, 한 chunk 분량의 PPU 전체가 ZRWA 안에 머물러 flash로 조기 commit되지 않고 in-place 갱신 가능.
> - 각 physical zone이 **private ZRWA** 를 가지므로 PPU를 독립적으로 처리 → 중앙집중식 metadata zone과 GC가 불필요.
> - 데이터와 패리티가 같은 zone에 공존하는 **all-to-all** 설계가 가능해져, RAIZN의 'all-to-one'(모든 data zone이 한 metadata zone에 쓰는) 병목을 제거.

## 설계 / 메커니즘
> [!note]- 토글: Zebra 아키텍처와 메커니즘
> **전체 구조 (Fig.6)**: Zebra는 여러 physical ZNS SSD의 zone을 묶어 하나의 logical zone을 만들고, data/parity chunk를 모든 ZNS SSD에 균등 분산한다. 모든 physical zone은 ZRWA mode로 open되며, data chunk와 parity chunk가 같은 zone에 공존한다. RAID 5/6 지원(논문은 RAID 5, (k+m)=(3+1) 예시).
>
> **Host-side 자료구조**: active logical zone의 WP/상태를 host memory에 보관하고, **in-memory stripe cache** 로 active stripe를 캐싱해 PPU 계산용 read 요청과 RAID write의 의존성을 줄인다.
>
> **Partial stripe append 처리 (Fig.7)**: (3+1) 예시에서 5개 쓰기를 순차 처리할 때, 데이터 chunk에 append하면서 패리티 chunk를 ZRWA 안에서 덮어써 부분 패리티를 갱신한다. 패리티의 I/O 패턴이 곧 repeated sequential overwrite. stripe가 가득 차면 옛 패리티가 immutable이 되고 패리티 chunk가 다음 window로 전진.
>
> **Real WP 문제와 lightweight metadata (§III-B, Fig.8)**: ZRWA zone은 WP가 ZRWA 시작점을 가리켜, 실제로 어디까지 유효 데이터가 쓰였는지(=**Real WP**) 알 수 없다. 게다가 ZRWA는 ZRWAFG(flush granularity, flash page 단위, 보통 16KB+) 단위로 이동해 coarse-grained. Zebra는 NVMe SSD의 **OOB(Out-Of-Band) 영역**(LBA당 8~64B spare)에 metadata를 써서 해결: 초기엔 OOB가 0으로 채워지고, 쓰기가 발생한 LBA의 OOB에 1을 기록. 복구 시 OOB를 traverse해 연속된 prefix의 끝을 Real WP로 복원. 비용은 **LBA당 1 bit**(패리티 chunk는 disk+machine 동시 실패 대비 LBA당 1 Byte, 이론상 ⌈log2(stripe_size/chunk_size)⌉ bit면 충분).
>
> **Fault Tolerance (§III-C)**:
> - *Disk-failure recovery*: 실패한 ZNS SSD로 가던 read는 다른 stripe와 패리티로 reconstructed read. 새 SSD가 들어오면 모든 stripe를 읽어 실패 SSD의 패리티/데이터 재계산 후 기록.
> - *Machine-failure recovery* (전원 손실): in-memory stripe cache와 logical zone WP가 소실됨. 3단계로 복구 — (1) zns-zone-management-recv로 physical zone 상태/WP 조회, (2) OOB scan으로 각 physical zone의 Real WP를 찾아 logical zone의 Real WP 동기화, (3) Real WP 너머의 invalid data를 0으로 덮어써 폐기(append-only 의미 유지) 후 패리티 재계산.
> - *Zero storage space reduction after recovery (Fig.9)*: RAIZN은 부분 기록된 stripe0를 no-overwrite 제약 때문에 재사용 못 하고 다음 stripe로 건너뛰어 영구적 공간 손실 + mapping table 메모리 오버헤드 발생. Zebra는 ZRWA의 in-place overwrite로 stripe0를 그대로 재사용 → 공간 손실 0.
>
> **구현 / 가속 (§III-D, §IV-A)**: SPDK 위 user-space zone-interface 모듈, C++ ~5.2K LoC. 패리티 계산(XOR/다항식)과 stripe cache memory copy를 **AVX** 명령으로 가속(memory copy/parity 계산 latency가 각각 11.6%/5.6% 차지, AVX로 6.1%/10% 절감). flush 명령은 PLP 지원 SSD에선 write 완료 즉시 durable로 처리.

## 평가
> [!note]- 토글: 실험 결과 (수치 + p.X)
> **셋업 (§IV-A, p.10~11)**: 비교 대상 RAIZN, 그리고 GC 미발생+zone당 metadata zone 할당한 비현실적 상한 RAIZN+. large-zone은 WD ZN540 2TB 4개로 (3+1) RAID 5, chunk 64KB. small-zone은 NVMeVirt로 PM1731a 모사한 7개로 (6+1) RAID 5. 서버: 2×10-core CPU, 64GB DRAM, Ubuntu 22.04, kernel 5.15.0, SPDK v22.09.
>
> **Large-zone write (Fig.10~11, p.12)**:
> - 1 open zone에서 4KB write throughput RAIZN 대비 +30%. open zone 증가 시 +8~31%. Zebra는 5 open zone으로 peak(1178 MB/s) 도달, RAIZN은 9개 필요. RAIZN은 14개 중 11개만 data로 사용(3개는 metadata), RAIZN+는 7개만 동시 open 가능.
> - request 크기 클 때 8KB/16KB/32KB에서 +34%/+17%/+14%. 192KB 이상은 PPU 소멸로 셋 다 물리 상한(~3400 MB/s) 수렴.
> - average latency RAIZN 대비 4KB/8KB에서 19%/20% 절감.
>
> **Small-zone write (Fig.12, p.12)**: zone이 channel 대역폭에 묶이는 small-zone에서 Zebra throughput이 open zone 증가에 따라 선형 증가, RAIZN은 metadata zone에 묶임. 4KB/8KB/16KB write 최대 **4.2x/3.9x/3.3x** 향상.
>
> **Read (Fig.10)**: sequential/random read 모두 RAIZN과 comparable(>64KB에서 수렴).
>
> **Mixed (Fig.13)**: large-zone에서 동일 read throughput 시 write throughput이 항상 더 높음. small-zone write-dominant에서 평균 2.2x.
>
> **Real-world traces (Fig.14, p.13)**: large-request(YCSB-A/B)는 full-stripe 위주라 비슷, 나머지 4개(평균 request ≤32KB)는 write throughput large-zone 1.3x / small-zone 3.8x.
>
> **RocksDB (Fig.15)**: fillsync에서 RAIZN 대비 large-zone 24~42%, small-zone 4.5~5.4x throughput 향상. 4개 db_bench workload 평균 throughput 17.7% 향상, average latency 최대 10% 절감.
>
> **Recovery (Fig.17b, §IV-E, p.11~12)**: machine-failure 복구 시간 RAIZN 대비 **14.2x** 단축(RAIZN은 복구 시간의 93%를 metadata zone traverse에 소모). disk-failure 복구는 1000GB 기준 899초, degraded read 64KB/128KB에서 6.1/9.2 GB/s.
>
> **RAID 6 (Fig.17c)**: (5+2) small-zone에서 <64KB write 평균 4.7x.
>
> **민감도**: chunk size는 64KB가 최적(4KB는 IOPS 한계로 평균 29% 손실, 16KB는 약간 빠르나 latency 58% 증가). in-memory stripe cache 메모리 = max_active_zone_count × stripe_size(실험 3.5MB).
>
> **ZapRAID 비교 (Fig.19)**: ZapRAID가 batching으로 write throughput 평균 24% 높지만, Zebra가 latency 4.1~7.8x 낮음(ZapRAID는 배칭 큐잉 지연).

## 섹션 노트
- **§I Introduction**: ZNS RAID의 PPU 문제 제기, RAIZN의 metadata zone 병목(최대 26% throughput 하락) 실측, ZRWA 활용 아이디어 제시.
- **§II Background & Motivation**: ZNS interface(WP, zone 상태), ZNS RAID(stripe/chunk, k+m), RAIZN 상세 분석 — (1) multi-zone PPU aggregation 경합(metadata-data 대역폭 격차 ~185MB/s, PM1731a는 76% 하락), (2) GC(throughput 15% 하락, P99 26→43µs), (3) log header WAR 2.98. ZRWA 정의(Table I: ZN540 ZRWA 1MB/ZRWAFG 16KB 등).
- **§III Design**: 아키텍처 개요, lightweight metadata로 Real WP 찾기, fault tolerance(disk/machine), 구현 디테일(AVX, flush).
- **§IV Evaluation**: micro/application/recovery/individual technique/overhead/RAID6/chunk size 민감도/타 시스템 비교.
- **§V Related Work**: ZNS storage(GC 최적화 vs ZNS feature 활용), SSD RAID(block-interface, Log-RAID). BIZA(ZRWA로 data+parity 캐시)와 달리 Zebra는 ZRWA 도입 시 crash consistency 문제 해결.
- **§VI Conclusion**: ZRWA 기반 all-ZNS RAID로 PPU 해결 + crash-consistent WP + 빠른 복구.

## 핵심 용어
- **ZNS (Zoned Namespace) SSD**: zone abstraction으로 append-only write만 허용해 device-level GC를 제거하고 예측 가능한 성능을 제공하는 SSD.
- **ZRWA (Zone Random Write Area)**: zone 앞단에 노출된 고정 크기 연속 LBA 영역. 이 안에서는 random write와 overwrite가 가능. NVMe 명세(2024) 및 상용 ZNS SSD가 지원.
- **ZRWAFG (ZRWA Flush Granularity)**: ZRWA가 flash로 flush되는 최소 단위(보통 flash page 크기, 16KB+). ZRWA window 이동의 coarse-grained 단위.
- **PPU (Partial Parity Update)**: full stripe보다 작은 부분 stripe 쓰기에서 패리티를 read-modify-write로 갱신하는 연산. ZNS의 no-overwrite와 충돌.
- **WP (Write Pointer)**: zone에서 다음 append 위치를 가리키는 포인터. non-ZRWA zone에선 유효 데이터 끝, ZRWA zone에선 ZRWA 시작점을 가리킴.
- **Real WP**: ZRWA zone에서 실제 연속 유효 데이터의 끝 LBA. WP와 의미가 달라 OOB metadata로 별도 추적.
- **metadata zone**: RAIZN이 PPU(parity log)를 append하는 전용 zone. 경합/GC/log header로 병목.
- **OOB (Out-Of-Band) area**: flash page당 8~64B의 spare 영역. Zebra가 LBA별 쓰기 여부 metadata를 기록.
- **all-to-all vs all-to-one**: 모든 data zone이 모든 zone에 패리티를 쓸 수 있는 분산 설계(Zebra) vs 한 metadata zone에 집중(RAIZN).

## 강점 · 한계 · 열린 질문
**강점**
- ZRWA의 'repeated sequential overwrite' 매칭이라는 통찰이 명확하고 우아함. metadata zone과 GC를 근본적으로 제거.
- crash consistency(OOB로 Real WP 복구)와 복구 속도(14.2x)까지 챙긴 완성도. 복구 후 공간 손실 0.
- small-zone에서 최대 4.2x로 효과가 크고, RAID 6/RocksDB 등 실용 시나리오 검증.

**한계**
- chunk 크기가 ZRWA size에 제약됨(상용 SSD ZRWA가 1MB~64MB라 충족하지만 향후 작은 ZRWA SSD에선 제약 가능).
- ZRWA + OOB + PLP 지원 SSD 의존(PLP 없으면 WP 이동이 flush 완료까지 지연).
- write throughput만 보면 batching 방식 ZapRAID에 24% 뒤짐(대신 latency 우위).
- small-zone 결과 상당수가 NVMeVirt 에뮬레이션 기반(실제 small-zone HW 부족).

**열린 질문**
- ZRWA window를 넘는 매우 긴 random write 패턴에서 PPU가 조기 commit될 때 성능 저하 정도는?
- multi-tenant 환경에서 active zone 수 제한(예: ZN540 14개)이 Zebra의 stripe cache 메모리 및 처리량에 미치는 영향?
- RAID 6 이상 높은 m에서 OOB metadata(LBA당 1B) 및 패리티 계산(AVX) 오버헤드의 확장성?

## ❓ Q&A (자가 점검)
> [!question]- Q1. ZNS RAID에서 PPU가 왜 문제인가?
> 답: 부분 stripe 쓰기는 패리티를 read-modify-write로 갱신(PPU)해야 하는데, ZNS는 append-only(no-overwrite)라 패리티 chunk를 제자리에서 덮어쓸 수 없다. 데이터센터 쓰기의 75%가 full stripe보다 작아 PPU가 빈번하다.

> [!question]- Q2. RAIZN은 어떻게 PPU를 처리하며 무엇이 병목인가?
> 답: PPU를 per-ZNS 전용 metadata zone에 append한다. 그러나 (1) 여러 logical zone이 한 metadata zone에 몰리는 multi-zone aggregation 경합, (2) obsolete PPU 회수용 RAID-level GC, (3) log header 쓰기 증폭(WAR 2.98)이 병목이 되어 throughput이 최대 59% 하락한다.

> [!question]- Q3. Zebra의 핵심 아이디어 한 문장은?
> 답: 패리티 갱신(PPU)이 'repeated sequential overwrite' 패턴임에 착안해, chunk를 ZRWA window 이내로 잡고 패리티를 ZRWA 안에서 in-place로 덮어써 metadata zone과 GC 없이 PPU를 처리한다.

> [!question]- Q4. ZRWA zone에서 Real WP를 어떻게 복구하나?
> 답: ZRWA mode의 WP는 ZRWA 시작점을 가리켜 실제 유효 데이터 끝을 모른다. Zebra는 쓰기 발생 LBA의 OOB(out-of-band) 영역에 1을 기록하고, 복구 시 OOB를 traverse해 연속 prefix의 끝을 Real WP로 복원한다. 비용은 LBA당 1 bit(패리티는 1 Byte).

> [!question]- Q5. machine-failure 복구는 어떻게 동작하나?
> 답: 3단계 — (1) zns-zone-management-recv로 physical zone 상태/WP 조회, (2) OOB scan으로 각 physical zone Real WP를 찾아 logical zone Real WP 동기화, (3) Real WP 너머 invalid data를 0으로 덮어쓰고 패리티 재계산. RAIZN 대비 14.2x 빠름(RAIZN은 시간 93%를 metadata zone traverse에 소비).

> [!question]- Q6. Zebra가 RAIZN보다 복구 후 공간을 아끼는 이유는?
> 답: RAIZN은 부분 기록된 stripe를 no-overwrite 제약 때문에 재사용 못 하고 다음 stripe로 건너뛰어 영구 공간 손실 + mapping table 메모리가 든다. Zebra는 ZRWA의 in-place overwrite로 부분 기록 stripe를 그대로 재사용해 공간 손실이 0이다.

> [!question]- Q7. large-zone과 small-zone에서 효과 차이는 왜 나나?
> 답: large-zone은 zone이 모든 channel을 공유해 RAIZN의 metadata zone도 대역폭을 어느 정도 확보하므로 이득이 +8~31%로 작다. small-zone은 zone이 한 channel에 묶여 metadata zone 대역폭이 심하게 제한되어 Zebra의 all-to-all 이점이 커져 최대 4.2x.

> [!question]- Q8. Zebra vs ZapRAID의 trade-off는?
> 답: ZapRAID는 작은 요청을 DRAM에서 모아 full-stripe로 batching해 PPU를 회피, write throughput이 평균 24% 높다. 그러나 batching 큐잉 지연으로 latency가 크고, Zebra는 batching 없이 즉시 처리해 latency를 4.1~7.8x 낮춘다.

## 🔗 Connections
[[ZNS]] · [[HPCA]] · [[2025]]

## References worth following
- [37] T. Kim et al., "RAIZN: Redundant Array of Independent Zoned Namespaces," ASPLOS 2023 — Zebra의 핵심 비교 대상, metadata zone 기반 ZNS RAID.
- [66] J. Lee et al., "WALTZ: Leveraging Zone Append to Tighten the Tail Latency of ZNS SSD," VLDB 2023 (ZapRAID 관련) — batching 기반 PPU 회피, latency vs throughput 대조군.
- [12] M. Bjørling et al., "ZNS: Avoiding the Block Interface Tax for Flash-based SSDs," USENIX ATC 2021 — ZNS 인터페이스의 기초.
- [59]/[7] NVMe ZRWA 명세 및 "What's New in NVMe Technology" — ZRWA 표준 동작 이해.
- [72] BIZA — ZRWA로 data+parity를 캐시하는 RAID, Zebra와 가장 가까운 ZRWA 활용 선행연구(crash consistency 차이).
- [32] J. Zhang et al., "Tailored: Achieving consistent low latency for commodity SSD arrays," FAST 2021 — PPU 비율 등 워크로드 특성의 근거.

## Personal annotations
<본인 메모 영역>
