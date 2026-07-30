---
title: "Flagger: Cooperative Acceleration for Large-Scale Cross-Silo Federated Learning Aggregation"
description: "DPU와 CSD(computational storage drive)를 이종 near-data 가속기로 협력시켜 cross-silo 연합학습의 homomorphic-encryption 기반 aggregation을 가속하는 아키텍처"
venue: ISCA
year: 2024
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/isc
  - venue/isca
  - year/2024
  - list/26s-v2
  - topic/near-data-processing
  - topic/computational-storage
  - topic/homomorphic-encryption
  - topic/federated-learning
---

# Flagger: Cooperative Acceleration for Large-Scale Cross-Silo Federated Learning Aggregation

> **ISCA 2024** · cluster/isc · Source: [Flagger - Cooperative Acceleration for Large-Scale Cross-Silo Federated Learning Aggregation.pdf](<Flagger - Cooperative Acceleration for Large-Scale Cross-Silo Federated Learning Aggregation.pdf>)

저자: Xiurui Pan (Peking University), Yuda An (Peking University), Shengwen Liang (Institute of Computing Technology, Chinese Academy of Sciences), Bo Mao (Xiamen University), Mingzhe Zhang (Institute of Information Engineering, Chinese Academy of Sciences), Qiao Li (Xiamen University), Myoungsoo Jung (KAIST and Panmnesia), Jie Zhang (Peking University, corresponding author)

## TL;DR
Cross-silo federated learning(FL)은 client의 gradient를 Paillier 방식 homomorphic encryption(HE)으로 암호화해 aggregator에 보내는데, ciphertext 크기 팽창과 modular multiplication 부하 때문에 aggregation이 전체 학습 iteration 시간의 최대 82.75%를 차지하는 병목이 된다. Flagger는 이 aggregation을 DPU(in-network)와 CSD(in-storage)라는 두 개의 이종 near-data accelerator에 분산시키는 협력형 아키텍처로, Montgomery 알고리즘(MontMul) 기반 aggregation을 pre-processing / combination / post-processing 세 단계로 나눠 각각의 특성(streaming vs. I/O-intensive)에 맞는 가속기에 배정한다. Flagger-Runtime이 이 두 가속기를 동적으로 오케스트레이션하고 peer-to-peer DMA로 host CPU/메모리를 경유하지 않는 datapath를 구현한다. FPGA 기반 실물 구현 평가에서 CPU-centric aggregator 대비 aggregation을 평균 436%, 전체 FL iteration을 평균 277% 가속했다(p.916).

## 문제 & 동기
Cross-silo FL에서는 client(병원·은행·정부기관 등의 data silo)가 로컬 학습 후 gradient를 Paillier HE로 암호화해 중앙 aggregator에 전송하고, aggregator는 ciphertext를 그대로 더해(additive homomorphism) global update를 만든다(p.917, Fig.1). 문제는 HE가 계산량과 데이터량을 동시에 부풀린다는 것이다: 1024-bit Paillier에서 ciphertext는 평문 대비 2배 크기(2048-bit)로 부풀고, modulo 연산이 매우 무겁다(p.917). 이로 인해 aggregator 한 곳에 network·compute·storage·datapath 부하가 집중된다(p.918, Fig.3 breakdown):
- **Network**: WAN을 통한 대량 ciphertext 수신이 iteration 시간의 상당 부분을 차지 (Memory 구성 38.96%, Storage 구성 29.64%, p.918).
- **Computation**: aggregator의 homomorphic addition 처리량이 client 측 encryption 처리량보다 훨씬 낮아 연산 자원이 과소활용되면서도 Computation이 Storage 구성 전체 오버헤드의 45.83%를 차지 (p.918).
- **Storage**: ciphertext 볼륨이 single iteration당 0.75~121.47GB에 달해 SSD 저장이 불가피해지고, SSD 사용 시 aggregation 시간이 Memory 대비 평균 113.83% 증가 (p.919, Table II 참조).
- **Data path**: NIC→host memory→SSD→host memory→accelerator로 이어지는 host 경유 데이터 이동(redundant copy)이 Internal IO 오버헤드로 aggregation 시간을 평균 70.31%(전 구간 기준 58.95%도 언급) 늘림 (p.915, p.919).

