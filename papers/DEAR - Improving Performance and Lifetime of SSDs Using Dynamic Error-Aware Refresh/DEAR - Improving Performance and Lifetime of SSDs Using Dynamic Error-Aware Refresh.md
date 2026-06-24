---
title: "DEAR: Improving Performance and Lifetime of SSDs Using Dynamic Error-Aware Refresh"
aliases: [DEAR]
description: "WL 단위 실시간 에러 추정(SEE)으로 불필요한 read-refresh를 줄여 SSD 성능·수명을 개선하는 동적 error-aware refresh 기법"
venue: MICRO
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/reliability
  - topic/refresh
  - topic/error
  - topic/lifetime
  - venue/micro
  - year/2025
---

# DEAR: Improving Performance and Lifetime of SSDs Using Dynamic Error-Aware Refresh

> **MICRO 2025** · `cluster/reliability` · Source: [DEAR - Improving Performance and Lifetime of SSDs Using Dynamic Error-Aware Refresh.pdf](<DEAR - Improving Performance and Lifetime of SSDs Using Dynamic Error-Aware Refresh.pdf>)

**저자**: Jaeyong Lee (Seoul National University), Beomjun Kim (Kyungpook National University), Myoungjun Chun (Soongsil University), Myungsuk Kim (Kyungpook National University), Jihong Kim (Seoul National University). 교신저자: Jihong Kim, Myungsuk Kim.

## TL;DR
기존 SSD는 block 단위 고정 read-count 임계값(C_max)으로 read-refresh(RR)를 trigger하지만, 3D NAND의 심한 process variation 때문에 최악 WL 기준으로 설정된 보수적 C_max가 대부분 block에서 RR을 조기에 불필요하게 호출한다. DEAR는 WL 단위로 실시간 에러 상태를 추정하는 경량 추정기 **SEE(Swift Error Estimator)** 를 도입해, 실제 read-disturb 위험이 감지된 WL만 선택적으로 refresh한다. 결과적으로 RR-induced write를 평균 64.2% 줄이고, 99.9th read latency를 44.9%, SSD 수명을 36.7% 개선했다.

## 문제 & 동기
3D NAND의 대용량화(3D stacking + multi-level cell)는 read-disturb 문제를 심화시켰다. read 시 비대상 WL에 인가되는 pass-through voltage(V_pass)가 셀을 약하게 program(soft-program)시켜 V_th를 우측으로 shift시키며, 반복 read로 누적되면 ECC 정정 능력을 초과해 데이터 손실을 유발한다. 이를 막기 위해 SSD는 block의 read count가 사전 정의 상수 C_max에 도달하면 RR(유효 페이지를 읽어 다른 free block으로 재기록)을 수행한다. 그러나 RR은 추가 read/write/erase(P/E)를 일으켜 host I/O와 충돌(tail latency 악화)하고 WAF를 증가시켜 수명을 깎는다.

핵심 문제는 C_max가 "모든 block이 보장 가능한" 최악 WL의 C_max로 설정된다는 점이다. 저자들의 11개 상용 SSD 측정 결과, 최악 block의 C_max는 최고 block보다 69.7% 낮고(inter-block), 같은 block 내에서도 최고 WL이 최악 WL보다 2.23배 많은 read를 견딘다(intra-block). 보수적 C_max는 대부분 block에서 RR을 조기 호출해 막대한 성능·수명 낭비를 초래한다.

> [!quote]- 📄 원문 표현 (paper)
> "Because existing schemes set thresholds based on the worst-case scenario (the lowest C_max), RR procedures are prematurely invoked for most blocks, leading to substantial and unnecessary performance and lifetime degradation." (p.2)
>
> "Our characterization (Figure 1) reveals two critical observations: (1) the worst block's C_max is 69.7% lower than that of the best block, and (2) within the same block, the best wordline (WL) tolerates 2.23 times more reads than the worst WL." (p.2)

## 핵심 통찰

> [!note]- 통찰 1 — Process variation으로 WL/block 간 C_max가 크게 다르다
> 3D NAND 제조 시 channel hole 직경이 h-layer에 따라 변동(위쪽이 넓고 아래쪽이 좁음)하고, 일부는 타원형으로 형성되어 셀별 에러 특성이 vertical 방향으로 달라진다. 결과적으로 C_max^WL의 분포가 매우 넓다(Figure 9 CDF). 3,686,400개 WL 측정에서 0K P/E 시 최고-최악 block 격차 250K reads, 2K P/E 시 같은 block 내 격차 4.14배까지 확대된다. 단일 고정 C_max는 더 이상 실용적이지 않다.

