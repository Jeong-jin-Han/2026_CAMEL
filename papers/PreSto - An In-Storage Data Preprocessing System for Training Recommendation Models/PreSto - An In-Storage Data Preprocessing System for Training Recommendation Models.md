---
title: "PreSto: An In-Storage Data Preprocessing System for Training Recommendation Models"
description: "추천모델(RecSys) 학습의 온라인 데이터 전처리(ETL)를 SmartSSD 기반 In-Storage Processing으로 오프로드해 CPU-centric 방식 대비 처리량 9.6배, 비용효율 4.3배, 에너지효율 11.3배를 달성하는 시스템"
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
  - topic/recsys
  - topic/smartssd
  - topic/near-data-processing
  - topic/data-preprocessing
---

# PreSto: An In-Storage Data Preprocessing System for Training Recommendation Models
> **ISCA 2024** · cluster/isc · Source: [PreSto - An In-Storage Data Preprocessing System for Training Recommendation Models.pdf](<PreSto - An In-Storage Data Preprocessing System for Training Recommendation Models.pdf>)

저자: Yunjae Lee† (School of Electrical Engineering, KAIST), Hyeseong Kim† (School of Electrical Engineering, KAIST), Minsoo Rhu (School of Electrical Engineering, KAIST) — † co-first authors, Minsoo Rhu가 corresponding author

## TL;DR
추천시스템(RecSys) 학습 파이프라인의 온라인 데이터 전처리(ETL) 단계는 feature generation(Bucketize)과 feature normalization(SigridHash, Log) 연산이 전체 전처리 시간의 79%를 차지하는 병목이며, 이 연산들은 inter-/intra-feature parallelism이 풍부하지만 compute-bound하고 working set이 작아 CPU로는 비효율적이다. PreSto는 이 연산을 SmartSSD(SSD+FPGA, NVMe U.2 폼팩터, 25W 전력 예산) 안의 전용 하드웨어(Decoder/Bucketize/SigridHash/Log unit)로 오프로드해 데이터가 저장된 곳에서 "in-storage" 전처리를 수행함으로써, 별도의 disaggregated CPU 서버 풀 없이도 GPU 학습 스루풋을 따라잡는다. PoC 프로토타입(단일 SmartSSD)과 이를 기반으로 한 분석 모델을 통해, baseline CPU-centric disaggregated 전처리 대비 평균 9.6× 전처리 시간 단축, 4.3× 비용효율(TCO) 개선, 11.3× 에너지효율 개선을 보인다.

## 문제 & 동기
RecSys 학습에서는 페타바이트급 raw feature 데이터를 GPU가 소비할 train-ready tensor로 바꾸는 ETL(Extract-Transform-Load) 전처리 단계가 필요하며, 최근에는 저장공간 문제로 offline 대신 "on-the-fly" online preprocessing이 대세가 되고 있다(p.341). 전처리와 학습을 같은 서버 노드에 co-locate하면 CPU 코어 수가 제한적이어서(예: DGX A100 서버는 GPU당 16 코어) GPU utilization이 20% 미만으로 떨어질 수 있고(Section III-A), 이를 해결하려는 server disaggregation(CPU 서버 풀을 전처리 전용으로 분리, Zhao et al. [70], Audibert et al. [5])은 배포 비용과 전력 소비가 크다(p.341, Fig.2b).

저자들의 characterization에 따르면, 프로덕션급 데이터(dense 504개·sparse 42개, 평균 sparse feature 길이 20)를 반영한 합성 모델 RM2-5는 공개 Criteo 데이터셋(RM1) 대비 전처리 지연이 최대 14× 증가하며(Fig.5, p.344), feature generation(Bucketize)과 feature normalization(SigridHash, Log)이 평균 79%의 전처리 시간을 차지해 가장 큰 성능 병목이 된다(p.342). Fig.3에서는 전처리 CPU 코어를 1→16으로 늘려도(15× 스루풋 향상) A100 GPU utilization이 20% 미만에 머무는 것을 보였고, Fig.4는 8-GPU 서버의 요구 스루풋을 맞추려면 RM5 기준 최대 367개의 CPU 코어가 필요함을 보였다.

Fig.6의 CPU/메모리 대역폭·LLC hit rate 분석은 Bucketize/SigridHash/Log 연산이 모두 CPU utilization은 높으면서 메모리 대역폭 이용률은 낮은(RM5에서도 최대 281.6GB/s의 15% 미만) compute-bound 특성을 가지며, working set도 수십 KB~수십 MB 수준으로 작아(Bucketize의 LLC hit rate 85%) 범용 CPU보다 domain-specific 가속기에 적합함을 보인다(p.345).

