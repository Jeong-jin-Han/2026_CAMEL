---
title: "Conduit: Programmer-Transparent Near-Data Processing Using Multiple Compute-Capable Resources in SSDs"
aliases: [Conduit]
description: "SSD 내부의 3개 이종 연산 자원(컨트롤러 코어·DRAM·플래시)에 instruction 단위로 자동·투명하게 연산을 오프로딩하는 NDP 프레임워크"
venue: HPCA
year: 2026
arxiv: "2601.17633"
tier: deep
status: done
tags:
  - paper
  - cluster/isc
  - topic/near-data
  - topic/in-storage-computing
  - topic/programmability
  - venue/hpca
  - year/2026
---

# Conduit: Programmer-Transparent Near-Data Processing Using Multiple Compute-Capable Resources in SSDs

> **HPCA 2026** · cluster/isc · Source: [Conduit - Programmer-Transparent Near-Data Processing Using Multiple Compute-Capable Resources in SSDs.pdf](<Conduit - Programmer-Transparent Near-Data Processing Using Multiple Compute-Capable Resources in SSDs.pdf>)

저자: Rakesh Nadig, Vamanan Arulchelvan, Mayank Kabra, Harshita Gupta, Rahul Bera, Nika Mansouri Ghiasi, Nanditha Rao, Qingcai Jiang, Andreas Kosmas Kakolyris, Yu Liang, Mohammad Sadrosadati, Onur Mutlu (ETH Zürich, Inria Paris)

## TL;DR

현대 SSD는 (1) 컨트롤러의 general-purpose embedded core, (2) SSD 내부 DRAM, (3) NAND flash chip 이라는 세 가지 이종 연산 자원을 가지며, 각각 in-storage processing (ISP), processing-using-DRAM (PuD-SSD), in-flash processing (IFP) 라는 NDP paradigm을 가능하게 한다. 기존 NDP 기법은 이 자원 중 하나둘만 격리되어(in isolation) 사용하고, 프로그래머가 직접 오프로딩 영역을 식별·매핑해야 하며(non-transparent), 자원 이질성을 무시한 채 bandwidth나 data movement 같은 제한된 지표로만 오프로딩을 결정한다. **Conduit**은 이를 해결하는 최초의 general-purpose, programmer-transparent NDP framework로, (1) 컴파일 타임에 LLVM pass로 loop를 SIMD로 vectorize하고 metadata를 IR에 임베드하며, (2) 런타임에 SSD 내부에서 6개 feature 기반 holistic cost function으로 instruction 단위(instruction-granularity)로 가장 적합한 자원을 선택해 native ISA로 변환·디스패치한다. 6개 data-intensive workload에서 최선의 기존 오프로딩 정책(DM-Offloading) 대비 **1.8배** 성능 향상, 에너지 **46%** 절감을 추가 하드웨어 없이 달성한다.

## 문제 & 동기 (Problem & Motivation)

NDP는 data movement bottleneck을 완화하지만, 기존 SSD-based NDP 기법에는 두 가지 핵심 한계가 있다. 첫째, 대부분 격리되어(in isolation) 동작해 한두 개 자원만 매핑하므로 SSD의 전체 연산 잠재력을 활용하지 못한다(예: Active Flash는 ISP에만, Flash-Cosmos는 flash chip만 사용). 둘째, application-specific하고 programmer-transparent하지 않아 프로그래머가 직접 오프로딩 가능 영역을 식별하고 데이터 배치/코드 스케줄링을 관리해야 한다(예: MARS).

또한 host와 near-memory unit 간 파티셔닝을 다루는 기존 오프로딩 모델(BW-Offloading, DM-Offloading)을 SSD에 적용해도 이득이 제한적이다. 이들은 SSD 연산 자원의 이질성(parallelism, access granularity, computation capability)을 고려하지 않고, bandwidth utilization이나 data movement cost 같은 제한된 system-level metric만 최적화하며, computation resource utilization, operand location, data dependency 같은 핵심 요소를 무시한다. 동기 실험(§3.2)에서 최선의 기존 모델인 DM-Offloading조차 Ideal 대비 평균 2.5배(LLM에서 최대 4.6배) 성능 격차를 보인다.

