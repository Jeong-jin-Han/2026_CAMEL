# AMPM (Access Map Pattern Matching)

## 분류
- **HW/SW**: HW — 전용 pattern matching logic와 memory access map table 회로로 프리페치 후보를 하드웨어에서 병렬 생성한다.
- **offline/online**: online — 런타임에 최근 접근된 zone("hot zone")을 감지하고 access footprint를 실시간 갱신하며 결정한다.
- **PC-localized / non-PC-localized**: non-PC-localized — memory access map은 zone(고정 크기 메모리 영역)별로 상태를 저장하고 "access order도 instruction address(PC)도 저장하지 않는다"고 명시, 즉 주소 영역 기준 추적이다.
- (근거 페이지: 초록·§2, "does not store any access order nor instruction address")

## 핵심 아이디어
- **메모리 패턴 인식 방식**: hot zone의 각 cache line 접근 이력을 2-bit state로 표현한 "memory access map"에 대해 stride 기반 pattern matching으로 접근 패턴을 인식한다.
- **작동 원리**: access map을 정렬(shift)한 뒤 병렬 pattern matching logic으로 다수의 stride 후보를 동시에 검출해 여러 프리페치 요청을 한 번에 생성한다.
