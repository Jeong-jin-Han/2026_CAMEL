---
title: "Deadlock · Livelock · Starvation — 세 가지 '멈춤'과 보장의 사다리"
aliases: [Deadlock, Livelock, Starvation, Coffman conditions, hold-and-wait, 교착상태, 기아, 보장의 사다리]
type: concept
tags:
  - concept
  - concept/os
  - topic/concurrency
  - topic/interconnect
---

# Deadlock · Livelock · Starvation — 세 가지 '멈춤'과 보장의 사다리

> [!abstract] 이 노트는 뭐지?
> [[Venice]]의 interconnect 설계를 뜯다 나온 동시성 이론 3형제. **deadlock의 4대 필요조건(Coffman)과 "하나만 깨면 원천 차단"**, livelock의 정의와 유한성 기반 보장, starvation과 fairness — 그리고 셋을 한 줄에 세우는 **보장의 사다리**를 정리한다. OS lock 이론으로 배우지만, lock이 없는 하드웨어(라우터·회로 스위칭)에도 그대로 적용되는 일반 이론이다. 원본: Venice NodeGraph의 deadlock/livelock/starvation 노드들.

## 한 문장
Deadlock은 "서로 쥐고 기다려서 **아무도** 못 움직임", livelock은 "계속 움직이는데 **아무도** 진전 없음", starvation은 "시스템은 진전하는데 **누군가만** 영원히 대기" — 셋은 서로 다른 실패이며, 이를 막는 보장도 $\text{deadlock-free} < \text{livelock-free}(\exists) < \text{starvation-free}(\forall)$로 강도가 다르다.

## 1. Deadlock — 4대 필요조건과 "하나만 깨면 된다"

**Coffman conditions** — deadlock이 성립하려면 네 가지가 **전부** 필요하다:

| # | 조건 | 뜻 |
|---|---|---|
| 1 | **Mutual exclusion** (상호배제) | 자원을 한 번에 하나만 쓸 수 있음 |
| 2 | **Hold-and-wait** (점유대기) | 자원을 쥔 채로 다른 자원을 기다림 |
| 3 | **No preemption** (비선점) | 쥔 자원을 강제로 뺏을 수 없음 |
| 4 | **Circular wait** (원형대기) | A→B→C→A로 서로가 서로의 자원을 기다리는 고리 |

> [!tip] 핵심 — 필요조건이므로 **하나만 깨면 deadlock은 원천적으로 불가능**
> 넷 다 있어야 성립하니, 설계자는 넷 중 **깨기 가장 싼 것 하나**를 골라 부수면 된다:
> - 상호배제 깨기 → 자원을 공유 가능하게 (read-only 복제 등)
> - **hold-and-wait 깨기** → 못 얻으면 쥔 것을 **놓고 물러남** ← [[Venice]]의 선택
> - 비선점 깨기 → 강제 회수 허용 (preemption)
> - 원형대기 깨기 → 자원에 전역 순서를 매겨 그 순서로만 획득 (lock ordering)

**Venice의 사례**: scout packet이 라우터 A의 port를 예약해 쥔 채 B의 port가 비기를 기다리면 hold-and-wait → 여러 scout가 얽히면 circular wait → deadlock. Venice는 막히면 **예약을 취소(cancel)하고 backtrack** — hold-and-wait 조건 자체가 성립하지 못하게 설계해 deadlock-free.

> [!note] lock이 없어도 적용된다 — 순수 자원-그래프 이론
> hold-and-wait는 mutex/semaphore라는 특정 SW 구현에 종속된 개념이 아니다. Venice엔 lock이 없다 — 그냥 하드웨어 라우터의 **reservation table 항목**을 쥐었냐 아니냐다. "자원을 배타적으로 점유한 채 다른 자원을 기다린다"는 **패턴** 자체를 가리키므로, 회로 스위칭 네트워크·lock-free 분산 트랜잭션 등 배타적 점유가 존재하는 모든 시스템에 똑같이 적용된다.

## 2. Livelock — 움직이는데 진전이 없다

deadlock은 멈춰서 못 가는 것, livelock은 **계속 상태가 바뀌는데 아무도 앞으로 못 가는 것** (복도에서 마주친 두 사람이 동시에 같은 쪽으로 계속 비켜주는 상황). backtrack·retry 같은 "deadlock 회피 동작"이 오히려 livelock의 재료가 된다 — 물러났다 다시 시도하기를 전원이 무한 반복하면?

