---
title: "STRAW: Stress-Aware WL-Based Read Disturbance Management for High-Density NAND Flash Memory"
description: "블록 단위가 아닌 wordline(WL) 단위로 read disturbance를 모니터링·완화해 read reclaim 오버헤드를 대폭 줄이는 STRAW 기법을 제안"
venue: ASPLOS
year: 2026
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/reliability
  - venue/asplos
  - year/2026
  - list/26s-v2
  - topic/read-disturbance
  - topic/nand-flash-reliability
  - topic/read-reclaim
  - topic/pass-through-voltage
---

# STRAW: Stress-Aware WL-Based Read Disturbance Management for High-Density NAND Flash Memory

> **ASPLOS 2026** · cluster/reliability · Source: [STRAW - Stress-Aware WL-Based Read Disturbance Management for High-Density NAND Flash Memory.pdf](<STRAW - Stress-Aware WL-Based Read Disturbance Management for High-Density NAND Flash Memory.pdf>)

저자: Myoungjun Chun (Soongsil University), Jaeyong Lee (Seoul National University), Inhyuk Choi (Seoul National University), Jisung Park (POSTECH), Myungsuk Kim (Kyungpook National University), Jihong Kim (Seoul National University)

## TL;DR
고밀도 3D NAND flash에서 read disturbance는 read 시 target WL(wordline)뿐 아니라 같은 블록의 모든 non-target WL에 pass-through voltage($V_{pass}$)를 인가해 미세하게 프로그래밍시키는 현상이며, 기존 대응책인 read reclaim(RR)은 블록 단위로만 관리되어 premature RR을 유발한다. STRAW는 (i) WL마다 disturbance를 개별 모니터링해 실제로 heavily disturbed된 WL만 reclaim하는 stress-aware WL-based Read Reclaim($\text{WR}^2$)과 (ii) read 시 invalid WL에는 $V_{pass}$를 높이고 그 여유분(error margin)을 이용해 valid WL의 $V_{pass}$를 낮추는 Stress-Reduced Read($\text{SR}^2$)를 결합한다. 160개 실제 3D TLC NAND 칩에 대한 대규모 characterization으로 WL별 신뢰도 모델(ERC$_{MAX}$, disturbance rate $\alpha$)을 도출하고, 이를 경량 FTL(StrawFTL)로 구현했다. NVMeVirt 기반 SSD 에뮬레이터 평가에서 state-of-the-art RR 기법 대비 RR-induced write를 평균 88.6%, 99th-percentile read-tail latency를 평균 66.3% 감소시켰다.

## 문제 & 동기
고밀도 3D NAND는 수직 WL 적층(예: 321-layer)과 MLC→TLC→QLC 전환으로 저장 밀도를 높여왔지만, 그 대가로 read disturbance가 급격히 악화되었다. Read 시 target WL에는 $V_{ref}$를, 나머지 모든 non-target WL에는 $V_{pass}$($>6\,V$)를 인가하는데, 이 $V_{pass}$가 non-target WL의 셀을 미세하게 프로그래밍시켜 반복 read 시 $V_{th}$ 분포를 이동시키고 결국 uncorrectable bit error를 유발한다 (p.1899, Fig.2). 블록 크기가 커질수록(예: 321-layer 3D NAND) 한 번의 read가 disturb하는 데이터량이 100MB를 넘는다.

기존 대응책은 모두 **block-level Read Reclaim(RR)**이다 — 블록의 read count $RC$가 임계값 $RC_{MAX}$를 넘으면 블록 내 모든 valid page를 복사해 disturbance 효과를 리셋한다. 저자들은 두 가지 근본적 한계를 지적한다: (1) **Block-level granularity** — WL 간 신뢰도(공정 편차) 및 인접/비인접 disturbance 영향이 매우 이질적인데도 블록 전체에 대해 worst-case WL 기준의 단일 $RC_{MAX}$를 적용하므로 premature RR이 빈발한다. (2) **Reactive 설계** — disturbance-induced error가 이미 누적된 후에야 개입하므로, 고밀도일수록(disturbance tolerance가 낮을수록) RR 비용이 반비례로 커진다.

