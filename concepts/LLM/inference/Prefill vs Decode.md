---
title: "Prefill vs Decode — LLM Inference의 두 단계"
aliases: [Prefill, Decode, Prefill vs Decode, TTFT, TPOT, GEMM, GEMV, Arithmetic Intensity]
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

## GEMM vs GEMV — compute-bound·memory-bound의 근본 원인
표에 나온 "병목" 차이는 attention뿐 아니라 Q/K/V/O projection·FFN 같은 **모든 weight 행렬 연산에 공통으로 적용되는 더 근본적인 이유**에서 나온다 — 한 번에 처리하는 토큰 수(batch dimension)에 따라 연산이 **GEMM**이 되느냐 **GEMV**가 되느냐가 갈린다.

- **GEMM(General Matrix-Matrix Multiplication)**: $C_{(M\times N)} = A_{(M\times K)} \times B_{(K\times N)}$. Prefill은 여러 토큰을 한 번에 행렬로 밀어 넣으므로 이 형태.
- **GEMV(General Matrix-Vector Multiplication)**: $y_{(M\times 1)} = A_{(M\times K)} \times x_{(K\times 1)}$. Decode는 토큰이 딱 1개(=벡터)이므로, $N=1$인 GEMM의 특수한 경우인 GEMV가 된다.

이 차이가 결정적인 이유는 **연산 강도(arithmetic intensity, FLOPs/Byte)** 때문이다.

$$\text{Arithmetic Intensity} = \frac{\text{FLOPs}}{\text{Bytes moved from memory}}$$

- **GEMM**: $M, N$이 커질수록 FLOPs($\approx 2MNK$)가 Bytes($\approx (MK+KN+MN)$)보다 훨씬 빠르게 늘어난다 — 한 번 읽어온 weight 행렬 $B$를 $M$개 토큰 전부에 대해 재사용(reuse)하기 때문이다. 그래서 토큰 수(배치)가 커지면 연산 강도가 높아져 **compute-bound**로 넘어간다.
- **GEMV**: $N=1$이라 FLOPs($\approx 2MK$)와 Bytes(주로 weight 행렬 $A$를 읽는 비용, $\approx MK$)가 같은 속도로 늘어나 연산 강도가 **항상 상수(약 2)로 고정**된다. weight 행렬을 아무리 키우거나 더 빠른 GPU를 써도 이 비율 자체는 안 바뀐다 — **그래서 decode는 태생적으로, 영구적으로 memory-bound다.**

