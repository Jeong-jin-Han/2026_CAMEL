---
title: "InstAttention: In-Storage Attention Offloading for Cost-Effective Long-Context LLM Inference"
aliases: [InstAttention, InstInfer]
description: "arXiv 원제 InstInfer. 디코딩 단계 attention과 KV cache를 Computational Storage Drive(CSD)로 오프로딩해 PCIe 대역폭 병목을 우회하는 저비용 long-context LLM 추론 시스템"
venue: HPCA
year: 2025
arxiv: "2409.04992"
tier: deep
status: done
tags:
  - paper
  - cluster/isc
  - topic/llm
  - topic/attention-offloading
  - topic/computational-storage
  - topic/kv-cache
  - venue/hpca
  - year/2025
---

# InstAttention: In-Storage Attention Offloading for Cost-Effective Long-Context LLM Inference

> **HPCA 2025** · cluster/isc · arXiv명 InstInfer · Source: [InstAttention - In-Storage Attention Offloading for Cost-Effective Long-Context LLM Inference.pdf](<InstAttention - In-Storage Attention Offloading for Cost-Effective Long-Context LLM Inference.pdf>)

**저자:** Xiurui Pan, Endian Li, Qiao Li, Shengwen Liang, Yizhou Shan, Ke Zhou, Yingwei Luo, Xiaolin Wang, Jie Zhang
(Peking University, Xiamen University, ICT/CAS, Huawei Cloud, Wuhan National Laboratory for Optoelectronics / HUST — Computer Hardware and System Evolution Laboratory)

---

## TL;DR

오프라인 long-context / large-batch LLM 추론에서 KV cache는 GPU VRAM을 압도하므로 host memory나 SSD로 오프로딩하는데, 이 경우 **좁은 PCIe 대역폭**을 통한 KV cache 전송이 새로운 병목이 된다. InstInfer(노트 제목상 InstAttention)는 **decoding-phase attention 연산과 KV cache를 Computational Storage Drive(CSD)로 오프로딩**하여, SSD 내부의 높은 flash channel 집계 대역폭(tens of GB/s)을 직접 활용함으로써 PCIe(3~6 GB/s)를 우회한다. flash 특성을 고려한 sparse attention 알고리즘(SparF), KV cache 전용 FTL의 dual address mapping, GPU-CSD P2P DMA를 co-design 했다. 13B 모델 + NVIDIA A6000 환경에서 FlexGen 대비 long-sequence throughput을 **최대 11.1배** 개선한다 (p.1 Abstract, p.2).

---

## 문제 & 동기

LLM 추론은 compute-bound인 **prefilling**과 memory-bound인 **decoding** 두 단계로 나뉜다. decoding은 KV cache(과거 토큰의 K,V 텐서 캐시)에 의존해 재계산을 피하지만, context length와 batch size가 커질수록 KV cache가 VRAM을 폭증시킨다. edge/personal device 같은 resource-constraint 시나리오에서는 GPU를 추가하기엔 비용 부담이 너무 크다.

기존 해법(DeepSpeed-MII, FlexGen)은 KV cache를 host memory나 SSD로 오프로딩하지만, host-GPU PCIe < GPU 내부 VRAM 대역폭이며 SSD는 더 낮고, SSD-GPU 간 직접 datapath 부재와 host-oriented storage stack이 성능 페널티를 악화시킨다.

- 13B 파라미터 모델, batch size 32, 4K 토큰 → KV cache 약 100GB로 모델 자체 크기의 **4.2배**. (p.1)
- OPT-13B, 2K 시퀀스, batch 128 → 모델 weight 약 24GB인데 KV cache는 최대 200GB. OPT-175B는 weight 325GB에 KV cache 최대 2.63TB. (p.4)
- DeepSpeed는 SSD 오프로딩 미지원 → batch 32에서 host→SSD kernel swapping 발생 시 **97.01% 성능 하락**. (p.5)

