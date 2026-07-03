---
title: "H1 — 워크로드 특화로 multi-node coherence 줄이기"
aliases: [H1, 워크로드 특화 coherence 가설, workload-specialized coherence]
type: hypothesis
status: working          # working(검증 전) / testing / supported / revised / dropped
formed: 2026-06-29
tags:
  - hypothesis
  - topic/cxl
---

# H1 — 워크로드 특화로 multi-node coherence 줄이기

> [!warning] 검증 전 가설
> 본인이 세운 **working hypothesis** (2026-06-29 형성). 사실 아님 — 검증 대상. 객관 배경은 [[CXL Multi-node Coherence]]·[[CXL Coherence]].

## 한 줄 가설
> **multi-node에서 강한 coherence/synchronization은 (거리×개수로) 비용이 폭발한다. → 범용(general-purpose)을 포기하고 워크로드(거대 모델)에 특화하면, 필요한 coherence만 남기고 나머지를 버려 용량은 키우고 비용은 줄일 수 있다.**

= [[TrainingCXL]]의 뼈대를 스스로 재발견한 것. CUDA의 성공 공식(특화)을 CXL에 적용.

## 문제 (Q2) — multi-node에서 비용 폭발
- coherence(값 일관성, HW 자동)와 synchronization(순서·원자성, SW 명시)은 **역할이 다르고 둘 다** multi-node에서 비싸짐. (구분: [[CXL Coherence]])
- **TM(Transactional Memory)** 예: 노드 수십~수천 × 멀티코어 = 동시 접근 폭증 → 같은 DPA에 몰림 → 충돌↑ → abort/retry 폭발.
- write 시 coherence 전파 = 거리 × 개수 (write-invalidate라도 멀리 여러 host엔 비쌈).
- → directory가 깨지는 지점은 [[CXL Multi-node Coherence]].

## 해법 가설 (Q3) — 워크로드 특화
- **CUDA 비유**: GPU 성공 = "아무거나"가 아니라 "병렬 패턴 특화"(대가: 프로그래머 명시). CXL도 "거대 모델 메모리 패턴 특화" → 안 쓰는 coherence를 버림.
- 근거(객관): **CtXnL**("strict는 overkill"→선택적, OLTP 2.08×), **TrainingCXL**(ML 학습 특성 알면 relaxed/batch-aware). **특화 TM**도 "충돌 드문 패턴"을 알면 롤백 거의 없어 살아남음.
- **composable infra**(CXL 3.0 port-based routing / fabric 재구성, Panmnesia switch) = 용도에 맞춰 메모리-컴퓨트 연결 동적 변경 → 특화를 가능케 하는 토대.

## 세 질문이 하나로 수렴
```
Q1 coherence(값) vs sync(순서) — 둘 다 필요, multi-node면 둘 다 비쌈
Q2 TM도 multi-node면 충돌↑ + coherence 전파↑          ← 문제
Q3 그러니 범용 말고 특화로 필요한 coherence만           ← 해법
        ↓
"multi-node에서 강한 coherence/sync는 비용 폭발 → 워크로드 특화로 줄여라"
```

## 적합성 (왜 내가)
- 이 접근 = **ML을 아는 시스템 연구자**만 가능 (거대 모델 메모리 패턴 이해 + HW 특화).
- 본인 배경: ML 경험(VRAIL, Point Transformer, checkpointing pain) + 시스템(KECC, Verilog, gem5). CAMEL이 정확히 이런 랩([[TrainingCXL]]이 증거).

## ⚠️ Trade-off / 열린 문제 (정직하게)
- **특화의 대가 = 범용성 상실** (CUDA가 비병렬에 약하듯).
- **누가 명시하나** — coherence를 사용자가 코딩? → 프로그래밍 복잡도↑. 그 복잡성 누가 감당.
- ★ **진짜 질문 = "특화/범용 경계를 어디에."** 너무 특화면 못 쓰고, 너무 범용이면 안 빨라짐 — 그 경계 설계가 연구.
- → "누가 명시하나(자동 profiling vs user 명시)"를 파고든 **다음 단계 = [[H2 — CXL 위에서 PGAS 재해석]]** (두 갈래가 PGAS로 수렴).

## 검증 계획 (다음에 팔 것)
- [ ] 거대 모델의 어떤 메모리 접근이 "coherence 불필요" 패턴인가 (구체화)
- [ ] 그 패턴을 누가 어떻게 명시 (API? 컴파일러 자동? HW 힌트?)
- [ ] 특화 TM의 충돌률을 실제 워크로드로 측정 (가설 검증)
- [ ] 방법론: FPGA로 feasibility 검증 ([[CXL Multi-node Coherence]]의 directory/relaxed 설계와 연결)

---
**관련**: [[CXL Multi-node Coherence]] · [[CXL Coherence]] · [[TrainingCXL]] · [[Communication Tax]]
