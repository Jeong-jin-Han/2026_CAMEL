---
title: "Midas Touch: Invalid-Data Assisted Reliability and Performance Boost for 3d High-Density Flash"
aliases: [Midas Touch]
description: "3D QLC의 invalid programming 낭비를 RP·NP·ADA로 전환해 성능·신뢰성·수명을 동시에 끌어올린 연구"
venue: HPCA
year: 2024
tier: deep
status: done
tags:
  - paper
  - cluster/reliability
  - topic/3d-nand
  - topic/reliability
  - venue/hpca
  - year/2024
---

# Midas Touch: Invalid-Data Assisted Reliability and Performance Boost for 3D High-Density Flash

> **HPCA 2024** · `cluster/reliability` · Source: [Midas Touch - Invalid-Data Assisted Reliability and Performance Boost for 3d High-Density Flash.pdf](Midas Touch - Invalid-Data Assisted Reliability and Performance Boost for 3d High-Density Flash.pdf)

저자: Qiao Li, Hongyang Dang, Zheng Wan, Congming Gao(교신저자), Min Ye, Jie Zhang, Tei-Wei Kuo, Chun Jason Xue — Xiamen University, City University of Hong Kong, Peking University, National Taiwan University, MBZUAI

## TL;DR
3D QLC NAND는 two-step programming(coarse → fine) 사이의 긴 시간 간격 때문에 일부 page가 두 단계 사이에 무효화되는 **invalid programming** 현상을 겪는다. 이 논문은 이 "낭비"를 자산으로 바꿔서, (1) valid page가 1개뿐인 partially-invalid WL은 두 번째 단계를 건너뛰고 다른 WL로 옮겨 재프로그래밍하는 **RP(Re-Programming)**, (2) invalid page에 해당하는 voltage state를 프로그래밍하지 않아 valid page의 noise margin을 넓혀 신뢰성을 높이는 **NP(Not-Programming)**, (3) invalid programming을 유발할 page를 SLC로 동적 배치하는 **ADA(Adaptive Data Allocation)**를 제안한다. 실제 3D QLC 칩 + SimpleSSD 시뮬레이터 평가에서 RP+ADA로 프로그래밍 실행시간 평균 13.51% 단축, NP로 RBER 32.8% 감소 → refresh 오버헤드 12% 감소·수명 30% 연장, 에너지 평균 4.8% 절감.

## 문제 & 동기
3D QLC NAND는 4-bit/cell의 좁은 voltage 분포 때문에 page 단위 프로그래밍이 불가능하고, 한 WL의 4개 page를 동시에 다루는 **two-step (coarse/fine) programming**을 쓴다. 두 단계는 interference·early charge loss를 줄이려 느슨하게 결합되어(loose coupling) 사이에 다른 WL들의 프로그래밍이 끼어들고, 그동안 buffer에 있던 page가 후속 write로 무효화될 수 있다. 그런데도 second-step의 fine programming은 그 invalid page까지 그대로 수행되어 시간·에너지를 낭비한다. 저자들은 이 무효 page를 신뢰성과 성능 향상에 활용한다("turn this waste into gold").

> [!quote]- 📄 원문 표현 (paper)
> "The programming sequence could lead to the phenomenon that some data become invalid between the two programming steps. This is a new issue, called *invalid programming*, in 3D QLC flash introduced by the new programming strategy, which has not been addressed in the literature. This work exploits this phenomenon for performance and reliability improvement in 3D QLC flash memories." (p.1, Abstract/Intro)
>
> "During this period, the four pages in the buffer may be invalidated by subsequent write requests... If the second-step programming still performs normal programming operations on it, it will be a waste of time and energy. This work focuses on turning this waste into gold." (p.3, §III & Fig.2 discussion)

## 핵심 통찰

> [!note]- ① Invalid programming은 흔하고 반복적이다 — 따라서 활용 가치가 크다
> Invalid ratio는 parallelism(open block 수)과 buffer size가 커질수록 증가한다. 가장 낮은 parallelism에서도 ratio가 15.4%에 달한다(write-back 100 page 중 15개가 invalid programming 유발, p.4 Fig.4). 게다가 특정 LPN은 invalid programming을 반복적으로 겪으며, 일부는 100번 넘게 겪는다(p.4 Fig.5). 이 반복성 때문에 SLC 재배치(ADA)와 같은 예측적 처리가 통한다.

