---
title: "RoMe: Row Granularity Access Memory System for Large Language Models"
description: "LLM의 순차적·대용량 메모리 접근 패턴에 맞춰 HBM 인터페이스를 cache-line(32B) 대신 row(4KB) 단위로 재설계, bank group/pseudo channel을 제거하고 그 pin을 채널 확장에 재활용해 대역폭 12.5% 향상과 컨트롤러 단순화를 동시에 달성한 메모리 시스템"
venue: HPCA
year: 2026
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/llm
  - venue/hpca
  - year/2026
  - list/26s-v2
  - topic/hbm
  - topic/memory-controller
  - topic/llm-inference
  - topic/dram-architecture
---

# RoMe: Row Granularity Access Memory System for Large Language Models

> **HPCA 2026** · cluster/llm · Source: [RoMe - Row Granularity Access Memory System for Large Language Models.pdf](<RoMe - Row Granularity Access Memory System for Large Language Models.pdf>)

저자: Hwayong Nam†\* · Seungmin Baek†\* · Jumin Kim† · Michael Jaemin Kim‡ · Jung Ho Ahn† — †Seoul National University, ‡Meta (\*equal contribution)

## TL;DR
현대 HBM은 프로세서 cache-line 크기(32B/64B)에 맞춘 column-level 접근을 유지하기 위해 bank group과 pseudo channel(PC)이라는 계층을 추가로 도입했고, 이는 timing parameter·bank state·scheduling 복잡도를 계속 키워왔다. 그런데 LLM 추론은 weight/activation/KV-cache를 수백 KB~수십 MB 단위로 순차적으로 읽는 workload라, 굳이 32B 단위로 쪼갤 필요가 없다. RoMe는 MC-DRAM 인터페이스를 `RD_row`/`WR_row` 두 커맨드로 단순화하고, bank group·PC를 제거한 새 뱅크 구조 **virtual bank(VBA)**를 도입하며, row-level 커맨드를 내부 DRAM 커맨드 시퀀스로 풀어주는 **command generator**를 logic die에 얹는다. 그 결과 채널당 C/A pin이 18개→5개로 줄어(72% 절감), 절약된 pin으로 채널을 8→9개/die로 늘려 대역폭을 12.5% 높이면서도 MC scheduling 로직 면적은 기존 대비 9.1%, DRAM/logic die 면적 오버헤드는 0.10%에 그친다. DeepSeek-V3·Grok-1·Llama3-405B decode에서 TPOT을 HBM4 대비 9~10.4% 줄이고 에너지도 소폭(0.7~1.9%) 개선한다.

## 문제 & 동기
현재 DRAM 메인메모리(DDR5, HBM4)는 접근 단위(access granularity)를 프로세서 cache-line 크기에 맞춰왔다 — HBM4는 32B, DDR5는 64B. 이 미세한 단위를 유지하기 위해 bank group(대역폭을 뱅크 간 인터리빙으로 확장)과 pseudo channel(채널 폭을 줄이는 대신 개수를 늘림)이 세대마다 추가되었고, 세대가 지날수록 C/A(command/address) pin 대 DQ pin 비율이 거의 두 배씩 뛰고 있다(p.3, Fig.2). 반면 MC는 여러 bank state(7가지)·수많은 timing parameter(15개, Table IV)·큰 request queue(≥45 entries, HBM4 기준)를 유지해야 하는 복잡한 scheduler를 필요로 한다.

그런데 LLM 추론(GEMM/GEMV 지배적, MLA/GQA/MoE 등 최신 아키텍처 포함)은 한 번에 수백 KB~수십 MB의 데이터를 순차 접근한다(p.2, Fig.1: weight/activation/KV-cache 분포). Grok-1의 예외적인 24KB 행렬 하나를 빼면 모든 weight 행렬이 12MB를 넘고, decode 단계 KV-cache는 그보다 더 크다. 이런 순차·대용량 패턴에서는 cache-line 단위 접근이 오히려 불필요한 복잡성만 유발한다는 것이 저자들의 문제의식이다.

