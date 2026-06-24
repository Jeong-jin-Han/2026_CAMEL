---
title: "Heimdall: Optimizing Storage I/O Admission with Extensive Machine Learning Pipeline"
aliases: [Heimdall]
description: "production trace에서 67%까지 떨어진 ML I/O admission 정확도를, 전체 ML pipeline 단계별 도메인 특화 혁신(period-based labeling, 3단계 noise filtering 등)으로 93%까지 끌어올려 평균 I/O latency를 15-35% 낮춘 EuroSys 2025 연구."
venue: Eurosys
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/fs
  - topic/io-admission
  - topic/ml
  - venue/eurosys
  - year/2025
---

# Heimdall: Optimizing Storage I/O Admission with Extensive Machine Learning Pipeline

> **Eurosys 2025** · `cluster/fs` · Source: [Heimdall - Optimizing Storage I-O Admission with Extensive Machine Learning Pipeline.pdf](<Heimdall - Optimizing Storage I-O Admission with Extensive Machine Learning Pipeline.pdf>)

저자: Daniar H. Kurniawan, Rani Ayu Putri, Peiran Qin, Kahfi S. Zulkifli, Ray A. O. Sinurat, Janki Bhimani, Sandeep Madireddy, Achmad Imam Kistijantoro, Haryadi S. Gunawi (University of Chicago, Bandung Institute of Technology, Florida International University, Argonne National Laboratory 등)

## TL;DR
SSD의 비결정적 internal operation(GC, buffer flush, wear leveling 등)으로 인한 tail latency를 줄이기 위해, redundant flash array에서 느린 device로 가는 I/O를 다른 replica로 reroute하는 것이 **I/O admission control**이다. 기존 ML 기반 admission(특히 LinnOS)은 최신 SSD/workload에서 정확도가 평균 67%까지 추락한다. Heimdall은 ML pipeline의 모든 단계(labeling, noise filtering, feature engineering, model/training engineering, deployment optimization)에 도메인 지식을 주입하여 정확도를 **67% → 93%**로 올리고, sub-µs(최저 0.08µs) inference latency와 28KB 메모리로 평균 I/O latency를 SOTA 대비 **15-35%** 낮춘다. user-level, in-kernel(Linux block layer), distributed(Ceph RADOS) 배포까지 검증했다.

## 문제 & 동기
데이터센터의 flash array는 µs급 access 속도를 제공하지만, SSD 내부의 비결정적 background 동작(GC, internal buffer flush, wear leveling)이 tail latency를 크게 증폭시킨다. GC는 latency를 최대 60배까지 악화시킬 수 있다. 이를 완화하기 위해, replica를 둔 redundant storage에서 느린 device로 가는 read I/O를 admit할지(2), decline(3)하고 front-end가 다른 replica로 reroute(4)할지 결정하는 것이 I/O admission control(replica selection)이다.

기존 heuristic(hedging, replica scoring, rate limiting)은 고정된 규칙으로 인해 복잡해진 현대 workload에 적응하지 못한다. ML 기반(LinnOS 등)이 등장했으나, 저자들이 MSR Cambridge/Alibaba/Tencent 최신 trace와 최신 device로 재평가하자 정확도가 **평균 67%**로 추락했다(원논문 주장 대비 큰 하락). admission control에서 낮은 정확도는 false admit(느린데 admit)·false reroute(빠른데 reroute)로 직접 I/O latency를 악화시키므로 특히 치명적이다. 핵심 진단은 기존 ML-for-systems 연구가 ML pipeline의 주요 단계(Figure 1)를 건너뛰거나 단순화한다는 점이다.

> [!quote]- 📄 원문 표현 (paper)
> "several ML-powered I/O admission controls [26, 32, 47] showed a significant drop in accuracy—averaging 67%, far below their original claims." (p.2)
>
> "GC operations can negatively affect performance by increasing latency by up to 60× [51]." (p.1)
>
> "These gaps occur when key stages of the ML design process, as shown in Figure 1, are insufficiently addressed. Skipping or over-simplifying these stages can hinder the model's potential in terms of accuracy, performance, and adaptability." (p.2)