정량적으로, 176-layer 3D QLC SSD는 30TB host read 후 8.7TB의 RR-induced internal write가 발생했고(Fig.3a, p.1900), 200TBW 가정 시 50MB/s read-only workload로 144일 만에 SSD가 소진될 수 있다(3년 보증 기간보다 훨씬 짧음, p.1900). 블록 내 최악 WL은 균일 접근 시 403K회, 최상 WL은 그보다 559K회 더 많은 read를 견디는 등(Fig.4b, p.1900) WL 간 tolerance 편차가 크고, 1K P/E cycle(PEC) 기준 3D QLC 칩의 최대 read count는 2D MLC·2D TLC 대비 각각 88.2%, 79% 낮다(p.1901).

> [!quote]- 📄 원문 표현 (paper)
> - "Our characterization of five real 3D NAND chip types shows that data loss can occur at a 88.2% lower RC in 3D QLC NAND flash memory than in 2D TLC NAND flash memory." (p.1898)
> - "Managing the read disturbance problem at block granularity is highly inefficient in modern NAND flash memory." (p.1900)
> - "Existing solutions for modern SSDs manage read disturbance reactively: they wait until disturbance-induced reliability degradation exceeds $RC_{MAX}$, and then trigger RR." (p.1900)

## 핵심 통찰 (Key Insight)

**1. Read disturbance는 WL 단위로 관리해야 한다.** 블록 내 WL마다 (i) 인접 WL에 더 큰 $V_{passH}$(≈10% 높음)가 인가되어 disturbance 영향이 비대칭적이고, (ii) 공정 편차로 WL 간 disturbance tolerance가 크게 다르다(p.1901, Fig.4). 이를 블록 전체에 대해 worst-case로 뭉뚱그리면 대부분의 WL에서 불필요하게 이른 RR이 발생한다. WL 단위로 실제 disturbance 누적을 추적하면, heavily disturbed WL만 선택적으로 reclaim하면서도 데이터 무결성을 보장할 수 있다.

> [!quote]- 📄 원문 표현 (paper)
> - "WR² is based on our key insight that the read disturbance should be managed at a finer granularity, i.e., per WL, not per block." (p.1901)

**2. Invalid WL의 여유 error margin을 valid WL의 $V_{pass}$ 저감에 전용(轉用)할 수 있다.** Read 시 non-target WL의 $V_{pass}$를 낮추면 disturbance는 줄지만 naïve하게 전체에 적용하면 target page의 RBER이 오히려 급증한다(1K PEC에서 1KB당 bit error가 18→84로 증가, ECC decoding time 5배 증가, p.1902). 그러나 invalid WL(더 이상 읽히지 않는 페이지)은 신뢰도에 영향이 없으므로, 그곳에는 오히려 $V_{pass}$를 높여 error margin을 만들고, 그 margin을 valid WL의 $V_{pass}$ 저감에 사용하면 데이터 무결성 손실 없이 disturbance 스트레스만 줄일 수 있다.

> [!quote]- 📄 원문 표현 (paper)
> - "Key Idea. Since the reliability impact of read disturbance is highly proportional to $V_{pass}$, reducing $V_{pass}$ is highly effective in mitigating stress on non-target WLs." (p.1902)
> - "Our key insights are that (i) applying a higher $V_{pass}$ to certain non-target WLs can reduce the RBER of the target WL, and (ii) the increased read-disturbance impact on invalid WLs does not affect overall data reliability." (p.1902)

## 설계 / 메커니즘 (Design)

