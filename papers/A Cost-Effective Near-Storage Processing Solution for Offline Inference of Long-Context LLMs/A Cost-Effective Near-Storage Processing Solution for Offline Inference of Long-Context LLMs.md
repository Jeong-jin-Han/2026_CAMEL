---
title: "A Cost-Effective Near-Storage Processing Solution for Offline Inference of Long-Context LLMs"
description: "KV cache I/O가 지배하는 오프라인 장문 컨텍스트 LLM 추론 병목을, 상용 SmartSSD 기반 근접 스토리지 attention 오프로딩(HILOS)으로 해결하는 실시스템 프레임워크"
venue: ASPLOS
year: 2026
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/isc
  - venue/asplos
  - year/2026
  - list/26s-v2
  - topic/near-storage-processing
  - topic/llm-inference
  - topic/kv-cache
  - topic/fpga-accelerator
---

# A Cost-Effective Near-Storage Processing Solution for Offline Inference of Long-Context LLMs

> **ASPLOS 2026** · cluster/isc · Source: [A Cost-Effective Near-Storage Processing Solution for Offline Inference of Long-Context LLMs.pdf](<A Cost-Effective Near-Storage Processing Solution for Offline Inference of Long-Context LLMs.pdf>)

저자: Hongsun Jang (Seoul National University), Jaeyong Song (Seoul National University), Changmin Shin (Seoul National University), Si Ung Noh (Seoul National University), Jaewon Jung (Seoul National University), Jisung Park (POSTECH), Jinho Lee (Seoul National University, corresponding author)

## TL;DR
오프라인 배치 LLM 추론(offloading-based batched inference)에서 KV cache I/O가 실행시간의 60% 이상을 차지하는 병목임을 175B 모델로 정량 규명하고, 상용 SmartSSD 위에 attention 연산만 근접 스토리지로 오프로드하는 attention near storage (ANS)를 제안한다. 여기에 호스트 유휴 자원을 활용하는 cooperative X-cache, 쓰기 지연을 숨기는 delayed KV cache writeback, 자원 제약적 FPGA에 맞춘 memory-efficient attention accelerator를 결합한 HILOS 프레임워크를 16개 SmartSSD 실시스템으로 구현했다. FlexGen/DeepSpeed 대비 최대 7.86배 처리량, 최대 85% 에너지 절감을 달성했으며, 175B 모델급 LLM을 128K 컨텍스트로 단일 A100에서 기존 하드웨어/모델 수정 없이 지원한다.

## 문제 & 동기
LLM 생성 추론은 모델 파라미터와 KV cache의 메모리 풋프린트가 단일 GPU 용량을 초과하기 쉽고, 특히 offline inference(온라인 대비 낮은 TTFT/TPOT 제약이 없어 긴 시퀀스를 허용하는 시나리오, benchmarking·정보 추출 등에 유용, p.5)에서는 이 문제가 더 커진다. FlexGen·DeepSpeed 계열의 offloading-based batched inference는 모델 가중치와 KV cache를 host memory/SSD로 확장해 배치를 키워 처리량을 높이지만, 배치 크기·컨텍스트 길이에 비례해 커지는 KV cache 전송 오버헤드가 발목을 잡는다(p.6). 175B급 OPT 모델을 A100 서버(512GB host memory, PCIe 4.0 SSD 4개)에서 측정한 결과, KV cache가 시스템 메모리 풋프린트를 지배해 테라바이트 규모에 도달하고(Figure 2a, p.7), 장문 컨텍스트에서는 데이터 이동이 실행시간의 60% 이상을 차지한다(Figure 2b, p.7, §3.1).

> [!quote]- 📄 원문 표현 (paper)
> - "Transferring the KV cache consumes over 60% of the total execution time." (p.7)
> - "Our study (§ 3.1) shows that KV cache I/O in the offloading-based batched inference accounts for over 60% of inference time." (p.6)
> - "the system becomes overwhelmingly bottlenecked by data movement" (p.7)

## 핵심 통찰 (Key Insight)

**1. Attention Near Storage (ANS) — attention 연산만 근접 스토리지로**
전체 레이어가 아니라 KV cache를 직접 소비하는 attention 연산만 NSP 가속기로 오프로드하면, 대용량 KV cache 트래픽은 디바이스 내부 경로에 갇히고 시스템 인터커넥트는 QKV 입력과 attention 출력이라는 훨씬 작은 텐서만 지나간다. 컨텍스트 길이 $s$, hidden dim $h$ 기준 baseline 인터커넥트 read 트래픽은 $4sh$바이트인데, ANS는 이를 $2h$바이트(attention 출력)로 대체한다(쓰기 트래픽은 $4h→6h$로 소폭 증가). 트래픽 감소비는 다음과 같이 컨텍스트 길이에 선형 비례한다.

