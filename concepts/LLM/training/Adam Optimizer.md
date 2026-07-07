---
title: "Adam Optimizer — 1st/2nd Momentum과 왜 표준이 됐는가"
aliases: [Adam, AdamW, Adam Optimizer, 1st moment, 2nd moment, momentum]
type: concept
tags:
  - concept
  - concept/llm
  - topic/llm
  - topic/llm-training
---

# Adam Optimizer — 1st/2nd Momentum과 왜 표준이 됐는가

> [!abstract] 이 노트는 뭐지?
> [[LLM Training Overview]]에서 "optimizer state가 파라미터의 2배 메모리를 먹는다"고 했을 때 그 실체가 바로 Adam의 **1st moment(momentum)**와 **2nd moment(적응적 학습률)**다. 왜 두 개의 moving average를 따로 유지하는지, 각각 어떤 문제를 풀려고 태어났는지, 그리고 왜 지금도 LLM 학습의 사실상 표준인지를 계보(SGD → Momentum → AdaGrad/RMSProp → Adam → AdamW)로 정리한다.

## 한 문장
Adam은 그래디언트($g$)와 그 제곱($g^2$)에 **같은 메커니즘(지수 가중합)**을 각각 적용해 $m_t$(최근 방향의 가중평균)와 $v_t$(최근 크기의 가중평균)를 만들고, $m_t/\sqrt{v_t}$로 **그래디언트를 자기 자신의 최근 크기로 정규화**함으로써 momentum(방향 스무딩)과 파라미터별 적응적 학습률을 **한 번의 연산으로 동시에** 얻어내는 옵티마이저다.

