---
title: "Smart-Infinity: Fast Large Language Model Training using Near-Storage Processing on a Real System"
aliases: [Smart-Infinity]
description: "CSD(SmartSSD) 내부 FPGA에서 LLM 파라미터 update를 near-storage 처리해 storage-offloaded 학습의 system interconnect(PCIe) 병목을 줄여 최대 2.11배 가속한 실제 시스템"
venue: HPCA
year: 2024
tier: deep
status: done
presenter: 정진
present-date: 2026-08-06
tags:
  - paper
  - cluster/mine
  - cluster/llm
  - topic/llm-training
  - topic/near-storage
  - topic/in-storage-computing
  - topic/computational-storage
  - topic/hw-sw-codesign
  - venue/hpca
  - year/2024
---

# Smart-Infinity: Fast Large Language Model Training using Near-Storage Processing on a Real System
> **HPCA 2024** · `cluster/llm` · Source: [Smart-Infinity - Fast Large Language Model Training using Near-Storage Processing on a Real System.pdf](<Smart-Infinity - Fast Large Language Model Training using Near-Storage Processing on a Real System.pdf>)

저자: Hongsun Jang, Jaeyong Song, Jaewon Jung, Jaeyoung Park, Youngsok Kim, Jinho Lee (Seoul National University / University of Texas at Austin / Yonsei University)

## TL;DR
- LLM이 커지면서 GPU 메모리 용량이 한계가 되고, 이를 **storage-offloaded training**(optimizer state·gradient를 SSD로 내림)으로 풀면 이번엔 SSD 대역폭과 host의 **system interconnect (PCIe)** 가 병목이 된다.
- **Smart-Infinity**는 **computational storage device (CSD, 예: SmartSSD)** 내부의 FPGA 가속기에서 파라미터 **update**를 수행(**SmartUpdate**)하여, optimizer state를 interconnect로 옮길 필요를 없애 storage 트래픽을 줄인다.
- 추가로 (1) buffer 사전 할당 + swap overlapping을 하는 **internal data transfer handler**, (2) gradient를 GPU에서 압축하고 CSD에서 복원하는 **SmartComp** 를 제안한다.
- PyTorch(DeepSpeed) 위에 완전히 통합된 ready-to-use 시스템이며, baseline 대비 **최대 2.11× speedup** 을 달성한다 (p.1, p.10).

## 문제 & 동기 (Problem & Motivation)
- LLM은 파라미터 수 증가로 발전했고, 용량을 맞추려 수십 개의 GPU가 필요해진다. 이를 완화하는 한 방법이 host memory/storage를 확장 메모리 계층으로 쓰는 **storage-offloaded training**이지만, GPU 대비 storage 대역폭이 수십~수백 배 낮아 **storage bandwidth bottleneck**이 생긴다 (p.1).
- 저자들의 측정에서 storage로/에서 데이터를 옮기는 데 **전체 학습 시간의 88% 이상**이 소비된다 (p.1). baseline breakdown에서 update 단계(optimizer state upload/offload)가 학습 시간의 **80% 이상**을 차지한다 (Fig.3a, p.3).
- RAID0로 SSD를 늘리면 대역폭이 늘 것 같지만, **4개 초과 시 speedup이 포화**되고 shared system interconnect가 새 병목이 된다 (Fig.3b, p.3~4). PCIe lane은 GPU·NIC·메모리와 공유되는 귀중한 자원이라 무한히 늘릴 수 없다 (p.1).
- 핵심 관찰: **optimizer state는 update 단계에서만 쓰이지만 전체 storage 대역폭의 75%를 소비**한다. update를 CSD 내부 가속기로 옮기면 gradient·model parameter만 옮기면 되어 전체 트래픽의 75%를 줄일 수 있다 (p.1).

