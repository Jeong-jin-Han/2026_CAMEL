---
title: "Scaling the IO Wall with Declarative IO"
description: "유지보수(maintenance) IO의 order/time/data flexibility를 declare 인터페이스로 노출해, 서로 다른 유지보수 태스크 간 IO를 병합함으로써 HDD가 감당해야 하는 IO 수요를 줄이는 DINGO 시스템"
venue: OSDI
year: 2026
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/fs
  - venue/osdi
  - year/2026
  - list/26s-v2
  - topic/storage-maintenance-io
  - topic/declarative-interface
  - topic/hdd-scaling
  - topic/distributed-file-system
---

# Scaling the IO Wall with Declarative IO

> **OSDI 2026** · cluster/fs · Source: [Scaling the IO Wall with Declarative IO.pdf](<Scaling the IO Wall with Declarative IO.pdf>)

저자: Sanjith Athlur (CMU), Sara McAllister (CMU/Wisconsin-Madison), Theo Gregersen (CMU), Timothy Kim (CMU), Yiwei Chen (CMU), Sarvesh Tandon (CMU), Lucy Wang (CMU), Daniel S. Berger (CMU / Microsoft Azure & U. Washington), Saurabh Kadekodi (Google), Arif Merchant (Google), Benjamin Berg (UNC Chapel Hill), Nathan Beckmann (CMU), Rashmi Vinayak (CMU), George Amvrosiadis (CMU), Gregory R. Ganger (CMU)

## TL;DR
HDD 용량은 계속 늘지만 대역폭은 그만큼 늘지 않아 storage cluster가 "IO wall"에 부딪히는데, 저자들은 hyperscaler 6곳 분석에서 이 IO의 45–70%가 scrubbing·garbage collection·rebalancing 등 data maintenance task에서 나온다는 것을 밝힌다. 이런 maintenance task들은 개별적으로는 캐시 친화적이지 않지만 서로 다른 task 간에는 상당한 데이터 중복(overlap)이 존재하며, 시점(order)·시각(time)·대상 데이터(data) 선택에 유연성(flexibility)을 갖는다는 관찰이 핵심이다. 이를 활용하기 위해 저자들은 태스크가 "이 블록들을 deadline까지 읽어야 한다"는 선언(declaration)만 storage system에 보내는 새 인터페이스인 Declarative IO를 제안하고, 이를 구현한 분산 스토리지 시스템 DINGO를 만든다. DINGO는 IO Planner에서 rate-based scheduling과 EC-group 기반 dispatcher로 선언들 간 overlap을 찾아 디스크 IO를 병합하며, HDFS 프로토타입에서 26%, 데이터센터 규모 시뮬레이션에서 28–51%의 유지보수 IO를 절감하고 1.7배 더 큰 HDD 배치를 가능케 한다.

## 문제 & 동기
데이터센터는 저비용·고용량 저장을 위해 HDD에 의존하지만, HAMR 등으로 용량은 빠르게 늘어나는 반면 디스크의 seek latency·transfer rate(즉 IOPS·대역폭)는 거의 늘지 않는다(p.1). 그 결과 disk-head time(디스크가 요청을 서비스하는 데 걸리는 실제 시간) 기준으로 단위 용량당 공급되는 IO가 계속 줄어들며, 총 유지보수+애플리케이션 IO 수요가 공급을 넘어서는 지점, 즉 **IO wall**에 도달한다(p.1, Fig.1a·Fig.3). 지금까지는 flash 캐시를 키워 애플리케이션 IO를 흡수해왔지만, 캐시로 흡수하기 쉬운 부분은 이미 다 흡수되었고 캐시를 뚫고 디스크까지 내려가는 IO는 reuse가 적은 scanning 패턴(주로 maintenance IO)이라 캐시 확장의 한계효용이 급감한다(p.3).

핵심 정량 동기: 6개 hyperscaler에 대한 통신·워크로드 분석 결과 maintenance task(scrubbing, garbage collection, rebalancing, reconstruction, georeplication, transcoding, indexing 등)가 클러스터 전체 disk IO의 **45–70%**를 차지한다(p.1, p.4 Sec 3.2). 이는 하한값이며 애플리케이션 계층/기타 유지보수까지 합치면 더 커질 수 있다. Google의 공개 I/O trace 분석(Fig.7, p.6)에서는 after-cache read traffic 중 24시간 윈도 내 약 24%, 7일 윈도 내 약 42%가 redundant read임을 확인했다.

