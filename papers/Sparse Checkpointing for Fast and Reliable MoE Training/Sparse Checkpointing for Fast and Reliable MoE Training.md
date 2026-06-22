---
title: "Sparse Checkpointing for Fast and Reliable MoE Training"
aliases: [Sparse Checkpointing, Sparse Checkpointing MoE]
description: "MoEvement — MoE의 expert-parallel 구조를 활용한 분산 in-memory checkpointing 시스템으로, sparse checkpointing·sparse-to-dense conversion·upstream logging으로 runtime-recovery tradeoff를 깨고 ETTR ≥ 0.94와 최대 8배 학습 가속을 달성"
venue: NSDI
year: 2026
tier: deep
status: done
presenter: 정진
present-date: 2026-08-20
tags:
  - paper
  - cluster/mine
  - cluster/llm
  - topic/moe
  - topic/checkpointing
  - topic/fault-tolerance
  - topic/distributed-training
  - topic/reliability
  - venue/nsdi
  - year/2026
---

# Sparse Checkpointing for Fast and Reliable MoE Training
> **NSDI 2026** · `cluster/llm` · Source: [Sparse Checkpointing for Fast and Reliable MoE Training.pdf](<Sparse Checkpointing for Fast and Reliable MoE Training.pdf>)
저자: Swapnil Gandhi (Stanford University), Christos Kozyrakis (Stanford University & NVIDIA)

## TL;DR
대규모 모델을 수천 개 GPU에서 장기간 학습할 때 failure는 피할 수 없는 현실인데, 기존 checkpointing 기법은 dense 모델을 겨냥해 설계되어 학습 state가 훨씬 큰 **MoE** 모델에 적용하면 costly stall이나 prolonged recovery로 효율이 크게 떨어진다. 저자들은 MoE의 expert-parallel 구조에 맞춘 분산 in-memory checkpointing 시스템 **MoEvement**("movement", MoE 모델과 training state의 이동에 초점, p.2 각주2)를 제안한다. 세 가지 핵심 아이디어는 (1) operator 부분집합을 여러 iteration에 걸쳐 점진적으로 스냅샷하는 **sparse checkpointing**, (2) sparse 스냅샷으로부터 일관된 dense checkpoint를 점진적으로 복원하는 **sparse-to-dense (checkpoint) conversion**, (3) pipeline-stage boundary에서 activation/gradient를 기록해 영향받지 않은 worker를 재실행하지 않고 localized recovery를 가능케 하는 **upstream logging**이다. 최대 64 expert를 갖는 다양한 MoE 모델 평가에서 MoEvement는 checkpointing overhead를 최대 4배, recovery overhead를 최대 31배 줄이고, 잦은 failure(MTBF가 10분만큼 짧아도)에서도 ETTR ≥ 0.94를 유지하며 최대 8배 학습 가속을 달성하되 synchronous training semantics를 훼손하지 않는다 (Abstract, p.1).

## 문제 & 동기 (Problem & Motivation)
대규모 분산 학습 클러스터는 수천 개 GPU로 구성되어 hardware fault, network disruption, software bug 등으로 failure가 불가피하다. Microsoft, ByteDance, Alibaba, Google 등 주요 조직은 평균 45분마다 한 번씩 failure를 보고했고, GPU 수가 늘수록 MTBF가 떨어진다. Meta는 131,072 GPU 작업에서 MTBF가 14분만큼 짧다고 보고했고, Alibaba Cloud는 상위 5% resource-intensive job에서 비정상 종료율 44%를 보고했다 — at scale, failure는 anomaly가 아니라 norm이다 (p.1, p.4).

Checkpointing은 주된 fault-tolerance 메커니즘이지만, 기존 기법은 모든 파라미터가 계산/수렴에 균일하게 기여하는 dense 모델을 겨냥한다. MoE 모델은 dense layer를 수십~수백(때로 수백만) 개의 sparsely activated expert operator로 바꾸는 근본적으로 다른 패러다임을 따르며, 각 token은 소수 expert만 활성화한다. 이 sparse·동적 activation 패턴이 전통적 checkpointing에 세 가지 핵심 challenge를 노출한다 (p.1).