> [!quote]- 📄 원문 표현 (paper)
> - "According to our study, more than 88% of the total training time is consumed by transferring data from/to the storage." (p.1)
> - "We identify that the optimizer states are only used in the update phase of the training but consume 75% of the total storage bandwidth. By moving the update task to the custom accelerator inside CSDs, only the gradients and model parameters are required to be transferred, reducing 75% of the entire traffic." (p.1)
> - "Unfortunately, the speedup saturates after using more than four SSDs. Shared system interconnect becomes a new bottleneck when using more than four SSDs." (p.3~4)

## 핵심 통찰 (Key Insight)
- CSD(near-storage processing, NSP)는 plain storage 대비 두 가지 고유 특징이 있다: (1) storage 근처에서 계산하는 경량 FPGA 가속기, (2) SSD↔FPGA 직접 **peer-to-peer (P2P)** 통신을 위한 내부 PCIe switch (p.4).
- 단일 CSD는 대역폭 boost가 없다(storage→FPGA와 storage→host 트래픽이 같은 수의 PCIe lane을 지남). 하지만 **여러 CSD를 쓰면 aggregate internal bandwidth가 CSD 수에 비례해 선형 증가**하는 반면 shared system interconnect 대역폭은 그대로다 (Fig.2, p.4).
- 따라서 Smart-Infinity의 1차 목표는 **shared system interconnect를 통한 storage↔host 데이터 전송을 최소화**하고, (1) 여러 CSD의 aggregate internal bandwidth와 (2) 각 CSD 가속기의 연산 능력을 최대한 활용하는 것이다 (p.4).
- 통신량 관점: baseline은 update에서 양방향 read/write로 8M(gradient + 3개 optimizer state)을 옮기지만, SmartUpdate는 system interconnect 트래픽을 **read 2M(갱신된 파라미터) + write 2M(backward의 gradient)** 인 2M로 최소화한다 (p.4, Table I p.4).

> [!quote]- 📄 원문 표현 (paper)
> - "However, when multiple CSD devices are on the system (Figure 2), the aggregated internal bandwidth linearly increases according to the number of CSDs, while the shared system interconnect bandwidth remains the same." (p.4)
> - "our primary goal is minimizing the data transfer between storage devices and host memory through the shared system interconnect" (p.4)
> - "SmartUpdate minimizes the total communication volume through the system interconnect to only 2M read during the update phase for the updated parameters and 2M write during backward for the gradients." (p.4)

## 설계 / 메커니즘 (Design)
Smart-Infinity는 세 가지 구성요소를 둔다.

### 1) SmartUpdate: Near-storage Update with CSD (Sec IV-A, Fig.4)
- baseline은 host CPU가 update를 수행하므로, gradient와 optimizer state를 SSD에서 host로 올렸다가 갱신 후 다시 내려야 한다 (Fig.4a, p.4).
- SmartUpdate는 CSD 내부 FPGA가 update를 수행한다: ① gradient·optimizer state를 내부 PCIe switch 기반 직접 P2P로 FPGA에 로드, ② 가속기가 update module로 파라미터 갱신, ③ 갱신된 optimizer state를 P2P로 SSD에 직접 되돌림, ④ 갱신된 weight parameter(2M)만 host memory로 전송 (Fig.4b, p.4).
- update 연산을 CPU에서 CSD로 offload하여 통신량을 기존 storage-offloaded 방식의 **(6+2)M에서 2M으로** 줄인다 (p.4). Adam은 FP32 optimizer state(parameter·momentum·variance)가 FP16 model(M) 대비 6M을 차지하는 것이 update를 옮기는 핵심 이유다 (p.2).

### 2) Internal Data Transfer Handler for SmartUpdate (Sec IV-B, Fig.5)
- naive 구현은 subgroup별 tasklet을 순차 실행하며, transfer를 overlap하려면 추가 device memory가 필요해 **out-of-memory(OOM)** 가 난다 (p.5).
- 제안하는 handler는 **변수 buffer를 분리하고 non-urgent 변수를 lazy하게 전송**한다. 초기화 시 가장 큰 optimizer state 크기로 device memory buffer를 **사전 할당(buffer pre-allocation)** 하고 reuse하여 재할당을 피한다 (Fig.5b, p.5).
- 두 스레드(thread 0/1)를 둬, thread 0은 GPU가 forward/backward에 즉시 필요한 **model parameter를 우선 SSD에 writeback**하고, momentum·variance처럼 다음 update에서만 쓰는 변수의 writeback과 다음 subgroup 로드는 thread 1이 낮은 우선순위로 처리한다. 이점: ① device memory 재할당 회피, ② GPU가 forward/backward를 더 일찍 시작, ③ SSD 전송을 overlap (p.5).

