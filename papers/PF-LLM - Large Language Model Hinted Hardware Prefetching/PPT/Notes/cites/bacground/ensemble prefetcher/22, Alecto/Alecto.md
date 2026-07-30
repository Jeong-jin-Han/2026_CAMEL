# Alecto (Integrating Prefetcher Selection with Dynamic Request Allocation)

> **출처**: Mengming Li, Qijun Zhang, Yongqing Ren, Zhiyao Xie. *"Integrating Prefetcher Selection with Dynamic Request Allocation Improves Prefetching Efficiency"*, **HPCA 2025** (HKUST + Intel). 이름 Alecto = **Selection + Allocation** 의 합성어.
> **맥락**: PF-LLM(§6.4)이 SOTA "ensemble/prefetcher-selection" 기법으로 지목하고 비교·비판하는 대상. 여기서는 "왜 Alecto가 실패하는가"를 설명하기 위해 방법론을 상세 정리.

---

## 분류

- **HW/SW**: **순수 하드웨어(HW)** — 온칩 하드웨어 테이블 3개(Allocation Table, Sample Table, Sandbox Table)로 구현. 총 스토리지 오버헤드 약 **1.3 KB**(P=3, Sandbox 포함), Sandbox 제외 시 약 760 B. gem5 사이클 정확 시뮬레이션으로 평가.
- **offline/online**: **online(런타임 적응)** — 런타임에 수집한 prefetching **accuracy** 를 기준으로 매 epoch(≈100 demand access)마다 각 prefetcher의 상태(state)를 갱신. 오프라인 프로파일/사전학습 없음.
- **sub-prefetcher 개수 / 구성**: Alecto 자체는 **prefetcher 개수 P에 무관하게 확장(선형 스토리지)** 되는 *스케줄러/오케스트레이터*이지 prefetcher가 아님. 실험 기본 구성은 **3개 composite**:
  - **GS** (Global Stream, stream prefetcher, IPCP의 GS)
  - **CS** (Cross-page Stride, stride prefetcher, IPCP의 CS)
  - **PMP** (spatial prefetcher, [27])
  - 추가 실험: spatial을 Berti/CPLX로 교체, temporal prefetcher(Triangel류 on-chip TP)까지 포함해 관리.
- **orchestration 방식**: **prefetcher selection + dynamic demand request allocation(DDRA)** 을 하나로 통합. **키(key) = PC(memory access instruction address)**. 즉 "이 PC의 demand request를 어느 prefetcher에게 (그리고 어느 aggressiveness로) 흘려줄지"를 PC별로 결정.

---

## 방법론 (상세)

### 핵심 아이디어
기존 selection 기법은 prefetcher의 **출력(prefetch degree, on/off)** 만 통제하지, prefetcher **테이블을 학습시키는 입력(demand request)** 은 통제하지 못한다. 그 결과 (1) 담당이 아닌 prefetcher까지 모든 demand로 학습되어 테이블이 오염되고(비적합 엔트리가 유용 엔트리를 축출 → prefetcher table miss 증가), (2) prefetch queue/cache/DRAM bandwidth 등 공유 자원 충돌이 발생. Alecto는 **"각 prefetcher가 자기 담당 패턴의 demand request만 받도록 입력을 미세 할당(fine-grained allocation)"** 해서 이 둘을 동시에 해결한다.

