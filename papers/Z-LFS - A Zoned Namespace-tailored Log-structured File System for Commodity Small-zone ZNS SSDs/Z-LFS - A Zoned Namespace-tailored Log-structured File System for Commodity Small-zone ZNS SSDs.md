---
title: "Z-LFS: A Zoned Namespace-tailored Log-structured File System for Commodity Small-zone ZNS SSDs"
aliases: [Z-LFS]
description: "Commodity small-zone ZNS SSD를 standalone으로 쓰는 ZNS 맞춤형 LFS — append-only 메타데이터, speculative log stream 관리, conflict-aware zone 할당으로 F2FS 대비 최대 33.44x 성능."
venue: ATC
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/zns
  - topic/zns
  - topic/log-structured
  - venue/atc
  - year/2025
---

# Z-LFS: A Zoned Namespace-tailored Log-structured File System for Commodity Small-zone ZNS SSDs

> **ATC 2025** · `cluster/zns` · Source: [Z-LFS - A Zoned Namespace-tailored Log-structured File System for Commodity Small-zone ZNS SSDs.pdf](Z-LFS%20-%20A%20Zoned%20Namespace-tailored%20Log-structured%20File%20System%20for%20Commodity%20Small-zone%20ZNS%20SSDs.pdf)

**저자**: Inhwi Hwang (Seoul National University), Sangjin Lee (Chung-Ang University), Sunggon Kim (Seoul National University of Science and Technology), Hyeonsang Eom (Seoul National University), Yongseok Son (Chung-Ang University, 교신저자)

## TL;DR
Z-LFS는 추가 CNS SSD 없이 commodity small-zone ZNS SSD를 단독으로 사용할 수 있도록 설계한 ZNS 맞춤형 log-structured file system이다. (1) 메타데이터를 immutable/mutable로 분류해 append-only로 관리하고(immutable은 segment 끝에 append, mutable은 delta logging), (2) log stream별 write intensity에 따라 active zone을 동적으로 조절하는 speculative log stream management로 zone-level parallelism을 극대화하며, (3) ZNS SSD 내부 자원(channel/die) 충돌을 피하는 conflict-aware zone allocation을 적용한다. F2FS 기반으로 Linux 커널 5.17.4에 구현했고, F2FS 대비 최대 33.44x, 최신 ZNS 인터페이스(eZNS) 대비 최대 3.5x 성능 향상을 보였다.

## 문제 & 동기
ZNS SSD는 zone 단위 sequential write + explicit reset이라는 제약 때문에 기존 file system을 그대로 쓰기 어렵다. F2FS 같은 LFS가 ZNS를 지원하지만 두 가지 문제가 있다. 첫째, F2FS는 메타데이터를 block 단위 고정 위치에 in-place update하여 wandering tree problem을 해결하는데, ZNS는 in-place update를 허용하지 않으므로 메타데이터용 추가 CNS SSD가 필요하다(저장 시스템 비용 증가). 둘째, F2FS는 log stream마다 단 하나의 zone만 active로 써서 zone-level parallelism이 제한되고, 특히 active zone이 많은 small-zone ZNS SSD(예: 최대 384개)에서 under-utilization이 발생한다. 또한 zone들이 동일 channel/die에 매핑되면 내부 자원 충돌이 일어난다.

> [!quote]- 📄 원문 표현 (paper)
> "Specifically, 1) LFS has a potential inability to use ZNS SSD as a standalone SSD [1] resulting from CNS-based metadata design and its in-place updates and 2) there can be a performance issue due to CNS-based data placement and I/O operations." (p.2)
>
> "Our goal in this study is to design a ZNS-tailored LFS to maximize 1) the cost-efficiency of the storage system and 2) the resource utilization of small-zone ZNS SSDs." (p.3)

## 핵심 통찰

