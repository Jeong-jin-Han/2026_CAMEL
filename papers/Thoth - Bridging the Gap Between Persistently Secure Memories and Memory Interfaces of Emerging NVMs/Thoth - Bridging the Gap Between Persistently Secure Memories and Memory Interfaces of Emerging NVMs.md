---
title: "Thoth: Bridging the Gap Between Persistently Secure Memories and Memory Interfaces of Emerging NVMs"
description: "미래 메모리 인터페이스(DDR-T/CXL/DDR5 on-die ECC)에서 ECC bit 재사용이 불가능해진 상황에, off-chip persistent partial-updates buffer로 MAC/counter 부분 갱신을 합쳐 크래시 일관성을 유지하며 쓰기 증폭을 줄이는 secure NVM 아키텍처"
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
  - topic/secure-memory
  - topic/nvm
  - topic/crash-consistency
  - topic/persistent-memory
---

# Thoth: Bridging the Gap Between Persistently Secure Memories and Memory Interfaces of Emerging NVMs
> **HPCA 2023** · cluster/reliability · Source: [Thoth - Bridging the Gap Between Persistently Secure Memories and Memory Interfaces of Emerging NVMs.pdf](<Thoth - Bridging the Gap Between Persistently Secure Memories and Memory Interfaces of Emerging NVMs.pdf>)

저자: Xijing Han (North Carolina State University), James Tuck (North Carolina State University), Amro Awad (North Carolina State University)

## TL;DR
State-of-the-art secure NVM 기법들은 MAC/encryption counter 같은 보안 메타데이터를 데이터와 같은 메모리 블록에 co-locate하기 위해 ECC bit를 반복적으로 재사용(overriding)하는 전제에 의존하는데, DDR-T·CXL·DDR5 같은 미래 메모리 인터페이스는 ECC 계산·관리를 DIMM/모듈 내부로 옮겨버려 호스트가 이 ECC bit에 접근할 수 없게 만든다. 이 경우 MAC과 counter를 각각 별도의 블록 쓰기로 원자적으로 persist해야 해서 메모리 쓰기당 최대 2번의 추가 쓰기가 발생한다. Thoth는 이를 해결하기 위해 memory 내부에 대용량 off-chip persistent partial-updates buffer(PUB)를 두고, MAC/counter의 부분 갱신(partial update)들을 오래 버퍼링하다가 write-back cache의 자연스러운 eviction·staleness로 인해 실제로는 추가 쓰기가 필요 없어지는 경우(최대 99.5%)를 활용해 write amplification을 크게 줄인다. Gem5 기반 평가에서 미래 인터페이스에 맞춰 조정한 baseline Anubis 대비 평균 1.22배(최대 1.44배) 성능 향상, 평균 32%(최대 40%) 쓰기 트래픽 감소를 달성했다.

## 문제 & 동기
오늘날 secure NVM 구현들(Anubis, Osiris, Dolos 등)은 MAC 또는 (일부) encryption counter를 데이터와 같은 메모리 블록의 ECC bit 영역에 override하여 co-locate시킴으로써, 데이터 쓰기 한 번에 보안 메타데이터도 원자적으로 같이 persist되게 만든다. 그러나 Intel DCPMM의 DDR-T 인터페이스, SK Hynix 3DXVXP 등 emerging NVM, 그리고 CXL 메모리 시맨틱 프로토콜은 ECC 계산을 메모리 모듈/DIMM 내부(on-die ECC)로 옮겨, 호스트가 남는 ECC bit를 확보할 수 없게 만든다(p.94-95). CXL은 폭이 66B인데 그중 2B만 전송 ECC용이고 나머지 64B는 순수 payload라 추가 bit가 없다(p.95). 이 경우 MAC(블록당 약 12.5% 오버헤드)과 counter(약 1.56%, 훨씬 작음)는 저장 효율을 위해 서로 다른 블록에 분리 저장되므로, 메모리 블록 쓰기 한 번마다 MAC 블록과 counter 블록에 대해 각각 별도의 원자적 쓰기가 필요해져 심각한 write amplification이 발생한다(p.95).

