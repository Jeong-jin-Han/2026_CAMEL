---
title: "ArtMem: Adaptive Migration in Reinforcement Learning-Enabled Tiered Memory"
aliases: [ArtMem]
description: "Q-learning RL agent가 fast-tier access ratio를 state로 삼아 migration scope를 동적 조정하는 tiered memory system, baseline 대비 평균 114% 성능 향상."
venue: ISCA
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/cxl
  - topic/tiered-memory
  - topic/rl
  - topic/migration
  - venue/isca
  - year/2025
---

# ArtMem: Adaptive Migration in Reinforcement Learning-Enabled Tiered Memory

> **ISCA 2025** · `cluster/cxl` · Source: [ArtMem - Adaptive Migration in Reinforcement Learning-Enabled Tiered Memory.pdf](<ArtMem - Adaptive Migration in Reinforcement Learning-Enabled Tiered Memory.pdf>)

**저자**: Xinyue Yi (Xiamen Univ.), Hongchao Du (City Univ. of Hong Kong) [공동 1저자], Yu Wang (Xiamen Univ.), Jie Zhang (Peking Univ.), Qiao Li (MBZUAI), Chun Jason Xue (MBZUAI)

## TL;DR
ArtMem은 DRAM과 PM/CXL로 구성된 tiered memory에서 page migration을 관리하는 **Q-learning 기반 model-free RL** 시스템이다. fast-tier access ratio를 state로, migration scope(이동 page 수 + hotness threshold)를 action으로 정의하고, access ratio 개선을 reward로 삼아 workload 변화에 적응적으로 migration 정책을 학습한다. 기존 정적/휴리스틱 tiering 대비 다양한 workload에서 평균 **114%** 성능 향상(범위 35%~172%)을 달성하며 overhead는 sampling 최대 3%, Q-table 연산 0.07%, Q-table 메모리 <10KB로 작다.

## 문제 & 동기
Tiered memory는 DRAM의 고비용·용량 한계를 PM/CXL 같은 느린(최대 2배 지연) tier로 보완하지만, 기존 시스템(AutoNUMA, AutoTiering, Nimble, HeMem, Thermostat, Kleio, Multi-clock 등)은 세 가지 한계를 가진다. (1) **workload 의존성**: 정적 hotness threshold/휴리스틱이 특정 workload에서만 잘 동작하고 다른 곳에선 suboptimal. (2) **fast-tier access ratio 미활용**: access ratio가 낮을 때(=migration이 비효과적일 때) 정책을 조정하지 못함. (3) **migration scope 고정**: workload 패턴에 따라 이동할 page 수를 동적으로 조정할 기회를 놓쳐 불필요한 migration 또는 fast tier 과소활용 발생.

ArtMem은 RL로 migration scope를 동적·workload-aware하게 조정해 이 세 한계를 해결한다.

> [!quote]- 📄 원문 표현 (paper)
> "This paper identifies three key limitations in existing tiered memory solutions. First, existing solutions often perform differently across different workloads... Second, they often fail to adjust migration strategies in response to low fast memory tier access rates... Third, they often miss the opportunity to dynamically tune the memory migration scope based on workload patterns..." (p.1, Abstract)
>
> "Compared to state-of-the-art solutions, ArtMem improves performance by 114% on average in all scenarios." (p.2)
>
> "Observation 1: Existing methods may perform well on certain workloads but poorly on others." (p.3) / "Observation 2: A low access ratio in the fast memory tier indicates that the current page migration mechanism is ineffective under such conditions." (p.4) / "Observation 3: The memory migration scope should be tuned dynamically for different workload patterns to fully exploit the tiered memory's potential." (p.4)

## 핵심 통찰

