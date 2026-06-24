---
title: "RiF: Improving Read Performance of Modern SSDs Using an On-Die Early-Retry Engine"
aliases: [RiF]
description: "flash die 내부에서 read-retry 필요 여부를 미리 예측해 불필요한 off-chip 전송·ECC 디코딩을 제거, channel 대역폭 낭비를 줄이는 on-die early-retry engine"
venue: HPCA
year: 2024
tier: deep
status: done
tags:
  - paper
  - cluster/reliability
  - topic/read-retry
  - topic/ftl
  - venue/hpca
  - year/2024
---

# RiF: Improving Read Performance of Modern SSDs Using an On-Die Early-Retry Engine

> **HPCA 2024** · `cluster/reliability` · Source: [RiF - Improving Read Performance of Modern SSDs Using an On-Die Early-Retry Engine.pdf](<RiF - Improving Read Performance of Modern SSDs Using an On-Die Early-Retry Engine.pdf>)

저자: Myoungjun Chun¹*, Jaeyong Lee¹*, Myungsuk Kim², Jisung Park³, Jihong Kim¹ (¹Seoul National University, ²Kyungpook National University, ³POSTECH; *공동 1저자)

## TL;DR
고밀도 3D NAND SSD에서는 read-retry가 흔하게 발생하는데, 기존 방식은 off-chip ECC 디코딩이 끝나야 retry 필요 여부를 알 수 있어 실패한 raw page를 flash channel로 전송하는 데 대역폭을 낭비한다. RiF(Retry-in-Flash)는 flash die 내부에 **ODEAR(On-Die EArly-Retry) engine**을 두어 (1) syndrome weight 기반으로 page가 uncorrectable한지 die 안에서 미리 예측(RP module, 98.7% 정확도)하고 (2) 그렇다면 die 안에서 즉시 V_REF를 조정해 재독(RVS module)함으로써, 디코딩 불가능한 page를 channel로 내보내지 않는다. 결과적으로 2K P/E cycle에서 평균 **72.1%**의 SSD I/O 대역폭 향상을 negligible한 area/power overhead(0.012 mm², 1.28 mW)로 달성한다.

## 문제 & 동기
- 현대 고성능 SSD는 다수의 flash channel을 병렬로 써서 높은 I/O 대역폭을 얻는다. 각 channel의 effective bandwidth가 떨어지면 전체 SSD 대역폭이 직접적으로 타격을 입는다 (p.1).
- 고밀도 3D TLC NAND는 error-prone하여, 1-KiB codeword당 72-bit 같은 강한 ECC를 써도 uncorrectable error를 완전히 못 막는다. RBER이 ECC 보정 능력을 넘으면 read-retry 절차가 시작된다 (p.1).
- read-retry는 같은 page를 (1 + N_RR)번 읽게 만들어 single host read를 증폭시키며 effective read bandwidth를 크게 떨어뜨린다 (p.1). tREAD = (tR + tDMA + tECC) × (N_RR + 1) (식 (1), p.4).
- 기존 연구(Sentinel, Swift-Read 등)는 좋은 V_REF를 골라 **N_RR을 줄이는** 데 집중했지만 본질적으로 **reactive**하다: off-chip ECC가 디코딩 실패를 선언한 **후에야** retry가 시작된다 (p.2).
- 저자가 식별한 기존 방식의 3가지 약점 (p.2): ① retry 결정이 off-chip ECC에서 이뤄져, ECC-decodable한 초기 page조차 일단 off-chip으로 전송돼야 하므로 최소 2번의 read가 필요(첫 실패 read의 channel 대역폭 낭비). ② retry 결정이 너무 늦다 — uncorrectable page가 channel을 통해 전송되어야 결정되므로 channel 대역폭이 낭비됨. ③ 큰 ECC 디코딩 latency 변동(최대 20배)이 후속 read transfer를 지연시켜 effective channel bandwidth를 떨어뜨림.

> [!quote]- 📄 원문 표현 (paper)
> "We propose a novel read-retry optimization scheme, Retry-in-Flash (RiF), which proactively minimizes the amount of time wasted in conventional read-retry procedures. Unlike existing read-retry solutions that focus on identifying an optimal read-reference voltage for a sensed page, the RiF scheme focuses on determining early on whether a read-retry will be required for the sensed data." (Abstract, p.1)
>
> "We identified three key weaknesses of existing read-retry solutions ... First, a decision to invoke a read-retry procedure is made by an off-chip ECC engine ... Second, a decision to invoke a read-retry procedure is made too late ... Third, large variation in ECC decoding latency often decreases the effective channel bandwidth by delaying the subsequent read transfer through a flash channel." (p.2)