> [!note]- ② Invalid page는 "읽히지 않으므로" 신뢰성 보장이 불필요하다
> Invalid page는 접근되지 않으므로 그 voltage state의 신뢰성을 보장할 필요가 없다. 그래서 invalid state에 해당하는 noise margin을 줄이고 그만큼 valid page state의 margin을 넓혀줄 수 있다. 이것이 NP scheme의 본질로, 같은 WL의 valid page RBER을 직접 낮춘다.

> [!note]- ③ valid page 개수(k)에 따라 최적 전략이 갈린다
> partially-invalid WL에 valid page가 k개 있을 때, 일반 평균 프로그래밍 시간은 (T1+T2)/k로 page당 비용이 커진다. RP로 다른 WL에 옮기면 비용은 (2·T1+T2)/4. 평가 칩에서 T2 ≈ 2·T1이므로, k=1일 때만 RP가 이득(즉 valid page threshold=1), k=2,3이면 NP로 신뢰성/에너지 이득을 취하는 게 낫다(p.5 §IV-B Benefits analysis, p.9 Fig.13).

> [!note]- ④ 하드웨어 변경 없이 coding(데이터 매핑)만으로 NP 구현
> program engine을 건드리지 않고, second-step 전에 voltage state representation을 re-code한다. 두 인접 state 사이에서 invalid가 되면 비트가 다른 쪽 state로 remap되어 fine programming 자체가 필요 없어진다(예: LSB invalid 시 S4의 0000을 S3의 1000으로 매핑, p.6 Fig.11). 하드웨어 비용 0.

## 설계 / 메커니즘
전체 구조(p.4 Fig.6)는 세 모듈로 구성된다: Re-programming module, Not-programming module, Adaptive allocator. second-step 시점에 WL의 valid page 수를 검사해 threshold보다 작으면 RP, 아니면 NP를 적용한다.

> [!abstract]- A. Re-Programming (RP) — second-step 건너뛰고 다른 WL로 이전
> partially-invalid WL의 valid page를 다른 incoming data와 묶어 다른 free WL에 first-step으로 프로그래밍한다. 옮긴 page는 새 위치에서 자기 second-step을 받으므로, 원래 WL의 느린 second-step(fine programming)은 통째로 생략된다.
> - **Program stage update**: PST(program stage table)에서 해당 page의 stage를 'coarse-programmed' → 'waiting'으로 되돌려 재프로그래밍 대상으로 만든다(p.5 Fig.7).
> - **Crash-consistent remap**(p.5 Fig.8): page validity bitmap에서 옛 PPN의 valid bit를 0으로, mapping table을 새 PPN으로 갱신. 갱신 순서를 지켜 crash 시에도 원본 page가 flash에 남아 P2L(OOB의 physical-to-logical) 정보로 복구 가능.
> - threshold를 너무 키우면 free WL 소모 → GC 폭증·수명 저하. 평가 칩에서 threshold=1이 최적(valid page 1개일 때만 RP).

> [!abstract]- B. Not-Programming (NP) — invalid state는 프로그래밍하지 않아 valid margin 확대
> invalid page의 voltage state에 대한 fine programming을 억제한다(p.6 Fig.9). 예: Si가 invalid면 Si-1과의 구분이 불필요하므로 Si를 finely program하지 않고 거친 위치에 둔다. 그러면 Si+1과의 noise margin이 크게 넓어져 read voltage Vi+1 관련 page의 신뢰성이 향상된다.
> - **Coding 기반 구현**(p.6 Fig.11): 하드웨어 변경 없이, second-step 전에 비트를 re-code. 한 invalid page는 인접 두 state의 데이터 비트를 같게 만들어 그 사이 read voltage가 불필요해지게 한다.
> - 96-layer(Flash A)·128-layer(Flash B) 실제 칩으로 분포 검증(p.6–7 Fig.10): LSB invalid 시 V4,V10,V12,V14 read가 불필요 → 해당 인접 state가 겹쳐도 valid page 신뢰성에 영향 없음.

> [!abstract]- C. Adaptive Data Allocation (ADA) — invalid 유발 page를 SLC로
> invalid programming의 근본 원인은 두 단계 사이의 시간 간격(programming sequence 제약상 제거 불가)이다. ADA는 invalid를 자주 유발하는 cold data를 SLC region에 배치해 QLC의 invalid programming을 줄인다(p.7 Fig.12, Algorithm 1).
> - **IPL(Invalid Programming List)**: LRU 리스트로 invalid programming을 유발한 LPN을 기록. hot data는 SLC로, cold data 중 IPL에 있으면 SLC로(그리고 LRU tail로 이동), 아니면 QLC로.
> - **IPL 길이 동적 조절**: SLC write ratio(SLC write/총 write-back)와 invalid ratio를 buffer write-back을 logical clock 삼아 모니터링. SLC write ratio 상한 초과 시 IPL 길이 절반, 하한 미만 시 2배. invalid ratio가 높으면 길이 2배. (파라미터 t1=0.15, t2=0.70, t3=0.07, MinLen=32, MaxLen=2048; Table III, p.9)