$$\frac{T_{BASE}}{T_{ANS}} = \frac{4sh+4h}{2h+6h} = \frac{s+1}{2} > 1 \quad (\because s>1)$$

**2. Cooperative X-cache — 호스트 유휴 자원의 활용**
ANS를 적용하면 호스트(GPU/CPU)는 decoding 단계에서 QKV projection과 MLP만 수행해 활용률이 20% 미만으로 떨어진다(Figure 4c, p.9). HILOS는 K/V 대신 그 이전 단계인 pre-projection activation $X$를 배치·헤드 차원의 $\alpha$ 비율만큼 host에 캐싱(cooperative X-cache)하고, decoding 시 GPU가 GPUDirect Storage로 $X$를 읽어 $K,V$를 재계산하는 동안 나머지 $1-\alpha$는 NSP가 attention을 수행하도록 병렬화한다. $X$는 $K,V$ 결합 크기의 절반이므로 시스템 인터커넥트·플래시 read 트래픽이 절반으로 줄고, 재계산 latency는 병렬 attention 연산 뒤에 숨는다. 최적 $\alpha$는 대역폭 비율로 결정된다: $\alpha = \dfrac{2B_{PCI}}{B_{SSD}+B_{PCI}}$ (p.9).

**3. Delayed KV Cache Writeback — 쓰기를 critical path에서 제거**
매 decoding step마다 생성되는 새 KV 엔트리(헤드당 256B)는 SSD 페이지 크기(4KiB)보다 훨씬 작아 즉시 쓰면 성능이 나쁘고 critical path에 놓인다. HILOS는 새 KV 엔트리를 host memory 버퍼에 모아두고, spill interval $c$(최적값 16, p.10)마다 페이지 단위로 일괄 기록한다. 버퍼된 값이 반복 재전송되는 중복을 막기 위해, host CPU가 현재 query와 버퍼된 key로 partial $QK^T$ 점수를 미리 계산해 그 결과만 가속기로 보낸다(p.10).

## 설계 / 메커니즘 (Design)
- **전체 디코딩 흐름 (Figure 4a, p.8)**: ① GPU가 host memory에서 attention layer weight 로드 → ② QKV projection 수행 → ③ Q/K/V를 SSD로 전송 → ④ NSP가 SSD에 저장된 전체 KV cache를 자체 DRAM으로 로드 후 attention 계산 → ⑤ attention 출력만 host로 반환 → ⑥ GPU가 MLP weight 로드 → ⑦ MLP 실행.
- **Cooperative X-cache 워크플로 (Figure 5b, p.8-9)**: prefill 단계에서 $\alpha$ 비율의 배치/헤드에 대해 입력 activation $X$를 host에 persist. decoding 시 GPU가 GDS로 $X$를 직접 읽어 $W_K, W_V$로 K/V를 재생성하는 동안, NSP는 나머지 $1-\alpha$ 비율의 저장된 KV cache에 대해 multi-head attention 수행.
- **Delayed writeback 워크플로 (Figure 6b, p.9-10)**: ① 새 KV 엔트리를 host 버퍼에 스테이징 → ② CPU가 버퍼된 key로 partial $QK^T$ 계산 → ③ 기존 K/V는 SSD→가속기로 로드 → ④ 새 query, partial $QK^T$ 스칼라, 새 V를 host에서 가속기로 전송 → ⑤ 가속기가 최종 attention 완성; 버퍼는 spill interval마다 SSD로 방출.
- **Attention 가속기 (Figure 7, p.10)**: Query-Key Product Unit(온라인 in-place transpose로 $K^T$ 저장 없이 128×128 블록 단위 전치), Softmax Statistics Aggregation Unit + Softmax Normalization Unit(2-pass softmax: 1차 패스에서 블록별 local max/합 계산, streaming update로 global max/sum 갱신 — Algorithm 1, p.10), Score-Value Product Unit. GQA 등 grouped-query attention은 $d_{group}$개 query head가 K/V 버퍼를 공유해 중복 read를 없애도록 네이티브 지원(p.11).
- **Full system integration (Figure 8, p.11)**: user-level(HLS 커스터마이징→Vitis 컴파일→비트스트림, lm-evaluation-harness 통합 시뮬레이터로 사이클 정확도 검증, 4K~32K 시퀀스에서 실측 대비 Pearson 상관계수 0.93), middleware(Inference Controller, Weights Prefetcher, Cache Scheduler가 §4.2의 $\alpha$ 자동 선택, Writeback Manager가 임계값 도달 시 비동기 spill), PyTorch/pybind11 연동.
- **하드웨어 테스트베드**: 최대 16개 Samsung SmartSSD(Kintex UltraScale+ KU15P FPGA, 4GB DDR4-2400)를 PCIe 스위치로 연결, P2P 통신으로 트래픽을 디바이스 내부에 국한(p.12, Figure 9a).

