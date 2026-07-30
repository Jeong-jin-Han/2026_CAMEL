---
title: "PACT: A Criticality-First Design for Tiered Memory"
description: "access frequency 대신 CPU stall 기여도(Per-page Access Criticality, PAC)를 온라인으로 추정해 tiered memory 페이지 배치를 결정하는 criticality-first 시스템"
venue: ASPLOS
year: 2026
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/cxl
  - venue/asplos
  - year/2026
  - list/26s-v2
  - topic/tiered-memory
  - topic/page-migration
  - topic/memory-level-parallelism
  - topic/hotness-vs-criticality
---

# PACT: A Criticality-First Design for Tiered Memory
> **ASPLOS 2026** · cluster/cxl · Source: [PACT - A Criticality-First Design for Tiered Memory.pdf](<PACT - A Criticality-First Design for Tiered Memory.pdf>)

저자: Hamid Hadian (Virginia Tech), Jinshu Liu (Virginia Tech), Hanchen Xu (Virginia Tech), Hansen Idden (Virginia Tech), Huaicheng Li (Virginia Tech) — 전원 Virginia Tech, Blacksburg, USA

## TL;DR
기존 tiered memory 시스템은 페이지를 access frequency("hotness")로 판단해 배치하지만, 실제 성능 영향(criticality)은 access pattern·MLP(memory-level parallelism)·latency에 따라 크게 달라 frequency만으로는 부정확하다. 이 논문은 각 페이지 접근이 CPU stall에 기여하는 정도를 온라인으로 정량화하는 fine-grained 지표 Per-page Access Criticality(PAC)를 제안하고, 이를 단 4개의 표준 CPU perf counter(LLC-miss + per-tier MLP)만으로 추정하는 경량 분석 모델을 개발한다. PACT는 이 PAC를 기반으로 eager demotion과 adaptive promotion(Freedman-Diaconis binning + reservoir sampling) 정책을 설계해, 13개 워크로드에서 최고 성능의 기존 tiering 시스템 대비 최대 61% 성능 향상과 최대 50배 적은 migration을 달성한다.