- **Challenge #1 Runtime–Recovery Tradeoff**: CheckFreq, Gemini 같은 SOTA 기법은 dense엔 효과적이나 MoE에선 학습 state가 한 자릿수 커져 iteration time이 늘어난다. 짧은 interval은 prohibitively expensive해서(매 iteration checkpointing 시 Gemini에서 DeepSeek-16.4B/64E 학습이 2.5배 느려짐), interval을 늘리면 runtime overhead는 줄지만 recovery time이 길어진다 (Fig 1a, p.2). MTBF가 떨어지면 recovery cost가 누적되어 ETTR이 급락한다 — Gemini의 ETTR은 2-hour MTBF에서 0.93으로 정점이지만 30-minute MTBF에선 최대 0.79, 10-minute MTBF에선 0.47까지 추락한다 (Fig 1b, p.1-2).
- **Challenge #2 Correctness–Efficiency Tension**: MoC-System의 Partial Expert Checkpointing(PEC)은 round-robin으로 일부 expert만 스냅샷해 overhead를 줄이지만, recovery 시 최근 checkpoint가 없는 expert가 stale parameter로 되돌아가 token loss를 유발하고 synchronous training semantics를 위반한다 (p.2).
- **Challenge #3 Global Rollback Scope**: synchronous semantics 보장을 위해 기존 기법은 faulty 여부와 무관하게 모든 worker를 공통 checkpoint로 roll back하며, 이 global rollback이 recomputation overhead를 키우고 scale이 커질수록 비용이 가파르게 증가한다 — 단일 worker failure가 수백 개의 healthy worker를 되돌릴 수 있다 (p.2). Table 1(p.4)은 CheckFreq/Gemini/MoC-System이 (low overhead & high frequency / fast recovery / full recovery / high ETTR) 네 항목 중 일부만 만족하고 MoEvement만 네 항목을 모두 충족함을 보인다.

> [!quote]- 📄 원문 표현 (paper)
> - "As large language models scale, training them requires thousands of GPUs over extended durations—making frequent failures an inevitable reality." (p.1)
> - "Meta projects an MTBF as low as 14 minutes for jobs utilizing 131,072 GPUs [40], underscoring an emerging reality: at scale, failure is not an anomaly—it is the norm." (p.1)
> - "As shown in Figure 1b, Gemini's ETTR peaks at 0.93 (2-hour MTBF) but is limited to at most 0.79 at 30-minute MTBF, plunging further to 0.47 at 10-minute MTBF." (p.1-2)
> - "during recovery, experts without recent checkpoints revert to stale parameters, causing token loss and breaking synchronous training semantics." (p.4)

## 핵심 통찰 (Key Insight)
MoEvement는 MoE 고유의 token-to-expert assignment 패턴을 활용한다. 첫째, token 분포가 동적이고 skew되어 있다: 대부분의 step에서 거의 모든 expert가 active(at least one token)이지만(DeepSeek-16.4B/64E에서 ≥62/64 expert가 ≈9.2K/10K iteration에서 활성), token share는 크게 변동한다 (Fig 4, p.3). 따라서 일부 expert만 checkpoint하면 token loss 위험이 있어 모든 operator를 한 window 안에 적어도 한 번은 스냅샷해야 하되, 모든 operator를 한 iteration에 동시에 checkpoint할 필요는 없이 여러 iteration에 걸쳐 부분집합씩 점진적으로 스냅샷할 수 있다 (p.3).

둘째, 학습의 iterative 특성상 operator의 full training state(model weight + optimizer state)를 더 이른 checkpoint에서 micro batch를 replay하여 재구성할 수 있다. 핵심 통찰은 mixed-precision(FP16-FP32) 학습에서 operator의 iteration $t$ FP32 state가 더 이른 iteration $s$($s<t$)의 FP32 state와 그 이후 적용된 parameter update에만 의존한다는 점이다 — 따라서 FP16 weight만으로도 필요한 input gradient를 계산할 수 있어, 아직 update하지 않은 operator의 FP32 state를 즉시 요구하지 않고도 재구성이 가능하다 (p.6).

> [!quote]- 📄 원문 표현 (paper)
> - "First, token distribution across experts is dynamic and skewed: nearly all experts are active (assigned at least one token) in most steps (≥ 62/64 in ≈9.2K/10K iterations), yet token shares fluctuate widely (Fig. 4)." (p.3)
> - "The key insight is that an operator's state at iteration t depends entirely on its FP32 state at some earlier iteration s (s < t) and the parameter updates applied since." (p.6)
> - "Crucially, replaying does not require FP32 states for operators not yet updating parameters; FP16 weights alone suffice to compute necessary input gradients." (p.6)