> [!quote]- 📄 원문 표현 (paper)
> - "HILOS addresses this issue with attention near storage (ANS), which offloads attention to a custom accelerator near storage. It confines the high-volume KV cache traffic to the device's internal path, allowing only the attention inputs and outputs to traverse the shared system interconnect." (p.8)
> - "we cache the pre-projection input activation X for the historical context instead of the derived K and V tensors. We term this technique cooperative X-cache." (p.9)
> - "we propose delayed KV cache writeback, which addresses the inefficient host-storage data transfer for new KV entries" (p.7)

## 평가 (Evaluation)
- **환경 (Table 1-2, p.12)**: A100(40GB)/H100(80GB) GPU, Xeon Gold 6342, SmartSSD(3.84TB) vs conventional PM9A3 SSD(3.84TB), H3 Falcon 4109 PCIe expansion. 모델: OPT-30B/66B/175B(MHA), Qwen2.5-32B(GQA), Mixtral-8x7B(MoE+GQA), GLaM-143B(MoE+MHA), 배치 16, FP16, 출력 64토큰, 컨텍스트 최대 128K.
- **베이스라인**: FlexGen 3종(host DRAM / 16 SmartSSD SSD offload / 16 PCIe3.0 SSD with FPGA off), DeepSpeed ZeRO-Inference + UVM 확장(DS+UVM(DRAM)).
- **주요 처리량 (Figure 10, p.13)**: HILOS(16 SmartSSD)는 FLEX(SSD) 대비 최대 7.86배 디코딩 처리량. FLEX(16 PCIe3.0 SSD, FPGA 비활성)은 FLEX(SSD)의 0.64~0.94배에 그침(PCIe 대역폭 포화). 4개 SmartSSD에서 FLEX(DRAM) 대비 1.10~1.36배, 16개로 확장 시 1.88~2.49배; FLEX(DRAM)이 배치 1에서도 메모리 고갈로 실패하는 장문 컨텍스트에서는 FLEX(SSD) 대비 5.3~7.8배(p.14).
- **모델 아키텍처 민감도 (Figure 12b, p.14)**: GQA/MoE 모델에서 1.16~3.36배 speedup.
- **Ablation (Figure 15, p.15)**: ANS 단독으로 FLEX(SSD) 대비 최대 3.39배; ANS+WB(delayed writeback)가 추가로 최대 1.32배; ANS+X(cooperative X-cache)가 ANS 대비 최대 1.64배.
- **비용 효율 (Figure 16a, p.15)**: 66B 모델에서 FLEX(SSD) 대비 최대 2.02배 cost-efficiency(tokens/sec/$). FLEX(DRAM)은 DRAM이 충분할 때 1.53배 우위지만 175B에서 역전, HILOS가 최대 1.68배 우위. $7,000 A100→$30,000 H100 업그레이드는 1.39배 speedup에 그치지만 HILOS는 비교 가능한 1.29배 speedup을 2.91배 높은 cost-efficiency로 달성.
- **내구성 (Figure 16b, p.15)**: Azure workload 통계 기반 요청 분류(Short/Medium/Long)로 16개 SmartSSD의 총 처리 가능 요청 수 평가, HILOS가 baseline 대비 1.34~1.47배 endurance 개선(spill interval 16→32 시 추가 1.02~1.05배), 175B Long 요청도 408만 건 이상 지원.
- **에너지 (Figure 17a, p.15)**: NVML(GPU)/RAPL(CPU·DRAM)/PCIe 확장보드 컨트롤러(SmartSSD) 측정, 저처리량 baseline 대비 HILOS가 전체적으로 우수한 에너지 효율(최대 85% 절감, abstract).
- **멀티노드 비교 (Figure 17b, p.15-16)**: vLLM 0.9.1 + FlashAttention + tensor/pipeline parallelism 기반 2노드(노드당 A6000×4, 512GB, EPYC 7302, InfiniBand EDR) 분산 구성 대비 HILOS가 1.64~1.81배 speedup.
- **정확도 (Figure 18c, p.16)**: envisioned ISP-CSD(16TB NAND, 8 flash channel, LPDDR5X 68GB/s) 모델로 FPGA 오버헤드를 OpenROAD 45nm→8nm 스케일 합성 평가(면적 0.47mm², 300MHz에서 1.13W). InstAttention의 lossy sparse KV retrieval(1/8 압축)은 LongBench 5개 데이터셋에서 32K+ 컨텍스트 시 F1 3.52~5.73%p 하락, HILOS는 FlashAttention과 동등한 lossless 정확도 유지.

