---
title: "Teaching The Old Dog New Tricks: Building Efficient Data Pipelines for Large-Scale LLM Pre-training (Operational Systems)"
description: "ByteDance 프로덕션 30일치 트레이스 분석으로 LLM 사전학습 데이터 파이프라인의 세 병목(cross-DC companion evaluation, 초기화 I/O storm, 멀티모달 transformation wall)을 규명하고, workload determinism을 노출하는 소프트웨어 정의 최적화(predictive checkpoint replication, proactive hotspot prediction, storage-side transformation offloading)로 legacy HDFS 위에서 해결한 논문"
venue: OSDI
year: 2026
award: "Best Paper"
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - award/best-paper
  - cluster/llm
  - venue/osdi
  - year/2026
  - list/26s-v2
  - topic/checkpointing
  - topic/storage-systems
  - topic/data-loading
  - topic/multimodal-training
---

# Teaching The Old Dog New Tricks: Building Efficient Data Pipelines for Large-Scale LLM Pre-training (Operational Systems)

> **OSDI 2026** · cluster/llm · Source: [Building Efficient Data Pipelines for Large-Scale LLM Pre-Training.pdf](<Building Efficient Data Pipelines for Large-Scale LLM Pre-Training.pdf>)

저자: Luofan Chen, Chenhan Wang (공동 1저자, University of Science and Technology of China & ByteDance Seed), Weidong Zhang, Jinxin Chi, Hequan Zhang, Zanbo Wang, Chenyuan Wang, Lishu Luo, Sijin Wu, Junqi Hu, Jun Wang, Cheng Chen (ByteDance Seed), Lixin Huang, Liyang Zhao, Yong Tian, Jun Guo (ByteDance), Youhui Bai (USTC, corresponding), Wencong Xiao (ByteDance Seed), Kang Chen (Tsinghua University), Cheng Li (USTC & Institute of Artificial Intelligence, Hefei Comprehensive National Science Center, corresponding)

## TL;DR
ByteDance의 대규모 LLM 사전학습 프로덕션 환경(수천~2만 GPU, 30일간 3만 개 job trace)을 정량 분석해, 데이터 파이프라인에 숨어 있던 세 가지 병목 — companion evaluation의 cross-DC 지연, job 재시작 시의 initialization I/O storm, 멀티모달 학습의 transformation wall — 을 규명한다. 저자들은 스토리지가 reactive하게 동작하는 반면 트레이닝 workload는 사실상 deterministic하다는 통찰에서 출발해, 이 결정성을 스토리지 계층에 노출하는 세 가지 소프트웨어 정의 최적화(predictive checkpoint replication, proactive hotspot prediction, storage-side transformation offloading)를 제안한다. 이 최적화들은 legacy Big Data 시스템인 HDFS를 교체하지 않고도 적용 가능하며, evaluation당 낭비 GPU 시간을 16,800시간에서 4,000시간으로, 체크포인트 로딩 시간을 40.8%, 데이터 로딩으로 인한 학습 stall을 63.2% 줄인다. Vertical co-design(training framework ↔ storage tier)이 아키텍처 전면 교체 없이도 엑사바이트급 학습 요구를 충족할 수 있음을 실증한다.

## 문제 & 동기
저자들은 LLM 사전학습 파이프라인을 storage tier와 dataloader 두 컴포넌트로 정의하고(Fig.1, p.350), pre-training 실행을 initialization / iterative training / companion evaluation 세 단계로 characterize한다(p.350). 30일간 수집된 3만 개 job trace 중 GPU 시간의 70%를 차지하는 대표 5개 workload(T-S/T-L/MM-S/MM-L/MM-O, Table 1, p.351)를 분석해 세 개의 underreported 병목을 발견했다: (1) companion evaluation이 원격 데이터센터의 checkpoint를 fetch할 때 발생하는 cross-DC latency trap, (2) job 재시작 시 소수 hot file에 수천 rank가 동시 접근하며 발생하는 initialization I/O storm, (3) 멀티모달 학습에서 비디오/이미지 디코딩 같은 CPU-intensive transformation이 host CPU 처리 속도를 초과해 GPU를 idle시키는 transformation wall(p.349-350). 이 병목들이 방치되는 근본 이유는 "스토리지 시스템은 reactive하게 동작하지만 트레이닝 workload는 본질적으로 deterministic"하기 때문이라고 주장한다(p.350).