## 핵심 통찰

> [!note]- 통찰 1: SSD internal 관리는 단일 I/O가 아니라 "기간(period)"에 걸쳐 영향을 준다 → period-based labeling
> LinnOS 등은 latency cutoff(inflection point)로 개별 I/O를 fast/slow로 라벨링하는데, 이는 부정확하다. 예컨대 큰 I/O는 measured latency가 cutoff보다 커서 "slow"로 라벨되지만, 다른 device로 reroute해도 크기 때문에 여전히 느리므로 잘못된 라벨이다. Heimdall은 device가 fast period인지 slow period(GC contention 등)인지를 추정하여, 그 period 안의 I/O들을 일괄 라벨한다. throughput drop과 latency spike를 함께 보고 busy 구간을 찾으며, throughput이 I/O size를 반영하므로 spike 감지에 더 민감하다.

> [!note]- 통찰 2: "Garbage in, garbage out" — 노이즈 제거가 가장 큰 정확도 기여
> training data의 3종 노이즈(slow period 안의 fast outlier, fast period 안의 slow outlier, 짧은 burst)는 model misprediction을 유발한다. 도메인 특화 3-stage noise filtering으로 이를 누적 16%p 정확도를 깎던 노이즈를 제거하면 정확도가 93%까지 오른다. Figure 14a 기준 noise filtering(LN)은 단일 단계 중 +16%로 가장 기여가 크다.

> [!note]- 통찰 3: per-page가 아닌 per-I/O period labeling이 모델을 단순하게 만든다
> LinnOS는 cutoff 기반 per-page(4KB) 결정이라 큰 I/O를 작은 4KB 블록으로 쪼개 각각 inference해야 한다. Heimdall은 period 기반 per-I/O 라벨이라 I/O 크기와 무관하게 한 번의 inference면 된다. 그 결과 NN은 hidden layer 2개·neuron 128/16으로 LinnOS보다 단순하면서(가중치 3472 vs 8448) 더 정확하다.

> [!note]- 통찰 4: real-world 배포를 위한 learning granularity(joint inference)로 정확도-throughput을 trade
> 모든 I/O마다 inference하면 intensive workload에서 CPU 과부하가 된다. P개의 I/O를 묶어 한 번의 inference로 P개 결정을 내리는 joint inference로, 약간의 정확도 손실로 throughput을 크게 올린다(녹색 신호등 비유). 이 기법은 ML-for-systems의 공통 난제인 real-world deployment에 일반화된다.

## 설계 / 메커니즘

Heimdall은 black-box로 동작하며 request latency를 모니터링해 admission/reroute를 결정한다. ML pipeline(Figure 1)의 8개 stage(Data Preparation, Data Labeling, Feature Engineering, Model Engineering, Training Engineering, Deployment Optimization, Platforms)에 도메인 혁신을 넣는다.

> [!abstract]- §3.1 Accurate Labeling (period-based labeling)
> - 3-stage 알고리즘(Figure 4): (a) latency가 높고 throughput이 낮을 때만 device busy로 의심 — `high_lat`, `low_thpt` threshold로 판단. (b) gradient descent 기반으로 SSD/workload별 threshold를 sensitivity-accuracy 균형점(Figure 3d)에서 자동 선택. (c) busy period의 시작/끝을 결정하고, slow period의 throughput median 미만 I/O들을 "decline(1)"로, fast는 "admit(0)"으로 라벨(TailZone 라벨링, lines 12-15).
> - cutoff→period-based 전환만으로 정확도 +5.5%p, 최종 93%까지 도달(Figure 5a).

> [!abstract]- §3.2 3-Stage Noise Filtering
> - Stage 1: slow period 안의 fast outlier 제거 — busy해도 NAND cache hit 등으로 "운 좋게" 빠른 I/O(낮은 latency·높은 throughput) 제거.
> - Stage 2: fast period 안의 slow outlier 제거 — read retry(voltage mismatch), ECC 등 rare device idiosyncrasy로 인한 일시적 느린 I/O 제거.
> - Stage 3: 짧은 burst(연속 3개 이하의 slow) 제거 — internal contention으로 보기 어려운 잡음. gradient descent(Figure 3d)로 high-accuracy·low-sensitivity threshold 선정.
> - 누적 정확도 +16%p(평균). Figure 6는 Alibaba trace 샘플로 각 노이즈 유형을 시각화.

