---
title: "ColdCode: Cold Data Encoding for Enhanced Reliability and Lifetime in 3D NAND Flash"
aliases: [ColdCode]
description: "3D NAND cold data에 대해 randomizer 대신 entropy-aware coding(skewed/reversed Huffman)으로 저-RBER 데이터 패턴을 만들어 RBER을 42%/30% 낮추고 수명을 2.97×/1.7× 연장하는 EuroSys 2026 연구."
venue: Eurosys
year: 2026
tier: deep
status: done
tags:
  - paper
  - cluster/reliability
  - topic/3d-nand
  - topic/ecc
  - topic/lifetime
  - venue/eurosys
  - year/2026
---

# ColdCode: Cold Data Encoding for Enhanced Reliability and Lifetime in 3D NAND Flash

> **EUROSYS 2026** · `cluster/reliability` · Source: [ColdCode - Cold Data Encoding for Enhanced Reliability and Lifetime in 3D NAND Flash.pdf](<ColdCode - Cold Data Encoding for Enhanced Reliability and Lifetime in 3D NAND Flash.pdf>)

저자: Qiao Li\*, Shangyu Wu\* (MBZUAI), Zheng Wan (Xiamen Univ.), Yufei Cui (McGill), Jie Zhang (Peking Univ., 교신저자), Chun Jason Xue (MBZUAI). (\* 공동 1저자)

## TL;DR
3D NAND flash는 보통 데이터를 `randomizer`로 균등 분포시켜 최악의 RBER 패턴을 피하지만, 이 논문은 8개 칩 측정을 통해 randomization이 cold data에 대해 **더 낮은 RBER을 얻을 기회를 막는다**는 것을 보인다. ColdCode는 file system이 전달하는 1-bit cold tag를 받아 randomizer를 대체하여, 데이터 엔트로피에 따라 **skewed coding**(저엔트로피) 또는 **reversed Huffman coding**(고엔트로피)을 적용해 저-RBER 패턴(중간 전압 상태 S3/S4 중심)으로 인코딩한다. 실측 170+ layer 칩에서 평균 RBER을 각각 42%/30% 낮추고 수명을 2.97×/1.7× 연장한다.

## 문제 & 동기
Cold storage는 거의 접근되지 않는 데이터를 저장하며 현대 데이터센터의 다수를 차지하는데, NAND flash의 data randomization은 이 영역을 잘 다루지 못한다. Flash vendor들은 최악의 데이터 패턴(특정 cell들을 retention에 취약한 high voltage state로 과도하게 프로그래밍)을 피하기 위해 randomizer를 보편적으로 적용한다. 예를 들어 TLC에서 3-bit 데이터를 8개 voltage state에 균등 분포시킨다. 그러나 randomization은 **모든 패턴을 균등화**함으로써, cold data에 대해 더 우수한 저-RBER 패턴을 탐색할 기회를 차단한다. 기존 planar flash의 신뢰성 향상 기법(high voltage state 제거, low voltage state로의 매핑 등)은 high-density 3D NAND에서 잘 동작하지 않으며 오히려 RBER을 악화시키기도 한다.

> [!quote]- 📄 원문 표현 (paper)
> "Cold storage, which stores rarely accessed data, dominates modern data centers but is poorly served by NAND flash's data randomization." (p.1, Abstract)
>
> "However, this paper demonstrates that data randomization rules out the opportunity to explore data patterns with very low RBERs, through a comprehensive analysis on data randomization in 3D NAND flash chips (across 8 models)." (p.1, Abstract)
>
> "However, this paper shows that reducing high-voltage states or increasing low-voltage states does not work well on high-density 3D NAND flash memories." (p.2)

## 핵심 통찰

> [!note]- 통찰 1: Randomization은 최악을 피하지만 최선도 막는다
> Randomizer는 데이터를 8개 voltage state에 균등 분포시켜 극단적으로 높은 RBER을 막는다. 하지만 8개 측정 칩 모두에서 일부 데이터 패턴은 Rand보다 RBER이 더 높고, 일부는 **더 낮다**(Observation 1, p.4). 즉 randomization은 cold data에 대해 더 좋은(저-RBER) 패턴을 활용할 기회를 봉쇄한다.