> [!note]- 통찰 1 — Fast-tier access ratio가 migration 효과의 실시간 지표 (Observation 2)
> perf로 측정한 DRAM access ratio와 정규화 성능 사이에 강한 상관(Pearson 0.89/0.81/0.87 for MEMTIS/AutoTiering/Nimble, Fig.3, p.4)이 존재한다. access ratio가 낮으면 현재 migration 메커니즘이 비효과적이라는 신호이므로, 이를 RL의 state·reward로 쓰면 휴리스틱과 달리 실시간 피드백 기반 결정을 내릴 수 있다.

> [!note]- 통찰 2 — Migration scope를 동적으로 조정해야 함 (Observation 3)
> 정적 hotness threshold는 같은 workload에서도 과대/과소 migration을 유발한다. 예: Pattern S1에서 MEMTIS는 DRAM 용량이 커서 모든 page를 hot으로 분류해 15GB를 옮기지만 실제로는 1GB만 필요. Liblinear/XSBench에서 threshold를 수동 튜닝하면 각각 47%/42% 성능 향상(Fig.4, p.4). 따라서 scope(page 수 + threshold)를 학습으로 정해야 한다.

> [!note]- 통찰 3 — Per-page가 아닌 system-level state/action으로 RL을 실용화
> 수백만 page에 대해 page 단위 RL은 Q-table 폭발로 비현실적. ArtMem은 access ratio를 k+1개 discrete state로, scope를 9개 migration size + ±8/±4/0 threshold action으로 압축(총 12 state, k=10)해 학습 속도와 표현력의 균형을 맞춘다(p.5, p.7).

> [!note]- 통찰 4 — RL을 application critical path에서 분리
> sampling/migration을 background kernel thread(코어당 ksampled, 시스템 kmigrated)로 비동기 처리해 application 성능에 영향 없이 RL 연산·migration을 수행(p.2, p.6-7).

## 설계 / 메커니즘

> [!abstract]- ArtMem 3계층 아키텍처 (Fig.5, p.5)
> - **상단 RL Framework**: Q-table, Adaptive&Efficient Action, Performance-driven Reward, Low-overhead State, 환경 피드백 루프.
> - **중단 ArtMem 함수**: ① Accesses distribution(EMA bin) ② Thresholds adaptation ③ Page sorting(LRU list) ④ Page migration in background, Migration volume adaptation, event queue.
> - **하단 Hardware**: Fast Tier Memory, Capacity Tier Memory, PMU(PEBS sampling).

> [!abstract]- State / Action / Reward 설계 (p.5-6)
> - **State**: fast memory access ratio τ = ⌊DRAM_access × k / (DRAM_access + PM_access)⌋, k+1개로 discretize. DRAM_access=PM_access=0(샘플 없음)인 경우는 access ratio 0과 구분하기 위해 별도 state (k+1) 부여.
> - **Action**: migration scope = migration number(이동 page 수) + hotness threshold. 각각 별도 Q-table·독립 action set으로 협력적 조정.
> - **Reward (Eq.2)**: τ_i − β + λ(τ_i − τ_{i−1}). β = 목표 access ratio. 직전 period에 migration이 없었으면 access ratio 변화에 보상/벌점을 주지 않도록 λ=0(있으면 λ=1). target deviation 항(−β)으로 과도한 access ratio 진동을 막아 system 안정화.

> [!abstract]- Dynamic Migration Scope: EMA + Page Sorting + RL (Fig.6, p.6)
> - **EMA**: 각 page의 access count를 base-2 exponential bin에 그룹화해 access 분포를 경량 추적. 2백만 sample마다 cooling(모든 bin/count를 절반으로 감쇠)해 stale 정보 할인.
> - **Hotness threshold 동적 조정**: 초기엔 DRAM 용량 기반으로 설정 후 cooling마다 reset, cooling 사이엔 RL이 학습한 정책으로 refine.
> - **Page sorting**: recency 활용. fast tier의 inactive list에서 demotion 후보를, capacity tier의 active list에서 promotion 후보를 우선 선택. 보수적인 기존 방식과 달리 migrated page를 항상 active list head에 삽입하는 aggressive 정책으로 최근 접근 page를 즉시 우선시.
> - **Migration number**: RL이 real-time 피드백으로 이동 page 수를 자동 학습(threshold 단독으로는 S1처럼 불필요 migration 발생).