> [!abstract]- §3.3-3.6 Feature / Model / Training Engineering
> - Feature engineering(§3.3): 기존 31 feature는 inference당 2ms overhead. low-correlation feature(timestamp, disk ID) 제거(feature selection), digitization 대신 **min-max scaling**(feature scaling) 채택 — robust/standard scaler는 모든 historical latency 값을 보관해야 해 메모리 비용이 크지만 min-max는 min/max만 필요해 가볍고 정확. historical depth는 **N=3**이면 충분(Figure 7c). 핵심 5 feature: queue length, historical queue length, historical latency, historical throughput, I/O size(Figure 7b).
> - Model exploration(§3.4): NN이 가장 높고 안정적인 정확도(Figure 8). tree 모델은 depth 제약이 있음.
> - Hyperparameter tuning(§3.5): hidden layer 2개, neuron 128/16, hidden layer activation은 ReLU, output layer는 single-neuron sigmoid(LinnOS의 2-neuron 대비 gradient 전파 계산량 절반)(Figure 9f).
> - Training(§3.6): slow/fast imbalance에 대해 weighted loss(biased training)는 dataset마다 최적값이 달라 비효과적. 대신 heavy write I/O 포함 period를 data selection에 넣고 rerate/resize augmentation으로 대응.

> [!abstract]- §4 Deployment Optimization (sub-µs inference)
> - Python-to-C(§4.1): Python→C/C++ 변환(20µs) → gcc -O3 optimization → quantization(가중치×1024로 정수화, 4 decimal 정밀도 유지). 최종 inference latency **0.05µs/inference**. CPU별: AMD Ryzen 9 5900HS 0.12µs, AMD EPYC 7352 0.08µs, Apple M1 Pro 0.05µs.
> - Joint/Group inference(§4.2): GPU batching(LAKE) 대신 host-to-GPU overhead 없는 joint inference로 P개 I/O를 한 inference로 결정. inference granularity 1~P 조절 가능. feature selection이 핵심 — 최신 I/O 위주로 feature를 우선화해 모델 복잡도 억제.

구현 규모(Table 1): 총 **20.9 KLOC**(주로 Python·C/C++). dataset prep 2.5K, design pipeline 3.6K, optimization 1.2K, retraining 0.2K + 통합(user-level 3.7K, Linux kernel 2.1K, Ceph RADOS 2.3K, eval module 5.3K).

## 평가

