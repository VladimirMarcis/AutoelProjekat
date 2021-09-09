# AutoelektronikaProjekat

## Uvod

Uz pomo? ovog projekta se simulira alarmni sistem za otvorena vrata na automobilu. Kao okruženje se koristi VisualStudio2019. Prilikom pisanja koda za ovaj projekat, implementiran je i MISRA strandard. 

## Zadatak

1. Pratiti stanja senzora vrata i brzine. Posmatrati  vrednosti koje se dobijaju iz UniCom softvera  sa kanala 0 kao izlaz iz sistema koji prati vrata na slede?i na?in: poruka se sastoji iz po?etnog bajta koji ozna?ava da su naredni podaci vrata (npr 0xFE), a potom i stanja vrata (0 – zatvorena vrata, 1 – otvorena vrata) redom za sva vrata, (na primer 1. bajt – prednja leva, 2. – prednja desna, 3.-zadnja leva, 4.-zadnja desna, 5.-gepek), opciono ubaciti i STOP bajt. Drugi tip poruke je poruka sa senzora brzine gde se može koristiti drugi START bajt (npr 0xEF), a zatim vrednost brzine (mogu?e je poslati heksadecimalnu vrednost, i opcioni STOP bajt).
2.  Realizovati komunikaciju PC-ja sa simuliranim sistemom. Slati naredbe preko simulirane serijske komunikacije. Naredbe i poruke koje se šalju  preko serijske veze treba da sadrže samo ascii slova i brojeve, i trebaju se završavati sa carriage return (CR),  tj brojem 13 (decimalno), ?ime se detektuje kraj poruke.  Naredbe i poruke su:
A) Slanje poruke ka PC-ju svaki put kada su vrata otvorena, a vozilo je u pokretu (brzina mu je ve?a od 5km/h). Neka poruka sadrzi upozorenje kao i koja vrata su u pitanju.
B) Komanda od strane PC-ja prema sistemu kojom se iskljucuje upozorenje za otvoren gepek

3. Preko prekida?a kontrolisati da li je alarm za gepek uklju?en, u tu svrhu pratiti stanje najdonje LEDovke na LED baru (podesiti jedan stubac na LED baru kao ulazni, a drugi kao izlazni), koja ?e simulirati prekida?. 

4. Ako se desi da je brzina ve?a od neke, a vrata otvorena, neka se na 7Seg displeju ispiše upozorenje  koja vrata su u pitanju, a na LED bars neka blinkaju sve diode kao vizuelni signal. Za gepek alarm zavisi od trenutnog moda rada. Ako je alarm za gepek isklju?en, samo na 7Seg displeju ispisati, ali ne blinkati diode.

## Potrebne periferije

Periferije, koje su potrebne za ovaj projekat su slede?e: 
Led_bar
7seg_dispej
AdvUniCom (softver za simulaciju serijske komunikacije)

## Taskovi i funkcije

### static void Serial_send_senzori(TimerHandle_t timer1)
Ovo je funkcija koja se poziva uz pomo? tajmera timer1 na svakih 200ms i ima za zadatak da na kanal0 serijske komunikacije na svakih 200ms šalje slovo 'Z'. Slovo 'Z' zapravo predstavlja triger poruku. Softver za simulaciju serijske komunikacije AdvUniCom ima mogu?nost da reaguje na odre?enu triger poruku. Kada stigne triger poruka, softver šalje automatski poruku, koja predstavlja odgovor na taj triger. Ovo zapravo simulira slanje podataka sa senzora brzine i senzora na vratima.

### static void Serial_receive_tsk_vrata(void* pvParameters)
Ovaj task služi za primanje podataka koje šalju senzori na vratima automobila. U tasku se proverava prvo da li u poruci koja dolazi sa kanala0 serijske komunikacije nalazi start bajt. Start bajt za senzore na vratima je 0xFE. Tako?e, proverava se da nije u pitanju ni stop bajt. Stop bajt  predstavlja 0xFF. Ukoliko nije u pitanju ni start a ni stop bajt onda se takav podatak ?uva i prosle?uje na dalju obradu. Npr: pošaljemo sa serijske \fe00100\ff onda to zna?i da su samo jedna vrata otvorena. U izrazu 00100, prvi broj predstavlja prednja leva vrata, drugi broj predstavlja prednja desna vrata, tre?i broj predstavlja zadnja leva vrata, ?etvrti broj predstavlja zadnja desna vrata a peti broj predstavlja gepek. Dakle u slu?aju da se šalje gore navedena poruka, jedino zadnja leva vrata bi bila otvorena.

### static void Serial_receive_tsk_brzina(void*pvParameters)
Ovaj task služi za primanje podataka koje šalje senzor brzine. Najpre se u tasku proverava da li se u poruci koja dolazi sa kanala0 serijske komunikacije nalazi start bajt. Start bajt za senzor brzine jeste 0xEF. Zatim se proverava da li je u pitanju stop bajt tj 0xFF. Ukoliko nije u pitanju niti start, niti stop bajt onda se takav podatak prosle?uje na dalju obradu. Npr: pošaljemo sa serijske \ef\07\ff\ onda bi to zna?ilo da smo zadali vrednost brzine 0x07 odnosno 7 u decimalnom. 
**napomena** Vrednost za brzinu se ne može zadati da je 0xfe(start bajt za senzore na vratima),0xef(start bajt za senzor brzine), 0x30 (0 u ASCII tabeli), 0x31 (1 u ASCII tabeli).

