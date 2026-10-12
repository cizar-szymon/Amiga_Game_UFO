AmiAgent 1.1 - zdalne sterowanie Amiga z PC (wgrywanie i uruchamianie programow,
zrzuty ekranu, tytuly okien, klawisze). Wymaga stosu TCP/IP (Roadshow) i OS 3.0+.

Instalacja (raz):
 1. Skopiuj AmiAgent do C:
 2. Ustaw haslo (wybierz wlasne, bez spacji):
      Echo "TwojeHaslo" >ENVARC:AmiAgent.key
 3. Na koncu S:User-Startup dopisz:
      ;BEGIN AmiAgent
      Run >NIL: C:AmiAgent
      ;END AmiAgent
 4. Zrestartuj Amige albo uruchom od razu:  Run >NIL: C:AmiAgent
 5. Sprawdz adres IP Amigi:  ShowNetStatus

Agent nasluchuje na porcie TCP 6800 (zmiana: C:AmiAgent PORT=nnnn).
Kto zna haslo, moze uruchamiac na Amidze dowolne polecenia: uzywaj tylko
w domowej sieci i nie przekierowuj tego portu na routerze.
Zatrzymanie: Break (Ctrl-C) w Shellu albo  Status  +  Break <numer procesu>.