> [!quote]- 📄 원문 표현 (paper)
> - "the aggregation process consumed up to 82.75% of the total training time for a single cross-silo FL training iteration" (p.915)
> - "the computing overhead of the aggregator (Computation in Figure 3b) still accounts for an average of 45.83% of the total overhead in Storage" (p.918)
> - "Frequent transmission of inflated ciphertexts through the aggregator's components... incurs to an elongated data path and multiple data replications, causing a 70.31% extension of the aggregation latency" (p.915)

## 핵심 통찰 (Key Insight)
1. **Aggregation을 Montgomery 알고리즘 기반 3단계로 분해하고, 각 단계의 I/O·compute 특성에 맞춰 서로 다른 near-data accelerator에 배정한다.** Pre-processing(ciphertext를 Montgomery space로 변환)과 post-processing(결과를 다시 변환)은 ciphertext 1개 + public key만 있으면 되는 per-ciphertext 연산이라 네트워크에서 스트리밍되며 도착하는 packet과 동시에 처리 가능 → in-network 가속(Flagger-DPU)에 적합. Combination(실제 합산)은 모든 client의 ciphertext가 storage에 모여야 시작 가능한 I/O-intensive 작업 → in-storage 가속(Flagger-CSD)에 적합(p.920). 이렇게 하면 Flagger-CSD에 도달하는 모든 ciphertext가 이미 Montgomery space에 있으므로 combining 단계에서 추가 변환이 필요 없다.
2. **DPU-CSD 간 peer-to-peer DMA + 통합 identifier(ACT-ID)로 host를 완전히 우회하는 datapath를 만든다.** 전통적 aggregator는 NIC↔host memory↔SSD↔host memory↔accelerator로 데이터를 여러 번 복사하지만, Flagger는 Flagger-Runtime의 orchestration만으로 DPU-CSD 간 직접 DMA 전송을 수행해 redundant copy를 제거한다(p.920, p.922).
3. **Flagger-CSD 내부에서 compute-IO 파이프라인을 구성해 느린 flash read latency를 MontMul 연산과 겹쳐(overlap) 감춘다.** 채널 단위 병렬성을 활용하도록 동일 client의 ciphertext를 서로 다른 채널에 분산 배치하고, DRAM write buffer에 있는 intermediate 결과를 flash에 있는 것보다 우선 처리하는 prioritization으로 compute engine이 항상 바쁘게 유지되게 한다(p.921).

> [!quote]- 📄 원문 표현 (paper)
> - "the whole aggregation is thereby segmented into three phases based on the Montgomery algorithm: the pre-processing phase... the combination phase... and the post-processing phase" (p.920)
> - "we employ peer-to-peer DMA transmission of ciphertexts between Flagger-DPU and Flagger-CSD, thereby completely bypassing the CPU and main memory" (p.920)
> - "Flagger-CSD prioritizes the processing of intermediate ciphertexts in the DRAM buffer over those in flash chips... minimizes the likelihood of evicting intermediate results to flash chips, which is exceedingly rare in our experiments" (p.921)

## 설계 / 메커니즘 (Design)
Flagger는 세 컴포넌트로 구성된다(Fig.4, p.919): **Flagger-DPU**, **Flagger-CSD**, **Flagger-Runtime**. 공통 연산 엔진으로 두 가속기 모두 **MontMul (Montgomery modular multiplication) engine**을 탑재해 long-integer modulo 연산을 짧은 정수 곱셈으로 대체한다(p.920, Fig.5). 반복형(iterative) digit-digit Montgomery multiplication으로 구현되어 2048-bit 정수 두 개의 곱을 1,088 cycle에 계산하며 300MHz에서 유닛당 이론 최대 처리량 275.37KOPS다(p.922).

