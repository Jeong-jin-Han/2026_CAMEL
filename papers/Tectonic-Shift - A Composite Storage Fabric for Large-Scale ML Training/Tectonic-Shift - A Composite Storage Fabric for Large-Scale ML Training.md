---
title: "Tectonic-Shift: A Composite Storage Fabric for Large-Scale ML Training"
description: "Meta의 ML 학습 인프라를 위한 HDD+flash 복합 스토리지 패브릭. flash 캐시(Shift)에 application-aware admission/reinsertion 정책을 적용해 IO 흡수율을 높이고 전력을 절감"
venue: ATC
year: 2023
tier: deep
status: done
presenter:
present-date:
tags:
  - paper
  - cluster/llm
  - venue/atc
  - year/2023
  - list/26s-v2
  - topic/composite-storage
  - topic/flash-cache
  - topic/ml-training-storage
  - topic/cache-admission-policy
---

# Tectonic-Shift: A Composite Storage Fabric for Large-Scale ML Training
> **ATC 2023** · cluster/llm · Source: [Tectonic-Shift - A Composite Storage Fabric for Large-Scale ML Training.pdf](<Tectonic-Shift - A Composite Storage Fabric for Large-Scale ML Training.pdf>)

저자: Mark Zhao (Stanford University and Meta), Satadru Pan (Meta), Niket Agarwal (Meta), Zhaoduo Wen (Meta), David Xu (Meta), Anand Natarajan (Meta), Pavan Kumar (Meta), Shiva Shankar P (Meta), Ritesh Tijoriwala (Meta), Karan Asher (Meta), Hao Wu (Meta), Aarti Basant (Meta), Daniel Ford (Meta), Delia David (Meta), Nezih Yigitbasi (Meta), Pratap Singh (Meta), Carole-Jean Wu (Meta), Christos Kozyrakis (Stanford University)

## TL;DR
Meta의 DLRM 학습 데이터를 서빙하는 기존 HDD 기반 분산 파일시스템 Tectonic은 IO 대역폭 수요가 저장 용량 수요보다 훨씬 빠르게 늘어나면서 전력 비효율에 부딪혔다. Tectonic-Shift는 각 Tectonic HDD 클러스터 앞단에 flash 캐시 계층 Shift를 투명하게 배치하는 composite storage fabric으로, HDD(저장 효율)와 flash(IO 효율)를 분리해 전력을 절감한다. 핵심은 eviction 정책이 아니라 admission/reinsertion 정책에 지능을 넣은 것으로, 학습 job이 스캔·처브(scan/churn) 패턴을 보이는 특성을 활용해 Request History Window(historic)와 Global Metadata Store의 job dataset 명세(future)로부터 캐시에 넣을 가치가 있는 데이터를 예측한다. 이를 통해 Shift는 LRU 기반 flash 캐시보다 1.51-3.28배 더 많은 IO를 흡수하고, 페타바이트 규모 프로덕션 클러스터에서 전력을 29% 절감했다.

## 문제 & 동기
Meta는 각 데이터센터에 수천 개 GPU로 구성된 학습 클러스터를 여러 곳 운영하며, 각 클러스터는 엑사바이트급 데이터를 저장하고 초당 수십 테라바이트 규모로 read를 서빙해야 한다(p.1/433). 기존 저장 패브릭 Tectonic은 disaggregated HDD 스토리지 노드 클러스터로 뒷받침되는데, HDD는 IO-per-watt 성능이 나쁘다. 그런데 학습 가속기(트레이너)들의 IO 요구량은 현대 HDD가 제공하는 것에 비해 크게, 그리고 계속 늘어나는 불균형을 만들어냈다(p.1/433).

Table 1(p.435)의 정량 분석: 100PB 저장·10TB/s IO 수요를 동시에 만족시키려면, HDD-only는 storage-only 대비 9.92배 더 많은 전력이 필요하고(이는 RS(9,6) 인코딩 가정 시 필요량보다 1.6EB 더 많은 디스크를 요구), Flash-only는 storage 수요 충족에 6.53배 더 많은 전력이 필요하다. 반면 이상적인 HDD+flash composite 구성은 IO 충족에 flash 1.69배, storage 충족에 HDD 1.00배 전력만 필요해 IO 계층의 전력 풋프린트를 Option1(HDD-only) 대비 3.69배 줄일 수 있다(p.436).

