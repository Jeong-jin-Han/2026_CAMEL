---
title: "In-Storage Acceleration of Retrieval Augmented Generation as a Service"
aliases: [RAGX, In-Storage RAG]
description: "RAG 서비스의 병목인 Search & Retrieval을 SSD 내부의 형태변환(metamorphic) 가속기 RAGX로 가속해 NVMe/GPU 대비 최대 4.3x/1.5x end-to-end 처리량을 달성한 ISCA 2025 논문."
venue: ISCA
year: 2025
tier: deep
status: done
tags:
  - paper
  - cluster/isc
  - topic/in-storage
  - topic/rag
  - topic/llm
  - venue/isca
  - year/2025
---

# In-Storage Acceleration of Retrieval Augmented Generation as a Service

> **ISCA 2025** · `cluster/isc` · Source: [In-Storage Acceleration of Retrieval Augmented Generation as a Service.pdf](<In-Storage Acceleration of Retrieval Augmented Generation as a Service.pdf>)

저자: Rohan Mahapatra, Harsha Santhanam, Christopher Priebe, Hanyang Xu, Hadi Esmaeilzadeh (University of California San Diego)

## TL;DR
RAG(Retrieval-Augmented Generation) 서비스를 datacenter에서 end-to-end로 분석하면 LLM inference가 아니라 **Search & Retrieval** 단계가 전체 지연의 평균 61%(networked EBS에서는 88%)를 차지하는 진짜 병목이다. 이 단계는 (1) query embedding을 위한 작은 language model 추론, (2) 다양한 자료구조에 대한 similarity/relevance 계산, (3) persistent storage에 대한 빈번하고 긴 지연의 iterative access라는 세 가지 도전을 동시에 가진다. 저자들은 SSD 내부에 **RAGX**라는 프로그래머블 가속기를 통합해, systolic array와 vector processor 사이를 동적으로 변신하는 **metamorphic accelerator**와 NAND/DRAM 메타데이터를 직접 탐색하는 **Metadata Navigation Unit (MNU)**으로 embedding-based·keyword-based retriever를 모두 지원한다. 결과적으로 Xeon CPU+NVMe 대비 최대 4.3x, A100 GPU+DRAM 대비 최대 1.5x end-to-end 처리량 향상을 얻으며, 데이터셋이 커질수록(0.5M→500M passages) 이득이 커진다.

## 문제 & 동기
RAG는 LLM의 고정된 사전학습 지식을 외부 knowledge base에서 실시간 검색한 정보로 확장해 hallucination을 줄이고, 비싼 재학습 없이 최신성을 유지한다. 그러나 데이터베이스가 계속 커지면서 배포가 in-memory에서 **persistent storage(NVMe SSD)** 기반으로 이동하고 있고, 이로 인해 성능 프로파일이 근본적으로 바뀐다. 저자들이 AWS에서 PubMed(5M docs) + 다양한 retriever로 실측한 결과, 전체 런타임의 평균 **61%가 Search & Retrieval 단계**(LLM inference가 아님)에 소비된다. 이 단계는 (1) language model로 query embedding 생성, (2) data-dependent한 dynamic shape의 similarity/relevance 계산, (3) iteration마다 직전 거리 계산 결과에 의존하는 sequential storage access의 삼중고를 가진다. PCIe interface traversal로 인한 access latency가 매 iteration마다 누적되어 시스템 지연을 크게 키운다.

> [!quote]- 📄 원문 표현 (paper)
> "empirical analysis reveals that the *Search & Retrieval* phase emerges as the dominant contributor to end-to-end latency." (p.1, Abstract)
>
> "we find that, on average, 61% of the total runtime is spent in the *Search & Retrieval* phase rather than LLM inference." (p.2, §1)
>
> "Even in the local NVMe configuration, 74% of the runtime is consumed by storage access, increasing to 94% with networked EBS storage." (p.5, §2.4)

## 핵심 통찰