## 핵심 통찰

> [!note]- 통찰 1 — read-retry는 이제 "드문 예외"가 아니라 "흔한 경우"다
> 2D NAND 시절엔 retry가 드물어 reactive off-chip 방식이 효율적이었다. 그러나 고밀도 3D NAND에서는 retry가 훨씬 빈번하다. 160개 실제 3D TLC chip을 특성화한 결과(Fig. 4, p.4), 0/200/500 P/E cycle에서 각각 17일/14일/10일 retention만 지나도 retry가 발생할 수 있다. 상용 SSD는 4주 이상 retention을 지원해야 하므로 **대부분의 read에서 retry가 common case**다. 심지어 fresh 상태(0 P/E cycle)에서도 retry가 필요할 수 있다 (p.4).

> [!note]- 통찰 2 — RBER과 syndrome weight 사이에 강한 상관이 있어, 디코딩 없이 correctability를 예측할 수 있다
> LDPC의 syndrome weight(Σs_k)는 RBER이 커질수록 함께 증가한다 (Fig. 10, p.7). 따라서 복잡한 LDPC 디코딩을 거치지 않고도 syndrome weight만 계산해 threshold ρ_s와 비교하면 page가 off-chip ECC로 보정 가능한지 예측할 수 있다. QC-LDPC 보정 능력 RBER 0.0085에 대응하는 syndrome weight ρ_s=3830을 기준으로 삼으면, **평균 99.1%(근사 적용 후 98.7%)** 예측 정확도를 얻는다 (p.7, p.9).

> [!note]- 통찰 3 — page 내 chunk 간 RBER이 유사하므로 전체 codeword를 다 볼 필요 없다
> 현대 NAND의 data randomization 덕분에 16-KiB page 내부의 오류가 균일하게 분포한다. 4-KiB chunk 단위로 봤을 때 chunk 간 RBER_max/RBER_min 차이가 4.5% 이하로 작다 (Fig. 12, p.8). 따라서 single codeword 크기(4-KiB) 하나만 검사해 syndrome을 계산하면 충분해, RP의 연산량을 크게 줄일 수 있다.

> [!note]- 통찰 4 — near-data processing을 read-retry에 적용
> RiF의 근본 아이디어는 in-flash processing(near-data processing) 개념을 read-retry 메커니즘에 도입한 것이다. flash die가 스스로 page의 correctability를 판단하게 함으로써, uncorrectable page를 off-chip ECC로 옮기는 비효율적 data movement 자체를 없앤다 (p.5). 저자 주장으로 read failure를 NAND flash 내부에서 예측한 첫 연구다 (p.13, §VII).

## 설계 / 메커니즘

> [!abstract]- ODEAR engine 전체 구조와 동작 흐름
> RiF-enabled flash die는 plane마다 **ODEAR(On-Die EArly-Retry) engine**을 둔다. ODEAR은 두 모듈로 구성 (p.5, Fig. 9):
> - **RP(Read-retry Predictor) module**: page가 off-chip ECC로 correctable한지 예측. ① read command가 오면 page를 page buffer에 sense → RP가 correctable로 예측하면 status register의 ready flag를 1로 set해 controller가 read 완료를 인지(❷). uncorrectable로 예측하면 RVS에 통보(❸).
> - **RVS(Read-Voltage Selector) module**: uncorrectable 예측 시 die 내부에서 Swift-Read command를 발행해 near-optimal V_REF를 찾고(❹) 그 전압으로 page를 re-read. 재독된 page는 RP를 다시 거치지 않고 곧장 off-chip ECC로 전송(❺❻). RVS는 controller 도움 없이 on-die로 동작 (p.5, p.7).
> 핵심: data transfer와 ECC 디코딩(Fig. 8의 ❷❸)을 uncorrectable page에 대해 ODEAR이 die 안에서 회피 → channel 대역폭 절약. RiF면 256-KiB read 실행시간이 SSD_one 대비 126 μs 단축(292 μs, Fig. 8c, p.6).