> [!quote]- 📄 원문 표현 (paper)
> - "modern ML storage fabrics often require more power than trainers themselves" (p.433)
> - "This option would require us to provision 9.92× more storage capacity than necessary — 1.6EB of disks assuming RS(9,6)!" (p.435)
> - "Scans and churns are well-known antagonist cache patterns [52], motivating the need for specialized and domain-specific admission and eviction policies." (p.437)

## 핵심 통찰 (Key Insight)
1. **Composite storage로 저장-효율 디바이스와 IO-효율 디바이스를 분리한다.** 단일 저장 매체(HDD든 flash든)에만 의존하면 storage capacity와 IO capacity 중 하나를 반드시 과잉 프로비저닝해야 한다. HDD(storage-efficient)와 flash(IO-efficient)를 함께 쓰면 각각 필요한 만큼만 프로비저닝해 전력을 최소화할 수 있다(Table1, p.435-436).
   > [!quote]- 📄 원문 표현 (paper)
   > - "Relying on a single storage hardware inherently precludes us from balancing storage and IO capacity. An ideal cluster would use both a storage-efficient device and IO-efficient device together" (p.435)

2. **Eviction이 아니라 admission/reinsertion에 지능을 넣는다.** ML 학습 workload는 웹 워크로드와 달리 대규모 scan과 churn 패턴을 보여 LRU 기반 eviction 정책이 쉽게 thrash된다. Shift는 CacheLib의 LRU/FIFO eviction은 그대로 두고, 무엇을 캐시에 admit/reinsert할지를 제어하는 데 집중해 thrashing을 원천 차단한다(Section5, p.437, 439).
   > [!quote]- 📄 원문 표현 (paper)
   > - "We prioritized admission and reinsertion policies as opposed to eviction policies such as LRU as a) we can prevent significant thrashing due to the scan and churn ... and b) we can control write rates to flash" (p.439)

3. **Application-aware 캐시: job의 dataset 명세로 미래 접근 패턴을 추론한다.** Tectonic-Shift는 트레이닝 job이 읽을 table partition·feature 목록을 애초에 알고 있다는 사실을 활용해, historic 정보(Request History Window, RHW)와 future 정보(Global Metadata Store, GMS의 활성 job dataset 명세)를 결합한 bucket priority로 admission/reinsertion 문턱값을 계산한다(Section5.1, p.439-440).
   > [!quote]- 📄 원문 표현 (paper)
   > - "not only is Tectonic-Shift transparent to users, it understands application information from training job specifications ... We present novel cache mechanisms that leverage this information to improve the performance of Shift by inferring training jobs' future data access patterns." (p.437)

## 설계 / 메커니즘 (Design)
- **아키텍처 개요(Figure6, p.438).** Shift는 각 Tectonic HDD 클러스터 앞에 놓인 flash 캐시 클러스터로, Client Library가 read(pread)를 block read로 분해한 뒤 cache eligibility를 판정한다. Ineligible 블록은 Tectonic Chunk Store에서 직접(1a) 읽고, eligible 블록은 consistent hash ring 위의 Shift Storage Node(SN)에 get(blockId, offset, length) 요청(1b)을 보낸다. 캐시 히트면 즉시 반환(2a), 미스면 SN이 admission 여부를 판단(3)하고 admit되면 Tectonic에서 fetch(4) 후 삽입, eviction된 세그먼트는 선택적으로 reinsert(5,6)된다.
- **SN 데이터 플레인.** 각 SN은 CacheLib을 내부 캐싱 엔진으로 사용해 DRAM+flash(SSD)를 관리한다. Tectonic block(보통 72MiB)은 고정 크기 segment(256KB, Section5.2에서 튜닝 근거 설명, p.440)로 쪼개져 캐시 객체 단위가 된다.
- **SN 컨트롤 플레인(Figure7, Table2, p.439).** MapSegment(s)로 세그먼트를 bucket(기본은 디렉토리 단위)에 매핑하고, 각 bucket에 admission/reinsertion 이진 정책을 할당한다. BucketPriority(b)는 두 성분의 조합:
  - Historic Priority: $BucketPriority(b) = TotalBytes(b) / UniqueBytes(b)$ — RHW가 RHWSize(기본 6시간) 윈도우 동안 관측한 반복 접근 정도(p.440).
  - Future Priority: GMS에 등록된 활성 job들의 dataset 명세 중 해당 bucket을 포함하는 job 수(p.440).
  - Hybrid 정책은 $BucketPriority_{Hybrid}(b) = \max(BucketPriority_{Historic}(b), BucketPriority_{Future}(b))$ 로 둘을 결합(p.441).
  - AdmitThreshold/ReinsertThreshold는 BucketRefreshTime(기본 10초)마다 갱신되고, PID 제어 feedback loop로 flash write rate 한도(endurance 제약) 이내로 유지되도록 동적 튜닝된다(p.440).
