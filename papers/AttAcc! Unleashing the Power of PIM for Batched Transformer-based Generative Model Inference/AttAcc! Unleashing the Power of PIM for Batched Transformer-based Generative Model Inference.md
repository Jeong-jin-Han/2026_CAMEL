---
title: "AttAcc! Unleashing the Power of PIM for Batched Transformer-based Generative Model Inference"
aliases: [AttAcc]
description: "배치된 TbGM 추론에서 memory-bound한 attention layer를 HBM 기반 PIM(AttAcc)으로, compute-bound한 FC layer를 GPU/TPU로 처리하는 GPU-PIM 이종 시스템 제안."
venue: ASPLOS
year: 2024
tier: deep
status: done
tags:
  - paper
  - cluster/isc
  - topic/pim
  - topic/llm
  - topic/transformer
  - venue/asplos
  - year/2024
---

# AttAcc! Unleashing the Power of PIM for Batched Transformer-based Generative Model Inference

> **ASPLOS 2024** · `cluster/isc` · Source: [AttAcc! Unleashing the Power of PIM for Batched Transformer-based Generative Model Inference.pdf](<AttAcc! Unleashing the Power of PIM for Batched Transformer-based Generative Model Inference.pdf>)

**저자**: Jaehyun Park\*, Jaewan Choi\*, Kwanhee Kyung, Michael Jaemin Kim, Yongsuk Kwon, Nam Sung Kim, Jung Ho Ahn (Seoul National University; University of Illinois Urbana-Champaign). (\*동등 기여)

## TL;DR
Transformer 기반 생성 모델(TbGM)의 Gen stage는 **attention layer** 때문에 GPU에서 비효율적이다. attention은 요청마다 고유한 KV matrix를 쓰므로 batching으로도 throughput이 안 오르고(weight 재사용 불가), KV matrix가 memory capacity를 잡아먹어 batch size와 SLO를 함께 제약한다. AttAcc는 attention layer를 **HBM 기반 PIM**으로, FC layer를 GPU(xPU)로 처리하는 이종 시스템을 제안하여, 같은 memory capacity(1,280GB) 기준 GPT-3 175B에서 최대 **2.81배 성능 / 2.67배 에너지 효율** 향상을 달성한다.

## 문제 & 동기
TbGM 추론은 입력 시퀀스를 이해하는 한 번의 **Sum(summarization) stage** 와, 토큰을 하나씩 만드는 반복적인 **Gen(generation) stage** 로 구성된다. 순차적 특성 때문에 Gen stage가 전체 실행 시간을 지배한다(GPT-3 175B에서 L_in=L_out=32일 때 Gen이 96% 이상). 각 decoder는 attention block(QKV generation FC, attention layer, projection FC)과 feedforward block(FF1, FF2 FC)으로 이루어진다.

- **FC layer**: batching으로 weight를 재사용하면 GEMV→GEMM이 되어 arithmetic intensity가 올라가 throughput 개선. batch 256에서 GPU utilization 71%, throughput 88배 향상.
- **attention layer**: KV matrix가 요청마다 **고유**하여 재사용 불가. GEMV(score/context) 연산으로 Op/B ~1의 memory-bound. batching해도 throughput이 안 오르고, GPU compute unit은 거의 idle.
- **두 가지 제약**: (1) KV matrix가 memory capacity를 폭증시킴 — GPT-3 (2048,2048) 1요청당 18GB, 640GB DGX에선 batch 18이 한계. (2) SLO 제약 — capacity가 충분해도 attention 실행 시간이 batch와 함께 늘어 SLO를 위반(50ms SLO면 max batch 27).

> [!quote]- 📄 원문 표현 (paper)
> "However, the Key and Value (KV) matrices of the attention layer are *unique* per (inference) request. That is, batching can neither reuse the KV matrices nor improve the throughput of processing the attention layer." (p.2)
>
> "the attention layer limits the maximum batch size and impacts the FC layer throughput due to two critical constraints: memory capacity and service level objective (SLO)." (p.2)
>
> "even when the batch size reaches 256, the utilization remains below 20% because of the substantial time spent on the attention layer." (p.4)

## 핵심 통찰