**Flagger-DPU (Fig.6, p.920)**: Xilinx Alveo U55C FPGA 카드 기반. Network Infrastructure(CMAC layer + 100Gbps TCP/IP stack, QSFP 포트)를 하드웨어로 구현해 host 개입 없이 TCP 패킷을 직접 처리한다. TCP 세그멘테이션으로 ciphertext가 패킷 경계에서 쪼개지지 않도록 각 TCP 패킷에 완결된 ciphertext만 담기게 강제한다(1024-bit Paillier ciphertext 256B, MSS 1460B → 패킷당 최대 5개 ciphertext = 1280B payload, 나머지는 패딩)(p.921). Processing Unit은 parser→MontMul engine→deparser로 구성되어 Rx buffer에서 읽어올 때 pre-process, Tx buffer에 쓸 때 post-process 함으로써 재전송 시 중복 계산을 피한다. Management Unit(session manager, task scheduler, data dispatcher, key table)이 client 세션과 MontMul 유닛 부하를 관리한다. 40개의 MontMul 유닛을 배치해 이론 최대 11.01MOPS(p.922).

**Flagger-CSD (Fig.7, p.921)**: Daisyplus OpenSSD(Xilinx ZU17EG UltraScale+ MPSoC, 쿼드코어 ARM, 2GB DRAM) 기반. 전통 SSD의 logical block address(LBA) 대신 **ACT-ID**(32-bit unique identifier, A=애플리케이션·C=클라이언트·T=모델 업데이트 내 ciphertext)를 도입해 FTL이 이를 직접 물리 주소(PBA)에 block-level로 매핑, page-level mapping보다 단순화된 in-storage aggregation을 지원한다(p.921). 동일 AC-ID(app+client) ciphertext는 같은 flash block에 순차 배치되고, 서로 다른 client의 ciphertext는 서로 다른 채널에 인터리브되어 채널-레벨 병렬성으로 read stall을 방지한다(p.922). Task scheduler가 매칭되는 AT-ID를 가진 ciphertext 쌍을 MontMul 유닛에 배정해 partial sum을 만들고, 이 intermediate 결과를 DRAM write buffer에 임시 저장한 뒤 다음 라운드 대상으로 재배정하는 방식을 aggregation이 끝날 때까지 반복한다(p.922).

**Flagger-Runtime**: FATE(오픈소스 FL 프레임워크) 위에 구축된 host-side 소프트웨어 오케스트레이션 계층. Control-plane(client 세션·모델 크기·public key 등 metadata 관리, DPU/CSD 드라이버를 통한 커맨드 전달)과 data-plane(ACT-ID 기반 NVMe write/read 커맨드로 DPU-CSD 간 P2P DMA 개시)을 모두 담당한다(p.921-922). 또한 **dynamic offloading**: DPU가 incoming traffic 폭주로 compute engine이 포화되면 pre-processing 작업 일부를 CSD로 전달하고, 반대로 CSD가 과부하면 combination 작업 일부를 DPU로 전달하는 실시간 부하 분산을 수행한다(p.922).

> [!quote]- 📄 원문 표현 (paper)
> - "An ACT-ID is a unique 32-bit integer assigned to a ciphertext in different components of Flagger. It is comprised of three fields: A for different FL Applications, C for Clients, and T for cipherText" (p.921)
> - "Flagger-Runtime addresses this challenge by dynamically directing excess computation during the pre-processing phase to Flagger-CSD... Flagger-CSD can also offload combination phase tasks to Flagger-DPU" (p.922)

## 평가 (Evaluation)
실물 FPGA 구현(Flagger-DPU on Alveo U55C, Flagger-CSD on Daisyplus OpenSSD) + FATE 기반 Flagger-Runtime으로 6개 cross-silo FL 워크로드(32xCN/32xLM/32xDN/32xEN/64xRN/64xVG, Table II, p.923)를 100Gbps DAC 케이블로 연결한 테스트베드에서 평가(Table I, p.923). CPU-centric baseline은 Intel i7-4790(4C8T) aggregator + 50 CPU thread AVX512 client encryption.