- **Client cache eligibility(Section5.3, p.440).** Client Library 단에서 non-ML 트래픽과 IO 수요가 HDD만으로 충분히 감당 가능한 저인기 table을 필터링해 Shift에 대한 RPC/메모리 압박을 줄인다.
- **투명성·단순성·확장성·지능(4가지 설계 원칙, Section4.1, p.437-438):** Transparent(기존 Tectonic API·pread 시맨틱 그대로 유지, 유저 개입 불필요), Simple(CacheLib 재사용, put API 없이 Tectonic Metadata Layer에만 의존), Scalable(SN은 완전히 decentralized, consistent hash ring), Intelligent(job dataset 명세를 활용).

> [!quote]- 📄 원문 표현 (paper)
> - "Shift SNs act only as a part of the data plane... Shift does not expose a put API. Inserts into cache are only made for missed get requests." (p.439)
> - "Shift is fully decentralized, consisting only of disaggregated flash storage nodes, each using local dynamic cache policies that adjust based on observed load." (p.437)

## 평가 (Evaluation)
- **정책 벤치마크(Table3 workload 정의, Figure8, p.441-442).** 6-node Shift 클러스터, node당 DRAM 16GB, Synchronized/Pipelined/Sequential 세 가지 대표 workload에서 FIFO/Historic/Future/Hybrid를 LRU 대비 평가. 전체 평균: FIFO 1.31×, Historic 1.51×, Future 3.28×, Hybrid 1.67× 더 많은 IO 흡수(p.442). Synchronized에서는 Future/Hybrid가 각각 2.27×/2.32×(Historic 2.01×), Pipelined에서는 Future가 5.84×(FIFO 1.86×, Historic은 오히려 0.80×로 LRU보다 나쁨), Sequential에서는 Historic/Future/Hybrid가 각각 1.71×/1.74×/1.69×(FIFO 1.06×) 기록(p.441-442).
- **Flash endurance 제약 하 평가(Figure9, p.442).** SN당 flash write를 100MB/s로 제한한 Synchronized workload에서, CacheLib 제공 Dynamic 정책 대비 Reject First 1.51×, Admit All 2.66×, Shift의 Historic/Future/Hybrid는 각각 2.14×/3.07×/2.99× 더 많은 IO 흡수. Admit All은 write limit을 넘겨 10.38× 더 많은 flash write를 요구했지만, Shift의 정책들은 CacheLib 대비 write rate가 5% 이내로 일치했다. Shift 정책들은 reject되는 객체에 대해 Tectonic HDD read를 아예 회피해, Dynamic 대비 cache fill(=HDD read)이 96% 더 적었다(Fig9c, p.442).
- **Reinsertion 효과(Table4, p.442).** Hybrid+dynamic threshold tuning에서 reinsertion을 켜면, 끈 baseline 대비 hit rate가 1.03배(3% 증가), HDD IO는 0.82배(18% 감소)로 개선. 단, write 한도가 낮을 때(≈300MB/s) reinsertion이 write limit을 계속 초과시키는 부작용도 관찰(p.442).
- **프로덕션 결과(Figure10, p.443).** 페타바이트 규모 프로덕션 클러스터에서 9시간 트레이스 비교: 고-IOPS table만 admit하는 Expert 수동 튜닝 정책은 전력 중립점(power-neutral IO absorption)에 필요한 IO의 0.21×밖에 흡수하지 못한 반면, Shift(Hybrid, min admit threshold 3.0, reinsertion 없음)는 이를 달성. 전체적으로 Tectonic-Shift는 HDD-only 대비 전력을 29% 절감했다(Abstract, p.433; Section6.2, p.443).

