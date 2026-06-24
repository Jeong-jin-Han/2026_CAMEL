---
title: "BIZA: Design of Self-Governing Block-Interface ZNS AFA for Endurance and Performance"
aliases: [BIZA]
description: "ZNS SSD의 ZRWA·내부 병렬성을 활용해 블록 인터페이스를 유지하면서 write amplification·throughput·tail latency를 동시에 개선한 self-governing AFA"
venue: SOSP
year: 2024
tier: deep
status: done
tags:
  - paper
  - cluster/zns
  - topic/zns
  - topic/all-flash-array
  - venue/sosp
  - year/2024
---

# BIZA: Design of Self-Governing Block-Interface ZNS AFA for Endurance and Performance

> **SOSP 2024** · `cluster/zns` · Source: [BIZA - Design of Self-Governing Block-Interface ZNS AFA for Endurance and Performance.pdf](<BIZA - Design of Self-Governing Block-Interface ZNS AFA for Endurance and Performance.pdf>)

**저자**: Shushu Yi, Shaocong Sun, Li Peng, Yingbo Sun, Ming-Chang Yang, Zhichao Cao, Qiao Li, Myoungsoo Jung, Ke Zhou, Jie Zhang (Peking University, CUHK, Arizona State University, Xiamen University, KAIST and Panmnesia, WNLO/HUST). 코드: https://github.com/ChaseLab-PKU/BIZA

## TL;DR
All-Flash Array(AFA)는 상위 SW에 어떤 외부 인터페이스를 노출하고 어떤 내부 인터페이스(SSD)를 쓸지가 미해결 문제다. 기존 block-interface AFA는 SSD 내부(GC, data placement)를 firmware에 숨겨 endurance·성능이 나쁘고, RAIZN 같은 ZNS-interface AFA는 ZNS를 그대로 노출해 호환성이 떨어진다. BIZA는 ZNS SSD를 내부 인터페이스로 쓰되 상위에는 친숙한 **block interface**를 노출하는 self-governing AFA로, ZNS의 **ZRWA(Zone Random Write Area)**와 zone/I-O channel **병렬성**을 능동적으로 활용한다. 그 결과 SOTA AFA 대비 write amplification을 **42.7% 감소**, throughput을 **93.2% 향상**, 99.99th tail latency를 **62.8% 감소**시켰다.

## 문제 & 동기
AFA의 핵심 설계 갈래는 (1) block-interface AFA (mdraid 등): 상위에 block을 노출하고 내부 SSD도 block-interface → SSD가 GC·data placement를 암묵적으로 처리해 endurance가 짧고 tail latency가 크다. (2) ZNS-interface AFA (RAIZN): ZNS를 그대로 상위에 노출 → cross-layer 최적화는 가능하나 sequential write만 허용되어 random write를 가정하는 대부분 SW와 호환되지 않는다. (3) adapter (dm-zap): block↔ZNS 매핑을 유지해 호환성은 풀지만, dm-zap+RAIZN처럼 단순 결합하면 ZNS의 이점이 모두 사라진다 — 서로 다른 수명의 데이터가 같은 zone에 섞여 endurance가 나빠지고(WA 33.3% 증가), zone당 in-flight write가 1개로 제한되거나 centralized parity zone 때문에 throughput이 이상치의 47.7%에 그치며, GC가 user I/O와 같은 I/O resource에서 충돌해 99.99th tail latency가 10.3배 악화된다.

> [!quote]- 📄 원문 표현 (paper)
> "In this work, we propose BIZA, a self-governing block-interface ZNS AFA to proactively schedule I/O requests and SSD internal tasks via the ZNS interface while exposing the user-friendly block interface to upper-layer software." (p.1, Abstract)
>
> "Our key insight is that the newly introduced features and internal parallelisms of ZNS SSDs can be exploited to address the write amplification and performance issues in existing AFA solutions." (p.2, §1)
>
> "neither dm-zap nor RAIZN explicitly exploits the ZNS interface to achieve cross-layer optimizations." (p.2, §1)

## 핵심 통찰

