---
title: "Lincoln: Real-Time 50~100B LLM Inference on Consumer Devices with LPDDR-Interfaced, Compute-Enabled Flash Memory"
aliases: [Lincoln]
description: "소비자 기기에서 50~100B LLM 실시간 추론을 위해 LPDDR 인터페이스+near-Flash 연산을 결합한 device-architecture co-design"
venue: HPCA
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/isc
  - topic/in-flash
  - topic/llm
  - topic/on-device
  - venue/hpca
  - year/2025
---

# Lincoln: Real-Time 50~100B LLM Inference on Consumer Devices with LPDDR-Interfaced, Compute-Enabled Flash Memory

> **HPCA 2025** · `cluster/isc` · Source: [Lincoln - Real-Time 50~100B LLM Inference on Consumer Devices with LPDDR-Interfaced, Compute-Enabled Flash Memory.pdf](Lincoln%20-%20Real-Time%2050~100B%20LLM%20Inference%20on%20Consumer%20Devices%20with%20LPDDR-Interfaced,%20Compute-Enabled%20Flash%20Memory.pdf)

**저자**: Weiyi Sun, Mingyu Gao, Zhaoshi Li, Aoyang Zhang, Iris Ying Chou, Jianfeng Zhu, Shaojun Wei, Leibo Liu (Tsinghua University / BNRist / Institute for Interdisciplinary Information Sciences / Shanghai Qi Zhi Institute / MetaX Technology). 교신저자: Leibo Liu, Jianfeng Zhu.

## TL;DR
소비자 기기(랩탑/PC, DRAM ≤64GB)에서 50~100B급 대형 LLM을 돌리려면 weight가 Flash에 상주하며 매 iteration마다 Flash→DRAM 로딩이 실행 시간을 지배한다. Lincoln은 (1) device level에서 array-shrinking + hybrid-bonding 3D stacking으로 Flash 내부 대역폭을 die당 ~34GB/s로 끌어올리고, (2) architecture level에서 compute-intensive **prefill**은 LPDDR 인터페이스(>100GB/s)로 weight를 NPU에 올려 처리하고, memory-intensive **generation**은 hybrid-bonding 기반 near-Flash 연산 + speculative decoding으로 내부 대역폭을 완전히 활용한다. 결과적으로 conventional SSD 기반 대비 prefill 최대 13.23배, generation 최대 254.1배 가속하며 실시간 latency 목표를 충족한다.

## 문제 & 동기
소비자 기기는 form factor 제약으로 DRAM이 보통 ≤64GB인데, >50B LLM은 weight가 >100GB, >200B LLM은 >200GB라 weight 대부분이 Flash에 상주할 수밖에 없다. 매 추론 iteration마다 전체 weight를 거치므로 (prefill·generation 모두) weight를 Flash에서 반복 로딩해야 하고, mainstream Flash의 빈약한 I/O 대역폭(수 GB/s)이 전체 실행을 지배한다. 두 가지 근본 병목이 존재한다: (1) NPU SoC와 Flash 간 **transmission 대역폭**이 낮다 (off-chip I/O 핀이 대부분 LPDDR DRAM에 할당, NAND Flash는 ~4-8 lane만, 외부 I/O ~10GB/s). (2) density 위주 설계로 Flash array 자체의 **내부 대역폭**이 낮다 (read latency >50us, 32 die+4 plane이어도 41GB/s에 그침).

> [!quote]- 📄 원문 표현 (paper)
> "Large-sized LLMs have to load most of their weights from Flash storage for every execution iteration, which dominates the execution time of both the prefill and the generation phase. This performance bottleneck is attributed to both the low internal Flash memory bandwidth and the low transmission bandwidth between Flash and the Neural Processing Unit (NPU)." (Abstract, p.1)
>
> "With over 10× lower bandwidth than DRAM's, such data loading completely dominates the model execution time, not only for the memory-intensive LLM generation phase, but even for the traditionally compute-intensive prefill phase." (§I, p.1)
>
> "For generation phase, the latency per output token is up to 41.8s, with 94.0% of the time contributed by the Flash-to-DRAM weight loading procedure on average. The same stands for the prefill phase, with 91.4% of its latency occupied by weight loading." (§III-A, p.4)

## 핵심 통찰