> [!quote]- 📄 원문 표현 (paper)
> - "we find that maintenance tasks account for at least 45-70% of disk IO" (p.4)
> - "Since disks are approaching a point where the total IO demand of datacenter workloads on HDDs exceeds IO supply — a phenomenon we refer to as the IO wall." (p.2)
> - "approximately 24% of after-cache disk read traffic is redundant when requests can be delayed or reordered within a 24-hour window. This redundancy increases to approximately 42% when reads are observed over 7 days" (p.6)

## 핵심 통찰 (Key Insight)

**1. Maintenance IO는 캐시로는 못 잡지만 서로 다른 task 간에는 대량 중복이 존재한다.** 각 task 자체는 스캔성이라 캐시에 안 걸리지만(reuse가 없음), scrubbing이 전체 블록을 훑는 동안 다른 task(rebalancing, garbage collection 등)도 겹치는 블록을 결국 건드리게 되므로 task 간에는 overlap이 크다. 다만 이 overlap이 시간적으로 너무 멀리 떨어져 있어(비동기적으로 스케줄) 캐시 하나로는 hit이 안 난다(p.2, p.5-6). 이것이 문제의 본질이자 해법의 실마리다.

> [!quote]- 📄 원문 표현 (paper)
> - "there is significant data overlap between different maintenance tasks... However, since maintenance tasks are not generally synchronized, most inter-task reuse is also too far apart in time to result in cache hits." (p.2)

**2. Maintenance task는 order-, time-, data-flexibility를 갖지만, imperative interface(get/put, read/write)는 이 유연성을 표현할 방법이 없다.** scrubbing은 "언제 어떤 순서로" 블록을 읽든 deadline 내에만 끝내면 되고(order+time-flexibility), rebalancing이나 garbage collection은 "어떤 특정 블록"이 아니라 "이 정도 양의 블록 중 아무거나"를 옮기면 된다(data-flexibility). 그런데 imperative I/O는 매 요청이 "지금 이 특정 데이터"를 요구하므로 이 유연성이 인터페이스 차원에서 사라진다(p.6-7). Declarative IO는 이 유연성을 명시적으로 시스템에 전달해, 시스템이 서로 다른 태스크의 접근을 시간적으로 겹치도록 재배열할 수 있게 한다.

> [!quote]- 📄 원문 표현 (paper)
> - "maintenance tasks are generally flexible in access order, access time, and even which data they access" (p.2)
> - "the current imperative distributed storage interfaces... do not expose flexibility — each request is for a specific data unit to be accessed now." (p.2)

**3. Scheduling for optimal reuse는 NP-hard이므로, deadline 준수를 우선하는 rate-based heuristic으로 실용적 절충을 취한다.** 최적 스케줄을 찾는 문제는 bin-packing/VM-packing으로 환원되어 NP-hard임을 증명(Appendix A.5, p.20)하고, 대신 각 선언의 IO rate(=요구량/남은 시간)를 계산해 rate가 높은(=deadline이 급한) 선언부터 quantum마다 스케줄하는 rate-based scheduler를 채택한다. 이는 이론적 최적은 아니지만 실전에서 상당한 reuse를 만들어내며(Optimal 대비 5% 이내, p.13 Takeaway 4), deadline miss를 방지한다(p.8-9).

## 설계 / 메커니즘 (Design)

**Declarative IO 인터페이스** (Sec.4, Fig.8 p.6): `declare(read_sets, sets_needed, deadline, callback)` 함수 하나가 핵심. `read_sets`는 `BlockSet`(불변·seal된 블록들의 그룹) 리스트이고, `sets_needed`로 이 중 몇 개나 읽어야 하는지(=data-flexibility) 지정 가능. `deadline`은 언제까지 다 읽혀야 하는지. 시스템은 조건을 만족하는 시점에 `callback`을 호출해 어떤 BlockSet을 지금 읽으라고 알려주고, 태스크는 그 즉시 기존 imperative read로 실제 데이터를 읽는다. append-only(sealed block) 스토리지를 전제로 하며, deletion·compute unavailability·IO load spike 등 corner case에도 기존 imperative 스토리지의 consistency 메커니즘(lease 등)에 얹혀 정합성을 보장한다(Sec.4.4, p.8-9). `declare_files`로 파일/오브젝트 단위 선언도 지원(Appendix A.3, Fig.23).