**Stress-Aware WL-based Read Reclaim ($\text{WR}^2$).** 각 WL$_i$에 대해 read-disturbance model이 두 파라미터를 정의한다: $ERC_{MAX}[WL_i]$ (해당 WL이 uncorrectable error 없이 견딜 수 있는 누적 effective read count 상한)와 disturbance rate $\alpha$ (인접 WL에 $V_{passH}$가 인가될 때의 disturbance가 기본 $V_{pass}$ 대비 몇 배인지의 비율, p.1902). Per-WL 카운터가 read count $RC$를 기록하고, WR²는 (❶) $\alpha \times$ 인접-WL read count로 $V_{passH}$의 기여분을 $ERC$로 환산, (❷) 비인접-WL 기여분과 합산해 총 $ERC[WL_i]$ 계산, (❸) 이를 $ERC_{MAX}[WL_i]$와 비교해 reclaim 여부를 결정한다(Fig.8, p.1902). 예시(Fig.7, p.1901): 특정 WL만 반복 read하는 패턴(PA)에서 기존 block-level RR은 블록 전체를 54,570 RC마다 reclaim해야 하지만, WR²는 인접 WL(WL34, WL36)만 각각 54,570·66,270 RC 후 개별 reclaim하고, 실제 최악 WL(WL37)의 reclaim은 548,044 RC까지 지연시켜 훨씬 많은 read를 추가로 허용한다.

**Stress-Reduced Read ($\text{SR}^2$).** Read 시 기본 $V_{pass}$는 인접 6.5V(비인접), 7.2V($V_{passH}$, 인접)인데, SR²는 두 단계로 조정한다: (i) invalid non-target WL의 $V_{pass}$를 올려 error margin 확보, (ii) 확보된 margin을 이용해 valid non-target WL의 $V_{pass}$를 낮춰 target WL의 $N_{ERR}$(1KB당 최대 허용 bit error 수)을 감소시킨다. Fig.9(p.1903) 예시: target이 WL4일 때 기본 $N_{ERR}=23$, 인접·비인접 invalid WL(WL1,2,3,5)의 $V_{pass}$를 각각 0.7V씩 올려 $N_{ERR}$을 13만큼 줄인 뒤(23→10), 그 여유로 valid WL(WL6,7)의 $V_{pass}$를 0.2V 낮춰 disturbance 스트레스를 완화(추가 $N_{ERR}$ +10, 최종 20 < 23)한다. 두 기법은 통합적으로 작동한다: WR²가 heavily disturbed WL을 reclaim하면 그 WL은 invalid가 되어 SR²가 활용할 margin이 늘어난다(p.1903).

**Read-disturbance/Vpass-reduction 모델링.** 160개 3D TLC 칩·3,686,400개 WL(11,059,200 페이지)에 대한 실측(FPGA 기반 커스텀 컨트롤러, SET FEATURES 명령으로 WL별 $V_{pass}$ 조정, JEDEC 표준 준수, p.1903)을 통해 WL을 초기 RBER 기준 Best/Good/Bad/Worst 4개 그룹으로 분류하고 PEC별 $ERC_{MAX}$·$\alpha$를 도출했다(Fig.11, p.1904). 예: 2K PEC에서 Worst 그룹은 $ERC_{MAX}=749K$, $\alpha=8.8$. $V_{pass}$ 저감이 $N_{ERR}$에 미치는 영향도 실측해(Fig.12, p.1904) 비인접 WL의 75%에 $\Delta V_{pass}=-10\%$ 적용 시 1K/2K PEC에서 $N_{ERR}$이 각각 33.3%/39.1% 감소함을 확인했고, 이를 바탕으로 인접 WL validity와 비인접 invalid 비율($R_{INV}$)에 따른 최종 $V_{pass}$-Reduction Model(Fig.13, p.1905)을 구축했다.

**StrawFTL 구현.** 기존 page-level FTL을 3개 자료구조로 확장한다: RPT(Read-reclaim Parameter Table, $ERC_{MAX}$·$\alpha$·$\beta_{2.5}$·$\beta_5$를 ⟨PEC, WL group⟩별로 오프라인 프로파일링해 저장), REC(Resource-Efficient Counters, WL별 $RC$를 추적), PVT(Pass-through Voltage Table, ⟨PEC, 인접 WL validity, $R_{INV}$⟩별 최소-안전 $V_{pass}$ 기록)(Fig.15, p.1905). Naïve per-WL 카운터는 2TB SSD 기준 약 125MB DRAM(WL당 3-byte 카운터)이 필요해 block-level 대비 2,568배 오버헤드가 있는데(p.1902), STRAW는 **Space-Saving algorithm**[49]으로 REC를 구현해 제한된 카운터 수로 빈도를 근사 추정하며, 추정값이 실제보다 과소평가되지 않음을 보장해(과대평가만 가능 → 안전 방향으로만 오차, premature RR은 유발할 수 있어도 데이터 손실은 없음, p.1906) 공간 오버헤드를 크게 절감한다.