> [!abstract]- Workflow & Q-learning 업데이트 (Algorithm 1, p.6-7)
> 1. Q(k,0)=1, 나머지 0으로 초기화(프로그램 로드 시 DRAM 100% → migration 불필요). 2. ε-greedy로 action a 선택. 3. migration thread가 갱신된 파라미터로 migration. 4. sampling으로 새 state τ_i 관측. 5. r = τ_i − β + λ(τ_i − τ_{i−1}). 6. Q(τ_{i−1},a) ← Q + α[r + γ·max_{a'}Q(τ_i,a') − Q(τ_{i−1},a)]. 갱신은 Q-learning 표준 TD.

> [!abstract]- 구현 세부 (§5, p.7-8)
> - Linux kernel **v5.15.19** prototype, open-source (github.com/Yitrus/ArtMem).
> - **2MB huge page** 단위 migration(주소 변환 overhead 감소), access data는 compound_page의 struct page 재사용으로 추가 메모리 없이 저장.
> - background **kmigrated**(migration) + 코어당 **ksampled**(sampling, PEBS+memory load 수집), atomic operation으로 일관성 보장.
> - memory cgroup 하 mount point 2개(memory.action_show / memory.threshold_show)로 user space RL과 통신, memory.hit_ratio_show로 sampling 정보 조회.
> - **heuristic 최소 hotness threshold = 16 accesses/page**(RL이 너무 낮게 잡아 thrashing 유발 방지). threshold action ±8/±4/0, migration size 최소 16MB(8×2MB)부터 2배씩 최대 2048MB, 0MB(no migration) 포함 9개. k=10 → 12 state.

## 평가

> [!success]- 실험 환경 & workload (Table 2/3, p.8)
> - Intel Xeon Gold 6330(28 core/socket), socket당 4×DDR4(16GB)+4×Optane PM(128GB). DRAM 64GB(fast) + PM 512GB(slow). PM은 ndctl로 remote NUMA node 구성. latency: fast 92ns/81GB/s, slow 323ns/26GB/s.
> - workload 8종: YCSB(32G), CC(69G), SSSP(64G), PR(25G), XSBench(69G), DLRM(72G), Btree(24G), Liblinear(68G).
> - baseline 7종: AutoNUMA, Nimble, Multi-Clock, TPP, Tiering-0.8, AutoTiering, MEMTIS. memory ratio(fast:slow) 2:1/1:1/1:2/1:8/1:16. AutoNUMA(1:16)이 baseline.

> [!success]- 전체 성능 (Fig.7, p.8-9)
> - **DRAM:PM ratio별 평균 향상**: AutoNUMA 132% / Nimble 124% / Multi-Clock 104% / TPP 91% / Tiering-0.8 72% / AutoTiering 67% (7 baseline 대비). best baseline 대비도 평균 10.4%~43.65% 향상.
> - **전체 평균 114% 향상**(범위 35%~172%, p.1/p.2).
> - In-memory DB(YCSB A-F): 부적격 page promotion을 selective하게 줄여 다른 방법 대비 평균 65% migration 감소.
> - Graph(SSSP/PR/CC): 12%~509% 향상(data locality 학습 효과).
> - HPC(XSBench): hot region을 즉시 fast tier에 배치.
> - DLRM: 5%~110% 향상(dense feature 순차 접근 학습).
> - Btree: multiclock 제외 4%~36% 향상.
> - Liblinear: MEMTIS보다 9% 낮지만 다른 대비 평균 76% 향상. sampling frequency를 올리면(+5.91% overhead) 추가 17.11% 향상.