> [!quote]- 📄 원문 표현 (paper)
> - "if future memory interfaces do not have extra bits that are suitable for co-locating secure metadata with data, then there are no effective solutions for persistently secure NVMs. The only available solution is to incur separate security metadata writes with each persistent memory write in an atomic fashion." (p.95)
> - "Such implementations may lose their effectiveness. Specifically, they will require two additional writes for the MAC and counter blocks." (p.95)
> - "encryption counters and MACs are not co-located in the same memory block; encryption counters have much less storage overhead compared to MACs (typically 12.5% for MACs vs. 1.56% for counters), and hence they are separated in different blocks, which causes two extra separate block writes to memory for each memory block write." (p.95)

## 핵심 통찰 (Key Insight)
- **부분 갱신을 오래 버퍼링하면 대부분 "공짜"로 사라진다**: MAC/counter의 partial update가 PUB에서 evict될 때, 그 메타데이터 블록이 이미 secure metadata cache에서 자연스럽게 evict/persist되었거나(already-evicted), cache에 clean 상태로 남아있거나(clean copy), 더 최신 partial update로 대체되어 stale해졌다면(stale copy) — 이 세 경우 모두 추가 쓰기 없이 안전하게 버릴 수 있다. 시간이 지날수록 이 확률이 커지는데, 그 이유는 (1) 메타데이터 블록이 이미 자연 eviction으로 persist됐거나, (2) 다른 더 오래된 partial update가 이미 같은 블록을 persist시켰거나(공간 지역성), (3) 같은 메타데이터 위치에 더 최신 갱신이 도착해 이전 것이 stale해졌기 때문(시간 지역성)이다(p.97-98). 이는 secure metadata cache의 write-back 특성과 애플리케이션의 partial update에 대한 공간/시간 지역성을 그대로 활용하는 것이어서 별도 하드웨어 예측 없이도 효과적이다.
- **PUB는 off-chip에 둬도 된다**: on-chip에 별도 캐시/버퍼를 늘리는 대신, PUB를 memory(NVM) 안에 크게(예: 64MB) 두어도 저장 오버헤드가 1% 미만이라 무방하다. 이는 partial update들이 pack되어 블록 단위로만 memory에 쓰이기 때문에 가능하다(p.98-99).

> [!quote]- 📄 원문 표현 (paper)
> - "the vast majority of partial security metadata updates when evicted from the PUB need no additional writes, if they are persistently buffered for long enough. This is because, after a long enough time passes, the probability is high that memory already contains their update." (p.98)
> - "with a large enough partial buffer size, the majority of partial updates (99.5% on average for the 500,000 buffer) do not cause a full block persist upon eviction." (p.98)
> - "the logical place to place the PUB is off-chip. However, this brings us to our first design question, how do we arrange partial updates in memory?" (p.98-99)

