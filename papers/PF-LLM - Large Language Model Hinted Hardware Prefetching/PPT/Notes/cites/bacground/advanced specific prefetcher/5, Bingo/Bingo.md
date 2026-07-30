# Bingo

## 분류
- **HW/SW**: HW — 전용 회로로 런타임에 page footprint를 history table에 저장·조회하는 hardware spatial data prefetcher.
- **offline/online**: online — trigger access가 발생할 때마다 관측된 접근을 관찰하고 실행 중에 예측·prefetch를 발행.
- **PC-localized / non-PC-localized**: PC-localized — 상태(footprint)를 `PC+Address`(long)와 `PC+Offset`(short) 등 PC 기반 event에 연관지어 추적하는 PPH(Per-Page History) 계열.

## 핵심 아이디어
- **메모리 패턴 인식 방식**: 한 page의 접근 footprint(어떤 cache block을 썼는지의 bit vector)를 하나가 아닌 여러 event(긴 `PC+Address`~짧은 `PC+Offset`)에 동시에 연관지어 spatial correlation을 포착.
- **작동 원리**: TAGE 방식으로 단일 통합 history table을 서로 다른 event로 여러 번 조회하여, 가장 긴(=가장 정확한) 매칭 event의 footprint를 찾아 accuracy와 prediction 기회를 모두 확보하며 prefetch.
