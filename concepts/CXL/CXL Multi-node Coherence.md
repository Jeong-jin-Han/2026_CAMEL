---
title: "CXL Multi-node Coherence — directory 난관과 NVLink 비교"
aliases: [CXL Multi-node Coherence, multi-node coherence, directory, NVLink vs CXL, NVLink, tiered memory]
type: concept
tags:
  - concept
  - concept/cxl
  - topic/cxl
---

# CXL Multi-node Coherence — 노드를 넘으면 무엇이 깨지나

> [!abstract] 이 노트
> single-node coherence([[CXL Coherence]])를 **여러 노드(pooling)로 확장**할 때 생기는 난관과, 같은 문제를 NVLink가 어떻게 다르게 푸는지. 변환 분산은 [[CXL Distributed Translation]].

## 배경 — 메모리 계층(tiered memory)
```
L1/L2/L3       (빠름·작음)  ── cache of ──→ local DRAM
local DRAM     ── (tiered, 캐시처럼) ──→ CXL memory pool (느림·큼)
```
- 위층이 아래층의 "빠른 사본 보관소". **local DRAM을 CXL pool의 캐시처럼** 쓰는 연구가 활발(tiered memory). → pool을 노드들이 공유하면 곧 multi-node coherence 문제로 이어짐.

## single-node directory를 그대로 확장하면 깨지는 4지점
single-node의 snoop + directory 방식을 multi-node로 확장하는 게 학계 1순위 접근이고, CXL 3.0도 directory + **Back-Invalidate**, fabric **Snoop Filter**로 실제 그렇게 한다. **단 그대로는 깨진다:**

1. **snoop(broadcast) 불가** — 노드가 멀고 많아 트래픽 폭발 → directory 의존 불가피.
2. **directory 크기 폭발** — host 수·메모리(100TB+)에 비례해 추적 정보가 메모리를 잠식 → **sparse/계층적 directory** 필요.
3. **directory 위치** — host/device/스위치 중 어디 둘까? 중앙집중이면 병목.
4. **latency(거리)** — 노드 간 무효화 왕복이 수백 ns~µs → **relaxed/선택적 coherence**로 우회.

## 완화 방향 + 최신 연구 (2025~2026)
- **CtXnL** (arXiv:2502.11046): "strict coherence는 overkill" → 선택적·hybrid coherence (OLTP 2.08× throughput).
- **Cohet** (arXiv:2511.23011): **계층적 coherence** — local agent 먼저, 없을 때만 global agent.
- **Tigon** (Huang 2025): HW coherence가 규모·용량에서 제한적임을 보임.
- **CCCL** (arXiv:2602.22457): CXL 풀로 노드 넘는 GPU collective (TITAN-II 스위치 + Micron CZ120 실측).
- 스위치 풀 scaling 한계: 포트 수, directory 크기, 버퍼, 핫스팟 큐잉.

## coherence만 비싼 게 아니다 — synchronization도 (TM 사례)
coherence(값 일관성, HW 자동)와 별개로 **synchronization**(순서·원자성, SW 명시: lock/atomic)도 multi-node에서 비싸진다. (구분은 [[CXL Coherence]])
- **Transactional Memory(TM)**: lock 대신 일련의 작업을 트랜잭션으로 묶어 — 충돌 없으면 통과, 충돌나면 **롤백**. 낙관적(optimistic)이라 *충돌이 드물 때* 빠름.
- **single→multi-node에서 TM이 불리**: 노드 수십~수천 × 멀티코어 = 동시 접근 주체 폭증 → 같은 DPA에 몰림 → 충돌↑ → **abort/retry 폭발**(TM의 아킬레스건).
- write 시 coherence 전파도 거리×개수로 폭발 — write-invalidate(신호만)라도 멀리 여러 host엔 비쌈.

## 워크로드 특화로 줄이기 (연구 방향)
범용 coherence를 강하게 유지하면 비용이 폭발하니, **용도를 좁혀(workload-specialized) 안 쓰는 coherence를 버리는** 접근이 활발하다.
- **CUDA 비유**: GPU 성공 공식 = "아무거나"가 아니라 "병렬 패턴에 특화"(대가: 프로그래머가 명시). CXL도 "거대 모델 메모리 패턴에 특화"하면 불필요한 coherence를 안 함.
- 근거: CtXnL("strict는 overkill"→선택적), TrainingCXL(ML 학습 특성 알면 relaxed). **특화 TM**도 "충돌 드문 패턴"을 알면 롤백 거의 없어 살아남음.
- **composable infra**(CXL 3.0 port-based routing / fabric 재구성)가 "용도에 맞춰 메모리-컴퓨트 연결을 동적 변경"을 실제로 가능케 함(Panmnesia switch).
- ★ **open design question = 특화/범용 경계를 어디에.** 너무 특화면 그 워크로드 밖에서 못 쓰고, 너무 범용이면 안 빨라짐 — 그 경계 + "누가 coherence를 명시하나(API/컴파일러/HW 힌트)"가 연구 지점.