> [!quote]- 📄 원문 표현 (paper)
> - "Unlike the compute-bound prefilling phase, the memory-bound decoding phase critically depends on KV cache I/O, as it requires frequent transfers of large KV cache volumes between the storage media and GPUs. This dependence makes data movement over a narrow PCIe bus a new performance bottleneck." (p.2)
> - "For a 2K-length sequence with batch size 128, the OPT-13B model occupies about 24GB for its model weights, and generates up to 200GB KV caches." (p.4)
> - "kernel swapping from host memory to SSD occurs, leading to a 97.01% performance decline." (p.5)

---

## 핵심 통찰

1. **CSD의 내부 대역폭 > 외부 PCIe.** 현대 SSD는 8~16 flash channel(채널당 1~2GB/s)을 집계해 tens of GB/s를 내부적으로 제공하는데, 이는 외부 PCIe(3~6GB/s)보다 훨씬 크다. 연산을 SSD 내부 엔진에 올리면 PCIe 병목을 우회하고 KV cache 대역폭 요구를 충족할 수 있다. (p.2)

2. **decoding-phase attention만 골라서 오프로딩.** roofline 분석 결과, decoding의 attention operator(Logit, Attend)는 GeMV 기반으로 arithmetic intensity가 극도로 낮아 GPU 가속이 무의미하고 KV cache I/O가 지배적이다. 반면 prefilling attention과 QKV/O Proj, FFN은 compute-intensive(GeMM)하므로 GPU에 남긴다. CSD 연산력은 GPU보다 2~3 orders of magnitude 약하므로 전체 decoding을 올리면 안 되고 memory-bound operator만 올린다. (p.5)

3. **sparse attention으로 연산·전송량 동시 절감.** 기존 sparsity 알고리즘은 flash의 page granularity / write amplification / random access 특성을 고려하지 않아 CSD에 부적합. flash-aware sparse 알고리즘이 필요.

- prefilling: QKV Proj., O Proj., FFN은 compute-intensive → GPU 유지. attention(Logit, Attend)도 GPU 계산력이 약한 CSD에선 심하게 제약되므로 prefilling attention도 GPU에 둠. (p.5)
- decoding: QKV Proj., O Proj., FFN은 GPU에선 memory-bound지만 CSD에선 operational intensity가 CSD 최대 연산력에 근접 → CSD에 큰 부담. 또한 weight matrix 기반 flat GeMM이라 KV cache와 무관. 따라서 KV cache 직접 접근이 필요한 attention operand(Logit, Attend)만 CSD로 보냄. (p.5)
- FlexGen decoding latency breakdown: batch 4,8에서 KV cache가 GPU에 들어가면 Weight Access가 주 병목이지만, batch 증가로 KV cache가 SSD로 가면 KV Cache Access 비중이 최대 **98.94%**까지 상승. (p.5)

> [!quote]- 📄 원문 표현 (paper)
> - "InstInfer only offloads the most performance-critical decoding-phase attention computations during long-context inference to CSDs, while leveraging the GPU to execute the remaining inference processes." (p.2)

---

## 설계 / 메커니즘

세 가지 하드웨어 컴포넌트로 구성 (Fig.7, p.6):
- **InstCSD**: decoding-phase attention 계산 + 대용량 KV cache 저장. SSD controller에 SparF Attention Engine과 KV Cache-FTL을 통합.
- **InstGPU**: prefilling 단계 전 계산 + decoding의 나머지(qkv Proj., o Proj., FFN) + prefilling 단계에서 KV cache 생성.
- **InstHost**: control plane — user request, task scheduling, data transmission 조율만 담당.

### 1) SparF Attention (flash-aware sparse q-attention, Algorithm 1)
SparQ를 flash에 맞게 확장. **dual-step loading**으로 처음엔 page(group) 단위, 다음엔 token 단위로 sparsity를 적용한다.
- q vector의 top-r hidden embedding을 골라(argtopk) approximate attention score ŝ 계산 → top-k 중요 토큰 선택 → 해당 토큰의 full K,V만 load. (p.6 Algo.1)
- 누락된 V는 가중평균(weighted mean, α 보정)으로 최종 attention output 근사: `out ← αs·V[j,:] + (1-α)v̄`.
- token group은 16개 연속 토큰을 같은 page에 묶어 저장하고 채널 간 stridden 배치. group 내 모든 토큰이 top-k 미만이면 group 무시, 아니면 dense로 간주 후 fine-grained filter. (p.7)