### static void Senzori_inf(void* pvParameters)
Ovaj task uzima podatke koje šalju dva prethodna taska i smešta u strukturu pa ih šalje na dalju obradu.

### static void Serial_receive_tsk_PC(void* pvParameters)
Ovaj task služi za primanje poruka koje se šalju od strane PCja. Slanje poruka od strane PCja je simulirano uz pomo? simulatora serijske komunikacije na kanalu1. Ukoliko nešto stigne od strane PCja, ovaj task to odmah prosledi na dalju obradu drugim taskovima.

### static void Serial_send_PC(void* pvParameters)
Ovaj task služi za slanje poruke u vidu upozorenja na kanal1 serijske komunikacije. Ukoliko je brzina ve?a od 5 i ukoliko su neka od vrata (nera?unaju?i gepek) otvorena, na serijskoj ?e se ispisati upozorenje kao i to koja su vrata otvorena. Tako?e, i na 7seg displeju ?e se ispisati koja vrata su otvorena. Ukoliko je otvoren gepek, bez obzira na to kolika je brzina, na serijskoj ?e se ispisati upozorenje da su 5. vrata otvorena a na 7seg ?e se ispisati broj 5. Upozorenje za gepek se može ugasiti ukoliko se pošalje sa kanala1 serijske broj \47\, što predstavlja slovo 'G' u ASCII tabeli. Tako?e, u ovom tasku se još proveravaju odre?eni podaci i neki podaci se šalju drugim taskovima na obradu.

### static void Led_bar_tsk(void* pvParameters)
Periferija Led_bar ima mogu?nost simulacije LED dioda ali i simulacije tastera. Ukoliko se ova periferija pozove recimo na slede?i na?in: Led_bars_plus.exe rRG to zna?i da ?emo dobiti jedan ulazni stubac dioda crvene boje koji simulira tastere zapravo i dva izlazna stubca, jedan crvene a drugi zelene boje. Ovaj task zapravo služi za paljenje odre?enih dioda kao i za ?itanje koji su tasteri pritisnuti. Ukoliko je gepek otvoren,prekida? za gepek u ON stanju i ukoliko sa serijske ne stiže \47\ onda ovaj task pali najdonju diodu u prvom drugom stubcu. Ukoliko su vrata otvorena i ukoliko je brzina ve?a od 5 onda ?e ovaj task naizmeni?no paliti i gasiti sve ledovke u tre?em stubcu. Tako?e, ovaj task šalje odre?ene informacije drugom tasku.

### void main_demo(void)
U ovoj funkciji se inicijalizuju sve periferije i kreiraju taskovi, semafori, redovi i tajmeri koji se koriste u kodu.

## Predlog simulacije sistema

Najpre je neophodno otvoriti sve periferije. Dakle, treba nam tri stubca Led dioda i to prvi treba biti ulazni a preostala dva izlazna. 7seg displej koji nam je potreban treba da ima samo jednu cifru. Simulator serijske komunikacije treba otvoriti tako da on zapravo simulira serijsku komuniakciju na kanalu0 i na kanalu1. Zatim treba podesiti da je triger poruka koja stiže na simulator kanala0 slovo 'Z' kao i da se kao odgovor na tu poruku šalje informacija sa senzora vrata tj \fe00000\ff\. Tada na 7seg displeju treba da stoji 0 jer nijedna vrata nisu otvorena a tako?e i brzina je 0. Na LED baru treba podesiti tako da najdonji taster u prvom stubcu bude pritisnut. Pošto su sva vrata zatvorena i brzina je 0, niti jedna LED dioda ne svetli i na kanalu1 serijske komunikacije ne stiže nikakvo upozorenje. Zatim postavimo da je gepek otvoren tj da je \fe00001\ff\ poruka koju šalju senzori na vratima. Tada na 7seg treba da se ispiše broj 5 jer su 5. vrata otvorena. Tako?e, treba da svetli i najdonja LEDovka u drugom stubcu jer je prekida? za gepek u ON stanju. Na kanal1 serijske stiže upozorenje da je gepek otvoren tj ispisuje se: UP: V5O. Ukoliko sada pošaljemo sa kanala1 serijske \47\ onda ?e prestati ispis upozorenja na kanalu1 serijske da je gepek uklju?en a ni LEDovka u drugom stubcu više ne?e svetleti bez obzira što je prekida? uklju?en i što je gepek otvoren. Na ovaj na?in smo proverili sve funkcionalnosti vezane za gepek. Sada možemo staviti takvu kombinaciju koju šalju senzori na vratima da budu otvorena neka druga vrata a gepek zatvoren. Npr \fe10000\ff\. Pošto je brzina i dalje nula, na 7seg ?e stajati nula, na kanalu1 serijske se ne?e ništa ispisivati a ni LED diode ne?e treptati. Ukoliko sada pošaljemo informaciju sa senzora brzine npr \ef\08\ff\, to zna?i da je brzina sada 7. Pošto su prednja leva vrata otvorena i brzina je ve?a od 5, na 7seg displeju se ispisuje broj 1, na kanalu1 serijske se ispisuje upozorenje da su prva vrata otvorena a tre?i stubac LED dioda treperi. 