> [!quote]- 📄 원문 표현 (paper)
> - "First, cross-datacenter (cross-DC) traffic emerges as a major source of latency when evaluating in-training models using remote checkpoints." (p.349)
> - "We argue that these bottlenecks persist because storage systems operate reactively while training workloads are inherently deterministic." (p.350)
> - "Together, these techniques reduce wasted GPU hours per evaluation from 16,800 to 4,000, shorten checkpoint loading time at each training start by 40.8%, and reduce training stalls caused by data loading by 63.2%." (p.349)

## 핵심 통찰 (Key Insight)
1. **Workload determinism을 스토리지에 노출하면 reactive → proactive 전환이 가능하다.** Evaluation 주기, checkpoint 저장 스케줄, dataloader의 global execution plan(샘플 순서)은 모두 job 제출 시점에 이미 알려진 결정적 정보다. 이 정보를 storage layer에 명시적으로 전달(hint)하면, 스토리지가 heat를 "감지"하기 전에 미리 replication/prefetch를 수행할 수 있어 hotspot이 생기기도 전에 문제를 없앨 수 있다.
2. **작은 텐서/글로벌 hot file로의 조각화가 cross-DC 지연과 I/O storm의 공통 근본 원인이다.** Checkpoint는 수천 개의 작은 tensor(LayerNorm, bias 등)로 구성되며 60% 이상이 16KB 미만(Fig.3, p.354)이라, cross-DC WAN에서는 RTT-bound throughput이 되고, 로컬 재시작 시에는 소수 replica에 수천 rank가 몰리는 "1-to-many" 병목이 된다. 두 문제 모두 replication 배치와 batching으로 완화 가능하다.
3. **스토리지 노드의 유휴 CPU가 host의 transformation 병목을 흡수할 수 있는 이미 지불된 자원이다.** Training host는 GPU 소비 속도를 못 따라가는 반면 storage node CPU는 20-30%만 사용 중(p.359). Deterministic dataloader가 미래 접근 패턴을 정확히 예측할 수 있으므로, storage tier를 "Disaggregated Pre-processing Engine"으로 재정의해 transformation을 training과 파이프라이닝할 수 있다.

> [!quote]- 📄 원문 표현 (paper)
> - "Contrary to common assumptions that metadata operations (e.g., open, getattr) are the bottleneck in distributed file systems, our analysis reveals that data contention is the primary culprit." (p.356)
> - "This opportunity arises from a fundamental I/O-compute mismatch in our cluster. We observe that while training nodes are Compute/Memory saturated, storage nodes are typically bounded by HDD seek latency or Network Interface Card (NIC) limits, leaving significant compute capacity idle." (p.359)

## 설계 / 메커니즘 (Design)

**① Cross-DC Companion Evaluation 최적화 (§3.3, p.354).** Companion evaluation은 checkpoint merging(원격 shard를 modality별 safetensors로 병합) → reshard → evaluate 세 단계로 진행되며(Fig.1 하단, p.350), merging 단계가 evaluation I/O 시간의 84.8%를 차지한다(p.354). 두 메커니즘을 도입한다: (a) **Predictive Checkpoint Replication** — evaluation이 약 1,000 step마다 규칙적으로 실행된다는 점을 이용해, checkpoint가 저장되는 즉시 pipelined replication을 시작하고 tensor-granularity WAN read 대신 큰 contiguous shard를 evaluation cluster 로컬에 미리 캐싱한다. 경량 server-side namespace 서비스 **NNProxy**가 evaluation job을 이 로컬 캐시로 라우팅한다. (b) **Signal-driven prioritization** — loss spike 등 이상 신호로 트리거된 긴급 evaluation에는 model scale·task importance·anomaly severity를 결합한 priority signal로 네트워크 대역폭을 선점 배분한다.