> [!quote]- 📄 원문 표현 (paper)
> - "Our characterization reveals that prior CPU-centric preprocessing is bottlenecked on feature generation and feature normalization operations as it fails to reap out the abundant inter-/intra-feature parallelism in RecSys preprocessing." (p.340, Abstract)
> - "This in turn leads to the feature generation and normalization to account for 79% of the RecSys data preprocessing time, causing the most significant performance bottleneck." (p.341)
> - "Even with 16 preprocessing workers ... the GPU spends less than 20% of its execution time actually conducting model training as the train-ready tensors are not being sufficiently supplied to the GPU." (p.344)

## 핵심 통찰 (Key Insight)
1. **전처리 연산의 병목(Bucketize/SigridHash/Log)은 compute-bound + 작은 working set + 높은 inter-/intra-feature parallelism을 가지므로 domain-specific 가속에 이상적이다.** 각 feature 컬럼은 서로 독립적으로 element-wise 변환되므로(inter-feature parallelism), 그리고 한 feature 내부의 각 값도 서로 독립적으로 처리 가능하므로(intra-feature parallelism), 하드와이어드 FPGA 로직으로 병렬화하기 쉽다.
2. **가속기를 "어디에" 둘 것인가가 핵심 설계 결정이다.** GPU에 co-locate(scale-up, Fig.7a)하면 가속기 수가 서버 내 GPU 슬롯 수로 제한되어 확장성이 없고 PCIe 버스가 hotspot이 된다. 반대로 가속기를 disaggregated pool로 두면(Fig.7b) 확장성은 얻지만 여전히 배포 비용·전력 문제가 남는다. PreSto는 SmartSSD(일반 SSD를 대체하는 NVMe U.2 폼팩터의 SSD+FPGA, 25W 이내)를 활용해 저장장치가 있는 곳에서 그대로 전처리를 수행("in-storage pre-processing")함으로써 두 문제를 모두 해결한다.
3. **로컬 처리는 데이터 이동 자체를 없앤다.** SmartSSD는 로컬 SSD에서 raw feature를 읽어 P2P(peer-to-peer)로 FPGA에 전달하므로, disaggregated CPU 풀 방식처럼 네트워크를 통해 raw 데이터를 복사해 오고(in) 전처리된 텐서를 다시 복사해 보내는(out) 오버헤드가 없다.

> [!quote]- 📄 원문 표현 (paper)
> - "Because the abundant inter-/intra-feature parallelism in data preprocessing is well-suited for domain-specific acceleration, our first key proposal is to offload the time-consuming feature generation and normalization operations to our accelerator." (p.341)
> - "SmartSSDs to become a drop-in replacement for normal SSDs while still staying within the NVMe's 25 Watts power envelope." (p.346)
> - "All data preprocessing operations are conducted locally within the storage system, all thanks to the use of commodity devices that operate within the NVMe SSD's power budget." (p.346)

## 설계 / 메커니즘 (Design)
**시스템 아키텍처(Fig.8, p.346).** PreSto ISP unit은 SmartSSD(SSD + 경량 FPGA, NVMe U.2)로, 저장 시스템 내 컬럼 파일(Apache Parquet 포맷 가정)을 로컬에서 읽어 자체 FPGA로 전처리한 뒤, 네트워크를 통해 GPU 학습 노드로 mini-batch를 전달한다.

**소프트웨어 아키텍처(Fig.9, p.346).** 두 컴포넌트로 구성된다.
- **Train manager**: 학습 워커 프로세스 일부로, 학습 job 정보를 받아 초기화하고(step ❶❷) mini-batch 입력용 input queue와 RPC를 설정하며, 준비된 mini-batch를 GPU로 전달해 학습을 개시(step ❻❼)한다. 또한 dummy mini-batch로 GPU의 최대 학습 스루풋 $T$를 offline으로 측정한다.
- **Preprocess manager**: SmartSSD 기반 전처리 워커를 spawn·관리한다(step ❸). 단일 SmartSSD의 전처리 스루풋 $P$를 offline 측정한 뒤, 필요한 SmartSSD 개수를 $T/P$로 산출해 GPU 요구를 충족하는 만큼만 동적으로 할당한다. 각 SmartSSD는 로컬 SSD에서 raw feature를 추출해 P2P로 FPGA에 전달(step ❹)하고, 전처리 완료된 데이터를 train manager의 input queue로 반환한다(step ❺).