> [!quote]- 📄 원문 표현 (paper)
> "A large body of prior SSD-based NDP techniques operate in isolation, mapping computations to only one or two NDP paradigms ... These techniques (1) are tailored to specific workloads or kernels, (2) do not offload computations across all three NDP paradigms in the SSD and thus fail to exploit the full computational potential of an SSD, and (3) lack programmer-transparency, often requiring significant manual effort to identify offloadable code regions and map them to the SSD computation resources" (p.1)
>
> "adapting these techniques to SSD-based NDP provides limited benefits because they (1) do not account for the architectural heterogeneity of the SSD computation resources, which vary in their parallelism, access granularities, and computation capabilities, and (2) optimize for only a limited set of system-level metrics such as bandwidth utilization (e.g., [28]) or data movement cost (e.g., [29]), while ignoring key factors such as computation resource utilization, operand location, and data dependencies" (p.2)
>
> "the best-performing prior model, DM-Offloading ... shows an average performance gap of 2.5× compared to an Ideal offloading approach that assumes no resource contention and always selects the resource with the lowest computation latency" (p.2)

## 핵심 통찰 (Key Insight)

단일 연산 자원에 의존하면 어떤 workload에서도 일관되게 좋지 않다. Case study(§3.1)에서 IFP는 I/O-intensive workload에 효과적(OSP 대비 70% 향상)이지만, ISP는 limited SIMD parallelism, IFP는 limited operation set에 제약된다. Performance bottleneck은 workload마다, 그리고 같은 workload의 실행 phase마다도 이동한다. 따라서 효과적인 SSD NDP는 computation capability, data locality, resource contention을 함께 고려해 fine-grained하게, workload·system-aware로 스케줄링해야 한다.

ISP/IFP를 단순히(naively) 결합하면 추가적인 inter-resource data movement 때문에 오히려 성능이 나빠질 수 있고(I/O-intensive에서 IFP 단독 대비 15% 저하), judiciously 결합해야만 이득이 난다는 것이 핵심 관찰이다.

> [!quote]- 📄 원문 표현 (paper)
> "no single execution model consistently performs well across different workloads ... performance bottlenecks shift across execution models ... These bottlenecks are workload-dependent and can vary across different execution phases of the same workload" (p.5)
>
> "Naively combining IFP and ISP reduces performance by 15% compared to IFP, because the additional data movement overhead between flash chips and the controller offsets the benefits of IFP's highly parallel computation capability" (p.4)
>
> "combining multiple SSD computation resources provides benefits, but only when done judiciously" (p.5)

## 설계 / 메커니즘 (Design)

Conduit은 두 단계로 동작한다(Figure 6, p.6).

**1) Compile-Time Preprocessing (Host, ⓐ).** 한 번만(one-time) 오프라인으로 수행. (a) **Loop auto-vectorization**: Clang으로 LLVM auto-vectorization을 돌려 오프로딩 가능 loop를 식별하고 SIMD로 변환. 커스텀 플래그 `-O3 -g -mllvm -force-vector-width=4096 -force-vector-interleave=1 -Rpass-analysis=loop-vectorize -Rpass=loop-vectorize` 사용. vector width 4096(32-bit operand 기준 16KB)은 NAND flash page에 정렬되어 data movement를 단순화하고 misaligned access를 피하며 FTL의 L2P mapping granularity와 맞춘다. fully vectorize 불가능한 loop는 partial vectorization(strip-mining)으로 처리. (b) **Vectorized code compilation**: 커스텀 LLVM pass가 instruction type, operand pointer, element size, vector length 같은 lightweight metadata를 optimized IR에 임베드. (c) **Binary transfer**: ARM ISA 바이너리로 컴파일 후 NVMe admin command(`fw-download`, `fw-commit`)를 재활용해 SSD로 전송.

**2) Runtime Dynamic Offloading & Instruction Transformation (SSD, ⓑ).** SSD 컨트롤러 내부에서 FTL와 함께 동작하는 **SSD Offloader**가 (1) holistic cost function 평가, (2) instruction transformation, (3) 자원 execution queue로 디스패치를 수행. SSD 내부에서 하는 이유는 모든 자원의 실시간 상태(queue occupancy, flash channel utilization, data location)를 알기 때문이고, programmer-transparency를 유지하기 위함이다.