> [!abstract]- RP의 read-retry 예측 메커니즘 (syndrome weight 기반)
> LDPC 디코더 전체를 die에 넣는 건 비현실적이라, RP는 **error-correctability predictor**만 넣는다 (p.6). 각 syndrome s_k는 read data 일부 비트의 XOR로 얻는다. 오류가 없으면 s_k=0, RBER이 커질수록 s_k=1 확률↑ → syndrome weight↑. RP는 syndrome weight > ρ_s 이면 uncorrectable로 판정 (p.6).
> - ρ_s 선택: uncorrectable page는 RBER이 보정 능력보다 큼. RBER과 syndrome weight가 1:1 관계이므로 보정 능력 RBER 0.0085에 해당하는 syndrome weight를 ρ_s=3830으로 설정 (p.7).
> - 예측 정확도: uncorrectable page에 대해 평균 99.1% (Fig. 11, p.7). RBER이 보정 능력에 가까울 땐 정확도 50.3%로 떨어지지만, 그 RBER 구간은 1개월 retention 요구 대비 2% 미만이고 mispredict의 영향이 미미하다 (p.7).

> [!abstract]- RP의 두 가지 근사 최적화 (구현 비용 절감)
> ① **Approximate Syndrome Computation** (p.8):
> - *Chunk-based prediction*: page 내 chunk 간 RBER 유사성을 이용해 4-KiB chunk 하나만 검사 (single codeword). 4-KiB 선택은 misprediction overhead와 성능 이득의 균형(1-KiB는 RBER 분산 최대 13.5%로 큼).
> - *Syndrome pruning*: QC-LDPC의 parity check matrix H를 t×t 서브행렬(circulant)로 보고, 첫 t개 syndrome만 예측에 사용 → syndrome 계산량을 r배 절감. 본 논문 H는 4×36 블록(1024×1024 서브행렬)으로, 4096개 중 1024개만 계산.
> ② **On-Die Syndrome Computation** — codeword rearrangement (p.10): QC-LDPC의 shifted identity matrix 때문에 syndrome 계산 비트가 segment에 불규칙 분포한다. controller가 ECC 인코딩 후 die로 쓸 때 codeword segment를 미리 회전시켜 두면 H가 단순 identity 서브행렬로 환원되어, syndrome 계산이 segment 간 XOR 후 1의 개수 세기로 단순화된다. read 시 controller가 원래 layout으로 복원(LDPC는 이미 barrel shifter 보유). 두 근사 적용 후에도 예측 정확도 98.7% 유지 (Fig. 14, p.10).
> RP의 하드웨어 구조(Fig. 16, p.11): segment_reg → XOR → syndrome_reg → weight counter → accumulator → comparator(ρ_s). fully pipelined라 page buffer에서 chunk 가져오는 시간(4-KiB ≈ 2.5 μs)이 예측 지연을 좌우.

## 평가

> [!example]- 실험 설정 및 주요 수치
> **설정** (p.12, Table I): MQSim-E 확장 시뮬레이터. 2-TiB SSD, 8 channel, 4 die/channel, 4 plane/die, plane당 1888 block, block당 576개 16-KiB page. tR=40 μs, tPROG=400 μs, tBERS=3.5 ms, tDMA=13 μs, tECC=1~20 μs, tPRED=2.5 μs. host interface PCIe 4.0 8.0 GB/s, channel 1.2 GB/s. 4-KiB QC-LDPC(보정 능력 0.0085). 160개 실제 3D TLC chip 특성화 기반 RBER lookup table. 워크로드 8종(AliCloud, Systor traces) — read ratio 0.27~0.96 (Table II, p.12).
>
> **비교 대상 SSD**: SENC(Sentinel), SWR(Swift-Read), SWR+(SWR+advanced V_REF tracking), RPSSD(SSD-controller level RP 변형), RiFSSD(제안), SSD_zero(retry 없는 이상적 SSD).
>
> **핵심 결과**:
> - 2K P/E cycle에서 RiFSSD가 SENC/SWR/SWR+ 대비 평균 I/O 대역폭 **+72.1% / +61.2% / +50.0%** (Fig. 17, p.13).
> - SSD_zero(이상치) 대비 차이 최대 1.8%에 불과 (2K P/E, p.13).
> - 0K/1K/2K P/E cycle에서 SOTA off-chip 방식(Sentinel) 대비 각각 +23.8% / +47.4% / +72.1% (p.2).
> - 기존 SSD_one(이상적 retry 1회)도 retry로 인해 0K/1K/2K에서 평균 19.4%/34.9%/50.4% 대역폭 저하 (p.5–6). 가장 read-intensive한 Ali_124에서 SSD_one은 2831 MB/s에 그치는 반면 SSD_zero는 6026 MB/s 달성 (p.6).
> - **Tail latency**: Ali_124의 99.99th-percentile tail latency가 2K P/E에서 SENC/SWR/SWR+ 대비 각각 -91.8% / -82.6% / -56.3% (Fig. 19, p.13).
> - **Channel usage**(Fig. 18, p.13): Ali_124 2K P/E에서 SWR은 channel 대역폭의 54.4%를 UNCOR+ECCWAIT로 낭비. RiFSSD/RPSSD는 UNCOR에서 각각 1.8% / 19.9%만 낭비.
> - **Overhead**(p.13, §VI-C): 130 nm·100 MHz에서 RP module은 0.012 mm², 1.28 mW (현대 flash die ~101 mm² 대비 negligible). read-retry 필요 시 RP는 unrecoverable page 전송 에너지 ~907 nJ를 절약하고 예측에 ~3.2 nJ만 추가.

