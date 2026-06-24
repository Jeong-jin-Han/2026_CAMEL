---
title: "FailureMiner: A Joint Key Decision Mining Scheme for Practical SSD Failure Prediction and Analysis"
aliases: [FailureMiner]
description: "RF의 결정 경로에서 joint key decision을 마이닝하고 boundary-preserving downsampling으로 데이터 불균형을 해결해 정확하고 해석 가능한 SSD 고장 예측을 달성한 FAST 2026 논문."
venue: FAST
year: 2026
tier: deep
status: done
tags:
  - paper
  - cluster/reliability
  - topic/failure-prediction
  - topic/ml
  - venue/fast
  - year/2026
---

# FailureMiner: A Joint Key Decision Mining Scheme for Practical SSD Failure Prediction and Analysis

> **FAST 2026** · `cluster/reliability` · Source: [FailureMiner - A Joint Key Decision Mining Scheme for Practical SSD Failure Prediction and Analysis.pdf](FailureMiner%20-%20A%20Joint%20Key%20Decision%20Mining%20Scheme%20for%20Practical%20SSD%20Failure%20Prediction%20and%20Analysis.pdf)

**저자**: Shuyang Wang, Yuqi Zhang, Haonan Luo (Samsung R&D Institute China Xi'an); Kangkang Liu (Tencent); Gil Kim, JongSung Na, Claude Kim, Geunrok Oh, Kyle Choi (Samsung Electronics); Ni Xue, Xing He (Samsung R&D Institute China Xi'an)

## TL;DR
FailureMiner는 SSD 텔레메트리(telemetry) 데이터를 사용해 실용적이고 해석 가능한 고장 예측을 목표로 한 두 가지 핵심 기법을 제안한다. (1) **Boundary-preserving downsampling**: 무작위로 healthy 샘플을 버리는 대신 고장 샘플과 패턴이 유사하고 분류 경계 근처에 있는 healthy 샘플을 선별 보존해 미묘한 차이를 모델이 학습하게 한다. (2) **Joint contribution-based key decision set extraction**: Random Forest의 수많은 결정(decision) 중 SHAP 기반 impact score와 동시 출현 빈도를 결합(Apriori식 확장)해, 고장과 직결된 소수의 속성 조합·임계값(joint key decision)만 추출한다. 결과적으로 기존 방법 대비 평균 precision 38.6%, recall 80.5% 개선하며, Tencent 데이터센터의 35만+ SSD에 1년 이상 실배포되었다.

## 문제 & 동기
엔터프라이즈 데이터센터에서 SSD 고장은 데이터 손실·서버 다운으로 이어져 신뢰성의 핵심 관심사다. 기존 연구는 (1) 데이터 전처리(downsampling), (2) 모델 빌딩(feature selection + classification), (3) 해석 기반 분석의 3단계를 따르지만, 저자들은 production 환경에서 세 가지 한계(lesson)를 발견했다.
- **Downsampling의 문제**: 고장 샘플 수가 극히 적어 데이터 불균형이 심하다. 기존 downsampling은 고장과 유사하고 분류 경계 근처에 있는 healthy 샘플에 주의를 기울이지 않아, 이들을 제거하면 모델이 미묘한 구분을 학습하지 못한다.
- **Feature selection의 문제**: 노이즈 속성 제거에 효과적이지만, 단독으로는 고장과 무관해 보여도 다른 속성과 결합 시 보조적 역할을 하는 유용한 속성까지 제거해 버린다.
- **속성 단위 해석의 한계(coarse-grained)**: outlier 임계값이나 속성 조합 같은 더 세밀한 정보가 고장 패턴 분석에 필요한데, 속성 수준 해석은 이를 제공하지 못한다.

> [!quote]- 📄 원문 표현 (paper)
> "First, downsampling the data of healthy SSDs can balance the ratio of failed and healthy samples. However, during downsampling, existing work lacks attention to healthy samples that are similar to failed ones and located near the classification boundary, making them easily misclassified." (p.2)
>
> "Second, although feature selection is effective in eliminating noisy attributes, it may inadvertently remove useful attributes as well. Some attributes may not be directly associated with failures and are often excluded during feature selection, but they can play an auxiliary role in failure prediction when combined with other attributes." (p.2)
>
> "Third, attribute-based model interpretation is also coarse-grained. More detailed information, such as outlier thresholds and attribute combinations associated with failures, is important for analyzing failure patterns." (p.2-3)

## 핵심 통찰