> [!quote]- 📄 원문 표현 (paper)
> - "WR² only requires simple modifications to the existing SSD firmware but no modification to the flash controller and chips." (p.1902)
> - "Applying SR² to commodity 3D NAND chips is highly feasible at low cost." (p.1903)
> - "SS ensures that errors in estimation do not result in read-disturbance-induced data corruption, although they may cause premature RR invocations." (p.1906)

## 평가 (Evaluation)
NVMeVirt[40]를 확장한 SSD 에뮬레이터로 480GB 용량(8채널, 채널당 2 die, die당 4 plane, plane당 232 블록, 블록당 2112 page, 176 layer/block, 4WL/layer)의 SSD를 모델링하고(Table 1, p.1906), FIO(2종), YCSB-A/E, Filebench, Lumos(그래프 처리), Llama(LLM 추론) 총 7개 워크로드로 평가했다(Table 2, p.1907). 비교 대상은 Baseline(conservative block-level RR), Cocktail[81]·Pagetype[26](기존 SOTA RR 최적화), STRAW−(WR²만), STRAW(WR²+SR²), STRAW+Cocktail이다.

- **RR-induced page copy**: STRAW는 Baseline 대비 PEC=⟨0K,1K,2K⟩에서 평균 ⟨90%, 92.2%, 93.6%⟩ 감소(Fig.17, p.1907). STRAW+Cocktail은 read-dominant 워크로드(read ratio>0.8)에서 STRAW 대비 최대 31%(평균 25.8%) 추가 감소(p.1907).
- **99.9th-percentile read latency**: Baseline 대비 STRAW(STRAW−)는 PEC=⟨0K,1K,2K⟩에서 ⟨65.2%, 71.1%, 75.6%⟩(⟨56.1%, 64%, 69.4%⟩) 감소(Fig.18, p.1907-1908). YCSB-E·Llama처럼 read 비중이 높은 워크로드에서 효과가 가장 컸다.
- **REC 크기 민감도**: 704-WL 블록당 32-entry/64-entry REC는 무제약(counter-per-WL) 대비 공간 오버헤드를 95.5%/91% 절감하는 대신 RR-induced page copy가 74.4%/26.6% 증가(Fig.19, p.1908).
- **Lifetime(총 block erasure 수)**: WR²는 block erasure를 즉시 트리거하지 않고 지연시키므로 mixed read/write 워크로드(FIO-2, YCSB-A)에서 GC 오버헤드가 늘 수 있으나, 실측 결과 STRAW는 모든 테스트 조건에서 total block erasure를 평균 53.6% 감소시켜 premature RR 방지 효과가 추가 GC 비용을 상회함을 보였다(Fig.20a, p.1908).
- **SR² 오버헤드**: invalid WL에 기본보다 10% 높은 $V_{pass}$를 적용해도 precharge latency 증가는 최대 4%에 grinding, precharge가 전체 read latency의 약 30%를 차지하므로 전체 영향은 ≤1.2%로 무시할 수준(Fig.20b, p.1908).

> [!quote]- 📄 원문 표현 (paper)
> - "STRAW reduces read-disturbance–induced writes and 99th-percentile read-tail latency by an average of 88.6% and 66.3%, respectively, over an SSD with the state-of-the-art RR technique." (p.1898-1899)
> - "STRAW reduces total block erasures across all tested conditions (53.6% on average), demonstrating that lifetime gains from preventing premature RR outweigh the additional GC cost—even under mixed workloads." (p.1908)