### 2) Hardware-Based Attention Engine
argtopk unit + NFC(NAND Flash Controller)에 내장된 filter + 두 개의 동일한 Attention Kernel(GeMV unit + Softmax)로 구성. q vector → argtopk로 top-r hidden embedding filter → NFC가 K[:,i] page fetch + filter → Attention Kernel①이 approximate score 계산 → argtopk로 top-k token → K[j,:], V[j,:] load + NFC filter → Attention Kernel②. 두 attention kernel은 real-time load 기반으로 스케줄. fine-grained parallelism으로 flash 긴 access latency를 은닉. (p.7-8, Fig.8)

### 3) KV Cache-oriented FTL: dual address mapping
SparF가 K cache를 token-index와 hidden embedding-index 양쪽으로 인덱싱해야 하므로, **두 가지 주소 매핑**을 둔다 (Fig.9):
- **Token-indexed mapping**: 16 연속 토큰을 group으로 같은 page에 저장, attention head를 서로 다른 channel/block에 stridden 분산. 4KB page random read 시 conventional FTL은 최대 16배 성능 저하 → group화로 완화. (p.7)
- **Hidden embedding-indexed mapping**: 한 hidden embedding이 여러 토큰에 연속 접근 필요 → K를 토큰별 1 embedding씩 page에 저장, page당 약 2K 토큰. 256~1K token 최소 granularity. 저장 비용이 낮아 **K matrix를 두 번 저장**(서로 다른 방향 인덱싱)해 random index와 write amplification을 동시에 줄임. (p.7-8)

### 4) 데이터 이동 / 시스템 통합
- **GPU-CSD P2P DMA**: host memory buffer를 우회해 PCIe lane으로 직접 전송. GPUDirect Storage(host filesystem 의존)와 달리 InstInfer는 독립적으로 동작, FTL이 모든 매핑/주소 변환을 InstCSD 내부 DRAM에서 처리해 host system overhead 대폭 감소. (p.8)
- **Layer-wise pipeline**: i번째 layer의 KV cache 전송을 (i+1)번째 layer 계산과 동시 진행. decoding은 GPU/CSD 간 q,k,v / attention output만 PCIe로 전송 → 전송량을 s/2배 감소(s=시퀀스 길이). (p.8)
- **Scale to CSD array**: multi-head attention의 head는 독립적이므로 n개 CSD에 head를 분산(예: OPT-13B 40 heads → CSD당 n_head/n). (p.8)

> [!quote]- 📄 원문 표현 (paper)
> - "InstInfer incorporates a dual-step loading strategy to manage the sparsity in the sequence: initially at the page level and subsequently at the token level." (p.2)

---

## 평가

**Testbed (p.9):** NVIDIA A6000 48GB VRAM, Intel Xeon 5320 2.2GHz 96GB DDR4, Samsung 980pro 2TB SSD, PCIe Gen4x16. 모델 OPT-13B, FP16, 입출력 길이 각 1024. 데이터셋 ShareGPT, WikiText-2, SQuAD, TriviaQA. 비교 대상: DeepSpeed, FlexGen(SSD), FlexGen-SparQ(1/8), InstI-Dense(SparF 미적용 baseline), InstI-SparF(1/8).

**하드웨어 구현 (p.9):** InstCSD는 Daisyplus OpenSSD(Xilinx ZU17EG UltraScale+ MPSoC, 4-core ARM, 2GB DRAM, PCIe Gen3x4) 기반. 실제 배포는 비용·채널 한계로 NVMeVirt 가상 NVMe + Xilinx Zynq7045 FPGA(285MHz, channel 8개로 확장, 1.4GB/s/channel)로 이식. Zynq7045 자원 사용률: LUT 80.27%, FF 58.92%, BRAM 46.06%, DSP 85.33% (Table I, p.9).