> [!quote]- 📄 원문 표현 (paper)
> - "In a conventional HBM-based memory system, these transfers are fragmented into hundreds of 32 B cache line transactions. This forces the memory controller to employ unnecessarily intricate scheduling, leading to growing inefficiency." (p.1, Abstract)
> - "Instead of issuing multiple consecutive read commands, we ask: why can't DRAM access granularity simply increase to match the row-level granularity in this scenario?" (p.1)
> - "During LLM execution, tens of megabytes of data typically need to be accessed sequentially at a time." (p.5)

## 핵심 통찰 (Key Insight)
1. **LLM 접근 패턴은 본질적으로 순차·대용량(coarse-grained)이라 overfetch 위험 없이 access granularity를 row 단위까지 올릴 수 있다.** 기존 fine-grained 아키텍처들은 낮은 spatial locality를 가정해 overfetch를 줄이려 했지만, LLM의 GEMM/GEMV는 애초에 row 전체를 다 쓰기 때문에 반대로 coarse-grained화가 유효하다.
   > [!quote]- 📄 원문 표현 (paper)
   > - "By exploiting the memory access patterns of LLMs, we propose RoMe, a Row-granularity-access Memory system designed to offer a simple and scalable memory system for LLM serving." (p.2)

2. **Row granularity에서는 AG_bank와 AG_MC를 cache-line 크기에 맞출 필요가 없으므로 bank group·PC 자체가 불필요해진다** — 이를 대체하는 새 계층이 **virtual bank(VBA)**다. VBA는 단일 유닛으로 최대 대역폭을 내도록 설계되어 MC가 bank group/PC interleaving을 관리할 필요 없이 VBA 간 interleaving만 신경 쓰면 된다.
   > [!quote]- 📄 원문 표현 (paper)
   > - "The key idea behind VBA is to deliver the full available bandwidth from a single VBA, eliminating the need for complex MC-side scheduling that accounts for bank group or PC interleaving." (p.6)

3. **Command generator를 logic die에 두어 row-level 커맨드(`RD_row`/`WR_row`)를 고정된 DRAM 커맨드 시퀀스로 정적으로(statically) 변환한다.** 기존 MC처럼 bank state·timing constraint를 동적으로 추적하지 않고 미리 정해진 간격으로 커맨드를 쏘기만 하면 되므로, C/A pin 수 자체를 줄일 수 있고 그 여유 pin을 채널 확장에 쓸 수 있다.
   > [!quote]- 📄 원문 표현 (paper)
   > - "Unlike a conventional MC, our command generator does not issue commands dynamically based on bank states or timing constraints. Instead, it issues predetermined DRAM commands at fixed intervals upon receiving a row-level command, operating in a simplified and static manner." (p.7)

## 설계 / 메커니즘 (Design)

**메모리 인터페이스 (§IV-A).** column-level 인터페이스(RD/WR + bank group/PC 지정)를 row-level 인터페이스로 교체: MC는 `RD_row`/`WR_row` 두 커맨드만 발급하며, AG_MC가 row 크기(RoMe에서 4KB, Table V)로 커진다. Logic die의 command generator가 이를 실제 DRAM 커맨드로 분해한다.

**Virtual bank, VBA (§IV-B, Fig.7~8).** bank group 제거를 위한 3가지 설계 후보(Fig.7b/c/d)와 PC 제거를 위한 2가지 후보(Fig.8a/b)를 조합해 총 6가지 구성을 모두 시뮬레이션했고 성능 편차는 3.6% 이내였지만, 면적 관점에서 차이가 컸다(p.6). 최종 채택안은 **Fig.7(d) + Fig.8(b)**: 서로 다른 bank group에 속한 두 뱅크가 time-multiplexed로 인터리빙되어 하나의 VBA를 이루고(내부 DRAM 구조 변경 불필요), 두 PC가 동시에 동작해 각자 자기 데이터 fetch 크기를 유지한다(legacy HBM1/2 channel mode와 유사). 이 조합은 추가 wiring/buffer 없이 대역폭을 두 배로 늘리면서 effective row 크기를 4KB로 만든다(1KB 베이스 row × 2뱅크 × 2PC 동시성 관점, Table V). Fig.7(b)(단일 뱅크가 AG_bank를 2배로 늘려 VBA가 되는 안)은 BK-BUS 폭과 I/O ctrl buffer를 두 배로 늘려야 해서 면적 오버헤드가 최대 77%까지 커진다(p.7, [51] 인용).