> [!note]- 통찰 1 — RAG의 진짜 병목은 LLM이 아니라 storage 기반 Search & Retrieval
> 기존 연구 대부분은 LLM inference 가속에 집중했지만, end-to-end RAG-as-a-service를 datacenter에서 측정하면 평균 61%(networked 88%)가 검색 단계다. Search & Retrieval 단계 내부에서도 storage access가 local NVMe 74%, networked EBS 94%를 차지한다. HNSW 같은 metadata graph가 storage access 횟수를 5M passages에서 395회로 줄여도, 각 access가 Samsung 970 EVO에서 평균 155 µs이고 iterative하게 누적되어 여전히 병목이다. (p.2, p.5)

> [!note]- 통찰 2 — embedding-based와 keyword-based retriever는 구조적으로 동일한 병목을 가진다
> 저자들은 retriever 동작을 세 plane으로 개념화한다: representation plane(NVMe의 embedding/posting list), metadata plane(DRAM의 HNSW graph/inverted index), compute plane(similarity/scoring). embedding-based(HNSW graph 탐색)와 keyword-based(inverted index posting list 탐색) 모두 metadata로 storage access를 guide하면서 iterative access와 compute를 interleave하는 동일한 패턴을 가진다. 따라서 둘 다 지원하는 하나의 프로그래머블 가속기가 가능하다. (p.3 §2.3, p.4 Fig.3/4)

> [!note]- 통찰 3 — distance 계산은 두 phase로 분해되어 systolic↔vector 변신을 정당화한다
> RAG의 distance computation은 (1) element-wise vector 연산(2D systolic array에 효율적 매핑)과 (2) normalization 같은 complex 연산(vector unit이 처리)으로 분해된다. neural embedding generation은 GeMM 위주여서 systolic array가 적합하고, distance function은 1D vector processing이 적합하다. 이 dual 특성이 **shape-shifting metamorphic accelerator**(같은 하드웨어가 systolic mode와 vector mode를 동적 전환)의 근거다. (p.6 §4, p.7 §4.1)

> [!note]- 통찰 4 — query embedding과 retrieval을 co-locate해야 network 오버헤드를 제거할 수 있다
> disaggregated datacenter에서 embedding을 별도 GPU 노드에 offload하면 embedding 자체는 빨라져도 두 EC2 instance(같은 zone) 사이 평균 86 ms network latency가 추가되어, idealized Disag-DRAM조차 CPU-DRAM 대비 5M에서 28% 낮은 처리량을 보인다. SSD 내부에 embedding과 retrieval을 함께 둬야 PCIe/network 오버헤드를 모두 없앨 수 있다. (p.5 §2.4)

## 설계 / 메커니즘

> [!abstract]- RAGX 시스템 통합 & data placement
> **In-storage 통합 (Fig.6):** SSD main controller 옆에 가속기를 두어 FTL·flash controller와 직접 상호작용하고, NAND array와 on-chip memory 사이를 DMA engine으로 직접 전송해 PCIe 시스템 인터커넥트 오버헤드를 회피한다. SSD의 엄격한 **15 W power budget** 안에서 동작해야 한다.
> **System primitives:** NVMe command set을 custom command로 확장해 host가 query embedding과 Search & Retrieval을 RAGX로 offload하도록 한다. 기존 NVMe primitive와 backward compatible.
> **Multi-device data placement (§3.2):** embedding-based의 경우 단일 큰 HNSW를 device마다 작은 **private HNSW**로 partition해 inter-device communication을 피하고 병렬 탐색한다(recall은 동일하거나 향상, 누적 compute는 증가). keyword-based의 경우 inverted index(metadata)를 모든 device에 **replicate**하고 posting list만 partition한다. peer-to-peer PCIe로 query를 broadcast하고 결과는 host CPU에서 top-k aggregation. (p.6 §3, §3.1, §3.2)