**Throughput (1 CSD, Fig.12, p.10):**
- DeepSpeed는 host memory 사용으로 small batch(4~16)에서 우세하나 batch 32에서 host memory 초과 → SSD swapping으로 baseline(bs=16) 대비 **32.6배 throughput 하락**.
- FlexGen은 bs=64까지 지원하나 throughput 낮음, bs=128에서 OOM.
- InstI는 layerwise 전송으로 더 큰 batch 지원, FlexGen(bs=64) 대비 **6.85배** 우세.
- InstI-SparF는 original InstI 대비 최대 **2.08배**(bs=256), baseline FlexGen 대비 최대 **11.1배**. (p.10)

**Throughput (2 CSD, Fig.13, p.10):** 전통적 오프로딩은 host filesystem 부담으로 다중 SSD 대역폭 집계 이점 미미. InstI는 P2P DMA + 내부 flash channel scaling으로 FlexGen 최대치(bs=32) 대비 **10.5배**, InstI-SparF(bs=256)는 FlexGen-SparQ(bs=32) 대비 **3.11배**. (p.10)

**Decoding latency breakdown (Fig.14,15, p.10):** 모든 시나리오에서 KV cache access가 주 병목. FlexGen(bs=64) 대비 KV cache access 비중을 dense에서 98.9%→80.7%(InstI), 76.4%(InstI-2 CSD)로, sparse에서 92.4%→82.3%, 74.0%로 감소. end-to-end로 dense InstI와 InstI-A는 KV cache access overhead를 각각 **88.1%, 94.0%** 감소. (p.10)

**Accuracy (Fig.11, p.9-10):** SparF는 vanilla SparQ와 거의 동일한 정확도, H2O/local attention보다 robust. compression ratio 1/8까지 negligible accuracy loss → 기본 1/8 채택.

**Scalability (Fig.17a, p.11):** 1-CSD 대비 20개 CSD 배포 시 dense **8.99배**, sparse **7.29배** throughput 개선. head-level parallelism이라 의존성 없어 scaling 용이.

**SparF engine breakdown (Fig.16, p.11):** dense 대비 SparF는 step 4의 Logit-0(추가 logit 계산) 과정이 추가되지만, 이 sparsity 식별 비용이 전체 성능 향상으로 보상됨.

> [!quote]- 📄 원문 표현 (paper)
> - "InstI outperforms FlexGen at bs=64 by 6.85×." (p.10)

---

## 섹션 노트

- **§I Introduction:** GPU-only / prior KV cache offloading / InstAttention 세 아키텍처 비교(Fig.1). decoding-phase attention 오프로딩 개념 제시.
- **§II Background:** LLM 구조(decoder block = self-attention + FFN), prefilling/decoding, KV cache로 O(s²)→O(s) 복잡도 감소, sparse attention(SparQ는 K cache를 token·hidden 양쪽 인덱싱 필요해 1.5배 KV footprint), SSD 구조(NAND die/channel/NFC/FTL/DRAM), CSD 정의.
- **§III Challenges & Opportunities:** KV cache 분석, 오프로딩 성능 저하 측정(Fig.4,5), CSD 오프로딩 기회 — roofline로 operator별 GPU/CSD 적합성 분석(Fig.6).
- **§IV Design:** InstInfer overview(Fig.7), SparF Attention(Algo.1, Fig.8), KV cache mapping(Fig.9), 시스템 통합·scaling.
- **§V Implementation:** OpenSSD/NVMeVirt/Zynq7045 배포, FlexGen 기반 SW stack.
- **§VI Evaluation:** accuracy, throughput(1/2 SSD), latency breakdown, scalability/sensitivity.
- **§VII Related Work:** PIM 기반 transformer 가속(시뮬레이터 한계), KV cache 관리(vLLM, LMDeploy, CachedAttention, Mooncake 등 — prefilling/online 중심이라 long-output decoding엔 부적합).
- **§VIII Conclusion:** CSD 기반 offline 추론으로 storage·bandwidth 문제 해결, FlexGen 대비 최대 11.1배.