> [!note]- 통찰 1: 데이터 불균형은 "경계 근처 유사 healthy 샘플"을 보존해야 풀린다
> 단순 무작위/대표 downsampling은 false alarm을 폭증시킨다. Table 2에 따르면 healthy:failed 비율을 1:1까지 downsampling하면 false alarm이 random downsampling 기준 무려 210.3x, representative downsampling 기준 203.3x로 증가한다. 핵심은 고장 샘플과 유사한 패턴을 가진 healthy 샘플(분류 경계 근처)을 제거하면 모델이 그 패턴을 전부 고장으로 취급해 false alarm이 늘어난다는 점이다. 따라서 고장 샘플을 클러스터링하고 각 클러스터 경계 근처의 유사 healthy 샘플을 보존해야 한다.

> [!note]- 통찰 2: 단일 결정이 아닌 "joint key decision"이 고장 패턴을 드러낸다
> 고장은 여러 속성·결정의 조합으로 나타난다(예: BadNandBlock와 ReadRetry가 동시에 급증). 기존 SDE는 빈도만 보고, SHAP는 단일 결정 중요도만 본다. 저자들은 Figure 2에서 SDE가 추출한 top-5 결정 중 4개는 SHAP 값이 거의 0(고장 예측에 기여 적음)임을 보여, 빈도만으로는 부정확함을 입증한다. 따라서 impact score(SHAP)와 동시 출현 빈도를 결합한 contribution score로 joint key decision을 추출한다.

> [!note]- 통찰 3: 해석은 인간 사고방식과 일치해야 운영자가 쓸 수 있다
> Joint key decision으로 속성 조합과 이상 값 범위를 드러내면 운영자가 고장 패턴·원인을 쉽게 이해할 수 있다. 이는 black-box 모델의 예측 결과만 주는 것보다 운영자의 신뢰와 즉각적 조치 능력을 높인다. 실제로 강한 결정(strong joint key decision)뿐 아니라 precision < 50%인 약한 결정(weak decision)도 SSD health에 영향을 주는 factor를 드러내 운영·유지보수 가이드를 제공한다.

## 설계 / 메커니즘

FailureMiner는 두 부분으로 구성된다(Figure 3): (1) Boundary-preserving downsampling, (2) Joint contribution-based key decision set extraction.

> [!abstract]- (1) Boundary-preserving downsampling (3단계)
> - **Temporal feature generation**: sliding window(w일) 내 속성 변화량을 temporal feature로 추출. `Delta_w A = max(A_{t-w..t}) - min(A_{t-w..t})`. w는 3, 7, 15(단기/주간/격주)로 설정해 변화 추세를 포착. 이 값은 원본 속성과 동일하게 취급.
> - **Failed SSD clustering**: 고장-무관 속성으로 클러스터링하면 초점이 흐려지므로, JIC(J-index classification) 방식으로 고장/healthy 구분 능력이 강한 속성을 선택하고 min-max 정규화. 그 후 K-means로 고장 SSD를 N개 클러스터(기본 N=50)로 분할. 각 클러스터 중심에서 최대 거리를 cluster boundary `B_n`으로 정의.
> - **Healthy SSD dividing**: healthy 데이터와 각 클러스터 중심 간 Euclidean 거리를 계산해 `B_n`보다 가까운 healthy 데이터를 해당 클러스터에 추가(유사 패턴). overfitting 방지를 위해 클러스터당 평균 유사 데이터 수만큼 무작위 healthy 데이터도 추가.

> [!abstract]- (2) Joint contribution-based key decision set extraction (3단계)
> - **Model training**: 각 클러스터마다 모든 원본 텔레메트리 + temporal feature로 Random Forest 학습(단일 속성이 결합으로 고장과 연관될 수 있어 feature selection 안 함).
> - **Key decision mining (impact score)**: RF는 수만 개 결정을 가짐. 결정 단위로 SHAP 값을 개선해 decision-based impact score를 계산. 각 결정 노드의 SHAP 값(해당 결정 유무에 따른 모델 출력 변화)을 여러 트리·경로에 걸쳐 합산. `imp_score > 0.1`인 결정을 key decision 후보로 선택.
> - **Key decision set extraction (joint contribution)**: Apriori에서 영감을 받아 impact score와 동시 출현 빈도를 결합. k-decision set의 contribution score는 `contrib_score = (1/k · Σ imp_score_i) × count_{i1..ik}` (Eq.1, p.4) — 평균 중요도 × 동시 출현 빈도. 임계값 thr(기본 descending contrib_score의 20th percentile)을 넘는 set를 선택하고, 1-decision set → 2-decision set → ... 로 k+1까지 반복 확장·병합하며 joint key decision을 식별.

