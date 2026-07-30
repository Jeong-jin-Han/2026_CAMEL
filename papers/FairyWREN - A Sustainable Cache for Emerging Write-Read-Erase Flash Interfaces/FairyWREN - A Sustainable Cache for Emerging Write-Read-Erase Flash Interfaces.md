---
title: "FairyWREN: A Sustainable Cache for Emerging Write-Read-Erase Flash Interfaces"
description: "WREN(Write-Read-Erase, 예: ZNS/FDP) 인터페이스에서 garbage collection과 cache admission/eviction을 'nest packing'으로 통합해 flash 쓰기를 12.5배 줄이고, 더 조밀한 flash(QLC/PLC) 사용을 가능케 해 탄소배출·비용을 각각 33%·35% 절감하는 flash cache"
venue: "ACM Transactions on Storage (TOS)"
year: 2025
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/zns
  - topic/zns
  - topic/flash-cache
  - topic/write-amplification
  - topic/garbage-collection
  - venue/tos
  - year/2025
  - list/26s-v2
---

# FairyWREN: A Sustainable Cache for Emerging Write-Read-Erase Flash Interfaces

> **ACM Transactions on Storage (TOS) 2025** · cluster/zns · Source: [FairyWREN - A Sustainable Cache for Emerging Write-Read-Erase Flash Interfaces.pdf](<FairyWREN - A Sustainable Cache for Emerging Write-Read-Erase Flash Interfaces.pdf>)

저자: Sara McAllister (Computer Science, Carnegie Mellon University), Yucong Wang (Computer Science, Carnegie Mellon University and Salesforce Inc.), Benjamin Berg (Computer Science, The University of North Carolina at Chapel Hill), Daniel S. Berger (Microsoft Research and University of Washington), Nathan Beckmann (Computer Science, Carnegie Mellon University), George Amvrosiadis (Carnegie Mellon University College of Engineering), Gregory R. Ganger (Electrical and Computer Engineering, Carnegie Mellon University)

## TL;DR
데이터센터 flash cache는 write 부하가 많아 flash의 제한된 write endurance를 빠르게 소진시키고, 이는 flash 교체 주기를 단축시켜 embodied carbon 배출을 늘린다. 기존 LBAD(Logical Block-Addressable Device) 인터페이스는 device 내부 garbage collection(GC)을 캐시로부터 숨겨 캐시가 GC로 인한 추가 쓰기(device-level write amplification, DLWA)를 전혀 제어할 수 없게 만든다. FairyWREN은 Erase를 first-class 연산으로 노출하는 새 인터페이스군인 WREN(Write-Read-Erase iNterfaces, 예: ZNS·FDP)을 이용해, GC 시 쓰는 EU(erase unit)를 캐시 admission/eviction 기회로 재사용하는 "nest packing"과 hot–cold object 분리를 결합한다. 그 결과 최신 계층형 캐시 Kangaroo 대비 flash 쓰기를 12.5배 줄이며, 이를 통해 더 조밀한(QLC/PLC) flash를 안전하게 사용할 수 있게 되어 flash 비용을 35%, 탄소배출을 33% 절감한다. 즉 이 논문은 "인터페이스만 바꾼다고 되는 게 아니라 캐시 설계 자체를 WREN에 맞춰 재설계해야 한다"는 것을 보이는 co-design 연구다.