> [!abstract]- D. Semi-Block Refresh — NP WL은 더 오래 refresh
> 한 block 안에 NP-programmed WL(RBER 낮음)과 normally-programmed WL(보통)이 공존하므로 둘을 다른 주기로 refresh한다. normal data는 일반 주기에, NP data는 더 긴 주기에 migration한 뒤 block 전체 erase(p.8 §IV onwards / p.8 Fig.22 area).

## 평가

> [!example]- 실험 셋업 & 성능
> - 실제 3D QLC 칩 2종: Flash A(96-layer, block당 1536 page, WL당 4 page, 16KB), Flash B(128-layer, 6 WL/layer, TLC WL 포함, 3048 page/block, 16KB). NP는 실제 칩에서 검증, 나머지는 SimpleSSD(SLC-QLC hybrid, 1920GB, SLC 15%, buffer 16MB, open block 8개)로 평가. (p.8 §V-A)
> - 파라미터(Table I, p.8): Erase 6.0ms, SLC program 0.1ms, QLC 1st-step 1.4ms, QLC 2nd-step 2.8ms (2nd가 약 2배), GC threshold 0.50.
> - 워크로드: Microsoft Cambridge Research 9종(mds_0, stg_0, src2_2, usr_0, web_1, prn_1, proj_0, prxy_0, wdev_0; Table II, p.9).
> - **RP+ADA: 프로그래밍 실행시간 평균 13.51% 단축**(p.10 Fig.19). RP 단독 reprogram operation 0.74%, RP+ADA 0.62%로 오버헤드 미미(p.10).
> - valid page threshold=1이 최적: 2~3으로 키우면 reprogram ratio 급증·실행시간 baseline보다 악화(p.9 Fig.13–14).

> [!example]- 신뢰성 (NP)
> - **NP-programmed data의 평균 RBER 32.8% 감소** → 전체 신뢰성 향상, 99th RBER 9.35% 감소(p.2 Abstract, §I).
> - LSB invalid 시 CSB/MSB/TSB RBER 각 38.50%/12.09%/21.64% 감소; 1 page invalid 최선의 경우 4 page에 대해 40.33%/38.50%/66.94%/32.84% 감소(p.11 Fig.20).
> - 2 page invalid 시 감소폭 더 큼(invalid page가 많을수록 valid page 신뢰성 개선 증가, p.11 Fig.20b–21).
> - **Semi-Block Refresh: refresh 오버헤드(block refresh count) 11.89% 감소, 수명 30.43% 연장**(p.8 Fig.22) → 본문 요약상 "12% refreshing 오버헤드 감소, 30% 수명 연장"(p.2).

> [!example]- 에너지 & invalid ratio
> - **NP 에너지 평균 3.5% 절감(최대 6.6%), NP+ADA 평균 4.8% 절감(최대 9.1%)**(p.12 Fig.23).
> - ESR(energy saving ratio): 1 page invalid 평균 11.7%, 2 page invalid 평균 23.4%(Table IV, p.12).
> - ADA invalid ratio: baseline 대비 13.9%~46.5% 감소(Ideal에는 못 미치나 큰 폭, p.9 Fig.15). ADA는 SLC write ratio를 낮게 유지(Infinite-IPL/Ideal은 SLC write ratio가 최대 85.2%까지 치솟아 GC 유발, p.10 Fig.16).
> - 공간 오버헤드 거의 없음(p.10 Fig.18).