### 3) SmartComp: CSD-aided Gradient Compression (Sec IV-C, Fig.6)
- SmartUpdate로 optimizer state 트래픽은 사라지지만, **gradient(2M)는 여전히 system interconnect를 지나** CSD 수가 늘면 새 병목이 된다. SSD write 대역폭이 read보다 훨씬 낮아 더 악화된다 (p.5).
- SmartComp는 GPU에서 gradient를 **magnitude-based (Top-K) 압축**(크기 큰 값만 index·value로 남기고 작은 것은 0으로)하고, FPGA에서 index에 따라 값을 흩뿌리는(scatter) **decompression**을 수행한다 (p.5~7). 압축으로 storage write 트래픽이 **2M에서 c%×2M로** 감소(c: compression ratio) (p.4, Table I p.4).
- 정렬이 필요한 압축은 강력한 GPU에서, index scatter뿐인 decompression은 경량 FPGA에서 분담하는 것이 합리적이다 (p.6).

### 4) Workload Distribution to Multiple CSDs (Sec IV-D)
- update가 element-wise라는 점을 이용해, model parameter를 flatten 후 각 CSD에 균등 분배하며, 각 CSD는 자기 파라미터에 대한 optimizer state를 갖고 P2P로 update한다. 이 분배는 **model architecture에 agnostic**이어서(layer/hidden dim/head 수 몰라도 됨) 채택이 단순하다 (p.7).

### 5) Accelerator Architecture & 실제 HW (Sec V~VI, Fig.7~8)
- FPGA microarchitecture는 **updater**(Adam용, 여러 PE + SIMD **AXPBY** units로 element-wise moving average 처리, SGD/AdaGrad/AdamW 확장 가능)와 **decompressor**(Top-K, idx로 값 라우팅, arithmetic 없음)로 구성된다 (Fig.7, p.7~8).
- SW 통합: **DeepSpeed** 위에 ZeRO-Infinity의 parameter update 부분을 교체. disutils로 C/C++ 모듈을 callable Python으로 빌드, **Xilinx OpenCL extension**으로 FPGA 제어, `CL_MEM_EXT_PTR_XILINX` 플래그로 buffer 사전 할당, `pread/pwrite`로 P2P. 옵션만 켜면 기존 DeepSpeed LLM 코드를 수정 없이 실행 (Fig.8, p.6~8).
- 실제 HW: **SAMSUNG SmartSSD (4TB)**, 내부 **Kintex UltraScale+ KU15P FPGA**(약 522K LUT, 984 BRAM, 1968 DSP, DDR4 4GB DRAM), PCIe Gen3.0 x4, 최대 10개 사용. GPU는 RTX A5000(default)/A100/A4000, CPU Xeon Gold 6342, Vitis/XRT 2023.1 (Table II, p.8).

> [!quote]- 📄 원문 표현 (paper)
> - "Instead of uploading the gradients and optimizer states to host memory, ① SmartUpdate directly loads them to FPGA through direct P2P communication which is possible because of the existence of an internal PCIe switch in each CSD." (p.4)
> - "Therefore, we propose an internal data transfer handler optimization which leads to better throughput of SmartUpdate, addressing the device memory consumption issue." (p.5)
> - "With the aid of the accelerator in CSDs, we propose Smart-Comp that applies gradient compression on top of SmartUpdate: compress the gradients using GPU, and decompress them before updating on CSDs." (p.5)
> - "Because of using flattened model parameters, the distribution procedure is agnostic to the model architecture." (p.7)