> [!note]- 통찰 토글
> - **메타데이터 생명주기 관찰**: ZNS 위 LFS에서는 segment가 한 번 쓰이면 GC로 erase되기 전까지 갱신 불가하므로, 일부 메타데이터(예: F2FS의 segment summary, SS)의 생명주기가 segment와 동일하다. 이런 write-once/never-update 메타데이터(immutable)는 segment 끝에 append만 하면 되어 metadata GC와 over-provisioning이 불필요해진다. 반대로 SIT(valid block bitmap), NAT(node 주소)는 자주 갱신되는 mutable 메타데이터로 별도 관리해야 한다. (p.7, §4.4.1)
> - **active zone 활용은 workload write intensity에 비례해야**: 모든 log stream에 동일하게 active zone을 정적으로 나누면(static striping), I/O가 특정 stream에 쏠릴 때 낮은 트래픽 stream이 자원을 낭비하고 높은 트래픽 stream은 자원을 못 쓴다. 동적 scaling이 필요하다. (p.6, §3.3)
> - **ZNS 내부 자원 충돌 모델링**: small-zone ZNS는 zone을 single die/channel에 fine-grained 매핑한다. 측정 결과 zone ID가 16의 배수일 때 channel-level 충돌로 19% 저하, 128의 배수일 때 die-level 충돌로 50% 이상 저하가 발생. 테스트베드는 16 channels, 128 dies로 round-robin 매핑됨을 추론. (p.5, §3.2)

## 설계 / 메커니즘

> [!note]- 설계 토글
> **세 가지 핵심 전략 (p.6, §4.2)**
> - **Strategy #1 — Append-only 메타데이터 관리**: 메타데이터를 segment 생명주기 정렬 여부로 immutable/mutable로 분류. Immutable 메타데이터는 동일 superzone 내 해당 segment 끝에 append(GC 시 자연 정리). Mutable 메타데이터는 delta logging.
> - **Strategy #2 — Speculative log stream management**: 각 log stream의 write intensity를 시간 window 내 write request 비율로 추정해 superzone 수를 비례 할당, active zone을 동적 scale.
> - **Strategy #3 — Conflict-aware zone allocation**: channel overlap 없는 연속 zone을 superzone으로 묶고, 동일 die에 매핑된 superzone들을 interference group(IG)으로 묶어 서로 다른 IG에서 zone 할당.
>
> **On-disk 레이아웃 (Figure 4, p.6)**: 세 영역 — delta log area(mutable 메타 logging), merge area(consolidate된 mutable 메타 테이블), main area(데이터/node log + immutable 메타). Main area의 연속 zone들은 superzone으로 조직되고 다시 고정 크기 segment로 분할.
>
> **Delta logging (Figure 5, p.7~8)**: mutable 메타데이터는 수십 바이트로 작고 partial update가 잦으므로, 변경분(delta)만 모아 새 metadata log block에 logging. Delta log area는 zone 쌍(MDlog0/MDlog1)을 circular log로 운영. Merge area는 metadata table 쌍(MD0/MD1)을 두고 교대로 갱신해 한 쪽은 항상 valid 상태 유지(crash consistency). Delta log area가 절반 차면 asynchronous merge로 delta를 table에 통합 후 flush.
>
> **Speculative log stream management (Figure 6/7, p.8~9)**: 6개 log stream(data/node × hot/warm/cold). log stream i의 active zone 수 A_i = A × (W_i / W_T) (W_i: write 트래픽, W_T: 동종 stream 합, A: 총 가용 active zone). A = min(A_peak, A_avail/2). high-traffic stream은 free list에서 superzone을 받아 scale up, low-traffic stream은 FINISH zone command로 scale down(background). data/node log stream은 동시 사용되지 않으므로 별도 speculation.
>
> **Conflict-aware zone allocation (Figure 8, p.9~10)**: 16 zone = 1 superzone(channel 충돌 회피), 동일 die superzone들을 IG로 묶음(테스트베드 8 IG). superzone을 non-overlapping IG 단위로 log stream에 할당, data/node log stream은 별도 IG list 유지. IG가 소진되면 overlapping IG에서 fallback, GC 시 free superzone이 가장 적은 IG를 victim 우선.
>
> **Crash consistency (p.10, §4.7)**: F2FS처럼 roll-back(마지막 CP 이후) + roll-forward(마지막 fsync 이후). immutable 메타는 segment 끝에 append되어 CP로 일관성 보장. mutable 메타는 delta log를 scan해 valid metadata table에 merge, checkpoint version과 log version 비교로 commit 여부 판정. 복구 후 closed zone을 FINISH로 full 전환해 active zone pool 초기화.
>
> **구현 (p.10, §4.8)**: F2FS 기반, 커널 5.17.4. SS→immutable, SIT/NAT→mutable로 분류. SIT/NAT는 생명주기가 달라 별도 delta log/merge area/memory buffer로 독립 관리.

## 평가