> [!note]- 통찰 2: 저전압 상태로 매핑하는 통념이 3D high-density에서 깨진다
> Planar flash에서는 high voltage state가 retention 오류의 주범이라 low voltage state로 매핑하면 신뢰성이 좋아진다고 알려져 있었다. 그러나 3D high-density에서는 RBER이 낮은 좋은 패턴이 low voltage state가 아니라 **medium voltage state**가 지배하는 패턴이며(Observation 1, p.4), low voltage 패턴(예: PS0)도 high voltage state(S7)의 left-shift를 유발해 RBER이 높을 수 있다(Observation 4, p.5).

> [!note]- 통찰 3: 연속 cell 결합이 skew를 드러낸다
> 1-cell 단위로 보면 각 WL의 cell은 8개 state에 거의 균등 분포(near-linear CDF)라 단순 매핑으로 skewed 패턴을 만들기 어렵다. 그러나 2-cell/4-cell로 연속 cell을 묶으면 분포가 치우쳐서(L/W/U-shaped workload에서 top 12.5% 결합 문자가 46.2%/48.3% 차지) skew를 활용할 여지가 생긴다(§4.2, Fig.7, p.8).

> [!note]- 통찰 4: Cold 여부에 따라 coding을 선택하는 협업 프레임워크
> File system이 파일의 hot/cold를 1-bit metadata로 SSD controller에 전달(NVMe append)하고, controller가 cold data에 한해 데이터 엔트로피를 추정하여 저엔트로피→skewed, 고엔트로피→reversed Huffman을 선택한다. Hot data는 기존 default(randomizer) 모드로 처리되어 영향이 없다(§4.1, Fig.5, p.7).

## 설계 / 메커니즘

> [!abstract]- ColdCode 전체 구조와 두 가지 coding
> **목표 패턴**: 측정에서 RBER이 가장 낮은 패턴은 S3(000, '0'으로 표기)와 S4(010, '2'로 표기)가 지배하는 **DS(double-state) 패턴**(PS34 등). ColdCode는 이 두 medium voltage state로 데이터를 몰아넣는 것을 목표로 한다.
>
> **흐름(Fig.5, p.7)**: Write buffer → controller의 ARM core가 엔트로피 계산 → 저엔트로피면 Skewed Coding, 고엔트로피면 Huffman Coding 적용 → mapping table/ECC 거쳐 flash array에 기록.
>
> **(1) Skewed Coding (§4.3, p.8)**: 저엔트로피(원본 텍스트/이미지 등) 대상. 연속 2-cell 또는 4-cell을 하나의 entity로 보고, 자주 나오는 결합 문자(combined character)를 선호 state(S3='0', S4='2')의 결합으로 매핑한다. 예: 2-cell에서 빈도 최고인 '34'를 '20'으로 매핑(Fig.8, p.8). Mapping table은 2-cell의 경우 64개 결합×6bit/2 = 24 byte로 매우 작아 OOB 영역에 저장. 4-cell은 4096 결합 중 상위 32개만 매핑(48 byte, WL 크기의 0.1%). Mapping table은 WL의 첫 write 요청에서만 계산하고 이후 재사용(파일 단위 동일 분포 가정).
>
> **(2) Reversed Huffman Coding (§4.4, p.9)**: 고엔트로피(압축/암호화 파일 등, 엔트로피≈8로 거의 균등) 대상. 균등 분포라 skew가 없으므로, **선호 문자가 없거나 적은 결합을 선호 문자(S3/S4)를 덧붙인 새 결합으로 인코딩**한다. 즉 균일 길이(2) 문자를 가변 길이로 바꿔 '0'/'2' 빈도를 높인다(Fig.9, p.9). 예: '77'→'002', '00'→'000'. 공간(coded data length)과 신뢰성 간 trade-off이며, OP(over-provision) overhead를 키운다(2-cell ~1.56%, 4-cell 매핑은 41.96% 선호문자에 9.86% overhead, 46.46%에 13.09% overhead — Table 1, p.9).
>
> **엔트로피 추정**: controller 내 ARM core에서 integer 연산 + 1KB 미만 lookup table로 sub-microsecond 수준 계산(§4.5.1, p.9~10). 원본 plain 파일은 WL마다 distinct한 낮은 엔트로피, 압축/암호화는 ≈8(Fig.10).
>
> **수명 모델(Eq.1, p.10)**: lifetime = PE×(1+OP) / (365×DWPD×WA×(1+OC)). RBER 감소 → refresh 빈도 감소 → WA 감소로 수명 증가. Skewed coding의 OC=0(다만 codeword당 2B parity loss로 ECC capacity <3% 감소), reversed Huffman은 OC를 10%/13.2%로 설정.
>
> **관리(§4.5.1, p.10)**: reversed Huffman은 constant-length 코딩이 한 WL을 넘을 수 있어, page translation table에 물리 block 내 순서를 기록해 두 WL에 걸친 데이터도 정확히 복원.