> [!abstract]- Metamorphic Accelerator 마이크로아키텍처 (Fig.7)
> systolic array of **metamorphic execution engines (XEs)** + 아래쪽 **scalar engines (SEs)** 집합으로 구성. 각 column 위에 vector processor front end가 있다.
> - **Systolic mode:** 각 XE가 conventional PE로 동작(internal mux의 control path=1). fused MAC + local weight buffer + forwarding logic. GeMM(embedding generation)에 최적.
> - **Vector mode:** control logic이 mux를 재구성해 각 column을 vector engine으로 재해석, fine-grained vertical pipelining으로 vector parallelism 확보. column마다 독립 program counter/instruction stream → 동시에 서로 다른 vector kernel 실행. distance function(non-GeMM)에 최적.
> - **SE:** 각 column 아래에 위치, division/square root/logarithm 같은 high-latency scalar 연산(ℓ²-norm, BM25 scoring) 담당. `⟨vector, scalar⟩` instruction pair 실행. systolic mode에서는 SE들이 모여 하나의 horizontal vector processor 형성(normalization/bias addition). (p.7-8 §4.1, §4.2)

> [!abstract]- Metadata Navigation Unit (MNU, Fig.8)
> 가속기에 적절한 데이터를 공급하는 유닛. metadata가 query-dependent하고 데이터 크기/위치가 사전에 unknown이라는 문제를 해결한다. 세 핵심 기능:
> 1. **Metadata interpretation:** DRAM의 HNSW graph(embedding-based) 또는 inverted index(keyword-based)를 walk해 가져올 representation의 주소·크기 결정. embedding-based는 embedding size×neighbor 수, keyword-based는 posting list 길이.
> 2. **NVMe command generation:** 내부 fetch 요청을 표준 NVMe read로 변환. command queue + **LBA translator**(메타데이터의 logical→physical LBA) + scatter-gather **DMA engine**(non-contiguous를 contiguous on-chip buffer로). 내부 대역폭 활용, PCIe 우회.
> 3. **Kernel configuration unit:** retriever별 **parametric pre-compiled kernel template**(template cache + parameter insertion + instruction buffer)을 runtime에 embedding shape/posting list size 등으로 채워 JIT compile 오버헤드 없이 다양한 워크로드 지원.
> 추가로 **Programmable Memory Interface (PMI):** dimension register + stride calculator + address generation으로 multi-dimensional·variable한 access pattern 지원. embedding-based에서 SE는 graph traversal 중 vertex score 정렬(priority queue) 담당. (p.8-9 §4.3)

## 평가

> [!success]- 실험 세팅 & 주요 수치
> **벤치마크 (Table 1):** 5개 RAG — BM25(keyword), SPLADEv2(embedding+keyword), Doc2Vec/ColBERT/GTR(embedding), 모두 LLAMA2 34B로 generation. RAGGED + BioASQ 3,800 queries, PubMed 데이터셋 4개 크기(0.5M/5M/50M/500M passages). top-k=100. (p.9-10)
> **비교 시스템 (Table 2):** CPU-NVMe(baseline), CPU-DRAM, Disag-NVMe, Disag-DRAM, GPU-DRAM, RAGX(metamorphic accelerator, 32×32 XE+32 SE, 4 GB DRAM, 13 W TDP, 1 GHz, $1.09/hr ra3.xlplus 기준). RTL은 Synopsys DC 2023.09 + FreePDK 45 nm, 1 GHz. cycle-level simulator로 평가. (p.10-11)
>
> **Throughput (Fig.9):** CPU-NVMe 대비 평균 **1.6x / 2.3x / 4.3x / 5.7x** (0.5M / 5M / 50M / 500M). 데이터셋 클수록 이득 증가(500M에서 0.5M 대비 평균 3.6x). GTR-LLAMA2는 0.5M→500M로 처리량 1.5x 향상, ColBERT는 0.5M→50M로 2.9x. drive 수: 2 drives 3.2x, 4/8 drives 4.1x(BM25/GTR). (p.10-11 §5.2.1)
> **vs 다른 시스템 (Fig.11):** CPU-DRAM 대비 평균 1.5x, Disag-NVMe 대비 4.3x, Disag-DRAM 대비 1.7x(50M). GPU-DRAM 대비 1.4x이며 GPU-DRAM은 query당 119% 더 비싸다(50M). (p.11 §5.2.1)
> **NVMe latency 감소:** storage access 런타임 비중을 CPU-NVMe의 26.6%/47.7%/63.2%/75% → **4.1%/6.7%/12.8%/40%**로 축소(0.5M/5M/50M/500M). GTR-LLAMA2 50M에서 NVMe latency 72%→27%(2.9x speedup), BM25 50M에서 60.9%→10.3%. (p.10 §5.2.1)
> **Energy (Fig.12):** Search & Retrieval 가속기 단독은 CPU-NVMe 대비 평균 **50x** 에너지 효율, GPU-DRAM 대비 2x. SPLADEv2는 최대 150x. 시스템 전체는 두 A100(400 W)의 Referenced Generation 지배로 1.12x(CPU-NVMe 대비). (p.11-12 §5.2.2)
> **Recall/Accuracy (Fig.13):** keyword-based는 변화 없음(replicate). embedding-based는 8 drives에서 ColBERT recall@10 +21.2%/recall@100 +18%, GTR +4%. end-to-end accuracy는 embedding-based가 2-3% 향상. (p.12 §5.2.3)
> **Cost:** RAGX가 모든 평가 시스템 중 query당 비용 최저. CPU-DRAM은 0.5M→500M에서 119%~391% 더 비쌈. (p.11-12)
> **Batch size (Fig.17):** GPU-NVMe 대비 small batch 8.5x, large batch(256) GPU-DRAM 대비 4.0x. (p.16 §5.2.5)
> **IVF 비교 (Fig.14):** non-graph vector DB(IVF) 대비 CPU-NVMe 기준 20%~7.9x 향상, CPU-DRAM 기준 12%~51%. (p.16 §5.2.4)

