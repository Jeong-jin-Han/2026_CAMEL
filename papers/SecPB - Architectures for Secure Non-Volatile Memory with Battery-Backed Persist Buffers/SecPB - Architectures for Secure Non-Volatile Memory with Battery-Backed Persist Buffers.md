---
title: "SecPB: Architectures for Secure Non-Volatile Memory with Battery-Backed Persist Buffers"
description: "persistent hierarchy에서 벌어지는 PoP/SPoP recoverability gap을 battery-backed SecPB로 메워, secure NVM의 strict persistency 오버헤드를 118.4%에서 1.3%까지 낮추는 설계 스펙트럼 연구"
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
  - topic/persistent-memory
  - topic/secure-memory
  - topic/crash-consistency
  - topic/memory-encryption
---

# SecPB: Architectures for Secure Non-Volatile Memory with Battery-Backed Persist Buffers

> **HPCA 2023** · cluster/reliability · Source: [SecPB - Architectures for Secure Non-Volatile Memory with Battery-Backed Persist Buffers.pdf](<SecPB - Architectures for Secure Non-Volatile Memory with Battery-Backed Persist Buffers.pdf>)

저자: Alexander Freij (North Carolina State University), Huiyang Zhou (North Carolina State University), Yan Solihin (University of Central Florida)

## TL;DR
persistent memory(PM)를 캐시 계층까지 확장하는 "persistent hierarchy" 접근(BBB의 battery-backed persist buffer 등)은 store의 Point of Persistency(PoP)를 코어 가까이로 당기지만, counter/MAC/Merkle tree root 같은 security metadata가 persistent해지는 시점(Security PoP, SPoP)은 여전히 memory controller에 남아 새로운 "recoverability gap"을 만든다. 이 논문은 이 gap을 없애기 위해 secure persistent buffers(SecPB)라는 battery-backed 구조를 core 옆에 추가로 두고, "crash recovery observer는 crash 시점이 아니라 recovery 시점의 상태만 일관되면 된다"는 관찰을 이용해 security metadata 갱신을 early(즉시)~late(crash 후) 스펙트럼 상에서 지연시키는 6가지 설계(NoGap/M/CM/BCM/OBCM/COBCM)를 제안한다. Gem5+SPEC2006 평가 결과 가장 lazy한 COBCM은 BBB(무보안) 대비 평균 1.3% 오버헤드, secure eADR 대비 753배 작은 배터리로 strict persistency·암호화·integrity verification을 동시에 달성한다.

## 문제 & 동기
Persistent hierarchy(예: Intel eADR, battery-backed buffer(BBB))는 캐시 계층 전체를 persistence 도메인에 포함시켜 flush/fence를 없애고 strict persistency(SP) 모델의 성능 오버헤드를 크게 줄인다. 그러나 이 논문은 이 흐름이 보안(암호화+integrity tree)과 결합될 때 새로운 문제가 생김을 지적한다: PoP(store가 persistent해지는 지점)는 코어 근처(L1D 급의 persist buffer)로 올라가지만, 데이터의 security metadata(memory tuple: ciphertext, counter, MAC, BMT root)가 실제로 persistent해지는 SPoP는 여전히 memory controller에 머물러, PoP와 SPoP 사이에 "recoverability gap"이 생긴다(Fig.1, p.677). 이 gap이 있으면 crash 후 복구 시 잘못된 plaintext를 복원하거나 integrity verification이 실패할 수 있다.

Naive하게 SPoP를 PoP에 맞추면(모든 security metadata를 store가 persist buffer에 들어가는 즉시 갱신) BMT root 갱신처럼 수백 cycle이 걸리는 연산이 critical path에 들어가 "매우 심각한 성능 병목"이 된다(p.678).

> [!quote]- 📄 원문 표현 (paper)
> - "While improving performance and programmability, persistent hierarchy creates a *new recoverability gap* as illustrated in Figure 1(b)." (p.678)
> - "Therefore, high-latency operations, especially BMT root update which may take hundreds of clock cycles, have now become part of the critical path of data persist. Therefore, a naive SecPB design incurs a very formidable new performance bottleneck." (p.678)
> - "Authors of PLP [18] pointed out that the memory tuple has to be atomically updated, hence the SPoP is moved up to match the PoP. On the other hand, persistent hierarchy moves up PoP to match the PoV to close the visibility and persistency gap." (p.677)