> [!note]- 평가 토글
> **테스트베드 (p.11, §6.1)**: i7-13700K(16 core), 32GB RAM, commodity small-zone ZNS SSD(40,704 zones, zone 96MB, 총 3.92TB, 최대 active zone 384), Linux 5.17.4. 비교 대상: F2FS, F2FS_SS(static striping), eZNS, eZNS+F2FS. ZenFS(RocksDB용)도 RocksDB 평가에 포함.
> - **Write throughput (Figure 9, p.12)**: F2FS 대비 sequential 최대 12.4x / random 25.2x, F2FS_SS 대비 sequential 47% / random 30% 향상. eZNS+F2FS 대비 sequential/random 모두 최대 3.5x. Z-LFS는 가장 바쁜 warm data log stream에 최대 128 active zone 사용(F2FS_SS는 64로 제한).
> - **Read throughput (Figure 9, p.12)**: sequential read는 F2FS 대비 50%, F2FS_SS 대비 22% 향상(conflict-aware 할당). random read는 locality 낮아 유사.
> - **Write latency (Table 1, p.12)**: 평균 latency F2FS 626us → Z-LFS 24.5us(최대 96.1% 감소), tail latency(99.9%) 47ms → 39ms. F2FS_SS(31.9us/103.9us), eZNS+F2FS(89.4us/45.3ms) 대비도 개선.
> - **GC-intensive (Figure 9, p.12)**: F2FS 대비 최대 3.3x throughput, eZNS+F2FS와 유사.
> - **Metadata-intensive (MDtest, Figure 10, p.12~13)**: file creation에서 F2FS 대비 최대 27.7x, F2FS_SS 대비 11.4x, eZNS+F2FS 대비 1.6x.
> - **WAF & 메모리 (Figure 11, p.13)**: WAF는 F2FS_SS와 유사(append-only 메타데이터 효과). peak 메모리는 F2FS 대비 최대 38%(FIO)/3%(MDtest) 높음, delta logging buffer로 인한 절대 증가량은 FIO +250MB, MDtest +380MB로 제한적(delta log area 크기로 상한).
> - **Active zone scaling (Figure 12, p.13)**: hot/warm/cold 3 phase에서 static 최적 스킴과 동등 성능 → runtime 적응 입증.
> - **Macro (Filebench, Figure 13, p.13)**: fileserver F2FS 대비 7.21x, varmail 33.44x(node flush 빈번), webserver 2.09x, videoserver 13.7x.
> - **RocksDB (Figure 14, p.14)**: fillseq ZenFS 대비 25.0x, fillrandom 9.28x, overwrite 9.01x; readrandom은 유사.
> - **공간 오버헤드 (p.11~12)**: delta log + merge area 총합 ZNS SSD 용량의 약 0.02%로 negligible. immutable append로 in-place 대비 공간 오버헤드 10.7x 절감.

## 섹션 노트
- **§1 Introduction**: ZNS SSD의 host-managed 특성과 기존 LFS(F2FS)의 두 문제(CNS 의존 메타데이터, CNS 기반 데이터 배치) 제시. 목표는 cost-efficiency + small-zone 자원 활용. 소스 공개(github.com/Z-LFS/Z-LFS).
- **§2 Background and Motivation**: small-zone(zone당 die 적음, active zone 多 최대 384) vs large-zone(zone당 die 多, active zone 少 최대 14) 비교. LFS의 세 challenge: append-only 메타데이터, active zone 활용, 내부 자원 충돌.
- **§3 Analysis of ZNS SSD performance**: zone 수에 따른 read/write 성능(128/256 active zone에서 ~2.4GB/s 포화, Figure 1), channel/die 충돌 측정(Figure 2), F2FS/eZNS/F2FS_SS 성능 비교(Figure 3).
- **§4 Design and Implementation**: 설계 목표 3개 → 전략 3개 → 아키텍처(Figure 4) → append-only 메타데이터(delta logging, Figure 5) → speculative log stream(Figure 6/7) → conflict-aware allocation(Figure 8) → crash consistency → 구현.
- **§5 Discussion**: large-zone ZNS 적용 가능(메타데이터/충돌 기법은 유효, speculative는 효과 제한적), single-stream LFS, multi-tenant 확장, 공간 오버헤드.
- **§6 Evaluation**: micro/macro/real-world 전방위 평가.
- **§7 Related Work / §8 Conclusion**: eZNS/ZMS/CSAL 등 인터페이스 연구, ZenFS/ZenFS+/WALTZ 등 RocksDB backend, ZNSwap/RAIZN/BIZA 등 커널 subsystem과 대비. Z-LFS는 인터페이스가 아닌 file system 자체를 ZNS에 맞춤 재설계.

