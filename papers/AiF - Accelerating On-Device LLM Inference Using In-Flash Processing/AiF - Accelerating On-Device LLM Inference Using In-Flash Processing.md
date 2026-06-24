---
title: "AiF: Accelerating On-Device LLM Inference Using In-Flash Processing"
aliases: [AiF]
description: "플래시 칩 내부에 GEMV를 직접 통합하고 cr-read·be-enc 두 기법으로 내부 읽기 대역폭 4배·신뢰성을 확보해 on-device LLM 추론을 가속하는 IFP(In-Flash Processing) 솔루션 (ISCA 2025)"
venue: ISCA
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/isc
  - topic/in-flash
  - topic/llm
  - topic/on-device
  - venue/isca
  - year/2025
---

# AiF: Accelerating On-Device LLM Inference Using In-Flash Processing

> **ISCA 2025** · `cluster/isc` · Source: [AiF - Accelerating On-Device LLM Inference Using In-Flash Processing.pdf](AiF%20-%20Accelerating%20On-Device%20LLM%20Inference%20Using%20In-Flash%20Processing.pdf)

저자: Jaeyong Lee, Hyeunjoo Kim, Sanghun Oh, Jihong Kim (Seoul National University), Myoungjun Chun (Soongsil University), Myungsuk Kim (Kyungpook National University). 교신저자: Jihong Kim.

## TL;DR
On-device LLM 추론은 메모리 용량 부족으로 모델 파라미터를 SSD에 offloading하지만, SSD의 낮은 외부 read 대역폭(4~8 GB/s)이 병목이 된다. 기존 ISP(In-Storage Processing)는 SSD 컨트롤러로 데이터를 보내야 해서 flash channel 대역폭(12.8~19.2 GB/s)에 묶이고, 기존 IFP(In-Flash Processing)는 (1) 여전히 부족한 read 대역폭과 (2) LLM 추론의 error 취약성을 해결하지 못한다. **AiF**는 GEMV를 flash chip 내부에서 직접 수행하고, 두 가지 flash read 기법 — **cr-read**(charge-recycling read, precharge/discharge 단계를 재활용해 내부 대역폭 2.8x↑)와 **be-enc**(bias-error encoding, LSB page 위주 배치로 BER 87.5%↓ + 경량 on-chip ECC) — 를 결합해 read 대역폭 4x 향상(read당 에너지 72.1%↓)을 달성한다. 1-TB AiFSSD(16 AiFChip)는 내부 read 대역폭 102.4 GB/s에 도달하며, 8개 실제 LLM 평가에서 baseline SSD offloading 대비 **평균 14.6x** throughput, in-memory inference 대비 **1.4x** higher throughput을 적은 메모리로 달성한다.

## 문제 & 동기
- On-device LLM은 cloud 의존을 줄여 latency·cost·privacy 이점이 크지만, 모델이 수십~수백 GB 파라미터를 요구한다. 고급 스마트폰은 8~12 GB, 노트북은 ~16 GB DRAM뿐이라 용량 제약이 심하다.
- 해결책으로 SSD에 파라미터를 offloading하지만, LLM 추론은 **read intensity가 높고 arithmetic intensity가 낮음(1~2 ops/byte)**. autoregressive 특성상 매 token 생성마다 전체 모델을 읽어야 한다. 식 (1): `max. tokens/s ≤ read bandwidth (GB/s) / model size (GB)`. 40 GB 모델을 SSD로 offload하면 이론적 최대 token rate가 0.1~0.2 tokens/s로 제한된다 (p.4).
- **ISP의 한계**: flash chip에서 읽은 데이터가 flash channel을 거쳐 컨트롤러로 가야 하는데, multi-drop bus topology가 channel당 한 chip만 전송 가능. consumer SSD는 8 channel × 1.6~2.4 GB/s = 12.8~19.2 GB/s aggregate에 그쳐 on-device LLM에 필요한 ~100 GB/s에 한참 못 미친다 (p.4).
- **IFP가 유망한 이유**: 여러 flash chip이 병렬로 동작하는 internal bandwidth는 channel 대역폭보다 훨씬 크다. 게다가 GEMV는 큰 matrix(예: 175-MiB)를 작은 output vector(예: 16-KiB)로 줄여 channel로 내보낼 데이터가 적다 → IFP에 적합 (p.4).
- 그러나 기존 IFP의 두 challenge: **(1) High Read Bandwidth** — 1-TB TLC SSD(16 chip)도 internal bandwidth가 25.6 GB/s에 그쳐 30B 모델에서 0.85 tokens/s 미만. **(2) High Reliability** — LLM은 error에 취약. LLaMA-3 8B(INT8)는 RBER 10⁻⁷에서 정확도가 60% 이상 급락하는데, NAND의 typical RBER은 >10⁻³ (p.4). 고처리량 on-chip ECC는 PPA 부담이 큼 — 102.4 GB/s ECC는 40.12 mm² 면적, 10.694 W 전력 요구(consumer SSD power budget 6~8 W 초과) (p.5).

