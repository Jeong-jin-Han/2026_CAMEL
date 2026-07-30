---
title: "PF-LLM: Large Language Model Hinted Hardware Prefetching"
aliases: [PF-LLM]
description: "Load 명령어 주변 assembly 코드 문맥을 fine-tuned LLM(PF-LLM)이 오프라인 분석해 prefetcher selection·degree·demand filtering 힌트를 생성하고, 경량 하드웨어 앙상블 LMHint Prefetcher가 런타임에 이를 소비해 oracle급 하드웨어 프리페칭을 구현하는 프레임워크"
venue: ASPLOS
year: 2026
award: "Best Paper"
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - award/best-paper
  - cluster/llm
  - venue/asplos
  - year/2026
  - list/26s-v2
  - topic/hardware-prefetching
  - topic/llm-for-systems
  - topic/microarchitecture
  - topic/cpu-cache
---

# PF-LLM: Large Language Model Hinted Hardware Prefetching

> **ASPLOS 2026** · cluster/llm · Source: [PF-LLM - Large Language Model Hinted Hardware Prefetching.pdf](<PF-LLM - Large Language Model Hinted Hardware Prefetching.pdf>)

저자: Ceyu Xu*† (HKUST), Xiangfeng Sun* (HKUST), Weihang Li (Duke University), Chen Bai (HKUST), Bangyan Wang (HKUST), Mengming Li† (HKUST), Zhiyao Xie (HKUST), Yuan Xie (HKUST) — *공동 1저자, †교신저자

## TL;DR
하드웨어 prefetcher는 런타임 정보에만 의존해 "언제·어떻게·얼마나 공격적으로 프리페치할지"를 결정하는 복잡도-정확도 trade-off에 갇혀 있다. PF-LLM은 load 명령어 주변 257줄 assembly 문맥을 코드 특화 LLM(Qwen2.5-Coder-0.5B-Instruct 파인튜닝)에 입력해 offline으로 prefetcher selection·degree·demand filtering 힌트를 생성하고, 이를 경량 하드웨어 앙상블 LMHint Prefetcher가 Prefetch Hint Table/Buffer를 통해 런타임에 그대로 따르게 한다. 이렇게 하면 온라인 학습의 trial-and-error 수렴 비용 없이 "PC별 oracle급" 프리페칭 정책을 즉시 적용할 수 있다. PF-LLM은 held-out 테스트에서 95.0% 정확도로 최적 정책을 예측하며, SPEC 2017 memory-intensive 벤치마크에서 최고 단일 prefetcher 대비 IPC 9.8%, 최고 ensemble 기법 대비 18.9% 향상을 달성한다.

## 문제 & 동기
전통적 하드웨어 prefetcher는 서브-나노초급 결정 지연과 온칩 면적·전력 제약 때문에 단순한 알고리즘만 구현할 수 있고, 넓은 프로그램 문맥을 활용하지 못한다. 여러 특화 prefetcher를 묶는 ensemble 방식도 등장했지만, 어떤 요청을 어느 sub-prefetcher로 라우팅할지 결정하는 orchestration(selection) 정책 자체가 온라인 학습에 의존해 (1) 느리고 비용이 큰 trial-and-error 수렴 기간이 필요하고 (2) 온칩 제약 때문에 단순 휴리스틱에 갇혀 더 풍부한 문맥을 활용하지 못하는 이중의 한계를 겪는다. 저자들은 Figure 1의 예시(락, strided struct 순회, spatial struct 멤버 접근, streaming string 읽기)를 통해 숙련된 개발자라면 코드만 보고도 적절한 프리페칭 전략을 식별할 수 있음을 보이고, 이를 LLM이 자동화할 수 있는지를 중심 질문으로 던진다.

> [!quote]- 📄 원문 표현 (paper)
> - "While sophisticated hardware prefetching algorithms exist, their exclusive reliance on runtime information limits their ability to adapt quickly and comprehend broader program context." (p.1)
> - "These online methods face two major challenges. On the one hand, they require a slow and costly trial-and-error convergence period to learn effective policies... On the other hand, strict on-chip area and latency constraints restrict these mechanisms to simple heuristics, preventing them from leveraging a wider program context to make more informed decisions." (p.2)
> - "This observation motivates our central question: If an experienced developer can identify these memory access patterns from code, can a modern large language model (LLM) perform the same task automatically?" (p.2)

## 핵심 통찰 (Key Insight)

