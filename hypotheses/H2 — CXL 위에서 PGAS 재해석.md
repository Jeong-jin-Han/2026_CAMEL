---
title: "H2 — CXL 위에서 PGAS 재해석 (워크로드 특화를 '어떻게')"
aliases: [H2, CXL PGAS, PGAS on CXL, CXL 위 PGAS, PGAS 재해석]
type: hypothesis
status: working          # working(검증 전) / testing / supported / revised / dropped
formed: 2026-06-29
tags:
  - hypothesis
  - topic/cxl
  - topic/pgas
---

# H2 — CXL 위에서 PGAS 재해석

> [!warning] 검증 전 가설
> 본인의 **working hypothesis** (2026-06-29 형성, notes5). 사실 아님 — 검증 대상. 객관 배경은 [[PGAS]]·[[CXL Multi-node Coherence]].
> **위치**: [[H1 — 워크로드 특화로 multi-node coherence 줄이기|H1]]이 *"왜 워크로드 특화"*라면, H2는 그 **다음 질문 = "그 특화를 어떻게 실현하나"**. H1의 열린 문제("누가 coherence를 명시하나")에 대한 답 탐색.

## 한 줄 가설
> multi-node coherence 특화(H1)를 실현하는 두 갈래(자동 배치 / 명시적 제어)를 파보면 둘 다 **PGAS**로 수렴한다. 전통 PGAS는 원격 접근이 **네트워크 메시지**라 느렸지만, **CXL의 cache-coherent 직접 접근**이 그 전제를 바꾸므로 — **"PGAS를 CXL 하드웨어 위에서 재해석"** 하는 것이 내 기여 지점 후보다.

## H1 → H2 연결 (왜 이 가설로 왔나)
H1의 열린 질문 = *"안 쓰는 coherence를 버리려면 누가 무엇이 특화됐는지 정하나?"* → 두 접근을 탐색:
```
접근 A (자동)  profiling으로 접근 패턴 추적 → 예측 → 미리 배치. user는 몰라도 됨.
접근 B (명시)  user가 coherence를 의식하며 코딩 (CUDA처럼).
        ↓ 탐색해보니 둘 다
접근 A = PGAS 컴파일러 자동 최적화 (Chapel "distributed" 자동 매핑)
접근 B = PGAS 명시적 제어 (UPC, Co-Array Fortran)
```
= 내가 던진 두 질문이 사실 PGAS의 오래된 논쟁 **"얼마나 자동 vs 얼마나 프로그래머에게"**의 양쪽. (PGAS 객관 설명은 [[PGAS]].)

## 접근 A — profiling 기반 자동 배치 (첫 아이디어)
- **아이디어**: user 명시는 어려우니, profiling으로 주소 접근 추적 → 다음 접근 예측 → 미리 migration해서 coherence 트래픽 최소화. 연산 말고 **주소 연산만 FPGA에서 빠르게.**
- **재발명 주의**: prefetching·page migration·access pattern profiling은 이미 수십 년 연구됨(→ [[PGAS]]의 인접 개념). 세미나 "그거 prefetching 아니에요?" 대비 필수.
- **새로움(있음)**: 전통 prefetch 목적 = *속도*(latency 숨기기), 내 목적 = **coherence 비용 최소화** → 결이 다름. + **multi-node CXL** 맥락(전통은 single-node). → 기여 지점 = "coherence 비용 기준, multi-node, 접근 예측 배치, FPGA."
- ⚠️ **벽(정직하게)**: ① 예측 틀리면 오히려 coherence↑(정확도가 생명), ② profiling 비용이 줄이려는 비용을 잡아먹을 수 있음, ③ 타이밍(실시간 vs 사전, 워크로드 바뀌면 무의미), ④ "똑똑한 예측"이 FPGA에서 빠를까(단순 주소 산술 이상이면 속도 이슈).

## 접근 B — user 명시 = 이미 PGAS였음
- CUDA가 "GPU vs CPU"를 의식하게 하듯, PGAS는 "로컬 vs 원격 메모리"를 의식하게 함 = 내가 원한 그것.
- **release consistency**(동기화 지점 이후에만 원격 업데이트 보임) = 내 "필요한 곳만 coherence"와 일치.
- → 0부터 안 해도 됨. UPC/Chapel/release consistency를 발판으로.

## ★ 핵심 베팅 — CXL가 PGAS를 다시 연다
```
전통 PGAS: 원격 접근 = 네트워크 메시지 (느림, message-passing 기반, InfiniBand 시대)
CXL 시대: 원격 접근 = CXL로 직접 메모리 접근 (cache-coherent) → 완전히 다른 설계 가능
```
- "PGAS를 CXL 하드웨어 위에서 재해석" = 기여 지점 후보.
- **낙담이 아니라 발판인 이유**: ① 내 직관이 학계 검증 방향(PGAS)과 일치 = 방향이 옳았다는 증거, ② 선행 자산(UPC/Chapel/release consistency) 위에서 시작, ③ CXL cache-coherent 전제가 전통 PGAS와 달라 다시 열린 문제.

## 적합성 (왜 내가)
- PGAS = **언어/컴파일러 + 메모리 시스템 + 분산**의 교집합.
  - 언어/컴파일러 = KECC(강지훈 교수 수업) → PGAS 언어 설계에 직접.
  - 메모리 시스템 = CXL(정명수 교수 랩).
  - → **두 롤모델(강지훈=컴파일러 / 정명수=메모리)의 교집합에 PGAS.** + ML 배경 = "ML 워크로드용 CXL-PGAS"라는 조합.

## 검증 계획 (다음에 팔 것)
- [ ] **PGAS 학습**(재발명 말고): UPC, Chapel, release consistency, one-sided communication.
- [ ] **"CXL 위 PGAS" 선행연구 탐색** — 이미 하는 사람 있는지, 있으면 뭘 하는지. (⚠️ 기여 지점 존재 여부의 핵심 관문)
- [ ] 접근 A 차별점: prefetching / page migration / CXL tiering 논문과 내 아이디어의 차이 명확화.
- [ ] 세미나 질문: *"기존 PGAS는 message-passing 기반인데, CXL의 cache-coherent 특성을 살려 PGAS를 재설계하는 연구가 있나요?"*

## 세미나/면담 대화 카드
1. "profiling으로 coherence를 최소화하는 데이터 배치를 FPGA로 하고 싶은데, 기존 prefetching/migration과 어떻게 다를 수 있을까요?"
2. "제 아이디어가 PGAS와 닿아 있던데, CXL의 cache-coherent 특성 위에서 PGAS를 재설계하는 방향이 있을까요?"
3. (강점) "언어/컴파일러(KECC) + 메모리 시스템이 만나는 PGAS 쪽에 관심이 있습니다."

---
**관련**: [[H1 — 워크로드 특화로 multi-node coherence 줄이기]] · [[PGAS]] · [[CXL Multi-node Coherence]] · [[TrainingCXL]] · [[Communication Tax]]