> [!example]- 평가 상세 (수치 + 페이지)
> - **셋업**(p.9): 2TB raw block trace(Alibaba/Microsoft/Tencent)→11TB 중간 데이터. Chameleon Storage-NVMe node(AMD EPYC 7352 2.30GHz 24-core, 256GB DRAM), 10종 SSD(Intel, Samsung, Hitachi 등). 50:50 train/test split로 unbiased 평가. ROC-AUC 주 지표 + PR-AUC, F1, FNR, FPR.
> - **Large-scale(§6.1, p.9-10)**: 500개 random trace(각 3분, 100k~10M I/O), 5종 augmentation. Heimdall이 평균 read latency에서 baseline 대비 최저, p50~p99.99 tail에서 SOTA(C3, AMS, Heron, LinnOS, hedging) 대비 우월(Figure 11). hedging은 p98 backup I/O 과부하로 오히려 baseline보다 평균 latency 악화.
> - **Kernel-level(§6.2, p.11)**: Intel DC-S3610 + Samsung PM961 이종 SSD에서 in-kernel 배포, non-baseline 대비 평균 latency **38-48% 낮음**(Figure 12).
> - **Wide-scale Ceph(§6.3, p.11)**: 10노드 Ceph(20 OSD, FEMU-emulated SSD). scaling factor SF=10일 때 p75부터 baseline tail이 커지는 구간을 효과적으로 잘라냄. 모든 SF에서 random 대비 latency reduction 양수(Figure 13).
> - **Accuracy ablation(§6.4, p.12)**: LinnOS baseline 67% → basic labeling(LB) 50.2%(digitization 제거 controlled lower bound) → +min-max scaling 67.5% → +accurate labeling(LA) 73% → +feature extraction(FE) 77% → +feature selection(FS) → +model engineering(M) → +noise filtering(LN) **93%**. noise filtering이 +16%로 최대 기여(Figure 14).
> - **Joint inference(§6.5, p.12-13)**: 기본(joint=1)은 0.5 mIOPS에서 latency가 2µs로 spike. joint=9면 4 mIOPS(8배 무거운 workload)에서도 2µs 이하 유지. joint 9면 정확도는 88%→81%(median)로 하락 — joint=3이 throughput/accuracy 균형점. CPU joint inference는 LAKE GPU batching 대비 inference latency 최대 10배 감소(Figure 15).
> - **Overhead(§6.6, p.13)**: LinnOS 대비 메모리 2.4배 적음(**28KB** vs 68KB), CPU 2.5배 적음, 가중치 3472 vs 8448(2.4배 적은 multiplication). joint=3(J3)은 LinnOS 대비 CPU overhead 85% 절감.
> - **Training time(§6.7, p.13)**: I/O 100만 개당 preprocessing 16.8s(CPU) + training 3.7s(GPU).
> - **Long-term retraining(§7, p.13-14)**: 8시간 Tencent trace(write IOPS가 read의 2배). 단일 학습은 정확도가 63%~82%로 drift. 1분마다 정확도 모니터링→80% 미만 시 직전 1분으로 retrain. 8시간 내 37회 retrain(평균 816k I/O 사용, 수 초 내 완료)(Figure 17).
> - **vs AutoML(§8, p.14-15)**: auto-Sklearn(16 classifier) 대비 ROC-AUC 평균 22% 높음. AutoML은 exploration time 1.8~4.8시간, dataset별 cosine similarity <0.01(generalization 불가) vs Heimdall은 항상 1.0(Figure 18).

## 섹션 노트
- §1 Introduction: 3대 기여 — period-based labeling, learning granularity(joint inference) 일반화, generic·extensible ML pipeline. user-level/in-kernel/Ceph 3단계 통합.
- §2 Background: admission problem 정의(admit/decline/reroute), redundancy(FusionRAID, Tiny-Tail Flash, EC-Cache)로 late data 재구성. write tail은 internal buffer로 rare하므로 read에 집중. LinnOS는 per-page(4KB) uniform 결정이라 I/O size를 feature로 안 씀.
- §3 Heimdall's Pipeline: labeling→noise filtering→feature/model/training engineering. ROC-AUC를 §6.4 전까지 정확도 지표로 사용.
- §4 Deployment Optimizations: negligible inference latency(Python-to-C) + joint/group inference.
- §5 Implementation Scale: 20.9 KLOC 분해(Table 1).
- §6 Evaluation: 5개 핵심 질문(large-scale, kernel, Ceph, pipeline ablation, joint inference).
- §7 Retraining: performance drift(workload/concept drift) 대응 preliminary retraining policy.
- §8 Discussion: broader impact(Table 2로 ML-for-storage 문헌의 pipeline gap 분석), AutoML 비교, fully-automated Heimdall은 future work.
- §9 Conclusion: code 공개(github.com/ucare-uchicago/Heimdall), 학생 대상 "mini competition" testbed.
- Appendix A: artifact(4 experiment E1-E4, Chameleon testbed, 2 unmounted SSD 필요).

