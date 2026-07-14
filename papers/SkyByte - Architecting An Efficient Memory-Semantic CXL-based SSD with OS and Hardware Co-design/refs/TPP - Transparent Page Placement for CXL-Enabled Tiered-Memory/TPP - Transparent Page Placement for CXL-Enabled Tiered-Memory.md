---
title: "TPP: Transparent Page Placement for CXL-Enabled Tiered-Memory"
aliases: [TPP]
type: paper-ref
venue: ASPLOS
year: 2023
ref-of: "SkyByte"
tags:
  - paper
  - ref
  - topic/tiered-memory
  - topic/page-placement
  - topic/os-memory-management
  - topic/cxl-memory
  - venue/asplos
  - year/2023
---

# TPP: Transparent Page Placement for CXL-Enabled Tiered-Memory

> **Source PDF**: [TPP - Transparent Page Placement for CXL-Enabled Tiered-Memory.pdf](<TPP - Transparent Page Placement for CXL-Enabled Tiered-Memory.pdf>)
> 🕸️ NodeGraph: [TPP.html (새 탭에서 렌더링, push 후 유효)](https://raw.githack.com/Jeong-jin-Han/2026_CAMEL/main/papers/SkyByte%20-%20Architecting%20An%20Efficient%20Memory-Semantic%20CXL-based%20SSD%20with%20OS%20and%20Hardware%20Co-design/refs/TPP%20-%20Transparent%20Page%20Placement%20for%20CXL-Enabled%20Tiered-Memory/TPP.html)
> **Authors**: Hasan Al Maruf (Univ. of Michigan), Hao Wang (NVIDIA), Abhishek Dhanotia (Meta), Johannes Weiner (Meta), Niket Agarwal (NVIDIA), Pallab Bhattacharya (NVIDIA), Chris Petersen (Meta), Mosharaf Chowdhury (Univ. of Michigan), Shobhit Kanaujia (Meta), Prakash Chauhan (Meta)
> **Venue / Year**: ASPLOS 2023
> **arXiv / DOI**: arXiv:2206.02878 · **Length**: 14 pages
> **Read status**: ☑ Full read (2026-07-14)
> **My reading purpose**: [[SkyByte]]가 자신의 adaptive page migration을 겨루는 **TPP-style OS baseline(SkyByte-CT)**의 원본. CXL tiered-memory에서 hot/cold 페이지 배치를 **OS/software로 투명하게** 푸는 대표작 — SkyByte의 **hardware-assisted migration**이 대비되는 상대. Linux NUMA-balancing 기반 transparent placement의 실제 production 근거(Meta, Linux v5.18 머지) 파악.

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
TPP는 CXL-Memory가 OS에 **CPU-less NUMA node**로 보이는 tiered-memory에서, application 수정·사전지식 없이 hot page는 빠른 local memory에, cold page는 느린 CXL-Memory에 두는 **OS-level 투명 페이지 배치(transparent page placement)** 메커니즘이다. 기존 Linux는 DRAM-only 동종 시스템 전제라 CXL tier에서 성능이 나쁜데, 핵심 원인은 (1) local node가 차면 **동기(critical-path) reclamation**이 새 allocation을 CXL-node로 밀어내고, (2) 기본 NUMA Balancing이 page의 active 상태를 안 보고 **즉시 promote**해 hot page가 CXL-node에 **갇히는(trapped) ping-pong**을 만든다는 것이다. TPP는 네 축으로 답한다 — (a) swap 대신 **경량 async demotion**(migration to CXL-node), (b) **allocation과 reclamation 경로 분리**(demotion_watermark > allocation_watermark로 local에 free headroom 유지), (c) **active-LRU 기반 hot page 판별 + hysteresis**로 trapped hot page만 promote(NUMA Balancing 증강, CXL-node만 sampling), (d) **page-type-aware allocation**(file cache는 CXL, anon은 local 선호). 페이지 온도 감지는 무거운 PEBS 대신 minor page fault + kernel LRU로 거의 zero-overhead. 프로파일링은 별도 user-space 툴 **Chameleon**(PEBS 기반, CPU 오버헤드 코어당 3-5%)으로 수행. Meta 4개 production workload(Web/Cache/DataWarehouse/Ads)를 FPGA CXL 1.1 카드에서 평가 — TPP는 all-local ideal baseline과 **<1% gap**, default Linux 대비 **최대 18%**, NUMA Balancing·AutoTiering 대비 **5-17%** 개선. 패치 대부분이 **Linux v5.18에 머지**됨.

---

## Core thesis
> "We propose a novel OS-level application-transparent page placement mechanism (TPP) for CXL-enabled memory. TPP employs a lightweight mechanism to identify and place hot/cold pages to appropriate memory tiers." (Abstract)
> "an effective page placement mechanism should efficiently offload cold pages to slower CXL-Memory while aptly identify trapped hot pages in CXL-node and promote them to the fast memory tier." (§5)

CXL-Memory는 SW 관점에서 CPU-less NUMA node일 뿐이라, tiered placement가 **원리상 OS의 NUMA/reclaim 메커니즘 재설계 문제**로 환원된다. 하지만 Linux의 동기적 reclamation과 상태 무시 promotion이 hot page를 느린 tier에 가두므로, **경량 demotion + alloc/reclaim 분리 + active-LRU 기반 promotion + page-type 인지 allocation**으로 두 tier의 장점을 투명하게 취할 수 있다.

---

## Why this matters to me
TPP는 [[SkyByte]]의 비교축을 이해하는 **필수 baseline**이다. SkyByte는 hot/cold 마이그레이션을 **하드웨어(디바이스 내부)**에서 하는데, 그 대조군인 **SkyByte-CT**가 바로 TPP류 **OS/software 페이지 배치**다. 즉 TPP를 알면 "왜 SkyByte가 마이그레이션을 HW로 내렸나"라는 질문의 반대편을 정확히 잡을 수 있다. 내 발표축 **"SSD 이중역할 × transparent co-design"**에서 TPP는 **software 쪽 극단**(OS가 온도 감지·마이그레이션 전담, 디바이스는 dumb memory)이고 SkyByte는 **hardware 쪽 극단**(디바이스가 자기 데이터를 능동 이주)이다. 결정적으로 TPP는 **single-host** tiered memory(한 host의 local↔CXL NUMA node)만 다룬다 — CXL tier가 **여러 host에 공유**되는 순간 TPP의 per-host page-temperature 모델이 깨지는 지점이 내 multi-node coherence 방향이 채울 빈칸이다.

---

## Structure overview
| § | Title | Pages | Key takeaway |
|---|---|---|---|
| 1 | Introduction | p.1-2 | CXL-Memory = CPU-less NUMA node; Linux는 동종 DRAM 전제라 CXL에서 부진 → OS-level 투명 배치 TPP 제안, Linux v5.18 머지 |
| 2 | Motivation | p.2-3 | 메모리가 rack TCO의 33% · power 37%; homogeneous 서버 설계의 CPU-memory 강결합 한계; CXL이 tiered memory 가능케 함 |
| 3 | Characterizing Datacenter Apps (Chameleon) | p.3-6 | PEBS 기반 경량 user-space 툴 Chameleon; 4 workload; 55-80% idle, anon>file 온도, 접근 패턴 수분~수시간 안정 |
| 4 | Design Principles of TPP | p.6-7 | 구현층=kernel; 온도 감지=minor page fault+LRU(PEBS 아님); CXL은 swap 아닌 load/store tier |
| 5 | TPP for CXL-Memory | p.7-9 | 4축: 경량 demotion · alloc/reclaim 분리(watermark) · active-LRU promotion(ping-pong 방지) · page-type-aware allocation |
| 6 | Evaluation | p.9-12 | 2:1·1:4 config, Meta 4 workload; ideal 대비 <1%, Linux 18%, NUMA/AutoTiering 5-17%, TMO는 orthogonal |
| 7 | Discussion & Future | p.12 | multi-tenant QoS, bandwidth-expansion allocation, HW 지원(CXL ASIC memory-side cache) |
| 8 | Related Work | p.12-13 | tiered memory·page placement·disaggregated memory(RDMA는 orthogonal) |
| 9 | Conclusion | p.13 | 첫 end-to-end 실전 배포 가능 CXL-Memory 시스템; Linux 18%, SOTA 5-17% 개선 |

---

## Section notes

### §1 Introduction (p.1-2)
CXL[7]은 memory를 compute에서 **분리(decouple)**해 DRAM-like bandwidth + cache-line granular access의 **중간 지연 tier**를 제공한다(Fig.1). SW 관점에서 CXL-Memory는 **CPU-less NUMA node**로 보이고, 그 특성(대역폭·용량·세대·기술)은 CPU 직결 메모리와 독립적이다. 문제: Linux 메모리 관리는 **동종 CPU-attached DRAM-only** 시스템용이라 tier마다 접근 지연이 다른 CXL 시스템(Fig.2)에서 성능이 나쁘다. TPP는 hot/cold를 경량으로 판별해 적절 tier에 두는 OS-level 투명 배치. **핵심 대비**: local node가 차면 default reclamation이 swap으로 내보내고 그 동안 새 allocation이 CXL로 몰림 + 느린 CXL의 hot page를 fast tier로 즉시 올림. 기여: **Chameleon**(경량 특성화 툴), **TPP**(대부분 Linux v5.18 머지), CXL 1.1 실기(pre-production x86 + FPGA) 평가.

> "From a software perspective, CXL-Memory appears to a system as a CPU-less NUMA node where its memory characteristics ... are independent of the memory directly attached to the CPU." (§1, p.1)

### §2 Motivation (p.2-3)
메모리는 Meta rack TCO의 **최대 33.3%**, power의 **최대 37.1%**로 성장(Fig.3). Homogeneous 서버 설계의 한계: (a) 메모리 컨트롤러가 단일 세대만 지원, (b) 용량이 2의 거듭제곱 단위, (c) DRAM 세대당 bandwidth-vs-capacity 점이 제한 → 대역폭 얻으려 용량 과투자, stranded 자원 발생. 기존 확장 버스(HyperTransport·Gen-Z·OpenCAPI)는 독점적·범용성 부족·coherency 결여. **CXL**[7]은 PCIe 5.0 기반 open interconnect로 byte-addressable·cache-line granular·HW coherency 유지, DRAM 대비 **50-100ns** 추가 지연(Fig.5: ~170-250ns)의 NUMA-like tier를 만든다 → slow tier 후보로 이상적. Datacenter workload는 메모리를 항상 다 쓰지 않아(55-80% idle) tiering 여지가 크다.

### §3 Characterizing Datacenter Applications — Chameleon (p.3-6)
기존 특성화 툴의 한계: IPT(Idle Page Tracking)는 tracking cycle 내 다중 접근을 1회로만 세고 물리주소 공간만 봄, 오버헤드 20-90%; 복잡한 PEBS user-space 툴은 코어당 15%+ CPU 오버헤드. → **Chameleon**: 경량 user-space 툴, kernel 수정·app 중단 없이 production에 바로 투입. 두 컴포넌트: **Collector**(PEBS로 LLC miss 로드/TLB miss 스토어 샘플, 200 event당 1샘플, 코어 그룹 duty-cycling) + **Worker**(가상↔물리 주소 변환, 페이지별 64-bit bitmap으로 interval별 activeness 추적). 오버헤드 단일 코어의 **3-5%**(메모리 대역폭 포화 합성 workload에선 7% 손실). 4개 production workload — **Web**(HHVM/Python), **Cache**(분산 메모리 객체 캐시, tmpfs), **DataWarehouse**(병렬 배치 쿼리), **Ads**(ML 연산).

핵심 관측(§3.2-3.6):
- **온도**: 2분 interval 안에 할당 메모리의 **55-80%가 idle**(DataWarehouse조차 hot은 평균 20%).
- **page type별 온도**: **anon page가 file page보다 뜨겁다**(Fig.8). Web은 anon 35-60% hot vs file 3-14% hot.
- **시간 안정성**: 접근 패턴이 수분~수시간 안정 → kernel-space에서 배치 결정할 시간 충분.
- **재접근**: cold page가 나중에 다시 hot 됨. Web은 80%가 10분 내 재접근 → 무작위 offload 위험.
- **동적성**: (de)allocation으로 물리주소의 hot↔cold가 빠르게 바뀜 → **정적 배치는 성능 악화**.

> "55-80% of an application's allocated memory remains idle within any two minutes interval." (§2/§3.2, p.3-5)

### §4 Design Principles of TPP (p.6-7)
세 질문에 답으로 설계: (1) **구현층** — user-space(Chameleon류 + move_pages())는 context switch·history 관리 오버헤드가 커 큰 working set에 안 맞음 → **kernel-space**가 덜 복잡하고 더 빠름. (2) **온도 감지** — PEBS는 CPU 벤더 간 비표준·counter 제한·상시 구동 부담; IPT/poisoning은 accessed bit clear + TLB flush로 고빈도 감시 시 심한 slowdown; Thermostat[31]은 2MB huge-page 전용. TPP 목표는 **page-size agnostic** → **NUMA Balancing의 minor page fault**만 온도 감지에 쓰고(오버헤드 낮게 유지), local node cold 감지는 **Linux 기존 LRU**로 충분(가상 zero-overhead). (3) **CXL 추상화** — swap-space로 쓰면(TMO의 zswap 등) 매 접근이 major page fault → 유효 지연이 200ns 훨씬 초과, CXL의 **load/store cache-line 접근이라는 핵심 가치 상실** → TPP는 CXL을 **main memory tier**로 다뤄 load-store로 warm/cold data 접근.

> "One of our design goals behind TPP is that it should be agnostic to page size." (§4, p.6)

### §5 TPP for CXL-Memory (p.7-9) — 네 축
설계공간 4영역: (a) 경량 demotion, (b) alloc/reclaim 분리, (c) hot-page promotion, (d) page-type-aware allocation.

**§5.1 경량 reclamation을 위한 migration**: local node가 차면 기본 reclamation은 swap으로 page-out → 느려서 그 동안 새 allocation이 CXL-node로 몰림. TPP는 reclamation 후보를 swap 대신 **별도 demotion list**에 넣어 CXL-node로 **비동기 migration**(migration이 swapping보다 수 order 빠름). inactive page부터 스캔해 hot page migration 확률을 낮춤. migration 실패 시 기본 reclamation으로 fallback. 다중 CXL-node면 CPU 거리 기반 static 선택으로 충분.

**§5.2 Allocation과 Reclamation 분리**: Linux는 zone별 min/low/high watermark. free가 low 아래면 reclamation 시작하고 새 allocation은 high 채울 때까지 **정지(halt)** → 고할당률에서 reclamation이 못 따라가 CXL-node로 page가 몰려 성능 악화. TPP는 **'reclamation 정지'와 '새 allocation 발생' 로직을 분리** — free가 **demotion_watermark**에 닿을 때까지 background reclamation 지속, 새 allocation은 별도 **allocation_watermark**만 만족하면 진행. demotion_watermark를 항상 alloc/low보다 높게 둬 **free headroom**을 유지 → (1) 짧고 뜨거운 새 할당을 local에 직접 매핑, (2) CXL의 trapped hot page promotion 수용. reclamation 공격성은 **/proc/sys/vm/demote_scale_factor**(기본 2%)로 조절.

**§5.3 CXL-node로부터의 promotion**: 압박으로 CXL에 간 page나 나중에 hot 된 demoted page를 안 올리면 **hot page가 CXL-node에 영영 갇힌다(trapped)**. TPP는 **NUMA Balancing을 증강** — kernel task가 각 node의 일부(기본 256MB) 샘플, remote CPU 접근 시 NUMA hint fault로 promote. 단 local의 hot을 다른 node로 올리는 건 무의미하므로 **CXL-node만 sampling**. **Ping-pong 문제**: 기본 NUMA Balancing은 active 상태 확인 없이 **즉시 promote** → 드물게 접근되는 page도 올라가 곧 demotion 후보가 되는 왕복. 해결: page의 **LRU 위치(age)** 확인 — **active LRU에 있을 때만** promotion 후보(1,Fig.13). inactive면 accessed 표시 후 active로 옮기고 다음 hint fault 때 승격 → **hysteresis** 부여. anon/file 별도 LRU라 type별 promotion rate 차등, 수렴 가속.

**§5.4 Page-type-aware allocation**: file cache는 warmup 중 많이 생기고 잘 안 쓰이는데 local을 점유해 anon을 CXL로 밀어냄. → **file cache/tmpfs는 CXL-node에 우선 할당**, anon은 local 정책 유지. hot 된 file cache는 나중에 promote. 작은 local + 크고 싼 CXL 구성에서도 성능 유지.

**§5.5 Observability**: /proc/vmstat 카운터로 demotion/promotion 통계 노출. **PG_demoted 플래그**(page flag의 미사용 0x40 bit)로 ping-pong(demote→재promote) 추적.

> "Without any promotion mechanism, hot pages will always be trapped in CXL-nodes and hurt application performance. To promote such pages, we augment Linux's NUMA Balancing." (§5.3, p.8)

### §6 Evaluation (p.9-12)
Linux v5.12에 통합. Pre-production x86 CPU + **FPGA 기반 CXL 1.1** 확장 카드(CXL-Memory가 CPU-less NUMA node로 노출). 성능 평가는 CXL 특성을 흉내낸 dual-socket로도 수행. 두 config: **2:1**(local:CXL 용량, production 유사, local이 hot working set 담당)과 **1:4**(스트레스 — hot의 일부만 local에 들어감). metric = app throughput + local node 트래픽 비율.

- **§6.1.1 (2:1) Web1**: default Linux는 local 비우는 게 TPP보다 **44× 느려** 새 allocation이 CXL에 영구 정착 → local 트래픽 22%, throughput **16.5% 감소**. TPP는 local 트래픽 90%, throughput 드롭 **0.5%**.
- **Cache1/Cache2 (2:1)**: default 3%·2% 감소를 TPP가 99.9%·99.6%로 회복.
- **DataWarehouse (2:1)**: default도 충분(0.5-0.7% 드롭). TPP는 anon 94%를 local에.
- **§6.1.2 (1:4) Cache1**: default는 anon 85%가 CXL에 갇혀 14% 드롭. TPP는 hot anon 97%를 local로 → local 20% 용량으로도 트래픽 85%, 드롭 **0.5%**.
- **(1:4) Cache2**: default **18% throughput loss**(anon 14%만 local). TPP는 hot anon 80% 회복, CXL 트래픽 41%여도 드롭 **5%**.
- **§6.1.3 CXL 지연 변동**: default Linux는 hot의 22-25%가 CXL에 갇혀 평균 메모리 접근 지연 **7×** 증가, throughput loss가 TPP보다 **2.2-2.8×** 큼. TPP는 hot의 4-5%만 CXL.
- **§6.2 컴포넌트**: **decoupling** 없으면 promotion 거의 정지(CXL trapped page가 55% 트래픽 → 12% 드롭); 있으면 promotion 평균 50KB/s(99%tile 1.2MB/s), local allocation rate 95%tile **1.6×**. **active-LRU** promotion은 promotion rate **11×↓**, demote-후-재promote 50%↓, promotion 성공률 **48%↑**(수렴은 5분 더 소요). **page-type-aware**(Table 2): Web1 2:1 local 97%/throughput 99.5%, Cache1 1:4 85%/99.8%, Cache2 1:4 72%/98.5%.
- **§6.3 SOTA 비교**: **NUMA Balancing** — Web1 throughput 17.2% 드롭(reclamation 42×·promotion 11× 느림, local 20%만); Cache1 1:4 46% local, 10% 드롭. **AutoTiering** — 1:4에서 크래시, 2:1에서도 TPP가 7% 우위. **TMO**[73] — **orthogonal·complementary**: TPP가 zswap을 **demote-then-swap 2단계**로 만들어 TMO의 process stall **30%↓**, memory saving **3%↑**; TMO는 free headroom을 만들어 TPP migration 실패율↓.
- steady-state migration 대역폭 **4-16MB/s(1-4K pages/s)** — CXL 링크 대역폭보다 훨씬 낮아 CPU 부담 미미.

> "TPP makes a tiered memory system performant as an ideal baseline (<1% gap) that has all the memory in the local tier. It is 18% better than today's Linux, and 5-17% better than existing solutions including NUMA Balancing and AutoTiering." (Abstract)

### §7 Discussion & Future Research (p.12)
- **Multi-tenant cloud**: 여러 tenant가 한 host를 공유하면 QoS 요구가 다를 때 TPP가 sub-optimal → QoS-aware 메모리 관리를 TPP 위에 얹어야.
- **Bandwidth-expansion allocation**: bandwidth-bound app엔 cold만 CXL에 두는 게 아니라 bandwidth-heavy·latency-insensitive page를 CXL에 분산해야(HW 지원 필요할 수 있음).
- **HW 지원**: CXL ASIC의 **memory-side cache + prefetcher**로 유효 지연↓, tier 간 data movement HW 지원으로 migration 오버헤드↓.

### §8 Related Work (p.12-13)
Tiered memory(NVM 확장), page placement(HW-assisted·app-guided는 datacenter 확장성 부족), transparent placement는 물리/가상 주소로 온도 감지하나 TLB invalidation·interrupt로 오버헤드 큼 → TPP는 in-kernel LRU로 충분. Nimble[76]은 huge-page migration 최적화지만 demotion이 critical path의 promotion을 기다려 성능 악화. AutoTiering[47]·Huang et al.[28]은 background migration + NUMA balancing이나 timer 기반 hot 감지가 비효율적이고 **alloc/reclaim 분리를 안 함**. **Disaggregated memory**(RDMA over InfiniBand/Ethernet)는 CXL보다 지연이 수 order 높고 TPP와 **orthogonal** — CXL·network tier를 함께 쓰며 각 tier에 TPP·disaggregation 적용 가능.

### §9 Conclusion (p.13)
Chameleon으로 datacenter 메모리 사용을 분석하고, 사전지식 없이 동작하는 OS-level 투명 배치 TPP를 설계 — Linux 대비 18%, NUMA Balancing·AutoTiering 대비 5-17% 개선. **hyperscale에 즉시 배포 가능한 첫 end-to-end 실전 CXL-Memory 시스템**을 특성화·평가한 것으로 자평.

---

## Key vocabulary
**Thesis / framing:**
- "OS-level application-transparent page placement"
- "CXL-Memory appears ... as a CPU-less NUMA node"
- "hot/cold page placement to appropriate memory tiers"

**Technical concepts:**
- "lightweight reclamation / async demotion (migration to CXL-node)"
- "decoupling allocation and reclamation" (demotion_watermark vs allocation_watermark)
- "trapped hot pages" / "ping-pong due to opportunistic promotion"
- "active-LRU-based hot page detection" / "hysteresis to page promotion"
- "page type-aware allocation" (anon→local, file cache→CXL)
- "minor page fault as temperature detection" (augmented NUMA Balancing)
- "Chameleon" (PEBS-based lightweight user-space characterization tool)

**Value language:**
- "transparently without any application-specific knowledge"
- "as performant as an ideal ... all-local baseline (<1% gap)"
- "can be deployed globally as a kernel release"

> ⚠ **피해야 할 어휘** (TPP-signature, echo 주의):
> - "transparent page placement" (이 논문 고유 프레이밍 — 그대로 쓰면 모방으로 보임)
> - "trapped hot pages" / "ping-pong due to opportunistic promotion"
> - "decoupling allocation and reclamation"

---

## Citable quantitative data
| 출처 (§/page) | 데이터 | 인용 맥락 |
|---|---|---|
| §2, p.2 (Fig.3) | 메모리가 rack TCO의 최대 **33.3%**, power의 **37.1%** (Meta) | 메모리 비용 위기 → tiering 동기 |
| §2, p.3 | CXL이 DRAM 대비 **50-100ns** 추가 지연(≈170-250ns) | CXL slow-tier 지연 특성 |
| §3, p.4 | 2분 interval 안에 할당 메모리 **55-80% idle** | cold 메모리 offload 여지 |
| Abstract | ideal all-local 대비 **<1% gap**, Linux 대비 **18%**, NUMA/AutoTiering 대비 **5-17%** | TPP 종합 성과 |
| §6.1.3, p.10 | default Linux는 hot의 22-25%가 CXL에 갇혀 avg latency **7×**, throughput loss **2.2-2.8×** | trapped hot page의 대가 |
| §6.2, p.11 | active-LRU promotion → promotion rate **11×↓**, 성공률 **48×↑** | ping-pong 억제 효과 |
| §6/§7, p.12 | steady-state migration **4-16MB/s (1-4K pages/s)** | migration 오버헤드 미미 |

---

## 🎯 Strategic anchor
> "In such a system, as memory access latency varies across memory tiers, application performance greatly depends on the fraction of memory served from the fast memory ... Linux's memory management mechanism is designed for homogeneous CPU-attached DRAM-only systems and performs poorly on CXL-Memory system." (§1, p.1-2)

→ **본인 활용**: TPP는 "CXL-Memory = **한 host의** CPU-less NUMA node"라는 전제 위에서만 성립한다 — page-temperature도, LRU도, watermark도 전부 **단일 host의 물리주소 공간** 안 이야기다. 면담에서 "TPP가 single-host tiered placement를 OS로 완성했다면, 그 다음 축은 **CXL tier가 여러 host에 공유될 때**"라고 이어갈 수 있다. 공유되는 순간 host A의 promotion(로컬로 끌어올림)이 host B의 view를 stale로 만들고, TPP의 per-host 온도 모델이 깨진다 → 여기가 **multi-host coherence(HDM-DB/back-invalidate)**가 필요한 지점이자 내 방향의 진입점.

---

## Connection to my research direction
| 차원 | TPP (2023) | SkyByte (2025) | 내 방향 |
|---|---|---|---|
| 배치 주체 | **OS/software**(kernel) | **하드웨어**(디바이스 내부 migration) | multi-host 조율 계층 |
| 온도 감지 | minor page fault + LRU (per-host) | HW counter (디바이스) | cross-host 접근성 |
| 대상 | CXL tiered-memory(Type-3, DRAM/NVM) | memory-semantic **CXL-SSD** | 공유 memory pool |
| 마이그레이션 | async demotion + NUMA-balance promotion | HW-assisted adaptive migration | multi-node migration |
| 일관성 범위 | **single-host** NUMA node | **single-host** device | **multi-host (HDM-DB/BI)** |
| 공통 한계 | single-host, per-host 온도 | single-host | 이걸 넘는 게 목표 |

TPP와 SkyByte는 **같은 문제(hot/cold 배치)를 다른 층에서** 푼다 — TPP는 OS가 온도 감지·마이그레이션을 전담(디바이스는 dumb memory), SkyByte는 디바이스가 자기 데이터를 능동 이주(SkyByte-CT가 TPP류 OS baseline). 내 연구는 **층이 아니라 scope**를 바꾼다: 둘 다 "한 host가 자기 tier를 본다"를 전제하는데, **여러 host가 같은 CXL tier를 공유**하면 TPP의 page-temperature는 host마다 달라 충돌하고, promotion으로 로컬에 끌어온 사본이 다른 host에 stale를 노출한다. 즉 TPP의 **watermark·LRU·promotion 정의가 전부 cross-host coherence 위에서 재설계 대상**이 된다. (→ [[SkyByte]], [[CXL Multi-node Coherence]], [[CXL Overview]])

---

## Open questions / gaps
- [ ] TPP는 **single-host** local↔CXL NUMA node만 다룸 — CXL tier가 **여러 host에 공유(pooling)**되면 page-temperature가 host마다 달라 배치 결정이 충돌.
- [ ] promotion(로컬로 migration)은 그 host의 물리 사본을 만든다 — **공유 시 다른 host는 stale**를 봄 → cross-host invalidation(CXL 3.0 back-invalidate) 필요. TPP엔 없음.
- [ ] watermark·LRU·PG_demoted가 전부 **단일 host address space** 전제 — pool 전체 온도/소유권은 미정의.
- [ ] 온도 감지를 minor page fault에 의존 → **여러 host의 접근**을 한 host의 fault로는 못 봄(remote access가 fault를 안 냄).
- [ ] HW-assisted migration(SkyByte류)이 TPP의 SW promotion보다 나은 지점 vs TPP의 kernel-transparency 이점 — 워크로드·공유도 의존.
- [ ] QoS-aware multi-tenant는 §7에서 future로만 언급 — 우선순위 다른 tenant 간 tier 경쟁 미해결.

---

## References worth following up
| 상태 | Ref [N] | Paper | 왜 봐야 |
|---|---|---|---|
| ☐ | [73] | Weiner et al., **TMO: Transparent Memory Offloading** (ASPLOS 2022) | TPP와 orthogonal·complementary한 swap 기반 offload — 2단계 demote-then-swap 이해 |
| ☐ | [47] | Kim et al., **AutoTiering** (USENIX ATC 2021) | TPP의 주요 비교 대상 — timer 기반 hot 감지·alloc/reclaim 미분리의 한계 |
| ☐ | [22] | **NUMA Balancing (AutoNUMA)** | TPP promotion의 토대 — minor page fault(NUMA hint fault) 메커니즘 |
| ☐ | [31] | Agarwal & Wenisch, **Thermostat** (ASPLOS 2017) | 2MB huge-page 전용 transparent 관리 — TPP의 page-size-agnostic 목표 대비 |
| ☐ | [52] | Li et al., **Pond: CXL-Based Memory Pooling** (ASPLOS 2023) | multi-host **memory pooling** — 내 multi-node 방향의 직접 인접작 |
| ☐ | [41] | Gouk et al., **DirectCXL** (USENIX ATC 2022) | CXL 메모리 disaggregation 실기 — pool 접근 지연 근거 |

---

## Personal annotations
<!-- 본인 메모 영역 -->