- **전체 성능**: Flagger-full은 CPU-centric 대비 aggregation 436%, 전체 iteration 277% 평균 가속(p.916, abstract/contribution). 개별 워크로드 기준으로는 최대 aggregation 657%, 총처리량 384% 가속(Fig.8, p.924).
- **단계별 기여**: CPU-Mont(CPU에서 Montgomery만 적용)는 baseline 대비 total/aggregation throughput 13%/15% 개선. FPGA-Mont(네트워킹 없는 순수 FPGA HE 가속)는 60% 적은 연산 능력에도 58%/66% 개선(NDP 없는 datapath가 병목이라서 CSD-acc가 FPGA-Mont보다 12%/16% 더 낫다). Flagger-raw(coordination 없는 loose coupling)는 145%/190%(total/aggregation), Flagger-full(동적 오프로딩+coordination)이 최댓값을 달성. Flagger-stream(스트리밍 방식 partial aggregation)은 Flagger-full 대비 22%/14% 낮은 성능(p.924).
- **Comb 단계 시간 절감**: CPU-Mont·FPGA-Mont가 Comb 시간을 각각 17.27%, 57.14% 줄이고, Flagger-full은 70.82%까지 줄인다 — DPU-CSD 협력 덕분(p.924).
- **자원 활용률**: MontMul 유닛의 평균 활용률(busy 비율)이 Flagger-full 52.44%, Flagger-raw 36.56%, FPGA-Mont 35.78%(Fig.9, p.924).
- **HE 가속기 비교(Table III, p.924)**: PipeFL·PHEP 대비 Flagger는 PipeFL보다 19.82배 우수. PHEP(ASIC)는 MontMul 자체 속도가 Flagger보다 3.15배 빠르지만, 전체 aggregation 성능에서는 Flagger가 PHEP보다 1.35배 앞선다.
- **FPGA 자원(Table IV, p.925)**: 단일 MontMul 유닛 LUT 6.4K/FF 8.9K/DSP 164. Flagger-DPU 전체는 DSP 72.70% 사용, Flagger-CSD는 DSP 82.70% 사용.
- **ASIC 추정**: 45nm 라이브러리로 150개 MontMul 유닛 합성 시 130mm², 600MHz에서 76.79MOPS로 FPGA 대비 약 7배(p.926).
- **네트워크/스토리지 세부(Fig.11, 12, p.925)**: in-network HE 처리 없이는 최대 79Gbps, in-network 처리 포함 시 45 operand까지 선형 증가해 41.12Gbps에서 포화(자원 과활용으로 이후 감소). In-storage aggregation은 8 operand(full utilization) 기준 flash page read latency 대비 추가 35.07% latency만 발생. Per-unit latency breakdown: DPU pre-processing은 Montgomery space 변환이 latency의 79.87% 차지(연산 자원 제약), CSD는 flash read가 실행시간의 51.72% 차지.
- **Pre-processing 위치 비교(Fig.13, p.925)**: client 측에서 pre-processing 수행(Client-pre) 대비 Flagger-full은 총 시간 최대 20.56% 절감. Client-pre는 client 부담 증가로 시간 비용이 평균 17.12% 늘고, Flagger-full의 network 비용은 Client-pre 대비 겨우 2.49% 더 길다.
- **확장성(Fig.14, p.926)**: client 수·HE key 크기·모델 크기를 늘려도 Flagger가 CPU-centric보다 더 완만하게 증가 — 특히 모델 크기가 커질수록(LLM급 워크로드 겨냥) Flagger의 이득이 커짐을 시사.

> [!quote]- 📄 원문 표현 (paper)
> - "compared with a traditional CPU-centric FL aggregator with adequate resources, Flagger expedites the aggregation process and overall FL iteration by 436% and 277%, respectively" (p.916)
> - "Flagger... leading to maximum speedups of 657% for aggregation and 384% for total throughput" (p.924)
> - "Flagger significantly outperformed PipeFL by 19.82×. Although PHEP has a 3.15× faster MontMul speed than Flagger owing to ASIC's superior performance over FPGA, Flagger still surpasses PHEP by 1.35× in overall aggregation" (p.924)