> [!success]- Ablation & 분석 (Fig.8-12, p.9-10)
> - **Ablation(Fig.8)**: EMA < EMA+PageSort < EMA+PageSort+RL. RL이 가장 큰 기여, DRAM 비율 낮을수록 RL 이득 커짐. PageSort는 PR/XSBench 등 특정 workload에서 >10% 기여.
> - **Migration volume(Fig.11)**: MEMTIS는 hotness threshold 의존으로 큰 변동 + ArtMem 대비 약 **10배** CPU overhead. ArtMem은 DLRM에서 MEMTIS 대비 **30,000배 적은** page migration.
> - **Number of migration**: ArtMem/AutoNUMA가 모든 시나리오에서 낮은 migration 유지(reward로 불필요 migration 억제 + access-based sorting).

> [!success]- Customizability / Sensitivity / Overhead (Fig.13-17, p.10-12)
> - **Latency-as-reward**: DRAM access ratio reward 대비 평균 3.4% 낮음(데이터 수집 overhead + 지연된 조정).
> - **Q-learning vs SARSA(Fig.13)**: 두 알고리즘 모두 유사한 향상(framework 적응성).
> - **Robustness(Fig.14)**: 25개(5×5) train/test 조합 중 7개만 10% 초과 성능 저하. 잘못 초기화해도 95% 성능 도달까지 평균 3 iteration(1~6).
> - **Hyperparameter(Fig.15)**: α=e^{−2}, γ=e^{−1}, ε=0.3, β 8~10, migration interval 10s(5~15s 최적).
> - **Memory scalability(Fig.16a)**: CC를 69GB→290GB로 키워도 ≥6% 향상 유지.
> - **Latency sensitivity(Fig.16b)**: latency gap 커질수록 baseline과 성능 격차 확대(강한 적응성).
> - **Mixed workload(Fig.16c)**: SSSP+XSBench+Btree 동시 실행 시 2nd-best 대비 11% 향상.
> - **Overhead**: sampling 최대 3% CPU, Q-table 연산 최대 0.07% CPU, 두 Q-table 합 메모리 <10KB.

## 섹션 노트
- **§1 Intro**: DRAM이 서버 비용의 ~40% 차지(p.1), PM/CXL tiering 필요성. 기존 시스템들의 정적 threshold 한계 지적.
- **§2 Background**: access monitoring(page table scan, page fault, hardware sampling/PEBS), migration metric(frequency vs recency, LRU/Multi-clock의 3rd LRU list), RL for system(Q-learning/Sarsa, MDP 5요소).
- **§3 Motivation**: MASIM simulator로 4개 합성 access pattern(S1-S4, 32GB total/16GB DRAM, Fig.1) 구성 → Observation 1-3 도출. Table 1에 7개 기존 설계의 장단점 패턴 정리.
- **§4 Design**: 3계층 아키텍처, RL framework(state/action/reward), dynamic migration scope(EMA+sorting+RL), RL 통합(background thread), workflow.
- **§5 Implementation**: kernel v5.15.19, 2MB huge page, ksampled/kmigrated, cgroup mount point, heuristic min threshold.
- **§6 Evaluation**: setup, 다양한 상황 성능, ArtMem 이해(ablation/migration/customizability/robustness/sensitivity/scalability), overhead.
- **§7 Conclusion**: RL 기반 적응형 tiered memory, 평균 114% 향상, minimal overhead.