> [!quote]- 📄 원문 표현 (paper)
> "However, the highly memory-bound nature of on-device LLMs makes inference speed heavily dependent on read bandwidth, leading to significant performance degradation due to the limited bandwidth of SSDs." (p.1, Abstract)
>
> "The key limitation of ISP solutions is that data read from flash chips must traverse bandwidth-constrained flash channels to reach the SSD controller, resulting in a significant bottleneck." (p.2)
>
> "Unlike the error-resilient applications targeted by prior IFP studies, even minimal error rates can cause a significant drop in LLM inference accuracy." (p.2)

## 핵심 통찰
1. **Read 절차 재설계로 internal bandwidth를 풀 수 있다 (cr-read)**: conventional flash read는 precharge/evaluation/discharge 3단계인데, 같은 block 내 연속 WL read에서는 이전 read의 voltage setting을 재활용할 수 있다. discharge를 생략하고 부분 precharge(recycling)만 하면 tR을 64% 줄여 effective bandwidth 6.4 GB/s(conventional 대비 2.8x)를 얻는다. LLM 파라미터는 write-once-read-many 특성이라 matrix를 한 block에 정렬 배치해 bulk read가 가능하다 (p.6).
2. **Page 신뢰성 이질성을 ECC가 아니라 data placement로 활용한다 (be-enc)**: TLC의 LSB/CSB/MSB page는 신뢰성이 다르다. V_TH state encoding을 (2,3,2)에서 LSB-prioritized (1,3,3)으로 바꾸면 LSB page는 SLC처럼 single-sensing read가 되고 BER이 87.5% 감소. LLM 파라미터를 LSB page에만 배치하면 LSB page 전용의 매우 compact한 on-chip ECC(ECC_LITE)로 충분 → PPA 오버헤드 최소화 (p.2, p.7-8).
3. **두 관찰 기반**: (Obs 1) tR 증가는 end-to-end latency에서 amortize되어 user-perceived bandwidth에 미치는 영향이 작다. (Obs 2) 현대 SSD ECC는 실제 RBER보다 훨씬 큰 capability(large ECC margin)를 가져 신뢰성 재분배 여유가 있다 (p.7).
4. **GEMV만 offload하고 vector op은 host에 남긴다**: AiFSSD는 어떤 LLM과도 결합 가능하고(model-agnostic), host와 동시 실행으로 두 대역폭을 동시 활용 가능하다 (p.5).

> [!quote]- 📄 원문 표현 (paper)
> "we observed that most wordlines in the block can reuse existing voltage settings between successive reads without the precharge and discharge steps." (p.4)
>
> "Second, taking advantage of this biased reliability characteristic, AiFSSD strategically stores model parameters only on LSB pages." (p.2)
>
> "Be-enc intentionally introduces heterogeneity in the reliability and performance between pages, storing IFP data (i.e., LLM parameters) only on the most reliable and fastest pages." (p.7)

