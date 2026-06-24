---
title: "OptimStore: In-Storage Optimization of Large Scale DNNs with On-Die Processing"
aliases: [OptimStore]
description: "NAND flash die 내부(on-die)에서 DNN optimizer(weight update) 단계를 직접 처리해 채널 데이터 이동을 제거, baseline SSD offloading 대비 2.8x speedup·3.6x 에너지 절감을 달성한 in-storage 구조"
venue: HPCA
year: 2023
tier: deep
status: done
tags:
  - paper
  - cluster/isc
  - topic/in-storage
  - topic/dnn-training
  - venue/hpca
  - year/2023
---

# OptimStore: In-Storage Optimization of Large Scale DNNs with On-Die Processing

> **HPCA 2023** · `cluster/isc` · Source: [OptimStore - In-Storage Optimization of Large Scale DNNs with On-Die Processing.pdf](OptimStore - In-Storage Optimization of Large Scale DNNs with On-Die Processing.pdf)

**저자**: Junkyum Kim (SAIT, Samsung / KAIST), Myeonggu Kang (KAIST), Yunki Han (KAIST), Yang-gon Kim (KAIST / System LSI, Samsung), Lee-sup Kim (KAIST)

## TL;DR
대규모 DNN 학습에서 Adam 같은 advanced optimizer는 master weight·momentum·variance 등 추가 state로 인해 메모리를 2~3x 더 요구하며, 이를 flash SSD로 offload하면 제한된 I/O bandwidth가 학습 전체를 심하게 느리게 만든다. OptimStore는 weight update(optimizer step)를 **NAND flash die 내부의 per-plane processing element(PE)** 에서 floating-point 연산으로 직접 수행함으로써, controller·interconnect·flash channel을 가로지르는 무거운 데이터 이동을 제거한다. 결과적으로 weight update 단계에서 baseline SSD offloading 대비 평균 **2.8x speedup, 3.6x 에너지 효율 향상**, end-to-end 학습 기준 **2.4x speedup**을 달성한다.

## 문제 & 동기
- 모델 크기는 최근 5년간 5000x 이상 증가했으나 AI accelerator memory는 2.5x만 증가 → "memory capacity wall" (Fig.1, p.1).
- Adam 등 advanced optimizer는 FP32 optimizer state + gradient가 FP16 inference weight 대비 최대 8x 용량을 차지하며, mixed-precision과 결합 시 peak memory가 또 한 단계 증가 (p.2).
- Weight update는 elementwise vector 연산뿐이라 arithmetic intensity가 낮음 → 여러 GPU를 동원해 처리하면 값비싼 GPU가 심하게 underutilize됨 (p.1).
- Deepspeed의 storage offloading은 PCIe 4.0 SSD read bandwidth가 ~7.3GB/s로 DRAM보다 한 차수 느려 비실용적으로 긴 I/O latency를 겪음 (p.1).
- 기존 ISP는 controller 근처(embedded core/별도 로직)에서 연산해 flash channel을 가로지르는 데이터 이동이 병목 → 채널 수에 묶여 SSD의 구조적 병렬성을 못 살림 (p.3).

> [!quote]- 📄 원문 표현 (paper)
> "However, when the model, especially the optimizer, is offloaded to flash, the limited I/O bandwidth severely slows down the overall training process." (p.1, Abstract)
>
> "ODP capability of OptimStore eliminates the heavy data movement that occurs through external interconnect and internal flash channels." (p.1, Abstract)
>
> "the key idea of ISP is to harness the power of aggregated internal bandwidth and reduce data movement over slow external interconnect. However this benefit is limited by the number of channels, hence ISP often fail to fully utilize structural parallelism of SSDs." (p.3)

## 핵심 통찰

> [!note]- 통찰 1 — Weight update는 die 내부에서 끝낼 수 있는 sequential·elementwise 연산
> GPT2(6.7B) fine-tuning 프로파일에서 weight update step이 전체 iteration 시간의 최대 93%, 그중 I/O access가 82%를 차지(Fig.3b, p.4). I/O 패턴은 optimizer state 사이를 가끔 점프하는 것 외엔 일정한 sequential read/write이며(Fig.3c), 가장 작은 tensor도 64MB로 커서 page granularity batch로 잘라도 overhead가 미미하다. 즉 byte addressability가 불필요하고 die 내부에서 완결 가능한 워크로드다.