> [!abstract]- impact score / contribution score 직관
> - imp_score는 단일 결정의 고장 예측 중요도(SHAP 기반)만 측정 → coarse.
> - 일부 고장은 다중 속성·결정의 조합으로만 드러남 → 동시 출현 빈도를 곱해 joint contribution을 측정.
> - 높은 contrib_score = 해당 결정 집합이 고장 식별에 더 크게 기여. 복잡한 결정 경로를 단순화하고 redundant·noisy 결정을 제거.

## 평가

> [!example]- 데이터셋 & 메트릭
> - **Tencent**: 35만+ Samsung PM9A3 SSD에서 2년간 수집한 7천만+ 텔레메트리 로그(85 속성, SMART 24 속성 대비 풍부). 고장 리스트 788건. 학습 1~13개월 / 테스트 14~23개월(Table 3).
> - **Alibaba**: 공개 MC2 SMART 데이터셋, 2만 SSD, 21 속성, 1천만+ 로그. 학습 1~12개월 / 테스트 13~24개월.
> - 메트릭: Precision(P), Recall(R), F0.5-score(precision에 가중 — production에서 false alarm 영향이 더 크기 때문).

> [!example]- 주요 성능 (Table 4, 5)
> - **Tencent (Table 4)**: FailureMiner P=82.2%, R=29.6%, F0.5=0.61. 비교: RF P=55.4%/R=20.4%/F0.5=0.41, CNN-LSTM P=50.0%/R=12.8%/F0.5=0.32, WEFR P=67.9%/R=15.2%/F0.5=0.40, MVTRF P=68.1%/R=19.6%/F0.5=0.46.
> - **Alibaba (Table 5)**: FailureMiner P=68.4%, R=26.4%, F0.5=0.52. 비교: RF 0.27, CNN-LSTM 0.23, WEFR 0.40, MVTRF 0.44 — generalizability 입증.
> - **개선폭**: 기존 방법 대비 평균 precision +38.6%, recall +80.5% (p.1, p.5).
> - 추출된 강한 joint key decision은 단 3개(RF의 총 117,404 결정 중) (p.5).

> [!example]- Joint vs 단일 결정 & Ablation (Table 6, Figure 5)
> - **Joint > single (Table 6)**: joint decision의 F0.5가 각 단일 결정보다 항상 큼. 예: Joint 1 = 0.55 vs single 0.44/0.32/0.001.
> - **Ablation (Figure 5)**: 원본 RF(P=55.4%, R=20.4%, F0.5=0.41) → BPD+RF(P=67.4%, F0.5=0.49, P +21.7%/R +13.7%) → RF+JKE(P=66.7%, F0.5=0.50, P +20.4%/R +25.5%) → 전체 FailureMiner(P=82.2%, R=29.6%, F0.5=0.61). 두 기법 모두 기여.

> [!example]- 하이퍼파라미터 N & 효율 (Table 7, 8)
> - **N의 영향(Table 7)**: N=3이면 클러스터 경계가 너무 넓어 R=25.2%로 저하. N=10~100에서는 정확도 안정(F0.5≈0.60), N=50에서 최고(P=82.2%, R=29.6%, F0.5=0.61). N=200이면 클러스터당 데이터 부족으로 overfitting, R=28.4% 하락. N 증가 시 클러스터당 데이터 감소로 학습시간 단축(N=3: 14237s → N=50: 1673s).
> - **효율(Table 8, Intel Xeon Platinum 8380)**: FailureMiner 학습 1673s / 예측 6s. 비교: CNN-LSTM 학습 2306s(RTX4090)/예측 2318s, MVTRF 3340s/365s. 예측 시간이 매우 짧아 online 배포에 적합.

> [!example]- 고장 패턴 & health factor 분석 (§6)
> - **3가지 강한 고장 패턴**: ① UECC-related(NandUECC>0 급증 → 데이터 손상/고장, decisions 1-1~1-3), ② DRAM-related(DramCECC 누적 → ECC 초과 시 고장; 3개 이상 주소에 에러 시 고장률 60%+, 1개 주소만이면 3.7%, Figure 7), ③ Capacitor-related(CapHealth가 healthy ~160에서 급락; PLP 실패 위험).
> - **시간 분포(Figure 6)**: UECC 결정으로 경보된 SSD의 51%가 1주 내 고장 보고, 76%는 2일 내 처리(평균 처리 3.8일).
> - **약한 결정 factor(Table 9)**: PCIe error(BadTLP≥4106 & BadDLLP≥43084 → 34x), bad block/read retry(BadNandBlock≥10 & ReadRetry≥50 → 60x), end-to-end error(ETEDeteError≥1 → 67x) — 고장률이 일반 SSD 대비 수십 배.
> - **실배포**: Tencent 데이터센터에 1년 이상 운영, 100+ SSD 고장을 사전 예측(p.7).