## 설계 / 메커니즘
- **전체 구조 (Figure 6, p.5)**: Host ↔ SSD Controller ↔ 다수의 AiFChip. matrix는 equal-sized sub-matrix로 나뉘어 AiFChip들에 sequential하게 저장. controller가 sub-vector를 모아 합쳐 host로 반환. AiFChip = ECC_LITE(경량 on-chip ECC decoder) + PEs(Product Elements: multiplier + adder tree).
- **Page Buffer Reuse (p.8)**: dedicated SRAM 대신 각 plane의 기존 page buffer(PB)를 input vector(8-KiB~32-KiB) 적재에 재활용. TLC는 plane당 최소 4 page latch가 있는데 AiFChip은 single-sensing read(be-enc 덕분에 SLC처럼 동작)라 1개 latch만 필요 → 남는 latch를 input vector용으로 전용.
- **On-Chip Computation Flow (Figure 14, Table 1, p.8-9)**: ① controller가 GEMV 명령(input vector 차원, block ID, offset, page 수) 전송 → ② input vector를 PB로 로드 → ③ plane들이 matrix를 연속 read → ④ PE에서 곱셈·누적 → ⑤ 각 segment는 계산 전 ECC_LITE로 error correction(1-KiB당 10-bit error 교정, 6.4 GB/s throughput) → ⑥ output vector를 FIFO에 저장, controller가 round-robin 폴링. **Table 1**: ECC_LITE 0.167 mm²/45.1 mW, PEs 0.026 mm²/3.98 mW, Others 0.016 mm²/2.6 mW → Total 0.209 mm²/51.68 mW (chip 면적의 0.2%). baseline ECC 대비 면적 15.01x↓, 전력 14.83x↓.
- **cr-read (Figure 8, 9, 10, p.6-7)**: ①precharge ②evaluation ③recycling ④evaluation. 연속 WL read 시 discharge 생략, next WL은 V_REF→V_PASS로 부분 충전(t_RECY), 이전 read WL은 V_REF→V_PASS로. 식 (2) conventional `tR=t_PRE+t_EVAL+t_DISCH`, 식 (3) cr-read `tR=t_RECY+t_EVAL`. 검증: 실제 NAND cell array + SPICE. real product의 single sensing read tR=28µs/power 24.22mW와 시뮬레이터 차이 2.9%/0.9%. recycling은 6.04µs delay만 추가.
- **be-enc (Figure 12, 13, p.7-8)**: V_TH state encoding을 (2,3,2)→LSB-prioritized (1,3,3)으로 변경. LSB page=1 sensing(SLC급), MSB page=3 sensing. (1,3,3)에서 LSB error는 V¹_REF에서만 발생(가장 stable한 voltage 범위)→ (2,3,2) 대비 LSB BER 80%↓ (4K P/E + 1년 retention 기준 49→9). **per-block 적용**: non-IFP block(standard I/O, (2,3,2))과 IFP block((1,3,3), LSB=LLM 파라미터/CSB·MSB=일반 data)로 구분, dynamic allocation. flash chip이 이미 runtime V_TH reconfiguration을 지원해 hardware 변경 불필요. drawback: MSB sensing 2→3 증가하나 IFP block에만 선택 적용.
- **System Support (p.9-10)**: NVMe 확장 명령 (i) `aif_post`(optimized layout으로 matrix 저장) (ii) `aif_gemv`(GEMV 수행). host application은 tensor table로 matrix LBA 추적, IFP data를 raw partition/별도 Namespace에 격리. SPDK 같은 userspace NVMe library로 kernel I/O stack bypass.
- **Host-AiFSSD 병렬 실행 (Figure 15, p.9)**: prefill은 host(고연산 활용), decode는 AiFSSD로 offload. multi-GPU scheduling 기법 영감으로 head-level/tensor-level parallelism — 각 attention head(h_i)가 독립적이라 Q/K/V 생성과 MHA overlap, FFN projection matrix를 host/AiFSSD로 분할.

> [!quote]- 📄 원문 표현 (paper)
> "Cr-read is a technique that specializes the flash read sequence for bulk reads within a block." (p.5, §4.2.1)
>
> "By reconfiguring the state encoding, the LSB page requires only one sensing operation, making it similar to SLC NAND, while the sensing count for the MSB page increases from two to three." (p.7-8)
>
> "The AiFChip's estimated area overhead is 0.209 mm², approximately 0.2% of the total flash chip area." (p.9, §4.4)