## 섹션 노트
- **§1 Introduction**: 고밀도 3D NAND(적층 WL, MLC/TLC/QLC)에서 read disturbance가 심화되는 배경과 STRAW의 두 메커니즘(WR², SR²)을 개괄.
- **§2 Background**: 3D NAND 구조(WL·string·block·plane·die)와 read disturbance 메커니즘($V_{pass}$가 non-target WL을 미세 프로그래밍) 설명.
- **§3 Motivation**: 상용 SSD 3종 characterization으로 RR의 부정적 영향(내구성·tail latency) 정량화, 기존 block-level·reactive RR의 두 한계 제시.
- **§4 STRAW: Key Mechanisms**: $\text{WR}^2$(WL 단위 reclaim)와 $\text{SR}^2$(적응적 $V_{pass}$ scaling) 설계 상세.
- **§5 Device Characterization Study**: 160개 3D TLC 칩·JEDEC 표준 기반 실측으로 read-disturbance model(ERC$_{MAX}$, $\alpha$)과 $V_{pass}$-reduction model 도출.
- **§6 Design and Implementation of StrawFTL**: RPT/REC/PVT 세 자료구조와 Space-Saving 기반 카운터 오버헤드 최적화.
- **§7 Evaluation**: NVMeVirt 에뮬레이터·7개 워크로드로 RR 효율성·read tail latency·lifetime·SR² 오버헤드 평가.
- **§8 Related Work**: RR 최적화(Cocktail, Pagetype 등)와 2D NAND용 $V_{pass}$-scaling 선행연구(Ha et al.[25], Cai et al.[9]) 대비 STRAW의 차별점(WL 단위 granularity, 고밀도 3D NAND 대상 검증) 논의.
- **§9 Conclusion**: WL 단위 disturbance 식별과 $V_{pass}$-scaling으로 premature RR을 없애 modern SSD의 lifetime·성능을 개선했다는 결론.

## 핵심 용어 (Key terms)
- **Read disturbance**: NAND read 시 target WL에 인가된 $V_{ref}$ 외에 non-target WL 전체에 $V_{pass}$가 인가되어 해당 WL이 미세하게 프로그래밍되며 반복 시 bit error를 유발하는 현상.
- **Read Reclaim (RR)**: 블록의 read count가 임계값을 넘으면 해당 블록의 모든 valid page를 새 블록으로 복사해 disturbance 효과를 리셋하는 SSD 내부 관리 동작.
- **$RC_{MAX}$**: 기존 block-level RR이 데이터 손실을 막기 위해 설정하는 블록 read count 상한(보수적으로 worst-case WL 기준 설정).
- **Pass-through voltage ($V_{pass}$)**: Read 시 non-target WL을 pass transistor처럼 동작시키기 위해 인가하는 전압으로, 이 값이 클수록 disturbance가 커짐.
- **$ERC_{MAX}$**: 개별 WL이 uncorrectable error 없이 견딜 수 있는 누적 effective read count 상한(WR²의 핵심 파라미터).
- **Disturbance rate ($\alpha$)**: 인접 WL에 $V_{passH}$가 인가될 때의 disturbance가 기본 $V_{pass}$ 대비 몇 배인지를 나타내는 비율.
- **$\text{WR}^2$ (stress-aware WL-based Read Reclaim)**: 블록이 아닌 WL 단위로 disturbance를 모니터링해 heavily disturbed된 WL만 선택적으로 reclaim하는 STRAW의 첫 번째 메커니즘.
- **$\text{SR}^2$ (Stress-Reduced Read)**: read 시 invalid WL의 $V_{pass}$는 높이고 그 margin으로 valid WL의 $V_{pass}$를 낮춰 disturbance를 완화하는 STRAW의 두 번째 메커니즘.
- **RBER (Raw Bit Error Rate)**: ECC 정정 전 원본 bit error 비율.
- **$N_{ERR}$**: 특정 WL의 1KB 데이터당 허용 가능한 최대 bit error 수(SR²의 판단 기준).
- **Space-Saving algorithm**: 제한된 개수의 카운터로 데이터 스트림 내 원소 빈도를 과소평가 없이 근사 추정하는 알고리즘, STRAW에서 REC(per-WL read count) 저장 공간 절감에 사용.
- **StrawFTL**: RPT(Read-reclaim Parameter Table)·REC(Resource-Efficient Counters)·PVT(Pass-through Voltage Table)로 기존 FTL을 확장한 STRAW 구현체.