## 섹션 노트
- **§1 Introduction**: 3가지 lesson(downsampling 경계 샘플 무시, feature selection의 유용 속성 제거, 속성 단위 해석의 coarse함) 제시.
- **§2 Data description**: Tencent 텔레메트리(85속성) vs SMART(24속성). 주요 속성 Table 1(NandUECC, BadNandBlock, DramCECC, CapHealth, ReadRetry, ProgramFail, BadTLP/DLLP/PHYError, ETEDeteError, AvgTemp 등).
- **§3 Background, attempts, lessons**: 설계 목표(high-accuracy/interpretable/user-friendly). 3가지 attempt(data balancing, model building=RF 선택, interpretation=SHAP/SDE 한계)와 lesson.
- **§4 Method**: §4.1 boundary-preserving downsampling, §4.2 joint contribution-based key decision set extraction.
- **§5 Evaluation**: dataset setup, strong joint key decisions, joint vs single, ablation, hyperparameter, deployment.
- **§6 Failure analysis**: §6.1 failure patterns(3종), §6.2 factors affecting SSD health(PCIe/bad block/ETE).
- **§7 Conclusion**: joint key decision 마이닝으로 fine-grained factor 발견, 기존 대비 우수, 35만+ SSD 실배포.

## 핵심 용어
- **Joint key decision**: RF 결정 경로에서 함께 자주 나타나고 고장 식별에 큰 기여를 하는 속성 조합·임계값 판단의 집합(k-decision set).
- **Boundary-preserving downsampling**: 고장 샘플과 유사하고 분류 경계 근처에 있는 healthy 샘플을 선별 보존하면서 데이터 불균형을 완화하는 downsampling.
- **Impact score (imp_score)**: 결정 단위로 개선한 SHAP 값. 단일 결정의 고장 예측 기여도. 기본 임계값 0.1.
- **Contribution score (contrib_score)**: `(1/k·Σ imp_score) × count` — 결정 집합의 평균 중요도와 동시 출현 빈도를 결합한 joint contribution 측정치(Eq.1).
- **Telemetry**: SMART(24속성)보다 풍부한 제조사(Samsung) 맞춤 모니터링 로그(85속성).
- **JIC (J-index classification)**: 속성 임계값을 순회하며 고장/healthy 구분 능력이 큰 속성을 선택하는 기법(failed SSD clustering의 속성 선택에 사용).
- **CapHealth**: 캐패시터 에너지 마진을 나타내는 health 지표(healthy ~160), PLP(Power Loss Protection)와 직결.
- **F0.5-score**: precision에 더 큰 가중을 둔 F-measure. production에서 false alarm 비용이 크기 때문에 사용.
- **Weak decision**: precision < 50%로 직접 예측엔 부적합하나 SSD health에 영향을 주는 factor를 드러내는 결정.

## 강점 · 한계 · 열린 질문
**강점**
- 실제 35만+ SSD에 1년 이상 배포된 production-validated 연구(Tencent + Samsung 협업).
- 두 기법의 ablation으로 각각의 기여를 명확히 분리 입증(BPD, JKE 모두 효과적).
- 117,404개 결정에서 단 3개의 해석 가능한 강한 결정으로 축약 — 운영자 친화적.
- 예측 시간 6s로 online 배포에 최적, deep learning 대비 효율 우수.
- Alibaba 공개 데이터셋으로 generalizability 검증.

**한계**
- Recall이 29.6%(Tencent)로 절대값은 낮음 — precision 우선 전략의 trade-off로, 많은 고장을 놓침.
- N=50 등 하이퍼파라미터 튜닝이 데이터 분포에 의존적이며 N=200 등 극단값에서 성능 저하.
- 분석된 고장 패턴은 Samsung PM9A3 등 특정 모델·텔레메트리 속성에 기반 — 다른 제조사 일반화는 SMART 데이터로만 부분 검증.
- joint key decision 추출의 thr(20th percentile), imp_score(0.1) 등 임계값이 휴리스틱.