## 섹션 노트
- **I. Introduction**: cross-silo FL·HE 배경과 aggregator가 병목이 되는 현상을 소개, network/computation/storage/data path 네 요인으로 원인 규명, 기여 3가지(DPU-CSD cooperative architecture, in-network HE via Flagger-DPU, in-storage aggregation via Flagger-CSD) 제시.
- **II. Background**: FL 학습 절차(negotiate key → local update → encrypt → upload → aggregate → distribute → decrypt), Paillier HE의 additive homomorphism, 전통 aggregator 아키텍처(host+NIC+SSD+accelerator, PCIe로 연결)와 데이터 흐름(①~⑥), DPU/CSD 아키텍처 개요.
- **III. Preliminary Study**: 6개 FL 앱에 대해 실측한 aggregation 오버헤드 breakdown(Fig.3)으로 network/computation/storage/data path 4대 병목 정량화.
- **IV. Flagger Overview**: 3단계(pre/combination/post-processing) 분해 원리와 DPU-CSD 역할 분담 근거, peer-to-peer DMA로 host 우회하는 coordination 설계.
- **V. Key Designs**: MontMul 엔진 원리(Montgomery 알고리즘), Flagger-DPU 상세(네트워크 스택 통합, 패킷 단위 pre/post-processing), Flagger-CSD 상세(ACT-ID 기반 FTL, DRAM buffer prioritization, 채널 병렬성), Flagger-Runtime(control/data-plane coordination, dynamic offloading).
- **VI. Implementation**: Alveo U55C 기반 Flagger-DPU(EasyNet 기반 TCP/IP 스택, XRT 드라이버), Daisyplus OpenSSD 기반 Flagger-CSD(OpenExpress NVMe controller 개조, Greedy-FTL 기반 FTL, UNVMe 라이브러리 드라이버), FATE 기반 Flagger-Runtime.
- **VII. Evaluation**: 테스트베드·워크로드(Table I,II) → 전체 성능 비교(7개 aggregator 플랫폼) → 컴포넌트별 deep-dive(자원 활용, in-network/in-storage 세부, HE 가속기 비교) → 확장성 테스트(client 수, key 크기, 모델 크기).
- **VIII. Related Work & Discussion**: 기존 FL 가속(client 측 위주), FL aggregator 아키텍처(in-network aggregation은 HE 미지원), NDP 연구 비교, Flagger의 타 HE 스킴(FHE 등)·경량 프라이버시 기법(differential privacy) 대비 적용 가능성 논의.
- **IX. Conclusion**: aggregator가 cross-silo FL의 핵심 병목임을 재확인하고 DPU+CSD 협력 근시데이터처리 아키텍처로 이를 해소했다고 요약, 저비용 NAND flash 활용으로 DRAM 의존도를 낮춘 점도 강조.

## 핵심 용어 (Key terms)
- **Cross-silo Federated Learning**: 병원·은행·정부기관 등 소수의 신뢰 가능한 대형 data silo가 원본 데이터를 공유하지 않고 공동으로 모델을 학습하는 FL 형태.
- **Homomorphic Encryption (HE) / Paillier scheme**: ciphertext 상태에서 직접 산술 연산이 가능한 암호화 기법. Paillier는 additive HE로 gradient 합산에 적합하지만 modulo 연산 부하와 ciphertext 크기 팽창(평문 대비 2배, 1024-bit 키 기준 2048-bit)을 유발.
- **Montgomery modular multiplication (MontMul)**: 무거운 long-integer modulo 연산을 짧은 정수 곱셈 반복으로 대체하는 알고리즘. Flagger의 DPU/CSD 공용 연산 엔진.
- **Data Processing Unit (DPU)**: SmartNIC 계열 near-data accelerator로 네트워크 포트에 근접해 스트리밍 데이터를 처리. 메모리 용량이 제한적(4~16GB)이라 대용량 I/O-heavy 작업엔 부적합.
- **Computational Storage Drive (CSD)**: SSD 컨트롤러에 compute engine을 내장한 근-데이터 저장장치. flash 채널의 높은 내부 대역폭을 이용해 대용량 데이터에 대한 연산을 저장장치 안에서 수행.
- **ACT-ID**: Flagger-CSD가 도입한 32-bit 통합 identifier(Application+Client+ciphertexT). 전통적 LBA를 대체해 FTL이 ciphertext를 블록 단위로 관리하게 함.
- **Flash Translation Layer (FTL)**: SSD 내부에서 논리주소를 물리 블록 주소로 매핑하는 계층. Flagger-CSD는 ACT-ID-to-PBA 매핑으로 이를 단순화.
- **Peer-to-peer DMA**: host CPU/메모리를 거치지 않고 두 accelerator(DPU↔CSD)가 직접 데이터를 주고받는 전송 방식.
- **Pre-/Combination/Post-processing phase**: Flagger가 aggregation을 나눈 세 단계 — ciphertext를 Montgomery space로 변환(pre), 실제 합산(combination), 결과를 원래 공간으로 복원(post).
- **BatchCrypt**: 여러 평문을 하나의 ciphertext로 묶어(batching) ciphertext 크기 팽창을 줄이는 기법. Flagger가 큰 모델(ResNet-152, VGG-11)에 적용해 HE 오버헤드를 완화(p.923).

