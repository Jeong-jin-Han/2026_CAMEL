---
title: "Informing Memory Operations: Providing Memory Performance Feedback in Modern Processors"
aliases: [Informing Memory Operations, Informing Loads, Informing Memory Ops]
type: paper-ref
venue: ISCA
year: 1996
ref-of: "SkyByte"
tags:
  - paper
  - ref
  - topic/informing-memory-operations
  - topic/hardware-software-codesign
  - topic/cache-miss-feedback
  - topic/context-switch-on-miss
  - venue/isca
  - year/1996
---

# Informing Memory Operations: Providing Memory Performance Feedback in Modern Processors

> **Source PDF**: [Informing Memory Operations - Providing Memory Performance Feedback in Modern Processors.pdf](<Informing Memory Operations - Providing Memory Performance Feedback in Modern Processors.pdf>)
> 🕸️ NodeGraph: [InformingMemoryOps.html (새 탭에서 렌더링, push 후 유효)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/SkyByte%20-%20Architecting%20An%20Efficient%20Memory-Semantic%20CXL-based%20SSD%20with%20OS%20and%20Hardware%20Co-design/refs/Informing%20Memory%20Operations%20-%20Providing%20Memory%20Performance%20Feedback%20in%20Modern%20Processors/InformingMemoryOps.html)
> **Authors**: **Mark Horowitz** (Stanford), **Margaret Martonosi** (Princeton), **Todd C. Mowry** (Univ. of Toronto), **Michael D. Smith** (Harvard)
> **Venue / Year**: ISCA 1996 (23rd International Symposium on Computer Architecture)
> **DOI**: 10.1145/232973.233000 · **Length**: 11 pages
> **Read status**: ☑ Full read (2026-07-14)
> **My reading purpose**: [[SkyByte]]가 되살린 아이디어의 **지적 뿌리(intellectual root)**. "하드웨어가 long-latency 이벤트(cache miss)를 소프트웨어에 알려서 소프트웨어가 스케줄링으로 대응한다"는 primitive의 원형. SkyByte의 NDR `SkyByte-Delay` opcode + "SkyByte Long Delay Exception → context switch"가 이 논문의 **context-switch-on-a-miss**의 직계 후손. CXL/SSD 이전 시대이므로, 계보의 출발점으로 프레이밍하려고 읽음.

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
load/store 명령은 메모리 계층이 평평(flat)하던 시절 정의되어, 소프트웨어에게 **"이 참조가 hit인가 miss인가"를 알려줄 방법이 없다**. 그래서 prefetching·multithreading·cache coherence 같은 소프트웨어 latency-tolerance 기법들이 메모리 동작을 직접 관측하지 못해 일반성이 떨어졌다. 이 논문은 **정보 제공 메모리 연산(informing memory operations)**을 제안한다 — 메모리 연산에, **cache miss일 때만 taken되는 conditional branch-and-link**를 (암묵적 또는 명시적으로) 결합한 것이다. 세 가지 구현을 제시한다: (1) **cache outcome condition code**(miss를 새 condition code로 만들어 explicit branch-and-link로 검사), (2) **squashed slot**(hit이면 뒤따르는 slot 명령을 squash — delay-slot 방식, 본문에선 제외), (3) **low-overhead cache miss trap**(MHAR/MHRR 두 레지스터로, OS를 거치지 않고 user space로 가벼운 trap). 핵심 통찰은 in-order(Alpha 21164)·out-of-order(MIPS R10000) superscalar가 이미 **branch·exception 처리 하드웨어**를 갖고 있어 필요한 HW가 거의 다 존재한다는 것. hit case 오버헤드는 (조건에 따라) 0 또는 1 instruction. 13개 SPEC92 벤치마크 중 12개에서 오버헤드 40% 미만. case study(fine-grained access control로 cache coherence)에서 informing 방식이 ECC-based 대비 평균 **11%**, reference-checking 대비 **24%** 빠름.

---

## Core thesis
> "we propose a new class of memory operations called informing memory operations, which essentially consist of a memory operation combined (either implicitly or explicitly) with a conditional branch-and-link operation that is taken only if the reference suffers a cache miss." (Abstract)
> "Informing memory operations are a general primitive for allowing software to observe and react to its own memory referencing behavior." (§5)