## 평가
- **방법론 (p.10, Table 2/3)**: NVMeVirt(SSD emulator) + llama.cpp 확장. 1-TB AiFSSD = 16 AiFChip, host memory 8-GB. AiF effective bandwidth = 6.4 GB/s/chip × 16 = 102.4 GB/s. 8 channel/2 chip per channel/4 plane per chip/16-KiB page. tR: (2,3,2) LSB=37µs, (1,3,3) LSB=28µs, cr-read 적용 시 LSB=9.7µs. PCIe 8.0 GB/s, ONFI(flash channel) 2.0 GB/s.
- **비교 시스템**: In-Memory(128-GB DDR5, 이상적), Memory+SSD(8-GB mem + 1-TB SSD offload, baseline), **AiF**, **AiF--**(cr-read·be-enc 없는 버전, 1.6 GB/s×16=25.6 GB/s, ECC_LITE error correction 불가).
- **Throughput (Figure 16, p.11)**: 8개 모델(LLaMA2-7B, LLaMA3-8B, Falcon-11B, LLaMA2-13B, Mixtral-8x7B(MoE), GPT-NeoX-20B, Falcon-40B, LLaMA3-70B). 대표 수치 — LLaMA2-13B: In-Memory 5.5 / Memory+SSD 0.7 / AiF-- 2.9 / **AiF 7.7** tokens/s. Falcon-40B: AiF 2.7 vs Memory+SSD 0.1. GPT-NeoX-20B: AiF 5.7 vs 0.23. **Memory+SSD 대비 평균 14.6x**, **In-Memory 대비 평균 1.4x**(8-GB만으로, 86.5 GB/s memory bandwidth 초과 효과), **AiF-- 대비 평균 2.67x** (p.11).
- **Energy (Figure 17a, p.12)**: cr-read 적용 시 In-Memory 대비 ~2x energy efficiency 개선. cr-read는 flash read energy를 3x 이상 절감. AiF는 In-Memory보다 ~7% less energy.
- **Scalability (Figure 17b, p.12)**: 1/2/4-TB capacity. capacity를 2배로 해도 성능은 1.35~1.68x로 sublinear scaling (vector op 간섭, NVMe control overhead 때문). Mixtral-8x7B 4-TB에서 14.7 tokens/s.
- **Overhead (Figure 18, p.12)**: CONV vs AiFSSD. sequential read 대역폭은 7.6 GB/s로 동일 유지(tR 증가에도). random read는 IOPS 6.8%↓(650→606), latency 9.3%↑(96→105µs) — be-enc IFP block 비율 높을 때 trade-off.
- **cr-read 검증 (Figure 10, p.7)**: tR 64% 감소, read energy 72.1% 감소(18.278 pJ/bit → 5.098 pJ/bit). functionality test에서 data distortion 없음.

> [!quote]- 📄 원문 표현 (paper)
> "Our evaluation results using eight real-world LLMs show that AiF provides a token generation throughput of 5.74 tokens/s for 20B models (2.7 tokens/s for 40B models), delivering over 14.6x the performance of baseline SSD offloading without IFP." (p.2)
>
> "Furthermore, compared to in-memory inference, which stores the entire model in device memory, AiF achieves 1.4x higher throughput while significantly reducing memory footprint." (p.2)
>
> "cr-read reduces tR by 64%. This substantial reduction in tR allows the AiFChip to achieve an effective bandwidth of 6.4 GB/s ... representing a 2.8x improvement over conventional flash chips." (p.7)

## 섹션 노트
- **§1 Introduction (p.1-2)**: on-device LLM 동기, SSD offloading 병목, ISP/IFP 한계, AiF 두 기법 소개.
- **§2 Background (p.2-3)**: NAND organization(Figure 1), read operation 3단계 precharge/evaluation/discharge(Figure 2), MLC technology와 TLC V_TH 분포·page(LSB/CSB/MSB, Figure 3).
- **§3 Motivation (p.3-4)**: §3.1 on-device LLM challenge(식 1), §3.2 ISP vs IFP 전략(Figure 4 internal bandwidth/RBER 영향), §3.3 IFP 두 challenge(bandwidth, reliability), ECC PPA 문제(Figure 5).
- **§4 AiF (p.5-9)**: §4.1 overview(Figure 6/7), §4.2 cr-read(Figure 8/9/10), §4.3 be-enc(Figure 11/12/13), §4.4 AiFChip design(Figure 14, Table 1).
- **§5 Integration (p.9-10)**: §5.1 워크플로(prefill/decode, Figure 15), §5.2 시스템 지원(NVMe 확장, SW 수정, app 요구사항).
- **§6 Evaluation (p.10-12)**: §6.1 setup(Table 2/3), §6.2 결과(Figure 16/17/18).
- **§7 Related Work (p.12-13)**: on-device LLM(quantization, I/O pipelining), in-storage/in-flash processing.
- **§8 Conclusion (p.13)**: 14.6x throughput, in-memory 대비 1.4x.