> [!note]- 통찰 2 — read-disturb 에러는 erased(ER) 상태에 집중된다
> Figure 4(a): 2000K reads 후 전체 bit error의 67%가 ER 상태에서 발생. soft-program 효과는 낮은 V_th 상태에서 더 크기 때문(V_pass가 높은 V_th 셀은 충분히 shift시키지 못함). 따라서 ER 상태의 fail-bit count(C_FBC(ER))만 측정해도 전체 read-disturb 에러를 정확히 대리(proxy)할 수 있어, full page read + ECC decoding 없이 빠르게 에러 추정이 가능하다.

> [!note]- 통찰 3 — 같은 h-layer의 WL들은 거의 동일한 에러 특성을 보인다 (intra-layer similarity)
> 같은 h-layer 내 WL들은 물리적 특성(channel hole 직경)이 유사하고, read 시 동일한 voltage 조건에 놓이므로 read-disturb 효과가 거의 같다. ΔC_x^i(층 내 최대/최소 C_max^WL 비율)가 1에 근접(예: 한 h-layer 내 4 WL 간 0.85% 이내). 따라서 h-layer당 leading WL 하나만 모니터링하면 나머지 WL의 에러 상태를 추론할 수 있어 모니터링 비용을 1/m(m=층당 WL 수)로 절감한다.

## 설계 / 메커니즘

> [!abstract]- DEAR 핵심 아이디어 & SEE의 3가지 최적화
> **DEAR의 key idea**: 고정 C_max 대신, 각 WL이 견딜 수 있는 near-optimal C_max^WL을 동적으로 산정하고, 실제로 disturb된 WL만 WL 단위로 selective refresh한다(기존은 block당 4 WL을 100 reads에서 통째로 refresh; DEAR는 300 reads까지 미룸 — Figure 10). 핵심 도전은 runtime에 각 WL 에러를 추적하는 비용(120MiB block의 모든 WL 모니터링 시 최대 337ms)이다. 이를 SEE(Swift Error Estimator)로 해결한다.
>
> **Opt 1 — Similarity-Aware Monitoring (SAM)**: intra-layer similarity를 이용, 각 h-layer의 leading WL(WL^1)만 모니터링하고 나머지는 추론. 모니터링 대상 WL 수를 4배(일반적으로 m배) 감소. (Figure 11)
>
> **Opt 2 — Fail-bit-assisted Error Monitoring (FEM)**: ER 상태의 C_FBC(ER)만 single sensing(V_REF^1)으로 측정 → ECC decoding 불필요. LSB read는 2 sensing, CSB는 3 sensing 필요한데 FEM은 1 sensing만 사용해 monitoring latency를 7배 절감. offline 구축한 correlation model로 C_FBC(ER)→실제 에러를 추정. (Figure 12)
>
> **Opt 3 — Block-wise Periodic Monitoring (BPM)**: 매 read마다가 아니라 read count가 C_INT 배수일 때만 block 단위로 모니터링(read-disturb는 점진 증가하므로 안전). 안전을 위해 RR trigger를 ECC_CAP보다 약간 보수적인 E_TH로 낮춰, 다음 모니터링 시점까지의 worst-case 누적을 흡수. (Figure 13)

> [!abstract]- SEE 동작 파라미터 & RR invocation flow
> - **C_FBC^TH**: RR을 trigger할 fail-bit count 임계값. 식 (2): C_FBC^TH = min{C_FBC | M_RBER(C_FBC) ≥ E_TH}. C_FBC를 worst-case(100th percentile) bit error rate로 보수적 매핑.
> - **C_INT 결정**: E_TH와 모니터링 overhead 간 trade-off(Figure 16). E_TH 클수록 RR 적게 호출되나 safety margin 작아져 모니터링 빈번. safety margin 5%일 때 2K P/E에서 C_INT=19.3K reads 필요.
> - **Interleaved monitoring (Figure 18)**: 321 h-layer block 1회 모니터링이 6.42ms 소요되므로, monitoring을 작은 단위로 쪼개 user read와 섞음. C_INT^WL = C_INT/h-layer수(예: 19.3K/321마다 1 WL 검사)로 분산해 amortize.
> - **RR invocation flow (Figure 19)**: ① block read count가 C_INT^WL 배수인지 확인 → ② WP(per-block WL pointer)가 가리키는 target WL의 C_FBC(ER) 측정·count → ③ C_FBC(ER) ≥ C_FBC^TH면 해당 WL + 같은 h-layer 이웃 WL refresh, 새 block으로 migrate, FTL이 mapping table 갱신.
> - **C_FBC 측정 HW(Figure 17)**: data randomization(V_th 균등 분포)으로 ER 셀 수가 일정(예: TLC에서 WL당 16K) → V_REF^1 sensing 후 '1' bit 수를 flash 내장 1s counter로 세고 ECC engine bypass. 28μs(20μs sensing + 8μs counting)에 측정. 기존 GET/SET FEATURE 명령 활용, NAND HW 변경 불필요.