메모리 동작을 소프트웨어가 **관측 불가능(unobservable)**한 것이 근본 문제다. branch는 어느 경로를 갔는지 관측 가능하지만 load/store는 hit/miss를 알 수 없다. informing memory operation은 miss를 **관측 가능하고 반응 가능한 이벤트**로 승격시켜, "프로세서가 원래는 stall할 miss 구간에서 소프트웨어가 handler 코드를 실행하게" 한다. 그리고 이 primitive를 지원하는 하드웨어는 이미 현대 프로세서 안에 (branch/exception 메커니즘으로) 대부분 존재한다.

---

## Why this matters to me
이 논문은 [[SkyByte]]가 부활시킨 아이디어의 **1996년 원형**이다. SkyByte는 CXL-SSD 접근이 (DRAM hit이 아니라) NAND까지 내려가 수 마이크로초 stall이 될 때, NDR `SkyByte-Delay` opcode와 "SkyByte Long Delay Exception"으로 **하드웨어가 그 long-latency 이벤트를 CPU/OS에 알려 context switch**를 유발한다. 이건 정확히 이 논문 §4.1.3의 **context-switch-on-a-miss multithreading**을 CXL 시대로 옮긴 것이다 — "cache miss라는 latency 이벤트를 소프트웨어에 노출해 다른 스레드로 전환"이 "SSD long stall이라는 latency 이벤트를 노출해 context switch"로 진화했다. 즉 내 발표축 **"SSD 이중역할 × transparent co-design"**의 co-design 파트가 어디서 왔는지를 이 논문이 규정한다. 더 흥미로운 건 내 박사 방향(multi-node CXL coherence)이다: 이 논문은 fine-grained access control로 **cache coherence**를 informing operation으로 구현하는 case study를 이미 담고 있다. 그렇다면 **multi-host 공유 메모리에서 remote back-invalidation·cross-host miss 같은 long-latency/coherence 이벤트를, 하드웨어가 어느 host의 스케줄러에 알려야 하는가?** 가 informing-ops의 다음 확장이 된다.

---

## Structure overview
| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.1-2 | load/store는 hit/miss를 소프트웨어에 노출 못 함(관측성 부재). 기존 모니터는 heavyweight or coarse-grained |
| 2 | Informing Memory Operations | p.2-3 | 세 접근: (2.1) cache outcome condition code, (2.2) low-overhead cache miss trap(MHAR/MHRR), (2.3) 요약(general·fine-grained·selective·low-overhead) |
| 3 | Implementation Issues | p.3-6 | in-order(Alpha 21164: replay trap 재활용), out-of-order(MIPS R10000: branch/exception 메커니즘), cache as visible state(speculative load, MSHR 수명 연장) |
| 4 | Uses of Informing Memory Operations | p.6-10 | prefetching, performance monitoring, context-switch-on-a-miss multithreading, coherence. 오버헤드 측정 + coherence case study |
| 5 | Conclusions | p.10 | general primitive, 현대 프로세서에 HW 이미 존재, 미래 innovation 촉발 기대 |

---

## Section notes

### §1 Introduction (p.1-2)
processor–memory 속도 격차로 메모리 latency가 지배적 병목이 됐다(uniprocessor에서 main memory 참조 = **50+ cycle**, multiprocessor는 더 큼). 소프트웨어 기법(compiler prefetching, cache blocking, page migration 등)이 상황별로 성공했지만 **일반성이 제한**됐는데, 근본 이유는 "**소프트웨어가 자기 자신의 메모리 동작을 직접 관측할 수 없다**"는 것이다. load/store는 메모리 계층이 flat하던 시절 정의되어, branch와 달리 hit/miss를 알려주는 메커니즘이 없다.

> "loads and stores offer no direct mechanism for software to determine if a particular reference was a hit or a miss." (§1)

기존 하드웨어 모니터링은 두 부류로 나뉘고 둘 다 부적합하다: **heavyweight**(모니터링 정보 접근이 프로그램 동작을 크게 교란) 또는 **coarse-grained**(실제 동작의 요약만 제공). Pentium/R10000/Alpha의 performance counter조차 특정 참조의 hit/miss를 알려면 참조 전후로 miss counter를 읽어야 해 극도로 느리고, out-of-order 머신(R10000)에선 counter 접근이 pipeline을 serialize한다.