## 평가

> [!success]- 실험 설정 및 주요 수치
> **플랫폼**: YEESTOR 9081/9086 SSD 평가 플랫폼. Characterization은 4개 vendor 8개 칩(TLC 5, QLC 3), 정밀 분석은 Micron 176-layer TLC. P/E 1000 cycle 후 100°C에서 2~4시간 baking으로 retention 모사, 300 block read로 read disturbance 모사. Workload: W-shaped Log, L-shaped Sound/Criteo, U-shaped Amazon/NeuralPS, E-shaped Wikipedia + Compressed/Encrypted.
>
> **RBER 감소(Fig.13, p.12)**: Origin 대비 MSB page bit error를 Skewed-2C 30%, Skewed-4C 42% 감소. Reversed Huffman은 10%/13.2% overhead로 MSB page error를 각각 21%/30% 감소(Abstract의 평균 42%/30%와 일치).
>
> **선호 문자 비율(Fig.12, p.11)**: randomization은 '0'+'2' 합 비율 약 25%. Skewed-2C/4C는 54%/58%로 증가(수동 구성 최저-RBER 패턴 PS34의 62.5%에 근접). 균등 분포(Wikipedia/Compressed/Encrypted)는 reversed Huffman 후 42%/47%.
>
> **수명 연장(Fig.16, p.13)**: Rand 대비 skewed coding 1.42×~2.97×, reversed Huffman 1.7×.
>
> **Refresh 주기(§p.13)**: default read voltage에서 Rand MSB는 8.35시간 baking 후 ECC capacity 도달(40°C 환산 2.4개월). Skewed-4C는 5.58개월로 연장(Rand의 2.29배). Skewed-4C의 ECC capacity는 mapping table 점유로 원본보다 약간 낮음.
>
> **Layer 간 변동(Fig.14, p.13)**: Origin에서 WL1256 MSB page는 1784 bit error vs WL6 LSB page 80 bit error로 10배 이상 차이. ColdCode가 WL 간/page type 간 변동을 모두 완화 → 신뢰성 보장 비용 절감.
>
> **읽기 성능(Fig.17, p.13)**: MSRC/TencentCloud/AliCloud trace에서 CodeCold가 Rand와 거의 동일한 read latency. Hot data는 코딩하지 않고, cold data는 대용량 단위(파일 전체 WL)로 접근되므로 추가 read overhead 없음.

## 섹션 노트
- **§1 Introduction (p.1~2)**: randomizer의 보편적 적용, planar 기법(high state 제거/low state 매핑)이 3D에서 실패함을 주장. 기여 4가지: characterization insight, novel encoding framework, real-device validation(42%/30% RBER, 2.97×/1.7× lifetime).
- **§2 Background (p.2~3)**: 3D NAND 구조(WL, charge trap cell), TLC Vth 분포(MSB/CSB/LSB page, Fig.1). P/E·retention·read disturbance에 의한 Vth shift. 기존 data coding(high state 제거, low state 매핑, Huffman 기반 PDLCS/DHC) 소개.
- **§3 Characterization (p.3~6)**: 24개 데이터 패턴(Rand, Single State PSi, Double State PSij). Obs1(Rand보다 높거나 낮은 패턴 존재), Obs2(3 page 간 변동을 일부 패턴이 완화), Obs3(low read voltage right-shift), Obs4(high read voltage left-shift), Obs(medium voltage). PS34/PS45가 가장 균형 잡힌 저-RBER.
- **§4 ColdCode (p.7~10)**: overview, data characteristics, skewed coding, reversed Huffman coding, procedure & analysis.
- **§5 Evaluation (p.11~13)**: state distribution, 선호문자 비율, 평균 RBER 감소, layer별, baking별 refresh, lifetime, read performance.
- **§6 Conclusion (p.13)**: randomizer가 cold data에 최선이 아님을 보이고 cold data encoding framework 제안.

