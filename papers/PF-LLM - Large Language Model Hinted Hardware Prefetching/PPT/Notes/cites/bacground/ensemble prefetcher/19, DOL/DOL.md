# DOL (Division of Labor: A More Effective Approach to Prefetching)

> Sushant Kondguli, Michael Huang (University of Rochester) — **ISCA 2018**
> composite prefetcher = **TPC** (= T2 + P1 + C1)

## 분류
- **HW/SW**: **순수 하드웨어(HW) prefetcher** — gem5(execution-driven simulator)로 평가하는 마이크로아키텍처 설계. 모든 컴포넌트(T2/P1/C1)와 coordinator가 하드웨어 로직·테이블(SIT, TPU, IM/RM 등)로 구현됨. 컴파일러/SW 힌트 없음.
- **offline/online**: **online(런타임 학습)** — 각 컴포넌트가 실행 중에 per-instruction으로 pattern을 학습·적응(SIT training, taint propagation, region monitoring). 단, 논문의 **LHF/MHF/HHF(low/mid/high-hanging fruit) 3분류는 "offline ground-truth 분석용"일 뿐 설계의 일부가 아님** — 저자가 명시("division is done offline to have a better approximation to ground truth"). 즉 실제 동작은 전부 online.
- **sub-prefetcher 개수 / 구성**: **3개의 custom 컴포넌트** (composite 이름 = **TPC**)
  - **T2**: canonical strided streams (내부 루프 안 단일 instruction의 constant-stride 반복). 루프 하드웨어 + Stride Identifier Table(SIT) 사용.
  - **P1**: pointer 패턴 2종 — (a) array of pointers(strided load가 가리키는 값을 다시 접근), (b) pointer chains(주소가 이전 iteration의 자기 destination에 의존). Taint Propagation Unit(TPU)로 dependent load 탐지.
  - **C1**: high spatial locality "carpet bombing" region prefetcher — super-cache-line(16 lines) region 전체를 prefetch. Instruction Monitor(IM) + Region Monitor(RM)로 dense region을 접근하는 instruction을 식별.
  - (추가로 기존 monolithic prefetcher들 — VLDP/SPP/FDP/SMS — 을 "component"로 붙여 실험하기도 함, §IV-E.)
- **orchestration 방식**: **coordinator(조율기)** 가 각 memory instruction을 컴포넌트에 **순차적으로(in turn)** 제시하는 **hardwired combinational 로직**. "division of labor"는 **prefetch scope(무엇을 target하는가)를 여러 전문화 컴포넌트에게 역할 분담**시키는 것 — scope와 accuracy를 **decouple**하는 게 핵심. 추가 저장공간 없이 조합 논리만으로 동작.

## 방법론 (상세)

### 핵심 아이디어 "division of labor"
- 문제의식: monolithic prefetcher는 **scope(coverage 야망)를 넓히면 accuracy가 떨어지는 내재적 tradeoff**를 가짐. 논문은 두 지표로 정량화 — **scope**(prefetcher가 시도한 miss footprint 비율)와 **effective accuracy**(회피된 miss / 발행된 prefetch 수). Fig.1에서 AMPM→BOP→SMS로 갈수록 scope 67%→76%→87% 오르지만 accuracy 58%→49%→48%로 하락.
- 해법: **하나의 monolithic 설계로 넓은 scope를 노리지 말고, 각각 좁지만 정확·효율적인 특화 컴포넌트를 만들어 협업(collaboration)**시킨다. 그러면 scope는 "좋은 조합"으로, accuracy는 "개별 컴포넌트 개선"으로 **독립적으로(decoupled)** 달성 가능.
- 즉 **여러 prefetcher가 각자 다른 access pattern을 전담(specialize)**하고, coordinator가 그들을 non-overlapping하게 배분. 이것이 단순히 여러 prefetcher를 병렬로 돌리는 "**shunting**"과 다른 점 — shunting은 노력이 겹치지만(overlapping) division of labor는 진짜 분업(§V-C3, Fig.15: composite가 shunting보다 항상 우수, shunting은 TPC 단독보다도 1~6% 나쁨).
- 이점 3가지: **Efficiency**(각 컴포넌트가 자기 pattern만 memorize → storage 효율, false positive 감소), **Clarity**(pattern별 성공확률을 따로 추적 → 자원배분·prefetch destination 결정 용이), **Flexibility**(feedback으로 컴포넌트 on/off·파라미터 조정 가능).