### §2 Informing Memory Operations (p.2-3)
핵심 분해: informing memory operation = **메모리 연산 + 그 연산의 hit/miss 신호에 predicate된 conditional branch-and-link**. hit이면 transfer-of-control이 nullify되고, miss이면 해당 참조 전용 코드로 제어가 넘어간다.

**§2.1 Cache Outcome Condition Code** — 모든 메모리 연산이 기본으로 informing이 되고, 하드웨어는 각 연산의 hit/miss를 user-visible state에 기록. 소프트웨어가 새 conditional branch-and-link 명령으로 직전 메모리 연산의 결과를 검사. 필요한 HW 대부분이 base machine에 이미 있음(특히 condition code 지원 머신) — miss가 그냥 하나의 condition code가 된다. 오버헤드: informing 연산마다 명령 하나 추가(branch-and-link). common case(hit)에 최적화하려면 이 branch를 **not-taken으로 예측** → mispredict 페널티는 miss case에만.

> "the cache miss simply becomes another condition code." (§2.1)

**§2.2 Low-Overhead Cache Miss Traps** — explicit check 명령의 오버헤드마저 제거. primary data cache miss 시 **user space로 가는 저오버헤드 trap**을 유발. 전통적 trap(OS로 context switch, 수백 cycle)을 피하고, conditional branch에 더 가깝게 설계 — running application의 PC만 바꾸고 OS 코드를 호출하지 않으며 user-visible 레지스터 하나만 저장. 두 레지스터 추가: **MHAR(Miss Handler Address Register)** = miss 시 호출할 handler 주소, **MHRR(Miss Handler Return Register)** = trap 시 return 주소 기록. MHAR=0이면 trapping 비활성.

> "One register is the Miss Handler Address Register (MHAR), which contains the instruction address of the handler to invoke on an informing memory operation cache miss. The other register is the Miss Handler Return Register (MHRR)..." (§2.2)

**§2.3 Summary** — 네 특성: **general**(특정 HW 조직에 독립), **fine grained**(저수준 관측), **selective notification**(triggering 이벤트에서만 호출), **low overhead**(호출 안 되면 거의 무교란). 두 방식 tradeoff: condition code는 HW 단순하지만 실행파일을 branch-on-miss로 재컴파일/instrument 필요. low-overhead trap은 HW 약간 복잡하지만 **컴파일러/에디터 없이 whole-system 모니터링 가능**(MHAR를 모든 프로세스의 general handler로 default 설정).

### §3 Implementation Issues (p.3-6)
제안된 메커니즘들은 아키텍처적으론 다르지만 HW 구현 복잡도는 비슷하고, 핵심은 **cache miss 시 안전·효율적인 control flow 변경**이다. low-overhead trap을 "load/store + 암묵적 conditional branch-and-link"로 보고 논한다. 고무적인 점: 필요한 HW가 대부분 **이미 있는 branch·exception 메커니즘**이다.

**§3.1 In-order (Alpha 21164)** — 21164는 4-issue superscalar, 흥미로운 stall model(issue된 명령은 stall 불가). 어려운 상황은 **"replay trap"**으로 처리하고 그중 하나가 이미 cache 관련(load가 hit 타이밍에 dependent 명령을 issue했는데 miss나면 replay trap → pipeline flush → 재시작). 이 **동일한 replay trap 메커니즘을 재활용**해 low-overhead trap 구현. informing 연산 직후 명령을 그 hit/miss 신호에 dependent로 표시하고 hit으로 예측 → miss면 replay trap 발생, 재issue 대신 implicit branch-and-link를 issue(MHRR에 return 주소 로드). load·store 둘 다에 발생.

**§3.2 Out-of-order (MIPS R10000)** — 순서가 미정이라 dependence·ordering 추적이 관건. R10000은 branch는 renaming logic의 shadow state로, exception은 graduation queue 최상단 도달 시 처리. low-overhead trap을 **branch 메커니즘** 또는 **exception 메커니즘** 둘 다로 구현 가능. branch 방식은 miss 오버헤드 낮지만 HW 부담 큼 — 각 참조가 잠재적 branch가 되면 shadow state가 **약 3배** 필요(R10000은 현재 predicted branch 3개 제한, 보통 branch당 메모리 연산 2개). exception 방식은 handler invocation이 느리지만(graduation queue 최상단 대기) HW는 매우 modest(reorder buffer가 miss 여부 기록).