**② Proactive Hotspot Prediction (§4.3, p.356-357).** Initialization 단계에서 트레이닝 프레임워크가 `SetReplicationHints` 인터페이스로 global metadata(world size N에 비례)와 replicated tensor(concurrency degree $\omega_f$)의 hot file 목록을 스토리지에 사전 통지한다. 스토리지는 다음 heuristic으로 replica 수를 결정한다(식 1, p.357):
$$R_{target}(f) = \left\lceil \frac{\omega_f}{C_{load}} \right\rceil$$
여기서 $C_{load}$는 단일 replica의 안전 concurrent load capacity다. 이 ephemeral replica는 TTL(Time-To-Live) 태그로 관리되어 startup phase가 끝나면 자동 회수된다. 프로덕션 가이드라인은 10k-GPU 클러스터에 64 replicas, 20k-GPU 클러스터에 128 replicas(p.357).

**③ Storage-Side Transformation Offloading — Pushdown Transformation Engine (§5.1, §5.3, p.358-360).** 먼저 **Deterministic Dataloader**를 구축한다: 원시 멀티모달 데이터를 20GB 단위 consolidated binary bin에 저장하고(Fig.10, p.358), 샘플 식별자를 bin 내 byte offset/length로 변환하는 metadata map, 그리고 매 epoch의 샘플 읽기 순서를 오프라인에서 고정하는 per-step info file을 사용해 global shuffling을 물리적 데이터 이동 없이 수행한다. 이 결정성 위에서 storage node는 (a) **Schedule synchronization**으로 dataset identifier·step 진행 상황을 공유받아 다음 데이터 블록 순서를 스스로 예측하고, (b) **Just-in-Time (JIT) transformation**으로 Consumer Queue를 유지하며 GPU가 Step N을 계산하는 동안 Step N+1의 decoding/cropping/normalization을 미리 수행하며(비디오는 frame sampling도 적용), (c) **Load-aware fallback**으로 storage node CPU 사용률이 안전 임계치(예: 80%)를 넘으면 transformation을 중단하고 raw byte를 반환, dataloader가 tensor/binary 포맷을 감지해 로컬 처리로 fallback한다.

> [!quote]- 📄 원문 표현 (paper)
> - "A lightweight server-side namespace service, NNProxy, then directs evaluation jobs to read from these local cached shards when available." (p.354)
> - "We propose a cooperative mechanism in which the training framework explicitly informs the storage system about upcoming access patterns via a SetReplicationHints interface." (p.356)
> - "We redesign the storage layer to act not just as a data repository, but as a Disaggregated Pre-processing Engine." (p.360)

## 평가 (Evaluation)

**Cross-DC 최적화 (§3.4, p.355):** 30일 관측 창(19개 pre-training task, 3,589회 companion evaluation, 156건의 critical model regression 탐지, p.353)에서 checkpoint merging latency가 평균 76.1% 감소, T-S 모델은 최대 89.3%, MM-L은 70.8% 개선. 이로써 I/O로 낭비되던 GPU 시간이 16,800시간→4,000시간(evaluation당)으로 줄고, 총 약 2백만 GPU 시간을 회수(p.355).

**Proactive Hotspot Prediction (§4.4, p.357):** 2,048-GPU 통제 실험에서 예측된 hot file(global metadata + replicated parameter)의 replication factor를 checkpoint recovery 전에 128로 확장. 전체 checkpoint loading 시간이 38.48초→22.78초로 **40.8%** 개선. peak aggregate read QPS 증가 및 hottest file의 straggler read 제거로 average read latency 감소(Fig.9, p.357).

**Storage-Side Transformation Offloading (§5.4, p.360):** P99 데이터 로딩 latency가 **85.7%** 감소, transformation straggler로 인한 학습 stall time이 **63.2%** 감소해 상대적 MFU가 **10.8%** 개선. Training host의 데이터 로딩 CPU 사용량은 **94%** 감소해 host-side 병목을 사실상 제거(p.360). (참고: Table 5, p.359에서 transformation이 MM-L 전체 데이터 로딩 시간의 94.4%(5.05s/5.35s)를 차지함이 offload 필요성의 근거.)