**DINGO 시스템 구조** (Sec.5, Fig.12 p.10): append-only 분산 파일시스템(HDFS 기반 프로토타입) + metadata service + storage node로 구성. 핵심 컴포넌트는 **IO Planner**로, (1) Declarations를 보관하고, (2) 매 scheduling quantum마다 어떤 BlockSet을 스케줄할지 정하는 **Planner**, (3) 선택된 BlockSet에 대해 콜백을 실제로 발사하는 **Dispatcher**로 구성.

- *Rate-based scheduling* (Sec.5.1): 각 선언의 IO rate = 남은 요구량/(deadline까지 남은 시간). quantum당 전역 IO budget이 소진될 때까지 rate가 높은 순으로 스케줄하며, 이미 다른 선언에 의해 스케줄된 블록은 "saved"로 처리해 IO budget을 소모하지 않는다(Fig.13, p.10). 남는 budget은 기본적으로 스케줄하지 않고 다른 우선순위 IO에 남겨둔다(leftover policy, Sec.6.4에서 검증).
- *EC-group 기반 dispatcher* (Sec.5.2): naive하게 겹치는 BlockSet을 찾는 dispatching group 계산은 $O(|scheduled\ BlockSet|^2)$로 수백만 개 선언 규모에서는 불가능. 대신 erasure-coding group(같은 블록 집합을 접근하는 BlockSet들의 목록)을 유지해 검색 범위를 좁혀 $O(|scheduled\ BlockSet|)$로 낮춘다.
- *Explicit cache management*: 캐시가 저절로 IO를 저장해줄 거라 믿는 대신, IO Planner가 스케줄된 블록에 대해 매 dispatching round마다 명시적 "cache directive"를 내려 해당 블록을 캐시에 pin한다. 이는 실제로 캐시가 evict/miss 등으로 예상만큼 저장 효과를 못 내는 문제를 해결하며, 필요한 캐시 용량은 극히 작다(1EB 클러스터에서도 pessimistic upper bound 240TB, 실질적으로는 훨씬 적음, p.10).

## 평가 (Evaluation)
평가는 (1) HDFS 기반 소규모 프로토타입(10노드, 노드당 3TB HDD, 40GbE)과 (2) 100PB급 datacenter simulator 두 갈래로 진행(Sec.6.1, p.11).

- **Takeaway 1**: 프로토타입에서 4개 HDFS 유지보수 태스크(rebalancing, reconstruction, scrubbing, transcoding, Table 1 p.11)로 DINGO는 디스크 read를 **26%** 절감(p.12).
- **Takeaway 2**: Alibaba block storage trace 기반 foreground workload와 함께 돌렸을 때, DINGO의 client 평균 read latency가 baseline보다 낮음(median 86ms vs 167ms, P99 832ms vs 1181ms avg; worst-case 423ms vs 587ms(median), 4969ms vs 6877ms(P99), Fig.15 p.12).
- **Takeaway 3**: 100PB 시뮬레이터에서 대표 maintenance read demand 2 MBps/TB, 48TB 드라이브 기준, DINGO는 disk utilization을 imperative 대비 최대 45% 낮춰 Cluster A에서 34.8%까지 내림(Fig.16a, p.13). 이는 최대 지원 드라이브 크기를 Cluster A 1.7배(36TB→64TB 수준, Fig.1과 일치), Cluster B 1.5배(→58TB)로 확대(Fig.16b, p.13).
- **Takeaway 4**: 미래를 다 아는 비현실적 lower bound인 Optimal 대비 DINGO는 5% 이내로 근접(p.13).
- **Takeaway 5**: 1EB 클러스터에서 IO Planner가 단일 서버(Dell PowerEdge R640, 240GB RAM)로 1시간 quantum의 모든 dispatching group을 15분 내에 계산, peak memory 205GB(p.13).
- **Takeaway 6–7**: rebalancing처럼 data-flexibility가 큰 태스크가 reconstruction 같이 deadline이 급한 태스크의 IO에 "무임승차"해 거의 공짜로 수행됨. 전통적 imperative scrubbing은 불필요한 IO를 상당히 발생시키는데, DINGO 분석 결과 scrubbing이 선언한 read의 약 26%가 이후 삭제될 데이터라 불필요했음(p.14).
- **Sec.6.3 breakdown** (Fig.17, p.14): Cluster A/B 각각에서 어떤 task가 어떤 task의 IO에 얹혀 절감되는지 시각화. Cluster B는 4배 더 많은 scrubbing을 추가 IO 거의 없이 수행 가능(p.15).
- **Sensitivity** (Sec.6.4): 요구량이 1→3.5 MBps/TB로 늘어도 DINGO는 39–51%(Cluster A), 28–46%(Cluster B) 절감 유지(Fig.18, p.15). Deadline을 늘리면(예: transcoding 1일→7일) disk utilization이 추가로 약 7%p 감소(Fig.20, p.15). Diurnal IO 공급 변동에도 rate-based scheduler가 할당량 내로 유지(Fig.21, p.16).

