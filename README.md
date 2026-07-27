# Autonomiczny System Rozpoznawania Mowy (Arduino)

Zaawansowany system rozpoznawania poleceń głosowych działający w pełni autonomicznie na mikrokontrolerze[cite: 2]. Układ próbkuje dźwięk z mikrofonu, przetwarza go za pomocą cyfrowych filtrów pasmowych i analizy przejść przez zero, a następnie porównuje z wgranymi wzorcami, wyświetlając wynik na ekranie OLED[cite: 2].

---

## Wykorzystane komponenty

* **Mikrokontroler** (zgodny z Arduino Nano)
* **Modół mikrofonu** (z regulacją wzmocnienia i offsetem DC)
* **Wyświetlacz OLED** (I2C) do prezentacji komunikatów i wyniku
* **Elementy bierne**: Rezystory (2k2), kondensator (100n)
* 
---

## Koncepcja działania

1. **Próbkowanie dźwięku**: Sygnał z mikrofonu jest analizowany w czasie rzeczywistym przez przetwornik ADC. Po przekroczeniu progu głośności nagranie dzielone jest na **13 segmentów czasowych**.
2. **Ekstrakcja cech**: Dla każdego segmentu wyodrębniane są cechy charakterystyczne za pomocą cyfrowych filtrów pasmowych i analizy przejść przez zero (macierz liczb).
3. **Algorytm dopasowywania**: Odcisk głosu jest porównywany z 10 zapisanymi wzorcami z uwzględnieniem odchylenia standardowego i przesunięć czasowych.
4. **Prezentacja wyniku**: Najlepiej dopasowany wzorzec jest natychmiast wyświetlany na ekranie OLED.

---

## Proces treningu i słownik

Wzorce bazują na 10 słowach: 
`URUCHOM`, `DOM`, `POCZĄTEK`, `DZIESIĘĆ`, `JEDEN`, `LUSTRO`, `SZAFA`, `PRAWO`, `MYSZ`, `SZNUR`.

* **Zbiórka danych**: Słowa były wielokrotnie wypowiadane, a surowe macierze cech przesyłano przez port szeregowy do komputera.
* **Generowanie wzorców**: Dane uśredniono i wyznaczono dla nich odchylenia standardowe.
* **Implementacja (PROGMEM)**: Wygenerowane statystyki zapisano w pliku nagłówkowym `Templates.h` w pamięci flash (`PROGMEM`), co pozwoliło oszczędzić pamięć RAM mikrokontrolera.

---

Projekt został zrealizowany na podstawie artykułu, który można znaleźć pod następującym linkiem:
https://www.instructables.com/Speech-Recognition-With-an-Arduino-Nano/