> [!note]- 통찰 토글
> - **ZRWA로 write amplification 완화**: ZRWA는 SSD 내부 write buffer의 추상화로, write pointer 뒤 고정 크기 영역에서 in-place update를 허용한다. 자주 갱신되는 data와 partial parity를 ZRWA에 흡수해 flash flush를 피하면 WA를 줄일 수 있다. 단 ZRWA는 매우 작다(ZN540에서 zone당 1MB, 최대 14MB) — SYSTOR 워크로드의 reuse distance CDF상 14MB 미만은 17%뿐이라 무턱대고 쓰면 83%가 흡수 안 된다. 따라서 "어떤 chunk를 같은 zone에 둘지"를 선별해야 한다. (p.5 §3.1, Table 2, Fig.4)
> - **intra-zone parallelism**: zone당 in-flight write 1개(dm-zap 방식)는 zone 대역폭의 34.7%만 낸다(예비 실험). 32 in-flight write를 허용하면 평균 54.5% throughput 회복. APPEND command는 ZRWA와 상호 배타적이고 대부분 SW가 block 기반이라 부적합 → ZRWA를 쓰되 별도 scheduler로 동시 write를 안전하게. (p.5 §3.2, Fig.5)
> - **inter-zone parallelism**: 서로 다른 I/O channel에 매핑된 zone들은 병렬 처리 가능. 같은 channel의 두 zone(Scenario 2)은 단일 zone 대비 throughput 개선 없음(1.0배)이나, 다른 channel(Scenario 3)은 대역폭 2배. 따라서 centralized parity zone(RAIZN)을 버리고 decentralized 설계로 가야 하며, GC도 user I/O와 다른 channel에서 돌려 tail latency를 낮춘다. (p.6 §3.3, Table 3)
> - **핵심 난관**: ZNS는 wear leveling 때문에 zone↔I/O channel 매핑을 호스트에 숨긴다. 이 불확실성이 격리된 zone 선택을 어렵게 한다 → 대부분 상용 ZNS SSD가 round-robin 매핑이라는 점을 이용해 guess-and-verify로 저비용 온라인 추정. (p.6 §3.3)

## 설계 / 메커니즘