> [!quote]- 📄 원문 표현 (paper)
> - "Shift absorbs 1.51 − 3.28× more IO than an LRU-based flash cache on a mix of representative training workloads" (p.434)
> - "Simply deploying a flash cache without intelligent and adaptable cache policies is inefficient; our production trace shows that doing so would only absorb 0.21× the IO needed to achieve power neutrality." (p.443)
> - "By employing the application-aware policies presented in Section 5, we show that Shift can exploit the unique characteristics of training jobs, saving 29% of power relative to using only HDDs for training data storage." (p.443)

## 섹션 노트
- **§1 Introduction**: 문제 정의(HDD IO-per-watt 한계, storage-IO 불균형)와 Tectonic-Shift/Shift의 4가지 원칙(transparent, simple, scalable, intelligent) 및 기여 요약.
- **§2 ML Data Storage and Ingestion Background**: DLRM 학습 데이터 파이프라인(Scribe → ETL → Hive table → Tectonic) 구조와 Tectonic File System(디렉토리/파일/블록/청크, RS 인코딩) 구조 설명.
- **§3 Production ML Storage Design Space**: 하드웨어(HDD-only/Flash-only/Composite) 및 소프트웨어(범용 캐시/ML-specific 캐시/투명·application-aware 캐시) 설계 공간 탐색과 production workload 특성(row-wise/column-wise reuse, temporal scan+churn) 분석.
- **§4 Tectonic-Shift Architecture**: 설계 원칙과 read 경로(Client Library ↔ Shift SN ↔ Tectonic) 상세 설명.
- **§5 Application-Aware Cache Policies**: Admission/Reinsertion 정책, RHW/GMS 기반 historic/future priority, threshold tuning, client cache eligibility.
- **§6 Deployment and Evaluation**: 벤치마크 정책 비교, flash endurance 제약 실험, reinsertion 효과, 프로덕션 결과.
- **§7 Lessons Learned and Open Questions**: 투명 인터페이스의 가치, 강건한 테스트/모니터링 도구, DRAM 활용법, data placement/job routing과의 co-design 가능성, priority-aware eviction(CacheLib 확장) 필요성 등 미해결 질문.
- **§8 Related Work**: 소프트웨어 flash/DRAM 캐시(Redis, memcached, RAMCloud, Pocket 등), ML-specific 캐시(CoorDL, Quiver, OneAccess, DIESEL, Cachew 등), 분산 파일시스템(GFS, Colossus, HDFS, Lustre, Spanner), CacheSack/Janus와의 비교.
- **§9 Conclusion**: Shift가 LRU 대비 1.51-3.28× IO 흡수, 전력 효율 29% 개선을 요약.