> [!note]- 토글: prefill과 generation은 병목이 다르므로 따로 처리해야 한다
> LLM 실행은 두 단계로 나뉜다. **Prefill**은 입력 토큰 전체에 대한 GEMM 중심 → compute-intensive. **Generation**은 토큰 1개씩 처리하는 GEMV 중심 → memory-intensive. 두 단계의 연산 강도가 근본적으로 달라 단일 해법으로는 부족하다. Lincoln은 prefill은 weight를 LPDDR로 빼내 compute-rich NPU에서, generation은 weight를 옮기지 않고 near-Flash logic에서 처리한다 (§III-B, §IV-A, p.4-5).

> [!note]- 토글: 핀(transmission)은 LPDDR로, 대역폭(internal)은 hybrid bonding으로 푼다
> 소비자 SoC의 고속 off-chip I/O는 대부분 LPDDR에 할당돼 있고 LPDDR 패키지는 저비용 wire bonding 수직 적층(>100GB/s)을 지원한다. 따라서 Flash die를 별도 채널 추가 없이 기존 LPDDR 채널 안의 extra rank로 vertical stacking → transmission 병목 해소이자 form factor·cost 절감. 한편 hybrid bonding 3D stacking은 이미 소비자 Flash 제품(YMTC 등)에 상용화돼 있어 peripheral 면적 오버헤드를 logic layer로 내려 array shrinking을 가능케 하고 near-Flash logic 자리도 마련한다 (§III-B, §IV-A, p.4-5).

> [!note]- 토글: near-Flash 연산은 generation에만 쓴다 (compute-light이므로)
> near-Flash logic은 매우 light-weight(plane당 2-element FP-Dot @400MHz)이라 compute-intensive한 prefill에는 부적합하다. memory-intensive generation의 GEMV에만 적합하다. 즉 "weight를 옮기지 말고 연산을 weight 옆으로 가져간다"는 PIM 철학을 generation에 한정 적용 (§III-B, p.2, p.5).

> [!note]- 토글: speculative decoding이 generation 실시간성의 마지막 퍼즐
> 내부 대역폭을 완전히 활용해도 >100B 모델의 1 iteration이 0.4s에 달해 generation latency 제약(0.2s)을 못 맞춘다. ~1B draft model로 ~7 토큰을 미리 예측하고 target model로 append-style prefill처럼 batch 검증 → iteration당 multi-token 생성, target weight를 1회만 접근. draft가 target보다 ~100배 작아 per-token latency·메모리 트래픽·에너지를 정확도 손실 없이 절감 (§IV-D, p.7).

## 설계 / 메커니즘

> [!abstract]- 토글: LPDDR-based packaging (transmission 해소)
> 표준 Flash 패키지(NVMe/UFS)를 버리고 기존 LPDDR memory 인터페이스에 의존. LPDDR 패키지 안에 4개의 Lincoln Flash die를 4개 DRAM die와 함께 수직 적층, package 핀을 통해 bonding wire로 연결. SoC MC는 4 channel에 연결되고 각 channel은 1 Flash rank + 1 normal DRAM rank를 가진다 (rank = 2 die, 각 16-bit channel의 8 DQ 핀에 연결). Flash die에 별도 채널을 주지 않고 기존 LPDDR 채널을 공유하는 extra rank로 둬서 채널 대역폭 낭비를 막고 DRAM 성능 유지. DRAM과 Flash rank는 flat memory space로 SW가 직접 접근 (DRAM은 Flash의 단순 cache가 아님) (§IV-A, p.5).

> [!abstract]- 토글: Lincoln Flash die (internal 대역폭 + near-Flash)
> XL-Flash(latency 4us, 16 plane/die, 4KB page) 기반. plane을 bitline 방향으로 거의 절반 축소하고 plane 수를 2배(최대 32 plane)로 늘려 die당 **internal read bandwidth ~34GB/s** 달성. Density 손실은 hybrid bonding으로 해소: Flash array layer와 logic layer를 수직 적층, page buffer·PHY 등 peripheral을 logic layer로 내려 **area efficiency 74.6%** (원 XL-Flash ~55% 대비 개선). Power supply 문제(BGA의 9 power pin)는 power domain·power/ground pin·decoupling capacitance를 복제해 모든 plane 병렬 동작을 가능케 함. tR은 시뮬레이션상 3.426us, XL-Flash 상한(4us) 고려해도 실성능은 최대 17% 이내 (§IV-B, p.5-6, §V-A, p.8).
>
> Logic layer 구성: (1) **near-PHY SRAM global buffer**(총 260KB) — double buffering으로 Flash read latency를 LPDDR timing violation 없이 은닉. (2) **FP-Dot near-Flash 연산 유닛** — Logic Block(plane당)에 dot product logic 배치, GEMV용. (3) **near-Flash ECC** — BCH(9088, 8192, 64) chunk-wise, controller-side ECC를 대체.

