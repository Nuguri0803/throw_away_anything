# Wiring Guide

## 공통 GND

다음 GND는 모두 공통 연결합니다.

- Battery -
- Arduino GND
- 24V 승압 컨버터 GND
- 13V 승압 컨버터 GND
- 5V 벅 컨버터 GND
- BTS7960 GND
- IRLZ44N Source
- WS2813 LED GND
- Fan GND

## 전원 분배

| 전원 | 연결 대상 |
| --- | --- |
| 11.1V LiPo -> 24V Boost | BTS7960 B+, B- |
| 11.1V LiPo -> 13V Boost | 니크롬 열선 +, MOSFET Source 기준 GND |
| 11.1V LiPo -> 5V Buck | Arduino 5V, LED 5V, Fan 5V |

배터리 + 바로 뒤에는 메인 스위치를 두는 것을 권장합니다. 퓨즈를 사용한다면 배터리 + 뒤에 인라인 자동차 블레이드 퓨즈 홀더를 배치합니다.

## BTS7960 모터 드라이버

| BTS7960 | 연결 |
| --- | --- |
| B+ | 24V Boost OUT+ |
| B- | 24V Boost OUT- |
| M+ / M- | DC 감속모터 양단 |
| VCC | Arduino 5V |
| GND | Arduino GND |
| R_EN / L_EN | Arduino 디지털 핀 |
| RPWM / LPWM | Arduino PWM 핀 |

제어 원칙:

- 정방향: `R_EN = HIGH`, `L_EN = HIGH`, `RPWM = PWM`, `LPWM = 0`
- 역방향: `R_EN = HIGH`, `L_EN = HIGH`, `RPWM = 0`, `LPWM = PWM`
- 정지: `RPWM = 0`, `LPWM = 0`

## 니크롬 열선 + IRLZ44N

| 부품 | 연결 |
| --- | --- |
| 13V Boost OUT+ | 니크롬 열선 한쪽 |
| 니크롬 열선 반대쪽 | IRLZ44N Drain |
| IRLZ44N Source | 13V Boost OUT- |
| Arduino PWM | 100~220 ohm 직렬 저항 -> Gate |
| Gate | 10k ohm 풀다운 -> GND |

주의:

- Gate에 13V를 넣지 않습니다.
- 열선 라인에는 점퍼선과 브레드보드를 사용하지 않습니다.
- 열선 주변에는 세라믹 단자대, 운모판, 세라믹판, 알루미늄 반사판을 사용합니다.
- 아크릴과 일반 플라스틱은 열선에서 충분히 이격합니다.

## WS2813 RGB LED

| LED | 연결 |
| --- | --- |
| +5V | 5V Buck OUT+ |
| GND | 5V Buck OUT- / Arduino GND |
| DI | Arduino 데이터 핀 -> 330 ohm 저항 |

LED 5V-GND 사이에 1000uF 전해 커패시터를 병렬 연결합니다. 극성은 긴 다리 +, 짧은 다리 - 입니다.

## 팬

단순 냉각 목적이면 5V에 직결할 수 있습니다. Arduino로 제어하려면 소형 MOSFET 또는 트랜지스터 스위칭 회로를 사용합니다. 이 저장소의 기본 펌웨어는 팬 제어 핀을 사용하도록 작성되어 있습니다.