---

## 핵심 용어

- **CSD (Computational Storage Drive):** SSD 내부에 ARM/NPU/FPGA 연산 엔진을 통합해 storage 근처에서 데이터 처리. 저장 비용은 SSD 수준이나 내부 대역폭이 높음. (p.2,4)
- **KV cache:** 디코딩 중 과거 토큰의 K,V 텐서 캐시. 재계산을 피해 O(s²)→O(s)로 줄이지만 메모리·I/O 폭증. (p.3)
- **SparF Attention:** SparQ를 flash 특성(page granularity)에 맞게 확장한 flash-aware sparse q-attention. dual-step(page→token) loading. (p.6)
- **dual address mapping:** KV cache-FTL이 token-indexed / hidden embedding-indexed 두 매핑을 유지해 SparF의 양방향 인덱싱 random access 완화. K matrix 이중 저장. (p.7)
- **NFC (NAND Flash Controller):** flash channel당 데이터 전송 담당. 여기에 filter를 내장해 sparse 토큰 걸러냄. (p.4,7)
- **P2P DMA:** GPU-CSD 직접 전송, host memory buffer 우회. (p.8)
- **roofline model:** operational intensity 대비 성능 한계 분석으로 operator의 memory-bound/compute-bound 판별. (p.5)

---

## 강점 · 한계 · 열린 질문

**강점**
- "CSD로 decoding attention을 올린다"는 명확한 한 문장 아이디어를 roofline 분석으로 정당화 — 왜 attention만, 왜 decoding만 인지 설득력 있음.
- 시뮬레이터가 아닌 **실제 하드웨어**(OpenSSD + Zynq7045 FPGA)에 full-stack 구현·배포. PIM 계열과 차별화.
- accuracy 손실 거의 없이(1/8) throughput 최대 11.1배, head-level parallelism으로 CSD array scaling 우수.

**한계**
- offline / long-context / large-batch 시나리오 특화. low-latency online inference엔 부적합(자체 명시).
- OpenSSD 플랫폼이 고가 FPGA·64GB·4채널로 실용성 제약 → NVMeVirt+Zynq7045로 우회 평가. 즉 일부 결과는 가상 NVMe 기반.
- CSD 내부 대역폭(11.2GB/s)이 여전히 GPU-host PCIe(32GB/s)보다 낮아, dense InstI는 DeepSpeed(bs=16) 대비 4.6%만 우세(sparse가 핵심 동력). (p.10)
- OPT-13B / LLaMA-2-7B 중심 평가. K cache 이중 저장은 추가 flash 용량 소비.

**열린 질문**
- MoE / GQA / 더 큰 모델(70B+)에서 head-level CSD 분산이 어떻게 동작하는가?
- 실제 상용 SSD(더 많은 채널·저렴한 프로세서)에서 attention engine의 연산력이 충분한가?
- SparF의 dynamic group size 조정 정책이 다양한 context length에 얼마나 robust한가?

---

## ❓ Q&A

> [!question]- Q1. 왜 전체 decoding이 아니라 attention만 CSD로 오프로딩하나?
> roofline상 QKV/O Proj.와 FFN은 weight matrix 기반 flat GeMM으로 CSD에선 연산 부담이 크고 KV cache와 무관하다. 반면 Logit/Attend는 GeMV 기반 memory-bound이며 KV cache 직접 접근이 필요해 CSD의 내부 대역폭이 결정적 이득을 준다. CSD 연산력은 GPU보다 2~3 orders 약하므로 memory-bound operator만 선택적으로 올린다. (p.5)

> [!question]- Q2. prefilling attention은 왜 GPU에 남기나?
> prefilling attention은 compute-intensive(GeMM)해서 연산력 약한 CSD에 올리면 심하게 느려진다. prefilling은 compute-bound 단계이므로 GPU가 적합. (p.5)