## 섹션 노트
- **§I Introduction**: host-side 최대 I/O 대역폭을 위해선 channel당 effective I/O 대역폭을 최대로 써야 함. read-retry가 single read를 (1+N_RR)배로 증폭. RiF는 ① retry 결정을 ECC 디코딩에서 분리, ② 최대한 이른 시점에 결정하는 게 novelty.
- **§II Background**: NAND 기본(V_TH, page/block 단위, MLC/TLC), P/E cycle에 따른 T_OX 손상·charge leakage로 read error 발생(Fig. 1). LDPC/QC-LDPC와 parity check matrix H·syndrome 설명(Fig. 2). 4-KiB QC-LDPC는 RBER 0.0085 초과 시 디코딩 실패 확률 급증, RBER 0.0085에서 반복횟수 20 도달(Fig. 3). V_REF 조정 read-retry 절차.
- **§III Read Retries in Modern 3D-Flash SSDs**: 160 chip 특성화로 retry 빈도 측정(Fig. 4) — retry는 common case. 기존 해법(Sentinel은 spare cell의 Sentinel Cells로 near-optimal V_REF 추정하나 추가 off-chip read로 N_RR이 최대 2배; Swift-Read는 1의 개수로 V_REF 예측)의 한계. SSD_one vs SSD_zero 비교로 retry의 대역폭 손실 정량화(Fig. 6). Root cause: 256-KiB read 타임라인 분석(Fig. 7) — SSD_one이 SSD_zero보다 166 μs 추가(252→418 μs).
- **§IV Design of RiF**: near-data processing을 retry에 적용. ODEAR=RP+RVS. RP의 syndrome weight 예측 원리, ρ_s=3830 도출, RVS의 die 내부 Swift-Read.
- **§V Implementation**: syndrome weight 계산의 die-level 구현 난점(추가 read latency 25%, 불규칙 bitwise). 두 최적화: approximate syndrome computation(chunk-based + syndrome pruning)과 codeword rearrangement, RP 하드웨어 조직(Fig. 16).
- **§VI Evaluations**: MQSim-E 기반 8 워크로드 평가. RiFSSD가 모든 P/E에서 우수, SSD_zero에 근접. tail latency, channel usage, overhead 분석.
- **§VII Related Work**: read-retry 최적화(대부분 N_RR 감소), in-flash processing(Flash-Cosmos, ParaBit, PiF). RiF는 read failure를 NAND 내부에서 예측한 첫 연구이자 in-flash processing을 일반 storage I/O 성능에 활용한 첫 연구.