## 설계 / 메커니즘 (Design)
시스템 아키텍처(Fig 3, p.5)는 MoEvement Coordinator와 각 worker의 MoEvement Agent(Checkpointing / Sparse-to-Dense Conversion / Upstream Logging)가 DeepSpeed 위에서 동작하며 host memory에 Sparse Checkpoint·Activation Log·Gradient Log를 둔다. 구현은 DeepSpeed v0.16 위 약 2K LoC 추가, per-operator granularity, GPU→CPU 전송은 dedicated CUDA stream에서 asynchronous `cudaMemcpyAsync`로 forward/backward pass와 overlap된다 (p.5, p.8, p.10).

**(1) Sparse Checkpointing (§3.2 / §3.5)**: 전체 학습 state를 한 iteration에 checkpoint하지 않고, operator 부분집합을 $W_{sparse}$ iteration에 걸쳐 점진적으로 스냅샷한다. 각 iteration에서 소수 operator만 checkpoint하며, _active_ operator는 full FP32 master weight+optimizer state를, 나머지(_frozen_)는 compute weight(FP16-FP32 mixed-precision에서 Adam 기준 파라미터당 2 byte vs 12 byte로 83% 작음)만 일시 저장한다 (p.4). checkpoint window $W_{sparse}$와 operator를 checkpoint하는 순서 두 파라미터를 함께 최적화한다(`SparseCheckpointSchedule`, Algorithm 1, p.7). `FindWindowSize()`는 모든 operator를 active로 시작해 점진적으로 일부를 frozen으로 바꾸며, 추정 snapshot time이 iteration time 안에 들어가는 가장 작은 $W_{sparse}$를 선택해 학습이 stall하지 않게 한다(복잡도 $O(|O|\log|O|)$, CPU에서 ≈0.1초, GPU 계산과 async 실행). operator 순서는 expert popularity(activation frequency)의 오름차순으로 정렬해(`OrderOperators()`), popular expert의 checkpointing을 window 내 뒤쪽으로 미루어 frozen 상태를 더 오래 유지하고 recomputation cost를 active 대비 약 33% 낮춘다 (p.7). per-iteration checkpoint size를 dense 대비 약 55% 줄여 checkpointing I/O를 계산과 완전히 overlap하고 stall을 제거한다 (Fig 6 Inset / Fig 5b, p.4). snapshot은 r개 peer node(default $r=2$)로 asynchronous하게 replicate되며, 모든 snapshot이 durable해야 "persisted"로 간주, 항상 persisted 1개 + in-flight 1개를 유지하고 새 것 persist 후 가장 오래된 것을 GC한다 (p.4, p.10). → Challenge #1 해소.

**(2) Sparse-to-Dense (Checkpoint) Conversion (§3.3)**: sparse checkpointing은 stall을 피하지만 서로 다른 iteration에 찍힌 스냅샷 사이에 temporal inconsistency를 낳는다(예: S-CKPT[10,13]에서 $E_1,E_2$는 iteration 10, $E_3,E_4$는 11, NE,G는 12에 찍힘). conversion은 여러 sparse 스냅샷에서 micro batch를 replay하고 점진적으로 update를 적용해 logically consistent dense checkpoint를 복원한다. operator는 FP32 state 가용 여부로 분류된다: _active_ operator는 FP32 weight+optimizer state를 갖고 forward/backward/optimizer update를 수행하고, _frozen_ operator는 FP16 weight만으로 forward+input-gradient만 수행하며 weight-gradient 계산과 optimizer update는 나중 _anchor_ snapshot이 FP32 state를 제공할 때까지 skip한다 (Fig 7, p.6). snapshot을 schedule 순서대로 load할수록 operator가 frozen→active로 전이하며, 마지막 snapshot load 후 dense checkpoint가 traditional dense checkpointing과 동일하게 복원된다 (Fig 8, iteration 11~13, p.7). 이로써 token loss 없이 synchronous semantics·accuracy·reproducibility를 보존한다. → Challenge #2 해소.