## 문제 & 동기
데이터센터 탄소배출 중 embodied emissions(제조·운송·폐기)는 재생에너지 전환이 진행될수록 비중이 커지는데(p.2), 그중 flash memory는 서버 embodied carbon의 40%를 차지한다(p.2). Embodied emissions를 줄이는 핵심 방법은 flash 수명을 늘리는 것인데, 최신 flash일수록(TLC→QLC→PLC) 셀 당 비트 수가 늘어 write endurance가 급격히 낮아진다(예: PLC는 TLC 대비 write endurance가 16%에 불과, p.6 Table 5 기준). 즉 6년 수명의 2-TB QLC drive는 초당 14 MB, 가용 대역폭의 0.09%만 쓸 수 있을 정도로 write budget이 타이트하다(p.2). 그런데 캐시는 근본적으로 write-intensive한 워크로드이고(hit rate 유지를 위해 계속 새 object를 admission해야 함), 게다가 flash는 4KB page 단위로만 쓰고 MB~GB 단위 region 단위로만 지울 수 있어 이 granularity mismatch가 device-level write amplification(DLWA)을 유발한다(p.5). 현재 지배적인 LBAD 인터페이스는 GC를 device firmware 안에 완전히 숨겨, 캐시가 GC로 인한 추가 쓰기를 전혀 제어하지 못한다 — 최신 state-of-the-art 캐시 Kangaroo조차 DLWA를 2~10배까지 유발한다(p.3). ZNS·FDP 같은 새 인터페이스가 Erase를 노출하지만, 이 논문은 "인터페이스를 바꾸는 것만으로는 쓰기가 줄지 않으며, 캐시를 그 인터페이스에 맞춰 재설계해야 한다"는 점을 정량적으로 보인다(p.9-12).

> [!quote]- 📄 원문 표현 (paper)
> - "Datacenters need to reduce embodied carbon emissions, particularly for flash, which accounts for 40% of embodied carbon in servers." (p.1)
> - "To achieve a six-year lifetime on a 2-TB quad-level cells (QLC) drive, the application can write only 14 MB/s, or 0.09% of available write bandwidth (Section 2)." (p.2)
> - "Flash caches must be re-designed to leverage the additional control provided by WREN." (p.3)
> - "Kangaroo cannot control device-level write amplification." (p.9)

## 핵심 통찰 (Key Insight)
1. **Nest packing — GC와 cache admission/eviction의 통합.** WREN에서는 EU를 Erase하기 전 host가 직접 GC(live data 재기록)를 수행해야 한다. FairyWREN은 이 GC 재기록 시점에 evict될 객체 자리에 새 객체를 함께 채워 넣어, "GC를 위한 쓰기"와 "admission을 위한 쓰기"를 한 번의 쓰기로 합친다. LBAD에서는 device가 GC를 알아서 하기 때문에 이 통합이 원천적으로 불가능했다 — 이것이 WREN이 있어야만 가능한 핵심 이유다(p.3, p.15).
2. **Deathtime/popularity에 따른 데이터 그룹핑(hot–cold set partitioning).** 같은 EU 안의 데이터가 비슷한 lifetime(자주 쓰이거나 자주 evict됨)을 가지면 GC 시점에 EU가 "거의 다 죽어있거나 거의 다 살아있는" 상태가 되어 재기록할 데이터가 적어진다. FairyWREN은 각 set을 hot subset(자주 접근/오래 생존)과 cold subset(새로 admission된, 죽을 확률 높음)으로 분리해 저장함으로써 nest packing만으로는 부족한 write reduction을 추가로 확보한다(p.15-16). Nest packing만으로 3.7배, hot–cold 분리 추가로 3.4배 감소(p.27).
3. **Slicing + double buffering으로 DRAM 오버헤드 최소화.** WREN 장치는 동시에 활성화(active)할 수 있는 EU 수가 4개 안팎으로 제한되는데, 캐시는 64개의 독립된 log-structured slice를 유지하고 싶어한다. FairyWREN은 하나의 활성 EU를 64개 slice가 공유하는 segment로 나누고(slicing), fragmentation을 줄이기 위해 primary/overflow의 double buffering을 적용해, 이론적 분석(balls-and-bins 모델)으로 예측한 fragmentation을 1%대로 낮춘다(p.17-19).

> [!quote]- 📄 원문 표현 (paper)
> - "The main insight in FairyWren is that every flash write, whether from the application or from garbage collection, is an opportunity to admit objects to the cache." (p.3)
> - "FairyWren uses WREN's control over data placement and garbage collection to reduce writes in two main ways... FairyWren groups data with similar lifetimes into the same EU." (p.13)
> - "Using both simulation, we find that this optimization limits the capacity loss from fragmentation to <1%, even for small (16 MB) buffers." (p.19)