> [!note]- 통찰 2 — 병목은 controller가 아니라 flash channel의 bus contention
> 내부 latency breakdown에서 read 요청의 19.52%가 채널 bus contention 대기, write의 66.05%가 채널 대기로 소비됨(p.4). I/O가 대규모·sequential·multi-plane일 때 채널 데이터 이동이 주된 병목 → 따라서 controller 근처가 아니라 **flash die level**에 NDP를 적용해야 채널 트래픽 자체를 없앨 수 있다.

> [!note]- 통찰 3 — On-die ECC NAND가 이미 상용화되어 die 내 로직 추가가 현실적
> per-plane ECC 로직을 가진 commercial NAND(예: [42])가 존재하므로, OptimStore의 per-plane PE도 구현 가능하며 면적 overhead는 원본 NAND die의 0.15%에 불과하다(p.5). pSLC + DNN 자체의 error resiliency 덕에 복잡한 ECC 없이 NaN 비교기만으로 충분하다(p.9).

## 설계 / 메커니즘

> [!abstract]- OptimStore 시스템 구성
> CPU·GPU와 함께 동작하는 heterogeneous system. GPU는 forward/backward, CPU는 텐서 마이그레이션·전체 흐름 관리, **OptimStore SSD**는 optimizer step 전담. 구성 요소(Fig.4, p.4): ① PE를 가진 flash die, ② DNN 학습용 경량 FTL(L2P, GC, checkpoint), ③ iteration manager firmware(learning rate scheduler + mixed-precision gradient scaler), ④ ONFI 확장 명령. 흐름: backward에서 GPU가 FP16 gradient 계산→CPU가 FP32 cast 후 flash die로 swap-in→on-die PE가 weight update 후 flash에 write-back→완료 후 새 weight를 CPU로 swap-out→다음 forward를 위해 iteration manager가 상수를 broadcast.

> [!abstract]- On-die PE 마이크로아키텍처
> plane마다 dedicated PE를 배치해 plane-level 병렬성 활용. PE는 subtract-shifter + 2 multiplier + fused multiply adder(FMA)로 구성된 pipelined 구조로, linear vector sum(LVS)·quadratic vector sum(QVS) 형태의 fused 연산과 fast inverse-square-root([63] 알고리즘)를 수행(Fig.5a, p.5). 데이터가 byte/word 단위로 전송되어 32-bit FP 한 입력에 4 transfer cycle이 들므로 PE는 NAND interface 주파수의 1/4로 동작. shared bus로 PE가 remote plane에도 접근 가능.

> [!abstract]- Fused Adam optimizer & 데이터 레이아웃
> NAND의 느린 byte-by-byte 전송 때문에 PE 명령 수를 최소화하는 것이 핵심. Algorithm 1(p.5)은 momentum·variance update에 bias-correction을 결합하고 learning rate scheduler(Line 3~5)·gradient scaling(Line 1~2)까지 fused하여 die 내부에서 floating-point division을 피한다. Adam의 4종 state(gradient/weight/momentum/variance)를 서로 다른 plane에 배치(multistream SSD 유사, Fig.5b). 같은 word line offset의 master weight·momentum·variance 페이지를 하나의 super-page로 묶어 single multi-plane operation으로 read/write. gradient는 임시값이라 cache register에만 둔다. 연산 흐름은 ❶상수 broadcast→❷state read→❸momentum/variance cache 복사→❹fused 연산(LVS·QVS·ISQRT·VMUL·LVS)→❿flash write-back으로 진행(Fig.6, p.6).