## 섹션 노트
- **§I Introduction**: invalid programming 정의, 3가지 기여(RP, NP, ADA) 제시.
- **§II Background**: 3D QLC flash(4-bit/16-state, gray code), two-step(coarse/fine) programming, SLC-QLC hybrid SSD 구조(PST, page validity table, mapping table 포함; p.3 Fig.3).
- **§III Motivation**: invalid ratio가 parallelism·buffer size에 비례(p.4 Fig.4), LPN별 invalid 반복성(p.4 Fig.5).
- **§IV Invalid-Data Assisted Programming**: A.Overview(Fig.6), B.RP(Fig.7,8), C.NP(Fig.9,10,11), D.ADA(Fig.12, Algorithm 1).
- **§V Evaluation**: 셋업(Table I,II,III) + 결과(valid page threshold, ADA invalid ratio, RP 성능, NP 신뢰성, NP 에너지).
- **§VI Related Works**: flash programming(Choi merge states[7], SLC re-programming[3,6], Gao 3D TLC re-program[11]), high-density coding(Lv gray code[32], WOM code), hybrid-flash management(Yang TLC→SLC[50], Yoo RL SLC cache[51]).
- **§VII Conclusion**: PLC(5-bit/cell)는 invalid programming이 더 심해질 것 → high-efficiency programming 설계 공간이 더 넓다.

## 핵심 용어
- **Invalid programming**: 3D QLC의 two-step 프로그래밍 사이 긴 간격 동안 buffer의 page가 무효화되어, 두 번째 단계가 무효 데이터를 그대로 프로그래밍하는 낭비 현상.
- **Two-step programming (coarse/fine)**: 좁은 voltage 분포의 QLC에서 WL의 4 page를 거친(coarse) 위치로 먼저, 이후(다른 WL 끼어든 뒤) 미세(fine) 위치로 프로그래밍. fine 단계가 약 2배 느림.
- **Loose coupling**: interference/early charge loss를 줄이려 두 단계를 즉시 이어붙이지 않고 다른 WL 작업을 끼워넣는 결합.
- **Partially-invalid WL**: 4 page 중 일부가 second-step 전에 invalid가 된 WL.
- **RP (Re-Programming)**: valid page가 threshold 미만인 WL의 valid page를 다른 WL로 옮겨 재프로그래밍해 느린 second-step을 생략하는 기법.
- **NP (Not-Programming)**: invalid state를 fine programming하지 않아 valid page의 noise margin을 넓혀 RBER을 낮추는 coding 기반 기법.
- **ADA (Adaptive Data Allocation)**: invalid programming 유발 page를 SLC로 동적 배치해 QLC invalid programming을 줄이는 전략.
- **IPL (Invalid Programming List)**: invalid programming 유발 LPN을 기록하는 LRU 리스트, 길이를 SLC write ratio/invalid ratio로 동적 조절.
- **valid page threshold**: RP 적용 여부 기준이 되는 valid page 개수(평가 칩 최적값=1).
- **Semi-Block Refresh**: RBER이 낮은 NP WL을 normal WL보다 긴 주기로 refresh해 refresh 비용을 줄이는 기법.
- **RBER (Raw Bit Error Rate)**: ECC 적용 전 원시 비트 오류율, 신뢰성 지표.

## 강점 · 한계 · 열린 질문
- **강점**: 기존 문헌이 다루지 않은 새 문제(invalid programming)를 정의하고, 같은 현상으로 성능·신뢰성·수명·에너지를 동시에 개선. NP는 하드웨어 변경 없이 coding만으로 구현. 실제 96/128-layer QLC 칩으로 분포·RBER 검증해 신뢰성이 높음.
- **한계**: 성능/공간 평가는 SimpleSSD 시뮬레이터 의존(end-to-end 실측 아님). RP threshold·IPL 파라미터가 칩/워크로드에 민감해 튜닝 필요. ADA는 invalid 유발 page를 SLC로 옮겨도 SLC→QLC migration 시 다시 invalid programming 가능(완전 제거 불가).
- **열린 질문**: PLC(5-bit) 등 더 고밀도 셀에서 invalid programming 비율과 NP 이득은? RP가 GC/wear leveling과 상호작용하며 장기 수명에 미치는 영향은? NP의 RBER 감소가 retention 시간이 길어진 노화 후반에도 유지되는지?

## ❓ Q&A (자가 점검)

> [!question]- Q1. invalid programming은 왜 3D QLC에서 새로 생기는 문제인가?
> 좁은 voltage 분포 때문에 QLC는 page 단위가 아닌 WL 단위 two-step(coarse/fine) programming을 쓰고, 두 단계가 loose coupling으로 떨어져 있다. 그 사이 간격 동안 buffer의 page가 후속 write로 무효화되는데, second-step이 그 무효 데이터까지 프로그래밍해 시간·에너지를 낭비한다. planar/page 단위 프로그래밍에는 없던 현상이다.