### selection/할당 로직
- **coordinator = hardwired combinational 로직, 추가 storage 없음**(Fig.7).
- **컴포넌트에 memory instruction을 순차로 제시**하고, 각 컴포넌트가 "내가 담당하는 instruction인가?"를 스스로 판정:
  - `isStridePC?` → **T2**가 담당
  - (T2가 담당 못하면) `isPtrPC?` → **P1**이 담당
  - (그것도 아니면) `isDensePC?` → **C1**이 담당
- **순서 = T2 → P1 → C1** (T2가 가장 많은 케이스를 커버하므로 먼저 시도; T2가 안 잡으면 P1; 마지막에 C1).
- **결정 키 = instruction(PC)**. 각 컴포넌트는 자기 테이블(T2/P1은 SIT, C1은 IM)에 "이 PC는 내 담당"이라고 학습해둔 상태로 판정. 즉 **어느 prefetcher가 담당하는지는 static instruction(PC) 단위로 배정**됨.
- prefetch destination도 컴포넌트별로 stratify: T2·P1은 정확도 높아 L1까지 prefetch, C1은 정확도 낮아 L2를 target.
- **기존 prefetcher를 component로 쓸 때의 coordinator 휴리스틱**(§IV-D 끝): ① 각 access pattern에 가장 적합한 컴포넌트 식별 ② 나머지 컴포넌트엔 그 access를 안 넘겨 오발행 최소화 ③ 여러 컴포넌트가 동시에 claim하면 **round-robin**으로 분배, prefetch된 line에 발행 컴포넌트 ID 태그 → demand hit 시 그 컴포넌트가 이후 담당.

### online 학습 방식 (런타임에 무엇을 학습/적응하나)
- **T2**: ① 루프 하드웨어로 inner loop 경계(back-to-back backward branch) 탐지, Non-Loop PC Table(20-entry)로 loop 아닌 branch 필터. ② **Stride Identifier Table(SIT)** 에 각 memory instruction의 (mPC, LastAddr, Delta) 추적, delta가 안정적이면 strided로 라벨. I-cache에 4-state(unknown/observation/strided/non-strided) 라벨링으로 primary miss 유발 instruction만 활성 추적. 같은 delta 16회 연속 → strided, 변하는 delta 4회 → non-strided. prefetch distance는 $d = \frac{AMAT+m}{T_{iter}}$ 로 런타임 계산(AMAT·iteration 실행시간 추적).
- **P1**: **Taint Propagation Unit(TPU)** — strided load의 destination register를 seed로 taint를 전파해 그에 의존하는 load(array of pointers) 탐지; pointer chain은 destination이 자기 자신에 transitively 의존하는지로 식별. 확장된 SIT에 special strided pointer로 표시, 런타임에 delta·value 추적하며 self-correcting.
- **C1**: **Instruction Monitor(IM)** + **Region Monitor(RM, 16-entry, 16-line region)** — region이 dense(≥6 lines set)한지 관찰, instruction별 `TotalRegions`/`DenseRegions` 카운터로 "이 instruction이 dense region을 높은 확률(>3/4)로 접근하는가"를 런타임 판정 → dense PC로 마킹 후 region prefetch trigger.
- **coordinator 자체의 학습**: 논문은 coordinator가 두 "conjectural principle"에 의존한다고 명시 — ① **Expertise can be measured**(컴포넌트별 effective accuracy를 측정해 pattern별 최적 컴포넌트 선택 가능) ② **Patterns are tied to static instructions**(pattern이 static instruction에 묶여 있으므로 static instruction 기반으로 division of labor를 확립 가능). 이 두 번째 원칙이 곧 **PC-centric 가정**.