### 3개 하드웨어 구조
1. **Allocation Table** (PC로 인덱싱, 64 entry): Alecto의 핵심. **PC마다 각 prefetcher의 상태(state)** 를 저장하고, 그 상태에 근거해 demand request 할당과 prefetch degree를 결정.
2. **Sample Table** (PC로 인덱싱, 64 entry): 런타임 메트릭 수집기. prefetcher별 `IssuedByP1`(발행한 prefetch 수), `ConfirmedP1`(후속 demand가 hit시킨 수) → **accuracy** 계산. 추가로 `Demand Counter`(epoch 타이밍용, 100 도달 시 Allocation Table 갱신 트리거), `Dead Counter`(prefetch를 오래 못 내면 증가하는 saturating counter, deadlock 방지용, threshold≈150 도달 시 해당 PC의 모든 상태를 UI로 리셋). PC 저장은 BPU식 XOR-fold 해시로 압축.
3. **Sandbox Table** (memory address로 인덱싱, 512 entry): 이중 역할 — (a) 최근 발행한 prefetch 주소를 기록해 후속 demand hit 여부(=정확도) 판정을 Sample Table에 제공, (b) **prefetch filter** 로 중복 prefetch 제거(tag hit이면 request drop).

### selection(선택) 로직 — 상태 기계(Fig. 5)
Allocation Table은 각 PC에 대해 prefetcher마다 3개 상태를 부여하고, 두 임계값 **PB(Proficiency Boundary=0.75)**, **DB(Deficiency Boundary=0.05)** 를 accuracy에 비교해 전이시킨다.
- **UI (Un-Identified)**: 적합성 미확정. 보수적으로 낮은 degree(예: c=5)만 허용.
- **IA_m (Identified & Aggressive)**: 적합·효율적. demand 할당 + degree 상향. 0..M(M=5)의 M+1개 substate, 값이 클수록 더 공격적. accuracy>PB면 IA_m→IA_{m+1}, <DB면 하향.
- **IB_n (Identified & Blocked)**: 부적합. demand 할당 **차단**. -N..0(N=8)의 N+1개 substate, 작을수록 오래 차단. epoch마다 IB_n→IB_{n+1}로 서서히 재평가로 복귀.
- 주요 전이: accuracy≥PB → UI를 IA_0로 승격, 나머지는 IB_0로 강등; accuracy<DB → UI를 IB_-N으로. temporal prefetcher는 예외적으로 강등(metadata 절약).
- **원칙**: 고성능 prefetcher에만 demand 몰아주고 더 공격적으로, 저성능은 일시(영구 아님) 차단해 낭비/충돌 억제.