## 설계 / 메커니즘 (Design)
- **위협 모델**: 버스 스누핑, 물리적 절도, 메모리 스캐닝, 리플레이, 데이터 변조까지 기존 secure NVM 연구(Anubis 등)와 동일; side-channel/timing/접근 패턴 유출은 범위 밖(p.99). Counter-mode encryption + split-counter + Bonsai Merkle Tree(BMT)를 baseline 보안 프리미티브로 채택(p.96-97).
- **PUB (Persistent Updates/Partial-updates Buffer, Fig. 2, p.98 / Fig.4,7 p.99,102)**: FIFO circular buffer로 memory에 위치, start/end/base 3개 포인터로 관리. 각 entry는 `{address, MAC, counter, status}`이며, address는 최대 512GB 모듈을 다룰 수 있는 32b, counter는 7b minor counter, MAC은 ciphertext에 대해 계산되는 두 단계 구조 — 128B 블록엔 16B MAC(1st-level), 이를 다시 8B로 압축한 2nd-level MAC을 PUB에 저장해 더 많은 partial update를 한 PUB 블록에 packing한다. 128B 캐시블록 기준 PUB 블록 하나에 9개, 256B 기준 19개의 partial update가 들어간다(p.99).
- **WPQ 연동 — Persistent Combining Buffer (PCB, Fig.4/7, p.99,102)**: 프로세서의 ADR-backed write-pending queue(WPQ, 보통 64 entries) 중 일부 entries를 PCB로 예약해 여러 partial update를 하나의 WPQ entry로 합친다. PCB-before-WPQ(주소 기반으로 PCB 안에서 먼저 병합 후 WPQ에 배치) vs PCB-after-WPQ(WPQ 전체에 bitmask를 둬 병합)를 비교했고, "augmented PCB-before-WPQ"(PCB 8 entries + 일반 WPQ 56 entries, 총 64)를 채택 — PCB-after-WPQ와 유사한 성능을 내면서 더 단순하다(p.101-102).
- **Eviction 정책 — WTBC vs WTSC (Fig.5/6, p.100-101)**: PUB entry가 evict될 때 그 partial update가 이미 다른 경로로 persist됐는지 판별해야 한다. **WTBC(Write-Back Through Bitmask Checks)**는 메타데이터 캐시 블록 내 개별 MAC/counter 단위의 fine-grain dirty bit를 추적해 정확히 판별하지만 캐시에 추가 저장 오버헤드가 든다. **WTSC(Write-Back Through Status Checks)**는 블록 단위의 dirty bit(2b status bit)만으로 근사 판별하는 더 보수적인 방식으로, 정확성은 떨어지지만(불필요한 persist가 약간 더 발생) 추가 면적이 필요 없다. 실험적으로 WTSC가 WTBC와 거의 동등한 효과를 내어 본문 전체의 기본 정책으로 채택되었다(p.101).
- **Recovery**: 크래시 시 PUB이 원본 위치의 메타데이터보다 더 최신 값을 갖고 있으므로, PUB을 역순(oldest→youngest)으로 스캔하며 해당 메타데이터 블록에 병합 후 write, ciphertext로부터 2-level MAC을 재계산해 검증한다. Integrity tree는 Anubis 방식대로 재구성한다. Anubis의 sub-second 복구시간에 더해 64MB PUB 기준 약 7초의 추가 복구시간이 발생한다(p.102, 각주 5).

> [!quote]- 📄 원문 표현 (paper)
> - "We aim to (1) realize a persistent partial updates buffer (PUB) in memory (2) upon eviction of a partial update entry from the PUB, discard the write-back of the corresponding metadata block when no longer necessary" (p.97)
> - "we found that an augmented version of PCB-before-WPQ, where we check the addresses of partial updates in the PCB upon each partial update such that they are merged, can minimize the pressure on the WPQ and obtain similar performance as in PCB-after-WPQ." (p.101)
> - "Thoth's responsibility is to merely merge the updates in PUB with the tree (and MAC blocks) to be re-constructed before verification." (p.102)

## 평가 (Evaluation)
- **환경**: Gem5 full-system cycle-level 시뮬레이터, 4-core X86-64 OoO @4GHz, 32GB DDR 기반 PCM (read 150ns/write 500ns), WHISPER의 database 벤치마크 4종(Hashmap, Ctree, Btree, RBtree) + in-house Array Swap 벤치마크, 트랜잭션 크기 기본 128B (Table I, p.102-103).
- **Baseline**: ECC 재사용이 여전히 가능하다고 가정한 strict persistence(=미래 인터페이스에 맞춰 조정한 Anubis) — MAC/counter 블록을 매 쓰기마다 별도로 persist.
- **성능**: 128B/256B 캐시블록 각각에서 WTSC/WTBC 정책 모두 유사한 성능. 128B 캐시블록에서 평균 speedup 1.22x(WTSC)/1.16x(256B, Fig.8, p.103). 논문 전체 최고치는 abstract 기준 "average of 1.22x (up to 1.44x)"(p.94).
- **쓰기 감소**: 128B 블록에서 평균 32%, 256B 블록에서 평균 37% 쓰기 감소(Fig.9, p.103). Swap 벤치마크는 트랜잭션 크기가 작고 메모리 접근이 적어 오히려 baseline 대비 speedup을 얻지 못함(쓰기는 20%/15% 줄였지만 성능 향상 없음, p.103).
- **트랜잭션 크기 민감도(Fig.10, Table II/III, p.104)**: 128B 캐시블록에서 트랜잭션 크기 128/512/1024/2048B에 대해 평균 speedup 1.22x/1.23x/1.19x/1.19x; 256B 블록은 1.16x/1.17x/1.14x/1.19x. 쓰기 감소는 128B 블록에서 32%/28%/24%/20%, 256B 블록에서 37%/33%/31%/20%(추정 오탈자 가능, 본문상 "37%, 33%, 31%로")로 트랜잭션 크기가 커질수록 감소폭이 줄어듦 — baseline WPQ에서도 병합 기회가 늘기 때문(p.104).
- **메타데이터 캐시 크기 민감도(Fig.11, p.104)**: counter/MAC 캐시 64kB/128kB → 1MB/2MB로 키우면 128B 블록 평균 speedup이 1.22x → 1.34x로, 256B 블록은 1.16x → 1.28x로 증가(캐시가 클수록 natural eviction이 줄어 write-back도 줄기 때문).
- **WPQ 크기 민감도(Fig.12, p.105)**: WPQ를 64→32→16 entries로 줄이면(그중 1/8을 PCB로 예약) 128B 블록에서 speedup이 1.48x, 1.65x로, 256B 블록에서 1.50x, 1.81x로 오히려 커짐 — baseline의 자연 병합 기회가 줄어드는 만큼 Thoth의 상대적 이득이 커짐.
- **이상적 ECC 대비 오버헤드**: 64kB(counter)/128kB(MAC) 캐시 기준, Thoth는 "미래에도 co-located ECC(=Anubis)가 있다고 가정한 이상적 baseline" 대비 평균 7%의 오버헤드만 낸다(p.105, §VI-F).

