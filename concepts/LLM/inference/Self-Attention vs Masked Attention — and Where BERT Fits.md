---
title: "Self-Attention vs Masked Attention — 그리고 BERT는 어디에 속하는가"
aliases: [Self-Attention, Masked Attention, Bidirectional Attention, MLM, Masked Language Modeling, BERT, Cross-Attention]
type: concept
tags:
  - concept
  - concept/llm
  - topic/llm
  - topic/attention
---

# Self-Attention vs Masked Attention — 그리고 BERT는 어디에 속하는가

> [!abstract] 이 노트는 뭐지?
> "Self-attention"과 "masked attention"은 자주 같이 언급되지만 사실 **서로 다른 질문에 답하는 독립적인 두 축**이다. 이걸 하나의 축처럼 섞어 이해하면 "BERT도 masking을 쓴다는데 GPT랑 뭐가 다르지?" 같은 혼란이 생긴다. [[Encoder-Decoder vs Decoder-Only Attention]]이 아키텍처 구조(attention 횟수)의 구분이라면, 이 노트는 attention **연산 자체의 분류법**이다.

## 한 문장
**Self-attention**(Query/Key/Value가 같은 시퀀스에서 나오는가, cross-attention과 대비되는 축)과 **masked attention**(어떤 위치를 못 보게 가리는가, unmasked/bidirectional과 대비되는 축)은 독립적으로 조합된다 — GPT는 **masked self-attention**, BERT는 **unmasked(bidirectional) self-attention**을 쓴다. 그리고 "BERT가 masking을 쓴다"고 할 때의 mask는 attention mask가 아니라 완전히 다른 개념인 **입력 토큰 자체를 가리는 `[MASK]` 토큰(MLM 학습 목표)**을 가리킨다 — 이름만 같은 별개의 개념이다.

## 두 개의 독립적인 축

**축 1 — Query/Key/Value가 어디서 오는가 (self vs cross)**

| | Query 출처 | Key/Value 출처 |
|---|---|---|
| **Self-attention** | 자기 자신의 시퀀스 | 자기 자신의 시퀀스 (Query와 동일 소스) |
| **Cross-attention** | 자기 자신의 시퀀스 | **다른** 시퀀스 (예: [[Encoder-Decoder vs Decoder-Only Attention\|encoder-decoder attention]]에서 Key/Value는 Encoder 출력) |

**축 2 — 어떤 위치를 볼 수 있는가 (masked vs unmasked)**

| | 설명 |
|---|---|
| **Masked (causal)** | 자기보다 미래 위치는 못 봄 — score 행렬의 미래 위치를 $-\infty$로 채운 뒤 softmax를 취해 사실상 0으로 만듦 |
| **Unmasked (bidirectional / full)** | 모든 위치를 다 볼 수 있음 — 제약 없음 |

이 두 축을 조합하면 실제로 쓰이는 세 가지 조합이 나온다:

| 조합 | 어디서 쓰이나 |
|---|---|
| **Masked + Self** | GPT류 decoder의 self-attention (→ [[Encoder-Decoder vs Decoder-Only Attention]]) |
| **Unmasked + Self** | **BERT**, 원조 Transformer의 **Encoder** |
| **Unmasked + Cross** | 원조 Transformer의 encoder-decoder attention (Encoder 출력은 이미 다 봐도 되는 정보) |
| (Masked + Cross) | 거의 안 씀 — 스트리밍 seq2seq 같은 특수 상황에서만 등장 |

**핵심**: "masked냐 아니냐"와 "self냐 cross냐"는 서로 무관한 질문이다. masked self-attention(GPT)과 unmasked self-attention(BERT)은 둘 다 "self"라는 공통점이 있을 뿐, mask 유무는 완전히 별개로 결정된다.

## BERT는 어떤 attention을 쓰는가
**BERT = Encoder만 있는 모델**이다(**B**idirectional **E**ncoder **R**epresentations from **T**ransformers, 이름 그대로). 원조 Transformer의 Encoder 구조를 그대로 쌓아올렸기 때문에, self-attention을 쓰되 **mask가 없다(unmasked/bidirectional)**. 모든 토큰이 자기 왼쪽·오른쪽 토큰을 전부 보고 문맥을 만든다 — 그래서 이름에 "Bidirectional"이 들어간다.