> [!abstract]- 토글: LPDDR interface 동작 (Double-Buffering Prefetch + ECC)
> NPU MC가 prefetch command(+partition address)를 Flash-side command buffer에 write → Flash plane read 트리거 → readout이 logic layer global buffer로 이동, partition address도 기록. NPU는 status register를 polling해 read-out 완료 확인 후 normal LPDDR access로 buffer 읽기. SRAM의 고속·near-PHY 위치가 LPDDR timing을 보장. **Double Buffering**: 현재 partition 연산 중 다음 partition prefetch를 발행해 Flash-side prefetch를 NPU 연산과 overlap (다음 partition은 FC 연산 특성상 예측 가능). NPU 연산 시간이 internal bandwidth 대비 >4배 큰 LPDDR 접근 시간보다 최소 4배 크므로 latency 완전 은닉. **ECC**도 near-Flash로 옮겨 redundant LPDDR access를 제거하고 prefetch에 통합 (controller-side Flash ECC는 us 단위라 ~100ns LPDDR과 mismatch했었음) (§IV-C, p.6-7).

> [!abstract]- 토글: 데이터 레이아웃 & 워크플로
> 행렬을 모든 Flash die의 aggregate global buffer에 맞는 large partition으로 슬라이스. partition의 row를 die들에 round-robin 분배(coarse-grained, near-Flash 친화적, LPDDR channel 병렬성 활용). die 내에서 row를 plane에 round-robin 분배, 각 plane의 page address는 동일(multi-plane operation 요건). **near-Flash 연산 흐름**: NPU가 input vector를 각 die global buffer에 broadcast → Flash page를 읽고 ECC 교정 → controller가 plane들에 input vector 순차 broadcast → 각 plane local 연산 유닛이 dot product 수행, intermediate result는 local register file → 최종 결과를 global buffer 거쳐 NPU가 readout. NPU 연산용엔 row-major 기본이지만 column-major도 지원. Endurance: SLC는 100K P/E cycle 견디고, read reclaim(re-programming)으로 read disturb 완화, warranty 5년·read disturb threshold 5K cycle 가정 시 P/E 소비는 310K에 그쳐 endurance threshold(4M) 훨씬 아래 (§IV-E·F, p.7-8).
>
> **Control system** (Fig.7): Management Layer(ML, SoC측 SW+HW)가 transaction-level command(64-bit, 16-bit OP/16-bit BUFF_ADDR/32-bit FLASH_ADDR) 발행 → Operation Layer(OL, Flash-side controller FC)가 micro-op으로 분해 수행. Flash Access Acceleration용 LAU(Lincoln Access Unit)를 NPU에, Flash Read Unit을 LPDDR controller에 추가 (<0.1% runtime overhead).

## 평가