**Command generator 배치 (§IV-C, Fig.9, Fig.11).** MC/logic die/DRAM die 세 위치를 검토: MC에 두면 기존 시스템 변경은 최소화되나 C/A pin 절감 이득을 못 얻고, DRAM die에 두면 TSV 절감은 가능하지만 die마다 하나씩 필요해 redundancy가 생긴다. RoMe는 **logic die**를 절충안으로 채택 — HBM4 logic die는 애초에 로직 공정으로 제작되고, hybrid bonding 등으로 die 간 TSV 배치 비용이 낮아지는 추세이기 때문이다(p.6). 두 뱅크를 tCCDS 간격으로 완벽히 인터리빙하기 위해, ACT 간 최소 간격 tRRDS를 만족시키려고 `tRRDS − tCCDS`만큼의 intentional delay를 첫 뱅크 ACT 전에 삽입한다(Fig.9).

**C/A pin 절감 (§IV-D, Fig.10).** bank group·PC 개념이 사라지므로 column RD/WR C/A pin(8개)이 불필요해지고, MRS도 row C/A pin으로 전송된다. bank address bit 하나도 VBA당 2뱅크 구조 덕에 불필요해진다. 결과적으로 ACT/PRE 제외 8개 row 커맨드 + MRS/RD_row/WR_row로 11개 커맨드를 5개 C/A pin만으로 표현 가능(REF 직후 RD_row/WR_row 간 최소 간격이 2×tRRDS 이상이라 5핀으로도 충분히 빠르게 커맨드를 보낼 수 있음, Fig.10). 이를 통해 **채널당 C/A pin을 18개에서 5개로, 72% 절감**한다.

**추가 채널 (§IV-E).** 절감된 13개 pin/channel을 이용해 HBM4 채널(120 pin) 대신 RoMe 채널(107 pin)을 구성, 32-channel 구성 기준 416개의 여유 pin이 생겨 단 12개의 추가 pin만으로 4개 채널을 더 추가할 수 있다. die당 채널 수를 8→9로 늘려 **큐브당 채널 32→36, 대역폭 약 12.5% 증가**(Table V: HBM4 2TB/s → RoMe 2.25TB/s).

**RoMe MC 아키텍처 (§V-A, Table III~IV, Fig.11).** row-level 커맨드만 다루므로 conventional MC의 ACT/PRE/RD/WR 간 timing constraint가 사라진다. Bank state는 Idle/Writing/Reading/Refreshing 4가지(conventional 7가지)로 단순화되고, RD_row/WR_row 완료 시 자동으로 Idle로 복귀한다. Timing parameter는 Read-to-Read/Write-to-Write 등 10종(Table III, tR2RS/tR2RR/tR2WS/tR2WR/tW2RS/tW2RR/tW2WS/tW2WR/tRD_row/tWR_row)뿐이며, 동일 stack ID(SID)가 아닌 접근에는 1~2nCK의 추가 지연이 붙는다. Bank FSM은 RoMe가 한 번에 최대 2개 VBA만 구동하므로 기본 2개만 필요하지만, per-bank refresh 추적을 위한 FSM이 더해져 Table IV 기준 5개다. Request queue depth는 HBM4의 ≥45 entries 대비 RoMe는 단 2 entries로 충분(4KB 요청 자체가 이미 하나의 큰 요청이라 bank-level parallelism 확보에 깊은 큐가 불필요). Scheduling은 FR-FCFS 기반 age-based 방식으로, active VBA를 먼저 확인 후 oldest-first로 ready request를 서빙한다. Page policy 자체가 불필요(row-granularity 접근은 항상 즉시 precharge).

