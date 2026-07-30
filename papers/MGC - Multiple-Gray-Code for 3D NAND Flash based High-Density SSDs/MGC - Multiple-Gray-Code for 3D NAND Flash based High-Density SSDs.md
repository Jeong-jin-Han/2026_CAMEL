---
title: "MGC: Multiple-Gray-Code for 3D NAND Flash based High-Density SSDs"
description: "3D NAND QLC SSD에서 write-friendly한 unbalanced gray-code(UGC)와 read-friendly/reliable한 balanced gray-code(BGC)를 access-pattern·reliability-stage에 따라 block 단위로 동적 arbitration하는 FTL 기법"
venue: HPCA
year: 2023
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/reliability
  - venue/hpca
  - year/2023
  - list/26s-v2
  - topic/3d-nand-flash
  - topic/gray-code-encoding
  - topic/flash-translation-layer
  - topic/ssd-reliability
---

# MGC: Multiple-Gray-Code for 3D NAND Flash based High-Density SSDs

> **HPCA 2023** · cluster/reliability · Source: [MGC - Multiple-Gray-Code for 3D NAND Flash based High-Density SSDs.pdf](<MGC - Multiple-Gray-Code for 3D NAND Flash based High-Density SSDs.pdf>)

저자: Yina Lv (East China Normal University), Liang Shi (교신저자, Software/Hardware Co-design Engineering Research Center, Ministry of Education & East China Normal University), Qiao Li (School of Informatics, Xiamen University), Congming Gao (East China Normal University), Yunpeng Song (East China Normal University), Longfei Luo (East China Normal University), Youtao Zhang (Computer Science Department, University of Pittsburgh)

## TL;DR
QLC 이상의 고밀도 3D NAND는 4비트(LSB/CSB/MSB/TSB)를 16개 전압 레벨에 매핑하는 gray-code 선택에 따라 read/program 성능과 reliability가 크게 갈리는데, 기존 SSD는 제조사가 정한 **고정된 gray-code 하나**만 쓴다. 이 논문은 write에 유리한 unbalanced gray-code(UGC, 예: GC(1,2,6,6))와 read/reliability에 유리한 balanced gray-code(BGC, 예: GC(3,4,4,4))를 블록 단위로 동적으로 섞어 쓰는 **MGC**를 제안한다. 애플리케이션의 access pattern(read-only/write-only/hot-read)과 SSD의 reliability 단계(Young/Middle/Old, retry 횟수 기반)를 동시에 고려해 어떤 블록을 어느 gray-code로 인코딩할지 실시간 arbitration하는 MGC-FTL을 firmware/FTL 양쪽에 구현했다. SSDsim 기반 시뮬레이션과 실제 176-layer QLC 칩 테스트 데이터로 검증한 결과, read latency를 UGC_RLV 대비 평균 26%(Middle stage 최대 51%) 개선하고 BGC 대비 Old stage에서 7.4% 개선하면서도 write 성능과 lifetime은 거의 그대로 유지한다.

## 문제 & 동기
3D NAND는 층수(24층→220+층, p.1)와 셀당 비트 수(1→4비트, QLC/PLC/HLC 등장, p.1)가 계속 늘면서 reliability와 성능이 함께 악화된다. 업계는 이를 완화하기 위해 (1) 셀을 두 단계에 나눠 프로그램하는 two-step programming(TSP)과 (2) 4비트 값을 16개 전압 레벨에 매핑하는 gray-code(GC) 인코딩, (3) LDPC ECC를 채택했는데, 제조사마다 서로 다른 gray-code를 고정적으로 쓰고 있어(Table I, p.2) 성능/reliability 특성이 제각각이다.