> [!question]- Q2. RP는 왜 valid page가 1개일 때만 이득인가?
> partially-invalid WL의 valid page당 평균 시간은 (T1+T2)/k(k=valid 수). RP로 옮기면 page당 (2·T1+T2)/4. 평가 칩에서 T2≈2·T1이므로 k=1이면 RP가 더 싸지만, k=2,3이면 (T1+T2)/k가 더 작아 second-step을 그냥 수행(대신 NP 적용)하는 게 낫다.

> [!question]- Q3. NP가 valid page의 신뢰성을 높이는 원리는?
> invalid page는 읽히지 않으므로 그 voltage state의 신뢰성을 보장할 필요가 없다. invalid state를 fine programming하지 않으면 인접 state와 겹쳐도 무방하고, 그 대신 valid page state와의 noise margin이 넓어져 read voltage 부근 오류가 줄어 RBER이 낮아진다.

> [!question]- Q4. NP는 하드웨어 변경 없이 어떻게 구현하나?
> program engine을 고치는 대신 second-step 전에 voltage state representation을 re-code한다. invalid가 된 인접 두 state의 데이터 비트를 같게 매핑하면 그 사이 read voltage가 불필요해지고 해당 state로의 fine programming도 생략된다(예: LSB invalid 시 S4의 0000을 S3의 1000으로 매핑).

> [!question]- Q5. ADA는 invalid programming을 완전히 없앨 수 있나?
> 없앨 수 없다. 근본 원인인 두 programming step 사이 시간 간격은 sequence 제약상 제거 불가능하다. invalid 유발 page를 SLC로 옮겨도 이후 SLC→QLC migration 시 다시 간격이 생겨 invalid programming이 발생할 수 있다. 그래서 ADA는 "완전 제거"가 아닌 "완화"(baseline 대비 13.9~46.5% 감소)를 목표로 한다.

> [!question]- Q6. Infinite-IPL/Ideal보다 ADA가 나은 점은?
> Infinite-IPL과 Ideal은 invalid ratio는 더 낮지만 SLC로 너무 많이 몰아 SLC write ratio가 최대 85.2%까지 치솟아 GC를 유발하고 성능을 해친다. ADA는 IPL 길이를 SLC write ratio 상·하한으로 동적 조절해 SLC write를 낮게 유지하면서 invalid ratio를 줄인다.

> [!question]- Q7. Semi-Block Refresh가 가능한 이유는?
> 한 block 안에 NP-programmed WL(RBER 낮음)과 normally-programmed WL이 섞여 있다. NP WL은 오류 축적이 느려 더 긴 retention을 견디므로, normal data를 먼저 일반 주기에 migration하고 NP data는 더 긴 주기에 migration한 뒤 block을 erase한다. 결과적으로 refresh count 11.89% 감소, 수명 30.43% 연장.

> [!question]- Q8. 이 기법들의 전체 효과를 한 문장으로?
> 같은 invalid programming 현상을 RP(성능), NP(신뢰성·에너지·수명), ADA(invalid 완화)로 활용해, 실행시간 13.51%↓, RBER 32.8%↓(→수명 30%↑·refresh 12%↓), 에너지 4.8%↓를 거의 추가 오버헤드 없이 달성한다.

## 🔗 Connections
[[Reliability]] · [[HPCA]] · [[2024]]

## References worth following
- **[32] Y. Lv et al., "Mgc: Multiple-gray-code for 3d nand flash based high-density ssds," HPCA 2023** — gray code별 성능·신뢰성 차이를 활용한 직접적 선행연구.
- **[8] H. Dang, Y. Yao, Z. Wan, Q. Li, "A study of invalid programming in 3d qlc nand flash memories," HotStorage 2023** — invalid programming 현상을 다룬 저자들의 선행 측정 연구.
- **[30] L. Long et al., "Adar: Application-specific data allocation and reprogramming optimization for 3d qlc flash," TCAD 2022** — 3D QLC re-programming/data allocation 비교 대상.
- **[7] W. Choi, M. Jung, M. Kandemir, "Invalid data-aware coding...," MICRO 2018** — invalid data 인지 coding의 원조(TLC 기반, QLC에서는 state overlap 문제).
- **[51] S. Yoo, D. Shin, "Reinforcement Learning-Based SLC cache technique...," HotStorage 2020** — hybrid SSD의 SLC cache 파라미터 최적화 관련 후속 비교.
- **[35][36] V. Mohan et al., flash power modeling (TCAD 2013 / DATE 2010)** — NP 에너지 절감 모델(Epgm, ESR) 도출의 근거.

## Personal annotations
<본인 메모 영역>