**Cost Function (Table 1, p.8).** 6개 feature를 사용: (1) Operation Type — ISP는 ~300 ISA instruction, PuD-SSD는 16개(arithmetic/predication/relational), IFP는 9개(bitwise 6 + arithmetic 3) 지원, (2) Operand Location — L2P table lookup으로 추적, (3) Data Dependence Delay (delay_dd), (4) Resource Queueing Delay (delay_queue), (5) Data Movement Latency (latency_dm) — 미리 계산해 SSD DRAM에 저장, (6) Expected Computation Latency (delay_comp). 자원 i의 총 latency는
`total_latency_resource_i = latency_comp + latency_dm + max(delay_dd, delay_queue)` (Eqn.1, p.8)
이며 `offloading_target = argmin(total_latency_ISP, total_latency_PuD_SSD, total_latency_IFP)` (Eqn.2)로 선택.

**Instruction Transformation Unit.** vectorized instruction을 target 자원의 native ISA로 변환. ISP는 ARM M-Profile Vector Extension (MVE), PuD-SSD는 SIMDRAM/MIMDRAM/Proteus의 ISA 확장(예: `bbop_op`), IFP는 Flash-Cosmos의 multi-wireline sensing(MWS)과 Ares-Flash의 `shift_and_add` primitive. vector width가 자원과 불일치하면(예: SSD DRAM 8KiB, controller core 32B) 더 작은 sub-operation으로 분할.

**System Integration (§4.4).** logical-page granularity로 데이터 배치, L2P mapping table로 위치 추적. 두 동작 모드: regular I/O mode와 computation mode(host I/O 정지, 모든 자원을 NDP에 할당). lazy coherence: 다른 자원이 데이터를 요청하거나 host로 전송할 때 등에만 동기화하고, L2P table에 owner/state/version 3개 field를 두어 page coherence를 관리. failure는 기존 FTL 메커니즘(ECC, bad-block remapping, GC, wear-leveling) 활용.

**Overhead Analysis (§4.5).** 추가 하드웨어 없음. SSD DRAM에 metadata table(operation type 2B, operand location 4bit, data dependence delay 2B, queueing/movement/computation latency 각 4B)과 300+ operation type을 native instruction으로 매핑하는 translation table(1.5 KiB) 저장. PuD-SSD는 MIMDRAM의 1.11% DRAM array area overhead, IFP는 Ares-Flash의 1.5% area overhead를 상속. runtime latency overhead는 평균 3.77μs(최대 33μs).

> [!quote]- 📄 원문 표현 (paper)
> "Conduit consists of two key steps. First, Conduit performs compile-time vectorization to identify offloadable code regions ... and transforms them into SIMD operations that match the internal bit-level parallelism of an SSD. Second, at runtime, within the SSD, Conduit (i) determines the most suitable SSD computation resource to execute each vector operation using a holistic cost function, which is based on six key factors ... (ii) translates each vector operation to the native ISA of the chosen resource, and (iii) dispatches the instruction to the chosen resource's execution queue" (p.2)
>
> "The -force-vector-width=4096 flag configures the vector width to 4096 for 32-bit operands (16KB in total), which aligns each vector to a typical NAND flash page" (p.7)
>
> "Conduit requires no hardware modifications. It builds on the hardware capabilities proposed by prior NDP techniques." (p.10)

## 평가 (Evaluation)

**환경 (Table 2, p.10).** in-house event-driven simulator, MQSim 기반(real SSD에 대해 validated)에 ISP/SSD DRAM/NAND flash chip 연산 모델 추가, internal DRAM은 Ramulator 2.0 기반. SSD: 48-WL-layer 3D TLC NAND 2TB, PCIe 4.0 8GB/s, flash channel 1.2GB/s, SSD DRAM 2GB LPDDR4-1866, controller 5x ARM Cortex-R8 @1.5GHz. Host: Intel Xeon Gold 5118(6코어), NVIDIA A100 GPU(real system). Workload는 PuD-SSD를 제외한 case study와 ISP/IFP/COTS DRAM 연산은 real-device characterization(Flash-Cosmos 등)으로 calibrate.

**Baseline.** ISP, PuD-SSD(MIMDRAM), Flash-Cosmos, Ares-Flash, BW-Offloading, DM-Offloading, Ideal(no contention, zero data movement latency, 최소 computation latency 선택 — 실현 불가능한 upper bound), 그리고 host CPU/GPU(real system).

