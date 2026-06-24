---
title: "Nemo: A Low-Write-Amplification Cache for Tiny Objects on Log-Structured Flash Devices"
aliases: [Nemo]
description: "작은 객체(tiny object)용 플래시 캐시에서 set-associative 매핑의 hash collision 확률을 높여 set fill rate를 끌어올리고 application-level write amplification을 1.56배 수준으로 낮춘 설계"
venue: ASPLOS
year: 2026
arxiv: "2603.09605"
tier: deep
status: done
tags:
  - paper
  - cluster/kvlsm
  - topic/caching
  - topic/write-amplification
  - topic/log-structured
  - topic/flash
  - venue/asplos
  - year/2026
---

# Nemo: A Low-Write-Amplification Cache for Tiny Objects on Log-Structured Flash Devices

> **ASPLOS 2026** · cluster/kvlsm · Source: [Nemo - A Low-Write-Amplification Cache for Tiny Objects on Log-Structured Flash Devices.pdf](<Nemo - A Low-Write-Amplification Cache for Tiny Objects on Log-Structured Flash Devices.pdf>)

**저자:** Xufeng Yang, Tingting Tan, Jingxin Hu, Congming Gao (교신저자), Mingyang Liu, Tianyang Jiang, Jian Chen, Linbo Long, Yina Lv, Jiwu Shu — Xiamen University, Chongqing University of Posts and Telecommunications, Tsinghua University, OpenHarmony Community

---

## TL;DR

수백 바이트 이하의 작은 객체(tiny object)를 다루는 플래시 기반 KV 캐시는 SSD device-level write amplification(DLWA)이 낮은 log-structured SSD(ZNS, FDP)를 써도 application-level write amplification(ALWA)이 여전히 높다. SOTA인 FairyWREN조차 Twitter trace에서 ALWA가 15배를 넘는데, 이 논문은 그 근본 원인이 **log-to-set 마이그레이션 시 set fill rate가 너무 낮다(약 7%)**는 점임을 이론·실측으로 규명한다. Nemo는 hash space를 작게 만들어 hash collision 확률을 일부러 높이고, 다수의 set을 묶은 **Set-Group(SG)** 단위로 batched flush/eviction을 수행해 평균 set fill rate를 89% 이상으로 끌어올린다. 동시에 메모리 비용을 낮추기 위해 exact per-object 매핑 대신 **Parallel Bloom Filter Group(PBFG)** 기반 근사 인덱싱, on-demand 메타데이터 offloading, 1-bit hybrid hotness tracking을 도입한다. 결과적으로 ALWA 1.56배, object당 메모리 8.3 bits로 FairyWREN 대비 플래시 쓰기를 9배(최대 90%) 줄인다.

---

## 문제 & 동기

- 현대 데이터센터의 KV 캐시는 TikTok 댓글, 트윗 등 수백 바이트 이하의 **작은 객체**를 수백만 단위로 다룬다(p.1, p.2). 메모리 가격 한계로 SSD를 캐시 매체로 쓰는 flash-based cache로 전환되고 있다.
- 플래시 캐시는 전통적 메모리 캐시에 없던 두 축을 추가로 최적화해야 한다(p.2):
  - **메모리 오버헤드**: tiny object일수록 인덱스 엔트리 수가 급증해 메모리 비용 상승.
  - **write amplification(WA)**: 객체 크기와 플래시 쓰기 단위(보통 4KB)의 불일치로 device wear 가속. WA는 **ALWA**(application-level, 데이터 관리 메커니즘이 유발하는 재기록)와 **DLWA**(device-level, NAND block erase·garbage collection이 유발) 두 source로 나뉜다.