## 설계 / 메커니즘 (Design)
FairyWREN은 CacheLib(Meta의 오픈소스 캐시 엔진, ref [16])의 flash cache 모듈로 구현되며, 캐시 용량을 **large-object cache (LOC)**와 **small-object cache (SOC)**로 분할한다(2 KB 기준, Figure 7, p.13-14).

- **LOC**: 2KB 초과 object를 담당하는 단순 log-structured 캐시. EU 크기의 in-memory segment buffer에 순차로 쌓다가 EU가 차면 통째로 flash에 쓰고, eviction 시 해당 EU를 그대로 Erase한다(WA ≈ 1×, p.14).
- **SOC**: FwLog와 FwSets의 2단 계층(Kangaroo[67,68] 구조 계승, p.14). FwLog는 DRAM 오버헤드가 상대적으로 큰 log-structured buffer로 SOC 용량의 약 5%만 차지하고, 나머지 95%는 set-associative store인 FwSets가 담당한다. FwSets는 개별 object가 아니라 **set 자체**를 log-structured store에 저장해 DRAM 인덱스 크기를 object당 몇 바이트 수준으로 줄인다(p.14).
- **Nest packing (Figure 8, p.15)**: FwLog나 FwSets 중 공간이 부족한 쪽에서 evict할 victim EU를 정하면, ① victim EU를 메모리로 읽고 ② FwLog에서 그 victim set들에 매핑되는 object를 찾아 ③ 새 set을 구성해 FwSets의 log 끝에 append로 다시 쓴 뒤 ④ victim EU를 Erase한다. 이 과정에서 FwLog에서 나가야 할 evict 대상 object들이 "공짜로" 함께 반영된다.
- **Hot–cold set partitioning (Figure 9, p.16)**: 각 set을 hot subset(자주 쓰임, RRIP 기반 popularity 추정)과 cold subset(새로 들어와 인기 여부 불확실)으로 나누어 별도 log-structured store로 관리. 매 삽입마다 cold subset만 쓰고, n번(논문 기본값 n=5)마다 두 subset을 merge-and-resort해 재분배함으로써 인기 객체는 재기록을 피한다(예: n=5, 8KB set 기준 40%의 쓰기 절감, p.16).
- **Slicing + double buffering (Figures 10-12, p.17-19)**: FwLog·FwSets 모두 64개(FwSets는 8개, p.19)의 독립 slice를 하나의 활성 EU segment 안에서 공유하도록 슬라이스 단위로 나눈다. 단순 단일 buffer는 20% 넘는 내부 fragmentation을 유발할 수 있어(balls-and-bins 근사로 이론적으로 분석, p.17-18), primary/overflow 이중 buffer로 fragmentation을 <1%까지 낮춘다.
- FairyWREN은 총 4개의 동시 active EU만 요구한다(LOC 1개, FwLog 1개, FwSets hot/cold 각 1개, p.16) — 대부분의 WREN 장치가 지원하는 범위 내.
- DRAM 오버헤드: 2TB 캐시 기준 Kangaroo 대비 19% 많은 DRAM(1.5GB 증가, object당 8.3 bits vs Kangaroo 7.0 bits, Table 3 p.20)만으로 12.5배 쓰기 절감을 달성.

> [!quote]- 📄 원문 표현 (paper)
> - "FairyWren partitions its capacity into a large-object cache (LOC) and a small-object cache (SOC)... The large-object cache (Section 5.2) stores objects larger than 2 KB." (p.13)
> - "FwSets... does not track individual objects, since this would incur too much DRAM overhead." (p.14)
> - "FairyWren currently needs four simultaneously active EUs: one for LOC, one for FwLog, and two for FwSets (one for the hot subsets and one for the cold subsets)." (p.16)
> - "FairyWren uses 19% more DRAM than Kangaroo, a 1.5-GB DRAM overhead increase for a 2-TB cache." (p.19)