> [!quote]- 📄 원문 표현 (paper)
> - "Thoth improves the performance by an average of 1.22x (up to 1.44x) while reducing write traffic by an average of 32% (up to 40%) compared to the state-of-the-art solution Anubis when adapted to future interfaces." (p.94)
> - "with 64kB(counter) and 128kB(MAC) metadata cache size, Thoth brings an overhead of merely 7% on average over baseline that hypothetically assumes all future chips will have co-located ECC (i.e., Anubis)." (p.105)
> - "Thoth achieves more speedup with smaller WPQ because less security metadata is coalesced in the baseline with smaller WPQ." (p.105)

## 섹션 노트
- **I. Introduction**: NVM의 밀도/저전력/data-retention 장점과 보안 요구를 소개하고, ECC 위치가 host에서 DIMM 내부로 이동하는 미래 인터페이스 트렌드가 기존 secure NVM 기법의 근본 전제를 깨는 문제를 제기한다.
- **II. Background**: counter-mode encryption(Fig.1)과 BMT 기반 integrity 보호, ADR/eADR, 그리고 DDR-T·DDR5·CXL의 on-die ECC 트렌드를 정리하며 host-accessible ECC bit가 사라지는 이유를 설명한다.
- **III. Motivation**: partial security metadata block update를 정의하고, FIFO 버퍼 크기별(500K/5K/50 entries) eviction 시나리오 분해(Fig.3)로 대형 버퍼가 write amplification을 거의 없앨 수 있음을 정량적으로 입증한다.
- **IV. Design**: 위협 모델, PUB 조직(§IV-A), eviction 정책 WTBC/WTSC(§IV-B), WPQ와의 상호작용/PCB(§IV-C), recovery 절차(§IV-D)를 순서대로 설계한다.
- **V. Evaluation**: Gem5 설정(Table I)과 전반적 성능(§V-B), 트랜잭션 크기(§V-C)·메타데이터 캐시 크기(§V-D)·WPQ 크기(§V-E) 민감도, ECC 있는 이상적 baseline과의 비교(§V-F)를 다룬다.
- **VI. Related Work**: Anubis/Osiris/Dolos/Soteria 등 secure NVM 계열과, ECC bit 재사용에 의존하는 기존 접근들의 공통 한계를 짚는다.
- **VII. Conclusion**: PUB이라는 off-chip persistent buffer로 미래 메모리 인터페이스에서도 최소 write amplification으로 crash consistency를 유지할 수 있음을 요약한다.