**Workload (Table 3, p.11).** 6개: AES(vectorizable 65%, reuse 15.2), XOR Filter(16%, 2.0), heat-3d(95%, 16), jacobi-1d(95%, 3), LlaMA2 Inference(70%, 1.8 — 7B INT8), LLM Training(60%, 5.2). 부동소수점은 INT8로 양자화(자원이 native FP 미지원). 각 workload memory footprint는 SSD DRAM 용량의 2배 초과.

**성능 (Figure 7a, p.12).** Conduit은 CPU 대비 평균 4.2배, GPU 대비 3.3배, ISP 대비 3.3배, PuD-SSD 대비 2.2배, Flash-Cosmos 대비 3.3배, Ares-Flash 대비 2.3배, BW-Offloading 대비 2.0배, DM-Offloading 대비 **1.8배** 향상. Ideal 성능의 **62%** 달성. compute-intensive workload(heat-3d, jacobi-1d, LlaMA2, LLM Training)에서 DM-Offloading 대비 평균 2.6배(최대 4.5배). memory-bound(AES, XOR Filter)에서는 1.2배.

**에너지 (Figure 7b, p.12).** CPU 대비 78.2%, GPU 58.2%, ISP 67.3%, PuD-SSD 60.6%, Flash-Cosmos 68.0%, Ares-Flash 47.8%, DM-Offloading **46.8%** 절감. Ideal 에너지 효율의 68% 달성.

**Tail latency (Figure 8, p.13).** LlaMA2 Inference에서 DM-Offloading 대비 99th percentile 5.6배, 99.99th 22.3배 감소(BW-Offloading 대비 각 1.8배/10.7배). jacobi-1d에서 DM-Offloading 대비 99th 1.1배, 99.99th 1.3배.

**Offloading 분석 (Figure 9/10, p.13).** memory-bound workload에서는 Conduit 포함 모든 정책이 ISP를 거의 안 씀(AES에 0.4%, XOR Filter에 0.6%). LlaMA2에서는 Conduit과 Ideal 모두 PuD-SSD와 ISP에 거의 균등 분배하고 IFP는 회피(multiplication이 잦은 operand 전송 필요). Conduit은 instruction type과 runtime 상태에 따라 동적으로 자원을 적응시켜 synchronization/data movement overhead를 줄인다.

> [!quote]- 📄 원문 표현 (paper)
> "Conduit outperforms all baselines, achieving average speedup of 4.2× over CPU, 3.3× over GPU, 3.3× over ISP, 2.2× over PuD-SSD, 3.3× over Flash-Cosmos, 2.3× over Ares-Flash, 2.0× over BW-Offloading and 1.8× over DM-Offloading" (p.12)
>
> "Conduit provides 62% of the Ideal policy's performance" (p.12)
>
> "Conduit has lower average energy consumption than all baselines, reducing energy consumption by 78.2% over CPU, 58.2% over GPU, 67.3% over ISP, 60.6% over PuD-SSD, 68.0% over Flash-Cosmos, 57.4% over Ares-Flash, and 46.8% over DM-Offloading" (p.12)

## 섹션 노트 (Section notes)

- **§1 Introduction**: NDP 동기, 세 paradigm(ISP/PuD-SSD/IFP) 소개, 기존 기법의 두 한계, 기여 3가지.
- **§2 Background**: SSD 구조(Figure 1: controller, DRAM, NAND, embedded core, flash controller, channel/queue). PuD-SSD 원리(Figure 2: ACT/PRE 명령으로 AND/OR/NOT/MAJORITY; RowClone, Ambit, SIMDRAM, MIMDRAM, COTS DRAM). IFP 원리(Figure 3: bitwise AND/OR via multi-wireline activation, S-latch/D-latch로 addition/multiplication).
- **§3 Motivation**: §3.1 case study(Figure 4 — OSP/ISP/IFP/IFP+ISP의 execution time 분해; I/O-intensive/compute-intensive/mixed). §3.2 prior offloading 모델(BW-/DM-Offloading) SSD 적용 시 효과(Figure 5 speedup; DM이 best지만 Ideal 대비 2.5배 격차).
- **§4 Conduit**: §4.1 overview, §4.2 design challenge 3가지(heterogeneity, shared bus contention, programmer burden), §4.3 detailed design(Figure 6), §4.4 system integration, §4.5 overhead.
- **§5 Methodology**: simulator(MQSim+Ramulator2.0), 5가지 NDP extension, performance/energy modeling, baseline, workload(Table 3).
- **§6 Evaluation**: performance, energy, tail latency, offloading decision, workload-resource interaction.
- **§7 Discussion**: extensibility(sort/search/accelerator 추가 가능), auto-vectorization 한계, irregular workload 적용성.
- **§8 Related Work**, **§9 Conclusion**.