- ZNS/FDP 같은 log-structured SSD는 application-managed 데이터 배치를 디바이스 친화적 쓰기 패턴과 정렬시켜 DLWA를 최소화한다. 하지만 log-structured **캐시**의 ALWA는 여전히 크다.
- SOTA인 **FairyWREN**(OSDI '24, Kangaroo의 후속)은 Twitter trace(평균 246B)에서 상용 ZNS SSD(ZN540) 기준 ALWA가 **15배 초과**(p.2). 저자들은 오픈소스 재현 중 HLog→HSet 마이그레이션 로직의 주요 버그 2건을 발견·수정했고, 원논문은 대부분 객체가 HSet에 도달하지 못해 WA가 과소보고되었으며 수정 후 동일 워크로드에서 15배 초과를 관측했다(p.4).
- 근본 원인은 set-associative 매핑이 큰 hash space를 쓸 때 한 번에 한 set만 flush되어 **write batching이 비효율(set fill rate ≈ 7%)**, 이것이 전체 ALWA의 약 14.20을 차지한다는 것(p.2).

> [!quote]- 원문 (p.1)
> "Even when deploying advanced log-structured flash devices (e.g., Zoned Namespace SSDs and Flexible Data Placement SSDs) with low device-level write amplification, application-level write amplification still dominates."
>
> "This work proposes Nemo, which enhances set-associative cache design by increasing hash collision probability to improve set fill rate, thereby reducing application-level write amplification." (p.1)

> [!quote]- 원문 (p.2)
> "For instance, FairyWREN, a state-of-the-art (SOTA) solution for tiny objects, exhibits an ALWA exceeding 15× when processing Twitter traces (average object size: 246 bytes) on a commercial ZNS SSD (ZN540)." (p.2)
>
> "flushing only a small number of tiny objects into a single set at a time results in inefficient write batching (i.e., a low set fill rate ≈7.0% based on our evaluation), contributing ≈14.20 of the total ALWA." (p.2)

---

## 핵심 통찰

1. **set fill rate가 ALWA를 지배한다.** Log-to-Set Write Amplification(L2SWA)은 `set size / E(set당 새로 쓰인 객체 총 크기)`로 정의된다(Eq. 3, p.5). hash range가 클수록 collision 전 객체가 분산되어 set당 새 객체가 적고 fill rate가 낮아진다. **Observation 1**: L2SWA(P)는 HLog 페이지 수와 HSet의 set 개수(=hash range)에만 의존하고 active migration과 무관(p.6).
2. **active migration이 passive보다 WA가 약 2배.** Case 3(active object migration)은 객체 lifespan을 절반으로 줄여 hash bucket당 객체 수를 감소시키므로 L2SWA(A) ≈ 2·L2SWA(P)(**Observation 3**, p.6). 따라서 `L2SWA = (2−p)·L2SWA(P)`로 단순화(Eq. 7).
3. **OP ratio가 p를 결정한다.** OP가 5%→50%로 오르면 active migration이 점차 사라지고 L2SWA(A)도 소멸(**Observation 4**, p.6). 하지만 HLog를 키우거나 HSet OP를 늘려도 메모리·플래시 활용도 희생만 클 뿐 near-minimal WA에는 못 미친다(**Observation 2**). 결국 WA를 줄이려면 **hash range를 작게(=collision 확률↑) 만들어 fill rate를 높이는** 것이 핵심(p.6).
4. **단기 hash skew가 fill rate를 떨어뜨린다.** SG가 비어 있다 채워지기 시작하면, 어떤 set이 처음 가득 찬 시점에 SG 내 나머지 set들의 fill rate는 25% 미만(대개 40% 미만)에 머문다(Fig. 8, p.8). 이 short-term skew를 평탄화하면 평균 fill rate를 7%→89% 이상으로 올릴 수 있다(p.2).
5. **index cache가 hotness를 암시적으로 인코딩한다.** 최근 접근된 객체의 PBFG가 캐시에 남는 경향이 있어, low-bit access counter와 결합하면 메모리 오버헤드를 크게 줄이며 hotness를 추론할 수 있다(p.7).

> [!quote]- 원문 (p.6)
> "When combining with Observations 2 and 4, this formulation shows that enlarging HLog or adjusting HSet's OP ratio still fails to achieve near-minimal flash writes, even at the cost of high memory and low flash utilization. A closer look at the formula indicates that, since HLog must remain small, write amplification is mainly driven by the large number of sets in HSet, i.e., a large hash range. The large hash range implies a lower probability of hash collisions before migration, which results in less newly written objects per set write, hereafter referred to as a low per-set fill rate." (p.6)

---

## 설계 / 메커니즘

### 전체 구조 (Fig. 7, p.7)

- **Set-Group (SG)**: set-associative 레이아웃을 논리적으로 묶은 단위. SG 크기는 device erase unit의 배수로 설정 가능. in-memory의 mutable SG가 객체를 모으고, 가득 차면 immutable SG가 되어 FIFO로 관리되는 on-flash SG pool로 flush된다. Nemo의 WA = SG들의 aggregate fill rate의 역수 `WA(Nemo) = 1 / E(FR_SG)` (Eq. 9, p.7).
- 두 write source(log buffering, log-to-set migration)를 **하나의 high-fill-rate sequential write로 통합** — set-associative caching의 논리적 장점을 유지하면서 RMW 오버헤드를 제거하는 "fourth class of flash cache design"(p.7).
- 메모리 측 3대 컴포넌트: Buffer Area(in-memory SG + Index Group), Index Cache(PBFG들), Hotness Tracker(access counter). 플래시 측: SG Pool + Index Pool.

### Operations (p.7)
1. **Insert**: 키를 해싱해 in-memory SG 내 set 위치를 찾아 버퍼링하고 in-memory index group의 BF를 갱신. 버퍼가 차면 on-flash pool로 flush.
2. **Lookup**: 먼저 in-memory SG의 intra-SG offset 확인 → 없으면 index cache의 PBFG 조회 → in-memory면 candidate SG들을 병렬 접근, 없으면 index pool에서 가져와 캐시.
3. **Eviction**: on-flash pool이 가득 차면 가장 오래된 SG를 evict. 이때 hot object는 in-memory SG로 write-back.

### C1: "Perfect" SG 준비 (§4.2, p.8)
short-term hash skew 완화를 위해 SG flush 시점을 전략적으로 지연하는 3가지 기법:
- **Buffered in-memory SGs**: 여러 in-memory SG를 circle queue로 구성. 새 객체는 queue front에 가까운 SG에 삽입, rear SG가 거의 차면 flush. flush/insert를 decouple해 blocking 없이 underutilized set이 객체를 더 흡수.
- **Probabilistic flushing**: 확률 파라미터로 flush를 추가 지연(단, 구현은 **count-based threshold** `p_th=4096`, p.10 Table 3). flush를 미루는 대신 hash로 정해진 set의 기존 객체를 evict해 새 객체 공간 확보. p_th=0.001이면 ~1000회 flush 시도 중 1회만 즉시 flush.
- **Hotness-aware writeback during SG eviction**: 캐시가 거의 차면 매 in-memory SG flush가 가장 오래된 on-flash SG의 eviction을 trigger. evicted SG의 hot object를 to-be-flushed in-memory SG로 re-insert해 set을 더 채우면서 hot data를 캐시에 유지.
- 세 기법 결합으로 fill rate **89.34%** 달성(p.8).

### C2: Lightweight Index Structure (§4.3, p.9)
- exact 매핑은 SG ID(10+bits), next pointer(32+bits), tag로 40+bits/obj 필요. Nemo는 **PBFG**(다수 Bloom filter 묶음)로 근사 인덱싱. BF 공간 효율은 target false-positive rate에만 의존(객체 수 무관).
- **SG-level BF → Set-level BF로 분해**: 같은 intra-SG offset의 set-level BF들을 서로 다른 SG에서 묶어 **Set-level PBFG** 구성. Lookup은 모든 set-level BF를 병렬 조회.
- **on-demand caching**: PBFG hotness = 멤버 객체들의 aggregate access frequency. Zipfian 분포라 set 접근도 highly skewed → 인기 PBFG만 메모리 유지.
- **Packed BF layout**: 여러 SG의 set-level BF를 한 flash page에 packing해, set-level PBFG retrieval 시 1페이지만 읽도록 정렬(Fig. 10, p.9). 4KB 페이지에 수백 개 set-level BF 수용.
- 1% false-positive 시 object당 9.6 bits, 0.1% 시 14.4 bits(p.7, p.10).

### C3: Hybrid Hotness Tracking (§4.4, p.9)
- index cache의 **recency**(cached PBFG)와 access counter의 **frequency**를 결합. 객체가 hot = access frequency 높고 + 최근 활성(cached PBFG와 연관).
- access counter는 **single-bit bitmap**. cached PBFG에 속한 set의 비트만 검사. victim SG eviction 시 hot object는 re-insert, cold object는 evict(Fig. 11, p.9).
- **periodic cooling**: 캐시 10% write마다 access counter 리셋(Table 3). hotness 추적을 객체 lifespan의 **later 30%**에서만 수행해 tracking 메모리 오버헤드 최소화(p.10).

> [!quote]- 원문 (p.7)
> "Specifically, Nemo consolidates two write sources into a single, high fill-rate sequential write, eliminating log-to-set migration and minimizing write amplification. It preserves the logical benefits of set-associative caching while adopting log-structured physical writes, preventing the read-modify-write overhead of traditional set-associative design." (p.7)

> [!quote]- 원문 (p.8)
> "Based on these three techniques, Nemo gradually constructs a 'perfect' SG, characterized by a high fill rate of 89.34%, which allows Nemo to achieve low WA and high cache utilization." (p.8)

---

## 평가

### 환경 (§5.1, p.10)
- 24-core Intel CPU, 128GB RAM, Ubuntu 22.04 (Linux 5.15), **WD Ultrastar DC ZN540 ZNS SSD**(zone당 1077MB), libzbd 사용.
- CacheLib 모듈로 구현. SG size = 4KB set, **275,712 sets/SG**, in-memory SG 2개, flushing threshold p_th=4096, cached PBFG ratio 50%, BF false positive 0.1%(=14.4 bits/obj), SG cooling 10% write마다(Table 3).
- Baselines: Log, Set, **FairyWREN(FW)**, **Kangaroo(KG)** — 모두 CacheLib에 구현, FW는 버그 수정 버전, KG는 ZNS 지원 추가(p.10).
- Twitter traces 4종(cluster 14/29/34/52, Zipf α 1.14~1.30, WSS >8GB), 4종을 동시 replay·merge해 working set 확대. cluster 14/29는 평균 객체 2×/3× downscale → 평균 246B(FW/KG는 265B/271B)(p.10~11).

### Write Amplification (Fig. 12, p.11)
- 정상 상태 WA: **Nemo 1.56**, Log 1.08, FW 15.20, Set 16.31, **KG 55.59**(Fig. 12a). KG는 cold-hot partitioning 부재 + GC와 log-to-set migration이 독립적으로 곱셈적 증가.
- Nemo는 **FW 대비 플래시 쓰기 9× 감소**, 메모리 8.3 bits/obj, OP ratio <1%(p.11).
- FW의 WA를 줄이려 HLog 5%→20%(메모리 73%↑), HSet OP 5%→50%(유효 캐시 거의 절반)로 키워도 Log20-OP5는 4.12, Log5-OP50은 6.56으로 여전히 Nemo보다 훨씬 높음(Fig. 12b, p.11).
- WA trend(Fig. 14): FW는 HLog만 활성일 때 1.1로 시작하나 operation 증가 시 passive migration trigger로 급증, active migration 시 second turning point. Nemo는 약 1.56으로 안정.
- Flash write pattern(Fig. 13): Nemo는 간헐적 소량 쓰기 + batched write. FW/KG는 연속 쓰기, KG의 분당 flash write가 FW보다 훨씬 높음.

### Read Latency (Fig. 15, p.11~12)
- Nemo가 p50/p99/p9999 모두 FW보다 우수·안정. p50은 Nemo가 5μs 우위.
- tail: Nemo p99 ≈131μs, p9999 ≈523μs로 안정. FW는 p99가 ~350μs 부근, p9999가 ~1488μs로 크게 fluctuate. FW의 빈번한 4KB write가 후속 read를 교란하기 때문(p.11~12).
- Read amplification은 Nemo가 FW의 3배 초과지만(모든 read 활동 집계) lookup latency는 낮고 안정 — read 병렬화 + write 패턴의 read 간섭 적음 때문(p.13).

### Miss Ratio (Fig. 16, p.12)
- Nemo와 FW의 miss ratio는 유사. hotness-aware writeback이 hot object를 캐시에 유지하고 hot data working set이 캐시 공간보다 작음.

### Design Breakdown (Fig. 17, §5.3, p.12)
- fill rate 기여: naïve **6.78%** → Buffered SG(B) **31.32%** → Probabilistic(P) **36.77%** → B+P **64.13%** → B+P+W(전체) **89.34%**(W가 추가로 25.21% 개선).

### Sensitivity & Overhead (§5.4~5.5, p.12~13)
- p_th 증가 시 새 객체 수는 늘지만 diminishing return: sacrificed objects 64→1024(16×)에 새 객체는 2×만 증가(Fig. 18).
- intra-SG offset 접근: hash 후에도 ~70% hot 접근이 top 30% hot set에 집중(Fig. 19a). 모든 in-memory PBFG ratio에서 <15% 요청만 flash PBFG 필요, 50% in-memory ratio면 <8%(Fig. 19b).
- **메모리 오버헤드(Table 6, p.13)**: Nemo **8.3 bits/obj** = set index 7.2(50%만 메모리 유지) + hotness 0.3(마지막 30%만 1-bit) + SG/index group buffer 0.8. vs FairyWREN 9.9, Naïve Nemo 30.4.
- PBFG 계산 오버헤드: candidate SG 계산 ~1μs(전체 lookup의 수십~수백μs 대비 negligible)(p.13).

> [!quote]- 원문 (p.11)
> "Under the default configuration, Nemo achieves a WA of 1.56. Nemo reduces flash writes by 9× compared to FW with a memory overhead of 8.3 bits/obj and requires less than 1% OP ratio." (p.11)

---

## 섹션 노트

- **§2 Background**: KV 캐시의 deletion은 user-driven, eviction은 캐시가 공간 소진 시 → 캐시가 무엇을 저장할지 더 유연(p.3). 기존 캐시 3분류 비교(Table 1): log-structured(WA 좋음, 메모리·flash util 나쁨), set-associative(메모리 좋음, WA·flash util 나쁨), hierarchical(WA 나쁨). Nemo는 세 축 모두 양호.
- **§3 Motivation**: hierarchical 캐시 WA의 3 source — Case 1(memory→front tier flush, 4KB 미정렬), Case 2(HLog→HSet, RMW dominant = L2SWA), Case 3.x(GC, FairyWREN은 GCWA를 L2SWA로 folding). Case 2/3가 L2SWA 구성(Fig. 3, p.4~5). FairyWREN WA = `1/E(FR_i) + L2SWA` (Eq. 1)에서 첫 항은 ~1, L2SWA가 지배.
- **§3.2 정량화**: L2SWA(P) = `(1−X)·N_Set / (2 N_Log)` (Eq. 6). 실측 L2SWA 14.2, p≈25%로 이론값 15.75와 근접. 9개 변수 정리(Table 2).
- **§6 Discussion**: SSD/NVMe 인터페이스 특별 요구 없음(coarse-grained FIFO write). large-zone ZNS(ZN540)는 SG=zone, small-zone(PM1731a, 96MB)는 SG=여러 zone, FDP는 여러 SG=reclaim unit. **한계**: index 정확도↑가 read amp↓를 항상 보장 안 함(더 큰 BF → index pool scatter → flash read↑), Appendix A에서 trade-off 모델링. hotness tracking의 "free-riding" — group으로 묶인 cold object가 hot으로 잘못 유지될 수 있음(p.13).
- **§7 Related Work**: flash cache 3분류(log-structured / set-associative / hierarchical) 대비 Nemo는 set-associative 레이아웃을 log-style batching에 통합 + on-demand PBFG 인덱싱.
- **Appendix A**: PBFG retrieval 비용 모델. flash access cost ≈ `min(N·o/s + 1 + (N−1)x)` (Eq. 10). BF false positive 0.1% → object-related flash access ~1+0.35, 0.01%로 낮추면 PBFG retrieval 7→9 페이지 증가하나 false positive는 1+0.03으로 감소 — total은 오히려 증가. 즉 optimal accuracy 존재(p.16). N=350 SGs.

---

## 핵심 용어

- **Tiny object**: 수백 바이트 이하(주로 ≤200B)의 작은 KV 객체.
- **ALWA / DLWA**: Application-Level / Device-Level Write Amplification. 데이터 관리 메커니즘이 유발하는 재기록 / NAND block erase·GC가 유발하는 재기록.
- **L2SWA (Log-to-Set Write Amplification)**: log tier→set tier 마이그레이션 시 `set size / 새로 쓰인 객체 평균 총 크기`. passive migration(P)와 active migration(A)로 구분.
- **Set fill rate (FR)**: set이 flush될 때 실제로 채워진 비율. Nemo의 WA = 1/E(FR).
- **Set-Group (SG)**: Nemo의 핵심 단위. 다수 set을 묶어 batched flush/eviction하는 mutable→immutable in-memory/on-flash 구조.
- **PBFG (Parallel Bloom Filter Group)**: exact 인덱스를 대체하는 근사 인덱싱. 병렬 조회로 candidate SG 식별. SG-level → Set-level로 분해.
- **OP (Over-Provisioning) ratio**: GC를 위해 예약된 HSet 비율. Nemo는 OP를 X로 재정의해 usable set = (1−X)N_Set.
- **Hotness-aware writeback**: evict되는 on-flash SG의 hot object를 in-memory SG로 re-insert.
- **ZNS / FDP SSD**: Zoned Namespace / Flexible Data Placement. application-managed 배치를 디바이스에 정렬해 DLWA 최소화하는 log-structured SSD.

---

## 강점 · 한계 · 열린 질문

**강점**
- "collision을 일부러 늘려 fill rate를 높인다"는 역발상이 명확한 이론(Eq. 1~9, Observation 1~4)으로 뒷받침되고, 실측(p≈25%, L2SWA 14.2 vs 이론 15.75)과 일치.
- SOTA FairyWREN 재현 시 발견한 버그를 투명하게 보고하고 수정 후 재평가(WA 과소보고 → 15× 초과).
- WA 1.56(near-optimal 1.56), 메모리 8.3 bits/obj, 9× flash write 감소, read tail latency 안정성까지 균형 달성. CacheLib 실장 + 실제 ZNS SSD 평가, 아티팩트 공개(Apache/CC-BY-4.0).

**한계**
- Read amplification이 FW의 3배 초과(병렬화·write 간섭 적음으로 latency는 상쇄하나 절대 read 횟수는 증가)(p.13).
- 근사 인덱싱(PBFG)의 정확도↑가 read amp↓를 보장 못 함 — BF 정확도와 index pool scatter 사이 trade-off 존재.
- hotness tracking의 "free-riding": PBFG group 단위로 묶여 cold object가 hot으로 오인되어 eviction 정확도 저하 가능.
- ZNS SSD 미보유 시 완전 재현 불가(아티팩트 명시).

**열린 질문**
- PBFG 정확도-read amp의 optimal point는 PBFG에 특화된 모델(Appendix A)인데, 다른 근사 인덱싱 구조에선 어떻게 일반화되나?
- flash capacity scaling 시 N 증가로 read amp 상승 → partitioning으로 완화한다는데, partition 수가 캐시 hit ratio/메모리에 미치는 영향은?
- 89.34% fill rate가 Zipf α나 객체 크기 분포가 크게 다른 워크로드(예: 균등 접근)에서도 유지되나?

---

## ❓ Q&A (자가 점검)

> [!question]- 1. log-structured SSD(ZNS/FDP)를 쓰면 DLWA가 낮아지는데, 왜 플래시 캐시의 WA가 여전히 문제인가?
> ZNS/FDP는 device-level WA(DLWA, NAND erase·GC 유발)를 최소화하지만, 데이터 관리 메커니즘이 유발하는 **application-level WA(ALWA)**는 줄이지 못한다. tiny object에서는 log-to-set 마이그레이션 시 set fill rate가 ~7%로 낮아 ALWA가 지배적이며, FairyWREN조차 ALWA 15× 초과(p.1~2).

> [!question]- 2. Nemo의 가장 반직관적인 핵심 아이디어는 무엇인가?
> hash space를 **작게** 만들어 hash collision 확률을 일부러 **높인다**. collision이 많아지면 set당 모이는 객체가 늘어 flush 시 set fill rate가 올라가고, 결국 L2SWA(=set size / 새 객체 총 크기)가 낮아진다. 큰 hash range가 낮은 fill rate의 원인이라는 Observation에서 도출(p.6).

> [!question]- 3. Set-Group(SG)이 기존 set-associative 캐시와 다른 점은?
> SG는 다수 set을 묶어 **batched flush/eviction**하는 단위로, 두 write source(log buffering + log-to-set migration)를 하나의 high-fill-rate sequential write로 통합한다. set-associative의 논리적 장점은 유지하되 log-structured 물리 쓰기를 채택해 traditional set-associative의 RMW 오버헤드를 제거한다("fourth class of flash cache design", p.7).

> [!question]- 4. "perfect SG"(fill rate 89.34%)는 어떤 3가지 기법으로 만들어지나?
> (1) Buffered in-memory SGs — circle queue로 flush/insert decouple해 underutilized set이 더 흡수, (2) Probabilistic(실제론 count-based, p_th=4096) flushing — flush를 지연하고 hash 기반 evict로 공간 확보, (3) Hotness-aware writeback — evicted SG의 hot object를 to-be-flushed SG로 re-insert. breakdown: 6.78%→B 31.32%→P 36.77%→B+P 64.13%→B+P+W 89.34%(Fig. 17, p.12).

> [!question]- 5. exact per-object 인덱스가 40+bits/obj인데 Nemo는 어떻게 메모리를 줄이나?
> exact 매핑(SG ID 10+bits + next pointer 32+bits + tag) 대신 **PBFG**(Bloom filter 묶음) 근사 인덱싱을 쓴다. BF 공간 효율은 객체 수가 아닌 target false-positive rate에만 의존. SG-level BF를 Set-level BF로 분해해 병렬 조회하고, hot PBFG만 메모리에 on-demand 캐싱(Zipf skew 활용), packed BF layout으로 retrieval을 1페이지로. 결과 0.1% FP 시 14.4 bits/obj(p.7, p.9).

> [!question]- 6. hybrid hotness tracking은 왜 메모리를 절약하나?
> index cache의 **recency**(cached PBFG = 최근 활성)와 single-bit access counter의 **frequency**를 결합하고, hotness 기록을 객체 lifespan의 **마지막 30%**에서만 수행한다. cached PBFG에 속한 set의 비트만 검사하므로 fine-grained per-object counter가 불필요 → object당 hotness 비용이 0.3 bits로 떨어진다(p.10, Table 6).

> [!question]- 7. 평가에서 Nemo가 FairyWREN 대비 보인 핵심 수치는?
> WA 1.56 vs FW 15.20(KG 55.59), 즉 **flash write 9× 감소**. 메모리 8.3 bits/obj(FW 9.9), OP ratio <1%, p99/p9999 read latency 안정(FW는 ~350μs/~1488μs로 fluctuate, Nemo ~131μs/~523μs), miss ratio는 FW와 유사(p.11~12).

> [!question]- 8. FairyWREN 재현 과정에서 저자들이 발견한 것은?
> HLog→HSet 마이그레이션 로직의 **major bug 2건**(+ minor) 때문에 대부분 객체가 HSet에 도달하지 못해, 원논문에서 HSet 기여가 과소보고되고 WA가 실제보다 낮게 측정되었다. 버그 수정 후 동일 워크로드에서 WA가 **15× 초과**로 관측됨(p.4).

---

## 🔗 Connections

[[KV-LSM]] · [[ASPLOS]] · [[2026]]

관련/유사 논문:
- [[FairyWREN]] — 직접 비교 대상이자 본 논문의 SOTA baseline (OSDI '24, set-associative + log-to-set migration with RMW folding)
- [[Kangaroo]] — FairyWREN의 전신, hierarchical tiny-object flash cache (SOSP '21)
- [[CacheLib]] — Nemo가 모듈로 구현된 프레임워크, set-associative tiny-object 캐시 엔진 (OSDI '20)
- [[Flashield]] — flash write amplification 제어 hybrid KV cache (NSDI '19)
- [[Cachesack]] — Google datacenter flash cache의 admission optimization (USENIX ATC '22)
- [[ZNS]] — Zoned Namespace SSD, Nemo의 평가 디바이스이자 DLWA 최소화 인터페이스 (USENIX ATC '21)
- [[FDP]] — Flexible Data Placement SSD, Nemo 호환 log-structured 인터페이스

## References worth following

- McAllister et al., **FairyWREN: A Sustainable Cache for Emerging Write-Read-Erase Flash Interfaces**, OSDI '24 [17] — 직접 비교·이론 분석 기반.
- McAllister et al., **Kangaroo: Caching Billions of Tiny Objects on Flash**, SOSP '21 [11] — hierarchical tiny-object 캐시의 출발점.
- Berg et al., **The CacheLib Caching Engine: Design and Experiences at Scale**, OSDI '20 [15] — set-associative tiny-object 캐시 + 구현 기반.
- Bjørling et al., **ZNS: Avoiding the Block Interface Tax for Flash-based SSDs**, USENIX ATC '21 [25] — log-structured SSD 인터페이스.
- samsung, **Flexible Data Placement (FDP) Whitepaper** [26] — FDP 인터페이스.
- Yang et al., **A Large-Scale Analysis of Hundreds of In-Memory Key-Value Cache Clusters at Twitter (Twitter cache-trace)**, TOS [27] — 평가 trace 출처·워크로드 특성.
- Qiu et al., **Can Increasing the Hit Ratio Hurt Cache Throughput?** [78] — fill-rate/throughput trade-off 관련.

## Personal annotations

<본인 메모 영역>