> [!abstract]- DEAR-SSD(시스템 통합) 오버헤드
> - Reference table(P/E cycle 범위별 C_INT^WL, C_FBC^TH 사전 특성화 값): 8 bytes/entry, 2K P/E 한도 시 총 160 bytes. flash에 firmware와 함께 저장, runtime에 내부 메모리로 load.
> - per-block WP entry 2 bytes; 88MiB block 가정 시 1TB SSD에 약 24KiB.
> - GC: DEAR가 highly-disturbed WL만 refresh해 일부 fragmented block 생기지만 기존 GC로 자연 회수되며 GC 변경 불필요.
> - Implementation overhead: 기존 SSD 대비 minor change(GET/SET FEATURE + 1s counter는 기존 special flash command), HW 무변경. 모니터링 overhead < 1.2%.

## 평가

> [!success]- 평가 환경 & 주요 수치
> **환경**: NVMeVirt(state-of-the-art SSD emulator) 확장으로 heterogeneous read-disturb 특성 emulation. 특성화는 160개 실제 3D TLC chip(3,686,400 WL). target SSD: 480GiB, 176 layers, C_max^WL=261 reads(0K P/E)~108 reads(2K P/E). 10개 workload(FIO, Filebench, YCSB), 각 ~400GiB dataset, 3시간+ 실행으로 steady-state 확보. baseline = priority-aware scheduling + block 단위 C_max RR; 비교군: Baseline, Cocktail(read-hot 분산), DEAR, DEAR+(=DEAR+Cocktail). (p.10)
>
> **RR-induced write 감소(Figure 20)**: DEAR가 Baseline 대비 평균 64.2% 감소(read-intensive 워크로드에서 최대 86.8%). Cocktail은 평균 16.3%(최대 42.8%)에 그침. DEAR+는 최대 95.4%, 평균 75.6% 감소. (p.10-11)
>
> **Read tail latency(Figure 21, 22)**: DEAR가 3개 P/E cycle 평균 τ99p를 24.4%/46.7%/35.9%, τ99.9p를 28.9%/50.9%/54.6% 감소(read-intensive). DEAR+는 τ99p 최대 34.2%(평균 53.4%), τ99.9p 최대 43.8%(평균 57.7%) / 52.3%(평균 62.8%) 감소. (p.11)
>
> **SSD 수명/WAF(Figure 23)**: 2K P/E에서 DEAR가 WAF를 Baseline 대비 최대 72.2%(평균 46.7%), Cocktail 대비 최대 70%(평균 44.7%) 감소. sequential write 워크로드(Web, Video)에서 큰 효과, random write(YCSB-B)에서는 GC 비효율로 효과 제한적. (p.11)
>
> **Monitoring overhead(Figure 24)**: C_INT^WL=60 reads 시 기존 SSD 대비 IOPS 0.7% 감소, latency 1.3% 증가에 불과. tight interval(15 reads, 1284 h-layer block 상당)에서도 매우 낮음. (p.12)
>
> **Abstract 종합 수치**: RR invocation 평균 64.2% 감소, 99.9th read latency 44.9% 개선, SSD 수명 36.7% 연장(state-of-the-art 대비). (p.1)

## 섹션 노트
- §1 Introduction: read-disturb 문제 배경, RR의 성능/수명 비용, 고용량 3D NAND에서 악화(block size 증가, m-bit MLC로 error margin 축소). QLC는 C_max가 MLC 대비 81.8% 낮음(Figure 7). 11개 상용 SSD 동기 실험.
- §2 Background: 3D NAND 구조(NAND string, h-layer/v-layer, channel hole 변동), flash 동작(program/erase/read), MLC, read-disturb 메커니즘(FN tunneling 식 (1): Error_RD ∝ V_pass²·exp(-1/V_pass)·T_read·N_read), state-dependent error(ER 집중).
- §3 Empirical Study: 11 SSD spec(Table 2). RR-induced write가 고용량 SSD에서 지수적 증가(T-176-A, Q-176-A는 200 TBW 보증의 경우 263일/77일 만에 소진 예측). read latency 변동(block size·m-bit MLC 효과). C_max^WL variation 분석(160 chip, 19,200 block).
- §4 Overview of DEAR: key idea, SEE의 3 최적화(SAM/FEM/BPM) 개요.
- §5 Design of SEE: real-device characterization(160 chip, FPGA + temperature controller, JEDEC 표준, ECC_CAP=60 bits/1KiB). intra-layer similarity 검증(ΔC_x^i ≈ 1), C_FBC-error correlation(변동 ±8 bits), C_INT 산정.
- §6 DearSSD System Integration: C_FBC 측정 HW, interleaved monitoring, RR invocation flow, GC, overhead 분석.
- §7 Evaluation: 방법론, RR write, read latency, lifetime/WAF, monitoring overhead.
- §8 Related Work: read-hot data classification, inter/intra-block reliability variation, V_pass scaling(모두 DEAR와 orthogonal/보완).
- §9 Conclusion.