## 핵심 통찰 (Key Insight)
1. **Recovery observer는 crash 시점이 아니라 recovery 시점의 일관성만 요구한다.** Crash와 recovery 사이에는 battery로 커버되는 시간 window가 존재하므로, 이 구간 동안 data/metadata를 "lazily" consistent하게 맞춰도 된다. 이것이 early(즉시 갱신)~late(crash 후 일괄 갱신) 스펙트럼의 근거이며, 성능/배터리 trade-off를 열어준다.
2. **Data-value-independent metadata는 store마다가 아니라 block당 한 번만 갱신해도 된다.** Counter/OTP/BMT root처럼 plaintext 값에 의존하지 않는 metadata는, crash observer가 SecPB가 drain된 이후의 상태만 보므로 같은 block에 대한 여러 store를 하나의 갱신으로 coalescing할 수 있다(반면 ciphertext/MAC 같은 data-value-dependent metadata는 이 최적화 대상이 아니다).
3. **성능/배터리 trade-off는 이산적 스펙트럼(design points)으로 파라미터화할 수 있다.** metadata 의존성 그래프(counter→OTP→BMT root, counter→ciphertext→MAC) 상에서 어디까지를 store 시점(early)에, 어디부터를 crash 후(late)에 계산할지 선택하는 것만으로 NoGap/M/CM/BCM/OBCM/COBCM 6개 지점을 얻는다.

> [!quote]- 📄 원문 표현 (paper)
> - "The optimizations are enabled by a key observation: the crash recovery observer does not need to see the cache/memory state at the time of a crash, only at the time of recovery. Thus, there exists a time gap between a crash and recovery where data and metadata can be made consistent." (p.678)
> - "For split counter and BMT scheme, security metadata can be divided into data value dependent ones (data ciphertext and MAC) ... versus data value independent ones (counters, OTPs, and BMT) that are computed without relying on the value of data plaintext." (p.682)
> - "the counter for the block can be incremented once (instead of once per store) when the block first becomes dirty." (p.682)

## 설계 / 메커니즘 (Design)
- **BBB → SecPB 재설계 (Fig.2, p.679).** 기존 BBB는 core마다 persist buffer(PB)를 두어 L1D와 동시에 store를 받고, 하이워터마크에서 PM으로 drain한다. SecPB는 여기에 CTR$/BMT$/MAC$ 같은 metadata cache, AES 암호화 엔진, BMT root 레지스터를 MC 쪽에 두고, SecPB 컨트롤러가 PoV·PoP·SPoP를 정렬하도록 조율한다.
- **Draining gap / sec-sync gap (Fig.3, p.680).** Early(naive) 전략은 store가 PB에 들어가는 즉시 SPoP까지 맞추므로 draining gap만 존재하지만 critical path가 느려진다. Late 전략은 draining 이후에도 security metadata 갱신이 못 따라가는 "sec-sync gap"이 추가로 생기고, 배터리는 이 두 gap을 모두 커버해야 한다. Hybrid 전략은 SPoP를 crash 이전(SPoP/1)과 이후(SPoP/2)로 나눠 절충한다.
- **6가지 design point (Fig.4, p.681-682).** metadata 의존성 그래프 상에서 counter increment → OTP 생성 → BMT root 갱신(counter 경로), counter → ciphertext → MAC(암호화 경로) 중 어디까지를 store 시점(E)에 하고 어디부터를 crash 후(L)에 미루는지로 정의된다.
  - **NoGap**: 모든 metadata를 즉시(eager) 갱신 — recoverability gap을 완전히 제거하지만 가장 느림.
  - **M**: MAC 계산만 crash 후로 미룸 — 같은 block에 여러 store가 있어도 dirty block당 MAC 1회만 생성.
  - **CM**: ciphertext 생성과 MAC을 모두 crash 후로 미룸(counter-mode encryption이라 XOR 1 cycle만 필요해 M과 성능은 비슷하지만 일 자체가 줄어듦).
  - **BCM**: BMT root 갱신까지 crash 후로 미룸 — 최대 8-level tree 왕복을 critical path에서 제거.
  - **OBCM**: OTP 생성까지 crash 후로 미룸.
  - **COBCM**: counter increment까지 포함해 모든 security metadata 갱신을 crash 후로 미루는 가장 lazy한 설계.