> [!note]- 통찰 1 — attention은 PIM에 이상적인 워크로드
> attention 가속이 PIM에 적합한 두 가지 이유: (1) KV matrix는 Sum stage에서 DRAM에 한 번 쓰이고(external bandwidth), Gen stage에서 여러 번 읽힌다(internal on-chip bandwidth). (2) GEMV는 reduction 연산이므로 input/output은 작고 큰 KV matrix는 DRAM 안에 머문다 → external bandwidth 소비 없이 처리 가능. external/internal bandwidth 소비 비율은 (d_emb + N_head·L)/(L·d_emb)로, GPT-3 175B에서 최대 1/128까지 작아진다. (p.5)

> [!note]- 통찰 2 — 이종 시스템으로 역할 분담
> memory-bound한 attention은 high-bandwidth PIM(AttAcc)이, compute-bound한 batched FC는 powerful한 xPU(GPU/TPU)가 맡는다. AttAcc만으로 attention 실행 시간을 줄여 SLO 하에서 max batch size를 늘릴 수 있고, 이는 FC layer throughput까지 끌어올린다. (p.2)

> [!note]- 통찰 3 — pipelining/co-processing으로 양쪽 활용도 극대화
> head-level pipelining으로 attention(AttAcc)과 FC(xPU)를 겹쳐 실행하고, feedforward co-processing으로 idle한 AttAcc에 FF FC 일부를 offload하여 두 자원의 활용도를 동시에 높인다. (p.6)

## 설계 / 메커니즘

> [!abstract]- AttAcc 마이크로아키텍처 — GEMV unit / softmax unit 배치
> 8-Hi HBM3 기반(DRAM die 8장 + buffer die, TSV 3D 적층). 두 종류의 PU: **GEMV unit**(score/context의 GEMV)과 **softmax unit**.
> - **GEMV unit 배치 3안 탐색**: AttAcc_buffer(buffer die의 pCH당), AttAcc_BG(DRAM die의 GBUS CTRL/bank group당), AttAcc_bank(bank당). 깊을수록 internal bandwidth↑·energy↓지만 area overhead↑(DRAM process). EDAP 평가 결과 **AttAcc_bank 선택** — area overhead를 10.84%로 제한하면서 성능·에너지 우위.
> - **softmax unit**: **buffer die에 배치**. GEMV(score)→softmax→GEMV(context) 데이터가 같은 die에 모여 불필요한 BG/die 간 전송 제거. softmax unit 수는 head 수 기준(최대 4,800), buffer die가 aggregate internal bandwidth의 1/9를 제공해 충분. DRAM process는 복잡한 softmax 로직에 부담 → buffer die의 logic process 사용. (p.5–7)

> [!abstract]- 데이터 매핑 (KV matrix)
> 계층적 매핑: (1) HBM level — head 단위로 HBM에 매핑(head-wise partitioning), KV matrix와 Q가 같은 HBM에 있으면 GEMV_score/context 독립 실행 가능, HBM 간 전송 제거. 각 head는 L에 따라 크기가 다르므로 Sum stage에서 greedy 배치로 load imbalance 최소화. (2) pCH/BG/bank level — row-wise(input vector 전송↓) vs column-wise(output vector 전송↓) 선택. K_i^T는 (column,column,row), V_i는 (row,row,column) partitioning. (3) multiplier level — K_i^T는 row-wise, V_i는 column-wise(append되는 KV vector의 load imbalance 방지). (p.7)

> [!abstract]- 컴포넌트 / 프로그래밍 모델
> - **GEMV unit**: 16 FP16 multiplier + 16 FP16 adder, double-buffered 16×256-bit buffer, control unit. row-wise는 adder tree, column-wise는 accumulator로 동작(input에 multiplexer).
> - **softmax unit**: 256 FP32 exponent/adder/multiplier + comparator tree + divider, 512KB buffer. 3-stage pipeline(max → exp → normalize).
> - **accumulator**: row-wise partitioning 시 GEMV/softmax 결과를 BG/pCH 단위로 reduce.
> - **AttAcc controller**: I/O module, DMA engine, instruction queue, HBM controller. PIM command(PIM_SET_CONFIG, PIM_ACT_AB, PIM_MAC_AB, PIM_SFM 등)를 표준 HBM command path로 발행.
> - **프로그래밍 모델**: CUDA/OpenCL 유사. host가 Att_inst를 AttAcc controller에 보냄. AttAcc::SetModel/UpdateRequest(config), AttAcc::MemCopy(DMA 데이터 이동), AttAcc::RunAttention(head별 attention offload). (p.7–8)