**(3) Upstream Logging (§3.4)**: dense checkpointing에선 한 worker의 failure가 모든 stage의 rollback을 유발해 non-faulty worker까지 재계산하게 만들고(cascaded rollback), 1F1B 스케줄에서 pipeline bubble을 유발해 recovery를 길게 만든다. MoEvement는 각 pipeline-stage boundary에서 (1) forward 시 downstream으로 전달되는 activation과 (2) backward 시 upstream으로 보내지는 gradient를 sender stage에서 host(CPU) memory에 logging한다. logging은 sender에서 local하게 일어나 추가 network 통신이 없고, upstream/downstream worker가 죽어도 접근 가능하다. 각 log는 iteration 번호·micro batch id로 tagged된다 (p.7). failure 시 모든 DP group을 멈추고 현재 iteration을 abort, 고장 worker를 spare로 교체한 뒤, 영향받은 data-parallel(DP) group만 가장 최근 sparse checkpoint로 roll back(보통 몇 iteration)하고, 실패한 stage가 upstream/downstream neighbor에 저장된 log로 local replay해 global recomputation 없이 consistent dense checkpoint를 복원한다. $W_1$만 실패할 때 startup/cool-down phase의 pipeline bubble을 피해 recovery latency를 23% 줄인다 (Fig 9b, p.7-8). obsolete해진 log는 새 sparse checkpoint가 persist되면 proactively garbage collect하며, host(CPU) memory의 2% 미만을 차지한다 (Table 6, p.8). concurrent/cascading failure는 인접 worker를 contiguous segment로 묶은 joint localized recovery로 처리한다(Appendix A, p.13). → Challenge #3 해소.

**복구 보장(§3.6)**: dense 기법은 $R$이 checkpoint interval로 bound되고 $\mathbb{E}[R] \approx \frac{1}{2} Ckpt_{interval} \cdot T_{iter}$이다. MoEvement는 두 phase(최근 sparse checkpoint에서 $W_{sparse}$ iteration replay로 dense 복원 + 최대 $W_{sparse}$ 추가 replay)로 $0 \le R \le 2 \times W_{sparse} \times T_{iter}$, $\mathbb{E}[R] \approx \frac{3}{2} \times W_{sparse} \times T_{iter}$를 달성한다. 경험적으로 $W_{sparse} \ll Ckpt_{interval}$이라 dense 대비 최대 26배 더 자주 checkpoint한다 (p.8).

> [!quote]- 📄 원문 표현 (paper)
> - "MoEvement introduces three key ideas. First, Sparse Checkpointing §3.2 incrementally captures subsets of operators across multiple iterations instead of checkpointing the full training state at once." (p.5)
> - "Second, Sparse-to-Dense Checkpoint Conversion §3.3 resolves the temporal inconsistency of sparse snapshots by incrementally reconstructing a logically consistent dense checkpoint—selectively replaying computations and activating operators as their master weights and optimizer state become available." (p.5)
> - "MoEvement avoids cluster-wide rollback with Upstream Logging, a lightweight mechanism that restricts rollback to only the affected data-parallel group." (p.7)
> - "On failure, only the affected data-parallel group rolls back to its most recent sparse checkpoint—typically just a few iterations—and completes sparse-to-dense conversion directly from the stored logs." (p.2)

## 평가 (Evaluation)
**Setup(§5.1)**: 주로 12개 Standard_NC96ads_A100_v4 node(node당 64-core AMD EPYC 7V13, 880GB RAM, 8개 A100 80GB GPU, Azure cloud)에서 평가. 같은 node 내 GPU는 600 GB/s NVLink, node 간은 8 NIC를 통한 80 Gbps. 원격 storage는 Azure Blob Storage(40 Gbps). Baseline은 Gemini(SOTA in-memory), CheckFreq(disk), MoC-System(MoE-specific). 모델은 MoE-LLaVa(4E), GPT-MoE(32E), QWen-MoE(64E), DeepSeek-MoE(64E, 16.4B total/3.7B active) 4종(Table 2, p.8). MTBF를 2시간~10분으로 바꾸며 12시간 학습 (p.8).