## 섹션 노트
- **§1 Introduction:** RAG 동기, context window 확장의 비효율(transformer는 context 길이에 quadratic), Search & Retrieval가 61% 차지. RAGX 제안 및 4.3x/1.5x 결과 요약.
- **§2 RAG: Concept to Datacenter Deployment:** Fig.2의 3단계 파이프라인(Search & Retrieval → Augmentation for Query Reconstruction → Referenced Generation). §2.1 offline vector DB generation(embedding은 NVMe, passage는 S3, metadata graph는 DRAM), §2.2 online query processing, §2.3 storage-compute interplay(3 plane 개념화), §2.4 datacenter characterization(병목 정량화).
- **§3 RAGX: In-Storage Acceleration for RAGs:** §3.1 system integration(NVMe command 확장, firmware, host/device·device/device 통신), §3.2 data placement(private HNSW partition, replicated inverted index).
- **§4 Metamorphic Accelerator:** §4.1 shape-shifting 개념(systolic↔vector 두 phase), §4.2 microarchitecture(XE/SE, Fig.7), §4.3 MNU(metadata interpretation, kernel config, PMI, NVMe command generation, compiler support).
- **§5 Evaluation:** §5.1 methodology, §5.2.1 throughput/cost, §5.2.2 energy, §5.2.3 recall/accuracy, §5.2.4 IVF 비교, §5.2.5 sensitivity(LLM config, networked drive, batch).
- **§6 Related Work:** systems for RAG(in-DRAM 가정), in-storage acceleration(genomics, DB, DL; ANN은 graph traversal/bitonic sort point solution만 존재, 프로그래머블·embedding generation 미지원).
- **§7 Conclusion / Appendix A:** artifact(GitHub he-actlab/ragx, Docker, RAGX simulator, CPU-DRAM baseline).