**Refresh/Write 처리 (§V-B).** All-bank refresh(REFab)는 baseline과 동일하게 처리. Per-bank refresh(REFpb)는 VBA 내 한 뱅크만 blocking해도 전체 VBA가 막히므로, 매 tREFIpb마다 REFpb를 발급하는 대신 **2×tREFIpb마다 한 번**, command generator가 VBA 내 두 뱅크에 tREFRD 간격으로 REFpb 두 개를 순차 발급 — VBA당 stall time을 2×tRFCpb에서 tRFCpb+tREFRD로 줄인다(예: 2×280ns → 280ns+8ns). Write는 4KB write buffer 없이 도착 즉시 처리(LLM은 read-dominant이므로 영향 미미).

> [!quote]- 📄 원문 표현 (paper)
> - "By reducing the number of C/A pins to five, RoMe is able to eliminate 72% of the C/A pins." (p.8)
> - "RoMe MC employing a highly simplified scheduler treats each 4 KB access as a single request, enabling it to saturate DRAM bandwidth with a significantly smaller request queue." (p.9)
> - "Row-level access removes the need for any page-policy mechanism." (p.9)

## 평가 (Evaluation)
**환경 (§VI-A).** 목표 가속기: BF16 arithmetic intensity 280 Op/B(NVIDIA B200 281 Op/B 참고), HBM4 큐브 8개(각 32GB, 16-Hi, 8Gbps) → 총 256GB, 16TB/s, BF16 4480TFLOPS. 8-accelerator 시스템을 LLMSimulator + Ramulator 2.0(cycle-accurate)으로 시뮬레이션, 4KB 요청 단위 구현. 평가 LLM은 Grok-1(GQA+MoE, 2/8 expert), DeepSeek-V3(MLA+MoE, 8/256 expert), Llama3-405B(GQA, dense FFN). Baseline은 open-page policy의 HBM4 MC, RoMe는 앞서 설명한 단순 MC.

**Decode TPOT (p.11, Fig.12, seq len 8K, 여러 batch size).** RoMe는 HBM4 대비 TPOT을 **DeepSeek-V3 10.4%, Grok-1 10.2%, Llama3 9.0%** 감소시킨다. 이는 채널 12.5% 증가에 비례하지 않는데, FFN 레이어 등 일부가 memory-bound가 아니기 때문이다.

**Channel Load Balance Ratio, LBR (p.12, Fig.13).** RoMe의 4KB 접근 단위에서 채널 간 데이터 분산이 균일한지 측정한 지표로, attention/FFN 레이어 모두 여러 batch size에서 HBM4 baseline(≈1) 대비 큰 편차 없이 유지 — row-granularity로 인한 load imbalance가 실질적으로 무시할 만한 수준임을 보인다. 모델별 hidden dimension 차이(DeepSeek-V3 7168·Grok-1 6144·Llama3 16384)와 병렬화 전략(TP/DP, MoE expert parallelism) 차이로 LBR 패턴이 갈린다.

**Prefill.** compute-bound 특성상 HBM4/RoMe 성능 차이가 **전 모델에서 0.1% 이내**로 사실상 없음(p.12).

**에너지 (p.13, Fig.14, batch=256).** RoMe는 HBM4 대비 에너지를 **DeepSeek-V3 1.9%, Grok-1 0.7%, Llama3 0.7%** 절감. ACT 에너지가 baseline 대비 각각 55.5%/86.0%/84.4%로 감소하는 것이 주 요인(row 단위로 한 번만 ACT를 하면 되므로 데이터량과 무관하게 ACT 횟수가 최소화됨). Command generator 에너지 오버헤드는 전체 대비 평균 0.06%로 미미.

**면적 오버헤드 (§VI-C).** μbump pitch 22μm, 4개 추가 채널에 필요한 TSV 위해 채널당 4배 μbump 수 증가 가정 → 48개 추가 μbump, TSV 면적 약 0.14mm², DRAM die 면적 +12%, logic die도 비례 증가 → **총 면적 오버헤드 0.10%**. Command generator 자체는 큐브당(36채널 기준) 4268.8μm², logic die 면적의 **0.003%**. MC scheduling 로직(scheduler+bank FSM+queue) 면적은 conventional MC(queue depth 64) 대비 RoMe(queue depth 4, 둘 다 FR-FCFS)가 **9.1%**만 차지.