> [!abstract]- pipelining & co-processing
> - **Attention-level pipelining**: 단일 HBM 내에서 GEMV와 softmax를 head 간 겹침(2 head 이상 필요).
> - **Head-level pipelining**: FC(xPU)와 attention(AttAcc)을 head granularity로 겹침. xPU는 SRAM 용량 한계로 head 일부만 생성, 생성 즉시 AttAcc가 해당 head의 attention 스케줄.
> - **Batch-level pipelining**: 두 batch 동시 처리 가능하나 batch size가 반으로 줄어 FC throughput 악화 → 실험 환경에서 해롭다고 결론.
> - **Feedforward co-processing**: layernorm 때문에 FF block은 attention과 overlap 불가 → idle한 AttAcc에 FF FC 일부 offload. FF1은 column-wise, FF2는 row-wise partitioning으로 중간 GELU output이 xPU/AttAcc 안에 머물도록. BW_xPU·BS : BW_AttAcc×1 비율로 batch size(BS)에 따라 분할 결정. (p.6)

## 평가

> [!success]- 실험 설정 & 주요 결과 (수치)
> - **비교 대상**: DGX×AttAccs (DGX A100 + AttAcc) vs DGX_Base(640GB) vs DGX_Large(1,280GB). DGX×AttAccs와 DGX_Large는 모두 1,280GB(40 HBM)이나, DGX×AttAccs는 DGX가 weight, AttAcc가 나머지(KV) 담당. HBM3 5.2Gbps/pin. AttAcc aggregate internal bandwidth **242 TB/s = DGX_Base의 9배**. (p.8)
> - **모델**: LLAMA 65B(FP16), GPT-3 175B(FP16), MT-NLG 530B(INT8). in-house simulator(Ramulator 기반, OPT 66B로 GPU 검증), 7nm ASAP7로 Verilog 합성. (p.9)
> - **throughput (SLO 없음)**: DGX×AttAccs가 같은 batch에서 DGX_Base 대비 **4.84배**, DGX_Large 대비 **2.48배** (GPT-3 175B, attention 가속 덕분). (p.9)
> - **최적화 효과(naive 대비)**: head-level pipelining 최대 1.15배, feedforward co-processing 추가 1.10배. 결합 시 DGX_Base 대비 **LLAMA 65B 3.49배 / GPT-3 175B 3.91배 / MT-NLG 530B 5.93배**, DGX_Large 대비 2.81×/2.39×/2.01×. (p.9)
> - **에너지**: energy/token이 DGX_Base 대비 최대 **66.7%(LLAMA)/65.9%(GPT-3)/66.8%(MT-NLG)** 절감. (p.10)
> - **SLO 하 throughput**: SLO 30ms일 때 GPT-3 (2048,2048)에서 DGX_Large는 batch 1(capacity 충분해도 48 불가), DGX×AttAccs는 attention 가속으로 batch 제약 완화 → DGX_Large 대비 **최대 56배** throughput. (p.10)
> - **bit-width 민감도**: bit-width 감소 시에도 DGX_Base 대비 최대 3.47배, DGX_Large 대비 2.59배. (p.10)
> - **다른 DGX 옵션 대비**: 2×DGX 대비 최대 1.72배. CPU에서 attention 실행(DGX_CPU)은 CPU의 낮은 bandwidth로 DGX_Base보다 느림. (p.13)
> - **area overhead**: HBM당 PU 면적 = DRAM die당 13.12mm²(121mm² HBM3의 10.84%) + buffer die당 1.40mm²(7nm logic). (p.13)

## 섹션 노트
- **§1 Introduction / §2 Background**: TbGM = Sum stage + 반복 Gen stage. Gen은 GEMV 위주, Sum은 GEMM 위주. GPT-3 175B에서 Gen이 실행 시간 지배(Fig.2).
- **§3 Benefits and Limits of Batching**: batching은 FC에는 이득(GEMV→GEMM, weight 재사용), attention에는 무익(KV unique). attention이 memory capacity와 SLO로 batch size 제약 + FC throughput까지 저하. roofline(Fig.3)에서 attention은 batch 무관하게 같은 지점(memory-bound).
- **§4 TbGM Accelerator Architecture**: 이종 플랫폼(Fig.5). PIM 적합 이유, GEMV/softmax unit 배치 탐색(AttAcc_bank 선택), KV 데이터 매핑.
- **§5 AttAcc Implementation**: GEMV unit / softmax unit / accumulator / controller 마이크로아키텍처, PIM command, 프로그래밍 모델 & 실행 흐름.
- **§6 Maximally Utilizing AttAcc**: attention/head/batch-level pipelining, feedforward co-processing.
- **§7 Evaluation**: 성능/에너지/SLO/bit-width/area.
- **§8 Discussion**: GQA/MQA(KV 공유 시 AttAcc 이득 감소, baseline은 cache 재사용 가능; systolic array로 대응 가능), bulk bitwise(attention 곱셈 지배라 bank-level parallelism이 우위), training(forward는 compute-intensive라 부적합, RLHF류 fine-tuning은 적합).
- **§9 Related Work**: TransPIM(batching 없음), StepStone(GEMM, 큰 batch attention 미해결), A³/ELSA/SpAtten/Sanger(pruning), GOBO/Olive(quantization), DFX(FPGA). AttAcc는 batching에 집중해 큰 TbGM의 attention을 가속하는 첫 PIM(CAL 2023의 확장).