> [!note]- 통찰 1: static assembly 문맥만으로도 최적 프리페칭 전략을 식별할 수 있다
> LLM은 전통적 방법보다 높은 추상화 수준에서 코드를 추론할 수 있어, 단순 배열 순회·구조체 배열 순회·연결 리스트 순회처럼 서로 다른 최적 프리페칭 전략을 요구하는 코드 패턴을 구분해낼 수 있다. 이는 하드웨어 prefetcher가 주소 스트림만 보고 고정 stride 같은 규칙성만 찾을 수 있는 것과 대비된다 (p.4).

> [!note]- 통찰 2: LLM은 여러 프로그램의 집단 경험을 일반화해 프로파일 없이도 견고하다
> Profile-Guided Optimization(PGO)은 특정 입력에 대한 프로파일링에 의존해 배포 워크로드와 다른 입력에 과적합될 위험이 있다. 반면 PF-LLM은 다양한 프로그램의 집합 코퍼스로 학습되어, 단일 프로그램의 동작을 암기하는 대신 static 코드 패턴과 dynamic 메모리 동작 사이의 관계를 프로그램 전반에 걸쳐 일반화해서 배운다. 그 결과 입력 데이터가 알려지지 않았거나 크게 달라지는 경우에도 PGO보다 견고하다 (p.4, p.13).

> [!note]- 통찰 3: "결정을 오프라인으로 옮기고 하드웨어는 그대로 따르게" 하는 인터페이스 분리
> 하드웨어는 여전히 "무엇을 할지" 결정하지 않고 "PC별로 지정된 정책을 실행"만 하면 되도록, 어려운 when/how/how-aggressively 결정을 런타임 하드웨어 밖으로 옮기고 오프라인 LLM 분석으로 대체한다. 이 인터페이스 덕분에 LMHint Prefetcher는 demand request filtering 힌트로 Pythia·Bingo 같은 정교한 sub-prefetcher의 내부 상태를 무관한 학습 신호로부터 보호하면서, 각 sub-prefetcher가 자신이 설계된 접근 스트림에만 집중하게 만들어 ensemble 성능이 개별 컴포넌트의 최댓값을 능가하는 "additive"한 효과를 낸다 (p.1, p.11).

## 설계 / 메커니즘 (Design)

> [!abstract]- PF-LLM 모델: 입력·출력·아키텍처 (Figure 3, §4.1, p.5-6)
> - **베이스 모델:** Qwen2.5-Coder-0.5B-Instruct를 그대로 파인튜닝(아키텍처 변경 없음). 프리페칭이라는 특화 태스크에는 소형 모델로도 충분하고 학습/추론 비용이 낮다.
> - **입력:** 대상 load 명령어를 `<load>...</load>` 특수 토큰으로 감싼 assembly 문맥. 앞 128줄 + load 자신 + 뒤 128줄, 총 257줄의 x86-64 assembly.
> - **출력(3종 힌트):** (1) **Prefetcher Selection Hint** — ensemble 내 사용할 sub-prefetcher를 지정하는 정수 인덱스, (2) **Prefetch Degree Hint(optional)** — 선택된 prefetcher의 공격성을 제어하는 정수(높을수록 더 공격적; sub-prefetcher가 degree 제어를 지원하지 않으면 미사용), (3) **Demand Request Filtering Hint(optional)** — 해당 load에서 나온 demand 요청을 받지 않을 sub-prefetcher를 지정, 정교한 prefetcher의 학습 노이즈를 줄임.
> - **프롬프트 템플릿(Listing 1, p.7):** Qwen 공식 system/user/assistant 형식을 따르며, assistant 출력은 `{"PFSel":..., "PF Degree":..., "Filter":...}` 형태의 JSON. loss는 오직 JSON 출력 토큰에만 계산되어(입력 assembly·system prompt 재현에는 loss 미부과) 프리페칭 힌트 생성 자체에 학습이 집중된다.

> [!abstract]- 학습 데이터셋 생성 (Table 1, §4.2, p.6-7)
> - ChampSim을 수정해 서브-prefetcher × degree의 모든 조합을 각각 별도 시뮬레이션으로 돌리고, load PC별 Average Memory Access Time(AMAT)을 기록.
> - **Ground-truth 정책:** 각 load PC마다 AMAT을 최소화하는 (prefetcher, degree) 조합을 최적 정책으로 선정.
> - **Filtering 라벨:** 각 PC에서 AMAT이 최악인 prefetcher를 filtering 대상으로 지정하되, 그 prefetcher가 Table 2에서 정의된 "advanced"(내부 상태가 정교한) 컴포넌트이면 필터링 힌트를 생략 — 내부 상태 파괴를 방지.
> - 학습에는 SPEC 2006, 평가에는 SPEC 2017만 사용해 테스트 데이터 유출을 방지 (§5.2, p.8).