> [!quote]- 📄 원문 표현 (paper)
> - "RoMe reduces TPOT by 10.4%, 10.2%, and 9.0% of HBM4 for DeepSeek-V3, Grok 1, and Llama 3, respectively." (p.12)
> - "the scheduling logic in RoMe MC occupies only 9.1% of the area of a conventional MC, indicating that RoMe achieves a much simpler architecture." (p.13)
> - "RoMe reduces energy consumption by 1.9%, 0.7%, and 0.7% for the three evaluated LLMs, respectively." (p.13)

## 섹션 노트
- **§I Introduction**: cache-line 단위 접근이 HBM 세대별로 bank group·PC를 계속 추가시켜 MC 복잡도를 키웠음을 지적하고, LLM의 순차 접근 특성을 활용한 row-granularity 인터페이스 RoMe를 제안. 4가지 기여(row-level 인터페이스, VBA, 단순화된 MC, 채널 확장)를 명시.
- **§II Conventional memory systems**: cache-line granularity의 목적(overfetch 방지, 다양한 access pattern 지원)과 bank group/PC 도입 배경, HBM 물리 구조(TSV, SID, channel/PC), conventional MC의 4대 구성요소(address mapping, R/W queue, per-bank state, command scheduler)를 정리.
- **§III Access pattern of LLMs**: prefill/decode 두 단계와 weight/activation/KV-cache 세 데이터 타입의 크기 분포(Fig.1)를 근거로 LLM의 순차·대용량 접근 특성을 논증.
- **§IV RoMe interface**: 인터페이스(RD_row/WR_row), VBA, command generator 배치, C/A pin 절감, 추가 채널까지 RoMe의 핵심 설계를 상세 기술(위 Design 절 요약).
- **§V Memory system under RoMe**: MC 아키텍처 단순화(timing param/FSM/state/queue/scheduling) 및 refresh/write 최적화.
- **§VI Evaluation**: 시스템/시뮬레이션 방법론, TPOT·LBR·에너지·면적 결과.
- **§VII Discussion**: 확장 가능성 — 더 큰 ECC codeword, sparse attention(gpt-oss, DeepSeek-V3.2 DSA) 등 fine-grained workload를 위한 hybrid RoMe+HBM4 아키텍처, TPU류 brawny-core 프로세서와의 co-design, training(microbatch 8192 tokens라 이미 coarse-grained) 적용 가능성, HBM 외 DRAM(DDR5 등, logic die 없음) 적용 시 pin 재활용 전략 차이.
- **§VIII Related work**: coarse-grained locality 관련 선행연구([1,3,36,59,74,75,81])와 fine-grained DRAM 아키텍처 선행연구([2,4,7,17,35,51,53,64,65,69,79,80,83]) 대비, RoMe는 LLM의 극단적 순차성을 활용해 반대 방향(더 coarse하게)으로 최적화한 점이 차별점.
- **§IX Conclusion**: row-granularity 인터페이스·VBA·command generator를 통해 스케줄링/하드웨어 오버헤드를 낮추면서 성능·에너지 효율을 동시에 높였다고 요약.

## 핵심 용어 (Key terms)
- **RoMe**: Row-granularity-access Memory system. HBM 인터페이스를 column-level에서 row-level로 바꾼 이 논문의 제안 시스템.
- **AG_bank / AG_MC**: 각각 뱅크 단위 접근 단위(access granularity)와 메모리 컨트롤러 관점의 접근 단위. 기존엔 cache-line 크기로 일치시켜야 했으나 RoMe는 row 크기로 확장.
- **Virtual bank (VBA)**: bank group과 pseudo channel을 대체하는 새 뱅크 계층. 서로 다른 bank group의 두 뱅크를 인터리빙해 단일 유닛처럼 최대 대역폭을 제공.
- **RD_row / WR_row**: RoMe가 MC-DRAM 인터페이스에서 사용하는 두 개의 row-level 커맨드.
- **Command generator**: logic die에 배치되어 row-level 커맨드를 ACT/RD·WR 시퀀스/PRE 같은 conventional DRAM 커맨드로 정적으로 변환하는 유닛.
- **Bank group / Pseudo channel (PC)**: 기존 HBM이 cache-line 단위 접근을 유지하면서 대역폭을 늘리기 위해 도입한 계층 구조. RoMe에서는 제거 대상.
- **C/A pin**: command/address pin. Row/column 커맨드 전송에 쓰이며 세대가 지날수록 DQ pin 대비 비중이 커져온 오버헤드 요소.
- **TPOT**: Time Per Output Token. decode 단계 성능 지표.
- **LBR (channel Load Balance Ratio)**: 여러 메모리 채널에 데이터가 얼마나 균일하게 분산되는지 나타내는 지표. 1에 가까울수록 균일.
- **Prefill / Decode**: LLM 추론의 두 단계 — 입력 토큰 전체를 한 번에 처리(compute-bound에 가까움) vs 토큰을 하나씩 자동회귀 생성(memory-bound에 가까움).
- **REFab / REFpb**: All-bank refresh(전체 뱅크 동시 정지) vs per-bank refresh(뱅크 단위, VBA 내 한 뱅크만 막혀도 전체 VBA 블로킹).