> [!note]- 설계 토글
> **I/O path / 아키텍처 (Fig.6, §4.1)**: BIZA는 log-structure 유사 방식으로 write를 zone에 append하되 ZRWA 덕분에 in-place overwrite도 가능. write 도착 시 ① parity 계산, ② RAID 레벨(RAID5 left-asymmetric)에 따라 data+parity를 chunk 단위로 어느 SSD에 둘지 결정, ③ **zone group selector**가 chunk를 obsolete chunk에서 격리할 zone group(서로 다른 I/O channel에 매핑된 open zone 집합)을 선택, ④ **GC avoidance**가 zone group 내 어느 zone에 쓸지 최종 결정, ⑤ **ZRWA-aware I/O scheduler**가 sliding-window로 ⑥ I/O dispatch, ⑦ 완료 latency를 수집해 zone↔channel 추정을 보정.
>
> **Mapping table (§4.1)**: 두 테이블 사용 — Block Mapping Table(BMT, LBN→40bit PA + 32bit SN)과 Stripe Mapping Table(SMT, SN→stripe 내 parity들의 PA). PA의 최상위 8bit=SSD index. 4TB×4 SSD 기준 32GB 메모리(0.19%). crash consistency는 SSD **OOB 영역**(페이지당 72bit)에 BMT/SMT 항목을 hitchhiking으로 기록해 매핑 테이블 별도 persist로 인한 WA를 피함.
>
> **Zone group selector (§4.2, Fig.7)**: ghost-cache 기반 알고리즘으로 hot data 식별. ① **LRU cache**: temporal locality 약한 chunk 필터링. ② **HR(high-revenue) cache**: predicted reaccess number > 임계 T1(경험적 3)인 chunk(자주 갱신될 것) 승격 — 최소 reaccess 항목 evict. ③ **HP(high-profit) cache**: reuse distance < 임계 T2(ZRWA 크기의 2배)인 chunk(자주 갱신+짧은 거리) 승격 — 최대 reuse distance evict. zone group은 ZRWA-aware/GC-aware/trivial 세 종류로 나눠 각각 HP/HR·LRU·기타 chunk를 격리. ghost cache는 속성(reaccess number, reuse distance)만 저장해 footprint 작음(7.6MB).
>
> **GC avoidance (§4.3)**: GC event를 user I/O와 다른 channel의 zone으로 보내 충돌 회피. GC 중 victim zone의 valid chunk를 새 zone(GC-interfered zone)으로 옮기고 그 channel을 BUSY 태그. write가 오면 BUSY 아닌 channel zone 선택(단 ZRWA in-place update는 on-device DRAM이라 BUSY 무시).
>
> **I/O channel detection — guess-and-verify (§4.3, Fig.8)**: round-robin 가정으로 1차 추정, 온라인 latency 모니터링으로 검증. GC 중 특정 zone의 latency가 평균보다 유의하게 높으면 그 zone이 BUSY channel에 mistaken 매핑되었다고 보고 vote 부여. 같은 zone에서 여러 번(예 3) latency spike 반복 시 vote 최다 channel로 추정 보정. AFA 생성 시 zone-to-zone diagnosis로 미리 확정된 zone(Zone z)의 vote는 무조건 신뢰.
>
> **ZRWA-aware I/O scheduler (§4.4, Fig.9)**: I/O reorder로 인한 write 실패 방지 + async write 지원. zone마다 bitmap + sliding window 유지로 ZRWA 위치를 정확히 추적(REPORT command 빈번 호출 부담 회피). sliding window 내 block 대상 request만 디바이스로 전송, 그 외(Request C)는 보류. write 완료 시 bitmap 해당 bit clear, 최좌측 bit가 0이면 window를 우측으로 shift.
>
> **구현 (§4.5)**: Linux kernel pluggable device mapper, 4K LOC. ZRWA 지원 위해 block layer/NVMe driver를 NVMe ZNS 1.1a + TP 4076b에 맞춰 1K LOC 보완. 하드웨어 수정 불필요(ZRWA·OOB는 상용 ZNS SSD 기본 기능).

## 평가