> [!quote]- 📄 원문 표현 (paper)
> - "HILOS achieves up to 7.86× throughput improvement over state-of-the-art offloading-based inference frameworks while reducing energy consumption by up to 85%." (Abstract)
> - "We demonstrate that HILOS cost-effectively supports 175 billion-parameter LLMs with a 128K context length on a single A100 GPU, without requiring any custom modifications to the existing hardware or model architecture." (p.7)
> - "With InstAttention's default 1/8 compression ratio, the accuracy degrades by 3.52 ∼ 5.73%p." (p.16)

## 섹션 노트
- **§1 Introduction**: offline inference의 특성(높은 latency 허용)과 offloading-based batched inference의 KV cache I/O 병목을 문제로 제시, HILOS 기여 3가지(NSP-offloading 최초, cooperative X-cache, delayed writeback + 효율적 가속기) 요약.
- **§2 Background**: KV cache의 prefill/decoding 두 단계 수식화(Eq.1-2), offloading-based batched inference의 baseline 절차(Figure 1), computational storage device/NSP(SmartSSD) 배경 소개.
- **§3 Motivation**: 175B OPT로 메모리 풋프린트·실행시간 분석(Figure 2), NSP의 장점 3가지(대용량 KV cache 저장, 고집적 대역폭, 호스트 자원 경합 완화, Figure 3의 PCIe 토폴로지 비교).
- **§4 HILOS Design**: ANS(§4.1, 트래픽 감소 수식), Cooperative X-cache(§4.2, 비용모델과 $\alpha$ 도출), Delayed KV Cache Writeback(§4.3), Attention Accelerator Architecture(§4.4, 2-pass softmax·온라인 전치·GQA 지원).
- **§5 Full System Integration**: user-level 설계 흐름, middleware 컴포넌트, 하드웨어 테스트베드, FPGA 구현 세부(dataflow pragma, FP16 저장/FP32 중간연산, 마스킹에 $-10^4$ 사용).
- **§6 Evaluation**: 실험 셋업(§6.1), 구현 결과/리소스 사용(§6.2, Table 3, 자원 활용은 softmax가 DSP 지배), 성능 비교(§6.3), 민감도(§6.4: 배치 크기, 모델 아키텍처, spill interval, X-cache ratio, 출력 길이), 비용·효율 분석(§6.6: cost-effectiveness, endurance, energy, multi-node).
- **§7 Discussion**: NSP→ISP 적용 가능성과 한계(§7.1), 미래 CSD 설계 방향(별도 exponential unit, 분리 clock domain, 더 균형 잡힌 용량/대역폭 트레이드오프, §7.2), CXL 기반 아키텍처로의 확장 가능성(§7.3).
- **§8 Related Work**: LLM inference acceleration, scalable memory/CXL, PIM, NSP 계열 선행연구와의 차별점(KV-cache I/O에 특화된 batched offline inference 타겟) 정리.
- **§9 Conclusion & Artifact Appendix**: 코드 공개(GitHub/Zenodo), HLS 합성·LLM 추론 배포 재현 워크플로 안내.