> [!quote]- 📄 원문 표현 (paper)
> - "These training-aware mechanisms reduce average checkpoint merging latency by 76.1%, with improvements reaching 89.3% for the T-S model and 70.8% for the MM-L model." (p.355)
> - "The total checkpoint loading time decreases from 38.48 s to 22.78 s, a 40.8% improvement." (p.357)
> - "Specifically, by offloading heavy lifting to the storage pool, the P99 data loading latency drops by 85.7%, effectively mitigating the straggler effect. In terms of training efficiency, stall time due to transformation stragglers is reduced by 63.2%, translating to a relative 10.8% improvement in MFU." (p.360)

## 섹션 노트
- **§1 Introduction**: 데이터 파이프라인이 더 이상 passive utility가 아니라 성능의 primary determinant임을 주장하며 세 병목과 세 최적화, 기여를 요약.
- **§2 Background and Motivation**: 파이프라인 아키텍처(storage tier + dataloader), 3단계 실행 모델, 데이터 타입(dataset/checkpoint/logits, Table 2), workload 다양성(5개 trace, Table 1)을 정의.
- **§3 Cross-DC Companion Evaluation**: companion evaluation이 gang scheduling과 하드웨어 이질성 때문에 원격 클러스터에서 실행될 수밖에 없는 이유, checkpoint merging이 지배적 비용인 이유(small-I/O latency trap), predictive replication과 signal-driven prioritization의 효과.
- **§4 Initialization I/O Storm**: 2,048-GPU 통제 실험으로 데이터 contention(메타데이터 아님)이 근본 원인임을 규명, embedding vs MoE expert의 replication 비대칭, SetReplicationHints 기반 proactive replica expansion.
- **§5 MM-L Transformation Wall**: deterministic dataloader의 on-disk 포맷(consolidated binary bin/metadata map/per-step info file), transformation이 데이터 로딩의 94.4%를 차지함을 실측, Pushdown Transformation Engine 설계와 load-aware fallback.
- **§6 What If?**: P2P 분산, dedicated transformation cluster, 특화 AI-native storage(3FS/AIStore) 마이그레이션을 각각 검토 후 기각한 이유를 논증 — legacy HDFS 유지가 실용적 선택임을 정당화.
- **§7 Related Work / §8 Conclusion**: PFS(Lustre/GPFS), object store(S3/Azure Blob), AI-native storage(3FS/AIStore), 기존 caching/coordination 연구(Quiver, SiloD, CoordL)와의 차별점을 정리하고, workload determinism 노출을 통한 vertical co-design이 legacy 시스템으로도 엑사바이트급 훈련 요구를 충족시킬 수 있음을 결론.

## 핵심 용어 (Key terms)
- **Companion evaluation**: 훈련과 병렬로 별도 클러스터에서 최신 checkpoint를 주기적으로 로드해 벤치마크를 실행하는 out-of-band 모델 품질 검증 파이프라인.
- **Checkpoint merging**: 분산 학습 중 병렬 저장을 위해 sharding된 checkpoint를 modality별 단일 safetensors로 재구성하는 단계.
- **NNProxy**: predictive replication으로 로컬에 캐싱된 checkpoint shard로 evaluation job을 라우팅하는 경량 server-side namespace 서비스.
- **SetReplicationHints**: 트레이닝 프레임워크가 향후 접근할 hot file 목록을 storage system에 사전 통지하는 인터페이스.
- **TTL 기반 ephemeral replica**: startup I/O storm 대응을 위해 프로액티브하게 생성했다가 startup phase 종료 후 자동 회수되는 임시 replica.
- **Consolidated binary bin**: 멀티모달 원시 데이터(비디오/오디오/이미지/텍스트)를 20GB 단위 대형 연속 파일로 묶어 저장하는 on-disk 포맷.
- **Metadata map**: 샘플 식별자를 binary bin 내 byte offset/length로 변환하는 logical-to-physical translation layer.
- **Per-step info file**: 매 학습 step에서 읽어야 할 샘플 식별자 순서를 오프라인에 미리 생성해두는 파일 — 물리적 데이터 이동 없이 global shuffling을 가능케 함.
- **Pushdown Transformation Engine / Just-in-Time (JIT) transformation**: storage node가 Consumer Queue를 유지하며 다음 step에 필요한 데이터를 미리 디코딩/변환해 training과 파이프라이닝하는 메커니즘.
- **Load-aware fallback**: storage node CPU 사용률이 안전 임계치를 초과하면 transformation을 중단하고 raw byte로 되돌려 client-side 처리로 전환하는 backpressure 기법.