## 평가 (Evaluation)
평가는 (i) 실제 flash 장치(Western Digital Ultrastar DC ZNS540 1-TB ZNS SSD, EU=1077 MiB, 3.5 device writes/day 정격, p.20)를 이용한 on-flash 실험과 (ii) 21일 Meta trace(6TB 고유 바이트, compulsory miss ratio 13.8%) 및 7일 Twitter trace(3.5TB, compulsory miss ratio 17.2%)를 이용한 시뮬레이션으로 구성되며(p.20-21), baseline은 state-of-the-art 계층형 캐시 **Kangaroo**(SOSP'21/TOS'22)다.

- **쓰기·WA 감소 (Figure 14, p.23)**: 동일 miss ratio(FairyWREN 0.575 vs Kangaroo 0.594) 조건에서 flash write rate가 97 MB/s → 7.8 MB/s로 **12.5배** 감소, write amplification은 23× → 1.89×로 **12.2배** 감소.
- **성능 (p.23)**: 동일 부하에서 FairyWREN 처리량 104 KOps/s vs Kangaroo 40.5 KOps/s. 99th-percentile latency는 FairyWREN 170 µs vs Kangaroo 1,370 µs.
- **탄소·비용 (Abstract, p.2; Figure 1 p.4; Figure 13 p.22)**: 6년 수명 기준 FairyWREN은 Kangaroo 대비 flash 비용 35%, flash 탄소배출 33% 절감 (Twitter production trace, 목표 miss ratio 30%). 별도 비교(Section 6.3, p.22)에서는 동일 miss ratio 목표로 Kangaroo 대비 전체 carbon emission 21.2% 절감 수치도 제시됨. Flashield류 log-structured 캐시는 DRAM 오버헤드가 커 emission이 Kangaroo보다도 1.7배 높음(Takeaway 0, p.22).
- **밀도 활용 (Takeaway 4, p.24; Figure 16 p.24)**: Kangaroo는 write rate가 높아 MLC/TLC까지만 안전하게 쓸 수 있는 반면, FairyWREN은 Twitter trace에서 QLC, Meta trace에서 PLC까지 사용 가능 — "최초로 QLC의 이득을 실제로 누리는 캐시 설계"(p.19).
- **수명 연장 (Takeaway 7, p.25; Figure 19 p.26)**: 동일 3.6TB 장치에서 FairyWREN은 Kangaroo 대비 Twitter trace에서 최소 2년, Meta trace에서 5년 넘게 수명을 연장.
- **분해 분석 (Takeaway 8-9, p.26-27; Figure 20 p.26)**: LBAD 위에서 Kangaroo를 그대로 WREN으로 옮기는 것만으로는(+WREN) 부족하며, nest packing 단독으로 쓰기 3.7배, hot-cold 분리 추가로 3.4배 감소, 두 최적화를 합쳐 QLC 수명이 Kangaroo baseline 대비 최대 33배, Physical Separation(스트림 분리) 대비 13배 증가.
- **DRAM/용량 제약 하 강건성 (Takeaway 10-11, p.27-28; Figures 21-22)**: 고정 flash 용량·DRAM(32GB) 제약에서도 FairyWREN이 더 낮은 miss ratio를 달성하며, DRAM이 8GB로 줄어도 Kangaroo보다 우위를 유지.

> [!quote]- 📄 원문 표현 (paper)
> - "FairyWren reduces writes by 12.5× over Kangaroo, from 97 MB/s to 7.8 MB/s." (p.23)
> - "we find that FairyWren's and Kangaroo's 99th-percentile latencies are 170 μs and 1,370 μs, respectively." (p.23)
> - "Nest packing reduces writes by at least 3.7× and hot–cold object separation reduces writes by another 3.4×." (p.27)
> - "FairyWren reduces flash writes by 92% over the research state-of-the-art Kangaroo, leading to a 33% carbon reduction and a 35% cost reduction." (p.19)