> [!abstract]- LMHint Prefetcher 하드웨어 (Figure 2, §4.4, p.7-8)
> - 힌트는 오프라인에 생성되어 main memory의 **Prefetch Hint Table(PHT)**에 virtual PC로 인덱싱되어 로드된다.
> - **Prefetch Hint Buffer(PHB)**: 256-entry, TLB와 유사하게 PHT 엔트리를 캐싱하는 온칩 버퍼. 히트 시 단일 사이클 접근.
> - 힌트는 PC당 **8-bit**로 인코딩: prefetcher selection 4-bit + prefetch degree 2-bit + demand filtering 2-bit. degree는 sub-prefetcher마다 native range가 다르므로 conservative(1)/moderate(2)/aggressive(3) tri-state로 정규화되어 각 prefetcher의 Q1/median/Q3에 매핑(예: DSPatch native 0-64에서 degree=3 → round(0.75×64)=48).
> - PHB miss 시 main memory의 PHT에서 채우며, 그 사이에는 예약된 zero-address 엔트리의 default 정책이 사용된다.
> - 선택된 ensemble 후보군은 Pythia*, SMS, AMPM, Bingo*, Sandbox, Power7, DSPatch*, MLOP, Stride, Stream, Next Line, PPF(Table 2, p.8; *는 demand filtering 대상에서 제외되는 "advanced" 컴포넌트).

## 평가 (Evaluation)

> [!example]- PF-LLM 모델 정확도 (Figure 4, 5, §6.1, p.8-9)
> - held-out 테스트셋에서 최종 예측 정확도 **95.0%** (p.9; §3에서도 "95% accuracy"로 재언급, p.4).
> - 정오분류 행렬(Figure 5a)은 강한 대각선 패턴을 보이며, 오분류(off-diagonal) 대부분이 두 번째로 좋은 정책으로 향해 있어 모델이 단일 최적 정책뿐 아니라 프리페칭의 일반적 특성까지 학습했음을 시사 (p.9).
> - 최적 정책 분포(Figure 5b)는 매우 불균일해서(예: Sandbox+moderate degree가 압도적으로 자주 최적) 모델은 이 편향된 분포도 그대로 재현 (p.9).

> [!example]- LMHint Prefetcher 종단 성능 (Figure 6, 7, §6.2, p.9-10)
> - **Ablation 4종:** LMHint-S(selection hint만) → LMHint-SD(+degree, 평균 IPC +0.3%) → LMHint-SDF(+demand filtering, 추가 +0.3%) → LMHint-SDFR(Figure 5b 기준 가장 빈번히 선택된 4개 sub-prefetcher만으로 하드웨어를 축소한 reduced-cost 구성).
> - **최종 성능:** 전체 기능을 갖춘 LMHint-SDF는 no-prefetch 대비 최고 단일 prefetcher(Sandbox) 대비 지오메트릭 평균 IPC **9.8%**, 최고 ensemble 기법(Alecto) 대비 **18.9%** 개선 (p.9, 요약문에서도 동일 수치 반복 p.1).
> - reduced-cost LMHint-SDFR은 11개 sub-prefetcher 전부가 아니라 4개만으로 구성되지만 LMHint-SDF 대비 평균 0.01% 근소하게 앞서 하드웨어 복잡도·면적을 줄이면서도 성능 손실이 없음을 보임 (p.10).
> - 기존 온라인 ensemble 기법(Alecto, DOL)은 오히려 최고 단일 prefetcher보다 성능이 떨어지는 경우가 있는데, 이는 PC-중심 온라인 정책이 Pythia 같은 non-PC-localized 정교한 spatial prefetcher의 내부 패턴 탐지 상태를 훈련 노이즈로 오염시키기 때문 (p.10-11).