## 핵심 용어 (Key terms)

- **NDP (Near-Data Processing)**: 데이터가 있는 곳 가까이에서 연산을 수행해 data movement bottleneck을 완화하는 기법.
- **ISP (In-Storage Processing)**: SSD 컨트롤러의 general-purpose embedded core(예: ARM Cortex-R5/R8)로 연산. ~300 ISA instruction 지원, limited SIMD parallelism.
- **PuD-SSD (Processing-using-DRAM in SSD)**: SSD 내부 DRAM에서 row activation 기반 in-place bulk bitwise/arithmetic 연산. SIMDRAM/MIMDRAM/Proteus 활용. 데이터를 flash channel로 옮겨야 하는 제약.
- **IFP (In-Flash Processing)**: NAND flash chip 내부에서 multi-wireline sensing(MWS)과 latch로 bitwise/arithmetic 연산. massive bit/array-level parallelism, 단 9개 연산만 지원.
- **Holistic cost function**: 6개 feature(operation type, operand location, data dependence delay, resource queueing delay, data movement latency, expected computation latency)로 instruction별 최적 자원을 선택하는 함수.
- **Instruction-granularity offloading**: loop 단위가 아닌 개별 vectorized instruction 단위로 자원을 선택해 오프로딩.
- **Programmer-transparent**: 프로그래머의 수동 코드 식별/매핑 없이 컴파일러 auto-vectorization으로 자동 처리.
- **BW-Offloading / DM-Offloading**: 각각 bandwidth utilization / data movement cost 최소화 기준으로 자원을 고르는 기존(host-near-memory) 오프로딩 모델.
- **L2P (Logical-to-Physical) mapping**: FTL이 logical page를 physical 위치로 매핑; operand location 추적과 coherence에 활용.
- **Lazy coherence**: 꼭 필요한 경우(타 자원 요청, host 전송, 공간 재사용, GC, power cycle)에만 동기화하는 약한 일관성 정책.

## 강점 · 한계 · 열린 질문

**강점**
- 최초의 general-purpose, programmer-transparent하면서 세 SSD 자원 모두를 instruction 단위로 활용하는 NDP framework.
- 추가 하드웨어 없이(no additional hardware cost) 기존 SSD/NVMe 인프라(NVMe admin command, FTL, L2P) 위에 구축.
- 6개 feature를 동시에 보는 holistic cost function으로 contention·dependency를 반영, tail latency까지 크게 개선.
- diverse workload(crypto, stencil, filter, LLM)에서 일관된 성능/에너지 이득.

**한계**
- LLVM auto-vectorization 의존: complex data dependency, memory aliasing, indirect access, complex control flow, atomic/sync 연산, unknown/small iteration count loop에서 실패 가능 → 프로그래머가 pragma/restructure 필요.
- 자원이 native floating-point를 지원하지 않아 workload를 INT8로 양자화해야 함(정확도 영향은 논문에서 평가하지 않음).
- 평가가 real silicon이 아닌 in-house event-driven simulator(MQSim+Ramulator) 기반(real device characterization으로 calibrate하지만).
- PuD-SSD를 case study(§3.1)에서 제외(ISP와 유사한 data-movement overhead 가정).
- cost function이 flash channel/DRAM bus의 real-time contention을 명시적으로 모델링하지 않고(latency_dm은 정적 추정), contention effect는 offloader가 간접 반영.

**열린 질문**
- 6개 feature 외 thermal/endurance/wear를 cost function에 넣으면? computation mode가 flash wear에 미치는 영향은?
- INT8 양자화가 LLM inference/training 정확도에 주는 영향과 실제 적용 가능성은?
- real SSD silicon 구현 시 3.77μs runtime overhead와 area overhead 추정이 유지되는가?