> [!question]- Q3. PCIe 병목을 어떻게 우회하나?
> attention을 SSD 내부에서 실행해 KV cache 대부분이 외부 PCIe가 아닌 내부 flash channel(집계 tens of GB/s)로만 이동하게 한다. GPU-CSD 간엔 q,k,v와 attention output만 P2P DMA로 전송한다. (p.2,8)

> [!question]- Q4. SparQ가 있는데 SparF가 필요한 이유는?
> SparQ 등 기존 sparsity는 flash의 page granularity, write amplification, random access 특성을 무시한다. SparF는 16-token group page 저장 + dual-step(page→token) loading으로 random read와 write amplification을 줄여 flash에 맞췄다. (p.6-7)

> [!question]- Q5. K cache를 두 번 저장하는 이유는?
> SparF가 K를 token-index(top-k token fetch)와 hidden embedding-index(approximate score 계산)로 모두 접근해야 한다. flash 저장 비용이 낮으므로 각 방향에 최적화된 두 매핑으로 이중 저장해 random indexing과 write amplification을 동시에 완화한다. (p.7)

> [!question]- Q6. 여러 CSD로 어떻게 확장하나?
> multi-head attention의 각 head는 독립적이므로 n개 CSD에 head를 분산(CSD당 n_head/n head 처리)하고 결과를 GPU에서 concat한다. 의존성이 없어 scaling이 직선적이며, 20 CSD에서 dense 8.99배 개선. (p.8,11)

> [!question]- Q7. dense InstI가 DeepSpeed 대비 4.6%만 빠른데 의미가 있나?
> CSD 내부 대역폭(11.2GB/s)이 GPU-host PCIe(32GB/s)보다 낮기 때문이다. 핵심 이득은 SparF가 KV cache 전송량을 줄이는 데서 나오며, InstI-SparF가 dense InstI 대비 최대 2.08배, FlexGen 대비 11.1배를 달성한다. (p.10)

> [!question]- Q8. 실제 하드웨어 검증은 어떻게 했나?
> InstCSD를 Daisyplus OpenSSD(Xilinx ZU17EG)에 구현했으나 고가·4채널 한계로, NVMeVirt 가상 NVMe + Zynq7045 FPGA(채널 8개·1.4GB/s 확장)로 이식해 실 시스템 통계와 함께 평가했다. (p.9)

---

## 🔗 Connections

[[In-Storage Computing]] · [[LLM Systems]] · [[HPCA]] · [[2025]]

관련: [[Smart-Infinity]] (storage 기반 LLM 가속, near-storage offloading 맥락) · [[Mooncake]] (KV cache-centric 아키텍처 — 본 논문 §VII에서 online/prefilling 중심이라 long-output decoding엔 부적합하다고 대비)

- KV cache offloading 비교 대상: FlexGen, DeepSpeed-MII / Zero-Inference
- sparse attention 계열: SparQ, H2O, Razorattention, Q-hitter, Scissorhands
- CSD/ISC 계열: Smartssd, Cosmos+ OpenSSD, GenStore, Beacongnn

## References worth following

- [50] Ribar et al., **SparQ Attention: Bandwidth-Efficient LLM Inference** (arXiv 2312.04985) — SparF의 기반 알고리즘.
- [53] Sheng et al., **FlexGen** (ICML 2023) — 주 비교 대상, SW stack 기반.
- [31] Lee et al., **InfiniGen: Efficient Generative Inference with Dynamic KV Cache Management** (OSDI 24).
- [49] Qin et al., **Mooncake: Kimi's KVCache-centric Architecture** (arXiv 2407.00079).
- [30] Kwon et al., **vLLM / PagedAttention** (SOSP 2023) — block-granularity KV cache 관리.
- [54] Soltaniyeh et al., **Near-Storage Processing with SmartSSD** (ICPE 2022).
- [27] Kim et al., **NVMeVirt** (FAST 23) — 평가용 가상 NVMe.
- [73] Zhang et al., **H2O: Heavy-Hitter Oracle** (NeurIPS) — accuracy 비교 baseline.

## Personal annotations

<본인 메모 영역>
