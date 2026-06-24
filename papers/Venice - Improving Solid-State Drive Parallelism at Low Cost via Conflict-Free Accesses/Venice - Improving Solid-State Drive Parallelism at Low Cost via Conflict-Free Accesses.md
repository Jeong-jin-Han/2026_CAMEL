---
title: "Venice: Improving Solid-State Drive Parallelism at Low Cost via Conflict-Free Accesses"
aliases: [Venice]
description: "SSD 컨트롤러-플래시 칩 사이에 저비용 interconnection network를 추가해 path conflict를 근본적으로 해소하는 메커니즘"
venue: ISCA
year: 2023
arxiv: "2305.07768"
tier: deep
status: done
tags: [paper, cluster/reliability, topic/ssd-parallelism, topic/ftl, topic/ssd-architecture, venue/isca, year/2023]
---

# Venice: Improving Solid-State Drive Parallelism at Low Cost via Conflict-Free Accesses

> **ISCA 2023** · cluster/reliability · Source: [Venice - Improving Solid-State Drive Parallelism at Low Cost via Conflict-Free Accesses.pdf](<Venice - Improving Solid-State Drive Parallelism at Low Cost via Conflict-Free Accesses.pdf>)

**저자:** Rakesh Nadig, Mohammad Sadrosadati (co-primary), Haiyu Mao, Nika Mansouri Ghiasi, Arash Tavakkol, Jisung Park, Hamid Sarbazi-Azad, Juan Gómez-Luna, Onur Mutlu — ETH Zürich, POSTECH, Sharif University of Technology, IPM

---

## TL;DR

상용 SSD는 SSD 컨트롤러와 NAND flash chip 사이를 **multi-channel shared bus** 구조로 연결한다. 한 channel을 여러 flash chip이 공유하므로, 같은 channel에 붙은 chip들을 향하는 여러 I/O 요청이 동시에 발생하면 **path conflict**(경로 충돌)가 일어나 직렬화된다. 이 문제 하나만 제거해도 ideal(path-conflict-free) SSD는 baseline 대비 평균 **4배** 빠르다.

Venice는 flash chip 옆에 **flash chip 자체를 수정하지 않고** 작은 **router chip**을 붙여 2D mesh interconnection network를 만들고, 세 가지 기법으로 path conflict를 근본 해소한다: (1) flash chip 옆 단순 router chip, (2) 데이터 전송 전 경로를 미리 예약하는 **path reservation** (scout packet 사용 → 라우터 내 대용량 버퍼 불필요), (3) network의 풍부한 path diversity를 활용하는 **non-minimal fully-adaptive routing**. 결과적으로 baseline 및 최선 prior work(NoSSD) 대비 성능을 크게 개선하면서 면적/전력 오버헤드는 낮게 유지한다.

## 문제 & 동기

- **Multi-channel shared bus 구조:** 상용 SSD는 컨트롤러를 4~16개 channel로 flash chip(channel당 4~32개)에 연결한다. 각 flash chip은 컨트롤러와 통신할 **path가 단 하나**뿐이고, 같은 channel의 chip들이 그 path를 공유한다.
- **Path conflict:** 같은 channel의 서로 다른 chip을 향하는 여러 I/O 요청은 같은 path를 써야 하므로 **직렬화**된다. 이는 SSD parallelism을 크게 제한한다. 예시(Figure 3)에서 두 read 요청이 같은 channel을 공유하면 평균 I/O latency가 **57% 증가**한다 (11.01μs vs 이상적 7.01μs; CMD 10ns, RD Operation 3μs, Transfer 4μs 기준).
- **Read에 더 치명적:** read는 data transfer time이 flash read latency와 비슷하거나 더 길어 path conflict 영향이 크다. write는 flash write latency(예: 100μs)가 service time을 지배해 상대적으로 영향이 작다.
- **정량화:** ideal path-conflict-free SSD가 baseline 대비 평균 4배(최대 11.74배) 빠르다 (Figure 4).
- **기존 접근의 한계 (§3.2~3.3):**
  - **pSSD (Packetized SSD)** [Kim et al.]: control/data pin을 모두 써서 channel 대역폭을 2배로. transfer latency만 줄일 뿐 conflict를 근본 해소 못 함. flash chip당 20% 면적 오버헤드.
  - **pnSSD (Packetized Network SSD)**: 2D mesh로 N×N 배열, chip당 path 2개. 부분 완화.
  - **NoSSD (Network-on-SSD)** [Tavakkol et al.]: 2D mesh interconnection network로 path diversity 大. 그러나 (1) router당 16KB buffer + chip당 4배 I/O pin이라는 큰 면적/비용 오버헤드, (2) deterministic dimension-order routing이라 path diversity를 활용 못 함. baseline 대비 평균 35% 개선에 그쳐 4배 ideal에 한참 못 미침.