### request allocation(요청 할당)
demand request 진입 시 PC로 Allocation Table lookup:
- **UI** prefetcher → identifier(sequence# + 보수적 degree c) 부여.
- **IA_m** prefetcher → identifier(sequence# + degree **c+m+1**). 그중 c 라인은 자기 캐시 레벨에 직접, 나머지 m+1 라인은 next-level로 prefetch.
- **IB_n** prefetcher → **identifier 미생성 = demand 미할당(차단)**.
identifier들은 demand와 함께 **multiplexer** 로 라우팅되어, 매칭된 prefetcher만 그 request를 받아 학습·prefetch 생성. → prefetcher별 "담당 필드"에 맞는 입력만 도달.

### online 학습 방식
런타임에 Sample/Sandbox Table이 prefetcher별 **정확도**(발행 대비 후속 demand hit)를 지속 측정 → epoch(100 demand access)마다 Allocation Table 상태 전이. 즉 "무엇을 학습"이 아니라 **"각 PC에서 어느 prefetcher가 지금 잘 맞는가"를 런타임 accuracy로 계속 재평가**하고, 그 결과로 입력 할당과 aggressiveness를 조정. Dead Counter로 패턴 변화(교착) 감지 시 상태 리셋.

### PC-centric인가? — **예 (강하게 PC 중심)**
- Allocation Table **과** Sample Table이 **둘 다 PC로 인덱싱**됨. selection 결정(어느 prefetcher가 UI/IA/IB인가)과 request 할당이 모두 **PC 단위로 저장·판단**된다.
- Fig. 2/서론에서 명시적으로 "prior work이 **PC-grained identification 없이** 선택 규칙을 세워 실패한다"고 비판하며, 자신은 "**PC 정보를 활용해 PC별로 적합 prefetcher를 pinpoint**"함을 핵심 차별점으로 내세운다.
- 따라서 **PF-LLM §6.4가 Alecto를 "PC-centric online policy"라 규정한 것은 정확**하다. Alecto는 설계 철학상 "PC마다 어느 prefetcher가 맞는가"를 학습하는 기법이다.

---

## PF-LLM이 이걸 비판하는 지점

> 아래는 PF-LLM의 주장을 이 논문 내용에 비추어 해석한 것으로, **괄호 표기 = Alecto 논문 자체 근거 / "PF-LLM 주장"** 을 구분.

- **PC 지역성에 대한 구조적 의존 (Alecto 논문 근거)**: selection·allocation의 상태와 accuracy 통계가 전부 **PC 키**로 관리된다. PC마다 "이 PC엔 stride, 저 PC엔 spatial" 식으로 담당을 나누는 것이 강점(Fig. 2의 interleaved PC 예시)이지만, 이는 곧 **prefetcher를 PC-localized 관점에서만 배치**한다는 뜻이다.
- **spatial/non-PC-localized prefetcher와의 부정합 (PF-LLM 주장)**: PMP·SMS·BOP 같은 spatial prefetcher는 본질적으로 **PC가 아니라 region/spatial footprint** 로 동작한다. 여러 PC가 같은 region에 협력해 접근하는 패턴을, PC별로 쪼개 "이 PC엔 spatial 차단(IB)"으로 판단하면 region 단위 유용 패턴을 놓칠 수 있다. Alecto는 spatial도 PC-keyed accuracy로 UI/IA/IB를 매기므로, **PC-지역성이 약한 워크로드(예: LLM/GNN류 대규모·불규칙 접근)에서 selection 신호가 희석**된다 — 이것이 PF-LLM이 "PC-centric online policy는 실패한다"고 지적하는 핵심.
- **online 적응의 반응 지연 (PF-LLM 주장)**: epoch(100 access)·Dead Counter(150) 기반 상태 전이는 **런타임에 관측이 누적된 뒤에야** 적합 prefetcher를 찾는다. 패턴이 빠르게 바뀌거나 PC 재사용이 적으면 "언제나 뒤늦게 학습"하는 구조 → PF-LLM처럼 **미래 접근을 사전에 힌트로 아는 방식** 과 대비된다.
- **주의**: Alecto 논문 자체는 spatial/temporal까지 잘 관리한다고 주장(Fig. 11 Berti/CPLX, Fig. 6/§IV-F temporal). 위 비판은 **Alecto 논문엔 없고 PF-LLM 측 관점**이다. 즉 "PC-centric이다"라는 사실 규정은 정확하되, "그래서 실패한다"는 평가는 PF-LLM의 주장.

---

## 핵심 요약

- Alecto는 prefetcher를 새로 만드는 게 아니라, **여러 sub-prefetcher(기본 GS+CS+PMP 3개)를 조율하는 순수 HW online 스케줄러**로, 기존 selection이 못 하던 **입력(demand request)의 미세 할당(DDRA)** 을 selection과 통합한 것이 핵심 기여다.
- **PC로 인덱싱된 Allocation/Sample Table** 이 런타임 accuracy로 각 PC마다 prefetcher를 UI/IA/IB 상태로 분류하고, 그 상태에 따라 demand를 "맞는 prefetcher에만" 흘려 테이블 오염·자원 충돌을 줄인다(prefetcher table miss ~1/3, Bandit 대비 IPC +2.76%/8-core +7.56%, 에너지 -7%, <1 KB).
- **한계(PF-LLM 관점)**: 정책 전체가 **PC 중심(PC-centric online)** 이라, PC 지역성이 약하거나 spatial/불규칙 대규모 접근(LLM류)에서는 selection 신호가 약해지고 런타임 학습이 항상 뒤따라가는 구조 — PF-LLM이 이 지점을 겨냥해 baseline으로 비판.
