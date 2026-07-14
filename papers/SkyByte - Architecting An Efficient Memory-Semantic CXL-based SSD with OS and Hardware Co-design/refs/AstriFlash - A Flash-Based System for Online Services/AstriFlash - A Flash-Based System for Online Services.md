---
title: "AstriFlash: A Flash-Based System for Online Services"
aliases: [AstriFlash]
type: paper-ref
venue: HPCA
year: 2023
tags:
  - paper
  - cluster/cxl
  - topic/flash-as-memory
  - topic/dram-cache
  - topic/latency-hiding
  - topic/hw-sw-codesign
  - topic/online-services
  - venue/hpca
  - year/2023
---

# AstriFlash: A Flash-Based System for Online Services

> **Source PDF**: [AstriFlash - A Flash-Based System for Online Services.pdf](<AstriFlash - A Flash-Based System for Online Services.pdf>)
> 🕸️ NodeGraph: [AstriFlash.html (새 탭에서 렌더링, push 후 유효)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/SkyByte%20-%20Architecting%20An%20Efficient%20Memory-Semantic%20CXL-based%20SSD%20with%20OS%20and%20Hardware%20Co-design/refs/AstriFlash%20-%20A%20Flash-Based%20System%20for%20Online%20Services/AstriFlash.html)
> **Authors**: Siddharth Gupta, Yunho Oh (Korea Univ), Lei Yan, Mark Sutherland, **Abhishek Bhattacharjee** (Yale), **Babak Falsafi** (EPFL EcoCloud), Peter Hsu
> **Venue / Year**: HPCA 2023
> **DOI**: 10.1109/HPCA56546.2023.10070955 · **Length**: 13 pages
> **Read status**: ☑ Full read (2026-07-14)
> **My reading purpose**: [[SkyByte]]의 **핵심 대조군(key competitor)**. 둘 다 "긴 flash/SSD 접근 지연을 다른 일로 전환(switch)해 은닉"하지만, AstriFlash는 **SSD를 black box·page 단위**로 두고 **user-level HW thread switch**로 숨긴다. SkyByte는 이를 명시적으로 반례로 들며 **OS↔SSD co-design·cacheline 단위·coordinated context switch**로 차별화한다. 이 대비를 정확히 잡아 발표축 "SSD 이중역할 × transparent co-design"에 배치하려 읽음.

---