> [!quote]- 📄 원문 표현 (paper)
> "Path sharing causes the path conflict problem, where an I/O request needs to wait for the path to become free, if the path is being used for another I/O request." (p.3)
> "We observe that the ideal SSD outperforms the baseline SSD by 4× on average across nineteen data-intensive real-world workloads." (p.1)
> "The path conflict problem affects the performance of read requests more than that of write requests. This is because data transfer time for a read request is comparable to or longer than the flash read latency, while the flash write latency ... dominates the total service time of a write request." (p.4)
> "prior SSD interconnection network designs have two main weaknesses ... First, prior works impose significant area (i.e., cost) overhead as they integrate a buffered router (e.g., 16KB buffer per router port) inside each flash chip. ... Second, prior works do not resolve path conflicts effectively because they employ a simple deterministic routing algorithm, which cannot utilize the interconnection network's rich path diversity." (p.2)

## 핵심 통찰

1. **Path diversity를 늘리면 conflict를 근본 해소할 수 있다.** 컨트롤러-chip 간 경로 수를 늘리는 것이 핵심. 단, 채널 수를 늘리는 것은 컨트롤러 I/O pin·복잡도 증가로 확장성이 없다.
2. **Router를 flash chip에서 분리하라 (저비용의 핵심 결정).** 기존 NoSSD는 router를 flash die 안에 넣어 chip 수정·pin 증가·대용량 buffer를 요구했다. Venice는 router를 **별도 chip**으로 flash chip 옆에 두어 commodity flash chip을 전혀 수정하지 않는다(면적 오버헤드 0 on chip).
3. **데이터 전송 전에 경로를 예약하면 라우터 buffer가 필요 없다.** 미리 conflict-free path를 잡아두면(path reservation) 전송 중 충돌이 없으므로 라우터에 데이터를 저장할 큰 buffer를 둘 필요가 없다.
4. **Deterministic routing은 풍부한 경로를 못 쓴다 → adaptive routing 필요.** minimal path가 막혔을 때도 idle link를 우회(non-minimal)해 conflict-free path를 찾는다.

> [!quote]- 📄 원문 표현 (paper)
> "Our key idea is to use a low-cost interconnection network to increase the path diversity between the SSD controller and flash chips." (p.2)
> "The key design decision that enables our approach to be low cost is the separation of the router from the flash chip such that the flash chip is not modified." (p.5)
> "Venice reserves a conflict-free path between the flash controller and the target flash chip for each I/O request before starting the transfer. This technique avoids the need for large buffers in each router that are otherwise required to store the data of each I/O request that experiences a path conflict." (p.5)
> "Venice's non-minimal fully-adaptive routing algorithm dynamically identifies a conflict-free path between the flash controller and the flash chip. This algorithm effectively utilizes the idle links in the interconnection network to find a non-minimal path when a minimal path is unavailable." (p.6)

## 설계 / 메커니즘

**개요:** Venice = 세 기법의 결합.