## 핵심 용어 (Key terms)
- **Tectonic**: Meta의 엑사바이트 규모 분산 파일시스템으로, disaggregated HDD 스토리지 노드 클러스터가 뒷받침하는 기존 프로덕션 저장 패브릭.
- **Shift**: 각 Tectonic HDD 클러스터 앞단에 놓이는 투명한 flash 캐시 계층으로, Tectonic-Shift의 IO-efficient 컴포넌트.
- **DPP Reader (Data PreProcessing Reader)**: 학습 job에 배정되어 Tectonic File System에서 raw byte를 연속적으로 읽고 텐서로 전처리하는 범용 CPU 노드.
- **CacheLib**: Meta의 범용 캐싱 엔진 라이브러리로, DRAM/flash 관리 및 LRU/FIFO eviction을 제공하며 Shift SN의 내부 데이터 플레인으로 쓰임.
- **Segment**: Shift가 캐시 객체로 다루는 고정 크기(기본 256KB) 데이터 단위로, Tectonic block(72MiB)을 세분한 것.
- **Bucket**: 함께 접근될 것으로 예상되는 segment들의 논리적 그룹(기본적으로 디렉토리 단위)으로, admission/reinsertion 정책이 적용되는 단위.
- **RHW (Request History Window)**: 과거 일정 기간(기본 6시간) 동안의 요청 이력을 추적해 historic priority를 계산하는 근거 데이터.
- **GMS (Global Metadata Store)**: 활성 학습 job들의 dataset 명세(어떤 table partition을 읽을지)를 담은 실시간 데이터베이스로, future priority 계산에 쓰임.
- **AdmitThreshold / ReinsertThreshold**: bucket priority가 이 값을 넘어야 캐시에 admit/reinsert되도록 하는 동적 튜닝 문턱값.
- **DLRM (Deep Learning Recommendation Model)**: Meta ML 인프라 수요를 지배하는 워크로드 유형으로, 구조화된 대규모 feature 데이터를 사용.
- **Scan/Churn pattern**: 학습 job이 페타바이트급 데이터를 한 번씩(단일 epoch) 훑고, 여러 job이 서로 다른 시점에 겹치며 캐시를 자주 교체시키는 antagonist 접근 패턴.

## 강점 · 한계 · 열린 질문
- **강점**: 프로덕션 규모(페타바이트)에서 실제로 배포되어 29% 전력 절감을 입증했고, 기존 Tectonic API·시맨틱을 그대로 유지해 애플리케이션 수정 없이 투명하게 도입 가능하며, CacheLib을 재사용해 구현 복잡도를 낮췄다.
- **한계**: CacheLib이 LRU/FIFO eviction만 지원해 priority 기반 eviction이 불가능하므로, historic/future 정보를 살리려면 reinsertion에 의존해야 하고 이는 추가 flash write(write amplification)를 유발한다(Table4, p.442; §7 p.443). Reinsertion은 HDD read를 18% 줄이지만 write rate 한도를 계속 초과시키는 트레이드오프가 있다.
- **열린 질문(§7, p.443-444)**: (1) data placement/job routing 정책과 Tectonic-Shift를 co-design하면 캐시 working set을 더 줄일 수 있는가? (2) CacheLib에 priority-aware eviction을 추가하면 reinsertion의 이점을 write 오버헤드 없이 얻을 수 있는가? (3) 언제(when) 객체가 미래에 읽힐지까지 고려하는 더 정교한 정책이 가능한가? (4) 현재는 DLRM 중심인데, vision/NLP 등 다른 ML 도메인과 non-ML 워크로드로 확장 시 정책이 여전히 유효한가?

## ❓ Q&A (자가 점검)
> [!question]- Tectonic-Shift가 eviction이 아니라 admission/reinsertion 정책에 집중한 이유는?
> ML 학습 workload가 scan(대규모 단일 epoch 스캔)과 churn(빈번한 재삽입/축출) 패턴을 보여 LRU 같은 eviction 기반 정책은 캐시 전체를 쉽게 thrash시키기 때문이다. Admission 단계에서 무엇을 애초에 캐시에 넣을지 통제하면 thrashing을 막고 flash write rate(endurance)도 함께 제어할 수 있다(p.439).

> [!question]- Historic priority와 Future priority는 각각 무엇을 근거로 계산되는가?
> Historic priority는 Request History Window(RHW)가 관측한 과거 접근 이력에서 $TotalBytes(b)/UniqueBytes(b)$로 계산되는 재사용 비율이다. Future priority는 Global Metadata Store(GMS)에 등록된, 아직 완료되지 않은 활성 학습 job들의 dataset 명세 중 해당 bucket을 포함하는 job의 수다(p.440).

