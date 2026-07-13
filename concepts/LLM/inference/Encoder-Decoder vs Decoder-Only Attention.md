---
title: "Encoder-Decoder vs Decoder-Only — Attention이 한 번이냐 두 번이냐"
aliases: [Encoder-Decoder, Decoder-Only, Masked Self-Attention, Encoder-Decoder Attention, Cross-Attention]
type: concept
tags:
  - concept
  - concept/llm
  - topic/llm
  - topic/attention
---

# Encoder-Decoder vs Decoder-Only — Attention이 한 번이냐 두 번이냐

> [!abstract] 이 노트는 뭐지?
> "Transformer는 Masked Attention 다음에 Full Attention을 한다"는 흔한 설명은 사실 **오리지널 Transformer(Attention Is All You Need)의 Encoder-Decoder 구조** 얘기다. 지금 대부분의 LLM(GPT 계열)이 쓰는 **Decoder-Only** 구조에는 애초에 그 두 번째 attention이 없다. 이 둘을 안 나누고 배우면 "그럼 full attention은 언제 쓰이지?"라는 혼란이 생긴다. [[Prefill vs Decode]]가 *실행 시점*(prefill/decode)의 구분이라면, 이 노트는 *아키텍처 구조* 자체의 구분이다.

## 한 문장
오리지널 Transformer의 Decoder 블록은 매 스텝마다 **masked self-attention**(자기 자신이 생성한 것들끼리) 한 번과 **encoder-decoder attention**(고정된 encoder 출력을 바라보는 것) 한 번, 총 **두 번**의 attention을 하는 반면, GPT 계열 **decoder-only** 구조는 encoder가 아예 없어 self-attention **한 번**만 반복한다.

## 오리지널 Transformer (Encoder-Decoder, 번역기 구조)

### 실행 순서
1. **Encoder가 먼저 끝낸다**: 입력 문장 전체("나는 밥을 먹었다")를 마스크 없이 통째로 분석하는 **순수 Full Attention**을 수행해, 문맥 정보가 꽉 찬 **Context 행렬**(고정된 요약본)을 출력한다.
2. **Decoder의 Masked Self-Attention**: 자기가 지금까지 생성한 단어들(예: 영어 번역 중인 단어들)끼리, 미래 컨닝을 막는 마스크를 쓴 self-attention.
3. **Decoder의 Encoder-Decoder Attention** (= cross-attention): 2번에서 만든 Query를 들고, 1번에서 Encoder가 고정해둔 Key/Value(원문 요약본)를 바라본다. 원문은 이미 다 봐도 되는 정보라 여기서는 **Full Attention**이 일어난다 — "masked 다음에 full attention"이라는 기억은 바로 이 지점이다.

### 행렬 shape의 변화 (한국어 원문 길이 4, 지금 영어 10번째 단어 생성 중이라 가정)
| Attention | Query | Key/Value | 결과 shape | 스텝이 지날수록 |
|---|---|---|---|---|
| Masked Self-Attention | 새 영어 토큰 ($1\times d$) | 지금까지의 영어 KV cache ($d\times 10$) | $1\times 10$ | **길어짐** ($1\times 11, 1\times 12, ...$) |
| Encoder-Decoder Attention | 위에서 만든 Query ($1\times d$) | Encoder가 고정한 한국어 KV ($d\times 4$) | $1\times 4$ | **고정** (원문 길이는 안 변함) |

두 attention 모두 "새 토큰 1개 대 과거 전체"라는 점에서 [[Prefill vs Decode]]의 decode 스텝과 같은 vector-matrix 구조지만, 하나는 계속 자라는 KV(자기 자신의 과거)를 보고 다른 하나는 크기가 고정된 KV(상대방의 고정된 요약)를 본다는 점이 다르다.

## GPT 계열 (Decoder-Only)

Encoder 자체가 없다. Self-attention 하나만 반복하며, [[Prefill vs Decode]]에서 다룬 두 단계로 나뉜다.

- **학습(training) 시점**: 정답 문장 전체를 행렬로 한 번에 밀어 넣어 병렬 처리하므로, 미래 토큰 컨닝을 막는 **lower triangular mask가 필수**다.
- **Prefill 시점**: 학습과 마찬가지로 prompt 전체를 행렬로 병렬 처리 — 마스크를 씌운 **Full Attention**(matrix-matrix)이 일어난다.
- **Decode 시점**: 새 토큰 1개만 입력되므로 애초에 미래 토큰이 존재하지 않는다 — 마스크 행렬 자체가 필요 없는 **vector-matrix 연산**(→ [[Prefill vs Decode]]의 수식 참고).

즉 "Masked"라는 이름이 붙는 이유는 **학습과 prefill 단계의 병렬 행렬 연산에서 컨닝을 막기 위한 장치**이기 때문이지, decode 단계에서 실제로 마스크 행렬을 씌우는 연산을 하기 때문이 아니다.

## 두 구조 비교

| | Encoder-Decoder (오리지널 Transformer) | Decoder-Only (GPT 계열) |
|---|---|---|
| Attention 횟수(Decoder 블록당) | 2번 (masked self + encoder-decoder) | 1번 (self-attention만) |
| Full Attention이 일어나는 곳 | Encoder 전체 + Decoder의 encoder-decoder attention | Prefill 스텝(자기 자신의 prompt 내부에서만) |
| "미래를 안 보는" 대상 | 자기 자신이 생성 중인 문장만 (원문은 이미 다 봐도 됨) | 자기 자신이 생성 중인 전체 시퀀스(prompt+생성분) |
| 고정된 KV가 따로 있는가 | 있음 (encoder 출력, 한 번 구워지면 안 변함) | 없음 (KV cache 자체가 계속 자람) |
| 대표 예시 | 원조 Transformer, T5, 번역 모델 | GPT, LLaMA, 대부분의 현대 LLM |

## 세 번째 축: Encoder-Only(BERT)는 어디에 속하나
이 노트는 지금까지 "attention이 몇 번 일어나는가"(구조) 기준으로 Encoder-Decoder와 Decoder-Only만 다뤘다. 그런데 **Encoder만 있는 구조도 있다** — 바로 **BERT**다. BERT는 Decoder가 아예 없이 원조 Transformer의 **Encoder만 쌓아올린** 구조라, self-attention은 하지만 **mask가 없다(unmasked/bidirectional)**. GPT의 masked self-attention과 대비되는 지점인데, "self-attention이냐 아니냐"와 "mask가 있냐 없냐"가 왜 별개의 축인지, 그리고 BERT 학습에서 말하는 `[MASK]` 토큰이 여기서 말하는 attention mask와 왜 완전히 다른 개념인지는 별도로 깊게 다룬다: [[Self-Attention vs Masked Attention — and Where BERT Fits]].

## 우리 위키와의 연결
- 실행 시점(prefill/decode) 구분: [[Prefill vs Decode]]
- Attention의 분류법(self vs cross, masked vs unmasked)과 BERT: [[Self-Attention vs Masked Attention — and Where BERT Fits]]
- 원 논문: Vaswani et al., "Attention Is All You Need" (NeurIPS 2017)

## Personal annotations
<본인 메모 영역>