### PC-centric인가?
- **예 — DOL의 coordination 정책은 명백히 PC-centric(instruction-based)이다.** 근거(논문 본문):
  - "Both T2 and P1 are already **instructions based**. They only identify instructions they can handle." (§IV-D)
  - coordinator는 memory **instruction**을 컴포넌트에 제시하고 `isStridePC/isPtrPC/isDensePC`로 배정 (Fig.7의 판정 신호가 전부 PC 기반).
  - C1조차 **Instruction Monitor**로 region locality를 **static instruction에 묶어** 관리("we try to associate a high locality access stream with instructions").
  - coordinator 설계 원칙 ②가 대놓고 "**Patterns are tied to static instructions**" — 그래야 "static instruction 기반으로 reasonable division of labor를 확립"할 수 있다고 명시.
- 따라서 **PF-LLM §6.4가 DOL을 "PC-centric online policy"라 부른 것은 논문 실체와 정확히 일치**한다. DOL의 분업·할당 단위가 PC(static instruction)이기 때문.

## PF-LLM이 이걸 비판하는 지점

- **핵심 비판(=PF-LLM 자체 주장, §6.4)**: DOL은 **PC(static instruction) 단위로 prefetcher를 배정**한다. 이 방식은 DOL 자신의 컴포넌트(T2=PC별 stride, P1=PC별 pointer, C1=PC별 dense region)가 **전부 PC-localized(PC에 pattern이 묶인)** 이기 때문에 잘 동작한다. 그러나 **spatial/global-history 계열처럼 non-PC-localized(특정 triggering PC에 묶이지 않고 주소공간 전역 pattern을 보는) prefetcher를 앙상블 관리**하려 하면, PC를 키로 하는 coordinator로는 어느 prefetcher가 언제 좋은지 깔끔히 배정할 수 없다 → 관리 실패.
- **논문 내부 근거(DOL 스스로 인정한 한계)**:
  - coordinator 설계가 "**컴포넌트의 non-ideal한 특성·idiosyncrasy에 크게 의존**"하며 "a thorough exploration ... is premature", "**first-effort coordinator**"라고 명시 → coordination이 견고한 일반 원리가 아니라 특정 컴포넌트 가정에 맞춘 것임을 저자도 인정.
  - 두 원칙 중 ② "Patterns are tied to static instructions"가 **성립할 때만** 통함 — 즉 pattern이 PC에 묶이지 않으면 DOL의 분업 논리가 깨진다. PF-LLM이 정확히 이 지점을 공격.
  - 여러 컴포넌트가 동시에 claim할 때 DOL의 처리는 단순 **round-robin**(§IV-D) → 진짜 "누가 이 access에 최적인가"를 online으로 정교하게 고르는 메커니즘이 아니라 임시방편.
  - 저자 스스로 "blindly allow all to try하면 pollution이 커져 이득을 상쇄/무효화한다"고 경고 → coordinator가 잘못 배정하면 오히려 해로움.
- 요약: **DOL의 orchestration은 "pattern이 PC에 국소화되어 있다"는 가정 위에 세워진 PC-centric online policy이며, 이 가정을 어기는(예: LLM-hinted·spatial·global) prefetcher를 관리할 때 무너진다** — 이것이 PF-LLM이 DOL을 baseline으로 두고 "왜 실패하는가"를 설명하는 논거.

## 핵심 요약
- **DOL = "division of labor"** — monolithic prefetcher의 scope↔accuracy tradeoff를 깨기 위해, 넓은 scope를 한 설계로 노리지 말고 **좁지만 정확한 특화 컴포넌트(T2 strided / P1 pointer / C1 spatial region) 3개를 coordinator로 분업**시킨 composite prefetcher(TPC). SPEC/graph/embedded/scientific에서 speedup 1.41(monolithic 1.21~1.33), traffic overhead 6%(타 8~12%)로 우수.
- **한계**: coordinator가 **PC(static instruction)를 키로 컴포넌트를 배정**하는 hardwired·round-robin 로직이라, 저자 스스로 "first-effort", "컴포넌트 non-ideal 특성에 의존"이라 인정. **pattern이 PC에 묶여 있다는 가정에 전적으로 의존** → pattern이 PC-localized하지 않은(spatial/global/LLM-hinted) prefetcher를 앙상블 관리하려 하면 이 PC-centric 정책이 실패한다는 것이 PF-LLM(§6.4)의 비판 지점.