> [!quote]- 📄 원문 표현 (paper)
> - "DINGO decreases disk reads by 28-51%, enabling devices that are 1.7× larger." (p.11)
> - "DINGO enables 1.7× larger drive adoption on petabyte-scale clusters in our DINGO simulator." (p.13)
> - "∼26% of reads declared for scrubbing were unnecessary because the data was subsequently deleted." (p.14)

## 섹션 노트
- **Sec.1 Intro**: HDD 용량 증가 대비 IO 공급 정체로 IO wall 발생. Maintenance task가 45–70% 차지함을 제시하고 Declarative IO/DINGO를 제안.
- **Sec.2 The impending IO wall**: 데이터센터 4-tier 아키텍처(application–data management–caching–bulk storage, Fig.2)와 disk-head time 정의, 캐시 확장의 한계효용 체감 논증.
- **Sec.3 Maintenance tasks**: 6개 hyperscaler 데이터로 maintenance IO 비중·구성(Fig.4–6) 제시, 동기화 어려움과 용량 비례 증가라는 두 challenge, 그리고 flexibility라는 opportunity 정리.
- **Sec.4 Declarative IO**: `declare` 인터페이스 정의, order/time/data-flexibility 표현법, 파일 단위 확장, consistency(삭제·append·메타데이터 변경) 보장 논의.
- **Sec.5 DINGO Design**: IO Planner의 rate-based scheduling, EC-group dispatcher, explicit cache 관리 세 가지 최적화.
- **Sec.6 Evaluation**: 프로토타입+시뮬레이터로 IO 절감, 드라이브 용량 확대, latency 영향, IO 절감 출처, sensitivity 분석(11개 Takeaway).
- **Sec.7 Related Work**: Duet/Quartet(캐시 이벤트 기반 로컬 스케줄링), disk-level 재정렬(freeblock scheduling), semantically-smart disks, 데이터베이스의 shared scan과 비교하며 Declarative IO가 여러 계층·조직에 걸친 분산 스토리지 인터페이스라는 차별점 강조.
- **Sec.8 Conclusion**: HDD가 IO wall에 근접했으며 maintenance IO가 그 핵심 원인임을 재확인, Declarative IO가 비용/전력/탄소/foot-print 절감에 광범위한 함의를 가짐을 논의.
- **Appendix**: disk-head time 수식(A.1), 유지보수 태스크별 상세 구현(A.2, Table 3), 파일 선언 인터페이스(A.3), LSM compaction 같은 복잡한 태스크로의 확장(A.4), scheduling NP-hardness 증명(A.5).

## 핵심 용어 (Key terms)
- **IO wall**: HDD 용량 증가에 비해 IO 공급(대역폭/IOPS)이 정체되어, 총 IO 수요가 공급을 초과하게 되는 임계 지점.
- **Disk-head time**: seek latency와 transfer time을 합산한, 디스크가 실제 요청을 서비스하는 데 쓰는 시간(Appendix A.1 수식 참고).
- **Declarative IO**: 태스크가 구체적 read/write 대신 "이런 데이터 집합을 이 deadline까지 읽어야 한다"는 선언만 시스템에 전달하는 새 인터페이스.
- **BlockSet**: `declare`가 다루는 최소 데이터 단위 그룹으로, 같은 시점에 함께 필요한 불변(sealed) 블록들의 집합(LBA와는 다름).
- **Order-/Time-/Data-flexibility**: 각각 접근 순서, 접근 시점, 정확히 어떤 데이터를 읽을지에 대해 태스크가 갖는 자유도.
- **Rate-based scheduling**: 각 선언의 (남은 요구량/남은 시간)을 IO rate로 계산해, 높은 rate 순으로 quantum마다 스케줄하는 DINGO의 휴리스틱.
- **EC-group (erasure-coding group)**: 동일한 블록 부분집합을 접근하는 BlockSet들을 모아둔 그룹으로, dispatching group 탐색 복잡도를 낮추는 인덱스.
- **Callback elision**: 선언된 BlockSet이 완전히 삭제된 경우, 불필요한 콜백 호출 자체를 생략하는 최적화.
- **Maintenance task**: scrubbing, garbage collection, rebalancing, reconstruction, georeplication, transcoding, indexing 등 latency에 민감하지 않지만 대량 데이터에 주기적으로 접근하는 데이터 유지보수 작업 전반.
- **Disk-head utilization**: disk-head time을 wall-clock time으로 나눈 값으로 디스크가 실제로 얼마나 바쁜지를 나타내는 지표.