- read 성능은 gray-code의 nSENSE(필요 reference voltage 개수)에 좌우되고 read retry가 늘면 LDPC 지연도 커진다(식 1-2, p.4).
- write 성능은 호환 가능한 TSP에 좌우된다 — TSP(4,16)은 TSP(16,16) 대비 프로그래밍 지연을 약 60%, 전체 write latency를 약 22% 줄인다(p.3).
- 그러나 **하나의 gray-code로는 read/write 성능을 동시에 최적화할 수 없다** (Observation #2, p.6): GC(1,2,6,6)/TSP(4,16)이 최고 write 성능을 내지만, reliability가 나빠지면 GC(3,4,4,4)/TSP(16,16)의 read latency가 GC(1,2,6,6) 대비 45%에 불과할 정도로 격차가 커진다(p.6).
- 게다가 **워크로드별로 최적 gray-code가 다르다** (Observation #3, p.6): read-dominant 워크로드(예: hm_1)는 BGC(GC(3,4,4,4))가, write-dominant 워크로드(예: prxy_0)는 UGC(GC(1,2,6,6))가 I/O 성능이 가장 좋다.

> [!quote]- 📄 원문 표현 (paper)
> - "Unfortunately, a fixed gray-code encoding design lacks the ability to meet the dynamic read and program performance requirements at both application and device levels." (p.1)
> - "It is impossible for a single gray-code to achieve excellent read and write performance at the same time." (p.6, Observation #2)
> - "Using a fixed gray-code cannot guarantee the I/O performance of workloads with different access patterns." (p.6, Observation #3)

## 핵심 통찰 (Key Insight)

**1. Gray-code 선택은 근본적으로 read-write-reliability 3자 trade-off다.** LSB/CSB/MSB/TSB 각 페이지를 몇 개의 reference voltage(nSENSE)로 구분하느냐가 gray-code마다 다르며(Table II, p.4), read 성능이 페이지마다 다른(unbalanced, UGC) 코드는 최소 read 횟수(예: LSB 1회)로 빠른 read를 내지만 TSB 등 일부 페이지는 매우 느리고, 4비트가 균등하게 어려운(balanced, BGC) 코드는 페이지 간 read 성능이 고르지만 프로그래밍이 느린 TSP(16,16)만 지원한다. UGC/BGC를 정적으로 하나만 고르면 이 trade-off의 한쪽만 취할 수밖에 없다.

**2. Reliability는 flash 수명 동안 정적이지 않고 Young→Middle→Old 세 단계로 열화되며, 단계마다 최적 gray-code가 바뀐다.** Young stage에는 어떤 gray-code든 retry가 거의 없어 write에 유리한 UGC가 최선이지만, Middle stage에 들어서면 UGC 블록의 MSB/TSB 페이지부터 retry가 발생하기 시작하고, Old stage에서는 UGC 블록 자체가 신뢰할 수 없어져 BGC로 전환해야 한다(p.5-6, Fig.3-4).

**3. Read-only 데이터가 전체 read의 85%, write-only 데이터가 전체 write의 91%를 차지한다는 access-pattern locality를 활용하면(p.7), per-page hotness 추적만으로 UGC/BGC 배치를 실시간 조정할 수 있다** — 즉 초기엔 BGC로 기본 배치하고 write가 감지되면 UGC로 바꾸며, 이후 read가 잦아진 UGC의 MSB/TSB 페이지(Type-1 page)만 선택적으로 BGC의 LSB/CSB로 옮기면(hot read data transformation) 변환 비용을 최소화하면서 read 성능을 얻는다.

> [!quote]- 📄 원문 표현 (paper)
> - "Studies have shown that BGC is more robust in terms of reliability because the four bits of a QLC flash cell have similar reference voltages and it requires fewer read retries than that of UGC at degraded reliability." (p.4)
> - "MGC-FTL exploits the reliability characteristic for better gray-code selection. With reliability degradation, we gradually use the gray-code with high reliability, e.g., BGC, to replace the less reliable gray-code, e.g., UGC." (p.6)
> - "Prior studies from Li et al. [35][47] and Lv et al. [47] revealed that read-only data takes 85% of read requests, and write-only data takes 91% of write requests, on average." (p.7)

## 설계 / 메커니즘 (Design)

**아키텍처 개요 (Fig.7, p.6):** MGC는 SSD Controller의 Host Interface Layer 아래 FTL에 **MGC-FTL**을 두고(garbage collection·wear leveling·LDPC와 나란히), Flash Interface Layer에는 UGC block/BGC block을 함께 두며 flash controller에 Information Encoder/Decoder(GCB, gray-code bitmap 참조)를 추가한다. 대표 gray-code로 UGC=GC(1,2,6,6)/TSP(4,16), BGC=GC(3,4,4,4)/TSP(16,16) 두 가지만 채택해(p.5) 구현 복잡도를 낮췄고, gray-code는 **block 단위**로만 적용된다(한 블록 안에서는 페이지마다 다른 gray-code를 못 씀, p.7).

- **Firmware(§V, p.7):** flash controller가 TSP(4,16)/TSP(16,16) 두 프로그래밍 시퀀스와 두 인코딩 테이블(각 16-entry)을 모두 지원하도록 확장. 인코딩 테이블 오버헤드는 flash area의 0.07% 미만(p.7, Fig.8).
- **GCB (gray-code bitmap):** 블록마다 어떤 gray-code를 쓰는지 DRAM에 캐시. 1TB SSD·16MB block 기준 bitmap 크기는 8KB(p.7).
- **① Access Characteristics Guided Gray-Code Arbitration (§VI-B, p.7-8):**
  - *Initial Gray-Code Arbitration*: 데이터가 처음 쓰일 때는 기본적으로 BGC를 채택하고, 이후 write가 이어지면 UGC로 전환(전환 비용은 첫 write 1회뿐).
  - *Hot Read Data Transformation*: read latency에 따라 페이지를 Type-1(UGC의 MSB/TSB, latency 높음)·Type-2(BGC 전체, 중간)·Type-3(UGC의 LSB/CSB, latency 낮음)로 분류하고, read(또는 LDPC retry) 횟수가 threshold $T_{UR}$(Type-1↔2 전환 기준, 더 공격적)/$T_{BR}$(Type-2↔3, 더 보수적)을 넘으면 UGC 블록의 hot Type-1 데이터를 BGC 블록으로 옮긴다(p.8).
  - Garbage collection·wear leveling도 조정: BGC 블록은 read-dominant라 invalid page가 적어 GC 대상이 잘 안 되고, UGC 블록은 더 자주 지워지므로 dynamic wear leveling 시 지워진 블록을 BGC 역할로 교대 배정하고 static wear leveling은 erase count 최소인 BGC 블록을 고른다(p.8).
- **② Reliability-Stage Aware Gray-Code Arbitration (§VI-C, Fig.9, p.8-9):**
  - *Young stage*: 모든 데이터를 UGC+TSP(4,16)로 기본 인코딩 — write/read 모두 우수, 변환 비용 0.
  - *Middle stage*: 첫 write는 여전히 UGC, 단 UGC 블록 MSB/TSB의 read count가 $T_{UR}$을 넘으면 BGC 블록으로 이전. update는 BGC 블록에 재기록.
  - *Old stage*: UGC(TSP(4,16))는 더 이상 reliable하지 않다고 판단해 초기 배치부터 BGC를 사용하고, UGC 블록은 read-only 폴백으로만 남긴다(LSB/CSB만 여전히 신뢰 가능하므로 hot read 데이터를 이 두 페이지로만 이전).

> [!quote]- 📄 원문 표현 (paper)
> - "We adopt the following gray-code selection policy. If the data is identified as read-only, BGC should be adopted to optimize read performance. If the data is identified as write-only, UGC should be adopted to optimize write performance." (p.7)
> - "For the Young stage, pages programmed with TSP(4,16) and encoded with UGC have superior write performance and comparable read performance to that of BGC. Then, we propose to use UGC as the default gray-code." (p.8)
> - "We conducted a similar analysis of storage overhead as that in [38], one mapping table costs less than 0.07% of the flash area." (p.7)

## 평가 (Evaluation)

**셋업(§VII-A, p.9-10):** SSDsim을 reliability model과 3D QLC 특화 기능으로 확장해 사용(Table IV(a): 4채널·2칩/채널·4플레인/칩·2048블록·1024페이지/블록·16KB페이지). 실제 YEESTOR 플랫폼의 176-layer QLC 칩(1K P/E cycle로 erase, baking으로 retention 가속) 테스트 데이터를 이용해 RBER→retry 횟수 모델(식 3-4)을 검증하고 Young/Middle/Old 단계별 retry 횟수를 Table IV(b)에 확정. 워크로드는 MSR 10종 + YCSB(RocksDB) 2종, 총 12개(Table IV(c)). 비교 대상은 UGC, UGC_RLV([47], read latency variation-aware), BGC, MGC(access-guided만), MGC_Stage(access+reliability-stage 모두 적용) 5가지.

- **Read 성능(Fig.11, p.11):** Middle stage에서 BGC가 대부분 워크로드에서 UGC보다 크게 우수; MGC_Stage는 UGC_RLV 대비 평균 26%, 최대 51% read latency 개선. Old stage에서는 MGC_Stage가 BGC 대비 평균 7.4% 개선.
- **Tail latency(Fig.14, p.11):** Middle stage 99th percentile 기준 BGC가 UGC 대비 평균 42% 감소, UGC_RLV는 UGC 대비 평균 12% 감소, MGC_Stage는 UGC 대비 두 nines에서 39% 감소.
- **Write 성능(Fig.12, p.11):** Middle stage에서 BGC/TSP(16,16)는 UGC/TSP(4,16) 대비 12-31% 더 높은 write latency. MGC/MGC_Stage는 첫 write만 BGC, 이후 update는 UGC로 arbitration하므로 write 성능이 UGC와 비슷.
- **I/O 성능(Fig.13, p.11):** read-intensive 워크로드(hm_1 등)와 write-intensive 워크로드(prxy_0 등) 모두에서 MGC/MGC_Stage가 개선을 달성.
- **Lifetime/Capacity(Fig.15-16, p.12):** MGC_Stage의 WAF(write amplification factor) 증가는 기존 기법 대비 작고, Old stage에서 capacity 감소는 워크로드별 0.6%-7.8%, 평균 1.7% (UGC 블록 일부만 read-only 용도로 남기기 때문).
- **Sensitivity(Fig.17, p.12):** $T_{BR}$을 2~8로 바꿔도 전체 migration 비용은 0.1%-0.8% 수준으로 작음; 논문은 $T_{BR}=3$을 채택.
- **Garbage collection(Fig.18, p.12):** GC 유무와 무관하게 MGC_Stage의 read/write/IO latency 결론은 동일.

> [!quote]- 📄 원문 표현 (paper)
> - "To sum up, compared with UGC_RLV, MGC improves the read performance by 26% on average and up to 51% at the Middle stage. At the Old stage, the read performance of MGC_Stage can be improved by 7.4% on average compared with BGC." (p.11)
> - "MGC_Stage can reduce the read latency by 39% at two nines compared with UGC." (p.11)
> - "The results show that the reduced capacity is around 0.6%-7.8% and 1.7% on average." (p.12)

## 섹션 노트
- **I. Introduction**: 3D NAND 고밀도화(층수·비트/셀 증가)로 성능/reliability가 악화되는 배경과 gray-code/TSP/LDPC 조합 문제를 제기하고 MGC의 3대 기여(성능 분석, MGC-FTL 인코딩 설계, SSDsim 평가)를 요약(p.1-2).
- **II. Background**: 3D NAND 구조(block/wordline/page, Fig.1)와 TSP(Fig.2, TSP(16,16)/(8,16)/(4,16)/(2,16))·read 동작·gray-code(Table II)·UGC/BGC 구분(Table III)을 정의(p.2-4).
- **III. Motivation**: reliability를 Young/Middle/Old 3단계로 나누고, 세 가지 Observation(#1 read latency 변동, #2 read/write 동시 최적화 불가, #3 워크로드별 최적 gray-code 상이)을 실험으로 제시(Fig.3-6, p.4-6).
- **IV. The Overview of MGC Flash**: MGC의 전체 아키텍처(Fig.7)와 MGC-FTL의 두 기법(access-guided, reliability-stage-aware) 개요(p.6-7).
- **V. Firmware Design for MGC Flash**: 인코딩 테이블 다중 지원과 GCB bitmap 설계, 오버헤드 분석(p.7).
- **VI. FTL for MGC Flash Memory**: initial arbitration/hot read transformation(§B)과 reliability-stage arbitration(§C, Fig.9), garbage collection·wear leveling 적응(p.7-9).
- **VII. Evaluation**: 실험 셋업·reliability modeling(RBER→retry, 식 3-4)·real chip 검증·5개 스킴 비교 결과(p.9-12).
- **VIII. Related Work**: RBER reduction, LDPC optimization, latency variation optimization, coding optimization(WOM codes 등) 네 갈래로 선행연구를 정리하며 MGC를 "3D NAND 고밀도 SSD를 위한 multiple-gray-code를 설계한 첫 연구"로 위치시킴(p.12).
- **IX. Conclusion**: MGC가 flash controller가 runtime에 적절한 gray-code를 선택하도록 해 성능과 lifetime을 함께 개선했다고 요약(p.12).

## 핵심 용어 (Key terms)
- **Gray-code (GC(A,B,C,D))**: n비트 셀 값을 $2^n$ 전압 레벨에 매핑하는 인코딩. A/B/C/D는 LSB/CSB/MSB/TSB 페이지를 읽는 데 필요한 최소 flash read 횟수.
- **UGC (unbalanced gray-code)**: 페이지 간 read 성능 격차가 큰 gray-code(예: GC(1,2,4,8), GC(1,2,6,6)). write에 유리하나 reliability 열화에 취약.
- **BGC (balanced gray-code)**: 4개 페이지의 read 성능이 고른 gray-code(예: GC(3,4,4,4)). read/reliability에 유리하나 TSP(16,16)만 지원.
- **TSP(2^m, 2^n)**: n비트/셀을 2단계로 프로그램 — 1단계에서 $2^m$ 레벨까지 coarse하게, 2단계에서 나머지를 fine하게 프로그램.
- **nSENSE**: 한 페이지를 read할 때 필요한 reference voltage(sensing) 개수. read latency $tR = nSENSE \times tSENSE$.
- **RBER (Raw Bit Error Rate)**: LDPC 보정 전 원시 비트 오류율. retry 횟수 모델링의 기반.
- **MGC-FTL**: 여러 gray-code를 동시에 지원하도록 확장한 flash translation layer. access-guided/reliability-stage-aware 두 arbitration 기법을 포함.
- **GCB (Gray-Code Bitmap)**: 블록별 채택 gray-code를 기록하는 비트맵, DRAM 캐시(1TB SSD 기준 8KB).
- **Reliability stage (Young/Middle/Old)**: LDPC retry 횟수 기준으로 나눈 flash 수명 단계. Young=retry 거의 없음, Middle=UGC MSB/TSB부터 retry 시작(SSD 수명 대부분을 차지), Old=UGC 신뢰 불가.
- **Type-1/2/3 page**: hot read data transformation에서 read latency 기준 분류 — Type-1(UGC MSB/TSB, 느림), Type-2(BGC 전체, 중간), Type-3(UGC LSB/CSB, 빠름).

## 강점 · 한계 · 열린 질문
- **강점**: 기존 하드웨어(gray-code 인코딩 테이블, TSP)의 조합 공간을 그대로 재활용해 block-granularity로 구현 복잡도를 낮췄고, 인코딩 테이블 오버헤드(<0.07% flash area)와 GCB(8KB/1TB)가 무시할 만큼 작다(p.7). 실제 176-layer QLC 칩의 RBER/retry 데이터로 reliability model을 검증해(p.9-10) 순수 시뮬레이션 가정에만 의존하지 않는다.
- **한계**: 실제 SSD 프로토타입이 아닌 SSDsim 기반 시뮬레이션 평가이며, 대표 gray-code로 UGC=GC(1,2,6,6)/BGC=GC(3,4,4,4) 단 두 가지만 채택해(p.5) 더 넓은 gray-code 조합 공간은 탐색하지 않았다. Old stage에서는 capacity가 워크로드별 최대 7.8% 줄어드는 트레이드오프가 있다(p.12, Fig.16). retry 횟수-reliability 관계는 Gaussian 분포로 단순화했고(p.10), $T_{UR}$/$T_{BR}$ 같은 threshold는 heuristic 튜닝에 의존한다(p.8, p.12).
- **열린 질문**: page/plane/chip 단위의 더 세밀한 granularity로 MGC를 확장하면 이득이 더 커질지(저자도 future work로 남김, p.7); WOM code나 LDPC 최적화 등 §VIII에서 언급된 직교적인 기법과 결합 시 효과; PLC(5비트)/HLC(6비트) 등 더 많은 비트/셀로 확장했을 때 UGC/BGC trade-off의 양상이 어떻게 바뀔지.

## ❓ Q&A (자가 점검)
> [!question]- UGC와 BGC의 근본적 차이는 무엇이고 왜 하나만으로는 부족한가?
> UGC는 페이지별 read 성능이 불균등해(예: LSB 1회, TSB 6회 read) write 성능이 좋은 TSP와 호환되지만 reliability 열화 시 read retry가 특정 페이지에 몰린다. BGC는 4개 페이지의 read 성능이 고르고 reliability에 강하지만 느린 TSP(16,16)만 지원한다. 워크로드마다 read/write 비율과 SSD 수명 단계가 다르므로 정적으로 하나만 쓰면 항상 손해를 본다(Observation #2/#3, p.6).

> [!question]- MGC-FTL은 어느 granularity로 gray-code를 적용하는가?
> Block 단위다. 페이지 단위로 다른 gray-code를 섞으면 controller의 프로그래밍 pulse 폭/높이 제어가 복잡해지므로, 한 블록은 지워질 때까지 하나의 gray-code(및 그에 대응하는 TSP)만 사용한다(p.7).

> [!question]- Reliability stage는 어떻게 정의·구분하는가?
> LDPC retry 발생 여부와 위치로 정의한다. Young=어떤 gray-code든 retry 없음, Middle=UGC 블록의 MSB/TSB 페이지부터 retry 시작(BGC는 아직 reliable), Old=UGC 블록의 MSB/TSB에서 retry가 threshold(논문에서 8)를 초과하는 상황(§III, p.5).

> [!question]- Hot Read Data Transformation은 어떤 기준으로 데이터를 옮기는가?
> 페이지를 read latency 기준 Type-1(UGC MSB/TSB)/Type-2(BGC 전체)/Type-3(UGC LSB/CSB)로 나누고, read(retry) 횟수가 threshold $T_{UR}$(Type-1→2 전환, aggressive) 또는 $T_{BR}$(Type-2→3, conservative)를 넘으면 QLC를 다시 써서(P/E cycle 소모) 더 빠른 페이지로 옮긴다. 이 threshold는 재기록 비용 대비 향후 read 이득이 클 때만 트리거되도록 설계됐다(p.8).

> [!question]- Old stage에서 UGC 블록은 완전히 못 쓰게 되는가?
> 아니다. UGC 블록의 LSB/CSB 페이지는 Old stage에서도 여전히 reliable하므로, hot read 데이터를 이 두 페이지에만 옮겨 read 성능 최적화에 재활용한다. 다만 MSB/TSB는 신뢰할 수 없어 새 데이터의 초기 배치는 BGC로 전환한다(§VI-C-3, p.8-9).

> [!question]- 평가에서 write 성능은 왜 MGC/MGC_Stage가 UGC와 비슷하게 나오는가?
> MGC는 데이터의 첫 write에만 BGC를 쓰고(reliability 확보를 위해) 이후 update는 UGC로 arbitration하기 때문에, write-heavy 워크로드에서는 실질적으로 UGC와 유사한 write latency를 낸다(p.11).

> [!question]- 실험에서 사용한 reliability model의 핵심 변수는 무엇인가?
> $\alpha$(RBER 계수), $nSENSE$(gray-code별 필요 reference voltage 수), $\delta$(retry당 RBER 감소 비율, 논문은 20%로 설정), $E_{LDPC}$(LDPC가 보정 가능한 최대 오류 비트 수)를 이용해 필요 최소 retry 수 $nRETRY$를 식 3-4로 유도하고, 실제 176L QLC 칩 데이터로 이 모델을 보정했다(p.9-10).

> [!question]- MGC가 관련 연구들과 차별화되는 지점은 무엇인가?
> RBER reduction, LDPC 최적화, latency variation 최적화, coding 최적화(WOM code 등) 각 방향의 선행연구는 존재하지만, 이들은 gray-code 자체를 여러 개 동시에 runtime에 조합해 3D NAND 고밀도 SSD의 동적 성능/reliability 요구를 맞추는 시도는 하지 않았다는 점에서 MGC가 최초라고 저자들은 주장한다(§VIII, p.12).

## 🔗 Connections
[[Reliability]] · [[HPCA]] · [[2023]]
관련: [[ColdCode - Cold Data Encoding for Enhanced Reliability and Lifetime in 3D NAND Flash]] · [[Midas Touch - Invalid-Data Assisted Reliability and Performance Boost for 3d High-Density Flash]] · [[STRAW - Stress-Aware WL-Based Read Disturbance Management for High-Density NAND Flash Memory]]

## References worth following
- Khakifirooz, A. et al., "A 1Tb 4b/Cell 144-Layer Floating-Gate 3D NAND Flash Memory with 40MB/s Program Throughput and 13.8 Gb/mm2 Bit Density," ISSCC 2021 [26] — TSP(4,16)을 실제로 제안한 Intel/Micron 논문으로 MGC의 write-fast TSP 근거.
- Shibata, N. et al., Toshiba/Kioxia, TSP(8,16) 제안 논문, ISSCC 2019 [60] — MGC가 비교하는 TSP 계열의 또 다른 대표 사례.
- Lv, Y. et al., "Latency variation aware read performance optimization on 3D high-density SSD," GLSVLSI 2020 [47] — 이 논문의 baseline인 UGC_RLV의 원 논문이자 저자(Lv)의 선행 연구.
- Cai, Y. et al., "Threshold voltage distribution in MLC NAND flash memory: Characterization, analysis, and modeling," DATE 2013 [27]; Papandreou, N. et al., "Reliability of 3D NAND flash memory with a focus on read voltage calibration from a system aspect," NVMTS 2019 [56] — 논문이 직접 인용하는 RBER·retry modeling(식 3-4)의 근거.
- Zhao, K. et al., "LDPC-in-SSD: Making advanced error correction codes work effectively in solid state drives," FAST 2013 [71] — LDPC read-retry 비용 모델(식 2)의 근거.

## Personal annotations
<!-- 본인 메모 영역 -->