> [!example]- 실전 워크로드·Overhead (Figure 8-10, §6.3-6.5, p.10-12)
> - **웹 서빙 워크로드(Apache, MySQL, RocksDB, Xapian):** LMHint-SDFR이 모든 baseline보다 우수하지만 SPEC 대비 개선폭은 작음. 이는 이들 앱이 CPU/메모리보다 I/O-bound이고 이미 수년간 수동 최적화·자체 캐싱을 갖췄기 때문 (p.10-11).
> - **654.roms 트레이스 분석(Figure 9):** LMHint Prefetcher의 IPC speedup 곡선이 모든 sub-prefetcher(Bingo, MLOP, Sandbox 등)의 "upper envelope"를 실시간으로 추적, region별로 그 구간 최고 prefetcher(MLOP, Sandbox 등) 성능에 즉시 근접해 oracle급 적응을 시각적으로 입증 (p.11).
> - **오프라인 추론 비용:** vLLM 서빙 시 1×NVIDIA H20 GPU에서 최대 **234 requests/sec** 처리(짧은 문맥 길이·독립적 요청·소형 0.5B 모델 덕분), SPEC 2017 전체 힌트 생성에 8-GPU 시스템 기준 **38.5분**(16-core 컴파일 25.4분과 비슷한 수준) (p.11-12).
> - **저장 오버헤드:** PHT 엔트리는 48-bit virtual PC + 8-bit 힌트 = 56 bit(7 byte)/load. 바이너리 1MB당 평균 10.62K load 명령어 존재 → PHT 오버헤드 **74.34 KB/MB**, static 프로그램 footprint 대비 **7.26%** 증가 (p.12).

## 섹션 노트
- **§1 Introduction:** memory wall과 ensemble prefetcher의 온라인 선택 정책 한계 제시, Figure 1로 static 문맥의 잠재력 예시, 4가지 기여(PF-LLM, LMHint Prefetcher, 9.8%/18.9% 성능, LLM-guided 마이크로아키텍처 최초 시도) 요약.
- **§2 Background:** 2.1 prefetcher ensemble의 구조(sub-prefetcher + orchestration layer)와 기존 접근의 두 하위 문제(라우팅, 선택) 및 Pythia 같은 정교한 prefetcher에 대한 기존 orchestration의 취약점. 2.2 compiler-based/PGO 오프라인 기법의 한계(휴리스틱 경직성, 입력 의존성, 재컴파일 필요성).
- **§3 Why an LLM is a Good Choice:** Deep Code Comprehension, Automated Heuristic Discovery, Generalization from Collective Experience, Leveraging Pre-trained Foundation Models 네 논거로 LLM 채택을 정당화.
- **§4 PF-LLM Model and LMHint Prefetcher:** 4.1 모델 아키텍처/입출력, 4.2 프롬프트 포맷·학습, 4.3 오프라인 힌트 생성 파이프라인, 4.4 PHT/PHB 기반 LMHint 하드웨어 설계.
- **§5 Experiments:** sub-prefetcher 후보 11종(Table 2)과 degree 정규화, ChampSim 설정(Table 3, Arm Neoverse N2 유사 코어), PF-LLM 파인튜닝 하이퍼파라미터(8×H20 GPU, LLaMA-Factory+DeepSpeed).
- **§6 Evaluation:** 6.1 예측 정확도(95%), 6.2 SPEC2017 ablation·baseline 비교, 6.3 실전 웹 서빙 워크로드, 6.4 654.roms 트레이스로 성능 이득 원천 분석, 6.5 오프라인 추론/저장 오버헤드 분석.
- **§7 Discussion:** 7.1 assembly vs source code 입력 선택 근거(범용성·binary 접근성·함수 인라인 문제 회피), 7.2 SW prefetch/PGO/HW prefetching 비교(PGO의 입력 과적합, SW prefetch의 명령어 오버헤드), 7.3 ISA·머신 구성 일반화 가능성(재학습으로 대응), 7.4 두 가지 한계(JIT/바이트코드 미지원, ASLR 미고려), 7.5 branch predictor·cache replacement 등으로의 확장 가능성.
- **§8 Conclusion:** LLM이 static 문맥을 분석해 오프라인으로 하드웨어를 안내하는 co-design 패러다임을 처음 제시했다고 요약.