## 강점 · 한계 · 열린 질문
**강점**: (1) 6개 hyperscaler와의 실제 협업 데이터로 문제의 정량적 규모(45–70%)를 확실히 뒷받침한다는 점이 설득력 있다. (2) 인터페이스 변경이라는 다소 근본적인 선택을 하면서도 append-only+imperative read 위에 얹는 방식으로 기존 consistency 메커니즘을 재사용해 구현 부담과 정합성 리스크를 낮췄다. (3) HDFS 프로토타입과 100PB 시뮬레이터 양쪽에서 결과를 교차검증(5% 이내 일치, p.11)해 신뢰도를 높였다.

**한계**: (1) Declarative IO는 append-only/sealed-block 스토리지를 전제로 하며(Sec.4.4), in-place update가 흔한 시스템에는 그대로 적용하기 어렵다. (2) rate-based scheduling은 이론적으로 최적이 아니며(NP-hard 문제의 heuristic), Optimal과의 gap이 워크로드에 따라 커질 가능성이 있다(논문은 5% 이내라 주장하지만 두 클러스터 데이터에 한정됨). (3) leftover IO allocation을 스케줄하지 않고 버리는 기본 정책은 "미래에 도움될 수 있는 오버랩"을 놓칠 수 있다고 스스로 인정한다(Sec.5.1, p.9). (4) client-facing latency 평가에서 transcoding의 클러스터형 write burst가 P50 latency를 일시적으로 6배까지 악화시키는 사례가 있어(Fig.15 관련 서술, p.12), write IO/CPU/네트워크까지 고려한 스케줄링은 future work로 남겨짐.

**열린 질문**: metastable failure(캐시처럼 declarative IO도 deadline을 못 맞추기 시작하면 악순환에 빠질 위험, Sec.5.2 p.9)의 실제 발생 조건과 방어책은 무엇인가? LSM compaction처럼 겹치는 작업 단위를 가진 더 복잡한 태스크(Appendix A.4)로 확장할 때 data-flexibility를 얼마나 유지할 수 있는가? Declarative IO를 batch analytics나 ML 학습 같은 latency에 덜 민감한 애플리케이션 IO로 확장하면 절감 폭이 어느 정도일지(Sec.8에서 future work로 언급)는 미해결로 남아 있다.

## ❓ Q&A (자가 점검)
> [!question]- IO wall이란 무엇이고, 왜 캐시를 늘리는 것만으로는 해결이 안 되는가?
> HDD 용량은 계속 커지지만 disk-head time(seek+transfer)은 거의 늘지 않아 단위 용량당 IO 공급이 줄어드는데, 총 IO 수요(특히 maintenance IO)가 이 줄어드는 공급을 넘어서는 지점이 IO wall이다(p.1-2). 캐시는 애플리케이션 IO의 reuse가 좋은 부분은 이미 흡수했고, 캐시를 통과해 디스크까지 내려가는 IO는 reuse가 적은 스캔성 maintenance IO라서 캐시를 더 키워도 한계효용이 급격히 준다(p.3).

> [!question]- Maintenance task가 전체 disk IO에서 차지하는 비중은 얼마이며 어떻게 측정했는가?
> 6개 hyperscaler와의 협업 및 워크로드 분석 결과 최소 45–70%(hyperscaler마다 하한값)이며(p.4, Sec.3.2), Meta·Microsoft·Google 3사의 세부 breakdown(Fig.4–6)으로 뒷받침한다.

> [!question]- Declarative IO의 `declare` 함수는 어떤 파라미터를 받고 각각 무엇을 의미하는가?
> `read_sets`(필요한 BlockSet들의 리스트), `sets_needed`(그 중 실제로 읽어야 할 개수, data-flexibility 표현), `deadline`(언제까지 다 읽혀야 하는지), `callback`(시스템이 특정 BlockSet을 지금 읽으라고 알려주는 콜백)이다(Fig.8, p.6).