## 강점 · 한계 · 열린 질문
- **강점**: (1) 실물 FPGA 2종(Alveo U55C + Daisyplus OpenSSD) + 실제 FL 프레임워크(FATE) 위에서 end-to-end로 구현·검증한 시스템 논문이라 시뮬레이션 한계가 없다. (2) DPU와 CSD 각각의 물리적 특성(네트워크 인접성 vs. flash 대역폭)에 맞춰 알고리즘 단계를 정확히 매핑한 설계 논리가 명료하다. (3) dynamic offloading으로 정적 분할의 부하 불균형 문제를 완화했다.
- **한계**: (1) 평가 threat model이 "honest-but-curious" aggregator로 한정되어 malicious aggregator 대응은 다루지 않는다(p.917). (2) MontMul 엔진은 Paillier 기반 HE에 특화되어 있어 FHE(NTT 연산 등) 적용 시 하드웨어 재설계가 필요함을 저자도 인정(p.927, "Adaptability of Flagger"). (3) client 측 encryption/decryption 시간은 평가에서 <1%로 제외되어(p.918) 실제로 encryption throughput이 낮은 저자원 client 환경에서의 효과는 별도 검증이 없다. (4) Flagger-CSD 용량은 실험에서 64GB×2로, 실제 수백GB급 대형 모델 워크로드에서의 storage scaling 한계는 명확히 다루지 않는다.
- **열린 질문**: ASIC 구현(7배 성능 개선 추정치)이 실제 tape-out 시 전력/면적 트레이드오프에서도 유효한가? Flagger의 ACT-ID/블록 매핑 방식이 다른 근-데이터 CSD 워크로드(GNN, LSM 등)에도 일반화될 수 있는가? DPU-CSD 간 dynamic offloading 정책이 다중 aggregator/다중 CSD로 확장될 때도 동일한 이득을 유지하는가?

## ❓ Q&A (자가 점검)
> [!question]- Flagger가 aggregation을 세 단계로 나누는 기준은 무엇이고, 왜 pre/post-processing은 DPU에, combination은 CSD에 배정하는가?
> Montgomery 알고리즘 기준으로 pre-processing(ciphertext를 Montgomery space로 변환)과 post-processing(결과를 복원)은 ciphertext 1개와 public key만 있으면 되는 per-ciphertext 연산이라 네트워크로 스트리밍되는 packet과 동시에 처리 가능하다. 반면 combination(실제 합산)은 모든 client의 ciphertext가 모여야 시작되는 I/O-intensive 작업이라 storage에 있는 CSD에서 처리하는 것이 데이터 이동을 최소화한다(p.920).

> [!question]- ACT-ID는 무엇이며 전통적 LBA 대비 어떤 이점이 있는가?
> ACT-ID는 Application·Client·ciphertext(within model update) 세 필드로 구성된 32-bit 통합 identifier로, Flagger-CSD의 FTL이 이를 직접 물리 블록 주소(PBA)에 block-level로 매핑한다. 전통적 page-level LBA 매핑 대신 이를 쓰면 어떤 client의 ciphertext인지 메타데이터가 매핑 테이블 자체에 포함되어 host 개입 없이 자율적으로 ciphertext를 관리·병합할 수 있다(p.921).

> [!question]- CPU-centric aggregator에서 aggregation이 병목이 되는 4가지 요인은?
> Network(WAN을 통한 대량 ciphertext 전송), Computation(aggregator의 homomorphic addition 처리 능력 부족·과소활용), Storage(ciphertext 볼륨 팽창으로 SSD 의존 시 I/O 지연), Data path(host 경유 redundant copy로 인한 datapath 연장) 네 가지다(p.918-919, Fig.3).

