---
title: "LLM Training Overview — Inference와 무엇이 다른가"
aliases: [LLM Training, Training Overview]
type: concept
tags:
  - concept
  - concept/llm
  - topic/llm
  - topic/llm-training
---

# LLM Training Overview — Inference와 무엇이 다른가

> [!abstract] 이 노트는 뭐지?
> LLM **training**의 실행 구조를 개념 차원에서 정리한다. 논문(`papers/`)이 아니라 **배경지식**(`concepts/`)이다. Inference 쪽 대응 개념은 [[Prefill vs Decode]] — 같은 Transformer 모델이 학습 때와 서빙 때 왜 이렇게 다른 메모리·연산 프로파일을 갖는지 대비해서 보면 이해가 빠르다.

## 한 문장
Training은 매 스텝마다 **forward pass + backward pass(역전파) + optimizer step(가중치 갱신)**을 반복하는 과정이며, inference의 forward-only 실행과 달리 **gradient와 optimizer state까지 GPU 메모리에 유지**해야 해서 메모리 요구량이 훨씬 크다.

## Training의 세 단계 (매 스텝 반복)
1. **Forward pass**: 입력 배치를 모델에 통과시켜 손실(loss)을 계산. Inference의 prefill과 연산 패턴은 비슷하지만(병렬 처리, compute-heavy), **나중에 backward에 쓸 activation을 버리지 않고 저장**해야 한다는 차이가 있다.
2. **Backward pass**: loss로부터 각 파라미터에 대한 gradient를 역전파로 계산. Forward 때 저장해둔 activation을 다시 사용하므로, activation 저장 여부가 메모리 사용량을 크게 좌우한다(→ activation checkpointing/recomputation은 이 메모리를 시간과 맞바꾸는 기법).
3. **Optimizer step**: 계산된 gradient로 옵티마이저(예: Adam)가 파라미터를 갱신. Adam은 파라미터마다 momentum·variance 두 개의 optimizer state를 추가로 유지하므로, 이 단계가 끝나야 다음 스텝의 forward로 넘어간다.

Inference는 이 중 **1번(forward)만, 그것도 activation을 저장하지 않고** 수행한다는 점에서 training과 근본적으로 다르다.

## 메모리 구성 요소 비교
| 구성 요소 | Training에 필요? | Inference에 필요? | 비고 |
|---|---|---|---|
| 모델 파라미터 (weights) | ✓ | ✓ | 공통 |
| Gradient | ✓ | ✗ | 파라미터와 같은 크기, backward에서만 생성 |
| Optimizer state | ✓ (예: Adam은 파라미터의 2배) | ✗ | training 메모리 폭증의 주범 |
| Activation | ✓ (backward를 위해 저장) | ✗ (prefill/decode 모두 즉시 버림) | activation checkpointing으로 일부만 저장 가능 |
| KV cache | 보통 불필요 | ✓ (decode에서 핵심, → [[Prefill vs Decode]]) | training은 매 스텝 전체 시퀀스를 다시 처리하므로 캐시 재사용 개념이 다름 |

→ 결과적으로 같은 파라미터 수의 모델이라도, **training은 순수 파라미터 크기의 여러 배(파라미터+gradient+optimizer state+activation)** 를 GPU 메모리에 올려야 한다. 이것이 "GPU 메모리가 모자라 여러 GPU/노드로 쪼갤 수밖에 없다"는 parallelism 논의의 출발점이다.

## Parallelism 전략 — 왜 필요하고 어떻게 나뉘는가
모델·옵티마이저 상태·activation을 한 GPU에 다 못 올리면, 계산과 데이터를 여러 장치로 쪼개야 한다. 이걸 이론적으로 총정리한 것이 [[Demystifying Parallel and Distributed Deep Learning - An In-Depth Concurrency Analysis]](Ben-Nun & Hoefler)이고, 실전 LLM 사례(Megatron, Gopher, PaLM, GPT)로 정리한 것이 [[Model Parallelism on Distributed Infrastructure - A Literature Review from Theory to LLM Case-Studies]]다.

| 전략 | 쪼개는 대상 | 특징 |
|---|---|---|
| **Data parallelism** | 배치(데이터) | 모델 전체를 각 GPU에 복제, gradient만 all-reduce로 동기화 |
| **Tensor(intra-operator) parallelism** | 한 연산(행렬곱) 내부 | 통신량이 크고 빈번 → 보통 NVLink로 강결합된 같은 노드 안에서만 사용 |
| **Pipeline(inter-operator) parallelism** | 레이어 단위 | 노드를 넘어 확장 가능하지만 **pipeline bubble**(유휴 시간) 발생 |
| **Hybrid** | 위 셋의 조합 | 실전 대규모 LLM(Megatron 등)은 거의 항상 세 가지를 함께 사용 |

이 표의 "통신량이 큰 것(tensor parallelism)은 강결합 인터커넥트 안에서만" 이라는 제약이, 왜 GPU 메모리가 하드웨어 인터커넥트(NVLink vs 이더넷, 나아가 CXL 같은 memory-semantic fabric)의 대역폭·지연에 근본적으로 종속되는지를 보여준다 — 내 CXL/memory-architecture 연구 관심사가 여기서 training parallelism 논의와 만난다.

## Storage-offloaded Training — 메모리가 정 부족할 때
GPU 메모리로도 안 되면, optimizer state·gradient를 **host memory나 SSD로 오프로드**하는 방법도 있다. [[Smart-Infinity - Fast Large Language Model Training using Near-Storage Processing on a Real System]]는 이 오프로드에서 생기는 interconnect(PCIe) 병목을, computational storage device(CSD) 내부 FPGA에서 파라미터 update를 직접 수행(SmartUpdate)해 우회하는 실제 시스템이다.

## 우리 위키와의 연결
- 이론 전반: [[Demystifying Parallel and Distributed Deep Learning - An In-Depth Concurrency Analysis]]
- LLM 실전 사례: [[Model Parallelism on Distributed Infrastructure - A Literature Review from Theory to LLM Case-Studies]]
- Storage-offloaded training: [[Smart-Infinity - Fast Large Language Model Training using Near-Storage Processing on a Real System]]
- Inference 쪽 대응 개념: [[Prefill vs Decode]]

## Personal annotations
<본인 메모 영역>