## 핵심 용어
- **IFP (In-Flash Processing)**: flash chip 내부에 accelerator를 통합해 internal bandwidth를 직접 활용하는 방식. ISP(컨트롤러 처리)와 달리 flash channel 병목을 회피.
- **GEMV (General Matrix-Vector Multiplication)**: matrix × vector. LLM 추론 디코드 단계 연산의 대부분이며 큰 입력을 작은 출력으로 줄여 IFP에 적합.
- **cr-read (charge-recycling read)**: 같은 block 내 연속 WL read에서 precharge/discharge를 재활용(recycling phase)해 tR을 줄이는 read 명령. internal bandwidth 2.8x↑.
- **be-enc (bias-error encoding)**: TLC V_TH state encoding을 (2,3,2)→(1,3,3) LSB-prioritized로 바꿔 LSB page를 SLC급으로 만들고 BER을 낮추는 인코딩. LLM 파라미터를 LSB page에 배치.
- **ECC_LITE**: LSB page만 처리하는 경량 on-chip ECC decoder(BCH 기반, 1-KiB당 10-bit, 6.4 GB/s). baseline 대비 면적 15.01x·전력 14.83x 절감.
- **tR**: flash read latency. cr-read에서 t_RECY+t_EVAL로 단축.
- **RBER (Raw Bit Error Rate)**: ECC 적용 전 raw bit error 비율. NAND은 typical >10⁻³, LLM 정확도는 10⁻⁷부터 급락.
- **Page Buffer Reuse**: 별도 SRAM 대신 plane의 page latch를 input vector 적재에 재활용(single sensing 덕분).
- **AiFChip / AiFSSD**: GEMV·cr-read·be-enc 지원 flash chip / 그 chip들로 구성된 SSD.
- **head-/tensor-level parallelism**: host와 AiFSSD 동시 실행을 위해 attention head·projection matrix를 분할하는 스케줄링.

## 강점 · 한계 · 열린 질문
**강점**
- 실제 NAND cell array 측정 + SPICE/EDA 시뮬레이션으로 cr-read·be-enc 물리적 검증(시뮬레이터 오차 tR 2.9%/power 0.9%).
- ECC PPA 문제를 hardware ECC 강화가 아닌 data placement(be-enc)로 우회 — 0.2% chip area, 51.68 mW로 매우 가벼움.
- model-agnostic(GEMV만 offload), MoE(Mixtral) 아키텍처와 잘 맞음(고용량+고대역폭).
- in-memory보다 빠르면서 메모리 footprint 대폭 절감(8-GB).

**한계**
- be-enc는 MSB page sensing 2→3 증가, IFP block의 random read IOPS 6.8%↓/latency 9.3%↑ (random read-intensive 워크로드에 trade-off).
- capacity scaling이 sublinear(2x 용량 → 1.35~1.68x) — vector op 간섭, NVMe control overhead.
- cr-read는 같은 block 내 연속 WL read에만 적용 가능(LLM의 write-once-read-many 특성에 의존). 일반 워크로드엔 효과 제한적.
- GC가 IFP block에 fragmentation을 유발할 수 있어 layout 유지 위해 page copy 시 LSB order 보존 필요.

**열린 질문**
- 더 큰 모델·더 많은 chip에서 control overhead를 줄이는 host/SSD scheduling 최적화(저자도 future work 명시).
- be-enc의 MSB 신뢰성 저하가 일반 user data에 미치는 장기 영향(retention/endurance) 정량화.
- quantization(INT4 등)과 be-enc 결합 시 정확도/대역폭 trade-off.

## ❓ Q&A (자가 점검)
> [!question]- Q1. 왜 ISP가 아니라 IFP를 선택했는가?
> ISP는 flash chip에서 읽은 데이터가 multi-drop bus인 flash channel(channel당 1 chip만 전송)을 거쳐 컨트롤러로 가야 해서 12.8~19.2 GB/s aggregate에 묶인다. on-device LLM은 ~100 GB/s가 필요하다. IFP는 여러 flash chip의 internal bandwidth를 직접 활용하고, GEMV가 큰 matrix를 작은 vector로 줄여 channel로 내보낼 데이터가 적어 적합하다.

> [!question]- Q2. cr-read의 핵심 아이디어와 효과는?
> conventional read의 precharge/evaluation/discharge 중 discharge를 생략하고, 같은 block 내 연속 WL read에서 이전 voltage setting을 재활용(recycling phase)한다. t_RECY는 단일 WL만 V_REF→V_PASS로 부분 충전하면 되어 t_PRE보다 짧다. 결과적으로 tR 64% 감소, effective bandwidth 6.4 GB/s(2.8x), read energy 72.1% 절감.