**Controlled failures (Table 3, p.9)**: MoEvement는 매 iteration checkpoint하면서도(window $W_{sparse}$ 내에서 operator 부분집합을 점진 스냅샷) per-iteration overhead를 ≤2% slowdown으로 유지한다. 반면 DeepSeek-MoE에서 MoC는 MTBF=10M일 때 최대 470% overhead까지 증가한다. recovery 시 rollback을 영향받은 DP group에 한정하고 recomputation을 $\lceil \frac{3}{2}W_{sparse}\rceil$ iteration으로 제한해, DeepSeek-MoE에서 MTBF=10M일 때 recovery time이 117–241초에 그친다(CheckFreq 9402–13086초, Gemini 3236–4627초 대비). 결과적으로 ETTR을 MTBF=10M에서도 0.951–0.964로 유지하며(CheckFreq 0.672, Gemini 0.728, MoC는 0.134까지 추락), recovery는 CheckFreq 대비 최대 31배, MoC 대비 18배 빠르고 token loss·synchronous semantics 손실이 없다. MTBF=10M에서 MoEvement는 CheckFreq보다 1.4배, Gemini보다 1.3배, MoC보다 7.1배 빠르게 학습을 완료한다 (p.9).

**Dynamic failures (Fig 10, p.10)**: GCP 인스턴스의 6시간 failure trace(24 failure event, 평균 MTBF ≈19분)를 replay하면, MoEvement가 가장 높은 goodput을 유지하며 CheckFreq·Gemini·MoC 대비 각각 1.25배·1.15배·1.98배 높은 goodput을 낸다. MoC는 lost token budget이 소진되며 snapshot당 checkpoint하는 expert 비율을 12.5%→100%로 올려 goodput이 71% 감소한다 (Fig 10c/10d, p.10).

**Scalability (Fig 11, p.10)**: simulator(Azure 측정 대비 최대 1.47% 편차로 검증, Table 4)로 DeepSeek-MoE를 32B/67B/145B/671B(512~16,384 GPU, 최대 64 pipeline stage)로 확장 평가. 671B 모델에서 10-minute MTBF일 때 MoEvement의 ETTR은 0.86으로, Gemini의 0.55 대비 1.55배 빠른 학습 (p.10).

**Accuracy (Fig 12 / Table 5, p.11)**: DeepSeek-MoE를 10K iteration 학습하며 2K/4K/6K/8K에 fault 주입. MoEvement와 Gemini는 fault-free DeepSpeed baseline의 validation loss를 잘 추종하나 MoC는 첫 두 failure 후 loss spike. downstream task(PIQA, HellaSwag, TriviaQA, NaturalQuestions)에서 MoEvement는 fault-free DeepSpeed·Gemini와 comparable accuracy를 내며 MoC는 consistently underperform (p.11).

**Breakdown (Fig 13, p.11)**: sparse checkpointing만으로 ETTR 0.37–0.57, frozen operator의 $B_{weight}$·optimizer update skip으로 conversion recovery cost ≈33% 감소, popularity-based reordering으로 64-expert 모델(QWen/DeepSeek) ETTR +32%·GPT-MoE(32E) +15%·MoE-LLaVa(4E) 변화 없음, upstream logging으로 모든 모델 ETTR ≈0.97(DeepSeek-MoE는 12-stage pipeline으로 +50% 최대 이득). 추가 GPU memory overhead는 없고 CPU memory 증가는 Gemini 대비 최대 17.2%로 가용 10TB CPU memory의 ≤2% (Table 6, p.13).

**Low-precision (Table 7, p.12)**: FP8 datatype 포함 5개 low-precision 구성에서 MoEvement는 모든 precision·MTBF에서 1–2% overhead로 ETTR 0.94–0.98을 유지하며, dense baseline 대비 최대 8배 end-to-end 학습 가속을 제공한다 (p.12).

> [!quote]- 📄 원문 표현 (paper)
> - "In contrast, MoEvement maintains high ETTR (0.951–0.964 at MTBF=10M) by effectively combining high frequency checkpointing with low runtime overhead." (p.9)
> - "MoEvement preserves all tokens and synchronous training semantics like CheckFreq and Gemini, yet recovers up to 31× and 18× faster, respectively." (p.9)
> - "MoEvement maintains the highest goodput throughout the trace, delivering 1.25×, 1.15×, and 1.98× higher goodput than CheckFreq, Gemini, and MoC-System, respectively." (p.10)
> - "In contrast, MoEvement maintains low and stable overhead (1–2%), consistently achieving ETTR of 0.94–0.98 across all precisions and MTBFs." (p.12)