## 핵심 용어
- **read-retry**: RBER이 ECC 보정 능력을 넘어 디코딩 실패 시, V_REF를 조정해 같은 page를 다시 읽는 절차. N_RR번 반복.
- **N_RR**: 한 read에서 반복되는 read-retry 횟수. 기존 연구의 주 최적화 대상(Sentinel은 평균 1.2까지 감소).
- **RiF (Retry-in-Flash)**: flash die 내부에서 retry 필요 여부를 예측·수행하는 제안 기법.
- **ODEAR (On-Die EArly-Retry) engine**: RiF를 구현하는 die 내 엔진. RP+RVS로 구성.
- **RP (Read-retry Predictor) module**: syndrome weight로 page의 off-chip correctability를 die 안에서 예측하는 모듈.
- **RVS (Read-Voltage Selector) module**: uncorrectable 예측 시 die 내부에서 Swift-Read로 near-optimal V_REF를 찾아 재독하는 모듈.
- **syndrome weight (Σs_k)**: LDPC parity check의 syndrome 합. RBER과 양의 상관 → correctability 예측 지표.
- **ρ_s**: syndrome weight threshold(=3830). 보정 능력 RBER 0.0085에 대응. 초과 시 uncorrectable 판정.
- **QC-LDPC**: Quasi-Cyclic LDPC. parity check matrix H가 circulant 서브행렬(shifted identity)로 구성. 하드웨어 구현 용이.
- **RBER (Raw Bit Error Rate)**: ECC 적용 전 raw 비트 오류율. 보정 능력 초과 시 retry 유발.
- **tECC / tDMA / tR**: 각각 ECC 디코딩 / channel 전송 / page sensing latency. tECC는 RBER에 따라 최대 20배 변동.
- **off-chip ECC engine**: channel-level에 있는 ECC 디코더. 기존엔 여기서만 retry 필요를 판단.
- **codeword rearrangement**: 쓰기 시 codeword segment를 미리 회전시켜 H를 단순 identity 형태로 만들어 on-die syndrome 계산을 단순화하는 기법.
- **Sentinel / Swift-Read**: 비교 대상 SOTA read-retry 최적화. 각각 spare cell 기반·1의 개수 기반 near-optimal V_REF 예측.

## 강점 · 한계 · 열린 질문
**강점**
- 문제 정의가 견고: 160개 실제 chip 특성화로 "retry는 이제 common case"임을 정량적으로 입증.
- 패러다임 전환: N_RR 최소화(reactive)에서 retry 필요 여부의 조기 예측(proactive)으로. 기존 V_REF 최적화 기법(Swift-Read)과 직교적이라 함께 쓸 수 있음.
- overhead가 매우 작음(0.012 mm², 1.28 mW)이면서 이득이 큼(+72.1%).
- 구현 현실성을 진지하게 다룸(두 단계 근사, codeword rearrangement로 LDPC 기존 barrel shifter 재활용).

**한계**
- RP 정확도가 RBER이 보정 능력에 가까울 때 50.3%로 급락(저자는 해당 구간 비중<2%로 정당화하나 워크로드/공정에 따라 달라질 수 있음).
- die마다 RP/RVS를 추가해야 하는 NAND vendor의 die 변경이 필요 — 채택 장벽.
- codeword rearrangement는 controller가 쓰기 시 layout 회전, 읽기 시 복원을 해야 하므로 controller-die 인터페이스 규약 변경 필요.
- 평가가 전부 시뮬레이션(MQSim-E)이며 실제 RiF-enabled chip 시제품 측정은 없음.

**열린 질문**
- QLC NAND나 더 강한/다른 ECC(예: 더 긴 codeword)에서도 syndrome weight 상관과 ρ_s 단일 threshold가 유효한가?
- RP 오예측(correctable을 uncorrectable로)으로 인한 불필요한 die 내 Swift-Read의 누적 비용은 write-heavy/혼합 워크로드에서 어떤가?
- retention/온도 변화에 따라 ρ_s를 동적으로 재보정해야 하는가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 기존 read-retry 방식의 근본적 비효율은 무엇인가?
> 답: reactive하다는 점. retry 필요 여부를 off-chip ECC 디코딩이 실패를 선언한 후에야 알 수 있어, (1) ECC-decodable한 첫 page조차 일단 channel로 전송돼야 하고, (2) uncorrectable page를 channel로 내보낸 뒤에야 결정되며, (3) 큰 ECC 디코딩 latency 변동이 후속 transfer를 막아 effective channel bandwidth를 낭비한다 (p.2).

> [!question]- Q2. RiF는 어떻게 이 문제를 해결하는가?
> 답: near-data processing을 retry에 적용해, flash die 내부 ODEAR engine이 die 안에서 (1) page의 correctability를 예측(RP)하고 (2) uncorrectable이면 die 안에서 V_REF를 조정해 재독(RVS)한다. 따라서 uncorrectable raw page를 channel로 내보내지 않아 대역폭 낭비를 막는다 (p.5).