**열린 질문**
- Recall을 precision 희생 없이 추가로 높일 수 있는가(현재 미발견된 고장 패턴)?
- 새로운 SSD 세대·firmware로 속성 분포가 바뀔 때 재학습 주기(현재 분기마다)는 충분한가?
- weak decision factor(PCIe/bad block/ETE)를 예측에 통합하면서 false alarm을 억제할 방법은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. FailureMiner가 기존 downsampling과 다른 핵심은?
> 무작위/대표 downsampling이 분류 경계 근처의 유사 healthy 샘플을 무차별 제거해 false alarm을 폭증(최대 210x)시키는 반면, FailureMiner는 고장 SSD를 클러스터링하고 각 클러스터 경계 내 유사 healthy 샘플을 보존(boundary-preserving)해 모델이 미묘한 차이를 학습하게 한다.

> [!question]- Q2. joint key decision은 어떻게 추출되며 단일 결정보다 나은 이유는?
> 각 결정의 SHAP 기반 impact score와 결정들의 동시 출현 빈도를 곱한 contribution score(Eq.1)로, Apriori식으로 1-decision set부터 k-decision set까지 확장 선택한다. 고장은 다중 속성 조합으로 나타나므로(예: BadNandBlock & ReadRetry 동시 급증) 단일 결정보다 F0.5가 항상 높다(Table 6).

> [!question]- Q3. 왜 F0.5-score를 사용하는가?
> production 환경에서는 false alarm(healthy SSD 교체 등)의 비용이 크므로, precision에 더 큰 가중을 두는 F0.5가 적합하다. 저자들은 0.1%의 false alarm rate 증가도 수백 건의 오경보를 낳아 허용 불가하다고 본다(p.7).

> [!question]- Q4. 성능 개선폭과 효율은 구체적으로?
> 기존 방법 대비 평균 precision +38.6%, recall +80.5%. Tencent에서 F0.5=0.61(RF 0.41, CNN-LSTM 0.32 대비). 예측 시간 6s로 CNN-LSTM(2318s) 대비 압도적이며 학습도 1673s로 moderate(Table 8).

> [!question]- Q5. 발견된 3가지 강한 고장 패턴은?
> ① UECC-related(NandUECC 급증 → NAND 데이터 손상), ② DRAM-related(DramCECC 누적 → ECC 초과 시 고장; 3+ 주소 에러 시 고장률 60%+), ③ Capacitor-related(CapHealth 급락 → PLP 실패 위험). 모두 §6.1에서 상세 분석.

> [!question]- Q6. 클러스터 수 N은 성능에 어떻게 영향을 주는가?
> N이 너무 작으면(3) 경계가 넓어 healthy 샘플이 과다 포함되어 recall 저하(25.2%), 너무 크면(200) 클러스터당 데이터 부족으로 overfitting·recall 하락(28.4%). N=50에서 최적(F0.5=0.61). N 증가 시 학습시간은 감소.

> [!question]- Q7. weak decision은 무엇이며 왜 추출하는가?
> precision < 50%로 직접 예측엔 부적합하지만 SSD health에 영향을 주는 factor를 드러내는 결정이다. PCIe error(34x), bad block & read retry(60x), end-to-end error(67x)처럼 고장률을 수십 배 높이는 factor를 식별해 운영·유지보수 가이드를 제공한다(Table 9).

> [!question]- Q8. 어떤 기반 모델을 쓰며 그 이유는?
> Random Forest를 base 모델로 사용한다. RF는 높은 예측 정확도, 효율적 실행, 강한 해석성을 제공하고(설계 목표와 부합), 다중 결정 트리의 투표로 deep learning 대비 계산 복잡도가 낮아 production 학습·추론 비용이 낮기 때문이다.

## 🔗 Connections
[[Reliability]] · [[FAST]] · [[2026]]

## References worth following
- Yuqi Zhang et al., "Multi-view feature-based SSD failure prediction: What, when, and why." FAST 2023. [13] — SDE/MVTRF의 기반, 본 논문의 직접 비교 대상.
- Fan Xu et al., "General feature selection for failure prediction in large-scale ssd deployment." DSN 2021. [12] — WEFR feature selection, Alibaba 데이터셋 출처.
- Sidi Lu et al., "Making disk failure predictions SMARTer!" FAST 2020. [22] — CNN-LSTM 및 JIC 접근의 출처.
- Scott M. Lundberg & Su-In Lee, "A unified approach to interpreting model predictions." NeurIPS 2017. [31] — SHAP, impact score의 토대.
- Rakesh Agrawal & Ramakrishnan Srikant, "Fast algorithms for mining association rules in large databases." VLDB 1994. [37] — Apriori, key decision set 확장의 영감.
- Yunfei Gu et al., "Exploit both SMART attributes and NAND flash wear characteristics to effectively forecast SSD-based storage failures in clusters." ATC 2024. [39] — 관련 SSD 고장 예측 최신 연구.

## Personal annotations
<본인 메모 영역>