## 핵심 용어
- **TbGM (Transformer-based Generative Model)**: GPT처럼 Transformer decoder 기반의 생성 모델. Sum stage 1회 + Gen stage 반복으로 추론.
- **Sum stage / Gen stage**: 입력 시퀀스 이해(GEMM 위주) / 토큰 자기회귀 생성(GEMV 위주).
- **attention layer vs FC layer**: score(QK^T)·softmax·context(SV)의 memory-bound GEMV / QKV·projection·FF1·FF2의 compute-bound GEMM(batched).
- **KV matrix**: 각 head의 K_i, V_i. 요청마다 고유, L에 비례해 크기 증가 → capacity·SLO 병목의 핵심.
- **Op/B (Operation per Byte)**: arithmetic intensity. FC는 batching으로 ↑, attention은 ~1로 고정.
- **PIM (Processing-in-Memory)**: 메모리 내부에 PU를 두어 internal bandwidth를 활용하는 아키텍처.
- **AttAcc_buffer / AttAcc_BG / AttAcc_bank**: GEMV unit을 buffer die / bank group / bank 위치에 두는 설계 후보. 본 논문은 bank 선택.
- **xPU**: GPU/TPU 등 고성능 compute platform.
- **head-level pipelining / feedforward co-processing**: attention과 FC를 head 단위로 겹치기 / idle한 AttAcc에 FF FC offload.
- **SLO (Service Level Objective)**: 토큰당 latency 목표. attention 실행 시간이 batch와 함께 늘어 batch size를 제약.

## 강점 · 한계 · 열린 질문
**강점**
- attention의 본질적 특성(KV unique, GEMV reduction)을 정확히 짚어 PIM 적합성을 정량화(external/internal 비율 최대 1/128).
- 단순 가속이 아니라 SLO·capacity 제약 완화 → FC throughput까지 끌어올리는 이종 시스템 관점.
- GEMV/softmax unit 배치를 EDAP로 체계적으로 탐색, 실제 HBM3 구조에 충실(JEDEC IDD7 power budget 고려).
- pipelining/co-processing으로 양쪽 자원 idle 최소화.

**한계**
- **GQA/MQA**에서 KV 공유로 baseline이 cache 재사용 가능 → AttAcc 이득 감소(저자도 인정, MHA가 주 타깃). 현대 LLM 다수가 GQA 사용.
- 실제 칩이 아닌 in-house simulator + 합성 기반 평가.
- batch-level pipelining은 실험 환경에서 해롭다고 결론 → 일반성 제한.
- training의 forward pass는 compute-intensive라 부적합.

**열린 질문**
- GQA가 표준이 된 환경에서 systolic-array GEMV unit이 실제로 경쟁력 있는가?
- CXL/NVLink 등 인터커넥트 대역폭이 head-level pipelining의 실효 이득을 어디까지 제약하는가?
- INT8/저비트 양자화 + bulk bitwise PIM과의 결합 가능성은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 왜 batching이 FC에는 효과적이고 attention에는 무익한가?
> 답: FC는 weight matrix가 모든 요청에 공유되어, batching하면 weight를 한 번 읽어 여러 요청이 재사용(GEMV→GEMM, Op/B↑). 반면 attention의 KV matrix는 요청마다 고유하여 재사용할 수 없고, batch가 커져도 attention throughput은 오르지 않으며 실행 시간만 선형 증가한다.

> [!question]- Q2. attention layer가 batch size를 제약하는 두 가지 경로는?
> 답: (1) capacity — KV matrix가 매우 커서(GPT-3 (2048,2048) 1요청당 18GB) memory capacity를 초과(640GB DGX는 batch 18 한계). (2) SLO — capacity가 충분해도 attention 실행 시간이 batch와 함께 늘어 토큰당 latency 목표를 위반(50ms SLO면 batch 27).