## 강점 · 한계 · 열린 질문
**강점**: (1) 160개 실제 3D TLC 칩에 대한 대규모 device characterization으로 모델을 실증적으로 뒷받침. (2) 기존 SSD 펌웨어와 컨트롤러/칩에 대한 변경이 거의 없이(SET FEATURES 명령만 활용) 구현 가능. (3) Cocktail 같은 기존 RR 최적화 기법과 결합 가능(orthogonal), STRAW+Cocktail로 추가 이득 확인. (4) Space-Saving 알고리즘으로 안전 방향(과대평가만) 오차를 보장하며 카운터 공간 오버헤드를 대폭 절감.

**한계**: (1) WR²가 block erasure를 지연시키는 특성상 mixed read/write 워크로드에서 GC 오버헤드가 증가할 수 있음(논문은 net positive라고 주장하나 GC 정책·워크로드에 따라 민감할 수 있음, p.1902, §7.3). (2) Characterization이 3D TLC 칩 160개에 한정되어 있고, QLC/PLC 등 마진이 더 좁은 기술로의 일반화는 "구조적 유사성" 가정에 의존(p.1903). (3) SET FEATURES 기반 per-WL $V_{pass}$ 프로그래밍 능력이 실제 상용 칩 전반(Samsung, Seagate 등 언급되나 검증은 자사 커스텀 칩 한정)에서 동일하게 지원되는지는 가정에 의존. (4) REC 크기가 워크로드 working-set 대비 작으면 premature RR이 다시 증가(Fig.19)하므로 REC 크기 튜닝이 워크로드 의존적.

**열린 질문**: WR²/SR²가 retention loss나 program interference 같은 다른 에러 소스와 상호작용할 때도 안전한가? 온도 변화에 따라 RPT/PVT 모델을 online으로 재보정할 필요는 없는가? 321-layer급 초고적층 칩에서 REC의 Space-Saving 카운터 규모가 어떻게 스케일하는가?

## ❓ Q&A (자가 점검)
> [!question]- 기존 block-level RR과 STRAW의 WR²는 근본적으로 무엇이 다른가?
> Block-level RR은 블록 전체에 대해 worst-case WL 기준의 단일 $RC_{MAX}$를 적용해 블록 read count가 넘으면 모든 valid page를 복사한다. WR²는 WL마다 개별적으로 disturbance 누적($ERC$)을 추적해 실제로 heavily disturbed된 WL만 reclaim하고, 나머지 WL의 reclaim은 필요한 시점까지 지연시킨다(p.1901, Fig.7).

> [!question]- SR²가 naïve한 전역 $V_{pass}$ 저감과 다른 점은?
> 전역적으로 $V_{pass}$를 낮추면 target WL의 RBER이 오히려 급증한다(1K PEC에서 bit error가 18→84로 5배 가까이 증가, p.1902). SR²는 invalid WL의 $V_{pass}$를 오히려 높여 확보한 error margin만큼만 valid WL의 $V_{pass}$를 낮춰, 데이터 무결성을 유지하면서 disturbance만 줄인다.

> [!question]- Disturbance rate $\alpha$는 무엇을 의미하며 왜 필요한가?
> 인접 WL에 인가되는 $V_{passH}$가 기본 $V_{pass}$ 대비 얼마나 더 큰 disturbance를 유발하는지의 비율이다(예: 2K PEC에서 Worst 그룹 $\alpha=8.8$). WR²는 이 값으로 인접/비인접 read count를 하나의 등가 disturbance 단위($ERC$)로 환산해 $ERC_{MAX}$와 비교한다(Fig.8, p.1902).

> [!question]- Space-Saving 알고리즘이 REC에 왜 필요하며 안전성은 어떻게 보장되는가?
> Naïve per-WL 카운터는 2TB SSD에서 약 125MB DRAM(블록-level 대비 2,568배)이 필요할 만큼 오버헤드가 크다. Space-Saving은 제한된 카운터로 빈도를 근사하되 실제 카운트를 과소평가하지 않도록 보장하므로, 추정 오차가 있어도 premature RR만 유발할 뿐 read-disturbance로 인한 데이터 손실은 발생하지 않는다(p.1906).