## 핵심 용어 (Key terms)
- **PF-LLM**: load 명령어 주변 assembly 문맥을 입력받아 prefetching 힌트 3종을 출력하도록 Qwen2.5-Coder-0.5B-Instruct를 파인튜닝한 모델.
- **LMHint Prefetcher**: PF-LLM이 생성한 힌트를 런타임에 소비해 sub-prefetcher를 orchestration하는 경량 하드웨어 prefetcher ensemble.
- **Prefetch Hint Table (PHT)**: main memory에 위치하며 virtual PC로 인덱싱된 힌트 저장 테이블.
- **Prefetch Hint Buffer (PHB)**: PHT 엔트리를 캐싱하는 256-entry 온칩 버퍼(TLB와 유사한 역할).
- **Prefetcher Selection / Degree / Demand Request Filtering Hint**: PF-LLM이 출력하는 세 종류의 힌트 — 각각 sub-prefetcher 지정, 공격성 제어, 학습 노이즈 차단을 담당.
- **AMAT (Average Memory Access Time)**: ChampSim에서 load PC별로 측정해 ground-truth 최적 정책을 정하는 데 쓰이는 지표.
- **Prefetcher Ensemble / Orchestration Layer**: 여러 sub-prefetcher를 하나의 인터페이스로 묶고 요청 라우팅·요청 선택을 담당하는 계층.
- **Compiler-based / Profile-Guided (PGO) Prefetching**: 기존 오프라인 접근으로, 각각 수작업 휴리스틱과 입력별 프로파일링에 의존하는 한계를 지님.
- **LMHint-S/SD/SDF/SDFR**: ablation에 쓰인 4가지 구성 — selection만, +degree, +filtering, reduced-cost(4개 sub-prefetcher만).
- **Alecto / DOL**: 비교 대상 기존 온라인 prefetcher ensemble orchestration 기법.

## 강점 · 한계 · 열린 질문
- **강점:** (1) 95% 정확도로 static assembly 문맥만으로 optimal prefetching policy를 예측할 수 있음을 실증. (2) 온라인 trial-and-error 없이 654.roms 트레이스에서 oracle-level upper envelope 추적을 시각적으로 입증(zero runtime adaptation latency). (3) 8-bit hint + 256-entry PHB로 하드웨어 오버헤드를 최소화하면서 7.26% footprint 증가라는 명시적 비용 분석 제공. (4) demand request filtering으로 Pythia·Bingo 등 정교한 sub-prefetcher 내부 상태를 보호해 기존 온라인 ensemble의 고질적 문제(§2.1, §6.2)를 해결. (5) SPEC 합성 벤치마크뿐 아니라 Apache/MySQL/RocksDB/Xapian 실전 워크로드까지 평가. (6) reduced-cost LMHint-SDFR로 11개→4개 sub-prefetcher만으로도 성능 손실 없이 하드웨어 비용 절감 가능성 제시.
- **한계:** (1) 평가가 단일 코어 컨텍스트로 한정되어 멀티코어 간섭·공유 캐시 환경은 미검증(§4 서두에서 저자 스스로 명시). (2) x86-64 ISA에 한정되며 다른 ISA·다른 머신 구성(캐시 크기·대역폭)에는 재학습이 필요(§7.3). (3) JIT 컴파일이나 Java 같은 바이트코드 기반 프로그램은 static 바이너리 분석에 의존하는 현재 구조로 지원 불가(§7.4). (4) ASLR을 고려하지 않아 static PC 기반 힌트와 런타임 주소가 어긋날 수 있음 — OS loader 연동으로 보완 필요(§7.4). (5) reduced-cost 4-prefetcher 선택 기준(Figure 5b 최빈값)이 벤치마크 스위트에 따라 달라질 수 있어 일반화 여부는 추가 검증 필요.
- **열린 질문:** 배포 후 온라인 incremental fine-tuning이나 힌트 갱신은 어떻게 이루어질 수 있는가? branch predictor나 cache replacement 정책처럼 §7.5에서 제안된 다른 마이크로아키텍처 메커니즘으로 확장할 때 오프라인 추론 비용과 힌트 인코딩 크기는 어떻게 변할 것인가? ASLR 대응을 위한 OS loader 통합의 실제 오버헤드는 얼마나 될 것인가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. PF-LLM이 던지는 중심 질문(central question)은 무엇인가?
> > 숙련된 개발자가 코드만 보고 메모리 접근 패턴과 적절한 프리페칭 전략을 식별할 수 있다면, 현대 LLM이 같은 분석을 자동으로 수행할 수 있는가 하는 질문이다 (p.2).

> [!question]- Q2. PF-LLM이 각 load 명령어에 대해 출력하는 세 가지 힌트는?
> > Prefetcher Selection Hint(사용할 sub-prefetcher 지정), Prefetch Degree Hint(공격성 제어, optional), Demand Request Filtering Hint(정교한 sub-prefetcher를 학습 노이즈로부터 보호, optional)이다.