**Venice의 회피**: scout가 같은 라우터를 최대 3번(2D mesh에서 $4-1$)까지만 방문하도록 제한 — 초과하면 source로 복귀해 새 scout로 재시도.

> [!tip] Livelock-freedom은 측정이 아니라 **유한성에서 나오는 논리적 보장**
> "적어도 하나는 결국 진전한다"(forward progress)는 latency를 재서 확인하는 통계가 아니라, 두 **유한성(finiteness)**의 조합에서 유도된다:
> 1. **탐색이 유한하게 끝난다** — 재방문 제한 + 유한한 mesh 지름 → 한 scout 시도는 반드시 성공 아니면 실패로 종결 (무한 배회가 구조적으로 불가능)
> 2. **점유가 유한 시간 안에 풀린다** — 예약된 link는 flash operation(수 $\mu s$)만큼만 잡다가 반드시 해제
>
> 둘을 합치면: 지금 모든 scout가 실패 중이어도 그건 일시적 backpressure일 뿐 — 진행 중인 전송이 끝나며 link가 풀리는 순간, 재시도하던 scout 중 **하나는 반드시** 그 link를 잡는다. ("99.98% 첫 시도 성공" 같은 수치는 이 보장의 확인이지 근거가 아님.)

## 3. Starvation — 시스템은 도는데 나만 굶는다

deadlock/livelock이 없어도, **특정 요청만 운 나쁘게 계속 밀려** 영원히 서비스 못 받을 수 있다. 시스템 전체는 진전하므로($\exists$) livelock-free지만, **모든** 요청이 언젠가 서비스된다는($\forall$) 보장은 별개다.

- 고전 해법: **aging**(기다린 시간만큼 우선순위↑), oldest-first, starvation counter
- **Venice는 starvation-freedom을 보장하지 않는다** — priority 구분 없는 egalitarian "fair competition"(모든 I/O가 동등 자격으로 경쟁). 언뜻 공정해 보이지만 위 고전 기법을 전혀 안 쓴다는 뜻이기도 하다

## 4. 보장의 사다리 (강도 순)

$$\text{deadlock-free} \;<\; \underbrace{\text{livelock-free}}_{\exists\,\text{: 누군가는 진전}} \;<\; \underbrace{\text{starvation-free}}_{\forall\,\text{: 모두가 언젠가 서비스}}$$

| 보장 | 약속 | Venice |
|---|---|---|
| deadlock-free | 전원 정지는 없다 | ✅ hold-and-wait 파괴 (cancel & backtrack) |
| livelock-free | 적어도 하나는 진전한다 ($\exists$) | ✅ 유한성 2개 (재방문 제한 + 점유 유한) |
| starvation-free | 모든 요청이 언젠가 서비스된다 ($\forall$) | ❌ 미보장 — egalitarian 경쟁만 |

**빈틈이 다음 연구가 된다**: Venice가 비워둔 $\forall$ 축을 후속 논문 **GC bypass**가 공략 — user I/O와 GC 간 우선순위 차등이 없어 생기는 GC-induced long-tail latency를, 전용 GC controller·priority·preemption으로 메움. *"논문 A가 무엇을 fair하다고 가정했고, 그 가정이 어디서 깨지며, 논문 B가 어떻게 메웠는가"* 프레임의 실제 사례.

## 5. 내 연구와의 연결
- **[[CXL Multi-node Coherence]]·[[H1 — 워크로드 특화로 multi-node coherence 줄이기|H1]]**: multi-node에서 TM 충돌→abort/retry 폭발은 livelock/starvation 스펙트럼의 문제 — 이 사다리로 "어떤 보장까지 줄 것인가"를 설계 축으로 쓸 수 있음
- **[[Venice]]**: cross-domain 이식의 본보기이자 이 이론의 하드웨어 실례 — OS 이론(Coffman)이 NoC/Dally & Towles 라우팅 이론과 같은 계보로 만나는 지점
- 발표 Q&A 방어: "왜 deadlock 안 나요?" → "4조건 중 hold-and-wait를 설계로 깼다" 한 줄이면 됨

---
**관련**: [[Venice]] · [[CXL Multi-node Coherence]] · [[Design Principles]] · [[H1 — 워크로드 특화로 multi-node coherence 줄이기]]
