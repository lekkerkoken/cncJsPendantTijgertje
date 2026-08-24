ISS-001 — Encoder als volwaardige InputManager-bron ✅ AFGEROND. — De Encoder wordt de bron van EncoderEvent. Voor ontwikkeling kan een tijdelijke seriële testinput events in de Encoder injecteren. Hierdoor blijft de volledige productieketen intact en hoeft main.cpp geen Event-objecten te manipuleren.

ISS-002 — InputManager coalescing: ✅ AFGEROND.
De fysieke input wordt asynchroon verzameld. Encoderpulsen worden samengevoegd tot één delta voordat ze als InputEvent naar de PendantController worden gestuurd.

Met deze verantwoordelijkheden:

Class	Verantwoordelijkheid
Encoder	Quadrature via ISR + encoder button
ButtonMatrix	Matrix scanning + debounce
InputManager	Input queues lezen + events normaliseren + encoder coalescing
PendantController	InputEvents interpreteren
JogPlanner	Joggedrag uitvoeren

En voorlopig:

geen core pinning.

FreeRTOS mag zelf bepalen waar de tasks draaien.

ISS-003 — Echte encoder testen
De tijdelijke KEY_1 → LEFT / KEY_3 → RIGHT simulatie vervangen zodra de echte encoder beschikbaar is.

ISS-004 — Async inputarchitectuur: ✅ AFGEROND.

De gewenste onafhankelijkheid bestaat al:

✅ ButtonMatrix eigen task
✅ Encoder onafhankelijk
✅ InputManager eigen task
✅ FreeRTOS queues tussen componenten
✅ loop() hoeft input niet te pollen
✅ ButtonMatrix en InputManager zijn losgekoppeld
✅ Controller krijgt alleen abstracte Events


ISS-005 — CNCjsClient async maken
De pendant-loop mag nooit wachten op netwerk-I/O.
CNCjsClient verwerkt netwerkverkeer opportunistisch en levert resultaten/events via een lokale, niet-blokkerende interface aan de rest van het systeem.
Ik denk dat we nu klaar zijn voor de echte ISS-005-architectuur

En ik zou die in twee delen splitsen:

ISS-005a — Connection health

Socket.IO non-blocking
reconnect state
heartbeat
heartbeat timeout
connection health beschikbaar maken
geen heartbeat queue
geen oude jogs uitvoeren na reconnect

ISS-005b — Async startup/reconnect

Want jouw huidige:

connectWiFi();
resolveCNCjs();
authenticate();

in begin() is nog steeds blokkerend. Dat is een apart probleem van de runtime-heartbeat.

ISS-006 — Data-uitwisseling tussen tasks
Waarschijnlijk queues/state snapshots/andere veilige mechanismen bepalen.

ISS-007 — JogPlanner async maken
Bijvoorbeeld een eigen vaste plannerfrequentie, onafhankelijk van de inputfrequentie.

ISS-008 — Jog cancel afronden
De bestaande CNCjs jogCancel netjes in de uiteindelijke keten opnemen.

ISS-009 — Sequence diagrams/documentatie bijwerken
Nu pas, want we weten inmiddels veel beter hoe de architectuur werkelijk geworden is.