> [!abstract]- 시스템 지원: ONFI 명령 · FTL · checkpoint
> 새 NAND 명령 확장: VPP(Volatile Page Program, gradient를 cache register에만 임시 저장), BPR/BPP(Buffered Page Read/Program, data register만 사용), ODC(On-Die Computation: LVS/QVS/VMUL/ISQRT 등, opcode·4 address DQ cycle, Table I p.7). FTL은 optimizer 전용 reservoir를 super-page/super-block granularity로 매핑해 mapping table을 원본의 1/4로 축소; sequential program 덕에 GC write amplification이 1에 근접하고 GC가 forward pass와 overlap. inter-die WL(super-page migration)과 Adam에서 idle plane이 생기므로 intra-die WL을 가끔 수행. model checkpointing은 optimizer tensor가 이미 persistent flash에 있어 PE 결과를 reservoir 밖 블록에 쓰는 방식으로 저렴하게 구현.

## 평가

> [!success]- 성능 (Fig.7, p.8)
> SimpleSSD([13]) 기반 시뮬레이션 + 실제 머신(Xeon 4210R, RTX3090 24GB)으로 forward/backward·CPU update 측정. baseline=Deepspeed NVMe offloading.
> - **Optimizer step**: C-ISP가 baseline 대비 평균 1.5x, OptimStore는 baseline 대비 **2.8x**, C-ISP 대비 **1.9x** speedup. DeepStore는 1.1x에 그침(write/gradient skipping 미지원, p.8).
> - **End-to-end**: forward/backward 포함 시 작은 모델 2.2x→큰 모델 2.5x, OptimStore 평균 **2.4x** speedup (Amdahl: 모델 클수록 optimizer 비중↑).
> - 6B 초과 모델은 CPU offloading이 system memory 부족으로 실행 불가(OOM); 예로 8B Adam 모델은 119GB + FP16 29.8GB 필요(p.3).

> [!success]- 에너지 효율 (Fig.8, p.8-9)
> Optimizer step 에너지: OptimStore **3.6x**, C-ISP 2.0x 감소(짧은 실행시간 + 낮은 CPU 활용). SSD-only 평균 전력은 baseline 대비 2.2x, C-ISP 대비 1.8x 크지만(모든 연산이 die 내부) 75W PCIe 한계 내. OptimStore는 채널 대기/DMA latency가 없어(MEM time이 latency 지배) C-ISP write/read 대비 유리(Fig.9).

> [!success]- 확장성·민감도·overhead (Fig.10-11, Table III, p.9)
> - **Scalability**: 채널 4/8/16, die 32→128로 늘릴 때 baseline은 32 die에서 이미 PCIe saturate, C-ISP는 write path 병렬화 한계로 die utilization ~30%에서 포화. OptimStore는 die 수 증가에 따라 꾸준한 speedup.
> - **Optimizer types**: Momentum이 연산 수 최소라 baseline 대비 **3.2x**로 최대 이득, Adam은 4 plane 중 1개 idle이라 최소 이득(Fig.11).
> - **FTL overhead**(Table III): intra-die WL이 optimizer step의 2.6%(예 GPT 6.7B 13.5s), GC는 forward pass와 overlap되는 짧은 latency(49~84ms).
> - **Lifetime**: 1TB pSLC, retention 3일로 완화 시 1.5M TBW(=30K P/E×50x×1TB) → 10.3B GPT 12.4M iteration, 9.4년 → 5년 warranty 충족(p.9).
> - **Area**: Synopsys DC, Samsung 65nm 기준 PE+로직이 NAND die의 0.15%.

## 섹션 노트
- §I Introduction: memory wall, optimizer memory 폭증, Deepspeed offloading의 I/O latency 문제 제기.
- §II Background: DNN 학습 3단계(forward/backward/weight update) + mixed-precision(Fig.2), modern SSD 계층(controller-channel-die-plane-page), ISP 정의.
- §III Motivations: CPU memory 부적합(8B 모델 119GB), Deepspeed 프로파일(update 93%, I/O 82%, Fig.3b), I/O 패턴 sequential·page-granular(Fig.3c), flash channel bus contention(read 19.52%, write 66.05% 대기).
- §IV Architecture: OptimStore overview(Fig.4), per-plane PE(Fig.5a), Fused Adam(Alg.1), 데이터 레이아웃(Fig.5b), 연산 흐름(Fig.6).
- §V System Support: ONFI 명령(Table I), optimizer 전용 FTL/GC/WL, model checkpointing, ioctl/NVMe feature 기반 programming.
- §VI Methodology: 5개 시스템 비교(BL/C-ISP=GenStore/DeepStore/OptimStore/CPU), SimpleSSD 수정, Table II 구성, GPT2·BERT·T5(2.88B/11.3B) 벤치.
- §VII Evaluations: 성능·에너지·확장성·optimizer 민감도·FTL overhead.
- §VIII Discussions: SSD lifetime(retention relaxation), ECC bypass(NaN 비교기만), pSLC BER<1e-8.
- §IX Related Works: Behemoth([26], standalone accelerator vs OptimStore의 NVMe offloading 보완), DeepStore([40], systolic array but no write support), GradPIM([23], DRAM-based, capacity 한계로 대규모 부적합).