## 핵심 용어
- **ZNS SSD (Zoned Namespace SSD)**: 물리 주소 공간을 고정 크기 zone으로 분할하고 zone 단위 sequential write + explicit reset을 요구하는 SSD. host가 데이터 관리 책임을 짐.
- **Small-zone / Large-zone ZNS SSD**: zone이 single die에 담겨 active zone이 많은(예 384) 것이 small-zone, zone이 여러 die에 걸쳐 active zone이 적은(예 14) 것이 large-zone.
- **Active zone**: opened/closed 상태로 write 가능한 zone. ZNS는 동시 active zone 수에 상한이 있음.
- **CNS (Conventional Namespace) SSD**: 기존 in-place update 가능한 일반 SSD.
- **Immutable / Mutable metadata**: write-once/never-update 메타(예 SS)는 immutable, 자주 갱신되는 메타(예 SIT/NAT)는 mutable로 Z-LFS가 분류.
- **Delta logging**: mutable 메타데이터의 변경분(delta)만 dedicated delta log area에 append하고 주기적으로 merge area의 table에 통합하는 기법.
- **Superzone**: channel 충돌 회피를 위해 연속된 16개 zone을 묶은 단위. 다시 segment로 분할.
- **Interference Group (IG)**: 동일 die에 매핑된 superzone들의 그룹. 서로 다른 IG에서 할당해 die-level 충돌 회피.
- **Zone-level parallelism**: 여러 active zone에 분산 I/O하여 얻는 병렬성(small-zone에서 특히 중요).
- **eZNS / v-zone**: 다수 zone을 묶은 logical zone(v-zone)을 통해 active zone을 동적 할당하는 ZNS 인터페이스(SOTA 비교 대상).
- **Speculative log stream management**: log stream별 write intensity를 추정해 active zone(superzone) 할당을 동적으로 조절하는 Z-LFS 기법.

## 강점 · 한계 · 열린 질문
**강점**
- ZNS 제약 하에서 추가 CNS SSD 없이 standalone 운용 → 저장 시스템 비용 절감.
- 순수 software(F2FS 기반), commodity ZNS SSD에 ZNS 펌웨어 수정 없이 적용 → 호환성 높음.
- micro/macro/real-world에 걸친 광범위 평가와 소스 공개.

**한계**
- delta logging buffer로 인한 메모리 추가 사용(FIO +250MB 등), 절대량은 작으나 존재.
- 단일 tenant 환경 평가 중심. multi-tenant fairness/rate-limiting은 future work.
- speculative log stream management는 large-zone ZNS나 single-stream LFS에서는 효과 제한적이라고 저자가 인정.
- ZNS 내부 매핑(16 channel/128 die)을 측정으로 추론한 모델에 의존 → 다른 ZNS 제품에 일반화 시 재측정 필요.

**열린 질문**
- 다양한 vendor의 ZNS 내부 자원 구조를 자동 발견하는 메커니즘이 있다면 conflict-aware 할당을 portable하게 만들 수 있는가?
- multi-tenant에서 speculative scaling과 fairness를 어떻게 동시 보장할 것인가?
- large-zone ZNS에서 immutable 메타데이터를 small random writable area에 in-place로 두는 변형의 실제 이득은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. F2FS가 ZNS SSD를 standalone으로 못 쓰는 근본 이유는?
> > F2FS는 메타데이터를 block-level 고정 위치에서 in-place update해 wandering tree problem을 해결하는데, ZNS는 in-place update와 random write를 금지한다. 따라서 메타데이터용 별도 CNS SSD가 필요해져 standalone 사용이 불가능하고 비용이 증가한다. (p.2)

> [!question]- Q2. Z-LFS가 메타데이터를 immutable/mutable로 나누는 기준과 처리 방식은?
> > 기준은 segment 생명주기와의 정렬 여부다. write-once/never-update(예 segment summary)는 immutable로 해당 segment 끝에 append하여 GC 시 함께 정리(metadata GC 불필요). 자주 갱신되는(예 SIT, NAT) mutable은 delta logging으로 변경분만 별도 delta log area에 기록 후 merge한다. (p.7~8)