**§3.3 Cache as Visible State** — informing operation이 cache 상태를 소프트웨어에 노출하니 새 문제 발생: coherence 같은 응용은 "새 line이 cache로 들어올 때마다 miss를 감지해 access rights를 검사"해야 하므로 이 검사가 우회되지 않도록 HW가 보장해야 한다. 그런데 out-of-order 머신은 first-level cache를 **speculative**하게 갱신한다. 해법: speculative informing load가 miss로 cache를 갱신하는 건 허용(common case 최적화)하되, load가 squash되면 그 line을 invalidate. 이를 위해 lockup-free cache의 **MSHR(Miss Status Handling Register) 수명을 연장**(명령이 squash 또는 graduate될 때까지 register 해제 지연). 시뮬레이션상 register 개수(8개)는 그대로 충분.

> "we must guarantee that the data updates the first-level cache only when the informing load operation commits." (§3.3)

### §4 Uses of Informing Memory Operations (p.6-10)
오버헤드는 (i) miss에 대응해 하는 일의 양, (ii) handler 주소를 얼마나 자주 바꾸는가로 결정.

**§4.1.1 Software-Controlled Prefetching** — prefetching의 난제는 "어느 동적 참조가 miss날지 예측". informing operation으로 (a) 이전 실행의 miss 프로파일로 재컴파일, 또는 (b) **on-the-fly**로 miss handler 안에 prefetch를 두어 실제 miss날 때만 prefetch 오버헤드 유발. handler는 작음(<10 instruction).

**§4.1.2 Performance Monitoring** — 모니터링 툴의 난제는 프로그램 교란 없이 상세 정보 수집. 이전 연구[HMMS95]는 informing operation으로 per-reference miss rate를 **런타임 오버헤드 25% 미만**으로 수집(handler ~10 instruction, MHRR의 return 주소로 hash table 갱신).

**§4.1.3 Software-Controlled Multithreading** — ★ SkyByte의 직계 조상. multithreading은 **cache miss 시점에 한 thread에서 다른 context로 전환**해 latency를 은닉한다. 기존 구현은 HW로 thread를 관리·전환했지만, informing operation은 **단일 miss handler가 (전부 소프트웨어 제어로) thread를 save/restart하는 소프트웨어 기반 접근**을 가능케 한다. 최적화: (1) primary가 아니라 **secondary cache miss에서만** switch 유발, (2) register save/restore 오버헤드를 컴파일러 최적화나 HW 지원(SPARC register window류)으로 최소화.

> "Multithreading tolerates memory latency by switching from one thread (or 'context') to another at the start of a cache miss... informing memory operations enable a software-based approach where a single miss handler could save and restart threads (all under software control) upon cache misses." (§4.1.3)

**§4.1.4 & §4.3 Cache Coherence (Fine-Grained Access Control) — case study** — Blizzard-E와 유사하나, ECC fault 대신 informing operation으로 block-level handler를 read miss·state-변경 write에서 호출. handler가 per-cache-line 보호 상태(INVALID/READONLY/READWRITE)를 조회해 access가 적절한지 판정. informing 방식은 cache에 tightly-coupled되어 handler invocation이 ECC fault handler보다 짧다.

**§4.2 Overhead** — 14개 SPEC92 벤치마크(cycle 단위 시뮬레이션, MIPS R10000·Alpha 21164 기반). 1·10-instruction generic handler에서 tomcatv 제외 12/13 벤치마크가 두 모델 모두 **오버헤드 40% 미만**. 흥미로운 결과: 명령 수가 30% 넘게 늘어도(mdljsp2, alvinn, out-of-order) 실행시간은 **1%만** 증가 → MHAR를 자주 바꾸는 기법(prefetching, multithreading, coherence)도 큰 페널티 없음. su2cor는 예외(8KB direct-mapped cache의 심한 conflict로 handler 빈발, 실행시간 3배). out-of-order가 큰 handler에서 in-order보다 훨씬 유리(handler를 다른 계산과 overlap). branch-처리 방식이 exception-처리 방식보다 좋음(compress에서 exception 방식은 1·10-instruction handler 실행시간 9%·7% 증가).