## 평가 (Evaluation)
- **약어**: BASE = DeepSpeed ZeRO-Infinity(+SW RAID0), SU = SmartUpdate, SU+O = handler 최적화 추가, SU+O+C = SmartComp 추가. 기본 compression ratio는 2%(1% gradient + indices) (p.8).
- **Speedup (Fig.9, p.8~9)**: GPT-2 8.4B / 6 SSD에서 baseline update가 학습 시간의 **75.57%**. SU는 6 SSD에서 1.18×~1.24×, 10 SSD에서 1.54×~1.60×. SU+O는 10 SSD에서 **1.60×~1.66×**. SU+O+C는 SU 대비 1.22×~1.31× 추가, baseline 대비 **1.85×~1.98×** (p.8~9).
- **대형 모델 확장성 (Fig.10, p.9)**: GPT-2 16.6B~33.0B에서 일관된 speedup. 33.0B에서 6 SSD 1.37×, 10 SSD **1.88×** (p.9).
- **CSD 수 / GPU 등급 (Fig.11, p.9~10)**: baseline은 4 SSD 넘으면 PCIe interconnect 한계로 확장 정지. Smart-Infinity는 aggregate internal bandwidth에 의존해 거의 선형 확장. A100 환경에서 **최대 2.11× speedup**. 단일 CSD에서는 대역폭 증가가 없어 약간 slowdown (p.9~10).
- **다른 optimizer (Fig.12, p.10)**: SGD/AdaGrad는 Adam보다 optimizer state가 3/4 적어 speedup이 약간 낮음. **다른 모델 (Fig.13, p.10)**: BLOOM/ViT에서 1.32×~1.85×.
- **Throughput (Fig.14, p.10)**: updater throughput은 **7GB/s 초과**로 SSD read/write보다 충분히 높음. decompressor는 SSD read를 약간 상회.
- **자원 사용 (Table III, p.8)**: Adam updater LUT 33.66%, DSP 11.03%; Top-K 추가 시 LUT 34.12%, URAM 35.94%.
- **Performance/$ (Fig.15, p.11)**: SmartSSD는 동용량 SSD($400)보다 약 6배 비싸($2,400) 1~3 CSD에서는 GFLOPS/$가 baseline보다 낮지만, **4 SSD 초과부터 더 효율적**이며 CSD 수 증가에 따라 계속 증가 (p.11).
- **Fine-tuning 정확도 (Table IV, p.11~12)**: SmartUpdate는 baseline과 algorithmically 동일해 정확도 동일. SmartComp는 lossy지만 GLUE(MNLI/QQP/SST-2/QNLI)에서 comparable accuracy (p.11~12).
- **Multi-GPU congested topology (Fig.17, p.12)**: 1~3 A4000 GPU 공유 환경에서도 10 CSD로 1.66×~1.86× speedup (p.12).

> [!quote]- 📄 원문 표현 (paper)
> - "Our evaluation shows that Smart-Infinity achieves up to 2.11× speedup over the baseline." (p.2)
> - "In GPT-2 8.4B model with 6 SSDs, the update time with the optimizer states communication time consumes 75.57% of the training time of the baseline." (p.8)
> - "The throughput of the updater is above 7GB/s, which is sufficiently higher than the SSD read/write." (p.10)
> - "SmartUpdate is algorithmically identical to the baseline training, so the accuracy is exactly the same as the baseline. SmartComp adopts lossy compression, but achieves comparable accuracy in all datasets." (p.11~12)