> [!success]- 토글: 성능·에너지·면적·발열 수치 (p.8-10)
> **구성** (Table I, p.8): NPU 32 TOPS(BF16) 4.60W + 4MB/2MB SRAM 0.431W. Memory: LPDDR5X-8533 16Gb x8, 2/4 packages × 4 channels × 2 DRAM die (+2 Lincoln Flash die). Lincoln Flash: SLC 96 wordline-layer, 32 plane, plane당 4.1484Gb, die당 132.75GB, tR=3.426us; logic layer total 71.0mm², near-Flash logic power 409.1mW. Baseline storage: TLC 2TB, tR=56us, NVMe PCIe 4.0 4 lane 8GB/s, 1.2GBps/channel.
> - **모델**: OPT-66B, LLaMA-65B, LAMDA-137B. Draft model: OPT-125M, NoFT-796M, LAMDA-2B. ShareGPT dataset, 90% tail latency 목표.
> - **Prefill latency**: Lincoln-2/Lincoln-4가 Base-2/BaseS-2 대비 **11.44배/11.46배**, Base-4/BaseS-4 대비 **13.23배/13.24배** 가속 (Fig.8a). SSD weight loading을 완전히 제거해 latency가 Base의 model execution time과 거의 동일.
> - **Prefill energy**: Base-2/Base-4 대비 평균 **34.5%/35.6%**, **30.1%/31.2%** 감소 (Fig.8b).
> - **Generation latency**: Base-2/Base-4 대비 **158.4배/254.1배**, BaseS-2/BaseS-4 대비 **47.8배/76.6배** 가속 (Fig.8c).
> - **Generation energy**: Base-2/Base-4 대비 **9.11배/3.66배**, BaseS-2/BaseS-4 대비 **8.46배/3.10배** 감소 (Fig.8d).
> - **절대 latency** (Table II, p.9): Lincoln-2/Lincoln-4 prefill — OPT-66B 1.633s/1.315s, LLaMA-65B 1.700s/1.269s, LAMDA-137B 3.772s/2.667s; generation per token — OPT-66B 0.085s/0.046s, LLaMA-65B 0.163s/0.087s, LAMDA-137B 0.264s/0.142s. 산업 기준 prefill 4.0~5.0s / generation 0.2s 목표를 거의/완전 충족.
> - **Ablation** (Fig.9): prefill에서 double buffering 단독은 10.98%/7.36% 개선(ECC가 LPDDR로 병목), near-Flash ECC 단독은 ECC를 줄이나 남은 오버헤드 23.2%/15.6%, 둘 다 적용 시 오버헤드 거의 제거. generation에서 speculative decoding 단독 2.61배/3.226배, near-Flash 단독 3.60배/4.05배, 둘 다 7.99배/10.63배.
> - **발열** (HotSpot, §V-D, p.10): Flash array energy 1×/1.5×/2×/2.5×/3× pressure test에서 최고 온도 상승 3.0K/3.9K/4.8K/5.7K/6.6K — mild~well acceptable.
> - **Cost** (§V-E, p.10): SLC + hybrid bonding + peripheral die 기준 $0.52/GB, conservative yield 적용 시 $0.72~$1.44/GB. 2-package 구성 $191/$382 (5년 warranty). conventional Flash design은 동일 internal bandwidth 위해 die 200~500개 필요($1500~$5000)라 cost-competitive.

## 섹션 노트
- **§I Introduction**: clouds privacy/queuing 문제 → vendor들이 on-device LLM 추진하나 양자화로도 <13B만 지원. 50~100B 실시간 추론이 목표. Flash storage 저성능의 근본 원인 2가지(transmission 핀, internal density 설계).
- **§II Background**: LLM 구조(decoder block의 FC = GEMM/GEMV, attention), prefill/generation 단계. 소비자 기기 메모리/스토리지 시스템(LPDDR >100GB/s vs Flash ~10GB/s external). Hybrid bonding for Flash(YMTC 상용화, TSV 없이 peripheral을 별도 die로 3D-stack).
- **§III Motivation**: Flash 병목 분석(Fig.3, generation weight loading 94.0%, prefill 91.4%; unlimited I/O여도 내부 latency로 부족). 기회: device level array shrinking, architecture level LPDDR(prefill)+near-Flash(generation).
- **§IV Lincoln System Design**: Overview, Flash layer design(density/power), LPDDR interface(double buffering + ECC), near-Flash logic(FP-Dot, speculative decoding 지원), data layout/workflow, endurance(read reclaim), control system(ML/OL, LAU).
- **§V Evaluation**: methodology(NVSim 기반 3DFPIM, ONNXim+Ramulator2, MQSim, HotSpot), 성능·에너지 비교, ablation, 발열, cost.
- **§VI Related Works**: 압축/distillation, LLM-in-a-Flash(dynamic sparsity), 기존 near-memory/storage(Smart-Infinity는 weight update만, CXL-PNM은 고용량 DRAM 필요). Lincoln은 NAND Flash로 LLM 가속을 처음 탐구.
- **§VII Conclusions**: device-architecture co-design으로 Flash 병목 해소, 50~100B 실시간 추론 가능.