> [!tip] 핵심은 "정규화" 하나다
> $m$과 $v$를 별개의 두 트릭으로 외우지 말 것. 그래디언트 $g$에 지수 가중합을 적용하면 "노이즈를 죽이고 history 기반 방향"($m$)이 나오고, $g^2$에 **같은** 지수 가중합을 적용하면 "최근 그래디언트 크기의 평균"($v$)이 나온다. 이 둘을 나누는 순간(**$m/\sqrt{v}$**), 파라미터마다 스케일이 자동으로 맞춰지는 정규화가 "덤으로" 따라온다 — momentum과 adaptive learning rate는 별개 기능이 아니라 **하나의 정규화 연산이 두 가지로 보이는 것**이다. 상세: [아래](#왜-두-개가-사실-하나의-메커니즘인가--정규화로-통합해서-보기).

## 출발점 — 순수 SGD의 두 가지 약점
$$\theta_{t} = \theta_{t-1} - \eta \, g_t$$
그래디언트 $g_t$ 방향으로 학습률 $\eta$만큼 매 스텝 이동하는 게 SGD의 전부다. 여기엔 두 가지 독립적인 문제가 있다.

1. **방향이 들쭉날쭉하다(진동)** — 손실 지형(loss landscape)이 한쪽으로 길쭉한 계곡(ravine) 모양이면, 그래디언트가 계곡 벽을 왔다갔다 튕기면서 정작 계곡을 따라 내려가는 속도는 느리다. 미니배치 노이즈까지 겹치면 매 스텝 방향이 크게 흔들린다.
2. **모든 파라미터가 같은 학습률을 공유한다** — 어떤 파라미터(예: 자주 등장하지 않는 토큰의 embedding)는 그래디언트가 가끔, 크게 오고, 어떤 파라미터(예: 자주 쓰이는 레이어)는 매번 작게 온다. 하나의 전역 학습률로는 이 둘을 동시에 잘 맞출 수 없다.

이 두 문제는 **서로 다른 해법**을 필요로 하고, 그 해법들이 각각 1st moment와 2nd moment로 발전했다.

## 1st Moment (Momentum) — "방향을 매끄럽게"
문제 1(진동)에 대한 답. 물리적 관성(momentum)에서 아이디어를 가져왔다 (Polyak, 1964의 heavy-ball method가 원조; Nesterov(1983)가 예측 지점에서 그래디언트를 미리 보는 변형을 제안).

$$m_t = \beta_1 m_{t-1} + (1-\beta_1) g_t$$

- 현재 그래디언트 $g_t$ 하나만 보는 게 아니라, **과거 그래디언트들의 지수 가중합**을 방향으로 쓴다. 재귀식을 풀어보면 이게 더 명확하다:
  $$m_t = (1-\beta_1)\sum_{i=1}^{t} \beta_1^{\,t-i}\, g_i$$
  즉 오늘 그래디언트엔 가중치 $(1-\beta_1)$, 어제 것엔 $(1-\beta_1)\beta_1$, 그저께 것엔 $(1-\beta_1)\beta_1^2$ ... 식으로 **지수적으로 attenuate(감쇠)된 가중치를 준 합**이다.
- 이 attenuation(감쇠) 자체가 "최근 것만 자동으로 반영되게" 만드는 장치다 — 슬라이딩 윈도우를 따로 관리할 필요 없이, 오래된 그래디언트는 가중치가 지수적으로 0에 가까워지면서 저절로 잊힌다.
- 그래서 $m_t$는 "오늘 하루치 그래디언트"가 아니라 **history 기반으로 가이드된 방향**이다. 일관되게 같은 방향을 가리키는 차원은 누적되어 **가속**되고, 매번 방향이 뒤집히는(진동하는) 차원은 서로 상쇄되어 **감쇠**된다.
- 효과: 계곡을 따라 내려가는 속도는 빨라지고, 벽에 튕기는 진동은 줄어든다. 미니배치 노이즈도 평균으로 완화된다.
- $\beta_1$(보통 0.9)은 "과거를 얼마나 오래 기억할지"를 정하는 감쇠율.

## 2nd Moment — "파라미터마다 학습률을 자동으로"
문제 2(파라미터별 스케일 차이)에 대한 답. 이 계보가 조금 더 복잡하다.

- **AdaGrad (Duchi et al., 2011)**: 그래디언트 제곱을 **처음부터 전부 누적**해서 학습률을 나눈다 — $\theta_t = \theta_{t-1} - \dfrac{\eta}{\sqrt{\sum_{i=1}^t g_i^2}+\epsilon} g_t$. 그래디언트가 자주·크게 온 파라미터는 학습률이 많이 깎이고, 드물게 온 파라미터(희소 그래디언트, 예: 임베딩)는 학습률이 거의 안 깎인다 — sparse 문제에 강함.
- **AdaGrad의 문제**: 분모가 **단조 증가만** 하므로, 학습이 길어지면 학습률이 0에 가깝게 죽어버려 더 이상 학습이 안 된다. 짧은 convex 문제엔 좋지만 긴 non-convex 학습(딥러닝)엔 치명적.
- **RMSProp (Hinton, 미출판 강의노트, 2012)**: 전체 누적 대신 **지수이동평균**으로 바꿔 이 문제를 해결.
  $$v_t = \beta_2 v_{t-1} + (1-\beta_2) g_t^2$$
  오래된 과거 그래디언트는 서서히 잊혀지므로 분모가 무한정 커지지 않는다 — 이게 바로 Adam의 2nd moment 식과 정확히 같은 형태다.

## Adam = 1st Moment + 2nd Moment + Bias Correction
Kingma & Ba (2014, ICLR 2015)가 momentum(1st moment)과 RMSProp(2nd moment)을 **하나로 결합**한 것이 Adam(**Ada**ptive **m**oment estimation)이다.

$$m_t = \beta_1 m_{t-1} + (1-\beta_1) g_t \qquad v_t = \beta_2 v_{t-1} + (1-\beta_2) g_t^2$$
$$\hat{m}_t = \frac{m_t}{1-\beta_1^t} \qquad \hat{v}_t = \frac{v_t}{1-\beta_2^t}$$
$$\theta_t = \theta_{t-1} - \eta \, \frac{\hat{m}_t}{\sqrt{\hat{v}_t}+\epsilon}$$

- 기본값: $\beta_1=0.9$, $\beta_2=0.999$, $\epsilon=10^{-8}$.
- **Bias correction의 진짜 정체 — "가중합"을 "가중평균"으로**: 위 $m_t$의 가중치들 $(1-\beta_1)\beta_1^{\,t-i}$ ($i=1,\dots,t$)을 전부 더하면 $1-\beta_1^t$이지, **1이 아니다**. 즉 $m_t, v_t$는 엄밀히 말하면 "가중평균"이 아니라 총 가중치가 $1-\beta^t$(< 1)인 **가중합**이라서, 학습 초반(t가 작을 때)엔 실제 값보다 작게 나온다. $(1-\beta^t)$로 나누는 것은 이 가중치 총합을 정확히 1로 재정규화해서 "가중합"을 진짜 "가중평균"으로 바꿔주는 것 — $t\to\infty$면 $\beta^t\to0$이라 보정이 사실상 사라진다.
- 결과적으로 $\hat{m}_t/\sqrt{\hat{v}_t}$ 하나가 momentum(방향 스무딩)과 파라미터별 스케일 정규화(adaptive LR)를 동시에 만든다 — 왜 이게 "두 기능"이 아니라 "한 메커니즘"인지는 다음 절 참고.

## 왜 두 개가 사실 하나의 메커니즘인가 — 정규화로 통합해서 보기
$m_t$와 $v_t$를 "momentum 트릭"과 "adaptive LR 트릭"이라는 서로 다른 두 발명품으로 외우면 왜 하필 이 둘을 묶었는지 이해가 안 된다. 사실은 **같은 연산(지수 가중합)을 다른 대상($g$ vs $g^2$)에 적용한 것**이고, 그 둘을 나누는 순간 정규화가 자동으로 따라온다.

- $m_t$ = 그래디언트($g$)의 지수 가중합 → "최근 방향의 신호(signal)"
- $v_t$ = 그래디언트 제곱($g^2$)의 지수 가중합 → "최근 크기의 평균 에너지(signal+noise)"
- $\hat m_t/\sqrt{\hat v_t}$ = 신호를 자기 자신의 에너지로 나눈 것 → 일종의 **signal-to-noise ratio**. 그래디언트가 꾸준히 한 방향(signal 강함)이면 비율이 커서 자신있게 이동하고, 방향이 들쭉날쭉(noise 지배적)하면 비율이 작아져(대략 $\pm1$ 근처로) 조심스럽게 이동한다.
- 이 나눗셈이 **동시에 두 가지를 공짜로** 해결한다: (1) 그래디언트가 원래 크던 작던 결과 스텝은 항상 비슷한 스케일이 되므로 파라미터마다 학습률을 따로 튜닝할 필요가 없어지고(= adaptive LR), (2) 그 스케일 정규화가 결과적으로 폭주도 막고 희소·미세 그래디언트도 상대적으로 죽지 않게 살려준다(= 양방향 정규화, "폭주 방지"는 그 절반일 뿐).

> [!warning] 딱 하나 정확히 짚을 부분 — variance가 아니라 RMS
> $v_t$는 그래디언트의 **분산(variance)**이 아니라 **제곱평균(uncentered 2nd moment, $E[g^2]$)**을 추정한다. $E[g^2] = \mathrm{Var}(g) + E[g]^2$라서, 그래디언트가 한쪽으로 꾸준히 편향돼 있으면(=$m$이 큰 상황) 그 크기까지 $v$에 그대로 반영된다. 그래서 엄밀한 통계적 "z-score 정규화"(평균을 빼고 표준편차로 나눔)는 아니고, **평균 제곱근(RMS) 크기로 정규화**하는 것에 더 가깝다 — 방향(신호)과 크기(에너지)를 같은 원재료 $g$에서 뽑아 나누는 것이므로, 순수 노이즈 대비 정규화라기보단 "그래디언트 자기 자신의 최근 스케일 대비 정규화"로 이해하는 게 정확하다.

## 왜 지금도(특히 LLM에서) Adam(정확히는 AdamW)을 많이 쓰는가
1. **파라미터마다 그래디언트 스케일이 극단적으로 다른 환경에 강하다** — Transformer는 임베딩 레이어, 깊은 레이어, LayerNorm 파라미터 등 층마다 그래디언트 크기 분포가 크게 다르다. 2nd moment의 파라미터별 적응적 스케일링이 이런 환경에 잘 맞는다.
2. **희소하거나 노이즈가 큰 그래디언트에 강하다** — 큰 vocabulary의 embedding처럼 대부분의 스텝에서 그래디언트가 0이거나 희소한 경우에도 안정적으로 동작한다(AdaGrad 계보의 장점 계승).
3. **하이퍼파라미터 튜닝 부담이 적다** — 기본값($\beta_1=0.9,\beta_2=0.999$)이 광범위한 아키텍처·데이터에서 거의 그대로 잘 작동해서, 대규모 LLM 학습처럼 한 번 실패하면 비용이 막대한 상황에서 "검증된 기본값"이 있다는 게 실무적으로 큰 이점이다.
4. **AdamW(Loshchilov & Hutter, 2019)로 진화** — 원래 Adam에 L2 정규화(weight decay)를 그래디언트에 더해서 넣으면, 적응적 스케일링($\sqrt{\hat{v}_t}$로 나누는 것) 때문에 weight decay 효과가 파라미터마다 왜곡된다. AdamW는 weight decay를 그래디언트 항에서 **분리(decouple)**해서 업데이트 식 마지막에 별도로 적용한다. 지금 거의 모든 LLM(GPT, LLaMA, PaLM 계열 등)은 Adam이 아니라 **AdamW**를 쓴다.
5. **Warmup과의 궁합** — 학습 초반엔 $v_t$ 추정치의 표본이 적어 분산이 크다(그래디언트가 몇 개 안 봤는데 $\sqrt{\hat{v}_t}$로 나누면 스텝이 불안정하게 튈 수 있음). 그래서 실전에서는 거의 항상 **learning rate warmup**을 함께 쓴다 — 이는 Adam 고유의 약점이자, 지금 모든 LLM 학습 레시피에 warmup이 기본으로 들어있는 이유이기도 하다.

## 메모리 관점 — [[LLM Training Overview]]와의 연결
Adam은 파라미터마다 $m_t$, $v_t$ **두 개**의 값을 추가로 유지해야 한다. FP32로 저장하면 파라미터 자체보다 **optimizer state가 2배** 더 많은 메모리를 차지한다 — [[LLM Training Overview]]에서 "optimizer state가 training 메모리 폭증의 주범"이라고 한 것이 바로 이것이다. 이 문제 때문에:
- **ZeRO(DeepSpeed)** 같은 기법은 이 $m_t, v_t$를 여러 GPU에 분산(shard)해서 GPU당 부담을 줄인다.
- **8-bit Adam / Adafactor** 같은 변형은 $m_t, v_t$ 자체를 저정밀도로 저장하거나 factorize해서 메모리를 줄인다.
- [[Smart-Infinity - Fast Large Language Model Training using Near-Storage Processing on a Real System]]는 이 optimizer state(와 그 update 연산)를 아예 GPU 밖, computational storage device로 오프로드하는 접근이다.

## 우리 위키와의 연결
- 상위 개념: [[LLM Training Overview]] (optimizer state가 왜 메모리를 많이 먹는지의 맥락)
- Optimizer state 오프로딩 실제 시스템: [[Smart-Infinity - Fast Large Language Model Training using Near-Storage Processing on a Real System]]
- Parallelism과의 관계: [[Demystifying Parallel and Distributed Deep Learning - An In-Depth Concurrency Analysis]](분산 학습에서 optimizer state 처리 방식 논의), [[Model Parallelism on Distributed Infrastructure - A Literature Review from Theory to LLM Case-Studies]]

## Personal annotations
<본인 메모 영역>
