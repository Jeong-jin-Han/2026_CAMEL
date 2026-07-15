---
title: HPCA
aliases: [HPCA]
tags: [meta/hub, hub/venue]
---
# HPCA — venue hub

IEEE International Symposium on High-Performance Computer Architecture.

## Papers (this wiki)

- [[Conduit]] — HPCA 2026
- [[N-DIPPER]] — HPCA 2026
- [[CCZNS]] — HPCA 2025
- [[InstAttention]] — HPCA 2025
- [[Lincoln]] — HPCA 2025
- [[NVMePass]] — HPCA 2025
- [[PIMnet]] — HPCA 2025
- [[SkyByte]] — HPCA 2025 🟡 🎤7/23
- [[Zebra]] — HPCA 2025
- [[BeaconGNN]] — HPCA 2024
- [[DockerSSD]] — HPCA 2024
- [[LightPool]] — HPCA 2024
- [[Midas Touch]] — HPCA 2024
- [[RiF]] — HPCA 2024
- [[Smart-Infinity]] — HPCA 2024 🟡
- [[OptimStore]] — HPCA 2023

## 🏛️ 왜 여기서 accept되나 (논문별 축적)
> 각 줄 = 그 논문의 ✅ 잘한 점(원문 근거)에서만. 페이지 없는 단정 금지. (작성 규약: [nodegraph_review.md](../../workflow/nodegraph_review.md) §3-0-2)

- [[SkyByte]] (2025) — 돋보인 점: FPGA prototype(Xilinx Zynq UltraScale+ ZU3EG)으로 write log/data cache 성능모델을 **실측 검증**(p.9) + retire-time exception으로 false-positive context switch를 **"추가 HW 비용 없이"** 제거(*"eliminates false-positive context switches ... at no extra hardware cost"*, p.5) → HPCA의 **"HW 메커니즘 novelty + feasibility·구현 친화"** taste와 정합.
- [[AstriFlash]] (2023) ⓡ — 돋보인 점: black box·SSD 무손 상태에서 100ns user-level HW thread switch(switch-on-miss)로 µs flash 지연을 은닉해 p99 tail 2% 손실 + 추가 HW 2KB/0.001mm²로 feasibility 실측 → HPCA의 HW novelty+feasibility taste와 정합. (p.4, p.8, p.10)

### 취향 요약 (누적 관찰 — ≥3편일 때만, "관찰"로)
- (2편: SkyByte·AstriFlash — 둘 다 **HW 메커니즘 + FPGA/실측 feasibility**로 채택. 관찰 축적 중, ≥3편 시 확정.)