> [!question]- Q3. LMHint Prefetcher가 힌트를 저장·조회하는 하드웨어 구조는?
> > main memory에 virtual PC로 인덱싱된 Prefetch Hint Table(PHT)과, 이를 캐싱하는 256-entry TLB 유사 온칩 버퍼 Prefetch Hint Buffer(PHB)로 구성된다. 힌트는 PC당 8-bit(selection 4bit + degree 2bit + filtering 2bit)로 인코딩된다.

> [!question]- Q4. PF-LLM 모델의 최종 예측 정확도는 얼마인가?
> > SPEC 2006으로 학습해 held-out 테스트셋에서 95.0%의 정확도로 최적 프리페칭 정책을 예측한다 (p.9).

> [!question]- Q5. SPEC 2017에서 LMHint-SDF가 baseline 대비 달성한 성능 개선은?
> > 최고 성능 단일 prefetcher(Sandbox) 대비 지오메트릭 평균 IPC 9.8%, 최고 성능 기존 ensemble 기법(Alecto) 대비 18.9% 개선을 달성한다.

> [!question]- Q6. Ablation 실험(LMHint-S → SD → SDF)에서 각 힌트 유형의 기여는?
> > selection hint만 쓰는 LMHint-S 대비, degree 제어를 추가한 LMHint-SD는 평균 IPC 0.3% 향상을, demand filtering까지 추가한 LMHint-SDF는 추가로 0.3% 향상을 가져온다. 두 세부 제어의 기여는 작지만 selection 자체가 가장 큰 이득을 준다.

> [!question]- Q7. 온라인 ensemble 기법(Alecto, DOL)이 오히려 단일 prefetcher보다 못한 성능을 보이는 이유는?
> > PC-중심 온라인 selection 정책이 Pythia처럼 넓은 메모리 영역의 패턴을 추적하는 정교한 spatial prefetcher의 내부 상태를, 무관한 demand 요청으로 훈련시키거나 핵심 demand 요청을 걸러내 오염시키기 때문이다. LMHint Prefetcher는 static, context-aware한 demand filtering 힌트로 이를 회피한다 (p.10-11).

> [!question]- Q8. 논문이 스스로 인정한 두 가지 주요 한계는?
> > (1) static 바이너리 분석에 의존하기 때문에 JIT 컴파일이나 Java 같은 바이트코드 기반 프로그램은 지원하지 못한다(향후 IR 분석으로 확장 가능). (2) static PC로 힌트를 인덱싱하기 때문에 ASLR로 인한 런타임 주소 랜덤화를 고려하지 않았다(OS loader 연동으로 해결 가능) (§7.4, p.13).

## 🔗 Connections
[[LLM Systems]] · [[ASPLOS]] · [[2026]]
관련: [[ArtMem - Adaptive Migration in Reinforcement Learning-Enabled Tiered Memory]] (학습 기반 정책으로 메모리 계층 결정을 자동화한다는 점에서 대비되는 RL-online vs LLM-offline 접근)

## References worth following
- **Pythia: A Customizable Hardware Prefetching Framework Using Online Reinforcement Learning** (Bera et al., MICRO 2021, ref [6]) — LMHint 앙상블에서 demand filtering 대상 제외되는 대표적 "advanced" 정교 prefetcher.
- **DSPatch: Dual Spatial Pattern Prefetcher** (Bera et al., MICRO 2019, ref [7]) — degree 정규화 예시(0-64 range)로 쓰인 또 다른 advanced sub-prefetcher.
- **Sandbox Prefetching: Safe Run-time Evaluation of Aggressive Prefetchers** (Pugsley et al., HPCA 2014, ref [30]) — 본 논문에서 "최고 성능 단일 prefetcher" baseline이자 최빈 최적 정책 sub-prefetcher.
- **Integrating Prefetcher Selection with Dynamic Request Allocation Improves Prefetching Efficiency** (Li et al., HPCA 2025, ref [22]) — "Alecto", 본 논문의 핵심 비교 대상 SOTA ensemble 기법.
- **Division of Labor: A More Effective Approach to Prefetching** (Konduguli & Huang, ISCA 2018, ref [19]) — "DOL" ensemble baseline.
- **Qwen2.5-Coder Technical Report** (Hui et al., 2024, ref [16]) — PF-LLM의 베이스 모델(0.5B) 출처.

## Personal annotations
<!-- 본인 메모 영역 -->
