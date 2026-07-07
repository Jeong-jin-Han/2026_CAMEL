---
title: "Prefill vs Decode — LLM Inference의 두 단계"
aliases: [Prefill, Decode, Prefill vs Decode, TTFT, TPOT]
type: concept
tags:
  - concept
  - concept/llm
  - topic/llm
  - topic/llm-serving
  - topic/kv-cache
---

# Prefill vs Decode — LLM Inference의 두 단계

> [!abstract] 이 노트는 뭐지?
> LLM inference를 이해하려면 가장 먼저 짚어야 하는 이분법. 논문(`papers/`, `concepts/LLM/*`)이 아니라 **배경지식**(`concepts/`)이다. 이 구분을 모르면 KV cache·speculative decoding·disaggregated serving 관련 논문(예: [[Mooncake]], [[SwiftSpec]])이 왜 그렇게 설계됐는지 이해가 안 된다.

## 한 문장
LLM inference는 **prefill**(prompt 전체를 한 번에 처리하는 병렬·compute-bound 단계)과 **decode**(토큰을 하나씩 순차 생성하는 memory-bound 단계)라는 성격이 정반대인 두 단계로 이뤄진다.

## 두 단계 비교
| | Prefill | Decode |
|---|---|---|
| 입력 | 프롬프트 전체 토큰 (예: 1000개) | 직전에 생성된 토큰 1개 |
| 처리 방식 | 모든 입력 토큰을 **한 번의 forward pass**로 병렬 처리 | 토큰 1개씩 **순차 반복**(autoregressive) |
| 병목 | **compute-bound** — 행렬곱(GEMM) 연산량이 큼 | **memory-bandwidth-bound** — 매 스텝 KV cache 전체를 HBM에서 읽어야 함 |
| 배치 효율 | 토큰 수가 많아 GPU 연산 유닛을 잘 채움 (연산 활용률 높음) | 토큰이 1개뿐이라 GPU가 대부분 놀고, 데이터 이동에 시간이 감 |
| 대표 지표 | **TTFT** (Time To First Token) | **TPOT** (Time Per Output Token), 또는 inter-token latency |
| 상태 | 이 단계에서 KV cache를 **처음 채움** | 매 스텝 KV cache에 **한 줄씩 추가**하며 전체를 다시 읽음 |
| 늘어나는 비용의 형태 | 프롬프트 길이에 비례해 연산량 증가 (attention은 길이의 제곱) | 생성 길이가 늘수록 **읽어야 할 KV cache가 계속 커짐** → 스텝마다 점점 느려짐 |

## 왜 decode가 memory-bound인가 — KV Cache
Transformer의 self-attention은 이전에 나온 모든 토큰의 key/value 벡터가 필요하다. 매번 다시 계산하면 낭비이므로, 이미 계산한 key/value를 **KV cache**에 저장해두고 재사용한다.

- Decode 스텝마다: 새 토큰 1개의 Q를 만들고, **지금까지 쌓인 KV cache 전체**를 HBM에서 읽어와 attention을 계산한다.
- 문제: 이때 실제 연산량(FLOPs)은 아주 작은데(토큰 1개), **읽어야 할 데이터량(KV cache 크기)**은 시퀀스 길이·레이어 수·헤드 수에 비례해 계속 커진다. → GPU 연산 유닛은 놀고, 데이터를 HBM에서 퍼오는 대역폭이 병목이 된다.
- 그래서 KV cache 압축·양자화·eviction·offloading(→ [[KV Cache Optimization Strategies for Scalable and Efficient LLM Inference]], [[Keep the Cost Down - A Review on Methods to Optimize LLM's KV Cache Consumption]])이나, 애초에 KV cache를 덜 만드는 구조 변경(MQA/GQA/MLA)이 decode 속도를 좌우하는 핵심 레버가 된다.
- [[Efficiently Scaling Transformer Inference]]는 이 문제를 정면으로 다뤄서, multiquery attention을 **배치 축으로 샤딩**해 칩당 KV cache 메모리 트래픽을 칩 수만큼 줄이는 전략을 제안한다.

## 왜 이 구분이 시스템 설계를 좌우하는가
- **다른 최적화 축**: prefill은 "연산을 어떻게 병렬화하나"의 문제, decode는 "메모리 접근을 어떻게 줄이나"의 문제. 같은 GPU 설정으로 둘 다 최적화하기 어렵다.
- **Disaggregated serving**: 이 비대칭성 때문에 아예 prefill 전담 GPU 그룹과 decode 전담 GPU 그룹을 분리하는 아키텍처가 등장했다 — [[Mooncake]](KVCache-centric, prefill/decode 분리 + 클러스터 유휴 자원으로 분산 KV cache pool 구성)가 대표적. [[SwiftSpec]]도 비슷한 문제의식(decode 단계 안에서 draft/target GPU 그룹을 아예 분리)을 공유하지만, SwiftSpec은 **prefill-decode 분리와는 orthogonal**하다고 스스로 밝힌다 — SwiftSpec은 decode "안"의 draft/target을 분리하는 것이지 prefill과 decode를 분리하는 게 아니기 때문.
- **Chunked prefill / continuous batching**: 긴 prompt의 prefill이 decode 요청들의 latency를 막지 않도록 prefill을 청크로 쪼개 decode와 인터리빙하는 스케줄링 기법도 이 비대칭성에서 나온 것.
- **Speculative decoding**([[SwiftSpec]] 등)은 이름 그대로 decode 단계의 memory-bound 특성(토큰 1개씩 순차 생성)을 우회하려는 시도 — 작은 draft 모델이 여러 토큰을 미리 만들고 큰 target 모델이 한 번에(=prefill처럼 병렬로) 검증해서, decode를 부분적으로 prefill의 병렬성으로 바꾼다.

## 우리 위키와의 연결
- KV cache 관리·최적화: [[KV Cache Optimization Strategies for Scalable and Efficient LLM Inference]], [[Keep the Cost Down - A Review on Methods to Optimize LLM's KV Cache Consumption]]
- Partitioning·저지연 서빙 이론: [[Efficiently Scaling Transformer Inference]]
- Prefill/Decode 분리 아키텍처: [[Mooncake]]
- Decode 단계 자체를 재설계: [[SwiftSpec]] (draft/target 비동기 분리, KV cache consistency)
- 상위 폴더: [[LLM Training Overview]] (같은 모델의 또 다른 실행 모드인 training과의 대비)

## Personal annotations
<본인 메모 영역>