## 핵심 용어
- **RAG (Retrieval-Augmented Generation)**: LLM에 외부 DB의 실시간 검색 정보를 주입해 context를 확장하고 hallucination을 줄이는 기법.
- **Search & Retrieval phase**: query를 embedding으로 변환하고 DB에서 top-k passage를 찾는 RAG 첫 단계. 본 논문이 지목하는 진짜 병목.
- **Metamorphic accelerator**: systolic array(GeMM/embedding)와 vector processor(distance function) 사이를 동적으로 변신하는 RAGX의 핵심 가속기.
- **XE (Metamorphic Execution Engine)**: systolic mode에서는 PE, vector mode에서는 pipeline stage로 동작하는 processing element. MAC + scratchpad + mux 네트워크.
- **SE (Scalar Engine)**: 각 column 아래의 division/sqrt/log 등 high-latency scalar 연산 유닛. ℓ²-norm, BM25 scoring 담당.
- **MNU (Metadata Navigation Unit)**: HNSW/inverted index 메타데이터를 walk해 데이터 주소·크기를 결정하고 NVMe command를 생성, kernel template을 채우는 유닛.
- **PMI (Programmable Memory Interface)**: dimension register/stride calculator/address generation으로 가변·다차원 메모리 access를 지원하는 MNU 구성요소.
- **HNSW (Hierarchical Navigable Small World)**: embedding-based retriever가 사용하는 graph metadata. semantic 근접 vertex를 잇는 graph로 storage access 횟수를 줄임.
- **Inverted index / posting list**: keyword-based retriever의 metadata/representation. keyword→posting list(passage ID + frequency).
- **Representation/Metadata/Compute plane**: retriever 동작을 storage(representation)/DRAM(metadata)/연산(compute)으로 나눈 개념적 3 plane.
- **Disaggregated datacenter**: compute와 storage 노드가 network로 분리된 datacenter. embedding offload 시 ~86 ms network latency 유발.

## 강점 · 한계 · 열린 질문
**강점**
- end-to-end RAG-as-a-service를 실제 AWS에서 측정해 통념(LLM이 병목)을 반박한 강한 동기. 5개 retriever·4개 데이터셋 크기·6개 시스템의 광범위한 평가.
- embedding-based와 keyword-based를 모두 지원하는 단일 프로그래머블 가속기(기존 in-storage ANN은 point solution). 데이터셋이 커질수록 이득 증가(scale-out 친화적).
- 15 W power budget을 지키면서 50x 에너지 효율(검색 단계)과 최저 query 비용 동시 달성.

**한계**
- RAGX는 실리콘 칩이 아니라 RTL synthesis + cycle-level simulator + AWS 실측 trace 조합으로 평가됨(실제 클라우드 배포 chip 부재). NVMe latency는 open-source simulator 모델링.
- 시스템 전체 에너지/처리량 이득은 두 A100(400 W)의 Referenced Generation이 지배해 희석됨(시스템 energy 1.12x). 검색 가속의 큰 이득이 LLM inference 비용에 묻힘.
- private HNSW partition은 누적 compute를 늘리는 trade-off; recall 향상은 retriever 의존적(GTR 4%, ColBERT 18%).
- GTR-LLAMA2 500M은 A100 DRAM에 embedding이 안 맞아 GPU-DRAM 비교 불가 등 대규모 실측 한계.

**열린 질문**
- 실제 SSD 컨트롤러에 metamorphic accelerator를 통합했을 때 thermal/area/yield 비용은? 15 W budget의 현실성은?
- batch가 커지고 H100/FlashAttention 등 LLM inference가 빨라지면 검색 가속의 상대적 이득은 어떻게 변하나(§5.2.5는 일부만 다룸)?
- IVF 외 최신 vector DB(예: DiskANN류)나 hybrid retriever에서도 동일한 이득이 유지되는가?

## ❓ Q&A (자가 점검)

> [!question]- Q1. 이 논문이 반박하는 RAG에 대한 통념은 무엇이고, 근거 수치는?
> 통념: RAG의 병목은 LLM inference다. 반박: end-to-end로 측정하면 Search & Retrieval가 평균 61%(networked EBS 88%)를 차지한다. Search & Retrieval 내부에서도 storage access가 local NVMe 74%, networked 94%다. (p.1, p.5)

> [!question]- Q2. metamorphic accelerator가 "변신"하는 두 모드와 각각의 용도는?
> Systolic mode: 각 XE가 PE로 동작, GeMM 즉 query embedding generation(language model 추론)에 최적. Vector mode: column을 vector engine으로 재구성, distance function 같은 non-GeMM similarity 계산에 최적. distance 계산이 element-wise vector phase와 complex(normalization) phase로 분해된다는 통찰에 기반. (p.7 §4.1)

> [!question]- Q3. MNU가 해결하는 핵심 문제와 세 가지 기능은?
> 문제: metadata가 query-dependent라 가져올 데이터의 크기·위치가 사전에 unknown. 기능: (1) HNSW/inverted index를 walk해 주소·크기 결정(metadata interpretation), (2) 내부 fetch를 NVMe read로 변환(LBA translator + scatter-gather DMA, PCIe 우회), (3) parametric kernel template을 runtime에 채워 JIT 없이 다양한 retriever 지원(kernel configuration). (p.8-9 §4.3)