이게 GPT와의 결정적 차이다:

| | BERT (Encoder-only) | GPT (Decoder-only) |
|---|---|---|
| Self-attention 종류 | **Unmasked** (bidirectional) | **Masked** (causal) |
| 볼 수 있는 범위 | 문장 전체(좌우 양방향) | 자기 자신 이전(왼쪽)만 |
| 적합한 과제 | **이해(understanding)** — 분류, 개체명 인식, 문장 유사도 | **생성(generation)** — 다음 토큰 예측 |
| 텍스트 생성 가능? | **불가능** — 미래를 이미 보고 있어서 순차 생성이라는 개념 자체가 안 맞음 | 가능 — 미래를 안 보므로 한 토큰씩 순차 생성 가능 |
| KV cache 재사용? | **불가능** — [[Prefill vs Decode#Causal Mask가 KV Cache를 가능하게 한다\|causal mask의 invariance]]가 없어서 매번 문장 전체를 다시 봐야 함 | 가능 — 이게 KV cache가 성립하는 이유 |

## "BERT가 마스킹을 쓴다"는 말의 함정 — `[MASK]`는 완전히 다른 개념
여기서 결정적인 용어 함정이 있다. **BERT도 분명히 "masking"을 쓰는데, 그건 attention mask가 아니라 완전히 다른 층위의 masking이다.**

- **Attention mask**(위에서 설명한 것): attention **score 행렬**에서 어떤 (query, key) 위치 쌍을 못 보게 하는 **계산 메커니즘**.
- **`[MASK]` 토큰 / Masked Language Modeling(MLM)**: BERT의 **학습 목표(pretraining objective)**. 입력 문장의 토큰 중 약 **15%**를 통째로 `[MASK]`라는 특수 토큰으로 가리고("나는 `[MASK]`을 먹었다"), 모델이 그 자리에 원래 뭐가 있었는지("밥")를 **양방향 문맥으로** 맞히도록 학습시킨다. 빈칸 채우기(cloze task)와 같은 구조다.

두 masking은 이름만 같을 뿐 완전히 다른 층위에서 일어난다:

| | Attention mask (causal) | `[MASK]` 토큰 (MLM) |
|---|---|---|
| 무엇을 가리나 | attention 계산에서 특정 **위치 쌍** | 입력 시퀀스의 특정 **토큰 자체** |
| 언제 적용되나 | 매 forward pass의 attention 연산 안 | 학습 데이터를 만들 때(전처리 단계) |
| 목적 | 미래 컨닝 방지 → 순차 생성을 가능하게 함 | 빈칸 채우기로 양방향 문맥 표현을 학습시킴 |
| BERT가 씀? | **안 씀** (bidirectional) | **씀** (핵심 pretraining task) |
| GPT가 씀? | **씀** (causal) | **안 씀** (다음 토큰 예측만 함) |

그래서 "BERT는 masking을 쓴다"와 "GPT는 masking을 쓴다"는 문장이 **둘 다 참**인데, 가리키는 대상이 **서로 완전히 다른 동음이의어**다 — 이게 이 영역이 헷갈리는 핵심 원인이다. (참고로 BERT는 MLM 외에 문장 두 개가 이어지는 문장인지 맞히는 **Next Sentence Prediction(NSP)**도 보조 목표로 썼는데, 후속 연구(RoBERTa 등)에서 NSP의 기여가 크지 않다는 게 밝혀져 이후 모델들은 대부분 뺐다.)

## 우리 위키와의 연결
- Encoder/Decoder 구조 자체(attention 횟수·구조): [[Encoder-Decoder vs Decoder-Only Attention]]
- Causal mask와 KV cache의 관계: [[Prefill vs Decode]]
- Training이 decode 루프 없이 한 번에 끝나는 이유(teacher forcing): [[LLM Training Overview#Training의 "Decode"는 없다 — Teacher Forcing]]

## Personal annotations
<본인 메모 영역>