## 핵심 용어 (Key terms)
- **PUB (Persistent Updates Buffer)**: MAC/counter의 partial update만 모아 블록 단위로 packing해 memory에 두는 off-chip FIFO 버퍼.
- **PCB (Persistent Combining Buffer)**: 프로세서 내 WPQ 중 일부 entry를 partial update 병합 전용으로 예약한 구조.
- **WPQ (Write Pending Queue)**: ADR로 배터리 백업되는 프로세서 내 소용량 persistent write buffer.
- **Partial (security metadata block) update**: 하나의 메모리 쓰기로 인해 MAC/counter 캐시 블록 내 일부만 갱신되는 것.
- **WTBC / WTSC**: PUB eviction 시 이미 persist된 갱신인지 판별하는 두 정책 — fine-grain dirty bitmask 방식 vs. 근사적인 블록 status bit 방식.
- **Bonsai Merkle Tree (BMT)**: counter에 대해서만 integrity tree를 구성하고 데이터는 counter를 포함한 MAC으로 신선도를 보장하는 경량 integrity 스킴.
- **Split-counter scheme**: 64B 블록당 major counter를 공유하고 각 블록마다 7b minor counter를 두는 counter 저장 방식.
- **ADR / eADR (Asynchronous DRAM Refresh / enhanced ADR)**: 정전 시 volatile 버퍼(WPQ 등)만 flush하는 최소 보장(ADR) vs. 전체 캐시 계층까지 flush하는 강화 옵션(eADR, 전원 공급 제약으로 실제 서버에서 비활성화되는 경우 많음).
- **On-die ECC**: DDR-T/DDR5/CXL처럼 ECC 계산·정정을 메모리 모듈 내부에서 수행해 host가 ECC bit를 재사용할 수 없게 만드는 방식.

## 강점 · 한계 · 열린 질문
- **강점**: 새로운 암호 프리미티브 없이, 기존 secure NVM 스킴(Anubis 등)에 "그대로 얹는" 형태로 적용 가능하며(§VI, p.105), 새 버퍼가 off-chip이라 온칩 면적/전력 부담이 거의 없다. Fig.3의 정량 분석(99.5% 무쓰기)이 설계의 핵심 근거를 명확히 뒷받침한다.
- **한계**: (1) Swap처럼 접근 지역성/트랜잭션 크기가 작은 워크로드에서는 오히려 이득이 거의 없거나 미세한 성능 저하가 관측됨(p.103). (2) 64MB PUB 기준 크래시 복구 시간이 약 7초 추가되어(p.102), 복구 지연에 민감한 시스템에서는 트레이드오프가 필요하다. (3) side-channel(전력/타이밍/접근패턴) 공격은 범위 밖으로 명시적으로 배제(p.99).
- **열린 질문**: PUB 크기·WPQ 재분배 비율의 최적점을 애플리케이션별로 어떻게 online으로 조정할지, eADR이 점차 보급될 경우 PUB의 상대적 이득이 어떻게 달라질지, CXL 메모리 시맨틱 프로토콜의 실제 66B 프레임 제약 하에서 PCB/PUB의 네트워크 레벨 오버헤드는 어떻게 되는지는 본문에서 직접 다뤄지지 않는다.

## ❓ Q&A (자가 점검)
> [!question]- Thoth가 해결하려는 근본 문제는 무엇인가?
> DDR-T/CXL/DDR5 같은 미래 메모리 인터페이스가 ECC 계산을 host에서 DIMM 내부로 옮기면서, 기존 secure NVM 기법들이 의존하던 "ECC bit를 MAC/counter로 재사용해 데이터와 co-locate"하는 전제가 성립하지 않게 되어, MAC과 counter를 각각 별도로 원자적 persist해야 하는 write amplification 문제가 생긴다(p.95).

> [!question]- 왜 MAC과 counter가 같은 블록에 저장되지 않는가?
> MAC(약 12.5% 오버헤드)과 counter(약 1.56%)의 저장 오버헤드 크기 차이 때문에 저장 효율을 위해 서로 다른 블록에 분리 저장되고, 이로 인해 한 번의 메모리 쓰기마다 최대 2개의 별도 블록 쓰기가 발생한다(p.95).