## 강점 · 한계 · 열린 질문
- **강점**: (1) LLM 고유의 순차·대용량 접근이라는 workload 특성을 정량적으로 근거(Fig.1)로 제시하고, 이를 활용해 MC의 근본 복잡도(timing param 15→10, bank state 7→4, queue depth 45→2, page policy 자체 제거)를 낮췄다. (2) 단순화로 절약한 C/A pin을 대역폭 확장(채널 추가)에 재투자하는 아이디어가 깔끔하다 — "단순화 = 성능 저하"라는 통념을 깨고 단순화가 곧 대역폭 증가로 직결되는 설계. (3) VBA 설계 공간을 6가지 조합으로 모두 실험해 성능(3.6% 이내 편차) vs 면적(최대 77% 차이) trade-off를 투명하게 제시.
- **한계**: (1) row 단위(4KB)로 커지면서 fine-grained/irregular 접근(sparse attention, MoE routing 등)에서는 overfetch 위험이 커질 수 있음 — 논문도 §VII에서 gpt-oss의 DSA(top-2048 token 선택) 같은 workload에는 취약할 수 있다고 인정. (2) 평가가 시뮬레이션(LLMSimulator+Ramulator2.0) 기반이며 실제 실리콘 검증은 없음; HBM4 timing도 JEDEC 미확정이라 선행 연구값을 차용([2],[51]). (3) command generator의 logic die 배치는 절충안일 뿐, DDR5 등 logic die가 없는 DRAM에는 그대로 적용 불가(§VII "Other types of DRAM"에서 저자도 명시).
- **열린 질문**: RoMe+HBM4 하이브리드(§VII)를 실제로 구현할 때 요청을 어떻게 fine/coarse로 분류·라우팅할 것인가? Processor-RoMe co-design(TPU류 brawny core)이 GPU 같은 many-core 가속기에도 적용 가능한가, 아니면 근본적으로 architecture-specific인가? 4KB보다 더 큰 row(예: KV-cache가 특히 큰 decode 후반부)에 적응적으로 대응하는 방안은?

## ❓ Q&A (자가 점검)
> [!question]- RoMe가 bank group과 pseudo channel을 제거할 수 있는 근본 이유는?
> row-level 접근에서는 AG_MC가 이미 row 크기이므로, AG_bank를 굳이 cache-line 크기에 맞출 필요가 없다. Bank group/PC는 원래 "cache-line 크기를 유지하면서 대역폭을 늘리기 위한" 장치였기 때문에, 그 전제(cache-line 유지)가 사라지면 존재 이유도 사라진다.

> [!question]- VBA는 정확히 어떻게 구성되는가?
> 서로 다른 bank group에 속한 두 개의 물리 뱅크가 time-multiplexed로 인터리빙되어(Fig.7d) 하나의 VBA를 이루고, 그 위에서 두 개의 pseudo channel이 동시에 각자 데이터를 fetch(Fig.8b)한다. 이 조합이 내부 DRAM 구조 변경 없이 최대 대역폭과 4KB effective row를 준다.