## 섹션 노트 (Section notes)
- **I. Introduction**: storage-offloaded training의 88% 전송 비용 문제 제기, 기여 4가지 요약(SmartUpdate, data transfer handler, SmartComp, PyTorch 통합) (p.1~2).
- **II. Background**: ZeRO의 optimizer state 분할, FP32 optimizer state가 6M(momentum+variance+param의 FP32) 차지, storage-offloaded training의 forward/backward/update 흐름 (Fig.1, p.2~3).
- **III. Motivation**: data transfer overhead와 RAID 한계로 system interconnect 병목을 실측 (Fig.3, p.3~4).
- **IV. Smart-Infinity**: Table I로 트래픽 변화((6+2)M→2M, write 2M→c%×2M), SmartUpdate/handler/SmartComp/multi-CSD 분배 (p.4~7).
- **V. Accelerator Architecture**: General Updater(AXPBY PE) + General Decompressor(Top-K) (Fig.7, p.7~8).
- **VI. Implementation**: DeepSpeed/disutils/pybind11/Xilinx OpenCL/pread-pwrite P2P (Fig.8, p.6~8).
- **VII. Evaluation**: Fig.9~17, Table II~IV (p.8~12).
- **VIII. Discussion**: multi-GPU congested topology, model compression 응용, storage expansion/pooling (p.12).
- **IX/X. Related Work & Conclusion**: near-data processing(DRAM~storage), DNN training acceleration, SmartSSD 기반이지만 multiple CSD로 system interconnect 병목 완화는 최초라 주장 (p.13~14).

## 핵심 용어 (Key terms)
- **storage-offloaded training**: GPU 메모리가 부족할 때 optimizer state·gradient를 SSD로 내려 host memory/storage를 확장 메모리 계층으로 쓰는 학습 방식.
- **computational storage device (CSD) / near-storage processing (NSP)**: storage 근처에 연산 엔진(경량 FPGA)과 내부 PCIe switch를 둔 storage 장치 (예: SmartSSD).
- **system interconnect (PCIe)**: host와 storage를 잇는 공유 채널. storage-offloaded 학습의 주 병목.
- **internal P2P communication**: CSD 내부 PCIe switch를 통한 SSD↔FPGA 직접 전송. host interconnect를 우회.
- **optimizer state**: Adam의 momentum·variance 등. mixed precision에서 FP32로 총 6M 차지.
- **SmartUpdate**: 파라미터 update를 CSD 내부 FPGA에서 수행해 통신량을 (6+2)M→2M으로 줄임.
- **SmartComp**: GPU에서 magnitude-based(Top-K) gradient compression, FPGA에서 decompression.
- **internal data transfer handler**: buffer 사전 할당 + swap overlapping으로 OOM 없이 전송을 overlap.
- **AXPBY unit**: 두 벡터 A,B를 계수 α,β로 곱해 element-wise 더하는 SIMD 유닛. 다양한 moving average 처리.

## 강점 · 한계 · 열린 질문
- **강점**: 시뮬레이터가 아닌 **실제 시스템**(SmartSSD + FPGA + DeepSpeed)에 완전 통합된 ready-to-use 솔루션. CSD 수에 따라 aggregate internal bandwidth가 선형 확장되어 baseline의 4 SSD 포화를 넘어선다. model architecture에 agnostic(flatten 분배)해 적용이 단순하고 옵션 하나로 활성화된다.
- **한계**: SmartSSD가 동용량 SSD 대비 약 6배 비싸 적은 CSD(1~3개)에서는 GFLOPS/$가 baseline보다 낮다(Fig.15, p.11). SmartComp는 lossy compression. SmartSSD/Xilinx FPGA(OpenCL/XRT) 생태계 종속. multi-GPU congested topology에서는 PCIe topology에 따라 speedup이 줄어든다(Fig.17, p.12).
- **열린 질문**: quantization/pruning/low-rank 같은 model compression 응용 시 STE·per-layer interval 등 비자명한 이슈가 있어 future work로 남김(p.12). 더 많은 decompression 엔진 배치 시 추가 가속 여지(p.10). 진짜 multi-node 분산 학습(collective)과의 협력은 미해결.

## ❓ Q&A (자가 점검)
> [!question]- Q1. storage-offloaded training에서 진짜 병목은 무엇이고, 왜 RAID로 못 푸는가?
> storage↔host 데이터 전송(88% 이상 시간)이 병목이며, RAID0로 SSD를 늘려도 4개 초과 시 shared system interconnect(PCIe)가 새 병목이 되어 speedup이 포화한다 (p.1, Fig.3, p.3~4).

