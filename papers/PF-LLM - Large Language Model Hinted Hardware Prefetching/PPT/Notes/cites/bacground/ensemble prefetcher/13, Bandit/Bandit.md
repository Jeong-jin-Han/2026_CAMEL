# Bandit (Micro-Armed Bandit: Lightweight & Reusable Reinforcement Learning for Microarchitecture Decision-Making)

> Gerasimos Gerogiannis, Josep Torrellas (UIUC). MICRO '23.

## 분류
- **HW/SW**: **HW** — MAB agent(*Bandit*)를 하드웨어로 구현. nTable·rTable 두 테이블 + arithmetic unit + control logic으로 구성, 총 storage overhead 단 **100 bytes**. HW performance counter를 reward로 읽음.
- **offline/online**: **online** — offline pre-training 없이 런타임에 environment와 상호작용하며 학습(online RL). 미지의 workload/configuration에 대한 adaptability·generalization이 목적.
- **sub-prefetcher 개수 / 구성**: prefetching use case에서 Bandit은 prefetch degree/offset을 직접 정하지 않고, 관습적 non-RL lightweight prefetcher들의 **coordinator(orchestrator)**로 작동. 대상은 **next-line, stride, stream** prefetcher (Section 5, [38]과 유사한 방식). SMT fetch use case에서는 arm이 sub-prefetcher가 아니라 **64개의 fetch Priority & Gating(PG) policy** (4 fetch-priority × 2^4 gating = 64).
- **orchestration 방식**: **Multi-Armed Bandit (MAB)** — RL 중 가장 단순한 형태. 구체 알고리즘은 **DUCB (Discounted Upper Confidence Bound)**를 HW로 구현(non-stationary/highly-varying 환경에 적합, γ forgetting factor로 과거 관측을 잊음). ε-Greedy, UCB, DUCB 세 변형을 비교하고 DUCB를 채택.

## 방법론
- **핵심 아이디어**: 환경을 **단일 state로 collapse**하여 각 action(=arm)당 값 하나만 추적 → 기존 MDP-RL/Contextual Bandit 대비 복잡도·storage 대폭 감소. 각 arm이 하나의 선택지(예: 어떤 prefetcher를 켤지, 또는 어떤 fetch PG policy를 쓸지)이고, **reward = IPC**를 최대화하도록 arm을 고름.
- **online 학습 방식**: bandit step마다 nextArm()으로 arm 선택 → 해당 microarchitecture unit에 전달 → step이 끝나면 HW counter에서 committed instruction 수로 IPC(=reward, r_step) 계산 → rTable(평균 reward r_i)·nTable(선택 횟수 n_i)을 updRew/updSels로 갱신. 이를 연속 반복. 초기엔 round-robin phase로 모든 arm을 1회씩 시도.
- **핵심 전제 — temporal homogeneity**: 저자들은 "**temporal homogeneity in the action space**"(같은 action이 충분히 긴 구간 동안 optimal하게 유지되는 성질)를 발견. 이 성질이 있으면 문제를 단일 state MAB로 근사 가능. Pythia의 top-2 action이 평균 60%/15% 선택되는 것으로 근거 제시.
- **PC-centric인가?**: **아니다** — MAB는 environment state를 구분하지 못하므로(예: cache line address 간, PC 간 구분 불가) PC별로 degree/offset을 직접 고르는 것은 부적절하다고 명시. 그래서 PC-based stride 등 관습 prefetcher들이 이미 PC별 구분을 담당하게 하고, Bandit은 그 위에서 orchestrator로만 동작(추론이 아닌 논문 명시 내용).

## 핵심 요약
- Microarchitecture 의사결정(data prefetching·SMT instruction fetch)을 **단일-state Multi-Armed Bandit(DUCB)**으로 근사하여, IPC를 reward로 online 학습하는 초경량(**100 B**)·재사용 가능 HW RL agent.
- Prefetching에선 RL이 prefetcher를 직접 만들지 않고 **next-line/stride/stream prefetcher를 조율하는 coordinator**로 쓰이며, Bingo·MLOP 대비 +2.6%/+2.3%, SOTA RL prefetcher Pythia와 유사 성능을 훨씬 적은 storage로 달성.