**가속기 마이크로아키텍처(Fig.10, p.347).** FPGA는 Decoder unit(Parquet 컬럼 파일 디코딩), Feature generation unit(Bucketize), Feature normalization unit(SigridHash, Log)을 하드와이어드 로직으로 구현한다. 두 가지 병렬화 최적화를 적용한다: (1) inter-feature parallelism — 개별 feature마다 전용 processing element를 두어 off-chip global memory(DRAM) 인터페이스 대역폭을 최대한 활용, (2) intra-feature parallelism — 각 PE가 double-buffering으로 다음 feature value의 fetch와 현재 value의 transform을 오버랩.

Table II(p.348)의 FPGA 자원 이용률(223MHz 동작): 전체 LUT 54.02%, REG 28.03%, BRAM 48.05%, URAM 27.59%, DSP 29.81% (Decoder/Bucketize/SigridHash/Log unit별 세부 분해 포함).

> [!quote]- 📄 원문 표현 (paper)
> - "As each preprocessing worker independently generates mini-batch inputs locally, our proposed system provides highly scalable preprocessing service." (p.346)
> - "To maximally exploit inter-/intra-feature parallelism, we employ the following design optimizations. First ... we deploy multiple processing elements dedicated to each individual feature... Second ... each processing element employs double-buffering to overlap the next feature value's data fetch operation with the current feature value's generation and normalization operations." (p.347)

## 평가 (Evaluation)
**방법론(Section V, p.347-348).** PoC 프로토타입: 3대의 2-소켓 Intel Xeon Gold 6242 노드(노드당 32코어; 하나는 storage node, 나머지 두 개는 disaggregated CPU 풀 최대 64코어)를 10Gbps Ethernet으로 연결, GPU 노드는 AMD EPYC 7502 + 단일 NVIDIA A100. PreSto용으로 storage node에 Samsung SmartSSD [59] 1개를 추가하고, 가속기는 Xilinx Vitis HLS 2022.2로 설계. TorchArrow(v0.1.0)와 TorchRec(v0.3.2), mini-batch 크기 8,192. 대규모(멀티 SmartSSD/멀티 CPU 노드) 추정은 PoC 실측치를 스케일링하는 analytical model 사용. Cost-efficiency = $(Throughput \times Duration) / (CapEx + OpEx)$, $OpEx = \sum(Power \times Duration \times Electricity)$, Duration=3년, Electricity=$0.0733/kWh$.

**주요 수치**
- **스루풋(Fig.11, p.349)**: 단일 SmartSSD PreSto는 32-코어 Disagg(32)를 항상 능가; Disagg(64, 2노드 64코어)만 PreSto를 평균 27% 능가하지만 비용은 2× 더 든다.
- **지연 분해(Fig.12, p.349)**: PreSto는 Disagg 대비 mini-batch당 전처리 지연을 평균 9.6×(최대 11.6×) 감소. Extract 단계의 decoding은 병렬화가 덜 되어 PreSto 전체 전처리 시간의 평균 40.8%를 차지.
- **네트워크 오버헤드(Fig.13, p.349)**: RPC 기반 inter-node 통신 지연을 PreSto가 평균 2.9× 감소; Disagg의 네트워크 오버헤드는 RM2 기준 end-to-end 시간의 9.1%.
- **가속기 요구량(Fig.14, p.350)**: 8×A100 서버를 충족하려면 PreSto는 최대 9개 SmartSSD(9×25W=225W)만 필요한 반면, Disagg는 최대 367개 CPU 코어(12개 서버 노드)가 필요.
- **에너지효율(Fig.15a, p.350)**: 평균 11.3×(최대 15.1×) 개선.
- **비용효율/TCO(Fig.15b, p.350)**: 평균 4.3×(최대 5.6×) 개선.
- **대안 가속기와 비교(Fig.16, p.350, Section VI-C)**: PreSto(SmartSSD)는 A100 GPU 대비 평균 2.5× 속도 향상, discrete U280 FPGA(disaggregated) 대비 평균 5% 성능 손실이지만 SmartSSD는 TDP 25W로 U280(225W)·A100(250W)보다 훨씬 낮은 전력을 소비. U280은 disaggregated 노드로 데이터를 in/out 복사하는 오버헤드가 end-to-end 전처리 시간의 평균 47.6%를 차지해 속도 이점이 상쇄되며, PreSto(SmartSSD)는 PreSto(U280) 대비 평균 2.9× 높은 에너지효율.
- **feature 수 민감도(Fig.17, p.350, Section VI-D)**: feature 수가 늘어날수록 Disagg의 지연은 거의 선형으로 증가하지만, PreSto는 inter-/intra-feature parallelism을 활용해 일관되게 큰 속도 향상을 유지.