## 섹션 노트 (Section notes)
- **§1 Introduction**: failure가 norm이라는 동기, dense 겨냥 checkpointing의 한계, MoEvement 3대 기법과 핵심 수치(overhead 4배·recovery 31배·ETTR≥0.94·8배 speedup) 소개 (p.1-2).
- **§2 Background & Related Work**: MoE 모델·distributed training(DP/TP/PP/EP 4가지 parallelism)·checkpointing 기법(CheckFreq, Gemini, MoC-System의 PEC, checkpoint-less Bamboo/Oobleck/ReCycle 등) 정리. Table 1(p.4)이 기존 기법의 한계를 비교 (p.2-5).
- **§3 The MoEvement Approach**: §3.1 overview, §3.2 sparse snapshot으로 stall 회피, §3.3 sparse-to-dense conversion, §3.4 upstream logging, §3.5 sparse checkpointing policy(Algorithm 1), §3.6 recovery guarantee. ETTR 근사식 $\approx \frac{1}{1+T_{ckpt}/(T_{iter}Ckpt_{interval})} \times \frac{1}{1+\mathbb{E}[R]/MTBF}$ (p.5).
- **§4 Implementation**: DeepSpeed v0.16 위 ≈2K LoC, per-operator granularity, pinned host buffer, `torch.no_grad()`로 frozen operator의 autograd tracking skip, `PipelineModule` 확장으로 upstream logging, `torch.distributed` send/recv 복제 (p.8-10).
- **§5 Evaluation**: §5.1 setup, §5.2 controlled failures, §5.3 dynamic failures, §5.4 scalability+simulator, §5.5 accuracy, §5.6 breakdown, §5.7 low-precision (p.8-12).
- **§6 Conclusion**: 3대 기법으로 runtime-recovery tradeoff를 깨고 correctness-efficiency tension 해소, global recomputation 제거, ETTR≥0.94(MTBF 10분)와 8배 speedup을 accuracy 손실 없이 달성 (p.12).
- **Appendices**: A(concurrent/cascading failure에서의 joint localized recovery), B(alternative operator ordering: soft-count, time-decayed, capacity-aware), C(ETTR estimating simulator) (p.13).

## 핵심 용어 (Key terms)
- **MoE (Mixture-of-Experts) / expert / gating(G)**: dense feed-forward layer를 여러 병렬 subnetwork(expert)로 대체하고, learned gating network가 각 token을 소수(보통 1~2개) expert로 routing하는 아키텍처. 계산은 sublinear로 두고 파라미터 수를 키운다 (p.2).
- **sparse checkpointing**: 전체 학습 state를 한 iteration에 찍지 않고 operator 부분집합을 여러 iteration($W_{sparse}$)에 걸쳐 점진적으로 스냅샷해 overhead를 낮추고 high-frequency checkpoint를 가능케 하는 기법 (p.5).
- **sparse-to-dense (checkpoint) conversion**: 서로 다른 iteration에 찍힌 sparse 스냅샷들의 temporal inconsistency를, micro batch replay와 점진적 update로 해소해 logically consistent dense checkpoint를 복원하는 메커니즘 (p.5-6).
- **upstream logging**: pipeline-stage boundary에서 forward activation과 backward gradient를 sender stage의 host memory에 기록해, failure 시 영향받은 DP group만 local replay하도록 rollback scope를 좁히는 기법 (p.7).
- **active / frozen operator**: active는 FP32 master weight+optimizer state를 갖고 forward/backward/optimizer update 모두 수행; frozen은 FP16 weight만으로 forward+input-gradient만 수행하고 weight-gradient·optimizer update를 anchor snapshot까지 skip (p.6).
- **$W_{sparse}$ / $W_{dense}$ (checkpoint window)**: 모든 operator가 적어도 한 번 스냅샷되는 데 걸리는 iteration 수. dense는 $W_{dense}=1$, sparse는 여러 iteration. `FindWindowSize()`가 stall 없는 최소 값을 선택 (p.7).
- **snapshot**: training state를 GPU에서 local CPU memory로 옮긴 것. r개 peer에 복제 완료 시 "persisted" (p.4).
- **ETTR (Effective Training Time Ratio)**: checkpointing·recovery를 제외한, 유용한 학습에 쓰인 wall-clock time의 비율 (p.1 각주1, p.5).
- **MTBF (Mean Time Between Failures)**: failure 사이 평균 시간. GPU 수가 늘수록 감소하며, ETTR은 runtime overhead와 recovery overhead 두 요소에 의해 결정된다 (p.1, p.5).
- **runtime–recovery tradeoff**: checkpoint interval을 짧게 하면 runtime overhead가 크고, 길게 하면 recovery 시 recomputation이 커지는 내재적 trade-off (p.1, p.5).
- **synchronous training semantics**: 모든 worker가 일관된 state로 동기 진행하며 token loss·재현성 손실이 없는 학습 의미. PEC의 stale parameter revert가 이를 위반한다 (p.2).
- **pipeline-stage boundary**: pipeline parallelism에서 인접 stage 사이 경계. forward activation은 downstream으로, backward gradient는 upstream으로 흐르며 upstream logging이 이 경계에서 기록한다 (p.7).