## 핵심 용어
- **I/O admission control (replica selection)**: redundant flash array에서 느린 device로 가는 I/O를 admit/decline하고 front-end가 다른 replica로 reroute하도록 escalate하는 정책. tail latency 감소가 목표.
- **period-based labeling**: 개별 I/O가 아니라 device의 fast/slow "기간"을 추정해 그 안의 I/O를 일괄 라벨링하는 Heimdall의 핵심 labeling 기법. cutoff(inflection point) 기반 라벨링의 부정확성을 해결.
- **latency cutoff**: latency CDF의 inflection point로 fast/slow를 가르는 기존 방식. variable I/O size에서 부정확(큰 I/O를 무조건 slow로 오판).
- **3-stage noise filtering**: slow period 내 fast outlier, fast period 내 slow outlier, 짧은 burst 노이즈를 제거하는 도메인 특화 전처리. 정확도 +16%.
- **false admit / false reroute**: 느린데 fast로 예측해 admit / 빠른데 slow로 예측해 reroute하는 오류. admission control에서 직접 latency를 악화.
- **joint/group inference**: P개 I/O의 feature를 모아 한 번의 inference로 P개 결정을 내리는 학습 granularity. throughput↑ accuracy↓ trade-off.
- **historical depth (N)**: 최근 N개 I/O의 정보(queue length, latency 등)를 feature로 쓰는 깊이. N=3이면 충분.
- **performance drift**: 시간에 따라 모델 정확도가 떨어지는 현상. workload 변화(input drift), device/환경 변화(concept drift)에서 기인.
- **scaling factor (SF)**: end-user request가 갖는 병렬 sub-request 수. SF가 클수록 tail latency가 증폭되어 admission control 효과가 커짐("Tail at Scale").
- **LinnOS**: light NN으로 per-page admission을 예측하는 선행 ML 기반 정책. 최신 환경에서 정확도 67%로 하락한 비교 대상.

## 강점 · 한계 · 열린 질문
**강점**
- ML pipeline 전 단계에 도메인 지식을 체계적으로 주입, 67%→93%라는 큰 정확도 개선과 단계별 ablation(Figure 14)으로 기여를 정량화.
- sub-µs(0.05µs) inference, 28KB 메모리, LinnOS 대비 2.4-2.5배 적은 overhead로 실제 배포 가능성 입증.
- user-level/in-kernel(Linux)/distributed(Ceph) 3단계 + 500 trace large-scale unbiased(50:50 split) 평가로 일반화 검증.
- code 및 artifact 공개, ML-for-storage testbed("playground")로서의 재사용성.

**한계**
- read I/O에 집중 — write tail은 internal buffer로 rare하다는 가정하에 다룸.
- retraining policy는 preliminary(언제·얼마나 retrain할지, catastrophic forgetting, drift 감지 등 미해결, §7).
- joint inference는 정확도 손실(joint=9에서 88→81%)을 수반, granularity 선택이 운영자 부담.
- fully-automated(AutoML류) Heimdall은 미구현(future work). 도메인 지식 주입이 수작업.

**열린 질문**
- long-term deployment에서 retrain 트리거의 최적 빈도/데이터량은? per-request logging overhead 없이 retraining 데이터를 어떻게 확보할까?
- Table 2의 다른 storage 도메인(caching, indexing, prefetching 등)에 동일 pipeline 방법론을 적용하면 얼마나 일반화되는가?
- workload drift를 사전 감지할 수 있는 I/O characteristic은 무엇인가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. I/O admission control이 왜 필요하며 무엇을 결정하는가?
> > SSD 내부의 비결정적 동작(GC, buffer flush, wear leveling)이 tail latency를 최대 60배까지 키운다. redundant flash array에서는 같은 데이터의 replica가 여러 device에 있으므로, 느린 device로 가는 read I/O를 admit할지(허용) decline할지 판단하고, decline 시 front-end가 다른(빠른) replica로 reroute하게 한다. 목표는 read tail/평균 latency 감소다.

> [!question]- Q2. 기존 ML 기반(LinnOS) 정확도가 67%로 떨어진 근본 원인은?
> > (1) cutoff 기반 per-page labeling이 variable I/O size에서 부정확(큰 I/O를 무조건 slow로 오판). (2) ML pipeline의 주요 단계(noise filtering, feature/model engineering 등)를 건너뛰거나 단순화. (3) 모델이 4년 전 SSD/workload 기준이라 빨라진 최신 device와 새 workload를 못 따라감.