**§4.3.2 Coherence Results** — informing-op 기반 coherence가 ECC-based·reference-checking 두 방식을 **항상** 능가. 평균 ECC-based 대비 **11%**, reference-checking 대비 **24%** 빠름. ECC 대비: coherence action time 개선. reference-checking 대비: (no-coherence-action case에서) 모든 참조가 아니라 **cache miss에서만** lookup하는 이득. 더 작은 network latency나 더 큰 primary cache일수록 informing 방식이 상대적으로 유리.

### §5 Conclusions (p.10)
informing memory operations = 소프트웨어가 자기 메모리 참조 동작을 관측·반응하는 **general primitive**. 두 구현(cache outcome condition code, low-overhead cache miss trap) 모두 현대 프로세서가 branch·exception 지원을 위해 이미 가진 HW로 대부분 충족. condition code는 HW 최소(miss 시 set되는 condition code bit 하나)지만 재컴파일 필요. low-overhead trap은 특수 컴파일러·에디터 없이 모니터링 가능(소스 없는 상용 SW·OS 코드도). 의의: 성능 모니터링을 넘어 prefetching·multithreading·access control 등 **훨씬 넓은 응용을 지원하는 기본 primitive** — commodity 프로세서마다 각 기능 전용 HW를 넣는 것보다 경제적. 이 generality가 미래 프로세서의 매력적 기능이며, 실제 HW에 등장하면 더 혁신적 사용을 촉발할 것.

---

## Key vocabulary
**Thesis / framing:**
- "informing memory operations"
- "providing memory performance feedback"
- "software cannot directly observe its own memory behavior"
- "a general primitive for allowing software to observe and react to its own memory referencing behavior"

**Technical concepts:**
- "conditional branch-and-link ... taken only if the reference suffers a cache miss"
- "cache outcome condition code"
- "low-overhead cache miss trap" / "Miss Handler Address Register (MHAR)" / "Miss Handler Return Register (MHRR)"
- "context-switch-on-a-miss multithreading"
- "fine-grained access control"
- "cache as visible state"

**Value language:**
- "general, fine grained, selective notification, low overhead"
- "the processor could normally stall" (miss 구간을 유용하게 쓴다는 프레이밍)
- "modern processors already contain the bulk of the necessary hardware support"

> ⚠ **피해야 할 어휘** (이 논문 signature, 직접 echo 주의):
> - "informing memory operations" / "informing loads" (이 논문 고유 명명)
> - "context-switch-on-a-miss" (이 논문 프레이밍)

---

## Citable quantitative data
| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §1, p.1 | uniprocessor main memory 참조 = **50+ cycle**, multiprocessor는 더 큼 | 메모리 latency 병목의 규모 |
| §4.2, p.7-8 | 13개 SPEC92 중 12개에서 informing 오버헤드 **<40%** (1·10-instr handler) | primitive의 실용성(저오버헤드) |
| §4.2, p.8 | 명령 수 30%+ 증가에도 실행시간 **+1%** (out-of-order) | handler 주소 자주 바꿔도 페널티 작음 |
| §4.3.2, p.10 | informing coherence가 ECC-based 대비 **평균 11%**, reference-checking 대비 **24%** 빠름 | co-design이 특수 HW 없이 이김 |
| Table 2, p.9 | Informing: 33-cycle lookup(6 pipe + 9 handler) / 25-cycle state change vs ECC 250-cycle read-to-invalid | informing handler가 ECC fault보다 가벼움 |

---

## 🎯 Strategic anchor
> "Multithreading tolerates memory latency by switching from one thread (or 'context') to another at the start of a cache miss ... informing memory operations enable a software-based approach where a single miss handler could save and restart threads (all under software control) upon cache misses." (§4.1.3, p.6)

→ **본인 활용**: 면담·자소서에서 [[SkyByte]]의 "SSD long stall → context switch"를 설명할 때, **1996년 이 문장이 그 원형**임을 짚는다. "하드웨어가 long-latency 이벤트를 소프트웨어에 노출해 스케줄링으로 은닉한다"는 co-design 원리는 cache miss(1996) → CXL-SSD stall(SkyByte, 2025)로 매체만 바뀌었을 뿐 불변이다. 내 "SSD 이중역할 × transparent co-design" 축의 co-design 계보를 30년 스케일로 그리는 앵커. 그리고 이 논문이 이미 coherence를 informing으로 구현했으니, **multi-host 공유 메모리의 coherence 이벤트를 informing하는 것**이 내 방향의 자연스러운 확장이라고 연결.

