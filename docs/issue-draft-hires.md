# Projekt issue do angree/AmiXcom (wersja polska, do akceptacji)

> Treść do przetłumaczenia na angielski przed wysłaniem. Wysyła użytkownik albo Claude po zgodzie.

---

**Tytuł:** RTG: natywne 640×400 już działa, pomiary na Warp1260X (68060) i propozycja PR-ów

Cześć,

testuję AmiXcom 0.9.14 na prawdziwym sprzęcie: Warp1260X (68060, wbudowane RTG Warpa, Picasso96
2.455 / cybergraphics 42.7), AmigaOS 3.2, dane UFO z wersji DOS. Port na RTG działa bardzo dobrze –
dziękuję za tak rzetelnie opisany projekt, notatki w `PROGRESS.md` i `LEFTOFF.md` bardzo pomogły.

Chciałbym zaproponować pracę nad wyższą rozdzielczością na RTG i zapytać, czy przyjmiesz takie zmiany
i w jakiej formie.

## 1. Natywne 640×400 działa bez zmian w kodzie

OpenXcom pozwala ustawić rozdzielczość bazową. W `user/options.cfg`:

```
displayWidth: 640
displayHeight: 400
geoscapeScale: 5        # SCALE_SCREEN
battlescapeScale: 5
nonSquarePixelRatio: false
```

albo w grze: Options → Video (rozdzielczość 640×400, obie skale „Screen”). sdlmini otwiera wtedy ekran
RTG 640×400×8 (LockBitMap + memcpy) i gra rysuje natywnie: większy glob, ok. 4× większy widok bitwy,
okna 320×200 wyśrodkowane 1:1, mysz trafia poprawnie. Menu, Geoscape i bitwa działają (sprawdzone
w WinUAE i na Warpie).

## 2. Pomiary na Warpie (68060, ekran CSGFX)

Binarka `openxcom-rtg` 0.9.14 zbudowana z repozytorium bez zmian; sondy `amigaPerfLog: 1`; sceny
sterowane autoinputem (bitwa z `amigaAutoBattle`, początek misji). ms = czas pracy na klatkę.

| Scena | 320×200 | natywne 640×400 | 640×480 (skalowane 2×) |
|---|---|---|---|
| Bitwa – bezruch | 50 kl./s (14 ms) | 26 kl./s (37 ms) | 6 kl./s |
| Bitwa – ruch kursora | 43–46 kl./s (19–20 ms) | 18 kl./s (53 ms) | 5 kl./s |
| Bitwa – przewijanie | 45 kl./s (17 ms) | 16–17 kl./s (58 ms) | 6 kl./s |
| Geoscape – bezruch | 50 kl./s (13 ms) | 17 kl./s (57 ms) | 6 kl./s |
| Geoscape – obrót globu | 42 kl./s (18 ms) | 11 kl./s (86 ms) | 6 kl./s |
| Geoscape – zoom | 46 kl./s (16 ms) | 4 kl./s (229 ms) | 6 kl./s |

Obserwacje:

- **Tryb 640×480 na RTG: 5–6 kl./s.** `Zoom::flipWithZoom` (`native/oxc-replace/Engine/Zoom.cpp`)
  w każdej klatce wypełnia cały ekran (`SDL_FillRect`) i skaluje z dzieleniem na każdy piksel
  (`x * src->w / dst->w`, 256 000 dzieleń na klatkę) – `flip` trwa ok. 146 ms.
- **640×400: ok. 18 ms na klatkę w `flip`** w każdej scenie: pełne porównanie ekranu w diff-copy
  (`blit8`, `memcmp` z libnix bajt po bajcie na każdą komórkę 32 B), do tego czyszczenie całego
  ekranu w każdej klatce (`fills` = 256 000 px/kl.). Kopia na kartę jest pomijana w 96–99% klatek.
- **Glob w 640×400:** 36 ms w bezruchu, 208 ms przy zoomie. Już w 320×200 każde pełne przerysowanie
  globu to pojedyncza klatka ok. 300 ms (szarpnięcie przy obrocie).
- **Bitwa w bezruchu w 640×400:** ok. 18 ms na skopiowanie obrazu mapy z cache, choć klatka się
  nie zmieniła.

## 3. Propozycja

Osobne, małe PR-y, każdy z pomiarem przed i po (WinUAE + Warp) i bez zmian w zachowaniu AGA:

1. **diff-copy w `blit8`**: porównywanie komórek długimi słowami zamiast `memcmp` (pomaga także
   w 320×200 i na AGA). Wyniki niżej / w PR.
2. **Mniej pracy, gdy klatka się nie zmieniła** (pomysł z Twojego `LEFTOFF.md`: nie czyścić i nie
   przerysowywać stanów, gdy nic się nie unieważniło).
3. **Glob w wyższej rozdzielczości** (dławik przerysowań / cache).
4. **Wybór backendu RTG** według trybu wybranego przez gracza: dziś `sdlmini_backend.c` sprawdza
   tylko, czy karta ma tryb 320×200.

## 4. Pytania

1. Czy przyjmiesz takie PR-y? Wolisz jeden na problem, jak opisałem?
2. Jak udostępnić graczowi wybór trybu? Proponuję jedną opcję w AmiXcomPrefs: „Ekran RTG: 320×200 /
   640×400”, która ustawia od razu rozdzielczość i obie skale (zamiast trzech list w Options → Video).
3. Co z trybem 640×480 (skalowanym) na RTG? Na Warpie 5–6 kl./s. Ukryć go dla RTG, zostawić jako
   opcję, czy przyspieszyć skalowanie 2×?
4. Czy planujesz coś w tym kierunku sam? Jeśli tak, chętnie pomogę testami na sprzęcie zamiast kodu.

Uwaga techniczna: buduję w Dockerze (bebbo gcc 6.5.0b z obrazu `sacredbanana/amiga-compiler`,
nowszy niż Twój). Binarki wychodzą o ok. 5% mniejsze (inna liczba instancji szablonów). Wszystkie
pomiary porównuję w obrębie tego samego toolchainu.

Pozdrawiam
