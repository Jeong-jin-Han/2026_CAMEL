# MLOP (Multi-Lookahead Offset Prefetching)

## 분류
- **HW/SW**: HW — 전용 회로(Access Map Table + Scores 테이블 + 단순 로직)로 구현되는 하드웨어 offset prefetcher다.
- **offline/online**: online — 런타임에 접근을 관찰하며 offset별 점수를 갱신하고 실행 중 best offset을 선택해 prefetch를 발행한다.
- **PC-localized / non-PC-localized**: non-PC-localized — PC가 아닌 메모리 주소(base address의 bit-vector 접근맵) 기준으로 상태를 추적하는 offset 기반 prefetcher다.

## 핵심 아이디어
- **메모리 패턴 인식 방식**: 접근 스트림을 특정 스트림에 묶지 않고 offset 단위로 처리하되, 각 offset이 "몇 개의 lookahead(선행) 앞에서 미래 접근을 덮을 수 있는지"를 여러 lookahead 레벨(16단계)마다 개별 점수로 평가한다.
- **작동 원리**: Access Map Table의 bit-vector로 최근 접근을 기록해 lookahead 레벨별 offset 점수를 매긴 뒤, 각 lookahead마다 best offset을 골라(작은 lookahead 우선) prefetch를 발행함으로써 miss coverage와 timeliness를 동시에 확보한다.