- **SecPB 아키텍처(Fig.5, p.682).** SecPB 엔트리는 Dp(plaintext 64B), O(OTP 64B), Dc(ciphertext 64B), C(counter 8b), B(BMT ack 1b), M(MAC 512b) 필드로 구성되고, 설계별로 어떤 필드를 채우는지가 다르다(Fig.5 상단 표). Store가 retire하면 L1D와 SecPB에 동시에 접근하고, 둘 다 miss면 MC의 metadata cache(CTR$/BMT$/MAC$)에서 fetch한다. 모든 valid bit이 set되어야 drain 가능하며, 이때 NoGap은 store buffer에 unblock 신호를 즉시 보내는 반면 COBCM은 PB에 새 값이 들어오는 즉시 다음 store를 받을 수 있어 stall이 거의 없다.
- **Cache coherence / inclusion (Section IV-C, p.683).** dirty block은 SecPB에서 PM으로 직접 drain되므로 LLC eviction 시 write-back이 필요 없다(special dirty state로 silently discard). Metadata cache는 directory로 태깅해 어느 코어의 SecPB에 엔트리가 있는지 추적하고, miss 시 SecPB 간에 엔트리를 migration하여 replication/coherence state 중복을 피한다.

> [!quote]- 📄 원문 표현 (paper)
> - "The most eager design is NoGap, which eliminates the sec-sync gap entirely by updating all security metadata and persisting them early." (p.681)
> - "The final model, COBCM, removes all security metadata updates from the critical write path and incurs an average overhead of nearly-negligible 1.3% compared to BBB without any security mechanisms." (p.686)
> - "To avoid that, the metadata caches are tagged with a directory that indicate which SecPB the metadata may also reside in." (p.683)

## 평가 (Evaluation)
Gem5 cycle-accurate 시뮬레이션(1-core OOO x86_64 4GHz, L1/L2/L3 + 32-entry WPQ, 8GB PCM, BMT 8-level, MAC latency 40 cycles — Table I, p.684), SPEC2006 18개 benchmark를 fast-forward 후 250M instruction 시뮬레이션(p.684).

- **성능 오버헤드 (32-entry SecPB, Table IV p.685):** BBB(무보안) 대비 slowdown — COBCM 1.3%, OBCM 1.5%, BCM 14.8%, CM 71.3%, M 73.8%, NoGap 118.4%. BCM→CM 구간에서 가장 큰 낙차(71.3%p)가 나는데, 이는 BMT root 갱신 latency가 critical path에서 빠지는 효과다(p.685).
- **benchmark별 편차 (Fig.6, p.686):** gamess는 noGap/M/CM 세 스킴에서 y축 범위(최대 6×)를 넘는 극단적 이상치를 보여 그래프 위에 18.2~20.6배 수준의 실측값이 별도 숫자로 표기되어 있다. 논문은 gamess의 PPTI(persists per thousand instructions) 47.4·NWPE(writes per SecPB entry) 2.1을 근거로, 8-level BMT마다 leaf-to-root 320 cycle(=8×40)이 반복되는 write locality가 원인이라고 분석하며, 이로부터 추정 IPC 0.11이 실제 IPC 0.13과 근접함을 확인한다(p.685).
- **배터리/에너지 (Table V, p.686-687):** SuperCap 기준 COBCM은 core 대비 4.89 mm³(53.6%)면 충분한 반면, secure eADR(s_eADR)은 3706 mm³(4459.6%)가 필요 — "753× decrease in the required battery capacity to support SecPB compared to s_eADR" (p.687). Li-Thin 기준으로도 COBCM 0.049 mm³(2.5%) vs s_eADR 37.06 mm³(206.9%)로 같은 경향을 보인다.
- **SecPB 크기 영향 (Fig.7/8, Table VI, p.686-687):** entry 수를 8→512로 늘리면 BMT root 갱신 횟수가 crop되어(8-entry도 이미 12.7%로 감소, 512-entry는 1.8%) coalescing 효과가 커지지만, 32~64 entry를 넘으면 성능 개선은 diminishing return이고 배터리 비용만 커져 32 entry를 default로 채택한다.
- **BMT height reduction과의 결합 (Fig.9, p.687-688):** 기존 Bonsai Merkle Forest 기법(DBMF: BMT height 2, SBMF: height 5, 4KB root cache)을 CM 모델에 적용하면 sp_dbmf 88.9% → cm_dbmf 33.3%(55.6%p 개선), sp_sbmf 3.43× slowdown → cm_sbmf 56.6%로 각각 크게 줄어, SecPB가 기존 BMT 높이 축소 기법과 상호보완적임을 보인다.