> [!question]- Q3. period-based labeling이 cutoff labeling보다 나은 이유는?
> > cutoff는 개별 I/O의 측정 latency만 보므로, 다른 device로 reroute해도 여전히 느릴 큰 I/O를 "slow"로 잘못 라벨한다. period-based는 device가 fast/slow 기간(GC contention 등)에 있는지를 throughput drop·latency spike로 추정해 그 기간의 I/O를 일괄 라벨하므로, "reroute하면 빨라질 수 있는가"라는 admission의 본질에 맞는다. 정확도 +5.5%p.

> [!question]- Q4. 3-stage noise filtering은 무엇을 제거하며 효과는?
> > (1) slow period 안의 fast outlier(NAND cache hit로 운 좋게 빠른 I/O), (2) fast period 안의 slow outlier(read retry·ECC 등 rare 이벤트), (3) 짧은 burst(연속 3개 이하 slow, internal contention으로 보기 어려움). 이 3종 노이즈가 누적 16%p 정확도를 깎으며, 제거 시 정확도가 93%까지 올라 단일 단계 중 최대 기여(+16%).

> [!question]- Q5. sub-µs inference latency는 어떻게 달성하나?
> > Python→C/C++ 변환(20µs)→gcc -O3 optimization→quantization(가중치×1024 정수화)로 0.05µs/inference까지 줄인다. 추가로 period-based per-I/O 라벨이라 I/O 크기와 무관하게 한 번만 inference하면 되고, NN이 hidden layer 2개·neuron 128/16으로 LinnOS보다 단순(가중치 3472 vs 8448)하다.

> [!question]- Q6. joint/group inference가 해결하는 문제와 trade-off는?
> > 모든 I/O마다 inference하면 intensive workload에서 CPU 과부하·latency spike(0.5 mIOPS에서 2µs)가 발생한다. P개 I/O를 묶어 한 번의 inference로 P개 결정을 내리면(녹색 신호등 비유) joint=9에서 4 mIOPS(8배 무거운 workload)도 2µs 이하로 처리한다. 대가는 정확도 하락(joint=9에서 88→81%)이며, joint=3이 균형점. GPU batching(LAKE) 대비 host-to-GPU overhead가 없어 inference latency 최대 10배 감소.

> [!question]- Q7. Heimdall은 어디에 배포되며 실제 latency 개선은?
> > user-level storage, Linux kernel block layer(in-kernel), Ceph RADOS(distributed) 3곳. large-scale(500 trace)에서 SOTA 대비 평균 I/O latency 15-35% 감소, in-kernel 이종 SSD에서 non-baseline 대비 38-48% 감소, Ceph에서 SF가 클수록 tail 절감 효과 확대.

> [!question]- Q8. retraining이 필요한 이유와 Heimdall의 정책은?
> > 장기 배포 시 workload drift(input drift)·device/환경 변화(concept drift)로 정확도가 63~82%로 출렁인다(performance drift). Heimdall은 1분마다 정확도를 모니터링해 80% 미만이면 직전 1분 데이터로 가볍게 retrain한다. 8시간 trace에서 37회 retrain(평균 816k I/O, 수 초 소요). 다만 언제·얼마나 retrain할지는 preliminary 수준이다.

## 🔗 Connections
[[File System]] · [[EuroSys]] · [[2025]]

## References worth following
- LinnOS (Hao et al., OSDI 2020, [32]) — Heimdall이 직접 개선한 light NN 기반 per-page I/O admission. 핵심 baseline.
- LAKE (Fingler et al., ASPLOS 2023, [26]) — kernel-space GPU batched ML inference. joint inference의 비교 대상.
- The Tail at Scale (Dean & Barroso, CACM 2013, [24]) — tail latency·hedging·scaling factor의 이론적 토대.
- Tiny-Tail Flash (Yan et al., FAST 2017, [81]) / FusionRAID (Jiang et al., FAST 2021, [35]) — redundancy로 late data를 재구성하는 admission 활용 선행 연구.
- IODA (Li et al., SOSP 2021, [51]) — host/device co-design으로 read predictability 확보, GC 영향(60×) 출처.

## Personal annotations
<본인 메모 영역>