## 핵심 용어
- **Read-disturb**: read 시 비대상 WL에 인가된 V_pass가 셀을 soft-program해 V_th를 우측 shift시키고, 반복 read로 누적되면 bit error가 ECC 한계를 초과하는 현상.
- **Read-Refresh (RR)**: read-disturb로 인한 데이터 손실 전에 유효 페이지를 읽어 다른 free block으로 재기록(migrate)하는 절차. 추가 P/E를 유발.
- **C_max / C_max^WL**: block(또는 WL)이 견딜 수 있는 최대 read count. 기존은 최악 WL/block의 C_max로 고정.
- **C_FBC(ER)**: erased(ER) 상태의 fail-bit count. read-disturb 에러의 경량 proxy(single sensing으로 측정).
- **SEE (Swift Error Estimator)**: DEAR의 online WL-level 에러 추정기. SAM/FEM/BPM 3 최적화로 구성.
- **SAM (Similarity-Aware Monitoring)**: 같은 h-layer WL의 에러 유사성을 이용해 leading WL만 모니터링.
- **FEM (Fail-bit-assisted Error Monitoring)**: C_FBC(ER) single sensing으로 ECC decoding 없이 에러 추정(7배 latency 절감).
- **BPM (Block-wise Periodic Monitoring)**: read count가 C_INT 배수일 때만 block 단위 주기 모니터링.
- **E_TH / C_FBC^TH**: RR trigger 에러 임계값(ECC_CAP보다 보수적) / 이에 대응하는 fail-bit count 임계값.
- **C_INT**: 모니터링 주기(reads). E_TH와 overhead trade-off로 결정.
- **WAF (Write Amplification Factor)**: 내부 write/host write 비율. RR이 증가시킴.
- **DEAR+**: DEAR + Cocktail(read-hot 분산) 결합 기법.

## 강점 · 한계 · 열린 질문
- **강점**: WL 단위 동적 에러 추정으로 보수적 고정 C_max의 근본 한계를 해결. 실제 160개 chip 특성화로 뒷받침. ECC engine을 우회하는 single-sensing C_FBC 측정 + interleaved monitoring으로 overhead < 1.2%(IOPS -0.7%). HW 변경 없이 기존 명령으로 구현 가능. 기존 기법(Cocktail, V_pass scaling)과 orthogonal하게 결합 가능(DEAR+).
- **한계**: random write 워크로드(YCSB-B)에서는 GC 비효율로 수명 개선 효과 제한적. offline reference table에 의존(P/E 범위별 C_INT^TH, C_FBC^TH 사전 특성화 필요) — chip별/lot별 variation 추적 비용 미논의. NVMeVirt emulator 기반 평가로 실제 SSD 컨트롤러 통합 검증은 미흡. SAM은 intra-layer similarity 가정에 의존 — 극단적 결함 WL이 있을 경우 추론 오류 위험.
- **열린 질문**: QLC(C_max가 TLC 대비 3.15배 낮음)에서 더 짧은 C_INT가 필요한데 실측 효과는? aging 진행에 따라 reference table을 runtime에 재특성화/보정하는 메커니즘은? data retention error 등 read-disturb 외 다른 reliability 요인과의 상호작용은?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 기존 RR 기법의 근본 문제는 무엇인가?
> > 답: block 단위 고정 C_max를 사용하되, 그 값을 "모든 block이 보장 가능한" 최악 WL/block 기준으로 보수적으로 설정한다. 3D NAND의 심한 process variation(inter-block 69.7%, intra-block 2.23배 격차) 때문에 대부분 block/WL은 훨씬 많은 read를 견딜 수 있는데도 RR이 조기에 불필요하게 호출되어 성능·수명을 낭비한다.