## 문제 & 동기
기존 tiered memory 관리 기법 대부분은 access frequency("hotness")를 기준으로 어떤 페이지를 fast tier(DRAM)에 둘지 결정한다. 그러나 modern out-of-order CPU는 MLP를 이용해 여러 memory request를 동시에 issue함으로써 latency를 amortize하므로, 접근 빈도가 같아도 실제 CPU stall에 미치는 영향(=criticality)은 access pattern에 따라 크게 다르다(p.1366-1367). 저자들은 Masim(합성 벤치마크), GUPS, tc-twitter(GAPBS) 세 워크로드를 CXL(190ns) 환경에서 profiling해, 동일 access frequency를 가진 페이지들 사이에서도 PAC(stall cost)가 최대 65배 차이 남을 보인다(p.1368, Figure 1, Takeaway #1).

> [!quote]- 📄 원문 표현 (paper)
> - "Tiered memory systems typically place pages based on access frequency (hotness), yet frequency alone fails to capture the true performance impact." (p.1366, Abstract)
> - "Pages with the same access frequency can differ in stall cost by up to 65× within a single application." (p.1368, Takeaway #1)
> - "Consider two pages with identical access frequency: one accessed during pointer-chasing (serialized) and another during array traversal (concurrent)... these pages have vastly different performance criticality." (p.1367)

## 핵심 통찰 (Key Insight)

### 1. Per-tier CPU stall은 LLC-miss와 per-tier MLP만으로 정확히 모델링 가능
96개 실제 워크로드(in-memory caching, graph processing, ML, HPC)에 대한 대규모 분석 결과, per-tier stall은 $\text{LLC-stalls} = k \times \frac{\text{LLC-misses}}{\text{MLP}}$ (Equation 1)로 모델링되며, 세 latency 구성(local DRAM 90ns / NUMA 140ns / CXL 190ns) 각각에서 Pearson correlation coefficient가 0.98을 넘는다(단순 LLC-miss만 쓰면 0.82-0.89). $k$는 tier별 latency·아키텍처 상수를 흡수하는 계수다(p.1369-1370, Figure 2). 이 모델이 효과적인 이유는 Little's Law + queueing theory 관점에서 각 LLC miss가 tier latency에 비례한 cost를 발생시키지만 MLP가 겹치는 실행 윈도우로 이 cost를 amortize하기 때문이다.

> [!quote]- 📄 원문 표현 (paper)
> - "Despite the lack of direct hardware support, per-tier CPU stalls can be accurately modeled as a function of LLC-misses and per-tier MLP." (p.1369, Takeaway #2)
> - "the model exhibits a significantly stronger linear relationship with LLC stalls, with Pearson correlation coefficients of 0.98 for the three configurations, versus 0.82–0.89 for LLC-miss." (p.1370)

### 2. Per-tier MLP는 CPU의 CHA/TOR queue occupancy로 직접 측정 가능
기존 offcore metric(예: Intel `Info_Memory_Latency_Load_L2_MLP`)은 system-wide MLP만 제공하고 tier별 구분이 없다. PACT는 CHA(Caching and Home Agent)의 TOR(Table Of Requests) queue occupancy counter를 이용해 MLP = $T_1/T_2$ (T1=TOR_OCCUPANCY, T2=TOR_OCCUPANCY_COUNTER0, 즉 outstanding request가 있던 cycle 수)로 정의, per-tier MLP를 실시간 측정한다(p.1370, Table 1, Takeaway #3). AMD Zen4에서는 TOR-like queue가 없어 Little's Law 기반 근사(MLP ≈ Latency × Bandwidth)로 대체 가능하다(p.1372).

> [!quote]- 📄 원문 표현 (paper)
> - "Per-tier MLP can be measured using CPU CHA queues' occupancy, which reflects the number of concurrent requests serviced by each memory tier." (p.1370, Takeaway #3)

### 3. Workload는 MLP가 tens-of-ms 단위로 안정적인 "phase" 구조를 가진다
MLP는 짧은 시간(수십 ms) 동안 거의 일정(quasi-stationary)하다가 워크로드 phase가 바뀔 때(초 단위 스케일)만 변한다. 이 phase-level stability 덕분에, 짧은 sampling window 내에서는 접근 빈도에 비례해 stall을 개별 페이지에 배분하는 proportional attribution이 정확한 근사가 된다(p.1370-1371, Figure 3, Takeaway #4). 이것이 online PAC 추정을 가능하게 하는 핵심 전제다.

> [!quote]- 📄 원문 표현 (paper)
> - "Per-tier MLP exhibits periodic stability, allowing uniform attribution of CPU stalls to individual memory accesses within each sampling window." (p.1370, Takeaway #4)
> - "workloads exhibit stable execution phases, in which MLP remains consistent for tens of milliseconds, while evolving over time to reflect changes in memory access patterns." (p.1367)

## 설계 / 메커니즘 (Design)

**PAC Sampling (Algorithm 1, p.1372)**: 20ms마다 (1) slow-tier LLC-misses·MLP를 $T_1/T_2$ delta로 측정, (2) Equation 1로 total stall $S = k \cdot \text{LLC-misses}/\text{MLP}$ 추정, (3) Intel PEBS(400-in-1 샘플링 rate, 5MB 압축 버퍼)로 개별 페이지 접근(vaddr, access count $A_p$)을 샘플링, (4) 각 페이지에 $S_p = S \cdot A_p/A_t$로 stall을 비례 배분, (5) optional cooling factor $\alpha \in [0,1]$로 EWMA 갱신($\text{PAC}[p] \leftarrow \alpha \cdot \text{PAC}[p] + S_p$). Table 1에 소요 counter가 명시됨: PEBS `MEM_LOAD_L3_MISS_RETIRED`, `TOR_OCCUPANCY`($T_1$), `TOR_OCCUPANCY_COUNTER0`($T_2$).

**Eager Demotion (4.4.1, Algorithm 2 p.1373)**: memory pressure를 기다리지 않고 fast tier에서 선제적으로 LRU 기반 페이지를 demote해 promotion용 headroom을 확보한다. demoted count $N_{\text{demoted}}$가 promoted count $N_{\text{promoted}} + m$ 미만이면 추가 demotion을 트리거(파라미터 $m$이 공격성 조절, 기본값 $m=0$).

**Adaptive Promotion (4.5, Algorithm 3 p.1374)**: PAC 분포가 workload마다 극도로 skewed하고 시간에 따라 변하므로 고정 threshold binning은 부적합하다. PACT는 (1) Reservoir sampling(100-entry 고정 크기, 신규 페이지 도착 시 $rnd < 100$ 조건으로 균일 교체)으로 전체 PAC 분포를 대표하는 표본을 유지하고, (2) 이 표본의 사분위수($Q_1, Q_3$)로부터 Freedman-Diaconis rule $W = 2 \times (Q_3 - Q_1)/\sqrt[3]{N_\text{page}}$로 bin width를 동적으로 계산하며, (3) promotion 후보 수 대 전체 페이지 비율($N_\text{page}/N_c$)이 threshold $T_\text{scale}$을 넘으면 bin width를 2배로 넓히고(scale up), 아래면 절반으로 줄이는(scale down) scaling optimization을 추가한다. 최상위 priority bin(전체의 1-5%)이 promotion 후보가 된다.

**Implementation (4.6)**: Linux 5.15 기반, TPP의 LRU-based demotion 신뢰성 수정 일부 반영, OS 기본 NUMA hint fault scanning 비활성화, PEBS record에서 미사용 필드 제거(5MB 버퍼로 압축), perf subsystem을 확장해 PMU counter로부터 PAC를 직접 계산, PEBS 처리·migration 각각에 dedicated thread 2개 사용. Tracked 4KB 페이지당 25 bytes(0.6% memory overhead)만 소요.

> [!quote]- 📄 원문 표현 (paper)
> - "PACT reduces overhead in the PEBS kernel interface by stripping unused fields from PEBS records, which allows us to use a compact 5MB buffer... PACT incurs minimal overhead, requiring 25 bytes per tracked 4KB page (0.6% memory overhead)." (p.1374)
> - "PACT dynamically adjusts the bin width based on the current distribution of PAC values. When the ratio N_page/N_c... exceeds a predefined threshold T_scale, PACT doubles the bin width." (p.1374)

## 평가 (Evaluation)
**환경**: dual-socket Intel Skylake(CloudLab), 10-core Xeon 2.2GHz, 96GB DDR4; local DRAM 90ns/52GB/s, NUMA 140ns/32GB/s, CXL 에뮬레이션 190ns(2.1× DRAM latency)(p.1375, §5.1). 13개 메모리 집약 워크로드(GAPBS graph analytics, GPT-2 inference, in-memory DB, SPEC CPU2017), footprint 6.6-40GB. 비교 대상 7개 SOTA: Soar, Alto, Memtis, Colloid, Nomad, TPP, NBT(Linux NUMA Balancing Tiering), 그리고 NoTier 베이스라인.

- **bc-kron (4KB, 7개 fast:slow 비율)**: PACT가 모든 baseline을 일관되게 능가하며 migration도 훨씬 적음. 예: Colloid의 slowdown이 pressure 증가에 따라 26%→59%로 급증하는 반면 PACT는 안정적(Figure 4, Table 2, p.1375). Takeaway #5: "PACT... promoting up to 10.4× fewer pages than the 2nd best for both 4KB and THP configurations." (p.1375)
- **bc-kron (THP)**: PACT가 모든 비율에서 가장 낮은 slowdown, baseline 대비 2-22% 우수, Colloid/NBT 대비 2.1-10.4배, Nomad 대비 1.2-9.6배 적은 promotion(Figure 5, p.1376).
- **12개 워크로드 종합(1:1 ratio)**: Takeaway #6 "PACT maintains robust performance advantage and migrates up to 50.1× and 40.6× fewer pages than Colloid and NBT, respectively." (p.1376) Figure 6에서 Colloid 대비 최대 33%, Nomad 대비 500% 이상 우수.
- **1:2 / 2:1 ratio CDF (Figure 7)**: Colloid·NBT·Memtis 대비 평균 개선 1:2에서 9.95%(Colloid) / 4.99%(NBT) / 16.6%(Memtis), 2:1에서 이와 유사(§5.3), peak 57%/20%/48%.
- **vs Alto/Soar (§5.4)**: PACT는 12개 중 8개 워크로드에서 Alto를 능가, 평균 7.6% 개선. Soar(offline object-level profiling) 대비는 대체로 2-3.7%/7-10%/7-9% 근소하게 열세(평균 3.3%), 일부(603.bwaves, bc-urand, sssp-kron)는 4-28%/16-48%/14-21% 열세 — object-level offline profiling의 이점이 드러나는 사례.
- **PACT Adaptivity (sssp-kron, Figure 8)**: Colloid는 8M+ migration을 트리거하는 반면 PACT는 단 180K(약 1/10 수준)로 더 낮은 slowdown(18% vs 25%) 달성(p.1377).
- **PAC vs Frequency (§5.6, Figure 9-10)**: 동일 migration count 조건에서 PAC 기반 정책이 frequency-only 정책 대비 18% 성능 향상, bc-urand/sssp-kron/silo에서 12-22% 우수.
- **Sensitivity (§5.7, Figure 10)**: PEBS rate 800→4000(sparser)일수록 slowdown 23%→30%로 증가; PAC sampling period 10ms→1000ms일수록 promotion 800K→1.7M, slowdown 20%→27%로 증가; cooling factor는 기본값 $\alpha=1.0$(no cooling)이 대체로 최선.
- **Bandwidth contention (§5.8)**: Colloid 대비 3.5-4.7배, Memtis 대비 2.2배 적은 migration으로 saturated bandwidth 하에서도 성능 유지(Takeaway #8, p.1379).
- **Colocation microbenchmark (§5.9, Figure 12)**: 두 개의 이질적 Masim 프로세스(sequential vs random) 동시 실행 시 Colloid 대비 sequential 112%, random 28%, 전체 61% slowdown 개선, PACT는 300K promotion으로 Colloid의 12M 대비 압도적으로 적음.
- **Redis/YCSB-C (§5.10, Figure 13)**: "+Both"(adaptive binning + scaling) 구성이 Colloid 대비 latency·throughput 최대 40% 개선, tail latency 크게 감소.

> [!quote]- 📄 원문 표현 (paper)
> - "Across 13 workloads, PACT achieves up to 61% performance improvement over the best of 7 state-of-the-art tiering designs with up to 50× fewer migrations." (p.1366, Abstract)
> - "PACT achieves strong performance improvements, by up to 61% over the second-best system... with an average gap of 4.1%, and a maximum gap of 11.8%." (p.1367)
> - "PACT... performs only 180K [migrations], an order of magnitude fewer, yet achieves lower slowdown (18% vs. 25%)." (p.1377)

## 섹션 노트
- **§1 Introduction**: hotness-based tiering의 근본적 한계(criticality ≠ frequency)를 지적하고 PAC/PACT를 소개, 96 workload 연구 기반 두 핵심 통찰(stall = f(LLC-miss, MLP), phase stability) 제시.
- **§2 Background**: SoarAlto(AOL, Amortized Offcore Latency)가 가장 가까운 선행연구지만 object-granularity·offline profiling이라는 한계를 지적하며 PAC과의 mechanism/scope 차이를 명확화. TMO 등 stall-pressure 신호 활용 선행연구와도 구분.
- **§3 Motivation**: Masim/GUPS/tc-twitter 세 워크로드로 frequency-criticality disconnect를 정량 실증(최대 65× 차이), Takeaway #1.
- **§4 PACT Design**: PAC 추정(4.2, per-tier stall/MLP 모델링), PAC 샘플링(4.3, Algorithm 1), migration 정책(4.4 eager demotion, 4.5 adaptive promotion/binning), 구현(4.6)을 순서대로 기술.
- **§5 Evaluation**: bc-kron 심층 분석(5.2), 12개 워크로드 종합(5.3), Soar/Alto 비교(5.4), adaptivity(5.5), PAC vs frequency(5.6), sensitivity(5.7), bandwidth contention(5.8), colocation(5.9), breakdown(Redis, 5.10) 순.
- **§6 Conclusion**: page-level online performance criticality를 tiered memory 관리의 first-class design principle로 제안했다는 기여를 재확인, 향후 CHMU(CXL 3.2) 등 hardware 발전과의 결합을 future work로 언급.

## 핵심 용어 (Key terms)
- **Per-page Access Criticality (PAC)**: 각 페이지 접근이 CPU stall에 기여하는 정도를 정량화한 online, page-granular 지표. access frequency가 아닌 실제 성능 영향을 측정.
- **Memory-Level Parallelism (MLP)**: out-of-order CPU가 동시에 발행하는 outstanding memory request 수; 높을수록 latency가 amortize되어 stall cost가 낮아짐.
- **CHA (Caching and Home Agent) / TOR (Table Of Requests)**: Intel CPU에서 core와 offcore(DRAM/CXL) 사이 memory traffic을 조율하는 하드웨어 유닛/큐; TOR occupancy로 per-tier MLP를 관측.
- **PEBS (Precise Event-Based Sampling)**: Intel의 low-overhead hardware sampling 메커니즘, LLC miss 이벤트 기반 페이지 접근 샘플링에 사용.
- **Eager Demotion**: memory pressure를 기다리지 않고 fast tier에서 선제적으로 페이지를 demote해 promotion용 공간을 미리 확보하는 정책.
- **Adaptive Promotion / Adaptive Binning**: PAC 분포의 skew·변동에 맞춰 promotion bin 경계를 동적으로 재조정하는 정책.
- **Freedman-Diaconis rule**: 사분위수 범위(IQR) 기반으로 histogram bin width를 통계적으로 최적화하는 공식, $W = 2(Q_3-Q_1)/\sqrt[3]{n}$.
- **Reservoir Sampling**: 전체 데이터 크기를 몰라도 스트림에서 고정 크기 k개의 균일 무작위 표본을 유지하는 온라인 샘플링 기법.
- **Cooling (temporal decay)**: PAC 값에 EWMA류 decay factor $\alpha$를 적용해 recency를 반영하는 optional 메커니즘 (PACT 기본값은 $\alpha=1.0$, 즉 no cooling).
- **CXL Hotness Monitoring Unit (CHMU)**: CXL 3.2에서 제공하는 device-side access sampling 기능, 향후 PAC 추정 정확도 개선의 잠재적 경로로 언급됨.

## 강점 · 한계 · 열린 질문
- **강점**: (1) 단 4개의 표준 CPU perf counter만으로 per-tier, per-page 수준의 online criticality 추정을 실현 — 하드웨어 수정 불필요. (2) 96개 워크로드 기반 실증적 모델링(Pearson r>0.98)으로 이론적 근거가 탄탄. (3) migration overhead를 최대 50배까지 줄이면서도 성능은 향상 — 실무 배포 관점에서 매력적. (4) sensitivity analysis(§5.7)가 광범위해 파라미터 튜닝 부담이 적음을 뒷받침(Takeaway #7).
- **한계**: (1) proportional attribution은 동일 sampling window(20ms) 내 access pattern이 latency-bound든 MLP-bound든 균질하다고 가정하는데, multi-tenant colocation에서 이질적 access pattern이 섞이면 criticality가 dilute될 수 있음(§4.3.7에서 저자도 명시적으로 인정, "observability gap rather than a flaw"). (2) 일부 워크로드(657.xz, tc-twitter, 603.bwaves, bc-urand, sssp-kron)에서는 Colloid/NBT/Soar가 더 우수 — 특정 access pattern(짧은 recency-sensitive 패턴, 매우 큰 단일 object)에서는 한계 노출. (3) AMD 플랫폼에서는 TOR-like queue 부재로 MLP 추정이 근사적(Little's Law 기반)이라 정확도가 Intel보다 떨어질 가능성.
- **열린 질문**: multi-tenant colocation 하에서 fine-grained stall observability를 어떻게 확보할 것인가(CHMU 등 device-side sampling과의 결합)? latency-weighted attribution($S_p = S \times A_p l_p / \sum A_i l_i$)으로 확장 시 실제 이득은 얼마나 될까? cooling 메커니즘의 체계적 최적화(§4.3.4에서 future work로 남김)는 어떤 형태가 되어야 하는가?

## ❓ Q&A (자가 점검)
> [!question]- PAC가 hotness(access frequency)와 근본적으로 다른 점은?
> hotness는 "얼마나 자주 접근되는가"만 측정하지만 PAC는 "그 접근이 실제로 CPU stall에 얼마나 기여하는가"를 측정한다. MLP가 높으면(예: 순차 array traversal) 접근이 잦아도 stall 기여가 작고, MLP가 낮으면(예: pointer-chasing) 접근이 적어도 stall 기여가 크다. Masim 실험에서 동일 frequency의 페이지들 사이에 최대 65배 PAC 차이가 관측됐다(p.1368).

> [!question]- PAC를 온라인으로 어떻게 4개 CPU counter만으로 계산하는가?
> (1) PEBS `MEM_LOAD_L3_MISS_RETIRED`로 slow-tier LLC-miss 이벤트를 샘플링, (2) CHA의 `TOR_OCCUPANCY`($T_1$)와 `TOR_OCCUPANCY_COUNTER0`($T_2$)로 per-tier MLP=$T_1/T_2$를 계산, (3) Equation 1 $\text{LLC-stalls}=k\cdot\text{LLC-misses}/\text{MLP}$로 total stall 추정, (4) 이를 PEBS로 샘플링된 페이지 접근 빈도에 비례해 배분한다(Algorithm 1, p.1372).

> [!question]- 왜 proportional attribution(접근 빈도 비례 배분)이 유효한가?
> MLP가 tens-of-milliseconds 단위의 짧은 sampling window(20ms) 내에서는 거의 일정한 "phase-level stability"를 보이기 때문에, 그 window 안에서는 개별 접근의 stall cost가 거의 동일(unit stall cost)하다고 가정할 수 있고, 따라서 접근 빈도 비례 배분이 합리적 1차 근사가 된다(Takeaway #4, p.1370).

> [!question]- eager demotion과 adaptive promotion은 각각 어떤 문제를 푸는가?
> eager demotion은 memory pressure를 기다리지 않고 선제적으로 fast tier 공간을 확보해 promotion이 지연되지 않도록 하는 것(공간 부족 문제). adaptive promotion은 PAC 분포가 극도로 skewed하고 workload마다/시간마다 변하는 상황에서 고정 threshold binning이 실패하는 문제를 Freedman-Diaconis rule + reservoir sampling + dynamic scaling으로 해결한다(§4.4-4.5).

> [!question]- PACT가 Soar에게 지는 사례들의 공통점은?
> Soar는 offline object-level profiling을 사용하는데, bc-kron처럼 매우 큰(~16GB) 단일 object가 fully DRAM에 못 들어가는 경우나, 603.bwaves/bc-urand/sssp-kron처럼 workload-specific access pattern이 offline insight로 더 잘 포착되는 경우 Soar가 우세하다. 이는 online adaptability와 offline global insight 간의 trade-off를 보여준다(§5.4, p.1377).

> [!question]- multi-tenant colocation에서 PAC의 한계는 무엇이며 저자는 이를 어떻게 방어하는가?
> 서로 다른 access pattern(latency-bound vs latency-tolerant)이 같은 tier에서 섞이면 proportional attribution이 진짜 critical한 페이지의 criticality를 희석시킬 수 있다. 저자는 이를 "criticality-first 원칙 자체의 결함이 아니라 observability gap"이라 주장하며, colocation microbenchmark(Figure 12)에서 여전히 기존 정책보다 우수함을 보여 방어한다(§4.3.7, §5.9).

> [!question]- cooling(temporal decay)이 기본적으로 비활성화(α=1.0)인 이유는?
> PAC 값이 이미 newly critical 페이지를 자연스럽게 높은 priority bin으로 빠르게 반영하므로 별도의 explicit decay 없이도 반응성이 충분하다고 판단했기 때문이다. sensitivity 실험(Figure 10c)에서도 cooling 적용이 대부분의 경우 성능을 저하시키거나 이득이 없었다(§5.7).

> [!question]- PACT는 어떻게 THP(Transparent Huge Page)와 4KB 페이지를 동시에 다루는가?
> PEBS 기반 criticality 감지는 항상 4KB granularity로 이루어지지만, 선택된 4KB 페이지가 2MB huge page에 속하면 `move_pages()`로 huge page 전체를 migration해 fine-grained criticality detection과 coarse-grained cost-efficient migration을 결합한다(p.1376).

## 🔗 Connections
[[CXL]] · [[ASPLOS]] · [[2026]]
관련: TPP, Colloid, Memtis, NBT 등 hotness 기반 tiering 계열과 직접 비교되며, CXL latency 특성(190ns 에뮬레이션)을 활용한다는 점에서 이 리스트의 다른 CXL 메모리/tiering 계열 논문들과 연결됨.

## References worth following
- **SoarAlto** (Liu et al., "Tiered Memory Management Beyond Hotness", OSDI 2025) — PAC와 가장 밀접한 선행연구(AOL, Amortized Offcore Latency); object-level offline profiling과의 차이가 PACT의 핵심 motivation.
- **TPP** (Maruf et al., "TPP: Transparent Page Placement for CXL-Enabled Tiered Memory", ASPLOS 2023) — PACT가 구현에서 일부 reliability fix를 차용한 실전 tiering 시스템, 주요 baseline.
- **Memtis** (Lee, Koma, Eom, "Efficient Memory Tiering with Dynamic Page Classification and Page Size Determination", SOSP 2023) — THP-aware baseline으로 4KB/THP 양쪽에서 강력한 비교 대상.
- **NOMAD** (Xiang et al., "Non-Exclusive Memory Tiering via Transactional Page Migration", OSDI 2024) — replication 기반 tiering, PACT 평가에서 가장 큰 slowdown(최대 800%)을 보인 대조군.
- **CXL Hotness Monitoring Unit (CHMU, CXL 3.2)** — device-side access sampling으로 향후 PAC 추정 정확도를 개선할 잠재적 하드웨어 경로로 논문이 직접 언급.

## Personal annotations
<!-- 본인 메모 영역 -->
