# Sandbox

## 분류
- **HW/SW**: HW — L2 접근마다 Bloom filter 기반 sandbox와 candidate 평가를 수행하는 전용 하드웨어 prefetcher(software prefetch 명령 삽입 없음).
- **offline/online**: online — 런타임에 candidate offset prefetcher들을 sandbox에서 시뮬레이션·평가해 정확도가 threshold를 넘으면 실제 prefetch를 켠다.
- **PC-localized / non-PC-localized**: non-PC-localized — 특정 stream이나 PC별 상태가 아니라 전역 주소 공간의 모든 memory access 패턴을 기준으로 offset을 평가한다(코어당 sandbox 1개).
- Ref: [[Sandbox_Prefetching_Safe_run-time_evaluation_of_aggressive_prefetchers.pdf]] (Pugsley et al., HPCA 2014)

## 핵심 아이디어
- **메모리 패턴 인식 방식**: 각 candidate offset $O$에 대해, 임의 주소 $A$ 접근 뒤 $A+O$ 접근이 실제로 뒤따르는지를 전체 access에 걸쳐 검사해 유용한 고정 offset을 전역적으로 확인한다.
- **작동 원리**: candidate가 낼 prefetch 주소를 실제 fetch 없이 Bloom filter(sandbox)에만 넣고, 이후 접근이 그 filter에 hit하면 점수를 올려 threshold 초과 시에만 실제 메모리 prefetch를 발동한다(대역폭·캐시 오염 없는 안전한 평가).