## 핵심 용어
- **ODP (On-Die Processing)**: NAND flash die 내부에 PE를 두어 연산하는 OptimStore의 핵심 패러다임. 기존 ISP가 controller/embedded core 근처에서 처리하는 것과 구분됨.
- **ISP (In-Storage Processing)**: 스토리지 내부에서 연산하는 NDP의 한 갈래. OptimStore는 die 안쪽까지 더 깊이 들어감.
- **Weight update / optimizer step**: gradient로 weight를 갱신하는 학습 단계. elementwise 연산이지만 advanced optimizer에서 메모리·시간 병목.
- **Mixed-precision training**: FP16 forward/backward + FP32 master weight·optimizer state. loss scaling으로 gradient underflow 방지(Fig.2).
- **PE (Processing Element)**: per-plane에 배치된 subtract-shifter+2 multiplier+FMA의 pipelined FP 연산기.
- **LVS / QVS**: Linear/Quadratic Vector Sum. v1←αv1+βv2 / v1←αv1+βv2². Adam update의 fused 연산 단위.
- **ISQRT**: fast inverse square root(1/√v1), variance 정규화에 사용.
- **pSLC (pseudo-SLC)**: TLC/QLC 셀을 1비트로 써서 access latency를 단축한 캐시 영역(tR≈30µs, tPROG≈100µs).
- **VPP / BPR / BPP / ODC**: OptimStore가 추가한 ONFI 명령(Volatile Page Program / Buffered Page Read·Program / On-Die Computation).
- **super-page / super-block**: 여러 plane·die에 걸친 페이지/블록 묶음. optimizer FTL 매핑 단위.
- **iteration manager**: learning rate scheduler와 gradient scaler 상수를 재계산해 die에 broadcast하는 controller firmware.

## 강점 · 한계 · 열린 질문
- **강점**: (1) 병목을 controller가 아닌 flash channel로 정확히 진단하고 die-level NDP로 채널 트래픽을 근본 제거. (2) ONFI 호환 명령 확장 + on-die ECC NAND 선례로 실현 가능성·면적(0.15%) 현실적. (3) Fused Adam으로 PE 명령 수와 FP division 최소화, multistream 유사 데이터 레이아웃으로 plane 병렬성 활용. (4) end-to-end 2.4x, 에너지 3.6x로 명확한 이득과 9.4년 수명 분석까지 포함.
- **한계**: (1) optimizer step만 가속하며 forward/backward는 여전히 GPU 의존 → Amdahl 상한. (2) Adam은 4 plane 중 1개 idle로 OptimStore 이득이 가장 작음(Momentum 3.2x vs Adam 더 낮음). (3) pSLC retention을 3일로 완화하는 가정에 수명 분석이 의존. (4) 실제 칩 제작이 아닌 SimpleSSD 시뮬레이션 + Verilog 합성 추정 기반.
- **열린 질문**: forward/backward까지 in-storage로 확장 가능한가? 더 큰(100B+) 모델에서 CPU swap 대역폭이 새 병목이 되지 않는가? Adam의 idle plane을 다른 optimizer state나 연산으로 채워 활용도를 높일 수 있는가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. OptimStore가 기존 ISP와 근본적으로 다른 점은?
> 기존 ISP는 controller의 embedded core나 별도 로직(near-controller)에서 연산해 데이터를 flash channel 너머로 옮겨야 한다. OptimStore는 **flash die 내부 per-plane PE**에서 연산(ODP)하여 채널 데이터 이동 자체를 없애고 die-level/plane-level 병렬성을 최대로 활용한다.