## 강점 · 한계 · 열린 질문
- **강점**: 30일·3만 job의 실제 프로덕션 trace에 기반한 정량 분석으로 병목을 구체적 수치(p.354의 84.8% I/O, p.359의 94.4% transformation 등)로 근거지음. 하드웨어 교체나 스토리지 시스템 전면 교체 없이 HDFS 위에 소프트웨어 정의 계층만 추가해 배포 가능하다는 실용적 강점(§6의 대안 기각 논증이 이를 뒷받침).
- **한계**: 세 최적화 모두 training workload의 determinism(고정된 evaluation 주기, deterministic dataloader의 사전 계산된 접근 순서)에 의존하므로, 비결정적 online RL이나 동적 curriculum 학습 등에는 그대로 적용하기 어려울 수 있음. Storage-side offloading은 storage node에 co-located idle CPU headroom이 존재한다는 가정에 의존하며, 저자들 스스로 cloud object store나 lean storage appliance에는 적용되지 않을 수 있다고 인정(§5.5, p.360). $C_{load}$, CPU 임계치(80%) 등 핵심 파라미터는 경험적 heuristic으로, 클러스터마다 재튜닝이 필요해 보임.
- **열린 질문**: replica expansion과 checkpoint 파일 layout(예: LayerNorm/bias를 별도 contiguous extent로 재배치하는 layout-agnostic format, §3.5 future work)을 실제로 통합하면 얼마나 더 이득이 있는가? Processed-sample caching이나 predictive transform-aware batching(§5.5 future work)은 아직 구현·평가되지 않았는데, repeated-access 워크로드(RL, fine-tuning)에서 실제 효과는 어느 정도인가?

## ❓ Q&A (자가 점검)
> [!question]- 왜 cross-DC checkpoint 병합이 그렇게 느린가?
> Transformer checkpoint는 LayerNorm/bias 등 수천 개의 disjoint tensor로 구성되며 60% 이상이 16KB 미만이다(Fig.3, p.354). Merge 시 tensor-granularity로 WAN read가 발생해 100ms급 RTT에 bound되고, HDFS의 128MB coarse-grained block 최적화와 mismatch가 생겨 read amplification까지 겹친다(p.354).

> [!question]- Predictive Checkpoint Replication은 무엇을 예측해 언제 동작하는가?
> Companion evaluation이 약 1,000 step마다 규칙적으로 실행된다는 스케줄 정보를 이용해, 훈련 시스템이 예정된 간격에 checkpoint 저장을 시작하는 순간 pipelined replication을 함께 시작하고 shard를 evaluation cluster의 로컬 스토리지에 batch로 미리 캐싱한다(p.354).

> [!question]- SetReplicationHints는 어떤 정보를 언제 전달하는가?
> 파티셔닝 전략과 world size로부터 결정적으로 계산 가능한 global metadata·replicated tensor의 hot file 집합과 예상 concurrency degree $\omega_f$를, checkpoint 저장 직후 혹은 job 스케줄 시점에 스토리지에 전달한다(p.356-357).