## 핵심 용어
- **Tiered memory**: 빠른 tier(DRAM)와 느린 tier(PM/CXL-attached memory) 간 page를 동적 migration해 비용·용량과 성능을 절충하는 메모리 구조.
- **Migration scope**: migration 대상이 되는 page 집합. migration number(이동 page 수)와 hotness threshold로 결정.
- **Fast memory access ratio (τ)**: 전체 memory access 중 fast tier(DRAM)에 적중한 비율. ArtMem의 RL state·reward 핵심 지표.
- **EMA (Exponential Moving Average)**: page access count를 base-2 exponential bin에 누적·감쇠시켜 access 분포를 경량 추적하는 기법.
- **Page sorting**: active/inactive LRU list 기반으로 recency를 반영해 promotion(capacity active)/demotion(fast inactive) 후보를 선택.
- **PEBS (Precise Event-Based Sampling)**: PMU 확장. 트리거 이벤트의 instruction address·register state 등 contextual 정보를 buffer에 기록하는 hardware sampling.
- **Q-learning**: model-free off-policy RL. Q(s,a)를 TD로 갱신해 최적 action-value를 학습. ArtMem은 ε-greedy로 action 선택.
- **Cooling**: 2백만 sample마다 모든 bin/count를 절반으로 감쇠해 stale access 정보를 할인하는 연산.
- **Hotness threshold**: page를 hot/cold로 분류하는 access count 기준. ArtMem은 최소 16으로 두고 RL이 ±8/±4/0으로 조정.

## 강점 · 한계 · 열린 질문
**강점**
- access ratio라는 측정 가능한 단일 지표를 state·reward로 삼아 RL을 실용적 차원(system-level state/action)으로 축소, Q-table <10KB·overhead <3%로 매우 가벼움.
- 다양한 workload·memory ratio·latency에서 일관된 향상(평균 114%), best baseline 대비도 우위.
- background thread로 critical path 분리, kernel v5.15.19 open-source 구현으로 재현 가능.
- Q-learning/SARSA, DRAM ratio/latency reward 모두 동작하는 framework 일반성.

**한계**
- per-page가 아닌 system-level 결정이라 region 내 세밀한 hot/cold 구분은 page sorting에 의존.
- Liblinear처럼 초기 access가 균일한 workload에서 access ratio가 0%까지 급락 후 회복에 시간 소요(p.9), 정확한 sampling에 ramp-up 속도가 좌우됨.
- 잘못된 Q-table 초기화 시 재학습(평균 3 iteration) 비용 존재, mismatched init에서 일부 workload 10% 이상 저하.
- Optane PM 기반 실험으로 CXL 실측은 없음(latency emulation으로 일반화 주장).

**열린 질문**
- CXL Type-3/멀티 tier(3개 이상)로 확장 시 state·action space는 어떻게 커지나?
- Liblinear 같은 균일 초기 패턴에서 ramp-up을 가속할 prefetch/predictor 결합 가능성은?
- multi-tenant 환경에서 cgroup별 독립 Q-table의 간섭/공정성은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. ArtMem이 RL의 state로 무엇을 쓰며 왜 그 선택이 핵심인가?
> > fast memory access ratio τ = ⌊DRAM_access × k / (DRAM_access+PM_access)⌋를 state로 사용한다(k+1 + 별도 1 state). Observation 2(access ratio와 성능의 Pearson 0.81~0.89 상관)에 근거해, 측정 가능하고 migration 효과를 실시간 반영하는 지표이기 때문이다. 샘플이 없는 경우(=0)는 access ratio 0과 구분하려고 별도 state로 둔다.

> [!question]- Q2. Reward 함수(Eq.2)의 각 항은 무슨 역할인가?
> > r = τ_i − β + λ(τ_i − τ_{i−1}). τ_i는 현재 access ratio(높을수록 좋음), −β는 목표 access ratio 대비 deviation을 penalize해 agent가 access ratio를 인위적으로 진동시켜 reward를 착취하는 것을 막아 안정화. λ(τ_i−τ_{i−1})는 직전 period 대비 개선/악화에 보상/벌점하되, 직전에 migration이 없었으면 λ=0으로 두어 workload 자체 변화에 잘못 책임을 묻지 않는다.