## 섹션 노트
- **§1 Introduction**: embodied carbon·flash endurance 트렌드를 근거로 문제를 제기하고, WREN·nest packing·hot-cold 분리라는 기여를 요약.
- **§2 Opportunities in Flash Caching**: flash가 DRAM보다 덜 carbon-intensive하고(12배), flash가 점점 조밀해지며(SLC→PLC), device 수명 연장이 지속가능성에 가장 효과적임을 데이터로 논증(Figure 2, 가격 추이).
- **§3 Challenges in Flash Caching**: DLWA·ALWA 정의, EU/block/page 계층 구조(Figure 3), 밀도가 높아질수록 write endurance가 급락함(Figure 4, Table 1)을 보이며 기존 캐시 설계(log-structured/set-associative/Kangaroo)가 DLWA·DRAM·unused flash 중 하나 이상을 놓친다는 것을 정리(Table 1).
- **§4 Write-Read-Erase iNterfaces**: LBAD의 한계, Multi-streamed SSD·Open-Channel SSD 등 기존 제안의 실패 원인을 분석하고 WREN의 3요소(Write/Read/Erase 연산, Erase 시 host가 GC 책임, 제한된 수의 active EU)를 정의. FIFO+ GC의 analytical model(Eq.1-3, W Lambert function)로 "EU 크기를 줄이는 것만으로는 WA를 크게 낮출 수 없다"(Figure 6)를 증명.
- **§5 FairyWREN Overview and Design**: LOC/SOC 아키텍처, nest packing, hot-cold partitioning, slicing 최적화를 상세 서술(위 설계 절 참조).
- **§6 Evaluation**: carbon/cost 모델(ACT 프레임워크 기반, Eq. carbon emissions), on-flash 실험, 시뮬레이션 기반 carbon/cost/수명/용량 민감도 분석 (Takeaway 0-11).
- **§7 Related Work**: hot-cold/deathtime 기반 grouping, GC 최소화를 노리는 기존 log-structured 캐시(DidaCache 등), object-size 기반 grouping과의 차별점을 정리.
- **§8 Conclusion**: WREN으로의 전환에는 캐시 재설계가 필요하며 FairyWREN이 이를 실증했다고 요약.

## 핵심 용어 (Key terms)
- **WREN (Write-Read-Erase iNterfaces)**: Erase를 first-class 연산으로 노출해 host가 GC를 직접 제어할 수 있게 하는 flash 인터페이스 총칭 (ZNS, FDP, Open-Channel SSD 포함).
- **EU (Erase Unit)**: 한 번에 Erase되는 최소 단위(ZNS의 zone, FDP의 reclaim unit에 대응); 최근 flash는 밀도가 높아지며 EU가 기가바이트 단위로 커짐.
- **DLWA (Device-Level Write Amplification)**: 애플리케이션이 요청한 바이트 대비 device가 실제로 flash에 쓰는 바이트의 비율. GC로 인한 live data 재기록 때문에 발생.
- **ALWA (Application-Level Write Amplification)**: 캐시가 작은 object를 admission/update하기 위해 필요 이상으로 큰 단위(예: 4KB page)를 써야 해서 생기는 증폭.
- **Nest packing**: GC로 인한 EU 재기록 시점에 evict 대상 object 자리를 새 object로 채워 admission과 GC를 한 쓰기로 통합하는 FairyWREN의 핵심 알고리즘.
- **LOC / SOC**: FairyWREN 캐시 용량을 2KB 기준으로 나눈 large-object cache(단순 log-structured)와 small-object cache(FwLog+FwSets 계층 구조).
- **FwLog / FwSets**: SOC 내부의 2단 계층. FwLog는 새 object를 버퍼링하는 log-structured 캐시(용량의 ~5%), FwSets는 set 단위로 flash에 저장되는 set-associative 캐시(~95%).
- **Hot–cold set partitioning**: 각 set을 인기 객체(hot, 재기록 회피)와 신규/비인기 객체(cold)로 나눠 쓰기를 줄이는 기법.
- **Slicing / double buffering**: 제한된 수의 active EU를 여러 논리적 log slice가 공유하도록 segment를 분할(slicing)하고, primary/overflow 두 버퍼로 내부 fragmentation을 줄이는 기법.
- **DWPD (Device-Writes-Per-Day)**: flash 장치의 write 내구성을 나타내는 지표(하루에 전체 용량 몇 배를 쓸 수 있는지).
- **Kangaroo**: FairyWREN이 계승하는 SOSP'21/TOS'22의 hierarchical flash cache(KLog+KSet); LBAD 위에서 낮은 DRAM 오버헤드로 ALWA를 줄이지만 DLWA는 제어하지 못함.