> [!note]- 평가 토글
> **셋업 (§5.1, Table 5)**: 26-core Xeon 5320, 512GB DDR4, Ubuntu 22.04 / kernel v5.15. ZNS=WD ZN540 4TB ×4 (RAID5; zone 1077MB, ZRWA 1MB×14 open zones, R/W 3265/2170 MB/s). conventional=WD SN640 4TB. fio v3.30, perf v5.15. 비교군: BIZA, RAIZN, dmzap+RAIZN, mdraid+dmzap, mdraid+ConvSSD. 각 실험 10회 평균.
> - **Microbenchmark write (Fig.10, §5.2)**: BIZA는 dmzap+RAIZN 대비 2.7배, mdraid+dmzap 대비 2.5배, mdraid+ConvSSD 대비 0.4배 높은 대역폭, 거의 4 ZNS SSD throughput의 92.2%(이상치) 달성. dmzap+RAIZN은 centralized 설계로 이상치의 47.7%에 그침.
> - **Microbenchmark read (Fig.11, §5.2)**: 모든 플랫폼 4KB read 유사. 64KB/192KB에서 mdraid 계열은 SW 병목으로 BIZA·dmzap+RAIZN보다 뒤처짐. read는 이상치(12.8GB/s)에 근접.
> - **I/O traces (Fig.12, §5.2)**: FIU/MSRC/MSPC/SYSTOR/Tencent. dmzap+RAIZN은 centralized parity zone 때문에 mdraid+dmzap 대비 평균 98.1% 뒤처짐. BIZA는 mdraid+dmzap 대비 write throughput 평균 76.5% 추가 향상, mdraid+ConvSSD와 동등. RAIZN 대비 평균 53.8% 향상.
> - **실제 앱 (Fig.13, §5.3)**: F2FS+filebench에서 BIZA가 RAIZN 대비 randomwrite +26.6%, fileserver +24.9%, webserver +18.7% (webserver는 write 4.8%뿐이라 개선 적음). RocksDB+db_bench(F2FS 위)에서 RAIZN 대비 최대 10.5%, 평균 8.0% 향상.
> - **Write amplification (Fig.14, §5.4)**: 14 zone(56MB ZRWA) 사용. BIZA가 mdraid+dmzap 대비 data write 평균 32.5% 감소(mdraid는 휘발성 in-host write buffer를 주기적 flush해야 하나 BIZA는 비휘발 ZRWA로 흡수). zone group selector 사용 시 미사용(BIZAw/oSelector) 대비 추가 12.6% write 감소. tencent는 90.2% chunk가 56MB 초과 reuse distance라 개선 적음(casa는 8.3%).
> - **GC tail latency (Fig.15, §5.5)**: BIZA가 BIZAw/oAvoid 대비 throughput-sensitive 시나리오에서 latency spike 27.4% 완화, latency-sensitive에서 74.9% 완화. ideal(BIZA no GC) 대비 99.99th write latency 4KB/64KB/192KB에서 1.0/0.9/1.4배.
> - **ZRWA 크기 민감도 (Fig.16, §5.6)**: ZRWA 4KB→1024KB 증가 시 data·parity write 모두 감소. 4KB(=chunk 크기)에서도 partial parity write가 전부 제거됨(곧 갱신될 partial parity 흡수).
> - **CPU overhead (Fig.17, §5.7)**: dm-zap이 centralized lock으로 spin해 dmzap+RAIZN·mdraid+dmzap CPU의 50.4%/84.7% 차지. BIZA는 dmzap+RAIZN 대비 31.5% CPU 추가 사용으로 I/O 병렬화, GB/s당 CPU 효율 88.5% 향상.
> - **요약 (Abstract, Conclusion)**: SOTA AFA 대비 WA 42.7%↓, write throughput 93.2%↑, 99.99th tail latency 62.8%↓.

## 섹션 노트
- **§1 Introduction**: AFA 인터페이스 선택 문제 정의, Fig.1으로 세 설계 비교, dm-zap+RAIZN 단순 결합의 3대 문제(endurance/throughput/tail) 정량화, BIZA 제안.
- **§2 Background and Challenge**: AFA=SSD RAID. block-interface AFA(§2.1, FTL/GC가 endurance·tail 악화), ZNS-interface AFA(§2.2, RAIZN sequential-only 한계), simple solution(§2.3, dm-zap adapter), Table 1 비교.
- **§3 Preliminary Study**: ZRWA 정의·크기 한계(§3.1, Table 2, Fig.3/4), intra-zone parallelism(§3.2, Fig.5), inter-zone parallelism(§3.3, Table 3), Table 4 challenge↔key insight 매핑.
- **§4 Design**: overview(§4.1, Fig.6, BMT/SMT/OOB), zone group selector(§4.2, ghost cache, Fig.7), GC-induced latency 해결(§4.3, GC avoidance + guess-and-verify, Fig.8), ZRWA-aware scheduler(§4.4, Fig.9), 구현(§4.5).
- **§5 Evaluation**: microbench write/read, traces, 실제 앱, WA, GC tail, ZRWA 민감도, CPU.
- **§6 Related Work**: large vs small zone, SSD I/O 인터페이스, GC 완화(SWAN/FusionRAID/IODA), inter-zone(eZNS), ZNS WA 감소(ZenFS/LL-compaction/LSM_ZGC), ZRWA를 cache로 보기, 미래 ZNS 설계(CQE에 매핑 piggyback, CXL).
- **§7 Conclusion**: WA 42.7%↓, throughput 93.2%↑, tail 62.8%↓.