## NVLink는 이 문제를 어떻게 푸나 (비교)
- 메커니즘: **Unified Virtual Address**(주소 매핑) + **NVSwitch**(스위치 라우팅) + cache coherence + flat address space — multi-node 스위치 층의 실제 사례.
- **replication**: NVLink SHARP가 multicast object를 N개 GPU에 N replica.
- **핵심 전략 = 강결합으로 회피**: NVL72가 72 GPU를 한 랙에 빽빽이 둬서 *거리 latency·directory 난관을 애초에 가까이 둬서 회피*.

| | NVLink | CXL |
|---|---|---|
| 개방성 | 폐쇄(NVIDIA 전용) | 개방 표준 |
| 대상 | GPU-GPU(+CPU) | 범용 메모리/스토리지/가속기 |
| 전략 | **강결합으로 거리 회피** | **분산(disaggregation) 정면돌파** |

→ **NVLink가 "가까이 묶어 회피"한 문제를, CXL은 "멀리 분산하며 풀어야" 한다.** 이게 세미나 단골 질문 *"왜 NVLink로 안 되고 CXL인가"*의 답이자, 현재 연구 frontier(open problem)가 있는 지점.

## 왜 multi-node가 필요한가 — "GPU 그냥 붙이면 안 되나?"
GPU를 NVLink로 붙이면 되지 왜 복잡한 multi-node? → 두 벽이 답.

**NVLink GPU 수는 상한이 있다**: HGX H100/H200 = 단일 NVLink 도메인 **8 GPU**(과거) → GB200 **NVL72**(2025) = 72 GPU → NVLink Switch 최대 **576 GPU**(non-blocking fabric). 늘었지만 **무한이 아님.**

**벽 1 — 규모**: 72(~576) 초과는 NVLink 불가 → InfiniBand/Ethernet으로 노드 연결 = **multi-node 필수**. (MS의 4,608-GPU 클러스터도 NVL72 랙 *사이*는 Quantum InfiniBand.) 거대 모델(GPT급)은 수천~수만 GPU → 72개로 안 끝남.
```
~72 GPU:  NVLink 한 도메인 = single-node처럼 (빠름)
72 초과:  여러 노드 + InfiniBand = multi-node (노드 내 빠름, 노드 간 느림 = 병목)
```

**벽 2 — 묶임 (연산-메모리 결합)**: "GPU 붙이기"는 연산과 메모리가 *함께* 늘어남 → 메모리만 필요해도 비싼 GPU를 통째로 사야 함. **CXL = 메모리와 연산 분리(disaggregation)** → 필요한 것만 늘림.
```
NVLink: 메모리 더 필요 → GPU 통째 추가 (연산 강제로 딸려옴, 비쌈)
CXL:    메모리 더 필요 → 메모리만 추가 (쌈)
```

→ NVIDIA도 **multi-node NVLink(IMEX)**를 한다 = 노드 넘는 메모리 공유의 필요성을 NVIDIA 스스로 증명. 차이는 폐쇄(NVLink) vs 개방(CXL). 세미나 질문 *"GPU 그냥 붙이면 안 되나?"*의 답 = **"72개에서 막히고(규모), 붙여도 메모리만 못 늘려서(묶임), multi-node CXL memory가 필요."**

## 한 줄 요약
> multi-node로 가면 single-node directory가 4지점(snoop 불가·크기 폭발·위치·거리)에서 깨짐 → sparse/계층 directory + relaxed/선택적 coherence로 완화(CtXnL·Cohet·CCCL). NVLink는 강결합(NVL72)으로 거리를 회피, CXL은 분산으로 정면돌파 — 거기에 frontier가 있다.

---
**관련**: [[CXL Coherence]] · [[CXL Distributed Translation]] · [[CXL Address Translation]] · [[CXL 3.0]] · [[CXL Overview]]