## 핵심 용어
- **prefill phase**: 입력 토큰 전체를 1 iteration으로 처리하는 단계, GEMM 중심 compute-intensive.
- **generation phase**: 토큰을 하나씩 생성하는 단계, GEMV 중심 memory-intensive. KV Cache 누적.
- **near-Flash computing**: Flash array 바로 아래 logic layer에 연산 유닛(FP-Dot)을 배치해 weight 옆에서 GEMV를 수행, weight 이동을 제거.
- **hybrid bonding**: wafer-to-wafer로 두 die의 metal layer를 직결하는 3D stacking. TSV 불필요로 저비용, peripheral을 별도 CMOS die로 제작해 면적·성능 개선.
- **array shrinking**: Flash array(plane)를 작게 만들어 read latency를 낮추고 plane 병렬성을 높여 internal bandwidth를 키우는 기법. XL-Flash가 대표.
- **LPDDR-interfaced Flash**: 표준 NVMe/UFS 대신 LPDDR memory 인터페이스로 Flash die를 LPDDR 채널의 extra rank로 통합, 고대역폭 transmission 확보.
- **Double-Buffering Prefetch**: 현재 partition 연산 중 다음 partition을 prefetch해 Flash read latency를 NPU 연산과 overlap.
- **speculative decoding**: 작은 draft model(~1B)이 여러 토큰을 미리 예측, target model이 batch로 한번에 검증해 iteration당 multi-token 생성(정확도 무손실).
- **read reclaim**: read disturb 누적 시 P/E로 재기록해 retention/disturb error를 완화하는 data refresh 유사 기법.
- **FP-Dot**: Logic Block 내 plane당 배치된 floating-point dot product 연산 유닛(2-element @400MHz).

## 강점 · 한계 · 열린 질문
**강점**
- prefill/generation의 상이한 병목을 LPDDR vs near-Flash로 분리 대응하는 깔끔한 co-design.
- hybrid bonding·array shrinking·LPDDR packaging 모두 이미 상용화된 기술 위에 구축 → 실현 가능성 높음.
- weight 1 copy만 두는 data layout으로 NPU·near-Flash 양쪽이 효율 접근, 용량 제약(265GB) 내 137B 모델까지 수용.
- endurance·발열·cost를 정량 분석해 5년 warranty·실시간 latency 충족 입증.

**한계**
- near-Flash logic이 compute-light라 prefill엔 무용 → LPDDR/NPU에 의존, prefill 가속은 weight loading 제거 효과가 대부분.
- speculative decoding 의존도가 높아 acceptance rate가 낮은 워크로드/모델에선 generation 이득이 줄 수 있음(LAMDA의 큰 draft model 오버헤드 사례).
- SLC 채택으로 density·cost가 TLC 대비 불리(설계가 hybrid bonding으로 일부 상쇄).
- 평가가 시뮬레이션 기반(NVSim/3DFPIM 보정, HotSpot)이며 실 silicon 검증은 없음.

**열린 질문**
- dynamic sparsity를 결합하면(저자도 future work 언급) near-Flash 활용도가 더 오를까?
- 4-bit 405B 같은 더 큰 모델로의 확장 시 LPDDR 채널·package 핀 예산은?
- read reclaim의 idle-time 분산이 실제 사용 패턴(bursty)에서도 0.4% 오버헤드를 유지할까?

## ❓ Q&A (자가 점검)

> [!question]- Q1. Lincoln이 해결하려는 두 가지 근본 병목은?
> 답: (1) NPU SoC와 Flash 간 transmission 대역폭이 낮다(off-chip 핀이 대부분 LPDDR에 할당, NAND은 4-8 lane, ~10GB/s). (2) density 위주 Flash array 설계로 internal read 대역폭이 낮다(latency >50us, 32 die에서도 41GB/s). prefill·generation 모두 매 iteration weight 로딩이 지배(94.0%/91.4%).

> [!question]- Q2. prefill과 generation을 다르게 처리하는 이유와 각각의 방식은?
> 답: prefill은 GEMM 중심 compute-intensive, generation은 GEMV 중심 memory-intensive로 연산 강도가 다르다. prefill은 LPDDR 인터페이스(>100GB/s)로 weight를 compute-rich NPU에 올려 처리. generation은 weight를 옮기지 않고 logic layer의 near-Flash FP-Dot으로 내부 대역폭을 활용하며 speculative decoding을 더한다.