## 핵심 용어
- **AFA (All-Flash Array)**: 여러 SSD를 묶어 용량·throughput을 모으고 redundancy(RAID parity)로 내결함성을 제공하는 스토리지 형태. 이 논문은 RAID5 중심.
- **ZNS (Zoned Namespace) SSD**: flash backbone을 zone으로 묶어 host가 write 위치/시점을 fine-grained하게 제어하게 하는 NVMe 표준. zone은 sequential write만, write pointer로 위치 추적, RESET으로 비움.
- **ZRWA (Zone Random Write Area)**: write pointer 뒤 고정 크기 영역에서 random/in-place write를 허용하는 ZNS의 새 기능. SSD 내부 write buffer의 추상화(battery-backed DRAM/NVM/SLC). ZN540에서 zone당 1MB, 최대 14MB로 매우 작음.
- **Write Amplification (WA)**: user write 대비 flash로 실제 내려간 write의 비율. GC 시 valid data 이주, 수명 다른 data 혼재가 WA를 키움.
- **I/O channel**: ZNS SSD 내부의 독립적 I/O resource 그룹(flash die 묶음). 다른 channel의 zone은 병렬 처리 가능.
- **Zone group**: 서로 다른 I/O channel에 매핑된 open zone 집합. BIZA가 chunk를 격리하고 GC를 user I/O와 분리하는 단위.
- **Ghost cache**: 실제 데이터 없이 속성(reaccess number, reuse distance)만 저장하는 캐시. zone group selector가 hot chunk를 저비용으로 식별.
- **guess-and-verify**: round-robin 가정으로 zone↔channel을 추정한 뒤 GC 중 latency spike와 vote로 온라인 보정하는 메커니즘.
- **OOB (Out-Of-Band) 영역**: flash 페이지에 딸린 보조 영역(ECC용). BIZA가 매핑 항목을 data write에 hitchhiking으로 기록해 별도 persist 없이 crash consistency 확보.

## 강점 · 한계 · 열린 질문
- **강점**: 호환성(block interface 유지)과 cross-layer 최적화(endurance/throughput/tail 동시 개선)를 모두 잡음. 하드웨어 수정 불필요, 상용 ZN540에서 즉시 동작. ghost-cache·guess-and-verify로 매핑 불확실성·작은 ZRWA라는 실제 제약을 우회. OOB hitchhiking으로 매핑 persist WA 회피.
- **한계**: reuse distance가 ZRWA(56MB)보다 큰 워크로드(tencent 90.2%)는 ZRWA 흡수가 어려워 WA 감소 효과가 작음. round-robin 매핑 가정에 의존 — non-round-robin SSD에서는 추정 정확도 불확실. RAID5 중심 평가(RAID6는 확장 주장만). guess-and-verify가 GC 중 latency spike에 의존하므로 GC가 드문 워크로드에서 보정 기회가 적을 수 있음.
- **열린 질문**: 미래 ZNS SSD가 CQE에 zone↔channel 매핑을 명시하면 guess-and-verify가 불필요해지는가? CXL coherence로 write pointer 동기화 시 intra-zone parallelism을 더 끌어낼 수 있는가? small-zone ZNS SSD로의 일반화 시 성능은? ZRWA를 일반 cache 알고리즘(FIFO 등)으로 다룰 때 hit rate가 더 좋아지는가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. BIZA가 풀려는 핵심 인터페이스 딜레마는 무엇인가?
> > 상위 SW 호환성을 위한 block interface 노출과, SSD 내부 제어(GC·placement)를 통한 cross-layer 최적화는 상충한다. block-interface AFA는 호환되나 endurance/tail이 나쁘고, ZNS-interface AFA(RAIZN)는 최적화 가능하나 sequential-only라 호환 안 된다. BIZA는 내부는 ZNS, 외부는 block으로 둘을 동시에 잡는다.

> [!question]- Q2. dm-zap+RAIZN 단순 결합이 실패하는 세 가지 이유는?
> > (1) endurance: 수명 다른 data가 같은 zone에 섞여 GC 시 valid data 이주로 WA 33.3% 증가. (2) throughput: zone당 in-flight write 1개(dm-zap)·centralized parity zone(RAIZN)으로 이상치의 47.7%만. (3) tail latency: GC가 user I/O와 같은 channel에서 충돌해 99.99th가 10.3배 악화.