> [!question]- Flagger-full과 Flagger-raw의 차이는 무엇이고 왜 Flagger-full이 더 빠른가?
> 둘 다 DPU+CSD를 사용하지만 Flagger-raw는 direct datapath와 unified task scheduling/dynamic offloading 없이 loosely coupled로 동작해 accelerator 간 부하 불균형이 생긴다. Flagger-full은 tightly coupled + dynamic offloading + coordination을 추가해 DPU/CSD MontMul 유닛 활용률을 각각 52.44%(vs raw 36.56%)로 끌어올려 aggregation 657%, total 384% 최대 speedup을 달성한다(p.924, Fig.9).

> [!question]- Flagger-stream이 Flagger-full보다 오히려 느린 이유는?
> Flagger-stream은 CPU가 partial aggregation을 수행하며 pre-processing과 combination을 overlap 시키려 하지만, CPU의 aggregation 처리 속도가 Flagger-DPU의 pre-processing 처리량보다 훨씬 느려서 Comb 단계가 Pre 단계보다 86.46% 더 길어지고, 결과적으로 Flagger-full보다 22%/14% 낮은 total/aggregation 성능을 보인다(p.924).

> [!question]- Flagger-CSD가 flash read latency를 어떻게 감추는가?
> compute-IO 파이프라인을 구성해 MontMul 연산과 flash 읽기를 overlap시키고, DRAM write buffer에 있는 intermediate 결과를 flash에 있는 것보다 우선 처리(prioritization)해 compute engine이 항상 바쁘게 유지되도록 한다. 또 동일 client의 ciphertext를 서로 다른 채널에 분산 배치해 채널-레벨 병렬성으로 read stall을 방지한다(p.921).

> [!question]- HE 키 크기가 커지거나 client 수·모델 크기가 늘어날 때 Flagger의 상대적 이점은 어떻게 변하는가?
> 세 축(client 수, HE key 크기, 모델 크기) 모두에서 CPU-centric은 시간 비용이 Flagger보다 더 가파르게 증가한다. 특히 모델 크기가 커질수록 Flagger의 이득이 더 커져, 저자들은 이를 대규모(LLM급) 워크로드로의 확장 가능성 근거로 제시한다(p.926, Fig.14).

## 🔗 Connections
[[In-Storage Computing]] · [[ISCA]] · [[2024]]
관련: [[DockerSSD - Containerized In-Storage Processing and Hardware Acceleration for Computational SSDs]] (동일 저자 Myoungsoo Jung, CSD 아키텍처 계열) · [[CIPHERMATCH - Accelerating Homomorphic Encryption-Based String Matching via Memory-Efficient Data Packing and In-Flash Processing]] (HE + in-storage 가속 계열) · [[BeaconGNN - Large-Scale GNN Acceleration with Asynchronous In-Storage Computing]] (CSD 기반 near-data accelerator 설계 계열)

## References worth following
- Zhang et al., "Batchcrypt: Efficient homomorphic encryption for cross-silo federated learning," USENIX ATC 2020 [101] — Flagger가 대형 모델(ResNet-152, VGG-11)에 적용한 ciphertext batching 기법의 원 논문.
- Zhang et al., "[FLASH]: Towards a high-performance hardware acceleration architecture for cross-silo federated learning," NSDI 2023 [103] — client 측 FL 가속 관련 선행 연구, Flagger의 워크로드 선정 근거로 인용.
- Shi et al., "PHEP: Paillier homomorphic encryption processors for privacy-preserving applications in cloud computing," HCS 2023 [74] — Flagger가 Table III에서 직접 성능 비교한 ASIC HE 가속기.
- Amiet, Curiger, Zbinden, "Flexible fpga-based architectures for curve point multiplication over gf(p)," DSD 2016 [10] — Flagger의 MontMul 엔진 하드웨어 구현이 기반한 iterative digit-digit Montgomery multiplication 원류 연구.
- Jung, "OpenExpress: Fully hardware automated open research framework for future fast NVMe devices," USENIX ATC 2020 [38] — Flagger-CSD의 NVMe 컨트롤러 구현 기반.

## Personal annotations
<!-- 본인 메모 영역 -->