> [!question]- Q2. 왜 controller가 아니라 flash channel이 병목인가?
> 대규모·sequential·multi-plane I/O에서 채널 byte-wide bus(~800~1200MT/s)가 데이터 이동을 직렬화한다. 프로파일상 read 요청의 19.52%가 bus contention, write의 66.05%가 채널 대기로 소비되어 채널이 주 병목이다(p.4).

> [!question]- Q3. Fused Adam optimizer가 필요한 이유는?
> NAND의 byte-by-byte 전송 때문에 단일 on-die 연산도 많은 cycle을 쓴다. 따라서 PE 명령 수를 줄이는 것이 핵심이며, momentum/variance update에 bias-correction·learning rate·gradient scaling을 fused하여 die 내부의 비싼 FP division과 중간 write를 제거한다(Alg.1, p.5).

> [!question]- Q4. Adam state를 어떻게 flash에 배치하나?
> gradient/weight/momentum/variance 4종을 서로 다른 plane에 두고(multistream 유사), 같은 word line offset의 weight·momentum·variance 페이지를 super-page로 묶어 single multi-plane operation으로 read/write한다. gradient는 임시값이라 cache register에만 둔다(Fig.5b, p.5-6).

> [!question]- Q5. 핵심 성능·에너지 수치는?
> Optimizer step에서 baseline SSD offloading 대비 평균 2.8x speedup·3.6x 에너지 절감, C-ISP 대비 1.9x. forward/backward 포함 end-to-end 평균 2.4x speedup(p.1, p.8).

> [!question]- Q6. 어떤 optimizer가 OptimStore에서 가장 큰 이득을 보나?
> Momentum이 연산 수가 가장 적어 4 plane을 모두 활용 → baseline 대비 3.2x로 최대. Adam은 4 plane 중 1개가 idle이라 상대적으로 이득이 작다(Fig.11, p.9).

> [!question]- Q7. on-die 연산에서 신뢰성/ECC는 어떻게 처리하나?
> 3D NAND의 주 error는 retention loss인데 tensor가 die에 잠깐만 머물러 영향이 거의 없고, pSLC는 수십만 P/E에서도 BER<1e-8. DNN의 error resiliency 덕에 복잡한 ECC 대신 erroneous 값이 NaN이면 MSB exponent를 뒤집는 NaN 비교기만으로 충분하다(p.9).

> [!question]- Q8. SSD 수명은 충분한가?
> retention을 1년→3일로 완화하면 P/E endurance가 50x 향상. 1TB pSLC 기준 1.5M TBW → 10.3B GPT의 12.4M iteration, 약 9.4년으로 통상 5년 warranty를 초과한다(p.9).

## 🔗 Connections
[[In-Storage Computing]] · [[HPCA]] · [[2023]]

## References worth following
- [26] S. Kim et al., "Behemoth: A flash-centric training accelerator for extreme-scale DNNs," FAST 2021 — 모든 tensor를 flash에 두는 standalone DNN 학습 가속기. OptimStore와 직접 비교 대상.
- [40] V. S. Mailthody et al., "DeepStore: in-storage acceleration for intelligent queries," MICRO 2019 — flash die에 systolic array를 둔 'chip level' 가속기. OptimStore가 write 지원 부재를 보완.
- [23] H. Kim et al., "GradPIM: A practical processing-in-DRAM architecture for gradient descent," HPCA 2021 — weight update를 DDR4 PIM으로 가속. DRAM capacity 한계 대비점.
- [27] D. P. Kingma, J. Ba, "Adam," 2014 — 본 연구가 가속하는 핵심 optimizer.
- [44] Microsoft Deepspeed — baseline NVMe offloading 시스템.
- [13] D. Gouk et al., "Amber/SimpleSSD," MICRO 2018 — 평가에 사용된 full-system SSD 시뮬레이터.

## Personal annotations
<본인 메모 영역>