### (1) Low-Cost Interconnection Network of Flash Chips (§4.1)
- **Flash node** = flash chip + 별도 **router chip**. router는 flash chip의 I/O data pin(원래 shared channel용 injection/ejection pin)을 통해 chip과 통신.
- flash node들을 **2D mesh** topology로 연결, router는 이웃 router와 bidirectional link로 연결.

### (2) Path Reservation (§4.2)
- I/O 요청마다 데이터 전송 전에 flash controller↔target flash chip 간 **conflict-free path를 예약**.
- **Scout packet**으로 예약: header flit + tail flit, 각 8-bit. header flit은 type(2-bit: header/tail, reserve/cancel) + destination flash chip ID(6-bit, 64 chip). tail flit은 source flash controller ID(3-bit, 8 controller).
- 각 router는 **router reservation table** 보유: packet ID, entry port(2-bit), exit port(2-bit), valid bit. log(n) bit으로 동시에 최대 n개 scout packet 전송 가능(8 controller → 3-bit).
- 예약된 link는 bidirectional: **forward path**(write, controller→chip), **backward path**(read, chip→controller). scout가 destination 도착 시 backward path로 source까지 되돌아가고, source controller가 예약된 path로 I/O를 schedule.
- 가장 가까운 free flash controller를 골라 scout packet을 보냄.

### (3) Non-Minimal Fully-Adaptive Routing (§4.3)
- scout packet을 라우팅할 때 idle link를 활용해 minimal path가 막혀도 **non-minimal(우회) conflict-free path**를 찾음 (Algorithm 1).
- 두 과제 해결:
  - **성능 오버헤드:** non-minimal path는 hop이 늘어 transfer latency 증가(Eq.1). 그러나 I/O data transfer는 크고(16KB) flash operation latency가 service time을 지배하므로, 긴 command transfer의 영향은 **negligible**.
  - **Deadlock/Livelock 회피:**
    - *Deadlock:* scout가 path conflict를 만나면 **backtracking**(upstream router로 되돌아가 다른 path 선택)으로 자원을 순환 의존하지 않게 해 deadlock 미발생.
    - *Livelock:* scout가 같은 router를 **최대 3번**(2D mesh, 4-1)까지만 방문하도록 제한. 초과 시 path back-trace, 최악의 경우 source controller로 복귀해 새 scout packet으로 재시도.
- Algorithm 1: destination router와의 X/Y 차이(Diff_x, Diff_y)로 9가지 case 분기, free output port를 output list에 추가. list가 2개면 LFSR pseudo-random으로 1개 선택, 1개면 그것 선택, 0개(empty)면 misroute(non-minimal) 또는 backtrack.

### 구현 파라미터
- 8×8 2D mesh, 8 flash controller, 64 flash chip. router는 flash node당 1개, port당 two 4-flit buffer, 1GHz, circuit switching.

> [!quote]- 📄 원문 표현 (paper)
> "We introduce a new building block, called flash node, which consists of a flash chip and a separate router chip. ... a flash chip communicates with a router chip using its I/O data pins (i.e., injection/ejection pins) that are otherwise used for connecting the flash chip to the shared channel." (p.5)
> "Venice identifies and reserves a path by sending a special packet called scout packet." (p.5)
> "Venice handles deadlock by using backtracking of a scout packet. ... As a result, a scout packet is never blocked due to resource unavailability in the network and deadlock does not happen. Venice handles livelock by restricting the number of times a scout packet can visit the same router." (p.7)
> "A non-minimal path has a longer distance compared to a minimal path. ... the longer command transfer time due to a non-minimal path has a negligible effect on the total service time of the I/O request." (p.7)

## 평가