## 강점 · 한계 · 열린 질문
- **강점**: (1) 인터페이스 변화(WREN)만으로는 부족하고 캐시 설계 재구성이 필요함을 이론(FIFO+ 모델)과 실측(real ZNS SSD) 양쪽으로 입증한 점이 설득력 있다. (2) DRAM 오버헤드를 19%만 늘리고 쓰기를 12.5배 줄이는 트레이드오프가 실질적이다. (3) 시뮬레이션뿐 아니라 실제 상용 ZNS SSD 위에서 프로덕션 trace로 검증했다는 점에서 재현성·신뢰도가 높다.
- **한계**: (1) FDP 상에서의 실측 실험은 없고 ZNS SSD로만 on-flash 실험이 이루어졌다(p.20). (2) hot-cold 분리는 miss ratio를 다소 높이는 트레이드오프가 있다(Section 6.6에서 언급, 정확한 수치는 제한적으로만 제시). (3) FairyWREN은 여전히 minimum-write(이상적 캐시) 대비 11% 이내이나 완전히 도달하지는 못하며(p.19), 특히 저(低) miss ratio·PLC 영역에서는 여전히 갭이 존재한다(Figure 16, p.24).
- **열린 질문**: (1) active EU 수 제약이 더 엄격한(4개 미만) 미래 WREN 장치에서 hot-cold subset을 더 세분화하는 것이 여전히 유효할지. (2) FDP처럼 무순서 쓰기를 허용하는 인터페이스에서 nest packing의 write pattern이 실제 DLWA에 미치는 영향은 시뮬레이션 근사(§4.4)에 의존하는데, 실측 검증이 더 필요할지. (3) 다른 workload(KV store, block storage 등)에도 nest packing/hot-cold 분리 아이디어가 얼마나 일반화될지.

## ❓ Q&A (자가 점검)
> [!question]- FairyWREN이 LBAD 대신 WREN을 요구하는 근본적 이유는?
> LBAD는 device firmware가 GC를 완전히 은닉하기 때문에 캐시가 GC로 인한 쓰기(DLWA)를 전혀 통제할 수 없다. WREN(Erase가 first-class 연산)은 host가 GC 시점과 대상을 알고 제어할 수 있게 해, GC 재기록과 캐시 admission을 하나의 쓰기로 합치는 nest packing을 가능하게 한다(p.3, p.9-10).

> [!question]- "WREN만 도입하면 DLWA가 줄어드는가"에 대한 논문의 답은?
> 아니다. §4.4에서 FIFO+ GC의 analytical model(Eq.1-3)로 EU 크기를 아주 작게(수십 KB 수준) 줄이지 않는 한 WA는 크게 낮아지지 않음을 보이고, 실제 EU는 이미 기가바이트 단위라 이 방법은 실현 불가능함을 논증한다(Figure 6, p.13). 즉 캐시 자체의 재설계(nest packing 등)가 필요하다.

> [!question]- Nest packing과 hot-cold 분리 중 어느 쪽이 더 큰 기여를 하는가?
> Nest packing이 쓰기를 최소 3.7배 줄이고, hot-cold object separation이 추가로 3.4배 줄인다(p.27). 둘을 합쳐야 Kangaroo baseline 대비 QLC 수명을 최대 33배 늘릴 수 있으며, 둘 중 하나만으로는 오늘날 QLC에서 합리적인(5년 근접) 수명을 달성하지 못한다.

> [!question]- FairyWREN의 DRAM 오버헤드는 Kangaroo 대비 얼마나 늘어나는가, 그 대가는?
> 2TB 캐시 기준 object당 8.3 bits (Kangaroo 7.0 bits) — 약 19%, 절대량으로 1.5GB 증가(Table 3, p.20). 그 대가로 flash 쓰기를 12.5배 줄이므로 순이익이 크다(p.19).