| | Prefill | Decode |
|---|---|---|
| 연산 형태 | **GEMM** ($M$개 토큰 동시 처리) | **GEMV** ($M=1$, 토큰 1개) |
| Arithmetic Intensity | 토큰 수(배치)에 비례해 증가 | **고정**(토큰 수와 무관하게 상수) |
| 토큰/배치를 늘리면? | reuse가 늘어 더 compute-bound에 가까워짐 | 여러 decode 요청을 배치로 묶어야 GEMV들이 다시 GEMM처럼 합쳐짐(→ continuous batching이 이걸 가능하게 함, [[#Static Batching의 한계와 돌파구]]) |

> weight projection(Q/K/V/O, FFN)은 배치를 키우면 이렇게 GEMV→GEMM으로 승격되지만, **attention 자체(각 요청이 자기 KV cache를 읽는 부분)는 요청마다 KV가 달라서 배치를 키워도 여전히 GEMV로 남는다** — 그래서 KV cache 압축·양자화가 별도로 중요한 최적화 축이 된다(바로 아래 절).

## 왜 decode가 memory-bound인가 — KV Cache
Transformer의 self-attention은 이전에 나온 모든 토큰의 key/value 벡터가 필요하다. 매번 다시 계산하면 낭비이므로, 이미 계산한 key/value를 **KV cache**에 저장해두고 재사용한다.

- Decode 스텝마다: 새 토큰 1개의 Q를 만들고, **지금까지 쌓인 KV cache 전체**를 HBM에서 읽어와 attention을 계산한다.
- 문제: 이때 실제 연산량(FLOPs)은 아주 작은데(토큰 1개), **읽어야 할 데이터량(KV cache 크기)**은 시퀀스 길이·레이어 수·헤드 수에 비례해 계속 커진다. → GPU 연산 유닛은 놀고, 데이터를 HBM에서 퍼오는 대역폭이 병목이 된다.
- 그래서 KV cache 압축·양자화·eviction·offloading(→ [[KV Cache Optimization Strategies for Scalable and Efficient LLM Inference]], [[Keep the Cost Down - A Review on Methods to Optimize LLM's KV Cache Consumption]])이나, 애초에 KV cache를 덜 만드는 구조 변경(MQA/GQA/MLA)이 decode 속도를 좌우하는 핵심 레버가 된다.
- [[Efficiently Scaling Transformer Inference]]는 이 문제를 정면으로 다뤄서, multiquery attention을 **배치 축으로 샤딩**해 칩당 KV cache 메모리 트래픽을 칩 수만큼 줄이는 전략을 제안한다.

## Causal Mask가 KV Cache를 가능하게 한다
KV cache가 "그냥 최적화 트릭"이 아니라 **causal mask(인과적 마스크) 덕분에 수학적으로 성립하는** 기법이라는 점이 핵심이다.

- **Causal mask의 역할**: autoregressive 생성에서 현재 토큰이 미래 토큰을 못 보게 가린다. 그 결과 **이미 나온 토큰들끼리 주고받은 attention 결과는, 뒤에 어떤 토큰이 새로 추가되든 절대 변하지 않는다(invariant)**.
- **KV cache의 아이디어**: 과거 계산 결과가 안 변한다면 매번 처음부터 다시 계산할 이유가 없다. 새 토큰의 K, V만 계산해서 캐시 뒤에 이어 붙이면(append) 된다.
- **만약 causal mask가 없다면**(BERT류 양방향 모델): 새 토큰이 들어올 때마다 과거 토큰들의 attention 결과 자체가 그 새 토큰의 영향을 받아 바뀌어야 한다 — 캐시해둔 값이 곧 stale해지므로 KV cache라는 최적화 기법 자체가 성립하지 않는다.
- 따라서 **causal mask = 캐시가 안전하다는 것을 보장하는 규칙**, **KV cache = 그 규칙 위에서만 가능한 메모리 최적화**라는 관계다.

## Decode 스텝의 실제 연산: Vector-Matrix
Causal mask는 보통 **하삼각행렬(lower triangular matrix)**로 배운다 — 하지만 이건 prefill/학습(training)처럼 여러 토큰을 **행렬로 한 번에** 처리할 때 필요한 개념적 정의다. Decode 스텝은 애초에 입력이 새 토큰 1개(벡터)뿐이라 미래 토큰이 물리적으로 존재하지 않으므로, 마스크 행렬 자체를 쓰지 않는 훨씬 가벼운 연산으로 최적화된다. (은닉 차원 $d$, 지금까지 누적된 토큰 수 $N$)

$$Score = Q_{current} \times (K_{cache})^T \quad\Rightarrow\quad (1\times d)\times(d\times N) = 1\times N$$

$$Output = \text{Softmax}(Score) \times V_{cache} \quad\Rightarrow\quad (1\times N)\times(N\times d) = 1\times d$$

즉 새 토큰 1개의 **Query 벡터**($1\times d$)와 지금까지 쌓인 **KV cache 행렬**($N\times d$)의 곱으로 끝난다 — $N$이 시퀀스 길이만큼 계속 커지는 게 decode 스텝이 뒤로 갈수록 느려지는(memory-bound가 심해지는) 이유이기도 하다.

> 정리: "Causal mask = 하삼각행렬"이라는 설명은 **prefill/학습 시점의 병렬 행렬 연산**을 위한 개념적 정의고, **decode 시점의 실제 구현**은 마스크 없는 vector-matrix 연산이다. 이 둘을 안 나누면 "그럼 마스크가 왜 필요하지?"라는 혼란이 생긴다.

## 왜 이 구분이 시스템 설계를 좌우하는가
- **다른 최적화 축**: prefill은 "연산을 어떻게 병렬화하나"의 문제, decode는 "메모리 접근을 어떻게 줄이나"의 문제. 같은 GPU 설정으로 둘 다 최적화하기 어렵다.
- **Disaggregated serving**: 이 비대칭성 때문에 아예 prefill 전담 GPU 그룹과 decode 전담 GPU 그룹을 분리하는 아키텍처가 등장했다 — [[Mooncake]](KVCache-centric, prefill/decode 분리 + 클러스터 유휴 자원으로 분산 KV cache pool 구성)가 대표적. [[SwiftSpec]]도 비슷한 문제의식(decode 단계 안에서 draft/target GPU 그룹을 아예 분리)을 공유하지만, SwiftSpec은 **prefill-decode 분리와는 orthogonal**하다고 스스로 밝힌다 — SwiftSpec은 decode "안"의 draft/target을 분리하는 것이지 prefill과 decode를 분리하는 게 아니기 때문.
- **Speculative decoding**([[SwiftSpec]] 등)은 이름 그대로 decode 단계의 memory-bound 특성(토큰 1개씩 순차 생성)을 우회하려는 시도 — 작은 draft 모델이 여러 토큰을 미리 만들고 큰 target 모델이 한 번에(=prefill처럼 병렬로) 검증해서, decode를 부분적으로 prefill의 병렬성으로 바꾼다.

## Static Batching의 한계와 돌파구
LLM 서빙에서 여러 사용자의 요청을 딥러닝 학습 때처럼 그냥 행렬로 묶어 처리(**static batching**)하면 두 가지 병목이 생긴다.

| 문제 | 원인 |
|---|---|
| **패딩 오버헤드(padding overhead)** | 요청마다 프롬프트/생성 길이가 다른데, 배치 안에서 가장 긴 것에 맞춰 나머지를 의미 없는 `[PAD]` 토큰으로 채워야 함 — GPU 메모리·대역폭 낭비 |
| **조기 종료 지체(straggler effect)** | 배치 내 짧은 요청이 먼저 `[EOS]`로 끝나도, 가장 긴 요청이 끝날 때까지 배치를 깰 수 없어 GPU가 그 자리에 묶임 |

현대 서빙 엔진은 이 두 문제를 각각 다른 기법으로 푼다:
- **Continuous batching(in-flight batching)**: 매 decode 스텝(iteration)마다 끝난 요청은 즉시 배치에서 빼고 새 요청을 그 자리에 끼워 넣는 동적 스케줄링 — straggler effect를 해소하는 동시에, 여러 요청의 GEMV를 하나의 GEMM으로 묶어 weight projection의 연산 강도를 끌어올리는 효과도 있다(→ [[#GEMM vs GEMV — compute-bound·memory-bound의 근본 원인]]).
- **PagedAttention**(vLLM 핵심 기술): 가상 메모리의 페이징 기법을 KV cache에 그대로 적용해, 연속되지 않은 메모리 공간에 KV cache를 파편화해 저장 — 패딩 없이도 가변 길이를 다룰 수 있게 해 padding overhead를 해소.

## 우리 위키와의 연결
- KV cache 관리·최적화: [[KV Cache Optimization Strategies for Scalable and Efficient LLM Inference]], [[Keep the Cost Down - A Review on Methods to Optimize LLM's KV Cache Consumption]]
- Partitioning·저지연 서빙 이론: [[Efficiently Scaling Transformer Inference]]
- Prefill/Decode 분리 아키텍처: [[Mooncake]]
- Decode 단계 자체를 재설계: [[SwiftSpec]] (draft/target 비동기 분리, KV cache consistency)
- 아키텍처 구조 자체의 구분(Encoder-Decoder vs Decoder-Only): [[Encoder-Decoder vs Decoder-Only Attention]]
- 상위 폴더: [[LLM Training Overview]] (같은 모델의 또 다른 실행 모드인 training과의 대비)

## Personal annotations
<본인 메모 영역>