> [!question]- Q3. Delta logging이 crash consistency를 어떻게 보장하는가?
> > delta log area는 zone 쌍을 circular log로 운용하고, merge area는 metadata table 쌍(MD0/MD1)을 교대로 갱신해 한 쪽은 항상 valid 상태를 유지한다. 복구 시 valid table을 읽고, checkpoint version 이하의 log version만 valid로 인정해 delta를 merge(roll-forward)한다. (p.7~8, p.10)

> [!question]- Q4. Speculative log stream management는 active zone을 어떻게 배분하는가?
> > 시간 window 내 각 log stream의 write request 비율 W_i/W_T로 write intensity를 추정하고, A_i = A × (W_i/W_T)로 active zone(superzone)을 비례 할당한다(A = min(A_peak, A_avail/2)). high-traffic stream은 free list에서 superzone을 받아 scale up, low-traffic stream은 FINISH zone command로 scale down한다. data/node stream은 동시 사용되지 않아 따로 speculation한다. (p.8~9)

> [!question]- Q5. Conflict-aware zone allocation에서 superzone과 IG의 역할 차이는?
> > superzone은 channel 충돌을 피하기 위해 channel overlap 없는 연속 16 zone을 묶은 단위다. IG(interference group)는 동일 die에 매핑된 superzone들의 그룹으로 die-level 충돌 회피용이다. Z-LFS는 서로 다른 IG의 superzone을 log stream에 할당해 두 종류 충돌을 모두 줄인다. (p.9~10)

> [!question]- Q6. F2FS_SS(static striping)에 비해 Z-LFS가 더 나은 이유는?
> > F2FS_SS는 모든 log stream에 active zone을 균등 정적 분배(예 64씩)해 write intensity가 쏠릴 때 자원을 낭비한다. Z-LFS는 speculative management로 가장 바쁜 stream(warm data)에 최대 128 active zone을 동적 할당하고, conflict-aware 배치로 read도 개선해 sequential write +47%, read +22% 향상한다. (p.6, p.12)

> [!question]- Q7. 가장 큰 성능 향상이 나온 워크로드와 그 이유는?
> > varmail(Filebench)에서 F2FS 대비 33.44x로 가장 컸다. varmail은 node를 빈번히 생성·flush하는 메타데이터 집약 워크로드인데, Z-LFS의 append-only 메타데이터 관리와 node를 main area로 효율 flush하는 점이 결합해 큰 이득을 냈다. (p.13)

> [!question]- Q8. Z-LFS의 추가 공간/메모리 오버헤드는 어느 정도인가?
> > delta log + merge area를 합쳐 ZNS SSD 용량의 약 0.02%로 negligible하며, immutable append 덕분에 in-place 방식 대비 공간 오버헤드를 10.7x 절감한다. 메모리는 delta logging buffer 때문에 F2FS 대비 peak +38%(FIO) 수준이나 절대 증가량(FIO +250MB)은 delta log area 크기로 상한이 있어 제한적이다. (p.11~13)

## 🔗 Connections
[[ZNS]] · [[ATC]] · [[2025]]

## References worth following
- Bjørling et al., "ZNS: Avoiding the block interface tax for flash-based SSDs", USENIX ATC 2021 [9] — ZNS 인터페이스의 근간 + ZenFS backend.
- Min et al., "eZNS: An elastic zoned namespace for commodity ZNS SSDs", OSDI 2023 [42] / TOS 2024 [43] — 본 논문의 핵심 SOTA 비교 대상(v-zone 기반 동적 active zone).
- Lee, Sim, Hwang, Cho, "F2FS: A new file system for flash storage", FAST 2015 [29] — Z-LFS 구현의 기반 LFS.
- Han et al., "ZNS+: Advanced zoned namespace interface for supporting in-storage zone compaction", OSDI 2021 [17] — ZNS 데이터 copy offload 인터페이스.
- Hwang et al., "ZMS: Zone abstraction for mobile flash storage", USENIX ATC 2024 [20] — mobile flash의 zone abstraction, on-device memory/fsync 대응.
- Lee, Kim, Lee, "WALTZ: Leveraging zone append to tighten the tail latency of LSM tree on ZNS SSD", VLDB 2023 [32] — zone append 기반 tail latency 개선.

## Personal annotations
<본인 메모 영역>