> [!question]- Q2. read-disturb 에러가 ER 상태에 집중된다는 사실을 DEAR는 어떻게 활용하는가?
> > 답: soft-program 효과가 낮은 V_th 상태에서 크므로 전체 에러의 67%가 ER 상태에서 발생한다. 따라서 ER 상태의 fail-bit count C_FBC(ER)만 single sensing(V_REF^1)으로 측정하면 full page read와 ECC decoding 없이 전체 read-disturb 에러를 정확히 추정할 수 있다(FEM, monitoring latency 7배 절감).

> [!question]- Q3. SAM이 모니터링 비용을 줄이는 원리는?
> > 답: 같은 h-layer의 WL들은 channel hole 직경 등 물리 특성과 read voltage 조건이 거의 같아 read-disturb 에러가 거의 동일하다(ΔC_x^i ≈ 1). 따라서 h-layer당 leading WL 하나만 모니터링하고 나머지 WL은 추론해, 모니터링 대상 WL 수를 1/m(층당 WL 수)로 줄인다.

> [!question]- Q4. BPM에서 모니터링 주기 C_INT를 어떻게 안전하게 정하나?
> > 답: read-disturb는 read count에 따라 점진적으로 증가하므로 매 read가 아닌 C_INT 배수에서만 모니터링한다. 다음 모니터링 시점까지의 worst-case 누적 에러를 흡수하기 위해 RR trigger를 ECC_CAP보다 보수적인 E_TH로 낮추고, 그 safety margin(예: 5%) 내에서 견딜 수 있는 최소 read 수로 C_INT를 설정(2K P/E에서 19.3K reads).

> [!question]- Q5. C_FBC(ER)를 28μs에 측정할 수 있는 이유는?
> > 답: data randomization으로 모든 V_th 상태가 균등 분포(TLC에서 WL당 ER 셀 16K 고정)된다. V_REF^1 single sensing 후 '1' bit 수를 flash 내장 1s counter로 세고(20μs sensing + 8μs counting) ECC engine을 우회한다. 기존 special flash command로 가능해 HW 변경이 불필요하다.

> [!question]- Q6. interleaved monitoring이 필요한 이유는?
> > 답: 321 h-layer block 1회 전체 모니터링은 6.42ms로 user read와 충돌 시 큰 지연을 유발한다. 이를 작은 단위(WL당)로 쪼개 user read 사이에 분산(C_INT^WL = C_INT/h-layer수, 예: 19.3K/321마다 1 WL 검사)해 overhead를 amortize한다.

> [!question]- Q7. DEAR의 핵심 정량 성과는?
> > 답: RR-induced write 평균 64.2% 감소(최대 86.8%), 99.9th read latency 44.9% 개선, SSD 수명(WAF 기준) 36.7% 연장. DEAR+(Cocktail 결합)는 RR write를 최대 95.4%, tail latency를 더 크게 줄인다. monitoring overhead는 IOPS -0.7%, latency +1.3%에 불과.

> [!question]- Q8. DEAR가 한계를 보이는 워크로드와 그 이유는?
> > 답: write 비율이 높은 워크로드(예: YCSB-A, YCSB-B). write가 많으면 RR 전에 GC가 block을 회수해 read-disturb 영향이 줄어든다(read-intensive 대비 효과 감소). 특히 random write는 GC 효율이 낮아 WAF 개선이 제한적이다.

## 🔗 Connections
[[Reliability]] · [[MICRO]] · [[2025]]

## References worth following
- [59] Shim, Kim, Chun, Park, Kim. "Exploiting Process Similarity of 3D Flash Memory for High Performance SSDs." MICRO 2019 — DEAR의 intra-layer/process similarity 관찰의 직접적 토대.
- [66] Zhang, Deng, Pang, Yue, Zhu. "Cocktail: Mixing Data with Different Characteristics to Reduce Read Reclaims." IEEE TCAD 2022 — DEAR+의 결합 대상(read-hot 분산 기법).
- [39] Liao, Lu, Cai, et al. "Block Refresh Scheduling and Data Reallocation against Read Disturb in SSDs." ACM TECS — RR 절차 최적화 관련 baseline scheduling.
- [3] Cai et al. "An Integrated Approach for Managing Read Disturbs in High-Density 3D NAND Flash Memory." IEEE TCAD 2016 — V_pass scaling, orthogonal 기법.
- [33] NVMeVirt (Mark Lapedus/관련) — 평가에 사용된 SSD emulator.
- [18] Han, Cho, Lee, Chung. "Page Type-Aware Tuning Technique for Read Disturb Management of NAND Flash Memory." IEEE TVLSI 2023 — page type별 read-disturb 관리, 비교 관점.

## Personal annotations
<본인 메모 영역>