> [!quote]- 📄 원문 표현 (paper)
> - "PreSto outperforms the baseline CPU-centric system with a 9.6× speedup in end-to-end preprocessing time, 4.3× enhancement in cost-efficiency, and 11.3× improvement in energy-efficiency on average for production-scale RecSys preprocessing." (p.340, Abstract)
> - "PreSto only requires a maximum of 9 ISP units to match such high training throughput demand ... incur (9×25)=225 Watts of worst-case power consumption ... Disagg ... requires up to 367 CPU cores." (p.350)
> - "PreSto (SmartSSD) delivers much higher energy-efficiency (an average 2.9×) vs. PreSto (U280) by being custom-designed to right-size its compute units for data preprocessing under a tighter power budget (25 Watts)." (p.350)

## 섹션 노트
- **I. Introduction**: online preprocessing으로의 전환 배경, co-location/disaggregation 각각의 한계, PreSto의 in-storage 접근 제안.
- **II. Background**: RecSys 학습 파이프라인(ETL: Extract-Transform-Load)과 feature generation(Bucketize, Algorithm 1)·normalization(SigridHash, Algorithm 2; Log) 설명, CPU-centric 소프트웨어/하드웨어 아키텍처.
- **III. Characterization and Motivation**: TorchArrow로 공개(Criteo/RM1) + 합성 프로덕션급(RM2-5) 데이터셋을 사용한 워크로드 특성 분석; 79% 병목, CPU 코어 수요, compute-bound 특성 규명(Fig.3-6, Table I).
- **IV. PreSto 설계**: co-located vs disaggregated 가속기의 트레이드오프 분석 후 SmartSSD 기반 ISP 채택; 소프트웨어(train/preprocess manager)·하드웨어(FPGA 마이크로아키텍처) 설계.
- **V. Methodology**: 벤치마크(Criteo + 4개 합성 모델), PoC 하드웨어 구성, 전력 측정(PCM, nvidia-smi, Vivado), cost-efficiency 정의.
- **VI. Evaluation**: PoC 기반 성능/비용효율(A), 대규모 분석 모델 기반 에너지효율/TCO(B), 대안 가속기(GPU/discrete FPGA) 비교(C), feature 수 민감도(D).
- **VII. Related Work**: DNN 전처리 일반(TrainBox, DALI, DLBooster 등), RecSys 데이터 저장/전처리(Zhao et al., XDL, InTune, RecD, Tectonic-shift), RecSys 학습/추론 가속, 범용/도메인특화 ISP(GLIST, SmartSAGE, RecSSD, RM-SSD, GraphSSD 등) — 저자는 이들 중 어느 것도 RecSys 전처리를 compute-bound ISP 문제로 다루지 않았다고 차별화.
- **VIII. Conclusion**: ISP로 전처리를 데이터가 있는 곳(storage)에서 수행함으로써 전처리-학습 성능 격차를 저비용·저전력으로 해소.