---

## Connection to my research direction
| 차원 | Informing Memory Ops (1996) | SkyByte (2025) | 내 방향 |
|---|---|---|---|
| 노출되는 이벤트 | primary **cache miss** | **SSD long stall**(NAND까지 내려감) | remote **back-invalidation** / cross-host miss |
| 알림 메커니즘 | condition code / low-overhead trap(MHAR·MHRR) | NDR `SkyByte-Delay` opcode + Long Delay Exception | multi-host coherence event notification |
| 소프트웨어 반응 | context-switch-on-a-miss(SW 스레드 전환) | OS context switch로 stall 은닉 | 어느 host의 스케줄러가 반응? |
| coherence | fine-grained access control(single node 내 shared) | single-host (HDM-H) | **multi-host (HDM-DB / BI)** |
| 범위 | uniprocessor + 소규모 MP(16 proc 시뮬) | single-host CXL-SSD | CXL 3.0 multi-node fabric |

이 논문은 "하드웨어가 memory 이벤트를 소프트웨어에 노출한다"는 **primitive 자체**를 세웠고, SkyByte는 그 primitive를 CXL-SSD의 long stall에 적용했다. 두 논문 모두 이벤트의 **관측 주체가 단일 host** 안에 있다. 내 연구는 이 primitive를 **multi-host**로 들어올린다 — 여러 host가 공유 메모리를 coherent하게 볼 때, remote back-invalidation이나 cross-host miss 같은 long-latency/coherence 이벤트가 발생하면 **하드웨어(또는 CXL fabric)가 어느 host의 어느 스케줄러에 그것을 informing해야 하는가?** 1996년의 "miss → local thread switch", SkyByte의 "SSD stall → local context switch"를, "remote invalidation → 원격 host 스케줄링 조정"으로 확장하는 것이 gap이다. (→ [[CXL Multi-node Coherence]], [[CXL Overview]])

---

## Open questions / gaps
- [ ] 이 논문의 informing 이벤트는 전부 **local**(자기 core의 cache miss). multi-processor coherence case study조차 각 node가 자기 miss를 informing할 뿐 — **cross-host 이벤트를 원격 host에 informing**하는 채널은 없음.
- [ ] MHAR/MHRR은 **single address space·single core context** 전제. 공유 메모리에서 어느 host의 handler가 remote invalidation을 처리하나?
- [ ] context-switch-on-a-miss는 miss latency < context switch cost일 때만 이득. CXL/SSD처럼 latency가 µs급이면 SkyByte가 보였듯 이득이 커지지만, multi-host coordination cost는 미지수.
- [ ] cache-as-visible-state(speculative update invalidation)를 multi-host directory/back-invalidate와 어떻게 조화? CXL 3.0 BI가 그 자리를 이어받는가?

---

## References worth following up
| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [HMMS95] | Horowitz, Martonosi, Mowry, Smith, **Informing Loads** (Stanford CSL-TR-95-673, 1995) | 이 논문의 직전 technical report — squashed-slot 방식·prefetching·monitoring 평가의 원출처 |
| ☐ | [SFL+94] | Schoinas et al., **Fine-Grain Access Control for Distributed Shared Memory** (ASPLOS 1994) | Blizzard-S/E — coherence case study의 비교 baseline, access control 계보 |
| ☐ | [Smi81] | B. J. Smith, **HEP Multiprocessor** (SPIE 1981) | multithreading(context switch on latency)의 원류 |
| ☐ | [MLG92] | Mowry, Lam, Gupta, **Compiler Algorithm for Prefetching** (ASPLOS 1992) | software-controlled prefetching의 표준 참조(같은 저자 Mowry) |
| ☐ | [FJ94] | Farkas, Jouppi, **Non-Blocking Loads** (ISCA 1994) | MSHR·lockup-free cache — cache-as-visible-state 구현 토대 |

---

## Personal annotations
<!-- 본인 메모 영역 -->