> [!question]- Replica 수는 어떻게 결정되며 실제 가이드라인은?
> $R_{target}(f) = \lceil \omega_f / C_{load} \rceil$ (식 1, p.357) heuristic으로 결정하며, 프로덕션에서는 10k-GPU 클러스터에 64 replicas, 20k-GPU 클러스터에 128 replicas를 codify했다(p.357).

> [!question]- Embedding과 MoE expert의 checkpoint recovery 병목 양상이 왜 다른가?
> Embedding(복제된 파라미터)은 단일 사본만 저장해 복원 시 수백 rank가 하나의 파일에 몰리는 one-to-many 병목이 생기지만, MoE expert(sharded 파라미터)는 병렬 그룹마다 별도 파일에 저장되어 rank들이 서로 다른 파일에 분산 접근하므로 병목이 완화된다(p.356).

> [!question]- 오프라인(사전) transformation을 왜 채택하지 않았는가?
> 디코딩된 텐서는 원본 압축 포맷 대비 비디오 40-100배, 이미지도 수십 배 storage amplification을 유발해 페타바이트 규모에서 저장 비용이 감당 불가능하며, crop size/frame count/resolution 등 하이퍼파라미터가 바뀌면 전체 데이터셋을 재생성해야 하는 inflexibility 문제가 있다(p.359).

> [!question]- Load-aware fallback은 구체적으로 어떻게 storage node를 보호하는가?
> Storage node CPU 사용률이 안전 임계치(예: 80%)를 초과하면 transformation을 중단하고 raw byte를 그대로 반환하며, training client가 수신 데이터가 tensor인지 binary인지 감지해 필요 시 로컬에서 변환을 이어서 처리한다(p.360).

> [!question]- 논문이 §6에서 기각한 세 가지 대안 아키텍처와 그 이유는?
> (1) P2P 분산(NCCL/UCX): all-to-all traffic이 gang-scheduled 클러스터의 NCCL collective와 경합하고 단일 slow peer가 gang 전체를 지연시키는 probabilistic tail latency 문제. (2) Dedicated transformation cluster: 디코딩된 텐서가 원본 대비 50-100배 커져 네트워크 증폭이 크고, 멀티모달 peak demand에 맞춰 프로비저닝하면 유휴 자원 낭비. (3) 특화 AI-native storage(3FS/AIStore) 마이그레이션: HDFS가 AI 훈련 외 여러 비즈니스 라인의 exabyte 데이터를 호스팅하는 data gravity 때문에 마이그레이션이 운영상 불가능(p.360-361).

## 🔗 Connections
[[LLM Systems]] · [[OSDI]] · [[2026]]
관련: [[Sparse Checkpointing for Fast and Reliable MoE Training]] · [[Tectonic-Shift - A Composite Storage Fabric for Large-Scale ML Training]] · [[PreSto - An In-Storage Data Preprocessing System for Training Recommendation Models]]

## References worth following
- Mohan et al., "CheckFreq: Frequent, fine-grained DNN checkpointing," FAST 2021 [29] — checkpoint frequency와 fault tolerance trade-off를 다뤄 이 논문의 checkpoint replication 설계와 직접 비교 가능.
- Wan et al., "ByteCheckpoint: A unified checkpointing system for large foundation model development," NSDI 2025 [50] — 같은 ByteDance 환경의 checkpoint 시스템으로, 이 논문이 다루는 sharded write 구조(redundancy elimination)의 원출처.
- Murray et al., "tf.data: A machine learning data processing framework," VLDB 2021 [33] — deterministic dataloader가 명시적으로 비교 대상으로 언급하는 기존 dataloader 아키텍처.
- Mohan et al., "Analyzing and mitigating data stalls in DNN training," VLDB 2021 [30] — data loading stall 분석의 선행연구로 CoordL 등 reactive coordination 접근과의 대조점.
- Chen et al., "CrossPipe: Towards optimal pipeline schedules for cross-datacenter training," USENIX ATC 2025 [5] — cross-DC 파이프라인 스케줄 최적화라는 인접 문제를 다뤄 companion evaluation의 storage-side 접근과 상호보완적.

## Personal annotations
<!-- 본인 메모 영역 -->