## ❓ Q&A (자가 점검)

> [!question]- Q. Conduit이 활용하는 SSD의 세 가지 이종 연산 자원과 각 NDP paradigm은?
> SSD 컨트롤러의 general-purpose embedded core → ISP, SSD 내부 DRAM → PuD-SSD, NAND flash chip → IFP. 세 자원은 parallelism, access granularity, computation capability가 서로 다르다(ISP ~300 ISA / PuD-SSD 16개 / IFP 9개 연산).

> [!question]- Q. 기존 SSD NDP 기법의 두 가지 한계는?
> (1) 한두 자원만 격리되어 사용해 SSD의 전체 연산 잠재력을 못 살리고 application-specific하며 programmer-transparent하지 않음, (2) 자원 이질성을 무시하고 bandwidth/data-movement 같은 제한된 metric만 최적화해 utilization, operand location, data dependency를 무시함.

> [!question]- Q. Conduit의 두 핵심 단계는?
> (1) Compile-time preprocessing: LLVM auto-vectorization으로 오프로딩 가능 loop를 식별·SIMD화하고 metadata를 IR에 임베드(한 번만). (2) Runtime dynamic offloading: SSD 내부에서 holistic cost function으로 instruction별 자원 선택, native ISA로 변환, execution queue로 디스패치.

> [!question]- Q. cost function이 사용하는 6개 feature와 자원 선택 식은?
> operation type, operand location, data dependence delay, resource queueing delay, data movement latency, expected computation latency. total_latency_i = latency_comp + latency_dm + max(delay_dd, delay_queue) 를 계산해 ISP/PuD-SSD/IFP 중 argmin을 선택.

> [!question]- Q. vector width를 4096(32-bit operand 기준 16KB)으로 설정한 이유는?
> 전형적인 NAND flash page(16KB)에 정렬되어 data movement를 단순화하고, misaligned access를 피하며, FTL의 L2P mapping granularity와 맞추기 위함. PuD-SSD/ISP는 더 작은 width를 지원하므로 offloader가 런타임에 sub-operation으로 분할한다.

> [!question]- Q. ISP/IFP를 단순 결합하면 왜 성능이 나빠질 수 있나?
> flash chip과 컨트롤러 사이의 추가 inter-resource data movement(bandwidth-limited flash channel) 때문에 IFP의 high parallelism 이득을 상쇄하기 때문. I/O-intensive에서 naive 결합은 IFP 단독 대비 15% 성능 저하.

> [!question]- Q. Conduit의 대표 성능/에너지 수치는?
> 최선의 기존 정책 DM-Offloading 대비 평균 1.8배 성능 향상, 에너지 46.8% 절감. CPU 대비 4.2배, Ideal 성능의 62% 달성. 추가 하드웨어 비용 없음.

> [!question]- Q. Conduit이 추가 하드웨어 없이 동작할 수 있는 이유는?
> 기존 NDP 기법(MIMDRAM, Flash-Cosmos, Ares-Flash)의 하드웨어 capability와 기존 SSD 인프라(NVMe admin command fw-download/fw-commit, FTL, L2P mapping)를 재활용하기 때문. SSD DRAM에 작은 metadata table(translation table 1.5 KiB 등)만 추가하며, PuD/IFP의 area overhead(1.11%/1.5%)는 기존 기법에서 상속.

## 🔗 Connections

[[In-Storage Computing]] · [[HPCA]] · [[2026]]

관련: [[SkyByte]], [[XHarvest]]

## References worth following

- Flash-Cosmos [10] — IFP로 flash chip 내부 bulk bitwise AND/OR 가속(MWS); Conduit의 IFP primitive 기반.
- MIMDRAM [26] / SIMDRAM [11] — PuD(processing-using-DRAM) framework; Conduit의 PuD-SSD ISA 확장 기반.
- Ares-Flash [201] — flash 내부 bitwise+arithmetic(shift_and_add) 확장; Conduit의 IFP arithmetic 기반.
- MQSim [217, 218] — Conduit simulator의 base SSD 모델.
- Ramulator 2.0 [219, 327-329] — internal DRAM 모델링 base.
- MARS [92] — SSD DRAM+controller로 genome analysis; programmer intervention이 필요한 multi-resource application-specific 사례.

## Personal annotations

<본인 메모 영역>