## 핵심 용어 (Key terms)
- **Near-Storage Processing (NSP)**: 스토리지 장치 내부 또는 인접 위치에 경량 연산 유닛(FPGA 등)을 배치해 호스트-스토리지 데이터 이동을 줄이는 아키텍처.
- **Attention Near Storage (ANS)**: attention 연산만 근접 스토리지 가속기로 오프로드해 KV cache 트래픽을 디바이스 내부 경로에 가두는 HILOS의 핵심 기법.
- **Cooperative X-cache**: pre-projection activation $X$를 host에 캐싱해 GPU가 K/V를 재계산하도록 하고, 이를 NSP의 attention 계산과 병렬화해 호스트 유휴 자원을 활용하는 기법.
- **Delayed KV Cache Writeback**: 새로 생성된 KV 엔트리를 host 버퍼에 모아 spill interval마다 일괄 기록, 쓰기 latency를 critical path에서 제거하는 기법.
- **Offloading-based Batched Inference**: GPU 메모리를 host DRAM/SSD로 확장해 배치를 키워 처리량을 높이는 오프라인 추론 방식(FlexGen, DeepSpeed ZeRO-Inference 등).
- **SmartSSD**: 온보드 FPGA와 DRAM을 갖춘 Samsung 상용 computational storage device, P2P PCIe로 NAND-DRAM 직접 데이터 이동 지원.
- **Two-Pass Softmax**: 온라인 softmax 알고리즘을 블록 단위 local max/sum 집계(1차) + 정규화(2차)로 축소해 on-chip memory 요구를 낮춘 하드웨어 구현(Algorithm 1).
- **GQA (Grouped-Query Attention)**: 여러 query head가 하나의 K/V head를 공유하는 attention 변형; HILOS 가속기는 $d_{group}$ 병렬 MAC 유닛으로 네이티브 지원.
- **Spill interval ($c$)**: delayed writeback에서 몇 decoding step마다 버퍼를 SSD로 방출할지 결정하는 파라미터(최적값 16).
- **X-cache ratio ($\alpha$)**: 배치·헤드 차원 중 cooperative X-cache(재계산) 방식으로 처리하는 비율, $B_{SSD}/B_{PCI}$ 비율에 따라 자동 선택.

## 강점 · 한계 · 열린 질문
- **강점**: 시뮬레이션이 아닌 상용 SmartSSD 16개 실시스템 + PyTorch 완전 통합 프레임워크로 구현·공개(GitHub/Zenodo). ANS/X-cache/delayed writeback 각각을 ablation(Figure 15)으로 기여도 분리 검증. 처리량뿐 아니라 cost-effectiveness, SSD endurance, energy, 멀티노드 GPU 대안과의 비교까지 다각도 평가. InstAttention 등 손실 압축 방식과 달리 lossless 정확도를 LongBench로 실증(Figure 18c).
- **한계**: 저자 스스로 인정하듯 현재 NSP(SmartSSD) 구조는 conventional 장치 대비 내부 스토리지 대역폭 자체를 늘리지 못하는 한계를 그대로 물려받음(§7.1). PCIe 5.0급 4배 대역폭 확장 시 softmax의 DSP 요구량이 2,000개를 넘어 현 SmartSSD 자원을 초과(§7.2). 평가된 장문 컨텍스트(최대 128K)는 "일부 모델은 이보다 짧게 사전학습되었지만, 향후 확장된 컨텍스트 모델을 예상해" 벤치마크한 것(p.12)이라 실제 초장문 학습 모델 기준 검증은 아님. Storage capacity 상당 부분(4TB 중 600GB 미만만 사용)이 미활용 상태로 남아 있음(§7.2).
- **열린 질문**: CXL.mem 기반 재설계(명시적 DMA 제거, 통합 주소공간)가 실제로 어느 정도 이득을 줄지는 정성적 논의(§7.3)에 그침, 정량 검증 없음. 미래 단일 ISP 디바이스(16TB NAND, 8채널)는 emulate(Figure 18b)만 되었고 실측 검증은 부재. "더 균형 잡힌" CSD(용량↓, 내부 대역폭·연산력↑) 설계가 실제 배포에서 얼마나 비용 이점을 주는지는 미해결.

## ❓ Q&A (자가 점검)
> [!question]- HILOS가 해결하려는 근본 병목은 무엇이며 어떻게 정량화했는가?
> KV cache I/O. 175B OPT 모델을 A100 서버(512GB host mem, PCIe 4.0 SSD 4개)에서 실측한 결과 KV cache 전송이 전체 실행시간의 60% 이상을 차지함을 확인했다(Figure 2b, p.7, §3.1).

> [!question]- ANS는 정확히 무엇을 근접 스토리지로 오프로드하고, 무엇을 GPU/CPU에 남기는가?
> attention 연산(QK^T, softmax, score-value product)만 NSP 가속기로 옮긴다. QKV projection과 MLP는 그대로 GPU에서 수행하며, 가속기와는 QKV 입력과 attention 출력만 주고받는다(Figure 4a, p.8).