> [!quote]- 📄 원문 표현 (paper)
> - "eADR requires a capacity of 149.32mm³ for SuperCap and 1.490mm³ for Li-Thin, ∼2500× larger than the energy source required for BBB." (p.687)
> - "The BCM model is the first model in the design spectrum that removes the BMT root update from the critical write path. We observed a massive performance improvement, with average execution time reduced by 56.5%." (p.686)
> - "cm_sbmf even outperformed the sp_dbmf scheme by 32.3%. This demonstrates the effectiveness of the optimizations proposed, even with a full height BMT." (p.688)

## 섹션 노트
- **I. Introduction** — persistent hierarchy가 만드는 recoverability gap을 제시하고, PoP/SPoP를 맞추는 SecPB와 6가지 design point가 이 논문의 기여임을 밝힌다.
- **II. Background** — threat model(물리적 접근 가능한 공격자, on-chip은 TCB), memory encryption(counter-mode+split counter)·integrity verification(BMT/MAC), 그리고 persistency model(strict persistency에 집중하는 이유: persistent hierarchy에서 SP가 게임체인저가 됨)을 정리한다.
- **III. SecPB Design Space** — 두 crash recovery invariant(memory tuple의 원자적 갱신, persist order invariant)를 설명하고, early/late/hybrid 전략과 draining gap/sec-sync gap 개념, drain-process vs drain-all policy를 논의한다.
- **IV. SecPB Architecture Design** — metadata 의존성 그래프 기반 6개 design point(NoGap/M/CM/BCM/OBCM/COBCM)와 counter-once-per-dirty-block 최적화, SecPB 하드웨어 구조·FSM·coherence/inclusion 처리를 상세화한다.
- **V. Evaluation Methodology** — Gem5 설정(Table I), 평가 대상 스킴(Table II: bbb/SP/COBCM/OBCM/BCM/CM/M/NoGap), 배터리 용량 추정 방법론(보수적 가정 1~6, SuperCap vs Li-Thin 에너지 밀도)을 다룬다.
- **VI. Evaluation & Results** — 성능 요약(Table IV), benchmark별 execution time(Fig.6), draining cost 비교(Table V), SecPB 크기 영향(Fig.7/8, Table VI), BMT height study(Fig.9)로 구성된다.
- **VII. Conclusion** — SecPB가 PoP/SPoP gap을 닫으면서 strict persistency 오버헤드를 최대 1.3%까지 낮췄음을 요약한다.