## 핵심 용어 (Key terms)
- **ETL (Extract-Transform-Load)**: RecSys 데이터 전처리의 3단계 — raw feature 추출, feature generation/normalization으로 변환, GPU HBM으로 적재.
- **Feature generation (Bucketize)**: dense feature를 미리 정의된 bucket 경계로 나눠 sparse feature로 변환하는 연산(Algorithm 1).
- **Feature normalization (SigridHash / Log)**: sparse feature를 embedding table 크기 범위로 hash 정규화(SigridHash, Algorithm 2)하거나 dense feature를 log 함수로 정규화하는 연산.
- **Inter-/intra-feature parallelism**: 서로 다른 feature 간(inter) 그리고 한 feature 내 서로 다른 사용자 값 간(intra) 데이터 의존성이 없어 병렬화 가능한 특성.
- **Server disaggregation**: 전처리 전용 CPU 서버 풀을 GPU 학습 노드와 분리해 필요한 만큼 코어를 동적 할당하는 기존 접근(Zhao et al., Audibert et al.).
- **SmartSSD**: 일반 NVMe U.2 SSD와 경량 FPGA를 결합한 computational storage 장치로, 25W 전력 예산 내에서 일반 SSD의 drop-in 대체품.
- **P2P (Peer-to-Peer) data transfer**: SmartSSD 내부에서 로컬 SSD의 raw 데이터를 호스트 CPU를 거치지 않고 직접 FPGA로 전달하는 방식.
- **Columnar format (Apache Parquet)**: 사용자 행이 아닌 feature 열 단위로 파일을 저장해 필요한 feature만 선택적으로 읽을 수 있게 하는 포맷.
- **Cost-efficiency (TCO)**: $Throughput \times Duration / (CapEx + OpEx)$로 정의되는 자본·운영 비용 대비 성능 지표.
- **Train manager / Preprocess manager**: PreSto 소프트웨어의 두 축으로, 각각 GPU 학습 스루풋 요구 산정·mini-batch 공급과 SmartSSD 워커 spawn·관리를 담당.

## 강점 · 한계 · 열린 질문
- **강점**: 워크로드 characterization(79% 병목, compute-bound·small working set)을 통해 왜 ISP가 적합한지 정량적으로 논증; SmartSSD라는 상용 25W computational storage에 맞춘 실용적 마이크로아키텍처; PoC + 분석 모델을 결합해 실측과 대규모 추정을 모두 제시.
- **한계**: 평가가 단일 training job, 통제된 소규모 클러스터(SmartSSD 1개) 기반 PoC + 분석적 스케일링에 의존하며, 실제 하이퍼스케일러의 수백~수천 GPU급 멀티노드/멀티테넌트 환경(논문도 p.348에서 "unavailability of SmartSSDs in cloud services"를 명시적 한계로 인정)에서의 검증은 아님. Decoder 유닛이 병목(전체 전처리 시간의 40.8%)으로 남아 있어 추가 개선 여지가 있음. Bucketize/SigridHash/Log 세 연산에 특화된 하드와이어드 로직이라 RecSys 전처리 연산 종류가 바뀌면(TorchArrow 외 다른 라이브러리·새로운 정규화 기법) 재설계가 필요할 수 있음.
- **열린 질문**: 여러 training job이 SmartSSD 풀을 동시에 공유(multi-tenant)할 때의 스케줄링/격리는? feature 종류가 계속 변화하는 프로덕션 환경에서 하드와이어드 유닛의 재구성(FPGA reconfiguration) 비용은? 더 큰 embedding table lookup/pooling 등 학습 단계 자체도 ISP로 확장할 수 있는가?

## ❓ Q&A (자가 점검)
> [!question]- PreSto가 오프로드하는 핵심 연산 두 가지는 무엇이며, 왜 전체 전처리 시간의 79%를 차지하는가?
> feature generation(Bucketize)과 feature normalization(SigridHash, Log)이다. 프로덕션급 모델(RM2-5)은 dense/sparse feature 수와 평균 sparse feature 길이가 공개 Criteo 데이터셋보다 훨씬 커서 이 연산들의 latency가 크게 증가하며, Fig.5·Fig.6(p.344-345)의 CPU/메모리/LLC 분석에서 CPU utilization은 높고 메모리 대역폭 이용률은 낮은 compute-bound 특성을 보였다.

> [!question]- 가속기를 GPU에 co-locate하는 방식(Fig.7a)과 disaggregated pool로 두는 방식(Fig.7b)의 한계는 각각 무엇인가?
> Co-locate 방식은 서버 내 슬롯 수로 가속기 수가 제한되어 확장성이 없고, 전처리 워커와 학습 워커가 PCIe 버스를 공유해 병목이 될 수 있다. Disaggregated pool은 확장성은 있지만 여전히 별도 서버 풀 배포에 따른 높은 CapEx/전력 소비 문제가 남는다(p.345-346).

> [!question]- PreSto가 필요한 SmartSSD 개수를 결정하는 방법은?
> Train manager가 dummy mini-batch로 GPU의 최대 학습 스루풋 $T$를 offline 측정하고, preprocess manager가 단일 SmartSSD의 전처리 스루풋 $P$를 offline 측정한 뒤, 필요한 SmartSSD 수를 $T/P$로 산출한다(p.347, step ❷).