## 강점 · 한계 · 열린 질문
**강점**
- MoE의 token skew와 학습 iterative 특성을 정면으로 활용해 runtime-recovery tradeoff를 깨고, low overhead(≤2%)와 fast localized recovery를 동시에 달성 (p.9).
- correctness-efficiency tension을 sparse-to-dense conversion으로 해소해 token loss·synchronous semantics 손실이 없음 — accuracy/downstream task에서 fault-free baseline과 comparable (Fig 12, Table 5, p.11).
- upstream logging으로 global rollback을 제거하고 scale이 커질수록 더 유리(671B에서 Gemini 대비 1.55배, p.10). low-precision(FP8 포함)까지 robust (Table 7, p.12).
- DeepSpeed 위 ≈2K LoC로 실용적 구현, GPU memory overhead 0, CPU memory 추가가 ≤2%로 modest (p.10, p.13).

**한계 / 열린 질문**
- ordering 이점이 expert popularity skew에 의존 — expert 수가 적은 모델(MoE-LLaVa 4E)에서는 popularity reordering 이득이 없고 shallow pipeline으로 upstream logging 이득도 작음 (p.11).
- mixed-precision(FP32 master + FP16 compute) 가정에 설계가 묶여 있음(저정밀로 확장은 했으나 FP32 anchor 개념에 의존).
- scalability와 large-model 결과(32B~671B)는 실제 수천 GPU 클러스터가 아니라 simulator 기반(최대 1.47% 편차로 검증되긴 했으나 실측 아님, 실측은 12노드 한정) (p.10).
- failure 복구 시 다른 DP group은 paused 상태로 일관성을 유지 → 완전 비동기 복구는 아니며, joint/cascading recovery는 slowest group이 좌우한다(Appendix A, p.13).
- 추가 host memory(activation/gradient log + frozen operator의 FP16 weight) 필요 — host RAM이 작은 환경에서 제약 가능. CXL/persistent memory substrate로 snapshot·log를 offload하는 결합 가능성은 미탐구.

## ❓ Q&A (자가 점검)
> [!question]- Q1. 기존 dense checkpointing 기법이 MoE에 적용될 때 실패하는 세 가지 challenge는?
> (1) Runtime–Recovery Tradeoff: 큰 MoE state로 짧은 interval은 prohibitively expensive, 긴 interval은 recovery가 길어 ETTR 급락. (2) Correctness–Efficiency Tension: PEC처럼 일부 expert만 찍으면 recovery 시 stale parameter로 token loss·synchronous semantics 위반. (3) Global Rollback Scope: 단일 worker failure가 모든(healthy 포함) worker를 roll back시켜 recomputation이 scale에 따라 가파르게 증가 (p.1-2).

> [!question]- Q2. MoEvement의 세 가지 핵심 기법은 각각 어떤 challenge에 대응하는가?
> sparse checkpointing → Challenge #1(runtime-recovery tradeoff)을 low-overhead high-frequency checkpoint로 해소; sparse-to-dense conversion → Challenge #2(correctness-efficiency tension)를 일관된 dense checkpoint 복원으로 해소; upstream logging → Challenge #3(global rollback scope)를 localized recovery로 해소 (p.2, p.5).

> [!question]- Q3. sparse checkpointing은 어떻게 stall 없이 checkpoint하는가?
> 모든 operator를 한 iteration에 찍지 않고 $W_{sparse}$ iteration에 걸쳐 operator 부분집합만 점진 스냅샷한다. active는 FP32 master weight+optimizer state를, frozen은 FP16 weight(83% 작음)만 저장해 per-iteration checkpoint size를 약 55% 줄이고, `FindWindowSize()`로 snapshot time이 iteration time 안에 들어가는 최소 $W_{sparse}$를 선택해 checkpointing I/O를 계산과 완전히 overlap한다 (p.4, p.7).