> [!question]- Q4. embedding-based와 keyword-based retriever의 multi-device data placement 전략은 어떻게 다른가?
> embedding-based: 단일 큰 HNSW를 device별 작은 private HNSW로 partition해 inter-device 통신 없이 병렬 탐색(recall 유지/향상, 누적 compute 증가). keyword-based: inverted index(metadata)를 모든 device에 replicate하고 posting list만 partition, 결과는 host CPU에서 top-k aggregation. (p.6 §3.2)

> [!question]- Q5. RAGX의 처리량 이득이 데이터셋 크기에 따라 어떻게 변하며 왜 그런가?
> CPU-NVMe 대비 평균 1.6x(0.5M)→2.3x(5M)→4.3x(50M)→5.7x(500M). DB가 커질수록 필요한 storage access 수가 비례 증가하는데(예: ColBERT 393→3023회) RAGX가 그 storage overhead를 제거하므로 이득이 커진다(500M에서 0.5M 대비 3.6x). (p.10-11 §5.2.1)

> [!question]- Q6. query embedding을 별도 GPU 노드로 offload하지 않고 SSD 내부에 co-locate하는 이유는?
> disaggregated에서 embedding offload는 두 EC2 instance 사이 평균 86 ms network latency를 추가해, embedding 자체를 빠르게 해도 idealized Disag-DRAM조차 CPU-DRAM 대비 5M에서 28% 낮은 처리량을 보인다. co-location으로 PCIe/network 오버헤드를 모두 제거한다. (p.5 §2.4)

> [!question]- Q7. 시스템 전체 에너지 이득(1.12x)이 검색 단계 이득(50x)보다 훨씬 작은 이유는?
> 시스템 에너지는 Referenced Generation을 수행하는 두 NVIDIA A100(각 400 W TDP)이 지배하기 때문. RAGX는 Search & Retrieval만 가속하므로 그 단계에서는 CPU-NVMe 대비 50x, GPU-DRAM 대비 2x이지만 전체로 보면 LLM inference 에너지에 희석된다. (p.11-12 §5.2.2)

> [!question]- Q8. RAGX 평가 방법의 한계(왜 실측이 아닌가)는?
> 연구용 칩의 실제 클라우드 배포가 현실적으로 불가능해, RTL synthesis(Synopsys DC, FreePDK 45 nm, 1 GHz) + cycle-level simulator + AWS 실측 trace(embedding/search 시퀀스, NVMe latency는 open-source simulator로 모델링) 조합으로 평가했다. Augmentation/Referenced Generation은 실제 AWS 측정값을 사용. (p.10-11 §5.1)

## 🔗 Connections
[[In-Storage Computing]] · [[ISCA]] · [[2025]]

## References worth following
- ColBERT: Efficient and Effective Passage Search via Contextualized Late Interaction over BERT (Khattab & Zaharia, SIGIR 2020) [36] — embedding-based retriever 대표 모델, 본 논문 벤치마크.
- SPLADE v2: Sparse Lexical and Expansion Model for Information Retrieval (Formal et al., 2021) [15] — keyword+embedding hybrid retriever 대표.
- GenStore: A High-Performance In-Storage Processing System for Genome Sequence Analysis (ASPLOS) [58] — in-storage acceleration 인접 사례, NVMe latency 모델링에도 활용.
- AttAcc! Unleashing the Power of PIM for Batched Transformer-based Generative Model Inference (ASPLOS 2024) [67] — LLM inference 가속(Referenced Generation 측) 대비군.
- RAGGED / BioASQ benchmark (Krithara et al.; Scientific Data) [24][39] — 평가에 사용한 RAG 벤치마크 suite·query set.
- NDSEARCH: Accelerating Graph-Traversal-Based ANN Search through Near Data Processing (Yiu et al.) [89] — in-storage ANN point solution, 본 논문이 차별화하는 선행연구.

## Personal annotations
<본인 메모 영역>