> [!question]- Q3. attention이 PIM에 적합한 이유는 무엇인가?
> 답: KV matrix는 Sum stage에서 DRAM에 한 번 쓰고 Gen stage에서 여러 번 읽으므로 internal bandwidth로 처리 가능하고, GEMV는 reduction이라 input/output이 작아 external bandwidth 소비 없이 KV가 DRAM 안에 머문다. external/internal 비율이 최대 1/128까지 작다.

> [!question]- Q4. GEMV unit과 softmax unit을 각각 어디에 배치했고 그 이유는?
> 답: GEMV unit은 **bank(AttAcc_bank)**에 — EDAP 평가에서 area overhead 10.84%로 제한하며 성능·에너지 우위. softmax unit은 **buffer die**에 — score/context GEMV 결과가 같은 die에 모여 BG/die 간 불필요한 전송을 제거하고, buffer die의 logic process가 복잡한 softmax 로직에 적합.

> [!question]- Q5. head-level pipelining과 feedforward co-processing은 각각 무엇을 해결하나?
> 답: head-level pipelining은 xPU(FC)와 AttAcc(attention)를 head 단위로 겹쳐 실행해 양쪽 idle 시간을 줄인다(최대 1.15배). feedforward co-processing은 layernorm 때문에 attention과 겹칠 수 없는 FF block의 FC 일부를 idle한 AttAcc에 offload하여 활용도를 높인다(추가 1.10배).

> [!question]- Q6. DGX×AttAccs와 DGX_Large는 같은 1,280GB인데 왜 성능이 다른가?
> 답: 둘 다 40 HBM·1,280GB이지만, DGX_Large는 모든 메모리를 GPU가 쓰는 반면 DGX×AttAccs는 GPU가 weight를, AttAcc가 KV를 담당한다. AttAcc의 aggregate internal bandwidth(242 TB/s, DGX_Base의 9배)로 attention을 가속해 같은 batch에서 GPT-3 175B 기준 2.48배 throughput을 낸다.

> [!question]- Q7. GQA/MQA에서 AttAcc 이득이 줄어드는 이유는?
> 답: MQA/GQA는 여러 head가 KV matrix를 공유하므로 baseline GPU가 on-chip cache로 KV를 재사용할 수 있다(arithmetic intensity↑). AttAcc의 강점은 high internal bandwidth인데, 공유 그룹이 커질수록(MQA에 가까울수록) baseline이 재사용으로 따라잡아 이득이 감소한다.

> [!question]- Q8. SLO가 타이트할 때 DGX×AttAccs의 throughput 이득이 커지는 이유는?
> 답: SLO가 타이트하면 baseline은 capacity가 충분해도 attention 실행 시간 때문에 작은 batch만 쓸 수 있다(30ms SLO에서 batch 1). AttAcc가 attention을 가속해 SLO를 지키면서 큰 batch가 가능 → FC throughput까지 올라가 DGX_Large 대비 최대 56배까지 향상된다.

## 🔗 Connections
[[In-Storage Computing]] · [[ASPLOS]] · [[2024]]

## References worth following
- [36] Sukhan Lee et al., "Hardware Architecture and Software Stack for PIM ... Industrial Product (HBM-PIM)," ISCA 2021 — JEDEC 표준 호환 상용 PIM, AttAcc의 기반 구조.
- [9] Jaewan Choi et al., "Unleashing the Potential of PIM: Accelerating Large Batched Inference of TbGM," IEEE CAL 2023 — 본 논문의 전신(확장판).
- [69] Minxuan Zhou et al., "TransPIM: A Memory-based Acceleration via SW-HW Co-Design for Transformer," HPCA 2022 — batching 없는 end-to-end Transformer PIM, 비교 대상.
- [66] Gyeong-In Yu et al., "ORCA: A Distributed Serving System for Transformer-Based Generative Models," OSDI 2022 — iteration-level scheduling, batching serving 기준.
- [8] Bahar Asgari (StepStone) — GEMM의 address-mapping-aware PIM, FC offload 비교점.
- [1] Joshua Ainslie et al., "GQA: Training Generalized Multi-Query Transformer Models from Multi-Head Checkpoints," 2023 — AttAcc 이득 감소 시나리오의 핵심.

## Personal annotations
<본인 메모 영역>