> [!question]- Q4. sparse 스냅샷의 temporal inconsistency는 무엇이고 어떻게 해소되는가?
> 서로 다른 operator가 서로 다른 iteration에 찍혀(예: $E_1,E_2$@10, $E_3,E_4$@11, NE,G@12) state가 불일치한다. conversion은 mixed-precision에서 operator state가 더 이른 FP32 state + 이후 update에만 의존한다는 통찰로, frozen operator는 FP16 weight로 forward+input-gradient만 수행하다 anchor snapshot이 FP32 state를 제공하면 active로 전이시키며 micro batch를 replay해 일관된 dense checkpoint를 복원한다 (p.6).

> [!question]- Q5. upstream logging은 무엇을 어디에 기록하고 왜 network overhead가 없는가?
> 각 pipeline-stage boundary에서 forward의 downstream activation과 backward의 upstream gradient를 sender stage의 host(CPU) memory에 기록한다. logging이 sender에서 local하게 일어나므로 추가 network 통신이 없고, upstream/downstream worker가 죽어도 log에 접근 가능하다. 각 log는 iteration·micro batch id로 tagged된다 (p.7).

> [!question]- Q6. MoEvement의 recovery time bound는?
> $0 \le R \le 2 \times W_{sparse} \times T_{iter}$이고 expected recovery time $\mathbb{E}[R] \approx \frac{3}{2} \times W_{sparse} \times T_{iter}$. 경험적으로 $W_{sparse} \ll Ckpt_{interval}$이며 dense 대비 최대 26배 더 자주 checkpoint한다 (p.8).

> [!question]- Q7. controlled failure 평가에서 핵심 수치는?
> MTBF=10M에서 ETTR 0.951–0.964 유지, recovery는 CheckFreq 대비 최대 31배·MoC 대비 18배 빠름, 학습은 CheckFreq보다 1.4배·Gemini보다 1.3배·MoC보다 7.1배 빠르게 완료. per-iteration overhead ≤2% (Table 3, p.9).

> [!question]- Q8. MoC-System은 dynamic failure trace에서 왜 성능이 떨어지는가?
> MoC는 lost token budget이 소진되면서 snapshot당 checkpoint하는 expert 비율을 12.5%에서 100%로 점진 증가시켜 accuracy를 지키려 하고, 그 결과 goodput이 71% 감소한다. MoEvement는 token loss 없이 가장 높은 goodput을 유지(MoC 대비 1.98배) (Fig 10, p.10).

> [!question]- Q9. MoEvement가 추가로 쓰는 메모리는?
> GPU memory overhead는 없고(no GPU memory overhead), 추가 CPU memory는 (i) anchor snapshot을 기다리는 frozen operator의 FP16 compute weight와 (ii) localized recovery용 activation/gradient log에서 발생한다. Gemini의 dense checkpoint 대비 최대 17.2% 증가지만 가용 10TB CPU memory의 ≤2%이다 (Table 6, p.13).

## 🔗 Connections
[[LLM Systems]] · [[NSDI]] · [[2026]]
관련: [[Smart-Infinity]] · [[SkyByte]] · [[XHarvest]]

## References worth following
- **[8] MoC-System (ASPLOS '25)** — Partial Expert Checkpointing(PEC). 주 비교 대상이자 Challenge #2(correctness-efficiency)의 반례. MoE-specific checkpointing의 token-loss 문제를 이해하는 핵심 (p.2, References p.1407).
- **[82] GEMINI (SOSP '23)** — in-memory checkpoint SOTA baseline. runtime-recovery tradeoff(Challenge #1)의 기준선 (p.1, References p.1411).
- **[56] CheckFreq (FAST '21)** — two-phase pipelined, disk-based checkpointing baseline. checkpoint-induced stall 개념의 출처 (p.2, References p.1410).
- **[13] Daly (2006)** — optimal checkpoint interval 이론($\mathbb{E}[R]\approx\frac{1}{2}$ interval). recovery cost·ETTR 모델링 근거 (p.5, References p.1407).
- **[12] DeepSeek-MoE (arXiv:2401.06066)** — 평가 모델이자 expert activation dynamics(Fig 4)의 근거 (p.2, References p.1407).
- **[17] ReCycle (SOSP '24)** — 동일 저자의 pipeline adaptation 기반 resilient training. upstream logging·localized recovery 아이디어 계보 (p.5, References p.1407).

## Personal annotations