## 핵심 용어
- **RBER (Raw Bit Error Rate)**: ECC 적용 전 NAND에서 읽은 raw bit의 오류율. 신뢰성/수명의 핵심 지표.
- **Randomizer**: LFSR 기반으로 사용자 데이터를 8개 voltage state에 균등 분포시키는 controller 기능. 최악 패턴 회피용으로 보편 적용.
- **Voltage state (S0~S7)**: TLC cell이 3-bit를 저장하기 위해 갖는 8개 threshold voltage 레벨. S0가 최저(낮은 charge), S7이 최고.
- **Cold/hot tag**: file system이 파일의 접근 빈도를 1-bit metadata로 표시하여 NVMe command로 SSD에 전달.
- **Skewed coding**: 저엔트로피 데이터에서 연속 cell(2/4)을 묶어 빈도 높은 결합을 선호 state(S3='0', S4='2')로 매핑하는 코딩. mapping table overhead 매우 작음.
- **Reversed Huffman coding**: 고엔트로피(균등) 데이터를 가변 길이 인코딩으로 선호 문자 빈도를 높이는 코딩. 공간 overhead(OC)와 신뢰성의 trade-off.
- **DS/SS pattern (Double/Single State)**: 데이터가 2개/1개 voltage state에 주로 몰린 합성 패턴. PS34(S3·S4 지배)가 최저 RBER.
- **Write Amplification (WA)**: 실제 flash 기록량/논리 기록량. Cold data의 retention 오류를 막기 위한 주기적 data refresh가 WA를 키우며 수명을 깎는다. RBER 감소→refresh 감소→WA 감소.
- **OOB (Out-Of-Band) 영역**: page당 사용자 데이터 외 메타(ECC parity, 주소 매핑) 저장 공간(2KB). mapping table(24~48B)을 여기 저장.

## 강점 · 한계 · 열린 질문
- **강점**: 8개 실제 칩의 체계적 characterization으로 "low voltage 매핑이 좋다"는 통념을 3D high-density에서 반박. file system 협업 + 엔트로피 적응형 두 coding으로 저/고엔트로피 cold data 모두 커버. 실측 170+ layer 칩에서 RBER 42%/30%, 수명 2.97×/1.7×, read latency 영향 거의 없음.
- **한계**: skewed coding은 파일 단위로 분포가 일정하다고 가정(첫 write에서 mapping 계산 후 재사용) — 파일 내 분포 변동이 크면 효과 저하 가능. reversed Huffman은 10~13.2% 공간 overhead로 OP를 소모. mapping table 점유로 Skewed-4C의 ECC capacity가 약간 낮아짐. cold/hot 분류 정확도(file system 의존)가 잘못되면 hot data가 small-granularity read에서 page→WL latency 증가를 겪을 수 있음.
- **열린 질문**: hot/cold 오분류 시 성능/신뢰성 영향의 정량화는? skewed/Huffman 외 다른 적응형 코딩이나 voltage tuning과의 결합 효과는? QLC(16 state)에서 최적 목표 state와 mapping table overhead는 어떻게 달라지나?

## ❓ Q&A (자가 점검)

> [!question]- Q1. randomizer가 cold data에 부적합한 근본 이유는?
> > randomizer는 데이터를 8개 state에 균등 분포시켜 최악(극단적 high RBER) 패턴을 피하지만, 동시에 Rand보다 RBER이 더 낮은 우수한 패턴을 활용할 기회를 봉쇄한다. cold data는 오래 보존되므로 retention 오류를 줄일 저-RBER 패턴이 특히 중요하다(Obs1, p.4).

> [!question]- Q2. 3D high-density에서 "low voltage state로 매핑하면 신뢰성이 좋다"는 통념이 왜 깨지나?
> > 측정상 최저 RBER 패턴은 low voltage가 아니라 medium voltage state(S3/S4) 지배 패턴이다. low voltage 지배 패턴(PS0)도 인접 high state(S7)의 left-shift를 유발해 RBER이 높을 수 있다(Obs1/Obs4, p.4~5).

