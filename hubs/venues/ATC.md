---
title: ATC
aliases: [ATC, USENIX ATC]
tags: [meta/hub, hub/venue]
---
# ATC — venue hub

USENIX Annual Technical Conference.

## Papers (this wiki)

- [[DecouKV]] — ATC 2025
- [[HotRAP]] — ATC 2025
- [[Z-LFS]] — ATC 2025
- [[Ethane]] — ATC 2024
- [[FastCommit]] — ATC 2024
- [[ScalaAFA]] — ATC 2024

## 🏛️ 왜 여기서 accept되나 (논문별 축적)
> 각 줄 = ✅ 잘한 점(원문 근거)에서만. (규약: [nodegraph_review.md](../../workflow/nodegraph_review.md) §3-0-2) · ⓡ = ref

- [[DirectCXL]] (2022) ⓡ — 돋보인 점: 상용 CXL 2.0 IP가 없어 CXL controller/switch/RISC-V RP를 전부 자작한 최초의 실물 CXL.mem disaggregation + 동일 testbed에서 RDMA를 64B load 8.3× 실측. (p.5-6)
- [[Overcoming the Memory Wall]] (2023) ⓡ — 돋보인 점: 최초 open-source CXL-flash feasibility 특성화로 flash-backed CXL memory 실현성을 real-application trace로 입증. (p.3)

### 취향 요약 (누적 관찰 — ≥3편일 때 확정)
- (2편 관찰: 둘 다 '실물 실측' 또는 'open-source 특성화'로 실용성을 증명 — ATC의 실용성·완성도 taste와 정합. 1편 더 축적 시 확정.)

## 🆕 26S v2 신규 (deep 완료)
> 2026-07-16 리스트 개정으로 편입 · PDF 전체 정독 완료(deep). venue/year는 PDF 실물 기준(시트 정정 포함).
- [[Light-Dedup - A Light-weight Inline Deduplication Framework for Non-Volatile Memory File Systems]] — ATC 2023 🆕(deep)
- [[Tectonic-Shift - A Composite Storage Fabric for Large-Scale ML Training]] — ATC 2023 🆕(deep)