> [!question]- Table 1의 Composite Storage 옵션이 HDD-only 대비 전력을 얼마나 줄이는가?
> 100PB 저장·10TB/s IO를 동시에 만족하는 이상적인 HDD+flash 구성은 flash 1.69배, HDD 1.00배 전력만 필요해, IO 계층 전력 풋프린트를 HDD-only(Option1, 9.92배) 대비 3.69배 줄인다(p.435-436).

> [!question]- Pipelined workload에서 Historic 정책이 LRU보다 오히려 나쁜 성능(0.80×)을 보인 이유는?
> Historic 정책은 job 2가 $P_3$를 읽기 시작하기 전까지는 $P_3$의 바이트를 전혀 admit하지 않아, RHW가 아직 관측하지 못한 초기 접근분을 놓치기 때문이다(p.441-442).

> [!question]- Reinsertion을 켰을 때의 트레이드오프는?
> Hit rate가 3% 증가하고 HDD read가 18% 줄지만(Table4, p.442), write rate가 낮게 제한된 경우 reinsertion이 계속 write limit을 초과시켜 flash 쓰기 마모를 늘린다. 저자들은 CacheLib이 selective(priority 기반) eviction을 지원하면 이 트레이드오프를 해소할 수 있다고 본다(p.442-443).

> [!question]- Shift는 왜 put API를 노출하지 않는가?
> Tectonic File System이 append-only/immutable sealed block 시맨틱을 갖고 파일 시스템 연산(생성/삭제/이름변경)은 Tectonic Metadata Layer가 중앙에서 처리하기 때문에, Shift SN은 데이터 플레인 역할만 하며 miss된 get 요청에 대해서만 캐시에 insert한다. 이는 보안(다중 데이터 사본 관리 불필요)과 단순성을 모두 확보한다(p.438-439).

> [!question]- 프로덕션 평가에서 Expert 수동 튜닝 정책이 실패한 이유는?
> Expert 정책은 고-IOPS table만 admit하도록 정적으로 설정되어, §3에서 특성화한 학습 job들의 제한적(limited) 데이터 재사용 패턴을 포착하지 못해 power neutrality에 필요한 IO의 0.21×밖에 흡수하지 못했다(p.443).

## 🔗 Connections
[[LLM Systems]] · [[ATC]] · [[2023]]
관련: [[PreSto - An In-Storage Data Preprocessing System for Training Recommendation Models]] · [[Nemo - A Low-Write-Amplification Cache for Tiny Objects on Log-Structured Flash Devices]] · [[Heimdall - Optimizing Storage I-O Admission with Extensive Machine Learning Pipeline]]

## References worth following
- **CacheSack: Admission optimization for Google datacenter flash caches** (ATC 2022, Yang et al.) — Shift와 유사하게 Google Colossus Flash Cache의 admission 정책 최적화를 목표로 하며, 카테고리별 admission 정책 분리·튜닝 접근이 Shift의 bucket 기반 정책과 비교할 대상(p.444, 448).
- **Janus: Optimal flash provisioning for cloud storage workloads** (ATC 2013, Albrecht et al.) — Colossus의 flash 계층 시스템으로, Shift와 달리 파일을 flash에 먼저 write-through한 후 HDD로 축출하는 방식이라 설계 철학 대비군.
- **The CacheLib caching engine: Design and experiences at scale** (OSDI 2020, Berg et al.) — Shift의 데이터 플레인 기반이 되는 Meta 범용 캐시 엔진, 세그먼트/eviction 정책 이해에 필수.
- **Facebook's tectonic filesystem: Efficiency from exascale** (FAST 2021, Pan et al.) — Tectonic-Shift가 앞단에 배치되는 기반 파일시스템 원 논문.
- **Quiver: An informed storage cache for deep learning** (FAST 2020, Vijaya Kumar & Sivathanu) — ML-specific 캐시의 대표 사례로, application 정보를 활용한다는 점에서 Shift의 application-aware 접근과 비교 가능.
- **Understanding data storage and ingestion for large-scale deep recommendation model training** (ISCA 2022, Zhao et al.) — 저자들이 인용하는 Meta DLRM workload 특성화 선행 연구로, §3의 workload characterization의 토대.

## Personal annotations
<!-- 본인 메모 영역 -->