> [!question]- WR²가 block erasure를 지연시키는 것이 SSD 수명에 부정적 영향을 주지 않는가?
> WR²는 block-level RR처럼 즉시 블록을 지우지 않고 블록 내 모든 WL이 heavily disturbed될 때까지 erasure를 미룬다. 이는 mixed read/write 워크로드에서 GC(garbage collection) 빈도를 늘릴 수 있지만, 실측(FIO-2, YCSB-A)에서 STRAW는 오히려 total block erasure count를 평균 53.6% 줄여 premature RR 방지 효과가 추가 GC 비용을 상회함을 보였다(Fig.20a, p.1908).

> [!question]- STRAW는 어떤 하드웨어 변경을 요구하는가?
> 플래시 칩 자체의 근본 회로 변경은 필요 없다. 상용 3D NAND 칩이 이미 갖춘 WL별 $V_{pass}$ switching 회로와 SET FEATURES 명령(ONFI 표준)만으로 WL별 $V_{pass}$를 조정할 수 있어, SSD 펌웨어(StrawFTL) 수준의 확장만으로 구현 가능하다(p.1903).

> [!question]- STRAW+Cocktail 조합이 왜 추가 이득을 주는가?
> Cocktail은 hot page를 블록 간에 재분배해 이후 RR 빈도를 줄이는 orthogonal한 최적화다. STRAW가 WL 단위로 premature RR 자체를 억제하는 것과 서로 다른 축에서 작동하므로, read-dominant 워크로드에서 STRAW 대비 최대 31%(평균 25.8%) 추가로 RR-induced page copy를 줄인다(p.1907).

## 🔗 Connections
[[Reliability]] · [[ASPLOS]] · [[2026]]
관련: [[RiF - Improving Read Performance of Modern SSDs Using an On-Die Early-Retry Engine]] · [[Midas Touch - Invalid-Data Assisted Reliability and Performance Boost for 3d High-Density Flash]] · [[DEAR - Improving Performance and Lifetime of SSDs Using Dynamic Error-Aware Refresh]]

## References worth following
- Zhang et al., "Cocktail: Mixing Data with Different Characteristics to Reduce Read Reclaims for NAND Flash Memory," IEEE TCAD 2022 [81] — STRAW 평가의 state-of-the-art baseline이자 STRAW와 결합 가능한 hot-page 재분배 기법.
- Han et al., "Page Type Data Migration Technique for Read Disturb Management of NAND Flash Memory," IEEE TVLSI 2023 [26] — 페이지 타입(MSB/CSB/LSB) 기반 read disturb 관리, STRAW와 비교되는 또 다른 SOTA.
- Ha, Jeong, Kim, "An Integrated Approach for Managing Read Disturbs in High-Density NAND Flash Memory," IEEE TCAD 2015 [25] — 2D NAND 대상 read-hot 블록 재프로그래밍+$V_{pass}$ 저감 통합 접근의 선행연구로 STRAW의 $V_{pass}$-scaling 아이디어의 뿌리.
- Cai, Luo, Ghose, Mutlu, "Read Disturb Errors in MLC NAND Flash Memory: Characterization, Mitigation, and Recovery," DSN 2015 [9] — read disturbance 현상 자체의 초기 characterization 및 $V_{pass}$ 저감 완화 기법.
- Chun, Kim, Kim, Park, Kim, "RiF: Improving Read Performance of Modern SSDs Using an On-Die Early-Retry Engine," HPCA 2024 [18][19] — 동일 저자 그룹의 read-path 성능 최적화 선행연구.
- Metwally, Agrawal, Abbadi, "Efficient Computation of Frequent and Top-k Elements in Data Streams," ICDT 2005 [49] — STRAW의 REC 구현에 쓰인 Space-Saving 알고리즘의 원 논문.

## Personal annotations
<!-- 본인 메모 영역 -->