> [!question]- PUB의 eviction 시 추가 쓰기가 필요 없는 3가지 경우는?
> already-evicted(메타데이터 블록이 이미 캐시에서 evict·persist됨), clean copy(캐시에 남아있지만 clean 상태), stale copy(더 최신 partial update로 대체되어 stale해짐) — 이 세 경우 모두 PUB entry를 안전하게 버릴 수 있다(p.98).

> [!question]- WTBC와 WTSC의 차이와 왜 WTSC를 최종 채택했는가?
> WTBC는 개별 MAC/counter 단위 dirty bitmask로 정확히 판별하지만 캐시 면적 오버헤드가 필요하고, WTSC는 블록 단위 status bit만으로 근사 판별해 추가 면적이 필요 없다. 실험적으로 WTSC가 WTBC와 거의 동일한 효과를 보여, 더 단순한 WTSC를 논문의 기본 정책으로 채택했다(p.101).

> [!question]- PCB-before-WPQ와 PCB-after-WPQ의 차이는?
> PCB-before-WPQ는 WPQ에 넣기 전 PCB(예약 entry)에서 먼저 주소 기반으로 병합하는 방식, PCB-after-WPQ는 WPQ 전체를 volatile bitmask로 관리해 WPQ에 들어간 후에도 병합 기회를 넓히는 방식이다. 논문은 주소 체크를 추가한 "augmented PCB-before-WPQ"(PCB 8 + WPQ 56)를 채택해 PCB-after-WPQ와 유사한 성능을 더 단순하게 얻었다(p.101-102).

> [!question]- 크래시 후 복구는 어떻게 이뤄지는가?
> PUB을 oldest→youngest 역순으로 스캔하며 대응하는 메타데이터 블록(counter, MAC)에 병합·기록하고, ciphertext로부터 2-level MAC을 재계산해 검증한 뒤 Anubis 방식으로 integrity tree를 재구성한다. 이 과정에서 Anubis의 sub-second 복구시간 외에 64MB PUB 기준 약 7초가 추가된다(p.102).

> [!question]- Swap 벤치마크에서 speedup이 거의 없는 이유는?
> Swap은 두 배열을 단순 교환하는 연산이라 접근하는 메모리 위치가 적고 트랜잭션 크기(128B)도 상대적으로 작아, 애초에 보안 메타데이터 쓰기 자체가 많지 않다. 쓰기는 20%(128B)/15%(256B) 줄였지만 이것이 유의미한 speedup으로 이어지지 않았다(p.103).

> [!question]- Thoth가 이상적(미래에도 ECC bit가 있다고 가정한) baseline 대비 갖는 오버헤드는 얼마인가?
> 64kB(counter)/128kB(MAC) 캐시 크기 기준 평균 7%의 오버헤드만 발생한다(p.105).

## 🔗 Connections
[[Reliability]] · [[HPCA]] · [[2023]]
관련: [[Root Crash Consistency of SGX-style Integrity Trees in Secure Non-Volatile Memory Systems]] · [[SecPB - Architectures for Secure Non-Volatile Memory with Battery-Backed Persist Buffers]]

## References worth following
- K. A. Zubair and A. Awad, "Anubis: Ultra-low overhead and recovery time for secure non-volatile memories," ISCA 2019 — Thoth가 직접 비교하는 baseline/adaptation 대상이자 recovery 메커니즘의 기반.
- M. Ye, C. Hughes, A. Awad, "Osiris: A low-cost mechanism to enable restoration of secure non-volatile memories," MICRO 2018 — ECC bit에 counter를 임베딩하는 대안적 co-location 기법.
- B. Rogers et al., "Using address independent seed encryption and bonsai merkle trees to make secure processors os- and performance-friendly," MICRO 2007 — 본 논문이 채택한 BMT의 원 출처.
- S. Van Doren, "Compute express link," HOTI 2019 — CXL 프로토콜(66B width, 2B ECC)의 근거가 되는 참고문헌.
- X. Han, J. Tuck, A. Awad, "Dolos: Improving the performance of persistent applications in adr-supported secure memory," MICRO 2021 — 동일 저자 그룹의 선행 연구로 persistent transaction latency 최적화를 다룸.

## Personal annotations
<!-- 본인 메모 영역 -->