> [!question]- Q3. RP는 LDPC 디코딩 없이 어떻게 correctability를 예측하나?
> 답: RBER과 syndrome weight(Σs_k)가 양의 상관(거의 1:1)을 보인다는 점을 이용한다. syndrome weight만 계산(XOR로 syndrome 구한 뒤 1의 개수 합산)해 threshold ρ_s=3830(보정 능력 RBER 0.0085에 대응)과 비교, 초과하면 uncorrectable로 판정한다. 평균 99.1% 정확도 (p.6–7).

> [!question]- Q4. die 안에서 syndrome을 계산하기 어려운 이유와 해결책은?
> 답: page buffer에서 데이터를 다시 읽어야 해 read latency가 ~25% 늘고, QC-LDPC의 shifted identity 때문에 syndrome 비트가 불규칙 분포해 bitwise 연산이 복잡하다. 해결: ① 근사 syndrome 계산(4-KiB chunk 하나만, 첫 t개 syndrome만 — syndrome pruning), ② codeword rearrangement로 쓰기 시 segment를 회전시켜 H를 identity로 단순화. 근사 적용 후 정확도 98.7% (p.8–10).

> [!question]- Q5. RVS의 역할과 기존 Swift-Read 사용과의 차이는?
> 답: RP가 uncorrectable로 예측하면 RVS가 die 내부에서 Swift-Read command를 발행해 near-optimal V_REF를 찾고 재독한다. 기존엔 ECC 디코딩 실패 후 controller가 Swift-Read를 발행했지만, RiF는 RVS가 controller 도움 없이 on-die로 내부 발행한다. 재독 page는 RP를 다시 거치지 않고 곧장 off-chip ECC로 전송된다 (p.7).

> [!question]- Q6. 핵심 성능 수치는?
> 답: 2K P/E cycle에서 RiFSSD가 SENC/SWR/SWR+ 대비 평균 I/O 대역폭 +72.1%/+61.2%/+50.0%, 이상적 SSD_zero에 1.8% 이내로 근접. Ali_124의 99.99th tail latency를 SENC 대비 91.8% 감소 (p.13).

> [!question]- Q7. RP 모듈의 area/power overhead는?
> 답: 130 nm·100 MHz에서 0.012 mm², 1.28 mW. 현대 flash die(~101 mm²) 대비 negligible. retry 시 unrecoverable page 전송 에너지 ~907 nJ를 절약하면서 예측에 ~3.2 nJ만 추가 (p.13).

> [!question]- Q8. retry가 "흔한 경우"라는 주장의 근거는?
> 답: 160개 실제 3D TLC chip 특성화에서 0/200/500 P/E cycle 기준 17/14/10일 retention만 지나도 RBER이 보정 능력을 넘었다. 상용 SSD는 4주 이상(JEDEC 기준 enterprise 3개월) retention을 요구하므로 대부분의 read에서 retry가 발생할 수 있다(Fig. 4, p.4).

## 🔗 Connections
[[Reliability]] · [[HPCA]] · [[2024]]

## References worth following
- [23] Qiao Li et al., "Shaving Retries with Sentinels for Fast Read over High-Density 3D Flash," MICRO 2020 — 주 비교 대상(Sentinel), spare cell 기반 near-optimal V_REF 예측.
- [32] Wanik Cho et al., "...Swift-Read...," ISSCC 2022 — RVS가 내부 발행하는 Swift-Read command의 원천 기술.
- [9] Yu Cai et al., "Error Characterization, Mitigation, and Recovery in Flash-Memory-Based SSDs," Proceedings of the IEEE 2017 — 3D NAND 오류 특성과 RBER/V_TH 배경의 표준 레퍼런스.
- [80] Jisung Park et al., "Flash-Cosmos: In-Flash Bulk Bitwise Operations...," MICRO 2022 — in-flash processing의 대표 연구이자 RiF의 사상적 배경.
- [49] Arash Tavakkol et al., "MQSim: A Framework for Enabling Realistic Studies of Modern Multi-Queue SSD Devices," FAST 2018 — 평가에 쓴 시뮬레이터(MQSim-E의 기반).

## Personal annotations
<본인 메모 영역>