## 핵심 용어 (Key terms)
- **PoV / PoP / SPoP**: Point of Visibility(다른 코어에 보이는 시점) / Point of Persistency(persistent해지는 시점) / Security PoP(security metadata가 persistent해지는 시점).
- **Recoverability gap**: persistent hierarchy에서 PoP가 코어 쪽으로 당겨지면서 SPoP와 벌어지는 간극, crash 시 plaintext 복원/integrity verification 실패를 유발.
- **BBB (Battery-Backed Buffer)**: L1D와 같은 레벨에 battery-backed persist buffer(PB)를 두어 persistent hierarchy를 구현하는 선행 기법(Alshboul et al.).
- **SecPB (secure persistent buffer)**: 이 논문이 제안하는, PB에 security metadata 필드(OTP/ciphertext/counter/BMT-ack/MAC)를 추가한 battery-backed 구조.
- **Memory tuple**: (ciphertext, counter, MAC, BMT root)로 구성되며 crash recovery 정확성을 위해 원자적으로 갱신·persist되어야 하는 단위(PLP 논문 정의).
- **BMT (Bonsai Merkle Tree)**: counter 무결성을 검증하는 Merkle tree 구조; root update가 leaf-to-root로 수백 cycle 걸리는 성능 병목.
- **OTP (one-time pad)**: counter-mode encryption에서 nonce+counter를 암호화해 만든 값으로, plaintext와 XOR해 ciphertext를 생성.
- **Data-value dependent vs independent metadata**: ciphertext/MAC(값 의존) vs counter/OTP/BMT(값 비의존) — 후자는 block당 1회 갱신으로 coalescing 가능.
- **NoGap/M/CM/BCM/OBCM/COBCM**: eager→lazy 순으로 어떤 metadata 갱신을 crash 이후로 미루는지에 따른 6개 design point.
- **PPTI / NWPE**: persists per thousand instructions / writes per SecPB entry — SecPB 성능 오버헤드를 설명하는 workload 특성 지표.
- **Drain-process vs drain-all policy**: crash 시 자신 프로세스의 PB 엔트리만 drain할지(ASID 태깅 필요), 전체를 drain할지 선택.

## 강점 · 한계 · 열린 질문
- **강점**: PoP/SPoP recoverability gap이라는 문제를 formal하게 정의하고, metadata 의존성 그래프로 파라미터화된 6개 design point를 체계적으로 탐색해 성능(118.4%→1.3%)과 배터리(753× 감소, secure eADR 대비)를 동시에 크게 개선했다. 기존 BMT 높이 축소 기법(DBMF/SBMF)과 결합 가능함을 보여 조합성(composability)도 확인했다.
- **한계**: 평가가 1-core 시뮬레이션(Table I)에 국한되어 있고, multi-core coherence/migration 메커니즘(Section IV-C)은 설계상 논의만 되고 실측 평가되지 않는다. 배터리 용량 추정은 명시된 보수적 가정(assumption 1~6, p.684)에 기반한 상한 추정치이며, side-channel(예: drain-all policy가 여는 채널)은 "beyond the scope of this work"로 명시적으로 제외했다(p.680).
- **열린 질문**: 논문은 strict persistency(SP)에 집중하는데, relaxed/epoch persistency 모델에서 store가 program order 밖에서 SecPB를 나갈 때 security metadata 갱신 순서를 어떻게 program order대로 유지할지(또는 COBCM 같은 lazy 접근으로 우회할지)의 실제 성능 영향은 다루지 않는다(p.683). 실제 multi-core 워크로드에서 migration 오버헤드가 얼마나 큰지도 향후 검증이 필요하다.

## ❓ Q&A (자가 점검)
> [!question]- PoP와 SPoP는 무엇이 다르고, recoverability gap은 왜 생기는가?
> PoP는 데이터가 persistent해지는 시점, SPoP는 그 데이터의 counter/MAC/BMT root 같은 security metadata가 persistent해지는 시점이다. BBB 같은 persistent hierarchy는 PoP를 L1D 옆 persist buffer로 당기지만 SPoP는 여전히 memory controller에 남아, 데이터는 persist됐는데 metadata는 아직인 gap이 생긴다(Fig.1, p.677-678).

> [!question]- 이 논문의 핵심 관찰(key observation)은 무엇인가?
> Crash recovery observer는 crash "시점"이 아니라 recovery "시점"의 일관된 상태만 보면 되므로, crash와 recovery 사이의 시간 동안 battery로 metadata를 lazily 맞춰도 된다는 점이다(p.678). 이것이 early~late 스펙트럼의 근거다.