> [!question]- Q3. Lincoln Flash die가 die당 ~34GB/s 내부 대역폭을 어떻게 얻고 density 손실은 어떻게 메우나?
> 답: XL-Flash 기반으로 plane을 bitline 방향 절반 축소 + plane 수 2배(최대 32 plane)로 read latency를 낮추고 plane 병렬성을 높여 34GB/s 달성. density 손실은 hybrid bonding 3D stacking으로 peripheral(page buffer·PHY)을 logic layer로 내려 area efficiency 74.6%(XL-Flash ~55%)로 회복.

> [!question]- Q4. LPDDR timing(~100ns)과 Flash read latency(us)의 mismatch를 어떻게 은닉하나?
> 답: logic layer의 near-PHY SRAM global buffer(260KB)에 readout을 staging하고, Double-Buffering Prefetch로 현재 partition 연산 중 다음 partition을 미리 prefetch해 Flash latency를 NPU 연산과 overlap. NPU 연산 시간이 LPDDR 접근보다 ≥4배 커서 완전 은닉. 추가로 ECC를 near-Flash로 옮겨 redundant LPDDR access를 제거하고 prefetch에 통합.

> [!question]- Q5. speculative decoding이 왜 필요하고 어떻게 동작하나?
> 답: 내부 대역폭을 다 써도 >100B 모델 1 iteration이 0.4s로 generation 제약(0.2s)을 못 맞춘다. ~1B draft model이 ~7 토큰을 순차 예측 → target model이 append-style prefill처럼 batch로 한번에 검증, target weight를 1회만 접근. draft가 ~100배 작아 per-token latency·트래픽·에너지를 무손실로 절감하며 iteration당 >1 토큰 생성.

> [!question]- Q6. SSD 기반 대비 prefill·generation 가속 수치는?
> 답: prefill은 Base-2/BaseS-2 대비 11.44/11.46배, Base-4/BaseS-4 대비 13.23/13.24배. generation은 Base-2/Base-4 대비 158.4/254.1배, BaseS(speculative baseline)-2/-4 대비 47.8/76.6배.

> [!question]- Q7. near-Flash 연산의 빈번한 고속 read로 인한 read disturb는 어떻게 관리하나?
> 답: read reclaim(re-programming, data refresh 유사)으로 disturb error 완화. SLC는 100K P/E cycle 견디고, batched reclaim을 every iteration(또는 idle time)에 1/5K씩 분산해 추론 오버헤드 ~0.4%. warranty 5년·read disturb threshold 5K 가정 시 P/E 소비 310K로 endurance threshold(4M) 훨씬 아래.

> [!question]- Q8. 왜 별도 Flash 채널을 만들지 않고 LPDDR 채널 안의 extra rank로 두나?
> 답: 별도 채널을 할당하면 채널 대역폭이 LLM 비추론 시 낭비되고 DRAM 시스템 성능을 해친다. 기존 LPDDR 채널을 공유하는 rank로 두면 DRAM 핀을 재사용해 form factor·cost를 줄이고, DRAM과 Flash rank가 flat memory space를 이뤄 SW가 직접 접근(Flash를 단순 cache로 쓰지 않음)할 수 있다.

## 🔗 Connections
[[In-Storage Computing]] · [[HPCA]] · [[2025]]

## References worth following
- K. Alizadeh et al., "LLM in a Flash: Efficient large language model inference with limited memory," ACL 2024 — Flash에 LLM을 두고 dynamic sparsity로 추론하는 알고리즘 접근(본 논문과 대비).
- H. Jang et al., "Smart-Infinity: Fast large language model training using near-storage processing on a real system," HPCA 2024 — near-storage LLM, 단 weight update에 한정(Lincoln과 범위 차이).
- W. Cho et al., "22.2 An 8.5gb/s/pin 12gb-lpddr5 sdram with a hybrid-bank architecture ... in a 2nd generation 10nm dram process," ISSCC 2020 — LPDDR-CXL-PNM near-near-bank 계열 비교 대상.
- "About xtacking - YMTC" / YMTC hybrid bonding 상용화 — device level 실현 가능성의 핵심 근거.
- Apple, "OPTQ/quantization, 14-inch MacBook spec" 및 §II XL-Flash 참고문헌 [50] — array-shrunk low-latency Flash 설계의 기반.
- Y. Cai et al., "Read disturb errors in MLC NAND flash memory: Characterization, mitigation, and recovery," DSN 2015 — read reclaim/endurance 분석의 토대.

## Personal annotations
<본인 메모 영역>