> [!question]- Q3. Action(migration scope)은 어떻게 구성되는가?
> > migration number와 hotness threshold 두 파라미터를 별도 Q-table·독립 action set으로 협력 조정한다. migration size는 0MB(no migration)와 16MB~2048MB(2배씩) 9개, threshold는 현재값에 ±8/±4/0(최소 16 accesses 유지). per-page가 아닌 system-level scope로 정의해 Q-table 폭발을 피한다.

> [!question]- Q4. EMA와 page sorting은 각각 어떤 정보를 담당하나?
> > EMA는 frequency(access 분포)를 base-2 exponential bin으로 추적하고 2백만 sample마다 cooling으로 감쇠한다. page sorting은 recency를 담당해 active/inactive LRU list로 demotion(fast inactive)/promotion(capacity active) 후보를 고르며, migrated page를 active list head에 즉시 삽입하는 aggressive 정책으로 최근 hot page를 빠르게 우선시한다. 둘이 합쳐져 frequency+recency 기반 경량 sorting을 구성한다.

> [!question]- Q5. RL이 application 성능에 영향을 주지 않는 이유는?
> > sampling(ksampled, 코어당)과 migration(kmigrated, 시스템)을 background kernel thread로 비동기 수행해 application critical path(memory access)와 분리한다. PEBS buffer를 ring buffer로 순차 처리하고 atomic operation으로 일관성을 유지한다. 그 결과 sampling overhead ≤3% CPU, Q-table 연산 ≤0.07% CPU에 그친다.

> [!question]- Q6. 평가에서 보고된 대표 성능 수치는?
> > 7개 baseline 대비 평균 향상은 AutoNUMA 132%/Nimble 124%/Multi-Clock 104%/TPP 91%/Tiering-0.8 72%/AutoTiering 67%, 전체 평균 114%(범위 35~172%), best baseline 대비도 평균 10.4~43.65%. migration 측면에선 MEMTIS 대비 CPU overhead 약 10배 적고 DLRM에서 page migration 30,000배 적음.

> [!question]- Q7. 정적 hotness threshold의 문제를 보여준 motivation 사례는?
> > Pattern S1에서 MEMTIS는 DRAM 용량이 충분해 모든 page를 hot으로 분류, 15GB를 옮기지만 실제 필요한 건 1GB뿐이다(p.4). Liblinear/XSBench에서 threshold를 수동 튜닝하면 각각 47%/42% 향상(Fig.4)되어, scope를 동적으로 정해야 한다는 Observation 3을 뒷받침한다.

> [!question]- Q8. heuristic 최소 hotness threshold(16)를 둔 이유는?
> > exploration 단계에서 RL이 threshold를 너무 낮게 설정하면 거의 모든 page가 hot으로 분류되어 capacity tier로 곧바로 demotion되는 thrashing이 발생한다. 이를 막기 위해 page당 최소 16 accesses를 hot 기준 하한으로 경험적으로 고정하고, RL은 그 위에서 ±8/±4/0으로 조정한다.

## 🔗 Connections
[[CXL]] · [[ISCA]] · [[2025]]

## References worth following
- MEMTIS (SOSP 2023, [30]): EMA 기반 hotness + dynamic page classification. ArtMem의 주 비교 대상이자 EMA 기법 출발점.
- TPP (ASPLOS 2023, [36]): CXL tiered memory용 transparent page placement, lightweight demotion. baseline.
- Kleio (HPDC 2019, [18]): ML/RL 기반 hybrid memory page scheduler — RL+tiering 선행 연구.
- Dong Xu et al. (USENIX ATC 2024, [64]): Adaptive Page Profiling and Migration for Tiered Memory — NUMA fault 기반 새 hot page 탐지, page sorting 근거.
- Multi-Clock (HPCA 2024, [35]): dynamic page placement, candidate LRU list 3단 구조. baseline.
- HeMem (SOSP 2021, [50]): hardware sampling 기반 big-data tiered memory 관리.

## Personal annotations
<본인 메모 영역>
