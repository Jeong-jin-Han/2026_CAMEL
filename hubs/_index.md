---
title: Paper Wiki — Index
aliases: [Index, Home]
tags: [meta/hub]
---
# CAMEL 26S Paper Wiki — Index

SSD-list 논문 위키의 마스터 진입점. 다축(토픽·학회·연도)으로 navigate. 작성 규칙은 [paper_wiki.md](../workflow/paper_wiki.md).

## By Topic (cluster hubs)
- [[CXL]] · [[In-Storage Computing]] · [[LLM Systems]] · [[KV-LSM]] · [[File System]] · [[Reliability]] · [[Infra]] · [[ZNS]] · [[Vector Search]]

## By Venue
- [[HPCA]] · [[ISCA]] · [[NSDI]] · [[FAST]] · [[ASPLOS]] · [[ATC]] · [[EuroSys]] · [[OSDI]] · [[MICRO]] · [[SOSP]]

## By Year
- [[2026]] · [[2025]] · [[2024]] · [[2023]]

## 현재 deep 정독 완료
**발표 4편 (mine)**
- [[SkyByte]] — HPCA'25, memory-semantic CXL-SSD (OS·HW co-design), 6.11×
- [[XHarvest]] — ISCA'25, CXL+TEE host 자원 harvesting SSD, cost −31.50%
- [[Smart-Infinity]] — HPCA'24, near-storage LLM training, 2.11×
- [[Sparse Checkpointing for Fast and Reliable MoE Training]] — NSDI'26, MoE sparse checkpointing (MoEvement), 8×

**배치① 추가분 (arXiv 확보)**
- [[Conduit]] — HPCA'26, programmer-transparent NDP, 1.8×
- [[Nemo]] — ASPLOS'26, tiny-object flash cache, WA 1.56
- [[Getting the MOST]] — FAST'26, mirror-optimized tiering, 2.34×
- [[InstAttention]] — HPCA'25, in-storage attention offloading, 11.1×
- [[Mooncake]] — FAST'25, KVCache-centric LLM serving, +525%

**배치② 추가분 (arXiv 확보)**
- [[HotRAP]] — ATC'25, LSM hot record retention/promotion, 5.4×
- [[Flash Caches FDP]] — EuroSys'25, NVMe FDP flash cache, DLWA 1.03
- [[CIPHERMATCH]] — ASPLOS'25, in-flash homomorphic encryption matching, 136.9×
- [[DockerSSD]] — HPCA'24, containerized in-storage processing, 7.9×
- [[Venice]] — ISCA'23, SSD conflict-free interconnect, 2.65×
- [[LeaFTL]] — ASPLOS'23, learning-based FTL, 매핑 7.5–37.7×↓
- [[GPU-Initiated BaM]] — ASPLOS'23, GPU-initiated storage access, 21.7× 저렴

## 📚 Concepts (배경지식 — 논문과 별개)
- **CXL**: [[CXL Overview]] (한 눈에) → [[CXL 1.0-1.1]] · [[CXL 2.0]] · [[CXL 3.0]] · [[CXL 3.1]] · [[CXL 3.2]] · [[CXL 4.0]] · [[CXL Glossary]] · [[CXL SOTA & Roadmap]]

## 🧭 Insights (교수님 framing — 논문과 별개)
- [[Communication Tax]] — *Compute Can't Handle the Truth* (Myoungsoo Jung, Panmnesia Tech Report, arXiv 2507.07223). communication tax → CXL composable → CXL-over-XLink. **발표 4편을 관통하는 상위 맥락.**

## 진행 현황
- 🎉 **SSD-list 75편 전부 deep 정독·노트 완료** (deep 75 / stub 0)
  - mine 4 + arXiv 자동 확보분 + 사용자 PDF 제공분(나머지 전부)
  - 모든 노트: 한국어 본문 + 원문 영어 인용 + 접이식 토글 + Q&A 자가점검
- insights: 1 ([[Communication Tax]]) / concepts: CXL 9
- 추적: `tmp/batch36_tracker.md` · `tmp/batch2_tracker.md`
- (참고) 허브의 curated "## Papers" 목록은 일부만 나열 — 나머지는 각 논문→허브 wikilink로 그래프·backlink에 자동 연결됨