> [!question]- Q3. ColdCode가 목표로 하는 데이터 패턴은 무엇이며 왜인가?
> > S3(000, '0')와 S4(010, '2')가 지배하는 double-state 패턴(PS34류). 측정에서 모든 구성 패턴 중 RBER이 가장 낮고 세 page(MSB/CSB/LSB) 간 RBER이 균형 잡혀 있기 때문이다(§3, §4.3, p.4·8).

> [!question]- Q4. skewed coding과 reversed Huffman coding은 각각 언제 쓰이며 핵심 차이는?
> > 저엔트로피 데이터(원본 텍스트/이미지)는 skewed coding으로 빈번한 결합 문자를 선호 state로 매핑(공간 overhead 거의 없음). 고엔트로피(압축/암호화, 엔트로피≈8)는 skew가 없어 reversed Huffman으로 가변 길이 인코딩하여 선호 문자 빈도를 높이되 10~13.2% 공간 overhead를 감수한다(§4.3·4.4, p.8~9).

> [!question]- Q5. 1-cell이 아니라 연속 2-cell/4-cell을 entity로 묶는 이유는?
> > 1-cell 단위로는 각 WL의 cell이 8개 state에 거의 균등 분포라 단순 매핑으로 최대 28.6%만 목표 state로 보낼 수 있다. 연속 cell을 묶으면 분포가 치우쳐(top 12.5% 결합이 46~48% 차지) skew를 활용할 수 있다(§4.2, p.7~8).

> [!question]- Q6. RBER 감소가 어떻게 수명 연장으로 이어지나?
> > RBER이 낮아지면 ECC capacity 도달까지 시간이 길어져 retention 오류 방지용 data refresh 빈도가 줄고, 이는 write amplification(WA)을 낮춘다. lifetime=PE×(1+OP)/(365×DWPD×WA×(1+OC)) 식에서 WA 감소가 수명을 키운다(Eq.1, p.10).

> [!question]- Q7. ColdCode가 정상(hot) 데이터 접근 성능에 미치는 영향은?
> > 거의 없음. hot data는 코딩하지 않고 기존 randomizer 모드로 처리되며, cold data는 대용량(파일 전체 WL) 단위로 접근되어 추가 read overhead가 없다. trace 실험에서 Rand와 거의 동일한 read latency(Fig.17, p.13).

> [!question]- Q8. 두 coding의 공간/ECC overhead는 구체적으로 얼마인가?
> > Skewed coding: mapping table 2-cell 24B, 4-cell 48B로 OOB에 저장(WL의 0.1%), OC=0이지만 codeword당 2B parity loss로 ECC capacity <3% 감소. Reversed Huffman: OC를 10%(선호문자 41.96%) 또는 13.2%(46.46%)로 설정(Table 1, p.9).

## 🔗 Connections
[[Reliability]] · [[EuroSys]] · [[2026]]

## References worth following
- Cai et al., "Error patterns in MLC NAND flash memory" (DATE 2012) [6] — planar flash 오류 패턴의 기반 연구, ColdCode가 반박하는 통념의 출발점.
- Luo et al., "Improving 3D NAND flash memory lifetime by tolerating early retention loss and process variation" (SIGMETRICS 2018) [31] — 3D NAND layer/WL 간 변동과 retention loss 특성.
- Ahrens et al., "Compression of data blocks to improve the reliability of non-volatile flash memories" (IDT 2016) [2] — 압축으로 ECC parity 여유 확보, reversed Huffman의 비교 대상.
- Chin-Hsien Wu et al., "A dynamic Huffman coding method for reliable TLC NAND flash" (TODAES 2021) [45, DHC] — flash용 Huffman 코딩, skewed/Huffman 설계의 직접 선행 연구.
- Yixin Luo et al., "HeatWatch: Improving 3D NAND flash memory device reliability by exploiting self-recovery and temperature awareness" (HPCA 2018) [30] — 3D NAND 신뢰성과 온도/refresh 상호작용.
- Papandreou et al., "Characterization and analysis of bit errors in 3D TLC NAND flash memory" (IRPS 2019) [36] — 3D TLC bit error 특성화, characterization 방법론 참고.

## Personal annotations
<본인 메모 영역>