> [!question]- Q3. be-enc는 신뢰성과 ECC 문제를 어떻게 해결하는가?
> TLC V_TH encoding을 (2,3,2)→LSB-prioritized (1,3,3)으로 바꿔 LSB page를 SLC처럼 single-sensing으로 만들고 BER을 80~87.5% 낮춘다. LLM 파라미터를 신뢰성 높은 LSB page에만 배치하면 LSB 전용의 매우 compact한 on-chip ECC(ECC_LITE)로 충분해 고처리량 ECC의 막대한 PPA 부담(40.12 mm²/10.694 W)을 피한다.

> [!question]- Q4. AiFChip의 area/power 오버헤드는 얼마이며 baseline ECC 대비 얼마나 절감되나?
> Table 1: ECC_LITE 0.167 mm²/45.1 mW + PEs 0.026 mm²/3.98 mW + Others 0.016 mm²/2.6 mW = Total 0.209 mm²/51.68 mW로 flash chip 면적의 0.2%. baseline(102.4 GB/s) ECC 대비 면적 15.01x, 전력 14.83x 절감 (be-enc로 ECC capability를 크게 줄인 덕분).

> [!question]- Q5. 대표 throughput 수치와 비교 대상은?
> 8개 실제 LLM에서 baseline SSD offloading(Memory+SSD) 대비 평균 14.6x, in-memory 대비 1.4x, AiF--(기법 없음) 대비 평균 2.67x. 예: LLaMA2-13B 7.7 tokens/s(Memory+SSD 0.7), GPT-NeoX-20B 5.74, Falcon-40B 2.7 tokens/s.

> [!question]- Q6. 왜 8-GB memory의 AiF가 128-GB in-memory보다 빠를 수 있는가?
> AiFSSD의 internal bandwidth가 102.4 GB/s(cr-read·be-enc로 향상)로 host memory bandwidth 86.5 GB/s를 초과하고, host와 AiFSSD가 head-/tensor-level parallelism으로 동시 추론하여 두 대역폭을 동시 활용하기 때문이다. 작은 모델일수록 host memory에 submatrix가 더 많이 들어가 concurrent inference 효과가 커진다.

> [!question]- Q7. AiF는 어떤 연산을 offload하고 어떤 것을 host에 남기는가?
> on-device LLM 추론의 주 병목인 GEMV만 AiFSSD로 offload하고, normalization·activation 같은 vector operation은 host에 남긴다. 이로써 어떤 LLM과도 결합 가능한 model-agnostic 유연성을 얻고, decode 단계만 offload하며 prefill은 host의 고연산으로 처리한다.

> [!question]- Q8. be-enc 적용의 trade-off(부작용)는?
> LSB를 SLC급으로 만드는 대신 MSB page sensing이 2→3으로 늘어 IFP block의 MSB read latency가 커진다. 그래서 be-enc((1,3,3))는 per-block으로 IFP block에만 선택 적용하고, IFP block의 random read는 IOPS 6.8%↓·latency 9.3%↑가 발생한다(sequential read 대역폭은 유지).

## 🔗 Connections
[[In-Storage Computing]] · [[ISCA]] · [[2025]]

## References worth following
- **[3] Alizadeh et al., "LLM in a Flash: Efficient Large Language Model Inference with Limited Memory" (arXiv 2023)** — SSD offloading 기반 on-device LLM의 핵심 선행연구.
- **[41] NVMeVirt (FAST '23)** — 본 논문 평가에 쓰인 software-defined virtual NVMe device emulator.
- **[24] Hu et al., "ICE: An Intelligent Cognition Engine with 3D NAND-based In-Memory Computing for Vector Similarity Search" (MICRO 2022)** — IFP vector search 선행연구.
- **[45] Han et al., "3D-FPIM: An Extreme Energy-Efficient DNN Acceleration System Using 3D NAND Flash-Based In-Situ PIM" (MICRO 2022)** — flash 기반 DNN IFP.
- **[90] Zhao et al., "LDPC-in-SSD: Making Advanced Error Correction Codes Work Effectively in Solid State Drives" (FAST '13)** — SSD ECC margin 관련 근거.
- **[87] Yi et al., "EdgeMoE: Fast On-Device Inference of MoE-based Large Language Models" (2023)** — MoE on-device 추론, AiF의 MoE 적합성과 연결.

## Personal annotations
<본인 메모 영역>