> [!question]- Command generator는 왜 logic die에 배치되었나?
> MC에 두면 C/A pin 절감 효과를 못 얻고, DRAM die에 두면 die마다 하나씩 필요해 redundant하다. Logic die는 이미 로직 공정으로 제작되고(면적 오버헤드 최소), hybrid bonding 등으로 die 간 배선 비용이 낮아지는 추세라 절충안으로 채택됐다(p.6).

> [!question]- C/A pin이 18개에서 5개로 줄어드는 근거는?
> Bank group/PC 제거로 column RD/WR C/A pin(8개)이 불필요해지고, MRS는 row C/A pin으로 전송, VBA가 2뱅크 구조라 bank address bit 하나도 불필요. 최종적으로 ACT/PRE 제외 8개 row 커맨드 + MRS/RD_row/WR_row 3개 = 11개 커맨드를 5개 핀 인코딩으로 표현 가능하며, REF 직후 최소 커맨드 간격이 2×tRRDS 이상이라 5핀으로도 타이밍을 만족한다(Fig.10).

> [!question]- 절약된 C/A pin으로 얻는 대역폭 이득은 얼마이며 어떻게 계산되는가?
> 채널당 13핀 절감(HBM4 120핀→RoMe 107핀), 32-channel 구성 기준 416개 여유 핀 확보, 12개 핀만으로 채널 4개 추가(die당 8→9채널, 큐브당 32→36채널) → 대역폭 12.5% 증가(Table V: 2TB/s→2.25TB/s).

> [!question]- Prefill에서는 왜 RoMe와 HBM4 성능 차이가 거의 없는가(<0.1%)?
> Prefill은 많은 입력 토큰을 한 번에 batch 처리하는 compute-bound 단계라 GEMM 연산이 지배적이고, 메모리 시스템 종류에 대한 민감도가 낮기 때문(p.12).

> [!question]- Load Balance Ratio(LBR)가 왜 중요한 지표인가?
> RoMe는 4KB라는 상대적으로 큰 granularity로 채널에 데이터를 분산하므로, 특정 채널에 데이터가 쏠리는 load imbalance가 대역폭 활용을 저해할 수 있다. LBR이 HBM4 baseline(≈1) 대비 유의미하게 낮아지지 않는다는 것을 보임으로써, row-granularity로 인한 이 부작용이 미미함을 입증한다(Fig.13).

> [!question]- RoMe가 근본적으로 취약할 수 있는 workload는?
> Sparse attention처럼 접근 패턴이 예측 불가능하고 irregular한 workload(예: DeepSeek Sparse Attention이 시퀀스 길이 2048 초과 시 top-2048 토큰만 선택하는 경우)에서는 4KB row 단위 fetch가 overfetch를 유발해 성능이 저하될 수 있다(§VII).

## 🔗 Connections
[[LLM Systems]] · [[HPCA]] · [[2026]]
관련: [[PF-LLM - Large Language Model Hinted Hardware Prefetching]] · [[Mooncake - Trading More Storage for Less Computation - A KVCache-centric Architecture for Serving LLM Chatbot]]

## References worth following
- [27] JEDEC, "High Bandwidth Memory DRAM (HBM4) Standard," 2025 — RoMe가 기준으로 삼는 HBM4 스펙(채널/PC/pin/대역폭 수치의 근거).
- [38] Kim et al., "Ramulator 2.0: A Modern, Modular, and Extensible DRAM Simulator" — 본 논문의 cycle-accurate DRAM 시뮬레이션 백엔드.
- [77] xAI, "grok1" (2024) 및 [12][13] DeepSeek-V3, Llama 3 기술 리포트 — 평가에 쓰인 세 LLM 아키텍처(MLA/GQA/MoE)의 원 출처.
- [51] O'Connor et al., "Fine-Grained DRAM: Energy-Efficient DRAM for Extreme Bandwidth Systems," MICRO 2017 — RoMe가 대비하는 fine-grained 접근 계열의 대표 선행연구, VBA(b) 설계의 면적 오버헤드(77%) 근거로도 인용.
- [54] OpenAI, "gpt-oss" 및 [11] DeepSeek-AI, "DeepSeek-V3.2-Exp" — RoMe가 취약할 수 있다고 논한 sparse-attention 기반 최신 LLM들(Discussion §VII).

## Personal annotations
<!-- 본인 메모 영역 -->