> [!question]- cooperative X-cache가 K/V 대신 pre-projection activation X를 캐싱하는 이유는?
> X는 K,V 결합 텐서 크기의 절반이라 저장·전송 트래픽을 절반으로 줄이고, 동시에 decoding 중 유휴 상태인 GPU 컴퓨트 자원을 K/V 재계산에 활용해 NSP의 attention 계산과 병렬화(latency 은폐)할 수 있기 때문이다(§4.2, p.8-9).

> [!question]- delayed KV cache writeback에서 버퍼링이 유발하는 부작용과 그 해결책은?
> 버퍼된 새 KV 값이 spill 전까지 매 decoding step마다 중복 전송되는 문제가 생긴다. 이를 완화하기 위해 host CPU가 현재 query와 버퍼된 key로 partial $QK^T$ 점수를 미리 계산해, 스칼라 결과만 가속기로 보낸다(p.10).

> [!question]- HILOS의 2-pass softmax가 전통적 3-pass 방식 대비 갖는 이점은?
> 온라인 softmax처럼 블록 단위 local max로 근사한 뒤 streaming update로 global max/sum을 갱신해, 전체 시퀀스에 걸친 global max를 먼저 구하는 pass를 없앤다. 이를 통해 on-chip 메모리 요구량이 시퀀스 길이에 따라 커지는 문제를 줄인다(Algorithm 1, §4.4, p.10-11).

> [!question]- 175B 모델에서 HILOS의 cost-efficiency는 baseline·H100 업그레이드 대비 어떤 수치를 보이는가?
> FLEX(SSD) 대비 최대 1.68배 cost-efficiency 향상(175B에서 FLEX(DRAM)은 용량 한계로 열위). A100→H100($30,000) 업그레이드는 1.39배 speedup만 주지만, HILOS는 비슷한 1.29배 speedup을 2.91배 높은 cost-efficiency로 달성한다(Figure 16a, p.15).

> [!question]- 저자들이 스스로 인정한 한계는 무엇인가?
> (1) 현 NSP(SmartSSD)는 conventional 장치 대비 내부 스토리지 대역폭 자체를 늘리지 못함, (2) PCIe 5.0급 대역폭 확장 시 softmax의 DSP 요구가 현재 자원(2,000+ DSP 필요)을 초과, (3) 4TB 중 600GB 미만만 쓰이는 등 storage capacity가 상당 부분 미활용(§7.1-7.2, p.16-17).

> [!question]- InstAttention과 비교했을 때 HILOS의 정확도 측면 이점은?
> InstAttention은 자원 제약 때문에 sparse KV retrieval(기본 1/8 압축)을 쓰는데, 32K+ 컨텍스트의 LongBench 5개 데이터셋에서 F1이 3.52~5.73%p 하락한다. HILOS는 FlashAttention과 동등한 lossless attention을 유지한다(Figure 18c, p.16).

## 🔗 Connections
[[In-Storage Computing]] · [[ASPLOS]] · [[2026]]
관련: [[InstAttention - In-Storage Attention Offloading for Cost-Effective Long-Context LLM Inference]] · [[Smart-Infinity - Fast Large Language Model Training using Near-Storage Processing on a Real System]] · [[OmniCache - Collaborative Caching for Near-storage Accelerators]]

## References worth following
- Sheng et al., "FlexGen: High-Throughput Generative Inference of Large Language Models with a Single GPU," ICML 2023 — HILOS가 통합·비교하는 offloading-based batched inference의 기준 프레임워크.
- Aminabadi et al., "DeepSpeed-Inference: Enabling Efficient Inference of Transformer Models at Unprecedented Scale," SC 2022 — 두 번째 주요 baseline(ZeRO-Inference).
- Pan et al., "InstAttention: In-Storage Attention Offloading for Cost-Effective Long-Context LLM Inference," HPCA 2025 — 동일 문제의 lossy sparse-retrieval 대안, HILOS와 정확도·구현 플랫폼 면에서 직접 대비됨.
- Dao, "FlashAttention-2," ICLR 2024 / Dao et al. FlashAttention NeurIPS 2022 — prefill 단계 및 정확도 비교 기준이 되는 attention 알고리즘.
- Kwon et al., "Efficient Memory Management for Large Language Model Serving with PagedAttention (vLLM)," SOSP 2023 — 멀티노드 비교 실험(§6.6)의 분산 GPU 대안 시스템.

## Personal annotations
<!-- 본인 메모 영역 -->