> [!question]- FairyWREN이 실제로 사용 가능하게 만드는 최대 flash 밀도는 무엇이고 근거는?
> Twitter trace에서는 QLC, Meta trace(덜 write-intensive)에서는 PLC까지 안전하게 쓸 수 있다(Takeaway 4, p.24, Figure 16). 반면 Kangaroo는 두 trace 모두에서 MLC/TLC까지만 가능하다 — 이것이 "FairyWREN이 최초로 QLC의 이득을 실제로 누리는 캐시"라는 주장의 근거다(p.19).

> [!question]- LOC와 SOC를 나누는 기준(2KB)의 의의는?
> Small object(<2KB)는 flash에서 index 오버헤드가 커 log-structured 방식으로는 DRAM이 폭증하는 반면(§3.3), large object는 per-object DRAM 오버헤드를 감당할 수 있어 단순 log-structured로 처리해도 된다. 따라서 크기 기준으로 분리해 서로 다른 최적화(LOC: 순차 로그, SOC: hierarchical + hot-cold)를 적용한다(p.13-14).

> [!question]- carbon emission 33% 감소라는 수치는 Kangaroo와 비교해 무엇을 고정하고 얻은 결과인가?
> 6년 수명, 동일 목표 miss ratio(예: Twitter 30%)를 고정한 상태에서 FairyWREN이 쓰기 감소 덕분에 더 조밀한 flash(QLC 등)를 안전하게 쓸 수 있어 필요한 총 flash 용량·overprovisioning이 줄어드는 데서 온다(Figure 1 p.4, Abstract p.2).

> [!question]- Physical Separation(스트림 분리)만으로는 왜 부족한가?
> Physical Separation은 LOC/SOC 컴포넌트를 서로 다른 EU에 분리해 상호 간섭만 없앨 뿐, nest packing이나 hot-cold 분리처럼 GC와 admission을 통합하거나 lifetime별로 데이터를 재배치하지는 않는다. 그 결과 Kangaroo 대비 쓰기는 다소 줄지만 여전히 과도한 overprovisioning이 필요해 QLC 드라이브에서 수명이 반년도 안 된다(p.27, Takeaway 8-9).

## 🔗 Connections
[[ZNS]] · [[ACM Transactions on Storage]] · [[2025]]
관련: [[eZNS - An Elastic Zoned Namespace for Commodity ZNS SSDs]] · [[BIZA - Design of Self-Governing Block-Interface ZNS AFA for Endurance and Performance]] · [[Z-LFS - A Zoned Namespace-tailored Log-structured File System for Commodity Small-zone ZNS SSDs]]

## References worth following
- **Kangaroo: Caching billions of tiny objects on flash** (McAllister et al., SOSP'21) / **Kangaroo: Theory and practice...** (TOS'22) — FairyWREN이 직접 확장하는 hierarchical flash cache baseline, KLog/KSet 구조의 원 논문.
- **ZNS: Avoiding the block interface tax for flash-based SSDs** (Bjørling et al., USENIX ATC'21) — FairyWREN이 실험에 사용한 WREN 인터페이스(ZNS)의 원 표준 논문.
- **Flexible Data Placement (FDP)** Technical Proposal 4146 (NVM Express, 2022) — FairyWREN이 WREN으로 분류하는 또 다른 산업 표준 인터페이스, ZNS와의 철학 차이를 §4.3에서 비교.
- **The CacheLib caching engine: Design and experiences at scale** (Berg et al., OSDI'20) — FairyWREN 구현의 기반이 되는 오픈소스 캐시 프레임워크.
- **Flashield: a hybrid key-value cache that controls flash write amplification** (Eisenman et al., NSDI'19) — §6.3에서 log-structured 캐시의 DRAM 오버헤드 비교 대상(Figure 13)으로 등장.
- **ACT: Designing sustainable computer systems with an architectural carbon modeling tool** (Gupta et al., ISCA'22) — FairyWREN의 carbon emission 모델(§6.2)이 기반하는 프레임워크.

## Personal annotations
<!-- 본인 메모 영역 -->