> [!question]- DINGO의 IO Planner가 최적 스케줄 대신 rate-based heuristic을 쓰는 이유는?
> reuse를 최대화하는 최적 스케줄링 문제는 VM/bin-packing으로 환원되어 NP-hard이기 때문이다(Appendix A.5, p.20). 대신 각 선언의 (남은 요구량/남은 시간) 비율이 높은 순으로 스케줄해 deadline 준수를 우선하며, 실전에서 이 방식도 상당한 reuse를 만들어낸다(p.9, Optimal 대비 5% 이내).

> [!question]- EC-group 기반 dispatcher는 어떤 문제를 해결하는가?
> naive하게 스케줄된 BlockSet 간 overlap을 전수 탐색하면 $O(|scheduled\ BlockSet|^2)$로 수백만 declaration 규모에서 다루기 어렵다. EC-group(같은 블록 부분집합을 접근하는 BlockSet들의 색인)을 유지하면 탐색을 $O(|scheduled\ BlockSet|)$로 줄일 수 있다(Sec.5.2, p.10).

> [!question]- HDFS 프로토타입과 100PB 시뮬레이터에서 각각 어느 정도의 IO 절감/드라이브 확장 효과가 나왔는가?
> 프로토타입은 디스크 read 26% 절감(p.12). 시뮬레이터는 두 hyperscaler 기반 클러스터에서 28–51% 절감이며, Cluster A는 최대 지원 드라이브 크기 1.7배(36TB급 imperative 한계 대비 64TB 지원), Cluster B는 1.5배(58TB) 확대를 가능케 한다(Fig.16, p.13).

> [!question]- Declarative IO가 삭제(deletion)된 블록에 대한 콜백을 어떻게 처리하는가?
> 선언이 참조한 블록이 deadline 전에 삭제될 수 있음을 전제하고, 태스크는 imperative read의 기존 guarantee를 상속받아 이를 처리한다. 만약 어떤 BlockSet의 모든 블록이 삭제되었다면 DINGO는 callback elision으로 아예 콜백을 생략해 불필요한 처리를 막는다(Sec.4.4, p.8-9).

> [!question]- 논문이 발견한 "imperative storage system은 불필요한 maintenance IO를 한다"는 구체적 근거는?
> Scrubbing이 삭제 예정 데이터까지 무차별적으로 검증하는 사례에서, 유연한 deadline을 허용하는 것만으로 scrubbing이 선언한 read의 약 26%가 실제로는 이후 삭제되어 불필요했음을 확인했다(Takeaway 7, p.14).

## 🔗 Connections
[[File System]] · [[OSDI]] · [[2026]]
관련: [[Fast, Transparent Filesystem Microkernel Recovery with Ananke]] · [[Discard-Based Garbage Collection for Distributed Log-Structured Storage Systems in ByteDance]] · [[Sleeping with One Eye Open - Fast, Sustainable Storage with Sandman]]

## References worth following
- Berg et al., "The CacheLib caching engine: Design and experiences at scale," USENIX OSDI 2020 — DINGO의 explicit cache 관리와 비교할 만한 대규모 캐시 엔진 설계.
- Amvrosiadis, Demke Brown, Goel, "Opportunistic storage maintenance," SOSP 2015 — maintenance task를 시스템이 다루는 초기 접근으로 Declarative IO의 문제의식과 직접 연결됨(Duet 논문).
- Deslauriers, McCormick, Amvrosiadis, Goel, Demke Brown, "Quartet: harmonizing task scheduling and caching for cluster computing," HotStorage 2016 — Duet을 확장해 map-reduce 서브태스크를 IO 근접 시점으로 스케줄, DINGO의 rate-based scheduling과 목적이 유사.
- Kadekodi et al., "Morph: Efficient file-lifetime redundancy management for cluster file systems," SOSP 2024 — transcoding/redundancy 변경을 다루는 저자 그룹의 선행 연구로 DINGO의 transcoding 태스크 모델과 연관.
- Zhang et al., "Characterizing, modeling, and benchmarking I/O workload of a production cloud scale block storage system," SOSP 2021 — Google I/O trace 재현(Fig.7의 redundancy 분석에 사용된 trace 출처).

## Personal annotations
<!-- 본인 메모 영역 -->