> [!question]- Q3. ZRWA가 WA 감소에 쓰이는 원리와 그 한계는?
> > ZRWA는 write pointer 뒤 영역에서 in-place update를 허용하는 SSD write buffer 추상화로, 자주 갱신되는 data·partial parity를 흡수해 flash flush를 막아 WA를 줄인다. 한계는 크기(ZN540 zone당 1MB, 최대 14MB) — reuse distance 14MB 미만 data가 17%뿐이라 선별 없이는 83%가 흡수 안 된다.

> [!question]- Q4. zone group selector는 어떤 chunk를 ZRWA에 남기려 하며 어떻게 식별하는가?
> > 자주 갱신되고(high reaccess number) reuse distance가 짧은(high-profit) chunk를 ZRWA에 남기려 한다. ghost cache 3단(LRU→HR→HP)으로, reaccess number>T1이면 HR로, reuse distance<T2(ZRWA 2배)이면 HP로 승격하고 obsolete chunk와 다른 zone group에 격리한다.

> [!question]- Q5. zone↔I/O channel 매핑이 숨겨진 것이 왜 문제이며 BIZA는 어떻게 푸는가?
> > ZNS SSD는 wear leveling 때문에 매핑을 숨겨, GC를 user I/O와 다른 channel로 보내려는 GC avoidance가 잘못된 zone을 고를 수 있다. BIZA는 대부분 상용 SSD가 round-robin이라는 점으로 1차 추정 후, GC 중 특정 zone의 latency spike 반복을 vote로 집계해 최다 vote channel로 온라인 보정한다(guess-and-verify).

> [!question]- Q6. ZRWA-aware I/O scheduler가 필요한 이유와 동작은?
> > I/O reorder가 발생하면 destination이 write pointer보다 뒤처져 ZRWA write가 실패할 수 있다. scheduler는 zone마다 bitmap+sliding window로 ZRWA 위치를 추적해, window 내 block 대상 request만 전송하고 나머지는 보류한다. write 완료 시 bit clear, 최좌측 bit가 0이면 window를 우측 shift한다. REPORT command 남발을 피한다.

> [!question]- Q7. 매핑 테이블 crash consistency를 어떻게 WA 없이 확보하는가?
> > BMT/SMT 항목(40bit LBN + 32bit SN)을 페이지당 72bit OOB 영역에 data write와 같은 flash programming으로 hitchhiking 기록한다. OOB는 ECC용으로 어차피 쓰이므로 매핑 테이블을 따로 persist할 때 생기는 추가 WA가 없다.

> [!question]- Q8. BIZA의 정량 성과를 한 줄로 요약하면?
> > SOTA AFA 대비 write amplification 42.7% 감소, write throughput 93.2% 향상, 99.99th tail latency 62.8% 감소이며, 4 ZNS SSD throughput 이상치의 92.2%를 달성한다.

## 🔗 Connections
[[ZNS]] · [[SOSP]] · [[2024]]

## References worth following
- **RAIZN** (Kim et al., ASPLOS 2022, [45]): ZNS SSD로 AFA를 구성하는 대표 연구. BIZA의 핵심 비교군이자 centralized parity zone 한계의 출발점.
- **ZNS: Avoiding the Block Interface Tax** (Bjørling et al., USENIX ATC 2021, [5]): ZNS 인터페이스의 동기·이점을 정립한 기반 논문.
- **dm-zap** (Western Digital, [13]): block↔ZNS adapter. BIZA가 통합·대체하려는 대상.
- **eZNS** (Min et al., OSDI 2023, [66]): inter-zone parallelism을 단순 다중 zone write로 활용 — BIZA가 channel 매핑 확인 없이는 부족하다고 비판하며 발전.
- **LSM_ZGC / ZenFS** (Choi et al. HotStorage [12] / Bjørling [5]): ZNS에서 수명 유사 data를 모아 WA를 줄이는 app-특화 기법 — BIZA는 범용 설계로 대비.
- **NVMe ZNS Command Set / TP 4076b** ([70][71]): ZRWA·APPEND 등 BIZA 구현이 의존하는 표준 명세.

## Personal annotations
<본인 메모 영역>