> [!question]- NoGap과 COBCM의 평균 성능 오버헤드 차이는?
> 32-entry SecPB 기준 NoGap(모든 metadata 즉시 갱신)은 BBB 대비 평균 118.4% slowdown, COBCM(모든 갱신을 crash 후로 지연)은 평균 1.3% slowdown이다(Table IV, p.685).

> [!question]- data-value-independent metadata coalescing 최적화란 무엇인가?
> counter/OTP/BMT처럼 plaintext 값에 의존하지 않는 metadata는 같은 block에 여러 store가 있어도 그 block이 처음 dirty가 될 때 한 번만 갱신하면 되는데, crash observer가 SecPB drain 이후 상태만 보기 때문에 가능한 최적화다(p.682).

> [!question]- COBCM SecPB와 secure eADR의 배터리 용량 차이는 얼마인가?
> SuperCap 기준 COBCM은 core 면적의 53.6%(4.89 mm³)만 필요한 반면 s_eADR(secure eADR)은 4459.6%(3706 mm³)가 필요해, 753배 차이가 난다(Table V, p.686-687).

> [!question]- BMT height 축소 기법(DBMF/SBMF)과 SecPB(CM 모델)를 결합하면 어떤 효과가 있는가?
> DBMF는 sp_dbmf 88.9% → cm_dbmf 33.3%로, SBMF는 sp_sbmf 3.43× slowdown → cm_sbmf 56.6%로 오버헤드가 크게 줄어, cm_sbmf가 sp_dbmf보다도 32.3% 더 낫다(Fig.9, p.687-688).

> [!question]- 멀티코어 환경에서 SecPB 간 metadata coherence는 어떻게 처리되는가?
> metadata cache를 directory로 태깅해 어느 코어의 SecPB에 특정 metadata가 있는지 추적하고, 다른 코어에서 miss가 나면 해당 SecPB로 엔트리를 migration시켜 directory를 갱신함으로써 replication과 별도 coherence state 없이 처리한다(Section IV-C, p.683).

> [!question]- 이 논문이 명시적으로 scope 밖이라고 선언한 것은 무엇인가?
> drain-all policy로 인해 열릴 수 있는 side channel 공격 대응은 "beyond the scope of this work"로 명시했고, application crash를 core에서 non-secure 앱과 같이 실행하는 시나리오도 제외 대상으로 언급된다(p.680).

## 🔗 Connections
[[Reliability]] · [[HPCA]] · [[2023]]
관련: [[Root Crash Consistency of SGX-style Integrity Trees in Secure Non-Volatile Memory Systems]] · [[Thoth - Bridging the Gap Between Persistently Secure Memories and Memory Interfaces of Emerging NVMs]] · [[Silo - Speculative Hardware Logging for Atomic Durability in Persistent Memory]]

## References worth following
- M. Alshboul, P. Ramrakhyani, W. Wang, J. Tuck, Y. Solihin, "BBB: Simplifying Persistent Programming using Battery-Backed Buffers," HPCA-27, 2021 — 이 논문이 확장하는 base battery-backed persist buffer 기법.
- A. Freij, S. Yuan, H. Zhou, Y. Solihin, "Persist Level Parallelism: Streamlining Integrity Tree Updates for Secure Persistent Non-Volatile Memory," MICRO 2020 (PLP) — SPoP/PoP 정합의 crash recovery invariant를 처음 정식화한 선행 연구.
- A. Freij, H. Zhou, Y. Solihin, "Bonsai merkle forests: Efficiently achieving crash consistency in secure persistent memory," MICRO-54, 2021 — 이 논문 §VI-E에서 결합 평가하는 DBMF/SBMF BMT 높이 축소 기법.
- X. Han, J. Tuck, A. Awad, "Dolos: Improving the performance of persistent applications in adr-supported secure memory," MICRO-54, 2021 — WPQ에서 metadata를 다루는 대안적 오버헤드 절감 접근.
- G. Suh, C. O'Donnell, S. Devadas, "AEGIS: A Single-Chip Secure Processor," IEEE Design & Test of Computers, 2007 — 이 논문이 채택한 split counter-mode encryption + BMT/MAC integrity verification의 기반 프리미티브.

## Personal annotations
<!-- 본인 메모 영역 -->