> [!question]- PreSto의 지연 중 가장 큰 비중을 차지하는 단계는 무엇이고 왜 개선이 어려운가?
> Extract 단계의 decoding으로, PreSto 전체 전처리 시간의 평균 40.8%를 차지한다(p.349). Decoding 알고리즘은 feature generation/normalization에 비해 병렬화 여지가 적기 때문에 상대적으로 개선폭이 작다.

> [!question]- PreSto(SmartSSD)가 discrete U280 FPGA(disaggregated) 대비 평균 5% 느린데도 왜 더 나은 선택인가?
> U280은 disaggregated 노드에 있어 원격 저장소로부터 raw 데이터를 in/out 복사하는 오버헤드가 end-to-end 시간의 평균 47.6%를 차지하고, TDP도 225W로 SmartSSD(25W)보다 훨씬 높다. 그 결과 PreSto(SmartSSD)는 PreSto(U280) 대비 평균 2.9× 높은 에너지효율을 달성한다(p.350).

> [!question]- Cost-efficiency는 어떻게 정의되며, PreSto와 Disagg 사이에서 무엇이 이 값을 가르는가?
> $Cost\text{-}efficiency = \dfrac{Throughput \times Duration}{CapEx + OpEx}$로 정의된다(p.348). PreSto와 Disagg 모두 동일한 GPU 학습 스루풋 요구를 만족시키므로 분자(Throughput×Duration)는 동일하고, 차이는 분모인 CapEx+OpEx(하드웨어 구입비 + 전력 소비 기반 운영비)에서 발생한다.

> [!question]- Fig.3의 실험(1~16 코어)에서 관찰된 두 가지 핵심 사실은?
> (1) 전처리 스루풋은 CPU 코어 수에 거의 선형으로 증가해 16 워커에서 단일 워커 대비 15× 향상을 달성하지만, (2) 16 워커를 다 써도 A100 GPU는 실행 시간의 20% 미만만 실제 모델 학습에 사용해 train-ready tensor 공급 부족으로 GPU가 유휴 상태에 머문다(p.343-344).

> [!question]- PreSto가 network overhead를 줄이는 근본 원리는?
> SmartSSD가 로컬 SSD에서 raw feature를 P2P로 직접 FPGA에 전달해 전처리를 스토리지 내부(storage node)에서 완결하므로, Disagg처럼 raw 데이터를 원격 CPU 풀로 복사해 오고 전처리된 텐서를 다시 복사해 보내는 inter-node RPC 통신 자체가 필요 없다(p.349, Fig.13).

## 🔗 Connections
[[In-Storage Computing]] · [[ISCA]] · [[2024]]
관련: [[Tectonic-Shift - A Composite Storage Fabric for Large-Scale ML Training]] · [[Smart-Infinity - Fast Large Language Model Training using Near-Storage Processing on a Real System]] · [[MaxEmbed - Maximizing SSD bandwidth utilization for huge embedding models serving]]

## References worth following
- Zhao et al., "Understanding Data Storage and Ingestion for Large-Scale Deep Recommendation Model Training," ISCA 2022 [70] — Meta의 프로덕션 RecSys 전처리 특성을 처음 규명한 연구로, 본 논문의 characterization·baseline 설계(disaggregation)의 직접적 근거.
- S. Pan et al., "Facebook's Tectonic Filesystem," FAST 2021 [55] — 본 논문이 저장 시스템 백엔드로 가정한 columnar 파일이 어떻게 파티션·저장되는지의 기반이 되는 실제 프로덕션 파일시스템.
- Y. Lee, J. Chung, M. Rhu, "SmartSAGE: Training Large-scale Graph Neural Networks using In-Storage Processing Architectures," ISCA 2022 [38] — 같은 저자 그룹의 선행 ISP 연구로, GNN 학습에 ISP를 적용한 방법론적 계보.
- M. Wilkening et al., "RecSSD: Near Data Processing for Solid State Drive Based Recommendation Inference," ASPLOS 2021 [66] — RecSys에 ISP를 적용한 선행 연구지만 추론(inference) 단계에 초점, PreSto는 전처리(preprocessing)로 차별화.
- G. Koo et al., "Summarizer: Trading Communication with Computing Near Storage," MICRO 2017 [31] — 데이터 집약적 워크로드에 대한 일반적 ISP 아키텍처의 원류 중 하나로 관련 배경 이해에 유용.

## Personal annotations
<!-- 본인 메모 영역 -->