**환경:** MQSim simulator. 두 구성 — performance-optimized(Samsung Z-NAND 기반, read 3μs/program 100μs/erase 1ms), cost-optimized(Samsung PM9A3, 3D TLC, read 3μs/program 650μs/erase 3.5ms). 19개 real-world data-intensive workload(MSR Cambridge, YCSB, Slacker, SYSTOR'17, YCSB RocksDB) + 6개 mixed workload. router는 UMC 65nm HDL 합성, link는 ORION 3.0.

**비교 대상:** Baseline SSD, pSSD, pnSSD, NoSSD, ideal path-conflict-free SSD.

- **성능 (Figure 9):**
  - performance-optimized: baseline/pSSD/pnSSD/NoSSD 대비 평균 **2.65×/2.10×/2.00×/1.92×** (최대 7.10×). path-conflict-free 대비 45% 이내.
  - cost-optimized: 평균 **1.67×/1.52×/1.55×/1.47×** (최대 3.68×). path-conflict-free 대비 25% 이내.
  - performance-optimized에서 개선이 더 큼(빠른 flash라 transfer time이 service time을 지배).
- **SSD throughput (Figure 10):** baseline/pSSD/pnSSD/NoSSD 대비 perf-opt에서 **176%/120%/113%/102%**, cost-opt에서 76%/58%/61%/51% 향상. path-conflict-free의 30%(perf)/10%(cost) 이내.
- **Tail latency (99th percentile):** src1_0에서 baseline/pSSD/pnSSD/NoSSD 대비 **32%/31%/30%/27%** 감소, hm_0에서 22%/21%/18%/17% 감소.
- **Mixed workloads (Figure 12):** baseline/pSSD/pnSSD/NoSSD 대비 평균 **1.83×/1.81×/1.80×/1.63×**. inter-request arrival time이 짧을수록(mix6) 개선이 더 큼.
- **Path conflict 비율 (Figure 13):** Venice는 **99.98%**의 I/O가 첫 시도에 conflict-free path 확보(0.02%만 대기). baseline/pSSD/pnSSD/NoSSD는 76.40%/78.47%/77.88%/80.65%에 그침.
- **Energy (Figure 14):** baseline/pSSD/pnSSD/NoSSD 대비 평균 **61%/54%/53%/46%** 감소. average power도 baseline 대비 4% 감소(link가 shared channel보다 전력 적게 소비).
- **Sensitivity (Figure 15):** 4×16/8×8/16×4 구성에서 baseline 대비 2×/2.6×/1.9×. 8×8(8 controller)에서 가장 큼.
- **면적/전력 오버헤드 (Table 4):** flash chip 자체엔 오버헤드 없음. router는 HDL상 614μm², PCB I/O pad 포함 ~8mm²(100mm² flash chip의 8%). link는 shared channel area의 0.04배, 총 interconnect link 면적은 baseline multi-channel shared bus 대비 **44% 낮음**. router 0.241mW, link 1.08mW(shared bus 대비 90% 적음). PCB에 8% 면적 오버헤드.

> [!quote]- 📄 원문 표현 (paper)
> "Venice consistently outperforms all the prior approaches across all workloads in both SSD configurations. In the performance-optimized SSD configuration, Venice outperforms Baseline SSD/pSSD/pnSSD/NoSSD by an average of 2.65×/2.10×/2.00×/1.92× across all workloads." (p.9)
> "Venice provides conflict-free paths (on the first try) for 99.98% of I/O requests, on average, across all workloads, while Baseline SSD/pSSD/pnSSD/NoSSD provides conflict-free paths for 76.40%/78.47%/77.88%/80.65% of I/O requests." (p.11)
> "Venice reduces energy consumption by an average of 61%/54%/53%/46% compared to Baseline SSD/pSSD/pnSSD/NoSSD." (p.11)
> "Venice's interconnect links occupy 44% lower area compared to the baseline multi-channel shared bus architecture." (p.12)

---

## 섹션 노트

- **§1 Introduction:** path conflict 정의, ideal SSD 4배, 채널 증가의 비확장성, prior work 한계 요약. Venice 3대 기여 명시.
- **§2 Background:** 현대 SSD 구조(HIL/FTL/FC, NAND chip→die→plane→block→page). FTL 4대 책임(address mapping out-of-place write, GC, wear-leveling, caching). multi-plane operation은 같은 offset일 때만 동시 동작.
- **§3 Motivation:** §3.1 path conflict 정의·정량화, §3.2 pSSD/pnSSD/NoSSD 설명, §3.3 prior work 효과 측정(NoSSD 35%로 4배 ideal에 못 미침), §3.4 goal.
- **§4 Venice:** §4.1 interconnection network, §4.2 path reservation(scout packet/router reservation table), §4.3 non-minimal fully-adaptive routing(Algorithm 1, deadlock/livelock 회피).
- **§5 Methodology:** MQSim, 두 SSD 구성(Table 1), 19 workload(Table 2) + 6 mixed(Table 3), Venice params.
- **§6 Evaluation:** 성능/throughput/tail latency/mixed/path conflict 비율/power·energy/sensitivity/area.
- **§7 Related Work:** flash array parallelism 개선(HLNAND, Decoupled SSD — path diversity 부족), 활용(I/O scheduling: PAQ, PIQ, FLIN — orthogonal).
- **§8 Discussion:** NDP 적용성(operand를 여러 chip에서 효율적으로 모음), GC 개선(victim block valid page 이동을 host I/O와 병렬화).

## 핵심 용어

- **Path conflict:** 같은 channel을 공유하는 flash chip들로 향하는 다수 I/O가 단일 path를 두고 직렬화되는 현상.
- **Path diversity:** 컨트롤러-chip 간 경로 수. Venice는 mesh로 이를 늘림.
- **Flash node:** flash chip + 별도 router chip 한 쌍 (Venice 신규 building block).
- **Scout packet:** path reservation에 쓰는 특수 패킷(header/tail flit, reserve/cancel mode).
- **Router reservation table:** 각 router가 보유, 어떤 packet이 어느 entry/exit port를 예약했는지 기록.
- **Forward/backward path:** 예약된 bidirectional link의 write 방향/read 방향.
- **Non-minimal fully-adaptive routing:** minimal path가 막혔을 때 idle link로 우회해 conflict-free path를 찾는 라우팅.
- **Path-conflict-free SSD:** 각 chip이 direct separate channel을 갖는 이상적 상한(평가 기준선).

## 강점 · 한계 · 열린 질문

**강점**
- path conflict를 **근본적으로** 해소(99.98% conflict-free)하면서, flash chip을 수정하지 않아 commodity 부품 그대로 사용 가능.
- router를 chip에서 분리한 결정 덕에 면적(interconnect link는 baseline 대비 -44%)·전력(-90% per link) 오버헤드가 낮음.
- path reservation으로 라우터 대용량 buffer(NoSSD 16KB/port) 제거.
- read-intensive·고강도 mixed workload에서 특히 큰 이득.

**한계 / 열린 질문**
- 실측이 아닌 **MQSim 시뮬레이션** 기반. router/link는 합성·ORION 모델로 추정.
- router를 **별도 chip**으로 PCB에 추가 → 8% PCB 면적 오버헤드, BOM/패키징 비용·신뢰성 영향은 정성적 논의에 그침.
- 8 flash controller(8×8 mesh)에서 sweet spot. controller 수가 적거나(4) 많을(16) 때 이득 감소 — 구성 의존성.
- write-heavy workload에서는 flash latency가 지배해 이득 제한적.
- scout packet 재전송 등 추가 최적화는 extended version([103])에 위임.

## ❓ Q&A (자가 점검)

> [!question]- Q1. Path conflict가 정확히 무엇이고 왜 생기나?
> multi-channel shared bus에서 한 channel을 여러 flash chip이 공유하기 때문이다. 같은 channel에 붙은 서로 다른 chip을 향하는 I/O 요청들은 단 하나뿐인 path를 공유해야 하므로 직렬화되어, parallelism을 제한하고 service time을 늘린다.

> [!question]- Q2. Path conflict가 read와 write 중 어디에 더 치명적인가? 왜?
> read에 더 치명적이다. read는 data transfer time이 flash read latency와 비슷하거나 더 길어 transfer 직렬화 영향이 크다. write는 flash write latency(예: 100μs)가 service time을 지배해 path conflict 영향이 상대적으로 작다.

> [!question]- Q3. Venice가 NoSSD 같은 기존 interconnection 방식과 다른 핵심 차이는?
> (1) router를 flash chip 내부가 아닌 **별도 chip**으로 분리해 commodity flash chip을 수정하지 않고 chip당 pin·면적 오버헤드를 없앴다. (2) **path reservation**으로 라우터 대용량 buffer를 제거했다. (3) deterministic dimension-order routing 대신 **non-minimal fully-adaptive routing**으로 path diversity를 실제로 활용한다.

> [!question]- Q4. Path reservation은 어떻게 동작하며 왜 라우터 buffer를 없앨 수 있나?
> 데이터 전송 전에 scout packet을 보내 flash controller↔target chip 간 conflict-free path를 미리 예약한다(각 router가 reservation table 기록). 전송 시점엔 충돌이 없으므로 라우터가 데이터를 임시 저장할 큰 buffer가 필요 없다.

> [!question]- Q5. Non-minimal routing의 성능 오버헤드가 작은 이유는?
> 우회 경로는 hop이 늘어 command/data transfer time이 증가하지만, I/O data transfer는 크고(예: 16KB) flash operation latency가 전체 service time을 지배한다. 따라서 더 긴 command transfer가 service time에 미치는 영향은 negligible하다.

> [!question]- Q6. Venice는 deadlock과 livelock을 각각 어떻게 막나?
> Deadlock: scout가 conflict를 만나면 upstream router로 **backtracking**해 자원 순환 의존을 없앤다(자원 미가용으로 영구 block되지 않음). Livelock: scout가 같은 router를 **최대 3회**(2D mesh, 4-1)만 방문하도록 제한하고, 초과 시 path back-trace 후 최악의 경우 source controller로 복귀해 새 scout로 재시도한다.

> [!question]- Q7. 대표 성능 수치는?
> performance-optimized 구성에서 baseline 대비 평균 2.65×(최대 7.10×), prior best NoSSD 대비 1.92×. cost-optimized에서 baseline 대비 1.67×. 에너지는 baseline 대비 평균 61% 감소. I/O의 99.98%가 첫 시도에 conflict-free path 확보.

> [!question]- Q8. Venice의 면적 오버헤드가 낮다고 주장하는 근거는?
> flash chip 자체에는 오버헤드가 전혀 없다(router를 chip에서 분리). interconnect link는 baseline multi-channel shared bus 대비 총 면적이 **44% 낮고**(link wire가 8배 얇아도 됨), router는 PCB에 8% 면적만 추가한다.

## 🔗 Connections

→ [[Reliability]] · [[ISCA]] · [[2023]]

## References worth following

- NoSSD / Network-on-SSD — Tavakkol et al. [7, 38]: Venice가 직접 비교하는 최강 prior interconnection 방식.
- pSSD / pnSSD — Kim et al. [15]: packetization 기반 channel 대역폭·path 증가.
- MQSim — Tavakkol et al. [57, 58]: 평가에 쓴 multi-queue SSD simulator.
- HLNAND [128–130], Decoupled SSD [89]: flash array parallelism 개선 관련 work.
- FLIN [23], PAQ [40], PIQ [11]: I/O scheduling으로 internal parallelism 활용(orthogonal).
- Principles and Practices of Interconnection Networks [102] — Dally & Towles: routing/deadlock 이론 기반.
- Venice extended version [103] (arXiv): scout packet 재전송 등 추가 최적화.
- In-flash bitwise / NDP 관련 [24, 139]: Venice의 NDP 적용성 논의 배경.

## Personal annotations

<본인 메모 영역>