> [!question]- Q2. SmartUpdate가 통신량을 줄이는 핵심 원리는?
> update를 host CPU 대신 CSD 내부 FPGA에서 수행한다. optimizer state(6M)를 내부 P2P로 FPGA↔SSD에서만 주고받고, system interconnect에는 갱신된 parameter 2M만 올린다. (6+2)M → 2M (p.4).

> [!question]- Q3. 단일 CSD로는 왜 큰 이득이 없나?
> 단일 CSD는 대역폭 boost가 없고(storage→FPGA와 storage→host가 같은 수의 PCIe lane 사용) FPGA 활용을 위한 base overhead가 더해져 약간 느려질 수도 있다. 이득은 여러 CSD의 aggregate internal bandwidth가 선형 증가할 때 나온다 (p.4~5, p.10).

> [!question]- Q4. internal data transfer handler가 푸는 문제는?
> naive overlap은 추가 device memory가 필요해 OOM이 난다. handler는 가장 큰 optimizer state 크기로 buffer를 사전 할당·reuse하고, GPU에 즉시 필요한 model parameter writeback을 우선하고 나머지를 lazy 처리해 OOM 없이 전송을 overlap한다 (p.5).

> [!question]- Q5. SmartComp에서 압축은 GPU, 복원은 FPGA에 둔 이유는?
> Top-K 압축은 정렬이 필요해 강력한 GPU가 적합하고, decompression은 index에 따라 값을 흩뿌리는 단순 작업이라 경량 FPGA로 충분하기 때문이다 (p.5~6).

> [!question]- Q6. SmartComp 압축이 정확도를 해치지 않나?
> SmartUpdate는 baseline과 algorithmically 동일해 정확도가 동일하다. SmartComp는 lossy지만 GLUE 모든 데이터셋에서 comparable accuracy를 보인다 (Table IV, p.11~12).

> [!question]- Q7. 최대 speedup과 그 조건은?
> A100 GPU 환경에서 최대 2.11× speedup. 일반적으로 SU+O+C가 10 SSD에서 baseline 대비 1.85×~1.98×, GPT-2 33.0B에서 1.88× (p.2, p.9~10).

> [!question]- Q8. 실제로 어떤 HW/SW 스택 위에서 동작하나?
> SAMSUNG SmartSSD(4TB, 내부 Kintex UltraScale+ KU15P FPGA, DDR4 4GB), GPU는 A5000/A100/A4000. SW는 DeepSpeed 위에 통합되고 Xilinx OpenCL extension, pread/pwrite P2P, pybind11을 사용. 옵션만 켜면 기존 코드 수정 없이 실행 (Table II p.8, Fig.8 p.7~8).

## 🔗 Connections
[[LLM Systems]] · [[In-Storage Computing]] · [[HPCA]] · [[2024]]
관련: [[Sparse Checkpointing for Fast and Reliable MoE Training]], [[SkyByte]], [[XHarvest]]

## References worth following
- **[96] ZeRO-Infinity** (Rajbhandari et al., SC 2021) — Smart-Infinity의 baseline storage-offloaded training framework.
- **[95] ZeRO** (Rajbhandari et al., SC 2020) — optimizer state partitioning으로 메모리 최적화, 6M 분석 근거.
- **[98] ZeRO-Offload** (Ren et al., USENIX ATC 2021) — host memory offloaded training.
- **[58] OptimStore** (Kim et al., HPCA 2023) & **[57] GradPIM** (Kim et al., HPCA 2021) — DNN training용 near-data update 선행연구(시뮬레이터/dedicated die 가정).
- **[73] Deep Gradient Compression** (Lin et al., ICLR 2018) — SmartComp의 gradient compression 근거.
- **[59] Adam** (Kingma & Ba, ICLR 2015) — update 대상 optimizer, 6M state 구조 정의.

## Personal annotations