## 📋 목차
- [TL;DR](#tldr)
- [Core thesis](#core-thesis)
- [Why this matters to me](#why-this-matters-to-me)
- [Structure overview](#structure-overview)
- [Section notes](#section-notes)
- [Key vocabulary](#key-vocabulary)
- [Citable quantitative data](#citable-quantitative-data)
- [🎯 Strategic anchor](#-strategic-anchor)
- [Connection to my research direction](#connection-to-my-research-direction)
- [Open questions / gaps](#open-questions--gaps)
- [References worth following up](#references-worth-following-up)
- [Personal annotations](#personal-annotations)

---

## TL;DR
AstriFlash는 datacenter online service가 값비싼 DRAM 대신 **50× 싸지만 1000× 느린(≈50µs) flash**를 main memory로 쓰게 하는 HW/SW **co-design**이다. 핵심은 (1) host DRAM을 **hardware-managed, set-associative cache**(flash 용량의 **3%**, 4KB page 단위)로 써 hot fraction을 담고 backing flash가 전체 dataset을 담는 2-tier 구조, (2) demand paging의 오버헤드를 **core-side(task/context switch)**와 **memory-side(page-table 수정·TLB shootdown)**로 나눠 **둘 다** 제거하는 것이다. memory-side는 OS를 빼고 DRAM-cache miss를 **accelerated hardware miss handler**(frontside/backside controller + in-DRAM **Miss Status Row**로 100s의 concurrent miss 추적)로 처리하고, core-side는 DRAM-cache miss 시 OS context switch(~5µs) 대신 **100ns user-level thread switch(switch-on-miss)**로 flash 접근을 다른 job과 overlap한다. OoO core가 committed store를 되돌릴 수 있도록 ASO speculation을 Store Buffer까지 확장하고, forward-progress bit·priority(aging) scheduler로 tail latency와 starvation을 관리한다. cycle-accurate full-system 시뮬(QFlex, 16× ARM A76) 결과 **DRAM-only 대비 throughput 95%**를 유지하면서 99%tile tail latency를 지키고 **memory cost를 20× 절감**한다.

---

## Core thesis
> "We propose AstriFlash, a hardware-software co-designed system that tightly integrates flash and DRAM to achieve DRAM-like performance with capacity and cost benefits of flash while maintaining the abstraction of virtual memory." (§I)
> "AstriFlash achieves better performance by addressing core-side and memory-side overheads synergistically." (§I)

online service의 병목은 flash의 물리 지연이 아니라 그것을 관리하는 **archaic OS demand paging**이다. paging 비용을 core-side/memory-side로 해부해 **양쪽을 동시에** — hardware DRAM cache(memory-side) + user-level switch-on-miss(core-side) — 제거하면 µs급 flash 접근을 **ns급 오버헤드**로 흡수해 DRAM-급 성능을 flash 가격으로 얻는다.

---

## Why this matters to me
AstriFlash는 [[SkyByte]]의 **가장 직접적인 대조군**이다. 두 논문은 같은 상위 아이디어 — "긴 저장장치 접근 지연을, 그 사이 다른 일로 **문맥을 전환(switch)**해 숨긴다" — 를 공유하지만 **어디를·어떻게** 전환하느냐가 정반대다. AstriFlash는 **SSD를 black box로**, PCIe로 **4KB page**를 통째로 당겨오고, 은닉은 **core 안 user-level HW thread switch(100ns)**로 한다. SkyByte는 SSD **컨트롤러와 OS를 함께 설계(co-design)**해 **cacheline 단위**로 접근하고 **cacheline write log**를 두며, 전환을 **OS↔SSD가 협응하는 coordinated context switch**로 만든다. SkyByte 논문이 AstriFlash를 "SSD를 black box·page granularity로 다룬다"고 명시적으로 비판하며 자기 차별점을 세우므로, 이 대비는 내 발표 서사에서 **"transparent co-design의 정도(degree)"** 축을 눈금 매기는 결정적 예다. 또 하나 중요한 사실: **AstriFlash의 Flash-Sync baseline이 바로 FlatFlash**다(§V) — FlatFlash(동기 대기, 27% throughput) → AstriFlash(switch, 95%)로 이어지는 계보가 문헌 안에 이미 새겨져 있다. 마지막으로 둘 다 **single-host** 설계라, 내 multi-node CXL coherence 방향이 채울 빈칸을 똑같이 드러낸다.

---

## Structure overview
| § | Title | Pages | Key takeaway |
|---|---|---|---|
| I | Introduction | p.1-2 | flash 50×싸고 1000×느림. paging 오버헤드를 core-side/memory-side로 분해, 둘 다 제거하는 co-design 제안. 3개 재설계(user-level thread, memory trap/SB speculation, in-DRAM miss table) |
| II | Flash-Integrated Hierarchies | p.2-3 | DRAM:flash 비율 분석(miss rate는 3% capacity에서 평탄, 64코어에 60GBps flash BW). programming abstraction(explicit I/O vs demand paging). paging의 core/memory-side 오버헤드 해부 |
| III | AstriFlash (key insights + design) | p.3-5 | insight① user-level thread로 core-side 제거(queuing: 단일서버→논리적 multi-server M/M/k), insight② accelerated miss handler로 memory-side 제거. DRAM cache page size 4KB, in-DRAM miss table |
| IV | Implementation | p.5-8 | flash addressing(PCIe BAR)·Midgard 주소변환, DRAM cache(frontside/backside controller, MSR), switch-on-miss HW/SW interface(handler/resume register, forward-progress bit), SB speculation(ASO 확장), user-level thread lib·priority scheduling(aging) |
| V | Methodology | p.9 | QFlex cycle-accurate full-system sim, 16× ARM A76, 256GB dataset/8GB(3%) DRAM cache, workloads(Silo·Masstree·ArraySwap·RBT·HashTable·TATP·TPCC), 비교 config 7종 |
| VI | Evaluation | p.10-11 | throughput 95%(DRAM-only 대비), service latency(Table II, Flash-Sync 대비 1.02×), tail latency(93% load에서 DRAM-only tail 매칭), GC 오버헤드 |
| VII | Related Work | p.11 | flash integration(FlatFlash·2B-SSD·FlashMap·SSDAlloc·PageSeer), emerging memory(NVM), user-level threading·killer microseconds |
| VIII | Conclusion | p.11 | switch-on-miss co-design으로 paging 제거, DRAM-급 성능·20× cost 절감 |

---

## Section notes

### §I Introduction (p.1-2)
동기: datacenter는 tail-latency 때문에 dataset을 DRAM에 두지만 DRAM은 비싸고 용량 확장이 안 된다. NAND flash는 **$/GB 50× 개선**·**지연 1000×(≈50µs)**. 많은 online service가 **ms급 SLO**라 몇 µs flash 접근을 흡수할 수 있고, 요청 분포가 **skewed**라 hot fraction만 DRAM에 두면 대부분 요청을 흡수한다 → 2-tier(DRAM cache + flash) 타당.

핵심 장애물은 flash 자체가 아니라 **demand paging**이다. paging은 원래 ms급 disk용이라, µs급 flash에는 오버헤드가 치명적. AstriFlash는 paging 오버헤드를 **core-side(task switch·context switch)**와 **memory-side(memory management)**로 나누고, DRAM을 **hardware-managed cache**(예: Intel Knights Landing)로 써 memory-side를, **user-level thread switch**로 core-side를 각각 제거한다. 세 가지 재설계가 필요:
1. µs급 stall을 흡수하는 **flexible user-level thread** switch-on-miss(기존 switch-on-miss는 ns급 memory stall·batch workload만 다룸).
2. µs급 DRAM-cache miss를 견디는 **memory trap** 재검토 — committed store를 OS 없이 Store Buffer에서 되돌리는 speculation.
3. **100s의 concurrent miss**를 값싸게 추적하는 **in-DRAM miss status table**(기존 DRAM cache 제안은 미지원).

> "AstriFlash efficiently absorbs the μs-scale flash latency by providing cross-stack integration with ns-scale overheads." (§I)

### §II Flash-Integrated Hierarchies (p.2-3)
**②A DRAM:flash 비율** — CloudSuite workload에서 DRAM miss ratio는 **DRAM이 flash의 ~3%**에서 평탄해진다. Eq.1로 flash BW 계산: `BW_Flash = BW_DRAM × MissRate × (PageSize/CacheBlockSize)`. 3% 기준 64코어에 **60GBps** 필요 → PCIe Gen5(128GBps)로 다중 SSD면 충족. 채택 구성: **1TB dataset in flash + 3%(32GB) DRAM cache**, flash가 50× 싸므로 1TB DRAM 대비 **memory cost 20× 절감**. 문제: 각 코어가 **5-25µs마다 DRAM miss** → µs급 paging이 병목.

**②B programming abstraction** — explicit I/O(programmer 관리, 영구 VA 불가)와 demand paging(투명, 그러나 성능 보장 어려움). AstriFlash는 stress test 위해 **programmer가 데이터 이동을 제어하지 않는(=투명한 virtual memory)** 쪽을 가정.

**②C demand paging 오버헤드 해부**:
- **memory-side**: page fault마다 OS가 NVMe 등으로 I/O 스케줄(storage stack ~10µs), page 이동 후 page table 갱신, TLB 일관성 위해 **global TLB shootdown**(broadcast, 코어 수에 안 스케일, **>10µs**).
- **core-side**: OoO가 50µs를 못 숨기므로 OS가 **page fault마다 context switch**로 asynchronous flash 접근 제공. context switch 하나가 **~5µs** core-side 오버헤드.

> "modern datacenter workloads have a DRAM miss every ∼10 μs per thread accompanied by 10 μs of page fault and context switch overhead." (§II.C)

### §III AstriFlash — key insights & design (p.3-5)
**insight① (core-side)**: online service의 응답 지연은 **queuing delay가 지배**한다. asynchronous flash + 경량 thread switch가 있으면 단일 물리 서버가 **논리적 multi-server queuing model(M/M/k)**처럼 동작 — pending flash를 기다리는 older request가 서버를 비워 younger request의 head-of-line blocking을 제거. 그래서 DRAM-only와 유사한 전체 응답 지연 유지(고부하일수록 효과 큼). 분석: ~10µs마다 flash 접근하는 앱이 DRAM-only의 **20% 이내**에 들려면 **평균 서비스 시간의 40× SLO** 필요.

**insight② (memory-side)**: OS demand paging은 SW(page table 수정)·HW(TLB shootdown broadcast) 동기화 때문에 **높은 paging 빈도에서 근본적으로 확장 불가**. flash를 memory-map하고 memory management를 **accelerated miss handler**(DRAM cache↔flash)에 캡슐화하면 paging 제거.

Figure 4는 3설계 비교: **(a) OS-Swap**(전통 paging), **(b) Flash-Sync**(동기 대기 = **FlatFlash**), **(c) AstriFlash**(switch-on-miss). Flash-Sync는 >80% throughput 손실, OS-Swap도 ~50% 손실.

**core-side design**: DRAM-cache miss 시 OoO는 miss를 낸 명령(예: Store Buffer의 committed store)을 **abort**하고 마지막 완료 명령 상태로 core를 되돌린 뒤 user-level scheduler 호출. 기존 memory trap 제안은 OS에 의존하나, ~10µs마다 miss라 **microarchitectural** 지원 필요. abort 가능성 때문에 명령은 직전 명령 완료 후에만 retire → 순차적(SC) 실행 강제 → 이를 만회하려 **post-retirement speculation(ASO 계열)** 사용. **user-level thread lib로 100ns 전환**(context switch 대비 50×, 최근 제안 대비 5× 빠름).

**memory-side design**: **hardware-managed DRAM cache**로 near-DRAM 용량 관리·데이터 이동, OS의 page-table 수정·TLB shootdown 제거. DRAM cache는 **4KB page granularity**(spatial locality; 64B면 32GB에 tag ~4GB로 비현실적, 4KB여도 64MB tag → SRAM 불가라 **tag를 DRAM에** 보관, serialized tag+data lookup). flash 지연이 길어 **100s의 concurrent miss** → SRAM MSHR 대신 **in-DRAM miss status table**.

### §IV Implementation (p.5-8)
**A. flash addressing & memory mapping**: PCIe **BAR**로 flash를 physical address space에 노출, OS가 page table로 VA→PA 매핑(PA ≡ SSD의 Logical Page Number). TB급 DRAM의 주소변환 문제는 **Midgard**[27](큰 cache 계층으로 변환 오버헤드 축소)로 잘 맞음. cold data는 page table도 flash에서 와야 하므로, **hybrid-DRAM(Knights Landing식)으로 DRAM을 flat+cached로 분할**해 page table을 항상 DRAM-resident로 유지(→ noDP 구성은 이걸 뺀 것).

**B. DRAM-cache organization**: 4KB page 단위 set-associative(각 DRAM row = 한 set, tag+data 필드). **Frontside Controller(FC)**: 전통 DRAM controller 확장, RAS로 row fetch → CAS로 tag 비교(각 tag 8B, 64B tag column = 8-way), hit면 data 반환·miss면 backside queue로. **Backside Controller(BC)**: **programmable(microcode/SW)**, flash에 4KB read 발행·victim eviction(dirty면 off-critical-path write-back)·miss 관리. **Miss Status Row(MSR)**: set-associative in-DRAM 구조(각 entry 8B, CAS로 검색), 중복 flash 요청 방지, 100s concurrent miss 저비용 추적.

**C. µs-scale switch-on-miss architecture**: FC가 miss signal을 core로(DRAM ECC error interface에 piggyback해 MSHR 해제). thread switch는 **Handler Address Register**(user-level handler VA, privileged로만 write)와 **Resume Register**(user mode R/W, miss 낸 명령 PC 저장)로 구현. ROB flush 후 PC를 handler로. **forward-progress bit**: 재스케줄된 thread가 또 miss나 재축출로 굶지 않도록, set 시 FC에서 **동기 완료 강제**(deadlock/starvation 방지). **precise exception & speculative store**: store가 SB에 있으면 기존 speculation으로 못 되돌림 → **ASO**[77]를 SB까지 확장(store당 PRF 4개 추가, 32-entry SB에 128 reg = 1KB SRAM). **core당 총 2KB(0.001mm², A76의 0.1%)**.

**D. user-level threads**: core당 32-64 thread, 단일 global job queue, cooperative run-to-completion(단, DRAM miss 시 대기). **priority scheduling with aging**: new job 우선순위 2, pending job 1 — 그러나 skewed 분포에서 pending queue starvation 가능 → **aging**(pending head의 나이가 평균 flash 응답시간 초과면 우선 실행). pending queue 가득 차면 새 miss는 가장 오래된 job의 flash 응답 대기.

### §V Methodology (p.9)
**QFlex**(Flexus 기반) cycle-accurate **full-system** 시뮬. **16× ARM Cortex-A76** 2GHz, 4-way OoO, 128-ROB, 32-SB, directory-based MESI, 1MB LLC/tile. DRAM cache **128K set·4-way·16KB row·4KB page**(RAS 55c, CAS 3c), SSD **50/100µs random read/write, 4KB page, TLC, plane-blocking GC**. 1TB/64코어를 **256GB/16코어 + 8GB(3%) DRAM cache**로 스케일다운. workload: Tailbench의 **Silo·Masstree**, microbench의 **ArraySwap·RBT·HashTable·TATP·TPCC**(Zipfian, DRAM miss **5-25µs**마다). 요청 도착은 Poisson.

### §VI Evaluation (p.10-11)
**A. throughput**: AstriFlash **DRAM-only의 95%**(AstriFlash-Ideal 96%). 5% 손실은 tag 비교·response 대기·miss마다 pipeline flush·thread switch(100ns). **OS-Swap 58%**, **Flash-Sync(=FlatFlash) 27%**.

**B. service latency (Table II, Flash-Sync 대비 정규화)**: **AstriFlash geomean 1.02×**(우수), **AstriFlash-noPS 6.82×**(FIFO는 pending 굶김), **AstriFlash-noDP 1.76×**(page table을 flash에서 walk → tail 악화, ~70% latency 손실).

**C. tail latency (TATP)**: 저부하에선 flash 접근분 때문에 AstriFlash tail이 높지만, 부하가 오르면 queuing과 flash 접근이 overlap되어 **AstriFlash 93% throughput이 DRAM-only 96%의 tail latency와 매칭**(3% throughput·20× cost 절감으로 동일 tail).

**D. GC 오버헤드**: 256GB flash는 read의 4% block, 1TB는 chip이 많아 blocked request가 4×+ 감소(<1%). flash write가 async라 GC는 대체로 off critical path.

### §VII Related Work (p.11)
**flash integration**: SSDAlloc(hybrid DRAM/flash, log-structured), **FlashMap**[31](unified address space), **2B-SSD**[9](byte+block 이중 경로), **FlatFlash**[1](horizontally-tiered, DRAM을 flash cache로, page promotion), PageSeer(NVM 맥락). **NVM**: Eisenman et al.(Facebook, NVM as SW-controlled cache). **user-level threading & killer microseconds**: informing memory operations[30]+lightweight multithreading[54]가 switch-on-miss의 뿌리; Duplexity·Cho et al.·AIFM.

### §VIII Conclusion (p.11)
HW/SW co-designed **µs-scale switch-on-miss** architecture로 demand paging 제거, flash에서 직접 서비스하며 DRAM-급 성능. **memory cost 20× 절감**, 미래 TB급 memory system 해법.

---

## Key vocabulary
**Thesis / framing:**
- "hardware-software co-design that tightly integrates flash and DRAM"
- "absorb μs-scale flash accesses with ns-scale overheads"
- "core-side and memory-side overheads ... synergistically"

**Technical concepts:**
- "μs-scale switch-on-miss architecture"
- "hardware-managed DRAM cache" (set-associative, 4KB page granularity)
- "user-level thread switch" (100ns, switch-on-miss)
- "accelerated (DRAM-cache) miss handler" / "frontside & backside controller"
- "in-DRAM Miss Status Row (MSR)" (100s concurrent misses)
- "forward-progress bit" / "Handler Address & Resume Register"
- "post-retirement speculation over the Store Buffer" (ASO 확장)
- "DRAM partitioning (flat + cached)" (page table DRAM-resident)

**Value language:**
- "DRAM-like throughput while maintaining the 99th-percentile tail latency"
- "reduces the memory cost by 20x"

> ⚠ **피해야 할 어휘** (AstriFlash-signature, echo 주의):
> - "μs-scale switch-on-miss architecture" (이 논문 고유 프레이밍)
> - "core-side and memory-side overheads" (이들의 해부 프레임)

---

## Citable quantitative data
| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §I, p.1 | flash: DRAM 대비 $/GB **50× 저렴**, 지연 **1000×(≈50µs)** | flash-as-memory 경제/지연 트레이드오프 |
| §II.A, p.2 | miss rate가 **DRAM=flash의 3%**에서 평탄, 64코어 **60GBps** flash BW | DRAM:flash 비율 설계 근거 |
| §II, p.2 | 1TB flash + 3%(32GB) DRAM cache → **memory cost 20×↓** | cost 절감 핵심 수치 |
| §II.C, p.3 | context switch **~5µs**(core-side), TLB shootdown **>10µs**(memory-side), DRAM miss **5-25µs**마다 | paging 오버헤드 해부 |
| §III.B, p.4 | user-level thread switch **100ns**(context switch 50×·최근 제안 5× 빠름) | switch-on-miss 경량성 |
| §IV.C, p.8 | core당 추가 **2KB SRAM = 0.001mm² (A76의 0.1%)** | HW 오버헤드 미미 |
| §VI.A + Fig.9 | throughput: AstriFlash **95%**, OS-Swap **58%**, Flash-Sync(FlatFlash) **27%** (DRAM-only 대비) | 종합 성과·baseline 대비 |
| Table II, p.10 | 99%tile service latency(Flash-Sync 정규화): AstriFlash **1.02**, noPS **6.82**, noDP **1.76** | scheduler·DRAM partition 기여 |

---

## 🎯 Strategic anchor
> "Previous proposals [1], [76] expose the flash access latency to the cores and force them to wait for a response, thus losing throughput. In contrast, AstriFlash provides a novel μs-scale switch-on-miss architecture that allows switching user-level threads on a DRAM miss to hide flash accesses efficiently and achieve DRAM-like throughput." (§III.B, p.4)

여기서 **[1]이 FlatFlash**다. 그리고 §V에서 **Flash-Sync = FlatFlash**로 못박아 27% throughput의 baseline으로 쓴다.

→ **본인 활용**: 면담/발표에서 "긴 접근 지연을 **문맥 전환으로 숨긴다**"는 한 줄 아이디어가 **어떻게 진화했는지**의 결정적 스냅샷으로 인용. FlatFlash(동기 대기, page promotion) → **AstriFlash(user-level HW thread switch, SSD를 black box·page 단위)** → **[[SkyByte]](OS↔SSD co-design, cacheline 단위, coordinated context switch)**. AstriFlash가 "SSD를 안 건드리고 core/OS 위에서만" 지연을 숨긴 정점이라면, SkyByte는 "SSD 컨트롤러까지 내려가 협응"으로 넘어간다 — 이 **"co-design을 어디까지 내리나"** 축이 내 발표 프레임의 눈금.

---

## Connection to my research direction
| 차원 | AstriFlash (HPCA'23) | SkyByte (HPCA'25) | 내 방향 |
|---|---|---|---|
| SSD 취급 | **black box** (PCIe BAR, 4KB page fetch) | **co-design** (OS↔SSD controller) | 공유 memory device |
| 접근 granularity | **page (4KB)** | **cacheline** | cacheline coherence |
| 지연 은닉 기법 | user-level **HW thread switch** (switch-on-miss, 100ns) | **coordinated context switch** + cacheline write log | multi-node scheduling |
| 인터커넥트 | PCIe (Gen5) + DRAM cache | CXL.mem (Type-3) | CXL 3.0 fabric |
| 일관성 범위 | **single-host** (core-internal speculation) | **single-host** (HDM-H) | **multi-host (HDM-DB/BI)** |
| 공통 한계 | single-host | single-host | 이걸 넘는 게 목표 |

AstriFlash와 SkyByte는 "긴 지연을 **일 전환으로 숨긴다**"는 원리를 공유하되 **전환의 위치와 granularity**가 갈린다. AstriFlash의 은닉은 철저히 **core 내부(user-level thread)** 사건이고 SSD는 손대지 않는 **black box**다. SkyByte는 SSD 컨트롤러와 OS를 함께 설계해 **cacheline 단위**로 내려가고 전환을 **양쪽이 협응**하게 만든다. 두 접근 모두 **한 host**가 한 device를 볼 때만 성립한다 — AstriFlash의 switch-on-miss·SB speculation·MSR은 전부 **한 코어/한 host의 microarchitecture** 안에서 닫혀 있다. 내 multi-node CXL coherence 방향에서는 stall이 **cross-host**로 발생한다: host A가 접근한 cacheline이 host B에 있어 지연될 때, "다른 thread로 전환해 숨긴다"만으로는 부족하고 **누가 최신본을 갖고 언제 back-invalidate하나(HDM-DB/BI)**를 정의해야 한다. 즉 AstriFlash의 은닉 기법은 재사용하되, **문제 정의를 single→multi host로** 다시 세우는 것이 gap. (→ [[SkyByte]], [[CXL Multi-node Coherence]], [[CXL Overview]])

---

## Open questions / gaps
- [ ] AstriFlash는 SSD를 **black box·page 단위**로 둔다 — SSD 내부(FTL·GC 타이밍)와 협응하면 (SkyByte처럼) 더 얻을 게 있나? 반대로 black box라 **이식성**은 SkyByte보다 높다.
- [ ] switch-on-miss·SB speculation·MSR이 전부 **single-host microarchitecture** 안에 닫혀 있음 — **multi-host 공유 CXL/flash**에서 cross-host stall은 thread switch만으로 안 됨(coherence 필요).
- [ ] page(4KB) granularity: skew가 강하면 4KB 안 대부분이 cold라 flash BW/DRAM 낭비 — SkyByte의 cacheline log가 이 지점을 정확히 공격.
- [ ] SC-강제 + post-retirement speculation의 복잡도/검증 비용 — relaxed consistency HW에 얼마나 침습적인가?
- [ ] user-level thread가 32-64개 필요 → 충분한 request 병렬성 없는 workload(저부하)에선 tail이 오히려 나쁨(§VI.C). load-dependent 이득.

---

## References worth following up
| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☑ | [1] | Abulila et al., **FlatFlash** (ASPLOS 2019) | AstriFlash의 Flash-Sync baseline·직접 선행. 이미 정리함 |
| ☐ | [27] | Gupta et al., **Midgard** (ISCA 2021) | AstriFlash 주소변환 기반. 같은 저자군, TB급 memory 변환 |
| ☐ | [77] | Wenisch et al., **ASO / store-wait-free** (ISCA 2007) | SB speculation 확장의 토대 — committed store 되돌리기 |
| ☐ | [30] | Horowitz et al., **Informing Memory Operations** (ISCA 1996) | switch-on-miss의 원류(informing loads) |
| ☐ | [9] | Bae et al., **2B-SSD** (ISCA 2018) | byte+block 이중 경로 SSD — FlatFlash/SkyByte 공통 비교축 |
| ☐ | [12] | Barroso et al., **Attack of the Killer Microseconds** (CACM 2017) | µs-scale 오버헤드 문제의식의 근거 |
| ☐ | [65] | Ruan et al., **AIFM** (OSDI 2020) | user-level preemption으로 far memory page 이동 — 유사 계열 |

---

## Personal annotations
<!-- 본인 메모 영역 -->
